#pragma once

#include "ui/ControlTarget.h"   // ADR-0049: the keys the Control target takes before the keymap
#include "ui/UiActions.h"

#include <algorithm>
#include <span>
#include <string>
#include <string_view>

namespace yesdaw::ui {

// ADR-0073 §2: a top-level menu as the menu bar builds it - its name and its actions.
struct EmptyStateMenu
{
    std::string name;
    std::span<const UiActionId> actions;
};

// The router's state a key passes through before the keymap: the Control target's navigation and interaction
// (ADR-0049), musical typing and step input (G3.6).
struct EmptyStateRouterState
{
    bool navigating = false;
    bool interacting = false;
    bool musicalTyping = false;
    bool stepInput = false;
};

// Whether the router takes `chord` before the keymap sees it in that state - the Control target's own keys (its law,
// routeControlKey, on the key the shell maps the press to), a plain printable key while musical typing is on, plain
// Left / Right while step input is on. Such a chord would not run the row's action there.
[[nodiscard]] inline bool emptyStateChordIntercepted (std::string_view chord, UiActionId chordAction,
                                                      const EmptyStateRouterState& router) noexcept
{
    bool ctrl = false, alt = false, shift = false;
    std::string_view key = chord;
    for (;;)
    {
        if (key.starts_with ("Ctrl+")) { ctrl = true; key.remove_prefix (5); }
        else if (key.starts_with ("Alt+")) { alt = true; key.remove_prefix (4); }
        else if (key.starts_with ("Shift+") && key.size() > 6) { shift = true; key.remove_prefix (6); }
        else break;
    }
    const bool plain = ! ctrl && ! alt;
    ControlKey control = ControlKey::Other;
    if (plain && key == "Tab")
        control = shift ? ControlKey::ShiftTab : ControlKey::Tab;
    else if (plain && ! shift && key == "Enter")
        control = ControlKey::Enter;
    else if (plain && ! shift && key == "Esc")
        control = ControlKey::Escape;
    else if (plain && ! shift && key == "Space")
        control = ControlKey::Space;
    else if (plain && (key == "Up" || key == "Right"))
        control = ControlKey::Increase;
    else if (plain && (key == "Down" || key == "Left"))
        control = ControlKey::Decrease;
    const auto& descriptors = uiActionDescriptors();
    const bool globalTransport = static_cast<std::size_t> (chordAction) < descriptors.size()
                              && defaultFocusContext (chordAction) == UiFocusContext::Global
                              && std::string_view (descriptors[static_cast<std::size_t> (chordAction)].stableId).starts_with ("transport.");
    if (routeControlKey (control, router.navigating, router.interacting, globalTransport) != ControlKeyRoute::Keymap)
        return true;
    if (router.musicalTyping && plain && key.size() == 1 && key[0] > 32 && key[0] < 127)
        return true;
    return router.stepInput && plain && ! shift && (key == "Left" || key == "Right");
}

// How an action is taken from where an empty-state row stands: its chord, if the router - in its current state and
// asked with the current Focus context - runs this action for it; else the menu path from the menu bar's own data
// ("Clip > Add MIDI Clip"); else nothing (the row is its noun alone). An action only a submenu holds has no path here.
[[nodiscard]] inline std::string emptyStateHow (const Keymap& keymap, UiActionId action, UiFocusContext focus,
                                                std::span<const EmptyStateMenu> menus, const EmptyStateRouterState& router = {})
{
    if (static_cast<std::size_t> (action) >= uiActionDescriptors().size())
        return {};
    const std::string& chord = keymap.chordFor (action);
    if (! chord.empty() && keymap.actionForChord (chord, focus) == action && ! emptyStateChordIntercepted (chord, action, router))
        return chord;
    for (const EmptyStateMenu& menu : menus)
        if (std::find (menu.actions.begin(), menu.actions.end(), action) != menu.actions.end())
            return menu.name + " > " + uiActionDescriptors()[static_cast<std::size_t> (action)].label;
    return {};
}

// The row's text: its noun, then the way in parentheses after two spaces (ADR-0063's tooltip form) when there is one.
[[nodiscard]] inline std::string emptyStateRowText (std::string_view noun, std::string_view how)
{
    std::string text (noun);
    if (! how.empty())
        text.append ("  (").append (how).append (")");
    return text;
}

} // namespace yesdaw::ui
