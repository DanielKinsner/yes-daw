# tools/clang-warnings.ps1 - Clang's compiler warnings on Windows, before CI sees them.
#
# MSVC /W4 accepts things the Linux (GCC) and macOS (Clang) jobs reject under -Werror: -Wsign-compare
# against size_t counts, unused const variables, and more. clang-tidy ships with VS Build Tools and runs
# Clang's own front end over build-ci's compile_commands.json, where the project's /W4 maps to Clang's
# -Wall -Wextra; every clang-diagnostic-* it reports is a warning CI would turn into an error.
# 2026-10-05: it reproduced run 37416955166's macOS/Linux sign-compare red in 13 s.
#
# Usage (after a ninja build has written build-ci/compile_commands.json):
#   powershell -ExecutionPolicy Bypass -File tools\clang-warnings.ps1            # C++ changed vs origin/main
#   powershell -ExecutionPolicy Bypass -File tools\clang-warnings.ps1 -Files a.cpp,b.cpp
# Exit 0 when no warning is found in our sources; 1 otherwise. Headers are checked through the
# translation units that include them.
param([string[]] $Files = @(), [string] $Base = 'origin/main')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$tidy = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\clang-tidy.exe'
if (-not (Test-Path -LiteralPath $tidy)) { Write-Host "FAIL: clang-tidy not found at $tidy"; exit 1 }
if (-not (Test-Path -LiteralPath 'build-ci\compile_commands.json')) { Write-Host 'FAIL: build-ci\compile_commands.json missing (build first)'; exit 1 }

if ($Files.Count -eq 0) {
  $changed = @(git diff --name-only $Base -- '*.cpp' '*.h') + @(git diff --name-only -- '*.cpp' '*.h') + @(git ls-files --others --exclude-standard -- '*.cpp' '*.h')
  $changed = $changed | Where-Object { $_ } | Sort-Object -Unique
  $units = New-Object System.Collections.Generic.List[string]
  foreach ($path in $changed) {
    if ($path -like '*.cpp') { $units.Add($path.Replace('/', '\')); continue }
    # A changed header: the shell unit that reaches every src header through MainComponentInternal.h,
    # plus any translation unit that includes the header by name.
    if ($path -like 'src/*') { $units.Add('src\ui\MainComponentControls.cpp') }
    $leaf = Split-Path -Leaf $path
    Get-ChildItem -Path src, tests -Recurse -Filter *.cpp | Where-Object { Select-String -LiteralPath $_.FullName -SimpleMatch -Pattern ('/' + $leaf + '"') -Quiet } |
      ForEach-Object { $units.Add((Resolve-Path -Relative $_.FullName).TrimStart('.', '\')) }
  }
  $Files = $units | Sort-Object -Unique
}
if ($Files.Count -eq 0) { Write-Host 'PASS: no C++ changes to check'; exit 0 }

$found = 0
foreach ($file in $Files) {
  # Windows PowerShell turns a native program's stderr ("1 error generated.") into a terminating error
  # under 'Stop', which once killed the run before the finding printed; read it as plain text instead.
  $ErrorActionPreference = 'Continue'
  $lines = & $tidy -p build-ci --checks='-*,bugprone-sizeof-container' --extra-arg=-Wno-unknown-pragmas --extra-arg=-Wno-unused-command-line-argument $file 2>&1 |
    ForEach-Object { "$_" } |
    Select-String -Pattern 'warning:|error:' | Where-Object { $_.Line -notmatch '_deps|juce-src|catch2|too many errors' }
  $ErrorActionPreference = 'Stop'
  foreach ($line in $lines) { Write-Host $line.Line; $found++ }
  Write-Host ('  checked ' + $file + ': ' + @($lines).Count + ' finding(s)')
}
if ($found -gt 0) { Write-Host "FAIL: $found Clang warning(s) CI would reject"; exit 1 }
Write-Host ('PASS: ' + $Files.Count + ' translation unit(s) clean under Clang -Wall -Wextra')
exit 0
