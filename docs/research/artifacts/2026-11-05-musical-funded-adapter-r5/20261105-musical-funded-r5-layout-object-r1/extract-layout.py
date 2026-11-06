"""Extract compiler-emitted public sizeof/alignof constants; never run an object."""
from pathlib import Path
import hashlib
import json
import struct

BASE = Path(__file__).resolve().parent
PLAN = BASE / "source-plan.json"
SOURCE = BASE / "layout.cpp"
OBJECT = BASE / "attempt-r2/layout.obj"
OUTPUT = BASE / "layout-record.json"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


if not __debug__:
    raise RuntimeError("UNOPTIMIZED_PYTHON_REQUIRED")
assert not OUTPUT.exists(), "Preserve the first completed extraction"
plan = json.loads(PLAN.read_text(encoding="utf-8"))
assert sha(SOURCE) == plan["source_sha256"]
receipt = json.loads((BASE / "attempt-r2/receipt.json").read_text(encoding="utf-8"))
assert receipt["exit"] == 0 and receipt["stages"]["compile"]["raw_exit"] == 0
assert receipt["target_executed"] is False and sha(OBJECT) == receipt["object_sha256"]
data = OBJECT.read_bytes()
machine, count, timestamp, symbols_at, symbols_count, optional, flags = struct.unpack_from("<HHIIIHH", data)
assert machine == 0x8664 and optional == 0 and 1 <= count <= 64
assert 20 + 40 * count <= len(data)
assert symbols_at + 18 * symbols_count + 4 <= len(data)
strings_at = symbols_at + 18 * symbols_count
strings_size = struct.unpack_from("<I", data, strings_at)[0]
assert strings_size >= 4 and strings_at + strings_size <= len(data)
sections = []
emitted_instruction_bytes = 0
for index in range(count):
    fields = struct.unpack_from("<8sIIIIIIHHI", data, 20 + 40 * index)
    name = fields[0].rstrip(b"\0").decode("ascii")
    raw_size, raw_at, relocations, section_flags = fields[3], fields[4], fields[7], fields[9]
    assert raw_at + raw_size <= len(data)
    if section_flags & 0x20:
        emitted_instruction_bytes += raw_size
    assert not name.startswith(".CRT$"), "No initializer section allowed"
    sections.append((name, raw_at, raw_size, relocations, section_flags))
found = []
index = 0
while index < symbols_count:
    name_bytes, value, section, kind, storage, auxiliaries = struct.unpack_from("<8sIhHBB", data, symbols_at + 18 * index)
    if name_bytes[:4] == b"\0\0\0\0":
        offset = struct.unpack_from("<I", name_bytes, 4)[0]
        assert 4 <= offset < strings_size
        end = data.find(b"\0", strings_at + offset, strings_at + strings_size)
        assert end >= 0
        name = data[strings_at + offset:end].decode("utf-8")
    else:
        name = name_bytes.rstrip(b"\0").decode("ascii")
    if name == plan["table_symbol"]:
        assert storage == 2 and kind == 0 and 1 <= section <= count
        found.append((value, section))
    assert index + auxiliaries < symbols_count
    index += 1 + auxiliaries
assert len(found) == 1
value, section = found[0]
name, raw_at, raw_size, relocations, section_flags = sections[section - 1]
assert name == ".rdata" and relocations == 0
assert section_flags & 0x40 and section_flags & 0x40000000
assert not section_flags & (0x20 | 0x20000000 | 0x80000000)
names = plan["types_in_order"]
words = 2 + 2 * len(names)
assert value + 8 * words <= raw_size
values = struct.unpack_from("<" + "Q" * words, data, raw_at + value)
assert values[0] == int(plan["magic_hex"], 16) and values[1] == len(names)
rows = []
for index, name in enumerate(names):
    size, alignment = values[2 + 2 * index:4 + 2 * index]
    assert size > 0 and alignment > 0 and alignment & (alignment - 1) == 0
    assert size % alignment == 0
    rows.append({"type": name, "sizeof": size, "alignof": alignment})
record = {"schema": "R5_PUBLIC_TYPE_COMPILER_CONSTANT_LAYOUT_R1",
          "scope": "Actual selected MSVC object constants only; no target execution or hardware claim",
          "compile_raw_exit": 0, "object_sha256": sha(OBJECT),
          "source_sha256": sha(SOURCE), "source_plan_sha256": sha(PLAN),
          "source_manifest_sha256": plan["target_source_manifest_sha256"],
          "extractor_sha256": sha(Path(__file__)), "coff_machine": "AMD64",
          "table_section": ".rdata", "table_relocations": 0, "emitted_instruction_bytes": emitted_instruction_bytes,
          "types": rows, "target_executed": False,
          "not_claimed": "No offsets, allocation/retained peaks, prices, cap fit, native/forward/whole API proof or query GO"}
OUTPUT.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8", newline="\n")
freeze = {"scope": "Whole compile-only layout source, plan, both wrappers, extractor and resulting table record",
          "files": [{"path": name, "sha256": sha(BASE / name)} for name in
                    ("layout.cpp", "source-plan.json", "compile-only.ps1", "compile-only-r2.ps1", "extract-layout.py", "layout-record.json")]}
(BASE / "first-green-layout-source-freeze.json").write_text(json.dumps(freeze, indent=2) + "\n", encoding="utf-8", newline="\n")
print(json.dumps({"types": len(rows), "record_sha256": sha(OUTPUT), "layouts": rows}))
