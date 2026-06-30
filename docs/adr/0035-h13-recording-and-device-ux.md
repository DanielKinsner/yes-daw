# 0035. H13 recording and device UX

- **Status:** Accepted
- **Date:** 2026-06-30
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0002 (real-time foundations), ADR-0005 (mechanical verification), ADR-0011
  (asset/clip/project identity), ADR-0012 (SQLite bundle schema and atomicity), ADR-0018 (recording
  latency and take writer), ADR-0019 (autosave recovery), ADR-0022 (playback transport model),
  ADR-0031 (device hot-swap survival), ADR-0033 (H12 operable session UX), ADR-0034 (Track/Bus mixer
  state), H13 plan, `CONTEXT.md`.

## Context

H12 closed the shipped `MainComponent` as an operable session surface: a harness can create/open/save a
Project, import audio, edit Clips, mixer state, and Notes, drive transport, and prove save/reopen parity.
The next user-visible risk is the recording path. H5 proved a headless bounded FIFO, writer-thread take
file, and frame-aligned audio/MIDI placement. H8 gave recording and autosave their first playback-runtime
callers. H10 proved device hot-swap survival with a control-side coordinator. None of those surfaces are
yet a usable recording workflow in the YES DAW app.

The H13 decision is not "add a record button." It is to wire the proven headless recording, transport,
device, and recovery surfaces into a mechanically testable shipped-shell flow without weakening the
audio-thread contract or inventing subjective hardware/listening gates.

## Options considered

1. **Recording and device UX now.**
   - Pros: turns H5/H8/H10 into the next real session workflow; uses H12's shipped-shell harness instead
     of another headless-only proof; exposes the device and recovery risks before polish and plugin
     deepening.
   - Cons: requires a bundle-persistent Take/Clip surface and careful device/monitoring language before
     implementation code.
2. **Session production polish next.**
   - Pros: improves the non-recording workflow built by H12.
   - Cons: leaves the recording spine unwired and delays the roadmap's explicit H13 exit.
3. **Plugin hosting deepening next.**
   - Pros: addresses known H3/H15+ plugin depth.
   - Cons: plugin hosting is not required for recording, device selection, monitoring, or recovery prompts;
     deepening it now would skip the user-facing recording horizon.

## Decision

H13 is **Recording and device UX**. It wires H5/H8/H10 headless surfaces into the H12 shipped app shell and
closes only on self-asserting gates.

H13 implementation follows these constraints:

- The primary UX gate drives the real `MainComponent`, like H12's `YesDawUiInputCheck`; a headless model
  or back-channel command path is not enough.
- Device selection, device refresh, and device hot-swap stay control-side. The audio callback only calls
  the active engine/driver path and never mutates device lifetime or Project state.
- Track arming is a control-side Track state that selects where audio or MIDI capture lands. It must be
  action/input reachable, disabled when invalid, and save/reopen tested if it becomes saved Project truth.
- Monitoring policy is explicit. Record monitoring may be off, direct-input, or latency-compensated, but
  each supported mode must have a mechanical assertion; no H13 gate asks Dan to listen for alignment.
- Recorded audio becomes bundle-owned immutable Assets plus Take metadata. Timeline Clips and Comps remain
  non-destructive references to Takes/Assets. H5's `.ysdtake` file remains an internal test artifact unless
  a later ADR accepts it as a user-facing bundle format.
- Audio and MIDI recording use the same Project-frame placement model from ADR-0018 and the same absolute
  transport contract from ADR-0022.
- Autosave recovery prompts are user-facing branches over the existing autosave snapshot validator. Restore
  and discard choices must be scriptable and must not depend on visual inspection.
- Latency calibration is mechanical. Prefer a deterministic fake-device calibration gate in CI; if real
  hardware is needed, it must be a one-command owner-machine script that prints PASS/FAIL.

If H13 needs a new irreversible bundle schema or recorded-audio asset format beyond "Asset + Take metadata
referenced by non-destructive Clips/Comps," write a focused ADR before implementation code lands.

## Consequences

- **Positive:** H13 converts prior headless recording/device work into a user-facing DAW workflow while
  keeping the H12 shipped-shell verification standard.
- **Positive:** recorded material becomes normal Project truth: bundle-owned data, stable IDs, save/reopen
  parity, autosave recovery, and non-destructive timeline placement.
- **Positive:** monitoring, latency, and hardware smoke stay mechanical instead of becoming "listen and
  judge" checks.
- **Negative / accepted costs:** the first H13 checkpoints must do schema/UX groundwork before the record
  flow feels complete.
- **Deferred:** third-party plugin insert/editor UX, advanced comp editing, driver crash recovery,
  sample-rate/channel remap on device changes, and visual-only polish remain outside H13 unless a gate
  proves a hard dependency.
