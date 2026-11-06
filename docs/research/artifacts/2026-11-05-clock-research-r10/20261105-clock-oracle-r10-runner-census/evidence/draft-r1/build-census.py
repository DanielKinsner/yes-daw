from pathlib import Path
import json, hashlib, re, copy, ntpath, os, urllib.request

R = Path(__file__).resolve().parents[3]
O = Path(__file__).resolve().parent.parent
P = R / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r10-source-prototype'
M = json.loads((P / 'source-manifest.json').read_text(encoding='utf-8'))
old = json.loads((R / 'build-ci/20261105-clock-oracle-r9-runner-census/census.json').read_text(encoding='utf-8'))
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest().upper()
I, NS, EG, DG, W, DT, ET, CW = 1220, 31, 1123, 720, 1039, 6827, 14757, 16835
H, B, NL, G, RS, EP, first = 9936, 73432, 17214, 34050, 13673, 47723, 1297
profile = dict(inputs=I, input_bytes=460232026, input_chunks=7849, namespaces=NS,
 entries=1092, files=1045, namespace_guard_rows=EG, namespace_directory_upper=DG,
 fence_identities=1225, fence_chunks=7862, fences=8, hash_calls=H, hash_bodies=B,
 resolve_calls=RS, nolink_outside_hash_walk_copy=447, walker_calls=W,
 walker_directory_iterations_upper=DT, walker_appended_rows_upper=ET,
 walker_canonical_calls_upper=CW, all_NoLink_calls_upper=NL,
 all_canonical_calls_upper=G, all_entry_path_calls_upper=EP,
 completed_entry_first_builds_upper=1294, additional_unpublished_failure_builds_upper=3,
 first_build_attempts_upper=first, stage_calls=784, normal_FileSet_calls=256,
 normal_FileSet_N_M=8615, all_FileSet_calls_with_controls=259, FileSet_N=8783,
 FileSet_M=8784, raw_byte_calls=6, raw_byte_units=4789990, raw_string_calls=16,
 raw_string_units=131072, normal_SDK_calls=19, byte_boundary_arrays_total=61)

def en(d, e):
 return 56 + 4725*d + (204+64*512)*e + 129+64*512-22

ns = NS*(56+129+64*512-22) + 4725*DG + (204+64*512)*EG
walk_before = 784*en(1,7)+8*ns+2*en(79,78)+2*en(61,60)+2*en(1,4)+en(1,1)
c = dict(old['composition'])
c.update(fixedBootstrapDefinitions=48036, fixedMainCatchAndSourceCallers=129300,
 completeGetHashHelpers=295*H+65*B, remainingNoLinkHelpers=7254*NL,
 allResolveHelpers=(64*512+135)*RS,
 allDirectoryWalkerHelpers=walk_before-4570*DT-4*CW,
 pinsOwnRows=8*(87+50*NS+38*EG+63*4+44*I+NS*(4+I)),
 FileSetNormalClosedSlots=256*100+161*8615+64*512*2*8615,
 twoUnexpectedFileSetRefusalReplacementSurpluses=2*(201+65726*1225),
 twoOwnedCopyHelpers=2*285+70*514,
 allRecursiveVisitorPasses=31586+4*1947224+5*135546922+7193129,
 twoReadJsonOwnRowsExcludingDependencyShape=688,
 dependencyOtherOwnRows=384+64*512+1221*(69+64*512),
 allByteGrammarAndCompactionHelpers=587*4789990+138*6,
 allStringRawHelpers=512*131072+106*16,
 allCanonicalHelperOwnRows=76*G,
 allCatalogueEntryPathHelpers=71*EP+(9529-71)*first,
 initializePathCatalogue=99285)
c.pop('dependencyShapeAll4MiBUnits')
c.pop('allRawJsonLexicalHelpers')
renames = dict(completeGetHashHelpers='allHashOwnRowsExcludingNoLink',
 remainingNoLinkHelpers='allNoLinkOwnRowsExcludingCanonical',
 allResolveHelpers='allResolveOwnRowsExcludingEntryPath',
 allDirectoryWalkerHelpers='allWalkerOwnRowsExcludingNoLinkAndCanonical',
 twoOwnedCopyHelpers='twoCopyOwnRowsExcludingNoLink',
 twoReadJsonOwnRowsExcludingDependencyShape='twoReadJsonOwnRows')
c = {renames.get(k,k):v for k,v in c.items()}
total = sum(c.values())
assert len(c)==51 and total==5543964320
graphs = copy.deepcopy(old['graphs'])
graphs['bundle'].update(fields=21,maps=1294,keys=5251,strings=3938,integers=1302,
 key_units=46364,string_units=829725,array_elements=1292,map_separators=3957,
 array_separators=1290,value_calls=6544,nodes_keys=11795,visitor=135546922,
 clone_dictionary_prime_capacity_sum=9074)
graphs['receipt']['clone_dictionary_prime_capacity_sum']=362

