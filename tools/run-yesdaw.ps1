# tools/run-yesdaw.ps1 - build (fresh) and launch YES DAW with one command (Windows).
#
# Phase 3 S3.1 (see docs/plans/2026-08-20-visual-parity-and-dogfood-execution-plan.md, D10).
# The one dogfood entrypoint: imports the MSVC environment itself (the "ninja says no work to do
# after a pull" trap is a missing vcvars64, not an up-to-date build - see the
# local-build-staleness-and-vcvars memory note), reconfigures + builds the `ci` Release preset,
# prints the exact source version + build timestamp, then launches the YesDaw GUI non-blocking.
# Exit 0 ONLY if the process actually started.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\run-yesdaw.ps1            build then launch
#   powershell -ExecutionPolicy Bypass -File tools\run-yesdaw.ps1 -NoBuild   launch build-ci/ as-is
param(
  [switch] $NoBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# --- MSVC environment (vcvars64) -------------------------------------------------------------
# cl.exe on PATH means a developer shell is already active; otherwise find vcvars64.bat via
# vswhere (any edition), fall back to the known 2022 install layouts, and import its variables.
function Import-VcVars {
  if (Get-Command cl.exe -ErrorAction SilentlyContinue) { return $true }

  $candidates = @()
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (Test-Path -LiteralPath $vswhere) {
    $install = & $vswhere -latest -products * `
      -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($install) { $candidates += (Join-Path $install.Trim() 'VC\Auxiliary\Build\vcvars64.bat') }
  }
  foreach ($base in @(${env:ProgramFiles(x86)}, ${env:ProgramFiles})) {
    foreach ($edition in @('BuildTools', 'Community', 'Professional', 'Enterprise')) {
      $candidates += (Join-Path $base "Microsoft Visual Studio\2022\$edition\VC\Auxiliary\Build\vcvars64.bat")
    }
  }

  foreach ($bat in $candidates) {
    if ($bat -and (Test-Path -LiteralPath $bat)) {
      cmd /c "`"$bat`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
          Set-Item -Path ("Env:" + $matches[1]) -Value $matches[2]
        }
      }
      return [bool] (Get-Command cl.exe -ErrorAction SilentlyContinue)
    }
  }
  return $false
}

# --- Version stamp ---------------------------------------------------------------------------
try {
  $version = (git -C $root describe --always --dirty 2>$null)
  if ($version) { $version = $version.Trim() }
} catch { $version = '' }
if (-not $version) { $version = 'unknown-nogit' }

# --- Build -----------------------------------------------------------------------------------
if (-not $NoBuild) {
  if (-not (Import-VcVars)) {
    Write-Error "[run] no MSVC toolchain found (vcvars64.bat) - run bootstrap/windows.ps1 first"
    exit 1
  }
  Write-Host "[run] building via ci preset (Release)..."
  Push-Location $root
  try {
    cmake --preset ci
    if ($LASTEXITCODE -ne 0) { Write-Error "[run] configure failed"; exit 1 }
    cmake --build --preset ci --target YesDaw
    if ($LASTEXITCODE -ne 0) { Write-Error "[run] build failed"; exit 1 }
  } finally { Pop-Location }
}

$exe = Join-Path $root 'build-ci\YesDaw_artefacts\Release\YesDaw.exe'
if (-not (Test-Path -LiteralPath $exe)) {
  Write-Error "[run] app not found: $exe (build first, or drop -NoBuild)"
  exit 1
}

$built = (Get-Item -LiteralPath $exe).LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss')
Write-Host "[run] version: $version"
Write-Host "[run] binary built: $built"

# --- Launch (non-blocking) -------------------------------------------------------------------
$proc = Start-Process -FilePath $exe -PassThru
Start-Sleep -Milliseconds 500
if ($null -eq $proc -or $proc.HasExited) {
  Write-Error "[run] YesDaw.exe did not stay running"
  exit 1
}
Write-Host "[run] PASS: YesDaw launched (pid $($proc.Id), version $version)"
exit 0
