# Editing improvements — autonomous, headless delivery

Dan selected option A on 2026-11-03: work through the ranked editing improvements, handle verification, do not wait for a personal session, and test headlessly without interrupting mouse, focus or audio devices. This is the current implementation brief supplementing the active Real-DAW plan and ADR-0078; STATUS is the live progress handoff.

## Outcome and boundaries

Make editing continuous and musically consistent, then deepen arrangement and piano-roll composition. Preserve existing saved Projects, Audio-thread laws, graph/PDC contracts, non-destructive Assets and WAV focus. Architecture/file-format work gets a new in-scope ADR and critic before code depends on it. Existing Accepted ADRs, goldens and RT annotations remain unchanged. No new owner interview or routine approval gate is required.

## Delivery order

1. Keep MIDI editing continuous: honor addressed note release, then publish existing-Clip note schedules without replacing the playing engine. First support notes-only changes with unchanged Clip/controller/graph state; widen only with proof.
2. Prepare heavy media off the interactive path: cancellable, owned stretch preparation first, then staged/decode/resample import and atomic owner adoption. Capture request intent and reject stale results; no worker owns the model or SQLite handle.
3. Make the full Tempo map consistent across metronome, musical MIDI creation and interchange; preserve legacy SampleLocked meaning. Export repeated MIDI and supported tempo/meter changes faithfully. New timing/default/migration semantics need an ADR.
4. Add an explicit musical/fixed audio policy, known-tempo loop fit and tempo-follow; voiceover remains fixed by default. Internal transient warping is a later extension.
5. Add explicit linked editing and collapsible Track organization. Define matching/undo and boundary behavior before stored groups; named section moves are a later extension.
6. Add ghost reference notes and a used-pitches-only piano-roll view, with one unambiguous active editing target.
7. Reproduce and diagnose recorded startup Underrun/intermittent input failures with available headless evidence. Confirmed regressions interrupt feature work. Native-input or hardware-specific proof stays pending when headless simulation cannot establish it.

## Verification and completion

Choose an existing focused regression or one feature smoke test before each behavior change. Saved-data changes always retain their tests; engine/Audio-thread changes get a critic. Per logical change: build, relevant tests, existing pure suite, STATUS entry, explicit-path commit to main and push; promptly fix CI red. Full real-app drives remain a milestone proof and are not run on this shared desktop.

Local baseline: configure/build apps-off Release and run its full CTest suite. Compile apps-on without launching; only run audited offscreen harnesses. Do not run unfiltered apps-on CTest: native input cases, native-shell constructor, native AlertWindow styling and SessionDriveCapture must be excluded. No run-yesdaw, SendInput, hardware measurement binaries or audio-device startup. Remote CI retains its existing gates.

The implementation is complete only when the requested capabilities and their applicable mechanical gates are met. Do not certify future units, subjective feel, native interaction or hardware from pure/model tests. If host limits interrupt the work, STATUS names the exact completed commit, outstanding proof and next committable step.
