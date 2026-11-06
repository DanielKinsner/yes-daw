"""Fresh independent R3 file/JSON/integer check;never imports any subject/helper."""
from pathlib import Path
from hashlib import sha256
import json,re

ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
checked=[];pins={}
def pin(p):return dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=sha256(p.read_bytes()).hexdigest().upper())
def need(label,value,**extra):
    checked.append(dict(label=label,pass_=bool(value),**extra))
    if not value:raise AssertionError(label)
def data(rel):
    p=ROOT/rel;pins[rel]=pin(p);return json.loads(p.read_bytes())
def src(rel):
    p=ROOT/rel;pins[rel]=pin(p);return p.read_text(encoding='utf-8-sig')
def verify(p):
    got=pin(ROOT/p['path']);pins[p['path']]=got
    need('raw pin:'+p['path'],got['bytes']==p['bytes'] and got['sha256']==p['sha256'])
prior_folder='build-ci/20261106-clock-r12-writer-components-independent-review-r1/'
prior_review=data(prior_folder+'source-pins.json')
for p in prior_review['subject_and_reference_files']+prior_review['authored_files']:verify(p)
r2=data('build-ci/20261106-clock-r12-writer-join-r2/writer-join.json')
r3=data('build-ci/20261106-clock-r12-writer-join-r3/writer-join.json')
need('exact R3 identity',pins['build-ci/20261106-clock-r12-writer-join-r3/writer-join.json']['sha256']=='246F9C0FF1ED59D20B9236954A400BB457B2A9F3C9D169415B90C7E74A41E3BE')
for p in r3['pinned_inputs']:verify(p)
graph=data('build-ci/20261106-clock-r12-writer-join-r2/phase-graph-input-snapshot.json')
need('graph snapshot association',pins['build-ci/20261106-clock-r12-writer-join-r2/phase-graph-input-snapshot.json']['sha256']==r3['graph_snapshot_original']['sha256'])
bundle=data('build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/bundle-bound.json')
need('bundle all20-digit variant',next(v for v in bundle['variants'] if not v['static_integrals_exact'])['bound']['bytes']==452921)

