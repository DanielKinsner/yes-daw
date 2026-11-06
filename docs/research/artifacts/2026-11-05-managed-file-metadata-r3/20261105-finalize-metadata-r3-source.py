"""Correct copied proposal metadata before source seal; no target/compiler call."""
from pathlib import Path
import hashlib
import json

root=Path(__file__).resolve().parents[1]
folder=root/'build-ci/20261105-managed-file-type-inventory-source-r3'
evidence=folder/'evidence/initial-unsealed'
assert not evidence.exists();evidence.mkdir()
manifest=folder/'source-pins.json';old=manifest.read_bytes()
(evidence/'source-pins.json').write_bytes(old)
author=root/'build-ci/20261105-author-metadata-r3-source.py'
(evidence/'author-source.py').write_bytes(author.read_bytes())
m=json.loads(old)
m.pop('authored_sources')
m['counts'].update(requested_types=22,corelib=4,required_named_types=22,reported_types_min=23,reported_types_max=26)
m['source_files']=[{'path':p.relative_to(root).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest().upper()} for p in (folder/'reader.cs',folder/'README.md',folder/'evidence/reader.diff',author,Path(__file__))]
m['unsealed_lineage']={'path':(evidence/'source-pins.json').relative_to(root).as_posix(),'bytes':len(old),'sha256':hashlib.sha256(old).hexdigest().upper(),'reason':'Initial copied R2 counts/authored-source descriptor labels remained stale; corrected before review or compiler/reader invocation. C# delta and domain unchanged.'}
manifest.write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8',newline='\n')
for row in m['source_files']:
    b=(root/row['path']).read_bytes();assert len(b)==row['bytes'] and hashlib.sha256(b).hexdigest().upper()==row['sha256']
print(json.dumps({'sealed_manifest_sha256':hashlib.sha256(manifest.read_bytes()).hexdigest().upper(),'required_named':m['counts']['required_named_types'],'target_invoked':False}))
