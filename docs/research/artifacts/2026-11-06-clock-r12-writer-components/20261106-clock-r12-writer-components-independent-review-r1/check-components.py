"""Independent source/file/hash/integer checks. Never imports/runs subject helpers."""
from pathlib import Path
from hashlib import sha256
import json,re

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
checks=[];inputs={}
def check(label,condition,**data):
    checks.append(dict(label=label,pass_=bool(condition),**data))
    if not condition:raise AssertionError(label)
def pin(p):return dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=sha256(p.read_bytes()).hexdigest().upper())
def path(rel):return ROOT/rel
def read(rel):
    p=path(rel);inputs[rel]=pin(p);return json.loads(p.read_bytes())
def source(rel):
    p=path(rel);inputs[rel]=pin(p);return p.read_text(encoding='utf-8-sig')
def verify(p):
    actual=pin(path(p['path']));inputs[p['path']]=actual
    check('pin:'+p['path'],actual['bytes']==p['bytes'] and actual['sha256']==p['sha256'])
family='build-ci/20261106-clock-r12-writer-family-r1/'
runtime='build-ci/20261106-clock-r12-serializer-runtime-source-r1/'
joined='build-ci/20261106-clock-r12-writer-join-r2/'
for folder in (family,runtime):
    j=read(folder+'source-pins.json')
    for p in j['files']:verify(p)
w=read(family+'writer-component.json');cold=read(family+'cold-supplement.json')
rt=read(runtime+'runtime-component.json');j=read(joined+'writer-join.json')
check('R2 exact subject',inputs[joined+'writer-join.json']['sha256']=='25C4167F94C8DE538861F391F2DF940D345ABD4139CD219E2ACA25DE97A047A5')
for group in (w['inputs'],cold['inputs'],rt['input_pins'],j['pinned_inputs']):
    for p in group:verify(p)
graph=read(joined+'phase-graph-input-snapshot.json')
check('R2 captured graph identity',inputs[joined+'phase-graph-input-snapshot.json']['sha256']==j['graph_snapshot_original']['sha256'] and inputs[joined+'phase-graph-input-snapshot.json']['bytes']==j['graph_snapshot_original']['bytes'])

def align(n):return n+(-n)%8
def arr(n,width):return align(24+n*width)
def text(n):return align(22+2*n)
distinct=sum(arr(n,2) for n in range(35,519))
def growth(L):
    # Independent bounded enumeration,not subject isqrt expression.
    return max(g for g in range(484) if g*(g+59)<=2*L)
def chunks(L):
    # Closed threshold form,not subject recurrence.
    for count,capacity in enumerate((256,512,1024,2048,4096,8192),1):
        if L<=capacity:return count
    return 6+(L-8192+7999)//8000
def escape(L):return min(distinct,arr(35,2)+2*L+42*growth(L))
def backings(n,width):return sum(arr(2**k,width) for k in range(2,max(2,(n-1).bit_length())+1)) if n else 0

for name,d in w['documents'].items():
    L=d['encoded_byte_upper']-1;n=chunks(L);cap=max(256,L+min(8000,L)-1)
    expected=dict(escape_growth_count_upper=growth(L),escape_all_distinct_array_requests=distinct,
       escape_output_coupled_array_requests=arr(35,2)+2*L+42*growth(L),escape_selected_array_requests=escape(L),
       StringBuilder_chunk_count_upper=n,StringBuilder_all_chunk_capacity_upper=cap,
       StringBuilder_all_chunk_array_requests=2*cap+30*n,StringBuilder_all_chunk_object_requests=48*n,
       ToString_request=text(L),LF_concat_request=text(L+1),UTF8_byte_array_request=arr(L+1,1))
    for k,v in expected.items():check('R1 arithmetic:'+name+':'+k,d[k]==v)
    check('R1 complete stated subtotal:'+name,d['selected_B_request_upper']==sum(expected[k] for k in ['escape_selected_array_requests','StringBuilder_all_chunk_array_requests','StringBuilder_all_chunk_object_requests','ToString_request','LF_concat_request','UTF8_byte_array_request'])+sum(d['fixed_objects_and_position_stack'].values()))
for p in w['phases']:
    check('R1 phase:'+p['id'],p['selected_B_request_upper']==sum(p['encode_vector'][k]*w['documents'][k]['selected_B_request_upper'] for k in w['documents']))
