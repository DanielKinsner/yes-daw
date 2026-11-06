#pragma once
// Layout-only replicas. No replica is constructed, invoked, or linked.
// Member/base/access/special-member correspondence is recorded separately.
namespace layout_purpose {
using Family=clock_stage::source_costs::Family;
struct Rejection:std::exception {
 const char* reason;std::array<std::uint64_t,6> values{};
 Rejection(const char* r,std::array<std::uint64_t,6> v):reason(r),values(v){}
 const char* what()const noexcept override{return reason;}
};
struct CleanupCloseState{std::uint64_t failures=0;int firstReturn=0,firstErrno=0;const char* firstSite="none";};
struct Cost {
 static const clock_stage::source_costs::Catalogue& catalogue();
 static std::uint64_t price(Family id);
 static void validate();
 static std::uint64_t plus(std::uint64_t a,std::uint64_t b);
 static std::uint64_t times(std::uint64_t count,std::uint64_t value);
 static std::uint64_t units(Family id,std::uint64_t count);
 static std::uint64_t wordUnits(bool verify,std::uint64_t words);
 static std::uint64_t inverseUnits(bool verify,std::uint64_t records);
 static std::uint64_t fixtureUnits(bool verify,std::uint64_t N,std::uint64_t g,int map);
 static std::uint64_t childUpper(bool verify);
};
struct Meter {
 std::uint64_t used=0,reserved,prior;
 Meter(std::uint64_t p,std::uint64_t r);
 void add(std::uint64_t n);
};
struct Text {
 std::array<char,65536> data{};std::size_t size=0;
 void add(const char* s);
 void u(std::uint64_t x);
 void i(std::int64_t x);
 void boolean(bool x);
 void quoted(const std::string& s);
};
struct Stream {
 FILE* file=nullptr;std::array<unsigned char,65536> buffer{};std::size_t at=0,size=0;
 std::uint64_t bytes=0,maximum;bool write;
 Stream(const std::string& name,bool w,std::uint64_t m);
 ~Stream()noexcept;
 Stream(const Stream&)=delete;Stream& operator=(const Stream&)=delete;
 void flush();
 void put(const unsigned char* p,std::size_t n);
 void word(std::uint64_t x);
 void exact(unsigned char* p,std::size_t n);
 std::uint64_t word();
 void finish(std::uint64_t expected);
};
struct Pair128{std::uint64_t high,low;};
struct FractionCache {
 std::array<std::pair<double,double>,735> rows{};std::uint64_t N,g,count;
 FractionCache(std::uint64_t n,bool independent);
};
struct CachedPort {
private:
 std::size_t segmentIndex_=0;unsigned cursorAdvances_=0;
public:
 std::vector<qc::Change> changes;qc::NumericAClock base;double rate;
 qc::Tick saved=-1;double wholeFrame=0,k=0,bpm=0,slope=0;Meter& meter;std::uint64_t misses=0;
 CachedPort(double r,int map,Meter& m);
 double frame(qc::Native t);
};
struct Picks {
 struct Item{std::uint64_t index;unsigned slot;};std::array<Item,10> items{};std::array<std::int64_t,10> values{};unsigned next=0;
 Picks(std::uint64_t R,std::uint64_t N,std::uint64_t W);
 void see(std::uint64_t i,std::int64_t q);
};
struct Bounds{std::uint64_t R,N,W;int map;qc::Cut begin,end;std::int64_t D;};
} // namespace layout_purpose
