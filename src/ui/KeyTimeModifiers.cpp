// YES DAW — ADR-0057: the key-time modifier bracket (see KeyTimeModifiers.h).
#include "ui/KeyTimeModifiers.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <commctrl.h>   // SetWindowSubclass (comctl32, which JUCE already links)
#endif

namespace yesdaw::ui {

namespace {

std::optional<juce::ModifierKeys> current;   // message thread only

#if JUCE_WINDOWS
constexpr UINT_PTR kKeyTimeSubclassId = 57;   // ADR-0057

bool synchronisedDown (int virtualKey) noexcept
{
    return (GetKeyState (virtualKey) & 0x8000) != 0;
}

// GetKeyState changes only as the thread reads key messages from its queue, so inside a key message's
// dispatch it reports the keys held when that key went down — not the keys held now.
juce::ModifierKeys synchronisedModifiers() noexcept
{
    int flags = 0;
    if (synchronisedDown (VK_SHIFT))   flags |= juce::ModifierKeys::shiftModifier;
    if (synchronisedDown (VK_CONTROL)) flags |= juce::ModifierKeys::ctrlModifier;
    if (synchronisedDown (VK_MENU))    flags |= juce::ModifierKeys::altModifier;

    // JUCE's own AltGr rule (updateKeyModifiers): Windows reports AltGr as left Ctrl + right Alt.
    if (synchronisedDown (VK_RMENU) && ! synchronisedDown (VK_RCONTROL))
        flags = (flags & ~juce::ModifierKeys::ctrlModifier) | juce::ModifierKeys::altModifier;

    return juce::ModifierKeys (flags);
}

LRESULT CALLBACK keyTimeSubclassProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                      UINT_PTR subclassId, DWORD_PTR)
{
    switch (message)
    {
        // The key messages JUCE's window procedure turns into KeyPresses and key-state changes (an Alt chord
        // is built inside WM_SYSKEYDOWN; a printable key inside the WM_CHAR TranslateMessage posted, which the
        // thread reads before any later keyboard input).
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        case WM_CHAR:
        {
            const ScopedKeyTimeModifiers bracket (synchronisedModifiers());
            return DefSubclassProc (hwnd, message, wParam, lParam);
        }
        case WM_NCDESTROY:
            RemoveWindowSubclass (hwnd, keyTimeSubclassProc, subclassId);
            break;
        default:
            break;
    }
    return DefSubclassProc (hwnd, message, wParam, lParam);
}
#endif

} // namespace

std::optional<juce::ModifierKeys> keyTimeModifiers() noexcept
{
    return current;
}

juce::KeyPress withKeyTimeModifiers (const juce::KeyPress& key)
{
    if (! current.has_value())
        return key;
    return juce::KeyPress (key.getKeyCode(), *current, key.getTextCharacter());
}

ScopedKeyTimeModifiers::ScopedKeyTimeModifiers (juce::ModifierKeys keyboardModifiers) noexcept
    : outer (current)
{
    current = keyboardModifiers.withoutMouseButtons();
}

ScopedKeyTimeModifiers::~ScopedKeyTimeModifiers() noexcept
{
    current = outer;
}

KeyTimeModifierWatcher::KeyTimeModifierWatcher (juce::Component& watched)
    : juce::ComponentMovementWatcher (&watched)
{
    attachTo (watched.getPeer());
}

KeyTimeModifierWatcher::~KeyTimeModifierWatcher()
{
    attachTo (nullptr);
}

void KeyTimeModifierWatcher::componentPeerChanged()
{
    juce::Component* const watched = getComponent();
    attachTo (watched != nullptr ? watched->getPeer() : nullptr);
}

// Always detach, then attach: JUCE's removeFromDesktop() tells no one, so a recreated window may carry
// the old handle's value while the old window took its subclass with it (WM_NCDESTROY).
void KeyTimeModifierWatcher::attachTo (juce::ComponentPeer* peer)
{
    void* const next = peer != nullptr ? peer->getNativeHandle() : nullptr;
#if JUCE_WINDOWS
    if (window != nullptr)   // a window already destroyed makes this a no-op
        RemoveWindowSubclass (static_cast<HWND> (window), keyTimeSubclassProc, kKeyTimeSubclassId);
    window = nullptr;
    if (next != nullptr && SetWindowSubclass (static_cast<HWND> (next), keyTimeSubclassProc, kKeyTimeSubclassId, 0))
        window = next;
#else
    juce::ignoreUnused (next);
#endif
}

} // namespace yesdaw::ui
