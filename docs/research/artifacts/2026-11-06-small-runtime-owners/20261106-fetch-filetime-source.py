from pathlib import Path
import hashlib
import json
import urllib.request

root = Path(__file__).resolve().parents[1]
folder = root / 'build-ci/20261106-small-runtime-owner-source-r1'
assert not (folder / 'filetime-origins.json').exists()
commit = 'e2f47b0110ed922f21a1522da67279133ce28f32'
url = 'https://api.github.com/repos/dotnet/dotnet/contents/src/runtime/src/libraries/Common/src/Interop/Windows/Kernel32?ref=' + commit
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YES-DAW-fixed-source-review'}), timeout=30) as response:
    index_raw = response.read(1048577)
assert len(index_raw) <= 1048576
(folder / 'kernel32-index.json').write_bytes(index_raw)
index = json.loads(index_raw)
names = [r['name'] for r in index if 'FILE' in r['name'].upper() and 'TIME' in r['name'].upper()]
matches = [r for r in index if r['name'] in {'Interop.FILE_TIME.cs', 'Interop.FILETIME.cs'}]
assert len(matches) == 1, names
row = matches[0]
download = row['download_url']
assert download.startswith('https://raw.githubusercontent.com/dotnet/dotnet/' + commit + '/')
with opener.open(urllib.request.Request(download, headers={'User-Agent': 'YES-DAW-fixed-source-review'}), timeout=30) as response:
    data = response.read(1048577)
assert len(data) <= 1048576
data.decode('utf-8-sig')
(folder / row['name']).write_bytes(data)
(folder / 'filetime-origins.json').write_text(json.dumps(dict(file=row['name'], url=download, bytes=len(data), sha256=hashlib.sha256(data).hexdigest().upper()), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(file=row['name'], bytes=len(data))))
