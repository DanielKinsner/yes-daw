#include "probe-types.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <new>
#include <vector>

namespace yesdaw::query_fp_probe {
using Status = e::MusicalAudioStatus;

bool sameControl (const NativeSnapshot& a, const NativeSnapshot& b) noexcept
{
    return a.supported && b.supported && a.control0 == b.control0
        && a.control1 == b.control1 && a.rounding == b.rounding;
}

struct Counts final
{
    U64 cases {}, builds {}, clockSuccesses {}, queries {}, replayCalls {};
    U64 prefixReplayCalls {}, partialReplayCalls {}, emptyFormulaCalls {};
    U64 knotOkBytes {}, knotStatusOnly {}, interiorOkBytes {}, failedQueries {};
    U64 operationBoundaries {}, nativeStatusChangedIntervals {}, errnoChangedIntervals {};
    std::array<U64, 9> buildStatus {}, queryStatus {};
};

struct Case final
{
    std::vector<e::TempoChange> rows;
    Status expectedBuild = Status::Ok;
    bool numericBuildRefusalAllowed = false;
};

struct Run final
{
    NativeSnapshot baseline;
    Counts counts;
    std::size_t caseIndex {};
    double rate {};
    e::Tick tick {};

    [[noreturn]] void fail (const char* reason, U64 expected = 0, U64 actual = 0) const noexcept
    {
        std::fprintf (stderr,
            "FAIL FP callsite probe: %s case=%zu rate=%.17g tick=%lld expected=%llu actual=%llu\n",
            reason, caseIndex, rate, static_cast<long long> (tick),
            static_cast<unsigned long long> (expected), static_cast<unsigned long long> (actual));
        std::exit (1);
    }

    NativeSnapshot before() const noexcept
    {
        const auto current = snapshot();
        if (! sameControl (baseline, current)) fail ("native control changed before operation");
        return current;
    }

    void after (const NativeSnapshot& previous) noexcept
    {
        const auto current = snapshot();
        ++counts.operationBoundaries;
        if (! sameControl (previous, current)) fail ("native control changed across operation");
        if (current.status0 != previous.status0 || current.status1 != previous.status1
            || current.exceptions != previous.exceptions) ++counts.nativeStatusChangedIntervals;
        if (current.errorNumber != previous.errorNumber) ++counts.errnoChangedIntervals;
    }

    bool append (e::Tick segment, e::Tick delta, double start, double end,
                 e::TempoCurve curve, double& frame, bool prefix) noexcept
    {
        const auto control = before();
        ++counts.replayCalls;
        if (prefix) ++counts.prefixReplayCalls;
        else ++counts.partialReplayCalls;
        const bool result = e::detail::appendTempoSegmentFrames (
            rate, segment, delta, start, end, curve, frame);
        after (control);
        return result;
    }

    Status replay (const Case& input, e::Tick requested, double& frame) noexcept
    {
        frame = 0.0;
        if (requested < 0) return Status::InvalidInput;
        if (input.rows.empty())
        {
            const auto control = before();
            ++counts.emptyFormulaCalls;
            frame = static_cast<double> (requested)
                  * (60.0 * rate / (120.0 * static_cast<double> (e::kTicksPerQuarter)));
            after (control);
        }
        else
        {
            std::size_t selected = 0;
            while (selected + 1 < input.rows.size() && input.rows[selected + 1].tick <= requested)
                ++selected;
            volatile double storedPrefix = 0.0;
            for (std::size_t i = 1; i <= selected; ++i)
            {
                double nextPrefix = storedPrefix;
                const auto& previous = input.rows[i - 1];
                const auto length = input.rows[i].tick - previous.tick;
                if (! append (length, length, previous.bpm, input.rows[i].bpm,
                              previous.curveToNext, nextPrefix, true))
                    fail ("full prefix replay failed after successful Clock");
                storedPrefix = nextPrefix;
            }
            frame = storedPrefix;
            const auto& row = input.rows[selected];
            if (requested > row.tick)
            {
                const bool haveNext = selected + 1 < input.rows.size();
                const auto nextTick = haveNext ? input.rows[selected + 1].tick : requested;
                const auto endBpm = haveNext ? input.rows[selected + 1].bpm : row.bpm;
                if (! append (nextTick - row.tick, requested - row.tick,
                              row.bpm, endBpm, row.curveToNext, frame, false))
                    return Status::NumericDomain;
            }
        }
        if (! std::isfinite (frame) || frame < 0.0) return Status::NumericDomain;
        return frame >= 0x1p63 ? Status::Overflow : Status::Ok;
    }

