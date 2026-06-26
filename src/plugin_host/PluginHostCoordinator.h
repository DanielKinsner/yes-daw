#pragma once

#include "plugin_host/PluginHostProtocol.h"

#include <juce_events/juce_events.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <utility>

namespace yesdaw::plugin_host {

class PluginHostCoordinator final : private juce::ChildProcessCoordinator
{
public:
    enum class HandshakeStatus
    {
        success,
        launchFailed,
        readyTimeout,
        probeSendFailed,
        echoTimeout,
        connectionLost
    };

    struct HandshakeResult
    {
        HandshakeStatus status { HandshakeStatus::launchFailed };
        bool readySeen { false };
        bool probeEchoed { false };
    };

    explicit PluginHostCoordinator (std::chrono::milliseconds timeout = std::chrono::milliseconds (4000))
        : timeout_ (timeout)
    {
    }

    ~PluginHostCoordinator() override
    {
        stop();
    }

    HandshakeResult launchAndHandshake (const juce::File& workerExecutable)
    {
        stop();
        resetState();

        if (! launchWorkerProcess (workerExecutable, kWorkerCommandLineId, static_cast<int> (timeout_.count())))
            return result (HandshakeStatus::launchFailed);

        if (! waitFor ([this] { return readySeen_ || connectionLost_; }))
            return result (HandshakeStatus::readyTimeout);

        if (connectionLost_)
            return result (HandshakeStatus::connectionLost);

        if (! sendMessageToWorker (makeMessage (kHandshakeProbeMessage)))
            return result (HandshakeStatus::probeSendFailed);

        if (! waitFor ([this] { return probeEchoed_ || connectionLost_; }))
            return result (HandshakeStatus::echoTimeout);

        if (connectionLost_)
            return result (HandshakeStatus::connectionLost);

        return result (HandshakeStatus::success);
    }

    void stop()
    {
        killWorkerProcess();
    }

    void handleMessageFromWorker (const juce::MemoryBlock& message) override
    {
        {
            std::lock_guard<std::mutex> lock (mutex_);

            if (messageMatches (message, kWorkerReadyMessage))
                readySeen_ = true;
            else if (messageMatches (message, kHandshakeProbeMessage))
                probeEchoed_ = true;
        }

        cv_.notify_all();
    }

    void handleConnectionLost() override
    {
        {
            std::lock_guard<std::mutex> lock (mutex_);
            connectionLost_ = true;
        }

        cv_.notify_all();
    }

private:
    static juce::MemoryBlock makeMessage (const char* text)
    {
        return juce::MemoryBlock (text, std::strlen (text) + 1);
    }

    static bool messageMatches (const juce::MemoryBlock& message, const char* text) noexcept
    {
        const auto expectedSize = std::strlen (text) + 1;
        return message.getSize() == expectedSize
            && std::memcmp (message.getData(), text, expectedSize) == 0;
    }

    template <typename Predicate>
    bool waitFor (Predicate&& predicate)
    {
        std::unique_lock<std::mutex> lock (mutex_);
        return cv_.wait_for (lock, timeout_, std::forward<Predicate> (predicate));
    }

    void resetState()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        readySeen_ = false;
        probeEchoed_ = false;
        connectionLost_ = false;
    }

    HandshakeResult result (HandshakeStatus status) const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return { status, readySeen_, probeEchoed_ };
    }

    const std::chrono::milliseconds timeout_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool readySeen_ { false };
    bool probeEchoed_ { false };
    bool connectionLost_ { false };
};

} // namespace yesdaw::plugin_host
