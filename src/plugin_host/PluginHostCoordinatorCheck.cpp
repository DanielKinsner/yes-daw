#include "plugin_host/PluginHostCoordinator.h"

#include <cstdio>

namespace {

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus;

    switch (status)
    {
        case Status::notStarted:      return "notStarted";
        case Status::success:         return "success";
        case Status::launchFailed:    return "launchFailed";
        case Status::readyTimeout:    return "readyTimeout";
        case Status::probeSendFailed: return "probeSendFailed";
        case Status::echoTimeout:     return "echoTimeout";
        case Status::connectionLost:  return "connectionLost";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::StopStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::StopStatus;

    switch (status)
    {
        case Status::notStarted:  return "notStarted";
        case Status::stopped:     return "stopped";
        case Status::stopTimeout: return "stopTimeout";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus;

    switch (status)
    {
        case Status::notStarted:             return "notStarted";
        case Status::launchFailed:           return "launchFailed";
        case Status::readyTimeout:           return "readyTimeout";
        case Status::probeSendFailed:        return "probeSendFailed";
        case Status::timeoutKilled:          return "timeoutKilled";
        case Status::killObservationTimeout: return "killObservationTimeout";
        case Status::connectionLost:         return "connectionLost";
        case Status::unexpectedResponse:     return "unexpectedResponse";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::CrashStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::CrashStatus;

    switch (status)
    {
        case Status::notStarted:         return "notStarted";
        case Status::launchFailed:       return "launchFailed";
        case Status::readyTimeout:       return "readyTimeout";
        case Status::connectionLost:     return "connectionLost";
        case Status::observationTimeout: return "observationTimeout";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind kind) noexcept
{
    using Kind = yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind;

    switch (kind)
    {
        case Kind::none:             return "none";
        case Kind::crash:            return "crash";
        case Kind::watchdogTimeout:  return "watchdogTimeout";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind action) noexcept
{
    using Action = yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind;

    switch (action)
    {
        case Action::none:                return "none";
        case Action::bypassAndRecompile:  return "bypassAndRecompile";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind command) noexcept
{
    using Command = yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind;

    switch (command)
    {
        case Command::none:                return "none";
        case Command::bypassAndRecompile:  return "bypassAndRecompile";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus;

    switch (status)
    {
        case Status::noAction:      return "noAction";
        case Status::commandReady:  return "commandReady";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus;

    switch (status)
    {
        case Status::noAction:          return "noAction";
        case Status::escalationReady:   return "escalationReady";
    }

    return "unknown";
}

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::ChildState state) noexcept
{
    using State = yesdaw::plugin_host::PluginHostCoordinator::ChildState;

    switch (state)
    {
        case State::idle:        return "idle";
        case State::launching:   return "launching";
        case State::ready:       return "ready";
        case State::handshaking: return "handshaking";
        case State::running:     return "running";
        case State::stopping:    return "stopping";
        case State::stopped:     return "stopped";
        case State::lost:        return "lost";
    }

    return "unknown";
}

bool requestMatches (yesdaw::plugin_host::PluginHostCoordinator::FailureActionRequest actual,
                     yesdaw::plugin_host::PluginHostCoordinator::FailureActionRequest expected) noexcept
{
    return actual.action == expected.action
        && actual.failureKind == expected.failureKind
        && actual.bypassRequested == expected.bypassRequested
        && actual.recompileRequested == expected.recompileRequested;
}

bool commandMatches (yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommand actual,
                     yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommand expected) noexcept
{
    return actual.command == expected.command
        && actual.failureKind == expected.failureKind
        && actual.bypassRequested == expected.bypassRequested
        && actual.recompileRequested == expected.recompileRequested;
}

bool commandResultMatches (yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandResult actual,
                           yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandResult expected) noexcept
{
    return actual.status == expected.status
        && requestMatches (actual.drainedRequest, expected.drainedRequest)
        && commandMatches (actual.command, expected.command)
        && actual.pendingRequestConsumed == expected.pendingRequestConsumed
        && actual.graphRecompileExecuted == expected.graphRecompileExecuted;
}

bool blacklistCandidateMatches (yesdaw::plugin_host::PluginHostCoordinator::BlacklistCandidateStatus actual,
                                yesdaw::plugin_host::PluginHostCoordinator::BlacklistCandidateStatus expected) noexcept
{
    return actual.failureKind == expected.failureKind
        && actual.candidate == expected.candidate
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate;
}

bool blacklistEscalationMatches (yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalation actual,
                                 yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalation expected) noexcept
{
    return actual.failureKind == expected.failureKind
        && actual.crashCandidate == expected.crashCandidate
        && actual.watchdogTimeoutCandidate == expected.watchdogTimeoutCandidate
        && actual.controlThreadEscalationRequested == expected.controlThreadEscalationRequested;
}

bool blacklistEscalationResultMatches (
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationResult actual,
    yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationResult expected) noexcept
{
    return actual.status == expected.status
        && blacklistCandidateMatches (actual.drainedCandidate, expected.drainedCandidate)
        && blacklistEscalationMatches (actual.escalation, expected.escalation)
        && actual.pendingCandidateConsumed == expected.pendingCandidateConsumed
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

bool deferredBlacklistEscalationStatusMatches (
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistEscalationStatus actual,
    yesdaw::plugin_host::PluginHostCoordinator::DeferredBlacklistEscalationStatus expected) noexcept
{
    return actual.status == expected.status
        && blacklistEscalationResultMatches (actual.lastResult, expected.lastResult)
        && actual.escalationRecorded == expected.escalationRecorded
        && actual.blacklistPolicyApplied == expected.blacklistPolicyApplied
        && actual.blacklistStatePersisted == expected.blacklistStatePersisted;
}

} // namespace

int main (int argc, char** argv)
{
    if (argc != 2)
    {
        std::printf ("FAIL: expected path to YesDawPluginHost worker executable\n");
        return 2;
    }

    const juce::File workerExecutable { juce::String (argv[1]) };
    if (! workerExecutable.existsAsFile())
    {
        std::printf ("FAIL: worker executable does not exist: %s\n", argv[1]);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator coordinator;
    const auto initialStatus = coordinator.status();
    const auto initialPendingAction = coordinator.pendingFailureActionRequest();
    const auto initialDeferredCommandStatus = coordinator.deferredGraphChangeCommandStatus();
    const auto initialBlacklistCandidate = coordinator.blacklistCandidateStatus();
    const auto initialPendingBlacklistCandidate = coordinator.pendingBlacklistCandidateStatus();
    const auto initialBlacklistEscalationResult = coordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto initialDeferredBlacklistEscalationStatus = coordinator.deferredBlacklistEscalationStatus();
    const auto initialDeferredBlacklistEscalationAcknowledgement =
        coordinator.acknowledgeDeferredBlacklistEscalationStatus();

    if (initialStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::idle
        || initialStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::notStarted
        || initialStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::notStarted
        || initialStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::notStarted
        || initialStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::notStarted
        || initialStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || initialStatus.launchAttempted
        || initialStatus.readySeen
        || initialStatus.probeEchoed
        || initialStatus.stopRequested
        || initialStatus.watchdogTimedOut
        || initialStatus.watchdogKillRequested
        || initialStatus.crashObservationRequested
        || initialStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator initial status is not idle: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (initialStatus.state),
                     statusName (initialStatus.handshakeStatus),
                     statusName (initialStatus.stopStatus),
                     statusName (initialStatus.watchdogStatus),
                     statusName (initialStatus.crashStatus),
                     statusName (initialStatus.failureKind),
                     initialStatus.launchAttempted ? 1 : 0,
                     initialStatus.readySeen ? 1 : 0,
                     initialStatus.probeEchoed ? 1 : 0,
                     initialStatus.stopRequested ? 1 : 0,
                     initialStatus.watchdogTimedOut ? 1 : 0,
                     initialStatus.watchdogKillRequested ? 1 : 0,
                     initialStatus.crashObservationRequested ? 1 : 0,
                     initialStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    if (initialPendingAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || initialPendingAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || initialPendingAction.bypassRequested
        || initialPendingAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator initial pending action request is wrong: action=%s failure=%s bypass=%d recompile=%d\n",
                     statusName (initialPendingAction.action),
                     statusName (initialPendingAction.failureKind),
                     initialPendingAction.bypassRequested ? 1 : 0,
                     initialPendingAction.recompileRequested ? 1 : 0);
        return 2;
    }

    if (initialDeferredCommandStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || initialDeferredCommandStatus.lastResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || initialDeferredCommandStatus.commandRecorded
        || initialDeferredCommandStatus.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator initial deferred command status is wrong: status=%s last=%s recorded=%d executed=%d\n",
                     statusName (initialDeferredCommandStatus.status),
                     statusName (initialDeferredCommandStatus.lastResult.status),
                     initialDeferredCommandStatus.commandRecorded ? 1 : 0,
                     initialDeferredCommandStatus.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    if (! blacklistCandidateMatches (initialBlacklistCandidate, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial blacklist-candidate status is wrong: failure=%s candidate=%d crash=%d watchdog=%d\n",
                     statusName (initialBlacklistCandidate.failureKind),
                     initialBlacklistCandidate.candidate ? 1 : 0,
                     initialBlacklistCandidate.crashCandidate ? 1 : 0,
                     initialBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    if (! blacklistCandidateMatches (initialPendingBlacklistCandidate, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial pending blacklist-candidate status is wrong: failure=%s candidate=%d crash=%d watchdog=%d\n",
                     statusName (initialPendingBlacklistCandidate.failureKind),
                     initialPendingBlacklistCandidate.candidate ? 1 : 0,
                     initialPendingBlacklistCandidate.crashCandidate ? 1 : 0,
                     initialPendingBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    if (initialBlacklistEscalationResult.status != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::noAction
        || ! blacklistCandidateMatches (initialBlacklistEscalationResult.drainedCandidate, {})
        || ! blacklistEscalationMatches (initialBlacklistEscalationResult.escalation, {})
        || initialBlacklistEscalationResult.pendingCandidateConsumed
        || initialBlacklistEscalationResult.blacklistPolicyApplied
        || initialBlacklistEscalationResult.blacklistStatePersisted)
    {
        std::printf ("FAIL: plugin host coordinator initial blacklist escalation result is wrong: status=%s drained=%s/%d/%d/%d escalation=%s/%d/%d/%d consumed=%d policy=%d persisted=%d\n",
                     statusName (initialBlacklistEscalationResult.status),
                     statusName (initialBlacklistEscalationResult.drainedCandidate.failureKind),
                     initialBlacklistEscalationResult.drainedCandidate.candidate ? 1 : 0,
                     initialBlacklistEscalationResult.drainedCandidate.crashCandidate ? 1 : 0,
                     initialBlacklistEscalationResult.drainedCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (initialBlacklistEscalationResult.escalation.failureKind),
                     initialBlacklistEscalationResult.escalation.crashCandidate ? 1 : 0,
                     initialBlacklistEscalationResult.escalation.watchdogTimeoutCandidate ? 1 : 0,
                     initialBlacklistEscalationResult.escalation.controlThreadEscalationRequested ? 1 : 0,
                     initialBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     initialBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     initialBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! deferredBlacklistEscalationStatusMatches (initialDeferredBlacklistEscalationStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial deferred blacklist escalation status is wrong: status=%s last=%s recorded=%d policy=%d persisted=%d\n",
                     statusName (initialDeferredBlacklistEscalationStatus.status),
                     statusName (initialDeferredBlacklistEscalationStatus.lastResult.status),
                     initialDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     initialDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    if (! deferredBlacklistEscalationStatusMatches (initialDeferredBlacklistEscalationAcknowledgement, {}))
    {
        std::printf ("FAIL: plugin host coordinator initial deferred blacklist escalation acknowledge/clear status is wrong: status=%s last=%s recorded=%d policy=%d persisted=%d\n",
                     statusName (initialDeferredBlacklistEscalationAcknowledgement.status),
                     statusName (initialDeferredBlacklistEscalationAcknowledgement.lastResult.status),
                     initialDeferredBlacklistEscalationAcknowledgement.escalationRecorded ? 1 : 0,
                     initialDeferredBlacklistEscalationAcknowledgement.blacklistPolicyApplied ? 1 : 0,
                     initialDeferredBlacklistEscalationAcknowledgement.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator noneFailureCoordinator;
    const auto queuedNoneFailureAction = noneFailureCoordinator.queueFailureActionRequest (
        { yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::bypassAndRecompile,
          yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none,
          true,
          true });
    const auto noneFailureCommandResult = noneFailureCoordinator.drainPendingFailureActionRequestToControlCommand();
    if (queuedNoneFailureAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || queuedNoneFailureAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || queuedNoneFailureAction.bypassRequested
        || queuedNoneFailureAction.recompileRequested
        || noneFailureCommandResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || noneFailureCommandResult.drainedRequest.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || noneFailureCommandResult.command.command != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind::none
        || noneFailureCommandResult.pendingRequestConsumed
        || noneFailureCommandResult.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator HostFailureKind::none produced a control command: queued=%s/%s status=%s drained=%s/%s command=%s/%s consumed=%d executed=%d\n",
                     statusName (queuedNoneFailureAction.action),
                     statusName (queuedNoneFailureAction.failureKind),
                     statusName (noneFailureCommandResult.status),
                     statusName (noneFailureCommandResult.drainedRequest.action),
                     statusName (noneFailureCommandResult.drainedRequest.failureKind),
                     statusName (noneFailureCommandResult.command.command),
                     statusName (noneFailureCommandResult.command.failureKind),
                     noneFailureCommandResult.pendingRequestConsumed ? 1 : 0,
                     noneFailureCommandResult.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    const auto queuedNoneBlacklistCandidate = noneFailureCoordinator.queueBlacklistCandidate (
        { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none, true, true, false });
    const auto drainedNoneBlacklistCandidate = noneFailureCoordinator.drainPendingBlacklistCandidateStatus();
    const auto queuedInconsistentBlacklistCandidate = noneFailureCoordinator.queueBlacklistCandidate (
        { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash, true, true, true });
    const auto inconsistentBlacklistEscalationResult =
        noneFailureCoordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto inconsistentDeferredBlacklistEscalationStatus =
        noneFailureCoordinator.recordDeferredBlacklistEscalationResult (inconsistentBlacklistEscalationResult);
    if (! blacklistCandidateMatches (queuedNoneBlacklistCandidate, {})
        || ! blacklistCandidateMatches (drainedNoneBlacklistCandidate, {})
        || ! blacklistCandidateMatches (queuedInconsistentBlacklistCandidate, {})
        || inconsistentBlacklistEscalationResult.status != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::noAction
        || inconsistentBlacklistEscalationResult.pendingCandidateConsumed
        || inconsistentBlacklistEscalationResult.blacklistPolicyApplied
        || inconsistentBlacklistEscalationResult.blacklistStatePersisted
        || ! deferredBlacklistEscalationStatusMatches (inconsistentDeferredBlacklistEscalationStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator accepted an invalid pending blacklist candidate/escalation: none=%s/%d/%d/%d drained=%s/%d/%d/%d inconsistent=%s/%d/%d/%d escalation=%s consumed=%d policy=%d persisted=%d receipt=%s recorded=%d\n",
                     statusName (queuedNoneBlacklistCandidate.failureKind),
                     queuedNoneBlacklistCandidate.candidate ? 1 : 0,
                     queuedNoneBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedNoneBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (drainedNoneBlacklistCandidate.failureKind),
                     drainedNoneBlacklistCandidate.candidate ? 1 : 0,
                     drainedNoneBlacklistCandidate.crashCandidate ? 1 : 0,
                     drainedNoneBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (queuedInconsistentBlacklistCandidate.failureKind),
                     queuedInconsistentBlacklistCandidate.candidate ? 1 : 0,
                     queuedInconsistentBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedInconsistentBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (inconsistentBlacklistEscalationResult.status),
                     inconsistentBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     inconsistentBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     inconsistentBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0,
                     statusName (inconsistentDeferredBlacklistEscalationStatus.status),
                     inconsistentDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator deferredReceiptCoordinator;
    yesdaw::plugin_host::PluginHostCoordinator blacklistReceiptCoordinator;

    const auto handshake = coordinator.launchAndHandshake (workerExecutable);

    if (handshake.status != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::success)
    {
        std::printf ("FAIL: plugin host coordinator handshake failed: status=%s ready=%d echo=%d\n",
                     statusName (handshake.status),
                     handshake.readySeen ? 1 : 0,
                     handshake.probeEchoed ? 1 : 0);
        return 2;
    }

    const auto runningStatus = coordinator.status();

    if (runningStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::running
        || runningStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::success
        || runningStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::notStarted
        || runningStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::notStarted
        || runningStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::notStarted
        || runningStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || ! runningStatus.launchAttempted
        || ! runningStatus.readySeen
        || ! runningStatus.probeEchoed
        || runningStatus.stopRequested
        || runningStatus.watchdogTimedOut
        || runningStatus.watchdogKillRequested
        || runningStatus.crashObservationRequested
        || runningStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator running status is wrong: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (runningStatus.state),
                     statusName (runningStatus.handshakeStatus),
                     statusName (runningStatus.stopStatus),
                     statusName (runningStatus.watchdogStatus),
                     statusName (runningStatus.crashStatus),
                     statusName (runningStatus.failureKind),
                     runningStatus.launchAttempted ? 1 : 0,
                     runningStatus.readySeen ? 1 : 0,
                     runningStatus.probeEchoed ? 1 : 0,
                     runningStatus.stopRequested ? 1 : 0,
                     runningStatus.watchdogTimedOut ? 1 : 0,
                     runningStatus.watchdogKillRequested ? 1 : 0,
                     runningStatus.crashObservationRequested ? 1 : 0,
                     runningStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto stop = coordinator.requestStopAndWait();

    if (stop.status != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped
        || ! stop.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator stop/lost-child check failed: status=%s connectionLost=%d\n",
                     statusName (stop.status),
                     stop.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto stoppedStatus = coordinator.status();

    if (stoppedStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::stopped
        || stoppedStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::success
        || stoppedStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped
        || stoppedStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::notStarted
        || stoppedStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::notStarted
        || stoppedStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || ! stoppedStatus.launchAttempted
        || ! stoppedStatus.readySeen
        || ! stoppedStatus.probeEchoed
        || ! stoppedStatus.stopRequested
        || stoppedStatus.watchdogTimedOut
        || stoppedStatus.watchdogKillRequested
        || stoppedStatus.crashObservationRequested
        || ! stoppedStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator stopped status is wrong: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (stoppedStatus.state),
                     statusName (stoppedStatus.handshakeStatus),
                     statusName (stoppedStatus.stopStatus),
                     statusName (stoppedStatus.watchdogStatus),
                     statusName (stoppedStatus.crashStatus),
                     statusName (stoppedStatus.failureKind),
                     stoppedStatus.launchAttempted ? 1 : 0,
                     stoppedStatus.readySeen ? 1 : 0,
                     stoppedStatus.probeEchoed ? 1 : 0,
                     stoppedStatus.stopRequested ? 1 : 0,
                     stoppedStatus.watchdogTimedOut ? 1 : 0,
                     stoppedStatus.watchdogKillRequested ? 1 : 0,
                     stoppedStatus.crashObservationRequested ? 1 : 0,
                     stoppedStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto normalStopFailure = coordinator.hostFailureReport();
    if (normalStopFailure.kind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || ! normalStopFailure.connectionLostSeen
        || normalStopFailure.watchdogTimedOut
        || normalStopFailure.watchdogKillRequested)
    {
        std::printf ("FAIL: plugin host coordinator normal stop failure report is wrong: failure=%s connectionLost=%d watchdogTimeout=%d watchdogKill=%d\n",
                     statusName (normalStopFailure.kind),
                     normalStopFailure.connectionLostSeen ? 1 : 0,
                     normalStopFailure.watchdogTimedOut ? 1 : 0,
                     normalStopFailure.watchdogKillRequested ? 1 : 0);
        return 2;
    }

    const auto normalStopBlacklistCandidate = coordinator.blacklistCandidateStatus();
    if (! blacklistCandidateMatches (normalStopBlacklistCandidate, {}))
    {
        std::printf ("FAIL: plugin host coordinator normal stop should not be a blacklist candidate: failure=%s candidate=%d crash=%d watchdog=%d\n",
                     statusName (normalStopBlacklistCandidate.failureKind),
                     normalStopBlacklistCandidate.candidate ? 1 : 0,
                     normalStopBlacklistCandidate.crashCandidate ? 1 : 0,
                     normalStopBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    const auto normalStopQueuedBlacklistCandidate = coordinator.queueBlacklistCandidateForCurrentFailure();
    const auto normalStopPendingBlacklistCandidate = coordinator.pendingBlacklistCandidateStatus();
    const auto normalStopDrainedBlacklistCandidate = coordinator.drainPendingBlacklistCandidateStatus();
    const auto normalStopBlacklistEscalationResult =
        coordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto normalStopDeferredBlacklistEscalationStatus =
        coordinator.recordDeferredBlacklistEscalationResult (normalStopBlacklistEscalationResult);
    if (! blacklistCandidateMatches (normalStopQueuedBlacklistCandidate, {})
        || ! blacklistCandidateMatches (normalStopPendingBlacklistCandidate, {})
        || ! blacklistCandidateMatches (normalStopDrainedBlacklistCandidate, {})
        || normalStopBlacklistEscalationResult.status != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::noAction
        || normalStopBlacklistEscalationResult.pendingCandidateConsumed
        || normalStopBlacklistEscalationResult.blacklistPolicyApplied
        || normalStopBlacklistEscalationResult.blacklistStatePersisted
        || ! deferredBlacklistEscalationStatusMatches (normalStopDeferredBlacklistEscalationStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator normal stop pending blacklist candidate/escalation should remain empty: queued=%s/%d/%d/%d pending=%s/%d/%d/%d drained=%s/%d/%d/%d escalation=%s consumed=%d policy=%d persisted=%d receipt=%s recorded=%d\n",
                     statusName (normalStopQueuedBlacklistCandidate.failureKind),
                     normalStopQueuedBlacklistCandidate.candidate ? 1 : 0,
                     normalStopQueuedBlacklistCandidate.crashCandidate ? 1 : 0,
                     normalStopQueuedBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (normalStopPendingBlacklistCandidate.failureKind),
                     normalStopPendingBlacklistCandidate.candidate ? 1 : 0,
                     normalStopPendingBlacklistCandidate.crashCandidate ? 1 : 0,
                     normalStopPendingBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (normalStopDrainedBlacklistCandidate.failureKind),
                     normalStopDrainedBlacklistCandidate.candidate ? 1 : 0,
                     normalStopDrainedBlacklistCandidate.crashCandidate ? 1 : 0,
                     normalStopDrainedBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (normalStopBlacklistEscalationResult.status),
                     normalStopBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     normalStopBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     normalStopBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0,
                     statusName (normalStopDeferredBlacklistEscalationStatus.status),
                     normalStopDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0);
        return 2;
    }

    const auto normalStopAction = coordinator.failureActionRequest();
    if (normalStopAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || normalStopAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || normalStopAction.bypassRequested
        || normalStopAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator normal stop action request is wrong: action=%s failure=%s bypass=%d recompile=%d\n",
                     statusName (normalStopAction.action),
                     statusName (normalStopAction.failureKind),
                     normalStopAction.bypassRequested ? 1 : 0,
                     normalStopAction.recompileRequested ? 1 : 0);
        return 2;
    }

    const auto normalStopQueuedAction = coordinator.queueFailureActionRequestForCurrentFailure();
    const auto normalStopPendingAction = coordinator.pendingFailureActionRequest();
    const auto normalStopDrainedAction = coordinator.drainPendingFailureActionRequest();
    if (! requestMatches (normalStopQueuedAction, normalStopAction)
        || ! requestMatches (normalStopPendingAction, normalStopAction)
        || ! requestMatches (normalStopDrainedAction, normalStopAction))
    {
        std::printf ("FAIL: plugin host coordinator normal stop pending action should remain empty: queued=%s/%s pending=%s/%s drained=%s/%s\n",
                     statusName (normalStopQueuedAction.action),
                     statusName (normalStopQueuedAction.failureKind),
                     statusName (normalStopPendingAction.action),
                     statusName (normalStopPendingAction.failureKind),
                     statusName (normalStopDrainedAction.action),
                     statusName (normalStopDrainedAction.failureKind));
        return 2;
    }

    const auto normalStopCommandResult = coordinator.drainPendingFailureActionRequestToControlCommand();
    if (normalStopCommandResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || normalStopCommandResult.drainedRequest.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || normalStopCommandResult.command.command != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind::none
        || normalStopCommandResult.pendingRequestConsumed
        || normalStopCommandResult.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator normal stop control command should remain empty: status=%s drained=%s/%s command=%s/%s consumed=%d executed=%d\n",
                     statusName (normalStopCommandResult.status),
                     statusName (normalStopCommandResult.drainedRequest.action),
                     statusName (normalStopCommandResult.drainedRequest.failureKind),
                     statusName (normalStopCommandResult.command.command),
                     statusName (normalStopCommandResult.command.failureKind),
                     normalStopCommandResult.pendingRequestConsumed ? 1 : 0,
                     normalStopCommandResult.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    const auto normalStopDeferredStatus =
        deferredReceiptCoordinator.recordDeferredGraphChangeCommandResult (normalStopCommandResult);
    if (normalStopDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || normalStopDeferredStatus.lastResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || normalStopDeferredStatus.commandRecorded
        || normalStopDeferredStatus.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator normal stop should not record a deferred graph-change command: status=%s last=%s recorded=%d executed=%d\n",
                     statusName (normalStopDeferredStatus.status),
                     statusName (normalStopDeferredStatus.lastResult.status),
                     normalStopDeferredStatus.commandRecorded ? 1 : 0,
                     normalStopDeferredStatus.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator watchdogCoordinator;
    const auto watchdog = watchdogCoordinator.launchAndExpectWatchdogTimeout (workerExecutable);

    if (watchdog.status != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::timeoutKilled
        || ! watchdog.readySeen
        || ! watchdog.watchdogTimedOut
        || ! watchdog.killRequested
        || ! watchdog.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator watchdog timeout failed: status=%s ready=%d timedOut=%d kill=%d connectionLost=%d\n",
                     statusName (watchdog.status),
                     watchdog.readySeen ? 1 : 0,
                     watchdog.watchdogTimedOut ? 1 : 0,
                     watchdog.killRequested ? 1 : 0,
                     watchdog.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto watchdogStatus = watchdogCoordinator.status();

    if (watchdogStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::stopped
        || watchdogStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::echoTimeout
        || watchdogStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::notStarted
        || watchdogStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::timeoutKilled
        || watchdogStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::notStarted
        || watchdogStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! watchdogStatus.launchAttempted
        || ! watchdogStatus.readySeen
        || watchdogStatus.probeEchoed
        || watchdogStatus.stopRequested
        || ! watchdogStatus.watchdogTimedOut
        || ! watchdogStatus.watchdogKillRequested
        || watchdogStatus.crashObservationRequested
        || ! watchdogStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator watchdog status is wrong: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (watchdogStatus.state),
                     statusName (watchdogStatus.handshakeStatus),
                     statusName (watchdogStatus.stopStatus),
                     statusName (watchdogStatus.watchdogStatus),
                     statusName (watchdogStatus.crashStatus),
                     statusName (watchdogStatus.failureKind),
                     watchdogStatus.launchAttempted ? 1 : 0,
                     watchdogStatus.readySeen ? 1 : 0,
                     watchdogStatus.probeEchoed ? 1 : 0,
                     watchdogStatus.stopRequested ? 1 : 0,
                     watchdogStatus.watchdogTimedOut ? 1 : 0,
                     watchdogStatus.watchdogKillRequested ? 1 : 0,
                     watchdogStatus.crashObservationRequested ? 1 : 0,
                     watchdogStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto watchdogFailure = watchdogCoordinator.hostFailureReport();
    if (watchdogFailure.kind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! watchdogFailure.connectionLostSeen
        || ! watchdogFailure.watchdogTimedOut
        || ! watchdogFailure.watchdogKillRequested)
    {
        std::printf ("FAIL: plugin host coordinator watchdog failure report is wrong: failure=%s connectionLost=%d watchdogTimeout=%d watchdogKill=%d\n",
                     statusName (watchdogFailure.kind),
                     watchdogFailure.connectionLostSeen ? 1 : 0,
                     watchdogFailure.watchdogTimedOut ? 1 : 0,
                     watchdogFailure.watchdogKillRequested ? 1 : 0);
        return 2;
    }

    const auto watchdogBlacklistCandidate = watchdogCoordinator.blacklistCandidateStatus();
    if (! blacklistCandidateMatches (
            watchdogBlacklistCandidate,
            { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout, true, false, true }))
    {
        std::printf ("FAIL: plugin host coordinator watchdog blacklist-candidate status is wrong: failure=%s candidate=%d crash=%d watchdog=%d\n",
                     statusName (watchdogBlacklistCandidate.failureKind),
                     watchdogBlacklistCandidate.candidate ? 1 : 0,
                     watchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     watchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    const auto queuedWatchdogBlacklistCandidate = watchdogCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto pendingWatchdogBlacklistCandidate = watchdogCoordinator.pendingBlacklistCandidateStatus();
    const auto drainedWatchdogBlacklistCandidate = watchdogCoordinator.drainPendingBlacklistCandidateStatus();
    const auto afterWatchdogBlacklistCandidateDrain = watchdogCoordinator.pendingBlacklistCandidateStatus();
    const auto queuedWatchdogBlacklistEscalationCandidate =
        watchdogCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto watchdogBlacklistEscalationResult =
        watchdogCoordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto afterWatchdogBlacklistEscalationDrain = watchdogCoordinator.pendingBlacklistCandidateStatus();
    if (! blacklistCandidateMatches (queuedWatchdogBlacklistCandidate, watchdogBlacklistCandidate)
        || ! blacklistCandidateMatches (pendingWatchdogBlacklistCandidate, watchdogBlacklistCandidate)
        || ! blacklistCandidateMatches (drainedWatchdogBlacklistCandidate, watchdogBlacklistCandidate)
        || ! blacklistCandidateMatches (afterWatchdogBlacklistCandidateDrain, {})
        || ! blacklistCandidateMatches (queuedWatchdogBlacklistEscalationCandidate, watchdogBlacklistCandidate)
        || ! blacklistEscalationResultMatches (
            watchdogBlacklistEscalationResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady,
              watchdogBlacklistCandidate,
              { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout, false, true, true },
              true,
              false,
              false })
        || ! blacklistCandidateMatches (afterWatchdogBlacklistEscalationDrain, {}))
    {
        std::printf ("FAIL: plugin host coordinator watchdog pending blacklist candidate escalation is wrong: queued=%s/%d/%d/%d pending=%s/%d/%d/%d drained=%s/%d/%d/%d after=%s/%d/%d/%d escalationQueued=%s/%d/%d/%d escalation=%s drained=%s/%d/%d/%d escalated=%s/%d/%d/%d consumed=%d policy=%d persisted=%d afterEscalation=%s/%d/%d/%d\n",
                     statusName (queuedWatchdogBlacklistCandidate.failureKind),
                     queuedWatchdogBlacklistCandidate.candidate ? 1 : 0,
                     queuedWatchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedWatchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (pendingWatchdogBlacklistCandidate.failureKind),
                     pendingWatchdogBlacklistCandidate.candidate ? 1 : 0,
                     pendingWatchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     pendingWatchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (drainedWatchdogBlacklistCandidate.failureKind),
                     drainedWatchdogBlacklistCandidate.candidate ? 1 : 0,
                     drainedWatchdogBlacklistCandidate.crashCandidate ? 1 : 0,
                     drainedWatchdogBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (afterWatchdogBlacklistCandidateDrain.failureKind),
                     afterWatchdogBlacklistCandidateDrain.candidate ? 1 : 0,
                     afterWatchdogBlacklistCandidateDrain.crashCandidate ? 1 : 0,
                     afterWatchdogBlacklistCandidateDrain.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (queuedWatchdogBlacklistEscalationCandidate.failureKind),
                     queuedWatchdogBlacklistEscalationCandidate.candidate ? 1 : 0,
                     queuedWatchdogBlacklistEscalationCandidate.crashCandidate ? 1 : 0,
                     queuedWatchdogBlacklistEscalationCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (watchdogBlacklistEscalationResult.status),
                     statusName (watchdogBlacklistEscalationResult.drainedCandidate.failureKind),
                     watchdogBlacklistEscalationResult.drainedCandidate.candidate ? 1 : 0,
                     watchdogBlacklistEscalationResult.drainedCandidate.crashCandidate ? 1 : 0,
                     watchdogBlacklistEscalationResult.drainedCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (watchdogBlacklistEscalationResult.escalation.failureKind),
                     watchdogBlacklistEscalationResult.escalation.crashCandidate ? 1 : 0,
                     watchdogBlacklistEscalationResult.escalation.watchdogTimeoutCandidate ? 1 : 0,
                     watchdogBlacklistEscalationResult.escalation.controlThreadEscalationRequested ? 1 : 0,
                     watchdogBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     watchdogBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     watchdogBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0,
                     statusName (afterWatchdogBlacklistEscalationDrain.failureKind),
                     afterWatchdogBlacklistEscalationDrain.candidate ? 1 : 0,
                     afterWatchdogBlacklistEscalationDrain.crashCandidate ? 1 : 0,
                     afterWatchdogBlacklistEscalationDrain.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    const auto watchdogDeferredBlacklistEscalationStatus =
        blacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (watchdogBlacklistEscalationResult);
    const auto watchdogDeferredBlacklistEscalationInspection =
        blacklistReceiptCoordinator.deferredBlacklistEscalationStatus();
    if (watchdogDeferredBlacklistEscalationStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady
        || ! blacklistEscalationResultMatches (watchdogDeferredBlacklistEscalationStatus.lastResult,
                                               watchdogBlacklistEscalationResult)
        || ! watchdogDeferredBlacklistEscalationStatus.escalationRecorded
        || watchdogDeferredBlacklistEscalationStatus.blacklistPolicyApplied
        || watchdogDeferredBlacklistEscalationStatus.blacklistStatePersisted
        || ! blacklistEscalationResultMatches (watchdogDeferredBlacklistEscalationInspection.lastResult,
                                               watchdogBlacklistEscalationResult)
        || ! watchdogDeferredBlacklistEscalationInspection.escalationRecorded
        || watchdogDeferredBlacklistEscalationInspection.blacklistPolicyApplied
        || watchdogDeferredBlacklistEscalationInspection.blacklistStatePersisted)
    {
        std::printf ("FAIL: plugin host coordinator watchdog deferred blacklist escalation receipt/status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d\n",
                     statusName (watchdogDeferredBlacklistEscalationStatus.status),
                     statusName (watchdogDeferredBlacklistEscalationInspection.status),
                     watchdogDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     watchdogDeferredBlacklistEscalationInspection.escalationRecorded ? 1 : 0,
                     watchdogDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     watchdogDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0,
                     watchdogDeferredBlacklistEscalationInspection.blacklistPolicyApplied ? 1 : 0,
                     watchdogDeferredBlacklistEscalationInspection.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator invalidBlacklistReceiptCoordinator;
    auto unconsumedBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    unconsumedBlacklistEscalationResult.pendingCandidateConsumed = false;
    auto policyAppliedBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    policyAppliedBlacklistEscalationResult.blacklistPolicyApplied = true;
    auto persistedBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    persistedBlacklistEscalationResult.blacklistStatePersisted = true;
    auto missingControlRequestBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    missingControlRequestBlacklistEscalationResult.escalation.controlThreadEscalationRequested = false;
    auto mismatchedBlacklistEscalationResult = watchdogBlacklistEscalationResult;
    mismatchedBlacklistEscalationResult.escalation.failureKind =
        yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash;
    const auto unconsumedDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (unconsumedBlacklistEscalationResult);
    const auto policyAppliedDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (policyAppliedBlacklistEscalationResult);
    const auto persistedDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (persistedBlacklistEscalationResult);
    const auto missingControlRequestDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (missingControlRequestBlacklistEscalationResult);
    const auto mismatchedDeferredBlacklistEscalationStatus =
        invalidBlacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (mismatchedBlacklistEscalationResult);
    if (! deferredBlacklistEscalationStatusMatches (unconsumedDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (policyAppliedDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (persistedDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (missingControlRequestDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (mismatchedDeferredBlacklistEscalationStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator accepted invalid deferred blacklist escalation result: unconsumed=%s policy=%s persisted=%s missingControl=%s mismatched=%s\n",
                     statusName (unconsumedDeferredBlacklistEscalationStatus.status),
                     statusName (policyAppliedDeferredBlacklistEscalationStatus.status),
                     statusName (persistedDeferredBlacklistEscalationStatus.status),
                     statusName (missingControlRequestDeferredBlacklistEscalationStatus.status),
                     statusName (mismatchedDeferredBlacklistEscalationStatus.status));
        return 2;
    }

    const auto watchdogAction = watchdogCoordinator.failureActionRequest();
    if (watchdogAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::bypassAndRecompile
        || watchdogAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout
        || ! watchdogAction.bypassRequested
        || ! watchdogAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator watchdog action request is wrong: action=%s failure=%s bypass=%d recompile=%d\n",
                     statusName (watchdogAction.action),
                     statusName (watchdogAction.failureKind),
                     watchdogAction.bypassRequested ? 1 : 0,
                     watchdogAction.recompileRequested ? 1 : 0);
        return 2;
    }

    const auto queuedWatchdogAction = watchdogCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto pendingWatchdogAction = watchdogCoordinator.pendingFailureActionRequest();
    const auto drainedWatchdogAction = watchdogCoordinator.drainPendingFailureActionRequest();
    const auto afterWatchdogDrainAction = watchdogCoordinator.pendingFailureActionRequest();
    if (! requestMatches (queuedWatchdogAction, watchdogAction)
        || ! requestMatches (pendingWatchdogAction, watchdogAction)
        || ! requestMatches (drainedWatchdogAction, watchdogAction)
        || afterWatchdogDrainAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || afterWatchdogDrainAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || afterWatchdogDrainAction.bypassRequested
        || afterWatchdogDrainAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator watchdog pending action queue/drain is wrong: queued=%s/%s pending=%s/%s drained=%s/%s afterDrain=%s/%s\n",
                     statusName (queuedWatchdogAction.action),
                     statusName (queuedWatchdogAction.failureKind),
                     statusName (pendingWatchdogAction.action),
                     statusName (pendingWatchdogAction.failureKind),
                     statusName (drainedWatchdogAction.action),
                     statusName (drainedWatchdogAction.failureKind),
                     statusName (afterWatchdogDrainAction.action),
                     statusName (afterWatchdogDrainAction.failureKind));
        return 2;
    }

    const auto queuedWatchdogCommandAction = watchdogCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto watchdogCommandResult = watchdogCoordinator.drainPendingFailureActionRequestToControlCommand();
    const auto afterWatchdogCommandDrainAction = watchdogCoordinator.pendingFailureActionRequest();
    if (! requestMatches (queuedWatchdogCommandAction, watchdogAction)
        || watchdogCommandResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! requestMatches (watchdogCommandResult.drainedRequest, watchdogAction)
        || ! commandMatches (watchdogCommandResult.command,
                             { yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind::bypassAndRecompile,
                               yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::watchdogTimeout,
                               true,
                               true })
        || ! watchdogCommandResult.pendingRequestConsumed
        || watchdogCommandResult.graphRecompileExecuted
        || afterWatchdogCommandDrainAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || afterWatchdogCommandDrainAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || afterWatchdogCommandDrainAction.bypassRequested
        || afterWatchdogCommandDrainAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator watchdog control command shell is wrong: queued=%s/%s status=%s drained=%s/%s command=%s/%s consumed=%d executed=%d afterDrain=%s/%s\n",
                     statusName (queuedWatchdogCommandAction.action),
                     statusName (queuedWatchdogCommandAction.failureKind),
                     statusName (watchdogCommandResult.status),
                     statusName (watchdogCommandResult.drainedRequest.action),
                     statusName (watchdogCommandResult.drainedRequest.failureKind),
                     statusName (watchdogCommandResult.command.command),
                     statusName (watchdogCommandResult.command.failureKind),
                     watchdogCommandResult.pendingRequestConsumed ? 1 : 0,
                     watchdogCommandResult.graphRecompileExecuted ? 1 : 0,
                     statusName (afterWatchdogCommandDrainAction.action),
                     statusName (afterWatchdogCommandDrainAction.failureKind));
        return 2;
    }

    const auto watchdogDeferredStatus =
        deferredReceiptCoordinator.recordDeferredGraphChangeCommandResult (watchdogCommandResult);
    const auto watchdogDeferredStatusInspection = deferredReceiptCoordinator.deferredGraphChangeCommandStatus();
    if (watchdogDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! commandResultMatches (watchdogDeferredStatus.lastResult, watchdogCommandResult)
        || ! watchdogDeferredStatus.commandRecorded
        || watchdogDeferredStatus.graphRecompileExecuted
        || ! commandResultMatches (watchdogDeferredStatusInspection.lastResult, watchdogCommandResult)
        || ! watchdogDeferredStatusInspection.commandRecorded
        || watchdogDeferredStatusInspection.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator watchdog deferred command receipt/status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d executed=%d inspectedExecuted=%d\n",
                     statusName (watchdogDeferredStatus.status),
                     statusName (watchdogDeferredStatusInspection.status),
                     watchdogDeferredStatus.commandRecorded ? 1 : 0,
                     watchdogDeferredStatusInspection.commandRecorded ? 1 : 0,
                     watchdogDeferredStatus.graphRecompileExecuted ? 1 : 0,
                     watchdogDeferredStatusInspection.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    yesdaw::plugin_host::PluginHostCoordinator crashCoordinator;
    const auto crash = crashCoordinator.launchAndExpectCrash (workerExecutable);

    if (crash.status != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::connectionLost
        || ! crash.readySeen
        || ! crash.crashObservationRequested
        || ! crash.connectionLostSeen
        || crash.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash)
    {
        std::printf ("FAIL: plugin host coordinator crash observation failed: status=%s ready=%d observation=%d connectionLost=%d failure=%s\n",
                     statusName (crash.status),
                     crash.readySeen ? 1 : 0,
                     crash.crashObservationRequested ? 1 : 0,
                     crash.connectionLostSeen ? 1 : 0,
                     statusName (crash.failureKind));
        return 2;
    }

    const auto crashStatus = crashCoordinator.status();

    if (crashStatus.state != yesdaw::plugin_host::PluginHostCoordinator::ChildState::lost
        || crashStatus.handshakeStatus != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::notStarted
        || crashStatus.stopStatus != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::notStarted
        || crashStatus.watchdogStatus != yesdaw::plugin_host::PluginHostCoordinator::WatchdogStatus::notStarted
        || crashStatus.crashStatus != yesdaw::plugin_host::PluginHostCoordinator::CrashStatus::connectionLost
        || crashStatus.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash
        || ! crashStatus.launchAttempted
        || ! crashStatus.readySeen
        || crashStatus.probeEchoed
        || crashStatus.stopRequested
        || crashStatus.watchdogTimedOut
        || crashStatus.watchdogKillRequested
        || ! crashStatus.crashObservationRequested
        || ! crashStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator crash status is wrong: state=%s handshake=%s stop=%s watchdog=%s crash=%s failure=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d crashObservation=%d connectionLost=%d\n",
                     statusName (crashStatus.state),
                     statusName (crashStatus.handshakeStatus),
                     statusName (crashStatus.stopStatus),
                     statusName (crashStatus.watchdogStatus),
                     statusName (crashStatus.crashStatus),
                     statusName (crashStatus.failureKind),
                     crashStatus.launchAttempted ? 1 : 0,
                     crashStatus.readySeen ? 1 : 0,
                     crashStatus.probeEchoed ? 1 : 0,
                     crashStatus.stopRequested ? 1 : 0,
                     crashStatus.watchdogTimedOut ? 1 : 0,
                     crashStatus.watchdogKillRequested ? 1 : 0,
                     crashStatus.crashObservationRequested ? 1 : 0,
                     crashStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    const auto crashFailure = crashCoordinator.hostFailureReport();
    if (crashFailure.kind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash
        || ! crashFailure.connectionLostSeen
        || crashFailure.watchdogTimedOut
        || crashFailure.watchdogKillRequested)
    {
        std::printf ("FAIL: plugin host coordinator crash failure report is wrong: failure=%s connectionLost=%d watchdogTimeout=%d watchdogKill=%d\n",
                     statusName (crashFailure.kind),
                     crashFailure.connectionLostSeen ? 1 : 0,
                     crashFailure.watchdogTimedOut ? 1 : 0,
                     crashFailure.watchdogKillRequested ? 1 : 0);
        return 2;
    }

    const auto crashBlacklistCandidate = crashCoordinator.blacklistCandidateStatus();
    if (! blacklistCandidateMatches (
            crashBlacklistCandidate,
            { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash, true, true, false })
        || crashBlacklistCandidate.failureKind == watchdogBlacklistCandidate.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash blacklist-candidate status is wrong: failure=%s candidate=%d crash=%d watchdog=%d watchdogFailure=%s\n",
                     statusName (crashBlacklistCandidate.failureKind),
                     crashBlacklistCandidate.candidate ? 1 : 0,
                     crashBlacklistCandidate.crashCandidate ? 1 : 0,
                     crashBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (watchdogBlacklistCandidate.failureKind));
        return 2;
    }

    const auto queuedCrashBlacklistCandidate = crashCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto pendingCrashBlacklistCandidate = crashCoordinator.pendingBlacklistCandidateStatus();
    const auto drainedCrashBlacklistCandidate = crashCoordinator.drainPendingBlacklistCandidateStatus();
    const auto afterCrashBlacklistCandidateDrain = crashCoordinator.pendingBlacklistCandidateStatus();
    const auto queuedCrashBlacklistEscalationCandidate =
        crashCoordinator.queueBlacklistCandidateForCurrentFailure();
    const auto crashBlacklistEscalationResult =
        crashCoordinator.drainPendingBlacklistCandidateToControlEscalation();
    const auto afterCrashBlacklistEscalationDrain = crashCoordinator.pendingBlacklistCandidateStatus();
    if (! blacklistCandidateMatches (queuedCrashBlacklistCandidate, crashBlacklistCandidate)
        || ! blacklistCandidateMatches (pendingCrashBlacklistCandidate, crashBlacklistCandidate)
        || ! blacklistCandidateMatches (drainedCrashBlacklistCandidate, crashBlacklistCandidate)
        || ! blacklistCandidateMatches (afterCrashBlacklistCandidateDrain, {})
        || drainedCrashBlacklistCandidate.failureKind == drainedWatchdogBlacklistCandidate.failureKind
        || ! blacklistCandidateMatches (queuedCrashBlacklistEscalationCandidate, crashBlacklistCandidate)
        || ! blacklistEscalationResultMatches (
            crashBlacklistEscalationResult,
            { yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady,
              crashBlacklistCandidate,
              { yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash, true, false, true },
              true,
              false,
              false })
        || crashBlacklistEscalationResult.escalation.failureKind
            == watchdogBlacklistEscalationResult.escalation.failureKind
        || ! blacklistCandidateMatches (afterCrashBlacklistEscalationDrain, {}))
    {
        std::printf ("FAIL: plugin host coordinator crash pending blacklist candidate escalation is wrong: queued=%s/%d/%d/%d pending=%s/%d/%d/%d drained=%s/%d/%d/%d after=%s/%d/%d/%d watchdog=%s escalationQueued=%s/%d/%d/%d escalation=%s drained=%s/%d/%d/%d escalated=%s/%d/%d/%d watchdogEscalation=%s consumed=%d policy=%d persisted=%d afterEscalation=%s/%d/%d/%d\n",
                     statusName (queuedCrashBlacklistCandidate.failureKind),
                     queuedCrashBlacklistCandidate.candidate ? 1 : 0,
                     queuedCrashBlacklistCandidate.crashCandidate ? 1 : 0,
                     queuedCrashBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (pendingCrashBlacklistCandidate.failureKind),
                     pendingCrashBlacklistCandidate.candidate ? 1 : 0,
                     pendingCrashBlacklistCandidate.crashCandidate ? 1 : 0,
                     pendingCrashBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (drainedCrashBlacklistCandidate.failureKind),
                     drainedCrashBlacklistCandidate.candidate ? 1 : 0,
                     drainedCrashBlacklistCandidate.crashCandidate ? 1 : 0,
                     drainedCrashBlacklistCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (afterCrashBlacklistCandidateDrain.failureKind),
                     afterCrashBlacklistCandidateDrain.candidate ? 1 : 0,
                     afterCrashBlacklistCandidateDrain.crashCandidate ? 1 : 0,
                     afterCrashBlacklistCandidateDrain.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (drainedWatchdogBlacklistCandidate.failureKind),
                     statusName (queuedCrashBlacklistEscalationCandidate.failureKind),
                     queuedCrashBlacklistEscalationCandidate.candidate ? 1 : 0,
                     queuedCrashBlacklistEscalationCandidate.crashCandidate ? 1 : 0,
                     queuedCrashBlacklistEscalationCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (crashBlacklistEscalationResult.status),
                     statusName (crashBlacklistEscalationResult.drainedCandidate.failureKind),
                     crashBlacklistEscalationResult.drainedCandidate.candidate ? 1 : 0,
                     crashBlacklistEscalationResult.drainedCandidate.crashCandidate ? 1 : 0,
                     crashBlacklistEscalationResult.drainedCandidate.watchdogTimeoutCandidate ? 1 : 0,
                     statusName (crashBlacklistEscalationResult.escalation.failureKind),
                     crashBlacklistEscalationResult.escalation.crashCandidate ? 1 : 0,
                     crashBlacklistEscalationResult.escalation.watchdogTimeoutCandidate ? 1 : 0,
                     crashBlacklistEscalationResult.escalation.controlThreadEscalationRequested ? 1 : 0,
                     statusName (watchdogBlacklistEscalationResult.escalation.failureKind),
                     crashBlacklistEscalationResult.pendingCandidateConsumed ? 1 : 0,
                     crashBlacklistEscalationResult.blacklistPolicyApplied ? 1 : 0,
                     crashBlacklistEscalationResult.blacklistStatePersisted ? 1 : 0,
                     statusName (afterCrashBlacklistEscalationDrain.failureKind),
                     afterCrashBlacklistEscalationDrain.candidate ? 1 : 0,
                     afterCrashBlacklistEscalationDrain.crashCandidate ? 1 : 0,
                     afterCrashBlacklistEscalationDrain.watchdogTimeoutCandidate ? 1 : 0);
        return 2;
    }

    const auto crashDeferredBlacklistEscalationStatus =
        blacklistReceiptCoordinator.recordDeferredBlacklistEscalationResult (crashBlacklistEscalationResult);
    const auto crashDeferredBlacklistEscalationInspection =
        blacklistReceiptCoordinator.deferredBlacklistEscalationStatus();
    if (crashDeferredBlacklistEscalationStatus.status
            != yesdaw::plugin_host::PluginHostCoordinator::BlacklistEscalationStatus::escalationReady
        || ! blacklistEscalationResultMatches (crashDeferredBlacklistEscalationStatus.lastResult,
                                               crashBlacklistEscalationResult)
        || ! crashDeferredBlacklistEscalationStatus.escalationRecorded
        || crashDeferredBlacklistEscalationStatus.blacklistPolicyApplied
        || crashDeferredBlacklistEscalationStatus.blacklistStatePersisted
        || ! blacklistEscalationResultMatches (crashDeferredBlacklistEscalationInspection.lastResult,
                                               crashBlacklistEscalationResult)
        || ! crashDeferredBlacklistEscalationInspection.escalationRecorded
        || crashDeferredBlacklistEscalationInspection.blacklistPolicyApplied
        || crashDeferredBlacklistEscalationInspection.blacklistStatePersisted
        || crashDeferredBlacklistEscalationInspection.lastResult.escalation.failureKind
            == watchdogDeferredBlacklistEscalationInspection.lastResult.escalation.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash deferred blacklist escalation receipt/status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d watchdogEscalation=%s crashEscalation=%s\n",
                     statusName (crashDeferredBlacklistEscalationStatus.status),
                     statusName (crashDeferredBlacklistEscalationInspection.status),
                     crashDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     crashDeferredBlacklistEscalationInspection.escalationRecorded ? 1 : 0,
                     crashDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     crashDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0,
                     crashDeferredBlacklistEscalationInspection.blacklistPolicyApplied ? 1 : 0,
                     crashDeferredBlacklistEscalationInspection.blacklistStatePersisted ? 1 : 0,
                     statusName (watchdogDeferredBlacklistEscalationInspection.lastResult.escalation.failureKind),
                     statusName (crashDeferredBlacklistEscalationInspection.lastResult.escalation.failureKind));
        return 2;
    }

    const auto acknowledgedDeferredBlacklistEscalationStatus =
        blacklistReceiptCoordinator.acknowledgeDeferredBlacklistEscalationStatus();
    const auto afterAcknowledgedDeferredBlacklistEscalationStatus =
        blacklistReceiptCoordinator.deferredBlacklistEscalationStatus();
    if (! deferredBlacklistEscalationStatusMatches (acknowledgedDeferredBlacklistEscalationStatus, {})
        || ! deferredBlacklistEscalationStatusMatches (afterAcknowledgedDeferredBlacklistEscalationStatus, {}))
    {
        std::printf ("FAIL: plugin host coordinator deferred blacklist escalation acknowledge/clear status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d policy=%d persisted=%d inspectedPolicy=%d inspectedPersisted=%d\n",
                     statusName (acknowledgedDeferredBlacklistEscalationStatus.status),
                     statusName (afterAcknowledgedDeferredBlacklistEscalationStatus.status),
                     acknowledgedDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     afterAcknowledgedDeferredBlacklistEscalationStatus.escalationRecorded ? 1 : 0,
                     acknowledgedDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     acknowledgedDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0,
                     afterAcknowledgedDeferredBlacklistEscalationStatus.blacklistPolicyApplied ? 1 : 0,
                     afterAcknowledgedDeferredBlacklistEscalationStatus.blacklistStatePersisted ? 1 : 0);
        return 2;
    }

    const auto crashAction = crashCoordinator.failureActionRequest();
    if (crashAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::bypassAndRecompile
        || crashAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash
        || ! crashAction.bypassRequested
        || ! crashAction.recompileRequested
        || crashAction.failureKind == watchdogAction.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash action request is wrong: action=%s failure=%s bypass=%d recompile=%d watchdogFailure=%s\n",
                     statusName (crashAction.action),
                     statusName (crashAction.failureKind),
                     crashAction.bypassRequested ? 1 : 0,
                     crashAction.recompileRequested ? 1 : 0,
                     statusName (watchdogAction.failureKind));
        return 2;
    }

    const auto queuedCrashAction = crashCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto pendingCrashAction = crashCoordinator.pendingFailureActionRequest();
    const auto drainedCrashAction = crashCoordinator.drainPendingFailureActionRequest();
    const auto afterCrashDrainAction = crashCoordinator.pendingFailureActionRequest();
    if (! requestMatches (queuedCrashAction, crashAction)
        || ! requestMatches (pendingCrashAction, crashAction)
        || ! requestMatches (drainedCrashAction, crashAction)
        || afterCrashDrainAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || afterCrashDrainAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || afterCrashDrainAction.bypassRequested
        || afterCrashDrainAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator crash pending action queue/drain is wrong: queued=%s/%s pending=%s/%s drained=%s/%s afterDrain=%s/%s\n",
                     statusName (queuedCrashAction.action),
                     statusName (queuedCrashAction.failureKind),
                     statusName (pendingCrashAction.action),
                     statusName (pendingCrashAction.failureKind),
                     statusName (drainedCrashAction.action),
                     statusName (drainedCrashAction.failureKind),
                     statusName (afterCrashDrainAction.action),
                     statusName (afterCrashDrainAction.failureKind));
        return 2;
    }

    const auto queuedCrashCommandAction = crashCoordinator.queueFailureActionRequestForCurrentFailure();
    const auto crashCommandResult = crashCoordinator.drainPendingFailureActionRequestToControlCommand();
    const auto afterCrashCommandDrainAction = crashCoordinator.pendingFailureActionRequest();
    if (! requestMatches (queuedCrashCommandAction, crashAction)
        || crashCommandResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! requestMatches (crashCommandResult.drainedRequest, crashAction)
        || ! commandMatches (crashCommandResult.command,
                             { yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandKind::bypassAndRecompile,
                               yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::crash,
                               true,
                               true })
        || ! crashCommandResult.pendingRequestConsumed
        || crashCommandResult.graphRecompileExecuted
        || crashCommandResult.command.failureKind == watchdogCommandResult.command.failureKind
        || afterCrashCommandDrainAction.action != yesdaw::plugin_host::PluginHostCoordinator::FailureActionKind::none
        || afterCrashCommandDrainAction.failureKind != yesdaw::plugin_host::PluginHostCoordinator::HostFailureKind::none
        || afterCrashCommandDrainAction.bypassRequested
        || afterCrashCommandDrainAction.recompileRequested)
    {
        std::printf ("FAIL: plugin host coordinator crash control command shell is wrong: queued=%s/%s status=%s drained=%s/%s command=%s/%s watchdogCommand=%s consumed=%d executed=%d afterDrain=%s/%s\n",
                     statusName (queuedCrashCommandAction.action),
                     statusName (queuedCrashCommandAction.failureKind),
                     statusName (crashCommandResult.status),
                     statusName (crashCommandResult.drainedRequest.action),
                     statusName (crashCommandResult.drainedRequest.failureKind),
                     statusName (crashCommandResult.command.command),
                     statusName (crashCommandResult.command.failureKind),
                     statusName (watchdogCommandResult.command.failureKind),
                     crashCommandResult.pendingRequestConsumed ? 1 : 0,
                     crashCommandResult.graphRecompileExecuted ? 1 : 0,
                     statusName (afterCrashCommandDrainAction.action),
                     statusName (afterCrashCommandDrainAction.failureKind));
        return 2;
    }

    const auto crashDeferredStatus =
        deferredReceiptCoordinator.recordDeferredGraphChangeCommandResult (crashCommandResult);
    const auto crashDeferredStatusInspection = deferredReceiptCoordinator.deferredGraphChangeCommandStatus();
    if (crashDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::commandReady
        || ! commandResultMatches (crashDeferredStatus.lastResult, crashCommandResult)
        || ! crashDeferredStatus.commandRecorded
        || crashDeferredStatus.graphRecompileExecuted
        || ! commandResultMatches (crashDeferredStatusInspection.lastResult, crashCommandResult)
        || ! crashDeferredStatusInspection.commandRecorded
        || crashDeferredStatusInspection.graphRecompileExecuted
        || crashDeferredStatusInspection.lastResult.command.failureKind == watchdogDeferredStatusInspection.lastResult.command.failureKind)
    {
        std::printf ("FAIL: plugin host coordinator crash deferred command receipt/status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d executed=%d inspectedExecuted=%d watchdogCommand=%s crashCommand=%s\n",
                     statusName (crashDeferredStatus.status),
                     statusName (crashDeferredStatusInspection.status),
                     crashDeferredStatus.commandRecorded ? 1 : 0,
                     crashDeferredStatusInspection.commandRecorded ? 1 : 0,
                     crashDeferredStatus.graphRecompileExecuted ? 1 : 0,
                     crashDeferredStatusInspection.graphRecompileExecuted ? 1 : 0,
                     statusName (watchdogDeferredStatusInspection.lastResult.command.failureKind),
                     statusName (crashDeferredStatusInspection.lastResult.command.failureKind));
        return 2;
    }

    const auto acknowledgedDeferredStatus =
        deferredReceiptCoordinator.acknowledgeDeferredGraphChangeCommandStatus();
    const auto afterAcknowledgedDeferredStatus =
        deferredReceiptCoordinator.deferredGraphChangeCommandStatus();
    if (acknowledgedDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || acknowledgedDeferredStatus.lastResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || acknowledgedDeferredStatus.commandRecorded
        || acknowledgedDeferredStatus.graphRecompileExecuted
        || afterAcknowledgedDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || afterAcknowledgedDeferredStatus.lastResult.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || afterAcknowledgedDeferredStatus.commandRecorded
        || afterAcknowledgedDeferredStatus.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator deferred command acknowledge/clear status is wrong: status=%s inspected=%s recorded=%d inspectedRecorded=%d executed=%d inspectedExecuted=%d\n",
                     statusName (acknowledgedDeferredStatus.status),
                     statusName (afterAcknowledgedDeferredStatus.status),
                     acknowledgedDeferredStatus.commandRecorded ? 1 : 0,
                     afterAcknowledgedDeferredStatus.commandRecorded ? 1 : 0,
                     acknowledgedDeferredStatus.graphRecompileExecuted ? 1 : 0,
                     afterAcknowledgedDeferredStatus.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    auto executedGraphRecompileResult = crashCommandResult;
    executedGraphRecompileResult.graphRecompileExecuted = true;
    const auto executedGraphRecompileDeferredStatus =
        deferredReceiptCoordinator.recordDeferredGraphChangeCommandResult (executedGraphRecompileResult);
    if (executedGraphRecompileDeferredStatus.status != yesdaw::plugin_host::PluginHostCoordinator::GraphChangeCommandStatus::noAction
        || executedGraphRecompileDeferredStatus.commandRecorded
        || executedGraphRecompileDeferredStatus.graphRecompileExecuted)
    {
        std::printf ("FAIL: plugin host coordinator accepted a deferred command result that claimed graph recompile execution: status=%s recorded=%d executed=%d\n",
                     statusName (executedGraphRecompileDeferredStatus.status),
                     executedGraphRecompileDeferredStatus.commandRecorded ? 1 : 0,
                     executedGraphRecompileDeferredStatus.graphRecompileExecuted ? 1 : 0);
        return 2;
    }

    std::printf ("PASS: plugin host coordinator launched worker, reported ready/handshake status, stopped worker, refused HostFailureKind::none commands, classified watchdog-timeout vs crash host failures, exposed and queued/drained future blacklist-candidate status, drained future blacklist escalation shells, recorded and acknowledged/cleared deferred blacklist escalation receipt/status without policy or persistence, requested future bypass/recompile actions, queued/drained pending failure actions, drained future control-thread graph-change command shells, recorded deferred command receipt/status, and acknowledged/cleared it without executing graph recompiles\n");
    return 0;
}
