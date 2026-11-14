"""Read the preserved admission-enabled object. This checker never launches a tool."""

from pathlib import Path
import runpy


HERE = Path(__file__).resolve().parent
OBJECT = Path("C:/Users/Daniel Kinsner/AppData/Local/Temp/YesDawClockR22EntryFrame-attempt1/entry.obj")
PIN = {"object_bytes": 482581, "object_sha256": "E93697D751EE7617A2F9850AFD855198CA68E831C29C5A022F5BDE83A8638163",
       "section_count": 1722}


def main():
    reader = runpy.run_path(str(HERE.parent / "2026-11-14-clock-r22-coff-resource/read-object.py"))
    names = ("main", "?run@clock_stage", "?readText@clock_stage", "main$catch$10", "main$catch$11")
    result = reader["read_object"](OBJECT, PIN, code_names=names)
    require = reader["require"]
    require(sum("reviewed_@Catalogue@source_costs" in name for name in result["object_unresolved_symbols"]) == 1,
            "expected one undefined catalogue")
    entry, = [row for row in result["unwind_records"] if row["unwind_symbol"] == "$unwind$main"]
    require(entry["code"] == "$LN405" and not entry["chained"], "entry range association differs")
    code = result["selected_code"]
    frames = {row["unwind_symbol"]: row["rsp_decrement"] for row in result["unwind_records"] if not row["chained"]}
    observed = tuple(frames["$unwind$" + code[name]["symbol"]] for name in names)
    require(len(result["unwind_records"]) == 349 and observed == (2328, 341864, 196744, 152, 104),
            "unexpected variant unwind records")
    run, read = (code[name]["symbol"] for name in names[1:3])
    require([at for at, label in code["main"]["direct_relative_calls"] if label == run] == [1133]
            and [at for at, label in code[names[1]]["direct_relative_calls"] if label == read] == [7184, 10037],
            "expected main → run → readText call edges")
    selected = sum(observed[:3])
    require(selected == 540936, "selected active path sum differs")
    print(f"PASS: 349 unwind records and exact entry/catch frame extents {observed}")
    print(f"Selected active RSP decrement: {selected:,} bytes")


if __name__ == "__main__":
    main()
