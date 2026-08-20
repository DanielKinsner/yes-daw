# Visual parity & dogfood execution plan (2026-08-20)

**What this is.** The decision-complete long-horizon plan from the 2026-08-20 planning session
(Fable 5 + Dan). It chains three phases: finish the open **N-run** (N3–N8), then a new
**V-run** that makes the arrangement view look and work like the reference image Dan posted, then
**dogfood prep** so Dan can open the app and edit a real song. It is written for an executor agent
that was NOT in the planning conversation; everything needed is in this file plus the repo docs it
names.

**Drift rule (read first).** If this plan contradicts current code/docs reality, verify reality,
follow reality, and note the discrepancy in the deviation log — do **not** improvise a new decision.
Claims in this plan were verified against `main` at `2cc0656` on 2026-08-20; the repo moves, the
plan doesn't.

**Process protocol.** The house rules apply verbatim and are NOT restated here: read
`docs/goals/2026-08-11-overnight-backlog-run-brief.md` and the 2026-08-12 brief before touching
anything. One item at a time, strict order, audit-before-build, shipped-boundary gates that fail
before and pass after, full local ctest with the owner-file isolation ritual, one feature commit +
exact-head nine-job CI green + a docs-only evidence commit per item, stop after 3 consecutive red
CI rounds on one item. Per-slice verify lists below are a **floor, not a ceiling** — after each
slice, re-check the repo's own conditional verify rules against the ACTUAL diff; a slice that grows
beyond its predicted footprint grows its verify obligations with it.

---

## Decisions locked (D-table)

| # | Decision | Consequence |
|---|---|---|
| D1 | **Order is: N3–N8 first, then V-run, then dogfood prep.** | No reordering. The N items build surfaces the V-run restyles (mixer layout N3, automation lane N4, track height N6, track colour N7). |
| D2 | **The visual truth for the arrangement view is `docs/design/arrangement-view-reference.png`.** "Match" means structural/layout/legibility parity judged element-by-element at real window sizes — NOT a pixel clone. | The older `src/ui/yes-daw-ui-gold standard mockup.png` is **superseded** for the arrangement view. Legacy gates pinned to the old mockup's semantics may be re-pinned to the new reference (house rule: re-pin to new semantics, never weaken). |
| D3 | **No fake data, ever.** If the reference shows a cell whose backing model doesn't exist (e.g. a key-signature display, track icons), ship the honest subset — omit the cell — and say so in STATUS.md. | The transport bar ships without a Key cell unless a key model exists; track headers use the N7 colour chip + name instead of instrument icons. Do NOT invent placeholder values. |
| D4 | **All three owner reality-lane items are deferred**: Smoke 2 (VST3 install), Smoke 3 (hardware loopback), the Smoke 1 ASIO question. Nothing in this plan blocks on them. H18 stays unopened. | The executor never waits on hardware. If Dan runs a smoke mid-plan, log the row per `docs/reality-lane.md` rules; it changes nothing in this plan. |
| D5 | **Same-track clip overlap stays as-is (summing).** The policy decision is parked, to be re-asked at the dogfood checkpoint with the standing recommendation (Logic-style later-wins behind a toggle). | Do not implement any overlap policy in this plan. |
| D6 | **Single-window topology for the arrangement view**, per the reference: track headers left, ruler + clip lanes center, **clip/track inspector right** (one already exists — `UiTheme.h` `inspectorWidth = 320`), **mixer dock bottom** (toggleable). The full-size Mixer panel remains a separate view; N3 applies to it unchanged. | The V-run adds/aligns the bottom mixer dock inside the arrangement view, reusing N3's strip-layout law — it does not fork a second strip implementation. |
| D7 | **Visual judgment loop**: render the REAL shipped shell at 1152×720, 1536×960, and 1920×1080 with `YESDAW_UI_SCREENSHOT_DIR` set (stale-PNG trap — see N1's process note in STATUS.md), judge against the reference yourself, iterate until it genuinely reads like the reference, THEN lock mechanical token/layout gates. | Screenshots judged without the env var set are invalid evidence. |
| D8 | **Dan's only gates are two screenshot verdicts** (checkpoints CP-B and CP-C below) **and the dogfood session itself.** Every other decision in this plan is pre-made or the executor's technical judgment. | Never send Dan a technical question. If something needs a product decision not in this table, that's a stop-trigger (below), phrased in plain English with lettered options and a recommendation. |
| D9 | **Out of scope for this whole plan**: MIDI CC/pitch-bend/sustain, H18 plugin hosting, ASIO backend, comping UI beyond the existing Comp button, time-stretch, tab-to-transient, ripple editing, the reference's SENDS/RACKS side tabs. | See parking lot. Do not drift into these even when adjacent code invites it. |
| D10 | **Dogfood launch path**: a one-command `tools/run-yesdaw.ps1` that proves freshness (pull state, rebuild via the `ci` preset with the vcvars64 environment, print the built git SHA) and launches the `YesDaw` GUI app, plus a plain-English `docs/HOW-TO-RUN.md`. | Dan has never launched the app; "double-click one script" is the acceptance bar. A stale-build launch is the known failure mode this script exists to kill. |

---

## Lanes

**Owner lane (Dan, date-gated):**
- CP-B screenshot verdict (after V-run topology+theme slices) — reply "closer" or "not right because X", one sentence.
- CP-C screenshot verdict (V-run complete) — same shape.
- The dogfood session itself (after CP-C approval): open the app via `tools/run-yesdaw.ps1`, import stems from a real song, edit, and note every friction point in plain words. No format required.
- *(Optional, unblocks a different track, not this plan)*: install one free VST3 and run `pwsh tools/plugin-smoke.ps1`.

**Executor lane:** everything else in this file.

**C-fallback (pre-authorized):** if Dan is unavailable ≥3 days at CP-B or CP-C, proceed on your own
judgment against the reference, mark the checkpoint "self-judged, Dan verdict pending" in STATUS.md,
and continue — Dan's verdict then applies retroactively and may reopen items. If Dan is unavailable
for the dogfood session, carve the next backlog from a fresh adversarial audit instead (the
"possibility of C" Dan approved).

---

## Phase 1 — finish the N-run (N3–N8)

The specs, gates, and evidence rules live in
`docs/goals/2026-08-15-mixer-surface-and-arrangement-backlog.md` and are NOT duplicated here.
Execute N3, N4, N5, N6, N7, N8 in order, exactly per that doc.

Two plan-level additions:
- **N3 note:** N3's "judge against Logic's mixer" now also means "judge against the bottom mixer
  dock in the reference image" — same strip anatomy (pan knob + value, S/M, fader with dB scale,
  live meter, numeric dB readout, master strip at right). Build N3's layout law so the V-run's
  bottom dock (D6) can reuse it at a shorter strip height; don't hardcode the panel's height into
  the law.
