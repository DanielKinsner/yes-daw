from pathlib import Path
import base64,hashlib,json
R=Path(__file__).resolve().parents[2];O=Path(__file__).resolve().parent
assert not (O/'freeze.json').exists()
H=lambda x:hashlib.sha256(x).hexdigest().upper()
checks=[]
def c(n,v):checks.append({'name':n,'pass':bool(v)})
a=R/'docs/research/artifacts/2026-11-06-clock-r13-runner-census-r2'
raw=(a/'archive-manifest.json').read_bytes();m=json.loads(raw)
c('archive exact first-green manifest',H(raw)=='AC5AAE3957A58B56A7CB3128E95FFE35C4084DAA5A6024ABD9CC36D26AF17BE6')
c('all410 source rows',len(m['files'])==410)
for row in m['files']:
 p=a/row['file'];b=p.read_bytes()
 c(row['file'],len(b)==row['bytes'] and H(b)==row['sha256'])
c('whole tree exactly410 plus archive manifest and README',len([p for p in a.rglob('*') if p.is_file()])==412)
checker=R/'docs/research/2026-11-06-r13-delivery-check.py'
c('D71 original checker unchanged',H(checker.read_bytes())=='D71CFF476DA4290FFED8221D2C1135A3E02693B1B3136DE79047F212FAD96BED')
for n in [1,2]:
 folder=R/f'docs/research/artifacts/2026-11-06-r13-staging-failure-r{n}'
 j=json.loads((folder/'failure-and-index.json').read_bytes())
 c('failure checker '+str(n),(folder/'failed-check-source.py').read_bytes()==checker.read_bytes())
 if n==2:
  b=base64.b64decode(j['postfailure_precorrection_git_index_raw_base64'],validate=True)
  c('second complete index preserved',len(b)==439141 and H(b)=='D3C98A69C3B7723A016ADC8C9995E431365FB656E10524BBC04B36F1E30CF648')
  log=base64.b64decode(j['stdout_raw_base64'],validate=True)
  c('actual whitespace failure2 preserved',j['current_read_only_reproduction_exit']==2 and len(log)==18659)
  for name in ['030-microsoft-FileStream-constructors.html','031-microsoft-Thread-Sleep.html']:
   c('actual HTML issue '+name,name.encode() in log and any(b' \t' in line for line in (a/'sources'/name).read_bytes().splitlines()))
  c('all410+7+266 reached before final gate',j['all410_source_and7_frozen_delivery_and266_partial_output_checks_reached_before_git_whitespace_failure'])
c('no frozen transform helper changed',H((R/'build-ci/20261106-archive-research-source-packet-r1.py').read_bytes())=='822E0AF600EDFB2348B0F1046E1D5BC587C276D941838CD0D809C5AD702258F0')
proposal={
 'disposition':'ACCEPT_STRATEGY_A_FOR_NEW_DELIVERY_SOURCE_ONLY',
 'diagnosis':'The first-green410 display archive is lossless but not Git-whitespace-clean. Its display transform strips trailing space and normalizes LF; it does not remove leading space-before-tab. D71 correctly refuses that final required gate after validating410 source rows,7 frozen delivery files and266 failed partial outputs. This is a presentation/staging incompatibility, not a numeric, source-candidate, ABI or target runtime failure.',
 'recommended':'A: preserve entire exact412-file tree (410 members+original archive-manifest+README) as a new deterministic ZIP/Base64 JSON container. Keep original410 archive members and all frozen helpers unchanged. Commit a clean byte-exact census argument separately for reading.',
 'new_supported_hypothesis':'A raw-byte container keeps complete original whitespace outside Git text-line diagnostics while preserving every byte and former mapping. A fresh staged roundtrip plus git diff --cached --check can therefore satisfy both losslessness and unchanged repository whitespace policy. No old checker assertion or gate is waived.',
 'required_concrete_steps':[
  'Preserve both complete failed staging inputs/index snapshots/checker bytes/stdout/stderr/exit and original partial266 lineage before any index/file correction.',
  'Before Move-Item verify absolute source and destination are within this repo, exact expected directories, nonoverlapping, destination absent; preserve entire original412-file tree. No delete/overwrite or string-built cross-shell move.',
  'New container writer explicitly enumerates all412 actual files including ignored.log, sorts stable relative names, stores fixed ZIP metadata and raw bytes, full member bytes/SHA, original AC5 manifest and DFC0 census pins/F0C freeze and original source metadata. Do not recode UTF8 or alter old manifest paths.',
  'New checker reads staged Git blobs with headless null-stdin Git. Strictly decodes Base64/ZIP, rejects duplicate/unsafe/absolute/dotdot member names, compares exact complete membership and each raw member against preserved original bytes and pinned hash. Reconstruct each original via old archive transformation metadata and compare current source pins, including nonUTF8 and raw-diff wrappers.',
  'Verify separate original delivery7 freeze and failed266 ZIP plus both staging snapshots remain recoverable; verify clean census argument and new checker/writer staged bytes exact, protected test and instructions unchanged.',
  'Run unchanged git diff --cached --check as final required gate. Freeze new writer/checker/whole output at first green; preserve any new failed attempt without patching old freezes. Container delivery gives no source/numeric/resource/ABI/Clock acceptance.'
 ],
 'alternative_B':'Wrap only HTML in a new display revision. Technically possible but adds many new row/path/transformation/helper changes. No advantage over A for preserving the already frozen whole archive.',
 'remaining':'This accepts a concrete correction strategy only, not unreviewed future writer/checker or its output. Root must validate that new concrete packet mechanically before certification.',
 'source_only':True,'target_execution':False,'Git_mutations_by_critic':False}
result={'checks':checks,'passed':sum(x['pass'] for x in checks),'failed':sum(not x['pass'] for x in checks),'critic':proposal}
(O/'review.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
assert result['failed']==0
files=[{'file':p.name,'bytes':p.stat().st_size,'sha256':H(p.read_bytes())} for p in sorted(O.iterdir()) if p.is_file()]
(O/'freeze.json').write_text(json.dumps({'files':files,'first_green':True,'target_execution':False},indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'passed':result['passed'],'failed':0,'review_sha256':H((O/'review.json').read_bytes()),'freeze_sha256':H((O/'freeze.json').read_bytes()),'disposition':proposal['disposition']}))
