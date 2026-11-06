"""Working file/data-only closure inventory. No lambda is compiled or invoked.
All sizes remain None: pointer width does not invent a closure sizeof fact.
"""
from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parent;R=P.parents[1]
base=R/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
paths={'CPP':base/'clock-oracle-r13-streamed-dependency-source-r1/clock_stage.cpp','QC':base/'waveform-definition-r3/independent_clock.h','TIME':R/'src/engine/Time.h'}
sources={k:p.read_text(encoding='utf-8').splitlines() for k,p in paths.items()}
rows=[]
def row(key,source,line,captures,role,occurrence=0):
    text=sources[source][line-1]
    rows.append(dict(id=key,source=source,path=paths[source].relative_to(R).as_posix(),line=line,occurrence_on_line=occurrence,source_line=text,captures=[dict(name=n,exact_source_type=t,mode='reference') for n,t in captures],storage_role=role,sizeof=None,alignof=None,requires='Actual selected closure-type layout and source-context correspondence, or independently accepted exact source/ABI representation proof; no capture_count*8 inference'))
row('work_action','CPP',240,[('totalWork','std::uint64_t')],'expect F action parameter; direct caller prvalue destination')
row('carry_action','CPP',241,[('independent','const std::pair<double,double>'),('normalized','const qc::Native')],'expect F action parameter')
row('invalid_native_action','CPP',242,[('production','const Production'),('independent','const std::pair<double,double>')],'expect F action parameter')
row('get','CPP',251,[('verify','bool'),('production','Production'),('fractions','const FractionCache'),('generator','CachedPort')],'named const get closure, one object remains for complete fixture scope')
row('q_action','CPP',253,[('actual','const std::int64_t'),('expected','const std::int64_t'),('i','std::uint64_t'),('R','const std::uint64_t'),('map','int')],'expect F action parameter')
row('truncated_action','CPP',255,[('root','const std::string&')],'expect F action parameter')
row('trailing_action','CPP',256,[('root','const std::string&')],'expect F action parameter')
row('metadata_action','CPP',257,[('actual','Text'),('text','const Text')],'expect F action parameter')
row('prefix_action','CPP',258,[('actual','const Text'),('text','const Text')],'expect F action parameter')
row('ramp_action','CPP',259,[('changed','const std::int64_t'),('b','const Bounds'),('N','const std::uint64_t'),('R','const std::uint64_t'),('map','int')],'expect F action parameter')
row('inverse_whole_action','CPP',264,[('corrupt','qc::Native'),('expected','yesdaw::engine::MusicalTime'),('j','std::int64_t')],'expect F action parameter',0)
row('inverse_fraction_action','CPP',264,[('corrupt','qc::Native'),('expected','yesdaw::engine::MusicalTime'),('j','std::int64_t')],'distinct expect F action type/parameter',1)
row('nextBit','QC',64,[('rem','std::uint64_t'),('denominator','std::uint64_t')],'named const nextBit; direct calls, no generic function-wrapper storage')
row('qc_tick_comparator','QC',103,[],'classic upper_bound four-argument predicate parameter; no ranges/_Pass_fn wrapper')
row('time_tick_comparator','TIME',652,[],'classic upper_bound four-argument predicate parameter')
row('time_native_comparator','TIME',755,[],'classic upper_bound four-argument predicate parameter')
data=dict(schema='WORKING_NATIVE_CLOSURE_CONTEXT_LEDGER_R1',state='INCOMPLETE_NO_LAYOUT_CREDIT',closures=rows,source_counts=dict(CPP=12,QC=2,TIME=2),candidate_coexistence_contexts=[dict(phase='common controls before rate/cache/get construction',expression='one active work/carry/invalid-native expect action; invalid-native may enter Time comparator; no get object'),dict(phase='FractionCache or carry fractionBounds',expression='one nextBit closure; get has not been constructed; FractionCache calls are sequential'),dict(phase='normal fixture query or inverse',expression='one get object plus the single currently invoked qc/Time comparator or std::less<void> object'),dict(phase='fixture expected numeric/IO failure',expression='one get object plus one directly constructed expect action parameter; action-specific helper scopes still counted'),dict(phase='unwind/control wrong-rejection',expression='entered action remains during its own unwind; caught original exception may coexist with a new Rejection; closures have their actual source lifetimes, not arbitrary extra copies')],classic_upper_bound_source_fact='xutility7462-7487 directly calls _Pred; no _Pass_fn in that classic body. Default three-argument overload delegates with std::less<>{}. Exact iterator/body/context inventory remains unfinished.',not_reached_closures=[dict(path='src/engine/Time.h',line=685,reason='acceptDelta belongs to tickForFrame, not called by native child checked inverse'),dict(path='src/engine/Time.h',lines=[949,1020,1123,1146,1152,1170],reason='meter-map APIs not called by this child')],not_a_complete_OTHER_or_fit=True,no_compile_or_native_run=True)
(P/'closure-contexts.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='WORKING_CLOSURE_DATA_ONLY_UNQUALIFIED',closures=len(rows),all_layout_sizes_unbound=all(r['sizeof'] is None for r in rows))))
