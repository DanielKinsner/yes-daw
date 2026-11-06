"""Own document/schema/file/integer author checks, not a supervisor/runtime implementation."""
import hashlib,json,pathlib,re
ROOT=pathlib.Path(__file__).resolve().parents[2]
HERE=pathlib.Path(__file__).parent
outputs=['protocol-schema.json','author-checks.json','correspondence.json','source-pins.json']
for name in outputs:
 if (HERE/name).exists():raise RuntimeError('PRESERVE_FIRST_COMPLETE_PASS_'+name)
checks=[];inputs={}
def sha(data):return hashlib.sha256(data).hexdigest().upper()
def rec(path):
 path=path.resolve();data=path.read_bytes()
 try:name=path.relative_to(ROOT).as_posix()
 except ValueError:name=path.as_posix()
 row={'path':name,'bytes':len(data),'sha256':sha(data)};inputs[name]=row
 return row
def read(path):rec(path);return path.read_text(encoding='utf-8')
def ck(name,value):
 checks.append({'id':name,'pass':bool(value)})
 if not value:
  (HERE/'failed-author-checks.json').write_text(json.dumps(checks,indent=2)+'\n',encoding='utf-8',newline='\n')
  raise RuntimeError(name)
def enc(value):return (json.dumps(value,indent=2,ensure_ascii=True)+'\n').encode()
tracked={
 'docs/adr/0090-clock-research-owned-process-supervisor.md':'697F696166AF1BB47C3CFC4844EF213A8B56D24A4B8371C2497D23C8A0B252EB',
 'docs/plans/2026-11-06-clock-supervisor-source-plan.md':'050294B6C025D2131BF222234BEC811FB59666BC3CFE414E286FE5E7B1DC6709',
 'build-ci/20261106-clock-supervisor-architecture-independent-review-r1/review.json':'319512FD08A16D03848771AF5750F7C9571A2EEE082B3EE3E9EF7F3F30A883DE',
 'build-ci/20261106-controller-owned-process-hypothesis-r1/hypothesis.json':'79F75D9781519941917CA43897512121649E3776406D724971033E6ACB578AF9',
 'build-ci/20261106-controller-v-wrapper-source-closure-r1/closure.json':'01B4289C2F8E521093987C259D15C5DD4BBF2388EFD9E594A90319C9B0BB5EE6',
 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1/run-clock.ps1':'9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788'}
for path,pin in tracked.items():ck('PRESERVED_INPUT_'+path,rec(ROOT/path)['sha256']==pin)
critic=json.loads(read(ROOT/'build-ci/20261106-clock-supervisor-architecture-independent-review-r1/review.json'))
ck('FIVE_FROZEN_FINDINGS',set(x['id'] for x in critic['findings'])=={'F1','F2','F3','F4','F5'})
adr=read(HERE/'0090-clock-research-owned-process-supervisor.md')
plan=read(HERE/'2026-11-06-clock-supervisor-source-plan.md')
protocol=read(HERE/'protocol-v1.md')
for file,text in [('ADR',adr),('PLAN',plan),('PROTOCOL',protocol)]:
 ck('UTF8_LF_'+file,'\r' not in text and text.endswith('\n'))
 ck('PRIVATE_PROPOSED_'+file,'Proposed' in text or 'private corrected proposal' in text)
 ck('NO_EXECUTION_AUTHORITY_'+file,'execution' in text.lower() and any(x in text.lower() for x in ['grants none','no adr/source/compile/run gate','not source implementation']))
ck('NORMATIVE_COMPANION_LINK_ADR','(protocol-v1.md)' in adr)
ck('NORMATIVE_COMPANION_LINK_PLAN','(protocol-v1.md)' in plan)
ck('NO_ACTUAL_SUPERVISOR_SOURCE',not any(p.suffix in ['.cpp','.h','.cs','.ps1','.exe','.obj','.dll'] for p in HERE.iterdir() if p.is_file()))

request=[('magic',8),('version',4),('totalBytes',4),('childIndex',4),('argumentCount',4),('environmentCount',4),('helperReservation',8),('mandatoryTailReservation',8),('manifestSHA',32),('runSHA',32),('targetSHA',32)]
u32=['version','totalBytes','childIndex','flags','targetPid','targetRawExit','status','primarySite','primaryReason','primaryNativeError','firstSecondarySite','firstSecondaryReason','firstSecondaryOwnNativeError','firstNativeSecondarySite','firstNativeSecondaryReason','firstNativeSecondaryError']
terminal=[('magic',8)]+[(x,4) for x in u32]+[(x,8) for x in ['secondaryFailureCount','sourcePrefixUsed','bodyPolls']]+[(x,32) for x in ['manifestSHA','runSHA','targetSHA']]+[(pipe+name,8) for pipe in ['stdout','stderr'] for name in ['Observed','Written','Omitted','Unretained','StartPosition','EndPosition','ErrorPartial']]+[('cleanupMask',4)]
def offsets(fields):
 rows=[];at=0
 for name,n in fields:rows.append({'field':name,'offset':at,'bytes':n});at+=n
 return rows,at
req_rows,req_size=offsets(request);term_rows,term_size=offsets(terminal)
ck('REQUEST_HEADER140',req_size==140)
ck('TERMINAL308',term_size==308)
ck('REQUEST_COMPILER41260',req_size+40*1028==41260)
ck('REQUEST_NATIVE18644',req_size+18*1028==18644)
ck('FULL_TRANSPORT79472',41260+2*18644+3*308==79472)
ck('EXPLICIT_NO_STRUCT_PADDING',308%8==4 and 'no alignment or trailing padding' in protocol)
req_table=[]
for line in protocol.splitlines():
 match=re.fullmatch(r'\| ([A-Za-z][A-Za-z0-9]*) \| (\d+) \| (\d+) \|',line)
 if match:req_table.append({'field':match[1],'offset':int(match[2]),'bytes':int(match[3])})
ck('ALL_REQUEST_OFFSETS_MATCH_TABLE',req_table==req_rows)
for row in term_rows:
 if row['field'] in ['secondaryFailureCount','sourcePrefixUsed','bodyPolls','cleanupMask']:
  expected=(row['field']+' is u32 at'+str(row['offset'])) if row['field']=='cleanupMask' else (row['field']+' at'+str(row['offset']))
  ck('TERMINAL_NORMATIVE_OFFSET_'+row['field'],expected in protocol)
ck('TERMINAL_PIPE_OFFSETS',next(x for x in term_rows if x['field']=='stdoutObserved')['offset']==192 and next(x for x in term_rows if x['field']=='stderrObserved')['offset']==248)
ck('TERMINAL_FOUR_POSITION_FIELDS',sum('Position' in x['field'] for x in term_rows)==4)
ck('TERMINAL_TWO_ERROR_PARTIAL_FIELDS',sum('ErrorPartial' in x['field'] for x in term_rows)==2)
ck('THREE_IMMUTABLE_FAILURE_RECORDS',all(x in u32 for x in ['primaryReason','firstSecondaryReason','firstNativeSecondaryReason']))

flags=[]
for line in protocol[protocol.index('## 5. Flags'):protocol.index('## 6. Status')].splitlines():
 match=re.fullmatch(r'\| (\d+) \| ([A-Za-z][A-Za-z0-9]*) \| (.+) \|',line)
 if match:flags.append({'bit':int(match[1]),'name':match[2],'meaning':match[3]})
ck('FLAGS_EXACT0_TO27',[x['bit'] for x in flags]==list(range(28)))
ck('FLAG_NAMES_UNIQUE',len({x['name'] for x in flags})==28)
flagmask=sum(1<<x['bit'] for x in flags)
ck('FLAGMASK0FFFFFFF',flagmask==0x0fffffff and 'allowed mask is0x0FFFFFFF' in protocol)
ck('INDEPENDENT_POSITION_FLAGS',all(x in {f['name'] for f in flags} for x in ['StdoutStartKnown','StdoutEndKnown','StderrStartKnown','StderrEndKnown']))
ck('COORDINATOR_COUNTER_VALIDITY_FLAG',flags[27]['name']=='CoordinatorCountersKnown')
ck('WORKER_EOF_SETTLEMENT_INDEPENDENT','EOF and worker settlement are independent observations' in protocol)
ck('NO_NONINTERACTIVE_STDIN_REDIRECTION','supervisor has no redirected standard streams' in protocol)
ck('EXPLICIT_TARGET_NUL_CREATE','CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE' in protocol)
ck('EXACT_THREE_INHERITED_HANDLES','exact target inherited-handle list contains those three handles only' in protocol)
ck('NO_AMBIENT_ENV_OR_STDIN_READ','never reads ambient stdin' in protocol)

reasons=[]
for line in protocol.splitlines():
 match=re.fullmatch(r'\| (\d+) \| ([A-Za-z][A-Za-z0-9]*) \| ([0-9,]+) \|',line)
 if match:reasons.append({'value':int(match[1]),'name':match[2],'allowed_sites':[int(x) for x in match[3].split(',')]})
ck('REASONS_EXACT0_TO47',[x['value'] for x in reasons]==list(range(48)))
ck('REASON_NAMES_UNIQUE',len({x['name'] for x in reasons})==48)
ck('SITE_RANGE0_TO13',all(0<=site<=13 for r in reasons for site in r['allowed_sites']))
ck('SITE0_REASON0_ONLY',[r['value'] for r in reasons if 0 in r['allowed_sites']]==[0])
reason_index={x['name']:x['value'] for x in reasons}
ck('DISTINCT_TIMEOUT_POLL_REASON',reason_index['BodyTimeout']!=reason_index['PollLimit'])
ck('DISTINCT_RAW_STAGE_GUARD_REASON',len({reason_index[x] for x in ['RawLogCap','StagePoolCap','StageUnknownName','StageReparse','StageDepth','StageExtent','StageIo']})==7)

env_names=['SystemRoot','windir','PATH','INCLUDE','LIB','TEMP','TMP','VSLANG']
sorted_env=sorted(env_names,key=str.lower)
ck('EXPLICIT_ENV8_KEYS',len(env_names)==8 and len(set(env_names))==8)
ck('EXPLICIT_SORTED_ENV_ORDER',sorted_env==['INCLUDE','LIB','PATH','SystemRoot','TEMP','TMP','VSLANG','windir'])
ck('ENV_KEY_CELLS43',sum(map(len,env_names))==43)
env_cells=sum(map(len,env_names))+8*512+8+8+1
ck('ENV4156CELLS8312BYTES',env_cells==4156 and 2*env_cells==8312)
ck('NO_BLANKET_ASCII_PATH_NARROWING','No blanket ASCII path restriction' in protocol)
ck('PRESERVE_EMPTY_ARGUMENTS','An empty argument is `""`' in protocol)
ck('PRESERVE_EMPTY_ENV_VALUES','Empty values are retained as `key=` entries' in protocol)
prefix='--request16='
ck('REQUEST_ARG_PREFIX12',len(prefix)==12)
ck('REQUEST_ENCODED_ARG2060',len(prefix)+4*512==2060)
ck('RAW_COMMANDLINE2576',2+512+1+12+2048+1==2576)
ck('TARGET_COMMANDLINE29271',512+2+28*(2*512+3)+1==29271)
ck('TARGET_NATIVE_COMMANDLINE6677',512+2+6*(2*512+3)+1==6677)
ck('DECODED_PATH_NOT_ENCODED512','decoded UTF16 cell' in protocol and 'encoded argument is at most2,060' in protocol)
ck('RUN_ID_DOMAIN_EXPLICIT','ASCII `YESDAW-CLOCK-SUPERVISOR-RUN-V1`, one zero byte, the32 manifest-SHA bytes, u32 code-unit length' in protocol)
ck('RUN_ID_NOT_COPY_ONLY','helper recomputes it' in protocol and 'parent independently recomputes' in protocol)

# Complete finite cleanup-mask extent, no execution of future cleanup code.
max_prefix=0;max_parent_late=0;valid_masks=0
for mask in range(1<<15):
 if mask&(1<<1) and mask&(1<<2):continue
 valid_masks+=1;n=mask.bit_count();max_prefix=max(max_prefix,n)
 late=n+1+(0 if mask&(1<<14) else 1)
 max_parent_late=max(max_parent_late,late)
ck('CLEANUP_PREFIX_MAX14',max_prefix==14)
ck('CLEANUP_PARENT_LATE_MAX15',max_parent_late==15)
ck('CLEANUP_MASK_SOURCE_EXTENT_EXPLICIT','cleanupMask has only bits0..14' in protocol)
ck('REAP_MASK_MUTUAL_EXCLUSION','Bits1/2 are mutually exclusive' in protocol)
ck('NO_OLD13_PRICE_TRANSFER','not an unreviewed transfer of the predecessor13-entry schedule' in protocol)

row_keys=['name','started','raw_exit_observed','raw_exit','timed_out','poll_limited','polls','poll_limit','log_cap','stdout_start','stderr_start','raw_stdout_bytes','raw_stderr_bytes','position_record_complete','raw_bytes_retained','raw_bytes_observed','observed_not_confirmed_retained','omitted_at_cap','unknown_undrained','both_eof','reads_settled','owned_storage_quarantined','cleanup','failure']
runner=read(ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1/run-clock.ps1')
row_line=next(x for x in runner.splitlines() if '$taskRow=[ordered]@{name=$Name;' in x)
actual_keys=re.findall(r'(?:@\{|;)([A-Za-z_]+)=',row_line)
ck('ALL24_R13_CHILD_ROW_KEYS',actual_keys==row_keys and len(row_keys)==24)
for name in row_keys:ck('PARENT_PROJECTION_KEY_'+name,name in protocol)
ck('UNKNOWN_STRENGTHENING_EXPLICIT','documented strengthening of old zero sentinels' in protocol)
ck('NO_RAW_TERMINAL_IN_JSON','No private terminal object/byte array enters JSON' in protocol)
ck('PARTIAL_ERROR_NOT_OBSERVED_CHUNK','never silently counted as a successful logical chunk' in protocol)
ck('CLEANUP_COMPLETED_DATA_ONCE','During cleanup, a successful but previously unconsumed ready result is accounted once' in protocol)
ck('NO_GUARD_CAP_OMISSION_CONFLATION','non-cap name/reparse/depth/extent/I/O guard sets StageGuardRejected, without increasing omitted' in protocol)
ck('PER_SHORT_WRITE_FRESH_SCAN','including a short-write continuation, repeat the complete stage scan/check' in protocol)
ck('PHYSICAL_NOT_CONFIRMED_COUNTER','Physical end-start is independently known' in protocol)
ck('FAILED_WRITE_UNDEFINED_COUNT_NOT_USED','do not use an undefined bytes-written output' in protocol)
ck('STABLE_BYTECOUNTERS_NOT_UNKNOWN_ZERO','their equalities are not used as evidence' in protocol)
for raw,expected in [(0,0),(259,259),(0x7fffffff,2147483647),(0x80000000,-2147483648),(0xffffffff,-1)]:ck('SIGNED_RAW_BITS_'+str(raw),(raw if raw<1<<31 else raw-(1<<32))==expected)
ck('RAW259_ACTUAL_OBSERVATION_REQUIRED','259 is a valid observed code, not proof of liveness' in protocol)

for key,literal in {
 'PREFIX_CUT':'BeginTail closes body admission under that same state',
 'TAIL_NO_FAKE_ACTUAL':'It is not reported as measured complete usage',
 'ENTIRE_RESERVATION_RETAINED':'never automatically refunds helperReservation-sourcePrefixUsed',
 'TAIL_FIXED_CLEANUP':'The entire bounded tail is reserved statically',
 'NO_POST_CUT_DEBIT':'No prefix debit may appear after the cut',
 'DISTINCT_EXIT_COMMIT':'only parent-observed helper exit0 plus the full terminal success matrix commits',
 'LATE_FAILURE_NONCOMMIT':'Any failed/no-progress write, flush, close or later mandatory action after record freeze requires exit11',
 'PRESERVE_INDIVIDUAL_VALID_FACTS':'retains those independently valid prefix observations',
 'NO_NOEXEC_STUB':'No R13 price or aggregate slack establishes a new value',
 'NO_COUNTER_RESET':'tail never relies on unused prefix units or resets the bank',
 'NO_CLEANUP_FREE':'OS process exit/handle close earns no scratch refund',
 'NO_NEW_TARGET_TIMEOUT_FROM_PARENT':'not a new target input-admission rule or a fabricated target timeout',
 'NO_CREATED_SUSPENDED_ATOMICITY':'do not substitute an unproved create-suspended/manual-assign atomicity claim',
 'NONRETURN_PENDING_QUARANTINE':'rather than unwinding pending stack storage',
 'EXPLICIT_UNQUALIFIED_NATIVE_TIME':'actual source/binary/native ABI and SDK ownership/time premises remain separate proof gates',
 'NO_CURRENT_ACCEPTANCE':'This private proposal grants none of those later gates'
}.items():ck(key,literal.lower() in protocol.lower())
ck('ALL_FIVE_HELPER_EXITS',all(x in protocol for x in ['codes:0 complete','10 no current','11 publication','12 a complete status4','13 the prefix meter']))
ck('PRECEDENCE_EXPLICIT','Precedence is11 for late failure, else13 for invalid meter, else12 for status4, else0' in protocol)
ck('NO_GATE_CIRCULARITY','No final-source/ABI proof is demanded merely to author its unrun candidate' in protocol)
ck('SOURCE_ONLY_AFTER_ROOT_AUTH','Root must explicitly authorize the subsequent unrun source candidate' in plan)
ck('FULL_EXECUTION_SEPARATE','Before any later helper/fixture/Clock attempt, require complete actual source/native/resource/binding acceptance' in plan)
ck('COUNTERPART_PROTO_LENGTHS_ALL_DOCS',all('308' in x and '140' in x and '41,260' in x and '18,644' in x for x in [adr,plan,protocol]))
ck('UNRAISED_GLOBAL_SCRATCH','unchanged32MiB' in adr and 'under32MiB' in plan and 'unchanged32MiB' in protocol)
ck('SOURCE_PROOF_NO_TARGET_EXECUTION',not any((HERE/x).exists() for x in ['execution-r1','run.ps1','supervisor.cpp']))
for path,pin in tracked.items():ck('INPUT_PRESERVED_AFTER_CHECK_'+path,rec(ROOT/path)['sha256']==pin)

schema={'schema':'PROPOSED_CLOCK_SUPERVISOR_PROTOCOL_V1_R2_DOCUMENT_SCHEMA_NOT_IMPLEMENTATION','status':'PRIVATE_PROPOSED_UNIMPLEMENTED_UNRUN_REQUIRES_INDEPENDENT_REVIEW','request':{'magic':'YCSREQ01','version':1,'header_bytes':140,'fields':req_rows,'max_string_units':512,'environment_count':8,'argument_counts':[28,6,6],'max_bytes_by_child':[41260,18644,18644]},'terminal':{'magic':'YCSRES01','version':1,'bytes':308,'fields':term_rows,'flags':flags,'allowed_flag_mask':'0x0FFFFFFF','status_codes':{'0':'settled_target_success','1':'settled_target_nonzero','2':'pre_execution_refusal','3':'recorded_failure_with_settled_owners','4':'unresolved_quarantine'},'reasons':reasons,'cleanup_mask_bits':15,'cleanup_reap_mutually_exclusive_bits':[1,2],'max_prefix_cleanup_rows':14,'max_parent_late_cleanup_rows':15},'helper_exits':{'0':'complete_publication_and_post_record_actions','10':'current_terminal_unavailable','11':'late_publication_or_post_record_failure','12':'quarantine_record_published','13':'invalid_prefix_meter_record_published'},'transport':{'requests':3,'terminals':3,'files':6,'directory_entries':1,'full_max_bytes':79472},'commandline':{'request_prefix':'--request16=','decoded_request_path_max_units':512,'encoded_argument_max_units':2060,'raw_commandline_max_including_NUL':2576,'target_max_including_NUL':[29271,6677,6677]},'environment':{'request_order':env_names,'native_block_order':sorted_env,'fixed_key_units':43,'native_block_max_units':4156,'native_block_max_bytes':8312},'parent_row_keys':row_keys,'cleanup_mask_arithmetic':{'valid_masks_checked':valid_masks,'max_prefix':max_prefix,'max_parent_late':max_parent_late},'future_execution_gates_all_false':True}
correspondence={'schema':'PROPOSED_SUPERVISOR_R2_AUTHORING_CORRESPONDENCE','status':'AUTHOR_PROPOSAL_NOT_INDEPENDENT_ACCEPTANCE','first_complete_own_check_count':len(checks),'findings_addressed':[
 {'id':'F1','concrete_change':'Four separately flagged u64 start/end positions; confirmed writes retained separately;308-byte real-field layout; combined parent completeness mapping.'},
 {'id':'F2','concrete_change':'28 flags including aggregate-counter validity and possible execution;48 reasons/site pairs; exact statuses, source-code259/signed projection, EOF-short/error-partial/cleanup accounting, first primary/secondary/native records, bounded cleanup mask and all24 parent keys.'},
 {'id':'F3','concrete_change':'Exact2576-cell raw command grammar, uppercase hex path rules, root-SHA byte recipe, explicit helper/target8-value environments and4156-cell block, nativeNUL stdin and exact target quote algorithm/argument domain.'},
 {'id':'F4','concrete_change':'Request140 adds mandatory-tail reservation; linearizable BeginTail prefix cut; unobserved prepaid cleanup/publication tail; all helper reservation retained; five helper exits with late-failure noncommit.'},
 {'id':'F5','concrete_change':'Architecture acceptance/root source-only authorization, frozen candidate whole reviews, separate compile-only purpose qualification and full actual execution proof/gate are ordered without circular prerequisite.'}],
 'root_steering_applied':'A: trusted-launcher null-stdin premise, no supervisor redirects/ambient stdin reads; explicit inheritable target NUL with exact target handle list. No EOF-pipe/implicit ACP StreamWriter path selected.',
 'semantic_changes_openly_declared':['308-byte terminal and140-byte request are unimplemented ProposedR2 wire choices, not compatibility claims with an executedR1.','Unavailable failure row values become null, instead of old zero sentinels; no new serialized child-row keys.','Confirmed native short-write increments can preserve finer known progress than old opaque FileStream.Write failures.','Cleanup list is a normative bounded mask projection with fresh maximum15, not transferred old13 price or a raised global policy cap.'],
 'remaining_external_proofs':['Actual creation-time/nesting/creator-death/reference/job-accounting/cancellation/native-time semantics.','Complete source/function/ABI/import/CRT/TLS/stack/handle/pipe/crypto/path/environment and cold owners under whole32MiB and retained/profile limits.','Real same-origin helper prefix/tail/caller/source work prices and complete phase schedules; no numeric values accepted here.','Actual source/binary/native/binding and separate helper/fixture/Clock execution authority.'],
 'tracked_subjects_preserved':tracked,'proposed_documents':[rec(HERE/x) for x in ['0090-clock-research-owned-process-supervisor.md','2026-11-06-clock-supervisor-source-plan.md','protocol-v1.md']]}
(HERE/'protocol-schema.json').write_bytes(enc(schema))
(HERE/'author-checks.json').write_bytes(enc({'schema':'OWN_DOCUMENT_DATA_INTEGER_CHECKS_R2','status':'PASS_PRIVATE_AUTHOR_CHECKS_NOT_INDEPENDENT_REVIEW','count':len(checks),'checks':checks}))
(HERE/'correspondence.json').write_bytes(enc(correspondence))
artifact_names=['0090-clock-research-owned-process-supervisor.md','2026-11-06-clock-supervisor-source-plan.md','protocol-v1.md','protocol-schema.json','author-checks.json','correspondence.json']
artifacts=[rec(HERE/x) for x in artifact_names]
pins={'schema':'CLOCK_SUPERVISOR_PRIVATE_PROTOCOL_AUTHORING_PINS_R2','status':'PRIVATE_PROPOSED_FROZEN_FIRST_COMPLETE_OWN_PASS_REQUIRES_FRESH_INDEPENDENT_REVIEW','own_check_count':len(checks),'inputs':list(inputs.values()),'proposed_artifacts':artifacts,'own_document_checker':rec(pathlib.Path(__file__)),'execution':{x:False for x in ['target_source_implementation','ADR_acceptance','PowerShell_parse_AST_import','compiler','metadata','program','helper','Clock_stage','GUI','Git']}}
(HERE/'source-pins.json').write_bytes(enc(pins))
print(json.dumps({'status':pins['status'],'checks':len(checks),'artifacts':artifacts,'pins_sha256':sha((HERE/'source-pins.json').read_bytes())},indent=2))
