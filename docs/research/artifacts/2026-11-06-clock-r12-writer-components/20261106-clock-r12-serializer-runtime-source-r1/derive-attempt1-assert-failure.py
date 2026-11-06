"""Source-sized direct serializer owners and failure paths; no target execution."""
from pathlib import Path
import hashlib
import json
import re

here = Path(__file__).resolve().parent
root = here.parents[1]
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
assert not (here / 'runtime-component.json').exists()
settings = (here / 'primary/JsonSerializerSettings.cs').read_text()
serializer = (here / 'primary/JsonSerializer.cs').read_text()
internal = (here / 'primary/JsonSerializerInternalWriter.cs').read_text()
base = (here / 'primary/JsonSerializerInternalBase.cs').read_text()
position = (here / 'primary/JsonPosition.cs').read_text()
streaming = (here / 'primary/StreamingContext.cs').read_text()
field_pattern = r'^\s*(?:internal|private|protected)\s+(?!(?:static|const)\b)([^\n;={]+)\s+(_[A-Za-z0-9]+)\s*;'
settings_fields = re.findall(field_pattern, settings, re.M)
serializer_fields = re.findall(field_pattern, serializer, re.M)
settings_auto = re.findall(r'^\s*public\s+([^\n{]+)\s+(\w+)\s*\{\s*get;\s*set;\s*\}', settings, re.M)
assert len(settings_fields) == 24 and len(settings_auto) == 7
assert len(serializer_fields) == 30
assert 'public virtual event EventHandler<ErrorEventArgs>? Error;' in serializer
assert settings_fields[20] == ('StreamingContext?', '_context')
assert serializer_fields[15] == ('StreamingContext', '_context')
assert 'private readonly object? _additionalContext;' in streaming
assert 'private readonly StreamingContextStates _state;' in streaming
assert 'Converters = new List<JsonConverter>();' in settings
assert 'private readonly List<object> _serializeStack = new List<object>();' in internal
assert 'IDictionaryEnumerator e = values.GetEnumerator();' in internal
assert 'DictionaryEntry entry = e.Entry;' in internal
assert 'foreach (object value in values)' in internal
assert 'Serializer.OnError(new ErrorEventArgs(currentObject, errorContext));' in base
assert 'StringBuilder sb = new StringBuilder(capacity);' in position

def a8(n):
    return (n + 7) // 8 * 8

def array(n, width):
    return a8(24 + n * width)

def string(n):
    return a8(22 + n * 2)

fixed = dict(JsonSerializerSettings_field_slot_upper=16 + 30 * 8 + 24,
             JsonSerializer_field_slot_upper=16 + 30 * 8 + 16,
             JsonSerializerInternalWriter_including_base_upper=16 + 8 * 8,
             Settings_Converters_List_field_slot_upper=40,
             InternalWriter_serializeStack_List_field_slot_upper=40,
             InternalWriter_serializeStack_all_old_backings=sum(array(n, 8) for n in (4, 8, 16, 32)))
assert sum(fixed.values()) == 1288
graphs = dict(marker=dict(maps=1, arrays=0, container_chain=1),
              receipt=dict(maps=58, arrays=23, container_chain=5),
              failure=dict(maps=79, arrays=1, container_chain=3),
              bundle=dict(maps=1294, arrays=2, container_chain=3))
for graph in graphs.values():
    graph['dictionary_traversal_enumerator_box_upper'] = 64 * graph['maps']
    graph['list_traversal_enumerator_box_upper'] = 48 * graph['arrays']
    graph['selected_direct_runtime_upper'] = sum(fixed.values()) + 64 * graph['maps'] + 48 * graph['arrays']

def failure_route(d, k=512):
    # Known fixed ASCII identifier keys contain none of JsonPosition's special
    # characters. BuildPath preallocates enough, so no escaped StringWriter or
    # grown chunks/numeric ToString fallback is required.
    capacity = d * max(k + 5, 22)
    per_path = 48 + array(capacity, 2) + string(capacity)
    calls = d + 1
    return dict(container_chain=d, identifier_key_units_upper=k,
        path_capacity_upper=capacity, BuildPath_calls_upper=calls,
        all_path_builders_arrays_strings_upper=calls * per_path,
        ErrorContext_once_field_slot_upper=64,
        ErrorEventArgs_field_slot_upper=calls * 32,
        boxed_array_error_member_indices_upper=d * 24,
        selected_failure_route_upper=calls * (per_path + 32) + 64 + d * 24)

