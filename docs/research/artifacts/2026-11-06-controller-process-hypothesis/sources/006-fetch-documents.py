import hashlib,json,pathlib,urllib.request
here=pathlib.Path(__file__).parent
dest=here/'primary-documents';dest.mkdir(exist_ok=True)
specs={
'GetModuleFileNameW.html':'https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew',
'GetModuleFileNameExW.html':'https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmodulefilenameexw',
'JobObjects.html':'https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects',
'UpdateProcThreadAttribute.html':'https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-updateprocthreadattribute',
'JobBasicAccounting.html':'https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_basic_accounting_information',
'CancelPendingIO.html':'https://learn.microsoft.com/en-us/windows/win32/fileio/canceling-pending-i-o-operations',
'AnonymousPipes.html':'https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations'}
rows=[]
for file,url in specs.items():
 path=dest/file
 if path.exists():raise RuntimeError('PRESERVE_'+file)
 with urllib.request.urlopen(url,timeout=30) as response:
  data=response.read();date=response.headers.get('Date')
 path.write_bytes(data);rows.append({'file':file,'url':url,'HTTP_date':date,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest().upper()})
path=here/'document-origins.json'
if path.exists():raise RuntimeError('PRESERVE_PRIOR_OUTPUT')
path.write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'fetched_primary_documents':len(rows),'bytes':sum(r['bytes'] for r in rows)},indent=2))
