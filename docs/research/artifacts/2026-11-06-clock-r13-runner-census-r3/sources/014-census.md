# ClockOracle R4 independent C++ census: HOLD / NOFIT

This is a source-only authoring result for the exact five-file `clock-oracle-r4-source-prototype` sealed by manifest `2D550D6DDDFF29DAC9BF589C3B23A52531A840E1BAFCC0B6AD5EA0591AF7E97B`. It defines no Catalogue data and grants no execution, binding, syntax, compiler, test, oracle, or production credit. Every prototype guard remains false. All 76 catalogue upper prices remain HOLD_UNBOUND. This document is an exhaustive family/obligation inventory and a decisive partial lower census, **not a completed 76-price upper certificate**.

The existing quote composition cannot fit the unchanged combined 2^34 source-action cap under the explicit scalar-action convention below: a disjoint hot-path subtotal alone is at least **17,821,237,248**, exceeding **17,179,869,184** by **641,368,064**. No launcher, inverse, I/O, setup, build, fraction generation, cleanup, or catalogue/quote/Meter work is included in this floor. Their work is positive. Missing upper expansions cannot repair that inequality.

## Scope and unit convention

The intended admitted protocol is counted prospectively. The current default executable path would refuse immediately at its false guard; that refusal is not a claim that the intended producer/verifier fits. No authored program was run. Source was read, exact files were hashed, arithmetic was performed independently, and only the three authorized census documents were authored.

One unit is one reached scalar source action: a scalar value read (including a literal occurrence), scalar initialization/write, arithmetic/bitwise/conversion/comparison/boolean operation, selected branch/conditional, call dispatch, return, loop condition/entry, or increment/decrement operation. A compound update includes its read, operation and write. A call pays its receiver/reference/argument evaluation and dispatch in the caller; its body is an exclusive callee row. Scalar by-value parameter bindings, implicit/default member initialization, aggregate element construction/copy, exception object construction and retirement must also be counted when closing an upper row. The lower proof deliberately omits many of those positive actions. It does not treat casts, fixed constants, default objects or source-visible wrappers as free because an optimizer may remove them.

An lvalue address/reference used as a call argument has one source evaluation. Member-name selection itself is not an extra scalar read on top of the selected member value. A vector/array/string member call still evaluates the receiver and its index/other arguments. A built-in subscript or dereference performs its visible address/value action; a library `operator[]` additionally has a call and its actual body. Temporary aggregate fields require their scalar initializations; optional named-return copies are not assumed elided in an upper bound. Guaranteed prvalue elision does not manufacture an extra copy.

Bulk OS/CRT/CLR/libm primitive internals remain opaque CPU/RSS; the call site and visible C++ wrapper source are counted. Compiler CPU, heap/RSS and temporary peak disk remain explicitly unqualified under Choice A. The visible MSVC iterator/vector/array/string/helper source is not recategorized as an opaque primitive to fit the cap. Compile-time type selection and untaken `if constexpr` branches are not runtime paths; the selected runtime bodies still count. `/O2` grants no source-action discount.

## Disjoint lower proof

References below use `CPP` = the pinned prototype `clock_stage.cpp`, `TIME` = pinned `src/engine/Time.h`, `QC` = pinned `independent_clock.h`. `source-pins.json` gives the exact physical identities. The expression rows are selected positive source actions, not arbitrary round prices and not an upper certificate. Every omitted operation is left positive/unpriced; no omitted family receives a zero price.

### RationalCut selected floor: 224

`CPP:211-214`. This covers the rational cut and its common high-endpoint, endpoint-comparison and cut-admission helpers. It excludes both clock frame bodies. A normal admitted non-carry fraction path witnesses the selections; the family upper must cover that path. The two frame dispatches are included once here, and their body prices are disjoint below.

