"""Independent file/text/hash/integer review; never loads target .NET/PS code."""
from pathlib import Path
import hashlib
import json

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SUBJECT = ROOT / 'build-ci/20261106-small-runtime-owner-source-r1'
checks = []
inputs = {}

def require(name, condition):
    checks.append(dict(name=name, passed=bool(condition)))
    if not condition:
        raise ValueError(name)

def read(path):
    p = ROOT / path
    b = p.read_bytes()
    inputs[path] = dict(path=path, bytes=len(b), sha256=hashlib.sha256(b).hexdigest().upper())
    return b

def text(path):
    return read(path).decode('utf-8-sig')

def snippet(path, fragment):
    require(path + ':' + fragment[:65], fragment in text(path))

def a8(n):
    return (n + 7) // 8 * 8

pins_path = 'build-ci/20261106-small-runtime-owner-source-r1/source-pins.json'
pins_bytes = read(pins_path)
require('subject whole pins identity', hashlib.sha256(pins_bytes).hexdigest().upper() == '54B203434707D774F355925C7AE3B79ADB30C823D525D4AA07CBBAF968A78BC5')
pins = json.loads(pins_bytes)
component_path = 'build-ci/20261106-small-runtime-owner-source-r1/small-owner-components.json'
component = json.loads(read(component_path))
for manifest in [pins['files'], component['input_pins']]:
    for row in manifest:
        b = read(row['path'])
        require('pin ' + row['path'], len(b) == row['bytes'] and hashlib.sha256(b).hexdigest().upper() == row['sha256'])

local = 'build-ci/20261106-small-runtime-owner-source-r1/'
for file, fragments in {
    'Number.BigInteger.cs': ['if (value.Length < 255)', 'buffer = stackalloc byte[value.Length + 1 + 1]', 'int StackAllocThreshold', 'BigIntegerParseNaiveThreshold = 1233'],
}.items():
    # Threshold is declared in the companion, not Number.BigInteger.
    for fragment in fragments:
        if fragment != 'int StackAllocThreshold':
            snippet(local + file, fragment)
for fragment in ['int StackAllocThreshold = 64;', '#else\n        internal const']:
    snippet(local + 'BigIntegerCalculator.Utils.cs', fragment)
for fragment in ['int base1E9Length = (intDigits.Length + PowersOf1e9.MaxPartialDigits - 1) / PowersOf1e9.MaxPartialDigits;', 'int resultLength = checked((int)(digitRatio * number.Scale) + 1 + 2);', 'if (number.Scale <= BigIntegerParseNaiveThreshold)', 'Naive(base1E9, trailingZeroCount, resultBuffer);', 'result = new BigInteger(resultBuffer, number.IsNegative);', 'uint carry = MultiplyAdd(bits.Slice(0, resultLength), PowersOf1e9.TenPowMaxPartial, base1E9[i]);']:
    snippet(local + 'Number.BigInteger.cs', fragment)
for fragment in ['internal readonly int _sign;', 'internal readonly uint[]? _bits;', 'value = value[..length];', '_bits = value.ToArray();', 'private static readonly BigInteger s_bnMinInt = new BigInteger(-1, new uint[] { kuMaskHighBit });', 'public BigInteger(ulong value)']:
    snippet(local + 'BigInteger.cs', fragment)
snippet(local + 'StringReference.cs', 'return new string(_chars, _startIndex, _length);')

reader = 'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextReader.cs'
for fragment in ['string number = _stringReference.ToString();', 'numberValue = BigIntegerParse(number, CultureInfo.InvariantCulture);', 'private static object BigIntegerParse(string number, CultureInfo culture)', 'return System.Numerics.BigInteger.Parse(number, culture);']:
    snippet(reader, fragment)
writer = 'build-ci/20261106-clock-r12-writer-family-r1/primary/JsonWriter.cs'
snippet(writer, 'if (value is BigInteger integer)\n                    {\n                        WriteValue(integer);')
require('no BigInteger WriteValue overload declaration', 'public virtual void WriteValue(BigInteger' not in text(writer))
jwriter = 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JTokenWriter.cs'
for fragment in ['public override void WriteValue(object? value)', 'AddRawValue(value, JTokenType.Integer, JsonToken.Integer);', 'AddJValue(new JValue(value, type), token);']:
    snippet(jwriter, fragment)
