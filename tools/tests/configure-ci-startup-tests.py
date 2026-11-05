"""Exercise cache startup against an independent CLI and real CMake projects."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(arguments, folder, environment, timeout=50):
    return subprocess.run(
        arguments, cwd=folder, env=environment, capture_output=True, text=True,
        timeout=timeout, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0,
    )


def exercise(output, helper):
    cmake, ninja = shutil.which("cmake"), shutil.which("ninja")
    check(cmake and ninja, "CMake and Ninja are required")
    check(helper.is_file() and not output.exists(), "Use an existing helper and a new output directory")
    output.mkdir(parents=True)
    fixture = output / "cache-cli"
    fixture.mkdir()
    (fixture / "cache.c").write_text(r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
int main(int argc, char **argv) {
    const char *path = getenv("YESDAW_CACHE_TEST_CALLS");
    const char *status = getenv("YESDAW_CACHE_TEST_STARTUP");
    FILE *file;
    if (!path || !status || argc != 2) return 9;
    file = fopen(path, "a");
    if (!file) return 8;
    fprintf(file, "%s\n", argv[1]);
    if (fclose(file) != 0) return 8;
    if (strcmp(argv[1], "--show-stats") == 0) return 0;
    if (strcmp(argv[1], "--start-server") != 0) return 9;
    if (strcmp(status, "timeout") == 0) {
#ifdef _WIN32
        Sleep(32000);
#else
        sleep(32);
#endif
        return 0;
    }
    return atoi(status);
}
''', encoding="utf-8", newline="\n")
    (fixture / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.25)\n"
        "set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)\n"
        "project(CacheCli C)\nadd_executable(sccache cache.c)\n",
        encoding="utf-8", newline="\n",
    )
    for arguments in (
        [cmake, "-S", str(fixture), "-B", str(fixture / "build"), "-G", "Ninja",
         f"-DCMAKE_MAKE_PROGRAM={ninja}", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER_LAUNCHER="],
        [cmake, "--build", str(fixture / "build")],
    ):
        compiled = run(arguments, fixture, dict(os.environ))
        check(compiled.returncode == 0, f"Cache CLI fixture build failed: {compiled.stdout}\n{compiled.stderr}")
    executable = fixture / "build" / ("sccache.exe" if os.name == "nt" else "sccache")
    check(executable.is_file(), "Native cache CLI fixture missing")
    if os.name == "nt":
        image = executable.read_bytes()
        offset = struct.unpack_from("<I", image, 60)[0]
        check(struct.unpack_from("<H", image, offset + 4)[0] == 0x8664
              and struct.unpack_from("<H", image, offset + 24)[0] == 0x20B
              and struct.unpack_from("<H", image, offset + 92)[0] == 3,
              "Fixture must be an AMD64 console executable")
    cases = (
        ("misleading_stats", "2", False),
        ("ready", "0", True),
        ("missing", None, False),
        ("address_in_use", "2", False),
        ("timeout", "timeout", False),
        ("configure_failure", "0", True),
        ("invalid_policy", "0", False),
    )
    results = []
    for name, startup, cached in cases:
        folder = output / name
        binaries = folder / "bin"
        binaries.mkdir(parents=True)
        if startup is not None:
            shutil.copy2(executable, binaries / executable.name)
        project = (
            "cmake_minimum_required(VERSION 3.25)\nproject(CacheStartup NONE)\n"
            'file(WRITE "${CMAKE_BINARY_DIR}/launchers.txt" '
            '"${CMAKE_C_COMPILER_LAUNCHER}\\n${CMAKE_CXX_COMPILER_LAUNCHER}\\n")\n'
        )
        if name == "configure_failure":
            project += 'message(FATAL_ERROR "EXPECTED REAL CONFIGURE FAILURE")\n'
        (folder / "CMakeLists.txt").write_text(project, encoding="utf-8", newline="\n")
        (folder / "CMakePresets.json").write_text(json.dumps({
            "version": 6,
            "configurePresets": [{
                "name": "ci", "generator": "Ninja", "binaryDir": "${sourceDir}/build",
                "cacheVariables": {"CMAKE_MAKE_PROGRAM": ninja},
            }],
        }), encoding="utf-8")
        environment = dict(os.environ)
        environment["PATH"] = str(binaries)
        environment["GITHUB_OUTPUT"] = str(folder / "github-output.txt")
        environment["YESDAW_CACHE_TEST_CALLS"] = str(folder / "cache-calls.txt")
        environment["YESDAW_CACHE_TEST_STARTUP"] = startup or "2"
        if name == "misleading_stats":
            stats = run([str(binaries / executable.name), "--show-stats"], folder, environment)
            check(stats.returncode == 0, "Fixture must reproduce stats success without readiness")
        policy = "unknown" if name == "invalid_policy" else "startup"
        completed = run([cmake, f"-DYESDAW_CI_CACHE_PROBE={policy}", "-P", str(helper)], folder, environment)
        (folder / "stdout.log").write_text(completed.stdout, encoding="utf-8")
        (folder / "stderr.log").write_text(completed.stderr, encoding="utf-8")
        calls_path = folder / "cache-calls.txt"
        calls = [[line] for line in calls_path.read_text().splitlines()] if calls_path.exists() else []
        if name == "invalid_policy":
            check(completed.returncode != 0 and "Unknown CI cache probe policy" in completed.stderr,
                  "Unknown policy must refuse")
            check(not calls and not (folder / "build").exists(), "Unknown policy performed work")
        else:
            expected_calls = [["--show-stats"]] if name == "misleading_stats" else []
            if startup is not None:
                expected_calls.append(["--start-server"])
            check(calls == expected_calls, f"{name}: wrong real cache CLI invocation {calls}")
            launchers = (folder / "build/launchers.txt").read_text().splitlines()
            check(launchers == (["sccache", "sccache"] if cached else ["", ""]),
                  f"{name}: incorrect actual CMake compiler launchers {launchers}")
            receipt = (folder / "github-output.txt").read_text()
            check(receipt == f"cache_enabled={str(cached).lower()}\n", f"{name}: incorrect cache receipt")
            if name == "configure_failure":
                check(completed.returncode != 0 and "EXPECTED REAL CONFIGURE FAILURE" in completed.stderr,
                      "Real CMake failure was masked")
            else:
                check(completed.returncode == 0, f"{name}: {completed.stderr}")
        results.append({"case": name, "helper_exit": completed.returncode, "cache_calls": calls})
    (output / "receipt.json").write_text(json.dumps({"passed": len(results), "cases": results}, indent=2) + "\n",
                                        encoding="utf-8")
    print(f"PASS {len(results)} cache-startup scenarios with real CMake configuration")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--helper", type=Path, default=Path(__file__).resolve().parents[1] / "configure-ci.cmake")
    options = parser.parse_args()
    exercise(options.output.resolve(), options.helper.resolve())
