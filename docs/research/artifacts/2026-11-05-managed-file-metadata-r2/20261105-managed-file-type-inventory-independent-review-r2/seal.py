"""Seal this source review only; no subject helper or assembly is executed."""
from pathlib import Path
from hashlib import sha256
import json

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
def pin(p):
    p=Path(p);b=p.read_bytes()
    return {'path':p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),'bytes':len(b),'sha256':sha256(b).hexdigest().upper()}
def write(name,value):(HERE/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
e=json.loads((HERE/'mechanical-evidence.json').read_bytes())
assert e['count']==48 and e['failures']==[]
for row in e['verified_files']:assert pin(ROOT/row['path'])==row
v='ACCEPT_SOURCE_ONLY_PUBLIC_FILE_METADATA_R2_NOT_EXECUTION_OR_LAYOUT_QUALIFICATION'
review={
 'schema':'FIXED_MANAGED_FILE_TYPE_INVENTORY_R2_INDEPENDENT_SOURCE_REVIEW',
 'verdict':v,
 'subject_manifest_sha256':'72F84E45F43CB4ED86E91B6C1CCB464A904E043E5338FAF941C47284E69DC7DC',
 'reader_sha256':'9215EED3EDAC196C983353ADC68D1BA95FCD0C4833ED1130FB7C8F39CCC8D348',
 'whole_reader_lines':377,'whole_reader_bytes':22475,
 'verified_files':e['verified_files'],
 'mechanical_checks':48,'failures':[],
 'source_correspondence':{
  'original_four_input_pins_and_21_names_exact':True,
  'Main_Fail_and_signature_field_decoder_byte_exact':True,
  'same_handle_file_hash_metadata_gate_byte_exact':True,
  'author_diff_matches_independent_diff':True,
  'new_walk':'Direct JObject child-name metadata, one unique exact-prefix/nonempty-decimal generated enumerator, three exact optional CoreLib type presence/field inventories.',
  'reported_types_min':22,'reported_types_max':25,
  'optional_absence_fabricates_field_row':False,
  'target_assembly_explicit_load_instance_reflection_constructor_method_IL_native_env_or_GUI_added':False,
  'frozen_R1_changed':False,
 },
 'publication':'Complete metadata preparation is buffered before one stdout write. Root retains ordinary output-transport failure/exit and complete-JSON checks; no transactional stdout guarantee.',
 'execution':{'reviewer_checker_only':True,'CSharp_compile':False,'reader_run':False,'AddType':False,'target_parse_import_load':False,'native_GUI_Git_CI':False},
 'holds':['actual_compiler_reference_host_and_hidden_launcher_review','actual_metadata_execution_and_raw_receipt','runtime_object_size_offset_padding_allocation_or_reclaim','32MiB_payload_fit','R11_complete_census_profile_binding_execution_GO'],
 'scope':'Whole functional source acceptance of this separate public-file metadata tool. No target execution or physical-resource approval.',
}
write('review.json',review)
write('source-pins.json',{'schema':'MANAGED_FILE_METADATA_R2_INDEPENDENT_SOURCE_REVIEW_PINS','verdict':v,'subject_manifest_sha256':review['subject_manifest_sha256'],'subject':e['verified_files'],'evidence':[pin(p) for p in sorted(HERE.rglob('*')) if p.is_file() and p.name!='source-pins.json']})
for row in json.loads((HERE/'source-pins.json').read_bytes())['evidence']:assert pin(ROOT/row['path'])==row
print(json.dumps({n:pin(HERE/n) for n in ['review.md','review.json','source-pins.json']},indent=2))
