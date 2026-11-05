#include "engine/MusicalAudioCutPlan.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

TEST_CASE ("Musical audio logical cuts retain exact boundaries and bounded cumulative calls",
           "[musical-audio-cut-plan][adr0083]")
{
    using namespace yesdaw::engine;
    using Status = MusicalAudioCutStatus;
    using Geometry = MusicalAudioStatus;
    const auto clockFor = [] (std::vector<TempoChange> rows, double rate = 15360.0) {
        std::shared_ptr<const MusicalAudioClock> clock;
        REQUIRE (MusicalAudioClock::build ({ rows.data(), rows.size() }, { rate }, {}, clock) == Geometry::Ok);
        REQUIRE (clock);
        return clock;
    };
    const auto windowFor = [] (const std::shared_ptr<const MusicalAudioClock>& clock,
                              Tick start, Tick length, std::uint64_t count) {
        MusicalAudioWindow window;
        REQUIRE (resolveMusicalAudioWindow (clock, start, length, count, window) == Geometry::Ok);
        return window;
    };
    const auto planFor = [] (const MusicalAudioWindow& window, MusicalAudioCutCaps caps = {}) {
        std::shared_ptr<const MusicalAudioCutPlan> plan;
        const auto result = buildMusicalAudioCutPlan (window, caps, {}, plan);
        REQUIRE (result.status == Status::Ok);
        REQUIRE (result.geometryStatus == Geometry::Ok);
        REQUIRE (plan);
        return plan;
    };
    struct ExpectedCall { std::uint64_t input; std::int64_t output; std::uint16_t count, produced; };
    const auto callsAre = [] (const MusicalAudioCutPlan& plan, std::span<const ExpectedCall> expected) {
        REQUIRE (plan.calls().size() == expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index)
        {
            const auto& actual = plan.calls()[index];
            const auto& row = expected[index];
            CHECK (actual.inputBegin == row.input);
            CHECK (actual.outputBegin == row.output);
            CHECK (actual.inputFrames == row.count);
            CHECK (actual.outputFrames == row.produced);
        }
    };
    const auto ledgerIs = [] (const MusicalAudioCutPlan& plan, auto independentCut) {
        const auto& window = plan.window();
        std::uint64_t input = 0;
        std::int64_t output = 0;
        std::size_t boundary = 0;
        for (const auto& call : plan.calls())
        {
            REQUIRE (call.inputFrames > 0);
            REQUIRE (call.outputFrames > 0);
            REQUIRE (call.inputFrames <= plan.caps().maxInputFrames);
            REQUIRE (call.outputFrames <= plan.caps().maxOutputFrames);
            REQUIRE (call.inputBegin == input);
            REQUIRE (call.outputBegin == output);
            REQUIRE (call.inputFrames <= window.inputFrames() - input);
            CHECK (std::isfinite (call.chordErrorUpper));
            CHECK (call.chordErrorUpper >= 0.0);
            CHECK (call.chordErrorUpper <= 0.25);
            REQUIRE (call.firstBoundary == boundary);
            REQUIRE (call.boundaryCount <= plan.boundaries().size() - boundary);
            const auto end = input + call.inputFrames;
            for (std::size_t count = 0; count < call.boundaryCount; ++count)
            {
                const auto& source = plan.boundaries()[boundary + count].source;
                CHECK (source.whole >= input);
                CHECK (source.whole < end);
                CHECK (source.denominator > 0);
                CHECK (source.numerator < source.denominator);
            }
            input = end;
            output += call.outputFrames;
            CHECK (output == independentCut (input));
            if (input < window.inputFrames()) CHECK (output < window.frameCount());
            boundary += call.boundaryCount;
        }
        CHECK (input == window.inputFrames());
        CHECK (output == window.frameCount());
        CHECK (boundary == plan.boundaries().size());
    };
    const auto accountingIs = [] (const MusicalAudioCutPlan& plan) {
        const auto& a = plan.accounting();
        const auto& clock = *plan.window().clock();
        const auto queryCost = 8192u + 128u * static_cast<std::uint64_t> (std::bit_width (clock.rows().size()));
        const auto expected = clock.constructionWorkUnits() + 16384u + a.cutQueries * queryCost
            + a.sourceProjections * 4096u + a.ratioQueries * 4096u + a.candidateChecks * 512u
            + a.callSteps * 256u + a.emissions * 64u + a.lengthProbes * 64u + a.rowVisits * 64u;
        CHECK (a.constructionWorkUnits == expected);
        CHECK (a.constructionWorkUnits <= kMusicalAudioMaxWorkUnits);
        CHECK (a.clockReservedBytes == clock.reservedBytes());
        CHECK (a.reservedBytes == a.clockReservedBytes + a.planReservedBytes);
        CHECK (a.reservedBytes <= kMusicalAudioMaxReservedBytes);
        CHECK (a.planReservedBytes >= plan.calls().size() * sizeof (MusicalAudioLogicalCall)
            + plan.boundaries().size() * sizeof (MusicalAudioTempoBoundary));
        CHECK (a.candidateChecks <= a.callSteps * 2048u);
        CHECK (a.lengthProbes <= a.callSteps * 4097u);
        CHECK (a.emissions == 2u * plan.calls().size());
    };

    SECTION ("constant cuts retain cumulative floors and one immutable clock")
    {
        const auto window = windowFor (clockFor ({ { 0, 60.0 } }), 0, 6000, 4000);
        const auto plan = planFor (window);
        const std::array<ExpectedCall, 4> expected {{
            { 0, 0, 1024, 1536 }, { 1024, 1536, 1024, 1536 },
            { 2048, 3072, 976, 1464 }, { 3024, 4536, 976, 1464 }
        }};
        callsAre (*plan, expected);
        ledgerIs (*plan, [] (std::uint64_t source) { return static_cast<std::int64_t> (3u * source / 2u); });
        CHECK (plan->window().clock() == window.clock());
        CHECK (plan->window().inputFrames() == 4000);
        CHECK (plan->window().frameCount() == 6000);
        CHECK (plan->boundaries().empty());
        accountingIs (*plan);
    }
    SECTION ("zero output coalesces all source input and supplied tiny caps can refuse")
    {
        const auto half = windowFor (clockFor ({ { 0, 60.0 } }), 0, 3, 6);
        const auto plan = planFor (half, { 2, 1 });
        const std::array<ExpectedCall, 3> expected {{ { 0, 0, 2, 1 }, { 2, 1, 2, 1 }, { 4, 2, 2, 1 } }};
        callsAre (*plan, expected);
        ledgerIs (*plan, [] (std::uint64_t source) { return static_cast<std::int64_t> (source / 2u); });
        auto failed = plan;
        CHECK (buildMusicalAudioCutPlan (half, { 1, 1 }, {}, failed).status == Status::LogicalCallDomainUnsupported);
        CHECK_FALSE (failed);

        const auto jump = windowFor (clockFor ({ { 0, 120.0 }, { 1, 60.0 } }), 0, 4, 4);
        const auto coalesced = planFor (jump, { 2, 2048 });
        const std::array<ExpectedCall, 2> joined {{ { 0, 0, 2, 1 }, { 2, 1, 2, 2 } }};
        callsAre (*coalesced, joined);
        ledgerIs (*coalesced, [] (std::uint64_t source) {
            return source == 0 ? std::int64_t { 0 } : static_cast<std::int64_t> (source - 1u);
        });
        REQUIRE (coalesced->boundaries().size() == 1);
        CHECK (coalesced->boundaries()[0].source == MusicalAudioSourcePosition { 1, 0, 1 });
        CHECK (coalesced->calls()[0].boundaryCount == 1);
        CHECK (coalesced->calls()[0].chordErrorUpper <= 0.25);
        accountingIs (*coalesced);
    }
    SECTION ("a trailing zero cut cell cannot be dropped or stranded")
    {
        const auto window = windowFor (clockFor ({ { 0, 120.0 } }, 48000.0), 1, 2, 6);
        const auto plan = planFor (window, { 5, 2048 });
        const std::array<ExpectedCall, 2> expected {{ { 0, 0, 3, 2 }, { 3, 2, 3, 1 } }};
        callsAre (*plan, expected);
        constexpr std::array<std::int64_t, 7> cuts { 0, 1, 1, 2, 2, 3, 3 };
        ledgerIs (*plan, [&] (std::uint64_t source) { return cuts[static_cast<std::size_t> (source)]; });
        CHECK (plan->calls().back().inputBegin + plan->calls().back().inputFrames == 6);
        accountingIs (*plan);
    }
    SECTION ("fractional jumps and several changes in a cell retain every exact row")
    {
        const auto window = windowFor (clockFor ({ { 0, 60.0 }, { 3, 40.0 } }), 0, 6, 5);
        const auto plan = planFor (window);
        const std::array<ExpectedCall, 3> expected {{ { 0, 0, 2, 2 }, { 2, 2, 1, 1 }, { 3, 3, 2, 4 } }};
        callsAre (*plan, expected);
        constexpr std::array<std::int64_t, 6> cuts { 0, 1, 2, 3, 5, 7 };
        ledgerIs (*plan, [&] (std::uint64_t source) { return cuts[static_cast<std::size_t> (source)]; });
        REQUIRE (plan->boundaries().size() == 1);
        CHECK (plan->boundaries()[0].tick == 3);
        CHECK (plan->boundaries()[0].rowIndex == 1);
        CHECK (plan->boundaries()[0].source == MusicalAudioSourcePosition { 2, 3, 6 });
        CHECK (plan->calls()[1].boundaryCount == 1);

        const auto dense = windowFor (clockFor ({ { 0, 240.0 }, { 1, 160.0 }, { 2, 240.0 }, { 3, 160.0 } }), 0, 4, 1);
        const auto one = planFor (dense);
        REQUIRE (one->calls().size() == 1);
        REQUIRE (one->boundaries().size() == 3);
        CHECK (one->calls()[0].boundaryCount == 3);
        for (std::size_t index = 0; index < 3; ++index)
        {
            CHECK (one->boundaries()[index].tick == static_cast<Tick> (index + 1));
            CHECK (one->boundaries()[index].source == MusicalAudioSourcePosition { 0, index + 1, 4 });
        }
        ledgerIs (*one, [] (std::uint64_t source) { return static_cast<std::int64_t> (source); });
        accountingIs (*one);
    }
    SECTION ("ramp prefix literals and conservative chords agree with the existing Q contract")
    {
        const auto window = windowFor (clockFor ({ { 0, 120.0 }, { 15360, 120.0, TempoCurve::LinearRamp },
                                                   { 76800, 240.0 } }, 48000.0), 15360, 30720, 49152);
        const auto plan = planFor (window);
        // Independent Decimal160 values already frozen in Window prep; integer atanh oracle also checks these floors.
        constexpr std::array<std::uint64_t, 5> source { 0, 12288, 24576, 36864, 49152 };
        constexpr std::array<std::int64_t, 5> expected { 0, 11307, 21421, 30571, 38924 };
        for (std::size_t index = 0; index < source.size(); ++index)
        {
            std::int64_t cut = -1;
            REQUIRE (window.cutAtSource (source[index], cut) == Geometry::Ok);
            CHECK (cut == expected[index]);
        }
        // Cross-component contract only: the new planner must consume Window's independently tested normative Q.
        ledgerIs (*plan, [&] (std::uint64_t input) {
            std::int64_t cut = -1;
            REQUIRE (window.cutAtSource (input, cut) == Geometry::Ok);
            return cut;
        });
        accountingIs (*plan);

        const auto increasing = windowFor (clockFor ({ { 0, 60.0, TempoCurve::LinearRamp }, { 8, 120.0 } }), 0, 4, 4);
        const auto up = planFor (increasing);
        const std::array<ExpectedCall, 2> ascending {{ { 0, 0, 2, 1 }, { 2, 1, 2, 2 } }};
        callsAre (*up, ascending);
        const auto descending = windowFor (clockFor ({ { 0, 120.0, TempoCurve::LinearRamp }, { 8, 60.0 } }), 0, 4, 4);
        const auto down = planFor (descending);
        const std::array<ExpectedCall, 1> descendingCalls {{ { 0, 0, 4, 2 } }};
        callsAre (*down, descendingCalls);
    }
    SECTION ("local ratio extrema take priority over a legal endpoint average")
    {
        const auto bad = windowFor (clockFor ({ { 0, 60.0 }, { 1, 20.0 }, { 2, 120.0 } }), 0, 4, 4);
        CHECK (bad.frameCount() == 5);
        std::shared_ptr<const MusicalAudioCutPlan> rejected;
        CHECK (buildMusicalAudioCutPlan (bad, {}, {}, rejected).status == Status::LocalRatioUnsupported);
        CHECK_FALSE (rejected);
        const auto half = planFor (windowFor (clockFor ({ { 0, 120.0 } }), 0, 4, 4));
        const auto twice = planFor (windowFor (clockFor ({ { 0, 30.0 } }), 0, 4, 4));
        CHECK (half->calls().size() == 1);
        CHECK (half->calls()[0].outputFrames == 2);
        CHECK (twice->calls().size() == 1);
        CHECK (twice->calls()[0].outputFrames == 8);
        const auto outside = windowFor (clockFor ({ { 0, 60.0, TempoCurve::LinearRamp }, { 8, 240.0 } }), 0, 4, 4);
        CHECK (outside.frameCount() == 2);
        CHECK (buildMusicalAudioCutPlan (outside, {}, {}, rejected).status == Status::LocalRatioUnsupported);
        CHECK_FALSE (rejected);
    }
    SECTION ("full uint64 source projection stays exact without a source-sized allocation")
    {
        const auto window = windowFor (clockFor ({ { 0, 60.0 } }), 0, 3, 4294967297ull);
        MusicalAudioSourcePosition out { 99, 98, 97 };
        REQUIRE (musicalAudioSourceForTick (window, 1, out) == Geometry::Ok);
        CHECK (out == MusicalAudioSourcePosition { 1431655765ull, 2, 3 });
        REQUIRE (musicalAudioSourceForTick (window, 2, out) == Geometry::Ok);
        CHECK (out == MusicalAudioSourcePosition { 2863311531ull, 1, 3 });
        REQUIRE (musicalAudioSourceForTick (window, 3, out) == Geometry::Ok);
        CHECK (out == MusicalAudioSourcePosition { 4294967297ull, 0, 1 });
        CHECK (musicalAudioSourceForTick (window, 4, out) == Geometry::InvalidInput);
        CHECK (out == MusicalAudioSourcePosition {});
        const auto wide = windowFor (clockFor ({ { 0, 60.0 } }, 15360.0 / 1048576.0), 0,
                                     std::numeric_limits<Tick>::max(), std::numeric_limits<std::uint64_t>::max());
        REQUIRE (musicalAudioSourceForTick (wide, std::numeric_limits<Tick>::max() - 1, out) == Geometry::Ok);
        CHECK (out == MusicalAudioSourcePosition { 18446744073709551612ull, 9223372036854775806ull, 9223372036854775807ull });
        std::shared_ptr<const MusicalAudioCutPlan> absent;
        CHECK (buildMusicalAudioCutPlan (wide, { 1, 1 }, {}, absent).status == Status::ResourceLimit);
        CHECK_FALSE (absent);
    }
    SECTION ("ordinary songs and 5000 same-BPM rows fit the unchanged combined budgets")
    {
        // Independent integer calibration: k=187.5, BPM187.5 and L=N give F(t)=t for these integral queries.
        const auto song = windowFor (clockFor ({ { 0, 187.5 } }, 48000.0), 0, 28800000, 28800000);
        const auto plan = planFor (song);
        REQUIRE (plan->calls().size() == 14063);
        CHECK (plan->calls().back().inputFrames == 1024);
        ledgerIs (*plan, [] (std::uint64_t source) { return static_cast<std::int64_t> (source); });
        accountingIs (*plan);
        CHECK (plan->accounting().cutQueries == 28124);
        CHECK (plan->accounting().constructionWorkUnits <= 261019271ull);
        std::vector<TempoChange> rows;
        rows.reserve (5000);
        for (Tick tick = 0; tick < 5000; ++tick) rows.push_back ({ tick, 60.0 });
        const auto dense = windowFor (clockFor (rows), 0, 5000, 5000);
        const auto indexed = planFor (dense);
        REQUIRE (indexed->boundaries().size() == 4999);
        REQUIRE (indexed->calls().size() == 5000);
        for (std::size_t index = 0; index < indexed->boundaries().size(); ++index)
        {
            CHECK (indexed->boundaries()[index].rowIndex == index + 1);
            CHECK (indexed->boundaries()[index].source == MusicalAudioSourcePosition { index + 1, 0, 1 });
        }
        ledgerIs (*indexed, [] (std::uint64_t source) { return static_cast<std::int64_t> (source); });
        accountingIs (*indexed);
        CHECK (indexed->accounting().constructionWorkUnits <= 172824625ull);
        auto refused = indexed;
        CHECK (buildMusicalAudioCutPlan (dense, {}, { 0, kMusicalAudioMaxWorkUnits }, refused).status == Status::ResourceLimit);
        CHECK_FALSE (refused);
        refused = indexed;
        CHECK (buildMusicalAudioCutPlan (dense, {}, { kMusicalAudioMaxReservedBytes, 0 }, refused).status == Status::ResourceLimit);
        CHECK_FALSE (refused);
    }
    SECTION ("failed and borrowed-output builds clear output while retaining their inputs safely")
    {
        auto window = windowFor (clockFor ({ { 0, 60.0 } }), 0, 16, 16);
        auto plan = planFor (window);
        const auto weak = std::weak_ptr<const MusicalAudioClock> (window.clock());
        window = {};
        REQUIRE (buildMusicalAudioCutPlan (plan->window(), {}, {}, plan).status == Status::Ok);
        REQUIRE (plan);
        CHECK_FALSE (weak.expired());
        REQUIRE (buildMusicalAudioCutPlan (plan->window(), { 0, 2048 }, {}, plan).status == Status::InvalidInput);
        CHECK_FALSE (plan);
        CHECK (weak.expired());
        const MusicalAudioWindow invalid;
        CHECK (buildMusicalAudioCutPlan (invalid, {}, {}, plan).status == Status::InvalidInput);
        CHECK_FALSE (plan);
        const auto valid = windowFor (clockFor ({ { 0, 60.0 } }), 0, 16, 16);
        plan = planFor (valid);
        CHECK (buildMusicalAudioCutPlan (valid, { 2049, 2048 }, {}, plan).status == Status::InvalidInput);
        CHECK_FALSE (plan);
        plan = planFor (valid);
        CHECK (buildMusicalAudioCutPlan (valid, { 2048, 2049 }, {}, plan).status == Status::InvalidInput);
        CHECK_FALSE (plan);
        MusicalAudioSourcePosition source { 1, 1, 2 };
        CHECK (musicalAudioSourceForTick (invalid, 0, source) == Geometry::InvalidClock);
        CHECK (source == MusicalAudioSourcePosition {});
    }
    SECTION ("independent literal ledger mutations fail their unchanged exact gate")
    {
        const auto valid = [] (std::span<const ExpectedCall> calls, std::uint64_t total, std::int64_t duration,
                               auto expectedCut) {
            std::uint64_t input = 0;
            std::int64_t output = 0;
            for (const auto& call : calls)
            {
                if (call.count == 0 || call.produced == 0 || call.input != input || call.output != output
                    || call.count > total - input) return false;
                input += call.count;
                output += call.produced;
                if (output != expectedCut (input) || (input < total && output == duration)) return false;
            }
            return input == total && output == duration;
        };
        constexpr std::array<ExpectedCall, 2> exact {{ { 0, 0, 3, 2 }, { 3, 2, 3, 1 } }};
        constexpr std::array<std::int64_t, 7> cuts { 0, 1, 1, 2, 2, 3, 3 };
        const auto cut = [&] (std::uint64_t input) { return cuts[static_cast<std::size_t> (input)]; };
        REQUIRE (valid (exact, 6, 3, cut));
        auto altered = exact;
        ++altered[0].count;
        CHECK_FALSE (valid (altered, 6, 3, cut));
        altered = exact;
        ++altered[1].output;
        CHECK_FALSE (valid (altered, 6, 3, cut));
        constexpr std::array<ExpectedCall, 1> dropped {{ { 0, 0, 5, 3 } }};
        CHECK_FALSE (valid (dropped, 6, 3, cut));
        constexpr std::array<ExpectedCall, 3> jump {{ { 0, 0, 2, 2 }, { 2, 2, 1, 1 }, { 3, 3, 2, 4 } }};
        constexpr std::array<std::int64_t, 6> jumpCuts { 0, 1, 2, 3, 5, 7 };
        REQUIRE (valid (jump, 5, 7, [&] (std::uint64_t input) { return jumpCuts[static_cast<std::size_t> (input)]; }));
        CHECK_FALSE (valid (jump, 5, 7, [] (std::uint64_t input) { return static_cast<std::int64_t> (7u * input / 5u); }));
        const std::array<MusicalAudioSourcePosition, 1> exactBoundary {{ { 2, 3, 6 } }};
        const auto boundaryGate = [&] (std::span<const MusicalAudioSourcePosition> rows) {
            return rows.size() == 1 && rows[0] == exactBoundary[0];
        };
        REQUIRE (boundaryGate (exactBoundary));
        CHECK_FALSE (boundaryGate (std::span<const MusicalAudioSourcePosition> {}));
        const std::array<MusicalAudioSourcePosition, 1> rounded {{ { 3, 0, 1 } }};
        CHECK_FALSE (boundaryGate (rounded));
    }
}
