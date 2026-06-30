# H13 plan - Recording and device UX

**Why this exists.** H5 proved recording alignment headlessly, H8 tied capture and autosave to playback
runtime, H10 proved device hot-swap survival, and H12 made the shipped app shell operable. H13 turns those
pieces into a scripted user-facing record flow: choose a device, arm a Track, set monitoring policy, record
audio and MIDI into bundle-owned Assets/Takes, edit basic take lanes/Comps, survive save/reopen/recovery,
and keep alignment/device gates green.

## Exit gate

H13 closes only when these gates are green in the full CI preset:

- **Recording UX harness:** a self-asserting test that constructs the real `MainComponent` selects a
  test/fake device, arms a Track, chooses a monitoring policy, records audio and MIDI through the shipped
  controls, creates bundle-persistent Take/Clip data, saves, reopens, exercises autosave recovery, and
  asserts Project parity. The harness drives the shipped window; model-only or back-channel commands do
  not satisfy this gate.
- **Recording alignment:** `YesDawRecordingCheck` remains green and any H13 bundle-facing recording gate
  proves recorded audio/MIDI lands within the ADR-0018 frame-placement contract, including a negative
  control that fails when latency compensation is omitted.
- **Device survival:** `YesDawDeviceHotSwapCheck` remains green and the H13 UX path maps device selection
  and refresh onto the ADR-0031 control-side coordinator. Device changes must not mutate device state from
  the audio callback.
- **Recovery parity:** recorded Takes, Clips, selected Comp ranges, arm/monitor state where saved, and
  recovery decisions survive normal save/reopen and valid autosave restore. A discard branch must also be
  asserted.
- **H12 regression lane:** `YesDawUiInputCheck`, `YesDawUiActionCheck`, `YesDawAppSmokeCheck`,
  `YesDawTimelineGpuCheck`, and `YesDawAccessibilityCheck` remain green as recording controls become real
  Components.

Hardware can have an owner-machine smoke, but H13 is not done by listening or watching. Any hardware lane
must be one command and print PASS/FAIL.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R YesDawRecordingCheck --output-on-failure
ctest --test-dir build-ci -R YesDawDeviceHotSwapCheck --output-on-failure
ctest --test-dir build-ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure
```

The H13 UX gate target name is expected to be `YesDawRecordingUxCheck` unless implementation finds an
existing test target is the cleaner home. Add it to the focused lane as soon as it exists.

## Build order

Each checkpoint is one small, independently green commit. H13 implementation code starts only after the
kickoff ADR/plan/status checkpoint is committed, pushed, and remote-green.

1. **Kickoff docs + H13 ADR. [done]** Accept ADR-0035, open `loop/horizon.md` to H13, add this plan,
   update `CONTEXT.md`, `docs/adr/README.md`, `docs/goals/roadmap.md`, and the live handoff. Docs-only.

2. **Recording UX harness skeleton.** Add the H13 focused harness around the real `MainComponent`. Gate:
   it can enumerate a deterministic fake/test device surface, prove Record is disabled with no armed
   Track/input, target the real recording controls, and keep the H12 UI focused lane green.

3. **Device selection + Track arming.** Wire device selection/refresh and Track arm/input selection through
   shipped controls and the UI action registry. Gate: selecting a device and arming a Track changes the
   control-side Project/UI state, invalid combinations are rejected, accessibility/action parity holds, and
   device refresh/hot-swap keeps ADR-0031 survival green.

4. **Bundle-persistent audio recording.** Connect the H5 writer output to Project bundle truth: recorded
   audio becomes an immutable Asset plus Take metadata, and a non-destructive Clip is placed on the armed
   Track. Gate: the harness records a deterministic signal, saves, reopens, decodes the recorded Asset
   through the same playback path as imported audio, and proves frame alignment with a biting
   no-compensation negative control.

5. **MIDI recording.** Record timestamped MIDI input into Project-frame Notes/Takes using ADR-0018
   placement and ADR-0017 MIDI Clip semantics. Gate: the harness records Notes at known offsets, save/reopen
   parity holds, and the H4/H8 MIDI timing/transport checks remain green.

6. **Monitoring policy + latency calibration.** Make monitoring mode explicit and scriptable. Gate: supported
   monitoring modes have mechanical assertions, direct-input and loopback placement use the right latency
   terms, and the fake-device calibration or one-command hardware smoke prints PASS/FAIL without asking Dan
   to listen.

7. **Take lanes and comp basics.** Surface recorded Takes on the timeline, allow simple lane selection and
   Comp range assembly, and keep Clips non-destructive. Gate: the harness creates multiple loop/punch Takes,
   selects Comp ranges, saves/reopens, undoes/redoes the selection, and verifies zero-filled gaps or muted
   ranges mechanically.

8. **Autosave recovery prompt.** Wire the existing autosave snapshot recovery surface into launch/open UX.
   Gate: the harness creates a valid autosave after recorded data, simulates a later abandoned edit, drives
   restore and discard choices through the shipped prompt, and proves the restored Project passes the normal
   bundle validators.

9. **End-to-end H13 record flow closeout.** Run one scripted session through device selection, arm, monitor,
   record audio/MIDI, take/comp basics, save/reopen, autosave recovery, and device survival. Gate: full H13
   focused lane plus full local `ci`, then push and wait for remote CI.

## Non-goals

- No plugin scanner, plugin validation, plugin editor embedding, or real third-party plugin insert UX.
- No advanced comp editor, comp flattening, playlist management, or vocal-production polish.
- No destructive audio edits; recorded audio is bundle-owned immutable data and Clips/Comps stay
  non-destructive.
- No subjective listening, visual inspection, or "Dan confirms the tone/alignment" gate.
- No driver crash recovery, sample-rate conversion, or channel-layout remap during device changes.
- No automation-lane recording beyond the existing MIDI/audio capture scope unless a later ADR brings it
  forward.

## Decisions to write

- **ADR-0035 - H13 recording and device UX:** recording/device UX follows H12 shipped-shell verification;
  device work stays control-side; monitoring/latency/recovery are mechanical; recorded audio becomes
  bundle-owned Assets plus Take metadata referenced by non-destructive Clips/Comps. **[accepted in kickoff]**

Write a narrow follow-up ADR before implementation if H13 needs a new irreversible bundle schema or
recorded-audio asset format that is not covered by ADR-0035 plus the existing ADR-0011/0012 migration
rules.

## Status

Opened on 2026-06-30 after H12 closeout was remote-green on `main` (`2dbb257`, GitHub Actions run
`28459661398`). ADR-0035 is accepted by the kickoff docs checkpoint. No H13 implementation code has
landed. Local docs-checkpoint gates are green: `git diff --check`; `cmake --preset ci`; VS DevShell
`cmake --build --preset ci`; and `ctest --preset ci --output-on-failure` **254/254**.
