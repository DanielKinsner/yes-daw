#include "interchange/Smf.h"
#include "interchange/ProjectSmf.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace yesdaw;
using engine::Tick;
using interchange::ProjectSmfStatus;

struct ParsedEvent
{
    std::uint64_t tick = 0;
    std::uint8_t status = 0, meta = 0;
    std::vector<std::uint8_t> data;
};

struct ParsedFile
{
    std::uint16_t ppq = 0;
    std::vector<std::vector<ParsedEvent>> tracks;
};

// Deliberately independent of both SMF readers/writers. Cumulative Ticks are wider than uint32.
ParsedFile parse (std::span<const std::uint8_t> bytes)
{
    std::size_t cursor = 0;
    const auto number = [&] (std::size_t width)
    {
        REQUIRE (width <= bytes.size() - cursor);
        std::uint32_t value = 0;
        while (width-- != 0) value = (value << 8u) | bytes[cursor++];
        return value;
    };
    REQUIRE (number (4) == 0x4D546864u);
    REQUIRE (number (4) == 6u);
    REQUIRE (number (2) == 1u);
    const auto count = number (2);
    ParsedFile out;
    out.ppq = static_cast<std::uint16_t> (number (2));
    for (std::uint32_t track = 0; track < count; ++track)
    {
        REQUIRE (number (4) == 0x4D54726Bu);
        const auto length = number (4);
        REQUIRE (length <= bytes.size() - cursor);
        const auto end = cursor + length;
        const auto vlq = [&]
        {
            std::uint32_t value = 0;
            for (int i = 0; i < 4; ++i)
            {
                REQUIRE (cursor < end);
                const auto byte = bytes[cursor++];
                value = (value << 7u) | (byte & 0x7Fu);
                if ((byte & 0x80u) == 0u) return value;
            }
            FAIL ("SMF delta/text length exceeds four-byte VLQ");
            return 0u;
        };
        std::vector<ParsedEvent> events;
        std::uint64_t tick = 0;
        while (cursor < end)
        {
            tick += vlq();
            REQUIRE (cursor < end);
            ParsedEvent event;
            event.tick = tick;
            event.status = bytes[cursor++];
            REQUIRE (event.status >= 0x80u);
            std::size_t dataSize = 0;
            if (event.status == 0xFFu)
            {
                REQUIRE (cursor < end);
                event.meta = bytes[cursor++];
                dataSize = vlq();
            }
            else
            {
                REQUIRE (event.status < 0xF0u);
                dataSize = (event.status & 0xF0u) == 0xC0u || (event.status & 0xF0u) == 0xD0u ? 1u : 2u;
            }
            REQUIRE (dataSize <= end - cursor);
            event.data.assign (bytes.begin() + static_cast<std::ptrdiff_t> (cursor),
                               bytes.begin() + static_cast<std::ptrdiff_t> (cursor + dataSize));
            cursor += dataSize;
            if (event.meta == 0x2Fu)
            {
                CHECK (event.data.empty());
                CHECK (cursor == end);
            }
            events.push_back (std::move (event));
        }
        REQUIRE_FALSE (events.empty());
        CHECK (events.back().meta == 0x2Fu);
        CHECK (std::count_if (events.begin(), events.end(), [] (const auto& e) { return e.meta == 0x2Fu; }) == 1);
        out.tracks.push_back (std::move (events));
    }
    CHECK (cursor == bytes.size());
    return out;
}

engine::EntityId id (std::uint64_t value)
{
    return engine::EntityId::fromBigEndianParts (0, value);
}

engine::Project baseProject()
{
    engine::Project project;
    project.id = id (1);
    project.tempoMap = { { 0, 120.0, engine::TempoCurve::Jump } };
    project.meterMap = { { 0, 4, 4 } };
    engine::Track track;
    track.id = id (2);
    track.strip.name = "First";
    project.tracks.push_back (track);
    return project;
}

engine::MidiClip clipOf (engine::EntityId clipId, engine::EntityId track, Tick start, Tick length,
                        engine::TimeBase base = engine::TimeBase::TempoLocked)
{
    engine::MidiClip clip;
    clip.id = clipId;
    clip.trackId = track;
    clip.timelineStart = start;
    clip.timelineLength = length;
    clip.timeBase = base;
    return clip;
}

