# 0090. Own clock-research child processes and bounded capture

- **Status:** Proposed
- **Date:** 2026-11-06
- **Deciders:** Root and independent source/resource critic within authorized Option A delivery.
- **Related:** ADR-0049, ADR-0078; the Musical prepared-lane plan; frozen R12 resource and V HOLD baselines.

## Problem

The clock-research controller has two source-derived resource gaps. Fragmented
pipe reads can create per-inner managed async owners despite only two pending
outer reads. Process.Kill(true) enumerates host processes and descendants;
bootstrap MainModule can enumerate loaded modules. Its existing child, log and
poll limits bound neither population. The frozen source/evidence stays intact.

This decision concerns research tooling needed for mechanical editing checks.
It changes no Project storage, rendering policy, numerical tolerance, production
budget, Accepted ADR, golden or real-time annotation.

## Options

1. **S1, recommended: native console supervisor and bounded files.** One small
   pinned helper owns each target's private job, two synchronous pipe workers,
   fixed buffers and capture coordinator. The controller reads bounded terminal
   data without redirected supervisor pipes. This replaces both structural
   gaps, but adds a binary, protocol, native lifetime and funding proof.
2. **S2: managed component with two dedicated synchronous workers.** Avoids a
   separate supervisor protocol but retains managed thread/exception/stack and
   unsettled-worker ownership in the controller.
3. **S3: fixed overlapped named-pipe state.** Avoids blocking workers but adds
   event/OVERLAPPED/cancellation ordering. Anonymous pipes cannot supply that
   overlapped implementation.

## Proposed decision

Use S1, subject to fresh independent acceptance of this decision and its
concrete source plan. Source authoring, compile-only purpose qualification and
later target execution are separate gates. This ADR grants none of them yet.

### Containment and handles

Create an unnamed noninheritable job with KILL_ON_JOB_CLOSE and no breakaway.
Use creation-time PROC_THREAD_ATTRIBUTE_JOB_LIST under an explicitly qualified
Windows10+ profile, with STARTUPINFOEX and an exact inherited stdio handle list
that excludes the private job handle. Qualify the documented creation and
failure semantics; do not invent transactional or creator-death guarantees the
selected interface has not established. Suspended creation followed by manual
assignment is not an unconditional substitute.

Do not use a host process census, PID-list-sized allocation or Kill(true).
Admission failure occurs before the target executes. Preserve the original
target binary gates and before/after fences in addition to helper identity.
Existing nested-job incompatibility becomes an honest unsupported profile,
without a breakaway fallback or an invented successful target result.

### Capture and complete-stage admission

Exactly two private workers each accumulate at most4096 bytes in one fixed
buffer, stopping at full chunk, EOF or error. A coordinator alone appends to
stdout.log/stderr.log, accounts the shared1MiB per-child cap and acknowledges
buffer reuse. Workers may receive arbitrary one-byte fragments. Source-price
all progress and refusal work; fixed buffers do not make loops free.

Retain the complete64MiB stage-pool check before every actual write, including
allowed names, reparse/depth/extent checks and first-refusal ownership. Replacing
it with a one-time free-space snapshot or reserving1MiB that a quiet child need
not use would need a separate equivalence proof and is not selected here.
Serialize stage scan/check/write across both streams. A short or failed write
must preserve its actual known/unknown progress and primary/secondary failure.

The helper has no redirected supervisor stdout/stderr. Its own console binary
and the already gated target use no shell, interactive stdin, desktop input,
GUI/device calls or error dialogs under the admitted launch profile. These are
not containment guarantees for arbitrary GUI executables.

### Private request and terminal format

These are per-attempt research files, never Project files. Store them in a new
explicitly owned transport directory outside stage, with every file/byte counted
in the existing retained/namespace/scratch model. Do not classify them as opaque
compiler output. Add their exact paths/caps to the new manifest and fences.

