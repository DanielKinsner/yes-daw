"""Source/document/data critique only. Produces no executable target source or execution gate."""
import hashlib,html,json,pathlib,re
ROOT=pathlib.Path(__file__).resolve().parents[2]
HERE=pathlib.Path(__file__).parent
BASE=ROOT/'build-ci/20261106-controller-v-wrapper-source-closure-r1'
for name in ['hypothesis.json','README.md','checks.json','source-pins.json']:
 if (HERE/name).exists():raise RuntimeError('PRESERVE_PRIOR_OUTPUT_'+name)
checks=[];inputs={}
def sha(data):return hashlib.sha256(data).hexdigest().upper()
def rec(path):
 path=path.resolve();data=path.read_bytes()
 try:name=path.relative_to(ROOT).as_posix()
 except ValueError:name=path.as_posix()
 row={'path':name,'bytes':len(data),'sha256':sha(data)};inputs[name]=row
 return row
def read(path):rec(path);return path.read_text(encoding='utf-8')
def ck(name,condition):
 checks.append({'id':name,'pass':bool(condition)})
 if not condition:
  (HERE/'failed-checks.json').write_text(json.dumps(checks,indent=2)+'\n',encoding='utf-8',newline='\n')
  raise RuntimeError(name)
def enc(x):return (json.dumps(x,indent=2,ensure_ascii=True)+'\n').encode()
base_json=read(BASE/'closure.json');base_pins=read(BASE/'source-pins.json')
ck('FROZEN_V_JSON_SEAL',sha(base_json.encode())=='01B4289C2F8E521093987C259D15C5DD4BBF2388EFD9E594A90319C9B0BB5EE6')
ck('FROZEN_V_PINS_SEAL',sha(base_pins.encode())=='7A9C4E971FE3340ED28342CE60836DBC46A3B8FE0AE9E9E1388B011113F11C45')
ck('V_STAYS_HOLD',json.loads(base_json)['verdict']=='HOLD_COMPLETE_SCOPED_BASELINE_WITH_FINITE_COUNTS_AND_TYPED_DECLARATION_COEFFICIENTS')
environment=read(HERE/'primary/Environment.cs');windows=read(HERE/'primary/Environment.Windows.cs')
interop=read(HERE/'primary/Interop.GetModuleFileName.cs')
process=read(BASE/'primary/ProcessManager.Win32.cs')
for name,literal,text in [
 ('ENV_PROCESS_PATH_CACHE','Interlocked.CompareExchange(ref s_processPath, GetProcessPath() ?? "", null);',environment),
 ('ENV_PROCESS_PATH_NULL','return (processPath.Length != 0) ? processPath : null;',environment),
 ('CURRENT_PROCESS_MODULE_ZERO','GetModuleFileName(IntPtr.Zero, ref builder.GetPinnableReference(), (uint)builder.Capacity)',windows),
 ('CURRENT_PATH_START_MAX_PATH','new ValueStringBuilder(stackalloc char[Interop.Kernel32.MAX_PATH])',windows),
 ('PATH_GROWS_BEFORE_RETURN','builder.EnsureCapacity(builder.Capacity * 2);',windows),
 ('DIRECT_KERNEL32_GETMODULEFILENAME','EntryPoint = "GetModuleFileNameW"',interop),
 ('OLD_STRIPS_EXTENDED_PREFIX','charsSpan = charsSpan.Slice(NtPathPrefix.Length);',process),
 ('OLD_MODULE_POPULATION_ARRAY','IntPtr[] moduleHandles = new IntPtr[modulesCount];',process)]:ck(name,literal in text)
env_get=windows[windows.index('private static string? GetProcessPath()'):windows.index('private static unsafe OperatingSystem GetOSVersion()')]
ck('DIRECT_QUERY_NO_MODULE_ENUMERATION','EnumProcessModules' not in env_get and 'GetModuleInformation' not in env_get)
ck('DIRECT_QUERY_NO_HARD_BUFFER_CAP','builder.EnsureCapacity(builder.Capacity * 2)' in env_get and '32767' not in env_get)
origins=json.loads(read(HERE/'origins.json'))
for row in origins:
 ck('OFFICIAL_PINNED_DOTNET_ORIGIN_'+row['file'],'/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/' in row['url'])
 if 'sha256' in row:ck('SOURCE_HASH_'+row['file'],rec(HERE/'primary'/row['file'])['sha256']==row['sha256'])
