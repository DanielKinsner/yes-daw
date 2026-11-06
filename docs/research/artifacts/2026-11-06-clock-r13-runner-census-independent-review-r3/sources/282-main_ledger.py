"""Root source-action proposal; no target or prior helper is imported."""
ROWS = []


def group(label, **roles):
    return dict(label=label, roles=roles, units=sum(roles.values()))


def row(lines, label, *groups, note='', multiplicity=1):
    ROWS.append(dict(lines=list(lines), label=label, multiplicity=multiplicity,
                     groups=list(groups), units=multiplicity * sum(g['units'] for g in groups), note=note))


# Bootstrap: script-parameter metadata is not re-evaluated as Type values.
row((1,7), 'script entry/four actually supplied string parameter destinations', group('script entry/inputs', E=1,W=4),
    note='Exact fresh-controller argv supplies all four strings; metadata/compiler/binder internals stay at the existing runtime boundary, not expanded source loops.')
row((8,8), 'typed ErrorActionPreference Stop assignment', group('string-to-public enum preference', R=1,O=1,W=1),
    note='Selected InitialSessionState.cs4551–4556 gives this automatic variable ArgumentTypeConverterAttribute(ActionPreference).')
row((9,16), 'eight source gates/tracking flags', group('literal Boolean destinations', R=8,W=8))
row((17,17), 'complete six-operand entry guard with refusal',
    group('six written negations and disjunctions', R=6,O=11,B=1), group('throw site', R=1,K=1),
    note='Positive source upper over all written operands; exact frozen false guards terminate, not a claimed reached bootstrap trace.')
row((20,24), 'three admitted caller path scans',
    group('three-element literal array and foreach fixed', element_R=3,element_W=3,array_S=1,foreach_S=1,terminal_K=1,exit_K=1),
    group('three foreach input/condition/body/advance groups', units=3*5),
    group('three complete null/length guards', units=3*22),
    group('three property-bound512 for frameworks', units=3*(13+14*512)),
    group('two actual Char-to-Int32 source operands per ASCII test', units=3*512*18),
    group('three fully qualified API/not/branch groups', units=3*9),
    group('three written refusal sites, each at most once per outer row', R=9,K=9),
    note='Length getter6; indexed4+typeR+Char-to-Int32 O for each cast. ASCII body18=two casts12+two literals2+comparisons2+or1+branch1. No hidden VM-byte fee.')
row((25,25), 'manifest length guard', group('Length/64/ne/branch', getter=6,literal_R=1,O=1,B=1),group('throw',R=1,K=1))
row((26,26), '64 manifest hexadecimal source checks',
    group('literal-bound for framework', units=8+9*64),
    group('indexed character/pattern/notmatch/branch', units=64*(4+1+1+1)),group('single written throw site', R=1,K=1))
row((27,27), 'seven literal suffixes', group('array and named destination', R=7,W=8,S=1))
row((28,28), 'seven derived suffix guards', group('named foreach fixed',units=4),
    group('per suffix foreach and complete expression',units=7*(5+6+1+6+1+1+1)),
    group('written throw at most once per suffix',R=7,K=7))
row((29,29), 'source manifest derived suffix guard', group('complete expression',getter=12,literal_R=1,O=2,B=1),group('throw',R=1,K=1))
row((30,30), 'three GetFullPath produced locals', group('three Type/argument/parameter/DET/destination ports',units=3*7))
row((31,31), 'manifest path and uppercase digest locals', group('static Join2',units=9),group('instance ToUpperInvariant',units=6))
row((32,32), 'four derived directory locals', group('static Join2 four times',units=4*9))
row((33,33), 'four own names literal array', group('array/local/element slots',R=4,W=5,S=1))
row((34,34), 'catalogue null local', group('null/read destination',R=1,W=1))
row((35,35), 'manifest and five integer state locals', group('six source values and destinations',R=6,W=6))
row((37,37), 'rooted byte hash buffer constructor', group('array Type/length/param/DET/local',type_R=1,length_R=1,param_W=1,D=1,E=1,T=1,local_W=1),
    note='CLR array internal zeroing CPU is primitive; all65536 controlled bytes remain a resource owner.')
