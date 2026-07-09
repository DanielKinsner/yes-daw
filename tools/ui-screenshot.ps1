# YES DAW - H16 CP8 mechanical UI screenshot capture.
#
# Builds and runs the headless screenshot harness. Exit 0 means a PNG was written and mechanically
# proved nonblank; no human visual judgment is part of this gate.
param(
  [string] $OutputDir = ''
)
$ErrorActionPreference = 'Stop'

$repo = Resolve-Path (Join-Path $PSScriptRoot '..')
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
  $resolvedOutputDir = Join-Path $repo 'build-ci\ui-screenshots'
} elseif ([System.IO.Path]::IsPathRooted($OutputDir)) {
  $resolvedOutputDir = $OutputDir
} else {
  $resolvedOutputDir = Join-Path $repo $OutputDir
}

New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null

Push-Location $repo
try {
  cmake --preset ci
  cmake --build --preset ci --target YesDawUiScreenshotCheck
  $env:YESDAW_UI_SCREENSHOT_DIR = $resolvedOutputDir
  ctest --preset ci -R YesDawUiScreenshotCheck --output-on-failure
  exit $LASTEXITCODE
} finally {
  Remove-Item Env:\YESDAW_UI_SCREENSHOT_DIR -ErrorAction SilentlyContinue
  Pop-Location
}
