"""Finite source/data checker only; never loads or invokes candidate native code."""
from pathlib import Path
import hashlib
import json
import re
import struct

root=Path(__file__).resolve().parents[1]
out=root/'build-ci/20261105-native-observation-restoration-lemma-r1'
assert not out.exists(), 'Preserve complete checker attempts'
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
inputs=[('20261105-native-observer-candidate-leaves-file-r1','2DEF19170ACAA0C8106178AFA3510FA4B294AA67970F2F16A3D37CDDB99F30BF'),('20261105-native-observer-candidate-leaves-file-r2','4B1A0C38B2F334C68CE6908D5F0EC2C2AAF4A43F261C6402A4A9D78A13986ACE')]
receipts={}
pins=[]
for folder,expected in inputs:
    p=root/'build-ci'/folder/'receipt.json';b=p.read_bytes();assert sha(b)==expected
    receipts[folder]=json.loads(b);pins.append(dict(path=p.relative_to(root).as_posix(),bytes=len(b),sha256=expected))
candidate=Path('C:/Windows/System32/ucrtbase.dll');raw=candidate.read_bytes()
assert sha(raw)=='DEDB7611E1ABC1FF3852DCAC3D4C93A048D729E25B061973B9969F22698F14CA'
pins.append(dict(path=str(candidate),bytes=len(raw),sha256=sha(raw)))
pe=struct.unpack_from('<I',raw,60)[0];opt=pe+24
assert raw[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',raw,opt)[0]==0x20B
base=struct.unpack_from('<Q',raw,opt+24)[0];sections=opt+struct.unpack_from('<H',raw,pe+20)[0];n=struct.unpack_from('<H',raw,pe+6)[0]
def offset(rva):
    for at in range(sections,sections+40*n,40):
        size,va,stored,location=struct.unpack_from('<IIII',raw,at+8)
        if va<=rva<va+max(size,stored):assert rva-va<stored;return location+rva-va
    raise AssertionError('RVA_OUTSIDE_RAW_SECTION')
def decode(path,begin,end):
    packet=[];current=None
    for line in path.read_text().splitlines():
        match=re.match(r'^\s+([0-9A-F]{16}):\s+(.*)$',line)
        if match:
            parts=match[2].split();cells=[]
            while parts and re.fullmatch('[0-9A-F]{2}',parts[0]):cells.append(parts.pop(0))
            assert cells
            current=dict(rva=int(match[1],16)-base,hex=''.join(cells),assembly=' '.join(parts))
            packet.append(current)
        elif current and re.fullmatch(r'\s+(?:[0-9A-F]{2}(?:\s+|$))+',line):
            current['hex']+=''.join(line.split())
    selected=[r for r in packet if begin<=r['rva']<end]
    cursor=begin
    for row in selected:
        b=bytes.fromhex(row['hex']);assert row['rva']==cursor and cursor+len(b)<=end
        assert raw[offset(cursor):offset(cursor)+len(b)]==b
        cursor+=len(b)
    assert cursor==end
    return selected
r1='20261105-native-observer-candidate-leaves-file-r1';r2='20261105-native-observer-candidate-leaves-file-r2'
specs=[(r1,'fegetround',0x56830,0x5686A),(r1,'fetestexcept',0x556E0,0x556FE),(r1,'fegetexceptflag',0x55710,0x55747),(r1,'feclearexcept',0x54D40,0x54D93),(r1,'fesetexceptflag',0x55800,0x5589E),(r1,'_controlfp_s',0x5FBA0,0x5FC09),(r2,'status_helper_55930',0x55930,0x5597A),(r2,'set_helper_558B0',0x558B0,0x55923),(r2,'control_helper_5FC10',0x5FC10,0x5FEFD)]
functions=[]
for folder,name,begin,end in specs:
    p=root/'build-ci'/folder/(name+'.disassembly.txt');b=p.read_bytes()
    record=next(r for r in receipts[folder]['commands'] if r['name']==name)
    assert record['raw_exit']==0 and record['stderr_bytes']==0 and sha(b)==record['stdout_sha256']
    pins.append(dict(path=p.relative_to(root).as_posix(),bytes=len(b),sha256=sha(b)))
    rows=decode(p,begin,end)
    functions.append(dict(name=name,begin_rva=begin,end_rva=end,bytes=end-begin,instructions=rows))
def abstract(s):return ((s>>5)&1)|((s>>3)&2)|((s>>1)&4)|((s&4)<<1)|((s&1)<<4)|((s&2)<<4)
def hardware(t):return (32 if t&1 else 0)|(16 if t&2 else 0)|(8 if t&4 else 0)|(4 if t&8 else 0)|(1 if t&16 else 0)|(2 if t&32 else 0)
assert len({abstract(s) for s in range(64)})==64
cases=[]
for old in range(64):
    assert hardware(abstract(old))==old
    flags=(abstract(old)&31)*((1<<24)+1)
    for current in range(64):
        current_flags=abstract(current)*((1<<24)+1)
        cleared=hardware(((current_flags&~(31|(31<<24)))>>24)&63)
        assert cleared==(current&2)
        merged=(((abstract(cleared)*((1<<24)+1))&~(31|(31<<24)))|(flags&(31|(31<<24))))
        set_status=hardware((merged>>24)&63)
        assert (set_status&~2)==(old&~2) and (set_status&2)==(current&2)
        final=(set_status&~63)|(old&63)
        assert final==old
    cases.append(dict(original_raw_six=old,abstract_six=abstract(old),fegetexceptflag_all=flags))
assert (~63)&63==0
rounding=[dict(mxcsr_rounding=b,fegetround=r) for b,r in [(0,0),(0x2000,0x100),(0x4000,0x200),(0x6000,0x300)]]
out.mkdir()
result=dict(schema='CANDIDATE_OBSERVATION_RESTORATION_SOURCE_LEMMA_R1',scope='Candidate bytes plus conditional bit/status algebra only; not actual linked/runtime/native binding or target execution',verified_function_spans=functions,status_mapping_cases=cases,status_pair_cases=4096,rounding_partition=rounding,selected_controlfp_arguments=dict(new=0,mask=0,nonnull_current_word=True),controlfp_zero_mask_identity='(~0 & current_abstract) | (new & 0) == current_abstract; comparison equal at5FCC4 bypasses all control writes/global accesses/callbacks',remaining=['actual final call/provider/loaded-module and readonly lifetime association','_errno TLS/lazy initialization and cold helpers retain stock SDK/ABI behavior premise','get_new_handler and actual handler/dispatch interval association','floor selected SDK contract/provider and extra dispatch are outside this fenv lemma','all source funding/physical resource/whole API binding and separate execution GO'],target_invoked=False,pins=pins)
(out/'lemma.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(functions=len(functions),bytes=sum(f['bytes'] for f in functions),instruction_rows=sum(len(f['instructions']) for f in functions),status_pair_cases=4096,lemma_sha256=sha((out/'lemma.json').read_bytes()))))
