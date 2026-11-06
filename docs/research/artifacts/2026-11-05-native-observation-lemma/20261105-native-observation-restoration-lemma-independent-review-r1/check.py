"""Independent read-only PE/disassembly/bit algebra; no native code invocation."""
from pathlib import Path
import hashlib
import json
import re
import struct

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
SUB=ROOT/'build-ci/20261105-native-observation-restoration-lemma-r1'
checks=[]
def ck(name,actual,expected):
    checks.append(dict(name=name,actual=actual,expected=expected,passed=actual==expected))
def sha(b):return hashlib.sha256(b).hexdigest().upper()
ck('subject source pins',sha((SUB/'source-pins.json').read_bytes()),'231B65E56FD6F2E99AD664687B9CF9B52C594959C8473713B120590B0D51A56C')
ck('subject whole lemma',sha((SUB/'lemma.json').read_bytes()),'8F88017B8FFAE57AA73DE744BCCB5B98F515D7C1ED1CB6CF3288DC4947272D9A')
pins=json.loads((SUB/'source-pins.json').read_bytes())
d=json.loads((SUB/'lemma.json').read_bytes())
for p in pins['files']:
    b=(ROOT/p['path']).read_bytes()
    ck('whole subject length '+p['path'],len(b),p['bytes'])
    ck('whole subject hash '+p['path'],sha(b),p['sha256'])
for p in d['pins']:
    path=Path(p['path'])
    if not path.is_absolute():path=ROOT/path
    b=path.read_bytes()
    ck('underlying input length '+p['path'],len(b),p['bytes'])
    ck('underlying input hash '+p['path'],sha(b),p['sha256'])

raw=Path('C:/Windows/System32/ucrtbase.dll').read_bytes()
pe=int.from_bytes(raw[60:64],'little'); opt=pe+24
ck('DOS signature',raw[:2].hex(),'4d5a')
ck('PE signature',raw[pe:pe+4].hex(),'50450000')
ck('AMD64 machine',int.from_bytes(raw[pe+4:pe+6],'little'),0x8664)
ck('PE32plus',int.from_bytes(raw[opt:opt+2],'little'),0x20b)
section_table=opt+int.from_bytes(raw[pe+20:pe+22],'little')
sections=[]
for i in range(int.from_bytes(raw[pe+6:pe+8],'little')):
    at=section_table+40*i
    virtual_size,rva,size,offset=struct.unpack_from('<IIII',raw,at+8)
    sections.append((rva,size,offset))
def code(rva,n):
    rows=[(base,size,off) for base,size,off in sections if base<=rva and rva+n<=base+size]
    if len(rows)!=1:raise ValueError('not one raw extent')
    base,size,off=rows[0]
    return raw[off+rva-base:off+rva-base+n]
functions={x['name']:x for x in d['verified_function_spans']}
total=0; rows=0
for name,f in functions.items():
    cursor=f['begin_rva']
    for row in f['instructions']:
        b=bytes.fromhex(row['hex'])
        ck(name+' contiguous '+hex(cursor),row['rva'],cursor)
        ck(name+' fresh PE bytes '+hex(cursor),code(cursor,len(b)).hex(),b.hex())
        cursor+=len(b); rows+=1
    ck(name+' complete end',cursor,f['end_rva'])
    total+=cursor-f['begin_rva']
ck('nine complete code extents',len(functions),9)
ck('all displayed code bytes',total,1427)
ck('all instruction rows',rows,411)

# Independent bit permutation derived one bit at a time from the actual
# status/set helper masks/shifts/NEG-SBB and BT-SETB sequences.
hardware_to_abstract=[4,5,3,2,1,0]
def to_flags(s):
    return sum(((s>>i)&1)<<j for i,j in enumerate(hardware_to_abstract))
def from_flags(t):
    return sum(((t>>j)&1)<<i for i,j in enumerate(hardware_to_abstract))
pair_count=0
for old in range(64):
    t=to_flags(old)
    ck('six-bit inverse '+str(old),from_flags(t),old)
    supplied=(t&31)|((t&31)<<24)
    row=d['status_mapping_cases'][old]
    ck('emitted abstract '+str(old),row['abstract_six'],t)
    ck('emitted fexcept_t '+str(old),row['fegetexceptflag_all'],supplied)
    for now in range(64):
        # FE_ALL_EXCEPT masks the abstract named five, leaving hardware DE bit1.
        clear=from_flags(to_flags(now)&~31)
        set_status=from_flags((to_flags(clear)&~31)|(t&31))
        ck('named clear '+str(old)+'/'+str(now),clear,now&2)
        ck('five restored '+str(old)+'/'+str(now),set_status,(old&~2)|(now&2))
        ck('six raw restored '+str(old)+'/'+str(now),(set_status&~63)|(old&63),old)
        pair_count+=1
