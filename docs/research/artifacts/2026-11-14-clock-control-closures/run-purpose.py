"""Measure the original Clock control-action closure types via /c only."""

import argparse
from pathlib import Path
import runpy
import sys

from instrument import NAMED, NAMES, prepare


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    sys.path[:0] = [str(NAMED)]
    measure = runpy.run_path(str(NAMED / "run-purpose.py"))["measure"]
    measure(args.source.resolve(strict=True), args.compiler.resolve(strict=True),
            args.output.resolve(), prepare, NAMES)
