"""Independent file/text/integer review; never loads or executes target PS/C++."""
from pathlib import Path
import hashlib
import json
import ntpath
import os
import re

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
CENSUS = ROOT / 'build-ci/20261105-clock-oracle-r10-runner-census'
SOURCE = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r10-source-prototype'
OLD_SOURCE = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r9-source-prototype'
checks = []
identities = []

def check(name, actual, expected):
    checks.append(dict(name=name, actual=actual, expected=expected, passed=actual == expected))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))

def pin(record, label):
    path = Path(record['path'])
    if not path.is_absolute():
        path = ROOT / path
    actual = dict(path=str(path), bytes=path.stat().st_size, sha256=sha(path))
    identities.append(actual)
    check(label + ' hash', actual['sha256'], record['sha256'])
    if 'bytes' in record:
        check(label + ' length', actual['bytes'], record['bytes'])

for name, expected in [
    ('census.md', 'AA708737759424772AA141FA3957989C95BB66F78BFD1245CE74DB7CC74E40EF'),
    ('census.json', 'D8B9FDFC1FE16FAC28D89ED80D3395715114F46CE49D572C23BE79CB10DEFBD7'),
    ('source-pins.json', '10BABDD0C7348784BD0DD410EFC6D79611339CD5A797055C199CD1C49E6374EA')]:
    check('sealed census ' + name, sha(CENSUS / name), expected)
c = read_json(CENSUS / 'census.json')
pins = read_json(CENSUS / 'source-pins.json')
m = read_json(SOURCE / 'source-manifest.json')
for family in ['subject_five', 'review_contract_lineage', 'packet_outputs', 'evidence']:
    for record in pins[family]:
        pin(record, family + ':' + record['path'])
for record in m['inputs']:
    pin(record, 'input:' + record['path'])
