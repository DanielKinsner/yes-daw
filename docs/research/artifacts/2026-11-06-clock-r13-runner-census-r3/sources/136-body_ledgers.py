"""New R2 source-role ledgers. Own integer arithmetic; no subject import/runtime.

This is an unfinished authorship worksheet until all74 bodies, main/callers and
the six composition schedules pass the complete builder. No price is accepted.
Every number is a source action under344408/DE33/268597, not a CPU/byte tariff.
"""

MODELS={}

def group(label, span, **terms):
    return dict(label=label,source_lines=list(span),terms=terms,units=sum(terms.values()))

def vector(label, span, **roles):
    return group(label,span,**roles)

def model(name, formula, groups, *, coefficients=None, alternatives=None, excluded=None, note=''):
    assert name not in MODELS
    MODELS[name]=dict(formula=formula,fixed_groups=groups,fixed=sum(g['units'] for g in groups),
                      coefficients=coefficients or {},alternatives=alternatives or {},
                      separately_composed=excluded or [],note=note,
                      source_count_status='NEW_R2_UNACCEPTED_SOURCE_PROPOSAL')

# Literal and named reads are counted. Primitive callers initialize parameters
# once; direct result destinations fuse with the caller's real slot. The role
# calculators below denote actual source forms, not a new primitive category.
INDEX=group('one indexed source cell',(1,1),base_R=1,key_R=1,index_O=1,selected_R=1)['units']
CHAINED_INDEX=INDEX+3
GETTER=group('named instance getter without destination',(1,1),receiver_R=1,receiver_W=1,D=1,E=1,T=1)['units']
GETTER_LOCAL=GETTER+1
STATIC_GETTER_ARGUMENT=group('static getter direct argument',(1,1),type_R=1,D=1,E=1,T=1,parameter_W=1)['units']
CTOR_RESULT=group('zero-argument constructor result',(1,1),type_R=1,D=1,E=1,T=1,result_W=1)['units']
FOREACH_FIXED_NAMED=group('foreach named enumerable fixed',(1,1),enumerable_R=1,S=1,terminal_K=1,exit_K=1)['units']
FOREACH_FIXED_INDEXED=INDEX+3
FOREACH_BODY=group('foreach body/advance/condition',(1,1),element_R=1,element_W=1,condition_K=1,body_K=1,advance_K=1)['units']

model('Spend-Task','17+C_long',[
 vector('complete normal source',(62,62),R=7,W=2,O=5,B=1,E=1,T=1)],
 alternatives=dict(negative=dict(R=3,W=1,O=2,B=1,E=1,K=1),remaining=dict(R=6,W=1,O=4,B=1,E=1,K=1)),
 note='C_long=1 only actual nonidentity Int32->Int64 caller argument; parameter W already here. Source Spend debit is not this operation price.')
model('Test-TaskPathCatalogueContext','26',[
 group('entry and source return',(63,65),E=1,T=1),
 group('null test',(64,64),null_R=1,catalogue_R=1,ne_O=1),
 group('three context equalities',(64,64),three_indexed_cells=3*INDEX,three_context_R=3,three_ceq_O=3),
 group('three written conjunctions',(64,64),and_O=3)],
 note='Short-circuit prefixes cannot exceed the complete true context path.')

add_common=[
 group('entry parameters',(66,66),E=1,param_W=2),
 group('catalogue local',(67,67),R=1,W=1),
 group('derived row count guard',(68,68),rows_index=INDEX,Count_receiver_W_D_E_T_result=5,maximum_index=INDEX,ge_O=1,B=1),
 group('five-field entry literal',(69,69),S=1,key_R=5,value_R=5,field_W=5,local_W=1),
 group('rows.Add',(70,70),rows_index=INDEX,receiver_W=1,Entry_R=1,param_W=1,D_E_T=3),
 group('RepoInput branch',(71,71),R=1,B=1),
]
add_true=[group('inputs.Add',(71,71),inputs_index=INDEX,receiver_W=1,two_argument_R=2,two_param_W=2,D_E_T=3),group('normal return',(80,80),T=1)]
drive=group('ordinary drive alias branch',(75,75),Name_Length=GETTER_LOCAL,literal3_R=1,ge_O=1,Name_index=INDEX,char_type_R=1,literal58_R=1,cast_O=1,ceq_O=1,and_O=1,B=1)
add_false=[
 group('aliases.Add',(72,72),aliases_index=INDEX,receiver_W=1,two_argument_R=2,two_param_W=2,D_E_T=3),
 drive,
 group('replace separator fields',(76,76),Name_receiver_R_W=2,two_static_char_fields=6,two_param_W=2,D_E_T=3,local_W=1),
 group('alias ContainsKey and not/branch',(77,77),aliases_index=INDEX,receiver_W=1,Slashes_R=1,param_W=1,D_E_T=3,operand_W=1,not_O=1,B=1),
 group('separator alias Add',(77,77),aliases_index=INDEX,receiver_W=1,two_argument_R=2,two_param_W=2,D_E_T=3),
 group('normal return',(80,80),T=1)]
model('Add-TaskPathCatalogueEntry','true62 / false119',add_common,
 alternatives=dict(repo_input=add_true,ordinary_alias=add_false),
 note='The type read in [char]58 is actual. UNC/device/shorter-name alternatives are prefixes; no filesystem fact is cached.')

init_max=group('derived maximum expression',(84,84),inputs_index=INDEX,input_Count_tail=5,long_type_R=1,cast_O=1,OwnNames_Count=GETTER_LOCAL,literal1_R=1,two_profile_cells=2*CHAINED_INDEX,four_add_O=4,local_W=1)
init_guard=group('checked profile bounds',(85,85),maximum_reads=4,profile_index=CHAINED_INDEX,integer_type_member_reads=6,literal_reads=2,comparisons=4,multiply=1,ors=3,B=1)
init_map=group('catalogue ten-field literal',(86,86),S=1,key_R=10,field_W=10,local_W=1,
               repo_base_root_R=3,maximum_type_R=1,maximum_R=1,maximum_cast_O=1,
               alias_expr_literal_R=2,alias_expr_Count_cells=2*(INDEX+5),alias_expr_maximum_R=1,
               alias_expr_arithmetic=4,
               rows_List_type_R=1,rows_int_type_R=1,rows_max_R=1,rows_cast_O=1,rows_param_W=1,rows_D_E_T=3,
               four_dictionary_ctor_types_boundaries=4*4,four_static_getter_arguments=4*STATIC_GETTER_ARGUMENT)
model('Initialize-TaskPathCatalogue','fixed+73I+own4*134+119+137*(56+3+4)+134*6',[
 group('entry',(81,81),E=1),init_max,init_guard,init_map,
 group('inputs foreach fixed',(87,87),indexed_expression_and_control=FOREACH_FIXED_INDEXED),
 group('own foreach fixed',(88,88),named_expression_and_control=FOREACH_FIXED_NAMED),
 group('manifest Add caller',(89,89),path_R=1,false_R=1,D=1),
 group('four indexed foreach fixed',(90,93),source_and_control=4*FOREACH_FIXED_INDEXED),
 group('final derived recipe guard',(94,94),rows_index=INDEX,Count_tail=5,Maximum_R=1,ne_O=1,B=1),
 group('normal return',(95,95),T=1)],
 coefficients=dict(input_row=dict(foreach=5,path_index=4,true_R=1,D=1,Add_true=62),
                   own_or_stage_row=dict(foreach=5,Join_type=1,two_arg_R=2,two_arg_W=2,Join_D_E_T=3,false_R=1,Add_D=1,Add_false=119),
                   descriptor_row=dict(foreach=5,Join_type=1,root_R=1,path_index=4,two_arg_W=2,Join_D_E_T=3,false_R=1,Add_D=1,Add_false=119)),
 note='All13static comparer sites are reconciled individually; four occur in this literal. Dictionary result destinations fuse with the four map field writes. No metadata/trust caching.')