def rounded(n):return 8*((n+7)//8)
def array(n,width):return rounded(24+n*width)
def string(n):return rounded(22+2*n)
def capacities(n):
    return [2**i for i in range(2,max(2,(n-1).bit_length())+1)] if n else []
def lists(n,width):return sum(array(c,width) for c in capacities(n))
def chunks(n):
    capacities0=(256,512,1024,2048,4096,8192)
    return next((i+1 for i,c in enumerate(capacities0) if n<=c),6+(max(0,n-8192)+7999)//8000)
def escapes(n):
    g=max(i for i in range(484) if i*(i+59)<=2*n)
    return min(sum(array(i,2) for i in range(35,519)),array(35,2)+2*n+42*g)
def error(d,key):
    c=d*max(key+5,22)
    return (d+1)*(56+array(c,2)+string(c)+32)+64+24*d

literal_dims={'marker':(319,1,0,1,35),'receipt':(23635,58,23,5,60),'failure':(262760,79,1,3,38),'bundle':(452921,1294,2,3,41)}
expected_documents={}
for name,(wire,maps,arrays,d,key) in literal_dims.items():
    top=graph['output_topologies'][name]
    need('closed graph:'+name,(top['counts']['maps'],sum(top['array_length_histogram'].values()),top['maximum_container_chain'],top['maximum_key_units'])==(maps,arrays,d,key))
    need('fixed ASCII identifiers:'+name,max(map(len,top['keys']))==key and all(re.fullmatch('[A-Za-z_][A-Za-z_0-9]*',s) for s in top['keys']))
    L=wire-1;W=L+3;count=chunks(W);cap=max(256,W+min(8000,W)-1)
    direct=280+272+80+40+40+lists(d,8)+64*maps+48*arrays
    pieces=dict(escape_all_old_arrays=escapes(W),StringBuilder_all_arrays=2*cap+30*count,
       StringBuilder_all_objects=56*count,returned_ToString=string(L),returned_LF_concat=string(wire),
       UTF8_byte_array=array(wire,1),StringWriter=64,JsonTextWriter=224,
       JsonPosition_stack=40+lists(d-1,32) if d>1 else 0,
       UTF8Encoding_plus_overwritten_fallbacks=120,direct_serializer_runtime_and_traversal=direct)
    doc=r3['documents'][name]
    need('all R3 document dimensions:'+name,(doc['byte_upper'],doc['nominal_text_upper'],doc['close_time_text_upper'],doc['maps'],doc['arrays'],doc['container_chain'],doc['key_units_upper'])==(wire,L,W,maps,arrays,d,key))
    need('all piece names:'+name,set(doc['requests'])==set(pieces))
    for k,v in pieces.items():need('R3 recomposition:'+name+':'+k,doc['requests'][k]==v)
    need('R3 per encode:'+name,doc['selected_per_encode_upper']==sum(pieces.values()))
    need('R3 path error:'+name,doc['one_failed_encode_path_upper']==error(d,key))
    expected_documents[name]=sum(pieces.values())
error_bound=max(error(d,k) for _,_,_,d,k in literal_dims.values())+error(3,38)+error(5,60)
orphan=3*2*(array(8000,2)+56)
need('finite error/addendum',error_bound==20408 and orphan==96480)
vectors=[(0,0,0,0),(1,2,1,0),(1,2,1,0),(1,3,1,0),(1,4,1,0),(1,4,1,1)]
totals=[]
for i,(phase,vec) in enumerate(zip(r3['phases'],vectors)):
    need('phase vector P'+str(i),tuple(phase['encode_vector'][n] for n in literal_dims)==vec)
    normal=sum(n*expected_documents[k] for k,n in zip(literal_dims,vec));active=bool(sum(vec))
    need('phase normal P'+str(i),phase['selected_encoding_requests']==normal)
    need('phase failure P'+str(i),phase['all_failed_encode_paths_upper']==(error_bound if active else 0))
    need('phase orphan P'+str(i),phase['orphan_retry_array_and_chunk_attempt_upper']==(orphan if active else 0))
    total=normal+(error_bound+orphan if active else 0);totals.append(total)
    need('phase total P'+str(i),phase['selected_B_upper']==total)
need('all six selected B prefixes',totals==[0,2755816,2755816,2999612,3243408,6800264])
need('known cold remains subtotal13344',r3['known_once_F_Bcold']==r2['known_once_F_Bcold']==13344)
need('orphan formula stated',r3['R3_corrections']['each_pair_request_upper']==array(8000,2)+56 and r3['R3_corrections']['whole_orphan_retry_upper']==orphan and r3['R3_corrections']['failed_encodes_upper']==3 and r3['R3_corrections']['orphan_pairs_per_failed_encode_upper']==2)

# Exhaustive structural leaf diff. Any unexpected delta fails;there is no
# rule allowing unchanged inherited source to become an accepted global fit.
diffs=[]
def compare(a,b,p=''):
    if type(a)!=type(b):diffs.append(p);return
    if isinstance(a,dict):
        for k in sorted(set(a)|set(b)):
            q=p+'/'+k
            if k not in a or k not in b:diffs.append(q)
            else:compare(a[k],b[k],q)
    elif isinstance(a,list):
        for i in range(max(len(a),len(b))):
            q=p+'/'+str(i)
            if i>=len(a) or i>=len(b):diffs.append(q)
            else:compare(a[i],b[i],q)
    elif a!=b:diffs.append(p)
compare(r2,r3)
allowed={'/schema','/status','/R3_corrections','/prior_r2_sha256','/pinned_inputs/5','/pinned_inputs/6'}
for name in literal_dims:
    allowed.update('/documents/'+name+'/'+s for s in ['requests/StringBuilder_all_objects','selected_per_encode_upper','one_failed_encode_path_upper'])
for i in range(6):
    allowed.add('/phases/'+str(i)+'/orphan_retry_array_and_chunk_attempt_upper')
    if i:allowed.update('/phases/'+str(i)+'/'+s for s in ['selected_encoding_requests','all_failed_encode_paths_upper','selected_B_upper'])
need('all R3 structural changes exactly scoped',set(diffs)==allowed,diffs=diffs)

sb=src('build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/StringBuilder.cs')
section=sb[sb.index('private void ExpandByABlock('):sb.index('private StringBuilder(StringBuilder from)')]
need('orphan request exact operation order',section.index('GC.AllocateUninitializedArray<char>(newBlockLength)')<section.index('new StringBuilder(this)')<section.index('m_ChunkChars = chunkChars;'))
need('chunk size selected source', 'int newBlockLength = Math.Max(minBlockCharCount, Math.Min(Length, MaxChunkSize));' in section and 'internal const int MaxChunkSize = 8000;' in sb)
needed_fields=['internal char[] m_ChunkChars;','internal StringBuilder? m_ChunkPrevious;','internal int m_ChunkLength;','internal int m_ChunkOffset;','internal int m_MaxCapacity;']
need('all five fields now source-slot56',all(s in sb for s in needed_fields) and 16+len(needed_fields)*8==56)
jw=src('build-ci/20261106-clock-r12-writer-family-r1/primary/JsonWriter.cs')
jtw=src('build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextWriter.cs')
conv=src('build-ci/20261105-clock-oracle-json-primary-sources/newtonsoft-JsonConvert.cs')
need('one using source', 'using (JsonTextWriter jsonWriter = new JsonTextWriter(sw))' in conv)
need('one Close source','public override void Close()\n        {\n            base.Close();\n\n            CloseBufferAndWriter();' in jtw)
need('null completion source','if (_currentState == State.Property)\n                {\n                    WriteNull();' in jw)
need('source cleanup text less than minimum growth',4+max(t[3] for t in literal_dims.values())<256)
need('ordinary complete component remains conditional',bool(r3['remaining']) and 'HOLD' in r3['status'] and r3['source_changed'] is False and r3['caps_or_domain_changed'] is False)
record=dict(schema='FRESH_R3_WRITER_COMPONENT_INDEPENDENT_CHECKS_R2',verdict='ACCEPT_CONDITIONAL_SOURCE_COMPONENT_ONLY',actual_check_count=len(checked),checks=checked,
 B_prefixes=totals,diagnostic20408=error_bound,orphan96480=orphan,known_cold_subtotal13344=r3['known_once_F_Bcold'],
 limits=['No complete cold F or global32MiB fit/NOFIT.','Header/slot/array/embedded representation and selected source/binary/profile remain conditional.','No target parse/codec/import/compiler/probe/program/stage/native/GUI/Git or author helper execution.'],inputs=list(pins.values()))
(HERE/'checks.json').write_bytes((json.dumps(record,indent=2)+'\n').encode())
print(json.dumps(dict(checks=len(checked),verdict=record['verdict'],B_prefixes=totals,checks_sha256=pin(HERE/'checks.json')['sha256'])))
