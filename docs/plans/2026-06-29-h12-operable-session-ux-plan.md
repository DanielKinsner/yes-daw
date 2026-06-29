# H12 plan - Operable Session UX

**Why this exists.** H11 made the native YES DAW app shell real, but much of the gold-standard mockup is
still projection-only. H12 turns the shell into an operable session workflow: create/open/save a Project,
import audio, hit-test and edit timeline material, adjust inspector/mixer/piano-roll state, drive
transport, undo/redo, save, reopen, and prove the same state mechanically.

## Exit gate

H12 closes only when these gates are green in the full CI preset:

- **UI input harness:** a self-asserting test creates or opens a `.yesdaw` Project bundle, imports a WAV,
  creates and edits Clips, drives play/stop/locate/loop, edits mixer and piano-roll state, saves, reopens,
  and asserts state parity.
- **Action/input parity:** every H12 user-visible mutation is reachable through the UI action registry,
  direct Component input, or an explicit harness command path, with negative controls for disabled or
  invalid edits.
- **Persistence parity:** imported Assets, Clips, mixer values, MIDI Notes, loop/locate state where saved,
  and undoable edit results survive save/reopen through the normal Project bundle validators.
- **H11 regression lane:** `YesDawUiActionCheck`, `YesDawAppSmokeCheck`, `YesDawTimelineGpuCheck`, and
  `YesDawAccessibilityCheck` remain green.

Human visual polish can keep using the one-command launch, but it is not the H12 gate.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R YesDawUiActionCheck --output-on-failure
ctest --test-dir build-ci -R YesDawAppSmokeCheck --output-on-failure
ctest --test-dir build-ci -R YesDawTimelineGpuCheck --output-on-failure
ctest --test-dir build-ci -R YesDawAccessibilityCheck --output-on-failure
```

The `YesDawUiInputCheck` target lands during H12. Add it to the focused lane as soon as it exists.

## Build order

Each checkpoint is one small, independently green commit. H12 implementation code starts only after the
kickoff ADR/plan checkpoint is committed, pushed, and remote-green.

1. **Kickoff docs + H12 ADR. [done]** Accept ADR-0033, open `loop/horizon.md` to H12, add this plan,
   update `CONTEXT.md`, `docs/adr/README.md`, `docs/goals/roadmap.md`, and the live handoff. Docs-only.

2. **UI input harness skeleton.** Add `YesDawUiInputCheck` with a headless driver that can open the app
   model, target named UI regions, run deterministic pointer/key gestures, and assert failures. Gate:
   harness boots against the existing H11 fixture and proves disabled/invalid input negative controls.

3. **Project lifecycle controls.** Wire new/open/save through native JUCE controls or action-backed app
   model paths, keeping file dialogs isolated behind injectable test choices. Gate: create/open/save/reopen
   a `.yesdaw` Project through the harness.

4. **Import WAV into Project bundle.** Add the user-facing import flow that copies a WAV into the Project
   bundle as an Asset and places a Clip on a selected Track. Gate: imported bytes decode, Asset metadata
   persists, and the Clip references the Asset without editing it in place.

5. **Timeline hit-testing and edit gestures.** Make tracks and Clips real input targets for select,
   drag/move, trim, split, fade, gain, snap, locate, and loop. Gate: harness performs the edit sequence,
   undo/redo restores expected state, and `YesDawTimelineGpuCheck` stays green.

6. **Inspector and mixer controls.** Turn selected Clip fields, fader, pan, mute, solo, meter/loudness
   readbacks, and relevant tabs into real controls instead of painted-only projections. Gate: harness edits
   values, saves/reopens, and proves disabled-state behavior.

7. **Piano-roll input wiring.** Make Note selection, move, length, transpose, quantize, and expression
   readback operable through input paths. Gate: harness edits a MIDI Clip, save/reopen parity holds, and
   existing H4/H11 MIDI timing/action checks remain green.

8. **Transport feedback and session smoke closeout.** Tie playhead, loop region, meter feedback, selection,
   and saved Project state into one end-to-end scripted session. Gate: full H12 focused lane plus full
   local `ci`, then push and wait for remote CI.

## Non-goals

- No plugin scanner, plugin validation, plugin editor embedding, or real third-party plugin insert UX.
- No new audio-engine policy unless an H12 input gate proves the UI lacks a command/query seam.
- No recording, take lanes, device-selection UX, or monitoring policy; that is H13.
- No visual-only completion criteria.
- No destructive audio edits; imported audio stays as immutable Assets and Clips stay non-destructive.

## Decisions to write

- **ADR-0033 - H12 operable session UX:** session UX before plugin-hosting deepening; real input wiring;
  UI input harness; save/reopen parity; H11 gates remain live. **[accepted in kickoff]**

Later H12 slices may add narrow ADRs only if they introduce a new irreversible decision. Otherwise they
should land as small implementation checkpoints behind ADR-0033.

## Status

Opened on 2026-06-29 after H11 closeout was remote-green on `main` (`e9436af`, GitHub Actions run
`28405529686`). ADR-0033 is accepted and the kickoff docs checkpoint is remote-green on commit `7ad455e`
with GitHub Actions run `28408643608` passing Linux, Windows, macOS, RTSan, and TSan. Local
docs-checkpoint gates are green: `cmake --preset ci`, `cmake --build --preset ci`, and
`ctest --preset ci --output-on-failure` **249/249**. The focused current UI lane is also green with
`ctest --test-dir build-ci -I 237,240 --output-on-failure` **4/4**. The next checkpoint is the UI input
harness skeleton; no H12 implementation code has landed yet.
