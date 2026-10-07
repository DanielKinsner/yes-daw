#include "ui/MainComponentInternal.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

namespace {

std::filesystem::path loadTestDirectory()
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-prepared-load-" + std::to_string (
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

void seedAudioBundle (const std::filesystem::path& directory)
{
    const auto wav = directory / "source.wav";
    const std::vector<float> samples (4096, 0.25f);
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        wav, yesdaw::engine::SampleRate { 48000.0 }, 1, samples.size(), samples).ok());
    auto decoded = yesdaw::ui::shell::decodeProjectWav (wav);
    REQUIRE (decoded.has_value());
    yesdaw::ui::UiAppModel source;
    REQUIRE (source.createProjectBundle (directory / "audio.yesdaw").ok());
    REQUIRE (source.importAudioFile (wav, std::move (*decoded)).ok());
    REQUIRE (source.saveProjectBundle().ok());
}

} // namespace

TEST_CASE ("prepared project load owns decoded audio after the shell result is destroyed",
           "[ui][input][project-load][prepared-project-load]")
{
    static_assert (! std::is_copy_constructible_v<yesdaw::ui::UiPreparedProjectBundle>);
    const auto directory = loadTestDirectory();
    seedAudioBundle (directory);
    yesdaw::ui::UiAppModel model;
    {
        auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (directory / "audio.yesdaw");
        REQUIRE (stored.assets.has_value());
        REQUIRE (stored.assets->size() == 1u);
        REQUIRE (stored.prepared.project().clips.size() == 1u);
        REQUIRE (model.loadPreparedProjectBundle (std::move (stored.prepared),
                                                  std::move (*stored.assets)).ok());
    }
    REQUIRE (model.bundlePath() == directory / "audio.yesdaw");
    REQUIRE (model.playbackReady());
    REQUIRE_FALSE (model.hasUnsavedChanges());
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
    const auto rendered = model.renderPlaybackFrames (512, 128);
    REQUIRE (rendered.size() == 1024u);
    REQUIRE (std::all_of (rendered.begin(), rendered.end(), [] (float sample) { return std::isfinite (sample); }));
    REQUIRE (std::any_of (rendered.begin(), rendered.end(), [] (float sample) { return std::abs (sample) > 0.1f; }));
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportStop).dispatched);
    REQUIRE (model.addAudioTrack().dispatched);
    REQUIRE (model.saveProjectBundle().ok());
    yesdaw::ui::UiAppModel reopened;
    REQUIRE (reopened.openProjectBundle (directory / "audio.yesdaw").ok());
    REQUIRE (reopened.project().tracks.size() == 2u);
}

TEST_CASE ("prepared load failure retains the editable original session",
           "[ui][input][project-load][prepared-project-load]")
{
    const auto directory = loadTestDirectory();
    seedAudioBundle (directory);
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "original.yesdaw").ok());
    REQUIRE (model.addAudioTrack().dispatched);
    const auto originalId = model.project().id;
    const auto dispatches = model.context().commandDispatchCount;
    SECTION ("an unopened wrapper cannot be adopted")
    {
        const auto result = model.loadPreparedProjectBundle ({}, {});
        REQUIRE (result.status == yesdaw::ui::UiAppLoadStatus::BundleOpenFailed);
    }
    SECTION ("playback refuses a bad decoded asset before attachment")
    {
        auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (directory / "audio.yesdaw");
        REQUIRE (stored.assets.has_value());
        yesdaw::ui::UiDecodedAsset& front = stored.assets->front();   // ADR-0059: a buffer is immutable — make a bad one
        std::vector<float> bad (front.interleaved().begin(), front.interleaved().end());
        bad.front() = std::numeric_limits<float>::quiet_NaN();
        front = yesdaw::ui::UiDecodedAsset::fromInterleaved (front.assetId, front.sampleRate, front.channels, std::move (bad));
        const auto result = model.loadPreparedProjectBundle (std::move (stored.prepared),
                                                              std::move (*stored.assets));
        REQUIRE (result.status == yesdaw::ui::UiAppLoadStatus::PlaybackBuildFailed);
    }
    REQUIRE (model.bundlePath() == directory / "original.yesdaw");
    REQUIRE (model.project().id == originalId);
    REQUIRE (model.context().commandDispatchCount == dispatches);
    REQUIRE (model.hasUnsavedChanges());
    REQUIRE (model.addAudioTrack().dispatched);
    REQUIRE (model.saveProjectBundle().ok());
    yesdaw::ui::UiAppModel reopened;
    REQUIRE (reopened.openProjectBundle (directory / "original.yesdaw").ok());
    REQUIRE (reopened.project().tracks.size() == 3u);
}

