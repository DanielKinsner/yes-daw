#pragma once

#include "engine/MusicalAudioWindow.h"
#include "engine/MusicalClick.h"

#include <bit>
#include <bitset>
#include <new>
#include <numeric>
#include <type_traits>

namespace yesdaw::engine {

enum class MusicalAudioCutStatus : std::uint8_t
{
    Ok, InvalidInput, GeometryRefused, LocalRatioUnsupported, ArithmeticUnproved,
    LogicalCallDomainUnsupported, ChordBoundUnproved, ResourceLimit
};

struct MusicalAudioCutResult
{
    MusicalAudioCutStatus status = MusicalAudioCutStatus::Ok;
    MusicalAudioStatus geometryStatus = MusicalAudioStatus::Ok;
};

struct MusicalAudioCutCaps
{
    std::uint16_t maxInputFrames = 2048, maxOutputFrames = 2048;
};

struct MusicalAudioSourcePosition
{
    std::uint64_t whole = 0, numerator = 0, denominator = 1;
    friend bool operator== (const MusicalAudioSourcePosition&, const MusicalAudioSourcePosition&) = default;
};

struct MusicalAudioTempoBoundary
{
    Tick tick = 0;
    std::size_t rowIndex = 0;
    MusicalAudioSourcePosition source;
};

struct MusicalAudioLogicalCall
{
    std::uint64_t inputBegin = 0;
    std::int64_t outputBegin = 0;
    std::uint16_t inputFrames = 0, outputFrames = 0;
    std::size_t firstBoundary = 0, boundaryCount = 0;
    double chordErrorUpper = 0;
};

struct MusicalAudioCutAccounting
{
    std::uint64_t clockReservedBytes = 0, planReservedBytes = 0, reservedBytes = 0;
    std::uint64_t constructionWorkUnits = 0;
    std::uint64_t cutQueries = 0, sourceProjections = 0, ratioQueries = 0;
    std::uint64_t candidateChecks = 0, callSteps = 0, emissions = 0;
    std::uint64_t lengthProbes = 0, rowVisits = 0;
};

[[nodiscard]] inline MusicalAudioStatus musicalAudioSourceForTick (
    const MusicalAudioWindow& window, Tick tick, MusicalAudioSourcePosition& out) noexcept
{
    out = {};
    if (! window.valid()) return MusicalAudioStatus::InvalidClock;
    if (tick < window.startTick() || tick - window.startTick() > window.lengthTicks())
        return MusicalAudioStatus::InvalidInput;
    const auto delta = static_cast<std::uint64_t> (tick - window.startTick());
    const auto length = static_cast<std::uint64_t> (window.lengthTicks());
    const auto input = window.inputFrames();
    if (delta == 0) return MusicalAudioStatus::Ok;
    if (delta == length) { out.whole = input; return MusicalAudioStatus::Ok; }
    const auto quotient = input / length, remainder = input % length;
    if (quotient > std::numeric_limits<std::uint64_t>::max() / delta) return MusicalAudioStatus::Overflow;
    const auto base = quotient * delta;
    std::uint64_t tail = 0, fraction = 0;
    musical_audio_detail::sourceQuotient (delta, remainder, length, tail, fraction);
    if (tail > std::numeric_limits<std::uint64_t>::max() - base) return MusicalAudioStatus::Overflow;
    out = { base + tail, fraction, fraction == 0 ? 1 : length };
    return MusicalAudioStatus::Ok;
}

namespace musical_cut_detail {

using Enclosure = click_detail::Enclosure;
using Status = MusicalAudioCutStatus;
inline constexpr std::uint64_t maxCalls = std::uint64_t { 1 } << 20;

// Exactly sized raw storage: construction is charged when a record is filled.
template<class T> class ExactArray
{
public:
    static_assert (std::is_trivially_destructible_v<T>);
    ExactArray() = default;
    ExactArray (const ExactArray&) = delete;
    ExactArray& operator= (const ExactArray&) = delete;
    ~ExactArray() { if (data_ != nullptr) std::allocator<T> {}.deallocate (data_, size_); }
    void allocate (std::size_t size)
    {
        if (size > 0) data_ = std::allocator<T> {}.allocate (size);
        size_ = size;
    }
    [[nodiscard]] T* data() noexcept { return data_; }
    [[nodiscard]] const T* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    T& operator[] (std::size_t index) noexcept { return data_[index]; }
    const T& operator[] (std::size_t index) const noexcept { return data_[index]; }
private:
    T* data_ = nullptr;
    std::size_t size_ = 0;
};

struct Segment
{
    Tick start = 0, end = 0, originalStart = 0, originalEnd = 0;
    double startBpm = 120.0, endBpm = 120.0;
    bool ramp = false;
    MusicalAudioSourcePosition sourceStart, sourceEnd;
    Enclosure firstRatio, lastRatio;
};

struct Scratch
{
    std::bitset<2049> visited;
    MusicalAudioLogicalCall accepted;
    bool capEligible = false, chordUnproved = false;
};

struct Builder;

[[nodiscard]] inline bool atOrBefore (const MusicalAudioSourcePosition& source, std::uint64_t value) noexcept
{
    return source.whole < value || (source.whole == value && source.numerator == 0);
}

[[nodiscard]] inline Enclosure negate (Enclosure value) noexcept
{
    return { -value.high, -value.low, value.numericFailure };
}

// Prove only the two closed ratio limits without forming an overflowing product.
// Zero means unproved; all other ratios retain their original interval.
[[nodiscard]] inline double exactRatioEndpoint (std::uint64_t length, std::uint64_t input,
                                                 double rate, double bpm) noexcept
{
    static_assert (sizeof (double) == sizeof (std::uint64_t)
                   && std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    const auto rateBits = std::bit_cast<std::uint64_t> (rate);
    const auto bpmBits = std::bit_cast<std::uint64_t> (bpm);
    constexpr std::uint64_t largestFinite = 0x7fefffffffffffff;
    if (length == 0 || input == 0 || rateBits == 0 || rateBits > largestFinite
        || bpmBits == 0 || bpmBits > largestFinite) return 0.0;
    const auto rateExponent = static_cast<int> ((rateBits >> 52) & 0x7ff);
    const auto bpmExponent = static_cast<int> ((bpmBits >> 52) & 0x7ff);
    constexpr std::uint64_t hiddenBit = std::uint64_t { 1 } << 52;
    const auto rateSignificand = (rateBits & (hiddenBit - 1)) | (rateExponent == 0 ? 0 : hiddenBit);
    const auto bpmSignificand = (bpmBits & (hiddenBit - 1)) | (bpmExponent == 0 ? 0 : hiddenBit);
    int power = (rateExponent == 0 ? -1074 : rateExponent - 1075)
        - (bpmExponent == 0 ? -1074 : bpmExponent - 1075);
    constexpr auto ppq = static_cast<std::uint64_t> (kTicksPerQuarter);
    constexpr auto common = std::gcd (std::uint64_t { 60 }, ppq);
    // These constant factors strip to one, leaving at most four runtime GCDs.
    static_assert (60 / common == 1 && std::has_single_bit (ppq / common));
    std::uint64_t left[] { length, 60 / common, rateSignificand };
    std::uint64_t right[] { input, ppq / common, bpmSignificand };
    for (unsigned index = 0; index < 3; ++index)
    {
        const auto leftTwos = std::countr_zero (left[index]);
        const auto rightTwos = std::countr_zero (right[index]);
        left[index] >>= leftTwos;
        right[index] >>= rightTwos;
        power += leftTwos - rightTwos;
    }
    if (power != -1 && power != 1) return 0.0;
    for (auto& numerator : left)
        for (auto& denominator : right)
        {
            if (numerator == 1 || denominator == 1) continue;
            auto a = numerator, b = denominator;
            // Each two remainders at least halve b; 128 bounds all uint64 inputs.
            for (unsigned step = 0; b != 0 && step < 128; ++step)
            {
                const auto remainder = a % b;
                a = b;
                b = remainder;
            }
            if (b != 0) return 0.0;
            numerator /= a;
            denominator /= a;
        }
    for (unsigned index = 0; index < 3; ++index)
        if (left[index] != 1 || right[index] != 1) return 0.0;
    return power == -1 ? 0.5 : (power == 1 ? 2.0 : 0.0);
}

} // namespace musical_cut_detail

class MusicalAudioCutPlan final
{
public:
    [[nodiscard]] const MusicalAudioWindow& window() const noexcept { return window_; }
    [[nodiscard]] MusicalAudioCutCaps caps() const noexcept { return caps_; }
    [[nodiscard]] std::span<const MusicalAudioLogicalCall> calls() const noexcept
    { return { calls_.data(), calls_.size() }; }
    [[nodiscard]] std::span<const MusicalAudioTempoBoundary> boundaries() const noexcept
    { return { boundaries_.data(), boundaries_.size() }; }
    [[nodiscard]] const MusicalAudioCutAccounting& accounting() const noexcept { return accounting_; }
private:
    friend struct musical_cut_detail::Builder;
    MusicalAudioCutPlan() = default;
    MusicalAudioWindow window_;
    MusicalAudioCutCaps caps_;
    MusicalAudioCutAccounting accounting_;
    musical_cut_detail::ExactArray<MusicalAudioLogicalCall> calls_;
    musical_cut_detail::ExactArray<MusicalAudioTempoBoundary> boundaries_;
    musical_cut_detail::ExactArray<musical_cut_detail::Segment> segments_;
    musical_cut_detail::Scratch scratch_; // Private construction workspace; never changes after publication.
};

namespace musical_cut_detail {

struct Builder
{
    std::shared_ptr<MusicalAudioCutPlan> plan;
    MusicalAudioLimits limits;
    MusicalAudioCutResult result;
    Enclosure sourceProgress, scale;

