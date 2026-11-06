"""Own source/file/hash/integer preparation only. No compiler, parser, decoder,
launcher, native object, target or old helper is invoked/imported.
"""
from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parent;R=P.parents[1]
S=R/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
if (P/'own-checks.json').exists():raise ValueError('Preserve first complete own pass')
checks=[];inputs=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
 b=p.read_bytes()
 try:n=p.relative_to(R).as_posix()
 except ValueError:n=p.as_posix()
 inputs.append(dict(path=n,bytes=len(b),sha256=sha(b)));return b
def ck(n,c):
 checks.append(dict(name=n,passed=bool(c)))
 if not c:raise ValueError(n)
def write(name,value):
 text=json.dumps(value,indent=2)+'\n' if not isinstance(value,str) else value
 (P/name).write_text(text,encoding='utf-8',newline='\n')
manifest_bytes=read(S/'source-manifest.json');m=json.loads(manifest_bytes)
ck('exact manifest',sha(manifest_bytes)=='10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3')
cpp_bytes=read(S/'clock_stage.cpp');cpp=cpp_bytes.decode()
ck('exact Clock only read',sha(cpp_bytes)=='F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206')
for path,expected in [(S/'source-costs.h','0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27'),(S.parent/'waveform-definition-r3/independent_clock.h','A75FB5D17C9CACA2113DC8567A3E27F9A40FD9531BC59084E07F88576D15CB64'),(R/'src/engine/Time.h','FC5A0861B527FFFE6ED2E6F9FE571D3F8974329BBD065DACFBC38DFEA8C98590')]:
 ck('exact layout dependency '+path.name,sha(read(path))==expected)
for name in ['docs/adr/0090-clock-research-owned-process-supervisor.md','docs/plans/2026-11-06-clock-supervisor-source-plan.md','build-ci/20261105-clock-oracle-separate-research-contract/contract.md','build-ci/20261106-clock-native-resource-boundary-audit-r1/source-pins.json']:
 b=read(R/name)
 if '0090-' in name:ck('ADR accepted and purpose gate',b'**Status:** Accepted' in b and b'compile-only purpose' in b)
 if 'source-plan' in name:ck('plan separate compilation gate',b'separate bounded compile-only purpose lane' in b)
tool=Path(m['compiler']['path']).parent
include=tool.parents[2]/'include'
ucrt=Path('C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt')
prefixes=[include.as_posix()+'/',ucrt.as_posix()+'/',tool.as_posix()+'/']
selected=[row for row in m['inputs'] if any(row['path'].startswith(v) for v in prefixes)]
ck('bounded selected file inventory',len(selected)==518)
for row in selected:
 b=read(Path(row['path']));ck('selected compiler/header '+row['path'],len(b)==row['bytes'] and sha(b)==row['sha256'])
