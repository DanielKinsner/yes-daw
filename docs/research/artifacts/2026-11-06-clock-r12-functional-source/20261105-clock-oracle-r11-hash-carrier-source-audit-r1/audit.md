# R11 hash-carrier consumer and wire audit

**Recommend a bounded internal carrier successor after independent review. No
successor code was written or executed.** It preserves all actual file opens,
fresh length/reparse/EOF checks, SHA256 bytes and explicit wire projections. It
changes no cap, admitted input domain, SDK, external schema or native binding.
The complete conditional six-prefix packet remains separately sealed/HOLD.

Audit subject is R11 runner0E41A01150AE5A588D7D42176F6A6C7A6D922F8CE3AE69AD474FDA26E05C7DA9.
All16 physical Get-TaskHash call sites and all8 Assert-TaskIdentity call sites were
read in their complete source callers. There is no direct serialization of a
Get-TaskHash result. The file manifest's source-work/binding fields remain unbound.

## Recommended representation

Use a builtin non-enumerable two-reference Tuple internally: Item1 is the same
boxed actual byte count; Item2 is the fresh32-byte SHA256 digest array. The pinned
IncrementalHash.GetHashAndReset source explicitly allocates `new byte[HashLengthInBytes]`;
the returned digest is separate from the reused input buffer and is not mutated
by any audited caller. Borrow it only within the request. No global cache,
filesystem metadata reuse, Span binding or hidden native helper is needed.

Keep ordinary descriptor-map Assert-TaskIdentity for before/after reference and
build rows. Add a distinct closed helper for a fresh digest versus a declared
identity: path, expected byte count, expected hash, actual tuple. Pass expected
fields as scalar arguments captured in the same order as the old map construction,
instead of allocating a three-field map per pin. Keep expected byte count untyped
so the existing integer kind is borrowed rather than silently coerced. All new
argument/binder/field/loop/first-write/resource work needs the new source census.

For manifest/pin expected hex, compare exactly64 characters to the uppercase
alphabet's high/low nibble of each actual byte. Do not accept lowercase or use
PowerShell vectorized array equality. The old case-sensitive uppercase comparison
must remain identical. A binary baseline may be compared to its own immutable
32-byte digest with an explicit32-cell loop; any selected helper must have closed
source callers and distinguish strings from byte arrays without arbitrary
conversion or a caller-provided truth flag. Pure hex formatting may be reused in
a caller after it has been computed from that same immutable digest.

On mismatch, retain the exact gate and five witness fields:
`BEFORE_AFTER_COMPLETE_INPUT_IDENTITY`, path,expected_bytes,observed_bytes,
expected_hash,observed_hash. Format hashes to the same64-character uppercase
strings before creating that witness. No raw byte array or Tuple enters the
receipt, control witness, reference/build/artifact descriptor, manifest, bundle
or final stdout. Existing output preflight still rejects unsupported graph types.

## Closed consumers

| Source line | Role | Required successor behavior |
|---:|---|---|
|146-153|Hash producer|Same hash open/read/EOF/length/finally sequence; return actual count/digest tuple; no hex on ordinary internal comparison.|
|252|Post-child binary identity|Fresh hash remains in finally. Compare to immutable baseline; preserve primary/secondary classification and mismatch wire witness.|
|761|Current manifest fence|Compare digest directly to external uppercase expected manifest hash; preserve literal changed-manifest failure.|
|763-764|Own4/history pin fences|Same Join/Resolve, values and fresh hash; replace expected map with captured scalar arguments. These are9792 expected-map occasions at eight full fences.|
|778|Reference descriptors|Item1 provides bytes; explicitly format Item2 once into unchanged `sha256` map field. Keep every pool/unknown/file-set check.|
|781|Before/after reference identity|Keep ordinary five-field descriptor maps and existing string-map identity helper. No tuple reaches this map-only comparison.|
|839|Build descriptors|Same three exact files and pool/file-set checks; project formatted hash into unchanged four-field map.|
|918-923|Predecessor mutation control|Keep original source hash, copied file check, one-byte mutation and corrupt actual digest. Both expected control and actual rejection must contain the same uppercase observed string/content and original integer fields; avoid accidental repeated formatting in the estimate.|
|931-935|Binary/GUI-copy control|Keep all fresh baseline/copy/mutant hashes, length caps, actual PE inspection and mutation offsets. Store only original hex-string fields in retained control DTO. Baseline digest is immutable across producer/verifier calls.|
|936,944|Baseline passing|Pass the owned baseline tuple only to the fenced helper; this argument is never serialized. Both post-child hashes remain.|
|947|Before/after build identity|Keep ordinary four-field descriptor maps/string comparison unchanged.|
|948|Control-file artifact rows|Same exact-byte condition and cap; explicit count/hex projection into unchanged four-field map.|
|954|Four stage artifact rows|Same path and fresh file hash, cap and origin; explicit formatted string projection.|
|967|Final bundle hash stdout|Format actual digest to the identical uppercase hash after the same written-file hash; preserve the complete output prefix.|

Direct bootstrap-controller hashes and captured JSON HashData/ToHexString paths
at276-328 and666-670 do not consume Get-TaskHash results. They stay unchanged.
SameReferences' old dictionary and all receipt/build/control/file-set DTO maps
also stay ordinary maps; this option is not a global map rewrite.

## Source-request estimate and gate

At the current provisional whole prefix,9936 return Hashtables168bytes become
two-reference Tuple objects32bytes: reduction1,351,296. The9792 pin expected
Hashtables264bytes disappear with scalar arguments: reduction2,585,088. All9800
successful fence hashes can avoid the152-byte64-character hex string:
reduction1,489,600. Sum5,425,984bytes before new caller/binder/temporary-box/loop/
failure-format work. If expected tuples40bytes were preferred instead, their
separate carrier reduction is2,193,408 and gross total5,034,304. These are source
allocation-request equations using pinned layout/ABI facts, not measured managed
deltas, net savings, live/peak/reclaim credit or a32MiB fit certificate.

The raw digest arrays remain allocated and counted (56bytes each with header).
New helper source actions and every path/witness/binder/temporary object remain
funded. Failed prefixes retain earlier owners and must be recomposed; normal-path
savings cannot be blindly subtracted from every phase. The source-order and
wire-content audit above closes a concrete implementation route, but a new UNRUN
source packet, whole functional critic, complete census, final canonical input/root/
env/profile, numeric fit/binding and separate root execution GO are still required.
No new allocation probe is needed to choose this route; no runtime equivalence
has been claimed. A final two/three-reference Tuple field/layout source association
must accompany any such implementation; existing Tuple`5 file metadata alone is
not falsely labelled a measured Tuple`2/3 size.

Primary source: [exact IncrementalHash result ownership](https://github.com/dotnet/dotnet/blob/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Security.Cryptography/src/System/Security/Cryptography/IncrementalHash.cs#L121),
[Tuple source](https://github.com/dotnet/dotnet/blob/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Private.CoreLib/src/System/Tuple.cs).
