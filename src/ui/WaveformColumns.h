// YES DAW - pure waveform column projection for UI painting (H16 CP3).

#pragma once

#include "persistence/WaveformPeakCache.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace yesdaw::ui {

struct WaveformColumn
{
    float min = 0.0f;
    float max = 0.0f;
    float rms = 0.0f;

    friend bool operator== (const WaveformColumn&, const WaveformColumn&) noexcept = default;
};

struct WaveformColumns
{
    std::size_t tierIndex = 0;
    std::uint32_t framesPerPeak = 0;
    std::vector<WaveformColumn> columns;

    friend bool operator== (const WaveformColumns&, const WaveformColumns&) noexcept = default;
};

// G2.19: decoded audio for the zoomed-in paint — the frames the peak cache's finest tier cannot
// resolve (below kWaveformPeakTier0Frames frames per pixel). Interleaved, `channels` wide.
struct WaveformSampleSource
{
    std::span<const float> interleavedSamples;
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;

    [[nodiscard]] bool valid() const noexcept
    {
        return channels != 0u && frames != 0u
            && interleavedSamples.size() >= static_cast<std::size_t> (frames) * channels;
    }
};

struct WaveformColumnViewport
{
    std::uint64_t sourceFrameOffset = 0;
    std::uint64_t sourceFrameCount = 0;
    double sampleRate = 0.0;
    double pixelsPerSecond = 0.0;
    int widthPixels = 0;
};

namespace waveform_columns_detail {

[[nodiscard]] inline std::size_t chooseTierIndex (const persistence::WaveformPeakCache& cache,
                                                  double framesPerPixel) noexcept
{
    std::size_t chosen = 0;
    for (std::size_t i = 0; i < cache.tiers.size(); ++i)
    {
        const auto framesPerPeak = cache.tiers[i].framesPerPeak;
        if (framesPerPeak == 0)
            break;

        if (static_cast<double> (framesPerPeak) <= framesPerPixel)
            chosen = i;
        else
            break;
    }

    return chosen;
}

[[nodiscard]] inline std::uint64_t ceilDiv (std::uint64_t value, std::uint64_t divisor) noexcept
{
    return value == 0u ? 0u : ((value - 1u) / divisor) + 1u;
}

[[nodiscard]] inline std::uint64_t columnStartFrame (std::uint64_t offset,
                                                     std::uint64_t frameCount,
                                                     int widthPixels,
                                                     int column) noexcept
{
    const long double scaled = (static_cast<long double> (column) * static_cast<long double> (frameCount))
                             / static_cast<long double> (widthPixels);
    return offset + static_cast<std::uint64_t> (std::floor (scaled));
}

[[nodiscard]] inline std::uint64_t columnEndFrame (std::uint64_t offset,
                                                   std::uint64_t frameCount,
                                                   int widthPixels,
                                                   int column) noexcept
{
    const long double scaled = (static_cast<long double> (column + 1) * static_cast<long double> (frameCount))
                             / static_cast<long double> (widthPixels);
    return offset + static_cast<std::uint64_t> (std::ceil (scaled));
}

} // namespace waveform_columns_detail

