// Immutable Control-built beat index. Audio searches quantized onset buckets, never samples
// musical time once per output frame or walks a collapsed range of beat ordinals.
#pragma once

#include "engine/Time.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace yesdaw::engine {

namespace click_detail {

inline constexpr double unitRoundoff = 0x1p-53;
// Both denominators and the inflation factor are exact binary numbers. Inflation by8u
// covers the two roundings of ratio/product; constexpr also rules out deferred Audio init.
inline constexpr double gamma8 = (8.0 * unitRoundoff / (1.0 - 8.0 * unitRoundoff)) * (1.0 + 8.0 * unitRoundoff);
inline constexpr double gamma16 = (16.0 * unitRoundoff / (1.0 - 16.0 * unitRoundoff)) * (1.0 + 8.0 * unitRoundoff);

// Enclosures certify the click's NEW local arithmetic. Compiled prefix anchors and inherited
// ramp/log kernel outputs remain accepted binary64 inputs; this is not a real-log error bound.
struct Enclosure
{
    double low = 0.0, high = 0.0;
    bool numericFailure = false;
};

[[nodiscard]] inline Enclosure unknown() noexcept
{
    return { -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), true };
}

[[nodiscard]] inline bool comparisonTimeRatio (double a, double b, double c, double& out) noexcept
{
    const double product = a * b;
    // Interior normal products and normal quotients round identically to the normalized
    // power-of-two route. DBL_MIN itself can hide a subnormal half-ULP before division.
    // Preserve that boundary and all other exponent extremes through the original helper.
    if (std::isnormal (product) && std::abs (product) > std::numeric_limits<double>::min()
        && std::isnormal (c))
    {
        const double value = product / c;
        if (std::isnormal (value))
        {
            out = value;
            return true;
        }
    }
    return detail::scaledTimeRatio (a, b, c, out);
}

[[nodiscard]] inline Enclosure addPoint (double a, double b) noexcept
{
    const double value = a + b;
    if (! std::isfinite (value))
        return unknown();
    const double virtualB = value - a;
    const double residual = (a - (value - virtualB)) + (b - virtualB); // error-free TwoSum
    if (residual == 0.0)
        return { value, value, false };
    return residual < 0.0 ? Enclosure { std::nextafter (value, -INFINITY), value, false }
                          : Enclosure { value, std::nextafter (value, INFINITY), false };
}

[[nodiscard]] inline Enclosure productPoint (double a, double b) noexcept
{
    const double value = a * b;
    if (std::isnan (value))
        return unknown();
    if (! std::isfinite (value))
        return value > 0.0 ? Enclosure { std::numeric_limits<double>::max(), INFINITY, true }
                            : Enclosure { -INFINITY, -std::numeric_limits<double>::max(), true };
    if (a == 0.0 || b == 0.0)
        return { value, value, false };
    const double residual = std::fma (a, b, -value);
    // A binary64 product has at most 106 significant bits. Below this scale a nonzero
    // residual can underflow; conservatively bracket instead of calling that product exact.
    if (residual == 0.0 && std::abs (value) >= 0x1p-968)
        return { value, value, false };
    if (residual < 0.0)
        return { std::nextafter (value, -INFINITY), value, false };
    if (residual > 0.0)
        return { value, std::nextafter (value, INFINITY), false };
    return { std::nextafter (value, -INFINITY), std::nextafter (value, INFINITY), false };
}

[[nodiscard]] inline Enclosure quotientPoint (double a, double b) noexcept
{
    if (b == 0.0)
        return unknown();
    const double value = a / b;
    if (std::isnan (value))
        return unknown();
    if (! std::isfinite (value))
        return value > 0.0 ? Enclosure { std::numeric_limits<double>::max(), INFINITY, true }
                            : Enclosure { -INFINITY, -std::numeric_limits<double>::max(), true };
    if (a == 0.0)
        return { value, value, false };
    const double residual = std::fma (-value, b, a);
    if (residual == 0.0 && std::abs (a) >= 0x1p-968)
        return { value, value, false };
    if (residual != 0.0)
        return (residual < 0.0) == (b > 0.0)
            ? Enclosure { std::nextafter (value, -INFINITY), value, false }
            : Enclosure { value, std::nextafter (value, INFINITY), false };
    return { std::nextafter (value, -INFINITY), std::nextafter (value, INFINITY), false };
}

[[nodiscard]] inline Enclosure add (Enclosure a, Enclosure b) noexcept
{
    if (a.low == a.high && b.low == b.high)
    {
        auto out = addPoint (a.low, b.low);
        out.numericFailure = out.numericFailure || a.numericFailure || b.numericFailure;
        return out;
    }
    const auto low = addPoint (a.low, b.low), high = addPoint (a.high, b.high);
    return { low.low, high.high, a.numericFailure || b.numericFailure || low.numericFailure || high.numericFailure };
}

[[nodiscard]] inline Enclosure multiply (Enclosure a, Enclosure b) noexcept
{
    if (a.low == a.high && b.low == b.high)
    {
        auto out = productPoint (a.low, b.low);
        out.numericFailure = out.numericFailure || a.numericFailure || b.numericFailure;
        return out;
    }
    if (a.low >= 0.0 && b.low >= 0.0)
    {
        const auto low = productPoint (a.low, b.low), high = productPoint (a.high, b.high);
        return { low.low, high.high, a.numericFailure || b.numericFailure || low.numericFailure || high.numericFailure };
    }
    const Enclosure products[] { productPoint (a.low, b.low), productPoint (a.low, b.high),
                                productPoint (a.high, b.low), productPoint (a.high, b.high) };
    Enclosure out { products[0].low, products[0].high, a.numericFailure || b.numericFailure };
    for (const auto& product : products)
    {
        out.low = std::min (out.low, product.low);
        out.high = std::max (out.high, product.high);
        out.numericFailure = out.numericFailure || product.numericFailure;
    }
    return out;
}

[[nodiscard]] inline Enclosure divide (Enclosure a, Enclosure b) noexcept
{
    if (b.low <= 0.0 && b.high >= 0.0)
        return unknown();
    if (a.low == a.high && b.low == b.high)
    {
        auto out = quotientPoint (a.low, b.low);
        out.numericFailure = out.numericFailure || a.numericFailure || b.numericFailure;
        return out;
    }
    if (a.low >= 0.0 && b.low > 0.0)
    {
        const auto low = quotientPoint (a.low, b.high), high = quotientPoint (a.high, b.low);
        return { low.low, high.high, a.numericFailure || b.numericFailure || low.numericFailure || high.numericFailure };
    }
    const Enclosure quotients[] { quotientPoint (a.low, b.low), quotientPoint (a.low, b.high),
                                 quotientPoint (a.high, b.low), quotientPoint (a.high, b.high) };
    Enclosure out { quotients[0].low, quotients[0].high, a.numericFailure || b.numericFailure };
    for (const auto& quotient : quotients)
    {
        out.low = std::min (out.low, quotient.low);
        out.high = std::max (out.high, quotient.high);
        out.numericFailure = out.numericFailure || quotient.numericFailure;
    }
    return out;
}

[[nodiscard]] inline Enclosure integer (std::uint64_t value) noexcept
{
    // Both halves and the power-of-two scale are exact. TwoSum encloses the final large
    // integer conversion, including odd local Ticks above double's integer precision.
    return addPoint (static_cast<double> (value >> 32u) * 0x1p32,
                     static_cast<double> (value & 0xffffffffu));
}