engine::Note noteOf (std::uint64_t noteId, Tick start, Tick length, std::int16_t key = 60, double velocity = 0.75)
{
    engine::Note note;
    note.id = id (noteId);
    note.startTick = start;
    note.lengthTicks = length;
    note.key = key;
    note.pitchNote = static_cast<double> (key) + 0.25;
    note.normalizedVelocity = velocity;
    note.portIndex = 1;
    note.channel = 2;
    return note;
}

std::vector<ParsedEvent> channels (const std::vector<ParsedEvent>& track)
{
    std::vector<ParsedEvent> out;
    for (const auto& event : track) if (event.meta == 0) out.push_back (event);
    return out;
}

std::uint32_t tempoOf (const ParsedEvent& event)
{
    REQUIRE (event.meta == 0x51u);
    REQUIRE (event.data.size() == 3u);
    return (static_cast<std::uint32_t> (event.data[0]) << 16u)
        | (static_cast<std::uint32_t> (event.data[1]) << 8u) | event.data[2];
}

template <typename OriginalMicros, typename ExtremumTick>
void checkClock (const ParsedFile& file, OriginalMicros original, ExtremumTick extremum)
{
    std::vector<ParsedEvent> tempos;
    for (const auto& event : file.tracks[0]) if (event.meta == 0x51u) tempos.push_back (event);
    REQUIRE_FALSE (tempos.empty());
    REQUIRE (tempos.front().tick == 0u);
    const auto horizon = file.tracks[0].back().tick;
    long double elapsed = 0.0L;
    for (std::size_t i = 0; i < tempos.size(); ++i)
    {
        const auto begin = tempos[i].tick;
        const auto end = i + 1u < tempos.size() ? tempos[i + 1u].tick : horizon;
        const auto micros = tempoOf (tempos[i]);
        REQUIRE (micros > 0u);
        const auto errorAt = [&] (long double tick)
        {
            const auto encoded = elapsed + (tick - static_cast<long double> (begin)) * micros / file.ppq;
            CHECK (std::abs (encoded - original (tick)) <= 100.0L);
        };
        errorAt (static_cast<long double> (begin));
        errorAt (static_cast<long double> (end));
        const long double inside = extremum (micros);
        if (inside > static_cast<long double> (begin) && inside < static_cast<long double> (end)) errorAt (inside);
        elapsed += static_cast<long double> (end - begin) * micros / file.ppq;
    }
}

} // namespace

TEST_CASE ("Project SMF legacy clock characterization exposes long fractional-tempo drift", "[project-smf][characterization]")
{
    const auto file = yesdaw::interchange::smfFromMusicalTracks ({}, 123.0, 4, 4);
    const double actualMicros = static_cast<double> (file.tracks[0].events[1].tempoMicrosPerQuarter) * 10000.0;
    const double expectedMicros = (60000000.0 / 123.0) * 10000.0;
    CHECK (std::abs (actualMicros - expectedMicros) > 1200.0);
    CHECK (std::abs (actualMicros - expectedMicros) < 1230.0);
}

