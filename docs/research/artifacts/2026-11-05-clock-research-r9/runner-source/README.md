# ClockOracle R9 reader-capacity correction — source only / UNRUN

2026-11-05. Own exactly clock_stage.cpp,source-costs.h,run-clock.ps1,README.md,
source-manifest.json in this new directory. Root authorized a metadata-only
successor to sealed R8. Whole R8 review accepted its raw-admission mechanism
SOURCE ONLY and found one unsupported reader-capacity claim in README/manifest.
R9 withdraws that claim; no lexer/domain/control/matrix/cap/guard behavior changes.
All R8 and older sealed files remain byte-exact. This source has not been parsed
by PowerShell, imported, compiled or executed. No helper,
program, test, stage, GUI, device, audio, Git or CI operation was run by this author.
All source/work/execution/compiler/produce/verify guards remain false; no Catalogue
data definition, family price, new research allowance or execution GO is supplied.

## Why this successor exists

R7's complete functional source review was ACCEPT, not a numeric/payload/native
certificate. Its subsequent runner census found a genuine controlled-payload
counterexample: 4,194,304 ASCII digit1 bytes pass the old dependency shape check.
Pinned JsonTextReader then grows old char[4,194,304] to new char[8,388,608] while
StringReader owns the decoded 4,194,304-unit text and the runner owns byte[65536].
Those four simultaneously live payloads total 33,619,968 bytes, exceeding the
unchanged 33,554,432-byte cap by 65,536, before the reader's later numeric refusal.
This is a source-level live lower, not a measured run or a claim MSVC emits it.
It needs no GC assumption and excludes other retained arrays, strings and graphs.

The unchanged R8 mechanism supplies strict raw JSON admission before ALL four SDK parser
call sites: 19 normal invocations (bootstrap1, predecessor1, dependency1,
C++control records14, summaries2). It does not parse text with another SDK or
regular expression, construct an AST, copy rejected tokens or stringify inputs.
The source-built input schemas and exact captured identities select distinct
finite profiles. Quoted metadata, paths and control/status strings remain strings.

## Supported scalar grammar and exact profiles

The existing closed JSON output domain is null, Boolean, String and the eight
builtin integral kinds. C++ control operands are six uint64 values; summaries
are nonnegative signed-representable integers. Full UInt64/FNV values survive:
18446744073709551615 has20 decimal units. Int64.MinValue is
-9223372036854775808, also20 units including its sign. No float field is required
by these parsed schemas. The exact predecessor and source manifest contain no
floating JSON values. All literal structural/lexical measurements are in the
new manifest; ordinary Python JSON artifact inspection is not target PS execution.

The helper admits strict decimal integers from Int64.MinValue through
UInt64.MaxValue. It checks digits, optional minus, no leading zero, at most20
units, and lexicographic sign-specific magnitude BEFORE the SDK. It never
constructs a numeric token string, BigInteger or floating intermediate. true,
false and null are checked against fixed4/5/4-unit literals. Other bare syntax
rejects before the SDK, including fraction/exponent forms, leading plus,
NaN/Infinity/undefined/constructors, identifiers, comments and single quotes.
This is the supported closed schema, not a general JSON compatibility claim.
Fractional values in arbitrary ignored dependency extras are explicitly unsupported;
no formerly meaningful supported field needs them.

