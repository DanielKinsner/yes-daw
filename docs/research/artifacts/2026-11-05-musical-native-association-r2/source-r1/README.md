# Native module association source, revision 1

Source-first and UNRUN. Independent source safety review precedes compilation
and execution. This separate purpose does not edit or extend the frozen finite
Clock probe, old engine headers, accepted ADRs, other proofs or production code.

The Windows x64 console program takes six imported CRT function addresses and
uses GetModuleHandleExW(FROM_ADDRESS, UNCHANGED_REFCOUNT) and a bounded module-path
buffer to report each actual loaded host path and RVA. It reads the documented
FMA3 getter once. It invokes no math operation, FMA3 setter, FP-control/status
setter, GUI, device, thread, child process, external data or environment reader.
There are no dynamic global constructors or throwing user operations. Every
ordinary failure prints one bounded diagnostic and exits1. Inventory/API/extent
checks are self-asserting; success exits0. Root must redirect all output and run
only a verified AMD64 CONSOLE image with a timeout and per-process no-error-box
mode in the serialized native lane.

Use the pinned actual project MSVC toolset, C++20 /O2 /Ob2 /MD /DNDEBUG /EHsc
/W4 /WX, no LTO, and /SUBSYSTEM:CONSOLE. Root records exact compiler/executable
identities and commands, pre/post candidate CRT file hashes, actual stdout and
raw exits. The whole source freezes at its first green, including this README.

This associates modules only for this new console executable and that run. It
cannot retrospectively identify modules from a previous probe, bind future R3
callers, prove a stable FMA3 dispatch interval or all-input native equivalence,
price source actions, admit a recipe or grant DSP/Ready/hardware credit.

Primary API documentation:

- https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw
- https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew
- https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/get-fma3-enable-set-fma3-enable?view=msvc-170
