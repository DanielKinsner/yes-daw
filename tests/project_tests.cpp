// YES DAW - headless checks for ADR-0011 Project value types.
//
// This locks the storage-facing EntityId/Asset/Clip/Project surface before SQLite schema v1 starts
// serializing it.

#include "engine/Project.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
using yesdaw::engine::EntityIdAllocator;
using yesdaw::engine::kMaxUlidTimestampMs;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::UlidEntropy;

static_assert (sizeof (EntityId) == 16);
static_assert (EntityId::kNumBytes == 16);
static_assert (AssetContentHash::kNumBytes == 32);
static_assert (std::is_trivially_copyable_v<EntityId>);
static_assert (std::is_trivially_copyable_v<AssetContentHash>);
static_assert (std::is_trivially_copyable_v<Asset>);
static_assert (std::is_trivially_copyable_v<Clip>);

namespace {

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

Asset makeAsset (EntityId id, std::uint64_t frames = 48000)
{
    Asset asset;
    asset.id = id;
    asset.frames = frames;
    asset.sampleRate = SampleRate { 48000.0 };
    asset.channels = 2;
    return asset;
}

Clip makeClip (EntityId id, EntityId assetId, std::uint64_t srcOffset, std::uint64_t srcLen)
{
    Clip clip;
    clip.id = id;
    clip.assetId = assetId;
    clip.srcOffset = srcOffset;
    clip.srcLen = srcLen;
    clip.timelineLength = 15360;
    return clip;
}

} // namespace

TEST_CASE ("EntityId is a fixed 16-byte ULID storage value", "[project][entity-id]")
{
    const EntityId zero;
    REQUIRE (zero.isZero());
    REQUIRE_FALSE (zero.isValid());

    const EntityId id = EntityId::fromBigEndianParts (0x0102'0304'0506'0708ull, 0x090A'0B0C'0D0E'0F10ull);

    REQUIRE_FALSE (id.isZero());
    REQUIRE (id.isValid());
    REQUIRE (id.bytes[0] == 0x01u);
    REQUIRE (id.bytes[15] == 0x10u);
    REQUIRE (id.ulidTimestampMs() == 0x0102'0304'0506ull);
}

TEST_CASE ("EntityIdAllocator emits valid monotonic ULIDs", "[project][entity-id][allocator]")
{
    EntityIdAllocator allocator;

    const EntityId first = allocator.allocate (0);
    const EntityId second = allocator.allocate (0);
    const EntityId later = allocator.allocate (1);
    const EntityId clockWentBack = allocator.allocate (0);

    REQUIRE (first.isValid());
    REQUIRE (second.isValid());
    REQUIRE (later.isValid());
    REQUIRE (clockWentBack.isValid());
    REQUIRE (first.ulidTimestampMs() == 0u);
    REQUIRE (second.ulidTimestampMs() == 0u);
    REQUIRE (later.ulidTimestampMs() == 1u);
    REQUIRE (clockWentBack.ulidTimestampMs() == 1u);
    REQUIRE (first < second);
    REQUIRE (second < later);
    REQUIRE (later < clockWentBack);

    REQUIRE_FALSE (allocator.allocate (kMaxUlidTimestampMs + 1u).isValid());

    UlidEntropy carrySeed {};
    carrySeed[8] = 0x01u;
    carrySeed[9] = 0xFFu;

    EntityIdAllocator carryAllocator { carrySeed };
    const EntityId beforeCarry = carryAllocator.allocate (7);
    const EntityId afterCarry = carryAllocator.allocate (7);

    REQUIRE (beforeCarry < afterCarry);
    REQUIRE (afterCarry.bytes[14] == 0x02u);
    REQUIRE (afterCarry.bytes[15] == 0x00u);
}

TEST_CASE ("EntityIdAllocator reports entropy exhaustion instead of reusing an id", "[project][entity-id][allocator]")
{
    UlidEntropy almostFull {};
    almostFull.fill (0xFFu);

    EntityIdAllocator allocator { almostFull };
    const EntityId lastIdForTimestamp = allocator.allocate (42);
    REQUIRE (lastIdForTimestamp.isValid());
    REQUIRE_FALSE (allocator.allocate (42).isValid());
    REQUIRE_FALSE (allocator.allocate (42).isValid());

    const EntityId nextTimestamp = allocator.allocate (43);
    REQUIRE (nextTimestamp.isValid());
    REQUIRE (lastIdForTimestamp < nextTimestamp);
}

