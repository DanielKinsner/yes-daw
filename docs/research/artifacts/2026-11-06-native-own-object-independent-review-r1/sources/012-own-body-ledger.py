"""Working DATA authoring, not a target/helper program. Not a complete envelope.
This enumerates named by-value formals/lexical objects for later source review.
Full-expression temporaries, references, call contexts and SDK closure remain HOLD.
Do not use a sum over all frames as an observed/coexistent stack allocation.
"""
from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parent;R=P.parents[1]
S=R/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
V=R/'build-ci/20261106-native-object-layout-purpose-source-r1/execution-r1/layout-values.json'
raw=V.read_bytes()
if hashlib.sha256(raw).hexdigest().upper()!='03A663FFA001DEAE6C284517D71B54607081F6017C1AF40A98B30B40945E750D':raise ValueError('LAYOUT_VALUES_IDENTITY')
values={r['name']:r['value'] for r in json.loads(raw)['rows']}
CPP=(S/'clock_stage.cpp').read_bytes()
if hashlib.sha256(CPP).hexdigest().upper()!='F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206':raise ValueError('CPP_IDENTITY')
source_inputs=[]
for key,path,expected in [('CPP',S/'clock_stage.cpp','F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206'),('QC',S.parent/'waveform-definition-r3/independent_clock.h','A75FB5D17C9CACA2113DC8567A3E27F9A40FD9531BC59084E07F88576D15CB64'),('TIME',R/'src/engine/Time.h','FC5A0861B527FFFE6ED2E6F9FE571D3F8974329BBD065DACFBC38DFEA8C98590'),('BOOK',S/'source-costs.h','0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27')]:
    data=path.read_bytes();actual=hashlib.sha256(data).hexdigest().upper()
    if actual!=expected:raise ValueError(key+'_IDENTITY')
    source_inputs.append(dict(alias=key,path=path.relative_to(R).as_posix(),bytes=len(data),sha256=actual))

# P/L denote by-value parameter/lexical object. A joined source scope may use
# fewer than this union of its distinct mutually exclusive declarations.
frames=[]
def frame(name,lines,params=(),locals=(),refs=(),large=(),extra=(),source='CPP'):
    rows=[]
    for role,groups in [('by-value-parameter',params),('lexical-object',locals)]:
        for typ,names in groups:
            size=values.get(typ+'.sizeof')
            rows.append(dict(role=role,type=typ,names=names.split(),count=len(names.split()),qualified_unit_size=size,bytes=None if size is None else size*len(names.split())))
    frames.append(dict(name=name,source=source,source_lines=lines,objects=rows,named_qualified_object_union=sum(z['bytes'] for z in rows if z['bytes'] is not None),unqualified_named_objects=[z for z in rows if z['bytes'] is None],reference_bindings=list(refs),large_or_containing_owner_sponsors=list(large),other_required_context=list(extra),coverage='NAMED_OBJECT_COMPONENT_ONLY_NOT_COMPLETE_FRAME_OR_OTHER_ENVELOPE'))

