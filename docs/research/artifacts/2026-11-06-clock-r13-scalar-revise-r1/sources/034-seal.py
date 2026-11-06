"""Seal an independent source/data critique; no subject execution or mutation."""
from pathlib import Path
import hashlib
import json

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]

def sha(b): return hashlib.sha256(b).hexdigest().upper()
def load(name): return json.loads((HERE/name).read_bytes())
def pin(p):
    b=p.read_bytes()
    return dict(path=p.relative_to(ROOT).as_posix(),bytes=len(b),sha256=sha(b))
def write(name,obj):
    p=HERE/name
    if p.exists(): raise ValueError('Preserve existing seal/output '+name)
    p.write_text(json.dumps(obj,indent=2)+'\n',encoding='utf-8',newline='\n')

evidence=load('checks.json')
assert evidence['count']==2123 and evidence['failed']==[]
assert all(row['passed'] for row in evidence['checks'])
ordered=load('ordered-ledgers.json')
assert len(ordered['all74coverage'])==74 and len(ordered['ledgers'])==22
callers=load('caller-inventory.json')
assert len(callers['occurrences'])==330 and len(callers['multiplicity_gaps'])==14
phases=load('phase-arithmetic.json')
assert phases['phases'][-1]['proposal_sum']==5536151256

inputs=load('verified-inputs.json')['files']
for row in inputs:
    p=Path(row['path'])
    if not p.is_absolute():p=ROOT/p
    b=p.read_bytes()
    assert (len(b),sha(b))==(row['bytes'],row['sha256']),row['path']