| Reached expression group | Selected units | Literal expansion / exclusions |
| --- | ---: | --- |
| First rational-query gate | 14 | `i<=N`:3; second `i<=max()/153600`:5; AND:1; reason:1; dispatch:1; successful gate `ok,!ok,branch`:3. The body of `max()` is omitted. |
| `product`, `whole`, `remainder` | 12 | Three groups of two operand reads, one arithmetic operation and one initialization: `3*4`. |
| Remainder-cache-key gate | 10 | Two reads, modulo, zero literal, equality, reason literal, dispatch, successful gate3. |
| Cache-pair indexing | 4 | Only remainder read, `g` read, division and cast. Array access/body, pair reads and pair initialization are omitted. |
| Tick initialization | 5 | `S`, whole read, conversion, addition, initialization. |
| `highEndpoint` caller and selected body | 28 | Caller two reads+dispatch3; helper gate16; selected no-carry return9. Gate16 = comparisons3+3+3, two ANDs, reason+dispatch, successful gate3; body of `max()` omitted. Return9 = comparison3, conditional1, tick/fraction reads2, two field writes2, return1. |
| `endpointGate` selected body | 7 | Whole equality3; two fraction argument reads2; equality of returned bit values1; AND1. Both `bits` bodies, all dispatches, argument copies, NOT and branch are omitted. |
| Two frame call dispatches | 2 | Dispatch only. Receiver/Native argument evaluation and result initialization are omitted. |
| `admittedCut` caller and body | 42 | Caller two reads+dispatch+return4; body boolean-expression24, failed-condition test3, aggregate return11. Math wrapper bodies are omitted. |
| **Total** | **224** | `14+12+10+4+5+28+7+2+42`. |

The 24 boolean-expression actions in `admittedCut` are: `isfinite(low)`2, `isfinite(high)`2, connecting AND1, `high>=low`3, AND1, `high-low<=1.0/1024`7, AND1, two floor call sites/equality/final AND6, and initialization1. Its return11 includes the true literal/field write2, low/floor/cast/floor-field write4, low and high field reads/writes4, and return1. This is reached ordinary acceptance work, not a failure-only tuple.

### Production fractional-body selected floor: 243

This is the **body** invoked by either rational endpoint after its dispatch above. It includes `productionFrame`'s own call to the checked production API, that API's exclusive body and the reached whole-Tick API's exclusive body. It excludes every search body, append body, scaled helper body, vector/iterator/array body, `hasValidFraction`, `SampleRate::isValid`, `isFinitePositive`, `checkedNativeFrame`, libm/classification wrappers, parameter bindings, and failure/cleanup work.

The selected path is an admitted nonzero fractional interior with a next segment, e.g. a whole Tick strictly inside the clip's active segment. A three-entry constant map witnesses the constant-family selections; a ramp interior executes all selected common groups plus additional ramp work. Jump-map interiors also exist. These are lower limits on the required **family upper prices**, not a claim that every actual invocation takes the maximum path.

| Exclusive source body | Selected units | Expression groups in source order |
| --- | ---: | --- |
| `CPP:207 productionFrame` | 16 | output initialization2; checked-call receiver/Native field evaluations+field initialization/reference/dispatch/status initialization8; status comparison/branch4; returned read/return2. |
| `TIME:728-789 frameForMusicalTime` | 122 | `2+8+3+6+2+6+4+2+4+4+7+4+2+2+3+7+8+5+14+4+2+4+7+6+6`. Expanded below. |
| `TIME:634-678 frameForTick` | 105 | `2+8+2+11+4+5+7+7+9+9+26+2+3`. Expanded below. |
| **Total** | **243** | `16+122+105`. |

The 122 groups are: frameOut initialization2; input predicate including its source call site8; checkedBuilt predicate3; nonempty/empty-call predicate6; local frame initialization2; whole-Tick call/NOT/branch6; zero-fraction test4; acceptedWhole initialization2; checkedNativeFrame call/status initialization4; status branch4; k initialization7; finite-positive call/NOT/branch4; bpm initialization2; slope initialization2; nonempty predicate3; upper-bound caller/upper initialization7; begin/subtraction/cast/index initialization8; bpm subscript caller/read/write5; selected next-segment/curve condition14; bpm-check caller4; extra initialization2; slope branch4; scaled caller/NOT/branch7; overflow predicate6; final checkedNativeFrame caller/return6.

The 105 groups are: frameOut initialization2; sample-rate/tick guard caller and condition8; empty predicate2; upper-bound/begin/subtraction/cast/upper initialization11; index initialization4; prefix subscript caller/read/frame initialization5; interior predicate7; haveNext initialization7; nextTick selection9; endBpm selection9; append argument evaluation/dispatch/NOT/branch26; output assignment2; isfinite caller/return3. The append26 includes sample-rate read1, segment subtraction argument6, delta subtraction argument6, start-bpm subscript argument4, end-bpm read1, curve subscript argument4, frame reference1, dispatch1, NOT1 and branch1.

