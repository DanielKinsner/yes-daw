"""Prepare a NEW complete proposal from immutable source and prior data lineage."""
from pathlib import Path
from hashlib import sha256
import json
import re

HERE=Path(__file__).resolve().parent
BASE=HERE.parent
ROOT=BASE.parent.parent
OLD=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r2'
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8',newline='\n')
expected={
 'census.md':'E2D319E6CF91F511C368DECB69810E9EAF185E158CD8C84A9C833D6A9BCB82FF',
 'census.json':'8E513B29752936B4CF5E5B2FBA46C936A91396C03F77B9F3ACA03D3BA2FD55C1',
 'source-pins.json':'7020AFCBEE46E5A66D5369D760D4CC0A7C4021BA14D7AED788C84A4DC6E58065',
}
for n,h in expected.items():assert digest(OLD/n)==h,n
rp=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2/review.json'
assert digest(rp)=='AD543B6DBDF99D2C471B7FAD9405B04ED367FA7EB778955EB113152C420600BB'
ledger=json.loads((HERE/'ordered-operands.json').read_bytes())
rows={r['site']:r for r in ledger['rows']}
old=json.loads((OLD/'census.json').read_bytes())
pins=json.loads((OLD/'source-pins.json').read_bytes())
for row in pins['subject']:
 p=ROOT/row['path'];assert p.stat().st_size==row['bytes'] and digest(p)==row['sha256'],row['path']
new=json.loads((OLD/'census.json').read_bytes())
new['schema']='YESDAW_PRODUCTION_FUNDED_ADAPTER_R4_COMPLETE_CENSUS_R3_PROPOSAL'
new['status']='COMPLETE_SOURCE_PROPOSAL_ORDERED_OPERANDS_REBUILT_REQUIRES_FRESH_WHOLE_NUMERIC_REVIEW'
new.pop('correction',None)
new['ordered_body_operand_ledger']={'path':'evidence/ordered-operands.json','rebuilt_affected_bodies':23,'ordered_parts':113,'full_vector_components':10,'earlier_and_callee_sponsorship_explicit':True,'all_420_factorizations_and_all_dependents_regenerated':True,'scope':'Every remaining source price remains a proposal for fresh whole review; prior REVISE grants no positive certificate.'}
write(BASE/'census.json',new)
write(BASE/'source-pins.json',pins)

text=(OLD/'evidence/complete.py').read_text()
for name,r in rows.items():
 expr='v('+','.join(k+'='+str(n) for k,n in zip(ledger['vector_order'],r['body_vector']) if n)+')'
 text,n=re.subn(r"\('"+re.escape(name)+r"',v\([^)]*\)","('"+name+"',"+expr,text)
 assert n in (1,2),(name,n)
text=text.replace("    co=co or {'Fixed':1}\n", "    co=co or {'Fixed':1}\n    ledger_rows={r['site']:r for r in json.loads((BASE/'evidence/ordered-operands.json').read_bytes())['rows']}\n    if name in ledger_rows:\n        ledger=ledger_rows[name]\n        assert a==ledger['body_vector'],name\n        why='ORDERED R3 BODY: '+'; '.join(p['source_operation'] for p in ledger['ordered_body_operands'])+' / SPONSORS: '+'; '.join(ledger['callee_or_earlier_sponsors'])\n")
text=text.replace('Actual prepared-fee caller read then branchless transfer','Branchless prepared-reference commit with no caller fee read')
text=text.replace('Named prepared fee caller and actual two-cell limits value copy','Prepared-reference commit and actual two-cell limits value copy')
text=text.replace('Branchless semantic transfer then actual mapped2','Branchless prepared-reference commit then actual mapped2')
(HERE/'complete.py').write_text(text,encoding='utf-8',newline='\n')
for name in ['envelopes.py','finalize.py']:
 (HERE/name).write_bytes((OLD/'evidence'/name).read_bytes())
print(json.dumps({'new_folder':BASE.relative_to(ROOT).as_posix(),'ledger_bodies':len(rows),'all_subject_seals_verified':True,'prior_seals_preserved':True,'target_execution':False},indent=2))