row((40,41), 'poll maximum and JSON bytes locals', group('two typed Int64 literals and destinations',R=2,W=2))
row((46,46), 'finite JSON visit cap expression', group('nine Int64 literals/eight operators/destination',R=9,O=8,W=1))
row((47,47), 'five raw/log/quarantine/receipt state locals', group('values and destinations',R=5,W=5))
row((48,48), 'integer domain and reviewed limit', group('MaxValue static data and local',type_R=1,field_selection_O=1,field_R=1,W=1),group('null local',R=1,W=1))
row((49,49), 'wrong represented static-field receiver — functionality HOLD',
    group('object Type receiver',type_R=1,receiver_W=1),group('method-name argument',R=1,param_W=1),
    group('written Type[] static-field source and argument',type_R=1,field_selection_O=1,field_R=1,param_W=1),
    group('nominal GetMethod boundary and local',D=1,E=1,T=1,W=1),
    note='This source upper is not a successful field/method observation. The separate static-adapter finding blocks functional promotion; no runtime failure was fabricated.')
row((50,51), 'three null and three Int64 zero locals', group('six values and destinations',R=6,W=6))
row((52,52), 'eleven fixed secondary site strings', group('literal array/local/slots',R=11,W=12,S=1))
row((53,54), 'two fixed secondary arrays', group('Type/Length receiver-boundary/constructor-boundary/local each',units=2*(1+6+3+1)),
    note='Length getter result initializes constructor parameter once; no duplicate parameter destination.')
row((55,55), 'raw primary null', group('literal and local',R=1,W=1))
row((56,56), 'thirty-field ordered receipt literal', group('one selected map/key/slot/local formation',S=1,key_R=30,field_W=30,local_W=1),
    group('27 scalar values',R=27),group('empty List and two empty array produced fields',Type_and_DET=4,two_Type_zero_param_DET=12),
    note='Ctor result destinations fuse with the three field writes. Exactly30 literal fields here; later dynamic schema growth is separate. No phantom ordered Type value/cast.')
row((59,59), 'eighteen signed threshold literals', group('typed array literal/whole conversion/local',type_R=1,array_S=1,array_conversion_O=1,local_W=1),group('literal source elements',R=18,W=18))
row((60,60), 'nineteen unsigned thresholds including Parse', group('typed array literal/whole conversion/local',type_R=1,array_S=1,array_conversion_O=1,local_W=1),group('first18 literal source elements',R=18,W=18),
    group('UInt64 Parse into final element',type_R=1,string_R=1,string_param_W=1,InvariantCulture_Type_DET_param=5,Parse_D_E_T=3,element_W=1),
    note='The fixed UInt64 Parse output writes its literal-array slot once. Typed array coercion is one semantic operation, not hidden per-cell CPU.')
row((61,61), 'twelve summary field strings', group('literal array/local/slots',R=12,W=13,S=1))
row((62,928), '74 source function registrations only',group('one source function-slot registration each',S=74),
    note='The source body/call inventory is74. No helper body, per-invocation signature Type read or hidden compiler/VM construction is charged here.')

# Each main row excludes every own helper body, entry, formal W and return.
# Every written cast/type-check retains O1 even for a runtime identity.
# Implicit argument binding has O only for a nonidentity conversion. A typed
# local annotation is not a written RHS cast. Array-key expressions are4/7.
row((929,932), 'bootstrap manifest read/hash/identity prefix',
    group('ReadBounded caller',Path_R=1,literal_R=1,Int32_Long_O=1,D=1,local_W=1),
    group('nested SHA and hex/expected guard',two_Type_R=2,bootstrap_R=1,two_param_W=2,two_D_E_T=6,operand_W=1,Expected_R=1,compare_O=1,B=1),
    group('throw',R=1,K=1),note='Try/handler metadata is not an additional source loop or hidden CLR unwind expansion.')
row((933,933), 'initialize trusted controller caller',group('own D',D=1))
row((934,934), 'visit and raw JSON caller ports',group('Length into AddVisit(long)',getter_without_dest=5,Int32_Long_O=1,D=1),
    group('raw byte guard caller/local',Bytes_R=1,three_literal_R=3,D=1,local_W=1))
row((935,935), 'UTF8 decoder and GetString produced local',group('encoding into receiver',type_R=1,two_boolean_R=2,two_param_W=2,D_E_T=3,receiver_W=1),
    group('GetString actual args/result',three_R=3,three_param_W=3,D_E_T=3,local_W=1))
row((936,936), 'computed trusted JSON cmdlet and released source references',
    group('computed command and declared boundary',command_R=1,D_E_T=3),
    group('InputObject and presence switch',string_R=1,param_W=1,switch_W=1),
    group('Depth and DateKind enum',depth_literal_R=1,depth_param_W=1,DateKind_literal_R=1,string_enum_O=1,DateKind_param_W=1),
    group('result and two explicit null writes',manifest_W=1,null_R=2,null_W=2))
row((939,939), 'bound Catalogue shape guard',group('binding state operand',chained_index=7,literal_R=1,compare_O=1),
    group('definition count operand',chained_index=7,Count_tail=5,zero_R=1,compare_O=1),group('or/branch/refusal',or_O=1,B=1,throw_R=1,throw_K=1))
