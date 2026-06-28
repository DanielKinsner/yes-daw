// YES DAW - H6 reliability helpers.
//
// Control/test-side utilities for the mechanical H6 gate. The hot audio path remains in CompiledGraph;
// this file summarizes measured block durations against the roadmap deadline.

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace yesdaw::engine {

struct DeadlineSoakStats
{
    std::uint64_t blocksProcessed = 0;
    std::uint64_t virtualFrames = 0;
    std::uint64_t underruns = 0;
    std::uint64_t blockPeriodNanos = 0;
    std::uint64_t p999BlockNanos = 0;
    std::uint64_t maxBlockNanos = 0;

    [[nodiscard]] bool passesDeadline() const noexcept
    {
        return blocksProcessed > 0
            && underruns == 0
            && p999BlockNanos < blockPeriodNanos;
    }
};

[[nodiscard]] inline std::uint64_t blockPeriodNanos (double sampleRate, int blockSize) noexcept
{
    if (sampleRate <= 0.0 || blockSize <= 0)
        return 0;

    const double nanos = (static_cast<double> (blockSize) * 1'000'000'000.0) / sampleRate;
    return nanos > 0.0 ? static_cast<std::uint64_t> (nanos) : 0;
}

[[nodiscard]] inline std::uint64_t audioBlocksForDuration (double sampleRate,
                                                           int blockSize,
                                                           std::uint64_t seconds) noexcept
{
    if (sampleRate <= 0.0 || blockSize <= 0 || seconds == 0)
        return 0;

    const double frames = sampleRate * static_cast<double> (seconds);
    const auto totalFrames = static_cast<std::uint64_t> (frames);
    return (totalFrames + static_cast<std::uint64_t> (blockSize) - 1u) / static_cast<std::uint64_t> (blockSize);
}

[[nodiscard]] inline DeadlineSoakStats summarizeDeadlineSoak (std::span<const std::uint64_t> blockNanos,
                                                              double sampleRate,
                                                              int blockSize,
                                                              std::uint64_t virtualSeconds,
                                                              std::uint64_t underruns)
{
    DeadlineSoakStats stats;
    stats.blocksProcessed = static_cast<std::uint64_t> (blockNanos.size());
    stats.virtualFrames = static_cast<std::uint64_t> (sampleRate) * virtualSeconds;
    stats.underruns = underruns;
    stats.blockPeriodNanos = blockPeriodNanos (sampleRate, blockSize);

    if (blockNanos.empty())
        return stats;

    std::vector<std::uint64_t> sorted (blockNanos.begin(), blockNanos.end());
    std::sort (sorted.begin(), sorted.end());

    const std::uint64_t n = static_cast<std::uint64_t> (sorted.size());
    const std::uint64_t p999Index = ((n * 999u) + 999u) / 1000u - 1u;
    stats.p999BlockNanos = sorted[static_cast<std::size_t> (p999Index)];
    stats.maxBlockNanos = sorted.back();
    return stats;
}

} // namespace yesdaw::engine
