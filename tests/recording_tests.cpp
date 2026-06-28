// YES DAW - H5 recording gate.
//
// Deterministic loopback checks for the recording spine: audio callback input enters a bounded FIFO,
// a writer thread drains it to disk, and the recorded take aligns back to the click reference after
// non-zero input + output latency compensation.
//
// The gate is built to BITE, not merely to go green: the latency negative control runs the FULL
// capture -> FIFO -> writer -> file -> read pipeline with compensation removed and proves the recorded
// peak lands at the WRONG (uncompensated) frame; the stereo, backpressure, loop-limit, direct-input,
// file-format-error, comp, and MIDI-edge paths are each exercised by an independent case.

#include "engine/Recording.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <thread>
#include <vector>

using Catch::Approx;
using yesdaw::engine::Event;
using yesdaw::engine::EventType;
using yesdaw::engine::IncomingRecordedMidiEvent;
using yesdaw::engine::RecordingCaptureResult;
using yesdaw::engine::RecordingChunkFifo;
using yesdaw::engine::RecordingCompSegment;
using yesdaw::engine::RecordingConfig;
using yesdaw::engine::RecordingLatencyModel;
using yesdaw::engine::RecordingMidiStatus;
using yesdaw::engine::RecordingTakeFile;
using yesdaw::engine::RecordingTakeFileStatus;
using yesdaw::engine::RecordingTakeFileWriter;
using yesdaw::engine::RecordedMidiEvent;
using yesdaw::engine::captureRecordingInputBlock;
using yesdaw::engine::findRecordedSample;
using yesdaw::engine::mapDeviceInputFrameToRecordingFrame;
using yesdaw::engine::readRecordingTakeFile;
using yesdaw::engine::recordMidiEventsToTimeline;
using yesdaw::engine::renderRecordingComp;

