#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace yesdaw::plugin_host {

inline constexpr const char* kWorkerCommandLineId = "yesdawpluginhost";
inline constexpr const char* kWorkerReadyMessage = "ready";
inline constexpr const char* kHandshakeProbeMessage = "yesdaw-host-handshake-v1";
inline constexpr const char* kWatchdogProbeMessage = "yesdaw-host-watchdog-timeout-v1";
inline constexpr const char* kRunningWatchdogRtLaneHangMessage =
    "yesdaw-host-running-watchdog-rt-lane-hang-v1";
inline constexpr const char* kRunningWatchdogRtLaneHangAckMessage =
    "yesdaw-host-running-watchdog-rt-lane-hang-ack-v1";

inline constexpr std::uint32_t kRtLaneLoadMessageMagic = 0x59445254u; // YDRT
inline constexpr std::uint32_t kRtLaneLoadReplyMagic = 0x59445252u;   // YDRR
inline constexpr std::uint32_t kRtLaneLoadMessageVersion = 1u;
inline constexpr std::size_t kRtLaneSharedMemoryNameCapacity = 128u;

enum class RtLaneLoadReplyStatus : std::uint32_t
{
    none = 0,
    accepted = 1,
    rejectedInvalidIdentity = 2,
    rejectedAttachFailed = 3
};

struct RtLaneLoadConfig
{
    std::uint32_t channels = 0;
    std::uint32_t maxBlockSize = 0;
    std::uint32_t maxEventsPerBlock = 0;
    std::uint32_t lastGoodHoldBlocks = 0;
    std::uint32_t bypassAfterMisses = 0;
};

struct RtLaneLoadMessage
{
    std::uint32_t magic = kRtLaneLoadMessageMagic;
    std::uint32_t version = kRtLaneLoadMessageVersion;
    RtLaneLoadConfig config;
    std::uint32_t sharedMemoryNameLength = 0;
    char sharedMemoryName[kRtLaneSharedMemoryNameCapacity] {};
};

struct RtLaneLoadReplyMessage
{
    std::uint32_t magic = kRtLaneLoadReplyMagic;
    std::uint32_t version = kRtLaneLoadMessageVersion;
    RtLaneLoadReplyStatus status = RtLaneLoadReplyStatus::none;
    std::int32_t attachFailure = 0;
    std::int32_t attachSystemError = 0;
    std::uint32_t sharedMemoryNameLength = 0;
    char sharedMemoryName[kRtLaneSharedMemoryNameCapacity] {};
};

static_assert (std::is_trivially_copyable_v<RtLaneLoadMessage>);
static_assert (std::is_trivially_copyable_v<RtLaneLoadReplyMessage>);

inline bool isValidRtLaneSharedMemoryName (std::string_view name) noexcept
{
    if (name.empty() || name.size() >= kRtLaneSharedMemoryNameCapacity)
        return false;

    for (const char ch : name)
        if (ch == '\0')
            return false;

    return true;
}

inline bool isValidRtLaneLoadConfig (RtLaneLoadConfig config) noexcept
{
    return config.channels > 0u
        && config.maxBlockSize > 0u
        && config.bypassAfterMisses >= config.lastGoodHoldBlocks;
}

inline RtLaneLoadMessage makeRtLaneLoadMessage (std::string_view sharedMemoryName,
                                                RtLaneLoadConfig config) noexcept
{
    RtLaneLoadMessage message;
    message.config = config;

    if (isValidRtLaneSharedMemoryName (sharedMemoryName))
    {
        message.sharedMemoryNameLength = static_cast<std::uint32_t> (sharedMemoryName.size());
        std::memcpy (message.sharedMemoryName, sharedMemoryName.data(), sharedMemoryName.size());
    }

    return message;
}

inline RtLaneLoadReplyMessage makeRtLaneLoadReplyMessage (RtLaneLoadReplyStatus status,
                                                          std::string_view sharedMemoryName,
                                                          std::int32_t attachFailure = 0,
                                                          std::int32_t attachSystemError = 0) noexcept
{
    RtLaneLoadReplyMessage message;
    message.status = status;
    message.attachFailure = attachFailure;
    message.attachSystemError = attachSystemError;

    if (isValidRtLaneSharedMemoryName (sharedMemoryName))
    {
        message.sharedMemoryNameLength = static_cast<std::uint32_t> (sharedMemoryName.size());
        std::memcpy (message.sharedMemoryName, sharedMemoryName.data(), sharedMemoryName.size());
    }

    return message;
}

inline bool isValidRtLaneLoadMessage (const RtLaneLoadMessage& message) noexcept
{
    if (message.magic != kRtLaneLoadMessageMagic
        || message.version != kRtLaneLoadMessageVersion
        || ! isValidRtLaneLoadConfig (message.config)
        || message.sharedMemoryNameLength == 0u
        || message.sharedMemoryNameLength >= kRtLaneSharedMemoryNameCapacity
        || message.sharedMemoryName[message.sharedMemoryNameLength] != '\0')
        return false;

    return isValidRtLaneSharedMemoryName (
        std::string_view (message.sharedMemoryName, message.sharedMemoryNameLength));
}

inline bool isValidRtLaneLoadReplyMessage (const RtLaneLoadReplyMessage& message) noexcept
{
    if (message.magic != kRtLaneLoadReplyMagic
        || message.version != kRtLaneLoadMessageVersion
        || message.status == RtLaneLoadReplyStatus::none
        || message.sharedMemoryNameLength >= kRtLaneSharedMemoryNameCapacity
        || message.sharedMemoryName[message.sharedMemoryNameLength] != '\0')
        return false;

    if (message.sharedMemoryNameLength == 0u)
        return message.status == RtLaneLoadReplyStatus::rejectedInvalidIdentity;

    return isValidRtLaneSharedMemoryName (
        std::string_view (message.sharedMemoryName, message.sharedMemoryNameLength));
}

inline std::string rtLaneSharedMemoryName (const RtLaneLoadMessage& message)
{
    return isValidRtLaneLoadMessage (message)
        ? std::string (message.sharedMemoryName, message.sharedMemoryNameLength)
        : std::string {};
}

inline std::string rtLaneSharedMemoryName (const RtLaneLoadReplyMessage& message)
{
    return isValidRtLaneLoadReplyMessage (message) && message.sharedMemoryNameLength > 0u
        ? std::string (message.sharedMemoryName, message.sharedMemoryNameLength)
        : std::string {};
}

inline bool copyRtLaneLoadMessage (const void* data, std::size_t bytes, RtLaneLoadMessage& out) noexcept
{
    if (data == nullptr || bytes != sizeof (RtLaneLoadMessage))
        return false;

    std::memcpy (&out, data, sizeof (out));
    return out.magic == kRtLaneLoadMessageMagic && out.version == kRtLaneLoadMessageVersion;
}

inline bool copyRtLaneLoadReplyMessage (const void* data, std::size_t bytes, RtLaneLoadReplyMessage& out) noexcept
{
    if (data == nullptr || bytes != sizeof (RtLaneLoadReplyMessage))
        return false;

    std::memcpy (&out, data, sizeof (out));
    return isValidRtLaneLoadReplyMessage (out);
}

} // namespace yesdaw::plugin_host
