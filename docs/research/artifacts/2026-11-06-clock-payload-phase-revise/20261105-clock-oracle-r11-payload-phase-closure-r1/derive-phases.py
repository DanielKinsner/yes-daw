"""Source/file/integer worksheet. Never parses/imports/runs the PS program."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
BASE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1'
M=json.loads((BASE/'source-manifest.json').read_bytes())
if hashlib.sha256((BASE/'source-manifest.json').read_bytes()).hexdigest().upper()!='52E3482AF966C562CBFB5119F145B79BC762C3EA8B17D050DC9CFF96CDF54F4D':
    raise ValueError('provisional input manifest changed')
phases=[
    dict(id='P0',name='before-created bootstrap/catalogue/index/pins/predecessor refusal',fences=1,children=0,native_result_parses=0,references=0,builds=0,root_walks=0,stage_calls=0,receipts=0,marker=0,bundle=0,failure_inventory=0,nonfence_hashes=0),
    dict(id='P1',name='created marker/controls/first-receipt/precompiler refusal',fences=1,children=0,native_result_parses=0,references=0,builds=0,root_walks=1,stage_calls=7,receipts=2,marker=1,bundle=0,failure_inventory=1,nonfence_hashes=3),
    dict(id='P2',name='compiler/finally/fence/artifact/dependency/PE-control refusal',fences=3,children=1,native_result_parses=0,references=0,builds=1,root_walks=1,stage_calls=264,receipts=2,marker=1,bundle=0,failure_inventory=1,nonfence_hashes=10),
    dict(id='P3',name='producer/control/first-reference/post-producer-receipt refusal',fences=5,children=2,native_result_parses=1,references=1,builds=1,root_walks=1,stage_calls=267,receipts=3,marker=1,bundle=0,failure_inventory=1,nonfence_hashes=67),
    dict(id='P4',name='verifier/second-reference/root/source-fence/final-receipt refusal',fences=8,children=3,native_result_parses=2,references=2,builds=2,root_walks=2,stage_calls=270,receipts=4,marker=1,bundle=0,failure_inventory=1,nonfence_hashes=131),
    dict(id='P5',name='late artifact/transitive/bundle/preflight/write/hash/footer refusal or success',fences=8,children=3,native_result_parses=2,references=2,builds=2,root_walks=2,stage_calls=272,receipts=4,marker=1,bundle=1,failure_inventory=1,nonfence_hashes=136)
]
for p in phases:
    f=p['fences'];s=p['stage_calls'];r=p['references'];b=p['builds'];w=p['root_walks']
    bad=0 if s==0 else 3
    p['stage_rows_upper']=6*s+bad
    p['walker_calls_upper']=s+31*f+r+b+w+1
    p['walker_directory_iterations_upper']=s+720*f+61*r+b+79*w+1
    p['walker_appended_rows_upper']=6*s+bad+1123*f+60*r+4*b+78*w+1
    p['hash_calls_upper']=1225*f+p['nonfence_hashes']
    p['fence_expected_threefield_maps']=1224*f
    p['normal_FileSet_calls_with_controls_upper']=31*f+b+r+(1 if r else 0)+(3 if r else 0)+p['native_result_parses']+(1 if w==2 else 0)
    p['normal_output_encodes_upper']=p['receipts']+p['marker']+p['bundle']+p['failure_inventory']
    p['explicit_twofield_hash_and_threefld_fence_map_requests']=168*p['hash_calls_upper']+264*p['fence_expected_threefield_maps']
    p['actual_digest_array_and_hex_string_requests_upper']=(56+152)*p['hash_calls_upper']
    p['internal_tuple_row_objects_upper']=56*p['walker_appended_rows_upper']
    p['walker_queued_twofield_maps_upper']=168*p['walker_directory_iterations_upper']
    p['poll_direction_object_arrays_upper']=40*7200*p['children']
    p['stage_allowed_sixname_arrays_upper']=72*p['stage_rows_upper']
    p['owned_row_three_scalar_referent_boxes_upper']=72*p['walker_appended_rows_upper']
    # Deliberately a partial numeric subtotal: the complete conditional sum below
    # names every other family, with its unqualified coefficients kept visible.
    names=['explicit_twofield_hash_and_threefld_fence_map_requests',
           'actual_digest_array_and_hex_string_requests_upper','internal_tuple_row_objects_upper',
           'walker_queued_twofield_maps_upper','poll_direction_object_arrays_upper',
           'stage_allowed_sixname_arrays_upper','owned_row_three_scalar_referent_boxes_upper']
    p['partial_selected_family_upper']=sum(p[k] for k in names)
    # Conditional typed subset, consistently INCLUDING qualified headers.
    # Both fixed parsed inputs, original bytes, compact source strings and all
    # fixed reader arrays are retained. Alternative earlier identity refusals
    # have smaller allocations in this subset and no additional parse graph.
    q=5143792+863848+499560+98040+812240+196040+2*30816+2*2072
    if p['children']:
        q+=4194328+8388632+1046744+2072
    if p['native_result_parses']>=1:
        q+=1088+2152+2240+4*2072
    if p['native_result_parses']>=2:
        q+=2688+5352+5660+12*2072
    p['typed_partial_join_upper']=q+p['partial_selected_family_upper']
    p['whole_upper_equation']=str(p['typed_partial_join_upper'])+' + Delta_'+p['id']

if phases[-1]['hash_calls_upper']!=9936 or phases[-1]['normal_FileSet_calls_with_controls_upper']!=259:
    raise ValueError('source schedule fails independent old global incidence cross-check')
if phases[-1]['walker_appended_rows_upper']!=10904 or phases[-1]['stage_rows_upper']!=1635:
    raise ValueError('new stage-incidence arithmetic')

terms=[
 dict(id='F',family='fixed source/bootstrap/definitions/registration/control literal owners',equation='All exact source fixed literal/typed/reference arrays,68 helper registrations,initial receipt/schema maps,thresholds,controller reflection/hook/pin/ref/argument arrays and key arrays at their actual source sites; use source-owner-sites.json, original census inventory and current correspondence.',gap='Exact pinned PS literal/compiler/binder copy and boxing-capacity correspondence. No generic per-action tariff and no zero wrapper allocation.'),
 dict(id='R',family='original captures/UTF8 source strings/grammar stacks',equation='max(prefix raw identity refusal, admitted fixed bootstrap branch)+max(predecessor raw refusal, fixed predecessor branch)+dependency branch if P>=P2+native1064/2662 captures if corresponding result reached+explicit hash/copy/mutation/PE/log/stream arrays at reached sites. Every array has24-byte header/align8; strings Align8(22+2n).',gap='Bounded report compact length variable<=4MiB; prefix alternatives are max, not extra successful decodes. Selected stream strategy buffers/source owners remain in V below.'),
 dict(id='J',family='reader/StringBuffer/JToken/parsed OrderedHashtable graphs',equation='fixed bootstrap5143792+predecessor863848 when reached, plus raw/decoded inputs separately R; reader arrays30816 each, report1046744, child33152 total and StringBuffers6216 fully reached; add generic dependency and child parsed graph formulas, reader/writer instance/stack metadata and all graph iterators.',gap='Generic report graph is bounded by2698 tokens including keys/S512/depth24, but needs whole maximization of maps/arrays/scalars/strings and Hashtable insertion/cleanup capacities. UInt64>Int64 BigInteger source branch needs its bounded20-character extra term. Existing graph numbers omit these named terms, never zero.'),
 dict(id='H',family='hash and expected identity carriers/digest/hex',equation='168*(1225*f+nonfenceHashes)+264*(1224*f)+208*(1225*f+nonfenceHashes) for selected families; other exceptional identity witness maps stay E and O.',gap='These are actual source allocation-request upper coefficients from ABI/source, not caller prices. Future internal representation changes require fresh consumer/census proof.'),
 dict(id='W',family='walker internal rows/queue records/backings/iterator owners',equation='56*E +72*E stored scalar referent upper +168*D queued maps; add exact List/Queue growth+ToArray/pipeline/binder arrays by each walker extent and selected scalar depth cells. E/D/call vectors are phase data, not finalCount-only capacities.',gap='Directory iterator objects/buffers and successful/first-refusing normalization strings are P/V. Literal/pipeline intermediate copy association is F/A, not omitted.'),
 dict(id='O',family='all original output/receipt/control/reference/build/artifact/transitive/failure DTO graphs',equation='All source-created maps/lists/arrays/boxed numeric cells from marker1,receipts<=4,controls<=10,reference56 twice,build3 twice,control files4,stage artifacts4,transitive1225,bundle1294 maps,failure<=79 maps,private measurement states and expected/refusal witnesses. Preserve aliases without cloning referents; retain actual replacement dictionaries/arrays cumulatively.',gap='Use exact source object graph/capacity schedule and selected PS literal map constructor convention; no direct tuple serialization. Canonical functional union/root changes transitive/bundle counts and must recompose.'),
 dict(id='K',family='ConvertToJson preprocessing clones and traversal enumerators',equation='At most marker1+receipts4+bundle1+failure1 encodes. Dictionary initial prime-cell sums7+1448+9074+553=11082 each bucket/entry array; plus80 per cloned dictionary and24 per backing array, List<object> all dyadic growth/instances/enumerators, all original ToArray results separately O/A.',gap='Exact object-list iterator/copy schedule, error witness graph and final union correspondence. Five bundle Measure passes create no codec clones; charge exactly one reached Encode.'),
 dict(id='B',family='writer escape arrays/StringBuilder chunks/ToString/LFconcat/UTF8 output',equation='Per reached output document: old writer arrays retained, generic seven-writer payload bound1881894 plus their qualified headers; append-only chunks<=document bytes+8000 chars, finalString/LFconcat/UTF8 document arrays all retained. Marker319,receipt23635 each,failure262760,bundle<=1048576 until new root/union bound selected.',gap='Use pinned JavaScriptUtils/JsonTextWriter/StringBuilder source and exact document schedule; no ReturnBuffer/reclamation credit. Old453759 root estimate is not automatically an R11/final-union bound.'),
 dict(id='P',family='path catalogue/pool/aliases/parents and every fresh path/refusal output',equation='Catalogue1220 input+own5+success68+failure1 identities=1294, at most1297 first-build attempts; parent[65] arrays and all retained alias/map/dictionary backings. Add each fresh iterator Current/GetFileName/namespace relative/Replace/raw Join/case/unknown/tilde normalization, FileSet expected/unknown/missing joins, safe-prefix/footer strings.',gap='Final canonical union and exact fixed-root pool geometry pending; current R11 own max178 does not transfer old1458/187423. Preserve unknown/tilde fallback. Bound native normalizer successful<=512 outputs and at most actual first-refusing prefixes by selected source/API capacities; no raw-length-to-normalized-length shortcut.'),
 dict(id='A',family='all source scalar/reference/literal/typed/pipeline/binder arrays and boxes',equation='Source-owner-sites.json plus original census full literal registry, phase counts and new tuple correspondence: normal polls<=7200*c create direction2 arrays,stage checks create six-name arrays per reached row; include exact key sets,task-read += copies,cleanup arrays,Args28/6/6,ToArray and concatenation arrays,FileSet input/set/backing arrays and enum owners.',gap='Pinned PowerShell expression compiler/pipeline/parameter copy capacities and literal-hoisting correspondence, including stored vs temporary scalar boxing. Explicit selected numeric subfamilies are shown in phase partial upper; do not also charge them twice.'),
 dict(id='N',family='incoming environment captures/outgoing blocks',equation='Sum for children1..c of Capture(Eentry_i,Eunits_i,Eunique_i,Bnative_i)+8-key outgoing builder/string/sorted-ref-array. Capture includes native block,key/value strings,Hashtable capacities,cleared inherited Dictionary old/new capacities and selected owner cells. Exact formulas retained from R10 census.',gap='14-key authored seed is not poststartup/import closure. Source/config/import writer proof must supply every actual E/V/B at each child boundary; no host env enumeration, no opaque inherited snapshot assigned0.'),
 dict(id='V',family='process/stream/command/native-path strategy buffers and owners',equation='Reached child c owns two4096 source read arrays,two configured log buffers,two hidden StreamReader byte/char buffers and underlying stream buffers,cleanup13/ref/task arrays; command buffers use exact28/6/6 arguments and bound quoting. Add source-owned read/copy/mutation/PE/write strategy/native/pooled arrays/handles at actual sites.',gap='Exact selected console encoding GetMaxCharCount,stream/path strategy and ArrayPool capacity/source association. Return/Dispose/null/Clear are not a release proof. OS child-copy/internal compiler heap stays separately opaque under contract.'),
 dict(id='E',family='controlled failure witnesses/copies/footer and retained raw references',equation='One primary plus11 first-secondary reference slots,13 cleanup obligations per reached child,actual main/finally/inventory/receipt refusal prefixes and source-created bounded witness/Join/DTO/footer strings and codec copies. Raw exceptions can alias slots; every explicit controlled projection is counted.',gap='Opaque Exception/ErrorRecord general heap is explicitly unqualified under accepted contract344408 lines47-50/109-110, not zero or freed. Earlier error-internal worksheet HOLD wording is superseded only to that exact boundary.')
]
data=dict(schema='R11_SIX_PREFIX_CONTROLLED_PAYLOAD_CONDITIONAL_JOIN_R1',
 status='WHOLE_CONDITIONAL_EQUATION_WITH_EXPLICIT_UNCLOSED_TERMS_NO_NUMERIC_FIT',
 source_manifest_sha256='52E3482AF966C562CBFB5119F145B79BC762C3EA8B17D050DC9CFF96CDF54F4D',
 cap=33554432,phases=phases,terms=terms,
 whole_equation='For each phase i: U_i=F_i+R_i+J_i+H_i+W_i+O_i+K_i+B_i+P_i+A_i+N_i+V_i+E_i, with disjoint ownership and the seven numeric selected families removed from A/O/etc when already charged. Whole conditional upper=max(U_P0,...,U_P5). Source scalar counts are not byte prices. No numeric U_i or fit is asserted until every gap coefficient/profile is source-qualified and recomposed.',
 residual_definition='Delta_i is exactly the sum of the named disjoint families not in typed_partial_join_upper: F; R fixed hash/copy/mutation/PE/grammar/log/stream array owners not in original captures; J generic dependency/native parsed graphs and reader/writer instance/stack owners; W queue/list/backing/iterator/depth-cell owners beyond selected maps/rows/stored row boxes; O all original output DTOs/states; K all serialization clones/iterators; B all document writer/chunk/strings/bytes; P all path catalogue/fresh/refusal strings/arrays; A all remaining literal/binder/pipeline/typed/ToArray/FileSet capacities; N all env captures/outgoing blocks; V all nonduplicated process/stream/command/path strategy buffers; E all controlled exception/witness/projection/footer objects/copies. Numeric selected H, W maps/tuple/stored boxes, A poll/stage literal arrays, and fixed parsed inputs/captures/readers are excluded from Delta to avoid double counting. Opaque general CLR/Exception/ErrorRecord heap remains unqualified under contract; no0 allocation claim.',
 branch_join='P0 raw identity failures do not also allocate successful parsed inputs; failed child never reaches its ReadChildResult or later phases. P1/P2 marker+2receipts+failure; P3 marker+3receipts+failure; P4 marker+4receipts+failure; P5 marker+4receipts+bundle+failure. Success itself has marker+3receipts+bundle. Maxima do not assume scope/null/GC releases.',
 stage_join='P0=0. P1 document<=7. P2 compiler257+documents7=264. P3 max(compiler257+failedProducer2+documents7,compiler257+normalProducer1+documents9)=267. P4 max(257+1+failedVerifier2+documents9,257+1+normalVerifier1+documents11)=270. P5 successful child scans257+1+1 plusdocuments13=272. At most three failed stage scans append a seventh row; successful scans have<=6 allowed names.',
 source_row_limits='namespace sum(entry_count+1)=1123 per fence, directories<=720; references rows60/dirs61,build rows4/dirs1,root rows78/dirs79,runs rows1/dirs1. Optional/extra empty directories are not assumed absent. All returned and first-refused metadata/path work remains included.',
 execution_boundary='No target PowerShell parse/AST/import/helper,compiler/program/protocol stage,GUI/device,Git/CI. Root separately executed only reviewed file-metadata/ABI tools. Current R11 source guards remain false and code/data/prices unbound. Canonical functional source union is provisional; no old price/profile or archive-copy admission transferred.')
(OUT/'phases.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n')
source=(BASE/'run-clock.ps1').read_text()
markers=('::new(', '@(', '@{', '.ToArray(', '.Substring(', '.Replace(', 'GetRelativePath(', 'Join-Path ', '.GetHashAndReset(', '::ToHexString(')
sites=[dict(line=i,source=line,matched=[s for s in markers if s in line]) for i,line in enumerate(source.splitlines(),1) if any(s in line for s in markers)]
(OUT/'source-owner-sites.json').write_text(json.dumps(dict(schema='R11_TEXT_ONLY_OWNER_SITE_CANDIDATES_R1',claim='Text inventory only, not target AST or complete allocator/census proof. Whole source/caller review and exact compiled PS correspondence remain required.',sites=sites),indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(phases=[{k:p[k] for k in ('id','stage_calls','stage_rows_upper','walker_calls_upper','walker_directory_iterations_upper','walker_appended_rows_upper','hash_calls_upper','partial_selected_family_upper')} for p in phases],owner_text_sites=len(sites)),indent=2))
