// YES DAW - H11 mixer/meter/loudness UI surface.
//
// Pure control-side projection: the UI surfaces saved Track/Bus strip controls, meter values, and
// loudness readings without changing the H3 mixer policy.

#pragma once

#include "engine/MixerGraphProjection.h"
#include "engine/MixerMutePolicy.h"
#include "engine/ProjectMixerProjection.h"
#include "ui/UiActions.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace yesdaw::ui {

enum class UiMixerTargetKind : std::uint8_t
{
    Track,
    Bus
};

enum class UiMixerActionStatus : std::uint8_t
{
    Ok,
    InvalidAction,
    InvalidTarget,
    InvalidValue
};

struct UiMixerTargetSelection
{
    UiMixerTargetKind kind = UiMixerTargetKind::Track;
    std::size_t index = 0;
};

struct UiMixerMeterReadout
{
    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    float rmsLeft = 0.0f;
    float rmsRight = 0.0f;
    bool valid = false;
};

struct UiMixerLoudnessReadout
{
    double integratedLufs = 0.0;
    double momentaryLufs = 0.0;
    double shortTermLufs = 0.0;
    double loudnessRangeLu = 0.0;
    double truePeakDbtp = 0.0;
    bool valid = false;
};

struct UiMixerSendReadout
{
    std::uint32_t sendOrdinal = 0;
    engine::NodeId faderNodeId = 0;
    double normalizedLevel = 0.0;
    std::size_t breakpointCount = 0;
    bool automated = false;
    // M5: the send ROW itself (ADR-0044), so a strip can paint what it actually routes. Before M5
    // this readout was built from automation lanes alone, so a plain send was invisible.
    engine::EntityId sendId {};
    engine::EntityId busId {};
    std::string busName;
    bool preFader = false;
    float linearGain = 1.0f;
};

struct UiMixerFxSlotReadout
{
    std::uint32_t slotOrdinal = 0;
    engine::EntityId insertId {};
    engine::NodeId fxNodeId = 0;
    engine::FxKind kind = engine::FxKind::Eq;
    bool enabled = true;
    std::size_t parameterCount = 0;
    bool gainReductionAvailable = false;
    float gainReductionDb = 0.0f;
    bool gainReductionValid = false;
};

struct UiMixerFxGainReductionReadout
{
    engine::EntityId insertId {};
    engine::NodeId fxNodeId = 0;
    float gainReductionDb = 0.0f;
    bool valid = false;
};

struct UiMixerTargetControl
{
    engine::EntityId targetId {};
    std::string name;
    float linearGain = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;
    bool soloSafe = false;
    bool sidechainVisible = false;
    UiMixerMeterReadout meter;
    std::vector<UiMixerFxGainReductionReadout> fxGainReductionReadouts;
};

struct UiMixerBusControl
{
    std::string name;
    engine::NodeId faderNodeId = 0;
    engine::NodeId panNodeId = 0;
    engine::NodeId meterNodeId = 0;
    engine::NodeId muteNodeId = 0;
    float linearGain = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;
    bool soloSafe = true;
    bool sidechainVisible = false;
    UiMixerMeterReadout meter;
};

struct UiMixerStrip
{
    UiMixerTargetKind kind = UiMixerTargetKind::Track;
    std::size_t index = 0;
    engine::EntityId clipId {};
    engine::EntityId targetId {};
    std::string name;
    engine::NodeId sourceNodeId = 0;
    engine::NodeId faderNodeId = 0;
    engine::NodeId panNodeId = 0;
    engine::NodeId meterNodeId = 0;
    engine::NodeId muteNodeId = 0;
    float linearGain = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;
    bool soloSafe = false;
    bool effectivelyMuted = false;
    bool sidechainVisible = false;
    UiMixerMeterReadout meter;
    std::vector<UiMixerSendReadout> sends;
    std::vector<UiMixerFxSlotReadout> fxSlots;
    // N7: the owning Track's persisted colour (kTrackColourUnset for buses — Bus carries no
    // colour field, so a bus strip always falls back to the historical index-based tint).
    std::uint32_t colour = 0;
    // G4.1: the strip's output route (an invalid id = Master) — the OUTPUT slot paints it.
    engine::EntityId outputBusId {};
};

struct UiMixerSurfaceSnapshot
{
    bool projectLoaded = false;
    std::vector<UiMixerStrip> tracks;
    std::vector<UiMixerStrip> buses;
    UiMixerLoudnessReadout loudness;
};

struct UiMixerActionPayload
{
    float linearGain = 1.0f;
    float pan = 0.0f;

