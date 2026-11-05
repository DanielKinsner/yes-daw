# 0089. Opt-in settlement for frozen Musical builder attempts

- **Status:** Accepted
- **Date:** 2026-11-05
- **Deciders:** Root and fresh engine/resource critic within Dan's authorized Option A scope (ADR-0049 / ADR-0078).
- **Related:** ADR-0083, ADR-0084, ADR-0085 and ADR-0088.

## Problem and evidence

The real request-owned origin starts before Musical planning. Existing builders expose successful cumulative accounting and exact result statuses, but some failed paths discard their private accounting. In MusicalAudioClock::build, the1+6R work guard precedes row validation and allocation; workUnits_ is stored only after object/control construction. InvalidClock or ResourceLimit cannot prove that store executed. The guard is eligibility, not consumption.

Request and Plan have stronger local prepayment points, but no old public API provides a universal failed-prefix or allocation trace. Rebuilding, rescanning, guessing an allocation site or filling a diagnostic counter would not create that evidence. Frozen headers/proofs/prices stay byte-exact.

## Options

A. Add an explicit opt-in frozen-builder attempt settlement policy (recommended). Charge one genuinely invoked attempt at a fixed semantic tariff using the unchanged numeric declarations, separately from observed construction or visits.

B. Require observing the old private stores. Existing Clock failure statuses cannot close this through unchanged public APIs; combined job settlement remains held.

C. Add allocator/legacy observation infrastructure. This is broader than the required internal settlement rule and is not authorized by this decision.

## Decision proposed

Use A only in the new same-origin Musical job adapter. It is a NEW conservative semantic entry policy for failed attempts, not a claim that an old eligibility guard consumed work or that an old accounting store executed. Existing old APIs and their successful accounting remain unchanged. Failed attempts can consume this new policy tariff earlier than the old successful-owner store; no output, numerical, saved-data or supported-domain rule is changed.

With R immutable input map rows, N selected mapped Frames and C logical channels, use exactly these stage increments:

- Clock attempt:1+6R.
- Request attempt:4096 additional units.
- Plan attempt:1024+N*(8+16C) additional units.

These are fixed semantic tariffs for one actually entered frozen-builder invocation. They are NOT literal new source-action counts, observed old stores, executed-row/sample counts, observed internal-body maxima or allocation receipts. Preserve every new wrapper/preflight/query/ownership/cleanup action as separately source-priced work. No tariff may hide those actions.

## Atomic commitment and invocation

The adapter owns immutable arguments and fresh empty output locals. Its independently funded preflight proves all required scalar/extent/byte/effective-limit/work guards and prepares every argument, checked tariff, counter slot and source-backed reservation before commitment. A preflight refusal invokes no builder and consumes no attempt tariff.

Consume the complete stage tariff exactly once immediately before ONE guaranteed actual builder invocation. The same already-funded source partition covers bookkeeping and invocation. Between commitment and entry there can be no cancellation branch, throwing argument construction, failed bookkeeping, resource test, allocation, callback, retry or early return. Prove valid references, nonoverflow and sufficient funding before that point. The actual source must make the short gap inspectable; merely setting an entered flag is not evidence of invocation.

All argument evaluation in that gap uses prepared fixed values/references with proved nonthrowing construction. Unexpected exceptions after real entry are still an attempted invocation and receive a named outer outcome after funded cleanup; they do not refund the tariff. A violated private invariant is not an admitted funded failure path. No code may commit a tariff and then report that the invocation never happened.

## Truthful accounting and cumulative reconciliation

The adapter uses one Clock-to-Request-to-Plan chain and terminates on failure; it does not retry a stage in the same request.

Record attemptChargeConsumed and actual invocation count separately from observed successful cumulative accounting and known/unavailable store/visit observations. The origin includes each real semantic attempt tariff once plus every new actual action and all live reservations. If successful Clock/Request/Plan outputs exist, compare their cumulative accounting to the already charged legacy contribution; do not add that cumulative value again.

A failed stage creates no fake Plan, stale-receipt credit, refund, new origin, reusable retry reservation or stage-cap reset. Unused ordinary source/cleanup reservation remains distinct from executed work. Calling the tariff consumed describes the new semantic invocation event only; it never substitutes for an unobserved body trace or turns an unexecuted source maximum into measured work.

Legacy limits remain cumulative. Prove prior legacy contribution once, the held current stage once and genuine remaining origin headroom after all other actual/reserved work. Preserve old effective minima, byte/overlap policies and stored Clock lookup eligibility. The512+2*bit_width(R) frameAt threshold is eligibility only and adds no debit. Lower-only recipe BODY limits remain separate from origin and legacy limits.

## Observable results and explicit unknowns

Return the old status and nested geometry status exactly. With mechanically proved immutable preflight and empty outputs:

- Request4096 is entered before nested key construction; key construction retains the existing clock and does not compile another map.
- Plan1024 and full scan-price prepayment occur before sample scanning. A returned ResourceLimit can only be the final allocation catch, proving N Frames and C*N samples were visited. NonFiniteSource still hides its failing index/channel and exact prefix.
- Clock InvalidClock/ResourceLimit do not prove workUnits_151 or a completed internal prefix. NumericDomain and success have their source-proved observations, without an invented allocation trace.

Unknown observations stay explicitly unavailable, not0 or N. Complete new source counters remain unscaled and inspectable. The recipe API's valid existing Plan does not bring failed creation into its observation window; the integrated job still requires its own source/resource/failure proof.

resolveMusicalAudioWindow/frameAt/source-position/standalone-key/equality calls outside an actually entered old region need their own complete new source census. Do not borrow CutPlan's price for an independent query, build a second clock or scan N again to infer an old failure trace. The ordinary Request constructs its key once inside its own region.

## Source, proof and admission

Accept this policy only after fresh engine/resource review. Then write the actual private adapter against the reviewed formation/request-owner source. Keep new literal costs unbound until whole source review and independent census. Mechanically test fee/refusal/invocation ordering, actual allocation failures, cancellation boundaries, precise statuses, known/unavailable observations, cumulative limits, no double accounting and every cleanup path.

All initial formation, ownership, source/API, independent whole R4 and combined integration obligations from ADR-0085/0088 remain. This decision grants no executable source, catalogue values, source-fit certificate, complete allocator/control-block/retained-overlap proof, waveform, Ready, renderer, worker/model/export/load or activation credit.

The production2^28 work cap,512MiB new-per-Clip and1GiB pending/cache resource limits stay unchanged. No Accepted ADR, old source/proof/golden/RT annotation, semantic price value, numerical tolerance, channel classification, DSP configuration or saved format is modified. Any failed source/proof requires correction; this policy cannot relabel a failure as tooling or a PASS.
