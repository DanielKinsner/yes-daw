# YES DAW — Fable 5 plan-elicitation pack

> Instance of [`framework.md`](framework.md) for YES DAW. Copy each stage's prompt into Fable 5 in
> order, feeding the previous stage's output into the next. Review between stages; stop early once
> you've mined the gold.

## Recommended dials for YES DAW

- **Altitude: whole-arc-to-shippable (high).** Your per-horizon detail is already well-served by your
  ADR → plan → CI machinery and cheap models. Fable 5's premium is best spent on the *strategic arc
  from H13 → shippable*, the still-shallow areas (real plugin hosting; real-hardware/device reality),
  and the framing question of what "shippable general-purpose DAW" even means for a solo builder.
- **Early-exit: run Stage 1 always.** It alone may reset your roadmap's ordering. Only spend on 3–4
  if 1–2 earn it.

## Stage 0 — Context Pack (assemble, then paste/attach to Fable 5)

Give Fable 5 these, in this order (curated high-signal set — do **not** dump all 36 ADRs):

1. **Constitution** — [`CLAUDE.md`](../../CLAUDE.md) (hard rules, how-we-work, verification-is-mechanical).
2. **Vocabulary** — [`CONTEXT.md`](../../CONTEXT.md) (so it uses your exact terms).
3. **Roadmap + status notes** — [`docs/goals/roadmap.md`](../goals/roadmap.md) (H0–H15+ with exit
   criteria and the brutally honest "STILL OPEN" notes — this is your single highest-signal file).
4. **Decisions ledger** — [`docs/adr/README.md`](../adr/README.md) (the ADR index + the five research
   forks), plus the *full text* of the architecture-defining ADRs the arc ahead touches:
   **0002** (RT foundations), **0007** (CompiledGraph + PDC), **0008** (Node contract),
   **0013** + **0015** (plugin state + out-of-process hosting runtime), **0020** (H7–H11 horizons),
   **0031** (device hot-swap), **0033** + **0035** (H12/H13 UX). The rest by index title only.
5. **Master build plan** — [`docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md`](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md).
6. **Live handoff** — the **top live packet** of [`STATUS.md`](../../STATUS.md) (current horizon +
   Now/Next). It's ~290 KB of history — paste only the current section, not the whole file.
7. **Your one-paragraph definition of "shippable"** (draft below — edit it before you send).

> Token tip: if the pack is too big for one turn, send Stage 1 with items 1–3 + 7 (constitution,
> vocabulary, roadmap, shippable-def) and the ADR *index*; attach full ADR text only at Stage 3.

## Draft "definition of shippable" (edit this — it's the target Fable 5 plans toward)

> *Shippable (v1 / alpha):* a solo musician can install YES DAW on Windows or macOS, create a
> project, import and record audio + MIDI, arrange and edit clips and notes, mix with the built-in
> nodes **and at least one class of real third-party plugin (VST3)**, and export a finished stereo
> master to WAV — with autosave/crash recovery, no data loss across save/reopen, and no audio-thread
> underruns on typical hardware. Everything is mechanically gated in CI except final visual/audible
> feel, which is a one-command self-asserting smoke on the owner's machine. Stem-separation and
> mastering stay separate apps (ADR-0003).

---

## Stage 1 — Blind-Spot Finder  *(cheap recon — run this first, always)*

