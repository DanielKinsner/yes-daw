# R6 formation / R2 adapter ownership correspondence

Status: PARTIAL_SOURCE_WINDOW_QUERY_HOLD_UNBOUND_UNCOMPILED_NOT_ADMITTED.
This is actual source correspondence, not a literal numeric census or native
qualification. PendingWindowQuery is undefined. No complete adapter is claimed.

## Lineage and explicit change boundary

formation-r6.h is the full separately named/namespaced successor of R1's
formation-r5.h, which retained the original bound R4 formation implementation.
The original129 Site ordinals and R1's full207 Site prefix are preserved. New
R6 Sites follow that prefix. LegacyLocalOwnerReturn is retained for ordinal
identity ONLY: its flawed R1 post-destruction trace has no R6 callsite. No value
or execution credit is assigned to that retired declaration here.

The two R1 blockers were: payment after local shared_ptr destruction, and State
owning computed legacy results contrary to ADR0088. Both mechanisms are removed.
R1's whole review found no other blocking three-attempt issue; that is prior
review evidence only. The new whole source still requires independent review.

Role::Result is appended after the old four role ordinals. Consequently roleCount,
array initialization/split/retirement loops, all affected prices and all layout/
root equations are NEW and UNBOUND. Raw gains six actual new-result counters.
State loses all LegacyArtifacts storage/getters/live flags and gains scalar
Result-issued/live flags. Control gains a Boolean and raw inline result storage.
LegacyReadout/Raw/receipt/Control/Application sizes change. No old census value,
old native size, old fixed envelope or old proof certifies any such change.

All predecessors are preserved; this packet alters no actual engine header,
Accepted ADR, golden, RT annotation, public recipe proof or earlier packet.

## One genuine origin and complete prior funding

The existing private genuine captured-request Formation primitive remains the
only origin creator. The complete expanded R6 logical root must fit 2^28 and be
held before its FIRST physical caller/argument/local/counter/initialization or
allocation action. Its source materializes that single reservation and hands
canonical accounting to State once. The scratch census is not another ledger.

Bounds remains declaration-only. rootEnvelope must include the newly expanded
first-source initialization, caller/Control/result storage, roleCount effects,
all legacyFixed/check/cleanup schedules and all role lifetimes, including Result.
Legacy schedules are split from that SAME root. No old Worker874/Control3082
envelope, Clock tariff, successful Plan total, unrelated semantic fee or public
caller Fund finances a new action. There is no reimbursement or budget lift.

All construction, move, caller, flag, scope, ledger, query, destruction and
refusal work must be counted under the pinned production logical convention.
The current source names its disjoint partitions but defines no numeric price.
Each partition expands its own wrappers/callers/conditions/counters/ledger calls;
SDK H/Z/O boundaries are applied once, not mistaken for CPU/native measurements.

legacyFixed holds the finite adapter, result construction/publication/handoff
and new Control hooks. legacyCleanup separately holds actual output/staging/
cell destruction and final unused-slice retirement. legacyCheck holds reserve
administration. Role::Result's lifetime is independently held from formation,
even on paths where never used. Its unused tail remains reserved/terminal; it
does not become credit. Query and three attempts use their typed slices only.

Reserve before owning capture/allocation becomes live. NativeStorage still
performs only actual State/map allocation attempts and actual failure handling.
Result/staging wrappers introduce no owner/control allocation. New sizeof/align
requirements are compiler assertions only, not measured layout evidence.
Raw.currentBytes still describes the actual State/map allocation payload only.
It excludes Control/caller/result/staging storage and opaque legacy allocations;
new fixed storage and simultaneous retained peaks require the separate complete
resource proof. A work reservation does not certify that byte closure.

## Actual ownership graph and finite schedule

State owns captured Metadata/Parents/map/options and accounting, NEVER worker or
result. A ControlRequestOwner occupies one of ApplicationRequests' two real raw
slots. Its resultBytes_ is caller-owned storage OUTSIDE State. It holds at most
one actual WorkerLegacyResult, with manual construction/destruction under the
same origin. resultLive_ is initialized in Control construction, including the
failed logical-root path; no result owner is implicitly initialized by raw bytes.

WorkerArtifacts exists only as an actual local AFTER ContextScope's genuine
Worker acquire and BEFORE that scope's terminal publication. It contains funded
Clock/Window/Request wrappers. Plan output is a funded local and moves directly
into the separate result. Actual Plan retains Request; Request retains Window/
Clock/source owners. There is no State/worker/result backreference in that graph.

