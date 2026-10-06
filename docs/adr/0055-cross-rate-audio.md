# 0055. Cross-rate audio: a rate-matched view built at the read boundary, two resampler tiers, one rate ratio for every edit

- **Status:** Accepted (agent, 2026-10-06, under ADR-0049's implementation-ADR clause, after a separate agent
  critic pass whose findings are folded in: a clip's view window is mapped by its two ends, so a split's halves
  still abut exactly and nothing reads past the view; the resampler is zero-phase, its cutoff rule and kernel
  numbers are pinned, and JUCE's interpolator (which does not band-limit a downsample) is not used; the stored
  stretch factor is relative to the clip's natural length at the project rate; the views' ownership, the
  reversed and faded reads, Strip Silence and the self-check are stated; how this sits inside ADR-0010 is said
  plainly. Committed alone, before any G5.1 cp2 code.)
- **Date:** 2026-10-06
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** **ADR-0010** (one project rate; an Asset keeps its original, content-hashed bytes and is
  resampled at the read boundary — a fast tier for live playback, a high-quality windowed-sinc tier for offline
  render and export; "a 44.1k Asset in a 48k Project renders within golden tolerance of an offline
  reference"); **ADR-0011** (a clip's `src_offset` / `src_len` are measured in Asset frames); ADR-0002 (the audio
  thread's rules); ADR-0030 (time-stretch); ADR-0036 (recorded takes); ADR-0048 (the Sampler); ADR-0054 (import
  formats; cross-rate refused until this ADR); the plan's G5.1 ("…then cross-rate playback/export as separate
  checkpoints … cross-rate RT/offline results against their declared references").

## Context

Every audio path today assumes one Asset frame is one project frame, and import refuses a file at another
rate ("no resampling yet"). The engine reads a clip 1:1 from `srcOffset` (`ClipSchedule.h`), plays
`min(srcLen, timelineLength)` frames, mirrors a reversed clip over `srcLen`, and prepares a stretched clip at the
**project** rate. Most edit maths is already ratio-based — split and both trims go through
`sourceLengthForSplit` (`leftTimeline × srcLen / timelineLength`) and keep a split's halves exactly contiguous in
Asset frames — but a few sites read ticks as Asset frames: the import / drop placement
(`timelineLength = srcLen = frames`), the slip edit (a tick delta applied to `srcOffset`), the stretch gestures
(`factor = timelineLength / srcLen`, `newLength = srcLen × factor`), Stretch to Loop, and Strip Silence (run
lengths in Asset frames cast to ticks; a 50 ms minimum measured at the project rate). The Sampler's pads carry no
rate: a 44.1 kHz sample would play at the wrong pitch. Nothing in validation or persistence ties
`timelineLength` to `srcLen`, and the bundle already stores each Asset's own rate. JUCE's windowed-sinc
interpolator does not lower its cutoff when downsampling, so it aliases.

**How this sits inside ADR-0010.** ADR-0010 fixes *where* resampling happens (at the read boundary, never in the
stored bytes) and its two tiers; this ADR decides *how*: each tier's output is computed on the control thread
into a derived buffer when the Asset is read for an engine or a render, so the audio thread's read stays a plain
1:1 copy (ADR-0002). It does not change ADR-0010's decision.

## Options considered

1. **A rate-matched view at the read boundary (chosen).** When audio is read for an engine or a render, a
   cross-rate Asset's decoded samples are resampled — on the control thread, once per tier — to the project
   rate; the engine is handed that derived buffer and the clip windows mapped into its frames, so every engine
   read stays 1:1 and the audio thread does not change. Derived data only: never stored, never a replacement for
   the original bytes (ADR-0010, the plan's "decoded/resampled caches are derived data").
2. **A resampler inside the audio thread's clip read.** Per-clip interpolator state, fractional positions in the
   schedule, a fast tier only (a high-quality sinc per voice is too costly live), and a second read path for
   offline. More real-time surface, two code paths, no gain over (1) except memory. Rejected.
3. **Re-express clip windows in project frames.** Simplest edit maths, but it changes ADR-0011's unit for
   `src_offset` / `src_len` (an Accepted contract) and silently breaks every saved cross-rate window if the
   project rate ever changes. Rejected.

## Decision

**Units stay.** `srcOffset` and `srcLen` remain in **Asset frames** (ADR-0011); `timelineStart`,
`timelineLength` and fades remain in ticks (project frames for SampleLocked clips). A clip's **rate ratio** is
`r = projectRate / assetRate` (exactly 1 for a same-rate Asset), and its lengths are related by
**`timelineLength = round (srcLen × r × stretchFactor)`**. The stored `stretchFactor` is therefore relative to
the clip's **natural length at the project rate** (`srcLen × r`): its range stays [0.5, 2.0] and the percentage a
person sees is that factor, whatever the Asset's rate. One helper owns the ratio and both directions of the
conversion; no edit site computes it by hand. Rounding everywhere is half away from zero (`llround`).

**Rates.** An Asset at any rate from 8 kHz to 384 kHz is accepted; outside that range the import is refused,
"unsupported sample rate (N Hz)".

**The read boundary: a rate-matched view.** For each Asset whose rate differs from the project's, the
control thread builds a **rate-matched view** — its decoded samples resampled to the project rate, length
`round (frames × r)` — and the engine is handed that view instead of the original decode. Building it never runs
on the audio thread; the audio thread's code does not change.

**The resampler.** Our own Kaiser-windowed sinc, **zero-phase**: each output frame is evaluated at its exact
input position (`n / r`), so the view adds no delay — Asset frame N appears at view frame `round (N × r)`. Its
cutoff is `passband × min (1, r) ×` the Asset's Nyquist — that is, below the lower of the two Nyquist
frequencies — so a downsample does not alias. Input outside the Asset is silence (zero padding). The kernel is
tabulated at 1024 points per zero crossing and read with linear interpolation; arithmetic is in double.
- **Live tier** (every engine the app builds for playback, and the Sampler's pads): 16 zero crossings each side,
  Kaiser β = 7, passband 0.90.
- **Offline tier** (export, bounce, every offline render, the self-check): 64 zero crossings each side,
  Kaiser β = 10, passband 0.95.
The tiers' outputs differ slightly by design (ADR-0010). The same input gives the same output on a platform;
across compilers it is equal within float rounding. A same-rate Asset is never resampled.

**Ownership.** A view is derived audio kept beside the decode it came from, in the model's shared per-Asset
storage that engines reference today (G0.5): built when the Asset is imported or the project opened, reused by
every engine rebuild, freed with its Asset, and retired under the same law as decoded audio (an engine that
still holds it is reclaimed before the storage is). An offline render builds its tier's views for that render.
The future export worker (G5.3) snapshots the views with the project.

**The engine view.** The engine build receives a project in which every cross-rate clip's window is mapped
into the view's frames **by its two ends**: `start' = round (srcOffset × r)`, `end' = round ((srcOffset +
srcLen) × r)`, `srcOffset' = start'`, `srcLen' = end' − start'`, clamped to the view's length; the Asset's frames
and rate are the view's. Mapping ends, not lengths, keeps a split's halves exactly abutting in the view
(`right.start' == left.end'`), and nothing reads past the view's last frame. In that project the engine's laws
hold unchanged and in project frames: it plays `min (srcLen', timelineLength)` frames, a reversed clip mirrors
over `srcLen'`, fades are in ticks = project frames, and a stretched cross-rate clip is stretched from its view
at the project rate. The mapping is a pure function of the saved project and the views; nothing mapped is saved.

