#include "engine/MusicalAudioCutPlan.h"
#include "independent_clock.h"
#include <bit>
#include <array>
#include <cfenv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <xmmintrin.h>
#pragma fenv_access(on)
#pragma fp_contract(off)
namespace {constexpr bool sourceReviewAccepted=false,referenceReplayCostAccepted=false,outputInventoryAccepted=false;
void need(bool x,const char*s){if(!x)throw std::runtime_error(s);}
std::int64_t read(std::ifstream& f){std::uint64_t x=0;for(unsigned j=0;j<8;++j){const int c=f.get();need(c!=EOF,"Q_TABLE_TRUNCATED");x|=std::uint64_t(static_cast<unsigned char>(c))<<(8*j);}return std::bit_cast<std::int64_t>(x);}}
int main(int argc,char**argv){try{
 need(sourceReviewAccepted&&referenceReplayCostAccepted&&outputInventoryAccepted,"SOURCE_HOLD_PLAN_SOURCE_REFERENCES_AND_COST");
 need(std::fegetround()==FE_TONEAREST&&(_mm_getcsr()&~0x3fu)==0x1f80u,"UnsupportedArithmetic");need(argc==3,"USAGE plan frozen-q-root new-output-root");
 const std::filesystem::path input(argv[1]),output(argv[2]);need(!std::filesystem::exists(output),"PLAN_OUTPUT_EXISTS");std::filesystem::create_directory(output);std::uint64_t totalCalls=0;
 using namespace yesdaw::engine;
 for(const std::uint64_t R:{44100ull,48000ull,96000ull})for(int kind=0;kind<9;++kind){const auto N=10*R;const auto spec=qc::fixtureMap(kind);std::vector<TempoChange> changes;for(auto x:spec)changes.push_back({x.tick,x.bpm,x.ramp?TempoCurve::LinearRamp:TempoCurve::Jump});std::shared_ptr<const MusicalAudioClock> clock;need(MusicalAudioClock::build({changes.data(),changes.size()},{double(R)},{},clock)==MusicalAudioStatus::Ok,"CLOCK_BUILD");MusicalAudioWindow window;need(resolveMusicalAudioWindow(clock,qc::clipStart,qc::clipLength,N,window)==MusicalAudioStatus::Ok,"WINDOW_BUILD");
  const std::string key="r"+std::to_string(R)+"-map"+std::to_string(kind);std::ifstream q(input/(key+"-q.bin"),std::ios::binary);need(bool(q),"Q_TABLE_OPEN");std::vector<std::int64_t> expected;expected.reserve(std::size_t(N)+1);for(std::uint64_t i=0;i<=N;++i)expected.push_back(read(q));need(q.get()==EOF,"Q_TABLE_EXTRA");need(expected.front()==0&&expected.back()==window.frameCount(),"WINDOW_Q_BINDING");
  std::shared_ptr<const MusicalAudioCutPlan> cuts;const auto result=buildMusicalAudioCutPlan(window,{},{},cuts);std::ofstream report(output/(key+"-metadata.json"));need(bool(report),"PLAN_REPORT_OPEN");report<<"{\"status\":"<<int(result.status)<<",\"geometry_status\":"<<int(result.geometryStatus)<<",\"boundaries\":[";
  if(result.status!=MusicalAudioCutStatus::Ok){report<<"],\"waveform\":\"NOT_REACHED\"}\n";continue;}need(bool(cuts),"PLAN_NULL");std::ofstream calls(output/(key+"-calls.bin"),std::ios::binary);need(bool(calls),"CALLS_OPEN");std::uint64_t i=0;std::int64_t o=0;
  for(const auto& c:cuts->calls()){need(totalCalls<131072,"REFERENCE_CALL_CAP");++totalCalls;need(c.inputBegin==i&&c.outputBegin==o&&c.inputFrames>0&&c.outputFrames>0&&c.inputFrames<=2048&&c.outputFrames<=2048,"CALL_CONSERVATION");need(expected[std::size_t(i)]==o,"CALL_BEGIN_Q");i+=c.inputFrames;o+=c.outputFrames;need(i<=N&&expected[std::size_t(i)]==o,"CALL_END_Q");for(const auto value:std::array<std::uint64_t,6>{c.inputBegin,std::uint64_t(c.outputBegin),c.inputFrames,c.outputFrames,c.firstBoundary,c.boundaryCount})for(unsigned byte=0;byte<8;++byte)calls.put(char((value>>(8*byte))&255));}
  need(i==N&&o==window.frameCount(),"FINAL_N_D");bool firstBoundary=true;for(const auto& b:cuts->boundaries()){need(b.source.denominator>0&&b.source.numerator<b.source.denominator,"FRACTIONAL_BOUNDARY");if(!firstBoundary)report<<',';firstBoundary=false;report<<"{\"tick\":"<<b.tick<<",\"whole\":"<<b.source.whole<<",\"numerator\":"<<b.source.numerator<<",\"denominator\":"<<b.source.denominator<<'}';}report<<"]}\n";need(bool(calls)&&bool(report),"PLAN_OUTPUT_WRITE");
 }
 std::cout<<"PLAN_COMPLETE "<<totalCalls<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 2;}}
