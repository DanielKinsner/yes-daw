# Finite old-Clock FP callsite measurement, revision 1

This directory was authored source-first and unrun. That is a historical authoring
statement, not a claim about any later execution. Root owns independent source
review, compilation and execution. Nothing here changes production code, accepted
ADRs, frozen proofs, funded-work caps, RT annotations or saved data.

This probe measures a finite question: do public integer-tick queries on the
unchanged old `MusicalAudioClock` agree bit-for-bit with a separate scalar replay
using the actual old `detail::appendTempoSegmentFrames` helper, under each tested
native compilation profile? It cannot establish whole source/native equivalence,
reveal every private prefix byte, establish an interval-wide FP/new-handler
contract, or qualify/authorize any funded query implementation. The R2 adapter is
not included or exercised. No source-trace work count is inferred from outputs.

## Source roles and finite inventory

- `build-and-query.cpp` defines NOINLINE bridges to the public old build and
  `frameAt({tick, 0, 1}, out)` methods. It never reads private fields.
- `replay-probe.cpp` has the sole `main`. It reconstructs only one scalar prefix
  in row order, forcing each completed prefix through `volatile double` storage.
  It uses the actual old append helper for full and partial segments. Its row
  search is linear and independent of the old compiled upper-bound search.
- `fp-observer.cpp` defines the NOINLINE read-only native observer. It is compiled
  in strict FP mode even when the two numerical translation units use precise FP.
  The shared header has observer declarations and POD values, no inline FP reads.

There are 16 fixed maps and 80 seeded maps, each with at most 16 rows, at six rates:
12000, 24000, 44100, 48000, 96000 and 192000 Hz. The fixed maps include empty and
single-row maps, equal/up/down ramps, adjacent representable BPMs, a long downward
ramp, a finite prefix above native-frame and exact-integer-double limits, extreme
and denormal BPMs, and three genuinely invalid BPM inputs. Seeded maps use a fixed
xorshift seed, positive bounded gaps and ordinary BPMs. All nonempty fixtures start
at tick zero and have strictly increasing nonnegative ticks.

For each successful Clock the probe checks every row knot, selected strict
interiors, guarded tail points, and a fixed list containing negative/zero/small
ticks, 2^53 +/- 1, and INT64_MAX - 1/INT64_MAX. Integer additions/subtractions are
bounded before use. No native integer cast occurs unless the replay frame is
finite, nonnegative and strictly below 2^63. An empty map uses the unchanged old
constant-tempo formula separately, with no invented row or append call.

Every ordinary valid map must build successfully. The three invalid BPM maps must
return `InvalidClock`. The two extreme ramp maps may build successfully or return
the actual old `NumericDomain` refusal. Other build outcomes fail, including
`ResourceLimit`; allocation loss cannot silently reduce the measured inventory.
Build status counters record the actual outcome. Refused maps receive no successful
query/prefix observation credit.

For `Ok` public queries, low/high double bit patterns (including signed zero) and
the native floor are compared. Every non-Ok result is compared by status and the
documented public output reset. A knot returning Overflow/NumericDomain contributes
only `knot_status_only_no_cache_bytes`; its private cached prefix bytes are not
observable and receive no byte-comparison credit. A failed full scalar replay
after a successful actual Clock fails immediately, including failures that would
otherwise be hidden by a zero-distance selected query.

All counters printed by an eventual run are calculated during that run. Source
inventory sizes are not measurement results. Query/replay totals are printed
rather than filled with manually authored PASS rows. Output is bounded: at most
one failure record, or a small fixed summary plus nine status rows and one result.

## Native control and observation limits

