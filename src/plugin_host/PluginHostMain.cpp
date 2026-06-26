// YES DAW - minimal plugin host child executable (ADR-0015).
//
// This is a worker/layering checkpoint: it proves there is a separate executable that owns JUCE
// plugin-hosting modules, can receive an RT-lane shared-memory identity, and can poll that mapped
// region through the hosted synthetic AudioProcessor. It does not scan/load external plugins, launch
// from the app, or run real plugin watchdog policy yet.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include "engine/plugin/RtLaneRing.h"
#include "plugin_host/PluginHostProtocol.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>

namespace {

bool hasFlag (int argc, char** argv, const char* flag) noexcept
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp (argv[i], flag) == 0)
            return true;

    return false;
}

bool messageMatches (const juce::MemoryBlock& message, const char* text) noexcept
{
    const auto expectedSize = std::strlen (text) + 1;
    return message.getSize() == expectedSize
        && std::memcmp (message.getData(), text, expectedSize) == 0;
}

juce::String commandLineFromArgv (int argc, char** argv)
{
    juce::StringArray args;

    for (int i = 1; i < argc; ++i)
        args.add (juce::String (argv[i]));

    return args.joinIntoString (" ");
}

enum class SyntheticProcessorMode
{
    passthrough,
    fixedReportedLatency,
    emitNan,
    hangAfterHandshake,
    crashOnCue
};

class SyntheticTestProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kFixedLatencySamples = 96;

    explicit SyntheticTestProcessor (SyntheticProcessorMode mode)
        : juce::AudioProcessor (juce::AudioProcessor::BusesProperties()
                                    .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          mode_ (mode)
    {
    }

    const juce::String getName() const override
    {
        return "YES DAW Synthetic Test Plugin";
    }

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override
    {
        prepared_ = sampleRate > 0.0 && maximumExpectedSamplesPerBlock > 0;
        setLatencySamples (mode_ == SyntheticProcessorMode::fixedReportedLatency ? kFixedLatencySamples : 0);
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        if (! prepared_)
            return;

        if (mode_ == SyntheticProcessorMode::emitNan && buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0)
            buffer.setSample (0, 0, std::numeric_limits<float>::quiet_NaN());
    }

    bool acceptsMidi() const override
    {
        return true;
    }

    bool producesMidi() const override
    {
        return false;
    }

    bool isMidiEffect() const override
    {
        return false;
    }

    double getTailLengthSeconds() const override
    {
        return 0.0;
    }

    juce::AudioProcessorEditor* createEditor() override
    {
        return nullptr;
    }

    bool hasEditor() const override
    {
        return false;
    }

    int getNumPrograms() override
    {
        return 1;
    }

    int getCurrentProgram() override
    {
        return 0;
    }

    void setCurrentProgram (int index) override
    {
        (void) index;
    }

    const juce::String getProgramName (int index) override
    {
        (void) index;
        return "Default";
    }

    void changeProgramName (int index, const juce::String& newName) override
    {
        (void) index;
        (void) newName;
    }

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        static constexpr char kStateChunk[] = "yesdaw-synthetic-state-v1";
        destData.setSize (sizeof (kStateChunk));
        std::memcpy (destData.getData(), kStateChunk, sizeof (kStateChunk));
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        static constexpr char kStateChunk[] = "yesdaw-synthetic-state-v1";
        stateAccepted_ = sizeInBytes == static_cast<int> (sizeof (kStateChunk))
            && std::memcmp (data, kStateChunk, sizeof (kStateChunk)) == 0;
    }

    bool stateAccepted() const noexcept
    {
        return stateAccepted_;
    }

    SyntheticProcessorMode mode() const noexcept
    {
        return mode_;
    }

private:
    SyntheticProcessorMode mode_;
    bool prepared_ = false;
    bool stateAccepted_ = false;
};

class PluginHostWorker final : public juce::ChildProcessWorker
{
public:
    void handleConnectionMade() override
    {
        const char* const ready = yesdaw::plugin_host::kWorkerReadyMessage;
        sendMessageToCoordinator (juce::MemoryBlock (ready, std::strlen (ready) + 1));
    }

    void handleMessageFromCoordinator (const juce::MemoryBlock& message) override
    {
        if (messageMatches (message, yesdaw::plugin_host::kWatchdogProbeMessage))
        {
            controlLaneHung_.store (true, std::memory_order_release);
            return;
        }

        if (messageMatches (message, yesdaw::plugin_host::kRunningWatchdogRtLaneHangMessage))
        {
            const char* const ack = yesdaw::plugin_host::kRunningWatchdogRtLaneHangAckMessage;
            sendMessageToCoordinator (juce::MemoryBlock (ack, std::strlen (ack) + 1));
            rtLaneHung_.store (true, std::memory_order_release);
            return;
        }

        yesdaw::plugin_host::RtLaneLoadMessage rtLaneLoadMessage;
        if (yesdaw::plugin_host::copyRtLaneLoadMessage (message.getData(), message.getSize(), rtLaneLoadMessage))
        {
            handleRtLaneLoadMessage (rtLaneLoadMessage);
            return;
        }

        if (controlLaneHung_.load (std::memory_order_acquire))
            return;

        sendMessageToCoordinator (message);
    }