    void query (const Case& input, const ClockOwner& clock, e::Tick requested, bool knot) noexcept
    {
        tick = requested;
        double expectedFrame = 0.0;
        const auto expected = replay (input, requested, expectedFrame);
        const auto control = before();
        e::MusicalAudioFramePosition actual;
        const auto status = frameAt (clock, requested, actual);
        after (control);
        ++counts.queries;
        const auto statusIndex = static_cast<std::size_t> (status);
        if (statusIndex >= counts.queryStatus.size()) fail ("unknown query status");
        ++counts.queryStatus[statusIndex];
        if (status != expected)
            fail ("query status", static_cast<U64> (expected), static_cast<U64> (status));
        if (status == Status::Ok)
        {
            const auto expectedBits = std::bit_cast<U64> (expectedFrame);
            if (std::bit_cast<U64> (actual.low) != expectedBits)
                fail ("low double bits", expectedBits, std::bit_cast<U64> (actual.low));
            if (std::bit_cast<U64> (actual.high) != expectedBits)
                fail ("high double bits", expectedBits, std::bit_cast<U64> (actual.high));
            const auto expectedFloor = static_cast<std::int64_t> (std::floor (expectedFrame));
            if (actual.absoluteFrame != expectedFloor)
                fail ("native floor", static_cast<U64> (expectedFloor), static_cast<U64> (actual.absoluteFrame));
            if (knot) ++counts.knotOkBytes;
            else ++counts.interiorOkBytes;
        }
        else
        {
            ++counts.failedQueries;
            if (knot) ++counts.knotStatusOnly;
            if (std::bit_cast<U64> (actual.low) != 0 || std::bit_cast<U64> (actual.high) != 0
                || actual.absoluteFrame != 0) fail ("failed query did not reset public output");
        }
    }

