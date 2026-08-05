# tools/verify-hardware.ps1 - H17 packaged hardware verifier (U1 policy + U5 orchestration).
#
# THE package-root command for the Windows portable alpha: unzip, open PowerShell in the package
# folder, run this with no arguments. It validates the package manifest, runs the packaged
# playback, recording, and headless dense-Timeline frame checkers with timeouts, and writes one
# structured aggregate + one plain summary + generated Reality-lane rows. Playback and recording
# will make a short, quiet sound on the default output device - that is information, not a prompt;
# nothing here ever asks a human to listen or judge.
#
# Policy mirror: src/app/HardwareVerification.h is the C++ authority for schema v1 - stage states,
# claim levels, stable failure codes, child-document acceptance, the pass-record consistency
# policy, and the KTD11 aggregate verdict (any measured fail -> exit 1; else any
# setup/crash/skipped -> exit 2; only all-pass -> exit 0). This script reimplements that policy in
# PowerShell. Both replay the SAME fixtures in tests/fixtures/hardware-verification/ (staged into
# the package as verify-fixtures/) and both hardcode the same fixture list, so the two
# implementations cannot drift silently. Change policy on one side => change the other side and
# the fixtures in the same commit.
#
# Package boundary (KTD5): every executable used for gate credit resolves LITERALLY under this
# script's own folder ($PSScriptRoot), must match package-manifest.json (relative path, byte size,
# SHA-256, compiled version), and PATH or a build tree is never searched. Integrity failure stops
# before any hardware launch and writes an exit-2 aggregate.
#
# Windows PowerShell 5.1 compatible. ASCII only, no BOM (see docs/solutions/h0-build-and-ci-gotchas.md:
# a single em-dash once made this whole file unparseable under powershell.exe).
#
# Usage:
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\verify-hardware.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\verify-hardware.ps1 -SelfTest [-FixtureDir <dir>]
#   powershell -NoProfile -ExecutionPolicy Bypass -File .\verify-hardware.ps1 -IntegrityOnly
#
# -SelfTest      device-free: replays the committed verdict fixtures through the SAME policy
#                functions the normal path uses, plus (when run from a real package) the package
#                mutation negative controls on disposable copies. Exit 0 = all agree, 1 = not.
# -IntegrityOnly internal/self-test surface: manifest validation only, stages recorded as skipped,
#                aggregate written, ALWAYS exits 2 (it can never produce a hardware verdict, so it
#                can never fabricate a PASS).
# -PlaybackSeconds / -RecordingSeconds set how long the measurements listen. They are durations,
# not thresholds - no flag on this script (or any checker) can relax a gate value.
#
# Exit codes (R10): 0 = every required stage passed its permitted claim; 1 = at least one
# completed measurement violated a gate; 2 = setup, package integrity, child crash/hang, or
# incomplete execution prevented a complete verdict.

param(
  [switch] $SelfTest,
  [string] $FixtureDir,
  [switch] $IntegrityOnly,
  [double] $PlaybackSeconds = 30,
  [double] $RecordingSeconds = 4
)
$ErrorActionPreference = 'Stop'

# --- Schema v1 constants (mirror of src/app/HardwareVerification.h) ---------------------------
$SchemaVersion = 1
$RequiredSampleRateHz = 48000
$MaxGrantedBlockFrames = 128
$StageNames = @('playback', 'recording', 'frame')

# Package-boundary failure codes (orchestrator-side; the R20 integrity vocabulary).
$CodeManifestMissing        = 'manifest_missing'
$CodeManifestInvalid        = 'manifest_invalid'
$CodeManifestFileMissing    = 'manifest_file_missing'
$CodeManifestPathEscape     = 'manifest_path_escape'
$CodeManifestSizeMismatch   = 'manifest_size_mismatch'
$CodeManifestHashMismatch   = 'manifest_hash_mismatch'
$CodeCheckerVersionMismatch = 'checker_version_mismatch'

# Stage child timeouts (seconds). Generous: a timeout means "synthesize crash and move on", never
# a relaxed verdict. Playback/recording get the measurement duration plus device-open headroom.
$FrameTimeoutSec = 300

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

