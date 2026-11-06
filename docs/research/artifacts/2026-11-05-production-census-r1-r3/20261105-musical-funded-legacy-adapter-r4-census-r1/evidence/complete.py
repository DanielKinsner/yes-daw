import json, pathlib
BASE=pathlib.Path(__file__).resolve().parent.parent
P=BASE/'census.json'
d=json.loads(P.read_text())
ORDER=d['vector_order']
def v(**kw): return [kw.get(k,0) for k in ORDER]
def add(*vs): return [sum(x) for x in zip(*vs)]
def helper(name,a,why): d['helpers'][name]={'tuple':a,'price':sum(a),'source':why,'d_vector':v()}
def row(name,a,co=None,why='',dv=None,cases=None):
    co=co or {'Fixed':1}
    e=next(x for x in d['events'] if x['site']==name)
    total=add(a,*[[n*x for x in d['helpers'][h]['tuple']] for h,n in co.items()])
    dd=add(dv or v(),*[[n*x for x in d['helpers'][h].get('d_vector',v())] for h,n in co.items()])
    e.update(status='PROPOSED_RECONCILED_PENDING_FRESH_INDEPENDENT_REVIEW',body_vector=a,helper_coefficients=co,constant_vector=total,B_vector=v(),d_vector=dd,price_coefficients=[sum(total),0,sum(dd)],source_derivation=why)
    if cases:e['caller_cases']=cases
def save():
    d['current_reconciled_rows']=sum(e['status']!='UNRECONCILED' for e in d['events'])
    P.write_bytes((json.dumps(d,indent=2)+'\n').encode())

helper('BOOL_HANDLE',v(R=1,C=1,H=1,Z=1),'Frozen public shared-owner boolean logical boundary; no pointee copy.')
helper('ARROW_HANDLE',v(R=1,X=1,H=1,Z=1),'Frozen shared-owner arrow logical boundary; returned pointer consumed at actual written caller.')
helper('HDEFAULT',v(H=1,Z=1),'Visible trivial-empty/shared-owner default constructor logical entry/return.')
helper('HDROP',v(H=1,Z=1,O=1),'Exact accepted catalogue HDROP: logical destructor plus one drop O, even empty; no recursive SDK implementation.')
helper('HMOVEASSIGN',v(H=1,Z=1,O=1),'Exact accepted catalogue HMOVEASSIGN: logical assignment and one destination-drop O, even empty.')
helper('WINDOW_CTOR',v(W=5,H=2,Z=2),'Frozen plain Window default constructor incl all five scalar DMIs and nested default handle.')
helper('WINDOW_DTOR',v(H=2,Z=2,O=1),'Exact accepted WINDOW_DTOR: own/member H/Z and one actual handle-drop O.')
helper('MAX2',v(H=1,Z=1),'Selected public container max/size/getter logical boundary, no constant read fee.')
helper('EXT3',v(R=1,H=1,Z=1),'Exact accepted EXT: one logical container extent/address read; caller selector outside.')
helper('MoveRef',v(X=1,H=1,Z=1),'Selected std::move body returns explicit reference cast, one H/Z; no referent copy or object construction.')
helper('ReadoutPrebodyDMI',v(W=4,H=3,Z=3),'LegacyReadout has four prebody boolean DMIs and two nested status default constructors plus aggregate constructor; sponsored actual Arm/Take before cell body.')

