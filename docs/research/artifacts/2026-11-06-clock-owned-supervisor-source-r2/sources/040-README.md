# Clock R15 complete owned-supervisor source proposal â€” UNRUN

This new sibling addresses the whole R14 critic's F1â€“F8 findings together. R14,
R13, all older sources, reviews, failures, catalogues and Accepted ADRs remain
unchanged. The target C++ and source-costs.h are byte-exact. Its relative
independent-clock include still resolves from this source-revisions sibling.

Accepted ADR0090 and ADR0091 govern protocol v2 (immutable v1 plus exact v2 delta).
Request140/41260/18644, terminal308, raw CLI, current identities, eight-value
environments, target NUL/handle list/quoting, flags/reasons, cleanup15, source
prefix/tail,7200 polls, >1800s deadline,1MiB raw/64MiB stage and32MiB controlled
whole scratch are unchanged. All execution flags are false; both Data remain
undefined. All prices, native/ABI/profile/final union/resource/binding/GO are HOLD.

Native source is under build-ci/20261106-clock-owned-supervisor-source-r2/native.
The ready slot stays immutable from publication until acknowledgement. A separate
exit record is read only after an actual thread-exit acquire. Completed Full,
EOF-short and logical zero-EOF results survive publisher meter/signal/ack failures
and are accounted once via ready or actual exit acquisition, using prepaid tail
work on debit refusal. Real synchronization errors stay distinct from read errors;
missing EOF alone does not manufacture a SourceInvariant. All native wait failures
are captured immediately through the common ready/exit helpers in body and cleanup.

TRUE/0 ReadFile is a private NoProgress control yield, never EOF or logical
completion. Active fill survives Continue; no observed bytes or EOF bit are added.
One normal coordinator poll can consume at most one slot per direction.7200
normal polls bound14400 such continuations plus two final pending slots; no
drain-until-progress loop, zero-write input guard or extra poll/deadline allowance
is introduced. Stop terminates continuation; mutable active fill is never read by
the coordinator. A frozen logical-error slot or actual thread-exit record is the
only source of known partial bytes. Pre-resume and cleanup can retain only one
pending slot per worker; they admit no extra normal continuation.

A failed FindClose retains its owner slot and prevents another search from
overwriting it, including publication scans. Other retained owner slots also have
no-overwrite checks. Each short-write attempt still follows a fresh complete
stage scan. A stage-cap refusal records precisely the remaining length-at once;
non-cap guards never become cap omissions. Typed PathResult distinguishes a source
debit refusal from actual extent rejection. Coordinator spend enters the ordered
meter/tail path immediately; no caller relabels a refused operation as a path guard.

Failed CreatePipe/CreateProcess outputs are copied only as private raw object
representations, never promoted to handles/PID/ChildCreated or passed to wait,
close, identity or resume APIs. A distinct unknown-native-creation state forces
status4/parent quarantine even when every established owner settles and the
private job is independently observed empty. Raw zeros do not prove absence.
Successful creator contracts establish normal owners. Unknown PID/exit remain
canonical zero with flags clear; conservative unknown_undrained remains true.

The parent runner's88 helper bodies are unchanged from R14. Only the derived
bootstrap node literal changes. Its strict binary terminal admission, nullable
old child keys, signed32 exit (including actually observed259), first-primary/
secondary policy, prefix preservation on late helper failure, before/after fences
and committed-status0 child parser route are preserved for fresh whole review.
Private slots/exit records/raw creator bytes never enter JSON. Transport six
files and helper companion retain their existing fences/namespace/descriptors.

The manifest provisionally extends R14's input union with all five R14 files and
five current native sources, preserving every old physical identity and adding a
closed current-native namespace. The helper binary's bytes/hash and native SDK
include union remain explicitly unbound for separately reviewed compile-only
purpose qualification and final recomposition. No old geometry, price or fit
transfers. Explicit buffers/copies/frames/stacks/native owners and every failed
or quarantined prefix count; generic runtime bookkeeping is unqualified, not zero,
and process exit/close/free/GC earns no physical-free credit.

Own file/text/hash/integer regression assertions cover each source counterexample
and analogous owner/caller/wait paths. They do not parse PowerShell/C++ or execute
the candidate. Actual mechanical runtime fixtures await separate source/native/
resource/binding acceptance and exact root execution authority. Whole independent
functional review and complete fresh all-language census/resource proof remain
required after first-green whole source freeze. No compilation or execution ran.
