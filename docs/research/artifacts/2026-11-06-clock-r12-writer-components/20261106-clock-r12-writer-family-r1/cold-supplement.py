"""Supplement the frozen first writer component; file/source/integer only."""
from pathlib import Path
import hashlib
import json
import urllib.request

here = Path(__file__).resolve().parent
root = here.parents[1]
assert not (here / 'cold-supplement.json').exists()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
url = 'https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Linq/src/System/Linq/ToCollection.cs'
target = here / 'primary/ToCollection.cs'
assert not target.exists()
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
with opener.open(urllib.request.Request(url, headers={'User-Agent': 'YES-DAW-fixed-source-review'}), timeout=30) as response:
    data = response.read(1048577)
assert len(data) <= 1048576
assert 'private const int DefaultInternalSetCapacity = 7;' in data.decode('utf-8-sig')
target.write_bytes(data)
origin = dict(file='ToCollection.cs', url=url, bytes=len(data), sha256=digest(data))

def a8(n):
    return (n + 7) // 8 * 8

def array(n, width):
    return a8(24 + n * width)

java = (root / 'build-ci/20261105-clock-oracle-json-primary-sources/newtonsoft-JavaScriptUtils.cs').read_text()
assert 'escapeChars.Union(new[]' in java and 'new bool[128]' in java
assert 'new HashSet<TSource>(DefaultInternalSetCapacity, _comparer)' in (here / 'primary/Union.cs').read_text()
assert 'private readonly Func<TKey, TValue> _creator;' in (here / 'primary/ThreadSafeStore.cs').read_text()
values = set([ord(c) for c in '\n\r\t\\\f\b'] + list(range(32)))
assert len(values) == 33
sizes = [len(values | {ord(c) for c in suffix}) for suffix in ["'", '"', '"\'<> &'.replace(' ', '')]]
assert sizes == [34, 34, 38]
hash_primes = [7, 17, 37, 89]
routes = []
for count in sizes:
    capacities = [n for n in hash_primes if n < count]
    capacities.append(next(n for n in hash_primes if n >= count))
    assert capacities == ([7, 17, 37] if count == 34 else [7, 17, 37, 89])
    routes.append(dict(distinct_chars=count, capacities=capacities,
        bucket_and_entry_array_upper=sum(array(n, 4) + array(n, 24) for n in capacities)))
parts = dict(JavaScriptUtils_three_UnionIterator2_field_slot_upper=3 * 80,
             JavaScriptUtils_three_HashSet_char_field_slot_upper=3 * 80,
             JavaScriptUtils_all_HashSet_backings_entry_stride_upper24=sum(r['bucket_and_entry_array_upper'] for r in routes),
             JavaScriptUtils_three_boxed_List_char_enumerators_upper=3 * 48,
             JavaScriptUtils_three_array_enumerator_field_slot_upper=3 * 48,
             char_EqualityComparer_object_field_slot_upper=24,
             List_char_and_List_state_array_shared_empty_arrays=2 * array(0, 8),
             Encoding_and_UTF8_shared_default_instances_plus_replaced_fallbacks=2 * 120,
             Shared_encoder_decoder_replacement_and_exception_fallback_objects=4 * 24,
             TextWriter_Null_object_field_slot_upper=40,
             EnumUtils_CamelCaseNamingStrategy_field_slot_upper=40,
             EnumUtils_ThreadSafeStore_field_slot_upper=32,
             EnumUtils_creator_delegate_field_slot_upper=64,
             EnumUtils_eighteen_boxed_underlying_JsonToken_values=18 * 24,
             EnumUtils_eighteen_empty_attribute_return_arrays_upper=18 * array(0, 8))
base = json.loads((here / 'writer-component.json').read_bytes())
known = base['known_once_F_Bcold_subtotal'] + sum(parts.values())
remaining = [
    dict(id='F_Bcold_EnumUtilsStore', source='ThreadSafeStore.cs:constructor/Get',
         exact_obligation='Selected HAVE_CONCURRENT_DICTIONARY build: one default ConcurrentDictionary<StructMultiKey<Type,NamingStrategy>,EnumInfo>, locks/buckets/count arrays and one JsonToken entry; default concurrency/SDK field and growth profile must be bounded. Alternate dictionary branch is not silently substituted.'),
    dict(id='F_Bcold_EnumReflection', source='EnumUtils.cs:InitializeValuesAndNames18',
         exact_obligation='Selected GetNames18, resolvedNames18, values18, EnumInfo and boxed underlying18 are counted. Shared reflection enum-name strings/FieldInfo/cache outputs and at most18 Cast plus18 Select iterator instances from EnumMemberAttribute-empty queries need explicit selected representation or an expressly scoped general-CLR-metadata exclusion; not0. No custom naming strategy or JsonToken field attributes are admitted by the pinned source.'),
    dict(id='B_SerializerRuntime', source='powershell-JsonObject.cs:ConvertToJson; JsonConvert.cs:SerializeObjectInternal',
         exact_obligation='JsonSerializerSettings/JsonSerializer/InternalWriter, their serialization traversal enumerators/serialize-stack and contract-cache owners must join B/K/F once. The selected writer-buffer subtotal is not the full codec allocation graph.'),
    dict(id='F_Bcold_Metadata', source='selected official source/installed metadata association',
         exact_obligation='Runtime type/comparer/delegate/metadata caches and literal storage retain their separately declared general CLR/SDK qualification; no total heap, RSS, cold loader or actual allocation-size claim follows from these field-slot source uppers.')]
result = dict(schema='R12_WRITER_COLD_SELECTED_COMPONENT_SUPPLEMENT_R1',
    status='FINITE_SELECTED_COLD_SOURCE_COMPONENT_WITH_PRECISE_REMAINING_CODEC_BOUNDARIES',
    base_component_sha256=digest((here / 'writer-component.json').read_bytes()),
    shared_once_only=True, HashSet_char_routes=routes, additional_known_once_parts=parts,
    additional_known_once_subtotal=sum(parts.values()), known_once_F_Bcold_subtotal=known,
    remaining=remaining, default_capacity_primary=origin,
    source_field_slot_condition='Selected x64 object header16, each scalar/reference field occupies no more than8 bytes; JsonPosition<=32, HashSet<char>.Entry<=24. Source association/metadata conditional, not actual runtime object sizes. No destructor/Dispose/Return/Clear/GC credit.',
    no_target_execution=True)
paths = [here / 'writer-component.json', here / 'derive.py', Path(__file__).resolve()] + sorted(p for p in (here / 'primary').iterdir() if p.is_file())
result['inputs'] = [dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in paths]
(here / 'cold-supplement.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(additional_known_once_subtotal=sum(parts.values()), known_once_F_Bcold_subtotal=known, remaining=[r['id'] for r in remaining]), indent=2))
