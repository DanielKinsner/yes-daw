#Requires -Version 7.0
param(
 [Parameter(Mandatory)][ValidateSet('ClockOracle')][string]$Stage,
 [Parameter(Mandatory)][string]$Repo,
 [Parameter(Mandatory)][string]$NewRunRoot,
 [Parameter(Mandatory)][string]$ReviewedManifestSha256
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
# The four native caller strings are admitted before any variable path copy,
# normalization, case conversion, substring, replacement or character-array copy.
foreach($taskArgumentValue in @($Repo,$NewRunRoot,$PSScriptRoot)){
 if($null -eq $taskArgumentValue -or $taskArgumentValue.Length -eq 0 -or $taskArgumentValue.Length -gt 512){throw 'INITIAL_PATH_LENGTH_BEFORE_COPY'}
 for($taskCharacterIndex=0;$taskCharacterIndex -lt $taskArgumentValue.Length;$taskCharacterIndex++){if([int]$taskArgumentValue[$taskCharacterIndex] -lt 32 -or [int]$taskArgumentValue[$taskCharacterIndex] -gt 126){throw 'PINNED_ASCII_PATH_DOMAIN'}}
 if(-not [IO.Path]::IsPathFullyQualified($taskArgumentValue)){throw 'FULLY_QUALIFIED_PATH_BEFORE_NORMALIZATION'}
}
if($ReviewedManifestSha256.Length -ne 64){throw 'INITIAL_MANIFEST_LENGTH'}
for($taskCharacterIndex=0;$taskCharacterIndex -lt 64;$taskCharacterIndex++){if($ReviewedManifestSha256[$taskCharacterIndex] -notmatch '[0-9A-Fa-f]'){throw 'INITIAL_MANIFEST_HEX'}}
$taskSuffixes=@('/opaque-temp','/stage/failure-inventory.json','/build/source-dependencies.json','/controls/predecessor-mutant.json','/controls/gui-mutant.exe','/data/references/native-inverse/r48000-map5.json','/data/references/clock/r96000-map8-metadata.json')
foreach($taskSuffix in $taskSuffixes){if($NewRunRoot.Length -gt (512-$taskSuffix.Length)){throw 'DERIVED_RUN_PATH_LENGTH_BEFORE_COPY'}}
if($PSScriptRoot.Length -gt (512-('/source-manifest.json').Length)){throw 'DERIVED_SOURCE_PATH_LENGTH_BEFORE_COPY'}
$taskBase=[IO.Path]::GetFullPath($PSScriptRoot);$taskRepo=[IO.Path]::GetFullPath($Repo);$taskRoot=[IO.Path]::GetFullPath($NewRunRoot)
$taskManifestPath=[IO.Path]::Join($taskBase,'source-manifest.json');$taskExpectedManifest=$ReviewedManifestSha256.ToUpperInvariant()
$taskStageRoot=[IO.Path]::Join($taskRoot,'stage');$taskDataRoot=[IO.Path]::Join($taskRoot,'data');$taskBuildRoot=[IO.Path]::Join($taskRoot,'build');$taskOpaqueRoot=[IO.Path]::Join($taskRoot,'opaque-temp')
$taskOwnNames=@('clock_stage.cpp','run-clock.ps1','source-costs.h','README.md')
$taskPathCatalogue=$null
$taskManifest=$null;$taskUnits=0L;$taskRunnerLimit=0L;$taskStageCap=67108864L;$taskRetainedCap=880803840L;$taskScratch=33554432L
# One rooted synchronous hashing owner; never borrowed by child async reads.
$taskHashBuffer=[byte[]]::new(65536)
# Independent policy admission: 1800seconds*1000/250ms =7200 normal poll bodies.
# This count is enforced directly; Sleep duration is not its proof.
$taskChildPollMaximum=7200L
$taskJsonBytes=0L
# Admitted extents at explicit Add-TaskJsonVisit sites, not all codec passes
# or source-action prices: bootstrap+predecessor1MiB
# each, dependency4MiB, two64KiB results, marker/bundle/failure1MiB each and
# twelve once-encoded64KiB receipts. Preflight visits are separately counted.
$taskJsonVisitCap=2L*1048576L+4194304L+2L*65536L+3L*1048576L+12L*65536L
$taskRawLogBytes=0L;$taskQuarantine=$null;$taskReceiptWrites=0;$taskCreated=$false;$taskAdmittedCompiler=$false
$taskIntegerDomainMax=[long]::MaxValue;$taskReviewedWorkLimit=$null
$taskObjectGetType=[object].GetMethod('GetType',[Type[]]::EmptyTypes)
$taskConvertToJson=$null;$taskConvertFromJson=$null;$taskJsonProfile=$null
$taskJsonPreflightNodes=0L;$taskJsonStringUnits=0L;$taskJsonEncodes=0L
$taskSecondarySites=@('compile-after-fence','produce-after-fence','verify-after-fence','produce-after-binary','verify-after-binary','compile-child-finally','produce-child-finally','verify-child-finally','failure-inventory','failure-receipt','owned-dispose')
$taskSecondaryRaw=[object[]]::new($taskSecondarySites.Length)
$taskSecondaryCounts=[long[]]::new($taskSecondarySites.Length)
$taskPrimaryRaw=$null
$taskReceipt=[ordered]@{schema=1;stage=$Stage;status='NOT_STARTED_NO_CERTIFICATION';source_manifest_sha256=$taskExpectedManifest;compiler_is_opaque=$true;compiler_internal_CPU_heap_RSS_temporary_peak_disk_qualified=$false;old_broad_compiler_output_bound_fulfilled=$false;children=[Collections.Generic.List[object]]::new();compiler_exit=$null;producer_exit=$null;verifier_exit=$null;vendor=$false;quality=$false;context=$false;Ready=$false;renderer=$false;reserved_source_work=$null;reserved_admitted_retained=$null;runner_controls=[object[]]::new(0);compiler_final_outputs=$null;source_dependencies=$null;producer=$null;verification=$null;runner_used=0L;failure_code=$null;primary_failure=$false;secondary_failures=[object[]]::new(0);failure_inventory_complete=$null;failure_inventory_code=$null;compiler_final_outputs_admitted=$false}
# Decimal formatting thresholds describe the fixed integral representation, NOT
# family prices or a research work allowance. No floating quotient is used.
$taskSignedDigitThresholds=[long[]]@(10,100,1000,10000,100000,1000000,10000000,100000000,1000000000,10000000000,100000000000,1000000000000,10000000000000,100000000000000,1000000000000000,10000000000000000,100000000000000000,1000000000000000000)
$taskUnsignedDigitThresholds=[uint64[]]@(10,100,1000,10000,100000,1000000,10000000,100000000,1000000000,10000000000,100000000000,1000000000000,10000000000000,100000000000000,1000000000000000,10000000000000000,100000000000000000,1000000000000000000,[uint64]::Parse('10000000000000000000',[Globalization.CultureInfo]::InvariantCulture))
$taskSummaryFields=@('status','source_manifest_sha256','entries','inverse_records','reserved','used','prior','composed_after','vendor','quality','Ready','compiler_peak_disk_qualified')
function Spend-Task([long]$n){if($n -lt 0 -or $n -gt $script:taskRunnerLimit-$script:taskUnits){throw 'CONTROLLED_RUNNER_SOURCE_WORK_REFUSED'};$script:taskUnits+=$n}
function Test-TaskPathCatalogueContext{
 return ($null -ne $script:taskPathCatalogue -and $script:taskPathCatalogue['repo'] -ceq $taskRepo -and $script:taskPathCatalogue['base'] -ceq $taskBase -and $script:taskPathCatalogue['root'] -ceq $taskRoot)
}
function Add-TaskPathCatalogueEntry([string]$Name,[bool]$RepoInput){
 $taskCatalogue=$script:taskPathCatalogue
 if($taskCatalogue['rows'].Count -ge $taskCatalogue['maximum']){throw 'PATH_CATALOGUE_DERIVED_IDENTITY_COUNT'}
 $taskEntry=@{name=$Name;repo_input=$RepoInput;canonical=$null;parents=$null;parent_count=0}
 $taskCatalogue['rows'].Add($taskEntry)
 if($RepoInput){$taskCatalogue['inputs'].Add($Name,$taskEntry)}else{
  $taskCatalogue['aliases'].Add($Name,$taskEntry)
  # Ordinary fully-qualified drive paths normalize slash spelling identically.
  # Device/UNC spellings retain only their exact original alias and fallback.
  if($Name.Length -ge 3 -and $Name[1] -ceq [char]58){
   $taskSlashes=$Name.Replace([IO.Path]::AltDirectorySeparatorChar,[IO.Path]::DirectorySeparatorChar)
   if(-not $taskCatalogue['aliases'].ContainsKey($taskSlashes)){$taskCatalogue['aliases'].Add($taskSlashes,$taskEntry)}
  }
 }
}
function Initialize-TaskPathCatalogue{
 # Approved physical file identities only. Directory parents are strings within
 # those identities, not cached filesystem metadata or additional file authority.
 $taskMaximum=[long]$taskManifest['inputs'].Count+$taskOwnNames.Count+1+$taskManifest['profile']['declared_success_run_files']+$taskManifest['profile']['declared_failure_extra_files']
 if($taskMaximum -ne $taskManifest['profile']['maximum_complete_fence_identities'] -or $taskMaximum -lt 1 -or $taskMaximum -gt [int]::MaxValue -or 65L*$taskMaximum -gt [int]::MaxValue){throw 'PATH_CATALOGUE_DERIVED_PROFILE'}
 $script:taskPathCatalogue=@{repo=$taskRepo;base=$taskBase;root=$taskRoot;maximum=[int]$taskMaximum;alias_maximum=(2L*$taskManifest['inputs'].Count+3L*($taskMaximum-$taskManifest['inputs'].Count));rows=[Collections.Generic.List[object]]::new([int]$taskMaximum);inputs=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal);aliases=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal);strings=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal);parent_strings=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)}
 foreach($taskPin in $taskManifest['inputs']){Add-TaskPathCatalogueEntry $taskPin['path'] $true}
 foreach($taskName in $taskOwnNames){Add-TaskPathCatalogueEntry ([IO.Path]::Join($taskBase,$taskName)) $false}
 Add-TaskPathCatalogueEntry $taskManifestPath $false
 foreach($taskA in $taskManifest['references']){Add-TaskPathCatalogueEntry ([IO.Path]::Join($taskDataRoot,$taskA['path'])) $false}
 foreach($taskA in $taskManifest['compiler_final_outputs']){Add-TaskPathCatalogueEntry ([IO.Path]::Join($taskBuildRoot,$taskA['name'])) $false}
 foreach($taskA in $taskManifest['control_files']){Add-TaskPathCatalogueEntry ([IO.Path]::Join($taskRoot,$taskA['path'])) $false}
 foreach($taskName in $taskManifest['stage_files']){Add-TaskPathCatalogueEntry ([IO.Path]::Join($taskStageRoot,$taskName)) $false}
 if($script:taskPathCatalogue['rows'].Count -ne $taskMaximum){throw 'PATH_CATALOGUE_COMPLETE_DERIVED_RECIPE'}
}
function Get-TaskCatalogueString([string]$Path){
 $taskStrings=$script:taskPathCatalogue['strings']
 if($taskStrings.ContainsKey($Path)){return $taskStrings[$Path]}
 if($taskStrings.Count -ge 65L*$script:taskPathCatalogue['maximum']){throw 'PATH_CATALOGUE_DERIVED_PARENT_COUNT'}
 $taskStrings.Add($Path,$Path);return $Path
}
function Get-TaskCatalogueEntryPath($Entry){
 if($null -ne $Entry['canonical']){return $Entry['canonical']}
 $taskName=$Entry['name'];$taskRaw=$taskName
 if($Entry['repo_input'] -and -not [IO.Path]::IsPathFullyQualified($taskName)){$taskRaw=Join-Path $taskRepo $taskName}
 # Tilde expansion can depend on current filesystem state. Those paths retain
 # the original normalization on EVERY call; no domain restriction is added.
 if($taskRaw.IndexOf([char]126) -ge 0){return [IO.Path]::GetFullPath($taskRaw)}
 $taskCanonical=[IO.Path]::GetFullPath($taskRaw)
 if($taskCanonical.IndexOf([char]126) -ge 0){return $taskCanonical}
 $taskCanonical=Get-TaskCatalogueString $taskCanonical
 $taskParents=[string[]]::new(65);$taskParentCount=0;$taskAt=$taskCanonical
 while($taskAt -and $taskParentCount -lt 65){
  $taskAt=Get-TaskCatalogueString $taskAt;$taskParents[$taskParentCount]=$taskAt;$taskParentCount++
  # Original Assert-NoLink rejects depth65 BEFORE attributes or next-parent.
  if($taskParentCount -eq 65){break}
  if($script:taskPathCatalogue['parent_strings'].ContainsKey($taskAt)){$taskAt=$script:taskPathCatalogue['parent_strings'][$taskAt]}else{
   $taskParent=[IO.Path]::GetDirectoryName($taskAt)
   if($taskParent){$taskParent=Get-TaskCatalogueString $taskParent}
   $script:taskPathCatalogue['parent_strings'].Add($taskAt,$taskParent);$taskAt=$taskParent
  }
 }
 $taskAliases=$script:taskPathCatalogue['aliases']
 foreach($taskAlias in @($taskRaw,$taskCanonical)){
  if(-not $taskAliases.ContainsKey($taskAlias)){
   if($taskAliases.Count -ge $script:taskPathCatalogue['alias_maximum']){throw 'PATH_CATALOGUE_DERIVED_ALIAS_COUNT'}
   $taskAliases.Add($taskAlias,$Entry)
  }
 }
 $Entry['canonical']=$taskCanonical;$Entry['parents']=$taskParents;$Entry['parent_count']=$taskParentCount
 return $taskCanonical
}
function Get-TaskCanonicalPath([string]$Path){
 if((Test-TaskPathCatalogueContext) -and $Path.IndexOf([char]126) -lt 0){
  if($script:taskPathCatalogue['aliases'].ContainsKey($Path)){return Get-TaskCatalogueEntryPath $script:taskPathCatalogue['aliases'][$Path]}
  if($script:taskPathCatalogue['strings'].ContainsKey($Path)){return $script:taskPathCatalogue['strings'][$Path]}
 }
 # Unknown strings and changed root contexts keep the original normalization.
 return [IO.Path]::GetFullPath($Path)
}
function Assert-NoLink([string]$Path){
 if($Path.Length -gt 512){throw 'PATH_LENGTH_BEFORE_NORMALIZATION'}
 $taskAt=Get-TaskCanonicalPath $Path;$taskDepth=0
 while($taskAt){Spend-Task 1;$taskDepth++;if($taskDepth -gt 64){throw 'Path depth'};if([IO.File]::Exists($taskAt) -or [IO.Directory]::Exists($taskAt)){if(([IO.File]::GetAttributes($taskAt)-band [IO.FileAttributes]::ReparsePoint)-ne 0){throw 'Reparse path refused'}};if((Test-TaskPathCatalogueContext) -and $script:taskPathCatalogue['parent_strings'].ContainsKey($taskAt)){$taskAt=$script:taskPathCatalogue['parent_strings'][$taskAt]}else{$taskAt=[IO.Path]::GetDirectoryName($taskAt)}}
}
function Get-TaskHash([string]$Path,[long]$Maximum){
 Spend-Task 64;Assert-NoLink $Path;$taskItem=[IO.FileInfo]::new($Path);$taskLength=$taskItem.Length;if($taskLength -gt $Maximum -or $taskLength -lt 0){throw 'Hash file cap'}
 $taskStream=$null;$taskHash=$null;$taskBuffer=$script:taskHashBuffer;$taskSeen=0L;$taskBefore=$script:taskPrimaryRaw;$taskBody=$null
 try{$taskStream=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,1,[IO.FileOptions]::SequentialScan);if($taskStream.Length -ne $taskLength){throw 'Hash input length changed before owned read'};$taskHash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
  while($taskSeen -lt $taskLength){Spend-Task 32;$taskN=[int][Math]::Min(65536L,$taskLength-$taskSeen);$taskStream.ReadExactly($taskBuffer,0,$taskN);$taskSeen+=$taskN;$taskHash.AppendData($taskBuffer,0,$taskN)}
  if($taskStream.ReadByte() -ne -1){throw 'Hash input grew beyond exact EOF'};return @{bytes=$taskSeen;sha256=[Convert]::ToHexString($taskHash.GetHashAndReset())}
 }catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}
 finally{Close-TaskOwner $taskHash;Close-TaskOwner $taskStream;Complete-TaskOwnerClose $taskBefore $taskBody}
}
function Assert-TaskPollAdmission([long]$CompletedPolls){
 if($CompletedPolls -lt 0 -or $CompletedPolls -ge $script:taskChildPollMaximum){Reject-TaskGate 'CHILD_POLL_LIMIT' @{completed=$CompletedPolls;limit=$script:taskChildPollMaximum;attempted=($CompletedPolls+1)}}
}
function Invoke-TaskChild([string]$Name,[string]$Executable,[string[]]$Arguments){
 if($Name -cnotin @('one-opaque-compile','oracle-produce','independent-full-verify')){throw 'Exact finite child name profile'}
 $taskChildIndex=if($Name -ceq 'one-opaque-compile'){0}elseif($Name -ceq 'oracle-produce'){1}else{2}
 if($Arguments.Length -ne $(if($taskChildIndex -eq 0){28}else{6})){throw 'EXACT_CHILD_ARGUMENT_EXTENT'}
 foreach($taskArgument in $Arguments){Assert-TaskString $taskArgument 512 $true}
 Spend-Task 4096;Spend-Task 8192;if($null -ne $script:taskQuarantine){throw 'Unsettled prior child owns storage; no next child'}
 $taskProcess=$null;$taskStarted=$false;$taskCancel=$null;$taskOut=$null;$taskErr=$null;$taskReadA=$null;$taskReadB=$null;$taskAccountA=$true;$taskAccountB=$true
 $taskA=[byte[]]::new(4096);$taskB=[byte[]]::new(4096);$taskDoneA=$false;$taskDoneB=$false;$taskExit=$null;$taskExitObserved=$false;$taskFailure=$null;$taskTimedOut=$false;$taskPollLimited=$false;$taskPolls=0L;$taskCap=$false
 $taskObserved=0L;$taskUnretained=0L;$taskWritten=0L;$taskOmitted=0L;$taskSettled=$true;$taskCleanup=[Collections.Generic.List[string]]::new(13);$taskBeforeA=0L;$taskBeforeB=0L;$taskEndA=0L;$taskEndB=0L;$taskPrimary=$null;$taskFooter=$null;$taskPositions=$true
 try{
  $taskCancel=[Threading.CancellationTokenSource]::new()
  $taskOut=[IO.FileStream]::new((Join-Path $taskStageRoot 'stdout.log'),[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
  $taskErr=[IO.FileStream]::new((Join-Path $taskStageRoot 'stderr.log'),[IO.FileMode]::Append,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
  $taskBeforeA=$taskOut.Position;$taskBeforeB=$taskErr.Position
  $taskInfo=[Diagnostics.ProcessStartInfo]::new();$taskInfo.FileName=$Executable;$taskInfo.WorkingDirectory=$taskRoot;$taskInfo.UseShellExecute=$false;$taskInfo.CreateNoWindow=$true;$taskInfo.RedirectStandardOutput=$true;$taskInfo.RedirectStandardError=$true
  Assert-TaskExactKeys $taskManifest['environment'] @('SystemRoot','windir','PATH','INCLUDE','LIB','TEMP','TMP','VSLANG')
  $taskInfo.Environment.Clear();foreach($taskKey in $taskManifest['environment'].Keys){Assert-TaskString $taskManifest['environment'][$taskKey] 512 $true;$taskValue=$taskManifest['environment'][$taskKey];if($taskValue -ceq '<OWNED_OPAQUE_TEMP>'){$taskValue=$taskOpaqueRoot};$taskInfo.Environment[$taskKey]=$taskValue}
  foreach($taskArgument in $Arguments){$taskInfo.ArgumentList.Add($taskArgument)}
  $taskProcess=[Diagnostics.Process]::Start($taskInfo);$taskStarted=$true;$taskWatch=[Diagnostics.Stopwatch]::StartNew()
  $taskReadA=$taskProcess.StandardOutput.BaseStream.ReadAtLeastAsync([Memory[byte]]::new($taskA),4096,$false,$taskCancel.Token).AsTask();$taskAccountA=$false
  $taskReadB=$taskProcess.StandardError.BaseStream.ReadAtLeastAsync([Memory[byte]]::new($taskB),4096,$false,$taskCancel.Token).AsTask();$taskAccountB=$false
  while(-not($taskDoneA -and $taskDoneB -and $taskProcess.HasExited)){
   $taskPollLimited=($taskPolls -ge $script:taskChildPollMaximum);Assert-TaskPollAdmission $taskPolls;$taskPolls++
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
 }catch{$taskPrimary=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};$taskFailure=if($taskPollLimited){'CHILD_POLL_LIMIT'}else{'CHILD_BODY_FAILURE_RAW_RETAINED'}}
 finally{
  try{
  if($null -ne $taskCancel){try{$taskCancel.Cancel()}catch{Add-TaskChildCleanup $taskCleanup 'read_cancel' $_ (5+$taskChildIndex)}}
  if($taskStarted){try{if(-not $taskProcess.HasExited){$taskProcess.Kill($true);$null=$taskProcess.WaitForExit(2000)};if($taskProcess.HasExited){$taskExit=$taskProcess.ExitCode;$taskExitObserved=$true}else{Add-TaskChildCleanup $taskCleanup 'own_child_exit_not_observed_2000ms' $null (5+$taskChildIndex)}}catch{Add-TaskChildCleanup $taskCleanup 'own_child_reap_failure' $_ (5+$taskChildIndex)}}
  [Threading.Tasks.Task[]]$taskReads=@();if($null -ne $taskReadA){$taskReads+=@($taskReadA)};if($null -ne $taskReadB){$taskReads+=@($taskReadB)}
  if($taskReads.Length -gt 0){try{$null=[Threading.Tasks.Task]::WaitAll($taskReads,2000)}catch{Add-TaskChildCleanup $taskCleanup 'read_settlement_diagnostic' $_ (5+$taskChildIndex)}}
  foreach($taskDirection in 0,1){
   $taskRead=if($taskDirection -eq 0){$taskReadA}else{$taskReadB};$taskAccount=if($taskDirection -eq 0){$taskAccountA}else{$taskAccountB}
   if($null -ne $taskRead){if(-not $taskRead.IsCompleted){$taskSettled=$false}else{if(-not $taskAccount){try{$taskN=$taskRead.GetAwaiter().GetResult();$taskObserved+=$taskN;$taskUnretained+=$taskN;if($taskN -eq 0){if($taskDirection -eq 0){$taskDoneA=$true}else{$taskDoneB=$true}}}catch{Add-TaskChildCleanup $taskCleanup 'settled_read_cancel_or_fault' $_ (5+$taskChildIndex)}}}}
  }
  foreach($taskSink in @($taskOut,$taskErr)){if($null -ne $taskSink){try{$taskSink.Flush()}catch{Add-TaskChildCleanup $taskCleanup 'retained_prefix_flush' $_ (5+$taskChildIndex)}}}
  if($null -ne $taskOut){try{$taskEndA=$taskOut.Position}catch{$taskPositions=$false;Add-TaskChildCleanup $taskCleanup 'retained_position_refusal' $_ (5+$taskChildIndex)}}
  if($null -ne $taskErr){try{$taskEndB=$taskErr.Position}catch{$taskPositions=$false;Add-TaskChildCleanup $taskCleanup 'retained_position_refusal' $_ (5+$taskChildIndex)}}
  if($taskSettled -and (-not $taskStarted -or $taskExitObserved)){
   [Array]::Clear($taskA,0,$taskA.Length);[Array]::Clear($taskB,0,$taskB.Length)
   foreach($taskOwner in @($taskOut,$taskErr,$taskCancel,$taskProcess)){if($null -ne $taskOwner){try{$taskOwner.Dispose()}catch{Add-TaskChildCleanup $taskCleanup 'settled_owner_dispose' $_ (5+$taskChildIndex)}}}
  }else{
   $script:taskQuarantine=@{process=$taskProcess;cancel=$taskCancel;readA=$taskReadA;readB=$taskReadB;bufferA=$taskA;bufferB=$taskB;stdout=$taskOut;stderr=$taskErr};Add-TaskChildCleanup $taskCleanup 'one_rooted_unsettled_owner_no_more_children' $null (5+$taskChildIndex)
  }
  if($taskCleanup.Count -gt 0 -and $null -eq $taskFailure){$taskFailure='Bounded cleanup refusal'}
  $taskRow=[ordered]@{name=$Name;started=$taskStarted;raw_exit_observed=$taskExitObserved;raw_exit=$taskExit;timed_out=$taskTimedOut;poll_limited=$taskPollLimited;polls=$taskPolls;poll_limit=$script:taskChildPollMaximum;log_cap=$taskCap;stdout_start=$taskBeforeA;stderr_start=$taskBeforeB;raw_stdout_bytes=$(if($taskPositions){$taskEndA-$taskBeforeA}else{$null});raw_stderr_bytes=$(if($taskPositions){$taskEndB-$taskBeforeB}else{$null});position_record_complete=$taskPositions;raw_bytes_retained=$taskWritten;raw_bytes_observed=$taskObserved;observed_not_confirmed_retained=$taskUnretained;omitted_at_cap=$taskOmitted;unknown_undrained=($taskStarted -and -not($taskDoneA -and $taskDoneB));both_eof=($taskDoneA -and $taskDoneB);reads_settled=$taskSettled;owned_storage_quarantined=($null -ne $script:taskQuarantine);cleanup=$taskCleanup.ToArray();failure=$taskFailure}
  $script:taskReceipt['children'].Add($taskRow)
  if($taskExitObserved){switch -Exact ($Name){'one-opaque-compile'{$script:taskReceipt['compiler_exit']=$taskExit};'oracle-produce'{$script:taskReceipt['producer_exit']=$taskExit};'independent-full-verify'{$script:taskReceipt['verifier_exit']=$taskExit};default{throw 'Unlisted child name in final owned record'}}}
  }catch{
   $taskFooter=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_}else{Set-TaskSecondary (5+$taskChildIndex) $_}
   if($null -eq $script:taskQuarantine){$script:taskQuarantine=@{process=$taskProcess;cancel=$taskCancel;readA=$taskReadA;readB=$taskReadB;bufferA=$taskA;bufferB=$taskB;stdout=$taskOut;stderr=$taskErr;raw_exit=$taskExit;raw_exit_observed=$taskExitObserved;footer_refused=$true}}
   Set-TaskRawExit $Name $taskExitObserved $taskExit
  }
 }
 if($null -ne $taskPrimary){throw $taskPrimary};if($null -ne $taskFooter){throw $taskFooter}
 if($null -ne $taskFailure){if($null -ne $script:taskPrimaryRaw){throw $script:taskPrimaryRaw};throw 'CHILD_CLEANUP_REFUSED'};if(-not $taskExitObserved -or $taskExit -ne 0){throw 'ACTUAL_CHILD_RAW_EXIT_FAILURE'}
 return $taskRow
}
function Add-TaskChildCleanup([Collections.Generic.List[string]]$Rows,[string]$Code,$Raw,[int]$Slot){
 # cancel1 + reap1 + settlement1 + two reads + two flushes + two positions
 # + four settled owners. Quarantine replaces those four owner disposals.
 $taskMaximum=1+1+1+2+2+2+4
 if($Rows.Count -ge $taskMaximum){throw 'FINITE_CHILD_CLEANUP_SCHEDULE'}
 Assert-TaskString $Code 64 $true;$Rows.Add($Code)
 if($null -ne $Raw){if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$Raw}else{Set-TaskSecondary $Slot $Raw}}
}
function Set-TaskRawExit([string]$Name,[bool]$Observed,$Value){
 if($Observed){switch -Exact ($Name){'one-opaque-compile'{$script:taskReceipt['compiler_exit']=$Value};'oracle-produce'{$script:taskReceipt['producer_exit']=$Value};'independent-full-verify'{$script:taskReceipt['verifier_exit']=$Value};default{throw 'INTERNAL_RAW_EXIT_SLOT'}}}
}
function Invoke-TaskFencedChild([string]$Name,[string]$Executable,[string[]]$Arguments,$ExpectedBinary=$null){
 if($Name -cnotin @('one-opaque-compile','oracle-produce','independent-full-verify')){throw 'EXACT_FENCED_CHILD_NAME'}
 $taskIndex=if($Name -ceq 'one-opaque-compile'){0}elseif($Name -ceq 'oracle-produce'){1}else{2}
 $taskBody=$null;$taskResult=$null
 try{Assert-TaskPins;$taskResult=Invoke-TaskChild $Name $Executable $Arguments}
 catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_}}
 finally{
  try{Assert-TaskPins}catch{if($null -eq $taskBody){$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_}}else{Set-TaskSecondary $taskIndex $_}}
  if($null -ne $ExpectedBinary){try{Assert-TaskIdentity @{path=$Executable;bytes=$ExpectedBinary['bytes'];sha256=$ExpectedBinary['sha256']} (Get-TaskHash $Executable 33554432)}catch{if($null -eq $taskBody){$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_}}else{Set-TaskSecondary (2+$taskIndex) $_}}}
 }
 if($null -ne $taskBody){throw $taskBody};return $taskResult
}
function Replace-TaskBounded([string]$Value,[string]$Token,[string]$Replacement){
 Assert-TaskString $Value 512 $true;Assert-TaskString $Token 512 $true;Assert-TaskString $Replacement 512 $true
 if($Token.Length -eq 0){throw 'NONEMPTY_REPLACEMENT_TOKEN'}
 $taskCount=0L;$taskAt=0
 while($taskAt -le $Value.Length-$Token.Length){$taskFound=$Value.IndexOf($Token,$taskAt,[StringComparison]::Ordinal);if($taskFound -lt 0){break};$taskCount++;$taskAt=$taskFound+$Token.Length}
 $taskLength=$Value.Length+$taskCount*($Replacement.Length-$Token.Length)
 if($taskLength -lt 0 -or $taskLength -gt 512){throw 'REPLACEMENT_LENGTH_BEFORE_COPY'}
 return $Value.Replace($Token,$Replacement)
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
function Get-TaskRuntimeType($Value){if($null -eq $Value){return $null};return $script:taskObjectGetType.Invoke($Value,$null)}
function Get-TaskBootstrapHash([string]$Path,[long]$ExpectedBytes){
 # This finite fixed controller-authentication region precedes config/limit
 # decoding. It must be prepaid by the future reviewed whole source bound;
 # no Spend debit or chosen numeric work allowance is inferred here.
 if($Path.Length -gt 512 -or $ExpectedBytes -le 0){throw 'BOOTSTRAP_HASH_DOMAIN'}
 $taskAt=$Path;$taskDepth=0
 while($taskAt){$taskDepth++;if($taskDepth -gt 64){throw 'BOOTSTRAP_PATH_DEPTH'};if(([IO.File]::Exists($taskAt) -or [IO.Directory]::Exists($taskAt)) -and (([IO.File]::GetAttributes($taskAt)-band [IO.FileAttributes]::ReparsePoint)-ne 0)){throw 'BOOTSTRAP_REPARSE'};$taskAt=[IO.Path]::GetDirectoryName($taskAt)}
 if([IO.FileInfo]::new($Path).Length -ne $ExpectedBytes){throw 'BOOTSTRAP_FILE_LENGTH'}
 $taskBefore=$script:taskPrimaryRaw;$taskBody=$null;$taskFile=$null;$taskHash=$null;$taskBuffer=$script:taskHashBuffer;$taskSeen=0L
 try{
  $taskFile=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,1,[IO.FileOptions]::SequentialScan)
  if($taskFile.Length -ne $ExpectedBytes){throw 'BOOTSTRAP_OPEN_LENGTH'};$taskHash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
  while($taskSeen -lt $ExpectedBytes){$taskN=[int][Math]::Min(65536L,$ExpectedBytes-$taskSeen);$taskFile.ReadExactly($taskBuffer,0,$taskN);$taskHash.AppendData($taskBuffer,0,$taskN);$taskSeen+=$taskN}
  if($taskFile.ReadByte() -ne -1){throw 'BOOTSTRAP_EXACT_EOF'};return [Convert]::ToHexString($taskHash.GetHashAndReset())
 }catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}
 finally{Close-TaskOwner $taskHash;Close-TaskOwner $taskFile;Complete-TaskOwnerClose $taskBefore $taskBody}
}
function Initialize-TaskController{
 # The trusted launcher must clear these BEFORE process startup as well.
 # These absence gates are a fail-closed confirmation, not a claim that an
 # already executed startup hook/profiler could be undone by this script.
 foreach($taskName in @('DOTNET_STARTUP_HOOKS','DOTNET_ADDITIONAL_DEPS','DOTNET_SHARED_STORE','CORECLR_ENABLE_PROFILING','CORECLR_PROFILER','COR_ENABLE_PROFILING','COR_PROFILER')){if($null -ne [Environment]::GetEnvironmentVariable($taskName)){throw 'CLOSED_CONTROLLER_STARTUP_ENVIRONMENT'}}
 $taskRuntime='C:\Users\SM - Dan\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell'
 $taskPins=@(
  @('pwsh.exe',301368L,'362A356CE7F0940EC74F73A8FC2C990A2CC24A38A11C90BBD8ECA947110AD139'),
  @('System.Management.Automation.dll',19646264L,'2D8AF026E589C5603629BFC3E7C57265B4167B12ECA3AE549C6A6C3CA7E4967B'),
  @('Microsoft.PowerShell.Commands.Utility.dll',1652504L,'2F6201BB3CAF4C08158BE733E786DD85B11537694F7E6AB40356FC0727A3596F'),
  @('Microsoft.PowerShell.Commands.Management.dll',1124152L,'40818818DD94A5FA1F53043E33D31D8312ADAD24EF61B06481DFAE4C45D6E677'),
  @('Newtonsoft.Json.dll',1824600L,'9AF3870D53662BA1C8E9B19EC8AD0020E815FFE9E916A1F3204AC6E206447F60'),
  @('Modules/Microsoft.PowerShell.Management/Microsoft.PowerShell.Management.psd1',16076L,'A73617CBA10DF0D2641DF909C6B8BD7981769B3FC78845976290093DF2941941'),
  @('Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1',16826L,'D103110AAE93CA1061FAB8ADD8F00EA6110740AC94B236654D0EFB0B08654AAA')
 )
 foreach($taskPin in $taskPins){$taskPath=[IO.Path]::Join($taskRuntime,$taskPin[0]);if((Get-TaskBootstrapHash $taskPath $taskPin[1]) -cne $taskPin[2]){throw 'EXACT_BOOTSTRAP_RUNTIME_IDENTITY'}}
 $taskSelf=[Diagnostics.Process]::GetCurrentProcess();$taskBefore=$script:taskPrimaryRaw;$taskBody=$null
 try{if(-not $taskSelf.MainModule.FileName.Equals([IO.Path]::Join($taskRuntime,'pwsh.exe'),[StringComparison]::OrdinalIgnoreCase)){throw 'EXACT_NATIVE_CONTROLLER'}}catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}finally{Close-TaskOwner $taskSelf;Complete-TaskOwnerClose $taskBefore $taskBody}
 $taskArgs=[Environment]::GetCommandLineArgs()
 $taskExpected=@('<APPHOST>','-NoLogo','-NoProfile','-NonInteractive','-File',[IO.Path]::Join($taskBase,'run-clock.ps1'),'-Stage','ClockOracle','-Repo',$Repo,'-NewRunRoot',$NewRunRoot,'-ReviewedManifestSha256',$ReviewedManifestSha256)
 if($taskArgs.Length -ne $taskExpected.Length){throw 'EXACT_FRESH_CONTROLLER_ARGUMENT_COUNT'}
 for($taskI=0;$taskI -lt $taskArgs.Length;$taskI++){Assert-TaskString $taskArgs[$taskI] 512 $true;if($taskI -gt 0 -and -not $taskArgs[$taskI].Equals($taskExpected[$taskI],[StringComparison]::OrdinalIgnoreCase)){throw 'EXACT_NO_PROFILE_NONINTERACTIVE_CONTROLLER'}}
 $taskApp=[IO.Path]::GetFullPath($taskArgs[0]);if(-not $taskApp.Equals([IO.Path]::Join($taskRuntime,'pwsh.exe'),[StringComparison]::OrdinalIgnoreCase) -and -not $taskApp.Equals([IO.Path]::Join($taskRuntime,'pwsh.dll'),[StringComparison]::OrdinalIgnoreCase)){throw 'EXACT_APPHOST_ARGUMENT'}
 $script:PSModuleAutoLoadingPreference='None'
 Microsoft.PowerShell.Core\Import-Module -Name ([IO.Path]::Join($taskRuntime,'Modules/Microsoft.PowerShell.Management/Microsoft.PowerShell.Management.psd1')) -ErrorAction Stop
 Microsoft.PowerShell.Core\Import-Module -Name ([IO.Path]::Join($taskRuntime,'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')) -ErrorAction Stop
 $script:taskConvertToJson=Microsoft.PowerShell.Core\Get-Command -Name 'Microsoft.PowerShell.Utility\ConvertTo-Json' -CommandType Cmdlet -ErrorAction Stop
 $script:taskConvertFromJson=Microsoft.PowerShell.Core\Get-Command -Name 'Microsoft.PowerShell.Utility\ConvertFrom-Json' -CommandType Cmdlet -ErrorAction Stop
 if($script:taskConvertToJson.ImplementingType -ne [Microsoft.PowerShell.Commands.ConvertToJsonCommand] -or $script:taskConvertFromJson.ImplementingType -ne [Microsoft.PowerShell.Commands.ConvertFromJsonCommand]){throw 'EXACT_SERIALIZER_IMPLEMENTING_TYPES'}
 foreach($taskCodec in @($script:taskConvertToJson,$script:taskConvertFromJson)){if(-not $taskCodec.ImplementingType.Assembly.Location.Equals([IO.Path]::Join($taskRuntime,'Microsoft.PowerShell.Commands.Utility.dll'),[StringComparison]::OrdinalIgnoreCase)){throw 'EXACT_SERIALIZER_ASSEMBLY'}}
 Assert-TaskCodecBinding
 # Fresh fourteen-argument NoProfile/NonInteractive invocation; these two exact
 # manifests load only their pinned DLLs. This source registers no type data,
 # extended properties, converter/default delegate, callback or custom object
 # in the serialization caller graph. The primary-source audit is in README.
}
function Assert-TaskString($Value,[int]$Maximum=512,[bool]$Ascii=$false){
 if((Get-TaskRuntimeType $Value) -ne [string] -or $Value.Length -gt $Maximum){throw 'EXACT_BOUNDED_STRING'}
 for($taskI=0;$taskI -lt $Value.Length;$taskI++){
  $taskC=[int]$Value[$taskI]
  if($Ascii -and ($taskC -lt 32 -or $taskC -gt 126)){throw 'ASCII_STRING_DOMAIN'}
  if([char]::IsHighSurrogate($Value[$taskI])){if($taskI+1 -ge $Value.Length -or -not [char]::IsLowSurrogate($Value[$taskI+1])){throw 'VALID_UTF16_SURROGATES'}}
  elseif([char]::IsLowSurrogate($Value[$taskI])){if($taskI -eq 0 -or -not [char]::IsHighSurrogate($Value[$taskI-1])){throw 'VALID_UTF16_SURROGATES'}}
 }
}
function Get-TaskSafePrefix([string]$Value,[int]$Maximum=512){
 if($Maximum -lt 0 -or $Maximum -gt 512){throw 'PREFIX_MAXIMUM'}
 $taskN=[Math]::Min($Maximum,$Value.Length)
 if($taskN -gt 0 -and [char]::IsHighSurrogate($Value[$taskN-1])){$taskN--}
 return $Value.Substring(0,$taskN)
}
function Assert-TaskIntegral($Value,[bool]$Nonnegative=$false){
 $taskT=Get-TaskRuntimeType $Value
 if($taskT -ne [sbyte] -and $taskT -ne [byte] -and $taskT -ne [int16] -and $taskT -ne [uint16] -and $taskT -ne [int32] -and $taskT -ne [uint32] -and $taskT -ne [int64] -and $taskT -ne [uint64]){throw 'EXACT_JSON_INTEGRAL_KIND'}
 if($Nonnegative -and $Value -lt 0){throw 'NONNEGATIVE_JSON_INTEGER'}
}
function Convert-TaskControlOperand($Value){
 # ONLY the six parsed C++ uint64 control operands use this normalization.
 # General JSON output and summary integral allowlists remain unchanged.
 if((Get-TaskRuntimeType $Value) -eq [System.Numerics.BigInteger]){
  if($Value.Sign -lt 0 -or $Value.CompareTo([System.Numerics.BigInteger]::new([uint64]::MaxValue)) -gt 0){throw 'CONTROL_OPERAND_UINT64_DOMAIN'}
  return [uint64]$Value
 }
 Assert-TaskIntegral $Value $true
 # Every other admitted nonnegative builtin integral kind fits UInt64 already;
 # preserve its exact kind instead of changing the global serializer domain.
 return $Value
}
function Assert-TaskExactKeys($Map,[string[]]$Keys){
 $taskT=Get-TaskRuntimeType $Map
 if($taskT -ne [Collections.Hashtable] -and $taskT -ne [Collections.Specialized.OrderedDictionary] -and $taskT -ne [System.Management.Automation.OrderedHashtable]){throw 'EXACT_BUILTIN_PARSED_MAP'}
 if($Map.Count -ne $Keys.Length){throw 'EXACT_OBJECT_FIELD_COUNT'}
 $taskSet=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($taskK in $Keys){$null=$taskSet.Add($taskK)}
 foreach($taskK in $Map.Keys){Assert-TaskString $taskK 512;if(-not $taskSet.Contains($taskK)){throw 'EXACT_OBJECT_FIELD_SET'}}
}
function Set-TaskSecondary([int]$Slot,$Raw){
 if($Slot -lt 0 -or $Slot -ge $script:taskSecondaryRaw.Length){throw 'INTERNAL_SECONDARY_SLOT'}
 if($script:taskSecondaryCounts[$Slot] -eq 0){$script:taskSecondaryRaw[$Slot]=$Raw}
 if($script:taskSecondaryCounts[$Slot] -lt [long]::MaxValue){$script:taskSecondaryCounts[$Slot]++}
}
function Close-TaskOwner($Owner){
 if($null -ne $Owner){try{$Owner.Dispose()}catch{if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_}else{Set-TaskSecondary 10 $_}}}
}
function Complete-TaskOwnerClose($PriorPrimary,$BodyRaw){if($null -eq $PriorPrimary -and $null -eq $BodyRaw -and $null -ne $script:taskPrimaryRaw){throw $script:taskPrimaryRaw}}
function Get-TaskSecondaryCodes{
 $taskCodes=[Collections.Generic.List[object]]::new($taskSecondarySites.Length)
 for($taskI=0;$taskI -lt $taskSecondarySites.Length;$taskI++){if($null -ne $taskSecondaryRaw[$taskI]){$taskCodes.Add(@{site=$taskSecondarySites[$taskI];code='FIRST_SECONDARY_RAW_RETAINED_NOT_SERIALIZED';count=$taskSecondaryCounts[$taskI]})}}
 return ,$taskCodes.ToArray()
}
function Assert-TaskCodecBinding{
 if($null -eq $script:taskConvertToJson -or $null -eq $script:taskConvertFromJson){throw 'TRUSTED_CODECS_NOT_BOUND'}
 # Reading this CLR auto-property does not invoke a registered delegate.
 if($null -ne [Newtonsoft.Json.JsonConvert]::DefaultSettings){throw 'JSON_DEFAULT_SETTINGS_DELEGATE_REFUSED'}
}
function Get-TaskIntegerDigits($Value){
 Assert-TaskIntegral $Value
 $taskDigits=1
 if($Value -lt 0){
  $taskDigits++
  foreach($taskThreshold in $script:taskSignedDigitThresholds){if($Value -le -$taskThreshold){$taskDigits++}}
 }else{
  $taskUnsigned=[uint64]$Value
  foreach($taskThreshold in $script:taskUnsignedDigitThresholds){if($taskUnsigned -ge $taskThreshold){$taskDigits++}}
 }
 return $taskDigits
}
function Get-TaskEscapedStringBytes([string]$Value){
 Assert-TaskString $Value $script:taskJsonProfile['string_units']
 $taskBytes=2L
 for($taskI=0;$taskI -lt $Value.Length;$taskI++){
  Spend-Task 1;$taskC=[int]$Value[$taskI]
  if($taskC -eq 34 -or $taskC -eq 92 -or $taskC -eq 8 -or $taskC -eq 9 -or $taskC -eq 10 -or $taskC -eq 12 -or $taskC -eq 13){$taskBytes+=2}
  elseif($taskC -lt 32 -or $taskC -ge 128){$taskBytes+=6}else{$taskBytes++}
  $script:taskJsonStringUnits++
 }
 return $taskBytes
}
function Add-TaskJsonBound($State,[long]$Bytes){if($Bytes -lt 0 -or $Bytes -gt $State['cap']-$State['bytes']){throw 'JSON_PREFLIGHT_BYTE_CAP'};$State['bytes']+=$Bytes}
function Visit-TaskJsonValue($Value,[int]$Depth,$State){
 if($Depth -gt $script:taskJsonProfile['depth']){throw 'JSON_PREFLIGHT_DEPTH'}
 # Every value/key emits at least one byte. The unchanged document byte cap
 # therefore bounds visits even for aliases/cycles; there is no padded node cap.
 if($State['nodes'] -ge $State['cap']){throw 'JSON_PREFLIGHT_FINITE_VISITS'}
 $State['nodes']++;$script:taskJsonPreflightNodes++;Spend-Task 1
 if($null -eq $Value){Add-TaskJsonBound $State 4;return}
 $taskT=Get-TaskRuntimeType $Value
 if($taskT -eq [string]){Add-TaskJsonBound $State (Get-TaskEscapedStringBytes $Value);return}
 if($taskT -eq [bool]){Add-TaskJsonBound $State $(if($Value){4}else{5});return}
 if($taskT -eq [sbyte] -or $taskT -eq [byte] -or $taskT -eq [int16] -or $taskT -eq [uint16] -or $taskT -eq [int32] -or $taskT -eq [uint32] -or $taskT -eq [int64] -or $taskT -eq [uint64]){Add-TaskJsonBound $State (Get-TaskIntegerDigits $Value);return}
 if($taskT -eq [Collections.Hashtable] -or $taskT -eq [Collections.Specialized.OrderedDictionary]){
  if($Value.Count -gt $script:taskJsonProfile['map_entries']){throw 'JSON_PREFLIGHT_MAP_EXTENT'}
  Add-TaskJsonBound $State 2;$taskSeen=0
  # Only exact source-built built-ins reach this branch. Their comparers and
  # enumerators are the actual literal/default ones; no IDictionary extension.
  foreach($taskEntry in $Value.GetEnumerator()){
   $taskSeen++;if($taskSeen -gt $Value.Count){throw 'JSON_MAP_CHANGED_DURING_PREFLIGHT'}
   if($State['nodes'] -ge $State['cap']){throw 'JSON_PREFLIGHT_FINITE_VISITS'};$State['nodes']++;$script:taskJsonPreflightNodes++
   Assert-TaskString $taskEntry.Key $script:taskJsonProfile['string_units']
   Add-TaskJsonBound $State ((Get-TaskEscapedStringBytes $taskEntry.Key)+1)
   if($taskSeen -gt 1){Add-TaskJsonBound $State 1}
   Visit-TaskJsonValue $taskEntry.Value ($Depth+1) $State
  }
  if($taskSeen -ne $Value.Count){throw 'JSON_MAP_CHANGED_DURING_PREFLIGHT'};return
 }
 if($taskT -eq [object[]] -or $taskT -eq [string[]] -or $taskT -eq [Collections.Generic.List[object]] -or $taskT -eq [Collections.Generic.List[string]]){
  $taskCount=$Value.Count;if($taskCount -gt $script:taskJsonProfile['array_entries']){throw 'JSON_PREFLIGHT_ARRAY_EXTENT'}
  Add-TaskJsonBound $State 2
  for($taskI=0;$taskI -lt $taskCount;$taskI++){if($taskI){Add-TaskJsonBound $State 1};Visit-TaskJsonValue $Value[$taskI] ($Depth+1) $State}
  if($Value.Count -ne $taskCount){throw 'JSON_ARRAY_CHANGED_DURING_PREFLIGHT'};return
 }
 throw 'JSON_UNSUPPORTED_VALUE_WITHOUT_STRINGIFICATION'
}
function Measure-TaskJson($Value,[long]$Maximum=1048576){
 Assert-TaskCodecBinding
 if($Maximum -le 0 -or $Maximum -gt 1048576 -or $null -eq $script:taskJsonProfile){throw 'JSON_DOCUMENT_PROFILE'}
 $taskState=@{cap=$Maximum;nodes=0L;bytes=0L}
 Visit-TaskJsonValue $Value 0 $taskState;Add-TaskJsonBound $taskState 1 # final LF
 return @{bytes=$taskState['bytes'];nodes=$taskState['nodes'];codec_preprocess_clone_visits=$taskState['nodes'];source_and_clone_lifetimes='PENDING_INDEPENDENT_PAYLOAD_REVIEW'}
}
function Join-TaskBoundedNames([Collections.Generic.List[string]]$Names,[int]$Maximum){
 if($Names.Count -gt $Maximum -or $Maximum -lt 0){throw 'JOIN_CONTAINER_EXTENT_BEFORE_COPY'}
 $taskLength=0L
 for($taskI=0;$taskI -lt $Names.Count;$taskI++){Assert-TaskString $Names[$taskI] 512;$taskLength+=$Names[$taskI].Length;if($taskI){$taskLength++}}
 $taskUpper=512L*$Maximum+[Math]::Max(0,$Maximum-1)
 if($taskLength -gt $taskUpper -or $taskLength -gt [int]::MaxValue){throw 'JOIN_LENGTH_BEFORE_ALLOCATION'}
 return [string]::Join('|',[Collections.Generic.IEnumerable[string]]$Names)
}
function Get-TaskEntries([string]$Root,[bool]$Recursive,[int]$Maximum,[Collections.Generic.List[object]]$Destination=$null){
 $taskRows=$Destination;if($null -eq $taskRows){$taskRows=[Collections.Generic.List[object]]::new()};$taskPending=[Collections.Generic.Queue[object]]::new();$taskPending.Enqueue(@{path=(Get-TaskCanonicalPath $Root);depth=0});$taskSeen=0
 while($taskPending.Count){$taskNext=$taskPending.Dequeue();$taskDirectory=$taskNext['path'];$taskDepth=[int]$taskNext['depth'];if($taskDepth -gt 64){Reject-TaskGate 'DIRECTORY_DEPTH_PROFILE' @{path=$taskDirectory;depth=$taskDepth;maximum=64}};Assert-NoLink $taskDirectory;$taskIterator=$null;$taskIteratorBefore=$script:taskPrimaryRaw;$taskIteratorBody=$null
   try{$taskIterator=[IO.Directory]::EnumerateFileSystemEntries($taskDirectory).GetEnumerator();while($taskIterator.MoveNext()){$taskCandidate=[string]$taskIterator.Current;Assert-TaskString $taskCandidate 512;$taskPath=Get-TaskCanonicalPath $taskCandidate;$taskSeen++;if($taskSeen -gt $Maximum){Reject-TaskGate 'DIRECTORY_ENTRY_PROFILE' @{root=$Root;maximum=$Maximum;observed=$taskSeen;overflow_path=$taskPath}};Spend-Task 1;if($taskPath.Length -gt 512){throw 'Finite owned/input path length'};$taskAttributes=[IO.File]::GetAttributes($taskPath);$taskDirectoryFlag=($taskAttributes -band [IO.FileAttributes]::Directory)-ne 0;$taskLink=($taskAttributes -band [IO.FileAttributes]::ReparsePoint)-ne 0;$taskLength=if($taskDirectoryFlag -or $taskLink){0L}else{[IO.FileInfo]::new($taskPath).Length};$taskRows.Add([pscustomobject]@{FullName=$taskPath;Name=[IO.Path]::GetFileName($taskPath);PSIsContainer=$taskDirectoryFlag;Length=$taskLength;Reparse=$taskLink});if($taskLink){throw 'Reparse entry refused before traversal'};if($Recursive -and $taskDirectoryFlag){if($taskDepth -ge 64){Reject-TaskGate 'DIRECTORY_DEPTH_PROFILE' @{path=$taskPath;depth=($taskDepth+1);maximum=64}};$taskPending.Enqueue(@{path=$taskPath;depth=($taskDepth+1)})}}}
   catch{$taskIteratorBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}
   finally{if($taskIterator -is [IDisposable]){Close-TaskOwner $taskIterator};Complete-TaskOwnerClose $taskIteratorBefore $taskIteratorBody}
 }
 return $taskRows.ToArray()
}
function Copy-TaskOwned([string]$Source,[string]$Destination,[long]$ExpectedBytes,[long]$Cap){
 if($ExpectedBytes -gt $Cap -or -not $Destination.StartsWith((Join-Path $taskRoot 'controls')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Declared bounded owned control copy'}
 Assert-NoLink $Source;Assert-NoLink $Destination;$taskIn=$null;$taskOut=$null;$taskBuffer=[byte[]]::new(65536);$taskSeen=0L;$taskBefore=$script:taskPrimaryRaw;$taskBody=$null
 try{$taskIn=[IO.FileStream]::new($Source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$taskOut=[IO.FileStream]::new($Destination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);if($taskIn.Length -ne $ExpectedBytes){throw 'Owned source copy length'};while($taskSeen -lt $ExpectedBytes){Spend-Task 128;$taskN=[int][Math]::Min(65536L,$ExpectedBytes-$taskSeen);$taskIn.ReadExactly($taskBuffer,0,$taskN);if($taskN -gt $Cap-$taskSeen){throw 'Owned control before-write cap'};$taskOut.Write($taskBuffer,0,$taskN);$taskSeen+=$taskN};if($taskIn.ReadByte() -ne -1){throw 'Owned source copy EOF'};$taskOut.Flush($true)}catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}finally{Close-TaskOwner $taskOut;Close-TaskOwner $taskIn;Complete-TaskOwnerClose $taskBefore $taskBody}
}
function Mutate-TaskOwnedBytes([string]$Path,[long]$Offset,[byte[]]$Replacement){
 if(-not $Path.StartsWith((Join-Path $taskRoot 'controls')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $Replacement.Length -gt 2){throw 'Bounded owned mutation path/bytes'};$taskFile=$null;$taskBefore=[byte[]]::new($Replacement.Length);$taskPriorPrimary=$script:taskPrimaryRaw;$taskBody=$null
 try{$taskFile=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);if($Offset -lt 0 -or $Offset+$Replacement.Length -gt $taskFile.Length){throw 'Owned mutation coordinate'};$taskFile.Position=$Offset;$taskFile.ReadExactly($taskBefore,0,$taskBefore.Length);$taskFile.Position=$Offset;$taskFile.Write($Replacement,0,$Replacement.Length);$taskFile.Flush($true)}catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}finally{Close-TaskOwner $taskFile;Complete-TaskOwnerClose $taskPriorPrimary $taskBody}
 return @{path=$Path;offset=$Offset;before=[Convert]::ToHexString($taskBefore);after=[Convert]::ToHexString($Replacement)}
}
function Assert-TaskRawJson([string]$Text,[int]$MaximumNodes,[int]$MaximumStringUnits,[int]$MaximumDepth){
 # A finite strict lexer/grammar precedes EVERY bound SDK parser invocation.
 # Frame0 is the document. Array states10/11/12: first/comma/required value;
 # object20/21/22/23/24: first key/colon/value/comma/required key. Parent state
 # advances before a child frame starts; closing a child never re-enters it.
 if($MaximumNodes -le 0 -or $MaximumStringUnits -le 0 -or $MaximumStringUnits -gt 512 -or $MaximumDepth -lt 1 -or $MaximumDepth -gt 24){throw 'JSON_RAW_PROFILE'}
 $taskStates=[byte[]]::new($MaximumDepth+1);$taskLevel=0;$taskNodes=0;$taskAt=0
 while($taskAt -lt $Text.Length){
  $taskC=[int]$Text[$taskAt]
  if($taskC -eq 32 -or $taskC -eq 9 -or $taskC -eq 10 -or $taskC -eq 13){$taskAt++;continue}
  $taskState=$taskStates[$taskLevel]
  if($taskC -eq 58){if($taskState -ne 21){throw 'JSON_RAW_COLON'};$taskStates[$taskLevel]=22;$taskAt++;continue}
  if($taskC -eq 44){if($taskState -eq 11){$taskStates[$taskLevel]=12}elseif($taskState -eq 23){$taskStates[$taskLevel]=24}else{throw 'JSON_RAW_COMMA'};$taskAt++;continue}
  if($taskC -eq 93 -or $taskC -eq 125){
   if($taskLevel -eq 0 -or ($taskC -eq 93 -and $taskState -ne 10 -and $taskState -ne 11) -or ($taskC -eq 125 -and $taskState -ne 20 -and $taskState -ne 23)){throw 'JSON_RAW_CONTAINER_END'}
   $taskLevel--;$taskAt++;continue
  }
  $taskKey=($taskC -eq 34 -and ($taskState -eq 20 -or $taskState -eq 24))
  if(-not $taskKey){
   if($taskState -eq 0){$taskStates[$taskLevel]=1}elseif($taskState -eq 10 -or $taskState -eq 12){$taskStates[$taskLevel]=11}elseif($taskState -eq 22){$taskStates[$taskLevel]=23}else{throw 'JSON_RAW_VALUE_POSITION'}
  }
  $taskNodes++;if($taskNodes -gt $MaximumNodes){throw 'JSON_RAW_NODE_PROFILE'}
  if($taskC -eq 34){
   $taskAt++;$taskStringUnits=0;$taskHigh=$false;$taskClosed=$false
   while($taskAt -lt $Text.Length){
    $taskUnit=[int]$Text[$taskAt];$taskAt++
    if($taskUnit -eq 34){if($taskHigh){throw 'JSON_RAW_UNPAIRED_SURROGATE'};$taskClosed=$true;break}
    if($taskUnit -lt 32){throw 'JSON_RAW_STRING_CONTROL'}
    if($taskUnit -eq 92){
     if($taskAt -ge $Text.Length){throw 'JSON_RAW_ESCAPE_END'};$taskUnit=[int]$Text[$taskAt];$taskAt++
     if($taskUnit -eq 117){
      $taskUnit=0
      for($taskHexAt=0;$taskHexAt -lt 4;$taskHexAt++){
       if($taskAt -ge $Text.Length){throw 'JSON_RAW_UNICODE_END'};$taskHex=[int]$Text[$taskAt];$taskAt++
       if($taskHex -ge 48 -and $taskHex -le 57){$taskDigit=$taskHex-48}elseif($taskHex -ge 65 -and $taskHex -le 70){$taskDigit=$taskHex-55}elseif($taskHex -ge 97 -and $taskHex -le 102){$taskDigit=$taskHex-87}else{throw 'JSON_RAW_UNICODE_HEX'}
       $taskUnit=$taskUnit*16+$taskDigit
      }
     }elseif($taskUnit -ne 34 -and $taskUnit -ne 92 -and $taskUnit -ne 47 -and $taskUnit -ne 98 -and $taskUnit -ne 102 -and $taskUnit -ne 110 -and $taskUnit -ne 114 -and $taskUnit -ne 116){throw 'JSON_RAW_ESCAPE_KIND'}
    }
    if($taskHigh){if($taskUnit -lt 56320 -or $taskUnit -gt 57343){throw 'JSON_RAW_UNPAIRED_SURROGATE'};$taskHigh=$false}elseif($taskUnit -ge 55296 -and $taskUnit -le 56319){$taskHigh=$true}elseif($taskUnit -ge 56320 -and $taskUnit -le 57343){throw 'JSON_RAW_UNPAIRED_SURROGATE'}
    $taskStringUnits++;if($taskStringUnits -gt $MaximumStringUnits){throw 'JSON_RAW_STRING_PROFILE'}
   }
   if(-not $taskClosed){throw 'JSON_RAW_UNTERMINATED_STRING'}
   if($taskKey){$taskStates[$taskLevel]=21};continue
  }
  if($taskC -eq 123 -or $taskC -eq 91){
   if($taskLevel -ge $MaximumDepth){throw 'JSON_RAW_DEPTH_PROFILE'};$taskLevel++;$taskStates[$taskLevel]=$(if($taskC -eq 123){20}else{10});$taskAt++;continue
  }
  if($taskC -eq 116 -or $taskC -eq 102 -or $taskC -eq 110){
   $taskLiteral=if($taskC -eq 116){'true'}elseif($taskC -eq 102){'false'}else{'null'}
   for($taskLiteralAt=0;$taskLiteralAt -lt $taskLiteral.Length;$taskLiteralAt++){if($taskAt -ge $Text.Length -or $Text[$taskAt] -cne $taskLiteral[$taskLiteralAt]){throw 'JSON_RAW_LITERAL'};$taskAt++};continue
  }
  # The supported fields are builtin decimal integers, Int64.MinValue through
  # UInt64.MaxValue. Count/sign/range are checked without a token Substring,
  # floating arithmetic, BigInteger construction or untrusted stringification.
  $taskStart=$taskAt;$taskNegative=($taskC -eq 45)
  if($taskNegative){$taskAt++}
  $taskDigitsStart=$taskAt
  if($taskAt -ge $Text.Length -or [int]$Text[$taskAt] -lt 48 -or [int]$Text[$taskAt] -gt 57){throw 'JSON_RAW_INTEGER_DIGIT'}
  $taskLeadingZero=([int]$Text[$taskAt] -eq 48)
  while($taskAt -lt $Text.Length -and [int]$Text[$taskAt] -ge 48 -and [int]$Text[$taskAt] -le 57){
   $taskAt++;$taskScalarUnits=$taskAt-$taskStart
   if($taskScalarUnits -gt 20){Reject-TaskGate 'JSON_RAW_SCALAR_UNITS' @{observed=$taskScalarUnits;maximum=20}}
  }
  $taskDigits=$taskAt-$taskDigitsStart
  if($taskLeadingZero -and $taskDigits -ne 1){throw 'JSON_RAW_INTEGER_LEADING_ZERO'}
  $taskMagnitudeLimit=if($taskNegative){'9223372036854775808'}else{'18446744073709551615'}
  if($taskDigits -gt $taskMagnitudeLimit.Length){throw 'JSON_RAW_INTEGER_RANGE'}
  if($taskDigits -eq $taskMagnitudeLimit.Length){
   for($taskCompareAt=0;$taskCompareAt -lt $taskDigits;$taskCompareAt++){
    $taskDigit=[int]$Text[$taskDigitsStart+$taskCompareAt];$taskLimitDigit=[int]$taskMagnitudeLimit[$taskCompareAt]
    if($taskDigit -gt $taskLimitDigit){throw 'JSON_RAW_INTEGER_RANGE'};if($taskDigit -lt $taskLimitDigit){break}
   }
  }
 }
 if($taskLevel -ne 0 -or $taskStates[0] -ne 1){throw 'JSON_RAW_INCOMPLETE_DOCUMENT'}
}
function Assert-TaskRawJsonBytes([byte[]]$Bytes,[int]$MaximumNodes,[int]$MaximumStringUnits,[int]$MaximumDepth){
 # Strict UTF8 validates original bytes without allocating decoded text.
 # Complete original-token grammar succeeds BEFORE any in-place rewrite.
 # Frame0 is the document. Array states10/11/12: first/comma/required value;
 # object20/21/22/23/24: first key/colon/value/comma/required key. Parent state
 # advances before a child frame starts; closing a child never re-enters it.
 if($MaximumNodes -le 0 -or $MaximumStringUnits -le 0 -or $MaximumStringUnits -gt 512 -or $MaximumDepth -lt 1 -or $MaximumDepth -gt 24){throw 'JSON_RAW_PROFILE'}
 if($Bytes.Length -gt 4194304){throw 'JSON byte cap before allocation'}
 $taskDecodedUnits=[Text.UTF8Encoding]::new($false,$true).GetCharCount($Bytes,0,$Bytes.Length)
 $taskStates=[byte[]]::new($MaximumDepth+1);$taskLevel=0;$taskNodes=0;$taskAt=0
 while($taskAt -lt $Bytes.Length){
  $taskC=[int]$Bytes[$taskAt]
  if($taskC -eq 32 -or $taskC -eq 9 -or $taskC -eq 10 -or $taskC -eq 13){$taskAt++;continue}
  $taskState=$taskStates[$taskLevel]
  if($taskC -eq 58){if($taskState -ne 21){throw 'JSON_RAW_COLON'};$taskStates[$taskLevel]=22;$taskAt++;continue}
  if($taskC -eq 44){if($taskState -eq 11){$taskStates[$taskLevel]=12}elseif($taskState -eq 23){$taskStates[$taskLevel]=24}else{throw 'JSON_RAW_COMMA'};$taskAt++;continue}
  if($taskC -eq 93 -or $taskC -eq 125){
   if($taskLevel -eq 0 -or ($taskC -eq 93 -and $taskState -ne 10 -and $taskState -ne 11) -or ($taskC -eq 125 -and $taskState -ne 20 -and $taskState -ne 23)){throw 'JSON_RAW_CONTAINER_END'}
   $taskLevel--;$taskAt++;continue
  }
  $taskKey=($taskC -eq 34 -and ($taskState -eq 20 -or $taskState -eq 24))
  if(-not $taskKey){
   if($taskState -eq 0){$taskStates[$taskLevel]=1}elseif($taskState -eq 10 -or $taskState -eq 12){$taskStates[$taskLevel]=11}elseif($taskState -eq 22){$taskStates[$taskLevel]=23}else{throw 'JSON_RAW_VALUE_POSITION'}
  }
  $taskNodes++;if($taskNodes -gt $MaximumNodes){throw 'JSON_RAW_NODE_PROFILE'}
  if($taskC -eq 34){
   $taskAt++;$taskStringUnits=0;$taskHigh=$false;$taskClosed=$false
   while($taskAt -lt $Bytes.Length){
    $taskUnit=[int]$Bytes[$taskAt];$taskAt++
    if($taskUnit -eq 34){if($taskHigh){throw 'JSON_RAW_UNPAIRED_SURROGATE'};$taskClosed=$true;break}
    if($taskUnit -lt 32){throw 'JSON_RAW_STRING_CONTROL'}
    # UTF8 was strictly validated as a whole. A raw leading sequence is one
    # scalar: two/three bytes => one UTF16 unit, four => a surrogate pair.
    # Escaped Unicode is handled below and must NOT enter this byte branch.
    $taskUnitCount=1
    if($taskUnit -ge 128){
     if($taskHigh){throw 'JSON_RAW_UNPAIRED_SURROGATE'}
     if($taskUnit -ge 240){$taskAt+=3;$taskUnitCount=2}elseif($taskUnit -ge 224){$taskAt+=2}else{$taskAt++}
     $taskUnit=0
    }
    if($taskUnit -eq 92){
     if($taskAt -ge $Bytes.Length){throw 'JSON_RAW_ESCAPE_END'};$taskUnit=[int]$Bytes[$taskAt];$taskAt++
     if($taskUnit -eq 117){
      $taskUnit=0
      for($taskHexAt=0;$taskHexAt -lt 4;$taskHexAt++){
       if($taskAt -ge $Bytes.Length){throw 'JSON_RAW_UNICODE_END'};$taskHex=[int]$Bytes[$taskAt];$taskAt++
       if($taskHex -ge 48 -and $taskHex -le 57){$taskDigit=$taskHex-48}elseif($taskHex -ge 65 -and $taskHex -le 70){$taskDigit=$taskHex-55}elseif($taskHex -ge 97 -and $taskHex -le 102){$taskDigit=$taskHex-87}else{throw 'JSON_RAW_UNICODE_HEX'}
       $taskUnit=$taskUnit*16+$taskDigit
      }
     }elseif($taskUnit -ne 34 -and $taskUnit -ne 92 -and $taskUnit -ne 47 -and $taskUnit -ne 98 -and $taskUnit -ne 102 -and $taskUnit -ne 110 -and $taskUnit -ne 114 -and $taskUnit -ne 116){throw 'JSON_RAW_ESCAPE_KIND'}
    }
    if($taskHigh){if($taskUnit -lt 56320 -or $taskUnit -gt 57343){throw 'JSON_RAW_UNPAIRED_SURROGATE'};$taskHigh=$false}elseif($taskUnit -ge 55296 -and $taskUnit -le 56319){$taskHigh=$true}elseif($taskUnit -ge 56320 -and $taskUnit -le 57343){throw 'JSON_RAW_UNPAIRED_SURROGATE'}
    $taskStringUnits+=$taskUnitCount;if($taskStringUnits -gt $MaximumStringUnits){throw 'JSON_RAW_STRING_PROFILE'}
   }
   if(-not $taskClosed){throw 'JSON_RAW_UNTERMINATED_STRING'}
   if($taskKey){$taskStates[$taskLevel]=21};continue
  }
  if($taskC -eq 123 -or $taskC -eq 91){
   if($taskLevel -ge $MaximumDepth){throw 'JSON_RAW_DEPTH_PROFILE'};$taskLevel++;$taskStates[$taskLevel]=$(if($taskC -eq 123){20}else{10});$taskAt++;continue
  }
  if($taskC -eq 116 -or $taskC -eq 102 -or $taskC -eq 110){
   $taskLiteral=if($taskC -eq 116){'true'}elseif($taskC -eq 102){'false'}else{'null'}
   for($taskLiteralAt=0;$taskLiteralAt -lt $taskLiteral.Length;$taskLiteralAt++){if($taskAt -ge $Bytes.Length -or $Bytes[$taskAt] -ne [int]$taskLiteral[$taskLiteralAt]){throw 'JSON_RAW_LITERAL'};$taskAt++};continue
  }
  # The supported fields are builtin decimal integers, Int64.MinValue through
  # UInt64.MaxValue. Count/sign/range are checked without a token Substring,
  # floating arithmetic, BigInteger construction or untrusted stringification.
  $taskStart=$taskAt;$taskNegative=($taskC -eq 45)
  if($taskNegative){$taskAt++}
  $taskDigitsStart=$taskAt
  if($taskAt -ge $Bytes.Length -or [int]$Bytes[$taskAt] -lt 48 -or [int]$Bytes[$taskAt] -gt 57){throw 'JSON_RAW_INTEGER_DIGIT'}
  $taskLeadingZero=([int]$Bytes[$taskAt] -eq 48)
  while($taskAt -lt $Bytes.Length -and [int]$Bytes[$taskAt] -ge 48 -and [int]$Bytes[$taskAt] -le 57){
   $taskAt++;$taskScalarUnits=$taskAt-$taskStart
   if($taskScalarUnits -gt 20){Reject-TaskGate 'JSON_RAW_SCALAR_UNITS' @{observed=$taskScalarUnits;maximum=20}}
  }
  $taskDigits=$taskAt-$taskDigitsStart
  if($taskLeadingZero -and $taskDigits -ne 1){throw 'JSON_RAW_INTEGER_LEADING_ZERO'}
  $taskMagnitudeLimit=if($taskNegative){'9223372036854775808'}else{'18446744073709551615'}
  if($taskDigits -gt $taskMagnitudeLimit.Length){throw 'JSON_RAW_INTEGER_RANGE'}
  if($taskDigits -eq $taskMagnitudeLimit.Length){
   for($taskCompareAt=0;$taskCompareAt -lt $taskDigits;$taskCompareAt++){
    $taskDigit=[int]$Bytes[$taskDigitsStart+$taskCompareAt];$taskLimitDigit=[int]$taskMagnitudeLimit[$taskCompareAt]
    if($taskDigit -gt $taskLimitDigit){throw 'JSON_RAW_INTEGER_RANGE'};if($taskDigit -lt $taskLimitDigit){break}
   }
  }
 }
 if($taskLevel -ne 0 -or $taskStates[0] -ne 1){throw 'JSON_RAW_INCOMPLETE_DOCUMENT'}
 # Acceptance above used ORIGINAL token boundaries. Only now compact JSON
 # whitespace outside strings. Read>=write, so unread bytes are never changed.
 $taskWrite=0;$taskQuoted=$false;$taskEscaped=$false
 for($taskRead=0;$taskRead -lt $Bytes.Length;$taskRead++){
  $taskByte=$Bytes[$taskRead]
  if($taskQuoted){
   $Bytes[$taskWrite]=$taskByte;$taskWrite++
   if($taskEscaped){$taskEscaped=$false}elseif($taskByte -eq 92){$taskEscaped=$true}elseif($taskByte -eq 34){$taskQuoted=$false}
  }elseif($taskByte -eq 32 -or $taskByte -eq 9 -or $taskByte -eq 10 -or $taskByte -eq 13){continue}else{
   $Bytes[$taskWrite]=$taskByte;$taskWrite++;if($taskByte -eq 34){$taskQuoted=$true}
  }
 }
 return $taskWrite
}
function Read-TaskBoundedBytes([string]$Path,[long]$Maximum){
 # Stat and opened length are admitted BEFORE any length-sized allocation.
 $taskLength=[IO.FileInfo]::new($Path).Length;if($taskLength -lt 0 -or $taskLength -gt $Maximum -or $Maximum -gt 4194304){throw 'JSON byte cap before allocation'};$taskFile=$null;$taskBefore=$script:taskPrimaryRaw;$taskBody=$null
 try{$taskFile=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,1,[IO.FileOptions]::SequentialScan);if($taskFile.Length -ne $taskLength){throw 'JSON length changed before bounded allocation'};$taskBytes=[byte[]]::new([int]$taskLength);$taskFile.ReadExactly($taskBytes,0,$taskBytes.Length);if($taskFile.ReadByte() -ne -1){throw 'JSON exact EOF'};return ,$taskBytes}catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}finally{Close-TaskOwner $taskFile;Complete-TaskOwnerClose $taskBefore $taskBody}
}
function Read-TaskJson([string]$Path,[long]$Maximum=1048576,[string]$ExpectedSha256=$null){
 Spend-Task 64;Assert-TaskCodecBinding;$taskBytes=Read-TaskBoundedBytes $Path $Maximum
 if($Maximum -eq 4194304){
  if($ExpectedSha256.Length -ne 64 -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($taskBytes)) -cne $ExpectedSha256){throw 'DEPENDENCY_CAPTURED_BYTE_IDENTITY_BEFORE_PARSE'}
 }elseif($ExpectedSha256 -cne '28909B793046C026890456FA59A841DA2B75450B60AAD5E8DD3888DFCCB23E71' -or [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($taskBytes)) -cne $ExpectedSha256){throw 'PREDECESSOR_CAPTURED_BYTE_IDENTITY_BEFORE_PARSE'}
 Add-TaskJsonVisit $taskBytes.Length
 if($Maximum -eq 4194304){$taskCompactLength=Assert-TaskRawJsonBytes $taskBytes (2*[int]$taskManifest['profile']['include_input_count']+256) 512 24}else{$taskCompactLength=Assert-TaskRawJsonBytes $taskBytes 2420 270 3}
 $taskRaw=[Text.UTF8Encoding]::new($false,$true).GetString($taskBytes,0,$taskCompactLength);$taskBytes=$null
 return & $script:taskConvertFromJson -InputObject $taskRaw -AsHashtable -Depth 24 -DateKind String
}
function Encode-TaskJson($Value,[long]$Maximum=1048576,$Measurement=$null){
 Assert-TaskCodecBinding
 if($null -eq $Measurement){$Measurement=Measure-TaskJson $Value $Maximum}
 if($Measurement['bytes'] -le 0 -or $Measurement['bytes'] -gt $Maximum -or $Maximum -gt 1048576){throw 'ADMITTED_JSON_LENGTH_BEFORE_CODEC'}
 # The entire private object graph has already been admitted. It is not
 # mutated between measurement and this exact bound cmdlet invocation.
 $taskText=& $script:taskConvertToJson -InputObject $Value -Depth 24 -Compress -EscapeHandling EscapeNonAscii
 if((Get-TaskRuntimeType $taskText) -ne [string] -or $taskText.Length -ne $Measurement['bytes']-1){throw 'EXACT_PREFLIGHT_SERIALIZED_LENGTH'}
 for($taskI=0;$taskI -lt $taskText.Length;$taskI++){if([int]$taskText[$taskI] -gt 127 -or [int]$taskText[$taskI] -eq 10 -or [int]$taskText[$taskI] -eq 13){throw 'ESCAPE_NONASCII_COMPRESSED_RESULT'}}
 $taskBytes=[Text.UTF8Encoding]::new($false,$true).GetBytes($taskText+"`n")
 if($taskBytes.Length -ne $Measurement['bytes'] -or $taskBytes.Length -gt $Maximum){throw 'ACTUAL_ENCODED_LENGTH_AND_CAP'}
 Add-TaskJsonVisit $taskBytes.Length;$script:taskJsonEncodes++
 return ,$taskBytes
}
function Get-TaskStageBytes{
 $taskSum=0L;if([IO.Directory]::Exists($taskStageRoot)){foreach($taskF in (Get-TaskEntries $taskStageRoot $false 7)){Spend-Task 1;if($taskF.PSIsContainer -or $taskF.Name -cnotin @('stdout.log','stderr.log','receipt.json','bundle.json','consumed.json','failure-inventory.json')){throw 'Unlisted controlled stage artifact'};if($taskF.Length -gt $taskStageCap-$taskSum){throw 'Controlled stage aggregate'};$taskSum+=$taskF.Length}};return $taskSum
}
function Assert-TaskDocumentWrite([string]$Path,[long]$Length){
 Assert-TaskString $Path 512
 $taskOld=if([IO.File]::Exists($Path)){[IO.FileInfo]::new($Path).Length}else{0L}
 if($Length -lt 0 -or $Length -gt $taskStageCap-((Get-TaskStageBytes)-$taskOld)){throw 'STAGE_POOL_BEFORE_ENCODING_AND_WRITE'}
}
function Write-TaskJsonBytes([string]$Path,[byte[]]$Bytes,[long]$Maximum){
 if($Bytes.Length -gt $Maximum -or $Maximum -gt 1048576){throw 'ENCODED_DOCUMENT_CAP'}
 Assert-TaskDocumentWrite $Path $Bytes.Length;[IO.File]::WriteAllBytes($Path,$Bytes)
}
function Write-TaskJson([string]$Path,$Value,[long]$Maximum=1048576,$Measurement=$null){
 if($null -eq $Measurement){$Measurement=Measure-TaskJson $Value $Maximum}
 Assert-TaskDocumentWrite $Path $Measurement['bytes']
 $taskRaw=Encode-TaskJson $Value $Maximum $Measurement
 Write-TaskJsonBytes $Path $taskRaw $Maximum
}
function Save-TaskReceipt{
 if($script:taskReceiptWrites -ge 12){throw 'Twelve bounded receipt writes'};$script:taskReceiptWrites++
 $taskReceipt['secondary_failures']=Get-TaskSecondaryCodes
 $taskPath=Join-Path $taskStageRoot 'receipt.json'
 $taskMeasure=Measure-TaskJson $taskReceipt 65536
 Assert-TaskDocumentWrite $taskPath $taskMeasure['bytes']
 $taskEncoded=Encode-TaskJson $taskReceipt 65536 $taskMeasure
 Write-TaskJsonBytes $taskPath $taskEncoded 65536
}
function Assert-TaskIdentity($Expected,$Observed){if($Expected['bytes'] -ne $Observed['bytes'] -or $Expected['sha256'] -cne $Observed['sha256']){Reject-TaskGate 'BEFORE_AFTER_COMPLETE_INPUT_IDENTITY' @{path=$Expected['path'];expected_bytes=$Expected['bytes'];observed_bytes=$Observed['bytes'];expected_hash=$Expected['sha256'];observed_hash=$Observed['sha256']}}}
function Assert-TaskBoundSourceInteger([string]$Field,$Value){
 # The value parameter is untyped: no input coercion precedes this gate.
 $taskAllowed=($Value -is [SByte] -or $Value -is [Int16] -or $Value -is [Int32] -or $Value -is [Int64])
 $taskType=if($null -eq $Value){'<null>'}else{(Get-TaskRuntimeType $Value).FullName}
 if(-not $taskAllowed){Reject-TaskGate 'EXACT_SIGNED_SOURCE_INTEGER_KIND' @{field=$Field;observed_type=$taskType}}
 # This is only signed cross-language representability. No new research work
 # ceiling is selected: the positive whole remains unbound Catalogue data.
 if($Value -le 0){Reject-TaskGate 'POSITIVE_SOURCE_INTEGER_REPRESENTATION_DOMAIN' @{field=$Field;observed_type=$taskType;observed_value=$Value;minimum=1L;representation_max=[long]::MaxValue}}
}
function Assert-TaskReservation([long]$Total,[long]$Limit){if($null -eq $script:taskReviewedWorkLimit){throw 'UNBOUND_RESEARCH_LIMIT'};if($Total -gt $Limit -or $Total -lt 0 -or $Limit -gt $script:taskReviewedWorkLimit){Reject-TaskGate 'WHOLE_STAGE_WORK_RESERVATION' @{total=$Total;limit=$Limit;global=$script:taskReviewedWorkLimit}}}
function Assert-TaskRetainedReservation([long]$Total,[long]$Limit){if($Total -gt $Limit -or $Total -lt 0 -or $Limit -gt 880803840L){Reject-TaskGate 'WHOLE_STAGE_RETAINED_RESERVATION' @{total=$Total;limit=$Limit;global=880803840L}}}
function Assert-TaskAttempt([bool]$Exists,[string]$Path){if($Exists){Reject-TaskGate 'ONE_ATTEMPT_ALREADY_CONSUMED' @{path=$Path;exists=$Exists}}}
function Assert-TaskPredecessor($Bundle){
 $taskT=Get-TaskRuntimeType $Bundle;if($taskT -ne [System.Management.Automation.OrderedHashtable] -and $taskT -ne [Collections.Hashtable]){throw 'PREDECESSOR_MAP_KIND'}
 if((Get-TaskRuntimeType $Bundle['artifacts']) -ne [object[]] -or (Get-TaskRuntimeType $Bundle['transitive_inputs']) -ne [object[]]){throw 'PREDECESSOR_ARRAY_KINDS'}
 Assert-TaskString $Bundle['source_manifest_sha256'] 64 $true
 if($Bundle['artifacts'].Count -ne 96 -or $Bundle['transitive_inputs'].Count -ne 147 -or $Bundle['source_manifest_sha256'] -cne '6822A96025D6EEADD79642AB50B0A30224A35429B0577D3246BBC173AC630E88'){Reject-TaskGate 'EXACT_R4_PREDECESSOR' @{artifacts=$Bundle['artifacts'].Count;transitive=$Bundle['transitive_inputs'].Count;source_manifest=$Bundle['source_manifest_sha256']}}
}
function Get-TaskPredecessorDescriptor($Parsed){
 Assert-TaskExactKeys $Parsed @('path','sha256','artifacts','transitive','complete_union_paths','complete_union_bytes')
 Assert-TaskString $Parsed['path'] 512;Assert-TaskString $Parsed['sha256'] 64 $true
 foreach($taskK in @('artifacts','transitive','complete_union_paths','complete_union_bytes')){Assert-TaskIntegral $Parsed[$taskK] $true}
 if($Parsed['sha256'] -cne '28909B793046C026890456FA59A841DA2B75450B60AAD5E8DD3888DFCCB23E71' -or $Parsed['artifacts'] -ne 96 -or $Parsed['transitive'] -ne 147 -or $Parsed['complete_union_paths'] -ne 244 -or $Parsed['complete_union_bytes'] -ne 74499159){throw 'EXACT_PREDECESSOR_DESCRIPTOR'}
 return @{path=$Parsed['path'];sha256=$Parsed['sha256'];artifacts=$Parsed['artifacts'];transitive=$Parsed['transitive'];complete_union_paths=$Parsed['complete_union_paths'];complete_union_bytes=$Parsed['complete_union_bytes']}
}
function Resolve-TaskInput([string]$Name){
 Assert-TaskString $Name 512
 if([IO.Path]::IsPathRooted($Name)){
  if(-not [IO.Path]::IsPathFullyQualified($Name)){throw 'FULLY_QUALIFIED_PINNED_INPUT'}
  if((Test-TaskPathCatalogueContext) -and $script:taskPathCatalogue['inputs'].ContainsKey($Name)){return Get-TaskCatalogueEntryPath $script:taskPathCatalogue['inputs'][$Name]}
  return [IO.Path]::GetFullPath($Name)
 }
 if($taskRepo.Length+1+$Name.Length -gt 512){throw 'DERIVED_INPUT_PATH_BEFORE_JOIN'}
 if((Test-TaskPathCatalogueContext) -and $script:taskPathCatalogue['inputs'].ContainsKey($Name)){return Get-TaskCatalogueEntryPath $script:taskPathCatalogue['inputs'][$Name]}
 return [IO.Path]::GetFullPath((Join-Path $taskRepo $Name))
}
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
 if($Observed.Count -gt $script:taskJsonProfile['array_entries'] -or $Expected.Count -gt $script:taskJsonProfile['array_entries']){throw 'FINITE_FILE_SET_EXTENTS'}
 # Build the ordinal expected set ONCE. No per-observed linear -cnotin scan.
 $taskExpectedSet=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);foreach($taskName in $Expected){Assert-TaskString $taskName 512;if(-not $taskExpectedSet.Add($taskName)){throw 'Duplicate declared expected file name'}}
 $taskSet=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal);$taskDuplicate='';$taskUnknown=[Collections.Generic.List[string]]::new();$taskMissing=[Collections.Generic.List[string]]::new()
 foreach($taskName in $Observed){Assert-TaskString $taskName 512;if(-not $taskSet.Add($taskName)){$taskDuplicate=$taskName};if(-not $taskExpectedSet.Contains($taskName)){$taskUnknown.Add($taskName)}}
 foreach($taskName in $Expected){if(-not $taskSet.Contains($taskName)){$taskMissing.Add($taskName)}}
 if($Observed.Count -ne $Expected.Count -or $taskMissing.Count -or $taskUnknown.Count -or $taskDuplicate){Reject-TaskGate 'COMPLETE_EXACT_REFERENCE_FILE_SET' @{observed=$Observed.Count;expected=$Expected.Count;missing=(Join-TaskBoundedNames $taskMissing $Expected.Count);unknown=(Join-TaskBoundedNames $taskUnknown $Observed.Count);duplicate=$taskDuplicate}}
}
function Get-TaskReferences{
 $taskRows=[Collections.Generic.List[object]]::new();$taskNames=[Collections.Generic.List[string]]::new();$taskBytes=0L
 foreach($taskF in (Get-TaskEntries $taskDataRoot $true 60)){Spend-Task 2;Assert-NoLink $taskF.FullName;if($taskF.PSIsContainer){continue};$taskRelative=[IO.Path]::GetRelativePath($taskDataRoot,$taskF.FullName).Replace([char]92,[char]47);$taskNames.Add($taskRelative);if(-not $taskCaps.ContainsKey($taskRelative)){throw 'Unknown reference output before admission'};$taskH=Get-TaskHash $taskF.FullName $taskCaps[$taskRelative];if($taskH['bytes'] -gt 152627224L-$taskBytes){throw 'Complete reference pool cap'};$taskBytes+=$taskH['bytes'];$taskRows.Add(@{path=$taskF.FullName;relative_path=$taskRelative;bytes=$taskH['bytes'];sha256=$taskH['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 Assert-TaskFileSet $taskNames.ToArray() @($taskCaps.Keys);return @{files=$taskRows.ToArray();bytes=$taskBytes;names=$taskNames.ToArray()}
}
function Assert-TaskSameReferences($Before,$After){if($Before['bytes'] -ne $After['bytes']){throw 'Generated reference byte sum changed'};$taskOld=@{};foreach($taskRow in $Before['files']){$taskOld[$taskRow['relative_path']]=$taskRow};foreach($taskRow in $After['files']){Assert-TaskIdentity $taskOld[$taskRow['relative_path']] $taskRow}}
function Get-TaskPEHeader([string]$Path){$taskItem=Get-Item -LiteralPath $Path;if($taskItem.Length -gt 33554432 -or $taskItem.Length -lt 88){throw 'PE file size'};$taskFile=$null;$taskBefore=$script:taskPrimaryRaw;$taskBody=$null
 try{$taskFile=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);$taskDos=[byte[]]::new(64);$taskFile.ReadExactly($taskDos,0,$taskDos.Length);$taskOffset=[BitConverter]::ToUInt32($taskDos,60);if($taskDos[0] -ne 77 -or $taskDos[1] -ne 90 -or $taskOffset -gt $taskItem.Length-24){throw 'Actual PE DOS/offset'};$taskFile.Position=$taskOffset;$taskHeader=[byte[]]::new(24);$taskFile.ReadExactly($taskHeader,0,$taskHeader.Length);$taskOptional=[BitConverter]::ToUInt16($taskHeader,20);if($taskOptional -lt 70 -or $taskOptional -gt 4096 -or $taskOffset+24+$taskOptional -gt $taskItem.Length){throw 'Actual PE optional size'};$taskOpt=[byte[]]::new($taskOptional);$taskFile.ReadExactly($taskOpt,0,$taskOpt.Length);return @{signature=[BitConverter]::ToUInt32($taskHeader,0);machine=[BitConverter]::ToUInt16($taskHeader,4);magic=[BitConverter]::ToUInt16($taskOpt,0);subsystem=[BitConverter]::ToUInt16($taskOpt,68);path=$Path;offset=$taskOffset;optional_bytes=$taskOptional}}
 catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}
 finally{Close-TaskOwner $taskFile;Complete-TaskOwnerClose $taskBefore $taskBody}
}
function Assert-TaskConsolePE($PE){if($PE['signature'] -ne 17744 -or $PE['machine'] -ne 34404 -or $PE['magic'] -ne 523 -or $PE['subsystem'] -ne 3){Reject-TaskGate 'ACTUAL_PE_CONSOLE_BEFORE_LAUNCH' @{signature=$PE['signature'];machine=$PE['machine'];magic=$PE['magic'];subsystem=$PE['subsystem']}}}
function Read-TaskChildResult($Child){
 Assert-TaskCodecBinding;$taskOut=Join-Path $taskStageRoot 'stdout.log';$taskFile=$null;$taskBefore=$script:taskPrimaryRaw;$taskBody=$null
 try{
  $taskFile=[IO.File]::OpenRead($taskOut);$taskFile.Position=$Child['stdout_start'];$taskLength=$Child['raw_stdout_bytes']
  if($taskLength -lt 0 -or $taskLength -gt 65536){throw 'Module JSON/control output cap'}
  Add-TaskJsonVisit $taskLength;$taskBuffer=[byte[]]::new([int]$taskLength);$taskFile.ReadExactly($taskBuffer,0,$taskBuffer.Length)
  $taskText=[Text.UTF8Encoding]::new($false,$true).GetString($taskBuffer)
  # A real bounded scan replaces Split/filter. Admit <=33 nonempty slices
  # BEFORE each Substring/list copy; visit every original UTF16 code unit.
  $taskLines=[Collections.Generic.List[string]]::new(33);$taskStart=0
  for($taskI=0;$taskI -le $taskText.Length;$taskI++){
   if($taskI -eq $taskText.Length -or $taskText[$taskI] -ceq "`n"){
    if($taskI -gt $taskStart){if($taskLines.Count -ge 33){throw 'Missing/bounded child result'};$taskLines.Add($taskText.Substring($taskStart,$taskI-$taskStart))}
    $taskStart=$taskI+1
   }
  }
  if($taskLines.Count -eq 0){throw 'Missing/bounded child result'}
  $taskMode=if($Child['name'] -ceq 'oracle-produce'){'produce'}else{'verify'}
  $taskExpected=$taskManifest['module_controls'][$taskMode]
  $taskRecords=[Collections.Generic.List[object]]::new($taskExpected.Count);$taskIds=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
  for($taskLine=0;$taskLine -lt $taskLines.Count-1;$taskLine++){
   Assert-TaskRawJson $taskLines[$taskLine] 13 128 2
   $taskParsed=& $script:taskConvertFromJson -InputObject $taskLines[$taskLine] -AsHashtable -Depth 16 -DateKind String
   Assert-TaskExactKeys $taskParsed @('control','gate','operands')
   Assert-TaskString $taskParsed['control'] 64 $true;Assert-TaskString $taskParsed['gate'] 128 $true
   if(-not $taskExpected.Contains($taskParsed['control']) -or -not $taskIds.Add($taskParsed['control'])){throw 'EXACT_MODULE_CONTROL_ID_SET'}
   if($taskParsed['gate'] -cne $taskExpected[$taskParsed['control']]){throw 'Actual module exact rejection reason'}
   if((Get-TaskRuntimeType $taskParsed['operands']) -ne [object[]] -or $taskParsed['operands'].Length -ne 6){throw 'EXACT_SIX_CONTROL_OPERANDS'}
   $taskOperands=[object[]]::new(6)
   for($taskOperand=0;$taskOperand -lt 6;$taskOperand++){$taskOperands[$taskOperand]=(Convert-TaskControlOperand $taskParsed['operands'][$taskOperand])}
   $taskRecords.Add(@{control=$taskParsed['control'];gate=$taskParsed['gate'];operands=$taskOperands})
  }
  Assert-TaskFileSet @($taskIds) @($taskExpected.Keys)
  Assert-TaskRawJson $taskLines[$taskLines.Count-1] 25 128 1
  $taskParsed=& $script:taskConvertFromJson -InputObject $taskLines[$taskLines.Count-1] -AsHashtable -Depth 16 -DateKind String
  Assert-TaskExactKeys $taskParsed $taskSummaryFields
  Assert-TaskString $taskParsed['status'] 128 $true;Assert-TaskString $taskParsed['source_manifest_sha256'] 64 $true
  foreach($taskK in @('entries','inverse_records','reserved','used','prior','composed_after')){Assert-TaskIntegral $taskParsed[$taskK] $true;if($taskParsed[$taskK] -gt [long]::MaxValue){throw 'SUMMARY_SIGNED_REPRESENTATION'}}
  foreach($taskK in @('vendor','quality','Ready','compiler_peak_disk_qualified')){if((Get-TaskRuntimeType $taskParsed[$taskK]) -ne [bool] -or $taskParsed[$taskK]){throw 'EXACT_FALSE_SUMMARY_FLAGS'}}
  $taskStatus=if($taskMode -ceq 'produce'){'CLOCK_PRODUCED_REQUIRES_INDEPENDENT_VERIFY'}else{'CLOCK_VERIFIED_REQUIRES_ROOT_OUTPUT_REVIEW'}
  if($taskParsed['status'] -cne $taskStatus -or $taskParsed['source_manifest_sha256'] -cne $taskExpectedManifest -or $taskParsed['entries'] -ne 16929027 -or $taskParsed['inverse_records'] -le 0 -or $taskParsed['inverse_records'] -gt 960000){throw 'EXACT_MODULE_SUMMARY_DOMAIN'}
  # Only these twelve validated fields plus our own validated controls survive.
  $taskResult=@{}
  foreach($taskK in $taskSummaryFields){$taskResult[$taskK]=$taskParsed[$taskK]}
  $taskResult['actual_control_witnesses']=$taskRecords.ToArray()
  return $taskResult
 }catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}
 finally{Close-TaskOwner $taskFile;Complete-TaskOwnerClose $taskBefore $taskBody}
}
function Get-TaskBuildArtifacts{
 $taskRows=[Collections.Generic.List[object]]::new();$taskSeen=[Collections.Generic.List[string]]::new();$taskSum=0L
 foreach($taskF in (Get-TaskEntries $taskBuildRoot $false 4)){if($taskF.PSIsContainer -or -not $taskBuildCaps.ContainsKey($taskF.Name)){throw 'Compiler final artifact set is not admitted'};$taskH=Get-TaskHash $taskF.FullName $taskBuildCaps[$taskF.Name];if($taskH['bytes'] -gt 67108864L-$taskSum){throw 'Compiler final aggregate not admitted'};$taskSum+=$taskH['bytes'];$taskSeen.Add($taskF.Name);$taskRows.Add(@{path=$taskF.FullName;bytes=$taskH['bytes'];sha256=$taskH['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 Assert-TaskFileSet $taskSeen.ToArray() @($taskBuildCaps.Keys);return @{files=$taskRows.ToArray();bytes=$taskSum}
}
function Assert-TaskDependencies($Build){
 $taskReportPath=Get-TaskCanonicalPath (Join-Path $taskBuildRoot 'source-dependencies.json');$taskReportHash=$null;$taskReportMatches=0
 foreach($taskArtifact in $Build['files']){if($taskArtifact['path'] -ceq $taskReportPath){$taskReportHash=$taskArtifact['sha256'];$taskReportMatches++}}
 if($taskReportMatches -ne 1){throw 'DEPENDENCY_CAPTURED_REPORT_DESCRIPTOR'}
 $taskD=Read-TaskJson $taskReportPath 4194304 $taskReportHash
 if((Get-TaskRuntimeType $taskD) -ne [System.Management.Automation.OrderedHashtable]){throw 'DEPENDENCY_ROOT_MAP'}
 $taskData=$taskD['Data'];if((Get-TaskRuntimeType $taskData) -ne [System.Management.Automation.OrderedHashtable]){throw 'DEPENDENCY_DATA_MAP'}
 Assert-TaskString $taskData['Source'] 512
 if((Resolve-TaskInput $taskData['Source']) -ine (Join-Path $taskBase 'clock_stage.cpp')){throw 'Compiler source-dependency source binding'}
 foreach($taskKey in @('ImportedModules','ImportedHeaderUnits')){
  if($taskData.Contains($taskKey)){if((Get-TaskRuntimeType $taskData[$taskKey]) -ne [object[]] -or $taskData[$taskKey].Length -ne 0){throw 'Unqualified compiler module/header-unit dependency'}}
 }
 if((Get-TaskRuntimeType $taskData['Includes']) -ne [object[]] -or $taskData['Includes'].Length -lt 1 -or $taskData['Includes'].Length -gt $taskManifest['profile']['include_input_count']){throw 'Compiler dependency Includes finite profile'}
 foreach($taskPath in $taskData['Includes']){Assert-TaskString $taskPath 512;if(-not $taskIncludeIndex.ContainsKey((Resolve-TaskInput $taskPath))){throw 'Source-dependency include outside pre-fenced conservative inventory'}}
 # No arbitrary parsed field is retained in the receipt.
 return @{includes=$taskData['Includes'].Length;report_is_additional_artifact_only=$true;complete_tool_inventory_replaced=$false}
}
function Save-TaskFailureInventory{
 $taskEntries=[Collections.Generic.List[object]]::new();$taskRows=[Collections.Generic.List[object]]::new();$taskComplete=$true;$taskReason=$null
 try{if([IO.Directory]::Exists($taskRoot)){$null=Get-TaskEntries $taskRoot $true ([int]$taskManifest['profile']['root_walk_entries_max']) $taskEntries}}catch{$taskComplete=$false;$taskReason='BOUNDED_FAILURE_WALK_REFUSED';Set-TaskSecondary 8 $_}
 foreach($taskF in $taskEntries){if($taskF.PSIsContainer){continue};$taskRows.Add(@{path=$taskF.FullName;bytes=$taskF.Length;reparse=$taskF.Reparse;admission='NOT_AUTOMATICALLY_ADMITTED';opaque_temporary=($taskF.FullName.StartsWith($taskOpaqueRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase))})}
 $taskReceipt['failure_inventory_complete']=$taskComplete;$taskReceipt['failure_inventory_code']=$taskReason
 Write-TaskJson (Join-Path $taskStageRoot 'failure-inventory.json') @{files=$taskRows.ToArray();complete=$taskComplete;code=$taskReason;root_prefix=(Get-TaskSafePrefix $taskRoot 512);compiler_temporary_peak_disk_qualified=$false;admitted_pool_success=$false;quarantine_retains_ownership=($null -ne $taskQuarantine)}
}

try{
 # Hash literal manifest before trusting source-sized arithmetic. Loading this
 # exact source-only revision is not authority: all three entry guards are false.
 $taskBootstrap=Read-TaskBoundedBytes $taskManifestPath 1048576;if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($taskBootstrap)) -cne $taskExpectedManifest){throw 'External manifest hash'}
 Initialize-TaskController
 Add-TaskJsonVisit $taskBootstrap.Length;$taskBootstrapCompact=Assert-TaskRawJsonBytes $taskBootstrap 15978 512 4
 $taskBootstrapRaw=[Text.UTF8Encoding]::new($false,$true).GetString($taskBootstrap,0,$taskBootstrapCompact)
 $taskManifest=& $taskConvertFromJson -InputObject $taskBootstrapRaw -AsHashtable -Depth 24 -DateKind String;$taskBootstrap=$null;$taskBootstrapRaw=$null
 # The prototype has no bound catalogue. Even later guard promotion alone
 # cannot turn HOLD_UNBOUND fields into authority or numeric prices.
 if($taskManifest['cost_catalogue']['binding_state'] -cne 'BOUND_REVIEWED' -or $taskManifest['cost_catalogue']['definition_files'].Count -eq 0){throw 'SOURCE_COST_CATALOGUE_UNBOUND_NO_PROTOCOL_GO'}
 foreach($taskKey in @('runner_upper','producer_upper','verifier_upper','whole_upper')){Assert-TaskBoundSourceInteger $taskKey $taskManifest['source_work'][$taskKey]}
 $taskRunnerLimit=[long]$taskManifest['source_work']['runner_upper'];$taskProducerLimit=[long]$taskManifest['source_work']['producer_upper'];$taskVerifierLimit=[long]$taskManifest['source_work']['verifier_upper'];$taskTotalWork=[long]$taskManifest['source_work']['whole_upper']
 if($taskProducerLimit -gt [long]::MaxValue-$taskRunnerLimit -or $taskVerifierLimit -gt [long]::MaxValue-$taskRunnerLimit-$taskProducerLimit){throw 'CHECKED_CROSS_LANGUAGE_SOURCE_SUM'}
 if($taskTotalWork -ne $taskRunnerLimit+$taskProducerLimit+$taskVerifierLimit){throw 'EXACT_DISJOINT_WHOLE_COMPOSITION'}
 $taskReviewedWorkLimit=$taskTotalWork;Assert-TaskReservation $taskTotalWork $taskReviewedWorkLimit
 if((Get-TaskRuntimeType $taskManifest['inputs']) -ne [object[]] -or $taskManifest['inputs'].Count -ne $taskManifest['profile']['input_count']){throw 'EXACT_INPUT_ARRAY_PROFILE'}
 if($taskManifest['inputs'].Count -gt [int]::MaxValue-$taskOwnNames.Count-1){throw 'CHECKED_JSON_ARRAY_PROFILE'}
 # Largest source-built output array is transitive inputs: history+own4+the
 # external manifest. Largest map is the complete fixed receipt schema.
 $taskJsonProfile=@{array_entries=($taskManifest['inputs'].Count+$taskOwnNames.Count+1);map_entries=$taskReceipt.Count;string_units=512;depth=24}
 Initialize-TaskPathCatalogue
 $taskPredecessorDescriptor=Get-TaskPredecessorDescriptor $taskManifest['predecessor_bundle']
 $taskInputIndex=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase);foreach($taskPin in $taskManifest['inputs']){$taskPath=Resolve-TaskInput $taskPin['path'];if($taskPath.Length -gt 512 -or $taskInputIndex.ContainsKey($taskPath)){throw 'Exact unique input paths'};$taskInputIndex.Add($taskPath,$taskPin)}
 if($taskInputIndex.Count -ne $taskManifest['profile']['input_count']){throw 'Actual closed profile count'}
 $taskIncludeIndex=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase);foreach($taskPair in $taskInputIndex.GetEnumerator()){$taskIncludeIndex.Add($taskPair.Key,$taskPair.Value)}
 foreach($taskName in $taskManifest['current_include_inputs']){if($taskName -cne 'source-costs.h'){throw 'Unlisted current source header'};$taskPin=@($taskManifest['sources']|Where-Object {$_['path'] -ceq $taskName});if($taskPin.Count -ne 1){throw 'Current source header exact own pin'};$taskIncludeIndex.Add((Join-Path $taskBase $taskName),$taskPin[0])}
 if($taskIncludeIndex.Count -ne $taskManifest['profile']['include_input_count']){throw 'Exact historical plus current header include profile'}
 $taskAuthoredBytes=[long](Get-Item -LiteralPath $taskManifestPath).Length;foreach($taskPin in $taskManifest['sources']){$taskAuthoredBytes+=[long]$taskPin['bytes']};if($taskAuthoredBytes -gt 1048576L){throw 'Own five-file source payload1MiB'}
 $taskControlCaps=0L;foreach($taskC in $taskManifest['control_files']){$taskControlCaps+=[long]$taskC['bytes_cap']};$taskReservedRetained=[long]$taskManifest['profile']['input_bytes']+$taskAuthoredBytes+152627224L+67108864L+67108864L+$taskControlCaps;Assert-TaskRetainedReservation $taskReservedRetained 880803840L
 if(-not $taskRoot.StartsWith((Join-Path $taskBase 'runs')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or $taskRoot.Substring((Join-Path $taskBase 'runs').Length+1).Contains([IO.Path]::DirectorySeparatorChar)){throw 'New owned direct run root'}
 Assert-TaskString $taskRoot 512 $true
 Assert-NoLink $taskRoot;Assert-TaskAttempt (Test-Path -LiteralPath $taskRoot) $taskRoot
 $taskRuns=Join-Path $taskBase 'runs';if((Test-Path -LiteralPath $taskRuns) -and @(Get-TaskEntries $taskRuns $false 1).Count -gt 0){throw 'Revision sole attempt already exists'}
 Assert-TaskPins;$taskPredecessor=Read-TaskJson (Resolve-TaskInput $taskPredecessorDescriptor['path']) 1048576 $taskPredecessorDescriptor['sha256'];Assert-TaskPredecessor $taskPredecessor
 $taskCaps=[Collections.Generic.Dictionary[string,long]]::new([StringComparer]::Ordinal);foreach($taskA in $taskManifest['references']){$taskCaps.Add($taskA['path'],[long]$taskA['bytes_cap'])}
 $taskBuildCaps=[Collections.Generic.Dictionary[string,long]]::new([StringComparer]::Ordinal);foreach($taskA in $taskManifest['compiler_final_outputs']){$taskBuildCaps.Add($taskA['name'],[long]$taskA['bytes_cap'])}
 $null=New-Item -ItemType Directory -Path $taskRoot;$taskCreated=$true
 foreach($taskDir in @($taskStageRoot,$taskBuildRoot,$taskOpaqueRoot,(Join-Path $taskDataRoot 'references/clock'),(Join-Path $taskDataRoot 'references/native-inverse'),(Join-Path $taskRoot 'controls'))){$null=New-Item -ItemType Directory -Path $taskDir}
 $taskMarkerPath=Join-Path $taskStageRoot 'consumed.json';$taskMarkerValue=@{stage=$Stage;manifest=$taskExpectedManifest;work=$taskTotalWork;retained_reservation=$taskReservedRetained;attempts=1;opaque_compiler_peak_disk_qualified=$false}
 $taskMarkerMeasure=Measure-TaskJson $taskMarkerValue;Assert-TaskDocumentWrite $taskMarkerPath $taskMarkerMeasure['bytes'];$taskMarker=Encode-TaskJson $taskMarkerValue 1048576 $taskMarkerMeasure
 $taskMarkerFile=$null;$taskMarkerBefore=$script:taskPrimaryRaw;$taskMarkerBody=$null
 try{$taskMarkerFile=[IO.File]::Open($taskMarkerPath,[IO.FileMode]::CreateNew);$taskMarkerFile.Write($taskMarker,0,$taskMarker.Length)}catch{$taskMarkerBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}finally{Close-TaskOwner $taskMarkerFile;Complete-TaskOwnerClose $taskMarkerBefore $taskMarkerBody}
 $taskControls=[Collections.Generic.List[object]]::new(10);Assert-TaskReservation $taskTotalWork $taskReviewedWorkLimit
 $null=Assert-TaskRawJsonBytes ([Text.UTF8Encoding]::new($false,$true).GetBytes('18446744073709551615')) 1 1 1;$null=Assert-TaskRawJsonBytes ([Text.UTF8Encoding]::new($false,$true).GetBytes('-9223372036854775808')) 1 1 1
 $taskControls.Add((Expect-TaskGate 'json-scalar-one-past-boundary' {$null=Assert-TaskRawJsonBytes ([Text.UTF8Encoding]::new($false,$true).GetBytes('184467440737095516150')) 1 1 1} 'JSON_RAW_SCALAR_UNITS' @{observed=21;maximum=20}))
 Assert-TaskPollAdmission ($taskChildPollMaximum-1)
 $taskControls.Add((Expect-TaskGate 'poll-one-past-boundary' {Assert-TaskPollAdmission $taskChildPollMaximum} 'CHILD_POLL_LIMIT' @{completed=$taskChildPollMaximum;limit=$taskChildPollMaximum;attempted=($taskChildPollMaximum+1)}))
 $taskControls.Add((Expect-TaskGate 'work-one-below' {Assert-TaskReservation $taskTotalWork ($taskTotalWork-1)} 'WHOLE_STAGE_WORK_RESERVATION' @{total=$taskTotalWork;limit=($taskTotalWork-1);global=$taskReviewedWorkLimit}))
 $taskControls.Add((Expect-TaskGate 'bytes-one-below' {Assert-TaskRetainedReservation $taskReservedRetained ($taskReservedRetained-1)} 'WHOLE_STAGE_RETAINED_RESERVATION' @{total=$taskReservedRetained;limit=($taskReservedRetained-1);global=880803840L}))
 $taskControls.Add((Expect-TaskGate 'one-use-already-consumed' {Assert-TaskAttempt (Test-Path -LiteralPath $taskMarkerPath) $taskMarkerPath} 'ONE_ATTEMPT_ALREADY_CONSUMED' @{path=$taskMarkerPath;exists=$true}))
 $taskSourceBundle=Resolve-TaskInput $taskPredecessorDescriptor['path'];$taskBundleHash=Get-TaskHash $taskSourceBundle 98009;$taskMutantBundle=Join-Path $taskRoot 'controls/predecessor-mutant.json';Copy-TaskOwned $taskSourceBundle $taskMutantBundle 98009 98009
 $taskIdentity=@{path=$taskMutantBundle;bytes=98009L;sha256=$taskPredecessorDescriptor['sha256']};Assert-TaskIdentity $taskIdentity (Get-TaskHash $taskMutantBundle 98009)
 $taskBundleFirst=$null;$taskBundleBefore=$script:taskPrimaryRaw;$taskBundleBody=$null
 try{$taskBundleFirst=[IO.File]::OpenRead($taskMutantBundle);$taskFirst=$taskBundleFirst.ReadByte()}catch{$taskBundleBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}finally{Close-TaskOwner $taskBundleFirst;Complete-TaskOwnerClose $taskBundleBefore $taskBundleBody}
 $taskMutation=Mutate-TaskOwnedBytes $taskMutantBundle 0 ([byte[]]@([byte]($taskFirst -bxor 1)));$taskCorrupt=Get-TaskHash $taskMutantBundle 98009
 $taskControls.Add((Expect-TaskGate 'own-tool-or-predecessor-changed' {Assert-TaskIdentity $taskIdentity $taskCorrupt} 'BEFORE_AFTER_COMPLETE_INPUT_IDENTITY' @{path=$taskMutantBundle;expected_bytes=98009L;observed_bytes=$taskCorrupt['bytes'];expected_hash=$taskIdentity['sha256'];observed_hash=$taskCorrupt['sha256']}));$taskControls[$taskControls.Count-1]['mutation']=$taskMutation;$taskControls[$taskControls.Count-1]['initial_copy_sha256']=$taskBundleHash['sha256']
 $taskReceipt['reserved_source_work']=$taskTotalWork;$taskReceipt['reserved_admitted_retained']=$taskReservedRetained;$taskReceipt['status']='OPAQUE_COMPILER_STARTING';$taskReceipt['runner_controls']=$taskControls.ToArray();Save-TaskReceipt
 $taskCompiler=Resolve-TaskInput $taskManifest['compiler']['path'];Assert-TaskConsolePE (Get-TaskPEHeader $taskCompiler)
 if($taskManifest['compiler']['arguments'].Count -ne 28){throw 'EXACT_COMPILER_ARGUMENT_EXTENT'}
 $taskArguments=[Collections.Generic.List[string]]::new(28)
 foreach($taskArg in $taskManifest['compiler']['arguments']){Assert-TaskString $taskArg 512 $true;$taskExpanded=Replace-TaskBounded $taskArg '<BASE>' $taskBase;$taskExpanded=Replace-TaskBounded $taskExpanded '<REPO>' $taskRepo;$taskExpanded=Replace-TaskBounded $taskExpanded '<BUILD>' $taskBuildRoot;$taskArguments.Add($taskExpanded)}
 $taskCompile=Invoke-TaskFencedChild 'one-opaque-compile' $taskCompiler $taskArguments.ToArray();if($taskCompile['raw_exit'] -ne 0){throw 'Compiler raw failure'}
 $taskBuild=Get-TaskBuildArtifacts;$taskReceipt['compiler_final_outputs']=$taskBuild;$taskReceipt['source_dependencies']=Assert-TaskDependencies $taskBuild;$taskAdmittedCompiler=$true
 $taskBinary=Join-Path $taskBuildRoot 'clock_stage.exe';$taskPE=Get-TaskPEHeader $taskBinary;Assert-TaskConsolePE $taskPE;$taskInitialBinary=Get-TaskHash $taskBinary 33554432
 $taskGuiPath=Join-Path $taskRoot 'controls/gui-mutant.exe';Copy-TaskOwned $taskBinary $taskGuiPath $taskInitialBinary['bytes'] 33554432;Assert-TaskIdentity @{path=$taskGuiPath;bytes=$taskInitialBinary['bytes'];sha256=$taskInitialBinary['sha256']} (Get-TaskHash $taskGuiPath 33554432);Assert-TaskConsolePE (Get-TaskPEHeader $taskGuiPath)
 $taskGuiMutation=Mutate-TaskOwnedBytes $taskGuiPath ([long]$taskPE['offset']+24+68) ([BitConverter]::GetBytes([uint16]2));$taskGui=Get-TaskPEHeader $taskGuiPath
 $taskControls.Add((Expect-TaskGate 'pe-gui-subsystem' {Assert-TaskConsolePE $taskGui} 'ACTUAL_PE_CONSOLE_BEFORE_LAUNCH' @{signature=$taskGui['signature'];machine=$taskGui['machine'];magic=$taskGui['magic'];subsystem=2}));$taskControls[$taskControls.Count-1]['mutation']=$taskGuiMutation;$taskControls[$taskControls.Count-1]['initial_copy_sha256']=$taskInitialBinary['sha256'];$taskControls[$taskControls.Count-1]['actual_mutant_sha256']=(Get-TaskHash $taskGuiPath 33554432)['sha256']
 $taskBinaryHash=Get-TaskHash $taskBinary 33554432
 $taskProduce=Invoke-TaskFencedChild 'oracle-produce' $taskBinary @('produce',$taskDataRoot,$taskExpectedManifest,[string]$taskRunnerLimit,[string]$taskRunnerLimit,[string]$taskTotalWork) $taskBinaryHash
 $taskProduced=Read-TaskChildResult $taskProduce;if($taskProduced['reserved'] -ne $taskProducerLimit -or $taskProduced['used'] -gt $taskProduced['reserved'] -or $taskProduced['prior'] -ne $taskRunnerLimit -or $taskProduced['composed_after'] -ne $taskRunnerLimit+$taskProducerLimit){throw 'Actual producer summary/ledger'}
 $taskBefore=Get-TaskReferences;Assert-TaskFileSet $taskBefore['names'] @($taskCaps.Keys)
 $taskMissing=@($taskBefore['names']|Where-Object {$_ -cne 'references/native-inverse/r48000-map5.json'});$taskControls.Add((Expect-TaskGate 'missing-inverse-descriptor' {Assert-TaskFileSet $taskMissing @($taskCaps.Keys)} 'COMPLETE_EXACT_REFERENCE_FILE_SET' @{observed=55;expected=56;missing='references/native-inverse/r48000-map5.json';unknown='';duplicate=''}))
 $taskExtra=@($taskBefore['names'])+@('unlisted.bin');$taskControls.Add((Expect-TaskGate 'extra-file' {Assert-TaskFileSet $taskExtra @($taskCaps.Keys)} 'COMPLETE_EXACT_REFERENCE_FILE_SET' @{observed=57;expected=56;missing='';unknown='unlisted.bin';duplicate=''}))
 $taskDuplicate=@($taskBefore['names'])+@($taskBefore['names'][0]);$taskControls.Add((Expect-TaskGate 'duplicate-file' {Assert-TaskFileSet $taskDuplicate @($taskCaps.Keys)} 'COMPLETE_EXACT_REFERENCE_FILE_SET' @{observed=57;expected=56;missing='';unknown='';duplicate=$taskBefore['names'][0]}))
 $taskReceipt['producer']=$taskProduced;$taskReceipt['status']='PRODUCED_VERIFY_PENDING';Save-TaskReceipt
 $taskPrior=$taskRunnerLimit+$taskProducerLimit
 $taskVerify=Invoke-TaskFencedChild 'independent-full-verify' $taskBinary @('verify',$taskDataRoot,$taskExpectedManifest,[string]$taskPrior,[string]$taskRunnerLimit,[string]$taskTotalWork) $taskBinaryHash
 $taskVerified=Read-TaskChildResult $taskVerify;if($taskVerified['reserved'] -ne $taskVerifierLimit -or $taskVerified['composed_after'] -ne $taskTotalWork -or $taskVerified['prior'] -ne $taskPrior -or $taskVerified['inverse_records'] -ne $taskProduced['inverse_records'] -or $taskVerified['used'] -gt $taskVerified['reserved']){throw 'Actual independent replay summary/ledger'}
 $taskAfter=Get-TaskReferences;Assert-TaskSameReferences $taskBefore $taskAfter;$taskFinalBuild=Get-TaskBuildArtifacts
 foreach($taskOldFile in $taskBuild['files']){$taskNow=@($taskFinalBuild['files']|Where-Object path -ceq $taskOldFile['path']);if($taskNow.Count -ne 1){throw 'Compiler artifacts changed'};Assert-TaskIdentity $taskOldFile $taskNow[0]}
 $taskControlsFiles=[Collections.Generic.List[object]]::new();foreach($taskC in $taskManifest['control_files']){$taskPath=Join-Path $taskRoot $taskC['path'];$taskH=Get-TaskHash $taskPath $taskC['bytes_cap'];if($taskC.Contains('exact_bytes') -and $taskH['bytes'] -ne $taskC['exact_bytes']){throw 'Actual bounded record-control file length'};$taskControlsFiles.Add(@{path=$taskPath;bytes=$taskH['bytes'];sha256=$taskH['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 $taskExpectedRunFiles=[Collections.Generic.List[string]]::new();foreach($taskN in $taskCaps.Keys){$taskExpectedRunFiles.Add('data/'+$taskN)};foreach($taskN in $taskBuildCaps.Keys){$taskExpectedRunFiles.Add('build/'+$taskN)};foreach($taskC in $taskManifest['control_files']){$taskExpectedRunFiles.Add($taskC['path'])};foreach($taskN in @('stdout.log','stderr.log','receipt.json','consumed.json')){$taskExpectedRunFiles.Add('stage/'+$taskN)}
 $taskObservedRunFiles=[Collections.Generic.List[string]]::new();foreach($taskF in (Get-TaskEntries $taskRoot $true ([int]$taskManifest['profile']['root_walk_entries_max']))){Assert-NoLink $taskF.FullName;if(-not $taskF.PSIsContainer){$taskObservedRunFiles.Add([IO.Path]::GetRelativePath($taskRoot,$taskF.FullName).Replace([char]92,[char]47))}}
 Assert-TaskFileSet $taskObservedRunFiles.ToArray() $taskExpectedRunFiles.ToArray()
 Assert-TaskPins;if($null -ne $script:taskPrimaryRaw){throw $script:taskPrimaryRaw};$taskReceipt['verification']=$taskVerified;$taskReceipt['runner_controls']=$taskControls.ToArray();$taskReceipt['runner_used']=$taskUnits;$taskReceipt['status']='CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED';Save-TaskReceipt
 $taskArtifacts=[Collections.Generic.List[object]]::new();foreach($taskA in @($taskAfter['files'])+@($taskBuild['files'])+@($taskControlsFiles.ToArray())){$taskArtifacts.Add($taskA)}
 foreach($taskName in @('stdout.log','stderr.log','receipt.json','consumed.json')){$taskPath=Join-Path $taskStageRoot $taskName;$taskH=Get-TaskHash $taskPath 67108864;$taskArtifacts.Add(@{path=$taskPath;bytes=$taskH['bytes'];sha256=$taskH['sha256'];origin_bundle_sha256=$taskExpectedManifest})}
 $taskAdmitted=[long]$taskManifest['profile']['input_bytes']+$taskAuthoredBytes;foreach($taskA in $taskArtifacts){$taskAdmitted+=[long]$taskA['bytes']}
 $taskTransitive=[Collections.Generic.List[object]]::new();foreach($taskPin in $taskManifest['inputs']){$taskTransitive.Add(@{path=(Resolve-TaskInput $taskPin['path']);bytes=$taskPin['bytes'];sha256=$taskPin['sha256'];origin_bundle_sha256=$taskExpectedManifest})};foreach($taskPin in $taskManifest['sources']){$taskTransitive.Add(@{path=(Join-Path $taskBase $taskPin['path']);bytes=$taskPin['bytes'];sha256=$taskPin['sha256'];origin_bundle_sha256=$taskExpectedManifest})};$taskTransitive.Add(@{path=$taskManifestPath;bytes=(Get-Item -LiteralPath $taskManifestPath).Length;sha256=$taskExpectedManifest;origin_bundle_sha256=$taskExpectedManifest})
 $taskBundle=[ordered]@{schema=1;stage=$Stage;status='CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED';source_manifest_sha256=$taskExpectedManifest;predecessor_bundle=$taskPredecessorDescriptor;artifacts=$taskArtifacts.ToArray();transitive_inputs=$taskTransitive.ToArray();admitted_retained_excluding_bundle=$taskAdmitted;bundle_bytes=0L;admitted_retained=0L;reserved_source_work=$taskTotalWork;runner_used=$taskUnits;compiler_operation='OPAQUE';compiler_temporary_peak_disk_qualified=$false;compiler_CPU_internal_heap_RSS_qualified=$false;old_broad_compiler_output_bound_fulfilled=$false;vendor=$false;quality=$false;context=$false;Ready=$false;renderer=$false}
 $taskStable=$false;$taskBundleMeasure=$null
 for($taskPass=0;$taskPass -lt 5;$taskPass++){
  $taskBundleMeasure=Measure-TaskJson $taskBundle
  if($taskBundle['bundle_bytes'] -eq $taskBundleMeasure['bytes'] -and $taskBundle['admitted_retained'] -eq $taskAdmitted+$taskBundleMeasure['bytes']){$taskStable=$true;break}
  $taskBundle['bundle_bytes']=$taskBundleMeasure['bytes'];$taskBundle['admitted_retained']=$taskAdmitted+$taskBundleMeasure['bytes']
 }
 if(-not $taskStable){throw 'Exact admitted bundle byte-length fixed point'}
 Assert-TaskRetainedReservation $taskBundle['admitted_retained'] 880803840L
 Write-TaskJson (Join-Path $taskStageRoot 'bundle.json') $taskBundle 1048576 $taskBundleMeasure
 Write-Output ('CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED bundle_sha256='+(Get-TaskHash (Join-Path $taskStageRoot 'bundle.json') 1048576)['sha256'])
}catch{
 if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_}
 $taskReceipt['status']='FAILED_OR_NOT_REACHED_NO_CERTIFICATION';$taskReceipt['failure_code']='PRIMARY_FAILURE_RAW_RETAINED_NOT_SERIALIZED';$taskReceipt['primary_failure']=$true;$taskReceipt['compiler_final_outputs_admitted']=$taskAdmittedCompiler
 if($taskCreated){
  try{Save-TaskFailureInventory}catch{Set-TaskSecondary 8 $_}
  try{Save-TaskReceipt}catch{Set-TaskSecondary 9 $_;Write-Error 'Failure footer incomplete; preserve all files and raw primary/secondary/child/owned-read state. No certification.' -ErrorAction Continue}
 }
 throw $script:taskPrimaryRaw
}
