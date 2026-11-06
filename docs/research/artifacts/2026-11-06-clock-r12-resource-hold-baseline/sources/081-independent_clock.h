#pragma once

// Test-only independent Numeric A port. No engine/planner/renderer headers.
// Operation order follows the pinned Time.h, including its whole-Tick path.
// This is fixture-domain code, not a production uint64 renderer admission API.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace qc {
using Tick = std::int64_t;
inline constexpr Tick ppq = 15360;
inline constexpr Tick clipStart = 3 * ppq + 1;
inline constexpr Tick clipLength = 10 * ppq;
inline constexpr Tick jumpTick = clipStart + 4 * ppq + 1;
struct Change { Tick tick; double bpm; bool ramp; };
struct Native { Tick whole; double frac; };
struct Cut { bool admitted; std::int64_t floor; double low; double high; };

inline void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
inline std::vector<Change> fixtureMap(int kind) {
    require(kind >= 0 && kind < 9, "CLOCK_UNKNOWN_FIXTURE");
    const double constants[5] {120.0, 80.0, 60.0, 40.0, 30.0};
    if (kind < 5)
        return {{0,90.0,false}, {clipStart,constants[kind],false},
                {clipStart+clipLength,constants[kind],false}};
    const bool down = kind == 5 || kind == 7;
    const double first = down ? 120.0 : 30.0;
    const double last = down ? 30.0 : 120.0;
    if (kind < 7)
        return {{0,90.0,false}, {clipStart,first,true},
                {clipStart+clipLength,last,false}};
    return {{0,90.0,false}, {clipStart,first,false}, {jumpTick,last,false},
            {clipStart+clipLength,last,false}};
}

inline bool scaled(double a, double b, double c, double& out) {
    out = 0.0;
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c) || c == 0.0)
        return false;
    if (a == 0.0 || b == 0.0) return true;
    int ap=0, bp=0, cp=0;
    const double am=std::frexp(a,&ap), bm=std::frexp(b,&bp), cm=std::frexp(c,&cp);
    out=std::ldexp((am*bm)/cm,ap+bp-cp);
    return std::isfinite(out) && out != 0.0;
}
inline void checkFrame(double f) {
    require(std::isfinite(f) && f >= 0.0 && f < 0x1p63,"CLOCK_FRAME_DOMAIN");
}

// Proper uint64 rational -> adjacent enclosing binary64 values by long division.
// No independent conversion of numerator/denominator to floating point.
inline std::pair<double,double> fractionBounds(std::uint64_t numerator,
                                               std::uint64_t denominator) {
    require(denominator != 0 && numerator < denominator,"CLOCK_RATIONAL_DOMAIN");
    if (numerator == 0) return {0.0,0.0};
    std::uint64_t rem=numerator;
    const auto nextBit=[&]() {
        const bool bit=rem >= denominator-rem;
        rem=bit ? rem-(denominator-rem) : rem+rem;
        return bit;
    };
    int exponent=-1;
    while (!nextBit()) { --exponent; require(exponent >= -64,"CLOCK_LONG_DIVISION"); }
    std::uint64_t mantissa=1;
    for (int i=0;i<52;++i) mantissa=(mantissa<<1u) | (nextBit()?1u:0u);
    const double low=std::ldexp(static_cast<double>(mantissa),exponent-52);
    const double high=rem == 0 ? low : std::nextafter(low,1.0);
    require(low > 0.0 && low <= high && high <= 1.0,"CLOCK_FRACTION_ENCLOSURE");
    return {low,high};
}

