# Clock R22 process and handle owners

Run `python -E -B check.py` with exact R22 staged. It pins the
manifest, runner, five native sources and selected installed startup
sources, then checks source incidences and arithmetic. The
[ABI results](../2026-11-14-clock-supervisor-abi/result.json)
and [attribute result](../2026-11-14-clock-attribute-size/result.json)
remain separate inputs. This work used file and source inspection.

| Phase | Maximum established helper-created handle references |
| --- | ---: |
| Suspended target returned, before the three parent stdio closes | 11 |
| Both workers created after the three parent stdio closes | 16 |
| Workers and one overlapping stage search | 17 |
| Terminal file and one search, retaining prior failed closes | 18 |

The request file stays open after capture in `sup_io.cpp:102–130`.
The current-identity hash file closes before successful setup and
refuses setup if its close fails. The common set includes the request
file, two logs, two pipe readers, the job, and the returned process
and primary-thread references. The three temporary parent stdio
references are two pipe writers and NUL. A successful child receives
three separately counted references to those same objects. Each
worker can add three unnamed events and one thread reference.
`sup_io.cpp:169–222` permits only one directory search at a time.
Terminal publication can begin with prior owners retained after
cleanup. The figures describe established references and supply no
physical close credit or count for unknown ownership behind a failed
creator's raw output.

The 24-byte `HANDLE_LIST` and 8-byte `JOB_LIST` value arrays are already
inside `State` and persist until attribute deletion. There is one
72-byte caller-request candidate from the host sizing observation.
`HeapSize` would report the requested block size and leave heap
metadata and backing open. The managed controller launch has a separate
inheritable-handle obligation because its selected stock `Process.Start`
path enables inheritance without an explicit handle list.
[Creation attributes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute),
[HeapSize](https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapsize)
and [controller launch owners](../../2026-11-14-clock-r22-controller-transport-resources.md)

## Supplied process inputs

The recipe admits `working/transport/<20-unit-terminal-name>`
within 512 UTF-16 units. Working plus any required separator is at
most 482 units. The native executable path is therefore at most 503
units and the data argument at most 486. The executable-path write
rejects embedded quotes and covers the same working prefix as the
data argument. Its `data` suffix prevents trailing backslash doubling.
The six arguments are mode (7), quoted data path (488), manifest (64)
and three decimals (19 each). Six separators, executable quotes and
final NUL bring the command to **1,128 units or 2,256 bytes**.

The helper starts before the terminal-path recipe runs. The controller
checks the request path against 512 units before `Process.Start`.
That path uses the 19-unit request filename, so the same working
prefix can use 483 units during helper startup. This covers startup
even if the helper later refuses the request.

The eight environment names total 43 units and the six fixed values
total 497. Each `opaque-temp` value can use 494 units for the helper
and 493 for a target admitted by the terminal-path recipe. Eight
equals signs, eight entry terminators and the final NUL give **1,545
units or 3,090 bytes** for the helper and **1,543 units or 3,086
bytes** for the target. These are the supplied logical input extents.

## Conditional CRT startup requests

The installed UCRT `env/get_environment_from_os.cpp` and
`env/environment_initialization.cpp` show one converted block, a
nine-pointer table and eight individual terminated strings per
direct initialization. The DLL initializer in
`internal/initialization.cpp` can initialize narrow variables before
`wmainCRTStartup` selects wide variables. Count both request histories
for the helper. The selected narrow target path uses the previously
initialized narrow table if one exists.

| Explicit CRT payload requests | Bytes |
| --- | ---: |
| Helper narrow environment using 1,545 units | 3,161 |
| Helper wide environment using 1,545 units | 6,250 |
| Both successful direct helper initializations | **9,411** |
| Native target narrow environment using 1,543 units | **3,157** |
| Wide unexpanded helper argv | 5,172 |
| Narrow unexpanded target argv | 1,188 |

The helper argv request includes a 512-unit executable token, a
2,060-unit argument, their two terminators and three pointers. The
target argv request uses the controller's admitted ASCII working
prefix and target filename, seven terminated arguments and eight
pointers. The one-byte narrow equations require the admitted ASCII
domain and a matching code page.

The installed MSVC `exe_common.inl` can call the wide initializer
before C++ startup and later call `_get_initial_wide_environment`
when invoking `wmain`. An unsuccessful first wide initialization can
leave a second direct attempt and then the narrow-to-wide clone.
`env/environment_initialization.cpp` invokes the clone setter with
`is_top_level_call` zero, so the eight nonempty, unique names append
without recursion or OS setters. `env/setenv.cpp` can request the
initial pointer cell and successively two through nine cells. A
failed initial table allocation costs one cell and reduces the
remaining append opportunities. Including all grown tables gives a
360-byte table-request upper. Add 3,088 for the eight converted clone
strings. Counting both failed direct wide requests at full widths
and retaining all request histories gives **19,109 helper bytes** in
this fallback envelope, including the initial narrow request.

Each selected direct initialization can also use an OS
`GetEnvironmentStringsW` block. The successful helper dual-width
path permits two logical 3,090-byte extents and the retry path permits
three. The target's single narrow initialization has one separate
logical 3,086-byte extent. Neither the Windows capacity of those
blocks nor reclaimed bytes follow from these extents.

`startup/argv_data.cpp` assigns both `GetCommandLineA` and
`GetCommandLineW` pointers before entry. They give two system-managed
representations, including for `wmain`. Under the exact ASCII
command profile their logical extents are up to 2,576 narrow bytes and
5,152 wide bytes for the helper, and 1,128 narrow bytes and 2,256 wide
bytes for the native target. The runtime backing, relationship to
existing process parameters and loaded implementation remain open.
The formulas depend on the final entry, default unexpanded argv,
actual raw contents, conversion, source-selected CRT paths and
loaded runtime association.
[CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw),
[GetCommandLineA](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getcommandlinea)
and [GetCommandLineW](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getcommandlinew)

## Stack and host preflight

The [pinned controller PE](../2026-11-14-clock-controller-startup/README.md#pinned-apphost-pe)
requests 1,572,864 reserve bytes and 4,096 initial commit bytes.
Zero-size worker creation uses the helper PE defaults. Count three
source-directed helper stacks including the initial thread, and one
explicit initial native target stack. Read their final PE fields as
data once linked. MSVC documents 1 MiB reserve and 4 KiB commit
defaults when the link uses them. Runtime threads, commitment,
per-thread backing and settlement remain separate. COFF frames are
contained in their corresponding stacks. [MSVC /STACK](https://learn.microsoft.com/en-us/cpp/build/reference/stack-stack-allocations)
and [Win32 thread stack size](https://learn.microsoft.com/en-us/windows/win32/procthread/thread-stack-size)

The reviewed [launcher probe](observe-launch-context.py) queried the
current headless Python process using `IsProcessInJob` and
`GetSystemInfo`. The [saved result](launch-context.json) reports a
4,096-byte page, 65,536-byte reservation granularity and membership in
an existing job. The immediate job returned limit flags `0x2800` and
UI restriction flags zero. This leaves ancestor limits, continuity
to actual launch and creator-death behavior open. A future exact
launch must account for nested jobs. The observation created no job or
target process. [Membership query](https://learn.microsoft.com/en-us/windows/win32/api/jobapi/nf-jobapi-isprocessinjob),
[job information](https://learn.microsoft.com/en-us/windows/win32/api/jobapi2/nf-jobapi2-queryinformationjobobject)
and [nested jobs](https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs)
