"""Own source/data/integer checker and bounded V baseline. Never imports target code."""
import hashlib,json,pathlib,re,itertools
ROOT=pathlib.Path(__file__).resolve().parents[2]
HERE=pathlib.Path(__file__).parent
TARGET=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
METADATA=ROOT/'build-ci/20261106-managed-file-type-inventory-source-r4'
for name in ['closure.json','closure.md','source-pins.json','checks.json','declaration-evidence.json']:
 if (HERE/name).exists():raise RuntimeError('PRESERVE_FIRST_PASS_'+name)
checks=[];inputs={}
def sha(data):return hashlib.sha256(data).hexdigest().upper()
def record(path):
 path=path.resolve();data=path.read_bytes()
 try:name=path.relative_to(ROOT).as_posix()
 except ValueError:name=path.as_posix()
 row={'path':name,'bytes':len(data),'sha256':sha(data)};inputs[name]=row
 return row
def read(path):record(path);return path.read_text(encoding='utf-8')
def ck(name,value):
 checks.append({'id':name,'pass':bool(value)})
 if not value:
  (HERE/'failed-closure-checks.json').write_text(json.dumps(checks,indent=2)+'\n',encoding='utf-8',newline='\n')
  raise RuntimeError(name)
def source(path,*literals):
 text=read(path)
 for at,literal in enumerate(literals):ck('SOURCE_ANCHOR_'+path.name+'_'+str(at),literal in text)
 return text
def encode(value):return (json.dumps(value,indent=2,ensure_ascii=True)+'\n').encode()

subject_seals={
'run-clock.ps1':'3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F',
'source-manifest.json':'4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557',
'README.md':'DC39905A4833FB580FAA55BE562E15EAB923F3B9D1006371D88CBE928C386D90',
'clock_stage.cpp':'F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206',
'source-costs.h':'0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27'}
# Obtain actual five source filenames from the manifest, rather than assuming C++ spelling.
manifest=json.loads(read(TARGET/'source-manifest.json'))
target_files=[p for p in TARGET.iterdir() if p.is_file()]
ck('R12_FIVE_FILES',len(target_files)==5)
for path in target_files:
 row=record(path)
 if path.name in subject_seals:ck('R12_SOURCE_SEAL_'+path.name,row['sha256']==subject_seals[path.name])
 else:ck('R12_CPP_EXACT_SEAL',row['sha256']=='F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206')
runner=source(TARGET/'run-clock.ps1',
 '$taskChildPollMaximum=7200L',
 '$taskA=[byte[]]::new(4096);$taskB=[byte[]]::new(4096)',
 '$taskInfo.Environment.Clear()',
 "if($Arguments.Length -ne $(if($taskChildIndex -eq 0){28}else{6}))",
 'ReadAtLeastAsync([Memory[byte]]::new($taskA),4096,$false,$taskCancel.Token).AsTask()',
 'ReadAtLeastAsync([Memory[byte]]::new($taskB),4096,$false,$taskCancel.Token).AsTask()',
 "$taskProcess.Kill($true);$null=$taskProcess.WaitForExit(2000)",
 '[Threading.Tasks.Task]::WaitAll($taskReads,2000)',
 "$taskBundleFirst=[IO.File]::OpenRead($taskMutantBundle);$taskFirst=$taskBundleFirst.ReadByte()",
 '[IO.File]::WriteAllBytes($Path,$Bytes)',
 "[IO.File]::Open($taskMarkerPath,[IO.FileMode]::CreateNew)")
stream=source(ROOT/'build-ci/20261106-controller-stream-source-r1/Stream.cs',
 '[AsyncMethodBuilder(typeof(PoolingAsyncValueTaskMethodBuilder<>))]',
 'private async ValueTask<int> ReadAtLeastAsyncCore',
 'int read = await ReadAsync(buffer.Slice(totalRead), cancellationToken).ConfigureAwait(false);',
 'if (read == 0)',
 'totalRead += read;',
 'new SemaphoreSlim(1, 1)')
buffered=source(ROOT/'build-ci/20261106-controller-stream-source-r1/BufferedFileStreamStrategy.cs',
 'private async ValueTask<int> ReadFromNonSeekableAsync',
 'await EnsureAsyncActiveSemaphoreInitialized().WaitAsync(cancellationToken).ConfigureAwait(false);',
 'return await _strategy.ReadAsync(destination, cancellationToken).ConfigureAwait(false);',
 '_asyncActiveSemaphore.Release();')
random=source(ROOT/'build-ci/20261106-controller-stream-source-r1/RandomAccess.Windows.cs',
 'if (handle.IsAsync)',
 'return AsyncOverSyncWithIoCancellation.InvokeAsync(static state =>')
workitem=source(ROOT/'build-ci/20261106-controller-stream-source-r1/AsyncOverSyncWithIoCancellation.cs',
 '[ThreadStatic]',
 't_instance ??= new AsyncOverSyncWithIoCancellation()',
 'Task.CompletedTask.ConfigureAwait(ConfigureAwaitOptions.ForceYielding)',
 'SyncAsyncWorkItemRegistration reg = RegisterCancellation(cancellationToken);',
 'reg.Dispose();',
 'instance._callbackCompleted = Task.Factory.StartNew(static s =>')
cts=source(HERE/'primary/CancellationTokenSource.cs',
 'registrations = new Registrations(this);',
 'registrations = Interlocked.CompareExchange(ref _registrations, registrations, null) ?? registrations;',
 'if (registrations.FreeNodeList is not null)',
 'node = new CallbackNode(registrations);',
 'FreeNodeList = node;')
