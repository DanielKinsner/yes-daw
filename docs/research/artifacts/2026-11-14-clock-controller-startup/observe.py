"""Read managed environment dimensions after one isolated stock PowerShell startup."""

import sys

# Invoke CPython with -I -S -B -X pycache_prefix=<this absolute path>.
CACHE_PREFIX = r"C:\Users\Daniel Kinsner\AppData\Local\Temp\YesDawClockControllerObservation-attempt1\unused-pycache"
if sys.pycache_prefix != CACHE_PREFIX or not sys.dont_write_bytecode:
    raise SystemExit("expected the fixed unused Python cache prefix and -B")

import nt

try:
    nt.lstat(CACHE_PREFIX)
except FileNotFoundError:
    pass
else:
    raise SystemExit("the Python cache prefix already exists")

import hashlib
import json
from pathlib import Path
import re
import stat


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
PROFILE = Path("C:/Users/Daniel Kinsner")
LOCAL = PROFILE / "AppData/Local"
TEMP_PARENT = LOCAL / "Temp"
RUNTIME = TEMP_PARENT / "YesDawClockPowerShell-765"
ATTEMPT = TEMP_PARENT / "YesDawClockControllerObservation-attempt1"
PYTHON = LOCAL / "Programs/Python/Python313"
SYSTEM32 = Path("C:/Windows/System32")
MANIFEST = REPO / (
    "build-ci/musical-audio-persistent-planner-certification/source-revisions/"
    "clock-oracle-r22-guard-read-source-r1/source-manifest.json"
)
MANIFEST_SHA = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"
PROBE_SHA = "14B7821B79FC950F1B6504926FAACE1FD44507EA52085876AB58AA77A1E3CCB0"
PYTHON_PINS = {
    "python.exe": "5341746F92483A93E44C313DE830F2FBA2956F0759094404A16B2FED06C9A2ED",
    "python313.dll": "0565965617D94274D7F2C2958D0BEF33392CD9D2F346F99D8E1BEDBDF264EE85",
    "Lib/subprocess.py": "970207FDD712C92F7DC14D1623D2574F7E0910CEB0B5C37652A7A0850F35A396",
    "Lib/ctypes/__init__.py": "2E63F01AC6B8FFC1ACFA4EEC4C253EEF8FBD65E09506D690CB3BE71A2D78303A",
    "Lib/ctypes/_endian.py": "F2752AB2425DAC9BFAFF5AA1A3C6CD66AA342FEE0DB2F2A42DE568FC59D5CBEA",
    "DLLs/_ctypes.pyd": "697D05CAC7C167C00CCF22EA4FDBC7A8DB93AB9C6421061191558E42478068C5",
    "DLLs/libffi-8.dll": "EFF52743773EB550FCC6CE3EFC37C85724502233B6B002A35496D828BD7B280A",
}
KERNEL32_SHA = "E132EBA4166256E2F414B91FE7D5386590BC5EA8626AEE5AF9C9738383CF2AE1"
KEYS = (
    "SystemRoot", "windir", "ComSpec", "PATH", "TEMP", "TMP", "USERPROFILE",
    "LOCALAPPDATA", "APPDATA", "ProgramFiles", "ProgramData", "PSModulePath",
    "POWERSHELL_TELEMETRY_OPTOUT", "POWERSHELL_UPDATECHECK", "PATHEXT",
)

