// G3.9 / ADR-0048 — the Sampler instrument: pads on the Track referencing Project Assets, one-shot
// and pitched playback, the ADSR, the pad verbs and their undo, persistence (v31), and the render
// goldens by equivalence.
//
// Gates:
//  1. Pads are rows: setSamplerPad replaces-or-adds by key and keeps key order; clearSamplerPad
//     removes; refusals (an unknown Asset, a pad out of range, an unknown key, an unknown Track);
//     the verbs are undoable as Track row edits; the Project's validity sees a pad naming a missing
//     Asset.
//  2. One-shot golden: a one-shot pad struck by a 1-frame note renders the sample VERBATIM from the
//     note's frame (attack 1 ms aside) — NoteOff is ignored, the whole sample plays; two strikes stack.
//  3. Pitched golden: a pitched pad an octave above its root renders the sample read at twice the
//     rate (linear interpolation — the reference is computed the same way from the asset); NoteOff
//     releases it through the ADSR (the tail fades); an unclaimed key plays the nearest LOWER pitched
//     pad transposed; a one-shot pad never answers to another key.
//  4. Parameters: the five ParamSpecs; gain scales the render; a Track with the Sampler kind and no
//     pads renders silence (not the synth).
//  5. Persistence: pads round-trip through the bundle (v31), a pad naming a missing Asset is refused
//     on write, and the asset cannot be swept while a pad references it (the foreign key).

#include "engine/GraphBuilder.h"
#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "engine/ProjectUndo.h"
#include "engine/nodes/SamplerNode.h"
#include "persistence/ProjectBundle.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::EventStream;
using yesdaw::engine::MidiClip;
using yesdaw::engine::Note;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditCommand;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::ProjectUndoStack;
using yesdaw::engine::SampleRate;
using yesdaw::engine::SamplerNode;
using yesdaw::engine::SamplerPad;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::TrackInstrumentKind;
using yesdaw::engine::Transport;

namespace {

constexpr double kSampleRate = 30720.0;   // 120 BPM => one tick == one frame
constexpr std::uint64_t kSampleFrames = 2000;

EntityId idFromLowByte (std::uint8_t low)
{
    EntityId id;
    id.bytes.fill (0);
    id.bytes.back() = low;
    return id;
}

// A deterministic mono sample: a decaying ramp with a sign flip every 37 frames (no periodicity that
// a resampler could hide behind).
std::vector<float> makeSample()
{
    std::vector<float> out (kSampleFrames, 0.0f);
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        const float decay = 1.0f - static_cast<float> (i) / static_cast<float> (out.size());
        const float sign = (i / 37) % 2 == 0 ? 1.0f : -1.0f;
        out[i] = sign * decay * (0.25f + 0.5f * static_cast<float> (i % 7) / 7.0f);
    }
    return out;
}

const std::vector<float>& sampleData()
{
    static const std::vector<float> data = makeSample();
    return data;
}

Asset makeAsset (std::uint8_t idLow)
{
    Asset asset;
    asset.id = idFromLowByte (idLow);
    asset.contentHash.bytes.fill (idLow);
    asset.frames = kSampleFrames;
    asset.sampleRate = SampleRate { kSampleRate };
    asset.channels = 1;
    return asset;
}

DecodedAssetAudio decodedFor (const Asset& asset)
{
    return DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                               std::span<const float> (sampleData().data(), sampleData().size()) };
}

Note makeNote (std::uint8_t idLow, Tick start, Tick length, std::int16_t key, double velocity = 1.0)
{
    Note note;
    note.id = idFromLowByte (idLow);
    note.startTick = start;
    note.lengthTicks = length;
    note.key = key;
    note.pitchNote = static_cast<double> (key);
    note.normalizedVelocity = velocity;
    note.channel = 0;
    return note;
}

SamplerPad makePad (std::int16_t key, std::uint8_t assetLow, std::int16_t rootKey, bool oneShot, const char* name = "Pad")
{
    SamplerPad pad;
    pad.key = key;
    pad.assetId = idFromLowByte (assetLow);
    pad.rootKey = rootKey;
    pad.oneShot = oneShot;
    pad.gain = 1.0;
    pad.setName (name);
    return pad;
}