row((940,940), 'four source-work field helper callers',group('literal array/foreach fixed',R=4,W=4,array_S=1,foreach_fixed=3),
    group('four caller rows',units=4*(5+1+7+1)))
row((941,941), 'four explicit Int64 work locals',group('Type/chained field/destination four times',units=4*(1+7+1+1)))
row((942,942), 'checked disjoint work overflow guard',group('first subtraction/comparison',R=2,MaxValue_type_select_value=3,O=2),
    group('second two subtractions/comparison',R=3,MaxValue_type_select_value=3,O=3),group('or/branch/refusal',or_O=1,B=1,throw_R=1,throw_K=1))
row((943,943), 'exact work composition guard',group('four values/two sums/comparison',R=4,O=3,B=1),group('throw',R=1,K=1))
row((944,944), 'reviewed limit assignment and reservation caller',group('assignment',R=1,W=1),group('two args/own dispatch',R=2,D=1))
row((945,945), 'exact input array kind/count guard',group('RuntimeType caller and Type comparison',index=4,D=1,result_operand_W=1,type_R=1,ne_O=1),
    group('count/profile comparison',index=4,Count_tail=5,profile_index=7,ne_O=1),group('or/branch/refusal',or_O=1,B=1,throw_R=1,throw_K=1))
row((946,946), 'checked output array profile',group('complete count/int bound expression',input_index=4,Count_tail=5,int_MaxValue=3,OwnNames_Count=6,one_R=1,two_sub_O=2,gt_O=1,B=1),group('throw',R=1,K=1))
row((949,949), 'four-field JSON profile map',group('map/keys/slots/local',S=1,key_R=4,field_W=4,local_W=1),
    group('actual array extent expression',input_Count=9,own_Count=6,one_R=1,two_add_O=2),group('other values',receipt_Count=6,two_literal_R=2))
row((950,950), 'initialize path catalogue caller',group('own dispatch',D=1))
row((951,951), 'predecessor descriptor caller/local',group('indexed value/dispatch/destination',index=4,D=1,W=1))
row((952,952), 'input dictionary and1220 source rows',group('dictionary comparer constructor and indexed foreach fixed',ctor=10,foreach_fixed=7),
    group('per actual input source row',units=1220*(5+6+20+9)),
    note='Per row: foreach5; Resolve path index4+D+localW6; full Length/ContainsKey/or/branch/refusal20; actual Dictionary.Add9. No Resolve body is included.')
row((953,953), 'closed input count guard',group('Count/profile/ne/branch/refusal',Count=6,profile_index=7,ne_O=1,B=1,throw_R=1,throw_K=1))
row((954,954), 'include dictionary copy1220 rows',group('constructor and explicit GetEnumerator foreach fixed',ctor=10,enumerator_result=6,foreach_fixed=3),
    group('per pair foreach/Add with two direct getter arguments',units=1220*(5+2+6+6+3)))
row((955,955), 'one current header include and four FilterScript callbacks',group('indexed current-include foreach fixed',units=7),
    group('row foreach and source-name guard',foreach=5,R=2,compare_O=1,B=1,throw_R=1,throw_K=1),
    group('source pipeline/literal callback/array capture/local',source_index=4,scriptblock_S=1,FilterScript_param_W=1,Where_D_E_T=3,capture_S=1,local_W=1),
    group('four source callbacks',units=4*(3+1+4+1+1+1)),
    group('exact one captured pin guard',Count=6,one_R=1,ne_O=1,B=1,throw_R=1,throw_K=1),
    group('include Add with JoinPath and indexed pin arguments',receiver_R_W=2,Join_args_and_boundary=8,first_param_W=1,pin_index=4,second_param_W=1,Add_D_E_T=3),
    note='Callback11=DET3+automatic inputW1+indexed4+NameR1+ceqO1+predicate destinationW1. Actual one current_include_input/four source rows must be checked against exact manifest.')
row((956,956), 'closed include count guard',group('Count/profile/ne/branch/refusal',Count=6,profile_index=7,ne_O=1,B=1,throw_R=1,throw_K=1))
row((957,957), 'authored byte count over own4',group('GetItem/Length/long Type local',cmdlet_args_and_boundary=6,produced_receiver_W=1,Length_D_E_T=3,local_W=1,long_type_R=1,explicit_cast_O=1),
    group('indexed foreach fixed',units=7),group('four add/cast source rows',units=4*(5+1+1+4+1+1+1)),
    group('1MiB guard/refusal',R=2,gt_O=1,B=1,throw_R=1,throw_K=1))
