# Clock R22 controller function correspondence

The [source check](check.py) compares the pinned R13 controller in
the reviewed census archive to the exact R22 controller. Of the
74 older function bodies, 69 are byte-identical and five changed.
R22 adds 14 functions for the supervisor handles, wire format,
budget and failure protocol. None of the older functions disappeared.

The five changed bodies are the path catalogue, child invocation,
child cleanup projection, controller initialization, and input pin
fence. Both scripts also have top-level statements with separate
source accounting. The scan reads exact source text with the known
column-one declarations. The first check found one shared function
whose closing brace is indented. The corrected check handles that
body as well. Neither PowerShell controller is loaded or parsed.

Run `python -E -B check.py` with the exact R22 candidate staged.
This starts the R22 controller ledger with a checked set of 19 new
or changed bodies and exposes the 69 reusable body identities for
review. Identical bodies still need current caller domains,
occurrence counts, different nested callees and fresh whole phase
composition before any bound transfers.

## Script root and success geometry

Excluding the function definitions leaves 177 baseline lines and
183 current lines, with 14 edited spans including comments and
spacing. The current manifest and source jointly supply these root
domains for the caller and lifetime worksheets:

| Root region | Current domain |
| --- | --- |
| Entry refusal and reflection | Eight false entry guards. The zero-parameter `GetType` lookup now takes the static `Type.EmptyTypes` field. |
| Bootstrap JSON | 1 MiB captured input, 17,209 token admission, 512 string units, depth four before the SDK parse. The previous token admission was 16,104. |
| Directory creation | One run root and seven listed child directory calls, including `transport`. |
| Expected file set before the final bundle | 56 reference, three build, four control, four stage and six transport names. The observed root walk is capped at 85 entries. |
| Transport artifact projection | Six name joins, at most six file hashes and six hex projections. The caps are 41,260, 18,644, 18,644 and three times 308 bytes. The 65,536-byte hash routine admits at most one explicit chunk per file. |
| Bundle projection | At most 73 artifact rows and 1,294 transitive rows, with at most five fixed-point measurements and one final encode. `sup.exe` adds one transitive descriptor. The final bundle is the 74th success file. |
| Retained sum | Add 79,472 reserved transport bytes and the actual helper pin. The six transport artifacts already enter the admitted sum through their individual rows. The helper is charged once as a separate input. |

The transport loops run at distinct sites. The first contributes six
expected-name additions and enlarges the file-set arguments. The
second contributes six artifact maps, digest carriers and hex calls
only after the final input fence. On failure, a cap or hash can stop
the second loop before all six visits. Use the current arguments for
the shared file-set, hash, JSON preflight and serialization bodies.
The listed cardinalities are conditional on the staged, authenticated
manifest. They give the new row and loop domains for fresh source and
resource accounting.
