// SOURCE-FIRST PROTOTYPE R4. R1/R2/R3 remain sealed. Catalogue data unbound; no execution authority.
#include "../waveform-definition-r3/independent_clock.h"
#include "engine/Time.h"
#include "source-costs.h"
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
namespace clock_stage {
constexpr bool sourceReviewAccepted=false, sourceWorkAccepted=false, executionAccepted=false;
constexpr bool compilerAccepted=false, produceAccepted=false, verifyAccepted=false;
constexpr bool compilerPeakDiskQualified=false, internalHeapRSSQualified=false, rendererReady=false;
constexpr std::uint64_t cap=1ull<<34, referencesCap=152627224, retainedCap=880803840;
constexpr std::uint64_t dense=16929027, queryUpper=16929297, inverseUpper=960000;
constexpr std::int64_t S=46081,L=153600;
constexpr char timeHash[]="FC5A0861B527FFFE6ED2E6F9FE571D3F8974329BBD065DACFBC38DFEA8C98590";
constexpr char portHash[]="A75FB5D17C9CACA2113DC8567A3E27F9A40FD9531BC59084E07F88576D15CB64";
static_assert(sizeof(double)==8 && sizeof(std::int64_t)==8 && std::endian::native==std::endian::little);
struct Rejection:std::exception {
 const char* reason;std::array<std::uint64_t,6> values{};
 Rejection(const char* r,std::array<std::uint64_t,6> v):reason(r),values(v){}
 const char* what()const noexcept override{return reason;}
};
struct CleanupCloseState{std::uint64_t failures=0;int firstReturn=0,firstErrno=0;const char* firstSite="none";};
CleanupCloseState cleanupClose;
void rememberCloseFailure(int result,int error,const char* site)noexcept{if(result!=0){if(cleanupClose.failures==0){cleanupClose.firstReturn=result;cleanupClose.firstErrno=error;cleanupClose.firstSite=site;}if(cleanupClose.failures<std::numeric_limits<std::uint64_t>::max())++cleanupClose.failures;}}
void cleanupFileClose(FILE* file,const char* site)noexcept{if(file){const int result=std::fclose(file);const int error=errno;rememberCloseFailure(result,error,site);}}
void cleanupDescriptorClose(int descriptor)noexcept{const int result=_close(descriptor);const int error=errno;rememberCloseFailure(result,error,"fdopen-descriptor");}
void reportCleanupClose()noexcept{std::fprintf(stderr,"cleanup_close_failures=%llu first_return=%d first_errno=%d first_site=%s\n",cleanupClose.failures,cleanupClose.firstReturn,cleanupClose.firstErrno,cleanupClose.firstSite);}
// Genuine no-witness overload: no six-field tuple exists on a success path.
void gate(bool ok,const char* r){if(!ok)throw Rejection(r,{});}
void gate(bool ok,const char* r,std::array<std::uint64_t,6> v){if(!ok)throw Rejection(r,v);}
std::uint64_t bits(double x){return std::bit_cast<std::uint64_t>(x);}
void reservationGate(std::uint64_t total,std::uint64_t limit){gate(total<=limit&&limit<=cap,"WHOLE_STAGE_WORK_RESERVATION",{total,limit,cap});}
void bytesGate(std::uint64_t total,std::uint64_t limit){gate(total<=limit&&limit<=retainedCap,"WHOLE_STAGE_RETAINED_RESERVATION",{total,limit,retainedCap});}
// No numeric catalogue is defined. Every lookup and checked quote operation
// is part of the later independent source census; none is free instrumentation.
using Family=source_costs::Family;
struct Cost {
 static const source_costs::Catalogue& catalogue(){return source_costs::immutableCatalogue();}
 static std::uint64_t price(Family id){return catalogue().family(id).upperUnits;}
 static void validate(){
  const auto& book=catalogue();
  for(std::size_t i=0;i<source_costs::familyCount;++i)(void)book.family(static_cast<Family>(i));
  const auto& whole=book.whole();
  gate(whole.runnerUpper>0&&whole.runnerUpper<=cap&&whole.producerUpper>0&&whole.producerUpper<=cap&&whole.verifierUpper>0&&whole.verifierUpper<=cap&&whole.totalUpper>0&&whole.totalUpper<=cap,"UNBOUND_OR_INVALID_WHOLE_CATALOGUE");
 }
 static std::uint64_t plus(std::uint64_t a,std::uint64_t b){gate(a<=cap&&b<=cap-a,"SOURCE_QUOTE_SUM");return a+b;}
 static std::uint64_t times(std::uint64_t count,std::uint64_t value){gate(value>0&&value<=cap&&count<=cap/value,"SOURCE_QUOTE_PRODUCT");return count*value;}
 static std::uint64_t units(Family id,std::uint64_t count){return times(count,price(id));}
 static std::uint64_t wordUnits(bool verify,std::uint64_t words){
  gate(words>0&&words<=std::numeric_limits<std::uint64_t>::max()-8191,"SOURCE_WORD_QUOTE_DOMAIN");
  const auto chunks=(words+8191)/8192;
  const auto fallback=verify?chunks:chunks-1;
  const auto dispatch=units(verify?Family::WordReadDispatch:Family::WordWriteDispatch,words);
  const auto fast=units(verify?Family::WordReadFast:Family::WordWriteFast,words-fallback);
  const auto slow=units(verify?Family::WordReadFallback:Family::WordWriteFallback,fallback);
  return plus(dispatch,plus(fast,slow));
 }
 static std::uint64_t inverseUnits(bool verify,std::uint64_t records){
  gate(records>0&&records<=inverseUpper,"SOURCE_INVERSE_QUOTE_DOMAIN");
  const auto api=verify?plus(price(Family::ProductionInverseNative),price(Family::ProductionForwardInverse)):
   plus(price(Family::QcInverseNative),price(Family::QcForwardNative));
  const auto outer=price(verify?Family::InverseVerifyOuter:Family::InverseProduceOuter);
  return plus(times(records,plus(api,outer)),wordUnits(verify,2*records));
 }
 static std::uint64_t fixtureUnits(bool verify,std::uint64_t N,std::uint64_t g,int map){
  const auto queries=N+11,zeros=g+7;gate(queries>=zeros,"SOURCE_FIXTURE_QUOTE_DOMAIN");
  const bool ramp=map==5||map==6;
  const auto fractional=price(verify?(ramp?Family::ProductionForwardRamp:Family::ProductionForwardConstant):
   (ramp?Family::PortFractionRamp:Family::PortFractionConstant));
  const auto zero=price(verify?(ramp?Family::ProductionForwardZeroRamp:Family::ProductionForwardZeroConstant):Family::PortZero);
  const auto cut=price(Family::RationalCut);
  const auto fractionalQueries=times(queries-zeros,plus(cut,times(2,fractional)));
  const auto zeroQueries=times(zeros,plus(cut,times(2,zero)));
  const auto records=plus(units(Family::DenseOuter,N+1),wordUnits(verify,N+1));
  return plus(plus(fractionalQueries,zeroQueries),records);
 }
 static std::uint64_t childUpper(bool verify){
  auto total=price(verify?Family::ChildFixedVerify:Family::ChildFixedProduce);
  if(!verify)total=plus(total,units(Family::PortCacheRefill,27*153611));
  for(const std::uint64_t R:{44100ull,48000ull,96000ull}){
   const auto g=R==44100?600ull:R==48000?19200ull:38400ull;
   for(int map=0;map<9;++map)total=plus(total,fixtureUnits(verify,10*R,g,map));
  }
  return plus(total,inverseUnits(verify,inverseUpper));
 }
};
struct Meter {
 std::uint64_t used=0,reserved,prior;
 Meter(std::uint64_t p,std::uint64_t r):reserved(r),prior(p){}
 void add(std::uint64_t n){gate(n<=reserved-used,"CHILD_SOURCE_WORK",{used,n,reserved});used+=n;}
};
struct Text {
 std::array<char,65536> data{};std::size_t size=0;
 void add(const char* s){while(*s){gate(size<data.size(),"METADATA_CAP");data[size++]=*s++;}}
 void u(std::uint64_t x){char digits[20];unsigned n=0;do{digits[n++]=char('0'+x%10);x/=10;}while(x);while(n){gate(size<data.size(),"METADATA_CAP");data[size++]=digits[--n];}}
 void i(std::int64_t x){gate(x>=0,"NEGATIVE_FIXTURE_METADATA");u(std::uint64_t(x));}
 void boolean(bool x){add(x?"true":"false");}
 void quoted(const std::string& s){add("\"");for(char c:s){gate(c>=32&&c<127&&c!='"'&&c!='\\',"SAFE_FIXED_STRING");char q[2]{c,0};add(q);}add("\"");}
};
std::string number(std::uint64_t x){Text t;t.u(x);return {t.data.data(),t.size};}
std::string path(const std::string& root,const std::string& relative){gate(root.size()<=512&&relative.size()<=128,"PATH_PROFILE");return root+"/"+relative;}
struct Stream {
 FILE* file=nullptr;std::array<unsigned char,65536> buffer{};std::size_t at=0,size=0;
 std::uint64_t bytes=0,maximum;bool write;
 Stream(const std::string& name,bool w,std::uint64_t m):maximum(m),write(w){
  if(w){int descriptor=-1;const auto e=_sopen_s(&descriptor,name.c_str(),_O_CREAT|_O_EXCL|_O_WRONLY|_O_BINARY,_SH_DENYRW,_S_IREAD|_S_IWRITE);gate(e==0,"NEW_OUTPUT_OPEN",{std::uint64_t(e)});file=_fdopen(descriptor,"wb");if(!file)cleanupDescriptorClose(descriptor);}
  else file=_fsopen(name.c_str(),"rb",_SH_DENYWR);
  gate(file!=nullptr,"STREAM_OPEN");if(std::setvbuf(file,nullptr,_IONBF,0)!=0){FILE* owner=file;file=nullptr;cleanupFileClose(owner,"setvbuf-refusal");gate(false,"STREAM_BUFFER_MODE");}
 }
 ~Stream()noexcept{FILE* owner=file;file=nullptr;cleanupFileClose(owner,"Stream-destructor");}
 Stream(const Stream&)=delete;Stream& operator=(const Stream&)=delete;
 void flush(){if(at){const auto written=std::fwrite(buffer.data(),1,at,file);const int error=errno;gate(written==at,"STREAM_WRITE",{bytes,at,written,std::uint64_t(error)});at=0;}}
 void put(const unsigned char* p,std::size_t n){gate(write&&n<=maximum-bytes,"OUTPUT_BEFORE_WRITE_CAP",{bytes,n,maximum});bytes+=n;while(n){const auto k=(std::min)(n,buffer.size()-at);std::memcpy(buffer.data()+at,p,k);at+=k;p+=k;n-=k;if(at==buffer.size())flush();}}
 void word(std::uint64_t x){
  if(write&&(at&7u)==0&&(bytes&7u)==0&&bytes<=maximum&&8<=maximum-bytes&&at<=buffer.size()&&8<=buffer.size()-at){std::memcpy(buffer.data()+at,&x,8);at+=8;bytes+=8;return;}
  unsigned char a[8];std::memcpy(a,&x,8);put(a,8);
 }
 void exact(unsigned char* p,std::size_t n){gate(!write&&n<=maximum-bytes,"INPUT_RECORD_CAP",{bytes,n,maximum});while(n){if(at==size){size=std::fread(buffer.data(),1,buffer.size(),file);at=0;gate(size>0&&!std::ferror(file),"EXACT_RECORD_LENGTH_AND_EOF",{bytes,n,size});}const auto k=(std::min)(n,size-at);std::memcpy(p,buffer.data()+at,k);at+=k;p+=k;n-=k;bytes+=k;}}
 std::uint64_t word(){
  std::uint64_t x=0;
  if(!write&&(at&7u)==0&&(bytes&7u)==0&&bytes<=maximum&&8<=maximum-bytes&&size<=buffer.size()&&at<=size&&8<=size-at){std::memcpy(&x,buffer.data()+at,8);at+=8;bytes+=8;return x;}
  exact(reinterpret_cast<unsigned char*>(&x),8);return x;
 }
 void finish(std::uint64_t expected){gate(bytes==expected,"EXACT_FILE_BYTES",{bytes,expected});if(write){flush();const int result=std::fflush(file);const int error=errno;gate(result==0,"STREAM_FLUSH",{std::uint64_t(std::int64_t(result)),std::uint64_t(error),bytes});}else{gate(at==size&&std::fgetc(file)==EOF&&!std::ferror(file),"EXACT_RECORD_LENGTH_AND_EOF",{bytes,at,size});}FILE* owner=file;file=nullptr;const int result=std::fclose(owner);const int error=errno;gate(result==0,"STREAM_CLOSE",{std::uint64_t(std::int64_t(result)),std::uint64_t(error),bytes});}
};
void writeText(const std::string& p,const Text& t){Stream f(p,true,65536);f.put(reinterpret_cast<const unsigned char*>(t.data.data()),t.size);f.finish(t.size);}
std::array<std::uint64_t,6> textWitness(const Text& a,const Text& b){std::uint64_t ha=14695981039346656037ull,hb=ha;std::size_t first=0;for(std::size_t i=0;i<a.size;++i)ha=(ha^std::uint64_t(static_cast<unsigned char>(a.data[i])))*1099511628211ull;for(std::size_t i=0;i<b.size;++i)hb=(hb^std::uint64_t(static_cast<unsigned char>(b.data[i])))*1099511628211ull;while(first<a.size&&first<b.size&&a.data[first]==b.data[first])++first;return {a.size,b.size,ha,hb,first};}
void textGate(const Text& observed,const Text& expected){gate(observed.size==expected.size&&std::memcmp(observed.data.data(),expected.data.data(),expected.size)==0,"EXACT_CLOCK_METADATA",textWitness(observed,expected));}
Text readText(const std::string& p,const Text& expected){Text t;Stream f(p,false,65536);t.size=expected.size;f.exact(reinterpret_cast<unsigned char*>(t.data.data()),t.size);f.finish(t.size);textGate(t,expected);return t;}
struct Pair128{std::uint64_t high,low;};
Pair128 multiply(std::uint64_t a,std::uint64_t b){std::uint64_t high=0;const auto low=_umul128(a,b,&high);return {high,low};}
int compareRational(std::uint64_t encoding,std::uint64_t numerator,std::uint64_t denominator){
 const auto exponent=unsigned((encoding>>52)&2047),shift=1075u-exponent;
 gate(exponent>=1013&&exponent<=1023&&shift>0&&shift<64,"INDEPENDENT_FRACTION_WIDTH");
 const auto product=multiply((encoding&0xfffffffffffffull)|(1ull<<52),denominator);
 const Pair128 target{numerator>>(64-shift),numerator<<shift};
 return product.high<target.high?-1:product.high>target.high?1:product.low<target.low?-1:product.low>target.low?1:0;
}
std::pair<double,double> independentFraction(std::uint64_t n,std::uint64_t d){
 gate(d>0&&n<d,"INDEPENDENT_RATIONAL_DOMAIN",{n,d});if(!n)return {0,0};
 std::uint64_t low=0x3f50000000000000ull,high=0x3ff0000000000000ull;
 gate(compareRational(low,n,d)<=0,"INDEPENDENT_FIXTURE_FRACTION_MINIMUM");unsigned iterations=0;
 while(low<high){gate(++iterations<=64,"INDEPENDENT_FRACTION_SEARCH");const auto middle=low+(high-low+1)/2;if(compareRational(middle,n,d)<=0)low=middle;else high=middle-1;}
 const auto upper=compareRational(low,n,d)==0?low:low+1;
 gate(compareRational(low,n,d)<=0&&compareRational(upper,n,d)>=0,"INDEPENDENT_FRACTION_ENCLOSURE");
 return {std::bit_cast<double>(low),std::bit_cast<double>(upper)};
}
struct FractionCache {
 std::array<std::pair<double,double>,735> rows{};std::uint64_t N,g,count;
 FractionCache(std::uint64_t n,bool independent):N(n),g(n==441000?600:n==480000?19200:n==960000?38400:0),count(g?n/g:0){
  gate(g&&count<=rows.size(),"FRACTION_PROFILE",{N,g,count});
  for(std::uint64_t r=0;r<count;++r){const auto original=qc::fractionBounds(r*g,N);rows[std::size_t(r)]=independent?independentFraction(r*g,N):original;gate(bits(rows[std::size_t(r)].first)==bits(original.first)&&bits(rows[std::size_t(r)].second)==bits(original.second),"ORIGINAL_CACHE_BITS",{r,bits(rows[std::size_t(r)].first),bits(original.first),bits(rows[std::size_t(r)].second),bits(original.second)});}
 }
};
static_assert(sizeof(Text)<=65552&&sizeof(Stream)<=65664);
static_assert(sizeof(qc::Change)<=32&&sizeof(yesdaw::engine::TempoChange)<=32);
static_assert(8*(sizeof(Stream)+sizeof(Text))+sizeof(FractionCache)+32768<=2097152);
// A genuine repeated-subexpression cache. base.frame(whole,0), k, segment and
// nearest-endpoint bpm/slope are evaluated in their original operation order.
// Each fractional result retains the original scaled/log1p/add grouping.
struct CachedPort {
private:
 std::size_t segmentIndex_=0;unsigned cursorAdvances_=0;
public:
 std::vector<qc::Change> changes;qc::NumericAClock base;double rate;
 qc::Tick saved=-1;double wholeFrame=0,k=0,bpm=0,slope=0;Meter& meter;std::uint64_t misses=0;
 CachedPort(double r,int map,Meter& m):changes(qc::fixtureMap(map)),base(r,changes),rate(r),meter(m){if(changes.size()<3||changes.size()>4)throw Rejection("PORT_CURSOR_MAP_PROFILE",{});}
 double frame(qc::Native t){
  qc::require(t.whole>=0&&t.frac>=0&&t.frac<1,"CLOCK_NATIVE_DOMAIN");
  if(saved!=t.whole){meter.add(Cost::price(Family::PortCacheRefill));gate(++misses<=153611,"PORT_CACHE_MISS_PROFILE");
   // Query order is begin,end,then the complete increasing dense grid. A
   // backward query resets this owned cursor. At most6 successful advances
   // across the <=4 pinned rows are admitted for one map, including rewind.
   if(t.whole<saved)segmentIndex_=0;
   if(segmentIndex_>=changes.size())throw Rejection("PORT_CURSOR_INDEX_PROFILE",{});
   while(segmentIndex_+1<changes.size()&&changes[segmentIndex_+1].tick<=t.whole){if(++cursorAdvances_>6)throw Rejection("PORT_CURSOR_ADVANCE_PROFILE",{});++segmentIndex_;}
   if(changes[segmentIndex_].tick>t.whole)throw Rejection("PORT_CURSOR_LOWER_BOUND",{});const auto i=segmentIndex_;
   saved=t.whole;wholeFrame=base.frame({t.whole,0});k=60.0*rate/double(qc::ppq);
   bpm=changes[i].bpm;slope=0;if(i+1<changes.size()&&changes[i].ramp&&changes[i+1].bpm!=bpm){const qc::Tick length=changes[i+1].tick-changes[i].tick,local=t.whole-changes[i].tick;slope=(changes[i+1].bpm-bpm)/double(length);qc::require(slope!=0&&std::isfinite(slope),"CLOCK_LOCAL_SLOPE");bpm=local<=length/2?bpm+slope*double(local):changes[i+1].bpm-slope*double(length-local);}}
  if(t.frac==0)return wholeFrame;
  qc::require(std::isfinite(bpm)&&bpm>0,"CLOCK_LOCAL_BPM");double extra=0;
  if(slope==0)qc::require(qc::scaled(t.frac,k,bpm,extra),"CLOCK_LOCAL_RATIO");else{double relative=0;qc::require(qc::scaled(slope,t.frac,bpm,relative)&&relative>-1,"CLOCK_LOCAL_LOG_DOMAIN");qc::require(qc::scaled(k,std::log1p(relative),slope,extra),"CLOCK_LOCAL_LOG_RATIO");}
  qc::require(extra<0x1p63-wholeFrame,"CLOCK_FRAME_OVERFLOW");const double result=wholeFrame+extra;qc::checkFrame(result);return result;
 }
};
using Production=yesdaw::engine::CompiledTempoMap;
std::array<yesdaw::engine::TempoChange,4> productionRows(int map,std::size_t& count){
 using Curve=yesdaw::engine::TempoCurve;gate(map>=0&&map<9,"PRODUCTION_FIXTURE_MAP");const double constants[5]{120,80,60,40,30};const bool down=map==5||map==7;const double first=map<5?constants[map]:down?120:30,last=map<5?first:down?30:120;
 count=map<7?3:4;std::array<yesdaw::engine::TempoChange,4> rows{{{0,90,Curve::Jump},{S,first,map==5||map==6?Curve::LinearRamp:Curve::Jump},{S+L,last,Curve::Jump},{S+L,last,Curve::Jump}}};
 if(map>=7){rows[2]={S+4*15360+1,last,Curve::Jump};rows[3]={S+L,last,Curve::Jump};}return rows;
}
Production buildProduction(std::uint64_t R,int map){std::size_t count=0;const auto rows=productionRows(map,count);Production out;gate(Production::build({rows.data(),count},{double(R)},out),"ACTUAL_PRODUCTION_CLOCK_BUILD");return out;}
double productionFrame(const Production& c,qc::Native t){double out=0;const auto status=c.frameForMusicalTime({t.whole,t.frac},out);if(status!=yesdaw::engine::TimeLookupStatus::Ok)throw Rejection("ACTUAL_PRODUCTION_FORWARD",{std::uint64_t(status),std::uint64_t(t.whole),bits(t.frac)});return out;}
qc::Native highEndpoint(qc::Tick tick,double fraction){gate(fraction>=0&&fraction<=1&&tick<std::numeric_limits<qc::Tick>::max(),"FRACTION_ENDPOINT_DOMAIN");return fraction==1?qc::Native{tick+1,0}:qc::Native{tick,fraction};}
void endpointGate(qc::Native observed,qc::Native expected){if(!(observed.whole==expected.whole&&bits(observed.frac)==bits(expected.frac)))throw Rejection("EXACT_NORMALIZED_FRACTION_ENDPOINT",{std::uint64_t(observed.whole),bits(observed.frac),std::uint64_t(expected.whole),bits(expected.frac)});}
qc::Cut admittedCut(double low,double high){const bool ok=std::isfinite(low)&&std::isfinite(high)&&high>=low&&high-low<=1.0/1024&&std::floor(low)==std::floor(high);if(!ok)throw Rejection("MusicalBoundaryAmbiguous",{bits(low),bits(high)});return {true,std::int64_t(std::floor(low)),low,high};}
template<class Clock>qc::Cut cut(Clock& clock,const FractionCache& cache,std::uint64_t i){
 gate(i<=cache.N&&i<=std::numeric_limits<std::uint64_t>::max()/153600,"RATIONAL_QUERY_DOMAIN");const auto product=i*153600,whole=product/cache.N,remainder=product%cache.N;gate(remainder%cache.g==0,"FRACTION_CACHE_KEY");const auto pair=cache.rows[std::size_t(remainder/cache.g)];const auto tick=S+qc::Tick(whole);const auto upper=highEndpoint(tick,pair.second);endpointGate(upper,pair.second==1?qc::Native{tick+1,0}:qc::Native{tick,pair.second});
 if constexpr(std::is_same_v<Clock,CachedPort>){const double low=clock.frame({tick,pair.first});const double high=clock.frame(upper);return admittedCut(low,high);}
 else{const double low=productionFrame(clock,{tick,pair.first});const double high=productionFrame(clock,upper);return admittedCut(low,high);}
}
void wordGate(std::int64_t observed,std::int64_t expected,std::uint64_t i,std::uint64_t R,int map){if(observed!=expected)throw Rejection("EXACT_Q_WORD",{std::uint64_t(observed),std::uint64_t(expected),i,R,std::uint64_t(map)});}
void inverseGate(qc::Native observed,yesdaw::engine::MusicalTime expected,std::uint64_t j){gate(observed.whole==expected.tick&&bits(observed.frac)==bits(expected.frac),"EXACT_INVERSE_NATIVE_BITS",{std::uint64_t(observed.whole),bits(observed.frac),std::uint64_t(expected.tick),bits(expected.frac),j});}
struct Picks {
 struct Item{std::uint64_t index;unsigned slot;};std::array<Item,10> items{};std::array<std::int64_t,10> values{};unsigned next=0;
 Picks(std::uint64_t R,std::uint64_t N,std::uint64_t W){items[0]={W,0};items[1]={N-W,1};for(unsigned k=0;k<4;++k){const auto source=2*R*(k+1);items[2+2*k]={source,2+2*k};items[3+2*k]={N-1-source,3+2*k};}for(unsigned a=1;a<10;++a){auto x=items[a];unsigned z=a;while(z&&x.index<items[z-1].index){items[z]=items[z-1];--z;}items[z]=x;}}
 void see(std::uint64_t i,std::int64_t q){while(next<10&&items[next].index==i){values[items[next].slot]=q;++next;}}
};
struct Bounds{std::uint64_t R,N,W;int map;qc::Cut begin,end;std::int64_t D;};
void field(Text& t,const char* name,std::uint64_t value){t.add(name);t.u(value);}
Text metadata(const Bounds& b,const Picks& picks,const std::string& manifest,bool inverse){
 Text t;t.add("{\"schema\":1,\"source_manifest_sha256\":");t.quoted(manifest);t.add(",\"time_sha256\":\"");t.add(timeHash);t.add("\",\"independent_clock_sha256\":\"");t.add(portHash);t.add("\"");
 field(t,",\"rate\":",b.R);field(t,",\"map\":",std::uint64_t(b.map));field(t,",\"S\":",S);field(t,",\"L\":",L);field(t,",\"N\":",b.N);field(t,",\"A\":",std::uint64_t(b.begin.floor));field(t,",\"B\":",std::uint64_t(b.end.floor));field(t,",\"D\":",std::uint64_t(b.D));field(t,",\"W\":",b.W);
 field(t,",\"A_low_bits\":",bits(b.begin.low));field(t,",\"A_high_bits\":",bits(b.begin.high));field(t,",\"B_low_bits\":",bits(b.end.low));field(t,",\"B_high_bits\":",bits(b.end.high));
 t.add(",\"complete_map\":[");std::size_t count=0;const auto rows=productionRows(b.map,count);for(std::size_t i=0;i<count;++i){if(i)t.add(",");t.add("[");t.i(rows[i].tick);t.add(",");t.u(bits(rows[i].bpm));t.add(",");t.u(rows[i].curveToNext==yesdaw::engine::TempoCurve::LinearRamp?1:0);t.add("]");}t.add("]");
 if(inverse){t.add(",\"file\":\"references/native-inverse/r48000-map5.bin\",\"record_layout\":\"int64-whole,uint64-binary64-frac,little-endian\"");field(t,",\"record_bytes\":",16);field(t,",\"records\":",std::uint64_t(b.D));field(t,",\"first_frame\":",std::uint64_t(b.begin.floor));field(t,",\"exclusive_frame_end\":",std::uint64_t(b.end.floor));t.add(",\"residual_limit\":\"1/1024\",\"bake_case_binding\":\"HOLD\",\"center_owner\":false}\n");return t;}
 const auto lo=(std::max)(std::int64_t(b.W),picks.values[0]),hi=(std::min)(b.D-std::int64_t(b.W),picks.values[1]);field(t,",\"edge_low\":",std::uint64_t(lo));field(t,",\"edge_high\":",std::uint64_t(hi));t.add(",\"windows\":[");bool first=true;unsigned windows=0;
 for(std::int64_t start=0;start+std::int64_t(b.R/4)<=b.D;start+=std::int64_t(b.R/20)){gate(++windows<=396,"WINDOW_GRID_PROFILE");if(!first)t.add(",");first=false;t.add("{\"start\":");t.i(start);t.add(",\"eligible\":");t.boolean(start>=lo&&start+std::int64_t(b.R/4)<=hi);t.add("}");}t.add("],\"bursts\":[");
 for(unsigned k=0;k<8;++k){if(k)t.add(",");t.add("{\"source\":");t.u(2*b.R*(k/2+1));t.add(",\"reverse\":");t.boolean(k%2!=0);t.add(",\"Q\":");t.i(picks.values[2+k]);t.add("}");}t.add("],\"physical_center_owner\":false}\n");return t;
}
template<class F>void expect(const char* id,F action,const char* reason,std::array<std::uint64_t,6> witness){bool caught=false;const auto beforeCleanup=cleanupClose.failures;try{action();}catch(const Rejection& e){gate(std::strcmp(e.reason,reason)==0&&e.values==witness,"CONTROL_WRONG_REJECTION");caught=true;gate(cleanupClose.failures==beforeCleanup,"CONTROL_CLEANUP_CLOSE_FAILURE",{beforeCleanup,cleanupClose.failures});std::printf("{\"control\":\"%s\",\"gate\":\"%s\",\"operands\":[%llu,%llu,%llu,%llu,%llu,%llu]}\n",id,e.reason,e.values[0],e.values[1],e.values[2],e.values[3],e.values[4],e.values[5]);}gate(caught,"CONTROL_GATE_NOT_REACHED");}
void controls(std::uint64_t totalWork){
 reservationGate(totalWork,cap);expect("work-one-below",[&]{reservationGate(totalWork,totalWork-1);},"WHOLE_STAGE_WORK_RESERVATION",{totalWork,totalWork-1,cap});
 const auto n=std::numeric_limits<std::uint64_t>::max()-1,d=std::numeric_limits<std::uint64_t>::max();const auto independent=independentFraction(n,d),original=qc::fractionBounds(n,d);gate(bits(independent.first)==bits(original.first)&&bits(independent.second)==bits(original.second)&&independent.second==1,"CARRY_CONTROL_REACHED");const auto normalized=highEndpoint(S,independent.second);endpointGate(normalized,{S+1,0});expect("fraction-carry",[&]{endpointGate({S,independent.second},normalized);},"EXACT_NORMALIZED_FRACTION_ENDPOINT",{S,bits(independent.second),S+1,0});
 const auto production=buildProduction(48000,5);gate(bits(productionFrame(production,normalized))==bits(productionFrame(production,{S+1,0})),"CARRY_ACTUAL_PRODUCTION_BASELINE");expect("fraction-carry-native-read",[&]{productionFrame(production,{S,independent.second});},"ACTUAL_PRODUCTION_FORWARD",{std::uint64_t(yesdaw::engine::TimeLookupStatus::InvalidInput),S,bits(independent.second)});
}
void run(bool verify,const std::string& root,const std::string& manifest,Meter& meter,std::uint64_t totalWork){
 meter.add(Cost::price(verify?Family::ChildFixedVerify:Family::ChildFixedProduce));controls(totalWork);std::uint64_t entries=0,inverseRecords=0;bool wordControl=false,inverseControl=false,prefixControl=false,rampControl=false,metadataControl=false;
 for(const std::uint64_t R:{44100ull,48000ull,96000ull}){const std::uint64_t N=10*R;const FractionCache fractions(N,verify);
  for(int map=0;map<9;++map){CachedPort generator(double(R),map,meter);auto production=buildProduction(R,map);
   // Complete query, dispatch/fast/fallback and outer work is quoted before
   // the first read/query. Catalogue rows and all instrumentation remain UNBOUND.
   meter.add(Cost::fixtureUnits(verify,N,fractions.g,map));
   const auto get=[&](std::uint64_t i){return verify?cut(production,fractions,i):cut(generator,fractions,i);};const auto a=get(0),z=get(N);gate(z.floor>a.floor&&z.floor-a.floor<=std::int64_t(20*R),"CLOCK_DURATION_PROFILE");const Bounds b{R,N,std::uint64_t(int(double(float(R))*0.12)),map,a,z,z.floor-a.floor};Picks picks(R,N,b.W);const auto stem="r"+number(R)+"-map"+number(std::uint64_t(map));Stream q(path(root,"references/clock/"+stem+"-q.bin"),!verify,8*(N+1));std::int64_t prior=-1;
   for(std::uint64_t i=0;i<=N;++i){const auto expected=get(i).floor-a.floor;gate(expected>=prior&&expected>=0&&expected<=b.D,"Q_MONOTONIC_PROFILE");prior=expected;const auto actual=verify?std::bit_cast<std::int64_t>(q.word()):expected;if(!verify)q.word(std::bit_cast<std::uint64_t>(expected));wordGate(actual,expected,i,R,map);picks.see(i,expected);++entries;
    if(verify&&!wordControl){wordGate(actual,expected,i,R,map);expect("q-off-by-one",[&]{wordGate(actual+1,expected,i,R,map);},"EXACT_Q_WORD",{std::uint64_t(actual+1),std::uint64_t(expected),i,R,std::uint64_t(map)});
     std::array<unsigned char,9> owned{};const auto encoded=std::bit_cast<std::uint64_t>(actual);std::memcpy(owned.data(),&encoded,8);owned[8]=0xa5;
     {Stream shortFile(root+"/../controls/short.bin",true,7);shortFile.put(owned.data(),7);shortFile.finish(7);}expect("truncated-record",[&]{Stream shortFile(root+"/../controls/short.bin",false,8);shortFile.word();},"EXACT_RECORD_LENGTH_AND_EOF",{7,1,0});
     {Stream extraFile(root+"/../controls/trailing.bin",true,9);extraFile.put(owned.data(),9);extraFile.finish(9);}expect("trailing-record",[&]{Stream extraFile(root+"/../controls/trailing.bin",false,9);extraFile.word();extraFile.finish(8);},"EXACT_RECORD_LENGTH_AND_EOF",{8,8,9});wordControl=true;}}
   q.finish(8*(N+1));gate(picks.next==10,"ALL_EDGE_BURST_POSITIONS");const auto text=metadata(b,picks,manifest,false);if(verify){auto actual=readText(path(root,"references/clock/"+stem+"-metadata.json"),text);if(!metadataControl){gate(actual.size>1,"METADATA_CONTROL_REACHED");actual.data[0]='[';expect("metadata-corruption",[&]{textGate(actual,text);},"EXACT_CLOCK_METADATA",textWitness(actual,text));metadataControl=true;}}else writeText(path(root,"references/clock/"+stem+"-metadata.json"),text);
   if(verify&&!prefixControl){std::size_t count=0;auto rows=productionRows(map,count);rows[0].bpm=120;Production mutant;gate(Production::build({rows.data(),count},{double(R)},mutant),"PREFIX_MUTANT_BUILD");const double changedA=productionFrame(mutant,{S,0}),changedB=productionFrame(mutant,{S+L,0});auto changed=b;changed.begin=admittedCut(changedA,changedA);changed.end=admittedCut(changedB,changedB);changed.D=changed.end.floor-changed.begin.floor;const auto actual=metadata(changed,picks,manifest,false);expect("full-prefix",[&]{textGate(actual,text);},"EXACT_CLOCK_METADATA",textWitness(actual,text));prefixControl=true;}
   if(verify&&map==5&&!rampControl){std::size_t count=0;auto rows=productionRows(map,count);rows[1].curveToNext=yesdaw::engine::TempoCurve::Jump;Production mutant;gate(Production::build({rows.data(),count},{double(R)},mutant),"RAMP_MUTANT_BUILD");const auto changed=std::int64_t(std::floor(productionFrame(mutant,{S+L,0})))-a.floor;expect("ramp-law",[&]{wordGate(changed,b.D,N,R,map);},"EXACT_Q_WORD",{std::uint64_t(changed),std::uint64_t(b.D),N,R,std::uint64_t(map)});rampControl=true;}
   if(R==48000&&map==5){const auto descriptor=metadata(b,picks,manifest,true);const std::string binary="references/native-inverse/r48000-map5.bin",json="references/native-inverse/r48000-map5.json";if(verify)readText(path(root,json),descriptor);else writeText(path(root,json),descriptor);Stream inv(path(root,binary),!verify,16*std::uint64_t(b.D));
    meter.add(Cost::inverseUnits(verify,std::uint64_t(b.D)));
    for(std::int64_t j=0;j<b.D;++j){const double frame=double(a.floor+j);qc::Native actual{};
     if(verify){actual.whole=std::bit_cast<std::int64_t>(inv.word());actual.frac=std::bit_cast<double>(inv.word());yesdaw::engine::MusicalTime expected;gate(production.musicalTimeForFrame(frame,expected)==yesdaw::engine::TimeLookupStatus::Ok,"ACTUAL_PRODUCTION_INVERSE");inverseGate(actual,expected,std::uint64_t(j));gate(std::abs(productionFrame(production,actual)-frame)<=1.0/1024,"INVERSE_FORWARD_RESIDUAL");
      if(!inverseControl){auto corrupt=actual;++corrupt.whole;expect("inverse-whole",[&]{inverseGate(corrupt,expected,std::uint64_t(j));},"EXACT_INVERSE_NATIVE_BITS",{std::uint64_t(corrupt.whole),bits(corrupt.frac),std::uint64_t(expected.tick),bits(expected.frac),std::uint64_t(j)});corrupt=actual;corrupt.frac=std::bit_cast<double>(bits(corrupt.frac)^1);expect("inverse-fraction",[&]{inverseGate(corrupt,expected,std::uint64_t(j));},"EXACT_INVERSE_NATIVE_BITS",{std::uint64_t(corrupt.whole),bits(corrupt.frac),std::uint64_t(expected.tick),bits(expected.frac),std::uint64_t(j)});inverseControl=true;}}
     else{actual=generator.base.inverse(frame);gate(std::abs(generator.base.frame(actual)-frame)<=1.0/1024,"INVERSE_FORWARD_RESIDUAL");inv.word(std::bit_cast<std::uint64_t>(actual.whole));inv.word(bits(actual.frac));}++inverseRecords;}
    inv.finish(16*std::uint64_t(b.D));}
  }}
 gate(entries==dense&&inverseRecords>0&&inverseRecords<=inverseUpper,"COMPLETE_CLOCK_INVENTORY",{entries,inverseRecords});if(verify)gate(wordControl&&inverseControl&&prefixControl&&rampControl&&metadataControl,"ALL_ACTUAL_NUMERIC_CONTROLS_REACHED");gate(cleanupClose.failures==0,"STREAM_CLEANUP_CLOSE_FAILURE",{cleanupClose.failures});
 std::printf("{\"status\":\"%s\",\"source_manifest_sha256\":\"%s\",\"entries\":%llu,\"inverse_records\":%llu,\"reserved\":%llu,\"used\":%llu,\"prior\":%llu,\"composed_after\":%llu,\"vendor\":false,\"quality\":false,\"Ready\":false,\"compiler_peak_disk_qualified\":false}\n",verify?"CLOCK_VERIFIED_REQUIRES_ROOT_OUTPUT_REVIEW":"CLOCK_PRODUCED_REQUIRES_INDEPENDENT_VERIFY",manifest.c_str(),entries,inverseRecords,meter.reserved,meter.used,meter.prior,meter.prior+meter.reserved);
}
std::uint64_t decimal(const char* p){std::uint64_t value=0;unsigned n=0;for(;*p;++p){gate(++n<=20&&*p>='0'&&*p<='9',"DECIMAL_ARGUMENT");const auto digit=unsigned(*p-'0');gate(value<=(std::numeric_limits<std::uint64_t>::max()-digit)/10,"DECIMAL_OVERFLOW");value=value*10+digit;}gate(n>0,"EMPTY_DECIMAL");return value;}
}
int main(int argc,char** argv){using namespace clock_stage;try{
 gate(sourceReviewAccepted&&sourceWorkAccepted&&executionAccepted&&compilerAccepted,"SOURCE_HOLD_NO_CLOCK_EXECUTION_GO");gate(argc==7,"USAGE_MODE_ROOT_MANIFEST_PRIOR_RUNNER_TOTAL");
 gate(std::fegetround()==FE_TONEAREST&&(_mm_getcsr()&~0x3fu)==0x1f80u,"UnsupportedArithmetic");const bool verify=std::strcmp(argv[1],"verify")==0;gate(verify||std::strcmp(argv[1],"produce")==0,"EXACT_MODE");gate(verify?verifyAccepted:produceAccepted,"SOURCE_HOLD_MODE_NOT_ACCEPTED");const std::string root=argv[2],manifest=argv[3];gate(manifest.size()==64,"MANIFEST_ARGUMENT");for(char c:manifest)gate((c>='0'&&c<='9')||(c>='A'&&c<='F'),"MANIFEST_HEX");
 const auto prior=decimal(argv[4]),runner=decimal(argv[5]),total=decimal(argv[6]);
 Cost::validate();const auto producerUpper=Cost::childUpper(false),verifierUpper=Cost::childUpper(true);const auto& whole=Cost::catalogue().whole();
 gate(whole.runnerUpper==runner&&whole.producerUpper==producerUpper&&whole.verifierUpper==verifierUpper,"UNBOUND_OR_MISMATCHED_WHOLE_CATALOGUE");
 const auto composed=Cost::plus(runner,Cost::plus(producerUpper,verifierUpper));reservationGate(composed,cap);
 gate(whole.totalUpper==composed&&total==composed&&prior==(verify?Cost::plus(runner,producerUpper):runner),"DISJOINT_CHILD_RESERVATION",{prior,runner,total,composed});Meter meter(prior,verify?verifierUpper:producerUpper);run(verify,root,manifest,meter,total);return 0;
 }catch(const Rejection& e){std::fprintf(stderr,"FAIL gate=%s operands=%llu,%llu,%llu,%llu,%llu,%llu\n",e.reason,e.values[0],e.values[1],e.values[2],e.values[3],e.values[4],e.values[5]);reportCleanupClose();return 2;}catch(const std::exception& e){std::fprintf(stderr,"FAIL exception=%s\n",e.what());reportCleanupClose();return 3;}}