# The three packaged stage checkers, in canonical run order (R21: later stages still run after an
# earlier failure so the run retains maximum evidence).
$StageCheckers = @(
  @{ Stage = 'playback';  Exe = 'YesDawHardwarePlaybackCheck.exe' },
  @{ Stage = 'recording'; Exe = 'YesDawHardwareRecordingCheck.exe' },
  @{ Stage = 'frame';     Exe = 'YesDawFrameCheck.exe' }
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
function Get-UtcNowIso {
  return (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd'T'HH:mm:ss'Z'")
}
function Get-FileSha256Hex {
  param([string] $Path)
  return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Write-JsonAtomic {
  param([string] $Path, $Object)
  $tmp = $Path + '.tmp'
  $Object | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $tmp -Encoding UTF8
  Move-Item -LiteralPath $tmp -Destination $Path -Force
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

# --- Orchestrator-side child handling shared by the normal path and the fixtures ----------------
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

# Launch one stage checker and classify what came back. The JSON document (not the exit code) is
# the evidence; exit codes only separate "wrote nothing and died" from "wrote nothing politely".
function Invoke-StageChild {
  param([string] $ExePath, [string] $Stage, [string] $RunId, [string] $OutJson,
        [int] $TimeoutSec, [string[]] $ExtraArgs, [string[]] $ArgsOverride)

  $argList = @('--run-id', $RunId, '--json-out', $OutJson) + (ConvertTo-SafeArray $ExtraArgs)
  # ArgsOverride exists ONLY for the self-test's hang-mechanics control, whose stand-in child
  # (powershell.exe) cannot accept the checker argument contract. The launch/wait/kill/classify
  # mechanics being proved are identical either way.
  if ((ConvertTo-SafeArray $ArgsOverride).Count -gt 0) { $argList = $ArgsOverride }
  $stdout = $OutJson + '.stdout.log'
  $stderr = $OutJson + '.stderr.log'

  # Windows PowerShell 5.1 does NOT quote array elements for Start-Process, so a results path
  # containing a space would shatter into several child arguments (observed live 2026-08-04: every
  # checker printed usage and exited under "C:\Users\Daniel Kinsner\..."). Quote explicitly.
  $quoted = foreach ($a in $argList) {
    if ($a -match '[\s"]') { '"' + ($a -replace '"', '\"') + '"' } else { $a }
  }
  $argString = $quoted -join ' '

  try {
    $process = Start-Process -FilePath $ExePath -ArgumentList $argString -PassThru -NoNewWindow `
      -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $null = $process.Handle   # cache the handle NOW or ExitCode reads $null after exit (PS 5.1)
  } catch {
    return @{ stage = $Stage; outcome = 'crash' }
  }

  if (-not $process.WaitForExit($TimeoutSec * 1000)) {
    try { $process.Kill() } catch {}
    try { [void] $process.WaitForExit(5000) } catch {}
    return @{ stage = $Stage; outcome = 'timeout' }
  }

  if (Test-Path -LiteralPath $OutJson) {
    $document = $null
    try { $document = Get-Content -LiteralPath $OutJson -Raw | ConvertFrom-Json } catch { $document = $null }
    return @{ stage = $Stage; outcome = 'document'; document = $document }
  }

  $exitCode = $process.ExitCode
  if ($null -eq $exitCode -or $exitCode -lt 0) { return @{ stage = $Stage; outcome = 'crash' } }
  return @{ stage = $Stage; outcome = 'missing' }
}

# --- Package integrity (KTD5) --------------------------------------------------------------------
function Test-PackageIntegrity {
  param([string] $Root)

  $result = @{ ok = $true; failure_codes = @(); details = @(); manifest = $null; package_version = '' }
  $manifestPath = Join-Path $Root 'package-manifest.json'

  if (-not (Test-Path -LiteralPath $manifestPath)) {
    $result.ok = $false
    $result.failure_codes = @($CodeManifestMissing)
    $result.details = @('package-manifest.json is missing')
    return $result
  }

  $manifest = $null
  try { $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json } catch { $manifest = $null }
  if ($null -eq $manifest -or -not (Test-Prop $manifest 'package_version') -or -not (Test-Prop $manifest 'entries')) {
    $result.ok = $false
    $result.failure_codes = @($CodeManifestInvalid)
    $result.details = @('package-manifest.json did not parse or lacks required fields')
    return $result
  }
  $result.manifest = $manifest
  $result.package_version = [string] $manifest.package_version

  $codes = New-Object System.Collections.Generic.List[string]
  $details = @()

  $versionFile = Join-Path $Root 'version.txt'
  if (-not (Test-Path -LiteralPath $versionFile) -or
      ((Get-Content -LiteralPath $versionFile -Raw).Trim() -cne [string] $manifest.package_version)) {
    if (-not $codes.Contains($CodeCheckerVersionMismatch)) { [void] $codes.Add($CodeCheckerVersionMismatch) }
    $details += 'version.txt does not match the manifest package_version'
  }

  $rootFull = [System.IO.Path]::GetFullPath($Root + [System.IO.Path]::DirectorySeparatorChar)
  foreach ($entry in (ConvertTo-SafeArray $manifest.entries)) {
    $rel = [string] $entry.path
    # Reject absolute paths, drive-qualified paths, and parent traversal BEFORE touching the disk.
    if ($rel -eq '' -or $rel -match '\.\.' -or $rel -match '^[\\/]' -or $rel -match ':') {
      if (-not $codes.Contains($CodeManifestPathEscape)) { [void] $codes.Add($CodeManifestPathEscape) }
      $details += ('entry path escapes the package root: ' + $rel)
      continue
    }
    $full = [System.IO.Path]::GetFullPath((Join-Path $Root $rel))
    if (-not $full.StartsWith($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
      if (-not $codes.Contains($CodeManifestPathEscape)) { [void] $codes.Add($CodeManifestPathEscape) }
      $details += ('entry resolves outside the package root: ' + $rel)
      continue
    }
    if (-not (Test-Path -LiteralPath $full)) {
      if (-not $codes.Contains($CodeManifestFileMissing)) { [void] $codes.Add($CodeManifestFileMissing) }
      $details += ('listed file is missing: ' + $rel)
      continue
    }
    $actualSize = (Get-Item -LiteralPath $full).Length
    if ($actualSize -ne [long] $entry.bytes) {
      if (-not $codes.Contains($CodeManifestSizeMismatch)) { [void] $codes.Add($CodeManifestSizeMismatch) }
      $details += ('size mismatch for ' + $rel + ': ' + $actualSize + ' != ' + $entry.bytes)
      continue
    }
    if ((Get-FileSha256Hex $full) -ne ([string] $entry.sha256).ToLowerInvariant()) {
      if (-not $codes.Contains($CodeManifestHashMismatch)) { [void] $codes.Add($CodeManifestHashMismatch) }
      $details += ('SHA-256 mismatch for ' + $rel)
      continue
    }
    # Compiled-version check for executables: the binary must report the manifest version itself.
    if ($rel -like '*.exe') {
      $reported = ''
      try { $reported = (& $full --version 2>$null | Out-String).Trim() } catch { $reported = '' }
      if ($reported -notlike ('*' + [string] $manifest.package_version + '*')) {
        if (-not $codes.Contains($CodeCheckerVersionMismatch)) { [void] $codes.Add($CodeCheckerVersionMismatch) }
        $details += ('compiled version mismatch for ' + $rel + ': "' + $reported + '"')
      }
    }
  }

  if ($codes.Count -gt 0) {
    $result.ok = $false
    $result.failure_codes = @($codes)
    $result.details = $details
  }
  return $result
}

# --- Aggregate + evidence writing ----------------------------------------------------------------
function ConvertTo-StageJsonObject {
  param($Record)
  return @{
    stage         = $Record.stage
    state         = $Record.state
    claim_level   = $Record.claim_level
    failure_codes = ConvertTo-SafeArray $Record.failure_codes
    detail        = $Record.detail
  }
}

function New-RealityLaneRows {
  param($Records, [string] $PackageVersion, [string] $RunId)
  $today = (Get-Date).ToUniversalTime().ToString('yyyy-MM-dd')
  $machine = $env:COMPUTERNAME
  if (-not $machine) { $machine = 'unknown-machine' }
  $smokeNames = @{
    playback  = 'Smoke 1 - Hardware playback (packaged verifier)'
    recording = 'Smoke 3 - Hardware recording round-trip (packaged verifier)'
    frame     = 'Smoke 4 - H16 frame smoke (packaged verifier, headless proxy)'
  }
  $rows = @()
  foreach ($name in $StageNames) {
    $record = @($Records | Where-Object { $_.stage -ceq $name })[0]
    $state = [string] $record.state
    $verdictWord = 'SETUP-INCOMPLETE'
    if ($state -ceq 'pass') { $verdictWord = 'PASS' }
    elseif ($state -ceq 'fail') { $verdictWord = 'FAIL' }
    $claimNote = ''
    if ($record.claim_level -ne '') { $claimNote = ' claim=' + $record.claim_level }
    $codesNote = ''
    $codes = ConvertTo-SafeArray $record.failure_codes
    if ($codes.Count -gt 0) { $codesNote = ' codes=' + ($codes -join ',') }
    $rows += ('| ' + $today + ' | ' + $smokeNames[$name] + ' | ' + $verdictWord + ' | ' + $machine +
              ' | Generated by verify-hardware.ps1 run ' + $RunId + ' on package ' + $PackageVersion +
              '.' + $claimNote + $codesNote + ' Stage state ' + $state +
              '; see result.json beside this row for the full measurements. |')
  }
  return $rows
}

function Write-AggregateEvidence {
  param([string] $ResultsDir, [string] $RunId, [string] $PackageVersion, [string] $ManifestSha,
        $Records, $Verdict, [string] $StartedAt, $IntegrityCodes, $IntegrityDetails)

  $stages = @{}
  foreach ($record in (ConvertTo-SafeArray $Records)) {
    $stages[$record.stage] = ConvertTo-StageJsonObject $record
  }

  $failureCodes = @((ConvertTo-SafeArray $IntegrityCodes) + (ConvertTo-SafeArray $Verdict.failure_codes))

  $aggregate = @{
    schema_version          = $SchemaVersion
    package_version         = $PackageVersion
    package_manifest_sha256 = $ManifestSha
    asio_compiled           = $false   # U6 wires the real capability flag through the manifest
    run_id                  = $RunId
    started_at              = $StartedAt
    completed_at            = Get-UtcNowIso
    host_os                 = [System.Environment]::OSVersion.VersionString
    machine                 = $env:COMPUTERNAME
    overall_state           = $Verdict.overall_state
    exit_code               = $Verdict.exit_code
    failure_codes           = $failureCodes
    integrity_details       = ConvertTo-SafeArray $IntegrityDetails
    stages                  = $stages
    generated_row_path      = 'reality-lane-rows.txt'
  }
  Write-JsonAtomic -Path (Join-Path $ResultsDir 'result.json') -Object $aggregate

  if ((ConvertTo-SafeArray $Records).Count -gt 0) {
    $rows = New-RealityLaneRows -Records $Records -PackageVersion $PackageVersion -RunId $RunId
    $tmp = Join-Path $ResultsDir 'reality-lane-rows.txt.tmp'
    Set-Content -LiteralPath $tmp -Value ($rows -join "`r`n") -Encoding UTF8
    Move-Item -LiteralPath $tmp -Destination (Join-Path $ResultsDir 'reality-lane-rows.txt') -Force
  }
}

# --- Normal path / integrity-only path -----------------------------------------------------------
function Invoke-Verification {
  param([bool] $StagesEnabled)

  $root = $PSScriptRoot
  $startedAt = Get-UtcNowIso
  $stamp = (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss')
  $runId = 'run-' + $stamp + '-' + ([System.Guid]::NewGuid().ToString('N').Substring(0, 8))

  $integrity = Test-PackageIntegrity -Root $root
  $packageVersion = $integrity.package_version
  if (-not $packageVersion) { $packageVersion = 'unknown' }

  # KTD4 layout: hardware-results/<UTC timestamp>-<package version>/, never overwriting a prior
  # run. Path segments stay SHORT on purpose: the recording stage nests a content-addressed bundle
  # below here, and Windows MAX_PATH bit a deep extraction live on 2026-08-04.
  $resultsRoot = Join-Path $root 'hardware-results'
  $resultsDir = Join-Path $resultsRoot ($stamp + '-' + $packageVersion)
  if (Test-Path -LiteralPath $resultsDir) {
    $resultsDir = $resultsDir + '-' + ([System.Guid]::NewGuid().ToString('N').Substring(0, 4))
  }
  New-Item -ItemType Directory -Force -Path $resultsDir | Out-Null

  $manifestSha = ''
  $manifestPath = Join-Path $root 'package-manifest.json'
  if (Test-Path -LiteralPath $manifestPath) { $manifestSha = Get-FileSha256Hex $manifestPath }

  if (-not $integrity.ok) {
    # Integrity failure stops BEFORE any hardware launch (R20) and still writes durable evidence.
    $verdict = @{ overall_state = 'setup'; exit_code = 2; failure_codes = @() }
    Write-AggregateEvidence -ResultsDir $resultsDir -RunId $runId -PackageVersion $packageVersion `
      -ManifestSha $manifestSha -Records @() -Verdict $verdict -StartedAt $startedAt `
      -IntegrityCodes $integrity.failure_codes -IntegrityDetails $integrity.details
    Write-Host 'verify-hardware: PACKAGE INTEGRITY FAILED - no hardware stage was launched.'
    foreach ($detail in $integrity.details) { Write-Host ('  - ' + $detail) }
    Write-Host ('verify-hardware: evidence: ' + $resultsDir)
    return 2
  }

  if (-not $StagesEnabled) {
    $records = @()
    foreach ($name in $StageNames) {
      $records += , (New-SynthesizedRecord -Stage $name -State 'skipped' -FailureCode '' -Detail 'integrity-only run')
    }
    $verdict = Get-AggregateVerdict -Records $records
    Write-AggregateEvidence -ResultsDir $resultsDir -RunId $runId -PackageVersion $packageVersion `
      -ManifestSha $manifestSha -Records $records -Verdict $verdict -StartedAt $startedAt `
      -IntegrityCodes @() -IntegrityDetails @()
    Write-Host 'verify-hardware: integrity OK; stages skipped (-IntegrityOnly can never produce a hardware verdict).'
    Write-Host ('verify-hardware: evidence: ' + $resultsDir)
    return 2
  }

  Write-Host ('verify-hardware: package ' + $packageVersion + ' integrity OK; run ' + $runId)
  Write-Host 'verify-hardware: note - playback and recording will make a short, quiet sound. This is informational only.'

  $records = @()
  foreach ($checker in $StageCheckers) {
    $stage = [string] $checker.Stage
    $exe = Join-Path $root ([string] $checker.Exe)
    $outJson = Join-Path $resultsDir ($stage + '.json')
    $extra = @()
    $timeout = $FrameTimeoutSec
    if ($stage -ceq 'playback') {
      $extra = @('--seconds', [string] $PlaybackSeconds)
      $timeout = [int] ($PlaybackSeconds + 90)
    }
    elseif ($stage -ceq 'recording') {
      # Short segment name: a content-addressed .yesdaw bundle nests below this (MAX_PATH).
      $extra = @('--seconds', [string] $RecordingSeconds,
                 '--work-dir', (Join-Path $resultsDir 'rec'))
      $timeout = [int] ($RecordingSeconds + 90)
    }

    Write-Host ('verify-hardware: running ' + $stage + ' stage (' + $checker.Exe + ', timeout ' + $timeout + ' s)...')
    $child = Invoke-StageChild -ExePath $exe -Stage $stage -RunId $runId -OutJson $outJson `
      -TimeoutSec $timeout -ExtraArgs $extra
    $records += , (ConvertTo-ChildRecord -Child $child -RunId $runId -CheckerVersion $packageVersion)
  }

  $verdict = Get-AggregateVerdict -Records $records
  Write-AggregateEvidence -ResultsDir $resultsDir -RunId $runId -PackageVersion $packageVersion `
    -ManifestSha $manifestSha -Records $records -Verdict $verdict -StartedAt $startedAt `
    -IntegrityCodes @() -IntegrityDetails @()

  Write-Host ''
  Write-Host ('verify-hardware: overall = ' + $verdict.overall_state + ' (exit ' + $verdict.exit_code + ')')
  foreach ($record in $records) {
    $claim = ''
    if ($record.claim_level -ne '') { $claim = ' claim=' + $record.claim_level }
    $codes = ConvertTo-SafeArray $record.failure_codes
    $codesText = ''
    if ($codes.Count -gt 0) { $codesText = ' codes=' + ($codes -join ',') }
    Write-Host ('  ' + $record.stage.PadRight(9) + ' ' + $record.state + $claim + $codesText)
  }
  Write-Host ('verify-hardware: evidence retained in ' + $resultsDir)
  Write-Host 'verify-hardware: the generated Reality-lane rows may be committed verbatim; never reclassified.'
  return $verdict.exit_code
}

# --- -SelfTest ------------------------------------------------------------------------------------
function Invoke-FixtureReplay {
  if (-not $FixtureDir) {
    $packaged = Join-Path $PSScriptRoot 'verify-fixtures'
    if (Test-Path -LiteralPath $packaged) {
      $script:FixtureDir = $packaged
    } else {
      $repoRoot = Split-Path -Parent $PSScriptRoot
      $script:FixtureDir = Join-Path $repoRoot 'tests\fixtures\hardware-verification'
    }
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

function Invoke-PackageMutationControls {
  # Runs only when this script sits inside a real package (manifest beside it). Each control
  # mutates a DISPOSABLE copy, runs the copied script with -IntegrityOnly, and asserts the named
  # code in the copy's aggregate. Both sides are proved: the clean copy first, then the mutations.
  $manifestPath = Join-Path $PSScriptRoot 'package-manifest.json'
  if (-not (Test-Path -LiteralPath $manifestPath)) {
    Write-Host 'selftest: package mutation controls skipped (no package-manifest.json beside the script; checkout mode)'
    return 0
  }

  $failures = 0
  $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
  $checkerRel = [string] (@($manifest.entries | Where-Object { $_.path -like '*.exe' })[0].path)

  $controls = @(
    @{ Name = 'clean_copy_integrity_ok'; Expect = ''; Mutate = { param($copy) } },
    @{ Name = 'manifest_missing'; Expect = $CodeManifestMissing;
       Mutate = { param($copy) Remove-Item -LiteralPath (Join-Path $copy 'package-manifest.json') -Force } },
    @{ Name = 'manifest_file_missing'; Expect = $CodeManifestFileMissing;
       Mutate = { param($copy) Remove-Item -LiteralPath (Join-Path $copy $checkerRel) -Force } },
    @{ Name = 'manifest_size_mismatch'; Expect = $CodeManifestSizeMismatch;
       Mutate = { param($copy) Add-Content -LiteralPath (Join-Path $copy $checkerRel) -Value 'x' -Encoding Ascii } },
    @{ Name = 'manifest_hash_mismatch'; Expect = $CodeManifestHashMismatch;
       Mutate = { param($copy)
         $target = Join-Path $copy $checkerRel
         $bytes = [System.IO.File]::ReadAllBytes($target)
         $bytes[$bytes.Length - 1] = $bytes[$bytes.Length - 1] -bxor 0xFF
         [System.IO.File]::WriteAllBytes($target, $bytes) } },
    @{ Name = 'manifest_path_escape'; Expect = $CodeManifestPathEscape;
       Mutate = { param($copy)
         $m = Get-Content -LiteralPath (Join-Path $copy 'package-manifest.json') -Raw | ConvertFrom-Json
         $m.entries[0].path = '..\outside.exe'
         $m | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $copy 'package-manifest.json') -Encoding UTF8 } },
    @{ Name = 'checker_version_mismatch'; Expect = $CodeCheckerVersionMismatch;
       Mutate = { param($copy)
         Set-Content -LiteralPath (Join-Path $copy 'version.txt') -Value 'not-the-real-version' -Encoding Ascii } }
  )

  foreach ($control in $controls) {
    $copy = Join-Path ([System.IO.Path]::GetTempPath()) ('yesdaw-verify-mutation-' + [System.Guid]::NewGuid().ToString('N'))
    Copy-Item -LiteralPath $PSScriptRoot -Destination $copy -Recurse
    try {
      & $control.Mutate $copy
      $copiedScript = Join-Path $copy 'verify-hardware.ps1'
      & powershell -NoProfile -ExecutionPolicy Bypass -File $copiedScript -IntegrityOnly *> $null
      $aggregate = $null
      # Newest first: the package copy may carry result dirs from earlier real runs.
      $resultFiles = @(Get-ChildItem -LiteralPath (Join-Path $copy 'hardware-results') -Recurse -Filter 'result.json' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending)
      if ($resultFiles.Count -ge 1) {
        $aggregate = Get-Content -LiteralPath $resultFiles[0].FullName -Raw | ConvertFrom-Json
      }

      $problem = ''
      if ($null -eq $aggregate) {
        $problem = 'no aggregate was written'
      }
      elseif ($control.Expect -eq '') {
        if ((ConvertTo-SafeArray $aggregate.failure_codes).Count -ne 0) {
          $problem = 'clean copy unexpectedly reported: ' + ((ConvertTo-SafeArray $aggregate.failure_codes) -join ',')
        }
        elseif ([int] $aggregate.exit_code -eq 0) {
          $problem = '-IntegrityOnly produced exit 0, which must be impossible'
        }
      }
      elseif ((ConvertTo-SafeArray $aggregate.failure_codes) -cnotcontains $control.Expect) {
        $problem = 'expected code ' + $control.Expect + ' but got [' + ((ConvertTo-SafeArray $aggregate.failure_codes) -join ',') + ']'
      }
      elseif ([int] $aggregate.exit_code -ne 2) {
        $problem = 'integrity failure must aggregate to exit 2'
      }

      if ($problem -eq '') {
        Write-Host ('[PASS] mutation ' + $control.Name)
      } else {
        $failures += 1
        Write-Host ('[FAIL] mutation ' + $control.Name + ' : ' + $problem)
      }
    } finally {
      Remove-Item -LiteralPath $copy -Recurse -Force -ErrorAction SilentlyContinue
    }
  }

  # Child-hang mechanics: the SAME launch/kill/synthesize function the normal path uses, against a
  # deliberately hanging child, must come back as a timeout within the declared budget.
  $hangJson = Join-Path ([System.IO.Path]::GetTempPath()) ('yesdaw-hang-' + [System.Guid]::NewGuid().ToString('N') + '.json')
  $outcome = Invoke-StageChild -ExePath 'powershell' -Stage 'frame' -RunId 'run-hang' -OutJson $hangJson `
    -TimeoutSec 2 -ExtraArgs @() -ArgsOverride @('-NoProfile', '-Command', 'Start-Sleep -Seconds 600')
  Remove-Item -LiteralPath ($hangJson + '.stdout.log'), ($hangJson + '.stderr.log') -Force -ErrorAction SilentlyContinue
  if ($outcome.outcome -ceq 'timeout') {
    Write-Host '[PASS] mutation child_hang_killed_at_timeout'
  } else {
    $failures += 1
    Write-Host ('[FAIL] mutation child_hang_killed_at_timeout : outcome was ' + $outcome.outcome)
  }

  if ($failures -eq 0) { return 0 }
  return 1
}

# --- Entry -------------------------------------------------------------------------------------
if ($SelfTest) {
  $fixtureExit = Invoke-FixtureReplay
  $mutationExit = Invoke-PackageMutationControls
  if ($fixtureExit -eq 0 -and $mutationExit -eq 0) { exit 0 }
  exit 1
}

if ($IntegrityOnly) {
  exit (Invoke-Verification -StagesEnabled $false)
}

exit (Invoke-Verification -StagesEnabled $true)