for name,a,co,why in [
('LegacyRootInstall',v(R=38,W=36,C=4,X=54,LE=1,H=8,Z=8),{'Pdirect':1,'E':8},'Five empty slice assignments plus three bounded attempt initializations; all four live/state flags and six loop tests. Contribution first write is in RootInstall, not duplicated.'),
('LegacyDistribute',v(R=3,X=9),{'Pdirect':1,'S':3},'Three typed splits for fixed/check/cleanup. Original Distribute separately owns its return/tail.'),
('LegacyCaptureOptions',v(R=13,W=15,X=4,H=2,Z=2),{'Pdirect':1},'Thirteen-cell options copy/placement constructor, two live/raw writes; no referenced source-owner copy.'),
('LegacyFinalProbe',v(R=1,C=1,X=1),{'Pdirect':1},'Actual parent-live guard; constant branch operand has no R.'),
('LegacySlicesRetire',v(R=19,W=16,C=4,X=50,LE=1),{'Pdirect':1,'T':8},'Eight actual typed retire calls: query, protected, three attempts, fixed, check, cleanup; three-element loop tests/updates plus flags.'),
('LegacyArtifactsDestroy',v(R=1,X=2,H=1,Z=1),{'Pdirect':1},'Actual WorkerArtifacts destructor entry/return/state member alias, member drops separately funded.'),
('LegacyArtifactsConstruct',v(W=1,X=5,H=1,Z=1),{'Fixed':1},'WorkerArtifacts constructor state reference and written member initializers; three stage owner constructors separately funded.'),
('LegacyEntry',v(R=12,W=127,C=11,X=26,H=1,Z=1),{'Fixed':1},'Run own H/Z, actual state alias and eleven invariant guards, legacyIssued store, full125-cell result value-init and stage overwrite.'),
('LegacyBudgetEntry',v(R=1,W=3,X=3,LE=1,H=1,Z=1),{'Fixed':1},'Total() own H/Z, six-phase scan entry and three local first writes.'),
('LegacyBudgetTest',v(R=2,W=1,C=1,X=3),{'Fixed':1},'Six-phase fixed loop test and local phase index.'),
('LegacyBudgetRow',v(R=7,W=3,X=13),{'Fixed':1,'D':2},'Two checked adds for phase actual/reserved, reads and destinations once.'),
('LegacyBudgetContinue',v(R=2,W=2,X=4),{'Fixed':1},'Phase scan accumulator assignment and loop increment.'),
('LegacyBudgetEnd',v(R=3,W=2,X=6),{'Fixed':1},'Final original cumulative aggregate fields and selected return.'),
('LegacyRefusal',v(R=1,W=2,X=1,H=1,Z=1),{'Fixed':1},'Reason parameter W, named reason R, result.outcome W, own H/Z; constant false return R0.'),
('LegacyStop',v(H=1,Z=1),{'Fixed':1},'Own stop logical entry/return; constant false R0.'),
('LegacyReady',v(H=1,Z=1),{'Fixed':1},'Own ready logical entry/return; constant true R0.'),
('LegacyCancel',v(R=5,W=3,C=1,X=6,H=2,Z=2),{'Fixed':1},'Actual cancellation wrapper, context.cancelled caller/return and raw counter; context body separately source-funded.'),
('LegacyCancellationTaken',v(W=1,X=1),{'Fixed':1},'Taken cancellation outcome overwrite; constant value R0.'),
('LegacySucceeded',v(W=1,X=1),{'Fixed':1},'Final successful outcome overwrite; constant value R0.'),
('LegacyAttemptKind',v(R=1,C=1),{'Fixed':1},'Stage Clock test with named stage read once.'),
('LegacyAttemptFeeLimit',v(R=3,C=1,X=1),{'Fixed':1},'Fee versus remaining effective work; two named limits plus subtraction.'),
('LegacyClockEligibility',v(R=3,W=1,C=1,X=3),{'Fixed':1,'Math1':1},'Named row-count bit_width eligibility; constant-only arithmetic/casts do not execute; two possibly nonidentity size_t/U64 casts qualified separately.'),
('LegacyAttemptReady',v(R=7,W=6,X=4,H=1,Z=1),{'Fixed':1,'Minimum':1},'Prepared index/fee and two-field limits aggregate assignment with one selected assignment edge; actual limit minimum call.'),
('LegacyByteHeadroom',v(R=1,C=1,X=2),{'Fixed':1},'Byte-cap headroom test; constant caps have no R.'),
('LegacyByteCompute',v(R=2,W=1,X=6),{'Fixed':1,'Minimum':2},'Nested actual byte minima and common byteLimit first write.'),
('LegacyClockExtent',v(R=2,C=2,X=3),{'Fixed':1},'Empty and maximum tempo row extent predicates.'),
('LegacyClockBytes',v(R=2,C=1,X=4),{'Fixed':1},'Clock storage capacity subtraction/product compared against admitted limit.'),
('LegacyClockByteStore',v(R=1,W=1,X=4),{'Fixed':1},'Computed clock byte count store after proved multiplication bounds.'),
('LegacyClockAllocatorLimits',v(R=2,C=2,X=3,H=4,Z=4),{'Fixed':1},'Two empty allocator constructors and two max_size calls. MSVC allocator destructor is trivial: no synthetic destructor edges.'),
('LegacyCommonIdentity',v(R=10,W=4,C=5,X=16,H=2,Z=2),{'Fixed':1,'Access':2,'Math1':1},'Common owns scalar checks and metadata/options access; references do not copy metadata or options.'),
('LegacyMappedPresent',v(X=2),{'Fixed':1,'Access':1,'BOOL_HANDLE':1},'Captured parent mapped-owner validity; reference accessor and shared-owner bool.'),
('LegacyMappedLayout',v(R=5,W=1,C=3,X=4),{'Fixed':1,'ARROW_HANDLE':1},'Mapped channels and selected source-window scalar domain.'),
('LegacyMappedExtent',v(R=7,W=2,C=3,X=6,H=1,Z=1),{'Fixed':1,'ARROW_HANDLE':2,'MAX2':1},'Mapped frame/window extent checks, container extent boundary, scalar locals.'),
('LegacyMappedProduct',v(R=5,C=2,X=6),{'Fixed':1,'ARROW_HANDLE':3,'EXT3':1},'Mapped element-count product and nonempty byte/address conditions.'),
('LegacyCellConstruct',v(R=130,W=258,C=3,X=22,H=4,Z=4),{'Pdirect':1},'State pointer plus four flags,125-cell temporary zero-init and125R/W assignment, placement PlanOwner and two counters. Four prebody DMIs sponsored by Arm.'),
('LegacyCellMove',v(R=137,W=138,C=6,X=46,H=3,Z=3),{'Pdirect':1,'Access':1,'MoveRef':1,'HDEFAULT':1},'Possible125-cell history assignment, plan move constructor, source flags and five counters. Take sponsors prebody DMIs; no plan allocation or referent copy.'),
('LegacyCellDestroy',v(R=6,W=2,C=3,X=15,H=1,Z=1),{'Pdirect':1,'Access':1,'HDROP':1},'All result destructor flag guards and actual plan drop before origin release. Separate owned/borrowed cleanup rows follow.'),
('LegacyCellDestroyOwned',v(R=3,W=5,C=1,X=9),{'Pdirect':1},'Owned origin flag/raw/reference bookkeeping before actual RoleResult release; state is not accessed after possible last free.'),
('LegacyCellDestroyBorrowed',v(R=1,W=1,C=1,X=2),{'Pdirect':1},'Borrowed-cell completion flag update and boolean guard.'),
('LegacyCellRetainOrigin',v(R=13,W=6,C=10,X=32,H=1,Z=1),{'Pdirect':1},'All ten retained-result/state invariant predicates, origin and ref-count updates; bound remains three simultaneous origin owners.'),
('LegacyCellPublish',v(R=222,W=223,X=15,H=2,Z=2),{'Pdirect':1},'Actual94-cell raw copy followed125-cell whole history copy, two selected assignments, completion and counters.'),
('LegacyResultPlanMove',v(R=1,W=1,X=5),{'Fixed':1,'Access':1,'MoveRef':1,'HMOVEASSIGN':1},'Destination plan accessor and actual shared-plan move assignment, ownerMoves counter; moved pointee not copied.'),
('LegacyControlResultProbe',v(R=1,C=1,X=1),{'Pdirect':1},'Actual resultLive test.'),
('LegacyControlResultDestroy',v(W=1,X=2),{'Pdirect':1,'Access':1},'Actual result accessor/explicit destruction and resultLive clear. Cell destructor body separately funded.'),
('LegacyControlResultDetach',v(R=1,W=1,C=1,X=2),{'Pdirect':1},'Upper true-case result flag branch/clear; false case has no write.'),
('LegacyArmResult',v(R=1,W=6,C=1,X=4,H=4,Z=4),{'Pdirect':1},'Four implicit prebody result DMIs, three default ctor edges, actual placement edge, resultLive and guard; cell body separate.'),
('LegacyAbortResult',v(R=1,W=1,C=1,X=3),{'Pdirect':1,'Access':1},'Actual failed-owner resultLive guard, accessor and clear; explicit destructor body separate.'),
('LegacyTakeResult',v(R=4,C=2,X=8,H=1,Z=1),{'Pdirect':1,'Q':1,'Access':1,'ReadoutPrebodyDMI':1},'Actual Take guards, owner selector, raw accessor, move call and all prebody four DMIs sponsored before body.'),
('LegacyRetainedResultCheck',v(R=1,C=1,X=1),{'Pdirect':1},'Checks current resultOriginLive only; after result destruction a retained owner may be issued sequentially.'),
('LegacyFinish',v(R=126,W=126,X=5,H=2,Z=2),{'Fixed':1},'Counter plus actual125-cell lvalue history return copy; outer prvalue return elided; publish body separately funded.'),
('LegacyWorkerCaller',v(R=2,C=1,X=5,H=1,Z=1),{'Pdirect':1,'Access':1},'New worker wrapper own entry/return, context alias and resultLive guard/accessor; receive/context bodies retain their own source funding.')
]:row(name,a,co,why)
next(e for e in d['events'] if e['site']=='LegacyClockEligibility')['d_vector']=v(X=2)
next(e for e in d['events'] if e['site']=='LegacyClockEligibility')['price_coefficients'][2]=2

