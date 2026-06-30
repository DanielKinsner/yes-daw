// YES DAW - headless checks for ADR-0011 Project value types.
//
// This locks the storage-facing EntityId/Asset/Clip/Project surface before SQLite schema v1 starts
// serializing it.

#include "engine/ClipEnvelope.h"
#include "engine/Project.h"
#include "engine/ProjectUndo.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <utility>

using Catch::Approx;
using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::cutNote;
using yesdaw::engine::evaluateClipGainEnvelope;
using yesdaw::engine::EntityId;
using yesdaw::engine::EntityIdAllocator;
using yesdaw::engine::kMaxUlidTimestampMs;
using yesdaw::engine::kTicksPerQuarter;
using yesdaw::engine::MidiClip;
using yesdaw::engine::moveClip;
using yesdaw::engine::moveNote;
using yesdaw::engine::Note;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditCommand;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::ProjectEditVerb;
using yesdaw::engine::ProjectUndoStack;
using yesdaw::engine::ProjectUndoStatus;
using yesdaw::engine::ProjectRecordingCompSegment;
using yesdaw::engine::RecordingMonitoringPolicy;
using yesdaw::engine::RecordingTake;
using yesdaw::engine::SampleRate;
using yesdaw::engine::setClipFades;
using yesdaw::engine::setClipGain;
using yesdaw::engine::setNoteLength;
using yesdaw::engine::SnapGrid;
using yesdaw::engine::splitClip;
using yesdaw::engine::splitNote;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Tick;
using yesdaw::engine::Track;
using yesdaw::engine::trimClip;
using yesdaw::engine::transposeNote;
using yesdaw::engine::quantizeNote;
using yesdaw::engine::UlidEntropy;

static_assert (sizeof (EntityId) == 16);
static_assert (EntityId::kNumBytes == 16);
static_assert (AssetContentHash::kNumBytes == 32);
static_assert (std::is_trivially_copyable_v<EntityId>);
static_assert (std::is_trivially_copyable_v<AssetContentHash>);
static_assert (std::is_trivially_copyable_v<Asset>);
static_assert (std::is_trivially_copyable_v<Clip>);
static_assert (std::is_trivially_copyable_v<Note>);
static_assert (std::is_trivially_copyable_v<ProjectEditCommand>);

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

Track makeTrack (EntityId id, std::string name = "Audio 1")
{
    Track track;
    track.id = id;
    track.strip.name = std::move (name);
    return track;
}

Clip makeClip (EntityId id, EntityId assetId, EntityId trackId, std::uint64_t srcOffset, std::uint64_t srcLen)
{
    Clip clip;
    clip.id = id;
    clip.assetId = assetId;
    clip.trackId = trackId;
    clip.srcOffset = srcOffset;
    clip.srcLen = srcLen;
    clip.timelineLength = 15360;
    return clip;
}

Note makeNote (EntityId id, Tick start, Tick length)
{
    Note note;
    note.id = id;
    note.startTick = start;
    note.lengthTicks = length;
    note.key = 64;
    note.pitchNote = 64.25;
    note.normalizedVelocity = 0.5;
    note.portIndex = 2;
    note.channel = 3;
    return note;
}

MidiClip makeMidiClip (EntityId id, EntityId trackId)
{
    MidiClip midiClip;
    midiClip.id = id;
    midiClip.trackId = trackId;
    midiClip.timelineStart = 1024;
    midiClip.timelineLength = 4096;
    midiClip.timeBase = TimeBase::TempoLocked;
    midiClip.notes = {
        makeNote (idFromLowByte (42), 0, 512),
        makeNote (idFromLowByte (43), 4096, 0),
    };
    return midiClip;
}

Project makeEditableProject()
{
    const EntityId assetId = idFromLowByte (30);
    const EntityId trackId = idFromLowByte (36);

    Clip clip = makeClip (idFromLowByte (31), assetId, trackId, 100, 800);
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
    project.tracks = { makeTrack (trackId, "Audio 1") };
    project.clips = { clip };
    return project;
}

Project makeTwoClipEditableProject()
{
    Project project = makeEditableProject();

    Clip second = project.clips.front();
    second.id = idFromLowByte (33);
    second.timelineStart = 90'000;
    second.timelineLength = 30'000;
    second.srcOffset = 0;
    second.srcLen = 400;
    second.gain = 1.0f;
    second.fadeIn = 0;
    second.fadeOut = 0;

    project.clips.push_back (second);
    return project;
}

Project makeMidiEditableProject()
{
    Project project = makeEditableProject();
    project.tracks.push_back (makeTrack (idFromLowByte (41), "MIDI Track"));
    project.midiClips = { makeMidiClip (idFromLowByte (40), idFromLowByte (41)) };
    return project;
}

Project makeRecordingCompEditableProject()
{
    Project project = makeTwoClipEditableProject();
    project.assets.push_back (makeAsset (idFromLowByte (34), 1200));
    project.clips[1].assetId = project.assets[1].id;
    project.clips[1].srcLen = 800;
    project.clips[1].timelineLength = 50'000;

    RecordingTake firstTake;
    firstTake.id = idFromLowByte (50);
    firstTake.assetId = project.clips[0].assetId;
    firstTake.trackId = project.clips[0].trackId;
    firstTake.clipId = project.clips[0].id;
    firstTake.timelineStart = project.clips[0].timelineStart;
    firstTake.frameCount = project.clips[0].srcLen;
    firstTake.takeOrdinal = 0;
    firstTake.monitoringPolicy = RecordingMonitoringPolicy::DirectInput;

    RecordingTake secondTake = firstTake;
    secondTake.id = idFromLowByte (51);
    secondTake.assetId = project.clips[1].assetId;
    secondTake.clipId = project.clips[1].id;
    secondTake.timelineStart = project.clips[1].timelineStart;
    secondTake.takeOrdinal = 1;

    project.recordingTakes = { firstTake, secondTake };
    return project;
}

