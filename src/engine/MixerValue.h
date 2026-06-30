// YES DAW - shared mixer value validation.

#pragma once

#include <cmath>

namespace yesdaw::engine {

inline constexpr float kMixerMaxLinearGain = 1.0e3f;

[[nodiscard]] inline bool mixerGainIsValid (float gain) noexcept
{
    return std::isfinite (gain) && gain >= 0.0f && gain <= kMixerMaxLinearGain;
}

[[nodiscard]] inline bool mixerPanIsValid (float pan) noexcept
{
    return std::isfinite (pan) && pan >= -1.0f && pan <= 1.0f;
}

} // namespace yesdaw::engine