Request version1 uses little-endian unsigned integers, magic `YCSREQ01`,
u32 version/totalBytes/childIndex/argumentCount/environmentCount, one u64
supervisor source-work reservation, followed by
three32-byte identities: manifest SHA, canonical run-identity SHA and expected
target SHA. Then come four length-prefixed UTF16 strings in order: target,
working directory, stage root, transport root; eight environment values in the
existing SystemRoot/windir/PATH/INCLUDE/LIB/TEMP/TMP/VSLANG order; then the exact
28 compiler or6 native-target arguments. Each string is u32 code-unit length
followed by that many u16 cells, at most512 units with the original string
guards. Header132 +40*(4+2*512) gives a derived maximum41,252 bytes. No padding,
trailing data, arbitrary error text, environment inheritance or extra argument
is admitted. Command-line transport of the request path must itself have a
fully specified bounded parser and exact quoting/profile correspondence.

Terminal version1 uses magic `YCSRES01`; ten u32 values in this order:
version, totalBytes, childIndex, flags, targetPid, targetRawExit, status,
primarySite, primaryError, firstSecondaryError; u64 secondaryFailureCount and
supervisorSourceUsed;
the same three32-byte identities; then four u64 counters per pipe, stdout first:
observed, written, omitted, unretained. Exact length is224 bytes. Encode fields
explicitly; never serialize C++ struct padding. Flags independently record child
creation, target-exit observation, each worker's settlement/EOF, current job-empty
observation, each written-position observation, source-counter observation and
whether each stored native error was actually observed. Unknown is represented by its
flag, never certified by a numeric zero. The expected target digest is an identity
label, not a claim that passing it proves the loaded image.

The source plan must define all flag bits/status/site codes, counter meanings,
overflow rules, request-path encoding and the parent projection to existing
diagnostics before dependent code. Exact length, magic/version/kind/EOF/current
identities and all cross-field invariants must pass before terminal admission.

### Settlement and failure

Observe the target's raw32-bit exit separately from supervisor exit. Normal
completion requires settled readers, completed/admitted writes, actual target
exit and a valid current empty-job proof. Job close, a generic job wait, a
termination request or an absent completion-port notification is insufficient.

Cancellation attempts and deadlines do not establish I/O completion. Retain
buffers, requests and workers until their completion is observed. Do not free
or recycle pending I/O storage. Missing/partial/wrong terminal data, abnormal
supervisor exit or unresolved cleanup becomes unknown/quarantine, preserves the
files/diagnostics/remaining owner terms and forbids another child. Killing only
the known supervisor is a cleanup action, not proof of target settlement.

### Whole accounting and bootstrap

Count the controller, helper and actual target together under the unchanged
32MiB controlled scratch policy. Include native/static/heap buffers, worker and
main stacks, handles, attributes, CRT/TLS/cold/native terms, file transport,
parent polling and failed-attempt ownership. No per-process exemption, opacity
extension, cap raise, quiet-output premise, GC/Dispose/close reclamation credit
or arbitrary safety multiplier is introduced. Source/resource/profile unknowns
keep the whole gate closed.

A successor may replace the bootstrap census with Environment.ProcessPath only
after preserving null/query refusal, original extended-prefix treatment and
the exact host comparison under the trusted immutable fresh-launch profile.
No global growth bound or universal API equivalence follows from the fixed path.

## Review and delivery

Fresh source/resource critic review must accept the exact protocol, owner state
machine, failure projections, complete source-work sponsorship and native/profile
premises before implementation. A separately reviewed compile-only purpose lane
may then qualify the helper binary/ABI without running it. Actual helper/fixture
or Clock execution needs its own complete source/resource/binding acceptance and
root one-attempt gate. Use source/injected errors and ordinary nonzero console
fixtures for abnormal paths; no deliberate crash/WER or desktop drive.

Musical public recipe, DSP, resources, jobs/model/export/load/controls and later
linked/folder/piano work remain unfinished. This decision cannot certify them.

## Primary references

- [Creation attributes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute)
- [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
- [Job accounting](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information)
- [Anonymous pipes](https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations)
- [Pending I/O cancellation](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations)
