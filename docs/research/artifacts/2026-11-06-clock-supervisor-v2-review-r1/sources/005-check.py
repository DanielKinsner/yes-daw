"""Own exact-file/text and finite integer/state delta checks only.
No peer sealer, target/parser/compiler/helper/native code is invoked.
"""
from pathlib import Path
import hashlib,json,re
P=Path(__file__).resolve().parent;R=P.parents[1]
if (P/'review.json').exists():raise ValueError('Preserve first complete own pass')
checks=[];pins=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(path):
 b=path.read_bytes();pins.append(dict(path=path.relative_to(R).as_posix(),bytes=len(b),sha256=sha(b)));return b
def ck(name,ok):
 checks.append(dict(name=name,passed=bool(ok)))
 if not ok:raise ValueError(name)
base=R/'build-ci/20261106-clock-supervisor-v2-extension-r1'
subject=read(base/'source-pins.json');ck('exact complete subject seal',sha(subject)=='7F16DCFC23E61FFDA4B2DFEA6814CCA2398D74F29A3656755F9183FFAB79F7CD')
decoded=json.loads(subject)
texts={}
for row in decoded['inputs']:
 b=read(R/row['path']);ck('subject/base '+row['path'],len(b)==row['bytes'] and sha(b)==row['sha256']);texts[Path(row['path']).name]=b.decode()
adr=texts['0091-clock-supervisor-job-termination-diagnostic.md'];v2=texts['protocol-v2.md'];v1=texts['clock-supervisor-protocol-v1.md']
ck('draft remains Proposed','**Status:** Proposed' in adr)
ck('accepted base remains exact',sha((R/'docs/adr/0090-clock-research-owned-process-supervisor.md').read_bytes())=='68DC0922BF56F0B9697CF178B69639D5346684EAC0829BEE9DB55F03CB4F6565')
ck('ADR0091 not yet applied',not list((R/'docs/adr').glob('0091*')))
for text in ['Use option1 only after','No compiler, helper, fixture or Clock execution follows.','first-primary/secondary ordering','Failed termination is an action failure','ADR-0090 and protocol v1 stay byte-exact','No source price transfers']:
 ck('ADR scope '+text,text in adr)
for text in ['does not coerce or accept v1','JobTerminationFailure','allowed site10','All0..47 reasons keep','immediately captured documented native error','reap-API-failure bit1','precedence over exit-unobserved bit2','first-primary/secondary/native-secondary ordering','Do not overwrite an earlier failure or invent an exit/empty-job observation','actual later job-query failure is a distinct32 event','cleanup maximum15','no price, ABI, fit or execution credit','normative only after ADR-0091']:
 ck('delta rule '+text,text in v2)
ck('exact five overrides',re.findall(r'^([1-5])\. ',v2,re.M)==['1','2','3','4','5'])
section=v1.split('## 6. Status, sites and reasons',1)[1].split('## 7.',1)[0]
old={int(n):(name,tuple(map(int,sites.split(',')))) for n,name,sites in re.findall(r'^\|\s*(\d+)\s*\|\s*(\w+)\s*\|\s*([0-9,]+)\s*\|',section,re.M)}
ck('complete old reasons',set(old)==set(range(48)))
ck('old32 exact query pair',old[32]==('JobQueryFailure',(10,)))
old_pairs={(site,n) for n,(_,sites) in old.items() for site in sites}
new_pairs=old_pairs|{(10,48)}
pair_cases=0
for site in range(14):
 for reason in range(49):
  pair_cases+=1
  ckname=None
  if ((site,reason) in new_pairs)!=((site,reason) in old_pairs or (site==10 and reason==48)):raise ValueError('PAIR_EXTENSION')
ck('only one new admitted pair',new_pairs-old_pairs=={(10,48)} and old_pairs<=new_pairs)
magic_cases=0
for expected in [b'YCSREQ02',b'YCSRES02']:
 admitted=[]
 for magic in [b'YCSREQ01',b'YCSREQ02',b'YCSRES01',b'YCSRES02']:
  for version in [1,2]:
   magic_cases+=1
   if magic==expected and version==2:admitted.append((magic,version))
 ck('strict v2 admission '+expected.decode(),admitted==[(expected,2)])
ck('same magic byte extents',all(len(m)==8 for m in [b'YCSREQ01',b'YCSREQ02',b'YCSRES01',b'YCSRES02']))
labels=[b'YESDAW-CLOCK-SUPERVISOR-RUN-V1',b'YESDAW-CLOCK-SUPERVISOR-RUN-V2']
ck('new equal-length domain label',labels[0]!=labels[1] and len(labels[0])==len(labels[1]))
ck('new pair fits old u32',0<=48<2**32 and 0<=10<2**32)
ck('compiler request unchanged',140+40*(4+2*512)==41260)
ck('native request unchanged',140+18*(4+2*512)==18644)
ck('terminal unchanged',8+16*4+3*8+3*32+2*7*8+4==308)
ck('transport unchanged',41260+2*18644+3*308==79472)
mask_cases=0;valid_masks=0;max_prefix=0;max_late=0
for mask in range(1<<15):
 mask_cases+=1
 normalized=(mask|(1<<1))&~(1<<2)
 if not(normalized&(1<<1)) or normalized&(1<<2) or ((normalized^mask)&~((1<<1)|(1<<2))):raise ValueError('REAP_PRECEDENCE')
 if mask&(1<<1) and mask&(1<<2):continue
 valid_masks+=1;n=mask.bit_count();max_prefix=max(max_prefix,n)
 max_late=max(max_late,n+1+int(not(mask&(1<<14))))
