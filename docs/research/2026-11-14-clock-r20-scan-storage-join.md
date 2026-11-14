# Clock R20 directory-scan object storage

The [supervisor ABI measurement](artifacts/2026-11-14-clock-supervisor-abi/result.json)
supplies sizes for the compound local objects in
[`stage_scan` and `validate_path`](artifacts/2026-11-14-clock-r20-path-copy/review.md).
The new explicit prefix copy still uses a full-size local `Wide`.

| Owner or phase | Live objects accounted here | Bytes |
| --- | --- | ---: |
| Supervisor root | `State`, including request, both workers and buffers | 191,064 |
| Scan and active child validation | Search pattern `Wide`, directory entry, child `Wide`, two 513-cell `wchar_t` arrays and validation's mutable `Wide` | 5,740 |
| Scan and child metadata query | Search pattern, directory entry, child `Wide` and attribute data | 2,692 |
| Scan and initial validation | Two 513-cell arrays and validation's mutable `Wide` | 3,084 |

The initial validation completes before the pattern declaration. The
metadata query follows child validation, so its local attribute object
does not coexist with the live validation objects. The recorded root
object can coexist with any of the three phases. Its sum with the
largest measured scan phase is 196,804 bytes of selected objects.

The caller's live state outside `State`, scalar locals and formals,
failure objects, native search/handle backing, stack reservation,
compiler-created storage, and overlap with the controller and target
need separate accounting. The table prices named object payload and
lifetime only.
