import collections, hashlib, json, pathlib, re
BASE=pathlib.Path(__file__).resolve().parent.parent
ROOT=BASE.parent.parent
P=BASE/'census.json'
d=json.loads(P.read_bytes()); E={e['site']:e for e in d['events']}
V=d['vector_order']
def vec(**kw):return [kw.get(k,0) for k in V]
def plus(*vv):return [sum(x) for x in zip(*vv)]
def sched(*names,**counts):
    c=collections.Counter(names);c.update(counts);return c
def join(*cc):
    out=collections.Counter()
    for c in cc:out.update(c)
    return out
def scale(c,n):return collections.Counter({k:v*n for k,v in c.items()})
def price(c):return [sum(E[k]['price_coefficients'][i]*n for k,n in c.items()) for i in range(3)]
def evalp(a,B=0,conv=1):return a[0]+a[1]*B+a[2]*conv
def maxsched(*cc):return max(cc,key=lambda x:evalp(price(x)))
S={}
def put(name,c,why):
    S[name]={'site_counts':dict(sorted(c.items(),key=lambda t:E[t[0]]['ordinal'])),'price_constant_B_d':price(c),'derivation':why,'reservation_only_not_actual_visits':True}
    return c
def named(n):return collections.Counter(S[n]['site_counts'])

# Reconcile all caller-case scalar prices from the actual common helper/body.
for e in d['events']:
    anchors=[]
    for f in ['formation-r8.h','worker-result.h','legacy-adapter.h','window-query.h','native-profile.h','native-observer.cpp']:
        for i,line in enumerate((ROOT/'build-ci/20261105-musical-funded-legacy-adapter-source-r4'/f).read_text().splitlines(),1):
            if re.search(r'\bSite::'+re.escape(e['site'])+r'\b',line):anchors.append({'file':f,'line':i})
    e['source_anchors']=anchors
    if e['site']=='QueryPacket':
        own=vec(R=1,W=2,H=1,Z=1)
        base=plus(own,d['helpers']['Fixed']['tuple'],d['helpers']['OldRow']['tuple'])
        for case in e['caller_cases']:case['price']=sum(plus(base,case['incoming']))
        assert e['price_coefficients'][0]==max(c['price'] for c in e['caller_cases'])
    if 'meaning' in e:e['meaning']=e['meaning'].replace('OldRow helper9','OldRow helper8')
    if 'source_derivation' in e:e['source_derivation']=e['source_derivation'].replace('OldRow helper9','OldRow helper8')
d['helpers']['AppendPrefix']['source']=d['helpers']['AppendPrefix']['source'].replace('helper9','helper8')

