"""Independent data/source review. Does not import or execute either launcher."""
import hashlib,json,pathlib,re,difflib
ROOT=pathlib.Path(__file__).resolve().parents[2]
NEW=ROOT/'build-ci/20261106-managed-file-type-inventory-source-r4'
OLD=ROOT/'build-ci/20261105-managed-file-type-inventory-source-r3'
OUT=ROOT/'build-ci/20261106-managed-file-type-inventory-launcher-independent-review-r4'
if OUT.exists():raise RuntimeError('PRESERVE_PRIOR_REVIEW')
OUT.mkdir()
checks=[];inputs={}
def sha(data):return hashlib.sha256(data).hexdigest().upper()
def read(path):
 path=path.resolve();data=path.read_bytes()
 try:name=path.relative_to(ROOT).as_posix()
 except ValueError:name=path.as_posix()
 inputs[name]={'path':name,'bytes':len(data),'sha256':sha(data)}
 return data
def ck(name,value):
 checks.append({'id':name,'pass':bool(value)})
 if not value:
  (OUT/'failed-checks.json').write_text(json.dumps(checks,indent=2)+'\n',encoding='utf-8',newline='\n')
  raise RuntimeError(name)
def pin(row,label):
 p=pathlib.Path(row['path']);p=p if p.is_absolute() else ROOT/p
 data=read(p);ck(label,len(data)==row['bytes'] and sha(data)==row['sha256'])

new_data=read(NEW/'run-reader.py');text=new_data.decode();old_data=read(OLD/'run-reader.py');old=old_data.decode()
ck('NEW_LAUNCHER_SEAL',sha(new_data)=='ABE5C20945428F69D494339F94EBB8F058FC98D0C3FCC988F3E7C5B1A53DBE54')
ck('OLD_LAUNCHER_SEAL',sha(old_data)=='1275FF05A85465C7FFC46C59D4579355A06C66FB7AE67D93406469630615A66B')
manifest_data=read(NEW/'source-pins.json');manifest=json.loads(manifest_data)
ck('R4_MANIFEST_SEAL',sha(manifest_data)=='10E3854D27239A9BDD1BDF0F798C57578B6D629B4EA9DD7F52A30A8861A11E89')
review_path=ROOT/'build-ci/20261106-managed-file-type-inventory-independent-review-r4/review.json'
review_data=read(review_path);review=json.loads(review_data)
ck('ROOT_WHOLE_READER_REVIEW_SEAL',sha(review_data)=='F4C2A7CE2323413F1859E2DE022BA9394D953279EB74030DEAFDB19C83172709')
ck('ROOT_WHOLE_READER_VERDICT',review['verdict']=='ACCEPT_SOURCE_ONLY_PUBLIC_FILE_METADATA_R4_NOT_EXECUTION_OR_LAYOUT_QUALIFICATION')
host_path=ROOT/'build-ci/20261105-managed-payload-abi-probe-independent-review-r2/review.json'
host_data=read(host_path);host=json.loads(host_data)
ck('HOST_REVIEW_SEAL',sha(host_data)=='57795D482F39EBDE2C965F6AB66BE533438EE71AC1590EDF820593ADB2014DFE')
for label,rows in [('READER_REVIEW',review['verified_files']),('HOST_REVIEW',host['verified_files']),('TARGET_INPUT',manifest['target_inputs']),('DEPENDENCY',manifest['reader_dependency_file_pins']),('MANIFEST_SOURCE',manifest['source_files'])]:
 for at,row in enumerate(rows):pin(row,label+'_'+str(at))
profile=ROOT/'build-ci/20261105-managed-file-type-inventory-source-r1/compilation-reference-profile.json'
profile_data=read(profile)
ck('DEFAULT_REFERENCE_PROFILE_SEAL',sha(profile_data)=='229900F0D91E29589B2DBE9584CDDC9812BFDB99625ED8924494BA093658C3F3')
refs=json.loads(profile_data)['default_reference_file_pins']
ck('DEFAULT_REFERENCE_ROWS_167',len(refs)==167)
for at,row in enumerate(refs):pin(row,'DEFAULT_REFERENCE_'+str(at))
runtime=pathlib.Path(manifest['target_inputs'][0]['path']).parent
ck('EXACT_REF_DIRECTORY_CONTENTS',{x.name for x in (runtime/'ref').glob('*.dll')}=={pathlib.Path(x['path']).name for x in refs})

