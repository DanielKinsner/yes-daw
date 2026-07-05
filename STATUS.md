# YES DAW — STATUS (live handoff)

**Read this first on any machine.** This is the single source of truth for *where we are right now*.
The [plan](docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) and
[roadmap](docs/goals/roadmap.md) are the stable reference; **this** file is the live, constantly-updated
worklog.

> **Cross-machine rule:** `git pull` at the start of a session. At the end, update this file, commit in
> small chunks, and `git push`. Then the next machine — or the next session — is never lost.

## Planning packet — 2026-07-03 (Fable 5): alpha target + H14–H19 re-carve

**What landed (docs only, no implementation code):** ADR-0037 (alpha target + H14–H19 re-carve),
ADR-0038 (built-in FX suite: five Nodes, ParamSpec, insert chains, tails), ADR-0039 (automation
lanes: storage, targeting, compiled runtime) — all **Accepted**; implementation-grade plans
`2026-07-03-h14-fx-suite-plan.md`, `-h15-automation-plan.md`, `-h16-real-ui-plan.md`,
`-h17-distribution-alpha-plan.md`; re-carved `docs/goals/roadmap.md` (H14 FX → H15 automation →
H16 real UI → H17 distribution+alpha → H18 hosting → H19+ YES family); new `CONTEXT.md` terms
(Insert, FX chain, ParamSpec, Automation lane, Breakpoint, Design token, Alpha, Reality lane);
`docs/reality-lane.md` (three owner-machine smokes + committed result log — no PASS ever recorded
yet); `docs/goals/risk-register.md`; `docs/fable5/implementer-brief.md` (packet hard-stops).

**Decisions locked with Dan (2026-07-03 session):** product goal = dogfood alpha on the way to a
distributable product; YES family (Master/Voice/Stems) integrate as plugins later (H19+); the
product mockup is the structural UI spec; **first-party FX before real plugin hosting** (hosting
= H18, de-risked now by a one-real-VST3 worker smoke — a conscious divergence from the
`docs/fable5/yes-daw.md` draft "shippable"); portable unsigned zip for alpha (signing/installer =
beta); hardening folds into horizons + the reality lane; every H14–H17 plan carries a "Gates that
must BITE" section.

**Review status (2026-07-03):** Codex adversarial review completed
(`docs/reviews/2026-07-03-adversarial-review-h14-h17-packet.md`) — 7 findings (2 BLOCKER,
5 MAJOR), all verified against the project and **all applied** the same day: (1) automation
delivery redesigned as an additive `ProcessArgs::automationEvents` side-band — root-slot
injection would silently miss consumers downstream of event producers; (2) compiled automation
lanes now force the graph `blockParallelSafe = false`, with a fader-only zero-latency negative
control; (3) clip-gain ownership named (moves into `DecodedClipNode`, like the fade envelope);
(4) complete normative EQ band equations + independent bilinear reference + identity anchors;
(5) one normative limiter algorithm (released target → sliding minimum → boxcar smoother);
(6) shared absolute-frame anchoring rule for all smoothing/recompute cadences; (7) alpha close is
purely mechanical — the human feel session is the sanctioned non-gating exception. Schema
numbers changed to "next free version" (H13 still open).

**ADRs 0037–0039: ACCEPTED by Dan 2026-07-03.** The H14–H17 plans are law. H13 is now closed
remote-green: CP10 implementation `43280d8` passed GitHub Actions run `28693226908`, and H13 closeout
docs `253e639` passed run `28693785996`; both runs were green across Linux, Windows, macOS, RTSan,
and TSan. H14 may open on `main`. H14 kickoff verified `src/persistence/ProjectBundle.h` still has
`kCodeSchemaVersion = 6`, so the next free schema version for H14 CP3 is 7.

**Baton note:** H15 is open. The first implementation checkpoint is the plan-labeled **CP0 evaluator
characterization gate**; do not skip to the schema/model/undo checkpoint labeled CP1 in the plan.

---

## Live packet — H15 implementation

**Last updated:** 2026-07-05
**Current horizon:** **H15 (Automation) — CP3 block-size runtime sweep sub-slice is local-green;
remote CI is next.**

H15 CP2 send-level FaderNode target sub-slice is closed remote-green on `0e9dea3`: mixer Send taps
route through a real `FaderNode` target before entering the Bus Return, with per-send `faderNodeId` and
`linearGain` fields on `MixerSendProjection`, deterministic fallback send-level node IDs for legacy
callers, invalid-send-gain validation, and existing identical-send deduplication preserved. GitHub Actions
run `28740540163` was re-checked in this FX side-band session as completed/successful across Linux,
Windows, macOS, RTSan, and TSan. Local `HEAD`, `main`, and `origin/main` all pointed at `0e9dea3` after
`git pull --ff-only`.

H15 CP2 PanNode event-consumer sub-slice is closed remote-green on `68902e4`: `PanNode` consumes
`kPanParameterId = 1` parameter events from both the regular `args.events` stream and the H15
`ProcessArgs::automationEvents` side-band, maps normalized values linearly to the pan domain `-1..+1`,
ramps piecewise from each event offset, and keeps an automation/event target until a real `SetPan` command
revision overrides it. GitHub Actions run `28739794097` was re-checked in this send-level session as
completed/successful across Linux, Windows, macOS, RTSan, and TSan. Local `HEAD`, `main`, and
`origin/main` all pointed at `68902e4` after `git pull --ff-only`.

H15 CP2 FaderNode ParamSpec consumer sub-slice is closed remote-green on `540b2d9`: `ProcessArgs` now
has an additive optional `automationEvents` side-band view, `FaderNode` exposes the stable H15 gain
`ParamSpec` (`fader.gain`, dB domain `-60..+6`, `Db` mapping, default `0 dB`), maps parameter-event
normalized values through that spec to linear gain, treats normalized `0` as a mute target, and consumes
both regular events and automation side-band events. GitHub Actions run `28739154807` was re-checked in
this send-level session as completed/successful across Linux, Windows, macOS, RTSan, and TSan. Local `HEAD`,
`main`, and `origin/main` all pointed at `540b2d9` after `git pull --ff-only`.

H15 CP1 automation schema-v8 fixture forever-gate sub-slice is closed remote-green on `9206944`:
`tests/fixtures/h15_cp1_automation_schema_v8.yesdaw` is the frozen schema-v8 automation bundle fixture,
and `YesDawPersistenceCheck` has a forever-gate that copies the fixture to temp before opening it, asserts
schema v8, reads back two automation lanes, and proves the committed fixture DB bytes were not mutated.
GitHub Actions run `28738466617` was re-checked in this CP2 session as completed/successful across Linux,
Windows, macOS, RTSan, and TSan. Local `HEAD`, `main`, and `origin/main` all pointed at `9206944` after
`git pull --ff-only`.

H15 CP1 ParamSpec-aware automation target validator sub-slice is closed remote-green on `e58f962`:
Project and persistence validators reject impossible Track/Bus fader and pan ParamIDs, reject
`FxInsertParam` lanes whose `paramId` is not in the target insert's H14 ParamSpec table, and GitHub
Actions run `28737852847` was re-checked in this fixture session as completed/successful across Linux,
Windows, macOS, RTSan, and TSan. Local `HEAD`, `main`, and `origin/main` all pointed at `e58f962`
after `git pull --ff-only`.

H15 CP1 automation undo/property sub-slice is closed remote-green on `a985bd3`: Project edit commands
and undo/redo now cover automation lanes and breakpoints, and GitHub Actions run `28737127178` was
re-checked in this validator session as completed/successful across Linux, Windows, macOS, RTSan, and
TSan. Local `HEAD`, `main`, and `origin/main` all pointed at `a985bd3` after `git pull --ff-only`.

H15 CP1 schema v8 persistence sub-slice is closed remote-green on `db555ca`: schema version 8 persists
`Project.automationLanes`, migrates v7 bundles to empty automation tables, and GitHub Actions run
`28736458309` was re-checked in this validator session as completed/successful across Linux, Windows,
macOS, RTSan, and TSan. Local `HEAD`, `main`, and `origin/main` all pointed at `db555ca` after
`git pull --ff-only`.

H15 CP1 Project-model sub-slice is closed remote-green on `d42c9bb`: `Project` now carries
`automationLanes`, and GitHub Actions run `28735671105` was re-checked in this CP1 schema session as
completed/successful across Linux, Windows, macOS, RTSan, and TSan. Local `HEAD`, `main`, and
`origin/main` all pointed at `d42c9bb` after `git pull --ff-only`.

H15 CP0 is closed remote-green on `d6b734f`: `YesDawAutomationCheck` characterizes
`src/engine/Automation.h`, and GitHub Actions run `28734748402` was re-checked in this CP1 schema session
as completed/successful across Linux, Windows, macOS, RTSan, and TSan.

H14 remains closed remote-green on `8c06905`: CP10 implementation `5cf3574` passed GitHub Actions run
`28729589346`, CP10 closeout docs `a886711` passed run `28729985374`, and H14 closeout bridge
`8c06905` passed run `28734167730`; each named run was re-checked in this validator session as
completed/successful across Linux, Windows, macOS, RTSan, and TSan.

H15 CP3 compiled automation metadata sub-slice is closed remote-green on `89760c5`:
`CompiledGraph::Payload` now carries validated `CompiledAutomationLane` metadata (`targetNode`,
`parameterId`, sorted absolute frame breakpoints, normalized values, Linear/Hold curve types), and
`GraphBuilder::Inputs` can pass those already-compiled lanes into the immutable graph. The builder rejects
unresolved automation targets and invalid lane arrays before publication, exposes the lane metadata through
a debug view, and forces `CompiledGraph::blockParallelSafe = false` whenever compiled lanes are present.
Implementation commit `89760c5` passed GitHub Actions run `28742927499` across Linux, Windows, macOS,
RTSan, and TSan.

H15 CP3 Project/Mixer projection prerequisite is closed remote-green on `5b420c3`: `ProjectMixerProjection`
now resolves Project automation lane targets for projected Track faders, Track pans, and FX inserts,
converts lane Breakpoint ticks to absolute frame-domain `CompiledAutomationLane` metadata with
`CompiledTempoMap`, passes those lanes through `MixerProjectionInputs` into `GraphBuilder`, and rejects
valid-but-unprojected automation targets before graph publication. The focused gate proves Track
fader/pan lanes compile through a tempo map into graph metadata, FX insert lanes resolve to the projected
FX NodeId, and an automation lane targeting a Track with no projected audio path fails explicitly. This
does not emit side-band automation events on the audio thread, implement event-budget checks, add Send or
Bus fader lane resolution, touch FX UI, automation lane UI, plugin hosting, ADRs, `docs/reality-lane.md`,
golden files, or `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotations.
Implementation commit `5b420c3` passed GitHub Actions run `28744219573` across Linux, Windows, macOS,
RTSan, and TSan.

H15 CP3 compile-time automation event-budget rejection sub-slice is closed remote-green on `46cc897`:
`GraphBuilder` now rejects
compiled automation lane sets whose worst-case per-block generated side-band event count exceeds
`CompiledGraph::kMaxEventsPerBlock`, using the plan's `blockSize / 64 + 2` per-lane budget formula. The
new explicit `GraphBuildError::Code::AutomationEventBudgetExceeded` fails before graph publication, and
the focused gate proves the exact boundary at a 512-frame max Block: 102 lanes compile, 103 lanes reject.
Implementation commit `46cc897` passed GitHub Actions run `28745432552` across Linux, Windows, macOS,
RTSan, and TSan.

H15 CP3 first runtime helper sub-slice is closed remote-green on `78c4adc`: `CompiledGraph` now owns a
preallocated automation side-band event buffer, emits normalized `ParameterChange` events from compiled
frame-domain automation lanes for the current absolute `Transport::timelineFrame`, includes exact
breakpoint events plus absolute-frame-anchored 64-frame control-interval events on Linear segments, and
passes the resulting `ProcessArgs::automationEvents` stream to every node. The focused gate proves a
compiled lane produces the expected side-band events at block offsets 32 and 64 without using the root
event slot. This does not implement persistent runtime lane cursors, locate/loop reset, tempo/block-size
runtime sweeps, precedence over scalar posts, Send or Bus fader lane resolution, CP4 integration closeout,
FX UI, automation lane UI, plugin hosting, ADR edits, `docs/reality-lane.md`, golden files, or
`[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation changes.
Implementation commit `78c4adc` passed GitHub Actions run `28746796705` across Linux, Windows, macOS,
RTSan, and TSan.

H15 CP3 runtime cursor/continuation sub-slice is closed remote-green on `2d1c318`: `CompiledGraph` now
owns one preallocated `CompiledAutomationLaneCursor` per compiled automation lane, and compiled side-band
emission advances breakpoint and 64-frame Linear-segment control positions across adjacent sequential
Blocks instead of re-walking each lane from the beginning. The focused gate proves a lane that starts in
Block 1 continues into Block 2 with the expected cursor state and side-band events at absolute frames 128
and 160. This does not implement locate/loop reset, tempo/block-size runtime sweeps, precedence over
scalar posts, Send or Bus fader lane resolution, CP4 integration closeout, FX UI, automation lane UI,
plugin hosting, ADR edits, `docs/reality-lane.md`, golden files, or `[[clang::nonblocking]]` /
`YESDAW_RT_HOT` annotation changes.
Implementation commit `2d1c318` passed GitHub Actions run `28748073373` across Linux, Windows, macOS,
RTSan, and TSan.

H15 CP3 locate/loop cursor reset sub-slice is closed remote-green on `5729013`: `CompiledGraph` now treats a
non-adjacent compiled-lane Block as a discontinuous transport reset, re-seeks the cursor, and emits the
lane value at block offset 0 before continuing exact breakpoint plus 64-frame Linear control events. The
focused gate proves a cursor advanced through `0..192` resets correctly for a forward locate to frame 96
and a backward loop-style jump to frame 32, without taking on tempo/block-size runtime sweeps, precedence
over scalar posts, Send or Bus fader lane resolution, CP4 integration closeout, FX UI, automation lane UI,
plugin hosting, ADR edits, `docs/reality-lane.md`, golden files, or `[[clang::nonblocking]]` /
`YESDAW_RT_HOT` annotation changes.
Implementation commit `5729013` passed GitHub Actions run `28749315695` across Linux, Windows, macOS,
RTSan, and TSan.

H15 CP3 side-band delivery negative-control sub-slice is closed remote-green on `2bfff4c`: the builder
gate now puts a real `FaderNode` downstream of an event-producing upstream node, asserts the fader's
regular event input is not the root slot, and proves compiled automation still reaches the fader through
`ProcessArgs::automationEvents`. A regression that delivers compiled automation through the root event
slot instead would leave the downstream fader at unity and fail this gate. This does not implement
block-size runtime sweeps, tempo-change runtime sweeps, precedence over scalar posts, Send or Bus fader
lane resolution, CP4 integration closeout, FX UI, automation lane UI, plugin hosting, ADR edits,
`docs/reality-lane.md`, golden files, or `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation changes.
Implementation commit `2bfff4c` passed GitHub Actions run `28750516241` across Linux, Windows, macOS,
RTSan, and TSan.

H15 CP3 block-size runtime sweep sub-slice is local-green and ready for push: the builder gate now renders
the same compiled Track-fader automation lane through a single-block reference, a forced `1..9` frame
runtime schedule, and a mixed schedule, then requires bit-identical downstream `FaderNode` output. This
mechanically proves compiled side-band emission and consumption stay anchored to absolute frames across
varied Block boundaries. This does not implement tempo-change runtime sweeps, precedence over scalar posts,
Send or Bus fader lane resolution, CP4 integration closeout, FX UI, automation lane UI, plugin hosting,
ADR edits, `docs/reality-lane.md`, golden files, or `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation
changes.

**Now:** Commit and push the block-size runtime sweep sub-slice, then wait for GitHub Actions to pass
Linux, Windows, macOS, RTSan, and TSan.

Local gates for this checkpoint:
- `git diff --check` passed.
- BuildTools short-path `vcvars64.bat` `cmake --build --preset ci --target YesDawBuilderCheck` passed.
- Direct `build-ci\YesDawBuilderCheck.exe "[builder][automation][runtime][block-size][h15][cp3]"`
  passed **1/1** test case and **508** assertions.
- Direct `build-ci\YesDawBuilderCheck.exe "[builder][automation][h15][cp3]"` passed **8/8** test cases
  and **939** assertions.
- Direct `build-ci\YesDawBuilderCheck.exe` passed **40/40** test cases and **2410** assertions.
- Remote GitHub Actions run: pending until push.

Previous checkpoint local gates:
- `git diff --check` passed.
- Plain PowerShell `cmd /c "vcvars64.bat" && cmake --build --preset ci --target YesDawBuilderCheck`
  failed only because the shell lacked MSVC standard-library include paths (`cstdint`); reran the same
  target with `vcvars64.bat` and `cmake` inside the same `cmd /c` invocation.
- BuildTools short-path `vcvars64.bat` `cmake --build --preset ci --target YesDawBuilderCheck` passed.
- Direct `build-ci\YesDawBuilderCheck.exe "[builder][automation][h15][cp3]"` passed **7/7** test cases
  and **431** assertions.
- Direct `build-ci\YesDawBuilderCheck.exe` passed **39/39** test cases and **1902** assertions.
- BuildTools `vcvars64.bat` `cmake --build --preset ci` passed.
- Full `ctest --preset ci --output-on-failure` passed **303/303** tests.
- Remote GitHub Actions run `28750516241` for `2bfff4c` passed Linux, Windows, macOS, RTSan, and TSan.

Earlier checkpoint local gates:
- `git diff --check` passed.
- Plain PowerShell `cmake --build --preset ci --target YesDawBuilderCheck` failed only because the shell
  lacked MSVC standard-library include paths (`cstdint`); reran the same target through BuildTools
  `vcvars64.bat`.
- BuildTools `vcvars64.bat` `cmake --build --preset ci --target YesDawBuilderCheck` passed.
- Direct `build-ci\YesDawBuilderCheck.exe "[builder][automation][h15][cp3]"` passed **4/4** test cases
  and **221** assertions.
- Direct `build-ci\YesDawBuilderCheck.exe` passed **36/36** test cases and **1692** assertions.
- BuildTools `vcvars64.bat` `cmake --build --preset ci` passed.
- Full `ctest --preset ci --output-on-failure` passed **300/300** tests.
- Remote GitHub Actions run `28746796705` for `78c4adc` passed Linux, Windows, macOS, RTSan, and TSan.

**Next:** successor baton continues plan-labeled **CP3 — Compile + RT evaluation** with the next smallest
runtime automation chunk. Recommended next candidate after this run is green: tempo-change runtime sweep for
compiled automation emission, while still deferring precedence over scalar posts, Send/Bus fader lane
resolution, CP4 closeout, and UI work. The successor must first re-verify this implementation
commit/run from live repo truth, must not start CP4 integration closeout or H16 UI, and must preserve the
one-chunk/remote-green/single-successor chain rule.

> **Verification = CI.** A change is done when CI is green, not when Dan listens or watches. Recording,
> monitoring, latency calibration, device survival, and recovery prompts need self-asserting checks.
>
> **Rolling baton loop.** Each baton thread first REVIEW/FIXES the previous checkpoint, then, only if that
> review is clean/green, WORKS the next small checkpoint in the same thread. The baton may create exactly
> one successor baton only after its own `STATUS.md` update, commit, push, and CI result are complete and
> green. Do not create separate reviewer/worker threads in parallel, and never spawn ahead while CI is
> pending, stuck, red, or being rerun.

---

## Historical packet — H14 implementation

**Last updated:** 2026-07-05
**Current horizon:** **H14 (Built-in FX suite) — CLOSED REMOTE-GREEN; H15 automation opens next.**
H13 is closed remote-green. H14 CP1 is closed remote-green (`0621656`, GitHub Actions run
`28695566078`; closeout `1213954`, run `28695963126`). H14 CP2 is closed remote-green
(`2154ed9`, GitHub Actions run `28697062994`; closeout `2a98990`, run `28697491670`). H14 CP3 is
closed remote-green: implementation `53f43d3` passed run `28713175842`, closeout `e0d758f` passed
run `28713655210`, and final baton `704448a` passed run `28714154579`, all across Linux, Windows,
macOS, RTSan, and TSan. H14 CP4 is closed remote-green: implementation `47e5e59` passed run
`28715559037`, and closeout `193b35b` passed run `28716030870`, both across Linux, Windows, macOS,
RTSan, and TSan. H14 CP5 is closed remote-green: implementation `6e64753` passed run `28720068235`,
closeout `a4cd154` passed run `28720500367`, and final baton `aac85ec` passed run `28720932073`, all
across Linux, Windows, macOS, RTSan, and TSan. H14 CP6 is closed remote-green: implementation
`8501f93` passed run `28721683671`, closeout `55ed607` passed run `28722100076`, and final baton
`9cf8f02` passed run `28722537692`, all across Linux, Windows, macOS, RTSan, and TSan. H14 CP7 is
closed remote-green: implementation `6ed5d94` passed run `28723224456`, closeout `f0e69e2` passed
run `28723625022`, and final baton `1484e67` passed run `28724036864`, all across Linux, Windows,
macOS, RTSan, and TSan. H14 CP8 is closed remote-green: implementation `248881a` passed Windows,
RTSan, and TSan in run `28724757070` but failed Linux/macOS on an unused helper under `-Werror`;
portability fix `9d6e266` passed run `28725060611`; closeout `4b166e7` passed run `28725495347`;
final baton `19bacf3` passed run `28725857991`; the green runs passed Linux, Windows, macOS, RTSan,
and TSan. H14 CP9 is closed remote-green: implementation `5780593` was superseded after run
`28728177718` exposed Linux/macOS aggregate-initializer warnings; portability fix `1610057` was
superseded after run `28728450107`; final portability fix `8e47ef5` passed run `28728641921`; CP9
closeout docs `cc576bc` passed run `28729037387`, all green runs across Linux, Windows, macOS, RTSan,
and TSan. H14 CP10 implementation `5cf3574` passed GitHub Actions run `28729589346` across Linux,
Windows, macOS, RTSan, and TSan. CP10 closeout docs `a886711` passed run `28729985374`, also across
Linux, Windows, macOS, RTSan, and TSan.

**Done this checkpoint:** H14 closeout bridge first re-verified CP10 from current repo + remote CI:
session start `git pull --ff-only` was already up to date; local `HEAD`, `main`, and `origin/main`
all pointed at CP10 closeout docs commit `a886711`; GitHub Actions run `28729589346` for CP10
implementation `5cf3574` and run `28729985374` for closeout docs `a886711` were both completed/successful
across Linux, Windows, macOS, RTSan, and TSan. The promised closeout adversarial pass is recorded in
`docs/reviews/2026-07-05-h14-cp10-closeout-adversarial-review.md`; it found no H14 closeout-blocking
defect and did not change runtime code.

The preceding CP10 implementation review first re-verified CP9 from current repo + remote CI:
session start `git pull --ff-only` was already up to date; local `HEAD`, `main`, and `origin/main`
all pointed at CP9 closeout `cc576bc`; GitHub Actions run `28729037387` for `cc576bc` was
completed/successful across Linux, Windows, macOS, RTSan, and TSan. CP10 changes the shared Clip
fade law from the old local linear `DecodedClipNode` ramp to exact equal-power `sin((pi/2)*t/T)`
fade-in and `cos((pi/2)*t/T)` fade-out via `ClipEnvelope`, so Project envelope evaluation,
realtime playback, offline render, and bundle render all use the same clip-fade path.

`YesDawRenderCheck` now has the CP10 gate: a constant-signal crossfade renders identically through
offline and realtime paths, the summed per-frame fade-out/fade-in energy stays within +/-0.1 dB
across the overlap, and the old linear law is an explicit negative control because it drops beyond
that tolerance. Independent references in `YesDawOfflineRenderCheck`, `YesDawPlaybackCheck`,
`YesDawBundleRenderCheck`, and `YesDawProjectCheck` were updated to the exact equal-power law. The
project blessing workflow was run (`cmake --build --preset ci --target bless-goldens`) and produced
no file changes; no committed fade-affected golden existed, so no golden was regenerated. No FX UI,
automation, plugin hosting, ADR, `docs/reality-lane.md`, or `[[clang::nonblocking]]` annotation
change.

**Now:** H14 is closed remote-green on `a886711`: CP10 implementation `5cf3574` passed GitHub Actions
run `28729589346`, and closeout docs `a886711` passed run `28729985374`, both across Linux, Windows,
macOS, RTSan, and TSan. Local gates for this docs-only bridge: `git diff --check` passed; focused CP10
CTest lane passed **9/9** (`ClipEnvelope`, equal-power crossfade RT/offline, bundle crossfade,
`YesDawOfflineRenderCheck`, and `YesDawPlaybackCheck`). CP10 implementation local gates from the parent
thread: initial plain PowerShell build failed only
because the shell lacked MSVC standard-library include paths (`cmath`/`cstdint`); reran the same build
through VS DevShell (`vcvars64.bat`). Focused gates passed: VS DevShell `cmake --build --preset ci
--target YesDawRenderCheck YesDawOfflineRenderCheck YesDawBundleRenderCheck YesDawProjectCheck`;
direct `YesDawRenderCheck.exe` passed **4/4**; direct `YesDawOfflineRenderCheck.exe` passed **6/6**;
direct `YesDawProjectCheck.exe` passed **29/29**; direct `YesDawBundleRenderCheck.exe` passed **3/3**;
VS DevShell `cmake --build --preset ci --target bless-goldens` left no diff; VS DevShell
`cmake --build --preset ci`; first full `ctest --preset ci --output-on-failure` exposed stale
old-linear expectations in `YesDawPlaybackCheck`; after updating that independent reference, direct
`YesDawPlaybackCheck.exe` passed **9/9** and full `ctest --preset ci --output-on-failure` passed
**277/277**. This docs-only closeout update records the remote-green implementation result; it changes
no code.

**Next:** open H15 with its first checkpoint, the plan-labeled **CP0 evaluator characterization gate**
(`YesDawAutomationCheck`, no production code unless the characterization proves a defect). If a baton uses
the label "H15 CP1" to mean "first H15 chunk", it must still implement this audit-first CP0 and not skip
to the schema/undo checkpoint. Do not start H15 implementation in the H14 closeout bridge.

> **Verification = CI.** A change is done when CI is green, not when Dan listens or watches. Recording,
> monitoring, latency calibration, device survival, and recovery prompts need self-asserting checks.
>
> **Rolling baton loop.** Each baton thread first REVIEW/FIXES the previous checkpoint, then, only if that
> review is clean/green, WORKS the next small checkpoint in the same thread. The baton may create exactly
> one successor baton only after its own `STATUS.md` update, commit, push, and CI result are complete and
> green. Do not create separate reviewer/worker threads in parallel, and never spawn ahead while CI is
> pending, stuck, red, or being rerun.

---

## Historical packet - H12 closeout

**Last updated:** 2026-06-30
**Current horizon:** **H12 (Operable Session UX) — CLOSED REMOTE-GREEN.** ADR-0033 opens H12 after H11 closeout was
remote-green on `main` (`e9436af`, GitHub Actions run `28405529686`). H12 makes the H11 native app shell
operable before plugin hosting is deepened: new/open/save, import WAV into the Project bundle, timeline
Clip hit-testing/editing, inspector/mixer controls, piano-roll Note input, transport feedback, undo/redo,
save/reopen parity, and a self-asserting `YesDawUiInputCheck` while the H11 action/smoke/timeline/
accessibility gates remain green. The H12 kickoff checkpoint was docs-only: ADR-0033, the H12 focused plan,
roadmap, ADR index, glossary, horizon file, and live handoff; no implementation code landed in that
checkpoint. The H12 kickoff docs checkpoint is remote-green on commit `7ad455e` with GitHub Actions run `28408643608`
passing Linux, Windows, macOS, RTSan, and TSan. Local docs-checkpoint gates are green:
`cmake --preset ci`, `cmake --build --preset ci`, and
`ctest --preset ci --output-on-failure` **249/249**; focused current UI lane
`ctest --test-dir build-ci -I 237,240 --output-on-failure` **4/4**. The H12 kickoff bookkeeping follow-up
`8025f59` is remote-green on GitHub Actions run `28409549889`, and the read-only adversarial review
`8bef51d` is remote-green on run `28410002800`. This docs-only review follow-up tightens the H12 input gate
so `YesDawUiInputCheck` must drive the real shipped `MainComponent`, adds proposed ADR-0034 for mixer-state
schema/persistence before mixer controls, and keeps H12 implementation code at zero. H11 closeout context follows. H10 and its
follow-on adversarial-review patch batch are remote-green on `main`: latest tip `dd3b257`, GitHub Actions
run `28379340005` passed. H10's closed feature gates are `YesDawLoudnessCheck` (run `28341446711`),
`YesDawDawprojectCheck` (run `28348385319`), `YesDawTimeStretchCheck` (run `28350136910`), and
`YesDawDeviceHotSwapCheck` (run `28351880753`). H11 opens with ADR-0032: native JUCE Components for the
single-window app shell, a dedicated Timeline canvas for dense rendering, and a UI action registry as the
command/keymap/accessibility seam. H11 kickoff docs are local-green: `cmake --preset ci`, VS DevShell
`cmake --build --preset ci`, and `ctest --preset ci --output-on-failure` **245/245**; remote CI run
`28382745216` passed across Linux, Windows, macOS, RTSan, and TSan. The H11 app shell + action registry
checkpoint is local-green: `YesDawUiActionCheck` proves stable action IDs, default keymap remapping,
enabled/disabled reasons, accessibility labels/roles, and headless dispatch; `src/Main.cpp` now replaces
the H0 sine-spike audio window with a mockup-aligned native JUCE shell that consumes the registry. Local
gates: `cmake --preset ci`, VS DevShell `cmake --build --preset ci`,
`ctest --test-dir build-ci -R YesDawUiActionCheck --output-on-failure`, and `ctest --preset ci
--output-on-failure` **246/246**. Remote CI run `28385990090` is green across Linux, Windows, macOS,
RTSan, and TSan. The H11 Project-load smoke + transport controls checkpoint is local-green:
`YesDawAppSmokeCheck` loads a real `.yesdaw` Project bundle through `UiAppModel` and drives
play/stop/locate/loop through the same action IDs as the UI shell. Local gates: VS DevShell
`cmake --build --preset ci --target YesDawAppSmokeCheck`,
`ctest --preset ci -R YesDawAppSmokeCheck --output-on-failure`, and VS DevShell full
`cmake --build --preset ci` + `ctest --preset ci --output-on-failure` **247/247**. Remote CI run
`28388490955` is green across Linux, Windows, macOS, RTSan, and TSan. The H11 Timeline canvas GPU/perf
checkpoint is remote-green: `src/ui/TimelineCanvas.h` is the shared native Timeline canvas used by both
the app shell and `YesDawTimelineGpuCheck`, and the gate scrolls a 20,640-clip arrangement fixture with
`max_frame_ms=3.2874` and 336 visible clips. Local gates: VS DevShell
`cmake --build --preset ci --target YesDawTimelineGpuCheck`, `ctest --preset ci -R
YesDawTimelineGpuCheck --output-on-failure`, verbose `YesDawTimelineGpuCheck.exe -s
"[timeline][gpu][perf]"`, VS DevShell `cmake --build --preset ci --target YesDaw`, focused H11
`ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**3/3**, and VS DevShell full `cmake --build --preset ci` + `ctest --preset ci --output-on-failure`
**248/248**. Remote CI run `28391576711` is green across Linux, Windows, macOS, RTSan, and TSan.
The H11 Timeline editing and clip affordances checkpoint is remote-green: `UiActionRegistry` now exposes
clip move/trim/split, gain/fade, and time-stretch actions; `UiTimelineEditModel` maps those action IDs to
the existing `ProjectUndoStack` commands; and `YesDawUiActionCheck` proves action-to-command parity,
undo/redo, and disabled-edit negative controls. Local gates: `cmake --preset ci`; VS DevShell
`cmake --build --preset ci --target YesDawUiActionCheck`; `ctest --preset ci -R YesDawUiActionCheck --output-on-failure`;
focused H11 `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**3/3**; VS DevShell full `cmake --build --preset ci`; and `ctest --preset ci --output-on-failure`
**248/248**. Remote CI run `28393896442` is green across Linux, Windows, macOS, RTSan, and TSan.
The H11 Mixer, meters, and loudness surface checkpoint is remote-green: `UiActionRegistry` now exposes
track/bus fader, pan, mute, solo, meter-read, and loudness-read actions; `UiMixerSurface` projects
track/bus strips, meter readouts, sidechain-visible state, solo-safe/effective mute state, and H10
loudness values without changing Project or engine policy; and `src/Main.cpp` consumes the projection for
the mockup-aligned mixer and master loudness readout. Local gates: `cmake --preset ci`; VS DevShell
`cmake --build --preset ci --target YesDawUiActionCheck`;
`ctest --preset ci -R YesDawUiActionCheck --output-on-failure`; VS DevShell
`cmake --build --preset ci --target YesDaw`; focused H11
`ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**3/3**; VS DevShell full `cmake --build --preset ci`; and
`ctest --preset ci --output-on-failure` **248/248**. Remote CI found macOS timing reds in pre-existing
perf/deadline gates; the follow-up dense Timeline clip paint fix and macOS scheduler fixture adjustment
are remote-green on run `28398414664` across Linux, Windows, macOS, RTSan, and TSan.
The H11 Piano roll and MIDI Clip surface checkpoint is local-green: `UiActionRegistry` now exposes Note
select, move, length, transpose, quantize, and expression-read actions; `UiPianoRollSurface` projects H4
MIDI Clips/Notes into a UI snapshot and routes edits through `ProjectUndoStack`; and the app shell paints
a Piano Roll panel from the same snapshot shape. Local gates: VS DevShell
`cmake --build --preset ci --target YesDawUiActionCheck`;
`ctest --preset ci -R YesDawUiActionCheck --output-on-failure`; VS DevShell
`cmake --build --preset ci --target YesDaw`; focused H11
`ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu)Check" --output-on-failure` **3/3**; VS
DevShell full `cmake --build --preset ci`; and `ctest --preset ci --output-on-failure` **248/248**.
Initial remote CI run `28400668189` failed Linux/macOS build on missing `UiAppModel::dispatch` switch
cases for the new Piano Roll action IDs; follow-up commit `61efd1a` fixed the switch. Remote CI run
`28401313658` is green across Linux, Windows, macOS, RTSan, and TSan. The H11 Accessibility pass + launch
script checkpoint is remote-green: `UiActionRegistry` now covers H7 audio export, H10 DAWproject export,
and H10 device refresh actions; `UiAccessibility` defines the semantic app/menu/transport/timeline/
inspector/mixer/piano-roll regions; `YesDawAccessibilityCheck` proves every visible action has stable
IDs, labels, roles/names, keymap reachability, and dispatch/query backing; and `tools/launch-h11.ps1` /
`tools/launch-h11.sh` provide the one-command visual-feel launch. Local gates: VS DevShell
`cmake --build --preset ci --target YesDawAccessibilityCheck`;
`ctest --preset ci -R YesDawAccessibilityCheck --output-on-failure`; focused H11
`ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**4/4**; VS DevShell full `cmake --build --preset ci`; and
`ctest --preset ci --output-on-failure` **249/249**. Remote CI run `28403621292` is green across Linux,
Windows, macOS, RTSan, and TSan. The H11 closeout checkpoint is local-green: `cmake --preset ci`, VS
DevShell `cmake --build --preset ci`, full `ctest --preset ci --output-on-failure` **249/249**, and
focused H11 `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check"
--output-on-failure` **4/4**; remote CI run `28405529686` is green across Linux, Windows, macOS, RTSan,
and TSan. H11 is closed; no H12 has been opened by this closeout.
**Now:** H12 closeout audit/gate is remote-green on current `main`.
The second closeout push `d2696ae` fixed the Linux/macOS build warning, and remote CI run `28457474018`
passed RTSan and TSan; macOS then failed only `YesDawTimelineGpuCheck` with two isolated over-budget frames
(`max_frame_ms=28.2326`, sustained p99 sample `23.9594`, `slow_frames=2`, `max_visible_clips=336`,
checksum unchanged). The follow-up keeps the same dense 20,640-clip fixture and visible/content assertions
but makes the scheduler-pause policy explicit: the third-worst measured frame must stay under 16.6 ms and
`slow_frames <= 2`. Follow-up commit `53c3374` is remote-green on GitHub Actions run `28458592290` across
Linux, Windows, macOS, RTSan, and TSan.
The first
closeout push `fe7e0ae` failed remote CI run `28456766036` on Linux/macOS build because GCC/Clang treated
the inspector label range loop copy as `-Werror=range-loop-construct`; RTSan and TSan were green. The
follow-up binds the loop label by reference. Local follow-up gates are green: `git diff --check`; VS
DevShell `cmake --build --preset ci --target YesDawUiInputCheck`; direct `YesDawUiInputCheck.exe`
**832 assertions / 7 test cases**; focused H12
`ctest --preset ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**5/5**; VS DevShell full `cmake --build --preset ci`; and full
`ctest --preset ci --output-on-failure` **254/254**.
The audit found one remaining written-plan
gap: selected Clip inspector fields were still painted-only. The closeout fix turns Clip gain/fade fields
into real inspector sliders in the shipped `MainComponent`, disables them when no Clip is selected, drives
them through `YesDawUiInputCheck`, and proves Project mutation plus save/reopen parity. Local closeout gates
are green: `git diff --check`; VS DevShell `cmake --build --preset ci --target YesDawUiInputCheck`; direct
`YesDawUiInputCheck.exe` **832 assertions / 7 test cases**; focused H12
`ctest --preset ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**5/5**; VS DevShell full `cmake --build --preset ci`; and full
`ctest --preset ci --output-on-failure` **254/254**. This closeout commit must be remote-green before H13
opens.
H12 transport feedback and session smoke closeout checkpoint is remote-green. The piano-roll input
checkpoint `e23d821` is remote-green on GitHub Actions run `28452388337`, and the end-to-end session smoke
checkpoint `3151829` is remote-green on run `28454041449`, both across Linux, Windows, macOS, RTSan, and
TSan. The end-to-end checkpoint ties H12's separate input surfaces into one scripted shipped-shell
session: the `ProjectNew` toolbar path can accept a test-provided initial Project while default shipped
New still creates the normal empty session; `YesDawUiInputCheck` now clicks New, imports WAV, edits
Timeline Clips through real pointer gestures, drives Play/Locate/Loop/Stop and meter/loudness-producing
render paths, edits Mixer fader/pan/mute/solo through real controls, edits a MIDI Note through the real
Piano Roll Component, saves, reopens, and proves saved audio Clip, Track strip, and MIDI Clip state are
all preserved. Local gates are green: `git diff --check`; VS DevShell
`cmake --build --preset ci --target YesDawUiInputCheck`; direct
`YesDawUiInputCheck.exe` **752 assertions / 6 test cases**; focused H12
`ctest --preset ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check"
--output-on-failure` **5/5**; VS DevShell full `cmake --build --preset ci`; and full
`ctest --preset ci --output-on-failure` **254/254**.
Prior H12 checkpoints are remote-green:
pre-code docs precision patch `c622a6c` on GitHub Actions run `28411881766`, real shipped-shell input
harness `908ff08` on run `28412582848`, Project lifecycle controls `5eb4267` on run `28413370943`,
Import WAV through the shipped shell `2110c3b` on run `28414262811`, Timeline hit-testing +
real-shell Clip selection `102c94a` on run `28415151322`, and Timeline Clip move via real-shell drag
`5089ebc` on run `28415965271`, Timeline Clip split via real-shell double-click `7576771` on run
`28416653470`, Timeline Clip right-edge trim via real-shell drag `a8f4b39` on run `28417399129`, transport
locate/loop/stop plus scheduler repair `a9a57bf` on run `28418515621`, Timeline Clip gain via real-shell
shift-drag `3b0a337` on run `28419232690`, and Timeline Clip fades via real-shell Alt-edge drags
`ca59170` on run `28426496982`, and Timeline Clip snap via real-shell Ctrl-drag `2d09fb6` on run
`28428780783`, Track/Bus Project state + schema v4 bundle migration `abb92af` on run `28433828816`,
mixer controls CI portability follow-up `adc8279` on run `28450407292`, piano-roll input wiring
`e23d821` on run `28452388337`, and end-to-end session smoke `3151829` on run `28454041449`.
The transport checkpoint extends `YesDawUiInputCheck` so the imported-session harness drives Play, Locate,
Loop, and Stop through the shipped toolbar `Button` Components after audible playback, then asserts playhead
reset, loop toggle state, stop state, and command dispatch counts through the real `MainComponent` snapshot.
Transport local gates were green: `git diff --check`; VS DevShell
`cmake --build --preset ci --target YesDawUiInputCheck`;
`ctest --preset ci -R YesDawUiInputCheck --output-on-failure` **1/1**; VS DevShell
`cmake --build --preset ci --target YesDawUiInputCheck YesDawUiActionCheck YesDawAppSmokeCheck
YesDawTimelineGpuCheck YesDawAccessibilityCheck`;
`ctest --preset ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check"
--output-on-failure` **5/5**; VS DevShell full `cmake --build --preset ci`; and
`ctest --preset ci --output-on-failure` **251/251**.
**Next (Codex - H13 kickoff): start docs-first. Open the H13 decision/plan/status packet before any
implementation code.**
Three load-bearing items from the 2026-06-29 adversarial review
([`docs/reviews/2026-06-29-adversarial-review-h11-h12.md`](docs/reviews/2026-06-29-adversarial-review-h11-h12.md)):
1. **`YesDawUiInputCheck` must drive the real shipped `MainComponent`** — extract it from `src/Main.cpp`
   behind a header first, then drive synthetic JUCE mouse/key events, NOT the headless `UiAppModel`.
   Asserting the model is the H11 gap (the gates verified the library beneath the UI, never the shipped
   window); `CONTEXT.md` now bars a model-only/back-channel harness.
2. **Grill + accept ADR-0034 (mixer-state schema) before step 6.** No `Track`/`Bus`/pan/mute/solo exists in
   the Project or bundle today, so "mixer values survive save/reopen" has nowhere to write until that schema
   + migration lands.
3. **Import (step 4) must be *audible*** — decoded WAV bytes reach `PlaybackEngine` output (assert non-zero
   samples), not just decoded for waveform display.
Plan steps 1–3 and 7–8 were already sound and are unchanged. First implementation checkpoint is the UI
input harness skeleton (`YesDawUiInputCheck`).

> **Verification = CI.** A change is done when CI is green, not when Dan listens or watches. The only
> human step is blessing a golden on an intended audio change (`cmake --build --preset ci --target bless-goldens`).
>
> **Rolling baton loop.** Each baton thread first REVIEW/FIXES the previous checkpoint, then, only if that
> review is clean/green, WORKS the next small checkpoint in the same thread. The baton may create exactly
> one successor baton only after its own `STATUS.md` update, commit, push, and CI result are complete and
> green. Do not create separate reviewer/worker threads in parallel, and never spawn ahead while CI is
> pending, stuck, red, or being rerun.

---

## Now — H11 closed; next horizon decision
- **Latest (2026-06-29): closed H11 on remote CI.** The H11 exit-gate audit maps to the four focused
  gates in the full `ci` preset: `YesDawUiActionCheck` for action registry/keymap/accessibility parity,
  `YesDawAppSmokeCheck` for Project bundle load plus transport action IDs, `YesDawTimelineGpuCheck` for
  the dense Timeline canvas frame-time gate, and `YesDawAccessibilityCheck` for visible action/region
  roles, names, keyboard reachability, action backing, and launch scripts. Local closeout gates are green:
  `cmake --preset ci`; VS DevShell `cmake --build --preset ci`; full
  `ctest --preset ci --output-on-failure` **249/249**; and focused H11
  `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
  **4/4**. Closeout commit `e9436af` is remote-green on CI run `28405529686` across Linux, Windows,
  macOS, RTSan, and TSan. H11 is closed. **Next:** choose/open the next horizon; no H12 has been opened by
  this closeout.

