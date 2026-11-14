# Clock comparator closure layouts

Pinned MSVC 19.44.35228 [measured all three comparator types](result.json)
at size and alignment one byte. It emitted three tagged read-only data
records and verified the exact staged headers among all 110 includes.

The independent source review verified byte-for-byte restoration of both
headers after removing the observer calls from their original comparator
expressions. The four-argument `upper_bound` calls still receive those
same deduced types by value.

Run `python -E -B purpose.py <new-source-directory>` and then repeat the
command with `--compiler <compiler-directory> --output <new-output-directory>`.
Only `/c` was used. The added wrappers serve layout observation only.