    [[nodiscard]] bool fail (Status status) noexcept
    { result = { status, MusicalAudioStatus::Ok }; return false; }

    [[nodiscard]] bool geometry (MusicalAudioStatus status) noexcept
    { result = { Status::GeometryRefused, status }; return false; }

    [[nodiscard]] bool charge (std::uint64_t& counter, std::uint64_t cost) noexcept
    {
        auto& work = plan->accounting_.constructionWorkUnits;
        if (cost > limits.maxWorkUnits - work || counter == std::numeric_limits<std::uint64_t>::max())
            return fail (Status::ResourceLimit);
        work += cost;
        ++counter;
        return true;
    }

    [[nodiscard]] bool row() noexcept { return charge (plan->accounting_.rowVisits, 64); }

    [[nodiscard]] bool reserve (std::uint64_t count, std::uint64_t element) noexcept
    {
        auto& a = plan->accounting_;
        if (element == 0 || count > std::numeric_limits<std::size_t>::max() / element
            || count > (limits.maxReservedBytes - a.reservedBytes) / element)
            return fail (Status::ResourceLimit);
        const auto bytes = count * element;
        a.planReservedBytes += bytes;
        a.reservedBytes += bytes;
        return true;
    }

    [[nodiscard]] Enclosure ratio (Enclosure bpm) const noexcept
    {
        return click_detail::multiply (sourceProgress, click_detail::divide (scale, bpm));
    }