The official [MSVC sourceDependencies example](https://learn.microsoft.com/en-us/cpp/build/reference/sourcedependencies?view=msvc-170)
was inspected during this source task. Its Version is a quoted1.2 string and its
dependency values are strings/maps/arrays. Existing checks still refuse nonempty
ImportedModules/ImportedHeaderUnits and require Source/Includes to match the
fenced conservative include inventory. The opaque compiler's actual report is
not executed or certified here; an unsupported future report fails closed.

| Parser input | Node+key cap | Decoded string units | Container depth |
| --- | ---: | ---: | ---: |
| Current externally hash-authenticated manifest | Exact source-derived count in PS/manifest |512|4|
| Same captured hash-authenticated predecessor bytes |2420|270|3|
| Dependency report |2*current include_input_count+256|512|24|
| One exact three-field C++control/six operands |13|128|2|
| One twelve-field C++summary |25|128|1|

Bootstrap profile is derived from this exact final five-file source package,
not copied from R7. Its raw byte[] is authenticated before decode as before.
Predecessor now ALSO hashes the SAME returned captured byte[] against the exact
pinned R4 bundle hash before decode/parse; an earlier path hash alone would not
prove that capture. Its independent literal profile comes from the exact98009-
byte pinned bundle:1332 values+1088keys, max string270 and container depth3.
Dependency-specific limits are never applied to bootstrap, predecessor or results.
New future binding metadata requires renewed literal/profile review, not silent
reuse of today's bootstrap count.

## Grammar, bounded state and errors

A byte[MaximumDepth+1] stack has one document frame plus at most24 container
frames. It contains only grammar states; no user strings, maps or node objects.
Every value and property key increments the finite node count before parsing.
Array/object state transitions distinguish first/required entries, comma/colon
and matching closes; trailing commas, missing keys/values and trailing content
fail before SDK entry. Whitespace is only JSON space/tab/LF/CR. The scanner
requires one complete document, not a valid prefix.

Double-quoted strings are indexed without Substring/ToCharArray. Raw control
characters and invalid escapes refuse. Four-digit Unicode escapes are decoded
into one bounded integer; decoded UTF16 units are counted before SDK entry.
Surrogates must form pairs, including across escapes. Closing with a pending
high surrogate fails. A supplementary pair counts two units, preserving the
existing UTF16 contract. Maximum raw quoted token length is6*S+2, at most3074
for S512; a numeric token is at most20 and a literal at most5. The scanner uses
no document-sized temporary or token-sized allocation.

Fixed error codes carry no rejected text. The only typed scalar-length rejection
has the bounded witness observed21/maximum20, raised as soon as the21st unit is
reached; it never scans/copies the remaining oversized scalar. Ordinary parsed
schema/type/field/reason validation remains after the SDK. Duplicate property
errors or other unexpected SDK errors can refer only to admitted bounded tokens
and depths, not an arbitrarily long number. General ErrorRecord/CLR heap/RSS and
all retained capacities remain unqualified; this does not relabel them free.

## Raw admission and the still-unqualified reader lifetimes

The four-MiB numeric counterexample now rejects in the source scanner at unit21,
before any SDK invocation. The already captured byte[]/decoded text/hash owner
still exist and count. No null/scope exit/Dispose is physical GC reclamation.
The scanner adds at most25 byte payload cells per invocation, plus fixed scalar
state and bounded fixed diagnostic operands. Every such owner/action is a new
census obligation, not credited by an old price.

The universal char[32768] reader upper proposed by R8 is WITHDRAWN. It omitted
Boolean/null matching: pinned JsonTextReader MatchValue2366-2369 calls
EnsureChars(value.Length-1,true) without the ShiftBufferIfNeeded invoked by the
string/number paths. A bound on token length alone does not bound that append
sequence across distinct tokens. The whole R8 review is the exact physical
input326372761430E5CA0B14E7ED4DF4751D2E23713212927B1B0AE4141CE5F3578A.

A precise source counterexample uses an array open at offset0 and false tokens
starting at1021,2045,4093,8189,16381,32765. Put a comma immediately after each
first five tokens and a closing] at32770; fill gaps with spaces. Length32771,
nodes7, container depth1 and five-unit Boolean tokens meet the raw dependency
profile. Later semantic dependency-root refusal does not precede SDK parsing.
The matching append demand forces successive reader capacities through65536
chars, refuting the proposed32768 upper. This is independently reviewed source
arithmetic, NOT an executed fixture, measured capacity or new32MiB violation.

The stronger four-MiB placement hypothesis is also rejected. For the reviewed
14-node/exact4194304-byte candidate, final false atC-6 fits theC-1 data region;
separator EnsureChars(0,false) shifts zero remaining data and reuses char[C].
Its last forced growth is2^21 to2^22 chars, giving only a21,037,056-byte four-
owner lower. That lower establishes neither a new32MiB failure nor whole fit.
No new universal reader-capacity number is substituted here.

