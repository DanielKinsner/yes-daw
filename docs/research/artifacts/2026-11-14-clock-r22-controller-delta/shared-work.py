"""Price shared R22 controller body groups under the reviewed R13 scalar convention."""

import hashlib
import json
from pathlib import Path
import re
import runpy


HERE = Path(__file__).resolve().parent
SCHEDULE = runpy.run_path(str(HERE / "shared-schedule.py"))
DELTA = SCHEDULE["DELTA"]
REQUIRE = SCHEDULE["REQUIRE"]
ARCHIVE = HERE.parent / "2026-11-06-clock-r13-runner-census-independent-review-r3/sources"
PRICE_PIN = "F9BE03F46F0139084FA823D49DFC1EB64DFA665D94F1DC665A7A57B4B1AC6353"
SHAPE_PIN = "F04B17303E993A01F6B6310D5CFFA47256BF4BA73F55098A34824C95D395825A"


def pin(path, digest):
    REQUIRE(hashlib.sha256(path.read_bytes()).hexdigest().upper() == digest, f"archive pin differs: {path.name}")
    return path


def output_shapes(manifest, lines):
    frozen = pin(ARCHIVE / "268-shapes.py", SHAPE_PIN)
    model = runpy.run_path(str(frozen))
    shapes = model["build_shapes"](len(manifest["inputs"]))
    names = (
        (shapes["marker"], 1209, b"$taskMarkerValue=@{"),
        (shapes["receipt"], 61, b"$taskReceipt=[ordered]@{"),
        (shapes["receipt"]["map"]["children"]["array"][0], 479, b"$taskRow=[ordered]@{"),
        (shapes["failure"], 1167, b"'failure-inventory.json') @{"),
        (shapes["bundle"], 1260, b"$taskBundle=[ordered]@{"),
    )
    for node, line, start in names:
        fields = lines[line - 1].split(start, 1)[1].split(b"}", 1)[0]
        keys = {name.decode("ascii") for name in re.findall(rb"(?:^|;)([a-zA-Z_][a-zA-Z_0-9]*)=", fields)}
        REQUIRE(keys == node["map"].keys(), f"output shape fields differ at {line}")
    for child in shapes["receipt"]["map"]["children"]["array"]:
        child["map"]["cleanup"] = model["array"]([model["text"](43) for _ in range(15)])
    failure_rows = shapes["failure"]["map"]["files"]["array"]
    failure_rows.extend([failure_rows[0]] * (manifest["profile"]["root_walk_entries_max"] - len(failure_rows)))
    bundle = shapes["bundle"]["map"]
    bundle["artifacts"]["array"].extend(model["descriptor"]() for _ in manifest["supervisor"]["transport_files"])
    bundle["transitive_inputs"]["array"].append(model["descriptor"]())
    return {key: model["graph_counts"](shape) for key, shape in shapes.items()}


def json_sizes(graph, visitor, marker=False):
    g = graph
    source = sum(visitor[price] * g.get(rows, 0) for price, rows in (
        ("map_fixed", "maps"), ("array_fixed", "arrays"), ("map_per_entry", "keys"),
        ("map_per_key_unit", "key_units"), ("map_separator", "map_separators"),
        ("array_per_entry", "array_elements"), ("array_separator", "array_separators"),
        ("string", "strings"), ("string_per_unit", "string_units"), ("integer", "integers"),
        ("boolean", "booleans")))
    # The complete nonnull alternatives in these shapes also dominate the
    # nullable fields' local null visit. The marker values are fixed ASCII.
    wire = (2 * (g.get("maps", 0) + g.get("arrays", 0)) + 3 * g.get("keys", 0)
            + g.get("map_separators", 0) + g.get("array_separators", 0) + g.get("key_units", 0)
            + (1 if marker else 6) * g.get("string_units", 0) + 2 * g.get("strings", 0)
            + 20 * g.get("integers", 0) + 5 * g.get("booleans", 0) + 1)
    return {"source": source, "wire": wire}