Project makeSamplerProject (std::vector<Note> notes, std::vector<SamplerPad> pads, Tick clipLength = 30720)
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { kSampleRate };
    project.assets = { makeAsset (20) };
    Track track;
    track.id = idFromLowByte (31);
    track.strip.name = "Drums";
    track.instrumentKind = TrackInstrumentKind::Sampler;
    for (const SamplerPad& pad : pads)
        REQUIRE (yesdaw::engine::setSamplerPad (project, track.id, pad) == ProjectEditStatus::TrackNotFound);   // no track yet
    project.tracks = { track };
    for (const SamplerPad& pad : pads)
        REQUIRE (yesdaw::engine::setSamplerPad (project, track.id, pad) == ProjectEditStatus::Applied);
    project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    MidiClip clip;
    clip.id = idFromLowByte (40);
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = clipLength;
    clip.timeBase = TimeBase::TempoLocked;
    clip.notes = std::move (notes);
    project.midiClips = { clip };
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

std::vector<float> renderProject (const Project& project)
{
    OfflineRenderOptions options;
    options.maxBlockSize = 64;
    const DecodedAssetAudio decoded = decodedFor (project.assets.front());
    auto built = yesdaw::engine::buildProjectGraph (project, std::span<const DecodedAssetAudio> (&decoded, 1), options);
    REQUIRE (built.ok());
    const std::uint16_t channels = built.channels;
    const std::uint64_t frames = built.frames;
    REQUIRE (frames > 0);
    std::vector<float> out (static_cast<std::size_t> (frames) * channels, 0.0f);
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
        for (std::uint16_t c = 0; c < channels; ++c)
            for (int i = 0; i < blockFrames; ++i)
                out[(offset + static_cast<std::uint64_t> (i)) * channels + c] = outputs[c][i];
        offset += static_cast<std::uint64_t> (blockFrames);
    }
    std::vector<float> left (static_cast<std::size_t> (frames), 0.0f);
    for (std::size_t i = 0; i < left.size(); ++i)
        left[i] = out[i * channels];
    return left;
}

double rmsOf (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t i = begin; i < end && i < samples.size(); ++i, ++count)
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);
    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}

// The strip's fader / pan / master path is unity at default, but the master and the strip's pan law
// scale a mono source: measure the path's gain once with a known impulse-free reference — the ratio
// between the render and the sample at a steady frame.
double pathGainFrom (const std::vector<float>& render, std::size_t noteFrame)
{
    // frame 100 after the note: the attack (1 ms = 31 frames) is over; sample[100] is nonzero.
    const std::size_t at = noteFrame + 100;
    REQUIRE (std::abs (sampleData()[100]) > 0.05f);
    return static_cast<double> (render[at]) / static_cast<double> (sampleData()[100]);
}

std::filesystem::path makeTempBundlePath (const std::string& label)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("yesdaw-sampler-" + label + "-" + std::to_string (stamp) + ".yesdaw");
}

} // namespace

