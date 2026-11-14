"""Associate the 30 measured layouts with their reviewed source facts and joins."""

import argparse
import hashlib
import json
from pathlib import Path


ARTIFACTS = Path(__file__).resolve().parents[1]
SOURCE = ARTIFACTS / "2026-11-06-native-sdk-context-ledger-r2/sources"
COMPILER = "88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D"
RECEIPTS = {
    "direct-layout": {
        "NUL": "std::nullptr_t", "LESS": "std::less<void>", "EQ": "std::equal_to<void>",
        "FA": "std::_Fake_allocator", "FP": "std::_Fake_proxy_ptr_impl",
        "TG": "std::_Tidy_guard<std::vector<qc::Change>>",
        "TAG_ZERO": "std::_Zero_then_variadic_args_t", "TAG_ONE": "std::_One_then_variadic_args_t",
        "TAG_CONCAT": "std::_String_constructor_concat_tag",
        "ALLOC_CHANGE": "std::allocator<qc::Change>", "ALLOC_CHAR": "std::allocator<char>",
        "EXDATA": "::__std_exception_data", "TempoMapView": "yesdaw::engine::TempoMapView",
    },
    "named-closures": {"OWN_CLOSURE_get": "get", "OWN_CLOSURE_nextBit": "nextBit"},
    "control-closures": {
        "OWN_CLOSURE_work_action": "work-one-below", "OWN_CLOSURE_carry_action": "fraction-carry",
        "OWN_CLOSURE_invalid_native_action": "fraction-carry-native-read",
        "OWN_CLOSURE_q_action": "q-off-by-one", "OWN_CLOSURE_truncated_action": "truncated-record",
        "OWN_CLOSURE_trailing_action": "trailing-record",
        "OWN_CLOSURE_metadata_action": "metadata-corruption", "OWN_CLOSURE_prefix_action": "full-prefix",
        "OWN_CLOSURE_ramp_action": "ramp-law", "OWN_CLOSURE_inverse_whole_action": "inverse-whole",
        "OWN_CLOSURE_inverse_fraction_action": "inverse-fraction",
    },
    "comparator-closures": {
        "OWN_CLOSURE_qc_tick_comparator": "qc_tick", "OWN_CLOSURE_time_tick_comparator": "time_tick",
        "OWN_CLOSURE_time_native_comparator": "time_native",
    },
    "append-closure": {"L_APPEND": "string_append"},
}


def read(path):
    raw = path.read_bytes()
    return json.loads(raw), hashlib.sha256(raw).hexdigest().upper()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def compose():
    source, source_hash = read(SOURCE / "038-layout-source-correspondence.json")
    joins, join_hash = read(SOURCE / "034-context-joins.json")
    facts = {row["id"]: row for row in source["facts"]}
    layouts = {}
    provenance = {}
    for group, mapping in RECEIPTS.items():
        path = ARTIFACTS / f"2026-11-14-clock-{group}/result.json"
        receipt, receipt_hash = read(path)
        require(receipt["compiler_sha256"] == COMPILER and receipt["raw_exit"] == 0,
                f"unqualified compile-only receipt: {group}")
        require(set(mapping.values()) == receipt["layouts"].keys(), f"incomplete receipt: {group}")
        provenance[group] = {
            "artifact": path.relative_to(ARTIFACTS).as_posix(),
            "receipt_sha256": receipt_hash,
            "object_sha256": receipt["object_sha256"],
        }
        for name, key in mapping.items():
            require(name not in layouts and name in facts, f"invalid fact association: {name}")
            layout = receipt["layouts"][key]
            extent, alignment = layout["sizeof"], layout["alignof"]
            require(type(extent) is int and type(alignment) is int and extent > 0 and alignment > 0
                    and alignment & (alignment - 1) == 0 and extent % alignment == 0,
                    f"invalid layout: {name}")
            layouts[name] = {
                **layout,
                "receipt": group,
                "receipt_key": key,
                "source_sha256": facts[name]["primary_pin"]["sha256"],
            }
    require(layouts.keys() == facts.keys() and len(layouts) == 30, "incomplete measured layout set")

    components = {}
    for join in joins["critical_joins"]:
        measured = {name: count for name, count in join["maximum_instances"].items()
                    if count and name in layouts}
        if join["id"] == "upper_tick4":
            predicates = [layout["sizeof"] for name, layout in layouts.items()
                          if name.endswith("_comparator")]
            require(len(predicates) == 3, "missing comparator layout")
            comparator = max(predicates) * join["maximum_instances"]["own_stateless_predicate"]
        else:
            comparator = 0
        if measured or comparator:
            components[join["id"]] = sum(layouts[name]["sizeof"] * n for name, n in measured.items()) + comparator
    return {
        "source_correspondence_sha256": source_hash,
        "source_joins_sha256": join_hash,
        "compiler_sha256": COMPILER,
        "receipts": provenance,
        "measured_types": layouts,
        "measured_components_in_critical_joins_bytes": components,
        "scope": "Selected measured object-size terms only. Whole resource fit remains open.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", type=Path, help="Verify the combined receipt exactly")
    args = parser.parse_args()
    result = compose()
    if args.check:
        require(read(args.check)[0] == result, "combined layout receipt differs")
        print(f"PASS: {len(result['measured_types'])} layout facts across {len(result['receipts'])} receipts")
    else:
        print(json.dumps(result, indent=2))
