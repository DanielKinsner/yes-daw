"""Independent ordinary-file/data arithmetic; no author helper or target execution."""
from collections import Counter
from hashlib import sha256
from pathlib import Path
import json
import re
import struct

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SUB=ROOT/'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1'
BASE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1'
def read(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def a8(n):return (n+7)//8*8
def arr(n,w):return a8(24+n*w)
checks=[]
pins=[]
def ck(name,result,detail=None):checks.append({'name':name,'pass':bool(result),'detail':detail})
mp=SUB/'source-pins.json'
ck('external_packet_pin',digest(mp)=='4D8B292BA5096826E21E9F16806CFDF36C26383DFCD35FFF67927FFA692AB61D')
m=read(mp)
for row in m['authored_files']+m['input_pins']:
 p=ROOT/row['path'];actual={'path':row['path'],'bytes':p.stat().st_size,'sha256':digest(p)}
 ck('raw_pin:'+row['path'],actual==row)
 pins.append(actual)
phase=read(SUB/'phases.json')
fixed=read(SUB/'fixed-inputs.json')
graphs=read(SUB/'fixed-graphs.json')
manifest=read(BASE/'source-manifest.json')
source=(BASE/'run-clock.ps1').read_text()

# Independent finite JSON lexical spans using an ordinary DATA-token regex.
# Validity is separately checked by Python's DATA JSON reader, never the target.
token=re.compile(r'"(?:\\.|[^"\\])*"|-?\d+|true|false|null|[\[\]{},:]')
lex=[]
for row in fixed['inputs']:
 p=ROOT/row['path'];raw=p.read_bytes();value=json.loads(raw)
 text=''.join(x.group() for x in token.finditer(raw.decode('ascii')))
 ck('compacted_data_value:'+row['name'],json.loads(text)==value)
 spans=list(token.finditer(text))
 shifts=[x.start()+int(x.group().startswith('"')) for x in spans if x.group().startswith('"') or x.group()[0] in '-0123456789']
 points=[0]+shifts+[len(text)]
 gap=max(y-x for x,y in zip(points,points[1:]))
 cap=1<<((max(1024,16*(gap+6))-1).bit_length())
 caps=[1<<k for k in range(10,cap.bit_length())]
 actual={'raw_bytes':len(raw),'raw_array_request':arr(len(raw),1),'compacted_source_string_request':a8(22+2*len(text)),'compacted_units':len(text),'maximum_shift_gap':gap,'capacity_upper':cap,'all_reader_array_requests_including_headers':sum(arr(c,2) for c in caps)}
 for k,v in actual.items():ck('fixed_input:'+row['name']+':'+k,v==(row[k] if k in row else row['lexical'][k]),v)
 lex.append(actual)

prsrc=(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs').read_text()
pr=[int(n) for n in re.findall(r'\b\d+\b',prsrc.split('internal static ReadOnlySpan<int> Primes =>',1)[1].split('];',1)[0])]
def prime(n):return next(x for x in pr if x>=n)
def capacities(n,kind):
 out=[];c=0
 while c<n:
  c=(4 if c==0 else c*2) if kind=='list' else prime(c*2)
  out.append(c)
 return out
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def hc(n):
 load=f32(.72);q=f32(f32(n)/load);c=3 if q<=3 else prime(int(q));out=[c]
 for inserted in range(n):
  if inserted>=int(f32(load*c)):c=prime(2*c);out.append(c)
 return out
def fixed_graph(v):
 t=Counter();c=Counter()
 def visit(x):
  if isinstance(x,dict):
   n=len(x);c['maps']+=1;c['fields']+=n
   t['jobject']+=112;t['jproperty_and_list']+=128*n
   t['conversion_enumerators']+=96
   t['jcontainer_lists']+=72+sum(arr(k,8) for k in capacities(n,'list'))
   if n:t['jobject_dictionaries']+=80+sum(arr(k,4)+arr(k,24) for k in capacities(n,'dict'))
   t['parsed_ordered_maps']+=144+arr(hc(n)[0],24)
   if n:t['parsed_ordered_maps']+=112+sum(arr(k,24) for k in hc(n))+arr(n,8)+32*n
   for k,y in x.items():t['strings']+=a8(22+len(k.encode('utf-16-le')));visit(y)
  elif isinstance(x,list):
   c['arrays']+=1;t['jarray']+=96;t['conversion_enumerators']+=40
   t['jcontainer_lists']+=40+sum(arr(k,8) for k in capacities(len(x),'list'))
   t['parsed_arrays']+=arr(len(x),8)
   for y in x:visit(y)
  else:
   c['scalars']+=1;t['jvalue']+=64
   if isinstance(x,str):t['strings']+=a8(22+len(x.encode('utf-16-le')))
   elif x is not None:t['scalar_box_upper']+=48
 visit(v)
 return dict(t),dict(c)
graph_rows=[]
for row in graphs['inputs']:
 t,c=fixed_graph(read(ROOT/row['path']))
 ck('fixed_graph_families:'+row['name'],t==row['graph']['families'],t)
 ck('fixed_graph_subtotal:'+row['name'],sum(t.values())==row['graph']['subtotal'],sum(t.values()))
 for k,n in c.items():ck('fixed_graph_count:'+row['name']+':'+k,n==row['graph']['counts'][k])
 graph_rows.append({'name':row['name'],'families':t,'subtotal':sum(t.values())})
ck('htable_insertion8',hc(8)==[11,23])
ck('htable_fixed_maps_under_collision_threshold',max(len(x) for x in [manifest])<101 and graphs['hashtable_examples'][-1]['entries']==55)

nominal=[]
for p in phase['phases']:
 f=p['fences'];s=p['stage_calls'];r=p['references'];b=p['builds'];w=p['root_walks'];c=p['children']
 E=6*s+(3 if s else 0)+1123*f+60*r+4*b+78*w+1
 D=s+720*f+61*r+b+79*w+1
 H=1225*f+p['nonfence_hashes']
 families=[168*H+264*1224*f,208*H,56*E,168*D,40*7200*c,72*(6*s+(3 if s else 0)),72*E]
 ck('nominal_walker_rows:'+p['id'],E==p['walker_appended_rows_upper'])
 ck('nominal_directory_iterations:'+p['id'],D==p['walker_directory_iterations_upper'])
 ck('nominal_hashes:'+p['id'],H==p['hash_calls_upper'])
 ck('nominal_selected_family_sum:'+p['id'],sum(families)==p['partial_selected_family_upper'])
 q=sum(x['subtotal'] for x in graph_rows)+sum(x['raw_array_request']+x['compacted_source_string_request']+x['all_reader_array_requests_including_headers'] for x in lex)+2*2072
 if c:q+=arr(4194304,1)+a8(22+2*4194304)+1046744+2072
 if p['native_result_parses']>=1:q+=1088+2152+2240+4*2072
 if p['native_result_parses']>=2:q+=2688+5352+5660+12*2072
 ck('nominal_typed_join:'+p['id'],q+sum(families)==p['typed_partial_join_upper'])
 nominal.append({'id':p['id'],'stage_calls':s,'E':E,'D':D,'H':H,'selected':sum(families),'typed_partial':q+sum(families)})
ck('namespace_row_recipe',sum(x['entry_count']+1 for x in manifest['closed_directory_inputs'])==1123)
ck('namespace_directory_recipe',len(manifest['closed_directory_inputs'])+sum(x['entry_count']+1 for x in manifest['closed_directory_inputs'] if x['recursive'])==720)
markers=('::new(', '@(', '@{', '.ToArray(', '.Substring(', '.Replace(', 'GetRelativePath(', 'Join-Path ', '.GetHashAndReset(', '::ToHexString(')
sites=[{'line':i,'source':line,'matched':[x for x in markers if x in line]} for i,line in enumerate(source.splitlines(),1) if any(x in line for x in markers)]
ck('owner_text_inventory187',len(sites)==187 and sites==read(SUB/'source-owner-sites.json')['sites'])
ck('thirteen_named_residual_families',[x['id'] for x in phase['terms']]==['F','R','J','H','W','O','K','B','P','A','N','V','E'])
ck('writer_doc_loose_upper_arithmetic',31122780+1881894+1406195==34410869)
ck('hash_carrier_proposal_only_arithmetic',9936*(168-32)+9792*(264-40)==3544704 and 9800*152==1489600)
ck('contract_pin',digest(ROOT/'build-ci/20261105-clock-oracle-separate-research-contract/contract.md')=='344408DAEC2BD355D44B0B6DA266BF00119B09DF5028B70524699E8E2CD36010')
ck('stdout_guard_only',"$taskLength -gt 65536" in source[source.index('function Read-TaskChildResult'):source.index('function Get-TaskBuildArtifacts')] and "raw_stderr_bytes" not in source[source.index('function Read-TaskChildResult'):source.index('function Get-TaskBuildArtifacts')])
ck('empty_LF_slices_skipped',"if($taskI -gt $taskStart)" in source[source.index('function Read-TaskChildResult'):source.index('function Get-TaskBuildArtifacts')])
alternatives=[{'id':'P0','cap_domain_stage_calls':0},{'id':'P1','cap_domain_stage_calls':7},{'id':'P2','cap_domain_stage_calls':257+7},{'id':'P3','cap_domain_stage_calls':2*257+9},{'id':'P4','cap_domain_stage_calls':3*257+11},{'id':'P5','cap_domain_stage_calls':3*257+13}]
for a,p in zip(alternatives,phase['phases']):
 a['reported_stage_calls']=p['stage_calls'];a['additional_stage_calls']=a['cap_domain_stage_calls']-p['stage_calls']
 a['additional_selected_row_slots']=6*a['additional_stage_calls']
 a['additional_selected_requests_before_other_residuals']=a['additional_stage_calls']*(168+6*(56+72+72))
out={'schema':'R11_PHASE_PACKET_INDEPENDENT_DATA_SOURCE_ARITHMETIC_REVIEW_R1','check_count':len(checks),'checks':checks,'failures':[x for x in checks if not x['pass']],'verified_input_pins':pins,'fixed_lexical_recompute':lex,'fixed_graph_recompute':graph_rows,'nominal_phase_recompute':nominal,'cap_domain_alternatives':alternatives,'meaning':'Arithmetic reproduces nominal conditional worksheet, not source proof of its quiet-output premise or a whole numeric capacity upper. No author helper/target invoked.'}
(HERE/'mechanical-evidence.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'checks':len(checks),'failures':out['failures'],'nominal':nominal,'cap_domain_alternatives':alternatives},indent=2))
raise SystemExit(1 if out['failures'] else 0)