# Same startup switches and argument shape as R22, with this probe in place of run-clock.ps1.
PROBE = r"""param([string]$Stage,[string]$Repo,[string]$NewRunRoot,[string]$ReviewedManifestSha256)
$ErrorActionPreference = 'Stop'
$PSModuleAutoLoadingPreference = 'None'
try {
    $runtime = 'C:\Users\Daniel Kinsner\AppData\Local\Temp\YesDawClockPowerShell-765'
    function Match-Path([string]$actual,[string]$expected) {
        if ($actual.StartsWith('\\?\',[StringComparison]::Ordinal)) { $actual = $actual.Substring(4) }
        if (-not $actual.Equals($expected,[StringComparison]::OrdinalIgnoreCase)) { throw 'ORIGIN' }
    }
    if ($PSVersionTable.PSVersion.ToString() -cne '7.6.5' -or -not [Environment]::Is64BitProcess) { throw 'HOST' }
    Match-Path ([Environment]::ProcessPath) ([IO.Path]::Join($runtime,'pwsh.exe'))
    Match-Path $PSHOME $runtime
    Match-Path ([System.Management.Automation.PSObject].Assembly.Location) ([IO.Path]::Join($runtime,'System.Management.Automation.dll'))
    Match-Path ([Environment].Assembly.Location) ([IO.Path]::Join($runtime,'System.Private.CoreLib.dll'))
    if ($Stage -cne 'ClockOracle' -or $Repo.Length -eq 0 -or $NewRunRoot -cne $PSScriptRoot -or
        $ReviewedManifestSha256 -cne '28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06') { throw 'PARAMS' }
    $argv = [Environment]::GetCommandLineArgs()
    $expected = @('', '-NoLogo', '-NoProfile', '-NonInteractive', '-File', $PSCommandPath,
                  '-Stage', $Stage, '-Repo', $Repo, '-NewRunRoot', $NewRunRoot,
                  '-ReviewedManifestSha256', $ReviewedManifestSha256)
    if ($argv.Length -ne $expected.Length) { throw 'ARGV' }
    $apphost = [IO.Path]::GetFileName($argv[0])
    if ($apphost -notin @('pwsh.exe','pwsh.dll')) { throw 'ARGV_HOST' }
    Match-Path $argv[0] ([IO.Path]::Join($runtime,$apphost))
    for ($i = 1; $i -lt $expected.Length; ++$i) {
        if ($argv[$i] -cne $expected[$i]) { throw 'ARGV_TOKEN' }
    }

    $sma = 'Microsoft.PowerShell.'
    foreach ($short in @('Management','Utility')) {
        $name = $sma + $short
        $path = [IO.Path]::Join($runtime,'Modules',$name,($name + '.psd1'))
        Microsoft.PowerShell.Core\Import-Module -Name $path -ErrorAction Stop
        $module = @(Microsoft.PowerShell.Core\Get-Module -Name $name)
        if ($module.Count -ne 1 -or $module[0].Name -cne $name) { throw 'MODULE' }
        Match-Path $module[0].Path $path
    }
    foreach ($binding in @(@('Management','Get-Item','GetItemCommand'),
                           @('Utility','ConvertTo-Json','ConvertToJsonCommand'),
                           @('Utility','ConvertFrom-Json','ConvertFromJsonCommand'))) {
        $command = Microsoft.PowerShell.Core\Get-Command -Name ($sma + $binding[0] + '\' + $binding[1]) -CommandType Cmdlet -ErrorAction Stop
        if ($command.ImplementingType.FullName -cne ('Microsoft.PowerShell.Commands.' + $binding[2])) { throw 'CMDLET' }
        Match-Path $command.ImplementingType.Assembly.Location ([IO.Path]::Join($runtime,('Microsoft.PowerShell.Commands.' + $binding[0] + '.dll')))
    }

    $keys = @('SystemRoot','windir','ComSpec','PATH','TEMP','TMP','USERPROFILE','LOCALAPPDATA',
              'APPDATA','ProgramFiles','ProgramData','PSModulePath','POWERSHELL_TELEMETRY_OPTOUT',
              'POWERSHELL_UPDATECHECK','PATHEXT')
    $lengths = [Collections.Generic.Dictionary[string,int]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($key in $keys) { $lengths.Add($key,-1) }
    $count = 0L
    $keyUnits = 0L
    $valueUnits = 0L
    foreach ($row in [Environment]::GetEnvironmentVariables().GetEnumerator()) {
        $count++
        $keyUnits += $row.Key.Length
        $valueUnits += $row.Value.Length
        if ($lengths.ContainsKey($row.Key)) {
            if ($lengths[$row.Key] -ne -1) { throw 'DUPLICATE_SLOT' }
            $lengths[$row.Key] = $row.Value.Length
        }
    }
    $culture = [Globalization.CultureInfo]::InvariantCulture
    [Console]::WriteLine([string]::Format($culture,'TOTAL {0} {1} {2}',$count,$keyUnits,$valueUnits))
    foreach ($key in $keys) {
        [Console]::WriteLine([string]::Format($culture,'KEY {0} {1}',$key,$lengths[$key]))
    }
} catch {
    [Console]::Error.WriteLine('PROBE_REFUSED')
    exit 1
}
""".encode("ascii")


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def verify_file(path, expected, size=None):
    info = path.lstat()
    require(stat.S_ISREG(info.st_mode) and not info.st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT,
            f"expected ordinary file: {path.name}")
    require(size is None or info.st_size == size, f"file extent differs: {path.name}")
    with path.open("rb") as handle:
        require(hashlib.file_digest(handle, "sha256").hexdigest().upper() == expected,
                f"file hash differs: {path.name}")


