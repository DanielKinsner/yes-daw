# 0084. Musical audio channel equivalence and logical stereo

- **Status:** Accepted (in-scope implementation decision; fresh independent architecture critic accepted this concrete contract). Implementation and executable admission remain pending.
- **Date:** 2026-11-05
- **Deciders:** Build agent and independent architecture critic, within Dan's Option A mandate and ADR-0049/0078.
- **Related:** ADR-0030 (pinned stretcher), ADR-0042 (stereo width and balance), ADR-0055 (rate-matched views), ADR-0059 (immutable owners), ADR-0083 (Musical prepared lane).

## Problem and evidence

ADR-0083 requires identical stereo to remain numerically identical, while preserving one pinned persistent
Signalsmith instance. The unmodified C2 instance does not establish that property. The headless seven-fixture
context experiment completed with compile/candidate/verifier exits 0 and per-channel pristine/observer bit
identity. Its identical-input fixture nevertheless has 2,144 unequal raw channel pairs out of 2,304, including
1,971 of 2,048 retained pairs; maximum absolute difference is 0.000536702573299408.

The first recorded divergence is synthesis 1, band 0, final output: the winning channel takes the fallback,
while the other channel applies phase locking and another normalization. Recorded finite binary32 operands
reproduce a one-ULP difference there. This is a reached local explanation, not a general quality theorem.
The state/context result supplies no stereo waveform admission. Its separate, corrected row forecast does
not change these samples or this finding.

Retained evidence: ignored stereo-context-r1 run `20261105-114929-3203284`, execution receipt `D97C1F19`,
verifier `D3A6B7F6`; independent root audit `CD73FC22`. The source-only proposal is
`build-ci/20261105-musical-stereo-identity-prep/packet.md`, SHA256
`24CCE90E11D2DD42F829E5423CE09806EAE005746AD85B8CD0AB9AA98E47003F`.
These artifacts remain immutable. This decision does not turn their context result into waveform proof.

## Options

A. **Classify complete source-channel equivalence before DSP and preserve explicit logical stereo
(selected).** An equivalent stereo source uses one physical C1 instance and a predetermined two-channel
projection. Distinct stereo uses one physical C2 instance. Both publish logical C2 prepared audio.

B. Keep the C2 hypothesis and refuse unqualified stereo. This retains the known blocker and does not deliver
the required identical-stereo behavior.

C. Modify or replace the dependency, or relax exact equality. This would reopen owner-settled contracts;
it is outside this decision and current implementation authority.

## Decision: complete numeric equivalence before configuration

This is an additive architecture for the previously unadmitted Musical lane. Fixed/scalar preparation,
dependency version and bytes, stored Asset/Project data, goldens, thresholds and RT annotations remain
unchanged. Exactly one seeded pinned instance exists per preparation and persists for its complete lifecycle.

After resolving the immutable project-rate view and selected source window, inspect every selected sample
of every logical channel on the worker. Nonfinite samples refuse before any DSP construction. For stereo,
two finite samples are equivalent when their IEEE binary32 bits match, or both magnitude bits are zero.
The two complete planes are equivalent only when this holds at every selected frame. An early mismatch
may determine distinctness, but it must not skip finite validation of the remaining selected samples.
No hash-only, prefix-only, approximate, amplitude-scaled or averaged classification is permitted.

Signed zeros are explicitly numerically equivalent. Select original logical channel 0 as the fixed
representative and supply its samples unchanged, including its own zero signs. Neither original plane,
its content hash nor its immutable owner is rewritten. Signed-zero differences remain diagnostic data;
they are not a new source rejection merely for their representation. Class membership is a numeric
relationship, not a claim that both original planes have identical bytes.

Selection and reversal obey ADR-0083: only the selected mapped window is reversed before the varying map.
Classification covers that complete ordered window. Unselected Asset prefix/suffix samples cannot enter
the class or the supplied context. Declared outside-window context is separately owned numeric zero.

## Physical channels, logical channels and materialization

**Logical channels** are the source width retained by prepared audio and both engine builders.
**Physical channels** are the channels actually supplied to the single DSP instance.
**Channel projection** is the immutable logical-to-physical map chosen before DSP:

| Input | Physical channels | Logical channels | Projection |
|---|---:|---:|---|
| Mono | 1 | 1 | `[0]` |
| Numerically equivalent stereo | 1 | 2 | `[0,0]` |
| Distinct stereo | 2 | 2 | `[0,1]` |

Use a materialized interleaved logical owner. A stereo result contains exactly `2*D` finite float samples,
with both members of an equivalent pair taken from the one physical result at that frame. This output
layout is selected before DSP; it never repairs a failed C2 result, selects a better channel afterward,
normalizes output or changes amplitude. Distinct channels are neither swapped nor blended.

Apply the native fractional envelope once to the physical sample before materializing its equivalent
logical channels; gain remains live. Distinct channels receive the same envelope independently. Complete
source-to-output, reverse, fade and gain laws remain those of ADR-0083; materialization is not their proof.