TEST_CASE ("Project SMF smoke preserves native arrangement occurrences and conductor metadata", "[project-smf]")
{
    auto project = baseProject();
    project.tempoMap.push_back ({ 30720, 60.0, engine::TempoCurve::Jump });
    project.meterMap.push_back ({ 38400, 3, 8 });
    engine::Track second;
    second.id = id (3); second.strip.name = "Second"; project.tracks.push_back (second);
    auto musical = clipOf (id (20), id (2), 15360, 38400);
    musical.notes = { noteOf (40, 3840, 11520) };
    musical.loopLengthTicks = 15360;
    musical.transposeSemitones = 2;
    musical.velocityOffset = -0.25;
    engine::MidiControlEvent control;
    control.id = id (50); control.number = 1; control.value = 0.5; control.channel = 2; control.portIndex = 1;
    musical.controlEvents.push_back (control);
    auto fixed = clipOf (id (21), id (3), 36000, 72000, engine::TimeBase::SampleLocked);
    fixed.notes = { noteOf (41, 50000, 10000, 65) };
    auto muted = musical;
    muted.id = id (22); muted.muted = true; muted.timelineStart = 9000000;
    project.midiClips = { fixed, muted, musical };
    const engine::EntityId chosen[] = { id (21), id (20), id (22), id (20) };
    const auto result = interchange::projectSmfBytes (project, chosen, "Song");
    REQUIRE (result.status == ProjectSmfStatus::Ok);
    CHECK (result.exportedClips == 2u);
    const auto file = parse (result.bytes);
    REQUIRE (file.tracks.size() == 3u);
    CHECK (file.ppq == 15360u);
    CHECK (std::string (file.tracks[1][0].data.begin(), file.tracks[1][0].data.end()) == "First");
    CHECK (std::string (file.tracks[2][0].data.begin(), file.tracks[2][0].data.end()) == "Second");
    const auto first = channels (file.tracks[1]);
    const std::uint64_t ticks[] = { 15360, 19200, 30720, 30720, 34560, 46080, 46080, 49920, 53760 };
    REQUIRE (first.size() == 9u);
    for (std::size_t i = 0; i < first.size(); ++i) CHECK (first[i].tick == ticks[i]);
    CHECK (first[0].status == 0xB2u);
    CHECK (first[0].data == std::vector<std::uint8_t> { 1, 64 });
    CHECK (first[1].status == 0x92u);
    CHECK (first[1].data == std::vector<std::uint8_t> { 62, 64 });
    CHECK (first[2].status == 0xB2u); // Controller precedes the coincident NoteOff.
    const auto secondEvents = channels (file.tracks[2]);
    REQUIRE (secondEvents.size() == 2u);
    CHECK (secondEvents[0].tick == 42880u);
    CHECK (secondEvents[1].tick == 46080u);
    CHECK (secondEvents[0].data == std::vector<std::uint8_t> { 65, 95 });
    CHECK (file.tracks[0].back().tick == 53760u);
    std::vector<std::uint64_t> tempoTicks, meterTicks;
    for (const auto& event : file.tracks[0])
    {
        if (event.meta == 0x51u) tempoTicks.push_back (event.tick);
        if (event.meta == 0x58u) meterTicks.push_back (event.tick);
    }
    CHECK (tempoTicks == std::vector<std::uint64_t> { 0, 30720 });
    CHECK (meterTicks == std::vector<std::uint64_t> { 0, 38400 });
    std::reverse (project.midiClips.begin(), project.midiClips.end());
    CHECK (interchange::projectSmfBytes (project, chosen, "Song").bytes == result.bytes);
}

TEST_CASE ("Project SMF serialized clock carries long constant-tempo rounding error", "[project-smf][clock]")
{
    auto project = baseProject();
    project.tempoMap[0].bpm = 123.0;
    project.midiClips = { clipOf (id (20), id (2), 0, 153600000) };
    const engine::EntityId chosen[] = { id (20) };
    const auto result = interchange::projectSmfBytes (project, chosen, "Clock");
    REQUIRE (result.status == ProjectSmfStatus::Ok);
    const auto file = parse (result.bytes);
    CHECK (std::count_if (file.tracks[0].begin(), file.tracks[0].end(), [] (const auto& e) { return e.meta == 0x51u; }) > 1);
    checkClock (file, [] (long double tick) { return tick * 60000000.0L / (123.0L * 15360.0L); },
                [] (std::uint32_t) { return -1.0L; });
}

TEST_CASE ("Project SMF ramp clock is certified at interior extrema before a later endpoint", "[project-smf][clock]")
{
    auto project = baseProject();
    project.tempoMap = { { 0, 120.0, engine::TempoCurve::LinearRamp }, { 61440, 240.0, engine::TempoCurve::Jump } };
    project.midiClips = { clipOf (id (20), id (2), 0, 46080) };
    const auto original = [] (long double tick) { return 2000000.0L * std::log1p (tick / 61440.0L); };
    const auto roundedAverage = std::round (original (46080.0L) * 15360.0L / 46080.0L);
    CHECK (std::abs (roundedAverage * 23040.0L / 15360.0L - original (23040.0L)) > 10000.0L);
    const engine::EntityId chosen[] = { id (20) };
    const auto result = interchange::projectSmfBytes (project, chosen, "Ramp");
    REQUIRE (result.status == ProjectSmfStatus::Ok);
    const auto file = parse (result.bytes);
    checkClock (file, original, [] (std::uint32_t tempo)
        { return ((60000000.0L / tempo) - 120.0L) * (61440.0L / 120.0L); });
    CHECK (file.tracks[0].back().tick == 46080u);
    for (const auto& event : file.tracks[0]) CHECK (event.tick < 61440u);
}

