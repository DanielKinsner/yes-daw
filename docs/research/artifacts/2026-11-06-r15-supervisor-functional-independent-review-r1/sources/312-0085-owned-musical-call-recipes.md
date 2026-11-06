# 0085. Owned single-pass Musical call recipes with prepaid dispatch

- **Status:** Accepted
- **Date:** 2026-11-05
- **Deciders:** Dan's authorized Option A scope; root agent and independent engine/resource critic for this implementation decision (ADR-0049 / ADR-0078).
- **Related:** [Musical prepared lane](0083-musical-audio-prepared-lane.md), [channel equivalence](0084-musical-audio-channel-equivalence.md), [active implementation plan](../plans/2026-11-04-musical-audio-prepared-lane-plan.md), [CONTEXT](../../CONTEXT.md).

This decides the internal immutable planner representation and its funded
preparation protocol. Existing product vocabulary, saved format, Numeric A,
accepted helper prices, source/channel rules and all production caps stay intact.
It grants no renderer or waveform/resource admission. Exact helper/action bounds
and the whole independent proof precede producer source.

## Concrete problem and scope

MusicalAudioCutPlan::Builder runs the same call candidate walk twice: count then
fill. The 96 kHz constant-Q2 witness costs8816256 per walk, plus24723 fixed/
metadata/clock; full legacy cost17657235. A real one-walk recipe can avoid one
semantic walk, but must pay its actual additional storage, validation, matching,
access and lifetime work. It need not reproduce ResourceLimit outcomes caused by
a different resource algorithm. This is a new production helper policy, not a
research-only bypass and not renderer/DSP admission.

Actual ChannelPlan source has NO CutPlan field/dependency/construction. It owns
an immutable ChannelRequest with key/original/mapped/window owners and an
accepted complete finite/equivalence classification/projection. Retain that
existing plan directly. Do not silently build a legacy CutPlan or rescan N.
ChannelPlan's inherited work already includes the request and shared clock.

## Options considered

A. Keep legacy planner only. Simple, but retains both real walks.
B. Add an opted-in owned paged recipe with one semantic walk (recommended).
C. Reprice vendor loops/change DSP configurations. Separate HOLD; neither is a
   prerequisite or authorized side effect of this narrow decision.

## Decision

Use option B: an opted-in uniquely owned paged recipe performs one call-candidate
walk, retains exact classified request provenance, and pays all new storage,
validation and lifetime work. Preserve the existing two-pass planner and proofs.
The prepaid dispatch contract below governs all entry and refusal paths.

## New API and provenance

Add MusicalAudioCallRecipe.h only after a whole independent proof is authored.
Legacy MusicalAudioCutPlan.h, ChannelPlan.h and old proof sources stay byte-exact.
The new header may include their value types/nonbuilding arithmetic helpers;
calling a legacy builder/helper owes every reached old charge. Copying the
semantic algorithm into the new policy does not waive its existing prices.

Public contracts follow; the whole proof fixes exact C++ spelling before producer source:

- MusicalAudioRecipePolicy { representationRevision=1, plannerPolicyRevision=1 }.
  This distinct policy tags the new call representation; it is an additional
  identity component, never an overwrite of the retained request key's identity.
- MusicalAudioRecipeLimits has lower-only maxReservedBytes, maxBodyWork and maxCalls.
  maxBodyWork is additional BODY allowance and may0; it never resets the origin
  work cap. maxCalls defaults to2^20; directory uses ceil(min(N,D,2^20)/128).
- MusicalAudioRecipeStatus: Ok, InvalidInput, UnsupportedPolicy, GeometryRefused,
  LocalRatioUnsupported, ArithmeticUnproved, LogicalCallDomainUnsupported,
  ChordBoundUnproved, ResourceLimit, Cancelled, ProvenanceMismatch, NotDispatched.
  Preserve the exact nested MusicalAudioStatus for geometry refusals.
- MusicalAudioRecipeReceipt: fixed-size, allocation-free raw counters; inherited
  cumulative work separately from new consumed work, cleanup reservation and
  total debit; requested/current/peak pure bytes, retained old/shared graph
  accounting, allocation attempts/successes/releases and constructed counts.