- **CP-A (after N8, non-blocking):** post before/after screenshots of the mixer, automation lane,
  and a colourised 3-track arrangement to STATUS.md's evidence paragraph and continue into Phase 2
  without waiting.

## Phase 2 — the V-run (arrangement view visual parity)

### Step 1 — audit and carve (docs-only)

Fresh adversarial audit of current `main` against `docs/design/arrangement-view-reference.png`,
reading the real paint/control code AND judging fresh screenshots at the three D7 sizes. Then carve
`docs/goals/<date>-arrangement-view-visual-parity-backlog.md` with items **V1–Vn** in shippable
order, each with file:line evidence and a shipped-boundary gate, per house style. The carve MUST
cover this element inventory (the locked definition of "matches the reference") — fold items that
turn out already-done into the audit evidence instead of carving them:

1. **Theme & typography pass** — near-black panel palette, one consistent type scale, legible at
   all three sizes. Tokens live in `src/ui/UiTheme.h`; `YesDawThemeAuditCheck` gates tokens today.
2. **Transport bar** — return-to-start / play / stop / record cluster; large time readout
   (bar|beat primary); tempo, time-sig, loop toggle as labelled cells; master meter + LUFS readout
   at the right end. Key cell per D3 (omit if no model).
3. **Track headers (left rail)** — track number, N7 colour chip, name, M/S/record-arm as real
   buttons, pan knob, mini-fader, live per-track stereo meter, add-track affordance, N6 drag-resize.
4. **Ruler** — bar numbers, section markers, loop region, playhead — restyled to the reference's
   proportions. (Marker/loop editing verbs shipped in the E-run; this is presentation.)
5. **Clips** — waveform clips tinted by N7 track colour, clip name label, fade in/out drawn as
   curves on the clip body, MIDI clips with note-preview rendering, unmistakable selection state.
6. **Right inspector** — Clip/Track tabs; clip name, start/end/length readouts; gain; fade in/out
   with curve display; the clip/track FX list; automation target + mini curve preview. An inspector
   surface exists (`UiTheme.h` inspector tokens; `clip.inspector` in `UiAccessibility.h`) — audit
   what's real, close the gap to the reference.
