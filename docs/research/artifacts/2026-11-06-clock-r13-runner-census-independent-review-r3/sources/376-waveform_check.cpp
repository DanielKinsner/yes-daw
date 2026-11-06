// Historical R5 measuring core was calibrated. This new driver and vendor candidate are UNRUN.
// Source authoring only; every new execution gate remains HOLD pending separate root review.
#include "fast_fit.h"
#include "engine/Time.h" // accepted-clock bit parity comparator only
#include <bit>
#include <cfenv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numbers>
#include <sstream>
#include <string>
#include <xmmintrin.h>
#if !defined(_MSC_VER) || defined(__clang__)
#error Same reviewed MSVC runtime backend required.
#endif
#pragma fenv_access(on)
#pragma fp_contract(off)
namespace {
constexpr double pi=std::numbers::pi;
constexpr bool coefficientAndTransformProofAccepted=false;
constexpr bool wholeCasePreflightClosed=false;
std::vector<std::string> split(const std::string& line) {
    qc::require(line.size()<=32768,"INPUT_ROW_TOO_LARGE");
    std::vector<std::string> result;
    std::size_t begin=0;
    for (;;) {
        const auto end=line.find('\t',begin);
        result.push_back(line.substr(begin,end==std::string::npos?end:end-begin));
        if (end==std::string::npos) break;
        begin=end+1;
    }
    return result;
}
double number(const std::string& s) {
    std::size_t used=0;
    const double value=std::stod(s,&used);
    qc::require(used==s.size() && std::isfinite(value),"INPUT_NUMBER_DOMAIN");
    return value;
}
std::uint64_t integer(const std::string& s) {
    qc::require(!s.empty() && s[0]!='-',"INPUT_INTEGER_DOMAIN");
    std::size_t used=0;
    const auto value=std::stoull(s,&used);
    qc::require(used==s.size(),"INPUT_INTEGER_DOMAIN");
    return value;
}

struct Row {
    std::string id, group, expected, geometry;
    std::size_t rate=0,start=0,m=0;
    double a1=0,f1=0,p1=0,a2=0,f2=0,p2=0,dc=0,referenceF=0,referenceRms=0;
};
Row parseRow(const std::vector<std::string>& c) {
    qc::require(c.size()==16,"CALIBRATION_COLUMN_COUNT");
    Row r;
    r.id=c[0];r.group=c[1];r.rate=static_cast<std::size_t>(integer(c[2]));
    r.start=static_cast<std::size_t>(integer(c[3]));r.m=static_cast<std::size_t>(integer(c[4]));
    r.a1=number(c[5]);r.f1=number(c[6]);r.p1=number(c[7]);r.a2=number(c[8]);
    r.f2=number(c[9]);r.p2=number(c[10]);r.dc=number(c[11]);r.expected=c[12];
    r.referenceF=number(c[13]);r.referenceRms=number(c[14]);r.geometry=c[15];
    qc::require((r.rate==44100 || r.rate==48000 || r.rate==96000)
        && r.m==r.rate/4 && r.start<=10*r.rate-r.m,"CALIBRATION_FIXTURE_DOMAIN");
    qc::require(r.a1>0.0 && r.a2>=0.0 && r.a1+r.a2+std::abs(r.dc)<=0.25
        && r.f1>0.0 && r.f1<static_cast<double>(r.rate)/2
        && (r.a2==0.0 || (r.f2>0.0 && r.f2<static_cast<double>(r.rate)/2))
        && r.referenceRms>0.0,"CALIBRATION_SOURCE_DOMAIN");
    qc::require(r.expected=="OK" || r.expected=="ESTIMATOR_UNPROVEN",
                "CALIBRATION_EXPECTED_STATUS_DOMAIN");
    return r;
}

std::vector<float> source(const Row& r,qf::Budget& budget) {
    budget.add(static_cast<std::uint64_t>(r.m));
    std::vector<float> samples(r.m);
    for (std::size_t n=0;n<r.m;++n) {
        budget.add(16);
        const double position=static_cast<double>(r.start+n)/static_cast<double>(r.rate);
        budget.library(1);
        double y=r.dc+r.a1*std::sin(2.0*pi*r.f1*position+r.p1);
        if (r.a2!=0.0) {
            budget.add(16); budget.library(1);
            y+=r.a2*std::sin(2.0*pi*r.f2*position+r.p2);
        }
        samples[n]=static_cast<float>(y);
        qc::require(std::isfinite(samples[n]) && std::abs(samples[n])<=0.25f,
                    "ANALYTIC_FLOAT_SOURCE_DOMAIN");
    }
    return samples;
}

bool sameBits(double a,double b) {
    return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);
}
std::uint64_t clockChecks(const std::filesystem::path& path,const std::filesystem::path& output) {
    std::ifstream input(path);qc::require(input.good(),"CLOCK_INPUT_UNAVAILABLE");
    std::ofstream receipt(output);qc::require(receipt.good(),"CLOCK_RECEIPT_UNAVAILABLE");
    receipt<<std::setprecision(17)<<"id\tmode\tstatus\tnormative_cut\tlow\thigh\tanalytic_error_frames\n";
    std::string line;std::getline(input,line);
    qc::require(line=="id\tmode\trate\tmap\twhole\tnum\tden\tframe\treference","CLOCK_HEADER");
    std::uint64_t count=0;
    while (std::getline(input,line)) {
        qc::require(count<4096,"CLOCK_QUERY_WORK_REFUSED");
        const auto c=split(line);qc::require(c.size()==9,"CLOCK_COLUMNS");
        const double rate=number(c[2]);
        const auto rawKind=integer(c[3]);qc::require(rawKind<9,"CLOCK_MAP_FIXTURE_DOMAIN");
        const int kind=static_cast<int>(rawKind);
        const auto changes=qc::fixtureMap(kind);
        qc::NumericAClock own(rate,changes);
        std::vector<yesdaw::engine::TempoChange> comparatorChanges;
        for (const auto& change:changes)
            comparatorChanges.push_back({change.tick,change.bpm,change.ramp?
                yesdaw::engine::TempoCurve::LinearRamp:yesdaw::engine::TempoCurve::Jump});
        yesdaw::engine::CompiledTempoMap comparator;
        qc::require(yesdaw::engine::CompiledTempoMap::build(
            {comparatorChanges.data(),comparatorChanges.size()},{rate},comparator),"CLOCK_COMPARATOR_BUILD");
        double error=0.0;
        if (c[1]=="F") {
            const auto rawWhole=integer(c[4]);
            qc::require(rawWhole<=static_cast<std::uint64_t>(std::numeric_limits<qc::Tick>::max()),
                        "CLOCK_WHOLE_NARROWING_DOMAIN");
            const auto w=static_cast<qc::Tick>(rawWhole);
            const auto num=integer(c[5]),den=integer(c[6]);
            const auto cut=own.cut(w,num,den);
            const auto bounds=qc::fractionBounds(num,den);
            double lo=0.0,hi=0.0;
            qc::require(comparator.frameForMusicalTime({w,bounds.first},lo)==yesdaw::engine::TimeLookupStatus::Ok,
                        "CLOCK_COMPARATOR_FORWARD");
            const yesdaw::engine::MusicalTime upper=bounds.second==1.0?
                yesdaw::engine::MusicalTime{w+1,0.0}:yesdaw::engine::MusicalTime{w,bounds.second};
            qc::require(comparator.frameForMusicalTime(upper,hi)==yesdaw::engine::TimeLookupStatus::Ok
                && sameBits(lo,cut.low) && sameBits(hi,cut.high),"CLOCK_NUMERIC_A_BACKEND_PARITY");
            error=std::max(std::abs(cut.low-number(c[8])),std::abs(cut.high-number(c[8])));
            receipt<<c[0]<<"\tF\t"<<(cut.admitted?"OK":"AMBIGUOUS")<<'\t'<<cut.floor
                   <<'\t'<<cut.low<<'\t'<<cut.high<<'\t'<<error<<'\n';
            qc::require(cut.admitted && error<=1.0/1024.0,"CLOCK_BRACKET_OR_ANALYTIC_CALIBRATION");
        } else {
            qc::require(c[1]=="I","CLOCK_MODE");
            const double frame=number(c[7]);const auto native=own.inverse(frame);
            yesdaw::engine::MusicalTime comparison;
            qc::require(comparator.musicalTimeForFrame(frame,comparison)==yesdaw::engine::TimeLookupStatus::Ok
                && native.whole==comparison.tick && sameBits(native.frac,comparison.frac),
                "CLOCK_INVERSE_BACKEND_PARITY");
            const double roundTrip=std::abs(own.frame(native)-frame);
            const double nativeValue=static_cast<double>(native.whole)+native.frac;
            error=std::abs(nativeValue-number(c[8]))*(60.0*rate/(15360.0*30.0));
            receipt<<c[0]<<"\tI\tOK\t0\t"<<native.whole<<'\t'<<native.frac<<'\t'<<error<<'\n';
            qc::require(roundTrip<=1.0/1024.0 && error<=1.0/1024.0,"CLOCK_INVERSE_ANALYTIC_CALIBRATION");
        }
        ++count;
    }
    return count;
}

