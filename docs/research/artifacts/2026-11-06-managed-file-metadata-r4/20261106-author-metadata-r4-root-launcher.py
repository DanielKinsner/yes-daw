"""Author one fresh bounded purpose launcher; never execute it here."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
old = root / 'build-ci/20261105-managed-file-type-inventory-source-r3'
new = root / 'build-ci/20261106-managed-file-type-inventory-source-r4'
review = root / 'build-ci/20261106-managed-file-type-inventory-independent-review-r4/review.json'
digest = lambda path: hashlib.sha256(path.read_bytes()).hexdigest().upper()
assert json.loads(review.read_bytes())['verdict'] == 'ACCEPT_SOURCE_ONLY_PUBLIC_FILE_METADATA_R4_NOT_EXECUTION_OR_LAYOUT_QUALIFICATION'
assert digest(new / 'source-pins.json') == '10E3854D27239A9BDD1BDF0F798C57578B6D629B4EA9DD7F52A30A8861A11E89'
assert not (new / 'run-reader.py').exists()
text = (old / 'run-reader.py').read_text(encoding='utf-8')
replacements = {
    'metadata revision3 attempt': 'metadata revision4 attempt',
    '20261105-managed-file-type-inventory-independent-review-r3/review.json': '20261106-managed-file-type-inventory-independent-review-r4/review.json',
    '5E870B52977ED4F984953DF2E78CEDD012BC7C62554398D768F9F959D1271EBD': digest(review),
    '3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2': digest(new / 'source-pins.json'),
    'PUBLIC_FILE_METADATA_SOURCE_R3_ACTUAL_ATTEMPT_R1': 'PUBLIC_FILE_METADATA_SOURCE_R4_ACTUAL_ATTEMPT_R1',
    'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V3': 'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V4',
    'inventory["required_named_types"] == 22': 'inventory["required_named_types"] == 84',
    '"../20261105-author-metadata-r3-source.py", "../20261105-finalize-metadata-r3-source.py"': '"../20261106-controller-v-wrapper-source-closure-r1/author-metadata-r4.py", "../20261106-author-metadata-r4-root-launcher.py"',
    'R12': 'R13',
}
for before, after in replacements.items():
    assert before in text, before
    text = text.replace(before, after)
before = '''expected = set(manifest["allowlist_required"]) | {generated} | {row["qualified_name"] for row in presence if row["present"]}
assert 23 <= inventory["reported_types"] <= 26
assert inventory["reported_types"] == len(inventory["types"]) == len(expected)
assert {row["qualified_name"] for row in inventory["types"]} == expected
assert all(len(row["declared_nonstatic_instance_fields"]) == row["declared_nonstatic_instance_field_count"] for row in inventory["types"])'''
after = '''expected = {(filename, name) for filename, names in manifest["allowlist_by_file"].items() for name in names}
expected.add(("Newtonsoft.Json.dll", generated))
expected.update((row["file"], row["qualified_name"]) for row in presence if row["present"])
assert 85 <= inventory["reported_types"] <= 88
assert inventory["reported_types"] == len(inventory["types"]) == len(expected)
assert {(row["file"], row["qualified_name"]) for row in inventory["types"]} == expected
input_pins = {Path(row["path"]).name: row for row in manifest["target_inputs"]}
for row in inventory["types"]:
    pin = input_pins[row["file"]]
    assert row["file_bytes"] == pin["bytes"] and row["file_sha256"] == pin["sha256"]
    assert len(row["declared_nonstatic_instance_fields"]) == row["declared_nonstatic_instance_field_count"]
    assert re.fullmatch(r"0x02[0-9A-F]{6}", row["metadata_token"])
    for field in row["declared_nonstatic_instance_fields"]:
        assert re.fullmatch(r"0x04[0-9A-F]{6}", field["metadata_token"])
        assert re.fullmatch(r"(?:[0-9A-F]{2})+", field["signature_blob_hex"])
        assert field["signature_blob_hex"].startswith("06")'''
assert text.count(before) == 1
text = text.replace(before, after)
(new / 'run-reader.py').write_text(text, encoding='utf-8', newline='\n')
proposal = dict(scope='UNRUN_ROOT_LAUNCHER_SOURCE_PROPOSAL_PENDING_INDEPENDENT_REVIEW',
                subject_manifest_sha256=digest(new / 'source-pins.json'),
                subject_reader_sha256=digest(new / 'reader.cs'),
                independent_source_review_sha256=digest(review),
                launcher_sha256=digest(new / 'run-reader.py'),
                powershell_wrapper_sha256=digest(new / 'run-reader.ps1'),
                predecessor_launcher_sha256=digest(old / 'run-reader.py'),
                changes=['R4 source/review/schema/count pins', 'Complete output uses (file,qualified_name) identity and validates each input pin/token/signature shape', 'Whole first-green freeze includes the two R4 author helpers'],
                unchanged=['Console AMD64 PE check', 'CreateNoWindow and SW_HIDE', 'stdin null and redirected output', '60s timeout/kill/reap and error-mode restoration', 'Explicit environment key allowlist; no values persisted', '167 default compiler reference pins before/after', 'No target Clock/parser/helper/stage GO'])
(new / 'root-launcher-proposal.json').write_text(json.dumps(proposal, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(proposal, indent=2))
