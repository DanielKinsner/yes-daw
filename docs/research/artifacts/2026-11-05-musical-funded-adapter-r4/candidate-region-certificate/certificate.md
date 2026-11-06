# Finite guarded-domain UCRT dependency certificate, revision 1

This closes the candidate-file dependency graph for the stated log1p/log/getter
transition, conditional on the exact nine regions being matched at runtime and
the separate platform/lifetime/control/dispatch premises. It is not a completed
loaded-image observation, NativeBinding, initializer, price, source-fit or GO.

Candidate ucrtbase.dll SHA-256:
`DEDB7611E1ABC1FF3852DCAC3D4C93A048D729E25B061973B9969F22698F14CA`.
ImageBase is0x180000000; SizeOfImage0x14C000. Root's file-only region, complete
relocation and decoded-instruction records are pinned in certificate.json.
No target/native math, setter, allocator, compiler or GUI execution is involved.

The classes CodeMatched/TableMatched below name required runtime matching
obligations. They do not claim that a matcher has executed. MutableFMASeparatelyProved
names an excluded mutable dependency; UnreachableByInputPredicate is a source/
instruction-domain proof, not finite-run branch evidence.

## Domain and the only call that can execute

The actual append helper calls log1p only after establishing finite x>-1, where
x is its relativeChange double. Controls are separately qualified nearest,
all traps masked, FTZ/DAZ off. Both signs of zero and all finite subnormals are
included. The certificate does not assume ordinary BPM magnitudes or exact tick
representation below2^53.

The wrapper tests the exponent and absolute bits. For +/-0 it returns the input
double without calling log. A nonzero finite subnormal takes its common ordinary
path. The -1 comparison constant is bound by the wrapper-minus-one region.
Thus x<-1, x==-1 and unordered/nonfinite wrapper error edges cannot execute.

For nonzero finite x>-1, the bound+1.0 constant gives y=RN(1+x). Its range is
[2^-53, DBL_MAX]: the next representable double above-1 is-1+2^-53; for negative
x near-1 the cancellation is exact, and away from that boundary y is larger.
At the positive end, adding1 to DBL_MAX rounds to DBL_MAX, not infinity.
Therefore y is positive NORMAL. The sole reachable direct call is wrapper
RVA0x4F190 to log RVA0xA8C60. It has no IAT or variable target.

For log input y, exponent bits E are970..2046 and E-1023 is-53..1023. The matched
exponent-bias lane is1023; the subnormal comparison constant is-1023. The
subnormal-renormalization branches cannot execute. The matched+infinity mask/
threshold excludes nonfinite arms, and y>0 excludes all zero/negative error arms.
No reachable log instruction calls the error helper at0x981B0. No reachable
wrapper instruction calls the exception/errno helper at0x55750.

## Every instruction and control-flow edge

Root's instructions.json contains all296 decoded rows in the three matched code
spans, each with exact RVA, assembly and original decode line. No truncated
0xA9060 byte from the earlier partial listing is used. The following half-open
ranges classify every row that can execute in the guarded domain; all remaining
rows in those spans are unreachable error/renormalization/alignment rows:

| Required CodeMatched range | Role |
|---|---|
| [0x4F130,0x4F1CE) | Wrapper finite/zero/subnormal routing, common log call/correction, return |
| [0xA8C60,0xA8DD5) | Dispatch and SSE general path, including conditional instructions |
| [0xA8DE0,0xA8E63) | SSE near-one path and return |
| [0xA8F20,0xA9057) | FMA general path, including conditional instructions |
| [0xA9060,0xA90CE) | FMA near-one path and return |
| [0xCC390,0xCC39C) | Getter's four instructions |

Conditional instructions themselves remain reachable when their taken edges are
impossible. Every reachable branch/call edge is listed here; all ordinary
straight-line fallthroughs stay within the same CodeMatched ranges:

