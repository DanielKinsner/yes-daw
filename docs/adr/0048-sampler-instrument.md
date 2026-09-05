# 0048. The Sampler instrument: pads on the Track, samples as Project Assets

- **Status:** Accepted (Dan, 2026-09-05: "accept the adr") <!-- Proposed | Accepted | Superseded by NNNN | Deprecated -->
- **Date:** 2026-09-05
- **Deciders:** Dan (the Real-DAW arc's G3.9 item), build agent (proposer)
- **Related:** ADR-0047 (one persisted instrument per Track — the slot this fills), ADR-0043
  (SimpleSynth, the first instrument), ADR-0002 (the audio thread never allocates), ADR-0009 (the
  sorted half-open event block), the G0.5 asset-ownership law (`AssetOwnership` / `AssetSamples`
  in `OfflineRenderer.h`), the plan's G3.9 and SS-4 in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md)

## Context

The Track instrument slot (ADR-0047) holds one kind today: SimpleSynth. SS-4 ("write a beat and a
chord progression") needs a drum kit — one-shots on pads, a pattern pencilled in a drum-mode piano
roll — and a pitched sample instrument is the same machine with note-off honoured and the pitch
following the key. Every reference DAW ships one (Logic's Quick Sampler / Drum Machine Designer,
Live's Simpler / Drum Rack, Cubase's Sampler Track); YES DAW has none.

Two things are hard to reverse:

1. **Where the samples live.** A sampler's pads reference audio files. If those bytes ride inside
   the instrument's opaque state blob (ADR-0047's `instrument_state`), the bundle's asset law —
   content-hashed files under `assets/`, foreign keys from every referencing row, orphan sweeps,
   the Clip's non-destructive reference (CLAUDE.md's hard rule) — does not see them: a pad's
   sample could be deleted from under it, duplicated per pad, or lost on a bundle copy.
2. **How the engine gets the bytes.** Audio Clips reach the graph through the G0.5 ownership law
   (`DecodedAssetAudio` views + shared `AssetSamples` owners, never a copy on the audio thread,
   never an allocation in `process()`). A second, private path for sampler pads would be a
   second thing to keep RT-safe.

## Options considered

1. **Option A — pads and their audio inside the instrument-state blob.**
   - Pros: no schema change; the slot stays opaque; nothing in persistence learns about pads.
   - Cons: breaks the asset law (no foreign keys, no orphan sweep, no dedup, no content hash);
     the tracks row grows by megabytes; a bundle copy or a hash rename silently loses pads; the
     audio must still be decoded into an owner for the engine, so a second decode path anyway.
2. **Option B — pads as Track rows referencing Project Assets (chosen).** A `SamplerPad`
   {trigger key, asset id, root key, one-shot, gain, name} on the Track; persisted in
   `sampler_pads` (v31) with foreign keys to `tracks` and `assets`; the pad's audio is a Project
   Asset imported like any WAV (the same content hash, the same `assets/` file, the same decode
   into the model's decoded-asset table); the engine gets the pad's samples through the SAME
   ownership law as Clips — the projection asks an `assetSamplesProvider` seam on its config
   and the build serves the shared `AssetSamples` owner (or the build's one copy).
   - Pros: one asset law; a pad's sample is shareable with a Clip of the same file; undo /
     redo, save, and the bundle sweep see pads as rows; a missing asset is a named refusal.
   - Cons: a schema version; the Track struct grows a vector; a pad row must be validated
     against the asset table on every project validity check.
3. **Option C — pads as their own entity kind with ids (like Clips).**
   - Pros: pads addressable by automation and by other tracks.
   - Cons: an entity id per pad, a new id-uniqueness surface, more verbs, for no need the plan
     names (pads are addressed by key, and the instrument's parameters are per Track).

## Decision

**Option B.** The Sampler is `TrackInstrumentKind::Sampler`, a third kind in ADR-0047's slot.
Its pads are `Track::samplerPads` — rows, persisted in `sampler_pads` (schema v31), each
referencing a Project Asset. Its per-Track parameters (attack, decay, sustain, release, gain)
ride ADR-0047's `instrument_state` blob and ParamSpec law exactly like the synth's. The engine's
`SamplerNode` receives one shared `AssetSamples` owner per pad from the projection, which obtains
it through the config's `assetSamplesProvider` — the G0.5 owner when the build has one, else the
build's single copy — so the audio thread reads samples it never allocated.

Playback law: a NoteOn on a pad's own key plays that pad at its root pitch; a NoteOn on a key no
pad claims plays the nearest lower **pitched** pad transposed by the interval (a one-shot pad never
answers to other keys); a one-shot pad ignores NoteOff and plays to the sample's end (or until
stolen); a pitched pad releases on NoteOff through the ADSR. Sixteen voices, the oldest stolen.
Pitch is a linear-interpolated read at 2^((key − root)/12); the pad's asset shares the project
sample rate (the import law already refuses others).

## Consequences

- **Positive:** a drum kit and a pitched sample instrument from one node; samples are ordinary
  Assets (deduped, hashed, swept, shareable with Clips); undo covers a pad edit as a Track row edit;
  the drum-mode roll can name keys from pad names without a second data source.
- **Negative / accepted costs:** schema v31; the Track struct carries pads every Track does not
  use; no per-pad envelope or filter (the plan's list is one-shot + pitched + ADSR); no zones or
  velocity layers (a later ADR if wanted); a pad's asset cannot be swept while the pad references
  it (that is the point).
- **Follow-ups:** `CONTEXT.md` gains **Sampler** and **Sampler pad**; the instrument panel shows a
  pad grid (load by chooser, drop a WAV on a pad); the piano roll's drum mode names keys from pads;
  `sampler_tests.cpp` renders goldens by equivalence (a one-shot pad renders the sample verbatim
  from the note's frame; a pitched pad an octave up renders the same sample read at twice the
  rate); parking lot: velocity layers, per-pad envelopes, round-robin, a pad's choke group.