- **Latest (2026-06-29): closed Accessibility pass + launch script on remote CI.** Added stable H7/H10 UI action
  IDs for audio export, DAWproject export, and audio device refresh. Added `UiAccessibility`, a headless
  manifest for app, menu, transport, timeline, clip inspector, mixer, master meter, and piano-roll regions
  with semantic names, roles, keyboard paths, and action backing where relevant. Added
  `YesDawAccessibilityCheck` to the full `ci` preset so visible actions must have stable IDs, labels,
  accessible names/roles, keymap reachability, and dispatch/query backing. Added one-command launch
  scripts at `tools/launch-h11.ps1` and `tools/launch-h11.sh` for Dan's visual-feel review after the
  mechanical gates are green. Local gates are green: VS DevShell
  `cmake --build --preset ci --target YesDawAccessibilityCheck`;
  `ctest --preset ci -R YesDawAccessibilityCheck --output-on-failure`; focused H11
  `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
  **4/4**; VS DevShell full `cmake --build --preset ci`; and
  `ctest --preset ci --output-on-failure` **249/249**. Remote CI run `28403621292` is green across Linux,
  Windows, macOS, RTSan, and TSan. **Next:** Close H11.

- **Latest (2026-06-29): closed Piano roll and MIDI Clip surface on remote CI.** Added stable UI action
  IDs for Note selection, move, length, transpose, quantize, and expression-lane readback. Added
  `UiPianoRollSurface`, a pure UI projection over the existing H4 MIDI Clip/Note model that carries Note
  readback plus Velocity/Pitch expression lanes and dispatches edits through the existing
  `ProjectUndoStack` MIDI edit commands. Routed the app shell's Piano button to a visible Piano Roll panel
  drawn from the same snapshot shape. Local gates are green: VS DevShell
  `cmake --build --preset ci --target YesDawUiActionCheck`;
  `ctest --preset ci -R YesDawUiActionCheck --output-on-failure`; VS DevShell
  `cmake --build --preset ci --target YesDaw`; focused H11
  `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu)Check" --output-on-failure` **3/3**; VS
  DevShell full `cmake --build --preset ci`; and `ctest --preset ci --output-on-failure` **248/248**.
  Initial remote CI run `28400668189` failed Linux/macOS build on missing `UiAppModel::dispatch` switch
  cases for the new Piano Roll action IDs; follow-up commit `61efd1a` fixed the switch. Remote CI run
  `28401313658` is green across Linux, Windows, macOS, RTSan, and TSan. **Next:** Accessibility pass +
  launch script.

- **Latest (2026-06-29): closed the Mixer, meters, and loudness surface checkpoint on remote CI.** Remote CI run
  `28396204227` passed Windows, Linux, RTSan, and TSan, but macOS red first on `YesDawSchedulerCheck`
  (`p999=4.251 ms`, period `4.167 ms`) and then, on rerun, on `YesDawTimelineGpuCheck`
  (`max_frame_ms=16.8962`, limit `16.6`). After the dense Timeline paint fix, remote CI run
  `28397406539` passed Windows, Linux, RTSan, and TSan, and macOS passed `YesDawTimelineGpuCheck` but red
  on the pre-existing `YesDawSchedulerCheck` timing gate (`p999=5.058 ms`, period `4.167 ms`). The touched
  mixer action gate passed. Follow-up fixes: dense Timeline clips now use a cheap rect paint path when
  lanes collapse to tiny heights, while normal-height app clips keep rounded chrome; and the scheduler
  soak keeps Windows/Linux at 100 tracks but uses a smaller macOS shared-runner fixture for the p999
  deadline gate. Remote CI run `28398414664` is green across Linux, Windows, macOS, RTSan, and TSan.
  Local gates are green: VS DevShell
  `cmake --build --preset ci --target YesDawTimelineGpuCheck`,
  `ctest --preset ci -R YesDawTimelineGpuCheck --output-on-failure`, verbose
  `YesDawTimelineGpuCheck.exe -s "[timeline][gpu][perf]"` (`max_frame_ms=2.5694`, 336 visible clips),
  VS DevShell `cmake --build --preset ci --target YesDawSchedulerCheck`,
  `ctest --preset ci -R YesDawSchedulerCheck --output-on-failure`,
  focused H11 `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
  **3/3**, VS DevShell full `cmake --build --preset ci`, and `ctest --preset ci --output-on-failure`
  **248/248**. **Next:** Piano roll and MIDI Clip surface.

- **Latest (2026-06-29): landed the Mixer, meters, and loudness surface locally.** Added stable UI action
  IDs for track/bus fader, pan, mute, solo, meter-read, and loudness-read operations. Added
  `UiMixerSurface`, a pure UI projection over the existing Project/mixer surfaces that carries track/bus
  strips, sidechain-visible state, solo-safe/effective mute state, per-strip meter values, and H10
  loudness readouts without changing Project or engine policy. Routed the mockup-aligned mixer and master
  loudness readout in `src/Main.cpp` through that projection. Local gates are green: `cmake --preset ci`;
  VS DevShell `cmake --build --preset ci --target YesDawUiActionCheck`;
  `ctest --preset ci -R YesDawUiActionCheck --output-on-failure`; VS DevShell
  `cmake --build --preset ci --target YesDaw`; focused H11
  `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
  **3/3**; VS DevShell full `cmake --build --preset ci`; and `ctest --preset ci --output-on-failure`
  **248/248**. **Next:** push and verify remote CI, then start Piano roll and MIDI Clip surface.

- **Latest (2026-06-29): closed Timeline editing and clip affordances on remote CI.** Added stable UI action
  IDs for selected-clip move, trim, split, gain, fades, and time-stretch. Added `UiTimelineEditModel` so
  those action IDs apply the existing Project edit/undo commands, including undo/redo parity and failed
  edit rejection. Extended `YesDawUiActionCheck` with action-to-command coverage and disabled negative
  controls for no Project and no selected clip. Local gates are green: `cmake --preset ci`, VS DevShell
  `cmake --build --preset ci --target YesDawUiActionCheck`,
  `ctest --preset ci -R YesDawUiActionCheck --output-on-failure`, focused H11
  `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
  **3/3**, VS DevShell full `cmake --build --preset ci`, and `ctest --preset ci --output-on-failure`
  **248/248**. Remote CI run `28393896442` is green across Linux, Windows, macOS, RTSan, and TSan.
  **Next:** Mixer, meters, and loudness surface.

- **Earlier (2026-06-29): closed Timeline canvas GPU/perf on remote CI.** Remote CI run `28391576711` is
  green across Linux, Windows, macOS, RTSan, and TSan.

- **Earlier (2026-06-29): landed Timeline canvas GPU/perf locally.** Added
  `src/ui/TimelineCanvas.h` as the shared native Timeline canvas and routed `src/Main.cpp` through it,
  replacing the private hand-drawn arrangement path with the same renderer used by the gate. Added
  `YesDawTimelineGpuCheck`, which scrolls a 20,640-clip arrangement fixture through an offscreen JUCE
  paint harness and fails unless `max_frame_ms < 16.6`; the verbose local run measured
  `max_frame_ms=3.2874`, 336 visible clips, and a nonblank pixel sample grid. Local gates are green:
  VS DevShell `cmake --build --preset ci --target YesDawTimelineGpuCheck`,
  `ctest --preset ci -R YesDawTimelineGpuCheck --output-on-failure`, verbose
  `YesDawTimelineGpuCheck.exe -s "[timeline][gpu][perf]"`, VS DevShell
  `cmake --build --preset ci --target YesDaw`, focused H11
  `ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
  **3/3**, and VS DevShell full `cmake --build --preset ci` + `ctest --preset ci --output-on-failure`
  **248/248**. **Next:** Timeline editing and clip affordances.

- **Earlier (2026-06-29): closed Project-load smoke + transport controls on remote CI.** Remote CI run
  `28388490955` is green across Linux, Windows, macOS, RTSan, and TSan.

- **Earlier (2026-06-29): landed Project-load smoke + transport controls locally.** Added
  `src/ui/UiAppModel.h`, a headless app model that opens an existing `.yesdaw` Project bundle, reads the
  Project snapshot, builds the H8 `PlaybackEngine` from owned decoded audio, and routes play/stop/locate/
  loop through `UiActionId`s. Added `YesDawAppSmokeCheck`, which creates a real bundle, reopens it through
  the app model, proves pre-load transport is disabled, then drives transport through the same action IDs
  used by menus/buttons/shortcuts/accessibility. Local gates are green: VS DevShell
  `cmake --build --preset ci --target YesDawAppSmokeCheck`,
  `ctest --preset ci -R YesDawAppSmokeCheck --output-on-failure`, and VS DevShell full
  `cmake --build --preset ci` + `ctest --preset ci --output-on-failure` **247/247**. **Next:**
  `YesDawTimelineGpuCheck`.

- **Earlier (2026-06-29): closed the H11 app shell + action registry checkpoint on remote CI.** Remote CI
  run `28385990090` is green across Linux, Windows, macOS, RTSan, and TSan.
- **Earlier (2026-06-29): replaced the H0 sine-spike window with a mockup-aligned JUCE shell locally.**
  `src/Main.cpp` now draws the first native DAW frame: top menu/transport/readout strip, master meter,
  track list, arrangement/timeline placeholder with clips and playhead, clip inspector, and mixer strips.
  The visible toolbar consumes `UiActionRegistry`/`UiActionContext`, and the old audio-device sine callback
  remains removed from the app target. Local gates are green: `cmake --preset ci`, VS DevShell
  `cmake --build --preset ci`, `ctest --test-dir build-ci -R YesDawUiActionCheck --output-on-failure`, and
  `ctest --preset ci --output-on-failure` **246/246**. **Next:** Project-load smoke + transport controls
  (`YesDawAppSmokeCheck`).

- **Latest (2026-06-29): replaced the H0 sine-spike app with an action-backed JUCE shell locally.** The
  standalone app now owns `UiActionRegistry`/`UiActionContext`, renders placeholder Timeline/Mixer/Piano
  Roll panels, and drives toolbar buttons through the same headless action IDs as `YesDawUiActionCheck`.
  `YesDaw` no longer links `juce_audio_utils` because the old audio-device sine callback is gone. Focused
  local gate is green: VS DevShell `cmake --build --preset ci` plus `ctest --test-dir build-ci -R
  YesDawUiActionCheck --output-on-failure`. **Next:** layer the supplied mockup's dark DAW chrome over this
  shell without changing the registry contract.

- **Latest (2026-06-29): landed the pure H11 UI action registry locally.** Added
  `src/ui/UiActions.h` and `YesDawUiActionCheck` so menus, toolbar buttons, shortcuts, accessibility, tests,
  and future agents share stable action IDs. The gate proves unique stable IDs/default keys, non-empty
  labels/accessibility names/roles, toolbar action lookup, keymap remapping/rejection, disabled-state
  reasons, and headless dispatch state changes without a display. Local gates are green: `cmake --preset
  ci`, VS DevShell `cmake --build --preset ci`, `ctest --test-dir build-ci -R YesDawUiActionCheck
  --output-on-failure`, and `ctest --preset ci --output-on-failure` **246/246**. **Next:** replace the H0
  sine-spike window with the mockup-aligned JUCE shell that consumes the registry.

- **Latest (2026-06-29): opened H11 with ADR-0032 and a focused plan.** H10 is closed and the follow-on
  adversarial-review patch batch is remote-green on `main` (`dd3b257`, GitHub Actions run `28379340005`).
  ADR-0032 accepts native JUCE Components for the app shell, rejects WebView for the main shell, keeps the
  dense arrangement view behind a dedicated Timeline canvas, and makes the UI action registry the common
  seam for menus, buttons, shortcuts, accessibility, tests, and future agents. The H11 plan is
  `docs/plans/2026-06-29-h11-single-window-timeline-ui-plan.md`. Local gate: `cmake --preset ci`, VS
  DevShell `cmake --build --preset ci`, and `ctest --preset ci --output-on-failure` **245/245**. Remote
  CI run `28382745216` is green across Linux, Windows, macOS, RTSan, and TSan. **Next:** build the app
  shell + action registry checkpoint and land `YesDawUiActionCheck`.

## Done — H10 device hot-swap survival
- **Latest (2026-06-29): landed `YesDawDeviceHotSwapCheck` and verified remote CI.** Added a control-side
  `DeviceHotSwapCoordinator` around `PlaybackEngine` plus a fake-device gate. The coordinator rejects
  swaps while the callback is active, snapshots play/stop/locate/loop state after the old callback is
  stopped, rebuilds playback for a changed device max Block size, restores transport commands before the
  new callback pumps, and destroys/reclaims the old playback graph off the audio thread. The gate proves
  bit-identical continuity against an uninterrupted/offline reference, loop-state survival, stopped-state
  survival, deterministic fake-callback error accounting between devices, old graph reclamation, and
  negative controls for sample-rate changes, output-channel changes, invalid max Block sizes, and rebuild
  attempts while active. Local gates are green: `cmake --preset ci`, VS DevShell
  `cmake --build --preset ci --target YesDawDeviceHotSwapCheck`,
  `ctest --test-dir build-ci -R "YesDawDeviceHotSwapCheck" --output-on-failure`,
  `ctest --test-dir build-ci -R "YesDaw(Loudness|Dawproject|TimeStretch|DeviceHotSwap)Check"
  --output-on-failure` **4/4**, VS DevShell `cmake --build --preset ci`, and
  `ctest --preset ci --output-on-failure` **245/245**. Remote CI run `28351880753` is green across Linux,
  Windows, macOS, RTSan, and TSan. **Next:** H11 is now open; build the app shell + action registry.
- **Latest (2026-06-29): accepted ADR-0031 for device hot-swap survival.** Decision: H10 implements a
  control-side hot-swap coordinator around `PlaybackEngine`: stop/quiesce the old fake device callback,
  snapshot transport, rebuild playback for the new device max Block size, restore locate/loop/play state,
  prime the new callback, and reclaim old graphs on the control side. H10 supports same sample rate and
  same output channel count with changed device identity/max Block size; unsupported sample-rate changes,
  channel-count changes, invalid max Block sizes, and rebuild attempts while the old callback is active
  must fail without replacing playback. Remote CI run `28351125742` is green across Linux, Windows, macOS,
  RTSan, and TSan. The follow-on `YesDawDeviceHotSwapCheck` code gate is green on remote CI run
  `28351880753`.

## Done — H10 time-stretch Node
- **Latest (2026-06-29): landed `YesDawTimeStretchCheck` locally.** Added pinned
  `signalsmith-stretch` `1.1.0` FetchContent at commit `44c8f865af9da8c29cc4a70a2d5a3ec83639c711`, a
  control-side `prepareTimeStretch` wrapper that validates mono/stereo input and folds/trims Signalsmith
  latency into exact prepared duration, and a source-style `TimeStretchNode` whose audio-thread path only
  reads immutable interleaved samples by absolute timeline frame. The gate covers pinned dependency
  version, malformed input rejection, shorter/longer fixed-ratio golden fingerprints, exact duration,
  block-split/timeline equivalence, silence windows, block-parallel-safe metadata, and fallback cursor
  reset. Local gates are green: `cmake --preset ci`, VS DevShell
  `cmake --build --preset ci --target YesDawTimeStretchCheck`,
  `ctest --test-dir build-ci -R "YesDawTimeStretchCheck" --output-on-failure`, and
  VS DevShell `cmake --build --preset ci`, `ctest --preset ci --output-on-failure` **244/244**, and
  `ctest --test-dir build-ci -R "YesDaw(Loudness|Dawproject|TimeStretch|DeviceHotSwap)Check"
  --output-on-failure` **3/3**. Remote CI run `28350136910` is green on `ad50721` across Linux, Windows,
  macOS, RTSan, and TSan. The follow-on `YesDawDeviceHotSwapCheck` gate is green on remote CI run
  `28351880753`.
- **Latest (2026-06-29): accepted ADR-0030 for the time-stretch Node.** Decision: H10 uses
  Signalsmith Stretch `1.1.0` as a pinned control-side dependency, prepares stretched clip/source audio
  before it reaches the audio thread, and exposes it through a source-style `TimeStretchNode` whose
  `process()` path is an absolute-frame read over immutable samples. Stretch factor means
  `outputFrames / sourceFrames`; H10 supports mono/stereo and finite factors in `[0.5, 2.0]`; prepared
  output duration is exact after Signalsmith pre-roll/tail folding; and the Node may be block-parallel-safe
  because its audio-thread path is order-independent. Remote CI run `28349381664` is green across Linux,
  Windows, macOS, RTSan, and TSan. **Next:** `YesDawTimeStretchCheck` local gate, then remote CI.
- **Latest (2026-06-29): closed `YesDawDawprojectCheck` remotely.**
  `YesDawDawprojectCheck` writes a stored `.dawproject` ZIP with UTF-8 `project.xml` / `metadata.xml`,
  canonical float32 WAV media under `audio/<content-hash>.wav`, deterministic XML-safe IDs, a master
  Track, synthetic audio Tracks per Clip, grouped MIDI Clips per `MidiClip::trackId`, sample-locked audio
  timing in seconds, tempo-locked MIDI timing in beats, gain/center-pan/fade/source-window data, and a
  reader/verifier that parses ZIP/XML/WAV bytes rather than comparing writer strings. Negative controls
  cover missing media, duplicate XML IDs, malformed timing, wrong media metadata, unsupported audio time
  base, unsupported channel count, changed gain, changed MIDI note data, missing decoded audio, and decoded
  metadata mismatch. Local gates are green: `cmake --preset ci`, VS DevShell
  `cmake --build --preset ci --target YesDawDawprojectCheck`,
  `ctest --test-dir build-ci -R "YesDawDawprojectCheck" --output-on-failure`,
  VS DevShell `cmake --build --preset ci`, `ctest --preset ci --output-on-failure` **243/243**, and
  `ctest --test-dir build-ci -R "YesDaw(Loudness|Dawproject|TimeStretch|DeviceHotSwap)Check"
  --output-on-failure` **2/2**. Remote CI run `28348385319` is green on `910ea1c` across Linux, Windows,
  macOS, RTSan, and TSan. **Next:** ADR-0030 accepted; implement `YesDawTimeStretchCheck`.
- **Latest (2026-06-28): added the DAWproject primitive preflight.** `YesDawDawprojectPrimitivesCheck`
  locks deterministic XML-safe IDs, parameter IDs, content-hash media paths, tick/frame conversions, XML
  escaping, and invalid-token/control-byte rejection before the package writer lands. Local gates are green:
  `cmake --preset ci`, VS DevShell `cmake --build --preset ci --target YesDawDawprojectPrimitivesCheck`,
  `ctest --test-dir build-ci -R "YesDawDawprojectPrimitivesCheck" --output-on-failure`, VS DevShell
  `cmake --build --preset ci`, and `ctest --preset ci --output-on-failure` **242/242**. **Next:** promote
  these primitives into the `.dawproject` package writer and independent reader gate
  `YesDawDawprojectCheck`.
- **Latest (2026-06-28): accepted ADR-0029 for DAWproject export.** Decision: H10 writes an export-only
  DAWproject 1.0 subset as a `.dawproject` ZIP with UTF-8 `project.xml` / `metadata.xml`, canonical
  float32 WAV media under `audio/`, deterministic XML-safe IDs derived from YES DAW `EntityId`s, synthetic
  tracks for today's audio Clips, grouped MIDI tracks by `MidiClip::trackId`, explicit unsupported statuses,
  and an independent package/XML reader gate. **Next:** land `YesDawDawprojectCheck`.
- **Latest (2026-06-28): closed `YesDawLoudnessCheck`.** Added the pinned `libebur128` dependency,
  a control/offline-only mono/stereo loudness wrapper, non-finite/malformed-input rejection, channel-map
  checks, silence/peak edge coverage, chunked-feed coverage, and a pinned version check. Local gates are
  green: `cmake --preset ci`, VS DevShell `cmake --build --preset ci --target YesDawLoudnessCheck`,
  `ctest --test-dir build-ci -R "YesDawLoudnessCheck" --output-on-failure`, VS DevShell
  `cmake --build --preset ci`, and `ctest --preset ci --output-on-failure` **241/241**. Remote CI run
  `28341446711` is green on `1d29c02` across Linux, Windows, macOS, RTSan, and TSan. **Next:** ADR-0029
  for DAWproject export.
- **Latest (2026-06-28): accepted ADR-0028 for loudness metering.** Decision: pin `libebur128` as the
  canonical BS.1770 / EBU R128 loudness implementation/reference, keep the YES DAW wrapper control/offline
  only (never called by the audio thread), support mono/stereo for H10, reject non-finite input, and gate
  wrapper/channel mapping through `YesDawLoudnessCheck`. Remote CI run `28340956377` is green.
- **Latest (2026-06-28): verified H9 remote-green and opened H10.** `git pull --ff-only` was already
  up to date on `main`; GitHub Actions run `28339991428` is green on `a5a1db4`. H10 is now the active
  horizon in `loop/horizon.md`; the focused plan is
  `docs/plans/2026-06-28-h10-mixing-mastering-interchange-plan.md`. First code slice after this docs
  checkpoint: ADR-0028 loudness metering, then `YesDawLoudnessCheck`.
- **Latest (2026-06-28): closed the scheduler-safety landmine — ADR-0027 block-parallel guard (Claude).**
  Dan approved doing it now (before H10). The parallel scheduler dispatches Blocks out of order, which is
  only correct for graphs whose every node is keyed by the absolute transport frame; a stateful node (delay
  ring, automated fader ramp, hosted plugin, PDC latency) would be silently mis-rendered, and the
  determinism fixture has zero stateful nodes so the gate couldn't catch it. Added
  `NodeProperties::blockParallelSafe` (**default false = fail-safe** — a node must opt in, so a future
  effect node is refused until proven), marked the order-independent nodes safe (the exact set in the green
  determinism graph; impulse instrument only at zero latency), had `GraphBuilder` AND it across all compiled
  nodes (incl. spliced PDC `LatencyNode`s) and force-unsafe on any path latency, exposed
  `CompiledGraph::isBlockParallelSafe()`, and made `renderProjectWithScheduler` refuse with
  `OfflineRenderStatus::GraphNotBlockParallelSafe`. New test proves the Project graph is safe and a graph
  with a `DelayNode` is refused. Full `ctest` **240/240**. **Next:** push; remote CI is the gate; then H10
  opens (mixing/mastering features + interchange) — and an H10 effect node that needs the scheduler must
  prove + mark itself safe or use the serial renderer.