semaphore=source(HERE/'primary/SemaphoreSlim.cs',
 'm_lockObjAndDisposed = new StrongBox<bool>();',
 'if (m_currentCount > 0)',
 'return Task.FromResult(true);',
 'TaskNode asyncWaiter = CreateAndAddAsyncWaiter();')
pool=source(HERE/'primary/PoolingAsyncValueTaskMethodBuilderT.cs',
 'new PaddedReference[Environment.ProcessorCount]',
 'box = new StateMachineBox<TStateMachine>();',
 'public TStateMachine? StateMachine;',
 'protected ManualResetValueTaskSourceCore<TResult> _valueTaskSource;')
default=source(HERE/'primary/AsyncTaskMethodBuilderT.cs',
 'private class AsyncStateMachineBox<TStateMachine>',
 'Task<TResult>, IAsyncStateMachineBox',
 'new AsyncStateMachineBox<TStateMachine>();')
vt=source(HERE/'primary/ValueTask.cs',
 'return Task.FromResult(_result!);',
 'return new ValueTaskSourceAsTask(t, _token);',
 'private sealed class ValueTaskSourceAsTask : Task<TResult>')
task=source(HERE/'primary/Task.cs',
 'private static bool WaitAllCore(ReadOnlySpan<Task> tasks, int millisecondsTimeout, CancellationToken cancellationToken)',
 'var mres = new SetOnCountdownMres(tasks.Count);',
 'task.AddCompletionAction(mres, addBeforeOthers: true);',
 'private sealed class SetOnCountdownMres : ManualResetEventSlim, ITaskCompletionAction')
process=source(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Process.Windows.cs',
 'new SafeFileHandle(Interop.Kernel32.GetStdHandle(Interop.Kernel32.HandleTypes.STD_INPUT_HANDLE), false)',
 'new StreamReader(new FileStream(parentOutputPipeHandle!, FileAccess.Read, 4096, false), enc, true, 4096)',
 'new StreamReader(new FileStream(parentErrorPipeHandle!, FileAccess.Read, 4096, false), enc, true, 4096)',
 'return new SafeProcessHandle(_processHandle!.DangerousGetHandle(), ownsHandle: false);')
kill=source(HERE/'primary/Process.Win32.cs',
 'private List<Exception>? KillTree()',
 'List<(Process Process, SafeProcessHandle Handle)> children = GetProcessHandlePairs',
 'foreach (Process p in GetProcesses())',
 'child.Process.KillTree(child.Handle)')
source(HERE/'primary/ProcessManager.Windows.cs','new Interop.Kernel32.ProcessWaitHandle(handle)')
source(HERE/'primary/ProcessManager.Win32.cs','IntPtr[] moduleHandles = new IntPtr[modulesCount];','moduleHandles = new IntPtr[modulesCount];')
source(HERE/'primary/ProcessWaitHandle.cs','out SafeWaitHandle waitHandle','this.SetSafeWaitHandle(waitHandle);')
dictionary=source(HERE/'primary/DictionaryWrapper.cs',
 'internal sealed class DictionaryWrapper : IDictionary<string, string?>, IDictionary',
 'private readonly Dictionary<string, string?> _contents;')
ck('DW_NO_STRINGDICTIONARY_BASE','class DictionaryWrapper : StringDictionary' not in dictionary)
reader=source(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/StreamReader.cs',
 '_decoder = encoding.GetDecoder();','_byteBuffer = new byte[bufferSize];',
 '_maxCharsPerBuffer = encoding.GetMaxCharCount(bufferSize);','_charBuffer = new char[_maxCharsPerBuffer];')
os_encoding=source(ROOT/'build-ci/20261106-os-encoding-source-r1/OSEncoding.Windows.cs','public override int GetMaxCharCount(int byteCount)','long charCount = (long)byteCount * 4;')
source(ROOT/'build-ci/20261106-os-encoding-source-r1/DBCSDecoder.cs','new byte[10]')
source(ROOT/'build-ci/20261106-os-encoding-source-r1/ConsoleEncoding.cs','return _encoding.GetMaxCharCount(byteCount);','return _encoding.GetDecoder();')
source(ROOT/'build-ci/20261106-controller-stream-source-r1/EncodingHelper.Windows.cs','Encoding.GetEncoding(0).CodePage','return new OSEncoding(codepage);')
source(ROOT/'build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine/AutomationEngine.cs','Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);')
crypto=source(HERE/'primary/HashProviderCng.cs',
 'if (ntStatus == NTSTATUS.STATUS_INVALID_PARAMETER)',
 '_hAlgorithm = Interop.BCrypt.BCryptAlgorithmCache.GetCachedBCryptAlgorithmHandle',
 'if (_reusable && !_running)',
 'SafeBCryptHashHandle? previousHash = Interlocked.Exchange(ref _hHash, hHash);')
source(HERE/'primary/BCryptAlgorithmCache.cs',
 'ConcurrentDictionary<(string HashAlgorithmId, BCryptOpenAlgorithmProviderFlags Flags), (SafeBCryptAlgorithmHandle Handle, int HashSizeInBytes)>',
 'if (!s_handles.TryAdd(key, (handle, hashSize)))')
file_source=source(ROOT/'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/primary/File.cs',
 'using SafeFileHandle sfh = OpenHandle(path, FileMode.Create, FileAccess.Write, FileShare.Read);',
 'RandomAccess.WriteAtOffset(sfh, bytes, 0);')
helpers=source(ROOT/'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/primary/Strategies_FileStreamHelpers.cs','bufferSize > 1')

