"""Read the stack and heap header fields of the pinned controller apphost."""

import hashlib
import json
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[4]
MANIFEST = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                   "clock-oracle-r22-guard-read-source-r1/source-manifest.json")


def sha(raw):
    return hashlib.sha256(raw).hexdigest().upper()


def main():
    manifest_raw = MANIFEST.read_bytes()
    if sha(manifest_raw) != "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06":
        raise ValueError("incorrect R22 manifest")
    manifest = json.loads(manifest_raw)
    path = manifest["controller"]["path"]
    pin, = [row for row in manifest["inputs"] if row["path"] == path]
    raw = Path(path).read_bytes()
    if len(raw) != pin["bytes"] or sha(raw) != pin["sha256"] or raw[:2] != b"MZ":
        raise ValueError("incorrect controller executable")

    pe, = struct.unpack_from("<I", raw, 60)
    optional = pe + 24
    if pe < 64 or optional + 104 > len(raw) or raw[pe:pe + 4] != b"PE\0\0":
        raise ValueError("invalid PE header extent")
    machine, = struct.unpack_from("<H", raw, pe + 4)
    optional_size, = struct.unpack_from("<H", raw, pe + 20)
    magic, = struct.unpack_from("<H", raw, optional)
    subsystem, = struct.unpack_from("<H", raw, optional + 68)
    if (machine, magic, subsystem) != (0x8664, 0x20B, 3) or not 104 <= optional_size <= len(raw) - optional:
        raise ValueError("expected a console AMD64 PE32+ image")
    stack_reserve, stack_commit, heap_reserve, heap_commit = struct.unpack_from("<QQQQ", raw, optional + 72)
    if (stack_reserve, stack_commit, heap_reserve, heap_commit) != (1572864, 4096, 1048576, 4096):
        raise ValueError("controller stack or heap field changed")
    print(f"PASS: pinned controller PE stack reserve {stack_reserve:,}, commit {stack_commit:,} bytes")
    print(f"PE heap reserve {heap_reserve:,}, commit {heap_commit:,} bytes")


if __name__ == "__main__":
    main()
