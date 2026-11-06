#Requires -Version 7.0
param([switch]$Execute,[Parameter(Mandatory=$true)][ValidateSet('Freeze','CompileAndRun')][string]$Stage,[string]$PreparedRun)
$ErrorActionPreference='Stop'
if(-not $Execute){throw 'UNRUN sources. Root must authorize this exact stage separately.'}
$taskRevision=[IO.Path]::GetFullPath($PSScriptRoot)
$taskRepo=[IO.Path]::GetFullPath((Join-Path $taskRevision '../../../..'))
if($taskRevision -cne [IO.Path]::GetFullPath((Join-Path $taskRepo 'build-ci/musical-audio-persistent-planner-certification/source-revisions/physical-c1-context-r5'))){throw 'Unexpected revision path'}
# Root prepays complete declared stage ceilings before any source/JSON/hash/file child work.
$taskReservedWork=260000000L;$taskReservedPayload=828304896L
# Retained trace/relation525266432 + frozen23762408 + six16MiB case workspaces
# + shared48MiB + control1MiB =701072360, below the unchanged complete forecast.
if(525266432L+23762408L+6L*16777216L+50331648L+1048576L -gt $taskReservedPayload){throw 'Shared retained/transient worksheet'}
# Largest case386630848 trace/relation +16811376 frozen +16MiB work +48MiB shared.
if(386630848L+16811376L+16777216L+50331648L -gt 536870912L){throw 'Per-case shared/transient worksheet'}
if($taskReservedWork -gt 268435456L -or $taskReservedPayload -gt 1073741824L){throw 'Prepaid parent envelope'}
$taskRunnerUnits=0L;$taskLogBytes=0L;$taskReceiptWrites=0
function Spend-TaskUnits([long]$Units){if($Units -lt 0 -or $Units -gt 1048576L-$script:taskRunnerUnits){throw 'Runner prepaid source-unit allowance'};$script:taskRunnerUnits+=$Units}
Spend-TaskUnits 131072 # fixed startup and32 prepaid receipt writes, including failure footer
function Get-TaskSha256([string]$Path){
    Spend-TaskUnits 1024
    $taskHashStream=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
    $taskHasher=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256);$taskHashBuffer=[byte[]]::new(4096)
    try{while($true){Spend-TaskUnits 2;$taskRead=$taskHashStream.Read($taskHashBuffer,0,4096);if($taskRead -eq 0){break};$taskHasher.AppendData($taskHashBuffer,0,$taskRead)};return [Convert]::ToHexString($taskHasher.GetHashAndReset())}finally{$taskHasher.Dispose();$taskHashStream.Dispose()}
}
$taskManifestPath=Join-Path $taskRevision 'authored-manifest.json'
$taskManifestHash=(Get-TaskSha256 $taskManifestPath)
$taskManifest=Get-Content -LiteralPath $taskManifestPath -Raw|ConvertFrom-Json
function Assert-TaskPins {
    if((Get-TaskSha256 $taskManifestPath) -cne $taskManifestHash){throw 'Reviewed manifest changed'}
    foreach($taskPin in $taskManifest.pins){$taskPinnedPath=if($taskPin.PSObject.Properties.Name -contains 'absolute_path'){$taskPin.absolute_path}else{Join-Path $taskRepo $taskPin.path};if((Get-TaskSha256 $taskPinnedPath) -ine $taskPin.sha256){throw ('Immutable source/data changed: '+$taskPinnedPath)}}
}
function Get-TaskBytes([string]$Path){
    $taskSum=0L
    Spend-TaskUnits 4096
    foreach($taskFile in (Get-ChildItem -LiteralPath $Path -Recurse -File)){Spend-TaskUnits 4;$taskSum+=$taskFile.Length}
    return $taskSum
}
function Assert-TaskStorage {
    $taskRuns=Join-Path $taskRevision 'runs'
    if((Test-Path -LiteralPath $taskRuns) -and (Get-TaskBytes $taskRuns) -gt $taskReservedPayload){throw 'MusicalResourceLimit: retained revision exceeds predeclared stage-overlap envelope'}
}
function Invoke-TaskChild([string]$File,[string[]]$Arguments,[string]$Log,[int]$Seconds,[bool]$CompilerEnvironment=$false){
    Spend-TaskUnits 4096
    Spend-TaskUnits 1024 # Prepaid cancellation/kill/two-read settlement/cleanup, including refusal.
    if($null -ne $script:taskUnsettledReadOwner){throw 'Prior raw reads remain owned and unsettled; no further child authority'}
    # Before launch: normal controlled buffers<=64KiB; special environment<=1MiB.
    if(65536L+1048576L -gt 2097152L){throw 'Shared launcher transient preflight'}
    $taskInfo=[Diagnostics.ProcessStartInfo]::new();$taskInfo.FileName=$File;$taskInfo.WorkingDirectory=$taskRepo
    $taskInfo.UseShellExecute=$false;$taskInfo.CreateNoWindow=$true;$taskInfo.RedirectStandardOutput=$true;$taskInfo.RedirectStandardError=$true
    if($CompilerEnvironment){$taskInfo.Arguments='/u /d /s /c ""'+$Arguments[0]+'" >nul 2>&1 && set"'}else{foreach($taskArg in $Arguments){$taskInfo.ArgumentList.Add($taskArg)}}
    $taskOutBuffer=[byte[]]::new(4096);$taskErrBuffer=[byte[]]::new(4096)
    $taskEnvOut=$null;$taskEnvErr=$null;$taskOutSink=$null;$taskErrSink=$null
    $taskEnvOutCount=0;$taskEnvErrCount=0;$taskEnvSeen=0L;$taskChildBytes=0L;$taskOmitted=0L;$taskObservedBytes=0L;$taskObservedUnretained=0L
    $taskStem=Join-Path ([IO.Path]::GetDirectoryName($Log)) ([IO.Path]::GetFileNameWithoutExtension($Log))
    $taskProcess=$null;$taskStarted=$false;$taskTimeout=$false;$taskLogCap=$false;$taskFailure=$null;$taskActualExit=$null
    $taskReadCancel=$null;$taskOutRead=$null;$taskErrRead=$null;$taskOutAccounted=$true;$taskErrAccounted=$true
    $taskOutDone=$false;$taskErrDone=$false;$taskReadsSettled=$true;$taskPendingReads=0;$taskCleanupFaults=@();$taskChildExitObserved=$false
    try {
        $taskReadCancel=[Threading.CancellationTokenSource]::new()
        if($CompilerEnvironment){$taskEnvOut=[byte[]]::new(65536);$taskEnvErr=[byte[]]::new(65536)}else{
            $taskOutSink=[IO.FileStream]::new(($taskStem+'.stdout.log'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
            $taskErrSink=[IO.FileStream]::new(($taskStem+'.stderr.log'),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read,4096,[IO.FileOptions]::SequentialScan)
        }
        $taskProcess=[Diagnostics.Process]::Start($taskInfo);$taskStarted=$true;$taskWatch=[Diagnostics.Stopwatch]::StartNew()
        $taskOutRead=$taskProcess.StandardOutput.BaseStream.ReadAsync($taskOutBuffer,0,4096,$taskReadCancel.Token);$taskOutAccounted=$false
        $taskErrRead=$taskProcess.StandardError.BaseStream.ReadAsync($taskErrBuffer,0,4096,$taskReadCancel.Token);$taskErrAccounted=$false
        while(-not($taskOutDone -and $taskErrDone -and $taskProcess.HasExited)){
            Spend-TaskUnits 2
            foreach($taskDirection in 0,1){
                $taskDone=if($taskDirection -eq 0){$taskOutDone}else{$taskErrDone}
                $taskRead=if($taskDirection -eq 0){$taskOutRead}else{$taskErrRead}
                if(-not $taskDone -and $taskRead.IsCompleted){
                    $taskN=$taskRead.GetAwaiter().GetResult()
                    if($taskDirection -eq 0){$taskOutAccounted=$true}else{$taskErrAccounted=$true}
                    $taskObservedBytes+=$taskN;$taskObservedUnretained+=$taskN;Spend-TaskUnits 16
                    if($taskN -eq 0){if($taskDirection -eq 0){$taskOutDone=$true}else{$taskErrDone=$true}}else{
                        # Assignment inside the branch preserves the actual byte[] object; no pipeline enumeration.
                        [byte[]]$taskBuffer=$null
                        if($taskDirection -eq 0){$taskBuffer=$taskOutBuffer}else{$taskBuffer=$taskErrBuffer}
                        if($CompilerEnvironment){
                            if($taskN -gt 65536L-$taskEnvSeen){$taskLogCap=$true;$taskOmitted+=$taskN;throw 'Bounded compiler-environment64KiB refusal'}
                            if($taskDirection -eq 0){[Array]::Copy($taskBuffer,0,$taskEnvOut,$taskEnvOutCount,$taskN);$taskEnvOutCount+=$taskN}else{[Array]::Copy($taskBuffer,0,$taskEnvErr,$taskEnvErrCount,$taskN);$taskEnvErrCount+=$taskN}
                            $taskEnvSeen+=$taskN;$taskObservedUnretained-=$taskN
                        }else{
                            $taskTake=[int][Math]::Min([long]$taskN,8388608L-$script:taskLogBytes)
                            if($taskTake -gt 0){$taskSink=if($taskDirection -eq 0){$taskOutSink}else{$taskErrSink};$taskSink.Write($taskBuffer,0,$taskTake);$script:taskLogBytes+=$taskTake;$taskChildBytes+=$taskTake;$taskObservedUnretained-=$taskTake}
                            if($taskTake -ne $taskN){$taskLogCap=$true;$taskOmitted+=$taskN-$taskTake;throw 'Bounded combined raw child-log8MiB refusal; captured prefix retained'}
                        }
                        if($taskDirection -eq 0){$taskOutRead=$taskProcess.StandardOutput.BaseStream.ReadAsync($taskOutBuffer,0,4096,$taskReadCancel.Token);$taskOutAccounted=$false}else{$taskErrRead=$taskProcess.StandardError.BaseStream.ReadAsync($taskErrBuffer,0,4096,$taskReadCancel.Token);$taskErrAccounted=$false}
                    }
                }
            }
            if($taskWatch.Elapsed.TotalSeconds -gt $Seconds){$taskTimeout=$true;throw 'Bounded own-child timeout; captured prefixes retained'}
            [void]$taskProcess.WaitForExit(10)
        }
        $taskActualExit=$taskProcess.ExitCode;$taskChildExitObserved=$true
        if($CompilerEnvironment){
            if(($taskEnvOutCount -band 1) -ne 0){throw 'Compiler environment UTF16LE byte geometry'}
            $taskNames=@('INCLUDE','LIB','LIBPATH','PATH','VCToolsInstallDir','VCINSTALLDIR','WindowsSdkDir','WindowsSDKVersion','UniversalCRTSdkDir','UCRTVersion','VSINSTALLDIR','VCToolsVersion','VSCMD_ARG_TGT_ARCH','VSCMD_ARG_HOST_ARCH')
            $taskCopied=@();$taskBegin=0
            # At most32,768 UTF16 code units. One line/value copy at a time; no whole-text/split array.
            for($taskAt=0;$taskAt -le $taskEnvOutCount;$taskAt+=2){
                Spend-TaskUnits 2
                if($taskAt -eq $taskEnvOutCount -or ($taskEnvOut[$taskAt] -eq 10 -and $taskEnvOut[$taskAt+1] -eq 0)){
                    $taskCount=$taskAt-$taskBegin;if($taskCount -ge 2 -and $taskEnvOut[$taskAt-2] -eq 13 -and $taskEnvOut[$taskAt-1] -eq 0){$taskCount-=2}
                    $taskLine=[Text.Encoding]::Unicode.GetString($taskEnvOut,$taskBegin,$taskCount);$taskEqual=$taskLine.IndexOf('=')
                    if($taskEqual -gt 0 -and $taskEqual -le 32){$taskName=$taskLine.Substring(0,$taskEqual);if($taskName -in $taskNames){$taskValue=$taskLine.Substring($taskEqual+1);Set-Item -LiteralPath ('Env:'+$taskName) -Value $taskValue;$taskCopied+=@($taskName);$taskValue=$null}}
                    $taskLine=$null;$taskBegin=$taskAt+2
                }
            }
            [IO.File]::WriteAllText($Log,('exit='+$taskActualExit+"`nCopied allowlisted names: "+($taskCopied -join ', ')+"`nCaptured environment bytes="+$taskEnvSeen+"; no values written to evidence.`n"),[Text.UTF8Encoding]::new($false))
        }
    }catch{
        $taskFailure=if($CompilerEnvironment){'Compiler-environment failure; '+$_.Exception.GetType().FullName+'; values omitted'}else{$_.Exception.Message}
    }finally{
        # No unbounded wait, buffer clear or resource disposal while either owned read is pending.
        if($null -ne $taskReadCancel){try{$taskReadCancel.Cancel()}catch{$taskCleanupFaults+=@('read_cancel:'+ $_.Exception.GetType().FullName)}}
        if($taskStarted){
            try{if(-not $taskProcess.HasExited){$taskProcess.Kill($true);[void]$taskProcess.WaitForExit(2000)}
                if($taskProcess.HasExited){$taskActualExit=$taskProcess.ExitCode;$taskChildExitObserved=$true}else{$taskCleanupFaults+=@('own_child_exit_not_observed_within_2000ms')}
            }catch{$taskCleanupFaults+=@('own_child_cleanup:'+ $_.Exception.GetType().FullName)}
        }
        [Threading.Tasks.Task[]]$taskOwnedReads=@()
        if($null -ne $taskOutRead){$taskOwnedReads+=@($taskOutRead)};if($null -ne $taskErrRead){$taskOwnedReads+=@($taskErrRead)}
        if($taskOwnedReads.Length -gt 0){try{[void][Threading.Tasks.Task]::WaitAll($taskOwnedReads,2000)}catch{$taskCleanupFaults+=@('read_wait:'+ $_.Exception.GetType().FullName)}}
        foreach($taskDirection in 0,1){
            $taskRead=$null;$taskAccounted=$true
            if($taskDirection -eq 0){$taskRead=$taskOutRead;$taskAccounted=$taskOutAccounted}else{$taskRead=$taskErrRead;$taskAccounted=$taskErrAccounted}
            if($null -ne $taskRead){
                if(-not $taskRead.IsCompleted){$taskReadsSettled=$false;$taskPendingReads++}elseif(-not $taskAccounted){
                    # Settled cleanup bytes are observed omissions, not claimed written/captured data.
                    try{$taskN=$taskRead.GetAwaiter().GetResult();$taskObservedBytes+=$taskN;$taskObservedUnretained+=$taskN
                        if($taskN -eq 0){if($taskDirection -eq 0){$taskOutDone=$true}else{$taskErrDone=$true}}
                    }catch{$taskCleanupFaults+=@('settled_read_'+$taskDirection+':'+ $_.Exception.GetType().FullName)}
                    if($taskDirection -eq 0){$taskOutAccounted=$true}else{$taskErrAccounted=$true}
                }
            }
        }
        # Flush any previously written prefix even when a read cannot be settled. No further writes follow.
        foreach($taskSink in @($taskOutSink,$taskErrSink)){if($null -ne $taskSink){try{$taskSink.Flush()}catch{$taskCleanupFaults+=@('prefix_flush:'+ $_.Exception.GetType().FullName)}}}
        if($taskReadsSettled){
            [Array]::Clear($taskOutBuffer,0,$taskOutBuffer.Length);[Array]::Clear($taskErrBuffer,0,$taskErrBuffer.Length)
            if($null -ne $taskEnvOut){[Array]::Clear($taskEnvOut,0,$taskEnvOut.Length)};if($null -ne $taskEnvErr){[Array]::Clear($taskEnvErr,0,$taskEnvErr.Length)}
            # Each independent disposal is attempted; a failure cannot skip the other owner.
            foreach($taskDisposable in @($taskOutSink,$taskErrSink,$taskReadCancel,$taskProcess)){if($null -ne $taskDisposable){try{$taskDisposable.Dispose()}catch{$taskCleanupFaults+=@('settled_dispose:'+ $_.Exception.GetType().FullName)}}}
        }else{
            # One bounded rooted owner, never serialized. Do not erase active storage or dispose its owner.
            $script:taskUnsettledReadOwner=@{process=$taskProcess;cancel=$taskReadCancel;stdout_read=$taskOutRead;stderr_read=$taskErrRead;stdout_buffer=$taskOutBuffer;stderr_buffer=$taskErrBuffer;environment_out=$taskEnvOut;environment_err=$taskEnvErr;stdout_sink=$taskOutSink;stderr_sink=$taskErrSink}
            $taskCleanupFaults+=@('raw_reads_not_settled_within_2000ms_owned_storage_retained_no_more_children')
        }
        if($taskCleanupFaults.Count -gt 0){if($null -eq $taskFailure){$taskFailure='Bounded child cleanup refusal; see value-free cleanup diagnostics'}else{$taskFailure+='; bounded cleanup diagnostics retained'}}
        $taskUnknownUndrained=$taskStarted -and -not($taskOutDone -and $taskErrDone)
        $taskReceipt.children+=@([ordered]@{file=[IO.Path]::GetFileName($File);raw_exit_code=$taskActualExit;raw_exit_observed=$taskChildExitObserved;timed_out=$taskTimeout;log_cap_refusal=$taskLogCap;raw_file_bytes=$taskChildBytes;environment_capture_bytes=$taskEnvSeen;bytes_observed_read=$taskObservedBytes;bytes_observed_not_confirmed_retained=$taskObservedUnretained;bytes_omitted_at_cap=$taskOmitted;unknown_undrained_bytes=$taskUnknownUndrained;both_streams_reached_eof=($taskOutDone -and $taskErrDone);read_settlement_complete=$taskReadsSettled;pending_raw_reads=$taskPendingReads;read_settlement_timeout_ms=2000;own_child_exit_timeout_ms=2000;owned_storage_quarantined=(-not $taskReadsSettled);cleanup_diagnostics=$taskCleanupFaults;failure=$taskFailure})
        if(-not $CompilerEnvironment -or $null -ne $taskFailure){
            $taskSummary='exit='+$taskActualExit+'; timeout='+$taskTimeout+'; cap_refusal='+$taskLogCap+'; raw_file_bytes='+$taskChildBytes+'; env_capture_bytes='+$taskEnvSeen+'; observed_not_confirmed_retained='+$taskObservedUnretained+'; unknown_undrained='+$taskUnknownUndrained+'; pending_reads='+$taskPendingReads+'; failure='+$taskFailure+"`n"
            [IO.File]::WriteAllText($Log,($taskSummary+'Normal stdout/stderr prefixes are separate raw-byte files; environment values are never written to evidence.'+"`n"),[Text.UTF8Encoding]::new($false))
        }
    }
    if($null -ne $taskFailure){throw $taskFailure};return $taskActualExit
}
Assert-TaskPins;Assert-TaskStorage
$taskRunsPath=Join-Path $taskRevision 'runs'
Spend-TaskUnits 4096
$taskExistingRuns=@();if(Test-Path -LiteralPath $taskRunsPath){$taskExistingRuns=@(Get-ChildItem -LiteralPath $taskRunsPath -Directory)}
if($Stage -ceq 'Freeze' -and $taskExistingRuns.Count -ne 0){throw 'Single Freeze only; retained failure/success cannot be silently retried'}
$taskPrepared=$null;$taskPrior=$null
if($Stage -ceq 'CompileAndRun'){
    if(-not $true){throw 'SOURCE_HOLD_FRESH_C1_CONTEXT_COST_AND_FREEZE_OUTPUT_REVIEW_REQUIRED_NO_COMPILER_AUTHORITY'}
    if(-not $PreparedRun -or $taskExistingRuns.Count -ne 1){throw 'Use the sole separately reviewed successful Freeze run'}
    $taskPrepared=[IO.Path]::GetFullPath($PreparedRun)
    if([IO.Path]::GetDirectoryName($taskPrepared) -cne (Join-Path $taskRevision 'runs')){throw 'Prepared run must be a direct revision runs child'}
    $taskPrior=Get-Content -LiteralPath (Join-Path $taskPrepared 'execution-receipt.json') -Raw|ConvertFrom-Json
    if($taskPrior.stage -cne 'Freeze' -or $taskPrior.status -cne 'oracles_and_reversible_copies_prepared_candidate_unrun' -or $taskPrior.freeze_exit -ne 0 -or $taskPrior.prepare_exit -ne 0 -or $taskPrior.authored_manifest_sha256 -cne $taskManifestHash){throw 'Prepared run/source revision is not the successful reviewed one'}
    foreach($taskPin in $taskPrior.prepared_hashes){if((Get-TaskSha256 (Join-Path $taskPrepared $taskPin.path)) -ine $taskPin.sha256){throw 'Prepared source/frozen data changed'}}
}
$taskRun=Join-Path $taskRevision ('runs/'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fffffff'))
if(Test-Path -LiteralPath $taskRun){throw 'Never overwrite a run'}
New-Item -ItemType Directory -Path $taskRun|Out-Null
Copy-Item -LiteralPath $taskManifestPath -Destination (Join-Path $taskRun 'authored-manifest.json')
$taskReceiptPath=Join-Path $taskRun 'execution-receipt.json'
$taskReceipt=[ordered]@{stage=$Stage;status='stage_not_started';started_utc=[DateTime]::UtcNow.ToString('o');authored_manifest_sha256=$taskManifestHash;prepared_run=$PreparedRun;freeze_exit=$null;prepare_exit=$null;compile_exit=$null;candidate_exit=$null;verify_exit=$null;production_accepted=$false;universal_context_certified=$false;waveform_run=$false;resource_certificate=$false;public_pcm_samples=0;native_app_run=$false;desktop_input=$false;audio_device_run=$false;hardware_run=$false;children=@()}
function Write-TaskReceipt {$script:taskReceiptWrites++;if($script:taskReceiptWrites -gt 32){throw 'Prepaid receipt inventory'};$taskReceipt['runner_reserved_work_this_invocation']=1048576L;$taskReceipt['runner_reserved_work_both_invocations']=2097152L;$taskReceipt['runner_counted_work']=$taskRunnerUnits;$taskReceipt['prior_runner_counted_work']=if($null -ne $taskPrior){[long]$taskPrior.runner_counted_work}else{0L};$taskReceipt['runner_counted_work_cumulative']=$taskRunnerUnits+$taskReceipt.prior_runner_counted_work;[IO.File]::WriteAllText($taskReceiptPath,(($taskReceipt|ConvertTo-Json -Depth 14).Replace("`r`n","`n")+"`n"),[Text.UTF8Encoding]::new($false))}
Write-TaskReceipt
try {
    if($Stage -ceq 'Freeze'){
        if($PreparedRun){throw 'Freeze takes no earlier run'}
        $taskReceipt.status='freezing_six_independent_C1_source_and_phase_programs';Write-TaskReceipt
        $taskReceipt.freeze_exit=Invoke-TaskChild 'python.exe' @('-B',(Join-Path $taskRevision 'freeze_context.py'),'--output',(Join-Path $taskRun 'frozen')) (Join-Path $taskRun 'freeze.log') 300
        if($taskReceipt.freeze_exit -ne 0){throw 'Freeze failed; candidate remains uncompiled/unrun'}
        $taskReceipt.prepare_exit=Invoke-TaskChild 'python.exe' @('-B',(Join-Path $taskRevision 'prepare_observer.py'),'--output',(Join-Path $taskRun 'observer')) (Join-Path $taskRun 'prepare.log') 120
        if($taskReceipt.prepare_exit -ne 0){throw 'Two-stage exact observer preparation failed'}
        $taskFrozen=Get-Content -LiteralPath (Join-Path $taskRun 'frozen/freeze-receipt.json') -Raw|ConvertFrom-Json
        foreach($taskCase in $taskFrozen.fixtures){if((Get-TaskBytes (Join-Path $taskRun ('frozen/'+$taskCase.name))) -gt 32MB){throw 'MusicalResourceLimit: case frozen payload exceeds reserved32MiB'}}
        if((Get-TaskBytes (Join-Path $taskRun 'observer')) -gt 4MB){throw 'MusicalResourceLimit: reversible source artifacts exceed4MiB'}
        Spend-TaskUnits 8192
        $taskReceipt['prepared_hashes']=@(Get-ChildItem -LiteralPath $taskRun -Recurse -File|Where-Object{$_.DirectoryName -cne $taskRun}|ForEach-Object{[ordered]@{path=[IO.Path]::GetRelativePath($taskRun,$_.FullName).Replace('\','/');sha256=(Get-TaskSha256 $_.FullName)}})
        $taskReceipt.status='oracles_and_reversible_copies_prepared_candidate_unrun'
    } else {
        $taskFrozen=Join-Path $taskPrepared 'frozen';$taskObserver=Join-Path $taskPrepared 'observer';$taskDependency=Join-Path $taskRepo 'build-ci/_deps/signalsmith_stretch-src'
        $taskFreezeReceipt=Get-Content -LiteralPath (Join-Path $taskFrozen 'freeze-receipt.json') -Raw|ConvertFrom-Json
        $taskSourceReceipt=Get-Content -LiteralPath (Join-Path $taskObserver 'preparation-receipt.json') -Raw|ConvertFrom-Json
        if($taskFreezeReceipt.freeze_work -gt 3000000 -or $taskSourceReceipt.prepare_work -gt 6000000 -or $taskFreezeReceipt.fixtures.Count -ne 6){throw 'Frozen whole-matrix source bound differs'}
        $taskPreparedWork=[long]$taskFreezeReceipt.freeze_work+[long]$taskSourceReceipt.prepare_work+2097152L
        if($taskPreparedWork -gt 11097152L){throw 'Combined prepaid preparation/runner ceiling'}
        $taskConsumed=Join-Path $taskPrepared 'compile-consumed.json'
        $taskConsumeStream=[IO.File]::Open($taskConsumed,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        try{$taskConsumeBytes=[Text.Encoding]::UTF8.GetBytes(('{"source_manifest_sha256":"'+$taskManifestHash+'","single_attempt":true}'+"`n"));$taskConsumeStream.Write($taskConsumeBytes,0,$taskConsumeBytes.Length)}finally{$taskConsumeStream.Dispose()}
        $taskEnv=Invoke-TaskChild $env:ComSpec @('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat') (Join-Path $taskRun 'compiler-environment.log') 60 $true
        if($taskEnv -ne 0){throw 'MSVC environment setup failed'}
        $taskCompiler=(Get-Command 'cl.exe' -CommandType Application -ErrorAction Stop).Source
        if([IO.Path]::GetFullPath($taskCompiler) -cne [IO.Path]::GetFullPath('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe')){throw 'Resolved compiler path differs from pinned compiler'}
        $taskCompilerHash=(Get-TaskSha256 $taskCompiler)
        if($taskCompilerHash -cne '88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D'){throw 'UnsupportedArithmetic: reviewed compiler artifact differs'}
        $taskReceipt['compiler_path']=$taskCompiler;$taskReceipt['compiler_sha256']=$taskCompilerHash;$taskReceipt['compiler_file_version']=(Get-Item -LiteralPath $taskCompiler).VersionInfo.FileVersion
        $taskExe=Join-Path $taskRun 'physical-c1-context.exe';$taskCandidate=Join-Path $taskRun 'candidate';New-Item -ItemType Directory -Path $taskCandidate|Out-Null
        $taskArgs=@('/nologo','/std:c++20','/EHsc','/O2','/fp:strict','/W4','/DNDEBUG','/D_USE_MATH_DEFINES',('/I'+$taskObserver),('/I'+(Join-Path $taskRepo 'src')),('/I'+$taskDependency),('/I'+(Join-Path $taskDependency 'dsp')),('/Fe'+$taskExe),('/Fo'+(Join-Path $taskRun 'physical-c1-context.obj')),(Join-Path $taskRevision 'context_probe.cpp'),'/link','/SUBSYSTEM:CONSOLE')
        $taskReceipt['compile_arguments']=$taskArgs;$taskReceipt.status='compiling_fresh_console_artifact';Write-TaskReceipt
        try{$taskReceipt.compile_exit=Invoke-TaskChild $taskCompiler $taskArgs (Join-Path $taskRun 'compile.log') 180}finally{$taskReceipt['compiler_sha256_after_compile']=(Get-TaskSha256 $taskCompiler);if($taskReceipt.compiler_sha256_after_compile -cne $taskCompilerHash){throw 'Compiler bytes changed during compile'}}
        if($taskReceipt.compile_exit -ne 0){throw 'Fresh physical C1 console compilation failed'}
        # Read only a bounded PE prefix; no retained executable bytearray.
        Spend-TaskUnits 4096
        $taskPeStream=[IO.FileStream]::new($taskExe,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read,4096,[IO.FileOptions]::RandomAccess)
        $taskPeBuffer=[byte[]]::new(256)
        try{
            if($taskPeStream.Length -lt 128 -or $taskPeStream.Length -gt 8388608L){throw 'Preflight linked executable size'}
            if($taskPeStream.Read($taskPeBuffer,0,64) -ne 64 -or $taskPeBuffer[0] -ne 77 -or $taskPeBuffer[1] -ne 90){throw 'PE DOS prefix'}
            $taskOffset=[BitConverter]::ToInt32($taskPeBuffer,60)
            if($taskOffset -lt 64 -or $taskOffset -gt $taskPeStream.Length-94){throw 'PE header position'}
            [void]$taskPeStream.Seek($taskOffset,[IO.SeekOrigin]::Begin)
            if($taskPeStream.Read($taskPeBuffer,0,94) -ne 94 -or [BitConverter]::ToUInt32($taskPeBuffer,0) -ne 0x4550 -or [BitConverter]::ToUInt16($taskPeBuffer,4) -ne 0x8664 -or [BitConverter]::ToUInt16($taskPeBuffer,20) -lt 70 -or [BitConverter]::ToUInt16($taskPeBuffer,24) -ne 0x20b -or [BitConverter]::ToUInt16($taskPeBuffer,92) -ne 3){throw 'Actual x64 console PE prerequisite'}
        }finally{$taskPeStream.Dispose()}
        if((Get-TaskBytes $taskRun) -gt 8MB){throw 'MusicalResourceLimit: pre-candidate build/log envelope exceeds8MiB'}
        $taskReceipt['executable_sha256']=(Get-TaskSha256 $taskExe);$taskReceipt.status='running_one_complete_six_C1_case_private_matrix_and_quarantined_controls';Write-TaskReceipt
        try{$taskReceipt.candidate_exit=Invoke-TaskChild $taskExe @($taskFrozen,$taskCandidate,[string]$taskPreparedWork) (Join-Path $taskRun 'candidate.log') 300}finally{$taskReceipt['executable_sha256_after_candidate']=(Get-TaskSha256 $taskExe);if($taskReceipt.executable_sha256_after_candidate -cne $taskReceipt.executable_sha256){throw 'Executable changed during candidate'}}
        if($taskReceipt.candidate_exit -ne 0){throw 'Private matrix failed/refused; retain failure without retry'}
        try{$taskReceipt.verify_exit=Invoke-TaskChild 'python.exe' @('-B',(Join-Path $taskRevision 'verify_context.py'),'--frozen',$taskFrozen,'--candidate',$taskCandidate) (Join-Path $taskRun 'verify.log') 300}finally{$taskReceipt['executable_sha256_after_verifier']=(Get-TaskSha256 $taskExe);if($taskReceipt.executable_sha256_after_verifier -cne $taskReceipt.executable_sha256){throw 'Executable changed during verifier'}}
        if($taskReceipt.verify_exit -ne 0){throw 'Independent C1/context/center/control verification failed or refused'}
        foreach($taskCase in $taskFreezeReceipt.fixtures){
            $taskFrozenBytes=Get-TaskBytes (Join-Path $taskFrozen $taskCase.name);$taskActualBytes=Get-TaskBytes (Join-Path $taskCandidate $taskCase.name)
            if($taskFrozenBytes+$taskActualBytes+16MB -gt 512MB){throw ('MusicalResourceLimit: full case payload '+$taskCase.name)}
        }
        foreach($taskPin in $taskPrior.prepared_hashes){if((Get-TaskSha256 (Join-Path $taskPrepared $taskPin.path)) -ine $taskPin.sha256){throw 'Candidate altered frozen source/oracles'}}
        $taskReceipt.status='six_request_bound_C1_cases_checked_quiet_refusal_preserved_no_production_admission'
    }
    Assert-TaskPins;Assert-TaskStorage
    Spend-TaskUnits 8192
    $taskReceipt['artifact_hashes']=@(Get-ChildItem -LiteralPath $taskRun -Recurse -File|Where-Object{$_.FullName -cne $taskReceiptPath}|ForEach-Object{[ordered]@{path=[IO.Path]::GetRelativePath($taskRun,$_.FullName).Replace('\','/');sha256=(Get-TaskSha256 $_.FullName)}})
    $taskReceipt['parent_reserved_work']=$taskReservedWork;$taskReceipt['parent_reserved_payload']=$taskReservedPayload;$taskReceipt['runner_reserved_work_this_invocation']=1048576L;$taskReceipt['runner_reserved_work_both_invocations']=2097152L;$taskReceipt['runner_counted_work']=$taskRunnerUnits;$taskReceipt['child_log_bytes']=$taskLogBytes;$taskReceipt['prior_runner_counted_work']=if($null -ne $taskPrior){$taskPrior.runner_counted_work}else{0};$taskReceipt['runner_counted_work_cumulative']=$taskRunnerUnits+$taskReceipt.prior_runner_counted_work
    $taskReceipt['finished_utc']=[DateTime]::UtcNow.ToString('o');Write-TaskReceipt
    Write-Output ('Retained limited physical-C1 receipt: '+$taskReceiptPath);exit 0
} catch {
    $taskReceipt.status='failed_or_precise_refusal';$taskReceipt['failure']=$_.Exception.Message;$taskReceipt['finished_utc']=[DateTime]::UtcNow.ToString('o');Write-TaskReceipt
    Write-Output ('Retained failure receipt: '+$taskReceiptPath);throw
}