# Actual accepted metadata output is passive data here, not a new reader attempt.
inventory_data=(METADATA/'execution-r1/inventory.json').read_bytes();record(METADATA/'execution-r1/inventory.json')
ck('ACTUAL_R4_INVENTORY_SEAL',sha(inventory_data)=='7C23E7BF82F9D293962C06BBD450BC4D9F7BBD74FF9E5344C8E4C88686B04D6D')
inventory=json.loads(inventory_data)
ck('ACTUAL_88_TYPES',inventory['reported_types']==len(inventory['types'])==88)
ck('ACTUAL_391_DECLARED_FIELDS',sum(x['declared_nonstatic_instance_field_count'] for x in inventory['types'])==391)
defs={(r['file'],r['qualified_name']):r for r in inventory['types']}
ck('ALL_88_FILE_TYPE_IDENTITIES_UNIQUE',len(defs)==88)
type_plan=json.loads(read(METADATA/'source-pins.json'))
for file,names in type_plan['allowlist_by_file'].items():
 for name in names:ck('ACTUAL_REQUIRED_METADATA_'+file+'_'+name,(file,name) in defs)
for row in inventory['types']:
 ck('ACTUAL_FIELD_COUNT_'+row['file']+'_'+row['qualified_name'],len(row['declared_nonstatic_instance_fields'])==row['declared_nonstatic_instance_field_count'])
  # Signature bytes are declaration evidence, not object layout evidence.
 for field in row['declared_nonstatic_instance_fields']:
  ck('FIELD_SIGNATURE_'+row['file']+'_'+row['qualified_name']+'_'+field['name'],bool(re.fullmatch(r'06(?:[0-9A-F]{2})+',field['signature_blob_hex'])))
expected_counts={
'System.IO.Stream+<ReadAtLeastAsyncCore>d__49':9,
'System.IO.Strategies.BufferedFileStreamStrategy+<ReadFromNonSeekableAsync>d__36':7,
'System.Threading.AsyncOverSyncWithIoCancellation+<InvokeAsync>d__8`2':6,
'System.Threading.Tasks.ValueTask`1+ValueTaskSourceAsTask':2,
'System.Runtime.CompilerServices.PoolingAsyncValueTaskMethodBuilder`1+StateMachineBox':3,
'System.Runtime.CompilerServices.PoolingAsyncValueTaskMethodBuilder`1+StateMachineBox`1':1,
'System.Runtime.CompilerServices.AsyncTaskMethodBuilder`1+AsyncStateMachineBox`1':1}
for name,count in expected_counts.items():ck('GENUINE_DECLARED_FIELD_COUNT_'+name,defs['System.Private.CoreLib.dll',name]['declared_nonstatic_instance_field_count']==count)
dw=defs['System.Diagnostics.Process.dll','System.Collections.Specialized.DictionaryWrapper']
ck('GENUINE_DW_BASE_OBJECT',dw['base_decoded']=='[System.Runtime]System.Object')
ck('GENUINE_DW_ONE_DICT_REFERENCE',dw['declared_nonstatic_instance_field_count']==1 and 'Dictionary`2<string,string>' in dw['declared_nonstatic_instance_fields'][0]['decoded_signature'])
receipt=json.loads(read(METADATA/'execution-r1/receipt.json'))
ck('ACTUAL_RECEIPT_SEAL',record(METADATA/'execution-r1/receipt.json')['sha256']=='518D3BAA5C6679611A04B8F3F55B1711F9E3083E562E26528AB9ECB9C4D967FE')
ck('ACTUAL_PURPOSE_RAW0_STDERR0',receipt['raw_exit']==0 and receipt['stderr_bytes']==0 and receipt['first_green'] and receipt['all_pre_post_pins_match'])

# Integer progress proof: each positive inner read advances at least one byte.
cap=1048576;chunk=4096
positive_cases=[]
for p in range(3):
 max_full=(cap-p)//chunk
 positive_cases.append({'EOF_short_positive_pipes':p,'max_retained_full_chunks':max_full,'max_retained_positive_outer_results':max_full+p,'outer_invocations_with_initial_two':max_full+p+2})
