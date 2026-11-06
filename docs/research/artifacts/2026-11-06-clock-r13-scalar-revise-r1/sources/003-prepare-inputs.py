"""Author new unaccepted R13 JSON operands from file/text lineage; no target load."""
from pathlib import Path
import hashlib
import json
import re

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
BASE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
NEW=BASE/'clock-oracle-r13-streamed-dependency-source-r1'
OLD=BASE/'clock-oracle-r10-source-prototype'
src=(NEW/'run-clock.ps1').read_text(encoding='utf-8')
old_src=(OLD/'run-clock.ps1').read_text(encoding='utf-8')
worksheet=json.loads((ROOT/'build-ci/20261105-clock-oracle-r10-runner-census/census.json').read_bytes())

def digest(b):return hashlib.sha256(b).hexdigest().upper()
def bodies(s):
    end=s.index('\ntry{\n # Hash literal manifest')
    anchors=list(re.finditer(r'^function ([A-Za-z0-9-]+)(?=\(|\{)',s[:end],re.M))
    return {m[1]:dict(text=s[m.start():anchors[i+1].start() if i+1<len(anchors) else end],first_line=s[:m.start()].count('\n')+1) for i,m in enumerate(anchors)}

actual=bodies(src);historical=bodies(old_src)
models={}
for name,body in actual.items():
    old_rows=[r for r in worksheet['helper_rows'] if r['name']==name]
    text=body['text']
    models[name]=dict(name=name,source_first_line=body['first_line'],source_last_line=body['first_line']+len(text.splitlines())-1,source_bytes=len(text.encode()),source_sha256=digest(text.encode()),whole_source=text,old_worksheet_rows=old_rows,old_worksheet_rows_status='UNACCEPTED_OPERANDS_NOT_R13_PRICES',whole_text_equal_R10=name in historical and text==historical[name]['text'],audit_scope='Every written body/control/caller/default/primitive/result/cleanup region must be sponsored by new complete equations; no missing-body0,oldtotalorunusedslack.')