Result owns one direct PlanOwner in raw storage, immutable historical readout
after completion, and one origin pointer when ownsOrigin_ is true. That pointer
retains State; State holds only lifecycle Boolean facts, no result pointer.
The embedded empty cell initially BORROWS State protected by Control. Every
actual adapter completion (including failed/cancelled outcomes) retains Result
before publishing completion. This makes even an empty failed result safe to
outlive Control; it never fabricates a Plan or success.

Finite reference counts:

| Actual point | Live origin references |
|---|---|
| Formation/never armed | Control = 1 |
| Armed/running before completion | Control + Worker = 2 |
| Result completed before Worker return | Control + Worker + Result = 3 |
| Worker dropped; awaiting/after terminal acquire | Control + Result = 2 |
| External handoff | Same two; ownership transfers, no retain/drop |
| Control dies first | Result = 1, context DetachedControl |
| Result dies first | Control = 1, context Control |
| Last real owner drops | 0, physical cleanup/free once |

The generic old Retained/Reader path cannot coexist with a live Result:
LegacyRetainedResultCheck rejects it while resultOriginLive_. Result publish
also proves !retainedIssued_ and exactly two current references. No new worker
is issued after workerIssued_. If Result dies while Control survives, the old
finite Retained path may run later; it creates no second Result or new origin.
These are internal finite ownership constraints, not rate/map/input whitelists.

## Physical destructor ordering: the first blocker

FundedOwner<T,Construct,Destroy> stores only a State pointer and aligned raw T
bytes. Its constructor pays Construct before placement-constructing the real
empty owner, then increments ownerInitializations. Constructor/caller/pointer/
argument actions are part of that independently expanded construction partition.

At each ACTUAL destructor entry it reads its sponsor, pays Destroy from the
already-held cleanup slice, explicitly executes value().~T(), then increments
ownerDestructions AFTER the physical call returns. There is no automatic T
member and therefore no second destructor after the wrapper body. Pointer/byte
members are trivial. No future physical drop is charged by Prepare.

Separate instantiations/Sites cover each Clock/Request/Plan output and each
Clock/Window/Request staging owner. Every branch with a constructed wrapper—
preflight refusal, cancellation, returned old failure, success/move, catch—uses
the same real C++ scope exit. Empty and moved-from owners still execute their
real paid destructor and their one completion increment. Exceptions after old
entry reach catch; wrappers remain live until after the named exception outcome.

WorkerArtifacts' destructor prepays ONLY its own group body/member invocation/
caller/return tail with LegacyArtifactsDestroy. Automatic members then execute
Request wrapper, Window wrapper, Clock wrapper, each paying its own actual drop.
The group has no aggregate counted fake owner-destruction event. No State can
die during these drops because Control and Worker references remain live.

WorkerLegacyResult similarly uses raw PlanOwner bytes. Its ACTUAL destructor
pays LegacyCellDestroy BEFORE plan().~PlanOwner(), then increments the real
owner/cell completion counters. The owned/borrowed branch follows that physical
drop. An owned result clears its ownership flags and counts its origin drop
under LegacyCellDestroyOwned before Ledger::drop(Result); no State access follows
the possible final free. A borrowed embedded cell uses LegacyCellDestroyBorrowed
and remains protected by its enclosing Control. No detached borrowed shell exists.

## Result cells, handoff and real acquire: the second blocker

Control::armWorker pays LegacyArmResult, checks its unique unused result slot,
placement-constructs one empty Cell under LegacyCellConstruct, then increments
the Worker reference and publishes Armed. A Cell constructor has raw scalar/
readout/Plan storage with no hidden owner default construction before its funded
body. The body initializes fields/readout and placement-constructs PlanOwner.

abortWorker drops the never-started Worker reference under its old finite role,
restores Control, pays LegacyAbortResult, clears resultLive_, destroys the real
empty Cell, then publishes Empty. No Result origin was retained. Never-armed,
failed capture and failed-root paths construct no Cell and drop no unknown owner.

Adapter finish runs AFTER the complete staging scope has physically ended.
LegacyFinish covers its new helper/caller/history return. Result::publish pays
LegacyCellRetainOrigin and proves same State, WorkerRunning, internal untaken
incomplete Cell, no prior Result/Retained, and exactly Control+Worker references.
It retains the already-funded Result role once. LegacyCellPublish copies the
real result/history and marks completion. This is not job/model Ready activation.