    [[nodiscard]] bool ratioAt (const Segment& segment, MusicalAudioPosition position, Enclosure& out,
                                bool prepaid = false) noexcept
    {
        if (! prepaid && ! charge (plan->accounting_.ratioQueries, 4096)) return false;
        Enclosure bpm { segment.startBpm, segment.startBpm, false };
        if (segment.ramp)
        {
            if (position.whole == segment.originalEnd && position.numerator == 0)
                bpm = { segment.endBpm, segment.endBpm, false };
            else if (position.whole != segment.originalStart || position.numerator != 0)
            {
                const auto span = static_cast<std::uint64_t> (segment.originalEnd - segment.originalStart);
                const auto local = static_cast<std::uint64_t> (position.whole - segment.originalStart);
                const auto fraction = click_detail::divide (click_detail::integer (position.numerator),
                                                            click_detail::integer (position.denominator));
                const auto difference = click_detail::add ({ segment.endBpm, segment.endBpm, false }, negate (bpm));
                if (local <= span / 2)
                {
                    const auto progress = click_detail::divide (click_detail::add (click_detail::integer (local), fraction),
                                                               click_detail::integer (span));
                    bpm = click_detail::add (bpm, click_detail::multiply (difference, progress));
                }
                else
                {
                    const auto remaining = click_detail::add (click_detail::integer (span - local), negate (fraction));
                    const auto progress = click_detail::divide (remaining, click_detail::integer (span));
                    bpm = click_detail::add ({ segment.endBpm, segment.endBpm, false },
                                            negate (click_detail::multiply (difference, progress)));
                }
            }
        }
        out = ratio (bpm);
        // Only metadata for constants or exact original ramp endpoints has this
        // extra proof. Prepaid sourceRatio and clipped ramp interiors stay unchanged.
        // Two interval primitives (128), four guarded GCDs (3072), and fixed
        // decode/strip/validation work (<=256) fit the existing 4096 prepayment.
        if (! prepaid && (! segment.ramp || (position.numerator == 0
                && (position.whole == segment.originalStart || position.whole == segment.originalEnd)))
            && ! out.numericFailure && std::isfinite (out.low) && std::isfinite (out.high)
            && out.low > 0.0 && out.high >= out.low
            && ! bpm.numericFailure && std::isfinite (bpm.low) && bpm.low > 0.0 && bpm.low == bpm.high
            && ((out.low < 0.5 && out.high >= 0.5) || (out.low <= 2.0 && out.high > 2.0)))
        {
            const auto& window = plan->window_;
            const auto endpoint = exactRatioEndpoint (static_cast<std::uint64_t> (window.lengthTicks()),
                window.inputFrames(), window.clock()->sampleRate().hz, bpm.low);
            if (endpoint != 0.0 && out.low <= endpoint && endpoint <= out.high)
                out = { endpoint, endpoint, false };
        }
        return true;
    }

