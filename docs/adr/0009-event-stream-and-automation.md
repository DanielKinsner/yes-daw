# 0009. Event stream — sample-accurate, block-sliced, type-generic; automation curves

- **Status:** Accepted
- **Date:** 2026-06-23
- **Deciders:** Dan (owner), build agent (H1)
- **Related:** ADR-0002 (#4 sample-accurate block-sliced events), ADR-0010 (events ride the tick/tempo
  time base), ADR-0008 (events flow through `process`),
  [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) decisions #4/#15,
  [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) → *Time & event model*,
  `CONTEXT.md` (Event — new, Automation).

## Context

Parameter changes, notes, and automation all need an exact position **inside** the processing Block so
timing is precise from the start, not "roughly right" (ADR-0002 #4). The struct that carries them and
the way automation is stored are irreversible: they are written into saved Projects and read by every
Node. MIDI is co-equal in the model from H1 (the pipe is MIDI-capable even while MIDI is dark), so the
event representation must be wide enough for MIDI 2.0 now — widening a `uint8` field later is a rewrite.
Automation curve representation rides this locked stream, so it is frozen here too (deepening-notes
risk #2).

## Options considered

1. **Model MIDI-1 now (`uint8` velocity/CC), widen later.**
   - Pros: smallest fields today.
   - Cons: widening to MIDI-2 resolution later is a format + code rewrite; the plan explicitly rejects
     this. Rejected.
2. **One generic, fixed-size event struct modeled on CLAP's family; MIDI is a variant; values
   normalized `double`.**
   - Pros: MIDI-1, MIDI-2/UMP, automation, and per-note expression all fit one trivially-copyable
     struct; 16-bit velocity / 32-bit CC represent losslessly; future event types are new variants, not
     a new container.
   - Cons: a `double` per value is wider than a `uint8`; SysEx can't live inline. Accepted (SysEx goes
     out-of-line).

## Decision

**One generic `Event` struct; MIDI is a variant** (modeled field-for-field on CLAP):

- `Event { uint32 timeInBlock; EventType type; uint16 flags; VoiceAddress voice; <payload>; }` —
  trivially-copyable, fixed-size, sorted ascending by `timeInBlock`.
- `VoiceAddress { int32 noteId; int16 portIndex, channel, key; }` (−1 = wildcard; a PCKN superset).
- **All values are normalized `double`** (the MIDI-2 widening): MIDI-1 maps `vel7/127.0`, UMP maps
  `vel16/65535.0`. `pitchNote` is continuous semitones, separate from `key` (decouples played key from
  sounding pitch for MPE/microtuning).
- **SysEx is never inline** — store `(offset, length)` into a per-Block side buffer to keep `Event`
  fixed-size.
- **Per-source monotonic read cursors,** not per-Block re-scan: each automation lane / sequence advances
  a cursor as the transport moves forward (per-Block cost O(events-in-block)); locate/seek/loop-wrap
  re-seeks by binary search.
- **Block-boundary edge rules are encoded now:** an event exactly on a boundary belongs to the *next*
  Block (half-open `[0, numFrames)`); convert each event through the full tempo map (don't assume
  constant samples/frame across a mid-Block tempo change); a note crossing a loop boundary emits a clean
  Off at the seam (never a hung voice); **PDC shifts the event stream by the same per-path latency as
  audio.**

**Automation curve representation** (frozen):

- Point storage `{ int64 tick; double value; CurveType curve_type; }`.
- **`CurveType` v1 ships all four: `Linear`, `Hold` (step), `Bezier`, `Log`/exp** (owner's call —
  full expressive set from day one; the enum is additive so more modes are a cheap later change).
- Evaluation is **sample-accurate or per-Block-offset**, riding the same event stream — not a separate
  timing path.

## Consequences

- **Positive:** one container for params, notes, automation, and expression; MIDI-2 fidelity from H1
  with no future widening; per-source cursors keep per-Block cost proportional to events *in* the Block;
  automation timing can never drift from event timing.
- **Negative / accepted costs:** `double` values and fixed-size events cost some bytes; SysEx needs the
  side-buffer indirection; four curve types to evaluate and test at H1.
- **Follow-ups:** CONTEXT.md gains **Event**. Tests: events land at exact in-Block offsets across Block
  boundaries and a tempo change (the H4 timing gate begins as a unit test now); automation evaluated
  sample-accurately matches an offline reference within golden tolerance.
