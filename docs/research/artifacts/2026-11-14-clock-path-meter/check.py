"""Join the pinned R22 string and path meter occurrence bounds."""

import json
from pathlib import Path
import re
import runpy


HERE = Path(__file__).resolve().parent
METER = runpy.run_path(str(HERE.parent / "2026-11-14-clock-meter-inventory" / "check.py"))
require = METER["require"]


def check():
    inventory = METER["inventory"]()
    manifest = json.loads(METER["MANIFEST"].read_bytes())
    sources = {}
    for pin in manifest["supervisor"]["native_sources"]:
        path = METER["ROOT"] / pin["path"]
        raw = path.read_bytes()
        require(METER["digest"](raw) == pin["sha256"], f"{path.name} pin differs")
        sources[path.name] = raw.decode()
    codec, io = (sources[name] for name in ("sup_codec.cpp", "sup_io.cpp"))
    profile = codec.split("bool validate_profile(", 1)[1].split("bool copy_text(", 1)[0]
    recipe = codec.split("bool validate_request_recipe(", 1)[1].split("bool build_environment(", 1)[0]
    common = recipe.split("if(r.child_index==0)", 1)[0]
    sizes = [int(size) for size in re.findall(r"join_path\(state,[^,]+,[^,]+,(\d+),", common)]
    require(len(sizes) == 6 and recipe.count("wide_from_binding(") == 1, "common recipe differs")
    require("i+=7;continue;" in codec and "L\"build\",5,build)" in codec
            and "for(U32 i=0;i<28;++i)" in recipe and "for(U32 i=0;i<8;++i)" in recipe
            and "L\"opaque-temp\",11,expected)" in recipe, "recipe expansion differs")
    require("r.argument_count!=(r.child_index==0?28u:6u)" in codec
            and "i<12u+r.argument_count" in codec, "request string count differs")
    cap = int(re.search(r"path_units = (\d+);", sources["sup.h"])[1])
    depth = int(re.search(r"if \(\+\+depth > (\d+)\)", io)[1])
    research = HERE.parents[1]
    capture = (research / "2026-11-14-clock-r22-capture-meter-census.md").read_text(encoding="utf-8")
    scans = int(re.search(r"\| `StageScan` \| ([\d,]+) \|", capture)[1].replace(",", ""))
    path = (research / "2026-11-14-clock-r18-scan-path-census.md").read_text(encoding="utf-8")
    scan_cells = int(re.search(r"\| `PathCell` charge argument sum \| ([\d,]+) \|", path)[1].replace(",", ""))
    validate = codec.count("validate_path(state,") + io.count("validate_path(state,") - 2 + 1
    require(validate == 9, "out-of-scan validation count differs")
    tokens = 28 * (cap // 7)
    joins = len(sizes) + tokens + 8 + 2
    suffix_cells = sum(sizes) + 5 * tokens + 8 * 11 + 2 * 10
    strings = profile.count("wide_from_binding(") + 1 + 12 + 28
    cell_calls = strings + 28
    guards = (scans * 8 + validate) * (depth + 1) + scans * 8 + joins
    expected = {
        "StringGuard": (strings, strings),
        "StringCell": (cell_calls, (1 << 32) - 1 + (cell_calls - 1) * cap),
        "PathGuard": (guards, guards),
        "PathCell": (scans * 16 + joins + validate,
                     scans * scan_cells + (joins + validate) * cap + suffix_cells),
    }
    require({name: inventory["family_sites"][name] for name in expected} ==
            {"StringGuard": 2, "StringCell": 3, "PathGuard": 3, "PathCell": 2}, "meter sites differ")
    census = (research / "2026-11-14-clock-r22-path-meter-census.md").read_text(encoding="utf-8")
    rows = re.findall(r"\| `(\w+)` \| ([\d,]+) \| ([\d,]+) \|", census)
    actual = {name: tuple(int(value.replace(",", "")) for value in (calls, amounts))
              for name, calls, amounts in rows}
    require(actual == expected and len(rows) == len(expected), "path meter table differs")
    print(f"PASS: {len(expected)} R22 path and string meter families")


if __name__ == "__main__":
    check()