    [[nodiscard]] bool sourceRatio (const Segment& segment, std::uint64_t source, Enclosure& out) noexcept
    {
        if (! charge (plan->accounting_.ratioQueries, 4096)) return false;
        MusicalAudioPosition position;
        const auto status = plan->window_.positionAtSource (source, position);
        if (status != MusicalAudioStatus::Ok) return geometry (status);
        return ratioAt (segment, position, out, true);
    }

    [[nodiscard]] bool admitRatio (Enclosure value) noexcept
    {
        if (value.numericFailure || ! std::isfinite (value.low) || ! std::isfinite (value.high)
            || value.low <= 0.0 || value.high < value.low)
            return fail (Status::ArithmeticUnproved);
        if (value.high < 0.5 || value.low > 2.0) return fail (Status::LocalRatioUnsupported);
        if (value.low < 0.5 || value.high > 2.0) return fail (Status::ArithmeticUnproved);
        return true;
    }

    [[nodiscard]] bool project (Tick tick, MusicalAudioSourcePosition& out) noexcept
    {
        if (! charge (plan->accounting_.sourceProjections, 4096)) return false;
        const auto status = musicalAudioSourceForTick (plan->window_, tick, out);
        return status == MusicalAudioStatus::Ok || geometry (status);
    }

    [[nodiscard]] bool metadata()
    {
        const auto& window = plan->window_;
        const auto rows = window.clock()->rows();
        const auto end = window.startTick() + window.lengthTicks();
        const auto first = rows.empty() ? std::size_t { 0 } : static_cast<std::size_t> (
            std::upper_bound (rows.begin(), rows.end(), window.startTick(),
                [] (Tick tick, const TempoChange& change) { return tick < change.tick; }) - rows.begin()) - 1;
        std::size_t segments = 0, boundaries = 0;
        if (rows.empty())
        {
            if (! row()) return false;
            segments = 1;
        }
        else for (std::size_t index = first; index < rows.size() && rows[index].tick < end; ++index)
        {
            if (! row()) return false;
            ++segments;
            if (rows[index].tick > window.startTick()) ++boundaries;
        }
        if (! reserve (segments, sizeof (Segment)) || ! reserve (boundaries, sizeof (MusicalAudioTempoBoundary)))
            return false;
        plan->segments_.allocate (segments);
        plan->boundaries_.allocate (boundaries);
        MusicalAudioSourcePosition previous;
        std::size_t boundary = 0;
        for (std::size_t ordinal = 0; ordinal < segments; ++ordinal)
        {
            if (! row()) return false;
            Segment segment;
            const auto index = first + ordinal;
            const bool haveNext = ! rows.empty() && index + 1 < rows.size();
            if (! rows.empty())
            {
                segment.originalStart = rows[index].tick;
                segment.originalEnd = haveNext ? rows[index + 1].tick : end;
                segment.startBpm = rows[index].bpm;
                segment.endBpm = haveNext ? rows[index + 1].bpm : segment.startBpm;
                segment.ramp = haveNext && rows[index].curveToNext == TempoCurve::LinearRamp
                    && segment.startBpm != segment.endBpm;
            }
            else segment.originalEnd = end;
            segment.start = std::max (window.startTick(), segment.originalStart);
            segment.end = std::min (end, segment.originalEnd);
            segment.sourceStart = previous;
            if (segment.end == end) segment.sourceEnd = { window.inputFrames(), 0, 1 };
            else
            {
                if (! project (segment.end, segment.sourceEnd) || ! row()) return false;
                std::construct_at (plan->boundaries_.data() + boundary,
                    MusicalAudioTempoBoundary { segment.end, index + 1, segment.sourceEnd });
                ++boundary;
            }
            if (! ratioAt (segment, { segment.start, 0, 1 }, segment.firstRatio)
                || ! ratioAt (segment, { segment.end, 0, 1 }, segment.lastRatio)
                || ! admitRatio (segment.firstRatio) || ! admitRatio (segment.lastRatio)) return false;
            std::construct_at (plan->segments_.data() + ordinal, segment);
            previous = segment.sourceEnd;
        }
        return boundary == boundaries || fail (Status::ArithmeticUnproved);
    }

