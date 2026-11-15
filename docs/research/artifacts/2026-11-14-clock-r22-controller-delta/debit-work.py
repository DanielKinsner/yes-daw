"""Recompose controller counter amounts from exact Spend-Task sites."""

import json
from pathlib import Path
import re
import runpy


HERE = Path(__file__).resolve().parent


def work():
    source = runpy.run_path(str(HERE / "shared-schedule.py"))
    manifest, lines = source["pinned_inputs"]()
    require = source["REQUIRE"]
    written = [number for number, line in enumerate(lines, 1)
               if not line.startswith(b"function Spend-Task(")
               for _ in re.finditer(rb"(?<![\w-])Spend-Task(?![\w-])", line)]
    require(written == [151, 154, 157, 197, 418, 672, 685, 735, 744, 967, 991, 1079],
            "written debit sites changed")
    expected_literal_amounts = {151: 1, 154: 64, 157: 32, 672: 1, 685: 1, 735: 1, 744: 128, 967: 64, 991: 1, 1079: 2}
    for line, amount in expected_literal_amounts.items():
        require(re.search(rb"\bSpend-Task " + str(amount).encode("ascii") + rb"\b", lines[line - 1]) is not None,
                f"literal debit amount changed at line {line}")

    schedule = source["reconstruct"](manifest)
    shared = runpy.run_path(str(HERE / "shared-work.py"))["work"]()
    local = runpy.run_path(str(HERE / "local-work.py"))["work"]()
    identity = schedule["input_identity_calls"]
    paths = schedule["paths"]
    document = schedule["json_document"]
    controls = schedule["build_target_controls"]
    repetitions = {"marker": document["marker"], "receipt": document["receipts"],
                   "failure": document["failure_inventory"], "bundle": document["bundle_passes"]}
    shapes = shared["output_shapes"]
    string_units = sum(count * (shapes[name]["key_units"] + shapes[name]["string_units"])
                       for name, count in repetitions.items())
    value_visits = sum(count * shapes[name]["value_calls"] for name, count in repetitions.items())
    visits = {151: 65 * paths["no_link"], 154: identity["hashes"], 157: identity["hash_chunks"],
              672: string_units, 685: value_visits, 735: paths["walk_rows"], 744: controls["copy_chunks"],
              967: 1 + controls["dependency_calls"], 991: document["stage_rows"],
              1079: schedule["walks"]["reference_captures"]["returned_rows"]}
    legacy = sum(expected_literal_amounts[line] * count for line, count in visits.items())
    require((string_units, value_visits, visits[735], legacy) == (4_877_855, 37_004, 9_656, 9_625_331),
            "controller legacy debit upper changed")

    require(b"$taskS['parent_poll_limit'] -gt [int]::MaxValue" in lines[244], "poll representation guard changed")
    before_native, poll_slope = shared["controller_authored_work_before_native_reservations"]
    representable = before_native + ((1 << 31) - 1) * poll_slope
    require(representable == 2_622_007_436_182, "controller work at max representable poll input changed")
    return {"legacy_visits_by_line": visits, "legacy_literal_debits": legacy,
            "controller_price_coefficients": local["debit_amount_multipliers"],
            "helper_reservation_visits": local["direct_reservation_debits"],
            "controller_authored_actions": (before_native, poll_slope),
            "controller_actions_at_max_representable_poll_limit": representable}


if __name__ == "__main__":
    print(json.dumps(work(), indent=2))
