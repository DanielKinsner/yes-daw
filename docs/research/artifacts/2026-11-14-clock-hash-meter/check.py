"""Check R22 request and hash meter bounds against the source and census tables."""

import json
from pathlib import Path
import re
import runpy


HERE = Path(__file__).resolve().parent
METER = runpy.run_path(str(HERE.parent / "2026-11-14-clock-meter-inventory" / "check.py"))
require = METER["require"]
FAMILIES = {
    "Decode": 2, "DecodeCell": 1,
    "Identity": 2, "HashFile": 1, "HashChunk": 4,
    "HashByte": 1, "HashBlock": 1, "HashRound": 1,
}


def check():
    inventory = METER["inventory"]()
    for family, sites in FAMILIES.items():
        require(inventory["family_sites"][family] == sites, f"{family} sites differ")
    manifest = json.loads(METER["MANIFEST"].read_bytes())
    sources = {}
    for pin in manifest["supervisor"]["native_sources"]:
        path = METER["ROOT"] / pin["path"]
        raw = path.read_bytes()
        require(METER["digest"](raw) == pin["sha256"], f"{path} pin differs")
        sources[path.name] = raw.decode()
    codec = sources["sup_codec.cpp"]
    core = sources["sup_core.cpp"]
    io = sources["sup_io.cpp"]
    main = sources["sup_main.cpp"]
    file_caps = [int(cap) for cap in re.findall(r"!hash_file\(state,[^,]+,(\d+),", codec)]
    path_units = int(re.search(r"path_units = (\d+);", sources["sup.h"])[1])
    request_bytes = int(re.search(r"request_max_bytes = (\d+);", sources["sup.h"])[1])
    domain = re.search(r'static constexpr U8 domain\[\] = "([^"]+)";', core)[1]
    require(file_caps == [1_048_576, 33_554_432], "file hash caps differ")
    require(main.count("!validate_request_recipe(state)") == 1, "recipe calls differ")
    require(main.count("!read_request(state)") == 1 and main.count("!decode_request(state)") == 1,
            "request reader or decoder calls differ")
    request_reader = io.split("bool read_request(", 1)[1].split("bool hash_file(", 1)[0]
    require("size.QuadPart > request_max_bytes" in request_reader
            and "while (at < length)" in request_reader
            and "got==0 || got>length-at" in request_reader
            and "at+=got;" in request_reader, "request extent or progress differs")
    decoder = codec.split("bool decode_request(", 1)[1].split("bool validate_request_recipe(", 1)[0]
    require(decoder.index("spend(state,Family::DecodeCell,state.request.captured)") <
            decoder.index("if(r.captured>maximum)"), "decode debit timing differs")
    require(codec.count("!hash_run_identity(state,actual)") == 1, "run hash calls differ")
    require("got==0 || got>want" in io and "at+=got;" in io, "file progress differs")
    require("const DWORD want=remaining>65536?65536:static_cast<DWORD>(remaining);" in io,
            "file fragment limit differs")
    require("sha.pending_count < 56 ? 56u-sha.pending_count : 120u-sha.pending_count" in core
            and "sha_update(state,sha,padding,count+8u)" in core, "padding differs")
    require("spend(state, Family::HashRound, 64)" in core, "round count differs")
    updates = core.split("bool hash_run_identity(", 1)[1]
    require(updates.count("sha_update(") == 4
            and "manifest_sha.bytes,32)" in updates and "length,4)" in updates
            and "encoded[2]" in updates and "sha_update(state,sha,encoded,2)" in updates,
            "run identity input differs")
    file_bytes = sum(file_caps)
    run_bytes = len(domain.encode("ascii")) + 1 + 32 + 4 + 2 * path_units
    file_updates = file_bytes + len(file_caps)
    run_updates = path_units + 4
    chunks = file_bytes + file_updates + run_updates + 2 * (len(file_caps) + 1)
    blocks = sum((size + 72) // 64 for size in [*file_caps, run_bytes])
    expected = {
        "Decode": (request_bytes + 1, request_bytes + 1),
        "DecodeCell": (1, request_bytes),
        "Identity": (2, 2),
        "HashFile": (len(file_caps), len(file_caps)),
        "HashChunk": (chunks, chunks),
        "HashByte": (file_updates + run_updates, blocks * 64),
        "HashBlock": (blocks, blocks),
        "HashRound": (blocks, blocks * 64),
    }
    census = HERE.parents[1] / "2026-11-14-clock-r20-identity-read-census.md"
    rows = re.findall(r"\| `(\w+)` \| ([\d,]+) \| ([\d,]+) \|", census.read_text(encoding="utf-8"))
    actual = {family: tuple(int(value.replace(",", "")) for value in (calls, amounts))
              for family, calls, amounts in rows}
    require(actual == expected and len(rows) == len(expected), "census table differs")
    print(f"PASS: {len(expected)} R22 request and hash meter families, {blocks:,} compression blocks per helper")


if __name__ == "__main__":
    check()