namespace {

// A single nonzero input sample to drop into the synthetic device stream.
struct Impulse
{
    int          channel     = 0;
    std::int64_t deviceFrame = 0;
    float        amplitude   = 1.0f;
};

std::filesystem::path tempTakePath (const char* name)
{
    // Process-unique salt + a per-process counter so parallel ctest jobs never collide on a name.
    static const unsigned salt = std::random_device {}();
    static std::atomic<unsigned> counter { 0 };
    return std::filesystem::temp_directory_path()
         / (std::string { name } + "_" + std::to_string (salt) + "_"
            + std::to_string (counter.fetch_add (1)) + ".ysdtake");
}

struct CaptureRunResult
{
    bool          writerOk       = false;
    bool          inputInvalid   = false;
    bool          fifoFull       = false;
    std::uint32_t framesAccepted = 0;
    std::uint32_t framesDropped  = 0;
};

// Drives a synthetic multi-channel device stream through the real capture -> FIFO -> writer pipeline.
// Exception-safe: the producer loop NEVER calls a Catch2 REQUIRE (which throws), so the writer thread
// is always joined — a mid-capture assertion can never leave a joinable std::thread to std::terminate.
// The caller inspects the returned result fields after this returns.
CaptureRunResult captureSyntheticInputToDisk (const RecordingConfig& config,
                                              const std::filesystem::path& path,
                                              const std::vector<Impulse>& impulses,
                                              std::int64_t totalDeviceFrames,
                                              std::uint32_t fifoCapacity = 64)
{
    CaptureRunResult result;

    RecordingChunkFifo fifo { fifoCapacity };
    RecordingTakeFileWriter writer;
    if (! writer.open (path, config))
        return result; // writerOk stays false; the caller REQUIREs on it.

    std::atomic<bool> done { false };
    bool writerOk = true;
    std::thread writerThread (
        [&]
        {
            while (! done.load (std::memory_order_acquire))
            {
                writerOk = writer.drain (fifo) && writerOk;
                std::this_thread::yield();
            }

            writerOk = writer.drain (fifo) && writer.close() && writerOk;
        });

    const int channels = config.channels;
    constexpr int kBlock = 37;
    for (std::int64_t deviceStart = 0; deviceStart < totalDeviceFrames; deviceStart += kBlock)
    {
        const int frames = static_cast<int> (std::min<std::int64_t> (kBlock, totalDeviceFrames - deviceStart));
        std::vector<std::vector<float>> buffers (
            static_cast<std::size_t> (channels), std::vector<float> (static_cast<std::size_t> (frames), 0.0f));
        for (const Impulse& impulse : impulses)
        {
            if (impulse.channel < 0 || impulse.channel >= channels)
                continue;

            if (impulse.deviceFrame >= deviceStart && impulse.deviceFrame < deviceStart + frames)
                buffers[static_cast<std::size_t> (impulse.channel)]
                       [static_cast<std::size_t> (impulse.deviceFrame - deviceStart)] = impulse.amplitude;
        }

        std::vector<const float*> channelPtrs (static_cast<std::size_t> (channels));
        for (int c = 0; c < channels; ++c)
            channelPtrs[static_cast<std::size_t> (c)] = buffers[static_cast<std::size_t> (c)].data();

        const RecordingCaptureResult capture =
            captureRecordingInputBlock (fifo, config, deviceStart, channelPtrs.data(), channels, frames);
        result.inputInvalid = result.inputInvalid || capture.inputInvalid;
        result.fifoFull = result.fifoFull || capture.fifoFull;
        result.framesAccepted += capture.framesAccepted;
        result.framesDropped += capture.framesDropped;
    }

    done.store (true, std::memory_order_release);
    writerThread.join();
    result.writerOk = writerOk;
    return result;
}

struct Peak
{
    float        value = 0.0f;
    std::int64_t frame = -1;
};

// Loudest |sample| on a channel across the whole take, with its timeline frame.
Peak findPeak (const RecordingTakeFile& file, std::uint32_t channel)
{
    Peak peak;
    for (const auto& chunk : file.chunks)
    {
        if (channel >= chunk.channels)
            continue;

        for (std::uint32_t frame = 0; frame < chunk.frameCount; ++frame)
        {
            const float value = std::fabs (
                chunk.samples[static_cast<std::size_t> (frame) * chunk.channels + channel]);
            if (value > peak.value)
            {
                peak.value = value;
                peak.frame = chunk.timelineStartFrame + static_cast<std::int64_t> (frame);
            }
        }
    }

    return peak;
}

} // namespace

TEST_CASE ("recorded take aligns to a click reference after non-trivial round-trip latency", "[recording][h5][gate]")
{
    const std::filesystem::path path = tempTakePath ("yesdaw_recording_align");
    std::filesystem::remove (path);

    RecordingConfig config;
    config.channels = 1;
    config.sampleRateHz = 48000.0;
    config.latency.inputLatencyFrames = 37;
    config.latency.outputLatencyFrames = 73;
    config.latency.includeOutputLatency = true;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 192;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));
    REQUIRE (roundTrip > 1);

    constexpr std::int64_t kClickFrame = 96;
    const std::int64_t impulseDeviceFrame = kClickFrame + roundTrip;

    const auto run = captureSyntheticInputToDisk (
        config, path, { { 0, impulseDeviceFrame, 1.0f } }, config.window.punchEndFrame + roundTrip + 16);
    REQUIRE (run.writerOk);
    REQUIRE_FALSE (run.inputInvalid);
    REQUIRE (run.framesDropped == 0u);
    REQUIRE (run.framesAccepted > 0u);

    const auto read = readRecordingTakeFile (path);
    REQUIRE (read.status == RecordingTakeFileStatus::Ok);
    REQUIRE (read.file.channels == 1u);

    const Peak peak = findPeak (read.file, 0);
    REQUIRE (peak.value == Approx (1.0f));
    REQUIRE (std::llabs (peak.frame - kClickFrame) <= 1);
    std::filesystem::remove (path);

    // Real negative control: re-run the SAME pipeline with latency compensation removed. The recorded
    // peak must land at the raw (uncompensated) device frame, NOT back at the click. This fails if a
    // regression ever zeroed compensatedLatencyFrames or dropped the subtraction from the capture path
    // (the previous gate only asserted this on a parallel pure-math call, which could not bite).
    const std::filesystem::path brokenPath = tempTakePath ("yesdaw_recording_align_nocomp");
    std::filesystem::remove (brokenPath);

    RecordingConfig broken = config;
    broken.latency.inputLatencyFrames = 0;
    broken.latency.outputLatencyFrames = 0;
    broken.window.punchEndFrame = impulseDeviceFrame + 16;

    const auto brokenRun = captureSyntheticInputToDisk (
        broken, brokenPath, { { 0, impulseDeviceFrame, 1.0f } }, broken.window.punchEndFrame + 16);
    REQUIRE (brokenRun.writerOk);
    REQUIRE (brokenRun.framesDropped == 0u);

    const auto brokenRead = readRecordingTakeFile (brokenPath);
    REQUIRE (brokenRead.status == RecordingTakeFileStatus::Ok);
    const Peak brokenPeak = findPeak (brokenRead.file, 0);
    REQUIRE (brokenPeak.value == Approx (1.0f));
    REQUIRE (std::llabs (brokenPeak.frame - impulseDeviceFrame) <= 1);
    REQUIRE (std::llabs (brokenPeak.frame - kClickFrame) > 1);
    std::filesystem::remove (brokenPath);
}