model('Get-TaskCatalogueString','hit21 / miss41',[
 group('entry/parameter',(96,96),E=1,W=1),
 group('strings local',(97,97),indexed=INDEX,local_W=1),
 group('ContainsKey branch',(98,98),receiver_R_W=2,Path_R=1,param_W=1,D_E_T=3,operand_W=1,B=1)],
 alternatives=dict(hit=[group('return indexed reference',(98,98),index=INDEX,T=1)],
                   miss=[group('bounded count guard',(99,99),Count=GETTER_LOCAL,literal65_R=1,maximum_index=INDEX,multiply_O=1,ge_O=1,B=1),
                         group('Add same reference twice',(100,100),receiver_R_W=2,two_Path_R=2,two_param_W=2,D_E_T=3),
                         group('return Path',(100,100),R=1,T=1)]),
 note='No referent cell copy; ContainsKey primitive body opaque. Full miss must use actual chained maximum index; source equations are regenerated rather than miss41 inherited.')

model('Get-TaskHashHex','15',[
 group('entry and parameter',(157,157),E=1,W=1),
 group('implicit Item2 getter',(157,157),receiver_R_W=2,D_E_T=3),
 group('typed direct argument',(157,157),byte_array_type_R=1,conversion_O=1,fused_parameter_W=1),
 group('Convert boundary and function return',(157,157),Convert_type_R=1,D_E_T=3,T=1)])
model('Test-TaskHashValue','hex1509 / binary647',[
 group('entry/parameters',(158,158),E=1,W=2),
 group('digest32 guard',(159,159),Length=GETTER_LOCAL,literal32_R=1,ne_O=1,B=1)],
 alternatives=dict(hex=[group('string kind',(160,160),Expected_R=1,string_type_R=1,is_O=1,B=1),
                         group('expected64 guard',(161,161),Length=GETTER_LOCAL,literal64_R=1,ne_O=1,B=1),
                         group('alphabet literal',(162,162),R=1,W=1),
                         group('32-loop fixed and return',(163,168),for_fixed=8,true_R=1,T=1)],
                   binary=[group('both kind tests',(160,169),two_Expected_R=2,two_type_R=2,two_is_O=2,two_B=2),
                           group('expected32 guard',(170,170),Length=GETTER_LOCAL,literal32_R=1,ne_O=1,B=1),
                           group('32-loop fixed and return',(171,172),for_fixed=8,true_R=1,T=1)]),
 coefficients=dict(hex_body=dict(loop=9,Digest_index=INDEX,int_type_R=1,cast_O=1,local_W=1,
                                 expected_high=6,alphabet_high=6,high_compare=1,or_O=1,expected_low=8,alphabet_low=6,low_compare=1,B=1),
                   binary_body=dict(loop=9,Digest_index=INDEX,Expected_index=INDEX,compare=1,B=1)),
 note='Exactly32iterations. Early mismatch/wrong kind/extent paths are bounded by their own source prefixes; no fabricated throwing normalreturn.')
model('Assert-TaskFreshHashIdentity','normal27+Test; refusal group85 replaces return1',[
 group('entry/parameters',(176,176),E=1,W=4),
 group('byte-count comparison',(177,177),Expected_R=1,Observed_Item1=GETTER_LOCAL,ne_O=1),
 group('written Boolean operators',(177,177),or_O=1,not_O=1,B=1),
 group('Test caller',(177,177),Item2_getter=GETTER,byte_array_type_R=1,cast_O=1,ExpectedHash_R=1,D=1,bool_operand_W=1),
 group('normal return',(182,182),T=1)],
 alternatives=dict(refusal=[group('expected hash projection',(178,178),kind_predicate=4,Convert_type_R=1,Expected_R=1,byte_array_type_R=1,cast_O=1,param_W=1,D_E_T=3,local_W=1),
                           group('observed hash projection',(179,179),Observed_R=1,D=1,local_W=1,GetHashHex=15),
                           group('five-field witness',(180,180),S=1,key_R=5,field_W=5,four_plain_value_R=4,Item1_getter=GETTER),
                           group('Reject caller/body',(180,180),reason_R=1,D=1,Reject=32)]),
 excluded=['Test-TaskHashValue'],note='Use four joined refusal opportunities, not3. Refusal witness actual getter result fuses with its map slot.')

model('Assert-TaskPollAdmission','admit11; reject56',[
 vector('normal checked domain',(183,185),R=4,W=1,O=3,B=1,E=1,T=1)],
 alternatives=dict(refusal=dict(entry_parameters=2,complete_guard=8,three_field_witness=12,reason_R=1,D=1,Reject=32)),
 note='Normal count0..7199; actual7200refusal control and possible later boundary refusal have their own witness branches.')
model('Reject-TaskGate','32',[
 vector('exception constructor, two Data writes, throw',(294,294),R=9,W=10,O=2,D=3,E=4,T=3,K=1)],
 note='Caller literal/read/witness construction separate. No CLR Exception object CPU/field expansion.')
model('Add-TaskJsonVisit','17+C_long',[
 vector('finite whole visit cap update',(302,302),R=7,W=2,O=5,B=1,E=1,T=1)],
 note='Same written scalar form as Spend; default/conversion caller once, no bulk byte CPU tariff.')
model('Get-TaskRuntimeType','actual16/null8',[
 group('entry/parameter',(303,303),E=1,W=1),
 group('null guard',(303,303),null_R=1,Value_R=1,eq_O=1,B=1),
 group('Invoke actual receiver/arguments/return',(303,303),method_receiver_R=1,Value_R=1,null_R=1,receiver_W=1,parameter_W=2,D_E_T=3,function_T=1)],
 alternatives=dict(null=dict(E=1,W=1,null_R_twice=2,Value_R=1,eq_O=1,B=1,T=1)),
 note='Actual Type result destination is supplied once by each source caller, not by this helper return.')
model('Assert-TaskString','49+64N+D; ASCII49+53N+D',[
 group('entry/parameters',(357,357),E=1,W=3),
 group('RuntimeType operator caller and body',(358,358),Value_R=1,D=1,result_W=1,body=16),
 group('kind/or',(358,358),string_type_R=1,ne_O=1,or_O=1),
 group('length guard',(358,358),Length=GETTER_LOCAL,Maximum_R=1,gt_O=1,B=1),
 group('for fixed and normal return',(359,365),for_fixed=13,T=1)],
 coefficients=dict(ASCII_body=dict(framework=14,int_local=7,ASCII_guard=10,high_surrogate_call_guard=11,low_surrogate_call_guard=11),
                   general_surrogate_body=dict(framework=14,int_local=7,ASCII_false_guard=3,high_or_low_outer=11,inner_range_and_matching_call=29)),
 note='Explicit default literal is charged only at omitted actual Maximum/Ascii sites. Named getter results remain once.64N is a source upper over actual general UTF16 branches.')
model('Assert-TaskIntegral','61 upper',[
 group('entry/parameters',(372,372),E=1,W=2),
 group('RuntimeType local',(373,373),Value_R=1,D=1,local_W=1,body=16),
 group('eight kind tests',(374,374),type_and_T_reads=16,ne_O=8,and_O=7,B=1),
 group('nonnegative branch',(375,375),Nonnegative_R=1,Value_R=1,zero_R=1,lt_O=1,and_O=1,B=1),
 group('normal return',(376,376),T=1)],
 note='Selected Int64/default-false56 and UInt64/default-false59 include their actual short-circuit type prefix plus omitted default literal; do not mechanically use61 at every caller.')
model('Get-TaskIntegerDigits','negative309 / unsigned307',[
 group('negative fixed source',(416,427),entry_parameters=2,Integral_caller_R_D=2,digits_literal_and_W=2,
       negative_test=4,initial_increment=3,foreach_named_fixed=4,return_R_T=2,Integral_Int64_default_false=56)],
 coefficients=dict(negative_threshold=dict(bodies=18,each=13),unsigned_threshold=dict(bodies=19,each=12)),
 alternatives=dict(unsigned_fixed=dict(entry_parameters=2,Integral_caller_R_D=2,digits_init=2,unsigned_test=4,foreach_named_fixed=4,return_R_T=2,Integral_UInt64_default_false=59,extra_nonnegative_selection=4)),
 note='Decimal thresholds are fixed representation literals, not prices. Actual full-negative18 and unsigned19 comparisons; no division/floating/stringification.')
