from pathlib import Path
import hashlib
import json
import re

root = Path.cwd()
folder = root / 'build-ci/20261105-musical-native-math-region-certificate-r1'
checks = []


def verify(name, condition):
    assert condition, name
    checks.append(name)


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


c = read_json(folder / 'certificate.json')
for row in c['input_records']:
    verify('pin:' + row['path'], hashlib.sha256((root / row['path']).read_bytes()).hexdigest().upper() == row['sha256'])
d, p, inventory = [read_json(root / row['path']) for row in c['input_records']]
rows = inventory['instructions']
lookup = {int(row['rva'], 16): row for row in rows}
verify('296 unique decoded rows', len(rows) == len(lookup) == 296)
verify('nine region rows', len(c['proposed_compared_regions']) == len(p['regions']) == 9)
for a, b in zip(c['proposed_compared_regions'], p['regions']):
    verify('region:' + a['role'], a['role'] == b['role'] and int(a['rva'], 16) == int(b['start_rva'], 16)
           and int(a['end_exclusive'], 16) == int(b['end_rva_exclusive'], 16) and a['bytes'] == b['bytes']
           and a['file_offset'] == b['raw_file_offset'] and a['sha256'] == b['sha256'])
verify('8284 bytes', sum(row['bytes'] for row in c['proposed_compared_regions']) == 8284)
verify('1572 code bytes', sum(row['bytes'] for row in c['proposed_compared_regions'] if row['class'] == 'CodeMatched') == 1572)
ranges = [(int(row['start'], 16), int(row['end_exclusive'], 16)) for row in c['instruction_enumeration']['ranges']]
for index, (a, b) in enumerate(ranges):
    verify('range:' + hex(a), a < b and a in lookup)
    for e, f in ranges[index + 1:]:
        verify('disjoint:' + hex(a) + ':' + hex(e), b <= e or f <= a)


def reachable(rva):
    return any(a <= rva < b for a, b in ranges)


def target(assembly):
    match = re.search(r'\b([0-9A-Fa-f]{16})\b', assembly)
    assert match, assembly
    return int(match.group(1), 16) - 0x180000000


controls, excluded, returns = {}, {}, []
for row in rows:
    rva, op = int(row['rva'], 16), row['assembly'].split()[0].lower()
    if op.startswith('j') or op == 'call':
        (controls if reachable(rva) else excluded)[rva] = op
    if reachable(rva) and op == 'ret':
        returns.append(rva)
ledger = {int(row['at'], 16): row for row in c['reachable_branch_instruction_edges']}
verify('all17 reachable branch/calls covered', set(controls) == set(ledger) and len(controls) == 17)
for rva, item in ledger.items():
    source = lookup[rva]
    verify('edge op:' + hex(rva), controls[rva] == item['operation'])
    verify('edge target:' + hex(rva), target(source['assembly']) == int(item['taken'], 16))
    if item['taken_class'] == 'CodeMatched':
        verify('matched target:' + hex(rva), reachable(int(item['taken'], 16)))
    field = 'otherwise' if 'otherwise' in item else 'return_site' if 'return_site' in item else None
    if field:
        following = next(row for row in rows if int(row['rva'], 16) > rva and row['source_span'] == source['source_span'])
        verify('fallthrough:' + hex(rva), int(following['rva'], 16) == int(item[field], 16))
verify('all reachable returns', set(returns) == {int(row, 16) for row in c['instruction_enumeration']['returns']})
unreachable = {int(row[0], 16): row for row in c['unreachable_control_edges']}
verify('all22 excluded branch/calls', set(unreachable) == set(excluded) and len(excluded) == 22)
for rva, row in unreachable.items():
    verify('excluded op:' + hex(rva), excluded[rva] == row[1])
    verify('excluded target:' + hex(rva), target(lookup[rva]['assembly']) == int(row[2], 16))

fixed, leas, indexed = {}, [], []
for row in rows:
    rva, assembly = int(row['rva'], 16), row['assembly']
    if not reachable(rva):
        continue
    match = re.search(r'\[([0-9A-Fa-f]{16})h?\]', assembly)
    if match:
        value = int(match.group(1), 16) - 0x180000000
        if assembly.split()[0].lower() == 'lea':
            leas.append((rva, value))
        else:
            fixed[rva] = value
    if re.search(r'\[[a-z0-9]+\+rax\*8\]', assembly):
        indexed.append(rva)