row((958,958), 'control and retained reservation source',group('zero local and indexed foreach fixed',R=1,W=1,foreach_fixed=7),
    group('four control additions',units=4*(5+1+1+4+1+1+1)),
    group('complete retained expression',long_Type_R=1,explicit_cast_O=1,profile_index=7,authored_R=1,three_literal_R=3,control_R=1,five_add_O=5,local_W=1),
    group('retained helper caller',variable_R=1,Int64_literal_R=1,D=1))
row((959,959), 'direct owned-root shape predicate',group('complete StartsWith/not source',units=25),group('complete Substring/Contains source',units=30),
    group('or/branch/refusal',or_O=1,B=1,throw_R=1,throw_K=1),
    note='Each JoinPath scalar Path has O1; static separator/comparison fields have TypeR/accessO/valueR; actual parameters/results are once. Detailed predicate port expansion is retained in the accompanying source argument.')
row((960,960), 'root ASCII helper caller',group('three actual arguments and own D',R=3,D=1))
row((961,961), 'NoLink and TestPath/Attempt callers',group('NoLink caller',R=1,D=1),
    group('TestPath into bool formal and remaining Attempt caller',path_R=1,path_param_W=1,scalar_array_O=1,TestPath_D_E_T=3,Attempt_D=1,root_R=1),
    note='TestPath result initializes the own Attempt bool formal W in its body; do not add another caller result write.')
row((962,962), 'sole run-attempt prefix',group('JoinPath local',units=9),group('TestPath operand',R=1,param_W=1,array_O=1,D_E_T=3,operand_W=1),
    group('GetEntries caller/default and captured Count',three_argument_R=3,own_D=1,Destination_default_null_R=1,capture_S=1,Count_receiver_W=1,Count_D_E_T=3,Count_operand_W=1),
    group('literal/comparison/and/branch/refusal',zero_R=1,gt_O=1,and_O=1,B=1,throw_R=1,throw_K=1))
row((963,963), 'Pins and authenticated predecessor callers',group('Pins own dispatch',D=1),
    group('Resolve/ReadJson produced local',path_index=4,Resolve_D=1,max_literal_R=1,Int32_Long_O=1,hash_index=4,ReadJson_D=1,local_W=1),
    group('Predecessor caller',R=1,D=1))
row((964,964), '56 reference caps dictionary rows',group('comparer constructor and indexed foreach fixed',ctor=10,foreach_fixed=7),group('each descriptor source row',units=56*(5+2+4+1+1+4+1+1+3)))
row((965,965), 'three compiler-output caps dictionary rows',group('comparer constructor and indexed foreach fixed',ctor=10,foreach_fixed=7),group('each descriptor source row',units=3*(5+2+4+1+1+4+1+1+3)))
row((966,966), 'new owned root and created flag',group('NewItem declared arguments/boundary/sink',ItemType_R=1,ItemType_W=1,Path_R=1,Path_W=1,array_O=1,D_E_T=3,sink_W=1),group('created Boolean assignment',R=1,W=1))
row((967,967), 'six derived directories',group('literal collection and foreach fixed',three_value_R_W=6,three_JoinPath_element_results=27,array_S=1,foreach_fixed=3),
    group('six iteration/NewItem source rows',units=6*(5+9)))
row((968,968), 'marker path and six-field value',group('JoinPath local',units=9),group('six-field literal map',S=1,key_R=6,value_R=6,field_W=6,local_W=1))
row((969,969), 'marker measure/admission/encode caller ports',group('Measure caller/default/local',R=2,Int32_Long_O=1,D=1,W=1),
    group('DocumentWrite caller',path_R=1,bytes_index=4,D=1),group('Encode caller/local',R=3,Int32_Long_O=1,D=1,W=1))
row((970,970), 'three marker owner/primary/body locals',group('values and destinations',R=3,W=3))
row((971,971), 'marker stream source/catch/finally callers',group('File.Open source',type_R=1,path_R=1,path_param_W=1,CreateNew_static_field=3,mode_param_W=1,D_E_T=3,local_W=1),
    group('Write body source',receiver_R_W=2,two_argument_R_W=4,Length_into_parameter=6,D_E_T=3),
    group('catch selected/input/primary/rethrow',catch_K=1,caught_input_W=1,Body_assignment=2,primary_guard=4,primary_assignment=2,rethrow_K=1),
    group('two finally own caller ports',Close_R_D=2,Complete_two_R_D=3),
    note='All close/Complete bodies stay outside this caller ledger. Try/finally runtime metadata/unwinding is not expanded.')
