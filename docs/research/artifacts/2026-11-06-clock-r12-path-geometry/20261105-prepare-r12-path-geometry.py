"""Provisional literal-path geometry and primary source fetch; no target calls."""
from pathlib import Path
import hashlib
import json
import ntpath
import urllib.request

root=Path(__file__).resolve().parents[1]
folder=root/'build-ci/20261105-clock-oracle-r12-path-geometry-r1'
assert not folder.exists();folder.mkdir();(folder/'primary').mkdir()
manifest_path=root/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/source-manifest.json'
raw=manifest_path.read_bytes();digest=lambda b:hashlib.sha256(b).hexdigest().upper()
assert digest(raw)=='4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557'
m=json.loads(raw)
repo=str(root).replace('/','\\');base=str(manifest_path.parent).replace('/','\\')
run=ntpath.join(base,'runs','source-clock-r1')
assert not Path(run).exists(), 'Proposed root only; do not create it'
normalize=lambda p:ntpath.normpath(p)
rows=[]
for r in m['inputs']:
    name=r['path'];canonical=normalize(name if ntpath.isabs(name) else ntpath.join(repo,name))
    rows.append(dict(kind='input',name=name,canonical=canonical))
for name in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json']:
    full=ntpath.join(base,name);rows.append(dict(kind='own',name=full,canonical=normalize(full)))
for descriptor in m['references']:
    full=ntpath.join(run,'data',descriptor['path']);rows.append(dict(kind='reference',name=full,canonical=normalize(full)))
for descriptor in m['compiler_final_outputs']:
    full=ntpath.join(run,'build',descriptor['name']);rows.append(dict(kind='compiler',name=full,canonical=normalize(full)))
for descriptor in m['control_files']:
    full=ntpath.join(run,descriptor['path']);rows.append(dict(kind='control',name=full,canonical=normalize(full)))
for name in m['stage_files']:
    full=ntpath.join(run,'stage',name);rows.append(dict(kind='stage',name=full,canonical=normalize(full)))
assert len(rows)==m['profile']['maximum_complete_fence_identities']==1294
strings=set();parents={}
for r in rows:
    assert '~' not in r['name'] and all(32<=ord(c)<=126 for c in r['name'])
    p=r['canonical'];chain=[]
    while p:
        assert len(chain)<65;chain.append(p);strings.add(p)
        drive,tail=ntpath.splitdrive(p)
        parent=None if tail in ('','\\') else ntpath.dirname(p)
        if parent==p:parent=None
        assert p not in parents or parents[p]==parent
        parents[p]=parent;p=parent
    r['parent_chain']=chain
unit_sum=sum(len(s) for s in strings)
result=dict(scope='PROVISIONAL_R12_LITERAL_GEOMETRY_ONLY_NOT_FINAL_UNION_OR_PAYLOAD_FIT',manifest_sha256=digest(raw),repo=repo,base=base,proposed_uncreated_run_root=run,
    source_root_selection='Concrete proposal only; final canonical functional/census/resource/data union still pending. No source/domain/cap change.',
    identities=len(rows),known_pool_strings=len(strings),known_pool_UTF16_units=unit_sum,
    maximum_canonical_units=max(len(r['canonical']) for r in rows),maximum_known_depth=max(len(r['parent_chain']) for r in rows),
    parent_array_reference_slots=65*len(rows),alias_limit=2*len(m['inputs'])+3*(len(rows)-len(m['inputs'])),
    canonical_windows_semantics_qualification='ntpath lexical Windows model only; selected no-tilde drive spelling/Join-Path/provider/Path.GetFullPath source correspondence remains to verify. No filesystem resolution or .NET target function ran.',
    all_known_strings_sorted=sorted(strings),parent_edges=[dict(path=k,parent=v) for k,v in sorted(parents.items())],rows=rows,
    not_allocation_or_savings=True,first_refusal_unknown_tilde_constructor_name_and_copy_schedules_pending=True)
(folder/'geometry.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
commit='e2f47b0110ed922f21a1522da67279133ce28f32'
prefix='https://raw.githubusercontent.com/dotnet/dotnet/'+commit+'/src/runtime/src/libraries/System.Private.CoreLib/src/System/IO/'
files=['FileInfo.cs','FileSystemInfo.cs','FileSystemInfo.Windows.cs','FileStream.cs','PathHelper.Windows.cs']
origins=[]
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
for name in files:
    with opener.open(urllib.request.Request(prefix+name,headers={'User-Agent':'YES-DAW-fixed-source-review'}),timeout=30) as response:data=response.read(1048577)
    assert len(data)<=1048576;data.decode('utf-8-sig');(folder/'primary'/name).write_bytes(data)
    origins.append(dict(file=name,url=prefix+name,bytes=len(data),sha256=digest(data)))
(folder/'primary-source-origins.json').write_text(json.dumps(dict(commit=commit,files=origins),indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({k:result[k] for k in ['identities','known_pool_strings','known_pool_UTF16_units','maximum_canonical_units','maximum_known_depth','parent_array_reference_slots','alias_limit']}))
