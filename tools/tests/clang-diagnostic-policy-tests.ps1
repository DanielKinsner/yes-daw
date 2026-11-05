# Pure classifier regression; no compiler or test executable is launched.
$ErrorActionPreference = 'Stop'
$taskRepo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $taskRepo 'tools/ClangDiagnosticPolicy.ps1')
$taskProof = Join-Path $taskRepo 'tests/playback_click_query_cache_tests.cpp'
$taskHash = (Get-FileHash -LiteralPath $taskProof).Hash
if ($taskHash -cne '452E0337E968EA2771DB692BC199F346AAD954B6D925758CBC365BE01ABEA945') {
  throw 'Frozen proof changed; the exception no longer applies'
}
$taskDiagnostic = $taskProof + ":38:48: warning: sizeof() doesn't return the size of the container; did you mean .size()? [bugprone-sizeof-container]"
$taskCases = @(
  @{ name='exact intentional footprint'; text=$taskDiagnostic; path=$taskProof; hash=$taskHash; expected='ReviewedObjectFootprint' },
  @{ name='different source hash'; text=$taskDiagnostic; path=$taskProof; hash=('0' * 64); expected='UnhandledTidy' },
  @{ name='different source path'; text=$taskDiagnostic; path=(Join-Path $taskRepo 'tests/playback_tests.cpp'); hash=$taskHash; expected='UnhandledTidy' },
  @{ name='different diagnostic path'; text=$taskDiagnostic.Replace('playback_click_query_cache_tests.cpp','playback_tests.cpp'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
  @{ name='different line'; text=$taskDiagnostic.Replace(':38:48:',':39:48:'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
  @{ name='different column'; text=$taskDiagnostic.Replace(':38:48:',':38:49:'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
  @{ name='different heuristic'; text=$taskDiagnostic.Replace('[bugprone-sizeof-container]','[bugprone-sizeof-expression]'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
  @{ name='different message'; text=$taskDiagnostic.Replace('did you mean .size()?','different message'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
  @{ name='compiler warning'; text=($taskProof + ':38:48: warning: unused variable [clang-diagnostic-unused-variable]'); path=$taskProof; hash=$taskHash; expected='Compiler' },
  @{ name='compiler error'; text=($taskProof + ':38:48: error: invalid expression [clang-diagnostic-error]'); path=$taskProof; hash=$taskHash; expected='Compiler' },
  @{ name='untagged compiler error'; text=($taskProof + ':38:48: error: invalid expression'); path=$taskProof; hash=$taskHash; expected='Compiler' },
  @{ name='changed severity'; text=$taskDiagnostic.Replace(': warning:',': error:'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' }
)
foreach ($taskCase in $taskCases) {
  $taskKind = Get-YesDawClangDiagnosticKind -Diagnostic $taskCase.text -SourcePath $taskCase.path -SourceSha256 $taskCase.hash -RepoRoot $taskRepo
  if ($taskKind -cne $taskCase.expected) { throw ('Classifier mismatch: ' + $taskCase.name + ' => ' + $taskKind) }
}
Write-Output ('PASS ' + $taskCases.Count + ' diagnostic policy cases; all changed/real diagnostics remain failures')
