// YES DAW — the colours JUCE-free code shares with the theme (ADR-0063): each defined once, here.
//
// UiTheme::Color reads these for the same tokens, the model's track-colour cycle reads them, and so does the app's
// window background — so a colour can never drift between the places that use it.

#pragma once

#include <cstddef>
#include <cstdint>

namespace yesdaw::ui::colours {

inline constexpr std::uint32_t appBackground = 0xff070a0du;
inline constexpr std::uint32_t accentBlue = 0xff3b8cffu;
inline constexpr std::uint32_t accentTeal = 0xff1bb5a6u;
inline constexpr std::uint32_t accentAmber = 0xffd29118u;
inline constexpr std::uint32_t accentPurple = 0xffa578ffu;
inline constexpr std::uint32_t accentCyan = 0xff20c8d8u;

// The layout pass, step 1: the track colours, in the order new tracks take them (the Logic / SoundCraft rainbow). A
// track with no colour of its own wears the entry for its position, on the rail, its clips and its mixer strip alike;
// dark text and dark waveforms read on every entry (each is at least 4.5:1 against appBackground).
// Muted (Dan, 2026-10-13: "the colors don't have to be so vibrant") - about half saturation, Logic's softer look.
inline constexpr std::uint32_t trackPalette[] = {
    0xffd17769u,   // red
    0xffd69b60u,   // orange
    0xffccbb66u,   // yellow
    0xff69b362u,   // green
    0xff59b3abu,   // teal
    0xff6c9ad9u,   // blue
    0xffa077d9u,   // purple
    0xffd977a0u,   // pink
};
inline constexpr std::size_t trackPaletteSize = sizeof (trackPalette) / sizeof (trackPalette[0]);

} // namespace yesdaw::ui::colours
