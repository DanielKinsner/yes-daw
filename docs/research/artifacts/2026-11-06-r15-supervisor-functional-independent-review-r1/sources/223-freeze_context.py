"""New physical-C1 oracles. Source authored only; root Freeze GO required."""
import argparse
from collections import Counter
from fractions import Fraction as F
import hashlib
import json
import math
from pathlib import Path
import struct

REV=Path(__file__).resolve().parent
ROOT=REV.parents[3]
CAP=512*1024*1024
BATCH=1024*1024*1024
WORK=1<<28
CALLS=1<<20
OTHER=48*1024*1024

def need(ok,why):
    if not ok:raise RuntimeError(why)

class Budget:
    def __init__(self,prior=0,stage_limit=14000000):
        need(0<=prior<=260000000 and 0<stage_limit<=WORK,'parent/stage budget domain')
        self.used=prior;self.prior=prior;self.limit=min(WORK,prior+stage_limit);self.categories=Counter()
    def fixed(self,n,kind):self.spend(n,kind)
    def spend(self,n=1,kind='source_visits'):
        need(n>=0 and n<=self.limit-self.used,'MusicalResourceLimit: prepaid stage work')
        self.used+=n;self.categories[kind]+=n

def rational(bits):
    exponent=(bits>>23)&255;sig=bits&0x7fffff;need(0<=bits<=0xffffffff and exponent!=255,'UnsupportedArithmetic: nonfinite bits')
    if exponent:sig|=1<<23
    power=exponent-150 if exponent else -149
    value=F(sig<<power) if power>=0 else F(sig,1<<-power)
    return -value if bits>>31 else value

def encode(v,negative_zero=False):
    v=F(v)
    if not v:return 0x80000000 if negative_zero else 0
    sign=0x80000000 if v<0 else 0;v=abs(v);n,d=v.numerator,v.denominator;e=n.bit_length()-d.bit_length()
    if (F(1<<e) if e>=0 else F(1,1<<-e))>v:e-=1
    shift=149 if e < -126 else 23-e
    q,r=divmod(n<<shift,d) if shift>=0 else divmod(n,d<<-shift)
    divisor=d if shift>=0 else d<<-shift;q+=int(2*r>divisor or (2*r==divisor and q&1))
    if e < -126:return sign|q
    if q==1<<24:q>>=1;e+=1
    need(e<=127,'UnsupportedArithmetic: binary32 overflow')
    return sign|((e+127)<<23)|(q-(1<<23))

def add(a,b):return encode(rational(a)+rational(b),a==b==0x80000000)
def mul(a,b):return encode(rational(a)*rational(b),bool((a^b)>>31))
def div(a,b):
    need(rational(b)!=0,'UnsupportedArithmetic: divide by zero')
    return encode(rational(a)/rational(b),bool((a^b)>>31))
def round_away(bits):
    v=rational(bits);need(v>=0,'negative callback offset');q,r=divmod(v.numerator,v.denominator);return q+int(2*r>=v.denominator)
def p2(n):need(n>0,'ring size');return 1<<(n-1).bit_length()
def fast_size(n):
    factor=1
    while n>=32:n=(n+1)//2;factor*=2
    while n not in (0,1,2,3,4,6,8,9,12,16,18,24):n+=1
    return factor*n

# name, law, windows, preset, calls, syntheses, analyses, trace bound, logical, reverse
CASES=[
 ('small-mono',0,4,0,18,32,64,488903,1,False),
 ('small-equivalent-reversed',0,4,0,18,32,64,488903,2,True),
 ('small-equivalent-gaps',4,11,0,46,70,140,1069513,2,False),
 ('small-equivalent-zero',3,4,0,18,0,0,7591,2,False),
 ('small-equivalent-quiet',5,4,0,18,0,0,7591,2,False),
 ('default-equivalent',0,2,1,14,16,32,5868307,2,False)]
