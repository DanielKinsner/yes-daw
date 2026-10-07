# G6.2 cp3 — the macOS `YesDawTimelineGpuCheck` frame exception, re-measured (ADR-0064)

Plan §8.2's standing exception (macOS `YesDawTimelineGpuCheck` failing only its sustained-frame budget) is due for its
G6.2 re-evaluation "on a measured runner/machine baseline". This is that baseline. No threshold was changed and no
run was repeated for luck. The §8.2 text belongs to the owner and is not edited here.

## What was measured

- **CI:** every completed run since the exception was written (2f48962, 2026-09-08) whose `macOS` and `Windows` jobs
  the GitHub API returned — 62 runs, 2026-09-08 21:45 → 2026-10-07 06:51 UTC — read with `gh api …/actions/runs` and
  each job's log. The check prints `max_frame_ms` / `sustained_frame_ms` / `slow_frames` only when it fails, so the
  numbers below are from failing runs; passing runs count as "sustained < 16.6 ms and slow frames within the CI
  allowance (8)".
- **Owner machine (Windows, this repo's `build-ci`):** the same test binary run alone ten times with Catch2 `-s`, which
  prints the numbers on a pass, 2026-10-08.

## Results

| Where | Runs | GPU check failed | Failed with nothing else failing | Sustained ms in failures (min / p25 / median / p75 / max) | Max frame ms in failures (min / median / max) |
|---|---|---|---|---|---|
| CI macOS | 56 | 41 (73 %) | 39 | 16.67 / 19.59 / 22.37 / 24.25 / 30.47 | 18.23 / 30.28 / 102.44 |
| CI Windows | 62 | 2 (3 %) | 2 | 17.06 / — / 17.48 / — / 17.91 | 20.04 / 22.10 / 24.16 |
| Owner machine (Windows), 10 runs | 10 | 0 | — | passes: 4.07 / 4.21 / 4.25 / 4.31 / 4.50 (all runs) | 4.27 / 4.58 / 4.87 (all runs) |

By day (failed / ran), macOS: 09-08 1/1, 09-09 0/3, 10-06 30/40, 10-07 10/12. Windows: 09-08 0/1, 09-09 0/3,
10-06 2/46, 10-07 0/12. Since the table was gathered: ea8c596 and 08be04a passed on macOS; 379b6ae and 1bc4a6f failed
(sustained 22.37 and 22.64 ms), each with nothing else failing.

## Reading

- **The renderer is not the bottleneck.** On the owner's machine the dense-arrangement paint holds 4.1–4.5 ms
  sustained, about a quarter of the 16.6 ms budget, with no slow frame in ten runs.
- **The macOS runner is.** The same code passes and fails: 9ed2983 passed and its child 86a0b31 failed with no change
  to any file the check compiles (`git diff` over `TimelineFrameCheck.h`, `TimelineCanvas.h`, `TimelineLayout.h`,
  `UiTheme.h`, `HardwareVerification.h` and the test is empty). Its failing sustained frame spans 16.7–30.5 ms — a
  spread no code change explains; most likely the shared macOS runner VM painting through CoreGraphics without a
  dedicated GPU (an inference: the runner's hardware is not visible to the job).
- **Windows CI** sits just under the budget (its two failures at 17.1 / 17.9 ms on one day), consistent with a slower
  shared runner, not a regression.

## Recommendation (for the owner's decision under §8.2)

1. **Renew the exception with this baseline (recommended).** Same narrow scope (macOS, `YesDawTimelineGpuCheck`,
   sustained budget only, nothing else failing), re-evaluated when the renderer, the test or the runner image changes,
   or if the owner machine's measurement rises above 8 ms sustained (half the budget).
2. **Make the macOS frame check informational in CI** (reported, never red), keeping the owner-machine
   `tools/verify-hardware.ps1` frame stage as the binding measurement. Same evidence, less noise.
3. **Renderer work for the macOS runner** (e.g. a GPU-backed timeline). Not justified by this data: the real machine
   has 4x headroom.
