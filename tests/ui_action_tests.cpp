#include "ui/UiActions.h"
#include "ui/UiMixerSurface.h"
#include "ui/UiTimelineEdits.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <set>
#include <string_view>
#include <vector>

using yesdaw::ui::AccessibilityRole;
using yesdaw::engine::Asset;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::ProjectEditVerb;
using yesdaw::engine::ProjectMixerNodeRole;
using yesdaw::engine::ProjectUndoStatus;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::ui::KeymapRebindStatus;
using yesdaw::ui::UiActionContext;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiActionKind;
using yesdaw::ui::UiActionRegistry;
using yesdaw::ui::UiMixerActionPayload;
using yesdaw::ui::UiMixerActionStatus;
using yesdaw::ui::UiMixerBusControl;
using yesdaw::ui::UiMixerLoudnessReadout;
using yesdaw::ui::UiMixerMeterReadout;
using yesdaw::ui::UiMixerSurfaceModel;
using yesdaw::ui::UiMixerTargetControl;
using yesdaw::ui::UiPanel;
using yesdaw::ui::UiTimelineEditModel;
using yesdaw::ui::UiTimelineEditPayload;
using yesdaw::ui::descriptorForStableId;
using yesdaw::ui::kUiActionCount;
using yesdaw::ui::mainShellToolbarActions;
using yesdaw::engine::projectMixerNodeIdForClip;
using yesdaw::ui::roleName;
using yesdaw::ui::uiActionDescriptors;

namespace {

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

Project makeTimelineEditProject()
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };

    Asset asset;
    asset.id = idFromLowByte (2);
    asset.frames = 1024;
    asset.sampleRate = project.sampleRate;
    asset.channels = 1;

    Clip clip;
    clip.id = idFromLowByte (3);
    clip.assetId = asset.id;
    clip.timelineStart = 0;
    clip.timelineLength = 8192;
    clip.srcOffset = 0;
    clip.srcLen = 512;
    clip.gain = 1.0f;
    clip.fadeIn = 0;
    clip.fadeOut = 0;
    clip.timeBase = TimeBase::SampleLocked;

    project.assets = { asset };
    project.clips = { clip };
    return project;
}

Project makeMixerProject()
{
    Project project = makeTimelineEditProject();

    Asset secondAsset = project.assets.front();
    secondAsset.id = idFromLowByte (5);
    project.assets.push_back (secondAsset);

    Clip secondClip = project.clips.front();
    secondClip.id = idFromLowByte (6);
    secondClip.assetId = secondAsset.id;
    secondClip.timelineStart = 8192;
    secondClip.gain = 0.5f;
    project.clips.push_back (secondClip);

    return project;
}

} // namespace

TEST_CASE ("H11 action registry exposes stable action ids, labels, keys, and accessible names",
           "[ui][actions]")
{
    const auto& actions = uiActionDescriptors();
    REQUIRE (actions.size() == kUiActionCount);

    REQUIRE (static_cast<int> (UiActionId::ProjectNew) == 0);
    REQUIRE (actions[0].stableId == std::string_view ("project.new"));
    REQUIRE (actions[1].stableId == std::string_view ("project.open"));
    REQUIRE (actions[3].stableId == std::string_view ("transport.play"));
    REQUIRE (descriptorForStableId ("timeline.clip.move")->id == UiActionId::TimelineClipMove);
    REQUIRE (descriptorForStableId ("timeline.clip.time_stretch")->id == UiActionId::TimelineClipTimeStretch);
    REQUIRE (descriptorForStableId ("mixer.target.set_fader")->id == UiActionId::MixerTargetSetFader);
    REQUIRE (descriptorForStableId ("mixer.meters.read")->id == UiActionId::MixerReadMeters);
    REQUIRE (descriptorForStableId ("mixer.loudness.read")->id == UiActionId::MixerReadLoudness);
    REQUIRE (descriptorForStableId ("help.show_keymap")->id == UiActionId::HelpShowKeymap);

    std::set<std::string_view> stableIds;
    std::set<std::string_view> defaultKeys;
    for (const auto& action : actions)
    {
        REQUIRE (action.stableId != nullptr);
        REQUIRE (std::string_view (action.stableId).find ('.') != std::string_view::npos);
        REQUIRE (stableIds.insert (action.stableId).second);
        REQUIRE (action.label != nullptr);
        REQUIRE_FALSE (std::string_view (action.label).empty());
        REQUIRE (action.defaultKey != nullptr);
        REQUIRE_FALSE (std::string_view (action.defaultKey).empty());
        REQUIRE (defaultKeys.insert (action.defaultKey).second);
        REQUIRE (action.accessibleName != nullptr);
        REQUIRE_FALSE (std::string_view (action.accessibleName).empty());
        REQUIRE_FALSE (std::string_view (roleName (action.accessibleRole)).empty());
        REQUIRE ((action.kind == UiActionKind::Command
                  || action.kind == UiActionKind::Toggle
                  || action.kind == UiActionKind::Query));
    }

    REQUIRE (descriptorForStableId ("transport.toggle_loop")->id == UiActionId::TransportToggleLoop);
    REQUIRE (descriptorForStableId ("missing.action") == nullptr);
}