put('VectorAdd',sched('QueryVectorEntry','QueryVectorEnd',QueryVectorTest=11,QueryVectorCell=10,QueryVectorContinue=10),'One own entry/local, exactly eleven tests/ten cell bodies/ten increments, one own normal exit. A packet calls this once irrespective of multiplicity.')
put('VectorSum',sched('QuerySumEntry','QuerySumEnd',QuerySumTest=11,QuerySumCell=10,QuerySumContinue=10),'Exactly ten accumulated vector cells and one extra final test; no sum of a maximum charged as actual packet.')
put('VectorRecord',sched('QueryRecordEntry','QueryRecordEnd',QueryRecordTest=11,QueryRecordCell=10,QueryRecordContinue=10),'Exactly ten destination additions and one extra final test. This is diagnostics bookkeeping in legacyQuery, not old callee work.')
put('Packet',join(sched('QueryPacket'),named('VectorAdd')),'One OldRow reference and one ten-cell add. Actual id/times/into caller cases are in QueryPacket/VectorEntry; a fixed Site upper is not an observed vector.')
put('PrepareOld',join(sched('QueryOldPrepare','QueryOldPrepared'),named('VectorSum')),'Actual vector sum, all admission predicates and one protected split. The exact future callee amount is separate; no semantic/max fee replaces it.')
put('NativeCheck',sched('QueryNativeCheckEntry','QueryNativeRead','QueryNativeCompare','QueryNativeRejected'),'Check read/matches and the larger of Accepted97/Rejected116; rejected chosen for bound only. No caller implicit-this or unqualified selector fee.')
put('ClassifyAppend',sched('QueryAppendClassify','QueryAppendJump','QueryAppendEqualTest','QueryAppendEqual','QueryAppendFullTest','QueryAppendFullRamp','QueryAppendSlope','QueryAppendSlopeZero','QueryAppendSlopeFinite','QueryAppendSlopeBad','QueryAppendRelative','QueryAppendRelativeBad','QueryAppendRelativeRange','QueryAppendRangeBad','QueryAppendRamp'),'Conservative finite union: every syntactic classifier partition can be reached at most once per invocation. Mutually exclusive returns are held, never all consumed. Full prefixes stop before slope work.')
put('AppendNew',join(named('ClassifyAppend'),named('PrepareOld'),named('VectorRecord'),scale(named('NativeCheck'),3),sched('QueryOldAppendInvoke','QueryOldAppendReturned','QueryOldPrefixReturned')),'One pre-classification check, one post-classification check and one post-old-call check; one selected old packet; one record and one Prefix/Partial return (both price97). AppendPrefix prebody is already in actual PrefixRow or PartialNext/LastArgs and is excluded here.')
put('SearchFixed',sched('QuerySearchEntry','QuerySearchTest','QuerySearchEnd'),'Search entry, one final count==0 test, checked final index and possible named three-cell return copy.')
put('SearchIteration',sched('QuerySearchTest','QuerySearchRow','QuerySearchFalse'),'One positive count test/body plus max(True103,False109). Each of two binary searches has at most bit_width(R) iterations; no unused comparator vector charged as actual.')
put('PrefixCallFixed',sched('QueryPrefixEntry','QueryPrefixTest','QueryPrefixEnd'),'Each of at most two prefixTo calls has its own entry/final equality test/end, even when it completes no full row.')
put('PrefixIteration',join(sched('QueryPrefixTest','QueryPrefixRow','QueryPrefixFailed','QueryNativeRejected'),named('AppendNew')),'One positive test, six argument cells and complete sponsored append. Continuation95 is dominated by actual old-return-failure branch92+reject116; reservation chooses that bound. A failure stops traversal. Shared monotone cursor completes at most R-1 rows across both calls.')
endpoint_rows=['QueryEndpointEntry','QueryEndpointResource','QueryEndpointEmpty','QueryEndpointMap','QueryEndpointPartial','QueryEndpointAtRow','QueryEndpointAfterFt','QueryEndpointFtFailed','QueryEndpointNative','QueryEndpointNonfinite','QueryEndpointNegativeTest','QueryEndpointNegative','QueryEndpointRangeTest','QueryEndpointOverflow','QueryEndpointOk','QueryPartialNextArgs']
put('EndpointFixed',join(sched(*endpoint_rows),scale(named('Packet'),12),scale(named('VectorAdd'),2),named('SearchFixed'),named('PrefixCallFixed'),named('AppendNew')),'Conservative union of all endpoint partitions once. PartialNextArgs153 dominates Last146. At most twelve packet invocations and two direct vector adds, one search and prefix call, one partial append. Empty/resource alternatives skip later pieces; held union is not a trace. Search positive iterations and shared full prefixes remain separate variable coefficients.')
predict_rows=['QueryPredictEntry','QueryScalarNull','QueryScalarNegative','QueryScalarLength','QueryScalarN','QueryScalarOverflow','QueryPredictionScalarChecks','QueryPredictionLengthCheck','QueryPredictionNCheck','QueryPredictionOverflowCheck','QueryPredictionEndpoints','QueryPredictionAFailed','QueryPredictionB','QueryPredictionBFailed','QueryPredictionCollapsed','QueryPredictionOk']
put('PredictFixed',join(sched(*predict_rows),scale(named('Packet'),3),scale(named('VectorAdd'),2),scale(named('EndpointFixed'),2)),'All finite predict scalar/return partitions at most once; at most three own packets, two own direct adds and two endpoints. A failed A prevents B. Both endpoint bounds held to dominate all prefixes without claiming both old calls occurred.')
resolve_rows=['QueryEntry','QueryResolvePrepare','QueryResolveCommitInvoke','QueryResolveReturned','QueryReconcileEntry','QueryReconcileFailure','QueryReconcileStatus','QueryReconcileSuccess','QueryReconcileMismatch','QueryReconcileOk','QueryNoInvocation']
put('ResolveFixed',join(sched(*resolve_rows),named('PrepareOld'),named('VectorRecord'),named('PredictFixed')),'Resolve once, at most one of three NoInvocation branches, one prepared exact resolve packet, one diagnostic vector record. Successful reconcile168 dominates empty137. Explicit restore prefix is already ResolvePrepare; restoration/check bodies separate below.')
put('ReplayFixed',sched('QueryNativeBeginReplay','QueryNativeRead','QueryNativeCompare','QueryNativeReplayReady','QueryNativeRejected'),'At most one beginReplay with one full snapshot and matches, union of ready/refusal; does not call NativeCheck.')
put('RestorePhysicalFixed',sched('QueryNativeRestore','QueryNativeRestoreResult','QueryNativeRestoreVerify','QueryNativeRead','QueryNativeCompare','QueryNativeRestoreReady','QueryNativeRejected'),'At most one actual restoreStatus call: restoring_=false is stored before testing result. One post-restore read/matches and union of success/refusal. Failed precheck can leave restoring true; destructor may retry only precheck, not repeat an already entered physical restore.')
put('QueryNewFixed',join(named('ResolveFixed'),named('ReplayFixed'),named('RestorePhysicalFixed'),scale(named('NativeCheck'),9),sched('QueryNativeScopeDestroy','QueryWindowPreScope','QueryWindowAfterScope')),'Nine constant native.check upper: Clock before/after2, endpoint extra checks<=4, explicit/destructor restoration prechecks<=2, post-old-resolve1. The six checks for two partial appends already belong two EndpointFixed.AppendNew. Per-prefix appends own their three checks. Destructor restore prefix already ScopeDestroy; explicit prefix already ResolvePrepare. Actual pre-scope accepted guard and after-scope outcome/request/plan guards included once. Early refusal leaves these held, not consumed.')

