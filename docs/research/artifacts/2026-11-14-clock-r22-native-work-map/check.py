"""Check that each R22 native function is represented in the source ledgers."""

from collections import Counter
import hashlib
import json
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
RESEARCH = HERE.parents[1]
ROOT = HERE.parents[3]
MANIFEST = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                   "clock-oracle-r22-guard-read-source-r1/source-manifest.json")
MANIFEST_PIN = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"
FUNCTION = re.compile(rb"^(?:\[\[noreturn\]\] )?(?:bool|void|int|U32|U64|DWORD|PathResult|Failure)"
                      rb"(?: WINAPI)? (\w+)\(([^;\n]*)\)\s*(?:noexcept\s*)?\{", re.M)

LEDGERS = {
    "sup_codec.cpp": {
        "r22-terminal-codec-scalar-census": "put32 put64 pair same_failure reason_present validate_terminal encode_terminal",
        "r22-decode-scalar-census": "get32 get64 cell high low valid_wide copy_text decode_request",
        "r22-profile-command-census": "wide_from_binding validate_profile parse_command",
        "r22-request-recipe-census": "literal text_matches append_template validate_request_recipe",
        "r22-scan-final-census": "join_path",
        "r22-launch-format-census": "append build_environment build_target_command",
    },
    "sup_core.cpp": {
        "r22-terminal-codec-scalar-census": "checked_add equal_digest",
        "r20-scalar-review-delta": "charge spend failure fail begin_tail",
        "r22-coordinator-census": "admit_body:Meter admit_body:State tail_requested",
        "r22-worker-thread-census": "admit_read request_tail",
        "r22-log-writer-census": "admit_write:State",
        "r22-capture-meter-census": "admit_write:Meter",
        "r22-profile-command-census": "equal_wide",
        "r22-sha-compress-census": "rotate compress",
        "r22-sha-stream-census": "sha_init sha_update sha_finish",
        "r22-run-identity-scalar-census": "hash_run_identity",
    },
    "sup_io.cpp": {
        "r22-close-owner-census": "live close_one",
        "r22-capture-creation-census": "capture_raw observe_position open_capture_owners create_contained_target",
        "r22-scan-final-census": "path_failure same_name allowed_stage_name validate_path stage_scan",
        "r22-identity-io-scalar-census": "read_request hash_file",
        "r22-log-writer-census": "native_count write_chunk",
        "r22-publication-scalar-census": "close_safe_owners publish_terminal",
    },
    "sup_main.cpp": {
        "r22-coordinator-census": "live counter workers_settled zero_unavailable_counters observe_target_exit coordinator "
                                  "cleanup_prefix finalize_record finish supervisor_main wmain",
        "r22-worker-thread-census": "worker_error worker_exit_error finish_worker publish_ready worker_entry start_workers",
        "r22-worker-coordinator-census": "stop_workers acknowledge ready_now cancel_workers settle_one consume_ready",
    },
}


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def sha(raw):
    return hashlib.sha256(raw).hexdigest().upper()


def functions(raw):
    for name, arguments in FUNCTION.findall(raw):
        name = name.decode("ascii")
        if name in {"admit_body", "admit_write"}:
            receiver = arguments.split(b"&", 1)[0].decode("ascii")
            require(receiver in {"Meter", "State"}, "unexpected meter overload")
            name += ":" + receiver
        yield name


def check_phases():
    text = (RESEARCH / "2026-11-14-clock-r22-native-phase-work.md").read_text(encoding="utf-8")
    detail, slot_table = text.split("| Helper slot | Prefix | Tail | Total |")
    rows = re.findall(r"\| ([\d,]+) \| ([\d,]+) \|\n", detail)
    amounts = [tuple(int(number.replace(",", "")) for number in row) for row in rows]
    expected = [
        (3_197_625 + 140 + 160 * 512 + 10_000_662, 0),
        (4_441_192, 0),
        (30_023_518_404, 0),
        (2_060 * 12_471 + 8 * 59_294 + 478_744, 59_294 + 2_904 + 220),
        (7_643_847, 0),
        (1_630 + 1_819 + 2 * 404, 2 * 404),
        (606_281_984, 2_084),
        (396_420_102, 1_502 + 4_144),
        (31_595_666, 48_320 + 6_894),
        (7_995_138, 474 + 1_624 + 5_172),
        (1_048_837 * 651_320, 2 * 651_320 + 309 * 618_355),
        (5 * 236, 42 * 236),
        (0, 20_202 + 30_330 + 4 * 44),
        ((4 + 45 + 73 + 28 + 8) * 103 + 6 * 160 + 220, 0),
        (2_280_550_078_421, 3_526_812),
    ]
    prefix, tail = (sum(column) for column in zip(*expected))
    require(amounts == [*expected, (prefix, tail)], "phase arithmetic differs")
    other_prefix = prefix - 4_441_192 + 2_001_040
    all_prefix, all_tail = prefix + 2 * other_prefix, 3 * tail
    expected_slots = [(prefix, tail, prefix + tail), (other_prefix, tail, other_prefix + tail),
                      (all_prefix, all_tail, all_prefix + all_tail)]
    rows = re.findall(r"\| ([\d,]+) \| ([\d,]+) \| ([\d,]+) \|\n", slot_table)
    observed_slots = [tuple(int(number.replace(",", "")) for number in row) for row in rows]
    require(observed_slots == expected_slots, "helper composition differs")
    bootstrap_spends = 2 + 8 + 2 + 41_262 + 132
    bootstrap = (16 + 1_492 + 180 + 3_197_625 + 2 * (59_294 + 1_026) + 2_641_376 + 851_214_430
                 + 4_441_192 + bootstrap_spends * 103 + 220)
    require(bootstrap == 865_881_989 and f"{bootstrap:,}" in text, "bootstrap upper differs")
    print(f"PASS: exact R22 phase arithmetic, {all_prefix + all_tail:,} actions across three helpers")


def main():
    raw = MANIFEST.read_bytes()
    require(sha(raw) == MANIFEST_PIN, "incorrect R22 manifest")
    seen = {}
    for pin in json.loads(raw)["supervisor"]["native_sources"]:
        path = ROOT / pin["path"]
        data = path.read_bytes()
        require(len(data) == pin["bytes"] and sha(data) == pin["sha256"], f"wrong pin: {path}")
        if path.suffix == ".cpp":
            ledgers = LEDGERS[path.name]
            for ledger in ledgers:
                require((RESEARCH / f"2026-11-14-clock-{ledger}.md").is_file(), f"missing {ledger}")
            named = Counter(name for group in ledgers.values() for name in group.split())
            require(Counter(functions(data)) == named, f"{path.name} coverage differs")
            seen[path.name] = named.total()
    require(seen.keys() == LEDGERS.keys() and sum(seen.values()) == 87, "unit coverage differs")
    print(f"PASS: {sum(seen.values())} exact R22 native function definitions mapped to source ledgers")
    check_phases()


if __name__ == "__main__":
    main()
