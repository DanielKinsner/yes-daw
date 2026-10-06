// YES DAW - ADR-0055 gates: cross-rate audio through rate-matched views.
//
// A 44.1 kHz Asset in a 48 kHz project: it lands at its true duration; live playback (the live tier) and an
// export (the offline tier) match the analytic 48 kHz signal; a click lands where the rate ratio puts it; a split
// renders the unsplit clip's audio; slip and stretch convert through the ratio; reopen and the self-check play
// it; a 44.1 kHz Sampler pad sounds at its own pitch.

#include "app/SelfCheck.h"
#include "engine/RateMatchedView.h"
#include "ui/MainComponentInternal.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <vector>

namespace {

constexpr double kAssetRate = 44'100.0;
constexpr double kProjectRate = 48'000.0;
constexpr double kRatio = kProjectRate / kAssetRate;

std::filesystem::path crossRateDirectory (const char* name)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-cross-rate-" + std::string (name) + "-"
           + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

std::vector<float> sineAt (double rate, std::size_t frames, double frequency = 1'000.0, double amplitude = 0.5)
{
    std::vector<float> out (frames);
    for (std::size_t i = 0; i < frames; ++i)
        out[i] = static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * frequency * static_cast<double> (i) / rate));
    return out;
}

// A 48 kHz model with one 44.1 kHz mono clip at timeline 0, made from `samples`.
struct CrossRateModel
{
    std::filesystem::path directory;
    std::filesystem::path bundle;
    yesdaw::ui::UiAppModel model;

    explicit CrossRateModel (const char* name, const std::vector<float>& samples)
        : directory (crossRateDirectory (name)), bundle (directory / "cross-rate.yesdaw")
    {
        const auto wav = directory / "asset-44k.wav";
        REQUIRE (yesdaw::io::writeFloat32WavFile (wav, yesdaw::engine::SampleRate { kAssetRate }, 1, samples.size(), samples).ok());
        REQUIRE (model.createProjectBundle (bundle).ok());
        REQUIRE (model.project().sampleRate.hz == kProjectRate);
        yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (wav);
        REQUIRE (decoded.decoded.has_value());
        REQUIRE (model.importAudioFile (wav, std::move (*decoded.decoded)).ok());
    }

    const yesdaw::engine::Clip& clip (std::size_t index = 0) const { return model.project().clips.at (index); }

    // Live playback from timeline 0: the left channel.
    std::vector<float> playLeft (std::uint64_t frames)
    {
        REQUIRE (model.locatePlaybackFrame (0));
        REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
        const std::vector<float> stereo = model.renderPlaybackFrames (frames, 128);
        REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportStop).dispatched);
        std::vector<float> left (stereo.size() / 2u);
        for (std::size_t i = 0; i < left.size(); ++i)
            left[i] = stereo[i * 2u];
        return left;
    }

    // An export (the offline tier): the left channel.
    std::vector<float> exportLeft()
    {
        const auto path = directory / ("export-" + std::to_string (++exports) + ".wav");
        REQUIRE (model.exportAudioFile (path).dispatched);
        const yesdaw::io::AudioDecodeResult read = yesdaw::io::decodeAudioFile (path);
        REQUIRE (read.audio.has_value());
        REQUIRE (read.audio->sampleRateHz == kProjectRate);
        const std::size_t channels = read.audio->channels;
        std::vector<float> left (read.audio->interleaved.size() / channels);
        for (std::size_t i = 0; i < left.size(); ++i)
            left[i] = read.audio->interleaved[i * channels];
        return left;
    }

    int exports = 0;
};

