"""Own complete new caller ledger checks; never parses/imports/runs PowerShell."""
from pathlib import Path
import hashlib
import json
import re
import runpy
import traceback

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SUB = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
CHECKS = []
PINS = []
sha = lambda raw: hashlib.sha256(raw).hexdigest().upper()


def ck(label, yes):
    CHECKS.append({'name': label, 'pass': bool(yes)})
    if not yes:
        raise AssertionError(label)


def pin(path, expected=None):
    raw = path.read_bytes()
    digest = sha(raw)
    if expected:
        ck('pin:' + path.name, digest == expected)
    PINS.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': len(raw), 'sha256': digest})
    return raw


def main():
    ck('preserve_first_complete_pass', not (HERE / 'caller-ledger.json').exists())
    raw = pin(SUB / 'run-clock.ps1', '9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788')
    manifest = json.loads(pin(SUB / 'source-manifest.json', '10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3'))
    text = raw.decode('utf-8')
    lines = text.splitlines()
    ck('source_extent', len(lines) == 1038)
    models = runpy.run_path(str(HERE / 'main_ledger.py'))
    rows = models['ROWS']
    coverage = set()
    for n, row in enumerate(rows):
        a, b = row['lines']
        ck('row_span_' + str(n), 1 <= a <= b <= 1038 and not coverage.intersection(range(a, b + 1)))
        coverage.update(range(a, b + 1))
        ck('row_multiplicity_' + str(n), isinstance(row['multiplicity'], int) and row['multiplicity'] > 0)
        for i, group in enumerate(row['groups']):
            ck('role_terms_' + str(n) + '_' + str(i), all(isinstance(v, int) and v >= 0 for v in group['roles'].values()) and sum(group['roles'].values()) == group['units'])
        ck('row_total_' + str(n), row['units'] == row['multiplicity'] * sum(x['units'] for x in row['groups']))
    target_lines = [i + 1 for i, line in enumerate(lines) if (i < 61 or i >= 928) and line.strip() and not line.lstrip().startswith('#')]
    ck('whole_noncomment_bootstrap_main_coverage', all(i in coverage for i in target_lines))
    functions = re.findall(r'^function ([\w-]+)', text, re.M)
    ck('74_exact_registration_names', len(functions) == len(set(functions)) == 74)
    ck('one_registration_sponsor', [x['units'] for x in rows if x['lines'] == [62, 928]] == [74])
    ck('five_main_caught_inputs', sum(g['roles'].get('caught_input_W', 0) * r['multiplicity'] for r in rows for g in r['groups']) == len(re.findall(r'catch\{', '\n'.join(lines[928:]))) == 5)
    ck('five_main_catch_selections', sum(g['roles'].get('catch_K', 0) * r['multiplicity'] for r in rows for g in r['groups']) == 5)
    ck('30_receipt_fields', len(re.findall(r'(?:\{|;)(\w+)=', lines[55])) == 30)
    ck('21_bundle_fields', len(re.findall(r'(?:\{|;)(\w+)=', lines[1018])) == 21)
    powers = ','.join(str(10 ** i) for i in range(1, 19))
    ck('18_signed_literal_thresholds', lines[58] == '$taskSignedDigitThresholds=[long[]]@(' + powers + ')')
    ck('19_unsigned_thresholds_with_one_Parse', lines[59] == '$taskUnsignedDigitThresholds=[uint64[]]@(' + powers + ",[uint64]::Parse('10000000000000000000',[Globalization.CultureInfo]::InvariantCulture))")
    ck('source_input_and_rows_domains', len(manifest['inputs']) == manifest['profile']['input_count'] == 1220 and len(manifest['sources']) == 4)
    ck('generated_domains', len(manifest['references']) == 56 and len(manifest['compiler_final_outputs']) == 3 and len(manifest['control_files']) == 4)
    ck('current_header_and_rootwalk_domains', len(manifest['current_include_inputs']) == 1 and manifest['profile']['root_walk_entries_max'] == 78)
    ck('success_namespace_decomposition', manifest['profile']['declared_success_run_files'] == 56 + 3 + 4 + 4 + 1)
    ck('fixed_28_compiler_arguments', len(manifest['compiler']['arguments']) == 28)
    ck('known_static_field_defect_retained', "[object].GetMethod('GetType',[Type[]]::EmptyTypes)" in lines[48])
    ports = ROOT / 'build-ci/20261106-clock-r13-runner-census-r2/public-cmdlet-ports.json'
    pin(ports, '135B3092E3CD24219B8758BE44C2672CA6078F5775033EF5BDB4F3B70DC099FC')
    for name in ['20261105-clock-oracle-separate-research-contract/contract.md','20261105-clock-oracle-research-leaf-census-conventions.md','20261106-research-callback-destination-clarifier-r1.md','20261106-research-catch-input-clarifier-r1.md']:
        pin(ROOT / 'build-ci' / name)
    primary = ROOT / 'build-ci/20261105-controller-environment-writer-closure-r2/primary/src'
    for path in [primary / 'System.Management.Automation/engine/parser/Compiler.cs', primary / 'System.Management.Automation/engine/InitialSessionState.cs', primary / 'System.Management.Automation/engine/LanguagePrimitives.cs', primary / 'Microsoft.PowerShell.Commands.Utility/commands/utility/Write-Object.cs', primary / 'Microsoft.PowerShell.Commands.Utility/commands/utility/Write.cs']:
        pin(path)
    pin(HERE / 'main_ledger.py')
    pin(HERE / 'argument.md')
    pin(HERE / 'WORKING.md')
    pin(Path(__file__))
    # Direct textual own-name inventory over the exact main only. No runtime or
    # syntactic PowerShell parser: this source region contains no function-name
    # diagnostic strings. Body definitions/other helpers are not entered.
    calls = []
    for i in range(928, 1038):
        line = lines[i]
        if line.lstrip().startswith('#'):
            continue
        for name in functions:
            for found in re.finditer(r'(?<![\w-])' + re.escape(name) + r'(?![\w-])', line):
                calls.append({'line': i + 1, 'helper': name, 'column': found.start() + 1})
    calls.sort(key=lambda x: (x['line'], x['column']))
    ck('ten_actual_Expect_call_sites', sum(x['helper'] == 'Expect-TaskGate' for x in calls) == 10)
    total = sum(x['units'] for x in rows)
    ck('finite_unscaled_positive_source_envelope', 0 < total < (1 << 63))

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

    report = {'schema': 'ROOT_R13_COMPLETE_BOOTSTRAP_MAIN_CALLER_PROPOSAL_R3', 'status': 'COMPLETE_OWN_CHECKED_SOURCE_UPPER_REQUIRES_FRESH_WHOLE_CRITIC', 'units': total, 'bootstrap_registration_units': sum(x['units'] for x in rows if x['lines'][1] <= 928), 'main_units': sum(x['units'] for x in rows if x['lines'][0] >= 929), 'rows': rows, 'own_written_main_calls': calls, 'own_written_main_call_count': len(calls), 'checks': CHECKS, 'check_count': len(CHECKS), 'own_helper_bodies_included': False, 'source_functionality': 'HOLD_WRONG_TYPE_ARRAY_STATIC_FIELD', 'trace_or_actual_usage_claimed': False, 'work_limit_Data_fit_execution_accepted': False}
    (HERE / 'caller-ledger.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    (HERE / 'source-pins.json').write_text(json.dumps({'schema': 'ROOT_R13_MAIN_CALLER_PROPOSAL_INPUT_PINS_R3', 'inputs': PINS}, indent=2) + '\n', encoding='utf-8', newline='\n')
    outputs = []
    for name in ['caller-ledger.json','source-pins.json','main_ledger.py','check_main.py','argument.md','WORKING.md','cast-sites.json','prepare_r3.py']:
        b = (HERE / name).read_bytes()
        outputs.append({'path': name, 'bytes': len(b), 'sha256': sha(b)})
    (HERE / 'first-complete-own-pass-freeze.json').write_text(json.dumps({'scope': 'complete source/data/integer proposal, not acceptance or target execution', 'files': outputs}, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'units': total, 'rows': len(rows), 'own_main_written_calls': len(calls), 'checks': len(CHECKS), 'pins_sha256': sha((HERE / 'source-pins.json').read_bytes())}))


if __name__ == '__main__':
    try:
        main()
    except BaseException:
        n = 1
        while (HERE / ('failed-own-check-r' + str(n))).exists():
            n += 1
        failure = HERE / ('failed-own-check-r' + str(n))
        failure.mkdir()
        for path in HERE.glob('*'):
            if path.is_file():
                (failure / path.name).write_bytes(path.read_bytes())
        (failure / 'failure.json').write_text(json.dumps({'checks': CHECKS, 'traceback': traceback.format_exc()}, indent=2) + '\n', encoding='utf-8', newline='\n')
        raise