def verify_runtime():
    raw = MANIFEST.read_bytes()
    require(digest(raw) == MANIFEST_SHA, "R22 manifest differs")
    manifest = json.loads(raw)
    root = RUNTIME.as_posix()
    prefix = root + "/"
    require(manifest["controller"]["path"] == prefix + "pwsh.exe"
            and RUNTIME.resolve(strict=True) == RUNTIME, "unexpected runtime origin")
    pins = [row for row in manifest["inputs"] if row["path"].startswith(prefix)]
    require(len(pins) == 323 and len({row["path"].casefold() for row in pins}) == 323,
            "unexpected runtime file inventory")
    for row in pins:
        verify_file(Path(row["path"]), row["sha256"], row["bytes"])
    directories = [row for row in manifest["closed_directory_inputs"]
                   if row["path"] == root or row["path"].startswith(prefix)]
    require(len(directories) == 4, "unexpected runtime directory inventory")
    for row in directories:
        base = Path(row["path"])
        require(base.resolve(strict=True) == base, "moved runtime directory")
        entries = list(base.rglob("*") if row["recursive"] else base.iterdir())
        require(all(not item.lstat().st_file_attributes & stat.FILE_ATTRIBUTE_REPARSE_POINT for item in entries),
                "redirected runtime member")
        names = {item.relative_to(base).as_posix() for item in entries if item.is_file()}
        require(len(entries) == row["entry_count"] and names == set(row["files"]),
                f"runtime membership differs: {base.name}")


def seed_environment():
    modules = (
        PROFILE / "Documents/PowerShell/Modules",
        Path("C:/Program Files/PowerShell/Modules"),
        RUNTIME / "Modules",
        SYSTEM32 / "WindowsPowerShell/v1.0/Modules",
    )
    seed = dict(zip(KEYS[:-1], (
        r"C:\Windows", r"C:\Windows", str(SYSTEM32 / "cmd.exe"), r"C:\Windows\System32;C:\Windows",
        str(ATTEMPT / "owned-temp"), str(ATTEMPT / "owned-temp"), str(PROFILE), str(LOCAL),
        str(PROFILE / "AppData/Roaming"), r"C:\Program Files", r"C:\ProgramData",
        ";".join(map(str, modules)), "1", "Off",
    ), strict=True))
    require(len(seed) == len({name.casefold() for name in seed}) == 14
            and all(name.isascii() and value.isascii() and "\0" not in value for name, value in seed.items()),
            "authored seed differs")
    return seed