TEST_CASE ("punch loop recording creates deterministic take ordinals and comp selection", "[recording][h5][loop][comp]")
{
    const std::filesystem::path path = tempTakePath ("yesdaw_recording_loop");
    std::filesystem::remove (path);

    RecordingConfig config;
    config.channels = 1;
    config.latency.inputLatencyFrames = 5;
    config.latency.outputLatencyFrames = 7;
    config.window.punchStartFrame = 64;
    config.window.punchEndFrame = 96;
    config.window.loopEnabled = true;
    config.window.loopStartFrame = 64;
    config.window.loopEndFrame = 96;
    config.window.maxLoopTakes = 2;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));

    constexpr std::int64_t kLoopClickFrame = 72;
    constexpr std::int64_t kLoopLength = 32;
    const std::int64_t take0DeviceFrame = kLoopClickFrame + roundTrip;
    const std::int64_t take1DeviceFrame = kLoopClickFrame + kLoopLength + roundTrip;
    // A third pass exists in the input but must be REJECTED by maxLoopTakes == 2.
    const std::int64_t take2DeviceFrame = kLoopClickFrame + 2 * kLoopLength + roundTrip;

    const auto run = captureSyntheticInputToDisk (
        config,
        path,
        { { 0, take0DeviceFrame, 0.25f }, { 0, take1DeviceFrame, 0.75f }, { 0, take2DeviceFrame, 0.5f } },
        take2DeviceFrame + kLoopLength + roundTrip + 16);
    REQUIRE (run.writerOk);
    REQUIRE (run.framesDropped == 0u);

    const auto read = readRecordingTakeFile (path);
    REQUIRE (read.status == RecordingTakeFileStatus::Ok);

    float sample = 0.0f;
    REQUIRE (findRecordedSample (read.file, 0, kLoopClickFrame, 0, sample));
    REQUIRE (sample == Approx (0.25f));
    REQUIRE (findRecordedSample (read.file, 1, kLoopClickFrame, 0, sample));
    REQUIRE (sample == Approx (0.75f));
    // maxLoopTakes == 2 means take ordinal 2 was never recorded: no sample exists for it.
    REQUIRE_FALSE (findRecordedSample (read.file, 2, kLoopClickFrame, 0, sample));

    RecordingCompSegment segment;
    segment.takeOrdinal = 1;
    segment.timelineStartFrame = config.window.loopStartFrame;
    segment.frameCount = static_cast<std::uint32_t> (config.window.loopEndFrame - config.window.loopStartFrame);

    std::vector<float> comped;
    REQUIRE (renderRecordingComp (read.file, std::span<const RecordingCompSegment> (&segment, 1), comped));
    REQUIRE (comped.size() == static_cast<std::size_t> (segment.frameCount));
    REQUIRE (comped[static_cast<std::size_t> (kLoopClickFrame - config.window.loopStartFrame)] == Approx (0.75f));
    std::filesystem::remove (path);
}

