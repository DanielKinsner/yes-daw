"""Independent ordinary-file/JSON/text arithmetic; never invokes a subject tool."""
from pathlib import Path
from hashlib import sha256
import collections
import difflib
import json
import math
import os
import re
import sys

REPO = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
BASE = REPO / 'build-ci/musical-audio-persistent-planner-certification/source-revisions'
NEW = BASE / 'clock-oracle-r11-tuple-metadata-source-r1'
OLD = BASE / 'clock-oracle-r10-native-path-correction-r3'
checks = []
pins = []

def digest(data):
    return sha256(data).hexdigest().upper()

def check(name, condition, detail=None):
    checks.append({'name': name, 'pass': bool(condition), 'detail': detail})

def read_pin(path, expected_sha=None, expected_size=None, label=None):
    p = Path(path)
    if not p.is_absolute():
        p = REPO / p
    b = p.read_bytes()
    record = {'path': str(p.relative_to(REPO)).replace('\\', '/') if p.is_relative_to(REPO) else str(p).replace('\\', '/'), 'bytes': len(b), 'sha256': digest(b)}
    pins.append(record)
    if expected_sha is not None:
        check('sha256:' + (label or record['path']), record['sha256'] == expected_sha, record['sha256'])
    if expected_size is not None:
        check('bytes:' + (label or record['path']), len(b) == expected_size, len(b))
    return b

def u16(s):
    return len(s.encode('utf-16-le')) // 2

def walk_json(value, depth=0):
    out = collections.Counter(values=1)
    if isinstance(value, dict):
        out['objects'] += 1
        out['keys'] += len(value)
        out['max_depth'] = depth + 1
        out['max_units'] = max((u16(k) for k in value), default=0)
        for v in value.values():
            child = walk_json(v, depth + 1)
            for k, n in child.items():
                out[k] = max(out[k], n) if k.startswith('max_') else out[k] + n
    elif isinstance(value, list):
        out['arrays'] += 1
        out['max_depth'] = depth + 1
        for v in value:
            child = walk_json(v, depth + 1)
            for k, n in child.items():
                out[k] = max(out[k], n) if k.startswith('max_') else out[k] + n
    elif isinstance(value, str):
        out['strings'] += 1
        out['max_units'] = u16(value)
    elif isinstance(value, bool):
        out['booleans'] += 1
    elif isinstance(value, int):
        out['integers'] += 1
        check('manifest_integer_domain', -(2**63) <= value <= 2**64 - 1)
    elif isinstance(value, float):
        out['floats'] += 1
    elif value is None:
        out['nulls'] += 1
    else:
        raise TypeError(type(value))
    return out

subject_expected = {
    'clock_stage.cpp': 'F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206',
    'run-clock.ps1': '0E41A01150AE5A588D7D42176F6A6C7A6D922F8CE3AE69AD474FDA26E05C7DA9',
    'source-costs.h': '0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27',
    'README.md': '9734D19F1E4CEB10FF2BACC0E67874AEADA639D4C13E8555CD6AEAED37FFD284',
    'source-manifest.json': '52E3482AF966C562CBFB5119F145B79BC762C3EA8B17D050DC9CFF96CDF54F4D',
}
# Separately supplied root pins, not subject-derived authority.
subject = {name: read_pin(NEW / name, h) for name, h in subject_expected.items()}
old = {name: read_pin(OLD / name) for name in subject}
old_review = json.loads(read_pin('build-ci/20261105-clock-oracle-r10-native-path-correction-independent-review-r3/review.json'))
check('predecessor_review_scope', old_review['verdict'] == 'ACCEPT_SOURCE_AND_CONDITIONAL_SCALAR_CORRESPONDENCE_ONLY_PAYLOAD_HOLD')
check('predecessor_review_manifest_identity', digest(old['source-manifest.json']) == old_review['subject_manifest_sha256'])
check('predecessor_review_runner_identity', digest(old['run-clock.ps1']) == old_review['runner_sha256'])
author = json.loads(read_pin('build-ci/20261105-clock-oracle-r11-tuple-metadata-source-authoring-r1/source-pins.json', '4ED7FBFC23870A0FD8F1A05DCB11FCC92CE3A74C41B39FD1DFD8F65F2233EF3B'))
for row in author['subject'] + author['author_evidence'] + author['source_audit_inputs']:
    read_pin(row['path'], row['sha256'], row['bytes'], 'author_record:' + row['path'])

