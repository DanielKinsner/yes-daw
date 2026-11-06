#Requires -Version 7.0
$ErrorActionPreference='Stop'
$taskRepo='C:/Users/SM - Dan/Documents/GitHub/yes-daw'
$taskSource=Join-Path $taskRepo 'build-ci/20261105-musical-query-fp-profile-probe-r1'
$taskRun=Join-Path $taskSource 'execution-r1'
if(Test-Path -LiteralPath $taskRun){throw 'Preserve every native attempt'}
$null=New-Item -ItemType Directory -Path $taskRun
$taskReceiptPath=Join-Path $taskRun 'receipt.json'
$taskReceipt=[ordered]@{exit=1;scope='Finite old Clock/public integer-query versus scalar old helper measurement only';stages=[ordered]@{};commands=[ordered]@{};desktop_input=$false;native_windows=$false;audio_device=$false;first_green=$false;source_manifest='DA5DDDA7E4613B0A26A682F169049F8E384AE2AC1CC85F8CCBFCA4B575B0DFEB';compiler='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe';compiler_sha256='88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D';executables=[ordered]@{}}
. (Join-Path $taskRepo 'build-ci/20261105-click-position-cache-integration-r1/console-helpers.ps1')
if((Get-FileHash -LiteralPath (Join-Path $taskSource 'source-pins.json')).Hash -cne $taskReceipt.source_manifest){throw 'Whole probe manifest changed'}
if((Get-FileHash -LiteralPath (Join-Path $taskRepo 'build-ci/20261105-musical-query-fp-profile-probe-r1-independent-review.json')).Hash -cne '4546EB0AE075E26B6601799F571A504B8371BE64CBC495193021A143AC8B90FE'){throw 'Whole source review changed'}
if((Get-FileHash -LiteralPath $taskReceipt.compiler).Hash -cne $taskReceipt.compiler_sha256){throw 'Compiler changed'}
$taskPins=Get-Content -LiteralPath (Join-Path $taskSource 'source-pins.json') -Raw -Encoding utf8|ConvertFrom-Json -AsHashtable
function Assert-ProbePins {
 foreach($taskName in $taskPins.authoredFiles.Keys){if((Get-FileHash -LiteralPath (Join-Path $taskSource $taskName)).Hash.ToLowerInvariant() -cne $taskPins.authoredFiles[$taskName]){throw ('Authored pin changed '+$taskName)}}
 foreach($taskName in $taskPins.oldQuotedIncludeClosure.Keys){if((Get-FileHash -LiteralPath (Join-Path $taskRepo $taskName)).Hash.ToLowerInvariant() -cne $taskPins.oldQuotedIncludeClosure[$taskName]){throw ('Old source pin changed '+$taskName)}}
}
Assert-ProbePins
Invoke-TaskChild 'environment' $env:ComSpec @('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat') 30 $true
$taskCommon=@('/nologo','/DWIN32','/D_WINDOWS','/EHsc','/O2','/Ob2','/DNDEBUG','/std:c++20','/MD','/W4','/WX','/Isrc',('/I'+$taskSource),'/c')
$taskObserver=Join-Path $taskRun 'fp-observer.obj'
$taskArguments=$taskCommon+@('/fp:strict',('/Fo'+$taskObserver),(Join-Path $taskSource 'fp-observer.cpp'))
$taskReceipt.commands['observer-compile']=@{file=$taskReceipt.compiler;args=$taskArguments}
Invoke-TaskChild 'observer-compile' $taskReceipt.compiler $taskArguments 120
foreach($taskProfile in @('precise-default','strict')){
 $taskObjects=@()
 foreach($taskStem in @('build-and-query','replay-probe')){
  $taskObject=Join-Path $taskRun ($taskProfile+'-'+$taskStem+'.obj')
  $taskArguments=$taskCommon
  if($taskProfile -ceq 'strict'){$taskArguments+=@('/fp:strict')}
  $taskArguments+=@(('/Fo'+$taskObject),(Join-Path $taskSource ($taskStem+'.cpp')))
  $taskStage=$taskProfile+'-'+$taskStem+'-compile'
  $taskReceipt.commands[$taskStage]=@{file=$taskReceipt.compiler;args=$taskArguments}
  Invoke-TaskChild $taskStage $taskReceipt.compiler $taskArguments 120
  $taskObjects+=@($taskObject)
 }
 $taskExecutable=Join-Path $taskRun ($taskProfile+'.exe')
 $taskArguments=@('/nologo')+$taskObjects+@($taskObserver,('/Fe'+$taskExecutable),'/link','/SUBSYSTEM:CONSOLE','/MACHINE:X64','/INCREMENTAL:NO')
 $taskStage=$taskProfile+'-link'
 $taskReceipt.commands[$taskStage]=@{file=$taskReceipt.compiler;args=$taskArguments}
 Invoke-TaskChild $taskStage $taskReceipt.compiler $taskArguments 120
 Assert-TaskConsole $taskExecutable
 $taskReceipt.executables[$taskProfile]=@{path=$taskExecutable;sha256=(Get-FileHash -LiteralPath $taskExecutable).Hash;pe_machine='AMD64';subsystem='CONSOLE'}
}
# Only this ephemeral console parent and its children inherit the no-error-box mode.
Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class FpProbeErrorMode { [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint value); }'
$taskOldMode=[FpProbeErrorMode]::SetErrorMode(0x8003)
try{
 [void][FpProbeErrorMode]::SetErrorMode($taskOldMode -bor 0x8003)
 foreach($taskProfile in @('precise-default','strict')){
  Assert-ProbePins
  Invoke-TaskChild ($taskProfile+'-measurement') (Join-Path $taskRun ($taskProfile+'.exe')) @() 120
  Assert-ProbePins
  if(-not $taskReceipt.first_green){
   $taskReceipt.first_green=$true
   [IO.File]::WriteAllText((Join-Path $taskRun 'first-green-whole-source-freeze.json'),((@{source_manifest=$taskReceipt.source_manifest;first_green_profile=$taskProfile;source_review='4546EB0AE075E26B6601799F571A504B8371BE64CBC495193021A143AC8B90FE';scope='All six whole authored probe files including helpers comments README frozen';utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json).Replace("`r`n","`n")+"`n"),[Text.UTF8Encoding]::new($false))
  }
 }
}finally{[void][FpProbeErrorMode]::SetErrorMode($taskOldMode)}
Assert-ProbePins
$taskReceipt.exit=0;Save-TaskReceipt
foreach($taskProfile in @('precise-default','strict')){Get-Content -LiteralPath (Join-Path $taskRun ($taskProfile+'-measurement.log')) -Encoding utf8}
