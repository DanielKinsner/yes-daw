# 0077. The shell asks its questions without a nested modal loop

- **Status:** Accepted (agent, 2026-10-13, under ADR-0049's implementation-ADR clause, after two separate agent critic
  passes; it changes how the shell asks its questions, not what it asks: ADR-0060's and ADR-0062's questions, wording
  and outcomes, ADR-0068 §4 and B37 stand, only their seams' signatures change. Routing macOS Cmd+Q and a Windows logoff
  through B37's close confirmation follows B37's intent)
- **Date:** 2026-10-13
- **Deciders:** build agent (proposer), two separate agent critics
- **Related:** the UI correctness pass, FIX 29 (found by unit 17's critic, measured); [ADR-0062](0062-missing-audio-relink.md)
  (the missing-audio questions); [ADR-0060](0060-new-project-templates-copies.md) (New / Open / Save a Copy / Save as
  Template and the unsaved-changes question); [ADR-0068](0068-autosave-and-recovery-write-stamp.md) §4 (a replace attempt
  and the unsaved tail); [ADR-0067](0067-interaction-and-motion-states.md) §7 (two timers); B37 (confirm on close).

## Context

Every question the native shell asks waits for its answer inside a nested modal loop: the file choosers
(`FileChooser::browseForFileToOpen` / `browseForFileToSave` / `browseForDirectory`), the alerts
(`AlertWindow::showOkCancelBox`, `showYesNoCancelBox`, both synchronous) and Save as Template's name
(`AlertWindow::runModalLoop`). The flows that ask read the answer as a return value and carry on: Open, Open Recent,
New, Save As, Save a Copy, Save as Template, Import / Export Audio and MIDI, the Sampler pad's file, the relink loop at
open (ADR-0062), New / Open's unsaved-changes question and the close confirmation (B37).

On Windows JUCE dispatches its posted messages in batches: it swaps the whole queue out and dispatches each message in
turn (`juce_Messaging_windows.cpp`, `dispatchMessages`); messages posted meanwhile go to a fresh queue. A modal loop
started inside one of the batch's messages holds back the rest of that batch until it returns. The Direct2D window's
vblank thread posts one message and blocks until it is handled (`juce_VBlank_windows.cpp`), and every window on that
monitor shares it; if that message is in the held batch, no window repaints - neither the main window nor the
question's own alert - while the UI tick (its timer re-posts) keeps running. Measured on the unit 17 build: the shell's
paint count held at 2 for 3 s under the launch-time question while the tick asked for about 30 repaints a second.

Which questions start inside a batch: those started by a posted message - a menu command (the popup's result and the
command are posted), the launch-time question unit 17 posts from the UI tick, anything from `callAsync`. A key chord is
handled straight from the window procedure, so Ctrl+O's chooser does not freeze today. macOS and Linux dispatch one
posted message at a time, so the freeze is Windows-only; asking without a loop is still right on every platform.

What is hard to reverse: the shape of every question flow. Once a flow continues in a callback, its code after the
question runs later, on a later message.

## Options considered

1. **Keep the loops; start each question outside a batch.**
   - Pros: no flow changes.
   - Cons: JUCE's public API has no unbatched route (timers, `callAsync` and async updates are all posted messages);
     forcing paints during the loop does not work under Direct2D (`performAnyPendingRepaintsNow` does nothing there).
2. **Ask asynchronously, keeping the test seams answerable at once (chosen).** A seam becomes a question that either
   answers at once (a test's lambda returning the answer, as today) or answers later (the native shell's
   `FileChooser::launchAsync`, an `AlertWindow` entered into modal state with a callback). Every flow continues in the
   answer's callback.
   - Pros: no nested loop in the shell; the ~600 test sites that set a seam to a lambda returning a path stay as they
     are; one flow shape for tests and the app.
   - Cons: each asking flow is rewritten, including its callers' code after the question. An immediate answer hides an
     ordering bug, so each flow also needs a test whose answer is held and given later.
3. **Make every seam asynchronous in its signature.**
   - Pros: the type says what happens.
   - Cons: every test that sets a seam changes, for nothing more tested than option 2's held-answer tests.
4. **C++20 coroutines (`co_await ask (...)`).**
   - Pros: each flow stays linear, which removes the "code after the question" class of bug by construction.
   - Cons: the project has no coroutine support type, executor or lifetime rules; about fourteen flows do not justify
     building one, and a suspended frame holding `this` across a shell teardown is a harder lifetime problem than a
     `SafePointer` in a callback.

## Decision

1. **No nested loop.** The `YesDaw` target is built with `JUCE_MODAL_LOOPS_PERMITTED=0` (JUCE's own default). JUCE
   modules are compiled per target, so the four test harnesses keep `=1` (their harness pumps the loop) without sharing
   objects with the app. The flag removes every blocking JUCE dialog call from the app at compile time:
   `FileChooser::browseFor*` / `showDialog`, the synchronous `AlertWindow` and `NativeMessageBox` forms,
   `PopupMenu::show`, `DialogWindow::showModalDialog`, `Component::runModalLoop`,
   `MessageManager::runDispatchLoopUntil` and `ModalComponentManager::runEventLoopForCurrentComponent`. A text gate
   covers what the compiler cannot see: the flag on the `YesDaw` target; and in `src/`, no `JUCE_MODAL_LOOPS_PERMITTED`
   (so tests never run code the app does not), no `runDispatchLoop`, no `AlertWindow::show*Box` (a `nullptr` callback
   still compiles and loses the answer), and no raw Win32 `MessageBox`, `DialogBox*`, `TrackPopupMenu` or `GetMessage`.
   No new timer is added (ADR-0067 §7's two).
2. **A question seam answers through a reply.** The thirteen `choose*` / `confirm*` seams of `MainComponentFileChoices`
   (`chooseNewProjectBundle`, `chooseOpenProjectBundle`, `chooseSaveAsProjectBundle`, `chooseSaveACopyProjectBundle`,
   `chooseSaveAsTemplateName`, `confirmReplaceTemplate`, `chooseImportAudioFile`, `chooseExportAudioFile`,
   `chooseImportMidiFile`, `chooseExportMidiFile`, `chooseSamplerPadFile`, `chooseMissingAudioReplacement`,
   `confirmCloseUnsavedChanges`) take a small `Question` type built from either a function that returns the answer
   (tests, as today) or one handed a reply to call later (the native shell). The answer keeps today's type and meaning:
   an empty path or name is a cancel.
   - The reply is copyable with shared one-shot state: a second call is ignored (and asserts); a reply destroyed uncalled
     ends the flow as refused (rule 5). An answer-later question copies what it needs from its arguments (a
     `const UiMissingAsset&` does not outlive the call).
   - `Question`'s call operator is private and friended to the shell's one ask helper, so the compiler keeps every ask
     on the helper.
   - A held-answer test needs no production seam: it builds the answer-later form, keeps the reply, and calls it later
     or drops it.
   - `newProjectDialogChoices`, `makeNewProject` and the device seams stay plain. The two inline three-way boxes (New /
     Open's unsaved-changes question when `nativePrompts`, and the close confirmation) are asked the same way, stay
     shell-owned and keep their own wording ("Don't Save" / "Close without saving").
   - The native relink question keeps its inner loop (a cancelled chooser shows the alert again) and ADR-0062's keys.
3. **A function that asks returns nothing, takes its continuation, and asks as its last statement.** Nothing that
   depends on the answer runs after the question; it moves into the continuation. Generic dispatch tails (refresh,
   repaint, probe stamps after any action) may stay. The continuation is a required parameter, so the compiler finds
   every caller (`confirmClose`, `confirmReplaceProject`, `saveCurrentProject`, `relinkMissingAudio`,
   `openProjectBundleAtPath`, the Sampler pad's refresh). Every outcome - answered, cancelled, or refused - runs the
   flow's closing steps (ADR-0068 §4's `endReplaceAttempt`, the refresh, repaint and title).
4. **A continuation checks it still belongs.** It holds the shell by `SafePointer` and does nothing if the shell is
   gone; the shell's destructor marks it closing first, so no continuation runs during it. One that acts on the project
   checks the model's attach generation (a new `UiAppModel::attachGeneration()` getter) is the one it asked under - not
   the bundle path, which Save As changes without a new attach (a close that saves the untitled session through Save As
   must still quit).
5. **One question at a time.** The shell marks a question open just before it calls the seam and clears the mark
   before the continuation runs, so a chain can ask its next question (close, then Save As; the relink alert, then its
   chooser). It never touches the mark after the seam returns (an immediate answer has already run). A flow that would
   ask while one is open is refused, which the continuation tells apart from a cancel: it receives
   `std::optional<Answer>`, empty when refused. A refused flow runs only its closing steps - no cancel message, no
   fallback - and the status line is unchanged. The launch-time question is never refused: the UI tick keeps it for the
   next tick that finds no question open. The FX preset prompt counts as a question; the non-modal autosave recovery
   prompt (ADR-0068) and the New Project overlay do not.
6. **The native questions.** A file chooser owns itself: a `shared_ptr` captured in its own `launchAsync` callback (JUCE
   moves the callback out before calling it), so no chooser is destroyed open inside the shell's destructor, whose
   message pump would run there. An alert is an `AlertWindow` entered into modal state with a callback and deleted by
   it (the FX preset prompt's pattern); Save as Template reads its name field in that callback. A continuation puts
   keyboard focus back on the component that had it before the question (the Windows chooser runs on a worker thread
   and the blocking paths' focus restore does not run for it).
7. **Quit.** `void mainComponentRequestQuit (juce::Component&, std::function<void()> quit)` is the asynchronous entry
   point: `Main.cpp`'s `closeButtonPressed` and a new `systemRequestedQuit` override (macOS Cmd+Q, a Windows logoff;
   today they skip B37's confirmation) both call it, and held-answer tests use it. On Save (once saved) or Close without
   saving it calls `quit` - `JUCEApplication::quit()`, never `systemRequestedQuit()`, which would ask again. A quit
   request while a question is open is refused, as JUCE already ignores `WM_CLOSE` under a modal component (a logoff
   then shows YES DAW as blocking). The harness wrappers that need an at-once answer - `mainComponentConfirmsClose` (13
   test sites) and the harness constructor's relink path - keep their shape: a missing answer counts as Cancelled (and
   asserts), and their continuations capture no stack locals by reference, so a late answer does nothing.
8. **The relink loop (ADR-0062)** asks about each missing file in turn, each next question from the previous answer's
   callback; the probe's question (`relinkAsking`) stays up until its answer arrives; outcomes and status messages are
   unchanged. The launch-time open does not fall back to the untitled session until the relink answer arrives.

## Consequences

- **Positive:** the window and the question's own alert repaint under every question; Cmd+Q and a logoff confirm
  unsaved changes as the close button does. The structural rules are compile-time (rules 1-3) and unit-tested with held
  answers (rules 3-5, 7-8); the paint itself is checked on a Windows drive.
- **Negative / accepted costs:** each asking flow and its callers are rewritten; code after a question runs on a later
  message; the Windows chooser is not owned by the main window (rule 6 restores focus; a click on the main window can
  still bring it in front of the chooser).
- **Units** (UI correctness pass FIX 29, each with gates red on the old code and named mutations; each lands whole, so
  no unit leaves the shipped app's close or open broken):
  - **29a** the `Question` type and reply, the ask helper with the open-question mark and focus restore, the
    `attachGeneration()` getter; the single-question flows (Import / Export Audio and MIDI, the Sampler pad, Save a
    Copy, Save as Template).
  - **29b** Save / Save As, both unsaved-changes questions, New / Open / Open Recent with ADR-0068's closing steps, and
    quit (`mainComponentRequestQuit`, `closeButtonPressed`, `systemRequestedQuit`) - together, since the close question
    and the quit that answers it cannot land apart.
  - **29c** the relink loop, an `openProjectBundleAtPath` completion, the launch-question tail (SS8 runs).
  - **29d** `JUCE_MODAL_LOOPS_PERMITTED=0` on `YesDaw` and the text gate (red on the old code: `YesDaw` does not build).
  - **29e** (Windows drive, off CI) the probe's `frame.paintCount` climbs under the SS8 launch-time alert and under a
    File-menu Open chooser and unsaved-changes alert opened by mouse - at least the larger of 5 and half a no-question
    baseline taken in the same run - and Space works after a chooser without a Focus step.
- **Follow-ups:** CONTEXT.md gains no term.
