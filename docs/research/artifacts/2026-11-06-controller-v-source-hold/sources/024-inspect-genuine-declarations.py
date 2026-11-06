import hashlib,json,pathlib
root=pathlib.Path(__file__).resolve().parents[2]
path=root/'build-ci/20261106-managed-file-type-inventory-source-r4/execution-r1/inventory.json'
data=path.read_bytes()
assert hashlib.sha256(data).hexdigest().upper()=='7C23E7BF82F9D293962C06BBD450BC4D9F7BBD74FF9E5344C8E4C88686B04D6D'
inventory=json.loads(data)
need=['<ReadAtLeastAsyncCore>','<ReadFromNonSeekableAsync>','<InvokeAsync>d__8','StateMachineBox','ValueTaskSourceAsTask','DictionaryWrapper','SemaphoreSlim','CancellationTokenSource','CallbackNode','Registrations','AsyncOverSyncWithIoCancellation','SafeFileHandle','SafeThreadHandle','HashProviderCng','IncrementalHash','StreamReader','FileStreamStrategy','ProcessStartInfo','ConsoleEncoding','OSEncoding','DecoderDBCS','Task`1','ConcurrencyBlock']
rows=[]
for row in inventory['types']:
 if any(x in row['qualified_name'] for x in need):
  rows.append({'file':row['file'],'type':row['qualified_name'],'base':row['base_decoded'],'fields':[(x['name'],x['decoded_signature']) for x in row['declared_nonstatic_instance_fields']]})
print(json.dumps(rows,indent=2))
