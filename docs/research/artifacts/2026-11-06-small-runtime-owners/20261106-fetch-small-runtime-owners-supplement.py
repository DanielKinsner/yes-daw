from pathlib import Path
import hashlib
import json
import urllib.request

root = Path(__file__).resolve().parents[1]
folder = root / 'build-ci/20261106-small-runtime-owner-source-r1'
assert not (folder / 'supplement-origins.json').exists()
base = 'https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/'
urls = {
    'BigIntegerCalculator.Utils.cs': base + 'System.Runtime.Numerics/src/System/Numerics/BigIntegerCalculator.Utils.cs',
    'Interop.WIN32_FILE_ATTRIBUTE_DATA.cs': base + 'Common/src/Interop/Windows/Kernel32/Interop.WIN32_FILE_ATTRIBUTE_DATA.cs',
    'StringReference.cs': 'https://raw.githubusercontent.com/JamesNK/Newtonsoft.Json/4e13299d4b0ec96bd4df9954ef646bd2d1b5bf2a/Src/Newtonsoft.Json/Utilities/StringReference.cs',
}
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
rows = []
for name, url in urls.items():
    assert not (folder / name).exists()
    with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YES-DAW-fixed-source-review'}), timeout=30) as response:
        data = response.read(1048577)
    assert len(data) <= 1048576
    data.decode('utf-8-sig')
    (folder / name).write_bytes(data)
    rows.append(dict(file=name, url=url, bytes=len(data), sha256=hashlib.sha256(data).hexdigest().upper()))
(folder / 'supplement-origins.json').write_text(json.dumps(dict(files=rows), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(rows), names=[r['file'] for r in rows])))
