"""Reconstruct the exact R18 controller file tree from the PowerShell release ZIP."""

import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[4]
MANIFEST = ROOT / (
    "build-ci/musical-audio-persistent-planner-certification/source-revisions/"
    "clock-oracle-r18-provenance-source-r1/source-manifest.json"
)
MANIFEST_HASH = "6F85483B234E1A4D052A84804A0AA15CC34E1392A1257E45EEFD9D1F06BB89E0"
ZIP_HASH = "32EB8F6CDCE08F86E987D625A2733E54AC3E289AE7E1621B14C0B5BCEC2434EA"


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def stage(archive, output):
    manifest_bytes = MANIFEST.read_bytes()
    require(digest(manifest_bytes) == MANIFEST_HASH, "R18 manifest differs")
    manifest = json.loads(manifest_bytes)
    with archive.open("rb") as stream:
        require(hashlib.file_digest(stream, "sha256").hexdigest().upper() == ZIP_HASH,
                "PowerShell release ZIP differs")
    original_root = manifest["controller"]["path"].removesuffix("/pwsh.exe")
    prefix = original_root + "/"
    pins = {row["path"].removeprefix(prefix): row for row in manifest["inputs"]
            if row["path"].startswith(prefix)}
    require(len(pins) == 323 and pins["pwsh.exe"]["sha256"] == manifest["controller"]["sha256"],
            "unexpected controller input inventory")

    with ZipFile(archive) as package:
        members = {member.filename.casefold(): member for member in package.infolist()}
        require(len(members) == len(package.infolist()), "duplicate ZIP member")
        output.mkdir()
        for name, pin in pins.items():
            member = members[name.casefold()]
            require(member.file_size == pin["bytes"], f"controller member size differs: {name}")
            data = package.read(member)
            require(digest(data) == pin["sha256"], f"controller member hash differs: {name}")
            path = (output / name).resolve()
            require(path.is_relative_to(output), f"unexpected output path: {name}")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        # Ref, Schemas and preview are closed root entries but contain none of
        # the admitted files. Keep only their directory entries.
        for name in ("ref", "Schemas", "preview"):
            (output / name).mkdir()

    directories = [row for row in manifest["closed_directory_inputs"]
                   if row["path"] == original_root or row["path"].startswith(prefix)]
    require(len(directories) == 4, "unexpected controller directory inventory")
    for row in directories:
        relative = row["path"][len(original_root):].lstrip("/")
        root = output / relative
        entries = list(root.rglob("*") if row["recursive"] else root.iterdir())
        files = {path.relative_to(root).as_posix() for path in entries if path.is_file()}
        require(len(entries) == row["entry_count"] and files == set(row["files"]),
                f"controller directory membership differs: {relative}")
    print(f"PASS: {len(pins)} PowerShell files and {len(directories)} closed directories")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    stage(args.archive.resolve(strict=True), args.output.resolve())
