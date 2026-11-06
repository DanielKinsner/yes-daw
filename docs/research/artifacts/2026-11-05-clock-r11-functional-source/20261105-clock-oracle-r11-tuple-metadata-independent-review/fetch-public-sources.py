"""Fetch two fixed public source files; no assembly or source execution."""
from hashlib import sha256
import json
from pathlib import Path
from urllib.request import urlopen

HERE = Path(__file__).resolve().parent
PREFIX = 'https://raw.githubusercontent.com/PowerShell/PowerShell/ea554a8f9c6085f54fb2828f7ea286c65c35599f/src/System.Management.Automation/engine/'
FILES = [('LanguagePrimitives.cs','LanguagePrimitives.cs'),('Binders.cs','runtime/Binding/Binders.cs')]
dest = HERE / 'primary-sources'
dest.mkdir(exist_ok=True)
rows=[]
for name, relative in FILES:
    url=PREFIX+relative
    with urlopen(url,timeout=30) as response:
        data=response.read(1048577)
    if len(data)>1048576:
        raise ValueError('fixed_public_source_exceeds_1MiB')
    (dest/name).write_bytes(data)
    rows.append({'path':'primary-sources/'+name,'url':url,'commit':'ea554a8f9c6085f54fb2828f7ea286c65c35599f','bytes':len(data),'sha256':sha256(data).hexdigest().upper(),'license':'MIT; original copyright/license headers preserved','scope':'Selected public source association only; not executed and not binary equivalence.'})
(HERE/'primary-source-origins.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(rows,indent=2))
