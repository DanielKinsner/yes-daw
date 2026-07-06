# H15 closeout adversarial review

- **Date:** 2026-07-06
- **Reviewer:** Codex successor baton
- **Repo state reviewed:** `main` @ `84b83537030ef8886e7403a62691b2a8133c4e4e`
- **Baseline:** `docs/plans/2026-07-03-h15-automation-plan.md`, ADR-0039, `loop/horizon.md`,
  `STATUS.md`, and the H14-H17 adversarial packet review.
- **Result:** no H15 closeout-blocking defect found.

## Baton re-verification

- `git pull --ff-only` was already up to date.
- Commit `84b83537030ef8886e7403a62691b2a8133c4e4e`
  (`docs(h15): mark implementation gates complete`) is present at local `HEAD`, `main`, and
  `origin/main`.
- GitHub Actions run `28768633340` for `84b83537030ef8886e7403a62691b2a8133c4e4e` completed
  successfully on Linux, Windows, macOS, RTSan, and TSan.
- No narrower H15 review artifact existed in `docs/reviews/`, so this pass used
  `docs/reviews/2026-07-03-adversarial-review-h14-h17-packet.md` as the stale-claim baseline and
  re-checked it against current source/tests/CI.

## Local verification

- `git diff --check` passed.
- `ctest --test-dir build-ci -R "YesDaw(Automation|FxAutomation|Builder|MixerProjection|Fader|Pan|Scheduler|Playback)Check" --output-on-failure`
  passed the executable-style H15 gates it selected: `YesDawAutomationCheck`, `YesDawFxAutomationCheck`,
  `YesDawPlaybackCheck`, and `YesDawSchedulerCheck`.
- Direct Catch2 filters passed for the discovered-case gates:
  `YesDawBuilderCheck.exe "[builder][automation]"`, `YesDawMixerProjectionCheck.exe "[automation]"`,
  `YesDawFaderCheck.exe "[fader][automation]"`, `YesDawPanCheck.exe "[pan][automation]"`,
  `YesDawProjectCheck.exe "[project][automation]"`, `YesDawPersistenceCheck.exe "[automation]"`, and
  `YesDawRuntimeCheck.exe "[runtime][automation]"`.

## Review findings

None.

## Evidence checked

- **Prior BLOCKER: root-slot automation misses downstream event consumers.** Current code emits compiled
  automation into the additive `ProcessArgs::automationEvents` side-band in `CompiledGraph::process`
  and passes that pointer to every node. `FaderNode`, `PanNode`, and the H14 FX nodes consume both
  regular events and automation side-band events. The biting gate
  `CompiledGraph side-band automation reaches downstream event-slot consumers` proves the target
  fader's regular event input is not the root slot while automation still reaches it.
- **Prior BLOCKER: scheduler refusal can pass for the wrong reason.** `GraphBuilder` now forces
  `blockParallelSafe = false` whenever compiled automation lanes are present, independent of node
  properties and graph latency. `YesDawSchedulerCheck refuses zero-latency fader-only automated graphs`
  proves the no-automation control is zero-latency and block-parallel-safe, then adding only a Track
  fader automation lane makes the graph unsafe and the scheduled render refuse with
  `GraphNotBlockParallelSafe`.
- **Storage and validation.** Schema v8 persists `automation_lanes` and `automation_breakpoints`, migrates
  v7 bundles to empty automation tables, opens the frozen H15 schema-v8 fixture, and rejects orphan
  owners, unknown roles, invalid strip/FX params, duplicate targets, orphan breakpoints, duplicate ticks,
  out-of-range values, and quarantined curves.
- **Undo/property.** `ProjectUndo` covers add/remove automation lanes and breakpoint add/move/value/curve/
  remove commands, and the randomized automation edit sequence fully undoes and redoes to bit-identical
  Project values.
- **Projection/runtime.** `ProjectMixerProjection` resolves Track fader/pan, SendLevel, BusFader, and
  FX insert automation lanes to compiled frame-domain lanes. `GraphBuilder` rejects unresolved compiled
  targets and over-budget lane sets before publication. `CompiledGraph` emits exact breakpoint events
  plus absolute-frame-anchored 64-frame Linear control events, advances cursors across contiguous Blocks,
  and reseeks on discontinuous locate/loop Blocks.
- **Precedence.** `CompiledGraph::applySetGain` and `applySetPan` reject scalar posts for targets with
  matching compiled automation lanes; runtime tests prove fader and pan automation stay ahead of scalar
  posts.
- **Integration closeout.** `YesDawPlaybackCheck` includes automated fader/pan, send ride, and one FX
  ParamSpec lane each for EQ, Compressor, Delay, Reverb, and Limiter. Each parity case proves automation
  changes offline output versus a static negative control and requires realtime playback to match the
  automated offline render bit-for-bit at device Block sizes 1, 7, and 64 where applicable.

## Residual scope

- H15 is headless read-mode automation only. Lane editor UI, touch/latch/write recording, MIDI CC/expression
  lanes, per-clip automation, modulation, stepped/bool params, plugin hosting, and FX UI remain outside H15.
- No production code change was made in this review pass, so no new mechanical product behavior needed a
  new gate beyond the focused H15 gates and `git diff --check`.
