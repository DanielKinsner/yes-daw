#!/usr/bin/env python3
"""Keep every compiler warning fatal except one pinned frozen-proof copy warning."""

from __future__ import annotations

import hashlib
from pathlib import Path
import re
import subprocess
import sys


REPO_ROOT = Path(__file__).resolve().parents[1]
PROOF_RELATIVE = Path("tests/playback_click_position_cache_tests.cpp")
EXPECTED_SOURCE_SHA = "AA731375071C5DA38CFD0F404FC94E162D77BBDCF0046D7A1BE03EA7EDC017D7"
EXPECTED_LOOP = (
    "                for (const auto [first,end] : "
    "std::array<std::pair<std::int64_t,std::int64_t>,8> {{"
)
DEMOTION = "-Wno-error=range-loop-construct"
POLICY_FAILURE = 2

# GCC's UTF8 quotation marks are normalized below. These are the actual GCC /
# AppleClang diagnostics and Clang's equivalent LP64 spelling of the same pair.
EXPECTED_MESSAGES = frozenset(
    (
        "loop variable '<structured bindings>' creates a copy from type "
        "'const std::pair<long int, long int>' [-Wrange-loop-construct]",
        "loop variable '[first, end]' creates a copy from type 'const value_type' "
        "(aka 'const std::pair<long long, long long>') [-Wrange-loop-construct]",
        "loop variable '[first, end]' creates a copy from type 'const value_type' "
        "(aka 'const std::pair<long, long>') [-Wrange-loop-construct]",
    )
)
LOCATED_WARNING = re.compile(r"^(.*):([0-9]+):([0-9]+): warning: (.*)$")
WARNING_SUMMARY = re.compile(r"^([0-9]+) warnings? generated\.$")
WARNING_WORD = re.compile(r"\bwarnings?\b", re.IGNORECASE)
ERROR_DIAGNOSTIC = re.compile(r"(?:^|\s)(?:fatal )?error:", re.IGNORECASE)
NOTE_LINE = re.compile(r"^(.*?)(?::[0-9]+(?::[0-9]+)?: )?note: ")
SOURCE_EXCERPT = re.compile(r"^\s*(?:[0-9]+\s*\||\|)")
ANSI_SGR = re.compile(r"\x1b\[[0-9;]*m")
SOURCE_SUFFIXES = frozenset((".c", ".cc", ".cpp", ".cxx", ".C", ".m", ".mm", ".i", ".ii"))


class PolicyError(Exception):
    pass


def verify_source(repo_root: Path = REPO_ROOT) -> Path:
    source = (repo_root / PROOF_RELATIVE).resolve(strict=True)
    content = source.read_bytes()
    if hashlib.sha256(content).hexdigest().upper() != EXPECTED_SOURCE_SHA:
        raise PolicyError("frozen source SHA256 changed")
    lines = content.decode("utf-8").splitlines()
    if len(lines) < 220 or lines[219] != EXPECTED_LOOP:
        raise PolicyError("frozen source line 220 shape changed")
    if lines[219].index("[first,end]") + 1 != 33:
        raise PolicyError("frozen source binding column changed")
    return source


def prepare_command(
    command: list[str], launcher_count: int, repo_root: Path = REPO_ROOT
) -> tuple[list[str], Path]:
    if launcher_count < 0 or launcher_count >= len(command) or any(not arg for arg in command):
        raise PolicyError("invalid compiler launcher chain")
    source = verify_source(repo_root)
    arguments = command[launcher_count + 1 :]
    if "-Werror" not in arguments:
        raise PolicyError("the normal warnings-as-errors gate is missing")
    if any(
        arg == "-w"
        or arg.startswith("-Wno-error")
        or arg == "-Wno-range-loop-construct"
        or arg.startswith("@")
        for arg in arguments
    ):
        raise PolicyError("preexisting diagnostic demotion, suppression or opaque response file")
    if arguments.count("-c") != 1:
        raise PolicyError("expected exactly one source compilation")
    source_index = arguments.index("-c") + 1
    if source_index >= len(arguments) or Path(arguments[source_index]).resolve(strict=True) != source:
        raise PolicyError("compiler source is outside the pinned frozen proof")
    for index, argument in enumerate(arguments):
        if index != source_index and not argument.startswith("-") and Path(argument).suffix in SOURCE_SUFFIXES:
            raise PolicyError("additional source is outside the pinned frozen proof")
    if "-" in arguments or "-x" in arguments or any(arg in arguments for arg in ("-E", "-S", "-fsyntax-only")):
        raise PolicyError("expected the normal object compilation mode")
    return [*command, DEMOTION], source


def classify_diagnostics(stdout: str, stderr: str, source: Path) -> int:
    accepted = 0
    summaries: list[str] = []
    for stream in (stdout, stderr):
        for original in stream.splitlines():
            line = ANSI_SGR.sub("", original)
            note = NOTE_LINE.match(line)
            if SOURCE_EXCERPT.match(line) or (note and not WARNING_WORD.search(note[1])):
                continue
            if ERROR_DIAGNOSTIC.search(line):
                raise PolicyError("compiler emitted an error diagnostic")
            summary = WARNING_SUMMARY.fullmatch(line)
            if summary:
                summaries.append(summary[1])
                continue
            if not WARNING_WORD.search(line) and "[-W" not in line:
                continue
            diagnostic = LOCATED_WARNING.fullmatch(line)
            if diagnostic is None:
                raise PolicyError("unlocated or unrecognized compiler warning")
            path, row, column, message = diagnostic.groups()
            message = message.translate(str.maketrans({"‘": "'", "’": "'"}))
            if (
                Path(path).resolve() != source
                or row != "220"
                or column != "33"
                or message not in EXPECTED_MESSAGES
            ):
                raise PolicyError("compiler warning is outside the exact frozen-source classification")
            accepted += 1
            if accepted > 1:
                raise PolicyError("the pinned compiler warning was repeated")
    if any(count != str(accepted) for count in summaries):
        raise PolicyError("compiler warning summary does not match classified diagnostics")
    return accepted


def main(argv: list[str] | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    try:
        if len(argv) < 4 or argv[0] != "--launcher-count" or argv[2] != "--":
            raise PolicyError("expected launcher count followed by the original compiler command")
        launcher_count = int(argv[1])
        command, source = prepare_command(argv[3:], launcher_count)
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=False, check=False)
        stdout = result.stdout.decode("utf-8", errors="replace")
        stderr = result.stderr.decode("utf-8", errors="replace")
        sys.stdout.write(stdout)
        sys.stderr.write(stderr)
        source_error = None
        try:
            if verify_source() != source:
                raise PolicyError("compiler source identity changed during compilation")
        except (PolicyError, OSError, UnicodeError) as error:
            source_error = error
        if result.returncode:
            if source_error is not None:
                print(f"frozen-proof-compile: REJECTED: {source_error}", file=sys.stderr)
            print(f"frozen-proof-compile: compiler raw exit {result.returncode}", file=sys.stderr)
            return result.returncode
        if source_error is not None:
            raise PolicyError(str(source_error))
        accepted = classify_diagnostics(stdout, stderr, source)
        print(f"frozen-proof-compile: accepted {accepted} exact pinned range-copy warning(s); all other warnings fatal")
        return 0
    except (PolicyError, OSError, ValueError, UnicodeError) as error:
        print(f"frozen-proof-compile: REJECTED: {error}", file=sys.stderr)
        return POLICY_FAILURE


if __name__ == "__main__":
    sys.exit(main())
