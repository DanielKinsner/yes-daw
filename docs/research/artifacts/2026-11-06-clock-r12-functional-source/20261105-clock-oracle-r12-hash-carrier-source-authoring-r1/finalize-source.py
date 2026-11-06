"""Pre-seal source-doc correction and file-only whole-source seal, never target execution."""
from pathlib import Path
import hashlib,json,shutil
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
NEW=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
OLD=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1'
preserved=OUT/'evidence-initial-unsealed'
if preserved.exists() or (OUT/'source-pins.json').exists():raise ValueError('preserve seal boundary; no rerun')
preserved.mkdir()
for p in [NEW/'README.md',NEW/'source-manifest.json',OUT/'source-correspondence.json']:
 shutil.copyfile(p,preserved/p.name)
old='The original nine runner controls remain byte-exact; the same tenth actual'
new='The original nine runner control IDs/reasons/wire witnesses remain; the same tenth actual'
doc=(NEW/'README.md').read_text()
if doc.count(old)!=1:raise ValueError('current doc repair anchor')
(NEW/'README.md').write_text(doc.replace(old,new),encoding='utf-8',newline='\n')
def pin(p):
 b=p.read_bytes()
 return dict(path=p.relative_to(ROOT).as_posix(),bytes=len(b),sha256=hashlib.sha256(b).hexdigest().upper())
m=json.loads((NEW/'source-manifest.json').read_bytes())
row=next(x for x in m['sources'] if x['path']=='README.md')
row.update(bytes=(NEW/'README.md').stat().st_size,lines=len((NEW/'README.md').read_bytes().splitlines()),sha256=pin(NEW/'README.md')['sha256'])
(NEW/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8',newline='\n')
c=json.loads((OUT/'source-correspondence.json').read_bytes())
c['source_rows']=m['sources'];c['manifest_sha256']=pin(NEW/'source-manifest.json')['sha256']
for check in c['checks']:
 if check['name']=='source row README.md':check.update(actual=row['sha256'],expected=row['sha256'],passed=True)
c['preseal_doc_correction']='Self-review caught inherited byte-exact-control-body wording after fresh identity helper call changes; corrected to unchanged IDs/reasons/wire witnesses. Initial UNRUN README/manifest/correspondence preserved. No runtime/target parser/compile/run occurred.'
(OUT/'source-correspondence.json').write_text(json.dumps(c,indent=2)+'\n',encoding='utf-8',newline='\n')
for row in m['sources']:
 if row['sha256']!=pin(NEW/row['path'])['sha256']:raise ValueError('source pin mismatch')
for n in ['clock_stage.cpp','source-costs.h']:
 if (NEW/n).read_bytes()!=(OLD/n).read_bytes():raise ValueError('protected source changed')
if any(m['guards'].values()):raise ValueError('guard promoted')
if not all(x['passed'] for x in c['checks']):raise ValueError('author check failed')
refs=[
 ROOT/'build-ci/20261105-clock-oracle-r11-tuple-metadata-source-authoring-r1/source-pins.json',
 ROOT/'build-ci/20261105-clock-oracle-r11-hash-carrier-source-audit-r1/source-pins.json',
 ROOT/'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/source-pins.json',
 ROOT/'build-ci/20261105-clock-oracle-r11-hash-carrier-source-audit-r1/IncrementalHash.cs',
 ROOT/'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/Tuple.cs']
files=[p for p in OUT.rglob('*') if p.is_file() and p.name!='source-pins.json']
pins=dict(schema='R12_UNRUN_HASH_CARRIER_SOURCE_AUTHOR_PINS_R1',
 status='SEALED_FOR_INDEPENDENT_WHOLE_FUNCTIONAL_SOURCE_REVIEW_UNRUN',
 subject=[pin(NEW/n) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json']],
 author_evidence=[pin(p) for p in sorted(files)],inputs=[pin(p) for p in refs],
 exact_scope='Fresh Tuple2 count/digest result, scalar expected identity arguments, three closed compare/format/refusal helpers and all audited consumers. Existing Tuple5 rows retained. C++/header exact, same provisional admission/domain/wire/caps;71 helpers. No direct Tuple/digest serialization.',
 qualification='Tuple2 binary/source/layout is PENDING. Tuple5 metadata is not Tuple2 measurement. No old price/resource/root/env/profile transfer. Complete canonical functional union/census/resource fit/binding and separate GO still pending; all source guards false and Data undefined.',
 execution='Ordinary file/Python source authoring/JSON/hash/integer correspondence only. No target PowerShell parser/AST/import/helper/compiler/program/stage,GUI/device,Git/CI invoked.',
 no_first_GREEN='No target syntax/runtime success. Source-only writer checks are not target execution or resource qualification.')
(OUT/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(pins=pin(OUT/'source-pins.json'),correspondence=pin(OUT/'source-correspondence.json'),subject=pins['subject']),indent=2))