navigation=load('navigation-corrections.json')
runner=(ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1/run-clock.ps1').read_text().splitlines()
for row in navigation['rows']:
    assert runner[row['correct_navigation_line']-1]==row['exact_source']

first=['check.py','ordered-ledgers.json','caller-inventory.json','phase-arithmetic.json','checks.json','verified-inputs.json']
write('first-green-review-helper-freeze.json',dict(
    schema='INDEPENDENT_REVIEW_FIRST_COMPLETE_GREEN_FILE_DATA_CHECKER_FREEZE_V1',
    checks=2123,failed=0,files=[pin(HERE/n) for n in first],
    scope='File/text/JSON/hash/integer self-check only, no target parser/helper/compiler/program/stage or runtime execution.',
    preserved_attempts=['attempt-r1','attempt-r2'],
    navigation_only_correction=pin(HERE/'navigation-corrections.json'),
    source_subjects_changed=False,prices_accepted=False))

findings=[
 dict(id='F1',kind='EXPLICIT_TYPE_RECEIVER_ARGUMENT_RESULT_SPONSORSHIP',ledgers=['L01','L02','L03','L04','L05','L06','L08','L09','L10','L11','L12','L13','L14','L15','L16'],
      requirement='Rebuild all74 helper and main/error ordered expressions with exact type/receiver/argument/operator/result roles. No local top-up or inherited operand acceptance.'),
 dict(id='F2',kind='NULLABLE_ARGUMENT_LIFT',ledgers=['L07'],
      requirement='Count one caller-side Int32 to Nullable<Int32> MaxDepth conversion; no field/body/allocation expansion.'),
 dict(id='F3',kind='WHOLE_PINS_FIXED_AND_ROWS',ledgers=['L08','L09','L17'],
      requirement='Replace the unsupported fixed90 basis with complete fixed source expansion; own4/input/namespace rows stay separate. Candidate local fixed96, own453,input37 are not accepted whole prices.'),
 dict(id='F4',kind='STATIC_PROPERTY_GETTER_CONSTRUCTOR',ledgers=['L18'],written_sites=13,
      requirement='Reconcile all13 StringComparer getter occurrences. Property getter5 plus constructor5=10; distinguish enum/static data and instance getter roles.'),
 dict(id='F5',kind='RAW_GRAMMAR_COMPONENT_TYPE_READ',ledgers=['L19'],
      requirement='Raw byte literal group37 omits TypeR1. Rebuild full grammar token classes and global charging coefficient; do not select588 merely from this one delta.'),
 dict(id='F6',kind='FOREACH_SOURCE_FORMATION',ledgers=['L20','L21','L22'],
      requirement='Count actual enumerable expression and explicit literal array slots in addition to foreach setup/terminal/exit. Reconcile all catalogue first-build/caller schedules.'),
 dict(id='F7',kind='INCOMPLETE_CALLER_INVENTORY',actual_occurrences=330,recorded_occurrences=315,deficient_locations=14,
      requirement='Inventory repeated main-line occurrences and zero-argument calls at end-of-line. Reconcile all occurrences to existing family sponsors; do not add fifteen duplicate charges.'),
 dict(id='F8',kind='COEXISTING_FRESH_REFUSALS',frozen_max=3,source_possible=4,
      requirement='Join prior caught predecessor refusal with native prePins, finallyPins and same-finally captured-binary refusals. No silent immutable-files premise or undocumented cross-family slack.'),
]
write('review.json',dict(
 schema='R13_COMPLETE_RUNNER_SCALAR_INDEPENDENT_WHOLE_REVIEW_V1',
 verdict='REVISE_WHOLE_SCALAR_SOURCE_PROPOSAL',
 subject_manifest_sha256='10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3',
 subject_runner_sha256='9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788',
 subject_census_sha256='9DAEFFB03433D0A6D215054462A69D67A2DD79B9A1267F747B70C84DA9EC7024',
 subject_pins_sha256='13616A59500136DE3801E5EEF565C582FFD6588322A4AF19158720887634F32D',
 subject_whole_freeze_sha256='B64ACA5B72E5BE414C5ECE0714DB3BF7C5BC75FDC240E81130ADE2CA434DA67A',
 validation=dict(checks=2123,failed=0,helpers_whole_source=74,admitted_inputs=1220,
                 actual_call_occurrences=330,ordered_local_counter_ledgers=22,
                 composition_families=54,phase_prefixes=6,
                 exact_author_sum_reproduced=5536151256,
                 source_input_pins_pre_seal_reverified=len(inputs)),
 findings=findings,
 candidate_local_ledgers=pin(HERE/'ordered-ledgers.json'),
 authoritative_navigation_correction=pin(HERE/'navigation-corrections.json'),
 all74coverage=ordered['all74coverage'],
 all54_six_phase_arithmetic='Reproduced unaccepted proposal exactly; arithmetic does not certify source-factor ownership.',
 joined_failure_schedule=phases['fresh_refusal_schedule'],
 scope=dict(file_text_json_integer_only=True,PowerShell_parser_or_AST=False,
            author_helpers_executed=False,target_import_helper_compiler_program_stage_native_GUI_Git_CI=False,
            subjects_changed=False,earlier_evidence_preserved=True,
            production_conventions_used=False,hidden_CLR_or_SDK_CPU_expanded=False),
 acceptance=dict(authored_5536151256_accepted=False,individual_price_acceptance=False,
                 unaffected_prices_certified=False,corrected_whole_upper_selected=False,
                 Data_defined=False,work_limit_selected=False,resource_fit=False,
                 source_binary_or_native_qualification=False,binding_or_Clock_GO=False),
 next_required='Complete regenerated whole proposal under ordered source-role hypothesis, all74 helpers/main/error/defaults/conversions,330callers,54families,six joined prefixes and fresh whole independent numeric critique. Preserve all frozen predecessors.'))

own=[pin(p) for p in sorted(HERE.rglob('*')) if p.is_file() and p.name!='source-pins.json']
write('source-pins.json',dict(schema='R13_RUNNER_NUMERIC_INDEPENDENT_REVIEW_PINS_V1',
 verdict='REVISE_WHOLE_SCALAR_SOURCE_PROPOSAL',own_files=own,verified_inputs=inputs,
 source_subjects_changed=False,target_executed=False,price_or_fit_or_GO=False))

# One complete post-seal read-byte verification, without rewriting any output.
sealed=load('source-pins.json')
for row in sealed['own_files']+sealed['verified_inputs']:
    p=Path(row['path'])
    if not p.is_absolute():p=ROOT/p
    b=p.read_bytes()
    assert [len(b),sha(b)]==[row['bytes'],row['sha256']],row['path']
print(json.dumps(dict(verdict='REVISE_WHOLE_SCALAR_SOURCE_PROPOSAL',checks=2123,
                     sealed_files=len(own),verified_inputs=len(inputs),post_seal_mismatches=0,
                     seals=[pin(HERE/n) for n in ['review.md','review.json','source-pins.json']]),indent=2))