row((972,972), 'control list and source reservation caller',group('capacity10 constructor',type_R=1,literal_R=1,param_W=1,D_E_T=3,local_W=1),group('reservation caller',R=2,D=1))
row((973,973), 'two boundary grammar own callers with actual UTF8 sources',group('two sources',units=2*(9+5+5)),
    note='Per source19: encoding ctor/receiver9, GetBytes literal/param/DET5, three Int32 literal own args+own D+sink W5. Own raw grammar body/typed formal byte[] W stay separate.')

# Expect's helper body already owns Action_R+D at its & invocation. The
# following callback upper contains its E/T and actual body, never another D.
row((974,974), 'JSON overlength negative-control caller/action',group('List/Expect/map formation',Expect_Add_ports=10,two_field_map=7),group('callback E/T and actual UTF8/raw caller',E_T=2,action_source=19))
row((975,975), 'last admitted poll caller',group('literal subtraction argument and own dispatch',R=2,sub_O=1,D=1))
row((976,976), 'poll-limit negative-control caller/action',group('List/Expect/three-field map',Expect_Add_ports=10,three_field_map=12),group('callback E/T and own caller',E_T=2,Polls_R=1,D=1))
row((977,977), 'work-one-below negative-control caller/action',group('List/Expect/three-field map',Expect_Add_ports=10,three_field_map=12),group('callback E/T and complete two-argument caller',E_T=2,R=3,sub_O=1,D=1))
row((978,978), 'bytes-one-below negative-control caller/action',group('List/Expect/three-field map',Expect_Add_ports=10,three_field_map=12),group('callback E/T and complete two-argument caller',E_T=2,R=3,sub_O=1,D=1))
row((979,979), 'one-use negative-control caller/action',group('List/Expect/two-field map',Expect_Add_ports=10,two_field_map=7),group('callback E/T and TestPath/Attempt caller',E_T=2,action_source=8))
row((980,980), 'predecessor copy setup callers',group('Resolve path/local',index=4,D=1,W=1),group('initial hash caller/local',R=2,Int32_Long_O=1,D=1,W=1),group('mutant JoinPath local',units=9),
    group('Copy actual args',two_path_R=2,two_literal_R=2,two_Int32_Long_O=2,D=1))
row((981,981), 'predecessor identity map and Fresh caller',group('three-field named identity map',S=1,key_R=3,field_W=3,local_W=1,two_scalar_R=2,sha_index=4),
    group('Fresh caller and nested GetHash caller',three_indices=12,Hash_Path_R=1,Hash_literal_R=1,Int32_Long_O=1,Hash_D=1,Fresh_D=1))
row((982,982), 'predecessor read owner/primary/body locals',group('three values and destinations',R=3,W=3))
row((983,983), 'predecessor OpenRead/ReadByte/catch/finally callers',group('OpenRead local',type_R=1,path_R=1,param_W=1,D_E_T=3,local_W=1),group('ReadByte local',receiver_R_W=2,D_E_T=3,local_W=1),
    group('catch selected/input/primary/rethrow',catch_K=1,caught_input_W=1,Body_assignment=2,primary_guard=4,primary_assignment=2,rethrow_K=1),group('finally two own callers',Close_R_D=2,Complete_two_R_D=3))
row((984,984), 'owned first-byte mutation and corrupt hash callers',group('Mutate path/offset/caller/local',Path_R=1,offset_literal_R=1,offset_Int32_Long_O=1,D=1,W=1),
    group('typed one-element byte replacement array',outer_type_R=1,array_S=1,array_coercion_O=1,byte_type_R=1,First_R=1,one_R=1,bxor_O=1,Int32_Byte_O=1,element_W=1),
    group('corrupt hash caller/local',R=2,Int32_Long_O=1,D=1,W=1))
row((985,985), 'changed-predecessor control and provenance updates',group('List/Expect five-field witness formation',Expect_Add_ports=10,map_S=1,key_R=5,field_W=5,path_R=1,bytes_literal_R=1,Item1_without_dest=5,hash_index=4,HashHex_R_D=2),
    group('callback E/T and Fresh caller',E_T=2,three_indices=12,Corrupt_R=1,D=1),
    group('two last-control assignments',two_LHS=28,Mutation_R=1,InitialHashHex_R_D=2),
    note='Each last-control LHS14 includes both written taskControls reads, Count getter, subtraction, both selections and actual field destination. Getter/hash results fuse with map/field destinations.')
