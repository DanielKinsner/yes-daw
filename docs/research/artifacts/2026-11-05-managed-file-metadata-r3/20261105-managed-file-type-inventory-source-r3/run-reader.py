"""Root-owned single hidden metadata revision3 attempt; no R12 execution authority."""
from pathlib import Path
import ctypes
import hashlib
import json
import os
import re
import struct
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).resolve().parent
RUN = SOURCE / "execution-r1"
REVIEW = ROOT / "build-ci/20261105-managed-file-type-inventory-independent-review-r3/review.json"
REVIEW_HASH = "5E870B52977ED4F984953DF2E78CEDD012BC7C62554398D768F9F959D1271EBD"
MANIFEST_HASH = "3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def assert_pins():
    reference_profile = ROOT / "build-ci/20261105-managed-file-type-inventory-source-r1/compilation-reference-profile.json"
    assert digest(reference_profile) == "229900F0D91E29589B2DBE9584CDDC9812BFDB99625ED8924494BA093658C3F3"
    reference_rows = json.loads(reference_profile.read_text(encoding="utf-8"))["default_reference_file_pins"]
    expected_names = {Path(row["path"]).name for row in reference_rows}
    assert {file.name for file in Path("C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/ref").glob("*.dll")} == expected_names
    for reference_row in reference_rows:
        reference_path = Path(reference_row["path"])
        assert reference_path.stat().st_size == reference_row["bytes"] and digest(reference_path) == reference_row["sha256"]
    assert digest(REVIEW) == REVIEW_HASH
    assert digest(SOURCE / "source-pins.json") == MANIFEST_HASH
    host_review = ROOT / "build-ci/20261105-managed-payload-abi-probe-independent-review-r2/review.json"
    assert digest(host_review) == "57795D482F39EBDE2C965F6AB66BE533438EE71AC1590EDF820593ADB2014DFE"
    rows = json.loads(REVIEW.read_text(encoding="utf-8"))["verified_files"] + json.loads(host_review.read_text(encoding="utf-8"))["verified_files"]
    for row in rows:
        path = Path(row["path"])
        if not path.is_absolute():
            path = ROOT / path
        assert path.stat().st_size == row["bytes"] and digest(path) == row["sha256"], str(path)


if not __debug__:
    raise RuntimeError("UNOPTIMIZED_PYTHON_REQUIRED")
assert os.name == "nt"
assert not RUN.exists(), "Preserve every actual attempt"
assert_pins()
manifest = json.loads((SOURCE / "source-pins.json").read_text(encoding="utf-8"))
runtime = Path("C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell")
exe = runtime / "pwsh.exe"
pe = exe.read_bytes()
offset = struct.unpack_from("<I", pe, 60)[0]
assert pe[:2] == b"MZ" and pe[offset:offset+4] == b"PE\0\0"
assert struct.unpack_from("<H", pe, offset+4)[0] == 0x8664
assert struct.unpack_from("<H", pe, offset+24)[0] == 0x20B
assert struct.unpack_from("<H", pe, offset+92)[0] == 3, "Console subsystem required"

# Select only ordinary OS/user directories. No inherited CLR startup, profiler,
# GC/JIT/stress variables or secret values. No environment values are persisted.
allow = ("SystemRoot", "WINDIR", "ComSpec", "TEMP", "TMP", "USERPROFILE",
         "LOCALAPPDATA", "APPDATA", "ProgramFiles", "ProgramFiles(x86)", "ProgramData")
environment = {key: os.environ[key] for key in allow if key in os.environ}
environment["PATH"] = str(runtime) + os.pathsep + str(Path(environment["SystemRoot"]) / "System32")
command = [str(exe), "-NoLogo", "-NoProfile", "-NonInteractive", "-File", str(SOURCE / "run-reader.ps1")]
RUN.mkdir()
receipt_path = RUN / "receipt.json"
receipt = {"schema": "PUBLIC_FILE_METADATA_SOURCE_R3_ACTUAL_ATTEMPT_R1", "first_green": False,
           "scope": "Public fixed-file type metadata only; no target code loading, runtime layout or R12 GO",
           "source_manifest_sha256": MANIFEST_HASH, "source_review_sha256": REVIEW_HASH,
           "launcher_sha256": digest(Path(__file__)), "command": command,
           "environment_keys_only": sorted(environment), "inherited_environment": False,
           "console_pe_verified": True, "create_no_window": True, "hidden_window_style": True,
           "stdin_null": True, "redirected_output": True, "timeout_seconds": 60,
           "desktop_input": False, "native_windows": False, "devices": False,
           "R12_execution_authorized": False}


