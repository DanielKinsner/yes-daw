$ErrorActionPreference = 'Stop'
$taskRepo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $taskRepo 'tools/ClangDiagnosticPolicy.ps1')
$taskProof = Join-Path $taskRepo 'tests/playback_click_position_cache_tests.cpp'
$taskHash = (Get-FileHash -LiteralPath $taskProof).Hash
if ($taskHash -cne 'AA731375071C5DA38CFD0F404FC94E162D77BBDCF0046D7A1BE03EA7EDC017D7') {
  throw 'Frozen position proof changed; reviewed footprints no longer apply'
}
$taskTotal = 0
foreach ($taskLocation in @(':18:48:', ':20:15:')) {
  $taskDiagnostic = $taskProof + $taskLocation + " warning: sizeof() doesn't return the size of the container; did you mean .size()? [bugprone-sizeof-container]"
  $taskCases = @(
    @{ name='exact intentional footprint'; text=$taskDiagnostic; path=$taskProof; hash=$taskHash; expected='ReviewedObjectFootprint' },
    @{ name='different hash'; text=$taskDiagnostic; path=$taskProof; hash=('0' * 64); expected='UnhandledTidy' },
    @{ name='different source path'; text=$taskDiagnostic; path=(Join-Path $taskRepo 'tests/playback_tests.cpp'); hash=$taskHash; expected='UnhandledTidy' },
    @{ name='different diagnostic path'; text=$taskDiagnostic.Replace('playback_click_position_cache_tests.cpp','playback_tests.cpp'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
    @{ name='different line'; text=$taskDiagnostic.Replace($taskLocation,':19:48:'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
    @{ name='different column'; text=$taskDiagnostic.Replace($taskLocation,':20:16:'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
    @{ name='different heuristic'; text=$taskDiagnostic.Replace('[bugprone-sizeof-container]','[bugprone-sizeof-expression]'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
    @{ name='different message'; text=$taskDiagnostic.Replace('did you mean .size()?','different message'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' },
    @{ name='compiler warning'; text=($taskProof+$taskLocation+' warning: unused variable [clang-diagnostic-unused-variable]'); path=$taskProof; hash=$taskHash; expected='Compiler' },
    @{ name='compiler error'; text=($taskProof+$taskLocation+' error: invalid expression [clang-diagnostic-error]'); path=$taskProof; hash=$taskHash; expected='Compiler' },
    @{ name='untagged compiler error'; text=($taskProof+$taskLocation+' error: invalid expression'); path=$taskProof; hash=$taskHash; expected='Compiler' },
    @{ name='changed severity'; text=$taskDiagnostic.Replace(': warning:',': error:'); path=$taskProof; hash=$taskHash; expected='UnhandledTidy' }
  )
  foreach ($taskCase in $taskCases) {
    $taskKind = Get-YesDawClangDiagnosticKind -Diagnostic $taskCase.text -SourcePath $taskCase.path -SourceSha256 $taskCase.hash -RepoRoot $taskRepo
    if ($taskKind -cne $taskCase.expected) { throw ('Position footprint mismatch '+$taskLocation+' '+$taskCase.name+' => '+$taskKind) }
    $taskTotal++
  }
}
Write-Output ('PASS '+$taskTotal+' position-footprint cases; changed and real diagnostics remain failures')