m = json.loads(subject['source-manifest.json'])
om = json.loads(old['source-manifest.json'])
check('exactly_five_subject_files', sorted(p.name for p in NEW.iterdir()) == sorted(subject))
for name, b in subject.items():
    check('UTF8_LF:' + name, b.decode('utf-8').encode('utf-8') == b and b'\r' not in b)
for s in m['sources']:
    check('source_descriptor:' + s['path'], len(subject[s['path']]) == s['bytes'] and digest(subject[s['path']]) == s['sha256'] and len(subject[s['path']].splitlines()) == s['lines'])
for name in ['clock_stage.cpp', 'source-costs.h']:
    check('predecessor_byte_exact:' + name, subject[name] == old[name])

input_ids = []
input_bytes = 0
input_chunks = 0
for row in m['inputs']:
    data = read_pin(row['path'], row['sha256'], row['bytes'], 'input:' + row['path'])
    p = Path(row['path'])
    if not p.is_absolute(): p = REPO / p
    normalized = os.path.normcase(os.path.abspath(p))
    input_ids.append(normalized)
    input_bytes += len(data)
    input_chunks += math.ceil(len(data) / 65536)
check('distinct_input_physical_names', len(input_ids) == len(set(input_ids)))
check('predecessor_inputs_unchanged', m['inputs'] == om['inputs'])
check('predecessor_closed_namespaces_unchanged', m['closed_directory_inputs'] == om['closed_directory_inputs'])
namespace_rows = []
for row in m['closed_directory_inputs']:
    root = Path(row['path'])
    if not root.is_absolute(): root = REPO / root
    entries = []
    files = []
    pending = [root]
    while pending:
        here = pending.pop()
        for p in here.iterdir():
            st = p.lstat()
            check('namespace_no_reparse:' + str(p), not bool(getattr(st, 'st_file_attributes', 0) & 0x400))
            relative = p.relative_to(root).as_posix()
            entries.append(relative)
            if p.is_dir():
                if row['recursive']: pending.append(p)
            else:
                files.append(relative)
    check('namespace_file_set:' + row['path'], sorted(files) == sorted(row['files']))
    check('namespace_file_count:' + row['path'], len(files) == row.get('file_count', len(row['files'])), len(files))
    check('namespace_entry_count:' + row['path'], len(entries) == row['entry_count'], len(entries))
    namespace_rows.append({'path': row['path'], 'entries': len(entries), 'files': len(files), 'recursive': row['recursive']})

