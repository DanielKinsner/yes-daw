#Requires -Version 7.0
$ErrorActionPreference='Stop'
$taskRepo='C:/Users/SM - Dan/Documents/GitHub/yes-daw'
$taskSource=Join-Path $taskRepo 'build-ci/20261105-musical-native-module-association-source-r2'
$taskRun=Join-Path $taskSource 'execution-r1'
if(Test-Path -LiteralPath $taskRun){throw 'Preserve every native attempt'}
$null=New-Item -ItemType Directory -Path $taskRun
$taskReceiptPath=Join-Path $taskRun 'receipt.json'
$taskReceipt=[ordered]@{exit=1;scope='Actual six imported function module paths/RVAs and FMA3 getter for this new console executable only';stages=[ordered]@{};commands=[ordered]@{};desktop_input=$false;native_windows=$false;audio_device=$false;first_green=$false;source_manifest='081D115CD0B561CFF70C4BB5E1EAB53BBA8FE0807E99DACF1EC7DA7B4BC82948';source_review='AF32EE5366D030C8BD3436C4BAA3DB17C85330A161AE0BCC771DB863C29591D9';compiler_sha256='88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D';candidate_ucrt_sha256='DEDB7611E1ABC1FF3852DCAC3D4C93A048D729E25B061973B9969F22698F14CA'}
. (Join-Path $taskRepo 'build-ci/20261105-click-position-cache-integration-r1/console-helpers.ps1')
$taskCompiler='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe'
if((Get-FileHash -LiteralPath $taskCompiler).Hash -cne $taskReceipt.compiler_sha256){throw 'Compiler changed'}
if((Get-FileHash -LiteralPath (Join-Path $taskSource 'source-pins.json')).Hash -cne $taskReceipt.source_manifest){throw 'Manifest changed'}
if((Get-FileHash -LiteralPath (Join-Path $taskRepo 'build-ci/20261105-musical-native-module-association-source-review-r2/review.json')).Hash -cne $taskReceipt.source_review){throw 'Source review changed'}
$taskPins=Get-Content -LiteralPath (Join-Path $taskSource 'source-pins.json') -Raw -Encoding utf8|ConvertFrom-Json -AsHashtable
function Assert-AssociationPins {
 foreach($taskItem in @($taskPins.inputs)+@($taskPins.outputs)){if((Get-FileHash -LiteralPath (Join-Path $taskRepo $taskItem.path)).Hash -cne $taskItem.sha256){throw ('Source pin changed '+$taskItem.path)}}
 if((Get-FileHash -LiteralPath 'C:/Windows/System32/ucrtbase.dll').Hash -cne $taskReceipt.candidate_ucrt_sha256){throw 'Candidate UCRT file changed'}
}
Assert-AssociationPins
Invoke-TaskChild 'environment' $env:ComSpec @('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat') 30 $true
$taskExe=Join-Path $taskRun 'association.exe'
$taskArguments=@('/nologo','/DWIN32','/D_WINDOWS','/EHsc','/O2','/Ob2','/DNDEBUG','/std:c++20','/MD','/W4','/WX',('/Fo'+(Join-Path $taskRun 'association.obj')),('/Fe'+$taskExe),(Join-Path $taskSource 'association.cpp'),'/link','kernel32.lib','/SUBSYSTEM:CONSOLE','/MACHINE:X64','/INCREMENTAL:NO')
$taskReceipt.commands['compile']=@{file=$taskCompiler;args=$taskArguments}
Invoke-TaskChild 'compile' $taskCompiler $taskArguments 120
Assert-TaskConsole $taskExe
$taskReceipt.executable_sha256=(Get-FileHash -LiteralPath $taskExe).Hash
Add-Type -TypeDefinition 'using System.Runtime.InteropServices; public static class ModuleAssociationErrorMode { [DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint value); }'
$taskOldMode=[ModuleAssociationErrorMode]::SetErrorMode(0x8003)
try{
 [void][ModuleAssociationErrorMode]::SetErrorMode($taskOldMode -bor 0x8003)
 Invoke-TaskChild 'association' $taskExe @() 30
}finally{[void][ModuleAssociationErrorMode]::SetErrorMode($taskOldMode)}
$taskReceipt.first_green=$true;Save-TaskReceipt
[IO.File]::WriteAllText((Join-Path $taskRun 'first-green-whole-source-freeze.json'),((@{manifest=$taskReceipt.source_manifest;review=$taskReceipt.source_review;scope='Whole association.cpp README.md and source-pins.json including all helpers/comments frozen';utc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json).Replace("`r`n","`n")+"`n"),[Text.UTF8Encoding]::new($false))
Assert-AssociationPins
$taskText=Get-Content -LiteralPath (Join-Path $taskRun 'association.log') -Raw -Encoding utf8
if(($taskText -split "`r?`n" | Where-Object {$_ -match '^symbol='}).Count -ne 6){throw 'Complete association rows absent'}
if(-not $taskText.Contains('PASS actual loaded module association for this console executable only')){throw 'Self-asserting result absent'}
$taskReceipt.exit=0;Save-TaskReceipt
Write-Output $taskText
