// YES DAW - H9 deterministic scheduled render path.
//
// This is the first scheduler gate: split Project render Blocks across a fixed worker set while keeping
// each immutable CompiledGraph's internal node/sum order canonical. Source nodes receive explicit
// Transport::timelineFrame values so each scheduled Block renders the same absolute Project frames as the
// serial H7 offline path.

#pragma once

#include "engine/OfflineRenderer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <vector>

namespace yesdaw::engine {

struct ScheduledRenderResult
{
    OfflineRenderStatus         status = OfflineRenderStatus::Ok;
    ProjectMixerProjectionError projectError;
    MixerProjectionError        mixerError;
    SampleRate                  sampleRate;
    std::uint16_t               channels = 0;
    std::uint64_t               frames = 0;
    std::uint32_t               workerCount = 0;
    std::vector<float>          interleavedSamples;

    [[nodiscard]] bool ok() const noexcept { return status == OfflineRenderStatus::Ok; }
};

[[nodiscard]] inline ScheduledRenderResult renderProjectWithScheduler (
    const Project& project,
    std::span<const DecodedAssetAudio> decodedAssets,
    std::uint32_t workerCount,
    OfflineRenderOptions options = {},
    std::vector<std::uint64_t>* blockNanos = nullptr)
{
    ScheduledRenderResult result;
    result.sampleRate = project.sampleRate;
    result.workerCount = workerCount;

    if (workerCount == 0u)
    {
        result.status = OfflineRenderStatus::InvalidProject;
        return result;
    }

    ProjectGraphResult first = buildProjectGraph (project, decodedAssets, options);
    result.projectError = first.projectError;
    result.mixerError = first.mixerError;
    if (! first.ok())
    {
        result.status = first.status;
        return result;
    }

    result.sampleRate = first.sampleRate;
    result.channels = first.channels;
    result.frames = first.frames;

    // ADR-0027: out-of-order Block dispatch is only correct for graphs whose every node is order-independent.
    // A stateful node (delay ring, automated ramp, hosted plugin, PDC latency) would be silently mis-rendered,
    // so refuse here instead. Such graphs must use the serial renderOfflineProject until the per-node scheduler.
    if (! first.graph->isBlockParallelSafe())
    {
        result.status = OfflineRenderStatus::GraphNotBlockParallelSafe;
        return result;
    }

    if (first.frames > std::numeric_limits<std::uint64_t>::max() / first.channels)
    {
        result.status = OfflineRenderStatus::OutputTooLarge;
        return result;
    }

    const std::uint64_t sampleCount = first.frames * first.channels;
    if (sampleCount > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()))
    {
        result.status = OfflineRenderStatus::OutputTooLarge;
        return result;
    }

    std::vector<std::unique_ptr<CompiledGraph>> graphs;
    graphs.reserve (workerCount);
    graphs.push_back (std::move (first.graph));
    for (std::uint32_t worker = 1; worker < workerCount; ++worker)
    {
        ProjectGraphResult built = buildProjectGraph (project, decodedAssets, options);
        if (! built.ok())
        {
            result.status = built.status;
            result.projectError = built.projectError;
            result.mixerError = built.mixerError;
            return result;
        }

        graphs.push_back (std::move (built.graph));
    }

    result.interleavedSamples.assign (static_cast<std::size_t> (sampleCount), 0.0f);
    const std::uint64_t totalBlocks =
        (first.frames + static_cast<std::uint64_t> (options.maxBlockSize) - 1u)
        / static_cast<std::uint64_t> (options.maxBlockSize);
    if (blockNanos != nullptr)
        blockNanos->assign (static_cast<std::size_t> (totalBlocks), 0u);

    std::atomic<std::uint64_t> nextBlock { 0 };
    std::atomic<bool> nonFinite { false };

    auto runWorker = [&] (std::uint32_t workerIndex)
    {
        CompiledGraph& graph = *graphs[workerIndex];
        std::vector<float> channelStorage (
            static_cast<std::size_t> (first.channels) * static_cast<std::size_t> (options.maxBlockSize),
            0.0f);
        std::vector<float*> outputs (first.channels, nullptr);
        for (std::uint16_t c = 0; c < first.channels; ++c)
            outputs[c] = channelStorage.data()
                       + static_cast<std::size_t> (c) * static_cast<std::size_t> (options.maxBlockSize);

        for (;;)
        {
            const std::uint64_t block = nextBlock.fetch_add (1, std::memory_order_relaxed);
            if (block >= totalBlocks)
                break;

            const std::uint64_t offset = block * static_cast<std::uint64_t> (options.maxBlockSize);
            const int blockFrames = static_cast<int> (
                std::min<std::uint64_t> (first.frames - offset, static_cast<std::uint64_t> (options.maxBlockSize)));

            Transport transport;
            transport.projectSampleRate = first.sampleRate;
            transport.timelineFrame = static_cast<std::int64_t> (offset);
            transport.hasTimelineFrame = true;
            transport.isPlaying = true;

            const auto before = std::chrono::steady_clock::now();
            EventStream events;
            graph.process (outputs.data(), first.channels, blockFrames, events, transport);
            const auto after = std::chrono::steady_clock::now();
            if (blockNanos != nullptr)
            {
                const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds> (after - before).count();
                (*blockNanos)[static_cast<std::size_t> (block)] = nanos > 0 ? static_cast<std::uint64_t> (nanos) : 1u;
            }

            for (int frame = 0; frame < blockFrames; ++frame)
            {
                const std::size_t outFrame = static_cast<std::size_t> (offset + static_cast<std::uint64_t> (frame));
                for (std::uint16_t channel = 0; channel < first.channels; ++channel)
                {
                    const float value = outputs[channel][frame];
                    if (! std::isfinite (value))
                        nonFinite.store (true, std::memory_order_relaxed);
                    result.interleavedSamples[outFrame * first.channels + channel] = value;
                }
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve (workerCount > 0u ? workerCount - 1u : 0u);
    for (std::uint32_t worker = 1; worker < workerCount; ++worker)
        threads.emplace_back (runWorker, worker);

    runWorker (0);

    for (std::thread& thread : threads)
        thread.join();

    result.status = nonFinite.load (std::memory_order_relaxed)
        ? OfflineRenderStatus::RenderProducedNonFinite
        : OfflineRenderStatus::Ok;
    return result;
}

} // namespace yesdaw::engine
