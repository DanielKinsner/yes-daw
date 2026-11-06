"""Ordinary source/integer-data checks only; never reads current environment."""
from pathlib import Path
import hashlib
import json
import re

root=Path(__file__).resolve().parents[1]
base=root/'build-ci/20261105-controller-environment-writer-closure-r2'
origin_path=base/'source-origins.json'
origin=json.loads(origin_path.read_text())
digest=lambda b:hashlib.sha256(b).hexdigest().upper()
assert digest(origin_path.read_bytes())=='5314810ED0AF072A0103248367FAF8A42E51D3C1835242D2D40309771D06DA68'
assert origin['commit']=='ea554a8f9c6085f54fb2828f7ea286c65c35599f'
assert len(origin['files'])==1245
texts={}
for row in origin['files']:
    p=base/'primary'/row['repository_path'];b=p.read_bytes()
    assert len(b)==row['bytes'] and digest(b)==row['sha256']
    texts[row['repository_path']]=b.decode('utf-8-sig')
seed_path=root/'build-ci/20261105-clock-oracle-r10-controller-environment-proposal-r1/proposal.json'
seed_raw=seed_path.read_bytes();assert digest(seed_raw)=='3294595F71EDB97130703808C6CB9B6136DAF56716C93E1580CC7DC2CB8E126E'
seed=json.loads(seed_raw)['seed_entries']
units=lambda s:len(s.encode('utf-16-le'))//2
assert len(seed)==len({k.casefold() for k in seed})==14
assert all(k and '=' not in k and '\0' not in k+v for k,v in seed.items())
assert sum(units(k)+units(v) for k,v in seed.items())==820
assert units(seed['PSModulePath'])==234
assert not any(k.casefold()=='pathext' for k in seed)
prefix='C:\\Users\\SM - Dan\\.cache\\codex-runtimes\\codex-primary-runtime\\dependencies\\native\\powershell;'
assert units(prefix)==93
writers=[]
for name,text in texts.items():
    for number,line in enumerate(text.splitlines(),1):
        if line.lstrip().startswith('//'):continue
        if re.search(r'\b(?:System\.)?Environment\.SetEnvironmentVariable\s*\(',line):
            writers.append(dict(repository_path=name,line=number,text=line.strip()))
assert len(writers)==13, len(writers)
expected_paths={
 'src/powershell/Program.cs':1,
 'src/Microsoft.PowerShell.ConsoleHost/host/msh/ConsoleHost.cs':2,
 'src/System.Management.Automation/security/SecuritySupport.cs':1,
 'src/System.Management.Automation/engine/AutomationEngine.cs':2,
 'src/System.Management.Automation/namespaces/EnvironmentProvider.cs':3,
 'src/System.Management.Automation/engine/InitialSessionState.cs':1,
 'src/System.Management.Automation/engine/Modules/ModuleIntrinsics.cs':1,
 'src/System.Management.Automation/engine/NativeCommandProcessor.cs':1,
 'src/System.Management.Automation/engine/remoting/fanin/WSManPlugin.cs':1,
}
assert {p:sum(w['repository_path']==p for w in writers) for p in expected_paths}==expected_paths
engine=texts['src/System.Management.Automation/engine/AutomationEngine.cs']
assert 'Environment.SetEnvironmentVariable("PATHEXT", ".CPL")' in engine
assert 'if (string.IsNullOrEmpty(pathext))' in engine
iss=texts['src/System.Management.Automation/engine/InitialSessionState.cs']
assert 'private bool _wasExecutionPolicySet = false;' in iss
assert 'if (_wasExecutionPolicySet)' in iss
assert 'ss.EnvironmentVariables.Add(this.EnvironmentVariables.Clone());' in iss
assert 'Parallel.Invoke(actions);' in iss and 'Bind_SetEnvironment\n' in iss
assert not re.search(r'class\s+\w+[^\{;]*:\s*[^\{;]*IModuleAssemblyInitializer', '\n'.join(texts.values()))
module=texts['src/System.Management.Automation/engine/Modules/ModuleIntrinsics.cs']
assert 'Environment.SetEnvironmentVariable(Constants.PSModulePathEnvVar, newModulePathString)' in module
assert 'public const string PSModulePathEnvVar = "PSModulePath";' in module
runtime=root/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1'
runner=runtime.read_text();assert digest(runtime.read_bytes())=='3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F'
assert 'SetEnvironmentVariable' not in runner and not re.search(r'(?i)\$env:',runner)
assert "$taskInfo.Environment.Clear();foreach" in runner
assert "@('<APPHOST>','-NoLogo','-NoProfile','-NonInteractive','-File'" in runner
assert "'-ExecutionPolicy'" not in runner and "'-ConfigurationFile'" not in runner
retained=dict(seed);retained['PATH']=prefix+seed['PATH'];retained['PATHEXT']='.CPL'
assert len(retained)==len({k.casefold() for k in retained})==15
fixed=sum(units(k)+units(v) for k,v in retained.items())-units(retained['PSModulePath'])
assert fixed==690
examples=[]
for p in [0,1,234,32766]:
    v=fixed+p;block=v+2*len(retained)+1
    assert block==721+p
    examples.append(dict(PSModulePath_units=p,Eentry=15,Eunique_valid_keys=15,EUTF16=v,Bnative_bytes=2*block))
