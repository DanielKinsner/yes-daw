// YES DAW - H3 mixer mute policy checks (ADR-0014).
//
// Two layers:
//   1. Pure rule tests over MixerMuteTarget flags - the ADR-0014 effective-mute truth table
//      (explicit Mute wins; SIP solo mutes non-soloed/non-solo-safe; solo-safe never overrides Mute).
//   2. Integration tests over a real built mixer projection - muting a Track's source removes its direct
//      path AND its Send contribution, SIP solo mutes the rest, a solo-safe Return stays audible without
//      leaking non-soloed Tracks, and a non-mute-capable target fails self-assertingly.

#include "engine/MixerMutePolicy.h"
#include "engine/MixerGraphProjection.h"
#include "engine/nodes/IdentityDcNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

using Catch::Approx;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::GraphId;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MixerBusProjection;
using yesdaw::engine::MixerMuteTarget;
using yesdaw::engine::MixerProjectionError;
using yesdaw::engine::MixerProjectionInputs;
using yesdaw::engine::MixerSendProjection;
using yesdaw::engine::MixerSendTap;
using yesdaw::engine::MixerTrackProjection;
using yesdaw::engine::NodeId;
using yesdaw::engine::applyMixerMutePolicy;
using yesdaw::engine::buildMixerGraphProjection;
using yesdaw::engine::mixerAnyActiveSolo;
using yesdaw::engine::mixerTargetIsEffectivelyMuted;

namespace {

constexpr NodeId kMasterSumId = 71000;
constexpr NodeId kMasterId    = 71001;
constexpr int    kBlock       = 256;
constexpr float  kCenterGain  = 0.70710677f;

struct StereoCapture
{
    std::vector<float> left;
    std::vector<float> right;
};

StereoCapture render (CompiledGraph& graph, int frames)
{
    StereoCapture cap;
    cap.left.assign (static_cast<std::size_t> (frames), -999.0f);
    graph.process (cap.left.data(), frames);

    cap.right.assign (static_cast<std::size_t> (frames), 0.0f);
    if (const float* const r = graph.debugMasterChannel (1))
        for (int i = 0; i < frames; ++i)
            cap.right[static_cast<std::size_t> (i)] = r[i];

    return cap;
}

MixerProjectionInputs baseProjection (GraphId graphId)
{
    MixerProjectionInputs inputs;
    inputs.id = graphId;
    inputs.masterSumNodeId = kMasterSumId;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = 48000.0;
    inputs.maxBlockSize = kBlock;
    return inputs;
}

MixerTrackProjection makeTrack (NodeId sourceId, float dc, NodeId faderId, NodeId panId, NodeId meterId, float gain)
{
    MixerTrackProjection track;
    track.source = std::make_unique<IdentityDcNode> (sourceId, dc, 1);
    track.faderNodeId = faderId;
    track.panNodeId = panId;
    track.meterNodeId = meterId;
    track.linearGain = gain;
    track.pan = 0.0f;
    return track;
}

MixerMuteTarget muteTarget (NodeId nodeId, bool muted, bool soloed, bool soloSafe)
{
    return MixerMuteTarget { nodeId, muted, soloed, soloSafe };
}

} // namespace

// ----------------------------------------------------------------------------------------------------
// 1. Pure rule tests (ADR-0014 effective-mute truth table).
// ----------------------------------------------------------------------------------------------------

TEST_CASE ("Mixer mute policy: with no solo every unmuted target is audible", "[mixer][mute][policy]")
{
    const std::array<MixerMuteTarget, 3> targets {
        muteTarget (1, false, false, false),
        muteTarget (2, false, false, false),
        muteTarget (3, true, false, false),   // explicit mute
    };

    REQUIRE_FALSE (mixerAnyActiveSolo (targets));
    REQUIRE_FALSE (mixerTargetIsEffectivelyMuted (targets[0], false));
    REQUIRE_FALSE (mixerTargetIsEffectivelyMuted (targets[1], false));
    REQUIRE (mixerTargetIsEffectivelyMuted (targets[2], false));   // explicit mute wins with no solo
}

