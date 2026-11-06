"""Authoring aid: fixed source candidate inventory; raw name hits have no metadata credit."""
import json,pathlib
here=pathlib.Path(__file__).parent
runtime=pathlib.Path('C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell')
extra={
'System.Private.CoreLib.dll':[
'System.IO.Stream','System.IO.FileStream','System.IO.StreamReader',
'System.IO.Strategies.FileStreamStrategy','System.IO.Strategies.OSFileStreamStrategy','System.IO.Strategies.SyncWindowsFileStreamStrategy','System.IO.Strategies.BufferedFileStreamStrategy',
'System.IO.Stream+<ReadAtLeastAsyncCore>d__49','System.IO.Strategies.BufferedFileStreamStrategy+<ReadFromNonSeekableAsync>d__36',
'System.Threading.AsyncOverSyncWithIoCancellation','System.Threading.AsyncOverSyncWithIoCancellation+SyncAsyncWorkItemRegistration','System.Threading.AsyncOverSyncWithIoCancellation+<InvokeAsync>d__8`2',
'System.Threading.CancellationTokenSource','System.Threading.CancellationTokenSource+Registrations','System.Threading.CancellationTokenSource+CallbackNode','System.Threading.CancellationTokenRegistration','System.Threading.CancellationToken',
'System.Threading.SemaphoreSlim','System.Threading.SemaphoreSlim+TaskNode',
'System.Threading.Tasks.Task','System.Threading.Tasks.Task`1','System.Threading.Tasks.ValueTask`1','System.Threading.Tasks.ValueTask`1+ValueTaskSourceAsTask',
'System.Runtime.CompilerServices.PoolingAsyncValueTaskMethodBuilder`1','System.Runtime.CompilerServices.PoolingAsyncValueTaskMethodBuilder`1+StateMachineBox','System.Runtime.CompilerServices.PoolingAsyncValueTaskMethodBuilder`1+StateMachineBox`1',
'System.Runtime.CompilerServices.AsyncTaskMethodBuilder`1','System.Runtime.CompilerServices.AsyncTaskMethodBuilder`1+AsyncStateMachineBox`1','System.Runtime.CompilerServices.AsyncValueTaskMethodBuilder`1',
'System.Threading.Tasks.Sources.ManualResetValueTaskSourceCore`1','System.Runtime.InteropServices.SafeHandle','Microsoft.Win32.SafeHandles.SafeHandleZeroOrMinusOneIsInvalid','Microsoft.Win32.SafeHandles.SafeFileHandle','Microsoft.Win32.SafeHandles.SafeThreadHandle',
'System.Memory`1','System.ValueTuple`3','System.Text.Encoding','System.Text.Decoder','System.Text.DecoderNLS','System.Diagnostics.Stopwatch','System.MarshalByRefObject'],
'System.Diagnostics.Process.dll':[
'System.Diagnostics.Process','System.Diagnostics.ProcessStartInfo','System.Collections.Specialized.DictionaryWrapper',
'Microsoft.Win32.SafeHandles.SafeProcessHandle','Microsoft.Win32.SafeHandles.SafeThreadHandle',
'System.Text.ConsoleEncoding','System.Text.OSEncoding','System.Text.DecoderDBCS'],
'System.Security.Cryptography.dll':[
'System.Security.Cryptography.IncrementalHash','System.Security.Cryptography.HashProvider','System.Security.Cryptography.HashProviderCng','System.Security.Cryptography.ConcurrencyBlock',
'Microsoft.Win32.SafeHandles.SafeBCryptHandle','Microsoft.Win32.SafeHandles.SafeBCryptHashHandle','Microsoft.Win32.SafeHandles.SafeBCryptAlgorithmHandle'],
'System.Text.Encoding.CodePages.dll':[
'System.Text.EncodingNLS','System.Text.BaseCodePageEncoding','System.Text.SBCSCodePageEncoding','System.Text.DBCSCodePageEncoding','System.Text.DecoderNLS'],
'System.ComponentModel.Primitives.dll':['System.ComponentModel.Component']}
records=[]
for file,names in extra.items():
 data=(runtime/file).read_bytes()
 for name in names:
  terminal=name.rsplit('+',1)[-1] if '+' in name else name.rsplit('.',1)[-1]
  records.append({'file':file,'proposed_fixed_type':name,'raw_terminal_name_hit_only':terminal.encode()+b'\0' in data})
out={'status':'PROPOSED_NAMES_NOT_TYPEDEF_OR_ASSOCIATION_EVIDENCE','extra_types':extra,'raw_terminal_name_checks':records}
path=here/'metadata-r4-fixed-type-plan.json'
if path.exists():raise RuntimeError('PRESERVE_PRIOR_OUTPUT')
path.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'extra_named_types':sum(map(len,extra.values())),'files':len(extra),'raw_terminal_name_misses':[x for x in records if not x['raw_terminal_name_hit_only']]},indent=2))