# Independent whole-file normalization, not execution of the author's generator.
normalized=text
replacement_pairs=[
 ('metadata revision4 attempt','metadata revision3 attempt'),
 ('20261106-managed-file-type-inventory-independent-review-r4/review.json','20261105-managed-file-type-inventory-independent-review-r3/review.json'),
 ('F4C2A7CE2323413F1859E2DE022BA9394D953279EB74030DEAFDB19C83172709','5E870B52977ED4F984953DF2E78CEDD012BC7C62554398D768F9F959D1271EBD'),
 ('10E3854D27239A9BDD1BDF0F798C57578B6D629B4EA9DD7F52A30A8861A11E89','3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2'),
 ('PUBLIC_FILE_METADATA_SOURCE_R4_ACTUAL_ATTEMPT_R1','PUBLIC_FILE_METADATA_SOURCE_R3_ACTUAL_ATTEMPT_R1'),
 ('FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V4','FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V3'),
 ('inventory["required_named_types"] == 84','inventory["required_named_types"] == 22'),
 ('"../20261106-controller-v-wrapper-source-closure-r1/author-metadata-r4.py", "../20261106-author-metadata-r4-root-launcher.py"','"../20261105-author-metadata-r3-source.py", "../20261105-finalize-metadata-r3-source.py"')]
for after,before in replacement_pairs:
 ck('EXPECTED_SCALAR_DELTA_'+after,normalized.count(after)==1);normalized=normalized.replace(after,before)
ck('EXACT_R13_SCOPE_LABELS',normalized.count('R13')==4)
normalized=normalized.replace('R13','R12')
new_a=normalized.index('expected = {(filename, name)')
new_b=normalized.index('receipt["first_green"] = True',new_a)
old_a=old.index('expected = set(manifest["allowlist_required"])')
old_b=old.index('receipt["first_green"] = True',old_a)
normalized=normalized[:new_a]+old[old_a:old_b]+normalized[new_b:]
ck('WHOLE_LAUNCHER_IDENTICAL_OUTSIDE_DECLARED_SCHEMA_PIN_OUTPUT_VALIDATION_DELTAS',normalized==old)
validation=text[text.index('expected = {(filename, name)'):text.index('receipt["first_green"] = True')]
for literal in [
'for filename, names in manifest["allowlist_by_file"].items() for name in names',
'expected.add(("Newtonsoft.Json.dll", generated))',
'expected.update((row["file"], row["qualified_name"]) for row in presence if row["present"])',
'85 <= inventory["reported_types"] <= 88',
'inventory["reported_types"] == len(inventory["types"]) == len(expected)',
'{(row["file"], row["qualified_name"]) for row in inventory["types"]} == expected',
'row["file_bytes"] == pin["bytes"] and row["file_sha256"] == pin["sha256"]',
'len(row["declared_nonstatic_instance_fields"]) == row["declared_nonstatic_instance_field_count"]',
're.fullmatch(r"0x02[0-9A-F]{6}", row["metadata_token"])',
're.fullmatch(r"0x04[0-9A-F]{6}", field["metadata_token"])',
're.fullmatch(r"(?:[0-9A-F]{2})+", field["signature_blob_hex"])',
'field["signature_blob_hex"].startswith("06")']:
 ck('NEW_VALIDATION_'+literal,literal in validation)