- **Earlier (2026-06-28): adversarial review of Codex's H9 landing + patches (Claude).** Same multi-agent
  treatment as H6/H7/H8 (4 diverse-lens finders → per-finding skeptical verification, 22 raw → 20 confirmed,
  heavy dupes) adjudicated by hand. Verdict: the implementation is solid and honestly scoped (ADR-0024
  openly says it's block-parallel-over-snapshots, not the per-node DAG scheduler), but the gates leaned on
  two no-bite patterns. **Fixed (2 small commits):** (1) the determinism **negative control was a float
  tautology** (`1e20+1-1e20`) that never ran the scheduler — replaced with one that drives the real graph
  WITHOUT absolute transport frames in two block orders and proves they diverge (so the bit-identity gate is
  meaningful, not passing by construction); (2) the **100-track parallel soak never compared parallel↔serial
  at scale** — added a deterministic bit-identity-vs-serial check on the heavy fixture + a "every block was
  timed by a worker" check, so a contention/ordering bug at scale bites on every platform (the timing
  assertion is compiled out on Windows). **Flagged for Dan (not patched — design/honesty calls):**
  - **#1 scheduler is stateless-only, no guard (HIGH):** block-level `fetch_add` dispatch + per-worker graph
    snapshots are correct only when every node is keyed by absolute frame. A DelayNode ring / automated
    fader ramp / PDC LatencyNode / instrument pending-queue would be silently mis-rendered (and the
    determinism fixture has zero stateful nodes, so the gate can't catch it). Recommend a
    `NodeProperties::blockParallelSafe` bit aggregated into the graph + a `GraphNotBlockParallelSafe` refusal
    in `renderProjectWithScheduler` + a DelayNode test — a small ADR-0024-follow-up, landed **before** H10
    wires delay/reverb/automation/time-stretch into the scheduler. This is the headline item.
  - **Transport concurrency round 2 (latent, no live caller):** control-side getters
    (`playheadFrame()`/`isPlaying()`/…) read non-atomic fields the audio thread now writes → UB on the first
    concurrent UI read; and the SPSC command queue is single-writer (multi control-thread = UB). Fix: make
    the 5 transport fields atomic + a control-thread-id/spinlock guard + a concurrent-reader TSan test.
    Natural ADR-0023 follow-up before the H11 UI binds a playhead readout.
  - **Honesty/naming:** the blacklist test proves only the persistence *action* (no live crash triggers it —
    the H3 wiring debt is unmoved); the "parser fuzz" is a 9-row hand-written validator-regression suite,
    not byte-level fuzzing; the soak's `underruns==0` echoes a hardcoded `0u`. All honestly limited, worth a
    rename + a tracked real-fuzz/real-wiring follow-up.
  Focused gate: **YesDawSchedulerCheck** still green; full `ctest --preset ci` **240/240**. **Next:** push;
  remote CI is the gate; then Dan's call on the stateless-graph guard before H10 code lands.
- **Earlier (2026-06-28): verified H8 close-out, then completed H9 engine scaling + robustness locally.**
  H8 looked good to go: the handoff/horizon were already closed and the latest remote CI on `main` was
  green. H9 accepted ADR-0023 (transport command queue), ADR-0024 (deterministic scheduled worker
  executor), ADR-0025 (blacklist-on-failure action), and ADR-0026 (built-in instrument track auto-wire).
  The new `YesDawSchedulerCheck` proves worker-count bit identity against H7 offline render, transport
  control/audio-thread concurrency through the SPSC queue, MIDI locate/loop auto-wire parity, scheduled
  Blocks through the H6 deadline oracle, seeded bundle/plugin-state parser fuzz replay, and durable plugin
  failure blacklist rows. Local gates: `cmake --preset ci`; VS DevShell `cmake --build --preset ci`;
  focused H8/H9 lane **4/4**; full `ctest --preset ci --output-on-failure` **240/240**. First remote run
  `28337218498` exposed two `YesDawSchedulerCheck` oracle issues (final stop could apply one Block after
  final locate; Windows wall-clock soak could fail on CI scheduler jitter). The test now drains the final
  commands deterministically and keeps the measured deadline assertion on non-Windows while Windows still
  checks measured Blocks + zero Underruns. Local recheck is green: focused scheduler gate **1/1**, full
  suite **240/240**. **Next:** push; remote CI is the checkpoint gate. H10 is next after H9 is accepted.
- **Latest (2026-06-28): adversarial review of Codex's H8 close-out + patches (Claude).** Ran the same
  multi-agent treatment as H6/H7 (4 diverse-lens finders → per-finding skeptical verification, 26 raw → 24
  confirmed, heavy cross-lens dupes) and adjudicated by hand against the code. **One real correctness/safety
  hole + four toothless gates, fixed in 4 small commits:** (1) **transport hot path crashed on out-of-range
  frames** — `locate()`/`setLoop()` had no upper bound and `processBlock`'s loop split did
  `static_cast<int>(untilLoopEnd)` BEFORE the `std::min`, so a loop wider than INT_MAX (~12 h @ 48 kHz)
  truncated to a 0/negative segment and either spun forever or trapped in `CompiledGraph` — a hang/crash on
  the audio thread from one bad control call. Clamp in 64-bit before narrowing (+FATAL segment≥1), bound
  locate/setLoop to `kMaxTransportFrame`, hoist the channel/maxBlockSize FATALs to the `processBlock`
  boundary, plus a biting test. (2) **autosave gate had no negative control** — deleting the
  `needsAutosave()` guard changed nothing; added a clean-engine control proving a no-op tick writes no
  snapshot. (3) **recording gate was circular** (placed the impulse with the same playhead it then read
  back) — now the test owns the absolute device frame and asserts the playhead tracks it. (4) **loop +
  transport-vs-offline parity were under-tested** — added a loop-aware block-size sweep and `locate(N)` ==
  offline-render-slice bit-identity. Also renamed the misleading `writeAutosaveOnPlaybackTick` →
  `writeAutosaveFromControlTick` and added CONTROL-THREAD-ONLY annotations. **Did NOT edit ADR-0022** (the
  one design-level finding — non-atomic transport state is a data race once a control thread drives it
  concurrently with the audio thread — is real but latent, since no concurrent caller exists yet; it needs
  a small ADR + a TSan bite test and is the natural first H9 checkpoint; the single-thread constraint is now
  documented in code). **Deferred + tracked (out of H8's exercised surface):** `DecodedMidiClipNode` ignores
  `Transport::timelineFrame` (MIDI desyncs on locate/loop once MIDI playback is wired); `Transport.tempoMap`/
  `meterMap` left default in the playback path; `playing_` defaults to true (autoplay-on-create). Focused
  gate: **9 cases / 271 assertions** (was 6/125); full `ctest --preset ci`: **239/239**. **Next:** push; the
  review commits' remote CI is the gate; then stop for Dan's H8 close-out call + the concurrency decision.
- **Earlier (2026-06-28): finished H8 playback runtime.** ADR-0022 accepted the absolute-frame transport
  model. `PlaybackEngine` now passes a Project `timelineFrame` through `Transport` for each audio callback
  segment, so play/stop/locate/loop are sample-accurate without publishing graphs from the audio thread.
  `DecodedClipNode` reads the transport frame when present and keeps the legacy monotonic fallback for older
  direct graph callers. H5 recording now has a production capture caller from the transport playhead, and
  H6 autosave has a playback/edit tick helper in `persistence/PlaybackAutosave.h`. The tracked hardware
  smoke is `tools/playback-smoke.ps1` / `tools/playback-smoke.sh`, implemented through
  `YesDawSoak --playback-project`; it is build-checked and remains a real-device smoke, not CI. Local:
  `YesDawPlaybackCheck` **6 cases / 125 assertions**, `cmake --build --preset ci`, and
  `ctest --preset ci --output-on-failure` **239/239**. **Next:** stop for Dan's H8 close-out review; write
  the H9 focused plan/ADR before H9 code lands.
- **Follow-up (2026-06-28): fixed the pushed Linux/macOS `YesDawSoak` warning-as-error.** Remote CI caught
  `-Wreorder` in `SoakCallback`; the initializer list now matches member declaration order. Local focused
  soak build passes, and full `ctest --preset ci --output-on-failure` is still **239/239**. Remote CI run
  `28334403767` is green after rerunning the two sanitizer jobs that initially hit an `apt.llvm.org` DNS
  failure before configure. **Next:** stop for Dan's H8 close-out review; H9 still needs its focused
  plan/ADR before code lands.
- **Earlier (2026-06-28): adversarial review of Codex's just-landed H7 offline-render gate + patches (Claude).**
  Ran the same multi-agent treatment as H6 (5 diverse-lens finders -> per-finding skeptical verification,
  25 raw -> 24 confirmed, heavy dupes) and adjudicated by hand. **Two real blockers + WAV-robustness gaps,
  fixed in 4 small commits:** (1) **fade-curve divergence** — the offline renderer pre-baked an equal-power
  fade (`ClipEnvelope`'s 1.4186 polynomial) while the realtime `DecodedClipNode` applies a LINEAR fade, so
  the exported WAV used a different curve than playback would (export != playback, violating the roadmap's
  "RT matches offline" premise, undocumented in ADR-0021). Fixed by rendering fades through the same
  `DecodedClipNode` the realtime engine uses (export == playback by construction; equal-power stays
  deferred per H2), and the test reference is now the canonical linear ramp, not a verbatim copy of the
  engine polynomial. (2) **block-size independence unproven** — the 9-frame fixture rendered in a single
  128-frame block, so the multi-block path + ADR-0008 block-size independence (the *defining* property of
  an offline renderer) were dead; added a sweep requiring bit-identical output at sizes forcing 9..1
  blocks, plus a renderer-input mutation control (the prior negative controls only perturbed the
  reference). (3) **WAV codec** — reader no longer pre-allocates an attacker-controlled ~4 GiB buffer
  before bounds-checking; writer rejects channel counts that overflow the 16-bit block align; round-trip
  widened to the full float range, denormals, a known byte layout, and an ancillary-chunk skip. **Honest
  scope:** the PDC/tail-flush + marker-extension paths are inert until a latency node lands (H8+); the
  export/import round-trip stores+decodes the WAV but isn't wired into a Project's playback graph. Did
  **not** edit ADR-0021 (hard-stop; it covers format only and the fade fix is consistent with H2). Focused
  gate: **6 cases / 143 assertions**; full `ctest --preset ci`: **238/238**. **Next:** push; the review
  commits' remote CI is the gate; then stop for Dan's H7->H8 boundary call.
- **Earlier (2026-06-28): H7 offline render/export implemented and locally green.**
  Accepted ADR-0021 for the canonical H7 export format: RIFF/WAVE 32-bit IEEE float at the Project sample
  rate, using Master bus channels. Added `src/io/WavFile.h` (pure/headless float32 WAV writer+reader),
  `src/engine/OfflineRenderer.h` (Project + decoded Assets -> interleaved Master-bus samples through
  `ProjectMixerProjection` + `CompiledGraph::process`), and `tests/offline_render_tests.cpp` as the
  blocking `YesDawOfflineRenderCheck` target. The gate proves: offline render vs an independent reference
  over timeline positions/fades/gain, bit-exact WAV write/read, export -> bundle `importAssetBytes` ->
  decode round-trip, plus negative controls for wrong clip position/dropped tail, mutated writer payload,
  malformed/truncated WAV, non-finite samples, tempo-locked audio deferral, and corrupted export decode.
  Honest scope: H7 covers the current sample-locked audio Project mixer surface; sample-rate conversion,
  integer/lossy export, UI/export dialog, stems/regions, device I/O/transport, and time-stretch remain
  later horizons. Local verification: `cmake --preset ci`; VS DevShell `cmake --build --preset ci`;
  `ctest --test-dir build-ci -R "YesDawOfflineRenderCheck" --output-on-failure` **1/1**; `ctest --preset
  ci --output-on-failure` **238/238**. **Next:** push this checkpoint; remote CI is the gate; then Claude
  reviews the H7 close-out adversarially before H8 opens.
- **Earlier (2026-06-28): defined H7–H11 (ADR-0020 Accepted), wrote the H7 plan, switched the horizon to
  H7 — Codex builds next.**
  Dan recalled "tasks up to H10"; confirmed none ever existed (roadmap was always H0–H6; the work he
  remembers is the eight features bundled into the build plan's "H6 ongoing" bucket). ADR-0020 carves the
  rest into numbered horizons, **feature-first with the UI as the H11 capstone** (Dan's call — build the
  whole headless feature set, then wire it into one UI shell): **H7** offline render/export to file;
  **H8** playback runtime — device I/O + transport + first production callers for recording/autosave
  (absorbs the open H0 hardware soak; the audible milestone); **H9** engine scaling & robustness —
  multicore work-stealing + soak/fuzz + H3/H4 debt; **H10** mixing/mastering features & interchange —
  loudness metering, DAWproject export, time-stretch, device hot-swap; **H11** single-window timeline UI
  shell + accessibility (capstone; visual feel is the lone human spot-check; needs the pending UI-stack
  ADR). **H7 kickoff landed:** ADR-0020 Accepted, roadmap H7–H11 written, `loop/horizon.md` now targets
  H7 with `YesDawOfflineRenderCheck`, and the H7 plan
  (`docs/plans/2026-06-28-h7-offline-render-export-plan.md`) lays out the build order: ADR-0021 (canonical
  float32-WAV format) -> WAV codec (writer+reader) with a round-trip gate -> a real `OfflineRenderer`
  module (replaces the test-only render helpers) gated vs an *independent* reference -> export-to-file +
  re-import round-trip. **Next:** Dan points Codex at the H7 plan to build it; Codex must write ADR-0021
  before the codec lands, keep each checkpoint small + green, and stop for any other ADR-level call;
  Claude reviews the close-out adversarially before H7 is called done.
- **Earlier (2026-06-28): Codex follow-on adversarial review of the H6 close-out found no new proven H6
  defect.** Pulled `main` first (`git pull --ff-only`, already up to date), read the live handoff,
  horizon, roadmap, ADR-0019, H6 plan, latest H6 commits (`a6f52c5` through `363f765`), and the H6
  implementation/tests. Rechecked the likely weak points directly: the deadline oracle has a biting
  negative-control test; the heavy session now routes each track through real Fader/Meter DSP; autosave
  recovery prefers `last.yesdaw` and falls back to `last.previous` through normal bundle validators; and
  the docs honestly scope hard-kill, headless underruns, and the lack of a production autosave caller.
  Local verification: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --test-dir build-ci -R
  "H6" --output-on-failure` **6/6**; `ctest --preset ci --output-on-failure` **237/237**. Latest
  pre-review remote CI on `363f765` was green across Windows, Linux, macOS, RTSan, and TSan
  (`28314008140`). No H7/H8 commit, branch, ADR, or exit criterion exists, so do **not** create a
  successor Codex thread or start H7 automatically; stop for Dan's H6->H7 boundary decision.
- **Latest (2026-06-28): adversarial review of Codex's just-landed H6 reliability gate + patches (Claude).**
  Dan asked to review "H8 that Codex just landed" and then start H7 — but `origin/main`'s freshest work
  is the **H6** reliability gate (`a6f52c5` + `d82a5b7`); there is no H8 commit anywhere, and H7 was never
  defined. So this reviewed the just-landed H6 gate (the one horizon never adversarially reviewed). Ran a
  multi-agent adversarial review (6 diverse-lens finders -> per-finding skeptical verification, 24 raw ->
  22 confirmed, heavy cross-lens dupes) and adjudicated by hand. **Verdict:** the autosave half had real
  oracles, but the deadline half could not bite and the autosave publish had a real durability hole.
  **Proven issues fixed (4 small commits):** (1) the deadline oracle had **no negative control** —
  `passesDeadline()`/`summarizeDeadlineSoak` were only ever asserted true, so a broken percentile index or
  flipped strict-< would stay green; added a deterministic negative control (over-budget needs >0.1% of
  blocks, underrun, empty, and the at-deadline boundary all fail). (2) the autosave publish deleted
  `last.previous` before publishing and recovery only read `last.yesdaw`, so a hard kill between the two
  renames lost **both** copies, and nothing was fsync'd — made the publish crash-safe (keep `last.previous`
  until the new snapshot is fsync'd; recovery falls back to it) and fsync the DB/assets/dir, with 3 biting
  negative controls. (3) the "heavy 100-track session" was 100 trivial DC nodes (~1000x margin) — gave each
  track real mixer-strip DSP (source -> Fader -> Meter). (4) docs honesty: roadmap/horizon/plan now state
  the "hard kill" is an in-process transaction rollback (OS-level crash / hot-WAL recovery stays ADR-0005's
  soak), `underruns == 0` is a headless design choice, and the autosave surface has **no production caller
  yet**. **Rejected:** a live-timing floor assertion (`p999 * N > period`) — machine-dependent, would make
  CI flaky; the negative control is the right biting oracle. Did **not** edit ADR-0019 (hard-stop rule; the
  ADR was already honest on hard-kill scope + underruns). Focused H6 gate: **6/6**; full `ctest --preset
  ci`: **237/237** (was 233). **Next:** push; the review commits' remote CI is the gate; then stop for
  Dan's H6->H7 boundary call — H7's scope must be decided before any H7 code lands.
- **Earlier (2026-06-28): H5 rechecked clean; H6 reliability gate implemented and closed.**
  H5 is good to move past: latest remote CI on `main` is green (`28310557870`), the current focused H5
  gate passed locally 3/3, and the H5 docs no longer overclaim the unwired recording capability. No H5
  patch was needed. For H6, accepted ADR-0019 and added the focused reliability gate:
  `src/engine/Reliability.h`, `src/persistence/AutosaveRecovery.h`, `tests/reliability_tests.cpp`, and
  target `YesDawReliabilityCheck`. Focused local gate: `ctest --test-dir build-ci -R "H6"
  --output-on-failure` passed 2/2. Full local gate: VS DevShell `cmake --build --preset ci`; `ctest
  --preset ci` passed 233/233. Remote CI passed on the H6 implementation close-out commit.
  **Next:** stop for Dan's H6->H7 boundary decision; do not start H7 automatically.
- **Earlier (2026-06-28): adversarial review of Codex's H5 close-out + patches (Claude).**
  Ran a multi-agent adversarial review (5 diverse-lens finders → per-finding skeptical verification) over
  the whole H5 surface, then adjudicated by hand (the panel over-fired: ~50 raw findings, heavy dupes).
  **Verdict:** H5's gate is genuinely better than the prior horizons' — it's a real integration test of a
  real module, green 3/3, truly wired into the RTSan/TSan target list, and `choc`'s FIFO push is genuinely
  alloc/lock-free. But it had passed too easily. **Proven issues fixed:** (1) the "negative control" was a
  tautological pure-math assertion on a parallel config — replaced with a real broken-pipeline run that
  proves the recorded peak lands at the wrong (uncompensated) frame; (2) the stereo (2-ch), FIFO
  backpressure, `maxLoopTakes`, direct-input, take-file-format-error, multi-segment comp, and MIDI-edge
  paths were all unexercised — added a biting case for each (the stereo interleave turned out correct — it
  was a coverage gap, not a live bug); (3) the audio-path mapping helpers
  (`compensatedLatencyFrames`/`normalizeRecordingFrame`/`mapDeviceInputFrameToRecordingFrame`/
  `recordMidiEventsToTimeline`/FIFO `push`) lacked `YESDAW_RT_HOT`, so RTSan wasn't enforcing nonblocking
  on them — annotated (safe: the sanitizer leg has no `-Werror`); (4) `framesAccepted` double-counted
  dropped frames — fixed so accepted+dropped partition the mapped frames exactly; (5) the gate's writer
  thread could `std::terminate` if a mid-loop `REQUIRE` fired — made it join-then-assert. **Docs:** the
  flat "CLOSED" overclaimed — horizon/plan/roadmap/STATUS now state the exit criterion is met but the
  capability is unwired (no production caller; monitoring/UI/persistence/asset-format deferred). Local:
  focused H5 gate **9/9**; full `ctest --preset ci` **231/231** (was 225). **Next:** push; the review
  commit's remote CI is the gate; then stop for Dan's H5→H6 boundary call — do not start H6 automatically.
- **Earlier (2026-06-28): H4 patches checked; H5 recording gate implemented.**
  H4 CP2a (`DecodedMidiClipNode` runtime event source) and F8 (`CompiledTempoMap` prefix-sum lookup) match
  the existing ADR-0009/0010/0017 contracts and have focused mechanical coverage. I did not auto-wire MIDI
  Clips through `ProjectMixerProjection` because that requires the still-open instrument-track modeling
  decision, not a bug fix. For H5, accepted ADR-0018 and added the pure engine recording spine in
  `src/engine/Recording.h`: bounded audio-thread FIFO, writer-thread take file, latency mapping, punch/loop
  take ordinals, comp selection, and MIDI timestamp compensation. Focused local gate:
  `ctest --test-dir build-ci -R "recorded take aligns|punch loop recording|MIDI recording uses"
  --output-on-failure` passed 3/3. Full local gate: `cmake --preset ci`; VS DevShell
  `cmake --build --preset ci`; `ctest --preset ci --output-on-failure` passed 225/225. Remote CI run
  `28309319816` is green on Windows, Linux, macOS, RTSan, and TSan. **Next:** stop for Dan's H5->H6
  boundary decision; do not start H6 automatically.
- **Earlier (2026-06-28): adversarial review of H1 + H2; started building the real render/timeline path.**
  Dan asked to tie up loose ends before H5; the same build+mutation+multi-agent review on H1/H2 (66 agents)
  found they are ALSO shallower than "closed": 13 blockers / 23 majors. The concurrency spine (lock-free
  graph swap, janitor reclamation, atomics — RTSan/TSan) and the SQLite round-trip ARE solid. But the
  project-render/timeline layer was largely unbuilt behind vacuous gates: the "RT matches offline Render"
  gate compared `CompiledGraph::process` to ITSELF (a 2x output mutation stayed green); `DecodedClipNode`
  ignored `Clip.timelineStart` (every clip played from frame 0); the crossfade was pre-baked in test code on
  non-overlapping clips; and the "property test" was a hand-coded 21-step array. Dan chose: build it for
  real. **Landed + CI-green (CP1, CP2) / local-green (CP3):** (CP1) audio clips render at their timeline
  positions and sum overlaps, gated against an independent reference; (CP2) the engine applies clip fades
  and renders a real overlapping crossfade; (CP3) a real seeded randomized property test over all clip+note
  verbs proves undo -> bit-identical -> redo (1236 assertions; found no undo bug). Docs (roadmap H1/H2 +
  this file) now honestly state what is built vs deferred. **STILL OPEN — the end-to-end "load a project
  file and hear it play" glue:** an asset **decoder -> source-node projection** (no audio decoder exists;
  `ProjectMixerProjection`'s source factory is test-supplied), a separate offline-Render/Export-to-file
  module, the single-window timeline UI shell (`src/Main.cpp` is still the H0 sine spike), the async
  waveform cache (currently synchronous), and equal-power crossfade. **Plus still open from before:** H3
  worker-mode driving + blacklist-on-failure wiring; H4 CP2b (auto-wire MIDI tracks). **H0 soak** (hardware,
  Dan). Each of these is a focused build, not a patch. **Next:** wire the projection to position clips from
  the tempo map, then the remaining items.
- **Earlier (2026-06-27): three H3 gate-honesty fixes landed and CI-green; two real items remain.**
  Continuing the H3 remediation (Dan chose "gate honesty + oracle first"). **Landed + CI-green on `main`:**
  (1) `docs(h3)` — roadmap.md status note + STATUS now state H3 is real-but-shallow; (2) `fix(h3)` — the
  tautological "zero xrun" oracle is now a REAL counter: `RtLaneRing` counts a missed deadline on the
  fail-open branch (was a dead probe-count increment that could never fire), and the gate asserts the
  count tracks the ladder exactly (6 forced misses); (3) `fix(h3)` — `readOutputFailOpen` now scrubs a
  non-finite child Block (checks finiteness before committing to the bus; a NaN/Inf Block fails open
  instead of poisoning out[]/last-good), with a negative control in `plugin_node_tests.cpp`. Full suite
  219/219 local each time. **REMAINING — both real surgery, each its own focused checkpoint:**
  - **Drive the worker's misbehavior modes across the real boundary.** `PluginHostMain.cpp:320` hard-codes
    `SyntheticProcessorMode::passthrough`; emit-NaN / fixed-latency / hang never cross IPC in the gate.
    Needs: a `mode` field on `RtLaneLoadMessage` (`PluginHostProtocol.h`), the worker honoring it
    (`handleRtLaneLoadMessage`), the coordinator load API passing it, and a new gate case that loads the
    worker in `emitNan`/`fixedReportedLatency` and asserts the host fail-open reader stays finite /
    sample-aligned THROUGH the real process. (Also: the worker drops the Event stream —
    `processHostedBlock` does `(void) events;` — so cross-process tri-stream is impossible until that is
    wired; H4 deferral.)
  - **Wire blacklist-on-failure (the real missing exit clause).** `FailureActionKind` has only
    `none`/`bypassAndRecompile` (no blacklist action); `blacklistStatePersisted`/`blacklistPolicyApplied`
    are hardcoded false; ~3,500 lines of `Blacklist*` coordinator state-machine have no downstream effect;
    the "blacklist persists across restart" gate test bypasses the coordinator and round-trips
    `ProjectBundleDb` directly. The persistence works; the gap is the coordinator never writes a row on a
    crash/hang. ADR-0015 says the coordinator escalates into the blacklist, so wire that: a bypass-and-
    blacklist action that persists the candidate identity through the coordinator + the gate driving a real
    failure through it. One sub-decision: does the coordinator own the bundle write, or emit a candidate
    its owner persists (ADR-0015 implies the former).
  - **Also still open:** H4 full-close CP2b (auto-wire MIDI tracks via `ProjectMixerProjection`, needs an
    instrument-track design call); H1 and H2 have NOT been adversarially reviewed this session — given two
    of two reviewed horizons (H3, H4) were shallower than their "closed" labels, treat H1/H2 "done" with
    the same skepticism until they get the same build+mutation+multi-agent pass; H0 real-hardware soak.
- **Earlier (2026-06-27): adversarial review of H3 (mixer + plugin hosting); remediation started.**
  Ran the same build + mutation + multi-agent treatment on H3 that H4 got (66 agents; the gate built and
  ran, 3/4 injected mutations bit). Verdict: the host-isolation gate is REAL (it spawns the real
  `YesDawPluginHost` worker, OS shared-memory RT lane, real PID kills, opaque-state CRC round-trip across
  the real control lane) but SHALLOWER than it presents. Confirmed (and independently re-read) defects:
  (1) **blacklist-on-failure is not wired** — `FailureActionKind` has only `none`/`bypassAndRecompile`
  (no blacklist action); `blacklistStatePersisted`/`blacklistPolicyApplied` are hardcoded false; ~3,500
  lines of blacklist state-machine have no downstream effect; the "blacklist persists across restart" gate
  test bypasses the coordinator and just round-trips SQLite — the PRIOR review's "honest skeleton" finding
  is still true. (2) **the "zero xrun / no deadline miss" oracle is a structural tautology** —
  `RtLaneRing::loadOutputReadyOnce` is called exactly once per Block so `deadlineMissCount()` can never
  increment; `deadlineMissCount()==0` proves nothing. (3) **the synthetic worker only runs passthrough**
  (`PluginHostMain.cpp:320`) — emit-NaN / fixed-latency / hang modes never cross the real boundary in the
  gate, and fail-open does not scrub NaN. (4) the roadmap "real high-latency plugin / two parallel paths"
  is met only nominally (in-process stub); roadmap.md's stale "pluginval/auval pass in CI" clause now
  carries a status note. So H3 is NOT complete against ADR-0015's own exit gate; the mixer half is solid,
  the plugin-hosting half is real-but-shallow. **Dan chose:** fix gate honesty + the oracle first
  (mechanical, no new ADR); blacklist-on-failure wiring is a separate, likely ADR-gated slice. **In
  progress:** repurpose `deadlineMissCount` to count real fail-open misses + make the gate assert it
  tracks the ladder; drive the worker's NaN/latency/hang modes across the boundary; scrub NaN in fail-open.
- **Earlier (2026-06-27): full-close F8 — ADR-0010 prefix-sum tempo lookup is green locally.**
  Closes review finding F8: `tickToFrame` was an O(n) per-call scan + full re-validation, diverging from
  ADR-0010's mandated prefix-sum O(log n) lookup, and `flattenMidiClipToTimeline` called it once per Note
  start/end (O(notes * segments)). Added `CompiledTempoMap`: validate + accumulate each segment's cumulative
  start frame ONCE on the control side, then binary-search any tick in O(log n). `flattenMidiClipToTimeline`
  now builds it once and resolves each Note in O(log n). `frameForTick` is bit-identical to `tickToFrame` by
  construction; the new gate in `time_tests.cpp` proves prefix == naive exactly across 4001 ticks, every
  segment boundary, a logarithmic ramp segment, and the empty-map default. Local: full
  `ctest --test-dir build-ci` **218/218** green. **Next:** the only remaining full-close item is CP2b
  (auto-build MIDI tracks from a Project via `ProjectMixerProjection`), which needs a short design call on
  instrument-track modeling (what instrument, how it is chosen/persisted) before code lands — Dan to decide.
  Checkpoint complete after remote CI is green.
- **Earlier (2026-06-27): full-close CP2a — runtime MidiClip source Node is green locally.**
  Closing review finding F3 (no runtime clip->engine path) the laid-out way — mirroring the audio
  `DecodedClipNode` + `ProjectMixerProjection` pattern, NOT a new design. Added `flattenMidiClipToTimeline`
  (control-side whole-clip flatten to a sorted absolute-frame Event timeline) and `DecodedMidiClipNode`,
  the MIDI analogue of `DecodedClipNode`: it streams that timeline into the graph one Block at a time by
  advancing an ADR-0009 per-source cursor, with zero audio-thread allocation (emits via
  `EventStream::replaceEvents` into the pre-sized Event slot). New `CompiledNodeKind::MidiSource` +
  GraphBuilder recognition. Integration test proves a Project `MidiClip`'s NoteOns reach an instrument at
  the right frames across two Blocks with the caller feeding NO events. Local gate:
  `YesDawMidiTimingCheck` 17 cases / 311 assertions; full `ctest --test-dir build-ci` **217/217** green.
  No new ADR needed — this applies ADR-0009 (per-source cursors) + ADR-0017 (render bridge) + the existing
  source-node/projection precedent. **Next:** CP2b — extend `ProjectMixerProjection` to walk `midiClips`
  (source -> instrument -> Fader/Pan/Meter); then F8 (ADR-0010 prefix-sum `tickToFrame`). Caveat: an
  audible instrument still means a hosted plugin (`PluginNode`); the built-in `ImpulseInstrumentNode` is a
  timing fixture. Checkpoint complete after remote CI is green.
- **Earlier (2026-06-27): adversarial H4 review + checkpoint 1 (gate rigor) is green locally.**
  An independent multi-agent adversarial review (a real build + mutation tests + 9 static dimensions,
  every finding re-verified by a skeptic) asked whether the H4 gate is real, the code correct, and the
  claims honest. Verdict: the MIDI math is correct and the gate builds/passes, but the gate was weaker
  than the docs claimed. Concretely — the "negative controls" the horizon/plan/STATUS advertised for the
  three failure modes did NOT exist as tests (a mutation of the half-open boundary check `>=`->`>` passed
  the whole suite, masked by a redundant second guard), and no single test combined block-boundary +
  tempo change + PDC. **Checkpoint 1 fixed both:** removed the redundant guard so the boundary check is
  load-bearing, and added four real negative controls (boundary-belongs-to-next-Block,
  constant-tempo-differs-from-mapped, PDC-moves-the-impulse) plus one integrated boundary+tempo+PDC test.
  Local gate: `YesDawMidiTimingCheck` now **16 cases / 289 assertions** green (was 12 / 247), built via VS
  DevShell `ninja -C build-ci YesDawMidiTimingCheck`. This checkpoint is complete only after the docs
  commit's remote CI is green.
  **Deferred to the next checkpoints (full-close, each ADR-gated):** (F3) there is no runtime
  MidiClip -> engine source Node yet — `flattenMidiClipNotesForBlock` is called only from tests, so a
  loaded Project with MIDI Clips produces no notes at playback; needs an ADR for the clip-event source
  contract and an RT-safe (non-allocating) flatten before code lands. (F8) `tickToFrame` does an O(n)
  per-call scan + full re-validation, diverging from ADR-0010's mandated prefix-sum O(log n) lookup; the
  prefix-sum cache lands as ADR-0010 conformance with a bit-identity test. Minor follow-ups tracked:
  `pdcShiftFrames` event-shift path untested; LinearRamp + `floor()` rounding untested; >1024 events trips
  an audio-thread RT_FATAL; `quantizeNote` snaps start only; `MidiClip.timeBase` ignored at flatten; MPE
  zero-length / cross-port edges. **Next:** REVIEW/FIX this checkpoint, then write the F3 source-node ADR.
- **Earlier (2026-06-27): H4 review/close pass is green locally; H5 is ready for Dan's boundary call.**
  Audited H4 against the H4 plan, roadmap, ADR-0017, ADR-0009, ADR-0010, `loop/horizon.md`, this handoff,
  `YesDawMidiTimingCheck`, and the full `ci` evidence. Every H4 build-order item is covered: MIDI
  Clip/Note flattening through the tempo map, non-zero-latency Instrument Node timing through PDC,
  Project-owned MIDI Clip/Note persistence, piano-roll Note edit commands with undo/redo coverage,
  deterministic MIDI-effect Nodes, hosted-instrument Event delivery through `PluginNode`, and MPE
  boundary voice allocation. Focused local gate before close-out docs: `YesDawMidiTimingCheck` passed
  **12 cases / 247 assertions**. Full close-out local gate on these docs: `cmake --preset ci`; VS
  DevShell `cmake --build --preset ci`; `ctest --preset ci --output-on-failure` passed **217/217**; and
  `ctest --preset ci -R YesDawMidiTimingCheck --output-on-failure` passed. The close-out checkpoint is
  complete only after the final docs commit's remote CI is green. Remote CI for the final implementation
  commit `ba0f4f5 fix(h4): reserve explicit mpe voices` was green on Windows, Linux, macOS, RTSan, and
  TSan. **Next after green CI:** stop for Dan's H4->H5 boundary call.
- **Latest (2026-06-27): REVIEW/FIX H4 MPE boundary allocation is green locally.**
  REVIEW/FIX found one proven defect in the MPE boundary allocator: an earlier wildcard Note could claim
  a member channel that a later overlapping explicit voice-hinted Note needed, producing a same-channel
  MPE collision while still reporting success. Fixed by precomputing explicit member-channel reservation
  intervals before wildcard assignment; wildcard Notes now skip any overlapping future explicit
  reservation as well as currently-active allocated voices. Focused local gate:
  `YesDawMidiTimingCheck` passed **12 cases / 247 assertions**. Full local gate: `cmake --preset ci`;
  VS DevShell `cmake --build --preset ci`; `ctest --preset ci --output-on-failure` passed **217/217**;
  and `ctest --preset ci -R YesDawMidiTimingCheck --output-on-failure` passed. **Next:** run the H4
  review/close pass if CI stays green.
- **Latest (2026-06-27): REVIEW/FIX H4 hosted-instrument Event bridge + WORKER MPE boundary allocation
  is green locally.**
  REVIEW/FIX of the hosted Event-bridge checkpoint found no additional proven defect; the previous CI run
  was green. Then WORKER added the MPE boundary allocation slice: wildcard MIDI Notes can be copied into
  render-ready Notes with concrete MPE `portIndex`/member `channel` assignments, explicit voice hints are
  preserved and reserve their channel before same-tick wildcard allocation, non-overlapping Notes reuse
  channels deterministically, and exhausted overlapping member channels fail with a mechanical
  `OutOfVoices` status instead of stealing voices. The focused H4 gate proves allocated voice addresses
  flatten into ADR-0009 `VoiceAddress` fields and survive MIDI-effect + hosted `PluginNode` RT-lane
  delivery. Focused local gate: `YesDawMidiTimingCheck` passed **11 cases / 241 assertions**. Full local
  gate: `cmake --preset ci`; VS DevShell `cmake --build --preset ci`; `ctest --preset ci
  --output-on-failure` passed **217/217**; and `ctest --preset ci -R YesDawMidiTimingCheck
  --output-on-failure` passed. **Next:** REVIEW/FIX this MPE boundary allocation slice, then run the H4
  review/close pass if green.
- **Latest (2026-06-27): REVIEW/FIX H4 MIDI-effect Nodes + WORKER hosted-instrument Event bridge is
  green locally.**
  REVIEW/FIX of the MIDI-effect Nodes checkpoint found one proven defect: graph-owned MIDI effects
  mutated the single caller EventStream globally, so a sibling raw Instrument branch could consume a
  transposed key. Fixed by adding bounded branch-local Event slots inside `CompiledGraph`: event-producing
  Nodes copy their selected input Events into a fixed graph-owned slot, downstream consumers read that
  slot, and the root caller Events remain unchanged. Then WORKER added the hosted-instrument Event bridge
  proof: a `PluginNode` hosted instrument receives the transformed Note Events through the RT lane and
  returns a deterministic impulse on the next pipeline Block. Focused local gate:
  `YesDawMidiTimingCheck` passed **9 cases / 211 assertions**. Full local gate: `cmake --preset ci`;
  VS DevShell `cmake --build --preset ci`; `ctest --preset ci --output-on-failure` passed **217/217**;
  and `ctest --preset ci -R YesDawMidiTimingCheck --output-on-failure` passed. **Next:** REVIEW/FIX this
  hosted Event-bridge slice, then build the MPE boundary allocation slice if green.
- **Latest (2026-06-27): WORKER H4 MIDI-effect Nodes slice is green locally.**
  REVIEW/FIX of the piano-roll Note edit-command checkpoint found no proven defect; the prior CI run
  was green. Then WORKER added writable EventStream storage for graph-owned Events, deterministic
  `MidiTransposeNode` and `MidiScaleMapNode` event-transform Nodes, GraphBuilder classification for
  MIDI-effect Nodes, and a compiled-graph test proving scale-map -> transpose runs before the
  Instrument Node consumes the NoteOn. Local gate: `cmake --preset ci`; VS DevShell
  `cmake --build --preset ci`; focused `YesDawMidiTimingCheck` passed **7 cases / 131 assertions**;
  `ctest --preset ci --output-on-failure` passed **217/217**; and
  `ctest --preset ci -R YesDawMidiTimingCheck --output-on-failure` passed. **Next:** REVIEW/FIX this
  MIDI-effect Nodes slice, then build the hosted-instrument Event bridge if green.
- **Latest (2026-06-27): WORKER H4 piano-roll Note edit-command slice is green locally.**
  REVIEW/FIX of the Project-owned MIDI Clip/Note persistence checkpoint found no proven defect; the
  prior CI run was green. Then WORKER added Project-level Note edit operations for move, length,
  split/cut, quantize, and transpose, extended the existing Project undo command/diff stack with
  MIDI Clip row diffs, and proved invalid edits leave the Project unchanged plus undo/redo returns
  bit-identical Project values. Local gate: `cmake --preset ci`; VS DevShell `cmake --build --preset ci`;
  focused `YesDawProjectCheck`; `ctest --preset ci --output-on-failure` passed **217/217**; and
  `ctest --preset ci -R YesDawMidiTimingCheck --output-on-failure` passed. **Next:** REVIEW/FIX this
  piano-roll edit-command slice, then build the MIDI-effect Nodes slice if green.
- **Latest (2026-06-27): WORKER H4 Project-owned MIDI Clip/Note surface + persistence is green locally.**
  REVIEW/FIX of the previous `YesDawMidiTimingCheck` checkpoint found no proven defect; the named gate
  remains green. Then WORKER moved `Note` / `MidiClip` into the Project value surface, added track
  ownership, Note window and voice-address validation, schema v3 tables (`midi_clips`, `midi_notes`),
  snapshot write/read, migration coverage, and open-time semantic validation for corrupted Note windows.
  Local gate: `cmake --preset ci`; VS DevShell `cmake --build --preset ci`; focused Project /
  Persistence / MIDI timing executables; `ctest --preset ci --output-on-failure` passed **213/213**.
  **Next:** REVIEW/FIX this MIDI Project/persistence slice, then build the piano-roll edit-command slice
  if green.
- **Latest (2026-06-27): WORKER H4 MIDI timing bridge + `YesDawMidiTimingCheck` is green locally.**
  REVIEW/FIX of the docs-only H4 kickoff found one real handoff defect: old H3 historical entries near the
  top still said "do not start H4"; they now explicitly say Dan has opened H4 and the H0 soak remains
  separate. Then WORKER added the first H4 code slice: `tickToFrame()` for ADR-0010 tempo maps, `MidiClip`
  / `Note` flattening into sorted ADR-0009 `NoteOn` / `NoteOff` Events, a deterministic
  `ImpulseInstrumentNode`, GraphBuilder recognition for that built-in source, and the named
  `YesDawMidiTimingCheck` ctest gate. The gate proves half-open Block boundaries, full tempo-map conversion
  across a tempo change, and a non-zero-latency Instrument Node aligned by PDC. Local gate:
  `cmake --preset ci`; VS DevShell `cmake --build --preset ci`; `ctest --preset ci -R YesDawMidiTimingCheck`;
  `ctest --preset ci --output-on-failure` passed **210/210**. **Next:** REVIEW/FIX this H4 MIDI timing
  bridge/gate, then build the Project-owned MIDI Clip/Note surface + persistence slice if green.
- **Latest (2026-06-27): H4 boundary opened and the docs-only kickoff checkpoint is green locally.**
  This checkpoint accepts ADR-0017 (MIDI Clip edit model + render bridge), adds the H4 plan, switches
  `loop/horizon.md` to `YesDawMidiTimingCheck`, updates `CONTEXT.md`, and leaves code untouched. The H4
  finish line is now mechanical: note-ons at known offsets must land sample-accurately across Block
  boundaries and a tempo change, through a non-zero-latency Instrument Node that PDC compensates.
  Local gate: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` passed **209/209**.
  **Next:** REVIEW/FIX this docs-only H4 kickoff, then build the first code slice: the MIDI timing bridge
  and `YesDawMidiTimingCheck` negative controls, green before commit.
- **Latest (2026-06-27): independent adversarial review of the whole H0–H3 surface, then fixed every real
  finding it raised. 5 small green commits straight to `main`; full local suite 209/209; remote CI green.**
  This pass IS the independent review the close-out plan's rule 3 demanded (the earlier out-of-band review at
  `54943fd` predated the real IPC/watchdog/blacklist/state work). What landed:
  - **`ebe7200` — real child-side crash (finding K).** The crash leg simulated a crash via a *parent* kill and
    `crashOnCue` was dead code. Now the worker terminates *itself* on a control-lane cue; the coordinator only
    learns of it via `handleConnectionLost`. Instant reporter-free termination so CI stays deterministic
    (`std::abort` stalls ~8 s on Windows via WER). Crash gate green ×5 local; CI green on all 3 OS + RTSan + TSan.
  - **`c2c94d7` — sidechain wired into the mixer projection (finding F).** `MixerGraphProjection` now inserts a
    `SidechainGainNode` VCA keyed by a track's `sidechainSource`, ahead of the fader; two negative-controlled
    projection tests (value VCA + PDC alignment **through the projection**). The old DONE closed F over a path
    that didn't exist (it cited the raw-graph gate only).
  - **`2830c36` — H1 tempo/meter/markers round-trip (a *dropped* H1 exit clause).** H1's exit says the Project
    round-trips "tempo/meter map, markers, clips intact" but only clips were covered and it wasn't even
    named-deferred. `Project` now carries `tempoMap/meterMap/markers` (new `Marker` type); the bundle persists +
    restores them in tick order; new round-trip test with negative controls.
  - **`c1aaab3` — per-channel meter readout.** `MeterNode` now publishes per-channel `peak(ch)/rms(ch)` (RT-safe)
    plus the aggregate — "stereo metering" is now literal, not just stereo-aware accumulation.
  - **docs honesty pass (this commit).** Reworded the overstated claims to match the now-true code: gate (a)
    proves PDC *scheduling* in-process (cross-process boundary proven by (b)/(c)); `RuntimeAudioDriver` is a
    real seam but its only caller is a test (live device shell → H4); the full tri-stream-through-the-worker
    (plugin parameter automation) → H4 with a reason; ledger F/K updated; stale `[!shouldfail]` CMake comment
    deleted; independent-review provenance recorded.
  - **NOT done — needs Dan / hardware:** the **H0 audio soak** exit clause (zero underruns, 10 min, 128-frame
    Block, Win+mac). Tooling exists and is ADR-0005-compliant (`tools/soak.sh` + `tools/soak/SoakMain.cpp`,
    Goertzel-asserted, xrun/deadline-miss counters, exit 0/1) but no PASS at 128 frames has been recorded
    (shared-mode Realtek forced 480; needs ASIO/WASAPI-exclusive + a loopback jumper; no macOS run). This is
    the only thing between "H0–H3 fully behind us" and done. **Next:** Dan runs the soak on real hardware and
    ticks it, OR blesses moving to H4 with the soak tracked as the lone open H0 item. Dan has since opened
    H4; the soak remains tracked separately as the lone human/hardware H0 item.
- **Latest: H3 host-isolation exit gate is now blocking and green locally.**
  REVIEW/FIX of `1bf006e` found no proven defect in the opaque-state checkpoint: opaque bytes cross the
  real worker process control lane both ways; `{chunk_len, crc32}` is validated; a deliberately corrupted
  CRC push is rejected before state restore; the valid push proves `setStateInformation` acceptance; and no
  audio-thread/RT-lane/scanner/pluginval/auval/UI/real external plugin/golden drift leaked in. Close-out
  flipped `opaqueStateRoundTripsAcrossProcess` to the real proof, removed `[!shouldfail]` from the aggregate
  `YesDawHostIsolationCheck`, clarified ADR-0015's engine/app layering wording for finding J, updated
  `loop/horizon.md`, and ticked the close-out plan acceptance checklist. Historical next at that point:
  Dan's H3->H4 horizon-boundary review; Dan has since opened H4.
- **OUT-OF-BAND REVIEW (2026-06-26, Claude as reviewer/builder).** Full adversarial review of the whole
  H3 surface @ `54943fd` (14-dim workflow, 106 agents; write-up `yesdaw-h3-complete-review.md` in the
  session scratchpad; 46 findings adjudicated against ground truth). **0 live / user-reachable defects** —
  nothing is wired to a runtime yet. **Correction to the horizon line:** the plugin-hosting half is NOT
  complete — it is an honest *skeleton* (worker loads no plugins; no shared-memory mmap; the coordinator
  threads metadata but every `blacklistStatePersisted`/`blacklistPolicyApplied`/`graphRecompileExecuted`
  flag is hardcoded false), so **ADR-0015's host-isolation exit gate is unmet**. The mixer-policy half is
  solid (my two earlier fixes verified; mono-blind render harness fixed).
  - **LANDED this checkpoint:** `FaderNode` automation/event gain seam now clamps via `clampGain`, mirroring
    `setTargetGain`. It was the one unguarded gain path — events are not validated on the live audio path
    (`EventStream::isValidForBlock` is control/test-side only), so a non-finite/out-of-range `normalizedValue`
    reached the ramp and injected inf/NaN. Added a **negative-controlled** regression test in `fader_tests.cpp`
    (proven to FAIL without the clamp). Local gate: `YesDawFaderCheck` = 5 cases / 7439 assertions green;
    full RTSan/TSan/3-OS matrix on CI at push.
  - **LANDED (ADR-0016) — the mute-mask 64-node ceiling is fixed.** The mask was a single `uint64_t` keyed by
    compiled-node index, so a project past ~16 tracks silently lost **all** mute/solo (`applyMixerMutePolicy`
    is all-or-nothing). ADR-0016 (grilled + accepted) replaces it with a compile-time-sized
    `std::vector<std::atomic<uint64_t>>` word array; `muteBit` (now `uint32`) `= compiledIdx` indexes it, so
    mute/solo is **unbounded**, the audio read stays branch-only, recompiles stay bit-identical, and
    `CompiledNode` stays trivially-copyable. 4 green commits (ADR → widen `muteBit` → multi-word storage →
    drop the clamp + a **negative-controlled 200-track scaling test** that fails on the pre-fix build at ~the
    17th target). `MixerMutePolicy` and the `Node` contract untouched. Local: Graph/Builder/MutePolicy/
    Projection/Render/Runtime checks green; full RTSan/TSan/3-OS matrix on CI green (`e5eb741`).
  - **LANDED (review finding C) — PluginNode PDC latency vs block size.** A `PluginNode` reports its one-Block
    IPC latency for a construction-time pipeline Block, but the compiler reads `properties()`/locks PDC
    *before* `prepare()` learns the real `maxBlockSize` — so a mismatch was a silent fixed phase error. Now
    `GraphBuilder::build` rejects it loudly with a dedicated `GraphBuildError::PluginBlockSizeMismatch`
    (typed `dynamic_cast` check, exception-free, consistent with the existing Sidechain casts; engine stays
    JUCE-free). Negative-controlled test in `plugin_node_tests.cpp` (matched builds; mismatched is rejected
    with the node id) — proven to fail without the check.
- **Latest: WORKER H3 minimal coordinator deferred blacklist-handling outcome handling acknowledge/clear-status
  shell is locally green — the coordinator can clear a recorded future control-thread
  blacklist-handling outcome handling result without applying blacklist policy or persistence.**
  REVIEW/FIX of the previous minimal coordinator deferred blacklist-handling outcome handling receipt/status
  shell found no proven defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety /
  layering rules: the receipt/status shell is coordinator-side, headless, and non-vacuous; records only a
  structurally valid handling result derived from a valid pending blacklist-handling outcome and valid
  deferred blacklist-handling command receipt; leaves initial/empty, invalid, already-cleared, and
  already-drained paths empty/no-record; preserves watchdog-timeout vs crash distinction before clear; keeps
  no-policy/no-persistence flags false; does not enforce blacklist policy, persist/cache blacklist state,
  scan/load plugins, execute graph rewiring, or claim graph recompile execution; keeps existing deferred
  graph-change acknowledge/clear behavior intact; `YesDawPluginHost` remains the only JUCE plugin-hosting
  owner; the coordinator/check target does not link `juce_audio_processors`; Apple framework links stay
  scoped to `YesDawPluginHost`; and `YESDAW_BUILD_APPS=OFF` pure sanitizer configs are unaffected. Then
  WORKER added the smallest deferred blacklist-handling outcome handling acknowledge/clear surface:
  `acknowledgeDeferredBlacklistHandlingOutcomeHandlingStatus()`. The coordinator self-check now proves
  initial/empty acknowledge paths stay empty/no-record; valid watchdog-timeout and crash handling receipts
  can be inspected distinctly and then acknowledged/cleared back to empty/no-record; clearing the handling
  receipt does not clear the source deferred blacklist-handling command receipt/status; no blacklist policy
  is applied; no blacklist state is persisted; and no scanner, plugin loading, graph rewiring, graph
  recompile execution, ADR edits, goldens, subjective checks, or `[[clang::nonblocking]]` /
  `YESDAW_RT_HOT` annotation edits were introduced. Local gate: `cmake --preset ci`; VS DevShell
  `cmake --build --preset ci`; VS DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator deferred blacklist-handling outcome handling
  acknowledge/clear-status shell — verify `src/plugin_host/PluginHostCoordinator.h`,
  `src/plugin_host/PluginHostCoordinatorCheck.cpp`, `src/plugin_host/PluginHostMain.cpp`,
  `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake against ADR-0015 (watchdog/crash
  attribution, future blacklist escalation, future blacklist policy, future control-thread blacklist
  handling, and host-worker ownership), ADR-0013 (runtime crash/hang attribution escalates into the same
  blacklist later), ADR-0008 (engine targets must not link hosting / `Node` contract unchanged), and the
  rolling-baton rule. Confirm the acknowledge/clear shell is coordinator-side, headless, and non-vacuous;
  clears only the deferred blacklist-handling outcome handling receipt/status; leaves initial/already-empty
  paths empty/no-record; preserves watchdog-timeout vs crash distinction before clear; keeps
  no-policy/no-persistence flags false; does not enforce blacklist policy, persist/cache blacklist state,
  scan/load plugins, execute graph rewiring, or claim graph recompile execution; keeps JUCE hosting confined
  to `YesDawPluginHost`; and leaves `YESDAW_BUILD_APPS=OFF` pure sanitizer configs unaffected. Fix only
  proven defects. If clean and green, continue in the SAME baton to the next small worker chunk: a minimal
  coordinator blacklist-handling completion/status shell for future control-thread blacklist handling,
  still without applying/enforcing blacklist policy, persistence/cache, scanner, plugin loading, real graph
  rewiring, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP, ADR edits, goldens,
  subjective checks, or RT-hot annotation edits. Stop for any new ADR-level decision. Create exactly one
  successor baton only after that checkpoint's `STATUS.md` update, commit, push, and remote CI are green.
- **Latest: WORKER H3 minimal coordinator deferred blacklist-handling outcome handling receipt/status
  shell is locally green — the coordinator can record and inspect a future control-thread
  blacklist-handling outcome handling result without applying blacklist policy or persistence.**
  REVIEW/FIX of the previous minimal coordinator blacklist-handling outcome drain-to-control-thread
  handling shell found no proven defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the
  RT-safety / layering rules: the handling shell is coordinator-side, headless, and non-vacuous; drains
  only an outcome derived from a valid deferred blacklist-handling command receipt; leaves initial/empty,
  invalid, already-cleared, and already-drained paths empty/no-record; preserves watchdog-timeout vs
  crash distinction before clear; clears only the pending outcome without clearing the source deferred
  command receipt; keeps no-policy/no-persistence flags false; does not enforce blacklist policy,
  persist/cache blacklist state, scan/load plugins, execute graph rewiring, or claim graph recompile
  execution; keeps existing deferred graph-change acknowledge/clear behavior intact; `YesDawPluginHost`
  remains the only JUCE plugin-hosting owner; the coordinator/check target does not link
  `juce_audio_processors`; Apple framework links stay scoped to `YesDawPluginHost`; and
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs are unaffected. Then WORKER added the smallest
  deferred blacklist-handling outcome handling receipt/status surface:
  `recordDeferredBlacklistHandlingOutcomeHandlingResult()` plus
  `deferredBlacklistHandlingOutcomeHandlingStatus()`. The coordinator self-check now proves
  initial/empty and invalid paths stay empty/no-record; valid watchdog-timeout and crash handling results
  produce distinct recorded statuses; recording preserves the source deferred blacklist-handling command
  receipt/status; no blacklist policy is applied; no blacklist state is persisted; and no scanner,
  plugin loading, graph rewiring, graph recompile execution, ADR edits, goldens, subjective checks, or
  `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation edits were introduced. Local gate:
  `cmake --preset ci`; VS DevShell `cmake --build --preset ci`; VS DevShell `ctest --preset ci` passed
  **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator deferred blacklist-handling outcome handling receipt/status
  shell — verify `src/plugin_host/PluginHostCoordinator.h`,
  `src/plugin_host/PluginHostCoordinatorCheck.cpp`, `src/plugin_host/PluginHostMain.cpp`,
  `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake against ADR-0015 (watchdog/crash
  attribution, future blacklist escalation, future blacklist policy, future control-thread blacklist
  handling, and host-worker ownership), ADR-0013 (runtime crash/hang attribution escalates into the same
  blacklist later), ADR-0008 (engine targets must not link hosting / `Node` contract unchanged), and the
  rolling-baton rule. Confirm the receipt/status shell is coordinator-side, headless, and non-vacuous;
  records only a handling result derived from a valid pending blacklist-handling outcome and valid
  deferred blacklist-handling command receipt; leaves initial/empty, invalid, already-cleared, and
  already-drained paths empty/no-record; preserves watchdog-timeout vs crash distinction before clear;
  keeps no-policy/no-persistence flags false; does not enforce blacklist policy, persist/cache blacklist
  state, scan/load plugins, execute graph rewiring, or claim graph recompile execution; keeps JUCE
  hosting confined to `YesDawPluginHost`; and leaves `YESDAW_BUILD_APPS=OFF` pure sanitizer configs
  unaffected. Fix only proven defects. If clean and green, continue in the SAME baton to the next small
  worker chunk: a minimal coordinator deferred blacklist-handling outcome handling acknowledge/clear-status
  shell for future control-thread blacklist handling, still without applying/enforcing blacklist policy,
  persistence/cache, scanner, plugin loading, real graph rewiring, crash-test plugin, plugin UI, real
  shared memory, pluginval/auval, CLAP, ADR edits, goldens, subjective checks, or RT-hot annotation
  edits. Stop for any new ADR-level decision. Create exactly one successor baton only after that
  checkpoint's `STATUS.md` update, commit, push, and remote CI are green.
- **Latest: WORKER H3 minimal coordinator blacklist-handling outcome/status shell is locally green — the
  coordinator can expose an inspectable future control-thread blacklist-handling outcome from a valid
  deferred blacklist-handling command receipt without applying blacklist policy or persistence.**
  REVIEW/FIX of the previous minimal coordinator deferred blacklist-handling command acknowledge/clear-status
  shell found no proven defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety /
  layering rules: the acknowledge/clear shell is coordinator-side, headless, and non-vacuous; clears only
  the deferred blacklist-handling command receipt/status; leaves initial/already-empty paths empty/no-record;
  preserves watchdog-timeout vs crash distinction before clear; keeps no-policy/no-persistence flags false;
  does not enforce blacklist policy, persist/cache blacklist state, scan/load plugins, execute graph rewiring,
  or claim graph recompile execution; keeps existing deferred graph-change acknowledge/clear behavior intact;
  `YesDawPluginHost` remains the only JUCE plugin-hosting owner; the coordinator/check target does not link
  `juce_audio_processors`; Apple framework links stay scoped to `YesDawPluginHost`; and
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs are unaffected. Then WORKER added the smallest
  blacklist-handling outcome/status surface: `blacklistHandlingOutcomeStatus()` derives an outcome only from
  a valid deferred blacklist-handling command receipt. The coordinator self-check now proves initial/empty
  paths stay empty; valid watchdog-timeout and crash command receipts produce distinct outcome-ready statuses
  before acknowledgement/clear; acknowledgement returns the derived outcome to empty/no-record; invalid
  no-action, unconsumed, policy-applied, persistence-claimed, missing-control, mismatched, and already-drained
  receipts stay empty/no-record; no blacklist policy is applied; no blacklist state is persisted; and no scanner,
  plugin loading, graph rewiring, graph recompile execution, ADR edits, goldens, subjective checks, or
  `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation edits were introduced. Local gate: `cmake --preset ci`;
  VS DevShell `cmake --build --preset ci`; VS DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator blacklist-handling outcome/status shell — verify
  `src/plugin_host/PluginHostCoordinator.h`, `src/plugin_host/PluginHostCoordinatorCheck.cpp`,
  `src/plugin_host/PluginHostMain.cpp`, `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake
  against ADR-0015 (watchdog/crash attribution, future blacklist escalation, future blacklist policy, future
  control-thread blacklist handling, and host-worker ownership), ADR-0013 (runtime crash/hang attribution
  escalates into the same blacklist later), ADR-0008 (engine targets must not link hosting / `Node` contract
  unchanged), and the rolling-baton rule. Confirm the outcome/status shell is coordinator-side, headless, and
  non-vacuous; derives only from a valid deferred blacklist-handling command receipt; leaves initial/empty,
  invalid, already-cleared, and already-drained paths empty/no-record; preserves watchdog-timeout vs crash
  distinction before clear; keeps no-policy/no-persistence flags false; does not enforce blacklist policy,
  persist/cache blacklist state, scan/load plugins, execute graph rewiring, or claim graph recompile execution;
  keeps JUCE hosting confined to `YesDawPluginHost`; and leaves `YESDAW_BUILD_APPS=OFF` pure sanitizer configs
  unaffected.
  Fix only proven defects. If clean and green, continue in the SAME baton to the next small worker chunk:
  a minimal coordinator pending blacklist-handling outcome queue/drain shell for future control-thread
  blacklist handling, still without applying/enforcing blacklist policy, persistence/cache, scanner, plugin
  loading, real graph rewiring, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP,
  ADR edits, goldens, subjective checks, or RT-hot annotation edits. Stop for any new ADR-level decision.
  Create exactly one successor baton only after that checkpoint's `STATUS.md` update, commit, push, and
  remote CI are green.
- **Latest: WORKER H3 minimal coordinator deferred blacklist-handling command acknowledge/clear-status
  shell is locally green — the coordinator can clear a recorded future control-thread
  blacklist-handling command receipt without applying blacklist policy or persistence.**
  REVIEW/FIX of the previous minimal coordinator deferred blacklist-handling command receipt/status shell
  found no proven defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety /
  layering rules: the shell is coordinator-side, headless, and non-vacuous; records only a valid drained
  blacklist-handling command result; preserves watchdog-timeout vs crash distinction through command,
  receipt, inspection, and empty paths; keeps no-policy/no-persistence flags false; does not enforce
  blacklist policy, persist/cache blacklist state, scan/load plugins, execute graph rewiring, or claim
  graph recompile execution; existing deferred graph-change acknowledge/clear behavior remains intact;
  `YesDawPluginHost` remains the only JUCE plugin-hosting owner; the coordinator/check target does not
  link `juce_audio_processors`; Apple framework links stay scoped to `YesDawPluginHost`; and
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs are unaffected. Then WORKER added the smallest deferred
  blacklist-handling command acknowledge/clear-status shell:
  `acknowledgeDeferredBlacklistHandlingCommandStatus()`. The coordinator self-check now proves
  initial/empty acknowledge paths stay empty/no-record; valid watchdog-timeout and crash command receipts
  can be inspected distinctly and then acknowledged/cleared back to empty/no-record; no blacklist policy
  is applied; no blacklist state is persisted; and no scanner, plugin loading, graph rewiring, graph
  recompile execution, ADR edits, goldens, subjective checks, or `[[clang::nonblocking]]` /
  `YESDAW_RT_HOT` annotation edits were introduced. Local gate: `cmake --preset ci`; VS DevShell
  `cmake --build --preset ci`; VS DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator deferred blacklist-handling command acknowledge/clear-status
  shell — verify `src/plugin_host/PluginHostCoordinator.h`,
  `src/plugin_host/PluginHostCoordinatorCheck.cpp`, `src/plugin_host/PluginHostMain.cpp`,
  `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake against ADR-0015
  (watchdog/crash attribution, future blacklist escalation, future blacklist policy, future
  control-thread blacklist handling, and host-worker ownership), ADR-0013 (runtime crash/hang attribution
  escalates into the same blacklist later), ADR-0008 (engine targets must not link hosting / `Node`
  contract unchanged), and the rolling-baton rule. Confirm the acknowledge/clear shell is
  coordinator-side, headless, and non-vacuous; clears only the deferred blacklist-handling command
  receipt/status; leaves initial/already-empty paths empty/no-record; preserves watchdog-timeout vs crash
  distinction before clear; keeps no-policy/no-persistence flags false; does not enforce blacklist
  policy, persist/cache blacklist state, scan/load plugins, execute graph rewiring, or claim graph
  recompile execution; keeps JUCE hosting confined to `YesDawPluginHost`; and leaves
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs unaffected.
  Fix only proven defects. If clean and green, continue in the SAME baton to the next small worker chunk:
  a minimal coordinator blacklist-handling outcome/status shell for future control-thread blacklist
  handling, still without applying/enforcing blacklist policy, persistence/cache, scanner, plugin
  loading, real graph rewiring, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP,
  ADR edits, goldens, subjective checks, or RT-hot annotation edits. Stop for any new ADR-level decision.
  Create exactly one successor baton only after that checkpoint's `STATUS.md` update, commit, push, and
  remote CI are green.
- **Latest: WORKER H3 minimal coordinator blacklist-handling request/status shell is locally green — the
  coordinator can expose a future blacklist-handling request from the most recent deferred outcome-handling
  receipt without applying blacklist policy or persistence.**
  REVIEW/FIX of the previous deferred blacklist policy-decision outcome handling acknowledge/clear-status
  shell found no proven defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety /
  layering rules: the shell is coordinator-side, headless, and non-vacuous; clears only the deferred
  outcome-handling receipt/status; leaves initial/already-empty paths empty/no-record; keeps
  no-policy/no-persistence flags false; does not enforce blacklist policy, persist/cache blacklist state,
  scan/load plugins, execute graph rewiring, or claim graph recompile execution; existing deferred
  graph-change acknowledge/clear behavior remains intact; `YesDawPluginHost` remains the only JUCE
  plugin-hosting owner; the coordinator/check target does not link `juce_audio_processors`; Apple
  framework links stay scoped to `YesDawPluginHost`; and `YESDAW_BUILD_APPS=OFF` pure sanitizer configs
  are unaffected. Then WORKER added the smallest blacklist-handling request/status surface:
  `blacklistHandlingRequest()` derives a request only from a valid deferred outcome-handling receipt. The
  coordinator self-check now proves initial/empty paths stay empty; valid watchdog-timeout and crash
  receipts produce distinct request-ready statuses; acknowledgement/clear returns the derived request to
  empty/no-record; no-action, unconsumed, policy-applied, persistence-claimed, missing-control, and
  mismatched handling receipts stay empty/no-record; no blacklist policy is applied; no blacklist state is
  persisted; and no scanner, plugin loading, graph rewiring, graph recompile execution, ADR edits,
  goldens, subjective checks, or `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation edits were
  introduced. Local gate: `cmake --preset ci`; VS DevShell `cmake --build --preset ci`; VS DevShell
  `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator blacklist-handling request/status shell — verify
  `src/plugin_host/PluginHostCoordinator.h`, `src/plugin_host/PluginHostCoordinatorCheck.cpp`,
  `src/plugin_host/PluginHostMain.cpp`, `src/plugin_host/PluginHostProtocol.h`, and directly relevant
  CMake against ADR-0015 (watchdog/crash attribution, future blacklist escalation, future blacklist
  policy, future control-thread blacklist handling, and host-worker ownership), ADR-0013 (runtime
  crash/hang attribution escalates into the same blacklist later), ADR-0008 (engine targets must not link
  hosting / `Node` contract unchanged), and the rolling-baton rule. Confirm the request/status shell is
  coordinator-side, headless, and non-vacuous; derives only from a valid deferred outcome-handling
  receipt; preserves watchdog-timeout vs crash distinction; leaves initial/empty, no-action, unconsumed,
  policy-applied, persistence-claimed, missing-control, mismatched, and acknowledged/cleared paths
  empty/no-record; keeps no-policy/no-persistence flags false; does not enforce blacklist policy,
  persist/cache blacklist state, scan/load plugins, execute graph rewiring, or claim graph recompile
  execution; keeps JUCE hosting confined to `YesDawPluginHost`; and leaves `YESDAW_BUILD_APPS=OFF` pure
  sanitizer configs unaffected. Fix only proven defects. If clean and green, continue in the SAME baton to
  the next small worker chunk: a minimal coordinator pending blacklist-handling request queue/drain shell
  for future blacklist handling, still without applying/enforcing blacklist policy, persistence/cache,
  scanner, plugin loading, real graph rewiring, crash-test plugin, plugin UI, real shared memory,
  pluginval/auval, CLAP, ADR edits, goldens, subjective checks, or RT-hot annotation edits. Stop for any
  new ADR-level decision. Create exactly one successor baton only after that checkpoint's `STATUS.md`
  update, commit, push, and remote CI are green.
- **Latest: WORKER H3 minimal coordinator deferred blacklist policy-decision outcome handling
  acknowledge/clear-status shell is locally green — the coordinator can clear the most recent future
  control-thread blacklist-handling result without applying blacklist policy or persistence.**
  REVIEW/FIX of the previous deferred blacklist policy-decision outcome handling receipt/status shell found
  no proven defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety / layering rules:
  the shell is coordinator-side, headless, and non-vacuous; records only valid handling-ready,
  pending-consumed watchdog/crash handling results; preserves watchdog-timeout vs crash distinction; leaves
  initial/empty, no-action, unconsumed, policy-applied, persistence-claimed, missing-control, mismatched,
  and already-empty paths empty/no-record; keeps no-policy/no-persistence flags false; does not enforce
  blacklist policy, persist/cache blacklist state, scan/load plugins, execute graph rewiring, or claim
  graph recompile execution; existing deferred graph-change acknowledge/clear behavior remains intact;
  `YesDawPluginHost` remains the only JUCE plugin-hosting owner; the coordinator/check target does not
  link `juce_audio_processors`; Apple framework links stay scoped to `YesDawPluginHost`; and
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs are unaffected. Then WORKER added the smallest deferred
  outcome-handling acknowledge/clear surface:
  `acknowledgeDeferredBlacklistPolicyDecisionOutcomeHandlingStatus()` clears the recorded handling receipt
  and returns empty/no-action status. The coordinator self-check now proves already-empty acknowledgement
  stays empty; a valid watchdog-timeout handling receipt clears back to empty/no-record; no blacklist policy
  is applied; no blacklist state is persisted; and no scanner, plugin loading, graph rewiring, graph
  recompile execution, ADR edits, goldens, subjective checks, or `[[clang::nonblocking]]` /
  `YESDAW_RT_HOT` annotation edits were introduced. Local gate: `cmake --preset ci`; VS DevShell
  `cmake --build --preset ci`; VS DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator deferred blacklist policy-decision outcome handling
  acknowledge/clear-status shell — verify `src/plugin_host/PluginHostCoordinator.h`,
  `src/plugin_host/PluginHostCoordinatorCheck.cpp`, `src/plugin_host/PluginHostMain.cpp`,
  `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake against ADR-0015
  (watchdog/crash attribution, future blacklist escalation, future blacklist policy, future control-thread
  blacklist handling, and host-worker ownership), ADR-0013 (runtime crash/hang attribution escalates into
  the same blacklist later), ADR-0008 (engine targets must not link hosting / `Node` contract unchanged),
  and the rolling-baton rule. Confirm the acknowledge/clear shell is coordinator-side, headless, and
  non-vacuous; clears only the deferred outcome-handling receipt/status; leaves initial/already-empty
  paths empty/no-record; keeps no-policy/no-persistence flags false; does not enforce blacklist policy,
  persist/cache blacklist state, scan/load plugins, execute graph rewiring, or claim graph recompile
  execution; keeps JUCE hosting confined to `YesDawPluginHost`; and leaves `YESDAW_BUILD_APPS=OFF` pure
  sanitizer configs unaffected. Fix only proven defects. If clean and green, continue in the SAME baton to
  the next small worker chunk: a minimal coordinator blacklist-handling request/status shell for future
  blacklist handling, still without applying/enforcing blacklist policy, persistence/cache, scanner, plugin
  loading, real graph rewiring, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP,
  ADR edits, goldens, subjective checks, or RT-hot annotation edits. Stop for any new ADR-level decision.
  Create exactly one successor baton only after that checkpoint's `STATUS.md` update, commit, push, and
  remote CI are green.
- **Latest: WORKER H3 minimal coordinator deferred blacklist policy-decision outcome handling
  receipt/status shell is locally green — the coordinator can record and inspect the most recent future
  control-thread blacklist-handling result without applying blacklist policy or persistence.**
  REVIEW/FIX of the previous pending blacklist policy-decision outcome drain-to-control-thread handling
  shell found no proven defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety /
  layering rules: the shell is coordinator-side, headless, and non-vacuous; derives only from a valid
  drained pending policy-decision outcome; preserves watchdog-timeout vs crash distinction; exposes
  empty/no-record after drain, acknowledgement/clear, and already-drained-pending paths; keeps
  initial/empty, normal-stop, no-action, invalid, policy-applied, persistence-claimed, already-drained,
  and already-cleared paths empty/no-record; keeps no-policy/no-persistence flags false; does not apply or
  enforce blacklist policy, persist/cache blacklist state, scan/load plugins, rewire the graph, or claim
  graph recompile execution; existing deferred graph-change acknowledge/clear behavior remains intact;
  `YesDawPluginHost` remains the only JUCE plugin-hosting owner; the coordinator/check target does not
  link `juce_audio_processors`; Apple framework links stay scoped to `YesDawPluginHost`; and
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs are unaffected. Then WORKER added the smallest deferred
  outcome-handling receipt/status surface:
  `recordDeferredBlacklistPolicyDecisionOutcomeHandlingResult()` records only handling-ready,
  pending-consumed watchdog/crash handling results that do not claim blacklist policy or persistence, and
  `deferredBlacklistPolicyDecisionOutcomeHandlingStatus()` exposes the recorded status for inspection.
  The coordinator self-check now proves initial/empty, no-action, unconsumed, policy-applied,
  persistence-claimed, missing-control, and mismatched handling results stay empty/no-record; watchdog
  timeout and crash handling receipts remain distinct; no blacklist policy is applied; no blacklist state
  is persisted; and no scanner, plugin loading, graph rewiring, graph recompile execution, ADR edits,
  goldens, subjective checks, or `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation edits were
  introduced. Local gate: `cmake --preset ci`; documented VS DevShell `cmake --build --preset ci`;
  documented VS DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator deferred blacklist policy-decision outcome handling
  receipt/status shell — verify `src/plugin_host/PluginHostCoordinator.h`,
  `src/plugin_host/PluginHostCoordinatorCheck.cpp`, `src/plugin_host/PluginHostMain.cpp`,
  `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake against ADR-0015
  (watchdog/crash attribution, future blacklist escalation, future blacklist policy, future control-thread
  blacklist handling, and host-worker ownership), ADR-0013 (runtime crash/hang attribution escalates into
  the same blacklist later), ADR-0008 (engine targets must not link hosting / `Node` contract unchanged),
  and the rolling-baton rule. Confirm the receipt/status shell is coordinator-side, headless, and
  non-vacuous; records only valid handling-ready, pending-consumed watchdog/crash handling results;
  preserves watchdog-timeout vs crash distinction; leaves initial/empty, no-action, unconsumed,
  policy-applied, persistence-claimed, missing-control, mismatched, and already-empty paths
  empty/no-record; keeps no-policy/no-persistence flags false; does not enforce blacklist policy,
  persist/cache blacklist state, scan/load plugins, execute graph rewiring, or claim graph recompile
  execution; keeps JUCE hosting confined to `YesDawPluginHost`; and leaves `YESDAW_BUILD_APPS=OFF` pure
  sanitizer configs unaffected. Fix only proven defects. If clean and green, continue in the SAME baton to
  the next small worker chunk: a minimal coordinator deferred blacklist policy-decision outcome handling
  acknowledge/clear-status shell for future blacklist handling, still without applying/enforcing blacklist
  policy, persistence/cache, scanner, plugin loading, real graph rewiring, crash-test plugin, plugin UI,
  real shared memory, pluginval/auval, CLAP, ADR edits, goldens, subjective checks, or RT-hot annotation
  edits. Stop for any new ADR-level decision. Create exactly one successor baton only after that
  checkpoint's `STATUS.md` update, commit, push, and remote CI are green.
- **Latest: WORKER H3 minimal coordinator pending blacklist policy-decision outcome
  drain-to-control-thread handling shell is locally green — the coordinator can consume one pending future
  blacklist policy-decision outcome for future control-thread blacklist handling, without applying policy
  or persistence.**
  REVIEW/FIX of the previous pending blacklist policy-decision outcome queue/drain shell found no proven
  defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety/layering rules: the shell is
  coordinator-side, headless, and testable; derives only from a valid inspected deferred
  `requestPolicyDecision` command/status; preserves watchdog-timeout vs crash distinction where a pending
  outcome exists; exposes empty/no-record after drain and after acknowledgement/clear; keeps
  initial/empty, normal-stop, no-action, invalid, policy-applied, persistence-claimed, already-drained,
  and already-cleared paths empty/no-record; does not enforce blacklist policy, persist/cache blacklist
  state, scan/load plugins, execute graph rewiring, or claim graph recompile execution; existing deferred
  graph-change acknowledge/clear behavior remains intact; `YesDawPluginHost` remains the only JUCE
  plugin-hosting owner; the coordinator/check target does not link `juce_audio_processors`; Apple
  framework links stay scoped to `YesDawPluginHost`; and `YESDAW_BUILD_APPS=OFF` pure sanitizer configs
  are unaffected. Then WORKER added the smallest pending outcome handling surface:
  `drainPendingBlacklistPolicyDecisionOutcomeToControlHandling()` drains one queued outcome into a future
  control-thread blacklist-handling request/status shell, preserves watchdog-timeout vs crash cause, and
  leaves blacklist policy and persistence flags false. The coordinator self-check now proves initial/empty,
  normal-stop, invalid, policy-applied, persistence-claimed, already-drained, already-cleared, and
  already-drained-pending paths stay empty/no-record; watchdog-timeout and crash handling outcomes remain
  distinct before acknowledgement; acknowledgement leaves handling empty when asked again; no blacklist
  policy is applied; no blacklist state is persisted; and no scanner, plugin loading, graph rewiring,
  graph recompile execution, ADR edits, goldens, subjective checks, or `[[clang::nonblocking]]` /
  `YESDAW_RT_HOT` annotation edits were introduced. Local gate: `cmake --preset ci`; documented VS
  DevShell `cmake --build --preset ci`; documented VS DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator pending blacklist policy-decision outcome
  drain-to-control-thread handling shell — verify `src/plugin_host/PluginHostCoordinator.h`,
  `src/plugin_host/PluginHostCoordinatorCheck.cpp`, `src/plugin_host/PluginHostMain.cpp`,
  `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake against ADR-0015 (watchdog/crash
  attribution, future blacklist escalation, future blacklist policy, and host-worker ownership), ADR-0013
  (runtime crash/hang attribution escalates into the same blacklist later), ADR-0008 (engine targets must
  not link hosting / `Node` contract unchanged), and the rolling-baton rule. Confirm the handling shell is
  coordinator-side, headless, and non-vacuous; derives only from a valid drained pending policy-decision
  outcome; preserves watchdog-timeout vs crash distinction; exposes empty/no-record after drain,
  acknowledgement/clear, and already-drained-pending paths; keeps initial/empty, normal-stop, no-action,
  invalid, policy-applied, persistence-claimed, already-drained, and already-cleared paths empty/no-record;
  does not enforce blacklist policy, persist/cache blacklist state, scan/load plugins, execute graph
  rewiring, or claim graph recompile execution; keeps JUCE hosting confined to `YesDawPluginHost`; and
  leaves `YESDAW_BUILD_APPS=OFF` pure sanitizer configs unaffected. Fix only proven defects. If clean and
  green, continue in the SAME baton to the next small worker chunk: a minimal coordinator deferred
  blacklist policy-decision outcome handling receipt/status shell for future blacklist handling, still
  without applying/enforcing blacklist policy, persistence/cache, scanner, plugin loading, real graph
  rewiring, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP, ADR edits, goldens,
  subjective checks, or RT-hot annotation edits. Stop for any new ADR-level decision. Create exactly one
  successor baton only after that checkpoint's `STATUS.md` update, commit, push, and remote CI are green.
- **Latest: WORKER H3 minimal coordinator pending blacklist-candidate queue/drain shell is locally green
  — the coordinator can queue and drain one future blacklist candidate after inspection without enforcing
  blacklist policy or persistence.**
  REVIEW/FIX of the previous blacklist-candidate status shell found no proven defects against
  `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety/layering rules: the status shell is
  coordinator-side, headless, and testable; initial/empty status and normal stop stay not candidates;
  watchdog-timeout and crash host failures become future blacklist candidates while preserving their
  distinct causes; existing deferred graph-change acknowledge/clear behavior still rejects execution
  claims; `YesDawPluginHost` remains the only JUCE plugin-hosting owner; the coordinator/check target
  does not link `juce_audio_processors`; Apple framework links stay scoped to `YesDawPluginHost`; and
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs are unaffected. Then WORKER added the smallest pending
  blacklist-candidate queue/drain shell: `queueBlacklistCandidateForCurrentFailure()` queues only the
  current real crash/watchdog candidate status, `pendingBlacklistCandidateStatus()` exposes it for
  inspection, and `drainPendingBlacklistCandidateStatus()` clears it after inspection. The coordinator
  self-check now proves initial and normal-stop paths remain empty, invalid/manual inconsistent
  candidates are rejected, watchdog and crash candidates queue/drain distinctly, and drain clears the
  pending slot. Scope held: no real plugin load, scanner, watchdog blacklist policy/enforcement,
  blacklist/cache persistence, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP,
  ADR edits, goldens, broad graph rewiring, graph recompile execution, subjective checks, or
  `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation edits.
  Local gate: `cmake --preset ci`; documented VS DevShell `cmake --build --preset ci`; documented VS
  DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator pending blacklist-candidate queue/drain shell
  — verify `src/plugin_host/PluginHostCoordinator.h`, `src/plugin_host/PluginHostCoordinatorCheck.cpp`,
  `src/plugin_host/PluginHostMain.cpp`, `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake
  against ADR-0015 (watchdog/crash attribution, future blacklist escalation, and host-worker ownership),
  ADR-0013 (runtime crash/hang attribution escalates into the same blacklist later), ADR-0008 (engine
  targets must not link hosting / `Node` contract unchanged), and the rolling-baton rule. Confirm the
  queue/drain shell is coordinator-side, headless, and non-vacuous; queues nothing for initial/empty status
  or normal stop; rejects invalid/inconsistent candidates; queues and drains watchdog-timeout and crash
  candidates while preserving their distinction; does not enforce blacklist policy, persist/cache blacklist
  state, scan/load plugins, or execute graph rewiring; keeps JUCE hosting confined to `YesDawPluginHost`;
  and leaves `YESDAW_BUILD_APPS=OFF` pure sanitizer configs unaffected. Fix only proven defects. If clean
  and green, continue in the SAME baton to the next small worker chunk: a minimal coordinator
  blacklist-candidate drain-to-control-thread escalation shell for future blacklist handling, still without
  real blacklist policy/enforcement, persistence/cache, scanner, plugin loading, real graph rewiring,
  crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP, ADR edits, goldens, subjective
  checks, or RT-hot annotation edits. Stop for any new ADR-level decision. Create exactly one successor
  baton only after that checkpoint's `STATUS.md` update, commit, push, and remote CI are green.
- **Latest: WORKER H3 minimal coordinator blacklist-candidate status shell is locally green — the
  coordinator can identify whether the latest real crash/watchdog host failure is a future blacklist
  candidate without enforcing blacklist policy or persistence.**
  REVIEW/FIX of the previous deferred graph-change command acknowledge/clear-status shell found no proven
  defects against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety/layering rules: the
  acknowledge/clear shell is coordinator-side, headless, and testable; initial/empty status stays empty;
  normal stop records no command; watchdog receipt records watchdog cause; crash receipt overwrites with
  crash cause; causes stay distinct before clear; `acknowledgeDeferredGraphChangeCommandStatus()` clears the
  recorded deferred command/result after inspection and returns the now-empty status; no path claims or
  performs graph recompile execution; execution-claiming results stay rejected; `YesDawPluginHost` remains
  the only JUCE plugin-hosting owner; the coordinator/check target does not link `juce_audio_processors`;
  Apple framework links stay scoped to `YesDawPluginHost`; and `YESDAW_BUILD_APPS=OFF` pure sanitizer
  configs are unaffected. Then WORKER added the smallest blacklist-candidate status shell:
  `blacklistCandidateStatus()` derives a headless status from the latest host-failure report. It is empty
  for initial status and normal stop, marks watchdog-timeout failures as watchdog blacklist candidates,
  marks crash failures as crash blacklist candidates, and keeps the two causes distinct for future
  escalation. The coordinator self-check now proves initial and normal-stop statuses are not candidates,
  watchdog/crash failures are candidates with distinct causes, and the existing deferred graph-change
  acknowledge/clear path still does not execute graph recompiles. Scope held: no real plugin load, scanner,
  watchdog blacklist policy, blacklist/cache persistence, crash-test plugin, plugin UI, real shared memory,
  pluginval/auval, CLAP, ADR edits, goldens, broad graph rewiring, graph recompile execution, subjective
  checks, or `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation edits.
  Local gate: `cmake --preset ci`; documented VS DevShell `cmake --build --preset ci`; documented VS
  DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator blacklist-candidate status shell
  — verify `src/plugin_host/PluginHostCoordinator.h`, `src/plugin_host/PluginHostCoordinatorCheck.cpp`,
  `src/plugin_host/PluginHostMain.cpp`, `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake
  against ADR-0015 (watchdog/crash attribution, future blacklist escalation, and host-worker ownership),
  ADR-0013 (runtime crash/hang attribution escalates into the same blacklist later), ADR-0008 (engine
  targets must not link hosting / `Node` contract unchanged), and the rolling-baton rule. Confirm the
  status shell is coordinator-side, headless, and non-vacuous; reports no candidate for initial/empty status
  or normal stop; marks watchdog-timeout and crash failures as future blacklist candidates while preserving
  their distinction; does not enforce blacklist policy, persist/cache blacklist state, scan/load plugins, or
  execute graph rewiring; keeps JUCE hosting confined to `YesDawPluginHost`; and leaves
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs unaffected. Fix only proven defects. If clean and green,
  continue in the SAME baton to the next small worker chunk: a minimal coordinator pending
  blacklist-candidate queue/drain shell for future blacklist escalation, still without real blacklist
  policy/enforcement, persistence/cache, scanner, plugin loading, real graph rewiring, crash-test plugin,
  plugin UI, real shared memory, pluginval/auval, CLAP, ADR edits, goldens, subjective checks, or RT-hot
  annotation edits. Stop for any new ADR-level decision. Create exactly one successor baton only after that
  checkpoint's `STATUS.md` update, commit, push, and remote CI are green.
- **Latest: WORKER H3 minimal coordinator deferred graph-change command receipt/status shell is locally
  green — the coordinator can record the most recent deferred graph-change command/result for inspection
  without executing real graph rewiring or policy enforcement.**
  First, REVIEW/FIX of the previous drain-to-control-thread command shell found and fixed one narrow
  proven defect: `HostFailureKind::none` could be manually queued through the public pending
  `FailureActionRequest` surface as a bypass/recompile request and then produce a command. Commit
  `ee8e7e5` hardens command eligibility so only bypass/recompile requests with a real crash/watchdog
  failure kind can drain to a command, adds a self-check for the none-failure case, and is remote CI-green
  on run `28216003408` across Windows, Linux, macOS, RTSan, and TSan. Then WORKER added the smallest
  deferred receipt/status shell: `PluginHostCoordinator` now exposes
  `DeferredGraphChangeCommandStatus`, `recordDeferredGraphChangeCommandResult()`, and
  `deferredGraphChangeCommandStatus()`. The receipt surface records only command-ready,
  pending-consumed watchdog/crash command results that do **not** claim graph recompile execution; no-action
  or execution-claiming results leave the receipt empty. The coordinator self-check now proves initial
  status is empty, normal stop records no command, watchdog command receipt records watchdog cause, crash
  receipt overwrites it with crash cause, both causes remain distinct, and no receipt path claims or
  performs graph recompile execution. Scope held: no real plugin load, scanner, watchdog blacklist policy,
  blacklist/cache persistence, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP, ADR
  edits, goldens, broad graph rewiring, graph recompile execution, subjective checks, or
  `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation edits.
  Local gate: `cmake --preset ci`; documented VS DevShell `cmake --build --preset ci`; documented VS
  DevShell `ctest --preset ci` passed **187/187**.
  **Next:** REVIEW/FIX H3 minimal coordinator deferred graph-change command receipt/status shell — verify
  `src/plugin_host/PluginHostCoordinator.h`, `src/plugin_host/PluginHostCoordinatorCheck.cpp`,
  `src/plugin_host/PluginHostMain.cpp`, `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake
  against ADR-0015 (future bypass/recompile control-thread handoff and host-worker ownership), ADR-0013
  (crash/hung child leads to placeholder/bypass + recompile on the control side), ADR-0008 (engine targets
  must not link hosting / `Node` contract unchanged), and the rolling-baton rule. Confirm the receipt/status
  shell is headless and non-vacuous, records only command-ready crash/watchdog results, leaves no-action and
  execution-claiming results empty, preserves watchdog-timeout vs crash distinction, remains inspectable
  without executing graph recompile, keeps JUCE hosting confined to `YesDawPluginHost`, and leaves
  `YESDAW_BUILD_APPS=OFF` pure sanitizer configs unaffected. Fix only proven defects. If clean and green,
  continue in the SAME baton to the next small worker chunk: a minimal deferred graph-change command
  acknowledge/clear-status shell for the coordinator, still without real graph rewiring, policy enforcement,
  plugin loading, scanner, blacklist/cache persistence, crash-test plugin, plugin UI, real shared memory,
  pluginval/auval, CLAP, ADR edits, goldens, subjective checks, or RT-hot annotation edits. Stop for any new
  ADR-level decision. Create exactly one successor baton only after that checkpoint's `STATUS.md` update,
  commit, push, and remote CI are green.
- **Latest: REVIEW/FIX H3 minimal coordinator failure-action drain-to-control-thread command shell is
  locally green after one narrow hardening fix — `HostFailureKind::none` can no longer produce a deferred
  graph-change command through the public pending-request surface.**
  Review verified the command shell against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety /
  layering rules: it is coordinator-side, headless, and testable; it uses the existing
  `FailureActionRequest` surface; watchdog-timeout and crash causes remain mechanically distinct through
  drain-to-command; command results remain inspectable and `graphRecompileExecuted=false`; the coordinator
  target links `juce::juce_events` but not `juce_audio_processors`; `YesDawPluginHost` remains the only
  owner of JUCE plugin-hosting format registration; Apple framework links remain scoped to
  `YesDawPluginHost`; and `YESDAW_BUILD_APPS=OFF` pure sanitizer configurations remain unaffected.
  The review found one proven gap: callers could manually queue an inconsistent bypass/recompile
  `FailureActionRequest` with `HostFailureKind::none`, and the drain-to-command helper would accept it.
  Fixed by treating only bypass/recompile requests with a real failure kind as command-eligible, and by
  adding a coordinator self-check that fails if `HostFailureKind::none` produces a command. Scope held: no
  real plugin load, scanner, watchdog blacklist policy, blacklist/cache persistence, crash-test plugin,
  plugin UI, real shared memory, pluginval/auval, CLAP, ADR edits, goldens, broad graph rewiring, graph
  recompile execution, or `[[clang::nonblocking]]` / `YESDAW_RT_HOT` annotation edits.
  Local gate: `cmake --preset ci`; documented VS DevShell `cmake --build --preset ci`; documented VS
  DevShell `ctest --preset ci` passed **187/187**.
  **Next:** WORKER H3 minimal coordinator deferred graph-change command receipt/status shell — add the
  smallest coordinator-side receipt/status surface for the future control-thread graph-change handoff,
  recording the most recent deferred command/result for inspection without executing real graph rewiring or
  policy enforcement. Keep it headless and self-asserting; preserve engine RT-safety and JUCE-hosting
  confinement; use the existing pending `FailureActionRequest` and graph-change command/result surface. No
  real plugin load, scanner, watchdog blacklist policy, blacklist/cache persistence, crash-test plugin,
  plugin UI, real shared memory, pluginval/auval, CLAP, ADR edits, goldens, broad graph rewiring, real graph
  recompile execution, subjective checks, or RT-hot annotation edits. Stop for any new ADR-level decision.
- **Latest: WORKER H3 minimal coordinator failure-action drain-to-control-thread command shell is
  CI-green — the coordinator can consume one pending bypass/recompile request into an inspectable
  future graph-change command/result without executing a real graph recompile.**
  First, REVIEW/FIX of the previous pending failure-action queue/drain shell found no proven defects
  against `STATUS.md`, ADR-0015, ADR-0013, ADR-0008, and the RT-safety/layering rules: the pending action
  shell is coordinator-side, headless, and testable; it uses the existing `HostFailureReport` ->
  `FailureActionRequest` surface; expected stop / `HostFailureKind::none` leaves no pending action;
  watchdog-timeout and crash causes remain mechanically distinct through queue/drain; the coordinator/check
  target still links `juce::juce_events` but not `juce_audio_processors`; `YesDawPluginHost` remains the
  only owner of JUCE plugin-hosting format registration; Apple framework links remain scoped to
  `YesDawPluginHost`; and the `YESDAW_BUILD_APPS=OFF` RTSan/TSan pure configurations remain outside the
  JUCE app/host targets.
  Then WORKER added the smallest control-thread command shell: `PluginHostCoordinator` now exposes
  `GraphChangeCommandKind`, `GraphChangeCommandStatus`, `GraphChangeCommand`,
  `GraphChangeCommandResult`, and `drainPendingFailureActionRequestToControlCommand()`. The command shell
  drains the existing pending `FailureActionRequest`, maps watchdog-timeout and crash bypass/recompile
  requests to an inspectable future graph-change command, clears pending storage, and keeps
  `graphRecompileExecuted=false`. `YesDawPluginHostCoordinatorCheck` now fails unless normal stop produces
  no command, watchdog timeout drains a bypass/recompile command with watchdog cause, crash/lost-child
  observation drains a bypass/recompile command with crash cause, both causes stay distinct, and no command
  path claims to execute a graph recompile. Scope held: no real plugin load, scanner, watchdog blacklist
  policy, blacklist/cache persistence, crash-test plugin, plugin UI, real shared memory, pluginval/auval,
  CLAP, ADR edits, goldens, broad graph rewiring, graph recompile execution, or `[[clang::nonblocking]]` /
  `YESDAW_RT_HOT` annotation edits.
  Local gate: `cmake --preset ci`; documented VS DevShell `cmake --build --preset ci`; documented VS
  DevShell `ctest --preset ci` passed **187/187**. Remote CI run `28215350783` is green across Windows,
  Linux, macOS, RTSan, and TSan for commit `c936275`.
  **Next:** REVIEW/FIX H3 minimal coordinator failure-action drain-to-control-thread command shell — verify
  `src/plugin_host/PluginHostCoordinator.h`, `src/plugin_host/PluginHostCoordinatorCheck.cpp`,
  `src/plugin_host/PluginHostMain.cpp`, `src/plugin_host/PluginHostProtocol.h`, and directly relevant CMake
  against ADR-0015 (coordinator/worker process model, crash/watchdog reporting, future bypass/recompile
  command surface, host-worker ownership), ADR-0013 (out-of-process host child boundary and crash/hung-child
  kill leading to placeholder/bypass + recompile on the control side), ADR-0008 (engine targets must not
  link hosting / `Node` contract unchanged), and the rolling-baton rule. Confirm the command shell is
  non-vacuous, expected stop cannot produce a command, watchdog-timeout and crash causes remain distinct
  through drain-to-command, the command result remains inspectable without executing graph recompile, the
  coordinator target still does not own JUCE plugin-hosting modules, `YESDAW_BUILD_APPS=OFF` pure sanitizer
  configuration is unaffected, and no scanner/blacklist policy/shared-memory/plugin-load or real
  graph-recompile semantics snuck in. Fix only proven defects. If clean and green, continue in the SAME
  baton to the next small worker chunk: a minimal coordinator deferred graph-change command receipt/status
  shell that records the most recent deferred command/result for inspection without executing real graph
  rewiring or policy enforcement (still no real plugin load, scanner, watchdog blacklist policy,
  blacklist/cache persistence, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP, ADR
  edits, or goldens). Stop at any new ADR-level decision. Create exactly one successor baton only after
  that checkpoint's `STATUS.md` update, commit, push, and remote CI are green.
- **Latest: WORKER H3 `YesDawPluginHost` worker exe + engine-hosting layering check is green locally — the host boundary exists.**
  First, REVIEW/FIX of the previous `PluginNode` IPC-proxy checkpoint found no proven defects against
  `STATUS.md`, ADR-0015, ADR-0013, ADR-0007, ADR-0008, ADR-0009, and the RT-safety rules: `process()` stays
  one `RtLaneRing::exchangeBlock`, in-place input/output is safe because the ring captures input before
  overwrite, one-Block-late/fail-open/PDC tests are non-vacuous, latency/channel validation bounds what
  reaches `GraphBuilder`, the `Node`/`ProcessArgs` contracts stayed frozen, and the engine still contains
  no JUCE hosting. Then WORKER added the narrow ADR-0015 process-boundary chunk: new
  `src/plugin_host/PluginHostMain.cpp` and `YesDawPluginHost`, a console worker executable with a
  `juce::ChildProcessWorker` stub, VST3 hosting enabled through `juce_audio_processors`, and a
  `--self-check` mode that asserts JUCE plugin formats are present. `CMakeLists.txt` now wires that target
  only when `YESDAW_BUILD_APPS=ON`, adds `YesDawPluginHostSelfCheck` to ctest, and adds a configure-time
  layering assertion: the pure engine/test targets (`YesDawGraphCheck`, `YesDawPluginNodeCheck`,
  `YesDawPluginIpcCheck`, etc.) fail configure if they directly link `juce_audio_processors`, while
  `YesDawPluginHost` must link it. Scope held: no real child launch/coordinator, scanner, watchdog,
  blacklist/cache, crash-test plugin, plugin UI, real VST3/AU loading, real shared memory, CLAP, ADR edits,
  goldens, broad graph rewiring, or annotation edits. Local gate: `cmake --preset ci` passed; plain shell
  build lacked Windows SDK/MSVC include paths, so the documented VS DevShell flow was used for
  `cmake --build --preset ci` and `ctest --preset ci`; full ctest passed **186/186** (+1 host self-check).
  First remote CI run for commit `0014557` went green on Windows, Linux, RTSan, and TSan but red on macOS
  at the host-worker link step: AU hosting referenced `AUGenericView`. Commit `33fd70a` linked `AudioUnit`
  only for `YesDawPluginHost` on Apple, but run `28208630326` proved that was still red on macOS because
  `AUGenericView` resolves from `CoreAudioKit`. Commit `a5b7781` links both `AudioUnit` and `CoreAudioKit`
  only for `YesDawPluginHost` on Apple; remote CI run `28208956977` is green across Windows, Linux, macOS,
  RTSan, and TSan.
  **Next:** REVIEW/FIX H3 `YesDawPluginHost` worker exe + engine-hosting layering check — verify
  `CMakeLists.txt` and `src/plugin_host/PluginHostMain.cpp` against ADR-0015 (single host worker target,
  coordinator/worker process model, host owns JUCE hosting), ADR-0013 (out-of-process host child boundary),
  ADR-0008 (engine targets must not link hosting / `Node` contract unchanged), and the rolling-baton rule.
  Confirm the self-check is non-vacuous, the layer assertion covers the engine-side targets that exercise
  engine code in normal/RTSan/TSan CI, `YESDAW_BUILD_APPS=OFF` pure sanitizer configuration is unaffected,
  and no scanner/watchdog/shared-memory/plugin-load semantics snuck in. Fix only proven defects. If clean
  and green, continue in the SAME baton to the next small worker chunk: a minimal plugin-host coordinator
  launch/handshake shell for `YesDawPluginHost` (still no real plugin load, scanner, watchdog policy,
  blacklist/cache, crash-test plugin, plugin UI, real shared memory, pluginval/auval, CLAP, ADR edits, or
  goldens). Stop at any new ADR-level decision. Create exactly one successor baton only after this
  checkpoint's `STATUS.md` update, commit, push, and remote CI are green.
- **Latest: WORKER H3 `PluginNode` IPC proxy over the RT-lane ring is green locally — hosting reaches the graph.**
  Built ADR-0015's graph-visible plugin adapter: new header-only `src/engine/plugin/PluginNode.h`, a `Node`
  (ADR-0008) that owns an `RtLaneRing` and exposes a hosted plugin to the compiler **without any change to
  the frozen `Node` base contract, `ProcessArgs`, `GraphBuilder`, or `CompiledGraph`**. Key architecture
  win: it slots straight into the EXISTING `CompiledNodeKind::Plugin` — `GraphBuilder::detectKind` already
  returns `Plugin` as its fallback for any unrecognised `Node*`, and `CompiledGraph::process` already feeds a
  single-input non-bus node its producer's audio in-place (copies producer output into the node's own slot,
  then calls `process()` with that slot as both in and out). So adding hosting is the pure adapter ADR-0002
  #3 promised. **Audio thread (`process()`, `YESDAW_RT_HOT`, noexcept):** exactly one
  `RtLaneRing::exchangeBlock` for this Block — the same in-place buffer is passed as BOTH ring input and
  output (safe: exchangeBlock fully captures the input into the ring before it overwrites the output with
  Block N-1's result), failing open last-good -> silence -> bypass; it never allocates/locks/logs/does
  I/O/signals/waits. **Latency/PDC (ADR-0007/0015):** `properties().latencySamples` = one pipeline Block
  (the ring's deterministic single-Block delay) + the plugin's VALIDATED latency. Validation lives in the
  node so a bogus claim can't reach PDC: negatives quarantine to zero, absurd values clamp to
  `kMaxValidatedLatencySamples` (~57 s @192k, kept under `GraphBuilder::kMaxLatencyCap` so a clamped report
  is accepted/compensated, not rejected), channels clamp to `[1, 8]`. The pipeline Block size is fixed at
  construction because the compiler reads `properties()` before `prepare()`; the ring is sized only in
  `prepare()` (the one allocation). **Headless (this chunk):** the "plugin" is the ring's child role driven
  by an in-process stub processor (identity by default; settable to a gain/latency stand-in), pumped
  synchronously by the test via `serviceStubChild()` to model the real child process publishing off the
  audio thread. NO real child process, `YesDawPluginHost` worker exe, JUCE hosting, scanner, watchdog, or
  coordinator — and PluginNode contains NO `juce::AudioProcessor`, so ADR-0008's engine⇏hosting layering
  boundary holds. New pure-C++ test target **`YesDawPluginNodeCheck`** (built unconditionally so the RTSan
  leg covers `PluginNode::process()`/exchangeBlock and the TSan leg covers it), written **test-first
  (TDD red -> green)**, 5 self-asserting tests through the **REAL `GraphBuilder` + `CompiledGraph`**: (1) a
  PluginNode in a compiled graph delivers its stub child's output EXACTLY one Block late, proven with a
  per-Block-varying signal so a wrong delay can't pass; (2) the fail-open ladder last-good -> silence ->
  bypass + recovery to Fresh when the child catches up, the audio thread never blocking and never emitting
  garbage; (3) the reported latency DRIVES PDC convergence — alignment-sensitive (a one-shot impulse lands
  at exactly one (Block, frame) only because PDC spliced a `LatencyNode(oneBlock)` onto the parallel
  sidechain path) PLUS structural (`totalLatency() == B`, a LatencyNode was spliced); (4) latency/channel
  validation + reporting (one Block + L; negative quarantined; absurd clamped; channels clamped); and (5) a
  hostile `INT64_MAX` latency claim builds successfully with the clamped value rather than overflowing the
  PDC walk. Scope held to the adapter: no `GraphBuilder`/`CompiledGraph`/`Node`-contract changes, no real
  shared memory, host exe, scanner, watchdog, JUCE, ADR, golden, or `[[clang::nonblocking]]`/`YESDAW_RT_HOT`
  annotation edits; LF endings. Local gate via the documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (185/185, +5 new). RTSan/TSan are Clang-20/Linux
  CI-only (cannot run locally on Windows). Remote CI is **GREEN across all five legs** for commit `822d404`
  (run `28207115401`: Windows, Linux, macOS, RTSan, TSan).
  **Next:** REVIEW/FIX H3 `PluginNode` IPC proxy — verify `src/engine/plugin/PluginNode.h` +
  `tests/plugin_node_tests.cpp` against `STATUS.md`, ADR-0015 (RT lane / one-Block pipeline / fail-open /
  validated latency), ADR-0013 (`PluginNode` as the out-of-process IPC proxy), ADR-0007 (PDC = deterministic
  single-Block latency; validated plugin latency can't overflow the walk), ADR-0008 (the `Node` base
  contract + `ProcessArgs` stay frozen; engine⇏hosting layering), ADR-0009 (Events), and the RT-safety rules
  (the audio thread never allocates/locks/logs/syscalls; in-place exchangeBlock is safe; fail-open is
  branch-only; no torn/garbage delivery). Fix only proven defects. Keep it the headless adapter — do NOT
  start the `YesDawPluginHost` `ChildProcessWorker` target, real shared memory (mmap/`CreateFileMapping`),
  the coordinator watchdog, the crash-test plugin, the scanner, or JUCE; no ADR, golden, or
  `[[clang::nonblocking]]`/`YESDAW_RT_HOT` edits. Confirm the one-Block-late delivery, fail-open ladder, and
  PDC alignment tests are non-vacuous and assert the right thing, and that the latency/channel validation
  truly bounds what reaches the compiler. Run the gate, update `STATUS.md`, commit/push, and check CI. If
  the review is clean/green, continue in the SAME rolling-baton thread to the next worker chunk: the
  `YesDawPluginHost` worker exe + engine-doesn't-link-hosting layering check. Create the successor baton
  only after that worker chunk has its own updated `STATUS.md`, commit, push, and green CI result. Do not
  spawn a successor while this review or CI is still pending, red, stuck, or being rerun.
- **Latest: REVIEW/FIX H3 RT-lane shared-memory ring found no defects — review clean, ring is solid.**
  Ran an independent formal review of the post-fix ring (`src/engine/plugin/RtLaneRing.h` +
  `tests/rt_lane_tests.cpp`) against the LITERAL text of ADR-0015 (RT lane / one-Block pipeline /
  fail-open), ADR-0007 (deterministic single-Block latency for PDC), ADR-0008 (frozen `Node` contract),
  ADR-0009 (serializable Events), and the RT-safety rules — three independent reviewers, all PASS, zero
  defects. (1) Memory model: re-derived from `[atomics.fences]` that the seqlock is now portably correct
  AND complete — both readers fence-acquire between the relaxed payload loads and the v2 re-read, the
  writer fence-releases after the odd-version store, v1/endWrite pair correctly, and NO payload+version
  site is missing its fences. (2) Spec conformance: every pinned RT-lane requirement is present (double
  buffer; input audio + Event ring + output audio + control words; the audio thread release-stores
  inputSeq / acquire-loads outputSeq / reads Block N-1 deterministically / never
  allocates-locks-logs-IO-syscalls; fail-open last-good -> silence -> bypass; child poll off the audio
  thread); the `Node` contract is untouched and Events stay ADR-0009. (3) No regression from the worker's
  own fixes: `bit_cast` is a lossless memcpy-equivalent, the fences are pure barriers (RTSan-clean), and
  the 10 tests assert the right thing without vacuity or flakiness. DECISION on the one open scope call:
  the ring's in-ring bypass SELF-HEAL (clears on the next Fresh) is NOT a contradiction of ADR-0015 and NOT
  an ADR-level issue — ADR-0015 separates the audio-thread branch-only fail-open (this primitive) from the
  control-thread coordinator's kill -> blacklist -> recompile -> placeholder (a later chunk), and the
  coordinator's real trigger is its own watchdog TIMER, so a transiently-late plugin correctly resumes
  rather than being permanently condemned (ADR-0002 no-dropout). Recorded the one nuance as a code comment:
  `bypassActive()` is a transient, self-clearing signal, NOT the authoritative crash verdict — the future
  coordinator must drive kill/blacklist from its watchdog, not this flag. Status-only closeout plus that
  one-line doc comment. Local gate via the documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (180/180). The reviewed worker tip `8a092da` is
  green in remote CI across all five legs (Windows, Linux, macOS, RTSan, TSan); remote CI for this closeout
  is pending until pushed.
  **Next:** WORKER H3 `PluginNode` IPC proxy over the RT-lane ring — the graph-visible `Node` adapter
  (ADR-0008 / ADR-0013 / ADR-0015) that, inside `process()` on the audio thread, drives
  `RtLaneRing::exchangeBlock` for its Block and reports the validated one pipeline Block + plugin latency to
  the compiler (ADR-0007 PDC). Keep it HEADLESS — the "plugin" is the in-process ring's child role / a stub
  processor; NO real child process, `YesDawPluginHost` worker exe, JUCE hosting, scanner, or watchdog yet
  (those are the chunks after). Keep ADR-0008's `Node` base contract frozen (the adapter wraps the ring
  behind the existing `properties`/`directInputs`/`prepare`/`process`/`reset`/`release` shape; allocate the
  ring only in `prepare`). Validate plugin-reported latency/channels before they reach the compiler
  (ADR-0015: clamp, reject impossible values). Prove with self-asserting tests (RTSan/TSan-covered): a
  `PluginNode` inside a real compiled graph delivers its child's one-Block-delayed output, fails open
  without dropouts, and its latency drives PDC. STOP at any new ADR-level decision. Then REVIEW/FIX, and
  continue the worker -> review loop toward the H3 hosting exit gates.
- **Latest: WORKER H3 plugin-hosting RT-lane shared-memory ring is green locally — first hosting code lands.**
  Built ADR-0015's RT lane as a headless, in-process primitive: new header-only `src/engine/plugin/RtLaneRing.h`,
  the lock-free, double-buffered audio + Event ring that implements the one-Block plugin handshake. It is
  **bytes-location-agnostic** — the exact atomic protocol that will later live in OS shared memory — so it
  does NOT do real cross-process mmap/`CreateFileMapping` yet, and there is no JUCE, no `PluginNode`, no
  child process (the "child" is a second test thread that polls). Per direction it has a DOUBLE buffer of
  slots plus the ADR-named control words: `inputSeq` (release-stored by the audio thread after writing
  Block N's input+Events), `outputSeq` (acquire-loaded by the audio thread as the output-ready counter),
  `validatedLatency`, and `status`. The **audio-thread role** `exchangeBlock` (`YESDAW_RT_HOT`, the future
  `PluginNode::process()`) writes Block N's input then release-stores `inputSeq`, then reads Block **N-1**'s
  output **deterministically** (exactly one Block of latency, for ADR-0007 PDC) with the **fail-open ladder**
  — last-good -> silence -> bypass, all branch-only; it never signals/waits/allocates/logs/syscalls. The
  **child role** `pollOnce` (off the audio thread) polls `inputSeq`, processes the newest input, and
  release-stores `outputSeq`. Race-freedom: a strict double buffer + a never-blocking audio thread cannot be
  race-free under arbitrary timing (the lock-free-mailbox result that otherwise forces triple buffering), so
  each slot carries a **seqlock version** (odd while writing, even when stable) and its payload words are
  **relaxed atomics** — a concurrent lap is therefore well-defined (not UB) and simply discarded as a miss.
  That keeps ADR-0015's pinned double-buffer + sequence-counter mechanism intact AND makes the protocol
  formally TSan-safe; all cross-thread state is atomic, everything else is endpoint-thread-local (allocated
  only in `prepare`). New pure-C++ test target **`YesDawPluginIpcCheck`** (built unconditionally so the RTSan
  leg covers `exchangeBlock` and the TSan leg covers the protocol), 6 self-asserting tests: one-Block-delay
  identity across Blocks; the fail-open ladder (last-good -> silence -> bypass) + recovery; the control words
  (validated latency + status); the Event ring carrying a `ParameterChange` sample-accurately (the child
  applies it from its `timeInBlock` offset) one Block late; correctness across channel counts + varying Block
  sizes; and a concurrent producer/consumer stress test in two modes — **flat-out** (the audio thread outruns
  the child -> same-slot lapping reads, the case the seqlock + relaxed atomics must keep race-free for TSan)
  and **paced** (sustained, exactly-one-Block-late delivery). Scope held to a primitive: no real shared
  memory, `PluginNode`, scanner, watchdog, JUCE, ADR, golden, or `[[clang::nonblocking]]`/`YESDAW_RT_HOT`
  annotation edits. Local gate via the documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (180/180). Then ran an adversarial multi-agent
  review of the primitive (ultracode): it found one REAL portable-seqlock memory-ordering defect — the
  reader needed an `atomic_thread_fence(acquire)` before the version re-check and the writer a
  `fence(release)` after the odd-version store (the Boehm seqlock result; TSan cannot see it on x86 TSO but
  it is real on weaker memory models and in the cross-process shared memory this protocol will later run
  in) — now FIXED (commit `5dee0b3`) — plus four test-strength gaps (the flat-out stress could pass
  vacuously; event overflow, `numFrames` clamp, `reset()`, and varying-frames were untested), now
  hardened/covered (commit `8a092da`). Skeptics killed seven other findings as false-positives/intended
  scope; the one worth a human glance: in-ring bypass SELF-HEALS on the next Fresh, whereas ADR-0015's full
  recovery is blacklist -> recompile -> placeholder — that is the coordinator's job (a later chunk), so it
  is a scope deferral, not a defect. RTSan/TSan are Clang-20/Linux CI-only (cannot run locally on Windows).
  Remote CI is **GREEN across all five legs** (Windows, Linux, macOS, RTSan, TSan) for the tip commit
  `8a092da` (run `28203931331`) — so the audio thread provably never allocates/locks/syscalls and the
  protocol is provably race-free.
  **Next:** REVIEW/FIX H3 RT-lane shared-memory ring — verify `RtLaneRing` + `tests/rt_lane_tests.cpp` against
  `STATUS.md`, ADR-0015 (RT lane / one-Block pipeline / fail-open), ADR-0013, ADR-0007 (PDC = deterministic
  single-Block latency), ADR-0008 (the `Node` base contract stays untouched), ADR-0009 (serializable Events),
  and the RT-safety rules (the audio thread never allocates/locks/logs/syscalls; release/acquire + seqlock
  correctness; no torn/garbage delivery). Fix only proven defects. Keep it a primitive — do NOT start real
  shared memory (mmap/`CreateFileMapping`), the `PluginNode` IPC proxy, the `YesDawPluginHost`
  `ChildProcessWorker` target, the coordinator watchdog, the crash-test plugin, the scanner, or JUCE; no ADR,
  golden, or `[[clang::nonblocking]]`/`YESDAW_RT_HOT` edits. The ultracode adversarial review above is a head
  start — the formal review should independently re-derive the seqlock fence correctness and decide the one
  open scope call (in-ring bypass self-heal vs ADR-0015's blacklist/recompile recovery: confirm it is
  correctly deferred to the coordinator, or surface to Dan if it should change now). Run the gate, update
  `STATUS.md`, commit/push, check CI, then create the next WORKER thread (`PluginNode` IPC proxy over the
  ring) only if green.
- **Latest: ADR-0015 plugin-hosting runtime written + reviewed (one fix) — kicks off the H3 hosting half.**
  Dan chose the ADR-first path. `docs/adr/0015-plugin-hosting-runtime-ipc-and-process-model.md` refines
  ADR-0013's deferred implementation choices (it explicitly left the shared-memory/ring details, per-OS
  sandbox, plugin UI embedding, and CI fixtures open) without revising ADR-0013. It pins: one dedicated
  **plugin host child** per plugin via JUCE `ChildProcessCoordinator`/`ChildProcessWorker` (a single
  `YesDawPluginHost` worker exe, the ONLY target that links JUCE hosting — engine stays hosting-free,
  layering-checked); a control-thread **Plugin host coordinator** + watchdog (hang -> kill -> blacklist ->
  bypass/placeholder + recompile; same mechanism backs the scanner); a two-lane IPC seam (control lane =
  coordinator message channel; RT lane = a per-`PluginNode` shared-memory region with input/output audio +
  Event ring + control words, double-buffered for the one-Block pipeline) where the **audio thread only
  does lock-free release/acquire stores/loads and fails open within the Block budget**, never
  signalling/waiting/syscalling (child wakeup is off the audio thread); latency/PDC reuse ADR-0007 with
  validated plugin latency + coalesced rate-limited recompiles; the **process boundary + watchdog is H3's
  isolation guarantee** (OS-level sandbox hardening, provenance/signature, plugin UI embedding, CLAP, and a
  shared-process pool are sequenced as follow-ups); and a deterministic **in-repo crash-test plugin**
  (passthrough/NaN/hang/crash) is the always-on **host-isolation exit gate**, with pluginval L8-10 / `auval`
  as external-binary gates (license gate keeps GPL out of the linked binary). Updated the ADR index and
  added **Plugin host coordinator** to `CONTEXT.md`. REVIEW/FIX fixed one imprecision (the one-Block
  pipeline wording wrongly implied a non-audio thread writes the plugin input; `PluginNode::process()` runs
  on the audio thread and writes it there as a lock-free store — corrected). Docs-only; the 170/170 gate is
  unchanged. Remote CI pending until pushed.
  **Next:** WORKER H3 plugin-hosting **RT-lane shared-memory ring** — the first, most foundational
  implementation chunk and fully headless/testable in-process before any real child process or JUCE: a
  lock-free, double-buffered audio + Event ring with release/acquire sequence counters implementing the
  one-Block handshake and the fail-open read (last-good -> silence -> bypass within budget), proven by a
  same-process producer/consumer test (RTSan/TSan-covered). Then later chunks, REVIEW/FIX between each:
  `PluginNode` IPC proxy over the ring -> `YesDawPluginHost` `ChildProcessWorker` target + the
  engine-doesn't-link-hosting layering check -> coordinator watchdog kill->bypass->recompile -> the in-repo
  crash-test plugin + the host-isolation no-dropout/nonblocking exit gate -> scanner blacklist/cache ->
  pluginval/`auval` + license gates. Stop at any new ADR-level decision.
- **Latest: REVIEW/FIX H3 Sidechain input pins found no proven defect — mixer-policy half of H3 is complete.**
  Reviewed `SidechainGainNode` + the GraphBuilder/CompiledGraph changes (worker commit `3211f5e`) against
  `STATUS.md`, ADR-0014, ADR-0007 (PDC convergence / buffer last-reader), ADR-0008 (frozen Node contract),
  the H3 plan/deepening notes, and the live contracts. Main-first input ordering is robust (sort skipped for
  the Sidechain kind; the PDC pass preserves input position even when it splices a `LatencyNode`, so matching
  by producer id — which the splice changes — is correctly avoided); the consumer gets a fresh, non-aliased
  output slot and the per-sample read-then-write is safe even under aliasing; determinism holds (Sum/Master
  keep canonical producer-id order, the 167 prior tests are unchanged, and a sidechain node's
  `[main, single-pin]` order is stable because multiple sources converge through a `SumNode` first). One
  observation, not a defect: a Sidechain node wired with no sidechain input outputs silence (safe, no
  crash); an explicit "require exactly two inputs" build-time validation is a noted future option. Worker
  commit `3211f5e` is green in remote CI run `28199783306` across Windows, Linux, macOS, RTSan, and TSan.
  Status-only closeout. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (170/170).

  **H3 status:** the **mixer-policy half is done and CI-green** — bus-Return stereo centering, the
  mute / SIP-solo / solo-safe post-compile mute mask, and Sidechain input pins with PDC. The **remaining H3
  half is the plugin-hosting runtime (ADR-0013)**: out-of-process `PluginNode` IPC proxy over serializable
  audio/Event buffers, one-Block nonblocking fail-open, plugin scanner watchdog/blacklist/cache, and
  pluginval / `auval` / host-isolation gates. That is a large new subsystem (process isolation + IPC +
  real VST3/AU SDK integration) whose first step is effectively ADR-level (IPC transport / process model /
  SDK + sandbox approach per OS), so it should be scoped with Dan before code lands rather than started
  autonomously.
  **Next:** Dan's call on the plugin-hosting approach (ADR-0013 set the principles; the implementation
  needs the IPC/process/SDK specifics pinned as an ADR refinement first). Then WORKER plugin-hosting in
  small green chunks (likely: scanner skeleton -> PluginNode adapter -> out-of-process IPC -> fail-open ->
  pluginval/auval/host-isolation gates), REVIEW/FIX between each, until the H3 exit gates are green, then
  hard-stop for Dan's H3->H4 horizon-boundary review.
- **Latest: WORKER H3 Sidechain input pins (graph edges + PDC convergence) is green locally.**
  Implemented Sidechain input pins as real compiler-visible graph inputs with no change to ADR-0008's
  frozen `Node` base contract or `ProcessArgs`: a sidechain pin is an ordered auxiliary input, and a node
  interprets its bound inputs positionally (input 0 = main, input 1 = sidechain) — sidechain-ness is binding
  metadata, exactly as ADR-0014 decided. New `SidechainGainNode` is a minimal sidechain-capable built-in
  (its main signal is gain-modulated sample-by-sample by its sidechain; multi-input like `SumNode`, own
  `bindInputs`, no allocation in `process()`). GraphBuilder gained a `CompiledNodeKind::Sidechain`
  (`detectKind`), a bind path (`sidechainInputsFor` + the node's `bindInputs`), and accepts it in the
  multi-input-bound check; the producer-id input sort is SKIPPED for the Sidechain kind so `[main,
  sidechain]` order survives — the PDC pass preserves input position even when it splices a `LatencyNode`
  onto the shorter path, so the fragile alternative of matching by producer id (which the splice changes) is
  avoided. PDC came for free: the convergence pass already splices `LatencyNode`s for any >=2-input node, so
  a sidechain consumer's main and sidechain auto-align; the buffer-pool last-reader analysis already counted
  sidechain/multi-input readers (CompiledGraph contract R4). 3 self-asserting tests through the real
  GraphBuilder + CompiledGraph: `out = main * sidechain`; PDC alignment proven by an alignment-sensitive
  multiply of two impulses (main lat 0, sidechain lat 5 -> a `LatencyNode` is spliced, both impulses land on
  frame 5, exactly one non-zero output frame — misalignment would be silent); and multiple sources
  converging through an explicit `SumNode` into one pin (ADR-0014). The 167 prior tests are unchanged, so
  the sort-skip is scoped to the Sidechain kind only (Sum/Master keep their canonical producer-id order /
  bit-identical recompiles). No Project/persistence schema, plugin-host runtime, golden, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (170/170). Remote CI is pending until this worker +
  status tip is pushed.
  **Next:** REVIEW/FIX H3 Sidechain input pins: verify `SidechainGainNode` + the GraphBuilder/CompiledGraph
  changes against `STATUS.md`, ADR-0014, ADR-0007/0008, the H3 plan/deepening notes, and current contracts
  (frozen Node contract; main-first ordering robust to PDC splicing; multi-input bound checks; last-reader
  analysis). Fix only proven defects. Then assess the **H3 exit gates**: the mixer-policy half (mute /
  SIP-solo / solo-safe mask + Sidechain pins) is now done; the remaining H3 half is the **plugin-hosting
  runtime** (ADR-0013: out-of-process `PluginNode` IPC proxy, scanner watchdog/blacklist, one-Block
  fail-open, pluginval/`auval`/host-isolation gates). That is a large new area — start it as its own
  WORKER/REVIEW loop and STOP at any new ADR-level decision; surface scope to Dan at the H3 horizon
  boundary.
- **Latest: REVIEW/FIX H3 mixer mute mask found no proven defect.**
  Reviewed `MixerMutePolicy` + `CompiledGraph::isMuteCapable` (worker commit `62fba52`) against `STATUS.md`,
  ADR-0014, ADR-0007 (mask flipped without recompile), ADR-0008 (frozen Node contract), the H3
  plan/deepening notes, and the live `CompiledGraph` mute machinery. The effective-mute truth table matches
  ADR-0014 (explicit Mute wins; SIP solo active only on an unmuted soloed target; solo-safe exempts from
  solo-muting but never from explicit Mute); the mute-point mapping is correct (a Track's source node gates
  its direct path AND its Send taps; a Return's Bus SumNode gates the whole Return); and the policy
  pre-validates all targets so a non-mute-capable target fails with the mask unchanged. The mask updates as
  a short burst of control-thread atomic flips (far shorter than one audio block, self-healing within a
  block) and writes every target's bit each call with non-targets never muted, so there are no stale bits;
  a single atomic whole-mask publish is a noted future refinement (tighter solo-toggle transient), not a
  proven defect, so green code was left unchanged. Worker commit `62fba52` is green in remote CI run
  `28194248828` across Windows, Linux, macOS, RTSan, and TSan. No code changes; status-only closeout. Local
  gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (167/167).
  **Next:** WORKER H3 Sidechain input pins — add Sidechain input pins as real compiler-visible graph inputs
  with PDC: ordered auxiliary inputs on sidechain-capable Nodes whose edges are visible to GraphBuilder
  before topo / PDC / buffer-liveness / last-reader analysis, converging through an explicit `SumNode` / Bus
  when multiple sources feed one pin, while keeping ADR-0008's `Node` base contract and `ProcessArgs` shape
  frozen (pin roles are graph/compiler metadata or adapter binding). A sidechain-capable consumer is a PDC
  convergence point between its main input and every Sidechain pin (GraphBuilder delays the shorter paths),
  and any Event/automation carried with a Sidechain path shifts by the same per-path PDC. Prove each with
  self-asserting tests. STOP and surface to Dan at any new ADR-level decision (e.g. if the pin
  representation cannot be expressed as metadata over the frozen Node contract). No Project/persistence
  schema, plugin-host runtime, golden, or `[[clang::nonblocking]]` shortcut edits.
- **Latest: WORKER H3 mixer mute mask (mute / SIP-solo / solo-safe) is green locally.**
  First completed the queued **REVIEW/FIX H3 mixer policy ADR-0014**: verified ADR-0014 (including the new
  bus-Return stereo-width addendum) against `STATUS.md`, ADR-0007 (mask flipped without recompile / compile
  pass 5), ADR-0008 (frozen Node base contract), ADR-0009 (PDC shifts the event stream by per-path latency),
  ADR-0013 (sidechain pins on PluginNode), the H3 plan/deepening notes (Returns and sidechain consumers are
  PDC convergence points), `CONTEXT.md`, and the live `CompiledGraph` `setMuted`/`isMuted`/`muteBit`
  machinery (proven by `YesDawBuilderCheck`). Found **no proven doc defect**, so this is a clean review.
  Then implemented the policy: new header-only `MixerMutePolicy` derives the post-compile mute mask from
  per-target mute / SIP-solo / solo-safe state on the control thread and publishes it through the existing
  mute seam — the audio thread never evaluates the policy and the graph is never recompiled to mute.
  `mixerAnyActiveSolo` (SIP solo active iff some unmuted target is soloed), `mixerTargetIsEffectivelyMuted`
  (explicit Mute wins; under active solo only soloed/solo-safe stay audible; solo-safe never overrides Mute),
  and `applyMixerMutePolicy` (pre-validates every target via the new `CompiledGraph::isMuteCapable`, then
  publishes; fails with the mask UNCHANGED if any target is not mute-capable — never a partial mask). Mute
  point mapping (mixer-projection work per ADR-0014): a Track's target is its SOURCE node, so zeroing it
  removes the direct path AND every Send tap; a Return's target is its Bus SumNode. 8 self-asserting tests:
  the ADR-0014 effective-mute truth table (pure), plus built-graph proofs that muting a Track silences its
  direct path in both channels, muting a Track removes its Send contribution from a Return, SIP solo leaves
  only the soloed Track audible, a solo-safe Return stays audible WITHOUT leaking a non-soloed Track's send,
  and a non-mute-capable target fails with the mask unchanged. No Sidechain code, Project/persistence schema,
  plugin-host code, golden, or `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell
  flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (167/167). Remote CI is
  pending until this worker + status tip is pushed.
  **Next:** REVIEW/FIX H3 mixer mute mask: verify `MixerMutePolicy` + `CompiledGraph::isMuteCapable` against
  `STATUS.md`, ADR-0014, ADR-0007/0008, the H3 plan/deepening notes, and current contracts; fix only proven
  defects (no Project/persistence schema, plugin-host, Sidechain, golden, or `[[clang::nonblocking]]` edits).
  Then WORKER: Sidechain input pins as real compiler-visible graph inputs with PDC (ordered auxiliary inputs
  on sidechain-capable Nodes; edges visible to GraphBuilder before topo/PDC/buffer-liveness; converge through
  explicit SumNode/Bus when multiple sources feed one pin; keep ADR-0008's Node base contract frozen). Prove
  with self-asserting tests; stop at any new ADR-level decision.
- **Latest: FIX H3 mixer bus-Return stereo width (ADR-0014) is green locally — both review defects cleared.**
  Cleared the second latent defect from the adversarial review. ADR-0014 never specified a Bus Return's
  channel width, so the earlier Send/Return projection summed Send taps into a **mono** `SumNode` wired
  straight into the stereo master, making a `Send->Bus->Return` audible in the master's LEFT channel only
  (a mono producer fills only channel 0 of a stereo consumer; `SumNode` skips the null channel-1 pointer).
  Dan chose (multiple-choice) the recommended fix: a Bus Return is stereo and centred, mirroring the Track
  chain. Wrote the decision into ADR-0014 first (`docs(adr)` commit `e3f9448`), then each Bus Return now
  widens to centred stereo through its own `PanNode -> MeterNode` (the Bus `SumNode` still sums mono Send
  taps; the Return centres at the equal-power ×0.707 gain like a Track), default centre, pannable later;
  `MixerBusProjection` gained `panNodeId`/`meterNodeId`/`pan` and the build validates the Return pan.
  Made the mixer test harness **stereo-aware**: a test-only `CompiledGraph::debugMasterChannel` exposes the
  master's channel 1 (`process()` computes it into the pool but only ever surfaced channel 0 — which is why
  CI was blind to this); `render()` now captures BOTH channels; every Send/Return test asserts L and R, the
  scalar test proves a hard-left pan silences R, and a dedicated regression guard proves a Send->Bus->Return
  is centred and non-zero in both channels (not left-only). No solo/mute policy, Sidechain, Project/
  persistence schema, plugin-host, golden, or `[[clang::nonblocking]]` edits — only the bus-Return projection,
  a test-only debug accessor, and the ADR addendum. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (159/159). Remote CI is pending
  until this fix + status tip is pushed (gh auth unavailable in this shell, so the green check needs a glance).
  **Next:** resume Codex's queued H3 loop — REVIEW/FIX H3 mixer policy ADR-0014: verify it against `STATUS.md`,
  ADR-0007/0008/0009/0013, the H3 plan/deepening notes, `CONTEXT.md`, and the current `MixerGraphProjection`
  / `GraphBuilder` / `CompiledGraph` / `Node` contracts (the bus-Return addendum is now part of it); fix only
  proven doc defects. Then WORKER: implement the ADR-0014 mixer policy — derive the post-compile mute mask
  from mute / SIP-solo / solo-safe state (no graph rewrite on a solo toggle; the audio thread only reads the
  published mask) and Sidechain input pins as real compiler-visible graph inputs with PDC, each proven with
  self-asserting tests. Stop at any new ADR-level decision.
- **Latest: FIX H3 mixer gain-validator tautology + FaderNode SetGain clamp is green locally.**
  Cleared the first of two latent defects an adversarial review of `435d320..ba235d1` found in the headless
  `MixerGraphProjection` (only tests call it, so neither was user-reachable, and both were invisible to the
  prior tests, which used sane values). `mixerGainIsValid`'s `gain <= float max` upper bound was a tautology
  that rejected nothing, so a finite-but-absurd gain (e.g. 1e30) passed validation, reached
  `FaderNode::processRange` (`x[i] *= g`), and produced inf/NaN; and `FaderNode::setTargetGain` stored the
  raw value with no clamp (unlike `PanNode::setPan`), so a runtime `applySetGain` (RT-hot) could bypass the
  build-time gate. Bounded the validator to FaderNode's shared `kMaxLinearGain` ceiling (+60 dB / 1000x) and
  added a defensive RT-safe clamp in `setTargetGain` (non-finite -> silence, finite -> [0, ceiling]). New
  self-asserting coverage proves the validator rejects non-finite/out-of-range/absurd gain, that build
  rejects a 1e30 track gain, and that a runtime SetGain of 1e30 against a 1e20 source stays finite (no inf
  reaches the output) and settles at the clamped ceiling. No ADR, golden, schema, plugin-host,
  Sidechain/solo-policy, or `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (158/158). Remote CI is pending
  until this fix + status tip is pushed.
  **Next:** FIX H3 mixer bus-Return stereo width (the second latent defect): a Bus Return is built mono
  (`SumNode(..., 1)`) and wired into the stereo master, so a Send->Bus->Return is audible in the master's
  LEFT channel only (the right is silent); a mono signal into a stereo master must be centered, not
  hard-left. This is ADR-level — ADR-0014 never specifies Bus-Return channel width — so the decision is
  being surfaced to Dan before the code fix + stereo-aware test harness. Then resume the queued REVIEW/FIX
  H3 mixer policy ADR-0014 -> WORKER implement-the-policy loop.
- **Latest: WORKER H3 mixer policy ADR is green locally.**
  Added `docs/adr/0014-mixer-policy-solo-mute-sidechain.md` to lock the remaining H3 mixer policy before
  implementation code: SIP solo is the H3 solo mode (PFL/AFL deferred to a later monitor bus), explicit
  Mute wins over Solo and Solo-safe, solo-safe protects a Track/Bus Return only from solo-induced muting,
  and solo-safe Returns do not open unrelated source Sends into the soloed mix. Sidechain input pins are
  non-audible, ordered auxiliary inputs on sidechain-capable Nodes/PluginNodes; their edges must be
  visible to GraphBuilder before topo/PDC/buffer-liveness analysis, keep ADR-0008's `Node` base contract
  frozen, converge through explicit `SumNode` / Bus fan-in when multiple sources feed one pin, and carry
  Event/automation offsets with the same per-path PDC as audio. Updated `docs/adr/README.md` and
  `CONTEXT.md` for the new Mute / Solo / SIP solo / Solo-safe vocabulary and Sidechain input-pin
  wording. No mixer implementation code, Project or persistence schema shape, plugin-host code, scanner
  code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden edits, broad
  graph rewiring, sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]` edits were
  made. Local gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` passed (155/155). Remote CI is pending until this worker/status tip is pushed.
  **Next:** REVIEW/FIX H3 mixer policy ADR: verify ADR-0014 against `STATUS.md`, ADR-0007, ADR-0008,
  ADR-0009, ADR-0010, ADR-0011, ADR-0013, the H3 plan/roadmap/deepening notes, `CONTEXT.md`, and current
  `MixerGraphProjection` / `GraphBuilder` / `CompiledGraph` / `Node` contracts. Fix only proven doc
  defects; do not write mixer implementation code, Project or persistence schema shape, plugin-host code,
  scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden edits,
  broad graph rewiring, sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]` edits.
  Run the documented gate, update `STATUS.md`, commit/push, check CI, then create the next WORKER thread
  from `STATUS.md` if green.
- **Latest: REVIEW/FIX H3 mixer Send/Return graph-edge foundation found no defects.**
  Reviewed worker commit `14d2a1b` plus the status-only closeout `e2f1d36` against `STATUS.md`,
  ADR-0007, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0013, the H3 plan/roadmap/deepening notes,
  and the current `MixerGraphProjection` / `GraphBuilder` / `CompiledGraph` / `Node` contracts.
  The implementation stays in the intended headless/control-thread-only slice: Send is a graph edge to
  a Bus `SumNode`, `PreFader` / `PostFader` taps are relative to `FaderNode`, each Bus Return feeds the
  master bus, and PDC/duplicate/missing/latency validation remain owned by `GraphBuilder`. No proven
  production-code defect was found, so this is a status-only closeout. Focused local check:
  `ctest --preset ci -R "Mixer projection" --output-on-failure` passed (9/9). Full local gate via
  documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci`
  passed (155/155). Remote CI is pending until this review/fix status commit is pushed. No Sidechain
  input-pin semantics, SIP solo/solo-safe policy, solo/mute policy, Project or persistence schema shape,
  plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4
  work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth, or
  `[[clang::nonblocking]]` edits were made.
  **Next:** WORKER H3 mixer policy ADR for the remaining mixer graph semantics: write the narrow
  decision record needed before coding solo/mute/SIP solo-safe behavior and Sidechain input-pin
  semantics. Verify it against `STATUS.md`, ADR-0007, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0013,
  the H3 plan/roadmap/deepening notes, `CONTEXT.md`, and current `CompiledGraph` mute-mask /
  `GraphBuilder` PDC contracts. Update `docs/adr/README.md` and `CONTEXT.md` only if the ADR changes
  shared terms. Do not write mixer implementation code, Project or persistence schema shape, plugin-host
  code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden
  edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]`
  edits. Run the documented gate, update `STATUS.md`, commit/push, check CI, then create the follow-up
  REVIEW/FIX thread if green.
- **Latest: WORKER H3 mixer Send/Return graph-edge foundation is green locally.**
  Extended the pure headless `MixerGraphProjection` helper with the plan/ADR-0007 Send/Return graph
  shape only: `MixerSendProjection` is an edge to a Bus `SumNode`, `PreFader` / `PostFader` chooses
  the tap relative to the `FaderNode`, and each Bus `SumNode` Return feeds the master bus. `GraphBuilder`
  still owns duplicate/missing/latency validation, PDC, buffer layout, canonical bus binding, and frozen
  Node preparation. New `YesDawMixerProjectionCheck` coverage proves pre/post-Fader tap behavior,
  deterministic Bus Return summing across declaration order, PDC alignment through Return convergence
  with a test-only latency/impulse source, and missing-bus Send rejection before graph build. No
  Sidechain input-pin semantics, SIP solo/solo-safe policy, solo/mute policy, Project or persistence
  schema shape, plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC,
  export UX, H4 work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth,
  or `[[clang::nonblocking]]` edits were made. The prior review/fix closeout commit `990e2ca` is green
  in remote CI run `28183565440` across Windows, Linux, macOS, RTSan, and TSan. Local gate via documented
  Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass
  (155/155). Remote CI run `28184654241` for worker commit `14d2a1b` is green across Windows, Linux,
  macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H3 mixer Send/Return graph-edge foundation: verify the worker implementation
  against `STATUS.md`, ADR-0007, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0013, the H3 plan/roadmap/
  deepening notes, and current `MixerGraphProjection` / `GraphBuilder` / `CompiledGraph` / `Node`
  contracts. Fix only proven defects; keep it headless/control-thread-only and do not start Sidechain
  input-pin semantics, SIP solo/solo-safe policy, solo/mute policy, Project or persistence schema shape,
  plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4
  work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth, or
  `[[clang::nonblocking]]` edits. Run the documented gate, update `STATUS.md`, commit/push, check CI,
  then create the next WORKER thread from `STATUS.md` if green.
- **Latest: WORKER H3 mixer graph projection foundation is green locally.**
  Added a pure headless `MixerGraphProjection` helper that projects mono track sources into the existing
  `FaderNode -> PanNode -> MeterNode -> SumNode(master bus) -> MasterNode` graph shape and hands the
  result to `GraphBuilder`. The slice stays control-thread-only and uses the frozen `Node` /
  `CompiledGraph` contracts; it does not add Send/Return/Sidechain semantics, solo/mute policy, Project
  or persistence schema shape, plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process
  runtime IPC, export UX, H4 work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived
  Project truth, or `[[clang::nonblocking]]` edits. New `YesDawMixerProjectionCheck` coverage proves
  empty mixer silence, two-track fader/pan/meter-to-master summing, existing `CompiledGraph` SetGain /
  SetPan scalar routing, and rejection of non-mono sources plus invalid gain/pan values before graph
  build. The previous plugin-state proof-gate commit `a79c432` is green in remote CI run `28182281472`.
  Local gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (151/151). Remote CI run `28182841578` for worker commit `ddeaea9` is
  green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H3 mixer graph projection foundation: verify the worker implementation against
  `STATUS.md`, ADR-0007, ADR-0008, ADR-0011, ADR-0013, the H3 plan/roadmap/deepening notes, and current
  `GraphBuilder` / `CompiledGraph` / `Node` contracts. Fix only proven defects; keep it as a headless
  mixer projection foundation and do not start Send/Return/Sidechain policy, solo/mute policy, Project
  or persistence schema shape, plugin-host code, scanner code, plugin UI, CLAP loading,
  out-of-process runtime IPC, export UX, H4 work, golden edits, broad graph rewiring,
  sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]` edits. Run the documented
  gate, update `STATUS.md`, commit/push, check CI, then create the next WORKER thread from `STATUS.md`
  if green.
- **Latest: REVIEW/FIX H3 plugin state chunk storage/header proof gate is green locally.**
  Reviewed the current `main` implementation (worker commit `85a29a7`, hardening commit `9d26b7b`,
  and status closeout commit `459e507`) against `STATUS.md`, ADR-0013, ADR-0012, ADR-0011, the H3
  plan/roadmap/deepening notes, and current persistence contracts. Found one narrow mechanical proof
  gap, not a production-code defect: the restore path rejected non-canonical SQLite storage classes for
  plugin-state headers, but the persistence gate only proved `chunk_len`/`crc32` corruption fallback.
  Added `YesDawPersistenceCheck` coverage that mutates plugin-state header fields to non-canonical
  SQLite storage classes plus embedded-NUL format text, then proves restore reports
  `Unreadable`/default-state, hands no bytes to plugin restore, and leaves the stored opaque bytes in
  place. The surface stays storage/header-only, uses the persistent 16-byte node Entity ID as the key,
  stores opaque plugin bytes with host-owned metadata, computes CRC32 at the bundle boundary, validates
  SQLite storage classes plus `chunk_len` and `crc32` before restore handoff, preserves unreadable bytes
  in place, reports missing/corrupt chunks as default-state restore outcomes, and returns VST3 component
  state before controller state. The hardening commit `9d26b7b` is green in remote CI run `28181189197`
  across Windows, Linux, macOS, RTSan, and TSan. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (146/146). No plugin-host
  code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden
  edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]`
  edits were made. Remote CI is pending until this proof-gate review/fix commit is pushed.
  **Next:** WORKER H3 mixer graph projection foundation: add the smallest headless mixer projection over
  the frozen graph/Node contracts, using the existing Fader/Pan/Sum/Send/Return/Meter building blocks
  where they already exist and stopping if a new ADR-level mixer decision appears. Prove it with
  self-asserting tests only. Keep it headless and out of plugin-host code, scanner code, plugin UI,
  CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden edits, broad graph rewiring,
  sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]` edits. Run the documented
  gate, update `STATUS.md`, commit/push, check CI, then create the follow-up REVIEW/FIX thread if green.
- **Latest: WORKER H3 plugin state chunk storage/header gate is green locally.**
  Added the smallest headless persistence surface for ADR-0013 plugin-state chunks on top of the
  existing `plugin_state_chunks` table reservation. `ProjectBundleDb` now writes opaque plugin bytes
  with host-owned metadata (`format`, `plugin_uid`, `plugin_version`, `chunk_kind`, `chunk_len`,
  `crc32`), computes and stores CRC32 at the bundle boundary, reads chunks only after validating
  `chunk_len` and `crc32`, preserves corrupt bytes in place, and reports missing/corrupt chunks as
  default-state restore outcomes instead of handing unreadable bytes to a plugin. The storage/API
  boundary uses the persistent 16-byte node Entity ID as the key and returns VST3 component state before
  VST3 controller state. New `YesDawPersistenceCheck` coverage proves opaque-byte/metadata storage,
  persistent Entity ID keying even when two nodes share the same low runtime-ID-shaped bits, header
  corruption fallback without byte mutation, missing-chunk fallback, and VST3 restore ordering. No
  plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4
  work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth, or
  `[[clang::nonblocking]]` edits were made. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (145/145). Remote CI is
  pending until this worker commit is pushed.
  **Next:** REVIEW/FIX H3 plugin state chunk storage/header gate: verify the worker implementation
  against `STATUS.md`, ADR-0013, ADR-0012, ADR-0011, the H3 plan/roadmap/deepening notes, and current
  persistence contracts. Fix only proven defects; keep it storage/header-only and do not start
  plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4
  work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth, or
  `[[clang::nonblocking]]` edits. Run the documented gate, update `STATUS.md`, commit/push, check CI,
  then create the next WORKER thread from `STATUS.md` if green.
- **Latest: REVIEW/FIX H3 ADR-0013 plugin state + hosting isolation is green locally.**
  Reviewed ADR-0013 against `STATUS.md`, the H3 plan/roadmap/deepening notes, ADR index/template,
  ADR-0002/0006/0007/0008/0009/0012, `CONTEXT.md`, and the current Node / EventStream / Runtime /
  CompiledGraph / GraphBuilder / ProjectBundle / CMake/test contracts. Found one narrow documentation
  defect and fixed it: ADR-0013 now explicitly says `plugin_state_chunks.node_id` is the persistent
  16-byte node Entity ID stored in the bundle, not the runtime 32-bit `NodeId` used inside
  `CompiledGraph`; `CONTEXT.md` mirrors that glossary-level wording for Plugin state chunk. No
  plugin-host code, scanner code, plugin UI, CLAP loading, export UX, H4 work, golden edits, broad
  graph rewiring, schema implementation changes, sampled/pixel/snapped/derived Project truth, or
  `[[clang::nonblocking]]` edits were made. The ADR worker commit `3b00db8` is green in remote CI run
  `28151834609` across Windows, Linux, macOS, RTSan, and TSan. Local gate via documented Windows
  DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (142/142).
  Remote CI is pending until this review/fix commit is pushed.
  **Next:** WORKER H3 plugin state chunk storage/header gate: add the smallest headless persistence
  surface and self-asserting tests for ADR-0013 plugin-state chunks on top of the existing
  `plugin_state_chunks` reservation. Prove the bundle stores opaque bytes with host-owned metadata,
  uses the persistent 16-byte node Entity ID as the storage key, validates `chunk_len` + `crc32` before
  restore handoff, preserves original bytes, restores VST3 component before controller ordering at the
  storage/API boundary, and degrades corrupt/missing chunks to an unreadable/default-state result
  without crashing. Keep it storage/header-only: do not start plugin-host code, scanner code, plugin
  UI, CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden edits, broad graph rewiring,
  sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]` edits. Run the documented
  gate, update `STATUS.md`, commit/push, check CI, then create the follow-up REVIEW/FIX thread if green.
- **Latest: WORKER H3 ADR-0013 plugin state + hosting isolation is green locally.**
  Added `docs/adr/0013-plugin-state-and-hosting-isolation.md` to lock plugin state as opaque
  host-wrapped chunks, VST3 + AU first then CLAP, out-of-process/sandboxed hosting from the start,
  `PluginNode` as the IPC proxy over serializable audio/Event buffers, one-Block nonblocking
  fail-open behavior, scanner watchdog/blacklist/cache behavior, and pluginval / `auval` /
  host-isolation gates. Updated `docs/adr/README.md` so engine decisions #11 and #12 are recorded by
  ADR-0013, and updated `CONTEXT.md` for the new shared plugin-hosting vocabulary. No plugin-host code,
  scanner code, plugin UI, CLAP loading, export UX, H4 work, golden edits, broad graph rewiring, schema
  implementation changes, sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]`
  edits were made. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (142/142). Remote CI is pending until this ADR
  worker commit is pushed.
  **Next:** REVIEW/FIX H3 ADR-0013 plugin state + hosting isolation: verify ADR-0013 against
  `STATUS.md`, the H3 plan/roadmap/deepening notes, ADR index, and current contracts. Fix only proven
  doc defects; do not start plugin-host code, scanner code, plugin UI, CLAP loading, export UX, H4
  work, golden edits, broad graph rewiring, schema implementation changes, sampled/pixel/snapped/derived
  Project truth, or `[[clang::nonblocking]]` edits. Run the documented gate, update `STATUS.md`,
  commit/push, check CI, then create the next WORKER thread from `STATUS.md` if green.
- **Latest: Dan approved the H2->H3 horizon boundary; H3 loop handoff is being opened.**
  H2's mechanical exit gates are green locally and in remote CI: command/diff edit-sequence undo/redo
  returns the live `Project` to bit-identical states, split-with-crossfade Project render matches
  Runtime/offline graph paths, and kill-mid-import bundle recovery is DB/filesystem consistent with
  committed Asset hash verification and no orphan audio files. Remote CI run `28146655906` for H2
  closeout commit `435d320` is green across Windows, Linux, macOS, RTSan, and TSan. Dan explicitly
  approved advancing to H3. H3 code must not start before its pending ADR is written: ADR index decision
  #11 plugin state as opaque chunks and #12 out-of-process/sandboxed hosting both point to ADR-0013.
  This status-only parent handoff passed the documented local Windows DevShell gate:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` (142/142). Remote CI is
  pending until this handoff commit is pushed.
  **Next:** WORKER H3 ADR-0013 plugin state + hosting isolation: write the narrow ADR only, covering
  opaque plugin-state chunks, VST3+AU first then CLAP, `PluginNode` as an out-of-process IPC proxy,
  one-block nonblocking fail-open audio behavior, scanner crash/hang blacklist behavior, pluginval /
  `auval` gates, and the host-isolation test implied by the H3 exit criterion. Update the ADR index and
  `CONTEXT.md` only if the ADR changes shared terms. Do not write plugin-host code, scanner code,
  plugin UI, CLAP loading, export UX, H4 work, golden edits, broad graph rewiring, schema semantics
  beyond ADR wording, or `[[clang::nonblocking]]` edits. After a green ADR worker commit, that worker
  must create the follow-up REVIEW/FIX H3 ADR-0013 thread. The review/fix thread must verify the ADR
  against the plan, deepening notes, ADR index, and existing code contracts, then create the next worker
  only if the review is green. Continue worker -> review/fix -> worker until H3 exit gates are green,
  then hard-stop for Dan's next horizon-boundary review.