docs={}
for row in json.loads(read(HERE/'document-origins.json')):
 data=read(HERE/'primary-documents'/row['file'])
 ck('PRIMARY_DOCUMENT_DOMAIN_'+row['file'],row['url'].startswith('https://learn.microsoft.com/en-us/windows/win32/'))
 ck('PRIMARY_DOCUMENT_HASH_'+row['file'],rec(HERE/'primary-documents'/row['file'])['sha256']==row['sha256'])
 docs[row['file']]={'url':row['url'],'plain':' '.join(html.unescape(re.sub('<[^>]*>',' ',data)).split())}
def doccheck(file,name,words):ck(name,all(word in docs[file]['plain'] for word in words))
doccheck('GetModuleFileNameW.html','DOC_CURRENT_EXECUTABLE',['If this parameter is NULL','executable file of the current process'])
doccheck('GetModuleFileNameW.html','DOC_LOAD_PATH_FORMAT',['same format','long or short file name'])
doccheck('GetModuleFileNameExW.html','DOC_CURRENT_PROCESS_PREFERRED_DIRECT_API',['current process','more efficient and more reliable'])
doccheck('UpdateProcThreadAttribute.html','DOC_CREATION_TIME_JOB_ATTRIBUTE',['PROC_THREAD_ATTRIBUTE_JOB_LIST','job handles to be assigned to the child process','Windows 10'])
doccheck('UpdateProcThreadAttribute.html','DOC_HANDLE_INHERITANCE_LIST',['PROC_THREAD_ATTRIBUTE_HANDLE_LIST','inheritable handles'])
doccheck('JobObjects.html','DOC_LAST_HANDLE_JOB_KILL',['JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE','last job object handle'])
doccheck('JobObjects.html','DOC_BREAKAWAY_IS_SEPARATE',['JOB_OBJECT_LIMIT_BREAKAWAY_OK','JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK'])
doccheck('JobObjects.html','DOC_NOTIFICATIONS_NOT_ALL_GUARANTEED',['delivery of messages','not guaranteed'])
doccheck('JobObjects.html','DOC_JOB_SIGNAL_IS_LIMIT_CASE',['set to signaled','end-of-job time limit'])
doccheck('JobBasicAccounting.html','DOC_ACTIVE_PROCESS_COUNT',['ActiveProcesses','currently associated with the job'])
doccheck('CancelPendingIO.html','DOC_CANCELLATION_NOT_GUARANTEED',['does not guarantee','will be canceled'])
doccheck('AnonymousPipes.html','DOC_ANON_NO_OVERLAPPED',['overlapped','not supported by anonymous pipes'])
runtime='C:\\Users\\SM - Dan\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\native\\powershell'
expected=runtime+'\\pwsh.exe'
ck('SELECTED_FIXED_HOST_PATH_UNDER260',len(expected)+1<260)