The 243 floor is valid separately for `ProductionForwardConstant` and `ProductionForwardRamp`; it is not a shared append or scaled price. No old 50/176/352/205/append/word coefficient was used.

### CachedPort fractional-body selected floor: 69

`CPP:182-197`. This excludes the refill body and all scaled/checkFrame/library bodies. It applies to the success fractional path; a ramp executes at least the selected non-ramp tail's work and additional relative/log calls.

| Group | Units |
| --- | ---: |
| Native-domain require expression/caller/successful require | 16 |
| Saved-whole comparison/branch | 4 |
| Fraction-zero comparison/branch | 4 |
| Local-BPM require expression/caller/successful require | 11 |
| Extra initialization | 2 |
| Slope comparison/branch | 4 |
| Selected scaled-call arguments/dispatch and require caller/body | 10 |
| Extra/whole-frame overflow require | 10 |
| Result read/read/add/initialization | 4 |
| checkFrame argument/dispatch only | 2 |
| Returned result read/return | 2 |
| **Total** | **69** |

The native require16 is three comparisons9, two ANDs2, reason/dispatch2, and require's read/NOT/branch3. The local-BPM require11 is isfinite call-site2, positive comparison3, AND1, reason/dispatch2, require3. The scaled caller/require10 is four arguments4, scaled dispatch1, reason/require dispatch2, require3. The overflow require10 is reads/subtraction/comparison5, reason/dispatch2, require3. Function parameter bindings and implicit successful returns are omitted throughout the lower proof.

### Existing `Cost::childUpper` composition

`CPP:79-99`, independent arithmetic:

- Rates `[44100,48000,96000]`; N `[441000,480000,960000]`; g `[600,19200,38400]`; nine maps each.
- Quoted queries: `9*(441000+480000+960000)+27*11 = 16,929,297`.
- Quoted zero queries: `9*(600+19200+38400)+27*7 = 523,989`.
- Quoted fractional queries: `16,929,297-523,989 = 16,405,308`.
- Seven constant maps: fractional `12,759,684`, zero `407,547`.
- Two ramp maps: fractional `3,645,624`, zero `116,442`.
- Cut floor in both children: `2*16,929,297*224 = 7,584,325,056`.
- Verifier fractional body floor: `2*16,405,308*243 = 7,972,979,688`.
- Producer fractional body floor: `2*16,405,308*69 = 2,263,932,504`.
- Combined selected floor: `7,584,325,056+7,972,979,688+2,263,932,504 = 17,821,237,248`.

Thus `producerUpper >= 6,056,095,032`, `verifierUpper >= 11,765,142,216`, and `runnerUpper+producerUpper+verifierUpper >= 17,821,237,248+runnerUpper > 2^34`. These are lower bounds, not proposed upper reservations. Every other term in childUpper is excluded, including its required positive fixed/zero/outer/word/refill/inverse rows. No ledger split, reset, dropped endpoint or reinterpretation of a primitive can produce a binding for this prototype under this convention.

Actual normal visits are different: per fixture get(0), get(N), then N+1 dense calls gives N+3, not N+11. Actual zero visits are g+3 and fractional visits N-g. Across 27 fixtures these are 16,929,081 / 523,881 / 16,405,200. The original quote conservatively reserves eight more queries per fixture, four extra zero and four extra fractional. The proof uses the actual `Cost::childUpper` prepayment counts; it does not fabricate execution events or claim a measured work count.

## Actual transitive call graph and exclusive ownership

`main` -> arithmetic/mode/manifest/decimal gates -> Cost::validate -> childUpper(false) and childUpper(true) -> whole composition/admission -> Meter -> run. This quotation/validation sequence occurs in **each** intended child, not once globally. A child shares one Meter.used/reserved/prior; there is no counter reset. run prepays ChildFixed once, fixtureUnits before each fixture's first query, producer refill before its refill body, and inverseUnits before its inverse records.

Every Cost helper, Catalogue helper and Meter::add invocation is real counted instrumentation. It must be expanded once in the child source prepayment (e.g. disjoint fixed instrumentation counts plus hot refill instrumentation), with no recursive pricing execution and no free self-check. Reading a literal bound is not a second execution of the source body it prices. Conversely the repeated price lookup in every producer cache miss cannot be charged merely once per process. No instrumented helper has a completed upper price in this document.

