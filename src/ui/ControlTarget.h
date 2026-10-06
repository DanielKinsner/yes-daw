// YES DAW — the Control target (G4.0b; ADR-0049 "Keyboard access through the existing command router").
//
// The Command router owns ONE logical Control target, separate from the Focus context: the visible,
// keyboard-operable control Tab / Shift+Tab walk to. Native widgets still never take keyboard focus
// (G0.2's law); the router decides what each key means. This header is the pure half — reading order,
// traversal, stale-target recovery, the key-priority table and the fader's dB step — so every rule is
// testable without a window. The shell supplies the entries (live widgets and painted controls) and
// performs the effects through the existing command/undo paths.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace yesdaw::ui {

enum class ControlTargetRole : std::uint8_t
{
    Button,      // Enter clicks it
    Toggle,      // Enter flips it (through its own click path)
    Chooser,     // Enter starts a choice; arrows step the items; Enter keeps, Esc restores
    Value,       // Enter starts an adjustment; arrows step the value; Enter keeps, Esc restores
    TextField    // Enter starts text entry (the field takes keyboard focus; text entry outranks all)
};

[[nodiscard]] constexpr const char* controlTargetRoleName (ControlTargetRole role) noexcept
{
    switch (role)
    {
        case ControlTargetRole::Button:    return "button";
        case ControlTargetRole::Toggle:    return "toggle";
        case ControlTargetRole::Chooser:   return "chooser";
        case ControlTargetRole::Value:     return "value";
        case ControlTargetRole::TextField: return "text";
    }
    return "button";
}

// Shell coordinates. Pure on purpose: no JUCE type crosses this header.
struct ControlTargetRect
{
    int x = 0, y = 0, width = 0, height = 0;

    [[nodiscard]] constexpr int centreY() const noexcept { return y + height / 2; }
    [[nodiscard]] constexpr int bottom() const noexcept { return y + height; }
    [[nodiscard]] constexpr bool empty() const noexcept { return width <= 0 || height <= 0; }
};

struct ControlTargetEntry
{
    std::string id;     // stable: a widget's component id, or a painted control's layout id
    std::string name;   // the accessible name
    ControlTargetRole role = ControlTargetRole::Button;
    ControlTargetRect bounds;
    int region = 0;     // shell region in Tab order: header, rail, arrange, inspector, dock, overlay
};