# Native matcher body has one pre-Clock pass. Fixed identity facts are17*32.
identity=sched('QueryNativeScope','QueryNativeIdentity','QueryNativeFactEnd',QueryNativeFactTest=18,QueryNativeFactRow=17,QueryNativeDigestEntry=17,QueryNativeDigestTest=17*33,QueryNativeDigestByte=17*32,QueryNativeDigestEnd=17,QueryNativeDigestZeroTest=17,QueryNativeFactContinue=17)
matcher_fixed=sched('NativeMatchEntry','NativeMatchProfileGuard','NativeMatchControlCall','NativeMatchControlGuard','NativeMatchLogModuleCall','NativeMatchGetterModuleCall','NativeMatchModuleGuard','NativeMatchModuleInfoCall','NativeMatchImageGuard','NativeMatchAnchorGuard','NativeMatchAnchorOffsets','NativeMatchRegionEntry','NativeMatchRegionTest','NativeMatchRegionEnd','NativeMatchRegionTotal','NativeAccessRegionEntry','NativeAccessRegionTest','NativeAccessRegionEnd','NativeAccessRegionTotal','NativeMatchAccepted','NativeMatchFailed','QueryNativeRejected')
baseline=sched('QueryNativeBaseline','QueryNativeRead','NativeMatchSelection','NativeMatchSelectionPolicy','NativeMatchSelectionStore','QueryNativeCompare','QueryNativeConstructComplete')
put('NativeMatchFixed',join(identity,matcher_fixed,baseline),'Full17-fact/32-byte digest identity traversal, exactly one pre-Clock match fixed prefix/final tests, one source-proven rejection tail plus baseline/read/matches and accepted selector0/1 guard. Early failure is a prefix of this conservative union, not proof of successful qualification.')
put('NativeComparedRegion',sched('NativeMatchRegionTest','NativeMatchRegionRow','NativeMatchRegionRole','NativeMatchRegionExtent','NativeMatchRegionAddress','NativeMatchPageEntry','NativeMatchPageTest','NativeMatchPageEnd','NativeMatchRegionContinue'),'One positive region row plus its page-loop entry/final test/end and continuation; final region-test/end/total paid in fixed. <=M rows, including the last refused row.')
put('NativeAccessRegion',sched('NativeAccessRegionTest','NativeAccessRegionRow','NativeAccessRegionRole','NativeAccessRegionExtent','NativeAccessRegionAddress','NativeAccessPageEntry','NativeAccessPageTest','NativeAccessPageEnd','NativeAccessRegionContinue'),'One address-only region row plus page-loop fixed tail. <=D rows, including last refusal. Successful4-byte aligned profile implies A=4D; no mutable-value comparison.')
put('NativeComparedPage',sched('NativeMatchPageTest','NativeMatchPageCall','NativeMatchPageGuard','NativeMatchPageExtent','NativeMatchPageChunk','NativeMatchByteEntry','NativeMatchByteTest','NativeMatchByteEnd','NativeMatchPageContinue'),'Positive page body plus byte-loop entry/final test/end. Positive completed chunk advances >=1 byte, so <=L bodies including a last refused body; all byte comparisons charged separately.')
put('NativeAccessPage',sched('NativeAccessPageTest','NativeAccessPageCall','NativeAccessPageGuard','NativeAccessPageExtent','NativeAccessPageChunk','NativeAccessPageContinue'),'Positive address-only page body; positive chunk => <=A page bodies, includes last refusal and no mutable-value read.')
put('NativeComparedByte',sched('NativeMatchByteTest','NativeMatchByteRead','NativeMatchByteContinue'),'One positive byte test/read, max(continue104,mismatch101). <=L total compared byte bodies. Mismatch106+reject116 once in fixed, no native byte dereference on access-only regions.')
d['schedules']=S
region=price(maxsched(named('NativeComparedRegion'),named('NativeAccessRegion')))
page=price(maxsched(named('NativeComparedPage'),named('NativeAccessPage')))
d['query_quote_12_terms']={
 'symbols':{'R':'captured tempo rows','P':'R==0?0:R-1','b':'bit_width(R)','E':'R==0?0:R+1','M':'private compared-region count UNBOUND','L':'private compared-byte count UNBOUND','D':'private address-only region count UNBOUND','A':'private address-only byte count UNBOUND'},
 'terms':[
 {'count':'1','unit_field':'pendingWindowQueryFixed','derived_price':price(named('QueryNewFixed'))},
 {'count':'P','unit_field':'pendingWindowQueryPerPrefix','derived_price':price(named('PrefixIteration'))},
 {'count':'2*b','unit_field':'pendingWindowQueryPerSearch','derived_price':price(named('SearchIteration'))},
 {'count':'E','unit_field':'OldQueryCatalogue.appendCeiling','derived_price':[94,0,0]},
 {'count':'1','unit_field':'OldQueryCatalogue.queryCeilingFixed','derived_price':[970,0,0]},
 {'count':'b','unit_field':'OldQueryCatalogue.queryCeilingPerBit','derived_price':[72,0,0]},
 {'count':'1','unit_field':'pendingNativeMatchFixed','derived_price':price(named('NativeMatchFixed'))},
 {'count':'M','unit_field':'pendingNativeMatchPerRegion','derived_price':region},
 {'count':'D','unit_field':'pendingNativeMatchPerRegion','derived_price':region},
 {'count':'L','unit_field':'pendingNativeMatchPerPage','derived_price':page},
 {'count':'A','unit_field':'pendingNativeMatchPerPage','derived_price':page},
 {'count':'L','unit_field':'pendingNativeMatchPerByte','derived_price':price(named('NativeComparedByte'))}],
 'execution':'All twelve checked terms execute even after one refuses; each failed term leaves total unchanged. Unit/count/header source expressions are in QueryQuote, each quoteTerm body and refusal/ready is separately held by legacyFixed.',
 'old_callee':'All190 exact accepted action vectors copied separately below. At actual prepareOld the selected vector is summed and protected, then commitOld transfers exactly that amount before one guaranteed old invocation; 94 and970+72b are only unused reserve ceilings.',
 'conservation':'legacyQuery -> queryCallee via same Phase::Context split; branchless commit consumes exact protected amount. Later new actions, failed native checks/restoration and diagnostic loops may spend legacyQuery only, not queryCallee.'}