    [[nodiscard]] bool chord (std::uint64_t input, std::uint64_t end, std::size_t first,
                              double& upper) noexcept
    {
        double minimum = INFINITY, maximum = 0.0, constantBpm = 0.0;
        bool constant = true, seen = false;
        for (std::size_t index = first; index < plan->segments_.size(); ++index)
        {
            const auto& segment = plan->segments_[index];
            if (segment.sourceStart.whole >= end) break;
            if (! row()) return false;
            Enclosure low = segment.firstRatio, high = segment.lastRatio;
            if (segment.ramp)
            {
                if (segment.sourceStart.whole < input
                    && ! sourceRatio (segment, input, low)) return false;
                if (! atOrBefore (segment.sourceEnd, end) && ! sourceRatio (segment, end, high)) return false;
            }
            if (! seen) constantBpm = segment.startBpm;
            seen = true;
            constant = constant && ! segment.ramp && segment.startBpm == constantBpm;
            if (low.numericFailure || high.numericFailure || ! std::isfinite (low.low) || ! std::isfinite (high.high))
            { upper = INFINITY; return true; }
            minimum = std::min ({ minimum, low.low, high.low });
            maximum = std::max ({ maximum, low.high, high.high });
        }
        if (! seen) { upper = INFINITY; return true; }
        if (constant) { upper = 0.0; return true; }
        const auto spread = click_detail::add ({ maximum, maximum, false }, { -minimum, -minimum, false });
        const auto size = static_cast<double> (end - input);
        const auto bound = click_detail::divide (click_detail::multiply ({ size, size, false }, spread), { 4.0, 4.0, false });
        upper = bound.numericFailure ? INFINITY : bound.high;
        return true;
    }

    enum class Attempt { Accepted, Zero, TooMuch, Tail, Chord, Visited, Stopped };

