# Clock controller host startup observation

The independently reviewed [launcher](observe.py), SHA-256
`E703C312632943A946F7CD2CB59AD296C7F44EAED6F02439989AC62D7A8C7569`,
started the recovered PowerShell 7.6.5 once with an explicit 14-key
environment, fixed headless switches, null stdin, bounded capture
and a separate observation script. The launcher exited 0. Its
preflight verified 323 files and four closed directories. The
script verified the loaded host, core assemblies, stock module and
cmdlet paths. The launcher checked the file pins again after exit.

The [observed managed snapshot](result.json) after the Management
and Utility imports had 15 entries, 155 key UTF-16 units and 751
value units. All entries matched the report's known names. The
supplied seed had 764 key/value units and a 1,586-byte logical
environment block. `PATH` and `PSModulePath` measured 99 and 278
units at the endpoint, and `PATHEXT` occupied a new managed slot
with four value units. No values were saved in the receipt.

The launch substituted the observation script and private directory
for the actual Clock script and run root. The result identifies one
endpoint for this seed. A future launch still needs a proof for
native hidden entries, intervening writers, captures before each
child, allocator backing and inherited handle behavior. No Clock,
supervisor, compiler or target was entered.

To repeat the file-only preflight, pass `--check` with the same
pinned interpreter and the fixed absent cache prefix required at the
top of the launcher. The live observation already consumed its
attempt directory.

## Conditional allocation join

Run `python -E -B check-shape.py` to check the selected array and
string requests. If each raw capture has exactly the 15 distinct,
well-formed measured rows and the pinned runtime uses the reviewed
CoreLib source and x64 layouts, each capture accounts for 2,584
string bytes, 1,632 Hashtable bucket bytes over four capacities,
and 528 bytes in the capacity-17 Dictionary's two arrays. The
wrapper copies references. Clearing the dictionary retains its
capacity and its subsequent eight entries fit. All old arrays
remain in the sum, giving 4,744 bytes per capture and 14,232
across three. Object shells, native blocks, outgoing encoding,
PowerShell wrappers and general runtime backing have other terms.

`Environment.GetEnvironmentVariables` can allocate strings before
dropping duplicate keys, and skips hidden native rows entirely.
The snapshot therefore does not determine raw allocation counts
or native block capacity. A complete startup and writer profile
must establish the raw shape across the actual capture intervals.

## Pinned apphost PE

Run `python -E -B check-host-pe.py` with the staged R22 manifest
and recovered apphost. The checker hashes `pwsh.exe` before reading
its AMD64 PE32+ console header. The stack fields request 1,572,864
bytes reserved and 4,096 initially committed. The heap fields request
1,048,576 reserved and 4,096 initially committed. These fields supply
the apphost's default image parameters. Runtime threads, explicit
per-thread settings, managed heaps and other committed backing need
their own ownership and lifetime evidence.
[Microsoft PE format reference](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#optional-header-windows-specific-fields-image-only)