TEST_CASE ("Project SMF orders coincident edges and quantized zero pairs without source-order dependence", "[project-smf]")
{
    auto project = baseProject();
    auto clip = clipOf (id (20), id (2), 0, 15360);
    clip.notes = { noteOf (50, 0, 7680), noteOf (40, 7680, 0), noteOf (30, 7680, 7680), noteOf (60, 0, 15360, 60, 0.0) };
    engine::MidiControlEvent bend;
    bend.id = id (70); bend.tick = 7680; bend.kind = engine::MidiControlKind::PitchBend; bend.channel = 2;
    clip.controlEvents = { bend };
    auto fixed = clipOf (id (21), id (2), 12001, 100, engine::TimeBase::SampleLocked);
    fixed.notes = { noteOf (80, 0, 1) };
    project.midiClips = { clip, fixed };
    const engine::EntityId chosen[] = { id (20), id (21) };
    const auto result = interchange::projectSmfBytes (project, chosen, "Edges");
    REQUIRE (result.status == ProjectSmfStatus::Ok);
    const auto file = parse (result.bytes);
    const auto events = channels (file.tracks[1]);
    std::vector<std::uint8_t> atBoundary;
    for (const auto& event : events) if (event.tick == 7680) atBoundary.push_back (event.status);
    CHECK (atBoundary == std::vector<std::uint8_t> { 0xE2, 0x82, 0x92, 0x82, 0x92 });
    std::vector<std::uint8_t> collapsed;
    for (const auto& event : events) if (event.tick == 7681) collapsed.push_back (event.status);
    CHECK (collapsed == std::vector<std::uint8_t> { 0x92, 0x82 });
    CHECK (events.size() == 9u); // Four audible pairs plus the bend; silent pair absent.
    std::reverse (project.midiClips[0].notes.begin(), project.midiClips[0].notes.end());
    std::reverse (project.midiClips.begin(), project.midiClips.end());
    CHECK (interchange::projectSmfBytes (project, chosen, "Edges").bytes == result.bytes);
}

TEST_CASE ("Project SMF refuses unsupported or uncertifiable inputs before publishing bytes", "[project-smf]")
{
    auto project = baseProject();
    project.midiClips = { clipOf (id (20), id (2), 0, 15360) };
    project.midiClips[0].notes = { noteOf (40, 0, 7680) };
    const engine::EntityId chosen[] = { id (20) };
    const auto refused = [&] (const auto& value, ProjectSmfStatus expected, interchange::ProjectSmfLimits limits = {})
    {
        const auto result = interchange::projectSmfBytes (value, chosen, "Refusal", limits);
        CHECK (result.status == expected);
        CHECK (result.bytes.empty());
        CHECK (result.exportedClips == 0u);
    };
    SECTION ("empty means none and unknown IDs refuse")
    {
        CHECK (interchange::projectSmfBytes (project, {}, "None").status == ProjectSmfStatus::NoUnmutedClips);
        const engine::EntityId unknown[] = { id (99) };
        CHECK (interchange::projectSmfBytes (project, unknown, "Unknown").status == ProjectSmfStatus::InvalidInput);
        project.midiClips[0].muted = true;
        refused (project, ProjectSmfStatus::NoUnmutedClips);
    }
    SECTION ("in-horizon Meter narrowing is refused")
    {
        project.meterMap[0] = { 0, 7, 39 };
        refused (project, ProjectSmfStatus::UnsupportedMeter);
        project.meterMap[0] = { 0, 256, 4 };
        refused (project, ProjectSmfStatus::UnsupportedMeter);
    }
    SECTION ("small resource budgets reject complete raw and clock plans")
    {
        auto limits = interchange::ProjectSmfLimits {};
        limits.maxOccurrenceEvents = 1;
        refused (project, ProjectSmfStatus::ResourceLimit, limits);
        limits = {}; limits.maxClockEvents = 1;
        refused (project, ProjectSmfStatus::ResourceLimit, limits);
        limits = {}; limits.maxFileBytes = 32;
        refused (project, ProjectSmfStatus::ResourceLimit, limits);
    }
    SECTION ("a sparse four-byte VLQ gap is not narrowed")
    {
        project.midiClips[0].notes.clear();
        project.midiClips[0].timelineLength = 0x10000000LL;
        refused (project, ProjectSmfStatus::FileLimit);
    }
    SECTION ("large fixed Frame inputs are not silently converted to doubles")
    {
        project.midiClips[0] = clipOf (id (20), id (2), 9007199254740993LL, 1, engine::TimeBase::SampleLocked);
        refused (project, ProjectSmfStatus::NumericPrecision);
    }
    SECTION ("one-Tick clock error outside the SMF tempo range refuses")
    {
        project.midiClips[0].notes.clear(); project.midiClips[0].timelineLength = 1;
        project.tempoMap[0].bpm = 1.0;
        refused (project, ProjectSmfStatus::NumericPrecision);
    }
}

