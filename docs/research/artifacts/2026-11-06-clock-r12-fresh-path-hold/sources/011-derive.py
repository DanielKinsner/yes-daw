"""R12 path referent source closure: files/text/hashes/JSON/integer arithmetic only.
No PowerShell, SDK method, target/parser/metadata helper or old checker is loaded.
"""
from pathlib import Path
import hashlib
import json
import ntpath
import re

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
GEO=ROOT/'build-ci/20261105-clock-oracle-r12-path-geometry-r1/geometry.json'
SOURCE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
ARCHIVE=ROOT/'docs/research/artifacts/2026-11-06-clock-r12-resource-hold-baseline'
BASE=ARCHIVE/'build-ci/20261106-clock-r12-payload-evaluation-r1'
checks=[];pins={}
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
    b=p.read_bytes();pins[p.relative_to(ROOT).as_posix()]=dict(path=p.relative_to(ROOT).as_posix(),bytes=len(b),sha256=sha(b));return b
def ck(name,a,b=True):
    ok=a==b;checks.append(dict(name=name,passed=ok))
    if not ok:raise ValueError(name)
def text(p):return read(p).decode('utf-8-sig')
def snippet(p,s):ck(p.name+':'+s[:55],s in text(p))
def a8(n):return ((n+7)//8)*8
def string(n):return 0 if n==0 else a8(22+2*n)
def arrchar(n):return a8(24+2*n)
def exact_string_request_sum(values):return sum(string(len(s.encode('utf-16-le'))//2) for s in values)
def j(parent,child):return parent+('' if parent.endswith(('\\','/')) or child.startswith(('\\','/')) else '\\')+child
def canonical(s):return ntpath.normpath(s) # Independent lexical arithmetic, not a Windows API.

if (HERE/'closure.json').exists():raise ValueError('Preserve first complete result')
g=json.loads(read(GEO));m=json.loads(read(SOURCE/'source-manifest.json'));src=text(SOURCE/'run-clock.ps1')
base=json.loads(read(BASE/'baseline.json'));ev=json.loads(read(BASE/'evaluation.json'))
ck('exact geometry',sha(read(GEO)),'BE95DC06AB85E7BF0A706656B08A6C2C8B5581C4AE3376BFE7AB1D98CADBAD9F')
ck('exact R12 manifest',sha(read(SOURCE/'source-manifest.json')),'4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557')
ck('exact R12 runner',sha(src.encode()),'3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F')
ck('same baseline source',(base['source_manifest_sha256'],base['runner_sha256']),(g['manifest_sha256'],sha(src.encode())))
ck('selected known geometry',(len(g['rows']),len(g['all_known_strings_sorted']),sum(map(len,g['all_known_strings_sorted'])),max(map(len,g['all_known_strings_sorted']))),(1294,1458,186478,270))
for p in [ARCHIVE/'README.md',ARCHIVE/'archive-manifest.json',ROOT/'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/review.md',ROOT/'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/source-pins.json',ROOT/'build-ci/20261106-controller-v-wrapper-source-closure-r1/closure.json']:
    read(p)

primary=ROOT/'build-ci/20261106-fresh-path-primary-source-r1'
manip=primary/'String.Manipulation.cs'
for s in ['if (oldChar == newChar)','if (firstIndex < 0)','string result = FastAllocateString(Length);','if (length == Length)','return InternalSubString(startIndex, length);','if (values is List<string?> valuesList)','return JoinCore(separator.AsSpan(), CollectionsMarshal.AsSpan(valuesList));','if (values.Length <= 1)','string result = FastAllocateString(totalLength);','if (typeof(T) == typeof(string))','if (replacementIndices.Length == 0)','string dst = FastAllocateString((int)dstLength);','internal const int StackallocIntBufferSizeLimit = 128;']:
    snippet(manip,s)
helper=ROOT/'build-ci/20261105-clock-oracle-r12-path-geometry-r1/primary/PathHelper.Windows.cs'
for s in ['new ValueStringBuilder(stackalloc char[PathInternal.MaxShortPath])','builder.AsSpan().Equals(path.AsSpan(), StringComparison.Ordinal) ? path : builder.ToString()','builder.EnsureCapacity(checked((int)result));','path.ToString()','ValueStringBuilder inputBuilder = default;','GetLongPathNameW','inputBuilder.Dispose();']:
    snippet(helper,s)
path=ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Path.cs'
for s in ['PathInternal.NormalizeDirectorySeparators(path.Substring(0, end))','if (path.Length == result.Length)','return result.ToString();','relativeTo = GetFullPath(relativeTo);','path = GetFullPath(path);','sb.EnsureCapacity(Math.Max(relativeTo.Length, path.Length));']:
    snippet(path,s)
snip_windows=ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Path.Windows.cs'
snippet(snip_windows,'if (PathInternal.IsExtended(path.AsSpan()))');snippet(snip_windows,'return PathHelper.Normalize(path);')
entrycore=ROOT/'build-ci/20261106-file-system-entry-core-source-r1/FileSystemEntry.cs'
snippet(entrycore,'ReadOnlySpan<char> relativePath = Directory.Slice(RootDirectory.Length);');snippet(entrycore,'return Join(OriginalRootDirectory, relativePath, FileName);')
snippet(primary/'FileSystemEnumerableFactory.cs','(ref FileSystemEntry entry) => entry.ToSpecifiedFullPath()')
snippet(primary/'FileSystemEntry.Windows.cs','return Path.Join(originalRootDirectory, relativePath, fileName);')
for f in ['ValueStringBuilder.cs','SharedArrayPool.cs','PathInternal.Windows.cs']:
    read(ROOT/'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/primary'/f)

# Exact known production requests before interning. A request upper is not an
# actual allocation observation, retained/live set or pool hit assumption.
parents=[x['parent'] for x in g['parent_edges'] if x['parent']]
ck('proper nonempty parent requests',(len(parents),sum(map(len,parents))),(1457,157789))
known_parent_bytes=exact_string_request_sum(parents)
relative_inputs=[r for r in g['rows'] if r['kind']=='input' and not ntpath.isabs(r['name'])]
absolute_slash_inputs=[r for r in g['rows'] if r['kind']=='input' and ntpath.isabs(r['name']) and '/' in r['name']]
noninputs=[r for r in g['rows'] if r['kind']!='input']
aliases=[r for r in noninputs if '/' in r['name']]
manifest_name=j(g['base'],'source-manifest.json')
initial_paths=[manifest_name]+[j(g['proposed_uncreated_run_root'],x) for x in ['stage','data','build','opaque-temp']]
catalogue_join=[r['name'] for r in noninputs if r['name']!=manifest_name]
ck('known request partitions',(len(relative_inputs),len(absolute_slash_inputs),len(noninputs),len(aliases),len(catalogue_join)),(363,857,74,60,73))
known_native_result=[r['canonical'] for r in absolute_slash_inputs+aliases]
known_entry_join=[r['canonical'] for r in relative_inputs]
alias_strings=[r['name'].replace('/','\\') for r in aliases]
known_one_time=dict(parent_candidates_before_interning=known_parent_bytes,absolute_slash_and_noninput_canonical_result_strings=exact_string_request_sum(known_native_result),relative_input_JoinPath_final_outputs=exact_string_request_sum(known_entry_join),noninput_alias_Replace_char_results=exact_string_request_sum(alias_strings),initial_five_IOJoin_outputs=exact_string_request_sum(initial_paths),catalogue_73_IOJoin_outputs=exact_string_request_sum(catalogue_join))
runtime=re.search(r"\$taskRuntime='([^']+)'",src)[1]
pin_block=src[src.index(' $taskPins=@('):src.index(' foreach($taskPin in $taskPins)')]
pin_leaves=re.findall(r"@\('([^']+)'",pin_block)
ck('seven controller pin leaves',len(pin_leaves),7)
controller_joins=[j(runtime,x) for x in pin_leaves]+[j(runtime,'pwsh.exe'),j(g['base'],'run-clock.ps1'),j(runtime,'pwsh.exe'),j(runtime,'pwsh.dll'),j(runtime,'Modules/Microsoft.PowerShell.Management/Microsoft.PowerShell.Management.psd1'),j(runtime,'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')]+[j(runtime,'Microsoft.PowerShell.Commands.Utility.dll')]*2
ck('controller IOJoin sites15',len(controller_joins),15)
known_one_time['controller_15_IOJoin_output_upper']=exact_string_request_sum(controller_joins)
known_one_time['seven_bootstrap_parent_candidates_upper']=7*64*string(270)+2*string(270)

ns_abs=[];ns_rel=[];ns_names=[];ns_all_paths=[]
for n in m['closed_directory_inputs']:
    root=n['path'];full=canonical(root if ntpath.isabs(root) else j(g['repo'],root))
    (ns_abs if ntpath.isabs(root) else ns_rel).append(full)
    for f in n['files']:
        ns_names.append(f.replace('/','\\'));ns_all_paths.append(canonical(j(full,f)))
ck('namespace branches',(len(ns_abs),len(ns_rel),len(ns_names)),(7,24,1045))
long_rows=[r for r in g['rows'] if len(r['canonical'])>=260]
ck('long known paths11',len(long_rows),11)
ck('all long are input leaves',all(r['kind']=='input' and not any(x['parent']==r['canonical'] for x in g['parent_edges']) for r in long_rows))
ck('all long in namespace files',all(r['canonical'] in ns_all_paths for r in long_rows))

# Every direct string-producing source site is inventoried with its actual line,
# enclosing helper and source. SDK/provider stages have separately named leaves.
starts=list(re.finditer(r'^function ([A-Za-z0-9-]+)(?=\(|\{)',src,re.M));main=src.index('\ntry{\n # Hash literal manifest')
def owner(offset):
    if offset>=main:return 'MAIN_AND_ERROR'
    matches=[x for x in starts if x.start()<=offset]
    return matches[-1][1] if matches else 'BOOTSTRAP_DEFINITIONS'
patterns={
 'JoinPath_provider':r'(?<![A-Za-z0-9-])Join-Path\b',
 'IO_Path':r'\[IO\.Path\]::(?:Join|GetFullPath|GetDirectoryName|GetFileName|GetRelativePath)\(',
 'Replace':r'\.Replace\(', 'Substring':r'\.Substring\(', 'iterator_Current':r'\$taskIterator\.Current',
 'safe_prefix':r'(?<![A-Za-z0-9-])Get-TaskSafePrefix\s',
 'other_provider_boundary':r'(?<![A-Za-z0-9-])(?:Get-Item|Test-Path|New-Item|Import-Module)\b'}
sites=[]
for kind,pat in patterns.items():
    for hit in re.finditer(pat,src):
        line=src.count('\n',0,hit.start())+1;line_text=src.splitlines()[line-1]
        if line_text.lstrip().startswith('#'):continue
        sites.append(dict(kind=kind,line=line,owner=owner(hit.start()),source=line_text))

stage=[0,7,264,523,782,784];fences=[1,1,3,5,8,8];children=[0,0,1,2,3,3];refs=[0,0,0,1,2,2];builds=[0,0,1,1,2,2];rootwalk=[0,1,1,1,2,2];copies=[0,1,2,2,2,2];receipts=[0,2,2,3,4,4];native=[0,0,0,1,2,2]
phases=[]
F=len(m['inputs'])+5;NS=31;EG=1123;DG=720
for i in range(6):
    S,f,c,r,b,w=stage[i],fences[i],children[i],refs[i],builds[i],rootwalk[i]
    created=int(i>0);dep=int(i>=2);success=int(i>=4);bundle=int(i==5)
    E=6*S+(3 if S else 0)+f*EG+60*r+4*b+78*w+1
    D=S+f*DG+61*r+b+79*w+1;W=S+f*NS+r+b+w+1
    ck('phase stage/rows '+str(i),(S,E),(base['phases'][i]['stage_calls'],ev['phases'][i]['walker_rows']))
    H=ev['phases'][i]['hashes'];NL=H+f*NS+60*r+78*success+1+D+2*copies[i]
    relative_calls=f*EG+60*r+78*success
    # Exact private GetFileName is after the explicit canonical Length<=512 gate.
    # This bound includes all failed Tuple/Add attempts and directory rows.
    file_name_bytes=E*string(512)
    # Four generic join outputs are an upper, not four extra FileSet calls:
    # two actual replacing refusal slots, missing+unknown in each. Each exact
    # List join has K<=1225. K0/1 aliases; K>=2 has one result of<=513K-1 units.
    join_one_max=string(513*F-1)
    generic_join_bytes=4*join_one_max
    # Three expected controls have missing/unknown counts1/0,0/1,0/0: all alias.
    ns_canonical=f*exact_string_request_sum(ns_abs)
    ns_join=f*exact_string_request_sum(ns_rel)
    # All pinned compiler placeholder occurrences5, no reintroduced angle tokens
    # in the selected fixed paths. Length guard precedes each Replace request.
    compiler_replace_bytes=dep*5*string(512)
    # Known non-shortening parent-copy failures remain at most3 unpublished
    # first builds*65 candidate parent requests; unknown/native expansion separate.
    failed_known_parent_bytes=3*65*string(270)
    # These are per-source invocation capacities conditional on the selected
    # documented normalization behavior; never a claim of a new physical rent.
    known_long_normalizer_calls=11*(8*f+1)+3
    known_relative_builder_rents=11*f
    conditional_rent_bytes=(known_long_normalizer_calls+known_relative_builder_rents)*arrchar(1024)
    join_counts={
     'entry_relative_first_build_upper':363+3,
     'namespace_relative_resolution':24*f,
     'compiler_report_relative_fallback_upper':dep*(m['profile']['include_input_count']+1),
     'child_stdout_and_stderr':2*c,'copy_owned_prefix':copies[i],'mutation_owned_prefix':copies[i],
     'receipt_path':receipts[i],'native_stdout_capture':native[i],
     'dependency_report_and_source':2*dep,'failure_inventory_path':created,
     'current_header':1,'direct_run_root_and_sibling_attempt':3,
     'created_directories':3*created,'marker_path':created,'mutant_predecessor_path':created,
     'binary_and_gui_paths':2*dep,'control_artifact_paths':4*success,
     'stage_artifact_paths':4*bundle,'own_transitive_paths':4*bundle,
     'bundle_write_and_hash_paths':2*bundle}
    other_known_final_joins=sum(join_counts.values())-(363+3)-24*f-dep*(m['profile']['include_input_count']+1)
    evaluated=dict(**known_one_time,namespace_absolute_canonical_results=ns_canonical,namespace_relative_JoinPath_final_outputs=ns_join,all_after_guard_GetFileName_strings=file_name_bytes,generic_missing_unknown_join_strings=generic_join_bytes,expected_FileSet_control_join_result_strings=0,compiler_five_Replace_results=compiler_replace_bytes,failed_known_parent_candidates=failed_known_parent_bytes,failed_known_relative_join_and_canonical_candidates=6*string(270),other_fixed_profile_JoinPath_final_results=other_known_final_joins*string(270),selected_root_leaf_Substring=string(len(ntpath.basename(g['proposed_uncreated_run_root']))),selected_safe_prefix_result_strings=0)
    # API-hidden string/buffer leaves deliberately have finite source call counts
    # and explicit unresolved extents, not zero allocation or arbitrary margins.
    normalizer_exposure_upper=192*NL+1344+base['phases'][i]['FileInfo_calls_upper']+(H+7+3+14+2*c)+6+E+13+S+created+2*relative_calls+4+1297+(NL+2*W+E+1)+(10*1220+8*31+1222+4)
    phase=dict(id='P'+str(i),source_schedule=dict(stage_calls=S,stage_rows=6*S+(3 if S else 0),fences=f,children=c,walkers=W,walker_rows=E,directory_iterations=D,Current_requests_upper=E+W,GetFileName_requests_upper=E,GetRelativePath_source_calls_upper=relative_calls,NoLink_calls_upper=NL,GetDirectoryName_unknown_fallback_calls_upper=64*NL+448,JoinPath_calls_upper=sum(join_counts.values()),JoinPath_site_families=join_counts,unknown_compiler_path_values_upper=dep*1222,all_normalizer_exposure_upper=normalizer_exposure_upper),evaluated_source_request_components=evaluated,evaluated_component_sum=sum(evaluated.values()),conditional_1024_char_rent_component=dict(known_normalizer_calls_upper=known_long_normalizer_calls,relative_builder_calls_upper=known_relative_builder_rents,per_request=arrchar(1024),bytes=conditional_rent_bytes,premise='Selected ordinary no-tilde spelling and required Win32 capacity<=271 (or0 error), selected pool bucket rules; no pool-hit/free credit. Provider/unknown/expanded calls excluded, remain residual.'),remaining_equation='P_fresh_i <= evaluated_component_sum + K_known_normalizer_and_error_i + K_1024_rents_i + Current_i + Relative_unknown_i + Provider_intermediates_i + Compiler_fallback_join_outputs_i + Parent_fallback_i + Native_normalizer_expansion_failure_i; no term silently0')
    phases.append(phase)

result=dict(schema='R12_FRESH_PATH_SOURCE_CLOSURE_R1',verdict='PARTIAL_SOURCE_CLOSURE_COMPLETE_PRODUCER_EQUATIONS_P_FRESH_STILL_HOLD',subject_manifest_sha256=g['manifest_sha256'],runner_sha256=sha(src.encode()),geometry_sha256=sha(read(GEO)),no_source_cap_profile_change=True,no_target_execution=True,evaluated_is_not_whole_upper_or_fit=True,known_one_time_requests=known_one_time,source_site_inventory=sites,phases=phases,identity_return_proofs=dict(GetFullPath='Path.Windows129..143+PathHelper25..40: extended returnsinput; ordinaryno~ exactnativecomparison returnsinput after scratch/native work, not another resultstring',ReplaceChar='String.Manipulation1271..1280: samechars/nomatch returnthis;one equal-length result otherwise',Substring='String.Manipulation2217..2261: fullrange returnsinput;empty returnsEmpty;propernonempty requestsnewstring',StringJoin='String.Manipulation785..800/916..932/1035..1068: exactList<string> fastpath to span;0/1 aliases;>=2 onefinalstring,noVSB builder or defensivecopy under closedprivateList no mutation',GetFileName='Path221..230: identity if span is entireinput,otherwise one suffix string; actual fullpaths containroot, after canonical512guard so result<=512',IteratorCurrent='UserEntries transform ToSpecifiedFullPath; FileSystemEntry.cs Directory.Slice(RootDirectory.Length),then private WindowsJoin. Currentcreatedbefore raw512gate; transformed resultnot bounded512 bythat laterguard.'),remaining=[dict(id='Current',calls='E_i+W_i',exact_leaf='Selected native filename span capacity and original/root/relative directory lengths for the first failing MoveNext, before raw512 guard. Need Windows enumeration buffer/name-length source/API association, not a new filename-domain restriction.'),dict(id='Provider',calls='JoinPath_calls_upper plus explicit GetItem/TestPath/NewItem/ImportModule source sites',exact_leaf='Pinned CombinePathCommand->SessionState.MakePath->GetProviderPath->LocationGlobber/ContractRelativePath requests and temporary arrays/strings. Final known join strings above excluded from this residual; discarded provider results are not0.'),dict(id='NativeNormalizer',calls='all_normalizer_exposure_upper; actual selected/cache-hit branches can tighten',exact_leaf='Unknown/~ and failure Win32 result/grow sequence, input/output builder rents/copies and explicit path.ToString on error; no assumed pool hit, no Dispose/Return/free. Known identity result alone is insufficient.'),dict(id='Relative',calls='GetRelativePath_source_calls_upper',exact_leaf='Both GetFullPath calls and EnsureCapacity(max(fullnames)) precede shortened relative result. Unknown expansion/capacity terms stay; known no~ proper-descendant results use pinned geometry.'),dict(id='OtherKnownJoinsAndParents',calls='source-site inventory and join family counts',exact_leaf='Root/bootstrap/fixed/output joins, root leaf Substring, bootstrap parent candidates and unknown/failure parent candidates still join their explicit output/input extents. Existing catalogue structures/FileInfo136 never sponsor these referents.'),dict(id='Association',exact_leaf='Selected source/binary/ABI/pool/provider/currentdrive and immutable final profile remain conditional; R13 root/union/prices receive no credit.' )],no_reclaim=True,no_numeric_price_or_binding_authority=True)
result['checks']=dict(count=len(checks),all_passed=all(x['passed'] for x in checks),rows=checks)
result['input_pins']=list(pins.values())
(HERE/'closure.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='FIRST_COMPLETE_OWN_SOURCE_CHECK_PASS_NOT_GLOBAL_CLOSURE',checks=len(checks),phases=[p['evaluated_component_sum'] for p in phases],conditional_rents=[p['conditional_1024_char_rent_component']['bytes'] for p in phases],sites=len(sites),sha256=sha((HERE/'closure.json').read_bytes()))))
