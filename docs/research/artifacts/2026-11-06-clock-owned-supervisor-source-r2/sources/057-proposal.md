# Complete R15 eight-finding successor — source-only

The root's complete R14 review returned REVISE with F1–F8, supported by its separate
I/O critic. This single new candidate addresses all eight and analogous caller,
owner and wait paths. It preserves R14 and every prior subject, Accepted document,
protocol v2 field/domain/cap and production byte. It does not inherit an acceptance
for unchanged rows or certify its own functional correctness. A fresh whole critic
must assess the complete candidate after the first complete own check pass.

**F1/F2 publication and acquisition.** Every ready-slot field is written before
publication and remains unchanged until successful acknowledgement. Publisher
debit refusal requests the cut without changing kind/count/partial/error. A failed
ready signal or ack wait appends a distinct exit diagnostic, never worker_error.
Workers write their exit record privately; the coordinator reads it only after a
successful actual thread-exit wait. The common settlement function acquires an
un-signaled completed slot, accounts it once without reset/ack APIs, then records
the real exit diagnostics. It serves body fallback and cleanup. Consumption debit
refusal immediately enters tail but still accounts the acquired completed record
from the prepaid tail. Missing EOF alone never manufactures an invariant error.
Generation exhaustion preserves the slot, records a separate counter failure,
invalidates counter availability and cannot wrap into a purported certificate.

**F3 waits.** ready_now is the one ready-wait/capture helper used by normal polls,
initial cleanup and every cleanup retry. settle_one is the common thread-exit wait
and acquisition path. Both immediately capture WAIT_FAILED before another API and
apply the existing WorkerJoinFailure/site9/mask3 rule. Started, ack, target-body
and target-cleanup waits retain their own explicit failed-result capture. A real
failed wait is never replaced by a later timeout or lack-of-EOF interpretation.

**F4 no progress.** TRUE/0 means a private NoProgress control yield, not closed EOF
or a completed logical read. The worker retains active_fill and buffer contents
across Continue. The slot carries no active-fill projection; consumption adds no
observed/omitted bytes and sets no EOF bit. A normal poll visits stdout then stderr
once, so at most one continuation per direction is admitted. Existing7200 bodies
allow at most14400 consumed no-progress controls plus two final pending slots.
Pre-resume start can hold only one pending slot per worker and admits no normal
continuation itself. Tail Stop cannot request another native read/continuation;
an already admitted call may complete. Cleanup consumes ready results once and
actual exit acquisition supplies any unfinished positive partial. An unfinished
operation canceled between physical calls carries a semantic cancellation
diagnostic with no fabricated native code. Synchronization failure retains its
actual diagnostic rather than duplicating it as a failed ReadFile. Only actual
ERROR_BROKEN_PIPE creates EOF-short/zero, with the prior synthetic next logical
zero after EOF-short. No peer zero-write restriction, extra poll/deadline or
drain-until-progress loop is added.

**F5/F6 owner and omission.** stage_scan refuses to create any search while its
single established search owner remains occupied. The slot is populated only by
a successful FindFirstFile result and cleared only after successful FindClose.
Failed close sets the existing owner-release group and preserves the exact value.
The same no-overwrite preconditions protect request/hash/capture/job/worker owner
slots. Every actual short-write attempt still has a fresh complete stage scan.
A repeated StagePoolCap refusal adds exactly length-at, once, before return.
Previously confirmed bytes remain written; non-cap failure never increases omitted.

**F7 typed cause.** join_path returns Ok, SourceRefused or ExtentRejected. All
callers compare the enum explicitly. Only ExtentRejected becomes StageExtent.
Coordinator spend records the actual meter refusal and linearized cut immediately,
before a caller can assign any later path/I/O reason. All coordinator debit sites,
including codec/hash/path/owner/consume/main, use this wrapper. Worker publication
keeps the separate cut request because only the coordinator owns terminal errors.
The write ticket wrapper similarly records an actual refused source admission.
Tail is prepaid; this does not turn a refused debit into uncharged success.

**F8 provenance.** Failed creator output object representations are retained in
fixed private byte arrays. No failed output word is evaluated as a handle/PID or
passed to CloseHandle/wait/identity/resume APIs. unknown_native_creation is set
independently of raw bytes (including all zeros), never cleared, and forces
status4/quarantine even after all established owners settle or the private job
is independently queried empty. The failed process path leaves normal process/
thread/PID/ChildCreated slots unavailable/canonical zero. Successful creators with
their success contract establish ordinary owners. A malformed success contract
also stays raw/unknown instead of promoting a partial collection. No failure is
treated as proof of absence; unknown native resource terms remain unqualified.

**Whole correspondence.** All five native files form a new private namespace.
All88 parent helper bodies remain byte-exact R14; only the main bootstrap node
literal is rederived. Target CPP/header, old control IDs, compiler arguments,
environment values, source/binary fences, strict JSON/UTF8/dependency/hash behavior,
terminal-v2 validator, nullable old child keys, signed exit259/0x80000000 projection,
parent first-error ordering and after-fence order remain whole-review subjects.
No private ready/exit/raw-creator array is a wire graph. The provisional input union
adds every R14 five-file identity plus the current five native files and a closed
native namespace; no equal-content physical identity is merged. Transport paths,
helper companion, retained arithmetic and actual byte/source pins are recomposed.

**Evidence boundary.** The own regression helper executes only handwritten finite
state/counter/arithmetic models and checks source spellings/order against frozen
counterexample seams. It invokes no target parser/AST/import/compiler/helper/
fixture/Clock/API/GUI and supplies no runtime regression credit. Each F finding
has a concrete modeled failing predecessor trace, supported corrected trace and
source assertion. Actual runtime fixtures remain for the separately gated lane.

Both Data are undefined, every execution flag is false and all actual source
prices, tail schedules, joined32MiB resources, retained/final union, native/ABI/
CRT/creation/cancellation/settlement/time/profile/binary binding and execution GO
remain UNBOUND/HOLD. Explicit bytes/copies/frames/stacks/owners count; generic
runtime bookkeeping is unqualified, not zero or a whole-RSS requirement. No close,
process exit, free or GC earns physical-free credit; no cap or padding changed.