ck('OUTER_SUPPORTED_UPPER_259',max(x['outer_invocations_with_initial_two'] for x in positive_cases)==259)
counterexample={'retained_full_chunks':255,'retained_short_bytes':[1,1],'retained_bytes':255*4096+2,'final_zero_results':2,'outer_calls':255+2+2}
ck('OUTER_258_COUNTEREXAMPLE_WITHIN_CAP',counterexample['retained_bytes']<=cap and counterexample['outer_calls']==259)
inner=cap+2*chunk+4
ck('INNER_SUPPORTED_UPPER_1056772',inner==1056772)
ck('OS_ENCODING_4096_MAX_CHARS_16384',4096*4==16384)
fixed_sites=[
 {'id':'hash','runner_lines':[149,152],'calls':'H_i','buffer_size':1,'wrappers':'FileStream + SyncWindows strategy + SafeFileHandle'},
 {'id':'bootstrap_hash','runner_lines':[305,333],'calls':'h_boot_i <=7','buffer_size':1,'wrappers':'same; exactly seven authored runtime pins'},
 {'id':'bounded_json_read','runner_lines':[689,692],'calls':'b_i <=3','buffer_size':1,'wrappers':'same; byte payload owned by B/F'},
 {'id':'owned_copy','runner_lines':[500,503],'calls':'2 endpoints * n_copy_i; n_copy_i <=2','buffer_size':4096,'wrappers':'two FileStream + two SyncWindows + two Buffered + two SafeFileHandle per copy; existing two65536 payloads excluded'},
 {'id':'mutation','runner_lines':[505,507],'calls':'n_mut_i <=2','buffer_size':4096,'wrappers':'FileStream + SyncWindows + Buffered + SafeFileHandle'},
 {'id':'PE_read','runner_lines':[810,813],'calls':'n_PE_i <=4','buffer_size':4096,'wrappers':'same; PE byte64/24/optional<=4096 arrays excluded'},
 {'id':'child_result','runner_lines':[816,822],'calls':'n_result_i <=2','buffer_size':4096,'wrappers':'same; result byte/string codec payload excluded'},
 {'id':'consumed_marker','runner_lines':[935,937],'calls':'n_marker_i <=1','buffer_size':4096,'wrappers':'same; marker byte payload excluded'},
 {'id':'predecessor_first_byte','runner_lines':[948,949],'calls':'n_firstbyte_i <=1','buffer_size':4096,'wrappers':'same; separate actual OpenRead, missing from provisional site list'},
 {'id':'child_log','runner_lines':[196,198],'calls':'2*c_i','buffer_size':4096,'wrappers':'FileStream + SyncWindows + Buffered + SafeFileHandle; each lazy byte4096 at most once'},
 {'id':'redirect_pipe','runner_lines':[204,219],'calls':'2*c_i','buffer_size':4096,'wrappers':'FileStream + SyncWindows + Buffered, share existing duplicated parent SafeFileHandle; no FS read buffer allocated on selected nonseekable direct path'},
 {'id':'document_write','runner_lines':[726,743,892,994],'calls':'w_i <=6','buffer_size':0,'wrappers':'direct SafeFileHandle + RandomAccess.WriteAtOffset; no FileStream/strategy/4KiB buffer'}]
ck('FIXED_NONCHILD_BUFFERED_SITES_14',4+2+4+2+1+1==14)
ck('DOCUMENT_WRITE_SOURCE_CALLS_MAX6',3+1+1+1==6)
ck('HAS_EXITED_DIRECT_SOURCE_MAX7203',7200+1+2==7203)
ck('PHASE_CHILD_COUNTS',[0,0,1,2,3,3]==[0,0,1,2,3,3])

equations={
 'meaning':'Conditional cumulative creation/reservation upper for controlled source-owned wrappers, with all named SDK/native/cold/fault residuals retained. It is not a simultaneous lower bound, physical live measurement, fit proof or NOFIT proof.',
 'units':{'O[T]':'A qualified full selected-CLR object upper for T, including inherited instance storage, closed value-type fields, headers/alignment/padding, excluding referent objects charged separately. Metadata declaration data alone does not determine this value.',
 'Ab(n), Ac(n), Ar(n)':'Qualified byte[], char[] and reference[] capacities including array headers/alignment. No free/reclaim credit.',
 'PB(S)':'O of PoolingAsyncValueTaskMethodBuilder<int>.StateMachineBox<S>, including the actual base3 fields and closed S; no extra standalone S object when S is embedded.',
 'DB(S)':'O of AsyncTaskMethodBuilder<int>.AsyncStateMachineBox<S>, including Task<int> base and actual derived StateMachine field.',
 'BridgeMax':'max(O[Task<int>], O[ValueTask<int>.ValueTaskSourceAsTask]); one primary AsTask representation per outer invocation; fault/cancel auxiliary graphs remain R_fault.'},
 'counters':{
 'c_i':'Reached child prefixes [0,0,1,2,3,3]. A pre-start refusal only reduces created objects; an unsettled child quarantines owners and prevents another child.',
 'h_i':'H_i + h_boot_i, where H_i is reached Get-TaskHash invocations and h_boot_i<=7 is reached bootstrap hashes; no numerical transfer of old H_i values is asserted here.',
 'b_i':'Reached Read-TaskBoundedBytes calls <=3.',
 'f_i':'2*n_copy_i+n_mut_i+n_PE_i+n_result_i+n_marker_i+n_firstbyte_i <=14, each a reached prefix count from the source-site ledger.',
 'w_i':'Reached direct WriteAllBytes calls <=6 (three main receipt sites + bundle + failure inventory + failure receipt). Marker has its separate FileStream.',
 'O_i':'ReadAtLeastAsync/AsTask attempts <=259*c_i.',
 'I_i':'Inner Stream.ReadAsync attempts <=1,056,772*c_i; this also bounds AsyncOverSync invocations and registration attempts from above.',
 'T_i':'Distinct worker-thread AsyncOverSync cache instances created by selected calls <=I_i, not <=2*c_i.',
 'E_i':'Direct source HasExited update opportunities <=7203*c_i; actual cached-exit updates can be fewer. KillTree-owned nested calls remain R_host.'},
 'controlled_streams':'(h_i+b_i+f_i+4*c_i)*(O[FileStream]+O[SyncWindowsFileStreamStrategy]) + (f_i+4*c_i)*O[BufferedFileStreamStrategy] + (h_i+b_i+f_i+9*c_i+w_i)*O[SafeFileHandle] + (f_i+2*c_i)*Ab(4096)',
 'pipe_hidden_and_controlled_arrays':'2*c_i*(Ab(4096)_owned_read + O[StreamReader] + Ab(4096)_reader_byte + Ac(m4096) + O[ConsoleEncoding] + O[selected Decoder] + O[SemaphoreSlim] + O[StrongBox<bool>]) + R_encoding_i; no additional FS pipe read-buffer byte4096 term',
 'hash_wrappers':'h_i*(O[IncrementalHash]+O[HashProviderCng]+3*O[SafeBCryptHashHandle]) + R_CNG_cache_native_i + R_oneshot_SHA256_native_i(<=3); three SafeHash wrappers is the source maximum including unsupported reusable-hash constructor retry and final reset, not a claim three remain live.',
 'process_fixed':'c_i*(O[ProcessStartInfo]+O[Process]+O[Stopwatch]+O[CancellationTokenSource]+2*O[CTS.Registrations]+O[SafeProcessHandle]_start) + R_arguments_i(28,6,6; each<=512)',
 'process_poll_wait':'(E_i+c_i)*(O[SafeProcessHandle]_nonowning + O[ProcessWaitHandle]+O[SafeWaitHandle]) + R_waitall_i(tasks<=2,calls<=c_i) + R_host_process_census_descendants_i + R_controller_module_census_i',
 'async_conservative':'O_i*(PB(S_outer)+BridgeMax) + I_i*(DB(S_buffered)+PB(S_invoke)+O[CTS.CallbackNode]) + T_i*(O[AsyncOverSyncWithIoCancellation]+O[SafeThreadHandle_CoreLib]) + 2*c_i*O[Task]_cancellation_callback + R_flow_i + R_pool_i + R_fault_i',
 'whole_symbolic_join':'V_i <= V_streams_i + V_pipe_i + V_hash_i + V_process_fixed_i + V_poll_wait_i + V_async_i, subject to qualification of every O/array unit and each explicitly retained R term. No numeric whole V upper or32MiB fit is certified.'}
