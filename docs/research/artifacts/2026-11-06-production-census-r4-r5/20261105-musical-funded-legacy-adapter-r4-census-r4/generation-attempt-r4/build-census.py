"""Source/file/integer-only production census proposal generator.

No target parser,compiler,import,helper,program,stage,native API or Git calls.
Imports ONLY the two newly authored sibling operand-data modules. Old packets
are comparison inputs,never a fallback source for any new body/helper price.
"""
from pathlib import Path
from hashlib import sha256
from collections import Counter
import importlib.util,json,re,sys
sys.dont_write_bytecode=True

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent
SOURCE=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-source-r4'
ORDER=['R','W','C','X','LE','H','Z','O','A','F']
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
def read(rel):return json.loads((ROOT/rel).read_bytes())
def module(name):
    s=importlib.util.spec_from_file_location(name.replace('-','_'),HERE/(name+'.py'))
    m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
def plus(*vectors):return [sum(x) for x in zip(*vectors)] if vectors else [0]*10
def times(v,n):return [x*n for x in v]
def pin(p):return dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=digest(p))
def save(name,data):(HERE/name).write_bytes((json.dumps(data,indent=2,ensure_ascii=True)+'\n').encode())
def affine(a,B=0,d=0):return a[0]+a[1]*B+a[2]*d
def fmt(a):return str(a[0])+('+'+str(a[1])+'*B' if a[1] else '')+('+'+str(a[2])+'*d' if a[2] else '')
def table(headers,rows):return '\n'.join(['|'+'|'.join(headers)+'|','|'+'|'.join(['---']*len(headers))+'|']+['|'+'|'.join(str(x).replace('|','/') for x in r)+'|' for r in rows])

source_names=['formation-r8.h','worker-result.h','legacy-adapter.h','window-query.h','native-profile.h','native-observer.cpp','README.md','source-correspondence.md','source-pins.json']
source_pins=[pin(SOURCE/n) for n in source_names]
assert source_pins[-1]['sha256']=='A5353D86D20DE812A75D6345780C6B489670DD61EB40AF87C4D2CE6776B28077'
body=module('body-ledgers');helpers=module('helper-ledgers').H
assert body.CLASS_STATIC_ID_X==0
texts={n:(SOURCE/n).read_text(encoding='utf-8') for n in source_names[:6]}
names=re.findall(r'^    (\w+), // SITE',texts['formation-r8.h'],re.M)
assert len(names)==420 and set(names)==set(body.ROWS)
assert len(helpers)==51
events=[]
for ordinal,name in enumerate(names):
    own=body.ROWS[name]
    vv=own['body_vector'];co=own['helper_coefficients']
    assert len(vv)==10 and all(type(x)is int and x>=0 for x in vv)
    assert all(k in helpers and type(n)is int and n>0 for k,n in co.items())
    for h in co:
        assert helpers[h]['tuple']==plus(*(p['vector'] for p in helpers[h]['ordered_parts']),
               *(times(helpers[k]['tuple'],n) for k,n in helpers[h]['helper_coefficients'].items()))
    constant=plus(vv,*(times(helpers[k]['tuple'],n) for k,n in co.items()))
    bv=own['body_B_vector']
    dv=plus(own['body_d_vector'],*(times(helpers[k]['d_vector'],n) for k,n in co.items()))
    anchors=[]
    for file,text in texts.items():
        for line,s in enumerate(text.splitlines(),1):
            if re.search(r'\bSite::'+re.escape(name)+r'\b',s):anchors.append(dict(file=file,line=line,text=s.strip()))
    assert anchors or name=='LegacyLocalOwnerReturn',name
    component_parts=[]
    for i,label in enumerate(ORDER):
        if vv[i]:
            p=[0]*10;p[i]=vv[i]
            component_parts.append(dict(component=label,vector=p,source_basis=own['operations']))
    events.append(dict(ordinal=ordinal,site=name,status='FRESH_COMPLETE_SOURCE_PROPOSAL_REQUIRES_WHOLE_INDEPENDENT_REVIEW',
        source_anchors=anchors,body_vector=vv,body_B_vector=bv,body_d_vector=own['body_d_vector'],
        helper_coefficients=co,constant_vector=constant,B_vector=bv,d_vector=dv,
        price_coefficients=[sum(constant),sum(bv),sum(dv)],
        ordered_body_operands=component_parts,source_operand_derivation=own['operations'],
        bare_own_identifier_or_initializer_boundary=own['bare_own_identifiers'],
        sponsor_boundary=own['sponsors'] or 'Actual scalar arguments/copies/returns and written caller paths are only in this packet or its explicitly named helper;no implicit-this/ref-binding/qualification selector.'))
E={e['site']:e for e in events}

# Concrete alternate callers retain all common body/helper work. Site prices
# are the maximum concrete scalar-price case,not componentwise padding.
E['ReceiveWorker']['new_adapter_caller_vector']=E['ReceiveWorker']['constant_vector'].copy()
E['ReceiveWorker']['new_adapter_caller_vector'][5]-=1
E['ContextReturn']['new_adapter_caller_vector']=E['ContextReturn']['constant_vector'].copy()
E['ContextReturn']['new_adapter_caller_vector'][6]-=1
E['ReceiveWorker']['caller_partition_note']='Always-present owner.receiveWorker is owned here. Check-only context.checkForReceiverOnce is owned by ContextDecision;the adapter does not reach that Site. Only old outer wrapper H is removed here.'
E['ContextReturn']['caller_partition_note']='Only old outer check wrapper Z is removed for adapter caller;LegacyWorkerCaller owns new wrapper HZ. Real ContextScope destruction/publication/drop remains here.'
E['QueryVectorEntry']['concrete_caller_cases']=[
 dict(case='direct two.path references/default times',body=[0,2,0,2,1,1,1,0,0,0]),
 dict(case='packet bare references/named times',body=[1,2,0,0,1,1,1,0,0,0]),
 dict(case='direct bare references/default times',body=[0,2,0,0,1,1,1,0,0,0])]
E['QueryRecordEntry']['concrete_caller_cases']=[
 dict(case='resolve rawarray3dots+predicted.path',body=[0,2,0,4,1,1,1,0,0,0]),
 dict(case='append rawarray3dots;OldRow/selected separately QueryOldAppendReturned',body=[0,2,0,3,1,1,1,0,0,0])]
E['QueryPacket']['concrete_caller_cases']=[
 dict(case='literal ID/defaulttimes/bareinto',body=[1,2,0,0,0,1,1,0,0,0]),
 dict(case='out.path+namedselected',body=[2,2,0,1,0,1,1,0,0,0]),
 dict(case='out.path+found.t/f multiplicity',body=[2,2,0,2,0,1,1,0,0,0]),
 dict(case='out.path+(out.status==Ok?literal:literal)',body=[2,2,1,2,0,1,1,0,0,0]),
 dict(case='compiled+(empty?literal:literal)',body=[2,2,1,0,0,1,1,0,0,0]),
 dict(case='out.path+(next or returned?literal:literal)',body=[2,2,1,1,0,1,1,0,0,0])]
for e in events:
    if 'concrete_caller_cases' in e:
        delta=plus(*(times(helpers[k]['tuple'],n) for k,n in e['helper_coefficients'].items()))
        for c in e['concrete_caller_cases']:
            c['constant_vector']=plus(c['body'],delta);c['price']=sum(c['constant_vector'])
        assert max(c['price'] for c in e['concrete_caller_cases'])==e['price_coefficients'][0]

def count(*sites,**repeat):
    c=Counter(sites);c.update(repeat);return c
def join(*counts):
    c=Counter()
    for a in counts:c.update(a)
    return c
