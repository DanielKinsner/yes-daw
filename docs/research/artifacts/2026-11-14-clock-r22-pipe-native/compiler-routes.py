"""Verify selected diagnostic and inheritance paths in the pinned MSVC driver."""

import json
from pathlib import Path
import runpy
import struct
import uuid


HERE = Path(__file__).resolve().parent
GUID = uuid.UUID("eda606cb-8b25-4790-89f3-4b8a52205a2a")
PDB_PIN = "86C512D4F1D55EA9CC0C0C427638B70050E8AD5DAB6FD329E9D9354EEFAD4147"


def main():
    nt = runpy.run_path(str(HERE / "check.py"))
    recipe = runpy.run_path(str(HERE / "compiler-imports.py"))
    require = nt["require"]
    manifest_raw = recipe["MANIFEST"].read_bytes()
    require(recipe["sha"](manifest_raw) == recipe["MANIFEST_SHA"], "R22 manifest changed")
    manifest = json.loads(manifest_raw)
    fields = manifest["compiler"]
    filename = Path(fields["path"])
    row, = [row for row in manifest["inputs"] if Path(row["path"]) == filename]
    require(row["sha256"] == fields["sha256"], "compiler pin differs")
    raw = nt["pinned"](filename, row["bytes"], row["sha256"])
    sections, read = nt["image"](raw, GUID, b"cl.pdb")
    names = {"?_write_helper@@YAXHPEBG@Z": 0x10C18, "?start_server@@YAPEAVProcessChannel@@H@Z": 0x18B08,
             "__imp_CreateProcessW": 0x451C0, "__imp_GetStdHandle": 0x451D8, "__imp_WriteFile": 0x451E0,
             "__imp_GetEnvironmentVariableW": 0x45090, "__imp__write": 0x45950}
    symbols = nt["symbols"](nt["pinned"](nt["PDB"].with_name("cl.pdb"), 905216, PDB_PIN), sections, names, GUID)
    require(symbols == names, "unexpected compiler route symbols")

    def verify(address, code):
        expected = bytes.fromhex(code)
        require(read(address, len(expected)) == expected, f"compiler code changed at {address:x}")

    def indirect(address, label):
        word, delta = struct.unpack("<Hi", read(address, 6))
        require(word == 0x15FF and address + 6 + delta == symbols[label], f"unexpected {label} dispatch")

    # fd 1/2 branch and startup variable read. A nonzero sizing result
    # selects the separate handle route before the normal CRT conversion.
    verify(0x10C43, "8d 41 ff 45 33 e4 48 8b f2 44 8b f1 83 f8 01 0f 87 98 68 01 00")
    verify(0x10C77, "45 33 c0 48 8d 0d 1f 52 03 00 33 d2")
    indirect(0x10C83, "__imp_GetEnvironmentVariableW")
    verify(0x10C89, "85 c0 0f 85 7a 68 01 00")
    key = "VS_UNICODE_OUTPUT"
    require(read(0x45EA0, 2 * (len(key) + 1)) == (key + "\0").encode("utf-16-le")
            and key.casefold() not in {key.casefold() for key in manifest["environment"]}, "driver environment branch changed")
    indirect(0x10D07, "__imp__write")
    verify(0x2752E, "48 83 cf ff 48 ff c7 66 44 39 24 7e 75 f6 44 8d 04 3f")
    indirect(0x27550, "__imp_WriteFile")

    # start_server populates STARTUPINFO with all three standard handle
    # values and sets the fifth CreateProcessW parameter to TRUE.
    verify(0x18B73, "c7 45 ec 00 01 00 00")
    verify(0x18B80, "b9 f6 ff ff ff")
    indirect(0x18B85, "__imp_GetStdHandle")
    verify(0x18B8B, "b9 f5 ff ff ff 48 89 45 00")
    indirect(0x18B94, "__imp_GetStdHandle")
    verify(0x18B9A, "b9 f4 ff ff ff 48 89 45 08")
    indirect(0x18BA3, "__imp_GetStdHandle")
    verify(0x18BAE, "48 89 45 10")
    verify(0x18D82, "48 8d 45 b0 48 89 44 24 40 4c 89 64 24 38 48 89 5c 24 30 "
          "c7 44 24 28 00 04 00 00 c7 44 24 20 01 00 00 00")
    indirect(0x18DA5, "__imp_CreateProcessW")
    print("PASS: selected MSVC driver diagnostic and inheriting server paths match exact binary and symbols")


if __name__ == "__main__":
    main()
