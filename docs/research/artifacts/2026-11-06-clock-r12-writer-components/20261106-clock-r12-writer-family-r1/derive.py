"""Source/integer writer request bounds only. Never imports or executes a target."""
from pathlib import Path
import hashlib
import json
import math

here = Path(__file__).resolve().parent
root = here.parents[1]
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
runner = root / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1'
assert digest(runner.read_bytes()) == '3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F'
text = runner.read_text()
for token in ['-Compress -EscapeHandling EscapeNonAscii', '$taskText.Length -ne $Measurement[\'bytes\']-1', '[Text.UTF8Encoding]::new($false,$true).GetBytes($taskText+"`n")']:
    assert token in text

inputs = [runner,
    root / 'build-ci/20261105-clock-oracle-json-primary-sources/newtonsoft-JavaScriptUtils.cs',
    root / 'build-ci/20261105-clock-oracle-json-primary-sources/newtonsoft-JsonConvert.cs',
    root / 'build-ci/20261105-clock-oracle-json-primary-sources/powershell-JsonObject.cs',
    root / 'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextWriter.cs',
    root / 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/StringBuilder.cs',
    root / 'build-ci/20261105-clock-oracle-r10-independent-functional-review/primary-sources/UTF8Encoding.cs',
    root / 'build-ci/20261105-managed-file-type-inventory-source-r3/execution-r1/inventory.json',
    root / 'build-ci/20261105-clock-oracle-r10-runner-census/census.json']
inputs += sorted(p for p in (here / 'primary').iterdir() if p.is_file())
pins = [dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in inputs]

def a8(n):
    return (n + 7) // 8 * 8

def array(n, width):
    return a8(24 + n * width)

def string(n):
    return a8(22 + 2 * n)

def chunk_count(length):
    """Minimum allowed chunk growth maximizes the number of retained chunks."""
    capacity = 256
    count = 1
    while capacity < length:
        capacity += min(capacity, 8000)
        count += 1
    return count