check('input count', len(m['inputs']), 1220)
check('input byte sum', sum(r['bytes'] for r in m['inputs']), 460232026)
check('input chunk sum', sum((r['bytes'] + 65535) // 65536 for r in m['inputs']), 7849)

namespaces = []
for spec in m['closed_directory_inputs']:
    base = Path(spec['path'])
    if not base.is_absolute():
        base = ROOT / base
    rows, files, reparse = [], [], []
    def walk(folder):
        with os.scandir(folder) as entries:
            for entry in entries:
                relative = Path(entry.path).relative_to(base).as_posix()
                rows.append(relative)
                status = entry.stat(follow_symlinks=False)
                if entry.is_symlink() or getattr(status, 'st_file_attributes', 0) & 0x400:
                    reparse.append(relative)
                elif entry.is_dir(follow_symlinks=False):
                    if spec['recursive']:
                        walk(Path(entry.path))
                else:
                    files.append(relative)
    walk(base)
    check('namespace entries:' + spec['path'], len(rows), spec['entry_count'])
    check('namespace exact files:' + spec['path'], sorted(files), sorted(spec['files']))
    check('namespace no reparse:' + spec['path'], reparse, [])
    namespaces.append(dict(path=spec['path'], entries=len(rows), files=len(files)))
check('namespace totals', [len(namespaces), sum(r['entries'] for r in namespaces), sum(r['files'] for r in namespaces)], [31, 1092, 1045])

# A textual declaration/span inventory, not PowerShell tokenization or AST.
def functions(path):
    text = path.read_text(encoding='utf-8')
    declarations = list(re.finditer(r'^function ([A-Za-z-]+)', text, re.M))
    main = text.index('\ntry{\n # Hash literal manifest') + 1
    bodies, locations = {}, {}
    for index, declaration in enumerate(declarations):
        end = declarations[index + 1].start() if index + 1 < len(declarations) else main
        bodies[declaration[1]] = text[declaration.start():end]
        last_line = text.count('\n', 0, end)
        if index + 1 == len(declarations) and text[:end].endswith('\n\n'):
            last_line -= 1  # Census excludes the empty separator before main.
        locations[declaration[1]] = [text.count('\n', 0, declaration.start()) + 1, last_line]
    return text, bodies, locations

text, bodies, locations = functions(SOURCE / 'run-clock.ps1')
old_text, old_bodies, old_locations = functions(OLD_SOURCE / 'run-clock.ps1')
check('source helper declarations', len(bodies), 68)
check('census helper rows', len(c['helper_rows']), 78)
check('complete helper name coverage', sorted(bodies), sorted(set(r['name'] for r in c['helper_rows'])))
for row in c['helper_rows']:
    check('helper source span:' + row['name'] + ':' + row.get('path', ''), locations[row['name']], row['lines'])
    if 'vector' in row:
        check('helper vector:' + row['name'] + ':' + row.get('path', ''), sum(row['vector'].values()), row.get('localUnits', row['units']))
    if 'fixedVector' in row:
        check('helper fixed vector:' + row['name'] + ':' + row.get('path', ''), sum(row['fixedVector'].values()), row.get('fixedUnits', row.get('fixedLocalUnits')))
    if 'fixedVector' in row and 'loop' in row:
        loop = row['loop']
        check('integer digit loop:' + row['path'], sum(loop['bodyVector'].values()), loop['bodyUnits'])
        check('integer digit total:' + row['path'], row['fixedUnits'] + sum(r['count'] * r['units'] for r in row['calls']) + loop['bodies'] * loop['bodyUnits'], row['units'])
same = sorted(name for name in bodies.keys() & old_bodies.keys() if bodies[name] == old_bodies[name])
changed = sorted(name for name in bodies.keys() & old_bodies.keys() if bodies[name] != old_bodies[name])
added = sorted(bodies.keys() - old_bodies.keys())
removed = sorted(old_bodies.keys() - bodies.keys())
check('changed existing helper bodies', changed, ['Assert-NoLink', 'Assert-TaskDependencies', 'Get-TaskEntries', 'Read-TaskJson', 'Resolve-TaskInput'])
check('new helper declarations', added, ['Add-TaskPathCatalogueEntry', 'Assert-TaskRawJsonBytes', 'Get-TaskCanonicalPath', 'Get-TaskCatalogueEntryPath', 'Get-TaskCatalogueString', 'Initialize-TaskPathCatalogue', 'Test-TaskPathCatalogueContext'])
check('removed shape helper', removed, ['Assert-TaskDependencyJsonShape'])
for name, values in c['new_family_groups'].items():
    check('new group sum:' + name, sum(values.values()), c['self_checks']['new_groups_sums'][name])
check('Entry parent body', 8 + 44 + 5 + 3 + 4 + 12 + 67, 143)
check('Entry full build', 9+7+14+8+12+7+12+44+12+8+65*143+5+7+2*36+15+2, 9529)
check('catalogue initialization', 209 + 73*1220 + 133*4 + 118 + 136*(56+3+4) + 133*6, 99285)
check('byte lexer grammar coefficient', sum(c['byte_lexical_derivation']['grammar_components'].values()), 536)
check('byte lexer fixed', sum(c['byte_lexical_derivation']['fixed_common'].values()), 94)
check('compaction per byte', sum(c['byte_lexical_derivation']['compaction'].values()), 51)
check('byte lexing total', 587*(497616+98009+4194304+20+20+21)+138*6, 2811724958)
check('string lexing total', 512*(2*65536)+106*16, 67110560)
check('ReadJson own pair', 2*311+24+19+17+6, 688)
check('main historical 88 rows', len(c['main_and_bootstrap']['historical_R7_rows']), 88)
check('main historical sum', sum(r['units'] for r in c['main_and_bootstrap']['historical_R7_rows']), 124455)
check('R10 main delta', sum(c['main_and_bootstrap']['R10_delta'].values()), 3245)
check('bootstrap definitions', sum(r['units'] for r in c['main_and_bootstrap']['historical_R9_definition_rows'])+2+6, 48036)

I = len(m['inputs'])
NS = len(namespaces)
EG = sum(n['entry_count']+1 for n in m['closed_directory_inputs'])
DG = sum(n['entry_count']+2 if n['recursive'] else 1 for n in m['closed_directory_inputs'])
F = I + len(m['sources']) + 1
FC = sum((r['bytes']+65535)//65536 for r in m['inputs']+m['sources']) + ((SOURCE/'source-manifest.json').stat().st_size+65535)//65536
# The non-fence schedule is derived independently from actual source call sites.
ref_chunks = sum((r['bytes_cap']+65535)//65536 for r in m['references'])
build_chunks = sum((r['bytes_cap']+65535)//65536 for r in m['compiler_final_outputs'])
nonfence_chunks = 2*ref_chunks + 2*build_chunks + 7*512 + 3*2 + (1+1+2) + (3*16+1+1+16) + 16
check('reference chunk caps', ref_chunks, 2342)
check('outside fence chunk schedule', nonfence_chunks, 10536)
H, B = 8*F+136, 8*FC+nonfence_chunks
RS = 10*I+8*NS+(I+1)+4
W = 784+8*NS+2+2+2+1
D = 784+8*DG+2*79+2*61+2+1
E = 784*7+8*EG+2*78+2*60+2*4+1
CW = 2*W+E
NL = H+(8*NS+120+78+1)+D+4
G = NL+CW+1
EP = G+RS
derived_profile = dict(namespace_guard_rows=EG, namespace_directory_upper=DG, fence_identities=F, fence_chunks=FC,
    hash_calls=H, hash_bodies=B, resolve_calls=RS, walker_calls=W, walker_directory_iterations_upper=D,
    walker_appended_rows_upper=E, walker_canonical_calls_upper=CW, all_NoLink_calls_upper=NL,
    all_canonical_calls_upper=G, all_entry_path_calls_upper=EP)
for key, value in derived_profile.items():
    check('schedule:' + key, value, c['profile'][key])

def entry(d, e):
    # The overflow alternative replaces, rather than duplicates, normal tail22.
    return 56+4725*d+(204+64*512)*e+129+64*512-22
ns_walk = NS*(56+129+64*512-22)+4725*DG+(204+64*512)*EG
walk_all = 784*entry(1,7)+8*ns_walk+2*entry(79,78)+2*entry(61,60)+2*entry(1,4)+entry(1,1)
normal_sets = 8*NS+8
set_elements = 8*sum(n['files'] for n in namespaces)+255
def join(k, units):
    return 66+87*k+64*units
def set_refusal(n, observed, missing, unknown):
    return 169+90*n+87*observed+64*512*(n+observed)+join(len(missing),sum(map(len,missing)))+join(len(unknown),sum(map(len,unknown)))
expected_controls = set_refusal(56,55,['references/native-inverse/r48000-map5.json'],[])+set_refusal(56,57,[],['unlisted.bin'])+set_refusal(56,57,[],[])
def visitor(graph, name):
    g = dict(graph)
    if name == 'failure':
        g.update(array_elements=78,array_separators=77)
    if name == 'marker':
        g.update(array_elements=0,array_separators=0)
    return 185*g['maps']+190*g['arrays']+223*g['keys']+31*g.get('map_separators',g['keys']-g['maps'])+212*g['key_units']+21*g['array_elements']+31*g['array_separators']+184*g['strings']+148*g['string_units']+455*g['integers']+115*g['booleans']
for name, graph in c['graphs'].items():
    check('visitor:' + name, visitor(graph,name), graph['visitor'])
C = {
 'fixedBootstrapDefinitions':48036, 'fixedMainCatchAndSourceCallers':124455+1600+3245,
 'InitializeControllerWithSevenFixedHashes':2136+53*14*512+7*3415+45*379,
 'allHashOwnRowsExcludingNoLink':295*H+65*B,
 'allNoLinkOwnRowsExcludingCanonical':7254*NL,
 'allResolveOwnRowsExcludingEntryPath':(64*512+135)*RS,
 'allWalkerOwnRowsExcludingNoLinkAndCanonical':walk_all-4570*D-4*CW,
 'stageByteOwnRows':784*532,
 'pinsOwnRows':8*(87+50*NS+38*EG+63*4+44*I+NS*(4+I)),
 'FileSetNormalClosedSlots':normal_sets*100+161*set_elements+64*512*2*set_elements,
 'twoUnexpectedFileSetRefusalReplacementSurpluses':2*(201+32862*F+32864*F),
 'threeActualExpectedFileSetRefusals':expected_controls,
 'childOwnRowsWithAllPollsCleanup':3*1044811+74*40+53*512*(40+3*8),
 'threeFencedWrapperOwnRows':3*107+1+2*69,
 '84CompilerReplacementHelpers':53*(84*512+84*512+28*(6+6+7))+217*84+18*18+61*38+66+4*86,
 'twoCopyOwnRowsExcludingNoLink':2*285+70*514,
 'twoOwnedMutationHelpers':2*263,'fourPEHeaderHelpers':4*333,'threeOrdinaryConsolePE':3*31,
 'oneUnexpectedConsolePERefusalReplacementSurplus':93-31,
 'predecessorDescriptor':2+4988+32824+3448+300+35+38,'predecessorMap':3557+62,
 'fourBoundSignedIntegerHelpers':4*110,'twoNormalWorkReservations':2*20,'twoNormalRetainedReservations':2*16,
 'normalAttempt':6,'normalLastLegalPoll':11,'rootASCIIValidation':53*512+49,
 '28CompilerArgumentASCIIValidations':28*(53*512+49),'elevenMeasurementOwnRowsAndDefaults':11*103+7,
 'allRecursiveVisitorPasses':31586+4*1947224+5*135546922+7193129,
 'fourSaveReceiptOwnRowsExcludingMeasurementHelper':4*(684-103),
 'twoWriteJsonOwnRowsExcludingMeasurementHelper':2*59+2+4,
 'thirteenDocumentOwnRows':13*(64*512+91),
 'sevenOnceEncodeOwnRows':38*((319-1)+4*(23635-1)+(1048576-1)+(262760-1))+7*175,
 'bootstrapBoundedRead':197,'twoReadJsonOwnRows':2*311+24+19+17+6,
 'dependencyOtherOwnRows':384+64*512+1221*(69+64*512),
 'twoChildResultOwnRowsWith14AdmittedControls':2*21557+38*131072+14*12630+8*14+15*2,
 'oneAttemptedBadControlPrefixBeforeRefusal':11886+8,
 'twoBuildArtifactOwnRows':2*(47+98*4),'twoReferenceOwnRows':2*(54+39*60+98*56),
 'sameReferencesIncluding56normalIdentities':30+51*56,'sevenOtherNormalIdentityHelpers':7*24,
 'twoUnexpectedIdentityRefusalReplacementSurpluses':2*(93-24),'failureInventoryOwnRows':193+64*78,
 'allByteGrammarAndCompactionHelpers':587*4789990+138*6,'allStringRawHelpers':512*131072+106*16,
 'allCanonicalHelperOwnRows':76*G,'allCatalogueEntryPathHelpers':71*EP+(9529-71)*(1294+3),
 'initializePathCatalogue':209+73*1220+133*4+118+136*(56+3+4)+133*6
}
check('51 independent composition names', sorted(C), sorted(c['composition']))
for key,value in C.items():
    check('composition:' + key, value, c['composition'][key])
    match = re.search(r'`'+re.escape(key)+r'`\s*\|\s*([\d,]+)', (CENSUS/'census.md').read_text())
    check('MD composition:' + key, int(match[1].replace(',','')) if match else None, value)
check('independent scalar sum', sum(C.values()), 5543964320)
check('declared scalar proposal', c['scalar_upper_proposal'], sum(C.values()))

# Capacity equations and proposal-profile path model, never a live-heap model.
payload = c['controlled_payload']
check('known byte requests', sum(sum(payload[key].values()) for key in ['known_normal_input_byte_requests','known_document_byte_upper_requests','known_fixed_array_requests','configured_stream_buffer_upper_requests']),6647190)
check('decoded five string requests',2*(497616+98009+4194304+2*65536)+2*5,9842012)
check('child slices',2*131072+2*66,262276)
def pow2(value): return 1 << (value-1).bit_length()
large = [min(pow2(max(1024,L+6)),pow2(max(1024,16*(6*512+2+7*N+6)))) for L,N in [(497616,15978),(98009,2420),(4194304,2698)]]
check('reader large dyadic capacities',large,[524288,131072,524288])
check('reader retained large arrays',sum(4*v-2048 for v in large),4712448)
check('reader child array upper',8*(131072+16*1030),1180416)
check('reader all array upper',4712448+1180416,5892864)
check('StringBuffer arrays',19*1024*2,38912)
check('writer buffer cumulative upper',7*2*sum(range(1,519)),1881894)
doc_bytes=319+4*23635+1048576+262760
check('writer chunks',2*(doc_bytes-7+7*8000),2924376)
check('ToString storage',2*doc_bytes,2812390)
check('LF concatenation storage',2*(doc_bytes+7),2812404)
check('parent ref arrays conditional',1294*65*8,672880)
check('guard-only upper subtotal',sum(payload['R10_guard_only_subtotal']['terms'].values()),33787198)
check('source clone receipt histogram maps',sum(c['graphs']['receipt']['clone_dictionary_field_histogram'].values()),58)
check('source clone receipt histogram keys',sum(int(k)*v for k,v in c['graphs']['receipt']['clone_dictionary_field_histogram'].items()),300)
primes=[3,7,11,17,23,29,37,47,59,71,89,107,131,163,197,239,293,353,431,521,631,761,919,1103,1327,1597,1931]
prime=lambda value:next(p for p in primes if p>=value)
receipt_primes=sum(prime(int(k))*v for k,v in c['graphs']['receipt']['clone_dictionary_field_histogram'].items())
check('receipt exact clone primes',receipt_primes,362)
check('all seven clone primes',7+4*receipt_primes+9074+553,11082)

base=ntpath.normpath(str(SOURCE)); repo=ntpath.normpath(str(ROOT)); run=ntpath.join(base,'runs','r10-one-reviewed-attempt')
transitive=[ntpath.normpath(r['path'] if ntpath.isabs(r['path']) else ntpath.join(repo,r['path'])) for r in m['inputs']]
transitive += [ntpath.join(base,n) for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json']]
artifacts=[ntpath.normpath(ntpath.join(run,'data',r['path'])) for r in m['references']]
artifacts += [ntpath.join(run,'build',r['name']) for r in m['compiler_final_outputs']]
artifacts += [ntpath.normpath(ntpath.join(run,r['path'])) for r in m['control_files']]
artifacts += [ntpath.join(run,'stage',n) for n in ['stdout.log','stderr.log','receipt.json','consumed.json']]
allpaths=transitive+artifacts+[ntpath.join(run,'stage',n) for n in ['bundle.json','failure-inventory.json']]
pool=set(); visits=0; depth=0
for path in allpaths:
    current=path; current_depth=0
    while current:
        pool.add(current);visits+=1;current_depth+=1
        parent=ntpath.dirname(current)
        if parent==current:break
        current=parent
    depth=max(depth,current_depth)
check('proposed exact root path profile',[len(allpaths),len(pool),sum(map(len,pool)),visits,depth],[1294,1458,186667,14548,18])
strings=['ClockOracle','CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED','A'*64,'OPAQUE',m['predecessor_bundle']['path'],'A'*64]
strings += [s for path in transitive+artifacts for s in [path,'A'*64,'A'*64]]
strings += [r['path'] for r in m['references']]
escaped=sum(len(json.dumps(s,ensure_ascii=True,separators=(',',':')))-2 for s in strings)
bundle_bytes=46364+2*5251+escaped+2*3938+20*1302+5*8+2*1294+2*2+5251+3957+1290+1
check('proposed encoded bundle strings',[len(strings),escaped],[3938,349198])
check('proposed bundle byte upper',bundle_bytes,453111)
check('proposed byte/chunk/string upper reduction',7*(1048576-bundle_bytes),4168255)
for key in ['new_work_limit_selected','combined_U_selected','binding_or_execution_GO']:
    check('no authority:' + key,c[key],False)

report=dict(scope='Independent fresh file hashes, namespace inventory, source-text declaration/body comparison and integer equations only; no target parser/AST/import/helper/compiler/program/stage execution.',
    checks=checks,check_count=len(checks),failures=[r for r in checks if not r['passed']],
    input_count=len(m['inputs']),input_file_identities=identities,namespaces=namespaces,
    helper_bodies=dict(same=same,changed=changed,added=added,removed=removed),
    independent_composition=C,source_functional_finding='R10 Initialize-TaskController compares backslash native paths to unnormalized forward-slash Path.Join expectations; separate required source repair, not a scalar arithmetic mismatch.',
    mathematical_limitations='Equations check a proposed positive source upper, not measured visits, all CLR internals, physical reclamation, a lower bound or payload fit.')
(OUT/'mechanical-evidence.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(checks=len(checks),failures=report['failures'],same_helpers=len(same),changed=changed,scalar=sum(C.values())),indent=2))
raise SystemExit(1 if report['failures'] else 0)
