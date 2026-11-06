$ErrorActionPreference='Stop'
$taskRepo='C:/Users/SM - Dan/Documents/GitHub/yes-daw'
$taskEvidence=Join-Path $taskRepo 'build-ci/20261105-musical-funded-r5-force-emission-object-r1'
$taskSource=Join-Path $taskRepo 'build-ci/20261105-musical-funded-r5-force-emission-source-r1'
$taskProfilePath=Join-Path $taskEvidence 'profile.json'
if((Get-FileHash -LiteralPath $taskProfilePath).Hash -cne '2D1A978C40BBEC2FAD84D1E2210975B4FF0590900B275D9E7F2B5EDA8B4176A2'){throw 'Profile changed'}
if((Get-FileHash -LiteralPath (Join-Path $taskEvidence 'root-source-review.json')).Hash -cne 'CDCBAAAF3A5485DC95FB65900A31E3B311AF23A6F4350FD0BA6255332F8D6485'){throw 'Root source review changed'}
$taskProfile=Get-Content -LiteralPath $taskProfilePath -Raw -Encoding utf8|ConvertFrom-Json -AsHashtable
$taskRun=$taskProfile.run_folder
if(Test-Path -LiteralPath $taskRun){throw 'Preserve every actual compiler attempt'}
$null=New-Item -ItemType Directory -Path $taskRun
$taskReceiptPath=Join-Path $taskRun 'receipt.json'
$taskReceipt=[ordered]@{exit=1;scope=$taskProfile.scope;stages=[ordered]@{};commands=[ordered]@{};source_manifest=$taskProfile.source_manifest_sha256;profile_sha256='2D1A978C40BBEC2FAD84D1E2210975B4FF0590900B275D9E7F2B5EDA8B4176A2';target_executed=$false;target_linked=$false;desktop_input=$false;native_windows=$false;audio_device=$false}
. (Join-Path $taskRepo 'build-ci/20261105-click-position-cache-integration-r1/console-helpers.ps1')
Assert-TaskConsole $taskProfile.compiler
if((Get-FileHash -LiteralPath $taskProfile.compiler).Hash -cne $taskProfile.compiler_sha256){throw 'Compiler changed'}
$taskSourcePins=Get-Content -LiteralPath (Join-Path $taskSource 'source-pins.json') -Raw -Encoding utf8|ConvertFrom-Json -AsHashtable
$taskR5Pins=Get-Content -LiteralPath (Join-Path $taskRepo 'build-ci/20261105-musical-funded-legacy-adapter-source-r5/source-pins.json') -Raw -Encoding utf8|ConvertFrom-Json -AsHashtable
function Assert-EmissionPins {
 if((Get-FileHash -LiteralPath (Join-Path $taskSource 'source-pins.json')).Hash -cne $taskProfile.source_manifest_sha256){throw 'Force source manifest changed'}
 if((Get-FileHash -LiteralPath (Join-Path $taskRepo 'build-ci/compile_commands.json')).Hash -cne $taskProfile.compile_commands_sha256){throw 'Actual app command metadata changed'}
 foreach($taskPin in @($taskSourcePins.inputs)+@($taskSourcePins.outputs)+@($taskR5Pins.inputs)+@($taskR5Pins.protected_artifacts)+@($taskR5Pins.outputs)){
  $taskPinPath=if([IO.Path]::IsPathFullyQualified($taskPin.path)){$taskPin.path}else{[IO.Path]::Join($taskRepo,$taskPin.path)}
  if((Get-FileHash -LiteralPath $taskPinPath).Hash -cne $taskPin.sha256){throw ('Source pin changed '+$taskPin.path)}
 }
}
Save-TaskReceipt
Assert-EmissionPins
Invoke-TaskChild 'environment' $env:ComSpec @('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat') 30 $true
# Feed reviewed native argument text DIRECTLY to cl.exe, never to a shell.
# All original app macro/include/compute flags remain, only TU/output paths differ.
$taskInfo=[Diagnostics.ProcessStartInfo]::new()
$taskInfo.FileName=$taskProfile.compiler
$taskInfo.Arguments=$taskProfile.actual_arguments_string
$taskInfo.WorkingDirectory=$taskProfile.original_record.directory
$taskInfo.UseShellExecute=$false
$taskInfo.CreateNoWindow=$true
$taskInfo.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
$taskInfo.RedirectStandardOutput=$true
$taskInfo.RedirectStandardError=$true
$taskInfo.RedirectStandardInput=$true
$taskReceipt.commands['compile']=@{file=$taskInfo.FileName;native_argument_string=$taskInfo.Arguments;working_directory=$taskInfo.WorkingDirectory;via_shell=$false;link=$false;target_run=$false}
Save-TaskReceipt
$taskWatch=[Diagnostics.Stopwatch]::StartNew()
$taskProcess=[Diagnostics.Process]::Start($taskInfo)
$taskProcess.StandardInput.Close()
$taskOut=$taskProcess.StandardOutput.ReadToEndAsync()
$taskErr=$taskProcess.StandardError.ReadToEndAsync()
$taskTimeout=-not $taskProcess.WaitForExit(120000)
if($taskTimeout){$taskProcess.Kill($true);$taskProcess.WaitForExit()}
$taskOutText=$taskOut.GetAwaiter().GetResult()
$taskErrText=$taskErr.GetAwaiter().GetResult()
$taskExit=$taskProcess.ExitCode
$taskProcess.Dispose()
[IO.File]::WriteAllText((Join-Path $taskRun 'compile-stdout.log'),$taskOutText,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $taskRun 'compile-stderr.log'),$taskErrText,[Text.UTF8Encoding]::new($false))
$taskReceipt.stages['compile']=@{raw_exit=$taskExit;timeout=$taskTimeout;seconds=$taskWatch.Elapsed.TotalSeconds}
Save-TaskReceipt
Assert-EmissionPins
if($taskTimeout -or $taskExit -ne 0){Write-Output ($taskOutText+$taskErrText);throw ('Actual force compile failure raw_exit='+$taskExit)}
$taskReceipt.object_sha256=(Get-FileHash -LiteralPath $taskProfile.object_path).Hash
$taskReceipt.object_bytes=(Get-Item -LiteralPath $taskProfile.object_path).Length
$taskReceipt.exit=0
Save-TaskReceipt
Write-Output ('compile raw_exit='+$taskExit+' object_bytes='+$taskReceipt.object_bytes)
