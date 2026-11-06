#include "independent_clock.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <xmmintrin.h>
#pragma fenv_access(on)
#pragma fp_contract(off)
namespace {
constexpr bool exactSourceReviewAccepted=false,fullOracleCostAccepted=false,stageOutputReservationAccepted=false;
constexpr std::uint64_t cap=1ull<<34;
struct Budget{std::uint64_t used=0;void add(std::uint64_t n){qc::require(used<=cap&&n<=cap-used,"ORACLE_WORK_REFUSED");used+=n;}};
void word(std::ofstream& out,std::int64_t x){const auto u=std::bit_cast<std::uint64_t>(x);for(unsigned j=0;j<8;++j)out.put(char((u>>(8*j))&255));qc::require(bool(out),"ORACLE_WRITE");}
struct Fractions{
 std::uint64_t N,g;std::vector<std::pair<double,double>> rows;
 Fractions(std::uint64_t n,Budget& b):N(n),g(std::gcd(n,std::uint64_t(153600))){
  const auto count=N/g;qc::require(count<=735,"ORACLE_FRACTION_PROFILE");b.add(count*2048);rows.reserve(std::size_t(count));
  for(std::uint64_t r=0;r<count;++r)rows.push_back(qc::fractionBounds(r*g,N));
 }
 qc::Cut cut(qc::NumericAClock& c,std::uint64_t i,Budget& b)const{
  b.add(256);qc::require(i<=N&&i<=std::numeric_limits<std::uint64_t>::max()/153600,"ORACLE_RATIONAL_DOMAIN");
  const auto product=i*153600,whole=product/N,rem=product%N;qc::require(rem%g==0,"ORACLE_CACHE_KEY");
  const auto f=rows[std::size_t(rem/g)];const auto tick=qc::clipStart+qc::Tick(whole);
  const double lo=c.frame({tick,f.first}),hi=f.second==1?c.frame({tick+1,0}):c.frame({tick,f.second});
  const bool ok=std::isfinite(lo)&&std::isfinite(hi)&&hi>=lo&&hi-lo<=1.0/1024&&std::floor(lo)==std::floor(hi);
  return {ok,ok?std::int64_t(std::floor(lo)):0,lo,hi};
 }
};
}
int main(int argc,char**argv){try{
 qc::require(exactSourceReviewAccepted&&fullOracleCostAccepted&&stageOutputReservationAccepted,"SOURCE_HOLD_ORACLE_SOURCE_COST_AND_STAGE_BOUNDS");
 qc::require(std::fegetround()==FE_TONEAREST&&(_mm_getcsr()&~0x3fu)==0x1f80u,"UnsupportedArithmetic");
 qc::require(argc==2,"USAGE oracle new-output-directory");std::filesystem::path output(argv[1]);qc::require(!std::filesystem::exists(output),"ORACLE_OUTPUT_EXISTS");
 Budget budget;budget.add(128000000);std::filesystem::create_directory(output);std::uint64_t entries=0;
 for(const std::uint64_t R:{44100ull,48000ull,96000ull}){const auto N=10*R;Fractions fractions(N,budget);
  for(int map=0;map<9;++map){qc::NumericAClock clock(double(R),qc::fixtureMap(map));const auto begin=fractions.cut(clock,0,budget),end=fractions.cut(clock,N,budget);qc::require(begin.admitted&&end.admitted&&end.floor>begin.floor,"MusicalBoundaryAmbiguous");
   const auto D=end.floor-begin.floor;qc::require(D<=std::int64_t(20*R),"ORACLE_FRAME_PROFILE");const int W=int(double(float(R))*0.12);
   const std::string name="r"+std::to_string(R)+"-map"+std::to_string(map);
   std::ofstream q(output/(name+"-q.bin"),std::ios::binary),meta(output/(name+"-metadata.json"));qc::require(bool(q)&&bool(meta),"ORACLE_OUTPUT_OPEN");
   std::int64_t prior=-1,edgeA=0,edgeB=0;
   for(std::uint64_t i=0;i<=N;++i){const auto cut=fractions.cut(clock,i,budget);qc::require(cut.admitted,"MusicalBoundaryAmbiguous");const auto local=cut.floor-begin.floor;qc::require(local>=prior&&local>=0&&local<=D,"ORACLE_MONOTONIC");prior=local;word(q,local);++entries;if(i==std::uint64_t(W))edgeA=local;if(i==N-std::uint64_t(W))edgeB=local;}
   const auto lo=std::max<std::int64_t>(W,edgeA),hi=std::min<std::int64_t>(D-W,edgeB);meta<<std::setprecision(17)<<"{\"rate\":"<<R<<",\"map\":"<<map<<",\"N\":"<<N<<",\"A\":"<<begin.floor<<",\"B\":"<<end.floor<<",\"D\":"<<D<<",\"W\":"<<W<<",\"edge_low\":"<<lo<<",\"edge_high\":"<<hi<<",\"windows\":[";bool firstWindow=true;
   for(std::int64_t start=0;start+std::int64_t(R/4)<=D;start+=std::int64_t(R/20)){budget.add(512);if(!firstWindow)meta<<',';firstWindow=false;meta<<"{\"start\":"<<start<<",\"eligible\":"<<((start>=lo&&start+std::int64_t(R/4)<=hi)?"true":"false")<<'}';}
   meta<<"],\"bursts\":[";bool firstBurst=true;for(std::uint64_t source:{2*R,4*R,6*R,8*R})for(bool reverse:{false,true}){const auto i=reverse?N-1-source:source;const auto cut=fractions.cut(clock,i,budget);qc::require(cut.admitted,"MusicalBoundaryAmbiguous");if(!firstBurst)meta<<',';firstBurst=false;meta<<"{\"source\":"<<source<<",\"reverse\":"<<(reverse?"true":"false")<<",\"Q\":"<<cut.floor-begin.floor<<'}';}
   meta<<"]}\n";if(R==48000&&map==5){std::filesystem::create_directories(output.parent_path()/"native-inverse");std::ofstream inverse(output.parent_path()/"native-inverse/r48000-map5.bin",std::ios::binary);qc::require(bool(inverse),"INVERSE_OUTPUT");for(std::int64_t j=0;j<D;++j){budget.add(2048);const auto t=clock.inverse(double(begin.floor+j));qc::require(std::abs(clock.frame(t)-double(begin.floor+j))<=1.0/1024,"INVERSE_RESIDUAL");word(inverse,t.whole);word(inverse,std::bit_cast<std::int64_t>(t.frac));}}
   qc::require(bool(q)&&bool(meta),"ORACLE_STREAM_FAILED");
  }}
 qc::require(entries==16929027,"ORACLE_INVENTORY");std::cout<<"ORACLE_COMPLETE "<<entries<<' '<<budget.used<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 2;}}
