#include "ui/MainComponentInternal.h"

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
        stored.assets->front().interleavedSamples.front() = std::numeric_limits<float>::quiet_NaN();
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