model('Get-TaskEscapedStringBytes','75+148N',[
 group('entry and parameter',(428,428),E=1,W=1),
 group('String validation fixed/default/caller',(429,429),Value_R=1,profile_cell=INDEX,D=1,Ascii_default_R=1,String_fixed=49),
 group('bytes initialization',(430,430),literal_R=1,W=1),
 group('for fixed and return',(431,438),for_fixed=13,return_R_T=2)],
 coefficients=dict(UTF16=dict(String_validation=64,for_framework=14,Spend1_with_conversion=20,int_local=7,all_false_short_escape_guard=28,nonASCII_or_control_guard=8,add6=4,stringUnits_increment=3)),
 note='All70body actions are source owned. Short escapes and safeASCII are bounded alternatives, no external encoding CPU fee.')
model('Add-TaskJsonBound','27+C_long',[
 vector('typed long state indexed check/update',(439,439),R=13,W=3,O=8,B=1,E=1,T=1)],
 note='State[0]/[2] physical Int64 cells; Bytes argument has one actual nonidentity conversion only where needed; no general VM boxing tariff.')

visitor_common=[group('entry/parameters',(440,440),E=1,W=3),
                group('depth guard',(441,441),Depth_R=1,profile_index=INDEX,gt_O=1,B=1),
                group('finite visits',(444,444),two_state_indices=2*INDEX,ge_O=1,B=1),
                group('state/node counters and Spend',(445,445),State_increment=6,global_increment=3,Spend_caller_and_body=20),
                group('null branch predicate',(446,446),null_R=1,Value_R=1,eq_O=1,B=1)]
visitor_types=group('nonnull runtime type local',(447,447),Value_R=1,D=1,local_W=1,RuntimeType=16)
model('Visit-TaskJsonValue','null86; string184+148N; bool115; integer455; map185+223K+31sep+212keyUnits+children; array199+20E+31sep+children',visitor_common,
 alternatives=dict(nonnull_type=visitor_types,
                   null=dict(State_R=1,literal4_R=1,Int32_Long_O=1,D=1,AddBound=27,T=1),
                   string=dict(prefix=77,State_R=1,Value_R=1,GetEscaped_D=1,Add_D=1,Escaped_fixed=75,AddBound=27,T=1),
                   boolean=dict(prefix=81,State_R=1,Value_R=1,B=1,selected_literal_R=1,Int32_Long_O=1,Add_D=1,AddBound=27,T=1),
                   integer=dict(prefix=113,State_R=1,Value_R=1,Digits_D=1,Add_D=1,Digits_max=309,Int32_Long_O=1,AddBound=27,T=1),
                   map_fixed=dict(prefix=121,extent=12,AddBound=31,Seen_init=2,foreach_fixed=9,unchanged_count_guard=9,T=1),
                   array_fixed=dict(prefix=137,Count_local=6,extent=7,AddBound=31,for_fixed=8,unchanged_count_guard=9,T=1)),
 coefficients=dict(map_entry=dict(foreach=5,Seen_increment=3,Count_guard=9,finite_node_guard=10,State_increment=6,global_node_increment=3,key_String_fixed_caller=60,key_Escaped_Add_fixed=113,separator_guard=4,recursive_caller=10),
                   map_key_UTF16=dict(String=64,Escape=148),map_separator=dict(AddBound_with_literal=31),
                   array_entry=dict(for_framework=9,separator_condition=2,Value_index=INDEX,Depth_R=1,literal1_R=1,add_O=1,State_R=1,D=1),
                   array_separator=dict(AddBound_with_literal=31)),
 excluded=['recursive Visit-TaskJsonValue children'],
 note='Array fixed199 includes final unchanged Count guard9; old190 was not an authority. Integer455 uses independent maxima guard32 and digit309 as explicit upper, not a purported joint path.')

model('Get-TaskCatalogueEntryPath','hit14/tilde71/first9529',[
 group('entry and canonical guard',(102,103),E=1,parameter_W=1,null_R=1,canonical_index=INDEX,ne_O=1,B=1)],
 alternatives=dict(hit=[group('return canonical cell',(103,103),index=INDEX,T=1)],
                   tilde=[group('name/raw locals',(104,104),name_index=INDEX,name_W=1,name_R=1,Raw_W=1),
                          group('relative repository normalization guard',(105,105),repo_input_index=INDEX,Path_type_R=1,Name_R=1,param_W=1,D_E_T=3,bool_operand_W=1,not_O=1,and_O=1,B=1),
                          group('Join and local',(105,105),Repo_R=1,Name_R=1,two_param_W=2,D_E_T=3,Raw_W=1),
                          group('raw tilde guard',(108,108),raw_receiver_R_W=2,char_type_R=1,literal126_R=1,cast_O=1,param_W=1,D_E_T=3,operand_W=1,zero_R=1,ge_O=1),
                          group('GetFullPath result',(109,109),Path_type_R=1,Raw_R=1,param_W=1,D_E_T=3,local_W=1),
                          group('canonical tilde guard',(110,110),canonical_receiver_R_W=2,char_type_R=1,literal126_R=1,cast_O=1,param_W=1,D_E_T=3,operand_W=1,zero_R=1,ge_O=1),
                          group('return produced canonical',(110,110),R=1,T=1)],
                   first=[group('name/raw locals',(104,104),name_index=INDEX,name_W=1,name_R=1,Raw_W=1),
                          group('repo guard and optional Join',(105,105),repo_guard=14,Join=8),
                          group('two tilde guards and normalization',(108,110),raw_tilde=12,FullPath=7,canonical_tilde=12),
                          group('canonical String miss/caller',(111,111),Canonical_R=1,D=1,local_W=1,String_miss=41),
                          group('parent array and locals',(112,112),string_array_type_R=1,literal65_R=1,param_W=1,D_E_T=3,array_W=1,count_literal_R_W=2,canonical_R=1,At_W=1),
                          group('parent while fixed',(113,122),initial_or_terminal_and_setup=8),
                          group('65 actual admitted parent upper',(113,122),rows=65*143),
                          group('aliases local',(123,123),index=INDEX,W=1),
                          group('two-value-array foreach fixed',(124,124),array_S=1,two_value_R=2,two_slot_W=2,foreach_S=1,terminal_K=1,exit_K=1),
                          group('two alias source rows',(124,129),rows=2*36),
                          group('three catalogue field writes',(130,130),three_bases_R=3,three_keys_R=3,three_values_R=3,three_index_O=3,three_field_W=3),
                          group('return canonical',(131,131),R=1,T=1)]),
 coefficients=dict(parent_row=dict(while_condition_control=8,String_miss_and_caller=44,indexed_store=5,Count_increment=3,depth_branch=4,parent_contains=12,miss_GetDirectoryName=7,nonnull_branch=2,parent_String_miss_and_caller=44,parent_Add=12,At_write=2),
                   alias_row=dict(foreach=5,ContainsKey_not_branch=10,derived_count_guard=12,Add=9)),
 note='First-use1297 upper remains a conditional profile count, never a trace. Parent loop stop at65 precedes the next filesystem operation. Explicit array11 plus foreach8 replace unsupported12/7, leaving the complete positive first upper9529.')
model('Get-TaskCanonicalPath','76+EntryPath when selected',[
 group('entry/parameter',(133,133),E=1,W=1),
 group('context caller/result and body',(134,134),D=1,result_W=1,Context=26),
 group('Path tilde comparison',(134,134),receiver_R_W=2,char_type_R=1,literal126_R=1,cast_O=1,param_W=1,D_E_T=3,result_W=1,zero_R=1,lt_O=1),
 group('and and source branch',(134,134),and_O=1,B=1),
 group('aliases ContainsKey',(135,135),index=INDEX,receiver_W=1,Path_R=1,param_W=1,D_E_T=3,result_W=1,B=1),
 group('strings ContainsKey',(136,136),index=INDEX,receiver_W=1,Path_R=1,param_W=1,D_E_T=3,result_W=1,B=1),
 group('maximal selected return/caller',(135,139),indexed=CHAINED_INDEX,T=1)],
 excluded=['Get-TaskCatalogueEntryPath'],note='Unknown/context-changed/tilde alternatives retain GetFullPath; branch upper76 bounds actual selected return caller, not a cached filesystem result.')