TEST_CASE ("MIDI recording uses the same latency compensation as audio recording", "[recording][h5][midi]")
{
    RecordingConfig config;
    config.channels = 1;
    config.latency.inputLatencyFrames = 4;
    config.latency.outputLatencyFrames = 6;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 256;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));

    Event event;
    event.type = EventType::NoteOn;
    event.voice.key = 60;
    event.payload.note.normalizedVelocity = 1.0;

    IncomingRecordedMidiEvent incoming;
    incoming.deviceInputFrame = 100 + roundTrip;
    incoming.event = event;

    RecordedMidiEvent out[1] {};
    const auto result = recordMidiEventsToTimeline (
        config,
        std::span<const IncomingRecordedMidiEvent> (&incoming, 1),
        std::span<RecordedMidiEvent> (out));

    REQUIRE (result.status == RecordingMidiStatus::Ok);
    REQUIRE (result.eventsWritten == 1u);
    REQUIRE (out[0].timelineFrame == 100);
    REQUIRE (out[0].takeOrdinal == 0u);
    REQUIRE (out[0].event.type == EventType::NoteOn);
    REQUIRE (out[0].event.voice.key == 60);
}

TEST_CASE ("MIDI recording filters out-of-window events and reports an undersized output", "[recording][h5][midi][edge]")
{
    RecordingConfig config;
    config.channels = 1;
    config.latency.inputLatencyFrames = 4;
    config.latency.outputLatencyFrames = 6;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 256;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));

    Event note;
    note.type = EventType::NoteOn;
    note.voice.key = 64;

    // One event inside the punch window, one compensated past punchEndFrame (must be filtered out).
    const IncomingRecordedMidiEvent incoming[2] = {
        { 50 + roundTrip, note },
        { config.window.punchEndFrame + roundTrip, note },
    };

    SECTION ("out-of-window event is dropped, in-window event is kept")
    {
        RecordedMidiEvent out[2] {};
        const auto result = recordMidiEventsToTimeline (
            config, std::span<const IncomingRecordedMidiEvent> (incoming, 2), std::span<RecordedMidiEvent> (out));
        REQUIRE (result.status == RecordingMidiStatus::Ok);
        REQUIRE (result.eventsWritten == 1u);
        REQUIRE (out[0].timelineFrame == 50);
    }

    SECTION ("undersized output reports OutputTooSmall after filling what it can")
    {
        // Two in-window events but room for only one.
        const IncomingRecordedMidiEvent twoInWindow[2] = { { 50 + roundTrip, note }, { 60 + roundTrip, note } };
        RecordedMidiEvent out[1] {};
        const auto result = recordMidiEventsToTimeline (
            config, std::span<const IncomingRecordedMidiEvent> (twoInWindow, 2), std::span<RecordedMidiEvent> (out));
        REQUIRE (result.status == RecordingMidiStatus::OutputTooSmall);
        REQUIRE (result.eventsWritten == 1u);
        REQUIRE (out[0].timelineFrame == 50);
    }

    SECTION ("an invalid config is rejected without writing")
    {
        RecordingConfig bad = config;
        bad.channels = 0; // invalid
        RecordedMidiEvent out[1] {};
        const auto result = recordMidiEventsToTimeline (
            bad, std::span<const IncomingRecordedMidiEvent> (incoming, 1), std::span<RecordedMidiEvent> (out));
        REQUIRE (result.status == RecordingMidiStatus::InvalidInput);
        REQUIRE (result.eventsWritten == 0u);
    }
}

