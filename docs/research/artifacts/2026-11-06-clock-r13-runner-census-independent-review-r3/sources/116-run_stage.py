"""Root-invoked, hidden, two-stage runner. No All stage; no vendor candidate.

Freeze must finish and its actual outputs be reviewed before a separate
CompileCalibrate invocation supplies the reviewed freeze-bundle SHA256.
R5 source readiness was promoted after independent numeric/cost review.
Root must review this exact successor before separately authorizing Freeze.
This source file has not been executed merely by being authored.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import uuid

BASE = Path(__file__).resolve().parent
# Source literal and matching reviewed manifest bind the actual readiness promotion.
# Metadata alone cannot promote the unchanged R4 source or bypass its false C++ guards.
SOURCE_READINESS_ACCEPTANCE = "R4_NUMERIC_COST_ACCEPTED_R5_STORAGE_PROFILE_ASSERTED_ROOT_STAGES_REQUIRED"


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def checked_file(path):
    require(path is not None and path.is_absolute() and path.is_file(),
            "EXPLICIT_ABSOLUTE_FILE_PATH_REQUIRED")
    return path.resolve()


def verify_console_pe(path):
    """Read the actual linked PE subsystem before allowing any launch."""
    with path.open("rb") as stream:
        size = path.stat().st_size
        dos = stream.read(64)
        require(len(dos) == 64 and dos[:2] == b"MZ", "LINKED_PE_DOS_HEADER")
        pe_offset = int.from_bytes(dos[60:64], "little")
        require(64 <= pe_offset <= size - 24, "LINKED_PE_HEADER_OFFSET")
        stream.seek(pe_offset)
        header = stream.read(24)
        require(len(header) == 24 and header[:4] == b"PE\x00\x00", "LINKED_PE_SIGNATURE")
        machine = int.from_bytes(header[4:6], "little")
        optional_size = int.from_bytes(header[20:22], "little")
        require(optional_size >= 70 and pe_offset + 24 + optional_size <= size,
                "LINKED_PE_OPTIONAL_HEADER_SIZE")
        optional = stream.read(optional_size)
        magic = int.from_bytes(optional[:2], "little")
        require(len(optional) == optional_size and magic in (0x10B, 0x20B),
                "LINKED_PE_OPTIONAL_HEADER_MAGIC")
        subsystem = int.from_bytes(optional[68:70], "little")
        require(subsystem == 3, "LINKED_PE_NOT_CONSOLE_REFUSE_LAUNCH")
    return {"status": "ACTUAL_LINKED_PE_CONSOLE_VERIFIED_BEFORE_LAUNCH",
            "machine": machine, "optional_header_magic": magic, "subsystem": subsystem,
            "subsystem_name": "IMAGE_SUBSYSTEM_WINDOWS_CUI", "file_bytes": size}


def verify_manifest(repo):
    manifest_path = BASE / "authored-manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest["authorization"] == "SOURCE_AUTHORED_ROOT_STAGE_GO_REQUIRED",
            "MANIFEST_AUTHORIZATION_MISMATCH")
    for item in manifest["sources"]:
        path = (BASE / item["path"]).resolve()
        require(path.is_relative_to(BASE) and path.is_file(), "SOURCE_PIN_PATH")
        require(sha256(path).upper() == item["sha256"].upper(), "AUTHORED_SOURCE_HASH_MISMATCH:" + item["path"])
    for item in manifest["preserved_pins"]:
        path = Path(item["absolute_path"]).resolve() if "absolute_path" in item else (repo / item["path"]).resolve()
        require(("absolute_path" in item or path.is_relative_to(repo)) and path.is_file(), "PRESERVED_PIN_PATH")
        require(sha256(path).upper() == item["sha256"].upper(), "PRESERVED_SOURCE_HASH_MISMATCH:" + item.get("path", item.get("absolute_path", "")))
    return manifest, sha256(manifest_path)


def hidden_child(command, cwd, output, name, timeout, env):
    require(os.name == "nt", "WINDOWS_MSVC_BACKEND_REQUIRED")
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    with (output / (name + ".stdout.log")).open("xb") as stdout, \
            (output / (name + ".stderr.log")).open("xb") as stderr:
        try:
            process = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                       stdout=stdout, stderr=stderr,
                                       creationflags=subprocess.CREATE_NO_WINDOW, startupinfo=startup)
            result = process.wait(timeout=timeout)
            return {"raw_exit_code": result, "timed_out": False}
        except subprocess.TimeoutExpired:
            # The still-live root-owned child identifies this stage's process tree.
            # Kill that tree before waiting again; do not enumerate/kill other work.
            taskkill = Path(env.get("SystemRoot", "C:/Windows")) / "System32" / "taskkill.exe"
            cleanup = subprocess.run([str(taskkill), "/PID", str(process.pid), "/T", "/F"],
                                     cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                     stdout=stdout, stderr=stderr, timeout=30, check=False,
                                     creationflags=subprocess.CREATE_NO_WINDOW, startupinfo=startup)
            try:
                result = process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                return {"raw_exit_code": None, "timed_out": True,
                        "cleanup_raw_exit_code": cleanup.returncode, "cleanup_complete": False}
            return {"raw_exit_code": result, "timed_out": True,
                    "cleanup_raw_exit_code": cleanup.returncode, "cleanup_complete": cleanup.returncode == 0}


def dump_new(path, value):
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def main(args):
    repo = args.repo.resolve()
    require(repo.is_dir(), "REPOSITORY_UNAVAILABLE")
    manifest, manifest_hash = verify_manifest(repo)
    output_root = args.output_root.resolve()
    require(output_root != BASE and output_root.is_relative_to(BASE), "OUTPUT_MUST_STAY_IN_NEW_IGNORED_UNIT")
    output_root.mkdir(parents=True, exist_ok=True)
    run_id = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ") + "-" + uuid.uuid4().hex[:8]
    run = output_root / (args.stage + "-" + run_id)
    run.mkdir()
    require(1 <= args.timeout_seconds <= 3600, "BOUNDED_TIMEOUT_REQUIRED")
    python = checked_file(args.python_path)
    env = os.environ.copy()  # inherited root-supplied toolchain; no environment probing
    env_hash = None
    if args.toolchain_env_json is not None:
        env_file = checked_file(args.toolchain_env_json)
        override = json.loads(env_file.read_text(encoding="utf-8"))
        require(isinstance(override, dict) and all(isinstance(k, str) and isinstance(v, str)
                for k, v in override.items()), "TOOLCHAIN_ENV_FORMAT")
        env.update(override)
        env_hash = sha256(env_file)  # never print environment values
    record = {"stage": args.stage, "status": "STARTED_NOT_CERTIFIED", "run": str(run),
              "source_manifest_sha256": manifest_hash, "toolchain_env_sha256": env_hash,
              "vendor_candidate_invoked": False, "native_or_device_invoked": False,
              "children": [], "source_execution_before_this_root_invocation": False}
    try:
        if args.stage == "Freeze":
            require(args.freeze_dir is None and args.reviewed_freeze_bundle_sha256 is None,
                    "FREEZE_DOES_NOT_CONSUME_PRIOR_REVIEW")
            freeze = run / "freeze"
            freeze.mkdir()
            child = hidden_child([str(python), "-B", str(BASE / "freeze_reference.py"), "--output", str(freeze)],
                                 BASE, run, "freeze", args.timeout_seconds, env)
            record["children"].append({"name": "freeze", **child})
            require(child["raw_exit_code"] == 0 and not child["timed_out"], "FREEZE_CHILD_FAILED")
            receipt = json.loads((freeze / "freeze_receipt.json").read_text(encoding="utf-8"))
            require(receipt["status"] == "ANALYTIC_FREEZE_ONLY_NOT_CALIBRATION"
                    and receipt["rows"] == 1125, "FREEZE_RECEIPT_DOMAIN")
            for name, expected in receipt["files_sha256"].items():
                require(name in ("calibration.tsv", "geometry.jsonl", "clock.tsv", "whole-cases.tsv", "whole-windows.tsv", "whole-case-source-hold.json")
                        and sha256(freeze / name) == expected, "FREEZE_OUTPUT_HASH_MISMATCH")
            bundle = {"status": "FROZEN_ACTUAL_OUTPUTS_AWAIT_ROOT_REVIEW",
                      "source_manifest_sha256": manifest_hash,
                      "files_sha256": {name: sha256(freeze / name) for name in
                                       ("calibration.tsv", "geometry.jsonl", "clock.tsv", "whole-cases.tsv", "whole-windows.tsv", "whole-case-source-hold.json", "freeze_receipt.json")},
                      "candidate_or_calibration_executed": False}
            dump_new(freeze / "freeze_bundle.json", bundle)
            record.update(status="FREEZE_COMPLETED_REVIEW_REQUIRED", freeze_dir=str(freeze),
                          freeze_bundle_sha256=sha256(freeze / "freeze_bundle.json"))
        else:
            require(manifest.get("source_readiness_acceptance") == SOURCE_READINESS_ACCEPTANCE,
                    "REVIEWED_SOURCE_READINESS_ACCEPTANCE_REQUIRED_BEFORE_COMPILE")
            require(args.freeze_dir is not None and args.reviewed_freeze_bundle_sha256 is not None,
                    "ACTUAL_FREEZE_OUTPUT_REVIEW_REQUIRED_BEFORE_COMPILE")
            freeze = args.freeze_dir.resolve()
            require(freeze.is_dir() and freeze.is_relative_to(output_root), "FROZEN_OUTPUT_PATH")
            bundle_path = freeze / "freeze_bundle.json"
            require(sha256(bundle_path).lower() == args.reviewed_freeze_bundle_sha256.lower(),
                    "REVIEWED_FREEZE_BUNDLE_HASH_MISMATCH")
            bundle = json.loads(bundle_path.read_text(encoding="utf-8"))
            require(bundle["status"] == "FROZEN_ACTUAL_OUTPUTS_AWAIT_ROOT_REVIEW"
                    and bundle["source_manifest_sha256"] == manifest_hash, "FREEZE_SOURCE_BINDING_MISMATCH")
            require(set(bundle["files_sha256"]) ==
                    {"calibration.tsv", "geometry.jsonl", "clock.tsv", "whole-cases.tsv", "whole-windows.tsv", "whole-case-source-hold.json", "freeze_receipt.json"},
                    "FREEZE_BUNDLE_FILES")
            for name, expected in bundle["files_sha256"].items():
                require(sha256(freeze / name) == expected, "REVIEWED_REFERENCE_CHANGED")
            receipt = json.loads((freeze / "freeze_receipt.json").read_text(encoding="utf-8"))
            require(receipt["rows"] == 1125 and receipt["clock_queries"] <= 4096
                    and receipt["vendor_invoked"] is False, "FROZEN_REFERENCE_DOMAIN")
            cl = checked_file(args.cl_path)
            require(sha256(cl).upper() == "88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D", "UNREVIEWED_COMPILER_ARTIFACT")
            # Exclusive one-use marker: a failed compile/calibration is retained,
            # never silently retried against the same reviewed bundle.
            dump_new(freeze / "compile-calibrate-consumed.json",
                     {"run": str(run), "source_manifest_sha256": manifest_hash,
                      "reviewed_freeze_bundle_sha256": args.reviewed_freeze_bundle_sha256})
            binary = run / "quality-calibration.exe"
            command = [str(cl), "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/MD",
                       "/permissive-", "/fp:strict", "/O2", "/I" + str(repo / "src"),
                       str(BASE / "quality_calibration.cpp"), "/Fe:" + str(binary),
                       "/Fo:" + str(run / "quality-calibration.obj"),
                       "/link", "/SUBSYSTEM:CONSOLE"]
            record["compiler"] = {
                "explicit_resolved_path": str(cl), "executable_sha256_before": sha256(cl),
                "profile": {"family_required_by_source": "MSVC_NOT_CLANG", "language": "C++20",
                            "runtime": "MD", "floating_point": "strict", "optimization": "O2",
                            "warnings": "W4_WX", "permissive": False,
                            "link_subsystem_request": "CONSOLE"},
                "exact_compile_argv": command}
            compile_result = hidden_child(command, run, run, "compile", args.timeout_seconds, env)
            record["children"].append({"name": "one_compile", **compile_result})
            record["compiler"]["executable_sha256_after"] = sha256(cl)
            require(record["compiler"]["executable_sha256_before"]
                    == record["compiler"]["executable_sha256_after"], "COMPILER_CHANGED_DURING_COMPILE")
            require(compile_result["raw_exit_code"] == 0 and not compile_result["timed_out"],
                    "COMPILE_CHILD_FAILED_NO_AUTOMATIC_RETRY")
            require(binary.is_file(), "COMPILER_RETURNED_WITHOUT_BINARY")
            record["linked_pe"] = verify_console_pe(binary)
            record["binary_sha256"] = sha256(binary)
            calibration_result = hidden_child(
                [str(binary), str(freeze / "calibration.tsv"), str(freeze / "clock.tsv"),
                 str(freeze / "whole-cases.tsv"), str(freeze / "whole-windows.tsv"),
                 str(run / "calibration"), str(receipt["rows"]), str(receipt["clock_queries"])],
                run, run, "calibration", args.timeout_seconds, env)
            record["children"].append({"name": "one_calibration", **calibration_result})
            require(sha256(binary) == record["binary_sha256"], "CALIBRATION_BINARY_CHANGED")
            record["reviewed_freeze_bundle_sha256"] = args.reviewed_freeze_bundle_sha256
            require(calibration_result["raw_exit_code"] == 0 and not calibration_result["timed_out"],
                    "CALIBRATION_CHILD_FAILED_NO_AUTOMATIC_RETRY")
            summary = json.loads((run / "calibration" / "summary.json").read_text(encoding="utf-8"))
            require(summary["status"] == "INSTRUMENT_CALIBRATED_ANALYTIC_FIXTURES_ONLY"
                    and summary["vendor_candidate_invoked"] is False
                    and summary["production_admission"] is False, "CALIBRATION_SUMMARY_DOMAIN")
            record.update(status="ANALYTIC_INSTRUMENT_CALIBRATION_COMPLETED_ONLY", summary=summary)
        _, after_hash = verify_manifest(repo)
        require(after_hash == manifest_hash, "SOURCE_CHANGED_DURING_STAGE")
    except Exception as error:
        record.update(status="FAILED_OR_UNREACHED_NO_CERTIFICATION", error=str(error))
        dump_new(run / "stage_receipt.json", record)
        print(json.dumps(record, sort_keys=True))
        return 2
    dump_new(run / "stage_receipt.json", record)
    print(json.dumps(record, sort_keys=True))
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", choices=("Freeze", "CompileCalibrate"))
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--python-path", type=Path, required=True)
    parser.add_argument("--cl-path", type=Path)
    parser.add_argument("--toolchain-env-json", type=Path)
    parser.add_argument("--output-root", type=Path, default=BASE / "root-runs")
    parser.add_argument("--freeze-dir", type=Path)
    parser.add_argument("--reviewed-freeze-bundle-sha256")
    parser.add_argument("--timeout-seconds", type=int, default=1800)
    raise SystemExit(main(parser.parse_args()))