groups = {
 'Context':dict(entry_return=2,nonnull=3,equalities=18,and_ops=3),
 'AddEntry_true':dict(entry_params=3,local=2,guard=15,entry_map=17,rows_Add=10,
  branch=2,inputs_Add=12,return_=1),
 'AddEntry_false':dict(entry_params=3,local=2,guard=15,entry_map=17,rows_Add=10,
  branch=2,alias_Add=12,drive_guard=17,Replace=14,contains_guard=13,
  separator_alias_Add=12,return_=1),
 'String_hit':dict(entry_param=2,local=5,contains_branch=9,return_cell=5),
 'String_miss':dict(entry_param=2,local=5,contains_branch=9,count_guard=14,
  Add=9,return_Path=2),
 'Entry_hit':dict(entry_param=2,nonnull_guard=7,return_cell=5),
 'Entry_tilde':dict(entry_guard=9,name_raw=7,repo_guard=14,Join=8,raw_tilde=12,
  GetFullPath=7,canonical_tilde=12,return_=2),
 'Entry_first':dict(entry_guard=9,name_raw=7,repo_guard=14,Join=8,raw_tilde=12,
  GetFullPath=7,canonical_tilde=12,pool_call=44,parent_array_locals=12,
  while_fixed=8,bodies65=65*143,aliases_local=5,foreach_fixed=7,
  alias_rows2=72,field_writes=15,return_=2),
 'Entry_parent_body':dict(condition_controls=8,pool=44,array_store=5,increment=3,
  depth_branch=4,link_contains=12,link_miss=67),
 'Entry_link_miss':dict(GetDirectoryName=7,nonnull=2,pool=44,Add=12,At_write=2),
 'Entry_alias_row':dict(foreach=5,contains=10,count_guard=12,Add=9),
 'Init_fixed':dict(entry=1,maximum=36,guards=28,map=95,inputs_foreach=6,
  own_foreach=3,manifest_caller=3,refs_foreach=6,compiler_foreach=6,
  controls_foreach=6,stage_foreach=6,final_guard=12,return_=1),
 'Canonical_own':dict(entry_param=2,context=28,tilde_guard=12,and_branch=2,
  alias_contains=12,strings_contains=12,return_cell=8),
 'Resolve_own':dict(entry_param=2,string_fixed_caller=53,rooted_branch=8,
  length_guard=18,context_contains=41,max_private9_fallback13=13),
 'NoLink_tail':dict(context=28,contains=11,and_branch=2,max_hit8_old7=8),
 'ReadJson_common':dict(entry_params=4,Spend=20,codec=19,bounded_read=201,
  first_Maximum=4,AddJsonVisit=24,second_Maximum=4,prefix_GetString=19,
  null_write=2,SDK=12,return_=1,Maximum_conversion=1),
 'Dependency_fixed':dict(entry_Build=2,report_canonical_caller=9,locals=4,
  descriptor_scan=50,match_guard=4,ReadJson_caller=5,root_type=22,Data_local=5,
  Data_type=22,Source_string_fixed=56,Source_comparison=16,optional_modules=109,
  Includes_guard=55,foreach_fixed=6,return_map=19)
}
expected = dict(Context=26,AddEntry_true=62,AddEntry_false=118,String_hit=21,
 String_miss=41,Entry_hit=14,Entry_tilde=71,Entry_first=9529,
 Entry_parent_body=143,Entry_link_miss=67,Entry_alias_row=36,Init_fixed=209,
 Canonical_own=76,Resolve_own=135,NoLink_tail=49,ReadJson_common=311,
 Dependency_fixed=384)
for k,v in expected.items():
 assert sum(groups[k].values())==v,(k,sum(groups[k].values()),v)
assert 209+73*1220+133*4+118+136*(56+3+4)+133*6==99285
assert 2*311+24+19+17+6==688

src = (P/'run-clock.ps1').read_text(encoding='utf-8')
starts = list(re.finditer(r'^function ([A-Za-z-]+)',src,re.M))
locations = {z[1]:[src[:z.start()].count('\n')+1,
 (src[:starts[i+1].start()].count('\n') if i+1<len(starts) else 863)]
 for i,z in enumerate(starts)}
rows = [copy.deepcopy(z) for z in old['helper_rows']
 if z['name']!='Assert-TaskDependencyJsonShape']
replace = {
 'Assert-NoLink':dict(own_upper=7254,excluded='Canonical body',
  formula='4570-7+3+64*(49-7)=7254'),
 'Get-TaskEntries':dict(own_formula='old factored walker -4570*D -4*CanonicalCalls',
  excluded='NoLink and Canonical bodies'),
 'Resolve-TaskInput':dict(own_formula='64*NameUnits+135',excluded='EntryPath body'),
 'Read-TaskJson':dict(own_pair=688,formula='2*311+24+19+17+6',excluded='byte helper'),
 'Assert-TaskDependencies':dict(own_formula='384+64*SourceUnits+sum(69+64*IncludeUnits)',
  excluded='ReadJson/Resolve/ReportCanonical bodies',
  historical421='Not borrowed: old unchanged fixed groups re-expand321; R10prefix/parameter adds63. R9upper/history preserved.')
}
for z in rows:
 z['historical_R9_lines']=z.pop('lines',None)
 z['lines']=locations[z['name']]
 z['price_status']='R10_PROPOSAL_REQUIRES_FRESH_NUMERIC_REVIEW'
 z.pop('historical_line_numbers',None)
 if z['name'] in replace:
  keep={k:z[k] for k in ['name','lines','historical_R9_lines','price_status']}
  keep.update(replace[z['name']]);z.clear();z.update(keep)
 elif z['name']=='Get-TaskHash':
  z['flattened_own']='295+65*chunks; oneNoLink separately composed'
 elif z['name']=='Copy-TaskOwned':
  z['flattened_own']='285+70*chunks; twoNoLink separately composed'
