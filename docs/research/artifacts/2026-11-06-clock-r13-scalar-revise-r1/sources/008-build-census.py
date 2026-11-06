"""Fresh complete R13 scalar proposal. Own file/hash/JSON/integer work only.

This does not load any prior generator, PowerShell parser, target helper or SDK.
All prices here are unaccepted source proposals pending independent whole review.
"""
from pathlib import Path
import hashlib
import json
import re

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SRC=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
OP=HERE/'body-operands.json'
if (HERE/'source-pins.json').exists():raise ValueError('Preserve first complete seal')
checks=[]
inputs={}
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
    b=p.read_bytes();inputs[str(p.relative_to(ROOT)).replace('\\','/')]=dict(path=str(p.relative_to(ROOT)).replace('\\','/'),bytes=len(b),sha256=sha(b));return b
def ck(name,a,b=True):
    ok=a==b;checks.append(dict(name=name,passed=ok))
    if not ok:raise ValueError(name)
def write(name,value):
    p=HERE/name
    if p.exists():raise ValueError('Preserve prior output '+name)
    p.write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8',newline='\n')
def sums(d):return sum(d.values())

operand=json.loads(read(OP));m=json.loads(read(SRC/'source-manifest.json'));source=read(SRC/'run-clock.ps1').decode()
ck('actual manifest',sha(read(SRC/'source-manifest.json')),'10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3')
ck('actual runner',sha(source.encode()),'9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788')
for row in m['sources']:
    b=read(SRC/row['path']);ck('own descriptor '+row['path'],(len(b),len(b.splitlines()),sha(b)),(row['bytes'],row['lines'],row['sha256']))
