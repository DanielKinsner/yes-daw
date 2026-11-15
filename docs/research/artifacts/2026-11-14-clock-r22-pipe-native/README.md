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
