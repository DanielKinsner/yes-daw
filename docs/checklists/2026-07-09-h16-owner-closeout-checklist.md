# H16 owner closeout checklist

Use this once, after H16 mechanical implementation is remote-green, to close the owner-machine and
single human-session part of H16. Do not use this file as a substitute for the required result records:
record smoke results in `docs/reality-lane.md`, and record human-session findings as token/layout fixes
or explicit deferrals in `STATUS.md`.

## Preconditions

- `main` is clean and up to date with `origin/main`.
- H16 mechanical implementation is remote-green.
- No H17 work has started.
- The reference mockup is `docs/design/arrangement-view-reference.png`.
- Current UI screenshots, if needed for side-by-side review, are generated with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\ui-screenshot.ps1 -OutputDir build-ci\ui-screenshots
```

## Frame-smoke result

Run the available H16 frame-smoke command from the repo root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\ui-frame-smoke.ps1
```

Record exactly one of these outcomes before H17 opens:

- `PASS`: add a dated H16 frame-smoke PASS row to `docs/reality-lane.md` with machine and measured
  detail from the smoke output.
- `FAIL`: add a dated H16 frame-smoke FAIL row to `docs/reality-lane.md`, keep H16 open, and turn the
  failure into the next smallest mechanical fix.
- `DEFERRED`: if the available command is not accepted as the required owner-machine windowed evidence,
  record Dan's explicit deferral in `STATUS.md` before opening H17.

## Human session

Open the reference mockup and current screenshots side by side. Review once, not as an open-ended loop.

Check these H16 mockup surfaces:

- Arrangement/timeline: ruler markers, real waveform clips, playhead, track headers, tools, snap, and
  transport readouts.
- Inspector: clip timing fields, gain, fades, clip/track context, and automation affordances.
- Mixer: sends view, FX-slot readouts, GR/meter/loudness readouts, strips, buses, and master strip.
- Piano roll: note surface, expression lane, tool/keymap affordances, and panel switching.
- Header/export: export WAV button, progress readout, cancel surface, and action-backed controls.

Record exactly one of these outcomes before H17 opens:

- `ACCEPTED`: note in `STATUS.md` that the single H16 human session was accepted with no H16-blocking
  findings.
- `FIXES REQUIRED`: list each finding as a token/layout fix only, then make the smallest mechanical
  commits with gates before closing H16.
- `DEFERRED`: list explicit deferred items and Dan's deferral decision in `STATUS.md` before opening H17.
