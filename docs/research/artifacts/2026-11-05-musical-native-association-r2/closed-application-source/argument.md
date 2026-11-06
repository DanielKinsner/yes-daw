# Closed-application FMA3 source argument, revision 1

The current application/dependency source contains no FMA3-selection writer or
new-handler setter. The complete pinned conservative source corpus supports the
application side of a stable-selection argument across all its threads. Runtime
startup ordering is supported by the installed CRT source and Microsoft contract.
The remaining qualification is concrete: associate the actual selected allocator/
MSVCP/UCRT bodies and eventual combined R3 binary with that source contract. This
is not an unbounded hypothetical-interference problem, and it does not require a
new lock, setter, architecture, rate/map restriction or user test.

This report does not define NativeBinding or a price. The original numerical
argument and all frozen sources/results are unchanged. `source-corpus.json`
contains exact file hashes, the actual build commands and every search match.

## Actual application source boundary

The current CMake target links25 application/JUCE translation units plus two
static dependency translation units: SQLite amalgamation and libebur128. Its
application sources are Main.cpp and12 UI implementation files. The remaining
12 TUs implement the selected JUCE audio_devices/audio_basics/core/events/
audio_formats/gui_extra/gui_basics/graphics/data_structures modules and their
split compilation files. There is no audio_processors/plugin-host module.

The source search covers3,125 C/C++/header/inline/assembly/Objective-C files across
`src`, generated YesDaw JuceLibraryCode, all JUCE modules, choc, Signalsmith,
SQLite and libebur128 source roots. It intentionally includes inactive platforms,
plugin modules and examples as a conservative superset. The25+2 actual TU
commands are separately retained; this is not a claim that all3,125 files are
preprocessed or compiled. No target preprocessor/parser was run.

Across that entire corpus there are zero matches for `_set_FMA3_enable`,
`__acrt_initialize_fma3`, the identified dispatch-word addresses, `set_new_handler`
or `_set_new_handler`. The nine broader FMA3/ISA matches are JUCE CPU-capability
fields/readers, not writes to CRT math configuration. The only definition of
JUCE_ENABLE_ALLOCATION_HOOKS is its default0. Actual compile commands and generated
configuration supply no enabling override. Thus JUCE's conditional allocation
listener/global-new replacement is excluded. Signalsmith's command-line memory
tracker replacement appears only in a nonlinked example, not the admitted target.

Important option distinction: actual GUI target commands contain `/O2 /Ob2` and
later JUCE `/Ox /MP`, `/MD`, C++20 and `/W4 /WX`. The existing pure FP probe did
not have this full GUI context. Its out-of-line helper disassembly cannot bind
future R3/application callsites. Default precise behavior does not make all
optimized contexts byte-identical.

## Reachable loading and callbacks

The application source has one direct GetProcAddress site for the fixed
`NtCreateFile` symbol from ntdll. JUCE's selected Windows dynamic imports name
OS facilities such as DirectSound/AVRT, DirectWrite, DPI/accessibility/WinRT,
Windows Media, version/network/shell functions. None names a CRT FMA setter.
The generic DynamicLibrary implementation exists, so its existence alone must
not be relabeled a loader whitelist; its selected callsites and configured-out
plugin/browser paths matter. Source/CMake excludes arbitrary hosted plugin code:
audio_processors is not linked; JUCE_ASIO defaults0 with no command override;
JUCE_WEB_BROWSER=0 and JUCE_USE_CURL=0; SQLite has SQLITE_OMIT_LOAD_EXTENSION.
choc's generic MemoryDLL and Signalsmith command tools are not app TUs.

No callback supplied by these application/dependency sources contains a named
FMA writer. Searching all their source, rather than only the worker, matters:
the selection is CRT global state and another app thread could otherwise change
it. The source evidence does not promise arbitrary public utility callbacks can
never be used differently by a future source revision. Its exact corpus/build
identity must be preserved when closing the combined caller.

Installed default scalar-new source calls malloc, then `_callnewh` on failure.
Installed UCRT new-handler storage initializes to encoded null and its public
setter is the source-visible mutation route. Installed MSVC `stdhndlr.cpp`
zero-initializes its C++ handler and invokes the C setter only from its public
`std::set_new_handler` implementation; get_new_handler only reads under its mutex.
With completed startup and no reachable setter in the closed admitted source,
the handler remains null, so this default allocation path invokes no application
new-handler callback before original Clock prefix computation. A plain null
snapshot alone would not establish that fact.

This allocator/source correspondence still needs the selected runtime association
below. Alternatively, any genuinely admitted nonnull callback needs its actual
source proof: controls restored across ABI return and no FMA-selection writer.
The Windows x64 ABI protects control registers, not the global FMA3 word. ABI
conformance alone cannot prove FMA-selection stability. No unsupported injected
module or ABI-violating behavior is included as an ordinary valid caller here.

## Startup precedes captured-request work

