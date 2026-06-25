// YES DAW - Project data-model value surface (ADR-0011).
//
// Storage-facing, JUCE-free value types for the non-destructive Asset -> Clip -> Project indirection.
// Persistence code will serialize these later; this header only locks the ID shape and semantic
// invariants that schema v1 will depend on.

#pragma once

#include "engine/Time.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace yesdaw::engine {

struct EntityId
{
    static constexpr std::size_t kNumBytes = 16;
    using StorageBytes = std::array<std::uint8_t, kNumBytes>;

    StorageBytes bytes {};

    static constexpr EntityId fromBytes (StorageBytes value) noexcept
    {
        return EntityId { value };
    }

    static constexpr EntityId fromBigEndianParts (std::uint64_t high, std::uint64_t low) noexcept
    {
        StorageBytes out {};

        for (std::size_t i = 0; i < 8; ++i)
            out[i] = static_cast<std::uint8_t> ((high >> ((7u - i) * 8u)) & 0xFFu);

        for (std::size_t i = 0; i < 8; ++i)
            out[8u + i] = static_cast<std::uint8_t> ((low >> ((7u - i) * 8u)) & 0xFFu);

        return EntityId { out };
    }

    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        for (const std::uint8_t b : bytes)
            if (b != 0u)
                return false;

        return true;
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return ! isZero();
    }

    [[nodiscard]] constexpr std::uint64_t ulidTimestampMs() const noexcept
    {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < 6; ++i)
            value = (value << 8u) | static_cast<std::uint64_t> (bytes[i]);

        return value;
    }

    friend constexpr bool operator== (const EntityId&, const EntityId&) noexcept = default;

    friend constexpr bool operator< (const EntityId& a, const EntityId& b) noexcept
    {
        for (std::size_t i = 0; i < kNumBytes; ++i)
        {
            if (a.bytes[i] < b.bytes[i])
                return true;
            if (a.bytes[i] > b.bytes[i])
                return false;
        }

        return false;
    }
};

static_assert (sizeof (EntityId) == EntityId::kNumBytes, "EntityId must stay a fixed 16-byte value");

using UlidEntropy = std::array<std::uint8_t, 10>;
constexpr std::uint64_t kMaxUlidTimestampMs = (1ull << 48u) - 1ull;

class EntityIdAllocator final
{
public:
    explicit constexpr EntityIdAllocator (UlidEntropy seed = {}) noexcept : entropy_ (seed) {}

    [[nodiscard]] EntityId allocate (std::uint64_t unixTimeMs) noexcept
    {
        if (unixTimeMs > kMaxUlidTimestampMs)
            return {};

        std::uint64_t timestamp = unixTimeMs;

        if (! haveLastTimestamp_)
        {
            haveLastTimestamp_ = true;
            lastTimestampMs_ = timestamp;
        }
        else if (timestamp <= lastTimestampMs_)
        {
            timestamp = lastTimestampMs_;
            if (! incrementEntropy())
                return {};
        }
        else
        {
            lastTimestampMs_ = timestamp;
        }

        EntityId id = makeUlid (timestamp, entropy_);
        if (! id.isValid())
        {
            if (! incrementEntropy())
                return {};

            id = makeUlid (timestamp, entropy_);
        }

        return id;
    }

private:
    static constexpr EntityId makeUlid (std::uint64_t timestampMs, const UlidEntropy& entropy) noexcept
    {
        EntityId::StorageBytes out {};

        for (std::size_t i = 0; i < 6; ++i)
            out[i] = static_cast<std::uint8_t> ((timestampMs >> ((5u - i) * 8u)) & 0xFFu);

        for (std::size_t i = 0; i < entropy.size(); ++i)
            out[6u + i] = entropy[i];

        return EntityId { out };
    }

    bool incrementEntropy() noexcept
    {
        std::size_t index = entropy_.size();
        while (index > 0 && entropy_[index - 1u] == 0xFFu)
            --index;

        if (index == 0)
            return false;

        ++entropy_[index - 1u];
        for (std::size_t i = index; i < entropy_.size(); ++i)
        {
            entropy_[i] = 0u;
        }

        return true;
    }

    bool          haveLastTimestamp_ = false;
    std::uint64_t lastTimestampMs_ = 0;
    UlidEntropy   entropy_ {};
};

struct AssetContentHash
{
    static constexpr std::size_t kNumBytes = 32;
    using StorageBytes = std::array<std::uint8_t, kNumBytes>;

