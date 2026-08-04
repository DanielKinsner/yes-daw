# tools/verify-hardware.ps1 - H17 packaged hardware verifier (U1: verdict policy + device-free -SelfTest).
#
# This is the package-root orchestrator for the Windows portable alpha. In U1 only the shared
# verdict policy and its -SelfTest route exist. The normal (no-argument) path will validate the
# package manifest and orchestrate the packaged playback/recording/frame checkers once U2-U5 land;
# until then it reports an honest setup-incomplete verdict and exits 2. It NEVER fabricates a PASS.
#
# Policy mirror: src/app/HardwareVerification.h is the C++ authority for schema v1 - stage states,
# claim levels, stable failure codes, child-document acceptance, the pass-record consistency
# policy, and the KTD11 aggregate verdict (any measured fail -> exit 1; else any
# setup/crash/skipped -> exit 2; only all-pass -> exit 0). This script reimplements that policy in
# PowerShell. Both replay the SAME fixtures in tests/fixtures/hardware-verification/ and both
# hardcode the same fixture list, so the two implementations cannot drift silently. Change policy
# on one side => change the other side and the fixtures in the same commit.
#
# Windows PowerShell 5.1 compatible. ASCII only, no BOM (see docs/solutions/h0-build-and-ci-gotchas.md:
# a single em-dash once made this whole file unparseable under powershell.exe).
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\verify-hardware.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\verify-hardware.ps1 -SelfTest [-FixtureDir <dir>]
#
# Exit codes (R10): 0 = every required check passed; 1 = a completed measurement violated a gate;
# 2 = no measured failure, but setup/integrity/crash/incompleteness prevented a complete verdict.
# For -SelfTest itself: 0 = every fixture agrees with the policy, 1 = at least one disagreement.

param(
  [switch] $SelfTest,
  [string] $FixtureDir
)
$ErrorActionPreference = 'Stop'

# --- Schema v1 constants (mirror of src/app/HardwareVerification.h) ---------------------------
$SchemaVersion = 1
$RequiredSampleRateHz = 48000
$MaxGrantedBlockFrames = 128
$StageNames = @('playback', 'recording', 'frame')

# The committed fixture set, alphabetical. tests/hardware_verification_tests.cpp hardcodes the
# same list; adding a fixture means updating BOTH lists in the same commit.
$FixtureNames = @(
  'capture-only-pass',
  'child-crash',
  'child-invalid-result',
  'child-missing-result',
  'child-version-mismatch',
  'child-wrong-run-id',
  'child-wrong-schema',
  'child-wrong-stage',
  'clean-pass',
  'frame-claim-mismatch',
  'invented-alignment-rejected',
  'measured-fail-retains-timeout',
  'mixed-fail-and-setup',
  'pass-contradicts-measurement',
  'setup-incomplete',
  'stage-skipped'
)

# --- Small helpers -----------------------------------------------------------------------------
function Test-Prop {
  param($Object, [string] $Name)
  return ($null -ne $Object -and $Object.PSObject.Properties.Name -contains $Name)
}
function Test-JsonNumber {
  param($Value)
  return ($Value -is [int] -or $Value -is [long] -or $Value -is [double] -or $Value -is [decimal])
}
function ConvertTo-SafeArray {
  param($Value)
  if ($null -eq $Value) { return @() }
  return @($Value)
}

function New-StageRecord {
  param([string] $Stage, [string] $State, [string] $Claim, $Measurements, $FailureCodes, [string] $Detail)
  return @{
    stage         = $Stage
    state         = $State
    claim_level   = $Claim
    measurements  = $Measurements
    failure_codes = ConvertTo-SafeArray $FailureCodes
    detail        = $Detail
  }
}

function New-SynthesizedRecord {
  param([string] $Stage, [string] $State, [string] $FailureCode, [string] $Detail)
  $codes = @()
  if ($FailureCode -ne '') { $codes = @($FailureCode) }
  return New-StageRecord -Stage $Stage -State $State -Claim '' -Measurements $null -FailureCodes $codes -Detail $Detail
}

