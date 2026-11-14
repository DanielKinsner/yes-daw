"""Tag each original Clock control-action template specialization for /c layout."""

import argparse
import hashlib
from pathlib import Path
import runpy


HERE = Path(__file__).resolve().parent
NAMED = HERE.parent / "2026-11-14-clock-named-closures"
BASE = runpy.run_path(str(NAMED / "instrument.py"))
NAMES = (
    "work-one-below", "fraction-carry", "fraction-carry-native-read", "q-off-by-one",
    "truncated-record", "trailing-record", "metadata-corruption", "full-prefix",
    "ramp-law", "inverse-whole", "inverse-fraction",
)


def prepare(output):
    inputs = {}
    for path, expected in BASE["PINS"].items():
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest().upper() != expected:
            raise ValueError(f"unexpected original source: {path}")
        inputs[path] = data
    original = inputs[BASE["CHECKER"] / "clock_stage.cpp"]
    source = original
    undo = []
    old_template = b"template<class F>void expect(const char* id,F action,const char* reason,std::array<std::uint64_t,6> witness){"
    new_template = old_template.replace(b"template<class F>", b"template<unsigned Site,class F>")
    new_template += b"(void)&::yesdaw_closure_layout::Observer<Site,F>::cells;"
    undo.append((new_template, old_template))
    for site, name in enumerate(NAMES, start=1):
        old = f'expect("{name}",[&]'.encode("ascii")
        new = f'expect<{site}>("{name}",[&]'.encode("ascii")
        undo.append((new, old))
    for new, old in undo:
        if source.count(old) != 1 or new in source:
            raise ValueError(f"unexpected control occurrence: {old!r}")
        source = source.replace(old, new)
    restored = source
    for new, old in reversed(undo):
        restored = restored.replace(new, old)
    if restored != original:
        raise ValueError("instrumentation does not restore the original source")
    return {
        output / "checker/clock_stage.cpp": BASE["PREAMBLE"] + source,
        output / "checker/source-costs.h": inputs[BASE["CHECKER"] / "source-costs.h"],
        output / "waveform-definition-r3/independent_clock.h": inputs[BASE["HEADER"]],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    files = prepare(output)
    output.mkdir()
    for path, data in files.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        print(hashlib.sha256(data).hexdigest().upper(), path.relative_to(output))
