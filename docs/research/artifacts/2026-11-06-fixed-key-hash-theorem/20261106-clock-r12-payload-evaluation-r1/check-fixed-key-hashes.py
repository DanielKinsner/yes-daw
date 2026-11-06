"""Independent Python uint arithmetic over fixed public SOURCE/DATA only.

No runtime GetHashCode, target SDK helper, parser, reflection or execution.
"""
from pathlib import Path
from hashlib import sha256
from collections import Counter
import json,re
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def read(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
src=ROOT/'build-ci/20261106-comparer-encoding-source-r1/String.Comparison.cs'
assert digest(src)=='332E2A21449AD65F2368D672A116246EADF3842DFCB45F35A713D912C51731C5'
geompath=ROOT/'build-ci/20261105-clock-oracle-r12-path-geometry-r1/geometry.json'
assert digest(geompath)=='BE95DC06AB85E7BF0A706656B08A6C2C8B5581C4AE3376BFE7AB1D98CADBAD9F'
g=read(geompath);rows=g['rows'];bs=chr(92)
base=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
m=read(base/'source-manifest.json')
prsrc=(ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs').read_text()
pr=[int(n) for n in re.findall(r'\b\d+\b',prsrc.split('internal static ReadOnlySpan<int> Primes =>',1)[1].split('];',1)[0])]
def prime(n):return next(x for x in pr if x>=n)
def growth(n):
 c=0;out=[]
 while c<n:c=prime(2*c);out.append(c)
 return out
def raw(r):
 p=r['name']
 return g['repo']+bs+p.replace('/',bs) if r['kind']=='input' and not(len(p)>2 and p[1]==':') else p
initial_aliases=set()
for r in rows:
 if r['kind']!='input':initial_aliases.update((r['name'],r['name'].replace('/',bs)))
before_aliases=set(initial_aliases);before_pool=set()
for r in rows[:1225]:
 before_aliases.update((raw(r),r['canonical']));before_pool.update(r['parent_chain'])
aliases=set(initial_aliases)
for r in rows:aliases.update((raw(r),r['canonical']))
pool=set(g['all_known_strings_sorted'])
assert (len(initial_aliases),len(before_aliases),len(aliases),len(before_pool),len(pool))==(134,2211,2211,1380,1458)

MASK=(1<<32)-1
def rot5(v):return ((v<<5)|(v>>27))&MASK
def step(h,w):return ((rot5(h)+h)^w)&MASK
def final(h1,h2):return (h1+h2*1566083941)&MASK
def instance_hash(s,ignore=False):
 assert all(ord(c)<128 for c in s)
 # Selected x64 little-endian string has a terminating UTF16 NUL. The
 # instance algorithm's3/1-character tails read that NUL, not arbitrary data.
 units=list(map(ord,s))+[0]
 if len(units)%2:units.append(0)
 words=[units[i]|(units[i+1]<<16) for i in range(0,len(units),2)]
 h1=h2=(5381<<16)+5381;n=len(s);i=0;norm=0x00200020 if ignore else 0
 while n>2:
  n-=4;h1=step(h1,words[i]|norm);h2=step(h2,words[i+1]|norm);i+=2
 if n>0:h2=step(h2,words[i]|norm)
 return final(h1,h2)
def span_hash(s,ignore=False):
 # Independent span-style tail implementation, equivalent for the same
 # UTF16 string under the source's explicit null-padding rule.
 u=list(map(ord,s));h1=h2=(5381<<16)+5381;at=0;norm=0x00200020 if ignore else 0
 def word(a,b=0):return (a|(b<<16))|norm
 while len(u)-at>=4:
  h1=step(h1,word(u[at],u[at+1]));h2=step(h2,word(u[at+2],u[at+3]));at+=4
 remain=u[at:]
 if len(remain)==3:h1=step(h1,word(remain[0],remain[1]));h2=step(h2,word(remain[2]))
 elif len(remain)==2:h2=step(h2,word(remain[0],remain[1]))
 elif len(remain)==1:h2=step(h2,word(remain[0]))
 return final(h1,h2)

sets=dict(aliases=(aliases,False),strings=(pool,False),parent_strings=(pool,False),
 catalogue_inputs=({r['path'] for r in m['inputs']},False),
 main_input_index=({r['canonical'] for r in rows[:1220]},True),
 main_include_index=({r['canonical'] for r in rows[:1220]}|{g['base']+bs+'source-costs.h'},True))
checks=[];tables=[]
for name,(keys,ignore) in sets.items():
 assert all('~' not in s and all(32<=ord(c)<=126 for c in s) for s in keys)
 equal=all(instance_hash(s,ignore)==span_hash(s,ignore) for s in keys)
 checks.append(dict(check='instance_vs_independent_span:'+name,passed=equal));assert equal
 caps=growth(len(keys));capacity_rows=[]
 for c in caps:
  # For capacity<=89 the actual table has at most89 entries; impossible to
  # exceed collisionCount100. For every later capacity, even the ENTIRE key
  # universe has bucket occupancy<101. Every prefix/subset/order is bounded.
  maximum=max(Counter(instance_hash(s,ignore)%c for s in keys).values())
  effective=c if c<=89 else maximum
  assert effective<101,(name,c,maximum)
  capacity_rows.append(dict(capacity=c,entire_universe_maximum_bucket=maximum,
    source_chain_upper=c if c<=89 else maximum,
    basis='actual Count<=capacity<=89' if c<=89 else 'entire fixed key universe, independent of insertion order or subset'))
 tables.append(dict(name=name,keys=len(keys),ordinal_ignore_case=ignore,growth_capacities=caps,
  rows=capacity_rows,maximum_relevant_full_universe_bucket=max(r['entire_universe_maximum_bucket'] for r in capacity_rows if r['capacity']>=197)))
assert growth(len(before_aliases))[-1]==growth(len(aliases))[-1]==4049
assert growth(len(before_pool))[-1]==growth(len(pool))[-1]==1931
out=dict(schema='R12_FIXED_DICTIONARY_HASH_CHAIN_SOURCE_THEOREM_R1',status='CONDITIONAL_FIXED_ROOT_SOURCE_PROPOSAL_NO_RUNTIME_HASH_OR_NATIVE_EXECUTION',
 source_pins=[dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=digest(p)) for p in [src,geompath,base/'source-manifest.json',ROOT/'build-ci/20261106-comparer-encoding-source-r1/NonRandomizedStringEqualityComparer.cs',ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Dictionary.cs']],
 arithmetic='Unchecked uint32 rotate-left5/add/xor and final multiplier1566083941; UTF16LE/NUL-terminated instance strings, andASCII-only ignore-case OR0x00200020. Dictionary uint hash modulocapacity follows selected GetBucket/FastMod semantics, not Runtime.GetHashCode.',
 prefix=dict(initial_aliases=134,precreated_aliases=2211,final_aliases=2211,precreated_strings=1380,final_strings=1458,
  precreated_alias_capacity=4049,final_alias_capacity=4049,precreated_string_parent_capacity=1931,final_string_parent_capacity=1931),
 tables=tables,checks=checks,
 theorem='Under the exact P root/provider/normalization/key-universe premise, none of these six Dictionaries can ever reach collisionCount>100, for any insertion order/prefix/subset. Their comparer-transition branch, newly allocated randomized comparer and same-capacity rehash arrays are unreachable. After the complete precreated source/input fence aliases/strings/parent_strings also never need capacity growth; unknown paths never insert. Thus post-created failure retries cannot repeat their Resize allocations.',
 failure_scope='Failures during catalogue/input/include creation or precreated first population terminate before taskCreated and have no inventory/footer retry. No2-retry/3-region assumption is used; SaveFailureInventory inner-catch continuation and all finally/footer paths remain admitted.',
 qualifications=['Exact selected P geometry/argv/provider/normalization/all known path identity and case spellings; future union/root must be regenerated.',
  'Selected little-endian runtime sources/Dictionary/NonRandomizedStringEqualityComparer branch association; no binary/source-equivalence, measured capacity or private field claim.',
  'General J report/FileSet unknown-key dictionaries still retain their possible rehash/comparer/failure behavior; this theorem does not cover them.',
  'No source change, runtime execution,32MiB fit,NOFIT or GO.'])
(HERE/'fixed-key-hash-chains.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(prefix=out['prefix'],tables=[{k:r[k] for k in ['name','keys','growth_capacities','maximum_relevant_full_universe_bucket']} for r in tables],checks=len(checks)),indent=2))