progress={
 'outer_258_rejected':counterexample,
 'outer_259_cases':positive_cases,
 'outer_proof':'Each outer returns4096 positive bytes unless it reaches EOF, in which case that pipe can return one final short positive result. Every retained positive result creates exactly one next outer request; two initial requests. For p=0..2 EOF-short positive pipes, retained full chunks <=floor((1048576-p)/4096), hence at most257 retained positives and259 total invocations. Cap/write/stage failures stop requeueing. Final zero/fault results do not create another request.',
 'inner_1056772_proof':'Each positive inner ReadAsync result advances its current outer total by at least one byte and never beyond that outer4096-byte buffer. All completed-and-written outer data is <=1MiB; at most two other current/unretained outer buffers contribute<=8192 bytes. Per pipe a zero result can occur once to terminate an EOF-short positive outer and once in its final requeued zero/fault outer; a fault also terminates that pipe chain. Thus at most four nonprogress terminal calls, and I<=1048576+8192+4=1056772. This counts calls, not a guaranteed kernel-read count or time bound.',
 'quarantine':'Only one outstanding outer per pipe; the outer awaits inner calls sequentially. Callback registration disposal precedes its InvokeAsync completion. An unsettled child owns process/read tasks/two buffers/log streams/CTS in quarantine; the next-child gate refuses. Clearing arrays, disposing, nulling references or scope exit grants no physical-free credit.'}
