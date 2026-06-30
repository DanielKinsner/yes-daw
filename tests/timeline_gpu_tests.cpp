// YES DAW - H11 Timeline canvas frame-time gate.
//
// This is a self-asserting offscreen JUCE paint harness: it scrolls a dense arrangement fixture through
// the same Timeline canvas renderer used by the app shell and fails if sustained measured paint exceeds
// the 16.6 ms frame budget. One outlier is tolerated so shared CI scheduler pauses do not masquerade as
// renderer regressions.

#include "ui/TimelineCanvas.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

using yesdaw::ui::Clip;
using yesdaw::ui::TimelineCanvasClipStyle;
using yesdaw::ui::TimelineCanvasPaintStats;
using yesdaw::ui::TimelineCanvasState;
using yesdaw::ui::TimelineCanvasTrack;
using yesdaw::ui::paintTimelineCanvas;

namespace {

const juce::Colour kTrackColours[] = {
    juce::Colour (0xff3b8cff),
    juce::Colour (0xff1bb5a6),
    juce::Colour (0xffd29118),
    juce::Colour (0xffa762f0),
    juce::Colour (0xff20c8d8),
    juce::Colour (0xff74df35)
};

std::vector<TimelineCanvasTrack> makeTracks (int count)
{
    std::vector<TimelineCanvasTrack> tracks;
    tracks.reserve (static_cast<std::size_t> (count));
    for (int i = 0; i < count; ++i)
        tracks.push_back ({ "Track", kTrackColours[i % 6], 0.35f + static_cast<float> (i % 8) * 0.07f });
    return tracks;
}

void makeClips (int lanes, int clipsPerLane, std::vector<Clip>& clips,
                std::vector<TimelineCanvasClipStyle>& styles)
{
    clips.reserve (static_cast<std::size_t> (lanes * clipsPerLane));
    styles.reserve (static_cast<std::size_t> (lanes * clipsPerLane));

    int id = 0;
    for (int lane = 0; lane < lanes; ++lane)
    {
        for (int clipIndex = 0; clipIndex < clipsPerLane; ++clipIndex)
        {
            const double start = static_cast<double> (clipIndex) * 3.0
                               + static_cast<double> ((lane + clipIndex) % 5) * 0.18;
            const double length = 1.15 + static_cast<double> ((lane * 3 + clipIndex) % 7) * 0.18;
            clips.push_back ({ id, lane, start, length });
            styles.push_back ({ kTrackColours[lane % 6], 0.38f + static_cast<float> ((id % 9)) * 0.06f });
            ++id;
        }
    }
}

int countDifferentSamples (const juce::Image& image)
{
    const auto first = image.getPixelAt (0, 0).getARGB();
    int different = 0;
    for (int y = 24; y < image.getHeight(); y += 48)
        for (int x = 24; x < image.getWidth(); x += 96)
            if (image.getPixelAt (x, y).getARGB() != first)
                ++different;
    return different;
}

} // namespace

TEST_CASE ("Timeline canvas scrolls a large arrangement under one 60 fps frame", "[timeline][gpu][perf]")
{
    constexpr int kWidth = 1920;
    constexpr int kHeight = 720;
    constexpr int kLanes = 48;
    constexpr int kClipsPerLane = 430;
    constexpr int kWarmupFrames = 24;
    constexpr int kMeasuredFrames = 160;
    constexpr double kFrameBudgetMs = 16.6;

    auto tracks = makeTracks (kLanes);
    std::vector<Clip> clips;
    std::vector<TimelineCanvasClipStyle> styles;
    makeClips (kLanes, kClipsPerLane, clips, styles);

    juce::Image image (juce::Image::ARGB, kWidth, kHeight, true);

    TimelineCanvasState state;
    state.tracks = tracks.data();
    state.trackCount = static_cast<int> (tracks.size());
    state.clips = clips.data();
    state.clipStyles = styles.data();
    state.clipCount = static_cast<int> (clips.size());
    state.viewport.pixelsPerSecond = 100.0;
    state.totalSeconds = static_cast<double> (kClipsPerLane) * 3.0;

    const auto paintFrame = [&] {
        juce::Graphics graphics (image);
        return paintTimelineCanvas (graphics, image.getBounds(), state);
    };

    TimelineCanvasPaintStats lastStats;
    for (int frame = 0; frame < kWarmupFrames; ++frame)
    {
        state.viewport.scrollSeconds = static_cast<double> (frame) * 1.5;
        state.playheadSeconds = state.viewport.scrollSeconds + 8.0;
        lastStats = paintFrame();
    }

    double maxFrameMs = 0.0;
    int maxVisibleClips = 0;
    int slowFrameCount = 0;
    bool hitCapacity = false;
    std::uint64_t checksum = 0;
    std::vector<double> frameTimes;
    frameTimes.reserve (kMeasuredFrames);

    for (int frame = 0; frame < kMeasuredFrames; ++frame)
    {
        state.viewport.scrollSeconds = static_cast<double> (frame) * 2.0;
        state.playheadSeconds = state.viewport.scrollSeconds + 8.0;

        const auto t0 = std::chrono::steady_clock::now();
        lastStats = paintFrame();
        const auto t1 = std::chrono::steady_clock::now();

        const double frameMs = std::chrono::duration<double, std::milli> (t1 - t0).count();
        frameTimes.push_back (frameMs);
        maxFrameMs = std::max (maxFrameMs, frameMs);
        if (frameMs >= kFrameBudgetMs)
            ++slowFrameCount;
        maxVisibleClips = std::max (maxVisibleClips, lastStats.visibleClips);
        hitCapacity = hitCapacity || lastStats.hitVisibleClipCapacity;
        checksum += image.getPixelAt ((frame * 37) % kWidth, (frame * 53) % kHeight).getARGB();
    }

    std::sort (frameTimes.begin(), frameTimes.end());
    const auto p99Index = static_cast<std::size_t> (std::max (0, kMeasuredFrames - 2));
    const double p99FrameMs = frameTimes[p99Index];

    INFO ("max_frame_ms=" << maxFrameMs << ", p99_frame_ms=" << p99FrameMs
                          << ", slow_frames=" << slowFrameCount
                          << ", max_visible_clips=" << maxVisibleClips
                          << ", total_clips=" << clips.size() << ", checksum=" << checksum);
    REQUIRE (maxVisibleClips >= 250);
    REQUIRE_FALSE (hitCapacity);
    REQUIRE (countDifferentSamples (image) >= 20);
    REQUIRE (p99FrameMs < kFrameBudgetMs);
    REQUIRE (slowFrameCount <= 1);
}
