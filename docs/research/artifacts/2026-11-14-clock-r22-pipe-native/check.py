"""Check the allocation path in the pinned local NPFS image as data."""

import hashlib
import os
from pathlib import Path
import struct
import uuid


DRIVER = Path("C:/Windows/System32/drivers/npfs.sys")
PDB = Path(os.environ["LOCALAPPDATA"]) / "Temp/YesDawClockR22PipeSymbols/npfs.download.pdb"
DRIVER_PIN = "7428025B18BD0ED4958295C4FCA56AB2E61A6AC83ECB7A4D858B6DEF8674CAE8"
PDB_PIN = "2100E70B4E9BD9C1541C46BC86C4C75DB5448C8927AEC3DD1448B9A1ACFA9E40"
GUID = uuid.UUID("c433acb0-ba96-c41b-aca8-3308cff250a6")


def require(condition, description):
    if not condition:
        raise ValueError(description)


def pinned(path, size, sha):
    with path.open("rb") as source:
        data = source.read(size + 1)
    require(len(data) == size and hashlib.sha256(data).hexdigest().upper() == sha, f"wrong pinned file: {path.name}")
    return data


def image(data, guid=GUID, pdb_name=b"npfs.pdb"):
    pe, = struct.unpack_from("<I", data, 60)
    require(data[:2] == b"MZ" and data[pe:pe + 4] == b"PE\0\0", "invalid PE")
    machine, sections = struct.unpack_from("<HH", data, pe + 4)
    optional_size, = struct.unpack_from("<H", data, pe + 20)
    magic, = struct.unpack_from("<H", data, pe + 24)
    require((machine, magic) == (0x8664, 0x20B) and optional_size >= 168, "expected AMD64 PE32+")
    rows = [struct.unpack_from("<8sIIII", data, pe + 24 + optional_size + 40 * i) for i in range(sections)]

    def read(rva, count):
        _, _, start, size, file_at = next(row for row in rows if row[2] <= rva < row[2] + row[3])
        offset = rva - start
        require(0 <= count <= size - offset and file_at + offset + count <= len(data), "invalid RVA extent")
        return data[file_at + offset:file_at + offset + count]

    debug_rva, debug_size = struct.unpack_from("<II", data, pe + 24 + 112 + 6 * 8)
    require(debug_size % 28 == 0, "invalid debug extent")
    record, = [row for row in struct.iter_unpack("<IIHHIIII", read(debug_rva, debug_size)) if row[4] == 2]
    codeview = read(record[6], record[5])
    name_end = codeview.index(0, 24)
    require(codeview[:24] == b"RSDS" + guid.bytes_le + struct.pack("<I", 1)
            and codeview[24:name_end].rsplit(b"\\", 1)[-1].lower() == pdb_name, "incorrect PE CodeView")
    return rows, read


def symbols(data, sections, names, guid=GUID):
    require(data[:32] == b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0", "invalid symbol file signature")
    block, _, total, dir_bytes, _, directory_map = struct.unpack_from("<6I", data, 32)
    require(block == 4096 and len(data) == block * total, "invalid PDB extent")

    def gather(indexes, size):
        require(all(0 < index < total for index in indexes), "invalid PDB block")
        return b"".join(data[index * block:(index + 1) * block] for index in indexes)[:size]

    directory = gather(struct.unpack_from(f"<{(dir_bytes + block - 1) // block}I", data, directory_map * block), dir_bytes)
    count, = struct.unpack_from("<I", directory)
    sizes = struct.unpack_from(f"<{count}I", directory, 4)
    streams = []
    cursor = 4 + 4 * count
    for size in sizes:
        blocks = 0 if size == 0xFFFFFFFF else (size + block - 1) // block
        streams.append(gather(struct.unpack_from(f"<{blocks}I", directory, cursor), size))
        cursor += 4 * blocks
    require(cursor == len(directory) and streams[1][12:28] == guid.bytes_le, "symbol directory or GUID differs")
    pdb_age, = struct.unpack_from("<I", streams[1], 8)
    dbi_age, = struct.unpack_from("<I", streams[3], 8)
    require((pdb_age, dbi_age) == (3, 1), "unexpected symbol stream ages")
    stream, = struct.unpack_from("<H", streams[3], 20)
    records = streams[stream]
    cursor = 0
    public = {}
    while cursor < len(records):
        length, kind = struct.unpack_from("<HH", records, cursor)
        end = cursor + 2 + length
        require(4 <= end - cursor <= len(records) - cursor, "invalid CodeView record")
        if kind == 0x110E:
            _, offset, section = struct.unpack_from("<IIH", records, cursor + 4)
            name = records[cursor + 14:end].split(b"\0", 1)[0].decode("ascii")
            if name in names:
                require(name not in public and 0 < section <= len(sections), "invalid or duplicate selected public symbol")
                public[name] = sections[section - 1][2] + offset
        cursor = end
    return public


def main():
    sections, read = image(pinned(DRIVER, 120264, DRIVER_PIN))
    expected = {"NpFsdWrite": 0x12540, "NpFastWrite": 0x11980, "NpCommonWrite": 0x126B0,
                "NpAddDataQueueEntry": 0x13110, "__imp_ExAllocatePool2": 0x8050}
    public = symbols(pinned(PDB, 118784, PDB_PIN), sections, expected)
    require(public == expected, "unexpected public write entries")

    def sequence(address, code, label):
        raw = bytes.fromhex(code)
        require(read(address, len(raw)) == raw, label)

    def near_call(address, destination):
        opcode, relative = struct.unpack("<Bi", read(address, 5))
        require(opcode == 0xE8 and address + 5 + relative == public[destination], f"missing call to {destination}")

    # The IRP write passes Parameters.Write.Length (r8d), which becomes ebp in
    # the common write routine. Its queue call passes ebp as argument six.
    sequence(0x12588, "44 8b 43 08", "IRP write length input")
    near_call(0x125A8, "NpCommonWrite")
    sequence(0x126CB, "41 8b e8", "common write saves full length in ebp")
    sequence(0x128BC, "8b c5 41 b9 01 00 00 00", "queue write route and kind")
    sequence(0x128DD, "89 6c 24 28 c7 44 24 20 00 00 00 00", "queue write length and mode")
    near_call(0x128E9, "NpAddDataQueueEntry")

    # Queue kind one selects checked allocation extent L + 48. Quota
    # accounting instead uses a separate min(remaining quota, remaining data).
    sequence(0x1319B, "44 8b 84 24 88 00 00 00 45 85 ed 0f 85 3f 01 00 00", "queue length and kind")
    sequence(0x132EB, "41 8d 48 30 41 3b c8 0f 83 b4 fe ff ff", "checked L plus 48")
    sequence(0x131AC, "44 8b 63 1c 44 2b 63 20 41 8b f0 2b b4 24 a0 00 00 00 8b c6 44 3b e6 41 0f 42 c4",
             "quota minimum stays separate")
    sequence(0x131EF, "0f 84 b7 00 00 00", "missing reusable cell takes allocation branch")
    sequence(0x132AC, "8b d1 b9 41 00 00 00 41 b8 4e 70 46 72", "allocation request, flags, and tag")
    opcode, relative = struct.unpack("<3si", read(0x132B9, 7))
    require(opcode == b"\x48\xff\x15" and 0x132C0 + relative == public["__imp_ExAllocatePool2"], "unexpected pool import")
    print("PASS: exact NPFS image and public symbols match the queued write allocation route")
    print("The selected allocation request is the whole write extent plus 48 bytes. Pipe quota uses a distinct minimum.")


if __name__ == "__main__":
    main()