TEST_CASE ("Mixer mute policy: a muted-and-soloed target does not engage solo for the others", "[mixer][mute][policy][solo]")
{
    const std::array<MixerMuteTarget, 2> targets {
        muteTarget (1, true, true, false),    // muted AND soloed -> does not count as active solo
        muteTarget (2, false, false, false),
    };

    REQUIRE_FALSE (mixerAnyActiveSolo (targets));
    REQUIRE (mixerTargetIsEffectivelyMuted (targets[0], false));        // explicit mute wins
    REQUIRE_FALSE (mixerTargetIsEffectivelyMuted (targets[1], false));  // others stay audible
}

TEST_CASE ("Mixer mute policy: SIP solo mutes non-soloed non-solo-safe targets only", "[mixer][mute][policy][solo]")
{
    const std::array<MixerMuteTarget, 4> targets {
        muteTarget (1, false, true, false),    // soloed
        muteTarget (2, false, false, false),   // plain -> solo-muted
        muteTarget (3, false, false, true),    // solo-safe -> stays audible
        muteTarget (4, true, false, true),     // solo-safe BUT explicitly muted -> muted
    };

    REQUIRE (mixerAnyActiveSolo (targets));
    REQUIRE_FALSE (mixerTargetIsEffectivelyMuted (targets[0], true));   // soloed stays audible
    REQUIRE (mixerTargetIsEffectivelyMuted (targets[1], true));         // solo-muted
    REQUIRE_FALSE (mixerTargetIsEffectivelyMuted (targets[2], true));   // solo-safe exempt from solo mute
    REQUIRE (mixerTargetIsEffectivelyMuted (targets[3], true));         // explicit mute beats solo-safe
}

// ----------------------------------------------------------------------------------------------------
// 2. Integration tests over a real built mixer projection.
// ----------------------------------------------------------------------------------------------------

TEST_CASE ("Mixer mute policy: muting a Track silences its direct path in both channels", "[mixer][mute][policy][render]")
{
    constexpr NodeId kSrc = 1100;

    MixerProjectionInputs inputs = baseProjection (1);
    inputs.tracks.push_back (makeTrack (kSrc, 0.5f, 1200, 1300, 1400, 1.0f));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);

    // Audible before muting: centre-panned 0.5 in both channels.
    StereoCapture out = render (*graph, kBlock);
    REQUIRE (out.left.back() == Approx (0.5f * kCenterGain).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (0.5f * kCenterGain).margin (1.0e-4f));

    const std::array<MixerMuteTarget, 1> targets { muteTarget (kSrc, true, false, false) };
    REQUIRE (applyMixerMutePolicy (*graph, targets));
    REQUIRE (graph->isMuted (kSrc));

    out = render (*graph, kBlock);
    for (float v : out.left)
        REQUIRE (v == 0.0f);
    for (float v : out.right)
        REQUIRE (v == 0.0f);
}

TEST_CASE ("Mixer mute policy: muting a Track removes its Send contribution from the Return", "[mixer][mute][policy][send]")
{
    constexpr NodeId kSrcA = 2100;   // muted
    constexpr NodeId kSrcB = 2110;   // audible

    MixerProjectionInputs inputs = baseProjection (2);
    inputs.buses.push_back (MixerBusProjection { 2200, 2201, 2202 });

    // Direct paths silenced (gain 0) so only the bus Return reaches the master; both send pre-fader.
    MixerTrackProjection a = makeTrack (kSrcA, 1.0f, 2120, 2130, 2140, 0.0f);
    a.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    MixerTrackProjection b = makeTrack (kSrcB, 2.0f, 2121, 2131, 2141, 0.0f);
    b.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    inputs.tracks.push_back (std::move (a));
    inputs.tracks.push_back (std::move (b));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);

    // Before muting: the Return carries both sends, (1.0 + 2.0) widened to centre.
    StereoCapture out = render (*graph, kBlock);
    REQUIRE (out.left.back() == Approx (3.0f * kCenterGain).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (3.0f * kCenterGain).margin (1.0e-4f));

    // Mute Track A at its source: its Send tap reads the zeroed source, so only B's send survives.
    const std::array<MixerMuteTarget, 2> targets {
        muteTarget (kSrcA, true, false, false),
        muteTarget (kSrcB, false, false, false),
    };
    REQUIRE (applyMixerMutePolicy (*graph, targets));

    out = render (*graph, kBlock);
    REQUIRE (out.left.back() == Approx (2.0f * kCenterGain).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (2.0f * kCenterGain).margin (1.0e-4f));
}

