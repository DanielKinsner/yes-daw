"""Fresh whole-purpose source/data checks. No proposal helper/decoder is run."""
from pathlib import Path
import hashlib
import json
import re
import traceback

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
SUB = ROOT / 'build-ci/20261106-native-object-layout-purpose-source-r1'
PIN_SHA = '7B8EF452D04B97D38B92D1E790FA61B355660A141FA4CB1D21A1002C4B3EA165'
checks = []
inputs = []
sha = lambda raw: hashlib.sha256(raw).hexdigest().upper()


def ck(name, yes):
    checks.append({'name': name, 'pass': bool(yes)})
    if not yes:
        raise AssertionError(name)


def main():
    raw = (SUB / 'source-pins.json').read_bytes()
    ck('exact_subject_seal', sha(raw) == PIN_SHA)
    pins = json.loads(raw)
    for i, row in enumerate(pins['files'] + pins['inputs']):
        path = Path(row['path'])
        if not path.is_absolute():
            path = ROOT / path
        content = path.read_bytes()
        ck('input_' + str(i), len(content) == row['bytes'] and sha(content) == row['sha256'])
    ck('input_inventory', len(pins['files']) == 22 and len(pins['inputs']) == 531)
    cpp = (SUB / 'layout.cpp').read_text(encoding='utf-8')
    replicas = (SUB / 'layout-types.h').read_text(encoding='utf-8')
    schema = json.loads((SUB / 'schema.json').read_bytes())
    correspondence = json.loads((SUB / 'correspondence.json').read_bytes())
    source_cpp = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1/clock_stage.cpp'
    original = source_cpp.read_text(encoding='utf-8')
    entries = schema['entries']
    pairs = re.findall(r'^ static_cast<std::uint64_t>\((.*)\), // (.*)$', cpp, re.M)
    ck('all_emitted_cells_exact_schema', pairs == [(x['expression'], x['name']) for x in entries])
    ck('194_unique_cells', len(entries) == len({x['name'] for x in entries}) == 194)
    kinds = {kind: [x for x in entries if x['kind'] == kind] for kind in ('sizeof', 'alignof', 'public-standard-layout-offset', 'profile-constant')}
    ck('complete_cell_classes', [len(kinds[k]) for k in kinds] == [67, 67, 55, 5])
    for row in kinds['sizeof']:
        ck('size_align_pair:' + row['name'], any(x['name'] == row['name'].replace('.sizeof', '.alignof') and x['expression'] == row['expression'].replace('sizeof(', 'alignof(', 1) for x in kinds['alignof']))
    for row in kinds['public-standard-layout-offset']:
        typ = row['expression'].removeprefix('offsetof(').split(',')[0]
        ck('legal_offset_guard:' + row['name'], 'static_assert(std::is_standard_layout_v<' + typ + '>);' in cpp)
    ck('no_private_offset_substitute', not any(any(word in x['name'] for word in ['Rejection.offsetof', 'CachedPort.offsetof', 'NumericAClock.offsetof', 'Production.offsetof', 'Catalogue.offsetof']) for x in entries))
    ck('exact_replica_inventory', [x['name'] for x in correspondence['replicas']] == ['Rejection', 'CleanupCloseState', 'Cost', 'Meter', 'Text', 'Stream', 'Pair128', 'FractionCache', 'CachedPort', 'Picks', 'Bounds'])
    for row in correspondence['replicas']:
        definition = row['original_definition']
        ck('whole_original_definition:' + row['name'], definition in original and sha(definition.encode()) == row['original_definition_sha256'])
        ck('whole_original_source:' + row['name'], row['source_whole_sha256'] == sha(source_cpp.read_bytes()))
        ck('ordered_storage_declared:' + row['name'], all(x in definition and x in replicas for x in row['storage_declarations']))
    ck('retained_virtual_base', 'struct Rejection:std::exception' in replicas and 'const char* what()const noexcept override{return reason;}' in replicas)
    ck('retained_copy_destructor_properties', '~Stream()noexcept;' in replicas and 'Stream(const Stream&)=delete;Stream& operator=(const Stream&)=delete;' in replicas)
    ck('retained_mixed_access_reference', 'private:\n std::size_t segmentIndex_=0;unsigned cursorAdvances_=0;\npublic:' in replicas and 'Meter& meter;' in replicas)
    ck('no_clock_TU_or_Data_or_entry', 'clock_stage.cpp' not in cpp and 'Catalogue::reviewed_' not in cpp and not re.search(r'\bmain\s*\(', cpp))
    ck('constant_readonly_output', '#pragma section(".ylayout",read)' in cpp and 'extern constinit const std::uint64_t yesdaw_layout_cells[]' in cpp)
    ck('no_new_pack_or_budget', '#pragma pack' not in cpp + replicas and '32768' not in cpp and '2097152' not in cpp)
    profile = json.loads((SUB / 'profile.json').read_bytes())
    args = profile['compiler']['arguments']
    exact_args = ['/nologo','/std:c++20','/EHsc','/W4','/WX','/MD','/permissive-','/fp:strict','/O2','/D_ITERATOR_DEBUG_LEVEL=0','/I<REPO>/src','/c','<SOURCE>','/Fo<OBJECT>','/sourceDependencies','<DEPS>']
    ck('exact_compile_only_recipe', args == exact_args)
    old_manifest = json.loads((source_cpp.parent / 'source-manifest.json').read_bytes())
    ck('all_ABI_flags_in_original_recipe', all(flag in old_manifest['compiler']['arguments'] for flag in exact_args[:10]))
    ck('no_ambient_build_flags', set(profile['environment']) == {'SystemRoot','windir','PATH','INCLUDE','TEMP','TMP','VSLANG'})
    ck('no_actual_authorization_or_attempt', not (SUB / 'compile-authorization.json').exists() and not (SUB / 'execution-r1').exists())
    for name in ('source-pins.json','layout.cpp','layout-types.h','schema.json','correspondence.json','profile.json','launch_compile.py','decode_coff.py','prepare_source.py','README.md'):
        path = SUB / name
        content = path.read_bytes()
        inputs.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': len(content), 'sha256': sha(content)})
    path = ROOT / 'build-ci/20261106-native-layout-root-launch-r1.py'
    content = path.read_bytes()
    ck('thin_caller_seal', sha(content) == '7E471316268BDE6A0D9816E9EFA248BDE9D50EA4F0FC0675286A584179497595')
    inputs.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': len(content), 'sha256': sha(content)})
    path = Path(__file__)
    content = path.read_bytes()
    inputs.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': len(content), 'sha256': sha(content)})
    report = {'verdict': 'ACCEPT_COMPILE_ONLY_PURPOSE_SOURCE', 'source_pins_sha256': PIN_SHA, 'whole_manual_review': 'review.md', 'checks': checks, 'check_count': len(checks), 'inputs': inputs, 'execution_authorized': False, 'required_before_compilation': 'independent thin caller review and root separate authorization', 'accepted_scope': 'source design for measured constant compiler type/layout values only; no native program or Clock execution, physical frame/RSS/fit, prices or Data'}
    (OUT / 'review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    (OUT / 'source-pins.json').write_text(json.dumps({'schema': 'INDEPENDENT_NATIVE_LAYOUT_PURPOSE_REVIEW_PINS_R1', 'inputs': inputs}, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'verdict': report['verdict'], 'checks': len(checks), 'source_pins_sha256': PIN_SHA}))


if __name__ == '__main__':
    try:
        main()
    except BaseException:
        (OUT / 'failed-checks.json').write_text(json.dumps({'checks': checks, 'traceback': traceback.format_exc()}, indent=2) + '\n', encoding='utf-8', newline='\n')
        (OUT / 'failed-checker.py').write_bytes(Path(__file__).read_bytes())
        raise