// Tab order: region first, then reading order inside the region — rows top to bottom, left to right
// within a row. A control joins the current row while its centre lies inside the row's first control
// (so a 24 px combo beside a 30 px button stays on the button's row). Ties break on x, then id, so the
// order is a pure function of the layout.
inline void orderControlTargets (std::vector<ControlTargetEntry>& entries)
{
    std::sort (entries.begin(), entries.end(), [] (const ControlTargetEntry& a, const ControlTargetEntry& b) {
        if (a.region != b.region) return a.region < b.region;
        if (a.bounds.centreY() != b.bounds.centreY()) return a.bounds.centreY() < b.bounds.centreY();
        if (a.bounds.x != b.bounds.x) return a.bounds.x < b.bounds.x;
        return a.id < b.id;
    });

    std::vector<int> rowOf (entries.size(), 0);
    int row = 0;
    for (std::size_t i = 0, anchor = 0; i < entries.size(); ++i)
    {
        const bool sameRow = i > 0 && entries[i].region == entries[anchor].region
                          && entries[i].bounds.centreY() <= entries[anchor].bounds.bottom();
        if (! sameRow)
        {
            anchor = i;
            ++row;
        }
        rowOf[i] = row;
    }

    std::vector<std::size_t> order (entries.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort (order.begin(), order.end(), [&] (std::size_t a, std::size_t b) {
        if (rowOf[a] != rowOf[b]) return rowOf[a] < rowOf[b];
        if (entries[a].bounds.x != entries[b].bounds.x) return entries[a].bounds.x < entries[b].bounds.x;
        return entries[a].id < entries[b].id;
    });

    std::vector<ControlTargetEntry> sorted;
    sorted.reserve (entries.size());
    for (const std::size_t i : order)
        sorted.push_back (std::move (entries[i]));
    entries = std::move (sorted);
}

// The keys the router can claim, already classified by the shell (modifiers folded in).
enum class ControlKey : std::uint8_t
{
    Tab,
    ShiftTab,
    Enter,
    Escape,
    Increase,   // Up / Right
    Decrease,   // Down / Left
    Space,
    Other
};

enum class ControlKeyRoute : std::uint8_t
{
    Keymap,          // not the router's: the keymap decides (Space transport, global transport, editor)
    Next,            // Tab: start navigation, or move to the next control (confirms an interaction)
    Previous,        // Shift+Tab
    Activate,        // Enter while navigating: click / start a choice / start a value / start text entry
    Confirm,         // Enter during an interaction: keep the value
    Cancel,          // Esc during an interaction: restore the value captured when it started
    EndNavigation,   // Esc while navigating: the editor context has the keys again
    Adjust           // an arrow during an interaction
};

// ADR-0049's priority after active text entry (a focused text field keeps its keys before the router
// sees them): Space transport, then Control-target Enter / Esc / Tab, then other global transport,
// then the active control's adjustment, then editor-context commands. One key, one route: Enter never
// both activates and returns to zero; an adjusting arrow never also nudges a Clip.
[[nodiscard]] constexpr ControlKeyRoute routeControlKey (ControlKey key,
                                                         bool navigating,
                                                         bool interacting,
                                                         bool chordIsGlobalTransport) noexcept
{
    switch (key)
    {
        case ControlKey::Space:    return ControlKeyRoute::Keymap;
        case ControlKey::Tab:      return ControlKeyRoute::Next;
        case ControlKey::ShiftTab: return ControlKeyRoute::Previous;
        case ControlKey::Enter:
            return interacting ? ControlKeyRoute::Confirm
                 : navigating  ? ControlKeyRoute::Activate
                               : ControlKeyRoute::Keymap;   // Return to zero
        case ControlKey::Escape:
            return interacting ? ControlKeyRoute::Cancel
                 : navigating  ? ControlKeyRoute::EndNavigation
                               : ControlKeyRoute::Keymap;
        case ControlKey::Increase:
        case ControlKey::Decrease:
            return interacting && ! chordIsGlobalTransport ? ControlKeyRoute::Adjust : ControlKeyRoute::Keymap;
        case ControlKey::Other:    return ControlKeyRoute::Keymap;
    }
    return ControlKeyRoute::Keymap;
}

// The router's state: which control is the target, whether navigation is on, and the value an
// interaction started from (what Esc restores).
class ControlNavigator
{
public:
    [[nodiscard]] bool navigating() const noexcept { return navigating_; }
    [[nodiscard]] bool interacting() const noexcept { return interacting_; }
    [[nodiscard]] const std::string& targetId() const noexcept { return target_; }
    [[nodiscard]] double interactionOrigin() const noexcept { return origin_; }

    // Tab / Shift+Tab over `ordered` (orderControlTargets order). Starting navigation lands on the
    // first (Tab) or last (Shift+Tab) control; after that the walk wraps. Nothing to target: false.
    bool step (std::span<const ControlTargetEntry> ordered, int direction)
    {
        if (ordered.empty())
        {
            endNavigation();
            return false;
        }
        const auto here = indexOf (ordered, target_);
        std::size_t next = 0;
        if (! navigating_ || here == npos)
            next = direction >= 0 ? 0 : ordered.size() - 1;
        else if (direction >= 0)
            next = (here + 1) % ordered.size();
        else
            next = (here + ordered.size() - 1) % ordered.size();
        interacting_ = false;
        retarget (ordered, next);
        return true;
    }

    // Accessibility targeting (a screen reader moved its focus onto a control): that control becomes
    // the target and navigation starts. Unknown id: false, nothing changes.
    bool targetById (std::span<const ControlTargetEntry> ordered, const std::string& id)
    {
        const auto index = indexOf (ordered, id);
        if (index == npos)
            return false;
        if (target_ != id)
            interacting_ = false;
        retarget (ordered, index);
        return true;
    }

    void endNavigation() noexcept
    {
        navigating_ = false;
        interacting_ = false;
        target_.clear();
    }

    // A control that disappeared, hid or disabled leaves no stale target: the target moves to the
    // control now at its old place in the order (or the last one); with nothing left, navigation
    // ends. Returns true when the target changed (an interaction on a vanished control is over).
    bool revalidate (std::span<const ControlTargetEntry> ordered)
    {
        if (! navigating_)
            return false;
        if (const auto here = indexOf (ordered, target_); here != npos)
        {
            lastIndex_ = here;
            return false;
        }
        interacting_ = false;
        if (ordered.empty())
        {
            endNavigation();
            return true;
        }
        retarget (ordered, std::min (lastIndex_, ordered.size() - 1));
        return true;
    }

    void beginInteraction (double origin) noexcept
    {
        interacting_ = navigating_;
        origin_ = origin;
    }

    void endInteraction() noexcept { interacting_ = false; }

private:
    static constexpr std::size_t npos = static_cast<std::size_t> (-1);

    [[nodiscard]] static std::size_t indexOf (std::span<const ControlTargetEntry> ordered, const std::string& id) noexcept
    {
        if (id.empty())
            return npos;
        for (std::size_t i = 0; i < ordered.size(); ++i)
            if (ordered[i].id == id)
                return i;
        return npos;
    }

    void retarget (std::span<const ControlTargetEntry> ordered, std::size_t index)
    {
        navigating_ = true;
        target_ = ordered[index].id;
        lastIndex_ = index;
    }

    std::string target_;
    std::size_t lastIndex_ = 0;
    double origin_ = 0.0;
    bool navigating_ = false;
    bool interacting_ = false;
};

// The painted fader's keyboard step: whole dB (Shift: a tenth), clamped to the rail's linear range.
// From silence the first step up lands on the floor; stepping below the floor reaches silence.
inline constexpr double kControlFaderFloorDb = -60.0;

[[nodiscard]] inline float stepFaderGainDb (float linearGain, int steps, double stepDb, double maxLinear) noexcept
{
    if (steps == 0)
        return linearGain;
    double db = linearGain > 0.0f ? 20.0 * std::log10 (static_cast<double> (linearGain)) : -1.0e9;
    if (db < kControlFaderFloorDb)
    {
        if (steps < 0)
            return 0.0f;
        db = kControlFaderFloorDb - stepDb;
    }
    db += static_cast<double> (steps) * stepDb;
    if (db < kControlFaderFloorDb - 1.0e-9)
        return 0.0f;
    const double linear = std::pow (10.0, db / 20.0);
    return static_cast<float> (std::min (linear, maxLinear));
}

} // namespace yesdaw::ui
