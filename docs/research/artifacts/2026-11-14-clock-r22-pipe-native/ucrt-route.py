"""Check the selected ANSI and binary write paths in the on-disk UCRT."""

from pathlib import Path
import runpy
import struct
import uuid


HERE = Path(__file__).resolve().parent
UCRT = Path("C:/Windows/System32/ucrtbase.dll")
UCRT_PIN = "DEDB7611E1ABC1FF3852DCAC3D4C93A048D729E25B061973B9969F22698F14CA"
PDB_PIN = "A50163C7FF628926C0D22C27D90D871D08E8BC1779AA8DF4E7DE86C990274C6F"
GUID = uuid.UUID("66a3e8df-b8b8-8614-6544-529e6a6dd4f4")


def main():
    nt = runpy.run_path(str(HERE / "check.py"))
    require = nt["require"]
    sections, read = nt["image"](nt["pinned"](UCRT, 1_399_656, UCRT_PIN), GUID, b"ucrtbase.pdb")
    expected = {"_write_nolock": 0x1C620, "__imp_WriteFile": 0xFD750}
    pdb = nt["pinned"](nt["PDB"].with_name("ucrtbase.pdb"), 2_748_416, PDB_PIN)
    require(nt["symbols"](pdb, sections, expected, GUID) == expected, "UCRT write symbols changed")

    def verify(address, code):
        raw = bytes.fromhex(code)
        require(read(address, len(raw)) == raw, f"UCRT code changed at {address:x}")

    def write_file(address):
        instruction, displacement = struct.unpack("<Hi", read(address, 6))
        require(instruction == 0x15FF and address + 6 + displacement == expected["__imp_WriteFile"],
                "UCRT write destination changed")

    # The nonconsole text branch tests sign bit FTEXT and the ANSI mode
    # byte. The other binary route passes the caller's original count.
    verify(0x1C634, "44 89 45 cc")
    verify(0x1C6F6, "42 38 5c 29 38 0f 8c 95 00 00 00")
    verify(0x1C70A, "44 8b 45 cc")
    write_file(0x1C71F)
    verify(0x1C796, "0f be 4d c8 85 c9 0f 85 b5 00 00 00 44 8b 4d cc 48 8d 4d e0 4d 8b c7 "
          "41 8b d6 e8 9b 0c 00 00")

    # The translation cursor starts at RSP+0x40, with a loop limit of
    # RSP+0x143F before another ordinary byte or a CR/LF pair.
    verify(0x1D464, "b8 50 14 00 00")
    verify(0x1D4C1, "48 8d 5c 24 40 48 3b fd 73 25 0f b6 07 48 ff c7 3c 0a 75 09 ff 46 08 c6 03 0d "
          "48 ff c3 88 03 48 ff c3 48 8d 84 24 3f 14 00 00 48 3b d8 72 d6")
    verify(0x1D4F0, "48 8d 44 24 40 44 89 7c 24 30 2b d8 4c 89 7c 24 20 44 8b c3 4c 8d 4c 24 30 "
          "48 8d 54 24 40 49 8b ce")
    write_file(0x1D511)
    extent = (0x143F - 1 + 2) - 0x40
    require(extent == 5120, "UCRT translation extent changed")
    print(f"PASS: exact Windows UCRT file matches the {extent:,}-byte ANSI write and separate caller-count binary route")


if __name__ == "__main__":
    main()