# Every ordinary Legacy.fixed/new caller is part of a prephysical fixed root slice.
# Union paths are intentionally finite reservation bounds, not invented executions.
root_paid={'LegacyRootInstall','LegacyDistribute','LegacyCaptureOptions'}
cleanup={'LegacyFinalProbe','LegacySlicesRetire','LegacyArtifactsDestroy','LegacyClockOutputDestroy','LegacyRequestOutputDestroy','LegacyPlanOutputDestroy','LegacyClockStageDestroy','LegacyWindowStageDestroy','LegacyRequestStageDestroy','LegacyCellDestroy','LegacyCellDestroyOwned','LegacyCellDestroyBorrowed','LegacyControlResultDestroy','LegacyAbortResult'}
fixed_names=[e['site'] for e in d['events'][129:234] if e['site'] not in root_paid|cleanup|{'LegacyLocalOwnerReturn'}]
fixed=sched(*fixed_names)
repeat={'LegacyBudgetEntry':3,'LegacyBudgetTest':21,'LegacyBudgetRow':18,'LegacyBudgetContinue':18,'LegacyBudgetEnd':3,'LegacyAttemptPrepare':3,'LegacyAttemptLimit':3,'LegacyAttemptFeeLimit':3,'LegacyAttemptKind':3,'LegacyAttemptReserve':3,'LegacyAttemptReserveRefused':1,'LegacyAttemptReady':3,'LegacyCancel':5,'LegacyCancellationTaken':1,'LegacyRefusal':1,'LegacyStop':1,'LegacyReady':9}
for k,n in repeat.items():fixed[k]=n
fixed.update(sched('QueryQuote','QueryQuoteRefused','QueryCallNotEntered','QueryCallEntered','QueryCallOuterFailure',QueryQuoteTerm=12,QueryQuoteOverflow=12,QueryQuoteReady=12))
put('LegacyFixed',fixed,'Finite union of all newly authored adapter/control/result caller paths. Three prepare calls each have six total() bodies/seven tests; five cancellation probes (chain,Clock,Window,Request,Plan), at most one cancellation-taken/refusal/stop; ready<=9 (common+queryreserve+three prepare+four stages); one quote with all12terms and both return branches conservatively held. All stage-return/catch alternatives held once. Control result and later retained-check paths are both held because sequential lifetimes are legal. No original Worker874 slack is used.')
check=sched(ReserveScan=4,ReserveFail=4,ReserveInstall=4)
put('LegacyCheck',check,'At most four genuine reserve invocations: query +Clock/Request/Plan semantic attempts. Every call pays six-phase ReserveScan and exactly Fail OR Install; union holds both87+131, not consumed as both. Capture reserve has a separate captureCheck slice.')
clean=sched(*sorted(cleanup))
clean['LegacyCellDestroy']=2
clean['LegacyCellDestroyOwned']=1
clean['LegacyCellDestroyBorrowed']=1
put('LegacyCleanup',clean,'Three local output wrappers plus three staged owners each destruct once, WorkerArtifacts once. At most two result physical cells (embedded+detached) each physically destruct once; one owned drop and one borrowed cleanup. ControlResultDestroy and AbortResult are alternatives each held once. Exactly one FinalProbe/SlicesRetire after final actual origin owner. All are prepaid before physical lifetime; no retirement refunds.')

