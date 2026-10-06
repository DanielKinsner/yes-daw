# 0053. Master strip: monitor Dim and Mute, a live loudness tap, master inserts on the master pane

- **Status:** Accepted (agent, 2026-10-06, under ADR-0049's implementation-ADR clause, after a separate agent
  critic pass whose six findings are folded in: the tap is taken from the mix inside the playback engine,
  before the metronome and input monitoring are summed in — not from the device buffer; the ring is one
  worst-case preallocation owned for the model's lifetime; memory ordering and the overflow rule are
  stated and the 0.1 LU gate holds only with no overflow; a locate while playing resets the readout and a
  loop wrap does not; the Dim/Mute ramp targets are pinned; true peak is the maximum over channels.
  Committed alone, before any G4.7 code.)
- **Date:** 2026-10-06
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G4.7 ("Master strip. Build: dim/mute, loudness readout in the header (exists as a
  meter), limiter editor. Gate: tokens.") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md);
  **ADR-0028** (loudness metering: libebur128 behind a control/offline wrapper; "a future live UI meter may
  feed it from non-audio-thread snapshots or a bounded analysis worker, but that worker boundary is outside
  this ADR" — this ADR defines that boundary); ADR-0002 (real-time foundations); ADR-0014 (the master is not
  a solo target); ADR-0034 (the master is implicit, not an editable Bus row); ADR-0038 (built-in FX — the
  Limiter); ADR-0046 (reference-DAW parity is the UI law).

## Context

The master today has a persisted gain (`masterLinearGain`) and a working insert chain
(`masterStrip.fxChain`, saved under the reserved master owner id): an insert added from the master's menu
already plays and its editor opens through the shared FX editor. But:

- There is **no Dim or Mute** anywhere — no way to drop the monitor level for a phone call or to silence the
  speakers without touching the mix.
- The header MASTER card and the mixer's master pane show **"-- LUFS" / "-- dBTP" always**: the readout is
  wired but nothing feeds it (`projectUiMixerSurface` is called without loudness). The ADR-0028 wrapper only
  analyses a whole buffer offline; a live meter needs a streaming form and a way to get the mix's samples
  off the audio thread.
- The master pane **paints no insert slots**, so the Limiter on the master is reachable only through a menu.
- What reaches the device is more than the mix: the playback engine sums the **metronome** into its output
  after the graph, and the model then sums **input monitoring** (direct or latency-compensated) into the
  device buffer.

What is hard to reverse: whether Dim/Mute are part of the *mix* (saved, rendered) or of the *monitor*
(transient, never rendered); where loudness is measured; and the real-time boundary for getting samples to
the meter.

## Options considered