[[nodiscard]] inline Enclosure nonnegative (Enclosure value) noexcept
{
    if (value.high < 0.0 || std::isnan (value.low) || std::isnan (value.high))
        return { 0.0, INFINITY, true };
    value.low = std::max (0.0, value.low);
    return value;
}

// Compare to the EXACT requested integer; converting an odd large request to double first
// would conflate adjacent native Frames. No cast is evaluated outside the native range.
[[nodiscard]] inline int compareScalar (double value, std::int64_t frame) noexcept
{
    if (value < 0.0)
        return -1;
    if (value >= 0x1p63)
        return 1;
    const auto whole = static_cast<std::int64_t> (value);
    if (whole < frame) return -1;
    if (whole > frame) return 1;
    return value == std::floor (value) ? 0 : 1;
}

enum class Order : std::uint8_t { Before, AtOrAfter, Uncertain };

[[nodiscard]] inline Order compare (Enclosure value, std::int64_t frame) noexcept
{
    if (compareScalar (value.high, frame) < 0) return Order::Before;
    if (compareScalar (value.low, frame) >= 0) return Order::AtOrAfter;
    return Order::Uncertain;
}

} // namespace click_detail

struct MusicalClickLookup
{
    TimeLookupStatus status = TimeLookupStatus::Ok;
    bool found = false;
    std::int64_t frame = 0;
    bool downbeat = false;
    // Work receipt for the bounded lookup, also usable without an Audio-thread timer/log.
    std::uint32_t segmentComparisons = 0;
    std::uint32_t ordinalProbes = 0;
    std::uint32_t roughProbes = 0, tightProbes = 0;
    std::uint32_t precisionComparisons = 0;
};

class MusicalClickMap
{
    struct LocatedOnset
    {
        MusicalClickLookup lookup;
        std::size_t segment = 0;
        std::uint64_t ordinal = 0;
    };

public:
    // Stack-only transport-window cursor. Bound the ordinal interval once, then reuse it
    // while jumping between distinct Frames; dense buckets do not repeat a full-domain search.
    class Window
    {
    public:
        [[nodiscard]] MusicalClickLookup next() noexcept;
        [[nodiscard]] const MusicalClickLookup& preparation() const noexcept { return preparation_; }

    private:
        friend class MusicalClickMap;
        const MusicalClickMap* map_ = nullptr;
        LocatedOnset pending_;
        MusicalClickLookup preparation_;
        std::int64_t cursor_ = 0, end_ = 0;
        std::size_t firstSegment_ = 0, lastSegment_ = 0;
        std::uint64_t firstOrdinal_ = 0, lastOrdinal_ = 0, ordinalStride_ = 0;
        bool hasPending_ = false, done_ = true, allowOrdinalHints_ = false;
    };

    [[nodiscard]] static TimeLookupStatus build (TempoMapView tempo, MeterMapView meter,
                                                SampleRate rate, MusicalClickMap& out)
    {
        out = MusicalClickMap {};
        if (! meter.empty())
        {
            if (meter.changes == nullptr || meter.changes[0].tick != 0 || ! meter.changes[0].isValid())
                return out.status_;
            for (std::size_t i = 1; i < meter.count; ++i)
                if (! meter.changes[i].isValid() || meter.changes[i].tick <= meter.changes[i - 1u].tick)
                    return out.status_;
        }
        MusicalClickMap prepared;
        CompiledTempoMap compiled;
        if (! CompiledTempoMap::build (tempo, rate, compiled))
        {
            double ignored = 0.0;
            out.status_ = compiled.frameForMusicalTime ({ 0, 0.0 }, ignored);
            return out.status_;
        }
        // Click ordinals need no bar numbering. Keep bar-index overflow/refusal on
        // its own Control index, without narrowing the accepted click map.
        (void) CompiledMeterMap::build (meter, prepared.meters_);
        prepared.rateHz_ = rate.hz;
        prepared.k_ = 60.0 * rate.hz / static_cast<double> (kTicksPerQuarter);
        if (! detail::isFinitePositive (prepared.k_))
        {
            out.status_ = TimeLookupStatus::NumericDomain;
            return out.status_;
        }
        const auto tempoCount = tempo.empty() ? 1u : tempo.count;
        prepared.tempo_.reserve (tempoCount);
        for (std::size_t i = 0; i < tempoCount; ++i)
        {
            const TempoChange change = tempo.empty() ? TempoChange {} : tempo.changes[i];
            double prefix = 0.0;
            if (! compiled.frameForTick (change.tick, prefix))
            {
                out.status_ = TimeLookupStatus::NumericDomain;
                return out.status_;
            }
            const bool haveNext = i + 1u < tempoCount;
            prepared.tempo_.push_back ({ change, prefix,
                haveNext ? tempo.changes[i + 1u].tick : 0,
                haveNext ? tempo.changes[i + 1u].bpm : change.bpm });
            auto& segment = prepared.tempo_.back();
            const bool ramp = segment.endTick > segment.change.tick
                && segment.change.curveToNext == TempoCurve::LinearRamp && segment.endBpm != segment.change.bpm;
            if (! ramp)
            {
                const auto scale = click_detail::quotientPoint (prepared.k_, segment.change.bpm);
                if (! scale.numericFailure && scale.low == scale.high && detail::isFinitePositive (scale.low))
                    segment.exactConstantScale = scale.low;
            }
        }
        for (std::size_t i = 1; i < prepared.tempo_.size(); ++i)
        {
            const auto& previous = prepared.tempo_[i - 1u];
            const auto& next = prepared.tempo_[i];
            const auto end = prepared.positionInTempo (previous,
                static_cast<std::uint64_t> (next.change.tick - previous.change.tick), 0, 1);
            const double low = std::floor (next.prefix);
            const double high = std::floor (std::max (next.prefix, end.high));
            if (high > low)
                prepared.joinBarriers_.push_back ({ low, high, false });
        }
        std::sort (prepared.joinBarriers_.begin(), prepared.joinBarriers_.end(),
            [] (const auto& a, const auto& b) { return a.low < b.low; });
        std::size_t merged = 0;
        for (const auto barrier : prepared.joinBarriers_)
            if (merged != 0 && barrier.low <= prepared.joinBarriers_[merged - 1u].high)
                prepared.joinBarriers_[merged - 1u].high = std::max (prepared.joinBarriers_[merged - 1u].high, barrier.high);
            else
                prepared.joinBarriers_[merged++] = barrier;
        prepared.joinBarriers_.resize (merged);
        prepared.segments_.reserve (meter.empty() ? 1u : meter.count);
        const auto count = meter.empty() ? 1u : meter.count;
        for (std::size_t i = 0; i < count; ++i)
        {
            const MeterChange change = meter.empty() ? MeterChange {} : meter.changes[i];
            const auto tickSpan = i + 1u < count
                ? static_cast<std::uint64_t> (meter.changes[i + 1u].tick - change.tick)
                : static_cast<std::uint64_t> (std::numeric_limits<Tick>::max() - change.tick) + 1u;
            std::uint64_t beats = 0, remainder = 0;
            if (! detail::meterMulDiv (tickSpan, change.denominator, 61440, beats, remainder))
            {
                out.status_ = TimeLookupStatus::Overflow;
                return out.status_;
            }
            beats += remainder != 0 ? 1u : 0u;
            Segment segment;
            segment.meter = change;
            segment.lastOrdinal = beats - 1u;
            segment.first = prepared.positionForOrdinal (segment, 0);
            segment.last = prepared.positionForOrdinal (segment, segment.lastOrdinal);
            // Keep the complete stored ordinal domain. An uncertain FLOOR at one ordinal
            // is not a monotonic failure and must never truncate later exact onsets.
            prepared.segments_.push_back (segment);
        }
        const auto& lastTempo = prepared.tempo_.back();
        prepared.domainEnd_ = prepared.positionInTempo (lastTempo,
            static_cast<std::uint64_t> (std::numeric_limits<Tick>::max() - lastTempo.change.tick) + 1u, 0, 1);
        prepared.compiled_ = std::move (compiled);
        prepared.status_ = TimeLookupStatus::Ok;
        out = std::move (prepared);
        return TimeLookupStatus::Ok;
    }