TEST_CASE ("Project SMF excludes metadata after a fractional horizon even at ceil EOT", "[project-smf][clock]")
{
    auto project = baseProject();
    project.tempoMap.push_back ({ 1, 0.001, engine::TempoCurve::Jump });
    project.meterMap.push_back ({ 1, 7, 39 });
    project.midiClips = { clipOf (id (20), id (2), 0, 1, engine::TimeBase::SampleLocked) };
    const engine::EntityId chosen[] = { id (20) };
    const auto result = interchange::projectSmfBytes (project, chosen, "Fractional");
    REQUIRE (result.status == ProjectSmfStatus::Ok);
    const auto file = parse (result.bytes);
    CHECK (file.tracks[0].back().tick == 1u);
    for (const auto& event : file.tracks[0])
        if (event.meta == 0x51u || event.meta == 0x58u) CHECK (event.tick == 0u);
    checkClock (file, [] (long double tick) { return tick * 500000.0L / 15360.0L; },
                [] (std::uint32_t) { return -1.0L; });
}

#include <cstdlib>
#include <new>

namespace project_smf_allocation_probe {
thread_local bool enabled = false;
thread_local std::size_t largest = 0;
thread_local std::size_t requested = 0;
}

void* operator new (std::size_t size)
{
    if (project_smf_allocation_probe::enabled)
        project_smf_allocation_probe::largest = std::max (project_smf_allocation_probe::largest, size);
    if (project_smf_allocation_probe::enabled) project_smf_allocation_probe::requested += size;
    if (void* memory = std::malloc (size == 0 ? 1u : size)) return memory;
    throw std::bad_alloc {};
}

void operator delete (void* memory) noexcept { std::free (memory); }
void operator delete (void* memory, std::size_t) noexcept { std::free (memory); }

TEST_CASE ("Project SMF ordinary NoteOn ties preserve full source identity before velocity", "[project-smf][regression]")
{
    auto project = baseProject();
    auto clip = clipOf (id (20), id (2), 0, 15360);
    clip.notes = { noteOf (1, 0, 15360, 60, 1.0), noteOf (2, 0, 15360, 60, 0.5) };
    project.midiClips = { clip };
    const engine::EntityId chosen[] = { id (20) };
    const auto result = interchange::projectSmfBytes (project, chosen, "Identity");
    REQUIRE (result.status == ProjectSmfStatus::Ok);
    const auto events = channels (parse (result.bytes).tracks[1]);
    REQUIRE (events.size() == 4u);
    CHECK (events[0].data[1] == 127u);
    CHECK (events[1].data[1] == 64u);
}

TEST_CASE ("Project SMF checks each selected identity and its unique Track reference", "[project-smf][regression]")
{
    auto project = baseProject();
    project.midiClips = { clipOf (id (20), id (2), 0, 15360) };
    SECTION ("duplicate selected Project Clip cannot stand in for a missing requested ID")
    {
        project.midiClips.push_back (project.midiClips[0]);
        const engine::EntityId chosen[] = { id (20), id (21) };
        const auto result = interchange::projectSmfBytes (project, chosen, "Missing");
        CHECK (result.status == ProjectSmfStatus::InvalidInput);
        CHECK (result.bytes.empty());
        CHECK (result.exportedClips == 0u);
    }
    SECTION ("duplicate Project Track cannot duplicate the selected content")
    {
        project.tracks.push_back (project.tracks[0]);
        const engine::EntityId chosen[] = { id (20) };
        const auto result = interchange::projectSmfBytes (project, chosen, "Duplicate Track");
        CHECK (result.status == ProjectSmfStatus::InvalidInput);
        CHECK (result.bytes.empty());
        CHECK (result.exportedClips == 0u);
    }
}

