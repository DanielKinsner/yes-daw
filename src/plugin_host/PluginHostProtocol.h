#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
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

inline constexpr std::uint32_t kPluginStateRequestMagic = 0x59445351u; // YDSQ
inline constexpr std::uint32_t kPluginStateReplyMagic = 0x59445352u;   // YDSR
inline constexpr std::uint32_t kPluginStateMessageVersion = 1u;
inline constexpr std::size_t kPluginStateChunkCapacity = 256u;

enum class RtLaneLoadReplyStatus : std::uint32_t
{
    none = 0,
    accepted = 1,
    rejectedInvalidIdentity = 2,
    rejectedAttachFailed = 3
};

enum class PluginStateRequestKind : std::uint32_t
{
    none = 0,
    pull = 1,
    push = 2
};

enum class PluginStateReplyStatus : std::uint32_t
{
    none = 0,
    pulled = 1,
    restored = 2,
    rejectedInvalidRequest = 3,
    rejectedNoProcessor = 4,
    rejectedChunkTooLarge = 5,
    rejectedCrcMismatch = 6,
    rejectedSetStateFailed = 7
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

struct PluginStateRequestMessage
{
    std::uint32_t magic = kPluginStateRequestMagic;
    std::uint32_t version = kPluginStateMessageVersion;
    PluginStateRequestKind kind = PluginStateRequestKind::none;
    std::uint32_t chunkLength = 0;
    std::uint32_t crc32 = 0;
    std::uint8_t chunk[kPluginStateChunkCapacity] {};
};

struct PluginStateReplyMessage
{
    std::uint32_t magic = kPluginStateReplyMagic;
    std::uint32_t version = kPluginStateMessageVersion;
    PluginStateReplyStatus status = PluginStateReplyStatus::none;
    std::uint32_t chunkLength = 0;
    std::uint32_t crc32 = 0;
    std::uint32_t stateAccepted = 0;
    std::uint8_t chunk[kPluginStateChunkCapacity] {};
};

static_assert (std::is_trivially_copyable_v<RtLaneLoadMessage>);
static_assert (std::is_trivially_copyable_v<RtLaneLoadReplyMessage>);
static_assert (std::is_trivially_copyable_v<PluginStateRequestMessage>);
static_assert (std::is_trivially_copyable_v<PluginStateReplyMessage>);

inline std::uint32_t crc32Bytes (std::span<const std::uint8_t> bytes) noexcept
{
    std::uint32_t crc = 0xffffffffu;

    for (const std::uint8_t byte : bytes)
    {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & static_cast<std::uint32_t> (-static_cast<int> (crc & 1u)));
    }

    return ~crc;
}

inline std::span<const std::uint8_t> pluginStateChunkBytes (const PluginStateRequestMessage& message) noexcept
{
    return { message.chunk, std::min<std::size_t> (message.chunkLength, kPluginStateChunkCapacity) };
}

inline std::span<const std::uint8_t> pluginStateChunkBytes (const PluginStateReplyMessage& message) noexcept
{
    return { message.chunk, std::min<std::size_t> (message.chunkLength, kPluginStateChunkCapacity) };
}

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

inline PluginStateRequestMessage makePluginStatePullRequestMessage() noexcept
{
    PluginStateRequestMessage message;
    message.kind = PluginStateRequestKind::pull;
    return message;
}

inline PluginStateRequestMessage makePluginStatePushRequestMessage (std::span<const std::uint8_t> chunk,
                                                                    std::uint32_t crc32) noexcept
{
    PluginStateRequestMessage message;
    message.kind = PluginStateRequestKind::push;

    if (chunk.size() <= kPluginStateChunkCapacity)
    {
        message.chunkLength = static_cast<std::uint32_t> (chunk.size());
        message.crc32 = crc32;
        if (! chunk.empty())
            std::memcpy (message.chunk, chunk.data(), chunk.size());
    }

    return message;
}

inline PluginStateReplyMessage makePluginStateReplyMessage (PluginStateReplyStatus status,
                                                            std::span<const std::uint8_t> chunk = {},
                                                            bool stateAccepted = false) noexcept
{
    PluginStateReplyMessage message;
    message.status = status;
    message.stateAccepted = stateAccepted ? 1u : 0u;

    if (chunk.size() <= kPluginStateChunkCapacity)
    {
        message.chunkLength = static_cast<std::uint32_t> (chunk.size());
        message.crc32 = crc32Bytes (chunk);
        if (! chunk.empty())
            std::memcpy (message.chunk, chunk.data(), chunk.size());
    }
    else
    {
        message.status = PluginStateReplyStatus::rejectedChunkTooLarge;
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

inline bool pluginStateRequestCrcMatches (const PluginStateRequestMessage& message) noexcept
{
    return message.chunkLength <= kPluginStateChunkCapacity
        && crc32Bytes (pluginStateChunkBytes (message)) == message.crc32;
}

inline bool pluginStateReplyCrcMatches (const PluginStateReplyMessage& message) noexcept
{
    return message.chunkLength <= kPluginStateChunkCapacity
        && crc32Bytes (pluginStateChunkBytes (message)) == message.crc32;
}

inline bool isValidPluginStateRequestMessage (const PluginStateRequestMessage& message) noexcept
{
    if (message.magic != kPluginStateRequestMagic
        || message.version != kPluginStateMessageVersion)
        return false;

    if (message.kind == PluginStateRequestKind::pull)
        return message.chunkLength == 0u && message.crc32 == 0u;

    if (message.kind == PluginStateRequestKind::push)
        return message.chunkLength > 0u
            && message.chunkLength <= kPluginStateChunkCapacity
            && pluginStateRequestCrcMatches (message);

    return false;
}

inline bool isValidPluginStateReplyMessage (const PluginStateReplyMessage& message) noexcept
{
    if (message.magic != kPluginStateReplyMagic
        || message.version != kPluginStateMessageVersion
        || message.status == PluginStateReplyStatus::none
        || message.chunkLength > kPluginStateChunkCapacity)
        return false;

    if (message.status == PluginStateReplyStatus::pulled
        || message.status == PluginStateReplyStatus::restored)
    {
        return message.chunkLength > 0u
            && pluginStateReplyCrcMatches (message)
            && (message.status != PluginStateReplyStatus::restored || message.stateAccepted != 0u);
    }

    return true;
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

inline bool copyPluginStateRequestMessage (const void* data, std::size_t bytes, PluginStateRequestMessage& out) noexcept
{
    if (data == nullptr || bytes != sizeof (PluginStateRequestMessage))
        return false;

    std::memcpy (&out, data, sizeof (out));
    return out.magic == kPluginStateRequestMagic && out.version == kPluginStateMessageVersion;
}

inline bool copyPluginStateReplyMessage (const void* data, std::size_t bytes, PluginStateReplyMessage& out) noexcept
{
    if (data == nullptr || bytes != sizeof (PluginStateReplyMessage))
        return false;

    std::memcpy (&out, data, sizeof (out));
    return isValidPluginStateReplyMessage (out);
}

} // namespace yesdaw::plugin_host