TEST_CASE ("stereo capture round-trips per-channel samples at the right frames", "[recording][h5][stereo]")
{
    const std::filesystem::path path = tempTakePath ("yesdaw_recording_stereo");
    std::filesystem::remove (path);

    RecordingConfig config;
    config.channels = 2;
    config.latency.inputLatencyFrames = 10;
    config.latency.outputLatencyFrames = 20;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 256;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));

    constexpr std::int64_t kLeftClick = 50;
    constexpr std::int64_t kRightClick = 120;

    const auto run = captureSyntheticInputToDisk (
        config,
        path,
        { { 0, kLeftClick + roundTrip, 0.5f }, { 1, kRightClick + roundTrip, -0.8f } },
        config.window.punchEndFrame + roundTrip + 16);
    REQUIRE (run.writerOk);
    REQUIRE_FALSE (run.inputInvalid);
    REQUIRE (run.framesDropped == 0u);

    const auto read = readRecordingTakeFile (path);
    REQUIRE (read.status == RecordingTakeFileStatus::Ok);
    REQUIRE (read.file.channels == 2u);

    float sample = 0.0f;
    // Each impulse appears on its own channel at its own frame...
    REQUIRE (findRecordedSample (read.file, 0, kLeftClick, 0, sample));
    REQUIRE (sample == Approx (0.5f));
    REQUIRE (findRecordedSample (read.file, 0, kRightClick, 1, sample));
    REQUIRE (sample == Approx (-0.8f));
    // ...and NOT on the other channel (a swapped interleave index would fail these).
    REQUIRE (findRecordedSample (read.file, 0, kLeftClick, 1, sample));
    REQUIRE (sample == Approx (0.0f));
    REQUIRE (findRecordedSample (read.file, 0, kRightClick, 0, sample));
    REQUIRE (sample == Approx (0.0f));
    std::filesystem::remove (path);
}

TEST_CASE ("FIFO backpressure drops chunks, reports it, and keeps the accounting exact", "[recording][h5][backpressure]")
{
    // Capacity-1 FIFO that is never drained: after the first chunk lands, every later push fails.
    RecordingChunkFifo fifo { 1 };

    RecordingConfig config;
    config.channels = 1;
    config.latency.inputLatencyFrames = 0;
    config.latency.outputLatencyFrames = 0;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 100000; // every device frame maps (no rejection), so it is accepted XOR dropped.

    constexpr std::int64_t kTotal = 2000;
    constexpr int kBlock = 37;
    std::uint32_t accepted = 0;
    std::uint32_t dropped = 0;
    bool sawFull = false;
    for (std::int64_t start = 0; start < kTotal; start += kBlock)
    {
        const int frames = static_cast<int> (std::min<std::int64_t> (kBlock, kTotal - start));
        std::vector<float> input (static_cast<std::size_t> (frames), 0.0f);
        const float* channels[1] = { input.data() };
        const auto capture = captureRecordingInputBlock (fifo, config, start, channels, 1, frames);
        accepted += capture.framesAccepted;
        dropped += capture.framesDropped;
        sawFull = sawFull || capture.fifoFull;
    }

    REQUIRE (sawFull);
    REQUIRE (dropped > 0u);
    // Every mapped frame is either handed to the FIFO or counted as dropped — never both, never lost.
    REQUIRE (accepted + dropped == static_cast<std::uint32_t> (kTotal));
    // The one slot means only a tiny prefix survives.
    REQUIRE (accepted <= static_cast<std::uint32_t> (kBlock));
}

TEST_CASE ("direct-input recording compensates input latency only", "[recording][h5][directinput]")
{
    RecordingLatencyModel model;
    model.inputLatencyFrames = 40;
    model.outputLatencyFrames = 100;

    std::int64_t both = 0;
    model.includeOutputLatency = true;
    REQUIRE (model.compensatedLatencyFrames (both));
    REQUIRE (both == 140);

    std::int64_t inputOnly = 0;
    model.includeOutputLatency = false;
    REQUIRE (model.compensatedLatencyFrames (inputOnly));
    REQUIRE (inputOnly == 40);

    RecordingConfig config;
    config.channels = 1;
    config.latency = model; // includeOutputLatency == false
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 500;

    std::int64_t timelineFrame = 0;
    std::uint32_t takeOrdinal = 0;
    REQUIRE (mapDeviceInputFrameToRecordingFrame (config, 200, timelineFrame, takeOrdinal));
    REQUIRE (timelineFrame == 160); // 200 - input(40), output latency excluded
    REQUIRE (takeOrdinal == 0u);
}