The first observer must recognize nearest rounding, masked FP exceptions and
disabled flush/denormals-are-zero behavior before the extreme fixtures are made.
MSVC x64 reads raw MXCSR plus `_controlfp_s(..., 0, 0)`, CRT rounding and exception
flags. It rejects ambiguous CRT controls. It does not read a raw x87 control word
on MSVC x64. GNU/Clang x64 additionally reads the x87 control/status words;
GNU/Clang ARM64 reads FPCR/FPSR and currently accepts only FPCR zero. Other native
observers, including MSVC ARM64, return unqualified. These source branches do not
constitute GNU/macOS/ARM64 native qualification.

The observer never writes controls, clears status/errno, enables traps or installs
a handler. Before/after samples surround actual build/query, each replay append
and the empty-map formula. A sampled control change fails. Status and errno may
change; counts describe observer-inclusive changed intervals. Sticky flags are
not cleared between operations, so unchanged samples do not imply no exceptions,
and a change does not isolate its cause. The observer itself and intervening
harness code may affect observations. Precise numerical code assumes the default
environment; strict observer calls do not prove controls/status inside an old call
or callback, compiler treatment of every intermediate, or control equivalence.

`std::get_new_handler()` must be null at initial and final boundaries. Nothing
installs a handler or forces allocation failure. Those samples do not prove the
handler remained null throughout every allocation interval.

All generated containers are local to the caught `main` path; there are no throwing
global constructors. Any mismatch exits 1 with a bounded diagnostic. Unsupported
initial native state or a caught fixture/runtime exception exits 2 and receives no
measurement credit. The successful self-asserting measurement exits 0. There are
no assert/abort/terminate, intentional crash/WER, GUI, audio, MIDI, device, thread,
external-data or environment-dump paths.

## Root's future build and evidence procedure

1. Independently review these whole sources before any compilation or execution.
   Verify `source-pins.json` against exact UTF-8/LF bytes and unchanged old headers.
2. Use the actual project MSVC x64 toolset, C++20 include paths/runtime and Release
   flags (`/O2 /Ob2 /MD /DNDEBUG /W4 /WX`). Keep NOINLINE boundaries, no `/GL`, no
   LTO and a normal CONSOLE entry point. Do not link/run a GUI target.
3. Build exactly two executables from the same whole source bytes. In profile A,
   compile both numerical TUs with the project's default `/fp:precise` behavior
   (no added FP override); in profile B compile them with `/fp:strict`. Compile
   `fp-observer.cpp` with `/fp:strict` in **both** executables. Set no `/fp:contract`
   or other unrecorded arithmetic option. Link ordinary C++ runtime dependencies.
4. Record actual compile/link commands, compiler binary hash/version, source pins,
   executable hashes, exit codes and complete bounded stdout/stderr. Execute only
   these console programs, headlessly, in root's serialized native lane.
5. Freeze the whole source set at the first GREEN, including all helpers, comments
   and this README. Do not copy/suffix or rewrite a whole probe to add work after
   that boundary. A future source revision needs a separately identified purpose
   and review, and cannot retroactively upgrade this finite evidence.

Root may request assembly listings as part of the same future builds to inspect
actual callsite/inlining/libm behavior. Neither matching assembly bytes for selected
functions nor equal finite outputs proves all paths, private states or all native
profiles. Any full equivalence claim still needs source induction and an explicit
native-profile/ABI qualification. Pending non-Windows profiles remain pending.

The pins cover the five authored non-manifest files and the literal quoted-include
closure beneath the unchanged old header. They do not pin the compiler/SDK/standard
library or certify a preprocessed dependency closure. The manifest's own SHA-256
is reported externally at handoff, avoiding a self-referential digest.

## Primary references checked by root

[MSVC `/fp` behavior](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170)
documents precise/strict behavior, rounding boundaries, contraction and FP
environment access. Strict observation is required here; using a strict observer
does not extend those guarantees into a precise numerical TU.

[Microsoft x64 ABI FP controls](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170#fpcsr)
describes nonvolatile control portions and volatile status portions. Call-boundary
ABI obligations and observed endpoints do not prove arbitrary callback intervals.
