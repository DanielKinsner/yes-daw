"""Independent R3 file/text/JSON/integer correspondence; no target execution."""
from pathlib import Path
import copy,hashlib,json,ntpath,re
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
PARENT=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
OLD=PARENT/'clock-oracle-r10-source-prototype'
R1=PARENT/'clock-oracle-r10-native-path-correction-r1'
R2=PARENT/'clock-oracle-r10-native-path-correction-r2'
NEW=PARENT/'clock-oracle-r10-native-path-correction-r3'
checks=[]
def ck(n,a,b):checks.append(dict(name=n,actual=a,expected=b,passed=a==b))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def js(p):return json.loads(p.read_text())
old,r1,r2,new=[js(p/'source-manifest.json') for p in [OLD,R1,R2,NEW]]
ck('original manifest SHA',sha(OLD/'source-manifest.json'),'91529AC11BB131BAE1B9083D95CE343877C5AF62AC50AF23628B63E1C1331B23')
ck('R1 manifest SHA',sha(R1/'source-manifest.json'),'FD2D09B6313EAC691B5003E9514FFAD47F5897E0B2A4AC5E341A68C925818F4B')
ck('R2 manifest SHA',sha(R2/'source-manifest.json'),'BFDE3D6EDB34BF15FA22E966703713656C6A0377538C5C5ED9DFF7B71592CB5E')
ck('R3 manifest SHA',sha(NEW/'source-manifest.json'),'824A2980CDACD71D084F5C9A62835299ACBB82E95FE693BF8FEF9E22F960D594')
expected=copy.deepcopy(old);expected['sources']=new['sources'];expected['R10_payload_repairs']['current_own5_path_UTF16_max']=179
ck('all manifest changes confined to source rows and corrected own-path maximum',new==expected,True)
expected_r3=copy.deepcopy(r2);expected_r3['sources']=new['sources'];expected_r3['R10_payload_repairs']['current_own5_path_UTF16_max']=179
ck('exact R2-to-R3 manifest delta',new==expected_r3,True)
for r in new['sources']:
 p=NEW/r['path'];b=p.read_bytes()
 ck('source pin:'+r['path'],[len(b),len(b.splitlines()),sha(p)],[r['bytes'],r['lines'],r['sha256']])
 ck('source LF/noBOM:'+r['path'],b'\r' not in b and not b.startswith(b'\xef\xbb\xbf'),True)
for name in ['run-clock.ps1','clock_stage.cpp','source-costs.h']:
 ck('same whole executable source R1/R2/R3:'+name,(NEW/name).read_bytes()==(R1/name).read_bytes()==(R2/name).read_bytes(),True)
for name in ['clock_stage.cpp','source-costs.h']:
 ck('same original CXX/header:'+name,(NEW/name).read_bytes()==(OLD/name).read_bytes(),True)
old_ps=(OLD/'run-clock.ps1').read_text();new_ps=(NEW/'run-clock.ps1').read_text()
old_literal=" $taskRuntime='C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell'"
ck('sole original-to-R3 runner delta',new_ps==old_ps.replace(old_literal,old_literal.replace('/','\\')),True)
ck('R2 README exact change',(R2/'README.md').read_text()==(R1/'README.md').read_text().replace('170 (one more than R9)','179 (ten more than R9)').replace('Narrow native-path correction, revision 1','Narrow native-path correction, revision 2'),True)
ck('R3 README exact heading change',(NEW/'README.md').read_text()==(R2/'README.md').read_text().replace('Narrow native-path correction, revision 2','Narrow native-path correction, revision 3'),True)
ck('own five source bytes',sum((NEW/n).stat().st_size for n in ['run-clock.ps1','clock_stage.cpp','source-costs.h','README.md','source-manifest.json']),655878)
ck('all manifest guards false',list(new['guards'].values()),[False]*len(new['guards']))
ck('guard/contract/profile/reference/env/cap equality',[new[k] for k in ['guards','contract','profile','references','control_files','compiler_final_outputs','environment','retention','source_work']],[old[k] for k in ['guards','contract','profile','references','control_files','compiler_final_outputs','environment','retention','source_work']])
for flag in ['taskSourceReviewAccepted','taskSourceWorkAccepted','taskExecutionAccepted','taskCompilerAccepted','taskProduceAccepted','taskVerifyAccepted','taskCompilerPeakDiskQualified','taskProductionContextQualityRendererReady']:
 ck('literal source guard false:'+flag,len(re.findall(r'^\$'+flag+r'=\$false$',new_ps,re.M)),1)
ck('source helper declarations',len(re.findall(r'^function [A-Za-z-]+',new_ps,re.M)),68)
for r in new['inputs']:
 p=Path(r['path']);p=p if p.is_absolute() else ROOT/p
 ck('fresh input:'+r['path'],[p.stat().st_size,sha(p)],[r['bytes'],r['sha256']])