options=[
 {'id':'S1','label':'Pinned native supervisor plus bounded spool transport','recommendation':'RECOMMENDED_CONDITIONAL_SOURCE_HYPOTHESIS',
 'structure':'One fixed native console supervisor per reached child, owning one unnamed Job Object and two private synchronous pipe workers with preallocated4096-byte read arrays. No managed per-inner async task/pool/registration graph. Parent has no redirected supervisor stdout/stderr and reads only admitted bounded spool/terminal files after the supervisor reports terminal state or exits.',
 'containment':'Prefer creation-time PROC_THREAD_ATTRIBUTE_JOB_LIST with the job already configured KILL_ON_JOB_CLOSE and no breakaway flags, combined with an exact handle inheritance list. The selected platform must qualify Windows10+/supported attribute. CREATE_SUSPENDED may still be used for setup ordering, but CreateSuspended->Assign alone is not atomic containment against supervisor death.',
 'benefit':'Eliminates both parent fragmented async-wrapper creation and host-wide KillTree enumeration by source. Process/cancellation/file/error state is a finite purpose-owned graph whose explicit native/CRT/stack costs can be counted.',
 'new_obligations':'New binary/input/source/ABI/native association, fixed request/result protocol and spool artifacts, whole source-operation funding and complete32MiB joined accounting. Supervisor cost is not exempt because it is another process. No size/fit/GO result is supplied here.'},
 {'id':'S2','label':'Controller-owned in-process job and two dedicated synchronous workers','recommendation':'SUPPORTED_ALTERNATIVE_LARGER_MANAGED_FAILURE_JOIN',
 'structure':'A small pinned controller component owns the same creation-time job containment and two dedicated synchronous read workers/preallocated read buffers, with explicit cancellation and bounded joins, preserving rooted quarantine in the controller.',
 'benefit':'Avoids a new supervisor/spool protocol and keeps actual target raw-exit observation in the controller.',
 'tradeoff':'Managed thread/worker/exception/native-stack joins remain in the same controller. An unsettled synchronous I/O worker must retain its buffer/component and can outlive the child; cancellation is not guaranteed. It also needs whole resource/source/ABI funding and cannot claim physical-free credit.'},
 {'id':'S3','label':'Owned job and fixed native overlapped named-pipe state','recommendation':'SUPPORTED_ALTERNATIVE_MORE_COMPLEX_FIRST_IMPLEMENTATION',
 'structure':'The same native supervisor or in-process component uses genuinely overlapped named pipes with one preallocated OVERLAPPED/event/read buffer per pipe and a source-owned finite state machine.',
 'benefit':'Can avoid two dedicated read-worker stacks and per-inner tasks.',
 'tradeoff':'Anonymous CreatePipe pipes do not support overlapped operations; a named-pipe design requires its own bounded unique-name/security/connection protocol and cancellation-completion/lifetime proof. Cancellation return is not completion. This is a larger first critic obligation than S1.'}
]
requirements=[
 {'id':'C1','topic':'Creation and ownership atomicity','required':'Configure the unnamed private job before CreateProcess; specify job membership during process creation with the admitted attribute, or supply another reviewed creator-death-safe containment proof. Do not claim a suspended but unassigned process is contained. Every create/attribute/assign/resume failure must preserve explicit observed/unknown cleanup state and refuse certification.'},
 {'id':'C2','topic':'Last-handle proof','required':'The supervisor is the sole intended job-handle owner. The job handle is noninheritable and absent from the child handle list; it is not copied to the parent, target, descendants or a named reopenable channel. Pass only exact owned stdin/null and stdout/stderr child pipe handles. No breakaway flags or silent fallback. Last-handle closure initiates termination; it is not an observed-settlement receipt.'},
 {'id':'C3','topic':'Concurrent raw output','required':'Drain both pipes concurrently; neither worker may wait for child exit before reading. Reuse fixed buffers regardless of one-byte fragmentation, arbitrary output timing or cap overflow. Preserve per-pipe raw prefixes, shared per-child1MiB retained-log cap, observed/retained/omitted/unknown accounting, current before-write complete-stage-pool admission and raw-output failure handling. No quiet-output premise, cap increase or new legitimate-input refusal is allowed.'},
 {'id':'C4','topic':'Native settlement','required':'Observe actual target process exit/bit-exact32-bit exit code separately from supervisor exit. Normal completion requires both read workers/requests settled, spool writes flushed/admitted and the owned job population settled under a valid current accounting query/other reviewed proof. Do not treat a generic job wait or a missing completion-port notification as all processes exited; both APIs have narrower documented semantics.'},
 {'id':'C5','topic':'Abnormal supervisor exit','required':'Parent may kill only the known supervisor process handle. Job close is a cleanup action, not proof of descendants reaped, buffers freed, raw output complete or target exit observed. Missing/malformed/partial terminal data, supervisor crash, timeout, unresolved native I/O or absent settlement proof sets unknown/quarantine and prevents another child; preserve all spool files and diagnostic owners. No next stage or success is synthesized from supervisor exit0 alone.'},
 {'id':'C6','topic':'Lifetime and cancellation','required':'Cancellation and job termination may fail to settle I/O before the bounded wait. Never recycle or free a buffer/OVERLAPPED/worker state while native I/O can still reference it. Retain rooted owners or conservatively quarantine the abnormal supervisor branch. A source shutdown path may not use unbounded retries while claiming the original1800s body plus bounded cleanup schedule.'},
 {'id':'C7','topic':'Protocol and immutable identity','required':'Define one small versioned exact request and terminal record with bounded path/argv/file extents, child name/index, manifest/run identity, target binary identity, explicit observed flags and numeric raw codes. Avoid arbitrary native error text/JSON growth. Verify exact length/schema/kind/EOF/current identity. Add every new input and spool/temporary artifact to a new source manifest and complete filesystem/fence/reparse/hash/primary-secondary ownership correspondence. Preserve the original target PE checks and binary before/after fences; checking only the supervisor is insufficient.'},
 {'id':'C8','topic':'Complete accounting','required':'Count supervisor source work, all native/static/heap buffers, two worker stacks and handles, job/attribute/kernel/CRT/TLS/cold owners under the adopted whole accounting convention, parent polling/wrappers, file transport reads/writes and all retained failed-attempt branches. Derive a joined phase lifetime upper; no per-process exemption, compiler-opacity extension, GC/Dispose/close free credit or fit claim from cumulative slack.'},
 {'id':'C9','topic':'No desktop interruption','required':'Fixed console supervisor and already gated console target use CREATE_NO_WINDOW, no shell, no interactive stdin and no GUI/input/foreground/device APIs, with reviewed inherited OS error-dialog policy. Do not claim these flags contain arbitrary GUI programs; only the admitted fixed source/binary profile is in scope. Any future app drive still needs the repository separate-session policy.'},
 {'id':'C10','topic':'Qualification order','required':'Write the in-scope architecture/protocol/resource proposal and obtain independent critic review first. A separately reviewed compile-only purpose lane may qualify the supervisor source/object/binary and ABI. Only a fresh complete source/native/resource/binding review plus root gate can authorize any later program/Clock attempt; no implementation or execution is authorized by this options packet.'}
]
bootstrap={
 'verdict':'LITERAL_REPLACEMENT_REQUIRES_PREFIX_CORRESPONDENCE_AND_PROFILE_SCOPE',
 'old_source':'Process.GetCurrentProcess().MainModule reaches module enumeration, native module metadata and growing pointer arrays. Its selected implementation removes a leading extended-length prefix before exposing FileName.',
 'new_source':'Environment.ProcessPath caches a Windows GetModuleFileNameW(NULL) result from a260-character initial stack buffer, growing a ValueStringBuilder only if truncated. The Windows API identifies the executable of the current process directly; no process/module census is involved.',
 'supported_narrow_hypothesis':'Under the same exact trusted fresh-launcher/immutable runtime path profile, reject null/query failure, preserve the old leading extended-prefix treatment and the same OrdinalIgnoreCase expected executable comparison. The admitted fixed executable path is '+str(len(expected))+' UTF16 units, so that selected successful direct query fits its initial260-character buffer. This is a source-profile implication, not a live ProcessPath observation or a global resource bound for arbitrary executable paths.',
 'not_claimed':'The property source has no hard growth cap, can be initialized concurrently and is cached; renamed/deleted-executable behavior is expressly undefined by its own remarks. It does not provide operationally identical API failure behavior or unconditional equivalence for arbitrary path aliases/prefixes/mutations. No silent path canonicalization, relaxed identity or host/runtime version inference is authorized.',
 'strict_bounded_alternative':'If the profile is insufficient for the chosen whole proof, a separately reviewed tiny direct GetModuleFileNameW(NULL) binding with a fixed expected-length-plus-prefix/NUL buffer may reject truncation before any growth; its new binding/storage/error source must be counted. That is a further source hypothesis, not authored here.',
 'baseline_unchanged':'R_controller_module_census remains HOLD in the frozen V baseline until a concrete successor source and whole functional/resource review accept its actual replacement.'}
