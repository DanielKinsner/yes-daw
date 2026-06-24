# bootstrap/windows.ps1 — bare Windows machine -> "can build", one command, zero hardcoded paths.
#   Run:  powershell -ExecutionPolicy Bypass -File bootstrap\windows.ps1
# CI is the real gate; a local build is only for iteration speed and is optional. Idempotent (safe to re-run).
$ErrorActionPreference = 'Stop'

function Ensure-Pkg($id, $override) {
  $a = @('install','--id',$id,'-e','--accept-package-agreements','--accept-source-agreements')
  if ($override) { $a += @('--override', $override) }   # array-splat passes the spaced string as ONE arg
  winget @a                                             # (fixes the cmd-quoting break we hit by hand)
  # winget returns -1978335189 ("no applicable update found") when a package is already installed — not an error.
  if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) { throw "winget failed for $id ($LASTEXITCODE)" }
}

Ensure-Pkg 'Kitware.CMake'
Ensure-Pkg 'Git.Git'
Ensure-Pkg 'Microsoft.VisualStudio.2022.BuildTools' `
  '--passive --wait --includeRecommended --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100'

# winget updates PATH in the registry, but THIS shell started before that — so a just-installed cmake
# isn't on PATH yet. Refresh PATH in-session so cmake is callable without opening a new terminal.
$env:Path = [System.Environment]::GetEnvironmentVariable('Path','Machine') + ';' +
            [System.Environment]::GetEnvironmentVariable('Path','User')

# Repo root derived from this script's own location — never a typed absolute path.
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

# Visual Studio generator (default) finds MSVC automatically — no dev-prompt, no Ninja env dance.
cmake -S "$repo" -B "$repo\build"
cmake --build "$repo\build" --config Debug
Write-Host "`n[bootstrap] OK. Run:  $repo\build\YesDaw_artefacts\Debug\YesDaw.exe"