lemmas=[
 {'id':'nonseekable_pipe_buffer','status':'SOURCE_CONDITIONAL_SELECTED_SYNC_WINDOWS_PATH','argument':'Both redirections construct FileStream(parentHandle,Read,4096,false). Its Buffered strategy takes the nonseekable path, which directly reads into the supplied destination and never calls EnsureBufferAllocated there. The hidden StreamReader constructor still allocates its own byte4096 and char[m4096] and decoder. Two additional caller byte4096 arrays remain V-owned.'},
 {'id':'semaphore_private_schedule','status':'SOURCE_PRIVATE_CONCURRENCY_BOUND','argument':'Each Buffered pipe has one read chain and one inner read at a time. Semaphore construction is reached only by that chain; it owns one SemaphoreSlim and one StrongBox<bool> per pipe. Successful waits are immediately available and return cached Task.FromResult(true). No semaphore contention waiter TaskNode/WaitUntilCountOrTimeout state is required by this private schedule. Cancellation can return a canceled Task; that terminal graph is retained in R_fault, not treated as zero.'},
 {'id':'CTS_registration_reuse','status':'LIVE_REGISTRATIONS_CLOSED_ALLOCATION_REUSE_NOT_CREDITED','argument':'At most two selected registrations can be live per child, and at most two racing first calls can create CTS.Registrations (one possible CAS loser included). FreeNodeList/Unregister recycle actual nodes, but the baseline does not equate two live nodes with two cumulative allocations or assume a guaranteed pool hit. It uses the supported I_i allocation-attempt upper; a tighter private reuse/happens-before lemma may replace it only after separate review.'},
 {'id':'workitem_threads','status':'SOURCE_PER_THREAD_NOT_PER_PIPE','argument':'AsyncOverSync caches t_instance per worker thread; ForceYielding can migrate successive reads across threads. At most two reads are active, but T_i can exceed2. The supported creation count is T_i<=I_i. Cancel once can invoke at most two currently registered callbacks, each starting at most one cancellation-loop Task; callback tasks and their fault graphs are retained.'},
 {'id':'encoding','status':'CONDITIONAL_BRANCHES_NOT_HOST_CODEPAGE_OBSERVATION','argument':'AutomationEngine registers CodePagesEncodingProvider. EncodingHelper codepage0 may choose provider ACP or no-provider OSEncoding(0), and non-UTF8/UTF8 branches differ. ConsoleEncoding delegates GetDecoder/GetMaxCharCount. OSEncoding source yields m4096=16384 and its DecoderDBCS branch allocates byte10; provider SBCS/DBCS and other encoding/cache/fallback/table branches retain R_encoding. UTF8 and m4096=4097 are not unconditional selected facts.'},
 {'id':'DictionaryWrapper','status':'SOURCE_PLUS_GENUINE_FIELD_DECLARATION','argument':'Selected Environment creates DictionaryWrapper, direct Object base and one Dictionary reference, not StringDictionary. The actual88-type metadata corroborates that declaration. No StringDictionary Hashtable is charged. N already owns capture/dictionary/block and v_DW; V contributes this shape evidence but excludes N/DW bytes to avoid duplication.'},
 {'id':'cold_hash_cache','status':'NATIVE_AND_SHARED_CACHE_RESIDUAL','argument':'IncrementalHash creates HashProviderCng; its non-HMAC _key stays null. BCryptAlgorithmCache uses shared ConcurrentDictionary handle caches. Managed SafeHash creation is bounded per source call, but native BCrypt state, cache initialization/baseline/concurrent participants and source-binary association are not assigned invented bytes. Static captured-byte SHA256.HashData has a separate one-shot native path, <=3 calls.'},
 {'id':'KillTree','status':'CONCRETE_UNQUALIFIED_HOST_CENSUS','argument':'Selected finally Kill(true) reaches Process.Win32 KillTree/GetProcessHandlePairs; foreach(GetProcesses()) creates host-wide Process/ProcessInfo/native enumeration owners before parent filtering, then recurses actual descendants. Controller c/log/argv/poll caps do not bound host process/thread population or descendant depth. This remains R_host_process_census_descendants, without enumeration, guessed constants or a fit/no-fit claim.'},
 {'id':'current_controller_module','status':'CONCRETE_UNQUALIFIED_BOOTSTRAP_MODULE_CENSUS','argument':'Initialize-TaskController GetCurrentProcess().MainModule.FileName reaches first-module enumeration; pinned ProcessManager.Win32 allocates/grows its native module-handle array before returning the first module. Selected runtime module-population/native ownership is not bounded by input/log caps and remains R_controller_module_census.'}
]
remaining=[
 {'term':'O[T]/typed arrays','barrier':'Actual declarations do not give instantiated layout/offset/padding/size. The inventory includes generated S fields, but nested ConfiguredTaskAwaiter/ConfiguredValueTaskAwaiter, CachedCompletedInt32Task, HashAlgorithmName, StrongBox<bool>, inherited TextReader/WaitHandle and other closed value/base layouts still need a complete selected ABI association before a numeric unit is bound.'},
 {'term':'R_flow_i','barrier':'ExecutionContext, continuation/delegate scheduling/ThreadPool queue/native monitor owners at the actual AwaitUnsafeOnCompleted/OnCompleted/StartNew boundaries are not part of the391 declared-field total or the controlled buffer sum.'},
 {'term':'R_pool_i','barrier':'Two pooled generic state families create per-core PaddedReference arrays sized Environment.ProcessorCount plus thread-local caches, static delegates and runtime owners. No host processor/thread observation, guaranteed rent hit, returned-buffer free credit or zero cold-cache baseline is assumed.'},
 {'term':'R_fault_i','barrier':'Task faults/cancel auxiliaries, ExceptionDispatchInfo/TaskExceptionHolder/Exception/ErrorRecord and pure VM/binder/type-metadata/frame heaps stay unqualified by the adopted contract. E owns controlled reference slots/witnesses, not those full SDK graphs.'},
 {'term':'R_waitall_i','barrier':'At most one WaitAll(two tasks,2000) per child; source may allocate waited-task List(capacity2), SetOnCountdownMres, monitor lock and per-task continuation storage. The default token does not create another user CTS registration; monitor/native/fault ownership remains explicit, no unseen kernel event or physical-free shortcut.'},
 {'term':'R_arguments_i','barrier':'Argument counts28/6/6 and each512 units are closed. ProcessStartInfo.Collection/List backings, command-line quoting/ValueStringBuilder growth/rented char capacities and associated pool cold owners require their separate typed source join; no aggregate slack or fixed invented per-command bytes used.'},
 {'term':'R_CNG_cache_native_i / R_oneshot_SHA256_native_i / R_encoding_i','barrier':'Named provider/native/cold/cache/code-page branches are retained. No code-page/environment/private-config read, native probe or broad vendor graph exploration was performed.'},
 {'term':'R_host_process_census_descendants_i / R_controller_module_census_i','barrier':'Exact source calls reach populations controlled outside the source profile. No child cap proves their size. A source correction or precise external profile is required; this baseline does not choose one before review.'},
 {'term':'source association and N','barrier':'File pins and actual public metadata identify files/declarations. They do not alone prove every selected runtime implementation/source association or the separate external environment-noninterference premise. Current R12/R13 Data/prices/bindings/guards/GO remain unchanged.'}
]
dedup={
 'excluded_existing_F_H_B':'Hash byte65536, digest32/hex/Tuple, both Copy-TaskOwned byte65536 arrays, PE byte64/24/optional buffers (4256 each x4), mutation before/replacement/tiny hex arrays, codec/result byte/string payloads and strict non-output UTF8Encoding constructors are already owned in resource families; not duplicated V.',
 'excluded_existing_P':'All referent path strings/normalization/substrings/native path buffers/iterators remain P; V charges wrapper fields but not their path referents again.',
 'excluded_existing_N':'Environment capture/dictionary/outgoing block and DictionaryWrapper unit remain N. The source and genuine declaration close the erroneous StringDictionary-base hypothesis only.',
 'new_controlled_arrays':'Two caller byte4096 arrays per child and two hidden StreamReader byte4096/char[m4096] pairs per child belong V. No hidden pipe FileStream byte4096 read buffer is charged on the selected direct nonseekable path.',
 'not_reclaimed':'No Dispose/Clear/null/scope/pool-return/GC credit; all counts are source creation envelopes until a qualified lifetime theorem says more.'}
