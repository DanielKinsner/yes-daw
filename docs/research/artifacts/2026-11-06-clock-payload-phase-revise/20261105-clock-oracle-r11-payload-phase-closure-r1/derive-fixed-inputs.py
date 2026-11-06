"""File/JSON-data arithmetic only: never imports or runs the target runner."""
from pathlib import Path
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
BASE = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1'
manifest = json.loads((BASE / 'source-manifest.json').read_bytes())

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def align8(n):
    return (n + 7) & ~7

def pow2(n):
    return 1 << (max(1, n) - 1).bit_length()

def compact(data):
    # Immutable, separately hash-pinned valid JSON DATA is the only input.
    # No SDK/PowerShell grammar or helper is invoked by this model.
    inside = escaped = False
    result = bytearray()
    for c in data:
        if inside:
            result.append(c)
            if escaped:
                escaped = False
            elif c == 92:
                escaped = True
            elif c == 34:
                inside = False
        elif c not in (9, 10, 13, 32):
            result.append(c)
            inside = c == 34
    if inside or escaped:
        raise ValueError('pinned data quoted-state mismatch')
    return bytes(result)

def lexical_spans(data):
    text = data.decode('ascii')  # exact pinned inputs are ASCII JSON wire text
    shifts = []
    strings = []
    number_widths = []
    i = 0
    while i < len(text):
        if text[i] == '"':
            start = i
            shifts.append(i + 1)  # ParseProperty/ParseString increment first
            i += 1
            escaped = False
            while i < len(text):
                c = text[i]
                i += 1
                if escaped:
                    escaped = False
                elif c == '\\':
                    escaped = True
                elif c == '"':
                    break
            else:
                raise ValueError('pinned quoted span incomplete')
            strings.append(dict(start=start, end=i, wire_units=i-start,
                                has_escape='\\' in text[start:i]))
        elif text[i] in '-0123456789':
            start = i
            shifts.append(i)  # ParseNumber calls ShiftBufferIfNeeded first
            i += 1
            while i < len(text) and text[i] in '0123456789':
                i += 1
            number_widths.append(i-start)
        else:
            i += 1
    # Beginning/end sentinels cover prefixes, terminal literals and delimiters.
    points = [0] + shifts + [len(text)]
    gaps = [(b-a, a, b) for a, b in zip(points, points[1:])]
    maximum = max(gaps)
    # Five extra characters cover every strict-grammar EnsureChars request,
    # a token's final delimiter and one sentinel slot. This is deliberately
    # looser than the exact parser read schedule; it is not a parser replay.
    window = maximum[0] + 6
    capacity = pow2(max(1024, 16*window))
    capacities = []
    c = 1024
    while c <= capacity:
        capacities.append(c)
        c *= 2
    return dict(compacted_units=len(text), shift_calls=len(shifts),
                string_tokens=len(strings), escaped_string_tokens=sum(s['has_escape'] for s in strings),
                max_string_wire_units=max(s['wire_units'] for s in strings),
                max_number_units=max(number_widths), maximum_shift_gap=maximum[0],
                maximum_shift_gap_start=maximum[1], maximum_shift_gap_end=maximum[2],
                conservative_window_units=window, capacity_upper=capacity,
                all_doubling_capacities=capacities,
                all_reader_array_requests_including_headers=sum(align8(24+2*n) for n in capacities),
                string_buffer_requests_including_header=align8(24+2*1024) if any(s['has_escape'] for s in strings) else 0)

def graph(value):
    result = dict(maps=[], arrays=[], strings=[], integer_count=0, bool_count=0, null_count=0)
    def walk(v):
        if isinstance(v, dict):
            result['maps'].append(len(v))
            for k, x in v.items():
                result['strings'].append(len(k))
                walk(x)
        elif isinstance(v, list):
            result['arrays'].append(len(v))
            for x in v:
                walk(x)
        elif isinstance(v, str):
            result['strings'].append(len(v.encode('utf-16-le'))//2)
        elif isinstance(v, bool):
            result['bool_count'] += 1
        elif v is None:
            result['null_count'] += 1
        elif isinstance(v, int):
            result['integer_count'] += 1
        else:
            raise ValueError('outside pinned integer JSON graph')
    walk(value)
    return dict(map_count=len(result['maps']), map_fields=sum(result['maps']),
                map_field_histogram={str(n):result['maps'].count(n) for n in sorted(set(result['maps']))},
                array_count=len(result['arrays']), array_elements=sum(result['arrays']),
                array_length_histogram={str(n):result['arrays'].count(n) for n in sorted(set(result['arrays']))},
                strings=len(result['strings']), string_units=sum(result['strings']),
                string_requests=sum(align8(22+2*n) for n in result['strings']),
                integer_count=result['integer_count'], bool_count=result['bool_count'], null_count=result['null_count'])

inputs = []
for name, path, expected in [
    ('bootstrap', BASE/'source-manifest.json', '52E3482AF966C562CBFB5119F145B79BC762C3EA8B17D050DC9CFF96CDF54F4D'),
    ('predecessor', ROOT/manifest['predecessor_bundle']['path'], '28909B793046C026890456FA59A841DA2B75450B60AAD5E8DD3888DFCCB23E71')]:
    if sha(path) != expected:
        raise ValueError('pinned source data changed: ' + name)
    raw = path.read_bytes()
    wire = compact(raw)
    value = json.loads(wire)
    spans = lexical_spans(wire)
    inputs.append(dict(name=name, path=path.relative_to(ROOT).as_posix(), sha256=sha(path),
                       raw_bytes=len(raw), raw_array_request=align8(24+len(raw)),
                       compacted_source_string_request=align8(22+2*len(wire)),
                       lexical=spans, graph=graph(value)))

reader = ROOT/'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextReader.cs'
result = dict(schema='R11_FIXED_DATA_READER_SOURCE_BOUND_INPUTS_R1',
              scope='Independent immutable JSON DATA lexical arithmetic, no target parser/helper/SDK execution, no observed private reader capacity or global fit.',
              reader_source=dict(path=reader.relative_to(ROOT).as_posix(), sha256=sha(reader)),
              proof_status='CONDITIONAL_SOURCE_UPPER_REQUIRES_INDEPENDENT_REVIEW', inputs=inputs,
              proof='Reader shifts before every quoted string/property (after opening quote) and number. Gap G between successive such sites, including start/end, covers every intervening literal/delimiter run and token. U=G+6 includes reached lookahead/sentinel slack. At a non-shifting site C-charPos>0.1C; C>=16U leaves >U spare, so no append replacement before the next site. At a shifting site count/positions reset. Nonappend reads retain<=5 required characters and cannot replace C>=1024. Append growth is exact doubling because required<=5. Starting at1024, C never exceeds nextPow2(max(1024,16U)); all older arrays remain charged. Source byte identity and ASCII validation make these gap facts exact for only the two pinned inputs. Generic compiler-dependency and changed manifest require separate bounds.',
              no_credit=['No physical free/reclaim', 'No whole CLR heap fit', 'No target parser or protocol execution', 'No binding/price/GO', 'No profile transfer after canonical input-union changes'])
(OUT/'fixed-inputs.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(inputs=[dict(name=r['name'], raw=r['raw_bytes'],
                                  compact=r['lexical']['compacted_units'], gap=r['lexical']['maximum_shift_gap'],
                                  cap=r['lexical']['capacity_upper'],
                                  reader=r['lexical']['all_reader_array_requests_including_headers']) for r in inputs]), indent=2))
