#include "ui/MidiCreationGeometry.h"

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <limits>
#include <vector>

namespace {
using namespace yesdaw;
using ui::MidiCreationContext;
using ui::MidiCreationSpan;
using ui::MidiCreationStatus;

MidiCreationContext context (engine::TimeBase base = engine::TimeBase::TempoLocked,
                             engine::Tick insertion = 0,
                             std::vector<engine::TempoChange> tempo = { { 0, 120.0 } },
                             std::vector<engine::MeterChange> meter = { { 0, 4, 4 } })
{
    MidiCreationContext result;
    REQUIRE (MidiCreationContext::prepare ({ tempo.data(), tempo.size() }, { meter.data(), meter.size() },
                                           { 48000.0 }, insertion, base, result) == MidiCreationStatus::Ok);
    return result;
}
} // namespace

TEST_CASE ("MIDI creation geometry maps physical insertion and source endpoints to actual bars",
           "[midi-creation-geometry][adr0082]")
{
    using engine::TimeBase;
    using engine::Tick;
    MidiCreationSpan span { 7, 9 };
    Tick position = 123;

    SECTION ("full jump map and off-bar Meter reset determine empty and exact imported ends")
    {
        for (const auto base : { TimeBase::TempoLocked, TimeBase::SampleLocked })
        {
            const auto c = context (base, 36000, { { 0, 120.0 }, { 30720, 60.0 } },
                                    { { 0, 4, 4 }, { 38400, 3, 8 } });
            CHECK (c.insertionTick() == 23040); // F(T)=25T/16 before30720, then48000+25(T-30720)/8.
            CHECK (c.insertionFrame() == 36000);
            REQUIRE (c.emptyClip (span) == MidiCreationStatus::Ok);
            CHECK (span.start == (base == TimeBase::TempoLocked ? 23040 : 36000));
            CHECK (span.end == (base == TimeBase::TempoLocked ? 38400 : 72000));
            REQUIRE (c.canonicalSourceEndpoint (480, 480, position) == MidiCreationStatus::Ok);
            CHECK (position == 38400);
            REQUIRE (c.importedClip (position, span) == MidiCreationStatus::Ok);
            CHECK (span.end == (base == TimeBase::TempoLocked ? 38400 : 72000));
            REQUIRE (c.sourceNote (0, 480, 480, span) == MidiCreationStatus::Ok);
            CHECK (span.start == 0);
            CHECK (span.length() == (base == TimeBase::TempoLocked ? 15360 : 36000));
            REQUIRE (c.sourcePosition (240, 480, position) == MidiCreationStatus::Ok);
            CHECK (position == (base == TimeBase::TempoLocked ? 7680 : 12000));
        }
    }
    SECTION ("empty at an exact bar starts the following denominator-aware bar")
    {
        const auto c = context (TimeBase::TempoLocked, 72000, { { 0, 120.0 }, { 30720, 60.0 } },
                                { { 0, 4, 4 }, { 38400, 3, 8 } });
        REQUIRE (c.emptyClip (span) == MidiCreationStatus::Ok);
        CHECK (span == MidiCreationSpan { 38400, 61440 });
        REQUIRE (c.importedClip (38400, span) == MidiCreationStatus::CollapsedClipWindow);
        CHECK (span == MidiCreationSpan {});
    }
    SECTION ("source PPQ rounds absolute Note endpoints once before deriving length")
    {
        const auto c = context();
        REQUIRE (c.sourceNote (1, 2, 1001, span) == MidiCreationStatus::Ok);
        CHECK (span == MidiCreationSpan { 15, 31 });
        CHECK (span.length() == 16); // Rounding the source duration separately incorrectly gives15.
        REQUIRE (c.sourcePosition (2, 1001, position) == MidiCreationStatus::Ok);
        CHECK (position == 31);
        REQUIRE (c.sourceNote (3, 3, 1001, span) == MidiCreationStatus::Ok);
        CHECK (span == MidiCreationSpan { 46, 46 }); // A zero-length Note remains zero.
        REQUIRE (c.sourcePosition (1, 30720, position) == MidiCreationStatus::Ok);
        CHECK (position == 1); // Canonical Tick tie rounds later.
        REQUIRE (c.canonicalSourceEndpoint (UINT32_MAX, 1, position) == MidiCreationStatus::Ok);
        CHECK (position == 65970697651200LL);
    }
    SECTION ("Fixed endpoints rebase nearest absolute Frames from the canonical anchor")
    {
        const auto c = context (TimeBase::SampleLocked, 1);
        CHECK (c.insertionTick() == 1); // Physical Frame1 inverses toTick0.64.
        REQUIRE (c.sourceNote (0, 1, 15360, span) == MidiCreationStatus::Ok);
        CHECK (span == MidiCreationSpan { 0, 1 });
        REQUIRE (c.nativeEndpoint (1, position) == MidiCreationStatus::Ok);
        CHECK (position == 1);
        REQUIRE (c.nativeEndpoint (2, position) == MidiCreationStatus::Ok);
        CHECK (position == 2); // Fractional-cursor integration and rendering Floor instead yield end3.
        const auto ties = context (TimeBase::SampleLocked, 0, { { 0, 1920000.0 } });
        REQUIRE (ties.sourcePosition (1, 1, position) == MidiCreationStatus::Ok);
        CHECK (position == 2); // One quarter is exactly1.5 Frames: ties later.
    }
    SECTION ("7/39 selects the actual rational bar before rounding in the destination")
    {
        const auto musical = context (TimeBase::TempoLocked, 0, { { 0, 120.0 } }, { { 0, 7, 39 } });
        REQUIRE (musical.emptyClip (span) == MidiCreationStatus::Ok);
        CHECK (span.end == 11028); // Bar2=143360/13.
        REQUIRE (musical.importedClip (22055, span) == MidiCreationStatus::Ok);
        CHECK (span.end == 22055); // Selected bar3=286720/13; nearest Tick22055.
        const auto fixed = context (TimeBase::SampleLocked, 0, { { 0, 120.0 } }, { { 0, 7, 39 } });
        REQUIRE (fixed.sourceNote (0, 22055, 15360, span) == MidiCreationStatus::Ok);
        CHECK (span.end == 34461); // 22055*25/16=34460.9375.
        REQUIRE (fixed.importedClip (22055, span) == MidiCreationStatus::Ok);
        CHECK (span.end == 34462); // Rational bar projects directly to448000/13=34461+7/13.
        const auto reset = context (TimeBase::TempoLocked, 0, { { 0, 120.0 } },
                                    { { 0, 7, 39 }, { 10000, 3, 8 } });
        REQUIRE (reset.emptyClip (span) == MidiCreationStatus::Ok);
        CHECK (span.end == 10000); // The change truncates the old fractional bar.
    }
    SECTION ("musical endpoints preserve exact integer origins beyond binary64 precision")
    {
        const auto c = context (TimeBase::TempoLocked, 0, { { 0, 120.0 } }, { { 0, 7, 39 } });
        constexpr Tick origin = 11027692307681280LL; // Bar10^12=(10^12-1)*143360/13, exactly integral.
        REQUIRE (c.nativeEndpoint (origin + 1, position) == MidiCreationStatus::Ok);
        CHECK (position == origin + 1);
        REQUIRE (c.importedClip (origin, span) == MidiCreationStatus::Ok);
        CHECK (span.end == origin); // Exact boundary is preserved, no extra bar.
        REQUIRE (c.importedClip (origin + 1, span) == MidiCreationStatus::Ok);
        CHECK (span.end == 11027692307692308LL);
        REQUIRE (c.nativeEndpoint (INT64_MAX, position) == MidiCreationStatus::Ok);
        CHECK (position == INT64_MAX);
        REQUIRE (c.importedClip (INT64_MAX, span) == MidiCreationStatus::Overflow);
        CHECK (span == MidiCreationSpan {});
    }
    SECTION ("unsupported inputs and invalid clocks publish no context or partial geometry")
    {
        MidiCreationContext c = context();
        CHECK (MidiCreationContext::prepare ({}, {}, { 48000.0 }, 0, static_cast<TimeBase> (99), c)
               == MidiCreationStatus::Unsupported);
        CHECK_FALSE (c.ready());
        CHECK (c.insertionTick() == 0);
        CHECK (c.emptyClip (span) == MidiCreationStatus::InvalidClock);
        CHECK (span == MidiCreationSpan {});
        CHECK (MidiCreationContext::prepare ({}, {}, { 48000.0 }, -1, TimeBase::TempoLocked, c)
               == MidiCreationStatus::InvalidInput);
        CHECK_FALSE (c.ready());
        const std::array<engine::TempoChange, 1> badTempo { engine::TempoChange { 2, 120.0 } };
        CHECK (MidiCreationContext::prepare ({ badTempo.data(), badTempo.size() }, {}, { 48000.0 }, 0,
                                             TimeBase::TempoLocked, c) == MidiCreationStatus::InvalidClock);
        const std::array<engine::MeterChange, 1> badMeter { engine::MeterChange { 0, 4, 0 } };
        CHECK (MidiCreationContext::prepare ({}, { badMeter.data(), badMeter.size() }, { 48000.0 }, 0,
                                             TimeBase::TempoLocked, c) == MidiCreationStatus::InvalidClock);
        CHECK (MidiCreationContext::prepare ({}, {}, { std::numeric_limits<double>::quiet_NaN() }, 0,
                                             TimeBase::TempoLocked, c) == MidiCreationStatus::InvalidClock);
        c = context();
        CHECK (c.sourceNote (2, 1, 1001, span) == MidiCreationStatus::InvalidInput);
        CHECK (span == MidiCreationSpan {});
        CHECK (c.sourcePosition (1, 0, position) == MidiCreationStatus::InvalidInput);
        CHECK (position == 0);
        CHECK (c.sourcePosition (1, 32768, position) == MidiCreationStatus::Unsupported);
        CHECK (position == 0);
    }
    SECTION ("uncertifiable inverse and collapsed native windows have distinct refusals")
    {
        const std::array<engine::TempoChange, 3> collapsed {
            engine::TempoChange { 0, 120.0 }, engine::TempoChange { 15360, 1.0e300 },
            engine::TempoChange { 15361, 120.0 }
        };
        MidiCreationContext c;
        CHECK (MidiCreationContext::prepare ({ collapsed.data(), collapsed.size() }, {}, { 48000.0 }, 24000,
                                             TimeBase::TempoLocked, c) == MidiCreationStatus::Precision);
        CHECK_FALSE (c.ready());
        const auto dense = context (TimeBase::SampleLocked, 0, { { 0, 1.0e9 } });
        CHECK (dense.emptyClip (span) == MidiCreationStatus::CollapsedClipWindow);
        CHECK (span == MidiCreationSpan {});
        const auto ordinary = context (TimeBase::SampleLocked);
        CHECK (ordinary.nativeEndpoint (INT64_MAX, position) == MidiCreationStatus::Overflow);
        CHECK (position == 0);
        CHECK (ordinary.importedClip (0, span) == MidiCreationStatus::CollapsedClipWindow);
        CHECK (span == MidiCreationSpan {});
    }
}
