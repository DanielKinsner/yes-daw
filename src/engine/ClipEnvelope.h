// YES DAW - derived Clip gain-envelope evaluation.
//
// Clip gain/fades are Project metadata. This header evaluates that metadata into
// a scalar for a clip-local position without storing sampled, snapped, pixel, or
// derived sample values back into Project truth.

#pragma once

#include "engine/Project.h"

#include <algorithm>
#include <cmath>

namespace yesdaw::engine {

struct ClipGainEnvelopeEvaluation
{
    bool  valid = false;
    float gain = 0.0f;
};

namespace detail {

inline constexpr double kHalfPi = 1.57079632679489661923;

[[nodiscard]] inline float equalPowerFadeGain (double x) noexcept
{
    const double clamped = std::clamp (x, 0.0, 1.0);
    return static_cast<float> (std::sin (kHalfPi * clamped));
}

[[nodiscard]] inline float fadeInGainAt (Tick localTick, Tick fadeLength) noexcept
{
    if (fadeLength <= 0 || localTick >= fadeLength)
        return 1.0f;

    return equalPowerFadeGain (static_cast<double> (localTick) / static_cast<double> (fadeLength));
}

[[nodiscard]] inline float fadeOutGainAt (Tick localTick, Tick timelineLength, Tick fadeLength) noexcept
{
    if (fadeLength <= 0)
        return 1.0f;

    const Tick fadeStart = fadeLength >= timelineLength ? 0 : timelineLength - fadeLength;
    if (localTick < fadeStart)
        return 1.0f;

    const double progress = static_cast<double> (localTick - fadeStart) / static_cast<double> (fadeLength);
    return equalPowerFadeGain (1.0 - progress);
}

} // namespace detail

[[nodiscard]] inline float evaluateClipFadeEnvelopeGain (Tick localTick,
                                                         Tick timelineLength,
                                                         Tick fadeIn,
                                                         Tick fadeOut) noexcept
{
    if (timelineLength <= 0 || localTick < 0 || localTick >= timelineLength)
        return 0.0f;

    const float fadeInGain = detail::fadeInGainAt (localTick, fadeIn);
    const float fadeOutGain = detail::fadeOutGainAt (localTick, timelineLength, fadeOut);
    return std::min (fadeInGain, fadeOutGain);
}

[[nodiscard]] inline ClipGainEnvelopeEvaluation evaluateClipGainEnvelope (const Clip& clip,
                                                                          Tick localTick) noexcept
{
    if (! detail::clipEditMetadataIsStorageSafe (clip) || clip.timelineLength <= 0)
        return {};

    if (localTick < 0 || localTick >= clip.timelineLength)
        return {};

    const float evaluatedGain = clip.gain
                              * evaluateClipFadeEnvelopeGain (localTick,
                                                              clip.timelineLength,
                                                              clip.fadeIn,
                                                              clip.fadeOut);

    if (! std::isfinite (evaluatedGain))
        return {};

    return { true, evaluatedGain };
}

} // namespace yesdaw::engine
