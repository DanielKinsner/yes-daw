"""Own ordinary-file/source/data arithmetic. No target parser or execution.

WORKING evaluated selected families, not a complete resource upper or fit.
"""
from collections import Counter
from hashlib import sha256
from pathlib import Path
import json
import re
import struct

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BASE = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
def read(p): return json.loads(p.read_bytes())
def digest(p): return sha256(p.read_bytes()).hexdigest().upper()
def a8(n): return (n + 7) // 8 * 8
def arr(n, w): return a8(24 + n * w)
assert digest(BASE / 'run-clock.ps1') == '3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F'
assert digest(BASE / 'source-manifest.json') == '4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557'
source = (BASE / 'run-clock.ps1').read_text()
manifest = read(BASE / 'source-manifest.json')
prsrc = (ROOT / 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs').read_text()
pr = [int(n) for n in re.findall(r'\b\d+\b', prsrc.split('internal static ReadOnlySpan<int> Primes =>', 1)[1].split('];', 1)[0])]
def prime(n): return next(x for x in pr if x >= n)
def capacities(n, kind):
    out = []; c = 0
    while c < n:
        c = (4 if c == 0 else c * 2) if kind == 'list' else prime(c * 2)
        out.append(c)
    return out
def f32(x): return struct.unpack('<f', struct.pack('<f', x))[0]
def hc(n):
    load = f32(.72); q = f32(f32(n) / load)
    c = 3 if q <= 3 else prime(int(q)); out = [c]
    for inserted in range(n):
        if inserted >= int(f32(load * c)):
            c = prime(2 * c); out.append(c)
    return out

# Independently re-expanded from the pinned sources, preserving every old
# backing request. The extra once-per-JObject string-dictionary collision
# rehash is a conservative upper, not an assertion that it happens.
def fixed_graph(value):
    t = Counter(); counts = Counter()
    def visit(x):
        if isinstance(x, dict):
            n = len(x); counts['maps'] += 1; counts['fields'] += n
            t['jobject'] += 112; t['jproperty_and_list'] += 128*n
            t['conversion_enumerators'] += 96
            t['jcontainer_lists'] += 72 + sum(arr(k, 8) for k in capacities(n, 'list'))
            if n:
                caps = capacities(n, 'dict')
                t['jobject_dictionaries'] += 80 + sum(arr(k, 4) + arr(k, 24) for k in caps)
                t['jobject_optional_string_collision_rehash_upper'] += arr(caps[-1], 4) + arr(caps[-1], 24)
            t['parsed_ordered_maps'] += 144 + arr(hc(n)[0], 24)
            if n:
                t['parsed_ordered_maps'] += 112 + sum(arr(k, 24) for k in hc(n)) + arr(n, 8) + 32*n
            for k, y in x.items():
                t['strings'] += a8(22 + len(k.encode('utf-16-le'))); visit(y)
        elif isinstance(x, list):
            counts['arrays'] += 1
            t['jarray'] += 96; t['conversion_enumerators'] += 40
            t['jcontainer_lists'] += 40 + sum(arr(k, 8) for k in capacities(len(x), 'list'))
            t['parsed_arrays'] += arr(len(x), 8)
            for y in x: visit(y)
        else:
            counts['scalars'] += 1; t['jvalue'] += 64
            if isinstance(x, str): t['strings'] += a8(22 + len(x.encode('utf-16-le')))
            elif x is not None: t['scalar_box_upper'] += 48
    visit(value)
    return dict(t), dict(counts)

token = re.compile(r'"(?:\\.|[^"\\])*"|-?\d+|true|false|null|[\[\]{},:]')
oldfixed = read(ROOT / 'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/fixed-inputs.json')
fixed = []
for name, p in [('bootstrap', BASE / 'source-manifest.json'), ('predecessor', ROOT / oldfixed['inputs'][1]['path'])]:
    raw = p.read_bytes(); value = json.loads(raw)
    compact = ''.join(x.group() for x in token.finditer(raw.decode('ascii')))
    assert json.loads(compact) == value
    spans = list(token.finditer(compact))
    shifts = [x.start() + int(x.group().startswith('"')) for x in spans if x.group().startswith('"') or x.group()[0] in '-0123456789']
    points = [0] + shifts + [len(compact)]
    gap = max(y-x for x, y in zip(points, points[1:]))
    cap = 1 << ((max(1024, 16*(gap+6))-1).bit_length())
    t, counts = fixed_graph(value)
    fixed.append(dict(name=name, path=p.relative_to(ROOT).as_posix(), bytes=len(raw), sha256=digest(p), compacted_units=len(compact), maximum_shift_gap=gap,
        raw_array=arr(len(raw), 1), compacted_string=a8(22+2*len(compact)), reader_array_cap=cap,
        reader_arrays=sum(arr(1<<i, 2) for i in range(10, cap.bit_length())), StringBuffer_array_upper=arr(1024, 2),
        graph_families=t, graph_counts=counts, graph_selected=sum(t.values())))

def fields(names): return dict.fromkeys(names.split(), 0)
def shape(x):
    c = Counter(); mh = Counter(); ah = Counter(); keys = set(); maximum = 0
    def visit(v, chain):
        nonlocal maximum
        if isinstance(v, dict):
            maximum = max(maximum, chain+1); c['maps'] += 1; mh[len(v)] += 1
            c['keys'] += len(v); keys.update(v)
            for z in v.values(): visit(z, chain+1)
        elif isinstance(v, list):
            maximum = max(maximum, chain+1); c['arrays'] += 1; ah[len(v)] += 1
            c['array_elements'] += len(v)
            for z in v: visit(z, chain+1)
    visit(x, 0)
    assert all(re.fullmatch('[A-Za-z_][A-Za-z_0-9]*', k) for k in keys)
    return dict(counts=dict(c), map_field_histogram=dict(sorted(mh.items())), array_length_histogram=dict(sorted(ah.items())),
        maximum_container_chain=maximum, maximum_zero_based_container_depth=maximum-1,
        keys=sorted(keys), maximum_key_units=max(map(len, keys)))

# Source-built output topology. All leaves below are scalar placeholders;
# counts, keys, container chains and capacities do not depend on leaf values.
summary = fields('status source_manifest_sha256 entries inverse_records reserved used prior composed_after vendor quality Ready compiler_peak_disk_qualified')
def module(n):
    d = summary.copy(); d['actual_control_witnesses'] = [dict(control=0, gate=0, operands=[0]*6) for _ in range(n)]; return d
child = fields('name started raw_exit_observed raw_exit timed_out poll_limited polls poll_limit log_cap stdout_start stderr_start raw_stdout_bytes raw_stderr_bytes position_record_complete raw_bytes_retained raw_bytes_observed observed_not_confirmed_retained omitted_at_cap unknown_undrained both_eof reads_settled owned_storage_quarantined cleanup failure')
child['cleanup'] = [0]*13
mutation = fields('path offset before after')
witnesses = [fields('observed maximum'), fields('completed limit attempted'), fields('total limit global'), fields('total limit global'), fields('path exists'), fields('path expected_bytes observed_bytes expected_hash observed_hash'), fields('signature machine magic subsystem')] + [fields('observed expected missing unknown duplicate') for _ in range(3)]
controls = [dict(id=0, gate=0, witness=w) for w in witnesses]
controls[5].update(mutation=mutation, initial_copy_sha256=0)
controls[6].update(mutation=mutation, initial_copy_sha256=0, actual_mutant_sha256=0)
receipt = fields('schema stage status source_manifest_sha256 compiler_is_opaque compiler_internal_CPU_heap_RSS_temporary_peak_disk_qualified old_broad_compiler_output_bound_fulfilled children compiler_exit producer_exit verifier_exit vendor quality context Ready renderer reserved_source_work reserved_admitted_retained runner_controls compiler_final_outputs source_dependencies producer verification runner_used failure_code primary_failure secondary_failures failure_inventory_complete failure_inventory_code compiler_final_outputs_admitted')
receipt.update(children=[child.copy() for _ in range(3)], runner_controls=controls,
    compiler_final_outputs=dict(files=[fields('path bytes sha256 origin_bundle_sha256') for _ in range(3)], bytes=0),
    source_dependencies=fields('includes report_is_additional_artifact_only complete_tool_inventory_replaced'), producer=module(3), verification=module(11),
    secondary_failures=[fields('site code count') for _ in range(11)])
marker = fields('stage manifest work retained_reservation attempts opaque_compiler_peak_disk_qualified')
failure = fields('files complete code root_prefix compiler_temporary_peak_disk_qualified admitted_pool_success quarantine_retains_ownership')
failure['files'] = [fields('path bytes reparse admission opaque_temporary') for _ in range(78)]
bundle = fields('schema stage status source_manifest_sha256 predecessor_bundle artifacts transitive_inputs admitted_retained_excluding_bundle bundle_bytes admitted_retained reserved_source_work runner_used compiler_operation compiler_temporary_peak_disk_qualified compiler_CPU_internal_heap_RSS_qualified old_broad_compiler_output_bound_fulfilled vendor quality context Ready renderer')
bundle.update(predecessor_bundle=fields('path sha256 artifacts transitive complete_union_paths complete_union_bytes'),
    artifacts=[fields('path relative_path bytes sha256 origin_bundle_sha256') for _ in range(56)] + [fields('path bytes sha256 origin_bundle_sha256') for _ in range(11)],
    transitive_inputs=[fields('path bytes sha256 origin_bundle_sha256') for _ in range(1225)])
docs = {n:shape(v) for n,v in [('marker',marker),('receipt',receipt),('failure',failure),('bundle',bundle)]}
expected = dict(marker=(1,0,6), receipt=(58,23,300), failure=(79,1,397), bundle=(1294,2,5251))
for n, row in docs.items(): assert tuple(row['counts'].get(k,0) for k in ['maps','arrays','keys']) == expected[n], (n,row)
assert docs['receipt']['map_field_histogram'] == {2:3,3:37,4:6,5:5,6:1,13:2,24:3,30:1}

# The byte-domain correction is independently derived in the immediately
# previous working subfamily packet; no target helper is called here.
cap = read(ROOT / 'build-ci/20261105-clock-oracle-r11-payload-cap-domain-closure-r2/cap-domain.json')
prior = read(ROOT / 'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/phases.json')
writer = read(ROOT / 'build-ci/20261106-clock-r12-writer-family-r1/writer-component.json')
writer_prefixes = [0,2628032,2628032,2867944,3107856,10757826]
hex_counts = [1,4,12,68,131,136]
outphases = []
for i,(p,q) in enumerate(zip(prior['phases'],cap['stages'])):
    s = q['stage_calls_upper']; f=p['fences']; c=p['children']; r=p['references']; b=p['builds']; w=p['root_walks']; parsed=p['native_result_parses']
    E=6*s+(3 if s else 0)+1123*f+60*r+4*b+78*w+1
    D=s+720*f+61*r+b+79*w+1
    H=1225*f+p['nonfence_hashes']
    terms=dict(fixed_captures_graphs_and_reader_selected=sum(x['raw_array']+x['compacted_string']+x['reader_arrays']+x['StringBuffer_array_upper']+x['graph_selected'] for x in fixed),
        hash_Tuple_digest_stored_length_box=112*H,
        hash_hex_strings_upper=152*hex_counts[i], predecessor_control_identity_map_and_stored_length=288 if i else 0,
        walker_Tuple_and_three_stored_scalar_boxes=128*E, walker_queue_map_and_stored_depth_box=192*D,
        explicit_poll_and_finally_direction_arrays_and_stored_boxes=88*7201*c,
        stage_sixname_literal_arrays=72*(6*s+(3 if s else 0)),
        native_capture_and_slices=q['capture_and_slice_selected_upper'], native_reader_arrays=q['native_reader_arrays_upper'], native_StringBuffer_arrays=q['native_StringBuffer_arrays_upper'],
        writer_selected_provisional=writer_prefixes[i], known_cold_writer_once_provisional=13256)
    if c: terms['dependency_raw_decoded_reader_StringBuffer_selected']=arr(4194304,1)+a8(22+2*4194304)+1046744+2072
    # 2 fixed decoders +2 fixed byte grammar constructors, 3 byte controls
    # each encoder+guard constructor; dependency contributes decoder+guard,
    # each captured native output contributes one decoder. Output encoders B.
    terms['non_output_strict_UTF8Encoding_and_overwritten_fallbacks']=120*(4+(6 if i else 0)+(2 if c else 0)+parsed)
    outphases.append(dict(id=p['id'], stages=s, appended_stage_rows=6*s+(3 if s else 0), fences=f, children=c, native_results=parsed,
        walker_rows=E, walker_directory_iterations=D, hashes=H, hex_strings_upper=hex_counts[i], selected_terms=terms, selected_partial=sum(terms.values())))
out=dict(schema='R12_EVALUATED_SELECTED_PAYLOAD_FAMILIES_WORKING_R1', status='PARTIAL_EVALUATION_NOT_COMPLETE_UPPER_FIT_OR_NOFIT',
    runner_sha256=digest(BASE/'run-clock.ps1'), manifest_sha256=digest(BASE/'source-manifest.json'),
    fixed_inputs=fixed, output_topologies=docs, phases=outphases,
    qualifications=['Selected ABI/source field-slot/array/string coefficients conditional. No source/binary-equivalence claim.',
        'No physical reclaim from null, scope, Dispose, Array.Clear, pool return or assumed GC.',
        'Writer component is provisional R1 and has known SDK failure-path additions; no complete codec claim.',
        'P canonical geometry and N conditional startup/environment profile remain explicit root-owned hypotheses.',
        'All unlisted original DTOs, clones, graph readers, wrappers, child-native scratch, source literal/backing arrays and controlled failures still need their evaluated joins.',
        'PS VM transient scalar frames/binder/type-metadata general heap remains unqualified under the original contract, not zero. Stored scalar boxes and explicit materialized arrays remain controlled.'])
(HERE/'evaluation.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(fixed=[{k:x[k] for k in ['name','bytes','compacted_units','maximum_shift_gap','graph_selected']} for x in fixed], output_topologies={n:{k:x[k] for k in ['counts','maximum_container_chain','maximum_key_units']} for n,x in docs.items()},phases=[{k:x[k] for k in ['id','stages','walker_rows','walker_directory_iterations','hashes','selected_partial']} for x in outphases]),indent=2))