Main.cpp enters through START_JUCE_APPLICATION. The selected JUCE macro creates
WinMain, which sets the application factory then enters JUCEApplicationBase::main.
UI model construction and the existing StretchPreparationJob/AudioImportJob
std::thread starts occur later. Those jobs currently implement fixed preparation;
there is no integrated MusicalPreparationRequestState/FundedLegacyStageAdapter
in application source yet. Future R3 integration must retain this ordering.

Installed CRT exe_common.inl initializes the CRT, runs C/C++ initializers and TLS
initialization, then invokes user WinMain/main. Installed UCRT appcrt_dllmain.cpp
performs process initialization on DLL_PROCESS_ATTACH and uses separate thread
attach/detach paths. Its initialization.cpp calls initialize_c, which executes
the C initializer array before the C++ initializer array. fma3_initializer.cpp
places the FMA initializer in `.CRT$XIC`. This gives the source ordering: UCRT
process startup selection completes before ordinary captured-request work; a
new worker thread is not a repeat of process FMA startup.

Microsoft documents [CRT initialization before user main](https://learn.microsoft.com/en-us/cpp/c-runtime-library/crt-initialization?view=msvc-170)
and [load-time DLL initialization](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-entry-point-function).
Its [FMA3 contract](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/get-fma3-enable-set-fma3-enable?view=msvc-170)
identifies startup CPU selection, the getter and the setter, and permits numeric
differences between the selected algorithms. This is a supported startup/source
argument; installed source version labels are not proof of byte-for-byte identity
with every patched runtime DLL.

## Existing binary and new console association

Root inspected the existing YesDaw.exe, SHA-256
`A16156B5C5D4ACB1618DBF5557B09C57FDD55856E65EAD4AF704BC74D62072B7`,
without launching it. It names MSVCP140.dll, VCRUNTIME140.dll and VCRUNTIME140_1.dll.
It has no direct FMA3 or new-handler setter import; it does import `_callnewh`,
malloc/free and GetProcAddress. Those are exact import-table facts, not a complete
indirect-call proof or an exact current-source build receipt.

Root's named System32 runtime candidates have pinned hashes and no app-local
same-named candidate. None imports a FMA3 setter. MSVCP140 **does** import
`_set_new_handler` and GetProcAddress. The first is consistent with stdhndlr.cpp's
public API implementation; it is not evidence that startup or a heap call invokes
that setter. Conversely, absence of a setter import cannot exclude a private
same-module write or an indirect resolver call. This is the precise remaining
MSVCP/allocator association boundary, not a newly invented callback exemption.

The separate reviewed module-association console actually ran0 with FMA3=1 and
associated log1p/get_FMA3_enable/fenv addresses with the pinned current System32
UCRT. Its floor address was an executable-local wrapper/thunk at RVA0x2158.
The output truthfully reports the containing module for that address; it does
not establish direct UCRT floor association. These facts apply only to that new
console executable/run. They do not retrospectively load-associate the old probe,
the existing GUI image, or the unfinished R3 binary.

## Conditional interval theorem and remaining exact work

Let the admitted process have the reviewed source/build identity, completed
qualified CRT startup, and the selected runtime behavior just described. Let
all application callbacks/threads during original Clock build through replay/
resolve be drawn from this closed source and ordinary admitted OS/runtime API
contracts. If the selected runtime has no additional post-startup writer reachable
through these allocator/standard-library calls, then no transition in that
interval writes the FMA3 selection. Its getter-observed initial selection is
therefore invariant, without needing to exclude ABA by repeated sampling.
This supplies the missing kind of premise for the earlier prefix induction.

The source half is now concrete and pinned. Remaining work is:

1. Seal/review actual R3 and its actual combined worker/control caller; identify
   its emitted sites under the real target options and preserve captured immutable
   map/rate/history and worker startup order. R3 outside the current build graph
   is not qualified by the current25 TU list.
2. Associate selected allocator and MSVCP handler/getter implementations with the
   reviewed source/API behavior, rather than importing all patched runtime
   identity from a14.44 installation folder. Confirm the actual combined native
   binary's imports/source identity. Current System32 candidates remain file
   evidence until that binary records actual module resolution.
3. Keep the previously identified UCRT log/getter/dispatch and strict restoration
   qualifications bound to that same combined binary. Equal snapshots remain
   rejection checks, not replacement interval proofs. Source prices/funding and
   all resource/renderer/Ready obligations remain separate.

The next useful headless step is a file-only association of the selected linked
scalar-new and MSVCP handler/getter ranges to their pinned source contracts, plus
whole R3/caller review. A later separately reviewed console combined qualification
can record actual module paths/addresses and restoration outcomes without GUI or
audio startup. No broader vendor catalogue, new settings API or Dan-operated
test is required. This report supplies source evidence and a conditional theorem,
not a completed NativeBinding or permission to execute new funded query source.