# Original formation source: exact changed sizes/vectors re-substituted, not old constants.
drop_non=sched('DropProbe','DropNonLast')
drop_last=sched('DropProbe','DropLast','DropMapTest','DropMapRetire','DropParentTest','DropParentRetire','DropRoleRetire')
put('DropNonLast',drop_non,'Actual nonlast role reference path.')
put('DropLastRole',drop_last,'Actual final-role shell; map/parent/state bodies and new Legacy cleanup hold their own separate slices.')
put('WorkerOrReaderRole',join(sched('ReceiveWorker','ContextCancel','ContextInspect','ContextDecision','ContextReturn'),drop_non),'Original check-only receiver remains an admitted alternative and dominates the new adapter receiver shell. Its exact original actions only are funded here. New workerBuildLegacy entry/return/callers/cancel probes/adapter all use LegacyFixed; no borrowed874 remainder.')
put('RetainedRole',join(sched('RetainedDestroy','RetainedDropCaller','ArmReader','AcquireReader'),drop_last),'Retained destructor+drop, one arm and larger actual acquire118 versus abort116. Result can be destroyed first and this whole schedule used subsequently; not treated mutually exclusive.')
control=sched('OwnerAdopt','OwnerConstructed','SlotErase','ControlDestroy','ControlDetachTest','ControlDetach','ControlDropCaller','SessionDestroyCheck','SessionDestroyRetiring','SessionDestroyFinish','ArmWorker','RetainExternal','AcquireWorker','DrainRetiring')
# Old complete Control source union retains Abort as well as once terminal-acquire.
control.update(sched('AbortWorker'))
control.update(drop_last)
put('ControlRole',control,'Complete own Control path union: adoption/body once,slot erase,destruction/detach/drop,all session administrative branches,one arm,abort alternative and one acquire with max caller Drain120 over Current113. Result-related new callers are LegacyFixed/Cleanup. All three Control/Worker/Result then later Control/Retained/Reader schedules remain separately held.')
put('ResultRole',drop_last,'One actual retained result-origin reference drop; cell physical destruction/callers are LegacyCleanup. Same final-role max758 suffices and coexists in reservation with all four other roles.')
capture_check=sched('CaptureCheck','CaptureSizes','CaptureProducts','CaptureNeed','ReserveScan','ReserveFail','CaptureReserveRefused')
put('CaptureCheck',capture_check,'Capture admission fixed prefix, maximum ReserveFail87+CaptureReserveRefused88 versus ReserveInstall131. Earlier count/sum/byte refusals are dominated; current d qualified.')
put('StateCleanup',sched('StateDestroy'),'Eight actual retire calls and five-role loop (new role size/action expansion in Site128); no old515 reused.')
put('MapCleanup',sched('MapDestroy'),'Actual old map release source unchanged; no TempoChange destructor loop.')
put('ParentCleanup',sched('ParentDestroy'),'Actual two handle drops after byte/access qualifiers; no deferred result cleanup is borrowed here.')
put('Snapshot',sched('SnapshotFixed',SnapshotRow=6),'Current Raw535 copied; six actual canonical phase rows. Old1638 has not been transferred.')

# Current formation branch equations, with only original closed body mathematics reused.
pr=lambda n: evalp(E[n]['price_coefficients'],conv=0)
snapshot=evalp(price(named('Snapshot')),conv=0)
post=max(snapshot,pr('NewFailureTest')+pr('ReadoutCommit')+snapshot,
    pr('NewFailureTest')+pr('SessionSelect')+max(pr('SessionKeepRunning'),pr('SessionReusePending'))+pr('SessionInstall')+pr('ReadoutCommit')+2*snapshot)
