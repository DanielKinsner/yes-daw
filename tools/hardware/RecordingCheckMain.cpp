// YES DAW — packaged real-capture recording stage checker (H17 packaged verifier, U4).
//
//   YesDawHardwareRecordingCheck [--run-id <id>] [--json-out <path>] [--seconds <n>] [--work-dir <dir>]
//   YesDawHardwareRecordingCheck --version
//
// Real capture through the REAL bounded recording path: the audio callback pushes input into the
// H5 RecordingChunkFifo via captureRecordingInputBlock (allocation- and lock-free), a control-side
// drain writes the take file through RecordingTakeFileWriter, and the captured samples are then
// committed through the SAME app::commitRecordedAudioTake service the app model uses (KTD7) into
// a fresh bundle — Asset bytes, Clip, RecordingTake, one snapshot. The bundle is reopened and the
// evidence chain is verified mechanically: linkage, canonical float-WAV format, and a sample hash
// computed before persistence and again after reopen.
//
// While capturing, the output plays a deterministic coded burst. Cross-correlating that burst in
// the capture gives a placement measurement — but alignment is CLAIMED only when the input route
// is mechanically identified as a device loopback endpoint AND the correlation is valid AND the
// ADR-0018-compensated error is inside the fixed tolerance (R16). A microphone or unclassified
// input can pass only as the explicit capture_only claim with alignment not_claimed (R17); its
// correlation, if any, is retained as a diagnostic value under a non-claim key.
//
// Exit codes mirror R10 per-stage: 0 = pass (full_alignment or capture_only), 1 = a completed
// measurement violated the gate, 2 = setup/incomplete (no input device, bundle failure, ...).

#include "app/HardwareVerification.h"
#include "app/RecordingAssetCommit.h"
#include "engine/Recording.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "0.0.0-dev"
#endif

namespace hw = yesdaw::app::hardware;
namespace eng = yesdaw::engine;
namespace persist = yesdaw::persistence;

namespace {

constexpr double kBurstAmplitude = 0.25;
constexpr double kBurstSeconds = 0.1;
constexpr double kBurstStartSeconds = 1.0;
constexpr double kCorrelationSnrThreshold = 8.0;

int printUsage()
{
    std::puts ("usage: YesDawHardwareRecordingCheck [--run-id <id>] [--json-out <path>]"
               " [--seconds <n>] [--work-dir <dir>]");
    std::puts ("       YesDawHardwareRecordingCheck --version");
    return 2;
}

const char* argValue (int argc, char** argv, const char* name)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string (argv[i]) == name)
            return argv[i + 1];
    return nullptr;
}

bool hasFlag (int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc; ++i)
        if (std::string (argv[i]) == name)
            return true;
    return false;
}

// Deterministic coded burst: sign sequence from a fixed LCG, one value per frame. Distinctive
// under cross-correlation, unlike a pure tone.
std::vector<float> makeCodedBurst (double sampleRateHz)
{
    const auto frames = static_cast<std::size_t> (std::max (1.0, sampleRateHz * kBurstSeconds));
    std::vector<float> burst (frames, 0.0f);
    std::uint32_t state = 0x59534457u;   // "YSDW"
    for (float& sample : burst)
    {
        state = state * 1664525u + 1013904223u;
        sample = (state & 0x80000000u) != 0 ? static_cast<float> (kBurstAmplitude)
                                            : static_cast<float> (-kBurstAmplitude);
    }
    return burst;
}

// 64-bit FNV-1a over the float bytes: the before/after-persistence identity check. Deterministic,
// not cryptographic — the property proved is "same bytes", not tamper resistance.
std::uint64_t hashSamples (const std::vector<float>& samples)
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto* bytes = reinterpret_cast<const unsigned char*> (samples.data());
    const std::size_t count = samples.size() * sizeof (float);
    for (std::size_t i = 0; i < count; ++i)
    {
        hash ^= static_cast<std::uint64_t> (bytes[i]);
        hash *= 1099511628211ull;
    }
    return hash;
}