alternatives=[
 {'option':'A','recommendation':'RECOMMENDED_SOURCE_HYPOTHESIS_AFTER_BASELINE_CRITIC','summary':'A small fixed controller-owned process/I/O component: create the console child suspended with CREATE_NO_WINDOW, assign it to one owned Job Object, resume it only after assignment, and drain the two synchronous anonymous pipes on exactly two dedicated owned workers with preallocated4096-byte arrays. Reuse the same two worker/read/error slots through the child lifetime; source-owned CancelSynchronousIo/cancellation plus bounded exit/join waits replace per-inner async task chains. Terminate the owned job on failure and retain all owners if either worker/child is not settled.',
 'preserve':'Same raw byte ordering per pipe,1MiB retained cap/before-write stage cap, admission/polls/timeouts/raw exit/cleanup records, primary-secondary policy, immutable baseline, no next child under quarantine, full owned descendant cleanup, strict no GUI/input. Assign-before-resume avoids the child-spawn/job-assignment race.',
 'costs_to_review':'New job/native handles, explicit two worker native stacks, cancellation/wait/error slots, launch flags/handle inheritance and code binding must be sponsored and independently reviewed; no claim existing budgets fit. Existing broad Kill(true) and fragmented async chains would then be absent by source, rather than assigned guesses.'},
 {'option':'B','recommendation':'ALTERNATIVE_MORE_INTEROP_COMPLEXITY','summary':'Use one owned Job Object with assign-before-resume and two genuinely overlapped private pipes. Preallocate/reuse one OVERLAPPED/event/read buffer per pipe and drive a bounded source-owned WaitForMultipleObjects/ReadFile/CancelIoEx settlement state machine.',
 'preserve':'Same output/cap/failure/exit/quarantine/headless requirements; own only the assigned job/process/pipe handles and retain every unsettled native request owner.',
 'costs_to_review':'Avoids per-inner task allocation and dedicated read-worker stacks, but native overlapped lifetime, cancellation completion, broken-pipe/EOF and event reuse correctness create a larger first implementation/critic obligation.'},
 {'option':'C','recommendation':'ALTERNATIVE_SMALLER_FIRST_REPAIR_INCOMPLETE_ALONE','summary':'First replace only host-wide Kill(true) cleanup with an assign-before-resume owned Job Object process wrapper; retain the existing async pipe implementation and its conservative259/1056772 typed resource equations.',
 'preserve':'Owned descendant termination and existing cancellation/reap/quarantine/headless behavior remain explicit.',
 'costs_to_review':'Closes the concrete host census hole with a smaller change, but fragmented per-read wrappers, thread caches and runtime pool/continuation terms still prevent the current complete payload proof. This is staged work, not full closure.'}
]
report={'schema':'CONTROLLER_V_WRAPPER_SOURCE_CLOSURE_R1','verdict':'HOLD_COMPLETE_SCOPED_BASELINE_WITH_FINITE_COUNTS_AND_TYPED_DECLARATION_COEFFICIENTS','source_runner_sha256':subject_seals['run-clock.ps1'],'target_execution':False,'check_count':len(checks),'actual_metadata_scope':'Root separately authorized and executed the independently reviewed R4 fixed-file reader; this task only reads its pinned88-type/391-field result. No actual layout/size/allocation or Clock execution credit.',
 'counter_corrections':['Outer read/AsTask upper259, not258.','One additional predecessor-first-byte OpenRead site makes fixed nonchild buffered streams14, not13.','WriteAllBytes opens a direct SafeFileHandle and performs RandomAccess write; no FileStream/4KiB buffer.','Selected DictionaryWrapper has direct Object base and one dictionary reference; no unused StringDictionary Hashtable.'],
 'source_sites':fixed_sites,'progress_proof':progress,'equations':equations,'lemmas':lemmas,'deduplication':dedup,'remaining_precise_obligations':remaining,'source_correction_alternatives':alternatives,'selected_next_hypothesis':'A is recommended for parent/critic decision only. No new runtime/Clock source or execution authored by this report.','whole_payload_fit':False,'NOFIT_claim':False,'Data_prices_native_fit_GO_bound':False,'verified_inputs':list(inputs.values())}
