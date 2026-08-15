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

## H17 packaged entry point (decided; not built yet)

ADR-0040 replaces the eventual owner workflow with one root-level `verify-hardware.ps1` command in
the extracted Windows package. With no arguments it will select default hardware and run packaged
playback, recording, and frame stages, then emit one structured result, one plain verdict, and a
measurement-generated result row. It will not require this repository, a build tree, CMake, the app
UI, listening, or visual judgment.

Until that command lands, the individual commands below remain the available developer-facing
surfaces and cannot earn H17 packaged-artifact credit. The package-aware requirements live in
[`docs/plans/2026-07-28-h17-packaged-hardware-verifier-plan.md`](plans/2026-07-28-h17-packaged-hardware-verifier-plan.md).

## Smoke 1 — Hardware playback (available NOW)

- **What it proves:** a known Project plays out the real audio device with zero Underruns at a
  128-frame Block (the H8 exit clause; absorbs the open H0 real-hardware soak).
- **Run:** `tools/playback-smoke.ps1` (Windows) / `tools/playback-smoke.sh` (macOS/Linux).
- **Asserts:** exit 0 with a printed `PASS` line including device name, block size, duration, and
  `underruns=0`; any Underrun, device open failure, or early abort exits nonzero.
- **Cadence:** record a PASS now, then re-run after any change to `RuntimeAudioDriver`,
  `PlaybackEngine`, or device hot-swap code, and at every horizon close.

## Smoke 2 — One real VST3 across the worker boundary (BUILT 2026-08-14; awaiting a plugin)

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
- **BUILT (M14, 2026-08-14):** `pwsh tools/plugin-smoke.ps1` (harness `YesDawPluginSmoke`). It
  launches the real worker child, loads ONE named plugin file into it over the control lane,
  processes 32 blocks of a known signal through the OS-shared-memory RT lane, and asserts exactly
  the list above. `-Synthetic` runs the identical path against the in-repo synthetic worker plugin
  and is a CI test (`YesDawPluginSmokeSynthetic`), so the harness itself can never rot; `--version`
  is pinned too. Setup problems exit 2 (no worker binary, no plugin installed, a file that will not
  load as a plugin) — never a false FAIL and never a false PASS.
- **STILL NOT RUN with a real plugin:** the owner machine has **no VST3 installed**
  (`C:\Program Files\Common Files\VST3` is empty), so the harness honestly exits 2. The remaining
  owner action is: install one free redistributable VST3 that processes audio at its default
  settings, then run `pwsh tools/plugin-smoke.ps1` and log the line below. Until that PASS exists,
  ADR-0037's H18 precondition is NOT satisfied.

## Smoke 3 — Hardware recording round-trip (after H13 closes)

