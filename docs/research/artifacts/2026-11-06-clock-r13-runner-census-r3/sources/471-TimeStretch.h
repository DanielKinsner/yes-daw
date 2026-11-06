// YES DAW - control-side time-stretch preparation (ADR-0030).
//
// Signalsmith Stretch is deliberately used here, before audio reaches the graph. The audio-thread Node
// consumes immutable prepared samples and never calls the third-party stretcher.

#pragma once

#include "signalsmith-stretch.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace yesdaw::engine {

enum class TimeStretchStatus : std::uint8_t
{
    Ok = 0,
    EmptySource,
    UnsupportedChannelLayout,
    SampleCountNotFrameAligned,
    InvalidSampleRate,
    InvalidStretchFactor,
    NonFiniteInput,
    InputTooLong
};

struct TimeStretchLibraryVersion
{
    std::size_t major = 0;
    std::size_t minor = 0;
    std::size_t patch = 0;
};

struct PreparedTimeStretch
{
    TimeStretchStatus status = TimeStretchStatus::Ok;
    std::vector<float> interleavedSamples;
    std::uint32_t channels = 0;
    std::uint64_t sourceFrames = 0;
    std::uint64_t outputFrames = 0;
    double stretchFactor = 1.0;

    [[nodiscard]] bool ok() const noexcept { return status == TimeStretchStatus::Ok; }
};

[[nodiscard]] constexpr TimeStretchLibraryVersion timeStretchLibraryVersion() noexcept
{
    return TimeStretchLibraryVersion {
        signalsmith::stretch::SignalsmithStretch<float>::version[0],
        signalsmith::stretch::SignalsmithStretch<float>::version[1],
        signalsmith::stretch::SignalsmithStretch<float>::version[2]
    };
}

namespace detail {

struct ConstPlanarChannel
{
    const float* samples = nullptr;
    int offset = 0;

    [[nodiscard]] float operator[] (int index) const noexcept
    {
        return samples[static_cast<std::size_t> (offset + index)];
    }
};

struct MutablePlanarChannel
{
    float* samples = nullptr;
    int offset = 0;

    [[nodiscard]] float& operator[] (int index) const noexcept
    {
        return samples[static_cast<std::size_t> (offset + index)];
    }
};

class ConstPlanarView
{
public:
    ConstPlanarView (const std::vector<std::vector<float>>& channels, int offset) noexcept
        : channels_ (channels), offset_ (offset)
    {
    }

    [[nodiscard]] ConstPlanarChannel operator[] (int channel) const noexcept
    {
        return ConstPlanarChannel { channels_[static_cast<std::size_t> (channel)].data(), offset_ };
    }

private:
    const std::vector<std::vector<float>>& channels_;
    int offset_ = 0;
};

class MutablePlanarView
{
public:
    MutablePlanarView (std::vector<std::vector<float>>& channels, int offset) noexcept
        : channels_ (channels), offset_ (offset)
    {
    }

    [[nodiscard]] MutablePlanarChannel operator[] (int channel) const noexcept
    {
        return MutablePlanarChannel { channels_[static_cast<std::size_t> (channel)].data(), offset_ };
    }

private:
    std::vector<std::vector<float>>& channels_;
    int offset_ = 0;
};

} // namespace detail

