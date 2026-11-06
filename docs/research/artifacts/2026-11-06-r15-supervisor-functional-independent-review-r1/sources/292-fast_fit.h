#pragma once
// R5 source readiness promoted after independent numeric/cost review; calibration UNRUN.
// Fixed-point coefficient objective unchanged; no renderer/vendor API.
#pragma fenv_access(on)
#pragma fp_contract(off)
#include "independent_clock.h"
#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>
#include <vector>

namespace qf {
inline constexpr std::uint64_t workCap=std::uint64_t{1}<<34;
inline constexpr std::uint64_t scratchCap=std::uint64_t{64}<<20;
inline constexpr double pi=std::numbers::pi;
inline constexpr double u=0x1p-53;
static_assert(sizeof(std::complex<double>)==16&&std::numeric_limits<double>::is_iec559&&std::numeric_limits<double>::digits==53&&std::numeric_limits<float>::digits==24);
inline constexpr double tableError=0x1p-37; // only a table admission ceiling; plans use actual certifiedMAX
struct Budget {
    std::uint64_t work=0,calls=0,limit;
    explicit Budget(std::uint64_t maximum=workCap):limit(maximum){qc::require(maximum<=workCap,"CHECKER_PHASE_CAP_DOMAIN");}
    void add(std::uint64_t n){qc::require(work<=limit&&n<=limit-work,"CHECKER_WORK_REFUSED");work+=n;}
    void library(std::uint64_t n){add(n);calls+=n;}
};
struct Sum {
    double value=0,correction=0;
    void add(double x){const double a=x-correction,n=value+a;correction=(n-value)-a;value=n;}
};
inline double up(double x,Budget& q){q.library(1);return std::nextafter(x,std::numeric_limits<double>::infinity());}
inline double down(double x,Budget& q){q.library(1);return std::nextafter(x,-std::numeric_limits<double>::infinity());}
inline std::complex<double> cmul(std::complex<double> a,std::complex<double> b){return {a.real()*b.real()-a.imag()*b.imag(),a.real()*b.imag()+a.imag()*b.real()};}
inline std::complex<double> cscale(std::complex<double> a,double b){return {a.real()*b,a.imag()*b};}
inline double gamma(double n,Budget& q){q.add(16);qc::require(n>=0&&n*u<0.01,"ESTIMATOR_UNPROVEN_ERROR_DOMAIN");const double numerator=up(n*u,q),denominator=down(1-numerator,q);return up(numerator/denominator,q);}

struct I {double lo,hi;};
inline I point(double x,Budget& q){q.add(2);qc::require(std::isfinite(x),"ESTIMATOR_UNPROVEN_INTERVAL_INPUT");return {x,x};}
inline I plus(I a,I b,Budget& q){q.add(4);return {down(a.lo+b.lo,q),up(a.hi+b.hi,q)};}
inline I neg(I a){return {-a.hi,-a.lo};}
inline I minus(I a,I b,Budget& q){return plus(a,neg(b),q);}
inline I times(I a,I b,Budget& q){q.add(12);const std::array<double,4> p{a.lo*b.lo,a.lo*b.hi,a.hi*b.lo,a.hi*b.hi};return {down(*std::min_element(p.begin(),p.end()),q),up(*std::max_element(p.begin(),p.end()),q)};}
inline I divide(I a,I b,Budget& q){q.add(8);qc::require(b.lo>0||b.hi<0,"ESTIMATOR_UNPROVEN_INTERVAL_POLE");q.add(4);const I inverse{down(1/b.hi,q),up(1/b.lo,q)};return times(a,inverse,q);}
inline I expand(I a,double error,Budget& q){q.add(4);qc::require(error>=0&&std::isfinite(error),"ESTIMATOR_UNPROVEN_ERROR_BOUND");return plus(a,{-error,error},q);}
inline double magnitude(I a,Budget& q){q.add(4);return std::max(std::abs(a.lo),std::abs(a.hi));}
inline double midpoint(I a,Budget& q){q.add(4);return a.lo+(a.hi-a.lo)/2;}
inline I piInterval(Budget& q){q.add(2);return {pi,0x1.921fb54442d19p+1};}
inline I angle(double frequency,double position,double rate,Budget& q){q.add(16);return divide(times(times(point(2*frequency,q),piInterval(q),q),point(position,q),q),point(rate,q),q);}
struct Trig {std::complex<double> value;double error;};
inline Trig trig(I theta,Budget& q){
    // nearest quadrant uses a float estimate only to select a reduction. The
    // actual reduced interval must independently fit [-0.8,0.8].
    q.add(32);q.library(1);const double approximate=midpoint(theta,q);
    const double quadrantReal=std::floor(approximate/(pi/2)+0.5);
    qc::require(std::abs(quadrantReal)<=1000000,"ESTIMATOR_UNPROVEN_TRIG_RANGE");
    const auto quadrant=static_cast<std::int64_t>(quadrantReal);
    const I halfPi{pi/2,0x1.921fb54442d19p+0};
    const I r=minus(theta,times(point(quadrantReal,q),halfPi,q),q);
    qc::require(magnitude(r,q)<=0.8,"ESTIMATOR_UNPROVEN_QUADRANT");
    const I rr=neg(times(r,r,q));I sine=r,cosine=point(1,q),st=r,ct=point(1,q);
    for(int k=1;k<=12;++k){
        q.add(8);
        st=divide(times(st,rr,q),point(double((2*k)*(2*k+1)),q),q);
        ct=divide(times(ct,rr,q),point(double((2*k-1)*(2*k)),q),q);
        sine=plus(sine,st,q);cosine=plus(cosine,ct,q);
    }
    // |r|<=0.8; first omitted sine/cosine terms are smaller than2^-80
    // (27! and26!, respectively), and alternating terms decrease.
    sine=expand(sine,0x1p-80,q);cosine=expand(cosine,0x1p-80,q);
    switch((quadrant%4+4)%4){case 1:{const I old=cosine;cosine=neg(sine);sine=old;break;}
        case 2:cosine=neg(cosine);sine=neg(sine);break;
        case 3:{const I old=cosine;cosine=sine;sine=neg(old);break;}default:break;}
    q.add(16);const std::complex<double> value(midpoint(cosine,q),midpoint(sine,q));
    const double error=up(std::max(std::abs(value.real()-cosine.lo),std::abs(cosine.hi-value.real()))
        +std::max(std::abs(value.imag()-sine.lo),std::abs(sine.hi-value.imag())),q);
    qc::require(error<=tableError,"ESTIMATOR_UNPROVEN_TABLE_ENCLOSURE");return {value,error};
}
inline std::size_t power2(std::size_t n,Budget& q){q.add(128);qc::require(n>0&&n<=131072,"CHECKER_TRANSFORM_CAP");std::size_t k=1;while(k<n)k*=2;return k;}
inline unsigned levels(std::size_t n,Budget& q){q.add(128);unsigned k=0;for(;n>1;n>>=1)++k;return k;}
struct FFT {
    std::size_t n;std::vector<std::complex<double>> roots;double maxRootError=0;
    FFT(std::size_t size,Budget& q):n(size){
        qc::require(n>=2&&n==power2(n,q),"CHECKER_FFT_LENGTH");q.add(n/2);roots.resize(n/2);
        for(std::size_t j=0;j<n/2;++j){q.add(8);const I theta=times(piInterval(q),point(-2*double(j)/double(n),q),q);const auto root=trig(theta,q);roots[j]=root.value;maxRootError=std::max(maxRootError,root.error);}
    }
    void apply(std::vector<std::complex<double>>& x,bool inverse,Budget& q)const{
        qc::require(x.size()==n,"CHECKER_FFT_VECTOR");
        for(std::size_t i=1,j=0;i<n;++i){q.add(2);std::size_t bit=n>>1;for(;j&bit;bit>>=1){q.add(2);j^=bit;}j^=bit;if(i<j){q.add(2);std::swap(x[i],x[j]);}}
        for(std::size_t width=2;width<=n;width*=2){const auto step=n/width;
            for(std::size_t begin=0;begin<n;begin+=width)for(std::size_t j=0;j<width/2;++j){
                q.add(12);auto w=roots[j*step];if(inverse)w=std::conj(w);
                const auto a=x[begin+j],b=cmul(x[begin+j+width/2],w);x[begin+j]=a+b;x[begin+j+width/2]=a-b;
            }}
        if(inverse)for(auto& z:x){q.add(2);z/=double(n);}
    }
    double relativeError(Budget& q)const{
        // Explicit cmul has4 rounded real products and2 additions. Component
        // bounds and Cauchy give etaMul=2*gamma3 and absolute8δ. A complex
        // add contributes relativeu and absolute2δ. qUpper encloses1+u.
        q.add(64);const I etaMul=times(point(2,q),point(gamma(3,q),q),q);
        const I H=times(plus(point(1,q),point(maxRootError,q),q),plus(point(1,q),etaMul,q),q);
        const double qUpper=plus(point(1,q),point(u,q),q).hi;
        const I Q=times(point(qUpper,q),H,q);I eta=point(0,q);const unsigned count=levels(n,q);
        for(unsigned i=0;i<count;++i){q.add(8);eta=minus(times(Q,plus(point(1,q),eta,q),q),point(1,q),q);}
        return eta.hi;
    }
    double absoluteError(Budget& q)const{
        q.add(64);const I etaMul=times(point(2,q),point(gamma(3,q),q),q);
        const I H=times(plus(point(1,q),point(maxRootError,q),q),plus(point(1,q),etaMul,q),q);
        const double qUpper=plus(point(1,q),point(u,q),q).hi;
        const I propagate=times(point(qUpper,q),plus(point(1,q),H,q),q);
        I error=point(0,q);const unsigned count=levels(n,q);
        for(unsigned i=0;i<count;++i){q.add(8);error=plus(times(propagate,error,q),point(16*std::numeric_limits<double>::denorm_min(),q),q);}
        return error.hi;
    }

};
inline double addU(double a,double b,Budget& q){q.add(2);return up(a+b,q);}
inline double mulU(double a,double b,Budget& q){q.add(2);return up(a*b,q);}
inline double divU(double a,double b,Budget& q){q.add(2);qc::require(b>0,"ERROR_BOUND_DIVISOR");return up(a/b,q);}
inline double sqrtU(double x,Budget& q){
    q.add(16);q.library(1);qc::require(x>=0&&std::isfinite(x),"ERROR_BOUND_SQRT_DOMAIN");double r=std::sqrt(x);
    if(x==0)return 0;
    for(unsigned i=0;i<8;++i){q.add(8);if(times(point(r,q),point(r,q),q).lo>=x)return r;r=up(r,q);}
    qc::require(false,"ERROR_BOUND_SQRT_UNPROVEN");return 0;
}
struct Affine {double t=0,d=0;};
inline Affine aa(Affine a,Affine b,Budget& q){return {addU(a.t,b.t,q),addU(a.d,b.d,q)};}
inline Affine am(double x,Affine a,Budget& q){return {mulU(x,a.t,q),mulU(x,a.d,q)};}
struct KahanMap {
    Affine error;std::size_t count;
    KahanMap(std::size_t m,Budget& q):count(m){
        qc::require(m>0&&m<=24000,"KAHAN_COUNT_DOMAIN");
        const double Q=addU(1,u,q);const Affine T{1,0},D{0,1};Affine E,C,S;
        for(std::size_t i=0;i<m;++i){q.add(8);
            const auto TC=aa(T,C,q),A=aa(am(Q,TC,q),D,q),ea=aa(am(u,TC,q),D,q);
            const auto en=aa(am(u,aa(S,A,q),q),D,q),et=aa(am(u,aa(A,en,q),q),D,q);
            const auto ec=aa(am(u,aa(en,et,q),q),D,q);
            const auto nextE=aa(aa(aa(E,C,q),ea,q),en,q);
            const auto nextC=aa(aa(en,et,q),ec,q),nextS=aa(am(Q,aa(S,A,q),q),D,q);
            E=nextE;C=nextC;S=nextS;
        }
        error=E;
    }
    double bound(double maxTerm,Budget& q)const{
        qc::require(maxTerm>=0&&std::isfinite(maxTerm),"KAHAN_TERM_DOMAIN");
        return addU(mulU(error.t,maxTerm,q),mulU(error.d,std::numeric_limits<double>::denorm_min(),q),q);
    }
};
struct Sequence {
    std::complex<double> current,step;double error=0,stepError=0;std::size_t index=0;
    Sequence(I initial,I increment,Budget& q){const auto a=trig(initial,q),b=trig(increment,q);current=a.value;step=b.value;error=a.error;stepError=b.error;}
    double bound(std::size_t n,Budget& q)const{
        qc::require(n<=24000,"BASIS_RECURRENCE_DOMAIN");
        I power=point(1,q),base=times(plus(point(1,q),point(stepError,q),q),plus(point(1,q),point(gamma(8,q),q),q),q);
        const std::size_t count=n;
        while(n){q.add(8);if(n&1)power=times(power,base,q);n>>=1;if(n)base=times(base,base,q);}
        const I relative=minus(times(plus(point(1,q),point(error,q),q),power,q),point(1,q),q);
        const I absolute=times(times(point(8*double(count),q),point(std::numeric_limits<double>::denorm_min(),q),q),power,q);
        return plus(relative,absolute,q).hi;
    }
    std::complex<double> pointNext(Budget& q){q.add(10);const auto value=current;current=cmul(current,step);++index;return value;}
    Trig next(Budget& q){
        const Trig result{current,error};q.add(10);current=cmul(current,step);
        const auto scale=times(plus(point(1,q),point(stepError,q),q),plus(point(1,q),point(gamma(8,q),q),q),q);
        const auto e=plus(minus(times(plus(point(1,q),point(error,q),q),scale,q),point(1,q),q),point(8*std::numeric_limits<double>::denorm_min(),q),q);
        error=e.hi;++index;return result;
    }
};
struct Correlation {double c,s,error;};
inline std::vector<std::complex<double>> realSpectrum(std::span<const float> y,std::size_t k,
    const FFT& plan,const std::vector<std::complex<double>>& roots,Budget& q){
    qc::require(k>=4&&plan.n==k/2&&roots.size()==k/2+1&&y.size()<=k,"CHECKER_PACKED_FFT_DOMAIN");
    q.add(k);std::vector<std::complex<double>> packed(k/2,{0,0});
    for(std::size_t n=0;n<y.size();n+=2){q.add(4);packed[n/2]={double(y[n]),n+1<y.size()?double(y[n+1]):0};}
    plan.apply(packed,false,q);q.add(k/2+1);std::vector<std::complex<double>> result(k/2+1);
    for(std::size_t j=0;j<=k/2;++j){q.add(28);const auto a=packed[j%(k/2)],b=std::conj(packed[(k/2-j%(k/2))%(k/2)]);
        const auto e=(a+b)*0.5,o=cmul(a-b,std::complex<double>(0,-0.5));result[j]=e+cmul(roots[j],o);}
    return result;
}
inline std::size_t trialCount(std::size_t rate,std::size_t k,Budget& q){q.add(32);q.library(1);return static_cast<std::size_t>(std::ceil((4*double(rate)/double(k))/0.02))+1;}
inline std::size_t checkedRate(std::size_t rate,Budget& q){q.add(128);qc::require(16u*(32768u+16384u+65537u+24000u+32768u)+8192u<=(4u<<20),"CHECKER_PLAN_RESERVATION");qc::require(rate==44100||rate==48000||rate==96000,"CHECKER_RATE_DOMAIN");return rate;}
struct Grid {
    std::size_t rate,m,k,t,p;double spacing;
    FFT seedPlan,convolutionPlan;KahanMap sumBounds;
    std::vector<std::complex<double>> chirp,kernel,seedRoots;
    double kernelError=0,maxChirpError=0,maxKernelInputError=0,maxSeedRecoveryError=0;
    Grid(std::size_t rateIn,Budget& q):rate(checkedRate(rateIn,q)),m(rate/4),k(power2(4*m,q)),
        t(trialCount(rate,k,q)),p(power2(m+t-1,q)),
        spacing((4*double(rate)/double(k))/double(t-1)),seedPlan(k/2,q),convolutionPlan(p,q),sumBounds(m,q){
        qc::require((rate==44100||rate==48000||rate==96000)&&t<=203&&p<=32768,"CHECKER_GRID_DOMAIN");
        q.add(k/2+1);seedRoots.resize(k/2+1);
        for(std::size_t j=0;j<=k/2;++j){q.add(8);const auto z=trig(times(piInterval(q),point(-2*double(j)/double(k),q),q),q);seedRoots[j]=z.value;maxSeedRecoveryError=std::max(maxSeedRecoveryError,z.error);}
        q.add(m+p);chirp.resize(m);kernel.assign(p,{0,0});
        const I step=angle(spacing,1,double(rate),q);
        for(std::size_t n=0;n<m;++n){q.add(8);const auto z=trig(times(step,point(-double(n*n)/2,q),q),q);chirp[n]=z.value;maxChirpError=std::max(maxChirpError,z.error);}
        // For 0<=trial<T and0<=sample<M, trial-sample lies in
        // [-(M-1),T-1]. P>=M+T-1 makes each such index unique moduloP.
        for(std::int64_t j=-static_cast<std::int64_t>(m-1);j<static_cast<std::int64_t>(t);++j){q.add(8);
            const auto at=j<0?static_cast<std::size_t>(static_cast<std::int64_t>(p)+j):static_cast<std::size_t>(j);
            const auto z=trig(times(step,point(double(j*j)/2,q),q),q);kernel[at]=z.value;maxKernelInputError=std::max(maxKernelInputError,z.error);}
        convolutionPlan.apply(kernel,false,q);kernelError=convolutionPlan.relativeError(q);
    }
    double seedError(double absoluteInputSum,Budget& q)const{
        const double e=addU(mulU(seedPlan.relativeError(q),absoluteInputSum,q),seedPlan.absoluteError(q),q);
        const double scaled=mulU(addU(2,maxSeedRecoveryError,q),e,q);
        const double rootAndRound=addU(maxSeedRecoveryError,mulU(gamma(32,q),addU(1,maxSeedRecoveryError,q),q),q);
        return addU(scaled,addU(mulU(rootAndRound,absoluteInputSum,q),32*std::numeric_limits<double>::denorm_min(),q),q);
    }
    std::size_t payload()const{return sizeof(*this)+16*(seedPlan.roots.capacity()+convolutionPlan.roots.capacity()+chirp.capacity()+kernel.capacity()+seedRoots.capacity());}
    std::vector<std::complex<double>> spectrum(std::span<const float> y,Budget& q)const{
        return realSpectrum(y,k,seedPlan,seedRoots,q);
    }
    std::vector<Correlation> correlations(std::span<const float> y,double lo,double hi,Budget& q)const{
        qc::require(y.size()==m,"CHECKER_CORRELATION_WINDOW");
        const double actualSpacing=(hi-lo)/double(t-1);
        qc::require(actualSpacing==spacing,"ESTIMATOR_UNPROVEN_KERNEL_GEOMETRY");
        const I omega=angle(lo,1,double(rate),q),delta=angle(spacing,1,double(rate),q);
        Sequence linear(point(0,q),neg(omega),q);q.add(p);std::vector<std::complex<double>> a(p,{0,0});
        Sum energy,absolute;double maxAmplitude=0;const double phaseError=linear.bound(m,q);
        for(std::size_t n=0;n<m;++n){q.add(40);const auto z=linear.pointNext(q);a[n]=cscale(cmul(chirp[n],z),double(y[n]));energy.add(double(y[n])*double(y[n]));absolute.add(std::abs(double(y[n])));maxAmplitude=std::max(maxAmplitude,std::abs(double(y[n])));}
        convolutionPlan.apply(a,false,q);for(std::size_t j=0;j<p;++j){q.add(8);a[j]=cmul(a[j],kernel[j]);}convolutionPlan.apply(a,true,q);
        const double eta=convolutionPlan.relativeError(q),absoluteFft=convolutionPlan.absoluteError(q);
        const double denorm=std::numeric_limits<double>::denorm_min();
        const double energyError=addU(sumBounds.bound(mulU(maxAmplitude,maxAmplitude,q),q),mulU(double(m),addU(mulU(u,mulU(maxAmplitude,maxAmplitude,q),q),denorm,q),q),q);
        const double ynorm=sqrtU(addU(energy.value,energyError,q),q);
        const double anorm1=addU(absolute.value,sumBounds.bound(maxAmplitude,q),q);
        const double prepRel=minus(times(times(plus(point(1,q),point(phaseError,q),q),plus(point(1,q),point(maxChirpError,q),q),q),plus(point(1,q),point(gamma(16,q),q),q),q),point(1,q),q).hi;
        const double a1=addU(mulU(anorm1,addU(1,prepRel,q),q),mulU(16*double(m),denorm,q),q);
        const double a2=addU(mulU(ynorm,addU(1,prepRel,q),q),mulU(16*double(m),denorm,q),q);
        const double kernelCount=double(m+t-1),b1=mulU(kernelCount,addU(1,maxKernelInputError,q),q);
        const double b2=mulU(sqrtU(kernelCount,q),addU(1,maxKernelInputError,q),q);
        const double ea=addU(mulU(eta,a1,q),absoluteFft,q),eb=addU(mulU(eta,b1,q),absoluteFft,q);
        const double productMean=mulU(addU(a2,ea,q),addU(b2,eb,q),q);
        const double productRound=addU(mulU(gamma(8,q),productMean,q),8*denorm,q);
        double convolutionError=addU(addU(mulU(ea,b2,q),mulU(eb,a2,q),q),mulU(ea,eb,q),q);
        convolutionError=addU(convolutionError,productRound,q);
        const double approximateMean=addU(productMean,productRound,q);
        convolutionError=addU(convolutionError,addU(mulU(eta,approximateMean,q),divU(absoluteFft,double(p),q),q),q);
        convolutionError=addU(convolutionError,addU(mulU(u,addU(mulU(a2,b2,q),convolutionError,q),q),2*denorm,q),q);
        const double inputModel=addU(mulU(anorm1,addU(addU(prepRel,maxKernelInputError,q),mulU(prepRel,maxKernelInputError,q),q),q),mulU(32*double(m),denorm,q),q);
        convolutionError=addU(convolutionError,inputModel,q);
        Sequence center(times(omega,point((double(m)-1)/2,q),q),times(delta,point((double(m)-1)/2,q),q),q);
        q.add(t);std::vector<Correlation> result(t);
        for(std::size_t j=0;j<t;++j){q.add(32);const auto z=center.next(q);
            const auto out=trig(times(delta,point(-double(j*j)/2,q),q),q);
            const auto value=cmul(cmul(a[j],out.value),z.value);
            const double f=lo+(hi-lo)*double(j)/double(t-1);
            const I affine=plus(point(lo,q),times(point(spacing,q),point(double(j),q),q),q);
            const double gridDifference=magnitude(minus(point(f,q),affine,q),q);
            const double drift=divide(times(times(point(2*double(m),q),piInterval(q),q),point(gridDifference,q),q),point(double(rate),q),q).hi;
            const I rotationScale=times(times(plus(point(1,q),point(out.error,q),q),plus(point(1,q),point(z.error,q),q),q),plus(point(1,q),point(gamma(16,q),q),q),q);
            const double modelRotation=plus(minus(rotationScale,point(1,q),q),point(drift,q),q).hi;
            const double error=addU(mulU(convolutionError,rotationScale.hi,q),addU(mulU(modelRotation,anorm1,q),32*denorm,q),q);
            result[j]={value.real(),-value.imag(),error};
        }
        return result;
    }
};

// Fixed point-coefficient objective: no uncomputed direct solve is pruned.
struct SignalStats {I energy,y0;double maxAmplitude=0,absoluteSumUpper=0,rawEnergy=0;};
inline SignalStats signalStats(std::span<const float> y,const KahanMap& map,Budget& q){
    Sum energy,total,absolute;double peak=0;
    for(float x:y){q.add(24);qc::require(std::isfinite(x),"ESTIMATOR_UNPROVEN_NONFINITE_SAMPLE");const double v=x;peak=std::max(peak,std::abs(v));energy.add(v*v);total.add(v);absolute.add(std::abs(v));}
    const double square=mulU(peak,peak,q),delta=std::numeric_limits<double>::denorm_min();
    const double ee=addU(map.bound(square,q),mulU(double(y.size()),addU(mulU(u,square,q),delta,q),q),q);
    return {expand(point(energy.value,q),ee,q),expand(point(total.value,q),map.bound(peak,q),q),peak,addU(absolute.value,map.bound(peak,q),q),energy.value};
}
struct BasisSpec {
    double frequency;std::complex<double> initial,step;double epsilon;
    I cc,ss,c;
};
static_assert(sizeof(Grid)<=8192);
static_assert(sizeof(BasisSpec)<=128);
inline I sine(I theta,Budget& q){const auto z=trig(theta,q);return expand(point(z.value.imag(),q),z.error,q);}
inline BasisSpec makeBasis(double f,const Grid& grid,Budget& q){
    q.add(64);const double m=double(grid.m);const I omega=angle(f,1,double(grid.rate),q);
    Sequence basis(times(omega,point(-(m-1)/2,q),q),omega,q);const double epsilon=basis.bound(grid.m,q);
    qc::require(epsilon<=0x1p-20,"ESTIMATOR_UNPROVEN_BASIS_ENCLOSURE");
    const I c1=divide(sine(times(omega,point(m/2,q),q),q),sine(times(omega,point(0.5,q),q),q),q);
    const I c2=divide(sine(times(omega,point(m,q),q),q),sine(omega,q),q);
    return {f,basis.current,basis.step,epsilon,times(plus(point(m,q),c2,q),point(0.5,q),q),times(minus(point(m,q),c2,q),point(0.5,q),q),c1};
}
struct Geometry {std::size_t seedBin=0;double lo=0,hi=0;std::vector<BasisSpec> basis;};
static_assert(sizeof(qf::Geometry)<=64);
inline Geometry makeGeometry(std::size_t peak,const Grid& grid,Budget& q){
    q.add(64);const double seed=double(peak)*double(grid.rate)/double(grid.k),radius=2*double(grid.rate)/double(grid.k);
    Geometry out{peak,seed-radius,seed+radius,{}};
    qc::require(out.lo>0&&out.hi<double(grid.rate)/2,"ESTIMATOR_UNPROVEN_BAND_EDGE");
    q.add(grid.t);out.basis.reserve(grid.t);
    for(std::size_t i=0;i<grid.t;++i){q.add(16);out.basis.push_back(makeBasis(out.lo+(out.hi-out.lo)*double(i)/double(grid.t-1),grid,q));}
    return out;
}
inline std::array<double,3> coefficients(const BasisSpec& b,const Correlation& c,const SignalStats& s,std::size_t m,Budget& q){
    q.add(512);double a[3][4]={{midpoint(b.cc,q),0,midpoint(b.c,q),c.c},{0,midpoint(b.ss,q),0,c.s},{midpoint(b.c,q),0,double(m),midpoint(s.y0,q)}};
    for(int col=0;col<3;++col){int pivot=col;for(int row=col+1;row<3;++row)if(std::abs(a[row][col])>std::abs(a[pivot][col]))pivot=row;
        qc::require(std::isfinite(a[pivot][col])&&std::abs(a[pivot][col])>1e-12*double(m),"ESTIMATOR_UNPROVEN_COEFFICIENT_PIVOT");
        for(int j=col;j<4;++j)std::swap(a[pivot][j],a[col][j]);
        for(int row=col+1;row<3;++row){const double factor=a[row][col]/a[col][col];for(int j=col;j<4;++j)a[row][j]-=factor*a[col][j];}}
    std::array<double,3> beta{};
    for(int row=2;row>=0;--row){double v=a[row][3];for(int j=row+1;j<3;++j)v-=a[row][j]*beta[std::size_t(j)];beta[std::size_t(row)]=v/a[row][row];qc::require(std::isfinite(beta[std::size_t(row)]),"ESTIMATOR_UNPROVEN_COEFFICIENT_FINITE");}
    return beta;
}
inline double evaluationError(const std::array<double,3>& beta,double amplitude,double basisMaximum,const KahanMap& map,Budget& q){
    q.add(32);const double delta=std::numeric_limits<double>::denorm_min(),Q=addU(1,u,q);
    const double p0=mulU(std::abs(beta[0]),basisMaximum,q),p1=mulU(std::abs(beta[1]),basisMaximum,q);
    const double ep0=addU(mulU(u,p0,q),delta,q),ep1=addU(mulU(u,p1,q),delta,q);
    const double a0=addU(p0,ep0,q),a1=addU(p1,ep1,q);
    const double e0=addU(mulU(u,addU(amplitude,a0,q),q),delta,q),d0=addU(mulU(Q,addU(amplitude,a0,q),q),delta,q);
    const double e1=addU(mulU(u,addU(d0,a1,q),q),delta,q),d1=addU(mulU(Q,addU(d0,a1,q),q),delta,q);
    const double e2=addU(mulU(u,addU(d1,std::abs(beta[2]),q),q),delta,q);
    const double ed=addU(addU(ep0,ep1,q),addU(addU(e0,e1,q),e2,q),q);
    const double exactD=addU(addU(amplitude,addU(p0,p1,q),q),std::abs(beta[2]),q);
    const double roundedSquare=mulU(addU(exactD,ed,q),addU(exactD,ed,q),q);
    const double productError=addU(mulU(u,roundedSquare,q),delta,q);
    const double squareError=addU(addU(mulU(2*exactD,ed,q),mulU(ed,ed,q),q),productError,q);
    const double termMaximum=addU(roundedSquare,productError,q);
    return addU(mulU(double(map.count),squareError,q),map.bound(termMaximum,q),q);
}
struct Trial {std::array<double,3> beta;I rss;double excessUpper;};
static_assert(sizeof(Trial)<=64);
inline Trial enclose(const BasisSpec& b,const Correlation& c,const SignalStats& signal,const Grid& grid,Budget& q){
    q.add(128);const double m=double(grid.m),eps=b.epsilon;
    const double pairDelta=mulU(m,addU(2*eps,mulU(eps,eps,q),q),q),dcDelta=mulU(m,eps,q),rhsDelta=mulU(eps,signal.absoluteSumUpper,q);
    const I zero=point(0,q);I G[3][3]={{expand(b.cc,pairDelta,q),expand(zero,pairDelta,q),expand(b.c,dcDelta,q)},
        {expand(zero,pairDelta,q),expand(b.ss,pairDelta,q),expand(zero,dcDelta,q)},
        {expand(b.c,dcDelta,q),expand(zero,dcDelta,q),point(m,q)}};
    I rhs[3]={expand(point(c.c,q),addU(c.error,rhsDelta,q),q),expand(point(c.s,q),addU(c.error,rhsDelta,q),q),signal.y0};
    const auto beta=coefficients(b,c,signal,grid.m,q);double ell=std::numeric_limits<double>::infinity();
    for(std::size_t row=0;row<3;++row){q.add(8);I bound=G[row][row];for(std::size_t col=0;col<3;++col)if(col!=row)bound=minus(bound,point(magnitude(G[row][col],q),q),q);ell=std::min(ell,bound.lo);}
    qc::require(ell>m/4,"ESTIMATOR_UNPROVEN_GRAM_EIGENVALUE");
    I rss=signal.energy;double residualSquare=0;
    for(std::size_t row=0;row<3;++row){q.add(8);rss=minus(rss,times(point(2*beta[row],q),rhs[row],q),q);I r=rhs[row];
        for(std::size_t col=0;col<3;++col){q.add(8);rss=plus(rss,times(times(point(beta[row],q),G[row][col],q),point(beta[col],q),q),q);r=minus(r,times(G[row][col],point(beta[col],q),q),q);}
        residualSquare=addU(residualSquare,mulU(magnitude(r,q),magnitude(r,q),q),q);
    }
    const double excess=divU(residualSquare,ell,q);
    // A declared numerical-precision guard, chosen before candidate data:
    // approximate-LS excess <=2^-32 of raw energy. Calibration still must pass.
    qc::require(signal.energy.lo>0&&excess<=times(point(0x1p-32,q),signal.energy,q).lo,"ESTIMATOR_UNPROVEN_LEAST_SQUARES_PRECISION");
    const double round=evaluationError(beta,signal.maxAmplitude,addU(1,eps,q),grid.sumBounds,q);
    return {beta,expand(rss,round,q),excess};
}
inline double directRss(std::span<const float> y,const BasisSpec& b,const Trial& trial,Budget& q){
    auto z=b.initial;Sum rss;
    for(float sample:y){q.add(40);const double p0=trial.beta[0]*z.real(),p1=trial.beta[1]*z.imag();const double d0=double(sample)-p0,d1=d0-p1,d2=d1-trial.beta[2];rss.add(d2*d2);z=cmul(z,b.step);}
    qc::require(std::isfinite(rss.value)&&rss.value>=0&&rss.value>=trial.rss.lo&&rss.value<=trial.rss.hi,"ESTIMATOR_UNPROVEN_DIRECT_RSS_ENCLOSURE");return rss.value;
}
struct Measurement {std::string status;double frequency=0,rms=0,seed=0,dominance=0,rss=0,seedAmplitudeErrorUpper=0;std::size_t trials=0,directTrials=0,winnerIndex=0;std::uint64_t scratch=0;};
struct Seed {Measurement measurement;std::size_t bin=0;SignalStats stats;};
inline Seed seedWindow(std::span<const float> y,const Grid& grid,Budget& q){
    qc::require(y.size()==grid.m,"CHECKER_WINDOW_DOMAIN");Seed out;
    q.add(64);out.measurement.scratch=2*grid.payload()+64*grid.k+32*grid.p+32*grid.m+256*grid.t+(1u<<20);
    qc::require(out.measurement.scratch<=scratchCap,"CHECKER_SCRATCH_REFUSED");out.stats=signalStats(y,grid.sumBounds,q);out.measurement.seedAmplitudeErrorUpper=grid.seedError(out.stats.absoluteSumUpper,q);
    q.add(16);q.library(1);out.measurement.rms=std::sqrt(out.stats.rawEnergy/double(y.size()));
    const auto spectrum=grid.spectrum(y,q);std::size_t peak=1;double power=-1,remaining=0;
    for(std::size_t j=1;j<spectrum.size();++j){q.add(8);const double p=spectrum[j].real()*spectrum[j].real()+spectrum[j].imag()*spectrum[j].imag();qc::require(std::isfinite(p),"ESTIMATOR_UNPROVEN_NONFINITE_SPECTRUM");if(p>power){power=p;peak=j;}}
    out.bin=peak;out.measurement.seed=double(peak)*double(grid.rate)/double(grid.k);
    for(std::size_t j=1;j<spectrum.size();++j){q.add(8);if(std::abs(double(j)*double(grid.rate)/double(grid.k)-out.measurement.seed)>2*double(grid.rate)/double(grid.m))remaining=std::max(remaining,spectrum[j].real()*spectrum[j].real()+spectrum[j].imag()*spectrum[j].imag());}
    out.measurement.dominance=remaining>0?power/remaining:std::numeric_limits<double>::max();
    out.measurement.status=power>0&&power>=4*remaining?"RANK_REQUIRED":"ESTIMATOR_UNPROVEN";return out;
}
struct Ranked {std::vector<Trial> trials;std::vector<std::size_t> finalists;};
inline Ranked rankWindow(std::span<const float> y,const Seed& seed,const Geometry& geometry,const Grid& grid,Budget& q){
    qc::require(seed.bin==geometry.seedBin&&seed.measurement.status=="RANK_REQUIRED","CHECKER_RANK_IDENTITY");
    const auto correlations=grid.correlations(y,geometry.lo,geometry.hi,q);Ranked out;q.add(2*grid.t);out.trials.reserve(grid.t);out.finalists.reserve(grid.t);
    double smallestUpper=std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<grid.t;++i){q.add(16);out.trials.push_back(enclose(geometry.basis[i],correlations[i],seed.stats,grid,q));smallestUpper=std::min(smallestUpper,out.trials.back().rss.hi);}
    for(std::size_t i=0;i<grid.t;++i){q.add(8);if(out.trials[i].rss.lo<=smallestUpper)out.finalists.push_back(i);}
    qc::require(!out.finalists.empty(),"ESTIMATOR_UNPROVEN_EMPTY_FINALISTS");return out;
}
inline std::uint64_t directUpper(std::size_t samples,std::size_t finalists,Budget& q){
    q.add(16);qc::require(samples<=24000&&finalists<=203,"DIRECT_PREFLIGHT_DOMAIN");return std::uint64_t(finalists)*(40*std::uint64_t(samples)+128);
}
inline Measurement finishWindow(std::span<const float> y,const Seed& seed,const Geometry& geometry,const Ranked& ranked,Budget& q){
    auto out=seed.measurement;double best=std::numeric_limits<double>::infinity();std::size_t winner=0;
    for(std::size_t i:ranked.finalists){q.add(64);const double value=directRss(y,geometry.basis[i],ranked.trials[i],q);if(value<best){best=value;winner=i;}}
    out.winnerIndex=winner;out.frequency=geometry.basis[winner].frequency;out.rss=best;out.trials=ranked.trials.size();out.directTrials=ranked.finalists.size();out.status=winner==0||winner+1==ranked.trials.size()?"ESTIMATOR_UNPROVEN":"OK";return out;
}
inline Measurement measure(std::span<const float> y,const Grid& grid,Budget& q){
    const auto seed=seedWindow(y,grid,q);if(seed.measurement.status!="RANK_REQUIRED")return seed.measurement;
    const auto geometry=makeGeometry(seed.bin,grid,q);const auto ranked=rankWindow(y,seed,geometry,grid,q);
    qc::require(directUpper(y.size(),ranked.finalists.size(),q)<=workCap-q.work,"CHECKER_WORK_REFUSED_WHOLE_CASE");
    return finishWindow(y,seed,geometry,ranked,q);
}
} // namespace qf
