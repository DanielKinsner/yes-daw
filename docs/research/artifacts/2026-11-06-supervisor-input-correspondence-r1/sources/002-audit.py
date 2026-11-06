"""Focused source/text/integer correspondence; no target parsing or execution."""
import hashlib,json,pathlib,re
root=pathlib.Path(__file__).resolve().parents[2];here=pathlib.Path(__file__).parent
if (here/'audit.json').exists():raise RuntimeError('PRESERVE_FIRST_PASS')
base=root/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
src=(base/'run-clock.ps1').read_bytes();text=src.decode();manifest=json.loads((base/'source-manifest.json').read_bytes())
digest=lambda b:hashlib.sha256(b).hexdigest().upper()
assert digest(src)=='9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788'
assert digest((base/'source-manifest.json').read_bytes())=='10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3'
suffixes=re.findall("'([^']*)'",text.split('$taskSuffixes=@(',1)[1].split(')',1)[0])
assert len(suffixes)==7 and max(map(len,suffixes))==48
assert "if($Path.Length -gt 512){throw 'PATH_LENGTH_BEFORE_NORMALIZATION'}" in text
assert 'Assert-NoLink $Source;Assert-NoLink $Destination;' in text
assert text.index('Copy-TaskOwned $taskSourceBundle $taskMutantBundle')<text.index("Invoke-TaskFencedChild 'one-opaque-compile'")
assert "'/controls/predecessor-mutant.json'" in text
assert "<BUILD>/source-dependencies.json" in manifest['compiler']['arguments']
assert 'REPLACEMENT_LENGTH_BEFORE_COPY' in text and 'Assert-TaskString $taskArgument 512 $true' in text
assert "if($taskValue -ceq '<OWNED_OPAQUE_TEMP>'){$taskValue=$taskOpaqueRoot}" in text
assert set(manifest['environment'])=={'SystemRoot','windir','PATH','INCLUDE','LIB','TEMP','TMP','VSLANG'}
effective_root=512-len('/controls/predecessor-mutant.json')
assert effective_root==479
bounds={'raw_NewRunRoot':512-max(map(len,suffixes)),'effective_canonical_root_before_every_child':effective_root,'working_directory_including_possible_single_trailing_separator':effective_root+1,'expanded_opaque_temp':effective_root+len('/opaque-temp'),'stage':effective_root+len('/stage'),'transport':effective_root+len('/transport'),'new_request_path':effective_root+len('/transport/child-0.request.bin'),'new_terminal_path':effective_root+len('/transport/child-0.terminal.bin'),'independent_compiler_replacement_effective_root':512-len('/build/source-dependencies.json')}
assert bounds=={'raw_NewRunRoot':464,'effective_canonical_root_before_every_child':479,'working_directory_including_possible_single_trailing_separator':480,'expanded_opaque_temp':491,'stage':485,'transport':489,'new_request_path':509,'new_terminal_path':510,'independent_compiler_replacement_effective_root':481}
assert len(manifest['compiler']['arguments'])==28
env_lengths={key:len(value) for key,value in manifest['environment'].items()}
assert all(n<=512 for n in env_lengths.values())
result={'schema':'FOCUSED_R13_SUPERVISOR_INPUT_CORRESPONDENCE_R1','verdict':'NO_ADMITTED_EXPANDED_ENV_512_MISMATCH_UNDER_SELECTED_PATH_RECIPE','bounds':bounds,'source_lines':{'initial_paths_and_seven_suffixes':[20,32],'per_child_final_args_and_environment':[186,204],'replacement_guard':[283,291],'AssertNoLink_input_guard':[141,144],'copy_AssertNoLink':[500,503],'canonical_root_and_prelaunch_sequence':[959,993],'native_six_arguments':[998,1006]},'reasoning':['Raw suffix guard bounds raw NewRunRoot464; it is not blindly transferred through GetFullPath/8.3 expansion.','Before any child, predecessor Copy-TaskOwned checks its destination via Assert-NoLink input512. Under the selected stock filesystem Join-Path correspondence, effective canonical root plus33<=512; a possible single trailing separator adds at most one to working-directory representation.','Therefore expanded opaque-temp is<=491, stage<=485, transport<=489 and fixed new request/terminal paths<=509/510. Later native children are dominated by the same completed precompiler prefix.','The actual compiler source-dependencies argument independently requires build-root/source-dependencies.json<=512 through checked Replace-TaskBounded and final Invoke-TaskChild guard.','All target arguments still receive the final ASCII512 guard, with counts28/6/6. Non-placeholder environment values receive ASCII512 before assignment; actual TEMP/TMP placeholders expand to the already bounded opaque root.','This is current-source correspondence, not target execution, universal filesystem/provider behavior, whole payload/fit, native binding or GO credit. Earlier conditional path/SDK/profile premises remain explicit.'],'actual_manifest_environment_value_lengths_before_expansion':env_lengths,'target_execution':False,'subjects_preserved':True}
raw=(json.dumps(result,indent=2)+'\n').encode();(here/'audit.json').write_bytes(raw)
inputs=[]
for path in [base/'run-clock.ps1',base/'source-manifest.json',root/'docs/research/clock-supervisor-protocol-v1.md']:
 data=path.read_bytes();inputs.append({'path':path.relative_to(root).as_posix(),'bytes':len(data),'sha256':digest(data)})
pins={'schema':'FOCUSED_INPUT_CORRESPONDENCE_PINS_R1','inputs':inputs,'audit_sha256':digest(raw),'own_checker_sha256':digest(pathlib.Path(__file__).read_bytes()),'no_target_execution':True}
(here/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'verdict':result['verdict'],'bounds':bounds,'audit_sha256':digest(raw),'pins_sha256':digest((here/'source-pins.json').read_bytes())},indent=2))
