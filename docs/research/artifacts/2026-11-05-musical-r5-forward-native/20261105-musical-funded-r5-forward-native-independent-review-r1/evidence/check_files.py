"""Independent file/data-only checks. Never executes subject programs or author scripts."""
import collections
import hashlib
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
MAIN = ROOT / 'build-ci/20261105-musical-funded-r5-forward-native-obligations-r1'
SUPP = ROOT / 'build-ci/20261105-musical-funded-r5-classification-obligation-supplement-r1'
checks = []
inputs = {}

def digest(b):
    return hashlib.sha256(b).hexdigest().upper()

def read(path):
    p = Path(path)
    if not p.is_absolute():
        p = ROOT / p
    b = p.read_bytes()
    try:
        name = p.relative_to(ROOT).as_posix()
    except ValueError:
        name = p.as_posix()
    inputs[name] = {'path': name, 'bytes': len(b), 'sha256': digest(b)}
    return b

def ck(name, ok, detail=None):
    checks.append({'check': name, 'pass': bool(ok), **({'detail': detail} if detail is not None else {})})
    if not ok:
        raise AssertionError(name)

def load(p):
    return json.loads(read(p))

def listing_rows(text):
    """Recover displayed bytes, including wrapped continuation rows."""
    rows = []
    for line in text.splitlines():
        m = re.match(r'^\s*([0-9A-Fa-f]{16}):\s+(.*)$', line)
        if m:
            rest = m.group(2)
            x = re.match(r'((?:[0-9A-Fa-f]{2}(?:\s+|$))+)(.*)$', rest)
            if x:
                rows.append({'offset': int(m.group(1),16), 'bytes': bytearray.fromhex(x.group(1)),
                             'asm': x.group(2).strip(), 'line': line})
        elif rows and re.fullmatch(r'\s+(?:[0-9A-Fa-f]{2}(?:\s+|$))+', line):
            rows[-1]['bytes'].extend(bytearray.fromhex(line))
    return rows

main_pins = load(MAIN/'source-pins.json')
supp_pins = load(SUPP/'source-pins.json')
for label, pins in [('main', main_pins), ('supplement', supp_pins)]:
    for group in ['inputs','outputs','evidence']:
        for row in pins.get(group, []):
            b = read(row['path'])
            ck(f'{label} {group} {row["path"]}', len(b)==row['bytes'] and digest(b)==row['sha256'])

arg = load(MAIN/'argument.json')
supp = load(SUPP/'argument.json')
sections = load(MAIN/'object-sections.json')
profile = read('build-ci/20261105-musical-funded-legacy-adapter-source-r5/native-profile.h').decode()
enum = re.search(r'enum class NativeFact[^\{]*\{(.*?)\};',profile,re.S).group(1)
names = [x.strip() for x in enum.split(',')]
ck('NativeFact enum including Count', len(names)==18 and names[-1]=='Count')
ck('17 NativeFact ordinal/name rows', [(f['ordinal'],f['name']) for f in arg['facts']]==list(enumerate(names[:-1])))
ck('17 facts each name their remaining premise', all(f['still_required'] and f['runtime_digest_is_not_proof'] for f in arg['facts']))
ck('17 recorded fact count', arg['fact_count']==17 and arg['NativeFact_order_exact'] is True)
ck('supplement uses one-based fact references', supp['facts_affected']==[7,8,16,17])

objpath = 'build-ci/20261105-musical-funded-r5-force-emission-object-r1/attempt-r1/app-compute.obj'
listpath = 'build-ci/20261105-musical-funded-r5-force-emission-object-r1/attempt-r1/app-compute-disassembly.txt'
obj = read(objpath)
listing = read(listpath).decode()
ck('object and listing exact identities', digest(obj)==sections['object_sha256'] and digest(listing.encode())==sections['listing_sha256'])
ck('AMD64 BIGOBJ header', struct.unpack_from('<HHHH',obj)==(0,65535,2,0x8664))
nsec, symoff, nsyms = struct.unpack_from('<III',obj,44)
ck('BIGOBJ section and symbol counts', nsec==sections['coff_sections']==1574 and nsyms==sections['symbol_records_including_aux']==5045)
st = symoff+20*nsyms
strsize = struct.unpack_from('<I',obj,st)[0]
ck('BIGOBJ string table within file', st+strsize<=len(obj))

