"""Read fixed source/data; independently recompute the conditional hash theorem.

Never imports the subject helper, invokes a runtime string hash, parses target
PowerShell, or creates its proposed run directory.
"""
from pathlib import Path
from collections import Counter
import hashlib
import json
import re
import struct

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
BASE = ROOT / 'build-ci/20261106-clock-r12-payload-evaluation-r1'
checks = []
inputs = []

def require(label, result):
    checks.append({'check': label, 'passed': bool(result)})
    if not result:
        raise AssertionError(label)

def load(path, expected=None):
    raw = path.read_bytes()
    h = hashlib.sha256(raw).hexdigest().upper()
    if expected:
        require('sealed:' + path.name, h == expected)
    inputs.append({'path': path.relative_to(ROOT).as_posix(),
                   'bytes': len(raw), 'sha256': h})
    return raw

load(BASE / 'check-fixed-key-hashes.py', 'BACD6D7F66B76334429005BBF38A7C028733E66E3596C649CE11827DB5D5671C')
subject = json.loads(load(BASE / 'fixed-key-hash-chains.json', '587CB936BC03C25E3C6336AC1240C8B4EA763AEAE1EE327F4D43D1987610E1FD'))
raws = {}
for pin in subject['source_pins']:
    path = ROOT / pin['path']
    raw = load(path, pin['sha256'])
    require('extent:' + path.name, len(raw) == pin['bytes'])
    raws[path.name] = raw
hash_helpers = load(ROOT / 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs',
                    '498F12BD53366F09D1BABAB8F95AF01088643EF941D23CDEA31E6178A59773F4').decode('utf-8-sig')