for name in ['LegacyClockOutputConstruct','LegacyRequestOutputConstruct','LegacyPlanOutputConstruct','LegacyClockStageConstruct','LegacyRequestStageConstruct']:
    row(name,v(R=1,W=3,X=8,H=3,Z=3),{'Pdirect':1},'Actual state reference initializer and raw placement handle default construction, live flag/counter; selected default ctor logical edges only.')
row('LegacyWindowStageConstruct',v(R=1,W=8,X=8,H=4,Z=4),{'Pdirect':1},'Window five scalar DMIs plus nested handle default construction; no duplicate prezeroing.')
for name in ['LegacyClockOutputDestroy','LegacyRequestOutputDestroy','LegacyPlanOutputDestroy','LegacyClockStageDestroy','LegacyRequestStageDestroy']:
    row(name,v(R=2,W=1,X=7,H=1,Z=1),{'Pdirect':1,'Access':1,'HDROP':1},'Physical shared-owner drop while genuine Worker context remains live; own guard/counter and raw accessor included.')
row('LegacyWindowStageDestroy',v(R=2,W=1,X=7,H=1,Z=1),{'Pdirect':1,'Access':1,'WINDOW_DTOR':1},'Physical Window/member handle destruction while genuine Worker context remains live.')

# Correct source selector partition in already reconciled binary search rows.
for name,delta in [('QuerySearchFalse',1),('QuerySearchEnd',-1)]:
    e=next((e for e in d['events'] if e['site']==name),None)
    if e and not e.get('selector_partition_corrected'):
        e['body_vector'][3]+=delta;e['constant_vector'][3]+=delta;e['price_coefficients'][0]+=delta;e['selector_partition_corrected']=True