ContextScope's actual destructor THEN performs its funded context return/drop
and release-store Terminal, with no origin work after the release. It drops
Worker while at least Control remains. Result is already a distinct owner;
neither ContextScope nor State owns it. Returned LegacyReadout is historical,
not live authority or a guarantee that later result cleanup is already observed.

Two alternative actual Control callers are present:

- Discard: acquireCurrentWorkerAfterTerminal performs the genuine acquire;
  subsequent replacement/destruction destroys the embedded Cell before dropping
  Control. Retiring drain and Application destructor similarly acquire the real
  terminal retiring worker before whole-owner destruction. Running teardown is
  private protocol misuse, not an admitted destructor context.
- Handoff: takeCurrentResultAfterTerminal itself invokes current.acquireWorker
  BEFORE checking/mutating State. Its new receive-safe caller prefix and all
  return work are held separately in LegacyTakeResult; common AcquireWorker is
  charged once under Control. This path is an alternative to acquireCurrent,
  never a second acquire. The private Take constructor proves real Control
  context/completed internal source/actual origin ownership and one untaken Cell.

Take copies history and move-constructs a real PlanOwner directly in the returned
object's caller-owned raw storage under LegacyCellMove. It transfers the origin
pointer/ownership flag, clearing source ownership and marking it taken; no new
reference, allocation, Fund or origin is created. C++ mandatory prvalue elision
means no additional arbitrary moves. Public copy/move/assignment are deleted;
constructors and Plan access are private. There is no caller-supplied Plan path.

The moved-from Cell still borrows State and is really destroyed under Control.
Control::destructor checks actual acquired ownership/context, pays its result
probe, clears the live flag and destroys any Cell before its own origin drop.
If an external Result remains, the separate LegacyControlResultDetach hook sets
DetachedControl. Result destructor subsequently runs in that serialized context.
If external Result dies first, Control protects State. Both final-order choices
therefore have real caller storage and complete cleanup source. General job
launch/join/publication, recipe ownership and model integration are still absent.

## Actual three-attempt chain

workerBuildLegacy creates ContextScope FIRST. That performs actual acquire of
Armed before any ledger debit. LegacyWorkerCaller covers the separate R6 caller,
checks a real result Cell exists, and invokes run. run proves captured request,
live fixed inputs, WorkerRunning and unused actual internal result; one issuance
flag prevents replay. It constructs real WorkerArtifacts under its funded group
entry, then calls chain. All chain returns first destroy actual local/staging
owners before finish. No stage retries and no new authority exists.

chain order is cancel -> common immutable preflight -> query reservation ->
Clock -> Window query -> Request -> Plan. Every refusal stops. Success stores
the named outer Ok only after the actual returned successful Plan. Scalar
Common/PreparedAttempt/readout/caller/return actions remain fully source-priced.

common validates actual captured rate/map shape, ready owner, C=1/2, selected
extent, vector shape/product domain, actual State+map headroom, old Clock declared
bytes/work domain and actual default allocator max_size domains. Map content was
actually copied/validated by formation. Private map/options expose no mutable
alias. Parents' shared_ptr<const> alone does NOT prove absence of external mutable
sample aliases or ownership of its get(). Actual immutable/serialized source
extent/lifetime/no-alias ingress remains a mandatory entry proof, currently HOLD;
this packet does not certify it. No N-sample scan or second Key/Clock is introduced.
Actual allocator/control/retained-byte closure remains HOLD;
the old sizeof+64 terms remain declared subtotals, never native receipts.

total has a separately metered fixed six-phase canonical sum loop. prepare
uses genuine remaining headroom after ALL other actual/reserved work and the
unchanged cumulative lower-only minima. Before current fee reservation its
ceiling is priorLegacy + headroom; equivalently after reservation it is
priorLegacy + heldCurrentFee + remainingHeadroom. Prior contribution appears once.
Clock additionally proves stored 512+2*bit_width(R) lookup eligibility without
debiting that threshold. Query upper is held once, never consumed as actual here.

Exact semantic increments remain Clock 1+6R, Request 4096, Plan 1024+N*(8+16C).
Checked scalar domains and all physical byte/extent/effective-limit/work guards
precede each commitment. Arguments and real output references are prepared first.
Fresh empty funded output wrappers exist before attempt reservation/cancellation.

