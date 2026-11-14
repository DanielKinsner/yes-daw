"""Read stack and storage facts from already-compiled R22 COFF objects."""

import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct


HERE = Path(__file__).resolve().parent
RECEIPT = HERE.parent / "2026-11-14-clock-supervisor-abi/r22-object-result.json"
CHILD_RECEIPT = HERE.parent / "2026-11-14-clock-r22-child-object/result.json"


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def checked_bytes(data, offset, size):
    require(0 <= offset <= len(data) and 0 <= size <= len(data) - offset, "COFF range")
    return data[offset:offset + size]


def unwind(data):
    version_flags, _, count, _ = struct.unpack("<4B", checked_bytes(data, 0, 4))
    flags = version_flags >> 3
    require(version_flags & 7 == 1 and flags in (0, 1, 2, 3, 4), "unexpected x64 unwind profile")
    codes = checked_bytes(data, 4, count * 2)
    consumed = 0
    stack_bytes = 0
    while consumed < count:
        _, descriptor = struct.unpack_from("<2B", codes, consumed * 2)
        opcode, info = descriptor & 15, descriptor >> 4
        consumed += 1
        if opcode == 0:  # PUSH_NONVOL
            stack_bytes += 8
        elif opcode == 1:  # ALLOC_LARGE
            require(info in (0, 1), "invalid ALLOC_LARGE form")
            fmt, size = ("<H", 2) if info == 0 else ("<I", 4)
            (alloc,) = struct.unpack(fmt, checked_bytes(codes, consumed * 2, size))
            stack_bytes += alloc * (8 if info == 0 else 1)
            consumed += size // 2
        elif opcode == 2:  # ALLOC_SMALL
            stack_bytes += info * 8 + 8
        elif opcode == 3:  # SET_FPREG
            pass
        elif opcode in (4, 8):  # SAVE_NONVOL or SAVE_XMM128
            consumed += 1
        elif opcode in (5, 9):  # SAVE_NONVOL_FAR or SAVE_XMM128_FAR
            consumed += 2
        else:
            raise ValueError(f"unsupported unwind opcode: {opcode}")
    require(consumed == count, "truncated unwind code operands")
    extra_at = 4 + 2 * ((count + 1) & ~1)
    if flags == 4:
        checked_bytes(data, extra_at, 12)
        require(stack_bytes == 0, "chained unwind record has its own RSP growth")
    elif flags:
        checked_bytes(data, extra_at, 4)  # Language handler address.
    return flags, stack_bytes


def code_and_relocations(raw, sections, symbols, prefix):
    matches = [symbol for symbol in symbols.values()
               if symbol[0] == prefix or symbol[0].startswith(prefix + "@")]
    require(len(matches) == 1, f"expected a single {prefix} symbol")
    label, start, section, _ = matches[0]
    name, size, data, reloc, count = sections[section]
    require(name in (".text$mn", ".text$x") and start == 0, f"unexpected {prefix} code section")
    refs = {}
    for index in range(count):
        offset, symbol, kind = struct.unpack_from("<IIH", raw, reloc + index * 10)
        require(offset + 4 <= size and offset not in refs, f"invalid {prefix} relocation")
        refs[offset] = kind, symbols[symbol]
    return label, checked_bytes(raw, data, size), refs


