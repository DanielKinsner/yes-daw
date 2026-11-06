import hashlib,json,pathlib,urllib.request
here=pathlib.Path(__file__).parent
primary=here/'primary';primary.mkdir(exist_ok=True)
prefix='https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/'
specs={
'Environment.cs':'System.Private.CoreLib/src/System/Environment.cs',
'Environment.Windows.cs':'System.Private.CoreLib/src/System/Environment.Windows.cs',
'Interop.GetModuleFileName.cs':'Common/src/Interop/Windows/Kernel32/Interop.GetModuleFileName.cs',
'Interop.GetModuleFileNameEx.cs':'Common/src/Interop/Windows/Kernel32/Interop.GetModuleFileNameEx.cs'}
rows=[]
for file,path in specs.items():
 dest=primary/file
 if dest.exists():raise RuntimeError('PRESERVE_'+file)
 url=prefix+path
 try:
  with urllib.request.urlopen(url,timeout=30) as r:data=r.read()
 except urllib.error.HTTPError as error:
  if error.code!=404:raise
  rows.append({'file':file,'url':url,'status':'EXACT_CANDIDATE_404_NOT_EVIDENCE'});continue
 dest.write_bytes(data);rows.append({'file':file,'url':url,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper()})
path=here/'origins.json'
if path.exists():raise RuntimeError('PRESERVE_PRIOR_OUTPUT')
path.write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(rows,indent=2))