def scaled(c,n):return Counter({k:v*n for k,v in c.items()})
def price(c):return [sum(E[k]['price_coefficients'][i]*n for k,n in c.items()) for i in range(3)]
S={}
def hold(name,c,proof):
    assert all(k in E and type(v)is int and v>0 for k,v in c.items())
    S[name]=dict(site_counts=dict(sorted(c.items(),key=lambda t:E[t[0]]['ordinal'])),
        price_constant_B_d=price(c),source_bound=proof,reservation_only_not_actual_visits=True)
    return c
def named(name):return Counter(S[name]['site_counts'])
comparisons=[]
def dominating(label,*cases):
    chosen=max(cases,key=lambda c:affine(price(c),d=1))
    for c in cases:
        a,b=price(chosen),price(c)
        assert a[1]==b[1] and all(affine(a,d=d)>=affine(b,d=d) for d in (0,1)),label
    comparisons.append(dict(label=label,case_prices=[price(c) for c in cases],chosen=price(chosen),domains='B>=0,d in{0,1};equal B slopes'))
    return chosen

# 1..18 query bookkeeper/classifier/search/endpoint/replay schedules.
hold('VectorAdd',count('QueryVectorEntry','QueryVectorEnd',QueryVectorTest=11,QueryVectorCell=10,QueryVectorContinue=10),'Exactly10 cells,11 tests,10 increments,one entry/end. Actual concrete entry case owns its callers.')
hold('VectorSum',count('QuerySumEntry','QuerySumEnd',QuerySumTest=11,QuerySumCell=10,QuerySumContinue=10),'Exactly10 scalar additions,11 tests,10 increments;named scalar return once.')
hold('VectorRecord',count('QueryRecordEntry','QueryRecordEnd',QueryRecordTest=11,QueryRecordCell=10,QueryRecordContinue=10),'Exactly10 diagnostic vector cells,11 tests,10 increments;not an old callee visit count.')
hold('Packet',join(count('QueryPacket'),named('VectorAdd')),'One exact old-row reference and one ten-cell add. Times multiplies values,not visits.')
hold('PrepareOld',join(count('QueryOldPrepare','QueryOldPrepared'),named('VectorSum')),'One exact vector sum and protected same-phase split before future old invocation;no old fee yet consumed.')
reject=dominating('check terminal result',count('QueryNativeAccepted'),count('QueryNativeRejected'))
hold('NativeCheck',join(count('QueryNativeCheckEntry','QueryNativeRead','QueryNativeCompare'),reject),'One current read/matches and the larger concrete success/reject tail.')
classify='QueryAppendClassify QueryAppendJump QueryAppendEqualTest QueryAppendEqual QueryAppendFullTest QueryAppendFullRamp QueryAppendSlope QueryAppendSlopeZero QueryAppendSlopeFinite QueryAppendSlopeBad QueryAppendRelative QueryAppendRelativeBad QueryAppendRelativeRange QueryAppendRangeBad QueryAppendRamp'.split()
hold('ClassifyAppend',count(*classify),'Finite union of every reached-at-most-once classifier Site;mutually exclusive returns are held,not consumed as one trace.')
append_return=dominating('full/partial append counter',count('QueryOldPrefixReturned'),count('QueryOldPartialReturned'))
hold('AppendNew',join(named('ClassifyAppend'),named('PrepareOld'),named('VectorRecord'),scaled(named('NativeCheck'),3),count('QueryOldAppendInvoke','QueryOldAppendReturned'),append_return),'Three potential checks:before classifier,after classifier,after actual old helper. Actual AppendPrefix pre-entry sponsor is separately in full/partial argument body.')
hold('SearchFixed',count('QuerySearchEntry','QuerySearchTest','QuerySearchEnd'),'Entry and final zero-count test/end/named3cell return. Positive iterations separately bounded.')
search_arm=dominating('binary search arm',count('QuerySearchTrue'),count('QuerySearchFalse'))
hold('SearchIteration',join(count('QuerySearchTest','QuerySearchRow'),search_arm),'Each positive step at least halves count,<=bit_width(R) per search;actual true/false incidences remain source-derived.')
hold('PrefixCallFixed',count('QueryPrefixEntry','QueryPrefixTest','QueryPrefixEnd'),'At most two prefixTo entries/final tests/exits,including target==at with zero full rows.')
prefix_tail=dominating('prefix old-return continuation/failure',count('QueryPrefixContinue'),count('QueryPrefixFailed','QueryNativeRejected'))
hold('PrefixIteration',join(count('QueryPrefixTest','QueryPrefixRow'),prefix_tail,named('AppendNew')),'Two calls share monotone at cursor;total completed full rows<=max(R-1,0). Last refusal included;failure stops further rows.')
partial_args=dominating('partial argument branch',count('QueryPartialNextArgs'),count('QueryPartialLastArgs'))
endpoint_fixed='QueryEndpointEntry QueryEndpointResource QueryEndpointEmpty QueryEndpointMap QueryEndpointPartial QueryEndpointAtRow QueryEndpointAfterFt QueryEndpointFtFailed QueryEndpointNative QueryEndpointNonfinite QueryEndpointNegativeTest QueryEndpointNegative QueryEndpointRangeTest QueryEndpointOverflow QueryEndpointOk'.split()
hold('EndpointFixed',join(count(*endpoint_fixed),partial_args,scaled(named('Packet'),12),scaled(named('VectorAdd'),2),named('SearchFixed'),named('PrefixCallFixed'),named('AppendNew')),'Conservative union of endpoint partitions:<=12 packets,<=2 own direct adds,one search/prefix call,one partial append. Full shared prefixes and positive searches separate.')
predict_fixed='QueryPredictEntry QueryScalarNull QueryScalarNegative QueryScalarLength QueryScalarN QueryScalarOverflow QueryPredictionScalarChecks QueryPredictionLengthCheck QueryPredictionNCheck QueryPredictionOverflowCheck QueryPredictionEndpoints QueryPredictionAFailed QueryPredictionB QueryPredictionBFailed QueryPredictionCollapsed QueryPredictionOk'.split()
hold('PredictFixed',join(count(*predict_fixed),scaled(named('Packet'),3),scaled(named('VectorAdd'),2),scaled(named('EndpointFixed'),2)),'Finite predict partition union,<=3 own packets/2 direct adds/two endpoints. Failure A prevents B;unused second bound is held.')
reconcile=dominating('success/failure-empty reconcile',count('QueryReconcileSuccess'),count('QueryReconcileEmpty'))
resolve_fixed='QueryEntry QueryResolvePrepare QueryResolveCommitInvoke QueryResolveReturned QueryReconcileEntry QueryReconcileFailure QueryReconcileStatus QueryReconcileMismatch QueryReconcileOk QueryNoInvocation'.split()
hold('ResolveFixed',join(count(*resolve_fixed),reconcile,named('PrepareOld'),named('VectorRecord'),named('PredictFixed')),'One resolve,<=one NoInvocation exit,one exact old resolve preparation and vector record. Explicit restore prefix already ResolvePrepare;physical restore/checks separate.')
hold('ReplayFixed',count('QueryNativeBeginReplay','QueryNativeRead','QueryNativeCompare','QueryNativeReplayReady','QueryNativeRejected'),'One beginReplay snapshot/matches plus conservative success/refusal union;not an extra NativeCheck.')
hold('RestorePhysicalFixed',count('QueryNativeRestore','QueryNativeRestoreResult','QueryNativeRestoreVerify','QueryNativeRead','QueryNativeCompare','QueryNativeRestoreReady','QueryNativeRejected'),'At most one actual restoreStatus:restoring=false before its result branch. Explicit failed precheck can be repeated by destructor;those two prefixes/checks are separately held.')
hold('QueryNewFixed',join(named('ResolveFixed'),named('ReplayFixed'),named('RestorePhysicalFixed'),scaled(named('NativeCheck'),9),count('QueryNativeScopeDestroy','QueryWindowPreScope','QueryWindowAfterScope')),'Nonappend checks<=9:Clock2,endpoint extras4,restore prechecks2,post-resolve1. Two partial append bounds already own6 checks;per-full-prefix owns3. No duplicate restore prefix.')