def check_child(raw, sections, symbols, result):
    _, code, refs = code_and_relocations(raw, sections, symbols, "main")
    require(len(code) == 94 and code.startswith(bytes.fromhex("48 8b c4 48 81 ec e8 00 00 00")),
            "guarded target main range changed")
    require(list(refs) == [34, 43, 50, 63] and all(kind == 4 for kind, _ in refs.values()),
            "guarded target main relocations changed")
    labels = [symbol[0] for _, symbol in refs.values()]
    require(labels[1:] == ["??0Rejection@clock_stage@@QEAA@PEBDV?$array@_K$05@std@@@Z",
                           "_TI2?AURejection@clock_stage@@", "_CxxThrowException"],
            "guarded target main does not construct and throw Rejection")
    _, start, section, _ = refs[34][1]
    name, size, data, _, _ = sections[section]
    literal = b"SOURCE_HOLD_NO_CLOCK_EXECUTION_GO\0"
    require(name == ".rdata" and start + len(literal) == size
            and checked_bytes(raw, data + start, len(literal)) == literal, "target guard literal changed")
    require(code[31:34] == bytes.fromhex("48 8d 15") and all(code[at - 1] == 0xE8 for at in (43, 63))
            and code[47:50] == bytes.fromhex("48 8d 15")
            and code[67:] == bytes.fromhex("90 b8 02 00 00 00 48 81 c4 e8 00 00 00 c3 "
                                           "b8 03 00 00 00 48 81 c4 e8 00 00 00 c3"),
            "target guard and refusal continuations changed")

    for name, size, report_at in (("main$catch$10", 147, 109), ("main$catch$11", 91, 63)):
        _, caught, catch_refs = code_and_relocations(raw, sections, symbols, name)
        require(len(caught) == size and catch_refs[report_at][1][0] == "fprintf"
                and catch_refs[report_at + 5][1][0] == "?reportCleanupClose@clock_stage@@YAXXZ",
                f"target {name} diagnostic path changed")

    run, code, refs = code_and_relocations(raw, sections, symbols, "?run@clock_stage")
    reader, _, _ = code_and_relocations(raw, sections, symbols, "?readText@clock_stage")
    call_sites = [at for at, (kind, symbol) in refs.items()
                  if kind == 4 and symbol[0] == reader and code[at - 1] == 0xE8]
    require(call_sites == [7184, 10037], "target run-to-readText call sites changed")

    expected = {"$unwind$" + label: width for label, width in
                (("main", 232), (run, 341864), (reader, 196744))}
    records = result["unwind_records"]
    observed = {row["unwind_symbol"]: row["rsp_decrement"] for row in records if not row["chained"]}
    require(len(records) == 349 and sum(row["chained"] for row in records) == 133
            and all(observed.get(label) == width for label, width in expected.items()),
            "target unwind profile changed")
    result["guarded_child_main"] = {"bytes": 94, "relocations": list(zip([34, 43, 50, 63], labels)),
                                    "rsp_decrement": observed["$unwind$main"]}
    result["child_run_read_text"] = {"call_sites": call_sites, "run_rsp_decrement": observed["$unwind$" + run],
                                    "read_text_rsp_decrement": observed["$unwind$" + reader]}


