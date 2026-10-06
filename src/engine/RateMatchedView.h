// YES DAW - ADR-0055: a cross-rate Asset's rate-matched view, and the engine's view of a project.
//
// A clip keeps its source window in Asset frames (ADR-0011). An Asset at another rate than the project's is
// read through a rate-matched view — its decode resampled to the project rate on the control thread — and the
// engine is built from `projectInViewFrames`: the same project with every cross-rate Asset's row at the project
// rate and the view's length, and every window on such an Asset mapped into view frames by its two ends (so a
// split's halves still abut exactly and nothing reads past the view). Pure; nothing here is ever saved.

#pragma once

#include "engine/ClipSchedule.h"
#include "engine/Project.h"
#include "engine/Resample.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>

namespace yesdaw::engine {

[[nodiscard]] inline bool assetNeedsRateMatchedView (const Asset& asset, SampleRate projectRate) noexcept
{
    return asset.sampleRate.hz != projectRate.hz;
}

// The rate ratio r = projectRate / assetRate (exactly 1 for a same-rate Asset).
[[nodiscard]] inline double rateRatio (double assetRateHz, double projectRateHz) noexcept
{
    return assetRateHz == projectRateHz ? 1.0 : projectRateHz / assetRateHz;
}

// An Asset frame (a window end or a point) in project / view frames: round (frame × r), half away from zero.
[[nodiscard]] inline std::uint64_t viewFrameFor (std::uint64_t assetFrame, double r) noexcept
{
    return r == 1.0 ? assetFrame
                    : static_cast<std::uint64_t> (std::llround (static_cast<double> (assetFrame) * r));
}

// A timeline length in ticks (project frames) for a source length in Asset frames: round (srcLen × r × stretch).
[[nodiscard]] inline Tick timelineLengthForSource (std::uint64_t srcLen, double r, double stretchFactor = 1.0) noexcept
{
    return static_cast<Tick> (std::llround (static_cast<double> (srcLen) * r * stretchFactor));
}

struct ViewWindow
{
    std::uint64_t srcOffset = 0;
    std::uint64_t srcLen = 0;
};

// A source window mapped into view frames by its two ends, clamped to the view's length.
[[nodiscard]] inline ViewWindow mapWindowToView (std::uint64_t srcOffset, std::uint64_t srcLen, double r,
                                                 std::uint64_t viewFrames) noexcept
{
    const std::uint64_t start = std::min (viewFrameFor (srcOffset, r), viewFrames);
    const std::uint64_t end = std::min (viewFrameFor (srcOffset + srcLen, r), viewFrames);
    return { start, end - start };
}

// The view's samples for one Asset decode (control thread / worker only).
[[nodiscard]] inline std::shared_ptr<const AssetSamples> buildRateMatchedSamples (std::span<const float> interleaved,
                                                                                  std::uint16_t channels,
                                                                                  double assetRateHz,
                                                                                  double projectRateHz,
                                                                                  ResampleQuality quality)
{
    auto view = std::make_shared<AssetSamples>();
    view->channels = std::max<int> (1, channels);
    view->interleaved = resampleInterleaved (interleaved, channels, assetRateHz, projectRateHz, quality);
    view->frames = channels > 0u ? view->interleaved.size() / channels : 0u;
    return view;
}

[[nodiscard]] inline bool projectHasCrossRateAssets (const Project& project) noexcept
{
    return std::any_of (project.assets.begin(), project.assets.end(),
                        [&project] (const Asset& asset) { return assetNeedsRateMatchedView (asset, project.sampleRate); });
}

// The project an engine or a render is built from (ADR-0055). A project with no cross-rate Asset comes back equal.
[[nodiscard]] inline Project projectInViewFrames (const Project& project)
{
    Project out = project;
    for (Asset& asset : out.assets)
    {
        if (! assetNeedsRateMatchedView (asset, project.sampleRate))
            continue;
        const double r = rateRatio (asset.sampleRate.hz, project.sampleRate.hz);
        const std::uint64_t viewFrames = resampledFrameCount (asset.frames, asset.sampleRate.hz, project.sampleRate.hz);
        for (Clip& clip : out.clips)
        {
            if (clip.assetId != asset.id)
                continue;
            const ViewWindow window = mapWindowToView (clip.srcOffset, clip.srcLen, r, viewFrames);
            clip.srcOffset = window.srcOffset;
            clip.srcLen = window.srcLen;
        }
        for (RecordingTake& take : out.recordingTakes)
        {
            if (take.assetId != asset.id)
                continue;
            const std::uint64_t takeFrames = mapWindowToView (0, take.frameCount, r, viewFrames).srcLen;
            for (ProjectRecordingCompSegment& segment : out.recordingCompSegments)
                if (segment.takeId == take.id)
                    segment.sourceOffset = std::min (viewFrameFor (segment.sourceOffset, r), takeFrames);
            take.frameCount = takeFrames;
        }
        asset.frames = viewFrames;
        asset.sampleRate = project.sampleRate;
    }
    return out;
}

} // namespace yesdaw::engine
