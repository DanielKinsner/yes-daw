"""Measure the selected MSVC string append closure from its original expression."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import sys


HERE = Path(__file__).resolve().parent
NAMED = HERE.parent / "2026-11-14-clock-named-closures"
BASE = runpy.run_path(str(NAMED / "instrument.py"))
CORRESPONDENCE = HERE.parent / "2026-11-06-native-sdk-context-ledger-r2/sources/038-layout-source-correspondence.json"
ROW, = [fact for fact in json.loads(CORRESPONDENCE.read_bytes())["facts"] if fact["id"] == "L_APPEND"]
HEADER = Path(ROW["path"])
WRAPPER = b"""namespace yesdaw_closure_layout {
template<bool Selected, class Closure>
constexpr Closure observe_append(Closure value) noexcept
{
    if constexpr (Selected)
        (void)&Observer<1,Closure>::cells;
    return value;
}
}
"""


def prepare(output):
    inputs = {}
    for path, expected in (BASE["PINS"] | {HEADER: ROW["primary_pin"]["sha256"]}).items():
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest().upper() != expected:
            raise ValueError(f"unexpected original source: {path}")
        inputs[path] = data
    excerpt = ROW["source_excerpt"].encode().replace(b"\n", b"\r\n")
    excerpt = excerpt[excerpt.index(b"[]"):excerpt.rindex(b"}") + 1]
    wrapped = (b"::yesdaw_closure_layout::observe_append<"
               b"is_same_v<_Elem, char> && is_same_v<_Traits, char_traits<char>>"
               b" && is_same_v<_Alloc, allocator<char>>>(" + excerpt + b")")
    original = inputs[HEADER]
    if original.count(excerpt) != 1:
        raise ValueError("unexpected selected append expression")
    staged_header = original.replace(excerpt, wrapped)
    if staged_header.replace(wrapped, excerpt) != original:
        raise ValueError("cannot restore the original append expression")

    # xstring can be reached through quotes in the original SDK headers.
    # Mirror the exact MSVC include root with only xstring substituted.
    profile = BASE["ROOT"] / ("docs/research/artifacts/2026-11-06-native-object-layout-purpose-source-r1/"
                              "sources/025-source-pins.json")
    pin_bytes = profile.read_bytes()
    if hashlib.sha256(pin_bytes).hexdigest().upper() != (
            "7B8EF452D04B97D38B92D1E790FA61B355660A141FA4CB1D21A1002C4B3EA165"):
        raise ValueError("original include pins changed")
    sdk_files = {}
    for row in json.loads(pin_bytes)["inputs"]:
        path = Path(row["path"])
        if path.is_relative_to(HEADER.parent):
            data = path.read_bytes()
            if hashlib.sha256(data).hexdigest().upper() != row["sha256"]:
                raise ValueError(f"unexpected SDK header: {path}")
            sdk_files[output / "msvc" / path.relative_to(HEADER.parent)] = (
                staged_header if path == HEADER else data)
    if len(sdk_files) != 361:
        raise ValueError(f"unexpected MSVC header count: {len(sdk_files)}")
    return {
        output / "checker/clock_stage.cpp": BASE["PREAMBLE"] + WRAPPER + inputs[BASE["CHECKER"] / "clock_stage.cpp"],
        output / "checker/source-costs.h": inputs[BASE["CHECKER"] / "source-costs.h"],
        output / "waveform-definition-r3/independent_clock.h": inputs[BASE["HEADER"]],
        **sdk_files,
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
        print(f"Prepared {len(files)} source files, including the pinned MSVC header mirror")
    elif args.compiler is None or args.output is None:
        parser.error("--compiler and --output must be given together")
    else:
        sys.path[:0] = [str(NAMED)]
        measure = runpy.run_path(str(NAMED / "run-purpose.py"))["measure"]
        measure(source, args.compiler.resolve(strict=True), args.output.resolve(),
                prepare, ("string_append",), replaced_headers=(HEADER,), first_include=source / "msvc",
                required_headers={source / name for name in (
                    "checker/source-costs.h", "waveform-definition-r3/independent_clock.h", "msvc/xstring",
                )})
