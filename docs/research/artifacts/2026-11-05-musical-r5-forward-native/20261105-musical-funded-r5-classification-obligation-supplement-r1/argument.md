# R5 classification leaf — narrow supplement

This supplements the sealed R5 forward/native argument without changing any of
its files or source. Candidate classification purity is now established for the
named file. Actual combined-call/provider/loader association remains HOLD.
No Data, price, runtime query, new anchor or R6 source is authored here.

Actual R5 compute object has direct REL32 `_dclass` references, symbol0x767
UNDEF External, not `__imp__dclass`. H uses that symbol at+CF/+ED and classifier
at+149/+193. SDK corecrt_math.h191 declares short __cdecl _dclass(double);
the C++ isfinite template435..438 tests fpclassify(_X)<=0. The double overload
calls _dtest(&_X), with the actual compiler lowering these selected classifications
to by-value `_dclass`. cmath1103 imports the C isfinite name. Existing probe
imports put `_dclass` and log1p in the same api-ms-win-crt-math-l1-1-0 descriptor;
that historical probe is not the future R5 image's import observation.

Root's new candidate-file read, raw exit0, has output SHA
`AD5F1A1B77D4780BB6C3A446FC5E8A040E513224E8C600ED92F8614596E5D913`.
It names the same UCRT candidate SHA
`DEDB7611E1ABC1FF3852DCAC3D4C93A048D729E25B061973B9969F22698F14CA`.
Only [RVA0x688F0,0x6894C),92 bytes, is the actual `_dclass` leaf. The requested
listing range continues through padding and other functions; it is not the leaf
extent. The five reachable returns are0x68918/0x6892A/0x68930/0x68945/0x6894B.
All branches stay inside this extent; no call/global data read/write occurs.

Let E=(bits>>52)&0x7ff and F=bits&0x000fffffffffffff. The complete partition is:

| Bits | Return short | finite predicate |
|---|---:|---|
| 1<=E<=2046 | -1 normal | true |
| E=0,F!=0 | -2 subnormal | true |
| E=0,F=0 | 0 zero, either sign | true |
| E=2047,F=0 | 1 infinity,either sign | false |
| E=2047,F!=0 | 2 NaN,all payloads/signs | false |

This covers every binary64 bit pattern by algebraic partition, including signed
zero,subnormals and signaling/quiet NaN payloads. xorps/movsd/movq are register
bit moves/zeroing, then all classification uses integer masks/tests. There is no
floating arithmetic/comparison,mode/status/errno read or write,dispatch lookup,
table,index,callback or incoming-state dependency. It does not quiet NaN through
floating arithmetic. Ordinary return-stack ABI memory remains caller-owned;
"no global read" does not pretend a ret instruction reads no stack.

Consequently identical input bits at actual old/new `_dclass` calls yield the
same <=0 decision with no FP/status/control/errno effect **if their actual selected
body is this qualified leaf or an independently qualified equivalent stock
implementation**. The old nine log spans do not prove that association.

The accepted complete PE relocation-directory record has block pages0xF9000,
0xFA000,0xFB000,0xFC000,0xFD000,0x138000. Its full affected footprints are disjoint
from this earlier0x688F0 span. All leaf constants are immediate code bits, with
no address-bearing data; same-image local branches preserve ASLR displacement.
The existing candidate-file proof can therefore be extended narrowly to these
92 bytes without a broader vendor graph or a new native numeric experiment.

## Supported routes before considering a source change

**A — recommended: exact final static call/import plus ordinary loader/source
association.** Once a genuine combined image exists, retain its object/link map,
selected default-library/import member, direct `_dclass` call relocations and
actual thunk/IAT descriptor. Establish where that thunk is resolved, its host
module/export/RVA and interval stability under the supported loader and the
already closed application/runtime source contract. Tie it to the same qualified
UCRT identity used for math, not merely a similar DLL filename or the historical
probe. This is an actual association obligation; current R5 no `_dclass` address
query does not make it automatically true.

If a reviewed linked/provider/loader proof already establishes the exact mapped
stock file/body and no reachable redirection, the SDK value contract plus this
body purity proof suffices; an added runtime anchor is unnecessary. Alternatively
the generic R5 region matcher can compare the92-byte leaf as an additional Code
region, while that **separate call-target association** still has to be proved.
This would conditionally change compared extents M9/L8284 to M10/L8376, leaving
address-only D/A unchanged. It is a prospective finite data option, not a new
definition or selected profile. Generic source funding changes by
`U_region+92*(U_page+U_byte)`, using independently accepted coefficients only;
the currently criticized numeric proposal is not used to price this extension.

**B — SDK value contract alone:** it establishes what isfinite must return, but
does not by itself associate this unresolved call/body or certify its incidental
native-state behavior. It can close the value-classification lemma, not the whole
ActualCombinedCaller/RelevantControl/FullDomainClassification premise without
an independently accepted supported runtime contract. Do not label the missing
association solved by declaration spelling or finite outputs.

**C — explicit classification anchor only if A cannot close:** a future reviewed
successor could take the exact `_dclass` function address without invoking it,
check module/export association through a fixed strict leaf and bind the extra
Code region. That requires whole source/census/bootstrap/ABI/qualification review
and can encounter an executable import thunk rather than a DLL address. It still
must show the actual H/classifier direct calls select that same entry. An anchor
alone is not a substitute for thunk/link correspondence. No source R6 is proposed
until the exact unsupported A step is identified.

The narrow next step is therefore the already-required final combined linkage/
provider/loader association, with this92-byte candidate purity proof attached.
Do not add a runtime query, invent profile booleans, bind Data, raise caps or
broaden the frozen nine-region certificate silently. NativeFact7/8/16/17 retain
their explicit actual-association/whole-proof conditions.
