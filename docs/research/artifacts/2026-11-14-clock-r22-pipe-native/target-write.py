"""Check selected R22 target diagnostic writes through the UCRT ANSI branch."""

import hashlib
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
UCRT = Path("C:/Program Files (x86)/Windows Kits/10/Source/10.0.26100.0/ucrt/lowio/write.cpp")
UCRT_SHA = "993B67351B92E713B16B330B5F6606FC28BD19CBA751A779CA87C0C7A3CA903B"


def require(ok, description):
    if not ok:
        raise ValueError(description)


def main():
    raw = UCRT.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == UCRT_SHA, "unexpected UCRT write source")
    buffer_kib, = re.findall(rb"BUF_SIZE = (\d+) \* 1024;", raw)
    maximum = int(buffer_kib) * 1024
    ansi = raw.split(b"static write_result __cdecl write_text_ansi_nolock(", 1)[1]
    ansi = ansi.split(b"static write_result __cdecl write_text_utf16le_nolock(", 1)[0]
    for port in (b"char lfbuf[BUF_SIZE]", b"lfbuf + sizeof(lfbuf) - 1",
                 b"lfbuf_it < lfbuf_end && source_it < buffer_end", b"*lfbuf_it++ = CR;",
                 b"*lfbuf_it++ = c;", b"static_cast<DWORD>(lfbuf_it - lfbuf)",
                 b"WriteFile(os_handle, lfbuf, lfbuf_length, &written, nullptr)"):
        require(ansi.count(port) == 1, "ANSI translation loop changed")
    require(ansi.count(b"WriteFile(") == 1 and maximum == 5120, "ANSI write submission changed")

    outputs = (HERE.parents[1] / "2026-11-14-clock-r22-target-output-resources.md").read_text(encoding="utf-8")
    require("1,064 and 2,662 bytes" in outputs and "1,294 bytes" in outputs, "target diagnostic bounds changed")
    limits = {"producer_stdout": 1064, "verifier_stdout": 2662, "target_stderr": 1294}
    for channel, length in limits.items():
        write_bytes = min(length, maximum)
        print(f"{channel}: at most {write_bytes:,} submitted bytes in one selected write, {write_bytes + 48:,} with queue entry")
    print(f"PASS: ANSI translation submits at most {maximum:,} bytes per WriteFile")


if __name__ == "__main__":
    main()
