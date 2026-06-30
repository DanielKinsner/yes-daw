# H12 plan - Operable Session UX

**Why this exists.** H11 made the native YES DAW app shell real, but much of the gold-standard mockup is
still projection-only. H12 turns the shell into an operable session workflow: create/open/save a Project,
import audio, hit-test and edit timeline material, adjust inspector/mixer/piano-roll state, drive
transport, undo/redo, save, reopen, and prove the same state mechanically.

## Exit gate

H12 closes only when these gates are green in the full CI preset:

- **UI input harness:** a self-asserting test that **constructs the real `MainComponent` shell**
  (headless, via the JUCE `MessageManager`) creates or opens a `.yesdaw` Project bundle, imports a WAV,
  creates and edits Clips, drives play/stop/locate/loop, edits mixer and piano-roll state, saves, reopens,
  and asserts state parity. The harness drives the **shipped window**, not a parallel model.
- **Action/input parity:** every H12 user-visible mutation is reachable by driving **real `MainComponent`
  input** - synthetic JUCE mouse/key events on the actual hit-tested Components - backed by the UI action
  registry, with negative controls for disabled or invalid edits. Asserting against the headless app model
  or a back-channel command path **does not** satisfy this gate; that is the H11 gap (the gates verified the
  library beneath the UI, never the shipped shell) and H12 must not repeat it.
- **Persistence parity:** imported Assets, Clips, mixer values, MIDI Notes, loop/locate state where saved,
  and undoable edit results survive save/reopen through the normal Project bundle validators. **Mixer
  values require the ADR-0034 schema** - the Project/bundle has no pan/mute/solo/bus fields today (see
  build order step 6).
- **H11 regression lane:** `YesDawUiActionCheck`, `YesDawAppSmokeCheck`, `YesDawTimelineGpuCheck`, and
  `YesDawAccessibilityCheck` remain green. Where H12 turns painted projections into real Components, the
  accessibility gate is **upgraded to query the real `AccessibilityHandler`s** of those Components, not just
  the static descriptor table.

