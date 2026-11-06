# Independent controller-environment R2 source review

**ACCEPT the conditional source equations and the explicit external-profile obligation. Actual N qualification remains HOLD.** This is not an unconditional15-entry environment theorem, a boundary snapshot, resource fit, immutable binding or execution GO.

Subject pins SHA256 `94D3B05DFA28ABFD616E2AC0E7EBDBA4E950D172BFA2FE37D20F6376FBAA034A` and writer-closure JSON `F43C234BF32F929BFF0076813BF1D4A51704E9626010B1E740433E2AA8191F87` match. Independent file/text/integer checks pass1,293 checks and retain1,257 verified input identities. All1,245 pinned official C# files/30,347,244 bytes match the ea554a8f9c6085f54fb2828f7ea286c65c35599f source inventory. No author checker, target code or environment API was executed.

## Conditional arithmetic and platform contract

The authored14-key seed is unique case-insensitively, contains820 key/value UTF16 units and a234-unit PSModulePath, and omits PATHEXT. Under the actual supported Windows source/profile, AutomationEngine44..62 writes PATHEXT=.CPL. Its own .CPL test makes that value idempotent across later engine creation. This contributes11 key/value units and one entry. The prior conditional14-entry post-startup calculation is incomplete and must not be used as a complete actual value; its sealed source remains untouched.

ConsoleHost121..145, under the selected direct apphost/application-base association, prepends the92-unit runtime directory and semicolon to the original30-unit PATH: growth93, resulting value123. The two writes are mutually exclusive and the prefix test prevents another identical front prefix in this direct startup. This does not infer a current user/machine PATH value or qualify a different tool/apphost location.

Removing the seed's234 PSModulePath value units leaves:

`820 +93 +11 -234 =690` fixed key/value units.

Thus, **only under the complete external profile**, for each capture boundary i:

- Eentry_i = Eunique_valid_keys_i =15.
- EUTF16_i =690+P_i, where P_i is actual process PSModulePath value length in UTF16 units at that boundary.
- Logical Unicode native-block units = EUTF16_i+2*15+1 =721+P_i; bytes =2*(721+P_i).

All four subject arithmetic examples match. P=234 gives924 units/1,910 bytes; the P=0/1 rows are algebraic illustrations, not source attainability or observed post-startup values.

The [Microsoft GetEnvironmentVariable contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getenvironmentvariable) explicitly includes the terminating NUL in the32,767-character maximum. For the supported wide-character process contract this supports P_i<=32,766, EUTF16_i<=33,456 and logical block bytes<=66,974. The [setter documentation](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setenvironmentvariable) supplies the variable limit/failure contract, and the pinned CoreLib setter translates a false native return into the relevant managed failure. No oversized successful value is inferred. Native failure atomicity/valid-block behavior remains part of the supported SDK/profile premise, not something established by a C# text scan.

The Unicode block calculation is not an ANSI32,767-total-character bound: [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw) distinguishes the ANSI total restriction and requires CREATE_UNICODE_ENVIRONMENT for a supplied Unicode block. That same contract says a supplied block does not automatically inherit per-drive entries. The eventual launcher must actually supply that block and flags. Logical content/terminator bytes are not a certificate of native allocator capacity or total heap footprint.

## Whole writer and exclusion argument

An independent whitespace-tolerant whole-text scan reproduces every13 direct Environment.SetEnvironmentVariable call in the same nine files. Source context confirms:

- Program's login deletion is UNIX-only. A wider scan also finds the Program POSIX SetEnv/setenv P/Invoke/call, wholly inside its UNIX region; it is not silently claimed absent from all source or relevant on the selected Windows build.
- ConsoleHost's two PATH branches and AutomationEngine's two PATHEXT branches have the restrictions above.
- ModuleIntrinsics1359..1379 may read user/machine/configuration/known-folder values and replace the existing PSModulePath key with a nonempty process value. P_i stays unknown; no registry equality, path idempotence or private configuration absence is inferred.
- SecuritySupport's process-policy writer requires its selected process-policy call. The stock unset ISS flag, guarded Clone/Bind behavior and absence of an argv policy option exclude it only under the specified unmodified default-host/configuration premise.
- EnvironmentProvider's three sites are actual set/remove operations. The exact closed R12 graph has no process setter or env-drive assignment; ProcessStartInfo.Environment.Clear/index assignments change the outgoing child dictionary, not the parent's environment. No generic provider/native-command/remoting pathway is admitted by this source profile.
- NativeCommandProcessor's PATH update is reached from Complete under _isPackageManager. The runner's direct Process.Start calls do not invoke that PowerShell native-command pipeline.
- WSMan's deletion belongs to its remoting token path, not the specified direct console host.