frame('Rejection::Rejection',[35,35],[('pointer','r'),('Witness6','v')],large=['new Rejection exception object includes base,reason,values; do not recount these as locals'])
frame('Rejection::what',[36,36])
frame('rememberCloseFailure',[40,40],[('int','result error'),('pointer','site')])
frame('cleanupFileClose',[41,41],[('pointer','file site')],[('int','result error')])
frame('cleanupDescriptorClose',[42,42],[('int','descriptor')],[('int','result error')])
frame('reportCleanupClose',[43,43])
frame('gate_no_witness',[45,45],[('bool','ok'),('pointer','r')],extra=['Rejection ctor witness parameter directly initialized from {}; new exception object separate'])
frame('gate_witness',[46,46],[('bool','ok'),('pointer','r'),('Witness6','v')],extra=['Rejection ctor distinct copied witness parameter; member values already inside exception object'])
frame('bits',[47,47],[('double','x')])
frame('workLimit',[48,48],locals=[('u64','value')])
frame('reservationGate',[49,49],[('u64','total limit')],[('u64','reviewed')])
frame('bytesGate_unreached',[50,50],[('u64','total limit')],extra=['No current source caller; not counted as reached'])
frame('Cost::catalogue',[55,55])
frame('Cost::price',[56,56],[('Family','id')])
frame('Cost::validate',[57,62],locals=[('size_t','i')],refs=['book','whole'])
frame('Cost::plus',[63,63],[('u64','a b')])
frame('Cost::times',[64,64],[('u64','count value')])
frame('Cost::units',[65,65],[('Family','id'),('u64','count')])
frame('Cost::wordUnits',[66,74],[('bool','verify'),('u64','words')],[('u64','chunks fallback dispatch fast slow')],extra=['Nested plus expression can have an outer partially initialized parameter set while inner plus runs'])
frame('Cost::inverseUnits',[75,81],[('bool','verify'),('u64','records')],[('u64','api outer')])
frame('Cost::fixtureUnits',[82,93],[('bool','verify'),('u64','N g'),('int','map')],[('u64','queries zeros fractional zero cut fractionalQueries zeroQueries records'),('bool','ramp')])
frame('Cost::childUpper',[94,102],[('bool','verify')],[('u64','total R g'),('int','map'),('Rates3','range_backing'),('RateInitializerList','range_header'),('pointer','range_begin range_end')],refs=['range_reference'],extra=['Initializer-list header and backing are distinct; three iterations do not coexist'])
frame('Meter::Meter',[106,106],[('u64','p r')],large=['Meter instance24 includes used/reserved/prior'])
frame('Meter::add',[107,107],[('u64','n')])
frame('Text::add',[111,111],[('pointer','s')],large=['Text object and data member counted in named component'])
frame('Text::u',[112,112],[('u64','x')],[('Digits20','digits'),('unsigned','n')])
frame('Text::i',[113,113],[('Tick','x')])
frame('Text::boolean',[114,114],[('bool','x')])
frame('Text::quoted',[115,115],locals=[('char','c'),('Quoted2','q'),('StringConstIterator','range_begin range_end')],refs=['s','range_reference'])
frame('number',[117,117],[('u64','x')],large=['Text t in large component'],extra=['Returned String constructed directly in caller result; actual caller full-expression owner must be listed'])
frame('path',[118,118],refs=['root','relative'],extra=['Two String temporaries/result from concat chain; source-selected move/direct-result fusion still to close'])
frame('Stream::Stream',[122,126],[('bool','w'),('u64','m')],[('int','descriptor e'),('pointer','setvbuf_owner')],refs=['name'],large=['Stream instance owns entire buffer plus file/count/mode members'])
frame('Stream::~Stream',[127,127],locals=[('pointer','owner')])
frame('Stream::flush',[129,129],locals=[('size_t','written'),('int','error')])
frame('Stream::put',[130,130],[('pointer','p'),('size_t','n')],[('size_t','k')],extra=['std::min second argument is a materialized size_t temporary bound to const reference'])
frame('Stream::word_write',[131,134],[('u64','x')],[('Fallback8','a')])
frame('Stream::exact',[135,135],[('pointer','p'),('size_t','n')],[('size_t','k')],extra=['std::min second argument materialized size_t reference temporary'])
frame('Stream::word_read',[136,140],locals=[('u64','x')])
frame('Stream::finish',[141,141],[('u64','expected')],[('int','flush_result flush_error close_result close_error'),('pointer','owner')])
frame('writeText',[143,143],refs=['p','t'],large=['Stream f in named large component'])
frame('textWitness',[144,144],locals=[('u64','ha hb'),('size_t','first a_loop_i b_loop_i')],refs=['a','b'],extra=['Aggregate witness result uses real caller destination; do not add a generic second returned array'])
frame('textGate',[145,145],refs=['observed','expected'],extra=['textWitness returned array feeds gate by-value parameter; direct result destination fusion required'])
frame('readText',[146,146],refs=['p','expected'],large=['Text t and caller Text return destination may coexist without NRVO; Stream f counted in large component'])
frame('multiply',[148,148],[('u64','a b')],[('u64','high low')],extra=['Pair128 braced return directly initializes its actual caller result'])
frame('compareRational',[149,155],[('u64','encoding numerator denominator')],[('unsigned','exponent shift'),('Pair128','product target')])
frame('independentFraction',[156,164],[('u64','n d')],[('u64','low high middle upper'),('unsigned','iterations')],extra=['Pair forwarding constructor may bind scalar prvalues as real temporaries; branch integer-zero versus double return operands differ'])
frame('FractionCache::FractionCache',[167,170],[('u64','n'),('bool','independent')],[('u64','r'),('DoublePair','original')],large=['FractionCache instance includes735 rows and N/g/count'],extra=['conditional Pair result before assignment into row; ctor/ref temporaries still need exclusive caller ledger'])
frame('CachedPort::CachedPort',[184,184],[('double','r'),('int','map')],refs=['m'],large=['CachedPort object160 embeds changes,NumericAClock,rate,cursor/cache/meter/misses members'],extra=['NumericAClock changes by-value vector parameter is distinct from CachedPort changes'])
frame('CachedPort::frame',[185,201],[('qcNative','t')],[('size_t','i'),('Tick','length local'),('double','extra relative result')],extra=['k/bpm/slope/wholeFrame are embedded persistent fields, not independent locals'])
frame('productionRows',[204,208],[('int','map')],[('Constants5','constants'),('bool','down'),('double','first last'),('ProductionRows4','rows')],refs=['count'],extra=['Optional named-return copy means rows plus actual caller result; two assigned TempoChange braced values are distinct full-expression temporaries'])
frame('buildProduction',[209,209],[('u64','R'),('int','map')],[('size_t','count'),('ProductionRows4','rows'),('Production','out')],extra=['Production caller result can coexist with out without NRVO; Production::build temporary reset64,TempoMapView formal layout not measured,SampleRate formal8 are separately required'])
frame('productionFrame',[210,210],[('qcNative','t')],[('double','out'),('TimeLookupStatus','status')],refs=['c'],extra=['MusicalTime temporary directly initializes Time checked-forward by-value parameter'])
frame('highEndpoint',[211,211],[('Tick','tick'),('double','fraction')],extra=['Native braced result is directly initialized in actual caller destination'])
frame('endpointGate',[212,212],[('qcNative','observed expected')])
frame('admittedCut',[213,213],[('double','low high')],[('bool','ok')],extra=['Cut braced result directly initializes caller result; failure Rejection is distinct'])
frame('cut_instantiations',[214,218],[('u64','i')],[('u64','product whole remainder'),('DoublePair','pair'),('Tick','tick'),('qcNative','upper'),('double','low high')],refs=['clock','cache'],extra=['Endpoint expected Native and frame input Native directly initialize their called by-value parameters; exact get/cut/prvalue return fusion still to close'])
frame('wordGate',[219,219],[('Tick','observed expected'),('u64','i R'),('int','map')])
frame('inverseGate',[220,220],[('qcNative','observed'),('MusicalTime','expected'),('u64','j')])
frame('Picks::Picks',[223,223],[('u64','R N W')],[('unsigned','k a z'),('u64','source'),('PicksItem','x')],large=['Picks248 already includes items[10],values[10],next'],extra=['Per-slot Item braced assignment source temporaries need source-site max, not ten retained copies'])
frame('Picks::see',[224,224],[('u64','i'),('Tick','q')])
frame('field',[227,227],[('pointer','name'),('u64','value')],refs=['t'])
frame('metadata',[228,237],[('bool','inverse')],[('size_t','count i'),('ProductionRows4','rows'),('Tick','lo hi start'),('bool','first'),('unsigned','windows k')],refs=['b','picks','manifest'],large=['Text t plus caller result counted in named Text multiplicities'],extra=['productionRows named-return local/destination; scalar temporary arguments to min/max const refs'])
frame('expect_instantiations',[238,238],[('pointer','id reason'),('Witness6','witness')],[('bool','caught'),('u64','beforeCleanup')],refs=['caught Rejection e'],extra=['F action actual closure type/value-parameter storage unmeasured; caller prvalue fusion; nested control-rejection lifetime'])
frame('controls',[239,243],[('u64','totalWork')],[('u64','reviewed n d'),('DoublePair','independent original'),('qcNative','normalized'),('Production','production')],extra=['Three distinct action closure types; buildProduction no-NRVO context; expected throw/witness nesting'])
frame('run',[244,270],[('bool','verify'),('u64','totalWork')],[('u64','entries inverseRecords R N encoded'),('int','map'),('bool','wordControl inverseControl prefixControl rampControl metadataControl'),('Rates3','range_backing'),('RateInitializerList','range_header'),('pointer','range_begin range_end'),('CachedPort','generator'),('Production','production prefix_mutant ramp_mutant'),('qcCut','a z'),('Bounds','b prefix_changed'),('Picks','picks'),('String','stem binary json'),('Tick','prior expected actual ramp_changed j'),('u64','i'),('Owned9','owned'),('size_t','prefix_count ramp_count'),('ProductionRows4','prefix_rows ramp_rows'),('double','prefix_changedA prefix_changedB frame'),('qcNative','inverse_actual corrupt'),('MusicalTime','inverse_expected')],refs=['root','manifest','meter','range_reference'],large=['fractions/q/text/read_actual/prefix_actual/descriptor/inv/returned Text temporaries have separate large component'],extra=['get closure and11 expect action types; precise mutually exclusive scope max; additional class full-expression temporaries; active child helper chains'])
frame('decimal',[271,271],[('pointer','p')],[('u64','value'),('unsigned','n digit')])
frame('boundedArgument',[272,276],[('pointer','p reason'),('size_t','maximum')],[('size_t','n')])
frame('main',[278,289],[('int','argc'),('pointer','argv')],[('size_t','manifestLength'),('int','i'),('bool','verify'),('String','root manifest'),('char','c'),('StringConstIterator','manifest_begin manifest_end'),('u64','prior runner total producerUpper verifierUpper composed'),('Meter','meter'),('pointer','what')],refs=['whole','manifest_range','caught Rejection e','caught std::exception e'])