def writer(byte_upper):
    assert 319 <= byte_upper <= 1048576
    length = byte_upper - 1
    # New capacities increase strictly from first35 into36..518. Each new
    # escape request is a disjoint emitted unchanged run plus at most6 cells.
    # Min sum of g new capacities is g*(g+71)/2; sum <= L+6g.
    g = min(483, (math.isqrt(3481 + 8 * length) - 59) // 2)
    assert g >= 0 and g * (g + 59) <= 2 * length
    if g < 483:
        assert (g + 1) * (g + 60) > 2 * length
    escape_all_distinct = sum(array(n, 2) for n in range(35, 519))
    escape_output_bound = array(35, 2) + 2 * length + 42 * g
    escape = min(escape_all_distinct, escape_output_bound)
    chunks = chunk_count(length)
    capacity_upper = max(256, length + min(8000, length) - 1)
    chunk_arrays = 2 * capacity_upper + 30 * chunks
    # Source/selected x64 field-slot uppers, not actual object-size measurements.
    sb_objects = 48 * chunks
    position_arrays = sum(array(n, 32) for n in (4, 8, 16, 32))
    fixed = dict(StringWriter_field_slot_upper=64,
                 JsonTextWriter_including_base_field_slot_upper=224,
                 JsonPosition_List_field_slot_upper=40,
                 JsonPosition_all_old_backings_stride_upper32=position_arrays,
                 UTF8Encoding_field_slot_upper=72,
                 overwritten_replacement_fallback_objects=48)
    row = dict(encoded_byte_upper=byte_upper, ASCII_text_units_upper=length,
               escape_growth_count_upper=g, escape_all_distinct_array_requests=escape_all_distinct,
               escape_output_coupled_array_requests=escape_output_bound,
               escape_selected_array_requests=escape,
               StringBuilder_chunk_count_upper=chunks,
               StringBuilder_all_chunk_capacity_upper=capacity_upper,
               StringBuilder_all_chunk_array_requests=chunk_arrays,
               StringBuilder_all_chunk_object_requests=sb_objects,
               ToString_request=string(length), LF_concat_request=string(byte_upper),
               UTF8_byte_array_request=array(byte_upper, 1), fixed_objects_and_position_stack=fixed)
    row['selected_B_request_upper'] = sum(row[k] for k in [
        'escape_selected_array_requests', 'StringBuilder_all_chunk_array_requests',
        'StringBuilder_all_chunk_object_requests', 'ToString_request',
        'LF_concat_request', 'UTF8_byte_array_request']) + sum(fixed.values())
    return row

docs = {name: writer(size) for name, size in [
    ('marker', 319), ('receipt', 23635), ('failure', 262760), ('bundle', 1048576)]}
vectors = [dict(marker=0, receipt=0, failure=0, bundle=0),
           dict(marker=1, receipt=2, failure=1, bundle=0),
           dict(marker=1, receipt=2, failure=1, bundle=0),
           dict(marker=1, receipt=3, failure=1, bundle=0),
           dict(marker=1, receipt=4, failure=1, bundle=0),
           dict(marker=1, receipt=4, failure=1, bundle=1)]
phases = [dict(id='P' + str(i), encode_vector=v, encode_count=sum(v.values()),
               selected_B_request_upper=sum(v[k] * docs[k]['selected_B_request_upper'] for k in docs))
          for i, v in enumerate(vectors)]
assert [p['encode_count'] for p in phases] == [0, 4, 4, 5, 6, 7]

# Once-only direct controlled cold owners; transitive codec metadata/cache
# construction is an explicit remaining term, not implicitly included or zero.
cold_known = dict(JavaScriptUtils_three_bool128_arrays=3 * array(128, 1),
    JavaScriptUtils_List_char_object_field_slot_upper=40,
    JavaScriptUtils_List_char_all_old_arrays=sum(array(n, 2) for n in (4, 8, 16, 32, 64)),
    JavaScriptUtils_union_second_input_arrays=sum(array(n, 2) for n in (1, 1, 5)),
    JsonWriter_template_outer8=array(8, 8),
    JsonWriter_template_eight_state10_arrays=8 * array(10, 4),
    JsonWriter_allStates_List_field_slot_upper=40,
    JsonWriter_allStates_old_reference_arrays=sum(array(n, 8) for n in (8, 16, 32)),
    JsonWriter_final_state_outer18=array(18, 8),
    EnumInfo_source_field_slot_upper=48,
    EnumInfo_three_array18=sum(array(18, 8) for _ in range(3)),
    TextWriter_shared_CRLF_array=array(2, 2),
    JsonPosition_List_shared_empty_array=array(0, 32))
result = dict(schema='R12_SERIALIZATION_BUFFER_AND_SELECTED_WRITER_SOURCE_COMPONENT_R1',
    status='WORKING_SOURCE_COMPONENT_NOT_COMPLETE_B_COLD_OR_GLOBAL_FIT',
    runner_sha256=digest(runner.read_bytes()), documents=docs, phases=phases,
    known_once_F_Bcold_subtotal=sum(cold_known.values()), known_once_F_Bcold=cold_known,
    remaining=['JavaScriptUtils actual LINQ Union iterator/enumerator/HashSet bodies and backings; DefaultInternalSetCapacity must be pinned',
               'EnumUtils ThreadSafeStore/ConcurrentDictionary/CamelCaseNamingStrategy and controlled reflection/attribute outputs; runtime metadata/cache internals remain separately qualified',
               'Shared Encoding/UTF8 fallback/default static initialization and TextWriter.Null object; not per-encode',
               'JsonSerializerSettings/JsonSerializer/InternalWriter, serialization traversal enumerators/serialize-stack and reflection-contract cache owners; allocate to B/K/F exactly once in whole join'],
    exclusions='O original graphs and K preprocessing clone containers are outside this B component and must be included by the whole phase owner. No free GC/Dispose/pool-return/scope credit. ABI field-slot numbers are selected source representation uppers, not measured actual object sizes. First-refusal may reduce a body, never increases encoded document/content caps under the admitted immutable graph/codec premise.',
    operations='Own Python/file/hash/integer only. No target PowerShell parse/AST/import/codec/compiler/native/program/stage or GUI/device execution.', inputs=pins)
(here / 'writer-component.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(documents={k:v['selected_B_request_upper'] for k,v in docs.items()}, phases=[p['selected_B_request_upper'] for p in phases], known_once_F_Bcold_subtotal=result['known_once_F_Bcold_subtotal'], remaining=result['remaining']), indent=2))