TEST_CASE ("H11 default shell toolbar actions all resolve through the registry", "[ui][actions]")
{
    const UiActionRegistry registry;
    std::set<UiActionId> seen;

    for (UiActionId id : mainShellToolbarActions())
    {
        const auto* descriptor = registry.descriptor (id);
        REQUIRE (descriptor != nullptr);
        REQUIRE (seen.insert (id).second);
        REQUIRE_FALSE (std::string_view (descriptor->stableId).empty());
        REQUIRE_FALSE (std::string_view (descriptor->label).empty());
        REQUIRE (registry.keymap().actionForChord (descriptor->defaultKey) == id);
    }
}

TEST_CASE ("H11 keymap remapping is stable and rejects duplicate or empty chords", "[ui][keymap]")
{
    UiActionRegistry registry;

    REQUIRE (registry.keymap().actionForChord ("Space") == UiActionId::TransportPlay);
    REQUIRE (registry.keymap().rebind (UiActionId::TransportPlay, "P") == KeymapRebindStatus::Ok);
    REQUIRE (registry.keymap().actionForChord ("P") == UiActionId::TransportPlay);
    REQUIRE (registry.keymap().actionForChord ("Space") == UiActionId::Count);

    REQUIRE (registry.keymap().rebind (UiActionId::TransportStop, "P")
             == KeymapRebindStatus::DuplicateChord);
    REQUIRE (registry.keymap().chordFor (UiActionId::TransportStop) == "K");

    REQUIRE (registry.keymap().rebind (UiActionId::TransportStop, "")
             == KeymapRebindStatus::EmptyChord);
    REQUIRE (registry.keymap().rebind (UiActionId::Count, "X")
             == KeymapRebindStatus::UnknownAction);
}