row((986,986), 'compiler-start receipt fields and save caller',group('three scalar indexed writes',units=3*5),group('ToArray produced indexed write',LHS=4,receiver_R_W=2,D_E_T=3),group('Save own dispatch',D=1))
row((987,987), 'compiler resolution and console-PE callers',group('Resolve nested compiler path/local',index=7,D=1,W=1),group('nested PE/console own callers',Path_R=1,PE_D=1,Console_D=1))
row((988,988), 'exact compiler argument count guard',group('chained Count/28/ne/branch/refusal',index=7,Count_tail=5,literal_R=1,ne_O=1,B=1,throw_R=1,throw_K=1))
row((989,989), 'capacity28 string argument list local',group('constructor source',type_R=1,literal_R=1,param_W=1,D_E_T=3,local_W=1))
row((990,990), '28 compiler argument validation/three replacements/Add callers',group('chained enumerable foreach fixed',units=10),
    group('28 source rows',units=28*(5+4+3*5+7)),
    note='Each row has foreach5, String caller4, three Replace caller/local5 and List.Add7. All String/Replace bodies and84 replacement loops are separately owned.')
row((991,991), 'compiler fenced call and raw exit guard',group('Fenced actual args/default/local',two_string_R=2,ToArray_without_destination=5,ExpectedBinary_default_null_R=1,D=1,local_W=1),
    group('raw exit comparison/refusal',index=4,zero_R=1,ne_O=1,B=1,throw_R=1,throw_K=1))
row((992,992), 'compiler build/dependency receipt fields',group('GetBuild/local',D=1,W=1),group('scalar indexed field write',units=5),
    group('Dependencies into indexed field',LHS=4,Build_R=1,D=1),group('admitted Boolean write',R=1,W=1))
row((993,993), 'native target path/PE/initial hash callers',group('JoinPath local',units=9),group('PE local',R=1,D=1,W=1),group('Console caller',R=1,D=1),
    group('hash local',R=2,Int32_Long_O=1,D=1,W=1))
row((994,994), 'GUI-subsystem mutant copy and identity/console callers',group('JoinPath local',units=9),
    group('Copy args',two_path_R=2,Item1_without_destination=5,literal_R=1,Int32_Long_O=1,D=1),
    group('Fresh args and nested hash',Path_R=1,two_getters_without_dest=10,Hash_R=2,Hash_Int32_Long_O=1,Hash_D=1,Fresh_D=1),
    group('nested PE/console callers',Path_R=1,PE_D=1,Console_D=1))
row((995,995), 'subsystem mutation and PE callers',group('Mutate path/offset/replacement/local',Path_R=1,offset_long_type_R=1,offset_index=4,offset_Int32_Long_O=1,two_offset_literal_R=2,two_add_O=2,
    BitConverter_type_R=1,uint16_type_R=1,two_R=1,Int32_UInt16_O=1,GetBytes_param_W=1,GetBytes_D_E_T=3,Mutate_D=1,local_W=1),group('PE local caller',R=1,D=1,W=1))
row((996,996), 'GUI refusal control and three provenance fields',group('List/Expect/four-field witness',Expect_Add_ports=10,map_S=1,key_R=4,field_W=4,three_indices=12,subsystem_literal_R=1),
    group('callback E/T and Console caller',E_T=2,Gui_R=1,D=1),
    group('three last-control provenance assignments',three_LHS=42,Mutation_R=1,InitialHashHex_R_D=2,ActualHashHex_nested_caller=5))
row((997,997), 'captured target hash caller/local',group('actual args/conversion/dispatch/destination',R=2,Int32_Long_O=1,D=1,W=1))
row((998,998), 'producer fenced-call arguments',group('name/target/hash/caller/local',R=3,D=1,W=1),group('six written argument elements',three_plain_R=3,three_plain_W=3,three_LongString_type_R=3,three_Long_R=3,three_LongString_O=3,three_element_W=3,array_S=1,objectarray_stringarray_O=1))
row((999,999), 'producer result caller and full ledger guard',group('result caller/local',R=1,D=1,W=1),group('four comparison operands',units=6+9+6+8),group('or/branch/refusal',three_or_O=3,B=1,throw_R=1,throw_K=1))
row((1000,1000), 'before-reference result and exact set callers',group('GetReferences/local',D=1,W=1),group('Observed/Expected/dispatch',Observed_index=4,Keys_getter=6,capture_S=1,objectarray_stringarray_O=1,D=1),
    note='GetReferences returns only after its own AssertFileSet proves56 unique names; pre-return60-entry failure work stays in its helper/family, not the later main loops.')
row((1001,1001), 'missing-reference capture and control',group('Where/capture/local prefix',index=4,scriptblock_S=1,FilterScript_param_W=1,Where_D_E_T=3,capture_S=1,local_W=1),
    group('56 authored filter callbacks',units=56*(3+1+1+1+1+1)),
    group('List/Expect/five-field witness',Expect_Add_ports=10,map_S=1,key_R=5,field_W=5,value_R=5),
    group('action E/T and FileSet actual caller',E_T=2,Observed_R=1,Observed_stringarray_O=1,Keys_getter=6,capture_S=1,Expected_stringarray_O=1,D=1))