    [[nodiscard]] Attempt attempt (std::uint64_t input, std::int64_t output, std::uint16_t count,
                                   std::size_t segment, std::size_t boundary) noexcept
    {
        auto& scratch = plan->scratch_;
        if (scratch.visited[count]) return Attempt::Visited;
        scratch.visited[count] = true;
        if (! charge (plan->accounting_.candidateChecks, 512)) return Attempt::Stopped;
        const auto end = input + count;
        auto target = plan->window_.frameCount();
        if (end != plan->window_.inputFrames())
        {
            const auto queryCost = 8192u + 128u * static_cast<std::uint64_t> (std::bit_width (plan->window_.clock()->rows().size()));
            if (! charge (plan->accounting_.cutQueries, queryCost)) return Attempt::Stopped;
            const auto status = plan->window_.cutAtSource (end, target);
            if (status != MusicalAudioStatus::Ok) { (void) geometry (status); return Attempt::Stopped; }
        }
        if (target < output || target > plan->window_.frameCount())
        { (void) geometry (MusicalAudioStatus::NumericDomain); return Attempt::Stopped; }
        const auto produced = target - output;
        if (produced == 0) return Attempt::Zero;
        if (produced > plan->caps_.maxOutputFrames) return Attempt::TooMuch;
        if (target == plan->window_.frameCount() && end != plan->window_.inputFrames()) return Attempt::Tail;
        scratch.capEligible = true;
        double bound = 0.0;
        if (! chord (input, end, segment, bound)) return Attempt::Stopped;
        if (! std::isfinite (bound) || bound < 0.0 || bound > 0.25)
        { scratch.chordUnproved = true; return Attempt::Chord; }
        std::size_t after = boundary;
        while (after < plan->boundaries_.size() && plan->boundaries_[after].source.whole < end)
        {
            if (! row()) return Attempt::Stopped;
            ++after;
        }
        scratch.accepted = { input, output, count, static_cast<std::uint16_t> (produced), boundary, after - boundary, bound };
        return Attempt::Accepted;
    }

    [[nodiscard]] bool pass (bool fill, std::size_t& callCount) noexcept
    {
        std::uint64_t input = 0;
        std::int64_t output = 0;
        std::size_t segment = 0, boundary = 0;
        callCount = 0;
        while (input < plan->window_.inputFrames())
        {
            if (! charge (plan->accounting_.callSteps, 256)) return false;
            auto& scratch = plan->scratch_;
            scratch.visited.reset();
            scratch.capEligible = scratch.chordUnproved = false;
            while (segment + 1 < plan->segments_.size() && atOrBefore (plan->segments_[segment].sourceEnd, input))
            { if (! row()) return false; ++segment; }
            const auto limit = static_cast<std::uint16_t> (std::min<std::uint64_t> (
                plan->caps_.maxInputFrames, plan->window_.inputFrames() - input));
            auto preferred = limit;
            if (boundary < plan->boundaries_.size())
            {
                const auto& next = plan->boundaries_[boundary].source;
                const auto stop = next.whole > input ? next.whole : input + 1;
                preferred = static_cast<std::uint16_t> (std::min<std::uint64_t> (preferred, stop - input));
            }
            bool accepted = false;
            auto count = preferred;
            for (;;)
            {
                if (! charge (plan->accounting_.lengthProbes, 64)) return false;
                const auto outcome = attempt (input, output, count, segment, boundary);
                if (outcome == Attempt::Stopped) return false;
                if (outcome == Attempt::Accepted) { accepted = true; break; }
                if (outcome == Attempt::Visited) break;
                auto next = static_cast<std::uint16_t> ((count + 1u) / 2u);
                if (outcome == Attempt::Zero)
                    next = static_cast<std::uint16_t> (std::min<unsigned> (limit, std::max<unsigned> (count + 1u, 2u * count)));
                else if (outcome == Attempt::Tail && plan->window_.inputFrames() - input <= limit)
                    next = limit;
                if (next == count) break;
                count = next;
            }
            if (! accepted) for (unsigned remaining = limit; remaining > 0; --remaining)
            {
                if (! charge (plan->accounting_.lengthProbes, 64)) return false;
                if (scratch.visited[remaining]) continue;
                const auto outcome = attempt (input, output, static_cast<std::uint16_t> (remaining), segment, boundary);
                if (outcome == Attempt::Stopped) return false;
                if (outcome == Attempt::Accepted) { accepted = true; break; }
            }
            if (! accepted) return fail (scratch.capEligible && scratch.chordUnproved
                ? Status::ChordBoundUnproved : Status::LogicalCallDomainUnsupported);
            if (callCount >= maxCalls || (fill && callCount >= plan->calls_.size())) return fail (Status::ResourceLimit);
            if (! charge (plan->accounting_.emissions, 64)) return false;
            const auto& call = scratch.accepted;
            if (fill) std::construct_at (plan->calls_.data() + callCount, call);
            input += call.inputFrames;
            output += call.outputFrames;
            boundary += call.boundaryCount;
            ++callCount;
        }
        return (output == plan->window_.frameCount() && boundary == plan->boundaries_.size())
            || geometry (MusicalAudioStatus::NumericDomain);
    }

