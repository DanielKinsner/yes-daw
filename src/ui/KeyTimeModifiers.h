// YES DAW — ADR-0057: a shell chord's modifiers are the ones held when its key went down.
//
// JUCE's Windows window reads Shift / Ctrl / Alt with GetAsyncKeyState — the physical state when the
// message thread gets round to a key — so a key that waited in the queue while the thread was busy (an
// engine rebuild after an edit) is read with whatever is held by then: a Ctrl+Z tapped during the wait
// arrives as Z. Windows also keeps the state as of each queued key (GetKeyState). While a key message is
// dispatched to the shell's window, that state is published here, and the Command router rebuilds the
// KeyPress from it. macOS and Linux windows already read the event's own modifiers.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <optional>

namespace yesdaw::ui {

// The Shift / Ctrl / Alt held when the key now being dispatched went down; nothing outside a key
// dispatch (a synthesised KeyPress, a test, macOS and Linux). Message thread only.
[[nodiscard]] std::optional<juce::ModifierKeys> keyTimeModifiers() noexcept;

// `key` with its Shift / Ctrl / Alt replaced by keyTimeModifiers() inside a key dispatch, else `key`.
// The key code and the text character are kept as JUCE made them.
[[nodiscard]] juce::KeyPress withKeyTimeModifiers (const juce::KeyPress& key);

// One key dispatch's bracket. Brackets nest: the outer value returns when an inner one ends. The
// Windows window hook opens one per key message; a test opens one to stand for a dispatch.
class ScopedKeyTimeModifiers final
{
public:
    explicit ScopedKeyTimeModifiers (juce::ModifierKeys keyboardModifiers) noexcept;
    ~ScopedKeyTimeModifiers() noexcept;

    ScopedKeyTimeModifiers (const ScopedKeyTimeModifiers&) = delete;
    ScopedKeyTimeModifiers& operator= (const ScopedKeyTimeModifiers&) = delete;

private:
    std::optional<juce::ModifierKeys> outer;
};

// Keeps the bracket on whichever native window the watched component is in, following it when JUCE
// recreates that window; removed when the watcher goes. Windows only — elsewhere it attaches nothing.
class KeyTimeModifierWatcher final : private juce::ComponentMovementWatcher
{
public:
    explicit KeyTimeModifierWatcher (juce::Component& watched);
    ~KeyTimeModifierWatcher() override;

    KeyTimeModifierWatcher (const KeyTimeModifierWatcher&) = delete;
    KeyTimeModifierWatcher& operator= (const KeyTimeModifierWatcher&) = delete;

    // The native window the bracket is on (nullptr: none).
    [[nodiscard]] void* attachedWindow() const noexcept { return window; }

private:
    using juce::ComponentMovementWatcher::componentMovedOrResized;
    using juce::ComponentMovementWatcher::componentVisibilityChanged;
    void componentPeerChanged() override;
    void componentMovedOrResized (bool, bool) override {}
    void componentVisibilityChanged() override {}

    void attachTo (juce::ComponentPeer* peer);

    void* window = nullptr;
};

} // namespace yesdaw::ui
