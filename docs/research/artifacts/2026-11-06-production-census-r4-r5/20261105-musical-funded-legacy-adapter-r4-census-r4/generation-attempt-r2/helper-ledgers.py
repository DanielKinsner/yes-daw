"""Fresh production helper operands. Data definitions only; no target code calls.

These are proposals derived from current source, not imported accepted prices.
Written designators and bare own names are distinct. Full numeric review remains.
"""
ORDER=('R','W','C','X','LE','H','Z','O','A','F')
def v(**kw):return [kw.get(k,0) for k in ORDER]
def plus(*values):return [sum(x) for x in zip(*values)] if values else [0]*10
def times(a,n):return [x*n for x in a]
H={}
def part(source,**kw):return dict(source_operation=source,vector=v(**kw))
def helper(name,source,parts,refs=None,bare=None,d=None):
    refs=refs or {}
    out=plus(*(p['vector'] for p in parts),*(times(H[k]['tuple'],n) for k,n in refs.items()))
    H[name]=dict(tuple=out,price=sum(out),source=source,ordered_parts=parts,
                 helper_coefficients=refs,bare_own_identifiers=bare or [],
                 bare_identifier_selector_X=0,d_vector=d or [0]*10,
                 scope='Complete proposed helper/source case, not an accepted tariff or SDK CPU/native/resource proof.')

helper('I','formation-r8.h:723-725',[
 part('enum parameter destination,body read,explicit nonidentity enum cast,own return',R=1,W=1,X=1,H=1,Z=1)])
helper('D','formation-r8.h:727-731',[
 part('two incoming scalar destinations',W=2),
 part('b>max()-a: two body reads,subtraction,comparison,visible max H/Z',R=2,C=1,X=1,H=1,Z=1),
 part('return a+b and owned function entry/return',R=2,X=1,H=1,Z=1)])
helper('M','formation-r8.h:732-738',[
 part('two incoming scalar destinations',W=2),
 part('divisor=a|static_cast<U64>(a==0)',R=2,W=1,C=1,X=2),
 part('b>max()/divisor',R=2,C=1,X=1,H=1,Z=1),
 part('return a*b and owned entry/return',R=2,X=1,H=1,Z=1)])
helper('E','formation-r8.h:740',[
 part('phase scalar parameter and direct two-field aggregate result,own H/Z',R=1,W=3,H=1,Z=1)])
helper('Bounds','formation-r8.h:561-565,726',[
 part('Ledger.bounds and Catalogue.reviewed visible owned calls,reference result only',H=2,Z=2)],
 bare=['reviewedData_ is a bare static data identifier. No written& or selector; no referred443-cell copy.'])

# cost(site) is a derivation part, not an added catalogue helper or runtime fee.
# It includes its actual site destination/caller read,ix transfer/body and Bounds.
cost_part=part('cost(site): two site reads,returned .sites[] scalar read plus caller site read; two parameter destinations;.sites/subscript/enumcast;cost/ix/Bounds HZ',R=4,W=2,X=3,H=4,Z=4)
helper('P','formation-r8.h:739,741-753',[
 cost_part,part('amount local first destination',W=1),
 part('fund.phase->ix->p: actual field/body reads,two destinations,field/cast',R=2,W=2,X=2,H=1,Z=1),
 part('three complete admitted guards,including max',R=8,C=3,X=10,H=1,Z=1),
 part('remaining/reserved/actual updates: actual target/index/amount reads,destinations and written operators',R=8,W=3,X=10),
 part('raw.sites[ix(site)]+=1,including ix body/parameter',R=3,W=2,X=5,H=1,Z=1),
 part('pay Site parameter and own entry/return',W=1,H=1,Z=1)],
 bare=['No implicit selector in Bounds.reviewedData_;P has explicit fund/s field paths.'])
helper('R','formation-r8.h:903-911',[
 cost_part,part('amount local and incoming Site destination,own entry/return',W=2,H=1,Z=1),
 part('trace.handedOff|(amount>trace.unused)',R=3,C=2,X=3),
 part('trace.actual=add(trace.actual,amount),all caller/body/destination operations',R=6,W=3,C=1,X=4,H=2,Z=2),
 part('trace.unused-=amount',R=2,W=1,X=2),
 part('trace.raw.sites[ix(site)]+=1,including ix body/parameter',R=3,W=2,X=5,H=1,Z=1)])
