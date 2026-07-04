# Reality lane — owner-machine smokes and their committed results

> **What this is.** The standing lane of one-command, self-asserting smokes that run on real
> hardware on Dan's machine — the checks CI *cannot* run. Each prints `PASS` or `FAIL` and asserts
> mechanically (exit 0/1); none asks a human to judge by ear or eye. Results are **committed to the
> log at the bottom of this file** so "it works on real hardware" is a dated fact in git, not a
> memory. Decided in ADR-0037; pattern from ADR-0005.
>
> **Why it exists.** The project's biggest structural risk is "green in CI, dead on a musician's
> machine." As of 2026-07-03 **no smoke below has ever recorded a PASS.** Every entry converts one
> reality risk into a cheap, repeatable fact.

## Smoke 1 — Hardware playback (available NOW)

- **What it proves:** a known Project plays out the real audio device with zero Underruns at a
  128-frame Block (the H8 exit clause; absorbs the open H0 real-hardware soak).
- **Run:** `tools/playback-smoke.ps1` (Windows) / `tools/playback-smoke.sh` (macOS/Linux).
- **Asserts:** exit 0 with a printed `PASS` line including device name, block size, duration, and
  `underruns=0`; any Underrun, device open failure, or early abort exits nonzero.
- **Cadence:** record a PASS now, then re-run after any change to `RuntimeAudioDriver`,
  `PlaybackEngine`, or device hot-swap code, and at every horizon close.

## Smoke 2 — One real VST3 across the worker boundary (small build task, then run)

- **What it proves:** the out-of-process hosting boundary (ADR-0015: spawned `YesDawPluginHost`
  worker, shared-memory RT lane, watchdog) survives contact with **real third-party plugin code**
  — not the synthetic passthrough processor. This is the cheapest possible de-risk of the entire
  H18/YES-family hosting bet, run years before H18 builds on it.
- **To build (one baton checkpoint, after H13 closes):** a `tools/plugin-smoke.ps1` that (1) points
  the existing worker at one named real VST3 on the owner machine (a free, redistributable synth or
  effect — pick one and pin its version in the log line), (2) loads it, processes N blocks of a
  known input through the RT lane, (3) asserts: no crash, no watchdog kill, no NaN/Inf in output,
  output differs from input (the plugin actually processed), opaque state chunk round-trips.
  **Guardrail:** this smoke must NOT grow into hosting features — no editor UI, no parameter
  surface, no scanner. Load → process → assert → exit.
- **Asserts:** exit 0 `PASS` with plugin name/version/hash; nonzero on any of the above.

## Smoke 3 — Hardware recording round-trip (after H13 closes)

- **What it proves:** the H13 record flow works on a real device: arm a Track, record a burst of
  known signal (loopback cable or the device's own loopback if available), and verify the recorded
  Take lands as a canonical float-WAV Asset (ADR-0036) whose content is non-silent and whose
  placement matches the compensated-latency contract (ADR-0018) within the calibrated tolerance.
- **To build:** `tools/recording-smoke.ps1` following the playback-smoke pattern; where loopback
  ground truth isn't available, the smoke degrades explicitly to "capture + persistence + format"
  assertions and says so in its PASS line — it never silently weakens.
- **Asserts:** exit 0 `PASS` with device, latency values used, and alignment error in frames.

## Result log (append-only; newest first)

Format: `YYYY-MM-DD | smoke | PASS/FAIL | machine | one-line detail (device, versions, numbers)`

| Date | Smoke | Result | Machine | Detail |
|---|---|---|---|---|
| — | — | — | — | *No reality-lane result has ever been recorded. First PASS goes here.* |