failed_initial=sum(pr(n) for n in ['RootFailedAdopt','RootFailedSnapshot','RootFailedConstructed','RootFailedSlotErase','RootFailedDestroy','RootFailedSessionCheck','RootFailedSessionFinish'])
failed_next=sum(pr(n) for n in ['RootFailedReadout','RootFailedDiscard','RootFailedSnapshot'])
physical_prefix=sum(pr(n) for n in ['RootBegin','RootSessionInit','RootCounterIssue','RootIdentity','RootAllocate','RootObject','RootInstall','LegacyRootInstall','Distribute','LegacyDistribute','CaptureDispatch','CaptureResultTest','CaptureRefused'])+post
root_constant=physical_prefix+sum(evalp(price(named(n)),conv=0) for n in ['StateCleanup','CaptureCheck','ControlRole','WorkerOrReaderRole','RetainedRole','WorkerOrReaderRole','ResultRole','LegacyFixed','LegacyCheck','LegacyCleanup'])
root_d=sum(price(named(n))[2] for n in ['StateCleanup','CaptureCheck','ControlRole','WorkerOrReaderRole','RetainedRole','WorkerOrReaderRole','ResultRole','LegacyFixed','LegacyCheck','LegacyCleanup'])
no_state=sum(pr(n) for n in ['RootBegin','RootSessionInit','RootCounterIssue','RootIdentity'])+max(pr('RootIdentityRefused'),pr('RootAllocate')+pr('RootAllocateFailed'))+pr('RootNoState')+failed_initial
per_search=max(pr('ClipLoopTest')+pr('ClipRow')+pr('ClipDecision')+max(pr('ClipContinue'),pr('ClipFound')),pr('AssetLoopTest')+pr('AssetRow')+pr('AssetDecision')+max(pr('AssetContinue'),pr('AssetFound')))
per_map=pr('MapLoopTest')+pr('MapRow')+max(pr('MapFirst'),pr('MapLater'))+pr('MapDecision')+max(pr('MapInvalid'),pr('MapStore')+pr('MapContinue'))
maploop=sum(pr(n) for n in ['MapLoopEntry','MapLoopTest','MapEnd','CaptureFinish'])
allocate=pr('MapAllocate')+max(pr('MapAllocateFailed'),pr('MapInstall')+maploop)
maptail=pr('MapNeededTest')+max(maploop,allocate)
product=pr('SourceProduct')+max(pr('SourceProductRefused'),maptail)
extent=pr('SourceExtent')+max(pr('SourceExtentRefused'),product)
layout=pr('SourceLayout')+max(pr('SourceLayoutRefused'),extent)
source=pr('SourceCheck')+max(pr('SourceMissing'),layout)
metadata=pr('CaptureMetadata')+pr('LegacyCaptureOptions')+pr('CaptureParents')+source
search_fixed=sum(pr(n) for n in ['ClipLoopEntry','ClipLoopTest','ClipEnd','ClipMissingTest'])
asset=search_fixed+max(pr('AssetMissing'),metadata)
clip=search_fixed+max(pr('ClipMissing'),asset)
variable_fixed=pr('CaptureWorkBegin')+clip
d['formation_equations']={
 'same_root_physical_fixed_constant_B_d':[root_constant,9,root_d],
 'no_state_constant_B_d':[no_state,9,0],
 'postcapture_fixed':post,'failed_initial_tail':failed_initial,'failed_next_tail':failed_next,
 'fixed_physical_prefix_constant_B_d':[physical_prefix,9,0],
 'perSearchRow_constant_d':[per_search,2], 'perMapRow_constant_d':[per_map,1],
 'variableCaptureFixed_constant_d':[variable_fixed,1],
 'needed':'variableCaptureFixed + (clipCount+assetCount)*perSearchRow + R*perMapRow + mapCleanup + parentCleanup',
 'branch_expansions':{'snapshot':snapshot,'map_loop_fixed':maploop,'map_allocate':allocate,'map_test':maptail,'product':product,'extent':extent,'layout':layout,'source':source,'metadata_options_parents':metadata,'search_fixed_each':search_fixed,'asset_fixed':asset,'clip_fixed':clip},
 'root_dependency':'literal helpers -> Site prices -> body schedules/drop/check/roles -> rootEnvelope. Capture/query/semantic amount reserves occur only after fixed root funds exist and each full cap scan succeeds. No Site invokes its own price derivation and no Data runtime factory is proposed.',
 'root_role_equation':'physicalFixedPrefix + StateCleanup + CaptureCheck + ControlRole + WorkerRole + RetainedRole + ReaderRole + ResultRole + LegacyFixed + LegacyCheck + LegacyCleanup',
 'capture_invocations':'Exactly one capture attempt per genuine new Origin. Count guards are not actual visits. Found/missing/invalid prefixes and two lookup loops preserve old closed scheduling; LegacyCaptureOptions123 is inserted once before parents/capture on successful lookup.',
 'check':'Current 443-cell Bounds is immutable future literal input, not a new action allowance. Original failure-root logical reservation is held before all incoming parameter/member writes.'}