    static UiMixerActionPayload setFader (float value) noexcept
    {
        UiMixerActionPayload payload;
        payload.linearGain = value;
        return payload;
    }

    static UiMixerActionPayload setPan (float value) noexcept
    {
        UiMixerActionPayload payload;
        payload.pan = value;
        return payload;
    }
};

struct UiMixerActionResult
{
    UiActionDispatchResult dispatch;
    UiMixerActionStatus mixerStatus = UiMixerActionStatus::Ok;
};

namespace detail {

inline const UiMixerTargetControl* findControlForTarget (std::span<const UiMixerTargetControl> controls,
                                                         engine::EntityId targetId) noexcept
{
    for (const UiMixerTargetControl& control : controls)
        if (control.targetId == targetId)
            return &control;
    return nullptr;
}

inline const engine::Clip* findFirstClipForTrack (const engine::Project& project, engine::EntityId trackId) noexcept
{
    for (const engine::Clip& clip : project.clips)
        if (clip.trackId == trackId)
            return &clip;

    return nullptr;
}

inline bool validLinearGain (float gain) noexcept
{
    return engine::mixerGainIsValid (gain);
}

inline bool validPan (float pan) noexcept
{
    return engine::mixerPanIsValid (pan);
}

inline std::string fallbackTrackName (std::size_t index)
{
    return "Track " + std::to_string (index + 1u);
}

inline std::string fallbackBusName (std::size_t index)
{
    return "Bus " + std::to_string (index + 1u);
}

// M5: the strip's REAL send rows (ADR-0044 persists them on the owner), with any automation lane
// for that ordinal merged in. The old law walked the lanes instead, so a send you added but never
// automated did not exist as far as the mixer surface was concerned. R13: the owner may be a
// Track or a Bus — the id law and the ordinal law are owner-generic.
inline std::vector<UiMixerSendReadout> sendReadoutsForTrack (const engine::Project& project,
                                                             engine::EntityId trackId)
{
    std::vector<UiMixerSendReadout> sends;

    const std::vector<engine::SendRow>* ownerSends = nullptr;
    if (const engine::Track* const track = project.findTrack (trackId))
        ownerSends = &track->sends;
    else if (const engine::Bus* const bus = project.findBus (trackId))
        ownerSends = &bus->sends;
    if (ownerSends == nullptr)
        return sends;

    sends.reserve (ownerSends->size());
    for (std::size_t ordinal = 0; ordinal < ownerSends->size(); ++ordinal)
    {
        const engine::SendRow& send = (*ownerSends)[ordinal];

        UiMixerSendReadout readout;
        readout.sendOrdinal = static_cast<std::uint32_t> (ordinal);
        readout.faderNodeId = engine::projectMixerSendLevelNodeIdForTrack (
            trackId, static_cast<std::uint32_t> (ordinal));
        readout.sendId = send.id;
        readout.busId = send.busId;
        readout.preFader = send.tap == engine::SendTap::PreFader;
        readout.linearGain = send.linearGain;
        readout.normalizedLevel = static_cast<double> (send.linearGain);
        if (const engine::Bus* const bus = project.findBus (send.busId))
            readout.busName = bus->strip.name;

        for (const engine::AutomationLaneData& lane : project.automationLanes)
        {
            if (! (lane.ownerEntity == trackId)
                || lane.role != engine::AutomationTargetRole::SendLevel
                || lane.paramId != readout.sendOrdinal)
                continue;

            readout.breakpointCount = lane.points.size();
            readout.automated = ! lane.points.empty();
            if (readout.automated)
                readout.normalizedLevel = lane.points.back().value;
        }

        sends.push_back (std::move (readout));
    }

    return sends;
}

inline std::vector<UiMixerFxSlotReadout> fxSlotReadoutsForStrip (const engine::MixerStripState& strip)
{
    std::vector<UiMixerFxSlotReadout> slots;
    slots.reserve (strip.fxChain.size());

    for (std::size_t i = 0; i < strip.fxChain.size(); ++i)
    {
        const engine::FxInsert& insert = strip.fxChain[i];

        UiMixerFxSlotReadout readout;
        readout.slotOrdinal = static_cast<std::uint32_t> (i);
        readout.insertId = insert.id;
        readout.fxNodeId = engine::projectMixerNodeIdForEntity (insert.id, engine::ProjectMixerNodeRole::Fx);
        readout.kind = insert.kind;
        readout.enabled = insert.enabled;
        readout.parameterCount = insert.normalizedParams.size();
        readout.gainReductionAvailable = insert.kind == engine::FxKind::Compressor
                                       || insert.kind == engine::FxKind::Limiter;
        slots.push_back (readout);
    }

    return slots;
}

inline const UiMixerFxGainReductionReadout* findFxGainReductionReadout (
    std::span<const UiMixerFxGainReductionReadout> readouts,
    engine::EntityId insertId,
    engine::NodeId fxNodeId) noexcept
{
    for (const UiMixerFxGainReductionReadout& readout : readouts)
    {
        if (readout.insertId == insertId)
            return &readout;
        if (readout.fxNodeId != 0 && readout.fxNodeId == fxNodeId)
            return &readout;
    }

    return nullptr;
}

inline std::vector<UiMixerFxSlotReadout> fxSlotReadoutsForStrip (
    const engine::MixerStripState& strip,
    std::span<const UiMixerFxGainReductionReadout> gainReductionReadouts)
{
    std::vector<UiMixerFxSlotReadout> slots = fxSlotReadoutsForStrip (strip);

    for (UiMixerFxSlotReadout& slot : slots)
    {
        const UiMixerFxGainReductionReadout* const readout =
            findFxGainReductionReadout (gainReductionReadouts, slot.insertId, slot.fxNodeId);
        if (readout == nullptr)
            continue;

        slot.gainReductionDb = readout->gainReductionDb;
        slot.gainReductionValid = readout->valid;
    }

    return slots;
}

inline void applyEffectiveMute (UiMixerSurfaceSnapshot& snapshot)
{
    std::vector<engine::MixerMuteTarget> targets;
    targets.reserve (snapshot.tracks.size() + snapshot.buses.size());

    for (const UiMixerStrip& track : snapshot.tracks)
        targets.push_back ({ track.muteNodeId, track.muted, track.soloed, track.soloSafe });
    for (const UiMixerStrip& bus : snapshot.buses)
        targets.push_back ({ bus.muteNodeId, bus.muted, bus.soloed, bus.soloSafe });

    const bool anySolo = engine::mixerAnyActiveSolo (std::span<const engine::MixerMuteTarget> (targets.data(), targets.size()));
    std::size_t targetIndex = 0;

    for (UiMixerStrip& track : snapshot.tracks)
        track.effectivelyMuted = engine::mixerTargetIsEffectivelyMuted (targets[targetIndex++], anySolo);
    for (UiMixerStrip& bus : snapshot.buses)
        bus.effectivelyMuted = engine::mixerTargetIsEffectivelyMuted (targets[targetIndex++], anySolo);
}

} // namespace detail

