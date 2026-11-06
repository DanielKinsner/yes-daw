"""Stream actual physical-C1 observations against frozen owners, phase program and exact arithmetic."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
from freeze_context import Budget,CAP,BATCH,WORK,OTHER,encode,rational,add,mul,div,need

def finite(bits):return 0<=bits<=0xffffffff and (bits>>23)&255!=255
def center_ok(y2,q):return abs(y2-2*q)<=4
def rotate(i,k,rho):return i+k-rho if i<rho else i-rho
def coverage_ok(expected,actual):return expected==actual
def quiet_refusal_ok(nonzero,refusal):return not nonzero or refusal=='MusicalQuietPathUnsupported'

def boot_program(c,b,k):
    for channel in range(c):
        for band in range(b):
            for field in (0,1,2,3):yield ('FW',field,channel,band)
    for band in range(b):
        for field in (6,7):yield ('FW',field,-1,band)
    for channel in range(c):
        for band in range(b):
            for field in (4,5):yield ('FW',field,channel,band)
    yield ('CFG',)
    for index in range(k):yield ('CW',index)
    yield ('RESET',)
    for channel in range(c):
        for band in range(b):
            for field in (0,1,2,3):yield ('FW',field,channel,band)

def clear_fields(c,b,flush=False):
    for channel in range(c):
        for band in range(b):
            for field in ((1,2) if flush else (0,1,2,3)):yield ('FW',field,channel,band)

def phase_program(c,b,step):
    def r(f,ch,band,count=None):return ('FZ' if band<0 or band>=b else 'FR',f,ch,band,count)
    def w(f,ch,band,count=None,owner=None):return ('FW',f,ch,band,count,owner)
    def m(band):yield r(6,-1,band);yield r(7,-1,band)
    def pair(f,ch,low):yield r(f,ch,low);yield r(f,ch,low+1)
    def finish(ch,band):
        yes=yield ('FB',ch,band)
        if yes:yield r(5,ch,band);yield r(5,ch,band)
        yield r(4,ch,band);yield w(2,ch,band,3)
    yield ('PC',)
    for ch in range(c):
        for band in range(b):
            yield ('ROT',ch,band);yield r(2,ch,band,0);yield w(2,ch,band,1)
            yield r(1,ch,band);yield w(1,ch,band,None,'hold')
    yield ('PP',)
    for ch in range(c):
        for band in range(b):yield r(0,ch,band);yield w(3,ch,band)
    for band in range(b):yield w(6,-1,band);yield w(7,-1,band)
    for ch in range(c):
        for band in range(b):
            yield from m(band);yield r(4,ch,band);yield from pair(3,ch,band)
            yield w(4,ch,band);yield r(4,ch,band);yield w(4,ch,band)
            yield from pair(0,ch,band);yield w(5,ch,band);yield from pair(1,ch,band)
            yield r(5,ch,band);yield r(2,ch,band,1);yield r(4,ch,band);yield w(2,ch,band,2)
    for band in range(b):
        for ch in range(c):yield r(4,ch,band)
        winner=yield ('WIN',band)
        yield from m(band)
        if band>0:
            yield from pair(0,winner,band-2);yield r(5,winner,band);yield r(2,winner,band-1,3)
            if band>=step:
                yield from pair(0,winner,band-2*step);yield r(5,winner,band);yield r(2,winner,band-step,3)
        if band<b-1:
            yield from m(band+1);yield from pair(0,winner,band-1);yield r(5,winner,band+1);yield r(2,winner,band+1,2)
            if band<b-step:
                yield from m(band+step);yield from pair(0,winner,band-step);yield r(5,winner,band+step);yield r(2,winner,band+step,2)
        yield from finish(winner,band)
        for ch in range(c):
            if ch!=winner:
                # These two argument reads commute. C++ leaves their relative order unspecified.
                yield ('PAIR',r(5,ch,band),r(5,winner,band))
                yield r(2,winner,band,3);yield from finish(ch,band)
    for ch in range(c):
        for band in range(b):yield r(0,ch,band);yield w(1,ch,band,None,'current')
    yield ('PE',)

class Grammar:
    def __init__(self,program):self.program=program;self.token=next(program,None);self.pair=[]
    def accept(self,tag,values,branch=None):
        token=self.token;need(token is not None,'MusicalBoundaryContextUnsupported: extra grammar event')
        if token[0]=='PAIR':
            if not self.pair:self.pair=list(token[1:])
            key=(tag,*values[:3]);match=None
            for index in (0,1):
                if index<len(self.pair) and self.pair[index][:4]==key:match=index;break
            need(match is not None,'MusicalBoundaryContextUnsupported: argument-read pair')
            matched=self.pair.pop(match)
            if not self.pair:self.token=next(self.program,None)
            return matched
        need(token[0]==tag,'MusicalBoundaryContextUnsupported: ordered phase/boot event')
        if tag in ('FR','FW','FZ'):need(token[1:4]==tuple(values[:3]),'MusicalBoundaryContextUnsupported: field grammar owner/index')
        elif tag in ('ROT','FB'):need(token[1:3]==tuple(values[:2]),'MusicalBoundaryContextUnsupported: band/channel order')
        elif tag in ('WIN','CW'):need(token[1]==values[0],'MusicalBoundaryContextUnsupported: band/window index order')
        self.token=self.program.send(branch) if tag in ('WIN','FB') else next(self.program,None)
        return token

def outputs(call,channels):
    if call['kind']==2:
        for c in range(channels):
            for p in range(call['output']):yield (3,c,p,p)
    else:
        fast=call.get('fast',False)
        for p in range(call['output']):
            for c in range(channels):yield (1 if fast else 0,c,p,p%call['input'] if fast else p)

def verify_case(frozen,actual,budget,generation,controls):
    budget.spend(8192,'bounded_case_metadata_allowance')
    g=json.loads((frozen/'geometry.json').read_text(encoding='utf-8'));c,w,h,k,b=g['channels'],g['W'],g['H'],g['K'],g['bands']
    need(c==1 and g['logical_channels'] in (1,2) and w%h==0 and w//h==4 and k<=6144,'frozen configuration source domain')
    budget.spend(g['analyses']*(w+16)+g['calls']*20+g['syntheses']*20,'oracle_parse_entry_allowance')
    analyses={a['id']:a for a in json.loads((frozen/'analysis-oracle.json').read_text(encoding='utf-8'))}
    syntheses={s['id']:s for s in json.loads((frozen/'synthesis-oracle.json').read_text(encoding='utf-8'))}
    calls=json.loads((frozen/'call-oracle.json').read_text(encoding='utf-8'))
    candidate=json.loads((actual/'candidate-receipt.json').read_text(encoding='utf-8'))
    need(candidate['calls']==g['calls'] and candidate['syntheses']==g['syntheses'] and candidate['analyses']==g['analyses'] and candidate['private_pcm_bit_compares']==c*g['R'],'candidate independent inventory')
    need(candidate['fast_calls']==g['fast_calls'] and candidate['quiet_nonzero']==g['quiet_nonzero'] and candidate['expected_refusal']==g['expected_refusal'],'candidate quiet/refusal inventory')
    need(candidate['production_accepted'] is False and candidate['public_pcm_samples']==0,'publication boundary')
    cap=g['output_capacity'];stride=g['output_stride'];budget.spend(3*cap+2*c,'cell_and_owner_initialization')
    cell_bits=[0]*cap;cell_nodes=[None]*cap;depth=[0]*cap;current=[0]*c;previous=[0]*c
    fields={};field_analysis={};spectra={};windows={};translations={};stats=Counter();writes=0
    boot=Grammar(boot_program(c,b,k));phase=None;phase_outputs=None;outside=None;call=None;output_order=None;quiet_values=[]
    pending_as=None;analysis=None;analysis_i=forward_i=spectrum_i=0;handoff=0;synthesis=0;inverse=None;inverse_bits={};is_i=iv_i=0
    synth_channel=0;clear_cursor=[0]*c;add_cursor=[0]*c;flush_clear=[0]*c;output_per_channel=[0]*c
    head_a=head_s=head_f=quiet_events=0;relations=retained=raw_samples=0;bytes_written=0;source_zero=valued_zero=interior=boundary=0
    case_controls={};residual_min=None;residual_max=None;last_read=[None]*c;actual_rows=0
    rel=(actual/'retained-relations.csv').open('x',encoding='ascii',newline='\n')
    rel.write('read,raw,channel,synthesis,wi,value,current-analysis,x2,y2,class\n')
    operands=(frozen/'operands.bin').open('rb')
    def fail(ok,why):need(ok,'MusicalBoundaryContextUnsupported: '+why)
    def mutant(name,valid,detail):
        need(not valid,'actual record control escaped '+name)
        if name not in controls:
            controls[name]=dict(fixture=g['name'],event=actual_rows,detail=detail)
        case_controls[name]=True
    def slot(channel,coord):
        fail(0<=channel<c,'ring channel');return (coord+channel*stride)&(cap-1)
    def all_owned(field,owners):
        budget.spend(c*b,'owner_band_visits')
        return all(field_analysis.get((field,ch,band))==owners[ch] for ch in range(c) for band in range(b))
    def finish_call():
        if call is None:return
        fail(output_order is not None and next(output_order,None) is None,'complete per-kind output order')
        fail(pending_as is None and phase is None,'call ends within handoff/phase')
        fail(outside is None or outside.token is None,'complete quiet/flush field clear')
        expected_a=0 if call['kind']==2 else 1
        expected_s=int(call['kind']==1 and not call.get('fast',False));expected_f=int(call['kind']==2)
        fail((head_a,head_s,head_f)==(expected_a,expected_s,expected_f),'exact history/STFT/flush advance inventory')
        fail(quiet_events==int(call.get('fast',False)),'fast branch inventory')
        if call.get('fast',False):mutant('false_STFT_advance_on_fast_return',head_s+1==expected_s,'actual fast call with zero STFT advances')
        if call['kind']==2:fail(flush_clear==[w]*c,'complete channel flush clears')
    def emit(raw,ch,e,wi,bits,aid,x2,y2,classification):
        nonlocal relations,bytes_written
        text=f'{retained-1},{raw},{ch},{e},{wi},{bits},{aid},{x2},{y2},{classification}\n'
        fail(len(text)<=96,'relation digit bound');need(relations<g['relation_limit'],'MusicalResourceLimit: relation count')
        need(len(text)<=g['relation_limit']*96-bytes_written,'MusicalResourceLimit: relation payload')
        rel.write(text);bytes_written+=len(text);relations+=1
    def consume_phase(tag,v):
        branch=None
        if tag=='FB':
            channel,band,norm,yes=v;fail(finite(norm) and rational(norm)>=0 and yes==int(rational(norm)<=rational(encode(F_NOISE))),'actual fallback threshold');branch=bool(yes)
        elif tag=='WIN':
            band,selected=v;budget.spend(c,'winning_physical_energy_visits');energy=fields[(4,0,band)][1]
            fail(finite(energy) and rational(energy)>=0,'owned physical C1 winning energy')
            fail(selected==0,'independent only-physical-channel winner');branch=0
        return phase.accept(tag,v,branch)
    # This is an actual candidate receipt from its real Plan/input owners, not a forged Plan.
    budget.spend(8192,'bound_request_metadata_and_generation_control')
    binding_path=actual/'binding.txt';need(binding_path.stat().st_size<=512,'binding text envelope')
    binding=binding_path.read_text(encoding='ascii').split();need(len(binding)==15,'binding field inventory')
    budget.spend(g['logical_channels']*(g['N']+14)+4096,'complete_original_hash_binding')
    original_path=frozen/'asset.bin';need(original_path.stat().st_size==4*g['logical_channels']*(g['N']+14),'original logical geometry bytes')
    original_hash=hashlib.sha256(original_path.read_bytes()).hexdigest().upper();need(original_hash==g['source_hash'],'independent complete C2 source hash')
    observed_binding=list(map(int,binding[:14]));expected_binding=[generation,g['logical_channels'],1,1,1,7,g['N'],int(g['reversed']),1,1,1,0,g['D'],0]
    fail(observed_binding==expected_binding and binding[14]==g['source_hash'],'exact original-width/hash/request binding')
    changed=list(observed_binding);changed[0]+=1
    mutant('cross_fixture_request_generation_substitution',changed==expected_binding,'actual owned-request binding generation changed')
    with (actual/'events.txt').open('r',encoding='ascii') as stream:
        while True:
            budget.spend(20,'observed_row_allowance');line=stream.readline(65)
            if not line:break
            fail(len(line)<=64 and line.endswith('\n'),'event line bound');actual_rows+=1;tokens=line.split();tag=tokens[0];stats[tag]+=1
            fail(len(tokens)<=11,'bounded event operands')
            if tag=='AR':
                op=tokens[1];a0,b0,want,got=map(int,tokens[2:]);value={'add':add,'mul':mul}[op](a0,b0)
                need(value==want==got,'UnsupportedArithmetic: prerequisite replay')
                if a0==b0==0x80000000:mutant('signed_zero_loss',value==0,'actual signed-zero prerequisite')
                continue
            v=list(map(int,tokens[1:]))
            if pending_as is not None:fail(tag=='FW','spectrum assignment must be immediately witnessed')
            if tag=='FP':fail(v[0]&~0x3f==0x1f80 and v[1]==0,'full FP controls')
            elif tag=='GEN':fail(v==[generation,12345],'fixture generation/seed')
            elif tag in ('CFG','CW','RESET'):
                boot.accept(tag,v)
                if tag=='CFG':fail(v==[w,h,k,c],'queried geometry')
                elif tag=='CW':
                    index,bit=v;fail(index==len(windows) and finite(bit),'window index/finite');windows[index]=bit
                    if index>=w:fail(bit==0,'FFT padding window')
                else:
                    budget.spend(3*cap,'cell_reset_entries');cell_bits=[0]*cap;cell_nodes=[None]*cap;depth=[0]*cap
            elif tag=='CALL':
                finish_call();fail(boot.token is None,'complete boot before calls');ordinal=v[0];fail(ordinal==stats['CALL']-1 and ordinal<len(calls),'call order')
                call=calls[ordinal];fail(v==[call[x] for x in ('ordinal','kind','input','output','source','raw','stft','history')],'frozen call/head tuple')
                output_order=outputs(call,c);head_a=head_s=head_f=quiet_events=0;output_per_channel=[0]*c;flush_clear=[0]*c
                if call.get('fast',False) and call['quiet_clear']:outside=Grammar(clear_fields(c,b))
                elif call['kind']==2:outside=Grammar(clear_fields(c,b,True))
                else:outside=None
                quiet_values=[]
                if call.get('fast',False):
                    budget.spend(c*call['input'],'quiet_operand_entries');operands.seek(call['operand_offset'])
                    raw=operands.read(c*call['input']*4);fail(len(raw)==c*call['input']*4,'quiet operand span');quiet_values=[x[0] for x in struct.iter_unpack('<I',raw)]
            elif tag in ('HA','HS','HF'):
                fail(call is not None,'advance without call')
                if tag=='HA':head_a+=1;fail(v==[call['input'],call['history']+call['input']],'history advance')
                elif tag=='HS':head_s+=1;fail(v==[call['output'],call['stft']+call['output']] and not call.get('fast',False),'normal STFT advance')
                else:head_f+=1;fail(v==[call['output'],call['stft']+call['output']] and call['kind']==2,'flush advance')
            elif tag=='QUIET':
                quiet_events+=1;fail(call is not None and call.get('fast',False) and v==[call['input'],call['output']],'exact quiet branch')
            elif tag=='DEC':
                synthesis,ci,off,io,interval,fresh,sought=v;s=syntheses[synthesis]
                fail(call['ordinal']==ci and [ci,off,io,interval,fresh,sought]==[s[x] for x in ('call','offset','input_offset','input_interval','fresh','sought')],'callback geometry')
            elif tag=='AB':
                aid,e,ci,prev,ch,start,width=v;analysis=analyses[aid]
                fail(aid==stats['AB'] and ci==call['ordinal'] and e==synthesis,'consecutive analysis/call/synthesis identity')
                fail(v==[analysis[x] for x in ('id','synthesis','call','previous','channel','start')]+[w],'channel/role analysis identity')
                fail(g['source_min']<=start and start+w<=g['source_end'],'analysis source extent');analysis_i=forward_i=spectrum_i=0
            elif tag=='AE':
                fail(analysis is not None and v==[analysis['id']] and (analysis_i,forward_i,spectrum_i)==(k,k,b),'complete analysis')
                (previous if analysis['previous'] else current)[analysis['channel']]=analysis['id']
            elif tag=='FBEGIN':fail(analysis is not None and v==[analysis['id'],k,g['rho']],'FFT owner/rotation')
            elif tag=='AI':
                aid,index,bit=v;fail(analysis is not None and aid==analysis['id'] and index==analysis_i and index<k,'FFT input identity/index')
                fail(bit==(analysis['bits'][index] if index<w else 0) and finite(bit),'selected/zero/K-pad FFT operand')
                mutant('wrong_FFT_operand_index',index+1==analysis_i,'actual FFT operand index');analysis_i+=1
            elif tag=='FV':
                aid,index,q,norm,window,result,negative=v;fail(analysis is not None and aid==analysis['id'] and index==forward_i and q==rotate(index,k,g['rho']) and negative==int(index<g['rho']),'forward transform identity/sign')
                operand=analysis['bits'][index] if index<w else 0
                fail(norm==encode(1) and window==windows[index] and finite(result) and mul(mul(operand^(0x80000000 if negative else 0),norm),window)==result,'forward window arithmetic');forward_i+=1
            elif tag=='AF':
                aid,band,real,imag=v;fail(analysis is not None and aid==analysis['id'] and band==spectrum_i and band<b and finite(real) and finite(imag),'FFT spectrum owner')
                spectra[analysis['previous'],analysis['channel'],band]=(aid,real,imag);spectrum_i+=1
            elif tag=='FEND':fail(analysis is not None and v==[analysis['id']] and spectrum_i==b,'complete FFT spectrum')
            elif tag=='AS':
                aid,prev,ch,band,real,imag=v;fail(aid in analyses and prev==analyses[aid]['previous'] and ch==analyses[aid]['channel'] and spectra.get((prev,ch,band))==(aid,real,imag),'completed channel/role spectrum handoff')
                pending_as=(1 if prev else 0,ch,band,aid,real,imag)
            elif tag in ('FR','FW','FZ'):
                if tag=='FZ':
                    f,ch,band,zero=v;fail(0<=f<4 and 0<=ch<c and (band<0 or band>=b) and zero==0 and phase is not None,'indexed zero owner');consume_phase(tag,v)
                    continue
                f,ch,band,version,real,imag=v;key=(f,ch,band)
                fail(0<=f<8 and 0<=band<b and ((f<6 and 0<=ch<c) or (f>=6 and ch==-1)) and finite(real) and finite(imag),'field domain/finite')
                if tag=='FW':
                    writes+=1;fail(version==writes and version<=9999999,'nonwrapping latest field version')
                    owner=None
                    if call is None:boot.accept(tag,v);fail(real==imag==0,'configured/reset field birth')
                    elif pending_as is not None:
                        fail(key==pending_as[:3] and (real,imag)==pending_as[4:],'analysis-to-field value preservation');owner=pending_as[3];pending_as=None
                    elif phase is not None:
                        token=consume_phase(tag,v)
                        if f==2:
                            at=ch*b+band;phase_outputs[at]+=1;fail(phase_outputs[at]==token[4],'rotation/preliminary/final output version')
                        if f==1 and token[5]=='current':owner=field_analysis.get((0,ch,band))
                    else:
                        fail(outside is not None,'unexpected outside-phase write');outside.accept(tag,v);fail(real==imag==0,'quiet/flush partial clear bits')
                        if f in (0,1):owner=0
                    fields[key]=(version,real,imag)
                    if f in (0,1) and owner is not None:field_analysis[key]=owner
                    elif call is None and f in (0,1):field_analysis[key]=0
                else:
                    valid=fields.get(key)==(version,real,imag) and version>0;fail(valid,'stale/uninitialized/wrong-channel field version')
                    mutant('wrong_phase_version',fields.get(key)==(version+1,real,imag),'actual latest field version changed')
                    if phase is not None:
                        token=consume_phase(tag,v)
                        if f==2:fail(phase_outputs[ch*b+band]==token[4],'ordered output read stage')
                    else:
                        fail(f==2 and ch==handoff//b and band==handoff%b and handoff<c*b,'complete ordered spectrum output handoff');handoff+=1
            elif tag=='PB':
                sid,fresh,factor=v;s=syntheses[sid];fail(phase is None and sid==synthesis and fresh==s['fresh'] and factor==s['time_factor_bits'],'phase generation/factor')
                fail(pending_as is None and all_owned(1,s['previous']),'held previous-analysis owner')
                budget.spend(c*b,'phase_output_initialization');phase_outputs=[0]*(c*b);phase=Grammar(phase_program(c,b,(2*k+h)//(2*h)));handoff=0
            elif tag in ('PC','PP','ROT','FB','WIN','PE'):
                fail(phase is not None,'phase event outside phase');consume_phase(tag,v)
                if tag=='PC':fail(v==[synthesis,encode(2),0],'clean factor/RNG branch')
                elif tag=='PP':fail(v==[encode(1),0],'identity pitch map policy')
                elif tag=='ROT':fail(all(finite(bit) for bit in v[2:]),'finite rotation operands')
                elif tag=='PE':
                    budget.spend(c*b,'phase_completion_visits');fail(v==[synthesis] and phase.token is None and all(x==3 for x in phase_outputs),'complete per-channel output phase');phase=None
            elif tag=='DR':fail(False,'unreached RNG branch')
            elif tag=='SY':
                sid,off,start=v;s=syntheses[sid];fail(sid==synthesis and off==s['offset'] and start==s['start'] and phase is None and handoff==c*b,'synthesis interval/handoff')
                fail(current==s['current'] and all_owned(0,s['current']),'held current-analysis owners');clear_cursor=[0]*c;add_cursor=[0]*c;synth_channel=0
            elif tag=='CL':
                ch,x,prior,now=v;at=slot(ch,x)
                if call['kind']==2:
                    expected_x=call['stft']+flush_clear[ch]
                    fail(output_per_channel[ch]==w and x==expected_x and flush_clear[ch]<w,'flush read/clear channel coordinate');flush_clear[ch]+=1
                else:
                    expected_x=syntheses[synthesis]['start']+w+clear_cursor[ch]
                    fail(ch==synth_channel and x==expected_x and clear_cursor[ch]<h,'future channel clear coordinate');clear_cursor[ch]+=1
                fail(prior==cell_bits[at] and now==0,'clear prior/result');mutant('wrong_clear_coordinate',x+1==expected_x,'actual clear coordinate shifted')
                budget.spend(depth[at],'retired_chain_node_visits');cell_bits[at]=0;cell_nodes[at]=None;depth[at]=0
            elif tag=='IB':
                sid,ch,K,rho=v;fail(sid==synthesis and ch==synth_channel and K==k and rho==g['rho'] and clear_cursor[ch]==h,'IFFT channel/config owner')
                inverse=(sid,ch);is_i=iv_i=0;inverse_bits={}
            elif tag=='IS':
                sid,ch,band,real,imag=v;fail(inverse==(sid,ch) and band==is_i and fields.get((2,ch,band),(0,None,None))[1:]==(real,imag),'IFFT field version/value input');is_i+=1
            elif tag=='IV':
                sid,ch,index,q,raw,norm,window,result,negative=v;fail(inverse==(sid,ch) and index==iv_i and q==rotate(index,k,g['rho']) and negative==int(index<g['rho']),'IFFT channel/index/sign')
                fail(is_i==b and norm==div(encode(1),encode(k)) and window==windows[index] and finite(raw) and finite(result),'IFFT norm/window/finite')
                fail(mul(mul(raw^(0x80000000 if negative else 0),norm),window)==result,'independent inverse window arithmetic');inverse_bits[index]=result;iv_i+=1
            elif tag=='IE':fail(inverse==tuple(v) and is_i==b and iv_i==k,'complete IFFT channel inventory')
            elif tag=='AD':
                sid,ch,x,wi,prior,operand,result=v;s=syntheses[sid];fail(sid==synthesis and ch==synth_channel and inverse==(sid,ch) and wi==add_cursor[ch] and wi<w and x==s['start']+wi and clear_cursor[ch]==h,'channel add identity/order')
                at=slot(ch,x);valid=cell_bits[at]==prior and inverse_bits.get(wi)==operand and finite(prior) and finite(operand) and finite(result) and add(prior,operand)==result
                fail(valid,'signed binary32 overlap add');mutant('wrong_overlap_result',add(prior,operand)==(result^1),'actual overlap result bit changed')
                need(depth[at]<5,'MusicalResourceLimit: source-derived chain depth');cell_nodes[at]=(sid,ch,wi,operand,cell_nodes[at]);cell_bits[at]=result;depth[at]+=1;add_cursor[ch]+=1
                if add_cursor[ch]==w:synth_channel+=1
            elif tag=='RO':
                kind,ch,raw,coord,index,value=v;expected=next(output_order,None);fail(expected is not None,'extra output')
                ek,ec,p,ei=expected;fail((kind,ch,index)==(ek,ec,ei) and raw==call['raw']+p and coord==call['stft']+index and finite(value),'every ordered raw/channel extraction')
                raw_samples+=1;output_per_channel[ch]+=1;mutant('one_frame_raw_read_shift',raw+1==call['raw']+p,'actual raw read shifted')
                if kind in (0,3):
                    fail(last_read[ch] is None or coord>last_read[ch],'monotone physical STFT read coordinate');last_read[ch]=coord
                    at=slot(ch,coord);fail(value==cell_bits[at],'actual ring read sum')
                else:
                    fail(kind==1 and call.get('fast',False),'exact source-copy quiet branch')
                    fail(value==quiet_values[ch*call['input']+index],'exact quiet source operand')
                    if value&0x7fffffff:
                        fail(quiet_refusal_ok(True,g['expected_refusal']),'nonzero quiet refusal')
                if g['retained_begin']<=raw<g['retained_end']:
                    retained+=1
                    if kind==1:
                        budget.spend(20,'retained_relation_allowance');source=call['source']+index;classification='quiet-refused' if value&0x7fffffff else 'source-zero'
                        source_zero+=int(classification=='source-zero');emit(raw,ch,0,index,value,0,2*source,2*(raw-g['OL']),classification)
                        if classification=='source-zero':mutant('dropped_valued_zero_relation',coverage_ok(1,0),'actual complete one-relation source-zero read omitted')
                        else:mutant('suppressed_nonzero_quiet_refusal',quiet_refusal_ok(True,'none'),'actual retained nonzero fast source copy without named refusal')
                    else:
                        node=cell_nodes[at];expected_count=depth[at];emitted=0;zero_in_read=False;second_in_read=False
                        while node is not None:
                            budget.spend(20,'retained_relation_allowance');sid,owner_ch,wi,bits,parent=node;s=syntheses[sid];aid=s['current'][ch];a=analyses[aid]
                            fail(owner_ch==ch and a['channel']==ch and s['start']<=s['start']+wi<s['start']+w,'retained channel/synthesis owner')
                            fail(g['source_min']<=a['start'] and a['start']+w<=g['source_end'],'retained source/context bounds')
                            delta=raw-(s['start']+wi);fail(delta==call['raw']-call['stft'],'every synthesis-window translation')
                            key=(sid,ch);seen=translations.setdefault(key,set());second=bool(seen and delta not in seen);seen.add(delta)
                            fail(len(seen)<=g['fast_calls']+1,'translation inventory bound');x2=a['center2'];y2=2*(raw-wi-g['OL'])+w
                            if bits&0x7fffffff:
                                if 0<=x2<=2*g['N']:
                                    q=x2;residual=y2-2*q;need(center_ok(y2,q),'CenterScheduleMismatch: actual contributing interior current center')
                                    residual_min=residual if residual_min is None else min(residual_min,residual);residual_max=residual if residual_max is None else max(residual_max,residual)
                                    mutant('three_frame_center_shift',center_ok(y2+(6 if residual>=0 else -6),q),'actual interior relation translated three Frames');interior+=1;classification='interior'
                                else:boundary+=1;classification='declared-boundary'
                            else:valued_zero+=1;classification='valued-zero';zero_in_read=True
                            emitted+=1;emit(raw,ch,sid,wi,bits,aid,x2,y2,classification)
                            second_in_read=second_in_read or second
                            node=parent
                        fail(coverage_ok(expected_count,emitted),'all linked add relations including zero/cancellation')
                        if zero_in_read:mutant('dropped_valued_zero_relation',coverage_ok(expected_count,emitted-1),'one actual zero relation omitted from a complete read')
                        if second_in_read:mutant('dropped_second_synthesis_translation',coverage_ok(expected_count,emitted-1),'one actual second-translation relation omitted from a complete read')
            else:fail(False,'unknown event '+tag)
    finish_call();operands.close();rel.close()
    fail(boot.token is None and stats['FP']==1 and stats['GEN']==1 and stats['AR']==5 and stats['CFG']==1 and stats['RESET']==1 and len(windows)==k,'boot/arithmetic/config inventory')
    fail(raw_samples==c*g['R'] and retained==c*g['D'] and stats['CALL']==g['calls'] and stats['DEC']==stats['PB']==stats['PE']==stats['SY']==g['syntheses'],'whole raw/retained/callback inventory')
    fail(stats['AB']==stats['AE']==stats['FBEGIN']==stats['FEND']==g['analyses'] and stats['AI']==stats['FV']==g['analyses']*k and stats['AF']==stats['AS']==g['analyses']*b,'whole analysis/spectrum inventory')
    fail(stats['IB']==stats['IE']==g['syntheses']*c and stats['IS']==g['syntheses']*c*b and stats['IV']==g['syntheses']*c*k and stats['AD']==g['syntheses']*c*w and stats['CL']==g['syntheses']*c*h+c*w,'whole channel synthesis/clear inventory')
    fail(stats['WIN']==g['syntheses']*b and stats['ROT']==stats['FB']==g['syntheses']*c*b and stats['QUIET']==g['fast_calls'],'phase/channel/quiet coverage')
    budget.spend(len(translations),'translation_summary_visits')
    translated=sum(len(v)>1 for v in translations.values())
    if g['name']=='small-equivalent-gaps':fail(source_zero>0 and valued_zero>0 and translated>0,'reached zero and second synthesis translation coverage')
    with (frozen/'expected-base.txt').open('r',encoding='ascii') as expected,(actual/'base-events.txt').open('r',encoding='ascii') as observed:
        while True:
            budget.spend(4,'base_stream_row_allowance');left=expected.readline(65);right=observed.readline(65)
            if not left:fail(not right,'extra base records');break
            fail(len(left)<=64 and len(right)<=64 and left.endswith('\n') and right.endswith('\n'),'base stream line bound')
            if left.rstrip().endswith(' *'):
                prefix=left.rstrip()[:-1];fail(left.startswith(('Y 0 ','Y 3 ')) and right.startswith(prefix) and right[len(prefix):].strip().isdecimal(),'base wildcard scope')
            else:fail(left==right,'independent source/energy/state/base event')
    budget.spend(3*cap+len(fields)+len(spectra)+len(analyses)*w,'bounded_case_retirement_allowance')
    result=dict(status='exact_request_bound_C1_fixture_context_checked',fixture=g['name'],calls=g['calls'],analyses=g['analyses'],syntheses=g['syntheses'],private_pcm_bit_compares=c*g['R'],retained_channel_samples=retained,
        retained_relations=relations,source_zero_relations=source_zero,valued_zero_relations=valued_zero,interior_relations=interior,boundary_relations=boundary,interior_signed_residual_half_frames=[residual_min,residual_max],
        distinct_translation_synthesis_channels=translated,expected_refusal=g['expected_refusal'],reached_record_controls=sorted(case_controls),
        context_certified=g['expected_refusal']=='none',waveform_run=False,resource_certificate=False,production_accepted=False,public_pcm_samples=0)
    with (actual/'verification-receipt.json').open('x',encoding='utf-8',newline='\n') as receipt:receipt.write(json.dumps(result,indent=2)+'\n')
    return result

# Exact 1e-15 is converted by the independent binary32 encoder, as in the pinned source.
from fractions import Fraction
F_NOISE=Fraction(1,10**15)

def verify_physical_controls(frozen,actual,budget):
    budget.spend(131072,'complete_physical_control_replay_allowance')
    path=actual/'physical-controls.txt';need(path.stat().st_size<=1024,'physical control metadata envelope')
    rows=[x.split() for x in path.read_text(encoding='ascii').splitlines()]
    need(len(rows)==3 and [r[0] for r in rows]==['REP','LAYOUT','C2'],'physical control inventory')
    rep=list(map(int,rows[0][1:]));need(len(rep)==4 and 0<=rep[0]<1024 and rep[3]==0,'representative control shape')
    asset=frozen/'small-equivalent-zero/asset.bin';need(asset.stat().st_size==4*2*1038,'zero original control bytes')
    with asset.open('rb') as stream:stream.seek(4*((7+rep[0])*2));original=stream.read(8)
    left,right=struct.unpack('<II',original)
    need([rep[1],rep[2]]==[left,right] and left!=right and (left&0x7fffffff)==(right&0x7fffffff)==0,'actual wrong logical1 supplied bits versus frozen representative0 gate')
    layout=list(map(int,rows[1][1:]));need(layout==[1,2,0],'actual queried C1 versus actual distinct C2 descriptor control')
    control=frozen/'old-C2-equivalent-control';original=(control/'asset.bin').read_bytes();changed=(control/'distinct-asset.bin').read_bytes()
    need(len(original)==len(changed)==4*2*1038,'distinct physical control original sizes')
    index=((7+1024-1)*2+1)*4;mutated=bytearray(original);value=struct.unpack_from('<I',mutated,index)[0];struct.pack_into('<I',mutated,index,value^1)
    need(bytes(mutated)==changed,'independently frozen last-frame one-ULP distinct physical control')
    for stem,data in [('asset',original),('distinct-asset',changed)]:
        meta=(control/(stem+'.txt')).read_text(encoding='ascii').split();need(len(meta)==6 and hashlib.sha256(data).hexdigest().upper()==meta[-1],'complete physical-control source hash')
    pcm=actual/'old-C2-output.bin';need(pcm.stat().st_size==4*4608,'complete actual old-C2 private output bytes')
    count=raw_bad=retained_bad=0
    with pcm.open('rb') as stream:
        for raw in range(2304):
            data=stream.read(8);need(len(data)==8,'C2 actual private PCM span');a,b=struct.unpack('<II',data)
            need(finite(a) and finite(b),'C2 physical control nonfinite sample')
            equal=a==b or ((a&0x7fffffff)==0 and (b&0x7fffffff)==0)
            raw_bad+=int(not equal);retained_bad+=int(not equal and 128<=raw<2176);count+=2
        need(not stream.read(1),'C2 extra actual private PCM')
    c2=list(map(int,rows[2][1:]));need(c2==[18,4608,2304,raw_bad,retained_bad,0] and raw_bad>0 and retained_bad>0,'actual old-C2 identical-stereo own gate failed without repair')
    return dict(wrong_representative=dict(local=rep[0],expected_original0_bits=left,actual_original1_bits=right,own_gate_failed=True,DSP_invoked=False),
        wrong_actual_configuration=dict(queried=1,descriptor_physical=2,own_gate_failed=True,process_calls=0),
        old_C2_equivalent_path=dict(calls=18,private_samples=count,pairs=2304,raw_numeric_differences=raw_bad,retained_numeric_differences=retained_bad,own_gate_failed=True,post_hoc_repair=False),
        proof_scope='physical input/configuration and one real DSP counterexample; no other waveform mutation credit')

def verify(frozen,actual):
    budget=Budget(stage_limit=228000000);budget.spend(1048576,'bounded_matrix_metadata_allowance')
    freeze=json.loads((frozen/'freeze-receipt.json').read_text(encoding='utf-8'));candidate=json.loads((actual/'candidate-receipt.json').read_text(encoding='utf-8'))
    preparation=json.loads((frozen.parent/'observer/preparation-receipt.json').read_text(encoding='utf-8'))
    prior=freeze['freeze_work']+preparation['prepare_work']+candidate['candidate_work']+2097152;need(candidate['prepared_plus_candidate_work']==prior,'combined prior work receipt')
    need(prior<=WORK-budget.used,'MusicalResourceLimit: combined prior work');budget.prior=prior;budget.used+=prior;budget.limit=min(WORK,prior+228000000);controls={};results=[]
    program=json.loads((frozen/'phase-program.json').read_text(encoding='utf-8'))
    need(program['version']=='physical-c1-context-r3-identity-ordered-phase' and program['channels']==1 and program['factor_bits']==encode(2) and program['random_draws']==0,'frozen phase program identity')
    for index,g in enumerate(freeze['fixtures']):
        budget.spend(128,'matrix_fixture_allowance');results.append(verify_case(frozen/g['name'],actual/g['name'],budget,index+1,controls))
    required={'signed_zero_loss','wrong_FFT_operand_index','wrong_phase_version','cross_fixture_request_generation_substitution','wrong_clear_coordinate','wrong_overlap_result','one_frame_raw_read_shift','three_frame_center_shift','false_STFT_advance_on_fast_return','dropped_valued_zero_relation','dropped_second_synthesis_translation','suppressed_nonzero_quiet_refusal'}
    need(set(controls)==required,'UnreachedControl: required actual record predicate')
    physical=verify_physical_controls(frozen,actual,budget)
    budget.spend(1024,'terminal_inventory_allowance');total=0
    for path in actual.rglob('*'):
        budget.spend(kind='artifact_entry_visits')
        if path.is_file():total+=path.stat().st_size
    need(total<=BATCH,'MusicalResourceLimit: matrix retained payload')
    need(budget.used-budget.prior<=228000000 and budget.used<=260000000<=WORK,'MusicalResourceLimit: frozen combined source bound')
    result=dict(status='six_request_bound_C1_context_fixtures_checked_quiet_refusal_preserved',fixtures=results,actual_record_controls=controls,physical_controls=physical,main_calls=132,extra_control_calls=18,parent_reserved_work=260000000,runner_reserved_work_both_invocations=2097152,freeze_work=freeze['freeze_work'],prepare_work=preparation['prepare_work'],candidate_work=candidate['candidate_work'],independent_work=budget.used-budget.prior,combined_work=budget.used,work_accounting=dict(budget.categories),
        proof_domain='six frozen current-request-bound physical-C1 fixtures and reached state paths; nonzero quiet refused',universal_context_certified=False,waveform_run=False,resource_certificate=False,production_accepted=False,public_pcm_samples=0)
    with (actual/'verification-receipt.json').open('x',encoding='utf-8',newline='\n') as receipt:receipt.write(json.dumps(result,indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--frozen',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True);a=p.parse_args();verify(a.frozen.resolve(),a.candidate.resolve())
