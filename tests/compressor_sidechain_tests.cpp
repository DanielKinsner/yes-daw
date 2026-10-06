// G4.4 / ADR-0051 — a Compressor keyed by another strip, rendered through the real Project graph
// (buildProjectGraph: the projection playback and offline render share).
//
// The mix is the classic ghost trigger: a sustained synth "Pad" feeds the "Music" bus, whose Compressor
// is keyed by a "Kick" track whose fader is all the way down — inaudible in the mix, yet its pre-fader
// signal still keys the Compressor. Gates (render goldens by property / equivalence):
//  1. Keyed: the Pad ducks while each kick sounds and recovers between kicks.
//  2. Unkeyed: the same Compressor listens to the Pad itself — steady gain, no kick-shaped duck.
//  3. ADR-0014: a muted Kick keys nothing; soloing the Pad and its bus solo-mutes the Kick: it keys nothing.
//  4. Bypass keeps the key edge but changes nothing: no duck.
//  5. Setting and clearing the key leaves a render bit-identical to never keying it.

#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/nodes/CompressorNode.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

using yesdaw::engine::Bus;
using yesdaw::engine::CompressorNode;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::EventStream;
using yesdaw::engine::FxInsert;
using yesdaw::engine::FxKind;
using yesdaw::engine::MidiClip;
using yesdaw::engine::Note;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::Transport;

namespace {

constexpr double kSampleRate = 30720.0;   // 120 BPM => one tick == one frame
constexpr Tick kKickLength = 1536;        // 50 ms
constexpr std::array<Tick, 3> kKicks { 7680, 15360, 23040 };

EntityId idFromLowByte (std::uint8_t low)
{
    EntityId id;
    id.bytes.fill (0);
    id.bytes.back() = low;
    return id;
}

Note makeNote (std::uint8_t idLow, Tick start, Tick length, std::int16_t key)
{
    Note note;
    note.id = idFromLowByte (idLow);
    note.startTick = start;
    note.lengthTicks = length;
    note.key = key;
    note.pitchNote = static_cast<double> (key);
    note.normalizedVelocity = 1.0;
    note.channel = 0;
    return note;
}

MidiClip makeClip (std::uint8_t idLow, EntityId trackId, std::vector<Note> notes)
{
    MidiClip clip;
    clip.id = idFromLowByte (idLow);
    clip.trackId = trackId;
    clip.timelineStart = 0;
    clip.timelineLength = 30720;
    clip.timeBase = TimeBase::TempoLocked;
    clip.notes = std::move (notes);
    return clip;
}

double normalizedCompressor (std::uint32_t paramId, double real)
{
    return yesdaw::engine::unmapToNormalized (yesdaw::engine::fxParamSpecForKind (FxKind::Compressor, paramId), real);
}

// Pad -> Music bus (Compressor, hard and fast) ; Kick -> master with its fader at zero.
Project makeGhostKickProject (bool keyed)
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { kSampleRate };
    project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };

    Track pad;
    pad.id = idFromLowByte (31);
    pad.strip.name = "Pad";
    pad.outputBusId = idFromLowByte (50);
    Track kick;
    kick.id = idFromLowByte (32);
    kick.strip.name = "Kick";
    kick.strip.linearGain = 0.0f;   // the ghost trigger: silent in the mix, still keying pre-fader
    project.tracks = { pad, kick };

    Bus music;
    music.id = idFromLowByte (50);
    music.strip.name = "Music";
    FxInsert compressor;
    compressor.id = idFromLowByte (80);
    compressor.kind = FxKind::Compressor;
    compressor.normalizedParams = {
        { CompressorNode::kThresholdParamId, normalizedCompressor (CompressorNode::kThresholdParamId, -40.0) },
        { CompressorNode::kRatioParamId, normalizedCompressor (CompressorNode::kRatioParamId, 20.0) },
        { CompressorNode::kAttackParamId, normalizedCompressor (CompressorNode::kAttackParamId, 0.1) },
        { CompressorNode::kReleaseParamId, normalizedCompressor (CompressorNode::kReleaseParamId, 10.0) },
    };
    if (keyed)
        compressor.sidechainSourceId = kick.id;
    music.strip.fxChain = { compressor };
    project.buses = { music };

    project.midiClips = {
        makeClip (40, pad.id, { makeNote (60, 0, 30720, 60) }),
        makeClip (41, kick.id, { makeNote (61, kKicks[0], kKickLength, 36), makeNote (62, kKicks[1], kKickLength, 36),
                                 makeNote (63, kKicks[2], kKickLength, 36) }),
    };
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