inline UiMixerSurfaceSnapshot projectUiMixerSurface (const engine::Project& project,
                                                     std::span<const UiMixerTargetControl> trackControls = {},
                                                     std::span<const UiMixerBusControl> busControls = {},
                                                     UiMixerLoudnessReadout loudness = {})
{
    UiMixerSurfaceSnapshot snapshot;
    snapshot.projectLoaded = project.hasValidAssetClipIndirection();
    snapshot.loudness = loudness;

    if (! snapshot.projectLoaded)
        return snapshot;

    snapshot.tracks.reserve (project.tracks.size());
    for (std::size_t i = 0; i < project.tracks.size(); ++i)
    {
        const engine::Track& trackRow = project.tracks[i];
        const engine::Clip* const firstClip = detail::findFirstClipForTrack (project, trackRow.id);
        const UiMixerTargetControl* const control = detail::findControlForTarget (trackControls, trackRow.id);

        UiMixerStrip strip;
        strip.kind = UiMixerTargetKind::Track;
        strip.index = i;
        strip.clipId = firstClip != nullptr ? firstClip->id : engine::EntityId {};
        strip.targetId = trackRow.id;
        strip.name = control != nullptr && ! control->name.empty()
            ? control->name
            : (! trackRow.strip.name.empty() ? trackRow.strip.name : detail::fallbackTrackName (i));
        strip.sourceNodeId = engine::projectMixerNodeIdForTrack (trackRow.id, engine::ProjectMixerNodeRole::Source);
        strip.faderNodeId = engine::projectMixerNodeIdForTrack (trackRow.id, engine::ProjectMixerNodeRole::Fader);
        strip.panNodeId = engine::projectMixerNodeIdForTrack (trackRow.id, engine::ProjectMixerNodeRole::Pan);
        strip.meterNodeId = engine::projectMixerNodeIdForTrack (trackRow.id, engine::ProjectMixerNodeRole::Meter);
        strip.muteNodeId = strip.sourceNodeId;
        strip.linearGain = control != nullptr ? control->linearGain : trackRow.strip.linearGain;
        strip.pan = control != nullptr ? control->pan : trackRow.strip.pan;
        strip.muted = control != nullptr ? control->muted : trackRow.strip.muted;
        strip.soloed = control != nullptr ? control->soloed : trackRow.strip.soloed;
        strip.soloSafe = control != nullptr ? control->soloSafe : trackRow.strip.soloSafe;
        strip.sidechainVisible = control != nullptr && control->sidechainVisible;
        strip.colour = trackRow.colour;
        strip.outputBusId = trackRow.outputBusId;   // G4.1
        strip.meter = control != nullptr ? control->meter : UiMixerMeterReadout {};
        strip.sends = detail::sendReadoutsForTrack (project, trackRow.id);
        strip.fxSlots = detail::fxSlotReadoutsForStrip (
            trackRow.strip,
            control != nullptr
                ? std::span<const UiMixerFxGainReductionReadout> (control->fxGainReductionReadouts.data(),
                                                                  control->fxGainReductionReadouts.size())
                : std::span<const UiMixerFxGainReductionReadout> {});
        snapshot.tracks.push_back (std::move (strip));
    }

    const std::size_t busCount = std::max (project.buses.size(), busControls.size());
    snapshot.buses.reserve (busCount);
    for (std::size_t i = 0; i < busCount; ++i)
    {
        const UiMixerBusControl* const control = i < busControls.size() ? &busControls[i] : nullptr;
        const engine::Bus* const bus = i < project.buses.size() ? &project.buses[i] : nullptr;

        UiMixerStrip strip;
        strip.kind = UiMixerTargetKind::Bus;
        strip.index = i;
        strip.targetId = bus != nullptr ? bus->id : engine::EntityId {};
        strip.name = control != nullptr && ! control->name.empty()
            ? control->name
            : (bus != nullptr && ! bus->strip.name.empty() ? bus->strip.name : detail::fallbackBusName (i));
        strip.faderNodeId = control != nullptr ? control->faderNodeId : 0;
        strip.panNodeId = control != nullptr ? control->panNodeId : 0;
        strip.meterNodeId = control != nullptr ? control->meterNodeId : 0;
        strip.muteNodeId = control != nullptr && control->muteNodeId != 0 ? control->muteNodeId : strip.faderNodeId;
        strip.linearGain = control != nullptr ? control->linearGain : (bus != nullptr ? bus->strip.linearGain : 1.0f);
        strip.pan = control != nullptr ? control->pan : (bus != nullptr ? bus->strip.pan : 0.0f);
        strip.muted = control != nullptr ? control->muted : (bus != nullptr && bus->strip.muted);
        strip.soloed = control != nullptr ? control->soloed : (bus != nullptr && bus->strip.soloed);
        strip.soloSafe = control != nullptr ? control->soloSafe : (bus != nullptr && bus->strip.soloSafe);
        strip.sidechainVisible = control != nullptr && control->sidechainVisible;
        strip.meter = control != nullptr ? control->meter : UiMixerMeterReadout {};
        if (bus != nullptr)
        {
            strip.outputBusId = bus->outputBusId;   // G4.1
            strip.fxSlots = detail::fxSlotReadoutsForStrip (bus->strip);
            // R13: bus strips paint their own send rows through the same owner-generic law.
            strip.sends = detail::sendReadoutsForTrack (project, bus->id);
        }
        snapshot.buses.push_back (std::move (strip));
    }

    detail::applyEffectiveMute (snapshot);
    return snapshot;
}