ck('exact compiler manifest identity',m['compiler']['sha256']=='88C8344236A27A6E727E0A8EDC49AAA2690BDC7A9464B9D18CC7ABE70A9F1C0D')
profile=dict(schema='SEPARATE_COMPILE_ONLY_LAYOUT_PURPOSE_PROFILE_R1',state='UNRUN_NO_COMPILER_GO',compiler=dict(path=m['compiler']['path'],sha256=m['compiler']['sha256'],arguments=['/nologo','/std:c++20','/EHsc','/W4','/WX','/MD','/permissive-','/fp:strict','/O2','/D_ITERATOR_DEBUG_LEVEL=0','/I<REPO>/src','/c','<SOURCE>','/Fo<OBJECT>','/sourceDependencies','<DEPS>']),environment={'SystemRoot':'C:/Windows','windir':'C:/Windows','PATH':tool.as_posix()+';C:/Windows/System32','INCLUDE':include.as_posix()+';'+ucrt.as_posix(),'TEMP':'<NEW_PURPOSE_RUN>/compiler-temp','TMP':'<NEW_PURPOSE_RUN>/compiler-temp','VSLANG':'1033'},difference_from_Clock='Same ABI-affecting flags and original include order. Replace whole Clock TU/link with this purpose TU and /c; emit dependencies and one object; no /DNDEBUG,/Zp,/arch override,link or executable.',include_roots=[include.as_posix(),ucrt.as_posix()],compiler_inventory_pinned_from_old_admission=True,compiler_internal_helpers_temp_peak_heap_RSS_not_certified=True,root_separate_approval_required=True)
write('profile.json',profile)
replica=read(P/'layout-types.h').decode()
groups=[
 ('Rejection','struct Rejection:std::exception {','struct CleanupCloseState',['const char* reason;std::array<std::uint64_t,6> values{};'],'std::exception; identical ctor/virtual what override bodies retained'),
 ('CleanupCloseState','struct CleanupCloseState{','CleanupCloseState cleanupClose;',['std::uint64_t failures=0;int firstReturn=0,firstErrno=0;const char* firstSite="none";'],'No base; same defaults'),
 ('Cost','struct Cost {','struct Meter {',[],'No nonstatic fields/bases/virtuals; all ten static signatures retained, body definitions omitted'),
 ('Meter','struct Meter {','struct Text {',['std::uint64_t used=0,reserved,prior;'],'No base/virtual; constructor/add signatures retained without bodies'),
 ('Text','struct Text {','std::string number(',['std::array<char,65536> data{};std::size_t size=0;'],'No base/virtual; all five method signatures retained without bodies'),
 ('Stream','struct Stream {','void writeText(',['FILE* file=nullptr;std::array<unsigned char,65536> buffer{};std::size_t at=0,size=0;','std::uint64_t bytes=0,maximum;bool write;','Stream(const Stream&)=delete;Stream& operator=(const Stream&)=delete;'],'No base/virtual; ctor/user-provided noexcept destructor, deleted copy and six method overload signatures retained; bodies omitted'),
 ('Pair128','struct Pair128{','Pair128 multiply(',['std::uint64_t high,low;'],'Exact one-line type except namespace'),
 ('FractionCache','struct FractionCache {','static_assert(sizeof(Text)',['std::array<std::pair<double,double>,735> rows{};std::uint64_t N,g,count;'],'No base/virtual; same constructor signature without body'),
 ('CachedPort','struct CachedPort {','using Production=',['std::size_t segmentIndex_=0;unsigned cursorAdvances_=0;','std::vector<qc::Change> changes;qc::NumericAClock base;double rate;','qc::Tick saved=-1;double wholeFrame=0,k=0,bpm=0,slope=0;Meter& meter;std::uint64_t misses=0;'],'Private then public access groups retained; Meter& refers to corresponding replica; same ctor/frame signatures without bodies; no offsetof on this nonstandard/reference-bearing type'),
 ('Picks','struct Picks {','struct Bounds{',['struct Item{std::uint64_t index;unsigned slot;};std::array<Item,10> items{};std::array<std::int64_t,10> values{};unsigned next=0;'],'Nested Item retained exactly; same ctor/see signatures without bodies'),
 ('Bounds','struct Bounds{','void field(',['std::uint64_t R,N,W;int map;qc::Cut begin,end;std::int64_t D;'],'Exact one-line type except namespace')]
correspondence=[]
for name,start,end,fields,note in groups:
 ck('one original start '+name,cpp.count(start)==1)
 original=start+cpp.split(start,1)[1].split(end,1)[0]
 for field in fields:ck('same complete storage declaration '+name+':'+field,field in original and field in replica)
 ck('replica declared '+name,('struct '+name) in replica)
 correspondence.append(dict(name=name,original_definition=original,original_definition_sha256=sha(original.encode()),storage_declarations=fields,base_access_special_member_correspondence=note,nominal_scope_change='clock_stage -> layout_purpose; not the same C++ nominal type',method_body_change='Only listed bodies omitted; no member instantiated/invoked. Nonvirtual body text is not storage; Rejection full body retained.',source_whole_sha256=sha(cpp_bytes)))