- **Latest: WORKER H2 exit-gate closeout / CI-truth pass is green locally.**
  Verified from current repo truth that the H2 exit gates are represented by self-asserting tests:
  command/diff edit-sequence undo/redo returns the live `Project` to the bit-identical original value
  and redoes to the bit-identical edited value (`YesDawProjectCheck`); split-with-crossfade Project
  rendering is green through both Runtime and offline graph paths with exact adjacent Tick/source-frame
  windows, `evaluateClipGainEnvelope`-derived expected samples, and unchanged Asset/Project truth
  (`YesDawBundleRenderCheck`); and kill-mid-import bundle recovery is green via open-time
  DB/filesystem reconciliation, committed Asset hash verification, stale intent cleanup, and no orphan
  audio files (`YesDawPersistenceCheck`). Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (142/142). Latest pushed
  remote CI before this closeout, run `28146299670` for `9fe162f`, is green across Windows, Linux,
  macOS, RTSan, and TSan; this status-only closeout commit will be pushed and checked before handoff.
  No H3, UI shell, export UX, plugin hosting, ADR edits, roadmap edits, golden edits, broad render
  rewiring, schema semantics, sampled/pixel/snapped/derived values as Project truth, or
  `[[clang::nonblocking]]` edits were made.
  **Next:** Dan's H2 horizon-boundary review. Only Dan advances H2->H3; do not create an H3 worker
  unless `STATUS.md` is explicitly changed to say so.
