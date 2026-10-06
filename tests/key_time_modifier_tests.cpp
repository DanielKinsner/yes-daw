// YES DAW — ADR-0057 [key-time-modifiers]: a shell chord's modifiers are the ones held when its key went down.
//
// JUCE's Windows window reads Shift / Ctrl / Alt with GetAsyncKeyState — the physical state when the message
// thread processes a key — so a key that waited in the queue while the thread was busy is read with whatever is
// held by then (2026-10-06: a drive's Ctrl+Z after a knob drag's engine rebuild arrived as Z, zoom to selection).
// The native gate reproduces that wait without touching the real keyboard: SetKeyboardState sets the message
// thread's synchronised state (what Windows says was held when the key was queued) while the physical state stays
// up, and a real WM_KEYDOWN goes through JUCE's own message loop (TranslateMessage, its window procedure) to the
// shell on a hidden native window.

#include "ui/KeyTimeModifiers.h"
#include "ui/MainComponent.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace {

using yesdaw::ui::ScopedKeyTimeModifiers;
using yesdaw::ui::UiActionId;

constexpr int kCtrl = juce::ModifierKeys::ctrlModifier;
constexpr int kShift = juce::ModifierKeys::shiftModifier;

std::unique_ptr<juce::Component> makeKeyTimeShell()
{
    juce::MessageManager::getInstance();
    auto shell = yesdaw::ui::createMainComponent (yesdaw::ui::MainComponentFileChoices {});
    REQUIRE (shell != nullptr);
    return shell;
}

std::string probedLastAction (juce::Component& shell)
{
    juce::var probe;
    REQUIRE (juce::JSON::parse (juce::String (yesdaw::ui::mainComponentStateProbeJson (shell)), probe).wasOk());
    return probe["lastAction"].toString().toStdString();
}

// Marks lastAction with a known action, so the next key's dispatch is unambiguous.
void resetLastAction (juce::Component& shell)
{
    yesdaw::ui::mainComponentDispatchAction (shell, UiActionId::TransportStop);
    REQUIRE (probedLastAction (shell) == "transport.stop");
}

} // namespace

TEST_CASE ("key-time modifiers: the router reads the modifiers of the key's own dispatch, else the KeyPress's",
           "[ui][input][shell][key-time-modifiers]")
{
    // The bracket: nothing outside a dispatch; brackets nest; mouse buttons are not key state.
    const juce::KeyPress ctrlZ ('z', juce::ModifierKeys (kCtrl), 0);
    REQUIRE_FALSE (yesdaw::ui::keyTimeModifiers().has_value());
    REQUIRE (yesdaw::ui::withKeyTimeModifiers (ctrlZ) == ctrlZ);
    {
        const ScopedKeyTimeModifiers outer (juce::ModifierKeys (kCtrl | juce::ModifierKeys::leftButtonModifier));
        REQUIRE (yesdaw::ui::keyTimeModifiers() == juce::ModifierKeys (kCtrl));
        {
            const ScopedKeyTimeModifiers inner { juce::ModifierKeys() };
            const juce::KeyPress bare = yesdaw::ui::withKeyTimeModifiers (ctrlZ);
            REQUIRE (bare.getKeyCode() == 'z');
            REQUIRE_FALSE (bare.getModifiers().isAnyModifierKeyDown());
        }
        REQUIRE (yesdaw::ui::keyTimeModifiers() == juce::ModifierKeys (kCtrl));
    }
    REQUIRE_FALSE (yesdaw::ui::keyTimeModifiers().has_value());

    auto shell = makeKeyTimeShell();

    // A modifier released while the key waited still counts: JUCE built a bare Z (0x1a is the Ctrl+Z character
    // Windows translated from the queued state), but the key went down under Ctrl.
    resetLastAction (*shell);
    {
        const ScopedKeyTimeModifiers dispatch { juce::ModifierKeys (kCtrl) };
        REQUIRE (shell->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys(), 0x1a)));
    }
    REQUIRE (probedLastAction (*shell) == "edit.undo");

    // A modifier pressed while the key waited does not: JUCE built Ctrl+Z, but the key went down bare.
    resetLastAction (*shell);
    {
        const ScopedKeyTimeModifiers dispatch { juce::ModifierKeys() };
        REQUIRE (shell->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys (kCtrl), 'z')));
    }
    REQUIRE (probedLastAction (*shell) == "timeline.zoom.selection");

    // The KeyListener half of the router (focus on the window itself) reads the same.
    auto* router = dynamic_cast<juce::KeyListener*> (shell.get());
    REQUIRE (router != nullptr);
    juce::Component fakeWindow;
    resetLastAction (*shell);
    {
        const ScopedKeyTimeModifiers dispatch { juce::ModifierKeys (kCtrl | kShift) };
        REQUIRE (router->keyPressed (juce::KeyPress ('Z', juce::ModifierKeys(), 0x1a), &fakeWindow));
    }
    REQUIRE (probedLastAction (*shell) == "edit.redo");

    // Outside a dispatch a synthesised chord keeps its own modifiers.
    resetLastAction (*shell);
    REQUIRE (shell->keyPressed (ctrlZ));
    REQUIRE (probedLastAction (*shell) == "edit.undo");
}

