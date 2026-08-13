# YES DAW — one-command shipped-path hardware record proof (E35).
#
#   pwsh tools/shipped-record-check.ps1 [-Seconds 3]
#
# Builds (if needed) and runs YesDawShippedRecordCheck, which drives the SAME model verbs the
# shipped Record button calls against the real audio device and self-asserts the committed take
# contains the played coded burst. Prints PASS/FAIL and exits 0/1; setup problems (no device,
# no inputs — a loopback route from an output into the recorded input is required) exit 2.
# Never "listen and check".
param(
  [double] $Seconds = 3.0,
  [string] $BuildDir = 'build-ci'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repoRoot "$BuildDir/YesDawShippedRecordCheck_artefacts/Release/YesDawShippedRecordCheck.exe"

if (-not (Test-Path $exe)) {
  Write-Host "building YesDawShippedRecordCheck..."
  ninja -C (Join-Path $repoRoot $BuildDir) YesDawShippedRecordCheck | Out-Host
}
if (-not (Test-Path $exe)) {
  Write-Host "SETUP: checker binary missing after build"
  exit 2
}

& $exe --seconds $Seconds
exit $LASTEXITCODE