    // One result per distinct floored Frame. With S segments this performs at most
    // 2*ceil(log2(S+1))+2 segment comparisons and 262 enclosure evaluations: two 64-step
    // searches with rough+tight fallback, two tight endpoints and two accent comparisons
    // with fallback (256+2+4). Window preparation performs at most two such lookups.
    // The unambiguous no-fallback path remains 66. With B merged join barriers,
    // precision comparisons add at most
    // 2*(ceil(log2(B+1))+1). A W-frame window emits at most W results even at any BPM.
    [[nodiscard]] MusicalClickLookup nextOnset (std::int64_t minimumFrame) const noexcept
    {
        const auto last = segments_.empty() ? 0u : segments_.size() - 1u;
        return findOnset (minimumFrame, 0, 0, last, segments_.empty() ? 0u : segments_.back().lastOrdinal).lookup;
    }

    [[nodiscard]] Window onsetsInRange (std::int64_t firstFrame, std::int64_t endFrame) const noexcept;

    // CONTROL THREAD: transient arrange coordinates use the same accepted anchors and
    // local arithmetic certificates as count-in. No fractional coordinate is stored.
    [[nodiscard]] const CompiledTempoMap& tempoMap() const noexcept { return compiled_; }
    [[nodiscard]] const CompiledMeterMap& meterMap() const noexcept { return meters_; }

    [[nodiscard]] click_detail::Enclosure framePosition (Tick tick) const noexcept
    {
        if (tick < 0 || status_ != TimeLookupStatus::Ok) return click_detail::unknown();
        const auto& tempo = tempoAtTick (tick);
        return positionInTempo (tempo, static_cast<std::uint64_t> (tick - tempo.change.tick), 0, 1);
    }

    [[nodiscard]] TimeLookupStatus meterAtPosition (Tick nativeStart, bool musical, MeterChange& out) const noexcept
    {
        if (nativeStart < 0) return TimeLookupStatus::InvalidInput;
        if (status_ != TimeLookupStatus::Ok) return status_;
        if (musical)
        {
            const auto upper = std::upper_bound (segments_.begin(), segments_.end(), nativeStart,
                [] (Tick tick, const auto& segment) { return tick < segment.meter.tick; });
            if (upper == segments_.begin()) return TimeLookupStatus::InvalidMap;
            out = (upper - 1)->meter;
            return TimeLookupStatus::Ok;
        }
        std::size_t first = 0, last = segments_.size();
        while (first < last)
        {
            const auto mid = first + (last - first) / 2u;
            const auto position = segments_[mid].first;
            if (position.numericFailure) return TimeLookupStatus::NumericDomain;
            if (click_detail::compareScalar (position.high, nativeStart) <= 0) first = mid + 1u;
            else if (click_detail::compareScalar (position.low, nativeStart) > 0) last = mid;
            else return TimeLookupStatus::NumericDomain;
        }
        if (first == 0) return TimeLookupStatus::InvalidMap;
        out = segments_[first - 1u].meter;
        return TimeLookupStatus::Ok;
    }

    [[nodiscard]] TimeLookupStatus frameDisplacement (Tick nativeStart, bool musical,
        std::int64_t numerator, std::uint64_t denominator, click_detail::Enclosure& displacement,
        bool& beforeOrigin) const noexcept
    {
        using namespace click_detail;
        displacement = {};
        beforeOrigin = false;
        if (nativeStart < 0 || denominator == 0 || denominator > static_cast<std::uint64_t> (INT64_MAX))
            return TimeLookupStatus::InvalidInput;
        if (status_ != TimeLookupStatus::Ok) return status_;
        if (numerator == 0) return TimeLookupStatus::Ok;
        MusicalTime start { nativeStart, 0.0 };
        Enclosure accepted;
        if (musical)
            accepted = framePosition (nativeStart);
        else
        {
            accepted = integer (static_cast<std::uint64_t> (nativeStart));
            if (accepted.low != accepted.high) return TimeLookupStatus::NumericDomain;
            std::uint32_t comparisons = 0;
            if (inJoinBarrier (nativeStart, comparisons)) return TimeLookupStatus::NumericDomain;
            const auto status = compiled_.musicalTimeForFrame (accepted.low, start);
            if (status != TimeLookupStatus::Ok) return status;
        }
        Enclosure destination;
        const auto status = spanDestination (start, accepted, numerator, denominator, destination,
                                             &displacement, beforeOrigin, musical);
        if (status != TimeLookupStatus::Ok || beforeOrigin) return status;
        if (displacement.numericFailure || (displacement.low <= 0.0 && displacement.high >= 0.0))
            return TimeLookupStatus::NumericDomain;
        return TimeLookupStatus::Ok;
    }

