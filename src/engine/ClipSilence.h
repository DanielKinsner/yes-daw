// YES DAW - G2.13: silence detection over a Clip's source window (control side, pure).
//
// A silent run is a stretch of frames whose peak across channels stays under `threshold`
// for at least `minRunFrames`. Strip Silence turns the runs into split + delete edits in ONE
// undo step; nothing here touches the audio thread or the asset.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace yesdaw::engine {

struct SilentRun
{
    std::uint64_t start = 0;   // first silent frame
    std::uint64_t end = 0;     // one past the last silent frame
};

[[nodiscard]] inline std::vector<SilentRun> detectSilentRuns (std::span<const float> interleaved,
                                                              int channels,
                                                              std::uint64_t frames,
                                                              float threshold,
                                                              std::uint64_t minRunFrames)
{
    std::vector<SilentRun> runs;
    if (channels <= 0 || frames == 0 || ! std::isfinite (threshold) || threshold <= 0.0f)
        return runs;
    const auto stride = static_cast<std::size_t> (channels);
    if (interleaved.size() < static_cast<std::size_t> (frames) * stride)
        return runs;

    std::uint64_t runStart = 0;
    bool inRun = false;
    for (std::uint64_t frame = 0; frame < frames; ++frame)
    {
        float peak = 0.0f;
        const std::size_t base = static_cast<std::size_t> (frame) * stride;
        for (std::size_t c = 0; c < stride; ++c)
            peak = std::max (peak, std::abs (interleaved[base + c]));
        const bool silent = std::isfinite (peak) && peak < threshold;
        if (silent && ! inRun)
        {
            inRun = true;
            runStart = frame;
        }
        else if (! silent && inRun)
        {
            inRun = false;
            if (frame - runStart >= minRunFrames)
                runs.push_back ({ runStart, frame });
        }
    }
    if (inRun && frames - runStart >= minRunFrames)
        runs.push_back ({ runStart, frames });
    return runs;
}

} // namespace yesdaw::engine