# --- Child-document acceptance (validation order is part of the contract; mirror exactly) -------
function ConvertTo-StageRecord {
  param($Document, [string] $ExpectedRunId, [string] $ExpectedStage, [string] $ExpectedCheckerVersion)

  function Reject([string] $code, [string] $why) {
    return New-SynthesizedRecord -Stage $ExpectedStage -State 'crash' -FailureCode $code -Detail $why
  }

  if ($null -eq $Document) { return Reject 'child_invalid_result' 'child result is not a JSON object' }

  if (-not (Test-Prop $Document 'schema_version') -or -not (Test-JsonNumber $Document.schema_version)) {
    return Reject 'child_invalid_result' 'child result has no numeric schema_version'
  }
  if ([int] $Document.schema_version -ne $SchemaVersion) {
    return Reject 'child_wrong_schema' ('child schema_version is not ' + $SchemaVersion)
  }

  if (-not (Test-Prop $Document 'run_id') -or $Document.run_id -isnot [string] -or $Document.run_id -eq '') {
    return Reject 'child_invalid_result' 'child result has no run_id'
  }
  if ($Document.run_id -cne $ExpectedRunId) {
    return Reject 'child_wrong_run_id' 'child run_id does not match this invocation'
  }

  if (-not (Test-Prop $Document 'stage') -or $Document.stage -isnot [string] -or $Document.stage -eq '') {
    return Reject 'child_invalid_result' 'child result has no stage name'
  }
  if ($Document.stage -cne $ExpectedStage) {
    return Reject 'child_wrong_stage' 'child stage does not match this invocation'
  }

  if (-not (Test-Prop $Document 'checker_version') -or $Document.checker_version -isnot [string] -or $Document.checker_version -eq '') {
    return Reject 'child_invalid_result' 'child result has no checker_version'
  }
  if ($Document.checker_version -cne $ExpectedCheckerVersion) {
    return Reject 'child_version_mismatch' 'child checker_version does not match the package'
  }

  # A child may never report crash/skipped about itself - those are orchestrator judgments.
  if (-not (Test-Prop $Document 'state') -or $Document.state -isnot [string] -or
      @('pass', 'fail', 'setup') -cnotcontains $Document.state) {
    return Reject 'child_invalid_result' 'child state must be pass, fail, or setup'
  }

  if (-not (Test-Prop $Document 'started_at') -or $Document.started_at -isnot [string] -or $Document.started_at -eq '' -or
      -not (Test-Prop $Document 'completed_at') -or $Document.completed_at -isnot [string] -or $Document.completed_at -eq '') {
    return Reject 'child_invalid_result' 'child result is missing started_at/completed_at'
  }

  if (-not (Test-Prop $Document 'duration_ms') -or -not (Test-JsonNumber $Document.duration_ms) -or
      [double] $Document.duration_ms -lt 0) {
    return Reject 'child_invalid_result' 'child result has no valid duration_ms'
  }

  $measurements = $null
  if (Test-Prop $Document 'measurements') {
    if ($Document.measurements -isnot [System.Management.Automation.PSCustomObject]) {
      return Reject 'child_invalid_result' 'child measurements is not an object'
    }
    $measurements = $Document.measurements
  }

  $failureCodes = @()
  if (Test-Prop $Document 'failure_codes') {
    foreach ($code in (ConvertTo-SafeArray $Document.failure_codes)) {
      if ($code -isnot [string]) {
        return Reject 'child_invalid_result' 'child failure_codes entry is not a string'
      }
      $failureCodes += $code
    }
  }

  # A fail/setup verdict with no named reason is unattributable evidence - reject it.
  if (($Document.state -ceq 'fail' -or $Document.state -ceq 'setup') -and $failureCodes.Count -eq 0) {
    return Reject 'child_invalid_result' 'child fail/setup carries no failure code'
  }

  $claim = ''
  if ((Test-Prop $Document 'claim_level') -and $Document.claim_level -is [string]) { $claim = $Document.claim_level }
  $detail = ''
  if ((Test-Prop $Document 'detail') -and $Document.detail -is [string]) { $detail = $Document.detail }

  return New-StageRecord -Stage $ExpectedStage -State $Document.state -Claim $claim `
    -Measurements $measurements -FailureCodes $failureCodes -Detail $detail
}

# --- Pass-record consistency policy (mirror of normalizeStageRecord) ----------------------------
function Invoke-StageRecordPolicy {
  param($Record)
  if ($Record.state -cne 'pass') { return }

  $m = $Record.measurements

  if ($Record.stage -ceq 'playback') {
    if ($Record.claim_level -cne 'locked_playback') {
      $Record.state = 'crash'
      $Record.failure_codes = @($Record.failure_codes) + 'claim_invalid'
      return
    }
    $hasRate = (Test-Prop $m 'granted_sample_rate_hz') -and (Test-JsonNumber $m.granted_sample_rate_hz)
    $hasBlock = (Test-Prop $m 'granted_block_frames') -and (Test-JsonNumber $m.granted_block_frames)
    if (-not $hasRate -or -not $hasBlock) {
      $Record.state = 'crash'
      $Record.failure_codes = @($Record.failure_codes) + 'playback_evidence_missing'
      return
    }
    # A completed measurement violating the locked gate is a measured FAIL, never setup (R14).
    if ([double] $m.granted_sample_rate_hz -ne $RequiredSampleRateHz) {
      $Record.state = 'fail'
      $Record.failure_codes = @($Record.failure_codes) + 'playback_wrong_sample_rate'
      return
    }
    if ([double] $m.granted_block_frames -gt $MaxGrantedBlockFrames) {
      $Record.state = 'fail'
      $Record.failure_codes = @($Record.failure_codes) + 'playback_block_exceeds_target'
      return
    }
    return
  }

  if ($Record.stage -ceq 'recording') {
    $alignmentStatus = ''
    if ((Test-Prop $m 'alignment_status') -and $m.alignment_status -is [string]) { $alignmentStatus = $m.alignment_status }

    if ($Record.claim_level -ceq 'capture_only') {
      # Capture-only must say so and must NOT carry an alignment value (R17: no invented zero).
      if ($alignmentStatus -cne 'not_claimed' -or (Test-Prop $m 'alignment_frames')) {
        $Record.state = 'crash'
        $Record.failure_codes = @($Record.failure_codes) + 'recording_invented_alignment'
      }
      return
    }
    if ($Record.claim_level -ceq 'full_alignment') {
      $route = ''
      if ((Test-Prop $m 'input_route') -and $m.input_route -is [string]) { $route = $m.input_route }
      $hasAlignment = (Test-Prop $m 'alignment_frames') -and (Test-JsonNumber $m.alignment_frames)
      if ($alignmentStatus -cne 'claimed' -or $route -cne 'device_loopback' -or -not $hasAlignment) {
        $Record.state = 'crash'
        $Record.failure_codes = @($Record.failure_codes) + 'recording_alignment_unproved'
      }
      return
    }
    $Record.state = 'crash'
    $Record.failure_codes = @($Record.failure_codes) + 'claim_invalid'
    return
  }

  if ($Record.stage -ceq 'frame') {
    # The packaged frame stage proves the headless dense-Timeline proxy, never window/GPU (R19).
    if ($Record.claim_level -cne 'headless_dense_timeline') {
      $Record.state = 'crash'
      $Record.failure_codes = @($Record.failure_codes) + 'frame_claim_mismatch'
    }
    return
  }
}

# --- KTD11 aggregate verdict (mirror of aggregateStages) ----------------------------------------
function Get-AggregateVerdict {
  param($Records)
  $records = ConvertTo-SafeArray $Records

  $incomplete = @{ overall_state = 'setup'; exit_code = 2; failure_codes = @('aggregate_incomplete') }

  $ordered = @()
  foreach ($name in $StageNames) {
    $matches0 = @($records | Where-Object { $_.stage -ceq $name })
    if ($matches0.Count -ne 1) { return $incomplete }   # missing or duplicate stage
    $ordered += $matches0[0]
  }
  if ($records.Count -ne $ordered.Count) { return $incomplete }   # unknown extra stage

  $anyFail = $false
  $anyIncomplete = $false
  $codes = New-Object System.Collections.Generic.List[string]
  foreach ($r in $ordered) {
    if ($r.state -ceq 'fail') { $anyFail = $true }
    if (@('setup', 'crash', 'skipped') -ccontains $r.state) { $anyIncomplete = $true }
    foreach ($c in (ConvertTo-SafeArray $r.failure_codes)) {
      if (-not $codes.Contains($c)) { [void] $codes.Add($c) }
    }
  }

  $overall = 'pass'
  if ($anyFail) { $overall = 'fail' }
  elseif ($anyIncomplete) { $overall = 'setup' }
  $exit = 0
  if ($overall -ceq 'fail') { $exit = 1 }
  elseif ($overall -ceq 'setup') { $exit = 2 }

  return @{ overall_state = $overall; exit_code = $exit; failure_codes = @($codes) }
}

# --- Orchestrator-side child handling shared by the (future) normal path and the fixtures -------
function ConvertTo-ChildRecord {
  param($Child, [string] $RunId, [string] $CheckerVersion)
  $slot = [string] $Child.stage
  switch ([string] $Child.outcome) {
    'document' {
      $record = ConvertTo-StageRecord -Document $Child.document -ExpectedRunId $RunId `
        -ExpectedStage $slot -ExpectedCheckerVersion $CheckerVersion
      Invoke-StageRecordPolicy -Record $record
      return $record
    }
    'timeout' { return New-SynthesizedRecord -Stage $slot -State 'crash' -FailureCode 'child_timeout' -Detail 'child hit its timeout' }
    'crash'   { return New-SynthesizedRecord -Stage $slot -State 'crash' -FailureCode 'child_crash' -Detail 'child exited abnormally' }
    'missing' { return New-SynthesizedRecord -Stage $slot -State 'crash' -FailureCode 'child_missing_result' -Detail 'child wrote no result' }
    'skipped' { return New-SynthesizedRecord -Stage $slot -State 'skipped' -FailureCode '' -Detail 'stage skipped' }
    default   { throw ('fixture child has unknown outcome: ' + $Child.outcome) }
  }
}