TEST_CASE ("H11 action enabled state explains disabled project, undo, and redo commands",
           "[ui][actions]")
{
    const UiActionRegistry registry;
    UiActionContext context;

    REQUIRE (registry.stateFor (UiActionId::ProjectOpen, context).enabled);

    const auto playBeforeProject = registry.stateFor (UiActionId::TransportPlay, context);
    REQUIRE_FALSE (playBeforeProject.enabled);
    REQUIRE (playBeforeProject.disabledReason == std::string_view ("no project loaded"));

    context.projectLoaded = true;
    REQUIRE (registry.stateFor (UiActionId::TransportPlay, context).enabled);

    const auto undoWithoutStack = registry.stateFor (UiActionId::EditUndo, context);
    REQUIRE_FALSE (undoWithoutStack.enabled);
    REQUIRE (undoWithoutStack.disabledReason == std::string_view ("nothing to undo"));

    context.canUndo = true;
    REQUIRE (registry.stateFor (UiActionId::EditUndo, context).enabled);

    const auto redoWithoutStack = registry.stateFor (UiActionId::EditRedo, context);
    REQUIRE_FALSE (redoWithoutStack.enabled);
    REQUIRE (redoWithoutStack.disabledReason == std::string_view ("nothing to redo"));

    const auto clipMoveNoSelection = registry.stateFor (UiActionId::TimelineClipMove, context);
    REQUIRE_FALSE (clipMoveNoSelection.enabled);
    REQUIRE (clipMoveNoSelection.disabledReason == std::string_view ("no clip selected"));

    context.timelineClipSelected = true;
    REQUIRE (registry.stateFor (UiActionId::TimelineClipMove, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::TimelineClipTrim, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::TimelineClipSplit, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::TimelineClipSetGain, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::TimelineClipSetFades, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::TimelineClipTimeStretch, context).enabled);

    const auto faderWithoutTarget = registry.stateFor (UiActionId::MixerTargetSetFader, context);
    REQUIRE_FALSE (faderWithoutTarget.enabled);
    REQUIRE (faderWithoutTarget.disabledReason == std::string_view ("no mixer target selected"));

    context.mixerTargetSelected = true;
    REQUIRE (registry.stateFor (UiActionId::MixerTargetSetFader, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::MixerTargetSetPan, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::MixerTargetToggleMute, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::MixerTargetToggleSolo, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::MixerReadMeters, context).enabled);
    REQUIRE (registry.stateFor (UiActionId::MixerReadLoudness, context).enabled);
}

TEST_CASE ("H11 action dispatch mutates only the headless app model behind action ids",
           "[ui][actions]")
{
    const UiActionRegistry registry;
    UiActionContext context;

    const auto disabledPlay = registry.dispatch (UiActionId::TransportPlay, context);
    REQUIRE_FALSE (disabledPlay.dispatched);
    REQUIRE_FALSE (context.isPlaying);
    REQUIRE (context.commandDispatchCount == 0);

    REQUIRE (registry.dispatch (UiActionId::ProjectNew, context).dispatched);
    REQUIRE (context.projectLoaded);
    REQUIRE (context.activePanel == UiPanel::Timeline);
    REQUIRE (context.commandDispatchCount == 1);

    REQUIRE (registry.dispatch (UiActionId::TransportPlay, context).dispatched);
    REQUIRE (context.isPlaying);
    REQUIRE (registry.dispatch (UiActionId::TransportStop, context).dispatched);
    REQUIRE_FALSE (context.isPlaying);

    context.playheadFrame = 4096;
    REQUIRE (registry.dispatch (UiActionId::TransportLocateStart, context).dispatched);
    REQUIRE (context.playheadFrame == 0);

    REQUIRE (registry.dispatch (UiActionId::TransportToggleLoop, context).dispatched);
    REQUIRE (context.loopEnabled);

    context.canUndo = true;
    REQUIRE (registry.dispatch (UiActionId::EditUndo, context).dispatched);
    REQUIRE (context.undoCount == 1);
    REQUIRE_FALSE (context.canUndo);
    REQUIRE (context.canRedo);
    REQUIRE (registry.dispatch (UiActionId::EditRedo, context).dispatched);
    REQUIRE (context.redoCount == 1);
    REQUIRE (context.canUndo);
    REQUIRE_FALSE (context.canRedo);

    REQUIRE (registry.dispatch (UiActionId::ViewMixer, context).dispatched);
    REQUIRE (context.activePanel == UiPanel::Mixer);
    REQUIRE (registry.dispatch (UiActionId::ViewPianoRoll, context).dispatched);
    REQUIRE (context.activePanel == UiPanel::PianoRoll);

    context.timelineClipSelected = true;
    REQUIRE (registry.dispatch (UiActionId::TimelineClipMove, context).dispatched);
    REQUIRE (context.activePanel == UiPanel::Timeline);
    REQUIRE (context.timelineEditCount == 1);
    REQUIRE (context.canUndo);
    REQUIRE_FALSE (context.canRedo);

    context.mixerTargetSelected = true;
    REQUIRE (registry.dispatch (UiActionId::MixerTargetSetFader, context).dispatched);
    REQUIRE (context.activePanel == UiPanel::Mixer);
    REQUIRE (context.mixerEditCount == 1);
    REQUIRE (registry.dispatch (UiActionId::MixerReadMeters, context).dispatched);
    REQUIRE (registry.dispatch (UiActionId::MixerReadLoudness, context).dispatched);
    REQUIRE (context.mixerReadCount == 2);

    REQUIRE (registry.dispatch (UiActionId::HelpShowKeymap, context).dispatched);
    REQUIRE (context.keymapVisible);
}

