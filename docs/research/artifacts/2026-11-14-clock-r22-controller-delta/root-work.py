"""Recompose the root caller ledger for the pinned R22 geometry."""

import hashlib
import json
from pathlib import Path
import runpy


def work():
    delta = runpy.run_path(str(Path(__file__).with_name("check.py")))
    result = delta["main"]()
    profile = result["root"]
    ledger = delta["BASELINE"].with_name("260-main-caller-component.json").read_bytes()
    require = delta["require"]
    require(hashlib.sha256(ledger).hexdigest().upper()
            == "B66FBF696A84F44C1C0BC44E975CE8EE0319264FD630A36DD9CF4BED778E0CE9", "baseline caller ledger changed")
    baseline = json.loads(ledger)
    old = {tuple(row["lines"]): row["units"] for row in baseline["rows"]}
    require(sum(old.values()) == baseline["units"] == 180799, "baseline root subtotal differs")
    require((old[952, 952], old[954, 954], old[1012, 1012], old[1017, 1017], old[1018, 1018])
            == (17 + 1220 * 40, 19 + 1220 * 22, 22 + 78 * 49, 16 + 67 * 14, 207 + 1220 * 34),
            "root row coefficients differ")
    new_input_rows = profile["transitive_rows"] - 6 - 1220
    transport = profile["transport_files"]
    additions = {
        "two guard initializations and two predicate inputs": 4 + 6,
        "two static path joins": 18,
        "function registrations": result["current"] - result["old"],
        "transport and binary retention expression": 16,
        "transport directory array slot and creation": 16,
        "input and include dictionary rows": new_input_rows * (40 + 22),
        "extra returned root rows": (profile["root_walk_max"] - 78) * 49,
        "transport expected-name loop": 10 + transport * 14,
        "transport artifact loop": 10 + transport * (5 + 9 + 26 + 4 + 31 + 30),
        "admitted binary amount and extra artifact summands": 14 + (profile["artifact_rows"] - 67) * 14,
        "transitive input rows and separate helper descriptor": new_input_rows * 34 + 43,
    }
    added = sum(additions.values())
    upper = baseline["units"] + added
    require((new_input_rows, added, upper) == (68, 7820, 188619), "root work composition differs")
    return upper, added


def main():
    upper, added = work()
    print(f"PASS: {upper:,} conditional root caller actions, including {added:,} for the R22 delta")


if __name__ == "__main__":
    main()
