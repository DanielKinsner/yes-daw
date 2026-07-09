# YES DAW - H16 CP8 mechanical timeline frame smoke.
#
# Runs the existing headless Timeline GPU/frame-time proxy. Exit 0 means the dense timeline fixture stayed
# under the sustained 60 fps budget. This wrapper does not write docs/reality-lane.md.
param()
$ErrorActionPreference = 'Stop'

$repo = Resolve-Path (Join-Path $PSScriptRoot '..')

Push-Location $repo
try {
  cmake --preset ci
  cmake --build --preset ci --target YesDawTimelineGpuCheck
  ctest --preset ci -R YesDawTimelineGpuCheck --output-on-failure
  exit $LASTEXITCODE
} finally {
  Pop-Location
}