- dispatchMusicalAudioCallRecipe(DispatchClaimPermit&&, optional<RecipeDispatchLease>&&,
  shared_ptr<const ChannelPlan> const&, CutCaps,
  RecipePolicy, RecipeLimits, CancelFlag const&, unique_ptr<const Recipe>& out)
  returns status+geometry+receipt BY VALUE after a funded invocation, including out==null.
  No lease means the private producer is NOT invoked and out remains unchanged.
- matchMusicalAudioCallRecipe(Recipe const&, shared_ptr<const ChannelPlan> const&,
  CutCaps, RecipePolicy, RecipeLimits) returns status+matched+work receipt. It performs
  metered exact request/key/window/clock/classification/cap/policy comparison.
- readMusicalAudioRecipeCall(Recipe const&, uint64 index, RecipeReadBudget&,
  LogicalCall& out) and readMusicalAudioRecipeBoundary(...) are metered checked
  O(1) copy-out accessors. No unmetered variable forEach callback/span/iterator.

The immutable recipe has a unique owner, retaining shared ChannelPlan, request,
key, clock and original/mapped owners through that plan. No new shared control
block is needed. Parent lifetime is real ownership, not pointer/hash equality.
Exact match compares the complete existing request key (whose operator== includes
all map-prefix rows, source owner identities, reverse/fades/tier/identity), explicit
window N/S/L/A/B/D and clock rate/rows, mapped window, logical/physical counts,
projection, channel policy, I/O caps and both new recipe revisions. Source owner
comparison includes control ownership and get(), never a recycled address alone.
A new request with a changed complete-prefix map/fade/reverse/source owner or
policy cannot reuse the old recipe even if endpoint duration/hash/address agrees.

Construction/request matching may walk the complete clock and must pay each
reached row/check. It is NOT ready-descriptor lookup. A later ready-only lookup
may compare a previously captured immutable validated descriptor in O(1), but
that descriptor and integration are outside this helper. This decision adds no
unmetered boolean match or O(1) claim for full-key comparison.

## Unique-out alias and overlap law

Copy/retain the incoming shared ChannelPlan and copy caps/policy/limits BEFORE
moving/clearing out: the caller may pass out->channelPlan() by reference. Move
previous out into a local whole unique owner; keep it through all preflight,
allocation/construction and final validation. After a valid funded dispatch, producer failure returns out null
with the by-value receipt; no previous history/preparation receipt is credited
against the new request. Old completed historical planning is not reexecuted or
charged again; actual old cleanup performed here is accounted separately by origin.

Charge simultaneous old/new recipe allocations and genuinely held parent graphs.
Deduplicate only actual shared owned subgraphs after same-control/same-get proof;
equal values/raw addresses do not release debit. Retain parents while destroying
old recipe payload so an alias remains valid. Release prior owner only after
construction/validation has finished or failed; receipt records actual peak,
releases and prepaid future cleanup, including allocation-failure/cancel paths.
An out parameter is producer staging, not a committed Project/playback owner.

## Exact storage and construction

Use raw operator-new storage with default-alignment static assertions, no vector,
make_shared, user callback, shrink/compaction or array-cookie allocation:

- one exact sizeof(Recipe) object request, held by unique_ptr;
- one exact MaxPages*sizeof(PageEntry) raw directory, explicitly construct every
  entry to {null,0}; PageEntry owns a pointer and constructed count;
- one128*sizeof(LogicalCall) raw allocation for each actually needed page;
- exact segment and fractional-boundary raw arrays from the existing metadata
  count/fill method, including every old row/ratio/source-projection charge.

M=min(N,D,1048576), MaxPages=ceil(M/128). N,D positive and each accepted ordinary
call consumes >=1 input and output Frame, so that is a valid checked upper bound.
Directory slots do not mean allocated pages. A page's unfilled tail contains no
constructed calls. Only accepted calls are placement-constructed; counts advance
after successful construction. Metadata constructed counts are separate. Final
page/slot address is index/128,index%128 after range/overflow checks. No call
compaction or second call-semantic pass occurs. Metadata's count/fill passes
remain and are paid; 'single pass' refers to the call candidate walk only.

Before EVERY request, prove exact size/product/alignment, free directory/record
capacity, remaining work including release reservation, and simultaneous payload.
Runtime receipt uses actual requested bytes, not size*capacity as a later rescue.
Allocation failure is caught as ResourceLimit, consumes attempted work and frees
only genuinely allocated/constructed payload; exact zero-size requests are skipped.
Type sizes are compile-time sizeof expressions, not guessed ABI constants.