LegacyClockCommitInvoke/RequestCommitInvoke/PlanCommitInvoke cover each immediate
commit/invocation partition. commitPrepared executes branchless scalar canonical
transfers and counters, then exactly ONE real frozen builder is the next call.
No cancel test, throwing argument, allocation, resource guard, callback, failure
return or other bookkeeping intervenes. Fee is consumed once only for actual
entry. Unexpected old exceptions consume that fee and get their named outcome
plus funded real cleanup; they refund nothing. Private invariant violations are
not underfunded public kernel/refusal paths.

Clock/Request successes move into empty worker staging. Successful Plan moves
into the actual separate Cell; LegacyResultPlanMove prepays that new real move.
Each success reads its actual cumulative accounting and compares it ONCE with
already-consumed legacy contribution. It never adds that total again. Clock
also checks actual declared bytes and row count against proved preflight.

Old returned enums/nested geometry status are copied exactly only after real
returns. Exceptions leave that old status unavailable. Clock InvalidClock/
ResourceLimit do not invent workUnits_151 or failed-prefix visits. NumericDomain
retains its source-proved cumulative observation. Request's actual immutable
preflight establishes entry before local4096 prepayment/key construction. Plan
preflight proves all earlier scalar/extent/work/byte guards pass: returned final
ResourceLimit therefore establishes full N/C*N visits; NonFiniteSource leaves
the hidden failing prefix explicitly unavailable. Success uses actual returned
owner counters. Unknown never becomes zero, N or a fabricated allocation site.

LegacyRaw distinguishes actual invocation/attempt tariffs, returned statuses,
exceptions, direct owner actions, cumulative getter/comparison actions, new
result lifecycle actions and source-proved outcome facts. Wrapper completion
counters occur after actual direct destruction. They are not old private raw
loop/allocation counters or native measurements. resultCopies denotes the named
history copies/guaranteed trivial return, not live object-count authority; its
snapshot is necessarily before later terminal/result/destruction events.

## New Site/partition coverage

Original129 and all78 R1 additions remain declaration ordinals. Every reached
affected R1 event has a NEW unbound source vector. R6 adds these event families:

| Family | Actual source coverage |
|---|---|
| Clock/Request/Plan Output Construct/Destroy | Empty real output placement initialization; actual paid-before-drop scope exit and completed drop counter. |
| Clock/Window/Request Stage Construct/Destroy | Actual worker-local intermediate ownership and reverse-order destruction. |
| CellConstruct | Actual Control-owned empty result cell/readout/PlanOwner construction. |
| CellDestroy / Owned / Borrowed | Immediate physical PlanOwner destruction, actual completion, then correct origin release or borrowed-cell retirement. |
| CellMove | One caller-owned prvalue result construction, history/Plan move and origin ownership transfer/clear. |
| ResultPlanMove | Actual successful Plan output transfer into the separate result. |
| CellRetainOrigin / CellPublish | Real same-origin Result retain after staging cleanup; exact result/history completion. |
| ControlResultProbe / Destroy / Detach | Real inline result lifetime and detached-origin context before Control drops. |
| ArmResult / AbortResult | Construct before Armed; empty never-started cleanup after Control restoration. |
| TakeResult | Actual receive-safe acquire caller, inline result lookup, one private prvalue handoff and return. |
| RetainedResultCheck | Mutual exclusion with live Result before old Retained issuance. |

Group LegacyArtifactsConstruct/Destroy now cover real worker-local group source,
not State artifacts. LegacyFinalProbe verifies no remaining Result origin before
last cleanup. LegacySlicesRetire classifies unused typed tails without lowering
canonical reserved. Role::Result drop uses the complete original literal drop
implementation with its own NEW lifetime envelope, including last/non-last and
all roleCount-dependent cleanup. Every helper/query/implicit owner action and
caller/member/return tail still needs a complete disjoint independent census;
this table assigns no value or source-fit certificate.

## Pending query: smaller B design ONLY

PendingWindowQuery::resolve has NO BODY. It receives the actual same-origin
staging Clock and fresh empty staging Window through a private adapter-only
entry; no public arbitrary Clock/result factory is introduced. Its future body
must invoke frozen resolveMusicalAudioWindow once. The held upper is not actual,
and eligibility512+2*bit_width(R) and the three old tariffs are unavailable to
finance it. No fourth tariff, private-field fabrication, observation callback,
second Clock object/array or source-sample N rescan is authorized here.

