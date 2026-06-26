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
        || initialStatus.launchAttempted
        || initialStatus.readySeen
        || initialStatus.probeEchoed
        || initialStatus.stopRequested
        || initialStatus.watchdogTimedOut
        || initialStatus.watchdogKillRequested
        || initialStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator initial status is not idle: state=%s handshake=%s stop=%s watchdog=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d connectionLost=%d\n",
                     statusName (initialStatus.state),
                     statusName (initialStatus.handshakeStatus),
                     statusName (initialStatus.stopStatus),
                     statusName (initialStatus.watchdogStatus),
                     initialStatus.launchAttempted ? 1 : 0,
                     initialStatus.readySeen ? 1 : 0,
                     initialStatus.probeEchoed ? 1 : 0,
                     initialStatus.stopRequested ? 1 : 0,
                     initialStatus.watchdogTimedOut ? 1 : 0,
                     initialStatus.watchdogKillRequested ? 1 : 0,
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
        || ! runningStatus.launchAttempted
        || ! runningStatus.readySeen
        || ! runningStatus.probeEchoed
        || runningStatus.stopRequested
        || runningStatus.watchdogTimedOut
        || runningStatus.watchdogKillRequested
        || runningStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator running status is wrong: state=%s handshake=%s stop=%s watchdog=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d connectionLost=%d\n",
                     statusName (runningStatus.state),
                     statusName (runningStatus.handshakeStatus),
                     statusName (runningStatus.stopStatus),
                     statusName (runningStatus.watchdogStatus),
                     runningStatus.launchAttempted ? 1 : 0,
                     runningStatus.readySeen ? 1 : 0,
                     runningStatus.probeEchoed ? 1 : 0,
                     runningStatus.stopRequested ? 1 : 0,
                     runningStatus.watchdogTimedOut ? 1 : 0,
                     runningStatus.watchdogKillRequested ? 1 : 0,
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
        || ! stoppedStatus.launchAttempted
        || ! stoppedStatus.readySeen
        || ! stoppedStatus.probeEchoed
        || ! stoppedStatus.stopRequested
        || stoppedStatus.watchdogTimedOut
        || stoppedStatus.watchdogKillRequested
        || ! stoppedStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator stopped status is wrong: state=%s handshake=%s stop=%s watchdog=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d connectionLost=%d\n",
                     statusName (stoppedStatus.state),
                     statusName (stoppedStatus.handshakeStatus),
                     statusName (stoppedStatus.stopStatus),
                     statusName (stoppedStatus.watchdogStatus),
                     stoppedStatus.launchAttempted ? 1 : 0,
                     stoppedStatus.readySeen ? 1 : 0,
                     stoppedStatus.probeEchoed ? 1 : 0,
                     stoppedStatus.stopRequested ? 1 : 0,
                     stoppedStatus.watchdogTimedOut ? 1 : 0,
                     stoppedStatus.watchdogKillRequested ? 1 : 0,
                     stoppedStatus.connectionLostSeen ? 1 : 0);
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
        || ! watchdogStatus.launchAttempted
        || ! watchdogStatus.readySeen
        || watchdogStatus.probeEchoed
        || watchdogStatus.stopRequested
        || ! watchdogStatus.watchdogTimedOut
        || ! watchdogStatus.watchdogKillRequested
        || ! watchdogStatus.connectionLostSeen)
    {
        std::printf ("FAIL: plugin host coordinator watchdog status is wrong: state=%s handshake=%s stop=%s watchdog=%s launch=%d ready=%d echo=%d stopRequested=%d watchdogTimeout=%d watchdogKill=%d connectionLost=%d\n",
                     statusName (watchdogStatus.state),
                     statusName (watchdogStatus.handshakeStatus),
                     statusName (watchdogStatus.stopStatus),
                     statusName (watchdogStatus.watchdogStatus),
                     watchdogStatus.launchAttempted ? 1 : 0,
                     watchdogStatus.readySeen ? 1 : 0,
                     watchdogStatus.probeEchoed ? 1 : 0,
                     watchdogStatus.stopRequested ? 1 : 0,
                     watchdogStatus.watchdogTimedOut ? 1 : 0,
                     watchdogStatus.watchdogKillRequested ? 1 : 0,
                     watchdogStatus.connectionLostSeen ? 1 : 0);
        return 2;
    }

    std::printf ("PASS: plugin host coordinator launched worker, reported ready/handshake status, stopped worker, and killed/reported a watchdog-timeout child\n");
    return 0;
}
