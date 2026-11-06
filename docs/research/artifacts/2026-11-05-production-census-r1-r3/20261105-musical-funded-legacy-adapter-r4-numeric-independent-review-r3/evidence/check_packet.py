"""Read sealed data/source and check integer arithmetic only; never execute subjects."""
from pathlib import Path
from collections import Counter
import hashlib
import json
import re
import struct

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / '20261105-musical-funded-legacy-adapter-r4-census-r3'
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
    'census.md': 'D01F74AE8986D0AFD843B6F7405C63F996505324343A498655595B2D9C46B0A4',
    'census.json': '3867588D746FD95D1445CF07A24D4C099777B248B195B85D48960B75097C2D5D',
    'source-pins.json': 'B467F90C7CE4D31330F3123A7DB231B31D409133D0CF3CF00C8E3413F839C909',
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
ftext = (repo / 'build-ci/20261105-musical-funded-legacy-adapter-source-r4/formation-r8.h').read_text(encoding='utf-8')
body = re.search(r'enum class Site\s*:[^{]+\{([^}]+)\}', ftext, re.S).group(1)
body = re.sub(r'//[^\n]*', '', body)
site_names = [x.strip() for x in body.split(',') if x.strip() and x.strip() != 'Count']
check('source_inventory', 'all420 ordered source names', site_names == [x['site'] for x in events])
check('prior_findings', 'owned copy getter return case closed', data['helpers']['GetterTwo']['tuple'] == [2,2,0,0,0,2,2,0,0,0])
check('prior_findings', 'distinct span conversion getter case closed', data['helpers']['ReturnSpanValue']['tuple'] == [2,2,0,0,0,2,2,0,0,0])
check('prior_findings', 'span helper occurs exactly at162and343', [e['ordinal'] for e in events if 'ReturnSpanValue' in e['helper_coefficients']] == [162,343])
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
check('MD_JSON', '51 helper rows', len(helper_lines) == len(data['helpers']) == 51)
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
        prices[0] += 10
    return sum(price * count for price, count in zip(prices, counts))


fit = data['conditional_fit']['conditional_examples_only']
native = fit['native_candidate_unbound']
for r, expected in fit['Q_for_R'].items():
    check('conditional_example', 'sealed Q R=' + r,
          quote(int(r), native['M'], native['L'], native['D'], native['A']) == expected)

# Full new auxiliary factorization and ordered-source proposal consistency.
factor = load(BASE / 'evidence/all-420-factorizations.json')
check('new_factorization', '420 matching ordered rows', len(factor['rows']) == 420)
for row, event in zip(factor['rows'], events):
    check('new_factorization', event['site'], all(row[k] == event[k]
          for k in ['ordinal','site','source_anchors','body_vector','body_B_vector',
                    'body_d_vector','helper_coefficients']))
    hv = plus([0]*10,*[[count * n for n in data['helpers'][name]['tuple']]
                for name, count in event['helper_coefficients'].items()])
    hd = plus([0]*10,*[[count * n for n in data['helpers'][name].get('d_vector',[0]*10)]
                for name, count in event['helper_coefficients'].items()])
    check('new_factorization', event['site'] + ' full body/helper/ABI',
          plus(event['body_vector'], hv) == event['constant_vector']
          and event['body_B_vector'] == event['B_vector']
          and plus(event['body_d_vector'], hd) == event['d_vector']
          and row['helper_vector'] == hv and row['helper_d_vector'] == hd
          and row['price'] == event['price_coefficients'])

ordered = load(BASE / 'evidence/ordered-operands.json')
check('ordered_operands', '23 rows',len(ordered['rows'])==23)
groups = 0
for row in ordered['rows']:
    parts=row['ordered_body_operands']; groups+=len(parts)
    check('ordered_operands', row['site'],
          [p['order'] for p in parts] == list(range(1,len(parts)+1))
          and plus(*[p['vector'] for p in parts]) == row['body_vector']
          == by_name[row['site']]['body_vector']
          and row['callee_or_earlier_sponsors'])
check('ordered_operands','113 groups',groups==113)

# Independently preserve all previous concrete corrections; no other price acceptance.
prior=load(repo/'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2/evidence/packet-checks.json')
for row in prior['site_corrections']:
    e=by_name[row['site']]
    check('prior_correction_closed',row['site'],e['constant_vector']==row['necessary_vector_if_other_components_retained']
          and e['price_coefficients']==row['necessary_price_if_other_components_retained'])