Complete independent typed-buffer/capacity/lifetime accounting remains HOLD.
It must include every string/number/Boolean/null/whitespace/separator path,
actual append and nonappend branches, all old/new arrays while copying, all
cumulative requested capacities without assumed GC reclamation, source text,
parsed/converted/clone graphs and every retained owner/failure prefix. Source
identity association does not prove reproduced binary/source equivalence.
Json string decode buffers, JToken/conversion graphs, output preprocessing maps/
lists/capacities, StringBuilder chunks, paths/indexes, returned strings/LF/UTF8,
all old retained buffers, failure branches and quarantine remain in the complete
post-correction lifetime worksheet. No missing owner is assignedzero.

## Real boundary control, inherited mechanisms and remaining gates

The normal actual helper receives full UInt64.MaxValue and Int64.MinValue legal
boundary literals. One new tenth runner control calls that SAME helper with the
21-unit184467440737095516150 and requires exact JSON_RAW_SCALAR_UNITS plus actual
observed21/maximum20 witness. None of these source calls has run. All original
nine runner controls and all C++ controls/source bytes remain unchanged.

R7's CPP/header are byte-exact here: all56 files,16,929,027 dense words,785
fraction pairs/carry, native inverse/full prefix/ramps/windows/bursts and BOTH
actual production endpoints per Q remain. No epsilon, coverage, threshold,
backend, ledger or accepted contract changes. R7's enforced7200 normal polls,
1800s child deadline/250ms pacing, cancellation/reap/two-second settlement,
quarantine, one rooted64KiB synchronous hash array, distinct async4096 buffers,
PE-console-before-launch gates, complete before/after fences, once receipt
encoding and final bundle fixed point remain. New control is an in-memory gate;
no extra child, reference file or physical artifact is introduced.

Inherited R7 mechanisms/primary identities are documented by the sealed R7
README/manifest, still physical inputs. R9 preserves ALL inherited1180 inputs,
adds the sealed R8five-file lineage and its whole source review, and adds only
the closed R8namespace. It recomputes inputs/bytes/chunks/include/fence/retention
from actual distinct physical identities. Equal bytes do not merge paths.
The unchanged840MiB retention cap and derived reservation are arithmetic only;32MiB scratch,
compiler temporary peak disk, universal CLR/CRT/OS CPU/heap/RSS remain held.

The independent R7 scalar critic found the bundle has56 FIVE-field references,
including relative_path, plus11 four-field artifacts; its old all67four-field
worksheet is invalid. Also no old helper subtotal or total is an accepted R8
price. Fresh whole functional source review precedes renewed full scalar and
lifetime censuses, corrected five-field graphs/new control/new helper work,
checked derivedU and independent review, actual private constinit data identity/
storage/lifetime review, then separate root one-attempt execution GO. Accepted
product2^28work/512MiBprepared/1GiBpeak remain unchanged. Vendor waveform,
quality, real resources, public recipe/legacy adapter/model/renderer/Ready and
actual tempo-follow activation receive no credit from this source package.

## Exact R9 source delta and next gate

CPP/header are byte-exact R8. The ONLY PS delta is the bootstrap node+key literal
outside every function, mechanically derived from this exact new manifest.
All helper bodies, raw integer/string/grammar rules, distinct other profiles,
ten real runner controls, C++matrix, limits, guards and child/cleanup behavior
remain byte-exact. R8profile fields retained under R8_narrow_corrections describe
historical R8; R9_metadata_correction contains the current derived profiles.
The dependency node expression is unchanged and uses the new include count.
The new namespace/ancestor seals and identity/profile/retention arithmetic are
metadata, not a new source allowance or a runtime admission result.

Fresh whole R9 functional review precedes complete renewed runner/both-child
scalar and controlled-payload census, checked wholeU/independent review, actual
constinit identity/storage/lifetime review and a separate root execution GO.
The R8raw mechanism's source acceptance does not grant those missing gates.
