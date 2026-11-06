from pathlib import Path
import hashlib,json,os,re,ntpath,urllib.request,difflib
R=Path(__file__).resolve().parents[2];O=Path(__file__).parent;B=R/'build-ci/musical-audio-persistent-planner-certification/source-revisions';P=B/'clock-oracle-r10-source-prototype';Q=B/'clock-oracle-r9-source-prototype'
m=json.loads((P/'source-manifest.json').read_text(encoding='utf-8'));old=json.loads((Q/'source-manifest.json').read_text(encoding='utf-8'));checks=[]
def h(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def check(n,a,b):checks.append({'name':n,'actual':a,'expected':b,'pass':a==b})
expected={'run-clock.ps1':'BF117EAEA5AABCD40FFE30C785B75C875BAF92A9DA995B181B37DBB5FB75FEDB','source-manifest.json':'91529AC11BB131BAE1B9083D95CE343877C5AF62AC50AF23628B63E1C1331B23','README.md':'A95A77743ABE02ABE03C4BA8C72C08889A1A4DD3DA5D103FF7A83DF10C235F6E','clock_stage.cpp':'F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206','source-costs.h':'0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27'}
for n,v in expected.items():check('subject '+n,h(P/n),v)
check('exact five files',sorted(x.name for x in P.iterdir()),sorted(expected))
for z in m['sources']:check('source identity '+z['path'],[h(P/z['path']),(P/z['path']).stat().st_size],[z['sha256'],z['bytes']])
for n in ['clock_stage.cpp','source-costs.h']:check('byte exact R9 '+n,(P/n).read_bytes()==(Q/n).read_bytes(),True)
inputs={z['path']:z for z in m['inputs']};check('input path uniqueness',len(inputs),len(m['inputs']))
for z in m['inputs']:
 p=Path(z['path']);p=p if p.is_absolute() else R/p;check('input '+z['path'],[h(p),p.stat().st_size],[z['sha256'],z['bytes']])
for z in old['inputs']:check('inherited identity '+z['path'],inputs.get(z['path']),z)
for name in ['compiler','environment','references','control_files','compiler_final_outputs','stage_files','module_controls','normal_gate_controls','fraction_residues']:
 check('inherited contract '+name,m[name],old[name])
actuals=[]
for z in m['closed_directory_inputs']:
 p=Path(z['path']);p=p if p.is_absolute() else R/p;rows=[];files=[];links=[]
 def walk(q):
  for e in os.scandir(q):
   rel=Path(e.path).relative_to(p).as_posix();rows.append(rel)
   if e.is_symlink() or e.stat(follow_symlinks=False).st_file_attributes&0x400:links.append(rel)
   elif e.is_dir(follow_symlinks=False):
    if z['recursive']:walk(Path(e.path))
   else:files.append(rel)
 walk(p);check('namespace rows '+z['path'],len(rows),z['entry_count']);check('namespace files '+z['path'],sorted(files),sorted(z['files']));check('no reparse '+z['path'],links,[]);actuals.append([len(rows),len(files)])
profile={'input_count':len(inputs),'input_bytes':sum(z['bytes'] for z in m['inputs']),'input_hash_chunks':sum((z['bytes']+65535)//65536 for z in m['inputs']),'include_input_count':len(inputs)+1,'directory_namespaces_count':len(actuals),'directory_namespace_entries':sum(v[0] for v in actuals),'directory_namespace_files':sum(v[1] for v in actuals),'complete_current_inputs':len(inputs)+5,'maximum_complete_fence_identities':len(inputs)+5+68+1}
for k,v in profile.items():check('profile '+k,v,m['profile'][k])
check('own5',sum(q.stat().st_size for q in P.iterdir()),655037);check('retained source reservation',460232026+1048576+152627224+67108864+67108864+33652457,781778011);check('retained headroom',880803840-781778011,99025829)
check('alias derivation',2*1220+3*(1294-1220),2662);check('parent string derivation',65*1294,84110)
# Bootstrap graph structural arithmetic only, not target JSON grammar invocation.
graph={'values':0,'keys':0,'max_string_units':0,'container_depth':0,'floating_values':0}
def graphwalk(v,d=0):
 graph['values']+=1
 if isinstance(v,str):graph['max_string_units']=max(graph['max_string_units'],len(v.encode('utf-16-le'))//2)
 elif type(v)==float:graph['floating_values']+=1
 elif isinstance(v,(dict,list)):
  graph['container_depth']=max(graph['container_depth'],d)
  if isinstance(v,dict):
   graph['keys']+=len(v)
   for k,q in v.items():graph['max_string_units']=max(graph['max_string_units'],len(k.encode('utf-16-le'))//2);graphwalk(q,d+1)
  else:
   for q in v:graphwalk(q,d+1)
graphwalk(m,1);graph['nodes_and_keys']=graph['values']+graph['keys']
check('bootstrap graph',graph,m['R10_payload_repairs']['bootstrap_observed'])
a=(Q/'run-clock.ps1').read_text(encoding='utf-8');b=(P/'run-clock.ps1').read_text(encoding='utf-8')
def fns(t):
 q=list(re.finditer(r'^function ([A-Za-z-]+)',t,re.M));return {z[1]:t[z.start():(q[i+1].start() if i+1<len(q) else t.index('\ntry{',z.start()))].strip() for i,z in enumerate(q)}
fa,fb=fns(a),fns(b);same=sorted(k for k in fa if k in fb and fa[k]==fb[k]);changed=sorted(k for k in fa if k in fb and fa[k]!=fb[k]);added=sorted(set(fb)-set(fa));removed=sorted(set(fa)-set(fb));v=m['R10_payload_repairs']
check('same helpers',same,sorted(v['helper_bodies_byte_exact_R9']));check('changed helpers',changed,sorted(v['changed_helper_bodies']));check('added helpers',added,sorted(v['added_helpers']));check('removed helper',removed,[v['removed_helper']]);check('helper count',len(fb),68)
check('false manifest guards',any(m['guards'].values()),False);check('unbound price definitions',m['cost_catalogue']['definition_files'],[]);check('false PS flags',len(re.findall(r'^\$task(?:SourceReviewAccepted|SourceWorkAccepted|ExecutionAccepted|CompilerAccepted|ProduceAccepted|VerifyAccepted|CompilerPeakDiskQualified|ProductionContextQualityRendererReady)=\$false$',b,re.M)),8)
check('SDK sites',b.count('& $script:taskConvertFromJson')+b.count('& $taskConvertFromJson'),4)
check('grammar source byte cap',"if($Bytes.Length -gt 4194304)" in fb['Assert-TaskRawJsonBytes'],True)
check('same string Raw helper',fb['Assert-TaskRawJson'],fa['Assert-TaskRawJson'])
check('byte literal controls',sum(len(q.encode('utf-8')) for q in ['18446744073709551615','-9223372036854775808','184467440737095516150']),61)
for n in ['Read-TaskJson','Assert-TaskRawJsonBytes','Resolve-TaskInput','Assert-NoLink','Get-TaskEntries','Assert-TaskDependencies']:
 (O/(n+'.txt')).write_text(fb[n]+'\n',encoding='utf-8',newline='\n')
# Source diff and pinned primary boundary files are evidence only.
(O/'run-clock-r9-r10.diff').write_text(''.join(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='R9/run-clock.ps1',tofile='R10/run-clock.ps1')),encoding='utf-8',newline='\n')
primary=[];sourcedir=O/'primary-sources';sourcedir.mkdir(exist_ok=True)
for z in v['external_primary_source_associations']:
 data=urllib.request.urlopen(z['url'],timeout=30).read();dest=sourcedir/z['url'].rsplit('/',1)[-1];dest.write_bytes(data);check('primary source '+dest.name,[len(data),hashlib.sha256(data).hexdigest().upper()],[z['bytes'],z['sha256']]);primary.append(dict(z,local_path=dest.relative_to(R).as_posix()))
out={'scope':'ordinary artifact/source/hash/text/arithmetic only; no target PowerShell parser/AST/import/helper/compiler/test/program/stage','checks':checks,'check_count':len(checks),'failures':[q for q in checks if not q['pass']],'profile':profile,'graph':graph,'helper_identity':{'same':same,'changed':changed,'added':added,'removed':removed},'primary_sources':primary}
(O/'audit-evidence.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'check_count':len(checks),'failures':out['failures'],'profile':profile,'graph':graph},indent=2))