// Mechanical input-route classification from the device/channel naming the backend reports.
// Loopback endpoints (e.g. WASAPI "... (loopback)") earn device_loopback; obvious microphones are
// named as such; anything else stays unclassified and can never earn alignment credit.
juce::String classifyInputRoute (const juce::String& deviceName)
{
    const juce::String lower = deviceName.toLowerCase();
    if (lower.contains ("loopback"))
        return hw::kRouteDeviceLoopback;
    if (lower.contains ("microphone") || lower.contains ("mic array") || lower.startsWith ("mic"))
        return hw::kRouteMicrophone;
    return hw::kRouteUnclassified;
}

// Audio callback: burst out, bounded-FIFO capture in. Everything on the audio thread is plain
// arithmetic plus captureRecordingInputBlock (RT-hot, no allocation/locks).
class RecordingCheckCallback final : public juce::AudioIODeviceCallback
{
public:
    explicit RecordingCheckCallback (eng::RecordingChunkFifo& fifo) : fifo_ (fifo) {}

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate_ = device->getCurrentSampleRate();
        burst_ = makeCodedBurst (sampleRate_);   // allocation happens BEFORE the stream runs
        burstStartFrame_ = static_cast<std::int64_t> (kBurstStartSeconds * sampleRate_);

        config_.sampleRateHz = sampleRate_;
        config_.channels = 1;   // set for real in the first callback once channel counts are known
        config_.latency.inputLatencyFrames = std::max (0, device->getInputLatencyInSamples());
        config_.latency.outputLatencyFrames = std::max (0, device->getOutputLatencyInSamples());
        config_.latency.includeOutputLatency = true;
        config_.window.punchStartFrame = 0;
        config_.window.punchEndFrame = std::numeric_limits<std::int64_t>::max();