def save():
    receipt_path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8", newline="\n")


save()
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.SetErrorMode.argtypes = [ctypes.c_uint]
kernel.SetErrorMode.restype = ctypes.c_uint
old_mode = kernel.SetErrorMode(0x8003)
kernel.SetErrorMode(old_mode | 0x8003)
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = subprocess.SW_HIDE
start = time.monotonic()
try:
    with subprocess.Popen(command, cwd=ROOT, env=environment, stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          creationflags=subprocess.CREATE_NO_WINDOW, startupinfo=startup) as child:
        try:
            stdout, stderr = child.communicate(timeout=60)
            receipt["timeout"] = False
        except subprocess.TimeoutExpired:
            child.kill()
            stdout, stderr = child.communicate()
            receipt["timeout"] = True
        receipt["raw_exit"] = child.returncode
finally:
    kernel.SetErrorMode(old_mode)
    receipt["seconds"] = time.monotonic() - start
    save()

(RUN / "inventory.json").write_bytes(stdout)
(RUN / "stderr.log").write_bytes(stderr)
receipt["stdout_bytes"] = len(stdout)
receipt["stderr_bytes"] = len(stderr)
receipt["stdout_sha256"] = hashlib.sha256(stdout).hexdigest().upper()
receipt["stderr_sha256"] = hashlib.sha256(stderr).hexdigest().upper()
save()
assert_pins()
receipt["all_pre_post_pins_match"] = True
save()
assert not receipt["timeout"] and receipt["raw_exit"] == 0 and not stderr
assert len(stdout) <= 524288
inventory = json.loads(stdout.decode("utf-8-sig"))
assert inventory["status"] == "PASS_FILE_METADATA_ONLY"
assert inventory["schema"] == "FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V3"
assert inventory["required_named_types"] == 22 and inventory["optional_named_types"] == 3
assert inventory["required_generated_type_selections"] == 1
assert inventory["target_assemblies_explicitly_loaded_or_executed"] is False
assert len(inventory["nested_type_name_discoveries"]) == 1
discovery = inventory["nested_type_name_discoveries"][0]
assert discovery["file"] == "Newtonsoft.Json.dll" and discovery["parent"] == "Newtonsoft.Json.Linq.JObject"
generated = discovery["selected_generated_type"]
assert re.fullmatch(r"Newtonsoft\.Json\.Linq\.JObject\+<GetEnumerator>d__[0-9]+", generated)
assert generated in discovery["declared_nested_type_names"]
presence = inventory["optional_type_presence"]
assert len(presence) == 3 and {row["qualified_name"] for row in presence} == set(manifest["optional_named_types"])
assert all(row["file"] == "System.Private.CoreLib.dll" and type(row["present"]) is bool for row in presence)
expected = set(manifest["allowlist_required"]) | {generated} | {row["qualified_name"] for row in presence if row["present"]}
assert 23 <= inventory["reported_types"] <= 26
assert inventory["reported_types"] == len(inventory["types"]) == len(expected)
assert {row["qualified_name"] for row in inventory["types"]} == expected
assert all(len(row["declared_nonstatic_instance_fields"]) == row["declared_nonstatic_instance_field_count"] for row in inventory["types"])
receipt["first_green"] = True
receipt["complete_output_validated"] = True
receipt["reported_types"] = inventory["reported_types"]
save()
freeze = {"scope": "Whole authored file-metadata reader/source packet and both root runners; metadata only, not runtime layout or R12",
          "manifest_sha256": MANIFEST_HASH, "review_sha256": REVIEW_HASH,
          "files": [{"path": name, "sha256": digest(SOURCE/name)} for name in ("reader.cs", "README.md", "source-pins.json", "../20261105-author-metadata-r3-source.py", "../20261105-finalize-metadata-r3-source.py", "run-reader.ps1", "run-reader.py")]}
(RUN / "first-green-whole-source-freeze.json").write_text(json.dumps(freeze, indent=2)+"\n", encoding="utf-8", newline="\n")
print(json.dumps({"raw_exit": receipt["raw_exit"], "types": inventory["reported_types"], "stderr_bytes": len(stderr), "seconds": receipt["seconds"], "receipt_sha256": digest(receipt_path), "scope": receipt["scope"]}))