check('cold base subtotal',w['known_once_F_Bcold_subtotal']==sum(w['known_once_F_Bcold'].values()))
check('cold supplement arithmetic',cold['additional_known_once_subtotal']==sum(cold['additional_known_once_parts'].values()))
check('known cold13256',cold['known_once_F_Bcold_subtotal']==w['known_once_F_Bcold_subtotal']+cold['additional_known_once_subtotal']==13256)
check('runtime direct fixed1288',sum(rt['fixed_per_encode'].values())==1288)
check('known join cold13344',j['known_once_F_Bcold']==cold['known_once_F_Bcold_subtotal']+sum(rt['additional_cold_direct_owner_terms'].values())==13344)
for r in cold['HashSet_char_routes']:
    check('cold HashSet arrays:'+str(r['distinct_chars']),r['bucket_and_entry_array_upper']==sum(arr(n,4)+arr(n,24) for n in r['capacities']))

profiles={'marker':(319,1,0,1,35),'receipt':(23635,58,23,5,60),'failure':(262760,79,1,3,38),'bundle':(452921,1294,2,3,41)}
def diagnostic(d,k,sb=48):
    C=d*max(k+5,22)
    return (d+1)*(sb+arr(C,2)+text(C)+32)+64+24*d
corrected_docs={}
for name,(maximum,maps,arrays,depth,key) in profiles.items():
    top=graph['output_topologies'][name]
    check('graph topology:'+name,top['counts']['maps']==maps and sum(top['array_length_histogram'].values())==arrays and top['maximum_container_chain']==depth)
    check('graph keys:'+name,max(map(len,top['keys']))==key and all(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',s) for s in top['keys']))
    d=j['documents'][name];L=maximum-1;W=L+3;n=chunks(W);cap=max(256,W+min(8000,W)-1)
    expected=dict(escape_all_old_arrays=escape(W),StringBuilder_all_arrays=2*cap+30*n,
       StringBuilder_all_objects=48*n,returned_ToString=text(L),returned_LF_concat=text(maximum),
       UTF8_byte_array=arr(maximum,1),StringWriter=64,JsonTextWriter=224,
       JsonPosition_stack=(40+backings(depth-1,32)) if depth>1 else 0,
       UTF8Encoding_plus_overwritten_fallbacks=120,
       direct_serializer_runtime_and_traversal=1288-576+backings(depth,8)+64*maps+48*arrays)
    check('R2 document dimensions:'+name,(d['byte_upper'],d['maps'],d['arrays'],d['container_chain'],d['key_units_upper'])==(maximum,maps,arrays,depth,key))
    for k,v in expected.items():check('R2 arithmetic:'+name+':'+k,d['requests'][k]==v)
    check('R2 perencode:'+name,d['selected_per_encode_upper']==sum(expected.values()))
    check('R2 diagnostic arithmetic:'+name,d['one_failed_encode_path_upper']==diagnostic(depth,key))
    check('runtime original graph:'+name,rt['graphs'][name]['selected_direct_runtime_upper']==1288+64*maps+48*arrays)
    check('runtime original path:'+name,rt['SDK_failure_routes'][name]['selected_failure_route_upper']==diagnostic(depth,512))
    corrected_docs[name]=sum(expected.values())+8*n
    # The corrected value is a candidate equation,not a new subject mutation.
check('R1 runtime error151512',rt['SDK_global_error_tail_upper']==2*diagnostic(5,512)+diagnostic(3,512)==151512)
error=max(diagnostic(d,k) for _,_,_,d,k in profiles.values())+diagnostic(3,38)+diagnostic(5,60)
check('R2 error20280',error==20280)
for p in j['phases']:
    count=sum(p['encode_vector'].values())
    normal=sum(p['encode_vector'][k]*j['documents'][k]['selected_per_encode_upper'] for k in p['encode_vector'])
    check('R2 phase normal:'+p['id'],normal==p['selected_encoding_requests'])
    check('R2 phase error:'+p['id'],p['all_failed_encode_paths_upper']==(error if count else 0))
    check('R2 phase total:'+p['id'],p['selected_B_upper']==normal+(error if count else 0))

# Source witnesses establish the request-before-attachment failure and one
# using-disposal path. No managed allocation or OOM condition is invoked.
sb=source('build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/StringBuilder.cs')
start=sb.index('private void ExpandByABlock(');end=sb.index('private StringBuilder(StringBuilder from)',start)
exp=sb[start:end]
check('SB allocate/construct/attach order',exp.index('GC.AllocateUninitializedArray<char>(newBlockLength)')<exp.index('new StringBuilder(this)')<exp.index('m_ChunkChars = chunkChars;'))
check('SB has five instance fields',all(x in sb for x in ['internal char[] m_ChunkChars;','internal StringBuilder? m_ChunkPrevious;','internal int m_ChunkLength;','internal int m_ChunkOffset;','internal int m_MaxCapacity;']))
check('generic SB source-slot56',16+5*8==56 and 48<56)
convert=source('build-ci/20261105-clock-oracle-json-primary-sources/newtonsoft-JsonConvert.cs')
jw=source(family+'primary/JsonWriter.cs')
jtw=source('build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextWriter.cs')
iw=source(runtime+'primary/JsonSerializerInternalWriter.cs')
check('JsonConvert one using', 'using (JsonTextWriter jsonWriter = new JsonTextWriter(sw))' in convert and 'jsonSerializer.Serialize(jsonWriter, value, type);' in convert)
check('writer disposal closes', 'if (_currentState != State.Closed && disposing)' in jw and 'Close();' in jw[jw.index('protected virtual void Dispose(bool disposing)'):jw.index('internal static void WriteValue',jw.index('protected virtual void Dispose(bool disposing)'))])
check('JsonTextWriter Close once', 'public override void Close()\n        {\n            base.Close();\n\n            CloseBufferAndWriter();' in jtw)
check('close pending property null','if (_currentState == State.Property)\n                {\n                    WriteNull();' in jw)
check('unhandled SDK body rethrows', 'ClearErrorContext();\n                    throw;' in iw)
runner=source('build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1')
check('runner only two failure-tail encodes', 'try{Save-TaskFailureInventory}catch{Set-TaskSecondary 8 $_}' in runner and 'try{Save-TaskReceipt}catch{Set-TaskSecondary 9 $_;' in runner)
check('R1 null counterexample',len('{"x":null}')-len('{"x":0}')==3)
corrected_error=max(diagnostic(d,k,56) for _,_,_,d,k in profiles.values())+diagnostic(3,38,56)+diagnostic(5,60,56)
orphan_per_failed=2*(arr(8000,2)+56)
orphan_global=3*orphan_per_failed
check('finite candidate orphan request upper',orphan_global==96480 and corrected_error==20408)
candidate=[]
for p in j['phases']:
    reach=bool(sum(p['encode_vector'].values()))
    normal=sum(p['encode_vector'][k]*corrected_docs[k] for k in corrected_docs)
    candidate.append(dict(id=p['id'],main_SB56_normal=normal,diagnostic_upper=corrected_error if reach else 0,
       orphan_request_upper=orphan_global if reach else 0,
       candidate_B=normal+(corrected_error+orphan_global if reach else 0)))

result=dict(schema='INDEPENDENT_WRITER_COMPONENT_SOURCE_FILE_INTEGER_REVIEW_R1',
 verdict='REVISE_R1_R2_FAILURE_ARRAY_AND_SB_SLOT_PREMISES_NO_GLOBAL_FIT_ACCEPTANCE',
 actual_check_count=len(checks),all_recorded_checks_pass=all(c['pass_'] for c in checks),checks=checks,
 subject_component_arithmetic_only='All stated R1/runtime/R2 arithmetic recomposed;this does not establish its source completeness.',
 candidate_next_equations=dict(StringBuilder_every_object_upper=56,diagnostic_global_upper=corrected_error,
   extra_pairs_per_failed_encode=2,failed_encodes_upper=3,per_extra_pair=arr(8000,2)+56,
   global_orphan_request_upper=orphan_global,documents_after_SB56=corrected_docs,phases=candidate),
 qualified_limits='Source/representation conditional request component only;not actual object sizes,liveness,reclaim,RSS,complete cold F,whole32MiB fit,binding,or GO.',
 execution=dict(target_parser_AST_import=False,target_compiler_codec_program_probe_stage=False,native_GUI_Git=False,own_file_hash_integer_checker=True),
 inputs=list(inputs.values()))
(HERE/'checks.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
print(json.dumps(dict(checks=len(checks),verdict=result['verdict'],candidate_B=[p['candidate_B'] for p in candidate],checks_sha256=pin(HERE/'checks.json')['sha256'])))
