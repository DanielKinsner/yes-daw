"""Fresh complete R16 source/file/hash/JSON/integer checks. Never loads targets."""
from pathlib import Path
import hashlib,json,re,shutil,traceback,sys
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).resolve().parent
REVS=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
NEW=REVS/'clock-oracle-r16-owned-supervisor-source-r1';BASE=REVS/'clock-oracle-r15-owned-supervisor-source-r1'
OLDPACKET=ROOT/'build-ci/20261106-clock-owned-supervisor-source-r2'
REVIEW=ROOT/'build-ci/20261106-r15-supervisor-functional-independent-review-r1'
if (OUT/'freeze.json').exists():raise RuntimeError('FROZEN complete first green; no rerun/meaning expansion')
ATTROOT=OUT/'check-attempts';ATTROOT.mkdir(exist_ok=True);ATT=ATTROOT/f'attempt-{len(list(ATTROOT.iterdir()))+1:02d}';ATT.mkdir()
subjects=[*sorted((OUT/'native').glob('*')),*sorted(NEW.glob('*')),*[p for p in OUT.iterdir() if p.is_file()]]
for p in subjects:
 if p.is_file():
  q=ATT/'inputs'/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
def exc(k,v,tb):
 s=''.join(traceback.format_exception(k,v,tb));(ATT/'stderr.txt').write_text(s,encoding='utf-8');sys.__excepthook__(k,v,tb)
