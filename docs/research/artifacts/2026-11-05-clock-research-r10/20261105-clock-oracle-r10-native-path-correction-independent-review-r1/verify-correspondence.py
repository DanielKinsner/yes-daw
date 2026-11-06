"""Read-only successor/source/profile correspondence; no target PS parsing/run."""
from pathlib import Path
import hashlib
import json
import ntpath
import re

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
OLD = ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r10-source-prototype'
NEW = ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r10-native-path-correction-r1'
RECEIPT = ROOT/'build-ci/20261105-clock-oracle-r10-native-path-correction-r1/source-correspondence.json'
checks=[]
def ck(name,actual,expected):checks.append(dict(name=name,actual=actual,expected=expected,passed=actual==expected))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest().upper()
old=json.loads((OLD/'source-manifest.json').read_text())
new=json.loads((NEW/'source-manifest.json').read_text())
receipt=json.loads(RECEIPT.read_text())
ck('original exact manifest',digest(OLD/'source-manifest.json'),'91529AC11BB131BAE1B9083D95CE343877C5AF62AC50AF23628B63E1C1331B23')
ck('successor exact manifest',digest(NEW/'source-manifest.json'),'FD2D09B6313EAC691B5003E9514FFAD47F5897E0B2A4AC5E341A68C925818F4B')
ck('successor exact runner',digest(NEW/'run-clock.ps1'),'4124D01AFEEC4E88C038C0A948800182561D8608DE175AF352CF574F164E9314')
for r in new['sources']:
    path=NEW/r['path'];data=path.read_bytes()
    ck('source hash:'+r['path'],digest(path),r['sha256'])
    ck('source size:'+r['path'],len(data),r['bytes'])
    ck('source lines:'+r['path'],len(data.splitlines()),r['lines'])
    ck('source LF/noBOM:'+r['path'],b'\r' not in data and not data.startswith(b'\xef\xbb\xbf'),True)
for filename in ['clock_stage.cpp','source-costs.h']:
    ck('unchanged executable source:'+filename,(NEW/filename).read_bytes()==(OLD/filename).read_bytes(),True)
old_runner=(OLD/'run-clock.ps1').read_text()
new_runner=(NEW/'run-clock.ps1').read_text()
old_line=" $taskRuntime='C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell'"
new_line=old_line.replace('/','\\')
ck('one old runtime literal',old_runner.count(old_line),1)
ck('one new native runtime literal',new_runner.count(new_line),1)
ck('only literal byte change',new_runner==old_runner.replace(old_line,new_line),True)
ck('same runner byte/line shape',[(NEW/'run-clock.ps1').stat().st_size,len(new_runner.splitlines())],[104099,974])
ck('same literal UTF16 extent',len(new_line.encode('utf-16-le')),len(old_line.encode('utf-16-le')))
expected_manifest=dict(old);expected_manifest['sources']=new['sources']
ck('manifest source rows only',new==expected_manifest,True)
ck('same manifest raw extent',(NEW/'source-manifest.json').stat().st_size,(OLD/'source-manifest.json').stat().st_size)
ck('same closed source/namespace inputs',new['inputs']==old['inputs'],True)
ck('same contracts/caps/guards',new['guards'],old['guards'])
ck('all manifest guards false',list(new['guards'].values()),[False]*len(new['guards']))
for flag in ['taskSourceReviewAccepted','taskSourceWorkAccepted','taskExecutionAccepted','taskCompilerAccepted','taskProduceAccepted','taskVerifyAccepted','taskCompilerPeakDiskQualified','taskProductionContextQualityRendererReady']:
    ck('literal false guard:'+flag,len(re.findall(r'^\$'+flag+r'=\$false$',new_runner,re.M)),1)
old_doc=(OLD/'README.md').read_text();new_doc=(NEW/'README.md').read_text()
ck('README preserves original prefix',new_doc.startswith(old_doc),True)
append=new_doc[len(old_doc):]
ck('README exact narrow section',append.startswith('\n## Narrow native-path correction, revision 1\n'),True)
for phrase in ['same byte and UTF16 length','no new runtime normalization call or allocation','guards remain false','source, scalar/profile/lifetime correspondence and separate execution GO']:
    ck('README correction disclosure:'+phrase,phrase in append,True)

# Fresh current identities/chunks; the whole original 1220-file set is revisited.
for r in new['inputs']:
    path=Path(r['path']);path=path if path.is_absolute() else ROOT/path
    ck('admitted input:'+r['path'],[path.stat().st_size,digest(path)],[r['bytes'],r['sha256']])