TEST_CASE ("take file reader rejects a missing file and a corrupt header", "[recording][h5][fileformat]")
{
    SECTION ("missing file -> OpenFailed")
    {
        const auto read = readRecordingTakeFile (tempTakePath ("yesdaw_recording_missing"));
        REQUIRE (read.status == RecordingTakeFileStatus::OpenFailed);
    }

    SECTION ("wrong magic -> FormatInvalid")
    {
        const std::filesystem::path path = tempTakePath ("yesdaw_recording_badmagic");
        {
            std::ofstream out (path, std::ios::binary | std::ios::trunc);
            const char junk[32] = { 'N', 'O', 'P', 'E', '!', '!', '!', '!' };
            out.write (junk, sizeof (junk));
        }
        const auto read = readRecordingTakeFile (path);
        REQUIRE (read.status == RecordingTakeFileStatus::FormatInvalid);
        std::filesystem::remove (path);
    }

    SECTION ("truncated header -> FormatInvalid")
    {
        const std::filesystem::path path = tempTakePath ("yesdaw_recording_truncated");
        {
            std::ofstream out (path, std::ios::binary | std::ios::trunc);
            const char magic[8] = { 'Y', 'S', 'D', 'W', 'R', 'E', 'C', '1' };
            out.write (magic, sizeof (magic)); // valid magic but nothing after it
        }
        const auto read = readRecordingTakeFile (path);
        REQUIRE (read.status == RecordingTakeFileStatus::FormatInvalid);
        std::filesystem::remove (path);
    }
}

TEST_CASE ("comp assembles multiple takes and zero-fills frames outside any take", "[recording][h5][comp]")
{
    const std::filesystem::path path = tempTakePath ("yesdaw_recording_comp");
    std::filesystem::remove (path);

    RecordingConfig config;
    config.channels = 1;
    config.latency.inputLatencyFrames = 5;
    config.latency.outputLatencyFrames = 7;
    config.window.punchStartFrame = 64;
    config.window.punchEndFrame = 96;
    config.window.loopEnabled = true;
    config.window.loopStartFrame = 64;
    config.window.loopEndFrame = 96;
    config.window.maxLoopTakes = 2;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));

    constexpr std::int64_t kClick = 72;
    constexpr std::int64_t kLen = 32;
    const auto run = captureSyntheticInputToDisk (
        config,
        path,
        { { 0, kClick + roundTrip, 0.25f }, { 0, kClick + kLen + roundTrip, 0.75f } },
        config.window.loopEndFrame + kLen + roundTrip + 16);
    REQUIRE (run.writerOk);

    const auto read = readRecordingTakeFile (path);
    REQUIRE (read.status == RecordingTakeFileStatus::Ok);

    const std::uint32_t len = static_cast<std::uint32_t> (kLen);
    const RecordingCompSegment segments[3] = {
        { 0, config.window.loopStartFrame, len },                       // take 0
        { 1, config.window.loopStartFrame, len },                       // take 1
        { 0, config.window.loopStartFrame + 100000, len },              // outside any recorded range -> silence
    };

    std::vector<float> comped;
    REQUIRE (renderRecordingComp (read.file, std::span<const RecordingCompSegment> (segments, 3), comped));
    REQUIRE (comped.size() == static_cast<std::size_t> (3 * len));

    const std::size_t clickOffset = static_cast<std::size_t> (kClick - config.window.loopStartFrame);
    REQUIRE (comped[clickOffset] == Approx (0.25f));            // segment 0, take 0
    REQUIRE (comped[len + clickOffset] == Approx (0.75f));      // segment 1, take 1
    for (std::uint32_t i = 0; i < len; ++i)
        REQUIRE (comped[2 * len + i] == Approx (0.0f));         // segment 2, zero-filled gap
    std::filesystem::remove (path);
}
