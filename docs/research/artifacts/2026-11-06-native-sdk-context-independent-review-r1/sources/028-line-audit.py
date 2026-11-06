from pathlib import Path
import hashlib,json
out=Path(__file__).resolve().parent
if (out/'first-green-freeze.json').exists():raise SystemExit('FROZEN')
p=out/'selected-sdk-ledger.json';j=json.loads(p.read_text(encoding='utf-8'))
rows=[{'id':r['id'],'pass':r['declaration_anchor'] in r['source_excerpt'],'lines':r['lines'],'source':r['source']} for r in j['rows']]
attemptroot=out/'line-audit-attempts';attemptroot.mkdir(exist_ok=True)
attempt=attemptroot/('attempt-'+str(len(list(attemptroot.iterdir()))+1).zfill(2));attempt.mkdir()
for x in out.iterdir():
 if x.is_file():(attempt/x.name).write_bytes(x.read_bytes())
result={'checks':rows,'failed':sum(not r['pass'] for r in rows),'source_only':True,'scope':'Source range/declaration correspondence preliminary gate, not complete own check or source/resource acceptance.'}
(attempt/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'failed':result['failed'],'ids':[r['id'] for r in rows if not r['pass']]}))
raise SystemExit(1 if result['failed'] else 0)