TEST_CASE ("Asset and Clip values carry ADR-0011 storage invariants", "[project][asset][clip]")
{
    const EntityId assetId = idFromLowByte (1);
    const Asset asset = makeAsset (assetId, 400);

    Clip clip = makeClip (idFromLowByte (2), assetId, 100, 300);
    clip.timelineStart = 15360;
    clip.timelineLength = 15360 * 4;
    clip.gain = 0.75f;
    clip.fadeIn = 128;
    clip.fadeOut = 256;
    clip.timeBase = TimeBase::SampleLocked;

    REQUIRE (asset.isValid());
    REQUIRE (clip.references (asset));
    REQUIRE (clip.sourceWindowFits (asset));
    REQUIRE (clip.srcOffset + clip.srcLen == asset.frames);
    REQUIRE (clip.timeBase == TimeBase::SampleLocked);

    const Clip unknownAsset = makeClip (idFromLowByte (3), idFromLowByte (99), 0, 1);
    REQUIRE_FALSE (unknownAsset.references (asset));
    REQUIRE_FALSE (unknownAsset.sourceWindowFits (asset));

    const Clip tooLong = makeClip (idFromLowByte (4), assetId, 100, 301);
    REQUIRE_FALSE (tooLong.sourceWindowFits (asset));

    const Clip overflow = makeClip (idFromLowByte (5), assetId, std::numeric_limits<std::uint64_t>::max(), 1);
    REQUIRE_FALSE (overflow.sourceWindowFits (Asset { assetId, {}, std::numeric_limits<std::uint64_t>::max(), SampleRate {}, 1 }));
}

TEST_CASE ("Project validates Asset to Clip indirection by EntityId", "[project][indirection]")
{
    const EntityId projectId = idFromLowByte (10);
    const EntityId assetA = idFromLowByte (11);
    const EntityId assetB = idFromLowByte (12);

    Project project;
    project.id = projectId;
    project.assets = {
        makeAsset (assetA, 1000),
        makeAsset (assetB, 256),
    };
    project.clips = {
        makeClip (idFromLowByte (20), assetA, 100, 900),
        makeClip (idFromLowByte (21), assetB, 0, 128),
    };

    REQUIRE (project.findAsset (assetA) == &project.assets[0]);
    REQUIRE (project.findAsset (idFromLowByte (99)) == nullptr);
    REQUIRE (project.hasValidAssetClipIndirection());

    Project duplicateClipId = project;
    duplicateClipId.clips[1].id = duplicateClipId.clips[0].id;
    REQUIRE_FALSE (duplicateClipId.hasUniqueEntityIds());
    REQUIRE_FALSE (duplicateClipId.hasValidAssetClipIndirection());

    Project duplicateAssetId = project;
    duplicateAssetId.assets[1].id = duplicateAssetId.assets[0].id;
    REQUIRE_FALSE (duplicateAssetId.hasUniqueEntityIds());
    REQUIRE_FALSE (duplicateAssetId.hasValidAssetClipIndirection());

    Project assetMatchesProjectId = project;
    assetMatchesProjectId.assets[1].id = project.id;
    REQUIRE_FALSE (assetMatchesProjectId.hasUniqueEntityIds());
    REQUIRE_FALSE (assetMatchesProjectId.hasValidAssetClipIndirection());

    Project clipMatchesAssetId = project;
    clipMatchesAssetId.clips[1].id = project.assets[0].id;
    REQUIRE_FALSE (clipMatchesAssetId.hasUniqueEntityIds());
    REQUIRE_FALSE (clipMatchesAssetId.hasValidAssetClipIndirection());

    Project invalidAsset = project;
    invalidAsset.assets[0].frames = 0;
    REQUIRE_FALSE (invalidAsset.assetsAreValid());
    REQUIRE_FALSE (invalidAsset.hasValidAssetClipIndirection());

    Project orphanClip = project;
    orphanClip.clips[0].assetId = idFromLowByte (99);
    REQUIRE_FALSE (orphanClip.clipsReferenceAssets());
    REQUIRE_FALSE (orphanClip.hasValidAssetClipIndirection());

    Project corruptSourceWindow = project;
    corruptSourceWindow.clips[1].srcOffset = 200;
    corruptSourceWindow.clips[1].srcLen = 100;
    REQUIRE_FALSE (corruptSourceWindow.clipsReferenceAssets());
    REQUIRE_FALSE (corruptSourceWindow.hasValidAssetClipIndirection());
}
