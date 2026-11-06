"""Independent read-only COFF constant extraction; never execute the object."""
from pathlib import Path
from hashlib import sha256
import json
import struct

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent.parent
F=ROOT/'build-ci/20261105-musical-funded-r5-conversion-identity-r1'
source=(F/'identity.cpp').read_bytes()
obj=(F/'attempt-r1/identity.obj').read_bytes()
assert sha256(source).hexdigest().upper()=='D7BC4BF2370653E32B07C35E3AB5A5FFF03B6BDE9EE5D20E18AE06EFC1FDEAC2'
assert sha256(obj).hexdigest().upper()=='23D37F717E962DD599864074771290E531B6B258C4EE6CEE6D99C1B0F15AE516'
assert b'std::is_same_v<std::uint64_t, std::size_t> ? 0ULL : 1ULL' in source
machine,section_count,stamp,symptr,symbol_count,optsize,flags=struct.unpack_from('<HHIIIHH',obj,0)
assert machine==0x8664 and optsize==0
strings=symptr+18*symbol_count
string_bytes=struct.unpack_from('<I',obj,strings)[0]
assert strings+string_bytes<=len(obj)
def cstring(at):
 end=obj.index(b'\0',at,strings+string_bytes)
 return obj[at:end].decode('ascii')
sections=[]
for i in range(section_count):
 at=20+optsize+40*i
 name=obj[at:at+8].rstrip(b'\0').decode('ascii')
 if name.startswith('/'):name=cstring(strings+int(name[1:]))
 _,_,size,offset,reloc,line,nrel,nline,char=struct.unpack_from('<IIIIIIHHI',obj,at+8)
 assert offset+size<=len(obj)
 sections.append({'name':name,'size':size,'offset':offset,'relocations':nrel,'characteristics':char})
matches=[]
i=0
while i<symbol_count:
 at=symptr+18*i
 name=(cstring(strings+struct.unpack_from('<I',obj,at+4)[0]) if obj[at:at+4]==b'\0'*4 else obj[at:at+8].rstrip(b'\0').decode('ascii'))
 value,section,typ,storage,aux=struct.unpack_from('<IhHBB',obj,at+8)
 if name=='yesdaw_conversion_identity':matches.append((value,section,storage))
 i+=1+aux
assert len(matches)==1
value,section,storage=matches[0]
assert section>0 and storage==2
s=sections[section-1]
assert s['name']=='.rdata' and s['relocations']==0
assert s['characteristics']&0x40000000 and not s['characteristics']&0x80000000
assert value+24<=s['size']
words=list(struct.unpack_from('<QQQ',obj,s['offset']+value))
assert words==[0x5945534441574431,1,0]
assert not any(x['name'].startswith('.CRT') for x in sections)
assert sum(x['size'] for x in sections if x['characteristics']&0x20)==0
record={'schema':'R3_INDEPENDENT_SELECTED_R5_TYPE_IDENTITY_COFF_EXTRACT','source_sha256':sha256(source).hexdigest().upper(),'object_sha256':sha256(obj).hexdigest().upper(),'machine':machine,'table_symbol':'yesdaw_conversion_identity','readonly_section':s['name'],'relocations':s['relocations'],'words':words,'selected_R5_U64_size_t_nonidentity_d':words[2],'source_includes':'cstddef,cstdint,type_traits; exact std::is_same_v compile-time expression','no_target_execution':True,'scope':'Actual selected R5 compile-only fundamental-type identity constant. Generic d equations remain; no R4 measured layout/offset equality, price/Data binding, resource/native fit or execution credit.'}
(HERE/'selected-R5-identity.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(record,indent=2))
