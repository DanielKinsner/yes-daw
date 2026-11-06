"""Independent data/source checker. Never loads or invokes the target language."""
from pathlib import Path
from collections import Counter
import hashlib
import json

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
checks = []
inputs = {}

def sha(b):
    return hashlib.sha256(b).hexdigest().upper()

def read(rel, expected=None):
    p = ROOT / rel
    b = p.read_bytes()
    record = dict(path=rel, bytes=len(b), sha256=sha(b))
    inputs[rel] = record
    if expected:
        check('pin:' + rel, record['sha256'] == expected)
    return b

def check(label, condition):
    checks.append(dict(check=label, passed=bool(condition)))
    if not condition:
        raise AssertionError(label)

def text(rel, expected=None):
    return read(rel, expected).decode('utf-8-sig')

def js(rel, expected=None):
    return json.loads(read(rel, expected))

def join(a, b):
    # Selected nonempty ordinary paths only; IO.Path.Join does not reset a root.
    check('join nonempty', bool(a) and bool(b))
    return a + ('' if a[-1] in '/\\' or b[0] in '/\\' else '\\') + b

def canonical_selected(p):
    # Independent restricted lexical model, not ntpath or any target path API.
    check('selected ASCII', all(32 <= ord(c) <= 126 for c in p))
    check('selected no tilde', '~' not in p)
    p = p.replace('/', '\\')
    check('selected C absolute', p.startswith('C:\\'))
    parts = p[3:].split('\\')
    check('selected no dot/empty/trailing segments', all(s and s not in ('.', '..') and not s.endswith((' ', '.')) for s in parts))
    check('selected no provider/wildcard/reserved separators', all(not any(c in s for c in ':*?"<>|') for s in parts))
    return p

def chain(p):
    result = []
    while p:
        result.append(p)
        p = None if len(p) == 3 else p[:max(3, p.rfind('\\'))]
    return result

base_rel = 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
geom_rel = 'build-ci/20261105-clock-oracle-r12-path-geometry-r1'
g = js(geom_rel + '/geometry.json', 'BE95DC06AB85E7BF0A706656B08A6C2C8B5581C4AE3376BFE7AB1D98CADBAD9F')
m = js(base_rel + '/source-manifest.json', '4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557')
runner = text(base_rel + '/run-clock.ps1', '3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F')
read('build-ci/20261105-prepare-r12-path-geometry.py')  # Source only; never executed.
repo = r'C:\Users\SM - Dan\Documents\GitHub\yes-daw'
base = join(repo, base_rel.replace('/', '\\'))
run = join(base, r'runs\source-clock-r1')
check('repo', g['repo'] == repo)
check('base', g['base'] == base)
check('uncreated root spelling', g['proposed_uncreated_run_root'] == run)
# Do not create or probe the proposed root. Its nonexistence remains a launch premise.
rows = []
for r in m['inputs']:
    name = r['path']
    raw = name if len(name) >= 3 and name[1] == ':' else join(repo, name)
    rows.append(dict(kind='input', name=name, canonical=canonical_selected(raw)))
for name in ['clock_stage.cpp', 'run-clock.ps1', 'source-costs.h', 'README.md', 'source-manifest.json']:
    raw = join(base, name)
    rows.append(dict(kind='own', name=raw, canonical=canonical_selected(raw)))
for kind, descriptors, parent, key in [
    ('reference', m['references'], join(run, 'data'), 'path'),
    ('compiler', m['compiler_final_outputs'], join(run, 'build'), 'name'),
    ('control', m['control_files'], run, 'path')]:
    for r in descriptors:
        raw = join(parent, r[key])
        rows.append(dict(kind=kind, name=raw, canonical=canonical_selected(raw)))
for name in m['stage_files']:
    raw = join(join(run, 'stage'), name)
    rows.append(dict(kind='stage', name=raw, canonical=canonical_selected(raw)))
pool = set()
edges = {}
for i, r in enumerate(rows):
    r['parent_chain'] = chain(r['canonical'])
    check(f'row:{i}', r == g['rows'][i])
    for k, p in enumerate(r['parent_chain']):
        parent = r['parent_chain'][k + 1] if k + 1 < len(r['parent_chain']) else None
        check(f'edge agreement:{i}:{k}', p not in edges or edges[p] == parent)
        edges[p] = parent
        pool.add(p)
check('pool exact', sorted(pool) == g['all_known_strings_sorted'])
check('edges exact', [dict(path=p, parent=edges[p]) for p in sorted(edges)] == g['parent_edges'])
units = lambda s: len(s.encode('utf-16-le')) // 2
derived = dict(identities=len(rows), known_pool_strings=len(pool), known_pool_UTF16_units=sum(map(units, pool)), maximum_canonical_units=max(units(r['canonical']) for r in rows), maximum_known_depth=max(len(r['parent_chain']) for r in rows), parent_array_reference_slots=65 * len(rows), alias_limit=2 * len(m['inputs']) + 3 * (len(rows) - len(m['inputs'])))
for k, v in derived.items():
    check('summary:' + k, v == g[k])
