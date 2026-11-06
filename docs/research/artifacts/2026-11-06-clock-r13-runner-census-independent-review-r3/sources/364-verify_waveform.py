"""Independent Definition R2 replay; stdlib only, never imports authored generation code."""
import argparse
from array import array
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import struct

SOURCE_REPLAY_REVIEW_ACCEPTED = False
ALL_REPLAY_HASH_HELPER_COSTS_ACCEPTED = False
STAGE_RESERVATION_ACCEPTED = False
ENTRY_EXECUTION_ACCEPTED = False
VENDOR_CONTEXT_QUALITY_PRODUCTION_RENDERER_ACCEPTED = False
BASE = Path(__file__).resolve().parent
CAP = 17179869184
RUNNERS = 2097152
CHILD = 7447259136
COMBINED = 14896615424
DATA_CAP = 74350592
CHUNK = 65536
SCALE = 1 << 56
LIMIT149 = 1 << 129
OWN = ('WaveformCandidate.h','waveform_probe.cpp','waveform_check.cpp','clock_oracle.cpp','plan_probe.cpp','independent_clock.h','fast_fit.h','freeze_waveform.py','verify_waveform.py','run-waveform.ps1','README.md')
SLOTS = {'phase':3235921920,'base':1926144000,'dyadic':481536000,'controls':737280000,'private':938377216,'fixed':128000000}


def require(ok, why):
    if not ok:
        raise RuntimeError(why)


class ReplayLedger:
    def __init__(self, prior):
        require(prior==RUNNERS+CHILD and prior+CHILD==COMBINED<=CAP, 'PRIOR_GENERATION_MUST_COMPOSE')
        self.prior = prior
        self.categories = {key:0 for key in SLOTS}
        self.helper = self.metadata_bytes = self.metadata_nodes = self.private_visits = 0
        self.owner_count = self.owner_bytes = self.owner_chunks = self.libm_calls = 0
        self.spend('fixed',128000000)
        self.spend('private',938377216)
    def spend(self, kind, amount):
        require(kind in SLOTS and 0<=amount<=SLOTS[kind]-self.categories[kind], 'REPLAY_CATEGORY_LIMIT')
        self.categories[kind] += amount
        require(self.prior+sum(self.categories.values())<=COMBINED, 'REPLAY_COMPOSED_LIMIT')
    def fixed(self, amount):
        require(0<=amount<=128000000-self.helper, 'REPLAY_ALL_HELPERS_LIMIT')
        self.helper += amount
    def node(self, count=1):
        require(0<=count<=131072-self.metadata_nodes, 'REPLAY_JSON_NODES')
        self.metadata_nodes += count
        self.fixed(256*count)
    def finish(self):
        require(self.categories==SLOTS and sum(self.categories.values())==CHILD, 'REPLAY_COMPLETE_CATEGORIES')
        require((self.owner_count,self.owner_bytes,self.owner_chunks)==(30,116858688,1796), 'REPLAY_ALL_FULL_OWNERS')
        require(self.private_visits<=1800000, 'REPLAY_PRIVATE_VISITS')
        return {'categories':dict(self.categories),'reserved':CHILD,'composed_after':self.prior+CHILD,'helper_upper_observed':self.helper,'metadata_bytes':self.metadata_bytes,'metadata_nodes':self.metadata_nodes,'private_visited_bytes':self.private_visits,'owner_count':self.owner_count,'owner_bytes':self.owner_bytes,'owner_chunks':self.owner_chunks,'libm_calls':self.libm_calls}


def json_bytes(value, maximum, budget):
    inspect_json(value, budget)
    raw = (json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False)+'\n').encode('ascii')
    require(len(raw) <= maximum, 'JSON_ENCODED_CAP')
    scan_json(raw, budget)
    return raw


def scan_json(raw, budget):
    require(len(raw) <= 4194304-budget.metadata_bytes, 'ALL_ENCODED_METADATA')
    budget.metadata_bytes += len(raw)
    budget.fixed(12*len(raw))
    depth = 0
    quoted = escaped = False
    for byte in raw:
        if quoted:
            if escaped:
                escaped = False
            elif byte == 92:
                escaped = True
            elif byte == 34:
                quoted = False
        elif byte == 34:
            quoted = True
        elif byte in (91,123):
            depth += 1
            require(depth <= 16, 'JSON_DEPTH')
        elif byte in (93,125):
            depth -= 1
            require(depth >= 0, 'JSON_DEPTH')
    require(depth == 0 and not quoted, 'JSON_TERMINATION')


def inspect_json(value, budget, count_pairs=True):
    stack = [(value,0)]
    while stack:
        item, depth = stack.pop()
        require(depth <= 16, 'JSON_DEPTH')
        if isinstance(item,dict):
            if count_pairs:
                budget.node(len(item))
            for key,child in item.items():
                require(isinstance(key,str) and len(key)<=1024, 'JSON_KEY')
                stack.append((child,depth+1))
        elif isinstance(item,list):
            budget.node(len(item))
            stack.extend((child,depth+1) for child in item)
        else:
            require(item is None or type(item) in (bool,int,float,str), 'JSON_TYPE')
            if isinstance(item,str):
                require(len(item)<=1024, 'JSON_STRING')
            if type(item) is int:
                require(abs(item).bit_length()<=256, 'JSON_INTEGER')
            if type(item) is float:
                require(math.isfinite(item), 'JSON_FINITE')


