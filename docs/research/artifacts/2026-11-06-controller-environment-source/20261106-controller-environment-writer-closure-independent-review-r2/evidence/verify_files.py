"""Independent ordinary-file/source-text/JSON/integer review; no environment API access."""
from pathlib import Path
import collections
import hashlib
import json
import re

ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/'build-ci/20261105-controller-environment-writer-closure-r2'
OUT=Path(__file__).resolve().parent
files={};checks=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
    p=Path(p);p=p if p.is_absolute() else ROOT/p;b=p.read_bytes()
    try:name=p.relative_to(ROOT).as_posix()
    except ValueError:name=p.as_posix()
    files[name]={'path':name,'bytes':len(b),'sha256':sha(b)}
    return b
def ck(n,ok):
    assert ok,n
    checks.append({'check':n,'pass':True})
def load(p):return json.loads(read(p))
pins=load(BASE/'source-pins.json')
ck('subject pins supplied seal',sha(read(BASE/'source-pins.json'))=='94D3B05DFA28ABFD616E2AC0E7EBDBA4E950D172BFA2FE37D20F6376FBAA034A')
for r in pins['files']:
    b=read(r['path']);ck('subject file '+r['path'],len(b)==r['bytes'] and sha(b)==r['sha256'])
o=load(BASE/'source-origins.json');c=load(BASE/'writer-closure.json')
ck('exact pinned PowerShell source commit',o['commit']=='ea554a8f9c6085f54fb2828f7ea286c65c35599f')
ck('1245unique CSharp source identities',len(o['files'])==1245==len({r['repository_path'] for r in o['files']}) and all(r['repository_path'].startswith('src/') and r['repository_path'].endswith('.cs') for r in o['files']))
source={};writers=[];wider=[];interface_mentions=[]
for r in o['files']:
    b=read(BASE/'primary'/r['repository_path']);ck('primary '+r['repository_path'],len(b)==r['bytes'] and sha(b)==r['sha256'])
    text=b.decode('utf-8-sig');source[r['repository_path']]=text
    # Full-text whitespace tolerance includes multiline call spellings.
    for m in re.finditer(r'\b(?:System\s*\.\s*)?Environment\s*\.\s*SetEnvironmentVariable\s*\(',text):
        line=text.count('\n',0,m.start())+1;row=text.splitlines()[line-1]
        if not row.lstrip().startswith('//'):writers.append({'repository_path':r['repository_path'],'line':line,'text':row.strip()})
    for i,line in enumerate(text.splitlines(),1):
        if re.search(r'\b(?:SetEnv|setenv|putenv|unsetenv|SetEnvironmentStrings|RtlSetEnvironment)\b',line):wider.append({'repository_path':r['repository_path'],'line':i,'text':line.strip()})
        if 'IModuleAssemblyInitializer' in line:interface_mentions.append({'repository_path':r['repository_path'],'line':i,'text':line.strip()})
ck('source bytes30347244',sum(r['bytes'] for r in o['files'])==c['source_bytes_verified']==30347244)
ck('all13direct setter sites exact',writers==c['writers'] and len(writers)==13)
counts=dict(collections.Counter(r['repository_path'] for r in writers))
ck('nine writer files',len(counts)==9 and sorted(counts.values())==[1,1,1,1,1,1,2,2,3])
ck('no source alias for System.Environment',not any(re.search(r'using\s+\w+\s*=\s*(?:global::)?System\s*\.\s*Environment',s) for s in source.values()))
ck('no direct CSharp initializer-interface class',not any(re.search(r'\bclass\s+\w+[^\{;]*:\s*[^\{;]*\bIModuleAssemblyInitializer\b',s) for s in source.values()))
ck('wider SetEnv family only Program Unix source',wider and all(r['repository_path']=='src/powershell/Program.cs' for r in wider))
program=source['src/powershell/Program.cs']
ck('Unix guards contain login deletion and SetEnv',program.index('#if UNIX',program.index('public static int Main'))<program.index('AttemptExecPwshLogin(args)')<program.index('#endif',program.index('public static int Main'))
   and '#if UNIX\n' in program and program.index('SetEnv(LOGIN_ENV_VAR_NAME')<program.rindex('#endif'))
