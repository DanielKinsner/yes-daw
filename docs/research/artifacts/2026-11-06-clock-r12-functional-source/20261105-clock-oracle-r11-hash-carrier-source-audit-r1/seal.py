"""Source/file audit arithmetic only; never imports or runs target helpers."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
SOURCE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1/run-clock.ps1'
raw=SOURCE.read_bytes()
expected='0E41A01150AE5A588D7D42176F6A6C7A6D922F8CE3AE69AD474FDA26E05C7DA9'
if hashlib.sha256(raw).hexdigest().upper()!=expected:raise ValueError('audit source pin changed')
text=raw.decode('utf-8')
calls=[dict(line=i,source=s) for i,s in enumerate(text.splitlines(),1) if 'Get-TaskHash ' in s]
identities=[dict(line=i,source=s) for i,s in enumerate(text.splitlines(),1) if 'Assert-TaskIdentity ' in s]
if len(calls)!=16 or len(identities)!=8:raise ValueError('closed source physical call-site inventory changed')
def pin(p):
    b=p.read_bytes()
    return dict(path=p.relative_to(ROOT).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest().upper())
data=dict(schema='R11_HASH_CARRIER_CLOSED_SOURCE_AUDIT_R1',
 status='RECOMMEND_BOUNDED_INTERNAL_CARRIER_SUCCESSOR_NO_IMPLEMENTATION',
 source=pin(SOURCE),physical_hash_calls=calls,physical_identity_calls=identities,
 conditional_whole_counts=dict(hash_returns=9936,fence_expected_maps=9792,successful_fence_hashes=9800),
 estimated_gross_requests_reduced=dict(return_tuple2=9936*(168-32),scalar_expected_fields=9792*264,skip_success_fence_hex=9800*152,total_before_new_work=9936*(168-32)+9792*264+9800*152,alternative_tuple3_expected_total=9936*(168-32)+9792*(264-40)+9800*152),
 mandatory_preservation=['all original fresh path/reparse/stat/hash/read/EOF/finally occasions and ordering',
 'same BEFORE_AFTER_COMPLETE_INPUT_IDENTITY five-key wire witness',
 'case-sensitive uppercase hex semantics and original scalar integer kinds',
 'all reference/build/control/artifact/receipt/bundle/final stdout descriptor shapes/content',
 'no Tuple/digest-array direct serialization',
 'descriptor-map identity helper remains for two before/after map roles',
 'do not globally replace taskExpected: controller argv and child module maps use the same local variable spelling',
 'no source scalar price/profile transfer; recompose every new argument/helper/loop/temporary allocation'],
 qualification='Source-only consumer/wire route. Tuple2/3 field/layout source correspondence and all new caller/binder work require proof; no claim net allocation delta/fit/live/reclaim/target execution.',
 implementation=False,target_execution=False)
(OUT/'audit.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n')
refs=[SOURCE,
 ROOT/'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/source-pins.json',
 ROOT/'build-ci/20261105-managed-payload-abi-probe-source-r2/execution-r1/receipt.json',
 ROOT/'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/Tuple.cs',
 ROOT/'build-ci/20261105-managed-file-type-inventory-source-r2/execution-r1/inventory.json']
pins=dict(schema='R11_HASH_CARRIER_CLOSED_SOURCE_AUDIT_PINS_R1',
          status=data['status'],files=[pin(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name!='source-pins.json'],inputs=[pin(p) for p in refs],
          execution='Source/file/primary-source/JSON/arithmetic only; no target PS parser/AST/import/helper/compiler/program/stage,GUI/device,Git/CI; no source successor beyond the prior R11 candidate authored.')
(OUT/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(audit=pin(OUT/'audit.json'),pins=pin(OUT/'source-pins.json'),gross=data['estimated_gross_requests_reduced']),indent=2))