    [[nodiscard]] TimeLookupStatus tickAtFramePosition (click_detail::Enclosure position, Tick& out) const noexcept
    {
        out = 0;
        if (status_ != TimeLookupStatus::Ok) return status_;
        if (position.numericFailure || position.low < 0.0 || ! std::isfinite (position.high))
            return TimeLookupStatus::NumericDomain;
        MusicalTime approximate;
        const auto status = compiled_.musicalTimeForFrame (
            position.low + (position.high - position.low) * 0.5, approximate);
        if (status != TimeLookupStatus::Ok) return status;
        Tick candidate = approximate.tick;
        if (approximate.frac >= 0.5)
        {
            if (candidate == INT64_MAX) return TimeLookupStatus::Overflow;
            ++candidate;
        }
        // The inverse supplies only a candidate. Certify its nearest-Tick cell through
        // forward half-Tick boundaries, with ties toward the later destination.
        const auto half = [&] (Tick tick) {
            const auto& tempo = tempoAtTick (tick);
            return positionInTempo (tempo, static_cast<std::uint64_t> (tick - tempo.change.tick), 1, 2);
        };
        for (int adjustment = 0; adjustment < 3; ++adjustment)
        {
            if (candidate > 0)
            {
                const auto lower = half (candidate - 1);
                if (lower.numericFailure) return TimeLookupStatus::NumericDomain;
                if (position.high < lower.low) { --candidate; continue; }
                if (position.low < lower.high) return TimeLookupStatus::NumericDomain;
            }
            const auto upper = half (candidate);
            if (upper.numericFailure) return TimeLookupStatus::NumericDomain;
            if (position.low >= upper.high)
            {
                if (candidate == INT64_MAX) return TimeLookupStatus::Overflow;
                ++candidate;
                continue;
            }
            if (position.high >= upper.low) return TimeLookupStatus::NumericDomain;
            out = candidate;
            return TimeLookupStatus::Ok;
        }
        return TimeLookupStatus::NumericDomain;
    }
    // CONTROL THREAD: accepted fractional cursor plus one EXACT nominal starting-Meter
    // bar. Certify the new rational addition and ceil against the same binary64 prefix/log
    // anchors as click lookup; an uncertain destination refuses instead of rounding twice.
    [[nodiscard]] TimeLookupStatus countInEndFrame (std::int64_t startFrame, std::int64_t& out) const noexcept
    {
        using namespace click_detail;
        out = 0;
        if (startFrame < 0) return TimeLookupStatus::InvalidInput;
        if (status_ != TimeLookupStatus::Ok) return status_;
        const auto accepted = integer (static_cast<std::uint64_t> (startFrame));
        if (accepted.low != accepted.high) return TimeLookupStatus::NumericDomain;
        std::uint32_t comparisons = 0;
        if (inJoinBarrier (startFrame, comparisons)) return TimeLookupStatus::NumericDomain;
        MusicalTime start;
        const auto status = compiled_.musicalTimeForFrame (accepted.low, start);
        if (status != TimeLookupStatus::Ok) return status;
        // Select the starting Meter against its actual fractional Frame, not a floored
        // onset bucket or the rounded inverse Tick. A future sub-Frame change is still future.
        std::size_t first = 0, last = segments_.size();
        while (first < last)
        {
            const auto mid = first + (last - first) / 2u;
            const auto position = segments_[mid].first;
            if (position.numericFailure) return TimeLookupStatus::NumericDomain;
            if (compareScalar (position.high, startFrame) <= 0) first = mid + 1u;
            else if (compareScalar (position.low, startFrame) > 0) last = mid;
            else return TimeLookupStatus::NumericDomain;
        }
        if (first == 0) return TimeLookupStatus::InvalidMap;
        const auto& meter = segments_[first - 1u].meter;
        Enclosure position;
        bool beforeOrigin = false;
        const auto spanStatus = spanDestination (start, accepted, 61440LL * meter.numerator,
                                                 meter.denominator, position, nullptr, beforeOrigin, false);
        if (spanStatus != TimeLookupStatus::Ok) return spanStatus;
        if (position.numericFailure) return TimeLookupStatus::NumericDomain;
        if (position.low >= 0x1p63) return TimeLookupStatus::Overflow;
        if (position.low < 0.0 || position.high >= 0x1p63) return TimeLookupStatus::NumericDomain;
        const double low = std::ceil (position.low), high = std::ceil (position.high);
        if (low != high || high >= 0x1p63) return TimeLookupStatus::NumericDomain;
        const auto opening = static_cast<std::int64_t> (low);
        if (opening <= startFrame || inJoinBarrier (opening, comparisons)) return TimeLookupStatus::NumericDomain;
        out = opening;
        return TimeLookupStatus::Ok;
    }

    // CONTROL THREAD: cheap preflight for ambiguity already established at map joins.
    // This does not certify all onset floors; later arithmetic lookup refusals stay named.
    [[nodiscard]] TimeLookupStatus checkCountInRange (std::int64_t firstFrame, std::int64_t endFrame) const noexcept
    {
        if (firstFrame < 0 || endFrame < firstFrame) return TimeLookupStatus::InvalidInput;
        if (status_ != TimeLookupStatus::Ok) return status_;
        if (firstFrame == endFrame) return TimeLookupStatus::Ok;
        std::size_t low = 0, high = joinBarriers_.size();
        while (low < high)
        {
            const auto mid = low + (high - low) / 2u;
            if (click_detail::compareScalar (joinBarriers_[mid].high, firstFrame) < 0) low = mid + 1u;
            else high = mid;
        }
        return low < joinBarriers_.size() && click_detail::compareScalar (joinBarriers_[low].low, endFrame) < 0
            ? TimeLookupStatus::NumericDomain : TimeLookupStatus::Ok;
    }

    [[nodiscard]] std::size_t segmentCount() const noexcept { return segments_.size(); }

private:
    using Enclosure = click_detail::Enclosure;

    struct TempoSegment
    {
        TempoChange change;
        double prefix = 0.0; // accepted compiled binary64 anchor, not a real-prefix enclosure
        Tick endTick = 0;
        double endBpm = 120.0;
        double exactConstantScale = 0.0;
    };

    struct Segment
    {
        MeterChange meter;
        std::uint64_t lastOrdinal = 0;
        Enclosure first, last;
    };

