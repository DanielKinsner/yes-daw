from pathlib import Path
import hashlib
import json
import urllib.request

root = Path(__file__).resolve().parents[1]
folder = root / 'build-ci/20261106-clock-r12-writer-family-r1'
assert not (folder / 'primary-fallback-origins.json').exists()
base = 'https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Private.CoreLib/src/System/'
names = ['Text/EncoderFallback.cs', 'Text/DecoderFallback.cs', 'Text/EncoderReplacementFallback.cs', 'Text/DecoderReplacementFallback.cs', 'Collections/Generic/List.cs']
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
rows = []
for name in names:
    url = base + name
    with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YES-DAW-fixed-source-review'}), timeout=30) as response:
        data = response.read(1048577)
    assert len(data) <= 1048576
    data.decode('utf-8-sig')
    file = Path(name).name
    assert not (folder / 'primary' / file).exists()
    (folder / 'primary' / file).write_bytes(data)
    rows.append(dict(file=file, url=url, bytes=len(data), sha256=hashlib.sha256(data).hexdigest().upper()))
(folder / 'primary-fallback-origins.json').write_text(json.dumps(dict(files=rows), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(rows), bytes=sum(r['bytes'] for r in rows))))
