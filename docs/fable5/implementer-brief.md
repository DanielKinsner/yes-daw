# For the implementing model — guardrails for the H14–H17 packet

> Stage-4 handoff artifact (see [`framework.md`](framework.md)). Audience: the cheaper model
> (baton loop) executing the H14–H17 plans. Assume you see only this file, the plans/ADRs it names,
> and the repo — not the planning conversation. House rules in
> [`CLAUDE.md`](../../CLAUDE.md) apply in full; this adds the packet-specific hard-stops.

## Standing hard-stops (unchanged, restated)

1. Commit only when the relevant gates are green; every commit independently green (`git bisect`
   must work). Small commits, straight to `main`, never squash.
2. Never edit an **Accepted** ADR, a **golden file**, or a `[[clang::nonblocking]]` /
   `YESDAW_RT_HOT` annotation. To change a locked decision: new superseding ADR, Dan decides.
3. Verification is mechanical (exit 0/1). Never write a checkpoint whose acceptance is "a human
   reviews/listens/watches."
4. Update `STATUS.md` (done / Now / Next) before each commit; push at session end; one baton
   successor only after your own CI result is green.

## New hard-stops specific to this packet

5. **ParamIDs are append-only forever.** Once a ParamSpec row (ID, name, unit, range, mapping,
   default) lands on `main`, never renumber, rename, re-range, or reuse it — these enter saved
   user projects. Additions are fine; changes require a superseding ADR.
6. **The FX-era fixture bundle is a golden.** The first FX-schema-version `.yesdaw` fixture,
   once committed, is never regenerated. The forever-gate that opens and re-renders it must stay
   green against current HEAD.
7. **Never widen a tolerance to make a gate pass.** A DSP gate tolerance is a decision recorded in
   the plan. If a gate fails inside the stated tolerance policy, the code is wrong — fix the code.
   If you believe the tolerance itself is wrong, stop and flag it in `STATUS.md`; do not adjust it
   in the same change that makes it pass.
8. **Negative controls land in the same commit as the gate they guard** — a gate without its named
   negative control is not done. The controls are named in each plan's "Gates that must bite"
   section; implement them as written or stop and flag.
9. **No taste decisions.** The plans specify algorithms, topologies, parameter ranges, and
   mappings. If a spec is ambiguous or looks wrong, stop and record the question in `STATUS.md`
   rather than choosing an alternative silently.
10. **Reality-lane results are owner-only.** Never write a row into
    [`docs/reality-lane.md`](../reality-lane.md)'s result log from CI or from your own run; only
    the smoke script running on Dan's machine appends results.
11. **Sequencing.** Do not open H14 implementation until (a) H13 is closed remote-green and (b)
    ADR-0037/0038/0039 flip from Proposed to Accepted after external (Codex) review — that flip is
    Dan's call, recorded in `STATUS.md`.
12. **Equal-power crossfade goldens.** H14's crossfade-law change will invalidate existing fade
    goldens. Follow the plan's explicit golden-update procedure (its own dedicated commit, listing
    every regenerated golden and why) — never regenerate goldens as a side effect of another
    change.
