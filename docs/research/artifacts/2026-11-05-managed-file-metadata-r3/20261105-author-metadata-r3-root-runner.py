"""Root-owned fresh hidden file-metadata launcher after independent source review."""
from pathlib import Path
import hashlib
import json

root=Path(__file__).resolve().parents[1]
old=root/'build-ci/20261105-managed-file-type-inventory-source-r2'
new=root/'build-ci/20261105-managed-file-type-inventory-source-r3'
review=root/'build-ci/20261105-managed-file-type-inventory-independent-review-r3/review.json'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
assert digest(review)=='5E870B52977ED4F984953DF2E78CEDD012BC7C62554398D768F9F959D1271EBD'
assert json.loads(review.read_text())['verdict'].startswith('ACCEPT')
assert digest(new/'source-pins.json')=='3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2'
assert not (new/'run-reader.py').exists() and not (new/'run-reader.ps1').exists()
ps=(old/'run-reader.ps1').read_text()
for before,after in [('72F84E45F43CB4ED86E91B6C1CCB464A904E043E5338FAF941C47284E69DC7DC','3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2'),('22475','22493'),('9215EED3EDAC196C983353ADC68D1BA95FCD0C4833ED1130FB7C8F39CCC8D348','7A8CCCE39C75E2C0030D01A864E2BFD55FDB81E5EA2B6A9644A8EF8B3B5B7F51')]:assert before in ps;ps=ps.replace(before,after)
(new/'run-reader.ps1').write_text(ps,encoding='utf-8',newline='\n')
py=(old/'run-reader.py').read_text()
replacements={
    'metadata revision2 attempt':'metadata revision3 attempt',
    '20261105-managed-file-type-inventory-independent-review-r2/review.json':'20261105-managed-file-type-inventory-independent-review-r3/review.json',
    '6388129EF1E04F6B2E39A2ED19458D1AAB0589AD343FB5932DA020FDCC02A675':'5E870B52977ED4F984953DF2E78CEDD012BC7C62554398D768F9F959D1271EBD',
    '72F84E45F43CB4ED86E91B6C1CCB464A904E043E5338FAF941C47284E69DC7DC':'3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2',
    'PUBLIC_FILE_METADATA_SOURCE_R2_ACTUAL_ATTEMPT_R1':'PUBLIC_FILE_METADATA_SOURCE_R3_ACTUAL_ATTEMPT_R1',
    'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V2':'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V3',
    'inventory["required_named_types"] == 21':'inventory["required_named_types"] == 22',
    'manifest["allowlist_original_required"]':'manifest["allowlist_required"]',
    '22 <= inventory["reported_types"] <= 25':'23 <= inventory["reported_types"] <= 26',
    '"evidence/author-source.py",':'"../20261105-author-metadata-r3-source.py", "../20261105-finalize-metadata-r3-source.py",',
    'R11':'R12',
}
for before,after in replacements.items():assert before in py,before;py=py.replace(before,after)
(new/'run-reader.py').write_text(py,encoding='utf-8',newline='\n')
authority=dict(scope='One separate bounded hidden public-file metadata sourceR3 attempt after independent whole source review and root launcher-delta review. Not an R12 target or allocation/size probe.',independent_review_sha256=digest(review),subject_manifest_sha256=digest(new/'source-pins.json'),reader_sha256=digest(new/'reader.cs'),compiler_reference_profile_sha256=digest(root/'build-ci/20261105-managed-file-type-inventory-source-r1/compilation-reference-profile.json'),launcher_sha256=digest(new/'run-reader.py'),powershell_wrapper_sha256=digest(new/'run-reader.ps1'),constraints=dict(console_PE=True,hidden=True,stdin_null=True,redirected_output=True,timeout_seconds=60,complete_exit_and_JSON_required=True,target_invoked=False,GUI_or_devices=False),scope_addition='Exactly one required Tuple`2 field/base/signature definition, retained original inventory and updated schema/count literals.',first_green_freezes_whole_source_and_launchers=True)
(new/'root-attempt-authorization.json').write_text(json.dumps(authority,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(authority))
