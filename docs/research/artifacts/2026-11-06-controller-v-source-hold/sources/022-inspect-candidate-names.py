"""Source-authoring aid only: raw fixed binary-name hits are not TypeDef evidence."""
import hashlib, json, pathlib, re, urllib.request

root = pathlib.Path(__file__).resolve().parents[2]
runtime = pathlib.Path('C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell')
files = {
 'System.Private.CoreLib.dll': [
  rb'<(?:ReadFromNonSeekableAsync|ReadAtLeastAsyncCore|InvokeAsync)>d__[0-9]+(?:`[0-9]+)?',
  rb'(?:AsyncStateMachineBox`[0-9]+|StateMachineBox`[0-9]+|ValueTaskSourceAsTask|SyncAsyncWorkItemRegistration|AsyncOverSyncWithIoCancellation|Registrations|CallbackNode|TaskNode|OSEncoding|DecoderDBCS|DecoderNLS|Decoder|SafeThreadHandle|SafeFileHandle|ConsoleEncoding|DictionaryWrapper|StringDictionaryWrapper|HashProviderCng|LiteHash|SafeBCryptHashHandle|StreamReader|SyncWindowsFileStreamStrategy|BufferedFileStreamStrategy|OSFileStreamStrategy)'
 ],
 'System.Diagnostics.Process.dll': [rb'(?:DictionaryWrapper|StringDictionaryWrapper|SafeProcessHandle|SafeThreadHandle|ProcessStartInfo|ProcessInfo|Process)'],
 'System.Security.Cryptography.dll': [rb'(?:HashProviderCng|LiteHash|SafeBCryptHashHandle|SafeBCryptAlgorithmHandle|HashProvider|IncrementalHash)'],
 'System.Console.dll': [rb'(?:ConsoleEncoding|OSEncoding|DecoderDBCS)'],
 'System.Text.Encoding.CodePages.dll': [rb'(?:SBCSCodePageEncoding|DBCSCodePageEncoding|BaseCodePageEncoding|EncodingNLS|DecoderNLS|DecoderDBCS)']
}
records = []
for name, patterns in files.items():
 path=runtime/name
 data=path.read_bytes()
 strings=set(re.findall(rb'[A-Za-z0-9_<>+.`]{3,}', data))
 hits=sorted({s.decode('ascii') for s in strings if any(re.fullmatch(p,s) for p in patterns)})
 records.append({'file':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper(),'raw_string_candidates_only':hits})
url='https://api.github.com/repos/dotnet/dotnet/contents/src/runtime/src/libraries/System.Diagnostics.Process/src/System/Collections/Specialized?ref=e2f47b0110ed922f21a1522da67279133ce28f32'
request=urllib.request.Request(url,headers={'User-Agent':'yes-daw-source-only-review'})
with urllib.request.urlopen(request,timeout=30) as response:
 directory=json.load(response)
result={'status':'AUTHORING_CANDIDATE_STRINGS_NOT_METADATA_OR_ASSOCIATION','files':records,'official_directory':{'url':url,'entries':[{'name':x['name'],'download_url':x.get('download_url')} for x in directory]}}
path=pathlib.Path(__file__).parent/'candidate-name-hits.json'
if path.exists(): raise RuntimeError('PRESERVE_PRIOR_OUTPUT')
path.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(result,indent=2))
