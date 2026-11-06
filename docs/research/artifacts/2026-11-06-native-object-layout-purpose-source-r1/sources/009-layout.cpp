// UNRUN compile-only purpose. No main, Data, constructor call or runtime entry.
#include "../musical-audio-persistent-planner-certification/source-revisions/waveform-definition-r3/independent_clock.h"
#include "../../src/engine/Time.h"
#include "../musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1/source-costs.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <intrin.h>
#include <io.h>
#include <share.h>
#include <string>
#include <sys/stat.h>
#include <type_traits>
#include <xmmintrin.h>
#pragma fenv_access(on)
#pragma fp_contract(off)
#include "layout-types.h"
#include <cstddef>
#if !defined(_M_X64) || !defined(_DLL) || !defined(_MT)
#error Unsupported compiler ABI/runtime profile
#endif
#ifdef NDEBUG
#error Original Clock profile has no NDEBUG definition
#endif
static_assert(_MSC_VER==1944 && _ITERATOR_DEBUG_LEVEL==0 && _CRT_PACKING==8);
static_assert(sizeof(void*)==8 && sizeof(std::size_t)==8 && sizeof(double)==8 && sizeof(std::uint64_t)==8 && sizeof(int)==4 && sizeof(bool)==1);
static_assert(std::endian::native==std::endian::little);
static_assert(sizeof(layout_purpose::Text)<=65552 && sizeof(layout_purpose::Stream)<=65664);
static_assert(sizeof(qc::Change)<=32 && sizeof(yesdaw::engine::TempoChange)<=32);
static_assert(std::is_standard_layout_v<layout_purpose::CleanupCloseState>);
static_assert(std::is_standard_layout_v<layout_purpose::Meter>);
static_assert(std::is_standard_layout_v<layout_purpose::Text>);
static_assert(std::is_standard_layout_v<layout_purpose::Stream>);
static_assert(std::is_standard_layout_v<layout_purpose::Pair128>);
static_assert(std::is_standard_layout_v<layout_purpose::FractionCache>);
static_assert(std::is_standard_layout_v<layout_purpose::Picks::Item>);
static_assert(std::is_standard_layout_v<layout_purpose::Picks>);
static_assert(std::is_standard_layout_v<layout_purpose::Bounds>);
static_assert(std::is_standard_layout_v<qc::Change>);
static_assert(std::is_standard_layout_v<qc::Native>);
static_assert(std::is_standard_layout_v<qc::Cut>);
static_assert(std::is_standard_layout_v<yesdaw::engine::TempoChange>);
static_assert(std::is_standard_layout_v<yesdaw::engine::MusicalTime>);
static_assert(std::is_standard_layout_v<yesdaw::engine::SampleRate>);
static_assert(std::is_standard_layout_v<clock_stage::source_costs::FamilyBound>);
static_assert(std::is_standard_layout_v<clock_stage::source_costs::WholeBound>);
#pragma section(".ylayout",read)
extern "C" {
__declspec(allocate(".ylayout")) extern constinit const std::uint64_t yesdaw_layout_cells[] = {
 0x59444C4159303031ull,1ull,194ull,
 static_cast<std::uint64_t>(sizeof(void*)), // pointer.sizeof
 static_cast<std::uint64_t>(alignof(void*)), // pointer.alignof
 static_cast<std::uint64_t>(sizeof(std::size_t)), // size_t.sizeof
 static_cast<std::uint64_t>(alignof(std::size_t)), // size_t.alignof
 static_cast<std::uint64_t>(sizeof(bool)), // bool.sizeof
 static_cast<std::uint64_t>(alignof(bool)), // bool.alignof
 static_cast<std::uint64_t>(sizeof(char)), // char.sizeof
 static_cast<std::uint64_t>(alignof(char)), // char.alignof
 static_cast<std::uint64_t>(sizeof(unsigned char)), // unsigned_char.sizeof
 static_cast<std::uint64_t>(alignof(unsigned char)), // unsigned_char.alignof
 static_cast<std::uint64_t>(sizeof(int)), // int.sizeof
 static_cast<std::uint64_t>(alignof(int)), // int.alignof
 static_cast<std::uint64_t>(sizeof(unsigned)), // unsigned.sizeof
 static_cast<std::uint64_t>(alignof(unsigned)), // unsigned.alignof
 static_cast<std::uint64_t>(sizeof(std::uint64_t)), // u64.sizeof
 static_cast<std::uint64_t>(alignof(std::uint64_t)), // u64.alignof
 static_cast<std::uint64_t>(sizeof(qc::Tick)), // Tick.sizeof
 static_cast<std::uint64_t>(alignof(qc::Tick)), // Tick.alignof
 static_cast<std::uint64_t>(sizeof(double)), // double.sizeof
 static_cast<std::uint64_t>(alignof(double)), // double.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Rejection)), // Rejection.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Rejection)), // Rejection.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::CleanupCloseState)), // CleanupCloseState.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::CleanupCloseState)), // CleanupCloseState.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Cost)), // Cost.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Cost)), // Cost.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Meter)), // Meter.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Meter)), // Meter.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Text)), // Text.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Text)), // Text.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Stream)), // Stream.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Stream)), // Stream.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Pair128)), // Pair128.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Pair128)), // Pair128.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::FractionCache)), // FractionCache.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::FractionCache)), // FractionCache.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::CachedPort)), // CachedPort.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::CachedPort)), // CachedPort.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Picks)), // Picks.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Picks)), // Picks.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Bounds)), // Bounds.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Bounds)), // Bounds.alignof
 static_cast<std::uint64_t>(sizeof(layout_purpose::Picks::Item)), // PicksItem.sizeof
 static_cast<std::uint64_t>(alignof(layout_purpose::Picks::Item)), // PicksItem.alignof
 static_cast<std::uint64_t>(sizeof(qc::Change)), // qcChange.sizeof
 static_cast<std::uint64_t>(alignof(qc::Change)), // qcChange.alignof
 static_cast<std::uint64_t>(sizeof(qc::Native)), // qcNative.sizeof
 static_cast<std::uint64_t>(alignof(qc::Native)), // qcNative.alignof
 static_cast<std::uint64_t>(sizeof(qc::Cut)), // qcCut.sizeof
 static_cast<std::uint64_t>(alignof(qc::Cut)), // qcCut.alignof
 static_cast<std::uint64_t>(sizeof(qc::NumericAClock)), // NumericAClock.sizeof
 static_cast<std::uint64_t>(alignof(qc::NumericAClock)), // NumericAClock.alignof
 static_cast<std::uint64_t>(sizeof(yesdaw::engine::TempoChange)), // TempoChange.sizeof
 static_cast<std::uint64_t>(alignof(yesdaw::engine::TempoChange)), // TempoChange.alignof
 static_cast<std::uint64_t>(sizeof(yesdaw::engine::MusicalTime)), // MusicalTime.sizeof
 static_cast<std::uint64_t>(alignof(yesdaw::engine::MusicalTime)), // MusicalTime.alignof
 static_cast<std::uint64_t>(sizeof(yesdaw::engine::SampleRate)), // SampleRate.sizeof
 static_cast<std::uint64_t>(alignof(yesdaw::engine::SampleRate)), // SampleRate.alignof
 static_cast<std::uint64_t>(sizeof(yesdaw::engine::CompiledTempoMap)), // Production.sizeof
 static_cast<std::uint64_t>(alignof(yesdaw::engine::CompiledTempoMap)), // Production.alignof
 static_cast<std::uint64_t>(sizeof(clock_stage::source_costs::Family)), // Family.sizeof
 static_cast<std::uint64_t>(alignof(clock_stage::source_costs::Family)), // Family.alignof
 static_cast<std::uint64_t>(sizeof(yesdaw::engine::TempoCurve)), // TempoCurve.sizeof
 static_cast<std::uint64_t>(alignof(yesdaw::engine::TempoCurve)), // TempoCurve.alignof
 static_cast<std::uint64_t>(sizeof(yesdaw::engine::TimeLookupStatus)), // TimeLookupStatus.sizeof
 static_cast<std::uint64_t>(alignof(yesdaw::engine::TimeLookupStatus)), // TimeLookupStatus.alignof
 static_cast<std::uint64_t>(sizeof(clock_stage::source_costs::FamilyBound)), // FamilyBound.sizeof
 static_cast<std::uint64_t>(alignof(clock_stage::source_costs::FamilyBound)), // FamilyBound.alignof
 static_cast<std::uint64_t>(sizeof(clock_stage::source_costs::WholeBound)), // WholeBound.sizeof
 static_cast<std::uint64_t>(alignof(clock_stage::source_costs::WholeBound)), // WholeBound.alignof
 static_cast<std::uint64_t>(sizeof(clock_stage::source_costs::Catalogue)), // Catalogue.sizeof
 static_cast<std::uint64_t>(alignof(clock_stage::source_costs::Catalogue)), // Catalogue.alignof
 static_cast<std::uint64_t>(sizeof(std::array<clock_stage::source_costs::FamilyBound,clock_stage::source_costs::familyCount>)), // FamilyRows.sizeof
 static_cast<std::uint64_t>(alignof(std::array<clock_stage::source_costs::FamilyBound,clock_stage::source_costs::familyCount>)), // FamilyRows.alignof
 static_cast<std::uint64_t>(sizeof(std::pair<double,double>)), // DoublePair.sizeof
 static_cast<std::uint64_t>(alignof(std::pair<double,double>)), // DoublePair.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<qc::Change>)), // ChangeVector.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<qc::Change>)), // ChangeVector.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<yesdaw::engine::TempoChange>)), // TempoVector.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<yesdaw::engine::TempoChange>)), // TempoVector.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<double>)), // DoubleVector.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<double>)), // DoubleVector.alignof
 static_cast<std::uint64_t>(sizeof(std::string)), // String.sizeof
 static_cast<std::uint64_t>(alignof(std::string)), // String.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<qc::Change>::iterator)), // ChangeIterator.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<qc::Change>::iterator)), // ChangeIterator.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<qc::Change>::const_iterator)), // ChangeConstIterator.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<qc::Change>::const_iterator)), // ChangeConstIterator.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<yesdaw::engine::TempoChange>::iterator)), // TempoIterator.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<yesdaw::engine::TempoChange>::iterator)), // TempoIterator.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<yesdaw::engine::TempoChange>::const_iterator)), // TempoConstIterator.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<yesdaw::engine::TempoChange>::const_iterator)), // TempoConstIterator.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<double>::iterator)), // DoubleIterator.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<double>::iterator)), // DoubleIterator.alignof
 static_cast<std::uint64_t>(sizeof(std::vector<double>::const_iterator)), // DoubleConstIterator.sizeof
 static_cast<std::uint64_t>(alignof(std::vector<double>::const_iterator)), // DoubleConstIterator.alignof
 static_cast<std::uint64_t>(sizeof(std::string::iterator)), // StringIterator.sizeof
 static_cast<std::uint64_t>(alignof(std::string::iterator)), // StringIterator.alignof
 static_cast<std::uint64_t>(sizeof(std::string::const_iterator)), // StringConstIterator.sizeof
 static_cast<std::uint64_t>(alignof(std::string::const_iterator)), // StringConstIterator.alignof
 static_cast<std::uint64_t>(sizeof(std::initializer_list<qc::Change>)), // ChangeInitializerList.sizeof
 static_cast<std::uint64_t>(alignof(std::initializer_list<qc::Change>)), // ChangeInitializerList.alignof
 static_cast<std::uint64_t>(sizeof(std::initializer_list<std::uint64_t>)), // RateInitializerList.sizeof
 static_cast<std::uint64_t>(alignof(std::initializer_list<std::uint64_t>)), // RateInitializerList.alignof
 static_cast<std::uint64_t>(sizeof(std::exception)), // Exception.sizeof
 static_cast<std::uint64_t>(alignof(std::exception)), // Exception.alignof
 static_cast<std::uint64_t>(sizeof(std::runtime_error)), // RuntimeError.sizeof
 static_cast<std::uint64_t>(alignof(std::runtime_error)), // RuntimeError.alignof
 static_cast<std::uint64_t>(sizeof(std::logic_error)), // LogicError.sizeof
 static_cast<std::uint64_t>(alignof(std::logic_error)), // LogicError.alignof
 static_cast<std::uint64_t>(sizeof(std::array<char,65536>)), // TextBuffer.sizeof
 static_cast<std::uint64_t>(alignof(std::array<char,65536>)), // TextBuffer.alignof
 static_cast<std::uint64_t>(sizeof(std::array<unsigned char,65536>)), // StreamBuffer.sizeof
 static_cast<std::uint64_t>(alignof(std::array<unsigned char,65536>)), // StreamBuffer.alignof
 static_cast<std::uint64_t>(sizeof(std::array<std::pair<double,double>,735>)), // FractionRows.sizeof
 static_cast<std::uint64_t>(alignof(std::array<std::pair<double,double>,735>)), // FractionRows.alignof
 static_cast<std::uint64_t>(sizeof(std::array<std::uint64_t,6>)), // Witness6.sizeof
 static_cast<std::uint64_t>(alignof(std::array<std::uint64_t,6>)), // Witness6.alignof
 static_cast<std::uint64_t>(sizeof(std::array<yesdaw::engine::TempoChange,4>)), // ProductionRows4.sizeof
 static_cast<std::uint64_t>(alignof(std::array<yesdaw::engine::TempoChange,4>)), // ProductionRows4.alignof
 static_cast<std::uint64_t>(sizeof(std::array<qc::Change,4>)), // FixtureRows4.sizeof
 static_cast<std::uint64_t>(alignof(std::array<qc::Change,4>)), // FixtureRows4.alignof
 static_cast<std::uint64_t>(sizeof(char[20])), // Digits20.sizeof
 static_cast<std::uint64_t>(alignof(char[20])), // Digits20.alignof
 static_cast<std::uint64_t>(sizeof(char[2])), // Quoted2.sizeof
 static_cast<std::uint64_t>(alignof(char[2])), // Quoted2.alignof
 static_cast<std::uint64_t>(sizeof(unsigned char[8])), // Fallback8.sizeof
 static_cast<std::uint64_t>(alignof(unsigned char[8])), // Fallback8.alignof
 static_cast<std::uint64_t>(sizeof(unsigned char[9])), // Owned9.sizeof
 static_cast<std::uint64_t>(alignof(unsigned char[9])), // Owned9.alignof
 static_cast<std::uint64_t>(sizeof(double[5])), // Constants5.sizeof
 static_cast<std::uint64_t>(alignof(double[5])), // Constants5.alignof
 static_cast<std::uint64_t>(sizeof(std::uint64_t[3])), // Rates3.sizeof
 static_cast<std::uint64_t>(alignof(std::uint64_t[3])), // Rates3.alignof
 static_cast<std::uint64_t>(offsetof(layout_purpose::CleanupCloseState,failures)), // CleanupCloseState.offsetof.failures
 static_cast<std::uint64_t>(offsetof(layout_purpose::CleanupCloseState,firstReturn)), // CleanupCloseState.offsetof.firstReturn
 static_cast<std::uint64_t>(offsetof(layout_purpose::CleanupCloseState,firstErrno)), // CleanupCloseState.offsetof.firstErrno
 static_cast<std::uint64_t>(offsetof(layout_purpose::CleanupCloseState,firstSite)), // CleanupCloseState.offsetof.firstSite
 static_cast<std::uint64_t>(offsetof(layout_purpose::Meter,used)), // Meter.offsetof.used
 static_cast<std::uint64_t>(offsetof(layout_purpose::Meter,reserved)), // Meter.offsetof.reserved
 static_cast<std::uint64_t>(offsetof(layout_purpose::Meter,prior)), // Meter.offsetof.prior
 static_cast<std::uint64_t>(offsetof(layout_purpose::Text,data)), // Text.offsetof.data
 static_cast<std::uint64_t>(offsetof(layout_purpose::Text,size)), // Text.offsetof.size
 static_cast<std::uint64_t>(offsetof(layout_purpose::Stream,file)), // Stream.offsetof.file
 static_cast<std::uint64_t>(offsetof(layout_purpose::Stream,buffer)), // Stream.offsetof.buffer
 static_cast<std::uint64_t>(offsetof(layout_purpose::Stream,at)), // Stream.offsetof.at
 static_cast<std::uint64_t>(offsetof(layout_purpose::Stream,size)), // Stream.offsetof.size
 static_cast<std::uint64_t>(offsetof(layout_purpose::Stream,bytes)), // Stream.offsetof.bytes
 static_cast<std::uint64_t>(offsetof(layout_purpose::Stream,maximum)), // Stream.offsetof.maximum
 static_cast<std::uint64_t>(offsetof(layout_purpose::Stream,write)), // Stream.offsetof.write
 static_cast<std::uint64_t>(offsetof(layout_purpose::Pair128,high)), // Pair128.offsetof.high
 static_cast<std::uint64_t>(offsetof(layout_purpose::Pair128,low)), // Pair128.offsetof.low
 static_cast<std::uint64_t>(offsetof(layout_purpose::FractionCache,rows)), // FractionCache.offsetof.rows
 static_cast<std::uint64_t>(offsetof(layout_purpose::FractionCache,N)), // FractionCache.offsetof.N
 static_cast<std::uint64_t>(offsetof(layout_purpose::FractionCache,g)), // FractionCache.offsetof.g
 static_cast<std::uint64_t>(offsetof(layout_purpose::FractionCache,count)), // FractionCache.offsetof.count
 static_cast<std::uint64_t>(offsetof(layout_purpose::Picks::Item,index)), // PicksItem.offsetof.index
 static_cast<std::uint64_t>(offsetof(layout_purpose::Picks::Item,slot)), // PicksItem.offsetof.slot
 static_cast<std::uint64_t>(offsetof(layout_purpose::Picks,items)), // Picks.offsetof.items
 static_cast<std::uint64_t>(offsetof(layout_purpose::Picks,values)), // Picks.offsetof.values
 static_cast<std::uint64_t>(offsetof(layout_purpose::Picks,next)), // Picks.offsetof.next
 static_cast<std::uint64_t>(offsetof(layout_purpose::Bounds,R)), // Bounds.offsetof.R
 static_cast<std::uint64_t>(offsetof(layout_purpose::Bounds,N)), // Bounds.offsetof.N
 static_cast<std::uint64_t>(offsetof(layout_purpose::Bounds,W)), // Bounds.offsetof.W
 static_cast<std::uint64_t>(offsetof(layout_purpose::Bounds,map)), // Bounds.offsetof.map
 static_cast<std::uint64_t>(offsetof(layout_purpose::Bounds,begin)), // Bounds.offsetof.begin
 static_cast<std::uint64_t>(offsetof(layout_purpose::Bounds,end)), // Bounds.offsetof.end
 static_cast<std::uint64_t>(offsetof(layout_purpose::Bounds,D)), // Bounds.offsetof.D
 static_cast<std::uint64_t>(offsetof(qc::Change,tick)), // qcChange.offsetof.tick
 static_cast<std::uint64_t>(offsetof(qc::Change,bpm)), // qcChange.offsetof.bpm
 static_cast<std::uint64_t>(offsetof(qc::Change,ramp)), // qcChange.offsetof.ramp
 static_cast<std::uint64_t>(offsetof(qc::Native,whole)), // qcNative.offsetof.whole
 static_cast<std::uint64_t>(offsetof(qc::Native,frac)), // qcNative.offsetof.frac
 static_cast<std::uint64_t>(offsetof(qc::Cut,admitted)), // qcCut.offsetof.admitted
 static_cast<std::uint64_t>(offsetof(qc::Cut,floor)), // qcCut.offsetof.floor
 static_cast<std::uint64_t>(offsetof(qc::Cut,low)), // qcCut.offsetof.low
 static_cast<std::uint64_t>(offsetof(qc::Cut,high)), // qcCut.offsetof.high
 static_cast<std::uint64_t>(offsetof(yesdaw::engine::TempoChange,tick)), // TempoChange.offsetof.tick
 static_cast<std::uint64_t>(offsetof(yesdaw::engine::TempoChange,bpm)), // TempoChange.offsetof.bpm
 static_cast<std::uint64_t>(offsetof(yesdaw::engine::TempoChange,curveToNext)), // TempoChange.offsetof.curveToNext
 static_cast<std::uint64_t>(offsetof(yesdaw::engine::MusicalTime,tick)), // MusicalTime.offsetof.tick
 static_cast<std::uint64_t>(offsetof(yesdaw::engine::MusicalTime,frac)), // MusicalTime.offsetof.frac
 static_cast<std::uint64_t>(offsetof(yesdaw::engine::SampleRate,hz)), // SampleRate.offsetof.hz
 static_cast<std::uint64_t>(offsetof(clock_stage::source_costs::FamilyBound,family)), // FamilyBound.offsetof.family
 static_cast<std::uint64_t>(offsetof(clock_stage::source_costs::FamilyBound,upperUnits)), // FamilyBound.offsetof.upperUnits
 static_cast<std::uint64_t>(offsetof(clock_stage::source_costs::WholeBound,runnerUpper)), // WholeBound.offsetof.runnerUpper
 static_cast<std::uint64_t>(offsetof(clock_stage::source_costs::WholeBound,producerUpper)), // WholeBound.offsetof.producerUpper
 static_cast<std::uint64_t>(offsetof(clock_stage::source_costs::WholeBound,verifierUpper)), // WholeBound.offsetof.verifierUpper
 static_cast<std::uint64_t>(offsetof(clock_stage::source_costs::WholeBound,totalUpper)), // WholeBound.offsetof.totalUpper
 static_cast<std::uint64_t>(_MSC_FULL_VER), // compiler.MSC_FULL_VER
 static_cast<std::uint64_t>(_MSC_VER), // compiler.MSC_VER
 static_cast<std::uint64_t>(_CRT_PACKING), // compiler.CRT_PACKING
 static_cast<std::uint64_t>(_ITERATOR_DEBUG_LEVEL), // compiler.ITERATOR_DEBUG_LEVEL
 static_cast<std::uint64_t>(clock_stage::source_costs::familyCount), // source.familyCount
};
}