class UiMixerSurfaceModel
{
public:
    UiMixerSurfaceModel() = default;

    explicit UiMixerSurfaceModel (engine::Project project,
                                  std::vector<UiMixerTargetControl> trackControls = {},
                                  std::vector<UiMixerBusControl> busControls = {},
                                  UiMixerLoudnessReadout loudness = {})
        : project_ (std::move (project)),
          trackControls_ (std::move (trackControls)),
          busControls_ (std::move (busControls)),
          loudness_ (loudness)
    {
        context_.projectLoaded = project_.hasValidAssetClipIndirection();
        context_.activePanel = UiPanel::Mixer;
    }

    [[nodiscard]] const UiActionRegistry& registry() const noexcept { return registry_; }
    [[nodiscard]] const UiActionContext& context() const noexcept { return context_; }
    [[nodiscard]] const engine::Project& project() const noexcept { return project_; }

    [[nodiscard]] UiMixerSurfaceSnapshot snapshot() const
    {
        return projectUiMixerSurface (project_,
                                      std::span<const UiMixerTargetControl> (trackControls_.data(), trackControls_.size()),
                                      std::span<const UiMixerBusControl> (busControls_.data(), busControls_.size()),
                                      loudness_);
    }

    [[nodiscard]] bool selectTrack (std::size_t index) noexcept
    {
        if (index >= project_.tracks.size())
            return false;

        selected_ = { UiMixerTargetKind::Track, index };
        context_.mixerTargetSelected = true;
        return true;
    }