- `cut` -> highEndpoint -> endpointGate/bits; two CachedPort::frame OR two productionFrame -> admittedCut. Both checked production endpoint APIs remain present on every verifier Q, including equal low/high binary64 fractions; there is no deduplication credit.
- CachedPort::frame miss -> Cost::price -> Catalogue accessor; Meter::add; native counter/cursor gates; base.frame({whole,0}); local k/BPM/slope. CachedPort fractional tail -> qc::scaled once or twice -> libm/classification wrappers; checkFrame/require. Cursor index removes the former second manual binary search but does not remove the actual base whole-Tick lookup.
- qc::NumericAClock::frame -> segmentFor -> upper_bound; prefix; optional qc::append; checkFrame; optional native fractional local law. Whole, endpoint, full-native inverse residual and prefix paths are distinct obligations.
- productionFrame -> frameForMusicalTime -> frameForTick -> upper_bound and appendTempoSegmentFrames; nonzero fraction additionally performs another actual upper_bound and local scaled law. Zero path retains the original whole-Tick order and returns through checkedNativeFrame; it is not the nonzero price with an arbitrary deduction.
- Producer inverse -> qc::inverse (prefix upper_bound/scaled/expm1) -> qc::frame full native fractional -> residual. Verifier inverse -> actual musicalTimeForFrame -> inverseGate/bits -> actual productionFrame fractional/zero -> residual. No inverse API uses a borrowed 176/352/205 price.
- Stream::word write/read dispatch -> aligned fast or actual put/exact fallback. put/exact -> array helpers, min, memcpy, full flush/refill, typed gates. finish -> actual fflush/fgetc/ferror/fclose; destructor -> bounded no-throw close witness. CRT bodies stay opaque, visible callers and footer do not.
- metadata -> productionRows/field/Text add/digits/quoted -> windows and burst loops. readText -> Stream/exact/finish -> textGate -> textWitness. textWitness executes even on successful text comparisons because it is passed to the explicit-witness gate.
- controls/expect -> actual normal APIs, typed operand equality, close-state checks and printf. Malformed stream controls use actual owned files and real fallback loops, not synthetic gate observations.

## MSVC iterator path: read source, no generic 50 price

Pinned `/D_ITERATOR_DEBUG_LEVEL=0` and C++20 are declared compiler arguments (`source-manifest.json:10096+`), not executed preprocessor evidence. `vector` const begin/end (`1845/1855`) construct actual `_Vector_const_iterator`; constructor (`vector:48`) initializes _Ptr and calls _Adopt. `_Iterator_base0::_Adopt` (`xmemory:1191`) has an empty visible body; its caller evaluation/dispatch still count. Const vector size/empty/subscript bodies (`vector:1908-1946`) are separate and not free.

`xutility:7462-7487 upper_bound` reaches `_Adl_verify_range`, two `_Get_unwrapped`, `distance`, each division/next/predicate/update loop, `_Seek_wrapped` and return. At debug level0 the vector friend `_Verify_range` is absent, so its instantiation does not execute a range-check body. `_Get_unwrapped` reaches vector::_Unwrapped (`vector:216`) -> `_Unfancy_maybe_null` raw-pointer specialization (`xmemory:79`) -> return pointer. `distance` on raw pointers (`xutility:1660`) returns subtraction. `next` (`1682`) calls advance (`1632`), whose raw random-access body performs `_Where += _Off`, then returns. `_Next_iter` (`1677`) performs actual prefix increment and return. `_Seek_wrapped` (`1477`) calls forward (`type_traits:1501`) and vector::_Seek_to (`vector:220`) -> `_Refancy_maybe_null` pointer specialization (`xmemory:312`), then writes _Ptr. Caller iterator subtraction (`vector:155`) includes _Compat's debug0 body, pointer subtraction/cast and return. Prefix search's no-comparator overload also constructs less and calls the comparator overload.

Three/four-entry generic lookup can take at most three iterations. An admitted interior query >=S in the pinned maps needs two; the final endpoint similarly needs two but takes different update branches. Native inverse lookups may select the prefix segment before S because floor(A) can be below the exact prefix frame. That case and control lookups must be derived separately. A family upper cannot silently use the two-iteration interior count for all endpoints, prefix/inverse and controls. No generic iterator numeric price is closed here. The quoted NOFIT proof excludes all of this source, so no unresolved iterator price can reverse it.

