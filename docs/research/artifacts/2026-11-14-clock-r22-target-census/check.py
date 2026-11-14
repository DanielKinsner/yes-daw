"""Verify current Clock target source and include correspondence to the archived census."""

import hashlib
import json
from pathlib import Path


HERE = Path(__file__).resolve().parent
ARTIFACTS = HERE.parent
ROOT = HERE.parents[3]
R22 = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
              "clock-oracle-r22-guard-read-source-r1")
ARCHIVE = ARTIFACTS / "2026-11-05-clock-research-r9"
R22_PIN = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def hash_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest().upper()


def identity(path):
    return str(path.resolve()).casefold()


def main():
    review = json.loads((ARCHIVE / "cpp-census-independent-review.json").read_bytes())
    require(review["verdict"] == "ACCEPT_CONDITIONAL_CPP_CENSUS_R2" and review["families"] == 76,
            "archived review subject differs")
    older = ARCHIVE / "cpp-census"
    for name, expected in review["sealed_files"].items():
        require(hash_file(older / name) == expected, f"edited archival census: {name}")
    pins = json.loads((older / "source-pins.json").read_bytes())
    census = json.loads((older / "census.json").read_bytes())
    require(hash_file(R22 / "source-manifest.json") == R22_PIN, "unexpected staged manifest")
    receipt = json.loads((ARTIFACTS / "2026-11-14-clock-r22-child-object/result.json").read_bytes())
    require(receipt["manifest_sha256"] == R22_PIN and receipt["raw_exit"] == 0
            and receipt["include_count"] == 110, "unexpected target compile receipt")
    observed = {identity(Path(name)): value for name, value in receipt["includes"].items()}
    expected_sources = {
        "clock_stage.cpp": R22 / "clock_stage.cpp",
        "source-costs.h": R22 / "source-costs.h",
        "Time.h": ROOT / "src/engine/Time.h",
        "independent_clock.h": (ROOT / "build-ci/musical-audio-persistent-planner-certification/"
                               "source-revisions/waveform-definition-r3/independent_clock.h"),
    }
    for name, path in expected_sources.items():
        old, = [row for row in pins["core_rechecked"] if Path(row["path"]).name == name]
        require(hash_file(path) == old["sha256"] and path.stat().st_size == old["bytes"],
                f"compiled source differs from review: {name}")
        selected = receipt["source_sha256"] if name == "clock_stage.cpp" else observed.get(identity(path))
        require(selected == old["sha256"], f"source not observed in purpose: {name}")
    for root_name, key, count in (("sdk_root", "sdk_hashes_rechecked", 25),
                                  ("ucrt_root", "ucrt_hashes_rechecked", 10),
                                  ("crt_behavior_source_root", "crt_behavior_only_rechecked", 3)):
        require(len(pins[key]) == count, f"archived {key} count differs")
        for row in pins[key]:
            path = Path(pins[root_name]) / row["path"]
            require(hash_file(path) == row["sha256"], f"behavior source changed: {path}")
            if key != "crt_behavior_only_rechecked":
                require(observed.get(identity(path)) == row["sha256"], f"include differs: {path}")
    book = expected_sources["source-costs.h"].read_text(encoding="utf-8")
    family_text = book.split("enum class Family : std::size_t {", 1)[1].split("};", 1)[0]
    names = [name.strip() for name in family_text.split(",")]
    require(names == [row["name"] for row in census["families"]] + ["Count"], "family order changed")
    require([row["id"] for row in census["families"]] == list(range(76)), "family IDs changed")
    require(all(type(row["upperUnits"]) is int and row["upperUnits"] > 0 for row in census["families"]),
            "invalid audited family operand")
    produce = sum(census["producer_childUpper_terms"].values())
    verify = sum(census["verifier_childUpper_terms"].values())
    accepted = review["accepted_conditional_uppers"]
    require((produce, verify, produce + verify) ==
            (accepted["ProducerUpper"], accepted["VerifierUpper"], accepted["children"]),
            "archived target arithmetic differs")
    print(f"PASS: 4 target files, 35 compiled SDK/UCRT headers, 3 CRT sources, 76 audited families")
    print(f"Conditional source operands: produce {produce:,}, verify {verify:,}, total {produce + verify:,}")
    return produce, verify


if __name__ == "__main__":
    main()
