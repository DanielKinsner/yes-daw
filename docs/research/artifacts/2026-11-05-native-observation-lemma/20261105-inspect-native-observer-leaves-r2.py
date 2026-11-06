"""Read-only bounded candidate SDK file inspection; never invokes target leaves."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
output = root / 'build-ci/20261105-native-observer-candidate-leaves-file-r2'
assert not output.exists(), 'Preserve each file-inspection attempt'
tool = Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/dumpbin.exe')
candidate = Path('C:/Windows/System32/ucrtbase.dll')
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
assert digest(tool.read_bytes()) == '12A1CD87238BD66DFDB788B4FCDCB91CE4B3F81236AAB12A7A29E5AE1D85AF50'
raw = candidate.read_bytes()
assert digest(raw) == 'DEDB7611E1ABC1FF3852DCAC3D4C93A048D729E25B061973B9969F22698F14CA'
pe = struct.unpack_from('<I', raw, 60)[0]
assert raw[:2] == b'MZ' and raw[pe:pe+4] == b'PE\0\0'
assert struct.unpack_from('<H', raw, pe+4)[0] == 0x8664
optional = pe+24
assert struct.unpack_from('<H', raw, optional)[0] == 0x20B
base = struct.unpack_from('<Q', raw, optional+24)[0]
nsections = struct.unpack_from('<H', raw, pe+6)[0]
sections = optional + struct.unpack_from('<H', raw, pe+20)[0]
def file_offset(rva):
    for at in range(sections, sections+40*nsections, 40):
        size, va, stored, offset = struct.unpack_from('<IIII', raw, at+8)
        if va <= rva < va+max(size, stored):
            assert rva-va < stored
            return offset + rva-va
    raise AssertionError('RVA_OUTSIDE_RAW_SECTION')
exception_rva, exception_size = struct.unpack_from('<II', raw, optional+112+8*3)
assert exception_size % 12 == 0
exception_offset = file_offset(exception_rva)
functions = [struct.unpack_from('<III', raw, at) for at in range(exception_offset, exception_offset+exception_size, 12)]
exports_path = root/'build-ci/20261105-musical-query-fp-native-association-r1/ucrtbase.dll-exports.stdout.log'
exports = [(int(m[1], 16), m[2]) for m in re.finditer(r'^\s*\d+\s+[0-9A-F]+\s+([0-9A-F]{8})\s+(\S+)\s*$', exports_path.read_text(), re.M)]
selected = (('status_helper_55930',0x55930),('set_helper_558B0',0x558B0),('control_helper_5FC10',0x5FC10),('floor_target_8D6D0',0x8D6D0))
assert len(exports) > 1000
output.mkdir()
receipt = dict(scope='Candidate SDK leaf FILE inspection only; no target calls, loading, runtime association, state proof, Data or execution GO', target_functions_invoked=False, desktop_input=False, native_windows=False, devices=False, tool_sha256=digest(tool.read_bytes()), candidate_sha256=digest(raw), exports_source_sha256=digest(exports_path.read_bytes()), commands=[])
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = subprocess.SW_HIDE
for name, begin in selected:
    records = [f for f in functions if f[0] == begin]
    assert len(records) <= 1
    next_export = min(rva for rva, _ in exports if rva > begin)
    end = records[0][1] if records else min(next_export, begin+1024)
    assert begin < end <= begin+8192
    command = [str(tool), '/nologo', '/disasm', f'/range:0x{base+begin:X},0x{base+end:X}', str(candidate)]
    done = subprocess.run(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE, startupinfo=startup, creationflags=subprocess.CREATE_NO_WINDOW, timeout=30)
    (output/(name+'.disassembly.txt')).write_bytes(done.stdout)
    (output/(name+'.stderr.log')).write_bytes(done.stderr)
    receipt['commands'].append(dict(name=name, command=command, range_rva=[begin,end], exact_begin_runtime_function=bool(records), runtime_function_record=list(records[0]) if records else None, requested_range_is_not_asserted_leaf_extent=True, raw_exit=done.returncode, stdout_bytes=len(done.stdout), stdout_sha256=digest(done.stdout), stderr_bytes=len(done.stderr)))
    (output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
    assert done.returncode == 0 and not done.stderr
assert digest(candidate.read_bytes()) == receipt['candidate_sha256']
assert digest(tool.read_bytes()) == receipt['tool_sha256']
receipt['pre_post_pins_match'] = True
receipt['reader_source_sha256'] = digest(Path(__file__).read_bytes())
(output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(raw_exits=[row['raw_exit'] for row in receipt['commands']], selected=[dict(name=row['name'],range_rva=row['range_rva'],runtime_function=row['exact_begin_runtime_function']) for row in receipt['commands']], receipt_sha256=digest((output/'receipt.json').read_bytes()))))