void requireProjectValueUnchanged (const Project& actual, const Project& expected)
{
    REQUIRE (actual.id == expected.id);
    REQUIRE (actual.sampleRate == expected.sampleRate);
    REQUIRE (actual.assets == expected.assets);
    REQUIRE (actual.tracks == expected.tracks);
    REQUIRE (actual.buses == expected.buses);
    REQUIRE (actual.clips == expected.clips);
    REQUIRE (actual.tempoMap == expected.tempoMap);
    REQUIRE (actual.meterMap == expected.meterMap);
    REQUIRE (actual.markers == expected.markers);
    REQUIRE (actual.midiClips == expected.midiClips);
    REQUIRE (actual.recordingTakes == expected.recordingTakes);
    REQUIRE (actual.recordingCompSegments == expected.recordingCompSegments);
}

enum class GeneratedUndoSequenceStepKind : std::uint8_t
{
    BeginGroup = 0,
    EndGroup,
    ApplyCommand
};

struct GeneratedUndoSequenceStep
{
    GeneratedUndoSequenceStepKind kind = GeneratedUndoSequenceStepKind::ApplyCommand;
    ProjectEditCommand command;
    ProjectEditStatus expectedEditStatus = ProjectEditStatus::Applied;
    bool expectedBoundaryResult = false;
    bool expectedRecorded = false;
    bool expectedCoalesced = false;
    bool expectedGroupOpen = false;
    std::size_t expectedUndoDepth = 0;
};

GeneratedUndoSequenceStep generatedBeginGroup (std::size_t expectedUndoDepth,
                                               bool expectedBoundaryResult = true,
                                               bool expectedGroupOpen = true)
{
    return {
        GeneratedUndoSequenceStepKind::BeginGroup,
        {},
        ProjectEditStatus::Applied,
        expectedBoundaryResult,
        false,
        false,
        expectedGroupOpen,
        expectedUndoDepth,
    };
}

GeneratedUndoSequenceStep generatedEndGroup (std::size_t expectedUndoDepth,
                                             bool expectedBoundaryResult = true,
                                             bool expectedGroupOpen = false)
{
    return {
        GeneratedUndoSequenceStepKind::EndGroup,
        {},
        ProjectEditStatus::Applied,
        expectedBoundaryResult,
        false,
        false,
        expectedGroupOpen,
        expectedUndoDepth,
    };
}

GeneratedUndoSequenceStep generatedApplyCommand (ProjectEditCommand command,
                                                 bool expectedGroupOpen,
                                                 std::size_t expectedUndoDepth,
                                                 bool expectedCoalesced = false,
                                                 ProjectEditStatus expectedEditStatus = ProjectEditStatus::Applied,
                                                 bool expectedRecorded = true)
{
    return {
        GeneratedUndoSequenceStepKind::ApplyCommand,
        command,
        expectedEditStatus,
        false,
        expectedRecorded,
        expectedCoalesced,
        expectedGroupOpen,
        expectedUndoDepth,
    };
}