helper('S','formation-r8.h:754-762',[
 part('phase/amount incoming destinations and own entry/return',W=2,H=1,Z=1),
 part('two admitted guards,including actual phase/ix transfer',R=6,W=1,C=2,X=7,H=1,Z=1),
 part('from.remaining-=amount',R=2,W=1,X=2),
 part('reserved[from.phase]-=amount with own ix',R=4,W=2,X=6,H=1,Z=1),
 part('reserved[phase] destination ix and explicit designators',R=2,W=2,X=4,H=1,Z=1),
 part('reserved[phase] RHS and checked add',R=8,W=3,C=1,X=6,H=3,Z=3),
 part('into={amount,phase}:actual two temporary destinations and memberwise assignment',R=4,W=4,H=1,Z=1)])
helper('T','formation-r8.h:763-767',[
 part('terminal phase/index destination subtree',R=2,W=2,X=5,H=1,Z=1),
 part('terminal RHS,remaining and checked add subtree',R=8,W=3,C=1,X=8,H=3,Z=3),
 part('fund.remaining=0 and own entry/return',W=1,X=1,H=1,Z=1)])
helper('Q','formation-r8.h:1601-1602',[
 part('index parameter/body read,launder pointer parameter,subscript/.bytes/cast/deref,own and SDK HZ',R=1,W=2,X=4,H=2,Z=2)],
 bare=['slots_ unqualified base is not a selector.'])
helper('V','formation-r8.h:922-933',[
 dict(source_operation='sixteen reached id.bytes SDK-array rows,mask,three diagnostics,index increment',vector=times(v(R=7,W=6,X=10,H=1,Z=1),16)),
 part('17 loop tests,initial bits/i/entry,post-loop diagnostics and bits predicate,own HZ',R=21,W=5,C=18,X=6,LE=1,H=1,Z=1)])
helper('J','formation-r8.h:934-945',[
 dict(source_operation='sixteen complete paired SDK-array rows,compare/eager mask,diagnostics/increment',vector=times(v(R=9,W=7,C=1,X=12,H=2,Z=2),16)),
 part('17 loop tests,initial equal/i/entry,post-loop diagnostics and named return,own HZ',R=21,W=5,C=17,X=6,LE=1,H=1,Z=1)])
helper('C','formation-r8.h:1303,1348-1352',[
 part('formed.receipt553-cell member copy and its written RHS selector',R=553,W=553,X=1,H=1,Z=1),
 part('formed.failureTail538-cell member copy and written RHS selector',R=538,W=538,X=1,H=1,Z=1),
 part('formed.state pointer read/destination and RHS selector',R=1,W=1,X=1),
 part('three flag initializations',W=3),
 part('Exchange default and actual atomic Signal destination/parameter',W=2,H=2,Z=2),
 part('formed.state=null;formed.failureTail.handedOff=true',W=2,X=3),
 part('owned Control constructor entry/return',H=1,Z=1)],
 bare=['state_,receipt_,failureTail_,armed_,terminalAcquired_,resultLive_ initializer labels:0X',
       'Exchange.signal DMI is real initialization,not an implicit member selector.'])
helper('U','formation-r8.h:574-575',[
 part('receipt553-cell copy and selected copy HZ',R=553,W=553,H=1,Z=1),
 part('disposition incoming scalar destination/body read/member destination',R=1,W=2),
 part('owned Readout constructor entry/return',H=1,Z=1)],
 bare=['receipt_,disposition_ are initializer labels,not written selectors.'])
helper('L','Actual typed role-life pay caller',[
 part('written s.roleLife_ plus builtin subscript',X=2)],refs={'P':1,'I':1})
helper('Ophys','formation-r8.h:1355-1362,actual outside .ownerEvent caller',[
 part('two Site destinations,own HZ,state guard,actual pointer/physical reads and written caller/deref/arrow/subscript',R=4,W=2,C=1,X=4,H=1,Z=1)],refs={'P':1,'I':1},
 bare=['Three state_ bare bases in guard/pay arguments add no selector.'])
