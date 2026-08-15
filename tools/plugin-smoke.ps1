# YES DAW — reality-lane Smoke 2: one real plugin across the worker boundary (M14; ADR-0037).
#
#   pwsh tools/plugin-smoke.ps1                      # first VST3 found in the OS folder
#   pwsh tools/plugin-smoke.ps1 -Plugin "C:\...\Thing.vst3"
#   pwsh tools/plugin-smoke.ps1 -Synthetic           # the in-repo synthetic worker plugin (CI path)
#
# Builds (if needed) and runs YesDawPluginSmoke, which launches the plugin-host worker child, loads
# ONE named plugin into it, processes real blocks through the OS-shared-memory RT lane and
# self-asserts: the worker stayed alive (no crash, no watchdog kill), no NaN/Inf came back, the
# plugin actually processed the signal, and the opaque state chunk round-trips.
#
# Prints PASS/FAIL and exits 0/1. Setup problems — no worker binary, no plugin installed, a file
# that will not load as a plugin — exit 2. Never "load it and listen".
param(
  [string] $Plugin = '',
  [switch] $Synthetic,
  [string] $BuildDir = 'build-ci'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$build = Join-Path $repoRoot $BuildDir
$exe = Join-Path $build 'YesDawPluginSmoke_artefacts/Release/YesDawPluginSmoke.exe'
$worker = Join-Path $build 'YesDawPluginHost_artefacts/Release/YesDawPluginHost.exe'

if (-not (Test-Path $exe) -or -not (Test-Path $worker)) {
  Write-Host 'building YesDawPluginSmoke...'
  ninja -C $build YesDawPluginSmoke | Out-Host
}
if (-not (Test-Path $exe)) {
  Write-Host 'SETUP: YesDawPluginSmoke binary missing after build'
  exit 2
}
if (-not (Test-Path $worker)) {
  Write-Host 'SETUP: YesDawPluginHost worker binary missing after build'
  exit 2
}

$smokeArgs = @('--worker', $worker)
if ($Synthetic) {
  $smokeArgs += '--synthetic'
} elseif ($Plugin -ne '') {
  $smokeArgs += @('--plugin', $Plugin)
}

& $exe @smokeArgs
exit $LASTEXITCODE
