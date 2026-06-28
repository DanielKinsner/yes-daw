// YES DAW - H9 plugin failure -> persistent blacklist action.

#pragma once

#include "persistence/ProjectBundle.h"

#include <cstdint>
#include <string>

namespace yesdaw::persistence {

enum class PluginFailureKind : std::uint8_t
{
    Crash,
    WatchdogTimeout
};

struct PluginFailureIdentity
{
    PluginStateFormat format = PluginStateFormat::Vst3;
    std::string       pluginUid;
    std::string       pluginVersion;
};

[[nodiscard]] inline std::string pluginFailureReason (PluginFailureKind kind)
{
    return kind == PluginFailureKind::Crash ? "crash" : "watchdog-timeout";
}

[[nodiscard]] inline BundleResult writePluginBlacklistEntryForFailure (
    ProjectBundleDb& db,
    const PluginFailureIdentity& identity,
    PluginFailureKind kind)
{
    return db.writePluginBlacklistEntry ({
        identity.format,
        identity.pluginUid,
        identity.pluginVersion,
        pluginFailureReason (kind)
    });
}

} // namespace yesdaw::persistence