model('Assert-NoLink','max(normal7061,depth65refusal7091)+Canonical',[
 group('entry/parameter',(141,141),E=1,W=1),
 group('length guard',(142,142),Length=GETTER_LOCAL,literal512_R=1,gt_O=1,B=1),
 group('Canonical caller/local and Depth initialization',(143,143),Path_R=1,D=1,At_W=1,zero_R=1,Depth_W=1),
 group('while fixed and implicit return',(144,145),terminal_At_R=1,terminal_K=1,loop_setup_S=1,exit_K=1,T=1)],
 coefficients=dict(parent_body=dict(while_At_R=1,condition_body_advance_K=3,Spend_literal_R=1,D=1,Int32_Long_O=1,Spend=17,
                                   depth_increment=3,depth_guard=4,File_Exists=7,Directory_Exists=7,exists_or_branch=2,
                                   GetAttributes=7,reparse_enum=3,band_O=1,zero_R=1,ne_O=1,B=1,
                                   Context_caller_result_body=28,parent_contains=11,and_branch=2,parent_selected_reference_write=8)),
 alternatives=dict(depth65_prefix=dict(while_condition_and_control=4,Spend=20,increment=3,guard=4,literal_R=1,throw_K=1),
                   replaced_normal_ending=dict(final_At_R=1,final_condition_K=1,function_T=1)),
 excluded=['Get-TaskCanonicalPath'],
 note='Per-body110. At most64 admitted filesystem parent bodies;65th33-action prefix refuses before filesystem checks, replacing3 normal terminal/return actions:21+64*110+33-3=7091. No FS-trust cache.')
model('Resolve-TaskInput','64NameN+135+EntryPath when selected',[
 group('entry/parameter',(803,803),E=1,W=1),
 group('String validation fixed/caller/default',(804,804),Name_R=1,literal512_R=1,D=1,defaultAscii_R=1,String_fixed=49),
 group('IsPathRooted and branch',(805,805),Path_type_R=1,Name_R=1,param_W=1,D_E_T=3,operand_W=1,B=1),
 group('derived relative path guard',(810,810),Repo_Length=GETTER_LOCAL,literal1_R=1,Name_Length=GETTER_LOCAL,two_add_O=2,literal512_R=1,gt_O=1,B=1),
 group('context ContainsKey branch',(811,811),Context_D_result=2,Context=26,inputs_index=INDEX,receiver_W=1,Name_R=1,param_W=1,D_E_T=3,result_W=1,and_O=1,B=1),
 group('selected caller/return upper',(806,812),fallback_type_R=1,Join_Repo_Name_with_fused_GetFullPath_param_W=8,D_E_T=3,T=1)],
 coefficients=dict(Name_UTF16=dict(String_general=64)),excluded=['Get-TaskCatalogueEntryPath'],
 note='Relative rooted and fully-qualified branches are re-expanded; source upper may sum positive branch envelopes but is not an executed trace. No copied referent bytes.')

model('Get-TaskSafePrefix','ASCII51/general54',[
 group('entry/parameters',(366,366),E=1,W=2),
 group('maximum domain',(367,367),two_Maximum_R=2,two_literal_R=2,two_compare_O=2,or_O=1,B=1),
 group('Math.Min local',(368,368),Math_type_R=1,Maximum_R=1,Length_getter=GETTER,two_param_W=2,D_E_T=3,local_W=1),
 group('surrogate boundary guard',(369,369),N_R=1,zero_R=1,gt_O=1,and_O=1,char_type_R=1,indexed_Value_Nminus1=6,param_W=1,D_E_T=3,operand_W=1,B=1),
 group('Substring return',(370,370),Value_receiver_R_W=2,zero_R=1,N_R=1,two_param_W=2,D_E_T=3,function_T=1)],
 alternatives=dict(general_extra_decrement=dict(R=1,O=1,W=1)),note='Sole current caller root ASCII proves decrement unreachable; generic form retained separately.')
model('Assert-TaskExactKeys','72+81K+64keyUnits',[
 group('entry/parameters',(389,389),E=1,W=2),
 group('RuntimeType local',(390,390),Map_R=1,D=1,local_W=1,RuntimeType=16),
 group('three kinds',(391,391),three_T_R=3,three_type_R=3,three_ne_O=3,two_and_O=2,B=1),
 group('actual count/length',(392,392),Map_Count=GETTER_LOCAL,Keys_Length=GETTER_LOCAL,ne_O=1,B=1),
 group('HashSet with static comparer',(393,393),static_getter_argument=STATIC_GETTER_ARGUMENT,constructor=CTOR_RESULT),
 group('two foreach fixed',(394,395),Keys_fixed=FOREACH_FIXED_NAMED,Map_Keys_getter_control=9),
 group('normal return',(396,396),T=1)],
 coefficients=dict(declared_key=dict(foreach=5,Add_receiver_R_W=2,key_R=1,param_W=1,D_E_T=3,null_result_W=1),
                   observed_key=dict(foreach=5,String_fixed_default_caller=53,ContainsKey_not_branch=10),key_UTF16=dict(String_general=64)),
 note='Static property/getter and constructor10, not9. Exact builtin map types only; no reflection/binder body expansion.')
model('Set-TaskSecondary','44 maximal first-unsaturated',[
 vector('all three actual guards/conditional writes',(397,401),R=19,W=6,O=11,B=3,D=1,E=2,T=2)],
 note='Slot length and Int64.MaxValue static field retained. No raw exception serialization or fictitious per-exception clone.')
model('Close-TaskOwner','null7/normal12/firstFailure17/secondaryFailure62',[
 group('entry/parameter and nonnull branch',(402,404),E=1,W=1,null_R=1,Owner_R=1,ne_O=1,B=1)],
 alternatives=dict(null=dict(T=1),normal=dict(Owner_R=1,receiver_W=1,D_E_T=3,T=1),
                   firstFailure=dict(Dispose=5,primary_null_test=4,raw_primary_assignment=2),
                   secondaryFailure=dict(Dispose=5,primary_nonnull_test=4,secondary_literal_R=1,raw_R=1,D=1,SetSecondary=44,T=1)),
 note='Dispose error K/catch transfer replaces primitive normalreturn at the opaque boundary; no physical reclaim credit. The62 path includes actual secondary helper, not all outcomes summed as a trace.')
model('Complete-TaskOwnerClose','normal16/propagate17',[
 vector('complete three predicate normal',(405,405),R=6,W=2,O=5,B=1,E=1,T=1)],
 alternatives=dict(propagate=dict(R=7,W=2,O=5,B=1,E=1,K=1)),
 note='PriorPrimary/Body arguments are read/dispatched at caller; no additional scalar destination for void result.')
model('Get-TaskSecondaryCodes','506',[
 group('entry and List constructor',(406,407),E=1,List_type_R=1,Length=GETTER,param_W=1,D_E_T=3,local_W=1),
 group('for Length framework',(408,408),for_fixed=13,eleven_condition_increment_bodies=11*14),
 group('eleven maximum retained slots',(408,408),eleven_source_rows=11*29),
 group('ToArray comma and return',(409,409),receiver_R_W=2,D_E_T=3,array_S=1,element_W=1,T=1)],
 note='Each row29=nonnull indexed guard7+three-field map14+Add receiver/parameter/dispatch8. The comma element W is explicit.11 is actual source sites, not padded metadata.')
model('Assert-TaskCodecBinding','normal18/refusal19',[
 vector('bound codecs and null DefaultSettings',(411,415),R=6,W=1,O=4,B=2,D=1,E=2,T=2)],
 alternatives=dict(default_refusal=dict(R=7,W=1,O=4,B=2,D=1,E=2,T=1,K=1)),
 note='DefaultSettings is static property; getter invoked, delegate not invoked. No static field/enum shortcut.')
