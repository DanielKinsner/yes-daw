"""Independent file/text/JSON/integer review; never imports or executes a subject.

This is a numeric critique, not a PowerShell parser, execution trace, replacement
census, resource proof, selected work allowance, or permission to run Clock.
"""
from pathlib import Path
import collections
import hashlib
import json
import re

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
AUTHOR = ROOT / 'build-ci/20261106-clock-r13-runner-census-r1'
SRC = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
CHECKS = []
INPUTS = {}

def digest(b):
    return hashlib.sha256(b).hexdigest().upper()

def read(p):
    b = p.read_bytes()
    key = p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix()
    INPUTS[key] = dict(path=key, bytes=len(b), sha256=digest(b))
    return b

def check(name, actual, expected=True):
    CHECKS.append(dict(name=name, actual=actual, expected=expected, passed=actual == expected))

def dump(name, obj):
    p = HERE / name
    if p.exists():
        raise ValueError('Preserve prior output: ' + name)
    p.write_text(json.dumps(obj, indent=2) + '\n', encoding='utf-8', newline='\n')

def load(p):
    return json.loads(read(p))

seals = {
    'census.json': '9DAEFFB03433D0A6D215054462A69D67A2DD79B9A1267F747B70C84DA9EC7024',
    'source-pins.json': '13616A59500136DE3801E5EEF565C582FFD6588322A4AF19158720887634F32D',
    'first-green-whole-source-freeze.json': 'B64ACA5B72E5BE414C5ECE0714DB3BF7C5BC75FDC240E81130ADE2CA434DA67A',
}
for name, sha in seals.items():
    check('external subject seal ' + name, digest(read(AUTHOR / name)), sha)
pins = load(AUTHOR / 'source-pins.json')
for group in ('own_files', 'subject', 'lineage'):
    for row in pins[group]:
        b = read(ROOT / row['path'])
        check('subject/lineage ' + row['path'], [len(b), digest(b)], [row['bytes'], row['sha256']])

conventions = {
 'build-ci/20261105-clock-oracle-separate-research-contract/contract.md': '344408DAEC2BD355D44B0B6DA266BF00119B09DF5028B70524699E8E2CD36010',
 'build-ci/20261105-clock-oracle-research-leaf-census-conventions.md': 'DE33D91712372C2FE887A02FB137AA9A96C8C3BB93CB0D88BF57AA32DBEAE564',
 'build-ci/20261105-clock-oracle-research-bulk-leaf-clarification.md': '268597D1F066779CE76DACFDBBC384FC452DC6B9AD319419F52DE597242BB3E8',
 'build-ci/20261106-research-getter-destination-clarifier-r1.md': '1F47F55CFC659159E12DACB0C3A4612F081E507617EFE7318168D570B1E623B3',
 'build-ci/20261106-research-nullable-argument-clarifier-r1.md': 'F019F6C9481E6359667AE1252BC2639B03756D686E0007C89C5FA8596C9E5842',
 'build-ci/20261106-research-static-property-clarifier-r1.md': 'FA46DBDCD261641BD6830AF14D7814FC51CDE5167478C723701B92FA33D1C337',
}
for path, sha in conventions.items():
    check('research convention ' + path, digest(read(ROOT / path)), sha)

census = load(AUTHOR / 'census.json')
operands = load(AUTHOR / 'body-operands.json')
manifest = load(SRC / 'source-manifest.json')
source_bytes = read(SRC / 'run-clock.ps1')
source = source_bytes.decode('utf-8')
lines = source.splitlines(keepends=True)
models = operands['models']
decls = [(i, re.match(r'^function ([A-Za-z0-9-]+)', line).group(1))
         for i, line in enumerate(lines, 1) if re.match(r'^function ([A-Za-z0-9-]+)', line)]
check('all actual function declarations', len(decls), 74)
check('whole declared function-name set', sorted(n for _, n in decls), sorted(models))
check('all exact source guards remain false', any(manifest['guards'].values()), False)
check('no literal catalogue definitions', manifest['cost_catalogue']['definition_files'], [])
check('frozen source LF only', b'\r' in source_bytes, False)
check('exact runner lines', len(lines), 1038)

