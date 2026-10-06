# 0057. A shell chord's modifiers are the ones held when its key went down

- **Status:** Accepted (agent, 2026-10-06, on Dan's request to repair the 2026-10-06 drive finding in the
  product, after a separate agent critic pass whose findings are folded in: JUCE's window procedure has no
  WM_SYSCHAR case, so the bracket covers the five key messages it does handle; the order that makes the WM_CHAR
  read safe is stated; a KeyPress handed to the router inside a key's dispatch is said to be read as that key's;
  the headless gate is said to stand in for the synchronised state, with the real queue's order proved by the
  drive; the drive always resumes the thread it suspends. One concern was rejected after checking: the gate's
  routing is not a focus toss — the shell is its own top level there, so JUCE always targets it. Committed
  alone, before the code.)
- **Date:** 2026-10-06
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** ADR-0046 §4 (keys go to the Command router, not to widgets); ADR-0049 (the router's dispatch
  order: text entry, Control target, then the keymap); ADR-0002 (the audio thread is untouched — this is the
  message thread only); the Session drive's `Key` primitive (`tools/session-drive.ps1`, commit 03763dc, which
  waits for the app to be idle before a chord); JUCE 8.0.4 `juce_gui_basics/native/juce_Windowing_windows.cpp`.

## Context

On Windows, JUCE's window reads Shift / Ctrl / Alt for every key message with `GetAsyncKeyState`: the
**physical** state at the moment the message thread processes the message (`HWNDComponentPeer::
updateKeyModifiers`, the first call in `doKeyDown`, `doKeyChar` and `doKeyUp`). Key messages wait in the
thread's queue while the thread is busy: an edit that rebuilds the playback engine, a project load, a long
paint. A chord tapped during that wait is read with the modifiers held when it is finally processed, not the
ones held when it was pressed.

Seen on 2026-10-06: the Session drive sent Ctrl+Z right after a mixer knob drag committed an edit and rebuilt
the engine; the probe's `lastAction` was `timeline.zoom.selection` — a bare Z. A user gets the same thing: a
quick Ctrl+Z during a busy moment zooms instead of undoing. The reverse also happens: a bare key, then a
modifier pressed while the key still waits, becomes a chord (Z, then Ctrl down → Undo).

Windows keeps a second key state, the **message-synchronised** one (`GetKeyState` / `GetKeyboardState`). It
changes only as the thread reads key messages from its queue, so while a key message is being dispatched it
reports the modifiers as of that key. Windows' own `TranslateMessage` uses it: the WM_CHAR for a queued Ctrl+Z
is 0x1A even when JUCE reads no Ctrl. That WM_CHAR is where JUCE builds the KeyPress for a printable key, and it
is still "as of that key" when it is read: `TranslateMessage` posts it, and the thread reads posted messages
before any later keyboard input (the documented `GetMessage` order), so Ctrl's key-up is read after it. macOS (`NSEvent.modifierFlags`) and Linux (the X event's `state`) take
modifiers from the event itself, so they are already right.

This needs an ADR because it fixes what a chord *means* at the Command router, and puts native window code in
the shell for the first time.

## Options considered

1. **Patch JUCE** (`updateKeyModifiers` reads `GetKeyState`).
   - Pros: fixes every component — text fields, popup menus, mouse-click modifiers.
   - Cons: forks a pinned dependency; every JUCE update must re-apply it; out of bounds for this fix.
2. **Drive-side only** (the idle wait of 03763dc, already in).
   - Pros: no product code.
   - Cons: users still hit it; the drive passes while the product is wrong.
3. **Re-derive modifiers from the text character** (a WM_CHAR of 0x1A means Ctrl).
   - Pros: no native hook.
   - Cons: only letters with Ctrl; nothing for Alt, for Shift on punctuation, or for F-keys, arrows and Del,
     which have no character. A partial fix that hides the rest.
4. **A thread-wide `WH_GETMESSAGE` hook** that records the synchronised modifiers when a key message is taken
   from the queue.
   - Pros: sees every window's keys.
   - Cons: it sees a message being *taken*, not the end of its dispatch, so the router cannot tell "this
     KeyPress came from that message" from a later synthesised KeyPress. More machinery than the problem
     needs.