shape = dict(walk_json(m))
shape['visits'] = shape['values'] + shape['keys']
profile = {
    'inputs': len(input_ids), 'input_bytes': input_bytes, 'input_chunks': input_chunks,
    'include_inputs': len(input_ids) + len(m['current_include_inputs']),
    'namespace_roots': len(namespace_rows),
    'namespace_entries': sum(r['entries'] for r in namespace_rows),
    'namespace_files': sum(r['files'] for r in namespace_rows),
    'own_five_bytes': sum(map(len, subject.values())),
    'own_five_chunks': sum(math.ceil(len(b) / 65536) for b in subject.values()),
    'current_fence_identities': len(input_ids) + 5,
    'success_files': len(m['references']) + len(m['compiler_final_outputs']) + len(m['control_files']) + len([n for n in m['stage_files'] if n != 'failure-inventory.json']),
    'own_five_path_units_max': max(u16(str((NEW / name).resolve())) for name in subject),
    'known_current_path_units_max': max(u16(str(Path(p).resolve())) for p in input_ids + [str(NEW / name) for name in subject]),
}
profile['maximum_complete_identities'] = profile['current_fence_identities'] + profile['success_files'] + 1
profile['parent_reference_capacity'] = 65 * profile['maximum_complete_identities']
profile['alias_entries_max'] = 2 * profile['inputs'] + 3 * (profile['maximum_complete_identities'] - profile['inputs'])
for k, actual, expected in [
    ('inputs',profile['inputs'],m['profile']['input_count']),
    ('input_bytes',input_bytes,m['profile']['input_bytes']),
    ('input_chunks',input_chunks,m['profile']['input_hash_chunks']),
    ('include_inputs',profile['include_inputs'],m['profile']['include_input_count']),
    ('namespace_roots',profile['namespace_roots'],m['profile']['directory_namespaces_count']),
    ('namespace_entries',profile['namespace_entries'],m['profile']['directory_namespace_entries']),
    ('namespace_files',profile['namespace_files'],m['profile']['directory_namespace_files']),
    ('current_fence',profile['current_fence_identities'],m['profile']['complete_current_inputs']),
    ('success_files',profile['success_files'],m['profile']['declared_success_run_files']),
    ('maximum_complete',profile['maximum_complete_identities'],m['profile']['maximum_complete_fence_identities']),
    ('own5max',profile['own_five_path_units_max'],178),
    ('knownmax',profile['known_current_path_units_max'],270),
    ('bootstrap_values',shape['values'],9945),('bootstrap_keys',shape['keys'],6074),
    ('bootstrap_visits',shape['visits'],16019),('bootstrap_depth',shape['max_depth'],4),
    ('bootstrap_units',shape['max_units'],435),('bootstrap_floats',shape.get('floats',0),0),
]: check('profile:' + k, actual == expected, {'actual': actual, 'expected': expected})
check('own5_under_1MiB', profile['own_five_bytes'] <= 1048576)
check('all_manifest_guards_false', all(v is False for v in m['guards'].values()))
check('no_work_price', m['source_work']['cap'] is None and m['source_work']['new_research_limit'] is None and all(m['source_work'][k] == 'HOLD_UNBOUND' for k in ['runner_upper','producer_upper','verifier_upper','whole_upper']))
check('provisional_admission_explicit', 'PROVISIONAL_PREDECESSOR_R3_ADMISSION' in m['profile']['profile_source'] and m['profile']['future_constinit_data_identity_included'] is False)
check('no_numeric_credit_in_R11', all(m['R11_tuple_metadata'][k] is False for k in ['old_census_or_price_transferred','global_payload_fit','binding_or_execution_GO']))
check('new_lifetime_row_qualification', 'no general heap or reclaim assertion' in m['controlled_lifetime']['metadata_rows'])
check('path_known_tilde_count', sum('~' in p for p in input_ids + [str(NEW / n) for n in subject]) == m['R10_payload_repairs']['known_current_identity_paths_with_tilde'] == 0)
check('path_alias_literal', profile['alias_entries_max'] == m['R10_payload_repairs']['path_alias_maximum'])
check('path_identity_literal', profile['maximum_complete_identities'] == m['R10_payload_repairs']['path_identity_maximum'])
check('path_string_literal', profile['parent_reference_capacity'] == m['R10_payload_repairs']['path_string_maximum'])
check('namespace_minimum_file_rows', 8 * profile['namespace_files'] == m['R11_tuple_metadata']['unconditional_successful_namespace_file_rows'] == 8360)
check('conditional_snapshot_rows', 8 * profile['namespace_entries'] == 8736 and m['R11_tuple_metadata']['snapshot_directory_rows_are_not_unconditional_lower'] is True)

