#Requires -Version 7.0
param(
 [Parameter(Mandatory)][ValidateSet('DefinitionFreeze')][string]$Stage,
 [Parameter(Mandatory)][string]$Repo,
 [Parameter(Mandatory)][string]$NewRunRoot,
 [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ReviewedManifestSha256,
 [Parameter(Mandatory)][string]$PythonPath,
 [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ReviewedPythonSha256,
 [string]$ReviewedInputBundle,
 [string]$ReviewedInputBundleSha256
)
$ErrorActionPreference='Stop'
$taskDefinitionSourceAccepted=$false
$taskStageExecutionAccepted=$false
$taskCompilerVendorContextQualityProductionRendererAccepted=$false
if(-not $taskDefinitionSourceAccepted -or -not $taskStageExecutionAccepted){throw 'SOURCE_HOLD_NO_FREEZE_OR_EXECUTION_GO'}
$taskBase=[IO.Path]::GetFullPath($PSScriptRoot)
$taskRepo=[IO.Path]::GetFullPath($Repo)
$taskRoot=[IO.Path]::GetFullPath($NewRunRoot)
$taskPython=[IO.Path]::GetFullPath($PythonPath)
$taskManifestPath=Join-Path $taskBase 'authored-manifest.json'
$taskExpectedManifest=$ReviewedManifestSha256.ToUpperInvariant()
$taskExpectedPython=$ReviewedPythonSha256.ToUpperInvariant()
$taskOwnNames=@('WaveformCandidate.h','waveform_probe.cpp','waveform_check.cpp','clock_oracle.cpp','plan_probe.cpp','independent_clock.h','fast_fit.h','freeze_waveform.py','verify_waveform.py','run-waveform.ps1','README.md')
$taskReservedWork=14896615424L
$taskChildWork=7447259136L
$taskRunnerWork=2097152L
$taskRetainedCap=141459456L
$taskDataCap=74350592L
$taskStageCap=67108864L
$taskScratch=33554432L
if($taskRunnerWork+2L*$taskChildWork -ne $taskReservedWork -or $taskReservedWork -gt 17179869184L -or $taskDataCap+$taskStageCap -ne $taskRetainedCap -or 18874368L -gt $taskScratch){throw 'Whole first-stage prepayment'}
$taskUnits=0L;$taskRegion=0;$taskReceiptWrites=0;$taskRawLogBytes=0L;$taskQuarantine=$null;$taskManifest=$null
$taskOwnedInputs=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
$taskReceipt=[ordered]@{schema=1;stage=$Stage;status='NOT_STARTED_NO_CERTIFICATION';source_manifest_sha256=$taskExpectedManifest;reserved_work=$taskReservedWork;reserved_retained_bytes=$taskRetainedCap;controlled_payload_upper=18874368L;reserved_controlled_payload=33554432L;generator_exit=$null;replay_exit=$null;compiler_called=$false;compiler_exit=$null;children=[Collections.Generic.List[object]]::new();vendor=$false;context=$false;queried_layout=$false;quality=$false;production_admission=$false;renderer=$false;native_app=$false;audio_device=$false}
function Spend-Task([long]$n){if($n -lt 0 -or $n -gt 1048576L-$script:taskUnits){throw 'Prepaid runner-region source limit'};$script:taskUnits+=$n}
function Start-TaskRegion([int]$id){if($id -ne $script:taskRegion+1 -or $id -gt 2){throw 'No runner reservation reset/reuse'};if($id -eq 2){$script:taskReceipt['runner_region1_used']=$script:taskUnits};$script:taskRegion=$id;$script:taskUnits=0L;Spend-Task 131072}
function Assert-NoLink([string]$Path){
 $taskAt=[IO.Path]::GetFullPath($Path);$taskDepth=0
 while($taskAt){Spend-Task 1;$taskDepth++;if($taskDepth -gt 64){throw 'Path depth'};if([IO.File]::Exists($taskAt) -or [IO.Directory]::Exists($taskAt)){if(([IO.File]::GetAttributes($taskAt)-band [IO.FileAttributes]::ReparsePoint)-ne 0){throw 'Reparse path refused'}};$taskAt=[IO.Path]::GetDirectoryName($taskAt)}
}
function Get-TaskHash([string]$Path,[long]$Maximum){
 Spend-Task 64;Assert-NoLink $Path;$taskItem=Get-Item -LiteralPath $Path
 if($taskItem.PSIsContainer -or $taskItem.Length -gt $Maximum -or $taskItem.Length -lt 0){throw 'Hash file cap'}
 $taskStream=$null;$taskHash=$null;$taskBuffer=[byte[]]::new(65536);$taskSeen=0L
 try{$taskStream=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,65536,[IO.FileOptions]::SequentialScan);$taskHash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
  while($true){Spend-Task 32;$taskN=$taskStream.Read($taskBuffer,0,65536);if($taskN -eq 0){break};if($taskN -gt $Maximum-$taskSeen){throw 'Hash input grew'};$taskSeen+=$taskN;$taskHash.AppendData($taskBuffer,0,$taskN)}
  return @{bytes=$taskSeen;sha256=[Convert]::ToHexString($taskHash.GetHashAndReset())}
 }finally{if($null -ne $taskHash){$taskHash.Dispose()};if($null -ne $taskStream){$taskStream.Dispose()}}
}
function Read-TaskJson([string]$Path,[long]$Maximum=262144){
 Spend-Task 64;$taskInfo=Get-Item -LiteralPath $Path;if($taskInfo.Length -gt $Maximum){throw 'JSON input cap'}
 $taskText=[IO.File]::ReadAllText($Path,[Text.Encoding]::UTF8)
 return ConvertFrom-Json -InputObject $taskText -AsHashtable -Depth 16
}
function Add-TaskInput([string]$Path,[long]$Bytes,[string]$Hash,[string]$Origin){
 $taskAbsolute=[IO.Path]::GetFullPath($Path);if($taskAbsolute.Length -gt 512 -or $taskAbsolute.StartsWith('\\') -or $Bytes -lt 0 -or $Hash -cnotmatch '^[0-9A-Fa-f]{64}$' -or $Origin -cnotmatch '^[0-9A-Fa-f]{64}$'){throw 'Input descriptor'}
 $taskRow=@{path=$taskAbsolute;bytes=$Bytes;sha256=$Hash.ToUpperInvariant();origin_bundle_sha256=$Origin}
 if($script:taskOwnedInputs.ContainsKey($taskAbsolute)){$taskOld=$script:taskOwnedInputs[$taskAbsolute];if($taskOld['bytes'] -ne $Bytes -or $taskOld['sha256'] -cne $taskRow['sha256']){throw 'Conflicting transitive path/hash alias'}}else{$script:taskOwnedInputs.Add($taskAbsolute,$taskRow)}
 if($script:taskOwnedInputs.Count -gt 256){throw 'Finite transitive input count'}
}
function Assert-TaskPins {
 $taskActualManifest=Get-TaskHash $taskManifestPath 262144
 if($taskActualManifest['sha256'] -cne $taskExpectedManifest){throw 'External current manifest hash mismatch'}
 $taskPins=@($taskManifest['sources']);if($taskPins.Count -ne 11){throw 'Exactly current own11 required'}
 $taskSeen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($taskPin in $taskPins){$taskName=[string]$taskPin['path'];if($taskName -cnotin $taskOwnNames -or -not $taskSeen.Add($taskName)){throw 'Current own11 exact path inventory'};$taskPath=Join-Path $taskBase $taskName;$taskObserved=Get-TaskHash $taskPath ([long]$taskPin['bytes']);if($taskObserved['bytes'] -ne $taskPin['bytes'] -or $taskObserved['sha256'] -cne $taskPin['sha256']){throw ('Current authored file changed: '+$taskName)}}
 $taskTotal=0L
 foreach($taskInput in $taskOwnedInputs.Values){$taskObserved=Get-TaskHash $taskInput['path'] ([long]$taskInput['bytes']);if($taskObserved['bytes'] -ne $taskInput['bytes'] -or $taskObserved['sha256'] -cne $taskInput['sha256']){throw ('Complete input changed: '+$taskInput['path'])};if($taskInput['bytes'] -gt 8388608L-$taskTotal){throw 'Complete input byte profile'};$taskTotal+=[long]$taskInput['bytes']}
 $taskPy=Get-TaskHash $taskPython 8388608;if($taskPy['sha256'] -cne $taskExpectedPython){throw 'Immediately fenced Python artifact changed'}
}
function Encode-TaskJson($Value){$taskEncoded=[Text.UTF8Encoding]::new($false).GetBytes((($Value|ConvertTo-Json -Depth 16 -Compress).Replace("`r`n","`n"))+"`n");if($taskEncoded.Length -gt 262144){throw 'Receipt/bundle encoded cap'};return ,$taskEncoded}
function Get-TaskStageBytes {
 $taskSum=$script:taskAuthoredBytes
 if([IO.Directory]::Exists($script:taskStageRoot)){
  foreach($taskFile in Get-ChildItem -LiteralPath $script:taskStageRoot -Force){Spend-Task 1;if($taskFile.PSIsContainer){throw 'Unlisted stage subdirectory'};if($taskFile.Name -cnotin @('stdout.log','stderr.log','receipt.json','bundle.json','consumed.json')){throw 'Unlisted stage file'};if($taskFile.Length -gt $taskStageCap-$taskSum){throw 'Whole stage pool'};$taskSum+=[long]$taskFile.Length}
 }
 return $taskSum
}
function Write-TaskJson([string]$Path,$Value){
 $taskBytes=Encode-TaskJson $Value;$taskOld=if([IO.File]::Exists($Path)){(Get-Item -LiteralPath $Path).Length}else{0L};$taskCurrent=Get-TaskStageBytes
 if($taskBytes.Length -gt $taskStageCap-($taskCurrent-$taskOld)){throw 'Before-write whole stage pool'}
 [IO.File]::WriteAllBytes($Path,$taskBytes)
}
function Save-TaskReceipt {if($script:taskReceiptWrites -ge 8){throw 'Eight prepaid receipt writes'};$script:taskReceiptWrites++;Write-TaskJson (Join-Path $script:taskStageRoot 'receipt.json') $script:taskReceipt}
function Invoke-TaskChild([string]$Name,[string[]]$Arguments){
 Spend-Task 4096;Spend-Task 8192;if($null -ne $script:taskQuarantine){throw 'Unsettled prior child owns storage; no next child'}
 $taskProcess=$null;$taskStarted=$false;$taskCancel=$null;$taskOut=$null;$taskErr=$null;$taskReadA=$null;$taskReadB=$null;$taskAccountA=$true;$taskAccountB=$true
 $taskA=[byte[]]::new(4096);$taskB=[byte[]]::new(4096);$taskDoneA=$false;$taskDoneB=$false;$taskExit=$null;$taskExitObserved=$false;$taskFailure=$null;$taskTimedOut=$false;$taskCap=$false
 $taskObserved=0L;$taskUnretained=0L;$taskWritten=0L;$taskOmitted=0L;$taskSettled=$true;$taskCleanup=[Collections.Generic.List[string]]::new();$taskBeforeA=0L;$taskBeforeB=0L
 try{
  $taskCancel=[Threading.CancellationTokenSource]::new()
  $taskOut=[IO.FileStream]::new((Join-Path $taskStageRoot 'stdout.log'),[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
  $taskErr=[IO.FileStream]::new((Join-Path $taskStageRoot 'stderr.log'),[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
  $taskBeforeA=$taskOut.Position;$taskBeforeB=$taskErr.Position
  $taskInfo=[Diagnostics.ProcessStartInfo]::new();$taskInfo.FileName=$taskPython;$taskInfo.WorkingDirectory=$taskRoot;$taskInfo.UseShellExecute=$false;$taskInfo.CreateNoWindow=$true;$taskInfo.RedirectStandardOutput=$true;$taskInfo.RedirectStandardError=$true
  foreach($taskArgument in $Arguments){$taskInfo.ArgumentList.Add($taskArgument)}
  $taskProcess=[Diagnostics.Process]::Start($taskInfo);$taskStarted=$true;$taskWatch=[Diagnostics.Stopwatch]::StartNew()
  $taskReadA=$taskProcess.StandardOutput.BaseStream.ReadAsync($taskA,0,4096,$taskCancel.Token);$taskAccountA=$false
  $taskReadB=$taskProcess.StandardError.BaseStream.ReadAsync($taskB,0,4096,$taskCancel.Token);$taskAccountB=$false
  while(-not($taskDoneA -and $taskDoneB -and $taskProcess.HasExited)){
   Spend-Task 64
   foreach($taskDirection in 0,1){
    $taskDone=if($taskDirection -eq 0){$taskDoneA}else{$taskDoneB};$taskRead=if($taskDirection -eq 0){$taskReadA}else{$taskReadB}
    if(-not $taskDone -and $taskRead.IsCompleted){
     Spend-Task 128;$taskN=$taskRead.GetAwaiter().GetResult();if($taskDirection -eq 0){$taskAccountA=$true}else{$taskAccountB=$true};$taskObserved+=$taskN;$taskUnretained+=$taskN
     if($taskN -eq 0){if($taskDirection -eq 0){$taskDoneA=$true}else{$taskDoneB=$true}}else{
      if($taskN -gt 1048576L-$taskWritten){$taskCap=$true;$taskOmitted+=$taskN;throw 'Controlled child raw-log cap; prefix retained'}
      $taskCurrent=Get-TaskStageBytes;if($taskN -gt $taskStageCap-$taskCurrent){$taskCap=$true;$taskOmitted+=$taskN;throw 'Before-write complete stage log pool'}
      [byte[]]$taskBuffer=$null;$taskSink=$null;if($taskDirection -eq 0){$taskBuffer=$taskA;$taskSink=$taskOut}else{$taskBuffer=$taskB;$taskSink=$taskErr}
      $taskSink.Write($taskBuffer,0,$taskN);$taskWritten+=$taskN;$script:taskRawLogBytes+=$taskN;$taskUnretained-=$taskN
      if($taskDirection -eq 0){$taskReadA=$taskProcess.StandardOutput.BaseStream.ReadAsync($taskA,0,4096,$taskCancel.Token);$taskAccountA=$false}else{$taskReadB=$taskProcess.StandardError.BaseStream.ReadAsync($taskB,0,4096,$taskCancel.Token);$taskAccountB=$false}
     }
    }
   }
   if($taskWatch.Elapsed.TotalSeconds -gt 1800){$taskTimedOut=$true;throw 'Owned child1800s timeout'}
   $null=$taskProcess.WaitForExit(250)
  }
  $taskExit=$taskProcess.ExitCode;$taskExitObserved=$true
 }catch{$taskFailure=$_.Exception.Message.Substring(0,[Math]::Min(512,$_.Exception.Message.Length))}
 finally{
  if($null -ne $taskCancel){try{$taskCancel.Cancel()}catch{$taskCleanup.Add('read_cancel')}}
  if($taskStarted){try{if(-not $taskProcess.HasExited){$taskProcess.Kill($true);$null=$taskProcess.WaitForExit(2000)};if($taskProcess.HasExited){$taskExit=$taskProcess.ExitCode;$taskExitObserved=$true}else{$taskCleanup.Add('own_child_exit_not_observed_2000ms')}}catch{$taskCleanup.Add('own_child_reap_failure')}}
  [Threading.Tasks.Task[]]$taskReads=@();if($null -ne $taskReadA){$taskReads+=@($taskReadA)};if($null -ne $taskReadB){$taskReads+=@($taskReadB)}
  if($taskReads.Length -gt 0){try{$null=[Threading.Tasks.Task]::WaitAll($taskReads,2000)}catch{$taskCleanup.Add('read_settlement_diagnostic')}}
  foreach($taskDirection in 0,1){
   $taskRead=if($taskDirection -eq 0){$taskReadA}else{$taskReadB};$taskAccount=if($taskDirection -eq 0){$taskAccountA}else{$taskAccountB}
   if($null -ne $taskRead){if(-not $taskRead.IsCompleted){$taskSettled=$false}else{if(-not $taskAccount){try{$taskN=$taskRead.GetAwaiter().GetResult();$taskObserved+=$taskN;$taskUnretained+=$taskN;if($taskN -eq 0){if($taskDirection -eq 0){$taskDoneA=$true}else{$taskDoneB=$true}}}catch{$taskCleanup.Add('settled_read_cancel_or_fault')}}}}
  }
  foreach($taskSink in @($taskOut,$taskErr)){if($null -ne $taskSink){try{$taskSink.Flush()}catch{$taskCleanup.Add('retained_prefix_flush')}}}
  if($taskSettled -and (-not $taskStarted -or $taskExitObserved)){
   [Array]::Clear($taskA,0,$taskA.Length);[Array]::Clear($taskB,0,$taskB.Length)
   foreach($taskOwner in @($taskOut,$taskErr,$taskCancel,$taskProcess)){if($null -ne $taskOwner){try{$taskOwner.Dispose()}catch{$taskCleanup.Add('settled_owner_dispose')}}}
  }else{
   $script:taskQuarantine=@{process=$taskProcess;cancel=$taskCancel;readA=$taskReadA;readB=$taskReadB;bufferA=$taskA;bufferB=$taskB;stdout=$taskOut;stderr=$taskErr};$taskCleanup.Add('one_rooted_unsettled_owner_no_more_children')
  }
  if($taskCleanup.Count -gt 0 -and $null -eq $taskFailure){$taskFailure='Bounded cleanup refusal'}
  $taskRow=[ordered]@{name=$Name;started=$taskStarted;raw_exit_observed=$taskExitObserved;raw_exit=$taskExit;timed_out=$taskTimedOut;log_cap=$taskCap;stdout_start=$taskBeforeA;stderr_start=$taskBeforeB;raw_bytes_retained=$taskWritten;raw_bytes_observed=$taskObserved;observed_not_confirmed_retained=$taskUnretained;omitted_at_cap=$taskOmitted;unknown_undrained=($taskStarted -and -not($taskDoneA -and $taskDoneB));both_eof=($taskDoneA -and $taskDoneB);reads_settled=$taskSettled;owned_storage_quarantined=($null -ne $script:taskQuarantine);cleanup=$taskCleanup.ToArray();failure=$taskFailure}
  $script:taskReceipt['children'].Add($taskRow)
  if($taskExitObserved){if($Name -ceq 'generation'){$script:taskReceipt['generator_exit']=$taskExit}else{$script:taskReceipt['replay_exit']=$taskExit}}
 }
 if($null -ne $taskFailure){throw $taskFailure};if(-not $taskExitObserved -or $taskExit -ne 0){throw ('Actual child raw exit: '+$taskExit)}
 return $taskRow
}
function Read-TaskChildResult($Child){
 if(-not $Child['both_eof'] -or -not $Child['reads_settled'] -or -not $Child['raw_exit_observed'] -or $Child['raw_exit'] -ne 0){throw 'Incomplete child receipt'}
 $taskStream=[IO.File]::OpenRead((Join-Path $taskStageRoot 'stdout.log'))
 try{$null=$taskStream.Seek([long]$Child['stdout_start'],[IO.SeekOrigin]::Begin);$taskLength=$taskStream.Length-$taskStream.Position;if($taskLength -gt 65536){throw 'Child summary cap'};$taskBytes=[byte[]]::new([int]$taskLength);$taskStream.ReadExactly($taskBytes,0,$taskBytes.Length);return ConvertFrom-Json -InputObject ([Text.Encoding]::UTF8.GetString($taskBytes)) -AsHashtable -Depth 16}finally{$taskStream.Dispose()}
}
function Get-TaskData {
 $taskResult=[Collections.Generic.List[object]]::new();$taskQueue=[Collections.Generic.Queue[object]]::new();$taskQueue.Enqueue(@($taskDataRoot,0));$taskDirs=0;$taskSum=0L
 $taskSeen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 while($taskQueue.Count -gt 0){$taskEntry=$taskQueue.Dequeue();$taskPath=[string]$taskEntry[0];$taskDepth=[int]$taskEntry[1];Assert-NoLink $taskPath;$taskDirs++;if($taskDirs -gt 128 -or $taskDepth -gt 8){throw 'Data directory geometry'}
  foreach($taskItem in Get-ChildItem -LiteralPath $taskPath -Force){if($taskItem.PSIsContainer){Assert-NoLink $taskItem.FullName;$taskQueue.Enqueue(@($taskItem.FullName,$taskDepth+1));continue};$taskRel=[IO.Path]::GetRelativePath($taskDataRoot,$taskItem.FullName).Replace([char]92,[char]47);if(-not $taskCaps.ContainsKey($taskRel) -or -not $taskSeen.Add($taskRel)){throw 'Exact92 subset/partial/unknown output'};$taskHash=Get-TaskHash $taskItem.FullName ([long]$taskCaps[$taskRel]);if($taskHash['bytes'] -gt $taskDataCap-$taskSum){throw 'Whole data aggregate'};$taskSum+=[long]$taskHash['bytes'];$taskResult.Add(@{path=$taskItem.FullName;relative_path=$taskRel;bytes=$taskHash['bytes'];sha256=$taskHash['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 }
 if($taskSeen.Count -ne 92){throw 'All92 data artifacts required'}
 return @{files=$taskResult.ToArray();bytes=$taskSum}
}
function Assert-TaskSameData($Before,$After){if($Before['bytes'] -ne $After['bytes']){throw 'Data byte sum changed'};$taskOld=@{};foreach($taskRow in $Before['files']){$taskOld[$taskRow['relative_path']]=$taskRow};foreach($taskRow in $After['files']){$taskPrior=$taskOld[$taskRow['relative_path']];if($null -eq $taskPrior -or $taskPrior['bytes'] -ne $taskRow['bytes'] -or $taskPrior['sha256'] -cne $taskRow['sha256']){throw 'After-replay generated92 fence changed'}}}
$taskCreated=$false;$taskStageRoot=$null;$taskDataRoot=$null;$taskAuthoredBytes=0L;$taskInputBundleData=$null
try{
 Start-TaskRegion 1
 if(-not $taskRoot.StartsWith((Join-Path $taskBase 'runs')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'New root outside this exact ignored source revision'}
 if($taskPython.StartsWith('\\') -or $taskRepo.StartsWith('\\')){throw 'Local filesystem inputs only'}
 Assert-NoLink $taskRoot
 if(Test-Path -LiteralPath $taskRoot){throw 'Never overwrite/retry a run root'}
 $taskRuns=Join-Path $taskBase 'runs';if((Test-Path -LiteralPath $taskRuns) -and @(Get-ChildItem -LiteralPath $taskRuns -Force).Count -gt 0){throw 'This revision already has its sole stage attempt; preserve it'}
 $taskHash=Get-TaskHash $taskManifestPath 262144;if($taskHash['sha256'] -cne $taskExpectedManifest){throw 'External manifest mismatch'}
 $taskManifest=Read-TaskJson $taskManifestPath
 if($taskManifest['stage'] -cne 'AUTHORED_SOURCE_ONLY_DEFINITION_FIRST_STAGE_HOLD'){throw 'Exact first-stage manifest schema'}
 $taskCaps=[Collections.Generic.Dictionary[string,long]]::new([StringComparer]::Ordinal)
 $taskPrivateCaps=0L;$taskReferenceCaps=0L
 foreach($taskArtifact in $taskManifest['stage_artifacts']){$taskName=[string]$taskArtifact['path'];$taskBytes=[long]$taskArtifact['bytes_cap'];if($taskBytes -lt 0 -or $taskName.Contains('..') -or $taskName.Contains(':') -or $taskName.Contains([char]92)){throw 'Reviewed relative output path'};$taskCaps.Add($taskName,$taskBytes);if($taskName.StartsWith('generated/')){$taskPrivateCaps+=$taskBytes}else{$taskReferenceCaps+=$taskBytes}}
 if($taskCaps.Count -ne 92 -or $taskPrivateCaps -ne 3014656L -or $taskReferenceCaps -ne 71335936L -or $taskPrivateCaps+$taskReferenceCaps -ne $taskDataCap){throw 'Exact whole92 before-write reservations'}
 foreach($taskPin in $taskManifest['sources']){$taskBytes=[long]$taskPin['bytes'];if($taskBytes -lt 0 -or $taskBytes -gt 1048576L-$taskAuthoredBytes){throw 'Current authored input pool'};$taskAuthoredBytes+=$taskBytes;Add-TaskInput (Join-Path $taskBase $taskPin['path']) $taskBytes $taskPin['sha256'] $taskExpectedManifest}
 $taskAuthoredBytes+=$taskHash['bytes'];if($taskAuthoredBytes -gt 1048576L){throw 'Current twelve source payload cap'}
 Add-TaskInput $taskManifestPath $taskHash['bytes'] $taskExpectedManifest $taskExpectedManifest
 foreach($taskPin in $taskManifest['preserved_pins']){$taskPath=if([IO.Path]::IsPathRooted($taskPin['path'])){$taskPin['path']}else{Join-Path $taskRepo $taskPin['path']};Add-TaskInput $taskPath ([long]$taskPin['bytes']) $taskPin['sha256'] $taskExpectedManifest}
 $taskPy=Get-TaskHash $taskPython 8388608;if($taskPy['sha256'] -cne $taskExpectedPython){throw 'Reviewed Python executable mismatch'};Add-TaskInput $taskPython $taskPy['bytes'] $taskExpectedPython $taskExpectedManifest
 if($ReviewedInputBundle -or $ReviewedInputBundleSha256){
  if(-not $ReviewedInputBundle -or $ReviewedInputBundleSha256 -cnotmatch '^[0-9A-Fa-f]{64}$'){throw 'Explicit reviewed predecessor hash pair'}
  $taskPendingBundles=[Collections.Generic.Queue[object]]::new();$taskPendingBundles.Enqueue(@{path=$ReviewedInputBundle;sha256=$ReviewedInputBundleSha256.ToUpperInvariant();depth=0})
  $taskSeenBundles=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
  while($taskPendingBundles.Count -gt 0){
   $taskPredecessor=$taskPendingBundles.Dequeue();$taskPriorPath=[IO.Path]::GetFullPath($taskPredecessor['path']);if($taskPriorPath.StartsWith('\\') -or $taskPredecessor['depth'] -gt 8 -or -not $taskSeenBundles.Add($taskPriorPath)){throw 'Predecessor cycle/network/depth refused'}
   $taskPriorHash=Get-TaskHash $taskPriorPath 262144;if($taskPriorHash['sha256'] -cne $taskPredecessor['sha256']){throw 'Predecessor bundle changed'}
   Add-TaskInput $taskPriorPath $taskPriorHash['bytes'] $taskPriorHash['sha256'] $taskPriorHash['sha256'];$taskPriorData=Read-TaskJson $taskPriorPath
   foreach($taskKey in @('artifacts','transitive_inputs')){if(-not $taskPriorData.Contains($taskKey)){throw 'Full predecessor artifact closure missing'};foreach($taskInput in $taskPriorData[$taskKey]){Add-TaskInput $taskInput['path'] ([long]$taskInput['bytes']) $taskInput['sha256'] $taskInput['origin_bundle_sha256']}}
   if($taskPriorData['reviewed_predecessor_bundle']){if($taskPriorData['reviewed_predecessor_sha256'] -cnotmatch '^[0-9A-Fa-f]{64}$'){throw 'Ancestor bundle hash missing'};$taskPendingBundles.Enqueue(@{path=$taskPriorData['reviewed_predecessor_bundle'];sha256=$taskPriorData['reviewed_predecessor_sha256'].ToUpperInvariant();depth=([int]$taskPredecessor['depth']+1)})}
  }
 }
 if($taskOwnedInputs.Count -gt 136 -or $taskOwnedInputs.Count+11+2+92 -gt 256){throw 'Complete per-fence source/input/artifact profile'}
 Assert-TaskPins
 $null=New-Item -ItemType Directory -Path $taskRoot
 $taskCreated=$true;$taskStageRoot=Join-Path $taskRoot 'stages/DefinitionFreeze';$taskDataRoot=Join-Path $taskRoot 'data'
 $null=New-Item -ItemType Directory -Path $taskStageRoot
 $taskMarker=Encode-TaskJson @{single_attempt=$true;stage=$Stage;source_manifest_sha256=$taskExpectedManifest;reserved_work=$taskReservedWork;retained_cap=$taskRetainedCap}
 $taskMarkerStream=$null;try{$taskMarkerStream=[IO.File]::Open((Join-Path $taskStageRoot 'consumed.json'),[IO.FileMode]::CreateNew);$taskMarkerStream.Write($taskMarker,0,$taskMarker.Length)}finally{if($null -ne $taskMarkerStream){$taskMarkerStream.Dispose()}}
 $taskReceipt['status']='GENERATION_STARTED_NOT_CERTIFIED';Save-TaskReceipt
 $taskGen=$null
 try{$taskGen=Invoke-TaskChild 'generation' @('-B',(Join-Path $taskBase 'freeze_waveform.py'),'--repo',$taskRepo,'--output',$taskDataRoot,'--manifest-sha',$taskExpectedManifest,'--prior-work','2097152')}finally{Assert-TaskPins}
 $taskReceipt['generator_exit']=$taskGen['raw_exit'];$taskGeneration=Read-TaskChildResult $taskGen
 if($taskGeneration['status'] -cne 'GENERATED_SOURCE_ONLY_REPLAY_REQUIRED' -or $taskGeneration['artifact_count'] -ne 92 -or $taskGeneration['source_manifest_sha256'] -cne $taskExpectedManifest -or $taskGeneration['ledger']['reserved'] -ne $taskChildWork -or $taskGeneration['ledger']['composed_after'] -ne 7449356288L){throw 'Exact generator source ledger/result'}
 $taskBefore=Get-TaskData
 if($taskGeneration['retained_data_bytes'] -ne $taskBefore['bytes']){throw 'Actual generation byte sum mismatch'}
 $taskReceipt['generation']=$taskGeneration;$taskReceipt['runner_region1_used']=$taskUnits;$taskReceipt['status']='GENERATED_EXACT_REPLAY_PENDING';Save-TaskReceipt
 Start-TaskRegion 2
 Assert-TaskPins
 $taskReplay=$null
 try{$taskReplay=Invoke-TaskChild 'independent-replay' @('-B',(Join-Path $taskBase 'verify_waveform.py'),'--root',$taskDataRoot,'--repo',$taskRepo,'--manifest-sha',$taskExpectedManifest,'--reference-sha',$taskGeneration['reference_manifest_sha256'],'--prior-work','7449356288')}finally{Assert-TaskPins}
 $taskReceipt['replay_exit']=$taskReplay['raw_exit'];$taskVerified=Read-TaskChildResult $taskReplay
 if($taskVerified['status'] -cne 'DEFINITION_SOURCE_REPLAY_COMPLETE_REQUIRES_FRESH_ROOT_OUTPUT_REVIEW' -or $taskVerified['artifact_count'] -ne 92 -or $taskVerified['pcm_words'] -ne 14166000 -or $taskVerified['finite_pcm_words'] -ne 14165999 -or $taskVerified['ledger']['reserved'] -ne $taskChildWork -or $taskVerified['ledger']['composed_after'] -ne $taskReservedWork){throw 'Exact independent replay and cumulative work required'}
 $taskAfter=Get-TaskData;Assert-TaskSameData $taskBefore $taskAfter
 if($taskVerified['retained_data_bytes'] -ne $taskAfter['bytes']){throw 'Actual replay byte sum mismatch'}
 Assert-TaskPins
 $taskReceipt['verification']=$taskVerified;$taskReceipt['runner_region2_prefix_used']=$taskUnits;$taskReceipt['status']='DEFINITION_SOURCE_REPLAY_COMPLETE_REQUIRES_FRESH_ROOT_OUTPUT_REVIEW'
 $taskReceipt['retained_total_excluding_bundle']=0L
 $taskReceipt['retained_data_bytes']=$taskAfter['bytes']
 $taskCurrentStage=Get-TaskStageBytes;$taskReceiptPath=Join-Path $taskStageRoot 'receipt.json';$taskOldReceipt=(Get-Item -LiteralPath $taskReceiptPath).Length
 # Resolve only an integer length fixed point; there is no self-hash dependency.
 $taskStable=$false
 for($taskPass=0;$taskPass -lt 4;$taskPass++){$taskRaw=Encode-TaskJson $taskReceipt;$taskExact=[long]$taskAfter['bytes']+$taskCurrentStage-$taskOldReceipt+$taskRaw.Length;if($taskReceipt['retained_total_excluding_bundle'] -eq $taskExact){$taskStable=$true;break};$taskReceipt['retained_total_excluding_bundle']=$taskExact}
 if(-not $taskStable){throw 'Final receipt byte-length fixed point'}
 Save-TaskReceipt
 $taskArtifacts=[Collections.Generic.List[object]]::new();foreach($taskRow in $taskAfter['files']){$taskArtifacts.Add($taskRow)}
 foreach($taskName in @('stdout.log','stderr.log','receipt.json','consumed.json')){$taskPath=Join-Path $taskStageRoot $taskName;$taskHash=Get-TaskHash $taskPath 67108864;$taskArtifacts.Add(@{path=$taskPath;bytes=$taskHash['bytes'];sha256=$taskHash['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 $taskActualExcludingBundle=[long]$taskAfter['bytes']+(Get-TaskStageBytes)
 if($taskActualExcludingBundle -ne $taskReceipt['retained_total_excluding_bundle']){throw 'Manual actual receipt/stage byte sum mismatch'}
 $taskTransitive=[Collections.Generic.List[object]]::new();foreach($taskInput in $taskOwnedInputs.Values){$taskTransitive.Add($taskInput)}
 $taskBundle=[ordered]@{schema=1;stage=$Stage;status='DEFINITION_SOURCE_REPLAY_COMPLETE_REQUIRES_FRESH_ROOT_OUTPUT_REVIEW';source_manifest_sha256=$taskExpectedManifest;reviewed_predecessor_bundle=$ReviewedInputBundle;reviewed_predecessor_sha256=$ReviewedInputBundleSha256;artifacts=$taskArtifacts.ToArray();transitive_inputs=$taskTransitive.ToArray();reference_root=$taskDataRoot;reserved_work=$taskReservedWork;generator_work=$taskChildWork;replay_work=$taskChildWork;runner_work=$taskRunnerWork;retained_total_excluding_bundle=$taskActualExcludingBundle;bundle_file_bytes=0L;retained_total=0L;retained_cap=$taskRetainedCap;context=$false;queried_layout=$false;quality=$false;vendor=$false;production_admission=$false;renderer=$false;compiler_called=$false}
 $taskStable=$false
 for($taskPass=0;$taskPass -lt 4;$taskPass++){$taskRaw=Encode-TaskJson $taskBundle;if($taskBundle['bundle_file_bytes'] -eq $taskRaw.Length -and $taskBundle['retained_total'] -eq $taskActualExcludingBundle+$taskRaw.Length){$taskStable=$true;break};$taskBundle['bundle_file_bytes']=$taskRaw.Length;$taskBundle['retained_total']=$taskActualExcludingBundle+$taskRaw.Length}
 if(-not $taskStable -or $taskBundle['retained_total'] -gt $taskRetainedCap){throw 'Whole retained bundle byte fixed point/cap'}
 Write-TaskJson (Join-Path $taskStageRoot 'bundle.json') $taskBundle
 $taskFinalActual=[long]$taskAfter['bytes']+(Get-TaskStageBytes)
 if($taskFinalActual -ne $taskBundle['retained_total']){throw 'Actual complete whole-pool final sum mismatch'}
 $taskBundleHash=Get-TaskHash (Join-Path $taskStageRoot 'bundle.json') 262144
 Write-Output ('DEFINITION_SOURCE_REPLAY_COMPLETE_REQUIRES_FRESH_ROOT_OUTPUT_REVIEW bundle_sha256='+$taskBundleHash['sha256']+' retained_bytes='+$taskFinalActual+' prepaid_work='+$taskReservedWork)
}catch{
 $taskReceipt['status']='FAILED_OR_NOT_REACHED_NO_CERTIFICATION';$taskReceipt['failure']=$_.Exception.Message.Substring(0,[Math]::Min(512,$_.Exception.Message.Length))
 if($taskCreated -and $null -ne $taskStageRoot){try{Save-TaskReceipt}catch{Write-Error 'Failure footer could not be written; retained prefixes and consumed marker remain; no success claim' -ErrorAction Continue}}
 throw
}
