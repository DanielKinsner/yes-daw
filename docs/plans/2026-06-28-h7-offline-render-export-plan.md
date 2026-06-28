# H7 plan — Offline render / export to file

**Why this exists.** H7 is complete when a Project can be **bounced to an audio file** and we can prove,
mechanically, that the file holds exactly what the engine renders. This discharges the H2-deferred
"offline Render/Export" clause and finally makes the build plan's highest-value DAW test real: an offline
render compared against an *independent* reference (not the engine compared to itself — the trap the
H1/H2 review caught). It also gives the project its **first real audio decoder** (WAV), which H8's runtime
and the eventual import pipeline both need.

## Exit gate

`YesDawOfflineRenderCheck` is the H7 blocking gate. It proves three things, each with a negative control:

1. **Offline render == independent reference.** Rendering a known Project offline to a float buffer equals
   a reference computed a *different* way (sum the Clips at their timeline positions with their gain/fades,
   as the H1 render reference does) within tolerance. A mutation of the renderer (wrong length, wrong clip
   position, dropped tail) must fail this compare.
2. **WAV round-trips bit-exactly.** A canonical 32-bit-float WAV written then read back is sample-identical.
   A mutation of the writer (wrong scale, byte order, a dropped/extra sample, wrong header size) must fail.
3. **Export → re-import round-trips.** The rendered buffer exported to a `.wav` file, then imported as an
   Asset through the normal bundle import path and decoded, yields samples that match the render.

## Build order

Each checkpoint is one small, independently-green commit; CI is the gate.

1. **H7 kickoff docs.** Accept ADR-0020, switch `loop/horizon.md` to H7, land this plan, and add the
   render/export terms to `CONTEXT.md`. Code untouched. *(Mostly done in the ADR-0020 commit; Codex
   confirms `loop/horizon.md` points at H7 and `CONTEXT.md` is updated.)*
2. **ADR-0021 — offline render + export format.** Short ADR locking the canonical format: **32-bit-float
   WAV (PCM, `WAVE_FORMAT_IEEE_FLOAT`)** at the Project sample rate, channels from the Master bus. This is
   the bit-exact gate format; integer formats (16/24-bit) are optional lossy exports, out of scope for the
   bit-exact round-trip. (Per the "no code before the decision is an ADR" rule.)
3. **WAV codec (writer + reader), pure + headless.** A `src/io/WavFile.h` (or `src/persistence/`) with a
   float32 WAV encoder and decoder, no JUCE, RTSan-irrelevant (control-side I/O). Gate: random-buffer
   write→read bit-exact round-trip; malformed-header / truncated-file rejection; negative control proving a
   mutated writer fails the round-trip.
4. **OfflineRenderer module.** Promote a real `OfflineRenderer` into `src/engine` that renders a Project to
   a float buffer over the **full timeline** (length derived from Clips/markers; flush the PDC/effect tail
   so nothing is truncated), built on `ProjectMixerProjection` + `CompiledGraph::process`. This replaces
   the test-only `renderByBlocks`/`renderOfflinePath` helpers in `bundle_render_tests.cpp`. Gate: offline
   render of a known Project == the independent reference within tolerance, with a negative control
   (wrong-length or wrong-position mutation fails).
5. **Export to file + re-import round-trip.** Wire `OfflineRenderer` → WAV writer → a `.wav` on disk; then
   import that file as an Asset (existing `importAssetBytes` path) and decode it with the new WAV reader;
   assert decoded == rendered within tolerance. Negative control: a corrupted exported file fails import or
   the compare.
6. **Close.** Run the focused `YesDawOfflineRenderCheck` and the full `ci` preset. H7 closes only if both
   are green; update `loop/horizon.md` to CLOSED and the roadmap H7 status note honestly.

## Non-goals (H7)

- No UI, no export dialog, no progress bar (a one-call API + the gate; UI is H11).
- No MP3 / FLAC / Ogg / AIFF — WAV only this horizon.
- No sample-rate conversion: render at the Project sample rate (SRC is a later horizon).
- No integer-format bit-exactness gate (float32 is canonical; int16/24 export is optional and lossy).
- No stem / multitrack / region export — a single master-bus bounce of the whole Project.
- No real-time/streaming export, no freeze/bounce-in-place track editing.
- No device I/O or transport — that is H8.

## Open decisions (stop for an ADR)

- The canonical format (checkpoint 2) is an ADR (0021). Anything beyond float32-WAV-at-project-SR
  (resampling on export, integer dithering policy, BWF/metadata chunks) is out of scope — defer, do not
  decide inline.
- Tail length / silence-trim policy for the render end: render through the full PDC + longest effect tail,
  do **not** auto-trim trailing silence in H7 (trimming is a UI/export-option concern). If a real
  ambiguity surfaces (e.g. an infinite-tail feedback Node), stop and raise it rather than guessing.

## Status

Implemented locally on 2026-06-28 and ready for Claude adversarial review. ADR-0021 is Accepted; the pure
float32-WAV codec, real `OfflineRenderer`, and export/import round-trip gate are in place. Local focused
gate: `ctest --test-dir build-ci -R "YesDawOfflineRenderCheck" --output-on-failure` passed 1/1. Full
local gate: `ctest --preset ci --output-on-failure` passed 238/238. Do not start H8 until Claude's H7
review is adjudicated.
