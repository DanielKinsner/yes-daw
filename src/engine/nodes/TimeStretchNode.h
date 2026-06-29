// YES DAW - source-style time-stretched clip Node (ADR-0030).
//
// The stretcher has already run on the control side. process() only reads immutable prepared samples by
// timeline frame, so this Node can participate in block-parallel scheduled renders.

#pragma once

#include "engine/Node.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::engine {

class TimeStretchNode final : public Node
{
public:
    TimeStretchNode (NodeId id,
                     std::vector<float> interleavedSamples,
                     int channels,
                     std::int64_t timelineStartFrames = 0) noexcept
        : id_ (id),
          samples_ (std::move (interleavedSamples)),
          channels_ (channels == 2 ? 2 : 1),
          timelineStart_ (timelineStartFrames > 0 ? timelineStartFrames : 0),
          frames_ (static_cast<std::int64_t> (samples_.size() / static_cast<std::size_t> (channels_)))
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }

    void prepare (double, int) override { playFrame_ = 0; }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 1)
            return;

        const int channelsToWrite = std::min (args.audio.numChannels, channels_);
        const std::int64_t blockStart = args.transport.hasTimelineFrame ? args.transport.timelineFrame : playFrame_;
        for (int i = 0; i < args.numFrames; ++i)
        {
            const std::int64_t local = (blockStart + static_cast<std::int64_t> (i)) - timelineStart_;
            for (int channel = 0; channel < channelsToWrite; ++channel)
            {
                float sample = 0.0f;
                if (local >= 0 && local < frames_)
                {
                    sample = samples_[static_cast<std::size_t> (local) * static_cast<std::size_t> (channels_)
                                      + static_cast<std::size_t> (channel)];
                }
                args.audio.channels[channel][i] = sample;
            }
        }

        if (! args.transport.hasTimelineFrame)
            playFrame_ += static_cast<std::int64_t> (args.numFrames);
    }

    void reset() noexcept override { playFrame_ = 0; }
    void release() override { samples_.clear(); samples_.shrink_to_fit(); }

    [[nodiscard]] std::int64_t timelineStartFrames() const noexcept { return timelineStart_; }
    [[nodiscard]] std::int64_t preparedFrames() const noexcept { return frames_; }

private:
    NodeId             id_ = 0;
    std::vector<float> samples_;
    int                channels_ = 1;
    std::int64_t       timelineStart_ = 0;
    std::int64_t       frames_ = 0;
    std::int64_t       playFrame_ = 0;
};

} // namespace yesdaw::engine