def json_diff(a, b, path=''):
    if type(a) != type(b): return [{'path':path,'old':a,'new':b}]
    if isinstance(a, dict):
        out = []
        for k in sorted(set(a) | set(b)):
            if k not in a: out.append({'path':path+'/'+k,'added':b[k]})
            elif k not in b: out.append({'path':path+'/'+k,'removed':a[k]})
            else: out.extend(json_diff(a[k],b[k],path+'/'+k))
        return out
    if isinstance(a, list):
        if len(a) != len(b): return [{'path':path,'old':a,'new':b}]
        return [d for i,(x,y) in enumerate(zip(a,b)) for d in json_diff(x,y,path+'/'+str(i))]
    return [] if a == b else [{'path':path,'old':a,'new':b}]

manifest_diff = json_diff(om,m)
expected_manifest_changes = {
    '/R10_payload_repairs/bootstrap_observed/keys',
    '/R10_payload_repairs/bootstrap_observed/nodes_and_keys',
    '/R10_payload_repairs/bootstrap_observed/values',
    '/R10_payload_repairs/changed_helper_bodies',
    '/R10_payload_repairs/current_own5_path_UTF16_max',
    '/R10_payload_repairs/helper_bodies_byte_exact_R9',
    '/R10_payload_repairs/raw_profiles/bootstrap/nodes_and_keys',
    '/R11_tuple_metadata', '/controlled_lifetime/metadata_rows',
    '/exact_source_critic', '/profile/future_literal_binding_profile_state',
    '/profile/profile_source', '/schema', '/source_authoring_go',
    '/sources/1/bytes','/sources/1/lines','/sources/1/sha256',
    '/sources/3/bytes','/sources/3/lines','/sources/3/sha256',
}
check('complete_manifest_delta_whitelist', {r['path'] for r in manifest_diff} == expected_manifest_changes)
(OUT / 'manifest-differences.json').write_text(json.dumps(manifest_diff,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
runner = subject['run-clock.ps1'].decode()
before = old['run-clock.ps1'].decode()
runner_diff = ''.join(difflib.unified_diff(before.splitlines(True), runner.splitlines(True), fromfile='accepted-R10-native-path-r3', tofile='R11-tuple-metadata'))
(OUT / 'runner.diff').write_text(runner_diff,encoding='utf-8',newline='\n')
# This inverse uses the exact public carrier mapping, not the author's edit script.
inverse = runner.replace('# Fresh internal metadata row: Item1 FullName, Item2 Name, Item3 directory,\n# Item4 Int64 length, Item5 reparse. Caller wire projections remain maps.\n','')
constructor = '[System.Tuple[object,object,object,object,object]]::new($taskPath,[IO.Path]::GetFileName($taskPath),$taskDirectoryFlag,$taskLength,$taskLink)'
original = '[pscustomobject]@{FullName=$taskPath;Name=[IO.Path]::GetFileName($taskPath);PSIsContainer=$taskDirectoryFlag;Length=$taskLength;Reparse=$taskLink}'
check('one_tuple_constructor', runner.count(constructor) == 1)
inverse = inverse.replace(constructor,original)
consumer_lines = []
for number,line in enumerate(runner.splitlines(),1):
    reads = re.findall(r'\$(taskF|_)\.Item([1-5])\b',line)
    if reads: consumer_lines.append({'line':number,'reads':len(reads),'items':[int(x[1]) for x in reads],'source':line})
for i, member in enumerate(['FullName','Name','PSIsContainer','Length','Reparse'],1):
    inverse = re.sub(r'(\$(?:taskF|_))\.Item'+str(i)+r'\b',r'\1.'+member,inverse)
inverse = inverse.replace('Assert-TaskRawJsonBytes $taskBootstrap 16019 512 4','Assert-TaskRawJsonBytes $taskBootstrap 15978 512 4')
check('complete_runner_inverse_byte_identity', inverse.encode() == old['run-clock.ps1'])
check('seven_consumers', len(consumer_lines) == 7)
check('twenty_five_consumer_reads', sum(r['reads'] for r in consumer_lines) == 25)
check('no_other_Item_reads', len(re.findall(r'\.Item[1-5]\b',runner)) == 25)
check('no_old_metadata_member_reads', not re.search(r'\$(?:taskF|_)\.(?:FullName|Name|PSIsContainer|Length|Reparse)\b',runner))
entries_calls = [{'line':i,'source':line} for i,line in enumerate(runner.splitlines(),1) if 'Get-TaskEntries' in line]
check('entries_definition_and_seven_callers',len(entries_calls)==8)
check('bootstrap_actual_literal', 'Assert-TaskRawJsonBytes $taskBootstrap '+str(shape['visits'])+' 512 4' in runner)
check('entry_guards_remain_false', all(re.search(r'\$'+name+r'=\$false\b',runner) for name in ['taskSourceReviewAccepted','taskSourceWorkAccepted','taskExecutionAccepted','taskCompilerAccepted','taskProduceAccepted','taskVerifyAccepted','taskCompilerPeakDiskQualified','taskProductionContextQualityRendererReady']))
check('runner_normal_control_inventory', re.findall(r"Expect-TaskGate '([^']+)'",runner) == m['normal_gate_controls']['runner'])
cpp = subject['clock_stage.cpp'].decode()
check('cpp_distinct_controls', re.findall(r'expect\("([^"]+)"',cpp) == m['normal_gate_controls']['cpp'])
check('cpp_guards_remain_false', all(re.search(r'\b'+name+r'=false\b',cpp) for name in m['guards']))
check('private_catalogue_no_value_definition', 'static const Catalogue reviewed_;' in subject['source-costs.h'].decode() and not re.search(r'Catalogue\s+Catalogue::reviewed_\s*=',cpp))
check('metadata_not_supported_JSON_scalar', '[System.Tuple' not in runner[runner.index('function Visit-TaskJsonValue'):runner.index('function Measure-TaskJson')])
check('reference_projection_still_five_map_fields', '$taskRows.Add(@{path=$taskF.Item1;relative_path=$taskRelative;bytes=$taskH[\'bytes\'];sha256=$taskH[\'sha256\'];origin_bundle_sha256=$taskExpectedManifest})' in runner)
check('build_projection_still_four_map_fields', '$taskRows.Add(@{path=$taskF.Item1;bytes=$taskH[\'bytes\'];sha256=$taskH[\'sha256\'];origin_bundle_sha256=$taskExpectedManifest})' in runner)
check('failure_projection_still_five_map_fields', '$taskRows.Add(@{path=$taskF.Item1;bytes=$taskF.Item4;reparse=$taskF.Item5;admission=\'NOT_AUTOMATICALLY_ADMITTED\';opaque_temporary=($taskF.Item1.StartsWith' in runner)
# Source-sized regions are an ordinary line partition, not a PowerShell parser.
def regions(text):
    starts=list(re.finditer(r'^function ([A-Za-z-]+)',text,re.M))
    main=text.index('\ntry{\n # Hash literal manifest')+1
    return {s[1]:text[s.start():starts[i+1].start() if i+1<len(starts) else main] for i,s in enumerate(starts)}
r9 = read_pin(BASE / 'clock-oracle-r9-source-prototype/run-clock.ps1').decode()
new_regions, r9_regions = regions(runner),regions(r9)
check('function_count',len(new_regions)==m['R10_payload_repairs']['functions']==68)
check('manifest_R9_same_region_list', sorted(n for n in new_regions.keys()&r9_regions.keys() if new_regions[n]==r9_regions[n]) == m['R10_payload_repairs']['helper_bodies_byte_exact_R9'])
check('manifest_R9_changed_region_list', sorted(n for n in new_regions.keys()&r9_regions.keys() if new_regions[n]!=r9_regions[n]) == m['R10_payload_repairs']['changed_helper_bodies'])
check('manifest_R9_added_regions', sorted(new_regions.keys()-r9_regions.keys())==sorted(m['R10_payload_repairs']['added_helpers']))
check('manifest_R9_removed_region', sorted(r9_regions.keys()-new_regions.keys())==[m['R10_payload_repairs']['removed_helper']])

tuple_path='build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/Tuple.cs'
tuple_source=read_pin(tuple_path,'A6000D29CC1DA0A3BA582D76E277DE74172FE7BC65EE0F658ABBD1F301711A24').decode()
tuple5=tuple_source[tuple_source.index('public class Tuple<T1, T2, T3, T4, T5> :'):tuple_source.index('public class Tuple<T1, T2, T3, T4, T5, T6> :')]
check('tuple5_not_enumerable', 'IEnumerable' not in tuple5 and 'IEnumerator' not in tuple5)
for i in range(1,6):
    check('tuple5_identity_cell:'+str(i), 'private readonly T'+str(i)+' m_Item'+str(i)+';' in tuple5 and 'public T'+str(i)+' Item'+str(i)+' => m_Item'+str(i)+';' in tuple5 and 'm_Item'+str(i)+' = item'+str(i)+';' in tuple5)

public_origins = json.loads(read_pin(OUT / 'primary-source-origins.json'))
public_expected = {
    'LanguagePrimitives.cs': ('284B0FAD37EF182C51566E8135B20E4C27464B1EE9352A1786E7DC76C3089711',291050),
    'Binders.cs': ('CCA6BC97C2CDC16FDC1EB612FABCCCEA919629BC4C04F952C00A458FF7C9F6AC',389899),
}
public_sources = {}
for row in public_origins:
    name=Path(row['path']).name
    h,n=public_expected[name]
    public_sources[name]=read_pin(OUT / row['path'],h,n).decode()
    check('public_source_commit:'+name,row['commit']=='ea554a8f9c6085f54fb2828f7ea286c65c35599f')
    check('public_source_origin_pin:'+name,row['sha256']==h and row['bytes']==n)
lp=public_sources['LanguagePrimitives.cs']
calc=lp[lp.index('private static GetEnumerableDelegate CalculateGetEnumerable('):lp.index('private static readonly CallSite<Func<CallSite, object, IEnumerator>> s_getEnumeratorSite')]
check('LP_no_Tuple_special_enumeration','Tuple' not in calc and 'ITuple' not in calc)
check('LP_regular_enumeration_requires_IEnumerable','typeof(IEnumerable).IsAssignableFrom(objectType)' in calc and 'return LanguagePrimitives.ReturnNullEnumerable;' in calc)
binders=public_sources['Binders.cs']
nonenum=binders[binders.index('private static IEnumerator NotEnumerableRule('):binders.index('private static IEnumerator ArrayRule(')]
check('binder_direct_scalar_branch',all(s in nonenum for s in ['obj is not PSObject','obj is not IEnumerable','obj is not IEnumerator','obj is not DataTable','!Marshal.IsComObject(obj)','return null;']))
check('binder_wrapper_debases_to_same_scalar_branch','NotEnumerableRule(site, PSObject.Base(obj))' in nonenum)
check('binder_no_Tuple_special_case','Tuple' not in nonenum and 'ITuple' not in nonenum)

report={'schema':'R11_INDEPENDENT_ORDINARY_FILE_TEXT_REVIEW_EVIDENCE_R1','checks':checks,'check_count':len(checks),'failed':[c for c in checks if not c['pass']], 'profile':profile,'bootstrap':shape,'namespaces':namespace_rows,'consumer_lines':consumer_lines,'entries_calls':entries_calls,'manifest_changed_paths':[r['path'] for r in manifest_diff],'scope':'Only ordinary Python/file reads/hashes/JSON/string/integer arithmetic. No target parser/compiler/helper/program/stage/import/GUI/Git.'}
(OUT/'mechanical-evidence.json').write_text(json.dumps(report,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
(OUT/'checked-input-pins.json').write_text(json.dumps(pins,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'checks':len(checks),'failed':report['failed'],'profile':profile,'bootstrap':shape,'manifest_changed_paths':report['manifest_changed_paths'],'consumers':[{k:v for k,v in r.items() if k!='source'} for r in consumer_lines]},indent=2))
sys.exit(1 if report['failed'] else 0)