def load_json(raw, budget):
    scan_json(raw, budget)
    def pairs(rows):
        budget.node(len(rows))
        result = {}
        for key,value in rows:
            require(key not in result and len(key)<=1024, 'JSON_DUPLICATE_OR_KEY')
            result[key] = value
        return result
    value = json.loads(raw,object_pairs_hook=pairs,parse_constant=lambda x:require(False,'JSON_NONFINITE'))
    inspect_json(value,budget,False)
    return value


def no_link(path):
    for part in [path,*path.parents]:
        if part.exists() or part.is_symlink():
            require(not part.is_symlink() and not (getattr(part.lstat(),'st_file_attributes',0)&0x400), 'REPARSE_REFUSED')


def read_small(path, maximum, budget):
    no_link(path)
    require(path.is_file(), 'INPUT_FILE')
    size = path.stat().st_size
    require(0<=size<=maximum, 'INPUT_CAP_BEFORE_ALLOCATION')
    result = bytearray(size)
    position = 0
    with path.open('rb') as stream:
        while position<size:
            budget.fixed(128)
            count = stream.readinto(memoryview(result)[position:min(size,position+CHUNK)])
            require(count is not None and count>0, 'SHORT_INPUT')
            position += count
        budget.fixed(128)
        require(stream.read(1)==b'', 'INPUT_GREW')
    return result


def digest(path, budget, maximum):
    no_link(path)
    require(path.is_file() and path.stat().st_size<=maximum, 'HASH_INPUT_CAP')
    h = hashlib.sha256()
    size = 0
    with path.open('rb') as stream:
        while True:
            budget.fixed(128)
            block = stream.read(CHUNK)
            if not block:
                break
            size += len(block)
            require(size<=maximum, 'HASH_GROWTH')
            h.update(block)
    return size,h.hexdigest()


def checked_manifest(repo, expected_hash, budget):
    raw = read_small(BASE/'authored-manifest.json',262144,budget)
    require(hashlib.sha256(raw).hexdigest().upper()==expected_hash.upper(), 'EXTERNAL_MANIFEST_HASH')
    manifest = load_json(raw,budget)
    own = manifest['sources']
    require(len(own)==11 and {x['path'] for x in own}==set(OWN), 'EXACT_OWN_ELEVEN')
    require(len({x['path'] for x in own})==11 and len(manifest['preserved_pins'])<=128, 'UNIQUE_SOURCE_PINS')
    total = 0
    for item in own+manifest['preserved_pins']:
        path = BASE/item['path'] if item in own else repo/item['path']
        if item in own:
            require(Path(item['path']).name==item['path'], 'OWN_PATH_ESCAPE')
        n,h = digest(path,budget,item['bytes'])
        require(n==item['bytes'] and h.upper()==item['sha256'], 'CURRENT_SOURCE_OR_PROTECTED_INPUT_CHANGED')
        total += n
    require(total<=8388608, 'ALL_IMMUTABLE_PIN_BYTES')
    return manifest


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


def reference_interval(rate, frequency, p, q, n, pi, denominators):
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
        st = quotient(negative(product(st,square)),denominators[j-1][0])
        ct = quotient(negative(product(ct,square)),denominators[j-1][1])
        sine,cosine = plus(sine,st),plus(cosine,ct)
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


def exact_equal(observed, expected, why):
    require(observed==expected,why)


def control_word(name,n,ch,base):
    if name.endswith('mixed-signed-zero'):
        return 0x80000000 if (n+ch)%2 else 0
    if ch==1 and name.endswith('last-ulp') and n==479999:
        return base^1
    if ch==1 and name.endswith('late-nan'):
        if n==0:
            return base^1
        if n==479999:
            return 0x7fc00035
    return base


def error_exact(exact,bounds,scale):
    error = max(abs(exact-(bounds[0]<<(149-scale))),abs(exact-(bounds[1]<<(149-scale))))
    require(error<=LIMIT149,'REPLAY_SOURCE_ERROR_2POW_MINUS20')
    return error


