from pathlib import Path
import hashlib
import json
import urllib.request

root = Path(__file__).resolve().parents[1]
folder = root / 'build-ci/20261106-small-runtime-owner-source-r1'
assert sorted(p.name for p in folder.iterdir()) == ['numerics-index.json']
commit = 'e2f47b0110ed922f21a1522da67279133ce28f32'
base = 'https://raw.githubusercontent.com/dotnet/dotnet/' + commit + '/src/runtime/src/libraries/'
index = json.loads((folder / 'numerics-index.json').read_bytes())
item = next(row for row in index if row['name'] == 'BigInteger.cs')
urls = {'BigInteger.cs': item['download_url'],
    'Number.BigInteger.cs': base + 'System.Runtime.Numerics/src/System/Number.BigInteger.cs',
    'MarshalByRefObject.cs': base + 'System.Private.CoreLib/src/System/MarshalByRefObject.cs',
    'Interop.GetFileAttributesEx.cs': base + 'Common/src/Interop/Windows/Kernel32/Interop.GetFileAttributesEx.cs'}
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
rows = []
for name, url in urls.items():
    assert url.startswith('https://raw.githubusercontent.com/dotnet/dotnet/' + commit + '/')
    with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YES-DAW-fixed-source-review'}), timeout=30) as response:
        data = response.read(1048577)
    assert len(data) <= 1048576
    data.decode('utf-8-sig')
    (folder / name).write_bytes(data)
    rows.append(dict(file=name, url=url, bytes=len(data), sha256=hashlib.sha256(data).hexdigest().upper()))
(folder / 'origins.json').write_text(json.dumps(dict(commit=commit, scope='Official source only; no target execution', initial_fetch_failure='Expected historical BigNumber.cs filename was absent in actual pinned Numerics directory; index/first helper/error preserved. Selected actual Number.BigInteger.cs under System.', files=rows), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(rows), names=[r['file'] for r in rows], bytes=sum(r['bytes'] for r in rows))))
