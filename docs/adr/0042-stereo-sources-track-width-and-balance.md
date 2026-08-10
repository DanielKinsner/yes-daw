# 0042. Stereo sources, Track width, and the balance law

- **Status:** Accepted
- **Date:** 2026-08-09
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0008 (Node contract), ADR-0014 (mixer projection), ADR-0022 (transport), ADR-0034
  (mixer schema), the 2026-08-08 honest-DAW recovery entries in `STATUS.md`.

## Context

The entire source path is mono-only: WAV import rejects any file with more than one channel, the
offline/live source factory refuses non-mono Assets, and the mixer projection hard-requires a
one-channel Track source that a `PanNode` widens to stereo. Normal music production audio is stereo.
Dan directed on 2026-08-09 that stereo is mandatory ("it HAS to have stereo") and that the fix must
not cut corners. The standing constraint from the 2026-08-08 recovery holds: never hide the stereo
decision behind a lossy downmix.

A second constraint: the render goldens and every existing mono Project must keep producing
bit-identical output. Stereo support must therefore be additive — a new width-aware path — not a
rewrite of the mono path.

## Options considered

1. **Width-aware strips: mono keeps Pan, stereo gets Balance.** A Track's width is derived from its
   Clips' Assets (any stereo Clip → stereo strip). Mono strips keep the existing equal-power
   `PanNode` bit-identically. Stereo strips run stereo end-to-end and place a new
   `StereoBalanceNode` in the pan slot. Accepted.
2. Downmix stereo files to mono at import. Rejected: lossy, explicitly forbidden.
3. Upmix everything to stereo at the source and make every strip stereo. Rejected for now: changes
   the rendered output of existing mono Projects (goldens break) and forces a simultaneous rewrite
   of sends/buses; the width-aware path reaches the same end state incrementally.
4. Explicit user-chosen Track width at creation (the Pro Tools model). Deferred, not rejected: the
   alpha's import-driven flow makes derived width invisible-correct. Explicit width arrives with
   recording-width selection UX, which needs it.

## Decision

**Asset width.** An Asset may carry 1 or 2 channels, stored interleaved. Import accepts mono and
stereo WAV; files with more than 2 channels are rejected with a clear error (no silent downmix).
The bundle schema already stores `channels` and needs no migration.

**Track width.** A Track's strip width is derived: 2 if any of its Clips references a stereo Asset,
else 1. Derivation is a projection-time fact, not stored state.

**The pan slot.**
- Mono strip: the existing `PanNode` (equal-power, gL=cos t, gR=sin t, t=(p+1)·π/4), unchanged and
  bit-identical.
- Stereo strip: a new `StereoBalanceNode`. At centre both channels pass at unity. Off-centre, the
  far channel is attenuated along the same quarter-cosine taper (gFar = cos(|p|·π/2)); the near
  channel stays at unity. Channels are never blended into each other. This is the Logic-style
  balance control.
- `StereoBalanceNode` reuses `PanNode`'s parameter id and normalized mapping, so pan automation
  lanes and the mixer pan control target the same parameter regardless of width.

**Mono Clips on a stereo strip.** `DecodedClipNode` becomes width-aware: it stores interleaved
samples and emits its Track's width. A mono Asset on a stereo strip is widened with equal-power
centre compensation (both channels ×cos(π/4) ≈ 0.7071), which makes its centred loudness identical
to the same Clip on a mono strip. A stereo Asset on a stereo strip plays its channels through
unchanged.

**Strip chain.** A stereo strip runs stereo from the source: source(2) → optional sidechain VCA(2)
→ `StereoBalanceNode` → inserts (already stereo) → Fader(2) → Meter(2). The built-in FX inserts,
Fader, Meter, and Sum nodes are already channel-generic.

**Sends and buses.** A Bus's width is derived the same way: 2 if any tap feeding it is stereo, else
1 (the existing mono bus path stays bit-identical). `SumNode` inputs gain a per-input scalar gain
(default 1.0 — bit-identical for existing graphs). A mono tap into a stereo Bus is bound to both
accumulator channels at ×cos(π/4), matching what the mono bus + centre `PanNode` return path
produces today. A stereo Bus's return chain uses `StereoBalanceNode` in its pan slot.

**Export.** Unchanged: the master bus is already stereo and exports stereo float32 WAV.

**Recording width** (mono capture today) is out of this decision's scope; it lands with the
explicit-track-width UX (option 4).

## Verification

- Mono bit-identity: an existing mono Project renders bit-identically to its pre-change output
  (golden compare untouched and green).
- Balance law: `StereoBalanceNode` at centre is unity on both channels (null test); at full right
  the left channel is exact zero and the right unity; the taper matches cos(|p|·π/2) within one
  LUT step; ramping is block-size independent.
- Loudness consistency: a mono Clip rendered on a stereo strip at centre equals the same Clip on a
  mono strip at centre pan, sample-exact.
- Stereo end-to-end: a stereo WAV with distinct L/R content imports byte-for-byte, reopens, plays
  with the two channels preserved (never swapped, never blended), and offline Render == RT playback
  within the established tolerance; export/reimport round-trips bit-exact.
- Rejection: a 3+-channel WAV import fails with a clear error and no Project mutation.
- RTSan/TSan stay green on the audio path; all mixer/FX/automation gates stay green.
