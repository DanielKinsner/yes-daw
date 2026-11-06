from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
here = root / 'build-ci/20261106-clock-r12-writer-join-r3'
assert not here.exists()
here.mkdir()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
prior_path = root / 'build-ci/20261106-clock-r12-writer-join-r2/writer-join.json'
assert digest(prior_path.read_bytes()) == '25C4167F94C8DE538861F391F2DF940D345ABD4139CD219E2ACA25DE97A047A5'
prior = json.loads(prior_path.read_bytes())
out = json.loads(prior_path.read_bytes())
out['schema'] = 'R12_WRITER_CODEC_SELECTED_JOIN_CORRECTION_R3'
out['status'] = 'SEALED_SOURCE_PROPOSAL_FOR_FRESH_INDEPENDENT_REVIEW_COLD_AND_GLOBAL_FIT_HOLD'

def a8(n):
    return (n + 7) // 8 * 8

def array(n, width):
    return a8(24 + n * width)

def string(n):
    return a8(22 + 2 * n)

def chunks(length):
    capacity = 256
    count = 1
    while capacity < length:
        capacity += min(capacity, 8000)
        count += 1
    return count

def failure(d, k):
    c = d * max(k + 5, 22)
    return (d + 1) * (56 + array(c, 2) + string(c) + 32) + 64 + 24 * d

for name, doc in out['documents'].items():
    count = chunks(doc['close_time_text_upper'])
    assert doc['requests']['StringBuilder_all_objects'] == 48 * count
    doc['requests']['StringBuilder_all_objects'] = 56 * count
    doc['selected_per_encode_upper'] = sum(doc['requests'].values())
    doc['one_failed_encode_path_upper'] = failure(doc['container_chain'], doc['key_units_upper'])
failed_encodes = 3
orphan_pairs_per_failed_encode = 2
orphan_upper = failed_encodes * orphan_pairs_per_failed_encode * (array(8000, 2) + 56)
diagnostic_upper = max(doc['one_failed_encode_path_upper'] for doc in out['documents'].values()) + out['documents']['failure']['one_failed_encode_path_upper'] + out['documents']['receipt']['one_failed_encode_path_upper']
assert orphan_upper == 96480 and diagnostic_upper == 20408
for phase in out['phases']:
    vector = phase['encode_vector']
    phase['selected_encoding_requests'] = sum(vector[name] * out['documents'][name]['selected_per_encode_upper'] for name in vector)
    active = bool(sum(vector.values()))
    phase['all_failed_encode_paths_upper'] = diagnostic_upper if active else 0
    phase['orphan_retry_array_and_chunk_attempt_upper'] = orphan_upper if active else 0
    phase['selected_B_upper'] = phase['selected_encoding_requests'] + phase['all_failed_encode_paths_upper'] + phase['orphan_retry_array_and_chunk_attempt_upper']
assert [p['selected_B_upper'] for p in out['phases']] == [0, 2755816, 2755816, 2999612, 3243408, 6800264]
out['R3_corrections'] = dict(
    generic_StringBuilder_object_field_slot_upper=56,
    basis='Header16+two-reference/three-int fields each<=8. No actual48-byte layout or allocation measurement assumed.',
    orphan_retry_source='ExpandByABlock allocates char[] before new StringBuilder(this). Object allocation failure leaves an unattached array; the single using-close can retry expansion. First failed main encode exits main; only failure inventory and footer can enter again.',
    failed_encodes_upper=failed_encodes, orphan_pairs_per_failed_encode_upper=orphan_pairs_per_failed_encode,
    each_pair_request_upper=array(8000, 2) + 56, whole_orphan_retry_upper=orphan_upper,
    failed_object_attempt_bytes_also_charged=True,
    ordinary_selected_closed_SDK_no_error_handler_or_retry=True,
    independent_critic_supported_values_are_checked_not_source_execution=True)
out['prior_r2_sha256'] = digest(prior_path.read_bytes())
out['pinned_inputs'] += [dict(path=prior_path.relative_to(root).as_posix(), bytes=prior_path.stat().st_size, sha256=digest(prior_path.read_bytes())),
    dict(path=Path(__file__).resolve().relative_to(root).as_posix(), bytes=Path(__file__).stat().st_size, sha256=digest(Path(__file__).read_bytes()))]
(here / 'writer-join.json').write_text(json.dumps(out, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(B_prefixes=[p['selected_B_upper'] for p in out['phases']], diagnostic_upper=diagnostic_upper, orphan_retry_upper=orphan_upper, report_sha256=digest((here / 'writer-join.json').read_bytes()))))