# 19..24 private code/table/access/page/byte admission. Positive page chunks
# advance>=1byte;the physical region profile/data is still absent.
identity=count('QueryNativeScope','QueryNativeIdentity','QueryNativeFactEnd',QueryNativeFactTest=18,QueryNativeFactRow=17,QueryNativeDigestEntry=17,QueryNativeDigestTest=17*33,QueryNativeDigestByte=17*32,QueryNativeDigestEnd=17,QueryNativeDigestZeroTest=17,QueryNativeFactContinue=17)
match_fixed='NativeMatchEntry NativeMatchProfileGuard NativeMatchControlCall NativeMatchControlGuard NativeMatchLogModuleCall NativeMatchGetterModuleCall NativeMatchModuleGuard NativeMatchModuleInfoCall NativeMatchImageGuard NativeMatchAnchorGuard NativeMatchAnchorOffsets NativeMatchRegionEntry NativeMatchRegionTest NativeMatchRegionEnd NativeMatchRegionTotal NativeAccessRegionEntry NativeAccessRegionTest NativeAccessRegionEnd NativeAccessRegionTotal NativeMatchAccepted NativeMatchFailed QueryNativeRejected'.split()
baseline=count('QueryNativeBaseline','QueryNativeRead','NativeMatchSelection','NativeMatchSelectionPolicy','NativeMatchSelectionStore','QueryNativeCompare','QueryNativeConstructComplete')
hold('NativeMatchFixed',join(identity,count(*match_fixed),baseline),'One pre-Clock pass;17 facts*32bytes,18 outer and17*33 inner tests;fixed matcher final tests/tails plus qualified baseline/selector0-or1. Failure is a prefix;nonzero digests are completeness checks,not proof booleans.')
hold('NativeComparedRegion',count(*'NativeMatchRegionTest NativeMatchRegionRow NativeMatchRegionRole NativeMatchRegionExtent NativeMatchRegionAddress NativeMatchPageEntry NativeMatchPageTest NativeMatchPageEnd NativeMatchRegionContinue'.split()),'<=M compared rows,including last refused row. Each row owns page entry/final test/end;outer final region test/end is fixed.')
hold('NativeAccessRegion',count(*'NativeAccessRegionTest NativeAccessRegionRow NativeAccessRegionRole NativeAccessRegionExtent NativeAccessRegionAddress NativeAccessPageEntry NativeAccessPageTest NativeAccessPageEnd NativeAccessRegionContinue'.split()),'<=D address-only rows,including last refused row;four-byte aligned getter footprint,successful A=4D. No mutable word comparison.')
hold('NativeComparedPage',count(*'NativeMatchPageTest NativeMatchPageCall NativeMatchPageGuard NativeMatchPageExtent NativeMatchPageChunk NativeMatchByteEntry NativeMatchByteTest NativeMatchByteEnd NativeMatchPageContinue'.split()),'<=L positive page bodies,including last refused page. Completed positive chunk advances>=1byte;byte-loop final test/entry/end included.')
hold('NativeAccessPage',count(*'NativeAccessPageTest NativeAccessPageCall NativeAccessPageGuard NativeAccessPageExtent NativeAccessPageChunk NativeAccessPageContinue'.split()),'<=A positive address-only page bodies,including last refusal;no word read or byte-loop charge.')
byte_tail=dominating('byte comparison tail',count('NativeMatchByteContinue'),count('NativeMatchByteMismatch'))
hold('NativeComparedByte',join(count('NativeMatchByteTest','NativeMatchByteRead'),byte_tail),'<=L actual byte tests/read/counter paths;last mismatch/refusal covered. Only a reached concrete leaf consumes work.')

# 25..38 application fixed/check/cleanup/roles/capture resources. All schedule
# selections are rederived here;no predecessor schedule price is loaded.
cleanup=set('LegacyFinalProbe LegacySlicesRetire LegacyArtifactsDestroy LegacyClockOutputDestroy LegacyRequestOutputDestroy LegacyPlanOutputDestroy LegacyClockStageDestroy LegacyWindowStageDestroy LegacyRequestStageDestroy LegacyCellDestroy LegacyCellDestroyOwned LegacyCellDestroyBorrowed LegacyControlResultDestroy LegacyAbortResult'.split())
root_added={'LegacyRootInstall','LegacyDistribute'}
fixed_names=[n for n in names[129:234] if n not in root_added|cleanup|{'LegacyLocalOwnerReturn'}]
fixed=count(*fixed_names)
repeats=dict(LegacyBudgetEntry=3,LegacyBudgetTest=21,LegacyBudgetRow=18,LegacyBudgetContinue=18,LegacyBudgetEnd=3,
 LegacyAttemptPrepare=3,LegacyAttemptLimit=3,LegacyAttemptFeeLimit=3,LegacyAttemptKind=3,LegacyAttemptReserve=3,
 LegacyAttemptReady=3,LegacyCancel=5,LegacyReady=9)
for n,k in repeats.items():fixed[n]=k
fixed.update(count('QueryQuote','QueryQuoteRefused','QueryCallNotEntered','QueryCallEntered','QueryCallOuterFailure','LegacyChainCaller',QueryQuoteTerm=12,QueryQuoteOverflow=12,QueryQuoteReady=12))
hold('LegacyFixed',fixed,'Exact fixed source union,including actual pre-worker LegacyCaptureOptions from legacyFixed and actual ordinal402 LegacyChainCaller. Three prepares/6-row totals,5cancel callsites,<=one taken/refusal/stop,<=9ready;12quote calls all execute. Every status/catch alternative held once;no Worker-role slack borrowed.')
hold('LegacyCheck',count(ReserveScan=4,ReserveFail=4,ReserveInstall=4),'At most query+three semantic fee reserve calls. Each actual call reaches Fail OR Install;union reserves both,not actuals. Capture reserve belongs CaptureCheck.')
clean=count(*sorted(cleanup));clean['LegacyCellDestroy']=2
hold('LegacyCleanup',clean,'Six stage/output drops,one group destructor,two possible physical result cells(one owned,one borrowed),control/abort result callers and final slice retirement;all before sponsor death,no refund.')
nonlast=count('DropProbe','DropNonLast');last=count('DropProbe','DropLast','DropMapTest','DropMapRetire','DropParentTest','DropParentRetire','DropRoleRetire')
hold('DropNonLast',nonlast,'Exact nonlast reference shell;retire never reduces canonical reserved.')
hold('DropLastRole',last,'Exact final-role shell;resource/state/new legacy cleanup have distinct already-held funds.')
hold('WorkerOrReaderRole',join(count('ReceiveWorker','ContextCancel','ContextInspect','ContextDecision','ContextReturn'),nonlast),'Original check-only caller is the greater shared wrapper case;all new adapter cancellation/caller/body work belongs LegacyFixed. Worker and Reader distinct role slices remain held.')
retained_end=dominating('retained acquire/abort',count('AcquireReader'),count('AbortReader'))
hold('RetainedRole',join(count('RetainedDestroy','RetainedDropCaller','ArmReader'),retained_end,last),'One Retained owner can exist after Result destruction;arm once and bound its acquire/abort branch. No whole-lifetime mutual exclusion with Result.')
acquire_caller=dominating('control acquire caller',count('DrainRetiring'),count('AcquireCurrentCaller'))
control=count(*'OwnerAdopt OwnerConstructed SlotErase ControlDestroy ControlDetachTest ControlDetach ControlDropCaller SessionDestroyCheck SessionDestroyRetiring SessionDestroyFinish ArmWorker RetainExternal AcquireWorker AbortWorker'.split())
hold('ControlRole',join(control,acquire_caller,last),'Complete Control administrative/adoption/slot/destruction/session/arm/abort/acquire/retained union. Result/new caller actions use separate fixed/cleanup funds. Current Take is an alternative actual acquire,not second acquire.')
hold('ResultRole',last,'Same final-reference shell reserved independently for Result. The owned cell destructor drops exactly that reference;no new origin on Take.')
reserve_terminal=dominating('capture reserve terminal',count('ReserveInstall'),count('ReserveFail','CaptureReserveRefused'))
hold('CaptureCheck',join(count('CaptureCheck','CaptureSizes','CaptureProducts','CaptureNeed','ReserveScan'),reserve_terminal),'Count/sum/byte admission prefix and actual reserve branch maximum. Earlier TooLarge/Overflow/Bytes refusal prefixes independently compared below.')
dominating('capture admission refusal prefixes',named('CaptureCheck'),
 count('CaptureCheck','CaptureTooLarge'),count('CaptureCheck','CaptureSizes','CaptureOverflow'),
 count('CaptureCheck','CaptureSizes','CaptureProducts','CaptureBytesRefused'))
