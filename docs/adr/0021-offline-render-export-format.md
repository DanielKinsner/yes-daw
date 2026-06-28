# 0021. Offline render export format

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent (H7)
- **Related:** ADR-0002 (real-time foundations), ADR-0010 (sample-rate policy), ADR-0011
  (Asset->Clip->Project indirection), ADR-0020 (H7-H11 roadmap), `CONTEXT.md` (Render, Export,
  Canonical export WAV), [H7 plan](../plans/2026-06-28-h7-offline-render-export-plan.md).

## Context

H7 needs the first real offline Render/Export surface: a Project can be rendered to samples, written to
a file, then decoded back mechanically. The file format is architectural because it becomes the bit-exact
gate format for future export work and the first decoder the bundle/import path can exercise without
JUCE. This decision must be written before the codec or export module lands.

The hard constraints are:

- Export must be checkable by CI with no listening.
- The bit-exact gate must not lose information to integer quantization or dithering policy.
- The codec must be pure/headless and usable by bundle tests without dragging the audio-device stack into
  the engine side.
- Project sample rate stays authoritative for H7; SRC and lossy export formats are later horizons.

## Options considered

1. **32-bit-float WAV at the Project sample rate.**
   - Pros: bit-exact float round-trip; simple RIFF/WAV container; DAW-standard enough for interchange;
     no dither/quantization decision; easy to decode in a pure test harness.
   - Cons: larger files than integer or compressed formats; not every consumer expects float WAV by
     default.
2. **24-bit or 16-bit PCM WAV.**
   - Pros: broadly compatible and smaller.
   - Cons: intentionally lossy for floating engine output; forces dither/noise-shaping decisions that are
     not needed for H7; bit-exact render round-trip would be impossible.
3. **FLAC / Ogg / MP3.**
   - Pros: smaller files.
   - Cons: adds third-party codec scope and, for lossy formats, makes exact verification the wrong gate.
     Rejected for this horizon.
4. **Raw float dump.**
   - Pros: easiest bit-exact test.
   - Cons: not a DAW export file users or other tools can open; dodges the actual export requirement.

## Decision

YES DAW's canonical H7 export format is **RIFF/WAVE, 32-bit IEEE float (`WAVE_FORMAT_IEEE_FLOAT`)**:

- Sample rate is the Project sample rate.
- Channel count is the Master bus channel count.
- Samples are written interleaved, little-endian float32.
- The H7 codec accepts only this canonical bit-exact format. PCM integer, BWF metadata, resampling,
  dither, FLAC/MP3/Ogg/AIFF, stem export, and region export are out of scope.

## Consequences

- **Positive:** H7 can prove `Render -> WAV -> decode` without quantization error, and future export UI can
  sit on a real headless API instead of a test helper.
- **Negative / accepted costs:** canonical exports are larger than integer/compressed files; integer and
  lossy export choices are deferred rather than silently decided.
- **Follow-ups:** add a pure float32 WAV reader/writer, add `YesDawOfflineRenderCheck`, and keep later
  integer/resampled/export-dialog work out of H7.
