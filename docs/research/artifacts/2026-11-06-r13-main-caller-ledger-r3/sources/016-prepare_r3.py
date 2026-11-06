"""New complete source proposal revision; read old files only as preserved data."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OLD = ROOT / 'build-ci/20261106-r13-main-caller-ledger-r2'
assert not (HERE / 'main_ledger.py').exists()
source = (OLD / 'main_ledger.py').read_text(encoding='utf-8')
assert hashlib.sha256((OLD / 'caller-ledger.json').read_bytes()).hexdigest().upper() == '0966C8A47AC17FE2283E99094E6315FD01F64FD041DF53DDE808F4F4C521928E'
changes = [
    ("# JSON integers reaching the explicit [long] casts here are Int64 under the\n# selected literal-manifest JsonTextReader/ConvertFromJson route. Type R stays;\n# no identity conversion O is manufactured. Array-key expressions are4/7.",
     "# Every written cast/type-check retains O1 even for a runtime identity.\n# Implicit argument binding has O only for a nonidentity conversion. A typed\n# local annotation is not a written RHS cast. Array-key expressions are4/7.", 1),
    ('4*(1+7+1)', '4*(1+7+1+1)', 1),
    ('local_W=1,long_type_R=1)', 'local_W=1,long_type_R=1,explicit_cast_O=1)', 1),
    ('5+1+1+4+1+1', '5+1+1+4+1+1+1', 3),
    ("group('complete retained expression',long_Type_R=1,profile_index=7", "group('complete retained expression',long_Type_R=1,explicit_cast_O=1,profile_index=7", 1),
    ('5+2+4+1+1+4+1+3', '5+2+4+1+1+4+1+1+3', 2),
    ("group('initial explicit Long expression',long_type_R=1,profile_index=7", "group('initial explicit Long expression',long_type_R=1,explicit_cast_O=1,profile_index=7", 1),
]
for before, after, count in changes:
    assert source.count(before) == count, before
    source = source.replace(before, after)
(HERE / 'main_ledger.py').write_text(source, encoding='utf-8', newline='\n')
argument = (OLD / 'argument.md').read_text(encoding='utf-8')
before = 'Explicit [long] manifest integer casts reaching these rows are identities under the selected fixed-literal manifest JsonTextReader/ConvertFromJson integral route; their written Type read remains, not a fabricated identity O.'
after = 'R3 corrects root R2: every written source cast/type-check has its explicit Type read and O1, even when its runtime value/type is already the destination. Implicit identity argument binding still has O0; a typed-local annotation is not a written RHS cast. This preserves the existing getter1F47 explicit byte[] cast rule. The complete29 written cast sites/3382 bounded occurrences are audited in cast-sites.json; all127 rows are recomputed and rechecked. Six rows omitted141 such operations in R2. Its180658 proposal remains immutable and REVISE, not accepted or retroactively certified.'
assert argument.count(before) == 1
argument = argument.replace(before, after).replace('# Complete R13 bootstrap and main caller proposal', '# Complete R13 bootstrap and main caller proposal R3')
(HERE / 'argument.md').write_text(argument, encoding='utf-8', newline='\n')
working = (OLD / 'WORKING.md').read_text(encoding='utf-8')
working = working.replace('root source proposal', 'root source proposal R3').replace('Actual nonidentity caller', 'Actual nonidentity caller')
working += '\nR3 preserves R2 after an explicit-cast accounting finding. Written cast/type-check O1 is distinct from implicit identity binding O0; all29 source cast sites receive a whole bounded multiplicity audit before the new complete pass. No old aggregate is used to choose the new sum.\n'
(HERE / 'WORKING.md').write_text(working, encoding='utf-8', newline='\n')

subject = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1/run-clock.ps1'
lines = subject.read_text(encoding='utf-8').splitlines()
multiplicities = {22:[1536,1536],59:[1],60:[1],941:[1,1,1,1],957:[1,4],958:[4,1],964:[56],965:[3],984:[1,1],995:[1,1],998:[1,1,1],1006:[1,1,1],1012:[1,78,78],1017:[1,67]}
import re
cast_pattern = re.compile(r'\[(?:long|int|byte|string|uint16|char|long\[\]|uint64\[\]|byte\[\])\](?=\s*[$@(\d])')
sites = []
for number, line in enumerate(lines, 1):
    if not (8 <= number <= 61 or 929 <= number <= 1038):
        continue
    found = list(cast_pattern.finditer(line))
    if found:
        assert number in multiplicities and len(found) == len(multiplicities[number]), number
    for n, match in enumerate(found):
        sites.append({'line': number, 'column': match.start() + 1, 'written_cast': match.group(), 'bounded_multiplicity': multiplicities[number][n], 'O_per_written_cast': 1, 'was_missing_in_R2': number in {941,957,958,964,965,1017}, 'source': line})
assert len(sites) == 29 and sum(x['bounded_multiplicity'] for x in sites) == 3382
assert sum(x['bounded_multiplicity'] for x in sites if x['was_missing_in_R2']) == 141
(HERE / 'cast-sites.json').write_text(json.dumps({'schema': 'COMPLETE_R13_MAIN_WRITTEN_CAST_SITE_AUDIT_R3', 'sites': sites, 'all_written_sites': 29, 'bounded_O_occurrences': 3382, 'implicit_identity_binding_O': 0, 'typed_annotation_distinct': True, 'no_target_execution': True}, indent=2) + '\n', encoding='utf-8', newline='\n')

checker = (OLD / 'check_main.py').read_text(encoding='utf-8')
checker = checker.replace("'ROOT_R13_COMPLETE_BOOTSTRAP_MAIN_CALLER_PROPOSAL_R2'", "'ROOT_R13_COMPLETE_BOOTSTRAP_MAIN_CALLER_PROPOSAL_R3'")
checker = checker.replace("'ROOT_R13_MAIN_CALLER_PROPOSAL_INPUT_PINS_R2'", "'ROOT_R13_MAIN_CALLER_PROPOSAL_INPUT_PINS_R3'")
marker = "    ck('finite_unscaled_positive_source_envelope', 0 < total < (1 << 63))"
extra = '''
    casts = json.loads(pin(HERE / 'cast-sites.json'))
    ck('29_complete_written_cast_sites', len(casts['sites']) == casts['all_written_sites'] == 29)
    ck('3382_source_bounded_written_cast_operations', sum(x['bounded_multiplicity'] * x['O_per_written_cast'] for x in casts['sites']) == 3382)
    for n, site in enumerate(casts['sites']):
        ck('exact_cast_source_' + str(n), lines[site['line'] - 1] == site['source'] and lines[site['line'] - 1][site['column'] - 1:].startswith(site['written_cast']) and site['O_per_written_cast'] == 1)
    old = json.loads(pin(ROOT / 'build-ci/20261106-r13-main-caller-ledger-r2/caller-ledger.json', '0966C8A47AC17FE2283E99094E6315FD01F64FD041DF53DDE808F4F4C521928E'))
    ck('whole127_row_correspondence', len(rows) == len(old['rows']) == 127 and [r['lines'] for r in rows] == [r['lines'] for r in old['rows']])
    for n, (a,b) in enumerate(zip(rows,old['rows'])):
        ck('full_row_delta_scope_' + str(n), (a == b) if a['lines'][0] not in {941,957,958,964,965,1017} else a['units'] > b['units'])
    ck('explicit_cast_delta_has_source_sponsors', total - old['units'] == sum(x['bounded_multiplicity'] for x in casts['sites'] if x['was_missing_in_R2']))
    pin(HERE / 'prepare_r3.py')
    pin(ROOT / 'build-ci/20261106-research-getter-destination-clarifier-r1.md')
'''
assert checker.count(marker) == 1
checker = checker.replace(marker, marker + '\n' + extra)
checker = checker.replace("'argument.md','WORKING.md']", "'argument.md','WORKING.md','cast-sites.json','prepare_r3.py']")
(HERE / 'check_main.py').write_text(checker, encoding='utf-8', newline='\n')
print(json.dumps({'new_source_authored': True, 'whole_rows': 127, 'written_cast_sites': len(sites), 'no_old_helper_executed': True}))
