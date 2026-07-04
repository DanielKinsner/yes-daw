# 0037. Alpha target and the H14–H19 re-carve

- **Status:** Accepted (2026-07-03, after the Codex adversarial review was applied)
- **Date:** 2026-07-03
- **Deciders:** Dan + Fable 5 (live grill session, 2026-07-03), informed by the
  [`docs/fable5/`](../fable5/yes-daw.md) plan-elicitation pack
- **Related:** ADR-0003 (product), ADR-0020 (H7–H11 carve — the precedent this follows),
  ADR-0032/0033/0035 (UI-era horizons), [`docs/goals/roadmap.md`](../goals/roadmap.md),
  [`docs/goals/risk-register.md`](../goals/risk-register.md),
  [`docs/reality-lane.md`](../reality-lane.md)

## Context

H0–H12 are closed remote-green and H13 (recording and device UX) is in flight. The roadmap beyond
H13 was a sketch: H14 "session production polish → alpha" and H15+ "plugin hosting deepening", with
no decided product goal to aim either at.

The 2026-07-03 planning session locked the product goal and surfaced one decisive gap:

1. **Product goal.** YES DAW is headed to a **distributable product** — one app in the YES family
   (YES Master, YES Voice, YES Stems ship separately today and later integrate with YES DAW as
   plugins). The nearest milestone is a **dogfood alpha**: Dan records, edits, mixes, and exports
   one real song with YES DAW on his machine. Alpha is deliberately the *minimum lovable cut* of
   the general-purpose vision (ADR-0003 scope is unchanged; alpha sequences it, it does not shrink
   it).
2. **The mix gap.** YES DAW has **no audio effects in any form** — built-in DSP is utility-only
   (fader/pan/sum/meter/time-stretch) and the plugin-hosting worker has only ever run passthrough.
   The "mix" step of record/edit/mix/export is impossible today, and the UI mockup (the H16
   structural spec) shows FX everywhere: channel inserts, Room Verb / Delay buses, a mastering
   limiter on the Master bus.
3. **The reality gap.** Everything is mechanically green, but almost nothing has touched reality:
   no real-hardware playback PASS has ever been recorded, no real third-party plugin has ever
   crossed the worker boundary, and the shipped shell has only ever run as an unoptimized
   CI-flavored build.

What is hard to reverse: horizon ordering spends months of loop time; the alpha definition decides
which subsystems get hardened first; and the FX/automation work introduces **data that enters saved
user projects** (parameter identity, automation lanes) — one-way doors that outlive any UI or DSP
iteration.

## Options considered

1. **Option A — UI first, then FX.**
   - Pros: visible progress; the mockup becomes real sooner.
   - Cons: builds a mixer with empty FX slots that must be reopened when FX land; UI is the one
     workstream that needs batched human review, while FX/automation are pure-headless agent work
     that can run unattended immediately.
2. **Option B — Real third-party hosting in alpha** (the `docs/fable5/` draft "shippable"
   definition includes "at least one class of real third-party plugin (VST3)").
   - Pros: a DAW that hosts real plugins is more credible to outside users; avoids writing DSP.
   - Cons: hosting is the shakiest subsystem in the repo (synthetic worker, passthrough only) and
     the least agent-friendly (real plugins misbehave in ways no CI gate predicts); the alpha user
     is **Dan**, not the public; the YES-family strategy needs first-party FX and a first-party
     parameter model *regardless*, so hosting-first would not remove the FX work, only delay it.
3. **Option C — First-party FX first; hosting deferred but reality-smoked now.** *(chosen)*
   - Pros: unblocks the mix step with work that is fully mechanically gatable (golden files, DSP
     property tests); gives PDC its first real nonzero-latency built-in consumer (the lookahead
     limiter), turning the H3 parallel-path alignment clause and H7's inert tail/PDC paths into
     live, biting tests; establishes the parameter-identity model the YES-family plugins will
     later standardize on.
   - Cons: alpha cannot host third-party plugins (accepted — see Consequences); five processors of
     first-party DSP to own and maintain.

## Decision

**Option C**, and the roadmap beyond H13 is re-carved as follows (each horizon opens docs-first
with its own focused plan, per house rules):

