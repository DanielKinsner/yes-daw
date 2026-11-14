"""Check exact R22 COFF default libraries and candidate linker input pins."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


ROOT = Path(__file__).resolve().parents[4]
ARTIFACTS = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                   "clock-oracle-r22-guard-read-source-r1/source-manifest.json")
MANIFEST_PIN = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"
LINK_FILES = {"link.exe", "legacy_stdio_definitions.lib", "msvcprt.lib", "msvcrt.lib",
              "oldnames.lib", "vcruntime.lib", "ucrt.lib", "kernel32.lib"}


def need(condition, reason):
    if not condition:
        raise ValueError(reason)


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def defaults(data):
    machine, count, _, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", data)
    need(machine == 0x8664 and optional_size == 0, "expected x64 COFF with no optional header")
    need(20 + 40 * count <= len(data), "truncated section table")
    sections = []
    for index in range(count):
        name, length, offset = struct.unpack_from("<8s8xII", data, 20 + 40 * index)
        if name == b".drectve":
            need(offset > 0 and offset + length <= len(data), "truncated directive section")
            sections.append(data[offset:offset + length].decode("ascii"))
    need(len(sections) == 1, "expected one directive section")
    return set(re.findall(r'/DEFAULTLIB:"([^"]+)"', sections[0]))


def main(objects):
    raw = MANIFEST.read_bytes()
    need(sha(raw) == MANIFEST_PIN, "wrong R22 manifest")
    manifest = json.loads(raw)
    inputs = {}
    for row in manifest["inputs"]:
        path = ROOT / row["path"]
        name = path.name.casefold()
        if name in LINK_FILES:
            need(name not in inputs, f"duplicate selected file: {name}")
            data = path.read_bytes()
            need(len(data) == row["bytes"] and sha(data) == row["sha256"],
                 f"wrong input bytes: {name}")
            inputs[name] = len(data)
    need(inputs.keys() == LINK_FILES, "selected link input set differs")
    receipt = json.loads((ARTIFACTS / "2026-11-14-clock-supervisor-abi/r22-object-result.json").read_bytes())
    need(receipt["manifest_sha256"] == MANIFEST_PIN, "wrong object receipt manifest")
    need(set(receipt["objects"]) == {f"sup_{name}.cpp" for name in ("codec", "core", "io", "main")},
         "wrong object receipt set")
    for source, row in receipt["objects"].items():
        artifact = objects / Path(source).with_suffix(".obj")
        data = artifact.read_bytes()
        need(len(data) == row["object_bytes"] and sha(data) == row["object_sha256"],
             f"wrong object bytes: {artifact.name}")
        need(defaults(data) == {"MSVCRT", "OLDNAMES"}, f"wrong defaults: {artifact.name}")
    print(f"PASS: {len(inputs)} exact candidate link files and all four R22 COFF directives")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("objects", type=Path)
    main(parser.parse_args().objects)