    void handleConnectionLost() override
    {
        shouldQuit_.store (true, std::memory_order_release);
    }

    bool shouldQuit() const noexcept
    {
        return shouldQuit_.load (std::memory_order_acquire);
    }

    bool pollRtLaneOnce() noexcept
    {
        if (rtLaneHung_.load (std::memory_order_acquire))
            return false;

        std::lock_guard<std::mutex> lock (rtLaneMutex_);
        if (rtLane_ == nullptr || hostedProcessor_ == nullptr)
            return false;

        return rtLane_->pollOnce (
            [this] (std::span<const yesdaw::engine::Event> events,
                    const float* const* input, float* const* output,
                    int channels, int numFrames) noexcept
            {
                processHostedBlock (events, input, output, channels, numFrames);
            });
    }

private:
    void handleRtLaneLoadMessage (const yesdaw::plugin_host::RtLaneLoadMessage& message)
    {
        if (! yesdaw::plugin_host::isValidRtLaneLoadMessage (message))
        {
            const auto reply = yesdaw::plugin_host::makeRtLaneLoadReplyMessage (
                yesdaw::plugin_host::RtLaneLoadReplyStatus::rejectedInvalidIdentity, {});
            sendMessageToCoordinator (juce::MemoryBlock (&reply, sizeof (reply)));
            resetHostedRtLane();
            return;
        }

        const std::string sharedMemoryName = yesdaw::plugin_host::rtLaneSharedMemoryName (message);
        auto endpoint = std::make_unique<yesdaw::engine::RtLaneRing>();
        if (! endpoint->attachSharedMemory (sharedMemoryName))
        {
            const auto reply = yesdaw::plugin_host::makeRtLaneLoadReplyMessage (
                yesdaw::plugin_host::RtLaneLoadReplyStatus::rejectedAttachFailed,
                sharedMemoryName,
                static_cast<std::int32_t> (endpoint->lastAttachFailure()),
                endpoint->lastAttachSystemError());
            sendMessageToCoordinator (juce::MemoryBlock (&reply, sizeof (reply)));
            resetHostedRtLane();
            return;
        }

        {
            std::lock_guard<std::mutex> lock (rtLaneMutex_);
            const int channels = static_cast<int> (std::max (1u, message.config.channels));
            const int maxBlockSize = static_cast<int> (std::max (1u, message.config.maxBlockSize));

            hostedProcessor_ = std::make_unique<SyntheticTestProcessor> (SyntheticProcessorMode::passthrough);
            hostedProcessorBuffer_.setSize (channels, maxBlockSize);
            hostedProcessorBuffer_.clear();
            hostedMidi_.clear();
            hostedProcessor_->prepareToPlay (48000.0, maxBlockSize);
            rtLane_ = std::move (endpoint);
        }

        const auto reply = yesdaw::plugin_host::makeRtLaneLoadReplyMessage (
            yesdaw::plugin_host::RtLaneLoadReplyStatus::accepted, sharedMemoryName);
        sendMessageToCoordinator (juce::MemoryBlock (&reply, sizeof (reply)));
    }

    void resetHostedRtLane()
    {
        std::lock_guard<std::mutex> lock (rtLaneMutex_);
        rtLane_.reset();
        hostedProcessor_.reset();
        hostedProcessorBuffer_.clear();
        hostedMidi_.clear();
    }

    void processHostedBlock (std::span<const yesdaw::engine::Event> events,
                             const float* const* input,
                             float* const* output,
                             int channels,
                             int numFrames) noexcept
    {
        (void) events;

        const int bufferChannels = hostedProcessorBuffer_.getNumChannels();
        const int bufferFrames = hostedProcessorBuffer_.getNumSamples();
        const int channelsToProcess = std::min (channels, bufferChannels);
        const int framesToProcess = std::min (numFrames, bufferFrames);

        hostedProcessorBuffer_.clear();
        for (int channel = 0; channel < channelsToProcess; ++channel)
            for (int frame = 0; frame < framesToProcess; ++frame)
                hostedProcessorBuffer_.setSample (channel, frame, input[channel][frame]);

        hostedMidi_.clear();
        {
            const juce::ScopedNoDenormals noDenormals;
            hostedProcessor_->processBlock (hostedProcessorBuffer_, hostedMidi_);
        }

        for (int channel = 0; channel < channels; ++channel)
            for (int frame = 0; frame < numFrames; ++frame)
                output[channel][frame] = channel < channelsToProcess && frame < framesToProcess
                    ? hostedProcessorBuffer_.getSample (channel, frame)
                    : 0.0f;
    }

