# Private native-region matcher: independent design critique

**ACCEPT A as the implementation direction, with the concrete requirements below.**
It can close the actual relevant-code/table association without an application-wide
cache or new module owner. It does not yet admit source execution, a NativeBinding
definition, a price or a complete native equivalence claim. The earlier reports
and every frozen artifact remain unchanged.

This is an ordinary private implementation of the accepted same-origin/native
qualification mechanism under ADR-0085/0088/0089. It changes no ownership topology,
saved format, numeric law, supported musical input domain or work/resource cap.
No additional ADR is required for this design. Adding an app-global loader/cache
owner, changing module lifetime policy, or replacing an owner-settled contract
would be a different design; none is needed here.

## 1. State exactly what a match establishes

The result means: the imported numerical entry addresses refer to one retained
image with the qualified RVAs, and every enumerated relevant instruction/immutable
data byte currently equals the reviewed profile. Combined with the separate
control, dispatch, lifetime and codegen facts, this supports the precise numerical
transition. It is not the complete CRT file's identity, an Authenticode claim,
a pathname check, proof that unexamined functions match, or proof that mutable
state never changes.

R4 should name this fact narrowly, for example relevant loaded math regions and
dispatch association, rather than silently upgrading R3's broader
LoadedCrtImageAndDispatchAssociation label to whole-image identity. A private
profile digest can identify reviewed evidence; nonzero bytes are only a
completeness check. No caller-supplied bool, token, range, expected byte buffer,
module handle, price table or override may establish qualification.

The profile must be private immutable constant-initialized data: exact region
roles, RVAs, sizes, expected bytes, entry RVAs and reviewed dependency evidence.
It must have no runtime initializer, lazy loader callback, heap vector, registry
or mutable successful-profile cache. A session-local matched state is ordinary
control state, derived only by the actual matcher; it is not evidence supplied
by its caller. All17 existing facts and both price definitions remain absent
until their complete actual proofs are accepted.

## 2. Entry addresses and the module boundary

Select the actual C double log1p overload before converting the function pointer
for Windows address lookup. The previous source review already found the
untyped-overload failure. Taking an address invokes no math. Take the FMA getter
address too, but do not call it until its required code is qualified.

Use GetModuleHandleExW with FROM_ADDRESS|UNCHANGED_REFCOUNT for each anchor.
Require both calls succeed, both nonnull handles agree and both guarded
integer address-minus-base values equal their qualified RVAs. A wrapper/thunk
must be recognized as such: the separate console's floor address was in its own
image. Do not infer its final math host from a containing-module result.
[GetModuleHandleExW contract](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw).

Use GetCurrentProcess with the selected K32GetModuleInformation/API configuration,
and require success, a nonzero image extent and lpBaseOfDll consistent with the
handle. Pin the selected PSAPI_VERSION/symbol/library spelling in actual source.
This API supplies image bounds, not a content identity. No process enumeration,
OpenProcess, LoadLibrary, FreeLibrary, PIN operation or new owning handle is needed.
[GetModuleInformation](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmoduleinformation),
[MODULEINFO](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-moduleinfo).

Check arithmetic before forming an address: positive region size, RVA within
SizeOfImage, length<=SizeOfImage-RVA, and representability of base+RVA+length in
uintptr_t. Use checked integer comparisons/subtractions, not unrelated C++ pointer
subtraction or unchecked end-address arithmetic. The MSVC AMD64 pointer/API byte
access model is an explicit supported-platform fact; this is not portable ISO
C++ code-memory introspection.

## 3. Page checks and the real lifetime premise

Before reading each chunk, VirtualQuery must return the complete selected
MEMORY_BASIC_INFORMATION representation. Require MEM_COMMIT, MEM_IMAGE,
AllocationBase==the qualified image base, positive RegionSize, BaseAddress<=cursor,
and cursor-BaseAddress<RegionSize. Derive a positive chunk no greater than both
the remaining expected span and the queried region remainder, using checked
arithmetic. A profile region can cross page/protection boundaries; one initial
query cannot license the remaining pages. [VirtualQuery](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualquery).