TEST_CASE ("prepared shell open retains missing and corrupt asset integrity refusal",
           "[ui][input][project-load][prepared-project-load]")
{
    const auto directory = loadTestDirectory();
    seedAudioBundle (directory);
    const auto bundle = directory / "audio.yesdaw";
    std::filesystem::path assetPath;
    {
        auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
        REQUIRE (stored.assets.has_value());
        assetPath = yesdaw::persistence::storedAssetPathForHash (
            bundle, stored.prepared.project().assets.front().contentHash);
    }
    SECTION ("missing bytes")
    {
        REQUIRE (std::filesystem::remove (assetPath));
    }
    SECTION ("changed bytes with the same length")
    {
        std::fstream damaged (assetPath, std::ios::binary | std::ios::in | std::ios::out);
        REQUIRE (damaged.good());
        damaged.seekp (-1, std::ios::end);
        damaged.put ('X');
        damaged.close();
        REQUIRE (damaged.good());
    }
    auto failed = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
    REQUIRE_FALSE (failed.assets.has_value());
    REQUIRE (failed.failureReason.find (assetPath.filename().string()) != std::string::npos);
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "original.yesdaw").ok());
    const auto originalId = model.project().id;
    REQUIRE_FALSE (model.loadProjectBundle (bundle, {}).ok());
    REQUIRE (model.project().id == originalId);
    REQUIRE (model.bundlePath() == directory / "original.yesdaw");
}

TEST_CASE ("prepared empty project keeps the existing transport-only open behavior",
           "[ui][input][project-load][prepared-project-load]")
{
    const auto directory = loadTestDirectory();
    const auto bundle = directory / "empty.yesdaw";
    {
        yesdaw::ui::UiAppModel source;
        REQUIRE (source.createProjectBundle (bundle).ok());
    }
    auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
    REQUIRE (stored.assets.has_value());
    REQUIRE (stored.assets->empty());
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.openPreparedProjectBundle (std::move (stored.prepared)).ok());
    REQUIRE (model.project().tracks.size() == 1u);
    REQUIRE (model.project().clips.empty());
    REQUIRE (model.playbackReady());
    REQUIRE (model.context().commandDispatchCount == 1u);
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
    const auto rendered = model.renderPlaybackFrames (512, 128);
    REQUIRE (rendered.size() == 1024u);
    REQUIRE (std::all_of (rendered.begin(), rendered.end(), [] (float sample) { return sample == 0.0f; }));
}

