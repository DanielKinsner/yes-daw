import hashlib,json,pathlib,re
BASE=pathlib.Path(__file__).resolve().parent.parent
ROOT=BASE.parent.parent
SUB=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-source-r5'
def pin(p):
 p=pathlib.Path(p)
 return {'path':p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest().upper()}
enum=re.search(r'enum class NativeFact[^\{]*\{(.*?)\};',(SUB/'native-profile.h').read_text(),re.S)[1]
names=[s.strip() for s in enum.split(',') if s.strip()!='Count']
F=[]
def fact(name,status,evidence,closed,remaining,next_step):
 F.append({'ordinal':len(F),'name':name,'status':status,'evidence':evidence,'static_implication':closed,'still_required':remaining,'smallest_next_step':next_step,'runtime_digest_is_not_proof':True})
source_review='build-ci/20261105-musical-funded-legacy-adapter-r5-source-correspondence-independent-review/review.json'
objfolder='build-ci/20261105-musical-funded-r5-force-emission-object-r1'
graph='build-ci/20261105-musical-native-math-region-certificate-r1/certificate.json'
graphreview='build-ci/20261105-musical-native-math-region-certificate-independent-review-r1/review.json'
closedapp='build-ci/20261105-musical-fma3-closed-application-source-r1/evidence.json'
allocator='build-ci/20261105-musical-native-allocator-association-r1/evidence.json'
old='build-ci/20261105-musical-window-query-path-census-r1/census.json'
strict='build-ci/20261105-musical-funded-r4-native-object-r1/inspection.json'
layout='build-ci/20261105-musical-funded-r5-layout-object-r1/layout-record.json'
objrec=objfolder+'/attempt-r1/receipt.json'
fact('FrozenSourceAndCatalogue','SOURCE_IDENTITIES_CLOSED_CATALOGUE_BINDING_PENDING',[source_review,old],
 'All six code files match accepted R4 whole algorithm, except reviewed R5 declaration-only move. Frozen engine helpers and exact190 ordered packets/case templates retained. Current R5 manifest exactF55E268E.',
 'Actual selected immutable Catalogue/OldQueryCatalogue/NativeBinding definitions, no runtime initializer, exact typed literal identities and all source/link pins. Complete numerical review is separate.',
 'Reuse the source correspondence; after complete numeric correction/review bind exact independently accepted immutable data, not current proposal numbers.')
fact('SelectedSdkTopologyAndConfiguration','SELECTED_SOURCE_TOPOLOGY_CLOSED_OBJECT_CONFIGURATION_RECORDED',[old,objrec,objfolder+'/profile.json'],
 'Pinned MSVC upper_bound count/half/first recurrence and iterator/hardening configuration determine actual t/f packets. Actual old compiled-tick object visibly inlines that recurrence; own R5 search follows it.',
 'Future linked caller must preserve selected SDK/macros/flags and ABI; flags/identity fields alone do not assert functional equivalence. No alternate linear walk or complexity ceiling substitutes for actual t/f.',
 'Bind actual source+toolchain+SDK+generated config and final caller command to current object, or repeat exact compile-only correspondence for a changed identity.')
fact('ActualPreciseComputeFlagsNoContract','APP_PROFILE_COMPILE_ONLY_CORE_OBJECT_EVIDENCE',[objrec,objfolder+'/profile.json','object-sections.json'],
 'Root compiled actual force TU under complete historical Main command, default precise/noGL, /O2/Ob2 then/Ox/MP; no compute strict override. Core16 whole sections have no displayed x87/FMA/control instruction and match all raw bytes.',
 'Actual final integration/link selection and any later flags/source changes. This object is not proof of future application version/caller; unresolved leaves retain native contracts.',
 'Preserve current app object as compile evidence; inspect final linked identity once genuine integration exists. Do not substitute pure-probe/O2 evidence for app context.')
