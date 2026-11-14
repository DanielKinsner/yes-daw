"""Check R22 command and terminal meter bounds against the pinned source."""

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
        raw = (METER["ROOT"] / pin["path"]).read_bytes()
        require(METER["digest"](raw) == pin["sha256"], f"{pin['path']} pin differs")
        sources[Path(pin["path"]).name] = raw.decode()
    header = sources["sup.h"]
    main = sources["sup_main.cpp"]
    codec = sources["sup_codec.cpp"]
    io = sources["sup_io.cpp"]
    for call in ("validate_profile", "parse_command", "build_target_command",
                 "build_environment", "finalize_record", "publish_terminal", "validate_terminal"):
        require(main.count(f"{call}(state)") == 1, f"{call} schedule differs")
    require(io.count("encode_terminal(state)") == codec.count("validate_terminal(state)") == 1,
            "encode or validation call schedule differs")
    require("r.argument_count!=(r.child_index==0?28u:6u)" in codec
            and "for(U32 a=0;a<state.request.argument_count;++a)" in codec
            and "if(length>path_units || length>(r.captured-at)/2u)" in codec,
            "argument or text admission differs")
    environment = codec.split("bool build_environment(", 1)[1].split("bool build_target_command(", 1)[0]
    lengths = [int(value) for value in re.search(r"lengths\[8\]=\{([^}]+)\}", environment)[1].split(",")]
    require(len(lengths) == 8 and "for(U32 i=0;i<8;++i)" in environment
            and "Family::EnvironmentCell,lengths[i]+value.length+2u" in environment,
            "environment schedule or debit differs")
    command_units = int(re.search(r"raw_command_units = (\d+);", header)[1])
    path_units = int(re.search(r"path_units = (\d+);", header)[1])
    terminal_bytes = int(re.search(r"terminal_bytes = (\d+);", header)[1])
    expected = {
        "Entry": (1, 1), "Profile": (1, 1), "RawCommand": (1, 1),
        "CommandCell": (1, command_units), "TargetQuote": (1, 1),
        "QuoteCell": (28, 28 * path_units), "Environment": (1, 1),
        "EnvironmentCell": (8, sum(lengths) + 8 * (path_units + 2)),
        "Encode": (1, 1), "EncodeCell": (1, terminal_bytes),
        "TerminalValidation": (2, 2),
    }
    require(all(inventory["family_sites"][name] == 1 for name in expected), "codec meter sites differ")
    census = HERE.parents[1] / "2026-11-14-clock-r22-codec-meter-census.md"
    rows = re.findall(r"\| `(\w+)` \| ([\d,]+) \| ([\d,]+) \|", census.read_text(encoding="utf-8"))
    actual = {name: tuple(int(value.replace(",", "")) for value in (calls, amounts))
              for name, calls, amounts in rows}
    require(actual == expected and len(rows) == len(expected), "codec meter table differs")
    print(f"PASS: {len(expected)} R22 command and terminal meter families")
    return set(expected)


if __name__ == "__main__":
    check()