    [[nodiscard]] TimeLookupStatus spanDestination (MusicalTime start, Enclosure accepted,
        std::int64_t numerator, std::uint64_t denominator, Enclosure& position, Enclosure* displacement,
        bool& beforeOrigin, bool nativeTickOrigin) const noexcept
    {
        using namespace click_detail;
        beforeOrigin = false;
        if (denominator == 0 || denominator > static_cast<std::uint64_t> (INT64_MAX))
            return TimeLookupStatus::InvalidInput;
        const auto signedInteger = [] (std::int64_t value) {
            const auto magnitude = value < 0 ? std::uint64_t { 0 } - static_cast<std::uint64_t> (value)
                                             : static_cast<std::uint64_t> (value);
            const auto point = integer (magnitude);
            return value < 0 ? Enclosure { -point.high, -point.low, point.numericFailure } : point;
        };
        const auto whole = numerator / static_cast<std::int64_t> (denominator);
        const auto remainder = numerator % static_cast<std::int64_t> (denominator);
        const auto fractionalSpan = divide (signedInteger (remainder), integer (denominator));
        if (whole > INT64_MAX - start.tick) return TimeLookupStatus::Overflow;
        if (whole < -start.tick) { beforeOrigin = true; return TimeLookupStatus::Ok; }
        Tick destination = start.tick + whole;
        auto fraction = add ({ start.frac, start.frac, false }, fractionalSpan);
        if (fraction.high < 0.0)
        {
            if (destination == 0) { beforeOrigin = true; return TimeLookupStatus::Ok; }
            --destination;
            fraction = add (fraction, { 1.0, 1.0, false });
        }
        if (fraction.low >= 1.0)
        {
            if (destination == INT64_MAX) return TimeLookupStatus::Overflow;
            ++destination;
            fraction = add (fraction, { -1.0, -1.0, false });
        }
        if (fraction.numericFailure || fraction.low < 0.0 || fraction.high >= 1.0)
            return TimeLookupStatus::NumericDomain;
        const auto& tempo = tempoAtTick (destination);
        const auto& initialTempo = tempoAtTick (start.tick);
        const auto constant = [] (const TempoSegment& segment) noexcept {
            return segment.endTick <= segment.change.tick || segment.change.curveToNext != TempoCurve::LinearRamp
                || segment.endBpm == segment.change.bpm;
        };
        const auto spanTicks = add (signedInteger (whole), fractionalSpan);
        if (constant (initialTempo) && constant (tempo))
        {
            const auto deltaTicks = signedInteger (tempo.change.tick - initialTempo.change.tick);
            const auto remainingTicks = add (spanTicks, { -deltaTicks.high, -deltaTicks.low, deltaTicks.numericFailure });
            const auto startOffset = add (accepted, { -initialTempo.prefix, -initialTempo.prefix, false });
            const auto initialScaled = multiply (startOffset, { initialTempo.change.bpm, initialTempo.change.bpm, false });
            const auto localTicks = add (divide (initialScaled, { k_, k_, false }), remainingTicks);
            if (localTicks.numericFailure || localTicks.low < 0.0
                || (tempo.endTick > tempo.change.tick
                    && compareScalar (localTicks.high, tempo.endTick - tempo.change.tick) >= 0))
                return TimeLookupStatus::NumericDomain;
            position = add ({ tempo.prefix, tempo.prefix, false },
                divide (add (initialScaled, multiply (remainingTicks, { k_, k_, false })),
                        { tempo.change.bpm, tempo.change.bpm, false }));
            if (displacement != nullptr)
            {
                if (&tempo == &initialTempo)
                    *displacement = divide (multiply (spanTicks, { k_, k_, false }),
                                             { tempo.change.bpm, tempo.change.bpm, false });
                else
                    *displacement = add (add (addPoint (tempo.prefix, -initialTempo.prefix),
                        multiply (startOffset, divide (addPoint (initialTempo.change.bpm, -tempo.change.bpm),
                                                       { tempo.change.bpm, tempo.change.bpm, false }))),
                        divide (multiply (remainingTicks, { k_, k_, false }),
                                { tempo.change.bpm, tempo.change.bpm, false }));
            }
        }
        else
        {
            position = positionInTempoFraction (tempo, static_cast<std::uint64_t> (destination - tempo.change.tick),
                                                fraction, fraction.low + (fraction.high - fraction.low) * 0.5);
            if (displacement != nullptr)
            {
                auto localTempo = tempo;
                localTempo.prefix = 0.0;
                const auto endLocal = positionInTempoFraction (localTempo,
                    static_cast<std::uint64_t> (destination - tempo.change.tick), fraction,
                    fraction.low + (fraction.high - fraction.low) * 0.5);
                auto startLocal = add (accepted, { -initialTempo.prefix, -initialTempo.prefix, false });
                if (nativeTickOrigin)
                {
                    localTempo = initialTempo;
                    localTempo.prefix = 0.0;
                    startLocal = positionInTempoFraction (localTempo,
                        static_cast<std::uint64_t> (start.tick - initialTempo.change.tick), {}, 0.0);
                }
                *displacement = add (add (addPoint (tempo.prefix, -initialTempo.prefix), endLocal),
                    { -startLocal.high, -startLocal.low, startLocal.numericFailure });
            }
        }
        return position.numericFailure ? TimeLookupStatus::NumericDomain : TimeLookupStatus::Ok;
    }

    [[nodiscard]] Enclosure positionInTempo (const TempoSegment& tempo, std::uint64_t localWhole,
                                             std::uint32_t numerator, std::uint32_t denominator) const noexcept
    {
        return positionInTempoFraction (tempo, localWhole,
            click_detail::quotientPoint (static_cast<double> (numerator), static_cast<double> (denominator)),
            static_cast<double> (numerator) / denominator);
    }

    [[nodiscard]] Enclosure positionInTempoFraction (const TempoSegment& tempo, std::uint64_t localWhole,
                                                     Enclosure fraction, double nominalFraction) const noexcept
    {
        using namespace click_detail;
        const Enclosure k { k_, k_, false };
        const Enclosure bpm { tempo.change.bpm, tempo.change.bpm, false };
        Enclosure whole, extra;
        const bool ramp = tempo.endTick > tempo.change.tick
            && tempo.change.curveToNext == TempoCurve::LinearRamp && tempo.endBpm != tempo.change.bpm;
        if (! ramp)
        {
            if (tempo.exactConstantScale != 0.0)
            {
                const Enclosure scale { tempo.exactConstantScale, tempo.exactConstantScale, false };
                whole = multiply (integer (localWhole), scale);
                if (fraction.high != 0.0)
                    extra = multiply (fraction, scale);
            }
            else
            {
                whole = divide (multiply (integer (localWhole), k), bpm);
                if (fraction.high != 0.0)
                    extra = divide (multiply (fraction, k), bpm);
            }
        }
        else
        {
            const Tick length = tempo.endTick - tempo.change.tick;
            double local = 0.0;
            if (! detail::appendTempoSegmentFrames (rateHz_, length, static_cast<Tick> (localWhole),
                tempo.change.bpm, tempo.endBpm, TempoCurve::LinearRamp, local))
                return { 0.0, INFINITY, true };
            // The inherited ramp kernel output is an accepted input. Its real-log error
            // and the compiled cumulative anchors are deliberately outside this certificate.
            whole = { local, local, false };
            if (fraction.high != 0.0)
            {
                const double slope = (tempo.endBpm - tempo.change.bpm) / static_cast<double> (length);
                const Tick tick = static_cast<Tick> (localWhole);
                const double localBpm = tick <= length / 2
                    ? tempo.change.bpm + slope * static_cast<double> (tick)
                    : tempo.endBpm - slope * static_cast<double> (length - tick);
                if (slope == 0.0 || ! std::isfinite (slope) || ! detail::isFinitePositive (localBpm))
                    return { 0.0, INFINITY, true };
                const Enclosure slopePoint { slope, slope, false }, bpmPoint { localBpm, localBpm, false };
                const auto relative = divide (multiply (slopePoint, fraction), bpmPoint);
                double nominal = 0.0;
                if (relative.numericFailure || ! std::isfinite (relative.low) || ! std::isfinite (relative.high)
                    || ! detail::scaledTimeRatio (slope, nominalFraction,
                                                 localBpm, nominal))
                    return { 0.0, INFINITY, true };
                const double minimum = std::min (relative.low, nominal);
                const auto minimumDomain = addPoint (1.0, minimum);
                if (minimumDomain.low <= 0.0)
                    return { 0.0, INFINITY, true };
                const auto below = addPoint (relative.low, -nominal), above = addPoint (relative.high, -nominal);
                const double distance = std::max ({ std::abs (below.low), std::abs (below.high),
                                                    std::abs (above.low), std::abs (above.high) });
                const auto radius = quotientPoint (distance, minimumDomain.low);
                const double logAnchor = std::log1p (nominal); // inherited binary64 log output
                if (! std::isfinite (logAnchor) || ! std::isfinite (radius.high))
                    return { 0.0, INFINITY, true };
                // Mean-value bound for the NEW rational-input perturbation about that accepted
                // log anchor. No guessed libm ULP allowance or true-real log claim is introduced.
                const auto logarithm = add ({ logAnchor, logAnchor, false },
                                             { -radius.high, radius.high, radius.numericFailure });
                extra = divide (multiply (k, logarithm), slopePoint);
            }
            const auto input = integer (localWhole);
            const double nominalTick = static_cast<double> (localWhole);
            const auto below = addPoint (input.low, -nominalTick), above = addPoint (input.high, -nominalTick);
            const double distance = std::max ({ std::abs (below.low), std::abs (below.high),
                                                std::abs (above.low), std::abs (above.high) });
            if (distance != 0.0)
            {
                const double minimumBpm = std::min (tempo.change.bpm, tempo.endBpm);
                const auto radius = divide (multiply ({ distance, distance, false }, k),
                                            { minimumBpm, minimumBpm, false });
                // Bound only the new integer-conversion perturbation about the inherited
                // ramp anchor, using its maximum positive derivative across this segment.
                whole = add (whole, { -radius.high, radius.high, radius.numericFailure });
            }
        }
        return nonnegative (add (add ({ tempo.prefix, tempo.prefix, false }, whole), extra));
    }

