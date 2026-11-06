# Controller environment N: source closure and exact external profile obligation

**Conditional source theorem, not a measured snapshot or unconditional N binding.**
The complete pinned PowerShell C# inventory has1,245 files/30,347,244 bytes.
All files were rehashed before the ordinary source/integer checker ran. It found
13 direct Environment.SetEnvironmentVariable source sites in nine files. No
current host environment, registry value or private user configuration was read.
No controller, parser, import, target compiler, stage or native probe ran.

The prior14-key seed is authored public-directory/task-path data at proposal SHA
3294595F71EDB97130703808C6CB9B6136DAF56716C93E1580CC7DC2CB8E126E.
It contains820 key/value UTF16 units and a234-unit PSModulePath value.

## The missing startup writer

Windows AutomationEngine.cs44..62 adds PATHEXT=.CPL when PATHEXT is absent.
It is absent from the seed. The resulting value satisfies the existing .CPL
test, so repeated engine creation does not append another extension. The prior
conditional14-entry calculation omitted this source writer; it must not be used
as a complete post-startup value. The seed/proposal itself remains unchanged.

ConsoleHost.cs121..145 prepends the92-unit actual application directory plus
semicolon to PATH when absent. This gives a93-unit growth for the exact proposed
seed. The original30-unit PATH then has123 units. It is not a user/machine PATH
observation. Direct Process.Start children do not enter NativeCommandProcessor's
package-manager PATH updater; that updater is only selected by its native-command
Complete path and _isPackageManager guard.

ModuleIntrinsics.SetModulePath may consult known folders and user/machine/settings
values, append entries, and write PSModulePath. This packet does not assume
idempotence, a particular Personal folder, absent user settings, or P=234. It
bounds the resulting *process value* P instead of inspecting private settings.

## Exhaustive identified PowerShell C# writers

| Source family | Identified sites | Relevant source restriction |
|---|---:|---|
| powershell/Program |1| Login-shell deletion is under UNIX; excluded by the supported Windows build. |
| ConsoleHost |2| Two mutually exclusive branches update the same existing PATH key. |
| AutomationEngine |2| Absent PATHEXT becomes .CPL; later checks are idempotent for that value. |
| SecuritySupport |1| Process execution-policy setter; absent exact argv option and unset stock ISS policy flag exclude it. |
| EnvironmentProvider |3| Set/remove provider operations; no env-provider write exists in the closed runner graph. |
| InitialSessionState |1| Binds configured entries; CreateDefault2's environment collection is empty, and Clone preserves it. |
| ModuleIntrinsics |1| Writes only the existing PSModulePath key; actual value remains P. |
| NativeCommandProcessor |1| Package-manager completion updater; no native-command pipeline in the runner. |
| remoting/WSManPlugin |1| Removes a remoting token variable; no remoting/session configuration path is selected. |

CreateDefault2 imports the core snap-in, then clones its collections. The default
_wasExecutionPolicySet flag is false; Clone uses the ExecutionPolicy setter only
when that flag was already set. Bind likewise tests the flag. The exact14 argv
entries exclude ExecutionPolicy, SettingsFile and ConfigurationFile overrides.
Startup Bind uses Parallel.Invoke and waits for its actions; module-update Bind
uses serial actions. This is not an assumption that startup work continues after
entry or that an endpoint snapshot excludes concurrent writers.

The complete C# source has no implementing IModuleAssemblyInitializer class;
the named interface and reflection loader are present. The two selected module
manifests name only their pinned DLLs and have no startup-script/type/format
payload fields. This is a source-level restriction. Exact binary/inheritance,
generated-source, native/dependency and loader association remains part of the
external supported-profile obligation below; the text inventory is not an IL
or compiled-binary equivalence certificate.

The authored telemetry opt-out1 selects the early return before telemetry client
initialization. Update-check Off selects the disabled notification arm. No
independent task callback/environment writer is introduced by the runner's
read/cancel/settlement helpers under their selected ordinary SDK contracts.

## Finite conditional sizes at each of three boundaries

Under the external profile obligation, valid unique environment keys are exactly
the14 seed keys plus PATHEXT. Case changes preserve their lengths. Let P_i be the
actual PSModulePath UTF16 value length at boundary i. Then:

- Eentry_i = Eunique_valid_keys_i =15.
- EUTF16_i =690+P_i, including both key and value units.
- Bnative_i =2*(721+P_i), including each equals/NUL and final block NUL.

At P=234 these give924 units and1,910 bytes. They are source arithmetic, not an
observed post-startup state. Microsoft's [GetEnvironmentVariable contract](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getenvironmentvariable)
limits a variable including its terminator to32,767 characters. Thus a successfully
stored process value has P<=32,766 under that supported Windows contract, yielding
EUTF16<=33,456 and Bnative<=66,974. The [setter](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-setenvironmentvariable)
and selected CoreLib setter translate failure rather than installing an oversized
value. A failed later write leaves a prior valid value; no small-settings or rate
whitelist is needed. These are per-capture inputs; they do not subtract any prior
Hashtable/string/dictionary/native-block request or assert pool reclamation.

The primary [CreateProcessW contract](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
does not automatically propagate per-drive environment entries when the caller
supplies its own block. The proposed launcher must actually supply that explicit
valid block, without inherited hidden/duplicate entries. The selected CoreLib
getter skips hidden entries while still copying the native block, so a bound on
only returned dictionary entries would otherwise be insufficient.

## External profile obligation and remaining work

Before selecting this N bound, the trusted pre-startup launcher must establish:

1. Exact direct Windows apphost, exact14 argv entries, supplied14-key unique valid
   Unicode environment block; no inherited drive entries, startup hooks/profilers,
   custom ISS, profiles, PSReadLine, settings/configuration overrides or injected
   loader/shim callbacks outside the supported runtime model.
2. Actual pinned host/CoreCLR/PowerShell/module/dependency association with the
   audited stock source and SDK behavior. This packet does not establish native
   apphost/CoreCLR startup environment-writer closure or generated/inherited
   initializer behavior merely from the C# scan or version strings.
3. After entry/imports, only the closed runner/ordinary managed SDK graph and
   direct child processes execute; no policy/provider/remoting/package-manager or
   other concurrent environment writer. Child execution does not mutate the
   parent's environment under the ordinary Windows process model.
4. The supported Windows getter/setter limit and valid-block premise, with no
   hidden/duplicate/malformed entries beyond the authored/setter-generated block.

If those premises cannot be established, Eentry/Eunique/EUTF16/Bnative remain
explicit external profile inputs. They cannot be manufactured from the outgoing
eight-key child dictionary, an inherited desktop snapshot or a convenient cap.
The predicate above is the precise outstanding obligation; no actual fresh-launch
profile, native-startup theorem, numerical N closure,32MiB fit, binding or GO is
claimed here. Independent source review is pending.

Primary source commit is ea554a8f9c6085f54fb2828f7ea286c65c35599f. The complete
URL/file/hash provenance and13 exact writer lines are in source-origins.json and
writer-closure.json. Third-party source text is retained locally for review, not
newly owned code. The checker source and its complete first passing output are
frozen; no accepted ADR, old source/seed, cap or evidence was rewritten.
