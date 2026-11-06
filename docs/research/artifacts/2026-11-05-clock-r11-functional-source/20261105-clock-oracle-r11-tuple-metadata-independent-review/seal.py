"""Seal this manually reviewed source argument and ordinary-file evidence only."""
from pathlib import Path
from hashlib import sha256
import json

HERE=Path(__file__).resolve().parent
REPO=HERE.parents[1]
def read(path): return json.loads(path.read_bytes())
def pin(path):
    p=Path(path); b=p.read_bytes()
    return {'path':p.relative_to(REPO).as_posix() if p.is_relative_to(REPO) else str(p).replace('\\','/'),'bytes':len(b),'sha256':sha256(b).hexdigest().upper()}
def write(name,value):
    (HERE/name).write_text(json.dumps(value,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
e=read(HERE/'mechanical-evidence.json')
assert not e['failed'] and e['check_count']==5331
all_pins=read(HERE/'checked-input-pins.json')
subject=[r for r in all_pins[:5]]
predecessor=all_pins[5:10]
verdict='ACCEPT_WHOLE_FUNCTIONAL_SOURCE_ONLY_PROVISIONAL_ADMISSION_NUMERIC_PAYLOAD_BINDING_EXECUTION_HOLD'
review={
 'schema':'R11_TUPLE_METADATA_INDEPENDENT_WHOLE_FUNCTIONAL_REVIEW_R1',
 'verdict':verdict,
 'subject_manifest_sha256':subject[4]['sha256'],
 'runner_sha256':subject[1]['sha256'],
 'whole_source_review':{'runner_lines':976,'cpp_lines':289,'header_lines':72,'README_lines':259,'manifest_complete_JSON_read':True,'checks':e['check_count'],'failures':[]},
 'source_correspondence':{
  'complete_inverse_runner_byte_identity_to_accepted_R3':True,
  'CPP_header_byte_exact_R3':True,
  'new_metadata_tuple_constructors':1,
  'Item_reads':25,
  'consumer_lines':[{'line':r['line'],'reads':r['reads'],'items':r['items']} for r in e['consumer_lines']],
  'GetTaskEntries_call_sites':7,
  'extra_count_only_call':True,
  'fresh_filesystem_checks_preserved':True,
  'partial_Destination_append_and_primary_cleanup_order_preserved':True,
  'manual_wire_projections_preserved':True,
  'metadata_rows_serialized_directly':False,
  'reference_descriptor_field_count':5,
  'new_entry_or_JSON_domain_restrictions':False,
  'new_stat_hash_trust_cache':False,
  'normal_runner_controls':10,
  'normal_cpp_control_emissions':14,
  'target_control_execution':False,
  'all_guards_false':True,
  'manifest_changed_paths':e['manifest_changed_paths'],
 },
 'public_tuple_and_enumeration_source':{
  'Tuple5_implements_IEnumerable_or_IEnumerator':False,
  'five_readonly_cells_assigned_and_read_directly':True,
  'selected_PowerShell_commit':'ea554a8f9c6085f54fb2828f7ea286c65c35599f',
  'LanguagePrimitives_sha256':'284B0FAD37EF182C51566E8135B20E4C27464B1EE9352A1786E7DC76C3089711',
  'Binders_sha256':'CCA6BC97C2CDC16FDC1EB612FABCCCEA919629BC4C04F952C00A458FF7C9F6AC',
  'direct_or_PSObject_wrapped_tuple_is_scalar_under_selected_source_branch':True,
  'target_binder_executed':False,
  'source_binary_equivalence_claim':False,
 },
 'profile':e['profile'],
 'bootstrap':{'bytes':subject[4]['bytes'],**e['bootstrap']},
 'admission_qualification':'Exact sealed provisional predecessor R3 original-path input and namespace set only. Root must settle the final canonical executable/SDK/source/contract/census/review/resource/binding union and recompute complete profile before numeric authority.',
 'resource_statement_scope':'432/carrier and 8360*432 lower-request arithmetic described in subject/audit only; not a new numerical, physical-live, reclamation, layout, RSS or32MiBfit certificate from this functional review.',
 'closed_functional_findings':[],
 'blocking_functional_findings':[],
 'holds':['final_canonical_original_path_input_union','complete_R11_runner_and_both_child_scalar_census','all_six_controlled_success_failure_environment_lifetime_joins','independent_complete_numeric_review','whole_derived_U','typed_immutable_identity_storage_profile_binding','separate_root_execution_GO','compiler_CPU_heap_RSS_temporary_disk_qualification','production_DSP_native_Ready_renderer_hardware'],
 'old_prices_or_path_geometry_transferred':False,
 'reviewer_checker_history':[
  {'step':'initial_unsealed','failure':'KeyError file_count: older namespace rows legitimately omit optional file_count; files list is authoritative.','correction':'Compare actual file count to file_count when present and otherwise to len(files).','subject_changed':False},
  {'step':'next_unsealed','failure':'Guard regex used unprefixed variable names instead of actual task-prefixed names.','correction':'Check actual eight task-prefixed false guard assignments.','subject_changed':False,'preserved_evidence':'evidence-first-unsealed/'},
 ],
 'boundary':'Independent ordinary file/text/JSON/hash/integer arithmetic and two fixed public-source reads only. No target PS parser/AST/import/helper/Add-Type/compiler/program/stage/native/GUI/device/Git/CI. No subject or predecessor mutation.',
}
write('review.json',review)
evidence_paths=sorted(p for p in HERE.rglob('*') if p.is_file() and p.name!='source-pins.json')
write('source-pins.json',{
 'schema':'R11_TUPLE_METADATA_INDEPENDENT_REVIEW_SOURCE_PINS_R1','verdict':verdict,
 'subject':subject,'accepted_predecessor_subject':predecessor,
 'author_pins':next(r for r in all_pins if r['path'].endswith('clock-oracle-r11-tuple-metadata-source-authoring-r1/source-pins.json')),
 'evidence':[pin(p) for p in evidence_paths],
 'all_ordinary_read_inputs':'checked-input-pins.json records every read identity, including all1220 admitted original paths and selected public source associations. No content dedup substitutes for a called physical path.',
 'scope':'Whole functional source only. Provisional union and all numeric/payload/binding/execution gates remain HOLD. No self hash is embedded; caller pins this external record.',
})
for row in read(HERE/'source-pins.json')['subject']+read(HERE/'source-pins.json')['accepted_predecessor_subject']+read(HERE/'source-pins.json')['evidence']:
    assert pin(REPO/row['path'])==row,row['path']
print(json.dumps({n:pin(HERE/n) for n in ['review.md','review.json','source-pins.json']},indent=2))
