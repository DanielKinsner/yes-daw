"""File-only evidence seal; does not run any target or model generator."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
def pin(p):
    d=p.read_bytes()
    return dict(path=p.relative_to(ROOT).as_posix(),bytes=len(d),sha256=hashlib.sha256(d).hexdigest().upper())
refs=[
 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1/source-manifest.json',
 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1/run-clock.ps1',
 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1/clock_stage.cpp',
 'build-ci/20261105-clock-oracle-r11-tuple-metadata-source-authoring-r1/source-pins.json',
 'build-ci/20261105-clock-oracle-r10-runner-census/census.json',
 'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/review-pins.json',
 'build-ci/20261105-managed-payload-abi-probe-source-r2/execution-r1/receipt.json',
 'build-ci/20261105-managed-file-type-inventory-source-r1/execution-r2/inventory.json',
 'build-ci/20261105-managed-file-type-inventory-source-r2/source-pins.json',
 'build-ci/20261105-managed-file-type-inventory-source-r2/execution-r1/inventory.json',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/collection-primary-sources.json',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/codec-primary-sources.json',
 'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextReader.cs',
 'build-ci/20261105-clock-oracle-json-primary-sources/powershell-JsonObject.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Hashtable.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Dictionary.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/List.cs'
]
files=[p for p in OUT.rglob('*') if p.is_file() and p.name!='source-pins.json']
phases=json.loads((OUT/'phases.json').read_bytes())
fixed=json.loads((OUT/'fixed-inputs.json').read_bytes())
graphs=json.loads((OUT/'fixed-graphs.json').read_bytes())
checks=[]
def ck(name,a,b):
    checks.append(dict(name=name,actual=a,expected=b,passed=a==b))
ck('six source phase rows',len(phases['phases']),6)
ck('complete phase identifiers',[p['id'] for p in phases['phases']],[f'P{i}' for i in range(6)])
ck('source stage joins',[p['stage_calls'] for p in phases['phases']],[0,7,264,267,270,272])
ck('full hash calls',phases['phases'][-1]['hash_calls_upper'],9936)
ck('all FileSet calls',phases['phases'][-1]['normal_FileSet_calls_with_controls_upper'],259)
ck('full metadata rows',phases['phases'][-1]['walker_appended_rows_upper'],10904)
ck('full stage rows',phases['phases'][-1]['stage_rows_upper'],1635)
ck('typed partial fully reached upper',phases['phases'][-1]['typed_partial_join_upper'],31122780)
ck('all thirteen residual families',len(phases['terms']),13)
ck('fixed input capacities',[x['lexical']['capacity_upper'] for x in fixed['inputs']],[8192,8192])
ck('fixed graph subtotals',[x['graph']['subtotal'] for x in graphs['inputs']],[5143792,863848])
if any(not x['passed'] for x in checks):
    raise ValueError('source/data arithmetic seal check mismatch')
packet=dict(schema='R11_SIX_PHASE_PAYLOAD_CONDITIONAL_SOURCE_PACKET_PINS_R1',
            status='SEALED_FOR_INDEPENDENT_SOURCE_ARITHMETIC_REVIEW_NO_NUMERIC_FIT',
            authored_files=[pin(p) for p in sorted(files)],input_pins=[pin(ROOT/p) for p in refs],
            source_data_cross_checks=checks,
            exact_execution_scope='Only independent Python file/JSON-data/source arithmetic/sealing and public primary-source downloads occurred in this author task. No target PowerShell parser/AST/import/helper,compiler/program/protocol stage,GUI/device,Git/CI; root alone ran separately reviewed finite ABI and file metadata tools.',
            remaining='Complete conditional whole equation with every Delta family named, not a numeric global upper/fit, source-proven NOFIT, physical heap/reclaim, price/binding or GO. Canonical final functional union/fixed-root/env and source coefficients still pending.',
            no_self_hash_cycle='Pins file intentionally not inside its authored-file list. External SHA seals its exact bytes; no executable manifest hash authority is manufactured here.')
(OUT/'source-pins.json').write_text(json.dumps(packet,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(report=pin(OUT/'report.md'),phases=pin(OUT/'phases.json'),pins=pin(OUT/'source-pins.json'),checks=len(checks)),indent=2))