TEST_CASE ("Sampler pads are Track rows: set / clear by key in key order, refusals, undo, validity", "[sampler]")
{
    Project project = makeSamplerProject ({}, {});
    const EntityId trackId = project.tracks[0].id;
    REQUIRE (yesdaw::engine::setSamplerPad (project, trackId, makePad (38, 20, 38, true, "Snare")) == ProjectEditStatus::Applied);
    REQUIRE (yesdaw::engine::setSamplerPad (project, trackId, makePad (36, 20, 36, true, "Kick")) == ProjectEditStatus::Applied);
    REQUIRE (project.tracks[0].samplerPads.size() == 2u);
    REQUIRE (project.tracks[0].samplerPads[0].key == 36);   // key order
    REQUIRE (project.tracks[0].samplerPads[1].nameView() == "Snare");
    REQUIRE (yesdaw::engine::setSamplerPad (project, trackId, makePad (36, 20, 48, false, "Kick2")) == ProjectEditStatus::Applied);   // replace by key
    REQUIRE (project.tracks[0].samplerPads.size() == 2u);
    REQUIRE (project.tracks[0].samplerPads[0].rootKey == 48);
    REQUIRE (project.tracks[0].findSamplerPad (36)->nameView() == "Kick2");

    REQUIRE (yesdaw::engine::setSamplerPad (project, trackId, makePad (40, 99, 40, true)) == ProjectEditStatus::SamplerPadAssetNotFound);
    SamplerPad bad = makePad (40, 20, 40, true);
    bad.gain = 3.0;
    REQUIRE (yesdaw::engine::setSamplerPad (project, trackId, bad) == ProjectEditStatus::InvalidSamplerPad);
    REQUIRE (yesdaw::engine::setSamplerPad (project, idFromLowByte (77), makePad (40, 20, 40, true)) == ProjectEditStatus::TrackNotFound);
    REQUIRE (yesdaw::engine::clearSamplerPad (project, trackId, 40) == ProjectEditStatus::SamplerPadNotFound);
    REQUIRE (yesdaw::engine::clearSamplerPad (project, trackId, 38) == ProjectEditStatus::Applied);
    REQUIRE (project.tracks[0].samplerPads.size() == 1u);

    // The verbs on the undo stack: a Track row edit each; undo restores the row.
    ProjectUndoStack undo;
    REQUIRE (undo.apply (project, ProjectEditCommand::setSamplerPad (trackId, makePad (42, 20, 42, true, "Hat"))).applied());
    REQUIRE (project.tracks[0].samplerPads.size() == 2u);
    REQUIRE (undo.apply (project, ProjectEditCommand::clearSamplerPad (trackId, 36)).applied());
    REQUIRE (project.tracks[0].samplerPads.size() == 1u);
    REQUIRE (project.tracks[0].samplerPads[0].key == 42);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.tracks[0].samplerPads.size() == 2u);
    REQUIRE (project.tracks[0].samplerPads[0].key == 36);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.tracks[0].samplerPads.size() == 1u);
    REQUIRE_FALSE (undo.apply (project, ProjectEditCommand::clearSamplerPad (trackId, 99)).applied());

    // Validity: a pad naming a missing Asset makes the Project invalid (the write refuses it).
    Project broken = project;
    broken.tracks[0].samplerPads[0].assetId = idFromLowByte (99);
    REQUIRE_FALSE (broken.hasValidAssetClipIndirection());
    REQUIRE (project.hasValidAssetClipIndirection());

    // The name law: 63 characters at most, always terminated.
    SamplerPad longName;
    longName.setName (std::string (200, 'x'));
    REQUIRE (longName.nameView().size() == SamplerPad::kMaxNameLength);
}

TEST_CASE ("Sampler one-shot golden: the sample verbatim from the note's frame, NoteOff ignored, strikes stack", "[sampler][golden]")
{
    // A one-frame note at frame 1000 on the Kick pad (one-shot).
    const Project project = makeSamplerProject ({ makeNote (50, 1000, 1, 36) }, { makePad (36, 20, 36, true, "Kick") });
    const std::vector<float> render = renderProject (project);
    REQUIRE (rmsOf (render, 0, 1000) == 0.0);
    const double pathGain = pathGainFrom (render, 1000);
    REQUIRE (pathGain > 0.1);
    // After the 1 ms attack (31 frames) the render IS the sample, scaled by the path's gain, to the end.
    for (std::size_t i = 40; i < kSampleFrames - 1; i += 13)
        REQUIRE (render[1000 + i] == Catch::Approx (sampleData()[i] * pathGain).margin (1.0e-4));
    // Past the sample's end: silence (the one-shot ends with the sample, not the note).
    REQUIRE (rmsOf (render, 1000 + kSampleFrames + 8, 1000 + kSampleFrames + 400) == 0.0);

    // Two strikes 500 frames apart stack (two voices); the sum is the two shifted samples.
    const Project two = makeSamplerProject ({ makeNote (50, 1000, 1, 36), makeNote (51, 1500, 1, 36) }, { makePad (36, 20, 36, true, "Kick") });
    const std::vector<float> stacked = renderProject (two);
    for (std::size_t i = 600; i < 1200; i += 17)
        REQUIRE (stacked[1000 + i] == Catch::Approx ((sampleData()[i] + sampleData()[i - 500]) * pathGain).margin (1.0e-4));
}