row((1002,1002), 'extra-reference capture and control',group('two captures/concatenation/local',Before_names_index=4,first_capture_S=1,literal_R=1,element_W=1,second_array_S=1,concat_O=1,local_W=1),
    group('List/Expect/five-field witness',Expect_Add_ports=10,map_S=1,key_R=5,field_W=5,value_R=5),
    group('action E/T and FileSet actual caller',E_T=2,Observed_R=1,Observed_stringarray_O=1,Keys_getter=6,capture_S=1,Expected_stringarray_O=1,D=1))
row((1003,1003), 'duplicate-reference capture and control',group('two captures/concatenation/local',Before_names_index=4,first_capture_S=1,selected_scalar_index=7,selected_element_W=1,second_array_S=1,concat_O=1,local_W=1),
    group('List/Expect/five-field witness',Expect_Add_ports=10,map_S=1,key_R=5,field_W=5,four_plain_value_R=4,duplicate_value_index=7),
    group('action E/T and FileSet actual caller',E_T=2,Observed_R=1,Observed_stringarray_O=1,Keys_getter=6,capture_S=1,Expected_stringarray_O=1,D=1))
row((1004,1004), 'producer receipt/save callers',group('two scalar indexed fields',units=2*5),group('own save dispatch',D=1))
row((1005,1005), 'verifier prior local',group('two values/add/destination',R=2,O=1,W=1))
row((1006,1006), 'verifier fenced-call arguments',group('name/target/hash/caller/local',R=3,D=1,W=1),group('six written argument elements',three_plain_R=3,three_plain_W=3,three_LongString_type_R=3,three_Long_R=3,three_LongString_O=3,three_element_W=3,array_S=1,objectarray_stringarray_O=1))
row((1007,1007), 'verifier result caller and full ledger guard',group('result caller/local',R=1,D=1,W=1),group('five comparison operands',units=6+6+6+9+9),group('or/branch/refusal',four_or_O=4,B=1,throw_R=1,throw_K=1))
row((1008,1008), 'after-reference/same/build callers',group('two produced locals',D=2,W=2),group('SameReferences args/dispatch',R=2,D=1))
row((1009,1009), 'three build identity rows with property-mode Where',group('indexed foreach fixed',units=7),group('three source rows',units=3*(5+17+11+6)),
    note='Property-mode Where17 has source index4, property R/W2, CEQ switchW1, indexed value/param5, DET3, captureS1, localW1. It has no authored FilterScript callback or fictitious per-item runtime loop.')
row((1010,1010), 'four control-file artifact rows',group('list constructor and indexed foreach fixed',ctor=5,foreach_fixed=7),group('four source rows',units=4*(5+12+7+23+24)),
    note='Per row: foreach5, JoinPath local12, GetHash caller7, complete Contains/optional exact-length guard/refusal23, four-field map/Add24. Getter outputs fuse with map field writes.')
row((1011,1011), 'complete67 expected prebundle file names',group('string list constructor',units=5),group('reference Keys foreach',fixed=9,rows=56*14),group('build Keys foreach',fixed=9,rows=3*14),
    group('four control names',fixed=7,rows=4*15),group('four stage literal names',literal_array=9,foreach_fixed=3,rows=4*14))
row((1012,1012), 'at most78 current root entries',group('list/own GetEntries actual caller/foreach fixed',ctor=5,Root_R=1,true_R=1,int_type_R=1,profile_index=7,Int64_Int32_O=1,Entries_D=1,Destination_default_null_R=1,collection_result_W=1,foreach_fixed=3),
    group('complete per-entry caller source',units=78*(5+6+8+30)),
    note='Per entry: foreach5, Item1 into NoLink/dispatch6, Item3/not/branch8, complete GetRelativePath/Replace into List.Add30. GetEntries/NoLink bodies are excluded.78 bounds refusal prefixes too, not only normal67 files.')
row((1013,1013), 'observed/expected final set caller',group('two ToArray into own formals and dispatch',two_receiver_R_W=4,two_D_E_T=6,D=1))
row((1014,1014), 'final pins/primary guard/receipt fields/save callers',group('Pins D',D=1),group('primary null guard/rethrow',R=2,ne_O=1,B=1,throw_R=1,throw_K=1),
    group('three scalar indexed fields and one ToArray field',three_scalar_fields=15,ToArray_field=9),group('Save D',D=1))