        outputFrame_ = 0;
        inputFrame_ = 0;
        framesAccepted_ = 0;
        framesDropped_ = 0;
        inputInvalid_ = false;
        channelsConfigured_.store (0, std::memory_order_relaxed);
    }

    void audioDeviceIOCallbackWithContext (const float* const* input, int numInput,
                                           float* const* output, int numOutput,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        // Coded burst out (all channels), silence elsewhere.
        for (int i = 0; i < numSamples; ++i)
        {
            const std::int64_t frame = outputFrame_ + i;
            float sample = 0.0f;
            if (frame >= burstStartFrame_
                && frame < burstStartFrame_ + static_cast<std::int64_t> (burst_.size()))
                sample = burst_[static_cast<std::size_t> (frame - burstStartFrame_)];
            for (int ch = 0; ch < numOutput; ++ch)
                if (output != nullptr && output[ch] != nullptr)
                    output[ch][i] = sample;
        }
        outputFrame_ += numSamples;

        // Bounded capture in: channel count fixed on first sight, then the H5 primitive.
        const int usable = std::min (numInput, eng::kMaxRecordingChannels);
        if (usable > 0)
        {
            if (channelsConfigured_.load (std::memory_order_relaxed) == 0)
            {
                config_.channels = usable;
                channelsConfigured_.store (usable, std::memory_order_relaxed);
            }
            std::array<const float*, eng::kMaxRecordingChannels> channels {};
            for (int c = 0; c < config_.channels && c < numInput; ++c)
                channels[static_cast<std::size_t> (c)] = input[c];

            const eng::RecordingCaptureResult captured = eng::captureRecordingInputBlock (
                fifo_, config_, inputFrame_, channels.data(), config_.channels, numSamples);
            framesAccepted_ += captured.framesAccepted;
            framesDropped_ += captured.framesDropped;
            inputInvalid_ = inputInvalid_ || captured.inputInvalid;
        }
        inputFrame_ += numSamples;
    }

    void audioDeviceStopped() override {}
    void audioDeviceError (const juce::String&) override { errored_ = true; }

    // Read after removeAudioCallback().
    [[nodiscard]] double sampleRate() const { return sampleRate_; }
    [[nodiscard]] int channels() const { return channelsConfigured_.load (std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t framesAccepted() const { return framesAccepted_; }
    [[nodiscard]] std::uint64_t framesDropped() const { return framesDropped_; }
    [[nodiscard]] bool inputInvalid() const { return inputInvalid_; }
    [[nodiscard]] bool errored() const { return errored_; }
    [[nodiscard]] std::int64_t burstStartFrame() const { return burstStartFrame_; }
    [[nodiscard]] const std::vector<float>& burst() const { return burst_; }

private:
    eng::RecordingChunkFifo& fifo_;
    eng::RecordingConfig config_;
    std::vector<float> burst_;
    double sampleRate_ = 48000.0;
    std::int64_t burstStartFrame_ = 0;
    std::int64_t outputFrame_ = 0;
    std::int64_t inputFrame_ = 0;
    std::uint64_t framesAccepted_ = 0;
    std::uint64_t framesDropped_ = 0;
    bool inputInvalid_ = false;
    bool errored_ = false;
    std::atomic<int> channelsConfigured_ { 0 };
};

struct ReconstructedCapture
{
    std::int64_t firstTimelineFrame = 0;
    std::vector<float> mono;   // channel 0, contiguous from firstTimelineFrame
};

ReconstructedCapture reconstructChannelZero (const eng::RecordingTakeFile& file)
{
    ReconstructedCapture out;
    std::int64_t minFrame = std::numeric_limits<std::int64_t>::max();
    std::int64_t maxFrame = std::numeric_limits<std::int64_t>::min();
    for (const eng::RecordingChunk& chunk : file.chunks)
    {
        minFrame = std::min (minFrame, chunk.timelineStartFrame);
        maxFrame = std::max (maxFrame, chunk.timelineStartFrame + static_cast<std::int64_t> (chunk.frameCount));
    }
    if (file.chunks.empty() || maxFrame <= minFrame)
        return out;

    out.firstTimelineFrame = minFrame;
    out.mono.assign (static_cast<std::size_t> (maxFrame - minFrame), 0.0f);
    for (const eng::RecordingChunk& chunk : file.chunks)
        for (std::uint32_t f = 0; f < chunk.frameCount; ++f)
            out.mono[static_cast<std::size_t> (chunk.timelineStartFrame - minFrame + static_cast<std::int64_t> (f))] =
                chunk.samples[static_cast<std::size_t> (f) * chunk.channels];
    return out;
}

struct CorrelationResult
{
    bool found = false;
    std::int64_t timelineFrame = 0;   // burst start position in timeline frames
    double snr = 0.0;
};

CorrelationResult correlateBurst (const ReconstructedCapture& capture, const std::vector<float>& burst)
{
    CorrelationResult result;
    if (burst.empty() || capture.mono.size() <= burst.size())
        return result;

    const std::size_t positions = capture.mono.size() - burst.size();
    std::vector<double> scores (positions, 0.0);
    for (std::size_t offset = 0; offset < positions; ++offset)
    {
        double sum = 0.0;
        for (std::size_t k = 0; k < burst.size(); ++k)
            sum += static_cast<double> (capture.mono[offset + k]) * static_cast<double> (burst[k]);
        scores[offset] = sum;
    }

    std::size_t peakIndex = 0;
    double peak = 0.0;
    for (std::size_t i = 0; i < positions; ++i)
        if (std::abs (scores[i]) > peak)
        {
            peak = std::abs (scores[i]);
            peakIndex = i;
        }

    double noiseSum = 0.0;
    std::size_t noiseCount = 0;
    for (std::size_t i = 0; i < positions; ++i)
    {
        const auto distance = i > peakIndex ? i - peakIndex : peakIndex - i;
        if (distance > burst.size())
        {
            noiseSum += scores[i] * scores[i];
            ++noiseCount;
        }
    }
    const double noiseRms = noiseCount > 0 ? std::sqrt (noiseSum / static_cast<double> (noiseCount)) : 0.0;
    result.snr = noiseRms > 0.0 ? peak / noiseRms : 0.0;
    result.found = peak > 0.0 && result.snr >= kCorrelationSnrThreshold;
    result.timelineFrame = capture.firstTimelineFrame + static_cast<std::int64_t> (peakIndex);
    return result;
}

} // namespace