def verify_profiles(root,budget):
    pi = pi_bracket()
    require(math.factorial(26)>1<<80,'REPLAY_FIXED_TAYLOR_REMAINDER')
    denominators = tuple(((2*j)*(2*j+1),(2*j-1)*(2*j)) for j in range(1,13))
    certs = []
    control48 = control56 = None
    for rate in (44100,48000,96000):
        for frequency in (220,997):
            for pp,qq,label in ((0,1,'p0'),(1,7,'p1q7')):
                period = rate//math.gcd(rate,frequency)
                key = f'r{rate}-f{frequency}-{label}'
                cert = load_json(read_small(root/f'references/source-certificates/{key}.json',65536,budget),budget)
                data = read_small(root/f'references/tone/{key}.bin',4*10*rate,budget)
                table = read_small(root/f'references/phase-intervals/{key}.bin',32*period,budget)
                require(len(data)==4*10*rate and len(table)==32*period, 'EXACT_PROFILE_LENGTH')
                require(4*10*rate+32*period+16*period+38400+262144<=16777216, 'REPLAY_PROFILE_PAYLOAD')
                require(cert['rate']==rate and cert['frequency']==frequency and cert['phase_p']==pp and cert['phase_q']==qq and cert['period']==period and cert['profile_id']==len(certs) and cert['words']==10*rate and cert['limit_numerator149']==LIMIT149,'PROFILE_DESCRIPTOR')
                require(hashlib.sha256(data).hexdigest()==cert['tone_sha256'] and hashlib.sha256(table).hexdigest()==cert['phase_sha256'],'PROFILE_HASHES')
                require((cert['pi_bounds_Q48'][0]<<8)<=pi[0]<=pi[1]<=(cert['pi_bounds_Q48'][1]<<8),'INDEPENDENT_PI_CONTAINED')
                refined = bytearray(16*period)
                for n in range(period):
                    budget.spend('phase',8192)
                    lo,hi,index,pid,reserved = struct.unpack_from('<qqQII',table,32*n)
                    require(index==n and pid==len(certs) and reserved==0 and lo<=hi,'EXACT_PHASE_RECORD')
                    a,b = reference_interval(rate,frequency,pp,qq,n,pi,denominators)
                    require((lo<<8)<=a<=b<=(hi<<8),'INDEPENDENT_ENCLOSURE_CONTAINMENT')
                    struct.pack_into('<qq',refined,16*n,a,b)
                maximum = -1
                witness = None
                for n in range(10*rate):
                    budget.spend('base',256)
                    budget.libm_calls += 1
                    # Full original n and exact grouping; never periodic libm-word reuse.
                    original = 0.2*math.sin(2*math.pi*frequency*n/rate+(math.pi/7 if pp else 0.0))
                    expected = struct.unpack('<I',struct.pack('<f',original))[0]
                    observed = struct.unpack_from('<I',data,4*n)[0]
                    exact_equal(observed,expected,'ORIGINAL_BASE_WORD_BITS')
                    lo,hi,_,_,_ = struct.unpack_from('<qqQII',table,32*(n%period))
                    a,b = struct.unpack_from('<qq',refined,16*(n%period))
                    exact = float149(observed)
                    e = error_exact(exact,(lo,hi),48)
                    error_exact(exact,(a,b),56)
                    if e>maximum:
                        maximum,witness = e,{'index':n,'word':observed,'interval':[lo,hi]}
                require(cert['maximum_numerator149']==maximum and cert['witness']==witness,'COMPLETE_SOURCE_MAXIMUM_WITNESS')
                if (rate,frequency,pp)==(48000,220,1):
                    control56 = bytes(refined)
                certs.append(cert)
                del data,table,refined
    require(len(certs)==12 and control56 is not None,'ALL_TONE_PROFILES')
    control48 = read_small(root/'references/phase-intervals/r48000-f220-p1q7.bin',76800,budget)
    return certs,control48,control56


def verify_dyadic(root,budget):
    for rate in (44100,48000,96000):
        zero = read_small(root/f'references/zero/r{rate}.bin',40*rate,budget)
        signal = read_small(root/f'references/burst/r{rate}.bin',40*rate,budget)
        require(len(zero)==len(signal)==40*rate and 80*rate+131072<=8388608,'DYADIC_PAYLOAD_LENGTH')
        radius = rate//2000
        positive = []
        for k in range(1,radius+1):
            v = (k*0x9e3779b9)&0xffffffff
            v = ((v^(v>>16))*0x85ebca6b)&0xffffffff
            v = ((v^(v>>13))*0xc2b2ae35)&0xffffffff
            v ^= v>>16
            positive.append(1 if v&1 else -1)
        values = list(reversed(positive))+[-2*sum(positive)]+positive
        require(values==list(reversed(values)) and sum(values)==0,'EXACT_BURST_LAW')
        power = 0
        while (1<<power)<4*max(map(abs,values)):
            power += 1
            require(power<=16,'BURST_FIXED_DOMAIN')
        expected = {}
        for point in (2*rate,4*rate,6*rate,8*rate):
            require(sum((point-radius+j)*v*v for j,v in enumerate(values))==point*sum(v*v for v in values),'EXACT_SOURCE_CENTROID')
            for j,value in enumerate(values):
                if value==0:
                    word = 0
                else:
                    n = abs(value)
                    top = n.bit_length()-1
                    require(top<=23 and -126<=top-power<=127,'EXACT_DYADIC_DOMAIN')
                    word = (0x80000000 if value<0 else 0)|((top-power+127)<<23)|((n<<(23-top))-(1<<23))
                expected[point-radius+j] = word
        for n in range(10*rate):
            budget.spend('dyadic',256)
            exact_equal(struct.unpack_from('<I',zero,4*n)[0],0,'EXACT_ZERO_WORD')
            exact_equal(struct.unpack_from('<I',signal,4*n)[0],expected.get(n,0),'EXACT_BURST_WORD')
        del zero,signal,expected