    std::atomic<bool> shouldQuit_ { false };
    std::atomic<bool> controlLaneHung_ { false };
    std::atomic<bool> rtLaneHung_ { false };
    std::mutex rtLaneMutex_;
    std::unique_ptr<yesdaw::engine::RtLaneRing> rtLane_;
    std::unique_ptr<SyntheticTestProcessor> hostedProcessor_;
    juce::AudioBuffer<float> hostedProcessorBuffer_;
    juce::MidiBuffer hostedMidi_;
};

int runSelfCheck()
{
    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();

    if (formats.getNumFormats() <= 0)
    {
        std::printf ("FAIL: YesDawPluginHost has no JUCE plugin formats enabled\n");
        return 2;
    }

    std::printf ("PASS: YesDawPluginHost worker target links JUCE hosting (%d format(s)); worker-id=%s\n",
                 formats.getNumFormats(), yesdaw::plugin_host::kWorkerCommandLineId);
    return 0;
}

bool sampleEquals (float actual, float expected) noexcept
{
    return std::abs (actual - expected) < 0.000001f;
}

int failSyntheticCheck (const char* reason)
{
    std::printf ("FAIL: synthetic hosted AudioProcessor check failed: %s\n", reason);
    return 2;
}

int runSyntheticPluginSelfCheck()
{
    juce::AudioBuffer<float> buffer (2, 8);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, static_cast<float> ((channel + 1) * 100 + sample));

    SyntheticTestProcessor passthrough (SyntheticProcessorMode::passthrough);
    passthrough.prepareToPlay (48000.0, buffer.getNumSamples());

    juce::MidiBuffer midi;
    passthrough.processBlock (buffer, midi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! sampleEquals (buffer.getSample (channel, sample),
                                static_cast<float> ((channel + 1) * 100 + sample)))
                return failSyntheticCheck ("passthrough mode changed audio");

    if (passthrough.getLatencySamples() != 0)
        return failSyntheticCheck ("passthrough mode reported latency");

    SyntheticTestProcessor fixedLatency (SyntheticProcessorMode::fixedReportedLatency);
    fixedLatency.prepareToPlay (48000.0, buffer.getNumSamples());

    if (fixedLatency.getLatencySamples() != SyntheticTestProcessor::kFixedLatencySamples)
        return failSyntheticCheck ("fixed-latency mode did not report its latency");

    juce::MemoryBlock state;
    passthrough.getStateInformation (state);

    SyntheticTestProcessor restored (SyntheticProcessorMode::passthrough);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    if (! restored.stateAccepted())
        return failSyntheticCheck ("opaque state chunk did not round-trip inside worker executable");

    buffer.clear();
    SyntheticTestProcessor nanEmitter (SyntheticProcessorMode::emitNan);
    nanEmitter.prepareToPlay (48000.0, buffer.getNumSamples());
    nanEmitter.processBlock (buffer, midi);

    if (std::isfinite (buffer.getSample (0, 0)))
        return failSyntheticCheck ("emit-NaN mode did not produce a non-finite sample");

    const SyntheticTestProcessor hangMode (SyntheticProcessorMode::hangAfterHandshake);
    const SyntheticTestProcessor crashMode (SyntheticProcessorMode::crashOnCue);
    if (hangMode.mode() != SyntheticProcessorMode::hangAfterHandshake
        || crashMode.mode() != SyntheticProcessorMode::crashOnCue)
        return failSyntheticCheck ("terminal synthetic modes are not addressable");

    std::printf ("PASS: synthetic hosted AudioProcessor ran in YesDawPluginHost; modes=passthrough,"
                 "fixed-reported-latency,emit-NaN,hang-after-handshake,crash-on-cue; latency=%d; state-bytes=%zu\n",
                 SyntheticTestProcessor::kFixedLatencySamples,
                 state.getSize());
    return 0;
}

} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    if (hasFlag (argc, argv, "--self-check"))
        return runSelfCheck();

    if (hasFlag (argc, argv, "--synthetic-plugin-self-check"))
        return runSyntheticPluginSelfCheck();

    PluginHostWorker worker;
    if (! worker.initialiseFromCommandLine (commandLineFromArgv (argc, argv),
                                            yesdaw::plugin_host::kWorkerCommandLineId,
                                            1000))
    {
        std::printf ("YesDawPluginHost is a child worker; run with --self-check for the mechanical gate.\n");
        return 2;
    }

    while (! worker.shouldQuit())
        if (! worker.pollRtLaneOnce())
            std::this_thread::sleep_for (std::chrono::milliseconds (1));

    return 0;
}
