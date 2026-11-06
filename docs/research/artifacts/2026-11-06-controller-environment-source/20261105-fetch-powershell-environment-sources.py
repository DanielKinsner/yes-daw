"""Fetch fixed official source text only; no target, import, or environment access."""
from pathlib import Path, PurePosixPath
import hashlib
import io
import json
import urllib.request
import zipfile

root=Path(__file__).resolve().parents[1]
dest=root/'build-ci/20261105-controller-environment-writer-closure-r2'
assert not dest.exists(), 'New source inventory only'
commit='ea554a8f9c6085f54fb2828f7ea286c65c35599f'
url='https://codeload.github.com/PowerShell/PowerShell/zip/'+commit
request=urllib.request.Request(url,headers={'User-Agent':'YES-DAW-fixed-source-review'})
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
with opener.open(request,timeout=60) as response:
    raw=response.read(33554433)
assert len(raw)<=33554432
prefix='PowerShell-'+commit+'/'
records=[]
dest.mkdir(parents=True)
for entry in zipfile.ZipFile(io.BytesIO(raw)).infolist():
    assert entry.filename.startswith(prefix)
    name=entry.filename[len(prefix):]
    logical=PurePosixPath(name)
    assert not logical.is_absolute() and '..' not in logical.parts and ':' not in name
    if not name.startswith('src/') or not name.endswith('.cs'):continue
    data=zipfile.ZipFile(io.BytesIO(raw)).read(entry)
    data.decode('utf-8-sig')
    target=(dest/'primary'/Path(*logical.parts)).resolve()
    assert target.is_relative_to(dest.resolve())
    target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    records.append({'repository_path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper()})
manifest={'scope':'Exact official pinned PowerShell source C# inventory for environment-writer closure; no source code executed or current environment read','commit':commit,'url':url,'archive_bytes':len(raw),'archive_sha256':hashlib.sha256(raw).hexdigest().upper(),'files':records}
(dest/'source-origins.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'source_files':len(records),'source_bytes':sum(r['bytes'] for r in records),'archive_bytes':len(raw),'manifest_sha256':hashlib.sha256((dest/'source-origins.json').read_bytes()).hexdigest().upper()}))