save()
print('reconciled',d['current_reconciled_rows'],'remaining',420-d['current_reconciled_rows'])

helper('GetterScalar',v(R=1,H=1,Z=1),'Visible selected scalar getter: one actual stored scalar read and one entry/return; actual written caller selector outside.')
helper('GetterReference',v(H=1,Z=1),'Visible selected reference getter: entry/return and no referent copy; actual caller selector outside.')
helper('GetterTwo',v(R=2,W=2,H=1,Z=1),'Visible two-scalar value getter return; both source reads/return writes and logical entry/return.')
helper('WindowValid',v(R=1,C=1,H=2,Z=2),'Window.valid visible entry/return and its handle/null comparison boundary; caller selector separate.')
helper('OwnerBefore',v(R=2,C=1,H=1,Z=1),'Visible shared-owner ordering boundary, two logical stored owner reads/comparison and H/Z; no recursive SDK algorithm.')

for name,a,co,why in [
('LegacyQueryReserve',v(R=2,W=1,C=2,X=3,H=1,Z=1),{'Fixed':1},'reserveQuery own H/Z, c.rows argument read/selector and quote result guard, amount local, actual reserve amount read and two typed slice selectors/condition. Reserve callee own parameters/body in ReserveScan.'),
('LegacyQueryReserveRefused',v(),{'Fixed':1},'Refusal branch own fixed call only; refuse callee separately funded.'),
('LegacyAttemptPrepare',v(R=7,W=6,C=4,X=8,H=1,Z=1),{'Fixed':1},'Four scalar parameters, actual stage-to-index cast/local, three invariants and existingTotal result/guard; total() own rows separate.'),
('LegacyAttemptLimit',v(R=13,W=4,C=3,X=6),{'Fixed':1,'Minimum':3},'Headroom, prior contribution, origin ceiling, three actual minima, both resource predicates; no constant read fees.'),
('LegacyAttemptReserve',v(R=2,C=1,X=3),{'Fixed':1},'Actual index/fee arguments and check/attempt slice selectors; reserve callee owns scalar destinations and complete six-phase scan.'),
('LegacyAttemptReserveRefused',v(),{'Fixed':1},'Refused attempt branch only; ordinary refusal callee owns reason argument/store.'),
('LegacyClockPrepare',v(R=6,W=7,C=3,X=11,H=4,Z=4),{'Fixed':1,'Access':2},'Clock own entry/return; stage, map/rate/fee locals; output/metadata raw access; PreparedAttempt+limits two DMIs and two prebody ctor edges; three pre-call guards. Named size_t cast qualified d.'),
('LegacyClockCommitInvoke',v(R=6,W=6,X=1,H=4,Z=4),{'Fixed':1,'CommitPrepared':1},'Actual prepared-fee caller read then branchless transfer; map2/rate1/limits2 by-value source/destination cells and three selected copies; old call visible H/Z; status local. Semantic1+6R charged once separately.'),
('LegacyClockReturned',v(R=3,W=3,C=2,X=7),{'Fixed':1},'Return counter/status-known/status stores, post-call native.check selector/guard and old status predicate.'),
('LegacyClockStore',v(R=1,W=1,X=6),{'Fixed':1,'BOOL_HANDLE':1,'Access':1,'MoveRef':1,'HMOVEASSIGN':1},'Output validity then actual staged clock move and ownerMoves update; no copied Clock pointee.'),
('LegacyClockCumulative',v(R=7,W=5,C=3,X=26),{'Fixed':1,'Access':3,'ARROW_HANDLE':3,'GetterScalar':2,'GetterReference':1,'EXT3':1},'All three staged clock accessor/getter chains, stored work local, two counters, all three cumulative/bytes/row comparisons, returned evidence and cumulative stores.'),
('LegacyClockException',v(R=1,W=2,X=6),{'Fixed':1},'Entered catch counter/outcome and actual native.check caller selector. Read/check bodies remain separately query funded.'),
('LegacyClockFailure',v(R=1,C=1),{'Fixed':1},'NumericDomain branch status read/test.'),
('LegacyClockNumericDomain',v(R=1,W=2,X=6),{'Fixed':1},'Source-proved semantic fee observation: two selected observation stores and named fee read, not returned-owner evidence.'),
('LegacyQueryPrepare',v(C=1,H=1,Z=1),{'Fixed':1},'Window stage own entry/return and cancellation guard.'),
('LegacyQueryInvoke',v(X=4),{'Fixed':1,'Access':2},'Actual clock/window staged raw accessors; called QueryCallResult is initialized by its returned direct aggregate, no phantom extra copy.'),
('LegacyQueryReturned',v(R=1,C=1,X=1),{'Fixed':1},'Actual called.invoked guard.'),
('QueryCallNotEntered',v(),{'Fixed':1},'Not-entered wrapper branch; stop callee separately funded.'),
('QueryCallEntered',v(R=5,W=2,C=4,X=9),{'Fixed':1},'Known/window-status stores, three qualification/reconciliation predicates and returned status test, all actual called/result field reads.'),
('QueryCallOuterFailure',v(),{'Fixed':1},'Outer failure branch only; separately funded stop.'),
('LegacyRequestPreflight',v(R=2,W=1,C=2,X=8,H=1,Z=1),{'Fixed':1,'Access':2},'Request own H/Z, stage, options/window references and two policy-field checks. Referenced captured options/window are not copied.'),
('LegacyRequestWindow',v(X=1),{'Fixed':1,'WindowValid':1},'Retained-window actual valid caller selector and visible boundary.'),
('LegacyRequestBytes',v(R=7,W=2,C=2,X=5),{'Fixed':1,'GetterReference':2,'ARROW_HANDLE':2,'GetterTwo':1,'GetterScalar':1,'Minimum':2},'Actual clock limits/bytes getter chains, byte minimum/local, both resource predicates. sizeof+64 declarations remain constant declared subtotals.'),
('LegacyRequestByteTail',v(R=3,W=1,C=1,X=2),{'Fixed':1},'withKey addition/local then requestBytes versus remaining byte capacity; constants R0.'),
('LegacyRequestPrepare',v(R=13,W=13,C=2,X=8,H=5,Z=5),{'Fixed':1,'Access':3},'Metadata/parents references, mapped2/tier1/identity6/policy2 local copies, output reference, PreparedAttempt+limits DMIs, inherited scalar arguments and two pre-call guards.'),
('LegacyRequestCommitInvoke',v(R=14,W=13,X=5,H=5,Z=5),{'Fixed':1,'CommitPrepared':1},'Branchless semantic transfer then actual mapped2/tier1/identity6/policy2/limits2 by-value caller cells/copies plus four referenced metadata/parent designators and prepared limits selector. Frozen4096 semantic fee separate once.'),
('LegacyRequestReturned',v(R=5,W=6,C=1,X=14,H=1,Z=1),{'Fixed':1},'Actual returned counter/known/result2/evidence/cumulative stores and old status.status test; one selected two-cell result assignment.'),
('LegacyRequestStore',v(R=1,W=1,X=6),{'Fixed':1,'BOOL_HANDLE':1,'Access':1,'MoveRef':1,'HMOVEASSIGN':1},'Actual shared-request owner move and ownerMoves counter after validity guard.'),
('LegacyRequestCumulative',v(R=6,W=5,C=1,X=18),{'Fixed':1,'Access':1,'ARROW_HANDLE':1,'GetterReference':1},'Staged request accounting reference, work local, two counters, actual cumulative equality and returned-owner evidence/stores.'),
('LegacyRequestException',v(R=1,W=2,X=5),{'Fixed':1},'Actual catch counter and outcome stores; no native check is called in this catch.'),
('LegacyPlanPreflight',v(R=4,W=2,C=4,X=10,H=1,Z=1),{'Fixed':1,'Access':1,'BOOL_HANDLE':1,'ARROW_HANDLE':3,'GetterTwo':1,'GetterScalar':1,'GetterReference':1,'WindowValid':1},'Plan own H/Z/stage, retained input, actual two-cell policy/one-cell channel getters, all policy/channel/window guards including explicit bitwise operators.'),
('LegacyPlanOwner',v(R=2,C=1),{'Fixed':1,'ARROW_HANDLE':3,'GetterReference':1,'GetterTwo':1,'BOOL_HANDLE':1},'Mapped owner ref, selected two-cell window value, actual owner/channel invariant checks.'),
('LegacyPlanExtent',v(R=5,C=3,X=3),{'Fixed':1,'ARROW_HANDLE':2,'MAX2':1},'All selected/window/frame-size predicates, named channel division and visible numeric_limits getter.'),
('LegacyPlanProduct',v(R=6,C=3,X=7),{'Fixed':1,'ARROW_HANDLE':4,'EXT3':1,'GetterReference':1,'GetterScalar':1},'All selected-length, actual mapped element count and inputFrames equality paths; arithmetic and explicit selectors retained.'),
('LegacyPlanBytes',v(R=9,W=2,C=2,X=6),{'Fixed':1,'ARROW_HANDLE':4,'GetterReference':3,'GetterTwo':2,'Minimum':3},'Actual window-clock/request two-cell limit getters, accounting ref, nested minima, retained byte local and both resource predicates.'),
('LegacyPlanTariff',v(R=3,W=1,C=1,X=4),{'Fixed':1},'Actual perFrame multiplication/addition and selected-length division guard; no fee is charged merely by constructing this eligibility expression.'),
('LegacyPlanFee',v(R=2,W=1,X=3),{'Fixed':1},'Actual1024+selected.srcLen*perFrame caller calculation/local; the eventual builder debit is separate once.'),
('LegacyPlanPrepare',v(R=4,W=2,C=2,X=3,H=2,Z=2),{'Fixed':1,'Access':1,'Minimum':1},'Output reference, PreparedAttempt+limits prebody DMIs, named inherited minima/fee/byte arguments and prepare/cancel guards.'),
('LegacyPlanCommitInvoke',v(R=3,W=2,X=1,H=2,Z=2),{'Fixed':1,'CommitPrepared':1},'Named prepared fee caller and actual two-cell limits value copy, selected old call H/Z; semantic1024+N*(8+16C) transferred once separately.'),
('LegacyPlanReturned',v(R=7,W=6,C=3,X=17,H=1,Z=1),{'Fixed':1},'Actual returned counter, known/result2/evidence/cumulative stores, full-scan outcome tests and subsequent success test.'),
('LegacyPlanStore',v(),{'Fixed':1,'BOOL_HANDLE':1},'Output validity check; actual plan move is separately prepaid LegacyResultPlanMove.'),
('LegacyPlanCumulative',v(R=7,W=8,C=1,X=32),{'Fixed':1,'Access':1,'ARROW_HANDLE':1,'GetterReference':1},'Destination accounting reference, both counters, equality, all six returned-owner evidence/frames/samples stores; no accounting aggregate copy.'),
('LegacyPlanFullScan',v(R=4,W=5,X=18),{'Fixed':1},'Actual source-proved full-scan evidence stores, selected frame/sample arithmetic and knownFullScanOutcomes increment.'),
('LegacyPlanException',v(R=1,W=2,X=5),{'Fixed':1},'Actual catch counter and outcome stores; no fabricated discarded private-accounting read.'),
('QueryQuoteRefused',v(),{'Fixed':1},'Explicit failed quote branch, ordinary refusal body separately funded.')
]:row(name,a,co,why)
e=next(e for e in d['events'] if e['site']=='LegacyClockPrepare');e['d_vector']=v(X=1);e['price_coefficients'][2]=1
save();print('adapter rows',d['current_reconciled_rows'])

