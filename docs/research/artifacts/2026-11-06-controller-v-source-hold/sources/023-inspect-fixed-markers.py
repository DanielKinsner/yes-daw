import hashlib,json,pathlib,urllib.request
here=pathlib.Path(__file__).parent
runtime=pathlib.Path('C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell')
specs={
'System.Private.CoreLib.dll':['StateMachineBox','AsyncStateMachineBox','Registrations','CallbackNode','ReadAtLeastAsyncCore','ReadFromNonSeekableAsync','InvokeAsync','ValueTaskSourceAsTask','FileStreamStrategy','SafeHandle','SemaphoreSlim','Stopwatch','ManualResetValueTaskSourceCore','PaddedReference'],
'System.Diagnostics.Process.dll':['DictionaryWrapper','SafeProcessHandle','SafeThreadHandle','ProcessStartInfo','ProcessWaitHandle'],
'System.Security.Cryptography.dll':['ConcurrencyBlock','SafeBCryptHandle','BCryptAlgorithmCache','HashProviderCng','HashProvider'],
'System.Console.dll':['ConsoleEncoding','OSEncoding','DecoderDBCS'],
'System.Text.Encoding.CodePages.dll':['DecoderNLS','DecoderDBCS','SBCSCodePageEncoding','DBCSCodePageEncoding']}
out=[]
for name,markers in specs.items():
 data=(runtime/name).read_bytes(); hits={}
 for marker in markers:
  needle=marker.encode(); pos=0; vals=set()
  while True:
   at=data.find(needle,pos)
   if at<0:break
   a=data.rfind(b'\0',max(0,at-100),at)+1
   b=data.find(b'\0',at,min(len(data),at+150))
   if b>=0 and b-a<180:
    raw=data[a:b]
    if all(32<=x<127 for x in raw):vals.add(raw.decode())
   pos=at+len(needle)
  hits[marker]=sorted(vals)[:20]
 out.append({'file':name,'raw_names_not_metadata':hits})
url='https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Diagnostics.Process/src/System/Collections/Specialized/DictionaryWrapper.cs'
data=urllib.request.urlopen(url,timeout=30).read(); path=here/'primary'/'DictionaryWrapper.cs'
if path.exists():raise RuntimeError('PRESERVE_PRIOR_OUTPUT')
path.write_bytes(data)
origin={'url':url,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper()}
result={'candidate_raw_string_hits_only':out,'dictionary_wrapper_primary':origin}
path=here/'fixed-marker-hits.json'
if path.exists():raise RuntimeError('PRESERVE_PRIOR_OUTPUT')
path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(result,indent=2))