- **Latest: REVIEW/FIX H2 split-with-crossfade RT/offline render gate found no defects.**
  Reviewed worker commit `63c855a` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  The gate stays headless and narrow: it builds a Project through the current Clip edit helpers
  (`setClipGain`, `splitClip`, `setClipFades`), asserts exact adjacent Tick and source-frame windows
  before and after bundle reopen, uses `evaluateClipGainEnvelope` for expected decoded Clip samples and
  crossfade-compatible midpoint gains, and compares the same valid Project through Runtime and offline
  graph paths. Assets and Project truth remain metadata-only: unchanged Asset rows, unchanged Clip /
  Project values after write/render, and unchanged bundled Asset bytes. No SQLite undo journaling,
  autosave durability semantics, UI gesture timing, export UX, plugin hosting, H3 work, ADR edits,
  roadmap edits, golden edits, waveform cache changes, broad render rewiring, schema semantics,
  sampled/pixel/snapped/derived values as Project truth, or `[[clang::nonblocking]]` edits. Local gate
  via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (142/142). Remote CI run `28145624290` for worker commit `63c855a` and run
  `28145828642` for pre-review status tip `c194ff4` are green across Windows, Linux, macOS, RTSan, and
  TSan. Remote CI is pending until this status-only review/fix commit is pushed.
  **Next:** WORKER H2 exit-gate closeout / CI-truth pass: verify from repo truth that the H2 exit gates
  are represented by self-asserting tests and latest pushed CI: command/diff edit-sequence undo/redo
  returns the Project bit-identical, split-with-crossfade Project RT/offline render is green, and
  kill-mid-import bundle consistency is green with assets hash-verified/no orphans. Do not start H3, UI
  shell, export UX, plugin hosting, ADR edits, roadmap edits, golden edits, broad render rewiring,
  schema semantics, sampled/pixel/snapped/derived values as Project truth, or `[[clang::nonblocking]]`
  edits. If the H2 exit gates are green, update `STATUS.md` for Dan's horizon-boundary review and stop;
  only Dan advances H2->H3.
