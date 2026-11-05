# 0088. Form Musical request-owned origins before planning

- **Status:** Accepted
- **Date:** 2026-11-05
- **Deciders:** Root and two engine/resource critics within Dan's authorized Option A scope (ADR-0049 / ADR-0078).
- **Related:** ADR-0083, ADR-0084, ADR-0085 and the active Musical-audio prepared-lane plan.

## Problem

The current application creates unique StretchPreparationJob and AudioImportJob owners. They own no Musical ChannelPlan, work ledger or reserved Fund. ChannelPlan exposes historical cumulative accounting only. Copying that number into a fixture does not establish live authority, and a near-cap completed Plan may leave no room for its first origin/refusal action.

The private bootstrap prototypes make later accounting and cleanup explicit, but they cannot create their missing predecessor by declaration. This decision supplies the concrete application ownership and first-authority mechanism before production source depends on it. No existing Accepted ADR or source is changed by this proposal.

## Options

A. Application-owned captured Musical request state, with an inline origin and explicit bounded intrusive ownership (recommended). One exact owner allocation has a source-visible lifetime; the unique worker and later recipe/read owners retain it. No separate shared control allocation or worker/result cycle is needed.

B. Application-owned shared_ptr request state. This has the same useful ownership graph, but a complete portable allocation/control-lifetime source contract is required before admission; sizeof+64 is not that contract.

C. Shared ownership of the complete thread job. This changes current reset-before-adoption behavior and requires explicit job/result/recipe cycle draining on every terminal path.

## Decision proposed

Use A. Add app::MusicalPreparationRequestState for the actual captured per-Clip immutable request, map and source owners, with its origin ledger embedded in that same allocation. The application forms it before Musical clock/request/channel planning. Existing Fixed stretch/import job ownership stays unique. The request state owns neither the worker nor its result.

OriginHandle retains this state through a private explicit intrusive ownership operation; no public weak/lock API or growable registry is introduced. Every retain, move, release and final destruction has its own already-funded source path. Moves clear their source. The finite admission/owner schedule proves reference-count nonoverflow; it does not rely on an unbounded CAS loop. Ledger mutation, including accounting for ownership changes, remains exclusive job-context work. Shared ownership does not authorize concurrent ledger access.

The actual application lifecycle must enforce that exclusivity: Control retains active/retiring workers until terminal publication, worker-side cleanup/accounting finishes before its release store, and Control consumes results only after acquire. Cancellation changes the borrowed cancellation flag without dropping the origin during running work. A later background read/rebuild requires an explicit serialized origin-context handoff. Origin handles or mutable ledgers are not exposed to the Audio thread.

## First nonrecursive formation primitive

There is no alleged existing Fund at the root. One private application formation primitive runs once for a genuine new captured Musical request. Its complete fixed formation/refusal/ownership/destruction envelope is logically reserved from that request's existing production work cap before physical initialization begins. Its literal source then materializes that ONE reservation and canonical phase state. It does not execute work and reimburse it afterward.

The primitive uses the unchanged production ceiling of2^28. It has no public arbitrary initial-authority scalar, caller price table, lower-origin-cap retry API or fresh per-stage budget. Lower-only BODY limits remain separate. Its complete finite first-source expansion, including first local/counter/ledger writes, argument/identity checks, allocation attempt, failure result, construction and final ownership cleanup, must be independently bounded and proved to fit before execution is enabled. Failure of that static source proof makes the implementation inadmissible; it does not raise a cap or certify a partial path.

All formation operations are part of this one pre-reserved nonrecursive source region. Ordinary funded transfers start only after real canonical storage and source-backed tickets exist. A first local scratch/census cell is not a second ledger, spending authority or caller-supplied reimbursement. No formation result or diagnostic manufactures another origin or reusable Fund.

Reserve actual future destruction before each owning allocation/capture becomes live. Application source allocates exactly the concrete request-state storage with checked size/alignment and real allocation-failure handling. Its inline ownership count and ledger avoid an implicit separately allocated control block. Variable map/request copying begins only after the fixed state exists and is admitted through funded regions; it is not hidden inside formation's fixed bound.

On success, initialization hands off canonical accounting exactly once to the inline ledger, preserving all actual/reserved totals. On failure, no recipe setup/admission authority is issued, acquired resources use their existing destruction slices, and unused reservations remain distinct from executed actions. There is no synthetic successful Plan or manual PASS receipt.

## Planning and inherited work

The same origin covers formation, entered legacy semantic regions, all new administration/planning/match/access/cleanup and outstanding reservations. Preserve every existing legacy price and old builder/proof byte. ChannelPlan cumulative construction work is represented ONCE: a successful final Plan is checked against the already-accounted legacy contribution, never added on top of it or used as new credit. Do not build a second clock or rescan source samples to recover missing observations.

A separately reviewed funded legacy-stage adapter must establish real success AND failure entry-point correspondence before integration. Existing failure enums discard local accounting and do not reveal all sample visits. A paid immutable preflight can prove entry into an existing prepaid scan-price region; that does not prove N samples were visited. Unknown failed-prefix observations remain explicitly unknown. Do not infer them from successful accounting, fabricate visited rows, relabel an unexecuted maximum as actual, or waive the accepted raw-counter obligations.

If unchanged public entry points cannot expose a required observation, the concrete opt-in observation mechanism and its source cost need their own reviewed implementation decision before use. This proposal grants no permission to edit old headers/proofs or to add an unpriced callback/rescan. Full legacy-stage and final public move-only recipe API closure remain required before the whole R4 proof or producer is admitted.

## Order and mechanical proof

1. Accept this ownership/formation mechanism only after fresh whole engine/resource critique; keep all numeric prices unbound.
2. Write the complete private application formation/request-owner source, including real allocation failure, all first writes, move/clear handoff, abandoned construction and final destruction. Review that whole source before independent census.
3. Independently derive every source partition and complete formation/ownership/refusal reservation; prove its acyclic funding equations under unchanged caps, with a genuine real caller rather than scalar-filled fixture state.
4. Close the funded legacy-stage success/failure adapter, then author the complete independent R4 proof against the actual combined source/API. Preserve the whole proof after first GREEN.
5. Build/test and integrate worker/model stages only through the active plan's applicable gates. This decision alone supplies no execution, production resource, waveform, Ready or activation credit.

Proof must cover real first initialization and ownership, actual allocation failure, invalid request identity, never-started setup, failed planning/cancel, final-worker publication order, recipe/read lifetime beyond the unique worker, alias/old-origin cleanup, single canonical accounting and exact/one-less funded internal regions. No valid admitted path may reach an unfunded partial kernel or private-protocol misuse. Formation cannot be tested by pretending an insufficient private initial authority is a funded public refusal.

## Compatibility and limits

This implements ADR-0085's job-established origin before preparation, embedded job/origin-state ownership, unchanged one-origin cap and nonrecursive administrative rule. It is not a free-bootstrap exception. The root logical reservation and its materialization require complete actual source/proof before execution; naming the primitive is not proof that it is funded.

No saved format, product timing policy, source/channel classification, Numeric A law, DSP configuration, quality threshold, resource ceiling, old semantic charge, Audio-thread rule or accepted public recipe requirement changes. CONTEXT and the active plan stay unchanged during the current pinned research stages. Whole renderer/resources/legacy-stage/worker/model/export/load/UI gates remain separate.