5. **Bracket each key message to the shell's window.** A window subclass on the shell's native window
   publishes the synchronised Shift / Ctrl / Alt for exactly as long as JUCE dispatches a key message to it;
   the router rebuilds the KeyPress with them.
   - Pros: an exact bracket; no JUCE change; one small Windows-only file.
   - Cons: covers the Command router only, not JUCE's own text fields, popup menus or mouse modifiers.

## Decision

**Option 5.**

1. **A shell chord's Shift / Ctrl / Alt are the ones held when its key was pressed** — the message-synchronised
   state — never the state when a busy message thread gets round to the key. Both directions: a modifier
   released during the wait still counts; a modifier pressed during the wait does not.
2. **The mechanism (Windows).** The shell keeps a window subclass (comctl32 `SetWindowSubclass`; JUCE already
   links comctl32) on its own native window. It follows the window: it re-attaches whenever JUCE gives the
   shell a new peer (even one with the old handle's value), and removes itself on `WM_NCDESTROY` and when the
   shell goes. For the duration of each WM_KEYDOWN, WM_SYSKEYDOWN, WM_KEYUP, WM_SYSKEYUP and WM_CHAR dispatch —
   the key messages JUCE's window procedure handles; it has no WM_SYSCHAR case, and builds an Alt chord inside
   WM_SYSKEYDOWN — it publishes `GetKeyState`'s Shift / Ctrl / Alt, with JUCE's AltGr rule (right Alt without
   right Ctrl is Alt alone). Brackets nest; the outer value comes back when an inner one ends.
3. **The router's entry applies it first.** `MainComponent::keyPressed` — both the focused-shell path and the
   KeyListener path — replaces the KeyPress's modifiers with the published ones before any route, so the
   Control target, Escape cancels, musical typing, step input and the keymap all see one chord. The key code
   and the text character stay as JUCE made them (JUCE derives them from the scan code and the synchronised
   state already).
4. **Outside a key dispatch, and on macOS and Linux, a KeyPress passes unchanged.** A synthesised KeyPress (the
   UI input harness, a programmatic call) keeps its own modifiers. Inside a key message's dispatch, a KeyPress
   handed to the router is read as that key's; the shell makes no programmatic key call of its own.
5. **Scope.** JUCE's active text fields (their own Ctrl+Z / Ctrl+C), JUCE popup menus, and mouse-click
   modifiers (JUCE reads those with `GetAsyncKeyState` too) keep JUCE's behaviour. They are not the Command
   router. Mouse modifiers are a parked follow-up, taken up if a drive or a user report shows the need.
6. **The Session drive keeps its idle wait and modifier holds.** They still serve popups and text fields, and
   keep the drives deterministic.

## Consequences

- **Positive:** a quick Ctrl+Z, Ctrl+S or Alt+arrow during an engine rebuild does what was pressed, and a bare
  key never becomes a chord because a modifier went down while it waited.
- **Gates:** `[key-time-modifiers]` in the UI input harness puts the real shell on a hidden native window, sets
  the thread's synchronised state (`SetKeyboardState`: Ctrl, Ctrl+Shift, Alt) while the physical state is up,
  posts a real key through JUCE's own message loop (TranslateMessage and its window procedure), and asserts
  both that JUCE built the bare key (the mechanism, still there) and that the router dispatched Undo, Redo and
  Nudge Right; a bare key stays bare; the bracket follows a recreated window. Its rules — nesting, the reverse
  direction, pass-through outside a dispatch, both router paths — are asserted through the bracket directly.
  The headless gate stands in for the synchronised state (a posted key-up does not change it), so the real
  queue's order is proved on the real app: an SS-2 step whose drive primitive suspends the app's message
  thread, sends the chord and releases every key (or presses the modifier only after the key), then always
  resumes the thread. The whole chord waits in the queue, every time; the step fails on the code before this
  ADR and passes after it.
- **Negative / accepted costs:** native Windows code in the shell (one source file). Text fields, popups and
  mouse modifiers still read the physical state (JUCE's mouse and pen paths reuse whatever its last key-state
  read found).
- **Follow-ups:** CONTEXT.md gains **Key-time modifiers**. Mouse-click modifiers stay parked.