    StorageBytes bytes {};

    friend constexpr bool operator== (const AssetContentHash&, const AssetContentHash&) noexcept = default;
};

struct Asset
{
    EntityId   id;
    AssetContentHash contentHash;
    std::uint64_t frames = 0;
    SampleRate    sampleRate;
    std::uint16_t channels = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return id.isValid() && frames > 0 && sampleRate.isValid() && channels > 0;
    }

    friend constexpr bool operator== (const Asset&, const Asset&) noexcept = default;
};

struct Clip
{
    EntityId id;
    EntityId assetId;
    Tick timelineStart = 0;
    Tick timelineLength = 0;
    std::uint64_t srcOffset = 0; // schema: clip.src_offset, measured in Asset frames
    std::uint64_t srcLen = 0;    // schema: clip.src_len, measured in Asset frames
    float gain = 1.0f;
    Tick fadeIn = 0;
    Tick fadeOut = 0;
    TimeBase timeBase = TimeBase::SampleLocked;

    [[nodiscard]] constexpr bool references (const Asset& asset) const noexcept
    {
        return assetId.isValid() && asset.id == assetId;
    }

    [[nodiscard]] constexpr bool sourceWindowFits (const Asset& asset) const noexcept
    {
        return references (asset) && srcOffset <= asset.frames && srcLen <= asset.frames - srcOffset;
    }

    friend constexpr bool operator== (const Clip&, const Clip&) noexcept = default;
};

enum class ProjectEditStatus : std::uint8_t
{
    Applied = 0,
    InvalidProject,
    InvalidClipId,
    ClipNotFound,
    DuplicateEntityId,
    InvalidTimelineWindow,
    InvalidSourceWindow
};

struct Project
{
    EntityId id;
    SampleRate sampleRate;
    std::vector<Asset> assets;
    std::vector<Clip> clips;

    [[nodiscard]] const Asset* findAsset (EntityId assetId) const noexcept
    {
        for (const Asset& asset : assets)
            if (asset.id == assetId)
                return &asset;

        return nullptr;
    }

    [[nodiscard]] bool hasValidEntityIds() const noexcept
    {
        if (! id.isValid())
            return false;

        for (const Asset& asset : assets)
            if (! asset.id.isValid())
                return false;

        for (const Clip& clip : clips)
            if (! clip.id.isValid() || ! clip.assetId.isValid())
                return false;

        return true;
    }

    [[nodiscard]] bool assetsAreValid() const noexcept
    {
        for (const Asset& asset : assets)
            if (! asset.isValid())
                return false;

        return true;
    }

    [[nodiscard]] bool hasUniqueEntityIds() const noexcept
    {
        for (std::size_t i = 0; i < assets.size(); ++i)
        {
            if (assets[i].id == id)
                return false;

            for (std::size_t j = 0; j < i; ++j)
                if (assets[i].id == assets[j].id)
                    return false;
        }

        for (std::size_t i = 0; i < clips.size(); ++i)
        {
            if (clips[i].id == id)
                return false;

            for (const Asset& asset : assets)
                if (clips[i].id == asset.id)
                    return false;

            for (std::size_t j = 0; j < i; ++j)
                if (clips[i].id == clips[j].id)
                    return false;
        }

        return true;
    }

    [[nodiscard]] bool clipsReferenceAssets() const noexcept
    {
        for (const Clip& clip : clips)
        {
            const Asset* const asset = findAsset (clip.assetId);
            if (asset == nullptr || ! clip.sourceWindowFits (*asset))
                return false;
        }

        return true;
    }

    [[nodiscard]] bool hasValidAssetClipIndirection() const noexcept
    {
        return sampleRate.isValid() && hasValidEntityIds() && assetsAreValid() && hasUniqueEntityIds() && clipsReferenceAssets();
    }
};