Existing source/clock/request/control allocations are pre-existing retained
ownership, not newly allocated by this helper. Report their inherited declared
reservation separately: existing sizeof+64 accounting is a subtotal, not a newly
proved allocation-request/whole-Clip bound. New pure allocation requests and
old/new recipe overlap are fully bounded here; whole DSP/view/parent ABI/batch/
cache resource admission remains separate. No hidden legacy build supplies fake
shared-clock credit. Inherit ChannelPlan cumulative work ONCE, add new recipe
work; no second clock construction is present and no clock subtraction applies.

## Semantic and resource laws

Preserve old candidate order/preferred/halving/fallback and visited behavior,
Numeric A/rational fraction bounds/common floors, full-prefix clock, local ratios
including exact0.5/2 endpoint repair, fractional Tempo boundaries, chord<=1/4,
zero-output coalescing, positive I/O<=2048 and exact final N/D/boundary totals.
Successful calls/boundaries and common non-resource refusals must equal legacy
at sufficient budgets; named geometry status is part of that comparison.
New cancellation/policy/provenance statuses apply only to the new API. Different
ResourceLimit outcomes are expected from a different paid algorithm and are not
semantic disagreement. No sample-rate/window whitelist or relaxed threshold.

Single-walk semantic validation remains inside the walk; final structural checks
never substitute for Numeric A/ratio/chord checks. Construction-private immutable
storage binds each accepted record to its actual reached validation. A required
semantic requery still pays its full old charge. Final structural validation
checks pages/counts, contiguous positive spans, cap/remaining bounds, fractional
boundary ranges/ordering, finite chord in[0,1/4], exact endpoints and complete key/
policy/owner/classification binding. Every loop and equality is metered.

All old charges stay exact: fixed16384,row64,ratio/source projection4096,
cut8192+128*bit_width(rows),candidate512,lengthProbe64,callStep256,emission64.
Count failed/duplicate probes as actually reached; no stage reset or division.

For NEW pure operations the catalog is literal logical actions, one unit for each
reached check/comparison, scalar field/byte read or write, loop entry/condition/
increment, checked arithmetic/address step, allocation attempt/release or owned
handle retain/release. These are algorithmic actions, not CPU instructions. Old
prepaid semantic regions retain their old price; their internals remain raw
counts and are not repriced. New directory/page/match/access/structural/lifetime
operations are never hidden in an old price. Fixed helper internals and all raw
counts have source-sized finite expansions in the whole proof. Every actual
administrative ledger/check/status action is counted ONCE under an already
reserved source-bounded slice in the SAME origin ledger; it is never free or
recursively charged to itself. See the mandatory dispatch contract below.

Each action is checked/debited before its body. Cleanup is reserved before its
allocation becomes live; receipt distinguishes actual consumed work from future
cleanup reservation. Actual cleanup transfers reserve to consumed without lowering
total debit. Refusal does not skip necessary cleanup, and cannot convert an
unreached reserved cleanup into a claimed executed event. A previous valid recipe
already owns its cleanup reservation; even a zero-budget new refusal releases it
under that old-origin reservation, reports those actual events separately, and
never uses the old reservation/receipt as credit for new actions. CancelFlag is a borrowed
synchronous atomic boolean, retained by caller for the entire build, never stored
or invoked as a callback; check it at bounded phase/candidate/page/validation
boundaries and charge each actual load/check. All arithmetic/counter/version
limits refuse before overflow. Separate raw loop entry/body/final-false/break,
helper/access/match/ownership counters are unscaled and remain inspectable.

## Mechanical promotion and boundary

One independently authored whole new proof comes FIRST, then source against that
proof; preserve it whole after first GREEN. Cover same successful calls/boundaries
and non-resource refusals against legacy plus independent exact/interval oracles;
constant/jump/ramp/prefix/fractional/reverse/selected/endpoint-ratio/final-partial/
zero-output cases;127/128/129 calls and page-directory boundaries; exact/one-over work, bytes, calls, directory,
index and constructed bounds; each actual allocation failure; alias input-out,
old/new shared versus distinct overlap; metered full-clock match/mismatch; cancel
at every bounded stage and visible failed receipts. Mutants must actually reach
changed owner/policy/Q/chord/entry gates and fail unchanged relevant checks.

