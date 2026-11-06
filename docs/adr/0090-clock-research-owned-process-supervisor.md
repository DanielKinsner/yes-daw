# 0090. Own clock-research child processes and bounded capture

- **Status:** Accepted — R2 architecture/protocol; independent whole review passed115 checks. Implementation, fit and execution remain gated.
- **Date:** 2026-11-06
- **Deciders:** Root and independent source/resource critic within authorized Option A delivery.
- **Related:** ADR-0049, ADR-0078; Musical prepared-lane plan; frozen R12/R13, resource and V HOLD baselines.
- **Normative companion:** [Clock supervisor protocol v1](../research/clock-supervisor-protocol-v1.md). This complete companion is normative. No companion rule is deferred to implementation.

## Problem and scope

Fragmented pipe reads can create per-inner managed async owners despite two pending outer reads. Process.Kill(true) performs host-wide process/descendant enumeration, and bootstrap MainModule can enumerate modules. Existing child/log/poll caps bound neither population. All predecessors remain frozen.

This concerns research tooling for mechanical editing checks. It changes no Project storage, DSP/render policy, numerical tolerance, Accepted ADR, golden or RT annotation. No price, Data, native binding, resource fit or execution authority follows from this proposal.

## Options and decision

1. **S1, recommended:** a pinned native console supervisor, private job and two fixed synchronous read workers, with bounded request/terminal files. This removes parent per-inner async owners and host KillTree enumeration, and adds explicitly counted native/protocol/funding obligations.
2. **S2:** an in-process managed component with two dedicated synchronous workers. It avoids a supervisor protocol but keeps managed failure/thread/stack lifetime joins in the controller.
3. **S3:** fixed overlapped named pipes. It can avoid read-worker stacks but adds event/OVERLAPPED/name/security/cancellation proofs; anonymous CreatePipe pipes are not an overlapped substitute.

Adopt S1. Root authorizes only a new unrun source candidate under the [source plan](../plans/2026-11-06-clock-supervisor-source-plan.md). This is not compiler, helper/fixture or Clock execution authority. The complete independent R2 review and unchanged Proposed subjects are preserved in `docs/research/artifacts/2026-11-06-clock-supervisor-protocol-review-r2/` and `2026-11-06-clock-supervisor-protocol-authoring-r2/`.

## Containment and headless ownership

Configure an unnamed noninheritable KILL_ON_JOB_CLOSE job without breakaway. Use STARTUPINFOEX creation-time PROC_THREAD_ATTRIBUTE_JOB_LIST under a separately qualified Windows10+ profile and the exact three-handle target inheritance list. Do not invent transactional/creator-death guarantees beyond the qualified interface; create-suspended followed by manual assignment is not an unconditional substitute. Preserve original target identity/PE/before-after fences as well as helper identity.

Supervisor stdout/stderr are not redirected. The supervisor never reads ambient stdin or discovers an environment. Its fresh launcher must separately qualify null/noninteractive stdin and the inherited OS error-dialog profile. Target stdin is its own explicitly opened inheritable Win32 NUL read handle, preserving the selected predecessor char-device semantics; it is not an EOF pipe. Only that handle and two target pipe writers are inherited. Job, parent pipe readers, log/request/terminal, synchronization and thread handles are excluded. Every NUL/pipe/handle/error/close owner is counted.

Use fixed console executables, CREATE_NO_WINDOW, no shell and no GUI/foreground/input/audio/MIDI calls or deliberate crash/WER fixtures. This is an admitted fixed profile, not containment of arbitrary GUI executables. Native cancellation/close and job last-handle termination are actions, not observed settlement or a refund. Unsupported nesting/creation semantics remain an honest HOLD/refusal; no breakaway fallback is permitted.

## Capture and exact protocol

Two private workers each own one4096-byte buffer. They publish immutable generation/ready data and wait for coordinator acknowledgement. Only the coordinator appends and updates aggregate counters. It processes stdout then stderr, preserves the shared1MiB per-child log cap, performs the complete current64MiB stage scan/name/reparse/depth/extent/aggregate check before every actual write and acknowledges only after every buffer use ends. Short native writes receive a fresh check before continuation. Arbitrary fragmentation, EOF-short, error-partial, cleanup-unconsumed and failed-write progress are defined in the companion. No quiet-output premise or one-time/bulk reservation replaces current stage admission.