TEST_CASE ("Mixer mute policy: SIP solo leaves only the soloed Track audible", "[mixer][mute][policy][solo][render]")
{
    constexpr NodeId kSrcA = 3100;   // soloed
    constexpr NodeId kSrcB = 3110;   // solo-muted

    MixerProjectionInputs inputs = baseProjection (3);
    inputs.tracks.push_back (makeTrack (kSrcA, 1.0f, 3120, 3130, 3140, 1.0f));
    inputs.tracks.push_back (makeTrack (kSrcB, 2.0f, 3121, 3131, 3141, 1.0f));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);

    const std::array<MixerMuteTarget, 2> targets {
        muteTarget (kSrcA, false, true, false),
        muteTarget (kSrcB, false, false, false),
    };
    REQUIRE (applyMixerMutePolicy (*graph, targets));
    REQUIRE_FALSE (graph->isMuted (kSrcA));
    REQUIRE (graph->isMuted (kSrcB));

    // Only Track A (1.0, centre) survives; B is solo-muted.
    const StereoCapture out = render (*graph, kBlock);
    REQUIRE (out.left.back() == Approx (1.0f * kCenterGain).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (1.0f * kCenterGain).margin (1.0e-4f));
}

TEST_CASE ("Mixer mute policy: a solo-safe Return stays audible without leaking non-soloed Tracks", "[mixer][mute][policy][solo][send]")
{
    constexpr NodeId kSrcA = 4100;   // soloed, sends to the Return
    constexpr NodeId kSrcB = 4110;   // not soloed, sends to the Return
    constexpr NodeId kBusSum = 4200; // the Return root (solo-safe)

    MixerProjectionInputs inputs = baseProjection (4);
    inputs.buses.push_back (MixerBusProjection { kBusSum, 4201, 4202 });

    MixerTrackProjection a = makeTrack (kSrcA, 1.0f, 4120, 4130, 4140, 0.0f);   // direct silent, send only
    a.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    MixerTrackProjection b = makeTrack (kSrcB, 2.0f, 4121, 4131, 4141, 0.0f);
    b.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    inputs.tracks.push_back (std::move (a));
    inputs.tracks.push_back (std::move (b));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);

    // Solo Track A; Track B is solo-muted at its source; the Return is solo-safe so it stays audible.
    const std::array<MixerMuteTarget, 3> targets {
        muteTarget (kSrcA, false, true, false),
        muteTarget (kSrcB, false, false, false),
        muteTarget (kBusSum, false, false, true),
    };
    REQUIRE (applyMixerMutePolicy (*graph, targets));
    REQUIRE_FALSE (graph->isMuted (kSrcA));
    REQUIRE (graph->isMuted (kSrcB));
    REQUIRE_FALSE (graph->isMuted (kBusSum));   // solo-safe Return is NOT muted

    // The Return carries only the soloed Track A's send (1.0), centre-widened; B's send is gone.
    const StereoCapture out = render (*graph, kBlock);
    REQUIRE (out.left.back() == Approx (1.0f * kCenterGain).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (1.0f * kCenterGain).margin (1.0e-4f));
}

TEST_CASE ("Mixer mute policy: a non-mute-capable target fails and leaves the mask unchanged", "[mixer][mute][policy][invalid]")
{
    constexpr NodeId kSrc = 5100;

    MixerProjectionInputs inputs = baseProjection (5);
    inputs.tracks.push_back (makeTrack (kSrc, 0.5f, 5200, 5300, 5400, 1.0f));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (graph->debugMuteMask() == 0u);

    // One valid target asking to mute, one bogus (absent) NodeId. The whole apply must fail BEFORE
    // publishing anything, so no partial mask leaks through.
    const std::array<MixerMuteTarget, 2> targets {
        muteTarget (kSrc, true, false, false),
        muteTarget (999999u, true, false, false),
    };
    REQUIRE_FALSE (applyMixerMutePolicy (*graph, targets));
    REQUIRE (graph->debugMuteMask() == 0u);   // unchanged: nothing was applied
    REQUIRE_FALSE (graph->isMuted (kSrc));
}
