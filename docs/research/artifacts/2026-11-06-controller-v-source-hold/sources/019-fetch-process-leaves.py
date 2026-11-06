import hashlib,json,pathlib,urllib.request
here=pathlib.Path(__file__).parent
prefix='https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/'
specs={
'Process.Win32.cs':'System.Diagnostics.Process/src/System/Diagnostics/Process.Win32.cs',
'ProcessManager.Windows.cs':'System.Diagnostics.Process/src/System/Diagnostics/ProcessManager.Windows.cs',
'ProcessManager.Win32.cs':'System.Diagnostics.Process/src/System/Diagnostics/ProcessManager.Win32.cs',
'BCryptAlgorithmCache.cs':'Common/src/Interop/Windows/BCrypt/BCryptAlgorithmCache.cs',
'ManualResetEventSlim.cs':'System.Private.CoreLib/src/System/Threading/ManualResetEventSlim.cs'}
records=[]
for name,path in specs.items():
 dest=here/'primary'/name
 if dest.exists():raise RuntimeError('PRESERVE_'+name)
 url=prefix+path
 try:
  with urllib.request.urlopen(url,timeout=30) as r:data=r.read()
 except urllib.error.HTTPError as error:
  if error.code!=404:raise
  records.append({'file':name,'url':url,'status':'EXACT_CANDIDATE_404_NOT_EVIDENCE'});continue
 dest.write_bytes(data);records.append({'file':name,'url':url,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper()})
path=here/'process-leaf-origins.json'
if path.exists():raise RuntimeError('PRESERVE_PRIOR_OUTPUT')
path.write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(records,indent=2))
