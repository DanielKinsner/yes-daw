# Adversarial review — H11 (UI shell) and H12 (operable session, in flight)

- **Date:** 2026-06-29
- **Reviewer:** Claude (adversarial pass, read-only)
- **Repo state reviewed:** `main` @ `8025f59` (`docs(h12): mark kickoff checkpoint done`), clean tree,
  local == `origin/main`.
- **Scope:** H11 is closed/remote-green; reviewed its shipped code + 4 gates. H12 is in flight and
  **docs-only** — reviewed on the repo only (no build, no run), as requested.
- **Method:** read every H11 UI source + gate, then fanned out 12 independent agents — 7 skeptics each
  trying to *refute* a candidate finding, 5 hunters sweeping the adapters, the timeline virtualization,
  the H12 docs, a per-gate "what would stay green?" mutation sweep, and a fair-credit/rule-compliance
  pass. Every claim below is quoted to `file:line` and was checked against the files, not taken on the
  agents' word.

---

## Verdict in one paragraph

H11 delivered a **genuinely good headless command layer** and a **polished visual mockup** — but **not an
operable window, and not a window any gate covers.** The shipped JUCE shell (`src/Main.cpp`) is a static
picture wired to a *mock* registry: it loads no Project, plays no audio, edits nothing, and is exercised
by **zero** mechanical gates. The real, well-built pieces (transport→`PlaybackEngine`, real
`ProjectEditCommand`+undo adapters) exist and are tested **in isolation** but are **not connected to the
window.** For a project whose entire ethos is "gates must bite," the cardinal gap is structural: **the one
artifact H11 was chartered to ship — "the first usable application window" — is the one artifact no gate
verifies.** You could replace `MainComponent`'s entire body with a black window and all four H11 gates
stay green. None of this is fraud — the docs partially disclose it and real input is scoped to H12 — but
"H11 closed / capstone delivered" overstates what landed. H12's plan, as written, can repeat the same gap.

**What's solid, what's not (one line each):**

| Area | Reality |
|---|---|
| Action registry / keymap / disabled-reasons | ✅ Real, tested, biting |
| Timeline + piano-roll edit adapters (`ProjectEditCommand`+undo) | ✅ Real, persistent, tested |
| Transport in `UiAppModel` (→ `PlaybackEngine`) | ✅ Real, with state readback + overflow guards |
| `.yesdaw` bundle round-trip in the smoke | ✅ Real |
| **Shipped window (`MainComponent`)** | ❌ Static mockup on the mock registry; engine-disconnected |
| **Shell test coverage** | ❌ None — no gate links `src/Main.cpp` |
| **Accessibility gate** | ❌ Validates a hand-kept table; shell wires no real AT/keyboard |
| **Mixer edits** | ❌ Projection-only — no Project write, no undo, no graph publish |
| **`...GpuCheck`** | ⚠️ CPU software raster; no GPU exists; H0 GPU risk uncovered |
| Timeline virtualization | ⚠️ Horizontal-only cull; silent-drop + O(n²) regressions unmeasured |

---

## H11 findings (ranked)

### H11-1 — The shipped window is a disconnected mockup, and no gate covers it (HIGH)

