# Current horizon — H3 (Mixer + plugin hosting)

> This file is the **oracle for "is the horizon done?"** — the thing that was missing and let stragglers
> through. The horizon closes **iff** the exit gate below is green. Not a judgment call.

## Exit criterion (the finish line)

A crashing/hanging plugin is **isolated with no audio dropout**, and a hosted plugin stays **sample-aligned**
under PDC — proven by a deterministic in-repo CI gate (not external plugins):

**`YesDawHostIsolationCheck`** (built in item 1 of the close-out plan). It runs an in-repo synthetic test
plugin as a **real hosted `juce::AudioProcessor` in the worker child**, inside a `buildMixerGraphProjection`
graph published through `Runtime`, and asserts (each clause negative-controlled):
- **(a)** tri-stream PDC (audio impulse + automation ramp + event) lands at the compensated sample through
  the hosted plugin;
- **(b)** crash/hang → running-watchdog kill (child PID killed) → auto recompile swapping a
  `CompiledNodeKind::Placeholder` at the offender's slot → blacklist row survives restart; audio fails open
  (RTSan-clean, deadline-miss counter == 0); emit-NaN stays finite; impossible latency quarantined;
- **(c)** opaque plugin-state chunk round-trips across the real process boundary (crc32/len validated).

`pluginval L8–10` + `auval` run **non-blocking** where runners have real plugins; the in-repo gate is the
**always-on blocking** gate (ADR-0015).

## Green command

```
cmake --preset ci && cmake --build --preset ci && ctest --preset ci          # whole suite green, AND:
ctest --preset ci -R YesDawHostIsolationCheck                                 # the exit gate green
```

## Status: **RED (by design)**

`YesDawHostIsolationCheck` **does not exist yet** — building it is **item 1** of the close-out plan. Write it
as a Catch2 `[!shouldfail]` gate so `main` stays green while it is correctly-red, and **remove `[!shouldfail]`
(flip to blocking) the moment the hosting makes it pass.** H3 is done when it is green **and** an independent
adversarial review signs off **and** the close-out plan's Acceptance checklist is fully ticked.

## The plan

Full build order, every subsystem, every finding/deferral dispositioned:
[`docs/plans/2026-06-26-h3-close-out-plan.md`](../docs/plans/2026-06-26-h3-close-out-plan.md).

**Do not close H3 — or start H4 — until the gate above is green.**