CONTROL=('old-C2-equivalent-control',0,4,0,18,32,64,488903,2,False)

# Original representative law and exact outside-zero law are separate from Asset sentinels.
def law(rate,code,c,x,n,reverse=False):
    if x<0 or x>=n:return 0
    if reverse:x=n-1-x
    if code==3:return 0x80000000 if x&1 else 0
    if code==4 and (512<=x<576 or 832<=x<1856 or x>=2368):return 0
    amplitude=1e-10 if code==5 else 0.1
    return struct.unpack('<I',struct.pack('<f',amplitude*(0.23+math.sin(2*math.pi*997*float(x)/rate))))[0]

def original_bit(rate,code,c,x,n):
    if x<7:return 0x3e800000 if c==0 else 0xbf000000
    if x>=7+n:return 0x3f400000 if c==0 else 0xbe000000
    value=law(rate,code,0,x-7,n)
    return value^0x80000000 if c==1 and value&0x7fffffff==0 else value


def source_row_bounds(c,w,h,k,b,n,calls,synth,analyses,fast,quiet_clears):
    # Complete observed tags, allowing every makeOutput fallback. RF includes FZ.
    normal=calls-2-fast;r=2*n+w
    observed=(analyses*(4+2*k+3*b)+synth*(94*b+4)
        +synth*(1+c*(h+2+b+k+w))+(10*c+2)*b+k+9
        +calls+(calls-1)+normal+1+fast+c*r+4*c*b*quiet_clears+2*c*b+c*w)
    base=analyses*w+c*r+3*calls+2+(calls-1)+2*synth+(calls-1)+normal+fast
    return observed,base

def digest(path,budget):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        while True:
            budget.spend(kind='hash_chunk_calls');chunk=stream.read(1024*1024)
            if not chunk:break
            h.update(chunk)
    return h.hexdigest().upper()