void transformChecks() {
    qf::Budget budget;
    for(const std::size_t n:{8u,16u,32u,64u}){
        qf::FFT complexPlan(n,budget),packedPlan(n/2,budget);
        budget.add(4*n);std::vector<std::complex<double>> actual(n),original(n),roots(n/2+1);
        std::vector<float> real(n);
        for(std::size_t j=0;j<n;++j){budget.add(12);const int value=int(j%7)-3;real[j]=float(value)/128;original[j]={double(real[j]),double(int(j%5)-2)/256};}
        for(std::size_t j=0;j<=n/2;++j){budget.add(8);roots[j]=qf::trig(qf::times(qf::piInterval(budget),qf::point(-2*double(j)/double(n),budget),budget),budget).value;}
        actual=original;complexPlan.apply(actual,false,budget);const auto packed=qf::realSpectrum(real,n,packedPlan,roots,budget);
        for(std::size_t k=0;k<n;++k){std::complex<double> reference(0,0),realReference(0,0);
            for(std::size_t j=0;j<n;++j){budget.add(32);budget.library(2);const double phase=-2*pi*double(k)*double(j)/double(n);const std::complex<double> z(std::cos(phase),std::sin(phase));reference+=original[j]*z;realReference+=double(real[j])*z;}
            qc::require(std::abs(reference-actual[k])<=1e-10*double(n),"FFT_DIRECT_DFT_QUALIFICATION_FAILED");
            if(k<=n/2)qc::require(std::abs(realReference-packed[k])<=1e-10*double(n),"PACKED_REAL_DEFINING_SUM_FAILED");
        }
    }
    // Defining-sum qualification of the circular chirp identity at every trial.
    for(const std::size_t m:{7u,11u,32u,64u})for(const std::size_t t:{5u,17u}){
        const auto p=qf::power2(m+t-1,budget);qf::FFT fft(p,budget);budget.add(3*p);
        std::vector<std::complex<double>> a(p,{0,0}),b(p,{0,0});std::vector<double> y(m);
        constexpr double f0=0.137,step=0.0013;
        for(std::size_t n=0;n<m;++n){budget.add(24);budget.library(2);y[n]=double(int(n%7)-3)/128;const double phase=-2*pi*f0*double(n)-pi*step*double(n*n);a[n]=y[n]*std::complex<double>(std::cos(phase),std::sin(phase));}
        for(std::int64_t j=-std::int64_t(m-1);j<std::int64_t(t);++j){budget.add(24);budget.library(2);const double phase=pi*step*double(j*j);b[j<0?std::size_t(std::int64_t(p)+j):std::size_t(j)]={std::cos(phase),std::sin(phase)};}
        fft.apply(a,false,budget);fft.apply(b,false,budget);for(std::size_t j=0;j<p;++j){budget.add(8);a[j]*=b[j];}fft.apply(a,true,budget);
        for(std::size_t j=0;j<t;++j){budget.add(24);budget.library(2);const double phase=-pi*step*double(j*j);const auto actual=a[j]*std::complex<double>(std::cos(phase),std::sin(phase));std::complex<double> reference(0,0);
            for(std::size_t n=0;n<m;++n){budget.add(24);budget.library(2);const double theta=-2*pi*(f0+step*double(j))*double(n);reference+=y[n]*std::complex<double>(std::cos(theta),std::sin(theta));}
            qc::require(std::abs(actual-reference)<=1e-10*double(m),"CHIRP_Z_DEFINING_SUM_FAILED");
        }
    }
}
void allTrialChecks(){
    // One independent component-control budget. This does not donate work to
    // a waveform case. Every trial is directly evaluated, not only finalists.
    qf::Budget budget;qf::Grid grid(48000,budget);const auto geometry=qf::makeGeometry(301,grid,budget);
    for(unsigned kind=0;kind<7;++kind){budget.add(grid.m);std::vector<float> y(grid.m);
        double frequency=220,dc=0,second=0,phase=0;
        if(kind==1)dc=0.01;if(kind==2)second=0.025;if(kind==3)frequency=(geometry.basis[50].frequency+geometry.basis[51].frequency)/2;
        if(kind==4)frequency=geometry.basis.front().frequency;if(kind==5)frequency=geometry.basis.back().frequency;if(kind==6)phase=pi;
        for(std::size_t n=0;n<grid.m;++n){budget.add(32);budget.library(second?2:1);const double x=double(n)/double(grid.rate);y[n]=float(dc+0.2*std::sin(2*pi*frequency*x+phase)+(second?second*std::sin(2*pi*997*x+pi/7):0));}
        qf::Seed seed;seed.bin=301;seed.measurement.status="RANK_REQUIRED";seed.stats=qf::signalStats(y,grid.sumBounds,budget);
        const auto corr=grid.correlations(y,geometry.lo,geometry.hi,budget);const auto rank=qf::rankWindow(y,seed,geometry,grid,budget);
        double best=std::numeric_limits<double>::infinity();std::size_t first=0;
        for(std::size_t i=0;i<grid.t;++i){budget.add(128);const auto& basis=geometry.basis[i];auto z=basis.initial;qf::Sum bc,bs,gcc,gss,gc,gcs,gs;
            for(float value:y){budget.add(72);bc.add(double(value)*z.real());bs.add(double(value)*z.imag());gcc.add(z.real()*z.real());gss.add(z.imag()*z.imag());gc.add(z.real());gcs.add(z.real()*z.imag());gs.add(z.imag());z=qf::cmul(z,basis.step);}
            const double B=qf::addU(1,basis.epsilon,budget),rhsTerm=qf::mulU(seed.stats.maxAmplitude,B,budget),gramTerm=qf::mulU(B,B,budget);
            const double rhsRound=qf::addU(grid.sumBounds.bound(rhsTerm,budget),qf::mulU(double(grid.m),qf::addU(qf::mulU(qf::u,rhsTerm,budget),std::numeric_limits<double>::denorm_min(),budget),budget),budget);
            const double rhsBound=qf::addU(qf::addU(corr[i].error,qf::mulU(basis.epsilon,seed.stats.absoluteSumUpper,budget),budget),rhsRound,budget);
            qc::require(std::abs(bc.value-corr[i].c)<=rhsBound&&std::abs(bs.value-corr[i].s)<=rhsBound,"ALL_TRIAL_CORRELATION_ENCLOSURE_FAILED");
            const double gramRound=qf::addU(grid.sumBounds.bound(gramTerm,budget),qf::mulU(double(grid.m),qf::addU(qf::mulU(qf::u,gramTerm,budget),std::numeric_limits<double>::denorm_min(),budget),budget),budget);
            const double gramDelta=qf::mulU(double(grid.m),qf::addU(2*basis.epsilon,qf::mulU(basis.epsilon,basis.epsilon,budget),budget),budget);
            const auto cc=qf::expand(basis.cc,qf::addU(gramDelta,gramRound,budget),budget),ss=qf::expand(basis.ss,qf::addU(gramDelta,gramRound,budget),budget);
            const auto cs=qf::expand(basis.c,qf::addU(qf::mulU(double(grid.m),basis.epsilon,budget),grid.sumBounds.bound(B,budget),budget),budget);
            const double crossBound=qf::addU(gramDelta,gramRound,budget),sinBound=qf::addU(qf::mulU(double(grid.m),basis.epsilon,budget),grid.sumBounds.bound(B,budget),budget);
            qc::require(cc.lo<=gcc.value&&gcc.value<=cc.hi&&ss.lo<=gss.value&&gss.value<=ss.hi&&cs.lo<=gc.value&&gc.value<=cs.hi&&std::abs(gcs.value)<=crossBound&&std::abs(gs.value)<=sinBound,"ALL_TRIAL_GRAM_ENCLOSURE_FAILED");
            const double value=qf::directRss(y,basis,rank.trials[i],budget);if(value<best){best=value;first=i;}
        }
        qc::require(std::find(rank.finalists.begin(),rank.finalists.end(),first)!=rank.finalists.end(),"TRUE_MINIMUM_PRUNED");
        const auto selected=qf::finishWindow(y,seed,geometry,rank,budget);qc::require(selected.winnerIndex==first&&selected.rss==best,"ALL_TRIAL_FIRST_MINIMUM_FAILED");
        if(kind==4)qc::require(first==0&&selected.status=="ESTIMATOR_UNPROVEN","LOW_BAND_EDGE_CONTROL_UNREACHED");
        if(kind==5)qc::require(first+1==grid.t&&selected.status=="ESTIMATOR_UNPROVEN","HIGH_BAND_EDGE_CONTROL_UNREACHED");
    }
    // Equal computed objectives with two actual direct evaluations exercise
    // comparator tie behavior independently of naturally near-tied grid rows.
    auto duplicate=geometry;duplicate.basis.resize(2);duplicate.basis[1]=duplicate.basis[0];
    budget.add(grid.m);std::vector<float> y(grid.m,0.125f);qf::Seed seed;seed.bin=301;seed.measurement.status="RANK_REQUIRED";
    qf::Trial t{{0,0,0.125},{-1,1},0};qf::Ranked rank{{t,t},{0,1}};
    const auto tie=qf::finishWindow(y,seed,duplicate,rank,budget);qc::require(tie.winnerIndex==0&&tie.rss==0,"DIRECT_EQUAL_RSS_FIRST_MINIMUM_FAILED");
}
struct WholeTotals {std::uint64_t cases=0,windows=0,maxCharged=0,maxPayload=0;};
WholeTotals wholeCases(const std::filesystem::path& definitions,const std::filesystem::path& windows,
                const std::filesystem::path& output){
    std::ifstream config(definitions),reference(windows);qc::require(config.good()&&reference.good(),"WHOLE_CASE_INPUT_UNAVAILABLE");
    std::ofstream receipt(output/"whole-results.tsv");qc::require(receipt.good(),"WHOLE_CASE_RECEIPT_UNAVAILABLE");
    receipt<<std::setprecision(17)<<"case\tchannel\tstart\tfrequency\trms\tcharged_work_so_far\tlibrary_calls_so_far\tstatus\n";
    std::string header;std::getline(config,header);qc::require(header=="case\trate\tchannels\tframes\twindows_per_channel","WHOLE_CASE_HEADER");
    std::getline(reference,header);qc::require(header=="case\tchannel\tstart\tfrequency\tphase\trms","WHOLE_WINDOW_HEADER");
    std::string line;std::uint64_t cases=0,windowCount=0,maxCharged=0,maxPayload=0;
    while(std::getline(config,line)){
        const auto c=split(line);qc::require(c.size()==5,"WHOLE_CASE_COLUMNS");const auto rate=std::size_t(integer(c[1])),channels=std::size_t(integer(c[2])),frames=std::size_t(integer(c[3]));
        qc::require((rate==44100||rate==48000||rate==96000)&&(channels==1||channels==2)&&frames==20*rate&&integer(c[4])==396,"WHOLE_CASE_DOMAIN");
        const std::size_t count=396*channels;
        qf::Budget budget;budget.add(1u<<20); // bounded case text/metadata/retirement allowance
        // Independent complete source-cost review accepted these upper ledgers.
        // Storage-profile assertions below enforce its two conditions; calibration is UNRUN.
        constexpr std::uint64_t setupUpper=400000000,seedWindowUpper=10500000,
            rankWindowUpper=9000000,geometryUpper=2000000;
        budget.add(setupUpper);qf::Budget setup(setupUpper);
        auto grid=std::make_unique<qf::Grid>(rate,setup);qc::require(setup.work<=setupUpper,"SETUP_PHASE_UPPER_FAILED");
        budget.calls+=setup.calls;
        // Reserve ALL actual-source grid/cache/ranking rows before allocation.
        // Capacity derives from the complete case inventory, never a fixed2-grid assumption.
        const auto cacheBytes=std::uint64_t(count)*grid->t*(128+64+sizeof(std::size_t));
        const auto payload=4*std::uint64_t(channels)*frames+cacheBytes+grid->payload()+64*grid->k+32*grid->p+(2u<<20);
        qc::require(payload<=qf::scratchCap,"WHOLE_CASE_SCRATCH_REFUSED");
        budget.add(channels*frames);std::vector<float> data(channels*frames);
        for(std::size_t ch=0;ch<channels;++ch)for(std::size_t n=0;n<frames;++n){budget.add(24);budget.library(1);const double f=ch?997:220,phase=ch?pi/7:0;data[ch*frames+n]=float(0.2*std::sin(2*pi*f*double(n)/double(rate)+phase));}
        struct Window {std::size_t channel,start,geometry=0;qf::Seed seed;qf::Ranked rank;};
        static_assert(sizeof(Window)<=512);
        budget.add(count);std::vector<Window> all;all.reserve(count);
        budget.add(seedWindowUpper*count);qf::Budget seedPhase(seedWindowUpper*count);
        for(std::size_t ch=0;ch<channels;++ch)for(std::size_t start=0;start+grid->m<=frames;start+=rate/20){
            seedPhase.add(128);const auto y=std::span<const float>(data.data()+ch*frames+start,grid->m);
            auto seed=qf::seedWindow(y,*grid,seedPhase);qc::require(seed.measurement.status=="RANK_REQUIRED","WHOLE_CASE_ESTIMATOR_UNPROVEN");
            seedPhase.add(16);const double knownFrequency=ch?997:220;qc::require(std::abs(seed.measurement.seed-knownFrequency)<=2*double(rate)/double(grid->k),"WHOLE_CASE_ANALYTIC_SEED_CONTAINMENT");
            all.push_back({ch,start,0,std::move(seed),{}});
        }
        qc::require(all.size()==count&&seedPhase.work<=seedWindowUpper*count,"SEED_PHASE_UPPER_FAILED");budget.calls+=seedPhase.calls;
        std::vector<std::size_t> actualBins;budget.add(count);actualBins.reserve(count);
        for(const auto& w:all){budget.add(actualBins.size()+8);if(std::find(actualBins.begin(),actualBins.end(),w.seed.bin)==actualBins.end())actualBins.push_back(w.seed.bin);}
        const std::uint64_t rankUpper=rankWindowUpper*count+geometryUpper*actualBins.size();
        budget.add(rankUpper);qf::Budget rankPhase(rankUpper);rankPhase.add(actualBins.size());std::vector<qf::Geometry> geometry;geometry.reserve(actualBins.size());
        for(std::size_t bin:actualBins){rankPhase.add(128);geometry.push_back(qf::makeGeometry(bin,*grid,rankPhase));}
        std::uint64_t finalistUpper=0;
        for(auto& w:all){rankPhase.add(geometry.size()+128);w.geometry=std::size_t(std::find(actualBins.begin(),actualBins.end(),w.seed.bin)-actualBins.begin());
            const auto y=std::span<const float>(data.data()+w.channel*frames+w.start,grid->m);w.rank=qf::rankWindow(y,w.seed,geometry[w.geometry],*grid,rankPhase);
            const auto add=qf::directUpper(grid->m,w.rank.finalists.size(),rankPhase);qc::require(add<=qf::workCap-finalistUpper,"CHECKER_WORK_REFUSED_WHOLE_CASE");finalistUpper+=add;
        }
        qc::require(rankPhase.work<=rankUpper,"RANK_PHASE_UPPER_FAILED");budget.calls+=rankPhase.calls;
        // Total finalists for every window and BOTH channels are known first.
        // Reserve their complete pass before evaluating even the first direct sample.
        const std::uint64_t directPhaseUpper=finalistUpper+4096*count;budget.add(directPhaseUpper);qf::Budget directPhase(directPhaseUpper);
        for(const auto& w:all){directPhase.add(4096);qc::require(bool(std::getline(reference,line)),"WHOLE_WINDOW_MISSING");const auto v=split(line);qc::require(v.size()==6&&v[0]==c[0]&&integer(v[1])==w.channel&&integer(v[2])==w.start,"WHOLE_WINDOW_IDENTITY");
            const auto y=std::span<const float>(data.data()+w.channel*frames+w.start,grid->m);
            const auto m=qf::finishWindow(y,w.seed,geometry[w.geometry],w.rank,directPhase);directPhase.library(2);
            const double cents=std::abs(1200*std::log2(m.frequency/number(v[3]))),db=std::abs(20*std::log10(m.rms/number(v[5])));
            const bool pass=m.status=="OK"&&cents<=0.25&&db<=0.01;
            receipt<<c[0]<<'\t'<<w.channel<<'\t'<<w.start<<'\t'<<m.frequency<<'\t'<<m.rms<<'\t'<<budget.work<<'\t'<<(budget.calls+directPhase.calls)<<'\t'<<(pass?"PASS":"FAIL")<<'\n';receipt.flush();qc::require(pass,"WHOLE_CASE_ANALYTIC_FAILED");++windowCount;
        }
        qc::require(directPhase.work<=directPhaseUpper,"DIRECT_PHASE_UPPER_FAILED");maxCharged=std::max(maxCharged,budget.work);maxPayload=std::max(maxPayload,payload);++cases;
    }
    qc::require(cases==6&&windowCount==3564&&!std::getline(reference,line),"WHOLE_CASE_COMPLETE_INVENTORY");return {cases,windowCount,maxCharged,maxPayload};
}
} // namespace
int waveform_analytic_main(int argc,char** argv){
    try{
        qc::require(argc==8,"USAGE: calibration clock whole-cases whole-windows new-output rows clockrows");
        qc::require(coefficientAndTransformProofAccepted&&wholeCasePreflightClosed,"SOURCE_READINESS_NUMERIC_AND_PHASE_UPPER_LEDGERS_REQUIRED");
        qc::require(std::fegetround()==FE_TONEAREST&&(_mm_getcsr()&~0x3fu)==0x1f80u,"UNSUPPORTED_ARITHMETIC_MODE");
        const std::filesystem::path output(argv[5]);qc::require(!std::filesystem::exists(output),"OUTPUT_ALREADY_EXISTS");std::filesystem::create_directory(output);
        transformChecks();allTrialChecks();const auto clocks=clockChecks(argv[2],output/"clock-results.tsv");qc::require(clocks==integer(argv[7]),"CLOCK_QUERY_COUNT_MISMATCH");
        std::ifstream input(argv[1]);std::ofstream receipt(output/"calibration-results.tsv");qc::require(input.good()&&receipt.good(),"CALIBRATION_STREAM_UNAVAILABLE");
        receipt<<std::setprecision(17)<<"id\texpected\tactual\tseed_hz\tdominance_power\tfrequency_hz\tcents_error\trms\trms_error_db\ttrials\twork\tlibrary_calls\tscratch_payload\tstatus\n";
        std::string line;std::getline(input,line);std::uint64_t rows=0,refusals=0,totalWork=0,totalCalls=0,maxScratch=0;std::unique_ptr<qf::Grid> grid;std::uint64_t setupWork=0,setupCalls=0;
        while(std::getline(input,line)){
            const Row row=parseRow(split(line));
            if(!grid||grid->rate!=row.rate){qf::Budget setup;grid=std::make_unique<qf::Grid>(row.rate,setup);setupWork=setup.work;setupCalls=setup.calls;}
            qf::Budget budget;budget.add(setupWork);budget.calls=setupCalls;
            const auto samples=source(row,budget);const auto m=qf::measure(samples,*grid,budget);budget.library(1);const double db=std::abs(20*std::log10(m.rms/row.referenceRms));
            double cents=0;if(row.expected=="OK"){budget.library(1);cents=m.status=="OK"?std::abs(1200*std::log2(m.frequency/row.referenceF)):std::numeric_limits<double>::max();}
            const bool pass=m.status==row.expected&&db<=0.01&&(row.expected!="OK"||(std::abs(m.seed-row.referenceF)<=2*double(row.rate)/double(grid->k)&&cents<=0.25));
            receipt<<row.id<<'\t'<<row.expected<<'\t'<<m.status<<'\t'<<m.seed<<'\t'<<m.dominance<<'\t'<<m.frequency<<'\t'<<cents<<'\t'<<m.rms<<'\t'<<db<<'\t'<<m.trials<<'\t'<<budget.work<<'\t'<<budget.calls<<'\t'<<m.scratch<<'\t'<<(pass?"PASS":"FAIL")<<'\n';receipt.flush();qc::require(pass,"ANALYTIC_CALIBRATION_ROW_FAILED");++rows;refusals+=m.status=="ESTIMATOR_UNPROVEN";totalWork+=budget.work;totalCalls+=budget.calls;maxScratch=std::max(maxScratch,m.scratch);
        }
        qc::require(rows==1125&&rows==integer(argv[6])&&refusals==45,"CALIBRATION_ROW_COUNT_MISMATCH");grid.reset();const auto whole=wholeCases(argv[3],argv[4],output);
        std::ofstream summary(output/"summary.json");qc::require(summary.good(),"SUMMARY_OUTPUT_UNAVAILABLE");
        summary<<"{\"status\":\"INSTRUMENT_CALIBRATED_ANALYTIC_FIXTURES_ONLY\",\"rows\":"<<rows
            <<",\"expected_equal_mixture_refusals\":"<<refusals<<",\"clock_queries\":"<<clocks
            <<",\"tone_charged_source_units_total\":"<<totalWork<<",\"tone_elementary_calls_charged_total\":"<<totalCalls
            <<",\"maximum_checker_payload_bytes\":"<<maxScratch<<",\"whole_cases\":"<<whole.cases
            <<",\"whole_window_rows\":"<<whole.windows<<",\"maximum_whole_case_charged_source_units\":"<<whole.maxCharged
            <<",\"maximum_whole_case_payload_bytes\":"<<whole.maxPayload
            <<",\"whole_phase_units_are_reserved_uppers\":true,\"vendor_candidate_invoked\":false,\"production_admission\":false,\"source_error_premise_measured\":false}\n";
        qc::require(summary.good(),"SUMMARY_WRITE_FAILED");std::cout<<"PASS: analytic instrument only; no vendor or renderer admission\n";return 0;

    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 2;}
}