snippet('build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JValue.cs', '_value = value;\n            _valueType = type;')

runner = 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1'
for fragment in ["$taskMagnitudeLimit=if($taskNegative){'9223372036854775808'}else{'18446744073709551615'}", "if($taskLeadingZero -and $taskDigits -ne 1){throw 'JSON_RAW_INTEGER_LEADING_ZERO'}", '[System.Numerics.BigInteger]::new([uint64]::MaxValue)']:
    snippet(runner, fragment)

lo, hi = 2**63, 2**64-1
require('all admitted overflow values have two uint limbs', 2**32 <= lo <= hi < 2**64)
require('decimal lengths bounded', len(str(lo)) == 19 and len(str(hi)) == 20)
require('base1e9 at most3', (20+8)//9 == 3)
# Exact rational bounds establish floor(digitRatio*scale)==1 or2, length<=5.
ratio_numerator = 10381025297
ratio_denominator = 100000000000
for digits in [19,20]:
    length = ratio_numerator*digits//ratio_denominator + 3
    require('resultLength ' + str(digits), length <= 5 <= 64)
    require('naive threshold ' + str(digits), digits <= 1233)
require('token64', a8(22+2*20) == 64)
require('uint2 32', a8(24+4*2) == 32)
require('two boxes upper32', a8(16+2*8) == 32)
require('b20', 64+32+2*32 == component['BigInteger20']['b20'] == 160)
require('scalar envelope', 64+160 == component['BigInteger20']['JValue_plus_b20'] == 224 < 344 < 1112)
require('explicit stack only', 22+2*64*4 == component['BigInteger20']['explicit_stackalloc_peak_bytes'] == 534)
require('static uint1', a8(24+4) == component['BigInteger20']['once_BigInteger_static_uint1'] == 32)
require('later constructor separate', component['BigInteger20']['later_control_constructor_not_in_b20'] and component['BigInteger20']['later_ConvertTaskControlOperand_UInt64Max_constructor_bits'] == 32)

fs = 'build-ci/20261105-clock-oracle-r12-path-geometry-r1/primary/'
for fragment in ['protected string FullPath', 'protected string OriginalPath', 'internal string? _name;', 'private string? _linkTarget;', 'private bool _linkTargetIsValid;']:
    snippet(fs+'FileSystemInfo.cs', fragment)
for fragment in ['private Interop.Kernel32.WIN32_FILE_ATTRIBUTE_DATA _data;', 'private int _dataInitialized = -1;']:
    snippet(fs+'FileSystemInfo.Windows.cs', fragment)
for fragment in ['public sealed class FileInfo : FileSystemInfo', 'public override string Name => _name ??= Path.GetFileName(OriginalPath);', 'return LengthCore;']:
    snippet(fs+'FileInfo.cs', fragment)
attrs=text(local+'Interop.WIN32_FILE_ATTRIBUTE_DATA.cs')
require('three embedded FILE_TIME', attrs.count('internal FILE_TIME ') == 3)
require('two filesize DWORD', attrs.count('internal uint nFileSize') == 2)
snippet(local+'Interop.FILE_TIME.cs', 'internal uint dwLowDateTime;\n            internal uint dwHighDateTime;')
require('FileInfo upper136', a8(16+6*8+9*8) == component['FileInfo']['entire_FileInfo_field_slot_upper'] == 136)
require('conditional scope', component['no_target_execution'] and 'NOT_ACTUAL_LAYOUT_OR_GLOBAL_FIT' in component['status'])

result = dict(status='PASS_INDEPENDENT_FILE_TEXT_INTEGER_CHECKS_ONLY', checks=checks, count=len(checks), inputs=list(inputs.values()), no_target_execution=True)
dest=HERE/'checks.json'
require('new output only', not dest.exists())
dest.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(status=result['status'], checks=result['count'], sha256=hashlib.sha256(dest.read_bytes()).hexdigest().upper())))
