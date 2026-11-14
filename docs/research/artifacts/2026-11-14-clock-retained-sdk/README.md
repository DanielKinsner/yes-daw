# Clock R22 retained bytes with observed supervisor headers

The R22 reservation is 822,112,722 bytes, including the full 32 MiB
supervisor binary cap and all 79,472 transport bytes. This is the
840 MiB retained-artifact calculation. Controlled scratch has a
separate 32 MiB cap.

The [compile-only receipt](../2026-11-14-clock-supervisor-abi/r22-object-result.json)
reports 103 include paths. The path and digest join found 23 already
present among the 1,288 manifest inputs. The other 80 are reached
Windows SDK headers on this machine. Their verified current bytes
add 4,414,985, taking the projection to 826,527,707 and leaving
54,276,133 of the 840 MiB envelope.

Run `python -E -B check.py` with the exact local R22 candidate staged.
The check matches normalized Windows path identities first and then
requires the same digest for every shared path. It hashes and sums the
new files. Identical bytes at separate paths keep separate allowances.

The projection feeds the final retained and namespace join. Any
selected link inputs and final binding files still need exact pins and
membership. The final canonical-input sum must be recomputed after
binding. No compiler temporary-disk or native scratch credit follows
from this projection.