def file_set_work(prices, table):
    set_price = prices["Assert-TaskFileSet"]
    join = prices["Join-TaskBoundedNames"]
    slots, expected, observed = map(sum, zip(*(row[1:] for row in table["slots_expected_observed"])))
    visited = slots * set_price["fixed"] + expected * set_price["expected"] + observed * set_price["observed"]
    visited += (expected + observed) * 512 * set_price["per_name_unit"]

    def refusal_tail(n, o, missing, unknown, units):
        return (set_price["refusal_fixed"] - set_price["fixed"]
                + n * (set_price["refusal_expected"] - set_price["expected"])
                + o * (set_price["refusal_observed"] - set_price["observed"])
                + 2 * join["fixed"] + (missing + unknown) * join["per_name"] + units * join["per_name_unit"])

    # Missing, extra, and duplicate tests continue through their exact catches.
    controlled = (refusal_tail(56, 55, 1, 0, len("references/native-inverse/r48000-map5.json"))
                  + refusal_tail(56, 57, 0, 1, len("unlisted.bin"))
                  + refusal_tail(56, 57, 0, 0, 0))
    n, o = table["max_single_expected"], table["max_single_observed"]
    unplanned = table["unplanned_diagnostic_refusals"] * refusal_tail(n, o, n, o, 512 * (n + o))
    return visited + controlled + unplanned


def check_body_partition(baseline, current, used):
    indirect = {
        "Add-TaskJsonBound": ("Measure-TaskJson", "Visit-TaskJsonValue"),
        "Close-TaskJsonReader": ("Read-TaskDependencyJson",),
        "Convert-TaskControlOperand": ("Read-TaskChildResult",),
        "Get-TaskCatalogueString": ("Get-TaskCatalogueEntryPath",),
        "Get-TaskEscapedStringBytes": ("Visit-TaskJsonValue",),
        "Get-TaskIntegerDigits": ("Visit-TaskJsonValue",),
        "Get-TaskSafePrefix": ("Save-TaskFailureInventory",),
        "Test-TaskPathCatalogueContext": ("Assert-NoLink", "Get-TaskCanonicalPath", "Resolve-TaskInput"),
        "Write-TaskJsonBytes": ("Save-TaskReceipt", "Write-TaskJson"),
    }
    shared = {name for name in baseline if baseline[name] == current.get(name)}
    other = {"Get-TaskHash"}
    REQUIRE(shared == used | indirect.keys() | other
            and len(shared) == len(used) + len(indirect) + len(other) == 69,
            "shared body coverage is not disjoint")
    for callee, callers in indirect.items():
        REQUIRE(all(callee.encode("ascii") in current[caller] for caller in callers),
                f"indirect sponsor no longer calls {callee}")
    return indirect


