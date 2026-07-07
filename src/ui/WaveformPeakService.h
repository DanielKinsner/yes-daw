// YES DAW - control-side waveform peak cache service (H16 CP2).
//
// Waveform peaks are derived, deletable UI/cache state. Project truth never depends on them.
// The audio thread never touches this class. The UI thread may poll tryGetReady(), which only
// copies a mutex-guarded shared_ptr; building and writing peak caches happens on the worker.

#pragma once

#include "persistence/WaveformPeakCache.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace yesdaw::ui {

class WaveformPeakService final
{
public:
    WaveformPeakService() = default;

    WaveformPeakService (const WaveformPeakService&) = delete;
    WaveformPeakService& operator= (const WaveformPeakService&) = delete;

    ~WaveformPeakService()
    {
        stop();
    }

    void start (std::filesystem::path bundlePath)
    {
        stop();

        {
            std::lock_guard lock { queueMutex_ };
            bundlePath_ = std::move (bundlePath);
            stop_ = false;
        }

        worker_ = std::thread ([this] { workerLoop(); });
    }

    void requestBuild (const engine::Asset& asset, std::vector<float> channelMajorSamples)
    {
        {
            std::lock_guard lock { queueMutex_ };
            jobs_.push_back (Job { asset, std::move (channelMajorSamples) });
        }

        queueCv_.notify_one();
    }

    [[nodiscard]] std::shared_ptr<const persistence::WaveformPeakCache> tryGetReady (
        const engine::AssetContentHash& hash) const
    {
        std::lock_guard lock { resultsMutex_ };
        const auto it = ready_.find (keyForHash (hash));
        if (it == ready_.end())
            return {};

        return it->second;
    }

    void registerPaintThread (std::thread::id threadId)
    {
        std::lock_guard lock { stateMutex_ };
        paintThreadId_ = threadId;
        hasPaintThread_ = true;
    }

    [[nodiscard]] bool builtOnForbiddenThread() const
    {
        std::lock_guard lock { stateMutex_ };
        return builtOnForbiddenThread_;
    }

    [[nodiscard]] std::thread::id workerThreadId() const
    {
        std::lock_guard lock { stateMutex_ };
        return workerThreadId_;
    }

    [[nodiscard]] std::thread::id lastBuildThreadId() const
    {
        std::lock_guard lock { stateMutex_ };
        return lastBuildThreadId_;
    }

    [[nodiscard]] std::uint64_t buildCount() const noexcept
    {
        return buildCount_.load (std::memory_order_acquire);
    }

    [[nodiscard]] std::shared_ptr<const persistence::WaveformPeakCache> forceSynchronousBuildOnCallerThread (
        const engine::Asset& asset,
        std::vector<float> channelMajorSamples)
    {
        return buildAndPublish (asset, std::span<const float> (channelMajorSamples.data(), channelMajorSamples.size()));
    }

private:
    struct Job
    {
        engine::Asset      asset;
        std::vector<float> channelMajorSamples;
    };

    static std::string keyForHash (const engine::AssetContentHash& hash)
    {
        return persistence::detail::peakHexBytes (hash.bytes);
    }

    void stop()
    {
        {
            std::lock_guard lock { queueMutex_ };
            stop_ = true;
        }

        queueCv_.notify_one();
        if (worker_.joinable())
            worker_.join();
    }

    void workerLoop()
    {
        {
            std::lock_guard lock { stateMutex_ };
            workerThreadId_ = std::this_thread::get_id();
        }

        for (;;)
        {
            Job job;
            {
                std::unique_lock lock { queueMutex_ };
                queueCv_.wait (lock, [this] { return stop_ || ! jobs_.empty(); });

                if (stop_ && jobs_.empty())
                    return;

                job = std::move (jobs_.front());
                jobs_.pop_front();
            }

            (void) buildAndPublish (job.asset,
                                    std::span<const float> (job.channelMajorSamples.data(),
                                                            job.channelMajorSamples.size()));
        }
    }

    [[nodiscard]] std::shared_ptr<const persistence::WaveformPeakCache> buildAndPublish (
        const engine::Asset& asset,
        std::span<const float> channelMajorSamples)
    {
        {
            std::lock_guard lock { stateMutex_ };
            lastBuildThreadId_ = std::this_thread::get_id();
            if (hasPaintThread_ && lastBuildThreadId_ == paintThreadId_)
                builtOnForbiddenThread_ = true;
        }

        auto built = persistence::buildWaveformPeakCache (asset, channelMajorSamples);
        if (! built.ok())
            return {};

        const auto written = persistence::writeWaveformPeakCache (bundlePath_, built.cache);
        if (! written.ok())
            return {};

        auto ready = std::make_shared<const persistence::WaveformPeakCache> (std::move (built.cache));
        {
            std::lock_guard lock { resultsMutex_ };
            ready_[keyForHash (asset.contentHash)] = ready;
        }

        buildCount_.fetch_add (1u, std::memory_order_release);
        return ready;
    }

    std::thread               worker_;
    mutable std::mutex        queueMutex_;
    std::condition_variable   queueCv_;
    std::deque<Job>           jobs_;
    std::filesystem::path     bundlePath_;
    bool                      stop_ = true;

    mutable std::mutex resultsMutex_;
    std::unordered_map<std::string, std::shared_ptr<const persistence::WaveformPeakCache>> ready_;

    mutable std::mutex stateMutex_;
    std::thread::id    workerThreadId_;
    std::thread::id    lastBuildThreadId_;
    std::thread::id    paintThreadId_;
    bool               hasPaintThread_ = false;
    bool               builtOnForbiddenThread_ = false;

    std::atomic<std::uint64_t> buildCount_ { 0 };
};

} // namespace yesdaw::ui
