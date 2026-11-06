#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("frozen_proof_compile", REPO_ROOT / "tools/frozen-proof-compile.py")
assert SPEC is not None and SPEC.loader is not None
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
SOURCE = REPO_ROOT / "tests/playback_click_position_cache_tests.cpp"

# Actual Linux / AppleClang CI messages after the single category demotion.
GCC = (
    "loop variable ‘<structured bindings>’ creates a copy from type "
    "‘const std::pair<long int, long int>’ [-Wrange-loop-construct]"
)
APPLE_CLANG = (
    "loop variable '[first, end]' creates a copy from type 'const value_type' "
    "(aka 'const std::pair<long long, long long>') [-Wrange-loop-construct]"
)


def warning(message: str = GCC, *, source: Path = SOURCE, row: int = 220, column: int = 33) -> str:
    return f"{source}:{row}:{column}: warning: {message}\n"


def original_command() -> list[str]:
    return ["sccache", "compiler", "-Wall", "-Wextra", "-Werror", "-o", "proof.o", "-c", str(SOURCE)]


class DiagnosticGateTests(unittest.TestCase):
    def test_exact_linux_and_apple_messages(self) -> None:
        for message in (GCC, GCC.replace("‘", "'").replace("’", "'"), APPLE_CLANG):
            with self.subTest(message=message):
                self.assertEqual(POLICY.classify_diagnostics("", warning(message), SOURCE), 1)

    def test_clang_lp64_spelling(self) -> None:
        message = APPLE_CLANG.replace("long long, long long", "long, long")
        self.assertEqual(POLICY.classify_diagnostics(warning(message), "", SOURCE), 1)

    def test_zero_warning_compile_and_context(self) -> None:
        context = f"{SOURCE}:220:22: note: this warning suggests a reference\n"
        context += '  220 | string_literal("warning: example");\n      | ^\n'
        self.assertEqual(POLICY.classify_diagnostics(context, "", SOURCE), 0)

    def test_clang_summary_and_color_preserve_exact_classification(self) -> None:
        text = "\x1b[1m" + warning(APPLE_CLANG).rstrip("\n") + "\x1b[0m\n1 warning generated.\n"
        self.assertEqual(POLICY.classify_diagnostics("", text, SOURCE), 1)

    def test_every_other_warning_fails(self) -> None:
        cases = {
            "header": warning(source=REPO_ROOT / "src/engine/PlaybackEngine.h"),
            "other_line": warning(row=221),
            "other_column": warning(column=34),
            "unknown_wording": warning(GCC.replace("creates a copy", "creates an expensive copy")),
            "other_category": warning(GCC.replace("range-loop-construct", "unused-variable")),
            "wrong_pair": warning(GCC.replace("long int, long int", "double, double")),
            "duplicate": warning() + warning(),
            "split_stream_duplicate": None,
            "unlocated": "clang++: warning: unused command-line argument [-Wunused-command-line-argument]\n",
            "missing_column": f"{SOURCE}:220: warning: {GCC}\n",
            "unknown_format": "warning [range-loop-construct]: loop creates a copy\n",
            "unknown_severity_format": f"{SOURCE}:220:33: advertencia: copy [-Wrange-loop-construct]\n",
            "unlocated_mentions_note": "warning: bad option; note: no classification\n",
            "empty_warning": f"{SOURCE}:220:33: warning: \n",
            "summary_without_diagnostic": "1 warning generated.\n",
            "summary_mismatch": warning() + "2 warnings generated.\n",
            "missing_category": warning(GCC.replace(" [-Wrange-loop-construct]", "")),
        }
        for label, text in cases.items():
            with self.subTest(label=label), self.assertRaises(POLICY.PolicyError):
                if text is None:
                    POLICY.classify_diagnostics(warning(), warning(), SOURCE)
                else:
                    POLICY.classify_diagnostics("", text, SOURCE)

    def test_errors_fail_even_if_backend_claims_success(self) -> None:
        for text in (f"{SOURCE}:220:33: error: invalid statement\n", "compiler: fatal error: missing input\n"):
            with self.subTest(text=text), self.assertRaises(POLICY.PolicyError):
                POLICY.classify_diagnostics(text, "", SOURCE)