TEST_CASE ("Project SMF refuses an impossible small file budget before allocating plans", "[project-smf][regression]")
{
    auto project = baseProject();
    auto clip = clipOf (id (20), id (2), 0, 15360);
    for (std::uint64_t i = 0; i < 1024; ++i) clip.notes.push_back (noteOf (100 + i, 0, 15360));
    project.midiClips = { std::move (clip) };
    const engine::EntityId chosen[] = { id (20) };
    auto limits = interchange::ProjectSmfLimits {};
    limits.maxFileBytes = 32;
    project_smf_allocation_probe::largest = 0;
    project_smf_allocation_probe::enabled = true;
    const auto result = interchange::projectSmfBytes (project, chosen, "Budget", limits);
    project_smf_allocation_probe::enabled = false;
    CHECK (result.status == ProjectSmfStatus::ResourceLimit);
    CHECK (result.bytes.empty());
    CHECK (project_smf_allocation_probe::largest == 0u);
}

TEST_CASE ("Project SMF working storage and traversal ceilings precede plan allocation", "[project-smf][resources]")
{
    auto project = baseProject();
    auto clip = clipOf (id (20), id (2), 0, 15360);
    for (std::uint64_t i = 0; i < 1024; ++i) clip.notes.push_back (noteOf (100 + i, 0, 15360));
    project.midiClips = { std::move (clip) };
    const engine::EntityId chosen[] = { id (20) };
    auto limits = interchange::ProjectSmfLimits {};
    SECTION ("format lower bound prevents a large event plan after a small selection index")
    {
        limits.maxFileBytes = 80;
    }
    SECTION ("working storage budget prevents even the selection copy")
    {
        limits.maxWorkingBytes = 1;
    }
    SECTION ("work budget prevents even the selection copy")
    {
        limits.maxWork = 0;
    }
    project_smf_allocation_probe::largest = 0;
    project_smf_allocation_probe::enabled = true;
    const auto result = interchange::projectSmfBytes (project, chosen, "Budget", limits);
    project_smf_allocation_probe::enabled = false;
    CHECK (result.status == ProjectSmfStatus::ResourceLimit);
    CHECK (result.bytes.empty());
    CHECK (project_smf_allocation_probe::largest <= sizeof (engine::EntityId));
}

#include <iostream>

TEST_CASE ("Project SMF admitted working budget covers observed container allocation requests", "[project-smf][resources][regression]")
{
    auto project = baseProject();
    project.tracks.clear();
    constexpr std::uint64_t trackCount = 1681;
    for (std::uint64_t index = 0; index < trackCount; ++index)
    {
        engine::Track track;
        track.id = id (10000 + index);
        track.strip.name = "Track";
        project.tracks.push_back (track);
        auto clip = clipOf (id (20000 + index), track.id, 0, 15360);
        for (std::uint64_t note = 0; note < 26; ++note)
            clip.notes.push_back (noteOf (100000 + index * 26 + note, 0, 15360));
        project.midiClips.push_back (std::move (clip));
    }
    std::vector<engine::EntityId> chosen;
    for (const auto& clip : project.midiClips) chosen.push_back (clip.id);
    auto limits = interchange::ProjectSmfLimits {};
    const auto accepts = [&] (std::size_t budget)
    {
        limits.maxWorkingBytes = budget;
        const auto result = interchange::projectSmfBytes (project, chosen, "Allocations", limits);
        REQUIRE ((result.status == ProjectSmfStatus::Ok || result.status == ProjectSmfStatus::ResourceLimit));
        return result.status == ProjectSmfStatus::Ok;
    };
    std::size_t low = 0, high = limits.maxWorkingBytes;
    REQUIRE (accepts (high));
    while (low < high)
    {
        const auto middle = low + (high - low) / 2u;
        if (accepts (middle)) high = middle;
        else low = middle + 1u;
    }
    limits.maxWorkingBytes = low;
    project_smf_allocation_probe::requested = 0;
    project_smf_allocation_probe::largest = 0;
    project_smf_allocation_probe::enabled = true;
    const auto result = interchange::projectSmfBytes (project, chosen, "Allocations", limits);
    project_smf_allocation_probe::enabled = false;
    const auto observed = project_smf_allocation_probe::requested;
    std::cout << "allocation certificate: tracks=" << trackCount << " accepted-budget=" << low
              << " observed-requests=" << observed << '\n';
    REQUIRE (result.status == ProjectSmfStatus::Ok);
    CAPTURE (low, observed);
    CHECK (observed <= low);
}
