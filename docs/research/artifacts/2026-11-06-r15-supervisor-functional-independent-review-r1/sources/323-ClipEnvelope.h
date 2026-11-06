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

// G2.10: ONE fade law for the renderer and the painted curve. `x` is the fade's progress (0 at
// silence, 1 at full gain); the curve amount bends the progress first (x^(2^-2c): +1 rises four
// times as fast at the start, -1 four times as slow), then the shape maps it to gain. Every
// shape is 0 at x = 0 and 1 at x = 1 and monotonic between.
inline constexpr double kLogFadeSteepness = 4.0;

[[nodiscard]] inline float fadeShapeGain (FadeShape shape, float curve, double x) noexcept
{
    const double clampedX = std::clamp (x, 0.0, 1.0);
    const double c = std::isfinite (curve) ? std::clamp (static_cast<double> (curve), -1.0, 1.0) : 0.0;
    const double bent = c == 0.0 ? clampedX : std::pow (clampedX, std::exp2 (-2.0 * c));
    switch (shape)
    {
        case FadeShape::Linear:     return static_cast<float> (bent);
        case FadeShape::EqualPower: return static_cast<float> (std::sin (kHalfPi * bent));
        case FadeShape::SCurve:     return static_cast<float> (bent * bent * (3.0 - 2.0 * bent));
        case FadeShape::Log:        return static_cast<float> ((1.0 - std::exp (-kLogFadeSteepness * bent))
                                                              / (1.0 - std::exp (-kLogFadeSteepness)));
    }
    return static_cast<float> (std::sin (kHalfPi * bent));
}

[[nodiscard]] inline float fadeInGainAt (Tick localTick, Tick fadeLength,
                                         FadeShape shape = FadeShape::EqualPower, float curve = 0.0f) noexcept
{
    if (fadeLength <= 0 || localTick >= fadeLength)
        return 1.0f;

    return fadeShapeGain (shape, curve, static_cast<double> (localTick) / static_cast<double> (fadeLength));
}

[[nodiscard]] inline float fadeOutGainAt (Tick localTick, Tick timelineLength, Tick fadeLength,
                                          FadeShape shape = FadeShape::EqualPower, float curve = 0.0f) noexcept
{
    if (fadeLength <= 0)
        return 1.0f;

    const Tick fadeStart = fadeLength >= timelineLength ? 0 : timelineLength - fadeLength;
    if (localTick < fadeStart)
        return 1.0f;

    const double progress = static_cast<double> (localTick - fadeStart) / static_cast<double> (fadeLength);
    return fadeShapeGain (shape, curve, 1.0 - progress);
}

} // namespace detail

[[nodiscard]] inline float evaluateClipFadeEnvelopeGain (Tick localTick,
                                                         Tick timelineLength,
                                                         Tick fadeIn,
                                                         Tick fadeOut,
                                                         FadeShape fadeInShape = FadeShape::EqualPower,
                                                         float fadeInCurve = 0.0f,
                                                         FadeShape fadeOutShape = FadeShape::EqualPower,
                                                         float fadeOutCurve = 0.0f) noexcept
{
    if (timelineLength <= 0 || localTick < 0 || localTick >= timelineLength)
        return 0.0f;

    const float fadeInGain = detail::fadeInGainAt (localTick, fadeIn, fadeInShape, fadeInCurve);
    const float fadeOutGain = detail::fadeOutGainAt (localTick, timelineLength, fadeOut, fadeOutShape, fadeOutCurve);
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
                                                              clip.fadeOut,
                                                              clip.fadeInShape,
                                                              clip.fadeInCurve,
                                                              clip.fadeOutShape,
                                                              clip.fadeOutCurve);

    if (! std::isfinite (evaluatedGain))
        return {};

    return { true, evaluatedGain };
}

} // namespace yesdaw::engine