failure_cases=[
 {'case':'supervisor dies after suspended Create before manual Assign','manual_sequence':'Unassigned suspended child can survive; unsupported atomic-containment claim.','S1_requirement':'Creation-time job assignment or a separately proved creator-death-safe protocol.'},
 {'case':'job handle inherited by target','manual_sequence':'Supervisor death does not close the last handle; KILL_ON_JOB_CLOSE inference fails.','S1_requirement':'Noninheritable private handle plus exact inherited stdio list.'},
 {'case':'supervisor killed or terminal record partly written','manual_sequence':'Target raw exit/descendant settlement is not inferred.','S1_requirement':'Quarantine/unknown; preserve files; forbid next child.'},
 {'case':'target or descendant keeps stderr write handle open after target exit','manual_sequence':'Pipe read may remain pending despite target exit.','S1_requirement':'Concurrent drain, bounded cancellation/settlement and retained/quarantined owners.'},
 {'case':'one-byte output fragments until cap','manual_sequence':'Same two fixed native read arrays are reused; iteration count still requires source work sponsorship.','S1_requirement':'No per-byte task/node allocation and no output-silence assumption.'},
 {'case':'job assignment feature unavailable or incompatible nesting','manual_sequence':'No silent breakaway or resume-before-containment fallback.','S1_requirement':'Fail before running the target and preserve raw refusal; platform feature remains an explicit qualification.'},
 {'case':'both pipes race the shared cap or a spool write fails','manual_sequence':'A successful reservation does not prove bytes were written; a failure must not disappear.','S1_requirement':'Single source-owned cap/accounting protocol distinguishes observed, reserved, written, omitted and unknown bytes; identical per-pipe prefix semantics.'}
]
ck('THREE_PARALLEL_DESIGN_OPTIONS',len(options)==3 and len({x['id'] for x in options})==3)
ck('TEN_REQUIRED_DESIGN_CLOSURES',len(requirements)==10)
ck('SEVEN_SOURCE_FAILURE_SCENARIOS',len(failure_cases)==7)
ck('BASELINE_JSON_PRESERVED_AFTER_READ',rec(BASE/'closure.json')['sha256']=='01B4289C2F8E521093987C259D15C5DD4BBF2388EFD9E594A90319C9B0BB5EE6')
ck('BASELINE_PINS_PRESERVED_AFTER_READ',rec(BASE/'source-pins.json')['sha256']=='7A9C4E971FE3340ED28342CE60836DBC46A3B8FE0AE9E9E1388B011113F11C45')
report={'schema':'CONTROLLER_OWNED_PROCESS_SOURCE_HYPOTHESIS_R1','verdict':'RECOMMEND_S1_NATIVE_SUPERVISOR_AS_CONDITIONAL_NEXT_SOURCE_DESIGN_NOT_IMPLEMENTATION_ACCEPTANCE','supersedes_recommendation_only':'The concrete root native-supervisor/spool candidate is now judged preferable to the frozen V baseline generic in-process A. V facts/equations/HOLD and all source bytes remain unchanged.',
 'source_execution':False,'options':options,'requirements':requirements,'failure_cases':failure_cases,'bootstrap_fix':bootstrap,'bounded_next_step':'Root chooses the source design, then author a concrete ADR/protocol/state/owner/cost proposal for independent critic review; no target/helper source or compile/run is authorized by this packet.','whole32MiB_scope_unchanged':True,'cap_raise':False,'quiet_output_law':False,'domain_narrowing_or_hidden_false_guards':False,'Data_prices_native_binding_GO':False,'check_count':len(checks),'verified_inputs':list(inputs.values()),'primary_document_urls':[x['url'] for x in docs.values()]}
