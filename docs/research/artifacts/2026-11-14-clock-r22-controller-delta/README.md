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