input_bytes=sum(r['bytes'] for r in new['inputs'])
input_chunks=sum((r['bytes']+65535)//65536 for r in new['inputs'])
own_bytes=sum((NEW/n).stat().st_size for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json'])
own_chunks=sum(((NEW/n).stat().st_size+65535)//65536 for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json'])
ck('input profile',[len(new['inputs']),input_bytes,input_chunks],[1220,460232026,7849])
ck('own five chunks',own_chunks,13)
ck('own five below fixed cap',own_bytes<=1048576,True)
ck('same bootstrap byte extent',(NEW/'source-manifest.json').stat().st_size,497616)

def json_shape(value,depth=0):
    if isinstance(value,dict):
        results=[json_shape(x,depth+1) for x in value.values()]
        return (1+len(value)+sum(r[0] for r in results),len(value)+sum(r[1] for r in results),max([depth+1]+[r[2] for r in results]),max([len(k) for k in value]+[r[3] for r in results]+[0]))
    if isinstance(value,list):
        results=[json_shape(x,depth+1) for x in value]
        return (1+sum(r[0] for r in results),sum(r[1] for r in results),max([depth+1]+[r[2] for r in results]),max([r[3] for r in results]+[0]))
    return (1,0,depth,len(value) if isinstance(value,str) else 0)
shape_old=json_shape(old);shape_new=json_shape(new)
ck('same bootstrap JSON shape/string maximum',shape_new,shape_old)

# Lexical candidate for the actual NEW base, not selected argv or native output.
repo=ntpath.normpath(str(ROOT));base=ntpath.normpath(str(NEW));run=ntpath.join(base,'runs','r10-one-reviewed-attempt')
trans=[ntpath.normpath(r['path'] if ntpath.isabs(r['path']) else ntpath.join(repo,r['path'])) for r in new['inputs']]
own_paths=[ntpath.join(base,n) for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json']]
trans+=own_paths
art=[ntpath.normpath(ntpath.join(run,'data',r['path'])) for r in new['references']]
art+=[ntpath.join(run,'build',r['name']) for r in new['compiler_final_outputs']]
art+=[ntpath.normpath(ntpath.join(run,r['path'])) for r in new['control_files']]
art+=[ntpath.join(run,'stage',n) for n in ['stdout.log','stderr.log','receipt.json','consumed.json']]
paths=trans+art+[ntpath.join(run,'stage',n) for n in ['bundle.json','failure-inventory.json']]
pool=set();visits=0;max_depth=0
for path in paths:
    at=path;depth=0
    while at:
        pool.add(at);visits+=1;depth+=1;parent=ntpath.dirname(at)
        if at==parent:break
        at=parent
    max_depth=max(max_depth,depth)
strings=['ClockOracle','CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED','A'*64,'OPAQUE',new['predecessor_bundle']['path'],'A'*64]
strings+=[s for path in trans+art for s in [path,'A'*64,'A'*64]]
strings+=[r['path'] for r in new['references']]
escaped=sum(len(json.dumps(s,ensure_ascii=True,separators=(',',':')))-2 for s in strings)
bundle=46364+2*5251+escaped+2*3938+20*1302+5*8+2*1294+4+5251+3957+1290+1
geometry=dict(selected_or_executed=False,base=base,new_run_root_proposal=run,identities=len(paths),pooled_strings=len(pool),pooled_UTF16_units=sum(map(len,pool)),chain_visits=visits,max_depth=max_depth,own5_path_maximum=max(map(len,own_paths)),known_current_identity_path_maximum=max(map(len,trans)),bundle_encoded_string_units=escaped,bundle_byte_upper=bundle,upper_request_reduction=7*(1048576-bundle),conditions='Lexical candidate only. Exact native normalization, eventual source/binding identities, selected argv/env and six lifetime/error phases remain separately required.')
report=dict(scope='Independent text/hash/artifact JSON/integer correspondence only; no target PS parser/AST/helper/import/compiler/program/stage execution.',checks=checks,failures=[r for r in checks if not r['passed']],check_count=len(checks),input_bytes=input_bytes,input_chunks=input_chunks,own_five_bytes=own_bytes,own_five_chunks=own_chunks,bootstrap_shape_old=shape_old,bootstrap_shape_new=shape_new,new_base_geometry=geometry,documentation_observation={'inherited_own5_maximum_claim':170,'new_own5_maximum':geometry['own5_path_maximum'],'lineage':'Original README prefix is deliberately preserved; its unqualified current own5 maximum must not be interpreted as successor profile data. Exact new profile supplied here is still unselected.'})
(OUT/'mechanical-evidence.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(checks=len(checks),failures=report['failures'],own_five_bytes=own_bytes,bootstrap_shape=shape_new,new_base_geometry=geometry),indent=2))
raise SystemExit(1 if report['failures'] else 0)
