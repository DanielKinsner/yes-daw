# Frozen proofs deliberately measure vector objects as well as their allocated payloads.
# Keep this exact exception visible; it never applies to compiler diagnostics or changed source.
function Get-YesDawClangDiagnosticKind {
  param([string]$Diagnostic, [string]$SourcePath, [string]$SourceSha256, [string]$RepoRoot)

  $proofPath = [IO.Path]::GetFullPath((Join-Path $RepoRoot 'tests/playback_click_query_cache_tests.cpp'))
  $actualPath = [IO.Path]::GetFullPath($SourcePath)
  $expected = $proofPath + ":38:48: warning: sizeof() doesn't return the size of the container; did you mean .size()? [bugprone-sizeof-container]"
  if ($actualPath -ceq $proofPath -and
      $SourceSha256 -ceq '452E0337E968EA2771DB692BC199F346AAD954B6D925758CBC365BE01ABEA945' -and
      $Diagnostic -ceq $expected) {
    return 'ReviewedObjectFootprint'
  }
  $positionPath = [IO.Path]::GetFullPath((Join-Path $RepoRoot 'tests/playback_click_position_cache_tests.cpp'))
  $positionFirst = $positionPath + ":18:48: warning: sizeof() doesn't return the size of the container; did you mean .size()? [bugprone-sizeof-container]"
  $positionSecond = $positionPath + ":20:15: warning: sizeof() doesn't return the size of the container; did you mean .size()? [bugprone-sizeof-container]"
  if ($actualPath -ceq $positionPath -and
      $SourceSha256 -ceq 'AA731375071C5DA38CFD0F404FC94E162D77BBDCF0046D7A1BE03EA7EDC017D7' -and
      ($Diagnostic -ceq $positionFirst -or $Diagnostic -ceq $positionSecond)) {
    return 'ReviewedObjectFootprint'
  }
  if ($Diagnostic -match '\[clang-diagnostic-[^\]]+\]' -or $Diagnostic -notmatch '\[[a-z][a-z0-9-]+\]$') {
    return 'Compiler'
  }
  return 'UnhandledTidy'
}