Request v1 is explicit LE:140-byte header, exact four path/eight environment/28-or6 argument strings, maximum41,260 compiler or18,644 native bytes. Header includes total and mandatory-tail reservations and three32-byte identities. The exact bounded raw command line is quoted helper path plus one uppercase hex-UTF16 request-path argument; decoded path<=512 cells and raw command line<=2,576 cells including NUL. The companion fixes the grammar, guard order, identity byte recipe, both process environments, target quoting domain, exact names and no-overwrite/EOF behavior.

Terminal v1 is exactly308 bytes, from8+16*4+3*8+3*32+2*7*8+4. It has explicit status/site/reason/native-error observations, primary/first-secondary/first-native-secondary records, source-prefix observation, body polls, three identities, separate confirmed-write and four physical positions, read-error partials and a bounded cleanup mask. No C++ padding is serialized. All flags, unavailable values, cross-field rules, signed32 exit reinterpretation (including a genuinely observed259), deadline/cap distinctions and existing child-row projections are normative in the companion. Missing values become explicit nulls in the successor row rather than false zero success. Successful row types and child names/counts remain unchanged; the stronger failure unknowns and new finite cleanup extent are openly reviewed research-wire changes.

Transport is the new owned root/transport directory outside stage, with three fixed request/terminal pairs, no progress files or retries. Full maximum transport bytes are79,472. All files, partial tails, namespace entries and buffers count under the unchanged whole scratch/retained policy; they are not opaque compiler output. Raw logs retain their existing stage paths.

## Failure, prefix accounting and completion

Actual target raw exit is independent of helper exit. Normal completion requires both logical EOFs and worker settlement, current job-empty proof, admitted/flushed writes, known positions, exact counters/identities and the complete success matrix. Generic job waits, close, termination request and absent notifications cannot establish it. Preserve each independently valid observation when a different owner or late publication action fails. Global quarantine/no next child is mandatory for unavailable/malformed terminal, nonzero/unobserved helper exit or unresolved ownership.

The helper reservation is a fresh reviewed same-origin slice of the runner bank. It contains a separately priced mandatory cleanup/publication tail. A serialized prefix meter closes admission and freezes its observation at BeginTail; all subsequent cleanup, worker return/settlement, formatting/publication/close/exit work is covered by the prepaid tail. The terminal reports observed PREFIX usage only, never complete actual helper usage. Unknown usage retains the whole slice; no automatic refund, reset, fake trace or new allowance is introduced.

After terminal-record freeze, helper exit0 is the separate publication/mandatory-tail commit. Controlled exits10/11/12/13 cover unavailable record, late tail failure, quarantine and invalid meter. A prewritten status0 cannot certify a failed tail. The parent preserves valid prefix facts but quarantines all noncommitted outcomes. General CLR/exception/ErrorRecord, native/cold/time and source/ABI association terms remain explicit obligations, not zero.

Count controller+helper+actual target together under unchanged32MiB controlled scratch, existing retained cap and original log/stage/target-body policies. Include main/worker stacks, all source/native/static/heap/CRT/TLS/attribute/job/NUL/pipe/synchronization/transport/poll/failure owners. No helper exemption, opacity extension, global cap raise, close/Dispose/GC/free credit or arbitrary multiplier is introduced.

The narrow bootstrap successor may use Environment.ProcessPath only with the original prefix/null/exact-host treatment under the immutable fresh-launch path profile. It removes the census only after concrete whole functional/resource review; no global growth bound or universal API equivalence is claimed.

## Ordered review and delivery gates

1. Fresh independent acceptance of the exact architecture/protocol/owner/funding strategy, then root's explicit authorization to author an unrun candidate.
2. New source-only helper/runner/manifest proposal; no R13 edits, Data, stub prices or executable authority.
3. Freeze the whole candidate at first complete passing own checks. Fresh whole functional correspondence, all-language census and resource proposals record every remaining native/ABI/profile/binding premise.
4. Where needed, a separately reviewed bounded compile-only purpose lane may qualify object/binary/ABI/import facts. It certifies neither runtime behavior nor Clock fit.
5. Only after complete actual source/native/resource/binding acceptance may root authorize one separately gated helper/fixture/Clock attempt. Mechanical console fixtures use source-injected failures and ordinary nonzero exits, never deliberate crashes/WER/desktop input.

Thus final source/native facts are not circular prerequisites to producing an unrun source candidate, while missing proof never becomes execution permission. Musical recipe/DSP/resources/render/jobs/model/export/load/controls and later editing work remain unfinished.

## Primary references

- [Creation attributes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute)
- [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
- [Job accounting](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information)
- [Anonymous pipes](https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations)
- [Pending I/O cancellation](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations)
