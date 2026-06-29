#pragma once

#include <ebur128.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace yesdaw::analysis {

enum class LoudnessStatus : std::uint8_t
{
    Ok,
    InvalidSampleRate,
    UnsupportedChannelLayout,
    SampleCountNotFrameAligned,
    NonFiniteInput,
    LibraryInitFailed,
    LibraryError
};

struct LoudnessMetrics
{
    double integratedLufs { 0.0 };
    double momentaryLufs { 0.0 };
    double shortTermLufs { 0.0 };
    double loudnessRangeLu { 0.0 };
    double relativeThresholdLufs { 0.0 };
    std::array<double, 2> samplePeakDbfs {
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    std::array<double, 2> truePeakDbtp {
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    std::uint32_t channels { 0 };
    std::uint64_t frames { 0 };
};

struct LoudnessResult
{
    LoudnessStatus status { LoudnessStatus::LibraryError };
    LoudnessMetrics metrics {};
};

struct LoudnessLibraryVersion
{
    int major { 0 };
    int minor { 0 };
    int patch { 0 };
};

inline constexpr int kLoudnessMode = EBUR128_MODE_I
                                   | EBUR128_MODE_LRA
                                   | EBUR128_MODE_TRUE_PEAK
                                   | EBUR128_MODE_HISTOGRAM;

inline LoudnessLibraryVersion loudnessLibraryVersion()
{
    LoudnessLibraryVersion version;
    ebur128_get_version (&version.major, &version.minor, &version.patch);
    return version;
}

namespace detail {

struct EburState
{
    ebur128_state* state { nullptr };

    ~EburState()
    {
        if (state != nullptr)
            ebur128_destroy (&state);
    }

    EburState() = default;
    explicit EburState (ebur128_state* s) : state (s) {}
    EburState (const EburState&) = delete;
    EburState& operator= (const EburState&) = delete;

    EburState (EburState&& other) noexcept : state (other.state)
    {
        other.state = nullptr;
    }

    EburState& operator= (EburState&& other) noexcept
    {
        if (this != &other)
        {
            if (state != nullptr)
                ebur128_destroy (&state);
            state = other.state;
            other.state = nullptr;
        }
        return *this;
    }
};

inline bool eburOk (int status) noexcept
{
    return status == EBUR128_SUCCESS;
}

inline double linearToDb (double value) noexcept
{
    return value > 0.0 ? 20.0 * std::log10 (value) : -std::numeric_limits<double>::infinity();
}

inline bool assignChannelMap (ebur128_state* state, std::uint32_t channels) noexcept
{
    if (channels == 1)
        return eburOk (ebur128_set_channel (state, 0, EBUR128_CENTER));

    if (channels == 2)
    {
        return eburOk (ebur128_set_channel (state, 0, EBUR128_LEFT))
            && eburOk (ebur128_set_channel (state, 1, EBUR128_RIGHT));
    }

    return false;
}

inline bool addFramesChunked (ebur128_state* state,
                              std::span<const float> interleaved,
                              std::uint32_t channels,
                              std::size_t frames,
                              std::size_t chunkFrames) noexcept
{
    std::size_t done = 0;
    const std::size_t stride = static_cast<std::size_t> (channels);
    const std::size_t step = chunkFrames == 0 ? frames : chunkFrames;

    while (done < frames)
    {
        const std::size_t n = std::min (step, frames - done);
        const float* const src = interleaved.data() + (done * stride);
        if (! eburOk (ebur128_add_frames_float (state, src, n)))
            return false;
        done += n;
    }

    return true;
}

inline bool readMetrics (ebur128_state* state, std::uint32_t channels, LoudnessMetrics& out) noexcept
{
    if (! eburOk (ebur128_loudness_global (state, &out.integratedLufs)))
        return false;
    if (! eburOk (ebur128_loudness_momentary (state, &out.momentaryLufs)))
        return false;
    if (! eburOk (ebur128_loudness_shortterm (state, &out.shortTermLufs)))
        return false;
    if (! eburOk (ebur128_loudness_range (state, &out.loudnessRangeLu)))
        return false;
    if (! eburOk (ebur128_relative_threshold (state, &out.relativeThresholdLufs)))
        return false;

    for (std::uint32_t ch = 0; ch < channels; ++ch)
    {
        double samplePeak = 0.0;
        double truePeak = 0.0;
        if (! eburOk (ebur128_sample_peak (state, ch, &samplePeak)))
            return false;
        if (! eburOk (ebur128_true_peak (state, ch, &truePeak)))
            return false;

        out.samplePeakDbfs[ch] = linearToDb (samplePeak);
        out.truePeakDbtp[ch] = linearToDb (truePeak);
    }

    return true;
}

} // namespace detail

inline LoudnessResult analyzeInterleavedLoudness (std::span<const float> interleaved,
                                                  std::uint32_t channels,
                                                  std::uint32_t sampleRate,
                                                  std::size_t chunkFrames = 0)
{
    if (sampleRate == 0)
        return { LoudnessStatus::InvalidSampleRate, {} };

    if (channels != 1 && channels != 2)
        return { LoudnessStatus::UnsupportedChannelLayout, {} };

    const std::size_t channelCount = static_cast<std::size_t> (channels);
    if (interleaved.size() % channelCount != 0)
        return { LoudnessStatus::SampleCountNotFrameAligned, {} };

    for (const float sample : interleaved)
    {
        if (! std::isfinite (sample))
            return { LoudnessStatus::NonFiniteInput, {} };
    }

    const std::size_t frames = interleaved.size() / channelCount;
    detail::EburState state {
        ebur128_init (channels, static_cast<unsigned long> (sampleRate), kLoudnessMode)
    };

    if (state.state == nullptr)
        return { LoudnessStatus::LibraryInitFailed, {} };

    if (! detail::assignChannelMap (state.state, channels))
        return { LoudnessStatus::LibraryError, {} };

    if (! detail::addFramesChunked (state.state, interleaved, channels, frames, chunkFrames))
        return { LoudnessStatus::LibraryError, {} };

    LoudnessMetrics metrics;
    metrics.channels = channels;
    metrics.frames = static_cast<std::uint64_t> (frames);
    if (! detail::readMetrics (state.state, channels, metrics))
        return { LoudnessStatus::LibraryError, {} };

    return { LoudnessStatus::Ok, metrics };
}

} // namespace yesdaw::analysis
