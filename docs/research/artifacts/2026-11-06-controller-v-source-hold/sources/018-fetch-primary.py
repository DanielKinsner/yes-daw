"""Bounded public pinned-source fetch only; no target assembly/code execution."""
from pathlib import Path
import hashlib
import json
import urllib.request
import urllib.error

OUT=Path(__file__).resolve().parent/'primary'
OUT.mkdir(exist_ok=True)
PREFIX='https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/'
SPECS={
 'PoolingAsyncValueTaskMethodBuilderT.cs':'System.Private.CoreLib/src/System/Runtime/CompilerServices/PoolingAsyncValueTaskMethodBuilderT.cs',
 'AsyncValueTaskMethodBuilderT.cs':'System.Private.CoreLib/src/System/Runtime/CompilerServices/AsyncValueTaskMethodBuilderT.cs',
 'AsyncTaskMethodBuilderT.cs':'System.Private.CoreLib/src/System/Runtime/CompilerServices/AsyncTaskMethodBuilderT.cs',
 'ValueTask.cs':'System.Private.CoreLib/src/System/Threading/Tasks/ValueTask.cs',
 'CancellationTokenSource.cs':'System.Private.CoreLib/src/System/Threading/CancellationTokenSource.cs',
 'CancellationToken.cs':'System.Private.CoreLib/src/System/Threading/CancellationToken.cs',
 'CancellationTokenRegistration.cs':'System.Private.CoreLib/src/System/Threading/CancellationTokenRegistration.cs',
 'SemaphoreSlim.cs':'System.Private.CoreLib/src/System/Threading/SemaphoreSlim.cs',
 'DictionaryWrapper.cs':'System.Diagnostics.Process/src/System/Diagnostics/DictionaryWrapper.cs',
 'StringDictionary.cs':'System.Collections.Specialized/src/System/Collections/Specialized/StringDictionary.cs',
 'ConsoleEncoding.cs':'Common/src/System/Text/ConsoleEncoding.cs',
 'OSEncoding.Windows.cs':'Common/src/System/Text/OSEncoding.Windows.cs',
 'HashProviderDispenser.Windows.cs':'System.Security.Cryptography/src/System/Security/Cryptography/HashProviderDispenser.Windows.cs',
 'HashProviderCng.cs':'System.Security.Cryptography/src/System/Security/Cryptography/HashProviderCng.cs',
 'LiteHash.Windows.cs':'System.Security.Cryptography/src/System/Security/Cryptography/LiteHash.Windows.cs',
 'SafeBCryptHashHandle.cs':'Common/src/Microsoft/Win32/SafeHandles/SafeBCryptHashHandle.cs',
 'Process.cs':'System.Diagnostics.Process/src/System/Diagnostics/Process.cs'}
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
records=[]
for name,suffix in SPECS.items():
    path=OUT/name;url=PREFIX+suffix
    if path.exists():raise RuntimeError('Preserve existing primary source:'+name)
    try:
        with opener.open(url,timeout=30) as response:data=response.read(1048577)
        if len(data)>1048576:raise RuntimeError('Bounded source limit:'+name)
        data.decode('utf-8-sig');path.write_bytes(data)
        records.append(dict(file=name,url=url,status='FETCHED_PINNED_SOURCE',bytes=len(data),sha256=hashlib.sha256(data).hexdigest().upper()))
    except urllib.error.HTTPError as e:
        if e.code!=404:raise
        records.append(dict(file=name,url=url,status='EXACT_CANDIDATE_404_NOT_EVIDENCE'))
(OUT.parent/'primary-origins.json').write_text(json.dumps(dict(commit='e2f47b0110ed922f21a1522da67279133ce28f32',files=records),indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps([dict(file=r['file'],status=r['status'],bytes=r.get('bytes')) for r in records]))