# Runner is sealed in this review independently.
runner = load(ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1').decode('utf-8-sig')
geometry = json.loads(raws['geometry.json'])
manifest = json.loads(raws['source-manifest.json'])
primes = list(map(int, re.findall(r'\b\d+\b', hash_helpers.split('internal static ReadOnlySpan<int> Primes =>')[1].split('];')[0])))
require('prime-prefix', primes[:5] == [3, 7, 11, 17, 23])
require('threshold', 'HashCollisionThreshold = 100;' in hash_helpers)
require('doubling', 'int newSize = 2 * oldSize;' in hash_helpers and 'return GetPrime(newSize);' in hash_helpers)

# Source's instance implementation reads little-endian uint words including
# the string terminator. Compute directly from bytes, with uint overflow.
def hash_string(text, ignore_case):
    raw = text.encode('utf-16-le') + b'\0\0\0\0'
    h = [0x15051505, 0x15051505]
    def take(which, offset):
        word = struct.unpack_from('<I', raw, offset)[0]
        if ignore_case:
            word |= 0x00200020
        value = h[which]
        rotated = ((value * 32) & 0xffffffff) + (value // (1 << 27))
        h[which] = ((rotated + value) ^ word) % (1 << 32)
    left = len(text)
    offset = 0
    while left > 2:
        take(0, offset)
        take(1, offset + 4)
        left -= 4
        offset += 8
    if left > 0:
        take(1, offset)
    return (h[0] + h[1] * 1566083941) % (1 << 32)

def capacities(count):
    result = []
    current = 0
    while current < count:
        current = next(p for p in primes if p >= max(3, current * 2))
        result.append(current)
    return result

rows = geometry['rows']
slash = '\\'
aliases = set()
pre_aliases = set()
pre_strings = set()
for row in rows:
    if row['kind'] != 'input':
        aliases.add(row['name'])
        aliases.add(row['name'].replace('/', slash))
pre_aliases.update(aliases)
require('initial-aliases', len(aliases) == 134)
for index, row in enumerate(rows):
    name = row['name']
    relative_input = row['kind'] == 'input' and not (len(name) > 2 and name[1] == ':')
    raw = geometry['repo'] + slash + name.replace('/', slash) if relative_input else name
    added = {raw, row['canonical']}
    aliases.update(added)
    if index < 1225:
        pre_aliases.update(added)
        pre_strings.update(row['parent_chain'])
strings = set(geometry['all_known_strings_sorted'])
sets = {
    'aliases': (aliases, False),
    'strings': (strings, False),
    'parent_strings': (strings, False),
    'catalogue_inputs': ({p['path'] for p in manifest['inputs']}, False),
    'main_input_index': ({p['canonical'] for p in rows[:1220]}, True),
    'main_include_index': ({p['canonical'] for p in rows[:1220]} | {geometry['base'] + slash + 'source-costs.h'}, True),
}
require('six-tables', len(subject['tables']) == len(sets) == 6)
summary = []
for table in subject['tables']:
    name = table['name']
    keys, ignore_case = sets[name]
    require(name + ':ascii', all(all(32 <= ord(c) <= 126 for c in key) and '~' not in key for key in keys))
    require(name + ':count', table['keys'] == len(keys))
    require(name + ':comparer', table['ordinal_ignore_case'] == ignore_case)
    caps = capacities(len(keys))
    require(name + ':capacities', table['growth_capacities'] == caps)
    require(name + ':rows', len(table['rows']) == len(caps))
    hashes = [hash_string(key, ignore_case) for key in keys]
    maxima = []
    for cap, row in zip(caps, table['rows']):
        counts = Counter(h % cap for h in hashes)
        maximum = max(counts.values())
        # Reproduce exact selected 64-bit FastMod for every finite input.
        multiplier = ((1 << 64) - 1) // cap + 1
        require(f'{name}:{cap}:fastmod', all((((((multiplier * h) % (1 << 64)) >> 32) + 1) * cap >> 32) % (1 << 32) == h % cap for h in hashes))
        upper = cap if cap <= 89 else maximum
        require(f'{name}:{cap}:row', row['capacity'] == cap and row['entire_universe_maximum_bucket'] == maximum and row['source_chain_upper'] == upper)
        require(f'{name}:{cap}:no-rehash', upper < 101)
        if cap >= 197:
            maxima.append(maximum)
    require(name + ':maximum', table['maximum_relevant_full_universe_bucket'] == max(maxima))
    summary.append({'table': name, 'keys': len(keys), 'max_relevant_bucket': max(maxima)})

expected_prefix = {
    'initial_aliases': 134, 'precreated_aliases': len(pre_aliases),
    'final_aliases': len(aliases), 'precreated_strings': len(pre_strings),
    'final_strings': len(strings),
    'precreated_alias_capacity': capacities(len(pre_aliases))[-1],
    'final_alias_capacity': capacities(len(aliases))[-1],
    'precreated_string_parent_capacity': capacities(len(pre_strings))[-1],
    'final_string_parent_capacity': capacities(len(strings))[-1],
}
require('all-prefix-values', subject['prefix'] == expected_prefix)
require('no-late-growth-alias', expected_prefix['precreated_alias_capacity'] == expected_prefix['final_alias_capacity'])
require('no-late-growth-string', expected_prefix['precreated_string_parent_capacity'] == expected_prefix['final_string_parent_capacity'])
require('1225-prefix-source', len(manifest['inputs']) + len(manifest['sources']) + 1 == 1225)
require('created-after-fence', runner.index('Assert-TaskPins;$taskPredecessor=') < runner.index('$taskCreated=$true'))
require('exact-context', all(s in runner for s in ["['repo'] -ceq $taskRepo", "['base'] -ceq $taskBase", "['root'] -ceq $taskRoot"]))
require('unknown-fallback-not-insert', '# Unknown strings and changed root contexts keep the original normalization.\n return [IO.Path]::GetFullPath($Path)' in runner.replace('\r\n', '\n'))
require('dictionary-threshold-branch', 'collisionCount > HashHelpers.HashCollisionThreshold && comparer is NonRandomizedStringEqualityComparer' in raws['Dictionary.cs'].decode('utf-8-sig'))
require('source-multiplier', '1566083941' in raws['String.Comparison.cs'].decode('utf-8-sig'))
report = {
    'status': 'ACCEPT_CONDITIONAL_R12_FIXED_KEY_SOURCE_THEOREM_ONLY',
    'checks': len(checks), 'tables': summary, 'prefix': expected_prefix,
    'scope': 'Exact R12 geometry/root and selected SDK-source association only. All key hashes and growth capacities independently recomputed without the subject code or SDK hash execution.',
    'qualification': 'HashHelpers was read by the author but absent from the frozen subject JSON pins; this review explicitly seals and checks that sixth dependency. No R13 root/union inheritance, actual binary association, whole resource fit, NOFIT or execution GO.',
}
(HERE / 'review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
(HERE / 'checks.json').write_text(json.dumps(checks, indent=2) + '\n', encoding='utf-8', newline='\n')
(HERE / 'source-pins.json').write_text(json.dumps(inputs, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(report, indent=2))
