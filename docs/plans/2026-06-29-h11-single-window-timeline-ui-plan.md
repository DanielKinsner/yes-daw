# H11 plan - Single-window timeline UI shell + accessibility

**Why this exists.** H7-H10 made the DAW's headless feature set real. H11 turns that into the first
usable application window: load a Project bundle, see the timeline, drive transport, inspect levels, edit
clips/Notes, and expose the H7-H10 features through a native app shell. ADR-0032 locks the UI stack before
code lands: native JUCE Components for the app shell, a dedicated Timeline canvas for dense rendering, and
an agent-native UI action registry as the seam between pixels and engine commands.

## Exit gate

H11 closes only when these gates are green in the full CI preset, plus the one-command visual-feel launch
has been handed to Dan:

- **Agent-native parity:** every shipped visible UI action has a stable action ID, label, default key
  binding where relevant, enabled/disabled reason, accessible role/name, and a headless command-layer
  implementation or explicit read-only query.
- **App smoke:** the app model loads a `.yesdaw` Project bundle, opens the single-window shell, and drives
  play/stop/locate/loop through the same action IDs used by menus, buttons, shortcuts, and accessibility.
- **Timeline frame-time:** the Timeline canvas scrolls a large arrangement fixture with
  `max_frame_ms < 16.6` under a self-asserting harness. Human eyes do not judge this gate.
- **Focused feature wiring:** transport, timeline clips, mixer levels, piano-roll Notes, loudness,
  DAWproject export, time-stretch, and device hot-swap controls all route through the action registry or
  explicit read-only projections.

The only subjective H11 check is visual feel. It is a one-command launch for Dan after the mechanical
gates are green; it does not replace CI.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure
```

The focused regex becomes fully active as the H11 gate targets land.

## Build order

Each checkpoint is one small, independently green commit. No UI code lands before the ADR that decides its
surface.

1. **Kickoff docs + UI-stack ADR. [done]** Accept ADR-0032, switch `loop/horizon.md` to H11, add this
   plan, update `CONTEXT.md`, `docs/adr/README.md`, `docs/goals/roadmap.md`, and the live handoff. This is
   a docs-only checkpoint.

2. **App shell + action registry.** Replace the H0 sine-spike window with a single-window JUCE shell that
   owns the UI action registry, default keymap, menus/toolbar placeholders, and a headless app model. Gate:
   `YesDawUiActionCheck` proves stable action IDs, keymap remapping, enabled/disabled state, and command
   dispatch without needing a display.

3. **Project-load smoke + transport controls.** Wire Project bundle open/load into the app model and drive
   H8 playback transport through action IDs. Gate: `YesDawAppSmokeCheck` loads a fixture bundle, calls
   play/stop/locate/loop through the UI action registry, and verifies the transport/readback state
   mechanically.

4. **Timeline canvas + frame-time harness.** Promote the existing `TimelineLayout` virtualization into the
   native Timeline canvas and add the GPU/frame-time measurement harness. Gate: `YesDawTimelineGpuCheck`
   scrolls a large arrangement fixture and fails unless `max_frame_ms < 16.6`.

5. **Timeline editing and clip affordances.** Surface clip move/trim/split/gain/fade and time-stretch
   controls through the action registry, backed by the existing Project edit/undo surfaces. Gate extends
   `YesDawUiActionCheck` with action-to-command parity and negative controls for disabled edits.

6. **Mixer, meters, and loudness surface.** Surface track/bus fader, pan, mute, solo, sidechain-visible
   state, per-track meters, and H10 loudness readings without changing engine policy. Gate proves action
   parity plus read-only meter/loudness projections.

7. **Piano roll and MIDI Clip surface.** Surface Note selection, move, length, transpose, quantize, and
   expression-lane readback for the H4 MIDI model. Gate proves action parity against the headless MIDI edit
   commands and keeps MIDI timing covered by existing gates.

8. **Accessibility pass + launch script.** Ensure every H11 visible control has a role/name, keyboard path,
   and action registry backing. Add the one-command launch Dan uses for visual-feel review. Gate:
   `YesDawAccessibilityCheck` plus full focused H11 lane.

9. **Close H11.** Run full local `ci`, focused H11 lane, push, verify remote CI, update `STATUS.md`,
   `loop/horizon.md`, and `docs/goals/roadmap.md`. If Dan's visual-feel launch finds a real issue, fix it
   as a focused checkpoint; otherwise mark H11 closed.

## Non-goals

- No WebView main shell.
- No new audio-engine behavior unless a UI gate proves a missing command/query seam.
- No plugin editor embedding; plugin editor UI remains a later ADR.
- No replacing mechanical gates with human visual or listening judgment.
- No advanced accessibility certification beyond semantic roles/names, keyboard reachability, and action
  parity for H11.

## Decisions to write

- **ADR-0032 - H11 UI stack and app shell:** native JUCE Components, dedicated Timeline canvas, UI action
  registry, agent-native parity, and WebView rejection. **[accepted in kickoff]**

Later H11 slices may add narrow ADRs only if the code needs a new irreversible decision. Otherwise they
should be small implementation checkpoints behind ADR-0032.

## Status

Opened on 2026-06-29 after H10 and the follow-on adversarial review patch batch were remote-green on
`main` (`dd3b257`, GitHub Actions run `28379340005`). ADR-0032 is accepted in this kickoff checkpoint.
The kickoff docs are green on remote CI run `28382745216`. The app shell + action registry checkpoint is
local-green and remote-CI pending: the H0 sine-spike window is replaced by the native JUCE shell, the shell
is aligned to the supplied dark DAW mockup direction, and `YesDawUiActionCheck` is in the full `ci` preset.
The next checkpoint after remote green is Project-load smoke + transport controls.
