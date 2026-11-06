# Windows MSVC precise-profile source/native argument, revision 1

The scalar-prefix induction is sufficient **conditionally**. The current evidence
does not yet establish a production NativeBinding: the selected runtime math
transition and caller-side restoration of extra observations remain unqualified,
and R3's actual caller has not been bound to emitted code. Finite equality is useful
reconciliation, not the premise that closes those obligations.

This argument adds no code, price, tariff, caller-supplied verification flag or
execution authority. All frozen probe sources, old headers and ADRs remain exact.
Only `argument.md` and `evidence.json` belong to this new directory.

## 1. Evidence boundary

Root's frozen probe receipt identifies MSVC toolset14.44.35207, compiler SHA-256
`88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D`,
Release `/O2 /Ob2 /MD /DNDEBUG /W4 /WX`, C++20, no LTO, and no FP override for the
precise numerical TUs. The separate observer TU was strict. Both console runs
returned0. Each measured576 build attempts,546 successes,22,284 public queries,
123,150 extra append calls,4,470 successful knot-byte comparisons and six
status-only knots. Those six do not reveal private cached doubles. Four sampled
native-status intervals changed; no errno-change interval was observed. That does
not establish preservation or exception absence between samples.

The subsequent root-owned file-association receipt records read-only dumpbin
results for existing files. Actual precise build/query and replay objects contain
the same displayed append-helper instruction sequence at offsets0x0..0x171.
Both have scalar SSE2 conversions/arithmetic, a double `movsd` result store, and
`call [__imp_log1p]` at+0x108. The old compiled-prefix builder, old frameForTick
and extra replay call that out-of-line symbol. The original builder visibly loads
the preceding double, calls the helper, then stores the returned double into its
next cache element. This is direct code inspection, independent of run equality.
It establishes these emitted object ranges, not future R3 code, linked-address
identity, all library internals or every native compilation.

The toolset folder version is not the compiler binary version: file metadata and
the run's `_MSC_FULL_VER=194435228` identify compiler19.44.35228. The candidate
System32 UCRT is10.0.26100.9711; candidate vcruntime140 is14.51.36247.0. These
versions are separately recorded rather than inferred from a directory name.

The executable imports `log1p` and `floor` through
`api-ms-win-crt-math-l1-1-0.dll`; it also imports `__setusermatherr`. An API-set
contract name is not an observed historical host-module address. The inspected
System32 ucrtbase.dll is a candidate file with a pinned hash, not a retroactive
loaded-module receipt. Its log1p export RVA is0x4F130; floor is0xF8040.

Root's next bounded file listings/data reads expose the relevant candidate path:
log1p compares against the confirmed-1.0 constant, forms y=1+x using the confirmed
+1.0 constant, calls `log` at RVA0xA8C60, and applies a correction. For the old
helper's guarded finite x>-1, nearest rounding gives positive normal y in
[2^-53, DBL_MAX]. Thus the wrapper's error/raise/errno branches are not reached.
This is a source/native domain deduction, not observed dynamic branch coverage.
The candidate `log` reads a global dword at RVA0x139E60: zero selects SSE2;
nonzero selects a distinct FMA path. MXCSR/control snapshots do not identify or
freeze this output-affecting selection. Root's final continuation covers that FMA
suffix. The confirmed+infinity classification constant at RVA0x107410 excludes
the nonfinite arms for y; positivity excludes both error-call arms. Across these
inspected valid-domain log arms, values derive from the operand, fixed tables and
the selected arithmetic path: no incoming errno/status read or control write is
present. Getter RVA0xCC390 reads the same dispatch dword and setter RVA0xCC3B0
can write it. This closes the candidate's narrow incoming-status/errno value
independence argument under stable selection and default controls; it does not
establish the actual loaded-module/interval association. Candidate floor similarly
dispatches through a separate ISA-state dword at RVA0x1384C8, though its exact
finite floor value has the documented mathematical contract below.

## 2. Exact source theorem

Let M be the immutable complete sorted map used by one actual successful old
Clock build, r its exact SampleRate double, and C[i] its private stored prefix.
Let H(r,s,d,a,b,c,p) denote one **actual old append helper** transition, including
its returned Boolean and resulting double bits. The theorem requires a qualified
H transition that is identical at original build, extra replay and real query
sites under the native facts in section5. It does not replace H with real-number
integration, another Clock, a copied logarithm expression or observed output alone.

For nonempty M the unchanged compiled builder assigns R positive-zero doubles.
Therefore C[0] has the bits of+0. For each i=1..R-1 it loads C[i-1] into a double,
sets s=M[i].tick-M[i-1].tick, calls H with d=s and the two exact BPMs/previous
curve, refuses if false, and stores the double into C[i]. Successful Clock
construction proves those old calls returned true and the complete cache exists;
it does not prove a native-frame bound on C[i].

