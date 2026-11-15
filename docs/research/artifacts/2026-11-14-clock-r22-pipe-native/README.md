# Clock R22 queued pipe backing

The [data-only checker](check.py) matches the on-disk AMD64 NPFS
image 10.0.26100.4202 and its Microsoft public symbol file. It
validates the PE CodeView GUID and age, the corresponding PDB
GUID and DBI age, five public routine/import addresses, and the
instruction bytes and call displacements below. Image bytes,
symbols, and unload/load identity are separate. Binding this to a
running host still requires the loaded driver association.

| Driver RVA | Selected path |
| --- | --- |
| `12588`, `125a8`, `126cb` | `NpFsdWrite` loads the 32-bit request length into `r8d`, calls `NpCommonWrite`, and the latter saves it in `ebp`. |
| `128a0–128e9` | The applicable queued route supplies kind one, mode zero and the full length `L` as the sixth argument of `NpAddDataQueueEntry`. The IRP is a separate input. |
| `1319b`, `132eb` | Kind one selects `L + 48` as a checked unsigned request extent. |
| `131ac–131c3` | The quota quantity uses the minimum of remaining queue quota and remaining write bytes. It is a distinct value from the allocation extent. |
| `131ef`, `132ac–132bf` | A missing reusable queue cell can reach `ExAllocatePool2`. Its size argument is the computed extent, with flags `0x41` and tag `NpFr`. |

The verified route can request storage for the full write plus a
48-byte queue header, including when part of the write has already
been delivered to the reader. This is a pool request before allocator
rounding. Microsoft's public description also permits the pipe to
grow using process quota. A size returned by an empty-pipe query
therefore cannot close this part of the 32 MiB proof.
[Microsoft native pipe buffering](https://learn.microsoft.com/en-us/windows/win32/devnotes/nt-create-named-pipe-file)

R22 controls each helper read to 4,096 bytes and admits capture to the
log after reading. Those checks supply neither a compiler write-size
bound nor a limit on simultaneous compiler and descendant writes.
The two numerical targets have separately bounded authored diagnostics.
A target pipe term still needs CRT and loader writer correspondence.
For the compiler slot, qualify the writer extent, outstanding requests,
inherited handles and queue lifetime before assigning any numeric
backing upper. Finishing the controller arithmetic does not resolve
these native terms.

The checked symbol file is available from the
[Microsoft symbol server](https://msdl.microsoft.com/download/symbols/npfs.pdb/C433ACB0BA96C41BACA83308CFF250A61/npfs.pdb).
Place the file at `%LOCALAPPDATA%/Temp/YesDawClockR22PipeSymbols/npfs.download.pdb`
and run `python -I -S -B check.py`. Only the pinned on-disk files are
read. No pipe, driver, compiler, helper or Clock target is started.

## Authored target diagnostic requests

`python -I -S -B target-write.py` checks the pinned UCRT SDK
`write_text_ansi_nolock` body. Its cursor check leaves room for CR
and LF in the 5,120-byte translation array. Each selected
`WriteFile` receives the actual bytes between the buffer base
and that cursor. The source-controlled diagnostics are tighter.
The selected write can submit at most 1,064 producer stdout bytes,
2,662 verifier stdout bytes or 1,294 target stderr bytes. Adding
the verified 48-byte queue header gives 1,112, 2,710 or 1,342
requested bytes for that queue route, respectively.

This per-call bound requires the final target and loaded CRT to
select the reviewed ANSI branch for the authored diagnostics.
Queue populations, allocator overhead, loader and failure
writers, and the compiler's output retain their own proof terms.

`python -I -S -B ucrt-route.py` independently verifies the
matching limit in the pinned on-disk UCRT image. Place its
[Microsoft symbol file](https://msdl.microsoft.com/download/symbols/ucrtbase.pdb/66A3E8DFB8B886146544529E6A6DD4F41/ucrtbase.pdb)
beside the NPFS symbols. In the ANSI branch the cursor starts at
`RSP+0x40` and can add at most two bytes after its last admissible
position below `RSP+0x143f`. The resulting `WriteFile` request is
at most 5,120 bytes. The separate binary branch passes the
caller's count without applying this buffer limit. The image
check leaves loaded UCRT identity and actual descriptor modes
at the final launch boundary.

## Compiler writer scope

Run `python -I -S -B compiler-imports.py` with the exact R22
manifest and recovered compiler staged. The check reads selected
normal imports from eight pinned images. The compiler recipe includes
linking. Five candidate images import `WriteFile` and five import
`CreateProcessW`. The driver `cl.exe` and C++ front end `c1xx.dll`
also import `_setmode`. An upper confined to a CRT text
buffer needs to rule out direct writes to the inherited endpoints
and mode changes. Imports identify which paths to audit. They supply
neither a reached writer graph nor argument bounds.

The R22 job configures kill-on-close. Documented Windows job memory
limits concern committed virtual memory. The LSA `QUOTA_LIMITS`
nonpaged field is unenforced by that subsystem, and `ExAllocatePool2`
with use-quota charges an existing process quota. This audit has no
supported finite pool cap for the R22 compiler slot. For example, a
32 MiB write extent fits the selected NPFS unsigned size calculation
and would request 32 MiB plus 48 bytes on that branch. That is a
witness to the gap in the current proof assumptions. The import
check supplies no evidence that MSVC actually issues such a write.
[Windows job memory limits](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_extended_limit_information)
[LSA quota fields](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-quota_limits)
[Pool allocation flags](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/pool_flags)

## Selected MSVC driver routes

`python -I -S -B compiler-routes.py` checks the same pinned
`cl.exe` with its
[Microsoft public symbols](https://msdl.microsoft.com/download/symbols/cl.pdb/EDA606CB8B25479089F34B8A52205A2A1/cl.pdb).
Place `cl.pdb` beside the downloaded NPFS symbol file. The check
associates symbols with exact instructions and IAT references.

- In the driver diagnostic helper, the descriptor 1/2 route reads
  `VS_UNICODE_OUTPUT`. A nonzero result can select the direct
  `WriteFile` call, whose count is twice the NUL-terminated UTF-16
  input length in its 32-bit argument. The initial R22 seed excludes
  this variable. Discharging that branch requires the variable to
  stay absent through the helper's read. The other observed route
  converts the input and calls CRT `_write`.
- The driver's `start_server` copies all three `GetStdHandle`
  results into `STARTUPINFO`, sets `STARTF_USESTDHANDLES`, and calls
  `CreateProcessW` with handle inheritance enabled. The exact
  compiler mode and reachable server call sites need a separate
  check before bounding writer population. [CreateProcessW standard handles](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)

This was an inspection of file bytes. No diagnostic branch or
compiler child was invoked.