Human visual polish can keep using the one-command launch, but it is not the H12 gate.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R YesDawUiActionCheck --output-on-failure
ctest --test-dir build-ci -R YesDawAppSmokeCheck --output-on-failure
ctest --test-dir build-ci -R YesDawUiInputCheck --output-on-failure
ctest --test-dir build-ci -R YesDawTimelineGpuCheck --output-on-failure
ctest --test-dir build-ci -R YesDawAccessibilityCheck --output-on-failure
```

The focused H12 UI lane is `YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check`.

## Build order

Each checkpoint is one small, independently green commit. H12 implementation code starts only after the
kickoff ADR/plan checkpoint is committed, pushed, and remote-green.

1. **Kickoff docs + H12 ADR. [done]** Accept ADR-0033, open `loop/horizon.md` to H12, add this plan,
   update `CONTEXT.md`, `docs/adr/README.md`, `docs/goals/roadmap.md`, and the live handoff. Docs-only.

2. **UI input harness skeleton. [done]** Add `YesDawUiInputCheck` with a headless driver that **constructs the
   real `MainComponent`** (via the JUCE `MessageManager`, no display), targets named UI regions, runs
   deterministic activation **against the actual Components**, and asserts failures. The skeleton starts
   with toolbar Button activation through JUCE's public click path; later non-button controls must add
   synthetic pointer/key gestures to satisfy the H12 exit gate. Gate: harness boots the real shell against
   the existing H11 fixture and proves disabled/invalid input negative controls. (`MainComponent`
   previously lived inside `src/Main.cpp` with no header - it is now extracted behind a testable entry
   point so a gate can construct it.)

3. **Project lifecycle controls. [done]** Wire new/open/save through native JUCE controls or action-backed
   app model paths, keeping file dialogs isolated behind injectable test choices. Gate: create/open/save/
   reopen a `.yesdaw` Project through the harness. The shipped `MainComponent` now owns `UiAppModel`,
   injects deterministic file choices for tests, creates/saves/opens the real bundle path through toolbar
   button clicks, and leaves transport disabled until a playback-ready/imported session exists.

4. **Import WAV into Project bundle (highest-value checkpoint - import must *play*, not just display).**
   Add the user-facing import flow that copies a WAV into the Project bundle as an Asset and places a Clip
   on a selected Track. Gate: imported bytes decode through the **same `DecodedAssetAudio` path
   `PlaybackEngine` consumes**, Asset metadata persists, the Clip references the Asset without editing it in
   place, **and the imported audio is audible - the harness renders a block through the engine and asserts
   non-zero output samples**. This closes the standing "decoder->source-node projection not built / the smoke
   injects pre-decoded PCM" gap (roadmap H1/H2 note); without the audible assertion, import only *shows*
   audio. May land as two commits: (4a) import + persist Asset/Clip, (4b) decoded audio reaches transport
   output.

5. **Timeline hit-testing and edit gestures.** Make tracks and Clips real input targets for select,
   drag/move, trim, split, fade, gain, snap, locate, and loop. **Pre-req:** promote `ui/TimelineLayout.h`
   from H0 "throwaway spike code, not the real UI" to production (update its header and drop the
   non-existent "GPU soak" promise) or replace it - hit-testing (pixels->clip, the inverse of `layoutVisible`)
   makes it load-bearing for real input. Gate: harness performs the edit sequence through real Component
   input, undo/redo restores expected state, and `YesDawTimelineGpuCheck` stays green.

6. **Inspector and mixer controls.** Turn selected Clip fields, fader, pan, mute, solo, meter/loudness
   readbacks, and relevant tabs into real controls instead of painted-only projections. **Depends on
   ADR-0034 (accepted mixer-state schema):** the next implementation checkpoint must add Track/Bus
   Project state and the bundle migration before mixer controls can claim save/reopen parity; this also
   resolves the H11 one-strip-per-Clip model (real tracks hold many clips). At least one meter/loudness
   value must be driven from the real engine, not echoed back from a test-injected constant. Gate: harness
   edits values through real input, saves/reopens, and proves disabled-state behavior.

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
- **ADR-0034 - Mixer-state schema and persistence:** add `Track`/`Bus` entities carrying ADR-0014 strip
  state (gain/pan/mute/solo/solo-safe/sidechain) to the Project model + a bundle migration, so the operable
  mixer can save/reopen and "tracks" stop being one-strip-per-Clip. This is an irreversible schema decision
  (ADR-0011/0012 ground) and **must be implemented before step 6 mixer-control edits claim persistence
  parity**. **[accepted]**

Later H12 slices may add narrow ADRs only if they introduce a new irreversible decision. Otherwise they
should land as small implementation checkpoints behind ADR-0033.

## Status

Opened on 2026-06-29 after H11 closeout was remote-green on `main` (`e9436af`, GitHub Actions run
`28405529686`). ADR-0033 is accepted and the kickoff docs checkpoint is remote-green on commit `7ad455e`
with GitHub Actions run `28408643608` passing Linux, Windows, macOS, RTSan, and TSan. Local
docs-checkpoint gates are green: `cmake --preset ci`, `cmake --build --preset ci`, and
`ctest --preset ci --output-on-failure` **249/249**. The focused current UI lane is also green with
`ctest --test-dir build-ci -I 237,240 --output-on-failure` **4/4**. The UI input harness skeleton landed in
the first H12 implementation checkpoint: `YesDawUiInputCheck` constructs the shipped `MainComponent`,
targets stable toolbar Component IDs, rejects disabled Play before Project load, and targets real toolbar
button Components. Local gates were green: VS DevShell
`cmake --build --preset ci --target YesDawUiInputCheck`, `ctest --preset ci -R YesDawUiInputCheck
--output-on-failure`, VS DevShell `cmake --build --preset ci --target YesDaw`, focused H12
`ctest --preset ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**5/5**, VS DevShell full `cmake --build --preset ci`, and `ctest --preset ci --output-on-failure`
**250/250**. That harness checkpoint is remote-green on commit `908ff08`, GitHub Actions run
`28412582848`. The Project lifecycle checkpoint is local-green: `UiAppModel` can create/open/save a
default session Project bundle, the shipped `MainComponent` drives New/Save/Open through injected file
choices, `YesDawUiInputCheck` reopens the `.yesdaw` through `ProjectBundleDb` and checks Project parity,
and empty-session transport stays disabled until the import/playback step supplies a playback-ready
session. Local gates are green: VS DevShell `cmake --build --preset ci --target YesDawUiInputCheck`,
`ctest --preset ci -R YesDawUiInputCheck --output-on-failure`, VS DevShell
`cmake --build --preset ci --target YesDaw`, focused H12
`ctest --preset ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**5/5**, VS DevShell full `cmake --build --preset ci`, and `ctest --preset ci --output-on-failure`
**250/250**. The next checkpoint is Import WAV into Project bundle, with the audible playback assertion
split as needed into small commits.

## Review notes (2026-06-29 adversarial pass)

The exit-gate, step 2, step 4, step 5, step 6, and "Decisions to write" edits above came from the
adversarial review in
[`docs/reviews/2026-06-29-adversarial-review-h11-h12.md`](../reviews/2026-06-29-adversarial-review-h11-h12.md).
The three load-bearing changes: (1) the harness must drive the **real `MainComponent`**, not the headless
model - closing the H11 gap where the gates verified the library beneath the UI but never the shipped
shell; (2) "mixer values survive save/reopen" needs **ADR-0034** (no mixer schema exists today); (3) import
must be **audible through the engine**, not just decoded for display. Steps 1-3, 7-8 are unchanged from the
original plan - they were already sound.

## Closeout

Closed locally on 2026-06-30 after the exit-gate audit fixed the remaining step 6 gap: selected Clip
gain/fade fields are now real inspector controls in the shipped `MainComponent`, not painted-only
projections. `YesDawUiInputCheck` drives those controls through JUCE Components, proves disabled-state
behavior, mutates Project Clip metadata, saves, reopens, and asserts parity. Local closeout gates are green:
`git diff --check`; VS DevShell `cmake --build --preset ci --target YesDawUiInputCheck`; direct
`YesDawUiInputCheck.exe` **832 assertions / 7 test cases**; focused H12
`ctest --preset ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**5/5**; VS DevShell full `cmake --build --preset ci`; and full
`ctest --preset ci --output-on-failure` **254/254**. H13 stays closed until this closeout commit is
remote-green.