engine=source['src/System.Management.Automation/engine/AutomationEngine.cs']
ck('Windows PATHEXT absent CPL/idempotence',all(t in engine for t in ['#if !UNIX','if (string.IsNullOrEmpty(pathext))','Environment.SetEnvironmentVariable("PATHEXT", ".CPL")','pathext.Equals(".CPL", StringComparison.OrdinalIgnoreCase)']))
iss=source['src/System.Management.Automation/engine/InitialSessionState.cs']
host=source['src/Microsoft.PowerShell.ConsoleHost/host/msh/ConsoleHost.cs']
entrance=source['src/Microsoft.PowerShell.ConsoleHost/host/msh/ManagedEntrance.cs']
ck('actual stock managed entry CreateDefault2', 'ConsoleHost.DefaultInitialSessionState = InitialSessionState.CreateDefault2();' in entrance and 'issProvidedExternally: false' in entrance)
ck('empty ISS constructor and lazy environment collection','protected InitialSessionState()\n        {\n        }' in iss and 'new InitialSessionStateEntryCollection<SessionStateVariableEntry>()' in iss)
ck('default environment clone and policy flag guarded','ss.EnvironmentVariables.Add(this.EnvironmentVariables.Clone());' in iss and 'private bool _wasExecutionPolicySet = false;' in iss and 'if (_wasExecutionPolicySet)' in iss)
ck('Bind action join and serial module update','Parallel.Invoke(actions);' in iss and 'if (updateOnly)' in iss and 'Bind_SetEnvironment' in iss)
ck('startup not a global join:three unawaited tasks',iss[:iss.index('public abstract class InitialSessionStateEntry')].count('Task.Run(')==3 and 'EarlyStartup.Init();' in entrance)
ck('native AMSI warmup before early return','Task.Run(() => AmsiUtils.WinScanContent(content: string.Empty, sourceMetadata: string.Empty, warmUp: true))' in iss
   and 'if (!CheckAmsiInit())' in source['src/System.Management.Automation/security/SecuritySupport.cs'])
ck('Console PATH prefix gated','path.StartsWith(pshome, StringComparison.Ordinal)' in host and 'pshome += Path.PathSeparator;' in host)
module=source['src/System.Management.Automation/engine/Modules/ModuleIntrinsics.cs']
ck('module only writes existing PSModulePath on nonempty value','public const string PSModulePathEnvVar = "PSModulePath";' in module and 'if (!string.IsNullOrEmpty(newModulePathString))' in module
   and 'Environment.SetEnvironmentVariable(Constants.PSModulePathEnvVar, newModulePathString)' in module)
ck('private module path values not asserted absent',all(t in module for t in ['EnvironmentVariableTarget.User','EnvironmentVariableTarget.Machine','PowerShellConfig.Instance.GetModulePath(ConfigScope.AllUsers)','PowerShellConfig.Instance.GetModulePath(ConfigScope.CurrentUser)']))
native=source['src/System.Management.Automation/engine/NativeCommandProcessor.cs']
ck('package manager completion guarded','if (_isPackageManager)' in native and 'UpdateProcessEnvPath(_originalUserEnvPath, _originalSystemEnvPath);' in native)

proposal=load('build-ci/20261105-clock-oracle-r10-controller-environment-proposal-r1/proposal.json')
ck('seed exact seal',sha(read('build-ci/20261105-clock-oracle-r10-controller-environment-proposal-r1/proposal.json'))=='3294595F71EDB97130703808C6CB9B6136DAF56716C93E1580CC7DC2CB8E126E')
seed=proposal['seed_entries']
units=lambda s:len(s.encode('utf-16-le'))//2
ck('14unique valid seed entries',len(seed)==len({k.casefold() for k in seed})==14 and all(k and '=' not in k and '\0' not in k+v for k,v in seed.items()))
ck('seed820/PSMP234 and noPATHEXT',sum(units(k)+units(v) for k,v in seed.items())==820 and units(seed['PSModulePath'])==234 and 'pathext' not in {k.casefold() for k in seed})
runtime='C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/'
prefix=runtime.rstrip('/').replace('/','\\')+';'
ck('exact appbase92 plussemicolon93',units(prefix)==93 and units(seed['PATH'])==30 and not seed['PATH'].startswith(prefix))
post=dict(seed);post['PATH']=prefix+seed['PATH'];post['PATHEXT']='.CPL'
fixed=sum(units(k)+units(v) for k,v in post.items())-units(post['PSModulePath'])
ck('conditional fixed690 and15keys',fixed==690 and len(post)==15)
for row in c['examples']:
    p=row['PSModulePath_units'];ck('example '+str(p),row=={'PSModulePath_units':p,'Eentry':15,'Eunique_valid_keys':15,'EUTF16':690+p,'Bnative_bytes':2*(721+p)})
