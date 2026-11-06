"""Read sealed data/source and check integer arithmetic only; never execute subjects."""
from pathlib import Path
from collections import Counter
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / '20261105-musical-funded-legacy-adapter-r4-census-r1'
OUT = Path(__file__).resolve().parent
checks = []


def pin(path):
    data = path.read_bytes()
    return {'path': str(path), 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest().upper()}


def check(kind, name, ok, detail=None):
    assert ok, (kind, name, detail)
    row = {'kind': kind, 'id': name, 'result': 'PASS'}
    if detail is not None:
        row['detail'] = detail
    checks.append(row)


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def plus(*vectors):
    return [sum(items) for items in zip(*vectors)]


def polynomial(text):
    result = [0, 0, 0]
    for term in text.split('+'):
        match = re.fullmatch(r'(?:(\d+)\*?)?([Bd])', term)
        if match:
            result[1 if match[2] == 'B' else 2] += int(match[1] or 1)
        else:
            assert term.isdecimal(), term
            result[0] += int(term)
    return result


data = load(BASE / 'census.json')
pins = load(BASE / 'source-pins.json')
md = (BASE / 'census.md').read_text(encoding='utf-8')
sealed = {
    'census.md': 'EF03FDF70D329508F7594DC60380881106BE6919542FE2756982E81FA26C6916',
    'census.json': '9FC18AA295F0469D2EF4589E2D171E7CCABD80F4E73479CD9538EEF647E96B62',
    'source-pins.json': 'A6BC8693976BFE82D80E544E47CCAFB945E48010A18FA6CFC202ABED181B5E25',
}
for name, sha in sealed.items():
    check('seal', name, pin(BASE / name)['sha256'] == sha)
records = []
repo = ROOT.parent
for category in ['subject', 'reference_files', 'authored_evidence', 'outputs']:
    for expected in pins[category]:
        actual = pin(repo / expected['path'])
        check('pin', expected['path'], all(actual[key] == expected[key]
              for key in ['bytes', 'sha256']))
        records.append({'category': category, **actual})

events = data['events']
by_name = {row['site']: row for row in events}
check('inventory', '420 ordered unique events', len(events) == len(by_name) == 420
      and [row['ordinal'] for row in events] == list(range(420)))
for name, helper in data['helpers'].items():
    check('helper_arithmetic', name, len(helper['tuple']) == 10
          and sum(helper['tuple']) == helper['price'])
for event in events:
    vector = list(event['body_vector'])
    for name, count in event['helper_coefficients'].items():
        check('helper_coefficient', event['site'] + '/' + name,
              isinstance(count, int) and count >= 0)
        vector = plus(vector, [count * n for n in data['helpers'][name]['tuple']])
    check('event_factorization', event['site'], vector == event['constant_vector'])
    check('event_scalar_sums', event['site'],
          [sum(event[key]) for key in ['constant_vector', 'B_vector', 'd_vector']]
          == event['price_coefficients'])
    check('event_nonnegative', event['site'], all(isinstance(n, int) and n >= 0
          for key in ['body_vector', 'constant_vector', 'B_vector', 'd_vector']
          for n in event[key]))
for name, schedule in data['schedules'].items():
    value = [sum(by_name[site]['price_coefficients'][i] * count
                 for site, count in schedule['site_counts'].items()) for i in range(3)]
    check('schedule_arithmetic', name, value == schedule['price_constant_B_d'], value)
    check('schedule_scope', name, schedule['reservation_only_not_actual_visits'] is True)

site_lines = re.findall(r'^\|(\d+)\|([^|]+)\|([^|]+)\|([^|]*)\|$', md, re.M)
check('MD_JSON', '420 event rows', len(site_lines) == 420)
for ordinal, name, price, anchors in site_lines:
    event = events[int(ordinal)]
    check('MD_event', name, event['site'] == name
          and polynomial(price) == event['price_coefficients'])
helper_text = md.split('## Helper worksheet\n', 1)[1].split('## Finite schedule index', 1)[0]
helper_lines = re.findall(r'^\|([^|]+)\|([0-9,]+)\|(\d+)\|$', helper_text, re.M)
check('MD_JSON', '50 helper rows', len(helper_lines) == len(data['helpers']) == 50)
for name, vector, price in helper_lines:
    helper = data['helpers'][name]
    check('MD_helper', name, [int(n) for n in vector.split(',')] == helper['tuple']
          and int(price) == helper['price'])