Construct one scalar P, initially the same positive-zero double. At step i, load
the previous stored P into a double and call the **same actual helper** with the
same values, then preserve the completed double before the next step. Base:
P[0] and C[0] have identical bits. Induction: equal preceding bits and equal
arguments, with the qualified identical H transition, imply equal return Boolean
and result bits. The old success therefore implies the replay full segment also
succeeds, and its next stored P has exactly C[i]'s bits. This covers every needed
prefix without observing the private cache or allocating a second prefix array.

This argument includes ticks above2^53: the same signed-int64-to-double conversion
rounds the same integer; it does not assume every tick is exactly representable.
It includes finite prefixes above2^63, tiny/huge BPMs, underflow and signed zero.
No cast of such a prefix to native Frame is justified by cache construction.
If H's guards or numeric result differ, the induction fails; final status equality
cannot repair that failure. A successful prefix alone cannot predict a partial
ramp's slope/relative predicates without their separately funded classification.

## 3. Selected SDK topology and integer query

The old `frameForTick` uses the selected MSVC14.44 xutility `upper_bound` at
lines7462..7481, not a complexity-only binary-search promise. It unwraps the
vector range, initializes count=R, and repeatedly chooses mid=count/2. The exact
predicate is q<M[first+mid].tick. True keeps the lower mid elements; false advances
first by mid+1 and subtracts mid+1 from count. With strictly increasing immutable
integer ticks this ends at the first row whose tick is greater than q. Because
row0.tick=0<=q, upper>=1 and selected index j=upper-1 is valid.

A linear classifier can find the same j but does not thereby know this SDK's
exact true/false/iterator path. Exact source counts require deriving that specific
selected SDK walk as well, with its debug/iterator macros and wrapper leaves
bound to the actual build. This argument introduces no replacement SDK price.

At q=M[j].tick, the old query loads C[j] and takes no append call. The inductive
P[j] is bit-identical, including finite values that subsequently overflow the
native-frame range. For q>M[j].tick, nextTick and endBpm are the next row's values
if present, otherwise q and the selected row's own BPM. Sorted nonnegative int64
ticks make both subtractions safe. Starting from a *copy* of P[j], the identical
partial H arguments/transition give the same Boolean and result bits as the old
query. Preserve P[j] separately; an endpoint's partial result must not contaminate
the continuing prefix walk toward endpoint B.

Empty M has no cache induction or append: the old constant120 BPM expression is
evaluated in its exact parenthesized order, with the same rate and tick conversion.
It must be bound independently; algebraically equivalent regrouping is insufficient.

For `{q,0,1}`, fractionBounds returns zero fractions, low==high, and
frameForMusicalTime takes its frac0 route after one frameForTick. A false forward
lookup yields NumericDomain. Otherwise checkedNativeFrame rejects nonfinite or
negative values, then values>=2^63, before any floor/native cast. A finite
nonnegative value below2^63 is safe. The public low/high doubles are identical;
their difference is zero and both floors have the same native result, so the
integer Ok suffix cannot newly become BoundaryAmbiguous under the theorem.
Microsoft's [floor contract](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/floor-floorf-floorl?view=msvc-170)
specifies the largest integral value no greater than the input. Every integral
result needed here is representable in binary64 and the native range.

The two endpoint results then determine old resolve's first/second failure,
collapse or successful Window path, along with its scalar/overflow/eligibility
guards and ownership actions. Those guards still execute and need their own exact
source classification. Default eligibility512+2*bit_width(R) is no consumed debit.
The actual resolve is invoked once only after its entire reached path is funded;
post-call status/Window reconciliation is an additional check.

## 4. What MSVC and the ABI do establish

Microsoft documents default precise mode as retaining source expression order
and rounding at assignments, casts, arguments and floating returns. VS2022 does
not enable contraction by default; intermediates can retain machine precision.
Precise code assumes the default environment, while runtime environment access
requires strict treatment. On this x64 object path the inspected SSE2 arithmetic
and explicit double stores supply the relevant binary64 boundaries; there is no
observed fused operation in the inspected helper. These facts explain why equal
inputs can support H equivalence without assuming all inline source has one
code shape. [MSVC FP contract](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170).

