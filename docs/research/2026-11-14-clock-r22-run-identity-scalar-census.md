# Clock R22 run-identity scalar bounds

Scope is the single `hash_run_identity` invocation in the
[pinned native recipe](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp).
The core function builds a SHA input from the domain, manifest digest, a
four-byte path length and at most 512 UTF-16 units. This worksheet covers
its local work. The `spend`, `sha_init`, `sha_update` and `sha_finish`
call boundaries and bodies belong to their respective rows.

| Region | Actions |
| --- | ---: |
| Respond to identity meter, SHA init, domain and manifest updates, and retire the outer helper | 36 |
| Form local SHA and length-array lifetimes, including the two SHA counter defaults, loop initializations and final conditions | 44 |
| One length-initialization iteration including loop work and typed shift/extraction | 32 |
| Test the length update before starting the path loop | 8 |
| One path iteration including length guard, typed load, two byte initializations and update outcome | 56 |
| Return the finish result | 8 |
| Per helper | 28,896 |

The length loop reads the path's U32 length, multiplies the index by
8, shifts, casts to U8 and stores into the byte array. Allow 10 loop
actions, five for the path length access, four for the bit extraction
and seven for the indexed store including array decay.

For each path unit allow 16 loop actions including reads through the
request and working-path members, 16 to form the typed code unit from
the indexed read, 16 to initialize both encoded bytes and retire the
iteration's local payload, and eight for the update outcome.

The local calls and their outcome checks fit the same envelope on
refusal. The SHA calls own the caller's argument formation, including
the explicit domain extent conversion, the manifest array decay and
length literals. The domain has static duration and its local
declaration forms no dynamic byte initialization.

Independent review accepted the local bound. It checked the exact
core and recipe pins, the two loop bounds, conversions, byte writes,
call ownership and arithmetic.