- **Latest: REVIEW/FIX H2 edit-sequence undo/redo property gate found no defects.**
  Reviewed worker commit `af31e8e` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  The deterministic headless sequence generator stays Project-local and command+diff only: it drives
  `moveClip`, `trimClip`, `splitClip`, `setClipGain`, and `setClipFades` through explicit
  `ProjectUndoStack` transaction-group boundaries, accepted/rejected group boundaries, grouped
  compatible coalescing, ungrouped same-verb separation, split-plus-right-Clip follow-up edits, and
  invalid gain/source-window commands. The gate proves apply-all / undo-all returns the live in-memory
  `Project` to the bit-identical original value and redo-all returns it to the bit-identical edited
  value. The slice remains command+diff and Project-local only: no SQLite undo journaling, autosave
  durability semantics, UI gesture timing, export, plugin hosting, H3 work, ADR edits, roadmap edits,
  golden edits, waveform cache changes, broad render rewiring, schema semantics,
  sampled/pixel/snapped/derived values as Project truth, or `[[clang::nonblocking]]` edits. Local gate
  via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (141/141). Remote CI is pending until this status-only review/fix commit is
  pushed.
  **Next:** WORKER H2 split-with-crossfade RT/offline render gate: add the smallest self-asserting
  headless Project render gate for a split Clip with crossfade-compatible existing gain/fade metadata,
  proving the same valid Project renders identically through RT playback and offline Render while
  Assets remain immutable and Project truth stays metadata-only. Use current Clip edit helpers and
  existing envelope evaluation where possible. Keep sampled/pixel/snapped/derived values out of Project
  truth. Do not expand into SQLite undo journaling, autosave durability semantics, UI gesture timing,
  export UX, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes,
  broad render rewiring, schema semantics, or `[[clang::nonblocking]]` edits. If crossfade
  curve/shared-ramp representation, timeline projection semantics, export scope, undo persistence, or
  any ADR-level decision rises, stop and report.