ck('all status pairs',pair_count,4096)

def target(s):
    match=re.fullmatch(r'(?:j\S+|call) ([0-9A-F]{16})',s)
    return int(match[1],16)-0x180000000 if match else None
def selected_cfg(name,prune):
    f=functions[name]; nodes={r['rva']:r for r in f['instructions']}
    pending=[f['begin_rva']]; seen=set()
    while pending:
        at=pending.pop()
        if at in seen:continue
        seen.add(at); row=nodes[at]; s=row['assembly']; next_at=at+len(bytes.fromhex(row['hex']))
        if s=='ret':continue
        if s.startswith('jmp '):pending.append(target(s));continue
        if s.startswith('j'):
            if at not in prune or prune[at]:pending.append(target(s))
            if at not in prune or not prune[at]:pending.append(next_at)
        else:
            # Calls are represented as a leaf return edge here. Their concrete
            # target closure is checked separately, not assumed callback-pure.
            pending.append(next_at)
    return [nodes[x] for x in sorted(seen)]
outer=selected_cfg('_controlfp_s',{0x5fbbb:False,0x5fbc6:False})
inner=selected_cfg('control_helper_5FC10',{0x5fcc7:False})
ck('selected wrapper sole call',[target(r['assembly']) for r in outer if r['assembly'].startswith('call ')],[0x5fc10])
ck('selected inner no calls',[r['assembly'] for r in inner if r['assembly'].startswith('call ')],[])
ck('selected inner no MXCSR load',[r['assembly'] for r in inner if r['assembly'].startswith('ldmxcsr ')],[])
ck('selected inner no absolute global memory',[r['assembly'] for r in inner if '[0000000180' in r['assembly']],[])
ck('selected equality branch reached',any(r['rva']==0x5fcc4 for r in inner),True)
ck('selected unequal branch excluded',any(r['rva']==0x5fcd8 for r in inner),False)
for f in d['verified_function_spans']:
    for r in f['instructions']:
        if r['assembly'].startswith('call ') and f['name'] not in ('_controlfp_s','control_helper_5FC10'):
            ck('status leaf selected concrete callee '+hex(r['rva']),target(r['assembly']) in (0x558b0,0x55930),True)

fail=[x for x in checks if not x['passed']]
result=dict(schema='INDEPENDENT_CANDIDATE_FENV_LEMMA_FILE_REVIEW_R1',
 scope='Candidate file/bit/control-flow argument only. No native invocation, state snapshot, final loader/provider association or NativeFact.',
 verdict='ACCEPT_SCOPED_CONDITIONAL_LEMMA' if not fail else 'REVISE',
 checks=len(checks),failures=fail,total_code_bytes=total,instruction_rows=rows,status_pairs=pair_count,
 selected_control_outer_rvas=[r['rva'] for r in outer],selected_control_inner_rvas=[r['rva'] for r in inner],
 analytical_premises=['Valid live caller fexcept_t/current-word writable pointers and stock Windows x64 ABI/home/stack ownership.',
 'Mask0/new0 makes invalid-new test false; nonnull pointer excludes null path. At helper 5FCB7 both masked operands yield exactly decoded old word for every reached decoding branch, making JNE5FCC7 false.',
 'The set helper reads only saved upper six abstract bits and writes currentMXCSR higher26 bits unchanged plus the inverse low-six permutation. Valid supported controls remain an admission/interval premise.',
 'fegetround/status leaves use integer/stack observations; no selected errno/global/callback effect. Raw final restoration remains a source argument, not native interval observation.'],
 held=['_errno TLS/lazy/cold path and its final post-restoration use','get_new_handler plus callback/control/FMA interval','floor ISA-selected provider/dispatch','actual final object/import/thunk/loaded-module byte association/lifetime','source funding/resource/binding/GO'],
 subject_pins_sha256='231B65E56FD6F2E99AD664687B9CF9B52C594959C8473713B120590B0D51A56C',
 subject_lemma_sha256='8F88017B8FFAE57AA73DE744BCCB5B98F515D7C1ED1CB6CF3288DC4947272D9A')
(OUT/'review.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(verdict=result['verdict'],checks=len(checks),failures=len(fail),review_sha256=sha((OUT/'review.json').read_bytes()))))
raise SystemExit(1 if fail else 0)