def read_dimensions(raw):
    lines = raw.decode("ascii").splitlines()
    require(len(lines) == len(KEYS) + 1 and re.fullmatch(r"TOTAL [0-9]+ [0-9]+ [0-9]+", lines[0]),
            "unexpected probe report shape")
    count, key_units, value_units = map(int, lines[0].split()[1:])
    lengths = {}
    for key, line in zip(KEYS, lines[1:], strict=True):
        match = re.fullmatch(rf"KEY {key} (-1|[0-9]+)", line)
        require(match is not None, "unexpected probe key record")
        lengths[key] = int(match[1])
    present = {key: length for key, length in lengths.items() if length >= 0}
    other = [count - len(present), key_units - sum(map(len, present)), value_units - sum(present.values())]
    require(all(value >= 0 for value in other) and (other[0] > 0 or other == [0, 0, 0]),
            "inconsistent probe dimensions")
    return {"managed_count": count, "managed_key_utf16_units": key_units, "managed_value_utf16_units": value_units,
            "known_value_utf16_units": {key: present.get(key) for key in KEYS},
            "other_managed_count": other[0], "other_key_utf16_units": other[1], "other_value_utf16_units": other[2]}


def preflight():
    require(sys.platform == "win32" and sys.version_info[:3] == (3, 13, 5) and sys.maxsize == (1 << 63) - 1
            and sys.implementation.name == "cpython" and Path(sys.executable).resolve() == PYTHON / "python.exe"
            and Path(sys.base_prefix).resolve() == PYTHON, "expected pinned 64-bit CPython")
    require(sys.flags.isolated and sys.flags.no_site and sys.flags.ignore_environment and sys.dont_write_bytecode
            and not sys.flags.optimize and sys.argv[1:] in ([], ["--check"]), "unexpected CPython launch flags")
    require(nt.__spec__.origin == "built-in" and not (PYTHON / "python313.zip").exists(),
            "unexpected CPython startup source")
    try:
        nt.lstat(CACHE_PREFIX)
    except FileNotFoundError:
        pass
    else:
        raise ValueError("the Python cache prefix already exists")
    require(sys.getwindowsversion()[:3] == (10, 0, 26200)
            and sys.getwindowsversion().platform_version == (10, 0, 26100), "unexpected Windows profile")
    require(digest(PROBE) == PROBE_SHA, "probe text changed")
    for name, pin in PYTHON_PINS.items():
        verify_file(PYTHON / name, pin)
    verify_file(SYSTEM32 / "kernel32.dll", KERNEL32_SHA)
    verify_runtime()


def receive(process, api, os_handle, clock, sleep):
    # Only this thread ever reads these anonymous byte pipes. Peek with no
    # destination, then read at most its observed available bytes.
    handles = [os_handle(pipe.fileno()) for pipe in (process.stdout, process.stderr)]
    caps = [2048, 0]
    captures = [bytearray(), bytearray()]
    closed = [False, False]
    deadline = clock() + 30
    for _ in range(3000):
        if clock() >= deadline:
            break
        for i, handle in enumerate(handles):
            if closed[i]:
                continue
            try:
                available, _ = api.PeekNamedPipe(handle)
            except OSError as error:
                if error.winerror not in (109, 233):  # Broken or disconnected pipe.
                    raise
                closed[i] = True
                continue
            if available:
                request = min(available, caps[i] + 1 - len(captures[i]))
                data, error = api.ReadFile(handle, request)
                require(error == 0 and 0 < len(data) <= request, "unexpected byte pipe read")
                captures[i].extend(data)
                require(len(captures[i]) <= caps[i], "probe stream cap")
        exit_code = process.poll()
        if exit_code is not None and all(closed):
            require(exit_code == 0, f"probe refused: exit={exit_code}")
            return captures[0]
        remaining = deadline - clock()
        if remaining <= 0:
            break
        sleep(min(0.01, remaining))
    raise TimeoutError("probe time or poll limit")


def retire(process, subprocess):
    if process is None:
        return False
    try:
        if process.poll() is not None:
            return True
    except OSError:
        pass
    try:
        process.terminate()
    except OSError:
        pass
    try:
        return process.wait(timeout=2) is not None
    except (OSError, subprocess.TimeoutExpired, KeyboardInterrupt):
        return False


