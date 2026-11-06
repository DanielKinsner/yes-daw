"""Seal source-only whole review; never execute the subject or its helpers."""
from pathlib import Path
from hashlib import sha256
import json

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SUB=ROOT/'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1'
def pin(p):
 p=Path(p);b=p.read_bytes()
 return {'path':p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),'bytes':len(b),'sha256':sha256(b).hexdigest().upper()}
def write(n,x):(HERE/n).write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8',newline='\n')
e=json.loads((HERE/'mechanical-evidence.json').read_bytes())
assert e['check_count']==105 and e['failures']==[]
v='REVISE_WHOLE_CONDITIONAL_JOIN_UNCLOSED_NATIVE_OUTPUT_CAP_DOMAIN_BRANCHES'
review={
 'schema':'R11_SIX_PHASE_PAYLOAD_WHOLE_INDEPENDENT_SOURCE_REVIEW_R1',
 'verdict':v,
 'subject_manifest_sha256':'4D8B292BA5096826E21E9F16806CFDF36C26383DFCD35FFF67927FFA692AB61D',
 'report_sha256':'E247A5479F986165C8554BBD1C2CBF343CFC75F2D7277C4AE5F7913926D97930',
 'phases_sha256':'EB245E1845CB1FBCF49234228DE313B6277F9473BFF811AB5F92EDDF99B91526',
 'arithmetic_checks':105,'arithmetic_failures':[],
 'whole_source_verdict_is_not_arithmetic_pass':True,
 'blocking_findings':[
  {'id':'F1','title':'Main-only native output law is not a full admitted-output/failure qualification',
   'source':'run-clock.ps1 Invoke-TaskChild158..232,Get-TaskStageBytes690..692,Read-TaskChildResult788..836,main/finally/failure source paths',
   'mechanism':'Every child retains up to1MiB through4096-minimum reads, permitting257 positive stage admissions. ReadChildResult only bounds stdout<=65536, skips empty LF slices and never rejects stderr. Native crash/loader/extra-success-noise output is not closed by the own-main1064/2662/1294 law.',
   'affected_phases':['P3','P4','P5'],
   'required':'Keep the existing small-output branch conditional; join finite cap-domain alternatives or separately prove an exact supported output premise. No source narrowing/cap change.',
   'cap_domain_alternatives':e['cap_domain_alternatives'],
   'simpler_failed_exit_alternatives':{'failed_producer_stage_calls':521,'failed_verifier_after_quiet_producer_stage_calls':524},
   'affected_selected_and_residual_families':['W tuple/queued-map/stored-scalar families','A stage-six-name arrays','W/A List/Queue/ToArray/pipeline owners','P/V fresh paths/iterators','E refusal/footer owners','R native original captures and source/slice strings','J native reader/StringBuffer arrays and parsed/instance owners'],
   'no_total_undercount_or_NOFIT_inference':True},
  {'id':'F2','title':'Residual exclusions do not explicitly restore the excess selected owners',
   'mechanism':'Delta excludes selected row/box/stage-array and native capture/reader subfamilies after charging only main-law constants. A V stream-buffer or J generic-graph/instance term does not itself specify their missing excess.',
   'required':'Recompose affected branches or add explicit nonzero source-indexed excess equations, including all additional container/copy/iterator/path/error ownership. Other loose upper slack is no proof of the separately stated counts.',
   'same_underlying_output_premise_as_F1':True}
 ],
 'phase_assessment':[
  {'id':r['id'],'nominal_typed_partial':r['typed_partial'],'nominal_arithmetic_reproduced':True,'count_scope':('Source-supported counted-family prefix; all residuals still unclosed' if r['id'] in ['P0','P1','P2'] else 'Conditional on unclosed native quiet-output law; cap-domain alternative required')} for r in e['nominal_phase_recompute']
 ],
 'supported_partial_equations':{
  'fixed_data_lexical_scans':e['fixed_lexical_recompute'],
  'fixed_graph_subtotals':[r['subtotal'] for r in e['fixed_graph_recompute']],
  'reader_fixed_capacity':8192,'reader_fixed_all_arrays_each':30816,
  'reader_generic_report_U':21966,'reader_generic_report_capacity':262144,'reader_generic_report_old_arrays':1046744,
  'native_16_initial_arrays_33152':'Main-only wire conditional, not universal admitted-output branch',
  'full_reader1141528_and_StringBuffer6216':'Nominal arithmetic only pending the native-output premise',
  'hashtable8_insertions':'11 buckets then23; unused OrderedHashtable base bucket arrays included',
  'layout_scope':'Selected ordinary x64 source/compiled-field-shape association; conservative class-slot/value-packing uppers, not observed offsets or physical class sizes',
  'iterator_scope':'Actual JObject state/KeyValuePair/this/inner fields; List reference enumerator24-value/40-boxed selected sequential case; other iterators and instance/stack owners remain named residuals',
  'source_owner_candidates187':'Exact text marker index reproduces; not a complete source allocator/binder census',
  'residual_families13':'All named and not assigned0; their index is not thirteen evaluated finite equations or a closed allocation ownership proof',
  'normal_hashes9936_FileSet259_clone11082_writer_document34410869':'Nominal worksheets/arithmetic, not a whole source or capacity acceptance',
 },
 'remaining_source_qualifications':[
  'Every13-family site/capacity/copy/box/iterator/codec/exception-witness ownership and disjoint subtraction; restore all cap-domain excess',
  'Generic dependency and native parsed graphs,UInt64/BigInteger extra owners,reader/writer/position-stack/serializer/settings source owners',
  'Exact PS literal/compiler/pipeline/binder/typed/ToArray copies remain controlled payload despite opaque primitive CPU/general heap',
  'Canonical original-path input/source/contract/census/review/resource/binding union and fixed-root catalogue geometry; no old1458/187423/453759 transfer',
  'Successful and first-refusing directory/path/native-pool capacities,unknown andtilde fallback,all fresh filesystem/hash checks',
  'Per-child incoming native environment/key/value/Hashtable/Dictionary capture plus poststartup/import/config writers;14-key seed and8-key outgoing set are not incoming bounds',
  'Explicit gate/witness/Data carrier/DTO/footer/copy terms stay nonzero controlled obligations; opaque Exception/ErrorRecord general heap remains unqualified under exact accepted contract',
  'All request lifetimes are cumulative; no null/scope/Clear/Dispose/ReturnBuffer/assumedGC physical release credit',
 ],
 'classification':{'global_numeric_upper':False,'fit':False,'NOFIT':False,'actual_coexisting_lower':False,'physical_RSS_or_reclaim':False,'source_prices_or_Data_binding':False,'execution_GO':False},
 'preservation':'Subject,all earlier packets and target source unchanged; provisional R11 allguardsfalse/unrun. No source/domain/cap amendment authorized by this review.',
 'operations':'Independent ordinary file/source/data JSON/hash/string/integer arithmetic only. No subject helper imported/executed; no target PS parse/AST/import/compiler/program/stage/assembly/native/GUI/device/Git/CI.',
}
write('review.json',review)
extra=[
 'build-ci/20261105-clock-oracle-separate-research-contract/contract.md',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/ArrayList.cs',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/OrderedDictionary.cs',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JTokenWriter.cs',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JsonSerializerInternalReader.cs',
]
write('source-pins.json',{
 'schema':'R11_SIX_PHASE_PAYLOAD_INDEPENDENT_WHOLE_REVIEW_PINS_R1','verdict':v,
 'subject_pins':pin(SUB/'source-pins.json'),'verified_subject_and_input_files':e['verified_input_pins'],
 'additional_source_reads':[pin(ROOT/x) for x in extra],
 'review_evidence':[pin(p) for p in sorted(HERE.iterdir()) if p.is_file() and p.name!='source-pins.json'],
 'scope':'Whole REVISE of conditional join. Nominal arithmetic and fixed source/data partial equations only; no fit/NOFIT/global numerical upper, Data orGO. Subject unchanged.',
})
allrows=json.loads((HERE/'source-pins.json').read_bytes())
for row in [allrows['subject_pins']]+allrows['verified_subject_and_input_files']+allrows['additional_source_reads']+allrows['review_evidence']:
 assert pin(ROOT/row['path'])==row,row['path']
print(json.dumps({n:pin(HERE/n) for n in ['review.md','review.json','source-pins.json']},indent=2))
