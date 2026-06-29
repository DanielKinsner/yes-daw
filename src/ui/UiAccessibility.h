// YES DAW - H11 accessibility surface.
//
// Headless accessibility contract for the native shell: action-backed controls resolve through
// UiActionRegistry, while custom-painted regions carry stable names and keyboard paths.

#pragma once

#include "ui/UiActions.h"

#include <array>
#include <cstddef>
#include <string_view>

namespace yesdaw::ui {

struct UiAccessibilityRegionDescriptor
{
    const char* stableId;
    const char* accessibleName;
    AccessibilityRole role;
    const char* keyboardPath;
    UiActionId backingAction;
};

[[nodiscard]] constexpr std::array<UiActionId, kUiActionCount> makeUiAccessibleActionIds() noexcept
{
    std::array<UiActionId, kUiActionCount> ids {};
    for (std::size_t i = 0; i < ids.size(); ++i)
        ids[i] = static_cast<UiActionId> (i);
    return ids;
}

inline constexpr auto kUiAccessibleActionIds = makeUiAccessibleActionIds();

inline constexpr std::array<UiAccessibilityRegionDescriptor, 9> kUiAccessibilityRegions {{
    { "app.window", "YES DAW main window", AccessibilityRole::Panel, "Application window", UiActionId::Count },
    { "app.menu_bar", "Application menus", AccessibilityRole::Panel, "Alt", UiActionId::Count },
    { "transport.panel", "Transport controls", AccessibilityRole::Panel, "Space / K / Home / L", UiActionId::TransportPlay },
    { "timeline.track_list", "Track list", AccessibilityRole::Panel, "Tab to track actions", UiActionId::ViewTimeline },
    { "timeline.canvas", "Timeline arrangement", AccessibilityRole::Panel, "1", UiActionId::ViewTimeline },
    { "clip.inspector", "Clip inspector", AccessibilityRole::Panel, "Tab to clip actions", UiActionId::TimelineClipSetGain },
    { "mixer.panel", "Mixer strips and meters", AccessibilityRole::Panel, "2", UiActionId::ViewMixer },
    { "mixer.master_meter", "Master loudness meter", AccessibilityRole::Panel, "Ctrl+Alt+L", UiActionId::MixerReadLoudness },
    { "piano_roll.panel", "Piano roll notes and expression lanes", AccessibilityRole::Panel, "3", UiActionId::ViewPianoRoll }
}};

[[nodiscard]] inline const std::array<UiActionId, kUiActionCount>& uiAccessibleActionIds() noexcept
{
    return kUiAccessibleActionIds;
}

[[nodiscard]] inline const std::array<UiAccessibilityRegionDescriptor, kUiAccessibilityRegions.size()>&
uiAccessibilityRegions() noexcept
{
    return kUiAccessibilityRegions;
}

[[nodiscard]] inline const UiAccessibilityRegionDescriptor* accessibilityRegionForStableId (std::string_view stableId) noexcept
{
    for (const auto& region : kUiAccessibilityRegions)
        if (region.stableId == stableId)
            return &region;

    return nullptr;
}

} // namespace yesdaw::ui