```
You are the most capable reasoning model I have access to, and my access to you is scarce and
expensive — I get a limited number of turns over the next 7 days. So do NOT spend this turn on
anything a cheaper model could do: no task breakdowns, no restating best practices, no summarizing
what I've given you.

I'm building YES DAW: a from-scratch, general-purpose, multi-track DAW (Logic / Pro Tools / Cubase /
Sonar class) in C++/JUCE 8 with our own audio engine. I've attached the constitution (CLAUDE.md), the
vocabulary (CONTEXT.md), the roadmap with brutally honest status notes (roadmap.md), the decisions
ledger (ADR index), and my one-paragraph definition of "shippable." Read them as ground truth: the
product, stack, and foundational architecture are DECIDED and I am not relitigating them.

Where I am: the engine is built headless-first through H10 (concurrency spine, CompiledGraph + PDC,
mixer, offline render, playback runtime, scheduler, loudness/interchange/time-stretch). The native UI
shell is operable (H11–H12). I'm in H13 (recording + device UX). Ahead: H14 = session polish → alpha;
H15+ = deepening real plugin hosting (today it's "real-but-shallow" — a synthetic in-process worker).

**Do NOT produce a plan.** This turn has exactly one job: find what I can't see. Give me, ranked by
leverage (highest first), and terse:

1. **The decisions I don't know I need to make** between here and a shippable product — especially
   irreversible ones, and ones my roadmap silently assumes.
2. **Where this architecture bites me before I ship** — the load-bearing assumptions most likely to
   break under real use, and the "green in CI but dead on a musician's machine" gaps.
3. **The 10 highest-leverage questions I am NOT asking you** — the ones a great DAW architect would
   want me to ask, that aren't on my roadmap.
4. **The soft spots in my "shippable" definition** — what it quietly omits, over- or under-scopes.
5. **What you'd need to know (that I haven't told you) to plan the rest of this well.**

Consider — among others, don't limit yourself — whether "general-purpose DAW" is the right ship
target for a solo builder or a scope trap; whether real plugin hosting is mis-sequenced *after* H14
when a DAW that can't host third-party plugins arguably isn't shippable; the gap between
"underruns==0 headless" and real audio interfaces/drivers/buffer sizes; whether a mechanical-CI-only
verification philosophy has a ceiling as the subjective UX "last mile" grows near ship; and the
unglamorous ship blockers that aren't "engine" (code signing/notarization, installer, auto-update,
crash telemetry, licensing, file-format version guarantees, accessibility, localization).

Be blunt. If my framing is wrong, say so and say why. Rank by "what will hurt most if I miss it."
```

---

## Stage 2 — Open Exploration  *("let it cook")*

```
Here are your blind-spot findings from the previous turn: [PASTE STAGE 1 OUTPUT].

Now open it up. Same scarcity rule — spend this turn on judgement only I can't get cheaply.

Given everything you know about YES DAW and those blind spots, map the space between where I am
(H13) and a shippable v1/alpha. I want you to think freely and challenge me, not to fill in my
roadmap:

1. Propose **2–3 distinct strategic arcs** to shippable — genuinely different sequencings, not the
   same plan re-lettered. For each: the core bet, the order of horizons/workstreams, what it
   deliberately cuts or defers, and the trade-off (time-to-ship vs completeness vs risk).
   At least one arc should seriously explore a **"minimum lovable DAW"** cut that ships narrower and
   sooner; at least one should explore **pulling real plugin hosting earlier** if you think it's on
   the critical path.
2. Tell me **where my current roadmap ordering (H14 polish → H15+ plugin hosting) is wrong**, if it
   is, and what you'd resequence.
3. Name the **2–3 highest-risk bets** in the whole endeavour to shippable, and for each, the cheapest
   experiment that would tell me early whether it holds.
4. Give me your **recommended arc and why** — the one you'd bet on if this were yours.

Disagree with me where the evidence says to. I'd rather be corrected now than after I've spent
budget building the wrong plan.
```

---

## Stage 3 — Narrow Drill  *(precision — after you've picked an arc)*