`corecrt_math.h:321` double fpclassify -> `_dtest`; `:435` isfinite -> fpclassify/compare/return. `<cmath>` exports that double overload; the wrapper is visible although `_dtest` internals are opaque. Double frexp/ldexp/log1p/expm1/floor declarations (`corecrt_math.h:537-551`) name primitive bodies, unlike the visible classification wrapper. `<bit>:31` bit_cast returns the builtin and must be charged with its wrapper. Array size/data/subscript (`array:501/529/537/562/566`) and forward/move/addressof (`type_traits:1501-1524`) remain visible obligations. Remaining string/vector allocation/copy/destruction transitive source has not been falsely marked fully expanded.

## Exact visit and lifetime inventory

These are source-derived counts/bounds, not executed observations:

| Obligation | Producer | Verifier |
| --- | ---: | ---: |
| CachedPort / qc::NumericAClock fixture constructions | 27 | 27 |
| Production clock normal fixtures + common carry baseline | 28 | 28 |
| Additional prefix/ramp mutant production builds | 0 | 2 |
| Total qc/production clock builds | 55 | 57 |
| Fraction-cache constructions | 3 | 3 |
| Original fraction comparisons | 785 cache rows +1 carry | 785 cache rows +1 carry |
| Independent fraction calls | 1 carry | 785 cache rows +1 carry |
| Normal Stream objects (27 Q+28 text+1 inverse) | 56 | 56 |
| Additional malformed-control Stream objects | 0 | 4 |
| Text zero-initialized objects (54 number +metadata +readText) | 82 | 111 |
| Potential named-return Text copies (metadata +readText) | 28 | 57 |
| Normal dense words | 16,929,027 writes | 16,929,027 reads |
| Actual quoted producer refill ceiling | 4,147,497 | 0 |

IndependentFraction performs at most 64 binary-search iterations plus its initial/final compareRational calls, each reaching 128-bit multiply/compare helpers. Original fractionBounds performs up to64 leading-bit steps plus52 mantissa iterations; zero and the full-width carry control are separate paths. Count actual nonzero rows (734+24+24=782), three zero rows and one nonzero carry; do not charge 786 identical maximum bodies and call it exact actual visits. A conservative upper may select those bounded paths explicitly.

The generator's actual normal unique-whole refill visits are 153,603 per fixture (begin/end plus the increasing dense whole grid after rewind), <=153,611 prepaid. Each fixture's owned cursor resets at most once after the end endpoint and advances at most6 times across the pinned <=4 changes. Cursor constructors/default members, each reset test, unsuccessful while conditions and the successful increments all count.

Per Q file write fallback = floor((words-1)/8192); read fallback = ceil(words/8192). All fast paths still execute dispatch predicates. The exact Q totals are:

| Rate / nine maps | Words per file | Write fallback per file | Read fallback per file |
| --- | ---: | ---: | ---: |
| 44100 | 441001 | 53 | 54 |
| 48000 | 480001 | 58 | 59 |
| 96000 | 960001 | 117 | 118 |
| Total | 16,929,027 | 2,052 | 2,079 |

Therefore Q fast writes16,926,975 and fast reads16,926,948. Native inverse has 2D words, D<=960000, so at maximum1,920,000 words, 234 write fallbacks /235 read refills, and1,919,766 /1,919,765 fast visits. These inverse counts are upper-domain substitutions; D is not asserted to equal960000. Every writer fallback at an already-full buffer enters put twice: first k=0 and real flush, then k=8. Every normal reader fallback refills once and copies8. Final partial/full flush, final EOF, and all constructors/destructors remain additional work.

The owned short control is different: first exact iteration refills7/copies7, next iteration refills0 and rejects with actual bytes/n/size =7/1/0. The trailing control refills9/copies8; finish observes at8 != size9 and rejects before fgetc. Both control destructors own and close their live FILE; writer controls use put7/put9 and finish, each with a fresh 64KiB zeroed buffer. There are no new mutant launches.