[[nodiscard]] inline PreparedTimeStretch prepareTimeStretch (std::span<const float> interleavedSamples,
                                                             std::uint32_t channels,
                                                             double sampleRate,
                                                             double stretchFactor)
{
    PreparedTimeStretch result;
    result.channels = channels;
    result.stretchFactor = stretchFactor;

    if (channels != 1u && channels != 2u)
    {
        result.status = TimeStretchStatus::UnsupportedChannelLayout;
        return result;
    }

    if (interleavedSamples.empty())
    {
        result.status = TimeStretchStatus::EmptySource;
        return result;
    }

    if (interleavedSamples.size() % static_cast<std::size_t> (channels) != 0u)
    {
        result.status = TimeStretchStatus::SampleCountNotFrameAligned;
        return result;
    }

    if (! std::isfinite (sampleRate) || sampleRate <= 0.0)
    {
        result.status = TimeStretchStatus::InvalidSampleRate;
        return result;
    }

    if (! std::isfinite (stretchFactor) || stretchFactor < 0.5 || stretchFactor > 2.0)
    {
        result.status = TimeStretchStatus::InvalidStretchFactor;
        return result;
    }

    for (const float sample : interleavedSamples)
    {
        if (! std::isfinite (sample))
        {
            result.status = TimeStretchStatus::NonFiniteInput;
            return result;
        }
    }

    const std::size_t sourceFrames = interleavedSamples.size() / static_cast<std::size_t> (channels);
    const double outputFramesExact = std::round (static_cast<double> (sourceFrames) * stretchFactor);
    if (sourceFrames == 0u || outputFramesExact < 1.0
        || sourceFrames > static_cast<std::size_t> (std::numeric_limits<int>::max())
        || outputFramesExact > static_cast<double> (std::numeric_limits<int>::max()))
    {
        result.status = TimeStretchStatus::InputTooLong;
        return result;
    }

    const int sourceFramesInt = static_cast<int> (sourceFrames);
    const int outputFramesInt = static_cast<int> (outputFramesExact);

    signalsmith::stretch::SignalsmithStretch<float> stretch (0);
    stretch.presetDefault (static_cast<int> (channels), static_cast<float> (sampleRate));

    const int inputLatency = stretch.inputLatency();
    const int outputLatency = stretch.outputLatency();
    const std::size_t channelCount = static_cast<std::size_t> (channels);
    const std::size_t inputStorageFrames = sourceFrames + static_cast<std::size_t> (inputLatency);
    const std::size_t outputStorageFrames = std::max (
        static_cast<std::size_t> (outputFramesInt) + static_cast<std::size_t> (outputLatency),
        static_cast<std::size_t> (outputLatency) * 2u);

    std::vector<std::vector<float>> input (channelCount, std::vector<float> (inputStorageFrames, 0.0f));
    std::vector<std::vector<float>> output (channelCount, std::vector<float> (outputStorageFrames, 0.0f));

    for (std::size_t frame = 0; frame < sourceFrames; ++frame)
        for (std::size_t channel = 0; channel < channelCount; ++channel)
            input[channel][frame] = interleavedSamples[frame * channelCount + channel];

    detail::ConstPlanarView seekInput (input, 0);
    stretch.seek (seekInput, inputLatency, 1.0 / stretchFactor);

    detail::ConstPlanarView processInput (input, inputLatency);
    detail::MutablePlanarView processOutput (output, 0);
    stretch.process (processInput, sourceFramesInt, processOutput, outputFramesInt);

    detail::MutablePlanarView tailOutput (output, outputFramesInt);
    stretch.flush (tailOutput, outputLatency);

    for (std::size_t channel = 0; channel < channelCount; ++channel)
    {
        for (int i = 0; i < outputLatency; ++i)
        {
            const float trimmed = output[channel][static_cast<std::size_t> (outputLatency - 1 - i)];
            output[channel][static_cast<std::size_t> (outputLatency + i)] -= trimmed;
        }
    }

    result.sourceFrames = static_cast<std::uint64_t> (sourceFrames);
    result.outputFrames = static_cast<std::uint64_t> (outputFramesInt);
    result.interleavedSamples.assign (static_cast<std::size_t> (outputFramesInt) * channelCount, 0.0f);
    for (std::size_t frame = 0; frame < static_cast<std::size_t> (outputFramesInt); ++frame)
        for (std::size_t channel = 0; channel < channelCount; ++channel)
            result.interleavedSamples[frame * channelCount + channel] =
                output[channel][static_cast<std::size_t> (outputLatency) + frame];

    return result;
}

} // namespace yesdaw::engine