write('correspondence.json',dict(scope='Reviewed layout equivalence hypotheses only; no actual compiler values yet',replicas=correspondence,actual_types=['qc::Change','qc::Native','qc::Cut','qc::NumericAClock','yesdaw::engine::TempoChange','yesdaw::engine::MusicalTime','yesdaw::engine::SampleRate','yesdaw::engine::CompiledTempoMap','clock_stage::source_costs::FamilyBound','clock_stage::source_costs::WholeBound','clock_stage::source_costs::Catalogue','Selected std::pair/vector/string/exception/array types'],packing='No new pack directive; same original includes and default compiler profile; _CRT_PACKING8 and x64 scalar profile asserted. Object padding is included in sizeof.',not_covered=['Private/nonstandard offsets','Actual Clock function closure layouts','Lifetime/multiplicity or complete automatic-object sum','Machine frames/spills/shadow space/RSS','CRT allocation/startup/TLS/exception message or FILE internals','Resource fit/prices/Data/target or supervisor behavior']))
types=[('pointer','void*'),('size_t','std::size_t'),('bool','bool'),('char','char'),('unsigned_char','unsigned char'),('int','int'),('unsigned','unsigned'),('u64','std::uint64_t'),('Tick','qc::Tick'),('double','double')]
types += [(n,'layout_purpose::'+n) for n,_,_,_,_ in groups]
types += [('PicksItem','layout_purpose::Picks::Item'),('qcChange','qc::Change'),('qcNative','qc::Native'),('qcCut','qc::Cut'),('NumericAClock','qc::NumericAClock'),('TempoChange','yesdaw::engine::TempoChange'),('MusicalTime','yesdaw::engine::MusicalTime'),('SampleRate','yesdaw::engine::SampleRate'),('Production','yesdaw::engine::CompiledTempoMap'),('Family','clock_stage::source_costs::Family'),('TempoCurve','yesdaw::engine::TempoCurve'),('TimeLookupStatus','yesdaw::engine::TimeLookupStatus'),('FamilyBound','clock_stage::source_costs::FamilyBound'),('WholeBound','clock_stage::source_costs::WholeBound'),('Catalogue','clock_stage::source_costs::Catalogue'),('FamilyRows','std::array<clock_stage::source_costs::FamilyBound,clock_stage::source_costs::familyCount>'),('DoublePair','std::pair<double,double>'),('ChangeVector','std::vector<qc::Change>'),('TempoVector','std::vector<yesdaw::engine::TempoChange>'),('DoubleVector','std::vector<double>'),('String','std::string'),('ChangeIterator','std::vector<qc::Change>::iterator'),('ChangeConstIterator','std::vector<qc::Change>::const_iterator'),('TempoIterator','std::vector<yesdaw::engine::TempoChange>::iterator'),('TempoConstIterator','std::vector<yesdaw::engine::TempoChange>::const_iterator'),('DoubleIterator','std::vector<double>::iterator'),('DoubleConstIterator','std::vector<double>::const_iterator'),('StringIterator','std::string::iterator'),('StringConstIterator','std::string::const_iterator'),('ChangeInitializerList','std::initializer_list<qc::Change>'),('RateInitializerList','std::initializer_list<std::uint64_t>'),('Exception','std::exception'),('RuntimeError','std::runtime_error'),('LogicError','std::logic_error'),('TextBuffer','std::array<char,65536>'),('StreamBuffer','std::array<unsigned char,65536>'),('FractionRows','std::array<std::pair<double,double>,735>'),('Witness6','std::array<std::uint64_t,6>'),('ProductionRows4','std::array<yesdaw::engine::TempoChange,4>'),('FixtureRows4','std::array<qc::Change,4>'),('Digits20','char[20]'),('Quoted2','char[2]'),('Fallback8','unsigned char[8]'),('Owned9','unsigned char[9]'),('Constants5','double[5]'),('Rates3','std::uint64_t[3]')]
entries=[]
def cell(name,expression,kind):entries.append(dict(name=name,expression=expression,kind=kind))
for name,typ in types:
 cell(name+'.sizeof','sizeof('+typ+')','sizeof')
 cell(name+'.alignof','alignof('+typ+')','alignof')
offsets={'CleanupCloseState':['failures','firstReturn','firstErrno','firstSite'],'Meter':['used','reserved','prior'],'Text':['data','size'],'Stream':['file','buffer','at','size','bytes','maximum','write'],'Pair128':['high','low'],'FractionCache':['rows','N','g','count'],'PicksItem':['index','slot'],'Picks':['items','values','next'],'Bounds':['R','N','W','map','begin','end','D'],'qcChange':['tick','bpm','ramp'],'qcNative':['whole','frac'],'qcCut':['admitted','floor','low','high'],'TempoChange':['tick','bpm','curveToNext'],'MusicalTime':['tick','frac'],'SampleRate':['hz'],'FamilyBound':['family','upperUnits'],'WholeBound':['runnerUpper','producerUpper','verifierUpper','totalUpper']}
lookup=dict(types);asserts=[]
for name,fields in offsets.items():
 typ=lookup[name];asserts.append('static_assert(std::is_standard_layout_v<'+typ+'>);')
 for member in fields:cell(name+'.offsetof.'+member,'offsetof('+typ+','+member+')','public-standard-layout-offset')