expected = {int(site, 16): int(ref['rva'], 16) for ref in c['valid_readonly_references'] for site in ref['sites']}
expected.update({int(site, 16): int(c['mutable_dependency']['rva'], 16) for site in c['mutable_dependency']['read_sites']})
verify('every fixed RIP read', fixed == expected)
widths = {'xmmword': 16, 'ymmword': 32, 'mmword': 8, 'qword': 8, 'dword': 4, 'word': 2, 'byte': 1}
for ref in c['valid_readonly_references']:
    first, width = int(ref['rva'], 16), ref['maximum_width']
    verify('readonly extent:' + ref['rva'], any(int(row['rva'], 16) <= first and first + width <= int(row['end_exclusive'], 16)
           and row['class'] == 'TableMatched' for row in c['proposed_compared_regions']))
    for site in ref['sites']:
        match = re.search(r'\b(xmmword|ymmword|mmword|qword|dword|word|byte) ptr', lookup[int(site, 16)]['assembly'])
        verify('operand width:' + site, match is not None and widths[match.group(1)] <= width)
table_sites = {int(site, 16): int(ref['base'], 16) for ref in c['index_proof']['read_sites'] for site in ref['sites']}
verify('six indexed reads', set(indexed) == set(table_sites) and len(indexed) == 6)
for rva, base in table_sites.items():
    source = lookup[rva]
    register = re.search(r'\[([a-z0-9]+)\+rax\*8\]', source['assembly']).group(1)
    preceding = [row for row in rows if int(row['rva'], 16) < rva and row['source_span'] == source['source_span']
                 and re.match(r'lea\s+' + register + ',', row['assembly'])]
    verify('indexed LEA:' + hex(rva), bool(preceding) and (int(preceding[-1]['rva'], 16), base) in leas)
    verify('indexed width:' + hex(rva), re.search(r'\b(mmword|qword) ptr', source['assembly']) is not None)
    verify('257-double span:' + hex(base), any(int(row['rva'], 16) == base and row['bytes'] == 257 * 8 for row in c['proposed_compared_regions']))
verify('all LEAs are table bases', {base for _, base in leas} == set(table_sites.values()))

lanes = {int(row['rva'], 16): int(row['uint64_hex'], 16) for row in d['constant_lanes']}
A, B = int(c['index_proof']['mantissa_mask_A'], 16), int(c['index_proof']['rounding_mask_B'], 16)
verify('index masks', A == lanes[0x107480] == 0xFF00000000000 and B == lanes[0x107490] == 0x80000000000)
verify('upper lanes zero', lanes[0x107488] == lanes[0x107498] == 0)
indices = []
for high in range(256):
    for low in range(2):
        bits = (high << 44) | (low << 43)
        value = ((bits & A) + ((bits & B) << 1)) >> 44
        verify('factorization:' + str(high) + ':' + str(low), value == high + low)
        indices.append(value)
verify('all possible indexes', min(indices) == 0 and max(indices) == 256)
verify('sum cannot overflow', max((high << 44) + (low << 44) for high in range(256) for low in range(2)) == 1 << 52)
rel, cert = d['base_relocation_directory'], c['relocation_certificate']
verify('complete relocation identity', cert['directory_bytes'] == rel['bytes'] and cert['raw_sha256'] == rel['raw_sha256']
       and cert['complete_entry_count'] == len(rel['entries']) == 1404 and cert['entry_types'] == sorted({row['type'] for row in rel['entries']}))
verify('all intersections empty', all(not row['base_relocation_intersections'] for row in p['regions']))
verify('mutable address only', int(c['mutable_dependency']['rva'], 16) == 0x139E60
       and int(c['mutable_dependency']['end_exclusive'], 16) == 0x139E64 and c['mutable_dependency']['bytes'] == 4
       and c['mutable_dependency']['raw_expected_value'] is None)
verify('no runtime claim', c['both_arms']['runtime_qualification_completed'] is False and c['both_arms']['candidate_dependency_graph_covers_both'] is True)
out = root / 'build-ci/20261105-musical-native-math-region-certificate-independent-review-r1/review.json'
assert not out.exists()
record = {
    'schema': 'INDEPENDENT_CANDIDATE_DEPENDENCY_CERTIFICATE_REVIEW',
    'decision': 'ACCEPT_CANDIDATE_GRAPH_CONDITIONAL_ON_STATED_PREMISES',
    'scope': 'Root read domain proof; independently cross-checked static file/graph/footprint/arithmetic identities only. No runtime or binding credit.',
    'checks': len(checks), 'check_names': checks,
    'certificate_md_sha256': hashlib.sha256((folder / 'certificate.md').read_bytes()).hexdigest().upper(),
    'certificate_json_sha256': hashlib.sha256((folder / 'certificate.json').read_bytes()).hexdigest().upper(),
    'checker_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest().upper(),
    'runtime_qualification': False, 'profile_data_defined': False, 'prices_defined': False,
    'target_program_executed': False,
}
out.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8', newline='\n')
print('PASS candidate dependency certificate:', len(checks), 'independent static checks')
print('certificate.md', record['certificate_md_sha256'])
print('certificate.json', record['certificate_json_sha256'])
print('review.json', hashlib.sha256(out.read_bytes()).hexdigest().upper())