```
I'm going with this arc: [NAME / PASTE THE CHOSEN ARC FROM STAGE 2].

Now go deep and precise. Constraints you must respect (they are locked; violating one is a decision,
not a detail):
- The audio thread never allocates, locks, logs, or does I/O (enforced by RTSan; annotated with
  [[clang::nonblocking]] / YESDAW_RT_HOT — never weaken those annotations).
- Routing is a DAG; per-node latency + plugin delay compensation exist from day one.
- Built-in DSP and hosted plugins share one format-neutral Node contract.
- Events are sample-accurate and block-sliced. Clips reference Assets non-destructively.
- Verification is mechanical: every checkpoint is gated by CI / a self-asserting test (exit 0/1),
  never "a human reviews the diff." The one allowed human check is visual/audible *feel*, via a
  one-command self-asserting smoke.
- ADRs are append-only. To change a locked decision, write a NEW ADR that supersedes the old one and
  say why — do not silently contradict an accepted ADR.

Produce, for my chosen arc:

1. **The irreversible decisions it forces, as ADR stubs** — one per decision, in my ADR shape
   (Context → Options weighed → Decision → Consequences), Status: Proposed. Number them from the next
   free ADR number, [NEXT ADR # = 0037]. Flag any that supersede an accepted ADR.
2. **A risk register** for this arc: each risk → likelihood/impact → the earliest mechanical signal
   that it's going wrong → the mitigation.
3. **The next 1–2 horizons in implementation-grade detail**, each with a single **mechanical exit
   criterion** phrased the way my roadmap phrases them (a self-asserting check that returns
   PASS/FAIL), the CI gates it adds, and the "not yet" guardrail of what it must NOT build.
4. For anything that can't be mechanically verified, **the specific self-asserting smoke script** (its
   inputs and its PASS/FAIL assertion) that gets it as close to mechanical as possible.

Be concrete enough that a cheaper model could open a plan file and start executing without guessing.
```

---

## Stage 4 — Structured Handoff  *(package for the implementing model)*

```
Assemble everything from the previous stages into a handoff pack a cheaper implementing model can
execute against, in my repo's house format. Output these as clearly separated, self-contained
artifacts I can drop into the repo:

1. **ADRs** — ready for docs/adr/NNNN-*.md, numbered from [0037], Status: Proposed, in my template
   (Context / Decision / Consequences), each superseding-note explicit where relevant.
2. **Horizon plan(s)** — ready for docs/plans/, in the style of my existing horizon plans: goal,
   mechanical exit criterion, checkpoint list (each an independently green, committable unit), CI
   gates, and the "not yet" guardrail.
3. **A risk register** — a standalone table.
4. **A "for the implementing model" brief** — the guardrails a cheaper Claude model running an
   autonomous /loop must never violate on this work. Mirror my existing hard-stops: commit only when
   CI is green; each commit independently green (git bisect); never edit accepted ADRs, golden
   files, or [[clang::nonblocking]] annotations; mechanical verification only; update STATUS.md and
   commit small at each checkpoint. Add any new guardrails this specific work needs.

Keep each artifact self-contained — assume the implementer sees only that artifact plus the repo, not
this conversation.
```

---

## Stage 5 — Red-Team  *(optional — before you trust the plan)*

```
Here is the plan pack you just produced: [PASTE STAGE 4 OUTPUT].

Attack it. Assume a smart, skeptical DAW architect reviews it and wants it to fail. Give me:
1. The **3 most likely ways this plan fails** in practice — be specific about the mechanism.
2. For each, the **earliest cheap signal** I'd see it failing, and the change that de-risks it.
3. Anything in the plan that is **confidently stated but actually uncertain** — where I'm being sold
   false precision.
Then give me the single highest-value change you'd make to the plan.
```

---

## Strategic tensions pre-loaded into these prompts (so you know what to watch for)

The prompts deliberately steer Fable 5 at the cross-cutting questions only it does well. When you
read its answers, these are the ones worth the most attention:

1. **"General-purpose DAW" vs a shippable cut.** The research warned scope creep sinks small-team
   DAWs; ADR-0003 chose full scope anyway (you already ship adjacent apps). Where's the minimum
   lovable cut to a *first* ship?
2. **Plugin-hosting sequencing.** Roadmap defers deep hosting to H15+, but a DAW that can't host real
   VST3s isn't shippable to real users, and today's hosting is synthetic/passthrough. Is it on the
   critical path and mis-scheduled?
3. **Mechanical-verification ceiling.** It's carried you brilliantly through a headless engine. The
   subjective UX/feel surface grows toward ship — does the philosophy have a last-mile ceiling?
4. **CI-green vs works-on-a-real-rig.** `underruns==0` is a headless choice; real-device soak is an
   owner-machine smoke, not CI. Latency calibration/monitoring are the newest, least-proven. This gap
   is where DAWs die.
5. **Solo-builder + agentic-loop sustainability.** Support, plugin-compat matrix, crash telemetry,
   signing/notarization, installer, auto-update, licensing, file-format version guarantees — none are
   "engine," all block "product."