#ifdef WAVEFORM_FAULT_DEFINE
#error The independent checker object must never receive fault defines.
#endif
namespace {constexpr bool waveformDriverSourceReviewed=false,waveformDriverFullCostClosed=false;}
extern "C" int waveform_source_gate(const float* data,std::size_t frames,int logical,int reportedPhysical,const std::uint16_t* projection,std::size_t count){
 if(!data||frames==0||(logical!=1&&logical!=2)||count!=std::size_t(logical)||!projection)return 3;
 bool equal=true;for(std::size_t i=0;i<frames;++i){const auto a=std::bit_cast<std::uint32_t>(data[i*std::size_t(logical)]);if((a&0x7f800000u)==0x7f800000u)return 1;
  if(logical==2){const auto b=std::bit_cast<std::uint32_t>(data[i*2+1]);if((b&0x7f800000u)==0x7f800000u)return 1;if(a!=b&&((a&0x7fffffffu)!=0||(b&0x7fffffffu)!=0))equal=false;}}
 const int expected=logical==1||equal?1:2;if(reportedPhysical!=expected||projection[0]!=0||(logical==2&&projection[1]!=std::uint16_t(expected==1?0:1)))return 2;return 0;
}
extern "C" int waveform_storage_gate(const float* physical,const float* logical,std::size_t D,int Cp,int Cl){
 if(!physical||!logical||D==0||Cp<1||Cp>2||Cl<1||Cl>2||Cp>Cl)return 3;
 for(std::size_t j=0;j<D;++j)for(int c=0;c<Cl;++c){const auto expected=std::bit_cast<std::uint32_t>(physical[j*std::size_t(Cp)+std::size_t(Cp==1?0:c)]);const auto actual=std::bit_cast<std::uint32_t>(logical[j*std::size_t(Cl)+std::size_t(c)]);if(expected!=actual)return 1;}return 0;
}
extern "C" int waveform_extraction_gate(const float* channel0,const float* channel1,const float* actual,std::size_t D,int C,std::size_t rawCount,int OL,int usedOffset){
 if(!channel0||!actual||D==0||C<1||C>2||(C==2&&!channel1)||OL<0||usedOffset<0||std::size_t(OL)+D>rawCount||std::size_t(usedOffset)+D>rawCount)return 3;
 bool changed=false;const std::array<const float*,2> raw{channel0,channel1};for(std::size_t j=0;j<D;++j)for(int c=0;c<C;++c){const auto observed=std::bit_cast<std::uint32_t>(actual[j*std::size_t(C)+std::size_t(c)]),selected=std::bit_cast<std::uint32_t>(raw[std::size_t(c)][j+std::size_t(usedOffset)]),expected=std::bit_cast<std::uint32_t>(raw[std::size_t(c)][j+std::size_t(OL)]);if(observed!=selected)return 3;changed=changed||observed!=expected;}
 if(usedOffset!=OL&&changed)return 1;return usedOffset==OL&&!changed?0:2;
}
extern "C" int waveform_case_check(const float* input,std::size_t N,const float* output,std::size_t D,int Cl,
 std::size_t R,std::int64_t edgeLow,std::int64_t edgeHigh,const double* expectedF,bool zero,bool equivalent,const std::int64_t* burstCenters,bool burst,const char* receiptPath){try{
 qc::require(waveformDriverSourceReviewed&&waveformDriverFullCostClosed,"SOURCE_HOLD_NEW_DRIVER_AND_ALL_HELPER_COSTS");
 qc::require(std::fegetround()==FE_TONEAREST&&(_mm_getcsr()&~0x3fu)==0x1f80u,"UnsupportedArithmetic");
 qc::require(input&&output&&expectedF&&Cl>=1&&Cl<=2&&N==10*R&&D<=20*R,"CASE_GEOMETRY");
 qf::Budget parent;parent.add(128000000);qf::Budget auxiliary(128000000);
 double peak=0;std::array<qf::Sum,2> sourceEnergy;
 for(std::size_t j=0;j<N;++j)for(int c=0;c<Cl;++c){auxiliary.add(8);const float x=input[j*std::size_t(Cl)+std::size_t(c)];qc::require(std::isfinite(x),"SOURCE_FINITE");peak=std::max(peak,std::abs(double(x)));sourceEnergy[std::size_t(c)].add(double(x)*x);}
 qc::require(peak<=0.25,"INPUT_PEAK");
 for(std::size_t j=0;j<D;++j)for(int c=0;c<Cl;++c){auxiliary.add(16);const float x=output[j*std::size_t(Cl)+std::size_t(c)];qc::require(std::isfinite(x)&&std::abs(double(x))<=1.25*peak+1e-6,"OUTPUT_FINITE_PEAK");if(zero)qc::require(x==0,"EXACT_ZERO");if(equivalent&&c==1)qc::require(x==output[j*2],"NUMERIC_EQUIVALENT_STEREO");}
 std::ofstream receipt(receiptPath,std::ios::binary);qc::require(bool(receipt),"CASE_RECEIPT_OPEN");const auto record=[&](std::array<std::uint64_t,8> fields){for(auto value:fields)for(unsigned j=0;j<8;++j)receipt.put(char((value>>(8*j))&255));qc::require(bool(receipt),"FIXED_RECORD_WRITE");};const auto result=[&](const char* status,std::size_t windows,std::size_t grids,std::uint64_t work,std::uint64_t scratch){std::ofstream meta(std::filesystem::path(receiptPath).parent_path()/(std::filesystem::path(receiptPath).filename()=="witness.bin"?"receipt.json":"case.json"));qc::require(bool(meta),"CASE_METADATA_OPEN");meta<<"{\"status\":\""<<status<<"\",\"windows\":"<<windows<<",\"grids\":"<<grids<<",\"work\":"<<work<<",\"scratch\":"<<scratch<<",\"record_bytes\":64}\n";qc::require(bool(meta),"CASE_METADATA_WRITE");};
 if(zero){result("ZERO_PASS",0,0,parent.work,0);return 0;}
 if(burst){qc::require(burstCenters,"BURST_CENTERS_REQUIRED");const auto W=std::int64_t(double(float(R))*0.12);for(int c=0;c<Cl;++c)for(int index=0;index<4;++index){const auto e=burstCenters[index],lo=e-2*W,hi=e+2*W;qc::require(lo>=0&&hi<std::int64_t(D),"BURST_WHOLE_REGION");qf::Sum energy,weighted;for(auto j=lo;j<=hi;++j){auxiliary.add(24);const double x=output[std::size_t(j)*std::size_t(Cl)+std::size_t(c)],power=x*x;energy.add(power);weighted.add(double(j)*power);}qc::require(energy.value>0&&std::isfinite(weighted.value),"BURST_POSITIVE_ENERGY");const double centroid=weighted.value/energy.value;record({std::uint64_t(c),std::uint64_t(e),std::bit_cast<std::uint64_t>(centroid),std::bit_cast<std::uint64_t>(energy.value),std::uint64_t(index),0,0,0});qc::require(std::abs(centroid-double(e))/double(R)<=0.005,"BURST_CENTROID_OWN_GATE");}result("BURST_PASS",8,0,parent.work,0);return 0;}

 parent.add(400000000);qf::Budget setup(400000000);qf::Grid grid(R,setup);
 struct Window {std::size_t channel,start,geometry=0;qf::Seed seed;qf::Ranked ranked;};static_assert(sizeof(Window)<=512);static_assert(sizeof(qf::Geometry)<=64);
 std::vector<std::size_t> starts;for(std::size_t j=0;j+R/4<=D;j+=R/20)if(std::int64_t(j)>=edgeLow&&std::int64_t(j+R/4)<=edgeHigh)starts.push_back(j);
 qc::require(starts.size()>=80&&starts.size()<=396,"CASE_ALL_WINDOW_INVENTORY");const auto w=starts.size()*std::size_t(Cl);
 const auto scratch=4*std::uint64_t(Cl)*D+std::uint64_t(w)*grid.t*(128+64+sizeof(std::size_t))+grid.payload()+64*grid.k+32*grid.p+(3ull<<20);
 qc::require(scratch<=qf::scratchCap,"CHECKER_SCRATCH_REFUSED");parent.add(26*std::uint64_t(Cl)*D);std::vector<float> planar(std::size_t(Cl)*D);
 for(std::size_t j=0;j<D;++j)for(int c=0;c<Cl;++c)planar[std::size_t(c)*D+j]=output[j*std::size_t(Cl)+std::size_t(c)];
 parent.add(10500000*w);qf::Budget seeds(10500000*w);std::vector<Window> all;all.reserve(w);
 for(int c=0;c<Cl;++c)for(auto j:starts){seeds.add(128);const auto y=std::span<const float>(planar.data()+std::size_t(c)*D+j,grid.m);auto seed=qf::seedWindow(y,grid,seeds);qc::require(seed.measurement.status=="RANK_REQUIRED","ESTIMATOR_UNPROVEN");all.push_back({std::size_t(c),j,0,std::move(seed),{}});}
 std::vector<std::size_t> bins;bins.reserve(w);for(const auto& x:all){auxiliary.add(bins.size()+8);if(std::find(bins.begin(),bins.end(),x.seed.bin)==bins.end())bins.push_back(x.seed.bin);}
 const auto rankUpper=9000000*w+2000000*bins.size();parent.add(rankUpper);qf::Budget ranks(rankUpper);std::vector<qf::Geometry> geometries;geometries.reserve(bins.size());for(auto b:bins)geometries.push_back(qf::makeGeometry(b,grid,ranks));
 std::uint64_t finalistUpper=0;for(auto& x:all){ranks.add(128+bins.size());x.geometry=std::size_t(std::find(bins.begin(),bins.end(),x.seed.bin)-bins.begin());const auto y=std::span<const float>(planar.data()+x.channel*D+x.start,grid.m);x.ranked=qf::rankWindow(y,x.seed,geometries[x.geometry],grid,ranks);const auto n=qf::directUpper(grid.m,x.ranked.finalists.size(),ranks);qc::require(n<=qf::workCap-finalistUpper,"CHECKER_WORK_REFUSED");finalistUpper+=n;}
 const auto directUpper=finalistUpper+4096*w;parent.add(directUpper);qf::Budget direct(directUpper);
 for(const auto& x:all){direct.add(4096);const auto y=std::span<const float>(planar.data()+x.channel*D+x.start,grid.m);const auto m=qf::finishWindow(y,x.seed,geometries[x.geometry],x.ranked,direct);auxiliary.library(3);const double rms=std::sqrt(sourceEnergy[x.channel].value/double(N)),cents=std::abs(1200*std::log2(m.frequency/expectedF[x.channel])),db=std::abs(20*std::log10(m.rms/rms));record({x.channel,x.start,std::bit_cast<std::uint64_t>(m.frequency),std::bit_cast<std::uint64_t>(cents),std::bit_cast<std::uint64_t>(db),x.seed.bin,x.ranked.finalists.size(),std::bit_cast<std::uint64_t>(m.rss)});qc::require(m.status=="OK","ESTIMATOR_UNPROVEN");qc::require(cents<=5,"PITCH_5CENT_OWN_GATE");qc::require(db<=1,"RMS_1DB_OWN_GATE");}
 result("CASE_PASS",all.size(),bins.size(),parent.work,scratch);return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 2;}}