def work():
    manifest, lines = SCHEDULE["pinned_inputs"]()
    SCHEDULE["check_edges"](lines)
    geometry = SCHEDULE["reconstruct"](manifest)
    price_file = pin(ARCHIVE / "252-body-components.json", PRICE_PIN)
    price = json.loads(price_file.read_bytes())["prices"]
    baseline = DELTA["collect"](DELTA["pinned"](DELTA["BASELINE"], DELTA["PINS"][0]))
    current = DELTA["collect"](lines)
    used = set()

    def p(name):
        REQUIRE(baseline[name] == current[name], f"shared body no longer matches: {name}")
        used.add(name)
        return price[name]

    def string_work(calls, units, kind):
        row = p("Assert-TaskString")
        return calls * row["fixed"] + units * row[kind]

    h = geometry["input_identity_calls"]
    paths = geometry["paths"]
    b = geometry["build_target_controls"]
    j = geometry["json_document"]
    terms = {}
    boots = p("Get-TaskBootstrapHash")
    terms["bootstrap verified files"] = h["bootstrap_hashes"] * (boots["fixed"] + 64 * boots["per_parent"])
    terms["bootstrap verified files"] += h["bootstrap_chunks"] * boots["per_chunk"]
    add = p("Add-TaskPathCatalogueEntry")
    entry = p("Get-TaskCatalogueEntryPath")
    n = paths["entry_path"]
    terms["path catalogue add and entry"] = (paths["catalogue_repo_adds"] * add["repo_input"]
                                           + paths["catalogue_other_adds"] * add["ordinary_alias"]
                                           + entry["tilde"] * n
                                           + (entry["first"] - entry["tilde"]) * min(n, paths["first_builds"]))
    terms["NoLink and Canonical"] = paths["no_link"] * p("Assert-NoLink")["upper"]
    terms["NoLink and Canonical"] += paths["canonical"] * p("Get-TaskCanonicalPath")
    resolve = p("Resolve-TaskInput")
    terms["Resolve"] = paths["resolve"] * (resolve["fixed"] + 512 * resolve["per_unit"])
    walker = p("Get-TaskEntries")
    terms["walkers"] = (paths["walk_calls"] * (walker["fixed"] + walker["overflow"] + 512 * walker["per_path_unit"]
                                             - walker["replaced_ending"])
                       + paths["walk_dirs"] * walker["per_directory"]
                       + paths["walk_rows"] * (walker["per_candidate"] + 512 * walker["per_path_unit"]))
    # Queue depths are assigned only at 0 or at depth+1 after depth<64.
    # The child-depth diagnostic nevertheless has a written refusal body
    # before the enqueue. Cover its three-slot witness and Reject independently
    # of any selected absolute-depth domination from the host path geometry.
    terms["recursive walker depth refusals"] = paths["recursive_walkers"] * (14 + p("Reject-TaskGate"))
    stage = p("Get-TaskStageBytes")
    terms["controller stage scan"] = j["stage_checks"] * stage["fixed"] + j["stage_rows"] * stage["per_row"]
    fresh = p("Assert-TaskFreshHashIdentity")
    terms["fresh identities"] = h["fresh_identity"] * fresh["normal"] + h["fresh_refusals_upper"] * fresh["refusal_surplus"]
    digests = p("Test-TaskHashValue")
    terms["digest comparisons and direct wire hex"] = (h["text_digest_tests"] * digests["hex"]
                                                       + h["binary_digest_tests"] * digests["binary"]
                                                       + h["standalone_wire_hex"] * p("Get-TaskHashHex"))
    for name in ("Assert-TaskFileSet", "Join-TaskBoundedNames"):
        p(name)
    terms["exact file sets and bounded refusal witnesses"] = file_set_work(price, geometry["file_sets"])

    fenced = p("Invoke-TaskFencedChild")
    terms["fenced child wrappers"] = 3 * fenced["fixed"] + 2 * fenced["binary"]
    replace = p("Replace-TaskBounded")
    sites = b["compiler_replacements"]
    tokens = [len("<BASE>"), len("<REPO>"), len("<BUILD>")]
    terms["compiler replacement bodies"] = (sites * (replace["fixed"] + 18)
                                           + (2 * sites * 512 + 28 * sum(tokens)) * replace["per_aggregate_argument_unit"]
                                           + 28 * sum(512 // token for token in tokens) * replace["per_occurrence"])
    copying = p("Copy-TaskOwned")
    terms["copies and mutations"] = b["copy_calls"] * copying["fixed"] + b["copy_chunks"] * copying["per_chunk"]
    terms["copies and mutations"] += b["mutation_calls"] * p("Mutate-TaskOwnedBytes")
    console = p("Assert-TaskConsolePE")
    terms["PE and console checks"] = b["pe_reads"] * p("Get-TaskPEHeader") + b["console_checks"] * max(console.values())
    terms["predecessor"] = p("Get-TaskPredecessorDescriptor") + p("Assert-TaskPredecessor")["refusal"]
    control = p("Expect-TaskGate")
    terms["ten negative controls"] = b["expect_calls"] * control["fixed"] + b["expect_keys"] * control["per_expected_key"]
    reservations = p("Assert-TaskReservation")
    retain = p("Assert-TaskRetainedReservation")
    attempt = p("Assert-TaskAttempt")
    poll = p("Assert-TaskPollAdmission")
    terms["root gates and controls"] = (2 * max(reservations.values()) + reservations["refusal"]
                                       + 2 * max(retain.values()) + retain["refusal"]
                                       + max(attempt.values()) + attempt["refusal"]
                                       + max(poll.values()) + poll["refusal"])
    deps = p("Assert-TaskDependencies")
    ds = p("Get-TaskDependencyString")
    terms["dependency descriptors and Includes"] = (deps["fixed"] + b["include_cap"] * deps["per_include"]
                                                    + (b["include_cap"] + 1) * (ds["fixed"] + 512 * ds["per_unit"]))
    reader = p("Read-TaskChildResult")
    terms["native result readers"] = (b["native_result_reads"] * reader["fixed"] + j["raw_result_units"] * reader["per_text_unit"]
                                     + b["module_control_records"] * reader["per_control"] + reader["bad_ID_prefix"])
    build = p("Get-TaskBuildArtifacts")
    ref = p("Get-TaskReferences")
    terms["artifact descriptor capture"] = (b["build_captures"] * (build["fixed"] + 4 * build["per_entry"])
                                           + b["reference_captures"] * (ref["fixed"] + 60 * (ref["per_entry"] + ref["per_file"])))
    same = p("Assert-TaskSameReferences")
    identity = p("Assert-TaskIdentity")
    terms["final descriptor identity"] = same["fixed"] + len(manifest["references"]) * same["per_file"]
    terms["final descriptor identity"] += 3 * identity["normal"] + 2 * (identity["refusal"] - identity["normal"])

    terms["captured byte reads"] = (p("Read-TaskBoundedBytes") + sum(p("Read-TaskJson").values())
                                   + p("Read-TaskDependencyJson"))
    raw = p("Assert-TaskRawJsonBytes")
    result = p("Assert-TaskRawJson")
    terms["raw grammars and byte compaction"] = (j["raw_byte_calls"] * raw["fixed_upper"]
                                                + j["raw_byte_units"] * raw["per_input_upper"]
                                                + j["raw_result_calls"] * result["fixed_upper"]
                                                + j["raw_result_units"] * result["per_input_upper"])
    doc = p("Assert-TaskDocumentWrite")
    terms["document checks, writes and receipts"] = (j["document_checks"] * (doc["fixed"] + 512 * doc["per_path_unit"])
                                                    + j["receipts"] * (p("Save-TaskReceipt") + p("Get-TaskSecondaryCodes"))
                                                    + sum(p("Write-TaskJson").values()))
    failure = p("Save-TaskFailureInventory")
    terms["failure inventory"] = failure["fixed"] + manifest["profile"]["root_walk_entries_max"] * failure["per_file"]
    shapes = output_shapes(manifest, lines)
    graph = {key: json_sizes(shape, p("Visit-TaskJsonValue"), key == "marker") for key, shape in shapes.items()}
    visit = {"marker": 1, "receipt": j["receipts"], "failure": 1, "bundle": j["bundle_passes"]}
    terms["document measure and recursive visits"] = j["measures"] * p("Measure-TaskJson")
    terms["document measure and recursive visits"] += sum(count * graph[key]["source"] for key, count in visit.items())
    encode = p("Encode-TaskJson")
    payload_caps = {"marker": graph["marker"]["wire"], "receipt": 65536,
                    "failure": min(1048576, graph["failure"]["wire"]), "bundle": 1048576}
    encoded_units = sum((j["receipts"] if key == "receipt" else 1) * (cap - 1) for key, cap in payload_caps.items())
    terms["encode output checks"] = j["encodes"] * encode["fixed"] + encoded_units * encode["per_preLF_unit"]

    terms["direct new and root signed integer guards"] = 125 * max(p("Assert-TaskBoundSourceInteger").values())
    terms["direct integral checks"] = 28 * p("Assert-TaskIntegral")
    terms["direct string guards"] = (string_work(19, 19 * 512, "general")
                                      + string_work(166 + 29 + 14 + 3, (166 + 29) * 512 + 14 * 64 + 3 * 22, "ASCII"))
    env = manifest["environment"]
    keys = p("Assert-TaskExactKeys")
    terms["supervisor environment exact keys"] = 3 * (keys["fixed"] + len(env) * keys["per_key"]
                                                       + sum(map(len, env)) * keys["per_actual_key_unit"])
    terms["direct startup codec plus bootstrap visit"] = max(p("Assert-TaskCodecBinding").values()) + p("Add-TaskJsonVisit")
    terms["direct root runtime type"] = p("Get-TaskRuntimeType")
    terms["direct local and root raw exit and secondary"] = 6 * p("Set-TaskRawExit") + (18 + 2) * p("Set-TaskSecondary")
    terms["standalone root and transport Close/Complete"] = 8 * (p("Close-TaskOwner")["upper"]
                                                                + max(p("Complete-TaskOwnerClose").values()))
    terms["standalone supervisor spends"] = 12 * p("Spend-Task")
    per_poll = 3 * (max(p("Assert-TaskBoundSourceInteger").values()) + p("Spend-Task"))
    indirect = check_body_partition(baseline, current, used)
    fixed = sum(terms.values())
    local = runpy.run_path(str(HERE / "local-work.py"))["work"]()
    local_fixed, local_poll = local["partial_controller_actions"]
    authored = fixed + local_fixed, per_poll + local_poll
    REQUIRE((fixed, per_poll, authored) == (6_359_781_815, 366, (6_372_354_136, 1218)),
            "reviewed controller source upper changed")
    return {
        "schema": "R22_SHARED_CONTROLLER_CONDITIONAL_WORK",
        "fixed_groups": terms,
        "fixed_upper": fixed,
        "per_parent_poll_limit": per_poll,
        "controller_authored_work_before_native_reservations": authored,
        "matched_priced_body_names": sorted(used),
        "shared_bodies_priced_in_enclosing_rows": indirect,
        "output_shapes": shapes,
        "output_size_and_visit_uppers": graph,
        "inclusive_file_hash_body_separate": True,
        "changed_local_bodies_and_root_caller_separate": True,
        "resource_fit_or_execution_claimed": False,
    }


if __name__ == "__main__":
    print(json.dumps(work(), indent=2))
