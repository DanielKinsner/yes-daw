"""Root-owned single hidden ABI attempt; no R10 execution authority."""
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
REVIEW = ROOT / "build-ci/20261105-managed-payload-abi-probe-independent-review-r1/review.json"
REVIEW_HASH = "5A91E3EED83085AC1CD7C040C61C5B25AE2F3E0902A8E8A87DA842B6FD449051"
MANIFEST_HASH = "A4DD60EDA30DB2F63566AC33015557F18BAA23BB6962F220E08659DC8A1EAD95"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def assert_pins():
    assert digest(REVIEW) == REVIEW_HASH
    assert digest(SOURCE / "source-pins.json") == MANIFEST_HASH
    for row in json.loads(REVIEW.read_text(encoding="utf-8"))["verified_files"]:
        path = Path(row["path"])
        if not path.is_absolute():
            path = ROOT / path
        assert path.stat().st_size == row["bytes"] and digest(path) == row["sha256"], str(path)


assert os.name == "nt"
assert not RUN.exists(), "Preserve every actual attempt"
assert_pins()
manifest = json.loads((SOURCE / "source-pins.json").read_text(encoding="utf-8"))
runtime = Path(manifest["runtime"]["root"])
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
command = [str(exe), "-NoLogo", "-NoProfile", "-NonInteractive", "-File", str(SOURCE / "run-probe.ps1")]
RUN.mkdir()
receipt_path = RUN / "receipt.json"
receipt = {"schema": "MANAGED_PAYLOAD_ABI_ACTUAL_ATTEMPT_R1", "first_green": False,
           "scope": "Separate finite managed allocation request ABI only; no R10 fit or GO",
           "source_manifest_sha256": MANIFEST_HASH, "source_review_sha256": REVIEW_HASH,
           "launcher_sha256": digest(Path(__file__)), "command": command,
           "environment_keys_only": sorted(environment), "inherited_environment": False,
           "console_pe_verified": True, "create_no_window": True, "hidden_window_style": True,
           "stdin_null": True, "redirected_output": True, "timeout_seconds": 60,
           "desktop_input": False, "native_windows": False, "devices": False,
           "R10_execution_authorized": False}


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

(RUN / "stdout.jsonl").write_bytes(stdout)
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
assert len(stdout) <= 262144
rows = [json.loads(line) for line in stdout.decode("utf-8-sig").splitlines()]
assert len(rows) == 178
measurements = [row for row in rows if row["kind"] == "measurement"]
assert len(measurements) == 156
expected = set()
for repeat in range(3):
    for family in ("Byte", "Char", "Int", "Reference", "String"):
        expected.update((family, size, repeat) for size in (1, 7, 17, 64, 435, 512))
    for family in ("BucketInt", "DictObject", "DictString"):
        expected.update((family, size, repeat) for size in (3, 7, 17, 37, 197, 1931))
    expected.update(("Hashtable", size, repeat) for size in (2, 3, 5, 10))
actual = [(row["family"], row["requested"], row["repeat"]) for row in measurements]
assert len(set(actual)) == 156 and set(actual) == expected
assert all(row["after"] - row["before"] == row["delta"] == row["source_predicted_delta"] for row in measurements)
assert sum(row["kind"] == "derived_array" for row in rows) == 12
assert sum(row["kind"] == "derived_dictionary" for row in rows) == 6
assert sum(row["kind"] == "derived_hashtable" for row in rows) == 3
summary = rows[-1]
assert summary["kind"] == "summary" and summary["exit"] == 0
assert summary["checks"] == 574 and summary["failures"] == 0
assert summary["measurement_rows"] == 156 and summary["repeats"] == 3
assert summary["allocation_budget"] == 8388608 and summary["final_gate_requires_raw_exit"] is True
receipt["summary"] = summary
receipt["first_green"] = True
receipt["complete_output_validated"] = True
save()
freeze = {"scope": "All four whole subject files including comments/helpers/docs, plus root launcher",
          "manifest_sha256": MANIFEST_HASH, "review_sha256": REVIEW_HASH,
          "files": [{"path": row["path"], "sha256": row["sha256"]} for row in manifest["files"]]
          + [{"path": "source-pins.json", "sha256": MANIFEST_HASH},
             {"path": "run-abi.py", "sha256": digest(Path(__file__))}]}
(RUN / "first-green-whole-source-freeze.json").write_text(json.dumps(freeze, indent=2)+"\n", encoding="utf-8", newline="\n")
print(json.dumps({"raw_exit": receipt["raw_exit"], "checks": summary["checks"],
                  "measurement_rows": len(measurements), "stderr_bytes": len(stderr),
                  "seconds": receipt["seconds"], "receipt_sha256": digest(receipt_path),
                  "scope": receipt["scope"]}))