Use current Protect, not AllocationProtect. The source must explicitly list
accepted readable protection classes and reject NOACCESS, GUARD and unknown
modifiers. For these immutable code/table profiles, prefer executable-read code
and read-only tables; writable/copy-on-write classes need an additional concrete
immutability argument and should not pass by an overly broad readable mask.
PAGE_EXECUTE alone is not an explicit read permission. MEM_IMAGE does not itself
prove an unmodified shared page. [Memory protection constants](https://learn.microsoft.com/en-us/windows/win32/memory/memory-protection-constants).

VirtualQuery is an observation, not a lock. UNCHANGED_REFCOUNT acquires no owner.
The supported read interval comes from the actual main image's static import
dependency retaining the host CRT, no relevant unload/remap/protection/code-write
path in the closed admitted application, and shutdown ordering that joins/drains
the request before module teardown. Do not replace this with repeated queries.
Conversely, there is no need to defend against arbitrary injected or
contract-violating modules as if they were ordinary valid callers.

Use a literal bounded byte comparison on the validated chunk, with no speculative
source over-read, C string function or unmetered memcmp dependency. Pin the actual
supported MSVC byte-access implementation and emitted footprint if needed.
Volatile byte reads can make each actual observation explicit; they do not solve
unload races or establish immutability. A normal source/lifetime proof suffices;
no new module cache, loader lock or global ownership mechanism is required.

## 4. Complete the dependency closure before defining profile bytes

The offline profile needs a finite dependency certificate rooted at the exact
log1p and getter entries. Enumerate every direct/conditional branch, internal
call, RIP-relative load and indexed-table read reachable on the old helper's
complete guarded domain. Classify each dependency as matched immutable code,
matched immutable bytes, separately proved mutable state, or a proved-unreachable
edge. Byte equality of a chosen fragment does not prove the chosen set complete.

For the inspected candidate, guarded log input is finite and>-1 and y=1+x is
positive normal. That supports exclusion of the wrapper/log domain-error paths
only after their actual classification constants and predicates are bound.
Never exclude an error/callback path just because finite fixtures did not reach it.
The SSE and FMA valid arms both exist. Prefer matching both finite arms and their
shared constants/tables. A selected-arm-only profile is possible only if its
selector value and true interval are already independently proved.

Indexed tables require derived complete index ranges for all supported inputs,
including tiny/huge BPMs and ticks above2^53. Do not match one observed table
element or guess a table length from nearby export names. The earlier actual
instruction listings expose multiple table bases and constants; their full
referenced extents still need the concrete finite certificate. That is attainable
file-only derivation, not a request for a whole vendor catalogue.

Check PE base relocations against every selected byte span. RIP-relative
displacements within an image are usually position independent; this must be
established for these exact spans. Reject an intersecting relocation unless the
source explicitly implements and prices its reviewed normalization. Do not
blindly compare IAT cells, absolute pointers, security cookies, mutable globals
or relocation-adjusted bytes to disk bytes. Map RVA/file offsets correctly.
[PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).

The FMA getter's instruction bytes may be matched, but its referenced dispatch
word remains mutable data. Associate its exact role/address with the separately
proved startup/no-reachable-writer interval and actual getter result; do not put
the observed value into an immutable byte region. This design cannot replace that
interval proof. Likewise two math anchors do not automatically qualify fenv,
errno, std::get_new_handler, classification helpers or a compiler-local floor
wrapper. Name their separate supported source/module/codegen contracts where
the combined proof relies on them, rather than adding unrelated regions silently.

## 5. Keep qualification before baseline and use the actual status contract

The current NativeObserver::read invokes fenv functions, std::get_new_handler and
the FMA getter. It must not run before this matcher. Qualification preflight
uses only compile identity, integer operations, supported intrinsic MXCSR control
reads, the named Win32 queries and byte comparisons. It invokes no getter, fenv,
libm, stdio, allocator, restoration helper or user callback while their required
runtime association is unqualified. Taking their imported addresses is distinct
from invoking them. Kernel contracts are the ordinary platform boundary, not
recursively qualified math. Keep FP-control observations in the selected strict
fixed-source leaf rather than importing precise-mode environment assumptions.

No owner-settled contract in ADR-0085/0088/0089 requires admission preflight to
preserve errno/volatile FP status. The matcher may leave ordinary volatile API
effects before the native baseline and original Clock, just like other new
preflight. It must not claim those effects are absent or restored. This avoids
a circular need to call an unqualified status library to qualify that library.
There is no need to add LastError/errno/fenv save/restore operations here.

After a successful match, establish the qualified strict native snapshot/baseline
and perform original Clock. The represented status-restoration promise begins
after Clock at beginReplay and ends immediately before the real resolve. Preserve
that exact extra-classifier/replay scope, including its early exits; do not erase
legitimate Clock/resolve effects or inflate it to raw x87/all-native preservation.
Control modes remain unchanged throughout; ABI/default checks and the true
control interval proof remain separate from volatile status observations.

## 6. Funding, resources and attainable promotion

Run the matcher once per genuine captured request, inside NativeSession identity
before original Clock and under the already held same-origin legacyQuery_ source
reservation. Its fixed profile gives finite region/byte counts; each page walk
must make positive bounded progress. There is no retry, lazy second match, extra
Clock, N-sample scan, new origin or fourth semantic tariff. A refusal enters no
old builder and consumes no old attempt tariff. The subsequent actual builder
commit/invocation gap still obeys ADR-0089.

The variable region/page/byte loops, checks and early returns must remain in the
funded window-query/NativeSession source, with actual per-partition Site payments
and raw counters before every reached body. The strict native TU exposes only
private fixed-source kernel leaves for log-anchor module, getter-anchor module,
module information and page information (plus the required fixed intrinsic
control observation). Each leaf is separately sponsored by its caller Site and
returns a value-initialized trivial POD even on failure. It contains no variable
walk, callback meter, hidden bulk comparison or public unfunded entry.

The complete new source/census must include caller entry/arguments, first POD
writes, profile accesses, address/range checks, every API call/return, region and
byte loop conditions/bodies/increments/final false, compared bytes, failure/result
construction, status capture/restore, session state and all cleanup. Reference
profile rows instead of copying them. Count the exact Windows stack structs and
maximum live stack/storage; constant expected arrays must be accounted according
to the existing resource boundary rather than called free. An opaque kernel API
leaf supplies no measured zero-allocation, CPU-time or hardware evidence.

No numeric quote or fit follows from this design. Derive the actual held source
envelope from the immutable private profile's region/byte extents and all exact
new source partitions. Its maximum is reserved, never charged as actual. Consume
only reached partition work before its body; retain unused capacity as unused,
with no post-hoc reimbursement. M/L profile extents are data, not caller prices.
Combine this with the unchanged origin cap and all other actual/reserved work.
Extra match cost may cause a genuine funded resource refusal; it cannot borrow
an old semantic tariff, use an arbitrary bulk SDK fee or raise a cap.

For a positive-length profile with M regions and total L bytes, positive chunk
progress permits a conservative page-iteration bound no greater than L and a
byte-comparison bound no greater than L, plus the finite M region overhead.
Actual source may justify a tighter page bound; neither bound is actual consumed
work. This makes a fixed pre-Clock reservation structurally attainable without
native page-count guesses. The complete independently derived coefficient sum,
combined origin inequality and stack/storage bounds must still fit unchanged
caps before a definition is supplied. This design alone supplies no numeric fit.

Recommended next steps are finite: author a separate unbound R4 private matcher
and bootstrap ordering; derive the concrete code/table/relocation certificate and
constant profile representation; obtain whole engine/resource review and an
independent complete source census; then author the required new whole proof and
run only the authorized headless qualification. Exercise bounded extent/page/API/
byte-mismatch refusals mechanically before any unsafe read, and successful real
module association and the existing post-Clock replay restoration through a
separately identified console program.
Do not deliberately dereference inaccessible memory or force traps/OOM.

Keep NativeBinding and prices undefined until all17 facts, actual combined source/
codegen identities, complete funding and the relevant profile proofs close.
This design acceptance is permission to prepare concrete source for review,
not a native PASS, prior/future module association, renderer/Ready certificate
or completion of Musical tempo-follow.
