// YES DAW - H3 item 4b runtime audio driver.
//
// The device callback calls processDeviceBlock() only. Graph construction and publish() stay on the
// Control thread; reclaim() stays on the janitor/control side. This keeps the production caller of
// Runtime::processBlock pure C++ so RTSan/TSan can cover it before any JUCE device shell owns it.

#pragma once

#include "engine/Runtime.h"
#include "rt/RtHot.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace yesdaw::engine {

class RuntimeAudioDriver final
{
public:
    RuntimeAudioDriver() : RuntimeAudioDriver (Runtime::Config {}) {}
    explicit RuntimeAudioDriver (Runtime::Config cfg) : runtime_ (cfg) {}

    RuntimeAudioDriver (const RuntimeAudioDriver&) = delete;
    RuntimeAudioDriver& operator= (const RuntimeAudioDriver&) = delete;

    // CONTROL THREAD: ownership transfers to Runtime on success.
    [[nodiscard]] bool publish (std::unique_ptr<CompiledGraph> graph) noexcept
    {
        return runtime_.publish (std::move (graph));
    }

    // CONTROL THREAD (R12): live scalar ops onto the running graph, through Runtime's one
    // ordered command queue. false = the bounded queue is full (control side backpressures).
    [[nodiscard]] bool postSetGain (NodeId node, float linearGain) noexcept
    {
        return runtime_.postSetGain (node, linearGain);
    }

    [[nodiscard]] bool postSetPan (NodeId node, float pan) noexcept
    {
        return runtime_.postSetPan (node, pan);
    }

    [[nodiscard]] bool postSetFxParam (NodeId node, ParameterId paramId, double normalizedValue) noexcept
    {
        return runtime_.postSetFxParam (node, paramId, normalizedValue);
    }

    // AUDIO THREAD / device callback: no allocation, locking, logging, or I/O.
    void processDeviceBlock (float* const* outputChannels,
                             int numOutputChannels,
                             int numFrames) noexcept YESDAW_RT_HOT
    {
        runtime_.processBlock (outputChannels, numOutputChannels, numFrames);
    }

    void processDeviceBlock (float* const* outputChannels,
                             int numOutputChannels,
                             int numFrames,
                             const Transport& transport) noexcept YESDAW_RT_HOT
    {
        runtime_.processBlock (outputChannels, numOutputChannels, numFrames, transport);
    }

    // JANITOR / CONTROL THREAD: never call from the Audio thread.
    std::size_t reclaim() noexcept { return runtime_.reclaim(); }

    std::uint64_t processedGen() const noexcept { return runtime_.processedGen(); }
    std::uint64_t scalarsApplied() const noexcept { return runtime_.scalarsApplied(); }

private:
    Runtime runtime_;
};

} // namespace yesdaw::engine