No DSP execution or broad vendor atom catalog is needed for this PURE proof.
Broad vendor body aggregation, configurations, physical state/context/quiet,
varying waveform quality, complete DSP/view/fade/peak/registry/lifetime resource,
renderer, jobs/builders/adoption/export/load activation and CI remain separate.
This decision supplies no full renderer, Ready, waveform, resource or Option A credit.

## Mandatory prepaid dispatch lease

A zero BODY allowance cannot fund entry, retain, out movement or refusal.
The independent whole proof must fix actual E and every administrative source
ceiling before production source/admission; no arbitrary numeric allowance or
cap increase is authorized. The semantic, storage and proof obligations above
remain mandatory. A decision is distinct from an executed or admitted result.

The job has ONE owned OriginWorkLedger with cap<=2^28, covering inherited
ChannelPlan work ONCE, administrative dispatch,entry/exit,planning,full matching,
access,cleanup and every outstanding reservation. RecipeLimits.maxBodyWork is an
ADDITIONAL lower-only BODY allowance and may be0. It is not a new origin ledger,
a fresh per-call cap or authority to spend E outside the Accepted cap. Existing
payload/call limits remain. The origin is established by the job before charged
preparation,with its bounded administration schedule reserved from the SAME cap;
there is no free origin bootstrap or unbounded retry/check API.

Concrete new pure protocol/types (fixed fields, no growable token registry):

- OriginWorkLedger is embedded in already-owned job/origin state. OriginHandle
  retains that state (an aliasing shared owner is permitted without allocating
  a new control block). Ledger state outlives every lease/recipe/reservation;
  leases and recipes retain OriginHandle. No borrowed dangling ledger pointer.
  This helper does not allocate a ledger/job,lease,clock or new shared control.
- Ledger fields hold cap,inherited debit,actual/reserved phase totals,origin ID,
  checked next/current serial and single active claim state. Tokens hold owned
  origin,serial,operation kind,amount/body ceiling and private live/consumed state;
  move clears the source. Unique recipes own move-only cleanup tokens; checked
  aggregate totals must equal their live reservations. Mutation is exclusive
  job-context work, not implicitly concurrent because ownership is shared.
- OriginSetupTicket and DispatchAdmissionPermit are noncopyable one-use values
  supplied by the origin's finite prepaid job-administration schedule. Their
  creation/validation/retain/return operations have actual finite source bounds
  counted once in that schedule. They are not fabricated from a stale receipt.
  A job that cannot fund setup/admission does not invoke these preparation APIs.
- tryReserveRecipeDispatch(OriginHandle const&, DispatchAdmissionPermit&&,
  BodyAllowance) executes under its ALREADY FUNDED administrative slice A. It
  checks origin identity,remaining cap/reservations and nonwrapping serial,then
  reserves fixed worst-case Entry/Exit E from that SAME origin. It returns a
  move-only optional RecipeDispatchLease, a one-use DispatchClaimPermit funded by
  the reserved claim part of A, plus an administrative receipt. Failure
  consumes actual funded admission work,issues no lease and touches no out.
- dispatchMusicalAudioCallRecipe(DispatchClaimPermit&&,optional<RecipeDispatchLease>&&,
  inputPlan,caps,policy,bodyLimits,cancel,out) is the only public producer-dispatch entry. Claim/serial/
  validity/owner checks consume the ALREADY FUNDED one-use claim permit. Test
  missing/replayed dispatch leases only with a fresh funded claim permit; a
  consumed dispatch E is never used to pay their rejection. The typed caller
  must have that live permit before invoking the dispatch wrapper. A missing,
  stale,wrong-origin,moved-from or already consumed lease never invokes the
  PRIVATE factory and leaves out unchanged. Record NotDispatched,not a producer
  refusal. Failed claim is already funded; it cannot reuse consumed E.
- A successfully claimed lease is single-use,binds one origin/serial,operation kind,E reservation
  and body ceiling,then invokes the private factory exactly once. Constructor/
  move/claim/receipt work is included in A or E exactly once. No lease operation
  allocates. Nonwrapping identity exhaustion refuses under funded admission.