# Record complete old exact catalogue without using its maximum as actual fee.
old=json.loads((ROOT/'build-ci/20261105-musical-window-query-path-census-r1/census.json').read_bytes())
d['old_callee_exact_catalogue']={'path':'build-ci/20261105-musical-window-query-path-census-r1/census.json','sha256':hashlib.sha256((ROOT/'build-ci/20261105-musical-window-query-path-census-r1/census.json').read_bytes()).hexdigest().upper(),'rows':old['rows'],'append_terminal_cases':old['append_terminal_cases'],'case_templates':old['case_templates'],'no_old_callee_recount_or_fee_change':True}
d['aggregate_initialization_cases']={'brace_LegacyReadout':'125 total W =121 implicit zero initializations +4 nested status DMIs; outer and nested aggregates use braces, no implicit initialization ctor H/Z. Selected later assignment/copy still charged separately.','default_member_LegacyReadout':'WorkerLegacyResult readout_ default-init: four DMIs, outer implicit ctor plus two status default ctors =3H/3Z, paid at Arm/Take before body.','Common_and_Prepared':'Two requested/limits DMIs and respective aggregate/nested default ctor edges sponsored actual LegacyChainCaller or Clock/Request/PlanPrepare before callee entry.','reference':'Reference binds/caller inputs do not copy referents; scalar by-value destinations have exactly one transfer; actual return copies and selected assignments explicit.'}

# Row entry/return counterpart for the new Worker caller is separate from old
# workerRequestCheck wrapper. Same fixed Site price covers the greater old case.
for sn in ['ReceiveWorker','ContextReturn']:
 e=E[sn]
 e['caller_partition_note']='Existing check-only caller includes its original wrapper entry/return. For workerBuildLegacy, the new wrapper entry/return is already LegacyWorkerCaller; subtract the old wrapper H at ReceiveWorker and its Z at ContextReturn from that concrete case. Fixed Site price remains the greater check-only case. Do not fabricate actual ContextCancel/Inspect/Decision on new adapter branch.'
 e['new_adapter_caller_vector']=e['constant_vector'].copy()
 e['new_adapter_caller_vector'][5 if sn=='ReceiveWorker' else 6]-=1
e=E['ReceiveWorker'];e['new_adapter_caller_vector'][3]-=1
e['caller_partition_note']+=' The absent original context.checkForReceiverOnce caller selector is also removed; actual owner.receiveWorker selector remains.'
d['lifetime_schedule']={
 'max_simultaneous_origin_references':3,
 'sequence':['Control only after formation','Control+Worker after arm; embedded result borrowed','Control+Worker+Result after publish','Control+Result after Worker terminal drop','Take transfers SAME Result reference to one detached cell; embedded cell borrowed/moved-from','Detached Result physically destroyed and Role::Result drops; Control may remain','Retained may now issue once since resultOriginLive=false','Control+Retained, then Control destruction ->Retained','Retained+Reader after reader arm; one receiver/abort and one acquire; final retained/readers/control drop owns cleanup'],
 'nonexclusive_reservation':'All five roleLifetime slices are held together. Result and Retained/Reader are sequentially possible, not mutually exclusive whole-Origin alternatives.',
 'physical_cells':'One embedded Result always constructed at arm; optional one detached Take. At most two destructor bodies, one owned-origin destruction, one borrowed; no returned temporary owner/destructor due mandatory prvalue elision.',
 'cancellation':'Five source load callsites maximum, first chain or any stage may take cancellation. One taken result stops rest. No user callback/retry; real context acquisition before ledger access; terminal publication once after cleanup.',
 'restoration':'At most two restore invocations/checks: explicit and destructor after failed explicit precheck. At most one restoreStatus physical call because restoring_=false precedes its result branch. No inferred flags/x87 restoration beyond exact source/native contract.',
 'last_drop':'Final role -> LegacyFinalProbe/SlicesRetire -> map ->parents -> role retirement ->StateDestroy. No state/reference reads after possible native free; unused reservations are terminal classification and never refunded.'}

def q(R,M,L,D,A,conv=1):
 c={'1':1,'P':max(R-1,0),'2*b':2*R.bit_length(),'E':R+1 if R else 0,'b':R.bit_length(),'M':M,'L':L,'D':D,'A':A}
 return sum(evalp(x['derived_price'],conv=conv)*c[x['count']] for x in d['query_quote_12_terms']['terms'])
def capture(ca,R,conv=1):return variable_fixed+conv+ca*(per_search+2*conv)+R*(per_map+conv)+pr('MapDestroy')+pr('ParentDestroy')
base=evalp([root_constant,9,root_d],B=4264,conv=0)
example={str(R):q(R,9,8284,1,4,0) for R in [0,1,2,64,1024]}
d['conditional_fit']={
 'cap':268435456,'unmodified':True,'actual_bindings_present':False,
 'complete_same_origin_sufficient_inequality':'(9*B+rootConstant+rootD*d) + needed(clipCount+assetCount,R,d) + Q(R,M,L,D,A,d) + (1+6*R) +4096 +(1024+N*(8+16*C)) <=268435456',
 'meaning':'Sufficient all-success reserve upper, not actual debit, not universal admission. Failed/skipped stage fees are unconsumed held/refused, and actual successful inherited cumulative is one+6R then+4096 then+PlanFee once. The current prepare headroom and each reserve enforce the same Origin cap; no independently funded subrequest is introduced.',
 'required_conditions':['All420 prices and complete finite schedules independently accepted and bound in immutable Catalogue data','Exact old190 row identities,append94/query970+72b accepted data bound','Native M/L/D/A data source closure,capacity/constness/ownership,no reachable writers and native proofs bound; currently definitions absent','B and d selected R4 layout/types source-qualified; R5 public size evidence alone is not R4 layout correspondence','Physical resources,own/SDK CPU,real cancellation/thread/parent ownership and query forward/store proof remain separate HOLD'],
 'conditional_examples_only':{'R4_ABI_substitution_not_measured':{'B':4264,'d':0,'root_fixed':base},'native_candidate_unbound':{'M':9,'L':8284,'D':1,'A':4},'Q_for_R':example},
 'no_universal_fit':'R/M/L/D/A/captured counts/N can force refusal; no actual count or execution is inferred from a maximum. Symbols have finite checked domains, not unbounded CPU obligations hidden behind SDK.',
 'native_upper_domain':'M<=L<=workCap and D<=A<=workCap are checked before positive walks; invalid profile can quote-refuse earlier. Twelve checked unit*count terms cannot silently overflow.'}
