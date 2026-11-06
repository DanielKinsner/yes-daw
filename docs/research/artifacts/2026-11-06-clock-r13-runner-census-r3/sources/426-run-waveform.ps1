#Requires -Version 7.0
param(
 [Parameter(Mandatory)][ValidateSet('DefinitionFreeze','OracleFreeze','PlanPreflight','AnalyticSelfCheck','CandidateBaseline','ActualOwnGateControls')][string]$Stage,
 [Parameter(Mandatory)][string]$Repo,
 [Parameter(Mandatory)][string]$NewRunRoot,
 [Parameter(Mandatory)][string]$ReviewedManifestSha256,
 [string]$ReviewedInputBundle,
 [string]$ReviewedInputBundleSha256,
 [string]$ClPath,
 [string]$PythonPath='C:/Users/SM - Dan/AppData/Local/Programs/Python/Python313/python.exe'
)
$ErrorActionPreference='Stop'
$taskSourceReviewed=$false
$taskAllStagesAndOutputBoundsReviewed=$false
$taskCompilerArtifactUpperAccepted=$false
if(-not $taskSourceReviewed -or -not $taskAllStagesAndOutputBoundsReviewed){throw 'SOURCE_HOLD_NO_FREEZE_COMPILER_OR_EXECUTION_AUTHORITY'}
$taskRepo=[IO.Path]::GetFullPath($Repo)
$taskRoot=[IO.Path]::GetFullPath($NewRunRoot)
$taskBase=[IO.Path]::GetFullPath($PSScriptRoot)
if(-not $taskRoot.StartsWith($taskBase+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Run root must be inside this ignored source revision'}
$taskManifestPath=Join-Path $taskBase 'authored-manifest.json'
if((Get-FileHash -LiteralPath $taskManifestPath).Hash -cne $ReviewedManifestSha256.ToUpperInvariant()){throw 'Actual reviewed source manifest mismatch'}
$taskManifest=Get-Content -LiteralPath $taskManifestPath -Raw|ConvertFrom-Json
$taskOwnNames=@('WaveformCandidate.h','waveform_probe.cpp','waveform_check.cpp','clock_oracle.cpp','plan_probe.cpp','independent_clock.h','fast_fit.h','freeze_waveform.py','verify_waveform.py','run-waveform.ps1','README.md')
$taskOwnPins=@($taskManifest.sources)
if($taskOwnPins.Count -ne 11 -or @($taskOwnPins.path|Select-Object -Unique).Count -ne 11 -or @(Compare-Object $taskOwnNames @($taskOwnPins.path) -CaseSensitive).Count -ne 0){throw 'Exact current eleven authored source pins required'}
foreach($taskPin in $taskOwnPins){$taskOwnPath=[IO.Path]::GetFullPath((Join-Path $taskBase $taskPin.path));if($taskOwnPath -cne [IO.Path]::GetFullPath((Join-Path $taskBase ([IO.Path]::GetFileName($taskPin.path))))){throw 'Current authored path escape'};$taskOwnFile=Get-Item -LiteralPath $taskOwnPath;if($taskOwnFile.Length -ne $taskPin.bytes -or (Get-FileHash -LiteralPath $taskOwnPath).Hash -cne $taskPin.sha256){throw ('Current authored source changed: '+$taskPin.path)}}
foreach($taskPin in $taskManifest.preserved_pins){if((Get-FileHash -LiteralPath (Join-Path $taskRepo $taskPin.path)).Hash -cne $taskPin.sha256){throw ('Protected input changed: '+$taskPin.path)}}
if(Test-Path -LiteralPath $taskRoot){throw 'Never overwrite or retry a retained run root'}
if($Stage -cne 'DefinitionFreeze'){
 if(-not $ReviewedInputBundle -or -not $ReviewedInputBundleSha256){throw 'Separate actual input-bundle review required'}
 if((Get-FileHash -LiteralPath $ReviewedInputBundle).Hash -cne $ReviewedInputBundleSha256.ToUpperInvariant()){throw 'Reviewed input bundle changed'}
 $taskInputFull=[IO.Path]::GetFullPath($ReviewedInputBundle);if(-not $taskInputFull.StartsWith($taskBase+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Reviewed input bundle must be inside this revision'}
 $taskInputData=Get-Content -LiteralPath $taskInputFull -Raw|ConvertFrom-Json
 foreach($taskArtifact in $taskInputData.artifacts){if((Get-FileHash -LiteralPath $taskArtifact.path).Hash -cne $taskArtifact.sha256){throw 'Frozen reviewed artifact changed'}}
 $taskConsumed=[IO.File]::Open((Join-Path (Split-Path $taskInputFull) 'consumed.json'),[IO.FileMode]::CreateNew);try{$taskBytes=[Text.Encoding]::UTF8.GetBytes(('{"next_stage":"'+$Stage+'","input_sha256":"'+$ReviewedInputBundleSha256+'"}'+"`n"));$taskConsumed.Write($taskBytes,0,$taskBytes.Length)}finally{$taskConsumed.Dispose()}
}
New-Item -ItemType Directory -Path $taskRoot|Out-Null
$taskBuildRoot=Join-Path $taskRoot 'build';$taskStageRoot=Join-Path $taskRoot ('stages/'+$Stage)
New-Item -ItemType Directory -Path $taskBuildRoot,$taskStageRoot|Out-Null
$taskPrivateRoot=Join-Path $taskRoot 'generated/private-sources'
$taskReceipt=[ordered]@{stage=$Stage;status='STARTED_NOT_CERTIFIED';manifest_sha256=$ReviewedManifestSha256;children=@();native_or_device=$false;desktop_input=$false;source_generation_prior_to_this_run=$false}
$taskLogTotal=0L
function Save-TaskReceipt {
 $taskText=($taskReceipt|ConvertTo-Json -Depth 16).Replace("`r`n","`n")+"`n"
 if([Text.Encoding]::UTF8.GetByteCount($taskText) -gt 262144){throw 'Receipt reservation'}
 [IO.File]::WriteAllText((Join-Path $taskStageRoot 'receipt.json'),$taskText,[Text.UTF8Encoding]::new($false))
}
function Invoke-Hidden([string]$Name,[string]$File,[string[]]$Arguments,[int]$Seconds){
 $taskInfo=[Diagnostics.ProcessStartInfo]::new();$taskInfo.FileName=$File;$taskInfo.WorkingDirectory=$taskRoot;$taskInfo.UseShellExecute=$false;$taskInfo.CreateNoWindow=$true
 $taskInfo.RedirectStandardOutput=$true;$taskInfo.RedirectStandardError=$true
 foreach($taskArgument in $Arguments){$taskInfo.ArgumentList.Add($taskArgument)}
 $taskCompileLog=$Name.StartsWith('compile-');$taskLogName=$Name.Replace('-probe','');$taskOutPath=if($taskCompileLog){Join-Path $taskBuildRoot ($taskLogName+'.stdout.log')}else{Join-Path $taskStageRoot 'stdout.log'};$taskErrPath=if($taskCompileLog){Join-Path $taskBuildRoot ($taskLogName+'.stderr.log')}else{Join-Path $taskStageRoot 'stderr.log'}
 $taskOutFile=[IO.File]::Open($taskOutPath,[IO.FileMode]::Append);$taskErrFile=[IO.File]::Open($taskErrPath,[IO.FileMode]::Append);$taskOutStart=$taskOutFile.Position;$taskErrStart=$taskErrFile.Position
 $taskProcess=[Diagnostics.Process]::Start($taskInfo);$taskWatch=[Diagnostics.Stopwatch]::StartNew();$taskA=[byte[]]::new(4096);$taskB=[byte[]]::new(4096)
 $taskReadA=$taskProcess.StandardOutput.BaseStream.ReadAsync($taskA,0,4096);$taskReadB=$taskProcess.StandardError.BaseStream.ReadAsync($taskB,0,4096);$taskEndA=$false;$taskEndB=$false
 try {
  while(-not ($taskEndA -and $taskEndB -and $taskProcess.HasExited)){
   if($taskWatch.Elapsed.TotalSeconds -gt $Seconds){$taskProcess.Kill($true);$taskProcess.WaitForExit();throw ('Actual child timeout: '+$Name)}
   if(-not $taskEndA -and $taskReadA.IsCompleted){$taskN=$taskReadA.GetAwaiter().GetResult();if($taskN -eq 0){$taskEndA=$true}else{if($taskN -gt (64MB-$script:taskLogTotal)){$taskProcess.Kill($true);throw 'Before-write log cap'};$taskOutFile.Write($taskA,0,$taskN);$script:taskLogTotal+=$taskN;$taskReadA=$taskProcess.StandardOutput.BaseStream.ReadAsync($taskA,0,4096)}}
   if(-not $taskEndB -and $taskReadB.IsCompleted){$taskN=$taskReadB.GetAwaiter().GetResult();if($taskN -eq 0){$taskEndB=$true}else{if($taskN -gt (64MB-$script:taskLogTotal)){$taskProcess.Kill($true);throw 'Before-write log cap'};$taskErrFile.Write($taskB,0,$taskN);$script:taskLogTotal+=$taskN;$taskReadB=$taskProcess.StandardError.BaseStream.ReadAsync($taskB,0,4096)}}
   $null=$taskProcess.WaitForExit(50)
  }
  $taskProcess.WaitForExit();$taskReceipt.children+=@{name=$Name;raw_exit=$taskProcess.ExitCode;seconds=$taskWatch.Elapsed.TotalSeconds;stdout_path=$taskOutPath;stderr_path=$taskErrPath;stdout_start=$taskOutStart;stderr_start=$taskErrStart;stdout_bytes=$taskOutFile.Position-$taskOutStart;stderr_bytes=$taskErrFile.Position-$taskErrStart};Save-TaskReceipt
  return $taskProcess.ExitCode
 } finally {$taskOutFile.Dispose();$taskErrFile.Dispose()}
}
function Confirm-Console([string]$Path){
 $taskBytes=[IO.File]::ReadAllBytes($Path);if($taskBytes.Length -lt 128 -or $taskBytes[0] -ne 77 -or $taskBytes[1] -ne 90){throw 'Actual PE DOS header'}
 $taskOffset=[BitConverter]::ToInt32($taskBytes,60);if($taskOffset -lt 64 -or $taskOffset+94 -gt $taskBytes.Length -or [BitConverter]::ToUInt32($taskBytes,$taskOffset) -ne 0x4550 -or [BitConverter]::ToUInt16($taskBytes,$taskOffset+92) -ne 3){throw 'Actual linked PE must be console before launch'}
}
function Compile-Unit([string]$Name,[string]$Source,[string[]]$Extra=@(),[bool]$ObjectOnly=$false){
 if(-not $taskCompilerArtifactUpperAccepted){throw 'SOURCE_HOLD_COMPILER_OBJECT_BINARY_AND_HELPER_BOUNDS'}
 if((Get-FileHash -LiteralPath $ClPath).Hash -cne '88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D'){throw 'Pinned compiler changed'}
 $taskObject=Join-Path $taskBuildRoot ($Name+'.obj');$taskBinary=Join-Path $taskBuildRoot ($Name+'.exe')
 $taskArgs=@('/nologo','/std:c++20','/EHsc','/W4','/WX','/MD','/permissive-','/fp:strict','/O2','/DNDEBUG',('/I'+(Join-Path $taskRepo 'src')),('/I'+$taskBase),('/I'+(Join-Path $taskRepo 'build-ci/_deps/signalsmith_stretch-src')),('/I'+$taskPrivateRoot),('/Fo:'+$taskObject))
 if($ObjectOnly){$taskArgs+='/c'}else{$taskArgs+=('/Fe:'+$taskBinary)}
 $taskArgs+=@($Source)+$Extra;if(-not $ObjectOnly){$taskArgs+=@('/link','/SUBSYSTEM:CONSOLE')}
 $taskExit=Invoke-Hidden ('compile-'+$Name) $ClPath $taskArgs 180;if($taskExit -ne 0){throw 'Actual compiler failure; no retry'}
 if(-not $ObjectOnly){Confirm-Console $taskBinary};return $(if($ObjectOnly){$taskObject}else{$taskBinary})
}
Save-TaskReceipt
try {
 if($Stage -ceq 'DefinitionFreeze'){
  $taskExit=Invoke-Hidden 'definition' $PythonPath @('-B',(Join-Path $taskBase 'freeze_waveform.py'),'--repo',$taskRepo,'--output',(Join-Path $taskRoot 'data')) 1800
  if($taskExit -ne 0){throw 'Actual DefinitionFreeze failure'}
 } elseif($Stage -ceq 'OracleFreeze'){
  New-Item -ItemType Directory -Path (Join-Path $taskRoot 'references')|Out-Null
  $taskBinary=Compile-Unit 'clock_oracle' (Join-Path $taskBase 'clock_oracle.cpp')
  $taskExit=Invoke-Hidden 'oracle' $taskBinary @((Join-Path $taskRoot 'references/clock')) 1800;if($taskExit -ne 0){throw 'Actual oracle failure'}
 } elseif($Stage -ceq 'PlanPreflight'){
  New-Item -ItemType Directory -Path (Join-Path $taskRoot 'references')|Out-Null
  $taskBinary=Compile-Unit 'plan_probe' (Join-Path $taskBase 'plan_probe.cpp')
  $taskBundle=Get-Content -LiteralPath $ReviewedInputBundle -Raw|ConvertFrom-Json
  $taskExit=Invoke-Hidden 'plan' $taskBinary @($taskBundle.frozen_clock_root,(Join-Path $taskRoot 'references/plans')) 900;if($taskExit -ne 0){throw 'Actual plan failure'}
 } else {
  $taskBundle=Get-Content -LiteralPath $ReviewedInputBundle -Raw|ConvertFrom-Json
  if($taskBundle.generated_lineage_accepted -cne $true){throw 'SOURCE_HOLD_GENERATED_SOURCE_LINEAGE'};if($Stage -cne 'AnalyticSelfCheck' -and ($taskBundle.complete_context_accepted -cne $true -or $taskBundle.complete_resources_accepted -cne $true)){throw 'SOURCE_HOLD_MATCHED_CONTEXT_AND_FULL_RESOURCES'}
  $taskPrivateRoot=[IO.Path]::GetFullPath($taskBundle.private_source_root)
  if(-not $taskPrivateRoot.StartsWith($taskBase+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Private-source root must stay inside reviewed revision'}
  $taskChecker=Compile-Unit 'waveform_check' (Join-Path $taskBase 'waveform_check.cpp') @() $true
  $taskCheckerHash=(Get-FileHash -LiteralPath $taskChecker).Hash
  $taskVariants=@('baseline','skip_late_finite','ignore_last_mismatch','swap_stored_channels','corrupt_constructed_output');$taskBinaries=@{}
  foreach($taskVariant in $taskVariants){$taskBinaries[$taskVariant]=Compile-Unit ($taskVariant+'-probe') (Join-Path $taskBase 'waveform_probe.cpp') @($taskChecker);if((Get-FileHash -LiteralPath $taskChecker).Hash -cne $taskCheckerHash){throw 'Independent checker object changed between links'}}
  if($Stage -ceq 'AnalyticSelfCheck'){
   $taskR5=Join-Path $taskRepo 'build-ci/20261105-musical-waveform-quality-prep/quality-calibration-r5/root-runs/Freeze-20261105T131446.163733Z-53d8220c/freeze'
   $taskExit=Invoke-Hidden 'analytic' $taskBinaries.baseline @('analytic',(Join-Path $taskR5 'calibration.tsv'),(Join-Path $taskR5 'clock.tsv'),(Join-Path $taskR5 'whole-cases.tsv'),(Join-Path $taskR5 'whole-windows.tsv'),(Join-Path $taskRoot 'analytic'),'1125','690') 1800
   if($taskExit -ne 0){throw 'Actual new-driver analytic self-check failed'}
  } else {
   $taskCases=Get-Content -LiteralPath $taskBundle.case_inventory -Raw|ConvertFrom-Json
   $taskAllowed=@($taskManifest.case_ids);if(@(Compare-Object $taskAllowed @($taskCases.ids)).Count -ne 0 -or $taskAllowed.Count -ne 294){throw 'Complete requested case inventory changed'}
   if($Stage -ceq 'ActualOwnGateControls'){$taskRows=@($taskBundle.control_invocations)}else{$taskRows=@($taskCases.cases)}
   foreach($taskCase in $taskRows){
    if($null -eq $taskCase.dsp_k -or $taskCase.actual_queried_layout -cne $true -or $null -eq $taskCase.matched_context_certificate -or $taskCase.matched_context_certificate.status -cne 'MATCHED_CURRENT_9MAP_PHYSICAL_CONTEXT'){throw 'SOURCE_HOLD_UNQUALIFIED_CONFIG_OR_MATCHED_CONTEXT'}
    $taskVariant=if($taskCase.variant){$taskCase.variant}else{'baseline'};if($taskVariant -cnotin $taskVariants){throw 'Unknown private source variant'}
    if($taskCase.id -cnotin $taskAllowed -and $taskCase.id -cnotin @($taskManifest.control_ids)){throw 'Unlisted case/control output id'}
    $taskCaseDir=Join-Path $taskRoot ($(if($Stage -ceq 'ActualOwnGateControls'){'controls/'}else{'case-results/'})+$taskCase.id);New-Item -ItemType Directory -Path $taskCaseDir|Out-Null;$taskCase.receipt=Join-Path $taskCaseDir $(if($Stage -ceq 'ActualOwnGateControls'){'witness.bin'}else{'windows.bin'})
    $taskLeft=if([IO.Path]::IsPathRooted($taskCase.left)){$taskCase.left}else{Join-Path $taskBundle.reference_root $taskCase.left};$taskRight=if([IO.Path]::IsPathRooted($taskCase.right)){$taskCase.right}else{Join-Path $taskBundle.reference_root $taskCase.right};$taskQ=Join-Path $taskBundle.frozen_clock_root ('r'+$taskCase.rate+'-map'+$taskCase.map+'-q.bin');$taskInverse=Join-Path $taskBundle.frozen_inverse_root 'r48000-map5.bin'
    $taskArgs=@($taskCase.mode,[string]$taskCase.rate,[string]$taskCase.map,[string]$taskCase.logical,$taskLeft,$taskRight,$taskQ,$taskCase.receipt,[string]$taskCase.dsp_k,$taskCase.owner_sha256,[string]$taskCase.frequencies[0],[string]$taskCase.frequencies[1],$taskInverse,[string]$taskCase.shape,[string]$taskCase.curve,[string]$taskCase.fade_in,[string]$taskCase.fade_out)
    $taskExit=Invoke-Hidden $taskCase.id $taskBinaries[$taskVariant] $taskArgs 300
    if($Stage -ceq 'ActualOwnGateControls'){
     if(-not $taskCase.unchanged_own_gate_marker){throw 'Frozen own-gate selector required'}
     $taskLast=$taskReceipt.children[-1];$taskRaw=@();foreach($taskWhich in @('stdout','stderr')){$taskStream=[IO.File]::OpenRead($taskLast[($taskWhich+'_path')]);try{$null=$taskStream.Seek([long]$taskLast[($taskWhich+'_start')],[IO.SeekOrigin]::Begin);$taskLength=[long]$taskLast[($taskWhich+'_bytes')];if($taskLength -gt 262144){throw 'Own-gate parse reservation'};$taskRawBytes=[byte[]]::new([int]$taskLength);$taskStream.ReadExactly($taskRawBytes,0,$taskRawBytes.Length);$taskRaw+=[Text.Encoding]::UTF8.GetString($taskRawBytes)}finally{$taskStream.Dispose()}}
     if(-not ($taskRaw -join "`n").Contains($taskCase.unchanged_own_gate_marker)){throw 'Own gate NOT_REACHED; unrelated failure earns no credit'}
    } elseif($taskExit -ne 0){throw 'Actual baseline failed; retain and stop'}
   }
  }
 }
  $taskArtifacts=@();foreach($taskFile in @(Get-ChildItem -LiteralPath $taskRoot -Recurse -File)){if($taskFile.Name -notin @('bundle.json','consumed.json')){$taskArtifacts+=@{path=$taskFile.FullName;bytes=$taskFile.Length;sha256=(Get-FileHash -LiteralPath $taskFile.FullName).Hash}}}
 $taskTotal=0L;foreach($taskArtifact in $taskArtifacts){if($taskArtifact.bytes -gt (840MB-$taskTotal)){throw 'Retained-stage sum exceeds prepaid envelope'};$taskTotal+=$taskArtifact.bytes}
 $taskReferenceOut=if($Stage -ceq 'DefinitionFreeze'){Join-Path $taskRoot 'data'}else{$taskInputData.reference_root};$taskPrivateOut=if($Stage -ceq 'DefinitionFreeze'){Join-Path $taskRoot 'data/generated/private-sources'}else{$taskInputData.private_source_root};$taskClockOut=if($Stage -ceq 'OracleFreeze'){Join-Path $taskRoot 'references/clock'}elseif($Stage -ceq 'DefinitionFreeze'){$null}else{$taskInputData.frozen_clock_root};$taskInverseOut=if($Stage -ceq 'OracleFreeze'){Join-Path $taskRoot 'references/native-inverse'}elseif($Stage -ceq 'DefinitionFreeze'){$null}else{$taskInputData.frozen_inverse_root};$taskInventoryOut=if($Stage -ceq 'DefinitionFreeze'){Join-Path $taskRoot 'data/references/original-case-inventory.json'}else{$taskInputData.case_inventory}
 $taskBundleOut=[ordered]@{stage=$Stage;source_manifest_sha256=$ReviewedManifestSha256;artifacts=$taskArtifacts;reviewed_input_bundle=$ReviewedInputBundle;reviewed_input_bundle_sha256=$ReviewedInputBundleSha256;reference_root=$taskReferenceOut;private_source_root=$taskPrivateOut;frozen_clock_root=$taskClockOut;frozen_inverse_root=$taskInverseOut;case_inventory=$taskInventoryOut;complete_context_accepted=$false;complete_resources_accepted=$false;generated_lineage_accepted=$false;independent_actual_output_review='REQUIRED';production_admission=$false}
 $taskBundleText=($taskBundleOut|ConvertTo-Json -Depth 16).Replace("`r`n","`n")+"`n";if([Text.Encoding]::UTF8.GetByteCount($taskBundleText) -gt 262144){throw 'Bundle metadata reservation'}
 [IO.File]::WriteAllText((Join-Path $taskStageRoot 'bundle.json'),$taskBundleText,[Text.UTF8Encoding]::new($false))
 $taskReceipt.status='STAGE_EXIT_ZERO_REQUIRES_FRESH_ACTUAL_OUTPUT_REVIEW';Save-TaskReceipt
} catch {$taskReceipt.status='FAILED_OR_UNREACHED_NO_CERTIFICATION';$taskReceipt.error=$_.Exception.Message;Save-TaskReceipt;throw}