class NumericAClock {
    double rate_;
    std::vector<Change> changes_;
    std::vector<double> prefix_;
    void append(std::size_t i, Tick segment, Tick delta, double& frame) const {
        require(segment > 0 && delta >= 0 && delta <= segment,"CLOCK_SEGMENT_DOMAIN");
        if (delta == 0) return;
        const double start=changes_[i].bpm;
        const double end=i+1 < changes_.size()?changes_[i+1].bpm:start;
        const double d=static_cast<double>(delta);
        const double k=60.0*rate_/static_cast<double>(ppq);
        if (!changes_[i].ramp || start == end) frame += d*k/start;
        else {
            const double slope=(end-start)/static_cast<double>(segment);
            require(slope != 0.0 && std::isfinite(slope),"CLOCK_RAMP_SLOPE");
            const double relative=slope*d/start;
            require(std::isfinite(relative) && relative > -1.0,"CLOCK_RAMP_DOMAIN");
            frame += (k/slope)*std::log1p(relative);
        }
        require(std::isfinite(frame),"CLOCK_PREFIX_DOMAIN");
    }
    std::size_t segmentFor(Tick whole) const {
        require(whole >= 0,"CLOCK_NEGATIVE_TICK");
        const auto upper=std::upper_bound(changes_.begin(),changes_.end(),whole,
            [](Tick value,const Change& c){return value<c.tick;});
        return static_cast<std::size_t>(upper-changes_.begin())-1u;
    }
public:
    NumericAClock(double rate, std::vector<Change> changes)
        :rate_(rate),changes_(std::move(changes)) {
        require(std::isfinite(rate_) && rate_ > 0.0 && !changes_.empty()
                && changes_[0].tick == 0,"CLOCK_CONFIGURATION");
        prefix_.assign(changes_.size(),0.0);
        for (std::size_t i=0;i<changes_.size();++i) {
            require(std::isfinite(changes_[i].bpm) && changes_[i].bpm > 0.0,
                    "CLOCK_BPM_DOMAIN");
            if (i != 0) {
                require(changes_[i].tick>changes_[i-1].tick,"CLOCK_MAP_ORDER");
                double f=prefix_[i-1];
                const Tick len=changes_[i].tick-changes_[i-1].tick;
                append(i-1,len,len,f);
                prefix_[i]=f;
            }
        }
    }
    double frame(Native t) const {
        require(t.whole >= 0 && t.frac >= 0.0 && t.frac < 1.0,"CLOCK_NATIVE_DOMAIN");
        const std::size_t i=segmentFor(t.whole);
        double result=prefix_[i];
        if (t.whole > changes_[i].tick) {
            const Tick end=i+1<changes_.size()?changes_[i+1].tick:t.whole;
            append(i,end-changes_[i].tick,t.whole-changes_[i].tick,result);
        }
        checkFrame(result);
        if (t.frac == 0.0) return result;
        const double k=60.0*rate_/static_cast<double>(ppq);
        double bpm=changes_[i].bpm, slope=0.0;
        if (i+1<changes_.size() && changes_[i].ramp && changes_[i+1].bpm != bpm) {
            const Tick length=changes_[i+1].tick-changes_[i].tick;
            const Tick local=t.whole-changes_[i].tick;
            slope=(changes_[i+1].bpm-bpm)/static_cast<double>(length);
            require(slope != 0.0 && std::isfinite(slope),"CLOCK_LOCAL_SLOPE");
            bpm=local<=length/2 ? bpm+slope*static_cast<double>(local)
                : changes_[i+1].bpm-slope*static_cast<double>(length-local);
        }
        require(std::isfinite(bpm) && bpm > 0.0,"CLOCK_LOCAL_BPM");
        double extra=0.0;
        if (slope == 0.0) require(scaled(t.frac,k,bpm,extra),"CLOCK_LOCAL_RATIO");
        else {
            double relative=0.0;
            require(scaled(slope,t.frac,bpm,relative) && relative > -1.0,
                    "CLOCK_LOCAL_LOG_DOMAIN");
            require(scaled(k,std::log1p(relative),slope,extra),"CLOCK_LOCAL_LOG_RATIO");
        }
        require(extra < 0x1p63-result,"CLOCK_FRAME_OVERFLOW");
        result += extra; checkFrame(result); return result;
    }
    Cut cut(Tick whole,std::uint64_t num,std::uint64_t den) const {
        const auto bounds=fractionBounds(num,den);
        const double lo=frame({whole,bounds.first});
        require(bounds.second != 1.0 || whole<std::numeric_limits<Tick>::max(),
                "CLOCK_CARRY_OVERFLOW");
        const double hi=bounds.second == 1.0 ? frame({whole+1,0.0})
                                             : frame({whole,bounds.second});
        const bool ok=hi>=lo && hi-lo<=1.0/1024.0 && std::floor(lo)==std::floor(hi);
        return {ok,ok?static_cast<std::int64_t>(std::floor(lo)):0,lo,hi};
    }
    Native inverse(double frameIn) const {
        checkFrame(frameIn);
        const double k=60.0*rate_/static_cast<double>(ppq);
        const auto upper=std::upper_bound(prefix_.begin(),prefix_.end(),frameIn);
        const std::size_t i=upper==prefix_.begin()?0u:
            static_cast<std::size_t>(upper-prefix_.begin())-1u;
        require(!(i>0 && frameIn==prefix_[i] && prefix_[i]==prefix_[i-1]),
                "CLOCK_INVERSE_PREFIX_AMBIGUOUS");
        const Tick origin=changes_[i].tick;
        const double local=frameIn-prefix_[i], bpm=changes_[i].bpm;
        double delta=0.0;
        if (i+1<changes_.size() && changes_[i].ramp && changes_[i+1].bpm != bpm) {
            const double slope=(changes_[i+1].bpm-bpm)
                /static_cast<double>(changes_[i+1].tick-origin);
            double exponent=0.0;
            require(slope != 0.0 && scaled(local,slope,k,exponent)
                && scaled(bpm,std::expm1(exponent),slope,delta),"CLOCK_INVERSE_RAMP");
        } else require(scaled(local,bpm,k,delta),"CLOCK_INVERSE_CONSTANT");
        require(std::isfinite(delta) && delta>=0.0 && delta<0x1p63,"CLOCK_INVERSE_DOMAIN");
        const double integral=std::floor(delta);
        const Tick whole=static_cast<Tick>(integral);
        require(whole<=std::numeric_limits<Tick>::max()-origin,"CLOCK_INVERSE_OVERFLOW");
        const Native out{origin+whole,delta-integral};
        require(i+1==changes_.size() || out.whole<changes_[i+1].tick,
                "CLOCK_INVERSE_SEGMENT");
        return out;
    }
};
} // namespace qc