check('identity profile', len(rows) == m['profile']['maximum_complete_fence_identities'] == 1294)
check('unique complete identities', len({r['canonical'] for r in rows}) == 1294)
aliases = set()
for r in rows:
    if r['kind'] != 'input':
        aliases.add(r['name'])
        aliases.add(r['name'].replace('/', '\\'))
for r in rows:
    raw = r['name']
    if r['kind'] == 'input' and not (len(raw) > 2 and raw[1] == ':'):
        # Selected Join-Path branch normalizes the relative child's separators.
        raw = join(repo, raw.replace('/', '\\'))
    aliases.update((raw, r['canonical']))
check('known alias subset', len(aliases) <= derived['alias_limit'])
origins = js(geom_rel + '/primary-source-origins.json', 'D38A2805DF47266A5407AC2074375C9364BEF1EE8E0A388E5703696F3A145076')
for o in origins['files']:
    b = read(geom_rel + '/primary/' + o['file'], o['sha256'])
    check('primary length:' + o['file'], len(b) == o['bytes'])
own_origins_rel = 'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/primary-origins.json'
own_origins = js(own_origins_rel)
for o in own_origins:
    b = read('build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/primary/' + o['file'], o['sha256'])
    check('additional primary length:' + o['file'], len(b) == o['bytes'])
for name in ['Path.cs', 'Path.Windows.cs']:
    read('build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/' + name)
ps_prefix = 'build-ci/20261105-controller-environment-writer-closure-r2'
ps = js(ps_prefix + '/source-origins.json', '5314810ED0AF072A0103248367FAF8A42E51D3C1835242D2D40309771D06DA68')
check('PS source commit', ps['commit'] == 'ea554a8f9c6085f54fb2828f7ea286c65c35599f')
needed = [
    'src/Microsoft.PowerShell.Commands.Management/commands/management/CombinePathCommand.cs',
    'src/System.Management.Automation/engine/PathInterfaces.cs',
    'src/System.Management.Automation/engine/SessionStateNavigation.cs',
    'src/System.Management.Automation/namespaces/NavigationProviderBase.cs',
    'src/System.Management.Automation/namespaces/LocationGlobber.cs',
    'src/System.Management.Automation/namespaces/FileSystemProvider.cs']
byname = {o['repository_path']: o for o in ps['files']}
for name in needed:
    o = byname[name]
    b = read(ps_prefix + '/primary/' + name, o['sha256'])
    check('PS length:' + name, len(b) == o['bytes'])
large = [r for r in rows if units(r['canonical']) >= 260]
check('long selected rows', len(large) == 11)
check('long all pool', len([p for p in pool if units(p) >= 260]) == 11)
# Win32 insufficient-buffer result includes NUL. Fresh builder pos0, capacity260.
scratch = []
for r in large:
    request = max(units(r['canonical']) + 1, 2 * 260)
    bucket = max(16, 1 << (request - 1).bit_length())
    check('selected scratch request520', request == 520)
    check('selected scratch bucket1024', bucket == 1024)
    scratch.append(dict(path=r['canonical'], units=units(r['canonical']), minimum_rent_request=request, selected_pool_char_capacity=bucket))
parent_values = [p for p in edges.values() if p is not None]
parent_requests = dict(complete_known_recipe_only=True, parent_edge_entries=len(edges), nonempty_proper_parent_substring_requests=len(parent_values), requested_UTF16_units=sum(map(units, parent_values)), distinct_parent_contents=len(set(parent_values)), repeated_parent_contents=len(parent_values)-len(set(parent_values)), distinct_parent_contents_UTF16_units=sum(map(units, set(parent_values))))
check('parent requests1457', len(parent_values) == 1457)
check('parent request units157789', parent_requests['requested_UTF16_units'] == 157789)
check('parent distinct164', len(set(parent_values)) == 164)
report = dict(scope='Independent restricted lexical/source correspondence only; no target execution, allocation, fit or GO', checks=len(checks), all_passed=all(c['passed'] for c in checks), derived=derived, categories=dict(Counter(r['kind'] for r in rows)), absolute_input_names=sum(len(r['name']) > 2 and r['name'][1] == ':' for r in rows if r['kind'] == 'input'), relative_input_names=sum(not (len(r['name']) > 2 and r['name'][1] == ':') for r in rows if r['kind'] == 'input'), conditional_complete_known_alias_keys=len(aliases), conditional_parent_edge_population=parent_requests, long_path_scratch=scratch, inputs=sorted(inputs.values(), key=lambda r:r['path']), check_rows=checks)
(OUT / 'file-checks.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({k:report[k] for k in ['checks','all_passed','derived','categories','absolute_input_names','relative_input_names','conditional_complete_known_alias_keys']}))
