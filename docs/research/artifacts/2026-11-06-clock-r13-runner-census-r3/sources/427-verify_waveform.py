"""Independent replay. Never imports the generator or calls vendor/production code."""
import argparse
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import struct

SOURCE_REPLAY_REVIEW_ACCEPTED = False
ALL_REPLAY_HASH_HELPER_COSTS_ACCEPTED = False
STAGE_RESERVATION_ACCEPTED = False
SCALE = 1 << 56
LIMIT149 = 1 << 129


def require(value, why):
    if not value:
        raise RuntimeError(why)


def ceiling(n, d):
    require(d > 0, 'REPLAY_DIVISOR')
    return -((-n)//d)


def bounds_ratio(n, d):
    return n*SCALE//d, ceiling(n*SCALE, d)


def plus(a, b):
    lo,hi=a[0]+b[0],a[1]+b[1]
    require(lo<=hi and max(abs(lo),abs(hi))<=2*SCALE,'REPLAY_SUM_WIDTH')
    return lo,hi


def negative(a):
    return -a[1], -a[0]


def product(a, b):
    require(max(map(abs,a)) <= 2*SCALE and max(map(abs,b)) <= 2*SCALE, 'REPLAY_PRODUCT_PREFLIGHT')
    values = [a[0]*b[0],a[0]*b[1],a[1]*b[0],a[1]*b[1]]
    require(max(map(abs,values)) <= 1 << 114, 'REPLAY_PRODUCT_WIDTH')
    return min(values)//SCALE, ceiling(max(values),SCALE)


def quotient(a, d):
    require(d > 0, 'REPLAY_QUOTIENT_DOMAIN')
    return a[0]//d, ceiling(a[1],d)


def pi_bracket():
    def exact_atan(d, count):
        total = sum((Fraction(1 if k % 2 == 0 else -1,(2*k+1)*d**(2*k+1)) for k in range(count)),Fraction(0))
        require(total.numerator.bit_length()<=4096 and total.denominator.bit_length()<=4096,'REPLAY_MACHIN_RATIONAL_WIDTH')
        rest = Fraction(1,(2*count+1)*d**(2*count+1))
        return (total,total+rest) if count % 2 == 0 else (total-rest,total)
    a,b = exact_atan(5,24),exact_atan(239,8)
    lower,upper = 16*a[0]-4*b[1],16*a[1]-4*b[0]
    require(Fraction(3)<lower<=upper<Fraction(4), 'INDEPENDENT_MACHIN')
    return lower.numerator*SCALE//lower.denominator,ceiling(upper.numerator*SCALE,upper.denominator)


def reference_interval(rate, frequency, p, q, n, pi):
    require(rate in (44100,48000,96000) and frequency in (220,997) and p in (0,1) and q in (1,7) and 0<=n<rate,'REPLAY_TONE_DOMAIN')
    a,b = 2*frequency*n*q+p*rate,rate*q
    require(a<1<<63 and b<1<<32,'REPLAY_PHASE_WIDTH')
    k,r = divmod(2*a,b)
    if 2*r >= b:
        k += 1
    h = 2*a-k*b
    require(abs(h)*2 <= b, 'REPLAY_EXACT_PHASE')
    values = [pi[0]*h,pi[1]*h]
    theta = min(values)//(2*b),ceiling(max(values),2*b)
    square = product(theta,theta)
    st,ct = theta,(SCALE,SCALE)
    sine,cosine = st,ct
    for j in range(1,13):
        st = quotient(negative(product(st,square)),(2*j)*(2*j+1))
        ct = quotient(negative(product(ct,square)),(2*j-1)*(2*j))
        sine,cosine = plus(sine,st),plus(cosine,ct)
    require(math.factorial(26)>1<<80, 'REPLAY_TAYLOR_REMAINDER')
    sine,cosine = (sine[0]-1,sine[1]+1),(cosine[0]-1,cosine[1]+1)
    return quotient((sine,cosine,negative(sine),negative(cosine))[k%4],5)


def float149(word):
    e = (word>>23)&255
    require(e != 255,'REPLAY_NONFINITE_WORD')
    m = word&0x7fffff
    value = m if e == 0 else ((1<<23)+m) << (e-1)
    return -value if word>>31 else value


def error149(word, bounds, scale_bits):
    exact = float149(word)
    e = max(abs(exact-(bounds[0] << (149-scale_bits))),abs(exact-(bounds[1] << (149-scale_bits))))
    require(e <= LIMIT149,'REPLAY_SOURCE_ERROR_2POW_MINUS20')
    return e


def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(65536),b''):
            value.update(block)
    return value.hexdigest()


def read_limited(path, maximum):
    require(path.is_file() and path.stat().st_size <= maximum,'REPLAY_FILE_CAP')
    return path.read_bytes()


