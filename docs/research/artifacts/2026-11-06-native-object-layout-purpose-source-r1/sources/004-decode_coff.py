"""Read one bounded ordinary AMD64 COFF object. No native code is loaded/run.
Public file format only; no target private fields or pointer dereferences.
"""
from pathlib import Path
import hashlib,json,struct

MAX_OBJECT=16*1024*1024
MAGIC=0x59444C4159303031
SYMBOL='yesdaw_layout_cells'

def require(condition,reason):
    if not condition:raise ValueError(reason)

def decode(path,schema):
    p=Path(path);size=p.stat().st_size
    require(20<=size<=MAX_OBJECT,'OBJECT_EXTENT')
    data=p.read_bytes();require(len(data)==size,'OBJECT_READ_CHANGED')
    def block(offset,count):
        require(0<=offset<=len(data) and 0<=count<=len(data)-offset,'COFF_RANGE')
        return data[offset:offset+count]
    def fields(fmt,offset):
        return struct.unpack(fmt,block(offset,struct.calcsize(fmt)))
    machine,nsec,stamp,symptr,nsyms,opt,flags=fields('<HHIIIHH',0)
    require(machine==0x8664 and opt==0 and 1<=nsec<=512,'ORDINARY_AMD64_COFF')
    require(1<=nsyms<=65536,'COFF_SYMBOL_COUNT')
    block(20,nsec*40);block(symptr,nsyms*18+4)
    string_base=symptr+nsyms*18
    string_size=fields('<I',string_base)[0]
    require(4<=string_size<=1024*1024,'COFF_STRING_EXTENT')
    strings=block(string_base,string_size)
    def name(raw):
        if raw[:4]==b'\0'*4:
            offset=struct.unpack('<I',raw[4:])[0]
            require(4<=offset<len(strings),'COFF_NAME_OFFSET')
            stop=strings.find(b'\0',offset,min(len(strings),offset+4096))
            require(stop>=offset,'COFF_NAME_NUL')
            return strings[offset:stop].decode('ascii')
        return raw.split(b'\0',1)[0].decode('ascii')
    sections=[]
    for i in range(nsec):
        raw,vsize,vaddr,nbytes,ptr,relptr,lineptr,nrel,nline,chars=fields('<8sIIIIIIHHI',20+i*40)
        require(raw[:1]!=b'/','NO_LONG_SECTION_NAMES')
        label=raw.split(b'\0',1)[0].decode('ascii')
        if nbytes:block(ptr,nbytes)
        if nrel:block(relptr,nrel*10)
        require(not (chars&0x20000000 and nbytes),'NO_EXECUTABLE_CODE')
        sections.append(dict(name=label,bytes=nbytes,ptr=ptr,relocations=nrel,characteristics=chars))
    matches=[];i=0
    while i<nsyms:
        raw,value,section,typ,storage,aux=fields('<8sIhHBB',symptr+i*18)
        label=name(raw)
        require(i+1+aux<=nsyms,'COFF_AUX_EXTENT')
        if label==SYMBOL:matches.append((value,section,storage,typ))
        require(not label.startswith(('??__E','??__F')),'NO_DYNAMIC_INITIALIZER_SYMBOL')
        i+=1+aux
    require(len(matches)==1,'ONE_LAYOUT_SYMBOL')
    offset,section,storage,typ=matches[0]
    require(storage==2 and 1<=section<=nsec,'EXTERNAL_DATA_SYMBOL')
    sec=sections[section-1]
    require(sum(r['name']=='.ylayout' for r in sections)==1 and sec['name']=='.ylayout' and sec['relocations']==0,'EXACT_DATA_SECTION_NO_RELOC')
    require((sec['characteristics']&0x40000040)==0x40000040,'READABLE_INITIALIZED_DATA')
    require((sec['characteristics']&0xA0000000)==0,'NONWRITABLE_NONEXECUTABLE_DATA')
    entries=schema['entries'];require(1<=len(entries)<=1024,'SCHEMA_CELL_COUNT')
    amount=8*(3+len(entries))
    require(offset%8==0 and offset<=sec['bytes'] and amount<=sec['bytes']-offset,'LAYOUT_CELL_EXTENT')
    cells=struct.unpack('<'+'Q'*(3+len(entries)),block(sec['ptr']+offset,amount))
    require(cells[:3]==(MAGIC,1,len(entries)),'LAYOUT_HEADER')
    names=[r['name'] for r in entries];require(len(set(names))==len(names),'UNIQUE_SCHEMA_NAMES')
    rows=[dict(name=r['name'],expression=r['expression'],kind=r['kind'],value=v) for r,v in zip(entries,cells[3:])]
    return dict(schema='COMPILE_ONLY_NATIVE_OBJECT_LAYOUT_VALUES_R1',object_bytes=len(data),object_sha256=hashlib.sha256(data).hexdigest().upper(),machine=machine,sections=nsec,cells=len(rows),rows=rows,scope='Actual constant compiler type/layout values only. No code loaded/run, no Clock execution, no frame/RSS/resource fit or Data.',no_native_execution=True)

def main():
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('object');parser.add_argument('schema')
    args=parser.parse_args()
    result=decode(args.object,json.loads(Path(args.schema).read_bytes()))
    print(json.dumps(result,indent=2))

if __name__=='__main__':
    try:main()
    except Exception as error:
        print(json.dumps(dict(status='COFF_REFUSAL',reason=str(error)[:160] if type(error) is ValueError else type(error).__name__)))
        raise SystemExit(1)