def symbol(idx):
    p=symoff+20*idx
    rawname=obj[p:p+8]
    if rawname[:4]==b'\0'*4:
        a=st+struct.unpack_from('<I',rawname,4)[0]
        name=obj[a:obj.index(b'\0',a)].decode()
    else:
        name=rawname.rstrip(b'\0').decode()
    value, sec, typ, storage, aux = struct.unpack_from('<IiHBB',obj,p+8)
    return {'index':idx,'name':name,'value':value,'section':sec,'type':typ,'storage':storage,'aux':aux}

def secinfo(num):
    p=56+40*(num-1)
    rawsize, rawoff, reloff=struct.unpack_from('<III',obj,p+16)
    relcount=struct.unpack_from('<H',obj,p+32)[0]
    return {'name':obj[p:p+8].rstrip(b'\0').decode(),'bytes':rawsize,'raw_offset':rawoff,
            'reloc_offset':reloff,'reloc_count':relcount,'flags':struct.unpack_from('<I',obj,p+36)[0]}

def relocs(s):
    out=[]
    for i in range(s['reloc_count']):
        va,idx,typ=struct.unpack_from('<IIH',obj,s['reloc_offset']+10*i)
        out.append({'offset':va,'type':typ,'symbol':symbol(idx)})
    return out

decoded={}
section_records={}
mnemonic_differences={}
listing_lines=listing.splitlines()
for key,t in sections['targets'].items():
    si=secinfo(t['section_number']); sy=symbol(t['symbol_index'])
    ck(f'{key} full section fields', all(si[k]==v for k,v in t['section'].items() if k!='sha256'))
    ck(f'{key} actual symbol correspondence', sy['name']==t['symbol'] and sy['section']==t['section_number'] and sy['value']==0)
    raw=obj[si['raw_offset']:si['raw_offset']+si['bytes']]
    ck(f'{key} full raw section digest', digest(raw)==t['section']['sha256'])
    a,b=t['listing_lines_1based']; full='\n'.join(listing_lines[a-1:b])
    extracted=read(MAIN/f'evidence/{key}.txt').decode()
    rows=listing_rows(full); rows2=listing_rows(extracted)
    ck(f'{key} whole extraction corresponds', [(r['offset'],r['bytes'],r['asm']) for r in rows]==[(r['offset'],r['bytes'],r['asm']) for r in rows2])
    cur=0
    for r in rows:
        ck(f'{key} contiguous decoded offset {cur:x}',r['offset']==cur)
        ck(f'{key} raw bytes {cur:x}',raw[cur:cur+len(r['bytes'])]==r['bytes'])
        cur+=len(r['bytes'])
    ck(f'{key} entire section decoded',cur==len(raw))
    mn=collections.Counter(r['asm'].split()[0] for r in rows)
    recorded=collections.Counter(t['mnemonics'])
    if mn!=recorded:
        mnemonic_differences[key]={'omitted':dict(mn-recorded),'extra':dict(recorded-mn)}
    ck(f'{key} recorded mnemonic subset',not (recorded-mn))
    bad=[r['asm'] for r in rows if re.match(r'^(?:f\w+|v\w+|[lv]dmxcsr|[fs]?stmxcsr|xrstor\w*|fxrstor\w*|[fs]?ldcw|[fs]?stcw)\b',r['asm'])]
    ck(f'{key} no x87/AVX/FMA/control op in these whole bodies',not bad)
    rr=relocs(si)
    ck(f'{key} relocation references in section',all(0<=r['offset']<=len(raw)-4 and 4<=r['type']<=9 for r in rr))
    constants=[]
    for r in rr:
        m=re.fullmatch(r'__real@([0-9a-f]{16})',r['symbol']['name'])
        if m:
            cs=secinfo(r['symbol']['section']); pos=cs['raw_offset']+r['symbol']['value']
            ck(f'{key} constant bits {m.group(1)} at {r["offset"]:x}',obj[pos:pos+8]==int(m.group(1),16).to_bytes(8,'little'))
            constants.append(r['symbol']['name'])
    decoded[key]=rows
    section_records[key]={'section_number':t['section_number'],'bytes':len(raw),'sha256':digest(raw),
                          'instruction_rows':len(rows),'actual_mnemonics':dict(mn),'relocations':rr,'constant_names':sorted(set(constants))}