#if JUCE_WINDOWS
namespace {

// Sets the message thread's synchronised state for the modifier keys — what Windows says was held when the next
// queued key went down — and leaves the physical state (GetAsyncKeyState) alone. Restored on exit.
class ScopedQueuedModifiers final
{
public:
    explicit ScopedQueuedModifiers (std::initializer_list<int> held)
    {
        REQUIRE (GetKeyboardState (saved.data()));
        std::array<BYTE, 256> state = saved;
        for (const int vk : { VK_SHIFT, VK_LSHIFT, VK_RSHIFT, VK_CONTROL, VK_LCONTROL, VK_RCONTROL, VK_MENU, VK_LMENU, VK_RMENU })
            state[static_cast<std::size_t> (vk)] = 0;
        for (const int vk : held)
            state[static_cast<std::size_t> (vk)] = 0x80;
        REQUIRE (SetKeyboardState (state.data()));
    }

    ~ScopedQueuedModifiers() { SetKeyboardState (saved.data()); }

    ScopedQueuedModifiers (const ScopedQueuedModifiers&) = delete;
    ScopedQueuedModifiers& operator= (const ScopedQueuedModifiers&) = delete;

private:
    std::array<BYTE, 256> saved {};
};

bool physicallyDown (int vk)
{
    return (GetAsyncKeyState (vk) & 0x8000) != 0;
}

// A key message's lParam as a keyboard makes it: repeat 1, the scan code, extended for the arrows, the Alt
// context bit for a system key, previous-state and transition bits for a release.
LPARAM keyLParam (UINT vk, bool down, bool alt)
{
    LPARAM bits = 1 | (static_cast<LPARAM> (MapVirtualKeyW (vk, MAPVK_VK_TO_VSC)) << 16);
    if (vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN)
        bits |= static_cast<LPARAM> (1) << 24;
    if (alt)
        bits |= static_cast<LPARAM> (1) << 29;
    if (! down)
        bits |= (static_cast<LPARAM> (1) << 30) | (static_cast<LPARAM> (1) << 31);
    return bits;
}

// Every KeyPress exactly as JUCE built it, before the shell routes it (the shell is its own top level here, so
// its listeners run before its keyPressed).
struct RawKeyRecorder final : juce::KeyListener
{
    std::vector<juce::KeyPress> keys;
    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        keys.push_back (key);
        return false;
    }
};

