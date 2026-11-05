# One frozen proof deliberately measures the vector object as well as its allocated payload.
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
  if ($Diagnostic -match '\[clang-diagnostic-[^\]]+\]' -or $Diagnostic -notmatch '\[[a-z][a-z0-9-]+\]$') {
    return 'Compiler'
  }
  return 'UnhandledTidy'
}
