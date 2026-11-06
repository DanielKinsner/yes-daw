# Named native progress premises: no numeric or execution credit

The source functional disposition is conditional on the named native/profile
premises. It does not close complete finite native-call/tail sponsorship.

In `native/sup_io.cpp:183-210`, `stage_scan` initializes `count=0`, increments it
only for names other than `.` and `..`, and refuses an eighth non-dot result.
The counter therefore bounds non-dot result bodies, not every FindNextFile call.
Dot results still reach StageEntry work and FindNextFile. Publication and its
short-write continuation scans are mandatory tail calls; a successful tail-capable
`spend` is not a finite enumeration counter or a refundable prefix bank.

Current primary API documentation examined on 2026-11-06:

- [FindNextFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findnextfilew)
  describes successful next-file/directory results, failure/indeterminate output,
  no-more-files, and filesystem-dependent unspecified ordering. It does not state
  a numeric dot-entry multiplicity bound.
- [FindFirstFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew)
  describes first search results, long/short-name matching, and warns that another
  thread/process can create or delete names between query and use. It supplies
  no quiet-directory or general immutable-snapshot premise.

Consequently a complete finite StageEntry/FindNext/tail source upper needs a
supported selected filesystem/search progress and dot-entry multiplicity premise,
or a separately reviewed explicit source bound. Neither finite namespace names
alone nor the non-dot count supplies it. No maximum of two dot results, each-name-
once theorem, quiet directory, snapshot uniqueness, sufficient slack or numeric
native time bound is silently assumed here. There is also no evidence in this
review of an actual repeated-dot API outcome or runtime hang; that is not a
supported source-bug counterexample. The current proposal already keeps all
native/profile and complete pricing/tail gates unqualified, so this is an
explicitly named HOLD at those later gates, not acceptance of a finite price.

Other source loops have explicit local progress/range arguments: bounded CLI /
request / UTF16 / SHA and quoting extents; path depth64; positive request/hash
read/write progress and exact remaining bytes; body7200 admissions; per-direction
normal TRUE0 continuations at most14400 plus two pending slots; positive-fill
byte/cap refusal; finite cleanup retry indices and fixed owner release groups;
terminal308 positive-progress writes; bounded parent enumeration counts/depth,
hash byte lengths, JSON grammar/preflight/token/container extents, control sets,
bundle iterations and full-fence occasions. These source bounds do not prove a
native call returns within a time/resource bound. In particular a blocked worker
ReadFile/INFINITE ack wait remains a pending owner; cancellation is an action,
not settlement, and the nonreturning process boundary stays separately gated.

R13 scalar evidence, even if separately conditionally accepted, supplies no
R16 source price, stage enumeration bound or admitted-group/tail sponsor.