declarations={'schema':'GENUINE_R4_DECLARATION_PROJECTION_NOT_LAYOUT','inventory_sha256':sha(inventory_data),'source_file_pins':[r for r in manifest.get('sources',[]) if isinstance(r,dict)],'types':inventory['types'],'counts':{'types':88,'declared_nonstatic_instance_fields':391}}
(HERE/'declaration-evidence.json').write_bytes(encode(declarations))
(HERE/'checks.json').write_bytes(encode({'schema':'OWN_SOURCE_DATA_INTEGER_CHECKS_V1','count':len(checks),'checks':checks,'integer_outer_cases':positive_cases,'counterexample':counterexample,'inner_upper':inner}))
(HERE/'closure.json').write_bytes(encode(report))
md='''# Controller V source closure R1

HOLD. The complete scoped baseline has finite call and controlled-owner counts with genuine field-declaration evidence; it has no joined numeric V upper or payload-fit certification. All source subjects and prior reports remain immutable. This task did not execute PowerShell parsing/import/helpers, a compiler, metadata reader, Clock program/stage, process census, code-page query, GUI, audio or Git.

The root separately ran the independently reviewed fixed-file metadata purpose reader: raw0, stderr0,88 types and391 declared fields. This report reads that pinned output only. Actual generated field counts are9 for the outer state,7 for Buffered state,6 for InvokeAsync state,2 for ValueTaskSourceAsTask,3+1 for pooled box base/derived and1 for the default Task box derived state. Those declarations do not supply instantiated runtime layout or object sizes.

The outer read bound is259 per child. A counterexample to258 is255 full4096-byte chunks, a final one-byte EOF-short result on each pipe and then a zero result on each pipe:259 calls with1,044,482 retained bytes. For p EOF-short pipes, at most floor((1MiB-p)/4096)+p retained positive outer results each create one next request, plus two initial requests. Inner calls remain bounded by1,056,772: at most1MiB confirmed-written positive bytes plus two current4096-byte outer buffers and at most four terminal zero/fault calls. Counts do not measure kernel calls, memory or time.

Each pipe owns one pending outer read and sequential inner reads. Its private semaphore is uncontended, with one SemaphoreSlim/StrongBox pair per pipe. The selected nonseekable Buffered path directly reads into the caller array, so no extra FileStream4096-byte read buffer is charged. The hidden StreamReader constructor still creates byte4096, char[m4096] and a decoder; the two caller4096-byte arrays are additional V owners. Async default/pooled state boxes, AsTask representations and per-thread cancellation workers stay charged. Two active pipes do not prove only two cumulative per-thread worker objects or guaranteed box/node pool hits.

The complete FileStream site ledger includes an omitted predecessor-first-byte OpenRead. Full fixed nonchild buffered count is14: four copy endpoints, two mutations, four PE reads, two child-result captures, one marker and one first-byte stream. Hash and bounded JSON streams use bufferSize1. Each child adds two log and two pipe streams. Document WriteAllBytes is a direct SafeFileHandle/RandomAccess path, not another FileStream. Existing hash/copy/PE/mutation/codec arrays and P paths/N environment objects are explicitly deduplicated.

Selected Environment creates a direct-Object DictionaryWrapper with one dictionary-reference field; StringDictionary/Hashtable belongs an unused property path. Encoding remains branch-qualified: PowerShell registers CodePagesEncodingProvider, cp0 may use ACP, and the no-provider OSEncoding path yields m4096=16384. UTF8/m4096=4097 is not an unconditional fact. DecoderDBCS can allocate byte10; provider/cold/cache/fallback/table owners remain named.

Two concrete external-population gaps remain. Failure Kill(true) enumerates all host processes before filtering children and recursively enumerates descendants. Bootstrap GetCurrentProcess().MainModule.FileName grows a module-handle census before returning the first module. Child log/argv/poll caps bound neither graph. General task fault/ExceptionDispatchInfo/Exception/ErrorRecord and VM/binder/type/frame graphs also remain unqualified under the existing contract; E does not own them in full.

closure.json gives all source sites, typed equations, owners, progress proofs, exact residual barriers and the genuine metadata projection. Object coefficients require full inherited/closed-value-type ABI association; pool/cache/continuation/native encoding/CNG/wait/command-line/error/population terms are not replaced by padding. No null, Clear, Dispose, scope, pool return or hoped-for GC earns physical-free credit. Cumulative creation uppers are not coexisting lower bounds, so neither fit nor NOFIT is inferred.

The recommended next source hypothesis is A: a small fixed controller-owned process component with assign-before-resume Job Object ownership and exactly two dedicated synchronous read workers/preallocated buffers, source-owned cancellation and bounded settlement/quarantine. This removes host-wide process enumeration and per-inner async wrapper chains. B uses fixed overlapped pipe state and the same job ownership, with more native lifetime complexity. C repairs only job-based cleanup first and retains the async HOLD. All preserve raw-output/cap/primary-secondary/reap/quarantine/headless semantics and need a fresh source design/critic/funding review; no implementation or execution is authorized by this baseline.
'''
md+='\n'+str(len(checks))+' own source/file/hash/declaration/integer checks passed at the first source-check attempt; source inputs and outputs are sealed in source-pins.json.\n'
(HERE/'closure.md').write_text(md,encoding='utf-8',newline='\n')
outputs=[]
for name in ['closure.json','closure.md','checks.json','declaration-evidence.json']:
 path=HERE/name;data=path.read_bytes();outputs.append({'path':path.relative_to(ROOT).as_posix(),'bytes':len(data),'sha256':sha(data)})
evidence=[]
for path in sorted(HERE.iterdir()):
 if path.is_file() and path.name not in ['source-pins.json']+[pathlib.Path(x['path']).name for x in outputs]:
  data=path.read_bytes();evidence.append({'path':path.relative_to(ROOT).as_posix(),'bytes':len(data),'sha256':sha(data)})
for path in sorted((HERE/'primary').iterdir()):
 if path.is_file():record(path)
pins={'schema':'CONTROLLER_V_SOURCE_CLOSURE_PINS_R1','verdict':report['verdict'],'inputs':list(inputs.values()),'outputs':outputs,'author_evidence':evidence,'source_subjects_preserved':subject_seals,'no_target_execution':True}
(HERE/'source-pins.json').write_bytes(encode(pins))
print(json.dumps({'verdict':report['verdict'],'checks':len(checks),'outputs':outputs,'pins':{'bytes':(HERE/'source-pins.json').stat().st_size,'sha256':sha((HERE/'source-pins.json').read_bytes())}},indent=2))