TEST_CASE ("H11 timeline edit actions dispatch to Project edit commands and undo",
           "[ui][actions][timeline-edit]")
{
    const EntityId clipId = idFromLowByte (3);
    const EntityId rightClipId = idFromLowByte (4);

    UiTimelineEditModel emptyModel;
    const auto noProject = emptyModel.dispatch (
        UiActionId::TimelineClipMove,
        UiTimelineEditPayload::moveTo (clipId, 256));
    REQUIRE_FALSE (noProject.dispatched);
    REQUIRE (noProject.state.disabledReason == std::string_view ("no project loaded"));

    UiTimelineEditModel model (makeTimelineEditProject());
    const auto noClipSelected = model.dispatch (
        UiActionId::TimelineClipMove,
        UiTimelineEditPayload::moveTo (clipId, 256));
    REQUIRE_FALSE (noClipSelected.dispatched);
    REQUIRE (noClipSelected.state.disabledReason == std::string_view ("no clip selected"));

    REQUIRE (model.selectClip (clipId));
    REQUIRE (model.registry().stateFor (UiActionId::TimelineClipMove, model.context()).enabled);

    auto result = model.dispatch (
        UiActionId::TimelineClipMove,
        UiTimelineEditPayload::moveTo (clipId, 256));
    REQUIRE (result.dispatched);
    REQUIRE (result.recorded);
    REQUIRE (model.lastAppliedCommand() != nullptr);
    REQUIRE (model.lastAppliedCommand()->verb == ProjectEditVerb::MoveClip);
    REQUIRE (model.project().clips[0].timelineStart == 256);
    REQUIRE (model.context().canUndo);

    result = model.dispatch (
        UiActionId::TimelineClipTrim,
        UiTimelineEditPayload::trimTo (clipId, 512, 4096, 16, 256));
    REQUIRE (result.dispatched);
    REQUIRE (model.lastAppliedCommand()->verb == ProjectEditVerb::TrimClip);
    REQUIRE (model.project().clips[0].timelineStart == 512);
    REQUIRE (model.project().clips[0].timelineLength == 4096);
    REQUIRE (model.project().clips[0].srcOffset == 16u);
    REQUIRE (model.project().clips[0].srcLen == 256u);

    result = model.dispatch (
        UiActionId::TimelineClipSetGain,
        UiTimelineEditPayload::setGain (clipId, 0.75f));
    REQUIRE (result.dispatched);
    REQUIRE (model.lastAppliedCommand()->verb == ProjectEditVerb::SetClipGain);
    REQUIRE (model.project().clips[0].gain == 0.75f);

    result = model.dispatch (
        UiActionId::TimelineClipSetFades,
        UiTimelineEditPayload::setFades (clipId, 120, 240));
    REQUIRE (result.dispatched);
    REQUIRE (model.lastAppliedCommand()->verb == ProjectEditVerb::SetClipFades);
    REQUIRE (model.project().clips[0].fadeIn == 120);
    REQUIRE (model.project().clips[0].fadeOut == 240);

    result = model.dispatch (
        UiActionId::TimelineClipSplit,
        UiTimelineEditPayload::splitAt (clipId, rightClipId, 2048, 128));
    REQUIRE (result.dispatched);
    REQUIRE (model.lastAppliedCommand()->verb == ProjectEditVerb::SplitClip);
    REQUIRE (model.project().clips.size() == 2u);
    REQUIRE (model.project().clips[0].timelineLength == 2048);
    REQUIRE (model.project().clips[0].srcLen == 128u);
    REQUIRE (model.project().clips[1].id == rightClipId);

    result = model.dispatch (
        UiActionId::TimelineClipTimeStretch,
        UiTimelineEditPayload::timeStretchToLength (clipId, 3072));
    REQUIRE (result.dispatched);
    REQUIRE (model.lastAppliedCommand()->verb == ProjectEditVerb::TrimClip);
    REQUIRE (model.project().clips[0].timelineLength == 3072);
    REQUIRE (model.project().clips[0].srcLen == 128u);
    REQUIRE (model.context().timelineEditCount == 6);

    const std::size_t undoDepthBeforeInvalid = model.undoStack().undoDepth();
    const float gainBeforeInvalid = model.project().clips[0].gain;
    result = model.dispatch (
        UiActionId::TimelineClipSetGain,
        UiTimelineEditPayload::setGain (clipId, -0.1f));
    REQUIRE_FALSE (result.dispatched);
    REQUIRE (result.editStatus == ProjectEditStatus::InvalidClipEnvelope);
    REQUIRE (model.undoStack().undoDepth() == undoDepthBeforeInvalid);
    REQUIRE (model.project().clips[0].gain == gainBeforeInvalid);

    result = model.dispatch (UiActionId::EditUndo);
    REQUIRE (result.dispatched);
    REQUIRE (result.undoStatus == ProjectUndoStatus::Applied);
    REQUIRE (model.project().clips[0].timelineLength == 2048);
    REQUIRE (model.context().canRedo);

    result = model.dispatch (UiActionId::EditRedo);
    REQUIRE (result.dispatched);
    REQUIRE (result.undoStatus == ProjectUndoStatus::Applied);
    REQUIRE (model.project().clips[0].timelineLength == 3072);
    REQUIRE_FALSE (model.context().canRedo);
}

