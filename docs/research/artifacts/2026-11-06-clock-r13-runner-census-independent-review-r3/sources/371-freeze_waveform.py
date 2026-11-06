"""Definition R3 source only; all execution fences deliberately HOLD."""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import struct

SOURCE_REVIEW_ACCEPTED = False
INTERVAL_AND_ALL_HELPER_COSTS_ACCEPTED = False
RETAINED_STAGE_BOUNDS_ACCEPTED = False
ENTRY_EXECUTION_ACCEPTED = False
VENDOR_CONTEXT_QUALITY_PRODUCTION_RENDERER_ACCEPTED = False
BASE = Path(__file__).resolve().parent
CAP = 1 << 34
RUNNERS = 2 << 20
CHILD = 7447259136
COMBINED = 14896615424
DATA_CAP = 74350592
RETAINED_CAP = 141459456
CHUNK = 65536
U = 1 << 48
ERROR = 1 << 129
OWN = ('WaveformCandidate.h','waveform_probe.cpp','waveform_check.cpp','clock_oracle.cpp','plan_probe.cpp','independent_clock.h','fast_fit.h','freeze_waveform.py','verify_waveform.py','run-waveform.ps1','README.md')
SLOTS = {'phase':3235921920,'base':1926144000,'dyadic':481536000,'controls':737280000,'private':938377216,'fixed':128000000}


def require(ok, why):
    if not ok:
        raise RuntimeError(why)


class GateRejected(RuntimeError):
    def __init__(self, reason, witness):
        super().__init__(reason)
        self.reason,self.witness = reason,witness


def artifact_name_gate(name,caps,seen):
    if name not in caps or name in seen or len(seen)>=92:
        raise GateRejected('EXACT_FILE_SET',{'name':name,'listed':name in caps,'seen':name in seen,'count':len(seen)})


def data_pool_gate(used,amount,limit=DATA_CAP):
    if not (0<=used and 0<=amount and 0<=limit<=DATA_CAP and amount<=limit-used):
        raise GateRejected('WHOLE_DATA_POOL',{'used':used,'amount':amount,'limit':limit})


def work_reservation_gate(reserved,limit=CAP):
    if not (0<=reserved<=limit<=CAP):
        raise GateRejected('WORK_RESERVATION',{'reserved':reserved,'limit':limit,'global_cap':CAP})


def expect_gate(name,function,reason,witness):
    try:
        function()
    except GateRejected as error:
        require(error.reason==reason and error.witness==witness,'CONTROL_WRONG_GATE_OR_WITNESS:'+name)
        return {'id':name,'gate':error.reason,'witness':error.witness}
    raise RuntimeError('CONTROL_GATE_NOT_REACHED:'+name)


class Budget:
    def __init__(self, prior, work_limit=CAP):
        work_reservation_gate(COMBINED,work_limit)
        require(prior == RUNNERS and RUNNERS+2*CHILD == COMBINED, 'ONE_PREPAID_PARENT')
        self.prior = prior
        self.categories = {k:0 for k in SLOTS}
        self.helper = self.metadata_bytes = self.metadata_nodes = self.private_visits = 0
        self.owner_chunks = self.owner_bytes = self.owner_count = self.libm_calls = 0
        self.spend('fixed', SLOTS['fixed'])
        self.spend('private', SLOTS['private'])
    def spend(self, kind, amount):
        require(kind in SLOTS and 0 <= amount <= SLOTS[kind]-self.categories[kind], 'CATEGORY_LIMIT:'+kind)
        self.categories[kind] += amount
        require(self.prior+sum(self.categories.values()) <= COMBINED, 'COMPOSED_WORK')
    def fixed(self, amount):
        require(0 <= amount <= SLOTS['fixed']-self.helper, 'FIXED_HELPER_LIMIT')
        self.helper += amount
    def node(self, count=1):
        require(0 <= count <= 131072-self.metadata_nodes, 'JSON_NODE_LIMIT')
        self.metadata_nodes += count
        self.fixed(256*count)
    def finish(self):
        require(self.categories == SLOTS and sum(self.categories.values()) == CHILD, 'ALL_CATEGORIES_REQUIRED')
        require(self.owner_count == 30 and self.owner_bytes == 116858688 and self.owner_chunks == 1796, 'ALL_COMPLETE_OWNER_STREAMS')
        require(self.private_visits <= 1800000, 'PRIVATE_VISITS')
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
    require(len({x['path'] for x in own})==11 and len(manifest['preserved_pins'])==129 and len(own)+len(manifest['preserved_pins'])+2<=144, 'UNIQUE_SOURCE_PINS')
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