def replay_source(root):
    require(SOURCE_REPLAY_REVIEW_ACCEPTED and ALL_REPLAY_HASH_HELPER_COSTS_ACCEPTED and STAGE_RESERVATION_ACCEPTED,'SOURCE_HOLD_INDEPENDENT_REPLAY_ALL_HELPERS_AND_STORAGE')
    pi = pi_bracket();total = 0;maximum = 0;work = 128000000;control_reference = None
    for rate in [44100,48000,96000]:
        for f in [220,997]:
            for pp,qq,label in [(0,1,'p0'),(1,7,'p1q7')]:
                key=f'r{rate}-f{f}-{label}';period=rate//math.gcd(f,rate)
                cert=json.loads(read_limited(root/'references/source-certificates'/str(key+'.json'),65536))
                data=read_limited(root/'references/tone'/str(key+'.bin'),4*10*rate)
                table=read_limited(root/'references/phase-intervals'/str(key+'.bin'),32*period)
                require(len(data)==4*10*rate and len(table)==32*period,'REPLAY_RECORD_COUNT')
                require(sha(root/'references/tone'/str(key+'.bin'))==cert['tone_sha256'] and sha(root/'references/phase-intervals'/str(key+'.bin'))==cert['phase_sha256'],'REPLAY_PINNED_STREAMS')
                refined=bytearray(16*period)
                for n in range(period):
                    work+=8192;require(work<=1<<34,'REPLAY_WORK_CAP')
                    lo,hi,index,profile,reserved=struct.unpack_from('<qqQII',table,32*n)
                    require(index==n and profile==cert['profile_id'] and reserved==0,'REPLAY_PHASE_KEY')
                    a,b=reference_interval(rate,f,pp,qq,n,pi)
                    require((lo<<8)<=a<=b<=(hi<<8),'FROZEN_INTERVAL_DOES_NOT_CONTAIN_INDEPENDENT_ENCLOSURE')
                    struct.pack_into('<qq',refined,16*n,a,b)
                localmax=-1
                for n in range(10*rate):
                    work+=256;require(work<=1<<34,'REPLAY_WORK_CAP')
                    word=struct.unpack_from('<I',data,4*n)[0];lo,hi,*_=struct.unpack_from('<qqQII',table,32*(n%period));a,b=struct.unpack_from('<qq',refined,16*(n%period))
                    localmax=max(localmax,error149(word,(lo,hi),48));error149(word,(a,b),56);total+=1
                if rate==48000 and f==220 and pp==1:
                    control_reference = bytes(refined)
                require(localmax==cert['maximum_numerator149'],'REPLAY_EXACT_MAXIMUM')
                maximum=max(maximum,localmax)
    require(total==7524000,'REPLAY_ALL_TONE_WORDS')
    for rate in [44100,48000,96000]:
        zero=read_limited(root/f'references/zero/r{rate}.bin',4*10*rate)
        signal=read_limited(root/f'references/burst/r{rate}.bin',4*10*rate)
        require(len(zero)==len(signal)==4*10*rate,'DYADIC_SOURCE_SIZE')
        radius=rate//2000;positive=[]
        for k in range(1,radius+1):
            value=(k*0x9e3779b9)&0xffffffff
            value=((value^(value>>16))*0x85ebca6b)&0xffffffff
            value=((value^(value>>13))*0xc2b2ae35)&0xffffffff
            value^=value>>16;positive.append(1 if value&1 else-1)
        integers=list(reversed(positive))+[-2*sum(positive)]+positive
        require(integers==list(reversed(integers)) and sum(integers)==0,'DYADIC_SYMMETRY_AND_ZERO_DC')
        m=0
        while 4*max(map(abs,integers))>1<<m:
            m+=1
        def exact_word(value):
            if value==0:return 0
            sign=0x80000000 if value<0 else 0;n=abs(value);top=n.bit_length()-1
            require(top<=23 and -126<=top-m<=127,'DYADIC_NORMAL_REPRESENTATION')
            return sign|((top-m+127)<<23)|((n<<(23-top))-(1<<23))
        expected={}
        for point in [2*rate,4*rate,6*rate,8*rate]:
            weighted=sum((point-radius+j)*value*value for j,value in enumerate(integers))
            require(weighted==point*sum(v*v for v in integers),'EXACT_SOURCE_CENTROID')
            for j,value in enumerate(integers):expected[point-radius+j]=exact_word(value)
        for n in range(10*rate):
            work+=256;require(work<=1<<34,'REPLAY_WORK_CAP')
            require(struct.unpack_from('<I',zero,4*n)[0]==0,'EXACT_ZERO_SOURCE_WORD')
            require(struct.unpack_from('<I',signal,4*n)[0]==expected.get(n,0),'EXACT_DYADIC_BURST_WORD')
    for name in ['r48000-equivalent-last-ulp','r48000-early-distinct-late-nan','r48000-mixed-signed-zero']:
        path=root/'references/control-sources'/str(name+'.interleaved.bin');data=read_limited(path,3840000);require(len(data)==3840000,'CONTROL_SOURCE_COUNT');nonfinite=0
        for n in range(480000):
            work+=512;require(work<=1<<34,'REPLAY_WORK_CAP')
            a,b=struct.unpack_from('<II',data,8*n)
            if name.endswith('mixed-signed-zero'):
                require((a&0x7fffffff)==0 and (b&0x7fffffff)==0 and a!=b,'MIXED_SIGNED_ZERO_REPLAY')
            else:
                require(control_reference is not None,'CONTROL_ANALYTIC_CACHE')
                reference=struct.unpack_from('<qq',control_reference,16*(n%2400))
                for word in [a,b]:
                    if ((word>>23)&255)==255:
                        nonfinite+=1;require(name.endswith('late-nan') and n==479999 and word==0x7fc00035,'EXPECTED_NONFINITE_CONTROL_ONLY')
                    else:
                        error149(word,reference,56)
        require(nonfinite==int(name.endswith('late-nan')),'CONTROL_NONFINITE_INVENTORY')
    return {'status':'SOURCE_INTERVAL_REPLAY_COMPLETE_NOT_VENDOR','base_words':total,'maximum_numerator149':maximum,'work':work,'production_admission':False}


