# Current horizon — H3 (Mixer + plugin hosting)

> This file is the **oracle for "is the horizon done?"** — the thing that was missing and let stragglers
> through. The horizon closes **iff** the exit gate below is green. Not a judgment call.

## Exit criterion (the finish line)

A crashing/hanging plugin is **isolated with no audio dropout**, and a hosted plugin stays **sample-aligned**
under PDC — proven by a deterministic in-repo CI gate (not external plugins):

**`YesDawHostIsolationCheck`** (built in item 1 of the close-out plan). It runs an in-repo synthetic test
plugin as a **real hosted `juce::AudioProcessor` in the worker child** (OS shared-memory RT lane), and
asserts (each clause negative-controlled):
- **(a)** tri-stream PDC **scheduling** — an audio impulse + an automation-ramp value + a synthetic event
  each land at the predicted compensated sample inside a `buildMixerGraphProjection` graph published through
  `Runtime`. The plugin DSP here runs in-process (the engine-side PDC walk is what's under test); the **real
  cross-process worker boundary** is proven by clauses (b) and (c) below, which both drive the live worker
  child. *(Driving the full automation/event tri-stream through the worker's RT-lane Event ring — parameter
  automation delivered to a hosted plugin — is sequenced to H4, where instrument/MIDI parameter delivery
  lands; the audio + state + crash/hang paths already cross the real boundary.)*
- **(b)** a **real child-side crash** (the worker terminates itself on cue) and a **hang** are each detected
  by the running watchdog, the child dies, recovery auto-recompiles swapping a `CompiledNodeKind::Placeholder`
  at the offender's slot, and the `{format,uid,version}` blacklist row survives restart; audio fails open
  (RTSan-clean, deadline-miss counter == 0); emit-NaN stays finite; impossible latency quarantined;
- **(c)** opaque plugin-state chunk round-trips across the real process boundary (crc32/len validated, a
  corrupted-CRC push rejected).

`pluginval L8–10` + `auval` run **non-blocking** where runners have real plugins; the in-repo gate is the
**always-on blocking** gate (ADR-0015).

## Green command

```
cmake --preset ci && cmake --build --preset ci && ctest --preset ci          # whole suite green, AND:
ctest --preset ci -R YesDawHostIsolationCheck                                 # the exit gate green
```

## Status: **GREEN (H3 closed)**

`YesDawHostIsolationCheck` is now a blocking Catch2 gate wired to `ctest -R YesDawHostIsolationCheck`.
The H3 exit criterion is satisfied when this gate is green together with the full `ci` preset, and the
close-out plan's Acceptance checklist is fully ticked.

## The plan

Full build order, every subsystem, every finding/deferral dispositioned:
[`docs/plans/2026-06-26-h3-close-out-plan.md`](../docs/plans/2026-06-26-h3-close-out-plan.md).

**H3 is closed. Do not start H4 until Dan completes the horizon-boundary review.**