covered = []
for name, row in models.items():
    actual = ''.join(lines[row['source_first_line']-1:row['source_last_line']])
    check('whole current body text ' + name, actual, row['whole_source'])
    check('whole current body hash ' + name, digest(actual.encode()), row['source_sha256'])
    check('whole current body length ' + name, len(actual.encode()), row['source_bytes'])
    check('one declared model ' + name, sum(n == name for _, n in decls), 1)
    covered.append(dict(name=name, first_line=row['source_first_line'], last_line=row['source_last_line'],
                        sha256=digest(actual.encode()), authored_formula=row['new_source_equation']['formula'],
                        disposition='WHOLE_SOURCE_AND_CALLER_SPONSORSHIP_REVIEWED_NO_INDIVIDUAL_PRICE_ACCEPTANCE'))

# Verify the actual admitted file set; no target programs, parser, or author code.
for row in manifest['inputs']:
    path = Path(row['path'])
    if not path.is_absolute():
        path = ROOT / path
    b = read(path)
    check('admitted input ' + row['path'], [len(b), digest(b)], [row['bytes'], row['sha256']])
for row in manifest['sources']:
    b = read(SRC / row['path'])
    check('own source descriptor ' + row['path'], [len(b), len(b.splitlines()), digest(b)],
          [row['bytes'], row['lines'], row['sha256']])

