"""Check R22 capture and stage meter bounds against source and occurrence tables."""

import json
from pathlib import Path
import re
import runpy


HERE = Path(__file__).resolve().parent
METER = runpy.run_path(str(HERE.parent / "2026-11-14-clock-meter-inventory" / "check.py"))
require = METER["require"]


def check():
    inventory = METER["inventory"]()
    manifest = json.loads(METER["MANIFEST"].read_bytes())
    sources = {}
    for pin in manifest["supervisor"]["native_sources"]:
        path = METER["ROOT"] / pin["path"]
        data = path.read_bytes()
        require(METER["digest"](data) == pin["sha256"], f"{path.name} pin differs")
        sources[path.name] = data.decode()
    header, main, io = (sources[name] for name in ("sup.h", "sup_main.cpp", "sup_io.cpp"))
    polls = int(re.search(r"body_poll_limit = (\d+);", header)[1])
    stage_bytes = int(re.search(r"raw_log_limit = (\d+);", header)[1])
    fill = int(re.search(r"pipe_buffer_bytes = (\d+);", header)[1])
    terminal_bytes = int(re.search(r"terminal_bytes = (\d+);", header)[1])
    cleanup_retries = int(re.search(r"retry<=(\d+);", main)[1])
    require("state.terminal.body_polls>=body_poll_limit" in main
            and "counter(state,state.terminal.body_polls,1)" in main, "normal poll control differs")
    require("if(cleanup) (void)spend(state,Family::Consume);" in main
            and "else if(!admit_body(state,Family::Consume)) cleanup=true;" in main
            and "const bool more=ok && !cleanup" in main, "consume debit or continuation differs")
    require("if (!worker.ready_consumed) consume_ready(state,i,true);" in main
            and "worker.settled=true;" in main, "settlement acquisition differs")
    require("!admit_read(state.meter)" in main and main.count("!publish_ready(worker)") == 1,
            "worker ticket or publication differs")
    require(io.count("!admit_write(state)") == 1 and "while (at<terminal_bytes)" in io,
            "log or publication schedule differs")
    require(main.count("consume_ready(state,i,false)") == 1
            and main.count("consume_ready(state,i,true)") == 3, "consume call sites differ")
    reached_reads = stage_bytes + 2 * fill + 2 * (polls + 1) + 2
    chunks = stage_bytes // fill + 2 + 1
    short_writes = stage_bytes + 1
    scans = 1 + chunks + short_writes + 1 + terminal_bytes
    expected = {
        "StageScan": scans, "StageEntry": 10 * scans,
        "Owner": 3, "CaptureSetup": 1, "TargetResume": 1, "WorkerStart": 2,
        "WorkerRead": reached_reads + 2, "WorkerPublish": 2 * (polls + 1),
        "Coordinator": polls + 1, "Poll": polls,
        "Consume": 2 * polls + 2 * (cleanup_retries + 2) + 2,
        "WriteChunk": chunks, "WriteCall": short_writes,
    }
    multiple = {"Owner", "Consume", "WriteCall"}
    require(all(inventory["family_sites"][name] == 1 + (name in multiple) for name in expected),
            "capture meter sites differ")
    census = HERE.parents[1] / "2026-11-14-clock-r22-capture-meter-census.md"
    rows = re.findall(r"\| `(\w+)` \| ([\d,]+) \|", census.read_text(encoding="utf-8"))
    actual = {name: int(calls.replace(",", "")) for name, calls in rows}
    require(actual == expected and len(rows) == len(expected), "capture meter table differs")
    print(f"PASS: {len(expected)} R22 capture and stage meter families")


if __name__ == "__main__":
    check()
