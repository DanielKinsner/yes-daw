# 0091. Distinguish supervisor job termination from job query failure

- **Status:** Proposed — source-only extension awaiting independent critic.
- **Date:** 2026-11-06
- **Deciders:** Root and independent source/resource critic within Option A delivery.
- **Related:** ADR-0090; its immutable protocol v1; the supervisor source plan.
- **Normative companion:** [Protocol v2 delta](protocol-v2.md).

## Problem

The unrun supervisor implementation exposed one missing diagnostic: v1 has JobQueryFailure32 at JobSettlement10, but no faithful reason for an actual TerminateJobObject failure. Calling termination a query would misstate the failing action. The first unsealed mapping is preserved as an authoring finding. No runtime failure occurred.

## Options and decision

1. **Recommended: add a distinct reason in protocol v2.** Preserve actual query failure32 and add JobTerminationFailure48 at site10. Version the unimplemented research transport; all byte extents and ownership policies remain unchanged.
2. Reuse query failure32 for a termination failure. Rejected: the published reason name would be false provenance.

Use option1 only after this exact ADR and companion pass fresh independent review. Root then authorizes only the corresponding unrun source changes and source checks. No compiler, helper, fixture or Clock execution follows.

## Exact scope

Protocol v2 consists of immutable v1 plus only the companion's explicit overrides. It changes request/terminal magic and numeric version, run-identity domain label, and adds one allowed site/reason pair. It changes no header/terminal offsets, field sizes, paths, environment/argv domain, flag, exit code, parent JSON key, cleanup-list extent, cap, cancellation/settlement rule or meter/reservation policy. All current source/native/profile/binding/fit and separate execution gates remain.

The new failure preserves the immediate documented native error, first-primary/secondary ordering and existing reap-failure cleanup category. Failed termination is an action failure, never observed exit or empty-job proof. Later independently observed facts remain valid; status/unknown/quarantine admission still uses the complete existing matrix. Actual query failures retain32.

This is a new internal research-wire revision before any supervisor target has been compiled or run. It changes no saved Project format, DSP policy, numerical threshold, Accepted ADR text or historical evidence. ADR-0090 and protocol v1 stay byte-exact. The candidate/manifest must pin both versions and this extension, then receive the already required whole source/census/resource review. No source price transfers by equal byte extent.
