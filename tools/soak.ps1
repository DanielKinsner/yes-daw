# YES DAW — H0 real-machine soak gate, native PowerShell (no Git Bash, no Python needed).
#
#   From PowerShell:   .\tools\soak.ps1 30
#   From cmd:          powershell -ExecutionPolicy Bypass -File tools\soak.ps1 30
#   Full 10-min gate:  .\tools\soak.ps1 600
#   Prove sound out:   .\tools\soak.ps1 600 -Loopback     (needs output wired back to an input)
#
# This is the AUDIO half of the H0 exit: a soak at a 128-frame Block (the roadmap target; -BlockSize to
# override) with zero dropouts; -Loopback also asserts sound physically left the device AND was 440 Hz.
# NOTE: the GPU-timeline 60 fps half (max_frame_ms < 16.6) is NOT YET implemented (needs the native
# render shell), so a PASS means the AUDIO gate only. Exit 0 = healthy, 1 = dropout/silence, 2 = setup.
param(
  [int]    $Seconds   = 600,
  [int]    $BlockSize = 128,
  [switch] $Loopback
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

# Find the built soak binary across the Visual Studio and Ninja layouts.
$candidates = @(
  "$repo\build\YesDawSoak_artefacts\Debug\YesDawSoak.exe",
  "$repo\build\YesDawSoak_artefacts\Release\YesDawSoak.exe",
  "$repo\build\Debug\YesDawSoak.exe",
  "$repo\build\Release\YesDawSoak.exe",
  "$repo\build\YesDawSoak.exe"
)
$bin = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $bin) {
  Write-Host "soak: YesDawSoak not built. Run  bootstrap\windows.ps1  first." -ForegroundColor Yellow
  exit 2
}

$stats = "$repo\soak-stats.json"
$soakArgs = @('--seconds', $Seconds, '--block-size', $BlockSize, '--stats-out', $stats)
if ($Loopback) { $soakArgs += '--loopback' }
& $bin @soakArgs

if (-not (Test-Path $stats)) { Write-Host "FAIL: soak wrote no stats (crash?)" -ForegroundColor Red; exit 2 }
$s = Get-Content $stats -Raw | ConvertFrom-Json
if ($s.setup_error) { Write-Host "FAIL: setup error: $($s.setup_error)" -ForegroundColor Red; exit 2 }

$problems = @()
# Device-reported xruns are authoritative when supported (>=0); -1 falls back to deadline misses.
if ($s.xruns -ge 0 -and $s.xruns -ne 0)                              { $problems += "xruns=$($s.xruns)" }
if ($s.deadline_misses -ne 0)                                        { $problems += "deadline_misses=$($s.deadline_misses)" }
if ($s.device_error)                                                 { $problems += "device_error" }
if ($s.block_budget_ms -gt 0 -and $s.max_block_ms -ge $s.block_budget_ms) { $problems += ("max_block_ms {0:N3} >= budget {1:N3}" -f $s.max_block_ms, $s.block_budget_ms) }
# The roadmap names a 128-frame Block; a larger forced block means the H0 target wasn't met.
if ($s.block_size -gt $s.requested_block_size) { $problems += ("block {0} > target {1} (device could not meet it — needs a low-latency/exclusive driver)" -f $s.block_size, $s.requested_block_size) }
if ($s.loopback) {
  if (-not ($s.loopback_peak_rms -gt 0.01)) { $problems += ("silent loopback (rms={0:N4})" -f $s.loopback_peak_rms) }
  # Non-silent isn't enough — the energy must be at 440 Hz (pure tone: 440_mag ~ sqrt(2)*rms).
  elseif (-not ($s.loopback_440_mag -gt (0.5 * $s.loopback_peak_rms))) { $problems += ("loopback not 440 Hz (440_mag={0:N4} vs rms={1:N4})" -f $s.loopback_440_mag, $s.loopback_peak_rms) }
}

Write-Host ('device="{0}" sr={1} block={2}/{3} xruns={4} deadline_misses={5} max_block_ms={6:N3}/{7:N3} loop_rms={8:N4} loop_440={9:N4}' -f `
  $s.device, $s.sample_rate, $s.block_size, $s.requested_block_size, $s.xruns, $s.deadline_misses, $s.max_block_ms, $s.block_budget_ms, $s.loopback_peak_rms, $s.loopback_440_mag)

if ($problems.Count -gt 0) { Write-Host ("FAIL: " + ($problems -join "; ")) -ForegroundColor Red; exit 1 }
Write-Host "PASS (audio gate; GPU 60 fps frame gate not yet implemented)" -ForegroundColor Green
exit 0