def replay_copies(root, repo):
    manifest=json.loads(read_limited(root/'generated/private-sources/source-copy-manifest.json',262144))
    def apply(data, edits):
        for edit in edits:
            a,b=edit['old'].encode(),edit['new'].encode();require(data.count(a)==1,'REPLAY_SOURCE_ANCHOR');data=data.replace(a,b,1)
        return data
    for row in manifest['lineage']:
        name='MusicalAudioChannelPlan.h' if row['kind']=='plan' else 'MusicalAudioChannelMaterialization.h';prefix=root/'generated/private-sources';final=read_limited(prefix/row['variant']/'engine'/name,65536)
        require(hashlib.sha256(final).hexdigest()==row['final_sha256'],'PRIVATE_SOURCE_FINAL_HASH')
        fault=prefix/'patches'/str(row['variant']+'.'+row['kind']+'.fault.inverse.patch')
        isolated=apply(final,json.loads(read_limited(fault,65536))) if fault.is_file() else final
        require(hashlib.sha256(isolated).hexdigest()==row['isolated_sha256'],'PRIVATE_FAULT_INVERSE')
        inverse=json.loads(read_limited(prefix/'patches'/str(row['variant']+'.'+row['kind']+'.isolation.inverse.patch'),65536))
        original=apply(isolated,inverse);require(original==(repo/'src/engine'/name).read_bytes(),'PRIVATE_ISOLATION_INVERSE_EXACT')
    require(len(manifest['lineage'])==10,'PRIVATE_COPY_INVENTORY')


def center_read_gate(records, A, D, Qx, wi, W, delta):
    require(delta in (-3,3),'CENTER_FROZEN_SIGN')
    baseline,mutant,changed=None,None,False
    retained=[]
    for r in records:
        p,j,physical,word,baseword=r['p'],r['j'],r['physical'],r['word'],r['baseline_word']
        require(j==p-(A+delta),'ACTUAL_PROJECT_READ_INDEX')
        if not r['outside']:
            require(physical==r['OL']+j and r['owner_matched'],'ACTUAL_RETAINED_OWNER')
            retained.append(p)
            y2=2*(p-A-wi)+W
            if r['witness']:
                mutant=y2-2*Qx;baseline=mutant-2*delta
        changed=changed or word!=baseword
    require(baseline is not None and mutant is not None,'CENTER_WITNESS_REACHED')
    return {'sample_locations_changed':changed,'center_rejected':abs(mutant)>4,'signed_composition_exact':mutant==baseline+2*delta,'sign_matches':delta==(3 if baseline>=0 else-3),'other_placement_rejected':bool(retained) and (min(retained)!=A or max(retained)!=A+D-1)}


def centroid(words, expected, W, rate):
    lo,hi=expected-2*W,expected+2*W
    require(0<=lo<=hi<len(words),'BURST_WHOLE_REGION')
    energy=math.fsum(float(words[j])**2 for j in range(lo,hi+1))
    require(math.isfinite(energy) and energy>0,'BURST_POSITIVE_ENERGY')
    mu=math.fsum(j*float(words[j])**2 for j in range(lo,hi+1))/energy
    return abs(mu-expected)/rate<=0.005


if __name__=='__main__':
    require(False,'SOURCE_HOLD_NO_REPLAY_OR_VERIFICATION_EXECUTION_AUTHORITY')
    parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,required=True);parser.add_argument('--repo',type=Path,required=True)
    args=parser.parse_args();replay_copies(args.root,args.repo);print(json.dumps(replay_source(args.root),sort_keys=True))
