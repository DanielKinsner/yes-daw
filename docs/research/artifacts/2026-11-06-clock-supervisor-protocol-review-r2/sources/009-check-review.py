"""Independent document/data/integer review; never loads or executes a target."""
import hashlib
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = pathlib.Path(__file__).resolve().parent
AUTHOR = ROOT / 'build-ci/20261106-clock-supervisor-protocol-authoring-r2'
SUBJECTS = {
    '0090-clock-research-owned-process-supervisor.md': 'E41566667441AE6A5B7FEB7A06260B929E4FCE8852FFA66072E4EDCDBF1F3827',
    '2026-11-06-clock-supervisor-source-plan.md': '80C4350199709565505B6E2E5C3D1B99C91E13F28B0DD32523193C75D617095C',
    'protocol-v1.md': 'BDBAF18070D56605F6B427E063591880FE161720F31D9648E2B2230D6475DDEE',
    'protocol-schema.json': '2744063FCFCA830B2B1561EFF02D724B003F04EC2D23907E1D872E23C1540638',
    'correspondence.json': '7EE88C592E2F8571AE564658BD51DBABFCC803F150DCD4551469C2E4245BEDC2',
    'source-pins.json': '6672E5FC4CBE0FF785E87BE0471F4BF0CD2BC9D461E58F1DD38A9478125B5A2C',
    'author-lineage.json': '2E5D76123271EDAC48ACCD6C761E7E0721E00DDD1DF68370C135E1A9B06DC5F3',
}
checks = []
inputs = []


def check(name, value):
    checks.append({'name': name, 'pass': bool(value)})
    if not value:
        raise AssertionError(name)


def pin(path, expected=None):
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest().upper()
    if expected:
        check('sha256:' + path.name, digest == expected)
    inputs.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': len(raw), 'sha256': digest})
    return raw