helper('Ofail','formation-r8.h:1355-1365,actual outside .ownerEvent caller',[
 part('two Site destinations,own HZ,state/failed reads,guard,and outside written selector',R=2,W=2,C=1,X=1,H=1,Z=1)],refs={'R':1},
 bare=['state_ and failureTail_ are bare bases,not selectors.'])
helper('Pdirect','Actual direct non-role typed slice caller',[
 part('actual s.slice selector',X=1)],refs={'P':1})
helper('Fixed','legacy-adapter.h:26-27/window-query.h:227-228',[
 part('wrapper Site destination/body read,actual typed slice selector,own HZ',R=1,W=1,X=1,H=1,Z=1)],refs={'P':1})
helper('NativePay','window-query.h:303-304',[
 part('NativeSession static wrapper Site destination/body read,own HZ;no unqualified method selector',R=1,W=1,H=1,Z=1)],refs={'Fixed':1})
helper('Access','worker-result.h:24;formation-r8.h:696-700,1323,1347',[
 part('launder pointer destination,actual cast/deref,owned and SDK HZ',W=1,X=2,H=2,Z=2)],
 bare=['bytes_,parentBytes_,metadataBytes_,legacyOptionsBytes_,planBytes_,resultBytes_ bare array bases:0X.'])
helper('Minimum','legacy-adapter.h:25',[
 part('two parameter destinations,two comparison reads,selected return read,own HZ',R=3,W=2,C=1,H=1,Z=1)])
helper('RestorePrefix','window-query.h:611-616',[
 part('state_ pointer body read,*state_,check returned guard,actual restore entry/return',R=1,C=1,X=1,H=1,Z=1)],
 bare=['state_ is unqualified;each explicit/destructor restore call sponsors this prefix even on check refusal.'])
helper('NativeControlLeaf','native-observer.cpp:30-38',[
 part('NativeControl whole zero3,explicit stores3,named return3/3;three dots,cast,and,ordinary complement;SDK/own/copy HZ',R=3,W=9,X=6,H=3,Z=3)])
helper('NativeModuleLeaf','native-observer.cpp:40-73 both exact anchor leaves',[
 part('PODzero3,entry/module locals,three API params/ok local,three result stores,named return;written flags OR retained',R=7,W=15,C=1,X=9,H=3,Z=3)])
helper('NativeImageLeaf','native-observer.cpp:75-90',[
 part('incoming module,POD3/nativeInfo3 zeroing,process/API/ok locals/params,result stores,named return;ordinary nonidentity DWORD sizeof cast retained',R=8,W=19,C=1,X=10,H=4,Z=4)])
helper('NativePageLeaf','native-observer.cpp:92-112',[
 part('incoming address,POD8/MBI8 zeroing,three API params/return,all result stores/named return;three size_t-to-U64 casts conditional d including expected sizeof',R=16,W=37,X=18,H=3,Z=3)],d=v(X=3))
helper('NativeReadLeaf','native-observer.cpp:117-148',[
 part('snapshot13zero,12direct stores,two locals,six API parameter cells,named13-cell return;written ordinary complement retained',R=17,W=46,X=21,H=10,Z=10)])
helper('NativeRestoreLeaf','native-observer.cpp:155-175',[
 part('four locals/four SDK scalar-pointer parameters/errno store;seven body values;all written status ops including ordinary complement',R=7,W=9,C=3,X=14,H=7,Z=7)])
helper('OldRow','window-query.h:217-219',[
 part('id parameter/body read,.rows/native subscript/enum cast,own HZ',R=1,W=1,X=3,H=1,Z=1)],
 bare=['data_ bare static base:0X;reference return does not copy10 cells.'])
helper('Math1','Actual selected bit_width/isfinite/floor/log1p leaf caller',[
 part('one actual scalar parameter destination and visible SDK HZ;caller read/expressions separate',W=1,H=1,Z=1)])
helper('CommitOld','window-query.h:283-291',[
 part('amount parameter',W=1),
 part('phase field/ix/local',R=2,W=2,X=3,H=1,Z=1),
 part('query remaining update',R=2,W=1,X=3),
 part('reserved and actual updates',R=6,W=2,X=8),
 part('owned entry/return',H=1,Z=1)])