# Actual selected qc/engine helper definitions. Reachability is explicit: the
# unused qc cut / engine tickToFrame/tickForFrame/meter methods are not reached.
frame('qc::require',[24,26],[('bool','value'),('pointer','message')],extra=['Source-requested runtime_error object/message clone on failure'],source='QC')
frame('qc::fixtureMap',[27,41],[('int','kind')],[('Constants5','constants'),('bool','down'),('double','first last')],extra=['Initializer-list backing3/4 Changes and header; vector return prvalue directly initializes caller member; default allocator temporary and selected SDK chain'],source='QC')
frame('qc::scaled',[43,52],[('double','a b c')],[('int','ap bp cp'),('double','am bm cm')],refs=['out'],source='QC')
frame('qc::checkFrame',[53,56],[('double','f')],source='QC')
frame('qc::fractionBounds',[59,77],[('u64','numerator denominator')],[('u64','rem mantissa'),('int','exponent i'),('double','low high')],extra=['nextBit closure unmeasured; returned Pair/zero-branch forwarding-reference scalar temporaries'],source='QC')
frame('qc::NumericAClock::append',[83,99],[('size_t','i'),('Tick','segment delta')],[('double','start end d k slope relative')],refs=['frame'],source='QC')
frame('qc::NumericAClock::segmentFor',[100,105],[('Tick','whole')],[('ChangeConstIterator','upper')],extra=['Comparator closure; begin/end/returned iterator materialization and actual upper_bound forwarding'],source='QC')
frame('qc::NumericAClock::NumericAClock',[107,123],[('double','rate'),('ChangeVector','changes')],[('size_t','i'),('double','f'),('Tick','len')],large=['NumericAClock receiver56 is embedded in CachedPort, not a new duplicate'],extra=['Prefix assign0.0 const-ref temporary; actual selected copy/default/move/fill/allocator guards'],source='QC')
frame('qc::NumericAClock::frame',[124,155],[('qcNative','t')],[('size_t','i'),('double','result k bpm slope extra relative'),('Tick','end length local')],source='QC')
frame('qc::NumericAClock::inverse',[166,194],[('double','frameIn')],[('double','k local bpm delta slope exponent integral'),('DoubleConstIterator','upper'),('size_t','i'),('Tick','origin whole'),('qcNative','out')],extra=['Named Native out plus actual returned assignment-source destination without NRVO; begin/end/returned iterators and default comparator'],source='QC')
frame('engine::detail::isFinitePositive',[21,24],[('double','value')],source='TIME')
frame('engine::MusicalTime::hasValidFraction',[52,55],source='TIME')
frame('engine::TempoChange::hasValidBpm',[310,313],source='TIME')
frame('engine::SampleRate::isValid',[336,339],source='TIME')
frame('engine::TempoMapView::empty',[355,355],source='TIME')
frame('engine::detail::appendTempoSegmentFrames',[381,414],[('double','sampleRate startBpm endBpm'),('Tick','segmentTicks deltaTicks'),('TempoCurve','curve')],[('double','delta k slope relativeChange')],refs=['frame'],source='TIME')
frame('engine::detail::scaledTimeRatio',[489,502],[('double','a b c')],[('int','aPower bPower cPower'),('double','aMantissa bMantissa cMantissa')],refs=['out'],source='TIME')
frame('engine::detail::checkedNativeFrame',[504,512],[('double','frame')],refs=['out'],source='TIME')
frame('engine::CompiledTempoMap::build',[583,633],[('TempoMapView_UNMEASURED','tempoMap'),('SampleRate','sampleRate')],[('size_t','validation_i prefix_i'),('double','frame'),('Tick','segmentTicks')],refs=['out'],extra=['Explicit CompiledTempoMap{} reset temporary64; literal0.0 fill-reference temporary; selected vector assign/reset objects'],source='TIME')
frame('engine::CompiledTempoMap::frameForTick',[635,675],[('Tick','tick')],[('size_t','upper i'),('double','frame endBpm'),('bool','haveNext'),('Tick','nextTick')],refs=['frameOut'],extra=['Source comparator closure and begin/end/distance result iterator lifetimes'],source='TIME')
frame('engine::CompiledTempoMap::frameForMusicalTime',[728,789],[('MusicalTime','time')],[('double','frame acceptedWhole k bpm slope extra relative'),('TimeLookupStatus','wholeStatus'),('TempoConstIterator','upper'),('size_t','i'),('Tick','length local')],refs=['frameOut'],extra=['Source comparator closure; caller iterators and forwarding objects'],source='TIME')
frame('engine::CompiledTempoMap::musicalTimeForFrame',[791,850],[('double','frame')],[('double','k local bpm delta slope exponent integral'),('Tick','origin whole'),('size_t','i'),('DoubleConstIterator','upper'),('bool','ramp'),('MusicalTime','result')],refs=['timeOut'],extra=['MusicalTime{} assignment-source temporary16; default comparator/iterator/context temporaries'],source='TIME')
frame('Catalogue::family',[60,66],[('Family','id')],[('size_t','index')],refs=['bound'],extra=['Source-requested logic_error object/message copy on failure'],source='BOOK')
frame('Catalogue::whole',[59,59],source='BOOK')
frame('immutableCatalogue',[68,68],source='BOOK')

