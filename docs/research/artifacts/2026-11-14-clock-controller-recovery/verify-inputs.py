"""Verify R18 input identity and directory membership under the local tool path mapping."""

import argparse
import hashlib
import json
from pathlib import Path
import sys

from stage import MANIFEST, MANIFEST_HASH, ROOT, digest, require


def inventory(controller, compiler, python):
    raw = MANIFEST.read_bytes()
    require(digest(raw) == MANIFEST_HASH, "R18 manifest differs")
    manifest = json.loads(raw)
    mapping = {
        Path(manifest["controller"]["path"]).parent: controller,
        Path(manifest["compiler"]["path"]).parent: compiler,
    }
    (old_python,) = [Path(row["path"]) for row in manifest["inputs"]
                     if row["path"].endswith("/Python313/python.exe")]

    def relocate(original):
        if original == old_python:
            return python
        for prefix, local in mapping.items():
            if original.is_relative_to(prefix):
                return local / original.relative_to(prefix)
        return ROOT / original

    files = {}
    visited = set()
    moved = 0
    for row in manifest["inputs"]:
        original = Path(row["path"])
        path = relocate(original)
        require(path.stat().st_size == row["bytes"], f"input size differs: {path}")
        with path.open("rb") as stream:
            require(hashlib.file_digest(stream, "sha256").hexdigest().upper() == row["sha256"],
                    f"input hash differs: {path}")
        resolved = path.resolve()
        require(resolved not in visited, f"duplicate physical input path: {path}")
        visited.add(resolved)
        files[original] = resolved
        if original == old_python or any(original.is_relative_to(prefix) for prefix in mapping):
            moved += 1

    directories = {}
    for row in manifest["closed_directory_inputs"]:
        original = Path(row["path"])
        root = relocate(original)
        require(root.is_dir(), f"missing input directory: {root}")
        entries = list(root.rglob("*") if row["recursive"] else root.iterdir())
        members = {p.relative_to(root).as_posix().casefold() for p in entries if p.is_file()}
        require(len(entries) == row["entry_count"] and members == {name.casefold() for name in row["files"]},
                f"directory membership differs: {root}")
        directories[original] = root
    return manifest, files, directories, moved


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("controller", type=Path)
    parser.add_argument("compiler", type=Path)
    args = parser.parse_args()
    _, files, directories, moved = inventory(args.controller.resolve(strict=True),
                                             args.compiler.resolve(strict=True),
                                             Path(sys.executable).resolve(strict=True))
    print(f"PASS: {len(files)} distinct inputs, {moved} relocated, {len(directories)} closed directories")
