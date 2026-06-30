# 0036. Recorded audio assets and Take metadata

- **Status:** Accepted
- **Date:** 2026-06-30
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0011 (asset/clip/project identity), ADR-0012 (SQLite bundle schema and atomicity),
  ADR-0018 (recording latency and take writer), ADR-0021 (offline render export format), ADR-0034
  (Track/Bus mixer state), ADR-0035 (H13 recording and device UX), H13 plan, `CONTEXT.md`.

## Context

ADR-0035 says recorded audio becomes bundle-owned immutable Assets plus Take metadata, and it explicitly
requires a focused ADR if H13 needs a new irreversible bundle schema or recorded-audio asset format. The
first H13 recording checkpoint proved the shipped shell can copy deterministic test-device bytes into the
existing opaque Asset store and place a non-destructive Clip, but the next H13 gate requires save/reopen
and decode of the recorded Asset. Opaque bytes are not enough for that gate.

H5 also deliberately kept `.ysdtake` as an internal deterministic test format, not a user-facing Project
bundle format. Promoting `.ysdtake` now would reverse that deferral and make the recording path depend on
a test artifact rather than a normal audio asset. The irreversible part is twofold: once recorded Projects
exist, the asset byte format must remain decodable, and once Take rows exist, later Projects must migrate
from that schema.

## Options considered

1. **Canonical float WAV Assets plus normalized Take/Comp metadata (recommended).**
   Recorded audio Assets are RIFF/WAVE, 32-bit IEEE float, little-endian, interleaved, at the Project/device
   sample rate and recorded channel count. The SQLite bundle adds normalized Take metadata linked to Asset,
   Track, and Clip identity; later Comp rows reference Take IDs and source/timeline windows.
   - Pros: uses a standard, tool-decodable format; reuses ADR-0021's float-WAV precedent; avoids promoting
     `.ysdtake`; keeps Take identity separate from Clip placement; migration and recovery can validate real
     rows rather than infer recording history from filenames.
   - Cons: writes a larger uncompressed asset than compressed formats; requires a schema migration before
     Take persistence code lands; loop comping needs explicit rows rather than piggybacking on Clip fields.
2. **Promote `.ysdtake` into the bundle as the recorded-audio format.**
   Store H5 take files directly as Assets and decode them with the H5 reader after reopen.
   - Pros: already exists; preserves take ordinal and timeline stamps in one file; fastest implementation.
   - Cons: contradicts ADR-0018 and ADR-0035's deferral; the format was built for deterministic tests, not
     durable user Projects; no external tool can inspect it; mixing file-level Take state with Project rows
     complicates migration and recovery.
3. **Keep raw float blobs as opaque Assets and store format hints in Take metadata.**
   The recording path writes interleaved float samples without a container and relies on bundle metadata for
   sample rate, channels, and frame count.
   - Pros: very small code path; no container overhead; close to the current first checkpoint.
   - Cons: creates a YES DAW-specific recorded-audio format anyway, but with fewer self-describing bytes;
     recovery and external inspection are weaker; byte-order and channel-layout rules become hidden schema.
4. **Infer Takes from Clips and skip Take metadata for H13.**
   A recorded Clip would point directly to an Asset; loop passes and comping would be represented as
   multiple Clips only.
   - Pros: no schema migration for Take rows.
   - Cons: loses Take identity; cannot distinguish an imported Clip from a recorded Take; makes loop take
     lanes and comp selections fragile; fails ADR-0035's "Assets plus Take metadata" requirement.

## Decision

**Option 1 is accepted.** Recorded audio Assets are canonical RIFF/WAVE files using 32-bit IEEE float PCM,
little-endian, interleaved samples. The Asset row remains the content-addressed immutable byte owner from
ADR-0011/0012; the WAV container is the recorded-audio Asset byte format, not a replacement for the Project
schema. H13 must not promote `.ysdtake` beyond internal tests.

Take metadata becomes normalized Project bundle truth in the next migration:

- `recording_takes` rows have stable Entity IDs and reference exactly one Asset and one Track.
- A Take row records at least `timeline_start`, `frame_count`, `take_ordinal`, `input_channel`,
  `device_stable_id`, and the chosen `monitoring_policy`.
- A recorded audio Clip references the recorded Asset as usual and is linked to the Take metadata by
  stable ID. The Clip remains the audible non-destructive placement; the Take remains the recorded pass.
- Basic Comp state is represented by ordered non-destructive Comp segments that reference Take IDs and
  source/timeline windows. A single-take recording may be represented as one full-length segment when the
  comp surface needs uniform handling.
- Autosave and recovery validate Take rows with the same standard as other bundle truth: referenced Asset,
  Track, and Clip IDs must exist; source windows must fit the Asset frame count; corrupt or missing WAV bytes
  fail the bundle validator rather than silently producing silence.

The H13 implementation can land this incrementally: first add the migration and single-Take round trip,
then add loop/comp segments and recovery gates. Each implementation checkpoint must be self-asserting.

## Consequences

- **Positive:** recorded audio can be decoded after save/reopen using a standard format; Projects can
  preserve Take identity without overloading Clip fields; recovery can validate Asset bytes and Take rows
  mechanically; H13 can build take lanes and basic comps on stable IDs.
- **Negative / accepted costs:** H13 now needs a schema migration and a recorded-WAV writer/reader gate;
  recorded Assets are uncompressed and larger than compressed alternatives; channel-layout depth beyond
  mono/stereo remains future work.
- **Follow-ups:** add `recording_takes` and Comp segment persistence with migration tests; change the
  shipped-shell record path from opaque test bytes to canonical float WAV Assets; add a save/reopen/decode
  H13 gate; keep **Recorded audio asset** and **Take metadata** aligned in `CONTEXT.md`.