def read_object(path, pin, *, child=False):
    require(20 <= path.stat().st_size == pin["object_bytes"] <= 16 * 1024 * 1024,
            "unexpected object extent")
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == pin["object_sha256"], "object hash differs")
    machine, section_count, _, table, slots, optional, _ = struct.unpack_from("<HHIIIHH", raw)
    require(machine == 0x8664 and optional == 0 and section_count == pin["section_count"] > 0,
            "expected pinned AMD64 COFF section count")
    require(20 + section_count * 40 <= table and 1 <= slots <= 65536, "invalid symbol table")
    string_at = table + slots * 18
    (string_size,) = struct.unpack("<I", checked_bytes(raw, string_at, 4))
    require(string_size >= 4, "invalid COFF string table")
    strings = checked_bytes(raw, string_at, string_size)
    sections = {}
    totals = collections.Counter()
    for index in range(section_count):
        name, _, _, size, data, reloc, _, reloc_count, _, _ = struct.unpack_from(
            "<8sIIIIIIHHI", raw, 20 + index * 40)
        name = name.rstrip(b"\0").decode("ascii")
        if size and name != ".bss":
            checked_bytes(raw, data, size)
        if reloc_count:
            checked_bytes(raw, reloc, reloc_count * 10)
        sections[index + 1] = name, size, data, reloc, reloc_count
        totals[name] += size
    symbols = {}
    index = 0
    while index < slots:
        name, value, section, _, storage, aux = struct.unpack_from("<8sIhHBB", raw, table + index * 18)
        if name.startswith(b"\0" * 4):
            start = int.from_bytes(name[4:], "little")
            require(4 <= start < len(strings), "invalid COFF name offset")
            end = strings.find(b"\0", start)
            require(end >= start, "unterminated COFF name")
            name = strings[start:end]
        symbols[index] = (name.rstrip(b"\0").decode("ascii"), value, section, storage)
        index += 1 + aux
    require(index == slots, "overrun after COFF auxiliary symbols")
    records = []
    for name, size, data, reloc, reloc_count in sections.values():
        if name != ".pdata":
            continue
        require(size == 12 and reloc_count == 3, "unexpected function table shape")
        relocations = {}
        for index in range(reloc_count):
            offset, symbol, kind = struct.unpack_from("<IIH", raw, reloc + index * 10)
            require(kind == 3 and offset not in relocations, "unexpected AMD64 relocation")
            relocations[offset] = symbols[symbol]
        require(set(relocations) == {0, 4, 8}, "incomplete function table entry")
        _, symbol_offset, unwind_index, _ = relocations[8]
        xname, xsize, xdata, _, _ = sections[unwind_index]
        require(xname == ".xdata", "unwind symbol is outside unwind section")
        _, _, addend = struct.unpack_from("<III", raw, data)
        code_offset = symbol_offset + addend
        require(code_offset % 4 == 0 and code_offset < xsize, "invalid unwind offset")
        unwind_raw = checked_bytes(raw, xdata + code_offset, xsize - code_offset)
        flags, stack_bytes = unwind(unwind_raw)
        records.append({
            "code": relocations[0][0],
            "unwind_symbol": relocations[8][0],
            "chained": flags == 4,
            "rsp_decrement": stack_bytes,
        })
    undefined = sorted(name for name, value, section, storage in symbols.values()
                       if value == 0 and section == 0 and storage == 2)
    require(len(records) == totals[".pdata"] // 12, "missing function table ranges")
    bss_symbols = sorted(name for name, _, section, storage in symbols.values()
                         if section > 0 and sections[section][0] == ".bss" and storage == 2)
    result = {"section_bytes": dict(totals), "unwind_records": records, "bss_symbols": bss_symbols,
              "object_unresolved_symbols": undefined}
    if path.name == "sup_main.obj":
        mains = [symbol for symbol in symbols.values() if symbol[0].startswith("?supervisor_main@")]
        require(len(mains) == 1, "expected a single supervisor_main")
        _, start, section, _ = mains[0]
        name, size, data, reloc, count = sections[section]
        require(name == ".text$mn" and start == 0 and size == 80, "guarded entry range changed")
        refs = []
        for index in range(count):
            offset, symbol, kind = struct.unpack_from("<IIH", raw, reloc + index * 10)
            require(kind == 4, "unexpected entry relocation kind")
            refs.append((offset, symbols[symbol][0]))
        require([offset for offset, _ in refs] == [20, 30, 50, 75], "guarded entry offsets changed")
        require([label.split("@", 1)[0] for _, label in refs] ==
                ["__chkstk", "__security_cookie", "??0State", "?finish"], "guard path changed")
        code = checked_bytes(raw, data, size)
        require(code.endswith(bytes.fromhex(
            "c6 44 24 30 00 48 8d 4c 24 40 0f b6 44 24 30 ba 0a 00 00 00 e8 00 00 00 00 cc"
        )), "expected volatile read and unconditional guard exit")
        result["guarded_supervisor_main"] = {"bytes": size, "relocations": refs}
    if child:
        check_child(raw, sections, symbols, result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("objects", type=Path)
    parser.add_argument("--child", type=Path, help="verify the exact guarded R22 clock_stage.obj as well")
    args = parser.parse_args()
    pins = json.loads(RECEIPT.read_bytes())["objects"]
    require(set(pins) == {"sup_codec.cpp", "sup_core.cpp", "sup_io.cpp", "sup_main.cpp"},
            "unexpected R22 object receipt")
    facts = {Path(name).stem: read_object(args.objects / Path(name).with_suffix(".obj"), pin)
             for name, pin in sorted(pins.items())}
    if args.child is not None:
        facts["clock_stage"] = read_object(args.child, json.loads(CHILD_RECEIPT.read_bytes()), child=True)
    print(json.dumps(facts, indent=2))


if __name__ == "__main__":
    main()
