# Combine the 30 measured Clock layouts

[The combined receipt](result.json) associates all 30 type IDs from the
reviewed source correspondence with their measured size, alignment, and
compile-only receipt. Each group carries its actual object and receipt
hashes. All five compiler invocations used the same exact MSVC pin.

The script substitutes those sizes into the measured terms of ten
critical source joins. For example, the vector move joins three
8-byte `nullptr_t` temporaries and a 1-byte constructor tag, for a
25-byte measured component. Pointer and owner terms in that join
still need their existing source and lifetime accounting.

Run `python -E -B compose.py --check result.json` from this directory.
The component table excludes any unpriced or unassociated objects, and
provides no whole-frame or total-resource bound.