for name,a,co,why in [
('QueryEntry',v(R=3,W=16,C=2,X=9,H=1,Z=1),{'Fixed':1,'Access':3},'Resolve own H/Z, beginReplay caller/guard, metadata start/length and mapped-frame scalar locals,13-cell Prediction value-init and predict guard. Each state accessor actual caller selector/field chain retained.'),
('QueryPredictEntry',v(R=13,W=29,H=2,Z=2),{'Fixed':1,'BOOL_HANDLE':1},'Three scalar parameters; predicted13-cell zero temporary plus13R/W assignment and selected assignment edge; clock bool logical boundary.'),
('QueryScalarNull',v(W=1,X=1),{'Fixed':1},'Invalid-clock predicted.status store; path packet owns its actual path selector.'),
('QueryScalarNegative',v(W=1,X=1),{'Fixed':1},'Invalid-start predicted.status store; constant return true R0.'),
('QueryScalarLength',v(W=1,X=1),{'Fixed':1},'Invalid-length predicted.status store.'),
('QueryScalarN',v(W=1,X=1),{'Fixed':1},'Zero-frame predicted.status store.'),
('QueryScalarOverflow',v(W=1,X=1),{'Fixed':1},'Overflow predicted.status store.'),
('QueryPredictionScalarChecks',v(R=1,C=1),{'Fixed':1},'Named start negative predicate.'),
('QueryPredictionLengthCheck',v(R=1,C=1),{'Fixed':1},'Named length nonpositive predicate.'),
('QueryPredictionNCheck',v(R=1,C=1),{'Fixed':1},'Named frame count zero predicate.'),
('QueryPredictionOverflowCheck',v(R=2,C=1,X=1),{'Fixed':1,'MAX2':1},'Named length/start overflow subtraction and visible max getter.'),
('QueryPredictionEndpoints',v(R=7,W=31,C=2,X=7),{'Fixed':1,'ARROW_HANDLE':2,'EXT3':1,'Math1':1,'Access':1,'GetterReference':1,'GetterTwo':1},'Shared at/prefix first writes; threshold/eligibility/rate;two13-cell endpoint value-inits; endpoint A scalar caller reads and return/status guards. Getter result cells accounted once.'),
('QueryPredictionAFailed',v(R=1,W=1,X=2),{'Fixed':1},'Actual A status source read/predicted destination write.'),
('QueryPredictionB',v(R=7,C=3,X=4),{'Fixed':1},'B scalar arguments/start+length, returned/status and collapsed-frame guards; each named scalar read once per written occurrence.'),
('QueryPredictionBFailed',v(R=1,W=1,X=2),{'Fixed':1},'Actual B status source read/predicted destination write.'),
('QueryPredictionCollapsed',v(W=1,X=1),{'Fixed':1},'Collapsed-window status destination.'),
('QueryPredictionOk',v(R=2,W=3,X=5),{'Fixed':1},'Success status plus both actual endpoint floor/predicted frame assignments.'),
('QueryEndpointEntry',v(R=15,W=29,C=2,X=1,H=2,Z=2),{'Fixed':1},'Tick/eligible/rate scalar parameters; thirteen-cell out zero temporary and assignment; eligible/map guards, own/assignment H/Z.'),
('QueryEndpointResource',v(W=1,X=1),{'Fixed':1},'Resource status store; FA_RESOURCE_LIMIT packet separate.'),
('QueryEndpointEmpty',v(R=3,W=1,C=1,X=8),{'Fixed':1,'Math1':1},'Two native.check caller selectors, exact empty numeric formula/order and out.frame store, actual isfinite caller read/field selection. Constant-only denominator folding has no runtime cast/arithmetic fee.'),
('QueryEndpointMap',v(R=7,W=1,C=2,X=6),{'Fixed':1},'Search/prefix actual scalar callers; out.frame copy; map row tick predicate. Search return cells own SearchEnd, vector args own VectorEntry.'),
('QueryEndpointPartial',v(R=7,W=1,C=4,X=7),{'Fixed':1,'Math1':1},'Row reference, next scalar/branch, append return guard, returned&&isfinite, final native check. Six argument cells and append prefix sponsored actual next/last argument row.'),
('QueryPartialNextArgs',v(R=16,W=12,X=13,H=1,Z=1),{'Fixed':1,'AppendPrefix':1},'Next row ref; six-cell RHS aggregate plus6R/W assignment and all named operands, segment/delta arithmetic, out.frame actual ref selector; complete append pre-entry sponsorship.'),
('QueryPartialLastArgs',v(R=14,W=12,X=8,H=1,Z=1),{'Fixed':1,'AppendPrefix':1},'Last-row six-cell RHS plus6R/W assignment, repeated delta/BPM reads and arithmetic, out.frame ref selector; complete append pre-entry sponsorship.'),
('QueryEndpointAtRow',v(R=1,X=2),{'Fixed':1,'Math1':1},'Actual isfinite out.frame caller and final native.check selector.'),
('QueryEndpointAfterFt',v(R=1,W=12,C=1,H=1,Z=1),{'Fixed':1},'finishEndpoint own H/Z, two bool scalar parameters, compiled10-cell value-init and frameForTickReturned guard. Packet ternary/read and vector caller selectors are paid in their concrete packets.'),
('QueryEndpointFtFailed',v(R=10,W=21,X=2,H=1,Z=1),{'Fixed':1},'Failure status and actual10-cell out.path RHS zero plus10R/W assignment; selected assignment H/Z. Packet/vector computation separate.'),
('QueryEndpointNative',v(R=11,W=20,C=1,X=2,H=1,Z=1),{'Fixed':1,'Math1':1},'Actual finite predicate plus common successful-FT tail path reset:10-cell zero RHS and10R/W assignment before final path packets.'),
('QueryEndpointNonfinite',v(W=1,X=1),{'Fixed':1},'NumericDomain status store.'),
('QueryEndpointNegativeTest',v(R=1,C=1,X=1),{'Fixed':1},'Actual negative frame test.'),
('QueryEndpointNegative',v(W=1,X=1),{'Fixed':1},'Negative NumericDomain status store.'),
('QueryEndpointRangeTest',v(R=1,C=1,X=1),{'Fixed':1},'Actual binary64 overflow frame test.'),
('QueryEndpointOverflow',v(W=1,X=1),{'Fixed':1},'Overflow status store.'),
('QueryEndpointOk',v(R=1,W=2,X=4),{'Fixed':1,'Math1':1},'Actual success status store, std::floor argument/return, named int64 cast and floor destination.'),
('QueryResolvePrepare',v(W=1,C=1,X=2),{'Fixed':1,'RestorePrefix':1},'Prepared path selector/amount local and actual explicit restoration caller selector/guard; restore precheck sponsored here even when it refuses before its own Restore row.'),
('QueryResolveCommitInvoke',v(R=4,W=1),{'Fixed':1,'CommitOld':1},'Named amount caller then branchless protected transfer; old resolve start/length/frames caller reads and status local. Actual old packet body/parameters/HZ separately protected once.'),
('QueryResolveReturned',v(R=9,W=5,C=2,X=13),{'Fixed':1},'Actual query counters, evidence store, nativeOk/equal locals, reconcile scalar callers and two-result bitwise mismatch predicate. Record vector caller selectors belong RecordEntry.'),
('QueryReconcileEntry',v(R=2,W=4,C=1,X=1,H=1,Z=1),{'Fixed':1},'Four scalar parameter destinations, own H/Z and actual returned versus predicted status predicate.'),
('QueryReconcileFailure',v(),{'Fixed':1},'Mismatched status constant false return.'),
('QueryReconcileStatus',v(R=1,C=1),{'Fixed':1},'Actual Ok status branch.'),
('QueryReconcileSuccess',v(R=5,C=6,X=23),{'Fixed':1,'WindowValid':1,'GetterScalar':7,'GetterReference':3,'OwnerBefore':2},'All nine successful identity/scalar checks, three clock refs/two get reads/two owner-before boundaries/five scalar getters, five named comparison operands/two predicted selectors/eight bitwise ands; no Window copy.'),
('QueryReconcileEmpty',v(C=6,X=14),{'Fixed':1,'WindowValid':1,'GetterReference':1,'GetterScalar':6},'All seven failure-empty checks, one clock ref/get and five scalar getters; constants R0 and six explicit bitwise ands.'),
('QueryReconcileMismatch',v(R=1,W=4,X=4),{'Fixed':1},'Unavailable evidence/outcome stores and actual two-cell returned QueryCallResult construction; status named R1.'),
('QueryReconcileOk',v(R=1,W=2),{'Fixed':1},'Actual successful QueryCallResult two-cell construction.'),
('QueryNoInvocation',v(W=2),{'Fixed':1},'Actual two constant fields of uninvoked QueryCallResult, no scalar source reads.'),
('QueryWindowPreScope',v(R=1,C=1,X=1),{'Fixed':1},'Scope constructor caller separately funded constructor row; accepted_ actual field guard.'),
('QueryWindowAfterScope',v(R=3,C=5,X=5),{'Fixed':1},'All three final native/restore/reconciliation outcome predicates/bitwise operators and request/plan return guards; chain return edges already LegacyChainCaller.')
]:row(name,a,co,why)
e=next(e for e in d['events'] if e['site']=='QueryPredictionEndpoints');e['d_vector']=v(X=1);e['price_coefficients'][2]=1
save();print('all rows',d['current_reconciled_rows'])
