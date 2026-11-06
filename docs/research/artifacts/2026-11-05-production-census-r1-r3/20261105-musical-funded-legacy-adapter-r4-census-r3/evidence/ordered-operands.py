"""Source-authored ordered production operands, not a scalar-price correction."""
from pathlib import Path
from hashlib import sha256
import json

HERE=Path(__file__).resolve().parent
BASE=HERE.parent
ROOT=BASE.parent.parent
ORDER=['R','W','C','X','LE','H','Z','O','A','F']
def v(**kw):return [kw.get(k,0) for k in ORDER]
def p(source,**kw):return {'source_operation':source,'vector':v(**kw)}
rows=[]
def add(site,anchor,parts,sponsors):
    for i,part in enumerate(parts):part['order']=i+1
    rows.append({'site':site,'source_anchor':anchor,'ordered_body_operands':parts,'body_vector':[sum(a['vector'][i] for a in parts) for i in range(10)],'callee_or_earlier_sponsors':sponsors,'scope':'Maximum reached body/caller case under the production logical convention; not an observed trace. Exact written operands are grouped in source order; no literal read fee or reference referent copy.'})

add('LegacyQueryReserve','legacy-adapter.h:143-157',[
 p('reserveQuery own entry and return',H=1,Z=1),
 p('U64 amount; has no scalar initialization store'),
 p('quote(s,c.rows,amount): one actual c.rows caller value read and field selector; returned-bool guard',R=1,C=1,X=1),
 p('Ledger::reserve: actual amount caller read, s.legacyCheck_ and s.legacyQuery_ written selectors, returned-bool guard',R=1,C=1,X=2),
],['Fixed owns this checkpoint implementation.','QueryQuote owns rows parameter W and the actual first amount=0 write; declaring the uninitialized local adds no W.','ReserveScan owns reserve amount parameter W and actual scan/body; ready/refuse bodies retain their separate Sites.'])
add('LegacyAttemptPrepare','legacy-adapter.h:159-173',[
 p('prepare stage,fee,inheritedWorkLimit,inheritedByteLimit scalar parameter destinations',W=4),
 p('prepare own entry/return',H=1,Z=1),
 p('index=static_cast<size_t>(stage)',R=1,W=1,X=1),
 p('index>=3',R=1,C=1),
 p('s.raw_.legacy.invocations[index]!=0: actual cell and index reads; three members and builtin subscript',R=2,C=1,X=4),
 p('s.legacyAttempt_[index].remaining!=0: actual cell and index reads; member/subscript/member',R=2,C=1,X=3),
 p('eager bitwise OR joining those invariant comparisons',X=1),
 p('existingTotal=total(s); returned local write and existingTotal>workCap',R=1,W=1,C=1),
],['total own entry, scan, incoming field loads, arithmetic and return belong LegacyBudgetEntry/Test/Row/Continue/End.','All reference parameters bind without copying any referent. Four scalar caller reads, where named, belong the actual stage callers; no incoming-parameter R is invented.'])
add('LegacyAttemptLimit','legacy-adapter.h:174-186',[
 p('headroom=workCap-existingTotal',R=1,W=1,X=1),
 p('existingLegacy=s.legacyEnteredContribution_',R=1,W=1,X=1),
 p('existingLegacy>workCap-headroom',R=2,C=1,X=1),
 p('originCumulativeCeiling=existingLegacy+headroom',R=2,W=1,X=1),
 p('effectiveWork nested minima: c.requested.maxWorkUnits, inheritedWorkLimit and originCumulativeCeiling named caller values; destination',R=3,W=1,X=2),
 p('(existingLegacy>effectiveWork)|(fee>headroom): four named reads, two comparisons, eager OR',R=4,C=2,X=1),
],['Minimum occurs three times with each actual parameter/body/selected return retained in the helper.','workCap and engine maximum constants have no production read/conversion fee.','Returned minimum temporaries are not invented named caller R operations.'])
add('LegacyClockCommitInvoke','legacy-adapter.h:247-249',[
 p('commitPrepared(s,prepared) reference call adds no caller referent or fee read'),
 p('old build value inputs map2/rate1/prepared.limits2, their actual parameter destinations and limits selector',R=5,W=5,X=1),
 p('three selected aggregate copy constructors and one visible old build call',H=4,Z=4),
 p('returned scalar status local destination',W=1),
],['CommitPrepared helper owns all17 real internal reads,6 writes, its ix call and own H/Z.','Clock builder semantic1+6R is separate, unchanged and consumed once immediately after the branchless commit.','No duplicate caller destination or reference read is added.'])
add('LegacyClockReturned','legacy-adapter.h:250-254',[
 p('s.raw_.legacy.returned[0]+=1: cell read/write, three members,builtin subscript and addition',R=1,W=1,X=5),
 p('result.clockStatusKnown=true',W=1,X=1),
 p('result.clockStatus=status',R=1,W=1,X=1),
 p('native.check() written selector and returned-bool guard',C=1,X=1),
 p('status!=Ok',R=1,C=1),
],['NativeCheck retains its read/compare/control/return Sites; this body owns only the literal native.check caller selector/guard.'])
add('LegacyClockException','legacy-adapter.h:280-284',[
 p('s.raw_.legacy.exceptions[0]+=1: read/write and five written operations',R=1,W=1,X=5),
 p('result.outcome=UnexpectedLegacyException',W=1,X=1),
 p('discarded native.check() still has its written selector; no discarded-result condition',X=1),
],['NativeCheck retains its complete body.','Literal false return has no R; the clock function entry/return belongs LegacyClockPrepare.'])
add('LegacyRequestPreflight','legacy-adapter.h:308-317',[
 p('request own entry/return',H=1,Z=1),
 p('result.stage=Request',W=1,X=1),
 p('s.legacyOptions() and artifacts.window_.value() reference callers: one and two selectors',X=3),
 p('options.policy.channelRevision!=1',R=1,C=1,X=2),
 p('options.policy.materializationRevision!=1',R=1,C=1,X=2),
 p('eager OR between policy comparisons',X=1),
],['Two Access helper bodies retain their own H/Z/reference operations; they do not own these written callers.','References options/retainedWindow create no aggregate copy or destination W.'])
add('LegacyRequestBytes','legacy-adapter.h:321-328',[
 p('retainedWindow.clock() caller feeding limits()',X=1),
 p('byteLimit=minimum(c.byteLimit,clockLimits.maxReservedBytes): two field reads/selectors and scalar destination',R=2,W=1,X=2),
 p('retainedWindow.clock() caller feeding reservedBytes(); clockBytes scalar destination',W=1,X=1),
 p('clockBytes>byteLimit',R=2,C=1),
 p('keyBytes>byteLimit-minimum(clockBytes,byteLimit): three named reads, subtraction,comparison',R=3,C=1,X=1),
 p('eager OR',X=1),
],['GetterReference2/ARROW_HANDLE2 retain actual clock reference/arrow boundaries once.','GetterTwo owns two limit-cell R/W and getter+selected copy H/Z; GetterScalar owns the actual bytes-cell R.','Minimum2 retains actual two parameter destinations/body/return.','constexpr keyBytes/requestBytes sizeof+64 do not execute a runtime read or expression.'])
add('LegacyRequestCommitInvoke','legacy-adapter.h:350-354',[
 p('commitPrepared(s,prepared) is a reference call with no extra fee or referent R'),
 p('metadata.clip,metadata.asset,parents.original,parents.readyView reference designators and prepared.limits selector',X=5),
 p('mapped2,tier1,identity6,policy2,limits2 value transfers',R=13,W=13),
 p('four selected aggregate copy constructors and one visible old factory call',H=5,Z=5),
],['CommitPrepared retains all17 actual internal reads including every prepared.fee use.','The two-cell returned result prvalue retains its original callee production; mandatory elision adds no caller copy/destination.','Unchanged4096 semantic debit is separate and once.'])
add('LegacyRequestReturned','legacy-adapter.h:355-361',[
 p('s.raw_.legacy.returned[1]+=1',R=1,W=1,X=5),
 p('result.requestStatusKnown=true',W=1,X=1),
 p('result.requestStatus=status: actual two-cell assignment and explicit destination',R=2,W=2,X=1,H=1,Z=1),
 p('result.observations[1].cumulativeEvidence=SourceProved',W=1,X=3),
 p('result.observations[1].cumulative=s.legacyEnteredContribution_',R=1,W=1,X=4),
 p('status.status!=Ok',R=1,C=1,X=1),
],['No compiler-generated per-field designator traversal is invented for the two-cell assignment.','Constant builtin array index1 still contributes one reached X.'])
add('LegacyRequestException','legacy-adapter.h:375-378',[
 p('s.raw_.legacy.exceptions[1]+=1',R=1,W=1,X=5),
 p('result.outcome=UnexpectedLegacyException',W=1,X=1),
],['There is no native.check invocation in this catch.','The enclosing request entry/return belongs LegacyRequestPreflight.'])
add('LegacyPlanExtent','legacy-adapter.h:399-401',[
 p('selected.srcLen==0',R=1,C=1,X=1),
 p('selected.srcOffset>mapped->frames: actual scalar field reads and selected field selector',R=2,C=1,X=1),
 p('mapped->frames>max()/channels: actual frame/channel reads,division,comparison',R=2,C=1,X=1),
 p('two eager OR operators',X=2),
],['ARROW_HANDLE2 retains each written mapped arrow once; no additional arrow selector is added.','MAX2 owns the visible max() logical call; compile-time type syntax is not a runtime operand.'])
add('LegacyPlanProduct','legacy-adapter.h:402-405',[
 p('selected.srcLen>mapped->frames-selected.srcOffset',R=3,C=1,X=3),
 p('mapped->interleaved.size()!=mapped->frames*channels: actual frame/channel reads,size selector,product,comparison',R=2,C=1,X=2),
 p('selected.srcLen!=input->window().inputFrames(): selected field read plus its selector and inputFrames selector/comparison',R=1,C=1,X=2),
 p('two eager OR operators',X=2),
],['ARROW_HANDLE4 owns the four written arrow logical boundaries once.','EXT3 owns the size extent read/HZ. GetterReference and GetterScalar own their actual reference/scalar bodies, not the explicit .inputFrames caller.'])
add('LegacyPlanBytes','legacy-adapter.h:406-414',[
 p('input->window().clock()->limits(): .clock selector outside arrow/reference/getter helper bodies',X=1),
 p('input->limits(): both limits results are actual two-cell copied values in GetterTwo helpers'),
 p('byteLimit nested minima: three explicit caller field reads/selectors and destination',R=3,W=1,X=3),
 p('retainedBytes=input->accounting().reservedBytes: scalar field read/selector and destination',R=1,W=1,X=1),
 p('retainedBytes>byteLimit',R=2,C=1),
 p('planBytes>byteLimit-minimum(retainedBytes,byteLimit): three named reads, subtraction,comparison',R=3,C=1,X=1),
 p('eager OR',X=1),
],['ARROW_HANDLE4, GetterReference3, GetterTwo2 and Minimum3 retain their complete logical bodies once.','The two corrected getters each own2R/2W and getter+copy H/Z; no second local/caller destination is charged.','constexpr planBytes sizeof+64 remains a constant expression.'])
add('LegacyPlanCommitInvoke','legacy-adapter.h:433-435',[
 p('commitPrepared(s,prepared) has no caller fee/referent read'),
 p('prepared.limits two-cell value input and actual parameter destination, explicit limits selector',R=2,W=2,X=1),
 p('selected two-cell copy constructor and one visible old plan call',H=2,Z=2),
],['CommitPrepared owns all17 actual body reads.','Returned plan-status prvalue has mandatory elision and no additional caller aggregate copy.','Unchanged1024+N*(8+16C) semantic debit remains separate, once.'])
add('LegacyPlanReturned','legacy-adapter.h:436-454',[
 p('s.raw_.legacy.returned[2]+=1',R=1,W=1,X=5),
 p('result.planStatusKnown=true',W=1,X=1),
 p('result.planStatus=status: two-cell assignment plus explicit destination',R=2,W=2,X=1,H=1,Z=1),
 p('result.observations[2].cumulativeEvidence=SourceProved',W=1,X=3),
 p('result.observations[2].cumulative=s.legacyEnteredContribution_',R=1,W=1,X=4),
 p('two status.status full-scan comparisons and eager OR',R=2,C=2,X=3),
 p('later status.status!=Ok after possible FullScan',R=1,C=1,X=1),
],['LegacyPlanFullScan owns only its entered stores/arithmetic/counter, not these surrounding tests.','Assignment copies scalar cells with one selected assignment H/Z, not generated per-field selectors.'])
add('LegacyPlanFullScan','legacy-adapter.h:446-451',[
 p('framesVisitedEvidence and samplesVisitedEvidence observation destinations',W=2,X=6),
 p('frames=selected.srcLen: observation destination and actual selected field read',R=1,W=1,X=4),
 p('samples=selected.srcLen*channels: observation destination,field read,channel read,product',R=2,W=1,X=5),
 p('s.raw_.legacy.knownFullScanOutcomes+=1',R=1,W=1,X=4),
],['Four observation destinations are each member/subscript/member, including the constant index.','Source-proved evidence is literal current returned control flow; no discarded private accounting/allocator trace is claimed.'])
add('LegacyPlanException','legacy-adapter.h:473-476',[
 p('s.raw_.legacy.exceptions[2]+=1',R=1,W=1,X=5),
 p('result.outcome=UnexpectedLegacyException',W=1,X=1),
],['No native.check is invoked in this catch.','The enclosing plan entry/return belongs LegacyPlanPreflight.'])
add('LegacyWorkerCaller','legacy-adapter.h:539-548',[
 p('workerBuildLegacy own entry/return',H=1,Z=1),
 p('auto& s=*context.state_: actual pointer read,member selector and dereference; no referent copy',R=1,X=2),
 p('owner.resultLive_ invariant field load/selector/test',R=1,C=1,X=1),
 p('owner.result() actual caller selector; class-qualified static run adds no object selector',X=1),
],['ReceiveWorker concrete new-adapter caller case already sponsors owner.receiveWorker selector; it is not counted again.','Access owns owner.result body. run/ContextScope actual body,cleanup and return scopes retain their existing distinct source funding.'])
add('QueryEntry','window-query.h:995-1001',[
 p('resolve own entry/return',H=1,Z=1),
 p('native.beginReplay() actual selector/returned-bool guard',C=1,X=1),
 p('start=s.metadata().clip.timelineStart',R=1,W=1,X=3),
 p('length=s.metadata().clip.timelineLength',R=1,W=1,X=3),
 p('frames=s.legacyOptions().mappedWindow.srcLen',R=1,W=1,X=3),
 p('Prediction predicted{} thirteen scalar cell initializations',W=13),
 p('predict start,length,frames actual named caller reads and returned-bool guard',R=3,C=1),
],['Access3 owns only the actual reference getter bodies.','QueryPredictEntry owns the three scalar parameter destinations and real predict body; it does not invent incoming-parameter reads.','beginReplay own body has ReplayFixed/NativeCheck source funding.'])
add('QueryEndpointMap','window-query.h:890-896',[
 p('search(s,tick) actual tick caller read',R=1),
 p('prefixTo found.index and rate named caller values, explicit found.index selector,returned-bool guard',R=2,C=1,X=1),
 p('out.frame=prefix',R=1,W=1,X=1),
 p('tick>s.map_[found.index].tick: tick,map pointer,index,row tick actual loads; four explicit designators',R=4,C=1,X=4),
],['SearchEntry/PrefixEntry own parameter writes, not these caller reads. SearchEnd owns its three-cell returned source/copy case.','Packet and VectorEntry own their concrete old-id/t/f/path incoming expressions; no second packet transfer is inserted here.'])
add('QueryEndpointPartial','window-query.h:898-920',[
 p('row reference=s.map_[found.index]: actual map/index reads and member,member,subscript designators',R=2,X=3),
 p('next=found.index+1<s.mapConstructed_: two reads,local write,comparison and three operations',R=2,W=1,C=1,X=3),
 p('if(next) selected argument-construction branch',R=1,C=1),
 p('append returned-bool guard',C=1),
 p('finishEndpoint returned&&isfinite(out.frame): returned/out.frame reads,short circuit and field selector',R=2,C=1,X=1),
 p('final native.check() written selector',X=1),
],['Math1 owns isfinite parameter destination and visible H/Z.','PartialNextArgs/LastArgs own six-cell argument construction/assignment, complete AppendPrefix sponsorship and actual out.frame append-reference selector.','QueryPacket concrete cases own the next/returned ternary incoming tests/reads and selected packet transfers. No duplicate packet caller action belongs here.'])
add('QueryPredictionEndpoints','window-query.h:947-957',[
 p('at=0 and prefix=0.0 scalar first writes',W=2),
 p('threshold: rows().size selector,constant-times-named-result product and addition,local destination',W=1,X=3),
 p('eligible: limits().maxWorkUnits field read/selector,threshold read,comparison and local destination',R=2,W=1,C=1,X=1),
 p('rate=s.metadata().projectRate.hz: actual scalar load,three written selectors and destination',R=1,W=1,X=3),
 p('Endpoint a{},b{}: two thirteen-scalar aggregate initializations',W=26),
 p('endpoint A actual start,eligible,rate named caller reads and returned-bool guard',R=3,C=1),
 p('a.status!=Ok field read,selector and condition',R=1,C=1,X=1),
],['ARROW_HANDLE2, ReturnSpanValue1, EXT3, Math1, GetterTwo1 and Access1 retain complete distinct bodies once.','ReturnSpanValue and GetterTwo each own2R/2W plus their actual getter/construction H/Z; no duplicated destination W.','The named U64/size_t cast contributes separately X*d; constants and constant-only casts have no runtime fee.','QueryPacket/VectorEntry own actual path/packet/vector caller operands. Rate here is a stored metadata field, not another SampleRate value getter.'])

