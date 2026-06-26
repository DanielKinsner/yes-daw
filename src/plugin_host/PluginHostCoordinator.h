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
        notStarted,
        success,
        launchFailed,
        readyTimeout,
        probeSendFailed,
        echoTimeout,
        connectionLost
    };

    enum class StopStatus
    {
        notStarted,
        stopped,
        stopTimeout
    };

    enum class ChildState
    {
        idle,
        launching,
        ready,
        handshaking,
        running,
        stopping,
        stopped,
        lost
    };

    struct HandshakeResult
    {
        HandshakeStatus status { HandshakeStatus::notStarted };
        bool readySeen { false };
        bool probeEchoed { false };
    };

    struct StopResult
    {
        StopStatus status { StopStatus::notStarted };
        bool connectionLostSeen { false };
    };

    struct ChildStatus
    {
        ChildState state { ChildState::idle };
        HandshakeStatus handshakeStatus { HandshakeStatus::notStarted };
        StopStatus stopStatus { StopStatus::notStarted };
        bool launchAttempted { false };
        bool readySeen { false };
        bool probeEchoed { false };
        bool stopRequested { false };
        bool connectionLostSeen { false };
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

        {
            std::lock_guard<std::mutex> lock (mutex_);
            launchAttempted_ = true;
            childState_ = ChildState::launching;
        }

        if (! launchWorkerProcess (workerExecutable, kWorkerCommandLineId, static_cast<int> (timeout_.count())))
            return result (HandshakeStatus::launchFailed);

        if (! waitFor ([this] { return readySeen_ || connectionLost_; }))
            return result (HandshakeStatus::readyTimeout);

        if (connectionLost())
            return result (HandshakeStatus::connectionLost);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            childState_ = ChildState::handshaking;
        }

        if (! sendMessageToWorker (makeMessage (kHandshakeProbeMessage)))
            return result (HandshakeStatus::probeSendFailed);

        if (! waitFor ([this] { return probeEchoed_ || connectionLost_; }))
            return result (HandshakeStatus::echoTimeout);

        if (connectionLost())
            return result (HandshakeStatus::connectionLost);

        return result (HandshakeStatus::success);
    }

    StopResult requestStopAndWait()
    {
        {
            std::lock_guard<std::mutex> lock (mutex_);
            stopRequested_ = true;
            childState_ = ChildState::stopping;
        }

        killWorkerProcess();

        if (! waitFor ([this] { return connectionLost_; }))
            return stopResult (StopStatus::stopTimeout);

        return stopResult (StopStatus::stopped);
    }

    void stop()
    {
        killWorkerProcess();
    }

    ChildStatus status() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return { childState_, handshakeStatus_, stopStatus_, launchAttempted_, readySeen_, probeEchoed_,
                 stopRequested_, connectionLost_ };
    }

    void handleMessageFromWorker (const juce::MemoryBlock& message) override
    {
        {
            std::lock_guard<std::mutex> lock (mutex_);

            if (messageMatches (message, kWorkerReadyMessage))
            {
                readySeen_ = true;
                childState_ = ChildState::ready;
            }
            else if (messageMatches (message, kHandshakeProbeMessage))
            {
                probeEchoed_ = true;
                childState_ = ChildState::running;
            }
        }

        cv_.notify_all();
    }

    void handleConnectionLost() override
    {
        {
            std::lock_guard<std::mutex> lock (mutex_);
            connectionLost_ = true;
            childState_ = stopRequested_ ? ChildState::stopped : ChildState::lost;
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
        launchAttempted_ = false;
        stopRequested_ = false;
        childState_ = ChildState::idle;
        handshakeStatus_ = HandshakeStatus::notStarted;
        stopStatus_ = StopStatus::notStarted;
    }

    bool connectionLost() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return connectionLost_;
    }

    HandshakeResult result (HandshakeStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        handshakeStatus_ = status;
        if (status == HandshakeStatus::success)
            childState_ = ChildState::running;
        return { status, readySeen_, probeEchoed_ };
    }

    StopResult stopResult (StopStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        stopStatus_ = status;
        return { status, connectionLost_ };
    }

    const std::chrono::milliseconds timeout_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    ChildState childState_ { ChildState::idle };
    HandshakeStatus handshakeStatus_ { HandshakeStatus::notStarted };
    StopStatus stopStatus_ { StopStatus::notStarted };
    bool launchAttempted_ { false };
    bool readySeen_ { false };
    bool probeEchoed_ { false };
    bool stopRequested_ { false };
    bool connectionLost_ { false };
};

} // namespace yesdaw::plugin_host
