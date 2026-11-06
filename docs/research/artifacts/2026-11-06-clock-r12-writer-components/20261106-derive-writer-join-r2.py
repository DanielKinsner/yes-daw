from pathlib import Path
import hashlib
import json
import math
import re

root = Path(__file__).resolve().parents[1]
here = root / 'build-ci/20261106-clock-r12-writer-join-r2'
assert not here.exists()
here.mkdir()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
graph_path = root / 'build-ci/20261106-clock-r12-payload-evaluation-r1/evaluation.json'
graph_raw = graph_path.read_bytes()
graph = json.loads(graph_raw)
assert graph['runner_sha256'] == '3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F'
# Preserve the exact working profile used; later phase-author revisions do not
# silently alter this component's source inputs or acceptance scope.
(here / 'phase-graph-input-snapshot.json').write_bytes(graph_raw)
p_path = root / 'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/bundle-bound.json'
p = json.loads(p_path.read_bytes())
variant = next(v for v in p['variants'] if v['static_integrals_exact'] is False)
assert variant['bound']['bytes'] == 452921
runtime_path = root / 'build-ci/20261106-clock-r12-serializer-runtime-source-r1/runtime-component.json'
runtime = json.loads(runtime_path.read_bytes())
buffer_path = root / 'build-ci/20261106-clock-r12-writer-family-r1/writer-component.json'
buffer = json.loads(buffer_path.read_bytes())
cold_path = root / 'build-ci/20261106-clock-r12-writer-family-r1/cold-supplement.json'
cold = json.loads(cold_path.read_bytes())

def a8(n):
    return (n + 7) // 8 * 8

def array(n, width):
    return a8(24 + n * width)

def string(n):
    return a8(22 + 2 * n)

def chunk_count(length):
    capacity = 256
    count = 1
    while capacity < length:
        capacity += min(capacity, 8000)
        count += 1
    return count

def backings(count, width):
    total = 0
    capacity = 4
    while count:
        total += array(capacity, width)
        if capacity >= count:
            return total
        capacity *= 2
    return total

def failure(d, k):
    c = d * max(k + 5, 22)
    return (d + 1) * (48 + array(c, 2) + string(c) + 32) + 64 + d * 24

maxima = dict(marker=319, receipt=23635, failure=262760, bundle=452921)
docs = {}
for name, maximum in maxima.items():
    top = graph['output_topologies'][name]
    keys = top['keys']
    assert keys and all(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', key) for key in keys)
    assert max(map(len, keys)) == top['maximum_key_units']
    d = top['maximum_container_chain']
    assert d == runtime['graphs'][name]['container_chain']
    maps = top['counts']['maps']
    arrays = sum(top['array_length_histogram'].values())
    assert maps == runtime['graphs'][name]['maps'] and arrays == runtime['graphs'][name]['arrays']
    length = maximum - 1
    # Close-time failure may emit null instead of a one-character nominal
    # integer. Its three extra characters affect writer buffers only.
    write_length = length + 3
    g = min(483, (math.isqrt(3481 + 8 * write_length) - 59) // 2)
    escape = min(sum(array(n, 2) for n in range(35, 519)), array(35, 2) + 2 * write_length + 42 * g)
    n = chunk_count(write_length)
    cap = max(256, write_length + min(8000, write_length) - 1)
    stack_count = max(0, d - 1)
    position_stack = 0 if not stack_count else 40 + backings(stack_count, 32)
    direct_runtime = 1288 - 576 + backings(d, 8) + 64 * maps + 48 * arrays
    pieces = dict(escape_all_old_arrays=escape,
        StringBuilder_all_arrays=2 * cap + 30 * n,
        StringBuilder_all_objects=48 * n,
        returned_ToString=string(length), returned_LF_concat=string(maximum),
        UTF8_byte_array=array(maximum, 1), StringWriter=64, JsonTextWriter=224,
        JsonPosition_stack=position_stack, UTF8Encoding_plus_overwritten_fallbacks=120,
        direct_serializer_runtime_and_traversal=direct_runtime)
    docs[name] = dict(byte_upper=maximum, nominal_text_upper=length,
        close_time_text_upper=write_length, container_chain=d,
        maps=maps, arrays=arrays, key_units_upper=top['maximum_key_units'],
        requests=pieces, selected_per_encode_upper=sum(pieces.values()),
        one_failed_encode_path_upper=failure(d, top['maximum_key_units']))
error_main = max(d['one_failed_encode_path_upper'] for d in docs.values())
error_tail = error_main + docs['failure']['one_failed_encode_path_upper'] + docs['receipt']['one_failed_encode_path_upper']
assert error_tail == 20280
phases = []
for old in buffer['phases']:
    vector = old['encode_vector']
    selected = sum(vector[k] * docs[k]['selected_per_encode_upper'] for k in vector)
    paths = 0 if not old['encode_count'] else error_tail
    phases.append(dict(id=old['id'], encode_vector=vector, selected_encoding_requests=selected,
        all_failed_encode_paths_upper=paths, selected_B_upper=selected + paths))
cold_known = cold['known_once_F_Bcold_subtotal'] + sum(runtime['additional_cold_direct_owner_terms'].values())
assert cold_known == 13344
result = dict(schema='R12_WRITER_CODEC_SELECTED_JOIN_CORRECTION_R2',
    status='SEALED_SOURCE_PROPOSAL_PENDING_INDEPENDENT_REVIEW_COLD_REMAINDERS_AND_GLOBAL_FIT_HOLD',
    source_changed=False, caps_or_domain_changed=False, no_target_execution=True,
    documents=docs, phases=phases, known_once_F_Bcold=cold_known,
    shared_K_J_identity_terms=runtime['shared_with_K_J_identity_terms'],
    first_failure_correction='R1 buffer argument did not cover close-time null insertion; use nominalL+3 for writer arrays, no increased returned text/UTF8 bound. R1 remains preserved and no complete fit was accepted.',
    root_profile='Provisional R12 selected literal root only; independently derived452921-byte bundle wire bound. Final canonical union and new geometry require fresh evaluation, not transfer.',
    graph_snapshot_original=dict(path=graph_path.relative_to(root).as_posix(), bytes=len(graph_raw), sha256=digest(graph_raw)),
    remaining=['EnumUtils default concurrent store/reflection and contract-resolver/binder/cache initialization are explicit once-only F source/storage profile terms',
        'General CLR/SDK metadata/type/loader/native/exception heap remains expressly unqualified outside controlled owners, never0 or total-process/RSS fit',
        'Independent source/ABI/whole phase review, exact final input/root/environment association, immutable binding and separate execution GO remain required'],
    pinned_inputs=[dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in [Path(__file__).resolve(), p_path, runtime_path, buffer_path, cold_path]],
    known_cold_timing='Charge once from earliest applicable prefix; previous initialization or BeforeFieldInit timing is not free credit.')
(here / 'writer-join.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(documents={k:v['selected_per_encode_upper'] for k,v in docs.items()}, error_tail=error_tail, phases=[p['selected_B_upper'] for p in phases], known_once_F_Bcold=cold_known, report_sha256=digest((here / 'writer-join.json').read_bytes())), indent=2))
