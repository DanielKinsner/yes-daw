from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
prior = root/'build-ci/20261105-managed-file-type-inventory-source-r1'
current = root/'build-ci/20261105-managed-file-type-inventory-source-r2'
review = root/'build-ci/20261105-managed-file-type-inventory-independent-review-r2/review.json'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest().upper()
assert sha(review) == '6388129EF1E04F6B2E39A2ED19458D1AAB0589AD343FB5932DA020FDCC02A675'
assert json.loads(review.read_text())['verdict'].startswith('ACCEPT_SOURCE_ONLY')
assert sha(current/'source-pins.json') == '72F84E45F43CB4ED86E91B6C1CCB464A904E043E5338FAF941C47284E69DC7DC'
assert not (current/'run-reader.py').exists() and not (current/'run-reader.ps1').exists()
ps=(prior/'run-reader-r2.ps1').read_text()
ps=ps.replace('8DB54EA7407FA296E658221580644825B532959387EDA287AC75311A5834318C','72F84E45F43CB4ED86E91B6C1CCB464A904E043E5338FAF941C47284E69DC7DC')
ps=ps.replace('18034','22475').replace('E9B1CC2AC4D5B1B3C8D1DE955A30A938E95EADB1FA92C8757350DB07BADACC74','9215EED3EDAC196C983353ADC68D1BA95FCD0C4833ED1130FB7C8F39CCC8D348')
(current/'run-reader.ps1').write_text(ps,encoding='utf-8',newline='\n')
py=(prior/'run-reader-r2.py').read_text()
replacements={
    'Root-owned single hidden ABI attempt; no R10 execution authority.':'Root-owned single hidden metadata revision2 attempt; no R11 execution authority.',
    'RUN = SOURCE / "execution-r2"':'RUN = SOURCE / "execution-r1"',
    '20261105-managed-file-type-inventory-independent-review-r1/review.json':'20261105-managed-file-type-inventory-independent-review-r2/review.json',
    '1C473DFDCBE4D10DA28A70A06708AE88055E36C421045B14E96C37FF1AFCDCEA':'6388129EF1E04F6B2E39A2ED19458D1AAB0589AD343FB5932DA020FDCC02A675',
    '8DB54EA7407FA296E658221580644825B532959387EDA287AC75311A5834318C':'72F84E45F43CB4ED86E91B6C1CCB464A904E043E5338FAF941C47284E69DC7DC',
    'reference_profile = SOURCE / "compilation-reference-profile.json"':'reference_profile = ROOT / "build-ci/20261105-managed-file-type-inventory-source-r1/compilation-reference-profile.json"',
    'run-reader-r2.ps1':'run-reader.ps1',
    'run-reader-r2.py':'run-reader.py',
    'PUBLIC_FILE_METADATA_ACTUAL_ATTEMPT_R2':'PUBLIC_FILE_METADATA_SOURCE_R2_ACTUAL_ATTEMPT_R1',
    'evidence/seal.py':'evidence/author-source.py',
    'R10':'R11',
}
for before,after in replacements.items():
    assert before in py,before
    py=py.replace(before,after)
start=py.index('assert inventory["requested_types"]')
end=py.index('assert all(len(row["declared_nonstatic_instance_fields"])',start)
py=py[:start]+'''assert inventory["schema"] == "FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V2"
assert inventory["required_named_types"] == 21 and inventory["optional_named_types"] == 3
assert inventory["required_generated_type_selections"] == 1
assert inventory["target_assemblies_explicitly_loaded_or_executed"] is False
assert len(inventory["nested_type_name_discoveries"]) == 1
discovery = inventory["nested_type_name_discoveries"][0]
assert discovery["file"] == "Newtonsoft.Json.dll" and discovery["parent"] == "Newtonsoft.Json.Linq.JObject"
generated = discovery["selected_generated_type"]
assert re.fullmatch(r"Newtonsoft\\.Json\\.Linq\\.JObject\\+<GetEnumerator>d__[0-9]+", generated)
assert generated in discovery["declared_nested_type_names"]
presence = inventory["optional_type_presence"]
assert len(presence) == 3 and {row["qualified_name"] for row in presence} == set(manifest["optional_named_types"])
assert all(row["file"] == "System.Private.CoreLib.dll" and type(row["present"]) is bool for row in presence)
expected = set(manifest["allowlist_original_required"]) | {generated} | {row["qualified_name"] for row in presence if row["present"]}
assert 22 <= inventory["reported_types"] <= 25
assert inventory["reported_types"] == len(inventory["types"]) == len(expected)
assert {row["qualified_name"] for row in inventory["types"]} == expected
''' + py[end:]
py=py.replace('receipt["reported_types"] = 21','receipt["reported_types"] = inventory["reported_types"]')
py=py.replace('"types": 21,','"types": inventory["reported_types"],')
(current/'run-reader.py').write_text(py,encoding='utf-8',newline='\n')
authority=dict(scope='One separate bounded hidden public-file metadata attempt after full independent source acceptance and root whole reader/launcher review. No R11 target or allocation/size proof.',
    independent_review_sha256=sha(review), subject_manifest_sha256=sha(current/'source-pins.json'),
    reader_sha256=sha(current/'reader.cs'), compiler_reference_profile_sha256=sha(prior/'compilation-reference-profile.json'),
    compile_options=['/optimize+','/warnaserror+','/unsafe-','/checked+'],
    launcher_sha256=sha(current/'run-reader.py'), powershell_wrapper_sha256=sha(current/'run-reader.ps1'),
    source_scope='Original21 types plus one discovered JObject iterator and three optional fixed CoreLib enumerators; no target code execution.',
    constraints=dict(console_pe=True,hidden=True,no_stdin=True,redirected_output=True,timeout_seconds=60,exact_exit_and_complete_json_required=True,no_GUI_or_devices=True),
    first_green_freezes_whole_source_and_launchers=True)
(current/'root-attempt-authorization.json').write_text(json.dumps(authority,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(authority))
