"""Source-built abstract wire-size bound; no target encoder or fixture encoder."""
from pathlib import Path
import hashlib
import json
from dataclasses import dataclass

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
base = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
raw = (base / 'source-manifest.json').read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() == '4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557'
m = json.loads(raw)
g = json.loads((ROOT / 'build-ci/20261105-clock-oracle-r12-path-geometry-r1/geometry.json').read_bytes())

@dataclass(frozen=True)
class Digits:
    maximum: int = 20

def escaped_bytes(s):
    # Exact R12 EscapeNonAscii preflight spelling for this all-ASCII graph.
    return 2 + sum(2 if c in '"\\\b\t\n\f\r' else 6 if ord(c) < 32 or ord(c) >= 128 else 1 for c in s)

def count(v, depth=0):
    result = dict(bytes=0, value_nodes=1, key_nodes=0, strings=0, string_units=0, numeric_tokens=0, boolean_tokens=0, max_depth=depth)
    def add(other):
        for k in result:
            result[k] = max(result[k], other[k]) if k == 'max_depth' else result[k] + other[k]
    if isinstance(v, Digits):
        result['bytes'] = v.maximum
        result['numeric_tokens'] = 1
    elif isinstance(v, bool):
        result['bytes'] = 4 if v else 5
        result['boolean_tokens'] = 1
    elif isinstance(v, int):
        result['bytes'] = len(str(v))
        result['numeric_tokens'] = 1
    elif isinstance(v, str):
        assert all(32 <= ord(c) <= 126 for c in v)
        result.update(bytes=escaped_bytes(v), strings=1, string_units=len(v))
    elif isinstance(v, list):
        result['bytes'] = 2 + max(0, len(v) - 1)
        for item in v:
            add(count(item, depth + 1))
    elif isinstance(v, dict):
        result['bytes'] = 2 + max(0, len(v) - 1)
        for k, item in v.items():
            result['bytes'] += escaped_bytes(k) + 1
            result['key_nodes'] += 1
            result['strings'] += 1
            result['string_units'] += len(k)
            add(count(item, depth + 1))
    else:
        raise TypeError(type(v))
    return result

rows = g['rows']
by_kind = lambda k: [r for r in rows if r['kind'] == k]
fake_hash = 'A' * 64  # Any admitted uppercase SHA256 hex string has identical extent.

def file_record(path, n, relative=None):
    d = dict(path=path, bytes=n, sha256=fake_hash, origin_bundle_sha256=fake_hash)
    if relative is not None:
        d['relative_path'] = relative
    return d

def graph(exact_static):
    artifacts = []
    for r, desc in zip(by_kind('reference'), m['references'], strict=True):
        artifacts.append(file_record(r['canonical'], Digits(), desc['path']))
    for k in ('compiler', 'control'):
        for r in by_kind(k):
            artifacts.append(file_record(r['canonical'], Digits()))
    stage = {r['canonical'].rsplit('\\', 1)[1]: r for r in by_kind('stage')}
    for name in ('stdout.log', 'stderr.log', 'receipt.json', 'consumed.json'):
        artifacts.append(file_record(stage[name]['canonical'], Digits()))
    transitive = []
    for r, pin in zip(by_kind('input'), m['inputs'], strict=True):
        transitive.append(file_record(r['canonical'], pin['bytes'] if exact_static else Digits()))
    own = {r['canonical'].rsplit('\\', 1)[1]: r for r in by_kind('own')}
    for pin in m['sources']:
        transitive.append(file_record(own[pin['path']]['canonical'], pin['bytes'] if exact_static else Digits()))
    transitive.append(file_record(own['source-manifest.json']['canonical'], len(raw) if exact_static else Digits()))
    pred = {k: (v if exact_static or not isinstance(v, int) else Digits()) for k, v in m['predecessor_bundle'].items()}
    result = dict(
        schema=1, stage='ClockOracle', status='CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED',
        source_manifest_sha256=fake_hash, predecessor_bundle=pred, artifacts=artifacts,
        transitive_inputs=transitive, admitted_retained_excluding_bundle=Digits(),
        bundle_bytes=Digits(), admitted_retained=Digits(), reserved_source_work=Digits(),
        runner_used=Digits(), compiler_operation='OPAQUE',
        compiler_temporary_peak_disk_qualified=False, compiler_CPU_internal_heap_RSS_qualified=False,
        old_broad_compiler_output_bound_fulfilled=False, vendor=False, quality=False, context=False,
        Ready=False, renderer=False)
    assert len(result) == 21
    assert len(artifacts) == 67 and len(transitive) == 1225 and len(pred) == 6
    assert sum('relative_path' in a for a in artifacts) == 56
    return result

reports = []
for exact in (False, True):
    v = graph(exact)
    whole = count(v)
    whole['bytes'] += 1  # One final LF; no BOM.
    whole['preflight_nodes_including_keys'] = whole['value_nodes'] + whole['key_nodes']
    components = {k:count(val)['bytes'] for k,val in v.items()}
    shell = 2 + len(v) - 1 + sum(escaped_bytes(k) + 1 for k in v) + 1
    assert whole['bytes'] == shell + sum(components.values())
    assert whole['bytes'] < 1048576
    reports.append(dict(scope='Exact selected literal path/profile geometry and success graph; every variable integral token bounded by20 decimal characters, no target serialization', static_integrals_exact=exact, top_level_fields=list(v), artifacts=67, references_with_relative_path=56, transitive_inputs=1225, predecessor_fields=6, numeric_token_domain='All built-in admitted integers need at most20 characters, including sign when present; this intentionally overbounds signed Int64 nonnegative counters.', component_value_bytes=components, top_level_keys_punctuation_and_LF=shell, bound=whole))

out = dict(scope='CONDITIONAL_SUCCESS_BUNDLE_WIRE_UPPER_NOT_ALLOCATION_FIT_OR_FINAL_UNION', manifest_sha256=hashlib.sha256(raw).hexdigest().upper(), geometry_sha256=hashlib.sha256((ROOT/'build-ci/20261105-clock-oracle-r12-path-geometry-r1/geometry.json').read_bytes()).hexdigest().upper(), hypotheses=['Exact selected root spelling and ordinary-drive/provider correspondence of the independent P argument.', 'Only the complete admitted successful graph at R12 lines981-994 reaches this bundle.', 'All artifact/reference exact-set guards have succeeded; no unknown path is admitted.', 'Pinned codec/preflight source contracts retained; no target encoding performed.'], variants=reports, bytes_not_equal_actual=True, target_execution=False)
(OUT/'bundle-bound.json').write_text(json.dumps(out, indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps([dict(exact_static_integrals=r['static_integrals_exact'], **r['bound']) for r in reports]))