fact('ObserverStrictObject','STRICT_OBJECT_SOURCE_CORRESPONDENCE_CLOSED_LINKED_BEHAVIOR_PENDING',[strict,source_review],
 'Actual strict object seven fixed leaves inspected, compiled0. R5 native-observer.cpp/native-profile.h are byte-identical to its R4 inputs. Control/anchor/module/page prequalification leaves have fixed footprints, no unqualified math getter call.',
 'Final strict-object link/import/module identity, selected observation/fenv semantics and true interval. No raw x87 promise. Kernel API return behavior/capacity separate.',
 'Reuse exact strict object if final symbols/dependencies correspond; separately qualify selected linked behavior without changing compute global FP flags.')
fact('PrivatePrefixInitialAndStoreInduction','ALL_INPUT_SOURCE_INDUCTION_AND_CORE_STORE_OBJECT_CORRESPONDENCE',[source_review,objrec,'object-sections.json'],
 'Positive-zero cache assign and replay base; same370-byte H symbol at original build/query/replay; prior-cache and named-frame binary64 load/store boundaries visible. Induction P[i]=C[i] holds for every admitted needed prefix under deterministic H/control/identity premises.',
 'Conjunction of actual native math/classification/control/dispatch/immutable-input premises. No private cache inspection or successful numeric-range assumption supplies it.',
 'Review the induction plus same-symbol/store section anchors; close named external native premises, without finite-map whitelist or another cache/Clock.')
fact('BuildReplayQueryCodegenEquivalence','ACTUAL_APP_OBJECT_CORE_TRANSITIONS_ESTABLISHED_CONDITIONAL_LEAVES',[objrec,'object-sections.json'],
 'Original compiled build+315, old compiled musical time+223, standalone tick+197, R5 prefix+1B6 and partial append+E5 all reference Hsection1A9. Classifier slope/relative SSE2 operands match H; empty formula and floor/cast core boundaries present. Shared H is emitted out-of-line; prefix inlines its wrapper only.',
 'Final symbol resolution/COMDAT selection, all required external leaf contracts and any future different compiled source. Core16 is not a claim about all object code/callees.',
 'Preserve this concrete object argument and verify eventual link/provider/source identity; no further generic expression-equality experiment needed.')
fact('FullDomainLibmAndClassification','GUARDED_CANDIDATE_LOG_GRAPH_CLOSED_CLASSIFICATION_ASSOCIATION_EXPLICIT',[graph,graphreview,old,objrec],
 'Finite x>-1 leads RN(1+x) in[2^-53,DBL_MAX], normal positive; both SSE/FMA code arms and every table/index dependency closed for entire guarded bit domain. Integer endpoint proof eliminates fractional/ambiguous branches; floor representability holds for0<=frame<2^63.',
 'Actual linked `_dclass` direct-relocation provider and status/control independence, selected floor contract/identity, loaded log region association. Nine log spans do not silently qualify these separate SDK leaves.',
 'Use existing UCRT export0x688F0 for bounded `_dclass` candidate-body/footprint association; final link/import remains explicit. Reuse existing candidate log certificate; do not reopen unrelated vendor graph.')
fact('RelevantLoadedMathRegionsAndDispatchAssociation','CANDIDATE_GRAPH_AND_ACTUAL_MATCHER_SOURCE_CLOSED_RUNTIME_BINDING_PENDING',[graph,source_review,objrec],
 'Matcher exact same-image anchors/module bounds/protection guards and byte-wise complete9 spans can reject different candidates before Clock; D/A address-only getter footprint separately validated. Relocation/index proof is already closed.',
 'Private immutable region/expected arrays/capacities and actual module/anchor/code/table/lifetime association, not present yet; own page checks are not module lifetime/no-writer proof. `_dclass` and floor actual provider association remains named.',
 'After complete proof select reviewed immutable extent data and bind final real module identities. No unbound candidate9/8284/1/4 is treated as a definition or runtime success.')
