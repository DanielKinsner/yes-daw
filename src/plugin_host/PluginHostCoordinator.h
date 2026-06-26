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

    enum class GraphChangeCommandKind
    {
        none,
        bypassAndRecompile
    };

    enum class GraphChangeCommandStatus
    {
        noAction,
        commandReady
    };

    enum class BlacklistEscalationStatus
    {
        noAction,
        escalationReady
    };

    enum class BlacklistPolicyDecisionRequestStatus
    {
        noAction,
        requestReady
    };

    enum class BlacklistPolicyDecisionCommandKind
    {
        none,
        requestPolicyDecision
    };

    enum class BlacklistPolicyDecisionCommandStatus
    {
        noAction,
        commandReady
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

    struct BlacklistCandidateStatus
    {
        HostFailureKind failureKind { HostFailureKind::none };
        bool candidate { false };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
    };

    struct BlacklistEscalation
    {
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadEscalationRequested { false };
    };

    struct FailureActionRequest
    {
        FailureActionKind action { FailureActionKind::none };
        HostFailureKind failureKind { HostFailureKind::none };
        bool bypassRequested { false };
        bool recompileRequested { false };
    };

    struct GraphChangeCommand
    {
        GraphChangeCommandKind command { GraphChangeCommandKind::none };
        HostFailureKind failureKind { HostFailureKind::none };
        bool bypassRequested { false };
        bool recompileRequested { false };
    };

    struct GraphChangeCommandResult
    {
        GraphChangeCommandStatus status { GraphChangeCommandStatus::noAction };
        FailureActionRequest drainedRequest;
        GraphChangeCommand command;
        bool pendingRequestConsumed { false };
        bool graphRecompileExecuted { false };
    };

    struct DeferredGraphChangeCommandStatus
    {
        GraphChangeCommandStatus status { GraphChangeCommandStatus::noAction };
        GraphChangeCommandResult lastResult;
        bool commandRecorded { false };
        bool graphRecompileExecuted { false };
    };

    struct BlacklistEscalationResult
    {
        BlacklistEscalationStatus status { BlacklistEscalationStatus::noAction };
        BlacklistCandidateStatus drainedCandidate;
        BlacklistEscalation escalation;
        bool pendingCandidateConsumed { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct DeferredBlacklistEscalationStatus
    {
        BlacklistEscalationStatus status { BlacklistEscalationStatus::noAction };
        BlacklistEscalationResult lastResult;
        bool escalationRecorded { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistPolicyDecisionRequest
    {
        BlacklistPolicyDecisionRequestStatus status { BlacklistPolicyDecisionRequestStatus::noAction };
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadPolicyDecisionRequested { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct BlacklistPolicyDecisionCommand
    {
        BlacklistPolicyDecisionCommandKind command { BlacklistPolicyDecisionCommandKind::none };
        HostFailureKind failureKind { HostFailureKind::none };
        bool crashCandidate { false };
        bool watchdogTimeoutCandidate { false };
        bool controlThreadPolicyDecisionRequested { false };
    };

    struct BlacklistPolicyDecisionCommandResult
    {
        BlacklistPolicyDecisionCommandStatus status { BlacklistPolicyDecisionCommandStatus::noAction };
        BlacklistPolicyDecisionRequest drainedRequest;
        BlacklistPolicyDecisionCommand command;
        bool pendingRequestConsumed { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
    };

    struct DeferredBlacklistPolicyDecisionCommandStatus
    {
        BlacklistPolicyDecisionCommandStatus status { BlacklistPolicyDecisionCommandStatus::noAction };
        BlacklistPolicyDecisionCommandResult lastResult;
        bool commandRecorded { false };
        bool blacklistPolicyApplied { false };
        bool blacklistStatePersisted { false };
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

    BlacklistCandidateStatus blacklistCandidateStatus() const
    {
        return blacklistCandidateStatusFor (hostFailureReport());
    }

    FailureActionRequest queueFailureActionRequest (FailureActionRequest request)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingFailureAction_ = canCreateGraphChangeCommand (request) ? request : FailureActionRequest {};
        return pendingFailureAction_;
    }

    FailureActionRequest queueFailureActionRequestForCurrentFailure()
    {
        return queueFailureActionRequest (failureActionRequest());
    }

    FailureActionRequest pendingFailureActionRequest() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingFailureAction_;
    }

    FailureActionRequest drainPendingFailureActionRequest()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto request = pendingFailureAction_;
        pendingFailureAction_ = {};
        return request;
    }

    GraphChangeCommandResult drainPendingFailureActionRequestToControlCommand()
    {
        const auto request = drainPendingFailureActionRequest();
        if (! canCreateGraphChangeCommand (request))
            return {};

        return { GraphChangeCommandStatus::commandReady,
                 request,
                 { GraphChangeCommandKind::bypassAndRecompile,
                   request.failureKind,
                   request.bypassRequested,
                   request.recompileRequested },
                 true,
                 false };
    }

    DeferredGraphChangeCommandStatus recordDeferredGraphChangeCommandResult (GraphChangeCommandResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordGraphChangeCommandResult (result))
        {
            lastDeferredGraphChangeCommandResult_ = result;
            deferredGraphChangeCommandRecorded_ = true;
        }
        else
        {
            lastDeferredGraphChangeCommandResult_ = {};
            deferredGraphChangeCommandRecorded_ = false;
        }

        return deferredGraphChangeCommandStatusLocked();
    }

    DeferredGraphChangeCommandStatus deferredGraphChangeCommandStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredGraphChangeCommandStatusLocked();
    }

    DeferredGraphChangeCommandStatus acknowledgeDeferredGraphChangeCommandStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredGraphChangeCommandResult_ = {};
        deferredGraphChangeCommandRecorded_ = false;
        return deferredGraphChangeCommandStatusLocked();
    }

    BlacklistCandidateStatus queueBlacklistCandidate (BlacklistCandidateStatus status)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingBlacklistCandidate_ = canQueueBlacklistCandidate (status) ? status : BlacklistCandidateStatus {};
        return pendingBlacklistCandidate_;
    }

    BlacklistCandidateStatus queueBlacklistCandidateForCurrentFailure()
    {
        return queueBlacklistCandidate (blacklistCandidateStatus());
    }

    BlacklistCandidateStatus pendingBlacklistCandidateStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingBlacklistCandidate_;
    }

    BlacklistCandidateStatus drainPendingBlacklistCandidateStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto status = pendingBlacklistCandidate_;
        pendingBlacklistCandidate_ = {};
        return status;
    }

    BlacklistEscalationResult drainPendingBlacklistCandidateToControlEscalation()
    {
        const auto candidate = drainPendingBlacklistCandidateStatus();
        if (! canCreateBlacklistEscalation (candidate))
            return {};

        return { BlacklistEscalationStatus::escalationReady,
                 candidate,
                 { candidate.failureKind,
                   candidate.crashCandidate,
                   candidate.watchdogTimeoutCandidate,
                   true },
                 true,
                 false,
                 false };
    }

    DeferredBlacklistEscalationStatus recordDeferredBlacklistEscalationResult (BlacklistEscalationResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordBlacklistEscalationResult (result))
        {
            lastDeferredBlacklistEscalationResult_ = result;
            deferredBlacklistEscalationRecorded_ = true;
        }
        else
        {
            lastDeferredBlacklistEscalationResult_ = {};
            deferredBlacklistEscalationRecorded_ = false;
        }

        return deferredBlacklistEscalationStatusLocked();
    }

    DeferredBlacklistEscalationStatus deferredBlacklistEscalationStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredBlacklistEscalationStatusLocked();
    }

    DeferredBlacklistEscalationStatus acknowledgeDeferredBlacklistEscalationStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredBlacklistEscalationResult_ = {};
        deferredBlacklistEscalationRecorded_ = false;
        return deferredBlacklistEscalationStatusLocked();
    }

    BlacklistPolicyDecisionRequest blacklistPolicyDecisionRequest() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return blacklistPolicyDecisionRequestFor (deferredBlacklistEscalationStatusLocked());
    }

    BlacklistPolicyDecisionRequest queueBlacklistPolicyDecisionRequestForDeferredEscalation()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        pendingBlacklistPolicyDecisionRequest_ =
            blacklistPolicyDecisionRequestFor (deferredBlacklistEscalationStatusLocked());
        return pendingBlacklistPolicyDecisionRequest_;
    }

    BlacklistPolicyDecisionRequest pendingBlacklistPolicyDecisionRequest() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return pendingBlacklistPolicyDecisionRequest_;
    }

    BlacklistPolicyDecisionRequest drainPendingBlacklistPolicyDecisionRequest()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        const auto request = pendingBlacklistPolicyDecisionRequest_;
        pendingBlacklistPolicyDecisionRequest_ = {};
        return request;
    }

    BlacklistPolicyDecisionCommandResult drainPendingBlacklistPolicyDecisionRequestToControlCommand()
    {
        const auto request = drainPendingBlacklistPolicyDecisionRequest();
        if (! canCreateBlacklistPolicyDecisionCommand (request))
            return {};

        return { BlacklistPolicyDecisionCommandStatus::commandReady,
                 request,
                 { BlacklistPolicyDecisionCommandKind::requestPolicyDecision,
                   request.failureKind,
                   request.crashCandidate,
                   request.watchdogTimeoutCandidate,
                   request.controlThreadPolicyDecisionRequested },
                 true,
                 false,
                 false };
    }

    DeferredBlacklistPolicyDecisionCommandStatus recordDeferredBlacklistPolicyDecisionCommandResult (
        BlacklistPolicyDecisionCommandResult result)
    {
        std::lock_guard<std::mutex> lock (mutex_);
        if (canRecordBlacklistPolicyDecisionCommandResult (result))
        {
            lastDeferredBlacklistPolicyDecisionCommandResult_ = result;
            deferredBlacklistPolicyDecisionCommandRecorded_ = true;
        }
        else
        {
            lastDeferredBlacklistPolicyDecisionCommandResult_ = {};
            deferredBlacklistPolicyDecisionCommandRecorded_ = false;
        }

        return deferredBlacklistPolicyDecisionCommandStatusLocked();
    }

    DeferredBlacklistPolicyDecisionCommandStatus deferredBlacklistPolicyDecisionCommandStatus() const
    {
        std::lock_guard<std::mutex> lock (mutex_);
        return deferredBlacklistPolicyDecisionCommandStatusLocked();
    }

    DeferredBlacklistPolicyDecisionCommandStatus acknowledgeDeferredBlacklistPolicyDecisionCommandStatus()
    {
        std::lock_guard<std::mutex> lock (mutex_);
        lastDeferredBlacklistPolicyDecisionCommandResult_ = {};
        deferredBlacklistPolicyDecisionCommandRecorded_ = false;
        return deferredBlacklistPolicyDecisionCommandStatusLocked();
    }

    static FailureActionRequest failureActionRequestFor (HostFailureReport report) noexcept
    {
        if (report.kind == HostFailureKind::none)
            return {};

        return { FailureActionKind::bypassAndRecompile, report.kind, true, true };
    }

    static BlacklistCandidateStatus blacklistCandidateStatusFor (HostFailureReport report) noexcept
    {
        if (report.kind == HostFailureKind::crash)
            return { report.kind, true, true, false };

        if (report.kind == HostFailureKind::watchdogTimeout)
            return { report.kind, true, false, true };

        return {};
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

    static bool canCreateGraphChangeCommand (FailureActionRequest request) noexcept
    {
        return request.action == FailureActionKind::bypassAndRecompile
            && request.failureKind != HostFailureKind::none;
    }

    static bool canRecordGraphChangeCommandResult (GraphChangeCommandResult result) noexcept
    {
        return result.status == GraphChangeCommandStatus::commandReady
            && result.pendingRequestConsumed
            && ! result.graphRecompileExecuted
            && canCreateGraphChangeCommand (result.drainedRequest)
            && result.command.command == GraphChangeCommandKind::bypassAndRecompile
            && result.command.failureKind == result.drainedRequest.failureKind
            && result.command.bypassRequested == result.drainedRequest.bypassRequested
            && result.command.recompileRequested == result.drainedRequest.recompileRequested;
    }

    static bool canQueueBlacklistCandidate (BlacklistCandidateStatus status) noexcept
    {
        if (! status.candidate)
            return false;

        return blacklistCandidateMatches (status, blacklistCandidateStatusFor ({ status.failureKind, true, false, false }));
    }

    static bool canCreateBlacklistEscalation (BlacklistCandidateStatus status) noexcept
    {
        return canQueueBlacklistCandidate (status);
    }

    static bool canRecordBlacklistEscalationResult (BlacklistEscalationResult result) noexcept
    {
        return result.status == BlacklistEscalationStatus::escalationReady
            && result.pendingCandidateConsumed
            && ! result.blacklistPolicyApplied
            && ! result.blacklistStatePersisted
            && canCreateBlacklistEscalation (result.drainedCandidate)
            && result.escalation.controlThreadEscalationRequested
            && result.escalation.failureKind == result.drainedCandidate.failureKind
            && result.escalation.crashCandidate == result.drainedCandidate.crashCandidate
            && result.escalation.watchdogTimeoutCandidate == result.drainedCandidate.watchdogTimeoutCandidate;
    }

    static BlacklistPolicyDecisionRequest blacklistPolicyDecisionRequestFor (
        DeferredBlacklistEscalationStatus status) noexcept
    {
        if (! canCreateBlacklistPolicyDecisionRequest (status))
            return {};

        const auto escalation = status.lastResult.escalation;
        return { BlacklistPolicyDecisionRequestStatus::requestReady,
                 escalation.failureKind,
                 escalation.crashCandidate,
                 escalation.watchdogTimeoutCandidate,
                 true,
                 false,
                 false };
    }

    static bool canCreateBlacklistPolicyDecisionRequest (DeferredBlacklistEscalationStatus status) noexcept
    {
        return status.status == BlacklistEscalationStatus::escalationReady
            && status.escalationRecorded
            && ! status.blacklistPolicyApplied
            && ! status.blacklistStatePersisted
            && canRecordBlacklistEscalationResult (status.lastResult);
    }

    static bool canCreateBlacklistPolicyDecisionCommand (BlacklistPolicyDecisionRequest request) noexcept
    {
        if (request.status != BlacklistPolicyDecisionRequestStatus::requestReady
            || request.failureKind == HostFailureKind::none
            || ! request.controlThreadPolicyDecisionRequested
            || request.blacklistPolicyApplied
            || request.blacklistStatePersisted)
            return false;

        if (request.failureKind == HostFailureKind::crash)
            return request.crashCandidate && ! request.watchdogTimeoutCandidate;

        if (request.failureKind == HostFailureKind::watchdogTimeout)
            return ! request.crashCandidate && request.watchdogTimeoutCandidate;

        return false;
    }

    static bool canRecordBlacklistPolicyDecisionCommandResult (
        BlacklistPolicyDecisionCommandResult result) noexcept
    {
        return result.status == BlacklistPolicyDecisionCommandStatus::commandReady
            && result.pendingRequestConsumed
            && ! result.blacklistPolicyApplied
            && ! result.blacklistStatePersisted
            && canCreateBlacklistPolicyDecisionCommand (result.drainedRequest)
            && result.command.command == BlacklistPolicyDecisionCommandKind::requestPolicyDecision
            && result.command.failureKind == result.drainedRequest.failureKind
            && result.command.crashCandidate == result.drainedRequest.crashCandidate
            && result.command.watchdogTimeoutCandidate == result.drainedRequest.watchdogTimeoutCandidate
            && result.command.controlThreadPolicyDecisionRequested
                == result.drainedRequest.controlThreadPolicyDecisionRequested;
    }

    static bool blacklistCandidateMatches (BlacklistCandidateStatus actual,
                                           BlacklistCandidateStatus expected) noexcept
    {
        return actual.failureKind == expected.failureKind
            && actual.candidate == expected.candidate
            && actual.crashCandidate == expected.crashCandidate
            && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate;
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

    DeferredGraphChangeCommandStatus deferredGraphChangeCommandStatusLocked() const
    {
        return { deferredGraphChangeCommandRecorded_ ? lastDeferredGraphChangeCommandResult_.status
                                                     : GraphChangeCommandStatus::noAction,
                 lastDeferredGraphChangeCommandResult_,
                 deferredGraphChangeCommandRecorded_,
                 lastDeferredGraphChangeCommandResult_.graphRecompileExecuted };
    }

    DeferredBlacklistEscalationStatus deferredBlacklistEscalationStatusLocked() const
    {
        return { deferredBlacklistEscalationRecorded_ ? lastDeferredBlacklistEscalationResult_.status
                                                      : BlacklistEscalationStatus::noAction,
                 lastDeferredBlacklistEscalationResult_,
                 deferredBlacklistEscalationRecorded_,
                 lastDeferredBlacklistEscalationResult_.blacklistPolicyApplied,
                 lastDeferredBlacklistEscalationResult_.blacklistStatePersisted };
    }

    DeferredBlacklistPolicyDecisionCommandStatus deferredBlacklistPolicyDecisionCommandStatusLocked() const
    {
        return { deferredBlacklistPolicyDecisionCommandRecorded_
                     ? lastDeferredBlacklistPolicyDecisionCommandResult_.status
                     : BlacklistPolicyDecisionCommandStatus::noAction,
                 lastDeferredBlacklistPolicyDecisionCommandResult_,
                 deferredBlacklistPolicyDecisionCommandRecorded_,
                 lastDeferredBlacklistPolicyDecisionCommandResult_.blacklistPolicyApplied,
                 lastDeferredBlacklistPolicyDecisionCommandResult_.blacklistStatePersisted };
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
    FailureActionRequest pendingFailureAction_;
    BlacklistCandidateStatus pendingBlacklistCandidate_;
    BlacklistPolicyDecisionRequest pendingBlacklistPolicyDecisionRequest_;
    GraphChangeCommandResult lastDeferredGraphChangeCommandResult_;
    BlacklistEscalationResult lastDeferredBlacklistEscalationResult_;
    BlacklistPolicyDecisionCommandResult lastDeferredBlacklistPolicyDecisionCommandResult_;
    bool launchAttempted_ { false };
    bool readySeen_ { false };
    bool probeEchoed_ { false };
    bool stopRequested_ { false };
    bool watchdogTimedOut_ { false };
    bool watchdogKillRequested_ { false };
    bool crashObservationRequested_ { false };
    bool connectionLost_ { false };
    bool deferredGraphChangeCommandRecorded_ { false };
    bool deferredBlacklistEscalationRecorded_ { false };
    bool deferredBlacklistPolicyDecisionCommandRecorded_ { false };
};

} // namespace yesdaw::plugin_host