failure = {name: failure_route(graph['container_chain']) for name, graph in graphs.items()}
# First failed main encode terminates the main try. Only failure inventory and
# receipt footer can then enter codec again; each is separately caught once.
error_tail = 2 * failure['receipt']['selected_failure_route_upper'] + failure['failure']['selected_failure_route_upper']
assert error_tail == 151416
vectors = [dict(marker=0, receipt=0, failure=0, bundle=0),
           dict(marker=1, receipt=2, failure=1, bundle=0),
           dict(marker=1, receipt=2, failure=1, bundle=0),
           dict(marker=1, receipt=3, failure=1, bundle=0),
           dict(marker=1, receipt=4, failure=1, bundle=0),
           dict(marker=1, receipt=4, failure=1, bundle=1)]
phases = [dict(id='P' + str(i), direct_runtime_upper=sum(v[k] * graphs[k]['selected_direct_runtime_upper'] for k in v),
               SDK_serialization_failure_route_upper=0 if i == 0 else error_tail) for i, v in enumerate(vectors)]
inputs = [root / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1',
    root / 'build-ci/20261106-clock-r12-writer-family-r1/source-pins.json',
    root / 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Dictionary.cs',
    root / 'build-ci/20261106-clock-r12-writer-family-r1/primary/List.cs']
inputs += sorted(p for p in (here / 'primary').iterdir() if p.is_file())
result = dict(schema='R12_DIRECT_SERIALIZER_RUNTIME_AND_FAILURE_SOURCE_COMPONENT_R1',
    status='PROPOSED_SOURCE_COMPONENT_COLD_CONTRACT_CACHE_AND_GENERAL_CLR_QUALIFICATION_HOLD',
    fixed_per_encode=fixed, source_field_inventory=dict(settings=settings_fields, settings_auto_properties=settings_auto, serializer=serializer_fields, serializer_event_field='Error'),
    graphs=graphs, graph_count_status='Source-profile inputs from whole phase owner; key inventory and exact chain/count derivation must be pinned before whole numerical acceptance',
    SDK_failure_routes=failure, SDK_global_error_tail_upper=error_tail, phases=phases,
    additional_cold_direct_owner_terms=dict(JsonPosition_SpecialCharacters_char18=array(18, 2), List_JsonConverter_shared_empty_array=array(0, 8)),
    shared_with_K_J_identity_terms=[dict(identity='List<object>.s_emptyArray', upper=array(0, 8)), dict(identity='SZGenericArrayEnumerator<object>.Empty', field_slot_upper=48)],
    R1_buffer_correction=dict(previous_pins_sha256='688C85CAB43BFBF54458E3E258BACBD41FBECD87C83BC5C671E06A1033178099',
        issue='R1 statement failure can only reduce writer output is too broad: AutoCompleteOnClose can replace one pending one-unit integer value with null.',
        abstract_counterexample=dict(nominal='{"x":0}', completed_after_failure='{"x":null}', extra_units=3),
        supported_next_hypothesis='Bound writer builder/escape arrays with nominalL+3; normal returned ToString/LF/UTF8 still use nominalL because failed codec never returns. Freeze R1 and issue separate corrected component; no cap/source/domain changes.'),
    remaining=['DefaultContractResolver/DefaultSerializationBinder and type-contract/thread-store/dynamic-delegate cache source/storage profile remains a once F term, not0 or a per-encode duplicate',
        'Actual selected source/binary/build/ordinary closed-caller association: no user converter, default-settings delegate, trace/reference handler, callbacks, custom objects, multidimensional arrays or reference-preserving setting',
        'General CLR exception/stack/type/metadata/loader/native heap remains explicitly unqualified outside controlled DTO/array/string/clone/stored-box owners, not0 or total-RSS credit',
        'Independent whole critic must validate graph/key/depth inputs, diagnostic and AutoComplete correction and join every phase before fit/binding/GO'],
    input_pins=[dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in inputs],
    no_target_execution=True)
(here / 'runtime-component.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(per_doc_runtime={k:v['selected_direct_runtime_upper'] for k,v in graphs.items()}, phases=phases, SDK_error_tail=error_tail), indent=2))