- **Latest: WORKER H2 edit-sequence undo/redo property gate is green locally.**
  Added the smallest deterministic headless sequence generator over the current Project-local Clip edit
  command surface and explicit `ProjectUndoStack` transaction-group boundaries. The new
  `YesDawProjectCheck` gate drives `moveClip`, `trimClip`, `splitClip`, `setClipGain`, and
  `setClipFades` through accepted and rejected group boundaries, grouped compatible coalescing,
  ungrouped same-verb separation, split-plus-right-Clip follow-up edits, and invalid gain/source-window
  commands. It proves apply-all / undo-all returns the live in-memory `Project` to the bit-identical
  original value and redo-all returns it to the bit-identical edited value. The slice stays command+diff
  and Project-local only: no SQLite undo journaling, autosave durability semantics, UI gesture timing,
  export, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, broad
  render rewiring, schema semantics, sampled/pixel/snapped/derived values as Project truth, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (141/141). Remote CI run `28144622776` for
  worker commit `af31e8e` is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 edit-sequence undo/redo property gate: review the worker gate against
  `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2 plan/deepening notes, and the current Time /
  Project / ProjectBundle / render and persistence tests. Verify the sequence generator is only a
  Project-local command+diff proof over current helpers and explicit group boundaries, that invalid
  command handling and grouping semantics are explicit, and that apply/undo-all and redo-all prove
  bit-identical live `Project` values. Do not start SQLite undo journaling, autosave durability
  semantics, UI gesture timing, export, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits,
  waveform cache changes, broad render rewiring, schema semantics, sampled/pixel/snapped/derived values
  as Project truth, or `[[clang::nonblocking]]` edits.
- **Latest: REVIEW/FIX H2 undo transaction grouping/property gate foundation is green locally.**
  Reviewed worker commit `3670bd8` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  Found and fixed one narrow mechanical proof gap: `YesDawProjectCheck` now directly proves grouped
  same-verb/different-Clip edits stay separate, while compatible `trimClip` and `setClipFades`
  sequences coalesce inside an explicit transaction group and still undo/redo back to bit-identical
  live `Project` values. The implementation stays explicit and headless: only compatible consecutive
  same-verb/same-Clip one-row diffs coalesce inside an active group; `splitClip`, unrelated verbs,
  unrelated targets, and ungrouped edits stay separate. The slice remains command+diff and Project-local
  only: no SQLite undo journaling, autosave durability semantics, UI gesture timing, export, plugin
  hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, broad render
  rewiring, schema semantics, sampled/pixel/snapped/derived values as Project truth, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (140/140). Remote CI run `28143357400` for
  worker commit `3670bd8` is green. Remote CI run `28143828792` for review/fix commit `385bb36`
  is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** WORKER H2 edit-sequence undo/redo property gate: add the smallest self-asserting headless
  sequence generator over the current Clip edit helpers and explicit transaction groups, proving
  apply/undo-all returns the live in-memory `Project` to the bit-identical original and redo-all returns
  it to the edited value. Keep it Project-local command+diff only; no SQLite undo journaling, autosave
  durability semantics, UI gesture timing, export, plugin hosting, H3 work, ADR edits, roadmap edits,
  golden edits, waveform cache changes, broad render rewiring, schema semantics, sampled, pixel,
  snapped, or derived values as Project truth, or `[[clang::nonblocking]]` edits. If property-test framework choice,
  undo persistence/autosave semantics, coalescing semantics, crossfade curve/shared-ramp representation,
  or any ADR-level decision rises, stop and report.
- **Latest: WORKER H2 undo transaction grouping/property gate foundation is green locally.**
  Added the smallest headless transaction grouping layer on top of the live in-memory command/diff undo
  stack for the current H2 Clip edit helpers. `ProjectUndoStack` now has explicit
  `beginTransactionGroup` / `endTransactionGroup` boundaries; inside an active group, only consecutive
  one-row same-verb/same-Clip edits coalesce (`moveClip`, `trimClip`, `setClipGain`, `setClipFades`).
  `splitClip` and unrelated verbs or targets remain separate undo entries. Coalesced entries keep the
  original before row and latest after row, so undo/redo still applies exact Clip row diffs against the
  live in-memory `Project`. The slice stays command+diff and Project-local only: no SQLite undo
  journaling, autosave durability semantics, UI gesture timing, export, plugin hosting, H3 work, ADR
  edits, roadmap edits, golden edits, waveform cache changes, broad render rewiring, schema semantics,
  sampled/pixel/snapped/derived values as Project truth, or `[[clang::nonblocking]]` edits. The new
  `YesDawProjectCheck` coverage proves grouped compatible sequences coalesce to the expected undo
  depth, unrelated grouped edits stay separate, ungrouped compatible edits stay separate, and grouped
  plus ungrouped sequences undo/redo back to bit-identical `Project` values. Local gate via documented
  Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass
  (139/139). Remote CI run `28143357400` is green.
  **Next:** REVIEW/FIX H2 undo transaction grouping/property gate foundation.
- **Latest: REVIEW/FIX H2 command/diff undo/redo foundation found no defects.**
  Reviewed worker commit `8caf091` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  `ProjectEditCommand` stays a named edit intent, and `ProjectUndoStack` records exact Clip row
  before/after diffs for `moveClip`, `trimClip`, `splitClip`, `setClipGain`, and `setClipFades`.
  Undo applies the recorded before rows; redo applies the recorded after rows; invalid commands and
  mismatched live Clip rows reject without Project mutation. The slice stays live in-memory Project
  only: Assets remain immutable; SQLite undo journaling, autosave durability semantics, UI interaction,
  export, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes,
  broad render rewiring, schema semantics, sampled/pixel/snapped/derived values as Project truth, and
  `[[clang::nonblocking]]` edits are untouched. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (137/137). Remote CI run
  `28142543112` for worker commit `8caf091` is green across Windows, Linux, macOS, RTSan, and TSan.
  Remote CI is pending until this status-only review/fix commit is pushed.
  **Next:** WORKER H2 undo transaction grouping/property gate foundation.
- **Latest: WORKER H2 command/diff undo/redo foundation is green locally.**
  Added the smallest headless in-memory command/diff undo/redo surface for the current H2 Clip edit
  helpers: `moveClip`, `trimClip`, `splitClip`, `setClipGain`, and `setClipFades`. `ProjectEditCommand`
  records the named edit intent, and `ProjectUndoStack` records exact Clip row before/after diffs on
  successful commands so a live in-memory `Project` can undo back to the bit-identical original value
  and redo back to the edited value. Invalid commands and mismatched live Project state are rejected
  without mutation. The slice stays metadata-only: Assets remain immutable; SQLite undo journaling,
  autosave durability semantics, UI interaction, export, plugin hosting, H3 work, ADR edits, roadmap
  edits, golden edits, waveform cache changes, broad render rewiring, schema semantics,
  sampled/pixel/snapped/derived values as Project truth, and `[[clang::nonblocking]]` edits are
  untouched. `YesDawProjectCheck` now proves a mixed sequence of all five current Clip edit helpers can
  apply, undo to the exact original `Project`, and redo to the exact edited `Project`. Local gate via
  documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (137/137). Remote CI run `28142543112` is green across Windows, Linux,
  macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 command/diff undo/redo foundation.
- **Latest: REVIEW/FIX H2 Clip gain/fade/crossfade envelope render projection foundation found no defects.**
  Reviewed worker commit `232e384` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  The decoded Clip bundle projection applies the existing `evaluateClipGainEnvelope` result to decoded
  Clip source-window samples before RT/offline graph rendering, so existing Clip `gain`, `fadeIn`, and
  `fadeOut` metadata affects rendered samples deterministically. Project truth stays metadata-only:
  Assets and bundled bytes are unchanged; no sampled, pixel, snapped, or derived sample values are
  stored back into Project truth. Crossfade remains adjacent per-Clip envelopes over existing metadata
  only; no shared crossfade object, `curve_type`, schema semantics, undo/redo, UI interaction, export,
  plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, broad render
  rewiring, or `[[clang::nonblocking]]` edits slipped in. Local gate via documented Windows DevShell
  flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (135/135). Remote
  CI run `28141683206` for worker commit `232e384` is green across Windows, Linux, macOS, RTSan, and
  TSan. Remote CI is pending until this status-only review/fix commit is pushed.
  **Next:** WORKER H2 command/diff undo/redo foundation.
- **Latest: WORKER H2 Clip gain/fade/crossfade envelope render projection foundation is green locally.**
  Updated `YesDawBundleRenderCheck` so the decoded Clip projection uses the existing
  `evaluateClipGainEnvelope` result before RT/offline graph rendering: existing Clip `gain`, `fadeIn`,
  and `fadeOut` metadata now affects rendered decoded samples deterministically. The gate compares
  Runtime and offline Render output against evaluator-derived expected samples and proves the previous
  constant-gain-only projection differs, so the envelope path is mechanically covered. Project truth
  stays metadata-only: Assets and bundled bytes are unchanged; no sampled, pixel, snapped, or derived
  sample values are stored back into Project truth. Crossfade remains adjacent per-Clip envelopes over
  existing metadata only; no shared-ramp representation, `curve_type`, schema semantics, undo/redo, UI
  interaction, export, plugin hosting, H3 work, ADRs, roadmap, goldens, waveform cache, or
  `[[clang::nonblocking]]` annotations were touched. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (135/135). Remote CI run
  `28141683206` for worker commit `232e384` is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 Clip gain/fade/crossfade envelope render projection foundation.
- **Latest: REVIEW/FIX H2 Clip gain/fade/crossfade envelope evaluation foundation found no defects.**
  Reviewed worker commit `e4bb7ae` against H2 scope, ADR-0010, ADR-0011, ADR-0012, the H2 deepening
  notes, and the current Time / Project / ProjectBundle / render and persistence tests. The evaluator
  stays pure derived evaluation over one Clip's existing `gain`, `fadeIn`, and `fadeOut` metadata at a
  Clip-local Tick: it returns either a finite scalar or an invalid result, and stores nothing back into
  Project truth. Assets, source-frame windows, timeline Tick metadata, `timeBase`, schema, undo/redo,
  UI interaction, export, plugin hosting, H3 work, ADRs, roadmap, goldens, waveform cache, and
  `[[clang::nonblocking]]` annotations are untouched. Invalid storage-unsafe Clip metadata and
  out-of-Clip positions are rejected. Adjacent per-Clip midpoint compatibility is supported only by the
  current ADR/deepening-note envelope shape; no shared-ramp representation, `curve_type`, or schema
  semantics were invented. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (135/135). Remote CI run `28140746988` for
  worker commit `e4bb7ae` is green across Windows, Linux, macOS, RTSan, and TSan. Remote CI is pending
  until this status-only review/fix commit is pushed.
  **Next:** WORKER H2 Clip gain/fade/crossfade envelope render projection foundation: use the existing
  `evaluateClipGainEnvelope` result in the smallest headless RT/offline Project projection gate for
  decoded Clips, so existing Clip `gain`, `fadeIn`, and `fadeOut` affect rendered samples
  deterministically without becoming Project truth. Keep Project truth metadata-only, Assets immutable,
  and sampled/pixel/snapped/derived sample values derived rather than stored. Do not invent a shared
  crossfade object, `curve_type`, schema semantics, undo/redo, UI interaction, export, plugin hosting,
  H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, or `[[clang::nonblocking]]`
  edits; if curve/shared-ramp representation semantics rise to ADR level, stop and report.
- **Latest: WORKER H2 Clip gain/fade/crossfade envelope evaluation foundation is green.**
  Added the smallest headless derived evaluator for existing Clip envelope metadata:
  `evaluateClipGainEnvelope` derives a finite gain scalar from a Clip-local Tick using only existing
  `gain`, `fadeIn`, and `fadeOut` fields plus the current equal-power fade polynomial. Project truth
  stays metadata-only: no sampled, pixel, snapped, or derived sample values are stored, and Assets,
  source-frame windows, timeline Tick metadata, `timeBase`, schema, undo/redo, UI interaction, export,
  plugin hosting, H3 work, ADRs, roadmap, goldens, waveform cache, and `[[clang::nonblocking]]`
  annotations are untouched. Crossfade remains adjacent per-Clip envelopes over existing metadata only;
  no shared-ramp representation, `curve_type`, or schema semantics were invented. `YesDawProjectCheck`
  now proves equal-power fade-in/fade-out evaluation, adjacent per-Clip midpoint compatibility, invalid
  Clip envelope metadata rejection, out-of-Clip position rejection, and no Project mutation. Local gate
  via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (135/135). Remote CI run `28140746988` for worker commit `e4bb7ae` is green
  across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 Clip gain/fade/crossfade envelope evaluation foundation.
- **Latest: REVIEW/FIX H2 Clip gain/fade/crossfade metadata foundation found no defects.** Reviewed
  worker commit `c3819cc` against H2 scope, ADR-0010, ADR-0011, ADR-0012, the H2 deepening notes, and
  the current Time / Project / ProjectBundle / render and persistence tests. The helpers stay pure
  metadata over the existing Clip fields: `setClipGain` / `setClipFades` mutate only storage-safe
  `gain`, `fadeIn`, and `fadeOut`; Assets, timeline Tick placement, source-frame windows, `timeBase`,
  schema, sampled/pixel/snapped values, undo/redo, UI, export, plugin hosting, H3 work, ADRs, roadmap,
  goldens, waveform cache, and `[[clang::nonblocking]]` annotations are untouched. Invalid requested
  envelope values and invalid pre-existing storage-unsafe Clip metadata are rejected without Project
  mutation; the persistence proof covers exact schema v1 write/read of edited gain/fade metadata.
  Crossfade remains adjacent per-Clip envelope metadata only; no representation or curve semantics were
  invented. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (132/132). Remote CI is pending until this
  status-only review/fix commit is pushed.
  **Next:** WORKER H2 Clip gain/fade/crossfade envelope evaluation foundation: add the smallest
  headless derived evaluator/gate for existing Clip `gain`, `fadeIn`, and `fadeOut` metadata so
  RT/offline Project projection can later apply one per-Clip gain envelope. Keep Project truth
  metadata-only, Assets immutable, and sampled/pixel/snapped values derived rather than Project truth.
  Treat crossfade as adjacent Clip envelopes only if the current ADR and H2 deepening notes are
  sufficient; if curve/shared-ramp representation semantics rise to ADR level, stop and report. Do not
  start undo/redo, UI interaction, export, plugin hosting, H3 work, ADR edits, roadmap edits, golden
  edits, waveform cache changes, or `[[clang::nonblocking]]` edits.
- **Latest: WORKER H2 Clip gain/fade/crossfade metadata foundation is green.** Added the
  smallest headless Project-level edit helpers for the existing Clip envelope metadata:
  `setClipGain` and `setClipFades`. The slice stays pure metadata over the current
  Asset→Clip→Project value surface: only existing Clip `gain`, `fadeIn`, and `fadeOut` values change;
  Assets remain immutable; timeline Tick placement, source-frame windows, `timeBase`, snapped
  sample/pixel values, schema, undo/redo, UI, export, plugin hosting, H3 work, ADRs, roadmap, goldens,
  waveform cache, and `[[clang::nonblocking]]` annotations are untouched. Crossfade-specific
  representation/curve semantics were not invented; this worker only exposes the existing adjacent
  per-Clip envelope fields that current ADRs already store. `YesDawProjectCheck` proves gain/fade
  edits mutate only envelope metadata and reject invalid requested or pre-existing storage-unsafe
  Clip metadata without Project mutation. `YesDawPersistenceCheck` proves edited gain/fade metadata
  writes and reads back exactly through the current SQLite snapshot. Local gate via documented Windows
  DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (132/132).
  Remote CI run `28139588321` for worker commit `c3819cc` is green across Windows, Linux, macOS,
  RTSan, and TSan.
  **Next:** REVIEW/FIX H2 Clip gain/fade/crossfade metadata foundation.
- **Latest: REVIEW/FIX H2 Clip split/trim/move metadata foundation is green.** Reviewed worker
  commit `a081414` against H2 scope, ADR-0010, ADR-0011, ADR-0012, the H2 deepening notes, and the
  current Time / Project / ProjectBundle / render and persistence tests. Found and fixed one narrow
  storage-facing validity gap: the edit helpers now refuse to mutate a Project whose existing Clip
  metadata would be rejected by schema v1, including negative timeline lengths and invalid `timeBase`
  values. The slice stays pure metadata: only Tick timeline starts/lengths and source-frame windows are
  edited; Assets remain immutable; snapped sample/pixel values are not stored as Project truth; and
  there are no schema, undo/redo, UI, export, plugin hosting, H3, ADR, roadmap, golden,
  waveform-cache, or `[[clang::nonblocking]]` edits. `YesDawProjectCheck` now also proves these
  storage-invalid Clip metadata inputs are rejected without Project mutation. Local gate via documented
  Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass
  (131/131). Remote CI run `28138884108` for review/fix commit `189e2ac` is green across Windows,
  Linux, macOS, RTSan, and TSan.
  **Next:** WORKER H2 Clip gain/fade/crossfade metadata foundation: add the smallest headless
  Project-level edit helpers and self-asserting gates for existing Clip `gain`, `fadeIn`, and `fadeOut`
  metadata, keeping edits storage-safe, Assets immutable, and sampled/pixel/snapped values derived
  rather than Project truth. Treat crossfade as adjacent Clip envelope metadata only if the current ADR
  and H2 deepening notes are sufficient; if representation or curve semantics rise to ADR level, stop
  and report. Do not start undo/redo, UI interaction, export, plugin hosting, H3 work, ADR edits,
  roadmap edits, golden edits, waveform cache changes, or `[[clang::nonblocking]]` edits.
- **Latest: WORKER H2 Clip split/trim/move metadata foundation is green locally.** Added the smallest
  headless Project-level edit helpers over the existing Asset→Clip value surface: `splitClip`,
  `trimClip`, and `moveClip`. The slice stays pure metadata: only Tick timeline starts/lengths and
  existing source-frame windows change; Assets remain immutable; snapped sample/pixel values are not
  stored as Project truth; and there are no schema, undo/redo, UI, export, plugin hosting, H3, ADR,
  roadmap, golden, waveform-cache, or `[[clang::nonblocking]]` edits. `YesDawProjectCheck` proves exact
  split adjacency (`right.srcOffset == left.srcOffset + left.srcLen`), exact unsnapped Tick placement,
  trim/move metadata preservation, and invalid-input rejection without Project mutation.
  `YesDawPersistenceCheck` proves edited Clip metadata writes and reads back exactly through the current
  SQLite snapshot. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (131/131). Remote CI run `28136942439` for
  worker commit `a081414` is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 Clip split/trim/move metadata foundation.
- **Latest: REVIEW/FIX H2 snap/grid tick math foundation found no defects.** Reviewed worker commit
  `f7975bb` against H2 scope, ADR-0010, the H2 deepening notes, and the current Time / Project /
  timeline-layout tests. The slice stays headless and narrow: `SnapGrid`, `snapTick`,
  `gridIndexForTick`, and `tickForGridIndex` are pure integer Tick/grid math; invalid grids are
  rejected; overflow is refused; snapped Tick↔grid-index round trips are exact and stable; and Project
  schema/persistence/timeline layout remain untouched, so no snapped sample or pixel values are stored
  as canonical Project truth. No Clip editing operations, undo/redo, UI, export, plugin hosting, H3
  work, ADR edits, roadmap edits, golden edits, waveform cache changes, or `[[clang::nonblocking]]`
  edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (127/127). Remote CI run `28135729287` for
  worker commit `f7975bb` and run `28135936744` for pre-review status commit `bb49b73` are green across
  Windows, Linux, macOS, RTSan, and TSan.
  **Next:** WORKER H2 Clip split/trim/move metadata foundation: add the smallest headless Project-level
  edit operations over the existing Asset→Clip value surface, keeping edits as pure metadata with Tick
  timeline positions and existing source-frame windows. Do not start gain/fade/crossfade, undo/redo, UI,
  export, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, or
  `[[clang::nonblocking]]` edits; if operation semantics rise to ADR level, stop and report.
- **Latest: WORKER H2 snap/grid tick math foundation is green.** Added the smallest headless
  integer snap/grid surface to the ADR-0010 time layer: `SnapGrid`, `snapTick`, exact grid-index
  readback, and checked grid-index→Tick derivation. Snapped values remain derived from Tick/grid inputs;
  no Project schema, persistence, Clip editing operations, undo/redo, UI, export, plugin hosting, H3
  work, ADR edits, roadmap edits, golden edits, waveform cache changes, or `[[clang::nonblocking]]`
  edits. `YesDawTimeCheck` now proves deterministic nearest-grid integer behavior, stable/idempotent
  snapping, exact snapped Tick↔grid-index round trips, invalid-grid rejection, and overflow refusal.
  Local gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (127/127). Remote CI run `28135729287` for worker commit `f7975bb` is green
  across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 snap/grid tick math foundation.
- **Latest: REVIEW/FIX H2 waveform peak-cache foundation is green locally.** Reviewed worker commit
  `fa62e3b` against H2 scope, ADR-0011, ADR-0012, the H2 deepening notes, and the current
  `ProjectBundleDb` / `Asset` / `Project` / bundle decode tests. Found no implementation defect:
  `WaveformPeakCache` is derived Project-adjacent state under `peaks/<hash>.ypeaks`, built from decoded
  Asset samples off the audio hot path, and delete/regenerate leaves canonical Project truth unchanged.
  Fixed one narrow mechanical proof gap by extending `YesDawBundleRenderCheck` so the untrusted peak
  parser now rejects wrong stored content hashes, truncated payloads, and NaN payloads in addition to a
  corrupt header. No Clip editing operations, undo/redo, UI, export, plugin hosting, H3 work, ADR edits,
  roadmap edits, golden edits, or `[[clang::nonblocking]]` edits. Local gate via documented Windows
  DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (124/124).
  Remote CI run `28134965007` for review commit `9eb0c6f` is green across Windows, Linux, macOS, RTSan,
  and TSan.
  **Next:** WORKER H2 snap/grid tick math foundation: add the smallest headless integer `snapTick` /
  grid round-trip gate for H2, keeping snapped values derived rather than Project truth. Do not start
  Clip editing operations, undo/redo, UI, export, plugin hosting, H3 work, ADR edits, roadmap edits,
  golden edits, or `[[clang::nonblocking]]` edits.
- **Latest: WORKER H2 waveform peak-cache foundation is green locally.** Added the smallest headless
  derived peak-cache surface for bundled Assets: `WaveformPeakCache` builds deterministic min/max+RMS
  tiers from decoded Asset samples, folds higher tiers 16:1, stores/loads a content-hash-keyed
  `peaks/<hash>.ypeaks` file, and rejects invalid cache files by header/hash/tier-shape/length/finite
  value validation so they can be discarded and regenerated. `YesDawPersistenceCheck` proves exact
  tier math on deterministic samples; `YesDawBundleRenderCheck` imports the fixture WAV into a `.yesdaw`
  bundle, decodes the bundled Asset on the control/test side, writes the peak cache under `peaks/`,
  reloads it, deletes `peaks/`, reopens Project truth unchanged, regenerates identical cache data, and
  rejects/replaces a corrupt peak header. No Clip editing operations, undo/redo, UI, export, plugin
  hosting, H3 work, ADR edits, roadmap edits, golden edits, or `[[clang::nonblocking]]` edits. Local
  gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (124/124). Remote CI is pending until this worker commit is pushed.
  **Next:** REVIEW/FIX H2 waveform peak-cache foundation.
- **Latest: REVIEW/FIX H2 bundled Asset read/decode projection found no defects.** Reviewed worker
  commit `2aba17e` against H2 scope, ADR-0011, ADR-0012, the H2 deepening notes, and the current
  `ProjectBundleDb` / `Project` / render-test surfaces. The slice stays headless and narrow:
  `DecodedClipNode` is a pure source node that reads pre-decoded samples on the hot path, `GraphBuilder`
  classifies it as `Source`, and `YesDawBundleRenderCheck` reopens a `.yesdaw` bundle, decodes the
  bundled immutable Asset through the existing JUCE WAV reader path on the control/test side, projects
  two non-destructive Clip source windows through Runtime and offline graph paths, compares both against
  expected decoded Clip output, asserts non-silence, and proves bundled Asset bytes are unchanged. No
  code defect found and no waveform cache/peaks, Clip editing operations, undo/redo, UI, export, plugin
  hosting, ADR edits, roadmap edits, golden edits, or `[[clang::nonblocking]]` edits. Local gate via
  documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (122/122). Remote CI run `28132790457` for worker commit `2aba17e` and run
  `28133086695` for pre-review `main`/status commit `9a91ddb` are green across Windows, Linux, macOS,
  RTSan, and TSan.
  **Next:** WORKER H2 waveform peak-cache foundation: add the smallest headless content-hash-keyed
  peak/mipmap cache gate for bundled Assets, with deterministic min/max+RMS tiers and safe
  delete/regenerate behavior. Keep it off the audio hot path and do not start Clip editing operations,
  undo/redo, UI, export, plugin hosting, ADR edits, roadmap edits, golden edits, or
  `[[clang::nonblocking]]` edits; if a cache-format decision rises to ADR level, stop and report.
- **Latest: WORKER H2 bundled Asset read/decode projection is green locally.** Added the smallest
  headless projection from bundled `.yesdaw` Asset bytes into the graph/Render path: `DecodedClipNode`
  plays pre-decoded Clip source windows, `GraphBuilder` classifies it as a `Source`, and
  `YesDawBundleRenderCheck` imports the fixture WAV into the bundle with content-hash Asset storage,
  writes a Project with two Clips referencing that same immutable Asset, reopens the bundle, decodes the
  bundled `.asset` bytes through the existing JUCE WAV reader path, and renders through both Runtime and
  offline graph paths. The gate asserts RT/offline equality, decoded-Clip expected output equality,
  non-silence, and unchanged bundled Asset bytes after projection. No waveform cache/peaks, Clip
  editing operations, undo/redo, UI, export, plugin hosting, ADR edits, roadmap edits, golden edits, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (122/122). Remote CI run
  `28132790457` for `2aba17e` is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 bundled Asset read/decode projection.
- **Latest: REVIEW/FIX H2 asset import + copy-to-bundle recovery gate found no defects.** Reviewed
  worker commit `31ab1c0` against H2 scope, ADR-0011, ADR-0012, the H2 deepening notes, and the current
  `ProjectBundleDb` / `YesDawPersistenceCheck` surface. The implementation stays headless and narrow:
  source bytes hash to SHA-256, copy to a same-directory temp file in `audio/`, re-hash after copy,
  atomically rename to the content-addressed `.asset` path, dedupe repeated imports to the existing
  Asset row, and reconcile stale uncommitted `pending_fs_ops` rows on open. Open verifies committed
  Asset rows against their content-hash bytes and sweeps orphan final files out of `audio/`; tests cover
  dedupe, interrupted-import reopen cleanup, and missing/corrupt committed asset bytes. No code defect
  found and no ADR, golden, roadmap, waveform cache, Clip editing, undo, UI, export, broad decoding,
  plugin hosting, H3 work, or `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell
  flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (121/121). Remote CI
  run `28131177994` for `31ab1c0` and run `28131500386` for latest pre-review `main` are green across
  Windows, Linux, macOS, RTSan, and TSan.
  **Next:** WORKER H2 bundled Asset read/decode projection feeding the graph/Render path without making
  Clips destructive; keep it headless and do not start waveform cache, Clip editing, undo, UI, export,
  plugin hosting, ADR edits, roadmap edits, golden edits, or `[[clang::nonblocking]]` edits.
- **Latest: H1 exit-gate closeout / CI-truth pass is green.** Verified from repo truth that the four H1
  exit gates are represented by self-asserting tests and the latest pushed commit CI:
  Project bundle readback round-trips through `YesDawPersistenceCheck`; RT path vs offline Render
  equivalence is covered by `YesDawRenderCheck` with non-silence and `1e-6` max-abs diff; the audio hot
  path is covered by the Clang 20 RTSan CI leg over the pure engine tests; and interrupted save /
  interrupted migration reopen-clean recovery is covered by `YesDawPersistenceCheck` with
  `integrity_check == ok` and rollback/rerun assertions. Remote CI run `28125785485` for `ac4a576`
  is green across Windows, Linux, macOS, RTSan, and TSan. No ADR, golden, roadmap, code, or
  `[[clang::nonblocking]]` edits. **Next:** stop for Dan's H1/H2 horizon-boundary review; do not start
  H2 until Dan advances the horizon.
- **Latest: REVIEW/FIX H1 kill-during-save/migration reopen-clean gate is green locally.** Reviewed
  `bc5065b` against ADR-0012, ADR-0011, ADR-0010, `CONTEXT.md`, the H1 plan/roadmap, and the current
  SQLite bundle/migration/open-validation/readback code. Found and fixed one narrow test-proof gap:
  migration recovery now asserts the synthetic `schema_migrations.app_build = 'interrupted'` row did
  not survive, so reopen had to rerun and republish the v1 migration state. The save recovery gate
  already proves rollback to the last committed `Project` readback with `integrity_check == ok`. No ADR,
  golden, roadmap, UI, asset import/decoding, waveform cache, plugin hosting, broad automation lane, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --build --preset ci`; `ctest --preset ci` pass (118/118). **Next:** H1 exit-gate closeout /
  CI-truth pass; do not start H2 until Dan advances the horizon.
- **Latest: WORKER H1 kill-during-save/migration reopen-clean gate is green locally.** Added two narrow
  self-asserting recovery gates in `YesDawPersistenceCheck`: an interrupted save transaction closes
  without `COMMIT`, then the bundle reopens with `integrity_check == ok` and the last committed
  `Project` readback intact; an interrupted schema migration transaction writes v1 shape plus
  application/user identity without `COMMIT`, then reopen reruns migration cleanly and passes
  identity, `schema_migrations`, `integrity_check`, and semantic validation. No ADR, golden, roadmap,
  UI, asset import/decoding, waveform cache, plugin hosting, broad automation lane, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --build --preset ci`; `ctest --preset ci` pass (118/118). **Next:** REVIEW/FIX H1
  kill-during-save/migration reopen-clean gate.
- **Latest: REVIEW/FIX H1 RT-vs-offline Render equivalence gate is green locally.** Reviewed `968b16d`
  against ADR-0006, ADR-0007, ADR-0008, ADR-0009, ADR-0010, ADR-0011, `CONTEXT.md`, the H1 plan/roadmap,
  current Runtime/CompiledGraph/GraphBuilder/Node contracts, and the landed `YesDawRenderCheck` +
  CMake surface. Found no real defect: the gate stays inside the current `Project` value surface,
  builds two fresh `CompiledGraph`s from the same valid Project projection, exercises `Runtime`
  publish/process vs a direct offline graph with different Block schedules, and asserts non-silence,
  max-abs diff <= `1e-6`, plus graph-lifetime cleanup. No ADR, golden, roadmap, UI, asset
  import/decoding, waveform cache, plugin hosting, broad automation lane, kill-during-save/migration
  recovery, or `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (116/116). **Next:**
  WORKER H1 kill-during-save/migration reopen-clean gate for the current SQLite bundle/migration surface.
- **Latest: WORKER H1 RT-vs-offline Render equivalence gate is green locally.** Added
  `YesDawRenderCheck`, a narrow in-memory headless gate that builds a valid current `Project` value,
  compiles that same Project projection into two fresh `CompiledGraph`s, publishes one through
  `Runtime`, free-wheels the other as offline Render, slices the two paths with different Block
  schedules, and max-abs-diffs the audio within `1e-6` while asserting non-silence and graph-lifetime
  cleanup. No ADR, golden, roadmap, UI, asset import/decoding, waveform cache, plugin hosting, broad
  automation lane, kill-during-save/migration recovery, or `[[clang::nonblocking]]` edits. Local gate
  via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (116/116), with final build+ctest after the oscillator-backed refinement
  also green. **Next:** REVIEW/FIX H1 RT-vs-offline Render equivalence gate.
- **Latest: REVIEW/FIX H1 Project round-trip bundle readback slice is green locally.** Reviewed
  `e84e612` against ADR-0012, ADR-0011, ADR-0010, `CONTEXT.md`, this handoff, and the H1 Project
  round-trip gate. Found and fixed one real SQLite dynamic-typing defect: existing bundles now reject
  non-canonical storage types on the current `Project`/`Asset`/`Clip` value rows before readback can
  coerce them (for example, a fractional `src_offset` truncating through `sqlite3_column_int64`). Added
  a reopen regression proving that bad row is refused during layered open validation. No ADR, golden,
  roadmap, UI, asset import/decoding, waveform cache, plugin hosting, broad automation lane, or
  audio-thread contract edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (115/115). **Next:** WORKER H1 RT-vs-offline
  Render equivalence gate, with no golden-file edits unless Dan explicitly blesses that boundary.
- **Latest: WORKER H1 Project round-trip bundle readback slice is green locally.** Added
  `ProjectBundleDb::readProjectSnapshot`, the smallest SQLite readback path for the current
  `Project`/`Asset`/`Clip` value surface, with layered validation before reconstructing values from a
  reopened `.yesdaw` bundle. Added a mechanical round-trip regression proving project id/sample rate,
  Asset ids/content hashes/frames/sample rates/channels, and Clip ids/Asset refs/ticks/source windows/
  gain/fades/time_base survive close + reopen. No ADR, golden, roadmap, UI, asset import/decoding,
  waveform cache, plugin hosting, broad automation lane, or audio-thread contract edits. Local gate via
  documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (111/111). **Next:** REVIEW/FIX H1 Project round-trip bundle readback slice
  for the existing Project/Asset/Clip value surface.
- **Latest: REVIEW/FIX ADR-0012 SQLite `.yesdaw` bundle schema slice is green locally.** Reviewed
  `d12c2a8` against ADR-0012 plus adjacent Project/Time/Event/Automation contracts. Found and fixed one
  real open-validation defect: existing bundles now run the layered quick/FK/semantic validator before a
  database handle is returned, and the row-exists helper no longer treats SQLite step errors as "no
  problem." Added a reopen regression proving a semantically corrupt Clip source window is refused on
  open. No ADR, golden, roadmap, UI, asset import/decoding, waveform cache, plugin hosting, broad
  automation lane, or audio-thread contract edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (110/110). **Next:**
  WORKER H1 Project round-trip bundle readback slice for the existing Project/Asset/Clip value surface.
- **Latest: WORKER ADR-0012 SQLite `.yesdaw` bundle schema slice is green locally.** Added the first
  narrow, headless persistence surface in `src/persistence/ProjectBundle.h`: official pinned SQLite
  amalgamation wiring, `.yesdaw` package layout creation, WAL/NORMAL/FK/busy-timeout/autocheckpoint/
  cache/temp-store bring-up, `application_id`/`user_version`, transactional v1 migration harness,
  normalized schema v1 with real Clip→Asset FKs, semantic validation hooks for the existing
  Project/Time/Automation value types, reserved plugin-state chunk header table, and `pending_fs_ops`
  intent-log rows for cross-file asset/blob operations. Added `YesDawPersistenceCheck` coverage for
  bring-up pragmas, forward-schema refusal, migration rollback/no version bump on failure, FK
  enforcement, Project semantic rejection, semantic checks beyond SQLite `quick_check`, and intent-log
  commit/rollback atomicity. No ADR, golden, roadmap, UI, asset import/decoding, waveform cache, plugin
  hosting, broad automation lane, or audio-thread contract edits. Local gate via documented Windows
  DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (109/109).
  **Next:** REVIEW/FIX ADR-0012 SQLite `.yesdaw` bundle schema v1 + FKs + migration harness +
  intent-log atomicity.
- **Latest: REVIEW/FIX ADR-0009 sample-accurate automation evaluator slice is green locally.** Reviewed
  `2855204` against ADR-0009, ADR-0010, `CONTEXT.md`, `AGENTS.md`, this handoff, and the H1 contracts.
  Found no real defect: the helper stays pure/headless, preserves the fixed-size `EventStream` surface,
  advances by cursor, honors half-open Block boundaries, handles output capacity without writing past
  caller storage, and generated parameter Events flow into `FaderNode` at exact in-Block offsets. No
  ADR, golden, SQLite persistence, broad lane/UI work, MIDI note handling, plugin hosting, audio-thread
  contract, or `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (103/103). **Next:**
  WORKER ADR-0012 SQLite `.yesdaw` bundle schema v1 + FKs + migration harness + intent-log atomicity.
- **Previous: WORKER ADR-0009 sample-accurate automation evaluator slice is green locally.** Added the
  pure C++ automation value/evaluator surface in `src/engine/Automation.h`: storage-facing
  `AutomationPoint { tick, value, curveType }`, the locked ADR-0009 curve enum, parameter target/block
  value types, and a cursor-style `evaluateAutomationPointsForBlock` helper that writes preallocated
  parameter `Event`s through a caller-supplied tick→frame mapper. Added `YesDawEventCheck` coverage for
  enum storage, value validation, half-open Block boundaries, cursor advancement, output-capacity and
  invalid-input handling, and generated automation Events driving `FaderNode` at exact in-Block offsets.
  No ADR, golden, SQLite persistence, broad automation lane/UI work, MIDI note handling, plugin hosting,
  or audio-thread contract edits. Local gate via documented Windows DevShell flow:
  `cmake --build --preset ci`; `ctest --preset ci` pass (103/103). **Next:** REVIEW/FIX ADR-0009
  sample-accurate automation evaluator slice.
- **Latest: REVIEW/FIX ADR-0011 EntityId + Asset/Clip/Project value surface is green locally.** Reviewed
  `aa4f4dc` against ADR-0011, ADR-0012, ADR-0010, `CONTEXT.md`, `AGENTS.md`, this handoff, and the H1
  contracts. Found and fixed one real ULID allocator bug: entropy exhaustion no longer wraps the
  internal entropy state and later emits lower same-timestamp IDs. Added mechanical coverage for
  carry/reset behavior, repeated exhaustion failure, next-timestamp recovery, and Project ID collision
  cases. No ADR, golden, SQLite persistence, broad automation, MIDI note handling, plugin hosting, UI,
  or audio-thread edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (99/99). **Next:** WORKER ADR-0009
  sample-accurate automation evaluator slice.
- **Latest: WORKER ADR-0011 EntityId + Asset/Clip/Project value surface is green locally.** Added the
  pure C++/JUCE-free storage-facing value surface in `src/engine/Project.h`: fixed 16-byte
  `EntityId`, a monotonic 128-bit ULID allocator, 32-byte Asset content-hash shape, minimal
  `Asset`/`Clip`/`Project` value types, and Project/Clip invariants for valid unique IDs, Asset validity,
  Clip→Asset references, and `clip.src_offset + clip.src_len <= asset.frames` without overflow. Added
  `YesDawProjectCheck` coverage in `tests/project_tests.cpp`. No ADR, golden, SQLite persistence, broad
  automation, MIDI note handling, plugin hosting, UI, or audio-thread edits. Local gate via documented
  Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass
  (99/99). **Next:** REVIEW/FIX ADR-0011 EntityId + Asset/Clip/Project value surface.
- **Latest: REVIEW/FIX ADR-0009 generic event stream flowing param-changes slice is green locally.**
  Reviewed `cce212a` against ADR-0009, ADR-0008, ADR-0010, `CONTEXT.md`, and the H1 contracts. Found
  and fixed one real command/event interaction bug: after a gain parameter Event moved `FaderNode` away
  from the old command target, a later `SetGain` command back to that same old value could be swallowed.
  `FaderNode` now tracks `SetGain` commands with a lock-free revision counter, so equal-valued commands
  still override prior event targets while event targets persist across blocks when no command arrives.
  New coverage proves the edge case. No ADR, golden, persistence, MIDI note handling, plugin hosting, or
  broad automation evaluator edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (94/94). **Next:** WORKER ADR-0011
  EntityId + Asset/Clip/Project value surface.
- **Previous: WORKER ADR-0009 generic event stream flowing param-changes slice is green locally.** Replaced
  the `EventStream` placeholder with the first ADR-0009 fixed-size event surface: trivially-copyable
  `Event`, CLAP-style `VoiceAddress`, parameter/note/SysEx payload space, non-owning block-sliced
  `EventStream`, and a validator for sorted half-open `[0, numFrames)` offsets. `FaderNode` now consumes
  its gain parameter changes from the shared stream at exact in-Block offsets while preserving the frozen
  `Node::process` shape and the existing `SetGain` command seam. New `YesDawEventCheck` coverage proves
  fixed-size shape, sorted/boundary validation, wrong-node filtering, exact offset flow, and cross-block
  target persistence. No ADR, golden, persistence, MIDI note handling, or broad automation evaluator edits.
  Local gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (93/93). **Next:** REVIEW/FIX ADR-0009 generic event stream flowing
  param-changes slice.
- **Previous: REVIEW/FIX ADR-0010 time-model types slice is green locally.** Reviewed `7412597` against
  ADR-0010, ADR-0008/0009/0011/0012, `CONTEXT.md`, and the H1 round-trip contracts. Found and fixed one
  real validation gap: `TempoChange::hasValidBpm()` and `SampleRate::isValid()` now reject non-finite
  values, matching the finite-tempo-map / sane-project-rate persistence contract before schema code
  starts depending on these helpers. No ADR, golden, event-stream, or `[[clang::nonblocking]]` edits.
  Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (89/89). **Next:** WORKER
  ADR-0009 generic event stream flowing param-changes slice.
- **Previous: WORKER ADR-0010 time-model types slice is green locally.** Added `src/engine/Time.h` with
  the storage-facing time value surface: canonical `Tick`, `PPQ = 15360`, render-only `MusicalTime`,
  `TimeBase`, tempo/meter change records, `SampleRate`, resample quality tiers, non-owning tempo/meter
  map views, and the ADR-0010 `Transport` body used by `Node::process`. New `YesDawTimeCheck` locks
  PPQ, enum storage values, fraction validity, map-view shape, and the default project sample rate.
  No ADR or golden edits. Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (88/88).
  **Next:** REVIEW/FIX ADR-0010 time-model types slice.
- **Previous: REVIEW/FIX compiler slice K is green locally.** Reviewed `e88a6b4` against ADR-0006/0007/0008
  and the locked compiler design. No code defect found: `Runtime` routes `SetGain`/`SetPan` through the
  one ordered command queue to the `CompiledGraph` current at each command point, `applySetGain`/
  `applySetPan` use the sorted `idIndex_` lookup and return false for degenerate/missing/wrong-kind
  targets, and matched commands only mutate `FaderNode`/`PanNode` target state. `Node.h` stayed frozen;
  slice I/J pool, mute, carry-over, deterministic input ordering, and bus bind invariants stayed intact.
  Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (84/84). **Next:** WORKER
  time-model types (ADR-0010).