extern "C" int waveform_fade_check(const float* unfaded,const float* actual,std::size_t D,int Cp,int Cl,
 const char* inversePath,std::int64_t S,std::int64_t L,std::int64_t fi,std::int64_t fo,int shape,double curve){try{
 qc::require(waveformDriverSourceReviewed&&waveformDriverFullCostClosed,"SOURCE_HOLD_FADE_INVERSE_HASH_AND_HELPER_COST");qc::require(unfaded&&actual&&D>0&&Cp>=1&&Cp<=Cl&&Cl<=2&&shape>=0&&shape<4&&std::isfinite(curve),"FADE_INPUT");
 std::ifstream reference(inversePath,std::ios::binary);qc::require(bool(reference),"FROZEN_INVERSE_REQUIRED");qf::Budget budget;
 const auto word=[&](){std::uint64_t v=0;for(unsigned j=0;j<8;++j){const int x=reference.get();qc::require(x!=EOF,"INVERSE_TRUNCATED");v|=std::uint64_t(static_cast<unsigned char>(x))<<(8*j);}return v;};
 const auto shapeValue=[&](double z){z=std::clamp(z,0.0,1.0);const double h=curve==0?z:std::pow(z,std::exp2(-2*std::clamp(curve,-1.0,1.0)));switch(shape){case 0:return h;case 1:return std::sin(std::numbers::pi*h/2);case 2:return h*h*(3-2*h);default:return (1-std::exp(-4*h))/(1-std::exp(-4.0));}};
 for(std::size_t j=0;j<D;++j){budget.add(4096);const auto tick=std::bit_cast<std::int64_t>(word());const double fraction=std::bit_cast<double>(word());qc::require(std::isfinite(fraction)&&fraction>=0&&fraction<1,"INVERSE_FRACTION");double u=double(tick-S)+fraction;if(u<0){qc::require(j==0,"FADE_LEADING_ONLY");u=0;}qc::require(u>=0&&u<double(L),"FADE_NATIVE_DOMAIN");const double start=std::max(0.0,double(L-std::min(L,fo))),a=fi<=0||u>=double(fi)?1:shapeValue(u/double(fi)),b=fo<=0||u<start?1:shapeValue(1-(u-start)/double(fo)),e=std::min(a,b);qc::require(e>=0&&e<=1,"FADE_RANGE");for(int c=0;c<Cl;++c){const double raw=unfaded[j*std::size_t(Cp)+std::size_t(Cp==1?0:c)],wanted=raw*e,observed=actual[j*std::size_t(Cl)+std::size_t(c)];qc::require(std::isfinite(observed)&&std::abs(observed-wanted)<=2e-6*std::max(1.0,std::abs(raw)),"ONCE_ONLY_FADE_GATE");}}
 qc::require(reference.get()==EOF,"INVERSE_EXTRA");return 0;
}catch(const std::exception&e){std::cerr<<"FADE_FAIL: "<<e.what()<<'\n';return 2;}}