fact('Fma3GetterAndTrueIntervalStability','CLOSED_APP_STOCK_RUNTIME_INTERVAL_PREMISE_CONDITIONAL_ACTUAL_CALLER',[closedapp,allocator,graph],
 'Getter emits0/1 from the same mutable dispatch DWORD used by log; both allowed arms can differ in last bits. Closed source/startup/no-setter and qualified stock callbacks supply true stability, not endpoint ABA sampling.',
 'Final combined source/runtime/provider/module identity and startup ordering; address-only readable same-image4-byte footprint through interval. Future admitted writer/callback changes invalidate the premise.',
 'Associate actual combined identity with pinned3125-source corpus and named runtime graph; no setter, new lock, profile boolean or repeated sampling is proposed.')
fact('OriginalClockAllocationCallbackInterval','NARROW_STOCK_ALLOCATOR_SOURCE_BODY_PREMISE_CONDITIONALLY_CLOSED',[allocator,closedapp],
 'Actual scalar-new archive member/provider/calls and candidate MSVCP/UCRT null-handler guards associated to source. Separate C/C++ null states plus absent setters mean no application allocation callback before successful original prefix math.',
 'Final default-library/archive member selection and actual loaded stock runtime correspondence; no hidden request-count/failed-allocation trace or complete physical-resource bound follows.',
 'Use final link/object provider association with existing scalar-new36729-byte member/candidate pins. No OOM execution or broad vendor disassembly needed.')
fact('NewHandlerIdentityAndInterval','SEPARATE_C_CPP_HANDLER_SOURCE_INTERVAL_CONDITIONAL',[allocator,closedapp,strict],
 'Both initial handler states are null under completed startup; source excludes both C/C++ setter calls. C++ getter-null alone is insufficient. Actual observed pointer must match the true qualified interval premise.',
 'Final closed caller/runtime association and expected handler binding; any admitted nonnull callback needs its actual source proof, not an ABI/FMA inference.',
 'Reuse the explicit no-setter/startup/default-provider argument and confirm final source/module identity; do not install a handler to test it.')
fact('RelevantControlInterval','OWNED_CORE_NO_MODE_WRITES_AND_NAMED_ABI_PREMISES',[objrec,'object-sections.json',strict,allocator],
 'Core selected owned sections perform no mode writes; strict restore merges low6 status only. Ordinary returning stock APIs preserve agreed Windows x64 controls. Other threads modes do not determine this worker controls; no app-wide default-control assertion.',
 'Actual worker-entry accepted controls, true interval across every selected external call/allowed callback, observer neutrality and actual caller/thread integration. Snapshot equality alone cannot exclude an intermediate mode change.',
 'Qualify the named worker scope/stock external calls with the same combined identity. No global FP policy change or new synchronization architecture.')
fact('ExceptionFlagAndErrnoRestoration','SOURCE_AND_STRICT_INSTRUCTION_SHAPE_CLOSED_SELECTED_BEHAVIOR_PENDING',[strict,source_review,objrec],
 'Replay target captured after Clock; five fenv flags and all6 rawMXCSR status bits restored with current controls,errno last. Explicit+destructor precheck<=2; physical restore<=1 because flag clears before result branch; no post-real-query restore.',
 'Selected linked fenv/errno/getter observation/restoration behavior, ordering/neutrality including post-restore read and error handling. Failure remains named refusal, not claimed successful preservation. No raw x87 theorem.',
 'Review/qualify exact strict bridge and linked selected leaves on normal/early paths, supported by source all-input reasoning. Finite status tests may support but cannot replace it.')
fact('ImmutableInputLifetimeNoAlias','PRIVATE_CAPTURE_OWNERSHIP_SOURCE_SHAPE_CLOSED_REAL_CALLER_PENDING',[source_review,'docs/adr/0088-musical-origin-formation.md'],
 'Captured map/options/metadata copied before handoff; state owns no worker/result backcycle; worker acquires before canonical mutation,cleanup precedes terminal publication. Same exact M/r retained through build,replay,query.',
 'Actual genuine input/source/control owners, no concurrent mutable alias, cancellation borrowed lifetime, real worker/model handoff/TSan and final integrated caller. Private protocol guards cannot manufacture provenance.',
 'Implement/test actual owner/control task integration under existingADR, then bind exact caller/input lifetime proof; no synthetic ProofAccess fixture accepted as production premise.')