    [[nodiscard]] Enclosure positionForOrdinal (const Segment& segment, std::uint64_t ordinal) const noexcept
    {
        std::uint64_t whole = 0, remainder = 0;
        if (! detail::meterMulDiv (ordinal, 61440, segment.meter.denominator, whole, remainder)
            || whole > static_cast<std::uint64_t> (std::numeric_limits<Tick>::max() - segment.meter.tick))
            return { 0.0, INFINITY, true };
        const Tick tick = segment.meter.tick + static_cast<Tick> (whole);
        const auto& tempo = tempoAtTick (tick);
        return positionInTempo (tempo, static_cast<std::uint64_t> (tick - tempo.change.tick),
                               static_cast<std::uint32_t> (remainder), segment.meter.denominator);
    }

    [[nodiscard]] const TempoSegment& tempoAtTick (Tick tick) const noexcept
    {
        if (tick >= tempo_.back().change.tick)
            return tempo_.back();
        if (tempo_.size() > 1u && tick >= tempo_[tempo_.size() - 2u].change.tick)
            return tempo_[tempo_.size() - 2u];
        const auto upper = std::upper_bound (tempo_.begin(), tempo_.end(), tick,
            [] (Tick value, const TempoSegment& tempo) noexcept { return value < tempo.change.tick; });
        return *(upper - 1);
    }

    [[nodiscard]] Enclosure roughPositionForOrdinal (const Segment& segment, std::uint64_t ordinal) const noexcept
    {
        using namespace click_detail;
        std::uint64_t whole = 0, remainder = 0;
        if (! detail::meterMulDiv (ordinal, 61440, segment.meter.denominator, whole, remainder)
            || whole > static_cast<std::uint64_t> (std::numeric_limits<Tick>::max() - segment.meter.tick))
            return { 0.0, INFINITY, true };
        const Tick tick = segment.meter.tick + static_cast<Tick> (whole);
        const auto& tempo = tempoAtTick (tick);
        const auto localWhole = static_cast<std::uint64_t> (tick - tempo.change.tick);
        const double local = static_cast<double> (localWhole);
        const double fraction = static_cast<double> (remainder) / segment.meter.denominator;
        double localFrame = 0.0, extra = 0.0, perturbation = 0.0;
        const bool ramp = tempo.endTick > tempo.change.tick
            && tempo.change.curveToNext == TempoCurve::LinearRamp && tempo.endBpm != tempo.change.bpm;
        if (! ramp)
        {
            if (tempo.exactConstantScale != 0.0)
            {
                localFrame = local * tempo.exactConstantScale;
                extra = fraction * tempo.exactConstantScale;
                if ((local != 0.0 && localFrame == 0.0) || (fraction != 0.0 && extra == 0.0))
                    return { 0.0, INFINITY, true };
            }
            else if (! comparisonTimeRatio (local, k_, tempo.change.bpm, localFrame)
                || ! comparisonTimeRatio (fraction, k_, tempo.change.bpm, extra))
                return { 0.0, INFINITY, true };
        }
        else
        {
            const Tick length = tempo.endTick - tempo.change.tick;
            if (! detail::appendTempoSegmentFrames (rateHz_, length, static_cast<Tick> (localWhole),
                tempo.change.bpm, tempo.endBpm, TempoCurve::LinearRamp, localFrame))
                return { 0.0, INFINITY, true };
            if (remainder != 0)
            {
                const double slope = (tempo.endBpm - tempo.change.bpm) / static_cast<double> (length);
                const Tick localTick = static_cast<Tick> (localWhole);
                const double bpm = localTick <= length / 2
                    ? tempo.change.bpm + slope * static_cast<double> (localTick)
                    : tempo.endBpm - slope * static_cast<double> (length - localTick);
                double relative = 0.0;
                if (slope == 0.0 || ! detail::isFinitePositive (bpm)
                    || ! comparisonTimeRatio (slope, fraction, bpm, relative)
                    || ! std::isnormal (relative))
                    return { 0.0, INFINITY, true };
                const double relativeError = std::nextafter (gamma8 * std::abs (relative), INFINITY);
                const double domain = std::nextafter (1.0 + std::nextafter (relative - relativeError, -INFINITY), -INFINITY);
                if (! std::isnormal (relativeError) || domain <= 0.0)
                    return { 0.0, INFINITY, true };
                const double logarithm = std::log1p (relative); // accepted inherited log input
                double radius = 0.0;
                if (! comparisonTimeRatio (k_, logarithm, slope, extra)
                    || ! comparisonTimeRatio (k_, relativeError, std::abs (slope), radius))
                    return { 0.0, INFINITY, true };
                perturbation = std::nextafter ((radius / domain) * (1.0 + gamma8), INFINITY);
            }
            if (localWhole > (std::uint64_t { 1 } << 53))
            {
                const double tickError = std::nextafter (gamma8 * std::abs (local), INFINITY);
                double radius = 0.0;
                if (! comparisonTimeRatio (tickError, k_, std::min (tempo.change.bpm, tempo.endBpm), radius))
                    return { 0.0, INFINITY, true };
                perturbation = std::nextafter (perturbation + radius * (1.0 + gamma8), INFINITY);
            }
        }
        const auto normalOrZero = [] (double value) noexcept { return value == 0.0 || std::isnormal (value); };
        if (! normalOrZero (tempo.prefix) || ! normalOrZero (localFrame) || ! normalOrZero (extra)
            || ! normalOrZero (perturbation))
            return { 0.0, INFINITY, true };
        const double center = (tempo.prefix + localFrame) + extra;
        const double scale = std::abs (tempo.prefix) + std::abs (localFrame) + std::abs (extra)
            + std::abs (perturbation) + 1.0;
        if (! std::isfinite (center) || ! std::isfinite (scale))
            return { 0.0, INFINITY, true };
        // Normal arithmetic only: integer conversion, rational division and normalized
        // multiply/divide need <=4 rounds per local term; final sums, scale and radius
        // construction bring the longest path below16. gamma16 bounds those errors.
        // gamma8 separately bounds <=4 rounds in the rational-input/derivative radius.
        // Absolute sums avoid cancellation; non-normal/overflow paths always use tight fallback.
        // No error allowance for an inherited prefix or libm output is invented here.
        const double error = std::nextafter (gamma16 * scale + perturbation, INFINITY);
        if (! std::isfinite (error))
            return { 0.0, INFINITY, true };
        return nonnegative ({ std::nextafter (center - error, -INFINITY),
                              std::nextafter (center + error, INFINITY), false });
    }