def observe(seed):
    import _ctypes
    import _winapi
    import ctypes
    import ctypes._endian
    import msvcrt
    import subprocess
    import time

    for module, name in (
        (_ctypes, "DLLs/_ctypes.pyd"), (ctypes, "Lib/ctypes/__init__.py"),
        (ctypes._endian, "Lib/ctypes/_endian.py"), (subprocess, "Lib/subprocess.py"),
    ):
        require(Path(module.__file__).resolve() == PYTHON / name, "unexpected launcher module origin")
    require(all(module.__spec__.origin == "built-in" for module in (_winapi, msvcrt, time))
            and TEMP_PARENT.resolve(strict=True) == TEMP_PARENT,
            "unexpected launch or scratch origin")
    ATTEMPT.mkdir()
    (ATTEMPT / "owned-temp").mkdir()
    probe = ATTEMPT / "probe.ps1"
    with probe.open("xb") as handle:
        handle.write(PROBE)
    verify_file(probe, PROBE_SHA, len(PROBE))
    exe = RUNTIME / "pwsh.exe"
    args = [str(exe), "-NoLogo", "-NoProfile", "-NonInteractive", "-File", str(probe),
            "-Stage", "ClockOracle", "-Repo", str(REPO), "-NewRunRoot", str(ATTEMPT),
            "-ReviewedManifestSha256", MANIFEST_SHA]
    require(len(args) == 14 and all(0 < len(arg) <= 512 and arg.isascii() for arg in args),
            "unexpected probe command extent")

    set_error_mode = ctypes.WinDLL(str(SYSTEM32 / "kernel32.dll")).SetErrorMode
    set_error_mode.argtypes = (ctypes.c_uint,)
    set_error_mode.restype = ctypes.c_uint
    process = None
    mode = set_error_mode(0x8003)
    try:
        try:
            process = subprocess.Popen(args, executable=str(exe), cwd=ATTEMPT, env=seed, bufsize=0,
                                       stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       close_fds=True, shell=False, creationflags=subprocess.CREATE_NO_WINDOW)
        finally:
            set_error_mode(mode)
        result = read_dimensions(receive(process, _winapi, msvcrt.get_osfhandle, time.monotonic, time.sleep))
    except BaseException as error:
        if not retire(process, subprocess):
            # Keep the sole attempt directory and disclose an unobserved exit.
            # The marker has no captured output. No next probe may reuse this path.
            marker = {"status": "QUARANTINED_EXIT_UNOBSERVED", "pid": None if process is None else process.pid}
            with (ATTEMPT / "quarantine.json").open("x", encoding="ascii") as output:
                json.dump(marker, output)
            raise RuntimeError("probe exit unobserved, attempt quarantined") from None
        raise RuntimeError(f"probe refused: {type(error).__name__}") from None
    finally:
        if process is not None:
            try:
                process.stdout.close()
            finally:
                process.stderr.close()
    verify_file(probe, PROBE_SHA, len(PROBE))
    preflight()
    return result


def main():
    preflight()
    seed = seed_environment()
    units = sum(len(key) + len(value) for key, value in seed.items())
    if sys.argv[1:]:
        print(f"PASS: source and disk pins, 323 runtime files, 4 directories, 14-key seed ({units} key/value UTF-16 units)")
        return
    dimensions = observe(seed)
    print(json.dumps({
        "probe_sha256": PROBE_SHA, "runtime_manifest_sha256": MANIFEST_SHA, "runtime_files": 323,
        "seed_entries": len(seed), "seed_key_value_utf16_units": units,
        "seed_logical_native_block_bytes": 2 * (units + 2 * len(seed) + 1),
        "scope": "Managed process dictionary at one endpoint after fixed imports. Native block and writer interval unqualified.",
        **dimensions,
    }, indent=2))


if __name__ == "__main__":
    main()