model('Measure-TaskJson','104+Ddefault+Visitor',[
 group('entry/parameters',(474,474),E=1,W=2),
 group('Codec caller and body',(475,475),D=1,Codec=18),
 group('document profile gate',(476,476),Maximum_R_twice=2,two_literal_R=2,two_compare_O=2,two_or_O=2,null_R=1,profile_R=1,eq_O=1,B=1),
 group('private long3 and cap',(478,478),long_array_type_R=1,literal3_R=1,param_W=1,D_E_T=3,State_W=1,State_index_store=5),
 group('Visitor caller',(479,479),Value_R=1,literal0_R=1,State_R=1,D=1),
 group('final LF bound',(479,479),State_R=1,literal1_R=1,Int32_Long_O=1,D=1,AddBound=27),
 group('four-field measurement map and return',(480,480),map_S=1,key_R=4,field_W=4,three_state_index_reads=3*INDEX,literal_R=1,T=1)],
 excluded=['Visit-TaskJsonValue'],note='Omitted Maximum literal once at actual caller only. Typed state creation12, no managed scalar box assumption.')
model('Join-TaskBoundedNames','68+87K+64nameUnits',[
 group('entry/parameters',(482,482),E=1,W=2),
 group('extent before copy',(483,483),Count=GETTER_LOCAL,Maximum_R_twice=2,zero_R=1,two_compare_O=2,or_O=1,B=1),
 group('length init',(484,484),literal_R=1,W=1),
 group('indexed for fixed',(485,485),fixed=13),
 group('512Maximum Math.Max upper',(486,486),literal512_R=1,Maximum_R=1,mul_O=1,Math_type_R=1,zero_R=1,Maximum_R_again=1,one_R=1,sub_O=1,two_param_W=2,D_E_T=3,result_W=1,add_O=1,local_W=1),
 group('before allocation test',(487,487),two_Length_R=2,Upper_R=1,int_type_member=3,two_gt_O=2,or_O=1,B=1),
 group('Join explicit cast and return',(488,488),string_type_R=1,separator_R=1,IEnumerable_type_R=1,Names_R=1,cast_O=1,two_param_W=2,D_E_T=3,T=1)],
 coefficients=dict(name_row=dict(for_framework=14,String_fixed_default_caller=56,indexed_length_sum=12,separator_guard_and_increment=5),name_UTF16=dict(String_general=64)),
 note='Empty/singleton alias result branches are capacity facts for P, not a scalar shortcut. Exact IEnumerable type read is included.')

model('Read-TaskBoundedBytes','196+C_long',[
 group('entry/parameters',(690,690),E=1,W=2),
 group('FileInfo and Length local',(692,692),FileInfo_type_R=1,Path_R=1,param_W=1,ctor_D_E_T=3,Length_receiver_W=1,Length_D_E_T=3,Length_W=1),
 group('length admission',(692,692),Length_R_twice=2,Maximum_R_twice=2,zero_R=1,literal4MiB_R=1,three_compare_O=3,two_or_O=2,B=1),
 group('owner locals',(692,692),three_R=3,three_W=3),
 group('sequential unbuffered stream constructor',(693,693),type_R=1,Path_R=1,three_enums=9,literal1_R=1,FileOptions_enum=3,six_param_W=6,D_E_T=3,local_W=1),
 group('opened length admission',(693,693),Length=GETTER_LOCAL,Length_R=1,ne_O=1,B=1),
 group('byte array length cast',(693,693),byte_array_type_R=1,int_type_R=1,Length_R=1,cast_O=1,param_W=1,D_E_T=3,Bytes_W=1),
 group('ReadExactly',(693,693),File_receiver_R_W=2,Bytes_R=1,zero_R=1,Length_getter=GETTER,three_param_W=3,D_E_T=3),
 group('EOF check',(693,693),File_receiver_R_W=2,D_E_T=3,result_W=1,minus1_R=1,ne_O=1,B=1),
 group('comma return',(693,693),Bytes_R=1,array_S=1,element_W=1,T=1),
 group('catch raw primary path',(693,693),raw_R=1,Body_W=1,null_R=1,Primary_R=1,eq_O=1,B=1,raw_R_again=1,Primary_W=1,K=1),
 group('close/complete finally',(693,693),File_R=1,Close_D=1,CloseOwner_max=62,Prior_R=1,Body_R=1,Complete_D=1,Complete_max=17)],
 note='C_long actual caller conversion is1 for bootstrap literal,0 for ReadJson typedMaximum. ByteArray ctor9, not8; shared primitive destinations are fused. No byte CPU loop.')
model('Close-TaskJsonReader','same source outcome bounds as CloseOwner:7/12/17/62',[
 group('entry and nonnull guard',(695,699),E=1,W=1,null_R=1,Reader_R=1,ne_O=1,B=1)],
 alternatives=dict(null=dict(T=1),normal=dict(Reader_R=1,receiver_W=1,D_E_T=3,T=1),
                   firstFailure=dict(Close=5,primary_null_guard=4,primary_assignment=2),
                   secondaryFailure=dict(Close=5,primary_nonnull_guard=4,slot_literal_R=1,raw_R=1,D=1,Secondary=44,T=1)),
 note='CloseInputfalse leaves StreamReader separately owned. No scope/Dispose/reclaim credit.')
model('Read-TaskDependencyJson','358',[
 group('entry/parameters',(700,700),E=1,W=2),
 group('five owner/raw locals',(701,701),R=5,W=5),
 group('borrowed MemoryStream',(703,703),type_R=1,Bytes_R=1,zero_R=1,Length_R=1,two_false_R=2,five_param_W=5,D_E_T=3,local_W=1),
 group('strict UTF8 and StreamReader',(704,704),StreamReader_type_R=1,Memory_R=1,UTF8_type_R=1,two_bool_R=2,two_UTF8_param_W=2,UTF8_D_E_T=3,fused_encoding_parameter_W=1,detect_false_R=1,literal1024_R=1,leave_true_R=1,four_remaining_param_W=4,Reader_D_E_T=3,local_W=1),
 group('JsonTextReader and CloseInput',(705,705),JsonReader_ctor=7,CloseInput_setter=7),
 group('settings constructor',(706,706),type_R=1,D_E_T=3,local_W=1),
 group('four enum setters',(707,710),four_receiver_R_W=8,four_enum_type_member_reads=12,four_param_W=4,four_D_E_T=12),
 group('MaxDepth nullable and Boolean setters',(711,711),two_receiver_R_W=4,two_literal_R=2,two_param_W=2,two_D_E_T=6,Int32_Nullable_O=1),
 group('CreateDefault serializer',(712,712),type_R=1,Settings_R=1,param_W=1,D_E_T=3,local_W=1),
 group('Deserialize comma return',(715,715),receiver_R_W=2,JsonReader_R=1,param_W=1,D_E_T=3,array_S=1,element_W=1,T=1),
 group('caught raw primary',(716,716),Body_assignment=2,primary_guard=4,primary_assignment=2,K=1),
 group('complete finally',(717,717),CloseJson_caller_and_max=64,CloseText_caller_and_max=64,CloseMemory_caller_and_max=64,Complete_caller_and_max=20)],
 note='All four setters use actual enum3; MaxDepth liftO1, not nullable storage fields. SDK parser body CPU opaque; all controlled parser owners separately pending.')
model('Get-TaskDependencyString','98+64N',[
 group('entry/parameter',(719,719),E=1,W=1),
 group('value null local',(721,721),null_R=1,W=1),
 group('JValue/String typed guard',(722,722),RuntimeType_caller_R_D_W=3,RuntimeType=16,JValue_type_R=1,eq_O=1,and_O=1,get_Type=GETTER_LOCAL,String_enum=3,eq_O_again=1,B=1),
 group('actual get_Value local',(722,722),get_Value=GETTER_LOCAL),
 group('String fixed/default/caller',(723,723),Value_R=1,literal512_R=1,D=1,defaultAscii_R=1,String_fixed=49),
 group('return actual string',(724,724),R=1,T=1)],
 coefficients=dict(UTF16=dict(String_general=64)),note='Source/Includes only; no arbitrary JToken stringification. RuntimeType operator destination is explicit once.')
