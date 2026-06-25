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

[[nodiscard]] inline float equalPowerFadeGain (double x) noexcept
{
    const double clamped = std::clamp (x, 0.0, 1.0);
    const double bend = clamped * (1.0 - clamped);
    const double shaped = bend * (1.0 + 1.4186 * bend) + clamped;
    return static_cast<float> (shaped * shaped);
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

[[nodiscard]] inline ClipGainEnvelopeEvaluation evaluateClipGainEnvelope (const Clip& clip,
                                                                          Tick localTick) noexcept
{
    if (! detail::clipEditMetadataIsStorageSafe (clip) || clip.timelineLength <= 0)
        return {};

    if (localTick < 0 || localTick >= clip.timelineLength)
        return {};

    const float fadeIn = detail::fadeInGainAt (localTick, clip.fadeIn);
    const float fadeOut = detail::fadeOutGainAt (localTick, clip.timelineLength, clip.fadeOut);
    const float evaluatedGain = clip.gain * std::min (fadeIn, fadeOut);

    if (! std::isfinite (evaluatedGain))
        return {};

    return { true, evaluatedGain };
}

} // namespace yesdaw::engine