hold('StateCleanup',count('StateDestroy'),'Eight retire invocations:root,check,five role,state;all actual five-role loop/source diagnostics and final native free tail.')
hold('MapCleanup',count('MapDestroy'),'One actual NativeStorage map release,no invented TempoChange destructor loop.')
hold('ParentCleanup',count('ParentDestroy'),'Two actual owner drops and full Access/Parents destructor;pointer-control ownership/ABI remains separate.')
hold('Snapshot',count('SnapshotFixed',SnapshotRow=6),'Snapshot fixed plus six prepaid rows before canonical reads;Raw535 and all phase cells included.')
assert len(S)==38

# Fully affine branch arithmetic. Source-bound masks are maxima over concrete
# arms;verification covers d0/d1 rather than testing one unexplained substitution.
def p(name):return E[name]['price_coefficients']
def ap(*values):return [sum(x) for x in zip(*values)]
def sp(c):return price(c)
def sum_sites(*nn):return sp(count(*nn))
def maximum(label,*aa):
    chosen=max(aa,key=lambda a:affine(a,d=1))
    assert all(chosen[1]==a[1] and all(affine(chosen,d=d)>=affine(a,d=d) for d in(0,1)) for a in aa),label
    comparisons.append(dict(label=label,case_prices=list(aa),chosen=chosen,domains='B>=0,d0/d1,equal B slope'))
    return chosen
snap=S['Snapshot']['price_constant_B_d']
post=maximum('postcapture branch',snap,ap(sum_sites('NewFailureTest','ReadoutCommit'),snap),
 ap(sum_sites('NewFailureTest','SessionSelect','SessionInstall','ReadoutCommit'),maximum('session selection',p('SessionKeepRunning'),p('SessionReusePending')),ap(snap,snap)))
failed_initial=sum_sites('RootFailedAdopt','RootFailedSnapshot','RootFailedConstructed','RootFailedSlotErase','RootFailedDestroy','RootFailedSessionCheck','RootFailedSessionFinish')
failed_next=sum_sites('RootFailedReadout','RootFailedDiscard','RootFailedSnapshot')
physical_prefix=ap(sum_sites('RootBegin','RootSessionInit','RootCounterIssue','RootIdentity','RootAllocate','RootObject','RootInstall','LegacyRootInstall','Distribute','LegacyDistribute','CaptureDispatch','CaptureResultTest','CaptureRefused'),post)
root_roles=['StateCleanup','CaptureCheck','ControlRole','WorkerOrReaderRole','RetainedRole','WorkerOrReaderRole','ResultRole','LegacyFixed','LegacyCheck','LegacyCleanup']
physical=ap(physical_prefix,*(S[n]['price_constant_B_d'] for n in root_roles))
no_state=ap(sum_sites('RootBegin','RootSessionInit','RootCounterIssue','RootIdentity','RootNoState'),
 maximum('no-state branch',p('RootIdentityRefused'),sum_sites('RootAllocate','RootAllocateFailed')),failed_initial)
root=maximum('root physical/no-state',physical,no_state)
comparisons.append(dict(label='initial/next failed tail',case_prices=[failed_initial,failed_next],chosen=maximum('failed-tail domination',failed_initial,failed_next)))
search=maximum('capture search iteration',
 ap(sum_sites('ClipLoopTest','ClipRow','ClipDecision'),maximum('clip found/continue',p('ClipFound'),p('ClipContinue'))),
 ap(sum_sites('AssetLoopTest','AssetRow','AssetDecision'),maximum('asset found/continue',p('AssetFound'),p('AssetContinue'))))
map_row=ap(sum_sites('MapLoopTest','MapRow','MapDecision'),maximum('map tick arm',p('MapFirst'),p('MapLater')),
 maximum('map invalid/store',p('MapInvalid'),sum_sites('MapStore','MapContinue')))
map_loop=sum_sites('MapLoopEntry','MapLoopTest','MapEnd','CaptureFinish')
allocation=ap(p('MapAllocate'),maximum('map allocation tail',p('MapAllocateFailed'),ap(p('MapInstall'),map_loop)))
map_tail=ap(p('MapNeededTest'),maximum('map absent/allocate',map_loop,allocation))
product=ap(p('SourceProduct'),maximum('source product tail',p('SourceProductRefused'),map_tail))
extent=ap(p('SourceExtent'),maximum('source extent tail',p('SourceExtentRefused'),product))
layout=ap(p('SourceLayout'),maximum('source layout tail',p('SourceLayoutRefused'),extent))
source_tail=ap(p('SourceCheck'),maximum('source owner tail',p('SourceMissing'),layout))
# LegacyCaptureOptions is NOT added here:its actual debit is legacyFixed.
metadata=ap(sum_sites('CaptureMetadata','CaptureParents'),source_tail)
search_fixed=sum_sites('ClipLoopEntry','ClipLoopTest','ClipEnd','ClipMissingTest')
asset=ap(search_fixed,maximum('asset capture tail',p('AssetMissing'),metadata))
clip=ap(search_fixed,maximum('clip capture tail',p('ClipMissing'),asset))
variable=ap(p('CaptureWorkBegin'),clip)
formation=dict(rootEnvelope=root,same_root_physical_fixed_constant_B_d=physical,no_state_constant_B_d=no_state,
 fixed_physical_prefix_constant_B_d=physical_prefix,postcapture_fixed=post,failed_initial_tail=failed_initial,failed_next_tail=failed_next,
 perSearchRow_constant_B_d=search,perMapRow_constant_B_d=map_row,variableCaptureFixed_constant_B_d=variable,
 needed='variableCaptureFixed+(clipCount+assetCount)*perSearchRow+R*perMapRow+MapCleanup+ParentCleanup',
 branch_expansions=dict(snapshot=snap,map_loop=map_loop,map_allocation=allocation,map_tail=map_tail,product=product,extent=extent,layout=layout,source=source_tail,metadata_parents=metadata,search_fixed_each=search_fixed,asset_fixed=asset,clip_fixed=clip),
 root_role_equation='physicalFixedPrefix +StateCleanup+CaptureCheck+Control+Worker+Retained+Reader+Result+LegacyFixed+LegacyCheck+LegacyCleanup',
 CaptureOptions_sponsor='Actual s.legacyFixed_ BEFORE worker;its one copy is included LegacyFixed,excluded variableCaptureFixed. No duplicate/different-slice sponsorship.')