    [[nodiscard]] bool build (const MusicalAudioWindow& window, MusicalAudioCutCaps caps,
                              MusicalAudioLimits requested)
    {
        if (! window.valid() || caps.maxInputFrames == 0 || caps.maxInputFrames > 2048
            || caps.maxOutputFrames == 0 || caps.maxOutputFrames > 2048) return fail (Status::InvalidInput);
        limits = { std::min ({ requested.maxReservedBytes, window.clock()->limits().maxReservedBytes, kMusicalAudioMaxReservedBytes }),
                   std::min ({ requested.maxWorkUnits, window.clock()->limits().maxWorkUnits, kMusicalAudioMaxWorkUnits }) };
        constexpr std::uint64_t fixedBytes = sizeof (MusicalAudioCutPlan) + 64 + 3 * 64 + 512;
        if (window.clock()->reservedBytes() > limits.maxReservedBytes
            || fixedBytes > limits.maxReservedBytes - window.clock()->reservedBytes()
            || window.clock()->constructionWorkUnits() > limits.maxWorkUnits
            || 16384u > limits.maxWorkUnits - window.clock()->constructionWorkUnits()) return fail (Status::ResourceLimit);
        const auto minimum = [] (std::uint64_t count, std::uint16_t perCall) {
            return count / perCall + static_cast<std::uint64_t> (count % perCall != 0);
        };
        if (minimum (window.inputFrames(), caps.maxInputFrames) > maxCalls
            || minimum (static_cast<std::uint64_t> (window.frameCount()), caps.maxOutputFrames) > maxCalls)
            return fail (Status::ResourceLimit);
        plan = std::shared_ptr<MusicalAudioCutPlan> (new MusicalAudioCutPlan);
        plan->window_ = window;
        plan->caps_ = caps;
        auto& a = plan->accounting_;
        a.clockReservedBytes = window.clock()->reservedBytes();
        a.planReservedBytes = fixedBytes;
        a.reservedBytes = a.clockReservedBytes + fixedBytes;
        a.constructionWorkUnits = window.clock()->constructionWorkUnits() + 16384;
        sourceProgress = click_detail::divide (click_detail::integer (static_cast<std::uint64_t> (window.lengthTicks())),
                                         click_detail::integer (window.inputFrames()));
        scale = click_detail::divide (click_detail::multiply ({ 60.0, 60.0, false },
            { window.clock()->sampleRate().hz, window.clock()->sampleRate().hz, false }),
            { static_cast<double> (kTicksPerQuarter), static_cast<double> (kTicksPerQuarter), false });
        if (! metadata()) return false;
        std::size_t count = 0;
        if (! pass (false, count) || ! reserve (count, sizeof (MusicalAudioLogicalCall))) return false;
        plan->calls_.allocate (count);
        std::size_t filled = 0;
        if (! pass (true, filled)) return false;
        return filled == count || geometry (MusicalAudioStatus::NumericDomain);
    }
};

} // namespace musical_cut_detail

[[nodiscard]] inline MusicalAudioCutResult buildMusicalAudioCutPlan (
    const MusicalAudioWindow& window, MusicalAudioCutCaps caps, MusicalAudioLimits limits,
    std::shared_ptr<const MusicalAudioCutPlan>& out)
{
    const auto retained = window;
    out.reset();
    musical_cut_detail::Builder builder;
    try
    {
        if (! builder.build (retained, caps, limits)) return builder.result;
        out = std::move (builder.plan);
        return {};
    }
    catch (const std::bad_alloc&) { return { MusicalAudioCutStatus::ResourceLimit, MusicalAudioStatus::Ok }; }
}

} // namespace yesdaw::engine