TEST_CASE ("H11 mixer actions project fader pan mute solo meters and loudness to the UI surface",
           "[ui][actions][mixer]")
{
    UiMixerSurfaceModel emptyModel;
    const auto noProject = emptyModel.dispatch (UiActionId::MixerReadMeters);
    REQUIRE_FALSE (noProject.dispatch.dispatched);
    REQUIRE (noProject.dispatch.state.disabledReason == std::string_view ("no project loaded"));

    Project project = makeMixerProject();
    const EntityId firstClipId = project.clips[0].id;
    const EntityId secondClipId = project.clips[1].id;
    const float originalFirstGain = project.clips[0].gain;

    std::vector<UiMixerTargetControl> trackControls {
        UiMixerTargetControl {
            firstClipId,
            "Vocal Lead",
            0.9f,
            0.25f,
            false,
            false,
            false,
            true,
            UiMixerMeterReadout { 0.8f, 0.7f, 0.4f, 0.3f, true }
        },
        UiMixerTargetControl {
            secondClipId,
            "Bass DI",
            0.5f,
            -0.1f,
            false,
            false,
            false,
            false,
            UiMixerMeterReadout { 0.45f, 0.4f, 0.2f, 0.18f, true }
        }
    };

    std::vector<UiMixerBusControl> buses {
        UiMixerBusControl {
            "Room Verb",
            7001,
            7002,
            7003,
            7000,
            0.65f,
            0.15f,
            false,
            false,
            true,
            false,
            UiMixerMeterReadout { 0.3f, 0.35f, 0.12f, 0.14f, true }
        }
    };

    const UiMixerLoudnessReadout loudness { -14.2, -12.8, -13.4, 4.1, -1.1, true };
    UiMixerSurfaceModel model (project, trackControls, buses, loudness);

    auto snapshot = model.snapshot();
    REQUIRE (snapshot.projectLoaded);
    REQUIRE (snapshot.tracks.size() == 2u);
    REQUIRE (snapshot.buses.size() == 1u);
    REQUIRE (snapshot.tracks[0].name == "Vocal Lead");
    REQUIRE (snapshot.tracks[0].sourceNodeId == projectMixerNodeIdForClip (firstClipId, ProjectMixerNodeRole::Source));
    REQUIRE (snapshot.tracks[0].faderNodeId == projectMixerNodeIdForClip (firstClipId, ProjectMixerNodeRole::Fader));
    REQUIRE (snapshot.tracks[0].panNodeId == projectMixerNodeIdForClip (firstClipId, ProjectMixerNodeRole::Pan));
    REQUIRE (snapshot.tracks[0].meterNodeId == projectMixerNodeIdForClip (firstClipId, ProjectMixerNodeRole::Meter));
    REQUIRE (snapshot.tracks[0].linearGain == 0.9f);
    REQUIRE (snapshot.tracks[0].pan == 0.25f);
    REQUIRE (snapshot.tracks[0].sidechainVisible);
    REQUIRE (snapshot.tracks[0].meter.peakLeft == 0.8f);
    REQUIRE (snapshot.buses[0].linearGain == 0.65f);
    REQUIRE (snapshot.buses[0].soloSafe);
    REQUIRE (snapshot.loudness.valid);
    REQUIRE (snapshot.loudness.integratedLufs == -14.2);

    const auto noSelection = model.dispatch (UiActionId::MixerTargetSetFader, UiMixerActionPayload::setFader (0.8f));
    REQUIRE_FALSE (noSelection.dispatch.dispatched);
    REQUIRE (noSelection.dispatch.state.disabledReason == std::string_view ("no mixer target selected"));

    REQUIRE (model.selectTrack (0));
    auto result = model.dispatch (UiActionId::MixerTargetSetFader, UiMixerActionPayload::setFader (0.72f));
    REQUIRE (result.dispatch.dispatched);
    REQUIRE (result.mixerStatus == UiMixerActionStatus::Ok);
    result = model.dispatch (UiActionId::MixerTargetSetPan, UiMixerActionPayload::setPan (-0.35f));
    REQUIRE (result.dispatch.dispatched);
    result = model.dispatch (UiActionId::MixerTargetToggleSolo);
    REQUIRE (result.dispatch.dispatched);

    snapshot = model.snapshot();
    REQUIRE (snapshot.tracks[0].linearGain == 0.72f);
    REQUIRE (snapshot.tracks[0].pan == -0.35f);
    REQUIRE (snapshot.tracks[0].soloed);
    REQUIRE_FALSE (snapshot.tracks[0].effectivelyMuted);
    REQUIRE (snapshot.tracks[1].effectivelyMuted);
    REQUIRE_FALSE (snapshot.buses[0].effectivelyMuted);
    REQUIRE (model.project().clips[0].gain == originalFirstGain);

    result = model.dispatch (UiActionId::MixerTargetSetFader, UiMixerActionPayload::setFader (-0.1f));
    REQUIRE_FALSE (result.dispatch.dispatched);
    REQUIRE (result.mixerStatus == UiMixerActionStatus::InvalidValue);
    result = model.dispatch (UiActionId::MixerTargetSetPan, UiMixerActionPayload::setPan (1.25f));
    REQUIRE_FALSE (result.dispatch.dispatched);
    REQUIRE (result.mixerStatus == UiMixerActionStatus::InvalidValue);

    REQUIRE (model.selectBus (0));
    result = model.dispatch (UiActionId::MixerTargetToggleMute);
    REQUIRE (result.dispatch.dispatched);
    result = model.dispatch (UiActionId::MixerTargetSetFader, UiMixerActionPayload::setFader (0.5f));
    REQUIRE (result.dispatch.dispatched);

    snapshot = model.snapshot();
    REQUIRE (snapshot.buses[0].muted);
    REQUIRE (snapshot.buses[0].effectivelyMuted);
    REQUIRE (snapshot.buses[0].linearGain == 0.5f);
    REQUIRE (model.context().mixerEditCount == 5);

    result = model.dispatch (UiActionId::MixerReadMeters);
    REQUIRE (result.dispatch.dispatched);
    result = model.dispatch (UiActionId::MixerReadLoudness);
    REQUIRE (result.dispatch.dispatched);
    REQUIRE (model.context().mixerReadCount == 2);
}
