"""Independent fixed-file/text review; no target parse, compiler or reader run."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OLD = ROOT / 'build-ci/20261105-managed-file-type-inventory-source-r3'
NEW = ROOT / 'build-ci/20261106-managed-file-type-inventory-source-r4'
checks = []
verified = {}

def check(name, value):
    checks.append(dict(check=name, passed=bool(value)))
    if not value:
        raise AssertionError(name)

def read(path, expected=None, length=None):
    data = path.read_bytes()
    digest = hashlib.sha256(data).hexdigest().upper()
    if expected:
        check('hash:' + path.name, digest == expected)
    if length is not None:
        check('length:' + path.name, len(data) == length)
    try:
        name = path.relative_to(ROOT).as_posix()
    except ValueError:
        name = path.as_posix()
    verified[name] = dict(path=name, bytes=len(data), sha256=digest)
    return data

manifest = json.loads(read(NEW / 'source-pins.json', '10E3854D27239A9BDD1BDF0F798C57578B6D629B4EA9DD7F52A30A8861A11E89', 15489))
previous = json.loads(read(OLD / 'source-pins.json', '3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2', 9455))
source = read(NEW / 'reader.cs', '542BB4126F6812ED83BBD0819A6F1464B8ECD7FFAE99FF36B22D8428EC43521F', 26861).decode('utf-8')
old = read(OLD / 'reader.cs', '7A8CCCE39C75E2C0030D01A864E2BFD55FDB81E5EA2B6A9644A8EF8B3B5B7F51', 22493).decode('utf-8')
ps = read(NEW / 'run-reader.ps1', '8069BD26AEBF064A57835D9852793A8F9266354DD1515FCA7C2E50535CFB53DC', 3147).decode('utf-8')
oldps = read(OLD / 'run-reader.ps1').decode('utf-8')
read(NEW / 'README.md', 'E3C32DAEB567D5FDF861F47501821D51BAEE76ECCDA38B9C2AA5133E96E8EFB2')
read(NEW / 'evidence/own-source-checks.json', 'AF60313AA6BD2CDAE2B80AD96CA6FA07F6C946CAF490CE78F0B020ABC8BA30CB')
prior_review = json.loads(read(ROOT / 'build-ci/20261105-managed-file-type-inventory-independent-review-r3/review.json', '5E870B52977ED4F984953DF2E78CEDD012BC7C62554398D768F9F959D1271EBD'))
check('prior-verdict', prior_review['verdict'].startswith('ACCEPT_SOURCE_ONLY'))

for group in ('target_inputs', 'reader_dependency_file_pins', 'authority_references', 'source_files'):
    for pin in manifest[group]:
        path = Path(pin['path'])
        if not path.is_absolute():
            path = ROOT / path
        read(path, pin['sha256'], pin['bytes'])
check('original-target-pins', manifest['target_inputs'][:4] == previous['target_inputs'])
check('original-dependency-pins', manifest['reader_dependency_file_pins'] == previous['reader_dependency_file_pins'])
runtime = Path('C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell')
files = {Path(p['path']).name: p for p in manifest['target_inputs']}
check('exact-eight-inputs', set(files) == {'Newtonsoft.Json.dll', 'System.Management.Automation.dll', 'System.Collections.Specialized.dll', 'System.Private.CoreLib.dll', 'System.Diagnostics.Process.dll', 'System.Security.Cryptography.dll', 'System.Text.Encoding.CodePages.dll', 'System.ComponentModel.Primitives.dll'})
check('all-targets-in-fixed-runtime', all(Path(p['path']).parent == runtime for p in manifest['target_inputs']))

start = '    private static readonly Input[] Inputs ='
end = '    private static readonly string[] OptionalCoreTypes ='
def split(text):
    a, rest = text.split(start)
    region, b = rest.split(end)
    return a, region, b
pre, region, post = split(source)
oldpre, oldregion, oldpost = split(old)
check('whole-prefix-identical', pre == oldpre)
replacements = {
    'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V4': 'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V3',
    'json.WriteNumber("required_named_types", 84);': 'json.WriteNumber("required_named_types", 22);',
    'if (requiredCount != 84 ||': 'if (requiredCount != 22 ||',
    '|| count < 85 || count > 88)': '|| count < 23 || count > 26)',
}
normalized = post
for fresh, prior in replacements.items():
    check('one-literal:' + fresh, normalized.count(fresh) == 1)
    normalized = normalized.replace(fresh, prior)
check('whole-decoder-control-and-output-identical', normalized == oldpost)

parsed = {}
for m in re.finditer(r'new\("([^"]+)", (\d+), "([0-9A-F]{64})", new\[\]\s*\{([^}]+)\}\)', region):
    name, size, digest, names = m.groups()
    check('unique-file:' + name, name not in parsed)
    parsed[name] = re.findall(r'"([^"\r\n]+)"', names)
    pin = files[name]
    check('literal-pin:' + name, int(size) == pin['bytes'] and digest == pin['sha256'])
    check('unique-types-in-file:' + name, len(parsed[name]) == len(set(parsed[name])))
    for typename in parsed[name]:
        check('fixed-name:' + name + ':' + typename, bool(re.fullmatch(r'[A-Za-z0-9_.+<>`]+', typename)))
check('all-inputs-parsed', set(parsed) == set(files))
check('literal-manifest-agreement', parsed == manifest['allowlist_by_file'])
check('84-required', sum(map(len, parsed.values())) == 84)
old_parsed = {m[1]: re.findall(r'"([^"\r\n]+)"', m[2]) for m in re.finditer(r'new\("([^"]+)", \d+, "[0-9A-F]{64}",\s*new\[\]\s*\{([^}]+)\}\)', oldregion)}
for name, types in old_parsed.items():
    check('old-type-prefix:' + name, parsed[name][:len(types)] == types)
check('22-original-62-added', sum(map(len, old_parsed.values())) == 22 and sum(map(len, parsed.values())) - 22 == 62)
check('all-count-gates', manifest['counts'] == dict(target_files=8, requested_types=84, required_named_types=84, added_fixed_required_types=62, optional_named_types=3, required_generated_selections=1, reported_types_min=85, reported_types_max=88))
check('optional-unchanged', manifest['optional_named_types'] == previous['optional_named_types'])

expected_ps = oldps
for old_value, new_value in [('3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2', '10E3854D27239A9BDD1BDF0F798C57578B6D629B4EA9DD7F52A30A8861A11E89'), ('22493', '26861'), ('7A8CCCE39C75E2C0030D01A864E2BFD55FDB81E5EA2B6A9644A8EF8B3B5B7F51', '542BB4126F6812ED83BBD0819A6F1464B8ECD7FFAE99FF36B22D8428EC43521F')]:
    check('wrapper-one-substitution:' + old_value, expected_ps.count(old_value) == 1)
    expected_ps = expected_ps.replace(old_value, new_value)
check('whole-powershell-wrapper-identical-except-three-pins', ps == expected_ps)
for required in ['FileAccess.Read, FileShare.Read', 'SHA256.HashData(file)', 'file.Position = 0;', 'PEStreamOptions.PrefetchMetadata | PEStreamOptions.LeaveOpen', 'if (!pe.HasMetadata)', 'if (!reader.IsAssembly)', 'REQUESTED_TYPE_MISSING', 'DUPLICATE_REQUESTED_TYPE', 'AMBIGUOUS_ENUMERATOR_NESTED_NAME', 'GENERIC_PARAMETER_GAP', 'TYPE_NESTING_CYCLE', 'TYPE_REFERENCE_CYCLE', 'TYPE_SPECIFICATION_CYCLE', 'if ((field.Attributes & FieldAttributes.Static) != 0) continue;', 'field.DecodeSignature(provider, context)', 'buffer.WriteByte(10);', 'return buffer.ToArray();']:
    check('whole-reviewed-boundary:' + required, required in source)
for banned in ['Assembly.Load', 'Assembly.LoadFrom', 'GetMethodBody', 'Activator.', 'DllImport', 'GetField(', 'MethodInfo', 'Marshal.OffsetOf', 'Unsafe.SizeOf', 'Process.Start', 'Environment.GetEnvironmentVariable']:
    check('no-target-execution-or-live-values:' + banned, banned not in source)
check('complete-output-before-write', source.index('byte[] output = ReadAll();') < source.index('stdout.Write(output, 0, output.Length);'))
check('arguments-refused', 'if (args.Length != 0)' in source)
check('source-files-LF', all(b'\r' not in read(NEW / name) for name in ['reader.cs', 'run-reader.ps1', 'README.md', 'source-pins.json']))
check('no-root-launcher-or-attempt-yet', not (NEW / 'run-reader.py').exists() and not any(NEW.glob('execution-*')))
check('qualifications-explicit', len(manifest['remaining_qualifications']) == 6)
report = dict(schema='FIXED_PUBLIC_FILE_METADATA_INVENTORY_R4_INDEPENDENT_WHOLE_SOURCE_REVIEW', verdict='ACCEPT_SOURCE_ONLY_PUBLIC_FILE_METADATA_R4_NOT_EXECUTION_OR_LAYOUT_QUALIFICATION', checks=len(checks), subject_manifest_sha256=hashlib.sha256((NEW / 'source-pins.json').read_bytes()).hexdigest().upper(), reader_sha256=hashlib.sha256((NEW / 'reader.cs').read_bytes()).hexdigest().upper(), whole_reader_bytes=len(source.encode()), whole_reader_lines=len(source.splitlines()), verified_files=list(verified.values()), scope='Eight pinned files;84 fixed required names plus unchanged one generated and0..3 optional. Public file declarations only, no target assembly/code/live values or resource fit. Root must separately review its hidden launcher and admit one purpose attempt; no Clock GO.', transport_note='Expected output identity must use (file,qualified_name): same qualified names can appear in multiple pinned assemblies; a flat name set would reject legitimate complete output.')
(HERE / 'review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
(HERE / 'checks.json').write_text(json.dumps(checks, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({k:v for k,v in report.items() if k != 'verified_files'}, indent=2))