ck('own5 bytes',sum(len(read(SRC/n)) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json']),674220)
ck('false guards',any(m['guards'].values()),False);ck('no Data',m['cost_catalogue']['definition_files'],[])
models=operand['models'];ck('74 models',len(models),74)
for name,row in models.items():
    ck('whole body pin '+name,sha(row['whole_source'].encode()),row['source_sha256'])
    ck('whole body present '+name,row['whole_source'] in source)
    ck('equation supplied '+name,'new_source_equation' in row)

# Re-evaluate every simple local vector/group, retaining even an old worksheet
# disagreement as a visible finding rather than changing the accepted subject.
operand_arithmetic=[]
for name,row in models.items():
    for i,r in enumerate(row['old_worksheet_rows']):
        if 'vector' in r:
            v=sums(r['vector']);declared=r.get('units',r.get('localUnits',v))
            operand_arithmetic.append(dict(helper=name,row=i,kind='vector',sum=v,declared=declared,equal=v==declared,authority='UNACCEPTED_R13_OPERAND'))
fresh_expected={
 ('Get-TaskHash','groups'):289,
 ('Get-TaskHashHex','groups'):14,
 ('Test-TaskHashValue','hex_fixed'):37,
 ('Test-TaskHashValue','hex_body'):36,
 ('Assert-TaskFreshHashIdentity','normal_groups'):25,
 ('Assert-TaskFreshHashIdentity','refusal_groups'):83,
 ('Assert-TaskPins','own4_groups'):52,
 ('Assert-TaskPins','input_groups'):36,
 ('Get-TaskEntries','fixed_groups'):54,
 ('Get-TaskEntries','directory_groups'):155,
 ('Get-TaskEntries','candidate_groups'):196,
 ('Get-TaskEntries','overflow_groups'):126,
 ('Get-TaskEntries','replaced_normal_terminal_groups'):26,
 ('Measure-TaskJson','groups'):104,
 ('Read-TaskDependencyJson','groups'):357,
 ('Read-TaskDependencyJson','finally_groups'):212,
 ('Get-TaskDependencyString','groups'):97,
 ('Read-TaskJson','pair_groups'):662,
 ('Get-TaskSecondaryCodes','groups'):506,
 ('Save-TaskReceipt','groups'):686,
 ('Assert-TaskDependencies','fixed_groups'):345,
 ('Invoke-TaskFencedChild','groups'):104,
 ('Invoke-TaskFencedChild','binary_groups'):72,
}
for (name,key),expected in fresh_expected.items():ck('fresh groups '+name+'/'+key,sums(models[name]['new_source_equation'][key]),expected)
ck('hex fixed32',37+32*(9+36),1477);ck('binary fixed32',39+32*(9+10),647)
ck('Fresh rejection surplus',83-1,82)

# Entire source inventory includes actual caller text at all helper-name sites.
# This is an ordinary textual declaration/call inventory, not a target PS parser.
main_start=source.index('\ntry{\n # Hash literal manifest')
source_inventory=[];caller_inventory=[]
for name,row in models.items():
    code=row['whole_source'];calls=[]
    for other in models:
        pat=re.compile(r'(?<![A-Za-z0-9_-])'+re.escape(other)+r'(?=\s|\()')
        for i,line in enumerate(code.splitlines()):
            visible=line.split('#',1)[0]
            for hit in pat.finditer(visible):
                if i==0 and visible[:hit.start()].rstrip()=='function':continue
                entry=dict(caller=name,callee=other,line=row['source_first_line']+i,source=line)
                calls.append(entry);caller_inventory.append(entry)
    source_inventory.append(dict(name=name,lines=[row['source_first_line'],row['source_last_line']],sha256=row['source_sha256'],bytes=row['source_bytes'],complete_body_owned=True,equation=row['new_source_equation'],callee_sites=calls,old_operands_status='UNACCEPTED_NO_PRICE_TRANSFER'))
for i,line in enumerate(source[main_start:].splitlines()):
    visible=line.split('#',1)[0]
    for name in models:
        if re.search(r'(?<![A-Za-z0-9_-])'+re.escape(name)+r'(?=\s|\()',visible):caller_inventory.append(dict(caller='MAIN_AND_ERROR',callee=name,line=source[:main_start].count('\n')+i+1,source=line))
fields=[]
for i,line in enumerate(source.splitlines(),1):
    for hit in re.finditer(r"\$taskManifest((?:\['[^']+'\])+)",line):fields.append(dict(line=i,written=hit[0],keys=re.findall(r"\['([^']+)'\]",hit[1]),source=line))

I=len(m['inputs']);NS=len(m['closed_directory_inputs']);EG=sum(x['entry_count']+1 for x in m['closed_directory_inputs']);DG=sum(x['entry_count']+2 if x['recursive'] else 1 for x in m['closed_directory_inputs']);files=sum(len(x['files']) for x in m['closed_directory_inputs'])
FC=sum((x['bytes']+65535)//65536 for x in m['inputs']+m['sources'])+(len(read(SRC/'source-manifest.json'))+65535)//65536
F=I+len(m['sources'])+1
ck('profile source', (I,NS,EG,DG,files,F,FC),(1220,31,1123,720,1045,1225,7862))
bad=[]
for row in m['inputs']:
    p=Path(row['path']);p=p if p.is_absolute() else ROOT/p;b=p.read_bytes()
    ck('input pin '+row['path'],(len(b),sha(b)),(row['bytes'],row['sha256']))
ck('input byte sum',sum(r['bytes'] for r in m['inputs']),460232026)
ck('input chunks',sum((r['bytes']+65535)//65536 for r in m['inputs']),7849)

# Whole current main/error rows. Source-count operands are expanded by actual
# current I and current hash-carrier call/projection sponsors, never old totals.
old_main=operand['old_main_rows_operands']['historical_R7_rows']
main_rows=[]
for row in old_main:
    nav=row['lines'];n=row['units'];basis='Current source-homologous explicit main/caller group; unaccepted source-count operand, not inherited price.'
    if nav=='616':n=38*I+16;basis='input index fixed16 +38 per actual admitted input'
    if nav=='618':n=22*I+18;basis='include index fixed18 +22 per copied pair'
    if nav=='680':n=34*I+207;basis='transitive fixed207 +34 per input, own4+manifest separately withinfixed'
    if nav=='601':n=52+9+5;basis='authentication/SDK caller source52 +raw prefix/source-destination9 +byte-profile handoff5'
    if nav=='627':n=10+4;basis='current authenticated predecessor additional literal/hash source argument cells4'
    if nav=='643':n=30;basis='unchanged source-built identity map14 +three scalar indexed arguments/hash caller16; Fresh body separate'
    if nav=='647':n=298;basis='actual control argument/action/witness/projection source298; expected Fresh body separately explicit, not oldIdentity93'
    if nav=='654':n=15;basis='three current source assignments plus actual Build argument1'
    if nav=='656':n=35;basis='Join8+Copy caller9+Fresh/Hash caller15+Console/PE caller3'
    if nav=='658':n=371;basis='current GUI control including its actual ConsolePE refusal; wire hex bodies separate'
    if nav=='672':n=288;basis='four source control projections, observed count direct result and hash caller selected once'
    if nav=='678':n=180;basis='four stage artifact source projections and hash/hex caller operands'
    if nav=='691':n=19;basis='final bundle hash/hex/stdout callers; helper bodies separately composed'
    main_rows.append(dict(navigation_R7_line_label=nav,proposed_current_units=n,basis=basis))
main_rows += [dict(navigation_R7_line_label='NEW_CURRENT',proposed_current_units=1,basis='InitializeTaskPathCatalogue source dispatch once'),dict(navigation_R7_line_label='NEW_RAW_BOUNDARIES',proposed_current_units=10+12+143+5+7+42,basis='two legal caller10;new control outer12;Expect143;script action caller5;ListAdd7;three exact byte-input constructor/caller replacements42')]
MAIN=sum(r['proposed_current_units'] for r in main_rows)
BOOT=sum(r['units'] for r in operand['old_main_rows_operands']['historical_R9_definition_rows'])+2+(74-62)
ck('bootstrap registration recompose',BOOT,48042)

# Exact output schema counts from complete source-built graphs. String extents
# here use the generic admitted512 domain, not an old selected run-root profile.
graphs={name:dict(g) for name,g in operand['old_graph_operands'].items()}
graphs['failure'].update(array_elements=78,array_separators=77)
graphs['marker'].update(array_elements=0,array_separators=0)
def visitor(g):
    return 185*g['maps']+190*g['arrays']+223*g['keys']+31*g.get('map_separators',g['keys']-g['maps'])+212*g['key_units']+21*g['array_elements']+31*g['array_separators']+184*g['strings']+148*g['string_units']+455*g['integers']+115*g['booleans']
for name,g in graphs.items():ck('graph visitor recompute '+name,visitor(g),g['visitor'])
def join(k,u):return 66+87*k+64*u
def set_refusal(n,o,missing,unknown):return 169+90*n+87*o+64*512*(n+o)+join(len(missing),sum(map(len,missing)))+join(len(unknown),sum(map(len,unknown)))
REF_CONTROLS=set_refusal(56,55,['references/native-inverse/r48000-map5.json'],[])+set_refusal(56,57,[],['unlisted.bin'])+set_refusal(56,57,[],[])

# Source-sized disjoint loops; no sleep/timing or quiet-child assumption.
raw=operand['old_raw_derivation_operands'];ck('raw536',sums(raw['grammar_components']),536);ck('raw94',sums(raw['fixed_common']),94);ck('compact51',sums(raw['compaction']),51)
byte_units=len(read(SRC/'source-manifest.json'))+98009+4194304+20+20+21
ck('byte call units',byte_units,4795984)
ref_chunks=sum((x['bytes_cap']+65535)//65536 for x in m['references']);build_chunks=sum((x['bytes_cap']+65535)//65536 for x in m['compiler_final_outputs'])
ck('reference/build chunks',(ref_chunks,build_chunks),(2342,1088))
child_fixed={k:v for r in models['Invoke-TaskChild']['old_worksheet_rows'] for k,v in r.get('ownFixedGroups',{}).items()}
ck('child source fixed sum',sums(child_fixed),1044811)
child_poll=next(r['poll'] for r in models['Invoke-TaskChild']['old_worksheet_rows'] if 'poll' in r)
ck('outer reads per child',2+child_poll['positiveAdmittedBeforeStageUpper'],259)
ck('maximum poll bodies',child_poll['normalBodyMaximum'],7200)

phase_args={
 'fences':[1,1,3,5,8,8], 'children':[0,0,1,2,3,3], 'stage':[0,7,264,523,782,784],
 'references':[0,0,0,1,2,2], 'builds':[0,0,1,1,2,2], 'root_walks':[0,1,1,1,2,2],
 'native_parses':[0,0,0,1,2,2], 'nonfence_hashes':[0,3,10,67,131,136],
 'nonfence_chunks':[0,6,3142,5996,10454,10536], 'copies':[0,1,2,2,2,2],
 'marker_measure':[0,1,1,1,1,1], 'receipt_measure':[0,2,2,3,4,4],
 'failure_measure':[0,1,1,1,1,1], 'bundle_measure':[0,0,0,0,0,5],
 'wire_hex_main':[0,2,4,4,8,13], 'binary_tests':[0,0,1,2,3,3],
 'other_fresh_hex':[0,2,2,2,2,2], 'other_fresh_identity':[0,2,3,4,5,5],
 'fresh_refusals':[2,3,3,3,3,3], 'resolve_other':[1,2,3,3,3,3]}

def compose(p):
    a={k:v[p] for k,v in phase_args.items()};f=a['fences'];child=a['children'];S=a['stage'];r=a['references'];b=a['builds'];rw=a['root_walks'];native=a['native_parses'];created=int(p>0);dep=int(p>=2);producer=int(p>=3);root_success=int(p>=4);bundle=int(p==5);copies=a['copies']
    H=f*F+a['nonfence_hashes'];B=f*FC+a['nonfence_chunks'];W=S+f*NS+r+b+rw+1
    D=S+f*DG+61*r+b+79*rw+1;E=6*S+(3 if S else 0)+f*EG+60*r+4*b+78*rw+1
    CW=2*W+E;NL=H+f*NS+60*r+78*root_success+1+D+2*copies;G=NL+CW+dep
    RS=I*(f+1+bundle)+f*NS+dep*(m['profile']['include_input_count']+1)+a['resolve_other'];EP=G+RS
    first=1294+3 # positive source upper even in prefixes; never actual first-build count
    normal_set_calls=f*NS+r+b+producer+native+root_success
    controls=0 if not native else (3 if native==1 else 14)
    normal_set_N=f*files+56*r+3*b+56*producer+controls+67*root_success
    markers=a['marker_measure'];receipts=a['receipt_measure'];failures=a['failure_measure'];bundlepasses=a['bundle_measure'];measures=markers+receipts+failures+bundlepasses;encodes=markers+receipts+failures+bundle
    doccalls=(1 if markers else 0)+2*receipts+2*failures+2*bundle
    raw_calls=2+dep+3*created
    raw_units=len(read(SRC/'source-manifest.json'))+98009+created*61+dep*4194304
    raw_string_calls=0 if not native else (4 if native==1 else 16);raw_string_units=native*65536
    test_hex=f*F+a['other_fresh_hex'];test_binary=a['binary_tests'];fresh_id=f*(F-1)+a['other_fresh_identity']
    wire_hex=56*r+3*b+a['wire_hex_main']
    profile=dict(**a,H=H,hash_bodies=B,walkers=W,directories=D,rows=E,walker_Canonical=CW,NoLink=NL,Canonical=G,Resolve=RS,EntryPath=EP,firstBuild_upper=first,FileSet_normal_calls=normal_set_calls,FileSet_normal_NM=normal_set_N,actual_module_controls=controls,Measure=measures,Encode=encodes,DocumentWrite=doccalls,raw_byte_calls=raw_calls,raw_byte_units=raw_units,raw_string_calls=raw_string_calls,raw_string_units=raw_string_units,fresh_identity=fresh_id,hex_tests=test_hex,wire_hex=wire_hex,outer_async_reads=259*child)
    C={
     'bootstrapDefinitionsAll74Registrations':BOOT,
     'wholeMainCallerAndCatchSourcePositivePrefix':MAIN,
     'controllerAndSevenFixedAuthenticatedHashes':2136+53*14*512+7*(279+49*64)+45*379,
     'freshSHAOwnBodiesExcludingNoLink':289*H+65*B,
     'allNoLinkOwnBodies':7254*NL,
     'allResolveOwnBodies':(64*512+135)*RS,
     'allWalkerOwnBodiesAndOverflowAlternatives':54*W+155*D+(196+64*512)*E+max(0,126+64*512-26)*W,
     'allStageByteOwnBodies':21*S+73*(6*S+(3 if S else 0)),
     'allPinsOwnCallerBodies':f*(90+50*NS+38*EG+52*4+36*I+NS*(4+I)),
     'normalFileSetSlots':100*normal_set_calls+(83+78+2*64*512)*normal_set_N,
     'twoFileSetRefusalSlotReplacements':2*(201+32862*F+32864*F),
     'threeActualReferenceControlRefusals':producer*REF_CONTROLS,
     'childOwnAllPollsAnd259OuterReads':child*1044811+74*sum([28,6,6][:child])+53*512*(sum([28,6,6][:child])+child*8),
     'fencedWrapperOwnCallers':104*child+int(child>0)+68*max(0,child-1),
     'compilerReplacementHelpers84':int(child>0)*(53*(84*512+84*512+28*(6+6+7))+217*84+18*18+61*38+66+4*86),
     'twoOwnedCopyBodiesAndLiteralConversion':285*copies+70*(0 if not copies else (2 if copies==1 else 514))+int(copies>0),
     'ownedMutationBodies':263*copies,
     'PEHeaderBodies':333*(0 if not dep else 4),
     'ordinaryConsoleChecksAndOneRefusalReplacement':int(child>0)*31+dep*(2*31+62),
     'predecessorDescriptor':2+4988+32824+3448+300+35+38,
     'predecessorMap':3557+62,
     'boundSourceSignedIntegers':4*110,
     'normalWorkReservations':20*(1+created),
     'normalRetainedReservations':16*(1+bundle),
     'normalOneUseAttempt':6,
     'lastLegalPollBoundary':11*created,
     'rootAsciiValidation':53*512+49,
     'compilerArgumentAsciiValidation':int(child>0)*28*(53*512+49),
     'allMeasureOwnAndDefaultReads':104*measures+markers+failures+bundlepasses,
     'allRecursiveVisitors':markers*visitor(graphs['marker'])+receipts*visitor(graphs['receipt'])+failures*visitor(graphs['failure'])+bundlepasses*visitor(graphs['bundle']),
     'receiptOwnWithoutMeasure':receipts*(686-104),
     'WriteJsonOwnWithoutMeasure':59*(failures+bundle)+2*failures+4*failures,
     'allDocumentWriteOwn':doccalls*(64*512+91),
     'allOnceEncodeOwn':38*(markers*(319-1)+receipts*(23635-1)+failures*(262760-1)+bundle*(1048576-1))+encodes*176,
     'bootstrapBoundedRead':198,
     'predecessorAndDependencyReadJsonOwn':662,
     'streamedDependencyReaderAllOwners':dep*357,
     'dependencyExtractionAndStringGetters':dep*(442+64*512+(I+1)*(115+64*512)),
     'nativeResultReaderOwnAnd14Controls':native*21557+38*raw_string_units+controls*(12630+8)+native*15,
     'oneBadControlPrefixReplacement':int(native>0)*(11886+8),
     'buildArtifactOwnWithoutHashHex':b*(47+101*4),
     'referenceOwnWithoutHashHex':r*(54+39*60+101*56),
     'sameReferenceDescriptorIdentities':root_success*(30+51*56),
     'finalBuildDescriptorIdentityBodies':root_success*3*24,
     'twoDescriptorIdentityRefusalReplacements':2*(93-24),
     'allFreshIdentityOwnWithoutTests':fresh_id*25+82*a['fresh_refusals'],
     'allNativeDigestHexAndBinaryTests':1477*test_hex+647*test_binary,
     'wireHexWrapperBodies':14*wire_hex,
     'failureInventoryOwn':failures*(193+64*78),
     'allRawByteGrammarAndCompaction':587*raw_units+138*raw_calls,
     'allRawStringGrammars':512*raw_string_units+106*raw_string_calls,
     'allCanonicalOwn':76*G,
     'allEntryPathOwn':71*EP+(9529-71)*first,
     'initializeCatalogue':209+73*I+133*4+118+136*(56+3+4)+133*6,
    }
    return dict(id='P'+str(p),profile=profile,composition=C,upper=sum(C.values()),note='Positive source upper. Whole-main/first-build/refusal envelopes are explicitly upper, not actual executed counts; no counter reset or later-phase credit.')

phases=[compose(i) for i in range(6)]
full=phases[-1];ck('full hash schedule',(full['profile']['H'],full['profile']['hash_bodies']),(9936,73432))
ck('full measure encode document',(full['profile']['Measure'],full['profile']['Encode'],full['profile']['DocumentWrite']),(11,7,13))
ck('full FileSet',(full['profile']['FileSet_normal_calls'],full['profile']['FileSet_normal_NM']),(256,8615))
ck('full comparisons',(full['profile']['fresh_identity'],full['profile']['hex_tests'],full['profile']['binary_tests'],full['profile']['wire_hex']),(9797,9802,3,131))
ck('all same composition keys',len({tuple(x['composition']) for x in phases}),1)
ck('all nonnegative groups',all(v>=0 for p in phases for v in p['composition'].values()))
ck('six distinct source phase schedules',len(phases),6)
whole=max(p['upper'] for p in phases)

result=dict(schema='R13_COMPLETE_74_HELPER_RUNNER_SCALAR_PROPOSAL_V1',state='SOURCE_ONLY_UNRUN_UNACCEPTED_WHOLE_PROPOSAL_PENDING_INDEPENDENT_NUMERIC_CRITIC',subject_manifest_sha256=sha(read(SRC/'source-manifest.json')),subject_runner_sha256=sha(read(SRC/'run-clock.ps1')),whole_functional_review_sha256='9A429533FF5A1990B75A29B3C899A113CB0156C499535DFF32ED8FD6BA415F0D',scalar_upper_proposal=whole,phases=phases,complete_helper_inventory=source_inventory,actual_written_caller_inventory=caller_inventory,manifest_field_bind_inventory=fields,whole_main_rows=main_rows,bootstrap_definition_rows=operand['old_main_rows_operands']['historical_R9_definition_rows'],graphs=graphs,raw_grammar=raw,old_operand_arithmetic=operand_arithmetic,convention=operand['old_convention_operands'],work_limit_selected=False,combined_U_selected=False,binding_or_execution_GO=False,resource_fit=False,source_prices_status='Every current body/caller equation is an unaccepted proposal; no old total/helper/call price authority transfers.',conditional_premises=['exact accepted current source and provisional manifest/input/namespace set','adopted separate research CLR/CRT/codec opaque CPU convention','current finite source schedule and explicit source-default/conversion/result-write sponsorship','fresh independent whole numeric critique before prices or Data','final canonical functional/proof/SDK union,root/environment,all controlled resource joins,immutable binding and separate root GO remain unclosed'],no_target_execution=True)
write('census.json',result)
write('source-inventory.json',dict(helpers=source_inventory,callers=caller_inventory,manifest_fields=fields,whole_main_rows=main_rows))
write('checks.json',dict(status='PASS_OWN_COMPLETE_FILE_HASH_JSON_INTEGER_CONSISTENCY_NOT_PRICE_ACCEPTANCE',count=len(checks),checks=checks,inputs=list(inputs.values()),no_target_execution=True))
md='# R13 complete runner scalar proposal\n\n**UNACCEPTED SOURCE PROPOSAL: '+format(whole,',')+'. No selected U, Data, fit or GO.**\n\n'
md+='All74 helper bodies, whole main/error callers, manifest field selections, six phase schedules, source defaults/results/cleanup and original controls are inventoried. Previous scalar worksheets remain unaccepted operands; their source groups are re-expanded and every current family is recomposed. No old generator/checker ran.\n\n'
md+='|Phase|Source upper|Fences|Children|Stage calls|Outer reads|\n|---|---:|---:|---:|---:|---:|\n'
for p in phases:md+='|'+p['id']+'|'+format(p['upper'],',')+'|'+str(p['profile']['fences'])+'|'+str(p['profile']['children'])+'|'+str(p['profile']['stage'])+'|'+str(p['profile']['outer_async_reads'])+'|\n'
md+='\n|Final phase family|Proposed upper|\n|---|---:|\n'
for name,value in full['composition'].items():md+='|'+name+'|'+format(value,',')+'|\n'
md+='\nThe stage cap domain remains0/7/264/523/782/784. Each child may emit its full admitted1MiB noise; no compiler/native quiet-output shortcut is assumed. Each child sponsors two initial and257positive requeue reads=259, up to7200poll bodies and260completed-direction bodies. Opaque inner async implementation CPU is not expanded.\n\n'
md+='Fresh digest tests cover8*1225+2hex and3binary cases;8*1224+5 FreshIdentity bodies and8 manifest test callers are distinct. All131 wire hex wrappers are separate. Descriptor Identity remains only for reference/build descriptors. State long[3], streamed dependency constructors/settings/JObject getters, both comma result elements and all three cleanup owners are explicit.\n\n'
md+='Positive source bounds are never relabelled actual counts. Whole-main prefix and first-build/refusal alternatives remain explicitly conservative. No normal return is claimed observed on a throwing path. No physical free, general heap, byte-request fit, native/platform or source/binary qualification follows. The32MiB/840MiB caps,56references,allcontrolIDs/matrices and false guards remain.\n\n'
md+='Read authoring-notes.md and body-operands.json for exact source group sponsorship and unaccepted old operands. Independent whole numeric critique must verify all primitive boundaries, typed conversions, comma elements, main controls, collector lifetimes and schedule coverage before any price/binding authority. All scalar/resource/root/environment/native obligations remain separate.\n'
(HERE/'census.md').write_text(md,encoding='utf-8',newline='\n')
print(json.dumps(dict(status='FIRST_COMPLETE_OWN_CONSISTENCY_PASS_NOT_NUMERIC_ACCEPTANCE',checks=len(checks),phases=[p['upper'] for p in phases],whole=whole,helpers=len(models),families=len(full['composition']),census_sha256=sha((HERE/'census.json').read_bytes()))))