ck('input profile',[len(new['inputs']),sum(r['bytes'] for r in new['inputs']),sum((r['bytes']+65535)//65536 for r in new['inputs'])],[1220,460232026,7849])
ck('manifest byte extent',(NEW/'source-manifest.json').stat().st_size,497616)
ck('fence chunks',sum((r['bytes']+65535)//65536 for r in new['inputs']+new['sources'])+((NEW/'source-manifest.json').stat().st_size+65535)//65536,7862)

def shape(v,depth=0):
 if isinstance(v,dict):
  x=[shape(a,depth+1) for a in v.values()]
  return [1+sum(a[0] for a in x),len(v)+sum(a[1] for a in x),max([depth+1]+[a[2] for a in x]),max([len(k) for k in v]+[a[3] for a in x]+[0]),sum(a[4] for a in x)]
 if isinstance(v,list):
  x=[shape(a,depth+1) for a in v]
  return [1+sum(a[0] for a in x),sum(a[1] for a in x),max([depth+1]+[a[2] for a in x]),max([a[3] for a in x]+[0]),sum(a[4] for a in x)]
 return [1,0,depth,len(v) if isinstance(v,str) else 0,int(isinstance(v,float))]
s=shape(new);ck('bootstrap values/keys/depth/maxstring/floats',s,[9922,6056,4,435,0])
ck('bootstrap visit count',s[0]+s[1],15978)
ck('bootstrap metadata exact',new['R10_payload_repairs']['bootstrap_observed'],dict(values=s[0],keys=s[1],max_string_units=s[3],container_depth=s[2],floating_values=s[4],nodes_and_keys=s[0]+s[1]))
repo=ntpath.normpath(str(ROOT));base=ntpath.normpath(str(NEW));run=ntpath.join(base,'runs','r10-one-reviewed-attempt')
trans=[ntpath.normpath(r['path'] if ntpath.isabs(r['path']) else ntpath.join(repo,r['path'])) for r in new['inputs']]
own=[ntpath.join(base,n) for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json']];trans+=own
art=[ntpath.normpath(ntpath.join(run,'data',r['path'])) for r in new['references']]
art+=[ntpath.join(run,'build',r['name']) for r in new['compiler_final_outputs']]
art+=[ntpath.normpath(ntpath.join(run,r['path'])) for r in new['control_files']]
art+=[ntpath.join(run,'stage',n) for n in ['stdout.log','stderr.log','receipt.json','consumed.json']]
paths=trans+art+[ntpath.join(run,'stage',n) for n in ['bundle.json','failure-inventory.json']]
pool=set();visits=0;depth_max=0
for path in paths:
 at=path;depth=0
 while at:
  pool.add(at);visits+=1;depth+=1;parent=ntpath.dirname(at)
  if parent==at:break
  at=parent
 depth_max=max(depth_max,depth)
strings=['ClockOracle','CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED','A'*64,'OPAQUE',new['predecessor_bundle']['path'],'A'*64]
strings+=[s for path in trans+art for s in [path,'A'*64,'A'*64]];strings+=[r['path'] for r in new['references']]
escaped=sum(len(json.dumps(s,ensure_ascii=True,separators=(',',':')))-2 for s in strings)
bundle=46364+2*5251+escaped+2*3938+20*1302+5*8+2*1294+4+5251+3957+1290+1
geometry=dict(base=base,run_root=run,selected_for_equations_only_not_GO=True,identities=len(paths),pooled_strings=len(pool),pooled_UTF16_units=sum(map(len,pool)),chain_visits=visits,max_depth=depth_max,own5_path_maximum=max(map(len,own)),current_identity_path_maximum=max(map(len,trans)),current_identity_tilde_count=sum('~' in p for p in trans),bundle_encoded_string_units=escaped,bundle_byte_upper=bundle,upper_request_reduction=7*(1048576-bundle))
ck('fresh R3 lexical geometry',[geometry[k] for k in ['identities','pooled_strings','pooled_UTF16_units','chain_visits','max_depth','own5_path_maximum','current_identity_path_maximum','current_identity_tilde_count','bundle_encoded_string_units','bundle_byte_upper','upper_request_reduction']],[1294,1458,187423,14548,18,179,270,0,349846,453759,4163719])
for key,expected_value in [('current_own5_path_UTF16_max',179),('known_current_path_UTF16_max',270),('known_current_identity_paths_with_tilde',0),('path_identity_maximum',1294),('path_alias_maximum',2662),('path_string_maximum',84110),('parent_array_capacity_per_identity',65)]:
 ck('all current R10 repair metadata:'+key,new['R10_payload_repairs'][key],expected_value)
ck('own5 maximum README accurate','current own5 maximum is179 (ten more than R9)' in (NEW/'README.md').read_text(),True)
report=dict(check_count=len(checks),checks=checks,failures=[c for c in checks if not c['passed']],geometry=geometry,scope='Independent source/file/text/JSON/integer proof only, no target PS parser/AST/helper/import/compiler/program/stage. Conditional scalar5543964320; no selected price, binding or GO. Lifetime/global32MiBfit remains held.',R2_metadata_finding_preserved=dict(manifest=sha(R2/'source-manifest.json'),field='R10_payload_repairs.current_own5_path_UTF16_max',actual=r2['R10_payload_repairs']['current_own5_path_UTF16_max'],required=179,closed_in_R3=new['R10_payload_repairs']['current_own5_path_UTF16_max']==179))
(OUT/'mechanical-evidence.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(checks=len(checks),failures=report['failures'],geometry=geometry),indent=2))
raise SystemExit(1 if report['failures'] else 0)
