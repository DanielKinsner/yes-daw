# YES DAW - H11 visual-feel launch.
#
# One command for Dan's visual-feel spot-check. It builds the native app, then opens the current
# build-ci YesDaw executable.
param(
  [switch] $NoBuild
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if (-not $NoBuild) {
  Push-Location $root
  try {
    cmake --build --preset ci --target YesDaw
  } finally {
    Pop-Location
  }
}

$candidates = @(
  (Join-Path $root 'build-ci\YesDaw_artefacts\Release\YesDaw.exe'),
  (Join-Path $root 'build-ci\YesDaw_artefacts\Debug\YesDaw.exe')
)

foreach ($candidate in $candidates) {
  if (Test-Path -LiteralPath $candidate) {
    Start-Process -FilePath $candidate
    Write-Host "Launched $candidate"
    exit 0
  }
}

Write-Error 'YesDaw executable was not found under build-ci\YesDaw_artefacts.'
exit 1