fact('CompleteNewSourceFunding','HOLD_COMPLETE_NUMERIC_CRITIQUE_CORRECTION_BINDING_PENDING',[source_review,'docs/adr/0088-musical-origin-formation.md','docs/adr/0089-frozen-musical-builder-attempts.md'],
 'Source has every explicit funded boundary, protected queryCallee and branchless commitment. ADR89 exact semantic attempts remain distinct from new literal source; all5 role lifetimes and sequential Result->Retained/Reader must be paid.',
 'Fresh complete420-row numeric acceptance/correction, all bounds/root/Q/cleanup/lifetime inequalities, exact typed immutable price definitions and same-Origin cap binding. Current sealed proposal is not accepted funding; no oldWorker slack.',
 'Wait complete independent REVISE/ACCEPT, preserve sealed originals, author bounded correction if required, then independently review/bind under unchanged2^28.')
fact('ActualCombinedCallerIdentity','HOLD_FORCE_OBJECT_IS_NOT_GENUINE_FINAL_INTEGRATION',[objrec,source_review,closedapp],
 'Compile-only wrapper reaches genuine public R5 worker entry without Data/main/stubstate. Exact complete historical Main command was retained; actual core compute object now exists.',
 'No final genuine application caller/link/module/task integration exists. Historical versiondefine, pure probe,37layout table and unlinked wrapper cannot identify future combined app.',
 'After prerequisites author actual admitted caller/whole proof under existing architecture; retain source+link/default-library/module+flags association. No producer/public recipe source authored here.')
fact('IndependentAllInputForwardProof','COMPLETE_CONDITIONAL_IMPLICATION_REQUIRES_INDEPENDENT_CONJUNCTION_REVIEW',[source_review,old,graphreview,objrec,'argument.md'],
 'Source induction/search/partial/empty/integer-floor/resolve tree establish exact classification and vector equivalence for every admitted input under the named native/caller/funding premises. Actual core object strengthens codegen premises without replacing external ones.',
 'Independent review of this whole argument and establishment of all16 other NativeFacts plus physical resource/whole publicAPI obligations. Nonzero digests and finite green points are not proof validation.',
 'Review this packet, resolve the concrete remaining hypotheses above and keep binding/execution held until complete conjunction is established.')
assert [x['name'] for x in F]==names and len(F)==17

obj=json.loads((BASE/'object-sections.json').read_bytes())
assert all(x.get('displayed_bytes_match_entire_raw_section') for x in obj['targets'].values())
assert len(obj['targets'])==16
assert all(not x['x87_FMA_or_control_instruction_lines'] for x in obj['targets'].values())
checks=[]
def has(label,pattern):
 s=(BASE/'evidence'/f'{label}.txt').read_text()
 assert re.search(pattern,s,re.M),(label,pattern)
 checks.append({'section':label,'assertion_regex':pattern})