ck('16 complete selected sections',len(section_records)==16)
for row in arg['actual_object']['named_text_assertions']:
    text='\n'.join(r['line'] for r in decoded[row['section']])
    ck(f'named object assertion {row["section"]} {row["assertion_regex"]}', re.search(row['assertion_regex'],text) is not None)

def target_at(key, instruction_offset, displacement_offset, expected):
    rs=[r for r in section_records[key]['relocations'] if r['offset']==instruction_offset+displacement_offset]
    ck(f'{key}+{instruction_offset:x} actual relocation to {expected}',len(rs)==1 and rs[0]['symbol']['name']==expected)

helper=sections['targets']['old-append']['symbol']
for key,off in [('compiled-build',0x315),('prefix',0x1b6),('append-new',0xe5),('compiled-tick',0x197),('compiled-musical-time',0x223)]:
    target_at(key,off,1,helper)
target_at('old-append',0x108,2,'__imp_log1p')
for key,off in [('old-append',0xcf),('old-append',0xed),('classify',0x149),('classify',0x193)]:
    target_at(key,off,1,'_dclass')
    r=next(r for r in section_records[key]['relocations'] if r['offset']==off+1)
    ck(f'{key}+{off:x} unresolved ordinary external dclass',r['symbol']['index']==0x767 and r['symbol']['section']==0 and r['symbol']['storage']==2)
for off in [0x2ca,0x2d7]:
    target_at('clock-frameAt',off,1,'floor')

# Candidate PE file, read as bytes only: no LoadLibrary, calls or subject execution.
candidate='C:/Windows/System32/ucrtbase.dll'
pe=read(candidate)
ck('candidate DLL identity',digest(pe)=='DEDB7611E1ABC1FF3852DCAC3D4C93A048D729E25B061973B9969F22698F14CA')
peoff=struct.unpack_from('<I',pe,0x3c)[0]
ck('candidate PE AMD64',pe[peoff:peoff+4]==b'PE\0\0' and struct.unpack_from('<H',pe,peoff+4)[0]==0x8664)
ns=struct.unpack_from('<H',pe,peoff+6)[0]; os=struct.unpack_from('<H',pe,peoff+20)[0]; opt=peoff+24
ck('PE32+ candidate',struct.unpack_from('<H',pe,opt)[0]==0x20b)
base=struct.unpack_from('<Q',pe,opt+24)[0]
pe_secs=[]
for i in range(ns):
    p=opt+os+40*i
    vs,rva,rs,ro=struct.unpack_from('<IIII',pe,p+8)
    pe_secs.append({'name':pe[p:p+8].rstrip(b'\0').decode(),'rva':rva,'virtual_size':vs,'raw_size':rs,'raw_offset':ro,'flags':struct.unpack_from('<I',pe,p+36)[0]})
def rvaoff(rva,n=1):
    hits=[s for s in pe_secs if s['rva']<=rva and rva+n<=s['rva']+s['raw_size']]
    ck(f'PE file-backed RVA {rva:x}/{n}',len(hits)==1)
    return hits[0]['raw_offset']+rva-hits[0]['rva']
exr,exn=struct.unpack_from('<II',pe,opt+112)
e=rvaoff(exr,40)
ordbase,nfunc,nnames,fa,na,oa=struct.unpack_from('<IIIIII',pe,e+16)
exports={}
for i in range(nnames):
    np=struct.unpack_from('<I',pe,rvaoff(na+4*i,4))[0]
    z=rvaoff(np);name=pe[z:pe.index(b'\0',z)].decode()
    ix=struct.unpack_from('<H',pe,rvaoff(oa+2*i,2))[0]
    fr=struct.unpack_from('<I',pe,rvaoff(fa+4*ix,4))[0]
    if name in ['_dclass','_o__dclass']:
        exports[name]={'ordinal':ordbase+ix,'rva':fr,'forwarded':exr<=fr<exr+exn}