class Store:
    def __init__(self, root, manifest, budget):
        require(not root.exists(), 'NEW_DATA_ROOT')
        require(DATA_CAP+67108864==RETAINED_CAP, 'WHOLE_FIRST_STAGE_PREPAYMENT')
        self.root,self.budget = root,budget
        self.allowed = {x['path']:x['bytes_cap'] for x in manifest['stage_artifacts']}
        require(len(self.allowed)==92 and sum(self.allowed.values())==DATA_CAP, 'EXACT_92_CAPS')
        self.files = {}
        self.used = self.private_used = self.reference_used = 0
        root.mkdir()
        no_link(root)
    def admit(self, name, length):
        artifact_name_gate(name,self.allowed,self.files)
        require(0<=length<=self.allowed[name], 'OUTPUT_PATH_OR_CAP')
        data_pool_gate(self.used,length,DATA_CAP)
        private = name.startswith('generated/')
        require(length <= (3014656-self.private_used if private else 71335936-self.reference_used), 'SUBPOOL_CAP')
        path = self.root/name
        require(path.absolute().is_relative_to(self.root.absolute()) and '..' not in Path(name).parts, 'OUTPUT_ESCAPE')
        no_link(path)
        require(not path.exists() and not Path(str(path)+'.part').exists(), 'OUTPUT_OVERWRITE')
        return path,private
    def put(self, name, data):
        if not name.startswith(('references/','generated/')):
            name = 'references/'+name
        path,private = self.admit(name,len(data))
        path.parent.mkdir(parents=True,exist_ok=True)
        part = Path(str(path)+'.part')
        h = hashlib.sha256()
        view = memoryview(data)
        with part.open('xb') as stream:
            for begin in range(0,len(view),CHUNK):
                self.budget.fixed(128)
                block = view[begin:begin+CHUNK]
                require(stream.write(block)==len(block), 'SHORT_WRITE')
                h.update(block)
        part.rename(path)
        self.used += len(data)
        if private:
            self.private_used += len(data)
        else:
            self.reference_used += len(data)
        self.files[name] = {'bytes':len(data),'sha256':h.hexdigest()}
        return self.files[name]
    def json(self, name, value):
        key = name if name.startswith(('references/','generated/')) else 'references/'+name
        return self.put(key,json_bytes(value,self.allowed[key],self.budget))


