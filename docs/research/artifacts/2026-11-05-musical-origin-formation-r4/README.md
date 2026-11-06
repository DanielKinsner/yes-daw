# Musical request-owner prototype: headless proof archive

The standalone Windows-native proof passed **135,111 checks**, covering **88
requests, 121 asserted actual native attempts and 117 actual allocations**. All
eight reviewed mutations compiled successfully and failed through ordinary
assertion exit1; the unchanged baseline exited0. This archive makes the tested
prototype and its evidence available through Git.

The production application does not include these sources. Worker/model
integration, the R5 legacy adapter, the public recipe, complete heap/resources,
DSP/waveform quality and Ready activation remain separate unfinished work.

## Preserved source

`proof.cpp`, `formation.h`, `formation-data.cpp` and `proof-access.h` are exact
copies of the reviewed inputs to the first passing proof. The whole proof,
including its helpers and comments, is frozen. Its SHA256 is
`D5F041F751D7B482C6C63C251CA52B2BE30160592E7A542DDC413F2EAFFC8D85`.
Historical SOURCE ONLY/UNRUN comments remain unchanged; the measured scope and
status are recorded here and in the execution receipts.

The archive manifest pins every source/evidence copy and the three direct engine
headers. Source and JSON copies preserve their original bytes. Raw console text
has LF line endings here; its exact original bytes are preserved as base64 in
the manifest, together with the original hash. The original ignored artifacts
were retained. No executable, object, generated PASS row or hardware claim is
included.

## Reproduction profile

From the repository root, compile this directory's `proof.cpp` and
`formation-data.cpp` exactly once, with this directory and `src` as include
directories. The measured profile was MSVC19.44, AMD64, C++20,
`/EHsc /MD /O2 /DNDEBUG /W4 /WX /fp:strict /link /SUBSYSTEM:CONSOLE`.
Use a separate ignored build directory. Link the normal C++ runtime/thread
support. Do not include another formation header or the public recipe test.

The executable is self-asserting: only the complete passing run exits0 and prints
its full inventory. An assertion exits1; an unexpected harness exception exits2
and supplies no partial proof credit. Run only as a redirected, bounded console
child without a native window, desktop input, audio device or GUI startup. The
root's recorded runs additionally suppressed error dialogs in the ephemeral
launcher process. This report does not qualify another compiler, ABI or native
floating-point profile.

`native-layout.cpp` is a separate console measurement, linked with the same
`formation-data.cpp`. It measured Raw1200, State1952/alignment8,
metadata384, Parents32, map row24, application5200, Control owner2584,
size_t8 and conversion-unit0. Thus the reviewed fixed root is26271, below2^28.
The static conditions and full proof were compiled; this is not a whole heap or
retained-parent/control-block certificate.

## Independent checks and boundaries

The proof independently models all129 prices, precise reached Site counts,
phase/slice conservation, source copies, real allocation failures, cancellation,
real acquire/publication paths, pending/retiring preservation, parent release
order and final byte snapshots before actual native free. It observes125 Sites.
Site21 is unreachable on admitted schedules; Sites29–31 require enormous real
vectors and remain source-qualified. No vector internals, initial authority or
allocation outcome were forged to exercise them.

`mutation-source-preparation.json` contains the eight exact header-only deltas
and their safety conditions. Data and observers stayed exact, and the whole
proof hash was checked before and after every mutation. The raw failure excerpts
and actual compiler/process receipts are included. No crash, timeout, compiler
failure or terminate outcome counted as a biting mutation.

The earlier R2 proof stopped at two MSVC constant-condition warnings before
execution. The independently reviewed R3 changed only those two guards to
`if constexpr`; every assertion and normal schedule remained exact. Its first
passing source is the one archived here.