7. **Bottom mixer dock** (D6) — toggleable dock inside the arrangement view reusing N3's strip law;
   selected-strip highlight follows the timeline selection (N2's `readoutStripFor` law is the
   selection source of truth).
8. **Toolbar row** — tool palette (E3's tools), snap chooser (E4's law), zoom control, surfaced as
   the reference's single toolbar row instead of scattered controls.

### Step 2 — execute V1–Vn

One item at a time per the house protocol. Every visual item: D7 judgment loop first, mechanical
gate second. **CP-B fires after the first topology+theme wave lands** (the carve must mark which
items constitute that wave — at minimum inventory items 1, 2, and the D6 dock skeleton): stop,
send Dan the three-size screenshots beside the reference, wait per the C-fallback clause.
**CP-C fires when Vn lands**: same shape, full-view screenshots.

## Phase 3 — dogfood prep

- **S3.1 `tools/run-yesdaw.ps1`** (D10): mirror the build half of `tools/package.ps1` (vcvars64 +
  `ci` preset Release — see the `local-build-staleness-and-vcvars` memory note; `ninja` can claim
  "no work to do" after a pull without the right environment), print `git describe --always --dirty`
  + build timestamp, launch the `YesDaw` exe non-blocking, exit 0 only if the process started.
  Verify: run it twice (cold + warm) on the executor machine; both launch and print the SHA.
- **S3.2 `docs/HOW-TO-RUN.md`**: plain English, no jargon — how to launch (the one script), how to
  make a project, import stems (drag or import verb), and where to write friction notes
  (`docs/dogfood/2026-XX-XX-dan-session-1.md`, a dated file with a blank bullet list). Include the
  one-paragraph "if it looks broken, run the script again and tell me the SHA it printed" freshness
  ritual.
- **S3.3 Dogfood readiness check**: from a packaged or fresh `ci` Release build, walk the exact
  path Dan will: new project → import 3+ real stems (use the repo's fixture WAVs) → split/move/fade
  → save → reopen. This is a self-asserting script or a gated test, not a hand-claim. Fix what it
  finds before handing over.
- **S3.4 Handover**: update STATUS.md ("ready for Dan"), commit, push, and write Dan a 5-line
  plain-English kickoff (what to run, what to try, where to gripe).

After Dan's session: his friction notes are the carve source for the next backlog. That carve is a
NEW planning moment — not covered by this plan.

---

## Do-not-touch (absolute)

- ADRs, golden outputs, `[[clang::nonblocking]]` annotations, `.github/workflows/ci.yml`.
- Never weaken or delete an existing gate (re-pinning to new semantics per D2 is allowed).
- The reference image `docs/design/arrangement-view-reference.png` — never edit, replace, or
  "improve" it.
- `docs/reality-lane.md` result rows (append-only; owner smokes are Dan's to log).
- Engine RT rules (ADR-0002): no UI work may add allocation/locks/logging/IO to the audio thread —
  `YesDawRtSafetyCheck`/RTSan gate this; meters and readouts stay on the existing lock-free paths.

## Stop-and-ask triggers (the ONLY reasons to contact Dan)

1. CP-B and CP-C screenshot verdicts (subject to the C-fallback clause).
2. A product decision not in the D-table becomes unavoidable (e.g. overlap policy stops being
   parkable). Ask in plain English, lettered options, one "(Recommended)".
3. The reference image conflicts with a locked ADR or an existing law in a way D2/D3 can't resolve.
   Present the conflict, don't pick silently.
4. 3 consecutive red CI rounds on one item (house rule — stop and report).

Everything else: execute without asking.

## Deviation log (required)

At every checkpoint (CP-A/B/C and each phase boundary), record in STATUS.md: where this plan was
unclear or wrong and what you did about it — every judgment call, every plan claim that didn't
match reality, every ripple beyond a slice's predicted footprint. Silent deviations are the failure
mode; logged deviations are how the next plan gets better.

## Parking lot (explicitly NOT this plan)

- Same-track clip overlap policy (D5 — re-ask at dogfood; recommendation on file).
- MIDI CC / sustain / pitch-bend / aftertouch model.
- H18 plugin hosting (gated on Dan's Smoke 2 run; needs its own kickoff ADR).
- ASIO backend (owner decision pending).
- SENDS/RACKS side tabs from the reference's mixer dock.
- Track instrument icons and a key-signature model (D3 honest-subset omissions — candidates for a
  later backlog, not sneak-ins).
- STATUS.md history archiving (housekeeping; do only if Dan asks).
