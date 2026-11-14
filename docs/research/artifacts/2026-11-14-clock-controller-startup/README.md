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