This is one structural reality with three faces (the skeptics confirmed all three; "if anything
understated").

**(a) The window is wired to the *mock* registry, not the engine.** `MainComponent` holds a bare
`UiActionRegistry registry; UiActionContext context;` ([Main.cpp:734](../../src/Main.cpp)) and every button
`onClick` calls `registry.dispatch(action, context)` ([Main.cpp:285-289](../../src/Main.cpp)) — which only
flips booleans in `UiActionContext` (e.g. `TransportPlay` → `context.isPlaying = true;`,
[UiActions.h:395-397](../../src/ui/UiActions.h)). It **never** instantiates `UiAppModel` or
`PlaybackEngine` (grep of `src/Main.cpp` for `UiAppModel|PlaybackEngine|loadProjectBundle` → no matches).
So **clicking "Play" produces no audio.** It produces *nothing visible at all* — `TransportPlay` is a plain
button, no draw code reads `context.isPlaying`. Only the Loop toggle highlight and the Mixer/Piano-roll
view switch react to clicks.

**(b) Everything on screen is hardcoded demo data.** The transport readout `"01:02:45:18"` / `"120.00"` /
`"4/4"` / `"Cmaj"` ([Main.cpp:388-395](../../src/Main.cpp)), the 23 timeline clips (`kClips`,
[Main.cpp:76-86](../../src/Main.cpp)), the 11 mixer strips (`kMixer` →
`makeDemoMixerSurface()`, [Main.cpp:109-154](../../src/Main.cpp)), the inspector `"Vocal Lead_03"` /
`"+2.4 dB"` ([Main.cpp:617-634](../../src/Main.cpp)), and the 6 piano-roll notes
(`makeDemoPianoRollSurface()`, [Main.cpp:156-193](../../src/Main.cpp)) are compile-time constants bound to
no loaded Project. The mixer faders, M/S/O buttons, piano notes, and inspector fields are
`drawText`/`fillRect`, not Components — they can't be clicked, focused, or queried.

**(c) The genuinely-built models are unreachable from the window.** `UiAppModel::dispatch` — the
production model — wires **only** transport; for `ProjectNew/Open/Save/Export*`, all timeline/mixer/
piano-roll edits, and device refresh it returns `dispatched=false` with `"…payload required"` /
`"…not wired"` ([UiAppModel.h:126-216](../../src/ui/UiAppModel.h)). `ProjectSave` falls through to the mock
and merely `++saveCount` ([UiActions.h:383-385](../../src/ui/UiActions.h)) — **no real save.** The real
edit adapters (`UiTimelineEditModel`, `UiPianoRollSurfaceModel`) that *do* apply `ProjectEditCommand`s with
undo are never constructed by `UiAppModel` **or** by `MainComponent`.

**(d) No gate exercises `src/Main.cpp`.** It is compiled only into the `YesDaw` GUI app
([CMakeLists.txt:115](../../CMakeLists.txt)); the four gates link only headless headers, and `MainComponent`
is defined inside the `.cpp` with no header, so no test can even construct it. CI itself says so:
*"CI builds the GUI app (proves it compiles + links) but **never RUNS it**"*
([.github/workflows/ci.yml:8-9](../../.github/workflows/ci.yml)). **Net: you could empty
`MainComponent::paint()` and delete every `onClick` — ship a black, dead window — and all four H11 gates
plus CI stay green.**

> **Fair caveat.** H11 was explicitly scoped as an "image-light, model-backed" shell with Project/transport
> wiring deferred to "later H11 slices" ([Main.cpp:3-5](../../src/Main.cpp)), and H12 (in flight) is the
> horizon that makes the session operable. The headless command path *is* real and gate-tested (see Fair
> Credit). The defect is not "it's fake" — it's "the operable window was the deliverable, and it was
> neither built nor gated." The roadmap/STATUS framing of H11 as the delivered "capstone … first usable
> application window" overstates that.

### H11-2 — The accessibility gate is a table self-check; the shell wires no real accessibility (HIGH)

ADR-0032 named accessibility "part of the H11 exit, not a postscript … semantic roles, names, keyboard
reachability" ([0032:29-30](../../docs/adr/0032-h11-ui-stack-and-app-shell.md)). What the gate actually
does ([tests/accessibility_tests.cpp](../../tests/accessibility_tests.cpp)):

- Iterates a generated `0..N` id array and asserts its `.size() == kUiActionCount` — a list *defined* as
  every enum value compared to its own length ([UiAccessibility.h:25-33](../../src/ui/UiAccessibility.h)).
  It "covers every action" tautologically.
- Asserts the static descriptor table has non-empty names/roles and that every region's `role == Panel`
  (all 9 regions are hardcoded `Panel`, [UiAccessibility.h:35-45](../../src/ui/UiAccessibility.h)).
- Greps the launch scripts for substrings (`"Start-Process"`, `"YesDaw_artefacts"`, `"Launched"`,
  [accessibility_tests.cpp:141-146](../../tests/accessibility_tests.cpp)) — it never runs them.

It **never instantiates a JUCE Component** or checks `AccessibilityHandler`/`getTitle`/`getRole`/focus
order. And `src/Main.cpp` sets **none** of `AccessibilityHandler`/`setTitle`/`setDescription`/
`setWantsKeyboardFocus`/`addKeyListener` (grep across `src/` → no matches). The 9 toolbar `TextButton`s get
default JUCE text labels; every other surface is raw `Graphics`. **A screen reader gets 9 labeled buttons
and an opaque canvas, and the gate cannot detect that.** Mutation that stays green: ship the shell with zero
accessibility wiring (which it largely is).

### H11-3 — Mixer "edits" are projection-only, and the mixer gate's meter/loudness checks are tautological (HIGH)

Unlike timeline/piano-roll edits (which persist to the Project via `ProjectUndoStack`),
`UiMixerSurfaceModel::dispatch` mutates **only** transient UI vectors — `control.linearGain =
payload.linearGain` into `trackControls_`/`busControls_` ([UiMixerSurface.h:313-394](../../src/ui/UiMixerSurface.h)).
There is **no `ProjectUndoStack` member, no `ProjectEditCommand`, no Project write, and no graph publish.**
The test proves the disconnect itself: after a fader change it asserts
`model.project().clips[0].gain == originalFirstGain` ([ui_action_tests.cpp:576](../../tests/ui_action_tests.cpp)).
So mixer fader/pan/mute/solo are **not undoable and not saved** — yet they sit behind the same action IDs as
the edits that *are* persistent, so they look equivalent from outside.

`effectivelyMuted` is computed with the real H3 policy (`mixerAnyActiveSolo` /
`mixerTargetIsEffectivelyMuted`, [UiMixerSurface.h:177-194](../../src/ui/UiMixerSurface.h)) but the result
is **display-only** — `applyMixerMutePolicy(...)`/`graph.setMuted` (the only functions that actually mute
audio) are never called.

The mixer gate's name claims it "project[s] fader pan mute solo meters and loudness," but meters/loudness
are **injected constants echoed back**: the fixture supplies `UiMixerMeterReadout{0.8f,…}` and
`UiMixerLoudnessReadout{-14.2,…}` ([ui_action_tests.cpp:503,535](../../tests/ui_action_tests.cpp)) and then
asserts them straight back (`peakLeft == 0.8f`, `integratedLufs == -14.2`). No metering or libebur128 LUFS
engine runs. Mutation that stays green: replace all real meter/LUFS computation with `return 0`.

### H11-4 — `YesDawTimelineGpuCheck` measures CPU software raster; the original H0 "GPU" risk is uncovered (MEDIUM)

The gate paints into a software `juce::Image(ARGB)` via `juce::Graphics`
([timeline_gpu_tests.cpp:90,101-104](../../tests/timeline_gpu_tests.cpp)). There is **zero**
OpenGL/`juce_opengl`/`OpenGLContext` anywhere in `src/` or `CMakeLists.txt`; `MainComponent` is a plain
`juce::Component` with no GL context; the `GpuCheck` target links only `juce_gui_basics`
([CMakeLists.txt:262-272](../../CMakeLists.txt)). So both the gate and the app are **100% CPU-rendered**.
The shipped app also never scrolls — `drawTimeline` hardcodes `scrollSeconds = 0.0`
([Main.cpp:481](../../src/Main.cpp)) and there is no `Timer`/animation — so "60 fps **while scrolling**" is
exercised only by the offscreen test.

Yet the roadmap still names "a 60fps **GPU** timeline" as an H0 "scariest unknown" and an exit criterion
("the GPU timeline holding 60fps while scrolling", [roadmap.md:16-19, 204-206](../../docs/goals/roadmap.md)).
Compounding it: the timeline core is `TimelineLayout.h`, whose own header says *"**THROWAWAY** spike code
(H0), **not the real UI** … the actual GPU rendering smoothness is proven by the real-hardware soak"*
([TimelineLayout.h:1-7](../../src/ui/TimelineLayout.h)) — but (a) it **is** now the production renderer
(`TimelineCanvas.h` shares it), and (b) **no soak renders the timeline** — `tools/soak/SoakMain.cpp` is
audio-only (`renderPlaybackProject`). So the original H0 GPU/vsync/compositor risk is covered by **no
mechanical gate and no soak**, while three docs assert "GPU."

> **Fair caveat (keeps this MEDIUM, not HIGH).** ADR-0032's *binding decision* is hedged — the renderer
> backend is "an implementation detail behind a narrow component boundary and a mechanical frame-time
> harness" ([0032:58-61](../../docs/adr/0032-h11-ui-stack-and-app-shell.md)) — so choosing CPU now is
> arguably within the ADR; the mislabel lives in the **gate name + roadmap exit lines**, not the ADR
> decision. The gate is also a *real* frame-budget check on the same `paintTimelineCanvas` the shell uses,
> with a `countDifferentSamples >= 20` guard against an all-blank false-green. For ~300 small clips,
> CPU at 3.3 ms may well be fine — the issue is the **label and the uncovered GPU risk**, not the number.

### H11-5 — Timeline virtualization: horizontal-only cull, silent clip-drop, and an unmeasured O(n²) (MEDIUM)

- **No vertical cull.** `layoutVisible` skips clips only on song-time (`clipEnd <= leftSec || clipStart >=
  rightSec`) and emits every in-time clip on every lane, with `y = lane * laneHeightPixels`
  ([TimelineLayout.h:63-79](../../src/ui/TimelineLayout.h)). The header claims "only the handful
  intersecting the viewport are ever laid out," but a clip on lane 9999 in the time window is still laid
  out and given an off-screen `y`. The gate masks this because `TimelineCanvas` forces all lanes to fit
  (`laneHeight = clipArea.getHeight()/laneCount`), so it never vertically scrolls.
- **Silent clip-drop.** `layoutVisible` stops at `count < outCapacity` and returns exactly
  `kVisibleClipCapacity` (4096); further visible clips are dropped with no visual artifact. The only guard
  is the `hitVisibleClipCapacity` tripwire, asserted `REQUIRE_FALSE`
  ([timeline_gpu_tests.cpp:138](../../tests/timeline_gpu_tests.cpp)) — but the fixture peaks at ~336 visible
  ([:137](../../tests/timeline_gpu_tests.cpp)), so the cap is **never approached** by the gate, and the
  production shell discards the stats (`(void) paintTimelineCanvas(...)`, [Main.cpp:486](../../src/Main.cpp)).
- **Quadratic on real edits.** `styleForClip` has an O(1) fast path **only when `clip.id == array index`**;
  otherwise it linear-scans all clips, once per visible clip → O(visible × total)
  ([TimelineCanvas.h:92-106,294](../../src/ui/TimelineCanvas.h)). The gate fixture assigns `id` ==
  insertion index ([timeline_gpu_tests.cpp:48-58](../../tests/timeline_gpu_tests.cpp)), so it **always**
  hits the fast path. Any real delete/reorder/non-contiguous id silently flips the renderer to quadratic —
  exactly the regression a frame-time gate exists to catch, and it would miss it.

### H11-6 — App smoke injects pre-decoded PCM and never asserts audio is produced (LOW)

Credit: the smoke writes a **real** `.yesdaw` bundle via `ProjectBundleDb`, reloads it, and drives
play/stop/locate/loop through a **real** `PlaybackEngine` with state read back. But the decoded audio is
**injected** — `makeDecodedAsset()` hands 16 hardcoded floats into `loadProjectBundle`
([app_smoke_tests.cpp:126-167](../../tests/app_smoke_tests.cpp)); the on-disk asset bytes are never
decoded, so "load bundle → real audio" is not proven (matches the standing roadmap note that the
decoder→source-node projection is unbuilt, [roadmap.md:35-36](../../docs/goals/roadmap.md)). It also never
pumps an output block or asserts the playhead advances (`drainTransport` calls `processBlock(nullptr,0,0)`,
[UiAppModel.h:247](../../src/ui/UiAppModel.h)) — it checks the `isPlaying` flag, not audible output. And it
is fully headless: it never "opens the single-window shell" despite the H11 gate wording
([h11 plan:17-18](../../docs/plans/2026-06-29-h11-single-window-timeline-ui-plan.md)).

### H11-7 — Documentation precision nits (LOW)

- **ADR-0033 misstates the window.** Its Context says "the window launches, **loads a Project bundle**,
  draws the timeline…" ([0033:11-12](../../docs/adr/0033-h12-operable-session-ux.md)) — inaccurate for the
  real window (only the headless smoke model loads a bundle). The same ADR self-corrects two lines later
  ("painted projection rather than real hit-tested controls"), and roadmap/horizon do **not** repeat the
  conflation, so it's a one-sentence overstatement. Suggested fix: "the **headless app model** loads a
  Project bundle."
- **A documented gate that doesn't exist.** `docs/ci-mechanical-verification.md:43` lists "App builds,
  window opens … under Xvfb, construct `MainComponent`, assert … `getBounds()==600×300` | ✅ cloud (Xvfb)"
  — but there is no Xvfb step in any workflow, and `600×300` is stale (the shell is `setSize(1536, 960)`,
  [Main.cpp:266](../../src/Main.cpp)). The verification matrix claims a shell gate that was never built.
- **Minor reporting quirks:** successful piano-roll edits return `undoStatus = NothingToUndo` even though
  they just recorded an undoable transaction ([UiPianoRollSurface.h:302-305](../../src/ui/UiPianoRollSurface.h));
  mixer reads bump `mixerReadCount` but not `commandDispatchCount`, unlike every edit
  ([UiMixerSurface.h:345-356](../../src/ui/UiMixerSurface.h)); a queue-full transport rejection is swallowed
  as `enabled=true, dispatched=false` with an empty reason ([UiAppModel.h:251-254](../../src/ui/UiAppModel.h)).
  None is currently test-guarded.

---

## H12 findings (docs-only — read on the repo, in flight)

H12 has landed **only docs** (ADR-0033, the plan, `CONTEXT.md`, roadmap, horizon — both post-H11 commits
are verified `.md`-only). "No implementation code has landed yet" is accurate. The plan is well-structured,
honestly scoped, and internally consistent. Two real risks and one nit:

### H12-1 — The exit gate, as written, can repeat H11's "test the library, not the shell" gap (MEDIUM→HIGH as a forward risk)

The parity clause allows reachability "through the UI action registry, direct Component input, **or an
explicit harness command path**" ([h12 plan:16-18](../../docs/plans/2026-06-29-h12-operable-session-ux-plan.md)),
and the harness step says it can "open the **app model**, target named UI regions, run … gestures"
([:48-50](../../docs/plans/2026-06-29-h12-operable-session-ux-plan.md)). The "or … harness command path" /
"app model" escape hatch means a harness that drives the headless `UiAppModel` directly would satisfy
`YesDawUiInputCheck` **without ever touching `MainComponent`** — leaving the operable window as unverified
as it is today. `CONTEXT.md` blesses the same loophole: the harness runs "against the YES DAW app
**model/Components**" ([CONTEXT.md:363](../../CONTEXT.md)) — the slash permits model-only.
**Recommendation:** make the H12 exit gate **require** driving real `MainComponent` input (synthetic
mouse/key on the actual hit-tested Components), and tighten the wording from "app model/Components" to
"hit-tested Components." Otherwise H12 can close green with the same disconnected shell.

> The project already set itself the right guardrail — `CONTEXT.md:359` says *Avoid "calling a painted
> mockup or projection-only surface 'operable'."* That is precisely H11's shell. H12 should make that
> guardrail mechanical, not aspirational.

### H12-2 — The same "loads a Project bundle" overstatement seeds H12's premise (LOW)

Covered in H11-7; flagged here because ADR-0033 *opens* H12 on the claim that the window already loads
bundles, which could let the harness assume a starting capability the shell doesn't have.

### H12-3 — Green-command / citation lag (LOW, near non-issue)

The plan's "Green command" block omits the gate H12 actually closes on (`YesDawUiInputCheck`), which
doesn't exist yet ([h12 plan:27-37](../../docs/plans/2026-06-29-h12-operable-session-ux-plan.md)), and the
docs cite `7ad455e` as the remote-green tip while HEAD is the later docs-only `8025f59`. Both are benign
(self-consistent for a docs kickoff; `8025f59` is `.md`-only) but worth tidying when the harness lands.

---

## Fair credit — what H11 genuinely got right

The headless layer is real engineering, not theater, and several gates **do** bite:

- **Transport is really wired to `PlaybackEngine`** with state readback and overflow guards —
  play/stop/locate route through `playback_->play()/stop()/locate(0)`, loop computes from engine `frames()`
  with `INT64_MAX` guard, and `syncContextFromPlayback()` reads `isPlaying/loopEnabled/playheadFrame` from
  the live engine ([UiAppModel.h:138-160,262-270](../../src/ui/UiAppModel.h)).
- **Timeline + piano-roll edits are real and undoable** — `UiTimelineEditModel`/`UiPianoRollSurfaceModel`
  build `engine::ProjectEditCommand`s and `undo_.apply(...)`; tests assert concrete mutations
  (`clips[0].timelineStart == 256`, `notes[0].key == 67`), real undo/redo round-trips, and invalid-input
  rejection (`InvalidClipEnvelope`, `InvalidNoteValue`) that leaves the undo stack untouched
  ([ui_action_tests.cpp:380-478, 604-707](../../tests/ui_action_tests.cpp)).
- **`.yesdaw` bundle round-trip** in the smoke is genuine end-to-end persistence (write snapshot +
  hash-addressed assets, reload via `ProjectBundleDb`, build a `PlaybackEngine`).
- **Keymap** rebinds with `UnknownAction`/`EmptyChord`/`DuplicateChord` rejection and clears the old owner
  on reassign ([UiActions.h:305-318](../../src/ui/UiActions.h); tested ui_action_tests.cpp:211-228).
- **Disabled-state machine** gives specific, asserted reasons ("no project loaded", "no clip selected", …)
  rather than a bare bool ([UiActions.h:340-362](../../src/ui/UiActions.h)).
- **Clean action-ID/pixel separation** — `UiActions.h` is pure C++; shell and all gates resolve the same
  IDs. This is the right seam for agent-native parity; it just isn't bound to the window yet.
- **Engine reuse, not reimplementation** — mixer reuses `mixerGainIsValid`/`mixerPanIsValid`, the H3
  mute-policy functions, and the deterministic `projectMixerNodeIdForClip` hash (tests assert UI node ids
  match the engine's).
- **Rules respected** — no UI code touches an RT-hot / `YESDAW_RT_HOT` path (transport mutators are
  Control-thread SPSC posts); LF endings and warnings-as-errors (`yesdaw_harden`) apply to all UI targets.
- **H12 docs** are honestly scoped (explicit non-goals, H13 deferral) with sharp `_Avoid_` guardrails that
  name the exact failure mode to prevent.

---

## Recommendations (priority order)

1. **Add one gate that drives the real `MainComponent`** (headless via `MessageManager`/offscreen, or
   Xvfb in CI). Construct the shell, dispatch through its actual buttons, and assert an observable effect.
   This closes the H11-1/H11-2/H12-1 gap — the single highest-leverage fix. The project's own
   `ci-mechanical-verification.md:43` already specifies a version of this; build it (and fix the `600×300`).
2. **Make H12's exit gate require hit-tested Component input**, and remove the "or … harness command path"
   / "model/Components" escape hatch from the plan + `CONTEXT.md`. Otherwise H12 closes green on the same
   disconnected shell.
3. **Decide the mixer's status honestly:** either wire mixer edits through `ProjectUndoStack` like
   timeline/piano-roll (so they persist + undo), or relabel them in code/docs as a read-only projection.
   Today they're shaped like edits but behave like scratch state, and the gate's meter/loudness asserts are
   tautological — drive at least one meter/LUFS value from the real engine.
4. **Rename `YesDawTimelineGpuCheck` → `…TimelineFrameTimeCheck`** (or attach a real GPU context), and
   reword the roadmap's "GPU timeline" exit so it matches the CPU reality — or schedule the real-GPU soak
   the `TimelineLayout.h` header already promises. Either way, stop asserting a GPU path that doesn't exist.
5. **Harden the virtualization gate** before it's load-bearing: add a fixture that *hits* the 4096 cap
   (prove silent-drop is caught), one that breaks `id == index` (prove the O(n²) path is measured), and a
   vertical-scroll case (prove off-screen lanes are culled).
6. **Tidy the doc nits** (H11-7): the ADR-0033 "window loads a bundle" sentence, the stale Xvfb matrix row,
   and the three minor reporting quirks.

> **Bottom line.** H11's *foundation* is strong and genuinely tested; its *façade* is a mockup that no gate
> guards, and a few gate names/roadmap lines claim more than the code does. None of it blocks H12 — but H12
> should be the horizon that makes the window real **and** makes a gate prove it, rather than inheriting
> H11's habit of testing the library beneath the UI instead of the UI itself.