- **What it proves:** the H13 record flow works on a real device: arm a Track, record a burst of
  known signal (loopback cable or the device's own loopback if available), and verify the recorded
  Take lands as a canonical float-WAV Asset (ADR-0036) whose content is non-silent and whose
  placement matches the compensated-latency contract (ADR-0018) within the calibrated tolerance.
- **To build:** `tools/recording-smoke.ps1` following the playback-smoke pattern; where loopback
  ground truth isn't available, the smoke degrades explicitly to "capture + persistence + format"
  assertions and says so in its PASS line — it never silently weakens.
- **Asserts:** exit 0 `PASS` with device, latency values used, and alignment error in frames.

## Smoke 4 — H16 frame smoke (available NOW)

- **What it proves:** the H16 dense Timeline frame-time smoke stays under the 60 fps budget on the
  owner machine before H17 starts. The current command is the H16 headless frame-time proxy; if Dan
  does not accept that as the required windowed evidence, record an explicit H16 deferral in
  `STATUS.md` instead of writing a PASS.
- **Run:** `powershell -NoProfile -ExecutionPolicy Bypass -File tools\ui-frame-smoke.ps1`.
- **Asserts:** exit 0 by building/running `YesDawTimelineGpuCheck`, whose dense arrangement fixture
  fails if sustained frame time exceeds 16.6 ms or the rendered image is blank.
- **Cadence:** record a PASS/FAIL at H16 closeout, then re-run after Timeline renderer, shell paint, or
  dense-session performance changes.

## Result log (append-only; newest first)

Format: `YYYY-MM-DD | smoke | PASS/FAIL | machine | one-line detail (device, versions, numbers)`

When a classification is wrong, append a newer correction instead of deleting history. The newest
applicable row controls gate accounting; an administrative correction is not itself new smoke evidence.

| Date | Smoke | Result | Machine | Detail |
|---|---|---|---|---|
| 2026-08-14 | Smoke 2 — One real VST3 across the worker boundary | SETUP (exit 2) | Dan's Windows 11 box | The harness is BUILT and proven on the synthetic path — `YesDawPluginSmoke --synthetic` exits 0: `PASS: the synthetic worker plugin crossed the worker boundary; blocks=30 peak=0.2500 state-bytes=26 signal=passthrough-exact` (real worker child, real OS-shared-memory RT lane, real opaque-state round-trip), and that exact path is now a CI test. With a REAL plugin it exits 2: `SETUP: no VST3 installed and none named` — `C:\Program Files\Common Files\VST3` is empty on this machine. Not a PASS and not a FAIL: nothing third-party has crossed the boundary yet, so **ADR-0037's H18 precondition is still unmet**. Owner action: install one free redistributable VST3 that processes audio at its defaults, run `pwsh tools/plugin-smoke.ps1`, and log the result row. Negative controls also verified by hand: a non-existent path and a non-plugin file (`README.md`) both exit 2 with the honest reason. |
| 2026-08-14 | Smoke 3 — Hardware recording round-trip (shipped-path checker) | FAIL | Dan's Windows 11 box | `pwsh tools/shipped-record-check.ps1` exit 1 on "Speakers (Focusrite USB Audio)": `captured frames=144480 channels=1 peak=0.0000 burst-snr=0.00`, then `FAIL: coded burst not found in the capture (loopback route required)`. The shipped Record path ran end-to-end on real hardware (capture + commit through the exact button verbs); the burst cross-correlation correctly refuses to PASS with no physical loopback routed. Owner action for the full PASS: route an output into input 1 and re-run. |
| 2026-07-28 | Smoke 1 — Hardware playback (gate correction) | FAIL | Dan's Windows 11 box | The locked command requested a 128-frame Block, the Focusrite WASAPI device granted 480 shared-mode frames, and `soak.ps1` correctly exited 1 with `block 480 > target 128`. This supersedes the 2026-07-27 PASS classification for gate accounting. The separate `-BlockSize 480` exit-0 run remains useful stability evidence, but it changed the target and therefore is not a Smoke 1 PASS. Administrative correction from the already committed output; not new smoke evidence and not H17 packaged-artifact credit. |
| 2026-07-27 | Smoke 1 — Hardware playback | PASS | Dan's Windows 11 box | `playback-smoke.ps1 -Seconds 120 -BlockSize 480` on "Speakers (Focusrite USB Audio)", 48 kHz, exit 0: deadline_misses=0, device_error=false, max_block_ms=0.318/10.0. **Caveat: 480-frame shared-mode block, not the 128 H8 target** — this device's WASAPI floor is 480 shared / 144 exclusive (measured), so 128 needs an ASIO backend (owner decision). Run at the default 128 request, the script correctly FAILs with "block 480 > target 128". Pre-fix note: before `fix(soak)` this smoke soaked pure silence (track-less project → PlaybackEngine::create failed); this PASS is real rendered Project audio. |
| 2026-07-27 | Smoke 4 — H16 frame smoke | PASS | Dan's Windows 11 box | `ui-frame-smoke.ps1` exit 0: YesDawTimelineGpuCheck passed in 0.53 s on the ci-preset Release build at af9f58e. |
