// YES DAW — reusable headless dense-Timeline frame measurement (H17 packaged verifier, U2).
//
// One measurement core shared by two consumers so they cannot drift:
//   - tests/timeline_gpu_tests.cpp   (the H11 Catch2 regression gate, with its CI outlier tolerance)
//   - tools/hardware/FrameCheckMain.cpp (the packaged YesDawFrameCheck stage checker, whose owner
//     policy is FIXED in src/app/HardwareVerification.h and never reads ambient CI)
//
// This header only MEASURES: it builds the deterministic dense arrangement fixture, scrolls it
// through the same offscreen TimelineCanvas paint path the app shell uses, and reports raw numbers
// (per-frame paint times, visible-clip stats, image-sampling counts). What those numbers MEAN —
// budgets, outliers, failure codes — is the verdict policy's business, not this file's.
//
// The evidence this produces is the accepted headless dense-Timeline proxy. It is NOT a real
// window / GPU presentation proof, and nothing here may be labeled as one (R18-R19).

#pragma once

#include "ui/TimelineCanvas.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

namespace yesdaw::ui {

struct TimelineFrameCheckConfig
{
    // The dense arrangement fixture the H11 gate has always used; the packaged checker runs these
    // exact defaults. Tests may shrink them to exercise the verdict policy on degenerate fixtures.
    int    width = 1920;
    int    height = 720;
    int    lanes = 48;
    int    clipsPerLane = 430;
    int    warmupFrames = 24;
    int    measuredFrames = 160;
    double pixelsPerSecond = 100.0;
};

struct TimelineFrameCheckResult
{
    std::vector<double> frameTimesMs;          // one entry per measured frame, unsorted
    double        maxFrameMs = 0.0;
    int           maxVisibleClips = 0;
    int           visibleClipCapacity = 0;
    bool          hitVisibleClipCapacity = false;
    int           distinctSamples = 0;         // grid-sampled distinct pixels of the final frame
    std::uint64_t checksum = 0;                // stable anti-elision digest of sampled pixels
    int           totalClips = 0;
};

namespace framecheck_detail {

// The historical fixture palette, expressed as UiTheme tokens (this file lives inside the H16
// theme-audit boundary, and five of the six original literals WERE these tokens; the sixth was a
// near-duplicate of accentPurple and is folded into it — fixture pixels carry no golden).
inline juce::Colour trackColour (int index)
{
    switch (index % 6)
    {
        case 0:  return UiTheme::Color::accentBlue();
        case 1:  return UiTheme::Color::accentTeal();
        case 2:  return UiTheme::Color::accentAmber();
        case 3:  return UiTheme::Color::accentPurple();
        case 4:  return UiTheme::Color::accentCyan();
        default: return UiTheme::Color::meterGreen();
    }
}

inline std::vector<TimelineCanvasTrack> makeTracks (int count)
{
    std::vector<TimelineCanvasTrack> tracks;
    tracks.reserve (static_cast<std::size_t> (count));
    for (int i = 0; i < count; ++i)
        tracks.push_back ({ "Track", trackColour (i), 0.35f + static_cast<float> (i % 8) * 0.07f });
    return tracks;
}

inline void makeClips (int lanes, int clipsPerLane, std::vector<Clip>& clips,
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
            styles.push_back ({ trackColour (lane), 0.38f + static_cast<float> ((id % 9)) * 0.06f });
            ++id;
        }
    }
}

inline int countDistinctSamples (const juce::Image& image)
{
    const auto first = image.getPixelAt (0, 0).getARGB();
    int different = 0;
    for (int y = 24; y < image.getHeight(); y += 48)
        for (int x = 24; x < image.getWidth(); x += 96)
            if (image.getPixelAt (x, y).getARGB() != first)
                ++different;
    return different;
}

} // namespace framecheck_detail

// Sustained (~95th-percentile-style) frame time: the worst frame AFTER discarding the allowed
// outliers. Callers choose the outlier count — the Catch2 gate relaxes it on shared CI runners,
// the packaged owner policy never does.
[[nodiscard]] inline double sustainedFrameMs (std::vector<double> frameTimesMs, int allowedOutlierFrames)
{
    if (frameTimesMs.empty())
        return 0.0;
    std::sort (frameTimesMs.begin(), frameTimesMs.end());
    const auto index = static_cast<std::size_t> (
        std::max (0, static_cast<int> (frameTimesMs.size()) - allowedOutlierFrames - 1));
    return frameTimesMs[index];
}

[[nodiscard]] inline int countFramesAtOrOverBudget (const std::vector<double>& frameTimesMs, double budgetMs)
{
    int slow = 0;
    for (const double ms : frameTimesMs)
        if (ms >= budgetMs)
            ++slow;
    return slow;
}

[[nodiscard]] inline TimelineFrameCheckResult runTimelineFrameCheck (const TimelineFrameCheckConfig& config)
{
    auto tracks = framecheck_detail::makeTracks (config.lanes);
    std::vector<Clip> clips;
    std::vector<TimelineCanvasClipStyle> styles;
    framecheck_detail::makeClips (config.lanes, config.clipsPerLane, clips, styles);

    juce::Image image (juce::Image::ARGB, config.width, config.height, true);

    TimelineCanvasState state;
    state.tracks = tracks.data();
    state.trackCount = static_cast<int> (tracks.size());
    state.clips = clips.data();
    state.clipStyles = styles.data();
    state.clipCount = static_cast<int> (clips.size());
    state.viewport.pixelsPerSecond = config.pixelsPerSecond;
    state.totalSeconds = static_cast<double> (config.clipsPerLane) * 3.0;

    const auto paintFrame = [&image, &state] {
        juce::Graphics graphics (image);
        return paintTimelineCanvas (graphics, image.getBounds(), state);
    };

    TimelineCanvasPaintStats lastStats;
    for (int frame = 0; frame < config.warmupFrames; ++frame)
    {
        state.viewport.scrollSeconds = static_cast<double> (frame) * 1.5;
        state.playheadSeconds = state.viewport.scrollSeconds + 8.0;
        lastStats = paintFrame();
    }

    TimelineFrameCheckResult result;
    result.totalClips = static_cast<int> (clips.size());
    result.frameTimesMs.reserve (static_cast<std::size_t> (config.measuredFrames));

    for (int frame = 0; frame < config.measuredFrames; ++frame)
    {
        state.viewport.scrollSeconds = static_cast<double> (frame) * 2.0;
        state.playheadSeconds = state.viewport.scrollSeconds + 8.0;

        const auto t0 = std::chrono::steady_clock::now();
        lastStats = paintFrame();
        const auto t1 = std::chrono::steady_clock::now();

        const double frameMs = std::chrono::duration<double, std::milli> (t1 - t0).count();
        result.frameTimesMs.push_back (frameMs);
        result.maxFrameMs = std::max (result.maxFrameMs, frameMs);
        result.maxVisibleClips = std::max (result.maxVisibleClips, lastStats.visibleClips);
        result.visibleClipCapacity = lastStats.visibleClipCapacity;
        result.hitVisibleClipCapacity = result.hitVisibleClipCapacity || lastStats.hitVisibleClipCapacity;
        result.checksum += image.getPixelAt ((frame * 37) % config.width,
                                             (frame * 53) % config.height).getARGB();
    }

    result.distinctSamples = framecheck_detail::countDistinctSamples (image);
    return result;
}

} // namespace yesdaw::ui