region=price(dominating('native shared region unit',named('NativeComparedRegion'),named('NativeAccessRegion')))
page=price(dominating('native shared page unit',named('NativeComparedPage'),named('NativeAccessPage')))
terms=[('1','pendingWindowQueryFixed',S['QueryNewFixed']['price_constant_B_d']),
 ('P','pendingWindowQueryPerPrefix',S['PrefixIteration']['price_constant_B_d']),
 ('2*b','pendingWindowQueryPerSearch',S['SearchIteration']['price_constant_B_d']),
 ('E','OldQueryCatalogue.appendCeiling',[94,0,0]),('1','OldQueryCatalogue.queryCeilingFixed',[970,0,0]),
 ('b','OldQueryCatalogue.queryCeilingPerBit',[72,0,0]),('1','pendingNativeMatchFixed',S['NativeMatchFixed']['price_constant_B_d']),
 ('M','pendingNativeMatchPerRegion',region),('D','pendingNativeMatchPerRegion',region),
 ('L','pendingNativeMatchPerPage',page),('A','pendingNativeMatchPerPage',page),
 ('L','pendingNativeMatchPerByte',S['NativeComparedByte']['price_constant_B_d'])]
query=dict(symbols={'R':'captured map rows','P':'max(R-1,0)','b':'bit_width(R)','E':'R?R+1:0','M/L/D/A':'private unbound compared/access region and byte counts'},
 terms=[dict(count=n,unit_field=f,derived_price=a) for n,f,a in terms],
 quote='All12 checked terms execute even after refusal;failed term leaves total unchanged. Actual quoteTerm source multiplication guarded before evaluation.',
 old_callee='Exactly selected old190 vector protected separately BEFORE restoration;branchless commit consumes it once immediately before actual guaranteed entry. Ceilings94/970+72b never actual visits/debits.')

old=read('build-ci/20261105-musical-window-query-path-census-r1/census.json')
assert len(old['rows'])==190 and len(old['case_templates'])==13
old_compositions=[]
for name,r in old['rows'].items():
    assert sum(r['vector'])==r['price']
    if 'sum' in r:
        subtotal=[0]*10
        for child in r['sum']:
            if isinstance(child,str):k,n=child,1
            else:k,n=child
            subtotal=plus(subtotal,times(old['rows'][k]['vector'],n))
        assert subtotal==r['vector'],name
        old_compositions.append(dict(name=name,children=r['sum'],vector=subtotal,price=sum(subtotal)))
assert len(old_compositions)==78
protected=dict(path='build-ci/20261105-musical-window-query-path-census-r1/census.json',
 sha256=digest(ROOT/'build-ci/20261105-musical-window-query-path-census-r1/census.json'),
 rows=old['rows'],append_terminal_cases=old['append_terminal_cases'],case_templates=old['case_templates'],
 exact_compositions=old_compositions,no_repricing=True)

copy_cells=dict(LegacyRaw=94,Raw=535,Canonical=18,Snapshot=16,FormationReceipt=553,RootCensus=538,Formed=1093,Readout=554,Bounds=443,
 CapturedMetadata=263,Clip=192,Asset=51,EntityId=16,TempoChange=3,LegacyPreparationOptions=13,LegacyReadout=125,
 NativeBuildIdentity=6,NativeControl=3,NativeModule=3,NativeImage=3,NativePage=8,NativeSnapshot=13,MODULEINFO=3,
 MEMORY_BASIC_INFORMATION_x64=8,ActionVector=10,Endpoint=13,Prediction=13,AppendArguments=6,Search=3)
assert copy_cells['Raw']==420+21+94
assert copy_cells['FormationReceipt']==1+16+535+1
assert copy_cells['RootCensus']==535+3 and copy_cells['Formed']==1+1+553+538
assert copy_cells['CapturedMetadata']==16+192+51+1+3
assert copy_cells['LegacyReadout']==1+1+4+2+4+3*6+94+1

def q(R,M,L,D,A,d=0):
    c={'1':1,'P':max(R-1,0),'2*b':2*R.bit_length(),'E':R+1 if R else 0,'b':R.bit_length(),'M':M,'L':L,'D':D,'A':A}
    return sum(affine(t['derived_price'],d=d)*c[t['count']] for t in query['terms'])