def verify_controls(root,budget,control48,control56):
    result = []
    for name in ('r48000-equivalent-last-ulp','r48000-early-distinct-late-nan','r48000-mixed-signed-zero'):
        raw = read_small(root/f'references/control-sources/{name}.interleaved.bin',3840000,budget)
        desc = load_json(read_small(root/f'references/control-sources/{name}.json',65536,budget),budget)
        require(len(raw)==3840000,'CONTROL_EXACT_LENGTH')
        maximum = nonfinite = 0
        witness = None
        for n in range(480000):
            budget.spend('controls',512)
            if name.endswith('mixed-signed-zero'):
                base = 0
            else:
                budget.libm_calls += 1
                original = 0.2*math.sin(2*math.pi*220*n/48000+math.pi/7)
                base = struct.unpack('<I',struct.pack('<f',original))[0]
            lo,hi,_,_,_ = struct.unpack_from('<qqQII',control48,32*(n%2400))
            a,b = struct.unpack_from('<qq',control56,16*(n%2400))
            for ch in (0,1):
                word = struct.unpack_from('<I',raw,8*n+4*ch)[0]
                exact_equal(word,control_word(name,n,ch,base),'EXACT_CONTROL_WORD_POSITION_PARITY_PAYLOAD')
                if word==0x7fc00035:
                    require(name.endswith('late-nan') and n==479999 and ch==1,'SOLE_EXPECTED_NAN')
                    nonfinite += 1
                    continue
                if name.endswith('mixed-signed-zero'):
                    e = 0
                else:
                    exact = float149(word)
                    e = error_exact(exact,(lo,hi),48)
                    error_exact(exact,(a,b),56)
                if e>maximum:
                    maximum,witness = e,{'index':n,'channel':ch,'word':word,'interval':[lo,hi]}
        expected = {'id':name,'words':960000,'valid_finite':not name.endswith('late-nan'),'nonfinite_count':int(name.endswith('late-nan')),'maximum_numerator149':maximum,'limit_numerator149':LIMIT149,'witness':witness,'owner_sha256':desc['owner_sha256'],'selection':[7,480000,480014],'logical':2,'sentinel_frames_each_end':7,'unselected_prefix_words':[0x3d800000,0xbe000000],'unselected_suffix_words':[0x3d800000,0xbe000000],'source_error_replay':'REQUIRED_UNRUN'}
        require(desc==expected and nonfinite==expected['nonfinite_count'],'ALL_CONTROL_DESCRIPTOR_AND_MAXIMUM_FIELDS')
        result.append(desc)
        del raw
    return result


def reconstructed_case(identifier):
    parts = identifier.split('-')
    rate = int(next(x[1:] for x in parts if x.startswith('r') and x[1:].isdigit()))
    mapping = int(next(x[3:] for x in parts if x.startswith('map') and x[3:].isdigit()))
    channels = 2 if 'equivalent' in parts or 'distinct' in parts or 'mixed-signed-zero' in identifier else 1
    mode = parts[0] if parts[0] in ('bake','reverse','zero','burst') else 'tone'
    f = [0,0]
    if mode=='tone' or mode=='bake':
        f[0] = int(next(x[1:] for x in parts if x.startswith('f') and x[1:].isdigit()))
        f[1] = 997 if 'distinct' in parts else f[0]
        phase = 'p0' if channels==1 or 'distinct' in parts else 'p1q7'
        left = 'references/tone/r%d-f%d-%s.bin'%(rate,f[0],phase)
        right = 'references/tone/r%d-f%d-p1q7.bin'%(rate,f[1]) if channels==2 else left
    else:
        left = right = 'references/%s/r%d.bin'%('zero' if mode=='zero' else 'burst',rate)
    if 'mixed-signed-zero' in identifier:
        left = right = 'references/control-sources/r48000-mixed-signed-zero.interleaved.bin'
    shape = next((i for i,x in enumerate(('Linear','EqualPower','SCurve','Log')) if x in parts),0)
    curve = -1 if 'minus1' in parts else 1 if 'plus1' in parts else 0
    fi,fo = (115200,115200) if 'overlap3L4' in parts else (307201,76800) if 'overlong2Lplus1' in parts else (0,0)
    physical = 2 if 'distinct' in parts else 1
    return {'id':identifier,'mode':mode,'rate':rate,'map':mapping,'logical':channels,'left':left,'right':right,'frequencies':f,'shape':shape,'curve':curve,'fade_in':fi,'fade_out':fo,'dsp_k':None,'queried_layout':'HOLD','context':'HOLD','inverse':'references/native-inverse/r48000-map5.bin','selection':[7,10*rate,10*rate+14],'native':[46081,153600],'reversed':mode=='reverse','physical':physical,'projection':[0] if channels==1 else [0,1] if physical==2 else [0,0]}