class FrozenCase:
    def __init__(self,out,definition,budget):
        self.out=out;self.budget=budget;self.name,self.code,windows,self.preset,self.expected_calls,self.expected_synth,self.expected_analyses,self.rows,self.logical,self.reversed=definition
        budget.spend(4096,'bounded_case_setup_allowance')
        self.rate=48000;self.c=1
        ratef=struct.unpack('<f',struct.pack('<f',self.rate))[0]
        raww,rawh=ratef*0.12,ratef*0.03
        need(0<raww<2**31 and 0<rawh<2**31,'default int domain')
        self.w,self.h=(int(raww),int(rawh)) if self.preset else (256,64)
        need((self.w,self.h)==((5760,1440) if self.preset else (256,64)),'default promoted binary64 preset order')
        self.k=2*fast_size((self.w+1)//2);self.bands=self.k//2;self.il=self.w//2;self.ol=self.w-self.il
        self.n=windows*self.w;self.d=2*self.n;self.r=self.d+self.w;self.relation_cap=5*self.c*self.d
        fast=16 if self.code in (3,5) else 9 if self.code==4 else 0
        quiet_clears=1 if self.code in (3,5) else 2 if self.code==4 else 0
        self.observed_upper,self.base_upper=source_row_bounds(self.c,self.w,self.h,self.k,self.bands,self.n,self.expected_calls,self.expected_synth,self.expected_analyses,fast,quiet_clears)
        need(self.observed_upper+self.base_upper<=self.rows,'MusicalResourceLimit: source tag row upper bound')
        need(self.rows*64+self.relation_cap*96+OTHER<=CAP,'MusicalResourceLimit: predeclared case payload')
        # Includes every frozen stream, source file, JSON value/label and metadata allowance.
        frozen_bound=(64*self.base_upper+4*self.c*(self.n+self.w+self.h)
            +4*self.logical*(self.n+14)+16*self.expected_analyses*self.w
            +512*(self.expected_analyses+self.expected_synth+self.expected_calls)+16384)
        need(frozen_bound<=32*1024*1024,'MusicalResourceLimit: frozen payload BEFORE files/buffers')
        self.frozen_upper=frozen_bound;out.mkdir()
        self.input_stride=self.w+self.h+1;self.input_capacity=p2(self.c*self.input_stride)
        self.output_stride=self.w+self.h;self.output_capacity=p2(self.c*self.output_stride)
        budget.spend(self.input_capacity+self.c*2,'initial_owner_entries')
        self.ring=[(0,None)]*self.input_capacity;self.current=[0]*self.c;self.previous=[0]*self.c
        self.state=[self.w,self.h,self.c,0,-1,2*self.w,1,0,1,encode(1)]
        self.raw=self.stft=self.history=self.ordinal=self.callbacks=self.fast=self.quiet_nonzero=0
        self.analyses=[];self.syntheses=[];self.call_rows=[];self.base_rows=0;self.payload=0;self.before_flush=None
        self.base=(out/'expected-base.txt').open('w',encoding='ascii',newline='\n')
        self.call_file=(out/'calls.txt').open('w',encoding='ascii',newline='\n')
        self.operands=(out/'operands.bin').open('wb')
        self.call_file.write(f'F {self.name} {self.rate} 1 {self.w} {self.h} {self.n} 2 1\n')
        self.event('F '+self.name);self.event('B '+' '.join(map(str,self.state)))
    def event(self,line):
        self.budget.spend(kind='frozen_base_rows');need(len(line)+1<=64,'base tag/digit bound')
        self.base_rows+=1;self.payload+=len(line)+1;need(self.payload<=OTHER,'MusicalResourceLimit: frozen payload')
        self.base.write(line+'\n')
    def slot(self,c,index):return (self.history+index+c*self.input_stride)&(self.input_capacity-1)
    def samples(self,start,count):
        self.budget.spend(self.c*count,'source_law_samples')
        return [[law(self.rate,self.code,c,start+i,self.n,self.reversed) for i in range(count)] for c in range(self.c)]
    def store(self,values,source,start):
        for c in range(self.c):
            for i in range(start,len(values[c])):
                self.budget.spend(kind='history_owner_writes');self.ring[self.slot(c,i)]=(values[c][i],(c,source+i))
    def analyse(self,values,source,offset,previous,synthesis):
        for c in range(self.c):
            self.budget.spend(kind='analysis_entries');bits=[];aid=len(self.analyses)+1
            for i in range(self.w):
                self.budget.spend(kind='analysis_operand_visits');index=offset+i
                if index<0:
                    bit,owner=self.ring[self.slot(c,index)]
                    need(owner is None or owner==(c,source+index),'MusicalBoundaryContextUnsupported: stale history')
                    need(owner is not None or bit==0,'unowned nonzero history');kind=1 if previous else 0
                else:
                    need(index<len(values[c]),'MusicalBoundaryContextUnsupported: source overread');bit=values[c][index];kind=3 if previous else 2
                bits.append(bit);self.event(f'R {kind} {c} {index} {bit} {source+index}')
            self.analyses.append(dict(id=aid,synthesis=synthesis,call=self.ordinal,previous=int(previous),channel=c,start=source+offset,center2=2*(source+offset)+self.w,bits=bits))
            (self.previous if previous else self.current)[c]=aid
    def io(self,kind,values,O,source):
        I=len(values[0]);self.budget.spend(kind='call_entries')
        self.budget.spend(self.c,'plane_shape_visits');need(len(values)==self.c and all(len(v)==I for v in values),'planar input shape')
        need(self.ordinal<CALLS and 0<=I<=(self.w+self.h if kind==0 else 2048) and 0<=O<=(self.w if kind==2 else 2048),'call domain')
        if kind==1:need(I>0 and O>0,'positive ordinary I/O')
        call=dict(ordinal=self.ordinal,kind=kind,input=I,output=O,source=source,raw=self.raw,stft=self.stft,history=self.history,operand_offset=self.operands.tell(),quiet_clear=False)
        self.call_rows.append(call);line=f'C {self.ordinal} {kind} {I} {O} {source} {self.raw} {self.stft} {self.history}'
        self.call_file.write(line+'\n');self.event(line);self.event('B '+' '.join(map(str,self.state)))
        for plane in values:
            for bit in plane:self.budget.spend(kind='operand_file_entries');self.operands.write(struct.pack('<I',bit))
        if kind==2:
            self.before_flush=dict(raw=self.raw,stft=self.stft,history=self.history)
            for c in range(self.c):
                for i in range(O):self.event(f'Y 3 {c} {i} {i} *')
            self.state[3]-=O;self.state[8]=1;self.raw+=O;self.stft+=O
        else:
            energy=0;start=max(0,I-self.w-self.h) if kind==0 else 0
            for c in range(self.c):
                for bit in values[c][start:]:
                    self.budget.spend(2,'binary32_energy_calls');energy=add(energy,mul(bit,bit))
            threshold=encode(F(1,10**15));low=rational(energy)<rational(threshold)
            self.event(f'E {kind} {energy} {threshold}');call['energy']=energy
            if kind==0:
                self.budget.spend(self.input_capacity,'history_reset_entries');self.ring=[(0,None)]*self.input_capacity
                self.store(values,source,start)
                if not low:self.state[5:7]=[0,1]
                self.history+=I;self.event(f'A {I}');self.state[7]=1;self.state[9]=encode(2)
                call['fast']=False
            elif low and self.state[5]>=2*self.w:
                self.fast+=1;call['fast']=True;call['quiet_clear']=bool(self.state[6]);self.event(f'Q {I} {O}');self.state[6]=0
                for i in range(O):
                    for c in range(self.c):
                        index=i%I;bit=values[c][index];self.event(f'Y 1 {c} {i} {index} {bit}')
                        self.quiet_nonzero+=int(bool(bit&0x7fffffff))
                self.store(values,source,max(0,I-self.w-self.h));self.history+=I;self.event(f'A {I}');self.raw+=O
            else:
                call['fast']=False
                if low:self.state[5]+=I
                else:self.state[5:7]=[0,1]
                for i in range(O):
                    self.budget.spend(kind='output_schedule_positions')
                    while self.state[3]<=i:
                        self.budget.spend(kind='callback_entries');offset=self.state[3]
                        self.budget.spend(2,'binary32_offset_calls');io=round_away(div(mul(encode(offset),encode(I)),encode(O)))-self.w
                        need(io==(2*offset*I+O)//(2*O)-self.w,'UnsupportedArithmetic: offset exact rounding')
                        interval=io-self.state[4];self.state[4]=io;fresh=int(bool(self.state[7] or interval>0));sought=self.state[7]
                        self.event(f'D {offset} {io} {interval} {fresh} {sought}');self.callbacks+=1
                        sid=self.callbacks
                        if fresh:
                            self.analyse(values,source,io,False,sid);self.state[8]=0
                            if sought or interval!=self.h:self.analyse(values,source,io-self.h,True,sid)
                        factor=self.state[9] if sought else div(encode(self.h),encode(max(1,interval)))
                        need(factor==encode(2) and fresh==1,'frozen clean factor-two branch differs')
                        self.event(f'T {factor}')
                        self.syntheses.append(dict(id=sid,call=self.ordinal,offset=offset,input_offset=io,input_interval=interval,fresh=fresh,sought=sought,start=self.stft+offset,current=list(self.current),previous=list(self.previous),time_factor_bits=factor))
                        self.budget.spend(2*self.c,'analysis_owner_copies');self.previous=list(self.current)
                        self.state[7]=0;self.state[3]+=self.h
                    for c in range(self.c):self.event(f'Y 0 {c} {i} {i} *')
                self.store(values,source,max(0,I-self.w));self.history+=I;self.event(f'A {I}');self.stft+=O;self.event(f'S {O}')
                self.state[3]-=O;self.state[4]-=I;self.raw+=O
        self.event('Z '+' '.join(map(str,self.state)));self.ordinal+=1
    def write_json(self,name,data):
        self.budget.spend(kind='frozen_json_serialization_calls')
        text=json.dumps(data,separators=(',',':'))+'\n';self.payload+=len(text)
        need(self.payload<=OTHER,'MusicalResourceLimit: frozen other payload');(self.out/name).write_text(text,encoding='utf-8',newline='\n')
    def asset(self):
        # All selected/unselected original logical planes freeze before any vendor execution.
        count=self.logical*(self.n+14);self.budget.spend(count*3+4096,'original_owner_serialization')
        need(4*count+4096<=32*1024*1024,'original owner preflight')
        with (self.out/'asset.bin').open('xb') as stream:
            for x in range(self.n+14):
                for c in range(self.logical):stream.write(struct.pack('<I',original_bit(self.rate,self.code,c,x,self.n)))
        self.asset_hash=digest(self.out/'asset.bin',self.budget)
        value=f'{self.logical} {self.n+14} 7 {self.n} {int(self.reversed)} {self.asset_hash}\n'
        self.budget.spend(4096,'bounded_asset_metadata');(self.out/'asset.txt').write_text(value,encoding='ascii',newline='\n')
    def run(self):
        self.asset()
        seek=self.w+self.h;self.io(0,self.samples(self.il-seek,seek),0,self.il-seek)
        I=1024 if self.preset else 64;x=0
        while x<self.n:
            self.budget.spend(kind='call_plan_visits');count=min(I,self.n-x);self.io(1,self.samples(self.il+x,count),2*count,self.il+x);x+=count
        self.io(2,[[]],self.w,self.il+self.n)
        refusal='MusicalQuietPathUnsupported' if self.quiet_nonzero else 'none'
        self.call_file.write('X '+refusal+'\n');self.base.close();self.call_file.close();self.operands.close()
        need((self.ordinal,self.callbacks,len(self.analyses))==(self.expected_calls,self.expected_synth,self.expected_analyses),'immutable expected lifecycle inventory differs')
        need(self.raw==self.r,'private frame count')
        expected_fast=16 if self.code in (3,5) else 9 if self.code==4 else 0
        need(self.fast==expected_fast and bool(self.quiet_nonzero)==(self.code==5),'frozen branch/refusal inventory differs')
        if self.code==4:need(self.before_flush==dict(raw=5632,stft=4480,history=3136),'gap heads differ')
        if self.code==5:need(law(self.rate,self.code,0,192,self.n)&0x7fffffff,'quiet source192 witness not nonzero')
        geometry=dict(frozen_payload_upper=self.frozen_upper,name=self.name,rate=self.rate,channels=self.c,logical_channels=self.logical,reversed=self.reversed,selected_offset=7,asset_frames=self.n+14,source_hash=self.asset_hash,preset=self.preset,law=self.code,W=self.w,H=self.h,K=self.k,bands=self.bands,rho=self.il,IL=self.il,OL=self.ol,N=self.n,D=self.d,R=self.r,
            input_stride=self.input_stride,input_capacity=self.input_capacity,output_stride=self.output_stride,output_capacity=self.output_capacity,source_min=self.il-self.w-self.h,source_end=self.il+self.n,
            retained_begin=self.ol,retained_end=self.ol+self.d,calls=self.ordinal,syntheses=self.callbacks,analyses=len(self.analyses),private_pcm_bit_compares=self.c*self.r,trace_base_row_limit=self.rows,observed_row_upper=self.observed_upper,base_row_upper=self.base_upper,event_line_limit=64,relation_limit=self.relation_cap,relation_line_limit=96,other_payload_limit=OTHER,expected_refusal=refusal,fast_calls=self.fast,quiet_nonzero=self.quiet_nonzero,before_flush=self.before_flush,after_flush=dict(raw=self.raw,stft=self.stft,history=self.history),base_rows=self.base_rows)
        need(self.base_rows==self.base_upper,'independent exact base-row count')
        self.budget.spend(len(self.analyses),'analysis_extent_visits')
        need(all(geometry['source_min']<=a['start'] and a['start']+self.w<=geometry['source_end'] for a in self.analyses),'analysis extent')
        self.budget.spend(4*self.c*self.bands+len(self.analyses)*(self.w+16)+len(self.call_rows)*20+len(self.syntheses)*20,'serialization_entry_allowance')
        for filename,data in [('geometry.json',geometry),('call-oracle.json',self.call_rows),('analysis-oracle.json',self.analyses),('synthesis-oracle.json',self.syntheses)]:self.write_json(filename,data)
        self.budget.spend(kind='geometry_text');(self.out/'geometry.txt').write_text(f'{self.name} {self.rate} {self.preset} {self.code} {self.w} {self.h} {self.k} 1 {self.n} {self.d} {self.r} {self.ordinal} {self.callbacks} {len(self.analyses)} {self.rows}\n',encoding='ascii',newline='\n')
        self.budget.spend(16,'case_artifact_inventory_allowance')
        need(sum(p.stat().st_size for p in self.out.iterdir() if p.is_file())<=32*1024*1024,'MusicalResourceLimit: case frozen payload reservation')
        return geometry

def write_controls(out,budget):
    # No unneeded context traces/oracles are copied into the1MiB control reservation.
    budget.spend(131072,'complete_physical_control_freeze_reservation')
    n=1024;logical=2;folder=out/'old-C2-equivalent-control'
    need(2*4*logical*(n+14)+4096+8192+65536+4*4608<1024*1024,'control frozen+actual preflight')
    folder.mkdir();asset=bytearray()
    for x in range(n+14):
        for c in range(logical):asset.extend(struct.pack('<I',original_bit(48000,0,c,x,n)))
    need(len(asset)==4*logical*(n+14),'control original bytes')
    def original(name,data):
        need(len(data)<=16384,'bounded physical control file');(folder/(name+'-asset.bin' if name else 'asset.bin')).write_bytes(data)
        h=hashlib.sha256(data).hexdigest().upper();(folder/(name+'-asset.txt' if name else 'asset.txt')).write_text(f'2 {n+14} 7 {n} 0 {h}\n',encoding='ascii',newline='\n')
    original('',asset);changed=bytearray(asset);index=((7+n-1)*2+1)*4
    bits=struct.unpack_from('<I',changed,index)[0];struct.pack_into('<I',changed,index,bits^1);original('distinct',changed)
    source=out/'small-mono';need((source/'calls.txt').stat().st_size<=4096 and (source/'operands.bin').stat().st_size<=8192,'bounded control lifecycle copy')
    calls=(source/'calls.txt').read_text(encoding='ascii');first,rest=calls.split('\n',1)
    need(first=='F small-mono 48000 1 256 64 1024 2 1','independent control source header')
    (folder/'calls.txt').write_text('F old-C2-equivalent-control 48000 1 256 64 1024 2 1\n'+rest,encoding='ascii',newline='\n')
    (folder/'operands.bin').write_bytes((source/'operands.bin').read_bytes())
    policy=dict(calls=18,physical_fault_channels=2,correct_physical_channels=1,N=1024,D=2048,R=2304,retained_begin=128,retained_end=2176,private_samples=4608,equality_pairs=2304,projection_repair_allowed=False,status='CONTROL_DATA_ONLY_UNRUN')
    (folder/'control-policy.json').write_text(json.dumps(policy,indent=2)+'\n',encoding='utf-8',newline='\n')

def freeze(out):
    # Exact static inventory/payload and all stage uppers are reserved before files/large buffers.
    need(260000000<=WORK,'parent source reservation')
    # Fresh independent source-counter worksheet; no candidate measurements.
    case_units=(97769,100883,248583,100883,100883,1003151)
    freezer_upper=sum(case_units)+1048576+131072+8192+1024+8192+10000+36
    need(freezer_upper==2859244 and freezer_upper<3000000,'complete independent Freeze source upper')
    need(828304896<=BATCH and 436962496<=CAP,'complete stage-overlap reservation')
    budget=Budget(stage_limit=3000000);budget.spend(1024*1024,'fixed_module_file_json_metadata_hash_allowance')
    need(not out.exists(),'never overwrite freeze');out.mkdir();inventory=[]
    for definition in CASES:
        budget.spend(128,'fixture_visits');case=FrozenCase(out/definition[0],definition,budget);inventory.append(case.run())
        budget.spend(len(case.analyses)*(case.w+16)+case.input_capacity,'bounded_case_retirement_allowance')
    budget.spend(6*6,'matrix_inventory_visits')
    totals=(sum(g['calls'] for g in inventory),sum(g['syntheses'] for g in inventory),sum(g['analyses'] for g in inventory),sum(g['private_pcm_bit_compares'] for g in inventory))
    need(totals==(132,150,300,43904),'independent six-case inventory')
    need(sum(g['trace_base_row_limit']*64+g['relation_limit']*96+OTHER for g in inventory)==827256320,'complete main payload reservation')
    # Independent existing-main source DATA supplies the exact separate C2 control lifecycle.
    write_controls(out,budget)
    program=dict(version='physical-c1-context-r3-identity-ordered-phase',channels=1,factor_bits=encode(2),noise_floor_bits=encode(F(1,10**15)),shared_fields=[6,7],per_channel_fields=[0,1,2,3,4,5],winner='only physical0',random_draws=0)
    budget.spend(8192,'phase_and_arithmetic_serialization_allowance')
    (out/'phase-program.json').write_text(json.dumps(program,indent=2)+'\n',encoding='utf-8',newline='\n')
    vectors=[('add',0x80000000,0x80000000),('add',0,0x80000000),('add',0x3f800000,0xbf800000),('mul',0x00800000,0x3f000000),('add',1,1)]
    (out/'arithmetic.txt').write_text(''.join(f'{op} {a} {b} '+str({'add':add,'mul':mul}[op](a,b))+'\n' for op,a,b in vectors),encoding='ascii',newline='\n')
    (out/'matrix.txt').write_text('6\n'+'\n'.join(g['name'] for g in inventory)+'\n',encoding='ascii',newline='\n')
    budget.spend(1024,'terminal_metadata');pins=[]
    for path in sorted(out.rglob('*')):
        budget.spend(4,'frozen_artifact_visits')
        if path.is_file():pins.append(dict(path=path.relative_to(out).as_posix(),sha256=digest(path,budget),bytes=path.stat().st_size))
    need(budget.used<=3000000,'Freeze subcounter upper')
    receipt=dict(status='six_C1_oracles_frozen_candidate_unrun',freeze_work=budget.used,runner_allowance_both_invocations=2097152,freezer_source_upper=freezer_upper,freezer_case_source_uppers=case_units,parent_reserved_work=260000000,stage_reserved_work=14000000,work_accounting=dict(budget.categories),fixtures=inventory,files=pins,
        proposed_main_trace_base_rows=7930808,proposed_retained_relations=184320,proposed_payload_main_plus_controls=828304896,terminal_flush_c_times_w_included=True,source_bounds_not_measurements=True,center_gate_frames=2,context_certified=False,waveform_run=False,resource_certificate=False,production_accepted=False,public_pcm_samples=0)
    budget.spend(8192,'final_receipt_serialization')
    receipt['freeze_work']=budget.used
    (out/'freeze-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);a=p.parse_args()
    need(a.output.resolve().parent.parent==REV/'runs','new revision run required');freeze(a.output.resolve())