row((1015,1015), '63 successful reference/build/control artifact rows',group('list/three captures/two concatenations/foreach fixed',ctor=5,two_indexed_captures=10,ToArray_result_capture=7,two_concat_O=2,foreach_fixed=3),
    group('63 foreach/Add rows',units=63*(5+7)))
row((1016,1016), 'four stage artifact rows',group('literal array and foreach fixed',literal_array=9,foreach_fixed=3),group('four source rows',units=4*(5+9+5+24)))
row((1017,1017), 'admitted bytes over67 artifacts',group('initial explicit Long expression',long_type_R=1,explicit_cast_O=1,profile_index=7,Authored_R=1,add_O=1,local_W=1),group('named foreach fixed',units=4),
    group('67 source rows',units=67*(5+1+1+4+1+1+1)))
row((1018,1018), '1225 transitive source rows',group('list constructor',units=5),group('1220 input rows',foreach_fixed=7,rows=1220*(5+2+23+4)),group('four own rows',foreach_fixed=7,rows=4*(5+2+29+4)),
    group('manifest row',receiver_R_W=2,map_S=1,key_R=4,field_W=4,Path_R=1,GetItem_Length_without_mapdest=10,two_sha_origin_R=2,Map_param_W=1,Add_D_E_T=3),
    note='Input map values are Resolve caller5, bytes index4, hash index4, originR1. Own map substitutes JoinPath caller11 for Resolve5. Manifest GetItem scalar Path array coercion remains counted.')
row((1019,1019), '21-field ordered bundle value',group('map/key/slot/local',S=1,key_R=21,field_W=21,local_W=1),group('nineteen scalar values and two ToArray field sources',scalar_R=19,two_receiver_R_W=4,two_D_E_T=6))
row((1020,1020), 'fixed-point stable/measure locals',group('two values and destinations',R=2,W=2))
row((1021,1025), 'at most five fixed-point passes',group('literal bound for framework',units=8+9*5),group('five complete positive pass prefixes',units=5*(5+22+8+10)),
    group('at most one reached true assignment/break',Boolean_R=1,W=1,break_K=1),
    note='Pass source45=Measure caller/default/local5+full condition22+two updates8/10. A reached break may omit later updates; this positive sum is an upper, not a trace. Measure bodies and encodes remain outside.')
row((1026,1026), 'fixed-point success guard',group('not/branch/refusal',R=1,not_O=1,B=1,throw_R=1,throw_K=1))
row((1027,1027), 'final retained helper caller',group('indexed amount/Int64 limit/own dispatch',index=4,literal_R=1,D=1))
row((1028,1028), 'bundle write caller',group('JoinPath into own formal',units=8),group('remaining WriteJson args/dispatch',Bundle_R=1,max_literal_R=1,Int32_Long_O=1,Measurement_R=1,D=1))
row((1029,1029), 'controlled final output caller',group('nested JoinPath/Hash/HashHex result operand',JoinPath_into_formal=8,max_literal_R=1,Int32_Long_O=1,Hash_D=1,HashHex_D=1,operand_W=1),
    group('prefix/concat/public WriteOutput',prefix_R=1,concat_O=1,PSObject_coercion_O=1,param_W=1,D_E_T=3),
    note='Selected Write-Object.cs21 declares InputObject PSObject (not PSObject[]). A source String result requires the semantic String-to-PSObject wrapper; its hidden implementation CPU is not expanded.')
row((1030,1031), 'outer caught-input and primary',group('selected catch/input',catch_K=1,caught_input_W=1),group('null guard/first assignment',guard=4,assignment=2))
row((1032,1032), 'four failure receipt fields',group('four scalar indexed writes',units=4*5))
row((1033,1033), 'created-prefix condition',group('source value/branch',R=1,B=1))
row((1034,1034), 'failure inventory caller and failed-inventory secondary',group('SaveFailure D',D=1),group('selected catch/automatic input',catch_K=1,caught_input_W=1),group('SetSecondary actual args/dispatch',R=2,D=1))
row((1035,1035), 'failure receipt caller/secondary/public WriteError',group('SaveReceipt D',D=1),group('selected catch/automatic input',catch_K=1,caught_input_W=1),group('SetSecondary args/dispatch',R=2,D=1),
    group('public WriteError actual parameters/boundary',message_R=1,message_param_W=1,Continue_R=1,string_enum_O=1,ErrorAction_param_W=1,D_E_T=3))
row((1036,1038), 'outer failure rethrow and structural closes',group('raw primary/rethrow',R=1,K=1),group('normal script exit alternative',T=1))

# The separate own checker freezes this complete proposed data at its first
# passing source/file/integer check. It supplies no independent acceptance.
