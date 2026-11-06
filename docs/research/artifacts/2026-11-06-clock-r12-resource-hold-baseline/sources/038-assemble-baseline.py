"""Freeze one complete HOLD disposition. Does not claim a whole numeric U."""
from pathlib import Path
from hashlib import sha256
import json,re
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def read(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def a8(n):return (n+7)//8*8
def arr(n,w):return a8(24+n*w)
old=read(HERE/'evaluation.json');C=read(HERE/'collections.json');O=read(HERE/'originals.json');N=read(HERE/'environment.json')
wrapperpath=ROOT/'build-ci/20261106-controller-v-wrapper-source-closure-r1/primary/DictionaryWrapper.cs'
assert digest(wrapperpath)=='B8E2B5381CF374C615F64C044DF2B5BB1E17FC3A44500331EFC4F7D53955CEEB'
hashes=read(HERE/'fixed-key-hash-chains.json')
base=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
source=(base/'run-clock.ps1').read_text();manifest=read(base/'source-manifest.json')
prsrc=(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs').read_text()
pr=[int(n) for n in re.findall(r'\b\d+\b',prsrc.split('internal static ReadOnlySpan<int> Primes =>',1)[1].split('];',1)[0])]
def prime(n):return next(x for x in pr if x>=n)
def caps(n):
 cs=[];c=0
 while c<n:c=4 if not c else 2*c;cs.append(c)
 return cs
def capture(n):return 40+sum(arr(c,8) for c in caps(n))+arr(n,8)
def grow(n):
 cs=[];c=0
 while c<n:c=prime(c*2);cs.append(c)
 return cs
def hset(n):return 80+sum(arr(c,4)+arr(c,24) for c in grow(n))
hashfreeze=ROOT/'build-ci/20261106-fixed-key-hash-independent-review-r1/freeze-manifest.json'
assert digest(hashfreeze)=='1F7FC534921DFCDE46CB1F425EDAE36D4AD769FAB18EF975FBDF259949D25883'
assert digest(HERE/'check-fixed-key-hashes.py')=='BACD6D7F66B76334429005BBF38A7C028733E66E3596C649CE11827DB5D5671C'
assert digest(HERE/'fixed-key-hash-chains.json')=='587CB936BC03C25E3C6336AC1240C8B4EA763AEAE1EE327F4D43D1987610E1FD'
remove_rehash=sum(arr(t['growth_capacities'][-1],4)+arr(t['growth_capacities'][-1],24) for t in hashes['tables'])

phases=[]
docchecks=[0,7,7,9,11,13]
for i,(c,o,n,first) in enumerate(zip(C['phases'],O['phases'],N['phases'],old['phases'])):
 # Replace the generic all-argument envelope with source-site-owned copies.
 # Namespace four materializations are retained: two in walker_captures,
 # two here. Already returned string[] arguments need no second capture.
 actual_captures=0;actual_typed=0
 for r in c['FileSet_vector']:
  name=r['name'];a=r['expected'];b=r['observed'];k=r['calls']
  if name.startswith('namespace:') or name in ['producer_ids','verifier_ids']:
   actual_captures+=k*(capture(a)+capture(b));actual_typed+=k*(arr(a,8)+arr(b,8))
  elif name in ['reference_internal','reference_external_before','build']:
   actual_captures+=k*capture(a);actual_typed+=k*arr(a,8)
  elif name=='missing_control':
   actual_captures+=k*(capture(b)+capture(a));actual_typed+=k*(arr(a,8)+arr(b,8))
  elif name in ['extra_control','duplicate_control']:
   actual_captures+=k*(capture(a)+arr(1,8)+arr(b,8)+capture(a));actual_typed+=k*(arr(a,8)+arr(b,8))
  else:assert name=='root_files'
 capture_delta=actual_captures-c['selected_terms']['A_source_key_argument_capture_upper']
 typed_delta=actual_typed-c['selected_terms']['A_FileSet_typed_argument_array_upper']
 child=first['children'];parsed=first['native_results']
 extra_sets=hset(4)*first['fences']+(hset(3) if parsed else 0)+(hset(11) if parsed>1 else 0)
 # Readable exact source Keys-view bound: one40-slot view per exact-key
 # check and per manifest-environment foreach, plus ten cached24-byte
 # expected-control key views and two cached private caps key views.
 exactkey_calls=1+child+(3 if parsed==1 else 14 if parsed>1 else 0)+parsed
 key_views=40*(exactkey_calls+child+parsed)+48+(240 if i else 0)
 # Current source raw controls do not reach SDK parsing. An uncaught SDK
 # parse failure aborts main, so at most one JTokenWriter close can insert
 # one replacement null JValue. This named source upper is separate from
 # the normal graph; selected ordinary codec association remains required.
 Jfailure=64
 rqty=13*int(bool(child))+sum(2*r['calls'] for r in c['FileSet_vector'] if max(r['expected'],r['observed'])>=102)+4
 rand_objects=40*rqty
 Finfo=first['hashes']+first['walker_rows']+7+2+int(bool(child))+(4 if child else 1 if i else 0)+docchecks[i]+1+int(i==5)
 FI=136*Finfo
 terms=dict(previous_selected_conditional=c['selected_partial'],
  remove_six_unreachable_fixed_rehash_backings=-remove_rehash,
  A_source_site_capture_correction=capture_delta,A_exact_string_array_alias_correction=typed_delta,
  A_two_expected_control_missing_unknown_List_backings=(112 if parsed else 0),
  A_own_fence_and_native_id_HashSets=extra_sets,A_explicit_key_view_instances=key_views,
  J_possible_failed_close_null_value=Jfailure,
  J_A_possible_randomized_comparer_objects=rand_objects,
  V_source_FileInfo_wrappers=FI,N_conditional_capture_and_outgoing=n['numeric_selected']+24*child)
 phases.append(dict(id=first['id'],stage_calls=first['stages'],stage_rows=first['appended_stage_rows'],
  children=child,FileInfo_calls_upper=Finfo,possible_randomized_comparer_instances_upper=rqty,
  selected_terms=terms,selected_numeric_partial=sum(terms.values()),
  remaining_equation=f"{sum(terms.values())} + P_fresh_{i} + V_managed_native_stream_async_{i} + V_native_child_{i} + F_SDKcold_{i} + A_association_delta_{i}"))

families=[
 dict(id='F',disposition='EVALUATED_LITERAL_FIXED_BYTES_KNOWN_COLD_PLUS_PROFILE_HOLD',evidence=['originals.json','collections.json','writer B R3'],remaining=['F_SDKcold']),
 dict(id='R',disposition='EVALUATED_RAW_CAPTURE_COMPACT_STRING_GRAMMAR_COPY_MUTATION_PE_BYTES',evidence=['evaluation.json','originals.json'],remaining=['V source stream/path buffers, separately owned']),
 dict(id='J',disposition='EVALUATED_FIXED_GRAPHS_GENERIC2698_CHILD13_25_AND_READER_INSTANCES',evidence=['evaluation.json','components.json','small-owner b20=160 source'],remaining=['A/source/SDK association conditions; normal/failure close-null source qualification']),
 dict(id='H',disposition='EVALUATED_TUPLE_DIGEST_STORED_LENGTH_HEX',evidence=['evaluation.json'],remaining=['V_hash provider/native wrappers, not digest or Tuple copies']),
 dict(id='W',disposition='EVALUATED_ROWS_STORED_BOXES_QUEUE_LIST_TOARRAY',evidence=['evaluation.json','components.json'],remaining=['P iterator strings/native buffers and V iterator wrappers']),
 dict(id='O',disposition='EVALUATED_ORIGINAL_DTO_MAP_LIST_BOX_AND_ALL11_PREFLIGHTS',evidence=['originals.json'],remaining=['A exact copy/alias association condition']),
 dict(id='K',disposition='EVALUATED_SEVEN_CODEC_PREPROCESS_CLONE_FAMILIES',evidence=['components.json'],remaining=['F SDK cold/type/cache source profile, not traversal duplicated in B']),
 dict(id='B',disposition='ACCEPTED_CONDITIONAL_R3_SOURCE_COMPONENT',evidence=['B R3 246F9C0F','independent reviewer B2EA9D0F'],remaining=['F_SDKcold']),
 dict(id='P',disposition='EVALUATED_STRUCTURES_FIXED_HASH_THEOREM_ACCEPTED_P_FRESH_HOLD',evidence=['collections.json','fixed-key-hash-chains.json','root fixed hash review freeze1F7FC5'],remaining=['P_fresh']),
 dict(id='A',disposition='EVALUATED_EXPLICIT_ARRAY_SET_COPY_SOURCE_ENVELOPES_WITH_ASSOCIATION_HOLD',evidence=['originals.json','collections.json','this source-site correction'],remaining=['A_association_delta']),
 dict(id='N',disposition='EVALUATED143504_PER_CHILD_CONDITIONAL_STOCK_ENV_PROFILE',evidence=['environment.json','root N writer closure','DictionaryWrapper.cs B8E2B538 one-ref source slot24'],remaining=['ordinary selected SDK/stock noninterference qualification']),
 dict(id='V',disposition='FILEINFO136_EVALUATED_OTHER_WRAPPERS_AND_NATIVE_CHILD_HOLD',evidence=['small-owner source','this FileInfo call schedule','numeric agent V source closure in progress'],remaining=['V_managed_native_stream_async','V_native_child']),
 dict(id='E',disposition='EVALUATED_SOURCE_WITNESSES_PROJECTIONS_REFERENCES_AND_B_FAILURE_CODEC_PATHS',evidence=['collections.json','B R3 failure/orphan model','fixed reference slots in F'],remaining=['General CLR/SDK exception,task-fault,ErrorRecord heap remains explicitly UNQUALIFIED under contract, not0'])]
out=dict(schema='R12_COMPLETE_RESOURCE_BASELINE_DISPOSITION_HOLD_R1',verdict='HOLD_NO_COMPLETE_NUMERIC_UPPER_NO_FIT_OR_NOFIT',
 complete_means='Every one of13 resource families has a concrete evaluated component or precise remaining source/profile equation. Complete does not mean all resource obligations are proved or the selected partial is a whole upper.',
 source_manifest_sha256=digest(base/'source-manifest.json'),runner_sha256=digest(base/'run-clock.ps1'),
 cap=33554432,source_or_cap_or_domain_changed=False,target_execution=False,
 phases=phases,families=families,
 fixed_hash_theorem=dict(helper_sha256=digest(HERE/'check-fixed-key-hashes.py'),result_sha256=digest(HERE/'fixed-key-hash-chains.json'),independent_freeze_sha256=digest(hashfreeze),removed_optional_array_requests=remove_rehash,
  scope='Only exact R12 provisional P geometry and selected source arithmetic; no R13 transfer.'),
 N_wrapper_source_correction=dict(path=wrapperpath.relative_to(ROOT).as_posix(),bytes=wrapperpath.stat().st_size,sha256=digest(wrapperpath),field_slot_upper=24,scope='Object plus one readonly Dictionary reference; direct IDictionary implementation, no StringDictionary/unused Hashtable. Source association/model only, not observed runtime size.'),
 remaining=[
  dict(id='P_fresh',required='Evaluate the complete source-indexed Join-Path/provider/fullpath/Current/GetFileName/GetRelativePath/Replace/safe-prefix/unknown-first-refusal strings, including failed-copy candidates and long native normalizer rents; this baseline already owns catalogue/parent-reference/dictionary structures and FileInfo objects.',authority='Root P geometry review explicitly leaves these request/lifetime terms open.'),
  dict(id='V_managed_native_stream_async',required='Selected source fields/buffers for every FileStream/crypto/provider/handle/process/Stopwatch/CTS/decoder/encoding/iterator/async wrapper; source actual stream site vector and concurrent handles; command-line ValueStringBuilder/native pools; callback/registration/worker/thread-static ownership.',equation='Sum actual siteCount*sourceWrapperUpper + c*(two4096 source read arrays + two4096 hidden StreamReader byte arrays + two char[m4096] arrays) + outerOps*v_outer + innerOps*v_inner + workerThreads*v_thread + bounded failed-allocation/cancellation tails; corrected outerOps<=259c and innerOps<=1056772c are separate source proposals. No unverified pool hit/rollback/reuse or cp0=UTF8.',owner='Root/numeric agent independent V source closure.',outer_correction='Earlier258 omitted two EOF-short positive returns.255 full4096 chunks+two1-byte EOFshort positive results fit1MiB; two subsequent zero results yield2initial+257positive successors=259. Earlier unsealed258 records remain preserved history, not current authority.'),
  dict(id='V_native_child',required='Joint controller plus one actual active native child; no per-process cap split. Count child-owned fixed Catalogue, FractionCache, Stream/Text, fixture/vector/prefix/string/argv/control/metadata/exception payloads and selected allocator/deallocation/ABI facts.',equation='Catalogue + atMost2*sizeof(Stream) + atMost4*sizeof(Text) (including no-NRVO return/caller alternatives) + sizeof(FractionCache) + actual live vectors/string capacities + remaining own POD/call-chain storage + selected CRT/FILE/native-library resource qualification. Source asserts Text<=65552,Stream<=65664 and an8*(Stream+Text)+FractionCache+32768<=2MiB condition, but the assertion alone is not the whole allocation/lifetime proof.',lifetime='Only observed native child exit releases that entire process term. Parent managed request owners are not freed by Clear,Dispose,null or scope.'),
  dict(id='F_SDKcold',required='Root B names EnumUtils/concurrent store/reflection/contract cache initialization and selected wrapper/array storage profiles. Shared nonrandomized comparer singleton sources and underlying comparer identities must be joined once rather than implicitly preinitialized/free.',boundary='Pure VM frames,transient scalar parameter/counter boxing,binder/type-metadata/general CLR execution and Exception/ErrorRecord/task-fault internals remain expressly UNQUALIFIED outside controlled source payload. This applies original contract47-50/109-110 and corrects R1 overbroad binder wording; it never claims their allocations0 or whole-process heap fit.'),
  dict(id='A_association_delta',required='Independently reconcile selected Compiler/Pipe/PipelineResult/PSToObjectArrayBinder and exact typed conversion branches at each explicit @()/function/ToArray/concat/key-view site. The candidate selected envelopes are finite and source-sized; a correction can be positive or negative. It is NOT safely0 by omission, nor a generic per-call boxing/iterator tariff.'),
  dict(id='PROFILE',required='Exact canonical final input/root/caller/environment/binary/source/ABI association and independent whole review. Current provisional1220-input/31-namespace geometry is not a promoted future binding or execution profile.')],
 controlled_failure_model=['All bounded abnormal native stdout/stderr remain admitted:1MiB child log cap,65536 captured stdout,33nonempty slices,4/12 SDK-call envelope. No quiet-main law.',
  'GetTaskStage can append a seventh first-refusing row; full source main/inventory/footer schedules remain. SaveFailureInventory inner walk catch CONTINUES to document write; no3-region retry shortcut.',
  'Known persistent dictionaries have no post-created growth and no comparer rehash only by independently accepted fixed-root theorem. Ephemeral generic J/FileSet failure candidates retain one capacity/rehash history and comparer40 per possible transition; whole source-failure review remains required.',
  'B R3 includes close-time +3 text,56-byte SB object field-slot bound,orphan expansion retry allocations and up to3 failed codec paths. R1/R2 are preserved revisions.',
  'All selected managed old allocations remain in the upper without GC credit. Native source releases/pool reuse require their own proof.'],
 branch_join='For bootstrap/predecessor raw identity failure compare max(rawcap request, fixed successful capture+decode+graph requests); the successful fixed branches dominate the raw-only alternatives. Compiler report and captured children use their full cap-domain alternatives. Max over all six source prefixes after every residual is joined; do not add six phase totals together.',
 numerical_meaning='Selected partials are conservative upper bounds of selected components under their source/profile premises, not lower bounds. A partial upper above32MiB proves neither actual coexisting NOFIT nor fit. No complete U_i is asserted while the residual/profile terms remain open.',
 tests='Ordinary Python/source/file/JSON/uint/integer arithmetic only. No target PowerShell parse/AST/import/helper,compiler,stage,native program,GUI,Git/CI or host environment enumeration by this author.',
 successor='R13 is a separate root-authored/reviewed successor; no R12 scalar/capacity/profile authority automatically transfers. This baseline proposes no further source optimization or cap change.')
(HERE/'baseline.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(removed_fixed_rehash=remove_rehash,phases=[{k:p[k] for k in ['id','FileInfo_calls_upper','possible_randomized_comparer_instances_upper','selected_numeric_partial']} for p in phases],remaining=[r['id'] for r in out['remaining']]),indent=2))