Each Stream buffer has65,536 zero-initialized scalar elements, plus file/at/size/bytes/default and maximum/write initialization. Each Text has65,536 zero-initialized char elements plus size. Each potential Text copy reads and writes all65,536 data elements and size; do not assume NRVO. The largest FractionCache has735 pair slots/1470 double elements zero-initialized, plus N/g/count; every actual row assignment/compare/bit witness is additional. productionRows initializes all four TempoChange entries even when count=3; the fourth is not free unused storage. Potential productionRows, buildProduction and vector return/move/copy/destructor work also remains in the required upper closure.

Metadata normal count27 /28 includes one verifier full-prefix mutant; inverse metadata count1 each. Normal metadata window loop is bounded396 admitted iterations per call, plus the terminating test and increment arithmetic. Eight burst objects and complete map rows3/4 remain. Text cap65536 is a byte/storage cap, not a free text source-action price. The verifier has28 normal readText calls. textWitness is called32 times:28 read comparisons, corruption control's argument and actual gate, prefix control's argument and actual gate. Each performs both full hash loops and the bounded first-difference loop, including on successful equal metadata. Number strings54 each construct a full zeroed Text before digit generation. Source-visible std::string construction/concatenation/retirement remains unclosed, not opaque by declaration.

## Catalogue and fixed/error ownership

`source-costs.h` is complete constructor/accessor source, but its private static data remains undefined. Logical fields are four whole uint64 values plus76 rows of `(Family:size_t, upper:uint64)` =156 eight-byte scalar fields =1,248 logical bytes under the declared AMD64 type widths, before any ABI padding. No actual sizeof/alignment/link placement is certified. A future binding must be `constinit const Catalogue Catalogue::reviewed_{literalWhole,literalRows}`; no runtime factory, local-static guard, lazy allocation, mutable rows, fabricated coefficient or default zero price. The constexpr constructor performs whole_/rows_ member copies semantically; its constant-evaluation/storage obligation is distinct from runtime lookup work. Its actual binding definition, identity, include edge and static layout still require source review and later mechanical qualification.

Catalogue::family validates index against76, row identity, positive price and2^34 ceiling and can throw logic_error. `whole`, immutableCatalogue, Cost::catalogue/price/validate/plus/times/units, wordUnits, inverseUnits, fixtureUnits and both childUpper quotations must all be counted once per actual invocation, including caller argument evaluation and checked sum/product gates. Repeated refill lookup/Meter work is bounded by actual quoted miss count and belongs in a counted hot/fixed instrumentation expansion, not a free startup row. Both childUpper quotations run in each child, and Cost::validate walks all76 families in each child.

Success and failure paths need a prefix ledger: no total maximum path can simultaneously add all mutually exclusive failures as if they execute together, but an upper must cover the largest successful prefix plus one applicable refusal and unwind/footer. Constructor failures after _sopen_s own either a descriptor or FILE. setvbuf refusal closes its FILE, records real fclose result/errno/site, then throws. Stream::finish nulls ownership before explicit close; a later throwing close gate must not cause a second fclose. Other failures invoke bounded destructor closes. rememberCloseFailure saturates, retains the first return/errno/site, and both main catches print original reason/operands or exception plus the cleanup footer. Expect catches Rejection, verifies exact reason and all six operands, checks no new cleanup failure and prints the real record. No-witness successful gates construct no six-field tuple; explicit-witness gates still construct/copy their tuple before checking success. std::exception/string/vector/library unwind helpers and closed footer maximum remain required unclosed source obligations.

## All 76 family obligations and next step

`census.json` lists all76 exact enum identities, ordinal, actual owners and unresolved expansion dependencies, with upperUnits=null. The family inventory is complete; its upper-price census is not. None may be copied to a binding as a zero, floor or old R2 price. ChildFixed must own the disjoint full setup/build/fraction/text/control/failure/retirement and quote instrumentation counts above; it must not duplicate bodies already charged by hot rows.

The supported result is **HOLD_NOFIT**, not a fabricated full upper. The current unchanged childUpper quote cannot be bound under this scalar-action model. The immediate reviewable next step is an independent critic of the expression floors and family ownership. If accepted, any further optimization is a new source hypothesis with the same endpoint/control/coverage/cap contracts and a new complete census. Lowering unit prices, making visible source primitive/opaque, dropping gates/endpoints, lifting caps, splitting/resetting the combined ledger, or promoting the current guards is unsupported. PS source work is a separate active census; its positive bound is unnecessary to establish this failure and has not been invented here.
