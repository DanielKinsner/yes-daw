"""Independent file-data/source arithmetic; no target runtime or helper call."""
from pathlib import Path
import hashlib
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
BASE = ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1'
M = json.loads((BASE/'source-manifest.json').read_bytes())
INVENTORY = ROOT/'build-ci/20261105-managed-file-type-inventory-source-r1/execution-r2/inventory.json'
if hashlib.sha256(INVENTORY.read_bytes()).hexdigest().upper() != 'B50DB68E7E10D67E12D3380FCD5D528C02F80F83155F3593E8C88392227C37FD':
    raise ValueError('compiled branch inventory pin changed')
ENUM_INVENTORY = ROOT/'build-ci/20261105-managed-file-type-inventory-source-r2/execution-r1/inventory.json'
if hashlib.sha256(ENUM_INVENTORY.read_bytes()).hexdigest().upper() != 'EA5A5F25A40F2017BD7BE223104C210887B4A3203281FA6E16445E823C6FBF80':
    raise ValueError('compiled enumerator inventory pin changed')

def a8(n): return (n+7)&~7
def arr(n, stride): return a8(24+n*stride)
def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]

HROOT = ROOT/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources'
prime_source = (HROOT/'HashHelpers.cs').read_text()
prime_span = prime_source.split('internal static ReadOnlySpan<int> Primes =>',1)[1].split('];',1)[0]
PRIMES = [int(s) for s in re.findall(r'\b\d+\b',prime_span)]
def prime(n):
    return next(x for x in PRIMES if x >= n)

def dictionary_capacities(n):
    caps=[]
    c=0
    while c<n:
        c=prime(0 if c==0 else 2*c)
        caps.append(c)
    return caps

def list_capacities(n):
    caps=[]
    c=0
    while c<n:
        c=4 if c==0 else 2*c
        caps.append(c)
    return caps

LOAD = f32(0.72)
def hashtable_capacities(n):
    # C# capacity/_loadFactor is binary32 division before assignment to double.
    # Source data here has maximum55 fields; these small quotients are separate
    # arithmetic models, not queried private Hashtable capacity observations.
    raw=f32(f32(n)/LOAD)
    c=3 if raw<=3 else prime(int(raw))
    caps=[c]
    count=0
    while count<n:
        if count>=int(f32(LOAD*c)):
            c=prime(2*c)
            caps.append(c)
        count+=1
    return caps

