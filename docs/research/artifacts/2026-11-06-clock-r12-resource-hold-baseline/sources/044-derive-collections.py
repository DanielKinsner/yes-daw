"""Finite source collection/request coefficients and explicit remaining profile."""
from pathlib import Path
from hashlib import sha256
import json,re
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def read(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def a8(n):return (n+7)//8*8
def arr(n,w):return a8(24+n*w)
base=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
source=(base/'run-clock.ps1').read_bytes();manifest=read(base/'source-manifest.json')
old=read(HERE/'evaluation.json');comp=read(HERE/'components.json');original=read(HERE/'originals.json')
profiles=read(ROOT/'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/phases.json')['phases']
prsrc=(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs').read_text()
pr=[int(n) for n in re.findall(r'\b\d+\b',prsrc.split('internal static ReadOnlySpan<int> Primes =>',1)[1].split('];',1)[0])]
def prime(n):return next(x for x in pr if x>=n)
def caps(n,kind):
 out=[];c=0
 while c<n:c=(4 if not c else c*2) if kind=='list' else prime(c*2);out.append(c)
 return out
def list_arrays(n):return sum(arr(k,8) for k in caps(n,'list'))
def capture(n):return 40+list_arrays(n)+arr(n,8)
def dictionary(n):
 cs=caps(n,'prime');total=80+sum(arr(k,4)+arr(k,24) for k in cs)
 if n>100:total+=arr(cs[-1],4)+arr(cs[-1],24)
 return total
def hashset(n):
 # Generic source field-slot upper:8 instance fields; ref-entry has3 fields
 # and stride at most24 under the selected alignment<=8 qualification.
 cs=caps(n,'prime');total=80+sum(arr(k,4)+arr(k,24) for k in cs)
 if n>100:total+=arr(cs[-1],4)+arr(cs[-1],24)
 return total

writerpath=ROOT/'build-ci/20261106-clock-r12-writer-join-r3/writer-join.json'
assert digest(writerpath)=='246F9C0FF1ED59D20B9236954A400BB457B2A9F3C9D169415B90C7E74A41E3BE'
B=read(writerpath)
smallpath=ROOT/'build-ci/20261106-small-runtime-owner-source-r1/small-owner-components.json'
assert digest(smallpath)=='99BA44F5628CDF40AE19452999D790445F395C3E7712BA6DD51F9934F8B33D2B'

# Structural catalogue storage only. P's independently qualified strings,
# provider/normalizer/iterator buffers are separate; this table owns no
# referent strings. All first-build parent arrays are retained.
P_struct=dict(catalogue_10field_map=72+arr(prime(13),24),entry_5field_maps=1294*264,
 rows_exact_List=40+arr(1294,8),input_Dictionary=dictionary(1220),alias_Dictionary=dictionary(2662),
 strings_Dictionary=dictionary(1458),parent_strings_Dictionary=dictionary(1458),
 all_parent_arrays_including_three_unpublished=1297*arr(65,8),
 first_build_alias_pair_arrays=1297*arr(2,8),
 stored_maximum_aliasmaximum_and_parent_count_boxes=24*(2+1297),
 main_input_index_Dictionary=dictionary(1220),main_include_index_Dictionary=dictionary(1221),
 input_index_enumerator_and_KeyValuePair_returns=64+1220*32,
 reference_caps_Dictionary=dictionary(56),build_caps_Dictionary=dictionary(3))

def fs_rows(p):
 rows=[]
 def add(name,n,m,count):rows.append(dict(name=name,expected=n,observed=m,calls=count))
 for d in manifest['closed_directory_inputs']:add('namespace:'+d['path'],len(d['files']),len(d['files']),p['fences'])
 add('reference_internal',56,56,p['references'])
 add('reference_external_before',56,56,int(bool(p['references'])))
 add('build',3,3,p['builds'])
 if p['native_result_parses']:add('producer_ids',3,3,1)
 if p['native_result_parses']>1:add('verifier_ids',11,11,1)
 if p['references']:
  add('missing_control',56,55,1);add('extra_control',56,57,1);add('duplicate_control',56,57,1)
 if p['id'] in ['P4','P5']:add('root_files',67,67,1)
 return rows
def fs_normal(r):
 # Explicit expected and observed HashSets +two always-created Lists.
 # Potential key views/type-array materializations are separately A below.
 return r['calls']*(hashset(r['expected'])+hashset(r['observed'])+80)
def walker_captures(p,q):
 # One bounded collection materialization on each source collection-valued
 # function/pipeline transfer, plus namespace name pipeline and its @().
 ns=sum(2*capture(d['entry_count']+1) for d in manifest['closed_directory_inputs'])
 return (p['fences']*ns+q['stages']*capture(6)+(24 if q['stages'] else 0)
   +p['references']*capture(60)+p['builds']*capture(4)+p['root_walks']*capture(78)+capture(1))
phases=[]
for i,(p,q,o) in enumerate(zip(profiles,old['phases'],original['phases'])):
 rows=fs_rows(p)
 sets=sum(fs_normal(r) for r in rows)
 n=sum(r['expected']*r['calls'] for r in rows);m=sum(r['observed']*r['calls'] for r in rows)
 calls=sum(r['calls'] for r in rows)
 if i==5:assert (calls,n,m)==(259,8783,8784)
 # At most two outside-control FileSet invocations can be replacements:
 # the first main refusal and its mandatory finally fence. This is extra
 # storage of the actual replacing calls, never two fabricated helper calls.
 # The unchanged source argument guard is1225, not a new budget.
 replacement_surplus=2*(2*hashset(1225)+2*capture(1225))
 typed_arg_arrays=8*(n+m)+48*calls
 # Literal/current view captures may alias existing exact arrays; one
 # materialized List+result array per actual argument is conservative.
 key_argument_captures=sum(r['calls']*(capture(r['expected'])+capture(r['observed'])) for r in rows)
 exactkey=dict(descriptor=(6,1),environment=(8,p['children']),control=(3,3 if p['native_result_parses']==1 else 14 if p['native_result_parses']>1 else 0),summary=(12,p['native_result_parses']))
 exactkey_cost=sum(c*(hashset(n)+2*arr(n,8)) for n,c in exactkey.values())
 walk_capture=walker_captures(p,q)
 # Explicit literal strings are ASCII source spans. Source contains no
 # expandable variable string literal, only a quoted LF. Every literal's
 # decoded payload length is <=its disjoint raw span. Quote count is an
 # upper for opener count, including quoted comments conservatively.
 quotes=source.count(b"'")+source.count(b'"')
 literal_string_upper=2*len(source)+28*quotes
 primitive_literal_slots=len(re.findall(rb'(?<![A-Za-z_0-9])(?:0[xX][0-9A-Fa-f]+|[0-9]+(?:[lLuU])?)',source))
 literal_scalar_upper=24*(primitive_literal_slots+2)
 # Normal predefined exact control/error strings are literal aliases.
 # Source-created unexpected witnesses are bounded independently of raw
 # exception internals; six source failure regions can each create <=one.
 E=6*(264+5*24)+a8(22+2*(64+40))+a8(22+2*160)
 # Replace previously provisional B R2 with independently reviewed R3.
 Bdelta=B['phases'][i]['selected_B_upper']-read(ROOT/'build-ci/20261106-clock-r12-writer-join-r2/writer-join.json')['phases'][i]['selected_B_upper']
 terms=dict(previous_selected_partial=o['selected_partial'],B_R3_correction=Bdelta,
  P_catalogue_and_index_structures_only=sum(P_struct.values()),
  A_FileSet_original_sets=sets,A_FileSet_replacing_failure_surplus=replacement_surplus,
  A_FileSet_typed_argument_array_upper=typed_arg_arrays,A_source_key_argument_capture_upper=key_argument_captures,
  A_ExactKeys_set_and_argument_arrays=exactkey_cost,A_collection_function_pipeline_transfers=walk_capture,
  F_fixed_literal_string_upper=literal_string_upper,F_fixed_literal_builtin_scalar_upper=literal_scalar_upper,
  E_source_failure_projection_upper=E)
 phases.append(dict(id=p['id'],FileSet_vector=rows,FileSet_sums=dict(calls=calls,N=n,M=m),selected_terms=terms,selected_partial=sum(terms.values())))
out=dict(schema='R12_STRUCTURAL_COLLECTION_AND_LITERAL_WORKING_R1',status='FINITE_CONDITIONAL_SELECTED_UPPER_WITH_V_P_N_COLD_RESIDUAL_HOLD',
 source_manifest_sha256=digest(base/'source-manifest.json'),B_R3_sha256=digest(writerpath),small_owner_sha256=digest(smallpath),
 P_structural_coefficients=P_struct,P_structural_total=sum(P_struct.values()),
 P_geometry_condition='Only the separately reviewed provisional ordinary-drive root/1294 identities/1458 pooled strings profile. Alias guard2662; three unpublished65-element arrays remain charged. P fresh strings and native buffers are not zero or included here.',
 A_materialization_qualification='Every explicitly requested collection-valued pipeline/capture/typed-array transfer has bounded source length. The assigned List/backing/result-array envelope must be joined to the selected PowerShell Compiler/Pipe/PipelineResult array-association proof; exact-array aliases may tighten it. No scalar-call frame allocation tariff is imposed.',
 literals_qualification='Literal string spans disjoint in this fixed non-expandable source; all decoded literal units <=rawsource bytes and literal count<=quote count. Numeric regex deliberately overcounts hashes/comments; no literal slot is assumed runtime-reallocated at every call. Source parser/binder/type metadata general heap remains unqualified.',
 phases=phases,
 remaining_coefficients=[
  dict(id='P_fresh',owner='Path/catalogue first-normalization/iterator Current/filename/relative/replace/unknown-prefix/native-normalizer allocations',coefficient='Root P source allocation request sum; this worksheet already owns only P structural maps/dictionaries/parent-reference arrays.'),
  dict(id='N_capture',owner='Inherited environment clone at each child then cleared Dictionary/outgoing block',coefficient='Sum_i Capture(E=15,V=690+P_i,B=2*(721+P_i)); P_i<=32766 only under qualified ordinary selected SDK noninterference/stock profile. No actual host snapshot.'),
  dict(id='V_hash',owner='IncrementalHash/provider/handle and FileStream wrapper/source buffers',coefficient='(H+7)*v_incremental + FileStreamCalls*v_stream; file/source/crypto CPU/internal native heap remains opaque, not counted as0.'),
  dict(id='V_FileInfo',owner='Source FileInfo wrappers',coefficient='136*F_i, F_i=all H+7bootstrap+walker_file_rows+3ReadBounded+4GetPE+document-write old-length checks+source GetItem wrappers. Each actual non-file row can reduce F_i; referent paths in P.'),
  dict(id='V_child',owner='Process/pipe/hidden StreamReaders/command/environment builder/task/async wrappers and authored C++ scratch',coefficient='c*(fixed_process_and_streams+2*Abyte4096+2*Achar(m4096))+outerOps*v_outer+innerOps*v_inner+max(one active native child payload), outerOps<=258c, innerOps<=1056772c pending exact SDK scheduling proof. m4096=selectedEncoding.GetMaxCharCount(4096), no assumed console codepage.'),
  dict(id='F_SDKcold',owner='Controlled selected serializer EnumUtils/concurrent-store/reflection/name/contract-cache owners',coefficient='Root B named once-only selected-profile remainder; general VM/binder/type-metadata heap expressly UNQUALIFIED, not0.'),
  dict(id='A_association',owner='Selected source association of explicit collection pipeline/typed-copy envelopes',coefficient='The finite evaluated A rows require the exact Compiler/Pipe/PipelineResult selected-source association, not a new per-action fee.')],
 no_claim='Complete numeric U, coexisting lower/NOFIT,physical-process/RSS/GC freedom,immutable binding and execution GO are all withheld.')
(HERE/'collections.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(P_structural_total=sum(P_struct.values()),phases=[dict(id=p['id'],selected_partial=p['selected_partial'],sets=p['FileSet_sums']) for p in phases]),indent=2))
