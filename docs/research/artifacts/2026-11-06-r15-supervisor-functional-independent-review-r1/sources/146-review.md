# Whole R14 supervisor/runner functional review - REVISE

This is an independent review of the complete frozen candidate, not the author's
1,555-check consistency result. Root did not author this candidate. The separate
I/O critic reviewed the same frozen native sources. The original candidate,
failed author/check attempts, first-green freeze and all predecessor evidence
remain unchanged. No target parser, AST, import, compiler, helper, fixture,
Clock program, GUI, desktop input or audio/MIDI device was invoked by this review.

## Complete subject and coverage

The subject is packet `20261106-clock-owned-supervisor-source-r1`, pins
88BA2BDA5D1DEB4DC378D82431EAD036A51174D7C9AC11D2B4D03DC621E626A5,
and its complete five-file R14 sibling. Root read all five native modules;
the parent bootstrap, all14 new helpers, all five changed old helpers and main;
the unchanged before/after binary/input fences and child-result route; the
author/checker/proposal/correspondence; and manifest/profile/namespace/output
changes against immutable R13. All74 predecessor helper bodies are compared,
with69 unchanged and five changed. Both target C++/header remain byte-exact.
Every declared1,230 input and all subject/accepted-document pins are rehashed.
This does not promote the provisional union into the final execution union.

Coverage includes request bounds, raw argv and UTF16, current run/manifest/target
identities, compiler/native recipe, argument quoting, eight-value environment,
path/reparse checks, hashing and stage scans; creation/job/stdio/attribute owners;
worker publication/ack/Stop, body/cleanup/poll/timeout paths; partial reads,
short/failed writes and independent positions; meter cut/static tail; immutable
first errors, terminal validation/publication/late exits; parent admission,
signed exit projection, null availability, cleanup extent/quarantine; namespace,
retained/input/transport/output correspondence and protected controls/fences.
Selected native, ABI, source-price and joined-resource propositions remain HOLD.

## Required corrections

1. **Published worker data is mutated before acknowledgement** (`sup_main.cpp:36-40`).
   After successful ready signaling, an ack-wait failure calls `worker_error`,
   rewriting kind/count/error while the coordinator may read that published
   generation. Keep the entire published slot immutable until acknowledgement;
   later synchronization failures belong in a distinct exit record, acquired
   only after the worker's exit is observed.

2. **A completed logical read can lose its bytes** (`sup_main.cpp:21-33`).
   WorkerPublish refusal and ready-signal failure replace a completed Full or
   EofShort record with Error/count0, while partial can remain0. For a completed
   4,096-byte read, neither observed nor errorPartial then retains its bytes.
   Keep the completed result for ready or thread-exit acquisition. Publication
   and meter errors remain separate; mandatory completion/accounting is prepaid
   tail work. Do not fabricate a read failure or duplicate one API failure.

3. **Some native wait failures are discarded** (`sup_main.cpp:253,303`).
   The coordinator's inline worker-thread wait and the cleanup retry's ready
   wait have no WAIT_FAILED branch. Later calls can overwrite the real native
   error; a later timeout can become the first diagnostic. Capture each actual
   failed wait immediately and apply its existing worker-settlement failure
   pair/mask, preserving first-error order and unavailable settlement.

4. **A successful zero-byte pipe read is treated as EOF** (`sup_main.cpp:136`).
   Microsoft distinguishes a successful zero-byte pipe read caused by a peer's
   zero-byte write from closed anonymous-pipe ERROR_BROKEN_PIPE. The current
   branch can stop both readers before subsequent real bytes and later admit
   status0. Preserve the documented distinction and bounded source/cancellation
   schedule; do not add a quiet-peer or zero-write-free domain assumption.
   See [ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile).

5. **An unreleased search owner can be overwritten** (`sup_io.cpp:163,192,359`).
   Failed FindClose retains search_owner, but a later stage_scan stores a new
   handle in that slot unconditionally, including during publication after a
   failed cleanup retry. Refuse/reuse safely while retaining the original owner;
   do not drop it or count its release merely because another scan is needed.

6. **A cap refusal during a short-write check loses omitted bytes**
   (`sup_io.cpp:318`). The initial scan records length on StagePoolCap, but the
   repeated per-write scan returns without recording length-at. Account the
   refused remaining bytes once, preserve already confirmed writes, and retain
   the distinction between cap omission and non-cap guard failure.

7. **A source-meter refusal is relabeled as a path extent error**
   (`sup_io.cpp:158-159,176`). join_path can return false because its source
   debit refused, but callers manufacture StageExtent/StageGuardRejected. Keep
   the actual refusal cause and its ordered meter/tail transition; report a
   path extent only when that extent guard actually failed. Apply the same rule
   to every helper/caller seam, rather than special-casing these two examples.

8. **Failed creator outputs become trusted handle owners**
   (`sup_io.cpp:237-241,282-289`). CreatePipe explicitly leaves failed outputs
   indeterminate; non-null words do not prove safe CloseHandle ownership.
   CreateProcess failure likewise lacks a demonstrated selected-native validity
   premise for the candidate's pre-success ChildCreated/PID/handle promotion.
   Retain raw/possible-partial provenance and unknown native ownership, quarantine
   when unresolved, and promote/use/close only established owners. This neither
   erases possible partial state nor invents a valid owner from arbitrary words.
   See [CreatePipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe)
   and [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).

These are source findings and supported counterexample paths, not observed
runtime failures. Source assertions identify their exact frozen seams; they are
not runtime regressions or compiler/ABI/fixture proof.

## Examined nonfindings and limits

The4,156-cell environment bound is correct: the final terminator uses the last
cell directly, not the spare-cell append helper. The protocol's combined parent
position-completeness rule intentionally makes both deltas null when incomplete.
Raw259 is preserved only after an actual signaled-process/exit-code observation.
The per-child raw-log cap matches R13's per-child counter, without introducing a
new cross-child cap. Prefix observation remains distinct from full sponsored
helper work; no refund follows. Reason48/termination and32/query remain distinct.

The general claim that failed ReadFile's byte count is necessarily undefined was
not substantiated for the examined broken-pipe path; it is not a finding. The
proposed HeapFree stale-error finding was also withdrawn after checking current
[HeapFree documentation](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapfree),
which permits GetLastError on failure. HeapAlloc is handled separately and does
not receive an invented native code. Argument-template replacement order remains
a binding/profile correspondence obligation, without a fabricated failing path
for the concrete selected paths. No physical-RSS or whole generic-CRT proof
requirement was added to the accepted controlled-source boundary.

## Disposition and next authority

**REVISE the whole candidate.** Root authorizes one complete new source-only
successor addressing all findings together and inspecting the analogous paths.
Do not patch this frozen candidate or any predecessor, Accepted ADR, golden,
production cap or RT annotation. Preserve complete author inputs and failures;
freeze every complete successor at first passing own checks; then obtain a new
whole independent functional review. A few passing seam assertions or fixes do
not establish whole acceptance.

Target parsing/AST/import, compilation, helper/fixture/Clock execution and any
Data/price/fit/native binding remain unauthorized. After functional acceptance,
the separate all-language census, controlled lifetime/native/profile/binding
gates and exact root one-attempt authority still apply. The actual type/layout
purpose and unchanged app-code CI retain only their previously measured scope.
