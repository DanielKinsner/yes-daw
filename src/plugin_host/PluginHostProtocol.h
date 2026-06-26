#pragma once

namespace yesdaw::plugin_host {

inline constexpr const char* kWorkerCommandLineId = "yesdawpluginhost";
inline constexpr const char* kWorkerReadyMessage = "ready";
inline constexpr const char* kHandshakeProbeMessage = "yesdaw-host-handshake-v1";
inline constexpr const char* kWatchdogProbeMessage = "yesdaw-host-watchdog-timeout-v1";

} // namespace yesdaw::plugin_host