    void one (const Case& input)
    {
        if (input.rows.size() > 16) fail ("fixture row ceiling");
        for (std::size_t i = 0; i < input.rows.size(); ++i)
            if ((i == 0 && input.rows[i].tick != 0)
                || (i != 0 && input.rows[i].tick <= input.rows[i - 1].tick))
                fail ("fixture tick order");
        ++counts.cases;
        ClockOwner clock;
        const auto control = before();
        const auto status = buildClock (input.rows.data(), input.rows.size(), { rate }, clock);
        after (control);
        ++counts.builds;
        const auto statusIndex = static_cast<std::size_t> (status);
        if (statusIndex >= counts.buildStatus.size()) fail ("unknown build status");
        ++counts.buildStatus[statusIndex];
        if (status != input.expectedBuild
            && ! (input.numericBuildRefusalAllowed && status == Status::NumericDomain))
            fail ("build status", static_cast<U64> (input.expectedBuild), static_cast<U64> (status));
        if (status != Status::Ok)
        {
            if (clock) fail ("failed Clock unexpectedly returned owner");
            return;
        }
        if (! clock) fail ("successful Clock returned no owner");
        ++counts.clockSuccesses;
        if (clock->rows().size() != input.rows.size()) fail ("actual Clock row extent");
        for (std::size_t i = 0; i < input.rows.size(); ++i)
            if (clock->rows()[i] != input.rows[i]) fail ("actual immutable Clock row value");
        for (const auto& row : input.rows) query (input, clock, row.tick, true);
        for (std::size_t i = 1; i < input.rows.size(); ++i)
        {
            const auto first = input.rows[i - 1].tick;
            const auto gap = input.rows[i].tick - first;
            if (gap > 1) query (input, clock, first + 1, false);
            if (gap > 2)
            {
                query (input, clock, first + gap / 2, false);
                query (input, clock, input.rows[i].tick - 1, false);
            }
        }
        const auto last = input.rows.empty() ? e::Tick { 0 } : input.rows.back().tick;
        if (last < std::numeric_limits<e::Tick>::max()) query (input, clock, last + 1, false);
        if (last <= std::numeric_limits<e::Tick>::max() - e::kTicksPerQuarter)
            query (input, clock, last + e::kTicksPerQuarter, false);
        constexpr std::array<e::Tick, 9> endpoints {
            -1, 0, 1, 17, (INT64_C (1) << 53) - 1, INT64_C (1) << 53,
            (INT64_C (1) << 53) + 1, INT64_MAX - 1, INT64_MAX
        };
        for (const auto& endpoint : endpoints) query (input, clock, endpoint, false);
    }
};

std::vector<Case> cases()
{
    using Curve = e::TempoCurve;
    constexpr auto jump = Curve::Jump;
    constexpr auto ramp = Curve::LinearRamp;
    std::vector<Case> out;
    const auto add = [&out] (std::initializer_list<e::TempoChange> rows,
                            Status expected = Status::Ok, bool numericRefusal = false) {
        out.push_back ({ std::vector<e::TempoChange> (rows), expected, numericRefusal });
    };
    add ({});
    add ({ { 0, 125.0, jump } });
    add ({ { 0, 125.0, ramp }, { 15360, 125.0, jump } });
    add ({ { 0, 60.0, ramp }, { 15360, 240.0, jump } });
    add ({ { 0, 240.0, ramp }, { 15360, 60.0, jump } });
    add ({ { 0, 125.0, ramp }, { 65537, std::nextafter (125.0, 126.0), jump } });
    add ({ { 0, 125.0, ramp }, { 65537, std::nextafter (125.0, 124.0), jump } });
    add ({ { 0, 240.0, ramp }, { (INT64_C (1) << 53) + 1, 0.001, jump } });
    add ({ { 0, 1.0e-9, jump }, { INT64_MAX - 1, 120.0, jump } });
    add ({ { 0, 1.0e-300, ramp }, { 15360, 1.0e300, jump } }, Status::Ok, true);
    add ({ { 0, 1.0e300, ramp }, { 15360, 1.0e-300, jump } }, Status::Ok, true);
    add ({ { 0, std::numeric_limits<double>::denorm_min(), jump } });
    add ({ { 0, std::numeric_limits<double>::max(), jump } });
    add ({ { 0, std::numeric_limits<double>::quiet_NaN(), jump } }, Status::InvalidClock);
    add ({ { 0, std::numeric_limits<double>::infinity(), jump } }, Status::InvalidClock);
    add ({ { 0, -1.0, jump } }, Status::InvalidClock);
    U64 random = UINT64_C (0x5f33ca9b4d1780e1);
    const auto next = [&random]() noexcept {
        random ^= random << 13;
        random ^= random >> 7;
        random ^= random << 17;
        return random;
    };
    for (unsigned item = 0; item != 80; ++item)
    {
        Case generated;
        const auto rows = 1u + static_cast<unsigned> (next() % 16u);
        e::Tick position = 0;
        for (unsigned i = 0; i != rows; ++i)
        {
            const auto choice = static_cast<unsigned> (next() % 7u);
            const auto curve = (next() & 1u) == 0 ? jump : ramp;
            constexpr std::array<double, 7> bpm { 25.0, 60.0, 93.0, 125.0, 187.0, 240.0, 400.0 };
            generated.rows.push_back ({ position, bpm[choice], curve });
            position += static_cast<e::Tick> (1u + next() % 65537u);
        }
        out.push_back (std::move (generated));
    }
    return out;
}

int run()
{
    static_assert (sizeof (double) == sizeof (U64) && std::numeric_limits<double>::is_iec559);
    const auto initial = snapshot();
    if (! initial.supported || std::get_new_handler() != nullptr)
    {
        std::fputs ("UNQUALIFIED FP callsite probe: require supported nearest/masked/no-flush native control and null new_handler\n", stderr);
        return 2;
    }
    Run state { initial, {}, 0, 0.0, 0 };
    const auto inputs = cases();
    if (inputs.size() != 96) state.fail ("fixture inventory");
    constexpr std::array<double, 6> rates { 12000.0, 24000.0, 44100.0, 48000.0, 96000.0, 192000.0 };
    for (std::size_t i = 0; i < inputs.size(); ++i)
    {
        state.caseIndex = i;
        for (const auto& rate : rates)
        {
            state.rate = rate;
            state.tick = 0;
            state.one (inputs[i]);
        }
    }
    if (state.counts.cases != 576 || state.counts.builds != state.counts.cases
        || state.counts.clockSuccesses == 0 || state.counts.knotOkBytes == 0
        || state.counts.knotStatusOnly == 0 || state.counts.partialReplayCalls == 0
        || state.counts.buildStatus[static_cast<std::size_t> (Status::InvalidClock)] == 0
        || state.counts.queryStatus[static_cast<std::size_t> (Status::NumericDomain)] == 0
        || state.counts.queryStatus[static_cast<std::size_t> (Status::Overflow)] == 0)
        state.fail ("complete inventory or required genuine outcome absent");
    const auto final = state.before();
    if (std::get_new_handler() != nullptr) state.fail ("new_handler changed at final boundary");
#if defined(_MSC_VER)
    constexpr unsigned compiler = _MSC_FULL_VER;
#elif defined(__clang__)
    constexpr unsigned compiler = __clang_major__ * 10000u + __clang_minor__ * 100u + __clang_patchlevel__;
#elif defined(__GNUC__)
    constexpr unsigned compiler = __GNUC__ * 10000u + __GNUC_MINOR__ * 100u + __GNUC_PATCHLEVEL__;
#else
    constexpr unsigned compiler = 0;
#endif
    const auto print = [] (const char* key, U64 value) {
        std::printf ("%s=%llu\n", key, static_cast<unsigned long long> (value));
    };
    print ("compiler_numeric", compiler);
    print ("control0", final.control0); print ("control1", final.control1);
    print ("maps", inputs.size()); print ("rate_cases", state.counts.cases);
    print ("clock_successes", state.counts.clockSuccesses); print ("queries", state.counts.queries);
    print ("extra_append_calls", state.counts.replayCalls);
    print ("full_prefix_append_calls", state.counts.prefixReplayCalls);
    print ("partial_append_calls", state.counts.partialReplayCalls);
    print ("empty_formula_calls", state.counts.emptyFormulaCalls);
    print ("knot_ok_public_bytes", state.counts.knotOkBytes);
    print ("knot_status_only_no_cache_bytes", state.counts.knotStatusOnly);
    print ("interior_ok_public_bytes", state.counts.interiorOkBytes);
    print ("failed_query_status_and_reset", state.counts.failedQueries);
    print ("snapshot_intervals", state.counts.operationBoundaries);
    print ("native_status_changed_observer_inclusive_intervals", state.counts.nativeStatusChangedIntervals);
    print ("errno_changed_observer_inclusive_intervals", state.counts.errnoChangedIntervals);
    for (std::size_t i = 0; i < state.counts.buildStatus.size(); ++i)
        std::printf ("build_status_%zu=%llu query_status_%zu=%llu\n", i,
            static_cast<unsigned long long> (state.counts.buildStatus[i]), i,
            static_cast<unsigned long long> (state.counts.queryStatus[i]));
    std::fputs ("PASS finite FP callsite/prefix replay measurement; no whole-equivalence or funding qualification\n", stdout);
    return 0;
}
}

int main()
{
    try { return yesdaw::query_fp_probe::run(); }
    catch (...)
    {
        std::fputs ("FAIL FP callsite probe: unexpected fixture/runtime exception; no measurement credit\n", stderr);
        return 2;
    }
}
