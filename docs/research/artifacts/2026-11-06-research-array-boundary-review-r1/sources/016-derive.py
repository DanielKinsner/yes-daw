"""Native Clock child source-owner model: own file/text/hash/integer checks only.
Never compiles, parses, loads or executes a target/helper/ABI program.
"""
from pathlib import Path
import hashlib
import json
import re

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
BASE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
SRC=BASE/'clock-oracle-r13-streamed-dependency-source-r1'
R12=BASE/'clock-oracle-r12-hash-carrier-source-r1'
STL=Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include')
checks=[];pins={}
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
    b=p.read_bytes()
    try:n=p.relative_to(ROOT).as_posix()
    except ValueError:n=p.as_posix()
    pins[n]=dict(path=n,bytes=len(b),sha256=sha(b));return b
def txt(p):return read(p).decode('utf-8-sig')
def ck(n,a,b=True):
    yes=a==b;checks.append(dict(name=n,passed=yes))
    if not yes:raise ValueError(n)
def contains(p,s):ck(p.name+':'+s[:70],s in txt(p))
def align8(n):return (n+7)//8*8
def string_cap(requested,old=15):return max(requested|15,old+old//2)

if (HERE/'owners.json').exists():raise ValueError('Preserve first complete green result')
cpp=txt(SRC/'clock_stage.cpp');book=txt(SRC/'source-costs.h');m=json.loads(read(SRC/'source-manifest.json'))
independent=BASE/'waveform-definition-r3/independent_clock.h'
time=ROOT/'src/engine/Time.h'
contract=ROOT/'build-ci/20261105-clock-oracle-separate-research-contract/contract.md'
read(contract)
ck('exact whole C++',sha(cpp.encode()),'F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206')
ck('R12 CPP exact',read(R12/'clock_stage.cpp'),cpp.encode())
ck('header R12 exact',read(R12/'source-costs.h'),read(SRC/'source-costs.h'))
ck('selected Time identity',sha(read(time)),'FC5A0861B527FFFE6ED2E6F9FE571D3F8974329BBD065DACFBC38DFEA8C98590')
ck('selected independent identity',sha(read(independent)),'A75FB5D17C9CACA2113DC8567A3E27F9A40FD9531BC59084E07F88576D15CB64')
ck('required selected compilation',all(x in m['compiler']['arguments'] for x in ['/std:c++20','/MD','/fp:strict','/D_ITERATOR_DEBUG_LEVEL=0','/O2','/SUBSYSTEM:CONSOLE']))
ck('all source guards false','sourceReviewAccepted=false, sourceWorkAccepted=false, executionAccepted=false' in cpp)
ck('no numeric definition','static const Catalogue reviewed_;' in book and 'Catalogue::reviewed_ =' not in book)
family_body=book.split('enum class Family : std::size_t {',1)[1].split('};',1)[0]
families=[x.strip() for x in family_body.replace('\n',' ').split(',') if x.strip()]
ck('Count sentinel',families[-1],'Count');family_count=len(families)-1;ck('76 families',family_count,76)
catalogue=32+16*family_count;ck('one prospective Catalogue payload',catalogue,1248)

for s in ['std::array<char,65536> data{};std::size_t size=0;', 'std::array<unsigned char,65536> buffer{};', 'std::array<std::pair<double,double>,735> rows{};', 'sizeof(Text)<=65552&&sizeof(Stream)<=65664', '8*(sizeof(Stream)+sizeof(Text))+sizeof(FractionCache)+32768<=2097152', 'changes(qc::fixtureMap(map)),base(r,changes)', 'for(int map=0;map<9;++map)', 'if(R==48000&&map==5)', 'return root+"/"+relative;', 'const std::string root=argv[2],manifest=argv[3]', 'Text readText(', 'return t;', '_IONBF', 'file=nullptr;cleanupFileClose', 'catch(const Rejection& e)', 'catch(const std::exception& e)']:
    contains(SRC/'clock_stage.cpp',s)
for s in ['std::vector<Change> changes_', 'std::vector<double> prefix_', 'prefix_.assign(changes_.size(),0.0)', 'changes_(std::move(changes))', 'return {{0,90.0,false}']:
    contains(independent,s)
for s in ['out = CompiledTempoMap {};', 'out.changes_.assign (tempoMap.changes, tempoMap.changes + tempoMap.count);', 'out.startFrame_.assign (tempoMap.count, 0.0);']:
    contains(time,s)
for f in ['vector','xstring','xmemory','utility','vcruntime_exception.h']:
    read(STL/f)
for s in ['_STD exchange(_Right._Mypair._Myval2._Myfirst, nullptr)', '_Construct_n(_Count, _Right_data._Myfirst, _Right_data._Mylast);', '_Buy_nonzero(_Count);', 'return _Newsize; // geometric growth would be insufficient']:
    contains(STL/'vector',s)
for s in ['_Small_string_capacity = _BUF_SIZE - 1', 'sizeof(value_type) <= 1 ? 15', 'return (_STD max)(_Masked, _Old + _Old / 2);', '++_Capacity; // Take null terminator into consideration', 'return _STD move(_Left.append(_Right));']:
    contains(STL/'xstring',s)
for s in ['_Alloc_proxy(const _Fake_allocator&) noexcept {}', 'using _Container_proxy_ptr = _Fake_proxy_ptr_impl;', 'return _Al.allocate(_Count);']:
    contains(STL/'xmemory',s)
contains(STL/'utility','_Ty1 first;');contains(STL/'utility','_Ty2 second;')
contains(STL/'vcruntime_exception.h','__std_exception_data _Data;')

# Conditional selected x64 field models, not observed sizes/offsets.
unit=dict(Text_source_assert_upper=65552,Stream_source_assert_upper=65664,
    FractionCache_pair16_x735_plus_three_U64=735*16+24,Catalogue_76rows=1248,
    CleanupCloseState=24,Pair128=16,Native_and_MusicalTime=16,Cut=32,
    Picks_Item=16,Picks=10*16+10*8+8,Bounds=104,Meter=24,
    std_vector_D0_header=24,std_string_char_D0_header=32,
    NumericAClock=56,Production=64,CachedPort=160,
    Rejection_own_fields_and_exception_base=80,source_witness_array6=48,
    production_rows_array4_assert_upper=4*32,fixture_initializer_list_array4_upper=4*32,
    Text_u_digits=20,Text_quoted_char_pair=2,Stream_fallback_word_array=8,
    verifier_owned_short_trailing_array=9,source_lambda_max5references=40)
core=2*unit['Stream_source_assert_upper']+4*unit['Text_source_assert_upper']+unit['FractionCache_pair16_x735_plus_three_U64']
old_expression=8*(unit['Stream_source_assert_upper']+unit['Text_source_assert_upper'])+unit['FractionCache_pair16_x735_plus_three_U64']+32768
ck('logical core405320',core,405320);ck('assert expression conditional',old_expression,1094280)
ck('assert inequality only',old_expression<=2097152)

rows_per_rate=7*3+2*4
rows=3*rows_per_rate
ck('27 fixture map rows87',(rows,3*9),(87,27))
fixture_vectors=2*rows*32+rows*8
prod_rows=dict(produce=rows+3,verify=rows+3+3+3)
vector_payload={mode:fixture_vectors+n*(32+8) for mode,n in prod_rows.items()}
ck('vector cumulative payload',(vector_payload['produce'],vector_payload['verify']),(9864,10104))
vector_allocs=dict(produce=27*5+2,verify=27*5+2+4)
ck('vector request counts',(vector_allocs['produce'],vector_allocs['verify']),(137,141))
# Largest live backing set: a three-row normal fixture plus one three-row mutant.
live_vectors=max(4*(2*32+8+32+8),3*(2*32+8+32+8)+(3*(32+8)))
ck('live vector backing456',live_vectors,456)

# Two path concatenations retain both old/new capacities in the request sum.
root_len=512;relative_len=128
root_capacity=string_cap(root_len);manifest_capacity=string_cap(64)
first_capacity=string_cap(root_len+1)
path_capacity=string_cap(root_len+1+relative_len,first_capacity)
path_request_payload=(first_capacity+1)+(path_capacity+1)
ck('root/path capacities',(root_capacity,first_capacity,path_capacity,path_request_payload),(527,527,790,1319))
stem_len=1+5+4+1
ck('fixed number/stem SSO',stem_len<=15 and 5<=15)
rel_first=string_cap(len('references/clock/')+stem_len)
rel_q=string_cap(len('references/clock/')+stem_len+len('-q.bin'),rel_first)
rel_meta=string_cap(len('references/clock/')+stem_len+len('-metadata.json'),rel_first)
ck('two relative concatenation requests',(rel_first,rel_q,rel_meta),(31,47,47))
relative_payload_per=rel_first+1+max(rel_q,rel_meta)+1
control_capacity=max(string_cap(root_len+len('/../controls/short.bin')),string_cap(root_len+len('/../controls/trailing.bin')))
ck('control concatenation capacity',control_capacity,543)
constant_inverse=2*(string_cap(42)+1)
root_manifest=(root_capacity+1)+(manifest_capacity+1)
strings_common=56*path_request_payload+54*relative_payload_per+constant_inverse+root_manifest
strings=dict(produce=strings_common,verify=strings_common+4*(control_capacity+1))
ck('cumulative native string payload',(strings['produce'],strings['verify']),(78888,81064))

mode_schedules={}
for verify in [False,True]:
    mode='verify' if verify else 'produce'
    metadata=28+int(verify);reads=28 if verify else 0
    text_occasions=54+2*metadata+2*reads
    streams=56+(4 if verify else 0)
    automatic_row_arrays=2*(28+metadata+(2 if verify else 0))
    mode_schedules[mode]=dict(fixtures=27,FractionCache_constructions=3,FractionCache_rows_capacity_each=735,FractionCache_rows_populated_total=735+25+25,CachedPort_constructions=27,production_normal_builds=27,production_controls_builds=1,production_mutant_builds=2 if verify else 0,vector_allocation_requests_upper=vector_allocs[mode],vector_ever_requested_payload=vector_payload[mode],vector_logical_live_backing_upper=live_vectors,Stream_constructions=streams,Text_number_calls=54,Text_metadata_calls=metadata,Text_readText_calls=reads,Text_source_owner_occasions_no_NRVO=text_occasions,productionRows_calls=automatic_row_arrays//2,productionRows_array4_owner_occasions_no_NRVO=automatic_row_arrays,string_path_calls=56,string_relative_concat_expressions=54,source_control_path_concats=4 if verify else 0,source_string_alloc_request_upper=224+(4 if verify else 0),string_ever_requested_payload_upper=strings[mode],own_expected_Rejection_throws=11 if verify else 3,unexpected_terminal_throw_extra_upper=1,logical_fixed_core_upper=core,prospective_catalogue_payload=1248,owned_static_hash_arrays=130,owned_static_cleanup=24,named_core_plus_static_plus_ever_container_payload=core+1248+130+24+vector_payload[mode]+strings[mode])
ck('Text no-NRVO owner counts',(mode_schedules['produce']['Text_source_owner_occasions_no_NRVO'],mode_schedules['verify']['Text_source_owner_occasions_no_NRVO']),(110,168))
ck('Stream construction schedule',(mode_schedules['produce']['Stream_constructions'],mode_schedules['verify']['Stream_constructions']),(56,60))
ck('metadata/read construction schedule',(mode_schedules['verify']['Text_metadata_calls'],mode_schedules['verify']['Text_readText_calls']),(29,28))

# Input extent only. CRT/PEB/startup representations and capacities remain named.
arg_units=7+512+64+3*20+6 # argv1..6 including their NULs
argv_extent=arg_units+(512+1)
ck('narrow argv extent',(arg_units,argv_extent,8*8),(649,1162,64))
env_keys=list(m['environment'])
ck('eight explicit child environment keys',len(env_keys),8)
key_units=sum(map(len,env_keys))
env_units=8*(512+len('/opaque-temp'))+key_units+2*8+1

# Written owner sites are source facts; counts are closed schedules above, not
# an inference from a generic byte allowance or the 2MiB static_assert.
site_patterns={
 'Text':['struct Text','std::string number(','Text readText(','Text metadata(','const auto text=metadata','auto actual=readText','const auto actual=metadata','const auto descriptor=metadata','if(verify)readText'],
 'Stream':['struct Stream','void writeText(','Text readText(','Stream q(','Stream shortFile','Stream extraFile','Stream inv('],
 'Clock_vectors':['struct CachedPort','CachedPort generator','auto production=buildProduction','const auto production=buildProduction','Production mutant','Production buildProduction'],
 'Fixed':['struct FractionCache','const FractionCache fractions','struct Picks','struct Bounds','struct Meter','struct Rejection','CleanupCloseState cleanupClose','owned{}'],
 'Strings':['std::string number(','std::string path(','const auto stem=','const std::string binary=','const std::string root=','root+"/../controls/']}
sites=[]
for kind,patterns in site_patterns.items():
    for line_no,line in enumerate(cpp.splitlines(),1):
        matched=[p for p in patterns if p in line]
        if matched:sites.append(dict(kind=kind,line=line_no,matched=matched,source=line))

result=dict(schema='CLOCK_NATIVE_CHILD_COMPLETE_SOURCE_OWNER_MODEL_R1',verdict='SCOPED_HOLD_NUMERIC_SOURCE_COMPONENTS_NOT_WHOLE_CHILD_PROOF',exact_CPP_sha256=sha(cpp.encode()),exact_source_costs_sha256=sha(read(SRC/'source-costs.h')),selected_compiler=m['compiler'],selected_ABI_conditions=['Windows AMD64 pointer/size_t8,uint64/double8,ordinary default packing/alignment','MSVC14.44 selected headers,std:c++20,ITERATOR_DEBUG_LEVEL0,default std::allocator,char string','future actual object/compiler/library/source association and constant Catalogue definition remain unqualified'],unit_models=unit,mode_schedules=mode_schedules,source_sites=sites,input_extent=dict(argv1_to6_narrow_characters_including_NUL=arg_units,argv0_to6_narrow_characters_upper=argv_extent,argv_pointer_table8x8=64,child_environment_keys=env_keys,environment_key_units=key_units,supplied_environment_block_UTF16_units_upper=env_units,supplied_environment_block_bytes_upper=env_units*2,environment_value_source_bound='Manifest values admitted512; TEMP/TMP can become root512+/opaque-temp12. Use524 upper rather than assume replacement never grows. Exact selected root tightens it; no host environment read.',scope='Input text extent only, not CRT startup argv/env/TLS allocations'),static_assert=dict(source_expression='8*(sizeof(Stream)+sizeof(Text))+sizeof(FractionCache)+32768<=2097152',conditional_numeric_expression=old_expression,whole_owner_proof=False,reason='It constrains named type sizes, not counts/frames/capacities/CRT/native owners.32768 has no independent whole-call-chain resource proof.'),live_vs_ever=dict(automatic='Text/Stream counts are source owner constructions and logical overlap, not cumulative heap allocation or observed stack frame size.',buffers='All successful/failed container request slots remain in ever-requested payload; no destructor/delete/realloc/allocator-reuse credit is used in this column.',process='Only actual observed child exit releases that process address space. No exit is observed here; unsettled/quarantined ownership remains charged.'),remaining=[dict(id='ABI_FRAME',required='Actual selected std::pair/vector/string/unit layout association; complete own/helper/selected-STL stack frames including no-NRVO return slots, spills,alignment,shadow space,exception unwind and possible inlining/nonreuse. Source logical2Streams/4Texts is not a physical frame certificate.',next='Read the selected object/assembly frame listing when that exact source compilation is authorized; no target run required. Do not sponsor it with unexplained32768.'),dict(id='CRT_FILE_NATIVE',required='At most2 app Stream FILE/fd owners or in-progress open failures plus3 CRT standard streams; _IONBF does not prove no FILE state,conversion/path/native handle or formatting buffer. Include failed _fdopen/setvbuf/fclose and root string normalization/conversion owners.',next='Pin selected import/module/source association for _sopen_s/_fsopen/_fdopen/setvbuf/stdio; classify controlled buffers under the existing boundary, not an indefinite vendor trace.'),dict(id='CRT_STARTUP_TLS',required='Pre-main narrow/wide argv and environment copies/tables,CRT startup,errno/fenv/libm locale/TLS/lazy initialization and active worker/runtime owners. Source supplies finite input extents but does not determine their allocation multiplicities.',next='Use exact admitted child command/environment and already selected runtime module/source associations; no host environment/process enumeration or synthetic probe credit.'),dict(id='EH_ALLOCATOR',required='Rejection unit80 excludes EH runtime header/TLS/exception storage and copied std::runtime_error/logic_error messages. Keep source-sized char/vector allocation requests separate from allocator rounding/retention and generic CRT heap/RSS, which remain explicitly unqualified by contract47-50.',next='Close the required selected allocation/exception resource profile without deliberate OOM/trap execution; no assumption that delete/close returned memory to OS.'),dict(id='DATA_AND_PROFILE',required='The prospective Catalogue1248 has no value definition. Final literal constinit binding/source/inputs/libs/ABI and actual mode admission remain HOLD; no prices or caller pass boolean created.'),dict(id='JOINT_CONTROLLER',required='Join one active producer/verifier child with its simultaneous controller phase/quarantine under whole32MiB. No per-process split,unobserved exit reset,compiler exception extension or R13/R12 transfer.',next='Recompose with exact controller and this qualified native model after the remaining leaves close; an upper miss is not a simultaneous lower/NOFIT.' )],contract_boundary='Existing contract344408DA lines47-50 leaves OS/CRT/libm primitive CPU/general heap/allocator overhead/RSS unqualified, while controlled payloads remain included. This report creates no new process/library exemption.',no_source_cap_profile_or_Data_change=True,no_target_execution=True,no_fit_NOFIT_or_GO=True,checks=dict(count=len(checks),rows=checks),input_pins=list(pins.values()))
(HERE/'owners.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='FIRST_COMPLETE_OWN_SOURCE_CHECK_PASS_SCOPED_HOLD',checks=len(checks),family_count=family_count,fixed_core=core,vector_ever=vector_payload,string_ever=strings,Text_counts={k:v['Text_source_owner_occasions_no_NRVO'] for k,v in mode_schedules.items()},sha256=sha((HERE/'owners.json').read_bytes()))))