ck('ONLY_VALIDATION_READS_AND_ASSERTIONS',all(line.startswith(('expected','input_pins','for ','    ','assert ')) or not line for line in validation.splitlines()))
ck('ASSERTIONS_CANNOT_BE_DISABLED_WITHOUT_STOPPING',text.index('if not __debug__:')<text.index('assert os.name == "nt"') and 'raise RuntimeError("UNOPTIMIZED_PYTHON_REQUIRED")' in text)
ck('FIRST_GREEN_AFTER_ALL_NEW_VALIDATION',text.index('receipt["first_green"] = True')>text.index('assert field["signature_blob_hex"].startswith("06")'))
ck('RAW_ARTIFACTS_BEFORE_POST_PINS_AND_VALIDATION',text.index('(RUN / "inventory.json").write_bytes(stdout)')<text.index('receipt["all_pre_post_pins_match"]')<text.index('inventory = json.loads'))
ck('PRE_POST_PINS_EXACTLY_TWO_CALLS',text.count('\nassert_pins()\n')==2)
ck('ONE_PROCESS_CREATION',text.count('subprocess.Popen(')==1)
ck('ONE_ATTEMPT_NOT_OVERWRITTEN','assert not RUN.exists(), "Preserve every actual attempt"' in text)
ck('HIDDEN_NO_INPUT','creationflags=subprocess.CREATE_NO_WINDOW, startupinfo=startup' in text and 'stdin=subprocess.DEVNULL' in text and 'startup.wShowWindow = subprocess.SW_HIDE' in text)
ck('NO_PROFILE_NONINTERACTIVE_FIXED_COMMAND','"-NoLogo", "-NoProfile", "-NonInteractive", "-File", str(SOURCE / "run-reader.ps1")' in text)
ck('TIMEOUT_KILL_REAP_AND_ERROR_MODE_FINALLY','stdout, stderr = child.communicate(timeout=60)' in text and 'child.kill()\n            stdout, stderr = child.communicate()' in text and 'finally:\n    kernel.SetErrorMode(old_mode)' in text)
ck('NO_EXEC_IMPORT_OF_LAUNCHER_IN_REVIEW',True)
ck('FROZEN_SOURCE_READER_HASH',sha(read(NEW/'reader.cs'))=='542BB4126F6812ED83BBD0819A6F1464B8ECD7FFAE99FF36B22D8428EC43521F')
ck('FROZEN_PS_WRAPPER_HASH',sha(read(NEW/'run-reader.ps1'))=='8069BD26AEBF064A57835D9852793A8F9266354DD1515FCA7C2E50535CFB53DC')
proposal_data=read(NEW/'root-launcher-proposal.json');proposal=json.loads(proposal_data)
for key,value in [('subject_manifest_sha256',sha(manifest_data)),('subject_reader_sha256',sha(read(NEW/'reader.cs'))),('independent_source_review_sha256',sha(review_data)),('launcher_sha256',sha(new_data)),('powershell_wrapper_sha256',sha(read(NEW/'run-reader.ps1'))),('predecessor_launcher_sha256',sha(old_data))]:ck('PROPOSAL_PIN_'+key,proposal[key]==value)
author=read(ROOT/'build-ci/20261106-author-metadata-r4-root-launcher.py')
ck('AUTHOR_HELPER_ONLY_SOURCE_CREATION',b'subprocess' not in author and b'ctypes' not in author and b'exec(' not in author)
ck('NO_ATTEMPT_OUTPUT_CREATED',not (NEW/'execution-r1').exists())
ck('UTF8_LF_LAUNCHER',b'\r' not in new_data and new_data.endswith(b'\n'))