std::array<GeneratedUndoSequenceStep, 21> generateProjectUndoEditSequence (EntityId firstId,
                                                                           EntityId secondId,
                                                                           EntityId rightId)
{
    return {
        generatedBeginGroup (0),
        generatedBeginGroup (0, false, true),
        generatedApplyCommand (ProjectEditCommand::moveClip (firstId, 1'000), true, 1),
        generatedApplyCommand (ProjectEditCommand::moveClip (firstId, 1'100), true, 1, true),
        generatedApplyCommand (ProjectEditCommand::setClipGain (firstId, 0.50f), true, 2),
        generatedApplyCommand (ProjectEditCommand::setClipGain (firstId, 0.875f), true, 2, true),
        generatedApplyCommand (ProjectEditCommand::setClipGain (firstId, -0.01f),
                               true,
                               2,
                               false,
                               ProjectEditStatus::InvalidClipEnvelope,
                               false),
        generatedEndGroup (2),
        generatedEndGroup (2, false, false),
        generatedApplyCommand (ProjectEditCommand::moveClip (firstId, 1'200), false, 3),
        generatedApplyCommand (ProjectEditCommand::moveClip (firstId, 1'300), false, 4),
        generatedBeginGroup (4),
        generatedApplyCommand (ProjectEditCommand::trimClip (secondId, 88'000, 22'000, 10, 350), true, 5),
        generatedApplyCommand (ProjectEditCommand::trimClip (secondId, 87'000, 21'000, 20, 340), true, 5, true),
        generatedApplyCommand (ProjectEditCommand::setClipFades (secondId, 10, 20), true, 6),
        generatedApplyCommand (ProjectEditCommand::setClipFades (secondId, 30, 40), true, 6, true),
        generatedApplyCommand (ProjectEditCommand::splitClip (firstId, rightId, 18'000, 300), true, 7),
        generatedApplyCommand (ProjectEditCommand::setClipGain (rightId, 0.25f), true, 8),
        generatedApplyCommand (ProjectEditCommand::setClipGain (rightId, 0.50f), true, 8, true),
        generatedApplyCommand (ProjectEditCommand::trimClip (secondId, 87'000, 1'000, 1'500, 1),
                               true,
                               8,
                               false,
                               ProjectEditStatus::InvalidSourceWindow,
                               false),
        generatedEndGroup (8),
    };
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
    const EntityId trackId = idFromLowByte (6);
    const Asset asset = makeAsset (assetId, 400);

    Clip clip = makeClip (idFromLowByte (2), assetId, trackId, 100, 300);
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

    const Clip unknownAsset = makeClip (idFromLowByte (3), idFromLowByte (99), trackId, 0, 1);
    REQUIRE_FALSE (unknownAsset.references (asset));
    REQUIRE_FALSE (unknownAsset.sourceWindowFits (asset));

    const Clip tooLong = makeClip (idFromLowByte (4), assetId, trackId, 100, 301);
    REQUIRE_FALSE (tooLong.sourceWindowFits (asset));

    const Clip overflow = makeClip (idFromLowByte (5), assetId, trackId, std::numeric_limits<std::uint64_t>::max(), 1);
    REQUIRE_FALSE (overflow.sourceWindowFits (Asset { assetId, {}, std::numeric_limits<std::uint64_t>::max(), SampleRate {}, 1 }));
}

TEST_CASE ("Project validates Asset to Clip indirection by EntityId", "[project][indirection]")
{
    const EntityId projectId = idFromLowByte (10);
    const EntityId assetA = idFromLowByte (11);
    const EntityId assetB = idFromLowByte (12);
    const EntityId trackId = idFromLowByte (13);

    Project project;
    project.id = projectId;
    project.assets = {
        makeAsset (assetA, 1000),
        makeAsset (assetB, 256),
    };
    project.tracks = { makeTrack (trackId, "Audio 1") };
    project.clips = {
        makeClip (idFromLowByte (20), assetA, trackId, 100, 900),
        makeClip (idFromLowByte (21), assetB, trackId, 0, 128),
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

    Project clipMatchesTrackId = project;
    clipMatchesTrackId.clips[1].id = project.tracks[0].id;
    REQUIRE_FALSE (clipMatchesTrackId.hasUniqueEntityIds());
    REQUIRE_FALSE (clipMatchesTrackId.hasValidAssetClipIndirection());

    Project invalidAsset = project;
    invalidAsset.assets[0].frames = 0;
    REQUIRE_FALSE (invalidAsset.assetsAreValid());
    REQUIRE_FALSE (invalidAsset.hasValidAssetClipIndirection());

    Project orphanClip = project;
    orphanClip.clips[0].assetId = idFromLowByte (99);
    REQUIRE_FALSE (orphanClip.clipsReferenceAssets());
    REQUIRE_FALSE (orphanClip.hasValidAssetClipIndirection());

    Project orphanTrack = project;
    orphanTrack.clips[0].trackId = idFromLowByte (99);
    REQUIRE_FALSE (orphanTrack.clipsReferenceTracks());
    REQUIRE_FALSE (orphanTrack.hasValidAssetClipIndirection());

    Project corruptSourceWindow = project;
    corruptSourceWindow.clips[1].srcOffset = 200;
    corruptSourceWindow.clips[1].srcLen = 100;
    REQUIRE_FALSE (corruptSourceWindow.clipsReferenceAssets());
    REQUIRE_FALSE (corruptSourceWindow.hasValidAssetClipIndirection());
}

TEST_CASE ("Project validates MIDI Clips and Note identity", "[project][midi]")
{
    Project project = makeEditableProject();
    project.tracks.push_back (makeTrack (idFromLowByte (41), "MIDI Track"));
    project.midiClips = { makeMidiClip (idFromLowByte (40), idFromLowByte (41)) };

    REQUIRE (project.midiClips.front().isValid());
    REQUIRE (project.hasValidAssetClipIndirection());

    Project duplicateMidiClipId = project;
    duplicateMidiClipId.midiClips.front().id = duplicateMidiClipId.clips.front().id;
    REQUIRE_FALSE (duplicateMidiClipId.hasUniqueEntityIds());
    REQUIRE_FALSE (duplicateMidiClipId.hasValidAssetClipIndirection());

    Project duplicateNoteId = project;
    duplicateNoteId.midiClips.front().notes[1].id = duplicateNoteId.midiClips.front().notes[0].id;
    REQUIRE_FALSE (duplicateNoteId.hasUniqueEntityIds());
    REQUIRE_FALSE (duplicateNoteId.hasValidAssetClipIndirection());

    Project noteExtendsPastClip = project;
    noteExtendsPastClip.midiClips.front().notes[0].startTick = 4090;
    noteExtendsPastClip.midiClips.front().notes[0].lengthTicks = 16;
    REQUIRE_FALSE (noteExtendsPastClip.midiClips.front().isValid());
    REQUIRE_FALSE (noteExtendsPastClip.hasValidAssetClipIndirection());

    Project invalidVoiceAddress = project;
    invalidVoiceAddress.midiClips.front().notes[0].channel = 16;
    REQUIRE_FALSE (invalidVoiceAddress.midiClips.front().notes[0].isValid());
    REQUIRE_FALSE (invalidVoiceAddress.hasValidAssetClipIndirection());

    Project missingTrackOwner = project;
    missingTrackOwner.midiClips.front().trackId = {};
    REQUIRE_FALSE (missingTrackOwner.midiClips.front().isValid());
    REQUIRE_FALSE (missingTrackOwner.hasValidAssetClipIndirection());
}

TEST_CASE ("Project Note edit operations mutate only targeted MIDI Note fields", "[project][midi][note-edit]")
{
    Project project = makeMidiEditableProject();
    REQUIRE (project.hasValidAssetClipIndirection());

    const Project original = project;
    const EntityId midiClipId = project.midiClips.front().id;
    const EntityId noteId = project.midiClips.front().notes.front().id;
    const EntityId rightNoteId = idFromLowByte (44);

    REQUIRE (moveNote (project, midiClipId, noteId, 256) == ProjectEditStatus::Applied);
    REQUIRE (setNoteLength (project, midiClipId, noteId, 768) == ProjectEditStatus::Applied);
    REQUIRE (quantizeNote (project, midiClipId, noteId, SnapGrid { 512 }) == ProjectEditStatus::Applied);
    REQUIRE (transposeNote (project, midiClipId, noteId, 7) == ProjectEditStatus::Applied);
    REQUIRE (splitNote (project, midiClipId, noteId, rightNoteId, 256) == ProjectEditStatus::Applied);
    REQUIRE (cutNote (project, midiClipId, rightNoteId) == ProjectEditStatus::Applied);

    REQUIRE (project.id == original.id);
    REQUIRE (project.sampleRate == original.sampleRate);
    REQUIRE (project.assets == original.assets);
    REQUIRE (project.clips == original.clips);
    REQUIRE (project.midiClips.size() == 1u);
    REQUIRE (project.midiClips.front().notes.size() == 2u);
    REQUIRE (project.hasValidAssetClipIndirection());

    const Note& edited = project.midiClips.front().notes[0];
    REQUIRE (edited.id == noteId);
    REQUIRE (edited.startTick == 512);
    REQUIRE (edited.lengthTicks == 256);
    REQUIRE (edited.key == 71);
    REQUIRE (edited.pitchNote == Approx (71.25));
    REQUIRE (edited.normalizedVelocity == Approx (0.5));
    REQUIRE (edited.portIndex == 2);
    REQUIRE (edited.channel == 3);
    REQUIRE (project.midiClips.front().notes[1] == original.midiClips.front().notes[1]);
}

TEST_CASE ("Project Note edit operations reject invalid input without mutating Project", "[project][midi][note-edit][invalid]")
{
    const Project project = makeMidiEditableProject();
    const EntityId midiClipId = project.midiClips.front().id;
    const EntityId noteId = project.midiClips.front().notes.front().id;
    const EntityId newNoteId = idFromLowByte (44);

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (moveNote (edited, {}, noteId, 128) == ProjectEditStatus::InvalidMidiClipId);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (moveNote (edited, idFromLowByte (99), noteId, 128) == ProjectEditStatus::MidiClipNotFound);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (setNoteLength (edited, midiClipId, {}, 128) == ProjectEditStatus::InvalidNoteId);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (setNoteLength (edited, midiClipId, idFromLowByte (99), 128) == ProjectEditStatus::NoteNotFound);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (moveNote (edited, midiClipId, noteId, 4096) == ProjectEditStatus::InvalidNoteWindow);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (setNoteLength (edited, midiClipId, noteId, 4097) == ProjectEditStatus::InvalidNoteWindow);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (splitNote (edited, midiClipId, noteId, project.assets.front().id, 128) == ProjectEditStatus::DuplicateEntityId);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (splitNote (edited, midiClipId, noteId, newNoteId, 0) == ProjectEditStatus::InvalidNoteWindow);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (splitNote (edited, midiClipId, noteId, newNoteId, project.midiClips.front().notes.front().lengthTicks)
                 == ProjectEditStatus::InvalidNoteWindow);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (cutNote (edited, midiClipId, idFromLowByte (99)) == ProjectEditStatus::NoteNotFound);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (quantizeNote (edited, midiClipId, noteId, SnapGrid { 0 }) == ProjectEditStatus::InvalidSnapGrid);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        edited.midiClips.front().notes.front().startTick = 3'584;
        edited.midiClips.front().notes.front().lengthTicks = 512;
        const Project before = edited;
        REQUIRE (quantizeNote (edited, midiClipId, noteId, SnapGrid { 4'096 }) == ProjectEditStatus::InvalidNoteWindow);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project edited = project;
        const Project before = edited;
        REQUIRE (transposeNote (edited, midiClipId, noteId, 64) == ProjectEditStatus::InvalidNoteValue);
        requireProjectValueUnchanged (edited, before);
    }

    {
        Project invalid = project;
        invalid.midiClips.front().notes.front().channel = 16;
        const Project before = invalid;
        REQUIRE (moveNote (invalid, midiClipId, noteId, 128) == ProjectEditStatus::InvalidProject);
        requireProjectValueUnchanged (invalid, before);
    }
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

TEST_CASE ("Project undo stack records command diffs for clip metadata edits", "[project][clip-edit][undo]")
{
    Project project = makeEditableProject();
    const Project original = project;
    const EntityId clipId = project.clips.front().id;
    const EntityId rightId = idFromLowByte (32);

    ProjectUndoStack undo;
    REQUIRE_FALSE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());
    REQUIRE (undo.undo (project) == ProjectUndoStatus::NothingToUndo);
    REQUIRE (undo.redo (project) == ProjectUndoStatus::NothingToRedo);

    auto result = undo.apply (project, ProjectEditCommand::moveClip (clipId, 1'024));
    REQUIRE (result.applied());
    REQUIRE (undo.undoDepth() == 1u);
    REQUIRE (undo.redoDepth() == 0u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::MoveClip);
    REQUIRE (undo.nextUndo()->diff.before.size() == 1u);
    REQUIRE (undo.nextUndo()->diff.after.size() == 1u);

    result = undo.apply (project, ProjectEditCommand::trimClip (clipId, 1'024, 20'000, 150, 600));
    REQUIRE (result.applied());

    result = undo.apply (project, ProjectEditCommand::setClipGain (clipId, 1.25f));
    REQUIRE (result.applied());

    result = undo.apply (project, ProjectEditCommand::setClipFades (clipId, 240, 480));
    REQUIRE (result.applied());

    result = undo.apply (project, ProjectEditCommand::splitClip (clipId, rightId, 8'000, 300));
    REQUIRE (result.applied());
    REQUIRE (undo.undoDepth() == 5u);
    REQUIRE_FALSE (undo.canRedo());
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::SplitClip);
    REQUIRE (undo.nextUndo()->diff.before.size() == 1u);
    REQUIRE (undo.nextUndo()->diff.after.size() == 2u);
    REQUIRE (undo.nextUndo()->diff.after[0].id == clipId);
    REQUIRE (undo.nextUndo()->diff.after[1].id == rightId);

    const Project edited = project;
    REQUIRE (edited.assets == original.assets);
    REQUIRE (edited.clips.size() == 2u);
    REQUIRE (edited.clips[0].id == clipId);
    REQUIRE (edited.clips[1].id == rightId);
    REQUIRE (edited.clips[0].timelineStart == 1'024);
    REQUIRE (edited.clips[0].timelineLength == 8'000);
    REQUIRE (edited.clips[1].timelineStart == 9'024);
    REQUIRE (edited.clips[0].srcOffset == 150u);
    REQUIRE (edited.clips[0].srcLen == 300u);
    REQUIRE (edited.clips[1].srcOffset == 450u);
    REQUIRE (edited.clips[0].gain == 1.25f);
    REQUIRE (edited.clips[1].gain == 1.25f);
    REQUIRE (edited.clips[0].fadeIn == 240);
    REQUIRE (edited.clips[1].fadeOut == 480);
    REQUIRE (edited.hasValidAssetClipIndirection());

    for (int i = 0; i < 5; ++i)
        REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, original);
    REQUIRE_FALSE (undo.canUndo());
    REQUIRE (undo.canRedo());
    REQUIRE (undo.redoDepth() == 5u);

    for (int i = 0; i < 5; ++i)
        REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, edited);
    REQUIRE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());
    REQUIRE (undo.redoDepth() == 0u);
}

TEST_CASE ("Project undo stack records command diffs for recording Comp selection", "[project][recording][comp][undo]")
{
    Project project = makeRecordingCompEditableProject();
    const Project original = project;
    const EntityId firstTakeId = project.recordingTakes[0].id;
    const EntityId secondTakeId = project.recordingTakes[1].id;

    ProjectUndoStack undo;
    const auto result = undo.apply (
        project,
        ProjectEditCommand::setRecordingCompSelection (
            idFromLowByte (60),
            firstTakeId,
            0,
            96,
            0,
            idFromLowByte (61),
            secondTakeId,
            160,
            96,
            0));

    REQUIRE (result.applied());
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::SetRecordingCompSelection);
    REQUIRE (undo.nextUndo()->recordingCompDiff.before.empty());
    REQUIRE (undo.nextUndo()->recordingCompDiff.after.size() == 2u);
    REQUIRE (project.recordingCompSegments.size() == 2u);
    REQUIRE (project.recordingCompSegments[0].takeId == firstTakeId);
    REQUIRE (project.recordingCompSegments[0].timelineStart == 0);
    REQUIRE (project.recordingCompSegments[0].timelineLength == 96);
    REQUIRE (project.recordingCompSegments[1].takeId == secondTakeId);
    REQUIRE (project.recordingCompSegments[1].timelineStart == 160);
    REQUIRE (project.recordingCompSegments[1].timelineLength == 96);
    REQUIRE (project.hasValidAssetClipIndirection());

    const Project edited = project;
    REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);
    requireProjectValueUnchanged (project, original);
    REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);
    requireProjectValueUnchanged (project, edited);
}

TEST_CASE ("Project undo stack records command diffs for MIDI Note edits", "[project][midi][note-edit][undo]")
{
    Project project = makeMidiEditableProject();
    const Project original = project;
    const EntityId midiClipId = project.midiClips.front().id;
    const EntityId noteId = project.midiClips.front().notes.front().id;
    const EntityId rightNoteId = idFromLowByte (44);

    ProjectUndoStack undo;

    auto result = undo.apply (project, ProjectEditCommand::moveNote (midiClipId, noteId, 256));
    REQUIRE (result.applied());
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::MoveNote);
    REQUIRE (undo.nextUndo()->midiDiff.before.size() == 1u);
    REQUIRE (undo.nextUndo()->midiDiff.after.size() == 1u);

    result = undo.apply (project, ProjectEditCommand::setNoteLength (midiClipId, noteId, 768));
    REQUIRE (result.applied());

    result = undo.apply (project, ProjectEditCommand::quantizeNote (midiClipId, noteId, SnapGrid { 512 }));
    REQUIRE (result.applied());

    result = undo.apply (project, ProjectEditCommand::transposeNote (midiClipId, noteId, 7));
    REQUIRE (result.applied());

    result = undo.apply (project, ProjectEditCommand::splitNote (midiClipId, noteId, rightNoteId, 256));
    REQUIRE (result.applied());

    result = undo.apply (project, ProjectEditCommand::cutNote (midiClipId, rightNoteId));
    REQUIRE (result.applied());
    REQUIRE (undo.undoDepth() == 6u);
    REQUIRE_FALSE (undo.canRedo());

    const Project edited = project;
    REQUIRE (edited.hasValidAssetClipIndirection());
    REQUIRE (edited.midiClips.front().notes.size() == 2u);
    REQUIRE (edited.midiClips.front().notes[0].startTick == 512);
    REQUIRE (edited.midiClips.front().notes[0].lengthTicks == 256);
    REQUIRE (edited.midiClips.front().notes[0].key == 71);
    REQUIRE (edited.midiClips.front().notes[0].pitchNote == Approx (71.25));

    for (int i = 0; i < 6; ++i)
        REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, original);
    REQUIRE_FALSE (undo.canUndo());
    REQUIRE (undo.redoDepth() == 6u);

    for (int i = 0; i < 6; ++i)
        REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, edited);
    REQUIRE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());
}

TEST_CASE ("Project undo stack groups compatible headless clip edit transactions", "[project][clip-edit][undo][group]")
{
    Project project = makeTwoClipEditableProject();
    REQUIRE (project.hasValidAssetClipIndirection());

    const Project original = project;
    const EntityId firstId = project.clips[0].id;
    const EntityId secondId = project.clips[1].id;
    const EntityId rightId = idFromLowByte (34);

    ProjectUndoStack undo;
    REQUIRE_FALSE (undo.transactionGroupOpen());
    REQUIRE (undo.beginTransactionGroup());
    REQUIRE (undo.transactionGroupOpen());
    REQUIRE_FALSE (undo.beginTransactionGroup());

    auto result = undo.apply (project, ProjectEditCommand::moveClip (firstId, 1'024));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 1u);

    result = undo.apply (project, ProjectEditCommand::moveClip (firstId, 2'048));
    REQUIRE (result.applied());
    REQUIRE (result.coalesced);
    REQUIRE (undo.undoDepth() == 1u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::MoveClip);
    REQUIRE (undo.nextUndo()->diff.before.size() == 1u);
    REQUIRE (undo.nextUndo()->diff.after.size() == 1u);
    REQUIRE (undo.nextUndo()->diff.before[0] == original.clips[0]);
    REQUIRE (undo.nextUndo()->diff.after[0] == project.clips[0]);

    result = undo.apply (project, ProjectEditCommand::setClipGain (firstId, 1.25f));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 2u);

    result = undo.apply (project, ProjectEditCommand::setClipGain (firstId, 0.75f));
    REQUIRE (result.applied());
    REQUIRE (result.coalesced);
    REQUIRE (undo.undoDepth() == 2u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::SetClipGain);
    REQUIRE (undo.nextUndo()->diff.before[0].gain == original.clips[0].gain);
    REQUIRE (undo.nextUndo()->diff.after[0].gain == 0.75f);

    result = undo.apply (project, ProjectEditCommand::moveClip (secondId, 10'000));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 3u);

    result = undo.apply (project, ProjectEditCommand::moveClip (secondId, 11'000));
    REQUIRE (result.applied());
    REQUIRE (result.coalesced);
    REQUIRE (undo.undoDepth() == 3u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::MoveClip);
    REQUIRE (undo.nextUndo()->diff.before[0] == original.clips[1]);
    REQUIRE (undo.nextUndo()->diff.after[0] == project.clips[1]);

    result = undo.apply (project, ProjectEditCommand::splitClip (firstId, rightId, 8'000, 300));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 4u);
    REQUIRE (project.clips.size() == 3u);
    REQUIRE (project.clips[0].id == firstId);
    REQUIRE (project.clips[1].id == rightId);
    REQUIRE (project.clips[1].srcOffset == project.clips[0].srcOffset + project.clips[0].srcLen);

    REQUIRE (undo.endTransactionGroup());
    REQUIRE_FALSE (undo.transactionGroupOpen());
    REQUIRE_FALSE (undo.endTransactionGroup());

    const Project edited = project;
    REQUIRE (edited.hasValidAssetClipIndirection());

    for (int i = 0; i < 4; ++i)
        REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, original);
    REQUIRE_FALSE (undo.canUndo());
    REQUIRE (undo.redoDepth() == 4u);

    for (int i = 0; i < 4; ++i)
        REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, edited);
    REQUIRE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());
}

TEST_CASE ("Project undo stack groups compatible MIDI Note move and length edits", "[project][midi][note-edit][undo][group]")
{
    Project project = makeMidiEditableProject();
    const Project original = project;
    const EntityId midiClipId = project.midiClips.front().id;
    const EntityId noteId = project.midiClips.front().notes.front().id;

    ProjectUndoStack undo;
    REQUIRE (undo.beginTransactionGroup());

    auto result = undo.apply (project, ProjectEditCommand::moveNote (midiClipId, noteId, 128));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 1u);

    result = undo.apply (project, ProjectEditCommand::moveNote (midiClipId, noteId, 256));
    REQUIRE (result.applied());
    REQUIRE (result.coalesced);
    REQUIRE (undo.undoDepth() == 1u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::MoveNote);
    REQUIRE (undo.nextUndo()->midiDiff.before[0] == original.midiClips[0]);
    REQUIRE (undo.nextUndo()->midiDiff.after[0] == project.midiClips[0]);

    const Project beforeLength = project;
    result = undo.apply (project, ProjectEditCommand::setNoteLength (midiClipId, noteId, 640));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 2u);

    result = undo.apply (project, ProjectEditCommand::setNoteLength (midiClipId, noteId, 768));
    REQUIRE (result.applied());
    REQUIRE (result.coalesced);
    REQUIRE (undo.undoDepth() == 2u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::SetNoteLength);
    REQUIRE (undo.nextUndo()->midiDiff.before[0] == beforeLength.midiClips[0]);
    REQUIRE (undo.nextUndo()->midiDiff.after[0] == project.midiClips[0]);

    result = undo.apply (project, ProjectEditCommand::transposeNote (midiClipId, noteId, 1));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 3u);

    result = undo.apply (project, ProjectEditCommand::transposeNote (midiClipId, noteId, 1));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 4u);

    REQUIRE (undo.endTransactionGroup());

    const Project edited = project;
    REQUIRE (edited.hasValidAssetClipIndirection());

    for (int i = 0; i < 4; ++i)
        REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, original);

    for (int i = 0; i < 4; ++i)
        REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, edited);
}

TEST_CASE ("Project undo stack separates targets and groups trim and fade edits", "[project][clip-edit][undo][group]")
{
    Project project = makeTwoClipEditableProject();
    REQUIRE (project.hasValidAssetClipIndirection());

    const Project original = project;
    const EntityId firstId = project.clips[0].id;
    const EntityId secondId = project.clips[1].id;

    ProjectUndoStack undo;
    REQUIRE (undo.beginTransactionGroup());

    auto result = undo.apply (project, ProjectEditCommand::moveClip (firstId, 1'000));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 1u);

    result = undo.apply (project, ProjectEditCommand::moveClip (secondId, 2'000));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 2u);

    result = undo.apply (project, ProjectEditCommand::moveClip (secondId, 3'000));
    REQUIRE (result.applied());
    REQUIRE (result.coalesced);
    REQUIRE (undo.undoDepth() == 2u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::MoveClip);
    REQUIRE (undo.nextUndo()->diff.before[0] == original.clips[1]);
    REQUIRE (undo.nextUndo()->diff.after[0] == project.clips[1]);

    const Project beforeTrim = project;
    result = undo.apply (project, ProjectEditCommand::trimClip (firstId, 1'500, 20'000, 150, 500));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 3u);

    result = undo.apply (project, ProjectEditCommand::trimClip (firstId, 1'750, 21'000, 160, 510));
    REQUIRE (result.applied());
    REQUIRE (result.coalesced);
    REQUIRE (undo.undoDepth() == 3u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::TrimClip);
    REQUIRE (undo.nextUndo()->diff.before[0] == beforeTrim.clips[0]);
    REQUIRE (undo.nextUndo()->diff.after[0] == project.clips[0]);

    const Project beforeFades = project;
    result = undo.apply (project, ProjectEditCommand::setClipFades (firstId, 120, 240));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 4u);

    result = undo.apply (project, ProjectEditCommand::setClipFades (firstId, 360, 480));
    REQUIRE (result.applied());
    REQUIRE (result.coalesced);
    REQUIRE (undo.undoDepth() == 4u);
    REQUIRE (undo.nextUndo() != nullptr);
    REQUIRE (undo.nextUndo()->command.verb == ProjectEditVerb::SetClipFades);
    REQUIRE (undo.nextUndo()->diff.before[0] == beforeFades.clips[0]);
    REQUIRE (undo.nextUndo()->diff.after[0] == project.clips[0]);

    REQUIRE (undo.endTransactionGroup());

    const Project edited = project;
    REQUIRE (edited.hasValidAssetClipIndirection());

    for (int i = 0; i < 4; ++i)
        REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, original);

    for (int i = 0; i < 4; ++i)
        REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);

    requireProjectValueUnchanged (project, edited);
}

TEST_CASE ("Project undo stack keeps compatible edits separate outside transaction groups", "[project][clip-edit][undo][group]")
{
    Project project = makeEditableProject();
    const Project original = project;
    const EntityId clipId = project.clips.front().id;

    ProjectUndoStack undo;
    auto result = undo.apply (project, ProjectEditCommand::moveClip (clipId, 1'024));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 1u);

    const Project afterFirstMove = project;
    result = undo.apply (project, ProjectEditCommand::moveClip (clipId, 2'048));
    REQUIRE (result.applied());
    REQUIRE_FALSE (result.coalesced);
    REQUIRE (undo.undoDepth() == 2u);

    const Project edited = project;
    REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);
    requireProjectValueUnchanged (project, afterFirstMove);

    REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);
    requireProjectValueUnchanged (project, original);

    REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);
    REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);
    requireProjectValueUnchanged (project, edited);
}

TEST_CASE ("Project undo stack rejects failed commands and mismatched live Project state", "[project][clip-edit][undo][invalid]")
{
    Project project = makeEditableProject();
    const Project original = project;
    const EntityId clipId = project.clips.front().id;

    ProjectUndoStack undo;
    const auto rejected = undo.apply (project, ProjectEditCommand::setClipGain (clipId, -0.01f));
    REQUIRE (rejected.editStatus == ProjectEditStatus::InvalidClipEnvelope);
    REQUIRE_FALSE (rejected.recorded);
    REQUIRE_FALSE (undo.canUndo());
    requireProjectValueUnchanged (project, original);

    REQUIRE (undo.apply (project, ProjectEditCommand::moveClip (clipId, 2'048)).applied());
    Project externallyChanged = project;
    externallyChanged.clips.front().timelineStart = 4'096;

    const Project beforeMismatch = externallyChanged;
    REQUIRE (undo.undo (externallyChanged) == ProjectUndoStatus::ProjectMismatch);
    requireProjectValueUnchanged (externallyChanged, beforeMismatch);
    REQUIRE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());
}

TEST_CASE ("Project generated edit sequence undo redo returns bit-identical Project values", "[project][clip-edit][undo][sequence]")
{
    Project project = makeTwoClipEditableProject();
    REQUIRE (project.hasValidAssetClipIndirection());

    const Project original = project;
    const EntityId firstId = project.clips[0].id;
    const EntityId secondId = project.clips[1].id;
    const EntityId rightId = idFromLowByte (35);

    ProjectUndoStack undo;
    const auto sequence = generateProjectUndoEditSequence (firstId, secondId, rightId);

    for (const GeneratedUndoSequenceStep& step : sequence)
    {
        const Project beforeStep = project;

        switch (step.kind)
        {
            case GeneratedUndoSequenceStepKind::BeginGroup:
                REQUIRE (undo.beginTransactionGroup() == step.expectedBoundaryResult);
                break;

            case GeneratedUndoSequenceStepKind::EndGroup:
                REQUIRE (undo.endTransactionGroup() == step.expectedBoundaryResult);
                break;

            case GeneratedUndoSequenceStepKind::ApplyCommand:
            {
                const auto result = undo.apply (project, step.command);
                REQUIRE (result.editStatus == step.expectedEditStatus);
                REQUIRE (result.recorded == step.expectedRecorded);
                REQUIRE (result.coalesced == step.expectedCoalesced);

                if (! step.expectedRecorded)
                    requireProjectValueUnchanged (project, beforeStep);
                else
                    REQUIRE (project.hasValidAssetClipIndirection());

                break;
            }
        }

        REQUIRE (undo.transactionGroupOpen() == step.expectedGroupOpen);
        REQUIRE (undo.undoDepth() == step.expectedUndoDepth);
        REQUIRE_FALSE (undo.canRedo());
    }

    REQUIRE_FALSE (undo.transactionGroupOpen());
    const Project edited = project;
    REQUIRE (edited.hasValidAssetClipIndirection());
    REQUIRE (edited.clips.size() == 3u);
    REQUIRE (edited.clips[0].id == firstId);
    REQUIRE (edited.clips[1].id == rightId);
    REQUIRE (edited.clips[2].id == secondId);
    REQUIRE (edited.clips[1].srcOffset == edited.clips[0].srcOffset + edited.clips[0].srcLen);

    std::size_t undoCount = 0;
    while (undo.canUndo())
    {
        REQUIRE (undo.undo (project) == ProjectUndoStatus::Applied);
        ++undoCount;
    }

    REQUIRE (undoCount == 8u);
    requireProjectValueUnchanged (project, original);
    REQUIRE_FALSE (undo.canUndo());
    REQUIRE (undo.canRedo());
    REQUIRE (undo.redoDepth() == 8u);

    std::size_t redoCount = 0;
    while (undo.canRedo())
    {
        REQUIRE (undo.redo (project) == ProjectUndoStatus::Applied);
        ++redoCount;
    }

    REQUIRE (redoCount == undoCount);
    requireProjectValueUnchanged (project, edited);
    REQUIRE (undo.canUndo());
    REQUIRE_FALSE (undo.canRedo());
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

// The actual H2 exit property — "any edit sequence + full undo returns the document bit-identical" — as a
// REAL randomized test over all clip AND note verbs, not the hand-coded 21-step array. Seeded for
// reproducibility; split-created IDs come from a fixed pool so the run is deterministic across machines.
TEST_CASE ("Randomized edit sequences fully undo to a bit-identical Project and redo back", "[project][undo][property]")
{
    Project project = makeTwoClipEditableProject();
    project.tracks.push_back (makeTrack (idFromLowByte (41), "MIDI Track"));
    project.midiClips = { makeMidiClip (idFromLowByte (40), idFromLowByte (41)) };

    const std::array<EntityId, 2> clipIds { idFromLowByte (31), idFromLowByte (33) };
    const EntityId                midiClipId = idFromLowByte (40);
    const std::array<EntityId, 2> noteIds { idFromLowByte (42), idFromLowByte (43) };

    std::mt19937 rng (0x00C0FFEEu);
    const auto pick = [&rng] (int n) { return static_cast<int> (rng() % static_cast<std::uint32_t> (n)); };
    const auto tick = [&rng] (Tick lo, Tick hi)
    {
        return lo + static_cast<Tick> (rng() % static_cast<std::uint32_t> (hi - lo + 1));
    };

    int freshLowByte = 100;   // pool for split-created clip/note IDs (kept < 250, never collides with base IDs)

    for (int rep = 0; rep < 12; ++rep)
    {
        const Project original = project;

        ProjectUndoStack undo;
        for (int step = 0; step < 60; ++step)
        {
            const EntityId clip = clipIds[static_cast<std::size_t> (pick (2))];
            const EntityId note = noteIds[static_cast<std::size_t> (pick (2))];

            ProjectEditCommand command = ProjectEditCommand::moveClip (clip, tick (0, 40'000));
            switch (pick (10))
            {
                case 0: command = ProjectEditCommand::moveClip (clip, tick (0, 40'000)); break;
                case 1: command = ProjectEditCommand::trimClip (clip, tick (0, 40'000), tick (1, 20'000),
                                                                tick (0, 800), tick (1, 400)); break;
                case 2: command = ProjectEditCommand::setClipGain (clip, static_cast<float> (pick (200)) / 100.0f); break;
                case 3: command = ProjectEditCommand::setClipFades (clip, tick (0, 256), tick (0, 256)); break;
                case 4:
                    if (freshLowByte < 250)
                        command = ProjectEditCommand::splitClip (clip, idFromLowByte (static_cast<std::uint8_t> (freshLowByte++)),
                                                                 tick (1, 20'000), tick (1, 400));
                    break;
                case 5: command = ProjectEditCommand::moveNote (midiClipId, note, tick (0, 8'000)); break;
                case 6: command = ProjectEditCommand::setNoteLength (midiClipId, note, tick (0, 8'000)); break;
                case 7: command = ProjectEditCommand::transposeNote (midiClipId, note, pick (49) - 24); break;
                case 8: command = ProjectEditCommand::quantizeNote (midiClipId, note,
                                                                    yesdaw::engine::SnapGrid { tick (1, 1'920) }); break;
                case 9:
                    if (freshLowByte < 250)
                        command = ProjectEditCommand::splitNote (midiClipId, note,
                                                                 idFromLowByte (static_cast<std::uint8_t> (freshLowByte++)),
                                                                 tick (1, 1'000));
                    break;
                default: break;
            }

            (void) undo.apply (project, command);   // invalid edits are rejected and leave the Project unchanged
        }

        const Project edited = project;
        REQUIRE (undo.undoDepth() > 10u);            // the sequence did meaningful work, not a no-op stack

        while (undo.canUndo())
            REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
        requireProjectValueUnchanged (project, original);   // full undo -> bit-identical original

        while (undo.canRedo())
            REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
        requireProjectValueUnchanged (project, edited);     // full redo -> bit-identical edited
    }
}
