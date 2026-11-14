"""Stage and measure the three exact comparator closures in Clock upper_bound calls."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import sys


HERE = Path(__file__).resolve().parent
NAMED = HERE.parent / "2026-11-14-clock-named-closures"
BASE = runpy.run_path(str(NAMED / "instrument.py"))
ROOT = BASE["ROOT"]
ENGINE_TIME = ROOT / "src/engine/Time.h"
CORRESPONDENCE = HERE.parent / "2026-11-06-native-sdk-context-ledger-r2/sources/038-layout-source-correspondence.json"
IDS = ("OWN_CLOSURE_qc_tick_comparator", "OWN_CLOSURE_time_tick_comparator",
       "OWN_CLOSURE_time_native_comparator")
NAMES = ("qc_tick", "time_tick", "time_native")
WRAPPER = b"""namespace yesdaw_closure_layout {
template<unsigned Site, class Closure>
constexpr Closure observe(Closure value) noexcept
{
    (void)&Observer<Site,Closure>::cells;
    return value;
}
}
"""


def prepare(output):
    facts = {row["id"]: row for row in json.loads(CORRESPONDENCE.read_bytes())["facts"]}
    pinned = BASE["PINS"] | {ENGINE_TIME: facts[IDS[1]]["primary_pin"]["sha256"]}
    inputs = {}
    for path, expected in pinned.items():
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest().upper() != expected:
            raise ValueError(f"unexpected original source: {path}")
        inputs[path] = data
    headers = {path: inputs[path] for path in (BASE["HEADER"], ENGINE_TIME)}
    for site, id in enumerate(IDS, start=1):
        row = facts[id]
        path = ROOT / row["path"]
        line = row["source_excerpt"].strip().encode()
        start, end = line.index(b"["), line.rindex(b"}") + 1
        closure = line[start:end]
        old = headers[path]
        wrapper = f"::yesdaw_closure_layout::observe<{site}>(".encode() + closure + b")"
        if old.count(closure) != 1:
            raise ValueError(f"unexpected original expression: {id}")
        new = old.replace(closure, wrapper)
        if new.replace(wrapper, closure) != old:
            raise ValueError(f"cannot restore original expression: {id}")
        headers[path] = new
    return {
        output / "checker/clock_stage.cpp": BASE["PREAMBLE"] + WRAPPER + inputs[BASE["CHECKER"] / "clock_stage.cpp"],
        output / "checker/source-costs.h": inputs[BASE["CHECKER"] / "source-costs.h"],
        output / "waveform-definition-r3/independent_clock.h": headers[BASE["HEADER"]],
        output / "include/engine/Time.h": headers[ENGINE_TIME],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    source = args.source.resolve()
    if args.compiler is None and args.output is None:
        files = prepare(source)
        source.mkdir()
        for path, data in files.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            print(hashlib.sha256(data).hexdigest().upper(), path.relative_to(source))
    elif args.compiler is None or args.output is None:
        parser.error("--compiler and --output must be given together")
    else:
        sys.path[:0] = [str(NAMED)]
        measure = runpy.run_path(str(NAMED / "run-purpose.py"))["measure"]
        measure(source, args.compiler.resolve(strict=True), args.output.resolve(),
                prepare, NAMES, source / "include", (ENGINE_TIME,))
