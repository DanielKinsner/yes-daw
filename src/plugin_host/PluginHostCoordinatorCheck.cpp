#include "plugin_host/PluginHostCoordinator.h"

#include <cstdio>

namespace {

const char* statusName (yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus status) noexcept
{
    using Status = yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus;

    switch (status)
    {
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
        case Status::stopped:     return "stopped";
        case Status::stopTimeout: return "stopTimeout";
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
    const auto handshake = coordinator.launchAndHandshake (workerExecutable);

    if (handshake.status != yesdaw::plugin_host::PluginHostCoordinator::HandshakeStatus::success)
    {
        std::printf ("FAIL: plugin host coordinator handshake failed: status=%s ready=%d echo=%d\n",
                     statusName (handshake.status),
                     handshake.readySeen ? 1 : 0,
                     handshake.probeEchoed ? 1 : 0);
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

    std::printf ("PASS: plugin host coordinator launched worker, completed ready/echo handshake, stopped worker, and observed connection loss\n");
    return 0;
}
