# Complete R16 source successor — F9/F10 and analogous admission paths

The whole R15 functional critic returned REVISE. This single new candidate fixes
F9/F10 together and retains all previous subjects and F1–F8 corrections. No old
reviewed row, source price or resource/native qualification transfers merely
because some source is unchanged. All source/ABI/native/controlled-resource/
profile/final-union/binding/GO gates remain HOLD, both Data undefined and all
execution guards false.

## F9: admission is different from prepaid completion

`admit_body(Meter&,Family)` is a prefix-only ticket under the **same** SRW lock
and tail/refused/valid/used/limit state as BeginTail. It admits only before the cut,
checks the real prospective positive family price and subtraction-first remaining
credit, debits once and freezes the prefix on refusal. A tail-capable `spend`
cannot serve as this ticket. The coordinator wrapper additionally refuses an
already observed stop_body and records the real source cause through BeginTail,
preserving the original primary/secondary/native-secondary ordering. Read and
write tickets use this same generic admission; no standalone tail snapshot is
the authority for starting a body action.

Target resume has its own TargetResume ticket immediately before TargetMayHaveRun
and ResumeThread. If a worker's first read refuses before this ticket, the target
stays suspended and the existing reached-owner pre-execution cleanup path settles
it. A cut after the resume ticket cannot revoke an already admitted native call:
MayHaveRun is honestly set before the attempted call, native errors are captured
immediately, and its completion/failure is part of the prepaid tail. No resumed
body group or fresh read/write/poll ticket can subsequently be admitted.

Each WorkerStart ticket admits one explicit bounded setup group: ownership
preconditions, its three events, CreateThread, worker-entry started publication
and one bounded started-event wait. A cut before its ticket creates none of those
owners. A cut after its ticket may leave the already admitted group completing
afterward; its actual source/copy/native ownership and completion/error paths must
be sponsored by the fresh prefix/tail census. This is an explicit pre-admitted
group, not implicit permission from tail-mode spend. The worker's first/further
native read still needs its separate prefix-only ticket; it cannot use the setup
ticket to read after the cut. A later worker group and target resume refuse.

The analogous fresh entry seams are protected too: CaptureSetup before current
capture setup/scans, per-log Owner, contained-target Owner, target quoting and
environment groups, coordinator/poll groups, normal acquired-slot Consume and
WriteChunk. The source contains no setup retry after refusal. Only groups admitted
before a concurrent cut may complete; the future mandatory tail must include the
maximum such pending setup/iteration/chunk completion, not treat it as measured
post-cut prefix usage or unused allowance. No refund or new per-process bank is
introduced. Capture/contained setup precede worker creation in the sole normal
call route; the prefix-only guards also prevent a known-tail entry into them.

Acquired ready-result accounting stays mandatory. A refused normal Consume ticket
switches that already acquired result to cleanup accounting. Tail/exit acquisition
uses tail-capable spend and accounts Full/EOF-short/zero results once; it never
drops completed bytes because its body admission refused. Publication, exit
records, final result consumption, cancellation/settlement/flush/positions/safe
releases, terminal validation/encoding/publication and post-record failures remain
prepaid tail. An already admitted write-chunk group may finish its current guards
after a cut, but every actual WriteFile needs its separate same-lock write ticket.
At most the admitted group completes; no new chunk or iteration is started by a
successful tail-mode spend.

The source-only regression packet covers no-cut, first-read cut before worker1,
cut before/after each setup ticket, cut before/after the resume ticket, real failed
resume after admission, invalid/refused source state, and mandatory acquired
completion after cut. These finite source/integer models supply no runtime
fixture, price or actual Windows synchronization qualification.

## F10: current descriptions have separate source-derived meanings

`metadata-derivation.json` records complete names and current caller correspondence:

- Prospective path catalogue:56 references +3 build +4 controls +6 stage +6
  transport =75 run-file positions. It includes potential bundle and failure
  inventory positions and excludes the helper companion outside the run root.
- The main's pre-final-bundle exact success recipe has73 files, using its four
  actual stage names. Adding bundle gives74 normal-success files. Failure inventory
  is the one prospective extra, rather than a success artifact.
- Failure/root walks retain the existing85-entry admission ceiling and depth64.
  Filtering admitted rows to files yields at most85 raw failure file rows; unknown
  or refused compiler outputs are not silently reduced to the75-name recipe.
- The existing bounded child cleanup schedule is15, distinct from11 controller
  first-secondary slots and from the mutually exclusive14 prefix categories.
- Current authenticated inputs1250 +four own sources +manifest +future helper
  identity =1256 current/transitive identities and dynamic output-array maximum.
  Adding75 prospective run positions gives1331 path-catalogue positions.34 closed
  namespaces contain1060 file names/1107 total entries. Every value is recomputed
  from this candidate's source/lists; final SDK/binary canonical union remains
  explicitly unbound.

Old unqualified69/78/13 descriptions are retained only in an explicitly unused
historical_R15 description object. Current fields are75/85/15 with clear meanings;
no precedence trick or blind69-to75 success substitution remains. Current JSON
max-array metadata follows the actual dynamic parent recipe, instead of retaining
1225. The C++ cache wording distinguishes785 total pair construction inventory
across735/25/25 sequential rate caches from735 maximum live rows. That changes no
producer/reference/price/cap. Current child-lifetime wording describes the actual
fixed-worker supervisor rather than the earlier async controller implementation.

## Whole preservation and remaining gates

All88 parent helper bodies remain byte-exact R15; only its main bootstrap node
literal is rederived. The whole five native files use a new private namespace.
Target CPP/header, relative independent-clock include, compiler arguments and
environment values, all runner/module/numeric controls, wire v2, nullable old
output keys, signed/raw exit projection, deadlines, before/after fences and
first-error ordering remain preserved whole-review subjects. No private ticket,
slot, exit or failed-creator object becomes a wire field.

F1/F2 keep immutable ready slots and completed bytes through publication failures;
F3 captures every actual wait error before another API; F4 retains TRUE/zero as
private bounded NoProgress; F5 preserves occupied failed-close owners; F6 counts
only the refused remaining cap omission; F7 preserves typed source/extent causes;
F8 keeps failed creator outputs as raw unknown provenance with quarantine. The new
ticket gate does not erase those acquired facts or replace cleanup with a retry.

The provisional union keeps every distinct predecessor physical identity and adds
all five R15 sibling files plus five current native files. No content deduplication
or future binary-size-zero assumption is used. Own five-file/bootstrap limits,
retained arithmetic, namespace/current identity fences and all false guards are
recomputed. Native/ABI/import/CRT/creation/nesting/cancellation/reference/stack/time
and loaded-image associations remain separate obligations. Explicit source-owned
objects count; generic runtime bookkeeping is unqualified, not zero or a demand
for physical RSS. No GC/free/close/return/process-exit credit or32768 padding is
introduced. Fresh whole critic review must follow the first complete own green.