def ln(needle):return text[:text.index(needle)].count('\n')+1
coverage=[
 {'lines':[1,42],'finding':'Exact source/review/host/default reference seals and all referenced file pins; file-only read-before-launch checks.'},
 {'lines':[43,58],'finding':'Assertions enforced by unconditional __debug__ rejection; Windows-only, preserved one attempt, fixed AMD64 PE32+ console subsystem host.'},
 {'lines':[59,78],'finding':'Eleven inherited ordinary directory keys only; PATH constructed explicitly; no environment values persisted. This is purpose-host isolation, not the separate Clock N proof.'},
 {'lines':[81,106],'finding':'Receipt initialized before process creation; CREATE_NO_WINDOW and SW_HIDE with stdin null/stdout+stderr redirected, primary communicate timeout60, kill/reap on timeout, parent error-mode restoration in finally.'},
 {'lines':[108,132],'finding':'Raw bytes and hashes saved before post-pin and complete-output admission. Raw0, no timeout, stderr0, <=524288 bytes, V4/status/84+3+1 flags all required.'},
 {'lines':[133,159],'finding':'Exactly one pinned JObject generated name, exactly three optional names and Boolean flags; complete expected set keyed by file+qualified_name. Count equality plus set equality excludes omissions, extra rows and duplicate rows, including same qualified names in distinct DLLs.'},
 {'lines':[150,159],'finding':'Each emitted row must carry the exact per-file byte/hash identity; type/field table-token prefixes and nonempty even uppercase field-signature hex beginning06 are independently checked. Reader source already owns complete signature decoding; launcher does not infer layout.'},
 {'lines':[160,len(text.splitlines())],'finding':'First-green is written only after every transport assertion; complete source/author/PS/Python hash freeze follows. No Clock gate or resource/ABI conclusion is written.'}
]
limits=[
 'ACCEPT is scoped to this root-authored source launcher and one separately authorized fixed-file metadata purpose attempt; it does not execute or accept the clock source, any target helper or a compiler attempt in this review.',
 'The 60-second argument bounds the primary communicate wait. The inherited timeout path kills its own child then reaps without a second timeout; this review does not certify a hard 60-second end-to-end host duration.',
 'The 524288-byte admission check is post-capture, not a streaming allocation cap. Reader output is fixed-file metadata only; no general arbitrary-child output resource theorem is claimed.',
 'Separate root execution must pin this reviewed launcher and use unoptimized Python; a preserved raw failure remains a failure. No source helper or subject was executed here.',
 'Metadata field shapes do not certify live type/object sizes, offsets, allocations, source-binary association, code-page profile, environment closure, resource fit or R13 GO.'
]
review_out={'schema':'R4_ROOT_LAUNCHER_INDEPENDENT_SOURCE_REVIEW_V1','verdict':'ACCEPT_SOURCE_ONLY_NARROW_PUBLIC_FILE_METADATA_OUTER_LAUNCHER','check_count':len(checks),'all_checks_pass':all(x['pass'] for x in checks),'subject_launcher_sha256':sha(new_data),'subject_manifest_sha256':sha(manifest_data),'whole_predecessor_delta_verified':True,'coverage':coverage,'limits':limits,'execution':{'subject_launcher':False,'author_helper':False,'PowerShell_parse_AST_import':False,'compiler':False,'metadata_reader':False,'Clock_target':False,'GUI':False,'Git':False},'verified_files':list(inputs.values())}
md='# Independent R4 outer launcher source review\n\nACCEPT, scoped to the frozen root-authored outer launcher and a separately authorized fixed-file metadata purpose attempt. The reviewer read the whole launcher, predecessor and author helper; no subject code, compiler, metadata reader or PowerShell parser/import/helper was executed.\n\n'+str(len(checks))+' own file/hash/source checks passed at the first checker run. The entire R3 launcher normalizes byte-exact after only the declared R4 lineage/pin/schema/count labels, per-file output validation and author-freeze path changes. All current source, host-review, target, dependency and167 compiler reference pins were rehashed.\n\nThe new output admission uses (file, qualified_name). Exact count and pair-set equality reject missing/extra/duplicate rows while preserving distinct DLL definitions with the same qualified name. Each row must reproduce its pinned file length/hash and field count; table token/signature shapes are checked before first-green. The pinned reader remains responsible for complete metadata and signature decoding.\n\nThe fixed console host, no profile/noninteractive command, explicit environment allowlist, CREATE_NO_WINDOW/SW_HIDE, null stdin, redirected outputs, raw failure preservation, pre/post pins, primary timeout/kill/reap, error-mode restoration and first-green freeze retain the predecessor behavior. These facts authorize no Clock execution or fit conclusion.\n\n'+ '\n'.join('- '+x for x in limits)+'\n'
(OUT/'review.md').write_text(md,encoding='utf-8',newline='\n')
(OUT/'review.json').write_text(json.dumps(review_out,indent=2)+'\n',encoding='utf-8',newline='\n')
(OUT/'checks.json').write_text(json.dumps({'schema':'OWN_READONLY_SOURCE_CHECKS_V1','checks':checks,'count':len(checks)},indent=2)+'\n',encoding='utf-8',newline='\n')
(OUT/'launcher.diff').write_text(''.join(difflib.unified_diff(old.splitlines(True),text.splitlines(True),fromfile='R3',tofile='R4')),encoding='utf-8',newline='\n')
pins={'schema':'R4_ROOT_LAUNCHER_INDEPENDENT_REVIEW_PINS_V1','verdict':review_out['verdict'],'verified_inputs':list(inputs.values()),'review_files':[]}
for name in ['review.md','review.json','checks.json','launcher.diff']:
 p=OUT/name;data=p.read_bytes();pins['review_files'].append({'path':p.relative_to(ROOT).as_posix(),'bytes':len(data),'sha256':sha(data)})
pins['checker']={'path':pathlib.Path(__file__).relative_to(ROOT).as_posix(),'bytes':pathlib.Path(__file__).stat().st_size,'sha256':sha(pathlib.Path(__file__).read_bytes())}
(OUT/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'verdict':review_out['verdict'],'checks':len(checks),'files':pins['review_files'],'pins_sha256':sha((OUT/'source-pins.json').read_bytes())},indent=2))