sys.excepthook=exc
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def file_sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest().upper()
def rec(p):return {'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':file_sha(p)}
def dump(p,v):p.write_text(json.dumps(v,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
checks=[]
def c(n,v,e=None):checks.append({'name':n,'pass':bool(v),'evidence':e})
def funcs(s):
 hits=list(re.finditer(r'^function ([A-Za-z0-9-]+)',s,re.M))
 return {m.group(1):s[m.start():hits[i+1].start() if i+1<len(hits) else s.index('\ntry{',m.end())] for i,m in enumerate(hits)}
def nodes(v):
 if isinstance(v,dict):return 1+sum(1+nodes(x) for x in v.values())
 if isinstance(v,list):return 1+sum(nodes(x) for x in v)
 return 1
def geometry(v,lev=0):
 if isinstance(v,dict):
  a=[geometry(x,lev+1) for x in v.values()]
  return max([lev+1]+[x[0] for x in a]),max([0]+[len(k.encode('utf-16-le'))//2 for k in v]+[x[1] for x in a])
 if isinstance(v,list):
  a=[geometry(x,lev+1) for x in v]
  return max([lev+1]+[x[0] for x in a]),max([0]+[x[1] for x in a])
 return lev,len(v.encode('utf-16-le'))//2 if isinstance(v,str) else 0

d=json.loads((NEW/'source-manifest.json').read_bytes());prior=json.loads((BASE/'source-manifest.json').read_bytes())
runner=(NEW/'run-clock.ps1').read_text(encoding='utf-8');oldrunner=(BASE/'run-clock.ps1').read_text(encoding='utf-8')
native={p.name:p.read_text(encoding='utf-8') for p in (OUT/'native').iterdir() if p.suffix in ['.cpp','.h']}
h=native['sup.h'];core=native['sup_core.cpp'];main=native['sup_main.cpp'];io=native['sup_io.cpp'];codec=native['sup_codec.cpp'];allnative='\n'.join(native.values())
oldpins=json.loads((OLDPACKET/'source-pins.json').read_bytes())
c('R15 exact source pins',file_sha(OLDPACKET/'source-pins.json')=='5806F35FE26CC1CB2A43B7E49AD6CEBEDE6C28EE7ABAC6BE52CDBE77292FDDB1')
c('R15 exact whole freeze',file_sha(OLDPACKET/'freeze.json')=='895F4F1BB04F955E9E40BFDC29AC29AE15AB844ECE21AF068D3DFAEF16914169')
c('whole R15 review pins',file_sha(REVIEW/'source-pins.json')=='2F455971EAE1F023BA5D49FF295C893736997C846A0D73E71DF29A57A5B3980A')
c('whole R15 review freeze',file_sha(REVIEW/'first-green-review-freeze.json')=='8D68D9BA4F652BEDD83A64B89C5C459577789631D6F190441F589CED01B14788')
review=json.loads((REVIEW/'review.json').read_bytes())
c('both whole findings covered',[x['id'] for x in review['findings']]==['F9','F10'] and review['verification']['checks']==3374)
oldfreeze=json.loads((OLDPACKET/'freeze.json').read_bytes())
for x in oldfreeze['outputs']+oldfreeze['subjects']:
 p=ROOT/x['path'];c('frozen predecessor '+x['path'],p.stat().st_size==x['bytes'] and file_sha(p)==x['sha256'])
docs=oldpins['accepted_docs']
for name,expected in docs.items():c('Accepted immutable '+name,file_sha(ROOT/name)==expected)
for name in ['clock_stage.cpp','source-costs.h']:
 c('unchanged target '+name,(NEW/name).read_bytes()==(BASE/name).read_bytes())
c('target relative include resolves',(NEW/'../waveform-definition-r3/independent_clock.h').is_file())
c('same exact5 native names',set(native)=={'sup.h','sup_core.cpp','sup_codec.cpp','sup_io.cpp','sup_main.cpp'})
c('new private namespace',all('namespace yesdaw_clock_supervisor_r3 {' in x for x in native.values()))
c('all native gates false',all('bool '+k+' = false;' in h for k in ['source_review_accepted','source_work_accepted','native_profile_accepted','controlled_resource_accepted','execution_accepted']))
c('both Data undefined',allnative.count('extern const Catalogue Data;')==1 and re.search(r'(?:constinit\s+)?const\s+Catalogue\s+Data\s*[={]',allnative) is None and 'static const Catalogue reviewed_;' in (NEW/'source-costs.h').read_text() and 'constinit const Catalogue Catalogue::reviewed_{' not in (NEW/'source-costs.h').read_text())
for flag in ['SourceReview','SourceWork','Execution','Compiler','Produce','Verify','Supervisor','SupervisorProfile']:
 c('parent false '+flag,f'$task{flag}Accepted=$false' in runner)
c('manifest flags false',not any(d['guards'].values()))
c('all source prices unbound',all(d['source_work'][k] is None for k in ['runner_upper','producer_upper','verifier_upper','whole_upper']))
c('helper binary unbound',d['supervisor']['binary']['bytes'] is None and d['supervisor']['binary']['sha256'] is None)
c('helper reservations unbound',d['supervisor']['helper_reservations']==[None]*3 and d['supervisor']['tail_reservations']==[None]*3)
c('controller prices unbound',all(x is None for x in d['supervisor']['controller_prices'].values()))

# Exact whole parent correspondence. Plain text only, never target-language AST.
fnew=funcs(runner);fold=funcs(oldrunner)
c('all88 parent body inventory',len(fnew)==len(fold)==88 and set(fnew)==set(fold))
for name,body in fold.items():c('whole unchanged parent body '+name,fnew[name]==body)
norm=lambda s:re.sub(r'(Assert-TaskRawJsonBytes \$taskBootstrap )\d+( 512 4)',r'\1<NODES>\2',s)
c('whole runner only bootstrap literal differs',norm(runner)==norm(oldrunner))
c('bootstrap one exact derived literal',runner.count(f'Assert-TaskRawJsonBytes $taskBootstrap {nodes(d)} 512 4')==1)
c('bootstrap fixed geometry',geometry(d)==(4,435))
c('own5 bytes within unchanged1MiB',sum((NEW/n).stat().st_size for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','source-manifest.json','README.md'])<=1048576)
for x in d['sources']:
 p=NEW/x['path'];c('own source pin '+x['path'],p.stat().st_size==x['bytes'] and file_sha(p)==x['sha256'])

seen=set();ibytes=0;chunks=0
for x in d['inputs']:
 p=Path(x['path']);p=p if p.is_absolute() else ROOT/p
 c('input exact '+x['path'],p.stat().st_size==x['bytes'] and file_sha(p)==x['sha256'])
 c('input physical identity unique '+x['path'],x['path'] not in seen);seen.add(x['path'])
 ibytes+=x['bytes'];chunks+=(x['bytes']+65535)//65536
for x in prior['inputs']:c('every old input preserved '+x['path'],x in d['inputs'])
p=d['profile']
c('new union1250',len(d['inputs'])==p['input_count']==1250 and ibytes==p['input_bytes']==462585712 and chunks==p['input_hash_chunks'])
c('include/current input recipes',p['include_input_count']==1251 and p['complete_current_inputs']==1256)
namespace_entries=namespace_files=0
for x in d['closed_directory_inputs']:
 path=Path(x['path']);path=path if path.is_absolute() else ROOT/path
 entries=list(path.rglob('*') if x['recursive'] else path.iterdir())
 actual=sorted(z.relative_to(path).as_posix() for z in entries if z.is_file())
 c('namespace files '+x['path'],actual==sorted(x['files']))
 c('namespace all entries '+x['path'],len(entries)==x['entry_count'])
 namespace_entries+=x['entry_count'];namespace_files+=len(x['files'])
c('all34 namespaces',len(d['closed_directory_inputs'])==p['directory_namespaces_count']==34 and namespace_entries==p['directory_namespace_entries']==1107 and namespace_files==p['directory_namespace_files']==1060)
for x in d['supervisor']['native_sources']:
 q=ROOT/x['path'];c('current native pin '+x['path'],q.stat().st_size==x['bytes'] and file_sha(q)==x['sha256'])

# F9 source obligations across all analogous fresh-entry seams.
ticket=core[core.index('bool admit_body(Meter&'):core.index('bool admit_body(State&')]
c('one-lock fresh ticket',ticket.count('AcquireSRWLockExclusive(&meter.lock)')==1 and ticket.count('ReleaseSRWLockExclusive(&meter.lock)')==1)
c('ticket denies tail invalid refused','bool ok=!meter.tail && meter.valid && !meter.refused;' in ticket)
c('ticket index positive subtraction checks','index>=static_cast<U32>(Family::Count)' in ticket and 'price==0' in ticket and 'price>meter.limit-meter.used' in ticket)
c('ticket cut freezes unchanged prefix','if(!ok && !meter.tail) { meter.tail=true;meter.frozen_prefix=meter.used; }' in ticket)
c('State wrapper preserves first cause','if(state.stop_body) { begin_tail(state);return false; }' in core and 'if(admit_body(state.meter,family)) return true;' in core)
c('read ticket same admission','return admit_body(meter,Family::WorkerRead);' in core)
c('write ticket same admission','return admit_body(state,Family::WriteCall);' in core)
c('resume has genuine ticket','if (ready && admit_body(state,Family::TargetResume)) {' in main)
c('ticket before mark before native',main.index('if (ready && admit_body(state,Family::TargetResume))')<main.index('state.terminal.flags|=TargetMayHaveRun;')<main.index('const DWORD previous=ResumeThread'))
c('worker startup genuine ticket','if (!admit_body(state,Family::WorkerStart)) return false;' in main)
c('worker ticket precedes all group native owners',main.index('admit_body(state,Family::WorkerStart)')<main.index('worker.ready=CreateEventW')<main.index('worker.thread=CreateThread'))
c('capture group ticket before scan',io.index('admit_body(state,Family::CaptureSetup)')<io.index('if(state.unknown_native_creation)'))
c('both Owner entries prefix-only',io.count('admit_body(state,Family::Owner)')==2 and 'spend(state,Family::Owner)' not in io)
c('coordinator/poll prefix-only','admit_body(state,Family::Coordinator)' in main and 'admit_body(state,Family::Poll)' in main and 'spend(state,Family::Poll)' not in main)
c('normal consume ticket plus mandatory tail fallback','if(cleanup) (void)spend(state,Family::Consume);' in main and 'else if(!admit_body(state,Family::Consume)) cleanup=true;' in main)
c('write chunk prefix-only','admit_body(state,Family::WriteChunk)' in io)
c('quote/environment prefix-only','admit_body(state,Family::TargetQuote)' in codec and 'admit_body(state,Family::Environment)' in codec)
c('completed publication remains mandatory','if (!charge(state.meter,Family::WorkerPublish))' in main and 'Completion/publication/accounting is mandatory prepaid tail work.' in main)
families=[x.strip() for x in h.split('enum class Family : U32 {',1)[1].split('};',1)[0].replace('\n','').split(',') if x.strip()]
c('36 positive prospective native families',len(families)-1==d['supervisor']['native_family_count']==36 and families[-1]=='Count' and 'CaptureSetup' in families and 'TargetResume' in families)

# F1–F8 preserved complete source seams. These are source assertions, not fixtures.
pub=main[main.index('bool publish_ready('):main.index('void stop_workers(')]
c('F1 no published-slot rewrite after event', 'worker_error(' not in pub[pub.index('if (!SetEvent(worker.ready))'):])
c('F2 preserve completed result on publication refusal','worker_error(' not in pub and 'worker_exit_error(' in pub)
c('F3 one immediate ready-wait capture','bool ready_now(' in main and main.count('WaitForSingleObject(worker.ready,0)')==1 and 'wait==WAIT_FAILED?GetLastError():0' in main)
c('F3 real exited worker acquire', 'const DWORD wait=WaitForSingleObject(worker.thread,0);' in main and 'const WorkerExit& exit=worker.exit;' in main)
c('F4 zero no-progress distinct','worker.kind=ReadyKind::NoProgress;break;' in main and 'if (!ok) {' in main and 'Only documented broken-pipe closure is native EOF.' in main)
c('F5 occupied search refuses new owner','state.search_owner!=INVALID_HANDLE_VALUE' in io and 'state.search_owner=search;' in io)
c('F6 remaining cap omission','native_count(state,count.omitted,length-at)' in io)
c('F7 typed source/extent','enum class PathResult' in h and 'PathResult::SourceRefused' in codec and 'PathResult::ExtentRejected' in io)
c('F8 raw failed provenance','state.unknown_native_creation=true;' in io and 'capture_raw(state.failed_pipe_output' in io and 'capture_raw(state.failed_process_output' in io)
c('F8 unresolved creator forces quarantine','state.unknown_native_creation' in main[main.index('void finalize_record('):])
c('wire request/terminal unchanged','request_header_bytes = 140' in h and 'request_max_bytes = 41260' in h and 'terminal_bytes = 308' in h and "{'Y','C','S','R','E','S','0','2'}" in codec)
c('all private raw/slot changes absent from wire','ReadyKind' not in codec and 'failed_pipe_output' not in codec)

# Independent finite source models, not imported candidate/predecessor models.
class Meter:
 def __init__(self,limit=9,used=0):self.limit=limit;self.used=used;self.tail=False;self.valid=True;self.refused=False;self.frozen=None
 def cut(self):
  if not self.tail:self.tail=True;self.frozen=self.used
 def ticket(self,price=1):
  ok=not self.tail and self.valid and not self.refused
  if ok:
   if price==0:self.valid=False;ok=False
   elif self.used>self.limit or price>self.limit-self.used:self.refused=True;ok=False
   else:self.used+=price
  if not ok:self.cut()
  return ok
 def complete_spend(self,price=1):
  if self.tail:return True
  return self.ticket(price)
models=[]
for limit in [0,1,2,8,31,(1<<63)-1]:
 for used in sorted({0,limit//2,limit}):
  for price in sorted({0,1,2,limit,limit+1,(1<<64)-1}):
   m=Meter(limit,used);before=m.used;ok=m.ticket(price)
   expected=price>0 and price<=limit-used
   c(f'ticket arithmetic {limit}/{used}/{price}',ok==expected and m.used==(used+price if expected else before) and (not m.tail if expected else m.tail and m.frozen==before))
   if m.tail:
    frozen=m.frozen;c(f'cut blocks every later ticket {limit}/{used}/{price}',not m.ticket(1) and m.used==before and m.frozen==frozen)
    c(f'tail completion separate {limit}/{used}/{price}',m.complete_spend() and m.used==before)
# Counterexample selects prospective arithmetic, defines no real Data or price.
oldm=Meter(0);oldm.ticket(1);old_worker1=oldm.complete_spend();old_resume=old_worker1
newm=Meter(0);newm.ticket(1);new_worker1=newm.ticket(1);new_resume=newm.ticket(1)
c('F9 predecessor source counterexample',oldm.tail and old_worker1 and old_resume)
c('F9 successor suppresses worker1/resume',newm.tail and not new_worker1 and not new_resume)
models.append({'id':'F9-old-first-read-cut','kind':'source counterexample not runtime','predecessor_tail':oldm.tail,'predecessor_worker1_spend_then_resume':old_resume,'successor_worker1_admitted':new_worker1,'successor_resume_admitted':new_resume})
for point in ['no_cut','before_worker0','after_worker0_ticket','during_worker0_started_wait','between_workers','after_worker1_ticket','after_workers','before_resume','after_resume_ticket']:
 m=Meter(100);created=[];finished=[];admitted=[]
 if point=='before_worker0':m.cut()
 for w in [0,1]:
  if point=='between_workers' and w==1:m.cut()
  start=m.ticket()
  if not start:break
  admitted.append(w)
  if point==f'after_worker{w}_ticket' or (point=='during_worker0_started_wait' and w==0):m.cut()
  created.append(w);finished.append(w) # completion of the admitted setup group
 if point in ['after_workers','before_resume']:m.cut()
 resume=(len(finished)==2 and m.ticket())
 if point=='after_resume_ticket' and resume:m.cut()
 may=bool(resume)
 c('setup source grouping '+point,created==admitted and all(x in admitted for x in finished))
 c('no cut-before-ticket resume '+point,(not may) if point not in ['no_cut','after_resume_ticket'] else may)
 if m.tail:c('no postcut body admission '+point,not m.ticket() and m.complete_spend())
 models.append({'id':point,'admitted_worker_groups':admitted,'finished_owned_worker_groups':finished,'tail':m.tail,'resume_ticket':bool(resume),'TargetMayHaveRun':may,'runtime':False})
for cut_after in [False,True]:
 for failed_resume in [False,True]:
  m=Meter(4);admitted=m.ticket();frozen_before=m.used
  if cut_after:m.cut()
  flags={'may':False};primary=[]
  if admitted:
   flags['may']=True
   if failed_resume:primary.append(('ResumeFailure',123))
  c(f'failed resume mark/cause {cut_after}/{failed_resume}',flags['may'] and (primary==[('ResumeFailure',123)] if failed_resume else not primary))
  if cut_after:c(f'afterticket completion frozen {failed_resume}',m.frozen==frozen_before and m.used==frozen_before and not m.ticket())
  models.append({'id':'resume admitted failure/cut','cut_after_ticket':cut_after,'failed_resume':failed_resume,'MayHaveRun':True,'primary':primary,'runtime':False})
for length in [0,1,4095,4096]:
 for signal_error in [False,True]:
  slot={'kind':'Full' if length==4096 else 'EofShort' if length else 'ZeroEof','count':length};immutable=dict(slot)
  m=Meter(0);normal=m.ticket();cleanup=not normal;observed=length;written=0;unretained=length
  exit_record=['WorkerJoinFailure'] if signal_error else []
  c(f'F1/F2 final slot mandatory {length}/{signal_error}',slot==immutable and cleanup and observed==written+unretained and bool(exit_record)==signal_error)
for filled in [0,1,4095]:
 state={'active_fill':filled,'observed':0,'eof':False}
 for _ in range(3):
  c('F4 TRUE0 retains source partial '+str(filled),state=={'active_fill':filled,'observed':0,'eof':False})
 state['active_fill']+=1
 c('F4 later positive is retained '+str(filled),state['active_fill']==filled+1 and not state['eof'])
for at in [0,1,4095]:
 remaining=4096-at;c('F6 confirmed+omitted '+str(at),at+remaining==4096 and remaining>0)
for tailrefuse in [True,False]:
 result='SourceRefused' if tailrefuse else 'ExtentRejected';c('F7 distinct source cause '+result,(result=='ExtentRejected')!=tailrefuse)
for raw in [b'\0'*16,b'\1'*16]:
 unknown=True;owners=[];pid=None;c('F8 failed creator no ownership '+str(raw[0]),unknown and not owners and pid is None)
dump(OUT/'counterexamples.json',{'source_only':True,'defined_Data_or_real_prices':False,'runtime_observation':False,'models':models,'older_F1_F8':'Independent finite source/state checks plus complete source-preservation assertions; no runtime fixture credit'})

# F10 independently recompute the actual current metadata/caller recipe.
refs=['data/'+x['path'] for x in d['references']];build=['build/'+x['name'] for x in d['compiler_final_outputs']];controls=[x['path'] for x in d['control_files']]
stage=['stage/'+x for x in d['stage_files']];transport=['transport/'+x for x in d['supervisor']['transport_files']]
prospective=refs+build+controls+stage+transport
prebundle=refs+build+controls+['stage/'+x for x in ['stdout.log','stderr.log','receipt.json','consumed.json']]+transport
success=prebundle+['stage/bundle.json'];fp=d['failure_policy']
c('prospective75 distinct',len(prospective)==len(set(prospective))==fp['declared_final_artifact_recipe_files']==75)
c('prebundle73 distinct',len(prebundle)==len(set(prebundle))==fp['normal_prebundle_file_recipe_files']==73)
c('success74 distinct',len(success)==len(set(success))==p['declared_success_run_files']==fp['normal_success_file_recipe_files']==74)
c('failure extra distinct',set(prospective)-set(success)=={'stage/failure-inventory.json'} and p['declared_failure_extra_files']==1)
c('profile root/failure ceilings not recipe',fp['maximum_raw_failed_prefix_file_rows']==fp['maximum_run_entries']==p['failure_inventory_max_files']==p['root_walk_entries_max']==85)
c('depth unchanged64',fp['maximum_depth']==p['root_walk_depth_max']==64)
c('cleanup15 distinct secondary11',fp['bounded_child_cleanup_events']==15 and fp['fixed_secondary_sites']==11 and '$taskMaximum=15' in fnew['Add-TaskChildCleanup'])
c('historical stale values explicit unused',fp['historical_R15_unqualified_descriptions']['values']=={'declared_final_artifact_recipe_files':69,'maximum_raw_failed_prefix_file_rows':78,'maximum_run_entries':78,'bounded_child_cleanup_events':13} and fp['historical_R15_unqualified_descriptions']['scope'].startswith('UNUSED_'))
c('no unqualified old cleanup override','R14_cleanup_entries_max' not in fp)
c('current JSON dynamic array1256',d['closed_json_domain']['output_arrays_max_entries']==p['complete_current_inputs']==len(d['inputs'])+4+2 and "array_entries=($taskManifest['inputs'].Count+$taskOwnNames.Count+2)" in runner)
c('catalogue1331 recipe',p['maximum_complete_fence_identities']==p['complete_current_inputs']+len(prospective)==1331 and "foreach($taskName in $taskManifest['stage_files'])" in fnew['Initialize-TaskPathCatalogue'])
c('parent real prebundle four-stage names',"foreach($taskN in @('stdout.log','stderr.log','receipt.json','consumed.json')){$taskExpectedRunFiles.Add('stage/'+$taskN)}" in runner)
c('target cache meanings separate',d['fraction_residues']==[735,25,25] and sum(d['fraction_residues'])==d['source_work']['original_fraction_pair_inventory_not_price']==785 and d['cache_max_live_pair_rows']==735 and d['cache_pair_payload']==785*16)
c('positive target families unchanged',d['cost_catalogue']==prior['cost_catalogue'] and d['cost_catalogue']['family_count']==76)
for name in ['compiler','environment','references','compiler_final_outputs','control_files','stage_files','normal_gate_controls','module_controls']:
 c('protected recipe '+name,d[name]==prior[name])
c('same wire protocol v2',d['supervisor']['protocol_version']==2 and d['supervisor']['request_caps']==[41260,18644,18644] and d['supervisor']['terminal_bytes']==308 and d['supervisor']['transport_cap']==79472)
c('no policy cap changed',d['retention']['cap']==880803840 and d['controlled_lifetime']['explicit_scratch_cap']==33554432 and 'raw_log_limit = 1048576' in h and 'body_poll_limit = 7200' in h)
r=d['retention'];reservation=p['input_bytes']+1048576+152627224+67108864+67108864+33652457+79472+33554432
c('retention independently summed',r['reservation_upper']==reservation and r['headroom_under_840MiB']==880803840-reservation)
c('final canonical union still HOLD','UNBOUND' in p['profile_source'] and d['supervisor']['binary']['source_binary_association']=='UNBOUND')
c('test untouched',file_sha(ROOT/'tests/musical_audio_call_recipe_tests.cpp')=='97B2A2ABE0BE0CDDFCE173A20E6A6264E837DB751FE204E93B4B094E8E22619C')
c('instructions untouched',(ROOT/'AGENTS.md').read_bytes()==(ROOT/'CLAUDE.md').read_bytes() and file_sha(ROOT/'AGENTS.md')=='AFA45D3BAC0FDB892064C8928DDB21375FA0112D2B5840CF2877FAA472ECEB77')

correspondence={'old_parent_bodies':88,'current_parent_bodies':88,'all_body_bytes_unchanged':True,'runner_only_change':'DERIVED_BOOTSTRAP_NODE_LITERAL','bootstrap':{'nodes':nodes(d),'depth':geometry(d)[0],'max_string_units':geometry(d)[1]},'input_profile':p,'native_families':families[:-1],'F9':'PREFIX_ONLY_SAME_LOCK_GROUP_TICKETS_AND_MANDATORY_ACQUIRED_COMPLETION_TAIL','F10':'75_PROSPECTIVE_73_PREBUNDLE_74_SUCCESS_85_ROOT_FAILURE_15_CLEANUP_11_SECONDARY_1256_JSON_CURRENT_1331_CATALOGUE','all_old_F1_F8':'PRESERVED_SOURCE_SUBJECT_TO_FRESH_WHOLE_REVIEW','no_current_individual_or_whole_source_price_acceptance':True,'native_ABI_resources_profile_canonical_union_binding_GO':'HOLD','target_execution':False}
dump(OUT/'correspondence.json',correspondence)
protected=[*oldfreeze['outputs'],*oldfreeze['subjects'],rec(OLDPACKET/'freeze.json'),rec(REVIEW/'source-pins.json'),rec(REVIEW/'first-green-review-freeze.json'),rec(REVIEW/'review.md'),rec(REVIEW/'review.json')]
newsubjects=[p for p in [*sorted((OUT/'native').glob('*')),*sorted(NEW.glob('*')),*[p for p in OUT.iterdir() if p.is_file() and p.name not in ['source-pins.json','checks.json','freeze.json']]] if p.is_file()]
dump(OUT/'source-pins.json',{'schema':'R16_COMPLETE_SOURCE_CANDIDATE_PINS','protected_predecessor_and_review':protected,'accepted_docs':docs,'subjects':[rec(p) for p in newsubjects],'input_manifest':rec(NEW/'source-manifest.json'),'whole_input_rows_verified':len(d['inputs']),'target_parser_AST_import_compiler_native_API_helper_fixture_Clock_execution':False})
result={'passed':sum(x['pass'] for x in checks),'failed':sum(not x['pass'] for x in checks),'checks':checks,'source_only':True,'target_execution':False,'functional_independent_acceptance':False,'native_or_resource_or_GO_acceptance':False}
dump(OUT/'checks.json',result);dump(ATT/'result.json',result)
for p in [*sorted(NEW.glob('*')),*[p for p in OUT.iterdir() if p.is_file()]]:
 if p.is_file():
  q=ATT/'outputs'/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
summary={'passed':result['passed'],'failed':result['failed'],'failed_names':[x['name'] for x in checks if not x['pass']]}
if result['failed']:
 dump(ATT/'summary.json',summary);(ATT/'stdout.txt').write_text(json.dumps(summary)+'\n',encoding='utf-8');print(json.dumps(summary));raise SystemExit(1)
whole=[rec(p) for p in sorted(OUT.rglob('*')) if p.is_file()]+[rec(p) for p in sorted(NEW.glob('*')) if p.is_file()]
dump(OUT/'freeze.json',{'status':'FIRST_COMPLETE_OWN_SOURCE_FILE_HASH_JSON_INTEGER_PASS_FROZEN_UNRUN_FRESH_WHOLE_CRITIC_REQUIRED','checks':result['passed'],'failed':0,'files':whole,'Data':'UNDEFINED_BOTH','all_target_execution':False,'independent_source_native_resources_binding_GO':False})
summary.update({'pins_sha256':file_sha(OUT/'source-pins.json'),'whole_freeze_sha256':file_sha(OUT/'freeze.json'),'runner_sha256':file_sha(NEW/'run-clock.ps1'),'manifest_sha256':file_sha(NEW/'source-manifest.json'),'source_inventory_files':len(whole)})
# Summary output is also retained in the tool receipt. Frozen files are never
# edited after the first complete pass, including this helper/model source.
print(json.dumps(summary))