d['resource_HOLD']={
 'own_new_heap_allocations':'Adapter/result/native matcher own no heap allocation; own state/map NativeStorage and old semantic builders allocate. This is source topology only, not allocator/native resource qualification.',
 'physical_pool_shape':'S +R*sizeof(TempoChange) +2*sizeof(OwnerSlot)+ApplicationRequests scalar shell +optional detached Result +WorkerArtifacts+up to one output wrapper+NativeSession+bounded callstack snapshots/endpoint/prediction/readouts +immutable Bounds443*8 +OldQueryCatalogue190*10*8+3*8 +NativeBindingData+M*sizeof(Region)+L +D*sizeof(AccessRegion). Live parent/Plan/Clock/Key/Request/control blocks and each allocator/native temporary remain separate measured/source ownership terms.',
 'no_hidden_ABI_copy':'No padding bytes treated as scalar cells, no runtime zeroing of uninitialized raw inline storage; actual sizeofRaw byte loop is B and explicit scalar aggregates use cell counts. Actual native/stack layouts and any ABI-hidden copies are qualification obligations.',
 'R4_compile_gate':'Root reports actual full-header MSVC/W4/WX compile exit2 C4324 at formation-r8.h1345 ControlRequestOwner resultBytes_ alignment after Exchange/flags. R4 preserved. This packet does not claim executable source admission.',
 'R5_separate':'Root reports minimal resultBytes_ declaration reorder compiles0 and37 public sizeof/align constants. Later independently reviewed R4->R5 runtime-action/census correspondence required. Neither unchanged offsets nor allocation/resource fit inferred.'}
d['status']='COMPLETE_FINITE_SOURCE_PROPOSAL_PENDING_INDEPENDENT_NUMERIC_REVIEW_UNBOUND_UNRUN_RESOURCE_NATIVE_HOLD'
d['schema']='YESDAW_PRODUCTION_FUNDED_ADAPTER_R4_COMPLETE_CENSUS_R2_V1'
d['seal_scope']='All420 Site vectors, exact190 protected callee rows, finite schedule/equation proposal. Independent review, source binding and execution admission are not granted by this packet.'
d['self_checks']={'event_count':len(E),'ordinals_exact_0_through_419':[e['ordinal'] for e in d['events']]==list(range(420)),'all_rows_reconciled':all(e['status']!='UNRECONCILED' for e in d['events']),'old_catalogue_rows':len(old['rows']),'schedule_count':len(S),'target_execution':False}
for e in d['events']:
 assert len(e['constant_vector'])==len(e['B_vector'])==len(e['d_vector'])==10
 assert e['price_coefficients']==[sum(e['constant_vector']),sum(e['B_vector']),sum(e['d_vector'])],e['site']
 assert all(isinstance(x,int) and x>=0 for a in [e['constant_vector'],e['B_vector'],e['d_vector']] for x in a),e['site']
 if 'helper_coefficients' in e:
  calc=plus(e['body_vector'],*[[n*x for x in d['helpers'][h]['tuple']] for h,n in e['helper_coefficients'].items()])
  assert calc==e['constant_vector'],e['site']
for name,s in S.items():assert price(collections.Counter(s['site_counts']))==s['price_constant_B_d']
assert len(E)==420 and len(old['rows'])==190
d['self_checks']['all_vector_and_schedule_arithmetic']='PASS_SOURCE_ARITHMETIC_ONLY'
P.write_bytes((json.dumps(d,indent=2)+'\n').encode())
print(json.dumps({'status':d['status'],'root':d['formation_equations'],'query':d['query_quote_12_terms']['terms'],'fixed_budgets':{k:S[k]['price_constant_B_d'] for k in ['LegacyFixed','LegacyCheck','LegacyCleanup','ControlRole','WorkerOrReaderRole','RetainedRole','ResultRole']},'Q_examples':example},indent=2))
