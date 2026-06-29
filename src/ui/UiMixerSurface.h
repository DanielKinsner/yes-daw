// YES DAW - H11 mixer/meter/loudness UI surface.
//
// Pure control-side projection: the UI can surface mixer controls, meter values, and loudness readings
// without adding a new Project schema or changing the H3 mixer policy.

#pragma once

#include "engine/MixerGraphProjection.h"
#include "engine/MixerMutePolicy.h"
#include "engine/ProjectMixerProjection.h"
#include "ui/UiActions.h"

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

struct UiMixerTargetControl
{
    engine::EntityId clipId {};
    std::string name;
    float linearGain = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;
    bool soloSafe = false;
    bool sidechainVisible = false;
    UiMixerMeterReadout meter;
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

inline const UiMixerTargetControl* findControlForClip (std::span<const UiMixerTargetControl> controls,
                                                       engine::EntityId clipId) noexcept
{
    for (const UiMixerTargetControl& control : controls)
        if (control.clipId == clipId)
            return &control;
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

    snapshot.tracks.reserve (project.clips.size());
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const engine::Clip& clip = project.clips[i];
        const UiMixerTargetControl* const control = detail::findControlForClip (trackControls, clip.id);

        UiMixerStrip strip;
        strip.kind = UiMixerTargetKind::Track;
        strip.index = i;
        strip.clipId = clip.id;
        strip.name = control != nullptr && ! control->name.empty() ? control->name : detail::fallbackTrackName (i);
        strip.sourceNodeId = engine::projectMixerNodeIdForClip (clip.id, engine::ProjectMixerNodeRole::Source);
        strip.faderNodeId = engine::projectMixerNodeIdForClip (clip.id, engine::ProjectMixerNodeRole::Fader);
        strip.panNodeId = engine::projectMixerNodeIdForClip (clip.id, engine::ProjectMixerNodeRole::Pan);
        strip.meterNodeId = engine::projectMixerNodeIdForClip (clip.id, engine::ProjectMixerNodeRole::Meter);
        strip.muteNodeId = strip.sourceNodeId;
        strip.linearGain = control != nullptr ? control->linearGain : clip.gain;
        strip.pan = control != nullptr ? control->pan : 0.0f;
        strip.muted = control != nullptr && control->muted;
        strip.soloed = control != nullptr && control->soloed;
        strip.soloSafe = control != nullptr && control->soloSafe;
        strip.sidechainVisible = control != nullptr && control->sidechainVisible;
        strip.meter = control != nullptr ? control->meter : UiMixerMeterReadout {};
        snapshot.tracks.push_back (std::move (strip));
    }

    snapshot.buses.reserve (busControls.size());
    for (std::size_t i = 0; i < busControls.size(); ++i)
    {
        const UiMixerBusControl& control = busControls[i];

        UiMixerStrip strip;
        strip.kind = UiMixerTargetKind::Bus;
        strip.index = i;
        strip.name = ! control.name.empty() ? control.name : detail::fallbackBusName (i);
        strip.faderNodeId = control.faderNodeId;
        strip.panNodeId = control.panNodeId;
        strip.meterNodeId = control.meterNodeId;
        strip.muteNodeId = control.muteNodeId != 0 ? control.muteNodeId : control.faderNodeId;
        strip.linearGain = control.linearGain;
        strip.pan = control.pan;
        strip.muted = control.muted;
        strip.soloed = control.soloed;
        strip.soloSafe = control.soloSafe;
        strip.sidechainVisible = control.sidechainVisible;
        strip.meter = control.meter;
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
        if (index >= project_.clips.size())
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
        const engine::Clip& clip = project_.clips[selected_.index];
        for (UiMixerTargetControl& control : trackControls_)
            if (control.clipId == clip.id)
                return control;

        UiMixerTargetControl control;
        control.clipId = clip.id;
        control.linearGain = clip.gain;
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
            if (selected_.index >= project_.clips.size())
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