model('Read-TaskJson','predecessor334+dependency324=658 +raw/helper bodies as specified',[
 group('entry/parameters and actual literal Maximum lift',(726,726),E=1,param_W=3,caller_Int32_Long_O=1),
 group('Spend/Codec',(727,727),Spend_literal_caller_conversion_body=20,Codec_caller_body=19),
 group('bounded byte read caller/body',(727,727),Path_R=1,Maximum_R=1,D=1,Bytes_W=1,ReadBounded_identity=196),
 group('first/second Maximum branch predicates',(728,732),two_Maximum_R=2,two_literal_R=2,two_eq_O=2,two_B=2),
 group('Add whole visit',(731,731),Length_getter=GETTER,Int32_Long_O=1,D=1,AddVisit=17)],
 alternatives=dict(dependency=[group('captured dependency identity',(729,729),Expected_Length=GETTER_LOCAL,literal64_R=1,ne_O=1,or_O=1,SHA_Hex_nested=12,Expected_R=1,cne_O=1,B=1),
                               group('raw profile caller/local',(732,732),Bytes_R=1,literal2_R=1,profile_index=CHAINED_INDEX,int_type_R=1,cast_O=1,multiply_O=1,literal256_R=1,add_O=1,literal512_R=1,literal24_R=1,D=1,Compact_W=1),
                               group('private reader comma caller',(732,732),Bytes_R=1,Compact_R=1,D=1,array_S=1,element_W=1,T=1)],
                   predecessor=[group('captured predecessor identity',(730,730),Expected_R_twice=2,literal_hash_R=1,two_cne_O=2,or_O=1,SHA_Hex_nested=12,B=1),
                                group('raw profile caller/local',(732,732),Bytes_R=1,three_profile_literal_R=3,D=1,Compact_W=1),
                                group('UTF8 prefix GetString',(733,733),encoding_constructor_with_fused_receiver=9,three_source_arguments_R=3,three_param_W=3,D_E_T=3,Raw_W=1),
                                group('bytes null',(733,733),null_R=1,W=1),
                                group('actual cmdlet and return',(734,734),command_R=1,Raw_R=1,depth_literal_R=1,DateKind_literal_R=1,four_param_W=4,D_E_T=3,T=1)]),
 excluded=['Assert-TaskRawJsonBytes','Read-TaskDependencyJson'],
 note='Bounded byte bodies included once here, bootstrap ReadBounded separate. Actual Maximum caller lift sponsored here, no duplicate in main. AsHashtable flag parameterW is source-owned; no fabricated implicit literal read.')

model('Get-TaskHash','287+C_long+65B; choose C_long upper1 once at each actual call',[
 group('entry/parameters and function return',(146,156),E=1,W=2,T=1),
 group('Spend/path/stat/length guard',(147,147),Spend64_caller_and_body=20,NoLink_Path_R_D=2,FileInfo_constructor=7,Length_local=6,two_compare_guard=8),
 group('six owner/value locals',(148,148),R=6,W=6),
 group('sequential unbuffered FileStream',(149,149),type_R=1,Path_R=1,three_enum_arguments=9,literal1_R=1,options_enum=3,six_param_W=6,D_E_T=3,local_W=1),
 group('opened length guard',(149,149),Length=GETTER_LOCAL,taskLength_R=1,ne_O=1,B=1),
 group('IncrementalHash with static SHA256 property',(149,149),IncrementalHash_type_R=1,SHA256_static_getter_argument=STATIC_GETTER_ARGUMENT,D_E_T=3,local_W=1),
 group('while fixed',(150,150),final_Seen_R=1,final_Length_R=1,lt_O=1,terminal_K=1,loop_S=1,exit_K=1),
 group('exact EOF',(151,151),ReadByte=GETTER_LOCAL,minus1_literal_R=1,ne_O=1,B=1),
 group('Tuple2 direct produced result',(151,151),Tuple_type_R=1,Seen_R=1,GetHashReset_receiver_R_W=2,GetHashReset_D_E_T=3,two_Tuple_param_W=2,Tuple_D_E_T=3),
 group('catch raw primary',(152,152),Body_assignment=2,primary_guard=4,primary_assignment=2,K=1),
 group('two owned close calls',(153,153),two_Owner_R=2,two_D=2,two_Close_max=124),
 group('Complete caller/body',(153,153),Prior_R=1,Body_R=1,D=1,Complete_max=17)],
 coefficients=dict(read_chunk=dict(while_condition_and_control=6,Spend32_caller_and_body=20,Math_Min_int_local=13,ReadExactly=11,Seen_add=4,AppendData=11)),
 excluded=['Assert-NoLink','Get-TaskCanonicalPath'],
 note='Signed contiguous -1 is literalR1 only (EBB5...), EOF9. C_long actual nonidentity0/1 may be upper bounded by one per call; name this as an upper coefficient, not an observed conversion on every manifest Int64. Digest/Tuple never enter wire graph.')
model('Get-TaskBootstrapHash','277+48D+45B; D<=64; depth65 prefix dominated inside this helper',[
 group('entry/parameters',(304,304),E=1,W=2),
 group('bootstrap domain guard',(308,308),Path_Length=GETTER_LOCAL,literal512_R=1,gt_O=1,or_O=1,Expected_R=1,zero_R=1,le_O=1,B=1),
 group('At and Depth locals',(309,309),R=2,W=2),
 group('parent loop fixed',(310,310),final_At_R=1,terminal_K=1,S=1,exit_K=1),
 group('chained FileInfo Length admission',(311,311),FileInfo_and_Length=11,Expected_R=1,ne_O=1,B=1),
 group('six owner/value locals',(312,312),R=6,W=6),
 group('sequential FileStream',(314,314),ctor=25),
 group('opened length guard',(315,315),Length=GETTER_LOCAL,Expected_R=1,ne_O=1,B=1),
 group('IncrementalHash',(315,315),type_R=1,SHA256_static_getter_argument=STATIC_GETTER_ARGUMENT,D_E_T=3,local_W=1),
 group('read loop fixed',(316,316),Seen_R=1,Expected_R=1,lt_O=1,terminal_K=1,S=1,exit_K=1),
 group('EOF',(317,317),ReadByte=GETTER_LOCAL,minus1_literal_R=1,ne_O=1,B=1),
 group('nested hash/hex return',(317,317),GetHashReset_without_destination=GETTER,Convert_type_R=1,fused_hex_argument_W=1,D_E_T=3,function_T=1),
 group('catch raw primary',(318,318),Body_assignment=2,primary_guard=4,primary_assignment=2,K=1),
 group('owner finally',(319,319),two_Close_callers_and_max=128,Complete_caller_and_max=20)],
 coefficients=dict(parent=dict(while_condition_and_control=4,Depth_increment=3,Depth_guard=4,File_Exists=7,Directory_Exists=7,or_O=1,and_O=1,GetAttributes=7,reparse_enum=3,band_O=1,zero_R=1,ne_O=1,B=1,GetDirectoryName=7),
                   read_chunk=dict(while_condition_and_control=6,Math_Min_int_local=13,ReadExactly=11,AppendData=11,Seen_add=4)),
 alternatives=dict(depth65_prefix=dict(entry=3,domain=13,At_Depth=4,loop_setup=1,parents64=64*48,depth65_condition_increment_guard_throw=13)),
 note='Depth refusal prefix3106 is below normal parent-only3349; explicit same-helper source domination, not a new cross-family allowance. Seven actual fixed identities24581790bytes/379chunks; both existence and enum reads counted, internal bulk CPU opaque.')