# Confirm source anchoring from ordinary text only, with all literal Site references.
source={p.name:p.read_text(encoding='utf-8').splitlines()
        for p in (repo/'build-ci/20261105-musical-funded-legacy-adapter-source-r4').glob('*')
        if p.suffix in ['.h','.cpp']}
occurrences={name:[] for name in by_name}
for fn,lines in source.items():
    for ln,line in enumerate(lines,1):
        for name in re.findall(r'\bSite::([A-Za-z0-9_]+)',line):
            if name!='Count':
                check('source_site_reference',fn+':'+str(ln)+'/'+name,name in by_name)
                occurrences[name].append({'file':fn,'line':ln})
for e in events:
    check('source_anchor',e['site'],all(a in occurrences[e['site']] for a in e['source_anchors']))
check('source_inventory','declared unentered190',not occurrences['LegacyLocalOwnerReturn'])

# Separately selected fundamental-type identity, from object bytes only.
identity_folder=repo/'build-ci/20261105-musical-funded-r5-conversion-identity-r1'
binary=(identity_folder/'attempt-r1/identity.obj').read_bytes()
machine,nsec,timestamp,syms,nsyms,opts,flags=struct.unpack_from('<HHIIIHH',binary)
check('selected_identity','ordinary AMD64 COFF',machine==0x8664 and opts==0)
secs=[]
for i in range(nsec):
    p=20+40*i
    sec={'name':binary[p:p+8].rstrip(b'\0').decode()}
    sec['bytes'],sec['offset'],sec['relocation_offset']=struct.unpack_from('<III',binary,p+16)
    sec['relocations']=struct.unpack_from('<H',binary,p+32)[0]
    sec['flags']=struct.unpack_from('<I',binary,p+36)[0]
    secs.append(sec)
strings=syms+18*nsyms
matches=[]
idx=0
while idx<nsyms:
    p=syms+18*idx;rawname=binary[p:p+8]
    if rawname[:4]==b'\0'*4:
        pos=strings+struct.unpack_from('<I',rawname,4)[0]
        name=binary[pos:binary.index(b'\0',pos)].decode()
    else:name=rawname.rstrip(b'\0').decode()
    value,section,typ,storage,aux=struct.unpack_from('<IhHBB',binary,p+8)
    if name=='yesdaw_conversion_identity':matches.append((value,section,storage))
    idx+=1+aux
check('selected_identity','unique public symbol',len(matches)==1 and matches[0][2]==2)
value,section,storage=matches[0];si=secs[section-1]
check('selected_identity','readonly exact24byte table',si['name']=='.rdata' and si['bytes']==24 and value==0
      and si['flags']&0x40000000 and not si['flags']&0xa0000000 and si['relocations']==0)
words=struct.unpack_from('<QQQ',binary,si['offset'])
check('selected_identity','tag/version/d0 exact',words==(0x5945534441574431,1,0)
      and data['selected_R5_fundamental_type_identity_separate']['words']==list(words))
check('selected_identity','no code/initializer or relocation',not any(s['bytes'] and s['flags']&0x20 for s in secs)
      and not any(s['name'].startswith('.CRT') or s['relocations'] for s in secs))
check('arithmetic','domain-extreme needed stays U64',f['needed_extreme_U64_upper_d1']==3272+268435456*972+536870912*609<2**40)
example=fit['all_success_inequality'];conditions=example['conditions']
root_number=root_total[0]+root_total[1]*conditions['B']+root_total[2]*conditions['d']
needed_number=(3271+conditions['d']+conditions['clipCount_plus_assetCount']*(970+2*conditions['d'])
               +conditions['R']*(608+conditions['d']))
base_number=(root_number+needed_number+quote(conditions['R'],native['M'],native['L'],native['D'],native['A'])
             +1+6*conditions['R']+4096+1024)