    [[nodiscard]] bool selectBus (std::size_t index) noexcept
    {
        if (index >= busControls_.size())
            return false;

        selected_ = { UiMixerTargetKind::Bus, index };
        context_.mixerTargetSelected = true;
        return true;
    }

    [[nodiscard]] UiMixerActionResult dispatch (UiActionId id, UiMixerActionPayload payload = {})
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { { id, state, false }, UiMixerActionStatus::Ok };

        switch (id)
        {
            case UiActionId::MixerTargetSetFader:
                if (! detail::validLinearGain (payload.linearGain))
                    return { { id, { true, "" }, false }, UiMixerActionStatus::InvalidValue };
                if (! applyToSelected ([&] (auto& control) { control.linearGain = payload.linearGain; }))
                    return { { id, { true, "" }, false }, UiMixerActionStatus::InvalidTarget };
                break;

            case UiActionId::MixerTargetSetPan:
                if (! detail::validPan (payload.pan))
                    return { { id, { true, "" }, false }, UiMixerActionStatus::InvalidValue };
                if (! applyToSelected ([&] (auto& control) { control.pan = payload.pan; }))
                    return { { id, { true, "" }, false }, UiMixerActionStatus::InvalidTarget };
                break;

            case UiActionId::MixerTargetToggleMute:
                if (! applyToSelected ([] (auto& control) { control.muted = ! control.muted; }))
                    return { { id, { true, "" }, false }, UiMixerActionStatus::InvalidTarget };
                break;

            case UiActionId::MixerTargetToggleSolo:
                if (! applyToSelected ([] (auto& control) { control.soloed = ! control.soloed; }))
                    return { { id, { true, "" }, false }, UiMixerActionStatus::InvalidTarget };
                break;

            case UiActionId::MixerReadMeters:
            case UiActionId::MixerReadLoudness:
            case UiActionId::MixerReadSends:
            case UiActionId::MixerReadFxSlots:
            case UiActionId::MixerReadGainReduction:
            case UiActionId::MixerReadBusFxSlots:
                return { registry_.dispatch (id, context_), UiMixerActionStatus::Ok };

            default:
                return { { id, { true, "" }, false }, UiMixerActionStatus::InvalidAction };
        }

        context_.activePanel = UiPanel::Mixer;
        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { { id, state, true }, UiMixerActionStatus::Ok };
    }

private:
    UiMixerTargetControl& trackControlForSelected()
    {
        for (UiMixerTargetControl& control : trackControls_)
            if (control.targetId == project_.tracks[selected_.index].id)
                return control;

        UiMixerTargetControl control;
        control.targetId = project_.tracks[selected_.index].id;
        control.name = project_.tracks[selected_.index].strip.name;
        control.linearGain = project_.tracks[selected_.index].strip.linearGain;
        control.pan = project_.tracks[selected_.index].strip.pan;
        control.muted = project_.tracks[selected_.index].strip.muted;
        control.soloed = project_.tracks[selected_.index].strip.soloed;
        control.soloSafe = project_.tracks[selected_.index].strip.soloSafe;
        trackControls_.push_back (std::move (control));
        return trackControls_.back();
    }

    template <typename Fn>
    bool applyToSelected (Fn&& fn)
    {
        if (! context_.mixerTargetSelected)
            return false;

        if (selected_.kind == UiMixerTargetKind::Track)
        {
            if (selected_.index >= project_.tracks.size())
                return false;

            fn (trackControlForSelected());
            return true;
        }

        if (selected_.index >= busControls_.size())
            return false;

        fn (busControls_[selected_.index]);
        return true;
    }

    UiActionRegistry registry_;
    UiActionContext context_;
    engine::Project project_;
    std::vector<UiMixerTargetControl> trackControls_;
    std::vector<UiMixerBusControl> busControls_;
    UiMixerLoudnessReadout loudness_;
    UiMixerTargetSelection selected_ {};
};

} // namespace yesdaw::ui
