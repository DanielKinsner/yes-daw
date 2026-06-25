// YES DAW - H3 mixer mute policy (ADR-0014).
//
// Control-thread-only: derive the post-compile mute mask from per-target mute / SIP-solo / solo-safe
// state, then publish it through the existing CompiledGraph mute seam (setMuted). The audio thread NEVER
// evaluates this policy - it only reads the already-published atomic mute mask, and changing solo/mute
// state never rewrites routing edges or recompiles the graph (ADR-0007 / ADR-0014).
//
// A mute "target" is a Track or a Bus Return, identified by the compiled Node whose ZEROED output
// silences that target's ENTIRE audible contribution:
//   - Track  -> its source Node. Zeroing the source removes the direct Fader/Pan/Meter path AND every
//               Send tap (Sends read the source, or the source-derived Fader), so a muted/solo-muted
//               Track leaks nothing into a Bus Return. This is what prevents a solo-safe reverb Return
//               from carrying non-soloed Tracks into the soloed mix (ADR-0014).
//   - Return -> its Bus SumNode. Zeroing it silences the whole Return chain into the master, regardless
//               of source solo state.
// Mapping a logical Track/Return to that exact compiled Node ID is mixer-projection work (ADR-0014); the
// policy here is pure over the flags plus that one Node ID per target.

#pragma once

#include "engine/CompiledGraph.h"

#include <span>

namespace yesdaw::engine {

struct MixerMuteTarget
{
    NodeId muteNodeId = 0;       // compiled Node whose zeroed output silences this target (see file header)
    bool   muted      = false;   // explicit Mute
    bool   soloed     = false;   // Solo engaged on this target
    bool   soloSafe   = false;   // exempt from solo-induced muting (never from explicit Mute)
};

// SIP solo is "active" iff some UNMUTED target is soloed. A target that is both muted and soloed does not
// engage solo for the others (it is silent anyway): "if no unmuted target is soloed, every unmuted target
// is audible" (ADR-0014).
[[nodiscard]] inline bool mixerAnyActiveSolo (std::span<const MixerMuteTarget> targets) noexcept
{
    for (const MixerMuteTarget& t : targets)
        if (t.soloed && ! t.muted)
            return true;
    return false;
}

// ADR-0014 effective-mute rule for one target:
//   - explicit Mute always wins;
//   - under active solo, only soloed or solo-safe targets stay audible;
//   - solo-safe only exempts from solo-induced muting, never from explicit Mute.
[[nodiscard]] inline bool mixerTargetIsEffectivelyMuted (const MixerMuteTarget& t, bool anyActiveSolo) noexcept
{
    return t.muted || (anyActiveSolo && ! t.soloed && ! t.soloSafe);
}

// Control thread: publish the effective mute mask for all targets.
//
// Fails (returns false and leaves the published mask UNCHANGED) if any target's Node is not a mute-capable
// compiled contribution point - ADR-0014 requires a self-asserting failure rather than silently leaving a
// target unmuted. On success every target's mute bit is set to its effective-mute decision and true is
// returned.
[[nodiscard]] inline bool applyMixerMutePolicy (CompiledGraph& graph, std::span<const MixerMuteTarget> targets)
{
    for (const MixerMuteTarget& t : targets)
        if (! graph.isMuteCapable (t.muteNodeId))
            return false;

    const bool anySolo = mixerAnyActiveSolo (targets);
    for (const MixerMuteTarget& t : targets)
        (void) graph.setMuted (t.muteNodeId, mixerTargetIsEffectivelyMuted (t, anySolo));

    return true;
}

} // namespace yesdaw::engine
