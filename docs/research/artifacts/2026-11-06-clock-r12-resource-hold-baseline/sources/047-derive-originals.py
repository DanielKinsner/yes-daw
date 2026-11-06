"""Source-created DTO, preflight and fixed-array request envelopes only."""
from hashlib import sha256
from pathlib import Path
import json,re,struct
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def read(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def a8(n):return (n+7)//8*8
def arr(n,w):return a8(24+n*w)
base=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
source=(base/'run-clock.ps1').read_text();manifest=read(base/'source-manifest.json')
first=read(HERE/'evaluation.json');components=read(HERE/'components.json')
profiles=read(ROOT/'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/phases.json')['phases']
prsrc=(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs').read_text()
pr=[int(n) for n in re.findall(r'\b\d+\b',prsrc.split('internal static ReadOnlySpan<int> Primes =>',1)[1].split('];',1)[0])]
def prime(n):return next(x for x in pr if x>=n)
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def hc(initial,final=None):
 n=initial if final is None else final
 q=f32(f32(initial)/f32(.72));c=3 if q<=3 else prime(int(q));out=[c]
 for inserted in range(n):
  if inserted>=int(f32(f32(.72)*c)):c=prime(2*c);out.append(c)
 return out
def hmap(initial,final=None):return 72+sum(arr(c,24) for c in hc(initial,final))
def ordered(n):return 64+40+arr(n,8)+72+sum(arr(c,24) for c in hc(n))+32*n
def lc(n):
 out=[];c=0
 while c<n:c=4 if not c else c*2;out.append(c)
 return out
def growlist(n):return 40+sum(arr(c,8) for c in lc(n))
def exactlist(n):return 40+arr(n,8)

# Each table row is a concrete source construction or source-stored payload
# replacement. Descriptor byte counts borrow H/J/W boxes and are not newly
# boxed here. Scalar-store envelopes below are limited to private newly
# produced fields; VM-only temporaries do not enter this table.
def original_rows(p):
 created=bool(p['marker']);c=p['children'];r=p['references'];b=p['builds'];parsed=p['native_result_parses'];late=bool(p['bundle']);postverify=p['id'] in ['P4','P5']
 rows=[]
 def add(name,count,unit,site,reason):
  rows.append(dict(owner=name,count=count,unit_upper=unit,requests=count*unit,source_lines=site,reason=reason))
 add('receipt_initial_ordered_map',1,ordered(30),[56],'One initial ordered root; every initial boxed DictionaryEntry included.')
 add('accepted_predecessor_descriptor_map',1,hmap(6),[769,917],'One source projection before created root; the final bundle borrows the same descriptor.')
 add('receipt_children_List',1,growlist(c),[56,249],'One rooted List, source-sized old backings.')
 receipt_updates=(4 if created else 0)+(2 if c else 0)+(2 if parsed else 0)+(4 if postverify else 0)+(6 if created else 0)+2*c+p['receipts']
 add('receipt_replaced_DictionaryEntry_objects',receipt_updates,32,[250,738,891,952,958,970,980,998],'Every source setter replaces an ordered DictionaryEntry; retained old entries are charged.')
 add('receipt_possible_new_scalar_referents',receipt_updates,24,[250,738,891,952,958,970,980,998],'Conservative one newly promoted builtin scalar per setter; reference/string writes can only reduce this upper. No descriptor/H/J boxes counted by this row.')
 add('child_ordered_rows',c,ordered(24),[248],'One actual 24-field row per reached child including every DictionaryEntry.')
 add('child_stored_scalar_field_upper',c,21*24,[248],'24 fields minus name/failure/cleanup; at most21 newly controlled builtin scalar referents.')
 add('child_cleanup_List_and_ToArray',c,exactlist(13)+arr(13,8),[194,248],'The thirteen-obligation List is exact-capacity, then one source ToArray.')
 add('one_quarantine_map',int(bool(c)),max(hmap(8),hmap(11))+3*24,[245,253],'Only one rooted quarantine can form; no next child after it. Maximize both8/11-field constructor schedules:8 can grow while11 does not. Source-cleared arrays are still retained.')
 add('marker_map',int(created),hmap(6)+3*24,[933],'Six literal fields; at most3 newly controlled numeric referents.')
 if created:
  witnesses=[2,3,3,3,2,5,4,5,5,5]
  control_count=10 if parsed else 7 if c else 6
  add('runner_actual_control_maps',control_count,hmap(3),[300,937,939,940,941,942,951,962,966,967,968],'Actual Expect result maps initially have3 fields.')
  add('runner_actual_witness_maps',1,sum(hmap(n) for n in witnesses[:control_count]),[937,939,940,941,942,951,962,966,967,968],'One actual caught rejection witness per reached control.')
  add('runner_expected_witness_maps',1,sum(hmap(n) for n in witnesses[:control_count]),[937,939,940,941,942,951,962,966,967,968],'Independent expected-witness literal passed to each control, even when references agree.')
  # All dynamic witness primitive stores are bounded by the actual field
  # count. H/J/PE alias referents are deliberately excluded from this row.
  add('runner_witness_new_numeric_expression_upper',1,24*(sum(witnesses[:control_count])- (2 if control_count>=6 else 0)-(4 if control_count>=7 else 0)),[937,939,940,941,942,966,967,968],'Relaxed primitive-store upper; exclude known predecessor H and GUI PE map aliases.')
  add('runner_control_grown_Hashtable_backings',int(control_count>=7),hmap(3,6)-hmap(3),[951,962],'Predecessor3->5 needs no extra backing; GUI3->6 crosses loadsize and grows once.')
  add('runner_controls_List',1,exactlist(10),[935],'Explicit source capacity10, no growth.')
  add('runner_controls_ToArray6',1,arr(6,8),[952],'Initial six real controls.')
  add('runner_controls_ToArray10',int(postverify),arr(10,8),[980],'Final ten real controls.')
  add('mutation_fourfield_maps',1+(1 if c else 0),hmap(4)+24,[508],'Each source offset can become one newly controlled numeric reference; path aliases P.')
 add('reference_family_original_containers',r,56*hmap(5)+growlist(56)+arr(56,8)+growlist(56)+2*arr(56,8)+hmap(3)+24,[805,807],'Per GetReferences:56 rows, row/name Lists, row array, two name arrays,3-field wrapper and one stored summed-byte scalar. Per-file byte counts borrow H.')
 add('build_family_original_containers',b,3*hmap(4)+growlist(3)+arr(3,8)+growlist(3)+arr(3,8)+hmap(2)+24,[866,868],'Per GetBuildArtifacts:3 rows, rows/seen Lists and arrays,2-field wrapper and one stored summed-byte scalar. Per-file byte counts borrow H.')
 add('PE_header_sevenfield_maps_and_stored_numeric_results',4 if c else 1 if created else 0,hmap(7)+6*24,[811],'Each returned PE map has7 fields/six fresh numeric results; captured byte arrays belong to A and path strings to P.')
 add('reference_comparison_old_Hashtable',int(r>1),hmap(0,56),[809],'Empty literal then56 actual writes; all old Hashtable backings retained.')
 add('dependency_projected_threefield_map',int(bool(c)),hmap(3)+3*24,[885],'Only3 admitted fields survive; conservative scalar referent bound3.')
 for n in ([3] if parsed==1 else [3,11] if parsed>1 else []):
  add('native_control_record_maps_'+str(n),n,hmap(3),[843],'Source map projects control/gate/operands; strings borrow J.')
  add('native_control_operand_arrays_'+str(n),n,arr(6,8),[841],'One six-object normalized operand array per admitted control.')
  add('native_BigInteger_normalization_'+str(n),6*n,32+32+24,[382,384],'Per possible BigInteger operand:MaxValue constructor bits32, conservative explicit returned value box32,normalizedUInt64box24. Not J b20.')
  add('native_control_records_List_'+str(n),1,exactlist(n)+arr(n,8),[831,860],'Exact records List and one ToArray.')
  add('native_summary_result_empty_then13',1,hmap(0,13),[858,860],'Empty Hashtable receives twelve validated fields plus controls; J scalar values are borrowed.')
 add('secondary_codes_per_receipt',p['receipts'],exactlist(11)+arr(11,8)+11*(hmap(3)+24),[407,408,409,738],'Each footer projects at most11 first-secondary maps and count referents; every replacement retained.')
 add('failure_original_rows_and_root',int(created),growlist(78)+arr(78,8)+78*hmap(5)+hmap(7)+82*24,[888,890,892],'78 output-file rows, List/array/root; bytes/reparse borrow W,opaque_temporary and root scalar stores have finite upper82.')
 if postverify:
  add('control_file_descriptor_family',1,growlist(4)+arr(4,8)+4*hmap(4),[976,981],'Four old/live control descriptors; bytes borrow H.')
  add('expected_observed_run_name_lists',1,growlist(67)+arr(67,8)+growlist(78)+arr(78,8),[977,978,979],'67 declared files; first-refusing observed list may reach78.')
 if late:
  add('artifact_List_and_ToArray',1,growlist(67)+arr(67,8)+4*hmap(4),[981,982,985],'67 aliases plus4 stage-created descriptor maps; borrowed earlier row objects are not copied.')
  add('transitive_List_and_rows',1,growlist(1225)+arr(1225,8)+1225*hmap(4)+24,[984,985],'Exactly1225 four-field new descriptor maps; input/own byte values borrow parsed input. One GetItem manifest-length numeric result is promoted into its descriptor.')
  add('bundle_ordered_root',1,ordered(21)+10*32+12*24,[985,990],'Root21 entries; predecessor descriptor is borrowed from its earlier owner; at most10 fixed-point ordered-entry replacements and12 newly stored builtin values.')
 return rows

# Value/key node count and Add-TaskJsonBound calls follow the whole output
# topology. Every nonempty map/array adds its delimiter once, each key once,
# each nonfirst member once, every scalar once, and finalLF once.
def preflight_profile(g):
 c=g['counts'];mh={int(k):v for k,v in g['map_field_histogram'].items()};ah={int(k):v for k,v in g['array_length_histogram'].items()}
 maps=c['maps'];arrays=c.get('arrays',0);keys=c['keys'];elements=c.get('array_elements',0)
 values=1+keys+elements;nodes=values+keys
 adds=values+keys+sum(n*max(0,k-1) for k,n in mh.items())+sum(n*max(0,k-1) for k,n in ah.items())+1
 return dict(values=values,nodes=nodes,AddJsonBound_calls=adds,
  stored_numeric_counter_boxes=24*(nodes+adds),
  source_Map_enumerators_and_DictionaryEntry_returns=80*maps+32*keys,
  state_and_result_maps=hmap(3)+hmap(4),one_cap_scalar_referent=24)
pf={n:preflight_profile(g) for n,g in first['output_topologies'].items()}
assert {n:(r['nodes'],r['AddJsonBound_calls']) for n,r in pf.items()}==dict(marker=(13,19),receipt=(765,1149),failure=(873,1269),bundle=(11795,17043))

fixed_arrays=dict(initial_three_arg_reference_array=arr(3,8),suffix_array7=arr(7,8),own_source_names4=arr(4,8),
 secondary_sites11=arr(11,8),secondary_raw11=arr(11,8),secondary_long_counts11=arr(11,8),
 signed_thresholds_typed18=arr(18,8),signed_thresholds_original_literal18=arr(18,8)+18*24,
 unsigned_thresholds_typed19=arr(19,8),unsigned_thresholds_original_literal19=arr(19,8)+19*24,
 summary_fields12=arr(12,8),initial_receipt_empty_arrays2=2*arr(0,8),
 hash_buffer65536=arr(65536,1),bootstrap_pin_nested_arrays=arr(7,8)+7*arr(3,8)+7*24,
 bootstrap_hook_names7=arr(7,8),bootstrap_codec_pair=arr(2,8),expected_controller_argv14=arr(14,8),
 fixed_bootstrap_digest_hex7=7*(arr(32,1)+a8(22+128)),capture_digest_hex2=2*(arr(32,1)+a8(22+128)))
phases=[]
for p,q in zip(profiles,components['phases']):
 rows=original_rows(p)
 schedule=dict(marker=p['marker'],receipt=p['receipts'],failure=p['failure_inventory'],bundle=5*p['bundle'])
 P=sum(schedule[n]*sum(v for k,v in pf[n].items() if k in ['stored_numeric_counter_boxes','source_Map_enumerators_and_DictionaryEntry_returns','state_and_result_maps','one_cap_scalar_referent']) for n in pf)
 # Remaining explicitly written source byte/reference arrays; VM-only
 # argument frames are excluded. This is not a claim to close all A paths.
 A=dict(fixed_definition_arrays=sum(fixed_arrays.values()),
  raw_grammar_state_arrays=32+32+(56 if p['children'] else 0)+(96 if p['marker'] else 0)+32*(4 if p['native_result_parses'] else 0)+32*(12 if p['native_result_parses']>1 else 0),
  raw_byte_control_encoded_arrays=(3*arr(21,1) if p['marker'] else 0),
  owned_copy_buffers=(arr(65536,1)*(2 if p['children'] else 1) if p['marker'] else 0),
  PE_captured_arrays=(arr(64,1)+arr(24,1)+arr(4096,1))*(4 if p['children'] else 1 if p['marker'] else 0),
  task_read_reference_concat_conversion_arrays=256*p['children'],
  child_sink_and_owner_literal_arrays=(arr(2,8)+arr(4,8))*p['children'],
  mutation_before_replacement_arrays_and_hex=(32+88+64 if p['marker'] else 0)+(32+32+64 if p['children'] else 0),
  compiler_and_native_argument_arrays=(arr(28,8) if p['children'] else 0)+arr(6,8)*p['native_result_parses'],
  BigInteger_cold_static_bits32=(32 if p['children'] else 0),
  dependency_captured_digest_and_hex=(208 if p['children'] else 0),
  SDK_BigInteger_controlled_stack_peak=(534 if p['children'] else 0))
 terms=dict(previous_component_selected=q['selected_partial'],O_original_DTOs=sum(r['requests'] for r in rows),
  O_preflight_private_state_boxes_maps_and_iterator_returns=P,F_A_explicit_arrays_and_bytes_selected=sum(A.values()))
 phases.append(dict(id=p['id'],original_rows=rows,preflight_measure_schedule=schedule,preflight_requests=P,selected_explicit_arrays=A,
  selected_terms=terms,selected_partial=sum(terms.values())))
out=dict(schema='R12_ORIGINAL_DTO_PREFLIGHT_AND_FIXED_ARRAY_WORKING_R1',status='SOURCE_SELECTED_PARTIAL_NOT_WHOLE_UPPER',
 inputs=[dict(path=(HERE/n).relative_to(ROOT).as_posix(),sha256=digest(HERE/n)) for n in ['evaluation.json','components.json']],
 literal_and_fixed_array_profile=fixed_arrays,preflight_per_document=pf,phases=phases,
 important_correction='Private Hashtable state stores a fresh numeric result on every nodes++ and bytes+=; these source-controlled boxes were absent from prior worksheet. Explicit map enumerators and boxed DictionaryEntry returns remain controlled, as in K.',
 remaining_A_F='This evaluated explicit table is not all source transfer work. Full remaining includes actual bounded FileSet/key-array/source-pipeline materializations, private catalogue dictionaries/maps and original literal strings. No blanket per-helper binder fee or zero is asserted.',
 unresolved_V='Selected FileStream/crypto/pipe/process/async/encoding wrappers and native-child payload still require source coefficients and phases. FileInfo136 and b20 source components are pending independent review.',
 no_claims=['No target execution','No source/cap/domain change','No complete numeric U or fit/NOFIT','No GC or physical-process-heap claim','No final union/ABI/environment binding or GO'])
(HERE/'originals.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(preflight=pf,fixed_arrays=sum(fixed_arrays.values()),phases=[dict(id=p['id'],O=p['selected_terms']['O_original_DTOs'],preflight=p['preflight_requests'],F_A=p['selected_terms']['F_A_explicit_arrays_and_bytes_selected'],partial=p['selected_partial']) for p in phases]),indent=2))
