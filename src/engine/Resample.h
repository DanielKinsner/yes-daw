// YES DAW - ADR-0055: the band-limited resampler behind a cross-rate Asset's rate-matched view.
//
// A Kaiser-windowed sinc evaluated at each output frame's exact input position (zero phase: no added delay),
// band-limited to the lower of the two Nyquist frequencies so a downsampled Asset does not alias. Two tiers
// (ADR-0010): a short kernel for the live view, a long one for offline render and export. Control thread /
// worker only — it allocates its output; the audio thread never calls it. Pure: the same input gives the same
// output.

#pragma once

#include "engine/Time.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

namespace yesdaw::engine {

struct ResamplerSpec
{
    int zeroCrossings = 16;      // the kernel's half-width, in zero crossings of the band-limiting sinc
    double kaiserBeta = 7.0;     // the window's sidelobe control
    double passband = 0.90;      // the cutoff as a fraction of the lower Nyquist (the rest is the transition band)
};

[[nodiscard]] constexpr ResamplerSpec resamplerSpecFor (ResampleQuality quality) noexcept
{
    return quality == ResampleQuality::OfflineRender ? ResamplerSpec { 64, 10.0, 0.95 }
                                                     : ResamplerSpec { 16, 7.0, 0.90 };
}

// The output length for `frames` at `inputRate` resampled to `outputRate`.
[[nodiscard]] inline std::uint64_t resampledFrameCount (std::uint64_t frames, double inputRate, double outputRate) noexcept
{
    return static_cast<std::uint64_t> (std::llround (static_cast<double> (frames) * outputRate / inputRate));
}

namespace detail {

[[nodiscard]] inline double besselI0 (double x) noexcept
{
    double sum = 1.0;
    double term = 1.0;
    const double quarterSquare = 0.25 * x * x;
    for (int k = 1; k < 200; ++k)
    {
        term *= quarterSquare / (static_cast<double> (k) * static_cast<double> (k));
        sum += term;
        if (term < sum * 1.0e-17)
            break;
    }
    return sum;
}

// The windowed sinc h(u) = sinc(u) · kaiser(u / Z) for u in [0, Z], tabulated at `kStepsPerCrossing` points per
// zero crossing and read with linear interpolation (|u| beyond Z is 0).
class WindowedSincTable
{
public:
    static constexpr int kStepsPerCrossing = 1'024;

    explicit WindowedSincTable (const ResamplerSpec& spec)
        : zeroCrossings_ (spec.zeroCrossings)
    {
        const int count = spec.zeroCrossings * kStepsPerCrossing + 2;
        values_.resize (static_cast<std::size_t> (count));
        const double windowNorm = besselI0 (spec.kaiserBeta);
        for (int i = 0; i < count; ++i)
        {
            const double u = static_cast<double> (i) / static_cast<double> (kStepsPerCrossing);
            if (u >= static_cast<double> (spec.zeroCrossings))
            {
                values_[static_cast<std::size_t> (i)] = 0.0;
                continue;
            }
            const double ratio = u / static_cast<double> (spec.zeroCrossings);
            const double window = besselI0 (spec.kaiserBeta * std::sqrt (1.0 - ratio * ratio)) / windowNorm;
            const double piU = std::numbers::pi * u;
            const double sinc = u == 0.0 ? 1.0 : std::sin (piU) / piU;
            values_[static_cast<std::size_t> (i)] = sinc * window;
        }
    }

    [[nodiscard]] double operator() (double u) const noexcept
    {
        const double position = std::abs (u) * static_cast<double> (kStepsPerCrossing);
        const auto index = static_cast<std::size_t> (position);
        if (index + 1u >= values_.size())
            return 0.0;
        const double fraction = position - static_cast<double> (index);
        return values_[index] + fraction * (values_[index + 1u] - values_[index]);
    }

    [[nodiscard]] int zeroCrossings() const noexcept { return zeroCrossings_; }

private:
    int zeroCrossings_ = 0;
    std::vector<double> values_;
};

[[nodiscard]] inline const WindowedSincTable& windowedSincTableFor (ResampleQuality quality)
{
    static const WindowedSincTable live (resamplerSpecFor (ResampleQuality::LivePlayback));
    static const WindowedSincTable offline (resamplerSpecFor (ResampleQuality::OfflineRender));
    return quality == ResampleQuality::OfflineRender ? offline : live;
}

} // namespace detail

// Resample interleaved audio from `inputRate` to `outputRate`. Output frame n reads the input at the exact
// position n · inputRate / outputRate; samples outside the input are silence. Same-rate input is returned as is.
[[nodiscard]] inline std::vector<float> resampleInterleaved (std::span<const float> input,
                                                             std::size_t channels,
                                                             double inputRate,
                                                             double outputRate,
                                                             ResampleQuality quality)
{
    if (channels == 0u || input.size() % channels != 0u || ! (inputRate > 0.0) || ! (outputRate > 0.0))
        return {};
    if (inputRate == outputRate)
        return std::vector<float> (input.begin(), input.end());

    const ResamplerSpec spec = resamplerSpecFor (quality);
    const detail::WindowedSincTable& table = detail::windowedSincTableFor (quality);
    const auto inputFrames = static_cast<std::int64_t> (input.size() / channels);
    const std::uint64_t outputFrames = resampledFrameCount (static_cast<std::uint64_t> (inputFrames), inputRate, outputRate);
    const double step = inputRate / outputRate;                                   // input frames per output frame
    const double cutoff = std::min (1.0, outputRate / inputRate) * spec.passband; // of the input's Nyquist
    const double halfWidth = static_cast<double> (spec.zeroCrossings) / cutoff;   // in input frames

    std::vector<float> output (static_cast<std::size_t> (outputFrames) * channels, 0.0f);
    std::vector<double> sums (channels, 0.0);
    for (std::uint64_t n = 0; n < outputFrames; ++n)
    {
        const double position = static_cast<double> (n) * step;
        const auto first = std::max<std::int64_t> (0, static_cast<std::int64_t> (std::ceil (position - halfWidth)));
        const auto last = std::min<std::int64_t> (inputFrames - 1, static_cast<std::int64_t> (std::floor (position + halfWidth)));
        std::fill (sums.begin(), sums.end(), 0.0);
        for (std::int64_t k = first; k <= last; ++k)
        {
            const double weight = cutoff * table ((static_cast<double> (k) - position) * cutoff);
            const float* frame = input.data() + static_cast<std::size_t> (k) * channels;
            for (std::size_t c = 0; c < channels; ++c)
                sums[c] += weight * static_cast<double> (frame[c]);
        }
        float* out = output.data() + static_cast<std::size_t> (n) * channels;
        for (std::size_t c = 0; c < channels; ++c)
            out[c] = static_cast<float> (sums[c]);
    }
    return output;
}

} // namespace yesdaw::engine