ck('conditional documented maxima arithmetic',32767-1==c['equations']['conditional_P_max']==32766 and 690+32766==c['equations']['conditional_EUTF16_max']==33456 and 2*(721+32766)==c['equations']['conditional_Bnative_bytes_max']==66974)
ck('telemetry optout/updateOff',seed['POWERSHELL_TELEMETRY_OPTOUT']=='1' and seed['POWERSHELL_UPDATECHECK']=='Off')
runner=read('build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1').decode()
ck('R12source exact identity',sha(runner.encode())=='3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F')
ck('no direct process env writes or env-drive variable in runner','SetEnvironmentVariable' not in runner and not re.search(r'(?i)\$env:',runner))
ck('child dictionary mutation only','$taskInfo.Environment.Clear();foreach' in runner and '$taskInfo.Environment[$taskKey]=$taskValue' in runner)
ck('14symbolic native argv and no policy/config override',"@('<APPHOST>','-NoLogo','-NoProfile','-NonInteractive','-File'" in runner and all(t not in runner for t in ["'-ExecutionPolicy'","'-SettingsFile'","'-ConfigurationFile'"]))
modfiles=[]
for n in ['Management','Utility']:
    p=Path(runtime)/'Modules'/('Microsoft.PowerShell.'+n)/('Microsoft.PowerShell.'+n+'.psd1')
    text=read(p).decode('utf-8-sig');body=text.split('# SIG # Begin signature block')[0]
    expected={'Management':'A73617CBA10DF0D2641DF909C6B8BD7981769B3FC78845976290093DF2941941','Utility':'D103110AAE93CA1061FAB8ADD8F00EA6110740AC94B236654D0EFB0B08654AAA'}[n]
    ck('exact selected module manifest '+n,sha(read(p))==expected)
    found=re.findall(r'(?m)^\s*(RootModule|NestedModules|ScriptsToProcess|TypesToProcess|FormatsToProcess|RequiredModules|RequiredAssemblies)\s*=\s*(.*)$',body)
    ck('DLL-only manifest '+n,len(found)==1 and found[0][0]=='NestedModules' and 'Microsoft.PowerShell.Commands.'+n+'.dll' in found[0][1])
    modfiles.append({'name':n,'loader_fields':found})
core=read('build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Environment.Variables.Windows.cs').decode()
core_origin=next(r for r in load('build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-source-evidence.json') if r['path'].endswith('Environment.Variables.Windows.cs'))
ck('exact official CoreLib source lineage',sha(core.encode())==core_origin['sha256']=='84E3F67D9C45361C862784C5F75FED4A53507B4D42B364A707C57F411DC6BE4A' and '/e2f47b0110ed922f21a1522da67279133ce28f32/' in core_origin['url'])
ck('native block copy includes hidden before managed skip','GetEnvironmentStringsW()' in core and 'if (i > 0)' in core and 'FreeEnvironmentStringsW(stringPtr)' in core)
ck('CoreLib setter failure translation','SetEnvironmentVariableCore' in core and 'if (!Interop.Kernel32.SetEnvironmentVariable(variable, value))' in core and 'ERROR_FILENAME_EXCED_RANGE' in core)
result={'schema':'CONTROLLER_ENVIRONMENT_R2_INDEPENDENT_FILE_SOURCE_CHECKS','checks_passed':len(checks),'checks':checks,'verified_files':list(files.values()),'writers':writers,'writer_files':counts,'wider_program_Unix_SetEnv_mentions':wider,'initializer_interface_mentions':interface_mentions,'module_manifest_loader_fields':modfiles,'conditional_profile':{'Eentry':15,'Eunique_valid_keys':15,'EUTF16':'690+P_i','logical_native_block_UTF16_units':'721+P_i','logical_native_block_bytes':'2*(721+P_i)','conditional_P_max':32766,'conditional_EUTF16_max':33456,'conditional_Bnative_bytes_max':66974},'new_named_external_gap':'EarlyStartup.Init fire-and-forget tasks/native AMSI and type/resource initialization are not joined by Bind Parallel.Invoke; completion or noninterference through captures remains an external obligation.','no_environment_registry_private_config_or_target_API_calls':True}
(OUT/'file-checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'checks':len(checks),'verified_files':len(files),'source_files':1245,'source_bytes':30347244,'direct_writers':len(writers),'writer_files':len(counts),'conditional_profile':result['conditional_profile']}))