TEST_CASE ("Sampler pitched golden: an octave up reads at twice the rate, NoteOff releases, unclaimed keys map to the lower pitched pad", "[sampler][golden]")
{
    // A pitched pad rooted at C4 on key 60; a long note an octave up (72) at frame 1000.
    const Project up = makeSamplerProject ({ makeNote (50, 1000, 4000, 72) }, { makePad (60, 20, 60, false, "Tone") });
    const std::vector<float> render = renderProject (up);
    const std::vector<float> reference = renderProject (makeSamplerProject ({ makeNote (50, 1000, 4000, 60) }, { makePad (60, 20, 60, false, "Tone") }));
    const double pathGain = pathGainFrom (reference, 1000);
    // Linear interpolation at rate 2: frame i reads sample[2i] exactly (integer positions).
    for (std::size_t i = 40; i < kSampleFrames / 2 - 1; i += 11)
        REQUIRE (render[1000 + i] == Catch::Approx (sampleData()[2 * i] * pathGain).margin (1.0e-4));
    // The octave-up voice ends when the sample runs out (half the frames) while the root voice goes on.
    REQUIRE (rmsOf (render, 1000 + kSampleFrames / 2 + 8, 1000 + kSampleFrames / 2 + 300) == 0.0);
    REQUIRE (rmsOf (reference, 1000 + kSampleFrames / 2 + 8, 1000 + kSampleFrames / 2 + 300) > 0.0);

    // NoteOff releases a pitched pad: a 300-frame note's tail fades within the 20 ms release (614 frames).
    const Project shortNote = makeSamplerProject ({ makeNote (50, 1000, 300, 60) }, { makePad (60, 20, 60, false, "Tone") });
    const std::vector<float> released = renderProject (shortNote);
    REQUIRE (rmsOf (released, 1300 + 700, 1300 + 900) == 0.0);
    REQUIRE (rmsOf (reference, 1300 + 700, 1300 + 900) > 0.0);

    // Two pads: a one-shot Kick on 36 and a pitched Tone on 60. Key 67 plays the Tone a fifth up
    // (the nearest lower pitched pad); key 40 plays NOTHING (the one-shot below never answers).
    const std::vector<SamplerPad> kit = { makePad (36, 20, 36, true, "Kick"), makePad (60, 20, 60, false, "Tone") };
    const std::vector<float> fifth = renderProject (makeSamplerProject ({ makeNote (50, 1000, 4000, 67) }, kit));
    const double ratio = std::pow (2.0, 7.0 / 12.0);
    for (std::size_t i = 40; i < 600; i += 23)
    {
        const double position = static_cast<double> (i) * ratio;
        const std::size_t index = static_cast<std::size_t> (std::floor (position));
        const double fraction = position - static_cast<double> (index);
        const double expected = (sampleData()[index] + (sampleData()[index + 1] - sampleData()[index]) * fraction) * pathGain;
        REQUIRE (fifth[1000 + i] == Catch::Approx (expected).margin (2.0e-4));
    }
    const std::vector<float> nothing = renderProject (makeSamplerProject ({ makeNote (50, 1000, 4000, 40) }, kit));
    REQUIRE (rmsOf (nothing, 0, nothing.size()) == 0.0);
}