class CommandAdmissionTests(unittest.TestCase):
    def test_actual_frozen_source_and_source_shape(self) -> None:
        expected_sha = "AA731375071C5DA38CFD0F404FC94E162D77BBDCF0046D7A1BE03EA7EDC017D7"
        self.assertEqual(hashlib.sha256(SOURCE.read_bytes()).hexdigest().upper(), expected_sha)
        self.assertEqual(POLICY.EXPECTED_SOURCE_SHA, expected_sha)
        self.assertEqual(POLICY.verify_source(), SOURCE)

    def test_chain_compiler_and_original_arguments_preserved(self) -> None:
        for chain in ([], ["sccache"], ["launcher", "--mode", "cache"]):
            command = chain + original_command()[1:]
            prepared, source = POLICY.prepare_command(command, len(chain))
            self.assertEqual(prepared[:-1], command)
            self.assertEqual(prepared[-1], "-Wno-error=range-loop-construct")
            self.assertEqual(source, SOURCE)

    def test_real_cmake_gnu_and_apple_argument_shapes(self) -> None:
        options = ["-DJUCE_USE_MP3AUDIOFORMAT=1", "-I/path with spaces/src", "-isystem", "/vendor/include"]
        options += ["-O3", "-DNDEBUG", "-std=gnu++20", "-Wall", "-Wextra", "-Werror", "-MD"]
        options += ["-MT", "target.o", "-MF", "target.o.d", "-o", "target.o", "-c", str(SOURCE)]
        for platform_options in ([], ["-arch", "arm64", "-isysroot", "/SDK path/MacOSX.sdk"]):
            command = ["sccache", "compiler"] + platform_options + options
            with self.subTest(platform_options=platform_options):
                prepared, _ = POLICY.prepare_command(command, 1)
                self.assertEqual(prepared, command + [POLICY.DEMOTION])

    def test_missing_or_weakened_warnings_gate_fails(self) -> None:
        for flag in ("-w", "-Wno-error", "-Wno-error=range-loop-construct", "-Wno-error=unused", "-Wno-range-loop-construct", "@opaque.rsp"):
            with self.subTest(flag=flag), self.assertRaises(POLICY.PolicyError):
                POLICY.prepare_command(original_command() + [flag], 1)
        with self.assertRaises(POLICY.PolicyError):
            POLICY.prepare_command([x for x in original_command() if x != "-Werror"], 1)

    def test_wrong_source_additional_source_and_other_compile_modes_fail(self) -> None:
        wrong_source = original_command()
        wrong_source[-1] = str(REPO_ROOT / "src/engine/PlaybackEngine.h")
        cases = [wrong_source, original_command() + ["another.cpp"], original_command() + ["-c", str(SOURCE)]]
        cases.append([argument for argument in original_command() if argument != "-c"])
        cases += [original_command() + [flag] for flag in ("-", "-E", "-S", "-fsyntax-only", "-x")]
        for command in cases:
            with self.subTest(command=command), self.assertRaises(POLICY.PolicyError):
                POLICY.prepare_command(command, 1)

    def test_invalid_launcher_chain_fails(self) -> None:
        for command, count in (([], 0), (original_command(), -1), (original_command(), 99), (["", *original_command()], 2)):
            with self.subTest(count=count), self.assertRaises(POLICY.PolicyError):
                POLICY.prepare_command(command, count)

    def test_missing_and_mutated_source_fail_without_touching_frozen_proof(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temporary_root = Path(directory)
            with self.assertRaises(OSError):
                POLICY.verify_source(temporary_root)
            synthetic = temporary_root / POLICY.PROOF_RELATIVE
            synthetic.parent.mkdir()
            synthetic.write_bytes(b"synthetic invalid source, not a copy of the frozen proof\n")
            with self.assertRaises(POLICY.PolicyError):
                POLICY.verify_source(temporary_root)
            synthetic_sha = hashlib.sha256(synthetic.read_bytes()).hexdigest().upper()
            with mock.patch.object(POLICY, "EXPECTED_SOURCE_SHA", synthetic_sha), self.assertRaises(POLICY.PolicyError):
                POLICY.verify_source(temporary_root)


class LauncherBoundaryTests(unittest.TestCase):
    def invoke(self, raw_exit: int, stderr: str = "") -> tuple[int, mock.Mock]:
        completed = subprocess.CompletedProcess([], raw_exit, b"compiler stdout\n", stderr.encode("utf-8"))
        with mock.patch.object(POLICY.subprocess, "run", return_value=completed) as child:
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                code = POLICY.main(["--launcher-count", "1", "--", *original_command()])
        return code, child

    def test_success_preserves_chain_and_uses_no_shell(self) -> None:
        code, child = self.invoke(0, warning(APPLE_CLANG))
        self.assertEqual(code, 0)
        self.assertEqual(child.call_args.args[0], original_command() + [POLICY.DEMOTION])
        self.assertFalse(child.call_args.kwargs["shell"])
        self.assertFalse(child.call_args.kwargs["check"])

    def test_raw_backend_failures_are_preserved(self) -> None:
        for raw_exit in (1, 17, 42):
            with self.subTest(raw_exit=raw_exit):
                code, _ = self.invoke(raw_exit, "compiler: error: actual failure\n")
                self.assertEqual(code, raw_exit)

    def test_success_with_unhandled_warning_fails(self) -> None:
        code, _ = self.invoke(0, "compiler: warning: unknown option\n")
        self.assertEqual(code, POLICY.POLICY_FAILURE)

    def test_source_identity_checked_again_after_compiler(self) -> None:
        completed = subprocess.CompletedProcess([], 0, b"", b"")
        with mock.patch.object(POLICY, "verify_source", side_effect=[SOURCE, POLICY.PolicyError("source changed")]):
            with mock.patch.object(POLICY.subprocess, "run", return_value=completed):
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    code = POLICY.main(["--launcher-count", "1", "--", *original_command()])
        self.assertEqual(code, POLICY.POLICY_FAILURE)

    def test_backend_failure_is_preserved_when_postcompile_source_pin_fails(self) -> None:
        completed = subprocess.CompletedProcess([], 17, b"", b"actual compiler failure\n")
        with mock.patch.object(POLICY, "verify_source", side_effect=[SOURCE, POLICY.PolicyError("source changed")]):
            with mock.patch.object(POLICY.subprocess, "run", return_value=completed):
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    code = POLICY.main(["--launcher-count", "1", "--", *original_command()])
        self.assertEqual(code, 17)

    def test_bad_launcher_input_never_starts_compiler(self) -> None:
        with mock.patch.object(POLICY.subprocess, "run") as child:
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(POLICY.main(["--launcher-count", "bogus", "--", *original_command()]), POLICY.POLICY_FAILURE)
        child.assert_not_called()


if __name__ == "__main__":
    unittest.main()
