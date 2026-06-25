// YES DAW - headless checks for ADR-0011 Project value types.
//
// This locks the storage-facing EntityId/Asset/Clip/Project surface before SQLite schema v1 starts
// serializing it.

#include "engine/ClipEnvelope.h"
#include "engine/Project.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

using Catch::Approx;
using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::evaluateClipGainEnvelope;
using yesdaw::engine::EntityId;
using yesdaw::engine::EntityIdAllocator;
using yesdaw::engine::kMaxUlidTimestampMs;
using yesdaw::engine::moveClip;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::SampleRate;
using yesdaw::engine::setClipFades;
using yesdaw::engine::setClipGain;
using yesdaw::engine::splitClip;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Tick;
using yesdaw::engine::trimClip;
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

Project makeEditableProject()
{
    const EntityId assetId = idFromLowByte (30);

    Clip clip = makeClip (idFromLowByte (31), assetId, 100, 800);
    clip.timelineStart = 777;
    clip.timelineLength = 50'000;
    clip.gain = 0.625f;
    clip.fadeIn = 32;
    clip.fadeOut = 64;
    clip.timeBase = TimeBase::SampleLocked;

    Project project;
    project.id = idFromLowByte (29);
    project.sampleRate = SampleRate { 48000.0 };
    project.assets = { makeAsset (assetId, 1200) };
    project.clips = { clip };
    return project;
}

void requireProjectValueUnchanged (const Project& actual, const Project& expected)
{
    REQUIRE (actual.id == expected.id);
    REQUIRE (actual.sampleRate == expected.sampleRate);
    REQUIRE (actual.assets == expected.assets);
    REQUIRE (actual.clips == expected.clips);
}