proposal=dict(schema='WORKING_NATIVE_OWN_NAMED_OBJECT_LEDGER_R1',state='INCOMPLETE_NOT_FIRST_COMPLETE_WHOLE_CHECK',frames=frames,frame_count=len(frames),large_component=dict(Stream=values['Stream.sizeof'],Text=values['Text.sizeof'],FractionCache=values['FractionCache.sizeof'],source_counts=[2,4,1],bytes=2*values['Stream.sizeof']+4*values['Text.sizeof']+values['FractionCache.sizeof']),OTHER_upper=None,full_auto_upper=None,root32MiB_fit=None,scope='Named own CPP/qc/Time/BOOK declaration components only; SDK frames, full-expression temporaries, closures/ref representations and actual nested multiplicities still incomplete. Source spans and every declaration require fresh whole check. Per-frame union is not physical frame layout or complete coexistence proof.',source_inputs=source_inputs,layout_values_sha256=hashlib.sha256(raw).hexdigest().upper(),no_compiler_decoder_launcher_or_target_execution=True)
(P/'own-named-objects.json').write_text(json.dumps(proposal,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='WORKING_NAMED_OBJECT_COMPONENT_WRITTEN_INCOMPLETE',frames=len(frames),large_component=proposal['large_component'],OTHER_upper=None)))