**Edits use the ratio.** Import and drop place a cross-rate file with `srcLen = frames` (Asset frames) and
`timelineLength = round (frames × r)`; the slip edit moves `srcOffset` by `round (Δticks / r)` Asset frames; the
stretch gestures and Stretch to Loop compute the factor as `timelineLength / (srcLen × r)` and the new length as
`round (srcLen × r × factor)`; Strip Silence (which refuses stretched clips) measures its 50 ms minimum run as
`round (0.05 × assetRate)` Asset frames and converts each run to ticks as `round (run × r)`. Split and trim keep
their ratio law (already rate-agnostic). The waveform painter and the inspector read the clip's own window over
its timeline span, as now.

**The Sampler.** A pad whose sample is at another rate plays its live-tier view, so its pitch is right; the
pad's root key and pitch law are unchanged.

**Import.** Cross-rate files are accepted (ADR-0054's refusal is lifted); the original bytes and the Asset's own
rate are stored as before.

**Out of scope.** Recording at a device rate other than the project's stays a warning (G7) — a take recorded
that way is not made right by this ADR; changing a project's rate stays unsupported (no UI exists); gapless
MP3/Ogg trimming stays out (ADR-0054); building views in the background (an open of many long cross-rate files
waits for them) is a later improvement.

## Consequences

- **Positive:** files at any common rate import, play and export at the project rate with no audio-thread change;
  live playback and export each use their declared tier; the clip window contract is unchanged, so a saved
  project never re-quantises.
- **Negative / accepted costs:** a view costs as much memory as the decode it came from; building it takes time at
  import and open (gated below for a three-minute stereo file) and the open waits for it; export pays the long
  kernel per cross-rate Asset; live playback and export of a cross-rate clip differ in the last bits; a clip end
  maps to the view with at most half a frame of rounding.
- **Gates (`[cross-rate]`):** a 1 kHz sine at 44.1 kHz resampled to 48 kHz is within -80 dB (offline) and -60 dB
  (live) RMS error of the analytic 48 kHz sine in the steady state, and back down 48 to 44.1 kHz likewise; a 96 kHz
  Asset with a 1 kHz and a 30 kHz tone renders at 48 kHz with the 1 kHz tone within 0.1 dB and the 30 kHz tone
  (aliasing to 18 kHz) at least 70 dB down on both tiers; an impulse at Asset frame N lands at view frame
  `round (N × r)` ± 1 (zero phase); a click in a cross-rate clip lands at timeline frame
  `start + round ((N − srcOffset) × r)` ± 1; a split cross-rate clip's halves abut exactly in the view and render
  the unsplit clip's audio; trim, slip and stretch on a cross-rate clip render the same audio at the same timeline
  places as their same-rate law predicts; a 44.1 kHz MP3 imports with `timelineLength = round (frames × 48000 /
  44100)`; a 44.1 kHz Sampler pad plays its tone at the right pitch; same-rate projects render bit-identically to
  before; reopening a project with cross-rate Assets rebuilds the views and plays the same; the self-check renders
  a cross-rate bundle; a three-minute stereo 44.1 kHz file's live view builds in under two seconds on the dev
  machine (measured, reported in the evidence).
- **Follow-ups:** `CONTEXT.md` gains **Rate-matched view** (the derived, never-stored resampled decode) and
  **Rate ratio**.