model('Assert-TaskFileSet','normal102+83N+78M+64units; refusal171+90N+87M+64units+twoJoins',[
 group('entry/parameters',(827,827),E=1,W=2),
 group('two finite extents',(828,828),Observed_Count=GETTER_LOCAL,Expected_Count=GETTER_LOCAL,two_profile_cells=2*INDEX,two_gt_O=2,or_O=1,B=1),
 group('ExpectedSet static comparer constructor',(830,830),getter=STATIC_GETTER_ARGUMENT,constructor=CTOR_RESULT),
 group('expected foreach fixed',(830,830),named_expression_control=FOREACH_FIXED_NAMED),
 group('ObservedSet static comparer constructor',(831,831),getter=STATIC_GETTER_ARGUMENT,constructor=CTOR_RESULT),
 group('duplicate empty literal',(831,831),R=1,W=1),
 group('Unknown/Missing Lists',(831,831),two_constructors=2*CTOR_RESULT),
 group('two other foreach fixed',(832,833),named_expression_control=2*FOREACH_FIXED_NAMED),
 group('final count/missing/unknown/duplicate predicate',(834,834),four_Count_getters=4*GETTER_LOCAL,Duplicate_R=1,ne_O=1,three_or_O=3,B=1),
 group('normal return',(835,835),T=1)],
 coefficients=dict(expected_declared_row=dict(foreach=5,String_fixed_default_caller=53,Add_not_guard=10),
                   expected_missing_scan=dict(foreach=5,Contains_not_guard=10),
                   observed_row=dict(foreach=5,String_fixed_default_caller=53,SetAdd_not_guard=10,ExpectedContains_not_guard=10),
                   string_UTF16=dict(String_general=64),refusal_missing_extra=dict(Missing_Add=7),
                   refusal_observed_extra=dict(Duplicate_assignment=2,Unknown_Add=7)),
 alternatives=dict(refusal_surplus=dict(witness_map_and_callers=36,Reject_caller_and_body=34,replaced_normal_return=-1)),
 excluded=['Join-TaskBoundedNames missing','Join-TaskBoundedNames unknown'],
 note='N expected and M observed; two joins count actual bounded names, no generic traversal CPU.2staticgetters add2 relative to old100/169. Generic main/finally failures replace slots, not extra helper calls.')
model('Get-TaskStageBytes','21S+73Rows',[
 group('entry/sum',(750,751),E=1,literal0_R=1,Sum_W=1),
 group('directory existence branch',(751,751),type_R=1,StageRoot_R=1,param_W=1,D_E_T=3,result_W=1,B=1),
 group('walker caller/foreach fixed',(751,751),StageRoot_R=1,false_R=1,literal7_R=1,D=1,enumerable_W=1,foreach_S=1,terminal_K=1,exit_K=1),
 group('return sum',(751,751),R=1,T=1)],
 coefficients=dict(row=dict(foreach=5,Spend1=20,Item3_getter=6,or_O=1,Item2_getter=6,six_known_names_literal_array=13,cnotin_O=1,first_B=1,
                            Item4_cap_guard=11,Sum_add_Item4=9)),excluded=['Get-TaskEntries'],
 note='Source known names6. Cumulative admitted rows6S plus at mostthree first-refusing7th rows. Stage0/7/264/523/782/784 are separate cap-domain alternatives, no quiet child-output premise.')
model('Assert-TaskDocumentWrite','89+64PathN+Stage',[
 group('entry/parameters',(753,753),E=1,W=2),
 group('String fixed/default/caller',(754,754),Path_R=1,literal512_R=1,D=1,defaultAscii_R=1,String_fixed=49),
 group('File.Exists branch',(755,755),type_R=1,Path_R=1,param_W=1,D_E_T=3,result_W=1,B=1),
 group('maximal old file length local',(755,755),FileInfo_type_R=1,Path_R=1,param_W=1,ctor_D_E_T=3,fused_Length_receiver_W=1,Length_D_E_T=3,Old_W=1),
 group('length/stage arithmetic predicate',(756,756),Length_R_twice=2,zero_R=1,lt_O=1,or_O=1,StageCap_R=1,GetStage_D=1,GetStage_operand_W=1,Old_R=1,two_sub_O=2,gt_O=1,B=1),
 group('normal return',(757,757),T=1)],
 coefficients=dict(Path_UTF16=dict(String_general=64)),excluded=['Get-TaskStageBytes'],
 note='Produced FileInfo receiver does not acquire a second named-reference R. Stage result used as arithmetic operand retains its W. Else old-length0 is a bounded smaller branch.')
model('Write-TaskJsonBytes','34+DocumentWrite',[
 group('entry/parameters',(758,758),E=1,W=3),
 group('encoded cap guard',(759,759),Bytes_Length=GETTER_LOCAL,Maximum_R_twice=2,literal1MiB_R=1,two_gt_O=2,or_O=1,B=1),
 group('DocumentWrite caller',(760,760),Path_R=1,Bytes_Length_getter=GETTER,D=1,Int32_Long_O=1),
 group('WriteAllBytes primitive',(760,760),File_type_R=1,Path_R=1,Bytes_R=1,two_param_W=2,D_E_T=3),
 group('normal return',(761,761),T=1)],excluded=['Assert-TaskDocumentWrite'],
 note='Length getter result fuses with DocumentWrite parameter W. File API internal copying opaque CPU; controlled owners still separate.')
model('Write-TaskJson','own+MeasureIfNull+Encode+2DocumentWrite',[
 group('entry/parameters',(762,762),E=1,W=4),
 group('measurement null predicate',(763,763),null_R=1,Measurement_R=1,eq_O=1,B=1),
 group('selected Measure caller/local',(763,763),Value_R=1,Maximum_R=1,D=1,Measurement_W=1),
 group('first DocumentWrite caller',(764,764),Path_R=1,Measurement_bytes_index=INDEX,D=1),
 group('Encode caller/local',(765,765),Value_R=1,Maximum_R=1,Measurement_R=1,D=1,Raw_W=1),
 group('WriteBytes caller and own body',(766,766),Path_R=1,Raw_R=1,Maximum_R=1,D=1,WriteBytes_own=34),
 group('normal return',(767,767),T=1)],
 excluded=['Measure-TaskJson','Visit-TaskJsonValue','Encode-TaskJson','2*Assert-TaskDocumentWrite'],
 note='Actual failure caller null selects Measure; finalbundle nonnull skips its4 caller actions and Measure body. Default Maximum1 and default Measurement1 belong at omitted actual sites only.')
model('Save-TaskReceipt','own+Secondary+Measure+Encode+2DocumentWrite',[
 group('entry and receipt limit/counter',(768,769),E=1,Count_R=1,literal12_R=1,ge_O=1,B=1,Count_increment=3),
 group('secondary projection map slot caller',(770,770),Receipt_R=1,key_R=1,index_O=1,field_W=1,GetSecondary_D=1),
 group('path Join local',(771,771),StageRoot_R=1,receipt_literal_R=1,two_param_W=2,D_E_T=3,Path_W=1),
 group('Measure caller/local',(772,772),Receipt_R=1,literal65536_R=1,D=1,Measure_W=1,Int32_Long_O=1),
 group('DocumentWrite caller',(773,773),Path_R=1,Measure_bytes_index=INDEX,D=1),
 group('Encode caller/local',(774,774),Receipt_R=1,literal65536_R=1,Measure_R=1,D=1,Encoded_W=1,Int32_Long_O=1),
 group('WriteBytes caller/body',(775,775),Path_R=1,Encoded_R=1,literal65536_R=1,D=1,Int32_Long_O=1,WriteBytes_own=34),
 group('normal return',(776,776),T=1)],
 excluded=['Get-TaskSecondaryCodes','Measure-TaskJson','Visit-TaskJsonValue','Encode-TaskJson','2*Assert-TaskDocumentWrite'],
 note='Each actual literal65536 ->long parameter conversion occurs once at its own Measure/Encode/WriteBytes caller. Secondary return is not a second map value copy.')

model('Assert-TaskIdentity','normal24/refusal88',[
 group('entry/parameters',(777,777),E=1,W=2),
 group('two byte/hash comparisons',(777,777),four_index_reads=4*INDEX,two_compare_O=2,or_O=1,B=1),
 group('normal return',(777,777),T=1)],
 alternatives=dict(refusal_surplus=dict(witness_S=1,witness_key_R=5,witness_field_W=5,five_value_index_reads=5*INDEX,reason_R=1,D=1,Reject=32,replaced_return=-1)),
 note='Five map fields directly initialize Reject Witness parameter; no second field-store or parameter write. Refusal branch does not acquire normalT. Descriptor identities remain distinct from fresh digest identities.')