| Horizon | Scope | One-line exit |
|---|---|---|
| **H14 — Built-in FX suite** | Parametric EQ, compressor, delay, reverb, lookahead limiter as built-in Nodes; insert chains on Track/Bus strips; parameter model + persistence; equal-power crossfade (the deferred H2 item — it is DSP and belongs here) | Every FX gate green incl. null/response/ballistics/RT60/ceiling checks with negative controls; limiter PDC parallel-path alignment green; offline == RT with FX |
| **H15 — Automation** | Audit-first verification of the ADR-0009 runtime, then lanes: data model, persistence, undo verbs, block-ramp runtime, tempo interaction | An automated mix parameter renders to the closed-form expected curve, block-size- and tempo-robust, save/reopen and undo-property green |
| **H16 — Real UI** | Structural parity with the mockup (ruler markers, waveform clips, inspector, sends view, automation lanes, FX slots), async waveform cache, LookAndFeel/design tokens, batched polish pass ending in **one** human eyeball session | Mockup-inventory checklist mechanically covered by the UI input harness; async-cache and real-GPU frame gates green |
| **H17 — Distribution + Alpha** | Optimized Release preset, portable-zip packaging (unsigned), packaged-build smoke, demo-project fixtures | **The alpha gate:** Dan records/edits/mixes/exports one real song on the packaged build; close = mechanical sub-asserts (export exists, reopens, loudness sane) + reality-lane PASSes; the human feel session is recorded, non-gating |
| **H18 — Plugin hosting deepening** | The real VST3/CLAP road: scanner, identity, validation, blacklist UX, editor hosting | Defined by its kickoff ADR; preconditioned by the reality-lane worker smoke below |
| **H19+ — YES family integration** | YES Master / Voice / Stems as plugins inside YES DAW | Defined later; each app ships solo first |

**Standing reality lane (not a horizon).** Three one-command, self-asserting owner-machine smokes
run as they become available and their PASS/FAIL results are **committed** to
[`docs/reality-lane.md`](../reality-lane.md): (1) the H8 hardware playback smoke — which has never
recorded a PASS; (2) a one-real-VST3 worker-boundary smoke — load one real plugin in the
`YesDawPluginHost` worker and render through it, proving the process boundary against real
third-party code years before H18 builds on it; (3) a real-hardware recording smoke once H13
closes.

**Gates must BITE.** Every H14–H17 plan document carries a "Gates that must bite" section naming
its negative controls up front. This makes the failure mode that burned H1–H6 (gates written to go
green, caught only by later adversarial review) a plan-time requirement instead of a review-time
discovery.

**Alpha definition (v2 — supersedes the draft in `docs/fable5/yes-daw.md`):**

> *Alpha:* on Dan's Windows machine, from a **portable, unsigned, zipped Release build**, YES DAW
> can create a Project; import and record audio and MIDI with compensated latency; arrange and
> edit Clips and Notes; mix with the built-in FX suite plus volume/pan/send automation; and export
> a stereo master WAV — with autosave/crash recovery, save/reopen parity, zero Underruns in the
> headless gates, and recorded reality-lane PASSes for playback and recording on real hardware.
> **Explicitly out of alpha:** third-party plugin hosting (H18); code signing, installers,
> auto-update, crash telemetry, licensing (beta, post-H17 — when signing/packaging arrives, adapt
> the yes-master playbook rather than copying it; that app is Tauri/Rust, this one is C++/JUCE).
> Visual/audible feel is one batched human session at H16/H17 close (the sanctioned GUI-feel
> exception) — it informs the product and its findings become tracked tasks, but it never gates a
> mechanical close (adversarial review finding 7).

## Consequences

- **Positive:** the mix step is unblocked by loop-friendly headless work; PDC/tail machinery gains
  real consumers (H3's parallel-path clause and H7's inert paths become live tests); the parameter
  model lands once, before automation and before any YES-family plugin needs it; the reality lane
  converts the project's three biggest "green in CI, dead on a musician's machine" risks into
  cheap, dated, committed PASS/FAIL facts.
- **Negative / accepted costs:** alpha is not shippable to the public (no third-party plugins, no
  installer) — it is a dogfood gate, and that is the point; first-party DSP becomes a maintained
  surface; the hosting bet stays unproven beyond the worker smoke until H18.
- **Follow-ups:** ADR-0038 (FX suite), ADR-0039 (automation lanes); plans for H14–H17;
  `CONTEXT.md` terms (Insert, FX chain, ParamSpec, Automation lane, Breakpoint, Reality lane,
  Alpha); re-carved [`roadmap.md`](../goals/roadmap.md);
  [`docs/goals/risk-register.md`](../goals/risk-register.md) as a living table;
  [`docs/fable5/implementer-brief.md`](../fable5/implementer-brief.md) for the implementing model.