def independent_owner(root,rate,logical,left,right,budget):
    require(array('I').itemsize==4,'FOUR_BYTE_GROUPING_ONLY')
    sentinel = (bytes.fromhex('0000803d') if logical==1 else bytes.fromhex('0000803d000000be'))*7
    h = hashlib.sha256()
    h.update(sentinel)
    read_bytes = blocks = 0
    direct = logical==1 or left.endswith('.interleaved.bin')
    count = 65536 if direct else 32768
    with (root/left).open('rb') as lf:
        rf = None if direct else (root/right).open('rb')
        try:
            while True:
                budget.fixed(128)
                first = lf.read(count)
                if not first:
                    break
                require(len(first)%4==0,'OWNER_FOUR_BYTE_GROUPS')
                if direct:
                    output = first
                else:
                    second = rf.read(len(first))
                    require(len(second)==len(first),'OWNER_ALL_CHANNEL_GROUPS')
                    require(2*len(first)<=65536,'OWNER_GROUP_BUFFER_BEFORE_ALLOCATION')
                    lwords,rwords = array('I'),array('I')
                    lwords.frombytes(first)
                    rwords.frombytes(second)
                    combined = array('I',[0])*(len(lwords)*2)
                    combined[0::2] = lwords
                    combined[1::2] = rwords
                    output = combined.tobytes()
                require(len(output)<=65536 and read_bytes+len(output)<=40*rate*logical,'COMPLETE_OWNER_LIMIT')
                h.update(output)
                read_bytes += len(output)
                blocks += 1
            require(rf is None or rf.read(1)==b'','OWNER_RIGHT_EOF')
        finally:
            if rf is not None:
                rf.close()
    require(read_bytes==40*rate*logical,'COMPLETE_ORIGINAL_SELECTED_BYTES')
    h.update(sentinel)
    budget.owner_count += 1
    budget.owner_bytes += read_bytes+2*len(sentinel)
    budget.owner_chunks += blocks
    return h.hexdigest()


def verify_case_owners(root,manifest,certs,controls,budget):
    inventory = load_json(read_small(root/'references/original-case-inventory.json',262144,budget),budget)
    require(inventory['ids']==manifest['case_ids'] and inventory['count']==294 and inventory['controls']==manifest['control_ids'],'ORIGINAL_ORDERED_CASE_IDS')
    require(inventory['profiles']==certs and len(inventory['cases'])==294,'ALL_CASE_PROFILE_BINDINGS')
    cache = {}
    for desc in controls:
        rel = 'references/control-sources/'+desc['id']+'.interleaved.bin'
        key = (48000,2,rel,rel)
        cache[key] = independent_owner(root,*key,budget)
        require(cache[key]==desc['owner_sha256'],'FULL_CLASSIFICATION_CONTROL_ORIGINAL_HASH')
    base_keys = set()
    for identifier,actual in zip(manifest['case_ids'],inventory['cases']):
        expected = reconstructed_case(identifier)
        key = (expected['rate'],expected['logical'],expected['left'],expected['right'])
        base_keys.add(key)
        if key not in cache:
            cache[key] = independent_owner(root,*key,budget)
        expected['owner_sha256'] = cache[key]
        require(actual==expected,'EVERY_REQUEST_OWNER_NATIVE_REVERSE_FADE_DESCRIPTOR')
    require(len(base_keys)==28 and len(cache)==30 and sum(10*k[0]*k[1] for k in base_keys)==27294000 and sum(14*k[1] for k in base_keys)==616,'COMPLETE_DISTINCT_OWNER_KEYS')
    require(inventory=={'ids':manifest['case_ids'],'cases':inventory['cases'],'count':294,'controls':manifest['control_ids'],'profiles':certs,'source_offset':7,'prefix_frames':7,'suffix_frames':7,'left_sentinel_word':0x3d800000,'right_sentinel_word':0xbe000000,'owner_hashes_dedup':'30 distinct full original byte streams; mixed-zero control/base stream shared;294 requests retained'},'COMPLETE_INVENTORY_SCHEMA')