    [[nodiscard]] click_detail::Order comparisonForOrdinal (const Segment& segment, std::uint64_t ordinal,
                                                            std::int64_t frame, MusicalClickLookup& work) const noexcept
    {
        ++work.ordinalProbes; ++work.roughProbes;
        const auto rough = click_detail::compare (roughPositionForOrdinal (segment, ordinal), frame);
        if (rough != click_detail::Order::Uncertain)
            return rough;
        ++work.ordinalProbes; ++work.tightProbes;
        return click_detail::compare (positionForOrdinal (segment, ordinal), frame);
    }

    [[nodiscard]] Enclosure countedPositionForOrdinal (const Segment& segment, std::uint64_t ordinal,
                                                       MusicalClickLookup& work) const noexcept
    {
        ++work.ordinalProbes; ++work.tightProbes;
        if (ordinal == 0) return segment.first;
        if (ordinal == segment.lastOrdinal) return segment.last;
        return positionForOrdinal (segment, ordinal);
    }

    [[nodiscard]] static TimeLookupStatus certifyFloor (Enclosure position, std::int64_t& frame) noexcept
    {
        frame = 0;
        if (position.numericFailure)
            return TimeLookupStatus::NumericDomain;
        if (position.low >= 0x1p63)
            return TimeLookupStatus::Overflow;
        if (position.low < 0.0 || position.high >= 0x1p63)
            return TimeLookupStatus::NumericDomain;
        const auto low = static_cast<std::int64_t> (position.low);
        const auto high = static_cast<std::int64_t> (position.high);
        if (low != high)
            return TimeLookupStatus::NumericDomain;
        frame = low;
        return TimeLookupStatus::Ok;
    }

    [[nodiscard]] LocatedOnset findOnset (std::int64_t minimumFrame,
                                         std::size_t firstIndex, std::uint64_t firstOrdinal,
                                         std::size_t lastIndex, std::uint64_t lastOrdinal,
                                         std::uint64_t ordinalStride = 0) const noexcept
    {
        using click_detail::Order;
        LocatedOnset located;
        auto& result = located.lookup;
        if (minimumFrame < 0)
        {
            result.status = TimeLookupStatus::InvalidInput;
            return located;
        }
        if (status_ != TimeLookupStatus::Ok || segments_.empty())
        {
            result.status = status_ == TimeLookupStatus::Ok ? TimeLookupStatus::InvalidMap : status_;
            return located;
        }
        if (inJoinBarrier (minimumFrame, result.precisionComparisons))
        {
            result.status = TimeLookupStatus::NumericDomain;
            return located;
        }
        bool uncertainSegments = false;
        std::size_t low = firstIndex, high = lastIndex + 1u;
        while (low < high)
        {
            const auto mid = low + (high - low) / 2u;
            ++result.segmentComparisons;
            const auto order = click_detail::compare (segments_[mid].last, minimumFrame);
            if (order == Order::Before) low = mid + 1u;
            else
            {
                high = mid;
                uncertainSegments = uncertainSegments || order == Order::Uncertain;
            }
        }
        const auto possibleSegment = low;
        if (possibleSegment > lastIndex)
        {
            if (click_detail::compareScalar (domainEnd_.high, minimumFrame) <= 0)
                result.status = TimeLookupStatus::Overflow;
            else if (click_detail::compareScalar (domainEnd_.low, minimumFrame) <= 0)
                result.status = TimeLookupStatus::NumericDomain;
            return located;
        }
        if (uncertainSegments)
        {
            low = possibleSegment; high = lastIndex + 1u;
            while (low < high)
            {
                const auto mid = low + (high - low) / 2u;
                ++result.segmentComparisons;
                const auto order = click_detail::compare (segments_[mid].last, minimumFrame);
                if (order == Order::AtOrAfter) high = mid;
                else low = mid + 1u;
            }
            if (low > lastIndex)
            {
                result.status = TimeLookupStatus::NumericDomain;
                return located;
            }
        }
        const auto firstSegment = low;
        const auto& segment = segments_[firstSegment];
        const auto ordinalBegin = firstSegment == firstIndex ? firstOrdinal : 0u;
        const auto ordinalEnd = firstSegment == lastIndex ? lastOrdinal : segment.lastOrdinal;
        std::uint64_t first = ordinalBegin, last = ordinalEnd;
        Order originOrder = Order::Before;
        if (first == 0)
        {
            ++result.segmentComparisons;
            originOrder = click_detail::compare (segment.first, minimumFrame);
        }
        bool uncertainOrdinals = originOrder == Order::Uncertain;
        if (originOrder != Order::AtOrAfter)
        {


            unsigned remaining = 64, hintComparisons = 0;
            bool haveHint = firstSegment == firstIndex && ordinalStride != 0
                && ordinalStride <= std::numeric_limits<std::uint64_t>::max() - firstOrdinal;
            std::uint64_t hint = haveHint ? firstOrdinal + ordinalStride : 0;
            while (first < last)
            {
                auto mid = first + (last - first) / 2u;
                if (haveHint)
                {
                    const auto capacity = std::uint64_t { 1 } << (remaining - 1u);


                    if (hint >= first && hint < last
                        && hint - first < capacity && last - hint <= capacity)
                    {
                        mid = hint;
                        ++hintComparisons;
                    }
                    else
                        haveHint = false;
                }
                --remaining;
                const auto order = comparisonForOrdinal (segment, mid, minimumFrame, result);
                if (order == Order::Before) first = mid + 1u;
                else
                {
                    last = mid;
                    uncertainOrdinals = uncertainOrdinals || order == Order::Uncertain;
                }
                if (haveHint)
                {
                    if (hintComparisons == 3u)
                        haveHint = false;
                    else if (first < last)
                        hint = order == Order::Before ? first : last - 1u;
                }
            }
        }
        const auto possibleOrdinal = first;
        auto position = countedPositionForOrdinal (segment, first, result);
        auto order = click_detail::compare (position, minimumFrame);
        if (order == Order::Before && firstSegment == lastIndex && first == ordinalEnd)
            return located;
        uncertainOrdinals = uncertainOrdinals || order == Order::Uncertain;
        if (uncertainOrdinals)
        {
            // Two one-sided searches bracket the first real ordinal. An uncertain midpoint
            // need not have a certifiable floor and is never assumed to end the clock.
            first = possibleOrdinal; last = ordinalEnd;
            while (first < last)
            {
                const auto mid = first + (last - first) / 2u;
                const auto comparison = comparisonForOrdinal (segment, mid, minimumFrame, result);
                if (comparison == Order::AtOrAfter) last = mid;
                else first = mid + 1u;
            }
            position = countedPositionForOrdinal (segment, first, result);
            order = click_detail::compare (position, minimumFrame);
        }
        if (order != Order::AtOrAfter)
        {
            result.status = TimeLookupStatus::NumericDomain;
            return located;
        }
        result.status = certifyFloor (position, result.frame);
        if (result.status == TimeLookupStatus::Overflow)
        {
            result.status = TimeLookupStatus::Ok; // next onset lies wholly beyond native Frames
            return located;
        }
        if (result.status != TimeLookupStatus::Ok)
            return located;
        if (inJoinBarrier (result.frame, result.precisionComparisons))
        {
            result.status = TimeLookupStatus::NumericDomain;
            return located;
        }
        if ((firstSegment != possibleSegment || first != possibleOrdinal) && result.frame != minimumFrame)
        {
            result.status = TimeLookupStatus::NumericDomain;
            return located;
        }

        // A definite onset in the requested Frame proves bucket existence even when several
        // sub-Frame ordinals at its left boundary are ambiguous. Only unresolved accent
        // distinctions matter: one proven downbeat is sufficient for same-Frame priority.
        result.downbeat = first % segment.meter.numerator == 0;
        bool uncertainAccent = false;
        if (! result.downbeat)
        {
            const auto untilDownbeat = segment.meter.numerator - first % segment.meter.numerator;
            if (untilDownbeat <= ordinalEnd - first)
            {
                const auto accent = comparisonForOrdinal (segment, first + untilDownbeat, result.frame + 1, result);
                result.downbeat = accent == Order::Before;
                uncertainAccent = accent == Order::Uncertain;
            }
            if (! result.downbeat && first > possibleOrdinal)
            {
                const auto previousDownbeat = first - first % segment.meter.numerator;
                if (previousDownbeat >= possibleOrdinal)
                {
                    const auto accent = comparisonForOrdinal (segment, previousDownbeat, minimumFrame, result);
                    result.downbeat = accent == Order::AtOrAfter;
                    uncertainAccent = uncertainAccent || accent == Order::Uncertain;
                }
            }
            if (! result.downbeat && firstSegment != possibleSegment)
            {
                if (ordinalBegin != 0)
                {
                    ++result.segmentComparisons;
                    originOrder = click_detail::compare (segment.first, minimumFrame);
                }
                result.downbeat = originOrder == Order::AtOrAfter;
                uncertainAccent = uncertainAccent || originOrder == Order::Uncertain;
            }
            if (! result.downbeat && firstSegment < lastIndex)
            {
                ++result.segmentComparisons;
                const auto accent = click_detail::compare (segments_[firstSegment + 1u].first, result.frame + 1);
                result.downbeat = accent == Order::Before;
                uncertainAccent = uncertainAccent || accent == Order::Uncertain;
            }
        }
        if (! result.downbeat && uncertainAccent)
        {
            result.status = TimeLookupStatus::NumericDomain;
            return located;
        }
        result.found = true;
        located.segment = firstSegment;
        located.ordinal = first;
        return located;
    }