assert examples[2]['EUTF16']==924 and examples[2]['Bnative_bytes']==1910
result=dict(schema='CONTROLLER_ENVIRONMENT_SOURCE_WRITER_CLOSURE_R2',
 status='CONDITIONAL_SOURCE_THEOREM_EXTERNAL_STARTUP_BUILD_PROFILE_OBLIGATION_REMAINS',
 scope='Pinned stock PowerShell source and authored seed only; no current environment, user/machine values, registry, controller or target execution',
 source_files_verified=1245,source_bytes_verified=sum(r['bytes'] for r in origin['files']),
 seed_entry_count=14,source_added_key='PATHEXT',source_added_value='.CPL',
 qualifying_post_startup_Eentry=15,qualifying_post_startup_Eunique_valid_keys=15,
 equations=dict(EUTF16='690+P',Bnative_bytes='2*(721+P)',P='Actual PSModulePath UTF16 value units at each boundary; never assumed234/idempotent',
  documented_Windows_value_max_including_terminator=32767,conditional_P_max=32766,
  conditional_EUTF16_max=33456,conditional_Bnative_bytes_max=66974),
 examples=examples,writers=writers,
 external_profile_obligations=[
  'Exact pre-startup direct apphost launch with supplied14-key unique valid Unicode block, exact14 argv entries, no inherited/drive-hidden variables or unlisted startup hooks/profilers/settings/configuration overrides.',
  'Exact supported Windows PowerShell/nativehost/CoreCLR/module/dependency build/source association; no native loader/shim/generated/third-party callback adds an unlisted environment writer. PowerShell C# source inventory alone does not certify native startup.',
  'CreateDefault2, empty configured environment collection, default unset execution-policy flag, no external ISS/session configuration/PSReadLine/profile/script/type-data augmentation; source and binary/inheritance/loader association must hold.',
  'Exact two pinned DLL-only module manifests, no external generated/inherited initializer or untrusted loader/type augmentation beyond the audited stock source. No implementing initializer class appears in these1245 source files; that is not a binary reflection/IL result.',
  'After startup/imports the source runner executes only its closed managed/helper graph and direct Process.Start children, with no native-command package-manager pipeline, environment-provider writes, remoting, policy changes or independent/concurrent environment writer.',
  'Windows process-environment getter/setter contract including32767-character value size with NUL; no malformed/hidden/duplicate native entries outside the authored/setter-generated valid block.'
 ],
 no_current_environment_read_or_dump=True,no_private_user_config_or_registry_read=True,
 no_actual_boundary_snapshot=True,no_unconditional_numeric_N_fit_or_GO=True)
output=base/'writer-closure.json';assert not output.exists();output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(source_files=1245,direct_SetEnvironmentVariable_sites=13,Eentry_conditional=15,EUTF16_max_conditional=33456,Bnative_max_conditional=66974,output_sha256=digest(output.read_bytes()))))