**Dim and Mute**
1. **Monitor controls (chosen)** — Dim and Mute act on what you hear, on the device output only; they are
   not saved and never reach an offline render, an export or a recording. The control-room model (Logic's
   Dim, a monitor controller's Dim/Cut).
   - Pros: an export can never come out silent or 20 dB down because a monitor button was left on.
   - Cons: a muted master is not a way to "turn off" a mix in the saved project (the master fader is).
2. **Mix controls** — saved master mute/dim that the render honours.
   - Cons: the classic "why is my bounce silent" trap; dim has no meaning for a file.

**Where loudness is measured**
1. **The mix: the graph's master output, inside the playback engine, before the metronome and input
   monitoring are summed in (chosen).** The readout is the song's loudness, the number an export of the same
   passage would report.
2. **The device buffer** (everything you hear). Rejected: the click and a performer's direct monitor would
   inflate "the mix's" loudness.

**Getting samples to the meter**
1. **A bounded lock-free tap to the message thread (chosen)** — the engine copies the mix into a
   preallocated single-producer/single-consumer ring; the UI timer drains it into a streaming ADR-0028
   meter on the message thread.
2. **A meter node in the graph** — refused by ADR-0028 (libebur128 never on the audio thread).
3. **Re-render offline for the readout** — not live, and a full render per refresh.

**Master inserts**
1. **Paint the master's insert slots on the master pane with the strips' slot law (chosen).**
2. **A dedicated "Limiter" button** — a second law for one device; rejected.

## Decision

**Monitor Dim and Mute.** Two monitor controls on the header MASTER card (beside its meter) and two actions
(Master Dim, Master Mute) in the Transport menu and the master's menu. **Dim** lowers the monitor by a fixed
**20 dB** (a theme constant); **Mute** silences it. They are the LAST stage of the device callback — after
the engine, the metronome and input monitoring, and after the header peak scan (the peak meter keeps
showing the signal, not the speakers) — a single gain on every output channel, ramped linearly over
**5 ms** from its current value to its new target on every change:
- Mute on → target 0 (whatever Dim is); Mute off → target -20 dB if Dim is on, else unity;
- Dim toggled while Mute is on changes only the target Mute will release to.

They are session state: not saved in the project, not undoable edits, off on open. They never affect an
offline render, an export, a bounce or a recording (none passes through the device callback). Mute wins over
Dim; both light on the card (Mute in the danger colour, Dim in amber) and the status line names them while
on, so a silent monitor is never a mystery.

**Live loudness tap.**
- **Where:** inside the playback engine's block, right after the graph renders the master output and before
  the metronome overlay; the model's input monitoring is summed later and is never seen. Channels 1–2 of the
  master output (mono when the master is mono).
- **The ring:** one SPSC float ring, preallocated ONCE on the message thread when the model is created, sized
  for the worst case it accepts — **192 kHz × 2 channels × 2 seconds** (768 000 floats, about 3 MB). The
  model owns it for its whole lifetime; every playback engine it builds is handed the same ring (an engine
  swap after an edit keeps feeding it), so it is never reallocated or freed under a running callback.
- **Real time:** the engine's write is a copy plus index stores — no allocation, lock, log or I/O. The writer
  loads the read index with `acquire`, copies, then stores the write index with `release`; the reader loads
  the write index with `acquire`, reads, then stores the read index with `release`.
- **Overflow:** a block that does not fit (the message thread stalled about two seconds) is skipped whole and
  an overflow counter is incremented (relaxed). The readout then shows its value with a leading "~" until the
  next reset — a measurement with a hole is never presented as exact.
- **The meter:** the UI timer drains the ring into a **streaming ADR-0028 meter** (`LiveLoudnessMeter`: one
  libebur128 state at the engine's rate and channel count, fed in chunks, queried after each drain) on the
  message thread.
- **Reset** (a fresh meter state, the overflow mark cleared) when the transport starts playing, when the
  playhead is located while playing (Home, a marker, a ruler click — the readout measures from where you
  started listening), and when the engine's sample rate or master channel count changes. A **loop wrap does
  not reset** (one continuous listen). While stopped, the last values hold.
- **Readouts:** the header card's LUFS button and the mixer's INTEGRATED card show **integrated LUFS since
  the reset**; the mixer's TRUE PEAK card shows the **maximum over channels of the true peak since the
  reset** (red above 0 dBTP, as painted today). Both show "--" until the meter reports a value.

**Master inserts.** The master pane paints the master's insert slots above its meters with the strips' slot
tokens and gestures (a click on a filled slot opens the FX editor — the Limiter shows its gain-reduction
meter there; an empty slot's click lists the kinds, Limiter first; drag-reorder and bypass as on a strip).
The model is unchanged: the slots are `masterStrip.fxChain` under the reserved master owner id.

## Consequences

- **Positive:** the reference-DAW monitor controls exist without any risk to the saved mix or its exports;
  the loudness readout measures the mix by the standards implementation, inside the audio thread's rules;
  the master's Limiter is one click away.
- **Negative / accepted costs:** Dim's level is fixed at 20 dB for now (a preference can come later); the
  tap's 3 MB is held for the model's lifetime; the live readout needs a running engine (it says "--" with no
  device); a two-second UI stall marks the integrated value approximate rather than recovering the lost
  audio.
- **Gates (`[master]`, `[loudness-live]`, tokens):** Dim scales the device output by -20 dB and Mute to
  silence, each ramped over 5 ms, with the pinned targets for Mute released under Dim; an export of the same
  project is byte-identical with them on or off; Dim/Mute are not saved. The live meter fed through the
  engine's tap reports, **when the overflow counter is 0**, within 0.1 LU of a direct ADR-0028 analysis of
  the same samples; the metronome and input monitoring do not change the readout; it resets at play and at a
  locate while playing, not at a loop wrap; an overflow marks the readout "~"; the tap's write is
  allocation-free (RTSan). The master pane's slots open the editor and add a Limiter. New geometry and colours
  are theme tokens.
- **Follow-ups:** `CONTEXT.md` gains **Monitor Dim / Mute** (monitor controls, never rendered) and **Live
  loudness** (the mix tap and its reset law).