def parse_graph(value):
    totals=dict(jobject=0,jarray=0,jproperty_and_list=0,jvalue=0,
                jcontainer_lists=0,jobject_dictionaries=0,
                parsed_ordered_maps=0,parsed_arrays=0,strings=0,
                scalar_box_upper=0,conversion_enumerators=0)
    counts=dict(maps=0,arrays=0,fields=0,scalars=0,integer_over_Int64=0)
    def visit(v):
        if isinstance(v,dict):
            n=len(v)
            if n>100:
                raise ValueError('fixed-map collision cleanup exclusion no longer holds')
            counts['maps']+=1; counts['fields']+=n
            # Actual generated JObject iterator has int state, a two-reference
            # KeyValuePair, this and one IEnumerator reference: upper56. Its
            # inner List<JToken>.Enumerator is a boxed24-byte value:40.
            totals['conversion_enumerators']+=56+40
            totals['jobject']+=112
            totals['jproperty_and_list']+=n*(104+24)
            totals['jcontainer_lists']+=32+40+sum(arr(c,8) for c in list_capacities(n))
            # Collection<JToken> and its derived dictionary-reference field are
            # included in32; List object upper40 is deliberately conservative.
            if n:
                totals['jobject_dictionaries']+=80+sum(arr(c,4)+arr(c,24) for c in dictionary_capacities(n))
            hc=hashtable_capacities(n)
            # OrderedHashtable object80 includes measured Hashtable fixed72
            # plus its one reference. Its inherited base buckets are unused.
            # OrderedDictionary upper64 exists even when empty. For n>0 its
            # own Hashtable object72, ArrayList upper40, actual initial n-cell
            # object array and n boxed two-reference DictionaryEntries follow.
            parsed=80+arr(hc[0],24)+64
            if n:
                parsed+=72+sum(arr(c,24) for c in hc)+40+arr(n,8)+32*n
            totals['parsed_ordered_maps']+=parsed
            for k,x in v.items():
                totals['strings']+=a8(22+2*len(k.encode('utf-16-le'))//2)
                visit(x)
        elif isinstance(v,list):
            counts['arrays']+=1
            totals['conversion_enumerators']+=40
            totals['jarray']+=96
            totals['jcontainer_lists']+=40+sum(arr(c,8) for c in list_capacities(len(v)))
            totals['parsed_arrays']+=arr(len(v),8)
            for x in v: visit(x)
        else:
            counts['scalars']+=1
            totals['jvalue']+=64
            if isinstance(v,str):
                totals['strings']+=a8(22+2*len(v.encode('utf-16-le'))//2)
            elif v is not None:
                if not isinstance(v,(int,bool)):
                    raise ValueError('fixed source scalar outside admitted integer JSON')
                if not isinstance(v,bool) and not (-2**63<=v<=2**63-1):
                    counts['integer_over_Int64']+=1
                    raise ValueError('BigInteger branch needs separate source equation')
                # At most one scalar object handed off by reader and one by
                # JTokenWriter. Actual primitive caches can reduce requests.
                totals['scalar_box_upper']+=2*24
    visit(value)
    return dict(counts=counts, families=totals, subtotal=sum(totals.values()),
                omitted_not_zero=['reader/writer instance and JsonPosition stack owners',
                                  'serializer/settings/resolver source-created controlled owners where contract includes them',
                                  'all raw bytes/decoded inputs/reader and StringBuffer arrays',
                                  'opaque general CLR heap remains unqualified, not zero'])

rows=[]
for name,path in [('bootstrap',BASE/'source-manifest.json'),('predecessor',ROOT/M['predecessor_bundle']['path'])]:
    rows.append(dict(name=name,path=path.relative_to(ROOT).as_posix(),
                     sha256=hashlib.sha256(path.read_bytes()).hexdigest().upper(),
                     graph=parse_graph(json.loads(path.read_bytes()))))
data=dict(schema='R11_FIXED_PARSED_GRAPH_SOURCE_UPPER_R1',
          status='CONDITIONAL_PARTIAL_SOURCE_EQUATIONS_NO_GLOBAL_FIT', inputs=rows,
          layout_rule='Pinned Windows x64 CoreCLR ordinary AutoLayout only: every primitive/reference field charged an8-byte slot, value structs recursively at their conservative field-slot widths, inherited fields retained, object header16 and round8 with minimum24. Exact compiled instance-field metadata selects JContainer event fields. This is an upper from HandleAutoLayout and ordinary base-size source, not an offset/runtime-size measurement. Reference-class normal fields have alignment<=8; no explicit layout/SIMD/COM/custom size on these types. The publicly measured array/string/Dictionary/Hashtable coefficients are separately used. All source-created old arrays retained.',
          class_upper_bytes={'JToken':48,'JContainer':88,'JObject':112,'JArray':96,'JProperty':104,'JPropertyList':24,'JValue':64,'JPropertyKeyedCollection_including_Collection_base':32,'List_or_ArrayList':40,'OrderedDictionary':64,'OrderedHashtable':80,'boxed_DictionaryEntry':32},
          hashtable_examples=[dict(entries=n,inserted_bucket_capacities=hashtable_capacities(n)) for n in [0,2,3,4,5,8,55]],
          exact_metadata_inventory_sha256='B50DB68E7E10D67E12D3380FCD5D528C02F80F83155F3593E8C88392227C37FD',
          exact_enumerator_inventory_sha256='EA5A5F25A40F2017BD7BE223104C210887B4A3203281FA6E16445E823C6FBF80',
          representation_limits='Not finite measured class sizes, no allocation delta/peak/live/reclaim observation. Token strings may alias via property-name tables but one fresh string per token safely bounds this family. No duplicate properties in these two pinned data artifacts. Arbitrary dependency/report and failure prefix need separate graph equations. Cannot transfer after changing final canonical union/manifest. All unlisted owners remain named, not zero.',
          no_authority=['No target execution', 'No binding/prices/fit/GO'])
(OUT/'fixed-graphs.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(fixed=[dict(name=r['name'],subtotal=r['graph']['subtotal'],families=r['graph']['families']) for r in rows]),indent=2))