I = len(manifest['inputs'])
NS = len(manifest['closed_directory_inputs'])
EG = sum(row['entry_count'] + 1 for row in manifest['closed_directory_inputs'])
DG = sum(row['entry_count'] + 2 if row['recursive'] else 1 for row in manifest['closed_directory_inputs'])
FILES = sum(len(row['files']) for row in manifest['closed_directory_inputs'])
F = I + len(manifest['sources']) + 1
FC = sum((row['bytes'] + 65535)//65536 for row in manifest['inputs'] + manifest['sources']) + (len(read(SRC/'source-manifest.json')) + 65535)//65536
check('manifest source-sized profile', [I, NS, EG, DG, FILES, F, FC], [1220,31,1123,720,1045,1225,7862])
check('input bytes/chunks', [sum(r['bytes'] for r in manifest['inputs']), sum((r['bytes']+65535)//65536 for r in manifest['inputs'])], [460232026,7849])
check('own five bytes', sum(len(read(SRC/n)) for n in ('run-clock.ps1','clock_stage.cpp','source-costs.h','README.md','source-manifest.json')), 674220)
check('bootstrap profile node/key visits', 'Assert-TaskRawJsonBytes $taskBootstrap 16104 512 4' in source)

# Independent occurrence inventory: retain every occurrence on a main line.
# This is a bounded textual name inventory, not a language parser or execution.
main_line = next(i for i,l in enumerate(lines, 1) if l.startswith('try{') and i>900)
owners = {}
for name,row in models.items():
    for i in range(row['source_first_line'],row['source_last_line']+1):
        owners[i]=name
calls = []
for i,line in enumerate(lines,1):
    caller = owners.get(i, 'MAIN_AND_ERROR' if i>=main_line else 'DEFINITIONS')
    if caller=='DEFINITIONS':
        continue
    visible=line.split('#',1)[0]
    for name in models:
        for hit in re.finditer(r'(?<![A-Za-z0-9_-])'+re.escape(name)+r'(?=\s|\()',visible):
            if visible[:hit.start()].strip()=='function':
                continue
            calls.append(dict(caller=caller,callee=name,line=i,column=hit.start()+1,source=line.rstrip('\n')))
claimed=collections.Counter((r['caller'],r['callee'],r['line']) for r in census['actual_written_caller_inventory'])
actual=collections.Counter((r['caller'],r['callee'],r['line']) for r in calls)
call_gaps=[dict(caller=k[0],callee=k[1],line=k[2],actual=v,recorded=claimed[k]) for k,v in sorted(actual.items()) if v!=claimed[k]]
check('actual textual caller occurrences',len(calls),320)
check('author textual caller occurrences',sum(claimed.values()),315)
check('four multiplicity locations',len(call_gaps),4)
check('five omitted occurrence rows',sum(r['actual']-r['recorded'] for r in call_gaps),5)

static_sites=[dict(line=i,source=line.rstrip('\n'),occurrences=len(re.findall(r'\[StringComparer\]::Ordinal(?:IgnoreCase)?',line)))
              for i,line in enumerate(lines,1) if '[StringComparer]::Ordinal' in line]
check('all static StringComparer getter occurrences',sum(r['occurrences'] for r in static_sites),13)
casts=[dict(line=i,written=h.group(0),source=line.rstrip('\n')) for i,line in enumerate(lines,1)
       for h in re.finditer(r'\[(?:int|long|uint64|uint16|byte|char|string|byte\[\]|Collections\.Generic\.IEnumerable\[string\])\](?!\s*::)',line)
       if not line.lstrip().startswith('#')]

# Scalar ledgers below are independently ordered, explicit role counts. They do
# not assert that a collection of local corrections is a new whole upper.
ledgers=[]
def ledger(id,helper,line,group,old,terms,expected,note=''):
    value=sum(terms.values())
    check('independent ledger '+id,value,expected)
    check('finding differs '+id,value!=old)
    ledgers.append(dict(id=id,helper=helper,line=line,group=group,frozen=old,terms=terms,
                        independently_derived_local=value,note=note,whole_replacement_accepted=False))

ledger('L01','Get-TaskHashHex',157,'Item2/cast/argument',7,dict(receiver_R=1,receiver_W=1,getter_D_E_T=3,cast_type_R=1,cast_O=1,fused_argument_W=1),8)
ledger('L02','Assert-TaskFreshHashIdentity',177,'Test caller',8,dict(getter_without_result=5,cast_type_R=1,cast_O=1,expected_R=1,Test_D=1,bool_operand_W=1),10,'Test parameter W2 remain solely in its callee entry3; no duplicate digest destination.')
ledger('L03','Test-TaskHashValue',164,'byte local',6,dict(indexed_R_O=4,cast_type_R=1,cast_O=1,local_W=1),7)
ledger('L04','Assert-TaskFreshHashIdentity',178,'expected projection maximum',12,dict(kind_predicate=4,Convert_type_R=1,expected_R=1,cast_type_R=1,cast_O=1,argument_W=1,D_E_T=3,local_W=1),13)
ledger('L05','Assert-TaskFreshHashIdentity',179,'observed projection',17,dict(caller_argument_R=1,own_D=1,local_W=1,GetHashHex=15),18)
ledger('L06','Get-TaskDependencyString',721,'typed guard',32,dict(runtime_caller_R_D_W=3,runtime_body=16,JValue_type_eq=2,and_O=1,get_Type_with_operand_W=6,String_enum=3,eq_O=1,B=1),33)
ledger('L07','Read-TaskDependencyJson',707,'two scalar setters',14,dict(two_named_receivers_R=2,two_literal_R=2,two_receiver_W=2,two_parameter_W=2,two_D_E_T=6,Int32_to_nullable_O=1),15)
ledger('L08','Assert-TaskPins',823,'Own4 guard',19,dict(first_path_index=4,OwnNames_R=1,cnotin_O=1,or_O=1,Add_receiver_R_W=2,Add_path_index=4,Add_parameter_W=1,Add_D_E_T=3,Add_bool_operand_W=1,not_O=1,B=1),20)
ledger('L09','Assert-TaskPins',824,'input byte sum',8,dict(sum_R=1,indexed_bytes=4,cast_type_R=1,cast_O=1,add_O=1,sum_W=1),9)
ledger('L10','Add-TaskPathCatalogueEntry',75,'drive guard',17,dict(Length_with_operand=6,literal_3_R=1,ge_O=1,indexed_Name=4,char_type_R=1,literal_58_R=1,cast_O=1,ceq_O=1,and_O=1,B=1),18)
ledger('L11','Initialize-TaskPathCatalogue',84,'maximum',36,dict(input_index=4,input_Count_tail=5,long_type_R=1,cast_O=1,OwnNames_Count=6,literal_1_R=1,two_profile_cells=14,four_adds_O=4,local_W=1),37)
ledger('L12','Join-TaskBoundedNames',488,'direct Join/cast/return',10,dict(string_type_R=1,separator_R=1,IEnumerable_type_R=1,Names_R=1,cast_O=1,two_argument_W=2,D_E_T=3,function_T=1),11)
ledger('L13','Get-TaskEntries',494,'depth local',6,dict(indexed_cell=4,int_type_R=1,cast_O=1,local_W=1),7)
ledger('L14','Get-TaskEntries',495,'candidate local',7,dict(Current_getter_without_destination=5,string_type_R=1,cast_O=1,local_W=1),8)
ledger('L15','Read-TaskBoundedBytes',693,'byte array construction',8,dict(array_type_R=1,int_type_R=1,length_R=1,cast_O=1,parameter_W=1,D_E_T=3,result_W=1),9)
ledger('L16','Encode-TaskJson',744,'scan per body',38,dict(three_indexed_reads=12,three_cast_type_R=3,three_cast_O=3,three_literals_R=3,three_compares_O=3,two_or_O=2,B=1,loop_framework=14),41)
ledger('L17','Assert-TaskPins',814,'whole fixed source',90,dict(entry_E=1,three_indexed_foreach_fixed=21,manifest_Hash_caller=4,manifest_Test_guard=12,static_comparer_and_set_ctor=10,sources_OwnNames_count_guard=17,sum_init=2,final_profile_guard=28,return_T=1),96,'Per-Own4, per-input and namespace rows are separate; this is not the narrow old90+2 correction.')
ledger('L18','Assert-TaskExactKeys',393,'set constructor',9,dict(static_getter_Type_R_D_E_T=4,constructor_argument_W=1,constructor_Type_R_D_E_T=4,result_W=1),10,'The same constructor/getter boundary applies to both FileSet constructors; all13 written StringComparer occurrences require source sponsor reconciliation.')
ledger('L19','Assert-TaskRawJsonBytes',650,'literal group',37,dict(original_group=36,int_type_R=1,cast_O=1),38,'This exposes a missing component atom. A new global coefficient requires whole grammar regeneration, not automatic537/588 selection.')
ledger('L20','Initialize-TaskPathCatalogue',87,'indexed foreach fixed',6,dict(base_R=1,key_R=1,index_O=1,selected_cell_R=1,setup_S=1,terminal_K=1,exit_K=1),7,'Also applies to references/compiler/control/stage indexed foreach fixed groups at90..93.')
ledger('L21','Initialize-TaskPathCatalogue',88,'OwnNames foreach fixed',3,dict(enumerable_R=1,setup_S=1,terminal_K=1,exit_K=1),4)
ledger('L22','Get-TaskCatalogueEntryPath',123,'two-value array foreach fixed',7,dict(array_S=1,two_value_R=2,two_element_W=2,foreach_S=1,terminal_K=1,exit_K=1),8)
check('GetHashHex local total',3+8+4,15)
check('Test hex whole local',37+32*(9+37),1509)
check('Fresh normal local',5+1+6+3+10+1+1,27)
check('Fresh rejection replacement group',13+18+20+2+32,85)
check('Fresh rejection replacement surplus',85-1,84)
check('Dependency string fixed',2+2+33+6+53+2,98)
check('ReadDependency whole local',357+1,358)
check('Own4 row local',5+20+11+5+5+7,53)
check('Input row local',5+6+5+5+7+9,37)
check('AssertString ASCII unaffected independent body',14+7+10+11+11,53)
check('GetRuntimeType independently expanded',2+4+10,16)
check('AssertIntegral max independently expanded',3+19+32+6+1,61)
check('digit negative independently expanded',19+56+18*13,309)
check('digit unsigned independently expanded',20+59+19*12,307)

# Four source-shaped output topologies: structural, not decoded output samples.
rootkeys=['schema','stage','status','source_manifest_sha256','predecessor_bundle','artifacts','transitive_inputs','admitted_retained_excluding_bundle','bundle_bytes','admitted_retained','reserved_source_work','runner_used','compiler_operation','compiler_temporary_peak_disk_qualified','compiler_CPU_internal_heap_RSS_qualified','old_broad_compiler_output_bound_fulfilled','vendor','quality','context','Ready','renderer']
refkeys=['path','relative_path','bytes','sha256','origin_bundle_sha256']
desckeys=['path','bytes','sha256','origin_bundle_sha256']
predkeys=['path','sha256','artifacts','transitive','complete_union_paths','complete_union_bytes']
bundle=dict(fields=21,maps=1+1+67+F,arrays=2,keys=21+6+56*5+11*4+F*4,
            strings=4+2+56*4+11*3+F*3,integers=6+4+67+F,booleans=8,
            key_units=sum(map(len,rootkeys))+sum(map(len,predkeys))+56*sum(map(len,refkeys))+(11+F)*sum(map(len,desckeys)),
            array_elements=67+F)
for k,v in bundle.items():
    check('source-built bundle topology '+k,v,census['graphs']['bundle'][k])
markerkeys=['stage','manifest','work','retained_reservation','attempts','opaque_compiler_peak_disk_qualified']
check('marker key units',sum(map(len,markerkeys)),80)
check('marker maps/keys', [1,len(markerkeys)], [census['graphs']['marker']['maps'],census['graphs']['marker']['keys']])
check('failure graph maps',1+78,census['graphs']['failure']['maps'])
check('failure graph keys',7+78*5,census['graphs']['failure']['keys'])

def visitor(g):
    return (185*g['maps']+190*g['arrays']+223*g['keys']+31*g.get('map_separators',g['keys']-g['maps'])+
            212*g['key_units']+21*g['array_elements']+31*g['array_separators']+184*g['strings']+
            148*g['string_units']+455*g['integers']+115*g['booleans'])
for name,g in census['graphs'].items():
    check('all graph proposal arithmetic '+name,visitor(g),g['visitor'])

# Independently recomposed proposed schedules. Passing this only says the frozen
# proposal adds up: erroneous primitive sponsors are not repaired by arithmetic.
args=dict(f=[1,1,3,5,8,8],c=[0,0,1,2,3,3],s=[0,7,264,523,782,784],r=[0,0,0,1,2,2],b=[0,0,1,1,2,2],rw=[0,1,1,1,2,2],n=[0,0,0,1,2,2],h=[0,3,10,67,131,136],chunks=[0,6,3142,5996,10454,10536],cp=[0,1,2,2,2,2],marker=[0,1,1,1,1,1],receipt=[0,2,2,3,4,4],failure=[0,1,1,1,1,1],bundlepass=[0,0,0,0,0,5],hexmain=[0,2,4,4,8,13],binary=[0,0,1,2,3,3],freshhex=[0,2,2,2,2,2],freshid=[0,2,3,4,5,5],refusals=[2,3,3,3,3,3],resolve=[1,2,3,3,3,3])
def joined(k,u): return 66+87*k+64*u
def refusal(n,o,mi,un):return 169+90*n+87*o+64*512*(n+o)+joined(len(mi),sum(map(len,mi)))+joined(len(un),sum(map(len,un)))
refs=refusal(56,55,['references/native-inverse/r48000-map5.json'],[])+refusal(56,57,[],['unlisted.bin'])+refusal(56,57,[],[])
recomposed=[]
for i,p in enumerate(census['phases']):
    a={k:v[i] for k,v in args.items()};f,c,s,r,b,rw,n,cp=(a[k] for k in ('f','c','s','r','b','rw','n','cp'))
    dep=int(i>=2);created=int(i>0);prod=int(i>=3);root=int(i>=4);out=int(i==5)
    H=f*F+a['h'];B=f*FC+a['chunks'];W=s+f*NS+r+b+rw+1;D=s+f*DG+61*r+b+79*rw+1
    E=6*s+(3 if s else 0)+f*EG+60*r+4*b+78*rw+1;CW=2*W+E
    NL=H+f*NS+60*r+78*root+1+D+2*cp;G=NL+CW+dep
    RS=I*(f+1+out)+f*NS+dep*(1221+1)+a['resolve'];EP=G+RS
    sets=f*NS+r+b+prod+n+root;ctrl=0 if not n else (3 if n==1 else 14)
    elems=f*FILES+56*r+3*b+56*prod+ctrl+67*root
    mk,rc,fl,bp=(a[k] for k in ('marker','receipt','failure','bundlepass'))
    ms=mk+rc+fl+bp;enc=mk+rc+fl+out;docs=mk+2*rc+2*fl+2*out
    bytecalls=2+dep+3*created;byteunits=503610+98009+61*created+4194304*dep
    strcalls=0 if not n else (4 if n==1 else 16);strunits=n*65536
    hx=f*F+a['freshhex'];fi=f*(F-1)+a['freshid'];wire=56*r+3*b+a['hexmain'];argc=sum([28,6,6][:c])
    profiles={'H':H,'hash_bodies':B,'walkers':W,'directories':D,'rows':E,'walker_Canonical':CW,'NoLink':NL,'Canonical':G,'Resolve':RS,'EntryPath':EP,'FileSet_normal_calls':sets,'FileSet_normal_NM':elems,'Measure':ms,'Encode':enc,'DocumentWrite':docs,'raw_byte_calls':bytecalls,'raw_byte_units':byteunits,'raw_string_calls':strcalls,'raw_string_units':strunits,'fresh_identity':fi,'hex_tests':hx,'wire_hex':wire,'outer_async_reads':259*c}
    for key,value in profiles.items(): check(p['id']+' source profile '+key,value,p['profile'][key])
    values=[48042,129215,2136+53*14*512+7*(279+49*64)+45*379,
        289*H+65*B,7254*NL,(64*512+135)*RS,54*W+155*D+(196+64*512)*E+(126+64*512-26)*W,
        21*s+73*(6*s+(3 if s else 0)),f*(90+50*NS+38*EG+52*4+36*I+NS*(4+I)),
        100*sets+(83+78+2*64*512)*elems,2*(201+32862*F+32864*F),prod*refs,
        1044811*c+74*argc+53*512*(argc+8*c),104*c+int(c>0)+68*max(0,c-1),
        int(c>0)*(53*(84*512+84*512+28*19)+217*84+18*18+61*38+66+4*86),
        285*cp+70*(0 if cp==0 else (2 if cp==1 else 514))+int(cp>0),263*cp,333*(4 if dep else 0),
        int(c>0)*31+dep*(2*31+62),41635,3619,440,20*(1+created),16*(1+out),6,11*created,
        53*512+49,int(c>0)*28*(53*512+49),104*ms+mk+fl+bp,
        mk*visitor(census['graphs']['marker'])+rc*visitor(census['graphs']['receipt'])+fl*visitor(census['graphs']['failure'])+bp*visitor(census['graphs']['bundle']),
        rc*(686-104),59*(fl+out)+6*fl,docs*(64*512+91),38*(mk*318+rc*23634+fl*262759+out*1048575)+176*enc,
        198,662,357*dep,dep*(442+64*512+(I+1)*(115+64*512)),n*21557+38*strunits+ctrl*12638+n*15,
        int(n>0)*11894,b*(47+101*4),r*(54+39*60+101*56),root*(30+51*56),root*72,138,
        25*fi+82*a['refusals'],1477*hx+647*a['binary'],14*wire,fl*(193+64*78),587*byteunits+138*bytecalls,
        512*strunits+106*strcalls,76*G,71*EP+(9529-71)*1297,209+73*I+133*4+118+136*63+133*6]
    check(p['id']+' all54 arity',len(values),54)
    for (key,claimed_value),value in zip(p['composition'].items(),values):
        check(p['id']+' family arithmetic '+key,value,claimed_value)
    check(p['id']+' independently added proposal',sum(values),p['upper'])
    recomposed.append(dict(id=p['id'],proposal_sum=sum(values),source_profiles=profiles,prices_accepted=False))
check('six-phase maximum proposal',max(p['proposal_sum'] for p in recomposed),5536151256)
check('source main row arithmetic',sum(r['proposed_current_units'] for r in census['whole_main_rows']),129215)

schedule=dict(frozen_counts=[2,3,3,3,3,3],source_possible_joint_route=[
 dict(line=985,region='previous caught predecessor negative control',Fresh_refusals=1),
 dict(line=276,region='native fenced pre-Pins sees same-length changed admitted input',Fresh_refusals=1),
 dict(line=279,region='mandatory finally Pins rechecks same changed admitted input',Fresh_refusals=1),
 dict(line=280,region='same finally binary hash checks independently changed executable',Fresh_refusals=1)],
 needed_joined_refusals=4,first_native_route_phase='P3',
 note='A source-domain possibility, not an executed trace. Successful earlier controls are followed by two independent same-length file changes. Pre-Pins refuses before launch; finally still reaches both checks. No quiet-output law, target execution, foreign normal-slot slack, or extra helper calls are inferred. Whole successor must rebuild refusal scheduling or prove an explicit source/contract exclusion.')

coverage_notes={
 'Spend-Task':'Normal/refusal and literal Int32-to-Long caller conversion; no byte-based CPU fee.',
 'Get-TaskHash':'All input/copy/binary/manifest identities and chunk schedules; Tuple2 constructor/return, cleanup and canonical excluded bodies.',
 'Invoke-TaskChild':'All7200polls, two directions,259outer calls, arbitrary fragmentation opaque primitive CPU; every own cleanup/CTS/quarantine outcome. No258 transfer.',
 'Invoke-TaskFencedChild':'Pre-Pins, required finally Pins, optional binary check and secondary outcomes; joint refusal finding applies.',
 'Initialize-TaskController':'Actual14 arguments,7authenticated fixed hashes, module/codec/getter chains, primary/secondary finally. No runtime/profile qualification inferred.',
 'Assert-TaskString':'Independent ASCII body53; general surrogate branch64 is not automatically changed by Encode finding.',
 'Get-TaskIntegerDigits':'Independent signed309/unsigned307 with exact18/19 thresholds and default false literal.',
 'Visit-TaskJsonValue':'All null/string/bool/integral/map/array/unsupported branches, caller depth/state/recursive parameters and separators; graph arithmetic reproduced without certifying uncorrected child prices.',
 'Get-TaskEntries':'Whole fixed/directory/candidate/overflow/cleanup paths; explicit type findings in directory/candidate/overflow; no normal terminal added to throwing branch.',
 'Assert-TaskRawJson':'Whole original grammar classes, all integer/range/UTF16/control/refusal paths; inherited512 coefficient is not separately certified.',
 'Assert-TaskRawJsonBytes':'Whole grammar/UTF8 validation/compaction/failure path; explicit literal cast omission; original bytes/work limits unchanged.',
 'Read-TaskJson':'Bootstrap separate; predecessor/dependency pair662 structure, default and ExpectedHash/Compact branch/caller comma. Whole regeneration required after leaf changes.',
 'Read-TaskChildResult':'Both bounded segments,33slices maximum, raw+exact keys+operand normalization+summary+cleanup; static comparer constructor and transitive exact-key findings apply.',
 'Assert-TaskDependencies':'JObject root/Data/Source/Includes, two optional arrays,1222string calls max, explicit SDK result destinations and new nullable lift propagate.',
 'Save-TaskFailureInventory':'Caught failed root walk continues to codec; footer remains possible; explicit casts, tuple projections, diagnostic references and write/measure callers retained.',
}
flagged=collections.defaultdict(list)
for row in ledgers: flagged[row['helper']].append(row['id'])
for row in covered:
    row['findings']=flagged[row['name']]
    row['whole_review_note']=coverage_notes.get(row['name'],'Full current body/caller/default/error region checked against declared primitive roles, global schedule and explicit boundary inventory. This REVISE gives no individual or inherited price authority.')

dump('ordered-ledgers.json',dict(schema='INDEPENDENT_R13_ORDERED_LOCAL_LEDGER_V1',ledgers=ledgers,all74coverage=covered,
                               explicit_type_text_inventory=casts,static_property_sites=static_sites))
dump('caller-inventory.json',dict(schema='INDEPENDENT_R13_CALL_OCCURRENCE_INVENTORY_V1',occurrences=calls,
                               author_count=315,actual_count=320,multiplicity_gaps=call_gaps))
dump('phase-arithmetic.json',dict(schema='INDEPENDENT_RECOMPOSITION_OF_UNACCEPTED_PROPOSAL_V1',phases=recomposed,
                               fresh_refusal_schedule=schedule,formula_authority='Arithmetic only; all54factors remain unaccepted pending complete successor.'))
dump('checks.json',dict(schema='INDEPENDENT_FILE_TEXT_JSON_INTEGER_CHECKS_V1',checks=CHECKS,
                       count=len(CHECKS),failed=[r['name'] for r in CHECKS if not r['passed']],
                       scope='Self-asserting source/data checks, not target/runtime/price/resource certification.'))
dump('verified-inputs.json',dict(schema='INDEPENDENT_REVIEW_READ_BYTE_PINS_V1',files=list(INPUTS.values())))
print(json.dumps(dict(checks=len(CHECKS),failed=[r['name'] for r in CHECKS if not r['passed']],helpers=len(models),callers=len(calls),ledgers=len(ledgers),proposal_recomposed=5536151256)))
if any(not r['passed'] for r in CHECKS):
    raise SystemExit(1)
