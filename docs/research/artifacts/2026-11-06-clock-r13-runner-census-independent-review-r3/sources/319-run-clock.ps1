#Requires -Version 7.0
param(
 [Parameter(Mandatory)][ValidateSet('ClockOracle')][string]$Stage,
 [Parameter(Mandatory)][string]$Repo,
 [Parameter(Mandatory)][string]$NewRunRoot,
 [Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ReviewedManifestSha256
)
$ErrorActionPreference='Stop'
$taskSourceReviewAccepted=$false
$taskSourceWorkAccepted=$false
$taskExecutionAccepted=$false
$taskCompilerAccepted=$false
$taskProduceAccepted=$false
$taskVerifyAccepted=$false
$taskCompilerPeakDiskQualified=$false
$taskProductionContextQualityRendererReady=$false
if(-not $taskSourceReviewAccepted -or -not $taskSourceWorkAccepted -or -not $taskExecutionAccepted -or -not $taskCompilerAccepted -or -not $taskProduceAccepted -or -not $taskVerifyAccepted){throw 'SOURCE_HOLD_NO_CLOCK_ORACLE_GO'}
$taskBase=[IO.Path]::GetFullPath($PSScriptRoot);$taskRepo=[IO.Path]::GetFullPath($Repo);$taskRoot=[IO.Path]::GetFullPath($NewRunRoot)
$taskManifestPath=Join-Path $taskBase 'source-manifest.json';$taskExpectedManifest=$ReviewedManifestSha256.ToUpperInvariant()
$taskStageRoot=Join-Path $taskRoot 'stage';$taskDataRoot=Join-Path $taskRoot 'data';$taskBuildRoot=Join-Path $taskRoot 'build';$taskOpaqueRoot=Join-Path $taskRoot 'opaque-temp'
$taskOwnNames=@('clock_stage.cpp','run-clock.ps1','source-costs.h','README.md')
$taskManifest=$null;$taskUnits=0L;$taskRunnerLimit=0L;$taskStageCap=67108864L;$taskRetainedCap=880803840L;$taskScratch=33554432L
$taskJsonBytes=0L;$taskJsonVisitCap=14388953L;$taskRawLogBytes=0L;$taskQuarantine=$null;$taskReceiptWrites=0;$taskCreated=$false;$taskAdmittedCompiler=$false
$taskReceipt=[ordered]@{schema=1;stage=$Stage;status='NOT_STARTED_NO_CERTIFICATION';source_manifest_sha256=$taskExpectedManifest;compiler_is_opaque=$true;compiler_internal_CPU_heap_RSS_temporary_peak_disk_qualified=$false;old_broad_compiler_output_bound_fulfilled=$false;children=[Collections.Generic.List[object]]::new();compiler_exit=$null;producer_exit=$null;verifier_exit=$null;vendor=$false;quality=$false;context=$false;Ready=$false;renderer=$false}
function Spend-Task([long]$n){if($n -lt 0 -or $n -gt $script:taskRunnerLimit-$script:taskUnits){throw 'CONTROLLED_RUNNER_SOURCE_WORK_REFUSED'};$script:taskUnits+=$n}
function Assert-NoLink([string]$Path){
 $taskAt=[IO.Path]::GetFullPath($Path);$taskDepth=0
 while($taskAt){Spend-Task 1;$taskDepth++;if($taskDepth -gt 64){throw 'Path depth'};if([IO.File]::Exists($taskAt) -or [IO.Directory]::Exists($taskAt)){if(([IO.File]::GetAttributes($taskAt)-band [IO.FileAttributes]::ReparsePoint)-ne 0){throw 'Reparse path refused'}};$taskAt=[IO.Path]::GetDirectoryName($taskAt)}
}
function Get-TaskHash([string]$Path,[long]$Maximum){
 Spend-Task 64;Assert-NoLink $Path;$taskItem=[IO.FileInfo]::new($Path);$taskLength=$taskItem.Length;if($taskLength -gt $Maximum -or $taskLength -lt 0){throw 'Hash file cap'}
 $taskStream=$null;$taskHash=$null;$taskBuffer=[byte[]]::new(65536);$taskSeen=0L
 try{$taskStream=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,65536,[IO.FileOptions]::SequentialScan);if($taskStream.Length -ne $taskLength){throw 'Hash input length changed before owned read'};$taskHash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
  while($taskSeen -lt $taskLength){Spend-Task 32;$taskN=[int][Math]::Min(65536L,$taskLength-$taskSeen);$taskStream.ReadExactly($taskBuffer,0,$taskN);$taskSeen+=$taskN;$taskHash.AppendData($taskBuffer,0,$taskN)}
  if($taskStream.ReadByte() -ne -1){throw 'Hash input grew beyond exact EOF'};return @{bytes=$taskSeen;sha256=[Convert]::ToHexString($taskHash.GetHashAndReset())}
 }finally{if($null -ne $taskHash){$taskHash.Dispose()};if($null -ne $taskStream){$taskStream.Dispose()}}
}
function Invoke-TaskChild([string]$Name,[string]$Executable,[string[]]$Arguments){
 if($Name -cnotin @('one-opaque-compile','oracle-produce','independent-full-verify')){throw 'Exact finite child name profile'}
 Spend-Task 4096;Spend-Task 8192;if($null -ne $script:taskQuarantine){throw 'Unsettled prior child owns storage; no next child'}
 $taskProcess=$null;$taskStarted=$false;$taskCancel=$null;$taskOut=$null;$taskErr=$null;$taskReadA=$null;$taskReadB=$null;$taskAccountA=$true;$taskAccountB=$true
 $taskA=[byte[]]::new(4096);$taskB=[byte[]]::new(4096);$taskDoneA=$false;$taskDoneB=$false;$taskExit=$null;$taskExitObserved=$false;$taskFailure=$null;$taskTimedOut=$false;$taskCap=$false
 $taskObserved=0L;$taskUnretained=0L;$taskWritten=0L;$taskOmitted=0L;$taskSettled=$true;$taskCleanup=[Collections.Generic.List[string]]::new();$taskBeforeA=0L;$taskBeforeB=0L;$taskEndA=0L;$taskEndB=0L
 try{
  $taskCancel=[Threading.CancellationTokenSource]::new()
  $taskOut=[IO.FileStream]::new((Join-Path $taskStageRoot 'stdout.log'),[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
  $taskErr=[IO.FileStream]::new((Join-Path $taskStageRoot 'stderr.log'),[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
  $taskBeforeA=$taskOut.Position;$taskBeforeB=$taskErr.Position
  $taskInfo=[Diagnostics.ProcessStartInfo]::new();$taskInfo.FileName=$Executable;$taskInfo.WorkingDirectory=$taskRoot;$taskInfo.UseShellExecute=$false;$taskInfo.CreateNoWindow=$true;$taskInfo.RedirectStandardOutput=$true;$taskInfo.RedirectStandardError=$true
  $taskInfo.Environment.Clear();foreach($taskKey in $taskManifest['environment'].Keys){$taskValue=[string]$taskManifest['environment'][$taskKey];if($taskValue -ceq '<OWNED_OPAQUE_TEMP>'){$taskValue=$taskOpaqueRoot};$taskInfo.Environment[$taskKey]=$taskValue}
  foreach($taskArgument in $Arguments){$taskInfo.ArgumentList.Add($taskArgument)}
  $taskProcess=[Diagnostics.Process]::Start($taskInfo);$taskStarted=$true;$taskWatch=[Diagnostics.Stopwatch]::StartNew()
  $taskReadA=$taskProcess.StandardOutput.BaseStream.ReadAtLeastAsync([Memory[byte]]::new($taskA),4096,$false,$taskCancel.Token).AsTask();$taskAccountA=$false
  $taskReadB=$taskProcess.StandardError.BaseStream.ReadAtLeastAsync([Memory[byte]]::new($taskB),4096,$false,$taskCancel.Token).AsTask();$taskAccountB=$false
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
      if($taskDirection -eq 0){$taskReadA=$taskProcess.StandardOutput.BaseStream.ReadAtLeastAsync([Memory[byte]]::new($taskA),4096,$false,$taskCancel.Token).AsTask();$taskAccountA=$false}else{$taskReadB=$taskProcess.StandardError.BaseStream.ReadAtLeastAsync([Memory[byte]]::new($taskB),4096,$false,$taskCancel.Token).AsTask();$taskAccountB=$false}
     }
    }
   }
   if($taskWatch.Elapsed.TotalSeconds -gt 1800){$taskTimedOut=$true;throw 'Owned child1800s timeout'}
   [Threading.Thread]::Sleep(250) # rate-bound even after the process exits while a pipe remains pending
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
  if($null -ne $taskOut){$taskEndA=$taskOut.Position};if($null -ne $taskErr){$taskEndB=$taskErr.Position}
  if($taskSettled -and (-not $taskStarted -or $taskExitObserved)){
   [Array]::Clear($taskA,0,$taskA.Length);[Array]::Clear($taskB,0,$taskB.Length)
   foreach($taskOwner in @($taskOut,$taskErr,$taskCancel,$taskProcess)){if($null -ne $taskOwner){try{$taskOwner.Dispose()}catch{$taskCleanup.Add('settled_owner_dispose')}}}
  }else{
   $script:taskQuarantine=@{process=$taskProcess;cancel=$taskCancel;readA=$taskReadA;readB=$taskReadB;bufferA=$taskA;bufferB=$taskB;stdout=$taskOut;stderr=$taskErr};$taskCleanup.Add('one_rooted_unsettled_owner_no_more_children')
  }
  if($taskCleanup.Count -gt 0 -and $null -eq $taskFailure){$taskFailure='Bounded cleanup refusal'}
  $taskRow=[ordered]@{name=$Name;started=$taskStarted;raw_exit_observed=$taskExitObserved;raw_exit=$taskExit;timed_out=$taskTimedOut;log_cap=$taskCap;stdout_start=$taskBeforeA;stderr_start=$taskBeforeB;raw_stdout_bytes=($taskEndA-$taskBeforeA);raw_stderr_bytes=($taskEndB-$taskBeforeB);raw_bytes_retained=$taskWritten;raw_bytes_observed=$taskObserved;observed_not_confirmed_retained=$taskUnretained;omitted_at_cap=$taskOmitted;unknown_undrained=($taskStarted -and -not($taskDoneA -and $taskDoneB));both_eof=($taskDoneA -and $taskDoneB);reads_settled=$taskSettled;owned_storage_quarantined=($null -ne $script:taskQuarantine);cleanup=$taskCleanup.ToArray();failure=$taskFailure}
  $script:taskReceipt['children'].Add($taskRow)
  if($taskExitObserved){switch -Exact ($Name){'one-opaque-compile'{$script:taskReceipt['compiler_exit']=$taskExit};'oracle-produce'{$script:taskReceipt['producer_exit']=$taskExit};'independent-full-verify'{$script:taskReceipt['verifier_exit']=$taskExit};default{throw 'Unlisted child name in final owned record'}}}
 }
 if($null -ne $taskFailure){throw $taskFailure};if(-not $taskExitObserved -or $taskExit -ne 0){throw ('Actual child raw exit: '+$taskExit)}
 return $taskRow
}

function Reject-TaskGate([string]$Reason,$Witness){$taskE=[InvalidOperationException]::new($Reason);$taskE.Data['gate']=$Reason;$taskE.Data['witness']=$Witness;throw $taskE}
function Expect-TaskGate([string]$Id,[scriptblock]$Action,[string]$Reason,$Expected){
 $taskCaught=$null;try{& $Action|Out-Null}catch{$taskCaught=$_.Exception}
 if($null -eq $taskCaught -or $taskCaught.Message -cne $Reason -or $taskCaught.Data['gate'] -cne $Reason){throw ('CONTROL_GATE_NOT_REACHED_OR_WRONG:'+ $Id)}
 $taskActual=$taskCaught.Data['witness'];if($taskActual.Count -ne $Expected.Count){throw ('CONTROL_WRONG_WITNESS:'+ $Id)}
 foreach($taskKey in $Expected.Keys){if(-not $taskActual.Contains($taskKey) -or $taskActual[$taskKey] -cne $Expected[$taskKey]){throw ('CONTROL_WRONG_WITNESS:'+ $Id)}}
 return @{id=$Id;gate=$taskCaught.Data['gate'];witness=$taskActual}
}
function Add-TaskJsonVisit([long]$Bytes){if($Bytes -lt 0 -or $Bytes -gt $script:taskJsonVisitCap-$script:taskJsonBytes){throw 'Whole finite JSON source-visit cap'};$script:taskJsonBytes+=$Bytes}
function Get-TaskEntries([string]$Root,[bool]$Recursive,[int]$Maximum,[Collections.Generic.List[object]]$Destination=$null){
 $taskRows=$Destination;if($null -eq $taskRows){$taskRows=[Collections.Generic.List[object]]::new()};$taskPending=[Collections.Generic.Queue[object]]::new();$taskPending.Enqueue(@{path=[IO.Path]::GetFullPath($Root);depth=0});$taskSeen=0
 while($taskPending.Count){$taskNext=$taskPending.Dequeue();$taskDirectory=$taskNext['path'];$taskDepth=[int]$taskNext['depth'];if($taskDepth -gt 64){Reject-TaskGate 'DIRECTORY_DEPTH_PROFILE' @{path=$taskDirectory;depth=$taskDepth;maximum=64}};Assert-NoLink $taskDirectory;$taskIterator=$null
  try{$taskIterator=[IO.Directory]::EnumerateFileSystemEntries($taskDirectory).GetEnumerator();while($taskIterator.MoveNext()){$taskPath=[IO.Path]::GetFullPath([string]$taskIterator.Current);$taskSeen++;if($taskSeen -gt $Maximum){Reject-TaskGate 'DIRECTORY_ENTRY_PROFILE' @{root=$Root;maximum=$Maximum;observed=$taskSeen;overflow_path=$taskPath}};Spend-Task 1;if($taskPath.Length -gt 512){throw 'Finite owned/input path length'};$taskAttributes=[IO.File]::GetAttributes($taskPath);$taskDirectoryFlag=($taskAttributes -band [IO.FileAttributes]::Directory)-ne 0;$taskLink=($taskAttributes -band [IO.FileAttributes]::ReparsePoint)-ne 0;$taskLength=if($taskDirectoryFlag -or $taskLink){0L}else{[IO.FileInfo]::new($taskPath).Length};$taskRows.Add([pscustomobject]@{FullName=$taskPath;Name=[IO.Path]::GetFileName($taskPath);PSIsContainer=$taskDirectoryFlag;Length=$taskLength;Reparse=$taskLink});if($taskLink){throw 'Reparse entry refused before traversal'};if($Recursive -and $taskDirectoryFlag){if($taskDepth -ge 64){Reject-TaskGate 'DIRECTORY_DEPTH_PROFILE' @{path=$taskPath;depth=($taskDepth+1);maximum=64}};$taskPending.Enqueue(@{path=$taskPath;depth=($taskDepth+1)})}}}
  finally{if($taskIterator -is [IDisposable]){$taskIterator.Dispose()}}
 }
 return $taskRows.ToArray()
}
function Copy-TaskOwned([string]$Source,[string]$Destination,[long]$ExpectedBytes,[long]$Cap){
 if($ExpectedBytes -gt $Cap -or -not $Destination.StartsWith((Join-Path $taskRoot 'controls')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Declared bounded owned control copy'}
 Assert-NoLink $Source;Assert-NoLink $Destination;$taskIn=$null;$taskOut=$null;$taskBuffer=[byte[]]::new(65536);$taskSeen=0L
 try{$taskIn=[IO.FileStream]::new($Source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$taskOut=[IO.FileStream]::new($Destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);if($taskIn.Length -ne $ExpectedBytes){throw 'Owned source copy length'};while($taskSeen -lt $ExpectedBytes){Spend-Task 128;$taskN=[int][Math]::Min(65536L,$ExpectedBytes-$taskSeen);$taskIn.ReadExactly($taskBuffer,0,$taskN);if($taskN -gt $Cap-$taskSeen){throw 'Owned control before-write cap'};$taskOut.Write($taskBuffer,0,$taskN);$taskSeen+=$taskN};if($taskIn.ReadByte() -ne -1){throw 'Owned source copy EOF'};$taskOut.Flush($true)}finally{if($null -ne $taskOut){$taskOut.Dispose()};if($null -ne $taskIn){$taskIn.Dispose()}}
}
function Mutate-TaskOwnedBytes([string]$Path,[long]$Offset,[byte[]]$Replacement){
 if(-not $Path.StartsWith((Join-Path $taskRoot 'controls')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $Replacement.Length -gt 2){throw 'Bounded owned mutation path/bytes'};$taskFile=$null;$taskBefore=[byte[]]::new($Replacement.Length)
 try{$taskFile=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);if($Offset -lt 0 -or $Offset+$Replacement.Length -gt $taskFile.Length){throw 'Owned mutation coordinate'};$taskFile.Position=$Offset;$taskFile.ReadExactly($taskBefore,0,$taskBefore.Length);$taskFile.Position=$Offset;$taskFile.Write($Replacement,0,$Replacement.Length);$taskFile.Flush($true)}finally{if($null -ne $taskFile){$taskFile.Dispose()}}
 return @{path=$Path;offset=$Offset;before=[Convert]::ToHexString($taskBefore);after=[Convert]::ToHexString($Replacement)}
}
function Assert-TaskDependencyJsonShape([string]$Text){
 $taskNodes=0;$taskDepth=0;$taskQuoted=$false;$taskEscape=$false;$taskUnicode=0;$taskStringUnits=0;$taskScalar=$false;$taskNodeMax=2*[int]$taskManifest['profile']['include_input_count']+256
 foreach($taskChar in $Text.ToCharArray()){
  if($taskQuoted){if($taskUnicode -gt 0){if(-not (($taskChar -ge '0' -and $taskChar -le '9') -or ($taskChar -ge 'a' -and $taskChar -le 'f') -or ($taskChar -ge 'A' -and $taskChar -le 'F'))){throw 'Dependency JSON Unicode escape'};$taskUnicode--;continue};if($taskEscape){$taskEscape=$false;$taskStringUnits++;if($taskChar -ceq 'u'){$taskUnicode=4}}elseif($taskChar -ceq '\'){$taskEscape=$true}elseif($taskChar -ceq '"'){$taskQuoted=$false}else{$taskStringUnits++};if($taskStringUnits -gt 512){throw 'Dependency JSON bounded decoded string'};continue}
  if($taskChar -ceq '"'){$taskQuoted=$true;$taskStringUnits=0;$taskNodes++;$taskScalar=$false}
  elseif($taskChar -ceq '{' -or $taskChar -ceq '['){$taskDepth++;$taskNodes++;$taskScalar=$false;if($taskDepth -gt 24){throw 'Dependency JSON depth'}}
  elseif($taskChar -ceq '}' -or $taskChar -ceq ']'){$taskDepth--;$taskScalar=$false;if($taskDepth -lt 0){throw 'Dependency JSON unmatched depth'}}
  elseif($taskChar -ceq ',' -or $taskChar -ceq ':' -or [char]::IsWhiteSpace($taskChar)){$taskScalar=$false}
  elseif(-not $taskScalar){$taskNodes++;$taskScalar=$true}
  if($taskNodes -gt $taskNodeMax){throw 'Dependency JSON preparse node profile'}
 }
 if($taskQuoted -or $taskEscape -or $taskUnicode -or $taskDepth){throw 'Dependency JSON incomplete bounded tokens'}
}
function Read-TaskBoundedBytes([string]$Path,[long]$Maximum){
 # Stat and opened length are admitted BEFORE any length-sized allocation.
 $taskLength=[IO.FileInfo]::new($Path).Length;if($taskLength -lt 0 -or $taskLength -gt $Maximum -or $Maximum -gt 4194304){throw 'JSON byte cap before allocation'};$taskFile=$null
 try{$taskFile=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,65536,[IO.FileOptions]::SequentialScan);if($taskFile.Length -ne $taskLength){throw 'JSON length changed before bounded allocation'};$taskBytes=[byte[]]::new([int]$taskLength);$taskFile.ReadExactly($taskBytes,0,$taskBytes.Length);if($taskFile.ReadByte() -ne -1){throw 'JSON exact EOF'};return ,$taskBytes}finally{if($null -ne $taskFile){$taskFile.Dispose()}}
}
function Read-TaskJson([string]$Path,[long]$Maximum=1048576){Spend-Task 64;$taskBytes=Read-TaskBoundedBytes $Path $Maximum;Add-TaskJsonVisit $taskBytes.Length;$taskRaw=[Text.UTF8Encoding]::new($false,$true).GetString($taskBytes);$taskBytes=$null;if($Maximum -eq 4194304){Assert-TaskDependencyJsonShape $taskRaw};return ConvertFrom-Json -InputObject $taskRaw -AsHashtable -Depth 24}
function Encode-TaskJson($Value){$taskBytes=[Text.UTF8Encoding]::new($false,$true).GetBytes((($Value|ConvertTo-Json -Depth 24 -Compress).Replace("`r`n","`n"))+"`n");if($taskBytes.Length -gt 1048576){throw 'Encoded JSON1MiB cap'};Add-TaskJsonVisit $taskBytes.Length;return ,$taskBytes}
function Get-TaskStageBytes{
 $taskSum=0L;if([IO.Directory]::Exists($taskStageRoot)){foreach($taskF in (Get-TaskEntries $taskStageRoot $false 7)){Spend-Task 1;if($taskF.PSIsContainer -or $taskF.Name -cnotin @('stdout.log','stderr.log','receipt.json','bundle.json','consumed.json','failure-inventory.json')){throw 'Unlisted controlled stage artifact'};if($taskF.Length -gt $taskStageCap-$taskSum){throw 'Controlled stage aggregate'};$taskSum+=$taskF.Length}};return $taskSum
}
function Write-TaskJson([string]$Path,$Value){$taskRaw=Encode-TaskJson $Value;$taskOld=if([IO.File]::Exists($Path)){(Get-Item -LiteralPath $Path).Length}else{0L};if($taskRaw.Length -gt $taskStageCap-((Get-TaskStageBytes)-$taskOld)){throw 'Stage before-write cap'};[IO.File]::WriteAllBytes($Path,$taskRaw)}
function Save-TaskReceipt{if($script:taskReceiptWrites -ge 12){throw 'Twelve bounded receipt writes'};$script:taskReceiptWrites++;$taskProbe=[Text.UTF8Encoding]::new($false,$true).GetBytes(($taskReceipt|ConvertTo-Json -Depth 24 -Compress));if($taskProbe.Length -gt 65535){throw 'Receipt64KiB source profile'};Add-TaskJsonVisit $taskProbe.Length;Write-TaskJson (Join-Path $taskStageRoot 'receipt.json') $taskReceipt}
function Assert-TaskIdentity($Expected,$Observed){if($Expected['bytes'] -ne $Observed['bytes'] -or $Expected['sha256'] -cne $Observed['sha256']){Reject-TaskGate 'BEFORE_AFTER_COMPLETE_INPUT_IDENTITY' @{path=$Expected['path'];expected_bytes=$Expected['bytes'];observed_bytes=$Observed['bytes'];expected_hash=$Expected['sha256'];observed_hash=$Observed['sha256']}}}
function Assert-TaskReservation([long]$Total,[long]$Limit){if($Total -gt $Limit -or $Total -lt 0 -or $Limit -gt 17179869184L){Reject-TaskGate 'WHOLE_STAGE_WORK_RESERVATION' @{total=$Total;limit=$Limit;global=17179869184L}}}
function Assert-TaskRetainedReservation([long]$Total,[long]$Limit){if($Total -gt $Limit -or $Total -lt 0 -or $Limit -gt 880803840L){Reject-TaskGate 'WHOLE_STAGE_RETAINED_RESERVATION' @{total=$Total;limit=$Limit;global=880803840L}}}
function Assert-TaskAttempt([bool]$Exists,[string]$Path){if($Exists){Reject-TaskGate 'ONE_ATTEMPT_ALREADY_CONSUMED' @{path=$Path;exists=$Exists}}}
function Assert-TaskPredecessor($Bundle){if($Bundle['artifacts'].Count -ne 96 -or $Bundle['transitive_inputs'].Count -ne 147 -or $Bundle['source_manifest_sha256'] -cne '6822A96025D6EEADD79642AB50B0A30224A35429B0577D3246BBC173AC630E88'){Reject-TaskGate 'EXACT_R4_PREDECESSOR' @{artifacts=$Bundle['artifacts'].Count;transitive=$Bundle['transitive_inputs'].Count;source_manifest=$Bundle['source_manifest_sha256']}}}
function Resolve-TaskInput([string]$Name){if([IO.Path]::IsPathRooted($Name)){return [IO.Path]::GetFullPath($Name)};return [IO.Path]::GetFullPath((Join-Path $taskRepo $Name))}
function Assert-TaskPins{
 foreach($taskDir in $taskManifest['closed_directory_inputs']){
  $taskRootDir=Resolve-TaskInput $taskDir['path'];Assert-NoLink $taskRootDir
  $taskActualFiles=@((Get-TaskEntries $taskRootDir $taskDir['recursive'] ([int]$taskDir['entry_count']+1))|Where-Object {-not $_.PSIsContainer})
  $taskObservedNames=@($taskActualFiles|ForEach-Object {[IO.Path]::GetRelativePath($taskRootDir,$_.FullName).Replace([char]92,[char]47)})
  Assert-TaskFileSet $taskObservedNames @($taskDir['files'])
 }
 $taskMh=Get-TaskHash $taskManifestPath 1048576;if($taskMh['sha256'] -cne $taskExpectedManifest){throw 'Current source manifest changed'}
 $taskSeen=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);if($taskManifest['sources'].Count -ne $taskOwnNames.Count){throw 'Current exact own4 source inventory'}
 foreach($taskPin in $taskManifest['sources']){if($taskPin['path'] -cnotin $taskOwnNames -or -not $taskSeen.Add($taskPin['path'])){throw 'Exact own4 names'};$taskExpected=@{path=(Join-Path $taskBase $taskPin['path']);bytes=$taskPin['bytes'];sha256=$taskPin['sha256']};Assert-TaskIdentity $taskExpected (Get-TaskHash $taskExpected['path'] $taskExpected['bytes'])}
 $taskSum=0L;foreach($taskPin in $taskManifest['inputs']){$taskPath=Resolve-TaskInput $taskPin['path'];$taskExpected=@{path=$taskPath;bytes=$taskPin['bytes'];sha256=$taskPin['sha256']};Assert-TaskIdentity $taskExpected (Get-TaskHash $taskPath $taskExpected['bytes']);$taskSum+=[long]$taskPin['bytes']}
 if($taskSum -ne $taskManifest['profile']['input_bytes'] -or $taskManifest['inputs'].Count -ne $taskManifest['profile']['input_count']){throw 'Derived exact input profile'}
}
function Assert-TaskFileSet([string[]]$Observed,[string[]]$Expected){
 # Build the ordinal expected set ONCE. No per-observed linear -cnotin scan.
 $taskExpectedSet=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($taskName in $Expected){if(-not $taskExpectedSet.Add($taskName)){throw 'Duplicate declared expected file name'}}
 $taskSet=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);$taskDuplicate='';$taskUnknown=[Collections.Generic.List[string]]::new();$taskMissing=[Collections.Generic.List[string]]::new()
 foreach($taskName in $Observed){if(-not $taskSet.Add($taskName)){$taskDuplicate=$taskName};if(-not $taskExpectedSet.Contains($taskName)){$taskUnknown.Add($taskName)}}
 foreach($taskName in $Expected){if(-not $taskSet.Contains($taskName)){$taskMissing.Add($taskName)}}
 if($Observed.Count -ne $Expected.Count -or $taskMissing.Count -or $taskUnknown.Count -or $taskDuplicate){Reject-TaskGate 'COMPLETE_EXACT_REFERENCE_FILE_SET' @{observed=$Observed.Count;expected=$Expected.Count;missing=($taskMissing -join '|');unknown=($taskUnknown -join '|');duplicate=$taskDuplicate}}
}
function Get-TaskReferences{
 $taskRows=[Collections.Generic.List[object]]::new();$taskNames=[Collections.Generic.List[string]]::new();$taskBytes=0L
 foreach($taskF in (Get-TaskEntries $taskDataRoot $true 60)){Spend-Task 2;Assert-NoLink $taskF.FullName;if($taskF.PSIsContainer){continue};$taskRelative=[IO.Path]::GetRelativePath($taskDataRoot,$taskF.FullName).Replace([char]92,[char]47);$taskNames.Add($taskRelative);if(-not $taskCaps.ContainsKey($taskRelative)){throw 'Unknown reference output before admission'};$taskH=Get-TaskHash $taskF.FullName $taskCaps[$taskRelative];if($taskH['bytes'] -gt 152627224L-$taskBytes){throw 'Complete reference pool cap'};$taskBytes+=$taskH['bytes'];$taskRows.Add(@{path=$taskF.FullName;relative_path=$taskRelative;bytes=$taskH['bytes'];sha256=$taskH['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 Assert-TaskFileSet $taskNames.ToArray() @($taskCaps.Keys);return @{files=$taskRows.ToArray();bytes=$taskBytes;names=$taskNames.ToArray()}
}
function Assert-TaskSameReferences($Before,$After){if($Before['bytes'] -ne $After['bytes']){throw 'Generated reference byte sum changed'};$taskOld=@{};foreach($taskRow in $Before['files']){$taskOld[$taskRow['relative_path']]=$taskRow};foreach($taskRow in $After['files']){Assert-TaskIdentity $taskOld[$taskRow['relative_path']] $taskRow}}
function Get-TaskPEHeader([string]$Path){$taskItem=Get-Item -LiteralPath $Path;if($taskItem.Length -gt 33554432 -or $taskItem.Length -lt 88){throw 'PE file size'};$taskFile=$null
 try{$taskFile=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$taskDos=[byte[]]::new(64);$taskFile.ReadExactly($taskDos,0,$taskDos.Length);$taskOffset=[BitConverter]::ToUInt32($taskDos,60);if($taskDos[0] -ne 77 -or $taskDos[1] -ne 90 -or $taskOffset -gt $taskItem.Length-24){throw 'Actual PE DOS/offset'};$taskFile.Position=$taskOffset;$taskHeader=[byte[]]::new(24);$taskFile.ReadExactly($taskHeader,0,$taskHeader.Length);$taskOptional=[BitConverter]::ToUInt16($taskHeader,20);if($taskOptional -lt 70 -or $taskOptional -gt 4096 -or $taskOffset+24+$taskOptional -gt $taskItem.Length){throw 'Actual PE optional size'};$taskOpt=[byte[]]::new($taskOptional);$taskFile.ReadExactly($taskOpt,0,$taskOpt.Length);return @{signature=[BitConverter]::ToUInt32($taskHeader,0);machine=[BitConverter]::ToUInt16($taskHeader,4);magic=[BitConverter]::ToUInt16($taskOpt,0);subsystem=[BitConverter]::ToUInt16($taskOpt,68);path=$Path;offset=$taskOffset;optional_bytes=$taskOptional}}
 finally{if($null -ne $taskFile){$taskFile.Dispose()}}
}
function Assert-TaskConsolePE($PE){if($PE['signature'] -ne 17744 -or $PE['machine'] -ne 34404 -or $PE['magic'] -ne 523 -or $PE['subsystem'] -ne 3){Reject-TaskGate 'ACTUAL_PE_CONSOLE_BEFORE_LAUNCH' @{signature=$PE['signature'];machine=$PE['machine'];magic=$PE['magic'];subsystem=$PE['subsystem']}}}
function Read-TaskChildResult($Child){$taskOut=Join-Path $taskStageRoot 'stdout.log';$taskFile=$null;try{$taskFile=[IO.File]::OpenRead($taskOut);$taskFile.Position=$Child['stdout_start'];$taskLength=$Child['raw_stdout_bytes'];if($taskLength -gt 65536){throw 'Module JSON/control output cap'};Add-TaskJsonVisit $taskLength;$taskBuffer=[byte[]]::new([int]$taskLength);$taskFile.ReadExactly($taskBuffer,0,$taskBuffer.Length);$taskText=[Text.UTF8Encoding]::new($false,$true).GetString($taskBuffer);$taskLines=@($taskText.Split("`n")|Where-Object {$_});if($taskLines.Count -lt 1 -or $taskLines.Count -gt 33){throw 'Missing/bounded child result'};$taskRecords=[Collections.Generic.List[object]]::new();for($taskLine=0;$taskLine -lt $taskLines.Count-1;$taskLine++){$taskRecord=ConvertFrom-Json -InputObject $taskLines[$taskLine] -AsHashtable -Depth 16;if($taskRecord.Count -ne 3 -or -not $taskRecord.Contains('control') -or -not $taskRecord.Contains('gate') -or -not $taskRecord.Contains('operands') -or $taskRecord['operands'].Count -ne 6){throw 'Actual module control witness schema'};$taskRecords.Add($taskRecord)};$taskRecordMode=if($Child['name'] -ceq 'oracle-produce'){'produce'}else{'verify'};$taskExpectedRecords=$taskManifest['module_controls'][$taskRecordMode];Assert-TaskFileSet @($taskRecords.ToArray()|ForEach-Object {$_['control']}) @($taskExpectedRecords.Keys);foreach($taskRecord in $taskRecords){if($taskRecord['gate'] -cne $taskExpectedRecords[$taskRecord['control']]){throw 'Actual module exact rejection reason'}};$taskResult=ConvertFrom-Json -InputObject $taskLines[-1] -AsHashtable -Depth 16;$taskResult['actual_control_witnesses']=$taskRecords.ToArray();return $taskResult}finally{if($null -ne $taskFile){$taskFile.Dispose()}}}
function Get-TaskBuildArtifacts{
 $taskRows=[Collections.Generic.List[object]]::new();$taskSeen=[Collections.Generic.List[string]]::new();$taskSum=0L
 foreach($taskF in (Get-TaskEntries $taskBuildRoot $false 4)){if($taskF.PSIsContainer -or -not $taskBuildCaps.ContainsKey($taskF.Name)){throw 'Compiler final artifact set is not admitted'};$taskH=Get-TaskHash $taskF.FullName $taskBuildCaps[$taskF.Name];if($taskH['bytes'] -gt 67108864L-$taskSum){throw 'Compiler final aggregate not admitted'};$taskSum+=$taskH['bytes'];$taskSeen.Add($taskF.Name);$taskRows.Add(@{path=$taskF.FullName;bytes=$taskH['bytes'];sha256=$taskH['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 Assert-TaskFileSet $taskSeen.ToArray() @($taskBuildCaps.Keys);return @{files=$taskRows.ToArray();bytes=$taskSum}
}
function Assert-TaskDependencies{
 $taskD=Read-TaskJson (Join-Path $taskBuildRoot 'source-dependencies.json') 4194304;$taskData=$taskD['Data'];if($null -eq $taskData -or (Resolve-TaskInput $taskData['Source']) -ine (Join-Path $taskBase 'clock_stage.cpp')){throw 'Compiler source-dependency source binding'}
 foreach($taskKey in @('ImportedModules','ImportedHeaderUnits')){if($taskData[$taskKey].Count -gt 0){throw 'Unqualified compiler module/header-unit dependency'}}
 if(-not $taskData.Contains('Includes') -or @($taskData['Includes']).Count -lt 1 -or @($taskData['Includes']).Count -gt $taskManifest['profile']['include_input_count']){throw 'Compiler dependency Includes finite profile'}
 foreach($taskPath in $taskData['Includes']){if(-not $taskIncludeIndex.ContainsKey([IO.Path]::GetFullPath($taskPath))){throw 'Source-dependency include outside pre-fenced conservative inventory'}}
 return @{includes=@($taskData['Includes']).Count;report_is_additional_artifact_only=$true;complete_tool_inventory_replaced=$false}
}
function Save-TaskFailureInventory{
 $taskEntries=[Collections.Generic.List[object]]::new();$taskRows=[Collections.Generic.List[object]]::new();$taskComplete=$true;$taskReason=$null
 try{if([IO.Directory]::Exists($taskRoot)){$null=Get-TaskEntries $taskRoot $true ([int]$taskManifest['profile']['root_walk_entries_max']) $taskEntries}}catch{$taskComplete=$false;$taskReason=$_.Exception.Message}
 foreach($taskF in $taskEntries){if($taskF.PSIsContainer){continue};$taskRows.Add(@{path=$taskF.FullName;bytes=$taskF.Length;reparse=$taskF.Reparse;admission='NOT_AUTOMATICALLY_ADMITTED';opaque_temporary=($taskF.FullName.StartsWith($taskOpaqueRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))})}
 $taskReceipt['failure_inventory_complete']=$taskComplete;$taskReceipt['failure_inventory_reason']=$taskReason
 Write-TaskJson (Join-Path $taskStageRoot 'failure-inventory.json') @{files=$taskRows.ToArray();complete=$taskComplete;reason=$taskReason;compiler_temporary_peak_disk_qualified=$false;admitted_pool_success=$false;quarantine_retains_ownership=($null -ne $taskQuarantine)}
}

try{
 # Hash literal manifest before trusting source-sized arithmetic. Loading this
 # exact source-only revision is not authority: all three entry guards are false.
 $taskBootstrap=Read-TaskBoundedBytes $taskManifestPath 1048576;if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($taskBootstrap)) -cne $taskExpectedManifest){throw 'External manifest hash'}
 Add-TaskJsonVisit $taskBootstrap.Length;$taskManifest=ConvertFrom-Json -InputObject ([Text.UTF8Encoding]::new($false,$true).GetString($taskBootstrap)) -AsHashtable -Depth 24;$taskBootstrap=$null
 $taskActualHost=[IO.Path]::GetFullPath([Diagnostics.Process]::GetCurrentProcess().MainModule.FileName);if(-not $taskActualHost.Equals((Resolve-TaskInput $taskManifest['controller']['path']),[StringComparison]::OrdinalIgnoreCase)){throw 'Exact pinned PowerShell7 controller executable required'}
 foreach($taskModule in $taskManifest['controller']['modules']){Import-Module -Name (Resolve-TaskInput $taskModule) -ErrorAction Stop}
 # The prototype has no bound catalogue. Even later guard promotion alone
 # cannot turn HOLD_UNBOUND fields into authority or numeric prices.
 if($taskManifest['cost_catalogue']['binding_state'] -cne 'BOUND_REVIEWED' -or $taskManifest['cost_catalogue']['definition_files'].Count -eq 0){throw 'SOURCE_COST_CATALOGUE_UNBOUND_NO_PROTOCOL_GO'}
 foreach($taskKey in @('runner_upper','producer_upper','verifier_upper','whole_upper')){if($taskManifest['source_work'][$taskKey] -is [string] -or $taskManifest['source_work'][$taskKey] -isnot [ValueType]){throw 'Source work must be actually bound reviewed integer fields'}}
 $taskRunnerLimit=[long]$taskManifest['source_work']['runner_upper'];$taskTotalWork=[long]$taskManifest['source_work']['whole_upper'];Assert-TaskReservation $taskTotalWork 17179869184L
 $taskInputIndex=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase);foreach($taskPin in $taskManifest['inputs']){$taskPath=Resolve-TaskInput $taskPin['path'];if($taskPath.Length -gt 512 -or $taskInputIndex.ContainsKey($taskPath)){throw 'Exact unique input paths'};$taskInputIndex.Add($taskPath,$taskPin)}
 if($taskInputIndex.Count -ne $taskManifest['profile']['input_count']){throw 'Actual closed profile count'}
 $taskIncludeIndex=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase);foreach($taskPair in $taskInputIndex.GetEnumerator()){$taskIncludeIndex.Add($taskPair.Key,$taskPair.Value)}
 foreach($taskName in $taskManifest['current_include_inputs']){if($taskName -cne 'source-costs.h'){throw 'Unlisted current source header'};$taskPin=@($taskManifest['sources']|Where-Object {$_['path'] -ceq $taskName});if($taskPin.Count -ne 1){throw 'Current source header exact own pin'};$taskIncludeIndex.Add((Join-Path $taskBase $taskName),$taskPin[0])}
 if($taskIncludeIndex.Count -ne $taskManifest['profile']['include_input_count']){throw 'Exact historical plus current header include profile'}
 $taskAuthoredBytes=[long](Get-Item -LiteralPath $taskManifestPath).Length;foreach($taskPin in $taskManifest['sources']){$taskAuthoredBytes+=[long]$taskPin['bytes']};if($taskAuthoredBytes -gt 1048576L){throw 'Own five-file source payload1MiB'}
 $taskControlCaps=0L;foreach($taskC in $taskManifest['control_files']){$taskControlCaps+=[long]$taskC['bytes_cap']};$taskReservedRetained=[long]$taskManifest['profile']['input_bytes']+$taskAuthoredBytes+152627224L+67108864L+67108864L+$taskControlCaps;Assert-TaskRetainedReservation $taskReservedRetained 880803840L
 if(-not $taskRoot.StartsWith((Join-Path $taskBase 'runs')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $taskRoot.Substring((Join-Path $taskBase 'runs').Length+1).Contains([IO.Path]::DirectorySeparatorChar)){throw 'New owned direct run root'}
 foreach($taskChar in $taskRoot.ToCharArray()){if([int]$taskChar -lt 32 -or [int]$taskChar -gt 126){throw 'This pinned console tool profile requires ASCII run path'}}
 Assert-NoLink $taskRoot;Assert-TaskAttempt (Test-Path -LiteralPath $taskRoot) $taskRoot
 $taskRuns=Join-Path $taskBase 'runs';if((Test-Path -LiteralPath $taskRuns) -and @(Get-TaskEntries $taskRuns $false 1).Count -gt 0){throw 'Revision sole attempt already exists'}
 Assert-TaskPins;$taskPredecessor=Read-TaskJson (Resolve-TaskInput $taskManifest['predecessor_bundle']['path']);Assert-TaskPredecessor $taskPredecessor
 $taskCaps=[Collections.Generic.Dictionary[string,long]]::new([StringComparer]::Ordinal);foreach($taskA in $taskManifest['references']){$taskCaps.Add($taskA['path'],[long]$taskA['bytes_cap'])}
 $taskBuildCaps=[Collections.Generic.Dictionary[string,long]]::new([StringComparer]::Ordinal);foreach($taskA in $taskManifest['compiler_final_outputs']){$taskBuildCaps.Add($taskA['name'],[long]$taskA['bytes_cap'])}
 $null=New-Item -ItemType Directory -Path $taskRoot;$taskCreated=$true
 foreach($taskDir in @($taskStageRoot,$taskBuildRoot,$taskOpaqueRoot,(Join-Path $taskDataRoot 'references/clock'),(Join-Path $taskDataRoot 'references/native-inverse'),(Join-Path $taskRoot 'controls'))){$null=New-Item -ItemType Directory -Path $taskDir}
 $taskMarkerPath=Join-Path $taskStageRoot 'consumed.json';$taskMarker=Encode-TaskJson @{stage=$Stage;manifest=$taskExpectedManifest;work=$taskTotalWork;retained_reservation=$taskReservedRetained;attempts=1;opaque_compiler_peak_disk_qualified=$false};$taskMarkerFile=$null;try{$taskMarkerFile=[IO.File]::Open($taskMarkerPath,[IO.FileMode]::CreateNew);$taskMarkerFile.Write($taskMarker,0,$taskMarker.Length)}finally{if($null -ne $taskMarkerFile){$taskMarkerFile.Dispose()}}
 $taskControls=[Collections.Generic.List[object]]::new();Assert-TaskReservation $taskTotalWork 17179869184L
 $taskControls.Add((Expect-TaskGate 'work-one-below' {Assert-TaskReservation $taskTotalWork ($taskTotalWork-1)} 'WHOLE_STAGE_WORK_RESERVATION' @{total=$taskTotalWork;limit=($taskTotalWork-1);global=17179869184L}))
 $taskControls.Add((Expect-TaskGate 'bytes-one-below' {Assert-TaskRetainedReservation $taskReservedRetained ($taskReservedRetained-1)} 'WHOLE_STAGE_RETAINED_RESERVATION' @{total=$taskReservedRetained;limit=($taskReservedRetained-1);global=880803840L}))
 $taskControls.Add((Expect-TaskGate 'one-use-already-consumed' {Assert-TaskAttempt (Test-Path -LiteralPath $taskMarkerPath) $taskMarkerPath} 'ONE_ATTEMPT_ALREADY_CONSUMED' @{path=$taskMarkerPath;exists=$true}))
 $taskSourceBundle=Resolve-TaskInput $taskManifest['predecessor_bundle']['path'];$taskBundleHash=Get-TaskHash $taskSourceBundle 98009;$taskMutantBundle=Join-Path $taskRoot 'controls/predecessor-mutant.json';Copy-TaskOwned $taskSourceBundle $taskMutantBundle 98009 98009
 $taskIdentity=@{path=$taskMutantBundle;bytes=98009L;sha256=$taskManifest['predecessor_bundle']['sha256']};Assert-TaskIdentity $taskIdentity (Get-TaskHash $taskMutantBundle 98009)
 $taskBundleFirst=$null;try{$taskBundleFirst=[IO.File]::OpenRead($taskMutantBundle);$taskFirst=$taskBundleFirst.ReadByte()}finally{if($null -ne $taskBundleFirst){$taskBundleFirst.Dispose()}}
 $taskMutation=Mutate-TaskOwnedBytes $taskMutantBundle 0 ([byte[]]@([byte]($taskFirst -bxor 1)));$taskCorrupt=Get-TaskHash $taskMutantBundle 98009
 $taskControls.Add((Expect-TaskGate 'own-tool-or-predecessor-changed' {Assert-TaskIdentity $taskIdentity $taskCorrupt} 'BEFORE_AFTER_COMPLETE_INPUT_IDENTITY' @{path=$taskMutantBundle;expected_bytes=98009L;observed_bytes=$taskCorrupt['bytes'];expected_hash=$taskIdentity['sha256'];observed_hash=$taskCorrupt['sha256']}));$taskControls[$taskControls.Count-1]['mutation']=$taskMutation;$taskControls[$taskControls.Count-1]['initial_copy_sha256']=$taskBundleHash['sha256']
 $taskReceipt['reserved_source_work']=$taskTotalWork;$taskReceipt['reserved_admitted_retained']=$taskReservedRetained;$taskReceipt['status']='OPAQUE_COMPILER_STARTING';$taskReceipt['runner_controls']=$taskControls.ToArray();Save-TaskReceipt
 $taskCompiler=Resolve-TaskInput $taskManifest['compiler']['path'];Assert-TaskConsolePE (Get-TaskPEHeader $taskCompiler)
 $taskArguments=[Collections.Generic.List[string]]::new();foreach($taskArg in $taskManifest['compiler']['arguments']){$taskArguments.Add(([string]$taskArg).Replace('<BASE>',$taskBase).Replace('<REPO>',$taskRepo).Replace('<BUILD>',$taskBuildRoot))}
 $taskCompile=$null;try{Assert-TaskPins;$taskCompile=Invoke-TaskChild 'one-opaque-compile' $taskCompiler $taskArguments.ToArray()}finally{Assert-TaskPins};if($taskCompile['raw_exit'] -ne 0){throw 'Compiler raw failure'}
 $taskBuild=Get-TaskBuildArtifacts;$taskReceipt['compiler_final_outputs']=$taskBuild;$taskReceipt['source_dependencies']=Assert-TaskDependencies;$taskAdmittedCompiler=$true
 $taskBinary=Join-Path $taskBuildRoot 'clock_stage.exe';$taskPE=Get-TaskPEHeader $taskBinary;Assert-TaskConsolePE $taskPE;$taskInitialBinary=Get-TaskHash $taskBinary 33554432
 $taskGuiPath=Join-Path $taskRoot 'controls/gui-mutant.exe';Copy-TaskOwned $taskBinary $taskGuiPath $taskInitialBinary['bytes'] 33554432;Assert-TaskIdentity @{path=$taskGuiPath;bytes=$taskInitialBinary['bytes'];sha256=$taskInitialBinary['sha256']} (Get-TaskHash $taskGuiPath 33554432);Assert-TaskConsolePE (Get-TaskPEHeader $taskGuiPath)
 $taskGuiMutation=Mutate-TaskOwnedBytes $taskGuiPath ([long]$taskPE['offset']+24+68) ([BitConverter]::GetBytes([uint16]2));$taskGui=Get-TaskPEHeader $taskGuiPath
 $taskControls.Add((Expect-TaskGate 'pe-gui-subsystem' {Assert-TaskConsolePE $taskGui} 'ACTUAL_PE_CONSOLE_BEFORE_LAUNCH' @{signature=$taskGui['signature'];machine=$taskGui['machine'];magic=$taskGui['magic'];subsystem=2}));$taskControls[$taskControls.Count-1]['mutation']=$taskGuiMutation;$taskControls[$taskControls.Count-1]['initial_copy_sha256']=$taskInitialBinary['sha256'];$taskControls[$taskControls.Count-1]['actual_mutant_sha256']=(Get-TaskHash $taskGuiPath 33554432)['sha256']
 $taskBinaryHash=Get-TaskHash $taskBinary 33554432
 $taskProduce=$null;try{Assert-TaskPins;$taskProduce=Invoke-TaskChild 'oracle-produce' $taskBinary @('produce',$taskDataRoot,$taskExpectedManifest,[string]$taskRunnerLimit,[string]$taskRunnerLimit,[string]$taskTotalWork)}finally{Assert-TaskPins;Assert-TaskIdentity @{path=$taskBinary;bytes=$taskBinaryHash['bytes'];sha256=$taskBinaryHash['sha256']} (Get-TaskHash $taskBinary 33554432)}
 $taskProduced=Read-TaskChildResult $taskProduce;if($taskProduced['status'] -cne 'CLOCK_PRODUCED_REQUIRES_INDEPENDENT_VERIFY' -or $taskProduced['source_manifest_sha256'] -cne $taskExpectedManifest -or $taskProduced['entries'] -ne 16929027 -or $taskProduced['reserved'] -ne $taskManifest['source_work']['producer_upper'] -or $taskProduced['used'] -gt $taskProduced['reserved']){throw 'Actual producer summary/ledger'}
 $taskBefore=Get-TaskReferences;Assert-TaskFileSet $taskBefore['names'] @($taskCaps.Keys)
 $taskMissing=@($taskBefore['names']|Where-Object {$_ -cne 'references/native-inverse/r48000-map5.json'});$taskControls.Add((Expect-TaskGate 'missing-inverse-descriptor' {Assert-TaskFileSet $taskMissing @($taskCaps.Keys)} 'COMPLETE_EXACT_REFERENCE_FILE_SET' @{observed=55;expected=56;missing='references/native-inverse/r48000-map5.json';unknown='';duplicate=''}))
 $taskExtra=@($taskBefore['names'])+@('unlisted.bin');$taskControls.Add((Expect-TaskGate 'extra-file' {Assert-TaskFileSet $taskExtra @($taskCaps.Keys)} 'COMPLETE_EXACT_REFERENCE_FILE_SET' @{observed=57;expected=56;missing='';unknown='unlisted.bin';duplicate=''}))
 $taskDuplicate=@($taskBefore['names'])+@($taskBefore['names'][0]);$taskControls.Add((Expect-TaskGate 'duplicate-file' {Assert-TaskFileSet $taskDuplicate @($taskCaps.Keys)} 'COMPLETE_EXACT_REFERENCE_FILE_SET' @{observed=57;expected=56;missing='';unknown='';duplicate=$taskBefore['names'][0]}))
 $taskReceipt['producer']=$taskProduced;$taskReceipt['status']='PRODUCED_VERIFY_PENDING';Save-TaskReceipt
 $taskPrior=[long]$taskRunnerLimit+[long]$taskManifest['source_work']['producer_upper'];$taskVerify=$null
 try{Assert-TaskPins;$taskVerify=Invoke-TaskChild 'independent-full-verify' $taskBinary @('verify',$taskDataRoot,$taskExpectedManifest,[string]$taskPrior,[string]$taskRunnerLimit,[string]$taskTotalWork)}finally{Assert-TaskPins;Assert-TaskIdentity @{path=$taskBinary;bytes=$taskBinaryHash['bytes'];sha256=$taskBinaryHash['sha256']} (Get-TaskHash $taskBinary 33554432)}
 $taskVerified=Read-TaskChildResult $taskVerify;if($taskVerified['status'] -cne 'CLOCK_VERIFIED_REQUIRES_ROOT_OUTPUT_REVIEW' -or $taskVerified['source_manifest_sha256'] -cne $taskExpectedManifest -or $taskVerified['entries'] -ne 16929027 -or $taskVerified['reserved'] -ne $taskManifest['source_work']['verifier_upper'] -or $taskVerified['composed_after'] -ne $taskTotalWork -or $taskVerified['used'] -gt $taskVerified['reserved']){throw 'Actual independent replay summary/ledger'}
 $taskAfter=Get-TaskReferences;Assert-TaskSameReferences $taskBefore $taskAfter;$taskFinalBuild=Get-TaskBuildArtifacts
 foreach($taskOldFile in $taskBuild['files']){$taskNow=@($taskFinalBuild['files']|Where-Object path -ceq $taskOldFile['path']);if($taskNow.Count -ne 1){throw 'Compiler artifacts changed'};Assert-TaskIdentity $taskOldFile $taskNow[0]}
 $taskControlsFiles=[Collections.Generic.List[object]]::new();foreach($taskC in $taskManifest['control_files']){$taskPath=Join-Path $taskRoot $taskC['path'];$taskH=Get-TaskHash $taskPath $taskC['bytes_cap'];if($taskC.Contains('exact_bytes') -and $taskH['bytes'] -ne $taskC['exact_bytes']){throw 'Actual bounded record-control file length'};$taskControlsFiles.Add(@{path=$taskPath;bytes=$taskH['bytes'];sha256=$taskH['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 $taskExpectedRunFiles=[Collections.Generic.List[string]]::new();foreach($taskN in $taskCaps.Keys){$taskExpectedRunFiles.Add('data/'+$taskN)};foreach($taskN in $taskBuildCaps.Keys){$taskExpectedRunFiles.Add('build/'+$taskN)};foreach($taskC in $taskManifest['control_files']){$taskExpectedRunFiles.Add($taskC['path'])};foreach($taskN in @('stdout.log','stderr.log','receipt.json','consumed.json')){$taskExpectedRunFiles.Add('stage/'+$taskN)}
 $taskObservedRunFiles=[Collections.Generic.List[string]]::new();foreach($taskF in (Get-TaskEntries $taskRoot $true ([int]$taskManifest['profile']['root_walk_entries_max']))){Assert-NoLink $taskF.FullName;if(-not $taskF.PSIsContainer){$taskObservedRunFiles.Add([IO.Path]::GetRelativePath($taskRoot,$taskF.FullName).Replace([char]92,[char]47))}}
 Assert-TaskFileSet $taskObservedRunFiles.ToArray() $taskExpectedRunFiles.ToArray()
 Assert-TaskPins;$taskReceipt['verification']=$taskVerified;$taskReceipt['runner_controls']=$taskControls.ToArray();$taskReceipt['runner_used']=$taskUnits;$taskReceipt['status']='CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED';Save-TaskReceipt
 $taskArtifacts=[Collections.Generic.List[object]]::new();foreach($taskA in @($taskAfter['files'])+@($taskBuild['files'])+@($taskControlsFiles.ToArray())){$taskArtifacts.Add($taskA)}
 foreach($taskName in @('stdout.log','stderr.log','receipt.json','consumed.json')){$taskPath=Join-Path $taskStageRoot $taskName;$taskH=Get-TaskHash $taskPath 67108864;$taskArtifacts.Add(@{path=$taskPath;bytes=$taskH['bytes'];sha256=$taskH['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 $taskAdmitted=[long]$taskManifest['profile']['input_bytes']+$taskAuthoredBytes;foreach($taskA in $taskArtifacts){$taskAdmitted+=[long]$taskA['bytes']}
 $taskTransitive=[Collections.Generic.List[object]]::new();foreach($taskPin in $taskManifest['inputs']){$taskTransitive.Add(@{path=(Resolve-TaskInput $taskPin['path']);bytes=$taskPin['bytes'];sha256=$taskPin['sha256'];origin_bundle_sha256=$taskExpectedManifest})};foreach($taskPin in $taskManifest['sources']){$taskTransitive.Add(@{path=(Join-Path $taskBase $taskPin['path']);bytes=$taskPin['bytes'];sha256=$taskPin['sha256'];origin_bundle_sha256=$taskExpectedManifest})};$taskTransitive.Add(@{path=$taskManifestPath;bytes=(Get-Item -LiteralPath $taskManifestPath).Length;sha256=$taskExpectedManifest;origin_bundle_sha256=$taskExpectedManifest})
 $taskBundle=[ordered]@{schema=1;stage=$Stage;status='CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED';source_manifest_sha256=$taskExpectedManifest;predecessor_bundle=$taskManifest['predecessor_bundle'];artifacts=$taskArtifacts.ToArray();transitive_inputs=$taskTransitive.ToArray();admitted_retained_excluding_bundle=$taskAdmitted;bundle_bytes=0L;admitted_retained=0L;reserved_source_work=$taskTotalWork;runner_used=$taskUnits;compiler_operation='OPAQUE';compiler_temporary_peak_disk_qualified=$false;compiler_CPU_internal_heap_RSS_qualified=$false;old_broad_compiler_output_bound_fulfilled=$false;vendor=$false;quality=$false;context=$false;Ready=$false;renderer=$false}
 $taskStable=$false;for($taskPass=0;$taskPass -lt 5;$taskPass++){$taskRaw=Encode-TaskJson $taskBundle;if($taskBundle['bundle_bytes'] -eq $taskRaw.Length -and $taskBundle['admitted_retained'] -eq $taskAdmitted+$taskRaw.Length){$taskStable=$true;break};$taskBundle['bundle_bytes']=$taskRaw.Length;$taskBundle['admitted_retained']=$taskAdmitted+$taskRaw.Length}
 if(-not $taskStable){throw 'Exact admitted bundle byte-length fixed point'};Assert-TaskRetainedReservation $taskBundle['admitted_retained'] 880803840L;Write-TaskJson (Join-Path $taskStageRoot 'bundle.json') $taskBundle
 Write-Output ('CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED bundle_sha256='+(Get-TaskHash (Join-Path $taskStageRoot 'bundle.json') 1048576)['sha256'])
}catch{
 $taskReceipt['status']='FAILED_OR_NOT_REACHED_NO_CERTIFICATION';$taskReceipt['failure']=$_.Exception.Message.Substring(0,[Math]::Min(512,$_.Exception.Message.Length));$taskReceipt['compiler_final_outputs_admitted']=$taskAdmittedCompiler
 if($taskCreated){try{Save-TaskFailureInventory;Save-TaskReceipt}catch{Write-Error 'Failure footer incomplete; preserve all files and raw child/owned-read state. No certification.' -ErrorAction Continue}}
 throw
}
