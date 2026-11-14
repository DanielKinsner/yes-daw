"""Stage exact-source instrumentation for the two named Clock closure types."""

import argparse
import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
REVISIONS = ROOT / "build-ci/musical-audio-persistent-planner-certification/source-revisions"
CHECKER = REVISIONS / "clock-oracle-r19-local-profile-source-r1"
HEADER = REVISIONS / "waveform-definition-r3/independent_clock.h"
PINS = {
    CHECKER / "clock_stage.cpp": "F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206",
    CHECKER / "source-costs.h": "0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27",
    HEADER: "A75FB5D17C9CACA2113DC8567A3E27F9A40FD9531BC59084E07F88576D15CB64",
}
PREAMBLE = b"""#pragma section(".yclos", read)
namespace yesdaw_closure_layout {
template<unsigned Site, class Closure>
struct Observer {
    __declspec(dllexport) __declspec(allocate(".yclos"))
    static inline constinit const unsigned long long cells[] = {
        0x5944434C4F533031ull, Site, sizeof(Closure), alignof(Closure),
    };
};
}
"""


def insert(source, anchor, statement):
    if source.count(anchor) != 1:
        raise ValueError(f"unexpected source occurrence: {anchor!r}")
    modified = source.replace(anchor, anchor + statement)
    if modified.replace(statement, b"") != source:
        raise ValueError("instrumentation cannot be removed byte-for-byte")
    return modified


def prepare(output):
    inputs = {}
    for path, expected in PINS.items():
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest().upper() != expected:
            raise ValueError(f"unexpected original source: {path}")
        inputs[path] = raw
    checker = insert(
        inputs[CHECKER / "clock_stage.cpp"],
        b"const auto get=[&](std::uint64_t i){return verify?cut(production,fractions,i):cut(generator,fractions,i);};",
        b"(void)&::yesdaw_closure_layout::Observer<1,decltype(get)>::cells;",
    )
    header = insert(
        inputs[HEADER], b"        return bit;\n    };\n",
        b"    (void)&::yesdaw_closure_layout::Observer<2,decltype(nextBit)>::cells;\n",
    )
    return {
        output / "checker/clock_stage.cpp": PREAMBLE + checker,
        output / "checker/source-costs.h": inputs[CHECKER / "source-costs.h"],
        output / "waveform-definition-r3/independent_clock.h": header,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    files = prepare(output)
    output.mkdir()
    for path, raw in files.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
        print(hashlib.sha256(raw).hexdigest().upper(), path.relative_to(output))