helper('CommitPrepared','legacy-adapter.h:212-225',[
 part('phase ix/context local,owned entry/return',R=1,W=2,X=1,H=2,Z=2),
 part('prepared-index attempt remaining',R=3,W=1,X=6),
 part('reserved/actual',R=6,W=2,X=10),
 part('legacy contribution',R=2,W=1,X=3),
 part('raw actual attempt charge',R=3,W=1,X=7),
 part('raw actual invocation count',R=2,W=1,X=6)])
helper('AppendPrefix','window-query.h:765-773',[
 part('full incoming destination,selected and amount first destinations,full/selected caller reads',R=2,W=3),
 part('two written native.check selectors/two returned predicates,owned append HZ',C=2,X=2,H=1,Z=1)],refs={'OldRow':1})

# Exact adopted SDK logical cases are expanded once at their visible boundary.
# They do not recursively price SDK implementation fields or phantom selectors.
helper('BOOL_HANDLE','Actual shared-owner bool/null predicate',[
 part('one logical stored owner read,comparison,SDK HZ',R=1,C=1,H=1,Z=1)])
helper('ARROW_HANDLE','Actual written shared-owner arrow',[
 part('logical stored pointer read,written arrow,SDK HZ',R=1,X=1,H=1,Z=1)])
helper('HDEFAULT','Actual SDK empty or move construction case,explicit selected caller recorded per Site',[
 part('visible SDK construction HZ;neither empty nor move construction introduces O',H=1,Z=1)])
helper('HDROP','Actual SDK owner destruction,including empty/moved-from',[
 part('visible SDK destruction HZ and one adopted drop O',H=1,Z=1,O=1)])
helper('HMOVEASSIGN','Actual SDK owner move assignment',[
 part('visible assignment HZ and destination-drop O,even empty',H=1,Z=1,O=1)])
helper('WINDOW_CTOR','src/engine/MusicalAudioWindow.h plain default initialization',[
 part('five scalar DMIs and own/member constructor HZ',W=5,H=2,Z=2)])
helper('WINDOW_DTOR','src/engine/MusicalAudioWindow.h actual destruction',[
 part('own/member destructor HZ and one SDK drop O',H=2,Z=2,O=1)])
helper('MAX2','Actual selected numeric_limits/max_size visible call',[
 part('visible constant-valued SDK HZ,no literal R',H=1,Z=1)])
helper('EXT3','Actual SDK size/empty/begin/end logical extent/address',[
 part('one logical stored extent/address read and SDK HZ,caller selector outside',R=1,H=1,Z=1)])
helper('MoveRef','Actual selected std::move call',[
 part('explicit reference cast and visible SDK HZ,no referent copy',X=1,H=1,Z=1)])
helper('ReadoutPrebodyDMI','Actual default-init LegacyReadout before WorkerLegacyResult Cell/Take bodies',[
 part('four nested status DMIs and three actual implicit ctor HZ,before later body pay',W=4,H=3,Z=3)])
helper('GetterScalar','Actual selected engine scalar getter',[
 part('one scalar body read,owned getter HZ',R=1,H=1,Z=1)],bare=['Returned bare member is a real R,not selectorX.'])
helper('GetterReference','Actual selected engine reference getter',[
 part('owned getter HZ,no scalar referent read/copy',H=1,Z=1)],bare=['Returned bare member has no invented address/selectorX.'])
helper('GetterTwo','Actual selected engine two-cell lvalue value getter',[
 part('two source/destination cells,getter and selected copy HZ',R=2,W=2,H=2,Z=2)],bare=['Bare member lvalue is copied;no generated per-field designator.'])
helper('WindowValid','src/engine/MusicalAudioWindow.h:223',[
 part('bare clock owner bool read/predicate,getter and SDK comparison HZ',R=1,C=1,H=2,Z=2)],bare=['clock_ bare base is not X.'])
helper('OwnerBefore','Actual selected shared-owner ordering boundary',[
 part('two logical owner reads/comparison,one SDK HZ',R=2,C=1,H=1,Z=1)])
helper('ReturnSpanValue','src/engine/MusicalAudioWindow.h:162',[
 part('vector base/extent2R2W,getter and selected span range-construction HZ',R=2,W=2,H=2,Z=2)],bare=['rows_ is bare;no .rows traversal,vector copy,O,extra destination or prezero.'])