double expectedEqualPowerGain (double x)
{
    const double bend = x * (1.0 - x);
    const double shaped = bend * (1.0 + 1.4186 * bend) + x;
    return shaped * shaped;
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

TEST_CASE ("Project splitClip creates exact adjacent Tick and source-frame windows", "[project][clip-edit][split]")
{
    Project project = makeEditableProject();
    const Asset originalAsset = project.assets.front();
    const Clip original = project.clips.front();

    constexpr Tick leftTicks = 12'345;
    constexpr std::uint64_t leftFrames = 321;
    const EntityId rightId = idFromLowByte (32);

    REQUIRE (splitClip (project, original.id, rightId, leftTicks, leftFrames) == ProjectEditStatus::Applied);
    REQUIRE (project.assets.size() == 1u);
    REQUIRE (project.assets.front() == originalAsset);
    REQUIRE (project.clips.size() == 2u);
    REQUIRE (project.hasValidAssetClipIndirection());

    const Clip& left = project.clips[0];
    const Clip& right = project.clips[1];

    REQUIRE (left.id == original.id);
    REQUIRE (right.id == rightId);
    REQUIRE (left.assetId == original.assetId);
    REQUIRE (right.assetId == original.assetId);

    REQUIRE (left.timelineStart == original.timelineStart);
    REQUIRE (left.timelineLength == leftTicks);
    REQUIRE (right.timelineStart == original.timelineStart + leftTicks);
    REQUIRE (right.timelineLength == original.timelineLength - leftTicks);

    REQUIRE (left.srcOffset == original.srcOffset);
    REQUIRE (left.srcLen == leftFrames);
    REQUIRE (right.srcOffset == left.srcOffset + left.srcLen);
    REQUIRE (right.srcLen == original.srcLen - leftFrames);
    REQUIRE (left.srcLen + right.srcLen == original.srcLen);

    REQUIRE (left.gain == original.gain);
    REQUIRE (right.gain == original.gain);
    REQUIRE (left.fadeIn == original.fadeIn);
    REQUIRE (right.fadeIn == original.fadeIn);
    REQUIRE (left.fadeOut == original.fadeOut);
    REQUIRE (right.fadeOut == original.fadeOut);
    REQUIRE (left.timeBase == original.timeBase);
    REQUIRE (right.timeBase == original.timeBase);
}

TEST_CASE ("Project trimClip and moveClip edit only placement metadata", "[project][clip-edit][trim][move]")
{
    Project project = makeEditableProject();
    const Asset originalAsset = project.assets.front();
    const Clip original = project.clips.front();

    REQUIRE (moveClip (project, original.id, -2048) == ProjectEditStatus::Applied);
    REQUIRE (project.clips.front().timelineStart == -2048);
    REQUIRE (project.clips.front().timelineLength == original.timelineLength);
    REQUIRE (project.clips.front().srcOffset == original.srcOffset);
    REQUIRE (project.clips.front().srcLen == original.srcLen);

    REQUIRE (trimClip (project, original.id, 4096, 22'000, 275, 250) == ProjectEditStatus::Applied);

    const Clip& edited = project.clips.front();
    REQUIRE (edited.timelineStart == 4096);
    REQUIRE (edited.timelineLength == 22'000);
    REQUIRE (edited.srcOffset == 275u);
    REQUIRE (edited.srcLen == 250u);
    REQUIRE (edited.id == original.id);
    REQUIRE (edited.assetId == original.assetId);
    REQUIRE (edited.gain == original.gain);
    REQUIRE (edited.fadeIn == original.fadeIn);
    REQUIRE (edited.fadeOut == original.fadeOut);
    REQUIRE (edited.timeBase == original.timeBase);
    REQUIRE (project.assets.front() == originalAsset);
    REQUIRE (project.hasValidAssetClipIndirection());
}

TEST_CASE ("Project setClipGain and setClipFades edit only envelope metadata", "[project][clip-edit][gain][fade]")
{
    Project project = makeEditableProject();
    const Asset originalAsset = project.assets.front();
    const Clip original = project.clips.front();

    REQUIRE (setClipGain (project, original.id, 1.25f) == ProjectEditStatus::Applied);
    REQUIRE (setClipFades (project, original.id, 960, 1920) == ProjectEditStatus::Applied);

    const Clip& edited = project.clips.front();
    REQUIRE (edited.id == original.id);
    REQUIRE (edited.assetId == original.assetId);
    REQUIRE (edited.timelineStart == original.timelineStart);
    REQUIRE (edited.timelineLength == original.timelineLength);
    REQUIRE (edited.srcOffset == original.srcOffset);
    REQUIRE (edited.srcLen == original.srcLen);
    REQUIRE (edited.timeBase == original.timeBase);
    REQUIRE (edited.gain == 1.25f);
    REQUIRE (edited.fadeIn == 960);
    REQUIRE (edited.fadeOut == 1920);
    REQUIRE (project.assets.front() == originalAsset);
    REQUIRE (project.hasValidAssetClipIndirection());
}

TEST_CASE ("Clip gain envelope derives equal-power fade gain without mutating Project", "[project][clip-envelope]")
{
    Project project = makeEditableProject();
    project.clips.front().timelineLength = 200;
    project.clips.front().gain = 0.5f;
    project.clips.front().fadeIn = 100;
    project.clips.front().fadeOut = 100;

    const Project before = project;
    const Clip& clip = project.clips.front();

    const auto beforeClip = evaluateClipGainEnvelope (clip, -1);
    REQUIRE_FALSE (beforeClip.valid);

    const auto start = evaluateClipGainEnvelope (clip, 0);
    REQUIRE (start.valid);
    REQUIRE (start.gain == Approx (0.0f).margin (1.0e-7f));

    const auto fadeInMid = evaluateClipGainEnvelope (clip, 50);
    REQUIRE (fadeInMid.valid);
    REQUIRE (fadeInMid.gain == Approx (0.5 * expectedEqualPowerGain (0.5)).margin (1.0e-6));

    const auto steady = evaluateClipGainEnvelope (clip, 100);
    REQUIRE (steady.valid);
    REQUIRE (steady.gain == Approx (0.5f).margin (1.0e-7f));

    const auto fadeOutMid = evaluateClipGainEnvelope (clip, 150);
    REQUIRE (fadeOutMid.valid);
    REQUIRE (fadeOutMid.gain == Approx (fadeInMid.gain).margin (1.0e-7f));

    const auto afterClip = evaluateClipGainEnvelope (clip, clip.timelineLength);
    REQUIRE_FALSE (afterClip.valid);

    requireProjectValueUnchanged (project, before);
}

TEST_CASE ("Adjacent Clip envelopes derive crossfade-compatible gains from existing metadata only", "[project][clip-envelope][crossfade]")
{
    Clip left = makeEditableProject().clips.front();
    left.timelineLength = 200;
    left.gain = 1.0f;
    left.fadeIn = 0;
    left.fadeOut = 100;

    Clip right = left;
    right.timelineStart = left.timelineStart + 100;
    right.fadeIn = 100;
    right.fadeOut = 0;

    const auto leftMidpoint = evaluateClipGainEnvelope (left, 150);
    const auto rightMidpoint = evaluateClipGainEnvelope (right, 50);

    REQUIRE (leftMidpoint.valid);
    REQUIRE (rightMidpoint.valid);
    REQUIRE (leftMidpoint.gain == Approx (rightMidpoint.gain).margin (1.0e-7f));
    REQUIRE (leftMidpoint.gain == Approx (expectedEqualPowerGain (0.5)).margin (1.0e-6));
}

TEST_CASE ("Clip gain envelope rejects invalid metadata and out-of-range positions", "[project][clip-envelope][invalid]")
{
    const Clip valid = makeEditableProject().clips.front();

    Clip invalidTimeline = valid;
    invalidTimeline.timelineLength = -1;
    REQUIRE_FALSE (evaluateClipGainEnvelope (invalidTimeline, 0).valid);

    Clip invalidGain = valid;
    invalidGain.gain = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE (evaluateClipGainEnvelope (invalidGain, 0).valid);

    invalidGain.gain = -0.1f;
    REQUIRE_FALSE (evaluateClipGainEnvelope (invalidGain, 0).valid);

    Clip invalidFade = valid;
    invalidFade.fadeOut = -1;
    REQUIRE_FALSE (evaluateClipGainEnvelope (invalidFade, 0).valid);

    REQUIRE_FALSE (evaluateClipGainEnvelope (valid, -1).valid);
    REQUIRE_FALSE (evaluateClipGainEnvelope (valid, valid.timelineLength).valid);
}

TEST_CASE ("Project clip edit operations reject invalid input without mutating Project", "[project][clip-edit][invalid]")
{
    Project project = makeEditableProject();
    const EntityId clipId = project.clips.front().id;
    const EntityId newClipId = idFromLowByte (40);

    {
        const Project before = project;
        REQUIRE (splitClip (project, idFromLowByte (99), newClipId, 100, 100) == ProjectEditStatus::ClipNotFound);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (splitClip (project, clipId, project.assets.front().id, 100, 100) == ProjectEditStatus::DuplicateEntityId);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (splitClip (project, clipId, newClipId, 0, 100) == ProjectEditStatus::InvalidTimelineWindow);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (splitClip (project, clipId, newClipId, project.clips.front().timelineLength, 100) == ProjectEditStatus::InvalidTimelineWindow);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (splitClip (project, clipId, newClipId, 100, 0) == ProjectEditStatus::InvalidSourceWindow);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (splitClip (project, clipId, newClipId, 100, project.clips.front().srcLen) == ProjectEditStatus::InvalidSourceWindow);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (trimClip (project, clipId, 0, -1, 0, 1) == ProjectEditStatus::InvalidTimelineWindow);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (trimClip (project, clipId, 0, 1, project.assets.front().frames + 1u, 1) == ProjectEditStatus::InvalidSourceWindow);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (setClipGain (project, idFromLowByte (99), 1.0f) == ProjectEditStatus::ClipNotFound);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (setClipFades (project, {}, 10, 10) == ProjectEditStatus::InvalidClipId);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (setClipGain (project, clipId, -0.01f) == ProjectEditStatus::InvalidClipEnvelope);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (setClipGain (project, clipId, std::numeric_limits<float>::infinity()) == ProjectEditStatus::InvalidClipEnvelope);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (setClipGain (project, clipId, std::numeric_limits<float>::quiet_NaN()) == ProjectEditStatus::InvalidClipEnvelope);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (setClipFades (project, clipId, -1, 10) == ProjectEditStatus::InvalidClipEnvelope);
        requireProjectValueUnchanged (project, before);
    }

    {
        const Project before = project;
        REQUIRE (setClipFades (project, clipId, 10, -1) == ProjectEditStatus::InvalidClipEnvelope);
        requireProjectValueUnchanged (project, before);
    }

    {
        Project invalid = project;
        invalid.sampleRate = SampleRate { 0.0 };
        const Project before = invalid;
        REQUIRE (moveClip (invalid, clipId, 123) == ProjectEditStatus::InvalidProject);
        requireProjectValueUnchanged (invalid, before);
    }

    {
        Project invalid = project;
        invalid.clips.front().timelineLength = -1;
        const Project before = invalid;
        REQUIRE (moveClip (invalid, clipId, 123) == ProjectEditStatus::InvalidProject);
        requireProjectValueUnchanged (invalid, before);
    }

    {
        Project invalid = project;
        invalid.clips.front().gain = -1.0f;
        const Project before = invalid;
        REQUIRE (setClipFades (invalid, clipId, 10, 10) == ProjectEditStatus::InvalidProject);
        requireProjectValueUnchanged (invalid, before);
    }

    {
        Project invalid = project;
        invalid.clips.front().fadeIn = -1;
        const Project before = invalid;
        REQUIRE (setClipGain (invalid, clipId, 1.0f) == ProjectEditStatus::InvalidProject);
        requireProjectValueUnchanged (invalid, before);
    }

    {
        Project invalid = project;
        invalid.clips.front().timeBase = static_cast<TimeBase> (255);
        const Project before = invalid;
        REQUIRE (splitClip (invalid, clipId, newClipId, 100, 100) == ProjectEditStatus::InvalidProject);
        requireProjectValueUnchanged (invalid, before);
    }
}
