// YES DAW - minimal plugin host child executable (ADR-0015).
//
// This is only the worker/layering checkpoint: it proves there is a separate executable that owns JUCE
// plugin-hosting modules. It does not scan, load plugins, mmap shared memory, launch from the app, or run
// the watchdog/coordinator yet.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <thread>

namespace {

constexpr const char* kWorkerCommandLineId = "yesdawpluginhost";

bool hasFlag (int argc, char** argv, const char* flag) noexcept
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp (argv[i], flag) == 0)
            return true;

    return false;
}

juce::String commandLineFromArgv (int argc, char** argv)
{
    juce::StringArray args;

    for (int i = 1; i < argc; ++i)
        args.add (juce::String (argv[i]));

    return args.joinIntoString (" ");
}

class PluginHostWorker final : public juce::ChildProcessWorker
{
public:
    void handleConnectionMade() override
    {
        const char ready[] = "ready";
        sendMessageToCoordinator (juce::MemoryBlock (ready, sizeof (ready)));
    }

    void handleMessageFromCoordinator (const juce::MemoryBlock& message) override
    {
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

private:
    std::atomic<bool> shouldQuit_ { false };
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
                 formats.getNumFormats(), kWorkerCommandLineId);
    return 0;
}

} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    if (hasFlag (argc, argv, "--self-check"))
        return runSelfCheck();

    PluginHostWorker worker;
    if (! worker.initialiseFromCommandLine (commandLineFromArgv (argc, argv), kWorkerCommandLineId, 1000))
    {
        std::printf ("YesDawPluginHost is a child worker; run with --self-check for the mechanical gate.\n");
        return 2;
    }

    while (! worker.shouldQuit())
        std::this_thread::sleep_for (std::chrono::milliseconds (50));

    return 0;
}