has('compiled-build',r'0000000000000315:.*call\s+\?appendTempoSegmentFrames')
has('compiled-build',r'00000000000002E4:.*movsd\s+xmm0,mmword ptr \[rax\+rsi\*8-8\]')
has('compiled-build',r'000000000000032B:.*movsd\s+mmword ptr \[rax\+rsi\*8\],xmm0')
has('prefix',r'00000000000001B6:.*call\s+\?appendTempoSegmentFrames')
has('prefix',r'000000000000026F:.*movsd\s+mmword ptr \[rax\],xmm0')
has('append-new',r'00000000000000E5:.*call\s+\?appendTempoSegmentFrames')
has('compiled-tick',r'0000000000000197:.*call\s+\?appendTempoSegmentFrames')
has('compiled-musical-time',r'0000000000000223:.*call\s+\?appendTempoSegmentFrames')
has('old-append',r'0000000000000108:.*call\s+qword ptr \[__imp_log1p\]')
has('old-append',r'0000000000000138:.*movsd\s+mmword ptr \[rax\],xmm0')
has('classify',r'0000000000000149:.*call\s+_dclass')
has('classify',r'0000000000000193:.*call\s+_dclass')
has('resolve-new',r'0000000000000215:.*call\s+\?restore@FundedNativeSession')
has('resolve-new',r'000000000000025D:.*call\s+\?resolveMusicalAudioWindow')
has('clock-frameAt',r'00000000000002CA:.*call\s+floor')
has('clock-frameAt',r'00000000000002D7:.*call\s+floor')
has('endpoint',r'0000000000000145:.*divsd\s+xmm1,mmword ptr \[__real@413c200000000000\]')
has('compiled-tick',r'0000000000000066:.*divsd\s+xmm2,mmword ptr \[__real@413c200000000000\]')
has('finish-endpoint',r'00000000000002CF:.*cvttsd2si\s+rax,xmm0')
result={'schema':'R5_ALL_INPUT_FORWARD_NATIVE_FACT_OBLIGATION_ARGUMENT_R1','status':'CONDITIONAL_ALL_INPUT_SOURCE_IMPLICATION_CORE_APP_OBJECT_CORRESPONDENCE_NOT_COMPLETE_NATIVE_BINDING','date':'2026-11-05','subject_manifest':pin(SUB/'source-pins.json'),'facts':F,'NativeFact_order_exact':True,'fact_count':17,
 'proof_steps':['Exact immutable captured M/r and successful Clock witness only','Positive-zero C0 and P0 base','Same actual H and binary64 stored prior values imply next equality','All needed full prefixes by induction without private reads','Shared cursor monotone across tick-ordered A/B;partial frame never overwrites full prefix','Exact SDK count/half/first recurrence and actual t/f packet','Identical selected j and partial arguments; safe signed differences','Every reached AS leaf classified before actual append; fullsuccess witnesses omitted full guards','Exact empty formula operation order and failure predicate','Scalar resolve refusal order; guarded start+length','Integer0/1 fraction eliminates high carry and fractional path','Forward false/native finite-negative-overflow classifications','Safe representable floor/native conversion; no BoundaryAmbiguous integer suffix','A-first/B-second failure,collapsed/success tree and whole190-row vector construction','Protected exact old call once after successful pre-restore;no post-old restore','True source/stock-runtime dispatch/control/newhandler intervals,not snapshot ABA inference','Five fenv/six raw MXCSR/errno restore scope and repeated failed precheck behavior','All5 same-Origin lifetimes including sequential Result then Retained/Reader','Complete conditional implication requires external premise conjunction'],
 'actual_object':{'scope':'Unlinked actual historical application-profile force wrapper object; not final genuine caller','object':pin(ROOT/(objfolder+'/attempt-r1/app-compute.obj')),'listing':pin(ROOT/(objfolder+'/attempt-r1/app-compute-disassembly.txt')),'section_count_inspected':16,'whole_displayed_raw_bytes_match':True,'direct_shared_H_section_hex':'1A9','H_bytes':370,'x87_FMA_control_in_selected_sections':0,'named_text_assertions':checks,'target_linked_or_executed':False},
 'additional_leaf':{'actual_relocation':'direct REL32 _dclass, symbol index0x767 UNDEF External; not __imp__dclass','selected_declaration':'corecrt_math.h191:short __cdecl _dclass(double),_ACRTIMP','selected_predicate':'C++ isfinite template435..438:fpclassify(_X)<=0; double fpclassify322..329 calls_dtest(&_X), compiler lowers selected uses to direct_dclass. cmath1103 imports _CSTD isfinite.','existing_candidate_export':'ucrtbase.dll-exports.stdout.log:_dclass RVA0x688F0 ordinal210; _o__dclass separate0x842A0','next':'Root bounded candidate body/footprint read then selected link/provider/ordinary stock behavior qualification; no9logspan claim for this separate leaf'},
 'not_claimed':['No unconditional all-input NativeBinding/proof','No nonzero digest proof validation','No finite measurement extrapolation','No final linked caller/module/provider identity','No whole resource fit or old private allocation/visit trace','No numeric census acceptance/price definitions','No Data/producer/public recipe/architecture/ADR edits','No compile/link/run/native test by this argument author'],
 'actions':{'ordinary_source_text_JSON_metadata_hash_reads':True,'independent_Python_integer_and_existing_binary_listing_association':True,'compiler_disassembler_target_parser_AST_import_helper_program_stage_native_GUI_Git_CI_by_author':False}}