def expected_inventory():
    caps = {}
    for rate in (44100,48000,96000):
        for f in (220,997):
            for phase in ('p0','p1q7'):
                key = f'r{rate}-f{f}-{phase}'
                caps['references/tone/'+key+'.bin'] = 40*rate
                caps['references/phase-intervals/'+key+'.bin'] = 32*(rate//math.gcd(rate,f))
                caps['references/source-certificates/'+key+'.json'] = 65536
        for kind in ('zero','burst'):
            caps[f'references/{kind}/r{rate}.bin'] = 40*rate
    for control in ('r48000-equivalent-last-ulp','r48000-early-distinct-late-nan','r48000-mixed-signed-zero'):
        caps['references/control-sources/'+control+'.interleaved.bin'] = 3840000
        caps['references/control-sources/'+control+'.json'] = 65536
    for name in ('definition-manifest','reference-manifest','source-error-summary','original-case-inventory'):
        caps['references/'+name+'.json'] = 262144
    for variant in ('baseline','skip_late_finite','ignore_last_mismatch','swap_stored_channels','corrupt_constructed_output'):
        for kind,name in (('plan','MusicalAudioChannelPlan.h'),('materializer','MusicalAudioChannelMaterialization.h')):
            caps['generated/private-sources/'+variant+'/engine/'+name] = 65536
            for suffix in ('isolation.patch','isolation.inverse.patch'):
                caps[f'generated/private-sources/patches/{variant}.{kind}.{suffix}'] = 65536
            if (variant,kind) in (('skip_late_finite','plan'),('ignore_last_mismatch','plan'),('swap_stored_channels','materializer'),('corrupt_constructed_output','materializer')):
                for suffix in ('fault.patch','fault.inverse.patch'):
                    caps[f'generated/private-sources/patches/{variant}.{kind}.{suffix}'] = 65536
    for name in ('source-copy-manifest','source-copy-verification'):
        caps['generated/private-sources/'+name+'.json'] = 262144
    require(len(caps)==92 and sum(caps.values())==74350592,'FIXED_INDEPENDENT92')
    return caps


def data_fence(root,caps,budget):
    files = {}
    pending = [(root,0)]
    directories = total = 0
    while pending:
        directory,depth = pending.pop()
        no_link(directory)
        directories += 1
        require(directories<=128 and depth<=8,'DIRECTORY_GEOMETRY')
        for path in directory.iterdir():
            budget.fixed(128)
            no_link(path)
            if path.is_dir():
                pending.append((path,depth+1))
                require(len(pending)<=128,'DIRECTORY_QUEUE')
                continue
            rel = path.relative_to(root).as_posix()
            require(rel in caps and rel not in files and len(files)<92,'EXACT_FILE_SET')
            n,h = digest(path,budget,caps[rel])
            total += n
            require(total<=74350592,'WHOLE_DATA_POOL')
            files[rel] = {'bytes':n,'sha256':h}
    require(set(files)==set(caps),'ALL92_FILES_REQUIRED')
    return files,total


def generator_ledger(value):
    require(value['categories']==SLOTS and value['reserved']==CHILD and value['composed_after']==RUNNERS+CHILD,'GENERATOR_CATEGORIES_AND_PRIOR')
    require(0<=value['helper_upper_observed']<=128000000 and 0<=value['metadata_bytes']<=4194304 and 0<=value['metadata_nodes']<=131072 and 0<=value['private_visited_bytes']<=1800000,'GENERATOR_HELPER_BOUNDS')
    require((value['owner_count'],value['owner_bytes'],value['owner_chunks'])==(30,116858688,1796),'GENERATOR_OWNER_COVERAGE')


def verify_manifests(root,files,expected_hash,source_hash,budget):
    reference = load_json(read_small(root/'references/reference-manifest.json',262144,budget),budget)
    require(files['references/reference-manifest.json']['sha256']==expected_hash.lower(),'GENERATION_REFERENCE_HASH_BOUNDARY')
    definition = load_json(read_small(root/'references/definition-manifest.json',262144,budget),budget)
    without_reference = {k:v for k,v in files.items() if k!='references/reference-manifest.json'}
    first90 = {k:v for k,v in without_reference.items() if k!='references/definition-manifest.json'}
    require(len(first90)==90 and len(without_reference)==91,'TWO_MANIFEST_CLOSURE_ORDER')
    require(set(reference)=={'schema','files','ledger','source_manifest_sha256','status','production_admission'} and reference['schema']==1 and reference['files']==without_reference and reference['source_manifest_sha256']==source_hash.upper() and reference['status']=='GENERATED_SOURCE_ONLY_REPLAY_REQUIRED' and reference['production_admission'] is False,'REFERENCE_FULL_MANIFEST')
    require(set(definition)=={'schema','files','ledger','source_manifest_sha256','candidate','production_admission'} and definition['schema']==1 and definition['files']==first90 and definition['source_manifest_sha256']==source_hash.upper() and definition['candidate']=='UNRUN_HOLD' and definition['production_admission'] is False,'DEFINITION_FULL_MANIFEST')
    generator_ledger(reference['ledger'])
    generator_ledger(definition['ledger'])
    return reference


def verify_copies(root,repo,budget):
    rows = load_json(read_small(root/'generated/private-sources/source-copy-manifest.json',262144,budget),budget)
    require(set(rows)=={'lineage','visited_bytes','extra_helpers','format'} and rows['extra_helpers']==[],'GENERATED_HELPER_SCHEMA')
    verification = load_json(read_small(root/'generated/private-sources/source-copy-verification.json',262144,budget),budget)
    originals = {name:read_small(repo/'src/engine'/name,65536,budget) for name in ('MusicalAudioChannelPlan.h','MusicalAudioChannelMaterialization.h')}
    variants = ('baseline','skip_late_finite','ignore_last_mismatch','swap_stored_channels','corrupt_constructed_output')
    expected_rows = []
    generator_visits = 0
    def apply(data,patch):
        for edit in patch:
            old,new = edit['old'].encode(),edit['new'].encode()
            budget.private_visits += 2*len(data)
            require(budget.private_visits<=1800000 and data.count(old)==1,'INDEPENDENT_UNIQUE_SOURCE_ANCHOR')
            data = data.replace(old,new,1)
        return data
    def same(a,b,why):
        budget.private_visits += max(len(a),len(b))
        require(budget.private_visits<=1800000 and a==b,why)
    for variant in variants:
        for kind,name in (('plan','MusicalAudioChannelPlan.h'),('materializer','MusicalAudioChannelMaterialization.h')):
            isolation = [{'old':'namespace yesdaw::engine {','new':'namespace yesdaw::engine::waveform_private_'+variant+' {'}]
            if kind=='materializer':
                isolation.append({'old':'#include "engine/MusicalAudioChannelPlan.h"','new':'#include "MusicalAudioChannelPlan.h"'})
            fault = []
            if (variant,kind)==('skip_late_finite','plan'):
                fault = [{'old':'if ((left & 0x7f800000u) == 0x7f800000u)','new':'if (equivalent && (left & 0x7f800000u) == 0x7f800000u)'},{'old':'if ((right & 0x7f800000u) == 0x7f800000u)','new':'if (equivalent && (right & 0x7f800000u) == 0x7f800000u)'}]
            elif (variant,kind)==('ignore_last_mismatch','plan'):
                fault = [{'old':'if (left != right && ((left & 0x7fffffffu) != 0 || (right & 0x7fffffffu) != 0))','new':'if (i + 1 < selected.srcLen && left != right && ((left & 0x7fffffffu) != 0 || (right & 0x7fffffffu) != 0))'}]
            elif (variant,kind)==('swap_stored_channels','materializer'):
                fault = [{'old':'bits[projection[channel]]','new':'bits[projection[physicalChannels == 2 ? 1 - channel : channel]]'}]
            elif (variant,kind)==('corrupt_constructed_output','materializer'):
                fault = [{'old':'        working->accounting_ = accounting;','new':'        working->samples_[0] = std::bit_cast<float> (std::bit_cast<std::uint32_t> (working->samples_[0]) ^ 1u);\n        working->accounting_ = accounting;'}]
            folder = root/'generated/private-sources'
            for purpose,expected in (('isolation.patch',isolation),('isolation.inverse.patch',[{'old':x['new'],'new':x['old']} for x in reversed(isolation)])):
                actual = load_json(read_small(folder/f'patches/{variant}.{kind}.{purpose}',65536,budget),budget)
                require(actual==expected,'EXACT_REVIEWED_ISOLATION_PATCH')
            if fault:
                for purpose,expected in (('fault.patch',fault),('fault.inverse.patch',[{'old':x['new'],'new':x['old']} for x in reversed(fault)])):
                    actual = load_json(read_small(folder/f'patches/{variant}.{kind}.{purpose}',65536,budget),budget)
                    require(actual==expected,'EXACT_REVIEWED_SEMANTIC_PATCH')
            original = originals[name]
            isolated = apply(original,isolation)
            final = apply(isolated,fault)
            actual = read_small(folder/variant/'engine'/name,65536,budget)
            same(actual,final,'ACTUAL_PRIVATE_HEADER_EXACT_FORWARD')
            restored = apply(actual,[{'old':x['new'],'new':x['old']} for x in reversed(fault)])
            if fault:
                same(restored,isolated,'SEMANTIC_INVERSE_TO_ISOLATED_BASELINE')
            restored = apply(restored,[{'old':x['new'],'new':x['old']} for x in reversed(isolation)])
            same(restored,original,'ISOLATION_INVERSE_TO_PRODUCTION')
            generator_visits += len(original)*8+len(final)*4+sum(len(x['old'].encode())+len(x['new'].encode()) for x in isolation+fault)
            expected_rows.append({'variant':variant,'kind':kind,'original_sha256':hashlib.sha256(original).hexdigest(),'isolated_sha256':hashlib.sha256(isolated).hexdigest(),'final_sha256':hashlib.sha256(final).hexdigest(),'fault_inverse_exact':True,'isolation_inverse_exact':True})
    require(rows['lineage']==expected_rows and rows['visited_bytes']==generator_visits<=1800000 and rows['format']=='exact old/new anchor patches, not fuzzy unified diff','COMPLETE_PRIVATE_LINEAGE')
    require(verification=={'status':'GENERATED_INVERSES_CHECKED_NOT_COMPILED','count':10,'visited_bytes':generator_visits},'PRIVATE_REPLAY_RECEIPT')


def in_memory_controls(budget):
    budget.fixed(1048576)
    def rejects(function):
        failed = False
        try:
            function()
        except RuntimeError:
            failed = True
        require(failed,'IN_MEMORY_OWN_GATE_CONTROL_NOT_REACHED')
    rejects(lambda:exact_equal(0x3e000001,0x3e000000,'base oneULP'))
    last = 'r48000-equivalent-last-ulp'
    late = 'r48000-early-distinct-late-nan'
    zero = 'r48000-mixed-signed-zero'
    require(control_word(last,479999,1,0x3e000000)==0x3e000001,'last control construction')
    rejects(lambda:exact_equal(control_word(last,479998,1,0x3e000000),0x3e000001,'moved last ULP'))
    rejects(lambda:exact_equal(control_word(late,479999,0,0x3e000000),0x7fc00035,'wrong NaN channel'))
    rejects(lambda:exact_equal(control_word(late,479999,1,0),0x7fc00000,'wrong NaN payload'))
    rejects(lambda:exact_equal(control_word(zero,0,0,0),0x80000000,'inverted zero parity'))
    rejects(lambda:error_exact(LIMIT149+1,(0,0),56))
    require(bytes.fromhex('0000803d000000be')!=bytes.fromhex('000000be0000803d'),'interleaving control')
    require(expected_inventory().get('references/not-listed.bin') is None,'unlisted control')
    rejects(lambda:require(74350592<=74350591,'pool one below'))
    rejects(lambda:require(COMBINED<=COMBINED-1,'work one below'))
    rejects(lambda:require('missing' in {'actual':'hash'},'missing predecessor'))


def replay(root,repo,expected_manifest,reference_hash,prior):
    require(SOURCE_REPLAY_REVIEW_ACCEPTED and ALL_REPLAY_HASH_HELPER_COSTS_ACCEPTED and STAGE_RESERVATION_ACCEPTED,'SOURCE_HOLD_NO_INDEPENDENT_REPLAY_GO')
    budget = ReplayLedger(prior)
    manifest = checked_manifest(repo,expected_manifest,budget)
    caps = expected_inventory()
    require({x['path']:x['bytes_cap'] for x in manifest['stage_artifacts']}==caps,'REVIEWED_INVENTORY_EQUALS_INDEPENDENT_FIXED_SET')
    before,total = data_fence(root,caps,budget)
    generation = verify_manifests(root,before,reference_hash,expected_manifest,budget)
    in_memory_controls(budget)
    verify_copies(root,repo,budget)
    certs,control48,control56 = verify_profiles(root,budget)
    verify_dyadic(root,budget)
    controls = verify_controls(root,budget,control48,control56)
    del control48,control56
    verify_case_owners(root,manifest,certs,controls,budget)
    summary = load_json(read_small(root/'references/source-error-summary.json',262144,budget),budget)
    require(summary['status']=='GENERATED_REQUIRE_EXACT_INDEPENDENT_REPLAY' and summary['profiles']==12 and summary['phase_rows']==395010 and summary['base_tone_words']==7524000 and summary['dyadic_base_words']==3762000 and summary['control_words']==2880000 and summary['pcm_words']==14166000 and summary['finite_pcm_words']==14165999 and summary['intentional_nonfinite_words']==1,'COMPLETE_EXACT_WORD_INVENTORY')
    require(summary['base_maximum_numerator149']==max(x['maximum_numerator149'] for x in certs) and summary['control_maxima']=={x['id']:x['maximum_numerator149'] for x in controls} and summary['vendor_invoked'] is False and summary['production_admission'] is False,'ALL_FINITE_SOURCE_MAXIMA')
    generator_ledger(summary['ledger'])
    after,total_after = data_fence(root,caps,budget)
    require(before==after and total==total_after,'AFTER_REPLAY_ALL92_INPUT_FENCE')
    checked_manifest(repo,expected_manifest,budget)
    return {'status':'DEFINITION_SOURCE_REPLAY_COMPLETE_REQUIRES_FRESH_ROOT_OUTPUT_REVIEW','source_manifest_sha256':expected_manifest.upper(),'reference_manifest_sha256':reference_hash.lower(),'artifact_count':92,'retained_data_bytes':total,'pcm_words':14166000,'finite_pcm_words':14165999,'intentional_nonfinite_words':1,'generator_reserved':generation['ledger']['reserved'],'ledger':budget.finish(),'vendor':False,'context':False,'queried_layout':False,'quality':False,'production_admission':False,'renderer':False}


if __name__=='__main__':
    require(ENTRY_EXECUTION_ACCEPTED,'SOURCE_HOLD_NO_REPLAY_EXECUTION_AUTHORITY')
    parser = argparse.ArgumentParser()
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--manifest-sha',required=True)
    parser.add_argument('--reference-sha',required=True)
    parser.add_argument('--prior-work',type=int,required=True)
    args = parser.parse_args()
    result = replay(args.root.absolute(),args.repo.absolute(),args.manifest_sha,args.reference_sha,args.prior_work)
    print(json.dumps(result,sort_keys=True,separators=(',',':'),allow_nan=False))
