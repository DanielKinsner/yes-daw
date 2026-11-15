"""Check selected normal imports of the exact R22 compiler-side images."""

import hashlib
import json
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[4]
MANIFEST = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                   "clock-oracle-r22-guard-read-source-r1/source-manifest.json")
MANIFEST_SHA = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"
APIS = {"CreateProcessW", "CreateThread", "DuplicateHandle", "GetProcAddress", "WriteFile", "_wspawnv",
        "_setmode", "_write", "fwrite", "_fwrite_nolock", "__stdio_common_vfprintf", "__stdio_common_vfwprintf"}
IMAGES = ("cl.exe", "c1xx.dll", "c2.dll", "link.exe", "mspdb140.dll", "mspdbcore.dll", "mspdbsrv.exe", "cvtres.exe")


def require(ok, description):
    if not ok:
        raise ValueError(description)


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def imports(raw):
    pe, = struct.unpack_from("<I", raw, 60)
    require(raw[:2] == b"MZ" and raw[pe:pe + 4] == b"PE\0\0", "unexpected PE header")
    machine, count = struct.unpack_from("<HH", raw, pe + 4)
    optional_size, = struct.unpack_from("<H", raw, pe + 20)
    optional = pe + 24
    magic, = struct.unpack_from("<H", raw, optional)
    require((machine, magic) == (0x8664, 0x20B) and optional_size >= 128, "expected AMD64 PE32+")
    sections = [struct.unpack_from("<IIII", raw, optional + optional_size + 40 * i + 8) for i in range(count)]

    def extent(rva, size):
        _, start, length, offset = next(row for row in sections if row[1] <= rva < row[1] + row[2])
        at = offset + rva - start
        end = min(len(raw), offset + length)
        require(size <= end - at, "PE directory extends past file-backed section")
        return at, end

    rva, length = struct.unpack_from("<II", raw, optional + 120)
    require(length >= 20 and length % 20 == 0, "unexpected import directory extent")
    table, _ = extent(rva, length)
    found = set()
    for lookup, stamp, forwarder, name, address in struct.iter_unpack("<5I", raw[table:table + length]):
        if not (lookup or stamp or forwarder or name or address):
            return found
        at, end = extent(lookup or address, 8)
        while at <= end - 8:
            entry, = struct.unpack_from("<Q", raw, at)
            if entry == 0:
                break
            if entry >> 63 == 0:
                name_at, name_end = extent(entry, 3)
                stop = raw.index(0, name_at + 2, name_end)
                found.add(raw[name_at + 2:stop].decode("ascii"))
            at += 8
        else:
            raise ValueError("unterminated import lookup")
    raise ValueError("unterminated import directory")


def main():
    data = MANIFEST.read_bytes()
    require(sha(data) == MANIFEST_SHA, "unexpected R22 manifest")
    manifest = json.loads(data)
    compiler = Path(manifest["compiler"]["path"])
    recipe = manifest["compiler"]["arguments"]
    require(recipe[16] == "/link" and "/c" not in recipe[:16], "unexpected compiler stage")
    pins = {Path(row["path"]): row for row in manifest["inputs"]}
    result = {}
    for name in IMAGES:
        path = compiler.parent / name
        pin = pins[path]
        with path.open("rb") as source:
            raw = source.read(pin["bytes"] + 1)
        require(len(raw) == pin["bytes"] and sha(raw) == pin["sha256"], f"compiler-side input changed: {name}")
        result[name] = imports(raw) & APIS
    require([name for name, found in result.items() if "WriteFile" in found]
            == ["cl.exe", "c1xx.dll", "c2.dll", "link.exe", "mspdbcore.dll"], "direct write importer set differs")
    require([name for name, found in result.items() if "CreateProcessW" in found]
            == ["cl.exe", "c1xx.dll", "c2.dll", "link.exe", "mspdb140.dll"], "process importer set differs")
    print(json.dumps({name: sorted(found) for name, found in result.items()}, indent=2))
    print("PASS: eight exact compiler-side image pins and selected normal import sets")


if __name__ == "__main__":
    main()