(BASE/'argument.json').write_bytes((json.dumps(result,indent=2)+'\n').encode())
inputs=[
 'docs/adr/0085-owned-musical-call-recipes.md','docs/adr/0088-musical-origin-formation.md','docs/adr/0089-frozen-musical-builder-attempts.md',
 *['build-ci/20261105-musical-funded-legacy-adapter-source-r5/'+x for x in ['formation-r8.h','worker-result.h','legacy-adapter.h','window-query.h','native-profile.h','native-observer.cpp','source-correspondence.md','README.md','source-pins.json']],source_review,
 'src/engine/Time.h','src/engine/MusicalAudioWindow.h','src/engine/MusicalAudioChannelPlan.h',old,
 'build-ci/20261105-musical-window-query-path-census-r1-independent-review.json',graph,'build-ci/20261105-musical-native-math-region-certificate-r1/certificate.md',graphreview,
 closedapp,'build-ci/20261105-musical-fma3-closed-application-source-r1/source-corpus.json','build-ci/20261105-musical-fma3-closed-application-source-r1/argument.md',allocator,'build-ci/20261105-musical-native-allocator-association-r1/report.md',
 strict,layout,'build-ci/20261105-musical-query-fp-profile-probe-r1-independent-review.json','build-ci/20261105-musical-query-fp-profile-probe-r1/execution-r1/receipt.json',
 'build-ci/20261105-musical-query-msvc-native-argument-r1/argument.md','build-ci/20261105-musical-query-fp-native-association-r1/ucrtbase.dll-exports.stdout.log',
 'build-ci/20261105-musical-funded-r5-force-emission-source-r1/source-pins.json',objfolder+'/profile.json',objfolder+'/root-source-review.json',objrec,
 objfolder+'/attempt-r1/disassembly-receipt.json',objfolder+'/attempt-r1/first-green-compile-source-freeze.json',objfolder+'/attempt-r1/app-compute.obj',objfolder+'/attempt-r1/app-compute-disassembly.txt',
 'C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt/corecrt_math.h',
 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include/cmath']
records=[pin(ROOT/x) for x in inputs]
assert records[11]['sha256']=='F55E268E0D24859E3305C8C75CB6EF2D0948C719C4B63AD665BCC02B0A11CC84'
# Ensure the actual reviewed subject six-code byte identities are intact.
review=json.loads((ROOT/source_review).read_bytes())
expected={x['path']:x['sha256'] for x in review['owned_packet_files']}
for r in records:
 if r['path'] in expected:assert r['sha256']==expected[r['path']],r['path']
outputs=[pin(BASE/x) for x in ['argument.md','argument.json','object-sections.json']]
evidence=[pin(p) for p in sorted((BASE/'evidence').iterdir()) if p.is_file()]
pins={'schema':'R5_FORWARD_NATIVE_OBLIGATION_ARGUMENT_SOURCE_PINS_R1','status':'SEALED_CONDITIONAL_ARGUMENT_REQUIRES_INDEPENDENT_REVIEW_NOT_BINDING','inputs':records,'outputs':outputs,'evidence':evidence,'self_checks':{'NativeFact_enum17_exact_order':True,'whole16_displayed_sections_equal_raw':True,'named_object_text_assertions':len(checks),'author_target_execution':False}}
(BASE/'source-pins.json').write_bytes((json.dumps(pins,indent=2)+'\n').encode())
for n in ['argument.md','argument.json','object-sections.json','source-pins.json']:print(n,pin(BASE/n)['sha256'])