schedule_text = md.split('## Finite schedule index\n', 1)[1]
schedule_lines = re.findall(r'^\|([^|]+)\|([^|]+)\|([^|]*)\|$', schedule_text, re.M)
schedule_lines = [row for row in schedule_lines if row[0] in data['schedules']]
check('MD_JSON', '38 schedule rows', len(schedule_lines) == len(data['schedules']) == 38)
for name, price, expanded in schedule_lines:
    counts = {}
    for term in expanded.split('; '):
        parts = term.split('*')
        counts[parts[0]] = int(parts[1]) if len(parts) == 2 else 1
    check('MD_schedule', name, counts == data['schedules'][name]['site_counts']
          and polynomial(price) == data['schedules'][name]['price_constant_B_d'])

old = data['old_callee_exact_catalogue']
old_source = load(repo / old['path'])
check('old_catalogue', 'exact 190 rows', old['rows'] == old_source['rows']
      and len(old['rows']) == 190)
check('old_catalogue', 'terminal cases and templates unchanged',
      old['append_terminal_cases'] == old_source['append_terminal_cases']
      and old['case_templates'] == old_source['case_templates'])
composition_count = 0
for name, row in old['rows'].items():
    check('old_row_sum', name, sum(row['vector']) == row['price'])
    if 'sum' in row:
        total = [0] * 10
        for term in row['sum']:
            name2, count = (term, 1) if isinstance(term, str) else term
            total = plus(total, [count * n for n in old['rows'][name2]['vector']])
        check('old_composition', name, total == row['vector'])
        composition_count += 1
check('old_catalogue', '78 compositions', composition_count == 78)

f = data['formation_equations']
schedules = data['schedules']
root_parts = ['StateCleanup', 'CaptureCheck', 'ControlRole', 'WorkerOrReaderRole',
              'RetainedRole', 'WorkerOrReaderRole', 'ResultRole',
              'LegacyFixed', 'LegacyCheck', 'LegacyCleanup']
root_total = plus(f['fixed_physical_prefix_constant_B_d'],
                  *[schedules[name]['price_constant_B_d'] for name in root_parts])
check('root_arithmetic', 'all five roles plus complete slices',
      root_total == f['same_root_physical_fixed_constant_B_d'], root_total)
check('copy_cells', 'Raw expansion', data['copy_cells']['Raw'] == 420 + 7 + 3 + 5 + 6 + 94)
check('copy_cells', 'receipt/tail/readout/formed',
      data['copy_cells']['FormationReceipt'] == 1 + 16 + 535 + 1 == 553
      and data['copy_cells']['RootCensus'] == 535 + 2 + 1 == 538
      and data['copy_cells']['Readout'] == 553 + 1 == 554
      and data['copy_cells']['Formed'] == 1 + 1 + 553 + 538 == 1093)
check('copy_cells', 'LegacyReadout expansion', 1 + 1 + 4 + 2 + 2 * 2
      + 3 * 6 + 94 + 1 == data['copy_cells']['LegacyReadout'] == 125)
check('copy_cells', 'captured metadata/options', 16 + 192 + 51 + 1 + 3
      == data['copy_cells']['CapturedMetadata'] == 263
      and 2 + 1 + 6 + 2 + 2 == data['copy_cells']['LegacyPreparationOptions'] == 13)
for larger, smaller in [('QuerySearchFalse', 'QuerySearchTrue'),
                        ('NativeMatchByteContinue', 'NativeMatchByteMismatch'),
                        ('QueryReconcileSuccess', 'QueryReconcileEmpty'),
                        ('DrainRetiring', 'AcquireCurrentCaller'),
                        ('AcquireReader', 'AbortReader')]:
    check('explicit_max_alternative', larger + '>=' + smaller,
          by_name[larger]['price_coefficients'][0] >= by_name[smaller]['price_coefficients'][0])
check('explicit_max_alternative', 'prefix failure+reject dominates success continuation',
      by_name['QueryPrefixFailed']['price_coefficients'][0]
      + by_name['QueryNativeRejected']['price_coefficients'][0]
      >= by_name['QueryPrefixContinue']['price_coefficients'][0])

terms = data['query_quote_12_terms']['terms']
check('quote', '12 exact count forms', [t['count'] for t in terms]
      == ['1', 'P', '2*b', 'E', '1', 'b', '1', 'M', 'D', 'L', 'A', 'L'])