int main (int argc, char** argv)
{
    if (hasFlag (argc, argv, "--version"))
    {
        std::printf ("YesDawHardwareRecordingCheck %s\n", YESDAW_VERSION_STRING);
        return 0;
    }
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--run-id" || arg == "--json-out" || arg == "--seconds" || arg == "--work-dir")
        {
            ++i;
            continue;
        }
        return printUsage();
    }

    const char* runIdArg = argValue (argc, argv, "--run-id");
    const char* jsonOutArg = argValue (argc, argv, "--json-out");
    const char* secondsArg = argValue (argc, argv, "--seconds");
    const char* workDirArg = argValue (argc, argv, "--work-dir");
    const juce::String runId = runIdArg != nullptr ? juce::String { runIdArg }
                                                   : juce::String { "standalone" };
    const double seconds = std::max (2.0, secondsArg != nullptr ? std::stod (secondsArg) : 4.0);

    const juce::String startedAt = juce::Time::getCurrentTime().toISO8601 (true);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();

    std::filesystem::path workDir;
    if (workDirArg != nullptr)
        workDir = std::filesystem::path { workDirArg };
    else
    {
        const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds> (
            std::chrono::steady_clock::now().time_since_epoch()).count();
        workDir = std::filesystem::temp_directory_path()
                / ("yesdaw-recording-check-" + std::to_string (nanos));
    }
    std::error_code ec;
    std::filesystem::create_directories (workDir, ec);

    const juce::ScopedJuceInitialiser_GUI juceInit;

    const auto emitDocument = [&] (const hw::StageRecord& record) -> bool
    {
        if (jsonOutArg == nullptr)
            return true;
        std::string error;
        if (! hw::writeJsonAtomically (std::filesystem::path { jsonOutArg },
                                       hw::stageDocumentToVar (record, runId), error))
        {
            std::printf ("RECORDING ERROR: could not write result JSON: %s\n", error.c_str());
            return false;
        }
        return true;
    };

    const auto emitSetup = [&] (const juce::String& why) -> int
    {
        hw::StageRecord record;
        record.stage = hw::kStageRecording;
        record.state = hw::StageState::setup;
        record.startedAt = startedAt;
        record.completedAt = juce::Time::getCurrentTime().toISO8601 (true);
        record.durationMs = juce::Time::getMillisecondCounterHiRes() - t0;
        record.failureCodes.add (hw::kFailureDeviceUnavailable);
        record.detail = why;
        record.checkerVersion = YESDAW_VERSION_STRING;
        record.measurements = juce::var { new juce::DynamicObject() };
        emitDocument (record);
        std::printf ("RECORDING SETUP: %s\n", why.toRawUTF8());
        return 2;
    };

    juce::AudioDeviceManager adm;
    juce::AudioDeviceManager::AudioDeviceSetup initial;
    initial.sampleRate = hw::kRequiredSampleRateHz;
    const juce::String initError = adm.initialise (2, 2, nullptr, true, {}, &initial);
    if (initError.isNotEmpty())
        return emitSetup ("audio device initialise failed: " + initError);

    auto* device = adm.getCurrentAudioDevice();
    if (device == nullptr)
        return emitSetup ("no duplex audio device opened");
    if (device->getActiveInputChannels().countNumberOfSetBits() == 0)
        return emitSetup ("no active input channels on the default route");

    const juce::AudioDeviceManager::AudioDeviceSetup granted = adm.getAudioDeviceSetup();
    const juce::String inputName = granted.inputDeviceName.isNotEmpty() ? granted.inputDeviceName
                                                                        : device->getName();

    // The real bounded path: the audio thread pushes into the SPSC FIFO, this thread drains. The
    // chunks are buffered here first because the take-file HEADER channel count must match what
    // the callback actually configured, which is only known once the stream runs.
    eng::RecordingChunkFifo fifo { 256 };
    RecordingCheckCallback callback { fifo };
    adm.addAudioCallback (&callback);

    std::printf ("RECORDING capturing %.0f s on \"%s\" (input route candidate: %s)...\n",
                 seconds, inputName.toRawUTF8(),
                 classifyInputRoute (inputName).toRawUTF8());

    std::vector<eng::RecordingChunk> drained;
    const auto drainFifo = [&fifo, &drained]
    {
        eng::RecordingChunk chunk;
        while (fifo.pop (chunk))
            drained.push_back (chunk);
    };

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::duration_cast<std::chrono::steady_clock::duration> (
                              std::chrono::duration<double> (seconds));
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
        drainFifo();
    }

    adm.removeAudioCallback (&callback);
    drainFifo();
    const bool deviceErrored = callback.errored();
    adm.closeAudioDevice();

    if (deviceErrored)
        return emitSetup ("the device reported an error during capture");
    if (callback.channels() == 0 || callback.framesAccepted() == 0)
        return emitSetup ("the input route delivered no frames");

    // Now run the drained chunks through the REAL H5 writer + reader round-trip.
    const std::filesystem::path takeFilePath = workDir / "capture.ysdwrec";
    eng::RecordingTakeFileWriter writer;
    eng::RecordingConfig writerConfig;
    writerConfig.sampleRateHz = callback.sampleRate();
    writerConfig.channels = callback.channels();
    bool writeOk = writer.open (takeFilePath, writerConfig);
    for (const eng::RecordingChunk& chunk : drained)
        writeOk = writeOk && writer.writeChunk (chunk);
    writeOk = writer.close() && writeOk;
    if (! writeOk)
        return emitSetup ("writing the bounded capture to the take file failed");

    // Read back through the real H5 reader and rebuild channel 0 on the compensated timeline.
    const eng::RecordingTakeFileReadResult takeFile = eng::readRecordingTakeFile (takeFilePath);
    if (takeFile.status != eng::RecordingTakeFileStatus::Ok)
        return emitSetup ("the captured take file did not read back");

    const ReconstructedCapture capture = reconstructChannelZero (takeFile.file);
    double sumSq = 0.0;
    for (const float sample : capture.mono)
        sumSq += static_cast<double> (sample) * static_cast<double> (sample);
    const double captureRms = capture.mono.empty()
                                ? -1.0
                                : std::sqrt (sumSq / static_cast<double> (capture.mono.size()));

    const CorrelationResult correlation = correlateBurst (capture, callback.burst());

    // Canonical persistence through the SHARED commit service into a fresh bundle.
    const std::filesystem::path bundlePath = workDir / "recording-check.yesdaw";
    std::filesystem::remove_all (bundlePath, ec);
    persist::ProjectBundleDb bundle;
    if (! persist::ProjectBundleDb::openOrCreateBundle (bundlePath, bundle).ok())
        return emitSetup ("could not create the verification bundle");

    eng::Project project;
    project.id = eng::EntityId::fromBigEndianParts (0x5245434f52444348ull, 1ull);   // "RECORDCH"
    project.sampleRate = eng::SampleRate { callback.sampleRate() };

    std::uint64_t nextIdLow = 2;
    const yesdaw::app::AllocateEntityId allocateId =
        [&nextIdLow] (std::uint8_t seedByte, const eng::Project&)
    {
        return eng::EntityId::fromBigEndianParts (0x5245434f52440000ull | seedByte, nextIdLow++);
    };
    const yesdaw::app::EnsureFallbackTrack ensureTrack =
        [&allocateId] (eng::Project& p) -> eng::Track&
    {
        if (! p.tracks.empty())
            return p.tracks.front();
        eng::Track track;
        track.id = allocateId (0x54u, p);
        track.strip.name = "Audio 1";
        p.tracks.push_back (track);
        return p.tracks.back();
    };

    yesdaw::app::RecordedAudioTakeRequest request;
    request.sampleRate = eng::SampleRate { callback.sampleRate() };
    request.frames = capture.mono.size();
    request.channels = 1;
    request.interleavedSamples = std::span<const float> (capture.mono.data(), capture.mono.size());
    request.inputChannel = 0;
    request.takeOrdinal = 0;
    request.deviceStableId = 0;
    request.monitoringPolicy = eng::RecordingMonitoringPolicy::Off;

    const std::uint64_t hashBefore = hashSamples (capture.mono);
    const yesdaw::app::RecordedTakeCommitResult commit = yesdaw::app::commitRecordedAudioTake (
        bundle, project, request, allocateId, ensureTrack);

    bool linkageValid = false;
    bool wavValid = false;
    bool hashMatch = false;
    if (commit.ok())
    {
        // Reopen from disk and verify the whole chain mechanically.
        persist::ProjectBundleDb reopened;
        eng::Project persisted;
        if (persist::ProjectBundleDb::openExistingBundle (bundlePath, reopened).ok()
            && reopened.readProjectSnapshot (persisted).ok())
        {
            const eng::RecordingTake* take = persisted.findRecordingTake (commit.takeId);
            const eng::Asset* asset = persisted.findAsset (commit.importedAsset.id);
            const eng::Clip* clip = nullptr;
            for (const eng::Clip& candidate : persisted.clips)
                if (candidate.id == commit.clipId)
                    clip = &candidate;

            linkageValid = take != nullptr && asset != nullptr && clip != nullptr
                        && take->assetId == asset->id && take->clipId == clip->id
                        && take->trackId == clip->trackId && clip->assetId == asset->id
                        && take->frameCount == asset->frames;

            if (asset != nullptr)
            {
                const std::filesystem::path assetPath =
                    bundlePath / persist::detail::assetRelativePathForHash (asset->contentHash);
                yesdaw::io::Float32Wav wav;
                if (yesdaw::io::readFloat32WavFile (assetPath, wav).ok())
                {
                    wavValid = wav.channels == 1
                            && wav.frames == capture.mono.size()
                            && wav.sampleRate.hz == callback.sampleRate();
                    hashMatch = wavValid && hashSamples (wav.interleavedSamples) == hashBefore;
                }
            }
        }
    }
    else
    {
        return emitSetup ("the canonical commit service rejected the capture (status "
                          + juce::String { static_cast<int> (commit.status) } + ")");
    }

    hw::RecordingMeasurement m;
    m.sampleRateHz = callback.sampleRate();
    m.channels = callback.channels();
    m.capturedFrames = capture.mono.size();
    m.droppedFrames = callback.framesDropped();
    m.captureRms = captureRms;
    m.wavValid = wavValid;
    m.linkageValid = linkageValid;
    m.hashMatch = hashMatch;
    m.inputRoute = classifyInputRoute (inputName);
    m.correlationFound = correlation.found;
    m.correlationSnr = correlation.snr;
    m.alignmentErrorFrames = correlation.found
                               ? correlation.timelineFrame - callback.burstStartFrame()
                               : 0;
    m.deviceName = inputName;
    m.backendName = adm.getCurrentAudioDeviceType();
    if (deviceErrored)
        m.captureRms = std::min (m.captureRms, -1.0);   // an errored stream cannot claim capture

    const hw::RecordingVerdict verdict = hw::evaluateRecordingMeasurement (m);
    const hw::StageRecord record = hw::makeRecordingStageRecord (
        m, verdict, YESDAW_VERSION_STRING,
        startedAt, juce::Time::getCurrentTime().toISO8601 (true),
        juce::Time::getMillisecondCounterHiRes() - t0);

    if (! emitDocument (record))
        return 2;

    juce::String codes;
    for (const auto& code : verdict.failureCodes)
        codes += (codes.isEmpty() ? "" : ",") + code;

    std::printf ("RECORDING %s: claim=%s route=%s captured=%llu dropped=%llu rms=%.5f "
                 "correlated=%d snr=%.1f align_err=%lld wav=%d link=%d hash=%d%s%s\n",
                 verdict.state == hw::StageState::pass ? "PASS" : "FAIL",
                 record.claimLevel.isEmpty() ? "none" : record.claimLevel.toRawUTF8(),
                 m.inputRoute.toRawUTF8(),
                 static_cast<unsigned long long> (m.capturedFrames),
                 static_cast<unsigned long long> (m.droppedFrames),
                 m.captureRms,
                 m.correlationFound ? 1 : 0,
                 m.correlationSnr,
                 static_cast<long long> (m.alignmentErrorFrames),
                 m.wavValid ? 1 : 0,
                 m.linkageValid ? 1 : 0,
                 m.hashMatch ? 1 : 0,
                 codes.isEmpty() ? "" : " codes=",
                 codes.toRawUTF8());

    return verdict.state == hw::StageState::pass ? 0 : 1;
}
