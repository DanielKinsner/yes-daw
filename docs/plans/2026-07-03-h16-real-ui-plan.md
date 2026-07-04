# H16 — Real UI: focused plan

> Decisions: [ADR-0037](../adr/0037-alpha-target-and-h14-h19-recarve.md) (mockup = structural
> spec; polish batched into ONE human session), ADR-0032 (native JUCE + Timeline canvas + action
> registry — unchanged). Precondition: H14 + H15 closed (FX chains and automation lanes exist as
> data for the UI to surface). Guardrails:
> [`docs/fable5/implementer-brief.md`](../fable5/implementer-brief.md).

**Goal.** Take the H11–H13 shell to structural parity with the product mockup: every control in
the mockup exists and works through `UiActionRegistry`, waveforms are real, FX/automation are
surfaced, and the look is driven by a central design-token system — then a batched agentic polish
pass, ending in the single sanctioned human eyeball session.

**Mechanical exit criterion.** The mockup-inventory checklist below is fully covered by
self-asserting gates (every row's "gate" column green in CI); the async-cache gate proves the UI
thread never decodes or builds peaks; the token-audit gate proves no raw colors outside the theme;
the real-GPU windowed frame smoke records a PASS in the reality lane; accessibility and all H11–H13
UI gates stay green.

---

## Mockup inventory (structural spec → current state → gate)

Current state verified against HEAD 2026-07-03: `MainComponent.cpp` (~2k lines, shipped shell),
`TimelineCanvas.h` (draws **fake hash-based waveforms** — `(clipId·37 + x·13) & 31`),
`WaveformPeakCache.h` (real peak pyramid, synchronous, **no UI consumer**), `UiActionRegistry`
(40+ actions), `UiMixerSurface`, `UiPianoRollSurface`, inline colors in `TimelineCanvas.h`.

| Mockup element | Today | H16 work | Gate (all extend `YesDawUiActionCheck` / `YesDawUiInputCheck` unless noted) |
|---|---|---|---|
| Transport bar: timecode BAR\|BEAT, tempo, time-sig, loop | Partial (transport actions exist) | Display formatting + editing via actions | Input harness sets/reads each field |
| Ruler with section markers (Intro/Verse/…) | Markers in model since H1; not drawn | Draw + hit-test + add/rename/move via actions | Marker edit round-trip through harness |
| Track headers: M/S/arm, pan, fader, meters | Exists (H12/H13) | Visual alignment to mockup only | Existing gates stay green |
| **Real waveforms in clips** | **Fake** | See CP2/CP3 (async cache + rendering) | Column-extremes gate vs fixture cache |
| Clip inspector: start/end/length, gain, fades + curve pickers | Gain/fades exist (H12 closeout) | Add position/length fields, fade-curve choice (linear/equal-power per H14) | Harness edits each; save/reopen parity |
| Clip FX panel | Nothing (per-clip FX deferred — shows the **track** chain read-only context) | Track FX chain list: add/remove/reorder/bypass, param edit | Action → `fx_inserts` row assertions |
| **Automation lanes with curves** | Nothing (data model from H15) | Lane show/hide per track; draw/drag/delete Breakpoints (pencil tool) | Harness draws a ramp → Project rows match; undo/redo |
| Mixer: strips, sends view, FX slots | Strips exist; no sends view, no FX slots | Sends view (level = H15 send FaderNode), FX slot column, GR meters (H14 readbacks) | Harness edits send level + insert bypass; meter readout sanity |
| Master strip: LUFS readout, dim/mono | LUFS exists (H10/H11) | Dim/mono buttons as actions | Action toggles asserted in render output |
| Tool palette (pointer/pencil/scissors/…) + snap menu | Partial | Complete the tool set as actions + keymap | Every tool has an action ID + key + accessibility role (registry audit case) |
| Key/scale display | No model | **NOT YET** (deferred, ADR-0037) | — |
| Racks | — | **NOT YET** | — |

## Gates that must BITE

| Gate | Proves | Named negative control |
|---|---|---|
| Token audit (`YesDawThemeAuditCheck`, new — a CTest script scanning `src/ui/`) | No raw `0x????????` colors or magic spacing outside `UiTheme.h` | Seed one inline color in a scratch branch case → scan fails |
| Async cache (`YesDawWaveformCacheCheck`, new) | Peaks build off the UI thread; paint never blocks past budget; tiers persist to `peaks/*.ypeaks` and reload | Force a synchronous build in the paint path → thread-identity assertion fails |
| Waveform columns | Rendered min/max columns match `WaveformPeakCache` fixture values at each zoom tier | Off-by-one tier selection → fails |
| Mockup-inventory rows | Each row's interaction works end-to-end through real input paths | Each edit case asserts the *Project/engine* state, not widget state (H12 pattern) |
| Agent-native parity | Every new UI affordance has an action ID (no click-only paths) | Registry audit: a Component found by the harness without a mapped action fails |
| Real-GPU frame smoke (`tools/ui-frame-smoke.ps1`, reality lane) | The 20k-clip fixture scrolls under budget in a real window on the owner machine | Headless `YesDawTimelineGpuCheck` stays as the CI proxy; the windowed smoke logs PASS/FAIL to `docs/reality-lane.md` |

## Checkpoints

**CP1 — Design tokens.** `src/ui/UiTheme.h`: named tokens (color roles, spacing scale, type
ramp, meter gradients) extracted from the mockup; migrate `TimelineCanvas.h`/`MainComponent.cpp`
inline constants; `YesDawThemeAuditCheck` (source scan as a CTest).

**CP2 — Async waveform cache.** A control-thread worker (single background thread owned by
`UiAppModel`) builds `WaveformPeakCache` per Asset on import/open, persists to
`peaks/<hash>.ypeaks` (path helper exists), publishes ready tiers atomically; UI polls
ready-state, paints placeholder until ready. Thread-identity assertion hook makes any UI-thread
peak-build a test failure. `YesDawWaveformCacheCheck` per the bite table.

**CP3 — Real waveform rendering.** Pure `computeWaveformColumns(cache, viewport) →
min/max/rms columns` (tier chosen by pixels-per-second), unit-gated against fixtures;
`TimelineCanvas::drawClipWaveform` consumes it, replacing the hash fake.

**CP4 — Ruler, markers, transport display, tools, snap, keymap completion.** Per inventory rows;
every new interaction is an action ID first (agent-native parity), then a Component.

**CP5 — Inspector + automation lane editing.** Inventory rows 5–7; the pencil-drawn Breakpoint
path goes through the H15 undo verbs; fade-curve picker exposes the H14 equal-power option.

**CP6 — Mixer buildout.** Sends view, FX slots, GR meters, dim/mono; all through actions;
`UiMixerSurface` extends its projection (readbacks stay readbacks — no engine policy in UI).

**CP7 — Export dialog + progress.** Surface H7 offline render with format fixed (canonical WAV),
destination picker, progress + cancel; headless-gated through the harness (choices injected like
`MainComponentFileChoices`).

**CP8 — Polish pass + human session.** (a) One-command screenshot script (`tools/ui-screenshot.ps1`:
launch, load demo fixture, capture PNGs per surface, exit). (b) Batched agentic iterate loop:
screenshot → compare against mockup → adjust **tokens and layout only** → repeat (design-iterator
style; no engine or model edits allowed in this loop). (c) The windowed frame smoke records its
reality-lane PASS. (d) **One human eyeball session** with a written checklist (each mockup surface
side-by-side); findings become token/layout fixes or explicitly deferred items — not open-ended
iteration.

## Not yet (guardrails)

Racks; key/scale display; per-clip FX UI; plugin editor windows (H18); theming/user-customizable
themes (tokens are internal); localization; touch/latch/write automation recording; any engine or
Project-model change beyond what H14/H15 landed (UI consumes, it does not redefine).