E is a source-derived maximum for ALL fixed entry and exit branches: ownership-
retaining inputPlan BEFORE any out move (including out->channelPlan() aliases),
copying scalar arguments,initial constant-time policy/layout/geometry/origin
checks,old-out move,checking whether BODY work can start,one fixed cancellation
load if required,result/status/receipt construction,publication or refusal and
bounded ledger settlement. E contains no variable plan/key/clock scan,heap request
or old payload cleanup. It includes the administrative integer/predicate/store
expansion for these entry/exit operations. Every reached entry/exit action debits
its E subreservation before its body. Unused E remains an outstanding reservation
until origin settlement; it is not executed work,negative debit or new credit.

With a valid funded lease and maxBodyWork=0 (or too tiny for the first complete
body admission): retain input,copy scalars,move old out to previous,return the
named ResourceLimit/body refusal with out NULL entirely under E. Perform NO
planner,allocation,key/clock scan or accessor BODY work. Previous cleanup runs
SOLELY under that previous owner's valid old-origin cleanup reservation. No old
receipt or reservation reduces the new origin's debit. If old/new share an origin,
include each real consumption/reservation once; otherwise report both origins
separately without lending funds. Old cleanup/retained origin handles remain live
until actual release. Input alias remains valid through that release.

For nonzero BODY work, each next region's admission/check is itself funded BEFORE
checking that region. Use a finite BodyCheckTicket drawn from the same body/origin
allowance,not a free check. The private ledger primitive has a fixed,nonrecursive
integer-operation expansion,charged once: it consumes its already reserved check
slice,checks both body and origin remaining limits,then reserves the region and
its next-check/cleanup requirements before execution. If that fails,the current
check was funded,no region executes,and E funds the terminal result. Initial E
can refuse without creating a BodyCheckTicket when the body allowance is too
small. No helper recursively calls the charge API to charge its own arithmetic.
Matching/access use the same typed admission/check protocol and SAME origin;
they do not get an unmetered boolean/equality/check escape or a fresh cap.

The implementation/whole proof must enumerate each administrative helper's exact
finite instruction-INDEPENDENT source action schedule,entry/branch/exit bounds,
maximum scheduled admissions and source ceilings for setup/admission/claim,E,
body-check/settlement/cleanup. Count these source actions once. A failed check's
slice is already reserved,including exact/one-less cases. No caller can retry
an exhausted admission permit indefinitely; a new attempt needs another funded
scheduled permit. Actual A/E/body ceilings are source-derived proof outputs,
not arbitrarily chosen constants or library/CPU timing claims.

By-value receipts separate inherited debit,admission A reserved/actual,E reserved/
actual/unused,body allowance/consumed including its check actions,new cleanup
consumed/future reserve,old cleanup origin/consumed/reserve,all remaining origin
reservations and FULL origin total. Historical E_reserved is not added again on
top of its consumed+unused parts. The total is inherited+all actual charged work+
all LIVE outstanding reservations (with same-origin union once). No unused
reservation is mislabeled executed or credited against another origin/body.

The whole proof FIRST must fix E and each administrative/helper ceiling,then
exercise zero/tiny body allowances with and without existing out,aliased input,
no lease/wrong/replayed lease NOT invoking producer and preserving out,funded
entry clearing out,exact/one-less dispatch reserve and body gate,prepaid failed
checks,old cleanup spending only its actual old-origin reservation,no stale
receipt credit and the full<=2^28 cumulative ledger including all reservations.
Every semantic,page,allocation-failure,matching/access,overlap,mutation and
portability obligation remains. Broad vendor/state/quality/resource HOLD is not
a gate on this narrow pure proof. The delivery order is this architecture
decision, the whole independent proof, then producer source and required checks.
Actual source and results still need their separate reviews before admission.

## Consequences

- The legacy planner remains available and unchanged; the new helper pays its
  actual extra ownership, paging, matching, access and cleanup costs.
- Maintaining two planner policies requires the independent equivalence and
  mutation proof. A proposed saving is never a complete preparation fit.
- No CONTEXT term changes: existing Clip, Asset, Musical window and channel
  meanings remain. The recipe and budget tokens are internal implementation types.
- Broader state/context/quality, full resource closure, builders, workers,
  adoption, export, load and activation retain their existing gates.