ManagedEntrance94 selects CreateDefault2 and passes issProvidedExternally:false. The protected ISS constructor is empty; the environment collection is lazily empty; CreateDefault2 imports the core snap-in and clones it; Clone copies the environment collection and conditionally sets policy only if _wasExecutionPolicySet was set. Bind_SetEnvironment only iterates that collection. Custom/inherited/generated/externally supplied state or configuration must still be excluded in the real supported profile. NoProfile by itself is not a proof of that whole condition.

Both exact pinned Management/Utility manifests have only their DLL NestedModules loader field, with no ScriptsToProcess/type/format/required module/assembly loader payload fields. The source inventory contains the initializer interface and reflective loader but no class declaring its implementation. This is source-level evidence, not a proof that no actual loaded type inherits an initializer or has generated/native/module/static initialization behavior. The packet correctly leaves binary/inheritance/generated/dependency/loader association external.

Telemetry opt-out1 selects the source early return before TelemetryClient construction. Update-check Off selects the disabled notification arm. These are specific source branches; they do not qualify arbitrary native/dependency initialization or every host background action.

## Concrete unclosed concurrency edge

The Bind argument must remain narrow. `Parallel.Invoke(actions)` at InitialSessionState2181 joins those six Bind actions. **It does not join all startup work.** ManagedEntrance67 separately calls EarlyStartup.Init; InitialSessionState29..69 starts three unawaited Task.Run lanes:

1. Windows AMSI warm-up.
2. Compiler/CachedReflectionInfo/ExpressionCache type initialization.
3. Resources, LanguagePrimitives and TypeAccelerators initialization/assembly loading.

The warm-up enters CheckAmsiInit before returning at SecuritySupport1438..1447; it can call AmsiInitialize/AmsiOpenSession. Those native/dependency/provider callbacks are not qualified merely because the pinned C# inventory has no extra direct setter. The theorem's existing no-other-writer/native/dependency/concurrency premises must explicitly include completion or noninterference of these lanes throughout all three capture intervals. This is a named outstanding external premise, not a counterexample to the conditional equations and not a new runtime mechanism. No private AMSI configuration or provider was inspected.

## What must be established before N is selected

The precise outstanding predicate is an actual exact direct Windows host/build/module/dependency launch, its14 specified argv entries, and an explicitly supplied unique valid14-key Unicode seed with no inherited hidden/duplicate/malformed entries, startup injection or custom ISS/profile/configuration augmentation. It must associate actual native apphost/CoreCLR/PowerShell/module and all relevant initialization/callback behavior with the supported source/SDK model, including the unjoined early-startup lanes above. Only the identified PATH/PATHEXT/PSModulePath changes may affect the seed through the captures; all other process/SDK/provider/remoting/policy/package-manager/concurrent writers must be absent or proved noninterfering.

The native valid-block/no-hidden-entry requirement is essential. CoreLib first obtains the native environment block and only then skips hidden/malformed entries when forming its Hashtable; its returned dictionary count does not bound skipped native content. The outgoing eight child entries do not establish this predicate. Neither an inherited desktop snapshot nor equal before/after observations proves the interval.

If these actual external conditions are not established, Eentry_i/Eunique_i/EUTF16_i/Bnative_i remain explicit unqualified profile inputs. A future source-qualified launcher/profile can establish them for its own exact fresh launch; it cannot retrospectively certify a prior host or use endpoint measurements alone to exclude intervening writers. Any separate public qualification must be reviewed first and must account for native-block contents/hidden entries and the true writer interval, without requiring raw private values. No additional probe is run or authorized by this review.

Even if this conditional N is later selected, it only parameterizes these captures. It does not bound private configuration caches, total startup/native memory, dictionary/clone capacity or complete payload lifetime without the separate source/ABI/phase proof. No previous allocation is freed by Clear/Dispose/null/scope/GC assumptions. The three capture terms, whole numeric/resource proof, immutable binding and32MiB fit/GO remain held.

Only ordinary source/file/hash/integer reads and live official Microsoft documentation were used. No current environment enumeration/value, registry, private configuration, target parser/import/helper/compiler/program/stage, native probe, GUI/audio/device or Git operation occurred. No subject or prior evidence changed.