fresh={
 'Get-TaskHash':dict(formula='289+65*B',groups=dict(parameters_conversion_and_return=5,first_line=43,locals=12,FileStream=25,open_length=9,IncrementalHash=10,loop_fixed=6,EOF=10,Tuple2_direct_result=12,finally_callers_and_Complete=24,two_CloseOwner=124,catch=9),excluded=['NoLink','Canonical']),
 'Get-TaskHashHex':dict(formula='14',groups=dict(entry_parameter_return=3,Item2_direct_cast_argument=7,Convert_type_dispatch_entry_return=4)),
 'Test-TaskHashValue':dict(formula='hex1477_or_binary647',hex_fixed=dict(entry_parameters=3,digest_extent=9,string_kind=4,string_extent=9,alphabet=2,for_fixed=8,return_true=2),hex_body=dict(byte_local=6,expected_high=6,alphabet_high=6,high_compare=1,or_operator=1,expected_low=8,alphabet_low=6,low_compare=1,branch=1),for_each_iteration=9,iterations=32,binary_fixed=39,binary_body=10,refusal_scope='Size/kind/late-mismatch prefixes; no fake terminal/advance on early return.'),
 'Assert-TaskFreshHashIdentity':dict(formula='25+Test;refusal replaces normalreturn with83 =>delta82',normal_groups=dict(entry_parameters=5,expected_count_read=1,observed_count=6,comparison_or_not=3,Test_caller=8,branch=1,return_=1),refusal_groups=dict(expected_projection=12,observed_projection_with_Hex=17,witness=20,Reject_caller=2,Reject_body=32),excluded=['Test-TaskHashValue']),
 'Assert-TaskPins':dict(formula='90+50NS+38EG+52*own4+36I+NS*(4+I)',own4_groups=dict(foreach=5,guard=19,Join=11,count_capture=5,hash_capture=5,fresh_identity_caller=7),input_groups=dict(foreach=5,Resolve_caller_and_destination=6,count_capture=5,hash_capture=5,fresh_identity_caller=7,sum=8),excluded=['FileSet','walker','NoLink','Canonical','EntryPath','Resolve','Hash','FreshIdentity','TestHashValue']),
 'Get-TaskEntries':dict(formula='54+155D+(196+64N)E+max(0,126+64N-26)',fixed_groups=dict(entry_parameters=5,rows_local=2,null_test=4,List=5,Queue=5,initial_Enqueue_and_Canonical_caller=14,seen=2,outer_false_and_setup=9,ToArray_comma_and_return=8),directory_groups=dict(queue_loop=9,Dequeue=6,path=5,depth=6,depth_gate=4,NoLink_caller=2,owner_locals=6,iterator=11,iterator_false=9,catch=9,finally=88),candidate_groups=dict(loop=9,candidate=7,string_fixed=53,Canonical_caller=3,seen=3,count_gate=4,Spend=20,path_gate=9,attributes=7,directory_flag=8,reparse_flag=8,length_alternative=15,Tuple5_and_Add=25,reparse_gate=2,recursive_gate=4,depth_gate=4,Enqueue=15),overflow_groups=dict(loop=9,candidate=7,string_fixed=53,Canonical_caller=3,seen=3,count_gate=4,witness=13,Reject_caller_and_body=34),replaced_normal_terminal_groups=dict(outer_false=9,ToArray_comma_return=8,iterator_false=9),excluded=['NoLink','Canonical']),
 'Get-TaskStageBytes':dict(formula='21*S+73*rows;rows<=6S+3 forS>0',scope='Six actual allowed names, plus at most three first-refusing seventh rows; walker separate.'),
 'Measure-TaskJson':dict(formula='104+Ddefault+Visitor',groups=dict(entry_parameters=3,codec_caller_and_body=19,profile=12,long3_constructor_and_cap_store=12,Visitor_caller=4,finalLF_bound_and_caller=31,measurement_return_map=23)),
 'Read-TaskBoundedBytes':dict(formula='198',changed_group=dict(direct_comma_return=4),scope='All other current source groups separately summed from worksheet operands after whole-source audit.'),
 'Close-TaskJsonReader':dict(formula='max7,12,17,62=62',scope='Complete source matches CloseOwner under Close for Dispose method token, same parameter/primitive/outcome action counts; no CPU internals.'),
 'Read-TaskDependencyJson':dict(formula='357',groups=dict(entry_parameters=3,owner_locals=10,MemoryStream=15,StreamReader_and_Encoding=22,JsonReader_and_CloseInput=14,Settings=5,four_enum_setters=36,two_scalar_setters=14,Serializer_CreateDefault=7,Deserialize_comma_return=10,catch=9,finally=212),finally_groups=dict(CloseJsonReader=64,CloseText=64,CloseMemory=64,CompleteOwner=20)),
 'Get-TaskDependencyString':dict(formula='97+64N',groups=dict(entry_parameter=2,local=2,exact_JValue_String_guard=32,payload_getter=6,AssertString_fixed_with_caller=53,return_=2)),
 'Read-TaskJson':dict(formula='pair662+ReadDependency357+RawBodies',pair_groups=dict(two_common=622,captured_identity_variants=24,dependency_profile_argument=19,predecessor_profile_argument=17,remaining_old_variant=6,bounded_comma_element_corrections=2,removed_dependency_text_tail=-34,new_dependency_reader_caller_comma=6),scope='Common rows rebuilt; bounded read bodies included here; raw grammar and new private reader separate.'),
 'Encode-TaskJson':dict(formula='38*(L-1)+176',changed_group=dict(direct_comma_return=4)),
 'Get-TaskSecondaryCodes':dict(formula='506',groups=dict(entry_List=12,length_for=167,eleven_rows=319,ToArray_comma_return=8)),
 'Save-TaskReceipt':dict(formula='686+2*DocumentWrite+Encode+Visitor',groups=dict(own_statements_callers=180,secondary_projection=506),scope='Includes Measure104, which is removed once when globally composing all Measure bodies.'),
 'Get-TaskReferences':dict(formula='54+39E+101F',excluded=['walker','NoLink','Hash','FileSet','Hex']),
 'Get-TaskBuildArtifacts':dict(formula='47+101E',excluded=['walker','Hash','FileSet','Hex']),
 'Assert-TaskDependencies':dict(formula='442+64SourceN+Includes*(115+64N)',fixed_groups=dict(entry=2,report_path_caller=9,locals=4,descriptor_scan=50,match=4,ReadJson_caller=5,root=22,Data_getter=12,Data_type=22,Source_caller=13,source_comparison=13,optional_arrays=112,Includes_getter=12,Includes_guard=46,foreach_fixed=4,summary=15),include_own=18,excluded=['ReadJson','Canonical','Resolve','GetDependencyString']),
 'Invoke-TaskFencedChild':dict(formula='107+Ddefault+2*Pins+Child+binary*(64+Hash+FreshIdentity)',scope='Original69 branch replaces expected-map caller with scalar property arguments; Fresh/Test body explicit once, no old normalIdentity24 inside.'),
}
for name,data in fresh.items():models[name]['new_source_equation']=data