// G4.6 / ADR-0052, the model half of "you hear a ride while it lasts": a lane holds the fader at the bottom;
// suspending it and posting the ride's value makes the playback loud before anything is committed; resuming
// lets the lane hold the fader down again.
TEST_CASE ("a ride's suspension and live value reach the running engine", "[ui][automation][automation-v2]")
{
    const auto directory = loadTestDirectory();
    const auto wav = directory / "ride-source.wav";
    std::vector<float> samples (192'000);
    for (std::size_t frame = 0; frame < samples.size(); ++frame)
        samples[frame] = 0.5f * std::sin (2.0f * 3.14159265f * 440.0f * static_cast<float> (frame) / 48'000.0f);
    REQUIRE (yesdaw::io::writeFloat32WavFile (wav, yesdaw::engine::SampleRate { 48000.0 }, 1, samples.size(), samples).ok());
    auto decoded = yesdaw::ui::shell::decodeProjectWav (wav);
    REQUIRE (decoded.has_value());
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "ride.yesdaw").ok());
    REQUIRE (model.importAudioFile (wav, std::move (*decoded)).ok());
    REQUIRE (model.setAutomationMode (yesdaw::engine::AutomationMode::Touch).dispatched);
    const yesdaw::engine::EntityId trackId = model.project().clips.front().trackId;
    const auto peak = [&model] (std::uint64_t frames) {
        const std::vector<float> rendered = model.renderPlaybackFrames (frames, 128);
        float p = 0.0f;
        for (const float v : rendered)
            p = std::max (p, std::abs (v));
        return p;
    };
    constexpr auto kFader = yesdaw::engine::AutomationTargetRole::TrackFader;
    constexpr std::uint32_t kGain = yesdaw::engine::FaderNode::kGainParameterId;

    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
    const float baseline = peak (4'800);
    REQUIRE (baseline > 0.2f);

    // A lane holding the fader at the bottom (its one point holds everywhere).
    REQUIRE (model.commitAutomationTouchRide (trackId, kFader, kGain, { { 0, 0.0 } }).dispatched);
    REQUIRE (model.context().isPlaying);
    (void) peak (2'400);
    REQUIRE (peak (4'800) < baseline * 0.05f);

    // The ride: suspend the lane, post the ride's value — heard on the running engine.
    const std::uint64_t appliedBefore = model.playbackLiveScalarsApplied();
    REQUIRE (model.setAutomationRideSuspended (trackId, kFader, kGain, true));
    REQUIRE (model.postAutomationRideValue (trackId, kFader, kGain, 1.0));
    (void) peak (2'400);
    INFO ("applied " << model.playbackLiveScalarsApplied() - appliedBefore);
    REQUIRE (model.playbackLiveScalarsApplied() == appliedBefore + 2u);
    REQUIRE (peak (4'800) > baseline * 0.5f);

    // Released: the lane holds the fader down again.
    REQUIRE (model.setAutomationRideSuspended (trackId, kFader, kGain, false));
    (void) peak (2'400);
    REQUIRE (peak (4'800) < baseline * 0.05f);
}

// G4.6 / ADR-0052: a pass whose lanes would exceed the engine's per-block event budget is refused whole, with
// the reason on the status line — never thinned or dropped silently, and the project and undo stay as they were.
TEST_CASE ("a pass too dense for the engine's event budget is refused whole with a reason", "[ui][automation][automation-v2]")
{
    const auto directory = loadTestDirectory();
    const auto wav = directory / "dense-source.wav";
    const std::vector<float> samples (48'000, 0.25f);
    REQUIRE (yesdaw::io::writeFloat32WavFile (wav, yesdaw::engine::SampleRate { 48000.0 }, 1, samples.size(), samples).ok());
    auto decoded = yesdaw::ui::shell::decodeProjectWav (wav);
    REQUIRE (decoded.has_value());
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "dense.yesdaw").ok());
    REQUIRE (model.importAudioFile (wav, std::move (*decoded)).ok());
    model.setPlaybackMaxBlockSize (4'096);   // one 4 096-frame block can hold 2 600+ breakpoints a tick apart
    const yesdaw::engine::EntityId trackId = model.project().clips.front().trackId;
    constexpr auto kFader = yesdaw::engine::AutomationTargetRole::TrackFader;
    constexpr std::uint32_t kGain = yesdaw::engine::FaderNode::kGainParameterId;

    REQUIRE (model.commitAutomationTouchRide (trackId, kFader, kGain, { { 0, 0.5 }, { 15'360, 0.6 } }).dispatched);
    const auto before = model.project().automationLanes;

    std::vector<yesdaw::ui::UiAppModel::AutomationTouchSample> dense;
    for (yesdaw::engine::Tick tick = 20'000; tick < 24'000; ++tick)
        dense.push_back ({ tick, 0.5 + 0.001 * static_cast<double> (tick % 100) });
    const auto refused = model.commitAutomationTouchRide (trackId, kFader, kGain, dense);
    REQUIRE_FALSE (refused.dispatched);
    REQUIRE (model.project().automationLanes == before);
    REQUIRE (model.statusLineIsError());
    REQUIRE (model.statusLineText().find ("too dense") != std::string::npos);

    // The undo history is untouched: one undo removes the first pass.
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::EditUndo).dispatched);
    REQUIRE (model.project().automationLanes.empty());
}

// G4.6 / ADR-0052: Automation Follows Clips — off, a clip move leaves the lane alone; on (an undoable project
// setting in the Edit menu), the clip carries the automation in its span, and one undo restores both.
TEST_CASE ("with Automation Follows Clips on, a moved clip carries its automation; one undo restores both",
           "[ui][automation][automation-v2][follow-clips]")
{
    const auto directory = loadTestDirectory();
    const auto wav = directory / "follow-source.wav";
    const std::vector<float> samples (96'000, 0.25f);   // 2 s
    REQUIRE (yesdaw::io::writeFloat32WavFile (wav, yesdaw::engine::SampleRate { 48000.0 }, 1, samples.size(), samples).ok());
    auto decoded = yesdaw::ui::shell::decodeProjectWav (wav);
    REQUIRE (decoded.has_value());
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "follow.yesdaw").ok());
    REQUIRE (model.importAudioFile (wav, std::move (*decoded)).ok());
    const yesdaw::engine::Clip clip = model.project().clips.front();
    REQUIRE (clip.timeBase == yesdaw::engine::TimeBase::SampleLocked);
    REQUIRE (clip.timelineStart == 0);

    // A fader lane: 0.2 at the start, a 0.9 peak at tick 30 720 (1 s at 120 BPM) inside the clip, 0.4 later.
    REQUIRE (model.commitAutomationTouchRide (clip.trackId, yesdaw::engine::AutomationTargetRole::TrackFader,
                                              yesdaw::engine::FaderNode::kGainParameterId,
                                              { { 0, 0.2 }, { 30'720, 0.9 }, { 153'600, 0.4 } }).dispatched);
    const auto lanesBefore = model.project().automationLanes;
    const auto hasPeakAt = [&model] (yesdaw::engine::Tick tick) {
        for (const auto& point : model.project().automationLanes.front().points)
            if (point.tick == tick && point.value == 0.9)
                return true;
        return false;
    };
    REQUIRE (hasPeakAt (30'720));

    // Off (the default): the move leaves the lane alone.
    REQUIRE_FALSE (model.project().automationFollowsClips);
    REQUIRE (model.selectTimelineClip (clip.id));
    REQUIRE (model.moveSelectedTimelineClipTo (48'000).dispatched);   // +1 s
    REQUIRE (model.project().automationLanes == lanesBefore);
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::EditUndo).dispatched);

    // On: the peak travels 1 s with the clip (tick 30 720 + 30 720 at 120 BPM).
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TimelineAutomationFollowsClipsToggle).dispatched);
    REQUIRE (model.project().automationFollowsClips);
    REQUIRE (model.context().automationFollowsClips);
    REQUIRE (model.selectTimelineClip (clip.id));
    REQUIRE (model.moveSelectedTimelineClipTo (48'000).dispatched);
    REQUIRE (model.project().clips.front().timelineStart == 48'000);
    REQUIRE (hasPeakAt (61'440));
    REQUIRE_FALSE (hasPeakAt (30'720));

    // One undo: the clip and its automation go back together.
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::EditUndo).dispatched);
    REQUIRE (model.project().clips.front().timelineStart == 0);
    REQUIRE (model.project().automationLanes == lanesBefore);
    REQUIRE (model.project().automationFollowsClips);   // the setting is its own step
}

// ---- G4.6 critic pass 2: Automation Follows Clips through every move path ------------------------------
namespace
{
// A model with one 2-second (96 000-frame) clip on track 1 at frame 0, follow-clips on; `wav` stays for more imports.
struct FollowModel
{
    std::filesystem::path wav;
    yesdaw::ui::UiAppModel model;

    explicit FollowModel (const char* name)
    {
        const auto directory = loadTestDirectory();
        wav = directory / (std::string (name) + ".wav");
        const std::vector<float> samples (96'000, 0.25f);
        REQUIRE (yesdaw::io::writeFloat32WavFile (wav, yesdaw::engine::SampleRate { 48000.0 }, 1, samples.size(), samples).ok());
        REQUIRE (model.createProjectBundle (directory / (std::string (name) + ".yesdaw")).ok());
        REQUIRE (model.importAudioFile (wav, decode()).ok());
        REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TimelineAutomationFollowsClipsToggle).dispatched);
    }

    yesdaw::ui::UiDecodedAsset decode() const
    {
        auto decoded = yesdaw::ui::shell::decodeProjectWav (wav);
        REQUIRE (decoded.has_value());
        return std::move (*decoded);
    }

    void lane (yesdaw::engine::EntityId track, std::vector<yesdaw::ui::UiAppModel::AutomationTouchSample> points)
    {
        REQUIRE (model.commitAutomationTouchRide (track, yesdaw::engine::AutomationTargetRole::TrackFader,
                                                  yesdaw::engine::FaderNode::kGainParameterId, points).dispatched);
    }

    bool peakAt (yesdaw::engine::EntityId track, yesdaw::engine::Tick tick, double value) const
    {
        for (const auto& lane : model.project().automationLanes)
            if (lane.ownerEntity == track)
                for (const auto& point : lane.points)
                    if (point.tick == tick && point.value == value)
                        return true;
        return false;
    }

    const yesdaw::engine::Clip& clip (std::size_t index) const { return model.project().clips.at (index); }
};

constexpr yesdaw::engine::Tick ticksForFrames (yesdaw::engine::Tick frames) { return frames * 15'360 / 24'000; }   // 48 kHz, 120 BPM
} // namespace

TEST_CASE ("follow-clips: a neighbour the Shuffle edit mode moves carries its own automation", "[ui][automation][automation-v2][follow-clips]")
{
    FollowModel f ("follow-shuffle");
    const yesdaw::engine::EntityId track = f.clip (0).trackId;
    REQUIRE (f.model.importAudioFileAt (f.wav, f.decode(), track, 96'000).ok());   // B right after A
    REQUIRE (f.model.project().clips.size() == 2u);
    const yesdaw::engine::Tick bBefore = f.clip (1).timelineStart;
    f.lane (track, { { 0, 0.5 }, { 92'160, 0.9 }, { 150'000, 0.3 } });   // the 0.9 peak sits inside B
    const auto lanesBefore = f.model.project().automationLanes;

    REQUIRE (f.model.dispatch (yesdaw::ui::UiActionId::EditModeShuffle).dispatched);
    REQUIRE (f.model.selectTimelineClip (f.clip (0).id));
    REQUIRE (f.model.moveSelectedTimelineClipTo (288'000).dispatched);   // A to the far right: its old place closes up
    const yesdaw::engine::Tick bMoved = f.clip (1).timelineStart - bBefore;
    INFO ("B moved " << bMoved << " frames");
    REQUIRE (bMoved != 0);                                               // Shuffle moved the neighbour
    REQUIRE (f.peakAt (track, 92'160 + ticksForFrames (bMoved), 0.9));   // and its automation went with it

    REQUIRE (f.model.dispatch (yesdaw::ui::UiActionId::EditUndo).dispatched);   // one step: clips and lanes
    REQUIRE (f.model.project().automationLanes == lanesBefore);
    REQUIRE (f.clip (1).timelineStart == bBefore);
}

TEST_CASE ("follow-clips: a move to another track leaves the automation alone", "[ui][automation][automation-v2][follow-clips]")
{
    FollowModel f ("follow-cross-track");
    const yesdaw::engine::EntityId track1 = f.clip (0).trackId;
    REQUIRE (f.model.addAudioTrack().dispatched);
    const yesdaw::engine::EntityId track2 = f.model.project().tracks.back().id;
    REQUIRE (! (track2 == track1));
    f.lane (track1, { { 0, 0.5 }, { 30'720, 0.9 } });
    const auto lanesBefore = f.model.project().automationLanes;

    REQUIRE (f.model.selectTimelineClip (f.clip (0).id));
    REQUIRE (f.model.moveSelectedTimelineClipToTrack (track2, 48'000).dispatched);
    REQUIRE (f.clip (0).trackId == track2);
    REQUIRE (f.model.project().automationLanes == lanesBefore);
}

TEST_CASE ("follow-clips: clips moved together on two tracks each carry their own track's automation", "[ui][automation][automation-v2][follow-clips]")
{
    FollowModel f ("follow-two-tracks");
    const yesdaw::engine::EntityId track1 = f.clip (0).trackId;
    REQUIRE (f.model.addAudioTrack().dispatched);
    const yesdaw::engine::EntityId track2 = f.model.project().tracks.back().id;
    REQUIRE (f.model.importAudioFileAt (f.wav, f.decode(), track2, 0).ok());
    REQUIRE (f.model.project().clips.size() == 2u);
    f.lane (track1, { { 0, 0.5 }, { 30'720, 0.9 } });
    f.lane (track2, { { 0, 0.4 }, { 15'360, 0.8 } });

    const std::array<yesdaw::engine::EntityId, 2> both { f.clip (0).id, f.clip (1).id };
    REQUIRE (f.model.selectTimelineClips (both));
    REQUIRE (f.model.moveSelectedTimelineClipTo (48'000).dispatched);
    REQUIRE (f.peakAt (track1, 30'720 + ticksForFrames (48'000), 0.9));
    REQUIRE (f.peakAt (track2, 15'360 + ticksForFrames (48'000), 0.8));
    REQUIRE_FALSE (f.peakAt (track1, 15'360 + ticksForFrames (48'000), 0.8));   // nothing crossed tracks
}

// ---- G4.7 / ADR-0053: the live loudness readout -------------------------------------------------------------------
namespace
{
// One 1-second clip at frame 0: a loud half (a 997 Hz sine at 0.5) then a quiet half (0.05, 20 dB down), so a
// measurement that wrongly kept the loud audio reads near the loud level (the quiet blocks fall under the
// relative gate) and one that restarted reads 20 LU lower.
struct LoudnessModel
{
    yesdaw::ui::UiAppModel model;

    LoudnessModel()
    {
        const auto directory = loadTestDirectory();
        const auto wav = directory / "loud-quiet.wav";
        std::vector<float> samples (48'000);
        for (std::size_t i = 0; i < samples.size(); ++i)
            samples[i] = (i < 24'000u ? 0.5f : 0.05f)
                         * static_cast<float> (std::sin (2.0 * 3.141592653589793 * 997.0 * static_cast<double> (i) / 48'000.0));
        REQUIRE (yesdaw::io::writeFloat32WavFile (wav, yesdaw::engine::SampleRate { 48000.0 }, 1, samples.size(), samples).ok());
        auto decoded = yesdaw::ui::shell::decodeProjectWav (wav);
        REQUIRE (decoded.has_value());
        REQUIRE (model.createProjectBundle (directory / "loud-quiet.yesdaw").ok());
        REQUIRE (model.importAudioFile (wav, std::move (*decoded)).ok());
    }

    std::vector<float> render (std::uint64_t frames)
    {
        std::vector<float> mix = model.renderPlaybackFrames (frames, 128);
        REQUIRE (mix.size() == frames * 2u);   // a stereo master
        return mix;
    }
};

struct Expected
{
    double integratedLufs = 0.0;
    double truePeakDbtp = 0.0;
};

Expected analyzed (const std::vector<float>& mix)
{
    const auto result = yesdaw::analysis::analyzeInterleavedLoudness (mix, 2, 48'000);
    REQUIRE (result.status == yesdaw::analysis::LoudnessStatus::Ok);
    return { result.metrics.integratedLufs, std::max (result.metrics.truePeakDbtp[0], result.metrics.truePeakDbtp[1]) };
}

void requireReadout (const yesdaw::ui::UiAppModel& model, const Expected& expected)
{
    const yesdaw::ui::UiMixerLoudnessReadout& live = model.liveLoudnessReadout();
    REQUIRE (live.valid);
    REQUIRE_FALSE (live.approximate);
    REQUIRE (live.integratedLufs == Catch::Approx (expected.integratedLufs).margin (0.1));
    REQUIRE (live.truePeakDbtp == Catch::Approx (expected.truePeakDbtp).margin (1.0e-4));
}
} // namespace

TEST_CASE ("ADR-0053 the live readout measures the mix since the listen began: play and locate restart it; stop holds",
           "[ui][mixer][loudness][loudness-live]")
{
    using yesdaw::ui::UiActionId;
    LoudnessModel f;
    REQUIRE_FALSE (f.model.liveLoudnessReadout().valid);   // nothing heard yet

    // Play from 0: 0.4 s of the loud half. The readout is exactly that audio's loudness and true peak.
    REQUIRE (f.model.dispatch (UiActionId::TransportPlay).dispatched);
    const std::vector<float> loud = f.render (19'200);
    f.model.serviceLiveLoudness();
    const Expected loudExpected = analyzed (loud);
    requireReadout (f.model, loudExpected);
    REQUIRE (f.model.liveLoudnessFrames() == 19'200u);

    // Stopped, the values hold — through a locate too (a locate only restarts a listen while playing).
    REQUIRE (f.model.dispatch (UiActionId::TransportStop).dispatched);
    f.model.serviceLiveLoudness();
    requireReadout (f.model, loudExpected);
    REQUIRE (f.model.locatePlaybackFrame (24'000));
    f.model.serviceLiveLoudness();
    requireReadout (f.model, loudExpected);

    // Play again: a fresh listen — cleared at once, then only the quiet half (20 LU below; a kept measurement
    // would read near the loud level).
    REQUIRE (f.model.dispatch (UiActionId::TransportPlay).dispatched);
    REQUIRE_FALSE (f.model.liveLoudnessReadout().valid);
    const std::vector<float> quiet = f.render (19'200);
    f.model.serviceLiveLoudness();
    requireReadout (f.model, analyzed (quiet));
    REQUIRE (f.model.liveLoudnessReadout().integratedLufs < loudExpected.integratedLufs - 15.0);

    // Located while playing (Home), then again (a ruler / marker locate): each one a fresh listen.
    REQUIRE (f.model.dispatch (UiActionId::TransportLocateStart).dispatched);
    REQUIRE_FALSE (f.model.liveLoudnessReadout().valid);
    const std::vector<float> loudAgain = f.render (19'200);
    f.model.serviceLiveLoudness();
    requireReadout (f.model, analyzed (loudAgain));
    REQUIRE (f.model.locatePlaybackFrame (24'000));
    REQUIRE_FALSE (f.model.liveLoudnessReadout().valid);
    const std::vector<float> quietAgain = f.render (19'200);
    f.model.serviceLiveLoudness();
    requireReadout (f.model, analyzed (quietAgain));
    REQUIRE (f.model.liveLoudnessReadout().integratedLufs < loudExpected.integratedLufs - 15.0);
    REQUIRE (f.model.liveLoudnessFrames() == 19'200u);
}

TEST_CASE ("ADR-0053 a loop wrap and an engine swap keep one listen; a dropped block marks the readout approximate",
           "[ui][mixer][loudness][loudness-live]")
{
    using yesdaw::ui::UiActionId;
    LoudnessModel f;
    REQUIRE (f.model.dispatch (UiActionId::TransportToggleLoop).dispatched);   // loops the clip: [0, 48 000)
    REQUIRE (f.model.dispatch (UiActionId::TransportPlay).dispatched);

    // 1.5 s across one wrap, serviced as a UI tick would: one continuous measurement of everything heard.
    std::vector<float> heard;
    for (int tick = 0; tick < 45; ++tick)
    {
        const std::vector<float> block = f.render (1'600);
        heard.insert (heard.end(), block.begin(), block.end());
        f.model.serviceLiveLoudness();
    }
    REQUIRE (f.model.liveLoudnessFrames() == 72'000u);
    requireReadout (f.model, analyzed (heard));

    // An edit that rebuilds the engine while playing (a new track) keeps feeding the same listen.
    const std::uint64_t rebuilds = f.model.playbackReplaceCount();
    REQUIRE (f.model.dispatch (UiActionId::TrackAdd).dispatched);
    REQUIRE (f.model.playbackReplaceCount() == rebuilds + 1);
    REQUIRE (f.model.context().isPlaying);
    const std::vector<float> afterSwap = f.render (9'600);
    heard.insert (heard.end(), afterSwap.begin(), afterSwap.end());
    f.model.serviceLiveLoudness();
    REQUIRE (f.model.liveLoudnessFrames() == 81'600u);
    requireReadout (f.model, analyzed (heard));

    // The UI stalls past the ring (8 s at 48 kHz stereo): whole blocks are dropped and counted, and the readout
    // says so ("~") until the next listen.
    REQUIRE (f.model.liveLoudnessOverflowCount() == 0u);
    (void) f.render (480'000);
    REQUIRE (f.model.liveLoudnessOverflowCount() > 0u);
    f.model.serviceLiveLoudness();
    REQUIRE (f.model.liveLoudnessReadout().valid);
    REQUIRE (f.model.liveLoudnessReadout().approximate);
    REQUIRE (f.model.liveLoudnessFrames() < 81'600u + 480'000u);

    REQUIRE (f.model.dispatch (UiActionId::TransportLocateStart).dispatched);   // a fresh listen
    const std::vector<float> fresh = f.render (19'200);
    f.model.serviceLiveLoudness();
    requireReadout (f.model, analyzed (fresh));

    // Shuttle (2x) is a scrub, not a listen: the engine does not feed the tap, so the readout holds.
    REQUIRE (f.model.dispatch (UiActionId::TransportShuttleFaster).dispatched);
    REQUIRE (f.model.context().shuttlePlaybackRate == 2);
    (void) f.render (9'600);
    f.model.serviceLiveLoudness();
    REQUIRE (f.model.liveLoudnessFrames() == 19'200u);
    requireReadout (f.model, analyzed (fresh));
}

// ADR-0053: the monitor stage's ramp — linear over 5 ms (240 frames at 48 kHz) from wherever the gain is to each
// new target; Mute wins over Dim and Dim under Mute only moves the target Mute releases to; unity is bit-exact.
TEST_CASE ("ADR-0053 the monitor stage ramps 5 ms to Dim and Mute targets and leaves unity untouched",
           "[ui][monitor][g47]")
{
    using yesdaw::ui::UiActionId;
    LoudnessModel f;   // a 48 kHz project, so the ramp is 240 frames
    auto& model = f.model;
    const bool canUndoBefore = model.context().canUndo;
    const bool dirtyBefore = model.hasUnsavedChanges();

    // Ones through the stage show the gain sample by sample.
    const auto pass = [&model] (int frames) {
        std::vector<float> left (static_cast<std::size_t> (frames), 1.0f);
        std::vector<float> right (static_cast<std::size_t> (frames), 1.0f);
        float* channels[2] = { left.data(), right.data() };
        model.applyMonitorStage (channels, 2, frames);
        REQUIRE (left == right);
        return left;
    };
    const auto requireRamp = [] (const std::vector<float>& gains, float from, float to) {
        REQUIRE (gains.size() >= 240u);
        for (std::size_t i = 0; i < 239u; ++i)
            REQUIRE (gains[i] == Catch::Approx (from + (to - from) * static_cast<float> (i + 1) / 240.0f).margin (1.0e-5));
        for (std::size_t i = 239u; i < gains.size(); ++i)
            REQUIRE (gains[i] == to);   // lands exactly and stays
    };
    const float dim = std::pow (10.0f, -20.0f / 20.0f);

    REQUIRE (pass (64) == std::vector<float> (64, 1.0f));   // unity: untouched

    REQUIRE (model.dispatch (UiActionId::MasterMonitorDimToggle).dispatched);
    REQUIRE (model.context().monitorDimmed);
    requireRamp (pass (300), 1.0f, dim);

    // Mute while dimmed ramps from the dimmed gain to silence; Dim off under Mute changes nothing heard.
    REQUIRE (model.dispatch (UiActionId::MasterMonitorMuteToggle).dispatched);
    requireRamp (pass (300), dim, 0.0f);
    REQUIRE (model.dispatch (UiActionId::MasterMonitorDimToggle).dispatched);
    REQUIRE_FALSE (model.context().monitorDimmed);
    REQUIRE (pass (64) == std::vector<float> (64, 0.0f));

    // Mute off releases to the target Dim left it — unity now — and a change mid-ramp restarts from where it is.
    REQUIRE (model.dispatch (UiActionId::MasterMonitorMuteToggle).dispatched);
    const std::vector<float> half = pass (120);
    REQUIRE (half.back() == Catch::Approx (0.5f).margin (1.0e-5));
    REQUIRE (model.dispatch (UiActionId::MasterMonitorDimToggle).dispatched);   // 0.5 -> dim over a fresh 240
    requireRamp (pass (300), half.back(), dim);
    REQUIRE (model.dispatch (UiActionId::MasterMonitorDimToggle).dispatched);
    requireRamp (pass (300), dim, 1.0f);
    REQUIRE (pass (64) == std::vector<float> (64, 1.0f));   // back to untouched

    // Session state: no edit, nothing to undo, nothing to save.
    REQUIRE (model.context().canUndo == canUndoBefore);
    REQUIRE (model.hasUnsavedChanges() == dirtyBefore);
}

// Copy and paste carry the whole clip: a stretched, reversed, muted, coloured clip pastes as itself (the
// paste used to rebuild the clip from a few fields and drop the rest — a stretched clip came back unstretched).
TEST_CASE ("copy and paste keep every clip setting", "[ui][timeline][clipboard]")
{
    using yesdaw::ui::UiActionId;
    LoudnessModel f;
    auto& model = f.model;
    REQUIRE (model.setSelectedTimelineClipStretchFactor (1.5f).dispatched);
    REQUIRE (model.dispatch (UiActionId::TimelineClipReverse).dispatched);
    REQUIRE (model.dispatch (UiActionId::TimelineClipToggleMute).dispatched);
    REQUIRE (model.dispatch (UiActionId::TimelineClipColourNext).dispatched);
    const yesdaw::engine::Clip original = model.project().clips.front();
    REQUIRE (original.stretchFactor == 1.5f);
    REQUIRE (original.reversed);
    REQUIRE (original.muted);

    REQUIRE (model.dispatch (UiActionId::TimelineClipCopy).dispatched);
    REQUIRE (model.locatePlaybackFrame (200'000));
    REQUIRE (model.dispatch (UiActionId::TimelineClipPaste).dispatched);
    REQUIRE (model.project().clips.size() == 2u);
    const yesdaw::engine::Clip pasted = model.project().clips.back();
    REQUIRE (pasted.id != original.id);
    REQUIRE (pasted.timelineStart != original.timelineStart);
    REQUIRE (pasted.stretchFactor == original.stretchFactor);
    REQUIRE (pasted.reversed == original.reversed);
    REQUIRE (pasted.muted == original.muted);
    REQUIRE (pasted.colour == original.colour);
    REQUIRE (pasted.fadeInShape == original.fadeInShape);
    REQUIRE (pasted.timelineLength == original.timelineLength);
    REQUIRE (pasted.srcLen == original.srcLen);
}
