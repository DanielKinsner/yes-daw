#include "interchange/SmfSourceTracks.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace {
using namespace yesdaw::interchange;
using yesdaw::engine::MidiControlKind;

SmfFile parseTracks (std::uint16_t ppq, std::initializer_list<std::vector<std::uint8_t>> tracks)
{
    std::vector<std::uint8_t> bytes {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, static_cast<std::uint8_t> (tracks.size()),
        static_cast<std::uint8_t> (ppq >> 8), static_cast<std::uint8_t> (ppq & 0xFF)
    };
    for (const auto& body : tracks)
    {
        const auto size = static_cast<std::uint32_t> (body.size());
        bytes.insert (bytes.end(), { 'M', 'T', 'r', 'k',
            static_cast<std::uint8_t> (size >> 24), static_cast<std::uint8_t> (size >> 16),
            static_cast<std::uint8_t> (size >> 8), static_cast<std::uint8_t> (size & 0xFF) });
        bytes.insert (bytes.end(), body.begin(), body.end());
    }
    SmfFile file;
    REQUIRE (readSmf (bytes, file) == SmfStatus::Ok);
    return file;
}

SmfEvent noteEvent (SmfEventKind kind, std::uint32_t tick, std::uint8_t key,
                    std::uint8_t velocity = 100, std::uint8_t channel = 0)
{
    SmfEvent event;
    event.kind = kind;
    event.tick = tick;
    event.data1 = key;
    event.data2 = velocity;
    event.channel = channel;
    return event;
}
} // namespace

