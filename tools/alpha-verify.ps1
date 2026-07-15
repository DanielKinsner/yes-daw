# tools/alpha-verify.ps1 — H17 CP5 mechanical close for the alpha gate (Windows).
#
# PowerShell sibling of tools/alpha-verify.sh. Runs the objective sub-asserts from
# docs/alpha-gate.md against a produced project bundle + its exported WAV. Exit 0 iff every assert
# passes. The subjective "feel session" is separate and never gates. An agent never writes a
# reality-lane PASS row — this only runs mechanical checks.
#
# The 5 asserts (each with a named negative control exercised by -SelfTest):
#   1. export exists + non-empty        (control: an empty file)
#   2. export re-imports bit-exact      (control: a non-WAV / garbage file)  -> YesDawSelfCheck --verify-wav
#   3. integrated loudness in -30..-6   (control: out-of-range values + an unmeasurable file) -> --loudness
#   4. bundle reopens, zero validators  (control: a path that is not a bundle) -> --selfcheck
#   5. autosave snapshot present        (control: a bundle dir with no autosave)
#
# Honest scope: the full POSITIVE pass needs a real produced song (H17 CP2's committed demo bundle,
# or your own session mix). -SelfTest does NOT need one. CI runs -SelfTest.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\alpha-verify.ps1 -Bundle <b.yesdaw> -Wav <mix.wav> [-SelfCheck <path>]
#   powershell -ExecutionPolicy Bypass -File tools\alpha-verify.ps1 -SelfTest [-SelfCheck <path>]
param(
  [string] $Bundle,
  [string] $Wav,
  [string] $SelfCheck,
  [switch] $SelfTest
)
$ErrorActionPreference = 'Stop'

$LufsMin = -30.0
$LufsMax = -6.0

function Resolve-SelfCheck {
  if ($SelfCheck) { return $SelfCheck }
  $root = Split-Path -Parent $PSScriptRoot
  $candidate = Join-Path $root 'build-ci\YesDawSelfCheck_artefacts\Release\YesDawSelfCheck.exe'
  if (Test-Path -LiteralPath $candidate) { return $candidate }
  $onPath = Get-Command YesDawSelfCheck -ErrorAction SilentlyContinue
  if ($onPath) { return $onPath.Source }
  throw "YesDawSelfCheck not found (pass -SelfCheck)"
}

# Assert helpers: return $true = pass, $false = fail.
function Assert-ExportExists([string] $w) {
  return (Test-Path -LiteralPath $w) -and ((Get-Item -LiteralPath $w).Length -gt 0)
}
function Assert-ReimportBitExact([string] $w) {
  & $script:Sc --verify-wav $w *> $null
  return ($LASTEXITCODE -eq 0)
}
function Assert-ReopenSelfCheck([string] $b) {
  & $script:Sc --selfcheck $b *> $null
  return ($LASTEXITCODE -eq 0)
}
function Assert-AutosavePresent([string] $b) {
  return (Test-Path -LiteralPath (Join-Path $b 'autosave\last.yesdaw'))
}
function LoudnessInRange([double] $x) {
  return ($x -ge $LufsMin -and $x -le $LufsMax)
}
function Assert-LoudnessRange([string] $w) {
  $out = & $script:Sc --loudness $w 2>$null
  if ($LASTEXITCODE -ne 0) { return $false }
  $m = [regex]::Match(($out -join "`n"), 'integrated\s+(-?[0-9.]+)\s+LUFS')
  if (-not $m.Success) { return $false }
  return (LoudnessInRange ([double] $m.Groups[1].Value))
}

function Report([string] $name, [bool] $pass) {
  if ($pass) { Write-Host "  PASS  $name" } else { Write-Host "  FAIL  $name" }
}

function Invoke-Verify {
  if (-not $Bundle -or -not $Wav) { Write-Error "alpha-verify: -Bundle and -Wav are required"; exit 2 }
  $script:Sc = Resolve-SelfCheck
  Write-Host "alpha-verify: bundle=$Bundle wav=$Wav"
  $fails = 0

  $r = Assert-ExportExists     $Wav;    Report "export exists + non-empty" $r;                          if (-not $r) { $fails = 1 }
  $r = Assert-ReimportBitExact $Wav;    Report "export re-imports bit-exact" $r;                        if (-not $r) { $fails = 1 }
  $r = Assert-LoudnessRange    $Wav;    Report "integrated loudness in $LufsMin..$LufsMax LUFS" $r;     if (-not $r) { $fails = 1 }
  $r = Assert-ReopenSelfCheck  $Bundle; Report "bundle reopens, zero validator errors" $r;             if (-not $r) { $fails = 1 }
  $r = Assert-AutosavePresent  $Bundle; Report "autosave snapshot present" $r;                         if (-not $r) { $fails = 1 }

  if ($fails -eq 0) { Write-Host "ALPHA-VERIFY PASS"; exit 0 }
  Write-Host "ALPHA-VERIFY FAIL"; exit 1
}

function Invoke-SelfTest {
  $script:Sc = Resolve-SelfCheck
  $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("yesdaw-alpha-selftest-" + [System.Guid]::NewGuid().ToString('N'))
  New-Item -ItemType Directory -Force -Path $tmp | Out-Null
  $bad = 0
  try {
    function CheckFails([string] $name, [bool] $result) {
      # $result is the assert's return; it MUST be $false (the control must fail).
      if ($result) { Write-Host "  BAD   control did NOT fail: $name"; $script:bad = 1 }
      else         { Write-Host "  ok    control fails: $name" }
    }

    $empty = Join-Path $tmp 'empty.wav'; New-Item -ItemType File -Force -Path $empty | Out-Null
    CheckFails "empty export -> export-exists" (Assert-ExportExists $empty)

    $garbage = Join-Path $tmp 'garbage.wav'; Set-Content -LiteralPath $garbage -Value 'this is not a wav file'
    CheckFails "garbage file -> reimport-bit-exact" (Assert-ReimportBitExact $garbage)
    CheckFails "garbage file -> loudness (unmeasurable)" (Assert-LoudnessRange $garbage)

    if (LoudnessInRange -70.0) { Write-Host "  BAD   -70 LUFS accepted"; $bad = 1 } else { Write-Host "  ok    -70 LUFS rejected" }
    if (LoudnessInRange 0.0)   { Write-Host "  BAD   0 LUFS accepted";   $bad = 1 } else { Write-Host "  ok    0 LUFS rejected" }
    if (LoudnessInRange -12.0) { Write-Host "  ok    -12 LUFS accepted" }          else { Write-Host "  BAD   -12 LUFS rejected"; $bad = 1 }

    CheckFails "non-bundle dir -> reopen-selfcheck" (Assert-ReopenSelfCheck $tmp)

    $noAuto = Join-Path $tmp 'no-autosave-bundle'; New-Item -ItemType Directory -Force -Path $noAuto | Out-Null
    CheckFails "bundle without autosave -> autosave-present" (Assert-AutosavePresent $noAuto)
  } finally {
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
  }

  if ($bad -eq 0) { Write-Host "ALPHA-VERIFY SELF-TEST PASS"; exit 0 }
  Write-Host "ALPHA-VERIFY SELF-TEST FAIL"; exit 1
}

if ($SelfTest) { Invoke-SelfTest } else { Invoke-Verify }
