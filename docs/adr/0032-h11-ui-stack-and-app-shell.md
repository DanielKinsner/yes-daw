# 0032. H11 UI stack and app shell

- **Status:** Accepted
- **Date:** 2026-06-29
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0002 (real-time engine foundations), ADR-0004 (JUCE 8 framework + own engine),
  ADR-0005 (mechanical verification - CI is the gate), ADR-0020 (H7-H11 roadmap), H11 plan,
  `CONTEXT.md`.

## Context

H11 is the capstone horizon: the first real YES DAW application window. H7-H10 deliberately built the
headless feature set first - offline render/export, playback transport, deterministic scheduling,
loudness, DAWproject export, time-stretch, and device hot-swap survival - so the UI can wire a stable
surface instead of chasing moving engine contracts.

The UI-stack fork is still open in the ADR index. The build plan recommended native JUCE Components plus
a GPU timeline canvas, but that recommendation was not yet locked. H11 cannot land application code until
this is decided, because the choice affects the process shape, accessibility model, command/keymap layer,
test harness, and whether `JUCE_WEB_BROWSER` becomes part of the app surface.

The hard constraints are inherited from ADR-0002 and ADR-0005:

- The Audio thread never allocates, locks, logs, or does I/O. UI work must stay on the Control thread or
  background workers and cross into the engine through bounded command surfaces.
- Every user-facing action needs an agent-native command equivalent so tests can drive it mechanically.
- The timeline must scroll at 60 fps under a self-asserting frame-time gate; human eyes are only for final
  visual feel, not for correctness.
- Accessibility is part of the H11 exit, not a postscript: visible controls need semantic roles, names,
  keyboard reachability, and parity with the command/keymap surface.

## Options considered

1. **Native JUCE Components for the app shell, with a dedicated GPU timeline canvas.**
   - Pros: matches ADR-0004; keeps the app in one C++ process and one toolchain; uses JUCE's native
     windowing, input, accessibility, audio-device, and plugin-hosting ecosystem; keeps `JUCE_WEB_BROWSER=0`;
     lets simple controls use normal Components while the dense timeline gets a measured renderer; easiest
     path to agent-native command parity because UI actions can call the same C++ command surface as tests.
   - Cons: we own more widget/layout code; the GPU timeline harness has to be built and measured rather
     than borrowed from a browser engine.
2. **WebView app shell over the C++ engine.**
   - Pros: fast iteration on layout, text, and styling; browser tooling is familiar; accessibility can be
     strong for ordinary document-like UI.
   - Cons: adds a second runtime and bridge; creates another message boundary between UI and engine; makes
     real-time-safe command boundaries harder to reason about; risks timer/input jitter; weakens native
     DAW integration; contradicts the build-plan recommendation and current `JUCE_WEB_BROWSER=0` stance.
3. **Pure JUCE Components with CPU painting only.**
   - Pros: simplest implementation and accessibility model; no renderer split.
   - Cons: does not answer the original H0 GPU timeline risk; dense timeline scroll and waveform rendering
     would fight the CPU frame budget as projects grow.
4. **Custom immediate-mode GPU UI for the whole app.**
   - Pros: strongest rendering control and consistent visuals.
   - Cons: accessibility, keyboard navigation, native menus/dialogs, text editing, and testability all
     become project-owned infrastructure; too much surface for H11.

## Decision

H11 uses **native JUCE Components for the app shell and ordinary controls**, plus a dedicated
**Timeline canvas** for the dense arrangement view. The Timeline canvas is allowed to use a GPU-backed
renderer, but the exact renderer backend is an implementation detail behind a narrow component boundary
and a mechanical frame-time harness.

H11 does **not** use WebView for the main app shell. The `YesDaw` target keeps `JUCE_WEB_BROWSER=0`
unless a later ADR accepts a narrow, isolated browser use.

The UI is a view over an explicit **UI action registry**:

- Every visible command has a stable action ID, default key binding, label, enabled/disabled reason, and a
  command-layer implementation or query.
- Tests and future agents drive the same action IDs as menus, buttons, shortcuts, and accessible actions.
- The app shell may read snapshots, meters, playhead, and Project summaries, but mutations go through
  command-layer surfaces that are testable without pixels.

The H11 mechanical gates are:

- **Agent-native parity:** every shipped visible action is registered, keyboard-reachable where relevant,
  accessible by role/name, and backed by a headless command or explicit read-only query.
- **App smoke:** a headless app model loads a Project bundle and drives H8 transport actions through the
  UI action registry.
- **Timeline frame-time:** the Timeline canvas scrolls a large arrangement fixture under the 60 fps budget
  with a self-asserting `max_frame_ms < 16.6` gate.

The final visual-feel check remains the one human spot-check named by ADR-0020, launched by a single
command. It cannot replace any mechanical gate.

## Consequences

- **Positive:** H11 can replace the H0 sine-spike window with a real single-window app while preserving
  the engine's real-time boundary; the command/keymap/accessibility surface is testable before the full
  visuals are polished; the dense timeline gets a measured renderer without forcing the mixer, transport,
  and dialogs into custom GPU UI.
- **Negative / accepted costs:** YES DAW owns its timeline renderer and frame-time harness; the first UI
  pass must build command registry plumbing before exciting visual work; WebView-based rapid iteration is
  off the table for the main shell.
- **Follow-ups:** update `CONTEXT.md` with UI action registry, Timeline canvas, Keymap, and Accessibility
  tree; add the H11 focused plan; land the first code checkpoint as an app-shell/action-registry slice
  before timeline GPU work.
