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

    enum class WatchdogStatus
    {
        notStarted,
        launchFailed,
        readyTimeout,
        probeSendFailed,
        timeoutKilled,
        killObservationTimeout,
        connectionLost,
        unexpectedResponse
    };

    enum class CrashStatus
    {
        notStarted,
        launchFailed,
        readyTimeout,
        connectionLost,
        observationTimeout
    };

    enum class HostFailureKind
    {
        none,
        crash,
        watchdogTimeout
    };

    enum class FailureActionKind
    {
        none,
        bypassAndRecompile
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

    struct WatchdogResult
    {
        WatchdogStatus status { WatchdogStatus::notStarted };
        bool readySeen { false };
        bool watchdogTimedOut { false };
        bool killRequested { false };
        bool connectionLostSeen { false };
    };

    struct CrashResult
    {
        CrashStatus status { CrashStatus::notStarted };
        bool readySeen { false };
        bool crashObservationRequested { false };
        bool connectionLostSeen { false };
        HostFailureKind failureKind { HostFailureKind::none };
    };

    struct HostFailureReport
    {
        HostFailureKind kind { HostFailureKind::none };
        bool connectionLostSeen { false };
        bool watchdogTimedOut { false };
        bool watchdogKillRequested { false };
    };

    struct FailureActionRequest
    {
        FailureActionKind action { FailureActionKind::none };
        HostFailureKind failureKind { HostFailureKind::none };
        bool bypassRequested { false };
        bool recompileRequested { false };
    };

    struct ChildStatus
    {
        ChildState state { ChildState::idle };
        HandshakeStatus handshakeStatus { HandshakeStatus::notStarted };
        StopStatus stopStatus { StopStatus::notStarted };
        WatchdogStatus watchdogStatus { WatchdogStatus::notStarted };
        CrashStatus crashStatus { CrashStatus::notStarted };
        HostFailureKind failureKind { HostFailureKind::none };
        bool launchAttempted { false };
        bool readySeen { false };
        bool probeEchoed { false };
        bool stopRequested { false };
        bool watchdogTimedOut { false };
        bool watchdogKillRequested { false };
        bool crashObservationRequested { false };
        bool connectionLostSeen { false };
    };

    explicit PluginHostCoordinator (std::chrono::milliseconds timeout = std::chrono::milliseconds (4000),
                                    std::chrono::milliseconds watchdogTimeout = std::chrono::milliseconds (250))
        : timeout_ (timeout),
          watchdogTimeout_ (watchdogTimeout)
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

    WatchdogResult launchAndExpectWatchdogTimeout (const juce::File& workerExecutable)
    {
        stop();
        resetState();

        {
            std::lock_guard<std::mutex> lock (mutex_);
            launchAttempted_ = true;
            childState_ = ChildState::launching;
        }

        if (! launchWorkerProcess (workerExecutable, kWorkerCommandLineId, static_cast<int> (timeout_.count())))
            return watchdogResult (WatchdogStatus::launchFailed);

        if (! waitFor ([this] { return readySeen_ || connectionLost_; }))
            return watchdogResult (WatchdogStatus::readyTimeout);

        if (connectionLost())
            return watchdogResult (WatchdogStatus::connectionLost);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            childState_ = ChildState::handshaking;
        }

        if (! sendMessageToWorker (makeMessage (kWatchdogProbeMessage)))
            return watchdogResult (WatchdogStatus::probeSendFailed);

        if (waitFor (watchdogTimeout_, [this] { return probeEchoed_ || connectionLost_; }))
        {
            if (connectionLost())
                return watchdogResult (WatchdogStatus::connectionLost);

            return watchdogResult (WatchdogStatus::unexpectedResponse);
        }

        {
            std::lock_guard<std::mutex> lock (mutex_);
            handshakeStatus_ = HandshakeStatus::echoTimeout;
            watchdogTimedOut_ = true;
            watchdogKillRequested_ = true;
            childState_ = ChildState::stopping;
        }

        killWorkerProcess();

        if (! waitFor ([this] { return connectionLost_; }))
            return watchdogResult (WatchdogStatus::killObservationTimeout);

        return watchdogResult (WatchdogStatus::timeoutKilled);
    }

    CrashResult launchAndExpectCrash (const juce::File& workerExecutable)
    {
        stop();
        resetState();

        {
            std::lock_guard<std::mutex> lock (mutex_);
            launchAttempted_ = true;
            childState_ = ChildState::launching;
        }

        if (! launchWorkerProcess (workerExecutable, kWorkerCommandLineId, static_cast<int> (timeout_.count())))
            return crashResult (CrashStatus::launchFailed);

        if (! waitFor ([this] { return readySeen_ || connectionLost_; }))
            return crashResult (CrashStatus::readyTimeout);

        if (connectionLost())
            return crashResult (CrashStatus::connectionLost);

        {
            std::lock_guard<std::mutex> lock (mutex_);
            childState_ = ChildState::stopping;
            crashObservationRequested_ = true;
        }

        killWorkerProcess();

        if (! waitFor ([this] { return connectionLost_; }))
            return crashResult (CrashStatus::observationTimeout);

        return crashResult (CrashStatus::connectionLost);
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
        return { childState_, handshakeStatus_, stopStatus_, watchdogStatus_, crashStatus_, failureKind_,
                 launchAttempted_, readySeen_, probeEchoed_, stopRequested_, watchdogTimedOut_,
                 watchdogKillRequested_, crashObservationRequested_, connectionLost_ };
    }

    HostFailureReport hostFailureReport() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return hostFailureReportLocked();
    }

    FailureActionRequest failureActionRequest() const
    {
        return failureActionRequestFor (hostFailureReport());
    }

    static FailureActionRequest failureActionRequestFor (HostFailureReport report) noexcept
    {
        if (report.kind == HostFailureKind::none)
            return {};

        return { FailureActionKind::bypassAndRecompile, report.kind, true, true };
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
            if (watchdogKillRequested_)
                failureKind_ = HostFailureKind::watchdogTimeout;
            else if (! stopRequested_)
                failureKind_ = HostFailureKind::crash;

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
        return waitFor (timeout_, std::forward<Predicate> (predicate));
    }

    template <typename Predicate>
    bool waitFor (std::chrono::milliseconds timeout, Predicate&& predicate)
    {
        std::unique_lock<std::mutex> lock (mutex_);
        return cv_.wait_for (lock, timeout, std::forward<Predicate> (predicate));
    }

    void resetState()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        readySeen_ = false;
        probeEchoed_ = false;
        connectionLost_ = false;
        launchAttempted_ = false;
        stopRequested_ = false;
        watchdogTimedOut_ = false;
        watchdogKillRequested_ = false;
        crashObservationRequested_ = false;
        childState_ = ChildState::idle;
        handshakeStatus_ = HandshakeStatus::notStarted;
        stopStatus_ = StopStatus::notStarted;
        watchdogStatus_ = WatchdogStatus::notStarted;
        crashStatus_ = CrashStatus::notStarted;
        failureKind_ = HostFailureKind::none;
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

    WatchdogResult watchdogResult (WatchdogStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        watchdogStatus_ = status;
        if (status == WatchdogStatus::timeoutKilled)
            childState_ = ChildState::stopped;
        return { status, readySeen_, watchdogTimedOut_, watchdogKillRequested_, connectionLost_ };
    }

    CrashResult crashResult (CrashStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        crashStatus_ = status;
        return { status, readySeen_, crashObservationRequested_, connectionLost_, failureKind_ };
    }

    HostFailureReport hostFailureReportLocked() const
    {
        return { failureKind_, connectionLost_, watchdogTimedOut_, watchdogKillRequested_ };
    }

    const std::chrono::milliseconds timeout_;
    const std::chrono::milliseconds watchdogTimeout_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    ChildState childState_ { ChildState::idle };
    HandshakeStatus handshakeStatus_ { HandshakeStatus::notStarted };
    StopStatus stopStatus_ { StopStatus::notStarted };
    WatchdogStatus watchdogStatus_ { WatchdogStatus::notStarted };
    CrashStatus crashStatus_ { CrashStatus::notStarted };
    HostFailureKind failureKind_ { HostFailureKind::none };
    bool launchAttempted_ { false };
    bool readySeen_ { false };
    bool probeEchoed_ { false };
    bool stopRequested_ { false };
    bool watchdogTimedOut_ { false };
    bool watchdogKillRequested_ { false };
    bool crashObservationRequested_ { false };
    bool connectionLost_ { false };
};

} // namespace yesdaw::plugin_host
