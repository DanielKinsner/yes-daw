"""NEW UNRUN source authoring only; never loads target PowerShell or helpers."""
from pathlib import Path
import copy,difflib,hashlib,json,ntpath,re
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
OLD=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1'
NEW=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
if NEW.exists():raise ValueError('preserve every previous source attempt')
NEW.mkdir(parents=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
checks=[]
def ck(name,a,b):
 checks.append(dict(name=name,actual=a,expected=b,passed=a==b))
 if a!=b:raise ValueError(name)
ck('predecessor manifest',sha(OLD/'source-manifest.json'),'52E3482AF966C562CBFB5119F145B79BC762C3EA8B17D050DC9CFF96CDF54F4D')
ck('predecessor runner',sha(OLD/'run-clock.ps1'),'0E41A01150AE5A588D7D42176F6A6C7A6D922F8CE3AE69AD474FDA26E05C7DA9')
for name in ['clock_stage.cpp','source-costs.h']:(NEW/name).write_bytes((OLD/name).read_bytes())
original=(OLD/'run-clock.ps1').read_text()
runner=original
edits=[]
def replace(old,new,count=1):
 global runner
 ck('exact edit anchor '+old[:72],runner.count(old),count)
 runner=runner.replace(old,new)
 edits.append(dict(old=old,new=new,occurrences=count))
replace('return @{bytes=$taskSeen;sha256=[Convert]::ToHexString($taskHash.GetHashAndReset())}',
        'return [System.Tuple[object,object]]::new($taskSeen,$taskHash.GetHashAndReset())')
helpers='''# Internal hash carrier: Item1 actual byte count, Item2 fresh SHA256 byte[32].
# Caller projections format hex; no digest or Tuple enters a wire graph.
function Get-TaskHashHex($Hash){return [Convert]::ToHexString([byte[]]$Hash.Item2)}
function Test-TaskHashValue([byte[]]$Digest,$Expected){
 if($Digest.Length -ne 32){throw 'INTERNAL_SHA256_DIGEST_EXTENT'}
 if($Expected -is [string]){
  if($Expected.Length -ne 64){return $false}
  $taskAlphabet='0123456789ABCDEF'
  for($taskI=0;$taskI -lt 32;$taskI++){
   $taskByte=[int]$Digest[$taskI]
   if($Expected[2*$taskI] -cne $taskAlphabet[$taskByte -shr 4] -or $Expected[2*$taskI+1] -cne $taskAlphabet[$taskByte -band 15]){return $false}
  }
  return $true
 }
 if($Expected -is [byte[]]){
  if($Expected.Length -ne 32){return $false}
  for($taskI=0;$taskI -lt 32;$taskI++){if($Digest[$taskI] -ne $Expected[$taskI]){return $false}}
  return $true
 }
 throw 'INTERNAL_HASH_EXPECTED_KIND'
}
function Assert-TaskFreshHashIdentity([string]$Path,$ExpectedBytes,$ExpectedHash,$Observed){
 if($ExpectedBytes -ne $Observed.Item1 -or -not (Test-TaskHashValue ([byte[]]$Observed.Item2) $ExpectedHash)){
  $taskExpectedHex=if($ExpectedHash -is [string]){$ExpectedHash}else{[Convert]::ToHexString([byte[]]$ExpectedHash)}
  $taskObservedHex=Get-TaskHashHex $Observed
  Reject-TaskGate 'BEFORE_AFTER_COMPLETE_INPUT_IDENTITY' @{path=$Path;expected_bytes=$ExpectedBytes;observed_bytes=$Observed.Item1;expected_hash=$taskExpectedHex;observed_hash=$taskObservedHex}
 }
}
'''
replace('function Assert-TaskPollAdmission(',helpers+'function Assert-TaskPollAdmission(')
replace("Assert-TaskIdentity @{path=$Executable;bytes=$ExpectedBinary['bytes'];sha256=$ExpectedBinary['sha256']} (Get-TaskHash $Executable 33554432)",
        'Assert-TaskFreshHashIdentity $Executable $ExpectedBinary.Item1 $ExpectedBinary.Item2 (Get-TaskHash $Executable 33554432)')
replace("if($taskMh['sha256'] -cne $taskExpectedManifest)",
        'if(-not (Test-TaskHashValue ([byte[]]$taskMh.Item2) $taskExpectedManifest))')
replace("$taskExpected=@{path=(Join-Path $taskBase $taskPin['path']);bytes=$taskPin['bytes'];sha256=$taskPin['sha256']};Assert-TaskIdentity $taskExpected (Get-TaskHash $taskExpected['path'] $taskExpected['bytes'])",
        "$taskExpectedPath=Join-Path $taskBase $taskPin['path'];$taskExpectedBytes=$taskPin['bytes'];$taskExpectedHash=$taskPin['sha256'];Assert-TaskFreshHashIdentity $taskExpectedPath $taskExpectedBytes $taskExpectedHash (Get-TaskHash $taskExpectedPath $taskExpectedBytes)")
replace("$taskExpected=@{path=$taskPath;bytes=$taskPin['bytes'];sha256=$taskPin['sha256']};Assert-TaskIdentity $taskExpected (Get-TaskHash $taskPath $taskExpected['bytes'])",
        "$taskExpectedBytes=$taskPin['bytes'];$taskExpectedHash=$taskPin['sha256'];Assert-TaskFreshHashIdentity $taskPath $taskExpectedBytes $taskExpectedHash (Get-TaskHash $taskPath $taskExpectedBytes)")
# Only these exact variable spellings denote hash results in their audited
# functions. Other taskExpected/module/argv and descriptor maps are untouched.
replace("$taskH['bytes']",'$taskH.Item1',9)
replace("$taskH['sha256']",'(Get-TaskHashHex $taskH)',4)
replace("Assert-TaskIdentity $taskIdentity (Get-TaskHash $taskMutantBundle 98009)",
        "Assert-TaskFreshHashIdentity $taskIdentity['path'] $taskIdentity['bytes'] $taskIdentity['sha256'] (Get-TaskHash $taskMutantBundle 98009)")
replace('Assert-TaskIdentity $taskIdentity $taskCorrupt',
        "Assert-TaskFreshHashIdentity $taskIdentity['path'] $taskIdentity['bytes'] $taskIdentity['sha256'] $taskCorrupt")
replace("$taskCorrupt['bytes']",'$taskCorrupt.Item1')
replace("$taskCorrupt['sha256']",'(Get-TaskHashHex $taskCorrupt)')
replace("$taskBundleHash['sha256']",'(Get-TaskHashHex $taskBundleHash)')
replace("$taskInitialBinary['bytes']",'$taskInitialBinary.Item1',2)
replace("Assert-TaskIdentity @{path=$taskGuiPath;bytes=$taskInitialBinary.Item1;sha256=$taskInitialBinary['sha256']} (Get-TaskHash $taskGuiPath 33554432)",
        'Assert-TaskFreshHashIdentity $taskGuiPath $taskInitialBinary.Item1 $taskInitialBinary.Item2 (Get-TaskHash $taskGuiPath 33554432)')
replace("$taskInitialBinary['sha256']",'(Get-TaskHashHex $taskInitialBinary)')
replace("(Get-TaskHash $taskGuiPath 33554432)['sha256']",'(Get-TaskHashHex (Get-TaskHash $taskGuiPath 33554432))')
replace("(Get-TaskHash (Join-Path $taskStageRoot 'bundle.json') 1048576)['sha256']",
        "(Get-TaskHashHex (Get-TaskHash (Join-Path $taskStageRoot 'bundle.json') 1048576))")

doc=(OLD/'README.md').read_text()
doc=doc.replace('# ClockOracle R11','# ClockOracle R12',1)
doc=doc.replace('The original nine runner controls remain byte-exact; the same tenth actual','The original nine runner control IDs/reasons/wire witnesses remain; the same tenth actual')
doc+='''
## R12 fresh hash digest carriers, source revision 1

UNRUN; all guards false, source prices and Data still undefined. Get-TaskHash
keeps every fresh path/reparse/stat/open-length/read/SHA256/EOF/finally occasion
and returns a non-enumerable Tuple<Object,Object>: actual byte count and a fresh
32-byte digest. The reused input buffer is distinct from that digest; callers
never mutate the digest. No filesystem fact/hash is cached, no Span/native helper
or SDK substitution is introduced, and every original fence remains.

Pin comparisons capture expected path/count/hash scalar values in the original
order; expected byte count remains untyped. Three new closed helpers format a
hash only for an explicit projection, compare exact uppercase hex nibbles or a
32-byte immutable baseline, and construct the unchanged identity rejection.
The map-to-map identity helper stays solely for reference/build descriptors.
All retained control/refusal/receipt/reference/build/artifact/bundle fields and
the final stdout hash remain uppercase strings with their original names/content.
No Tuple or raw digest enters serialization. The original mutation/PE controls,
partial owners, primary/secondary paths, byte/refusal caps and unknown/tilde paths
remain. New helper/caller/binder/loop/temporary-box and formatting-failure work
requires fresh complete census and all six source-prefix/resource joins.

Tuple2 binary/source/layout qualification is PENDING. Existing Tuple5 metadata is
not a Tuple2 measurement. Source request estimates are neither net managed deltas
nor live/peak/reclaim or32MiB fit. The complete canonical functional source/proof/
SDK/contract/census/review/resource/binding union and exact root/env profile remain
pending. Existing inputs are admitted only PROVISIONALLY for source preparation;
no R11/R10 prices/resources/root geometry or archive-copy authority transfers.
Independent whole functional review precedes new full census/resource closure,
immutable binding and separate root target GO. No target parser/AST/import/helper,
compiler/program/stage, GUI/device or Git/CI ran during this source authoring.
'''
old_manifest=json.loads((OLD/'source-manifest.json').read_bytes())
manifest=copy.deepcopy(old_manifest)
manifest['schema']='YESDAW_CLOCK_ORACLE_R12_FRESH_HASH_DIGEST_SOURCE_PROTOTYPE_UNBOUND'
manifest['source_authoring_go']='ROOT_APPROVED_ORDINARY_INTERNAL_FRESH_HASH_CARRIER_AND_SCALAR_IDENTITY_ARGUMENTS'
manifest['exact_source_critic']='PENDING_FRESH_WHOLE_R12_FUNCTIONAL_REVIEW_THEN_NEW_COMPLETE_CENSUS_AND_SIX_PHASE_RESOURCE_CLOSURE'
manifest['R12_hash_carrier']={
 'state':'SOURCE_ONLY_UNRUN_ALL_GUARDS_FALSE_PROVISIONAL_ADMISSION_NO_NEW_PRICE_OR_GO',
 'predecessor_source_manifest_sha256':sha(OLD/'source-manifest.json'),
 'row_type':'System.Tuple<System.Object,System.Object>',
 'cells':['actual_byte_count','fresh_SHA256_byte_array_32'],
 'expected_identity':'Captured scalar path/count/hash arguments; count untyped. Exact uppercase hex or immutable32-byte baseline comparison.',
 'wire_projection':'All original hash fields remain64-character uppercase strings; no Tuple/digest directly serialized.',
 'new_helpers':['Get-TaskHashHex','Test-TaskHashValue','Assert-TaskFreshHashIdentity'],
 'Tuple2_binary_source_layout_qualification':'PENDING; Tuple5 inventory is not Tuple2 measurement.',
 'source_request_estimate_is_not_net_measurement_or_fit':True,
 'fresh_filesystem_hash_occasion_preserved':True,'digest_mutated_by_callers':False,
 'current_admission':'Provisional existing input union for UNRUN functional source preparation only. Root settles canonical functional proof/data union before final profile/census/resource/binding authority.',
 'old_census_price_resource_transferred':False,'global_payload_fit':False,'binding_or_execution_GO':False}
manifest['controlled_lifetime']['hash_results']='Fresh two-reference Tuple owns actual count and distinct32-byte digest. Exact compare or explicit uppercase wire/refusal formatting; no hash/FS cache/reuse. Tuple2 binary/source/layout and all caller/binder/box/loop/failure capacities pending fresh proof.'
manifest['profile']['profile_source']='PROVISIONAL_EXISTING_ADMISSION_FOR_UNRUN_R12_FUNCTIONAL_SOURCE_PREPARATION; final functional union/root/env/census/resources require fresh recomposition before authority.'
base=ntpath.normpath(str(NEW));repo=ntpath.normpath(str(ROOT))
own=[ntpath.join(base,n) for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json']]
paths=[ntpath.normpath(r['path'] if ntpath.isabs(r['path']) else ntpath.join(repo,r['path'])) for r in manifest['inputs']]+own
own_max=max(map(len,own));path_max=max(map(len,paths))
doc=re.sub(r'current own5 maximum is[0-9]+ \(provisional R11 base\)',f'current own5 maximum is{own_max} (provisional R12 base)',doc)
doc=doc.replace('Full new work census must price all68 helper registrations/bodies','Full new work census must price all71 helper registrations/bodies')
manifest['R10_payload_repairs']['current_own5_path_UTF16_max']=own_max
manifest['R10_payload_repairs']['known_current_path_UTF16_max']=path_max
manifest['R10_payload_repairs']['known_current_identity_paths_with_tilde']=sum('~' in p for p in paths)
def funcs(text):
 starts=list(re.finditer(r'^function ([A-Za-z-]+)',text,re.M));main=text.index('\ntry{\n # Hash literal manifest')+1
 return {a[1]:text[a.start():starts[i+1].start() if i+1<len(starts) else main] for i,a in enumerate(starts)}
r9=funcs((ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r9-source-prototype/run-clock.ps1').read_text())
def shape(v,depth=0):
 if isinstance(v,dict):
  x=[shape(a,depth+1) for a in v.values()]
  return [1+sum(a[0] for a in x),len(v)+sum(a[1] for a in x),max([depth+1]+[a[2] for a in x]),max([len(k) for k in v]+[a[3] for a in x]+[0]),sum(a[4] for a in x)]
 if isinstance(v,list):
  x=[shape(a,depth+1) for a in v]
  return [1+sum(a[0] for a in x),sum(a[1] for a in x),max([depth+1]+[a[2] for a in x]),max([a[3] for a in x]+[0]),sum(a[4] for a in x)]
 return [1,0,depth,len(v) if isinstance(v,str) else 0,int(isinstance(v,float))]
last=None
for iteration in range(8):
 fs=funcs(runner)
 manifest['R10_payload_repairs']['helper_bodies_byte_exact_R9']=sorted(n for n in fs.keys()&r9.keys() if fs[n]==r9[n])
 manifest['R10_payload_repairs']['changed_helper_bodies']=sorted(n for n in fs.keys()&r9.keys() if fs[n]!=r9[n])
 (NEW/'run-clock.ps1').write_text(runner,encoding='utf-8',newline='\n')
 (NEW/'README.md').write_text(doc,encoding='utf-8',newline='\n')
 for row in manifest['sources']:
  p=NEW/row['path'];row.update(bytes=p.stat().st_size,lines=len(p.read_bytes().splitlines()),sha256=sha(p))
 s=shape(manifest);visits=s[0]+s[1]
 manifest['R10_payload_repairs']['raw_profiles']['bootstrap']['nodes_and_keys']=visits
 manifest['R10_payload_repairs']['raw_profiles']['bootstrap']['container_depth']=s[2]
 manifest['R10_payload_repairs']['bootstrap_observed']=dict(values=s[0],keys=s[1],max_string_units=s[3],container_depth=s[2],floating_values=s[4],nodes_and_keys=visits)
 runner=re.sub(r'Assert-TaskRawJsonBytes \$taskBootstrap [0-9]+ 512 [0-9]+',f'Assert-TaskRawJsonBytes $taskBootstrap {visits} 512 {s[2]}',runner)
 doc=re.sub(r'Final source-profile arithmetic: bootstrap.*?caller uses [0-9]+/512/[0-9]+\.',f'Final source-profile arithmetic: bootstrap {s[0]} values + {s[1]} keys\n= {visits} node/key visits; maximum string {s[3]} UTF16 units,\ncontainer depth {s[2]}, no floating JSON values. The exact PS byte-helper\ncaller uses {visits}/512/{s[2]}.',doc,flags=re.S)
 raw=(json.dumps(manifest,indent=2)+'\n').encode()
 state=(raw,runner,doc)
 if state==last:break
 last=state
else:raise ValueError('metadata source fixed point did not settle')
(NEW/'source-manifest.json').write_bytes(raw)
ck('71 complete source helpers',len(funcs(runner)),71)
ck('all16 original hash physical call sites retained',runner.count('Get-TaskHash '),original.count('Get-TaskHash '))
ck('map identity only two descriptor callers',runner.count('Assert-TaskIdentity '),2)
ck('fresh identity six audited caller lines',runner.count('Assert-TaskFreshHashIdentity '),6)
ck('no old hash-result field reads',len(re.findall(r'\$(?:taskH|taskMh|taskInitialBinary|taskBinaryHash|taskBundleHash|taskCorrupt|ExpectedBinary)\[\'(?:bytes|sha256)\'\]',runner)),0)
ck('descriptor map helper byteexact',funcs(runner)['Assert-TaskIdentity'],funcs(original)['Assert-TaskIdentity'])
ck('all guards false',list(manifest['guards'].values()),[False]*len(manifest['guards']))
ck('same provisional input identities',manifest['inputs'],old_manifest['inputs'])
for key in ['references','module_controls','control_files','compiler_final_outputs','dense_Q_entries','fraction_residues']:
 ck('same domain '+key,manifest[key],old_manifest[key])
for name in ['clock_stage.cpp','source-costs.h']:ck('byteexact '+name,sha(NEW/name),sha(OLD/name))
ck('bootstrap caller',re.search(r'Assert-TaskRawJsonBytes \$taskBootstrap ([0-9]+) 512 ([0-9]+)',runner).groups(),(str(visits),str(s[2])))
ck('README bootstrap',re.search(r'bootstrap ([0-9]+) values \+ ([0-9]+) keys\n= ([0-9]+) node/key visits',doc).groups(),(str(s[0]),str(s[1]),str(visits)))
ck('exact data shape',shape(manifest),s)
ck('own source cap',sum(p.stat().st_size for p in NEW.iterdir())<=1048576,True)
for row in manifest['sources']:ck('source row '+row['path'],sha(NEW/row['path']),row['sha256'])
for p in NEW.iterdir():ck('UTF8LF '+p.name,b'\r' not in p.read_bytes() and not p.read_bytes().startswith(b'\xef\xbb\xbf'),True)
diff=''.join(difflib.unified_diff(original.splitlines(True),runner.splitlines(True),fromfile='sealed-R11/run-clock.ps1',tofile='UNRUN-R12/run-clock.ps1'))
(OUT/'runner.diff').write_text(diff,encoding='utf-8',newline='\n')
packet=dict(schema='R12_UNRUN_HASH_CARRIER_SOURCE_CORRESPONDENCE_R1',status='SOURCE_ONLY_UNRUN_REQUIRES_WHOLE_REVIEW_COMPLETE_NEW_CENSUS_RESOURCE_FIT_AND_BINDING',predecessor_manifest=sha(OLD/'source-manifest.json'),manifest_sha256=sha(NEW/'source-manifest.json'),source_rows=manifest['sources'],bootstrap_shape=manifest['R10_payload_repairs']['bootstrap_observed'],own_path_max=own_max,current_identity_max=path_max,helper_count=len(fs),new_helpers=manifest['R12_hash_carrier']['new_helpers'],edits=edits,provisional_input_count=len(manifest['inputs']),Tuple2_qualification='PENDING; no Tuple5-to-Tuple2 measurement transfer',checks=checks,no_target_parser_AST_import_helper_compile_program_stage_run=True)
(OUT/'source-correspondence.json').write_text(json.dumps(packet,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({k:packet[k] for k in ['status','manifest_sha256','source_rows','bootstrap_shape','own_path_max','current_identity_max','helper_count']},indent=2))