Original and mapped source metadata retain their original channel count. A stereo prepared owner reports
channels 2, and both builders use `sourceChannels=2` at offset 0/count D/start A with the already-baked
reverse/fades cleared. It must not enter ADR-0042's mono-on-stereo widening path, which multiplies by
0.70710678. Track width and unity-centre stereo Balance continue to derive from the original stereo Asset.
No new projected Audio-thread reader or aliasing layout is introduced by this first representation.

## Request identity and ready-only adoption

Extend the complete Musical preparation request identity with explicit channel-policy and materialization
revisions. Retain all ADR-0083 dependencies, including exact original/mapped C2 owners, source window, tier,
map prefix, reverse and fades. No derived classification is saved in the Project or Asset.

The worker creates an immutable channel descriptor containing logical count, physical count, projection,
policy/representation revisions and its binding to that exact owned request. The prepared identity
includes this descriptor. Only legal table rows above are publishable, and the worker must certify the
complete finite/equivalence scan for the exact input before using a C1 stereo descriptor.

Ready lookup, accepting service ticks and adoption must validate request binding, descriptor legality,
revisions and complete output geometry without scanning N samples or preparing synchronously. A miss or
uncertified descriptor is a preparation/refusal result, not permission to process on the message or Audio
thread. Existing stale-source/history, latest-request, ownership and cache invalidation rules remain.
Unrelated live gain/name edits do not change this derived channel policy or invalidate otherwise eligible PCM.

## State, quiet input and resource obligations

C1 may take different energy/quiet branches from C2 because it sums one plane rather than two. It inherits
no C2 branch certificate. Independently certify actual physical C1 energy, source/history, current/previous
analysis, phase, extraction and retained contributions under the original 1e-15 quiet threshold. The
selected named nonzero-quiet refusal remains mandatory until its mapping is proved. Never boost the
representative, drop tiny samples, dither, normalize, reset at Tempo changes or fabricate C2 analyses from
one physical channel. Physical records describe actual C1; logical projection has its own complete ledger.

Charge the full selected-plane finite/equivalence scan, planar input, physical DSP/FFT/rings, raw/retained
buffers, logical stereo materialization, native fades, peaks, serializer capacity and metadata. Include
simultaneously live original/view/map/plan/cache/retiring/new owners and growth overlap. Logical stereo
still reserves its full `2*D` output even when physical DSP is C1. Shared storage is counted once, with
actual ownership proof. Caps remain 2^28 work/512 MiB per Clip and 1 GiB pending batch/reusable cache.
Neither the measuring instrument's budget nor a mono DSP allocation saving enlarges those limits.

## Mechanical finish line before activation

All checks below are unrun obligations. Architecture acceptance alone supplies no renderer admission.

- Freeze independent exact classifications for mono, identical/distinct stereo, both zero signs, mixed
  signed-zero-only differences, last-frame one-ULP mismatch, nonfinite late samples, nonzero offsets,
  selected reversal, differing unselected sentinels and immutable owner ABA. Validate all finite samples
  even after distinctness is known. Source bytes/metadata and failed results remain unchanged.
- Run actual C1/C2 preparation and logical materialization. Require exact D/channel, correct logical width,
  numeric equality for equivalent stereo, original-channel provenance for distinct stereo and the new
  physical state/context/quiet certificate. Query real physical geometry; do not synthesize C2 records.
- Execute physical own-gate controls: incorrectly merge a last-frame mismatch; skip late finite validation;
  omit logical stereo/use mono widening; swap distinct projection; corrupt one materialized sample; and
  run the old C2 path on equivalent input. Each must reach and fail its own unchanged gate.
- Close the independent whole-case checker budget and actual four-second pitch/RMS/peak/transient/fade
  matrix, including signed zeros and silence/resume. Exact stereo by construction supplies no other
  quality credit. Existing physical pitch/level/reverse/latency/center controls remain required.
- Prove both real builders' unity stereo reads with blocks 1/7/128/257, seek/loops and live gain; complete
  request/descriptor stale-result rejection, export provenance, cancellation/retirement and actual
  allocation/work/lifetime controls. Keep Fixed outputs and required RTSan/TSan/deadline CI green.

## Authority and consequences

Fresh architecture review must confirm this concrete extension preserves ADR-0030's pinned backend,
ADR-0042's stereo/no-lossy-downmix/unity law and ADR-0083's one persistent instance and numeric equality.
Within those contracts it is an in-scope implementation decision under ADR-0049/0078, not a new product,
dependency or file-format choice. No schema migration or owner approval gate is added. An actual conflict
with an owner-settled contract stops promotion and requires concrete alternatives.

The added costs are a complete worker scan, explicit descriptor identity and full stereo materialization.
Benefits remain conditional on mechanical proof: deterministic equivalent-channel identity without
altering the dependency or accepting a tolerance. Full Option A, including linked editing/folders and
piano reference views, remains unfinished.