def needed(ca,R,d=0):return affine(variable,d=d)+ca*affine(search,d=d)+R*affine(map_row,d=d)+affine(S['MapCleanup']['price_constant_B_d'],d=d)+affine(S['ParentCleanup']['price_constant_B_d'],d=d)
illustrations={str(R):q(R,9,8284,1,4) for R in (0,1,2,64,1024)}
B=4264;ca=2;R=1;rr=affine(root,B=B);nn=needed(ca,R);qq=q(R,9,8284,1,4)
before=rr+nn+qq+(1+6*R)+4096+1024
fit=dict(cap=268435456,unchanged=True,Data_defined=False,prices_bound=False,
 all_success_sufficient_inequality='root(B,d)+needed(clipCount+assetCount,R,d)+Q(R,M,L,D,A,d)+(1+6R)+4096+(1024+N*(8+16C))<=268435456',
 no_universal_admission=True,
 conditional_examples_only=dict(B4264_d0_separate_selected_R5_fact_not_R4_layout=True,R=R,clip_plus_asset=ca,M=9,L=8284,D=1,A=4,
 root=rr,needed=nn,Q=qq,Q_for_R=illustrations,before_N_term=before,C1_N1=before+24,C1_N_sufficient_ceiling=(268435456-before)//24,C2_N_sufficient_ceiling=(268435456-before)//40),
 limitations='Logical source proposal only. No R4 layout/offset/physical-fit/native/true-interval/forward proof or actual execution follows. Qualifying R5 B/d facts remain separate;generic B/d retained.')

# Every newly written case is compared against R3 only AFTER its independent
# row/helper definitions above have been generated. No fallback fill exists.
previous=read('build-ci/20261105-musical-funded-legacy-adapter-r4-census-r3/census.json')
deltas=[]
for e,prior in zip(events,previous['events']):
    assert (e['ordinal'],e['site'])==(prior['ordinal'],prior['site'])
    if (e['body_vector'],e['constant_vector'],e['d_vector'])!=(prior['body_vector'],prior['constant_vector'],prior['d_vector']):
        deltas.append(dict(site=e['site'],ordinal=e['ordinal'],previous_body=prior['body_vector'],fresh_body=e['body_vector'],
          previous_price=prior['price_coefficients'],fresh_price=e['price_coefficients'],source=e['source_operand_derivation']))
helper_deltas=[dict(helper=k,previous=previous['helpers'][k]['tuple'],fresh=h['tuple']) for k,h in helpers.items() if previous['helpers'][k]['tuple']!=h['tuple']]

# Full text inventory is navigation/support for the manual source operands,
# not a regex-derived execution trace or invented compiler field traversal.
inventory=[]
for file,text in texts.items():
    for line,raw in enumerate(text.splitlines(),1):
        code=raw.split('//',1)[0]
        tokens=[]
        for m in re.finditer(r'\b[A-Za-z_]\w*_\b',code):
            prefix_text=code[:m.start()].rstrip()
            kind='written_member_selection' if prefix_text.endswith(('.', '->')) else ('qualification_alone_X0' if prefix_text.endswith('::') else 'unqualified_identifier_or_initializer_label_X0')
            tokens.append(dict(name=m.group(),column=m.start()+1,classification=kind))
        if tokens:inventory.append(dict(file=file,line=line,source=raw,tokens=tokens))
initializer_sources=[]
for file,start,end,owner in [
 ('formation-r8.h',574,575,'U:Readout'),('formation-r8.h',594,596,'CaptureParents:Parents'),
 ('formation-r8.h',653,657,'RootIdentity:CapturedIntent'),('formation-r8.h',1348,1352,'C:ControlRequestOwner'),
 ('formation-r8.h',1468,1469,'RetainExternal:RetainedOriginOwner'),('formation-r8.h',1522,1524,'ReceiveWorker/Reader:ContextScope'),
 ('worker-result.h',18,23,'six FundedOwner Construct bodies'),('worker-result.h',72,73,'LegacyArtifactsConstruct:WorkerArtifacts')]:
    initializer_sources.append(dict(file=file,from_line=start,to_line=end,sponsor=owner,
       text='\n'.join(texts[file].splitlines()[start-1:end]),rule='Initializer labels are X0;real scalar destination/copy/constructor/default-member actions retain R/W/H/Z/O. Exact callable definitions/whole-file pins are authoritative.'))
source_inventory=dict(schema='FRESH_ALL_SOURCE_IDENTIFIER_BOUNDARY_INVENTORY',scope='All six selected C++ source texts. Lexical inventory includes declarations/comments-free code candidates and is not an executable parse,observed count,or price derivation.',
 qualified_id_rule='namespace::name and Class::staticData qualification alone X0. Written dot/arrow/subscript/address/deref/nonidentity casts remain separately counted. Literal production adds no R;ordinary conditional/arithmetic/cast actions remain even with constant operands. Only required constant-evaluation contexts and unevaluated operands are excluded.',
 source_files=source_pins[:6],identifier_occurrences=inventory,constructor_initializer_sponsors=initializer_sources,
 whole_source_review_groups=[
  dict(source='formation-r8.h:1..1785',sites='0..132 excluding external worker/artifacts',ownership='Formation admission/capture/owners/roles/cleanup plus current-added slice/copy checkpoints. Every original129 body freshly entered;no inherited129 acceptance.'),
  dict(source='worker-result.h:1..177',sites='133,136,207..233 and189/232 actual callers',ownership='All six physical owner construction/destruction instances,group lifetime,two result cells,origin retain/transfer/drop,publish,Take. No labels or bare fields charged selectors.'),
  dict(source='legacy-adapter.h:1..544',sites='135,137..206,347..350,402',ownership='Three real old semantic entries;allcommon/prepare/status/catch/cancel/caller/prebody paths. Chain402 and pre-worker CaptureOptions131 fixed sponsorship explicitly retained.'),
  dict(source='window-query.h:1..1030',sites='234..346,351..419 excluding402',ownership='All quote/predict/search/prefix/classification/replay/restoration/reconciliation/native region/page/byte/body/caller cases plus exact protected old row references.'),
  dict(source='native-observer.cpp/native-profile.h',sites='leaf helper cases,actualComputeIdentity',ownership='All fixed POD/native stack writes/parameters/possible named returns and selected public logical SDK leaves;no variable walks hidden in opaque SDK fees.')],
 outside_finite_schedule=[
  dict(source='Readout.status/disposition and Application.currentFormationStatus',state='Historical getters,not reached by admitted finite source schedule;actual future consumers need sponsorship,not free polling.'),
  dict(source='Site::LegacyLocalOwnerReturn',state='Retained enum without callable source use;unreachable,no fake reached zero-price body.'),
  dict(source='RootFailedSessionRetiring',state='Body retained but count0 under valid failed no-state-current/retiring invariant;not omitted from all420 inventory.')])

partitions=dict(root='RootBegin/session/entry/counter/identity/allocation/install/distribute/capturedispatch/postcapture/snapshot and129/130;logical reservation before first physical caller write.',
 captureCheck='25..32 and actual capture reserve79..81.',captureWork='33..76 and map/parent cleanup reservations;EXCLUDES131 because actual source debits legacyFixed.',
 roleLife='Five complete role slices coexist;original owner/context/reference administrative paths and dropping-role shells.',
 legacyFixed='All added ordinary adapter/control/result/caller/quote/402Chain actions INCLUDING131CaptureOptions,EXCLUDING root129/130 and cleanup events.',
 legacyCheck='Actual query+three semantic attempt reserve scans;four genuine calls maximum.',
 legacyCleanup='Six physical owner drops,group destructor,two cell destructors and owned/borrowed/control/abort/final retirement paths.',
 legacyQuery='All new matcher/check/classifier/replay/query/prediction/restoration/vector/digest/page/byte/resolve actions.',
 queryCallee='Only exact selected old append/resolve vector;protected split before restoration;immediate branchless commit once before old call.',
 legacyAttempt='Only old builder semantic fees at actual three calls,never every cumulative observation or future fee.')
lifetime=dict(maximum_simultaneous_origin_references=3,all_five_role_reservations_coexist=True,
 ordered_source_sequence=['Control','Control+Worker with embedded borrowed Cell','Control+Worker+Result after publish','Control+Result after worker drop',
 'Take transfers SAME Result to detached Cell;embedded becomes borrowed/moved-from','Owned Result cell destruction drops Result and clears resultOriginLive',
 'Control may subsequently issue Retained','Control+Retained then Control drops','Retained+Reader;each arm/receive-or-abort/acquire once'],
 six_owner_drops='Clock/Request/Plan local output plus Clock/Window/Request worker stages;each once,not deferred prepaid lifetime simulation.',
 physical_result_cells='At most embedded+one detached=2 destructors;one owned origin drop,one borrowed cleanup;mandatory returned prvalue has no extra temporary owner.',
 restoration='At most explicit+destructor restore prechecks2;actual restoreStatus<=1 because restoring false precedes result check. Physical restore before exact real resolve;no restoration afterward.',
 no_refund='Retire classifies unused reservations terminally;never refunds headroom or counts unused loop maxima actual.',
 no_after_free='Last role performs legacy/map/parent/state cleanup and only prepaid return tails remain after native State free.')
conservation=dict(start='One new Origin root reserved before initial request/argument/local writes. Scratch actual+unused imports exactly once;handedOff prohibits later authority.',
 pay='Same-Origin reserved->actual and exact reached rawSite counter;loop upper is reservation only.',
 split='Held Slice changes phase/owner without new allowance;no new root/subrequest/query origin.',
 attempted_builders='Prepare headroom includes all current actual/reserved and prior legacy entered contribution;guarded reserve then branchless commit immediately before one guaranteed old call.',
 cumulative='Clock1+6R,Request prior+4096,Plan prior+1024+N*(8+16C);observations compare already consumed work,never add it again.',
 protected_query='Exact old selected vector split before new restoration/bookkeeping;only commitOld consumes it.',
 termination='Cleanup and all prebody/caller tails prepaid before ownership/free. Retire never releases allowance.',
 acyclic='Literal helper operands ->Site prices ->finite schedules ->root/capture/Q coefficients ->future immutable bindings. No Site dynamically derives its own price.')
packet=dict(schema='YESDAW_PRODUCTION_FUNDED_ADAPTER_R4_FRESH_CENSUS_R4_V1',status='COMPLETE_SOURCE_PROPOSAL_UNBOUND_REQUIRES_FRESH_WHOLE_NUMERIC_REVIEW',
 vector_order=ORDER,contract='Production404E214B+SDK709B;qualification and bare identifier/initializer labels alone X0;ordinary read/write/copy/call remains.',
 target_manifest_sha256=source_pins[-1]['sha256'],copy_cells=copy_cells,
 symbolic_ABI=dict(B='sizeof exact selected Raw;R4 layout unqualified. Separate R5 B4264 only illustrative,not R4 equality/offset/physical fit.',d='0 if selected U64/size_t identical,1 otherwise. Generic term remains;separate selected R5 d0 does not transfer whole native/resource qualification.'),
 helpers=helpers,events=events,schedules=S,query_quote_12_terms=query,formation_equations=formation,
 old_callee_exact_catalogue=protected,aggregate_initialization_cases=dict(braced_LegacyReadout='125W=121zero+4nestedDMIs,no aggregate init ctor HZ;selected later assignment/copy separate.',default_member_LegacyReadout='4DMIs+3implicitconstructor HZ before Cell/Take,payment in actual Arm/Take caller.',Common_Prepared='Two requested/limits DMIs+two ctor HZ sponsored by Chain402 or stage Prepare before callee body.',references='No referent copy/store/address invented for plain reference binding;real written address/deref and byvalue transfers retained.'),
 fund_partitions=partitions,lifetime_schedule=lifetime,same_origin_conservation=conservation,conditional_fit=fit,
 source_boundary_corrections=dict(previous_whole_REVISEs_preserved=True,all420_freshly_entered=True,all51_helpers_freshly_entered=True,
 no_unflagged397_acceptance=True,changed_helper_vectors=helper_deltas,changed_event_factorizations=deltas,
 schedule_corrections=['Actual LegacyChainCaller402 added once to LegacyFixed;prior range129..233 failed to include it.',
 'LegacyCaptureOptions131 now belongs actual legacyFixed payer;removed from variable capture work,not double funded.',
 'Receive adapter case removes only old wrapper H;ContextDecision separately owns absent check-only selector. ContextReturn removes only old wrapper Z.'],
 source_prose_conflict='Earlier static catalogue/address wording and implicit-this/initializer explanations cannot sponsor nonexistent written operators. Bounds reference return X0;OldRow actual rows/subscript/cast X3;all qualifiers alone X0. Prior R3 partial NativeCompare24 retained an extra qualification X;fresh body23.'),
 source_bound_comparisons=comparisons,
 resource_HOLD=['R4 actual layout/offsets/totalphysicalresources;separate R5 declaration reorder/public B/d scope only.',
 'Exact immutable NativeBinding/Catalogue/OldQueryCatalogue data definitions and independent all420/51 numeric acceptance.',
 'Actual loaded native regions/codegen/libm/restoration/new-handler/FMA/control true intervals and full-domain prefix/query forward proof.',
 'Actual task/model/cancellation/thread/parent ownership/TSan and complete retained allocation/control blocks/stacks/resource pool/public recipe.',
 'SDK/native/OS CPU and allocator internals remain opaque,not zero or qualified by source vector counts.'],
 execution=dict(target_PS_parser_AST_import=False,target_compile_link=False,target_helper_program_test_stage=False,native_GUI_devices_Git_CI=False,
 ordinary_new_Python_file_JSON_hash_integer_generation_only=True),
 self_checks=dict(events=420,helpers=51,schedules=38,old_rows=190,old_compositions=78,old_templates=13,body_defaults_from_old_packet=0,
 fresh_all_row_helper_source_basis=True,source_prices_accepted=False,Data_bound=False,fit_or_GO=False))

# Exact reached schedule coverage:all nonempty bodies except proved source-only
# unreachable cases must appear in root/capture/manual variable/fixed/role paths.
covered=set(k for s in S.values() for k in s['site_counts'])
covered.update('RootBegin RootSessionInit RootNextEntry RootCounterAlias RootCounterIssue RootCounterExhausted RootIdentity RootIdentityRefused RootAllocate RootAllocateFailed RootObject RootInstall RootNoState RootFailedAdopt RootFailedConstructed RootFailedReadout RootFailedDiscard RootFailedSnapshot RootFailedDestroy RootFailedSlotErase RootFailedSessionCheck RootFailedSessionFinish Distribute CaptureDispatch CaptureTooLarge CaptureOverflow CaptureBytesRefused CaptureWorkBegin ClipLoopEntry ClipLoopTest ClipRow ClipDecision ClipFound ClipContinue ClipEnd ClipMissingTest ClipMissing AssetLoopEntry AssetLoopTest AssetRow AssetDecision AssetFound AssetContinue AssetEnd AssetMissingTest AssetMissing CaptureMetadata CaptureParents SourceCheck SourceMissing SourceLayout SourceLayoutRefused SourceExtent SourceExtentRefused SourceProduct SourceProductRefused MapNeededTest MapAllocate MapAllocateFailed MapInstall MapLoopEntry MapLoopTest MapRow MapFirst MapLater MapDecision MapInvalid MapStore MapContinue MapEnd CaptureFinish CaptureResultTest CaptureRefused ReadoutCommit NewFailureTest AbandonNewState SessionSelect SessionKeepRunning SessionReusePending SessionInstall LegacyRootInstall LegacyDistribute'.split())
# Dominated concrete alternatives remain explicitly covered by maxima/branches.
covered.update('ReceiveReader AcquireCurrentCaller AbortReader QueryNativeAccepted QuerySearchTrue QueryPrefixContinue QueryOldPartialReturned QueryPartialLastArgs QueryReconcileEmpty NativeMatchByteMismatch ReserveInstall'.split())
excluded={'LegacyLocalOwnerReturn','RootFailedSessionRetiring'}
assert set(names)==covered|excluded,sorted(set(names)-covered-excluded)
assert all(type(x)is int and x>=0 for e in events for x in e['constant_vector']+e['B_vector']+e['d_vector'])
assert all(pin(SOURCE/n)==p for n,p in zip(source_names,source_pins))
save('source-inventory.json',source_inventory)
save('all-420-factorizations.json',dict(schema='FRESH_FULL_420_BODY_HELPER_ABI_FACTORIZATIONS',vector_order=ORDER,rows=events,helpers=helpers))
save('census.json',packet)

md=['# Fresh production census R4 proposal','',
 '**All420 source bodies and51 helpers rebuilt;whole independent numeric review and all binding/native/resource/GO gates remain pending.**','',
 'This is a new production404E214B/709B packet for the exact nine-file R4 source. No prior unflagged row was accepted by default. The sibling body/helper ledgers are freshly authored inputs;the generator has no old-row fallback. Old JSON is read only after fresh construction,for differences and the protected exact190 catalogue. No target parser/compiler/helper/program/stage/native/UI/Git was run.','',
 'Each body has its complete proposed ten-component vector,source operand description,explicit helper coefficients,caller/bare-name/initializer/prebody sponsor boundary and current literal source anchors. The full text inventory records every underscore-member candidate in all six source texts;it is navigation and cannot turn lexical counts into execution evidence. All420 helper/body/B/d factorizations are emitted independently of the former prices.','',
 'Qualification alone and unqualified own names/initializer labels add X0. Actual scalar reads/writes and every written dot/arrow/subscript/deref/address/nonidentity dynamic cast remain charged. References copy no referent. The earlier Bounds/static-address prose does not create a written address operator. OldRow retains rows/subscript/cast X3. The old190 vectors/78 compositions/13 templates are preserved exactly and not repriced.','',
 'The wider source audit changes constructors and original formation paths as well as native bodies. RootInstall is now explicitly R621/W637/X125:the W expansion is535+18+2+2+4+20+25+13+12+5+1;the source operation packet lists every term. Distribute X52 is caller fields31+role increments5+diagnostics15+ordinary RHS addition1. These are new proposed counts for independent review,not rounding to old totals.','',
 'Required constant-evaluation contexts(constexpr initialization,static_assert,type/array extents) and sizeof unevaluated operands have no runtime actions. Ordinary written ?:,arithmetic,unary complement/minus and nonidentity casts retain C/X even with constant operands. Reader is R0 but its ordinary selections remain conditions. ContextInspect has five actual identity/state checks plus Reader-role selection C6;two metadata bools copied into an integral data mask add no condition. The native expected-size cast adds d when size_t differs from U64;the DWORD sizeof cast is nonidentity on selected Windows x64.','',
 'Three schedule/sponsor repairs are explicit:ordinal402 LegacyChainCaller is included once in LegacyFixed;LegacyCaptureOptions131 is held in its actual legacyFixed payer and removed from variable capture;the adapter Receive case removes only old wrapper H,with absent check selector already owned by missing ContextDecision. ContextReturn removes only old wrapper Z. No allowance from another typed slice or old Worker role remainder supplies missing work.','',
 'Source tuple order is `(R,W,C,X,LE,H,Z,O,A,F)`. Each Site price is sum(body+named helper expansions+B-vector*B+d-vector*d). All conditional branch maxima are checked at d0/d1,with equal B slopes;finite unions are reservations,never asserted actual visits.','',
 table(['Held schedule','constant+B*B+d*d'],[(n,fmt(s['price_constant_B_d'])) for n,s in S.items()]),'',
 'All five roles coexist in reservation. Actual maximum references is3(Control+Worker+Result);after Result destruction Control may issue Retained and Reader. Six actual stage/output owner drops and at most two physical result cells(one owned,one borrowed) are separately sponsored before destruction. Take transfers the same origin,not a new one. Retirement never refunds headroom;no state reads follow possible final free.','',
 'Every vector add/sum/record owns10 cells,11 tests,10 increments and one entry/end. Two searches each have at most bit_width(R) positive steps. Two endpoints share the monotone full-prefix cursor,so full append rows<=R-1;each endpoint has<=one partial append. Append owns three possible checks. The fixed query owns nine other checks:Clock2,endpoint extras4,restore prechecks2,post-resolve1. At most one physical restore;explicit failed precheck may be repeated by destructor.','',
 'Native admission is once before Clock. Identity visits<=17facts*32digest bytes,with18outer and17*33inner tests. Compared region rows<=M,address-only rows<=D,compared pages<=L,address pages<=A,compared byte bodies<=L because completed positive chunks advance>=1byte. Per-region/page fixed tails and final tests are included. Raw mutable dispatch words are address-only,never compared. Native/profile/source proofs remain absent.','',
 table(['Query unit','Multiplier','Proposed upper'],[(t['unit_field'],t['count'],fmt(t['derived_price'])) for t in query['terms']]),'',
 'All12 quote terms execute even after refusal;the rejected term leaves total unchanged. Exact old append/resolve vector is selected and protected before later restoration/new work,then consumed by a branchless commit immediately before one guaranteed old call. Old94 and970+72b ceilings reserve only. Clock1+6R,Request4096,and Plan1024+N*(8+16C) each become actual once at real entry;returned cumulative observations compare already-consumed work.','',
 table(['Root/capture quantity','Proposed affine'],[(k,fmt(v)) for k,v in formation.items() if isinstance(v,list)]),'',
 '`needed=variableCaptureFixed+(clipCount+assetCount)*perSearchRow+R*perMapRow+MapCleanup+ParentCleanup`. CaptureOptions is excluded from this expression because its actual debit belongs LegacyFixed. All capture early-failure/found/missing/invalid branches remain explicit;count guards do not prove actual visits.','',
 '`root(B,d)+needed+Q(R,M,L,D,A,d)+(1+6R)+4096+(1024+N*(8+16C))<=268435456` is a sufficient held-source upper,not universal admission. The unchanged source reserve/headroom guards can refuse. No cap or source/domain/SDK/ADR was changed.','',
 'Conditional illustration only with separately selected R5 B4264/d0,R1,clip+asset2,and unbound candidate M9/L8284/D1/A4:root '+str(rr)+',needed '+str(nn)+',Q '+str(qq)+',before-N total '+str(before)+'. C1/N1 gives'+str(before+24)+';the sufficient N ceilings are'+str((268435456-before)//24)+'(C1) and'+str((268435456-before)//40)+'(C2). This is neither measured R4 layout nor physical/native/full fit. Generic B/d remains.','',
 'Brace LegacyReadout125 cells=121zero+fourDMIs,with no aggregate initialization constructor fee;later assignment/copy is separate. Default member readout_ before Cell/Take has fourDMIs+three implicit ctor HZ,paid at actual Arm/Take. Common/requested and PreparedAttempt/limits prebody defaults belong Chain402/stage Prepare. Native POD/native stack/API caller/possible named-return copies are explicit;opaque OS/CRT/SDK CPU is not certified zero.','',
 'All accepted contracts,original190 vectors/78 compositions,prior failed packets/reviews,source/caps/goldens remain untouched. R4 C4324 and separate R5 reorder/public ABI facts do not transfer offset equality or total allocation fit. Actual native loaded-region/codegen/libm/restoration/true-interval/forward proof,parent ownership,real task integration,TSan,complete heap/control-block/retained graph/stacks/resource pool and public recipe remain HOLD. Data and prices remain undefined.','',
 table(['#','Site','Proposed price','Body vector'],[(e['ordinal'],e['site'],fmt(e['price_coefficients']),','.join(map(str,e['body_vector']))) for e in events]),'',
 table(['Helper','Proposed vector','Price'],[(k,','.join(map(str,h['tuple'])),h['price']) for k,h in helpers.items()]),'',
 'This complete packet is ready only for fresh whole independent numeric review. Its generator checks file/JSON/hash/integer correspondence;it cannot mechanically certify its handwritten source price premises.','']
(HERE/'census.md').write_bytes(('\n'.join(md)).encode())

refs=['build-ci/20261105-production-operation-boundary-census-conventions.md','build-ci/20261105-production-query-sdk-boundary-clarification.md',
 'build-ci/20261105-musical-origin-formation-r4-census-r2/census.json','build-ci/20261105-musical-origin-formation-r4-census-r2/census.md',
 'build-ci/20261105-musical-window-query-path-census-r1/census.json','build-ci/20261105-musical-window-query-path-census-r1-independent-review.json',
 'build-ci/20261106-production-qualified-id-boundary-independent-review-r1/review.json',
 'build-ci/20261106-production-constant-expression-boundary-independent-review-r1/review.json',
 'build-ci/20261106-production-constant-cast-context-independent-review-r1/review.json',
 'docs/adr/0085-owned-musical-call-recipes.md','docs/adr/0088-musical-origin-formation.md']
for rev in ('r1','r2','r3'):
    refs += ['build-ci/20261105-musical-funded-legacy-adapter-r4-census-'+rev+'/'+n for n in ('census.json','census.md','source-pins.json')]
for folder in ('20261105-musical-funded-legacy-adapter-r4-numeric-independent-review',
 '20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2','20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r3'):
    refs += ['build-ci/'+folder+'/'+n for n in ('review.json','source-pins.json')]
refs += ['src/engine/MusicalAudioWindow.h','src/engine/Time.h','src/engine/Project.h',
 'src/engine/MusicalAudioChannelPlan.h','src/engine/ClipSchedule.h',
 'build-ci/20261105-musical-funded-r5-conversion-identity-r1/identity-record.json',
 'build-ci/20261105-musical-funded-r5-layout-object-r1/layout-record.json']
for rel in refs:assert (ROOT/rel).is_file(),rel
save('source-pins.json',dict(schema='FRESH_PRODUCTION_R4_COMPLETE_CENSUS_R4_PINS',status='SEALED_COMPLETE_PROPOSAL_PENDING_WHOLE_NUMERIC_REVIEW',
 subject=source_pins,reference_files=[pin(ROOT/r) for r in refs],
 authored_sources=[pin(HERE/n) for n in ('body-ledgers.py','helper-ledgers.py','build-census.py')],
 outputs=[pin(HERE/n) for n in ('census.json','census.md','source-inventory.json','all-420-factorizations.json')],
 no_target_execution=True,no_Data_or_price_binding=True,source_hypothesis='All420 and51 explicitly entered from selected source;no previous unflagged price acceptance or generator execution.'))
for n in ['census.json','census.md','source-inventory.json','all-420-factorizations.json','source-pins.json']:
    assert b'\r' not in (HERE/n).read_bytes();print(n,(HERE/n).stat().st_size,digest(HERE/n))
print('SOURCE_ONLY_INTEGER_PACKET',len(events),len(helpers),len(S),len(old_compositions),'UNBOUND_NO_TARGET_EXECUTION')
