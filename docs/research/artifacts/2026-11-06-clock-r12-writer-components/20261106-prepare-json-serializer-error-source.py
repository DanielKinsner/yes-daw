from pathlib import Path
import hashlib
import json
import urllib.request

root = Path(__file__).resolve().parents[1]
folder = root / 'build-ci/20261106-clock-r12-serializer-runtime-source-r1'
assert not (folder / 'primary-error-origins.json').exists()
newtonsoft = 'https://raw.githubusercontent.com/JamesNK/Newtonsoft.Json/4e13299d4b0ec96bd4df9954ef646bd2d1b5bf2a/Src/Newtonsoft.Json/'
urls = {name: newtonsoft + remote for name, remote in [
    ('JsonPosition.cs', 'JsonPosition.cs'), ('ErrorContext.cs', 'Serialization/ErrorContext.cs'),
    ('ErrorEventArgs.cs', 'Serialization/ErrorEventArgs.cs')]}
urls['StreamingContext.cs'] = 'https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Private.CoreLib/src/System/Runtime/Serialization/StreamingContext.cs'
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
rows = []
for name, url in urls.items():
    with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YES-DAW-fixed-source-review'}), timeout=30) as response:
        data = response.read(1048577)
    assert len(data) <= 1048576
    data.decode('utf-8-sig')
    assert not (folder / 'primary' / name).exists()
    (folder / 'primary' / name).write_bytes(data)
    rows.append(dict(file=name, url=url, bytes=len(data), sha256=hashlib.sha256(data).hexdigest().upper()))
(folder / 'primary-error-origins.json').write_text(json.dumps(dict(files=rows), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(rows), bytes=sum(r['bytes'] for r in rows))))