// Queues one key's press and release on the shell's window as a keyboard does (a system key while Alt is held),
// then runs JUCE's own message loop until JUCE has built its KeyPress and the release has gone through.
void queueKeyAndRun (juce::Component& shell, RawKeyRecorder& recorder, UINT vk, bool alt = false)
{
    REQUIRE (shell.getPeer() != nullptr);
    auto* const hwnd = static_cast<HWND> (shell.getPeer()->getNativeHandle());
    const std::size_t before = recorder.keys.size();
    REQUIRE (PostMessageW (hwnd, alt ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, keyLParam (vk, true, alt)));
    REQUIRE (PostMessageW (hwnd, alt ? WM_SYSKEYUP : WM_KEYUP, vk, keyLParam (vk, false, alt)));
    for (int i = 0; i < 200 && recorder.keys.size() == before; ++i)
        (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
    REQUIRE (recorder.keys.size() == before + 1);
}

} // namespace

TEST_CASE ("key-time modifiers: a chord that waited in the queue keeps the modifiers held when it went down",
           "[ui][input][shell][key-time-modifiers][native]")
{
    auto shell = makeKeyTimeShell();
    // A hidden, off-screen native window: JUCE's real window procedure, but never shown or activated, so the
    // gate never takes the screen or the keyboard focus.
    shell->setVisible (false);
    shell->setTopLeftPosition (-20000, -20000);
    shell->addToDesktop (0);
    REQUIRE (shell->getPeer() != nullptr);
    RawKeyRecorder recorder;
    shell->addKeyListener (&recorder);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);   // the window's creation messages

    // Ctrl+Z, Ctrl already up when the thread reads the key: JUCE builds a bare Z (the mechanism, still there),
    // the router dispatches Undo.
    resetLastAction (*shell);
    {
        const ScopedQueuedModifiers queued { VK_CONTROL, VK_LCONTROL };
        queueKeyAndRun (*shell, recorder, 'Z');
    }
    if (! physicallyDown (VK_CONTROL))
        CHECK_FALSE (recorder.keys.back().getModifiers().isCtrlDown());
    REQUIRE (probedLastAction (*shell) == "edit.undo");

    // Ctrl+Shift+Z: Redo.
    resetLastAction (*shell);
    {
        const ScopedQueuedModifiers queued { VK_CONTROL, VK_LCONTROL, VK_SHIFT, VK_LSHIFT };
        queueKeyAndRun (*shell, recorder, 'Z');
    }
    if (! physicallyDown (VK_SHIFT))
        CHECK_FALSE (recorder.keys.back().getModifiers().isShiftDown());
    REQUIRE (probedLastAction (*shell) == "edit.redo");

    // Alt+Right: a system key on JUCE's extended-key path (no character message): Nudge Right.
    resetLastAction (*shell);
    {
        const ScopedQueuedModifiers queued { VK_MENU, VK_LMENU };
        queueKeyAndRun (*shell, recorder, VK_RIGHT, true);
    }
    if (! physicallyDown (VK_MENU))
        CHECK_FALSE (recorder.keys.back().getModifiers().isAltDown());
    REQUIRE (probedLastAction (*shell) == "edit.nudge_right");

    // A bare Z queued with nothing held stays Z: the bracket adds only what was held.
    resetLastAction (*shell);
    {
        const ScopedQueuedModifiers queued (std::initializer_list<int> {});
        queueKeyAndRun (*shell, recorder, 'Z');
    }
    REQUIRE (probedLastAction (*shell) == "timeline.zoom.selection");

    // JUCE recreates the native window: the bracket follows to the new one.
    shell->removeFromDesktop();
    shell->addToDesktop (0);
    REQUIRE (shell->getPeer() != nullptr);
    // Let the new window's creation messages go through first: Windows re-syncs a thread's key state to the
    // physical keyboard on focus changes, which would wipe the queued state set below.
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    resetLastAction (*shell);
    {
        const ScopedQueuedModifiers queued { VK_CONTROL, VK_LCONTROL };
        queueKeyAndRun (*shell, recorder, 'Z');
    }
    REQUIRE (probedLastAction (*shell) == "edit.undo");

    // Nothing is left published once the dispatches end.
    REQUIRE_FALSE (yesdaw::ui::keyTimeModifiers().has_value());
    shell->removeKeyListener (&recorder);
    shell->removeFromDesktop();
}
#endif