TEST_CASE ("Sampler parameters: the five specs, gain scales the render, a padless Sampler is silent", "[sampler]")
{
    for (std::uint32_t id = 1; id <= 5; ++id)
    {
        const yesdaw::engine::ParamSpec spec = yesdaw::engine::instrumentParamSpecForKind (TrackInstrumentKind::Sampler, id);
        REQUIRE (spec.id == id);
        REQUIRE (std::string (spec.name).rfind ("sampler.", 0) == 0);
        REQUIRE (yesdaw::engine::instrumentKindAcceptsParameterId (TrackInstrumentKind::Sampler, id));
    }
    REQUIRE_FALSE (yesdaw::engine::instrumentKindAcceptsParameterId (TrackInstrumentKind::Sampler, 6));

    Project loud = makeSamplerProject ({ makeNote (50, 1000, 1, 36) }, { makePad (36, 20, 36, true, "Kick") });
    const std::vector<float> reference = renderProject (loud);
    // gain spec 0..2 linear: normalized 0.25 = 0.5x.
    REQUIRE (yesdaw::engine::setTrackInstrumentParam (loud, loud.tracks[0].id, SamplerNode::kGainParamId, 0.25) == ProjectEditStatus::Applied);
    const std::vector<float> half = renderProject (loud);
    for (std::size_t i = 1040; i < 2900; i += 29)
        REQUIRE (half[i] == Catch::Approx (reference[i] * 0.5).margin (1.0e-5));

    const Project padless = makeSamplerProject ({ makeNote (50, 1000, 4000, 60) }, {});
    const std::vector<float> silent = renderProject (padless);
    REQUIRE (rmsOf (silent, 0, silent.size()) == 0.0);
    // The same project as a synth is not silent — the kind is what plays.
    Project synth = padless;
    synth.tracks[0].instrumentKind = TrackInstrumentKind::SimpleSynth;
    REQUIRE (rmsOf (renderProject (synth), 1000, 3000) > 0.01);
}

TEST_CASE ("Sampler pads persist (v31): round-trip, a missing Asset refuses, the foreign key holds the Asset", "[sampler]")
{
    Project project = makeSamplerProject ({ makeNote (50, 0, 1, 36) }, { makePad (36, 20, 36, true, "Kick"), makePad (60, 20, 48, false, "Tone") });
    project.tracks[0].samplerPads[1].gain = 0.75;
    const std::filesystem::path path = makeTempBundlePath ("pads");
    {
        yesdaw::persistence::ProjectBundleDb db;
        REQUIRE (yesdaw::persistence::ProjectBundleDb::openOrCreateBundle (path, db).ok());
        const yesdaw::persistence::BundleResult written = db.writeProjectSnapshot (project);
        INFO (written.message);
        REQUIRE (written.ok());
        Project back;
        const yesdaw::persistence::BundleResult read = db.readProjectSnapshot (back);
        INFO (read.message);
        REQUIRE (read.ok());
        REQUIRE (back.tracks[0].instrumentKind == TrackInstrumentKind::Sampler);
        REQUIRE (back.tracks[0].samplerPads.size() == 2u);
        REQUIRE (back.tracks[0].samplerPads[0] == project.tracks[0].samplerPads[0]);
        REQUIRE (back.tracks[0].samplerPads[1] == project.tracks[0].samplerPads[1]);
        REQUIRE (back.tracks[0].samplerPads[1].nameView() == "Tone");
        REQUIRE (back.tracks[0].samplerPads[1].gain == Catch::Approx (0.75));

        Project broken = project;
        broken.tracks[0].samplerPads[0].assetId = idFromLowByte (99);
        REQUIRE (db.writeProjectSnapshot (broken).status == yesdaw::persistence::BundleStatus::SemanticInvalid);

        // The asset the pads reference cannot be deleted from under them.
        const auto deleteAsset = db.executeSql ("DELETE FROM assets WHERE id = X'00000000000000000000000000000014';");
        REQUIRE ((deleteAsset.sqliteCode == SQLITE_CONSTRAINT || deleteAsset.sqliteCode == SQLITE_CONSTRAINT_FOREIGNKEY));
    }
    std::error_code ec;
    std::filesystem::remove_all (path, ec);
}