ck('cleanup prefix max14',max_prefix==14)
ck('late cleanup max15',max_late==15)
ck('no new mask category',all((mask|(1<<1))<(1<<15) for mask in [0,(1<<15)-1]))
# Small independent ordering oracle for the normative new event. Not helper code.
def append_event(primary,secondary,native_secondary,count,event):
 if primary is None:return event,secondary,native_secondary,count
 return primary,secondary if secondary is not None else event,native_secondary if native_secondary is not None or not event['known'] else event,count+1
ordering_cases=0;proof_cases=0
for native in [0,5,2**32-1]:
 event=dict(site=10,reason=48,known=True,error=native)
 for primary in [None,dict(site=7,reason=25,known=False,error=0),dict(site=6,reason=23,known=True,error=5)]:
  for secondary in [None,dict(site=5,reason=17,known=False,error=0)]:
   for native_secondary in [None,dict(site=4,reason=15,known=True,error=995)]:
    ordering_cases+=1
    p,s,n,c=append_event(primary,secondary,native_secondary,int(secondary is not None),event)
    if primary is None:
     if p!=event:raise ValueError('PRIMARY_RECORD')
    else:
     if p!=primary or s!=(secondary if secondary is not None else event) or n!=(native_secondary if native_secondary is not None else event) or c!=int(secondary is not None)+1:raise ValueError('SECONDARY_ORDER')
     query=dict(site=10,reason=32,known=True,error=87)
     qp,qs,qn,qc=append_event(p,s,n,c,query)
     if qp!=p or qs!=s or qn!=n or qc!=c+1:raise ValueError('QUERY_DISTINCT_EVENT')
 for target_exit_known in [False,True]:
  for job_empty_known in [False,True]:
   proof_cases+=1
   before=(target_exit_known,job_empty_known)
   after=before # Diagnostic records add neither observation.
   if after!=before:raise ValueError('FABRICATED_PROOF')
   success_allowed=(event is None) # v1 status0/1 require no diagnostic/mask.
   if success_allowed:raise ValueError('FALSE_SUCCESS')
ck('first-record models complete',ordering_cases==36)
ck('proof independence models complete',proof_cases==12)
article=read(P/'review.md').decode()
for text in ['ACCEPT_PROPOSED_ADR0091_AND_EXACT_V2_DELTA_FOR_ROOT_APPLICATION','No functional finding','Status0/1 remain impossible','full prior predicates','No C++ enum layout','first primary is immutable','after record freeze','no compiler/helper/fixture/Clock GO','not implementation/runtime tests']:
 ck('review scope '+text,text.lower() in article.lower())
result=dict(schema='INDEPENDENT_SUPERVISOR_V2_NARROW_EXTENSION_REVIEW_R1',verdict='ACCEPT_PROPOSED_ADR0091_AND_EXACT_V2_DELTA_FOR_ROOT_APPLICATION',findings=[],subject_pins_sha256=sha(subject),base_ADR0090_and_v1_unchanged=True,allowed_delta=dict(request_magic='YCSREQ02',terminal_magic='YCSRES02',version=2,run_domain='YESDAW-CLOCK-SUPERVISOR-RUN-V2',additional_pair=[10,48],reason='JobTerminationFailure'),model_cases=dict(reason_pairs=pair_cases,strict_magic_version=magic_cases,mask_states=mask_cases,valid_masks=valid_masks,first_record_ordering=ordering_cases,proof_independence=proof_cases),extents=dict(request=140,compiler=41260,native=18644,terminal=308,transport=79472,cleanup_prefix=max_prefix,cleanup_late=max_late),scope='Exact delta contract consistency only, not whole unchanged architecture review or dependent implementation/ABI/native/resource/census acceptance',no_prices_fit_Data_or_execution_GO=True,checks=checks,input_pins=pins,discovery_note='Initial guessed adr-0091.md read emitted a nonterminating missing-path error (combined command exit0); actual pinned 0091-clock-supervisor-job-termination-diagnostic.md then read completely. No subject edits or peer checker execution.')
(P/'review.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='OWN_NARROW_V2_SOURCE_MODEL_CHECK_EXIT0',checks=len(checks),cases=result['model_cases'],verdict=result['verdict'],review_json_sha256=sha((P/'review.json').read_bytes()))))
