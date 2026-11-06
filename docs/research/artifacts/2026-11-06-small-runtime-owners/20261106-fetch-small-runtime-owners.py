from pathlib import Path
import hashlib
import json
import urllib.request

root = Path(__file__).resolve().parents[1]
folder = root / 'build-ci/20261106-small-runtime-owner-source-r1'
assert not folder.exists()
folder.mkdir()
commit = 'e2f47b0110ed922f21a1522da67279133ce28f32'
base = 'https://raw.githubusercontent.com/dotnet/dotnet/' + commit + '/src/runtime/src/libraries/'
index_url = 'https://api.github.com/repos/dotnet/dotnet/contents/src/runtime/src/libraries/System.Runtime.Numerics/src/System/Numerics?ref=' + commit
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
rows = []
def fetch(name, url, maximum=1048576):
    with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YES-DAW-fixed-source-review'}), timeout=30) as response:
        data = response.read(maximum + 1)
    assert len(data) <= maximum
    data.decode('utf-8-sig')
    (folder / name).write_bytes(data)
    rows.append(dict(file=name, url=url, bytes=len(data), sha256=hashlib.sha256(data).hexdigest().upper()))
    return data
index = json.loads(fetch('numerics-index.json', index_url))
selected = [row for row in index if row['name'] in {'BigInteger.cs', 'BigNumber.cs'}]
assert {row['name'] for row in selected} == {'BigInteger.cs', 'BigNumber.cs'}
for row in selected:
    assert row['download_url'].startswith('https://raw.githubusercontent.com/dotnet/dotnet/' + commit + '/')
    fetch(row['name'], row['download_url'])
fetch('MarshalByRefObject.cs', base + 'System.Private.CoreLib/src/System/MarshalByRefObject.cs')
fetch('Interop.GetFileAttributesEx.cs', base + 'Common/src/Interop/Windows/Kernel32/Interop.GetFileAttributesEx.cs')
(folder / 'origins.json').write_text(json.dumps(dict(commit=commit, scope='Official source bytes only; no target execution or runtime value observation', files=rows), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(rows), names=[r['file'] for r in rows], bytes=sum(r['bytes'] for r in rows))))