new_prices={'Test-TaskPathCatalogueContext':26,'Add-TaskPathCatalogueEntry':'62/118',
 'Initialize-TaskPathCatalogue':99285,'Get-TaskCatalogueString':'21/41',
 'Get-TaskCatalogueEntryPath':'14/71/9529','Get-TaskCanonicalPath':'76+Entry',
 'Assert-TaskRawJsonBytes':'587*L+138'}
for n,v in new_prices.items():
 rows.append(dict(name=n,lines=locations[n],upper=v,
  price_status='NEW_R10_PROPOSAL_REQUIRES_FRESH_NUMERIC_REVIEW',
  source_groups='new_family_groups/byte_lexical_derivation'))
assert len({z['name'] for z in rows})==68

base,repo=ntpath.normpath(str(P)),ntpath.normpath(str(R))
run=ntpath.join(base,'runs','r10-one-reviewed-attempt')
paths=[ntpath.normpath(z['path'] if ntpath.isabs(z['path'])
 else ntpath.join(repo,z['path'])) for z in M['inputs']]
paths +=[ntpath.join(base,k) for k in
 ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json']]
trans=paths.copy()
art=[ntpath.normpath(ntpath.join(run,'data',z['path'])) for z in M['references']]
art +=[ntpath.join(run,'build',z['name']) for z in M['compiler_final_outputs']]
art +=[ntpath.normpath(ntpath.join(run,z['path'])) for z in M['control_files']]
art +=[ntpath.join(run,'stage',k) for k in
 ['stdout.log','stderr.log','receipt.json','consumed.json']]
paths +=art+[ntpath.join(run,'stage',k) for k in
 ['bundle.json','failure-inventory.json']]
assert len(paths)==1294
pool=set();chains=0;depth=0
for x in paths:
 d=0
 while x:
  pool.add(x);chains+=1;d+=1;n=ntpath.dirname(x)
  if n==x:break
  x=n
 depth=max(depth,d)
strings=['ClockOracle','CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED',
 'A'*64,'OPAQUE',M['predecessor_bundle']['path'],'A'*64]
strings +=[z for v in trans+art for z in [v,'A'*64,'A'*64]]
strings +=[z['path'] for z in M['references']]
encoded_units=sum(len(json.dumps(z,ensure_ascii=True,separators=(',',':')))-2
 for z in strings)
assert len(strings)==3938 and encoded_units==349198
bundle_profile=(46364+2*5251+encoded_units+2*3938+20*1302+5*8+
 2*1294+4+5251+3957+1290+1)
assert bundle_profile==453111
primary=[]
sources=[('StringBuffer.cs','https://raw.githubusercontent.com/JamesNK/Newtonsoft.Json/4e13299d4b0ec96bd4df9954ef646bd2d1b5bf2a/Src/Newtonsoft.Json/Utilities/StringBuffer.cs'),
 ('JsonTextWriter.cs','https://raw.githubusercontent.com/JamesNK/Newtonsoft.Json/4e13299d4b0ec96bd4df9954ef646bd2d1b5bf2a/Src/Newtonsoft.Json/JsonTextWriter.cs'),
 ('Compiler.cs','https://raw.githubusercontent.com/PowerShell/PowerShell/ea554a8f9c6085f54fb2828f7ea286c65c35599f/src/System.Management.Automation/engine/parser/Compiler.cs')]
for name,url in sources:
 raw=urllib.request.urlopen(url,timeout=30).read()
 primary.append(dict(name=name,url=url,bytes=len(raw),
  sha256=hashlib.sha256(raw).hexdigest().upper(),
  qualification='source association only; no binary equivalence/runtime credit'))
bad=[]
for z in M['inputs']:
 p=Path(z['path']);p=p if p.is_absolute() else R/p
 if p.stat().st_size!=z['bytes'] or sha(p)!=z['sha256']:bad.append(z['path'])
assert not bad
ns_actual=[]
for z in M['closed_directory_inputs']:
 p=Path(z['path']);p=p if p.is_absolute() else R/p
 rr=[];ff=[];links=[]
 def visit(q):
  for e in os.scandir(q):
   rel=Path(e.path).relative_to(p).as_posix();rr.append(rel)
   if e.is_symlink() or e.stat(follow_symlinks=False).st_file_attributes&0x400:
    links.append(rel)
   elif e.is_dir(follow_symlinks=False):
    if z['recursive']:visit(Path(e.path))
   else:ff.append(rel)
 visit(p)
 assert len(rr)==z['entry_count'] and sorted(ff)==sorted(z['files']) and not links
 ns_actual.append((len(rr),len(ff)))
assert [len(ns_actual),sum(v[0] for v in ns_actual),sum(v[1] for v in ns_actual)]==[31,1092,1045]

payload=copy.deepcopy(old['controlled_payload'])
payload['status']='R10_COMPLETE_FAMILY_CONDITIONAL_WORKSHEET_GLOBAL_JOIN_HOLD'
payload['known_normal_input_byte_requests']['bootstrap']=497616
payload['known_fixed_array_requests']['three_literal_byte_controls']=61
payload['known_requested_byte_subtotal']=6647190
payload['decoded_text'].update(normal_units_sum=4921001,normal_storage_bytes=9842012,
 normal_storage_equation='2*(497616+98009+4194304+2*65536)+2*5=9842012; compaction may lower actual, not allocation-free.')
payload['reader_witnesses']['R10_inside_escape']='Compaction removes R9 whitespace pacing for large inputs, but4MiB inside-string mixed escape/ASCII can retain near8MiB wire text and many512-unit parsed strings. Child slices keep original bounded string admission. No fixed32768 cap or small decoded-string theorem.'
payload['serialization']['map_clone_capacity_cells']=dict(marker=7,receipts_four=1448,
 bundle=9074,failure=553,total_each_of_entry_and_bucket_arrays=11082)
payload['serialization']['map_clone_storage']='11082*(4+dictionary_entry_stride), plus explicit object fields/headers if in the qualified representation. Expected x64entry24 is CONDITIONAL pending separate public allocation/ABI probe. No32MiBfit.'
payload['serialization']['StringBuilder']='Closed strings<=512/integers<=20 imply append spans<=3072chars. Append-only chunks max8000 give retained chunk capacity<=M+8000 perdocument; retain finalToString,LFconcat,UTF8bytes. Source upper pending review, no free credit.'
payload['serialization']['writer_buffers']='Nullpool JsonTextWriter beginschar35. JavaScriptUtils requests<=512+6=518 and strictly grows capacity; retain every old array. Perwriter2*sum(1..518)=268842bytes,seven1881894. Also3*128bool escape flags and writer/token/property fields. Dates/base64/custom values not admitted. Source/binary association pending.'
payload['json_reader']['R10_candidate_source_bound']={
 'status':'DERIVED_SOURCE_UPPER_NEEDS_FRESH_REVIEW_NOT_FIT',
 'growth':'Initial1024. Strict EnsureChars relative<=4; nonappend remaining+required+1<=6, so no nonappend replacement. Direct nonappend reads are at consumed boundary. Append max(2C,used+required+1)=2C for C>=1024/required<=5; old arrays remain charged.',
 'uncompacted':'C<=nextPow2(max(1024,L+6)); retained array bytes<4*C. Child sumL<=131072 over16parses gives8*(131072+16*1030)=1180416bytes.',
 'compacted':'String/property/number starts shift in last10percent. String wire span<=6S+2, totalnonstring run<=7N. U=6S+2+7N+6. C>=16U ensures remaining after nonshift>C/16>=U since0.1>=1/16; no append growth before another shift. C<=min(nextPow2(max(1024,L+6)),nextPow2(max(1024,16U))). This is not an arbitrary32768 cap.',
 'large_capacity_upper':[524288,131072,524288],
 'large_requested_bytes':4712448,'children_requested_bytes':1180416,
 'all_reader_requested_bytes':5892864,
 'StringBuffer':'ReadStringIntoBuffer resetsPosition. EnsureBuffer initializes1024; each decodedstring<=512 so no growth.19arrays*2048=38912bytes; no return/null physicalfree.'}
payload['path_catalogue']={
 'guard_slots':dict(rows=1294,aliases=2662,string_keys=84110,parent_links=84110,
  parent_ref_arrays=1294,per_array_capacity=65),
 'conditional_tight_profile':dict(repo=repo,base=base,new_run_root_proposal=run,
  selected_or_executed=False,identities=1294,unique_strings=len(pool),
  pooled_UTF16_units=sum(len(z.encode('utf-16-le'))//2 for z in pool),
  chain_visits=chains,max_depth=depth,parent_arrays_if_ref8=1294*65*8,
  bundle_encoded_string_units=encoded_units,bundle_byte_upper=bundle_profile,
  conditions='Exact externally checked argv/no tilde/native normalization agrees with lexical Windows forms. Recompute for eventual bound-source identities/GO, not silently reuse this proposal.'),
 'owners':'One10-field catalogue map,1294five-field maps,List<object>(1294),inputs/alias/string/link dictionaries and everygrowth/rehash backing,65-reference array percompletedentry,partialfailed arrays/strings/aliases and3unpublishedfailures; all initial normalization/parent Substrings beforepoollookup and every originalfallback. Wholecatalogue retained.',
 'growth':'DictGrow(n)=sum3,7,17,37,89,197,431,919,1931,4049,8419,17519,36353,75431,156437... until>=n, plusatmostone nonrandomizedstring rehash<=finalcapacity. Buckets int4/selectedentrystride. CatalogueRowsList exactcapacity1294/noresize. Scalar fields/objectmetadata separately qualified.',
 'fresh_native_outputs':'Directoryiterator Current still createsFullName eachoccasion; GetFileName and namespaceGetRelativePath/Replace still allocate. Count every such string, path/native/pooled capacity and first/finally/footer failure prefix; no metadata cache or GC credit.'}
payload['digest_and_original_map_owners']={
 'digest_calls':9946,'equation':'9936IncrementalHash+7bootstrap+3capturedSHA=9946; each32byte digest/64UTF16hex, plusmutation2/4hex andmanifestcase.',
 'original_hash_maps':9936,'expected_three_field_maps':9792,
 'literal_source':'Compiler.BuildHashtable emits Hashtable(fieldcount,OrdinalIgnoreCase); ordered emits OrderedDictionary(fieldcount,comparer). H(k)=GetPrime(floor(k/.72f)) after Initial3 gate, NOT genericDictionary.GetPrime(k). H2=3,H3=7,H5=7,H10=17. Conditional bucketstride24: hash2field buckets715392bytes;9792expected3field buckets1645056;1294catalogue5field buckets217392. Fields/boxedscalarvalues/orderedarrays/PSCustomObject conversion separate.',
 'ABI':'Source dictionary uintHash/intNext/keyRef/valueRef suggests24-byte element;x64Hashtable keyRef/valueRef/hashColl suggests24. Ref8/int4/char2/long8/bool1 all CONDITIONAL until selected public-purpose profile qualifies. No privatefield/env hack.'}
payload['source_scalar_reference_arrays']['fixed']+=';68registrations,catalogue10fieldmap/1294entrymaps,byte-control20/20/21 arrays andbyte stackfamilies.'
payload['source_scalar_reference_arrays']['variable_extent']='R10namespace31/1092rows1045files,FileSet259/N8783/M8784,inputs1220/transitive1225. All original literal/typed/binder/ToArray/pipeline/concat sites retained, plus catalogue65ref arrays and alias-pair arrays. Poll directions2 and stage names6 remain fresh source constructions.'
payload['environment']['qualification_plan']='Separate RootGO launcher constructs explicit fresh nonsecret env, never host clone. Pinned8values plus source-qualified PSModulePath/startup additions; startup/profiling hooks absentBEFORElaunch. Purpose pinned-pwsh profile can self-assert public aggregate entry/unit/nativeblock counts and whitelist only without values/hashes/privatefields. RecheckatGO; no existing env was enumerated, no profile selected/executed here.'
payload['representation']['ABI_hold']='Separate bounded C# publicallocation/EnsureCapacity probe may qualify selected refs/array/string alignment/dictionary/Hashtable/OrderedDictionary coefficients. GC.GetAllocatedBytesForCurrentThread deltas are request/profile evidence only, never live/reclaim/RSS. Object-header inclusion remains explicit qualified representation, not silently zero.'
subtotal=dict(known_bytes=6647190,decoded_wire=9842012,child_slices=262276,
 reader=5892864,StringBuffer=38912,writer_arrays=1881894,chunks=2924376,
 ToString=2812390,LF_concat=2812404,parent_arrays_ref8=672880)
assert sum(subtotal.values())==33787198
payload['R10_guard_only_subtotal']=dict(terms=subtotal,sum=33787198,
 meaning='Conservative no-reclaim request UPPER subtotal, not a coexisting lower. Above33554432 before otherfamilies, so cannot certifyfit; does NOT establishNOFIT. Proposed profile bundle453111 lowers byte/chunk/string upper by7*(1048576-453111)=4168255; remaining owners/phase/error joins stillrequired.')
payload['phase_join_frontier']={
 'status':'PRECISE_CONDITIONAL_HOLD_NOT_NUMERIC_GLOBAL_UPPER',
 'cases':['before-created bootstrap/hash/decode/catalogue/index/fence failure',
  'marker/firstreceipt/precompile failure','compiler/cleanup/fence/outputinventory/dependency failure',
  'producer/control/reference failure','verifier/control/sourcefence/finalreceipt failure',
  'late bundle/preflight/fixedpoint/encode/write/hash failure'],
 'constraint':'Retain all earlier requests withoutGC, add actual footer/inventory prefixes and mandatorychildfinallyfences. A maximal badchildslice does not reach finalbundle. Do not conflate4receipts+bundle+everymaximal faileddecode as observedtrace. CPPsuccessfulsource has normal JSON/controlstdout and catch-only boundedstderr; nativebinaryassociation/outputextents needqualification beforetightercaseuse.',
 'unclosed_coefficients':['PS typedparameter/ToArray/pipeline conversion multiplicities;PSObject/PSNoteProperty/backing/layout for listed sourceDTOsites',
  'JToken/LineInfo/JProperty/parsedOrderedHashtable child/backing capacities at bootstrap15978/pred2420/dep2698/control13/summary25 incl20digitBigInteger and strings',
  'nativeGetFullPath/GetRelativePath/iterator/FileName/ArrayPool capacities/unknownprefix ownership underexactargv profile; Current freshpermetadataoccasion',
  'allreached DecoderFallback/JSONPath/Win32/methodwrapper/ErrorRecord/cleanup/localizedresource message copies withsourceprefix/firstslot/no-reclaim',
  'selected ABI/objectmetadata and3inheritednativeenv/startup captures'],
 'next_supported_work':'Qualify small public ABI coefficients; source-close literal/binder/PSCustomObject and JToken/error capacities;join6finite phases underexactnonsecret argv/env. Ifjoinedupper stillmisses32MiB, next narrow sourcecandidate isapprovedordinal pureGetFileName andnamespaceRelativePath/Replace stringreuse preservingcase/context/allfreshFS/reparse/hash checks. DoesnotremoveiteratorCurrent;deriveactualrequestreduction beforeadoption. No capraise/inputdomainrestriction.'}
payload['conclusion']='HOLD:no global32MiBfit/no source-proven R10coexistingviolation/no newlimit/binding/GO. Scalarproposalreviewable separately. No GC/null/Clear/scope/Dispose/ReturnBuffer credit; upper never relabelled actual.'
payload['json_reader']['input_lengths']='R10bootstrap497616,pred98009,dep4194304 originalbytecaps;compactprefix lengths<=those. Childslice total131072 over16SDKcalls.3byteboundarycontrols do not reachSDK.'
payload['json_reader']['finite_loose_upper']='Historical loose request bound is superseded as the current candidate by R10_candidate_source_bound, still pending independent numeric review. No universalcapacity certified.'
payload['path_strings']={
 'known_source':'Canonical/parent catalogue reuses strings only. NativeDirectoryiterator Current, GetFileName, namespaceGetRelativePath/Replace, rawJoin/case/unknownGetFullPath remain new request families.',
 'registry':'See path_catalogue conditional fixed-root profile and digest/originalcontainer/array rows. OriginalR9parent955? counts are historical and NOT transferred toR10.',
 'conditional_equation':'AllKnownFirstCanonAndParentRequests + AllFreshIteratorCurrentAndNameRelativeStrings + Alias/Join/case strings + UnknownFirst/Finally/FooterNativePathPrefixes + selectedpooled/nativebuffers. Eachterm retained cumulatively unless explicit physicalrelease proved.',
 'global_join':'HOLD: actual6phasecounts/nativeprefix capacities/selected normalizer/ArrayPool association required; no normalizedlength<=raw512 inference for unknown/tilde inputs.'}
payload['reader_witnesses']={
 'historical_R9':'Small32771false-whitespace witness forcedchar65536 and4MiB13false candidate gave21037056. LargeR10bytecompaction removes that whitespacepacing; do not transfer either as R10lower.',
 'R10_inside_escape':payload['reader_witnesses']['R10_inside_escape']}
payload['actual_coexisting_lower']='UNESTABLISHED_FOR_R10; historicalR9lower not transferred after source repairs.'
payload['source_scalar_reference_arrays']['historical_R9_literal_registry']=payload['source_scalar_reference_arrays'].pop('literal_site_registry')
payload['source_scalar_reference_arrays']['literal_site_registry']=[dict(line=i,source=l.strip())
 for i,l in enumerate(src.splitlines(),1) if '@(' in l or re.search(r'\[(?:byte|char|object|string|long|uint64)\[\]\]::new',l)]
payload['subtotal_scope']='Cumulative upper requests for normal rawcaptures,7encodes,explicit arrays/streambuffers plus61bytecontrolarrays. Before unknown conversion/path/error/ABI terms. A failed capturedidentity prefix can use1MiB but never reachesdecoder/SDK and is a separate phase.'

packet=dict(schema='YESDAW_CLOCK_R10_RUNNER_CENSUS_V1',
 state='COMPLETE_SCALAR_PROPOSAL_CONDITIONAL_CAPACITY_LIFETIME_HOLD_SOURCE_ONLY_UNRUN',
 subject_manifest_sha256=sha(P/'source-manifest.json'),
 whole_functional_review_sha256=sha(R/'build-ci/20261105-clock-oracle-r10-independent-functional-review/review.json'),
 scalar_upper_proposal=total,new_work_limit_selected=False,combined_U_selected=False,
 binding_or_execution_GO=False,profile=profile,composition=c,graphs=graphs,
 receipt_byte_upper=23635,new_family_groups=groups,helper_rows=rows,
 helper_coverage=dict(unique_names=68,rows=len(rows),
  missing=sorted(set(locations)-{z['name'] for z in rows}),
  removed_wrapper='Assert-TaskDependencyJsonShape;formula nowatbytecaller;no zero-pricedhelper'),
 byte_lexical_derivation=dict(formula='587*L+138',original_grammar_per_input=536,
  grammar_components=dict(outer=234,string=134,hex=61,digits=38,magnitude=32,literal=37),
  string_extra='Original111+unitCount init2+UTF8branch/test max20+incrementdelta1=134;allrawskipops counted.',
  literal_extra='Original36+explicit char-to-int cast1 at bytecompare=37.',
  fixed_common=dict(entry_params=5,profile_guard=20,stack_ctor=10,locals=6,
   outer_terminal=10,final_grammar=11,byte_cap=9,GetCharCount=23),
  failure_exit_upper=44,compaction_per_input=51,
  compaction=dict(forLength=14,read_local=5,quoted_branch=2,whitespace_test=16,
   copy=5,write_increment=3,quote_test=4,quoted_assignment=2),
  compaction_fixed=21,
  proof='94+536L+max(failure44,normal51L+21)<=587L+138. No normalreturn fabricated onthrow;no unreadbyte overwritten.',
  byte_calls=6,byte_units=4789990,byte_work=2811724958,
  string_formula='UNCHANGED512L+106;16calls/sumL131072=>67110560. BadID replaces unreachable summaryslice/call.'),
 path_schedule_proof=dict(equations={
  'hash':'8*1225+136=9936','chunks':'8*7862+10536=73432',
  'resolve':'10*1220+8*31+1221+4=13673','outsideNoLink':'8*31+120+78+1=447',
  'walkers':'784+8*31+2+2+2+1=1039','D':'784+8*720+2*79+2*61+2+1=6827',
  'E':'784*7+8*1123+2*78+2*60+2*4+1=14757','walkerCanonical':'2*1039+14757=16835',
  'allNoLink':'9936+447+6827+4=17214','allCanonical':'17214+16835+1=34050',
  'EntryPath':'atmost34050+13673=47723','firstBuild':'1294completed+3failed=1297'},
  first_failure_explanation='Beforecreatedfailure has nofooter. By firstsuccessfulsourcefence,all1220inputs+own5materialized. Childfinallyfences revisit onlythosecompletedentries. Aftercreated, atmostone mainbody buildfailure,oneinventory buildfailure,onefailurereceipt buildfailure propagate; failurehelpers do not retry afterthrow. Cheaptilde/unpublished normalization mayrepeat, so everyEntrycall pays71. Whole71*47723+(9529-71)*1297=15655359 is upper,not measuredactualcalls.',
  flattening='Hashown295+65B,NoLinkown7254 excludesCanonical;walkers subtract4570D and oldcanonical7=>newcaller3 perroot/appended/overflow;Canonicalown76 excludesEntry;Copyown285+70B excludes2NoLink. Context26 folded intail/Canonical/Resolve;StringPool41/21 folded inEntry;AddEntry62/118 folded inInit. No doublebodycount.',
  namespace_guard='sum(entrycount+1)=1123;sum(entrycount+2 ifrecursive else1)=720'),
 main_and_bootstrap=dict(historical_R7_rows=old['main_error_rows_lineage']['old_rows'],
  historical_R7_sum=124455,R9_main=126055,
  R10_delta=dict(input_index=1292,include_copy=748,transitive=1156,bootstrap=5,
   Init_dispatch=1,Build_argread=1,byte_controls3=42),
  main_current=129300,bootstrap_old=48028,new_catalogue_null=2,
  net_registrations=6,bootstrap_current=48036,
  byte_control_caller='Ctorreceiver9+GetBytes literal/param/boundary excludingfusedByteparam5=14; replacesoldliteralread1,addsnullresultwrite1=>+14percall.20/20/21bytearrays retained.',
  historical_R9_definition_rows=old['fixed_bootstrap_definition_rows']),
 corrections=dict(Dependency_fixed='Old421re-expanded321;newprefix/Buildparam adds63=>384. No old unexplained100 borrowed;historicalR9upper/source untouched.',
  clone='Accepted narrowerratum362receipt/1448four;R10bundle9074=>all7=11082.',
  history='ReadJson403=>303,Copy9525=>9425,controller2137=>2136 preserved. OldfullR4fitUNKNOWN;no falseNOFIT.'),
 controlled_payload=payload,convention=old['convention'],
 composition_convention=dict(old['composition_convention'],arithmetic='51disjoint rows;integeraddition;no selectedlimit',sourceRegistration='68registrations;no bodyexecution'),
 external_primary_source_associations=primary,
 self_checks=dict(composition_rows=len(c),sum=total,new_groups_sums=expected,
  byte_component_sum=536,raw_fixed_common=94,raw_total=2878835518,
  recursive_visitors=692748221,clone_primes=11082,input_hash_mismatches=bad,
  namespaces=[31,1092,1045],target_execution=False,
  scope='ordinary source/artifactJSON/hash/inventory/text/integer and primarysource reads only;no targetparser/AST/import/helper/compiler/program/test/stage/GUI/Git/CI'))
assert sum(packet['main_and_bootstrap']['R10_delta'].values())==3245
assert not packet['helper_coverage']['missing']
assert sum(packet['byte_lexical_derivation']['grammar_components'].values())==536
assert sum(packet['byte_lexical_derivation']['fixed_common'].values())==94
(O/'census.json').write_text(json.dumps(packet,indent=2)+'\n',encoding='utf-8',newline='\n')

md='''# ClockOracle R10 complete runner census

**Scalar proposal: 5,543,964,320. Controlled payload: CONDITIONAL HOLD. SOURCE ONLY / UNRUN.**

All68source helpers/78path rows,88historical main-error rows and current deltas are covered.51disjoint source families sum by integeraddition. No R9total is a R10price; all current graph/caller/byte/catalogue work is recomposed. Fresh independent numeric review required. No selected/combined U, binding, parser/compiler/stage GO, native/Ready or32MiBfit follows. All original sources remain exact.

## Source-action proposal

| Family | Upper proposal |
| --- | ---: |
'''
md+=''.join('| `'+k+'` | '+format(v,',')+' |\n' for k,v in c.items())
md+='| **Total** | **'+format(total,',')+'** |\n'
md+='''
Source profile1220inputs/460232026bytes/7849chunks;31namespaces/1092entries/1045files/no reparse. Eight1225identity/7862chunk fences;9936hash calls/73432chunk bodies. Guards1123namespace rows/720directories;1039walkers/6827directories/14757rows. All17214NoLink/34050Canonical/13673Resolve/<=47723EntryPathcalls separately composed. Every original FS/reparse/hash occasion remains.

Opaque-member selection remains covered by dispatch: Length6/forLength14N+13/GetRuntimeType16, no extra member-name O. Explicit index/array/type/cast/default/argument/call/return/branch/loop actions remain. CLR/codec CPU/generalheap boundary unchanged; explicit payload is never free. Positive alternatives are upper envelopes, not measured traces or semantic tariffs.

Byte admission6calls/4789990originalbytes: grammar536L=234outer+134string+61hex+38digit+32magnitude+37literal. Common94+max(failure44,normalcompaction51L+21)<=587L+138. Child stringhelper16calls/sum131072units retains512L+106. UTF8count/literal conversion/decodedprefix/resultwrites and61literalbytes counted. ReadJson pair688 is fully source-grouped. Dependency fixedown384 includes descriptor/hash handoff; historical421 independently expands321, and its unexplained100 is not used for new work.

Catalogue: context26;AddEntry62/118;StringPool21/41;Entry14/71/9529;Canonicalown76;Resolveown64N+135;NoLinkown7254. First builds<=1294completed+3unpublishedfailures, everyEntrycall pays71 for repeatabletilde/uncached branches. Source groups and the3failure-prefix proof are inJSON. All partial objects/backings remain owners; no GC/retry/resetcredit.

Bundle1294maps/5251keys/3938strings/1302integers has135546922visitor upper; all11passes692748221. All56relative_path fields retained. Exact clones362receipt/1448four/bundle9074/all7=11082. Receipt23635/marker319/failure262760/onebundlecap1MiB remain. Five preflights are not fiveencodes.

## Controlled capacity/lifetime frontier

Complete named family worksheet, not a falsely numeric globalfit. Known bytes6647190,decode9842012,childslices262276,proposedreader5892864/StringBuffer38912,writerarrays1881894,chunks2924376,ToString2812390,LFconcat2812404,parentarrays672880 give conservative subtotal33787198 before otherowners. This UPPER exceeds33554432, so cannot certifyfit. It is NOT an actual coexistinglower and proves noNOFIT.

Reader proposal is profile/size dependent, not universal32768. Strict tokens bound EnsureChars relative4; append doubles/nonappend does not replace. Compacted string span6S+2/nonstring7N and last10percent shifting give the documented dyadic16-based bound. Childslices retain original bounded whitespace. All old arrays retained. New capacity derivations need fresh review; inside-string mixed escapes/ASCII still allow near8MiB wiretext and many parsedstrings.

An explicit proposed exact-root profile (not selected/executed) yields1458pool strings/186667UTF16units/maxdepth18 and bundle453111bytes, reducing byte/chunk/string upper4168255. It does not remove iteratorCurrent,filename/relative strings,DTO/conversion/backing arrays,JToken/parsedgraphs,digests,errorpayload or3inheritedenvcaptures. No null/Clear/Dispose/scope/ReturnBuffer is physicalfree. JSON names every term and conditional selected field/stride association.

Precise HOLD: sourceclose PS literal/binder/PSCustomObject layouts/copies; JToken/LineInfo/parsedmap capacities; native path/pool/errorformat payload; exact nonsecret controller startup/env profile; and six finite success/failure phase joins. A maximal badchild cannot also reach finalbundle. A separate bounded publicallocation/EnsureCapacity probe may qualify coefficients, with request deltas distinct from live/reclaim evidence. No privatefield/envhack proposed.

Next: independent numeric critic, close those typed/phase equations under exact argv/env, then reviewed wholeU/binding/storage/separateGO. If joinedupper stillmisses32MiB, next narrow sourcecandidate is ordinal approved purefilename/namespace-relative-string reuse preservingcase/context/every FS/reparse/hashcheck. It does not remove iteratorCurrent; derive actual request reduction beforeadoption. No capraise, stronger rejection, alternate SDK or budgetsplit/reset.

Verification: source/text/artifactJSON/hash/inventory/integer and exact primarysource reads only. No targetPSparser/AST/import/helper,compiler/program/test/stage,GUI/device,Git/CI or env enumeration. Threepacket files plus necessary file-only generator evidence were written; the initial oversized tool command failed before dispatch with Windows error206. No target execution occurred.
'''
(O/'census.md').write_text(md,encoding='utf-8',newline='\n')
records=[]
for name in ['build-ci/20261105-clock-oracle-r10-independent-functional-review/review.json',
 'build-ci/20261105-clock-oracle-r10-independent-functional-review/review-pins.json',
 'build-ci/20261105-clock-oracle-r9-runner-census/census.json',
 'build-ci/20261105-clock-oracle-r9-clone-capacity-erratum-r1/erratum.json',
 'build-ci/20261105-clock-oracle-r9-clone-capacity-erratum-independent-review-r1/review.json',
 'build-ci/20261105-clock-oracle-separate-research-contract/contract.md',
 'build-ci/20261105-clock-oracle-research-leaf-census-conventions.md',
 'build-ci/20261105-clock-oracle-research-bulk-leaf-clarification.md']:
 q=R/name;records.append(dict(path=name,bytes=q.stat().st_size,sha256=sha(q)))
pins=dict(schema='YESDAW_CLOCK_R10_RUNNER_CENSUS_PINS_V1',
 state='SEALED_SOURCE_ONLY_COMPLETE_SCALAR_PROPOSAL_CONDITIONAL_LIFETIME_HOLD',
 subject_five=[dict(path=q.relative_to(R).as_posix(),bytes=q.stat().st_size,
  sha256=sha(q)) for q in sorted(P.iterdir())],
 input_identity_audit=dict(count=1220,bytes=460232026,chunks=7849,mismatches=bad,all_rehashed=True),
 namespace_audit=dict(namespaces=31,entries=1092,files=1045,reparse=[],exact_file_sets=True),
 review_contract_lineage=records,new_primary_sources=primary,
 packet_outputs=[dict(path=q.relative_to(R).as_posix(),bytes=q.stat().st_size,
  sha256=sha(q)) for q in [O/'census.md',O/'census.json']],
 evidence=[dict(path=Path(__file__).relative_to(R).as_posix(),
  bytes=Path(__file__).stat().st_size,sha256=sha(Path(__file__)))],
 target_execution_or_GO=False,
 self_checks='51sourcefamilies/68helpers/newgroups/graphs/currentprofiles checked with Pythonintegers; fresh independent numericreview required.')
(O/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n',encoding='utf-8',newline='\n')
for q in [O/'census.md',O/'census.json',O/'source-pins.json']:
 print(q.name,q.stat().st_size,sha(q))
