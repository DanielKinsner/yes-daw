#pragma once
#include "engine/MusicalAudioCutPlan.h"
#include "engine/MusicalAudioChannelMaterialization.h"
#include "signalsmith-stretch.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <span>
#include <stdexcept>
#include <vector>
#include <xmmintrin.h>
#pragma fenv_access(on)
#pragma fp_contract(off)
namespace waveform_r1 {
using namespace yesdaw::engine;
inline constexpr bool sourceReviewAccepted=false;
inline constexpr bool completeResourceGraphAccepted=false;
inline constexpr bool varyingPhysicalContextAccepted=false;
inline constexpr bool actualQueriedPhysicalLayoutAccepted=false;
inline constexpr bool physicalCenterReadCompositionAccepted=false;
inline void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
inline void arithmetic(){need(std::fegetround()==FE_TONEAREST&&(_mm_getcsr()&~0x3fu)==0x1f80u,"UnsupportedArithmetic");}
struct Budget {
    std::uint64_t work=0,bytes=0;
    void spend(std::uint64_t n){need(work<=(1ull<<28)&&n<=(1ull<<28)-work,"MusicalResourceLimit: work");work+=n;}
    void reserve(std::uint64_t n){need(bytes<=(512ull<<20)&&n<=(512ull<<20)-bytes,"MusicalResourceLimit: bytes");bytes+=n;}
};
enum class Intervention{None,Pitch6Cent,Level1p1dB,DoubleFade,ReverseAfter,LatencyPlus1,OmitOutputLatency,Center3Frame,MonoWiden,OldC2};
struct Configuration {int requestedChannels=0,w=0,h=0,k=0,il=0,ol=0;double seekPlaybackRate=0;};
struct Read {std::int64_t project=0,local=-1,physical=-1;int channel=0;std::uint32_t word=0;bool outside=true;};
inline Read projectRead(std::span<const float> pcm,std::int64_t A,std::int64_t D,int channels,
                        std::int64_t p,int c,std::int64_t delta,std::int64_t OL){
    need(A>=0&&D>0&&channels>0&&channels<=2&&c>=0&&c<channels,"ProjectReadDomain");
    need(delta>=-3&&delta<=3&&A<=std::numeric_limits<std::int64_t>::max()-3,"ProjectReadOrigin");
    need(p>=std::numeric_limits<std::int64_t>::min()+A+3,"ProjectReadSubtract");
    const auto j=p-(A+delta);Read r{p,j,-1,c,0,true};
    if(j<0||j>=D)return r;
    need(std::uint64_t(D)<=std::numeric_limits<std::size_t>::max()/std::uint64_t(channels)&&pcm.size()==std::size_t(D)*std::size_t(channels),"ProjectReadGeometry");
    need(j<=std::numeric_limits<std::int64_t>::max()-OL,"ProjectReadPhysicalOverflow");
    r.physical=OL+j;r.word=std::bit_cast<std::uint32_t>(pcm[std::size_t(j)*std::size_t(channels)+std::size_t(c)]);r.outside=false;return r;
}
struct Contribution {std::int64_t wi=0,sourceCut=0;int W=0;bool owned=false,nonzeroInterior=false;};
inline std::int64_t centerResidualTwice(const Read& r,std::int64_t A,const Contribution& c){
    need(!r.outside&&c.owned&&c.nonzeroInterior&&c.W>0&&c.wi>=0&&c.wi<c.W,"CenterOwnershipRequired");
    need(std::abs(r.project-A)<(1ll<<50)&&std::abs(c.sourceCut)<(1ll<<50),"CenterIntegerDomain");
    return 2*(r.project-A-c.wi)+c.W-2*c.sourceCut;
}
inline int centerDelta(std::int64_t d2){return d2>=0?3:-3;}
inline float shape(FadeShape s,float curve,double x){
    const double z=std::clamp(x,0.0,1.0),c=std::clamp(double(curve),-1.0,1.0);
    const double h=c==0?z:std::pow(z,std::exp2(-2*c));
    switch(s){case FadeShape::Linear:return float(h);case FadeShape::EqualPower:return float(std::sin(std::numbers::pi*h/2));case FadeShape::SCurve:return float(h*h*(3-2*h));case FadeShape::Log:return float((1-std::exp(-4*h))/(1-std::exp(-4)));}
    throw std::runtime_error("UnsupportedFadeShape");
}
inline float envelope(const Clip& clip,double u){
    need(std::isfinite(u)&&u>=0&&u<double(clip.timelineLength),"FractionalFadeDomain");
    const double L=double(clip.timelineLength),fi=double(clip.fadeIn),fo=double(clip.fadeOut),begin=std::max(0.0,L-fo);
    const float a=fi<=0||u>=fi?1:shape(clip.fadeInShape,clip.fadeInCurve,u/fi);
    const float b=fo<=0||u<begin?1:shape(clip.fadeOutShape,clip.fadeOutCurve,1-(u-begin)/fo);
    return std::min(a,b);
}
inline void bake(AssetSamples& samples,const Clip& clip,const MusicalAudioWindow& window,Intervention fault,Budget& b){
    const auto rows=window.clock()->rows();CompiledTempoMap clock;
    need(CompiledTempoMap::build({rows.data(),rows.size()},window.clock()->sampleRate(),clock),"FadeClockBuild");
    for(std::uint64_t j=0;j<samples.frames;++j){b.spend(2048);MusicalTime t;
        const auto p=window.startFrame()+std::int64_t(j);
        need(clock.musicalTimeForFrame(double(p),t)==TimeLookupStatus::Ok,"MusicalInversePrecision");
        double roundTrip=0;need(clock.frameForMusicalTime(t,roundTrip)==TimeLookupStatus::Ok&&std::abs(roundTrip-double(p))<=1.0/1024,"MusicalInversePrecision");
        double u=double(t.tick-window.startTick())+t.frac;
        if(u<0){need(j==0&&roundTrip<=double(window.startFrame())+1,"LeadingFloorFadeDomain");u=0;}
        const float e=envelope(clip,u);
        for(int c=0;c<samples.channels;++c){const auto index=std::size_t(j)*std::size_t(samples.channels)+std::size_t(c);samples.interleaved[index]*=e;if(fault==Intervention::DoubleFade)samples.interleaved[index]*=e;}
    }
}
template<class Plan>std::shared_ptr<const AssetSamples> renderPhysical(const std::shared_ptr<const Plan>& plan,
    const MusicalAudioCutPlan& cuts,Configuration cfg,Intervention fault,Budget& b,std::array<std::vector<float>,2>* retainedRaw=nullptr){
    need(sourceReviewAccepted&&completeResourceGraphAccepted&&varyingPhysicalContextAccepted&&actualQueriedPhysicalLayoutAccepted,"SOURCE_HOLD_CONTEXT_LAYOUT_FULL_RESOURCES");
    arithmetic();need(bool(plan)&&plan->request()->window().frameCount()>0,"RequestRequired");
    const auto& win=plan->request()->window();const auto N=win.inputFrames();const auto D=std::uint64_t(win.frameCount());
    const int C=fault==Intervention::OldC2?int(plan->logicalChannels()):int(plan->physicalChannels());need(C==cfg.requestedChannels&&(C==1||C==2)&&cfg.w>0&&cfg.h>0&&cfg.w<=std::numeric_limits<int>::max()-cfg.h&&cfg.il>=0&&cfg.ol>=0&&cfg.k>=cfg.w&&std::int64_t(cfg.il)+cfg.ol==cfg.w,"ConfigurationPreflight");
    need(D<=(std::numeric_limits<std::size_t>::max()/sizeof(float))/std::uint64_t(C)-std::uint64_t(cfg.w),"RawProductOverflow");
    b.reserve(4*std::uint64_t(C)*(D+std::uint64_t(cfg.w))+4*std::uint64_t(C)*D+4*std::uint64_t(C)*(std::uint64_t(cfg.w+cfg.h)+2048));
    // This payload subtotal is deliberately not the missing allocator/DSP certificate.
    signalsmith::stretch::SignalsmithStretch<float> dsp(12345);
    dsp.configure(C,cfg.w,cfg.h);dsp.reset();
    need(dsp.blockSamples()==cfg.w&&dsp.intervalSamples()==cfg.h&&dsp.inputLatency()==cfg.il&&dsp.outputLatency()==cfg.ol,"QueriedPublicGeometryMismatch");
    if(fault==Intervention::Pitch6Cent)dsp.setTransposeFactor(float(std::exp2(6.0/1200)));
    const auto read=[&](int c,std::int64_t x){b.spend(16);if(x<0||std::uint64_t(x)>=N)return 0.0f;if(fault==Intervention::ReverseAfter||fault==Intervention::OldC2){const auto request=plan->request();const auto at=request->mappedWindow().srcOffset+std::uint64_t(x);return request->mappedOwner()->interleaved[std::size_t(at)*std::size_t(plan->logicalChannels())+std::size_t(c)];}float value=0;need(int(plan->physicalSample(std::uint16_t(c),std::uint64_t(x),value))==0,"PhysicalInputRead");return value;};
    const int seekCount=cfg.w+cfg.h;std::array<std::vector<float>,2> in,raw;std::array<const float*,2> ip{};std::array<float*,2> op{};
    for(int c=0;c<C;++c){in[std::size_t(c)].resize(std::size_t(std::max(seekCount,2048)));raw[std::size_t(c)].resize(std::size_t(D)+std::size_t(cfg.w));for(int i=0;i<seekCount;++i)in[std::size_t(c)][std::size_t(i)]=read(c,cfg.il-seekCount+i);ip[std::size_t(c)]=in[std::size_t(c)].data();}
    need(!cuts.calls().empty(),"EmptyCutPlan");need(std::isfinite(cfg.seekPlaybackRate)&&cfg.seekPlaybackRate>=0.5&&cfg.seekPlaybackRate<=2,"FrozenContinuousSeekRate");dsp.seek(ip,seekCount,cfg.seekPlaybackRate);
    std::uint64_t source=0,output=0;
    for(const auto& call:cuts.calls()){b.spend(128);need(call.inputBegin==source&&call.outputBegin==std::int64_t(output)&&call.inputFrames>0&&call.outputFrames>0&&call.inputFrames<=2048&&call.outputFrames<=2048,"CallConservation");
        for(int c=0;c<C;++c){for(int i=0;i<call.inputFrames;++i)in[std::size_t(c)][std::size_t(i)]=read(c,std::int64_t(source)+cfg.il+i);op[std::size_t(c)]=raw[std::size_t(c)].data()+output;}
        dsp.process(ip,int(call.inputFrames),op,int(call.outputFrames));source+=call.inputFrames;output+=call.outputFrames;}
    need(source==N&&output==D,"FinalN_D");for(int c=0;c<C;++c)op[std::size_t(c)]=raw[std::size_t(c)].data()+D;dsp.flush(op,cfg.w);
    auto physical=std::make_shared<AssetSamples>();physical->channels=C;physical->frames=D;physical->interleaved.resize(std::size_t(D)*std::size_t(C));
    const int offset=fault==Intervention::OmitOutputLatency?0:cfg.ol+(fault==Intervention::LatencyPlus1?1:0);
    need(offset>=0&&std::uint64_t(offset)+D<=D+std::uint64_t(cfg.w),"ExtractionDomain");
    for(std::uint64_t j=0;j<D;++j)for(int c=0;c<C;++c){b.spend(16);const auto local=fault==Intervention::ReverseAfter?D-1-j:j;float value=raw[std::size_t(c)][std::size_t(local)+std::size_t(offset)];if(fault==Intervention::Level1p1dB)value*=float(std::pow(10.0,1.1/20));need(std::isfinite(value),"NonfinitePhysicalOutput");physical->interleaved[std::size_t(j)*std::size_t(C)+std::size_t(c)]=value;}
    if(retainedRaw)*retainedRaw=std::move(raw);
    return physical;
}
} // namespace waveform_r1