review_path=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2/review.json'
assert sha256(review_path.read_bytes()).hexdigest().upper()=='AD543B6DBDF99D2C471B7FAD9405B04ED367FA7EB778955EB113152C420600BB'
review=json.loads(review_path.read_bytes())
old=json.loads((ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r2/census.json').read_bytes())
by_name={e['site']:e for e in old['events']}
expected={e['site']:e for e in review['necessary_site_corrections_if_other_components_retained']}
assert len(rows)==23 and set(expected)=={r['site'] for r in rows}
for r in rows:
    previous=by_name[r['site']]
    r['ordinal']=previous['ordinal']
    r['old_body_vector']=previous['body_vector']
    r['delta_vector']=[a-b for a,b in zip(r['body_vector'],previous['body_vector'])]
    # This is a cross-check after source-ledger construction, not the body definition.
    assert r['delta_vector']==expected[r['site']]['delta_vector'],(r['site'],r['body_vector'],r['delta_vector'])
result={'schema':'PRODUCTION_R4_CENSUS_R3_ORDERED_BODY_CALLER_OPERANDS_V1','vector_order':ORDER,'production_conventions':'404E214B/709B8A43','target_manifest_sha256':'A5353D86D20DE812A75D6345780C6B489670DD61EB40AF87C4D2CE6776B28077','scope':'Complete source-ordered body vectors for all23 affected packets, including every component rather than only the25 witnessed errors. Full420 event/helper/schedule recomposition remains a new proposal for fresh whole critic.','rows':sorted(rows,key=lambda r:r['ordinal']),'review_witness_crosscheck':{'affected_sites':23,'component_witnesses':25,'all_own_source_rebuilt_vectors_match_necessary_deltas':True},'execution':'Own ordinary Python integer/JSON/file/hash only. No target parser,compiler,helper,program,stage,native,GUI,Git orCI.'}
(HERE/'ordered-operands.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'rows':len(rows),'ordered_source_parts':sum(len(r['ordered_body_operands']) for r in rows),'body_vectors':{r['site']:r['body_vector'] for r in result['rows']}},indent=2))