| Instruction RVA | Taken/call edge | Classification and reason |
|---|---|---|
| 0x4F15F je | 0x4F1CE | UnreachableByInputPredicate: finite x has exponent!=0x7ff |
| 0x4F163 je | 0x4F1BD | CodeMatched: zero/subnormal exponent; other finite values fall through |
| 0x4F172 ja | 0x4F1EE | UnreachableByInputPredicate: -1>x is false |
| 0x4F179 jp | 0x4F181 | Taken edge unreachable: finite ordered comparison; this target is independently reached by normal fallthrough |
| 0x4F17B je | 0x4F202 | UnreachableByInputPredicate: x!=-1 |
| 0x4F190 call | 0xA8C60 | CodeMatched: only reachable direct call |
| 0x4F1CA je | 0x4F1AD | CodeMatched: absbits(x)==0; preserves signed-zero low return |
| 0x4F1CC jmp | 0x4F165 | CodeMatched: finite nonzero subnormal common path |
| 0xA8C71 jne | 0xA8F20 | CodeMatched: FMA selection nonzero; zero falls through SSE |
| 0xA8CA0 jae | 0xA8EF0 | UnreachableByInputPredicate: absbits(y)<+infinity |
| 0xA8CC6 je | 0xA8E70 | UnreachableByInputPredicate: exponent is never-1023 |
| 0xA8CF8 jb | 0xA8DE0 | CodeMatched: abs(y-1)<1/16; both near/general alternatives covered |
| 0xA8D21 jbe | 0xA8EB0 | UnreachableByInputPredicate: y>0 |
| 0xA8F49 je | 0xA9150 | UnreachableByInputPredicate: exponent mask is not+infinity |
| 0xA8F57 jbe | 0xA9100 | UnreachableByInputPredicate: y>0 |
| 0xA8F75 je | 0xA90CE | UnreachableByInputPredicate: exponent is never-1023 |
| 0xA8FA9 jb | 0xA9060 | CodeMatched: same near-one split; both alternatives covered |

The reachable returns are0x4F1BC,0xA8DD4,0xA8E62,0xA9056,0xA90CD and0xCC39B.
The excluded subgraphs contain all other branches/calls in the decoded spans:
wrapper0x4F1CE..0x4F21C; SSE0xA8E70..0xA8F1A; FMA0xA90CE..0xA918A,
plus alignment gaps. Their internal branches cannot bypass a proven unreachable
incoming edge. The complete error-edge ledger is also recorded in certificate.json.

The matched getter zeroes EAX, compares the DWORD at0x139E60, sets AL on nonzero
and returns. Hence its result is0 or1. The getter and log's dispatch use the same
RIP-relative mutable word. No reachable instruction reads errno, FP status,
an IAT function pointer, a security cookie or another mutable configuration.
There is no reachable allocation/callback/control-mode write. Ordinary stack
save/restore and register operands are local, not immutable profile data.

## Exact index bound and all immutable data dependencies

For original y bits b, mask A=0x000FF00000000000 selects mantissa bits44..51;
mask B=0x0000080000000000 selects bit43. Both paths compute

`i=((b&A)+((b&B)<<1))>>44 = top8MantissaBits + roundingBit`.

Thus0<=i<=256. The sum is at most2^52, so it cannot overflow uint64. The three
indexed instructions read exactly8 bytes at base+8*i. Every257-element span is
therefore sufficient through its final element and exclusive end. Near-one paths
make no indexed reads. Packed high lanes do not feed the low-double result or
the integer index: the index is the low64 bits, and the relevant vector-mask
upper lanes are zero. Scalar arithmetic uses the low double; there is no
horizontal operation importing an unspecified incoming upper lane.

All TableMatched references reachable in the domain are covered by this ledger.
Widths are maximum actual operand widths, including128-bit masks:

| RVA / width | Required role and reachable use |
|---|---|
| 0x1020E8 /8 | Wrapper+1, y construction and correction |
| 0x103E10 /8 | Wrapper-1 domain compare |
| 0x107410 /16 | +infinity exponent threshold/mask, SSE and FMA classification |
| 0x107450 /16 | Mantissa mask |
| 0x107460 /16 | Exponent bias1023 |
| 0x107480 /16 | Top8 mantissa mask A |
| 0x107490 /16 | Rounding-bit mask B |
| 0x1074A0 /8 | Near-one denominator constant2 |
| 0x1074B0 /8 | y-1; unreachable renormalization additionally reads16 bytes here |
| 0x1074E0 /16 | Normalized mantissa OR constant0.5 |
| 0x107510 /8 | General polynomial coefficient |
| 0x107520 /8 | General polynomial coefficient |
| 0x107530 /8 | General polynomial coefficient |
| 0x107540 /8 | General polynomial coefficient |
| 0x107550 /8 | General polynomial coefficient |
| 0x107560 /8 | Subnormal exponent sentinel-1023 |
| 0x107580 /8 | Near-one threshold1/16 |
| 0x107590 /16 | Absolute-value mask |
| 0x1075A0 /8 | Near-one polynomial coefficient |
| 0x1075B0 /8 | Near-one polynomial coefficient |
| 0x1075C0 /8 | Near-one polynomial coefficient |
| 0x1075D0 /8 | Near-one polynomial coefficient |
| 0x1075E0 /8 | Split log(2) high coefficient |
| 0x1075F0 /8 | Split log(2) low coefficient |
| 0x109420+8*i /8 | High table: SSE0xA8DA0; FMA0xA9029 |
| 0x109C30+8*i /8 | Low table: SSE0xA8DAD; FMA0xA9036 |
| 0x10A650+8*i /8 | Reciprocal table: SSE0xA8D2B; FMA0xA8FCE |