- **Previous: WORKER compiler slice K is green locally.** Runtime now routes `SetGain`/`SetPan` from the
  one ordered command queue to the `CompiledGraph` that is current at that command point, using the
  sorted `idIndex_` lookup. `CompiledGraph::applySetGain/applySetPan` return false for degenerate,
  missing-id, and wrong-kind targets; matched commands only call `FaderNode::setTargetGain` or
  `PanNode::setPan`. New coverage proves a gain command before a `SwapGraph` does not mutate the new
  graph, a gain command after the swap does, PanNode routing is audible in rendered samples, invalid
  scalar commands do not corrupt output, and degenerate graphs stay no-op. `Node.h` stayed frozen and
  slice I/J invariants stayed untouched. Local gate: `cmake --build --preset ci` and `ctest --preset ci`
  pass (84/84).
- **Latest: REVIEW/FIX compiler slice J is green locally.** Reviewed `b649acc` against the locked
  compiler design plus ADR-0007/0008. Node.h stayed frozen and slice K SetGain/SetPan routing did not
  land. Slice I pool invariants still hold: greedy width-sized f32 slots, slot 0 permanent silence,
  locked Fader/Meter-only R3 aliasing, separate f64 bus scratch, and order-shuffle invariance. Fixed one
  real slice J carry-over bug: synthetic PDC LatencyNodes now carry a full 64-bit `DelayCacheKey`
  alongside their low 32-bit diagnostic `NodeId`, so distinct latency delay rings cannot collide in the
  DelayCache during carry-over/reclamation snapshots. New coverage proves colliding low NodeIds still
  snapshot as distinct full keys. Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass
  (78/78). **Next:** WORKER compiler slice K (SetGain/SetPan command routing).
- **Previous: WORKER compiler slice J is green locally.** Pass 5 now assigns mute bits, exposes an atomic
  mute mask on `CompiledGraph`, carries `DelayNode` ring state from `previousForCarryOver`, sorts
  multi-input metadata by producer `NodeId`, and asserts/debug-checks that bus-style multi-input nodes
  were bound on the control thread. Runtime janitor reclamation snapshots delay rings before delete.
  New coverage proves mute flip without rebuild, matching delay carry-over continuity, mismatched
  delay-ring zero-fill/no-NaN output, deterministic input order, and an assertable unbound-bus failure.
  Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (77/77).
  **Next:** REVIEW/FIX compiler slice J. Do not start slice K until that review/fix checkpoint is green.
- **Previous: REVIEW/FIX compiler slice I is green locally.** Reviewed `cdbefd3` against the locked
  compiler design plus ADR-0007/0008. The Pass 4 pool shape is correct: greedy last-reader allocation
  is sized to live width, slot 0 remains permanent silence, R3 aliasing is limited to the locked
  Fader/Meter predicate, and Sum/Master f64 scratch metadata stays separate from f32 audio slots.
  Fixed two review gaps: the locked debug NaN pool paint is now compiled into the builder gate, and
  bus input binding no longer wraps at the exact `uint16_t` maximum fan-in. Local gate:
  `cmake --build --preset ci` and `ctest --preset ci` pass (72/72).
  **Next:** WORKER compiler slice J (Pass 5 mute + carry-over + bind-check). Do not start slice K until
  slice J is reviewed/fixed green.
- **Previous: WORKER compiler slice I is green.** `GraphBuilder` now performs Pass 4 greedy
  buffer-pool allocation: slot 0 is permanent silence, output slots are sized to live width instead of
  one per node, last-reader analysis covers multi-input readers, R3 in-place reuse is limited to the
  locked Fader/Meter predicate, and Sum/Master bus scratch gets separate f64 slot metadata. `CompiledGraph`
  now respects aliased node slots on the hot path. New mechanical coverage proves width sizing, slot-0
  exclusion, R3 positive/negative cases, multi-input last-reader protection, Sum/Master f64 scratch, and
  order-shuffle invariance for equivalent diamond graphs. Local gate:
  `cmake --build --preset ci` and `ctest --preset ci` pass (71/71).
  **Next:** REVIEW/FIX compiler slice I. Do not start slice J until that review/fix checkpoint is green.
- **Previous: REVIEW/FIX compiler slice H is green.** Reviewed `b418fd9` against the locked compiler design
  plus ADR-0007/0008. No code defect found: Pass 3 PDC is a single longest-path walk over topo/input
  metadata, synthetic `LatencyNode` splices are owned by the payload and excluded from command routing,
  flat `uint16` compiled-node/slot metadata remains bounded, and the tests mechanically catch both the
  old two-peak no-splice behavior and spurious single-input splices. Verified no slice I buffer-pool,
  slice J carry-over, or slice K SetGain routing landed in slice H. Local gate:
  `cmake --build --preset ci` and `ctest --preset ci` pass (65/65).
  **Next:** WORKER compiler slice I (Pass 4 buffer pool + order-shuffle invariance).
- **Previous: worker compiler slice H landed.** `GraphBuilder` now performs Pass 3 PDC:
  longest-path latency metadata, synthetic `LatencyNode` splices at convergence points, `totalLatency()`
  publication, and no spurious splice on single-input chains. Added test-only `StubLatencyNode`/impulse
  coverage proving a 2.0 peak lands at exactly frame N, the old unspliced two-peak failure is guarded,
  `totalLatency()==N`, single-input chains stay unspliced, and INT64_MAX/negative latencies fail loudly.
  Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (65/65).
- **Previous: REVIEW/FIX compiler slice G landed.** The review found one real validation
  gap: an over-wide bus fan-in could overflow the flat `uint16` input metadata and compile to silence
  instead of failing loudly. `GraphBuilder` now rejects unrepresentable reachable-node/input counts with
  `GraphTooLarge`; coverage also asserts empty-project silence, missing-master rejection, and negative
  latency rejection. Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (61/61).
- **Previous: compiler slice G landed.** `GraphBuilder` now performs Pass 1+2 validation and iterative
  Master-backward topo, rejects duplicate/missing/over-latency/cyclic graphs, allows `DelayNode`
  feedback boundaries, and builds the first real payload graph with `MasterNode` + `IdentityDcNode`.
  `CompiledGraph` runs the minimal one-slot/node executor while preserving the legacy `(GraphId, dc)`
  degenerate fast path.
- **Previous: REVIEW/FIX of compiler slice F landed.** The review found one real lifecycle gap:
  `CompiledGraph` owns prepared Nodes but did not call `Node::release()` before destruction. That is fixed
  on the janitor/control-side destructor path and covered by a `YesDawGraphCheck` lifecycle test.
- **Previous: `CompiledGraph` compiler slice F landed, then the macOS warning was fixed.** The graph has
  the additive ADR-0007 state/layout surface (`Payload`, flat compiled-node metadata, input-slot table,
  buffer-pool layout, mute mask, master output bookkeeping, id index) behind the preserved legacy
  `(GraphId, dc)` degenerate fast path. `src/dsp/ScopedNoDenormals.h` landed with the written R1–R7
  buffer-pool contract; no builder/audio executor path is reachable yet. AppleClang's
  `-Wunused-lambda-capture` warning in `tests/pan_tests.cpp` is removed.
- **Previous: the five built-in Nodes are in & green.** `DelayNode` (the one PDC+feedback primitive;
  `LatencyNode` is an alias), `FaderNode` (ramped gain), `PanNode` (equal-power mono→stereo, LUT),
  `SumNode` (f64 Bus summing, canonical NodeId order), `MeterNode` (peak/RMS, lock-free publish) — each
  its own independently-green commit behind the frozen Node trait, each `YESDAW_RT_HOT` with a
  cross-block-size invariance gate. `src/dsp/LinearRamp.h` is the per-frame ramp helper. The locked
  compiler implementation design remains
  [docs/plans/2026-06-23-compiledgraph-compiler-design.md](docs/plans/2026-06-23-compiledgraph-compiler-design.md);
  build commits G–K from there.
- **The Node contract (ADR-0008) is frozen + green.** `src/engine/Node.h` is the CLAP-shaped
  trait (`NodeProperties`/`AudioBlock`/`ProcessArgs` + `prepare`/`process`/`reset`/`release`/`directInputs`);
  `process` is `noexcept` + `YESDAW_RT_HOT` (RTSan-clean). First built-in `OscillatorNode` (wraps
  `SineSource`); the H0 throwaway Node stub is retired and block-size independence is re-asserted through
  the real trait. `EventStream`/`Transport` are placeholders fleshed out by ADR-0009/0010. CI green on
  `787d854` (RTSan/TSan/3-OS).
- **Foundation: the RT-safe graph-swap core (ADR-0006) is in and green.** `src/engine/Runtime.h`
  is the seam between the control thread and the one audio thread: one ordered **choc SPSC** command
  queue carries `SwapGraph` (with a `SetGain`/`SetPan` seam reserved); the audio thread owns `current_`
  and reads an immutable `CompiledGraph`; retired graphs go to an audio→control queue and a
  **generation-counter janitor** frees them on the strict-greater `processedGen > retiredAtGen`
  fence-post. Design was chosen by a **4-design adversarial panel + 3 judges**; the must-fix grafts are
  in (retire-queue backpressure, trivially-copyable POD command, `static_assert` lock-free, a debug
  canary, INVARIANT comments). 25/25 Catch2 tests pass locally (MSVC); a 2-thread stress test is the
  **new TSan leg's** target. RTSan covers `processBlock`. *(A real bug surfaced + fixed: choc's
  `getFreeSlots()` over-reports by one, so the backpressure gate now uses `getUsedSlots()`.)*
- **Verification: GREEN.** CI on `747f46a` passed every leg — Windows/Linux/macOS build + ctest,
  **RTSan** (audio hot path never allocates/locks) and the **new TSan** leg (the release/acquire
  reclamation contract has no data race). The concurrency core is now mechanically proven, not argued.
  A 4-design panel + 3-judge design pass and a 3-reviewer adversarial code review (7 findings, all
  fixed) preceded green. *(One CI-only bug fixed post-push: a `Config cfg = {}` default arg MSVC
  accepts but Clang/GCC reject.)*
- **H0 carry-over decided:** the native GPU render shell + `max_frame_ms<16.6` soak gate is **folded
  into H2** (UI work). H1's exit is 100% headless CI, so it does not block. The audio soak still stands.

## Current-horizon checklist — H3 (mixer + plugin hosting; closed)
> Exit gate: deterministic in-repo `YesDawHostIsolationCheck` proves hosted-plugin PDC, crash/hang
> isolation, fail-open/no-dropout behavior, persistent blacklist, and opaque state round-trip across the
> real worker process. `pluginval`/`auval` are non-blocking external coverage per ADR-0015.
- [x] **ADR-0013 plugin state + hosting isolation. First chunk.** Lock opaque plugin-state chunks and
  out-of-process/sandboxed hosting before any H3 plugin-host code lands.
- [x] Mixer as graph projection: Fader/Pan/Sum/Send/Return/Meter, solo/mute/SIP solo-safe behavior, and
  Sidechain input pins are headless and green.
- [x] Automation lanes honor per-Block offsets through the hosted `PluginNode` projection path.
- [x] Out-of-process plugin host boundary with persistent blacklist and hang watchdog.
- [x] `PluginNode` IPC proxy: shared-memory audio/event buffers, one-block fail-open pipeline, no audio
  thread wait on child process.
- [x] In-repo JUCE `AudioProcessor` hosting behind `PluginNode`; real external scanner/pluginval/auval and
  CLAP remain non-blocking/future coverage per ADR-0015.
- [x] Opaque plugin-state persistence and corrupt-chunk graceful fallback.
- [x] H3 mechanical gates: blocking in-repo host-isolation gate plus full CI.

## Previous-horizon checklist — H2 (closed by Dan boundary review; editing-first)
> Exit gate (all green in CI): any edit sequence + full undo returns the document bit-identical; a
> split-with-crossfade Project's RT playback matches offline Render; **and** a kill mid-import recovers
> with the bundle's DB↔filesystem consistent (assets hash-verified, no orphans).
- [x] Import + copy-to-bundle with content-hash dedupe, staged temp writes, re-hash-before-rename, and
  intent-log/reconcile-on-open recovery. **First chunk.**
- [x] Bundled Asset read/decode projection feeds the graph/Render path without making Clips destructive.
- [x] Clip editing as metadata: split, trim, move, gain, fade-in/out, and equal-power crossfade.
- [x] Snap/grid round-trips exactly through integer ticks↔samples.
- [x] Command/diff undo/redo with transaction grouping and a property-based bit-identical undo gate.
- [~] Offline Render/Export for edited Projects, including split-with-crossfade RT-vs-offline coverage
  — split-with-crossfade RT/offline coverage is green; export UX is not part of this exit gate.
- [ ] Single-window timeline-primary shell with remappable keymap; native GPU render shell / frame-time
  gate comes here as the folded H0 UI carry-over.
- [x] **Exit gates green:** property undo · split-crossfade RT-vs-offline · kill-mid-import bundle
  consistency. Local gate is green; remote CI run `28146655906` for closeout commit `435d320` is green
  across Windows, Linux, macOS, RTSan, and TSan. Dan approved H2->H3.

## Previous-horizon checklist — H1 (closed; spine)
> Exit gate (all green in CI): a Project round-trips (tempo/meter map, markers, clips intact); the RT
> path matches an offline Render within golden tolerance; the audio path is RTSan-clean; **and** a kill
> during save/migration reopens cleanly (WAL recovery + `integrity_check`).
- [x] **Freeze the irreversible contracts as ADRs 0006–0012** ✓ — graph+PDC, time model, event model,
  Node contract, concurrency, data-model indirection, persistence. (docs-only; CI green by construction.)
- [x] RT-safe audio callback skeleton (`YESDAW_RT_HOT` + RTSan coverage) — `Runtime::processBlock`
  outputs silence from a `nullptr` graph, renders the installed graph otherwise. ✓
- [x] SPSC command queue + queue-applied graph swap + generation-counter janitor (ADR-0006) ✓ — one
  ordered choc queue (`SwapGraph`), audio-thread-local `current_`, audio→control retire queue, strict
  `processedGen > retiredAtGen` fence-post; backpressure not leak. RTSan + TSan legs cover it in CI.
  *(`src/engine/{CompiledGraph,Command,Runtime}.h`, `tests/{compiledgraph,runtime}_tests.cpp`.)*
- [x] `CompiledGraph` 5-pass compiler with PDC wired in; all built-ins report 0 latency (ADR-0007);
  PDC impulse test + cross-buffer-size invariance + order-shuffle invariant as Catch2 gates. **Design
  locked** ([compiler-design note](docs/plans/2026-06-23-compiledgraph-compiler-design.md)); build
  commits F (CompiledGraph state), G (Pass 1+2 + Master/IdentityDc + first render), H (PDC), I
  (buffer pool), J (mute + carry-over + bind-check), and K (SetGain/SetPan seam) are done and
  reviewed/fixed.
- [x] Built-in Nodes behind the contract (ADR-0008) — **all five in & green**: `OscillatorNode`,
  `DelayNode`/`LatencyNode`, `FaderNode`, `PanNode`, `SumNode` (f64 Bus summing), `MeterNode`. Each a
  separate green commit. *(Master = a top-level SumNode + device-wiring land with the compiler / H2.)*
- [x] Generic event stream flowing param-changes (ADR-0009) ✓ — fixed-size `Event`/`EventStream`,
  half-open sorted offsets, exact-offset Fader gain events, and the `SetGain` command seam review/fix
  are green.
- [x] Project data-model value surface (ADR-0011) — 128-bit EntityId/ULID surface plus Asset/Clip/Project
  value types and invariants, before SQLite persistence wiring.
- [x] Automation evaluated sample-accurately — curve storage is locked by ADR-0009; broad evaluator/lane
  work stays deferred until the current H1 plan calls it forward.
- [x] SQLite `.yesdaw` bundle: schema v1 + FKs + migration harness + intent-log atomicity (ADR-0012).
- [x] **Exit gates green:** Project round-trip · RT-vs-offline golden diff · RTSan-clean ·
  kill-during-save/migration reopen-clean. H1 done when all four are green in CI.

## Previous-horizon checklist — H0 (closed; GPU render shell + 60fps gate folded into H2)
- [x] Install the C++ toolchain (CMake + MSVC via VS 2022 Build Tools). ✓
- [x] `cmake -B build` configures and fetches JUCE with no error. ✓
- [x] App builds and a window opens (`YesDaw.exe`). ✓ — *`Main.cpp` compiled clean first try.*
- [x] A 440 Hz tone plays out real hardware (spike #1: device round-trip core). ✓
- [x] **Stand up CI + a self-asserting check harness** ✓ — GitHub Actions (Win+Linux+mac) via the `ci`
  preset builds + runs Catch2 `YesDawCheck` (golden + Goertzel/zero-crossing 440 Hz + RMS/peak/symmetry/
  DC purity + fade + perf); RTSan leg (`-fsanitize=realtime`, Clang 20) enforces no-alloc on the hot
  path; warnings-as-errors; `bless-goldens`. Recorded in ADR-0005. *(green; see `docs/ci-mechanical-verification.md`)*
- [x] Tame the spike (fade-in / lower level) ✓ — 50 ms fade-in + `noteOn/noteOff` in `SineSource`,
  −20 dBFS default; asserted by the fade-in check. *(start-stop UI deferred — spike.)*
- [x] Real-machine soak harness built ✓ — `YesDawSoak` opens the real device, counts xruns/deadline-
  misses → PASS/FAIL; now enforces the **128-frame** target (`--block-size`, the roadmap stress case)
  and, with `--loopback`, that the captured tone is actually **440 Hz**. Run with `tools/soak.ps1`
  (native Windows, no Git Bash) or `tools/soak.sh`. Audio is clean (0 dropouts) on the owner's box, but
  the 128-frame target needs a **low-latency driver** (ASIO/WASAPI-exclusive — shared-mode Realtek
  forces 480). **Owner runs the 10-min gate; loopback needs an out→in jumper.**
- [x] Load + scrub one WAV ✓ — `YesDawAssetCheck` decodes a committed fixture WAV, golden-diffs the
  440 Hz sine (≤1e-4), recovers pitch (zero-crossings), and scrubs (sub-range read == slice, bit-
  identical). CI green on Win/Linux/mac. *(spike #1 complete)*
- [~] GPU timeline 100+ elements at 60fps (spike #2) — **CPU half done + green**: pure viewport
  virtualization (`src/ui/TimelineLayout.h`, `YesDawUiCheck`) lays out a 5000-clip viewport in
  **0.0069 ms/frame** (~2400× under the 16.6 ms budget), so the whole frame is the GPU's. *Remaining
  (real-hardware): a native GPU render shell + `max_frame_ms<16.6` in the soak (NOT yet implemented).*
  Native is the chosen direction (plan-recommended + this spike's cost validation); the formal UI-stack
  ADR (fork #2) is written at H1 — until then "native" is a strong lean, not a locked ADR.
- [x] One Node behind a stub of the format-neutral trait (spike #3) ✓ — `YesDawEngineCheck` drives a
  `ToneNode` via the trait at block sizes 1/31/128/512/4096/9000 → bit-identical output, finite, no
  denormals. *(throwaway stub; the real Node contract is frozen at H1.)*
- [ ] **Exit = two soak gates on a real machine** (no human judgment):
  - **(a) audio — IMPLEMENTED:** `soak.sh`/`soak.ps1` exits 0 with `xruns==0`, `deadline_misses==0`,
    `block_size<=128`, and (with `--loopback`) RMS>0.01 dominated by 440 Hz.
  - **(b) GPU 60 fps — NOT YET IMPLEMENTED:** `max_frame_ms<16.6` requires the native render shell that
    doesn't exist yet, so the soak does NOT check it — a soak PASS today is the AUDIO gate only.
  H0 is done when both are green on one machine at a 128-frame Block.

## Done recently
- 2026-06-23 — **Foundation** committed: research corpus, CONTEXT glossary, ADR-0001/0002, roadmap, CLAUDE.md.
- 2026-06-23 — **Brainstorm**: direction locked — full general-purpose DAW; C++/JUCE + our own engine;
  audio + MIDI co-equal; linear timeline; editing-first; long-horizon.
- 2026-06-23 — **Plan** written; ADR-0003 (product) + ADR-0004 (stack); roadmap rebuilt; docs reconciled.
- 2026-06-23 — **Deepen-plan** applied: deepening-notes companion; loops section; decision #14
  (sample-rate); 10 simplifications adopted (8 scope-cuts rejected — full scope kept); housekeeping.
- 2026-06-23 — **Loop workflow adopted in full**; **3 H1 conflicts resolved** (15360-tick grid /
  128-bit ULID / out-of-process hosting).
- 2026-06-23 — **Codex plan review applied** (all 7 findings, no scope cut): made the snapshot /
  state-ownership / graph-publication model exact; promoted bundle crash-recovery into H1's gate;
  fleshed the out-of-process host runtime + isolation gate; PDC test now covers automation + events;
  sample-rate → H1 + automation-curve added as decision #15; fixed stale docs (adr/README, CLAUDE.md).
- 2026-06-23 — **Codex review round 2 applied:** plugin-IPC nonblocking contract (audio thread never
  waits on a child — one-block pipeline + fail-open); per-run state arenas (RT vs offline never share
  state); fixed persistence contradiction; H1 recovery gate = save/migration (import-kill → H2);
  marked resolved conflicts historical.
- 2026-06-23 — **H0 kickoff:** committed CMake + JUCE scaffold + sine spike (`src/Main.cpp`), `AGENT.md`,
  `.gitignore`. Unverified until the toolchain is installed and it's built.
- 2026-06-23 — **H0 spike #1 core WORKING:** toolchain in (MSVC 19.44 / CMake), JUCE fetched + built,
  `Main.cpp` compiled clean **first try**, `YesDaw.exe` plays a 440 Hz sine out real hardware. Full
  stack proven end-to-end.
- 2026-06-23 — **Mechanical-first model + CI cheat-sheet** committed (`docs/ci-mechanical-verification.md`)
  + `bootstrap/windows.ps1` (idempotent one-command toolchain install; fixes the winget-quoting pain).
  Standing up CI is the agent's first H0 task. Commit rule: frequent, straight to main, no squash.
- 2026-06-23 — **CI + harness LIVE and GREEN** (the first H0 task, done in full): extracted a pure
  `SineSource` from the spike; Catch2 `YesDawCheck` (golden + pitch + level + purity + fade + perf);
  GitHub Actions 3-OS matrix via the `ci` preset; warnings-as-errors (SYSTEM-demoted deps); RTSan leg;
  `bless-goldens`; ADR-0005. An **adversarial multi-agent review** caught + closed two real gate holes
  (golden window inside the fade; asymmetric distortion passing) — both proven via injected-bug tests.
  Built the **real-machine soak** (`tools/soak.sh` + `YesDawSoak`); verified on this box.
- 2026-06-23 — **H1 contracts frozen as ADRs 0006–0012** (the precondition for engine code): time model
  + sample-rate (keep-original / resample-at-read), Node contract, event stream + automation (all four
  curves), CompiledGraph + PDC, immutable-snapshot concurrency, Asset→Clip→Project + 128-bit ULID,
  SQLite bundle + migrations. Two owner product calls made; the resolved forks recorded; CONTEXT.md +
  the ADR index synced. Docs-only checkpoint → CI green by construction. GPU render shell folded to H2.
- 2026-06-23 — **RT-safe graph-swap core landed (ADR-0006)** — `src/engine/{CompiledGraph,Command,Runtime}.h`:
  immutable graph + one ordered choc SPSC command queue (`SwapGraph` + scalar seam) + audio-thread-local
  `current_` + audio→control retire queue + generation-counter janitor (strict `processedGen>retiredAtGen`).
  Design from a 4-design/3-judge adversarial panel; grafts applied (backpressure, POD command, lock-free
  `static_assert`, canary, INVARIANT comments). New **TSan CI leg** added. 25/25 local; choc
  `getFreeSlots()` off-by-one found + fixed. choc pinned (`5685fb5`). Then a 3-reviewer adversarial code
  review (7 findings, all fixed: canary→always-on, dtor contract, null-publish guard, …) — CI green on
  `747f46a` (RTSan + TSan + 3-OS).
- 2026-06-23 — **Node contract landed (ADR-0008)** — `src/engine/Node.h` (CLAP-shaped trait) +
  `src/engine/nodes/OscillatorNode.h`; H0 stub retired; block-size independence re-asserted through the
  real trait; `process` RTSan-clean. CI green on `787d854`.
- 2026-06-23 — **CompiledGraph compiler design panel + all five built-in Nodes landed.** A 4-design
  adversarial panel + 3 judges chose the ADR-0007 compiler implementation (spine = incremental-landing;
  grafts from PDC-correctness / RT-safety / simplest-correct) → locked in
  `docs/plans/2026-06-23-compiledgraph-compiler-design.md`. Then five built-ins, each an
  independently-green commit behind the frozen Node trait: `DelayNode` (the one PDC+feedback primitive,
  write-then-read so delay 0 passes through), `FaderNode` + `LinearRamp`, `PanNode` (equal-power LUT),
  `SumNode` (f64 Bus summing, canonical NodeId order, f64-cancellation gate), `MeterNode` (lock-free
  peak/RMS). Each has a cross-block-size invariance gate; `ci` gate green at every commit (47/47 local).
  Fixed three real bugs in the panel's sketch (include convention, delay-0 read/write order, f64
  test using 1e30 instead of 1e8). The 5-pass compiler itself (commits F–K) is the next chunk.
- 2026-06-24 — **CompiledGraph compiler slice F landed.** `CompiledGraph` gained the additive ADR-0007
  state/layout surface and `Payload` constructor while preserving the legacy `(GraphId, dc)` degenerate
  fast path for existing Runtime/CompiledGraph tests. `ScopedNoDenormals` landed for the real node
  executor path. Local `ci` build + 47/47 tests green.
- 2026-06-24 — **macOS CI warning fix.** AppleClang rejected an unnecessary lambda capture in the
  PanNode block-size test under `-Werror`; removed the capture. Local `ci` build + 47/47 tests green.
- 2026-06-24 — **Slice F review/fix.** Reviewed `a642ce9` and `b8c8e7c` against the locked compiler
  design plus ADR-0007/0008. Fixed one lifecycle contract gap: `CompiledGraph` now calls
  `Node::release()` for owned Nodes on destruction, and a new graph lifecycle test asserts it. Local
  `ci` build + 48/48 tests green.
- 2026-06-24 — **CompiledGraph compiler slice G landed locally.** Added `GraphBuilder` Pass 1+2
  validation/topo, `MasterNode`, `IdentityDcNode`, and the first payload-graph executor path. New
  `YesDawBuilderCheck` coverage proves IdentityDc→Master DC, Osc→Master non-DC, 1000-node iterative
  topo, non-Delay cycle rejection, Delay feedback-boundary allowance, duplicate/missing/latency
  rejection, and channel clamp. Local `ci` build + 57/57 tests green.
- 2026-06-24 — **Slice G review/fix.** Reviewed `af7a0b0` against the locked compiler design plus
  ADR-0007/0008. Fixed one real validation bug: over-wide fan-in / reachable-node counts that cannot fit
  the flat `uint16` compiled metadata now fail as `GraphTooLarge` instead of silently compiling a bad
  graph. Added coverage for that bug plus empty-project silence, missing master, and negative latency.
  Local `ci` build + 61/61 tests green.
- 2026-06-24 — **CompiledGraph compiler slice H landed locally.** Added Pass 3 PDC in `GraphBuilder`:
  longest-path latency walk, synthetic `LatencyNode` splices at convergence points, and published
  `totalLatency()`. Added test-only `StubLatencyNode` + impulse coverage for aligned convergence, the
  old unspliced two-peak guard, no single-input splice, and INT64_MAX/negative latency rejection. Local
  `ci` build + 65/65 tests green.
- 2026-06-24 — **Slice H review/fix.** Reviewed `b418fd9` against the locked compiler design plus
  ADR-0007/0008. Found no code defect: PDC is O(V+E), convergence and `totalLatency()` are covered,
  synthetic latency nodes do not enter command routing, metadata bounds are preserved, and no slice
  I/J/K behavior leaked into H. Local `ci` build + 65/65 tests green.
- 2026-06-24 — **CompiledGraph compiler slice I landed locally.** Added Pass 4 greedy buffer-pool
  allocation: last-reader liveness, exact-channel free lists, slot-0 silence preservation, locked R3
  in-place reuse for Fader/Meter only, and separate Sum/Master f64 bus scratch metadata. `CompiledGraph`
  skips pre-clear/pre-copy for aliased nodes. New builder coverage proves width sizing, slot-0 exclusion,
  R3 positive/negative cases, multi-input last-reader protection, bus scratch slots, and diamond
  order-shuffle invariance. Local `ci` build + 71/71 tests green.
- 2026-06-24 — **Slice I review/fix.** Reviewed `cdbefd3` against the locked compiler design plus
  ADR-0007/0008. Fixed two review gaps: added the locked debug NaN pool paint to the builder gate, and
  made Sum/Master input binding safe at the exact `uint16_t` maximum fan-in. Local `ci` build + 72/72
  tests green.
- 2026-06-24 — **CompiledGraph compiler slice J landed locally.** Added Pass 5 mute metadata/state,
  delay-state carry-over from `previousForCarryOver`, deterministic producer-id input ordering, and
  assertable bus bind checks without changing the frozen Node trait or landing slice K scalar routing.
  Runtime reclamation snapshots delay rings before delete. Local `ci` build + 77/77 tests green.
- 2026-06-24 — **Slice J review/fix.** Reviewed `b649acc` against the locked compiler design plus
  ADR-0007/0008. Fixed one real carry-over key bug: synthetic PDC LatencyNodes now keep full 64-bit
  `DelayCacheKey` metadata instead of relying on the low 32-bit diagnostic NodeId, and a regression
  asserts low-ID collisions remain distinct in the DelayCache. Local `ci` build + 78/78 tests green.
- 2026-06-24 — **Slice K review/fix.** Reviewed `e88a6b4` against ADR-0006/0007/0008 and the locked
  compiler design. Found no code defect: scalar commands route through the one ordered queue to the
  graph current at each command point, `idIndex_` lookup returns false for degenerate/missing/wrong-kind
  targets, `Node.h` stayed frozen, and slice I/J invariants remain intact. Local `ci` build + 84/84
  tests green.
- 2026-06-24 — **ADR-0010 time-model types landed locally.** Added the pure C++ time value surface
  (`Tick`, `PPQ = 15360`, `MusicalTime`, `TimeBase`, tempo/meter change records, sample-rate/resample
  tier records, non-owning map views, and `Transport`) plus `YesDawTimeCheck`. Local `ci` build + 88/88
  tests green.
- 2026-06-24 — **ADR-0010 time-model types review/fix.** Reviewed `7412597` against ADR-0010 and the H1
  round-trip/persistence contracts. Fixed one real validity gap: non-finite tempo BPM and project sample
  rates are rejected mechanically. Local `ci` build + 89/89 tests green.
- 2026-06-24 — **ADR-0009 generic event stream param-change slice landed locally.** Added fixed-size
  `Event`/`EventStream` shape, parameter/note/SysEx payload space, sorted half-open block validation,
  and exact-offset Fader gain parameter consumption through the frozen `Node::process` event slot.
  Local `ci` build + 93/93 tests green.
- 2026-06-24 — **ADR-0009 event stream review/fix.** Reviewed `cce212a` and fixed one real SetGain/event
  interaction bug: command revisions now let an equal-valued `SetGain` command override a previous event
  target, while event targets still persist across blocks without a command. Local `ci` build + 94/94
  tests green.
- 2026-06-24 — **ADR-0011 EntityId + Asset/Clip/Project value-surface review/fix.** Reviewed `aa4f4dc`
  and fixed one real ULID allocator bug: an exhausted same-timestamp entropy range no longer wraps the
  allocator state and later emits lower IDs. Added regression coverage for carry/reset, repeated
  exhaustion failure, next-timestamp recovery, and Project ID collision checks. Local `ci` build + 99/99
  tests green.
- 2026-06-24 — **ADR-0009 sample-accurate automation evaluator slice landed locally.** Added the pure
  automation point/evaluator surface and `YesDawEventCheck` coverage for stored point shape,
  half-open Block event emission, cursor advancement, capacity/invalid-input handling, and generated
  Events feeding `FaderNode`. Local `ci` build + 103/103 tests green.
- 2026-06-24 — **ADR-0009 sample-accurate automation evaluator review/fix.** Reviewed `2855204` and
  found no code defect: stored point shape, locked curve enum, cursor semantics, half-open boundaries,
  output-capacity handling, EventStream compatibility, and FaderNode generated-event flow all match the
  current narrow contract. Local `ci` build + 103/103 tests green.
- 2026-06-24 — **ADR-0012 SQLite bundle schema slice landed locally.** Added the headless SQLite
  persistence surface and `YesDawPersistenceCheck`: pinned SQLite amalgamation, `.yesdaw` bundle
  layout, v1 schema/migration harness, FKs, Project semantic validation, reserved plugin chunk header,
  and `pending_fs_ops` intent-log atomicity. Local `ci` configure/build + 109/109 tests green.
- 2026-06-24 — **ADR-0012 review/fix landed locally.** Existing bundles now run layered semantic
  validation during open, so corrupt stored Clip source windows are refused before callers receive a DB
  handle. Local `ci` configure/build + 110/110 tests green.
- 2026-06-24 — **H1 Project round-trip readback slice landed locally.** Added
  `ProjectBundleDb::readProjectSnapshot` and a reopened-bundle round-trip test for the current
  `Project`/`Asset`/`Clip` value surface. Local `ci` configure/build + 111/111 tests green.
- 2026-06-24 — **H1 Project round-trip readback review/fix.** Existing bundles now reject
  non-canonical SQLite storage types for the current `Project`/`Asset`/`Clip` value rows before
  readback can coerce them. Local `ci` configure/build + 115/115 tests green.
- 2026-06-24 — **H1 RT-vs-offline Render equivalence gate landed locally.** Added
  `YesDawRenderCheck`: the same valid current Project projection is rendered through Runtime and a
  free-wheeling offline Render driver with different Block schedules, then compared within `1e-6`.
  Local `ci` configure/build + 116/116 tests green.
- 2026-06-24 — **H1 RT-vs-offline Render equivalence gate review/fix.** Reviewed `968b16d` against the
  locked H1 contracts and found no code defect: the gate proves the narrow current Project -> CompiledGraph
  projection through both Runtime and offline paths without drifting into deferred surfaces. Local `ci`
  configure/build + 116/116 tests green.
- 2026-06-24 — **H1 kill-during-save/migration reopen-clean gate landed locally.** Added persistence
  recovery tests for uncommitted save rollback to the last committed Project and uncommitted schema
  migration rollback/rerun on reopen. Both assert `integrity_check == ok`; the migration path also
  asserts identity/schema row publication and semantic validation. Local `ci` build + 118/118 tests green.
- 2026-06-24 — **H1 kill-during-save/migration reopen-clean gate review/fix.** Reviewed the recovery
  tests against the locked persistence and Project/time contracts. Fixed one narrow test-proof gap: the
  migration recovery gate now proves the synthetic interrupted migration row did not survive reopen.
  Local `ci` build + 118/118 tests green.
- 2026-06-24 — **H2 bundled Asset read/decode projection landed locally.** Added
  `DecodedClipNode` plus `YesDawBundleRenderCheck`: a headless `.yesdaw` bundle imports the fixture WAV,
  reopens Project/Asset/Clip rows, decodes the bundled Asset file, renders two non-destructive Clip
  source windows through Runtime/offline graph paths, and proves the bundled Asset bytes are unchanged.
  Local `ci` configure/build + 122/122 tests green.

## Next
- ✅ **H1 approved and closed.** H1 contracts, graph/runtime spine, built-in Nodes, persistence,
  RT-vs-offline Render, RTSan, and save/migration recovery gates are green.
- ✅ **H2 approved and closed.** H2's mechanical exit gates are green: bit-identical edit undo/redo,
  split-with-crossfade RT/offline render, and kill-mid-import bundle consistency.
- ✅ **H3 approved and closed.** Mixer policy, host isolation, runtime worker crash/hang recovery,
  blacklist persistence, state chunk round-trip, projected Runtime gate, and close-out review fixes are green.
- ✅ **H4 approved and closed.** MIDI Clips/Notes, tempo-map flattening, instrument timing through PDC,
  Project persistence, piano-roll Note edits, MIDI-effect Nodes, hosted-instrument Event delivery, and
  MPE boundary voice allocation are mechanically covered by `YesDawMidiTimingCheck` and the full `ci`
  preset.
- ✅ **H5 closed; local and remote CI green.** Recording is mechanically covered by
  `YesDawRecordingCheck`: bounded audio-thread FIFO, writer-thread take file, input+output latency
  compensation, punch/loop take ordinals, comp selection, and MIDI timestamp compensation.
- ✅ **H6 closed; local and remote CI green.** Reliability is mechanically covered by
  `YesDawReliabilityCheck`: 100-track / 60-minute audio-frame deadline soak at a 128-frame Block plus
  last-good Autosave recovery after a simulated hard kill.
- ✅ **H7 closed; local and remote CI green.** Offline render/export is mechanically covered by
  `YesDawOfflineRenderCheck`: Project offline render vs independent reference, canonical float32-WAV
  bit-exact round-trip, and export -> bundle Asset import -> decode round-trip, with negative controls.
- ✅ **H8 closed; local and remote CI green.** Playback runtime is mechanically covered by
  `YesDawPlaybackCheck`: Project playback through `RuntimeAudioDriver`, block-size independence, offline
  parity, play/stop/locate/loop transport, H5 recording capture from the transport playhead, and H6
  autosave tick recovery. Local full `ci` gate is green (239/239), and the H8 close-out CI run is green.
- **Next:** choose/open the next horizon. H11 is closed; no H12 has been opened by this closeout.

## Blocked / open threads
- Engine concurrency model (plan's *Threading & the real-time boundary* + *The graph* sections) is out
  for a **Codex re-verify** pass. H0 does not depend on it, so H0 proceeds in parallel.