md='''# Owned-process source hypothesis R1

Recommend S1: the root's pinned native console supervisor with fixed read workers and bounded spool/terminal transport. It removes the parent per-inner managed async wrapper graph and host-wide KillTree census in one concrete design. S2 keeps those workers and job ownership inside the controller; S3 uses a more complex fixed overlapped named-pipe state machine. Every supervisor byte, stack, handle, native/CRT owner and transport object remains in the whole32MiB accounting. No implementation, compilation, execution, fit or GO is accepted here.

The creation sequence needs one correction before authoring: CreateSuspended followed by Assign closes child execution before assignment, but supervisor death in that gap can leave an unassigned suspended child. Prefer creation-time PROC_THREAD_ATTRIBUTE_JOB_LIST under an explicitly qualified supported profile; Microsoft documents Windows10+ support and the list's assignment during process creation. Exact inherited-handle lists must exclude the private job handle. [UpdateProcThreadAttribute](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute).

The supervisor owns one unnamed noninheritable job handle, no breakaway settings, two private read workers and their fixed4096-byte arrays. Parent uses no redirected supervisor stdout/stderr, so it does not recreate hidden StreamReaders and async pipe wrappers. Child raw stdout/stderr go to bounded admitted spool files with the original shared1MiB cap and stage-pool rules. Both pipes must be drained concurrently, including arbitrarily fragmented or abnormal output. Anonymous pipes support synchronous operations, so the overlapped alternative requires genuinely named pipes. [Anonymous Pipe Operations](https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations).

Job last-handle close is a termination action. It does not establish observed target exit, complete output or descendant settlement. A job wait is not a generic all-exited event, and ordinary job completion-port messages are not all guaranteed. Normal terminal admission needs actual target raw-exit observation, both drainers settled, admitted spool completion and a valid current empty-job/settlement proof. Parent killing only the known supervisor is permitted cleanup, but missing/partial terminal evidence must become unknown/quarantine and forbid another child. [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects), [Job basic accounting](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information).

Cancellation is an attempt, not completion. Keep every buffer, native request and worker owner until completion is observed; bounded waits that fail to settle must retain/quarantine the branch. The protocol must preserve observed versus unobserved target exit and written versus merely observed/omitted bytes. No quiet-output premise, cap raise, same-user host process scan or arbitrary extra input rejection may replace these obligations. [Canceling pending I/O](https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations).

The narrow bootstrap repair is supported conditionally: Environment.ProcessPath uses GetModuleFileNameW(NULL) directly, so it avoids the module census. Preserve null/query refusal, the old MainModule leading-extended-prefix treatment and the exact OrdinalIgnoreCase expected-host comparison under the trusted immutable fresh-launch path profile. That fixed successful path fits the initial260-character buffer. A literal replacement is not a universal source-equivalence proof: the old source strips the extended prefix, the new source does not; the new property caches and has no global growth cap. Microsoft documents the current-executable query and possible load-path formats, and recommends the direct API for the current process. [GetModuleFileNameW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew), [GetModuleFileNameExW](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmodulefilenameexw).

hypothesis.json records all three options, ten concrete design closures and seven failure scenarios. The next step is a concrete source/ADR/protocol/owner/funding proposal with fresh critic review. Supervisor compile-only qualification and complete source/native/resource/binding gates remain separate. Frozen V HOLD, R12/R13 sources and every previous helper/report stay unchanged.
'''
md+='\n'+str(len(checks))+' own primary-source/document/file/hash checks passed on the first checker attempt. No current process/environment/code-page values or private configs were queried.\n'
(HERE/'hypothesis.json').write_bytes(enc(report));(HERE/'README.md').write_text(md,encoding='utf-8',newline='\n')
(HERE/'checks.json').write_bytes(enc({'schema':'SOURCE_DOCUMENT_INTEGER_CHECKS_V1','count':len(checks),'checks':checks}))
outputs=[]
for name in ['hypothesis.json','README.md','checks.json']:
 data=(HERE/name).read_bytes();outputs.append({'path':(HERE/name).relative_to(ROOT).as_posix(),'bytes':len(data),'sha256':sha(data)})
helpers=[]
for name in ['fetch-primary.py','fetch-documents.py','seal-hypothesis.py']:
 data=(HERE/name).read_bytes();helpers.append({'path':(HERE/name).relative_to(ROOT).as_posix(),'bytes':len(data),'sha256':sha(data)})
pins={'schema':'OWNED_PROCESS_HYPOTHESIS_SOURCE_PINS_R1','verdict':report['verdict'],'inputs':list(inputs.values()),'outputs':outputs,'own_source_data_helpers':helpers,'no_target_source_or_execution':True}
(HERE/'source-pins.json').write_bytes(enc(pins))
print(json.dumps({'verdict':report['verdict'],'checks':len(checks),'outputs':outputs,'pins_sha256':sha((HERE/'source-pins.json').read_bytes())},indent=2))