Domain/callgraph: resolve first retains Clock/resets output/checks start,length,
N and integer overflow; early refusals query no endpoint. A is {start,0,1}; B is
{start+length,0,1} only if A succeeds. fractionBounds takes numerator-zero return;
high==low skips the second compiled lookup. frameForMusicalTime frac0 calls
frameForTick once then checkedNativeFrame. No fractional/inverse/sourceQuotient
loop is entered. Nonempty frameForTick uses selected std::upper_bound, cached
private prefix and at most one append helper. Empty map uses its old120 formula.
Floors/collapse/status/ownership paths still need their actual complete census.

Recommended future classifier: one separately metered scalar-prefix walk to A's
selected index, then (if A succeeds) onward to B's index. Preserve the accumulated
prefix separately from each partial endpoint result. No array/allocation is
needed. CALL the ACTUAL old detail::appendTempoSegmentFrames for extra integration
work, rather than copying its floating integration/log1p expression. Every EXTRA
old helper call is outside the semantic regions and must have its exact literal
branch-specific vector derived/debited BEFORE entry, including all wrappers.

For a full prefix segment, immutable sorted rows prove segmentTicks=deltaTicks>0
and valid BPM; curve and equality select the constant/ramp arm. Under proved
identical numeric conditions, original Clock success proves genuine-ramp slope
and relative guards passed. Final return isfinite(frame) has the same action
vector for true/false. For partial segments, separately metered original-order
slope/relative predicates establish the precise short-circuit path before helper
entry. Actual extra helper bool/double outputs then establish the downstream
frameForTick/native-frame path. They are actual PUBLIC HELPER OUTPUTS; any old
query path/count inferred from them remains SOURCE-DERIVED, never measured raw.

Before the ONE actual resolve call, prove its entire reached path including
early refusal, first/second failure, checkedNativeFrame, floor/collapse/status
and ownership actions; debit its exact vector from the held query slice. Charge
all replay/predicate/search/profile/check/reconciliation work separately. A
matching returned status/Window cannot substitute for forward path equivalence.

Unclosed forward-proof obligations include:

- Exact immutable map/rate identity, lifetime/no mutable alias, actual earlier
  Clock success and induction from the cache's initial positive-zero through
  every exact stored prefix. Finite cached values need not be below2^63.
- Same compiled/inlined numeric behavior at original build, extra helper and
  real query sites: contraction/FMA, operation order, intermediate double stores,
  target precision and selected compiler/options. Same inline text alone is not
  a proof that optimized callsites have identical numeric transformations.
- Relevant FP rounding/precision/FTZ/DAZ/trap/flag state through the ORIGINAL
  cache computation, not just before/after snapshots. Allocator/new-handler or
  opaque library callbacks before that computation must be qualified; selected
  source absence of setters is not binary/CRT/interval evidence or a null handler.
- Repeatable supported libm behavior and relevant errno/FP-flag/cache effects;
  counted SDK logical leaves confer no hidden native allocation/resource credit.
- Selected SDK binary-search identity/comparator/iterator/short-circuit path.
  Integer rows can derive topology only against the admitted algorithm identity.
- Full supported input domain: empty/single/multiple rows, jumps/equal/ramp,
  exact boundaries/tail, tiny/huge values, underflow/overflow/negative zero and
  first/second endpoint failures. No convenient input/rate/map whitelist.
- All classifier/helper/query/profile/canonical/refusal/cleanup work fits a
  complete prior same-origin reservation. Exact pre-entry branch derivation is
  essential; do not discover and reimburse its cost after executing a branch.
- Actual resolve status and available output bytes agree afterward. This is an
  additional reconciliation check; it cannot retrospectively prove hidden paths.

No arbitrary verified Boolean/public native-profile token may waive these.
An internal qualified profile must arise from concrete admitted source/binary/
platform evidence and actual funded checks. No such profile is supplied here.
If any forward proof fails, HOLD remains. No query code or price is added in R2.

## Mechanical gates still required

Fresh WHOLE partial engine/resource review; actual query successor and independent
forward proof; complete independent literal census/envelopes; native layout/FP/
allocator/resource qualification; reviewed binding; actual independent whole
combined source/API proof, with all supported success/failure/allocation/cancel/
lifetime/one-less cases; later public recipe/worker/model/renderer integration.
Old proof is frozen including helpers/comments; unrun recipe proof is unchanged.
Source reads/edits/byte pins are not execution, measured source fit, first GREEN,
actual allocation receipts, waveform/Ready/activation or production evidence.