// The relative error after the best gain (the strip's pan law and the master scale the mix; the shape is what
// the resampler owns), in dB, over [from, to).
double gainNormalisedErrorDb (const std::vector<float>& actual, const std::vector<float>& reference, std::size_t from, std::size_t to)
{
    double cross = 0.0;
    double energy = 0.0;
    for (std::size_t i = from; i < to; ++i)
    {
        cross += static_cast<double> (actual[i]) * static_cast<double> (reference[i]);
        energy += static_cast<double> (reference[i]) * static_cast<double> (reference[i]);
    }
    const double gain = cross / energy;
    double error = 0.0;
    for (std::size_t i = from; i < to; ++i)
    {
        const double d = static_cast<double> (actual[i]) - gain * static_cast<double> (reference[i]);
        error += d * d;
    }
    return 10.0 * std::log10 (error / (gain * gain * energy));
}

std::size_t peakIndex (const std::vector<float>& samples)
{
    std::size_t best = 0;
    for (std::size_t i = 0; i < samples.size(); ++i)
        if (std::abs (samples[i]) > std::abs (samples[best]))
            best = i;
    return best;
}

} // namespace

TEST_CASE ("ADR-0055 a 44.1 kHz clip lands at its true duration and plays (live) and exports (offline) as the analytic signal",
           "[cross-rate]")
{
    CrossRateModel f ("sine", sineAt (kAssetRate, 88'200));   // two seconds
    const yesdaw::engine::Clip& clip = f.clip();
    REQUIRE (clip.srcLen == 88'200u);                         // the window stays in Asset frames
    REQUIRE (clip.timelineLength == 96'000);                  // round (88 200 x 48 000 / 44 100)
    const yesdaw::engine::Asset* asset = f.model.project().findAsset (clip.assetId);
    REQUIRE (asset != nullptr);
    REQUIRE (asset->sampleRate.hz == kAssetRate);             // the Asset keeps its own rate
    REQUIRE (asset->frames == 88'200u);

    const std::vector<float> reference = sineAt (kProjectRate, 96'000);
    const std::vector<float> live = f.playLeft (96'000);
    const double liveDb = gainNormalisedErrorDb (live, reference, 4'000, 92'000);
    INFO ("live " << liveDb << " dB");
    REQUIRE (liveDb < -60.0);

    const std::vector<float> exported = f.exportLeft();
    REQUIRE (exported.size() >= 96'000u);
    const double offlineDb = gainNormalisedErrorDb (exported, reference, 4'000, 92'000);
    INFO ("offline " << offlineDb << " dB");
    REQUIRE (offlineDb < -80.0);
}

TEST_CASE ("ADR-0055 a click in a 44.1 kHz clip lands where the rate ratio puts it; a split renders the same audio",
           "[cross-rate]")
{
    std::vector<float> clicks (44'100, 0.0f);
    clicks[10'000] = 0.9f;
    clicks[30'000] = -0.9f;
    CrossRateModel f ("click", clicks);
    REQUIRE (f.clip().timelineLength == 48'000);

    const auto expected = [] (std::size_t assetFrame) { return static_cast<long long> (std::llround (static_cast<double> (assetFrame) * kRatio)); };
    const std::vector<float> live = f.playLeft (48'000);
    std::vector<float> firstHalf (live.begin(), live.begin() + 24'000);
    std::vector<float> secondHalf (live.begin() + 24'000, live.end());
    REQUIRE (std::abs (static_cast<long long> (peakIndex (firstHalf)) - expected (10'000)) <= 1);
    REQUIRE (std::abs (static_cast<long long> (peakIndex (secondHalf)) + 24'000 - expected (30'000)) <= 1);
    const std::vector<float> exported = f.exportLeft();
    REQUIRE (std::abs (static_cast<long long> (peakIndex (std::vector<float> (exported.begin(), exported.begin() + 24'000))) - expected (10'000)) <= 1);

    // Split between the clicks: the halves abut exactly in the view and the render is unchanged.
    REQUIRE (f.model.selectTimelineClip (f.clip().id));
    REQUIRE (f.model.splitSelectedTimelineClipAt (20'000).dispatched);
    REQUIRE (f.model.project().clips.size() == 2u);
    const yesdaw::engine::Project mapped = yesdaw::engine::projectInViewFrames (f.model.project());
    const auto& left = mapped.clips[0].timelineStart < mapped.clips[1].timelineStart ? mapped.clips[0] : mapped.clips[1];
    const auto& right = mapped.clips[0].timelineStart < mapped.clips[1].timelineStart ? mapped.clips[1] : mapped.clips[0];
    REQUIRE (right.srcOffset == left.srcOffset + left.srcLen);   // abutting in view frames
    const std::vector<float> split = f.playLeft (48'000);
    double worst = 0.0;
    for (std::size_t i = 0; i < live.size(); ++i)
        worst = std::max (worst, static_cast<double> (std::abs (split[i] - live[i])));
    INFO ("worst split difference " << worst);
    REQUIRE (worst < 1.0e-6);
}

TEST_CASE ("ADR-0055 slip and stretch on a 44.1 kHz clip convert through the rate ratio", "[cross-rate]")
{
    CrossRateModel f ("edits", sineAt (kAssetRate, 44'100));
    REQUIRE (f.model.selectTimelineClip (f.clip().id));
    REQUIRE (f.model.dispatch (yesdaw::ui::UiActionId::TimelineClipStretchToLoop).dispatched == false);   // no loop region
    REQUIRE (f.model.setSelectedTimelineClipStretchFactor (1.5f).dispatched);
    REQUIRE (f.clip().stretchFactor == 1.5f);
    REQUIRE (f.clip().timelineLength == 72'000);   // round (44 100 x r x 1.5): the natural 48 000 stretched by half
    REQUIRE (f.model.setSelectedTimelineClipStretchFactor (1.0f).dispatched);
    REQUIRE (f.clip().timelineLength == 48'000);
    REQUIRE (f.model.stretchSelectedTimelineClipTo (f.clip().timelineStart + 96'000).dispatched);
    REQUIRE (f.clip().stretchFactor == 2.0f);      // 96 000 / (44 100 x r): twice the natural length
    REQUIRE (f.model.setSelectedTimelineClipStretchFactor (1.0f).dispatched);

    // Slip: split at the clip's middle, then slip the right half by 480 ticks — 441 Asset frames, not 480.
    REQUIRE (f.model.splitSelectedTimelineClipAt (24'000).dispatched);
    const yesdaw::engine::Clip* right = nullptr;
    for (const yesdaw::engine::Clip& candidate : f.model.project().clips)
        if (candidate.timelineStart == 24'000)
            right = &candidate;
    REQUIRE (right != nullptr);
    REQUIRE (right->srcOffset == 22'050u);
    REQUIRE (f.model.selectTimelineClip (right->id));
    REQUIRE (f.model.slipSelectedTimelineClipBy (480).dispatched);
    for (const yesdaw::engine::Clip& candidate : f.model.project().clips)
        if (candidate.timelineStart == 24'000)
            REQUIRE (candidate.srcOffset == 22'050u - 441u);
}

TEST_CASE ("ADR-0055 a cross-rate project reopens and plays the same, and the self-check renders it", "[cross-rate]")
{
    std::vector<float> samples = sineAt (kAssetRate, 22'050, 440.0);
    CrossRateModel f ("reopen", samples);
    const std::vector<float> before = f.playLeft (24'000);
    REQUIRE (f.model.saveProjectBundle().ok());

    auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (f.bundle);
    INFO (stored.failureReason);
    REQUIRE (stored.assets.has_value());
    yesdaw::ui::UiAppModel reopened;
    REQUIRE (reopened.loadPreparedProjectBundle (std::move (stored.prepared), std::move (*stored.assets)).ok());
    REQUIRE (reopened.locatePlaybackFrame (0));
    REQUIRE (reopened.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
    const std::vector<float> stereo = reopened.renderPlaybackFrames (24'000, 128);
    for (std::size_t i = 0; i < before.size(); ++i)
        REQUIRE (stereo[i * 2u] == before[i]);

    const yesdaw::app::SelfCheckResult check = yesdaw::app::runSelfCheck (f.bundle);
    INFO (check.message);
    REQUIRE (check.ok);
    REQUIRE (check.sampleRateHz == kProjectRate);
}

TEST_CASE ("ADR-0055 a 44.1 kHz Sampler pad sounds at its own pitch in a 48 kHz project", "[cross-rate]")
{
    using namespace yesdaw::engine;
    const auto id = [] (std::uint8_t low) {
        EntityId out;
        out.bytes.fill (0);
        out.bytes.back() = low;
        return out;
    };
    const std::vector<float> tone = sineAt (kAssetRate, 44'100, 1'000.0);
    Project project;
    project.id = id (1);
    project.sampleRate = SampleRate { kProjectRate };
    Asset asset;
    asset.id = id (20);
    asset.contentHash.bytes.fill (20);
    asset.frames = tone.size();
    asset.sampleRate = SampleRate { kAssetRate };
    asset.channels = 1;
    project.assets = { asset };
    Track track;
    track.id = id (31);
    track.strip.name = "Pads";
    track.instrumentKind = TrackInstrumentKind::Sampler;
    project.tracks = { track };
    SamplerPad pad;
    pad.key = 60;
    pad.assetId = asset.id;
    pad.rootKey = 60;
    pad.oneShot = true;
    pad.gain = 1.0;
    pad.setName ("Tone");
    REQUIRE (setSamplerPad (project, track.id, pad) == ProjectEditStatus::Applied);
    project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    MidiClip clip;
    clip.id = id (40);
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = 15'360;   // one beat: half a second at 120 BPM
    clip.timeBase = TimeBase::TempoLocked;
    Note note;
    note.id = id (41);
    note.startTick = 0;
    note.lengthTicks = 15'360;
    note.key = 60;
    note.pitchNote = 60.0;
    note.normalizedVelocity = 1.0;
    note.channel = 0;
    clip.notes = { note };
    project.midiClips = { clip };
    REQUIRE (project.hasValidAssetClipIndirection());

    // What the model hands an engine: the view-frame project and the live-tier view.
    const Project engineProject = projectInViewFrames (project);
    const std::shared_ptr<const AssetSamples> view =
        buildRateMatchedSamples (tone, 1, kAssetRate, kProjectRate, ResampleQuality::LivePlayback);
    const DecodedAssetAudio decoded { asset.id, project.sampleRate, view->frames, 1,
                                      std::span<const float> (view->interleaved.data(), view->interleaved.size()) };
    OfflineRenderOptions options;
    options.maxBlockSize = 64;
    auto built = buildProjectGraph (engineProject, std::span<const DecodedAssetAudio> (&decoded, 1), options);
    REQUIRE (built.ok());
    const std::uint16_t channels = built.channels;
    std::vector<float> storage (static_cast<std::size_t> (channels) * 64u, 0.0f);
    std::vector<float*> outputs (channels, nullptr);
    for (std::uint16_t c = 0; c < channels; ++c)
        outputs[c] = storage.data() + static_cast<std::size_t> (c) * 64u;
    std::vector<float> left;
    for (std::uint64_t offset = 0; offset < 24'000; offset += 64)
    {
        Transport transport;
        transport.projectSampleRate = built.sampleRate;
        transport.isPlaying = true;
        transport.hasTimelineFrame = true;
        transport.timelineFrame = static_cast<std::int64_t> (offset);
        EventStream events;
        built.graph->process (outputs.data(), channels, 64, events, transport);
        left.insert (left.end(), outputs[0], outputs[0] + 64);
    }
    // Count rising zero crossings over a steady 0.3 s: a 1 kHz tone gives 300; played at the wrong rate it
    // would give 1 kHz x 48 / 44.1 = 1 088 Hz, 326.
    int crossings = 0;
    for (std::size_t i = 2'401; i < 2'400 + 14'400; ++i)
        crossings += (left[i - 1] <= 0.0f && left[i] > 0.0f) ? 1 : 0;
    INFO ("crossings " << crossings);
    REQUIRE (std::abs (crossings - 300) <= 1);
}