namespace detail {

[[nodiscard]] inline bool addTickChecked (Tick a, Tick b, Tick& out) noexcept
{
    if (b > 0 && a > std::numeric_limits<Tick>::max() - b)
        return false;

    if (b < 0 && a < std::numeric_limits<Tick>::min() - b)
        return false;

    out = a + b;
    return true;
}

[[nodiscard]] inline Clip* findClip (Project& project, EntityId clipId) noexcept
{
    for (Clip& clip : project.clips)
        if (clip.id == clipId)
            return &clip;

    return nullptr;
}

[[nodiscard]] inline bool projectContainsEntityId (const Project& project, EntityId id) noexcept
{
    if (! id.isValid())
        return false;

    if (project.id == id)
        return true;

    for (const Asset& asset : project.assets)
        if (asset.id == id)
            return true;

    for (const Clip& clip : project.clips)
        if (clip.id == id)
            return true;

    return false;
}

[[nodiscard]] inline bool clipSourceWindowFitsProject (const Project& project, const Clip& clip) noexcept
{
    const Asset* const asset = project.findAsset (clip.assetId);
    return asset != nullptr && clip.sourceWindowFits (*asset);
}

[[nodiscard]] inline bool clipEditMetadataIsStorageSafe (const Clip& clip) noexcept
{
    return clip.timelineLength >= 0 && clip.fadeIn >= 0 && clip.fadeOut >= 0 && clip.gain >= 0.0f
           && clip.gain <= std::numeric_limits<float>::max()
           && (clip.timeBase == TimeBase::TempoLocked || clip.timeBase == TimeBase::SampleLocked);
}

[[nodiscard]] inline bool projectCanApplyClipEdit (const Project& project) noexcept
{
    if (! project.hasValidAssetClipIndirection())
        return false;

    for (const Clip& clip : project.clips)
        if (! clipEditMetadataIsStorageSafe (clip))
            return false;

    return true;
}

} // namespace detail

[[nodiscard]] inline ProjectEditStatus moveClip (Project& project, EntityId clipId, Tick newTimelineStart) noexcept
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    Clip* const clip = detail::findClip (project, clipId);
    if (clip == nullptr)
        return ProjectEditStatus::ClipNotFound;

    clip->timelineStart = newTimelineStart;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus trimClip (Project& project,
                                                 EntityId clipId,
                                                 Tick newTimelineStart,
                                                 Tick newTimelineLength,
                                                 std::uint64_t newSrcOffset,
                                                 std::uint64_t newSrcLen) noexcept
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    Clip* const clip = detail::findClip (project, clipId);
    if (clip == nullptr)
        return ProjectEditStatus::ClipNotFound;

    if (newTimelineLength < 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    Clip edited = *clip;
    edited.timelineStart = newTimelineStart;
    edited.timelineLength = newTimelineLength;
    edited.srcOffset = newSrcOffset;
    edited.srcLen = newSrcLen;

    if (! detail::clipSourceWindowFitsProject (project, edited))
        return ProjectEditStatus::InvalidSourceWindow;

    *clip = edited;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus splitClip (Project& project,
                                                  EntityId clipId,
                                                  EntityId rightClipId,
                                                  Tick leftTimelineLength,
                                                  std::uint64_t leftSourceLength)
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid() || ! rightClipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    if (detail::projectContainsEntityId (project, rightClipId))
        return ProjectEditStatus::DuplicateEntityId;

    std::size_t clipIndex = project.clips.size();
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        if (project.clips[i].id == clipId)
        {
            clipIndex = i;
            break;
        }
    }

    if (clipIndex == project.clips.size())
        return ProjectEditStatus::ClipNotFound;

    const Clip original = project.clips[clipIndex];
    if (leftTimelineLength <= 0 || leftTimelineLength >= original.timelineLength)
        return ProjectEditStatus::InvalidTimelineWindow;

    Tick rightTimelineStart = 0;
    if (! detail::addTickChecked (original.timelineStart, leftTimelineLength, rightTimelineStart))
        return ProjectEditStatus::InvalidTimelineWindow;

    if (leftSourceLength == 0 || leftSourceLength >= original.srcLen)
        return ProjectEditStatus::InvalidSourceWindow;

    Clip left = original;
    left.timelineLength = leftTimelineLength;
    left.srcLen = leftSourceLength;

    Clip right = original;
    right.id = rightClipId;
    right.timelineStart = rightTimelineStart;
    right.timelineLength = original.timelineLength - leftTimelineLength;
    right.srcOffset = original.srcOffset + leftSourceLength;
    right.srcLen = original.srcLen - leftSourceLength;

    if (! detail::clipSourceWindowFitsProject (project, left) || ! detail::clipSourceWindowFitsProject (project, right))
        return ProjectEditStatus::InvalidSourceWindow;

    project.clips.insert (project.clips.begin() + static_cast<std::ptrdiff_t> (clipIndex + 1u), right);
    project.clips[clipIndex] = left;
    project.clips[clipIndex + 1u] = right;
    return ProjectEditStatus::Applied;
}

} // namespace yesdaw::engine