[[nodiscard]] inline WaveformColumns computeWaveformColumns (const persistence::WaveformPeakCache& cache,
                                                             const WaveformColumnViewport& viewport)
{
    WaveformColumns result;
    if (cache.sourceFrames == 0u
        || cache.channels == 0u
        || cache.tiers.empty()
        || viewport.sourceFrameCount == 0u
        || viewport.sourceFrameOffset >= cache.sourceFrames
        || ! std::isfinite (viewport.sampleRate)
        || viewport.sampleRate <= 0.0
        || ! std::isfinite (viewport.pixelsPerSecond)
        || viewport.pixelsPerSecond <= 0.0
        || viewport.widthPixels <= 0)
    {
        return result;
    }

    const std::uint64_t availableFrames = cache.sourceFrames - viewport.sourceFrameOffset;
    const std::uint64_t visibleFrames = std::min (viewport.sourceFrameCount, availableFrames);
    if (visibleFrames == 0u)
        return result;

    const double framesPerPixel = viewport.sampleRate / viewport.pixelsPerSecond;
    const std::size_t tierIndex = waveform_columns_detail::chooseTierIndex (cache, framesPerPixel);
    if (tierIndex >= cache.tiers.size())
        return result;

    const persistence::WaveformPeakTier& tier = cache.tiers[tierIndex];
    if (tier.framesPerPeak == 0u)
        return result;

    const std::uint64_t peakCount = static_cast<std::uint64_t> (tier.peaks.size()) / cache.channels;
    if (peakCount == 0u || tier.peaks.size() != static_cast<std::size_t> (peakCount) * cache.channels)
        return result;

    result.tierIndex = tierIndex;
    result.framesPerPeak = tier.framesPerPeak;
    result.columns.reserve (static_cast<std::size_t> (viewport.widthPixels));

    const std::uint64_t sourceEnd = viewport.sourceFrameOffset + visibleFrames;
    for (int column = 0; column < viewport.widthPixels; ++column)
    {
        const std::uint64_t columnStart = std::min (
            waveform_columns_detail::columnStartFrame (viewport.sourceFrameOffset,
                                                       visibleFrames,
                                                       viewport.widthPixels,
                                                       column),
            sourceEnd);
        const std::uint64_t columnEnd = std::min (
            waveform_columns_detail::columnEndFrame (viewport.sourceFrameOffset,
                                                     visibleFrames,
                                                     viewport.widthPixels,
                                                     column),
            sourceEnd);
        if (columnStart >= columnEnd)
            continue;

        const std::uint64_t firstPeak = columnStart / tier.framesPerPeak;
        const std::uint64_t lastPeak =
            std::min (waveform_columns_detail::ceilDiv (columnEnd, tier.framesPerPeak), peakCount);
        if (firstPeak >= lastPeak)
            continue;

        WaveformColumn out {
            std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            0.0f
        };
        double sumSq = 0.0;
        std::uint64_t weightedFrames = 0u;

        for (std::uint64_t peakIndex = firstPeak; peakIndex < lastPeak; ++peakIndex)
        {
            const std::uint64_t peakStart = peakIndex * tier.framesPerPeak;
            const std::uint64_t peakEnd = std::min (peakStart + static_cast<std::uint64_t> (tier.framesPerPeak),
                                                    cache.sourceFrames);
            const std::uint64_t overlapStart = std::max (columnStart, peakStart);
            const std::uint64_t overlapEnd = std::min (columnEnd, peakEnd);
            if (overlapStart >= overlapEnd)
                continue;

            const std::uint64_t overlapFrames = overlapEnd - overlapStart;
            for (std::uint16_t channel = 0; channel < cache.channels; ++channel)
            {
                const auto& peak =
                    tier.peaks[static_cast<std::size_t> (peakIndex) * cache.channels + channel];
                out.min = std::min (out.min, peak.min);
                out.max = std::max (out.max, peak.max);
                sumSq += static_cast<double> (peak.rms) * static_cast<double> (peak.rms)
                       * static_cast<double> (overlapFrames);
                weightedFrames += overlapFrames;
            }
        }

        if (weightedFrames == 0u)
            continue;

        out.rms = static_cast<float> (std::sqrt (sumSq / static_cast<double> (weightedFrames)));
        result.columns.push_back (out);
    }

    return result;
}

// G2.19: the same column law as computeWaveformColumns, fed by the decoded samples instead of a
// peak tier — every column's min / max / rms over exactly the frames it covers. At one sample per
// pixel (the zoom ceiling) each column IS one sample, so the painted tops trace the wave itself.
[[nodiscard]] inline WaveformColumns computeWaveformColumnsFromSamples (const WaveformSampleSource& source,
                                                                        const WaveformColumnViewport& viewport)
{
    WaveformColumns result;
    if (! source.valid()
        || viewport.sourceFrameCount == 0u
        || viewport.sourceFrameOffset >= source.frames
        || viewport.widthPixels <= 0)
    {
        return result;
    }

    const std::uint64_t availableFrames = source.frames - viewport.sourceFrameOffset;
    const std::uint64_t visibleFrames = std::min (viewport.sourceFrameCount, availableFrames);
    if (visibleFrames == 0u)
        return result;

    result.tierIndex = 0;
    result.framesPerPeak = 1;
    result.columns.reserve (static_cast<std::size_t> (viewport.widthPixels));
    const std::uint64_t sourceEnd = viewport.sourceFrameOffset + visibleFrames;
    for (int column = 0; column < viewport.widthPixels; ++column)
    {
        const std::uint64_t columnStart = std::min (
            waveform_columns_detail::columnStartFrame (viewport.sourceFrameOffset, visibleFrames, viewport.widthPixels, column),
            sourceEnd);
        std::uint64_t columnEnd = std::min (
            waveform_columns_detail::columnEndFrame (viewport.sourceFrameOffset, visibleFrames, viewport.widthPixels, column),
            sourceEnd);
        // Finer than one frame per column: the column shows the frame it sits on.
        if (columnEnd <= columnStart)
            columnEnd = std::min (columnStart + 1u, sourceEnd);
        if (columnStart >= columnEnd)
            continue;

        WaveformColumn out { std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), 0.0f };
        double sumSq = 0.0;
        std::uint64_t counted = 0u;
        for (std::uint64_t frame = columnStart; frame < columnEnd; ++frame)
            for (std::uint16_t channel = 0; channel < source.channels; ++channel)
            {
                const float v = source.interleavedSamples[static_cast<std::size_t> (frame) * source.channels + channel];
                out.min = std::min (out.min, v);
                out.max = std::max (out.max, v);
                sumSq += static_cast<double> (v) * static_cast<double> (v);
                ++counted;
            }
        if (counted == 0u)
            continue;
        out.rms = static_cast<float> (std::sqrt (sumSq / static_cast<double> (counted)));
        result.columns.push_back (out);
    }
    return result;
}

} // namespace yesdaw::ui
