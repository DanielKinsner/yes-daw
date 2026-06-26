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

    std::printf ("PASS: plugin host coordinator launched worker, reported ready/handshake status, stopped worker, and classified watchdog-timeout vs crash host failures\n");
    return 0;
}
