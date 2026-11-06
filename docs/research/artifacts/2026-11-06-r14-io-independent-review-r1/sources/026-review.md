# R14 native I/O independent source review

Disposition: **REVISE**. This covers every function in `sup_io.cpp`, its
`sup.h` ownership declarations and relevant core/codec/main caller contracts.
Root owns the whole runner/helper/manifest review. These are source findings,
not observed native failures. No target parser, AST, import, compiler, helper,
fixture, Clock, GUI or device ran. No source/ABI/native/resource/binding or
execution gate is accepted.

## Complete findings

1. **IO1: a failed search close can lose its owner.** `sup_io.cpp:163`
   unconditionally replaces `state.search_owner`. A failed `FindClose` at192
   retains the previous search owner; a failed cleanup retry at360 also retains
   it. Terminal publication performs another `stage_scan` at371/376 and can
   replace that unreleased value with a new search handle. The old handle is no
   longer represented, despite the required retained-owner/quarantine rule.
   Never reuse an occupied owner slot. Refuse/quarantine or use a separately
   counted bounded owner representation; do not claim a failed close released it.

2. **IO2: continuation-stage cap refusal loses omitted bytes.** The initial
   `write_chunk` stage refusal at311–313 increments omitted, but the fresh
   per-write scan at318 returns directly. A cap race before the first actual
   write, or after a confirmed short write, sets StagePoolCap without adding the
   refused remaining chunk to omitted. Protocol7 requires the cap omission and
   a fresh complete check for each actual write; it allows no quiet-output
   premise. Add exactly the remaining `length-at` on a cap refusal, preserve
   earlier confirmed increments and do not mark non-cap guards as omissions.

3. **IO3: a meter refusal is mislabeled as a stage extent rejection.**
   `join_path` (`sup_codec.cpp:153–157`) returns false both for a source debit
   refusal and an extent violation. `stage_scan` at158–159 and176 unconditionally
   converts that false to StageExtent/StageGuardRejected. A source-prefix refusal
   on an otherwise valid short path therefore gets a fictitious filesystem
   refusal before BeginTail records SourcePrefixLimit. Preserve the refusal
   cause and first-failure order; do not manufacture a stage guard failure from
   a refused operation that did not run.

4. **IO4: a successful zero-byte pipe read is treated as EOF.**
   `sup_main.cpp:135–139` treats `ok && got==0` as ZeroEof/EofShort. The Win32
   documentation distinguishes a peer zero-byte write from closed-writer
   ERROR_BROKEN_PIPE. A zero-byte write followed by real data can therefore end
   capture early and let a later clean target exit certify incomplete output.
   No zero-write-free compiler/target premise exists. Classify actual native EOF
   separately; retain a bounded no-progress/refusal policy without inventing EOF.
   This conclusion follows from the documented pipe cases, not a runtime drive.
   [ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile),
   [WriteFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile).

5. **IO5: failed creator outputs are promoted to established owners.**
   `sup_io.cpp:237–241` copies failed CreatePipe outputs into normal pipe owner
   slots; cleanup later passes them to CloseHandle. Win32 explicitly describes
   failed CreatePipe outputs as indeterminate. Preserve possible partial/unknown
   provenance, force quarantine where ownership remains unresolved and do not
   operate on those words as safely owned handles. `CreateProcessW` at278–289
   likewise promotes handles and ChildCreated/PID before testing success. Its
   documentation does not supply CreatePipe's explicit indeterminate wording,
   so that stronger statement is not made for CreateProcess. It nevertheless
   supplies no failed-call validity guarantee for this promotion: nonnull and
   non-INVALID is not proof of a newly owned process or valid PID. Retain raw
   unknown/possible partial state separately and require supported selected
   native validity before asserting ChildCreated or waiting/closing it. A false
   return cannot erase possible partial state or establish safe release.
   [CreatePipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe),
   [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw),
   [PROCESS_INFORMATION](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information),
   [CloseHandle](https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle).

## Other examined paths

The complete function/API inventory is recorded mechanically. Current bounded
full-path/volume/parent/reparse checks, fixed-name stage enumeration, subtraction
pool admission, hash/request positive-progress loops plus EOF probe, request and
target share-read ownership, NUL stdin, explicit inheritance, creation-time job
attribute, suspended creation and capture append/position separation were read.
Failed writes never use an undefined count; confirmed increments are distinct
from independent positions. Terminal bytes freeze before publication, use
CREATE_NEW and positive bounded short writes, flush/close and separate helper
exit. Post-freeze errors change tail outcome; they do not rewrite terminal bytes.
These observations do not qualify native job/reference/cancellation/time/ABI
semantics, whole controlled storage, cold/CRT or physical process RSS.

Root's immutable-ready, completed-result loss and ignored-wait-failure findings
were cross-checked as relevant callers; they remain root's whole-review items.
The current official HeapFree documentation permits GetLastError on failure;
HeapAlloc carries the different no-SetLastError rule and candidate allocation
failure correctly uses no native code. Therefore no HeapFree stale-error finding
is asserted. Nor is an undocumented claim that the broken-pipe ReadFile count is
undefined: ReadFile documents its initial zeroing; the actual EOF distinction is
IO4 above. SDK10.0.26100.0 declarations were read and pinned as source only, with
no compilation or SDK/binary association credit.
[HeapFree](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapfree),
[HeapAlloc](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc).

The own checker confirms pinned source correspondence, complete I/O inventory and
the finite counterexamples described here. PASS means this source review packet
is internally verified; its candidate disposition remains REVISE. Existing
Accepted documents and the frozen candidate are unchanged.
