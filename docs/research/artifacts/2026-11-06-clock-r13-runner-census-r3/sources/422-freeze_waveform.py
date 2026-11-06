"""Authored source only. No imports, generation or Freeze have been run by author."""
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
CAP = 1 << 34
U = 1 << 48
ERROR = 1 << 129
BASE = Path(__file__).resolve().parent


def require(value, message):
    if not value:
        raise RuntimeError(message)


class Budget:
    def __init__(self, maximum=CAP):
        self.maximum, self.used, self.library_calls = maximum, 0, 0
    def spend(self, n):
        require(0 <= n <= self.maximum-self.used, 'DEFINITION_WORK_REFUSED')
        self.used += n


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


def analytic_interval(rate, frequency, phase_p, phase_q, n, pi_bounds):
    require(rate in (44100, 48000, 96000) and frequency in (220, 997) and phase_p in (0, 1) and phase_q in (1, 7), 'TONE_PROFILE')
    a, b = 2*frequency*n*phase_q+phase_p*rate, rate*phase_q
    require(a < 1 << 63 and b < 1 << 32, 'EXACT_PHASE_WIDTH')
    k = (4*a+b)//(2*b)
    h = 2*a-k*b
    require(abs(h)*2 <= b, 'QUADRANT_REDUCTION')
    ends = [x*h for x in pi_bounds]
    theta = interval(min(ends)//(2*b), ceildiv(max(ends), 2*b))
    square = mul(theta, theta)
    sin_poly, cos_poly = coefficient(12, True), coefficient(12, False)
    for j in range(11, -1, -1):
        sin_poly = add(coefficient(j, True), mul(square, sin_poly))
        cos_poly = add(coefficient(j, False), mul(square, cos_poly))
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


class Store:
    def __init__(self, root, proof, budget):
        self.root, self.budget, self.used = root, budget, 0
        self.allowed = {x['path']:x['bytes_cap'] for x in proof['literal_reference_artifacts']}
        self.allowed.update({x['path']:x['bytes_cap'] for x in proof['generated_source_preparation']['literal_generated_artifacts']})
        self.files = {}
    def put(self, name, data):
        if not name.startswith(('references/','generated/')):
            name = 'references/'+name
        require(name in self.allowed and len(data) <= self.allowed[name], 'UNLISTED_OR_OVERSIZED_ARTIFACT:'+name)
        require(len(data) <= (256+8)*(1 << 20)-self.used, 'RETAINED_REFERENCE_SOURCE_CAP')
        path = (self.root/name).resolve()
        require(path.is_relative_to(self.root.resolve()) and not path.exists(), 'NO_OVERWRITE_OR_ESCAPE')
        self.budget.spend(64*len(data))
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as stream:
            stream.write(data)
        self.used += len(data)
        self.files[name] = {'bytes':len(data), 'sha256':hashlib.sha256(data).hexdigest()}
    def json(self, name, value):
        self.put(name, (json.dumps(value, indent=2, sort_keys=True)+'\n').encode())


def checked_manifest(repo):
    manifest = json.loads((BASE/'authored-manifest.json').read_text())
    for item in manifest['sources']:
        path = BASE/item['path']
        require(hashlib.sha256(path.read_bytes()).hexdigest().upper() == item['sha256'], 'AUTHORED_SOURCE_CHANGED')
    for item in manifest['preserved_pins']:
        require(hashlib.sha256((repo/item['path']).read_bytes()).hexdigest().upper() == item['sha256'], 'PRESERVED_INPUT_CHANGED:'+item['path'])
    return manifest


def replacements(data, edits):
    current = data
    for old, new in edits:
        require(current.count(old) == 1, 'SOURCE_ANCHOR_NOT_UNIQUE')
        current = current.replace(old, new, 1)
    return current


def patch_bytes(edits):
    return (json.dumps([{'old':a.decode(), 'new':b.decode()} for a,b in edits], indent=2)+'\n').encode()


def source_copies(repo, store, proof):
    initial = {k:(repo/k).read_bytes() for k in proof['generated_source_preparation']['production_inputs']}
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
            products += [(prefix+variant+'/engine/'+name, final), (prefix+'patches/'+variant+'.'+kind+'.isolation.patch', patch_bytes(isolation)), (prefix+'patches/'+variant+'.'+kind+'.isolation.inverse.patch', patch_bytes([(b,a) for a,b in reversed(isolation)]))]
            if fault:
                products += [(prefix+'patches/'+variant+'.'+kind+'.fault.patch', patch_bytes(fault)), (prefix+'patches/'+variant+'.'+kind+'.fault.inverse.patch', patch_bytes([(b,a) for a,b in reversed(fault)]))]
            visits += len(original)*8+len(final)*4+sum(len(x)+len(y) for x,y in isolation+fault)
            lineage.append({'variant':variant, 'kind':kind, 'original_sha256':hashlib.sha256(original).hexdigest(), 'isolated_sha256':hashlib.sha256(isolated).hexdigest(), 'final_sha256':hashlib.sha256(final).hexdigest(), 'fault_inverse_exact':True, 'isolation_inverse_exact':True})
    require(visits <= 4161536, 'SOURCE_COPY_VISITED_BYTE_CAP')
    store.budget.spend(512*visits+(16 << 20))
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


def definition_freeze(repo, output):
    require(SOURCE_REVIEW_ACCEPTED and INTERVAL_AND_ALL_HELPER_COSTS_ACCEPTED and RETAINED_STAGE_BOUNDS_ACCEPTED, 'SOURCE_HOLD_DEFINITION_INTERVAL_HASH_HELPERS_AND_STORAGE')
    manifest = checked_manifest(repo)
    proof = json.loads((repo/manifest['accepted_correction_proof']['path']).read_text())
    require(hashlib.sha256((repo/manifest['accepted_correction_proof']['path']).read_bytes()).hexdigest().upper() == manifest['accepted_correction_proof']['sha256'], 'CORRECTION_BINDING')
    require(not output.exists(), 'NEW_OUTPUT_REQUIRED')
    budget = Budget();budget.spend(128000000);output.mkdir(parents=True);store = Store(output, proof, budget)
    source_copies(repo, store, proof)
    pi_bounds = machin();profiles = [];profile_id = 0
    for rate in [44100,48000,96000]:
        for frequency in [220,997]:
            for pp,qq,label in [(0,1,'p0'),(1,7,'p1q7')]:
                period = rate//math.gcd(frequency,rate);key = f'r{rate}-f{frequency}-{label}'
                table = bytearray(32*period)
                for n in range(period):
                    budget.spend(8192)
                    bounds = analytic_interval(rate,frequency,pp,qq,n,pi_bounds)
                    struct.pack_into('<qqQII',table,32*n,bounds[0],bounds[1],n,profile_id,0)
                data = bytearray(4*10*rate);maximum = -1;witness = None
                for n in range(10*rate):
                    budget.spend(256);budget.library_calls += 1
                    # Preserve original libm-derived float words; the interval is a separate checker.
                    value = 0.2*math.sin(2*math.pi*frequency*n/rate+(math.pi/7 if pp else 0.0))
                    struct.pack_into('<f',data,4*n,value);word = struct.unpack_from('<I',data,4*n)[0]
                    lo,hi,_,_,_ = struct.unpack_from('<qqQII',table,32*(n%period));error = source_error(word,(lo,hi))
                    if error > maximum:
                        maximum,witness = error, {'index':n,'word':word,'interval':[lo,hi]}
                store.put('tone/'+key+'.bin',data);store.put('phase-intervals/'+key+'.bin',table)
                certificate = {'rate':rate,'frequency':frequency,'phase_p':pp,'phase_q':qq,'period':period,'profile_id':profile_id,'words':10*rate,'maximum_numerator149':maximum,'limit_numerator149':ERROR,'witness':witness,'tone_sha256':hashlib.sha256(data).hexdigest(),'phase_sha256':hashlib.sha256(table).hexdigest(),'pi_bounds_Q48':list(pi_bounds),'independent_replay':'REQUIRED_UNRUN'}
                store.json('source-certificates/'+key+'.json',certificate);profiles.append(certificate);profile_id += 1
        budget.spend(128*20*rate)
        store.put(f'zero/r{rate}.bin',bytes(4*10*rate));store.put(f'burst/r{rate}.bin',burst(rate))
    left = bytearray((output/'references/tone/r48000-f220-p1q7.bin').read_bytes());pair = bytearray(8*480000)
    for n in range(480000):
        word = struct.unpack_from('<I',left,4*n)[0];struct.pack_into('<II',pair,8*n,word,word)
    controls = []
    for name in ['r48000-equivalent-last-ulp','r48000-early-distinct-late-nan','r48000-mixed-signed-zero']:
        data = bytearray(pair)
        if name.endswith('last-ulp'):
            word = struct.unpack_from('<I',data,len(data)-4)[0];struct.pack_into('<I',data,len(data)-4,word^1)
        elif name.endswith('late-nan'):
            word = struct.unpack_from('<I',data,4)[0];struct.pack_into('<I',data,4,word^1);struct.pack_into('<I',data,len(data)-4,0x7fc00035)
        else:
            for n in range(480000):
                struct.pack_into('<II',data,8*n,0x80000000 if n&1 else 0,0 if n&1 else 0x80000000)
        budget.spend(256*960000)
        store.put('control-sources/'+name+'.interleaved.bin',data)
        prefix=struct.pack('<II',0x3d800000,0xbe000000)*7;budget.spend(64*(len(prefix)*2+len(data)));owner_digest=hashlib.sha256(prefix+data+prefix).hexdigest()
        descriptor={'owner_sha256':owner_digest,'id':name,'words':960000,'valid_finite':not name.endswith('late-nan'),'source_error_replay':'REQUIRED' if not name.endswith('mixed-signed-zero') else 'EXACT_DYADIC','unselected_prefix_words':[0x3d800000,0xbe000000],'unselected_suffix_words':[0x3d800000,0xbe000000],'sentinel_frames_each_end':7}
        store.json('control-sources/'+name+'.json',descriptor);controls.append(descriptor)
    cases=[];source_hashes={}
    for case_id in proof['case_ids_fixed_before_authoring']:
        rate=int(re.search(r'-r(\d+)',case_id).group(1));map_index=int(re.search(r'-map(\d+)',case_id).group(1))
        logical=2 if 'equivalent' in case_id or 'distinct' in case_id or 'mixed-signed-zero' in case_id else 1
        kind='bake' if case_id.startswith('bake-') else 'reverse' if case_id.startswith('reverse-') else 'zero' if case_id.startswith('zero-') else 'burst' if case_id.startswith('burst-') else 'tone'
        frequencies=[0,0] if kind in ('zero','burst','reverse') else [int(re.search(r'-f(\d+)',case_id).group(1)),0]
        if kind in ('tone','bake'):
            left=f'references/tone/r{rate}-f{frequencies[0]}-'+('p0' if logical==1 or 'distinct' in case_id else 'p1q7')+'.bin'
            frequencies[1]=997 if 'distinct' in case_id else frequencies[0]
            right=f'references/tone/r{rate}-f{frequencies[1]}-p1q7.bin' if logical==2 else left
        else:
            left=right=f'references/{"zero" if kind=="zero" else "burst"}/r{rate}.bin'
        if 'mixed-signed-zero' in case_id:
            left=right='references/control-sources/r48000-mixed-signed-zero.interleaved.bin'
        key=(rate,logical,left,right)
        if key not in source_hashes:
            data_left=(output/left).read_bytes();data_right=(output/right).read_bytes()
            interleaved=left.endswith('.interleaved.bin')
            digest=hashlib.sha256();prefix=b''.join(struct.pack('<I',0x3d800000 if c==0 else 0xbe000000) for c in range(logical))
            digest.update(prefix*7)
            for j in range(10*rate):
                budget.spend(64*4*logical)
                digest.update(data_left[j*4*logical:(j+1)*4*logical] if interleaved else data_left[4*j:4*j+4]+(data_right[4*j:4*j+4] if logical==2 else b''))
            digest.update(prefix*7);source_hashes[key]=digest.hexdigest()
        shape=next((i for i,n in enumerate(['Linear','EqualPower','SCurve','Log']) if '-'+n+'-' in case_id),0)
        curve=-1 if '-minus1-' in case_id else 1 if '-plus1-' in case_id else 0
        fade_in=3*153600//4 if 'overlap3L4' in case_id else 2*153600+1 if 'overlong2Lplus1-L2' in case_id else 0
        fade_out=fade_in if 'overlap3L4' in case_id else 153600//2 if 'overlong2Lplus1-L2' in case_id else 0
        cases.append({'id':case_id,'mode':kind,'rate':rate,'map':map_index,'logical':logical,'left':left,'right':right,'owner_sha256':source_hashes[key],'frequencies':frequencies,'shape':shape,'curve':curve,'fade_in':fade_in,'fade_out':fade_out,'dsp_k':None,'queried_layout':'HOLD','context':'HOLD','inverse':'references/native-inverse/r48000-map5.bin'})
    store.json('original-case-inventory.json', {'ids':proof['case_ids_fixed_before_authoring'],'cases':cases,'count':294,'controls':proof['original_matrix_and_controls']['control_ids'],'profiles':profiles,'source_offset':7,'prefix_frames':7,'suffix_frames':7,'left_sentinel_word':0x3d800000,'right_sentinel_word':0xbe000000,'owner_hashes_dedup':'exact same frozen source-file identities, rate/channel/sentinels; separate requests retained'})
    store.json('source-error-summary.json', {'status':'FROZEN_SOURCE_INTERVAL_CHECKS_REQUIRE_INDEPENDENT_REPLAY','profiles':12,'base_tone_words':7524000,'pi_bounds_Q48':list(pi_bounds),'budget_used':budget.used,'library_calls':budget.library_calls,'vendor_invoked':False,'replay':'UNRUN','source_error_control_full_replay':'UNRUN_REQUIRED'})
    store.json('definition-manifest.json', {'files':store.files,'work':budget.used,'retained_bytes':store.used,'oracle':'UNRUN','candidate':'UNRUN','production_admission':False})
    return {'files':store.files,'work':budget.used,'bytes':store.used}


if __name__ == '__main__':
    require(False, 'SOURCE_HOLD_NO_FREEZE_OR_GENERATION_AUTHORITY')
    parser = argparse.ArgumentParser();parser.add_argument('--repo',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args();print(json.dumps(definition_freeze(args.repo.resolve(),args.output.resolve()),sort_keys=True))