    [[nodiscard]] bool inJoinBarrier (std::int64_t frame, std::uint32_t& comparisons) const noexcept
    {
        std::size_t low = 0, high = joinBarriers_.size();
        while (low < high)
        {
            const auto mid = low + (high - low) / 2u;
            ++comparisons;
            if (click_detail::compareScalar (joinBarriers_[mid].low, frame) <= 0) low = mid + 1u;
            else high = mid;
        }
        if (low == 0)
            return false;
        ++comparisons;
        return click_detail::compareScalar (joinBarriers_[low - 1u].high, frame) >= 0;
    }

    CompiledTempoMap compiled_; // Control-only inverse for accepted count-in cursors
    CompiledMeterMap meters_; // Control-only bar index for fractional arrange snapping
    std::vector<TempoSegment> tempo_;
    std::vector<Segment> segments_;
    // Rounded prefix anchors can create a downward bucket jump against more exact local
    // arithmetic. Only those derived Frame ranges refuse; outside them bucket ordering holds.
    std::vector<Enclosure> joinBarriers_;
    Enclosure domainEnd_;
    double rateHz_ = 48000.0, k_ = 187.5;
    TimeLookupStatus status_ = TimeLookupStatus::InvalidMap;
};

inline MusicalClickMap::Window MusicalClickMap::onsetsInRange (std::int64_t firstFrame,
                                                              std::int64_t endFrame) const noexcept
{
    Window window;
    window.map_ = this;
    window.cursor_ = firstFrame;
    window.end_ = endFrame;
    if (firstFrame < 0 || endFrame < firstFrame)
    {
        window.pending_.lookup.status = TimeLookupStatus::InvalidInput;
        window.hasPending_ = true;
        window.done_ = false;
        return window;
    }
    if (firstFrame == endFrame)
        return window;
    const auto lastIndex = segments_.empty() ? 0u : segments_.size() - 1u;
    const auto lastOrdinal = segments_.empty() ? 0u : segments_.back().lastOrdinal;
    auto first = findOnset (firstFrame, 0, 0, lastIndex, lastOrdinal);
    window.preparation_ = first.lookup;
    window.hasPending_ = true;
    window.done_ = false;
    if (first.lookup.status != TimeLookupStatus::Ok || ! first.lookup.found || first.lookup.frame >= endFrame)
    {
        first.lookup.found = false;
        first.lookup.ordinalProbes = first.lookup.segmentComparisons = first.lookup.precisionComparisons = 0;
        first.lookup.roughProbes = first.lookup.tightProbes = 0;
        window.pending_ = first;
        return window;
    }
    const auto after = findOnset (endFrame, first.segment, first.ordinal, lastIndex, lastOrdinal);
    window.preparation_.ordinalProbes += after.lookup.ordinalProbes;
    window.preparation_.roughProbes += after.lookup.roughProbes;
    window.preparation_.tightProbes += after.lookup.tightProbes;
    window.preparation_.segmentComparisons += after.lookup.segmentComparisons;
    window.preparation_.precisionComparisons += after.lookup.precisionComparisons;
    window.firstSegment_ = first.segment;
    window.firstOrdinal_ = first.ordinal;
    window.lastSegment_ = lastIndex;
    window.lastOrdinal_ = lastOrdinal;
    if (after.lookup.status == TimeLookupStatus::Ok && after.lookup.found)
    {
        window.allowOrdinalHints_ = true;
        window.lastSegment_ = after.segment;
        if (after.ordinal != 0)
            window.lastOrdinal_ = after.ordinal - 1u;
        else
        {
            --window.lastSegment_; // first is strictly before endFrame, hence a predecessor exists
            window.lastOrdinal_ = segments_[window.lastSegment_].lastOrdinal;
        }
    }
    // A later numeric/domain refusal remains deferred until the cursor reaches it; keep the
    // usable earlier onsets and their tails instead of turning an unused future probe into silence.
    first.lookup.ordinalProbes = first.lookup.segmentComparisons = first.lookup.precisionComparisons = 0;
    first.lookup.roughProbes = first.lookup.tightProbes = 0;
    window.pending_ = first;
    return window;
}

inline MusicalClickLookup MusicalClickMap::Window::next() noexcept
{
    if (done_)
        return {};
    LocatedOnset located;
    if (hasPending_)
    {
        located = pending_;
        hasPending_ = false;
    }
    else
    {
        if (cursor_ >= end_)
        {
            done_ = true;
            return {};
        }
        located = map_->findOnset (cursor_, firstSegment_, firstOrdinal_, lastSegment_, lastOrdinal_,
                                  allowOrdinalHints_ ? ordinalStride_ : 0u);
    }
    if (located.lookup.status != TimeLookupStatus::Ok || ! located.lookup.found)
    {
        done_ = true;
        return located.lookup;
    }
    if (located.lookup.frame >= end_)
    {
        done_ = true;
        return {};
    }
    cursor_ = located.lookup.frame + 1;


    ordinalStride_ = allowOrdinalHints_ && located.segment == firstSegment_ && located.ordinal > firstOrdinal_
        ? located.ordinal - firstOrdinal_ : 0u;
    firstSegment_ = located.segment;
    firstOrdinal_ = located.ordinal;
    return located.lookup;
}

} // namespace yesdaw::engine
