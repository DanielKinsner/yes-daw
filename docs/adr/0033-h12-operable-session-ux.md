# 0033. H12 operable session UX

- **Status:** Accepted
- **Date:** 2026-06-29
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0005 (mechanical verification - CI is the gate), ADR-0011 (asset/clip/project),
  ADR-0022 (playback transport model), ADR-0032 (H11 UI stack and app shell), H12 plan, `CONTEXT.md`.

## Context

H11 closed the native YES DAW app shell: the window launches, loads a Project bundle, draws the timeline,
projects mixer and piano-roll surfaces, exposes actions through the UI action registry, and keeps the H11
mechanical gates green. The next risk is not deeper plugin hosting. The next risk is whether a Project can
be operated as a session: create/open/save, import audio, select/edit timeline material, adjust mixer and
inspector state, edit Notes, drive transport, undo/redo, and reopen with the same state.

The supplied gold-standard mockup shows the product direction, but H11 still leaves much of that surface as
painted projection rather than real hit-tested controls. H12 exists to make the app functionally testable
before deepening plugin hosting.

The constraints stay the same:

- UI actions stay on the Control thread and cross into the engine through existing command/model surfaces.
- Every new user-visible mutation needs an action, command, or input-harness path that can be asserted
  without human judgment.
- Imported audio is copied into the Project bundle as immutable Assets; Clips remain non-destructive.
- The H11 frame-time and accessibility gates remain live while input wiring is added.

## Options considered

1. **Operable session UX first.**
   - Pros: turns the existing app shell into a usable, mechanically testable DAW workflow; exercises the
     Project bundle, asset import, edit commands, transport, mixer, piano roll, undo/redo, and save/reopen
     path together; delays plugin-hosting depth until the session surface can actually host it.
   - Cons: requires careful UI input harness design before visually exciting features expand.
2. **Deepen plugin hosting next.**
   - Pros: addresses known H3 follow-up depth around real plugin identity, scanning, validation, and insert
     UX.
   - Cons: plugin hosting is not required for the H12 exit; it would deepen a subsystem before the basic
     app workflow can be operated and tested.
3. **Recording/device UX next.**
   - Pros: connects H5/H8/H10 user-facing flows.
   - Cons: recording depends on a usable session surface for arming, placement, monitoring policy, and
     recovery prompts; it fits better after H12.
4. **Visual polish pass next.**
   - Pros: improves perceived product quality.
   - Cons: does not create a mechanical correctness gate and risks masking projection-only surfaces.

## Decision

H12 is **Operable Session UX**. It makes the existing YES DAW app shell functionally operable and
mechanically testable before plugin hosting is deepened.

H12 will add real input wiring and state parity for:

- Project lifecycle: new/open/save, recent/open path where practical, and save/reopen parity.
- Import: copy a WAV into the Project bundle as an immutable Asset and place Clips on the timeline.
- Timeline: hit-test tracks and Clips; select, drag/move, trim, split, fade, gain, snap, locate, and loop.
- Inspector and mixer: edit selected Clip fields, fader, pan, mute, solo, and read meters/loudness.
- Piano roll: select/move/length/transpose/quantize Notes through real input paths.
- Transport feedback: playhead, loop region, meter readback, and state changes visible through projections.
- Undo/redo: every H12 mutation must round-trip through the existing undo surface or declare a tracked gap.

The H12 exit gate is a self-asserting **UI input harness** that creates or opens a Project bundle, imports a
WAV, creates and edits Clips, drives transport/loop/locate, edits mixer and piano-roll state, saves,
reopens, and asserts state parity. The existing H11 focused gates stay green:
`YesDawUiActionCheck`, `YesDawAppSmokeCheck`, `YesDawTimelineGpuCheck`, and
`YesDawAccessibilityCheck`.

Plugin scanner, real plugin identity deepening, validation, blacklist UX, and plugin editor embedding are
explicitly deferred unless an H12 gate proves a hard dependency.

## Consequences

- **Positive:** the product becomes operable through a repeatable Project workflow before later feature
  deepening; H13 recording/device UX can build on real session input; H15+ plugin hosting work will have a
  usable insert/session surface to attach to.
- **Negative / accepted costs:** the first H12 work must build boring-but-critical input harness plumbing;
  visual polish stays subordinate to mechanical behavior.
- **Follow-ups:** add the H12 focused plan, update `CONTEXT.md` with Operable Session UX and UI input
  harness terms, update `loop/horizon.md`, and keep `STATUS.md` as the live checkpoint handoff.