def main():
    data = {name: pin(AUTHOR / name, sha) for name, sha in SUBJECTS.items()}
    schema = json.loads(data['protocol-schema.json'])
    protocol = data['protocol-v1.md'].decode('utf-8-sig')
    correspondence = json.loads(data['correspondence.json'])
    for path, sha in correspondence['tracked_subjects_preserved'].items():
        pin(ROOT / path, sha)
    # Derive field geometry independently, rather than trust schema totals.
    for label, total in [('request', 140), ('terminal', 308)]:
        offset = 0
        for field in schema[label]['fields']:
            check(label + ':' + field['field'], field['offset'] == offset and field['bytes'] in (4, 8, 32))
            offset += field['bytes']
        check(label + ':derived_extent', offset == total)
    request_caps = [140 + (4 + 8 + argc) * (4 + 512 * 2) for argc in (28, 6, 6)]
    check('request:derived_caps', request_caps == schema['request']['max_bytes_by_child'] == [41260, 18644, 18644])
    check('transport:derived_bytes', sum(request_caps) + 3 * 308 == schema['transport']['full_max_bytes'] == 79472)
    flags = re.findall(r'^\| (\d+) \| (\w+) \|', protocol.split('## 5. Flags and availability')[1].split('## 6.')[0], re.M)
    check('flags:all_28_unique', [int(n) for n, _ in flags] == list(range(28)) and len({s for _, s in flags}) == 28)
    reasons = re.findall(r'^\| (\d+) \| (\w+) \| ([\d,]+) \|', protocol.split('## 6. Status, sites and reasons')[1].split('## 7.')[0], re.M)
    check('reasons:all_48_unique', [int(n) for n, _, _ in reasons] == list(range(48)))
    check('reasons:valid_sites', all(0 <= int(site) <= 13 for _, _, sites in reasons for site in sites.split(',')))
    check('reasons:site_pairs', sum(len(sites.split(',')) for _, _, sites in reasons) == 50)
    check('raw_cli:prefix', len('--request16=') == 12)
    check('raw_cli:maximum', 2 + 512 + 1 + 12 + 4 * 512 + 1 == schema['commandline']['raw_commandline_max_including_NUL'] == 2576)
    check('target_cli:maximum', [512 + 2 + argc * (2 * 512 + 3) + 1 for argc in (28, 6, 6)] == schema['commandline']['target_max_including_NUL'])
    env = schema['environment']
    check('environment:exact_keys', set(env['request_order']) == {'SystemRoot', 'windir', 'PATH', 'INCLUDE', 'LIB', 'TEMP', 'TMP', 'VSLANG'})
    check('environment:order', sorted(env['request_order'], key=str.casefold) == env['native_block_order'])
    key_units = sum(map(len, env['request_order']))
    check('environment:key_units', key_units == env['fixed_key_units'] == 43)
    check('environment:block', 2 * (key_units + 8 * (1 + 512 + 1) + 1) == env['native_block_max_bytes'] == 8312)
    # Exhaust the new diagnostic mask, with the stated exclusive reap categories.
    valid_masks = [m for m in range(1 << 15) if m & 6 != 6]
    maximum_prefix = max(m.bit_count() for m in valid_masks)
    maximum_late = max(m.bit_count() + 1 + int(not (m & (1 << 14))) for m in valid_masks)
    check('cleanup:valid_masks', len(valid_masks) == 24576)
    check('cleanup:prefix_max', maximum_prefix == 14)
    check('cleanup:late_max', maximum_late == 15)
    check('cleanup:schema_match', schema['cleanup_mask_arithmetic'] == {'valid_masks_checked': len(valid_masks), 'max_prefix': maximum_prefix, 'max_parent_late': maximum_late})
    expected_keys = 'name started raw_exit_observed raw_exit timed_out poll_limited polls poll_limit log_cap stdout_start stderr_start raw_stdout_bytes raw_stderr_bytes position_record_complete raw_bytes_retained raw_bytes_observed observed_not_confirmed_retained omitted_at_cap unknown_undrained both_eof reads_settled owned_storage_quarantined cleanup failure'.split()
    check('parent:24_existing_keys', schema['parent_row_keys'] == expected_keys and len(set(expected_keys)) == 24)
    for raw, signed in [(0, 0), (259, 259), (0x7fffffff, 2147483647), (0x80000000, -2147483648), (0xffffffff, -1)]:
        check('signed_raw:' + str(raw), (raw if raw < (1 << 31) else raw - (1 << 32)) == signed)
    # Independent observations must remain distinguishable in failure records.
    confirmed, start, end, observed = 0, 100, 103, 4096
    check('failure:physical_vs_confirmed', end - start == 3 and confirmed != end - start and observed - confirmed == 4096)
    observed, written, omitted = 8192, 4096, 4096
    check('failure:unretained_identity', observed == written + (observed - written) and omitted <= observed - written)
    check('read:maximum_prefix_bytes', (1 << 20) + 2 * 4096 == 1056768)
    check('helper:five_distinct_exits', set(schema['helper_exits']) == {'0', '10', '11', '12', '13'})
    check('execution:all_false', schema['future_execution_gates_all_false'] is True)
    # Whole manual review is recorded separately; these are guardrails, not a behavioral test.
    for heading in range(1, 14):
        check('protocol:section_' + str(heading), len(re.findall(r'^## ' + str(heading) + r'\.', protocol, re.M)) == 1)
    for marker in ('BeginTail', 'sourcePrefixUsed', 'mandatoryTailReservation', 'TargetMayHaveRun', 'CoordinatorCountersKnown', 'no next child', 'never automatically refunds', 'unknown/quarantine', 'never reads ambient stdin'):
        check('normative_presence:' + marker, marker in protocol)
    check('F1_F5:addressed_not_certified', [r['id'] for r in correspondence['findings_addressed']] == ['F1', 'F2', 'F3', 'F4', 'F5'])
    pin(pathlib.Path(__file__))
    result = {'schema': 'ROOT_INDEPENDENT_SUPERVISOR_PROTOCOL_R2_REVIEW', 'verdict': 'ACCEPT_ARCHITECTURE_PROTOCOL_SOURCE_AUTHORING_ONLY', 'whole_review': 'review.md', 'checks': checks, 'check_count': len(checks), 'not_accepted': ['implementation', 'source_prices', 'ABI', 'native_association', 'resource_fit', 'execution_GO'], 'inputs': inputs}
    (OUT / 'review.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
    (OUT / 'source-pins.json').write_text(json.dumps({'schema': 'ROOT_PROTOCOL_R2_REVIEW_INPUT_PINS', 'inputs': inputs}, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'verdict': result['verdict'], 'checks': len(checks)}))


if __name__ == '__main__':
    try:
        main()
    except BaseException as exc:
        (OUT / 'failed-checks.json').write_text(json.dumps({'checks': checks, 'error': str(exc)}, indent=2) + '\n', encoding='utf-8', newline='\n')
        (OUT / 'failed-checker.py').write_bytes(pathlib.Path(__file__).read_bytes())
        raise