std::vector<float> renderLeft (const Project& project)
{
    OfflineRenderOptions options;
    options.maxBlockSize = 64;
    auto built = yesdaw::engine::buildProjectGraph (project, std::span<const DecodedAssetAudio> {}, options);
    REQUIRE (built.ok());
    const std::uint16_t channels = built.channels;
    const std::uint64_t frames = built.frames;
    REQUIRE (frames >= 30720u);
    std::vector<float> left (static_cast<std::size_t> (frames), 0.0f);
    std::vector<float> storage (static_cast<std::size_t> (channels) * static_cast<std::size_t> (options.maxBlockSize), 0.0f);
    std::vector<float*> outputs (channels, nullptr);
    for (std::uint16_t c = 0; c < channels; ++c)
        outputs[c] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (options.maxBlockSize);
    std::uint64_t offset = 0;
    while (offset < frames)
    {
        const int blockFrames = static_cast<int> (std::min<std::uint64_t> (frames - offset, static_cast<std::uint64_t> (options.maxBlockSize)));
        Transport transport;
        transport.projectSampleRate = built.sampleRate;
        transport.isPlaying = true;
        transport.hasTimelineFrame = true;
        transport.timelineFrame = static_cast<std::int64_t> (offset);
        EventStream events;
        built.graph->process (outputs.data(), channels, blockFrames, events, transport);
        for (int i = 0; i < blockFrames; ++i)
            left[static_cast<std::size_t> (offset) + static_cast<std::size_t> (i)] = outputs[0][i];
        offset += static_cast<std::uint64_t> (blockFrames);
    }
    return left;
}

double rmsOf (const std::vector<float>& samples, Tick begin, Tick end)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (Tick i = begin; i < end && static_cast<std::size_t> (i) < samples.size(); ++i, ++count)
        sum += static_cast<double> (samples[static_cast<std::size_t> (i)]) * static_cast<double> (samples[static_cast<std::size_t> (i)]);
    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}

// The Pad's level while each kick sounds (past its attack) over its level once the kick's release tail and
// the Compressor's own release have passed — the worst (largest) ratio over the three kicks.
double worstDuckRatio (const std::vector<float>& render)
{
    double worst = 0.0;
    for (const Tick onset : kKicks)
    {
        const double during = rmsOf (render, onset + 300, onset + kKickLength);
        const double after = rmsOf (render, onset + 5700, onset + 7680);
        REQUIRE (after > 1.0e-4);
        worst = std::max (worst, during / after);
    }
    return worst;
}

double steadiness (const std::vector<float>& render)   // max |during/after - 1| over the kicks
{
    double worst = 0.0;
    for (const Tick onset : kKicks)
    {
        const double during = rmsOf (render, onset + 300, onset + kKickLength);
        const double after = rmsOf (render, onset + 5700, onset + 7680);
        REQUIRE (after > 1.0e-4);
        worst = std::max (worst, std::fabs (during / after - 1.0));
    }
    return worst;
}

} // namespace

TEST_CASE ("A keyed Compressor ducks the bus with a ghost kick; unkeyed, muted, solo-muted and bypassed do not",
           "[sidechain-key][render][compressor]")
{
    const std::vector<float> keyed = renderLeft (makeGhostKickProject (true));
    const std::vector<float> unkeyed = renderLeft (makeGhostKickProject (false));

    // 1. The Pad ducks hard while each (inaudible) kick sounds — at least 12 dB.
    INFO ("keyed worst duck ratio " << worstDuckRatio (keyed));
    REQUIRE (worstDuckRatio (keyed) < 0.25);

    // 2. Unkeyed, the Compressor follows the steady Pad: no kick-shaped duck — and it does compress (the
    //    Pad is far over -40 dB), so its level sits well under the keyed render's between-kick level.
    INFO ("unkeyed steadiness " << steadiness (unkeyed));
    REQUIRE (steadiness (unkeyed) < 0.05);
    REQUIRE (rmsOf (unkeyed, kKicks[0] + 5700, kKicks[0] + 7680) < 0.5 * rmsOf (keyed, kKicks[0] + 5700, kKicks[0] + 7680));

    // 3. ADR-0014: the key follows its source's mute and solo — a muted Kick, or a solo on the Pad and its
    //    bus (which solo-mutes the non-soloed Kick), keys nothing: the Pad passes uncompressed and steady.
    {
        Project muted = makeGhostKickProject (true);
        muted.tracks[1].strip.muted = true;
        const std::vector<float> render = renderLeft (muted);
        REQUIRE (steadiness (render) < 0.05);
        REQUIRE (rmsOf (render, kKicks[0] + 5700, kKicks[0] + 7680)
                 > 2.0 * rmsOf (unkeyed, kKicks[0] + 5700, kKicks[0] + 7680));
    }
    {
        Project soloed = makeGhostKickProject (true);
        soloed.tracks[0].strip.soloed = true;   // the Pad and its bus soloed (an active solo mutes every
        soloed.buses[0].strip.soloed = true;    // non-soloed strip): the Kick is solo-muted and keys nothing
        REQUIRE (steadiness (renderLeft (soloed)) < 0.05);
    }

    // 4. Bypass keeps the key edge but the Compressor changes nothing: no duck.
    {
        Project bypassed = makeGhostKickProject (true);
        bypassed.buses[0].strip.fxChain[0].enabled = false;
        REQUIRE (bypassed.hasValidAssetClipIndirection());
        REQUIRE (steadiness (renderLeft (bypassed)) < 0.05);
    }

    // 5. Setting then clearing the key renders bit-identically to never keying it.
    {
        Project cleared = makeGhostKickProject (true);
        REQUIRE (yesdaw::engine::setFxInsertSidechain (cleared, cleared.buses[0].id, cleared.buses[0].strip.fxChain[0].id, EntityId {})
                 == yesdaw::engine::ProjectEditStatus::Applied);
        REQUIRE (renderLeft (cleared) == unkeyed);
    }
}