def ceildiv(a, b):
    require(b > 0, 'POSITIVE_DIVISOR')
    return -((-a)//b)


def interval(a, b):
    require(isinstance(a, int) and isinstance(b, int) and a <= b and max(abs(a), abs(b)) <= 2*U, 'Q48_INTERVAL_WIDTH')
    return a, b


def add(a, b):
    return interval(a[0]+b[0], a[1]+b[1])


def neg(a):
    return -a[1], -a[0]


def mul(a, b):
    require(max(map(abs, a)) <= 2*U and max(map(abs, b)) <= 2*U, 'Q48_PRODUCT_PREFLIGHT')
    products = [x*y for x in a for y in b]
    require(max(map(abs, products)) <= 1 << 98, 'Q48_PRODUCT_WIDTH')
    return interval(min(products)//U, ceildiv(max(products), U))


def div(a, n):
    require(n > 0, 'Q48_DIVISOR')
    return interval(a[0]//n, ceildiv(a[1], n))


def rational(n, d):
    require(d > 0 and abs(n).bit_length() <= 192 and d.bit_length() <= 192, 'RATIONAL_WIDTH')
    return n*U//d, ceildiv(n*U, d)


def machin():
    def atan(d, count):
        total = (0, 0)
        for k in range(count):
            term = rational(1, (2*k+1)*d**(2*k+1))
            total = (total[0]+term[0], total[1]+term[1]) if k % 2 == 0 else (total[0]-term[1], total[1]-term[0])
        remainder = ceildiv(U, (2*count+1)*d**(2*count+1))
        return (total[0], total[1]+remainder) if count % 2 == 0 else (total[0]-remainder, total[1])
    a, b = atan(5, 24), atan(239, 8)
    result = 16*a[0]-4*b[1], 16*a[1]-4*b[0]
    require(3*U < result[0] <= result[1] < 4*U, 'MACHIN_BRACKET')
    require(math.factorial(26) > 1 << 80, 'TAYLOR_REMAINDER')
    return result


def coefficient(k, sine):
    value = rational(1, math.factorial(2*k+int(sine)))
    return neg(value) if k % 2 else value


def analytic_interval(rate, frequency, phase_p, phase_q, n, pi_bounds, coefficients):
    require(rate in (44100, 48000, 96000) and frequency in (220, 997) and phase_p in (0, 1) and phase_q in (1, 7), 'TONE_PROFILE')
    a, b = 2*frequency*n*phase_q+phase_p*rate, rate*phase_q
    require(a < 1 << 63 and b < 1 << 32, 'EXACT_PHASE_WIDTH')
    k = (4*a+b)//(2*b)
    h = 2*a-k*b
    require(abs(h)*2 <= b, 'QUADRANT_REDUCTION')
    ends = [x*h for x in pi_bounds]
    theta = interval(min(ends)//(2*b), ceildiv(max(ends), 2*b))
    square = mul(theta, theta)
    sin_poly, cos_poly = coefficients[0][12], coefficients[1][12]
    for j in range(11, -1, -1):
        sin_poly = add(coefficients[0][j], mul(square, sin_poly))
        cos_poly = add(coefficients[1][j], mul(square, cos_poly))
    sine, cosine = mul(theta, sin_poly), cos_poly
    sine = interval(sine[0]-1, sine[1]+1)
    cosine = interval(cosine[0]-1, cosine[1]+1)
    value = (sine, cosine, neg(sine), neg(cosine))[k % 4]
    return div(value, 5)


def numerator149(word):
    sign = -1 if word >> 31 else 1
    exponent, mantissa = (word >> 23) & 255, word & 0x7fffff
    require(exponent != 255, 'NONFINITE_SOURCE_WORD')
    return sign*(mantissa if exponent == 0 else ((1 << 23)+mantissa) << (exponent-1))


def source_error(word, bounds):
    exact = numerator149(word)
    error = max(abs(exact-(bounds[0] << 101)), abs(exact-(bounds[1] << 101)))
    require(error <= ERROR, 'SOURCE_ERROR_2POW_MINUS20')
    return error


def replacements(data, edits):
    current = data
    for old, new in edits:
        require(current.count(old) == 1, 'SOURCE_ANCHOR_NOT_UNIQUE')
        current = current.replace(old, new, 1)
    return current


def patch_bytes(edits, budget):
    return json_bytes([{'old':a.decode(), 'new':b.decode()} for a,b in edits],65536,budget)


def source_copies(repo, store, proof):
    require(29041+3014656+4194304+393216<=8388608, 'PRIVATE_PHASE_PAYLOAD_BEFORE_ALLOCATION')
    for item in proof['generated_source_preparation']['literal_generated_artifacts']:
        store.admit(item['path'],item['bytes_cap'])
    initial = {k:read_small(repo/k,65536,store.budget) for k in proof['generated_source_preparation']['production_inputs']}
    for k,h in proof['generated_source_preparation']['production_inputs'].items():
        require(hashlib.sha256(initial[k]).hexdigest().upper() == h, 'PRODUCTION_SOURCE_PIN')
    products, lineage, visits = [], [], 0
    variants = proof['generated_source_preparation']['variants']
    finite = [(b'if ((left & 0x7f800000u) == 0x7f800000u)', b'if (equivalent && (left & 0x7f800000u) == 0x7f800000u)'), (b'if ((right & 0x7f800000u) == 0x7f800000u)', b'if (equivalent && (right & 0x7f800000u) == 0x7f800000u)')]
    last = [(b'if (left != right && ((left & 0x7fffffffu) != 0 || (right & 0x7fffffffu) != 0))', b'if (i + 1 < selected.srcLen && left != right && ((left & 0x7fffffffu) != 0 || (right & 0x7fffffffu) != 0))')]
    swap = [(b'bits[projection[channel]]', b'bits[projection[physicalChannels == 2 ? 1 - channel : channel]]')]
    corrupt = [(b'        working->accounting_ = accounting;', b'        working->samples_[0] = std::bit_cast<float> (std::bit_cast<std::uint32_t> (working->samples_[0]) ^ 1u);\n        working->accounting_ = accounting;')]
    for variant in variants:
        for kind, name in [('plan','MusicalAudioChannelPlan.h'), ('materializer','MusicalAudioChannelMaterialization.h')]:
            original = initial['src/engine/'+name]
            isolation = [(b'namespace yesdaw::engine {', ('namespace yesdaw::engine::waveform_private_'+variant+' {').encode())]
            if kind == 'materializer':
                isolation.append((b'#include "engine/MusicalAudioChannelPlan.h"', b'#include "MusicalAudioChannelPlan.h"'))
            isolated = replacements(original, isolation)
            fault = finite if variant == 'skip_late_finite' and kind == 'plan' else last if variant == 'ignore_last_mismatch' and kind == 'plan' else swap if variant == 'swap_stored_channels' and kind == 'materializer' else corrupt if variant == 'corrupt_constructed_output' and kind == 'materializer' else []
            final = replacements(isolated, fault) if fault else isolated
            restored = replacements(final, [(b,a) for a,b in reversed(fault)]) if fault else final
            require(restored == isolated, 'FAULT_INVERSE')
            require(replacements(restored, [(b,a) for a,b in reversed(isolation)]) == original, 'ISOLATION_INVERSE')
            prefix = 'generated/private-sources/'
            products += [(prefix+variant+'/engine/'+name, final), (prefix+'patches/'+variant+'.'+kind+'.isolation.patch', patch_bytes(isolation, store.budget)), (prefix+'patches/'+variant+'.'+kind+'.isolation.inverse.patch', patch_bytes([(b,a) for a,b in reversed(isolation)], store.budget))]
            if fault:
                products += [(prefix+'patches/'+variant+'.'+kind+'.fault.patch', patch_bytes(fault, store.budget)), (prefix+'patches/'+variant+'.'+kind+'.fault.inverse.patch', patch_bytes([(b,a) for a,b in reversed(fault)], store.budget))]
            visits += len(original)*8+len(final)*4+sum(len(x)+len(y) for x,y in isolation+fault)
            lineage.append({'variant':variant, 'kind':kind, 'original_sha256':hashlib.sha256(original).hexdigest(), 'isolated_sha256':hashlib.sha256(isolated).hexdigest(), 'final_sha256':hashlib.sha256(final).hexdigest(), 'fault_inverse_exact':True, 'isolation_inverse_exact':True})
    require(visits <= 1800000, 'SOURCE_COPY_VISITED_BYTE_CAP')
    store.budget.private_visits = visits
    require(sum(len(data) for _,data in products)<=3014656, 'PRIVATE_BUFFER_SUM')
    for name,data in products:
        store.put(name, data)
    store.json('generated/private-sources/source-copy-manifest.json', {'lineage':lineage, 'visited_bytes':visits, 'extra_helpers':[], 'format':'exact old/new anchor patches, not fuzzy unified diff'})
    store.json('generated/private-sources/source-copy-verification.json', {'status':'GENERATED_INVERSES_CHECKED_NOT_COMPILED', 'count':len(lineage), 'visited_bytes':visits})


def burst(rate):
    radius = rate//2000
    q = []
    for k in range(1, radius+1):
        v = (k*0x9e3779b9)&0xffffffff;v ^= v >> 16;v = (v*0x85ebca6b)&0xffffffff;v ^= v >> 13;v = (v*0xc2b2ae35)&0xffffffff;v ^= v >> 16
        q.append(1 if v&1 else -1)
    center = -2*sum(q)
    m = 0
    while max(1, abs(center))*2**(-m) > 0.25:
        m += 1
    words = bytearray(4*10*rate)
    values = list(reversed(q))+[center]+q
    require(sum(values) == 0 and values == list(reversed(values)), 'EXACT_BURST_SUM_SYMMETRY')
    for point in [2*rate,4*rate,6*rate,8*rate]:
        for i,value in enumerate(values):
            struct.pack_into('<f', words, 4*(point-radius+i), value*2**(-m))
    return words


def case_row(case_id):
    rate = int(re.search(r'-r(\d+)',case_id).group(1))
    map_index = int(re.search(r'-map(\d+)',case_id).group(1))
    logical = 2 if any(x in case_id for x in ('equivalent','distinct','mixed-signed-zero')) else 1
    kind = 'bake' if case_id.startswith('bake-') else 'reverse' if case_id.startswith('reverse-') else 'zero' if case_id.startswith('zero-') else 'burst' if case_id.startswith('burst-') else 'tone'
    frequencies = [0,0]
    if kind in ('tone','bake'):
        f = int(re.search(r'-f(\d+)',case_id).group(1))
        frequencies = [f,997 if 'distinct' in case_id else f]
        left = f'references/tone/r{rate}-f{f}-'+('p0' if logical==1 or 'distinct' in case_id else 'p1q7')+'.bin'
        right = f'references/tone/r{rate}-f{frequencies[1]}-p1q7.bin' if logical==2 else left
    else:
        left = right = f'references/{"zero" if kind=="zero" else "burst"}/r{rate}.bin'
    if 'mixed-signed-zero' in case_id:
        left = right = 'references/control-sources/r48000-mixed-signed-zero.interleaved.bin'
    shape = next((i for i,n in enumerate(('Linear','EqualPower','SCurve','Log')) if '-'+n+'-' in case_id),0)
    curve = -1 if '-minus1-' in case_id else 1 if '-plus1-' in case_id else 0
    fi = 115200 if 'overlap3L4' in case_id else 307201 if 'overlong2Lplus1-L2' in case_id else 0
    fo = fi if 'overlap3L4' in case_id else 76800 if 'overlong2Lplus1-L2' in case_id else 0
    physical = 2 if 'distinct' in case_id else 1
    return {'id':case_id,'mode':kind,'rate':rate,'map':map_index,'logical':logical,'left':left,'right':right,'frequencies':frequencies,'shape':shape,'curve':curve,'fade_in':fi,'fade_out':fo,'dsp_k':None,'queried_layout':'HOLD','context':'HOLD','inverse':'references/native-inverse/r48000-map5.bin','selection':[7,10*rate,10*rate+14],'native':[46081,153600],'reversed':kind=='reverse','physical':physical,'projection':[0] if logical==1 else [0,1] if physical==2 else [0,0]}


def owner_hash(root, rate, logical, left, right, budget):
    require(rate in (44100,48000,96000) and logical in (1,2), 'OWNER_LAYOUT')
    prefix = struct.pack('<I',0x3d800000) if logical==1 else struct.pack('<II',0x3d800000,0xbe000000)
    sentinel = prefix*7
    h = hashlib.sha256()
    h.update(sentinel)
    selected = 0
    chunks = 0
    direct = logical==1 or left.endswith('.interleaved.bin')
    plane_chunk = CHUNK if direct else CHUNK//2
    with (root/left).open('rb') as a:
        b = None if direct else (root/right).open('rb')
        try:
            while True:
                budget.fixed(128)
                x = a.read(plane_chunk)
                if not x:
                    break
                require(len(x)%4==0, 'OWNER_WORD_ALIGNMENT')
                if direct:
                    block = x
                else:
                    y = b.read(len(x))
                    require(len(y)==len(x), 'OWNER_PLANE_LENGTH')
                    require(2*len(x)<=CHUNK, 'OWNER_BEFORE_ALLOCATION')
                    block = bytearray(2*len(x))
                    for byte in range(4):
                        block[byte::8] = x[byte::4]
                        block[byte+4::8] = y[byte::4]
                require(len(block)<=CHUNK and selected+len(block)<=4*10*rate*logical, 'OWNER_CHUNK_CAP')
                h.update(block)
                selected += len(block)
                chunks += 1
            require(b is None or b.read(1)==b'', 'OWNER_EXTRA_RIGHT')
        finally:
            if b is not None:
                b.close()
    require(selected==4*10*rate*logical, 'OWNER_COMPLETE_SELECTED')
    h.update(sentinel)
    budget.owner_count += 1
    budget.owner_bytes += selected+2*len(sentinel)
    budget.owner_chunks += chunks
    return h.hexdigest()


def profiles(store, budget):
    pi_bounds = machin()
    coefficients = (tuple(coefficient(k,True) for k in range(13)),tuple(coefficient(k,False) for k in range(13)))
    result = []
    for rate in (44100,48000,96000):
        for frequency in (220,997):
            for pp,qq,label in ((0,1,'p0'),(1,7,'p1q7')):
                period = rate//math.gcd(frequency,rate)
                key = f'r{rate}-f{frequency}-{label}'
                require(4*10*rate+32*period+131072+196608<=8388608, 'PROFILE_PAYLOAD')
                store.admit('references/phase-intervals/'+key+'.bin',32*period)
                store.admit('references/tone/'+key+'.bin',4*10*rate)
                require(4*10*rate+32*period<=DATA_CAP-store.used, 'PROFILE_BATCH_RETAINED_CAP')
                table = bytearray(32*period)
                for n in range(period):
                    budget.spend('phase',8192)
                    lo,hi = analytic_interval(rate,frequency,pp,qq,n,pi_bounds,coefficients)
                    struct.pack_into('<qqQII',table,32*n,lo,hi,n,len(result),0)
                data = bytearray(4*10*rate)
                maximum = -1
                witness = None
                for n in range(10*rate):
                    budget.spend('base',256)
                    budget.libm_calls += 1
                    value = 0.2*math.sin(2*math.pi*frequency*n/rate+(math.pi/7 if pp else 0.0))
                    struct.pack_into('<f',data,4*n,value)
                    word = struct.unpack_from('<I',data,4*n)[0]
                    lo,hi,_,_,_ = struct.unpack_from('<qqQII',table,32*(n%period))
                    e = source_error(word,(lo,hi))
                    if e>maximum:
                        maximum,witness = e,{'index':n,'word':word,'interval':[lo,hi]}
                tone = store.put('tone/'+key+'.bin',data)
                phase = store.put('phase-intervals/'+key+'.bin',table)
                cert = {'rate':rate,'frequency':frequency,'phase_p':pp,'phase_q':qq,'period':period,'profile_id':len(result),'words':10*rate,'maximum_numerator149':maximum,'limit_numerator149':ERROR,'witness':witness,'tone_sha256':tone['sha256'],'phase_sha256':phase['sha256'],'pi_bounds_Q48':list(pi_bounds),'independent_replay':'REQUIRED_UNRUN'}
                require(len(json_bytes(cert,2048,budget))<=2048, 'PROFILE_DESCRIPTOR_GEOMETRY')
                store.json('source-certificates/'+key+'.json',cert)
                result.append(cert)
                del data,table
    return result,pi_bounds


def dyadic_sources(store, budget):
    for rate in (44100,48000,96000):
        budget.spend('dyadic',128*20*rate)
        require(4*10*rate+131072<=8388608, 'DYADIC_PAYLOAD')
        store.admit(f'references/zero/r{rate}.bin',4*10*rate)
        zero = bytes(4*10*rate)
        store.put(f'zero/r{rate}.bin',zero)
        del zero
        store.admit(f'references/burst/r{rate}.bin',4*10*rate)
        signal = burst(rate)
        store.put(f'burst/r{rate}.bin',signal)
        del signal


def control_sources(store, budget, cache):
    left_path = 'references/tone/r48000-f220-p1q7.bin'
    left = read_small(store.root/left_path,1920000,budget)
    table = read_small(store.root/'references/phase-intervals/r48000-f220-p1q7.bin',76800,budget)
    require(len(left)==1920000 and len(table)==76800 and 1920000+3840000+1310720<=8388608, 'CONTROL_PAYLOAD')
    descriptors = []
    for name in ('r48000-equivalent-last-ulp','r48000-early-distinct-late-nan','r48000-mixed-signed-zero'):
        rel = 'references/control-sources/'+name+'.interleaved.bin'
        store.admit(rel,3840000)
        data = bytearray(3840000)
        maximum = 0
        witness = None
        nonfinite = 0
        for n in range(480000):
            budget.spend('controls',512)
            word = struct.unpack_from('<I',left,4*n)[0]
            a,b = word,word
            if name.endswith('mixed-signed-zero'):
                a,b = (0,0x80000000) if n%2==0 else (0x80000000,0)
            elif name.endswith('last-ulp') and n==479999:
                b ^= 1
            elif name.endswith('late-nan'):
                if n==0:
                    b ^= 1
                if n==479999:
                    b = 0x7fc00035
            struct.pack_into('<II',data,8*n,a,b)
            lo,hi,_,_,_ = struct.unpack_from('<qqQII',table,32*(n%2400))
            for channel,value in enumerate((a,b)):
                if value==0x7fc00035:
                    require(name.endswith('late-nan') and n==479999 and channel==1, 'SOLE_NONFINITE')
                    nonfinite += 1
                    continue
                e = 0 if name.endswith('mixed-signed-zero') else source_error(value,(lo,hi))
                if e>maximum:
                    maximum,witness = e,{'index':n,'channel':channel,'word':value,'interval':[lo,hi]}
        require(nonfinite==int(name.endswith('late-nan')), 'CONTROL_NAN_COUNT')
        rel = 'references/control-sources/'+name+'.interleaved.bin'
        store.put(rel,data)
        del data
        key = (48000,2,rel,rel)
        cache[key] = owner_hash(store.root,*key,budget)
        descriptor = {'id':name,'words':960000,'valid_finite':not name.endswith('late-nan'),'nonfinite_count':nonfinite,'maximum_numerator149':maximum,'limit_numerator149':ERROR,'witness':witness,'owner_sha256':cache[key],'selection':[7,480000,480014],'logical':2,'sentinel_frames_each_end':7,'unselected_prefix_words':[0x3d800000,0xbe000000],'unselected_suffix_words':[0x3d800000,0xbe000000],'source_error_replay':'REQUIRED_UNRUN'}
        require(len(json_bytes(descriptor,2048,budget))<=2048, 'CONTROL_DESCRIPTOR_GEOMETRY')
        store.json('control-sources/'+name+'.json',descriptor)
        descriptors.append(descriptor)
    del left,table
    return descriptors


def generation_controls(store,budget):
    # Entire bounded control suite is inside the existing128M fixed-helper slot.
    budget.fixed(32768)
    known = next(iter(store.allowed))
    store.admit(known,0)
    data_pool_gate(0,DATA_CAP,DATA_CAP)
    work_reservation_gate(COMBINED,CAP)
    records = []
    records.append(expect_gate('generation-unlisted-artifact',lambda:store.admit('references/not-listed.bin',0),'EXACT_FILE_SET',{'name':'references/not-listed.bin','listed':False,'seen':False,'count':0}))
    records.append(expect_gate('generation-data-pool-one-below',lambda:data_pool_gate(0,DATA_CAP,DATA_CAP-1),'WHOLE_DATA_POOL',{'used':0,'amount':DATA_CAP,'limit':DATA_CAP-1}))
    records.append(expect_gate('generation-work-one-below',lambda:Budget(RUNNERS,COMBINED-1),'WORK_RESERVATION',{'reserved':COMBINED,'limit':COMBINED-1,'global_cap':CAP}))
    return records


def definition_freeze(repo, output, expected_manifest, prior):
    require(SOURCE_REVIEW_ACCEPTED and INTERVAL_AND_ALL_HELPER_COSTS_ACCEPTED and RETAINED_STAGE_BOUNDS_ACCEPTED, 'SOURCE_HOLD_NO_DEFINITION_GO')
    budget = Budget(prior)
    budget.fixed(32768)  # R3 normal shared-validator/cardinality bodies, inside fixed128M.
    manifest = checked_manifest(repo,expected_manifest,budget)
    original = manifest['original_r2_proof']
    raw = read_small(repo/original['path'],131072,budget)
    require(hashlib.sha256(raw).hexdigest().upper()==original['sha256'], 'ORIGINAL_REVIEWED_RECIPE')
    proof = load_json(raw,budget)
    store = Store(output,manifest,budget)
    gate_controls = generation_controls(store,budget)
    source_copies(repo,store,proof)
    certs,pi_bounds = profiles(store,budget)
    dyadic_sources(store,budget)
    cache = {}
    controls = control_sources(store,budget,cache)
    cases = []
    base_keys = set()
    for case_id in manifest['case_ids']:
        row = case_row(case_id)
        key = (row['rate'],row['logical'],row['left'],row['right'])
        base_keys.add(key)
        if key not in cache:
            cache[key] = owner_hash(output,*key,budget)
        row['owner_sha256'] = cache[key]
        require(len(json_bytes(row,768,budget))<=768, 'CASE_DESCRIPTOR_GEOMETRY')
        cases.append(row)
    require(len(cases)==294 and len(base_keys)==28 and len(cache)==30, 'ALL_REQUESTS_AND_OWNERS')
    require(sum(10*k[0]*k[1] for k in base_keys)==27294000 and sum(14*k[1] for k in base_keys)==616, 'OWNER_KEY_DIMENSIONS')
    store.json('original-case-inventory.json',{'ids':manifest['case_ids'],'cases':cases,'count':294,'controls':manifest['control_ids'],'profiles':certs,'source_offset':7,'prefix_frames':7,'suffix_frames':7,'left_sentinel_word':0x3d800000,'right_sentinel_word':0xbe000000,'owner_hashes_dedup':'30 distinct full original byte streams; mixed-zero control/base stream shared;294 requests retained'})
    store.json('source-error-summary.json',{'status':'GENERATED_REQUIRE_EXACT_INDEPENDENT_REPLAY','profiles':12,'phase_rows':395010,'base_tone_words':7524000,'dyadic_base_words':3762000,'control_words':2880000,'pcm_words':14166000,'finite_pcm_words':14165999,'intentional_nonfinite_words':1,'base_maximum_numerator149':max(x['maximum_numerator149'] for x in certs),'control_maxima':{x['id']:x['maximum_numerator149'] for x in controls},'pi_bounds_Q48':list(pi_bounds),'ledger':budget.finish(),'vendor_invoked':False,'production_admission':False,'gate_controls':gate_controls})
    require(len(store.files)==90, 'FIRST90_OUTPUTS')
    store.json('definition-manifest.json',{'schema':1,'files':dict(store.files),'ledger':budget.finish(),'source_manifest_sha256':expected_manifest.upper(),'candidate':'UNRUN_HOLD','production_admission':False})
    require(len(store.files)==91, 'FIRST91_OUTPUTS')
    store.json('reference-manifest.json',{'schema':1,'files':dict(store.files),'ledger':budget.finish(),'source_manifest_sha256':expected_manifest.upper(),'status':'GENERATED_SOURCE_ONLY_REPLAY_REQUIRED','production_admission':False})
    require(set(store.files)==set(store.allowed) and len(store.files)==92 and store.used<=DATA_CAP, 'COMPLETE_EXACT92')
    for name,item in store.files.items():
        n,h = digest(output/name,budget,item['bytes'])
        require(n==item['bytes'] and h==item['sha256'], 'AFTER_GENERATION_FILE_FENCE')
    checked_manifest(repo,expected_manifest,budget)
    return {'status':'GENERATED_SOURCE_ONLY_REPLAY_REQUIRED','data_root':str(output),'reference_manifest_sha256':store.files['references/reference-manifest.json']['sha256'],'artifact_count':92,'retained_data_bytes':store.used,'ledger':budget.finish(),'source_manifest_sha256':expected_manifest.upper(),'vendor':False,'production_admission':False,'gate_controls':gate_controls}


if __name__ == '__main__':
    require(ENTRY_EXECUTION_ACCEPTED,'SOURCE_HOLD_NO_FREEZE_OR_GENERATION_AUTHORITY')
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--manifest-sha',required=True)
    parser.add_argument('--prior-work',type=int,required=True)
    args = parser.parse_args()
    result = definition_freeze(args.repo.absolute(),args.output.absolute(),args.manifest_sha,args.prior_work)
    print(json.dumps(result,sort_keys=True,separators=(',',':'),allow_nan=False))