# --- -SelfTest: replay the committed fixtures through the SAME policy functions -----------------
function Invoke-SelfTest {
  if (-not $FixtureDir) {
    $repoRoot = Split-Path -Parent $PSScriptRoot
    $script:FixtureDir = Join-Path $repoRoot 'tests\fixtures\hardware-verification'
  }
  if (-not (Test-Path -LiteralPath $script:FixtureDir)) {
    Write-Host ('selftest: FAIL - fixture directory not found: ' + $script:FixtureDir)
    return 1
  }

  # Exact-set check: a fixture added on only one side of the C++/PowerShell mirror fails here.
  $present = @(Get-ChildItem -LiteralPath $script:FixtureDir -Filter '*.json' |
    ForEach-Object { $_.BaseName } | Sort-Object)
  $wanted = @($FixtureNames | Sort-Object)
  if (($present -join '|') -cne ($wanted -join '|')) {
    Write-Host 'selftest: FAIL - fixture set does not match the hardcoded lockstep list:'
    Write-Host ('  on disk : ' + ($present -join ', '))
    Write-Host ('  expected: ' + ($wanted -join ', '))
    return 1
  }

  $failures = 0
  foreach ($name in $FixtureNames) {
    $path = Join-Path $script:FixtureDir ($name + '.json')
    $fixture = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json

    $records = @()
    foreach ($child in (ConvertTo-SafeArray $fixture.children)) {
      $records += , (ConvertTo-ChildRecord -Child $child -RunId $fixture.run_id -CheckerVersion $fixture.checker_version)
    }
    $verdict = Get-AggregateVerdict -Records $records

    $problems = @()
    $expected = $fixture.expected
    if ($verdict.overall_state -cne [string] $expected.overall_state) {
      $problems += ('overall_state ' + $verdict.overall_state + ' != ' + $expected.overall_state)
    }
    if ($verdict.exit_code -ne [int] $expected.exit_code) {
      $problems += ('exit_code ' + $verdict.exit_code + ' != ' + $expected.exit_code)
    }
    foreach ($prop in $expected.stage_states.PSObject.Properties) {
      $record = @($records | Where-Object { $_.stage -ceq $prop.Name })
      if ($record.Count -ne 1) { $problems += ('stage ' + $prop.Name + ' not found once'); continue }
      if ($record[0].state -cne [string] $prop.Value) {
        $problems += ('stage ' + $prop.Name + ' state ' + $record[0].state + ' != ' + $prop.Value)
      }
    }
    $expectedCodes = ConvertTo-SafeArray $expected.failure_codes
    if ((@($verdict.failure_codes) -join '|') -cne ($expectedCodes -join '|')) {
      $problems += ('failure_codes [' + (@($verdict.failure_codes) -join ',') + '] != [' + ($expectedCodes -join ',') + ']')
    }
    if (Test-Prop $expected 'stage_claims') {
      foreach ($prop in $expected.stage_claims.PSObject.Properties) {
        $record = @($records | Where-Object { $_.stage -ceq $prop.Name })
        if ($record.Count -ne 1 -or $record[0].claim_level -cne [string] $prop.Value) {
          $problems += ('stage ' + $prop.Name + ' claim mismatch')
        }
      }
    }

    if ($problems.Count -eq 0) {
      Write-Host ('[PASS] ' + $name)
    }
    else {
      $failures += 1
      Write-Host ('[FAIL] ' + $name + ' : ' + ($problems -join '; '))
    }
  }

  Write-Host ('selftest: ' + ($FixtureNames.Count - $failures) + '/' + $FixtureNames.Count + ' fixtures agree with the verdict policy')
  if ($failures -eq 0) { return 0 }
  return 1
}

# --- Entry -------------------------------------------------------------------------------------
if ($SelfTest) {
  exit (Invoke-SelfTest)
}

# Normal path: the packaged stage checkers arrive in U2-U5. Until they are staged into the package
# this command cannot produce a hardware verdict, and it says so instead of inventing one (R10).
Write-Output 'verify-hardware: the packaged playback/recording/frame checkers are not shipped yet (U1 policy-only build).'
Write-Output 'verify-hardware: verdict = setup-incomplete. Run -SelfTest to exercise the verdict policy without hardware.'
exit 2
