"""Stage the verified MSVC layout compiler and headers from a Microsoft offline layout."""

import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile


HERE = Path(__file__).resolve().parent
ARTIFACTS = HERE.parent
ROOT = ARTIFACTS.parents[2]
OLD = ARTIFACTS / "2026-11-06-native-object-layout-purpose-source-r1/sources"
PINS = OLD / "025-source-pins.json"
PROFILE = OLD / "022-profile.json"
PINNED_PINS_HASH = "7B8EF452D04B97D38B92D1E790FA61B355660A141FA4CB1D21A1002C4B3EA165"
BIN = "/bin/hostx64/x64/"


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def load(path):
    return json.loads(path.read_bytes())


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def matches(data, pin):
    return len(data) == pin["bytes"] and digest(data) == pin["sha256"]


def toolchain_inputs():
    raw = PINS.read_bytes()
    require(digest(raw) == PINNED_PINS_HASH, "original compiler pins changed")
    archive = json.loads(raw)
    (profile_pin,) = [row for row in archive["files"]
                      if row["path"] == "build-ci/20261106-native-object-layout-purpose-source-r1/profile.json"]
    require(matches(PROFILE.read_bytes(), profile_pin), "original compiler profile changed")
    pins = archive["inputs"]
    compiler = {row["path"].casefold().split(BIN)[1]: row
                for row in pins if BIN in row["path"].casefold()}
    headers = [row for row in pins if "/include/" in row["path"].casefold()]
    require(len(compiler) == 91 and len(headers) == 427, "original compiler inventory changed")
    return compiler, headers


def prepare(layout, output):
    compiler, headers = toolchain_inputs()
    base = layout.resolve(strict=True)
    catalog = load(base / "Catalog.json")
    require(catalog["info"]["buildVersion"] == "17.14.37628.2", "wrong Microsoft layout")
    payload_hashes = {
        row["sha256"].upper()
        for package in catalog["packages"]
        if package["id"].startswith("Microsoft.VC.14.44.17.14.")
        for row in package.get("payloads", [])
        if row["url"].startswith("https://download.visualstudio.microsoft.com/")
    }
    recovered = {}
    packages = {}
    for path in sorted(base.glob("Microsoft.VC.14.44.17.14.*/payload.vsix")):
        with ZipFile(path) as package:
            for member in package.infolist():
                path_lower = member.filename.casefold()
                if member.is_dir() or BIN not in path_lower:
                    continue
                name = path_lower.split(BIN, 1)[1]
                pin = compiler.get(name)
                if pin is None or name in recovered or member.file_size != pin["bytes"]:
                    continue
                data = package.read(member)
                if matches(data, pin):
                    if path not in packages:
                        package_hash = digest(path.read_bytes())
                        require(package_hash in payload_hashes, f"package hash differs: {path.parent.name}")
                        packages[path] = package_hash
                    recovered[name] = data
    require(recovered.keys() == compiler.keys(), "missing or different compiler files")

    profile = load(PROFILE)
    include_roots = [Path(path) for path in profile["include_roots"]]
    for row in headers:
        require(matches(Path(row["path"]).read_bytes(), row), f"header differs: {row['path']}")
    for root in include_roots:
        found = {p.resolve() for p in root.rglob("*") if p.is_file()}
        pinned = {Path(row["path"]).resolve() for row in headers if Path(row["path"]).is_relative_to(root)}
        require(found == pinned, f"header membership differs: {root}")

    output.mkdir()
    for name, data in recovered.items():
        # Write relative paths from the pinned inventory, never from the ZIP.
        pinned_path = compiler[name]["path"].split("/bin/Hostx64/x64/")[1]
        destination = (output / pinned_path).resolve()
        require(destination.is_relative_to(output), "invalid pinned tool path")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    receipt = {
        "build": catalog["info"]["buildVersion"],
        "compiler": compiler["cl.exe"]["sha256"],
        "compiler_files": len(recovered),
        "header_files": len(headers),
        "packages": {path.parent.name: package_hash for path, package_hash in sorted(packages.items())},
    }
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("layout", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    prepare(args.layout, args.output.resolve())