The Windows x64 ABI makes x87 control and MXCSR control fields nonvolatile, with
documented exceptions. Ordinary callees restore modified controls before return;
the caller supplies the agreed environment. The standard MXCSR profile is nearest,
all traps masked, DAZ0 and FTZ0. Its six status flags are volatile. This contract
can bridge ordinary successful allocator/new-handler returns before the original
prefix walk, provided the selected calls obey the ABI and there is no agreement
to leave changed controls. It is not a proof of controls inside an opaque callee,
a callback's temporary environment, exception-unwind restoration, or sticky-state
restoration. [Microsoft x64 ABI](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170#fpcsr).

The old Clock performs owning allocations/vector initialization before computing
prefixes. Installed `new_scalar.cpp` loops through malloc and `_callnewh` on
failure; installed UCRT `new_handler.cpp` reads a mutable registered callback.
This source is informative but is not yet proven to be the selected linked
allocator body. A handler returning normally under the stated ABI restores
controls before the later prefix computation. A caught allocation failure gives
no successful cache and therefore no induction credit. A nonconforming callback,
unqualified replacement allocator, changed-control agreement or asynchronous
context mutation is outside the conditional theorem, not silently proved absent.

The read-only search of application `src` found no new-handler setter. That is
partial source absence, not proof about linked libraries, runtime registration,
concurrent setters or a null handler over an interval. Standard get_new_handler
returns the current handler only; setters install allocation-failure callbacks.
[Microsoft new-handler contract](https://learn.microsoft.com/en-us/cpp/standard-library/new-functions?view=msvc-170).
`ScopedNoDenormals` intentionally changes controls in an audio render scope;
therefore an app-wide claim that all threads/scopes always use default controls
would be false. The future captured-request worker must establish its own scope.

## 5. Sufficient NativeBinding facts, with current gaps

These are conjunctive facts, not a caller Boolean or a newly priced permission:

1. **Immutable input and history.** One retained actual successful Clock, exact
   complete row/rate identity and lifetime, no mutable alias/data race, and the
   original successful build belongs to the same captured request/native session.
   Fresh endpoints and output preserve the accepted alias/ownership laws.
2. **Actual numerical sites.** Pin the entire source/toolset/options/SDK identity
   and all emitted original-build, replay and query arithmetic/conversion/store
   paths. Any compiler intrinsic/constant folding must have the same transition
   on the complete supported domain. The frozen probe objects support their own
   H ranges. They do not certify an uncompiled R3 caller or future app binary.
3. **Controls and transfer.** Establish default controls before the *original*
   Clock; the actual preparation scope contains no uncontrolled setter; qualified
   allocator/library/callback transfers restore ABI controls before each relevant
   arithmetic phase, including admitted unwinds. Strict endpoint checks are useful
   rejection checks but cannot supply missing interval/callee facts.
4. **Selected runtime numerical transition.** Identify the actual resolved math
   module/import/CPU-dispatch path and qualify its log1p return bits for identical
   input bits under the accepted controls, independent of incoming errno/status
   and allowed prior calls. Qualify internal temporary-control/callback behavior
   and any output-affecting global/cache state. This is not a demand for correct
   rounding: a stable identical approximation is sufficient for this induction.
5. **Extra-work effects and timing.** Preserve the caller-visible environment
   around the extra classifier/replay if the new helper promises the old effects.
   Restore the selected state before real resolve and on every earlier exit,
   with explicit success/failure handling and funded cleanup. Do not erase real
   resolve's legitimate effects afterward. Reconcile actual old output separately.
6. **Complete same-origin source closure.** The actual native checks, predicate
   classification, replay, SDK topology, state capture/restore, query debit,
   guaranteed invocation, reconciliation, refusal and destruction all need prior
   source-derived reservations and literal census. No extra semantic tariff,
   inherited receipt credit, cap change or after-the-fact reimbursement applies.

Facts1/2/3 are only partially established for future R3. Fact4's current concrete
barrier is the candidate-to-loaded-module association plus the stability of the
identified log-dispatch dword across original build/replay/query. The valid-domain
native valid-domain paths are inspected; no whole-library catalogue is requested.
Microsoft says UCRT math can choose implementations at runtime and
can differ by CPU/UCRT version; compiler and import-library identity alone do not
identify that selected implementation. [UCRT math behavior](https://learn.microsoft.com/en-us/cpp/c-runtime-library/floating-point-support?view=msvc-170).
The log1p page gives its mathematical/special-input behavior but no all-input
bitwise independence theorem for incoming status/errno or mutable dispatch.
[log1p contract](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/log1p-log1pf-log1pl2?view=msvc-170).
The documented `_get_FMA3_enable` reports the current selection; startup selects
it according to CPU support and `_set_FMA3_enable` can change it. Microsoft
expressly allows different results between its two algorithms. A read-only getter
therefore supplies an attainable actual profile observation, but equal endpoint
getters cannot exclude an intervening change-and-return. An interval fact must
come from the admitted process/callback source contract, not a Boolean supplied
by its caller. An attainable closure is to show CRT startup selection completes
before captured-request work and no reachable setter/direct writer exists in the
admitted app, dependencies, active threads or permitted allocation callbacks over
that interval. The current source grep is partial support, not this whole closure.
It needs no new setter, synchronization mechanism or architecture. Arbitrary
injected/ABI-violating modules are outside the ordinary supported application
contract, not a reason to declare an unknowable interference blocker.
[FMA3 selection contract](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/get-fma3-enable-set-fma3-enable?view=msvc-170).
The old helper's log input is finite and greater than-1; denormal inputs still
belong to that domain. No domain/rate/map whitelist can replace this obligation.

## 6. Status/errno are separate from value-path equivalence

The selected old source does not branch on errno or read FP status. Masked
arithmetic comparisons/classification are determined by their operands and
controls; sticky flags do not themselves appear as old source predicates.
That source fact can make incoming status/errno irrelevant to the *source* path.
It does not prove that an opaque selected libm implementation never reads them,
nor that extra calls leave the caller's observable state unchanged. Those are
different obligations. The frozen observer did not save/restore them and never
claimed to do so.

There is an exact SDK restoration caveat: installed fenv.h defines FE_ALL_EXCEPT
as five standard flags and excludes `_SW_DENORMAL`, defined separately in float.h.
Consequently fegetexceptflag/feclearexcept/fesetexceptflag(FE_ALL_EXCEPT) alone
cannot establish restoration of all six raw MXCSR status bits. Neither those
declarations nor the probe observes/restores raw x87 state on MSVC x64. A bridge
claiming only the five flags must say so; a whole-native-status promise needs
actual qualified capture/restore source covering the remaining relevant state.
No intentional trap, environment-mode change or test of raised exceptions is
authorized by this argument.

The now-readable but **unsealed** R3 source addresses the five-flag omission:
native-observer.cpp explicitly restores all six raw MXCSR status bits while
retaining the current control bits, then writes saved errno. Its fexcept_t calls
remain limited to the standard five flags. The caller restores before its exact
query debit/one resolve, and has a scope-cleanup restoration path; it does not
restore after real resolve. These are actual provisional source observations,
not native restoration results or whole-source acceptance. Return codes, final
observer effects and every funded early-cleanup branch still need the complete
source review/census and selected native bridge qualification. No raw x87
restoration claim is supplied.

R3's provisional `prefixTo` has one named double prefix, a named double frame for
each actual old helper call and a completed `prefix=frame` assignment. Its endpoint
copies the prefix before partial integration, so a partial result does not replace
the full prefix used toward B. Its search mirrors the selected SDK count/half/
predicate topology. Its NativeBinding declares private immutable evidence data
without defining it; nonzero digests are explicitly completeness checks, not proof
validation. This is the correct conditional shape for the theorem, while actual
R3 emitted-site identity, immutable caller history and complete source obligations
remain unbound. Unsealed R3 bytes are not promoted to a pinned accepted artifact.

## 7. Attainable headless next step

**A - recommended:** complete the actual source/callgraph closure and whole R3
review. Root has already obtained the needed candidate log1p/log valid-domain
instructions, classification constants, getter/setter association and fenv masks.
The remaining source step is the concrete startup/no-reachable-dispatch-writer
argument covering app/dependency threads and permitted callbacks. Do not expand
to unrelated vendor paths or replace that argument with repeated snapshots.
The files remain candidates until a
separate authorized headless loader receipt establishes actual IAT target/module
association for the new binary. Do not rerun or edit the frozen whole probe to
retroactively add those observations. Existing object listings need no recompilation.

After R3 whole source review, an explicitly identified console/native qualification
can record its actual imported math target/module hash and emitted sites, then
mechanically check the qualified strict bridge's restoration on normal/early
paths. Such finite checks support, but cannot replace, the reachable-code state
independence argument. No GUI, audio/device startup or Dan-operated check is needed.

**B:** retain HOLD at this exact leaf if its reachable native implementation or
restoration cannot be qualified. Record the unsupported dependency and continue
safe source/census work. Switching only the new TU to strict, sampling more cases,
or copying the same expression does not solve an unproved original-cache/native
transition. An algorithm/runtime replacement would be a separate reviewed change,
not an inferred permission from these two GREEN measurements.

This provides a conditional sufficient theorem and a narrow, testable next
hypothesis. It supplies no bound query prices, source-fit certificate, new native
binding, recipe/renderer/Ready evidence, production activation or complete Option A.