model('Assert-TaskBoundSourceInteger','normal54/kindRefusal90/nonpositiveRefusal105',[
 group('entry/parameters',(778,778),E=1,W=2),
 group('four exact signed-kind tests/local',(780,780),four_Value_R=4,four_type_R=4,four_is_O=4,three_or_O=3,Allowed_W=1),
 group('null/type selection predicate',(781,781),null_R=1,Value_R=1,eq_O=1,B=1),
 group('RuntimeType FullName local',(781,781),Value_R=1,D=1,RuntimeType=16,fused_FullName_receiver_W=1,FullName_D_E_T=3,Type_W=1),
 group('allowed guard',(782,782),Allowed_R=1,not_O=1,B=1),
 group('positive guard',(785,785),Value_R=1,zero_R=1,le_O=1,B=1),
 group('normal return',(786,786),T=1)],
 alternatives=dict(kind_surplus=dict(two_field_witness=7,reason_R=1,D=1,Reject=32,replaced_positive_guard_and_return=-5),
                   nonpositive_surplus=dict(five_field_witness=18,reason_R=1,D=1,Reject=32,replaced_return=-1)),
 note='Value untyped; no pre-admission coercion. Signed representability is not an invented source-work limit. Actual caller can use max105 as an upper; no old110 floor.')
model('Assert-TaskReservation','normal20/refusal63',[
 group('entry/parameters',(787,787),E=1,W=2),
 group('bound limit null guard',(787,787),null_R=1,Reviewed_R=1,eq_O=1,B=1),
 group('three reservation predicates',(787,787),Total_R_twice=2,Limit_R_twice=2,zero_R=1,Reviewed_R=1,three_compare_O=3,two_or_O=2,B=1),
 group('normal return',(787,787),T=1)],
 alternatives=dict(refusal_surplus=dict(three_field_witness=10,reason_R=1,D=1,Reject=32,replaced_return=-1)),
 note='Reviewed source limit remains unbound. Caller actual Int32->long conversions are separate named ports; no numeric limit is selected.')
model('Assert-TaskRetainedReservation','normal16/refusal59',[
 group('entry/parameters',(788,788),E=1,W=2),
 group('three retained predicates',(788,788),Total_R_twice=2,Limit_R_twice=2,zero_R=1,cap_literal_R=1,three_compare_O=3,two_or_O=2,B=1),
 group('normal return',(788,788),T=1)],
 alternatives=dict(refusal_surplus=dict(three_field_witness=10,reason_R=1,D=1,Reject=32,replaced_return=-1)),
 note='The existing880803840 cap is unchanged; semantic source price does not certify disk/heap fit.')
model('Assert-TaskAttempt','normal6/refusal46',[
 group('entry/parameters',(789,789),E=1,W=2),
 group('Exists guard',(789,789),R=1,B=1),
 group('normal return',(789,789),T=1)],
 alternatives=dict(refusal_surplus=dict(two_field_witness=7,reason_R=1,D=1,Reject=32,replaced_return=-1)),
 note='One attempt/occupied path control is actual; witness map parameter/result destinations occur once.')
model('Assert-TaskConsolePE','normal31/GUIrefusal89',[
 group('entry/parameter',(847,847),E=1,W=1),
 group('four PE comparisons',(847,847),four_index_reads=4*INDEX,four_literal_R=4,four_ne_O=4,three_or_O=3,B=1),
 group('normal return',(847,847),T=1)],
 alternatives=dict(refusal_surplus=dict(four_field_map_S=1,key_R=4,field_W=4,four_index_values=4*INDEX,reason_R=1,D=1,Reject=32,replaced_return=-1)),
 note='Source actual GUI control reaches this helper. No PE execution or native qualification is claimed.')
model('Convert-TaskControlOperand','builtin89; BigInteger53;rangeRefusal54',[
 group('entry/parameter',(377,377),E=1,W=1),
 group('BigInteger runtime type predicate',(380,380),Value_R=1,D=1,RuntimeType=16,result_W=1,BigInteger_type_R=1,eq_O=1,B=1)],
 alternatives=dict(builtin=[group('Integral caller/body',(384,384),Value_R=1,true_R=1,D=1,Integral=61),group('return same builtin',(387,387),R=1,T=1)],
                   biginteger=[group('Sign comparison',(381,381),Sign=GETTER_LOCAL,zero_R=1,lt_O=1),
                               group('CompareTo UInt64 upper',(381,381),Value_receiver_R_W=2,BigInteger_type_R=1,UInt64Max_static_member=3,constructor_param_W=1,ctor_D_E_T=3,fused_CompareTo_param_W=1,CompareTo_D_E_T=3,result_W=1,zero_R=1,gt_O=1,or_O=1,B=1),
                               group('UInt64 cast return',(382,382),uint64_type_R=1,Value_R=1,cast_O=1,T=1)]),
 note='Only six operands in each14real control record. Explicit constructor-owned bits array belongs to resource worksheet; no internal BigInteger CPU expansion. BigInteger range failure literalR/K replaces successful UInt64 cast/return.')
model('Expect-TaskGate','93+25K+selectedAction',[
 group('entry/parameters',(295,295),E=1,W=4),
 group('caught null initialization',(296,296),R=1,W=1),
 group('actual throwing action dispatch',(296,296),Action_R=1,D=1),
 group('catch Exception local',(296,296),Exception_getter=GETTER_LOCAL,local_W_fused=0,catch_transfer_K=1),
 group('reason/gate predicates',(297,297),null_Caught_guard=3,Message_getter_comparison=8,Data_gate_getter_index_comparison=11,two_or_O=2,B=1),
 group('actual witness local',(298,298),Data_getter_with_operand_W=GETTER_LOCAL,key_R=1,index_O=1,selected_R=1,local_W=1),
 group('witness counts',(298,298),Actual_Count=GETTER_LOCAL,Expected_Count=GETTER_LOCAL,ne_O=1,B=1),
 group('Expected.Keys foreach fixed',(299,299),get_Keys_and_controls=9),
 group('three-field retained control map',(300,300),map_S=1,key_R=3,field_W=3,Id_R=1,Data_gate_source=9,Actual_R=1,T=1)],
 coefficients=dict(witness_key=dict(foreach=5,Actual_Contains_not=10,Actual_index=4,Expected_index=4,cne_O=1,or_O=1,B=1)),
 excluded=['selected actual action own source'],
 note='Successful controls reach an actual throw/catch, so Out-Null after it is not reached. Unexpected normally returning Action has its actual primitive call/refusal branch bounded separately by its source prefix; no syntheticPASS.')
model('Replace-TaskBounded','fixed+threeASCIIvalidations+actualIndexOfloop+Replace',[
 group('entry/parameters',(284,284),E=1,W=3),
 group('three String callers and fixed portions',(285,285),three_argument_R=3,three_512_R=3,three_true_R=3,three_D=3,three_String_fixed=3*49),
 group('nonempty token guard',(286,286),Length=GETTER_LOCAL,zero_R=1,eq_O=1,B=1),
 group('Count/At locals',(287,287),two_literal_R=2,two_W=2),
 group('while fixed',(288,288),fixed=17),
 group('derived length local',(289,289),Value_Length=GETTER_LOCAL,Count_R=1,Replacement_Length=GETTER_LOCAL,Token_Length=GETTER_LOCAL,sub_O=1,mul_O=1,add_O=1,local_W=1),
 group('derived length guard',(290,290),Length_R_twice=2,two_literal_R=2,two_compare_O=2,or_O=1,B=1),
 group('actual Replace return',(291,291),Value_receiver_R_W=2,two_argument_R=2,two_param_W=2,D_E_T=3,T=1)],
 coefficients=dict(all_argument_UTF16=dict(String_ASCII=53),IndexOf_body=dict(while_condition_and_control=18,IndexOf_and_Found_local=13,found_guard=4,Count_increment=3,At_advance=9)),
 note='Exactly84calls from28arguments×three tokens. Token bodies depend actual6/6/7token lengths and bounded512source/replacement lengths; derive aggregate maximal occurrence schedule from them, not an arbitrary retry multiplier.')
