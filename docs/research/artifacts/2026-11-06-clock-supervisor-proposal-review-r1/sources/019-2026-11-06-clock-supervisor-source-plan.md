# Clock-research supervisor: proposed source delivery

Proposed under ADR-0090. The existing R12/R13 source, catalogues, gates and
evidence remain frozen. Root controls tracked edits, one heavy lane and all
execution authority. Nothing below is implemented, compiled or run.

## 1. Accept the decision and exact protocol

Fresh critic reviews the complete ADR, this plan and the source-options packet
at `build-ci/20261106-controller-owned-process-hypothesis-r1`. Resolve every
architecture/format ambiguity before accepting ADR-0090 or dependent code.

Request magic/version/header/string sequence and the derived41,252-byte bound
are defined in the ADR. Inputs retain exactly28/6 arguments, eight named
environment values and existing string guards; no arbitrary extension fields.
The source-work reservation is a reviewed same-origin runner slice, not a new
allowance, reset or substitute for the independent whole census. Failure before
source-counter observation retains that reservation and reports unknown usage.

Terminal is exactly224 bytes, with the ADR's explicit little-endian field order.
Bits0..9 mean, respectively: child created; raw exit observed; stdout worker
settled; stderr worker settled; stdout EOF; stderr EOF; current job-empty
observed; stdout final-position observation; stderr final-position observation;
source-counter observation. Bits10/11 mean primary native error observed and
first secondary native error observed. All other bits are invalid. A numeric zero with its
observation bit clear is unavailable, not a successful observation.

Status codes:0 settled target success;1 settled target nonzero;2 pre-execution
refusal;3 body failure;4 cleanup unknown/quarantine. Primary-site codes are0 none,
1 request,2 containment,3 spawn,4 read,5 stage admission,6 write,7 deadline,
8 target exit,9 worker settlement,10 job settlement and11 terminal publication.
A semantic refusal uses its site and leaves the native-error observation bit
clear; never read stale GetLastError or invent an API error. Native errors are
captured immediately at the documented failing API.
Secondary count saturates; retain first secondary Win32 error separately from
the first primary. Preserve existing controller primary/secondary semantics.

Per-pipe observed counts bytes delivered by settled reads, once. Written counts
confirmed successful write progress. Omitted counts the observed subset refused
before writing by a cap/guard. Unretained counts observed bytes without confirmed
successful writes, including omitted bytes. Thus written+unretained=observed
when those observations are exact, and omitted<=unretained. A failed write makes
position/physical progress unknown unless the final owned-handle position query
establishes it; do not manufacture a partial-write value. Preserve that distinction
in the parent diagnostic projection and raw retained files.

Request-path transport uses one source-bounded argument containing hex-encoded
UTF16 request-path cells, at most512 units. Specify/review exact executable
quoting, argument prefix, hex/NUL/EOF rules and the bounded direct command-line
parser before code; do not use arbitrary native argv or environment discovery.
All request/terminal files use fixed per-child names in the new owned transport
directory, exact EOF, current three identities and no-overwrite creation. Their
bytes and first-failure tails enter the existing namespace/retained/scratch sums.

## 2. Prepare a complete source-only successor

Write a new private supervisor and new runner revision; never patch R13 or define
its Data. All proposed helper and target execution guards start false. C++/Win32
code, entry/CRT strategy, imports, stacks, buffers, handles and every I/O and
failure owner require source correspondence. Do not suppress warnings, change
real-time annotations or credit guessed native allocation/free behavior.

Before target creation: same input/binary/fence checks, bounded request decode,
all owned storage/reservations, configured private job and exact handle-list
admission. Creation-time job membership/profile must have a supported proof;
unproved creator-death/native semantics remain HOLD, never an atomicity claim.
No inherited job handle or breakaway. If unsupported, never resume an uncontained
target. Preserve every returned failure and reached ownership state.

Use two private synchronous read workers, each with one4096-byte buffer and an
owned ready/acknowledgement protocol. The coordinator alone scans the complete
stage, checks names/reparse/first-refusal/aggregate and appends, matching the
existing shared1MiB and64MiB caps before each write. It acknowledges only after
all uses of the buffer end. Neither a short read nor process exit alone is EOF.
Derive full/short/zero/error progress counts; do not assume quiet child output.

Preserve1800s body/poll admission and bounded cleanup semantics, accounting
supervisor and parent loops separately. Cancellation never frees pending I/O.
Observe/copy target raw exit before releasing target handles. Job accounting
requires a documented current proof; account for references retained by process
and thread handles, and do not wait on a job as a generic all-exit event.

Publish terminal only after the relevant outcomes are observed and its exact
fields/flags/counter invariants hold. Parent admission requires exact identity,
length/version/EOF/status and cross-field checks; status0 additionally requires
every success observation. Missing/partial terminal or abnormal helper exit
retains unknown/quarantine and forbids another child. Last-job-handle close is
not a settlement certificate or resource refund.

Replace bootstrap MainModule only with a reviewed ProcessPath/prefix/null/host
correspondence under the exact fresh-launch path profile. Keep that narrow source
change explicit in whole functional and resource reviews.

## 3. Freeze source and obtain fresh whole reviews

One complete independent functional correspondence covers the whole runner,
helper, binary protocol, source/fence/file recipes, output/counter projections,
all failure/cancel/partial-write/cleanup paths and successor bootstrap. One fresh
whole census covers all languages, callers, argument coercions, getters,
constructors, runtime counters and all phase/failure schedules. No price carries
from R13 by naming or internal arithmetic agreement.

Join controller+supervisor+actual target ownership under32MiB, counting every
explicit stack/buffer/static/native/CRT/transport/cold/failed-prefix term without
per-process exemptions. Source primitive CPU opacity does not erase explicit
owners. Record exact unresolved platform/ABI/source/binary/native premises.
Freeze all helpers/oracles/source at first passing own checks; preserve failures.

## 4. Qualify mechanically without desktop interaction

After fresh source/critic acceptance, root may prepare a separate reviewed
compile-only purpose lane for objects/binary/ABI/console/import association.
No source acceptance implies compiler or executable authority. No fake Data or
price stub is used to manufacture a runnable binary.

Before later fixture/helper/Clock execution, complete the actual source/native/
resource/binding proof and a separate root one-attempt gate. Audited console
fixtures mechanically cover fragmented dual output, shared-cap races, blocked
stderr after target exit, partial writes, wrong/truncated terminal, clean nonzero
supervisor exit and source-injected API failures. No deliberate crash/WER,
windows, desktop input or audio/MIDI startup. Missing native proof remains pending.

## 5. Return to editing delivery

Commit logical reviewed chunks to main with STATUS and exact CI. Separate
docs-only CI, compile-only results and executable outcomes. This tooling slice
does not enable Musical audio. Continue public recipe, DSP/resources, renderer,
jobs/model/export/load/controls, then linked editing/folders and piano views.