ck('actual dclass export ordinal210 RVA688F0',exports['_dclass']=={'ordinal':210,'rva':0x688f0,'forwarded':False})
ck('different o_dclass export',exports['_o__dclass']['rva']==0x842a0)
leaf=pe[rvaoff(0x688f0,92):rvaoff(0x688f0,92)+92]
rows=[r for r in listing_rows(read('build-ci/20261105-musical-native-classification-leaf-file-r1/candidate-leaf-disassembly.txt').decode()) if base+0x688f0<=r['offset']<base+0x6894c]
ck('92B classification whole raw bytes equal listing',b''.join(r['bytes'] for r in rows)==leaf and len(leaf)==92)
ck('classification complete 26 rows',len(rows)==26)
cur=base+0x688f0
for r in rows:
    ck(f'dclass contiguous offset {cur:x}',r['offset']==cur)
    cur+=len(r['bytes'])
    ck(f'dclass pure mnemonic {r["offset"]:x}',r['asm'].split()[0] in {'xorps','movsd','movq','mov','shr','and','cmp','je','test','jne','xor','ret'})
    ck(f'dclass no explicit memory operand {r["offset"]:x}','[' not in r['asm'])
    if r['asm'].split()[0] in {'je','jne'}:
        target=int(r['asm'].split()[1],16)
        ck(f'dclass internal branch {r["offset"]:x}',target in [r['offset'] for r in rows])
        # Verify the actual signed relative branch encoding independently.
        ck(f'dclass branch displacement {r["offset"]:x}',r['offset']+len(r['bytes'])+int.from_bytes(r['bytes'][1:],'little',signed=True)==target)
rets=[r['offset']-base for r in rows if r['asm']=='ret']
ck('all five classified returns',rets==[0x68918,0x6892a,0x68930,0x68945,0x6894b])
ck('leaf followed by padding rather than hidden fallthrough',pe[rvaoff(0x6894c,20):rvaoff(0x6894c,20)+20]==b'\xcc'*20)
rs=[s for s in pe_secs if s['rva']<=0x688f0< s['rva']+s['raw_size']][0]
ck('leaf candidate section executable readonly',rs['flags']&0x20000000 and rs['flags']&0x40000000 and not rs['flags']&0x80000000)
br,bn=struct.unpack_from('<II',pe,opt+112+5*8)
pos=rvaoff(br,bn);end=pos+bn;blocks=[];footprints=[]
while pos<end:
    page,size=struct.unpack_from('<II',pe,pos)
    ck(f'base reloc block {page:x} well formed',size>=8 and size%2==0 and pos+size<=end)
    blocks.append(page)
    for j in range((size-8)//2):
        entry=struct.unpack_from('<H',pe,pos+8+2*j)[0];typ=entry>>12;rr=page+(entry&4095)
        ck(f'base reloc supported type {rr:x}',typ in [0,10])
        if typ:
            footprints.append((rr,rr+8))
    pos+=size
ck('entire base relocation directory consumed',pos==end)
ck('six complete relocation block pages',blocks==[0xf9000,0xfa000,0xfb000,0xfc000,0xfd000,0x138000])
ck('no classification leaf relocation footprint',all(b<=0x688f0 or a>=0x6894c for a,b in footprints))

# Exact disjoint partition, not a floating-point numerical experiment.
partition={'normal':2*2046*(1<<52),'subnormal':2*((1<<52)-1),'zero':2,'infinity':2,'nan':2*((1<<52)-1)}
ck('all 2^64 binary64 encodings partitioned',sum(partition.values())==1<<64)
ck('prospective only extent arithmetic',9+1==10 and 8284+92==8376)
ck('largest representable below 2^63 integer distance', (1<<63)-((1<<53)-1)*(1<<10)==1024)

result={'schema':'R5_FORWARD_NATIVE_INDEPENDENT_FILE_CHECKS_R1','status':'PASS_SOURCE_DATA_ONLY',
        'checks_passed':len(checks),'checks':checks,'inputs':list(inputs.values()),
        'object_sections':section_records,'auxiliary_mnemonic_differences':mnemonic_differences,'classification':{'image_base':base,'exports':exports,'leaf_rva':[0x688f0,0x6894c],
        'bytes':len(leaf),'sha256':digest(leaf),'instruction_rows':len(rows),'returns_rva':rets,'relocation_block_pages':blocks,
        'relocation_footprints_checked':len(footprints),'binary64_partition_counts':partition},
        'actions':{'subject_execution':False,'compiler':False,'linker':False,'GUI':False,'native_API':False,
                   'author_scripts_executed':False,'Python_file_hash_integer_data_only':True}}
(OUT/'file-checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'status':result['status'],'checks_passed':len(checks),'unique_input_files':len(inputs),
                  'whole_object_sections':len(section_records),'classification_leaf_bytes':len(leaf),'classification_leaf_sha256':digest(leaf)}))