def quote(rows, region, byte_count, access_regions, access_bytes, d=0, corrected=False):
    counts = [1, max(rows - 1, 0), 2 * rows.bit_length(), rows + 1 if rows else 0,
              1, rows.bit_length(), 1, region, access_regions, byte_count, access_bytes, byte_count]
    prices = [t['derived_price'][0] + t['derived_price'][2] * d for t in terms]
    if corrected:
        prices[0] += 8
    return sum(price * count for price, count in zip(prices, counts))


fit = data['conditional_fit']['conditional_examples_only']
native = fit['native_candidate_unbound']
for r, expected in fit['Q_for_R'].items():
    check('conditional_example', 'old Q R=' + r,
          quote(int(r), native['M'], native['L'], native['D'], native['A']) == expected)

# Necessary corrections from independently inspected concrete getter cases.
# These are not written back into any subject, and are not a whole replacement census.
delta = {
    'LegacyClockCumulative': [2, 2, 0, 0, 0, 1, 1, 0, 0, 0],
    'LegacyRequestBytes': [0, 0, 0, 0, 0, 1, 1, 0, 0, 0],
    'LegacyPlanPreflight': [0, 0, 0, 0, 0, 1, 1, 0, 0, 0],
    'LegacyPlanOwner': [0, 0, 0, 0, 0, 1, 1, 0, 0, 0],
    'LegacyPlanBytes': [0, 0, 0, 0, 0, 2, 2, 0, 0, 0],
    'QueryPredictionEndpoints': [2, 2, 0, 0, 0, 2, 2, 0, 0, 0],
}
site_corrections = []
for name, dv in delta.items():
    e = by_name[name]
    price = list(e['price_coefficients'])
    price[0] += sum(dv)
    site_corrections.append({'ordinal': e['ordinal'], 'site': name, 'delta_vector': dv,
                             'sealed_price': e['price_coefficients'],
                             'necessary_price_if_other_components_retained': price,
                             'necessary_vector_if_other_components_retained': plus(e['constant_vector'], dv)})
schedule_corrections = []
for name, schedule in schedules.items():
    increment = sum(schedule['site_counts'].get(site, 0) * sum(dv)
                    for site, dv in delta.items())
    if increment:
        price = list(schedule['price_constant_B_d'])
        price[0] += increment
        schedule_corrections.append({'schedule': name, 'delta': increment,
                                     'sealed_price': schedule['price_constant_B_d'],
                                     'necessary_price_if_other_components_retained': price})
check('necessary_delta', 'LegacyFixed +16',
      next(x['delta'] for x in schedule_corrections if x['schedule'] == 'LegacyFixed') == 16)
check('necessary_delta', 'QueryNewFixed +8',
      next(x['delta'] for x in schedule_corrections if x['schedule'] == 'QueryNewFixed') == 8)
necessary_root = list(root_total)
necessary_root[0] += 16
example = fit['all_success_inequality']
baseline = example['source_upper_excluding_N_term']
necessary_baseline = baseline + 24
necessary_examples = {'root': necessary_root,
                      'Q_for_R': {r: quote(int(r), native['M'], native['L'], native['D'], native['A'], corrected=True)
                                  for r in fit['Q_for_R']},
                      'source_upper_excluding_N_term': necessary_baseline,
                      'C1_N1': necessary_baseline + 24,
                      'C1_max_N': (2**28 - necessary_baseline) // 24,
                      'C2_max_N': (2**28 - necessary_baseline) // 40,
                      'scope': 'Necessary conditional data arithmetic only; no price acceptance or fit/binding credit.'}
result = {'schema': 'R4_NUMERIC_INDEPENDENT_DATA_CHECKS_V1',
          'scope': 'Whole packet file/hash/vector/MD/schedule/old-table arithmetic checks; not source-correctness acceptance.',
          'check_count': len(checks), 'categories': dict(Counter(x['kind'] for x in checks)),
          'checks': checks, 'pins': records,
          'site_corrections': site_corrections, 'schedule_corrections': schedule_corrections,
          'necessary_conditional_examples': necessary_examples,
          'blocked_by_source_findings': ['owned aggregate return copy H/Z omitted',
                                        'rows() by-value vector-to-span construction classified as a reference getter'],
          'subject_executed_or_edited': False}
(OUT / 'packet-checks.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({'checks': len(checks), 'categories': result['categories'],
                  'necessary_site_corrections': site_corrections,
                  'necessary_schedule_corrections': schedule_corrections,
                  'necessary_conditional_examples': necessary_examples}))
