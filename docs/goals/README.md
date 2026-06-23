# Goals & long-horizon roadmap

This folder holds the **long arc** of YES DAW — the horizons we work toward over weeks and months,
not the day-to-day tasks. It exists so that `/goals` and `/loop` always have a stable, durable target
to measure progress against, independent of any single conversation.

## Files

- [`roadmap.md`](roadmap.md) — the staged roadmap (horizons H0…Hn), each with a goal, exit criteria,
  and explicit "not yet" guardrails. **Currently a pre-brainstorm draft** distilled from the research;
  it will be revised once the product wedge (ADR fork #1) is decided.

## How horizons, goals, and loops relate

- A **horizon** (e.g., "H1 — Technical Prototype") is a milestone with a crisp *exit criterion*:
  one demonstrable, testable capability. You can only be "done" with a horizon by meeting it.
- A **goal** is a horizon's exit criterion phrased as an outcome ("multitrack audio plays through the
  node graph, sanitizer-clean, with metering").
- A **`/loop`** runs against the *current* horizon's open work until its exit criterion is met, then
  stops. Horizons are intentionally sized so a loop has a clear finish line and can't run forever.

## Guardrail

Every horizon lists what to **avoid** building yet. The research is blunt that scope creep — MIDI,
plugin hosting, time-stretch, collaboration — is what sinks small-team DAW efforts. The "not yet"
list is as load-bearing as the goal.
