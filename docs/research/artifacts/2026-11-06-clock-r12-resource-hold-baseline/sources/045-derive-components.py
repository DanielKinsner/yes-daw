"""Additive ordinary source/integer worksheet; preserves the first evaluation."""
from collections import Counter
from fractions import Fraction
from functools import lru_cache
from hashlib import sha256
from pathlib import Path
import json
import re
import struct

HERE=Path(__file__).resolve().parent; ROOT=HERE.parents[1]
def read(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def a8(n):return (n+7)//8*8
def arr(n,w):return a8(24+n*w)
oldpath=HERE/'evaluation.json'
assert digest(oldpath)=='872E24B3B7A11476AAE4949E6B0D43B54D6DD19B266FF0FA52D66A26691E5C4F'
old=read(oldpath)
base=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
manifest=read(base/'source-manifest.json')
prsrc=(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs').read_text()
pr=[int(n) for n in re.findall(r'\b\d+\b',prsrc.split('internal static ReadOnlySpan<int> Primes =>',1)[1].split('];',1)[0])]
def prime(n):return next(x for x in pr if x>=n)
def caps(n,kind):
 out=[];c=0
 while c<n:
  c=(4 if c==0 else c*2) if kind=='list' else prime(c*2)
  out.append(c)
 return out
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def hc(n):
 load=f32(.72);q=f32(f32(n)/load);c=3 if q<=3 else prime(int(q));out=[c]
 for inserted in range(n):
  if inserted>=int(f32(load*c)):c=prime(2*c);out.append(c)
 return out
def list_arrays(n):return sum(arr(k,8) for k in caps(n,'list'))
def dict_arrays(n,possible_collision=False):
 seq=caps(n,'dict')
 result=sum(arr(k,4)+arr(k,24) for k in seq)
 if possible_collision and n>100:result+=arr(seq[-1],4)+arr(seq[-1],24)
 return result
def parsed_map_arrays(n):
 # Both OrderedHashtable's base Hashtable and its inner OrderedDictionary
 # table are retained. ArrayList stores a boxed DictionaryEntry per field.
 if not n:return arr(hc(0)[0],24)
 return arr(hc(n)[0],24)+sum(arr(k,24) for k in hc(n))+arr(n,8)+32*n
def map_base(n):
 return 112+72+96+144+(80+112 if n else 0)
def object_cost(k,s):
 # k raw properties; duplicate names can reduce the final parsed-map size.
 # Maximize final-count-dependent storage, rather than assuming hc monotone.
 return (map_base(k)+128*k+a8(22+2*s)*k+list_arrays(k)+dict_arrays(k,True)
         +max(parsed_map_arrays(u) for u in range(k+1)))
def array_cost(k):return 96+40+40+list_arrays(k)+arr(k,8)

# Exact finite child tree maximization for the raw guard's small node/depth
# domain. A node is each JSON value or property key. Only scalar placeholders
# are used: this does not invoke the target parser or generate target inputs.
@lru_cache(None)
def graph_dp(N,depth,s):
 dp=[None]*(N+1);dp[1]=64+a8(22+2*s)
 if depth==0:return tuple(dp)
 child=graph_dp(N-1,depth-1,s) if N>1 else (None,)
 # partition[k][budget] is the maximum cost of k child values using budget.
 partitions=[[None]*(N+1) for _ in range(N)]
 partitions[0][0]=0
 for k in range(1,N):
  for b in range(k,N):
   best=None
   for v in range(1,b-k+2):
    if v>=len(child) or child[v] is None or partitions[k-1][b-v] is None:continue
    z=partitions[k-1][b-v]+child[v]
    best=z if best is None else max(best,z)
   partitions[k][b]=best
 for n in range(1,N+1):
  if n==1:dp[n]=max(dp[n] or 0,object_cost(0,s),array_cost(0))
  for k in range(1,n):
   b=n-1
   if partitions[k][b] is not None:dp[n]=max(dp[n] or 0,array_cost(k)+partitions[k][b])
   b=n-1-k
   if b>=k and partitions[k][b] is not None:dp[n]=max(dp[n] or 0,object_cost(k,s)+partitions[k][b])
 return tuple(dp)
child_control=graph_dp(13,2,128)
child_summary=graph_dp(25,1,128)
child13=max(x for x in child_control if x is not None)
child25=max(x for x in child_summary if x is not None)

# Generic report: finite source-derived coefficient proof, not a guess based
# on the compiler's normal report. Let N include keys, K be key count, and A
# be array elements. Every tree obeys A=N-2K-1 and K<=(N-1)//2.
N=2*manifest['profile']['include_input_count']+256
maximum_keys=(N-1)//2
def ceil_fraction(q):return (q.numerator+q.denominator-1)//q.denominator
list_ratio=max((Fraction(list_arrays(k),k),k) for k in range(1,N))
dict_ratio=max((Fraction(dict_arrays(k,True),k),k) for k in range(1,maximum_keys+1))
parsed_ratio=max((Fraction(parsed_map_arrays(k),k),k) for k in range(1,maximum_keys+1))
lc,dc,pc=map(ceil_fraction,[list_ratio[0],dict_ratio[0],parsed_ratio[0]])
scalar=64+a8(22+2*512)
# Each nonempty map base<=616; empty<=424. Empty parsed map table<=96.
map_fixed=max(map_base(k)+(parsed_map_arrays(0) if k==0 else 0) for k in range(maximum_keys+1))
array_fixed=96+40+40+24
key_cost=128+a8(22+2*512)+lc+dc+pc
array_edge=lc+8
# Replace all value-node fixed charges by scalar (which dominates map/array
# bases). Substitution of A produces this whole guard-domain source upper.
assert scalar>=map_fixed and scalar>=array_fixed
K=maximum_keys if key_cost-scalar-2*array_edge>=0 else 0
generic=scalar*(N-K)+key_cost*K+array_edge*(N-2*K-1)
report=dict(node_cap=N,key_cap=maximum_keys,map_fixed_upper=map_fixed,array_fixed_upper=array_fixed,
 scalar_upper_excluding_unclosed_BigInteger20=scalar,list_arrays_per_element=lc,dict_arrays_per_key=dc,parsed_map_arrays_per_key=pc,
 ratio_witnesses=dict(list=[str(list_ratio[0]),list_ratio[1]],dictionary=[str(dict_ratio[0]),dict_ratio[1]],parsed_map=[str(parsed_ratio[0]),parsed_ratio[1]]),
 property_key_cost=key_cost,array_edge_cost=array_edge,selected_graph_upper_excluding_BigInteger20=generic,
 equation='scalar*(N-K)+keyCost*K+arrayEdge*(N-2K-1), K=floor((N-1)/2) when its coefficient is positive',
 BigInteger20='b20 controlled parser payload remains explicit. If 64+b20<=scalar, the string scalar envelope subsumes it; otherwise add at most N*(64+b20-scalar). Not assigned0.')

# PowerShell ConvertToJson preprocessing only. The subsequent Newtonsoft
# serializer traversal enumerators are owned by root B, never included here.
clone={}
for name,g in old['output_topologies'].items():
 mh={int(k):v for k,v in g['map_field_histogram'].items()}
 ah={int(k):v for k,v in g['array_length_histogram'].items()}
 dictionary_instances=80*sum(mh.values())
 dictionary_backings=sum(n*(arr(prime(k),4)+arr(prime(k),24)) for k,n in mh.items() if k)
 dictionary_enumerators=80*sum(mh.values())
 dictionary_entry_copies=32*g['counts']['keys']
 enumerable_lists=40*sum(ah.values())
 enumerable_backings=sum(n*list_arrays(k) for k,n in ah.items())
 enumerable_enumerators=48*sum(ah.values())
 terms=dict(dictionary_instances=dictionary_instances,dictionary_backings=dictionary_backings,
  original_dictionary_enumerators=dictionary_enumerators,original_DictionaryEntry_boxed_copies=dictionary_entry_copies,
  enumerable_List_instances=enumerable_lists,enumerable_List_backings=enumerable_backings,original_array_or_List_enumerators=enumerable_enumerators)
 clone[name]=dict(terms=terms,upper=sum(terms.values()),dictionary_prime_capacity_sum=sum(n*prime(k) for k,n in mh.items() if k))
assert {n:r['dictionary_prime_capacity_sum'] for n,r in clone.items()}==dict(marker=7,receipt=362,failure=553,bundle=9074)

def walk_backings(rows,dirs):
 return 40+list_arrays(rows)+arr(rows,8)+40+list_arrays(dirs)
namespace=sum(walk_backings(d['entry_count']+1, d['entry_count']+2 if d['recursive'] else 1) for d in manifest['closed_directory_inputs'])
profiles=read(ROOT/'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/phases.json')['phases']
writerpath=ROOT/'build-ci/20261106-clock-r12-writer-join-r2/writer-join.json'
assert digest(writerpath)=='25C4167F94C8DE538861F391F2DF940D345ABD4139CD219E2ACA25DE97A047A5'
writer=read(writerpath)
phases=[]
fixed_collision_tightening=sum(x['graph_families']['jobject_optional_string_collision_rehash_upper'] for x in old['fixed_inputs'])
for i,(p,q,b) in enumerate(zip(profiles,old['phases'],writer['phases'])):
 # Root namespace directory bound used earlier was root+entry_count+1;
 # source foreach processes exactly those queued directories or first refusal.
 # Here each independent call retains its own backing arrays and ToArray.
 W=(q['stages']*walk_backings(6,1)+24+(namespace*p['fences'])
    +p['references']*walk_backings(60,61)+p['builds']*walk_backings(4,1)
    +p['root_walks']*walk_backings(78,79)+walk_backings(1,1))
 if not q['stages']:W-=24
 Ksum=sum(b['encode_vector'][n]*clone[n]['upper'] for n in clone)
 parsed=p['native_result_parses'];sdk=3+(4 if parsed else 0)+(12 if parsed>1 else 0)
 # 3 is an upper for bootstrap, predecessor and generic report; P0/P1 lack
 # report and use exactly2, retaining source-sized conditional schedule.
 sdk=2+(1 if p['children'] else 0)+(4 if parsed else 0)+(12 if parsed>1 else 0)
 instance_base=1128*sdk
 position_stacks=2*(192+192+(2056 if p['children'] else 0)+(4*192 if parsed else 0)+(12*192 if parsed>1 else 0))
 Jnew=(generic if p['children'] else 0)+((3*child13+child25) if parsed else 0)+((11*child13+child25) if parsed>1 else 0)+instance_base+position_stacks
 # Selected source correction: fixed maps have fewer than101 fields, so the
 # >100 Dictionary string collision-rehash branch cannot execute there.
 terms=dict(prior_selected_partial=q['selected_partial'],remove_impossible_fixed_collision_arrays=-fixed_collision_tightening,
  replace_B_R1_with_B_R2=b['selected_B_upper']-q['selected_terms']['writer_selected_provisional'],
  replace_known_cold_13256_with_13344=88, W_List_Queue_ToArray_backings=W,K_preprocessing_clones=Ksum,
  J_generic_child_graph_and_instances=Jnew)
 phases.append(dict(id=p['id'],new_selected_terms=terms,selected_partial=sum(terms.values()),
  J_SDK_call_upper=sdk,J_reader_writer_instance_base=instance_base,J_position_stacks=position_stacks,
  unresolved_b20_nodes_upper=(N if p['children'] else 0)+(3*13+25 if parsed else 0)+(11*13+25 if parsed>1 else 0)))
out=dict(schema='R12_J_K_W_SOURCE_COMPONENTS_WORKING_R1',status='EVALUATED_PARTIAL_WITH_PRECISE_B20_AND_OTHER_OWNER_GAPS_NOT_WHOLE_U',
 predecessor_evaluation_sha256=digest(oldpath),B_R2_sha256=digest(writerpath),
 fixed_collision_tightening=fixed_collision_tightening,
 fixed_collision_proof='Every pinned bootstrap/predecessor JObject has fewer than101 raw/retained properties; Dictionary requires collisionCount>100, so its randomized string comparer replacement cannot occur. This removes the deliberately conservative optional arrays from only those fixed graphs.',
 report=report,child_graphs=dict(control_N13_depth2=child13,summary_N25_depth1=child25,producer=3*child13+child25,verifier=11*child13+child25,
  full_control_DP=[x for x in child_control],full_summary_DP=[x for x in child_summary],
  BigInteger20='String scalar envelope344 covers all scalar branches only if root proves64+b20<=344; otherwise retain the positive excess per node.'),
 K_clones=clone,W_namespace_all_old_backings_per_fence=namespace,phases=phases,
 source_conditions=['Generic source field-slot bounds, selected x64 array/string/dictionary representations, no current-private-field inspection.',
  'Hashtable insertion-only occupancy cannot trigger cleanup before load-size expansion; no remove calls, no hash-randomization branch in selected Hashtable source.',
  'JObject Dictionary may have at most one NonRandomizedStringEqualityComparer replacement; its maximum final backing is charged for generic maps over100 fields.',
  'No Dispose/null/scope/GC/pool-return physical reclamation credit. Every old managed backing remains charged.',
  'K DictionaryEntry Current copies and original collection enumerators are separate from B serializer traversal enumerators.',
  'All first evaluation bytes are preserved; this file explicitly records its tightening and B successor, and is not final accepted evidence.'],
 remaining=['O original DTOs and private states','A explicit typed/literal/pipeline source transfers not already selected','F fixed script/bootstrap/literal owners and selected SDK cold profile',
  'P canonical geometry, fresh path/normalization/iterator strings and native buffers (root-owned)',
  'N conditional environment clone/block profile (root-owned; ordinary selected SDK noninterference qualification remains)',
  'V source-associated wrapper/stream/encoding/process and authored native-child live payload','E bounded source-controlled error/refusal projections and copies',
  'b20 selected BigInteger parse owners (root-closing); B R2 has independent whole-review corrections pending'])
(HERE/'components.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(fixed_collision_tightening=fixed_collision_tightening,report=report,child=[child13,child25],K={n:d['upper'] for n,d in clone.items()},W_namespace=namespace,phases=[{k:p[k] for k in ['id','selected_partial','J_SDK_call_upper']} for p in phases]),indent=2))