for name,expr in [('compiler.MSC_FULL_VER','_MSC_FULL_VER'),('compiler.MSC_VER','_MSC_VER'),('compiler.CRT_PACKING','_CRT_PACKING'),('compiler.ITERATOR_DEBUG_LEVEL','_ITERATOR_DEBUG_LEVEL'),('source.familyCount','clock_stage::source_costs::familyCount')]:cell(name,expr,'profile-constant')
schema=dict(schema='NATIVE_LAYOUT_CONSTANT_CELL_SCHEMA_R1',magic=0x59444C4159303031,version=1,symbol='yesdaw_layout_cells',section='.ylayout',entries=entries,values='UNOBSERVED_UNRUN',scope='sizeof/alignof and only legal public standard-layout offsets; no private field access or object construction')
write('schema.json',schema)
headers=['#include "../musical-audio-persistent-planner-certification/source-revisions/waveform-definition-r3/independent_clock.h"','#include "../../src/engine/Time.h"','#include "../musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1/source-costs.h"']
original_includes=[line for line in cpp.splitlines() if line.startswith('#include <')]
translation='// UNRUN compile-only purpose. No main, Data, constructor call or runtime entry.\n'+'\n'.join(headers+original_includes)+'\n#pragma fenv_access(on)\n#pragma fp_contract(off)\n#include "layout-types.h"\n#include <cstddef>\n#if !defined(_M_X64) || !defined(_DLL) || !defined(_MT)\n#error Unsupported compiler ABI/runtime profile\n#endif\n#ifdef NDEBUG\n#error Original Clock profile has no NDEBUG definition\n#endif\nstatic_assert(_MSC_VER==1944 && _ITERATOR_DEBUG_LEVEL==0 && _CRT_PACKING==8);\nstatic_assert(sizeof(void*)==8 && sizeof(std::size_t)==8 && sizeof(double)==8 && sizeof(std::uint64_t)==8 && sizeof(int)==4 && sizeof(bool)==1);\nstatic_assert(std::endian::native==std::endian::little);\nstatic_assert(sizeof(layout_purpose::Text)<=65552 && sizeof(layout_purpose::Stream)<=65664);\nstatic_assert(sizeof(qc::Change)<=32 && sizeof(yesdaw::engine::TempoChange)<=32);\n'+'\n'.join(asserts)+'\n#pragma section(".ylayout",read)\nextern "C" {\n__declspec(allocate(".ylayout")) extern constinit const std::uint64_t yesdaw_layout_cells[] = {\n 0x59444C4159303031ull,1ull,'+str(len(entries))+'ull,\n'+''.join(' static_cast<std::uint64_t>('+row['expression']+'), // '+row['name']+'\n' for row in entries)+'};\n}\n'
write('layout.cpp',translation)
ck('unique purpose cells',len(set(x['name'] for x in entries))==len(entries))
ck('fixed cell extent',len(entries)<=1024 and (len(entries)+3)*8<=16384)
ck('no target inclusion or Data definition','clock_stage.cpp' not in translation and 'Catalogue::reviewed_' not in translation and ' main(' not in translation)
ck('no invented other-object budget','32768' not in translation and '2097152' not in translation)
ck('all original system includes retained',[x for x in translation.splitlines() if x.startswith('#include <')][:-1]==original_includes)
for name in ['layout-types.h','decode_coff.py','launch_compile.py','README.md']:
 b=read(P/name);ck('LF '+name,b'\r' not in b)
launcher=(P/'launch_compile.py').read_text();decoder=(P/'decode_coff.py').read_text();readme=(P/'README.md').read_text()
for term in ['ROOT_SEPARATE_COMPILE_ONLY_GO','ACCEPT_COMPILE_ONLY_PURPOSE_SOURCE','subprocess.DEVNULL','subprocess.CREATE_NO_WINDOW','env=env','child.wait(timeout=5)','SetErrorMode','ADMITTED_HEADER_DEPENDENCIES']:
 ck('launcher guard '+term,term in launcher)
for term in ['NO_EXECUTABLE_CODE','NO_DYNAMIC_INITIALIZER_SYMBOL','EXACT_DATA_SECTION_NO_RELOC','NONWRITABLE_NONEXECUTABLE_DATA','COFF_RANGE','COFF_AUX_EXTENT','LAYOUT_HEADER']:
 ck('decoder guard '+term,term in decoder)
for term in ['UNRUN','No compiler','no link','replica','physical','32KiB','authorization','failed','COFF']:
 ck('scope disclosure '+term,term.lower() in readme.lower())
write('own-checks.json',dict(status='FIRST_COMPLETE_OWN_SOURCE_FILE_INTEGER_CHECK_PASS',checks=checks,count=len(checks),cell_count=len(entries),types=len(types),replicas=len(groups),compiler_header_inputs=len(selected),input_pins=inputs,no_compiler_decoder_launcher_or_target_execution=True))
print(json.dumps(dict(status='OWN_SOURCE_PREPARATION_EXIT0',checks=len(checks),cells=len(entries),types=len(types),replicas=len(groups),profile_inputs=len(selected),whole_proposal='UNRUN_NO_COMPILER_GO')))