TEST_CASE ("SMF source tracks preserve integer endpoints and legacy pairing without quarter conversion",
           "[smf-source-tracks][adr0082]")
{
    SECTION ("re-strike isolates key and channel while equal-onset notes retain closure order")
    {
        const std::vector<std::uint8_t> bytes {
            'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x03, 0xE9,
            'M', 'T', 'r', 'k', 0, 0, 0, 49,
            0x00, 0xFF, 0x03, 0x04, 'K', 'e', 'y', 's',
            0x00, 0x90, 60, 127,
            0x00, 0x91, 60, 64,
            0x00, 0x90, 64, 32,
            0x0A, 0x80, 64, 0,
            0x0A, 0x90, 60, 100,
            0x0A, 0x81, 60, 0,
            0x0A, 0x80, 60, 0,
            0x00, 0x90, 72, 50,
            0x14, 0xFF, 0x7F, 0x01, 0x42,
            0x00, 0xFF, 0x2F, 0x00
        };
        SmfFile file;
        REQUIRE (readSmf (bytes, file) == SmfStatus::Ok);
        CHECK (file.ticksPerQuarter == 1001);
        const auto source = smfSourceTracks (file);
        REQUIRE (source.size() == 1);
        CHECK (source[0].name == "Keys");
        CHECK (source[0].lastTick == 60);
        CHECK (source[0].controls.empty());
        const std::vector<SmfSourceNote> expected {
            { 0, 10, 64, 32, 0 }, { 0, 20, 60, 127, 0 }, { 0, 30, 60, 64, 1 },
            { 20, 40, 60, 100, 0 }, { 40, 60, 72, 50, 0 }
        };
        CHECK (source[0].notes == expected);
    }
    SECTION ("noncanonical PPQ retains absolute endpoints and zero-length pairs")
    {
        const auto file = parseTracks (1001, { {
            0x01, 0x90, 60, 100, 0x01, 0x80, 60, 0,
            0x01, 0x90, 64, 127, 0x00, 0x80, 64, 0, 0x00, 0xFF, 0x2F, 0x00
        } });
        CHECK (file.ticksPerQuarter == 1001);
        const auto source = smfSourceTracks (file);
        REQUIRE (source.size() == 1);
        CHECK (source[0].lastTick == 3);
        CHECK (source[0].notes == std::vector<SmfSourceNote> { { 1, 2, 60, 100, 0 }, { 3, 3, 64, 127, 0 } });
    }
    SECTION ("controller-only tracks preserve all kinds and same-tick event order")
    {
        const auto file = parseTracks (480, { {
            0x01, 0xB2, 64, 127, 0x00, 0xC3, 5,
            0x01, 0xD4, 64, 0x00, 0xA5, 67, 32,
            0x01, 0xE6, 0, 0, 0x01, 0xE6, 0, 64, 0x01, 0xE6, 127, 127,
            0x00, 0xFF, 0x2F, 0x00
        } });
        const auto source = smfSourceTracks (file);
        REQUIRE (source.size() == 1);
        CHECK (source[0].notes.empty());
        CHECK (source[0].lastTick == 5);
        REQUIRE (source[0].controls.size() == 7);
        const std::vector<SmfSourceControl> expected {
            { 1, MidiControlKind::ControlChange, 64, 1.0, 2 },
            { 1, MidiControlKind::ProgramChange, 5, 0.0, 3 },
            { 2, MidiControlKind::ChannelPressure, 0, 64.0 / 127.0, 4 },
            { 2, MidiControlKind::PolyPressure, 67, 32.0 / 127.0, 5 },
            { 3, MidiControlKind::PitchBend, 0, -1.0, 6 },
            { 4, MidiControlKind::PitchBend, 0, 0.0, 6 },
            { 5, MidiControlKind::PitchBend, 0, 8191.0 / 8192.0, 6 }
        };
        CHECK (source[0].controls == expected);
    }
    SECTION ("first nonempty name and retained track order survive metadata-only drops and missing EOT")
    {
        const auto file = parseTracks (480, {
            { 0x00, 0xFF, 0x51, 0x03, 0x09, 0x27, 0xC0, 0x32, 0xFF, 0x2F, 0x00 },
            { 0x00, 0xFF, 0x03, 0x00,
              0x00, 0xFF, 0x03, 0x04, 'K', 'e', 'y', 's',
              0x00, 0xFF, 0x03, 0x05, 'L', 'a', 't', 'e', 'r',
              0x01, 0xB0, 1, 64, 0x3B, 0xFF, 0x2F, 0x00 },
            { 0x00, 0xFF, 0x03, 0x04, 'D', 'r', 'o', 'p' },
            { 0x07, 0x90, 72, 50, 0x0A, 0xFF, 0x7F, 0x01, 0x42 },
            { 0x00, 0xFF, 0x2F, 0x00 }
        });
        const auto source = smfSourceTracks (file);
        REQUIRE (source.size() == 2);
        CHECK (source[0].name == "Keys");
        CHECK (source[0].lastTick == 60);
        CHECK (source[0].notes.empty());
        REQUIRE (source[0].controls.size() == 1);
        CHECK (source[0].controls[0].tick == 1);
        CHECK (source[1].name.empty());
        CHECK (source[1].lastTick == 17);
        CHECK (source[1].notes == std::vector<SmfSourceNote> { { 7, 17, 72, 50, 0 } });
    }
    SECTION ("running status and velocity-zero are decoded before pairing")
    {
        const auto file = parseTracks (96, { {
            0x00, 0x90, 60, 100, 0x00, 64, 100,
            0x60, 60, 0,
            0x00, 0xF0, 0x02, 0x7E, 0xF7,
            0x60, 0x80, 64, 0, 0x00, 0xFF, 0x2F, 0x00
        } });
        REQUIRE (file.tracks.size() == 1);
        REQUIRE (file.tracks[0].events.size() == 6);
        CHECK (file.tracks[0].events[2].kind == SmfEventKind::NoteOff);
        CHECK (file.tracks[0].events[2].tick == 96);
        CHECK (file.tracks[0].events[3].kind == SmfEventKind::Other);
        const auto source = smfSourceTracks (file);
        REQUIRE (source.size() == 1);
        CHECK (source[0].lastTick == 192);
        CHECK (source[0].notes == std::vector<SmfSourceNote> { { 0, 96, 60, 100, 0 }, { 0, 192, 64, 100, 0 } });
    }
    SECTION ("open-note flush uses activation order and every event contributes to extent")
    {
        SmfFile file;
        SmfTrack track;
        track.events = {
            noteEvent (SmfEventKind::NoteOn, 0, 72, 90),
            noteEvent (SmfEventKind::NoteOn, 0, 60, 80),
            noteEvent (SmfEventKind::NoteOn, 0, 67, 70),
            noteEvent (SmfEventKind::NoteOn, 0, 72, 50)
        };
        SmfEvent extent;
        extent.kind = SmfEventKind::TrackName;
        extent.tick = 90;
        extent.text = "Tail";
        track.events.push_back (extent);
        file.tracks.push_back (track);
        const auto source = smfSourceTracks (file);
        REQUIRE (source.size() == 1);
        CHECK (source[0].name == "Tail");
        CHECK (source[0].lastTick == 90);
        CHECK (source[0].notes == std::vector<SmfSourceNote> {
            { 0, 0, 72, 90, 0 }, { 0, 90, 60, 80, 0 }, { 0, 90, 67, 70, 0 }, { 0, 90, 72, 50, 0 }
        });
    }
    SECTION ("bounded integer endpoints and direct event semantics require no PPQ division")
    {
        for (const auto ppq : { std::uint16_t { 1 }, std::uint16_t { 1001 }, std::uint16_t { 32767 } })
        {
            SmfFile file;
            file.ticksPerQuarter = ppq;
            file.tracks.push_back ({ {
                noteEvent (SmfEventKind::NoteOn, UINT32_MAX - 1, 60, 64, 15),
                noteEvent (SmfEventKind::NoteOff, UINT32_MAX, 60, 0, 15)
            } });
            const auto source = smfSourceTracks (file);
            REQUIRE (source.size() == 1);
            CHECK (source[0].lastTick == UINT32_MAX);
            CHECK (source[0].notes == std::vector<SmfSourceNote> { { UINT32_MAX - 1, UINT32_MAX, 60, 64, 15 } });
        }
        SmfFile backwards;
        backwards.tracks.push_back ({ {
            noteEvent (SmfEventKind::NoteOn, 20, 64, 0, 1),
            noteEvent (SmfEventKind::NoteOff, 10, 64, 0, 1)
        } });
        const auto source = smfSourceTracks (backwards);
        REQUIRE (source.size() == 1);
        CHECK (source[0].lastTick == 20);
        CHECK (source[0].notes == std::vector<SmfSourceNote> { { 20, 20, 64, 0, 1 } });
    }
    SECTION ("empty extraction and maximum accepted PPQ keep parsing at its existing boundary")
    {
        CHECK (smfSourceTracks (SmfFile {}).empty());
        const auto empty = parseTracks (480, { { 0x00, 0xFF, 0x2F, 0x00 } });
        CHECK (smfSourceTracks (empty).empty());
        const auto file = parseTracks (32767, { { 0x00, 0x90, 60, 100, 0x01, 0x80, 60, 0 } });
        CHECK (file.ticksPerQuarter == 32767);
        const auto source = smfSourceTracks (file);
        REQUIRE (source.size() == 1);
        CHECK (source[0].notes == std::vector<SmfSourceNote> { { 0, 1, 60, 100, 0 } });
        const std::vector<std::uint8_t> smpte {
            'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 0, 0x80, 0x60
        };
        SmfFile refused;
        CHECK (readSmf (smpte, refused) == SmfStatus::UnsupportedFormat);
    }
}
