# YES DAW - H8 real-device playback smoke.
#
# Runs the real hardware soak through PlaybackEngine instead of the H0 sine-only path. Exit 0 means the
# device accepted the requested Block, reported zero xruns/deadline misses, and (with -Loopback) captured
# the expected 440 Hz Project output.
param(
  [int]    $Seconds   = 600,
  [int]    $BlockSize = 128,
  [switch] $Loopback
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'soak.ps1') -Seconds $Seconds -BlockSize $BlockSize -Loopback:$Loopback -PlaybackProject
exit $LASTEXITCODE