# Explicit new proposed equations for the entire remaining body set. Their old
# source/count operands are visible and unaccepted, never imported as prices.
same_equations={
 'Spend-Task':'normal17+actual_long_conversion;refusal<=15',
 'Test-TaskPathCatalogueContext':'2+3+18+3=26',
 'Add-TaskPathCatalogueEntry':'true62;false118',
 'Initialize-TaskPathCatalogue':'209+73I+133*own4+118+136*(references+build+controls)+133*stageNames',
 'Get-TaskCatalogueString':'hit21;miss41',
 'Get-TaskCatalogueEntryPath':'each71+(9529-71)*firstBuildIndicator',
 'Get-TaskCanonicalPath':'own76+EntryPathIfSelected',
 'Assert-NoLink':'own7254+Canonical',
 'Assert-TaskPollAdmission':'admit11;refusal27+Reject32=59',
 'Invoke-TaskChild':'own1044811+74*argc+53*(argUnits+8envUnits)+positiveReads*Stage',
 'Add-TaskChildCleanup':'own2417;included in13 scheduled child cleanup slots, not second composition',
 'Set-TaskRawExit':'own29;included in child footer alternative',
 'Replace-TaskBounded':'84helpers:53*(inputUnits+replacementUnits+tokenUnits)+217*84+18*18+61*38+66+4*86',
 'Reject-TaskGate':'32;actual error branches own callers and witness construction separate',
 'Expect-TaskGate':'93+25*witnessKeys+ActualSelectedAction',
 'Add-TaskJsonVisit':'17+actual_long_conversion<=18',
 'Get-TaskRuntimeType':'null8;actualObjectGetType16',
 'Get-TaskBootstrapHash':'279+49D+45B;sevenactualD<=64,379chunks',
 'Initialize-TaskController':'2136+53*14*512+7*(279+49*64)+45*379',
 'Assert-TaskString':'64N+49+Ddefault;ASCII53N+49',
 'Get-TaskSafePrefix':'ASCII private caller51;general surrogate alternative54',
 'Assert-TaskIntegral':'<=61;actual eight kind tests and nonnegative branch',
 'Convert-TaskControlOperand':'builtin89;BigInteger53;range_refusal54',
 'Assert-TaskExactKeys':'71+81K+64*keyUnits',
 'Set-TaskSecondary':'44 first-unsaturated;other branches prefixes',
 'Close-TaskOwner':'null7,normal12,firstFailure17,secondaryFailure62',
 'Complete-TaskOwnerClose':'normal16;propagate17',
 'Assert-TaskCodecBinding':'normal18;refusal19',
 'Get-TaskIntegerDigits':'negative309;unsigned307',
 'Get-TaskEscapedStringBytes':'148N+75',
 'Add-TaskJsonBound':'27+actual_long_conversion<=28;typed long[] identity',
 'Visit-TaskJsonValue':'null86;string184+148N;bool115;int455;map185+223K+31separators+212keyUnits+children;array190+21E+31separators+children',
 'Join-TaskBoundedNames':'66+87K+64*nameUnits',
 'Copy-TaskOwned':'own285+70B;twoNoLink separate',
 'Mutate-TaskOwnedBytes':'263',
 'Assert-TaskRawJson':'512L+106;full original token-state grammar',
 'Assert-TaskRawJsonBytes':'94+536L+max(failure44,compaction51L+21)<=587L+138',
 'Assert-TaskDocumentWrite':'64PathN+91+Stage',
 'Write-TaskJsonBytes':'34+DocumentWrite',
 'Write-TaskJson':'59+Ddefaults+2DocumentWrite+Encode+MeasureIfNeeded;Visitor separate',
 'Assert-TaskIdentity':'equal24;refusal93;descriptor-only callers',
 'Assert-TaskBoundSourceInteger':'positive54;either actual refusal<=110',
 'Assert-TaskReservation':'normal20;refusal caller/witness+Reject in actual controls',
 'Assert-TaskRetainedReservation':'normal16;refusal caller/witness+Reject in actual controls',
 'Assert-TaskAttempt':'normal6;refusal48',
 'Assert-TaskPredecessor':'3557+62=3619',
 'Get-TaskPredecessorDescriptor':'2+4988+32824+3448+300+35+38=41635',
 'Resolve-TaskInput':'own64NameN+135;EntryPath separate',
 'Assert-TaskFileSet':'normal100+83N+78M+64sumUnits;refusal169+90N+87M+64sumUnits+twoJoins',
 'Assert-TaskSameReferences':'30+51F includesF descriptorIdentity24',
 'Get-TaskPEHeader':'333',
 'Assert-TaskConsolePE':'normal31;actualGUIrefusal93',
 'Read-TaskChildResult':'21557+38TextN+12630Controls+8ControlRawCallers+15SummaryRawCallers;Raw/FileSet separate',
 'Save-TaskFailureInventory':'193+64FileRows;walker/WriteJson separately composed',
}
for name,formula in same_equations.items():
    if name in models:models[name]['new_source_equation']=dict(formula=formula,whole_source_audit='Current written source and all declared groups/paths are proposed anew; old row operands are retained explicitly for independent whole critique, not accepted call prices.')

payload=dict(schema='R13_COMPLETE_BODY_OPERANDS_UNACCEPTED_V1',subject_manifest_sha256='10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3',source=NEW.relative_to(ROOT).as_posix(),models=models,worksheet_identity=dict(path='build-ci/20261105-clock-oracle-r10-runner-census/census.json',sha256=digest((ROOT/'build-ci/20261105-clock-oracle-r10-runner-census/census.json').read_bytes())),old_new_family_operands=worksheet['new_family_groups'],old_raw_derivation_operands=worksheet['byte_lexical_derivation'],old_graph_operands=worksheet['graphs'],old_main_rows_operands=worksheet['main_and_bootstrap'],old_convention_operands=worksheet['convention'],status='UNACCEPTED_SOURCE_OPERANDS_NEW_WHOLE_CHECK_NOT_YET_RUN')
dest=HERE/'body-operands.json'
if dest.exists():raise ValueError('Preserve authored operand file')
dest.write_text(json.dumps(payload,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='SOURCE_OPERANDS_AUTHORED_NOT_FULL_PASS',bodies=len(models),unmodeled=sorted(set(models)-set(fresh)-set(same_equations)),sha256=digest(dest.read_bytes()))))
