// YES DAW — the colours JUCE-free code shares with the theme (ADR-0063): each defined once, here.
//
// UiTheme::Color reads these for the same tokens, the model's track-colour cycle reads them, and so does the app's
// window background — so a colour can never drift between the places that use it.

#pragma once

#include <cstdint>

namespace yesdaw::ui::colours {

inline constexpr std::uint32_t appBackground = 0xff070a0du;
inline constexpr std::uint32_t accentBlue = 0xff3b8cffu;
inline constexpr std::uint32_t accentTeal = 0xff1bb5a6u;
inline constexpr std::uint32_t accentAmber = 0xffd29118u;
inline constexpr std::uint32_t accentPurple = 0xffa578ffu;
inline constexpr std::uint32_t accentCyan = 0xff20c8d8u;

} // namespace yesdaw::ui::colours