check('conditional_example','complete sealed all-success arithmetic',base_number==example['source_upper_excluding_N_term']
      and base_number+24==example['C1_example_N1_total']
      and (2**28-base_number)//24==example['C1_max_N_by_sufficient_upper']
      and (2**28-base_number)//40==example['C2_max_N_by_sufficient_upper'])

# Necessary witnessed boundary corrections, retaining all other proposed components.
# This is NOT a complete corrected census: the systematic all-source audit must continue.
body_remove={
 'QueryNativeScope':6,'QueryNativeBaseline':1,'QueryNativeConstructComplete':1,
 'QueryNativeCheckEntry':1,'QueryNativeIdentity':1,'QueryNativeRead':2,
 'QueryNativeCompare':2,'QueryNativeAccepted':1,'QueryNativeRejected':3,
 'QueryNativeBeginReplay':3,'QueryNativeReplayReady':1,'QueryNativeRestore':2,
 'QueryNativeScopeDestroy':2,'QueryNativeRestoreVerify':3,'QueryNativeRestoreReady':1,
 'NativeMatchEntry':1,'NativeMatchAccepted':1,'NativeMatchFailed':1,
 'NativeMatchSelection':2,'NativeMatchSelectionPolicy':1,'NativeMatchSelectionStore':3,
}
helper_remove={'Access':1,'RestorePrefix':1}
corrections=[]
for e in events:
    removed=body_remove.get(e['site'],0)+sum(helper_remove.get(n,0)*c for n,c in e['helper_coefficients'].items())
    if removed:
        v=list(e['constant_vector']);v[3]-=removed
        check('necessary_boundary_delta_nonnegative',e['site'],v[3]>=0)
        corrections.append({'ordinal':e['ordinal'],'site':e['site'],
            'body_X_removed':body_remove.get(e['site'],0),
            'helper_X_removed':removed-body_remove.get(e['site'],0),
            'constant_X_delta':-removed,'sealed_price':e['price_coefficients'],
            'necessary_constant_vector_if_other_components_retained':v,
            'necessary_price_if_other_components_retained':[sum(v),*e['price_coefficients'][1:]]})
delta={r['site']:r['constant_X_delta'] for r in corrections}
schedule_corrections=[]
for name,s in schedules.items():
    dx=sum(c*delta.get(site,0) for site,c in s['site_counts'].items())
    if dx:
        p=list(s['price_constant_B_d']);p[0]+=dx
        schedule_corrections.append({'schedule':name,'constant_delta':dx,'sealed_price':s['price_constant_B_d'],
                                     'necessary_price_if_other_components_retained':p})
new_schedules={n:list(s['price_constant_B_d']) for n,s in schedules.items()}
new_schedules.update({r['schedule']:r['necessary_price_if_other_components_retained'] for r in schedule_corrections})
root_dx=sum(new_schedules[n][0]-schedules[n]['price_constant_B_d'][0] for n in root_parts)
terms_delta={
 'pendingWindowQueryFixed':new_schedules['QueryNewFixed'][0]-schedules['QueryNewFixed']['price_constant_B_d'][0],
 'pendingWindowQueryPerPrefix':new_schedules['PrefixIteration'][0]-schedules['PrefixIteration']['price_constant_B_d'][0],
 'pendingWindowQueryPerSearch':new_schedules['SearchIteration'][0]-schedules['SearchIteration']['price_constant_B_d'][0],
 'pendingNativeMatchFixed':new_schedules['NativeMatchFixed'][0]-schedules['NativeMatchFixed']['price_constant_B_d'][0],
 'pendingNativeMatchPerRegion':new_schedules['NativeComparedRegion'][0]-schedules['NativeComparedRegion']['price_constant_B_d'][0],
 'pendingNativeMatchPerPage':new_schedules['NativeComparedPage'][0]-schedules['NativeComparedPage']['price_constant_B_d'][0],
 'pendingNativeMatchPerByte':new_schedules['NativeComparedByte'][0]-schedules['NativeComparedByte']['price_constant_B_d'][0],
}
result={'schema':'R4_CENSUS_R3_INDEPENDENT_DATA_CHECKS_R1','scope':'Whole sealed packet arithmetic and source-inventory checks; not correctness acceptance.',
 'status':'ARITHMETIC_PASS_SOURCE_BOUNDARY_REVISE','check_count':len(checks),
 'categories':dict(Counter(x['kind'] for x in checks)),'checks':checks,'pins':records,
 'literal_source_occurrences':occurrences,'necessary_witnessed_body_X_removals':body_remove,
 'selected_identity_object_sections':secs,'selected_identity_words':list(words),
 'necessary_witnessed_helper_X_removals':helper_remove,'necessary_site_corrections':corrections,
 'necessary_schedule_corrections':schedule_corrections,
 'necessary_root_constant_delta_from_witnessed_schedule_changes_only':root_dx,
 'necessary_quote_unit_deltas_from_witnessed_changes_only':terms_delta,
 'not_complete_corrected_price_or_fit':True,'subject_executed_or_edited':False}
(OUT/'packet-checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'checks':len(checks),'categories':result['categories'],
 'necessary_witnessed_body_rows':len(body_remove),'helper_rows':len(helper_remove),
 'affected_site_prices':len(corrections),'root_partial_delta':root_dx,'quote_unit_partial_deltas':terms_delta}))
