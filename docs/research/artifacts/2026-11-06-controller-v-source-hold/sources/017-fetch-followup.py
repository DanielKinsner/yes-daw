import hashlib,json,pathlib,urllib.request
here=pathlib.Path(__file__).parent
base='https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/'
specs={
 'Task.cs':'System.Private.CoreLib/src/System/Threading/Tasks/Task.cs',
 'FileStream.cs':'System.Private.CoreLib/src/System/IO/FileStream.cs',
 'SemaphoreSlim.Lock.cs':'System.Private.CoreLib/src/System/Threading/Lock.cs',
 'SafeBCryptHandle.cs':'Common/src/Microsoft/Win32/SafeHandles/SafeBCryptHandle.cs',
 'SafeBCryptAlgorithmHandle.cs':'Common/src/Microsoft/Win32/SafeHandles/SafeBCryptAlgorithmHandle.cs',
 'Interop.BCryptAlgorithmCache.cs':'Common/src/Interop/Windows/BCrypt/Interop.BCryptAlgorithmCache.cs',
 'ProcessWaitHandle.cs':'Common/src/Interop/Windows/Kernel32/Interop.ProcessWaitHandle.cs',
 'Process.Windows.Kill.cs':'System.Diagnostics.Process/src/System/Diagnostics/Process.Windows.Kill.cs'
}
out=[]
for file,path in specs.items():
 dest=here/'primary'/file
 if dest.exists():raise RuntimeError('PRESERVE_'+file)
 url=base+path
 try:
  with urllib.request.urlopen(url,timeout=30) as response:data=response.read()
 except urllib.error.HTTPError as error:
  if error.code!=404:raise
  out.append({'file':file,'url':url,'status':'EXACT_CANDIDATE_404_NOT_EVIDENCE'});continue
 dest.write_bytes(data)
 out.append({'file':file,'url':url,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper()})
url='https://api.github.com/repos/dotnet/dotnet/contents/src/runtime/src/libraries/System.Diagnostics.Process/src/System/Diagnostics?ref=e2f47b0110ed922f21a1522da67279133ce28f32'
request=urllib.request.Request(url,headers={'User-Agent':'yes-daw-source-only-review'})
with urllib.request.urlopen(request,timeout=30) as response:listing=json.load(response)
record={'origins':out,'official_directory':{'url':url,'candidate_names':[x['name'] for x in listing if 'ProcessManager' in x['name'] or 'Kill' in x['name'] or 'Windows' in x['name']]}}
path=here/'followup-origins.json'
if path.exists():raise RuntimeError('PRESERVE_PRIOR_OUTPUT')
path.write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(record,indent=2))