The528-byte constant span is intentionally a conservative contiguous superset.
It includes immutable error-only constants0x107400/420/430/570/600/604/608,
upper-zero lanes, padding and unreferenced numerical lanes. No extra callee is
qualified by matching those extra bytes. Other wrapper error-only constants at
0x105FB0/0x102968/0x103E00 are excluded because their blocks are unreachable.
Every included constant/table byte is ordinary readonly numerical/bit data;
none is an address-bearing IAT/pointer/cookie field. The tables' values need not
be correctly rounded logarithms for this dependency/transition identity proof.

## Nine proposed compared spans and separate mutable extent

Exact roles, SHA-256 values and raw file offsets are in certificate.json, copied
from root's independently generated regions record. All spans are contiguous
within themselves; no sampled byte or table lane substitutes for full coverage.

| Role | Half-open RVA span | Bytes |
|---|---|---:|
| log1p code | [0x4F130,0x4F21D) |237|
| Both log arms | [0xA8C60,0xA918B) |1323|
| Getter code | [0xCC390,0xCC39C) |12|
| Shared constants | [0x107400,0x107610) |528|
| High table | [0x109420,0x109C28) |2056|
| Low table | [0x109C30,0x10A438) |2056|
| Reciprocal table | [0x10A650,0x10AE58) |2056|
| Wrapper+1 | [0x1020E8,0x1020F0) |8|
| Wrapper-1 | [0x103E10,0x103E18) |8|

Total: nine compared regions,8,284 bytes;1,572 code bytes and6,712 data bytes.
The three tables each contain257 complete doubles. No gap between separately
named table spans is silently treated as compared.

MutableFMASeparatelyProved is DWORD[0x139E60,0x139E64), inside writable .data,
not one of those byte-expectation regions. The runtime module bound must cover
this additional dependency, not merely the maximum compared end0x10AE58.
Require at least0x139E64 module extent (candidate SizeOfImage is0x14C000), and
support its committed/readable same-image page/lifetime before the getter is
invoked. A funded address-only page guard can do so without reading/matching
the word. Its checks are additional actual source work; nine/L8284 are not a
waiver of them. The word's initial getter value must remain invariant by the
separate closed-application interval argument.

## ASLR/relocations and certificate boundary

Root recorded the COMPLETE PE base-relocation directory:2856 bytes, six blocks,
1404 entries of types0/10, raw directory SHA-256 in certificate.json. Type0 is
padding; type10 affects eight bytes. Block pages are0xF9000,0xFA000,0xFB000,
0xFC000,0xFD000 and0x138000. Even the full possible page-offset plus8-byte
footprint of these blocks is disjoint from all nine proposed spans. Root also
checked every actual intersection and found none.

All relevant instruction addressing is register/stack, same-image RIP-relative
load/LEA/direct branch/call, or one of the bounded table indexes. ASLR shifts
both instruction and its same-image target by the same base, preserving those
displacements. Matched data are numerical/bit values, not load-adjusted pointers.
These nine exact regions therefore require no base-relocation normalization.
This does not assume arbitrary loaded bytes already equal the file: the actual
matcher still checks them, page safety and module associations before Clock.

Both FMA0 and FMA1 are qualified branches of this dependency graph. They may
return different last bits; the proof requires the same captured initial choice
through original build/replay/query, not equality between the two algorithms.
Actual machine support/startup selection, imported-module ownership/lifetime,
caller codegen, original private-prefix induction, strict restoration and all
same-origin source/resource funding remain separate premises.

No static instruction/table count here is an observed runtime trace or a work
price. This finite graph certificate is a prerequisite for independently reviewed
immutable region data; it does not define that data or any NativeBinding fact,
authorize execution, or supply renderer/Ready/Musical activation credit.
