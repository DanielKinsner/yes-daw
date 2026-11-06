"""Bound the selected decimal20 branch and FileInfo fields from pinned source."""
from pathlib import Path
import hashlib
import json
import math

here = Path(__file__).resolve().parent
root = here.parents[1]
assert not (here / 'small-owner-components.json').exists()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
big = (here / 'BigInteger.cs').read_text()
number = (here / 'Number.BigInteger.cs').read_text()
utils = (here / 'BigIntegerCalculator.Utils.cs').read_text()
assert 'int StackAllocThreshold = 64;' in utils
assert 'BigIntegerParseNaiveThreshold = 1233' in number
assert 'public const int MaxPartialDigits = 9;' in number
assert 'if (value.Length < 255)' in number
assert 'buffer = stackalloc byte[value.Length + 1 + 1]' in number
assert '_bits = value.ToArray();' in big
assert 'internal readonly int _sign;' in big and 'internal readonly uint[]? _bits;' in big
assert 'return new string(_chars, _startIndex, _length);' in (here / 'StringReference.cs').read_text()
assert 'private readonly object' not in (here / 'MarshalByRefObject.cs').read_text()
filetime = (here / 'Interop.FILE_TIME.cs').read_text()
assert 'internal uint dwLowDateTime;' in filetime and 'internal uint dwHighDateTime;' in filetime
attrs = (here / 'Interop.WIN32_FILE_ATTRIBUTE_DATA.cs').read_text()
assert attrs.count('internal FILE_TIME ') == 3
assert 'internal int dwFileAttributes;' in attrs and attrs.count('internal uint nFileSize') == 2

def a8(n):
    return (n + 7) // 8 * 8

def array(n, width):
    return a8(24 + n * width)

def string(n):
    return a8(22 + 2 * n)

# Integer-only interval proof; no target parser or BigInteger code is run.
lo = 2**63
hi = 2**64 - 1
assert len(str(hi)) == 20
assert 2**32 <= lo <= hi < 2**64
assert (hi.bit_length() + 31) // 32 == 2
assert math.ceil(20 / 9) == 3
assert int(0.10381025297 * 20) + 1 + 2 == 5
assert 3 <= 64 and 5 <= 64 and 20 <= 1233
b20 = dict(copied_decimal_token_String20=string(20), result_uint2=array(2, 4),
           JsonTextReader_returned_BigInteger_box_field_slot_upper=32,
           JsonWriter_to_JTokenWriter_BigInteger_box_field_slot_upper=32)
assert sum(b20.values()) == 160
assert 64 + sum(b20.values()) == 224 < 344 < 1112
result = dict(schema='SELECTED_BIG_INTEGER20_AND_FILEINFO_SOURCE_COMPONENTS_R1',
    status='SOURCE_CONDITIONAL_COMPONENT_PROPOSAL_NOT_ACTUAL_LAYOUT_OR_GLOBAL_FIT',
    BigInteger20=dict(domain='Only decimal positive Int64-overflow integers in[2^63,2^64-1], already admitted by unchanged raw integer-range grammar; no negative overflow/hex/exponent/fraction/new domain',
        extra_heap_beyond_JValue64=b20, b20=sum(b20.values()), JValue_plus_b20=224,
        subsumed_by_child_scalar_envelope344=True, subsumed_by_report_scalar_envelope1112=True,
        explicit_stackalloc_peak_bytes=22 + 2 * 64 * 4,
        stack_note='534 explicit stack bytes across the synchronous parse chain, not per-node cumulative heap; ordinary CLR/native frames/metadata remain separately qualified',
        once_BigInteger_static_uint1=array(1, 4),
        later_ConvertTaskControlOperand_UInt64Max_constructor_bits=array(2, 4),
        later_control_constructor_not_in_b20=True,
        remaining='Selected Release/runtime/source/binary association and ordinary shared NumberFormatInfo/culture/type metadata are explicit profile premises, not allocated-per-token0 or total heap/RSS proof. No target parse or allocation probe.'),
    FileInfo=dict(MarshalByRefObject_instance_fields=0, FileSystemInfo_reference_fields=4,
        FileSystemInfo_bool_and_int_fields=2, WIN32_FILE_ATTRIBUTE_DATA_equivalent_DWORD_fields=9,
        attribute_value_field_slot_upper=72,
        entire_FileInfo_field_slot_upper=16 + 6 * 8 + 72,
        Name_lazy_not_requested_by_Length=True,
        referent_strings_and_Path_normalization_separate=True,
        qualification='Selected Windows source/base association with x64 header16 and <=8 per scalar/ref slot.136 is an upper, not measured object size/offset; all actual FileInfo occasions and source-normalization/stream/error owners still join V/P.'),
    no_target_execution=True)
paths = sorted(p for p in here.iterdir() if p.is_file())
paths += [root / p for p in [
    'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextReader.cs',
    'build-ci/20261106-clock-r12-writer-family-r1/primary/JsonWriter.cs',
    'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JTokenWriter.cs',
    'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JValue.cs',
    'build-ci/20261105-clock-oracle-r12-path-geometry-r1/primary/FileInfo.cs',
    'build-ci/20261105-clock-oracle-r12-path-geometry-r1/primary/FileSystemInfo.cs',
    'build-ci/20261105-clock-oracle-r12-path-geometry-r1/primary/FileSystemInfo.Windows.cs',
    'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1']]
result['input_pins'] = [dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in paths]
(here / 'small-owner-components.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(b20=sum(b20.values()), JValue_plus_b20=224, explicit_stackalloc_peak=534, FileInfo_field_slot_upper=136, component_sha256=digest((here / 'small-owner-components.json').read_bytes()))))
