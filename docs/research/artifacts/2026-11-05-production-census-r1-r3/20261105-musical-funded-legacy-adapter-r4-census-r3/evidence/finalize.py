import hashlib,json,pathlib
BASE=pathlib.Path(__file__).resolve().parent.parent
ROOT=BASE.parent.parent
P=BASE/'census.json';d=json.loads(P.read_bytes())
events=d['events'];S=d['schedules'];f=d['formation_equations']
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def aff(a):
 out=str(a[0])
 for n,k in zip(a[1:],['B','d']):
  if n:out+='+'+(str(n)+'*' if n!=1 else '')+k
 return out
def table(headers,rows):
 return '\n'.join(['|'+'|'.join(headers)+'|','|'+'|'.join(['---']*len(headers))+'|']+['|'+'|'.join(str(x).replace('|','/') for x in row)+'|' for row in rows])

# Finite admission illustration, not a literal data definition or observed ABI.
B,R,ca,M,L,D,A,C=4264,1,2,9,8284,1,4,1
root=f['same_root_physical_fixed_constant_B_d'][0]+9*B
needed=f['variableCaptureFixed_constant_d'][0]+ca*f['perSearchRow_constant_d'][0]+R*f['perMapRow_constant_d'][0]+223
q=int(d['conditional_fit']['conditional_examples_only']['Q_for_R'][str(R)])
base=root+needed+q+(1+6*R)+4096+1024
d['conditional_fit']['conditional_examples_only']['all_success_inequality']={'conditions':{'B':B,'d':0,'R':R,'clipCount_plus_assetCount':ca,'M':M,'L':L,'D':D,'A':A},'source_upper_excluding_N_term':base,'C1_example_N1_total':base+24,'C1_max_N_by_sufficient_upper':(268435456-base)//24,'C2_max_N_by_sufficient_upper':(268435456-base)//40,'illustration_only_no_runtime_data_or_binding':True}
d['formation_equations']['needed_extreme_U64_upper_d1']=3272+268435456*972+536870912*609
d['formation_equations']['needed_extreme_not_admission_or_visits']=True
d['selected_R5_fundamental_type_identity_separate']=json.loads((BASE/'evidence/selected-R5-identity.json').read_bytes())
d['fund_partitions']={
 'root':['0..24 except check/body loci','23 Distribute','24 CaptureDispatch','77/78 result/refused','82/83 Snapshot','86 ReadoutCommit','87 NewFailureTest','89..92 session next','129 LegacyRootInstall','130 LegacyDistribute'],
 'captureCheck':'25..32 and actual initial capture reserve79..81',
 'variableCaptureWork':'33..76 plus131 LegacyCaptureOptions; map/parent held cleanup split before physical ownership',
 'roles':'84/85 and88 Control;93..99 Control;100 Worker;101 Reader;102..105 actual Worker/Reader;106/107 Control;108 Retained;109 Control;110/111 Retained;112..115 Control;116/117 Retained;118..125 actual dropping role including Result',
 'resourceCleanup':'126 map,127 parent,128 State;132/133/134 and six owner destroys/two cell destroys/new detach/abort cleanup use legacyCleanup',
 'legacyFixed':'All other new adapter/control/result ordinary checkpoints129..233 excluding root/capture/cleanup;234..237 quote;347..350 quote/outer call statuses;402 chain',
 'legacyCheck':'Actual query and three stage Ledger::reserve invokes,79..81 formal check argument',
 'legacyQuery':'238..346 except none quote,351..401 and403..419: matcher,read/check/restore,predict/search/prefix/vector/diagnostics/resolve bookkeeping',
 'queryCallee':'Only exact190 catalogue-selected old append or resolve vector. Its reservation protected before later restoration, no new bookkeeping may draw it.',
 'legacyAttempt':'Only three unchanged entered builder semantic fees; private prepared.index makes0/1/2 slots distinct. Builder incoming source copies/DMIs/callers belong legacyFixed.'}
d['same_origin_conservation']={
 'root_start':'One logical Origin root reservation BEFORE first actual argument/local/member write; after physical formation actual+unused imports once, scratch permanently dead.',
 'pay':'fund.remaining -= exact bound Site price; canonical reserved -=amount; actual +=amount; reached raw site count increments. No additional max-loop debit.',
 'split':'Same Origin/Phase reservation moves held slices; canonical phase bookkeeping only, no new allowance.',
 'attempt':'prepare proves headroom including prior legacy entered contribution; reserve admits current exact fee; immediately branchless commit then exactly one frozen builder; no cancellation/exception/check between commit and call.',
 'successive_cumulative':['Clock returned cumulative must equal1+6R already consumed','Request returned cumulative must equalprior+4096 already consumed','Plan returned cumulative must equalprior+1024+N*(8+16C) already consumed'],
 'protected_query':'prepareOld computes exact selected vector and splits it; commitOld consumes it once immediately before one call; future unentered packet never consumed.',
 'retire':'Unused lifetime/callee/attempt/query/fixed/check/cleanup remains terminal reserved classification, never returned as headroom or recounted actual.',
 'root_acyclic':'Helpers -> fixed Site vectors -> all explicit schedule bounds -> Bounds/root proposal -> actual immutable catalogue values. No runtime price factory, recursion or self-charging census.'}

# Current source pins first; verify subject still exact, then seal only this new packet.
pins=json.loads((BASE/'source-pins.json').read_bytes())
for entry in pins['subject']:
 pp=ROOT/entry['path'];assert digest(pp)==entry['sha256'] and pp.stat().st_size==entry['bytes'],entry['path']
refs=[
 'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r1/census.md',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r1/census.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r1/source-pins.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review/review.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review/source-pins.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r2/census.md',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r2/census.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r2/source-pins.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2/review.md',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2/review.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2/source-pins.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2/evidence/source-ledgers.json',
 'build-ci/20261105-production-operation-boundary-census-conventions.md',
 'build-ci/20261105-production-query-sdk-boundary-clarification.md',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-independent-review/review.json',
 'build-ci/20261105-musical-funded-legacy-adapter-r4-independent-review/review-pins.json',
 'build-ci/20261105-musical-origin-formation-r4-census-r2/census.json',
 'build-ci/20261105-musical-origin-formation-r4-census-r2/census.md',
 'build-ci/20261105-musical-window-query-path-census-r1/census.json',
 'build-ci/20261105-musical-window-query-path-census-r1-independent-review.json',
 'build-ci/20261105-musical-funded-r5-layout-object-r1/layout-record.json']
refs.extend([
 'build-ci/20261105-musical-funded-r5-conversion-identity-r1/identity.cpp',
 'build-ci/20261105-musical-funded-r5-conversion-identity-r1/identity-record.json',
 'build-ci/20261105-musical-funded-r5-conversion-identity-r1/source-plan.json',
 'build-ci/20261105-musical-funded-r5-conversion-identity-r1/attempt-r1/identity.obj',
 'build-ci/20261105-musical-funded-r5-conversion-identity-r1/attempt-r1/receipt.json',
 'build-ci/20261105-musical-funded-r5-conversion-identity-r1/first-green-whole-source-freeze.json'])
pins['reference_files']=[]
for rel in refs:
 pp=ROOT/rel
 if pp.exists():pins['reference_files'].append({'path':rel,'bytes':pp.stat().st_size,'sha256':digest(pp)})
 else:raise RuntimeError('Missing expected exact reference '+rel)
pins['state']='SEALED_SOURCE_ONLY_CENSUS_PROPOSAL_REQUIRES_INDEPENDENT_REVIEW'
pins['schema']='YESDAW_PRODUCTION_FUNDED_ADAPTER_R4_CENSUS_R3_PINS_V1'
d['self_checks']['subject_nine_file_pins_reverified_before_seal']=True
d['self_checks']['line_endings']='UTF8/LF for new authored artifacts; historical inputs hashed raw bytes'
d['self_checks']['no_target_execution']='No target C++/PowerShell parser,AST,import,compiler,linker,helper,program,test,stage,native,GUI,Git orCI invoked by this census agent. Evidence helpers are ordinary Python JSON/file/hash/integer arithmetic only.'
P.write_bytes((json.dumps(d,indent=2)+'\n').encode())

lines=['# R4 funded adapter — complete production census proposal R3','',
'**Complete finite proposal; independent numeric review, data binding and whole proof remain pending. Native, resource and executable-source admission remain HOLD.**',
'',
'This packet covers all420 ordered Sites in the exact nine-file R4 manifest `A5353D86D20DE812A75D6345780C6B489670DD61EB40AF87C4D2CE6776B28077`, plus the complete190-row accepted protected old-query catalogue. It applies production conventions `404E214B31AA1F1FA273224DCF28355C1DB65E3798635416D802702E2458613A` and the query/SDK clarification. It does not apply the separate Clock research tariff. The prior formation129 body mathematics is reused only in preserved partitions; changed Raw/receipt/role/copy/initializer/cleanup paths are explicitly re-expanded.',
'',
'R3 is a complete replacement proposal following whole R2 REVISE AD543B6DBDF99D2C471B7FAD9405B04ED367FA7EB778955EB113152C420600BB. Its new counting hypothesis uses complete ordered source operands for all23 affected bodies,113 statement groups and explicit earlier/callee/caller sponsorship. Uninitialized locals have no invented W; reference binding has no prepared-fee R; every written nested designator,builtin subscript,eager operator,named caller transfer and status guard remains actual. Full420 helper/body/B/d factorizations,51 helper cases,38 source schedules,all12 query terms,root and conditional examples are regenerated. Neither prior REVISE certified the remaining397 body prices. The whole new proposal requires fresh numeric review; signed errors cannot cancel into approval and no unused reserve supplies an operation fee.',
'',
'The prior getter corrections remain: six owned two-cell lvalue-return copies retain2R/2W plus getter and copy H/Z; two rows() returns atSites162/343 use distinct by-value span construction. Actual references remain references. No extra destination,prezero,DMI,vector copy,O or allocation is introduced. All R1/R2 and source seals remain exact.',
'',
'`evidence/ordered-operands.json` gives every component of the23 rebuilt bodies in113 lexical-source groups, with actual argument/parameter/SDK ownership and named earlier sponsors. Lexical ordering is not a claimed C++ evaluation trace. `evidence/all-420-factorizations.json` independently exposes every body,helper,conditional ABI vector,source anchor and caller case. These are complete proposals, not a positive certificate borrowed from an earlier negative review.',
'',
f"`census.json` contains all ten-component body/helper/ABI vectors, every current source callsite anchor, concrete caller cases,420 prices,190 exact old vectors,{len(S)} explicit finite schedules and the complete root/role/Q equations. `source-pins.json` pins subject/reference/evidence files and the two census outputs. This markdown is an index and readable numeric worksheet; the JSON provides the independently recomputable detail.",
'',
'## Ordered body and caller ownership changes','',
table(['Site','Prior R2','Rebuilt R3','Body statement groups'],[(x['site'],aff(x['old']),aff(x['new']),len(next(r for r in json.loads((BASE/'evidence/ordered-operands.json').read_bytes())['rows'] if r['site']==x['site'])['ordered_body_operands'])) for x in d['correction']['source_sized_effects']]),
'',
'The first amount write is in QueryQuote, not reserveQuery. CommitPrepared owns its17 real prepared/phase/updated-cell reads; its const-reference caller supplies none of those again. ReceiveWorker owns the actual receive selector before LegacyWorkerCaller. Raw Access/getter helpers exclude each written caller designator. QuerySearchEntry/PrefixEntry/PredictEntry own parameter destinations, while the named caller reads remain in the source caller. PartialNext/LastArgs already own the append out.frame selector and pre-entry AppendPrefix actions. Concrete QueryPacket/VectorEntry cases already own their path/id/times/ternary inputs; none is duplicated in the surrounding partial body.',
'',
'## Boundary and copy ownership','',
'Vector order is `(R,W,C,X,LE,H,Z,O,A,F)`; scalar price is its sum. Constants and constant-only expressions have no runtime R/X. A scalar caller read and parameter destination are one transfer, not two incoming reads. Actual value aggregates use every scalar source/destination cell and selected copy/assignment H/Z; reference binds do not copy referents. Each written nested member/subscript/deref/address/nontrivial cast is X. Unqualified member calls have no invented selector or implicit-this transfer. All owned control/loops/firstwrites/parameters/native POD initialization/return paths are expanded. SDK calls keep their visible logical H/Z, parameters/results/extent/access and actual caller actions; internal Win32/intrinsic/CRT/SDK CPU/output writes are not recursively invented or certified zero.',
'',
table(['Aggregate','Scalar cells'],d['copy_cells'].items()),
'',
'`LegacyReadout result{}` and brace RHS `{}` initialize125 cells total:121 zero initializations plus4 nested status DMIs, no initialization constructor H/Z. Default-init `readout_` before Cell/Take bodies instead executes4 DMIs and3 implicit constructor H/Z; actual Arm/Take caller sponsors them before the later body debit. Common/requested and PreparedAttempt/limits each have2 prebody DMIs sponsored chain and actual stage callers. No raw inline byte storage is silently pre-zeroed. Root explicit byte zero loop remains `B=sizeof(Raw)`; padding is not treated as scalar fields.',
'',
'Native leaves include our NativeControl3/Module3/Image3/Page8/Snapshot13 POD zero/copy/return actions and actual MODULEINFO3/MBI8 value-init before API calls. Page copies every source info field even API refusal. API opaque behavior does not create extra logical output-field fees or imply zero CPU. NativeRead/Restore covers errno/fenv/MXCSR/CRT/new-handler/FMA getter visible boundaries; actual CPU/footprints/control neutrality remain unbound native qualifications.',
'',
'## Finite fixed schedules and root','',
table(['Held schedule','Derived upper'],[(n,aff(S[n]['price_constant_B_d'])) for n in ['DropNonLast','DropLastRole','StateCleanup','MapCleanup','ParentCleanup','WorkerOrReaderRole','RetainedRole','ControlRole','ResultRole','CaptureCheck','LegacyFixed','LegacyCheck','LegacyCleanup','Snapshot']]),
'',
'These are reservation bounds, never presumed actual visits or extra debits. Finite unions deliberately hold mutually exclusive source exits. The concrete path consumes only its reached checkpoints. Control uses complete administrative/acquire/abort/session alternatives. The original Worker/Reader874 schedule only covers their original check-only receiver source; new adapter/cancel/caller/return actions use LegacyFixed. The new adapter receiver concrete case removes the original check-only wrapper entry/return and absent selector; the fixed shared Site price remains the greater original case.',
'',
'All five role lifetime reservations coexist. Control+Worker can become Control+Worker+Result (maximum3 references). Worker drops; Take transfers the SAME Result reference to one detached cell. After Result physically destructs, Result origin drops and `resultOriginLive` clears; Control can then issue Retained and eventually Reader. Result and Retained/Reader are not mutually exclusive across the whole lifetime. At most two physical result cells destruct, one owned and one borrowed; at most six stage/output owners each physically destruct once. Cleanup occurs while the genuine source context is live, before any last State free.',
'',
f"Current snapshot={f['branch_expansions']['snapshot']}; postcapture={f['postcapture_fixed']}; failed initial tail={f['failed_initial_tail']}; failed next tail={f['failed_next_tail']}. Physical fixed prefix=`9*B+{f['fixed_physical_prefix_constant_B_d'][0]}`. Add StateCleanup, CaptureCheck, all five roles, LegacyFixed, LegacyCheck and LegacyCleanup:",
'',
f"`rootEnvelope = {aff(f['same_root_physical_fixed_constant_B_d'])}`; no-state bound=`{aff(f['no_state_constant_B_d'])}`. These are proposed immutable Bounds values, not an actual definition or selection of a new cap.",
'',
f"Capture variable fixed=`{f['variableCaptureFixed_constant_d'][0]}+d`; per search row=`{f['perSearchRow_constant_d'][0]}+2*d`; per map row=`{f['perMapRow_constant_d'][0]}+d`; held resource cleanup116+107. Thus `needed=3271+d+(clipCount+assetCount)*(970+2*d)+R*(608+d)`. The extra123 is the one actual captured LegacyPreparationOptions copy. All old found/missing/invalid loop schedules remain source-shaped; early failures stop future bodies. Even domain extrema give needed<2^40; that arithmetic upper is not admitted or visited work.",
'',
'The dependency order is acyclic: helper vectors -> event prices -> drop/resource/role/check/fixed schedules -> root. Initial root funds exist before physical request/argument/local writes. Every later capture/query/semantic reservation scans the same canonical Origin; splits only move held funds, pay transfers reserved to actual, and retire never refunds unused schedules.',
'',
'## Complete twelve-term query/native reserve','',
'Let `P=max(R-1,0)`, `b=bit_width(R)`, `E=R?R+1:0`. M/L are compared immutable region/byte extents; D/A are address-only mutable getter extents. Definitions remain absent. The source checks profiles before walks; successful address profile requires aligned4-byte regions and A=4D. No mutable dispatch value is compared.',
'',
table(['Unit field','Source-derived upper','Multiplier'],[(x['unit_field'],aff(x['derived_price']),x['count']) for x in d['query_quote_12_terms']['terms']]),
'',
'The quote calls all12 checked terms even after refusal. A refused term leaves total unchanged and no product overflow is evaluated. At actual admission/call, old append vectors are selected exactly (AS_JUMP61, FLAT64, RAMP94 and all refusal cases retained); old resolve actual packets are rebuilt from the exact190 rows. The old94 and970+72b ceilings are only held reserves. `prepareOld` sums the selected10-cell vector and protects it into queryCallee; later native restoration/bookkeeping cannot consume it. Branchless commit immediately precedes the one real old callee, and only that exact amount becomes actual once.',
'',
'Two binary searches each have at most b positive iterations. The two endpoints share one monotone prefix cursor, so total full prefixes<=R-1; each endpoint has at most one partial append. Every append has exactly the three potential source checks, one selected old invocation and its record. Per endpoint at most12 packets and2 direct vector adds; predict adds at most3 packets/2 direct adds. Each vector add/sum/record has10 bodies,11 tests,10 increments and one entry/end. Full component Site multiplicities are in the named JSON schedules, not convenience multipliers.',
'',
'Native matching executes once before Clock. Identity has17 facts,32 digest bytes each,18 outer tests and17*33 inner tests. Each positive completed page chunk advances at least1 byte: compared pages<=L, access-only pages<=A, compared byte bodies<=L. Region fixed tails and final tests are included in per-region/fixed coefficients; last refusal paths are covered. Shared region/page coefficients are the source-derived maximum of compared/access cases, not arbitrary power or ABI-sized guesses.',
'',
'Fixed nonappend check upper is9: Clock before/after2; endpoint extra checks<=4; explicit/destructor restoration prechecks<=2; post-resolve1. Two partial appends separately own6 checks; full prefixes own3 each. Restoration can have two calls/prechecks if the explicit precheck fails and leaves restoring true. There is at most one actual restoreStatus call because it clears restoring before the result branch. Destructor/refusal/catch paths are included; no retry/reset allowance is added.',
'',
'## Same-Origin cap and remaining holds','',
'Sufficient all-success inequality under the unchanged cap is:',
'',
'`rootEnvelope + needed + Q(R,M,L,D,A) + (1+6*R) +4096 +(1024+N*(8+16*C)) <= 268435456`.',
'',
'All three builder fees keep ADR0089 meanings and occur once only at actual entries. Successively inherited cumulative checks equal already-consumed Clock, then Request, then Plan totals; they are not extra debits. Refused/skipped calls consume none of their future callee fee. LegacyCheck holds exactly query+three attempt reserve scans, separately from capture. LegacyFixed holds all three prepare/total six-phase scans, five cancellation loads, at most one cancellation-taken/refusal/stop, at most nine ready calls, and every actual caller/staging/result path.',
'',
f"Conditional arithmetic illustration only: B4264,d0,R1,clip+asset2 and unbound candidate M9/L8284/D1/A4 give root{root:,}, needed{needed:,}, Q{q:,}; total before Plan per-frame term{base:,}. C1,N1 gives{base+24:,}<268435456; sufficient N ceiling is{(268435456-base)//24:,} for C1 or{(268435456-base)//40:,} for C2. The4264 value is separately object-qualified for selected R5, not measured R4 layout or offset equality. d specifically denotes U64/size_t nonidentity conversion and remains conditional; d0 is an illustration, not an alias observation or destruction term. No selected Data definition, actual trace, native/full-resource fit, Ready or execution admission follows. Counts/limits may force ordinary refusal; no cap was changed.",
'',
'A separate selected-R5 compile-only constant now establishes fundamental-type identity d0 for that exact selected compiler/header profile: the independently read COFF object23D37F717E962DD599864074771290E531B6B258C4EE6CEE6D99C1B0F15AE516 contains only the expected readonly tagged table words(tag,1,0), no relocations,initializer sections or emitted code. Its319-byte source uses exactly std::is_same_v<std::uint64_t,std::size_t>. The independent extraction is retained as evidence/selected-R5-identity.json. This separate R5 fact does not change any generic d vector/equation, assert R4 layout/offset equality, define Data or qualify execution/resource fit.',
'',
'R4 has a separate concrete executable-source HOLD: parent reports actual MSVC `/W4 /WX` full-header compile exit2 C4324 at formation-r8.h1345 for aligned resultBytes_ after exchange/flags. R4 is preserved. Parent reports R5 minimal declaration reorder compiles0 and37 public sizeof/align constants. That only supplies separate R5 public ABI evidence; no unchanged offsets/R4 correspondence/resource fit is borrowed here.',
'',
'Whole CPU/SDK/native behavior, no-reachable-writer profile, strict codegen/binary64 prefix/forward/query proof, parent pointer-control ownership/liveness, real task launch/join/cancel/acquire, TSan, allocator/control-block/complete retained graph/stack/session/resource pool and whole public recipe proof remain HOLD. Declared sizeof+64 subtotals are not complete allocation evidence. NativeBinding/Catalogue/OldQueryCatalogue definitions and prices remain unbound. Historical unmetered getters still need actual consumer sponsorship; this finite packet adds no polling allowance.',
'',
'## Mechanical self-checks and complete event worksheet','',
f"Source-only arithmetic rechecked420 ordered event rows, all helper+body sums, all B/d vector sums,190 old catalogue rows,{len(S)} finite schedules,12 quote terms and nine subject raw-byte seals. Evidence Python performs ordinary file/JSON/hash/integer arithmetic only. This agent ran no target parser/AST/import/compiler/linker/helper/program/test/stage/native/GUI/Git/CI. Parent compile findings are explicitly separate lineage. All prior packets and target sources remain untouched.",
'',
'Each row below is `helper coefficients + body +B-vector*B+d-vector*d`; full vectors and every source callsite are in JSON. Row190 is declared but unentered (no `Site::LegacyLocalOwnerReturn` reference), not a zero-price reached event. Site21 also has source-excluded count0 for valid failed no-state retiring invariant.',
'',
table(['#','Site','constant+B*B+d*d','source anchors'],[(e['ordinal'],e['site'],aff(e['price_coefficients']),', '.join(a['file']+':'+str(a['line']) for a in e['source_anchors'])) for e in events]),
'',
'## Helper worksheet','',
table(['Helper','R,W,C,X,LE,H,Z,O,A,F','Price'],[(name,','.join(map(str,h['tuple'])),h['price']) for name,h in d['helpers'].items()]),
'',
'## Finite schedule index','',
table(['Schedule','constant+B*B+d*d','Site multiplicities in JSON'],[(name,aff(s['price_constant_B_d']),'; '.join(k+('*'+str(n) if n!=1 else '') for k,n in s['site_counts'].items())) for name,s in S.items()]),
'',
'This is a complete proposed production scalar/schedule packet for independent numeric review. It grants no runtime or production admission.',
'']
(BASE/'census.md').write_bytes(('\n'.join(lines)).encode())
pins['authored_evidence']=[]
for pp in sorted((BASE/'evidence').iterdir()):
 if pp.is_file():pins['authored_evidence'].append({'path':pp.relative_to(ROOT).as_posix(),'bytes':pp.stat().st_size,'sha256':digest(pp)})
pins['outputs']=[{'path':(BASE/n).relative_to(ROOT).as_posix(),'bytes':(BASE/n).stat().st_size,'sha256':digest(BASE/n)} for n in ['census.md','census.json']]
(BASE/'source-pins.json').write_bytes((json.dumps(pins,indent=2)+'\n').encode())
for n in ['census.md','census.json','source-pins.json']:
 pp=BASE/n;assert b'\r' not in pp.read_bytes();print(n,pp.stat().st_size,digest(pp))
