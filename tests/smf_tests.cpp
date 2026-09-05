// G3.7 — Standard MIDI File read / write (src/interchange/Smf.h): the byte law and the musical bridge.
//
// Gates:
//  1. Byte golden: a two-track format-1 file (tempo track + one note track with a CC) writes to the
//     exact bytes the SMF spec dictates — header, chunk lengths, VLQ deltas, meta events, End of Track.
//  2. Round-trip: write → read → the same SmfFile (events in tick order, explicit status); the reader
//     also takes running status and NoteOn-velocity-0 as NoteOff (a hand-written file), skips SysEx
//     and unknown metas (kept as Other at their tick), and refuses a bad header / format 2 / SMPTE
//     division / a truncated track / a bad VLQ.
//  3. VLQ: the spec's worked values (0, 0x7F, 0x80, 0x3FFF, 0x4000, 0x0FFFFFFF) encode and decode.
//  4. The musical bridge: notes pair per channel + key; a re-struck key ends the earlier note; an
//     open note ends at the track's end; controls carry the Clip's normalized values (a bend's 14-bit
//     centre is 0.0, a CC's 127 is 1.0); the head tempo / meter are read; a track without notes or
//     controls (the tempo track) is dropped; musical → file → musical is the identity at 960 PPQ.

#include "interchange/Smf.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <span>
#include <vector>

using namespace yesdaw::interchange;
using yesdaw::engine::MidiControlKind;

namespace {

SmfFile makeTwoTrackFile()
{
    SmfFile file;
    file.format = 1;
    file.ticksPerQuarter = 960;
    SmfTrack head;
    { SmfEvent e; e.kind = SmfEventKind::TrackName; e.text = "YES"; head.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::Tempo; e.tempoMicrosPerQuarter = 500000; head.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::TimeSignature; e.numerator = 3; e.denominatorPow2 = 2; head.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::EndOfTrack; head.events.push_back (e); }   // explicit: the round-trip compares whole
    file.tracks.push_back (head);
    SmfTrack notes;
    { SmfEvent e; e.kind = SmfEventKind::NoteOn; e.tick = 0; e.channel = 0; e.data1 = 60; e.data2 = 100; notes.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::ControlChange; e.tick = 480; e.channel = 0; e.data1 = 1; e.data2 = 64; notes.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::NoteOff; e.tick = 960; e.channel = 0; e.data1 = 60; e.data2 = 64; notes.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::EndOfTrack; e.tick = 1920; notes.events.push_back (e); }
    file.tracks.push_back (notes);
    return file;
}

} // namespace

TEST_CASE ("SMF byte golden: a format-1 file writes the spec's exact bytes", "[smf][golden]")
{
    const std::vector<std::uint8_t> bytes = writeSmf (makeTwoTrackFile());
    const std::vector<std::uint8_t> expected = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0x03, 0xC0,
        // Track 0: name "YES", tempo 500000 (0x07A120), 3/4, End of Track.
        'M', 'T', 'r', 'k', 0, 0, 0, 26,
        0x00, 0xFF, 0x03, 0x03, 'Y', 'E', 'S',
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,
        0x00, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x2F, 0x00,
        // Track 1: NoteOn C4 @0, CC1 @480 (delta 0x83 0x60), NoteOff @960 (delta 480), EOT @1920 (delta 0x87 0x40).
        'M', 'T', 'r', 'k', 0, 0, 0, 19,
        0x00, 0x90, 60, 100,
        0x83, 0x60, 0xB0, 1, 64,
        0x83, 0x60, 0x80, 60, 64,
        0x87, 0x40, 0xFF, 0x2F, 0x00
    };
    REQUIRE (bytes == expected);
}

TEST_CASE ("SMF VLQ: the spec's worked values encode and decode", "[smf]")
{
    struct Case { std::uint32_t value; std::vector<std::uint8_t> bytes; };
    const Case cases[] = {
        { 0x00000000, { 0x00 } },
        { 0x0000007F, { 0x7F } },
        { 0x00000080, { 0x81, 0x00 } },
        { 0x00003FFF, { 0xFF, 0x7F } },
        { 0x00004000, { 0x81, 0x80, 0x00 } },
        { 0x0FFFFFFF, { 0xFF, 0xFF, 0xFF, 0x7F } },
    };
    for (const Case& c : cases)
    {
        std::vector<std::uint8_t> out;
        detail::putVlq (out, c.value);
        REQUIRE (out == c.bytes);
        detail::Cursor in { std::span<const std::uint8_t> (out), 0 };
        std::uint32_t back = 0;
        REQUIRE (in.vlq (back));
        REQUIRE (back == c.value);
        REQUIRE (in.at == out.size());
    }
}

TEST_CASE ("SMF round-trip and the tolerant reader", "[smf]")
{
    const SmfFile file = makeTwoTrackFile();
    const std::vector<std::uint8_t> bytes = writeSmf (file);
    SmfFile back;
    REQUIRE (readSmf (bytes, back) == SmfStatus::Ok);
    REQUIRE (back == file);
    REQUIRE (writeSmf (back) == bytes);

    SECTION ("running status, NoteOn velocity 0, SysEx and unknown meta")
    {
        const std::vector<std::uint8_t> hand = {
            'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x00, 0x60,   // format 0, PPQ 96
            'M', 'T', 'r', 'k', 0, 0, 0, 28,
            0x00, 0x90, 60, 100,          // NoteOn C4
            0x00, 64, 100,                // running status: NoteOn E4
            0x60, 60, 0,                  // running status, velocity 0: NoteOff C4 @96
            0x00, 0xF0, 0x02, 0x7E, 0xF7, // SysEx, skipped (kept as Other)
            0x00, 0xFF, 0x7F, 0x01, 0x42, // sequencer-specific meta, kept as Other
            0x60, 0x80, 64, 0,            // NoteOff E4 @192
            0x00, 0xFF, 0x2F, 0x00
        };
        SmfFile parsed;
        REQUIRE (readSmf (hand, parsed) == SmfStatus::Ok);
        REQUIRE (parsed.format == 0);
        REQUIRE (parsed.ticksPerQuarter == 96);
        REQUIRE (parsed.tracks.size() == 1u);
        const std::vector<SmfEvent>& events = parsed.tracks.front().events;
        REQUIRE (events.size() == 7u);
        REQUIRE (events[0].kind == SmfEventKind::NoteOn);
        REQUIRE (events[1].kind == SmfEventKind::NoteOn);
        REQUIRE (events[1].data1 == 64);
        REQUIRE (events[2].kind == SmfEventKind::NoteOff);
        REQUIRE (events[2].tick == 96);
        REQUIRE (events[3].kind == SmfEventKind::Other);
        REQUIRE (events[4].kind == SmfEventKind::Other);
        REQUIRE (events[5].kind == SmfEventKind::NoteOff);
        REQUIRE (events[5].tick == 192);
        REQUIRE (events[6].kind == SmfEventKind::EndOfTrack);
    }

    SECTION ("refusals")
    {
        SmfFile ignored;
        std::vector<std::uint8_t> bad = bytes;
        bad[0] = 'X';
        REQUIRE (readSmf (bad, ignored) == SmfStatus::InvalidHeader);
        bad = bytes;
        bad[9] = 2;   // format 2
        REQUIRE (readSmf (bad, ignored) == SmfStatus::UnsupportedFormat);
        bad = bytes;
        bad[12] = 0xE7;   // SMPTE division
        REQUIRE (readSmf (bad, ignored) == SmfStatus::UnsupportedFormat);
        bad = bytes;
        bad.resize (bytes.size() - 3);   // the last track cut short
        REQUIRE (readSmf (bad, ignored) == SmfStatus::Truncated);
        REQUIRE (readSmf (std::vector<std::uint8_t> { 'M', 'T', 'h', 'd' }, ignored) == SmfStatus::InvalidHeader);
        const std::vector<std::uint8_t> badVlq = {
            'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0x00, 0x60,
            'M', 'T', 'r', 'k', 0, 0, 0, 9,
            0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x90, 60, 100
        };
        REQUIRE (readSmf (badVlq, ignored) == SmfStatus::BadVlq);
    }
}

TEST_CASE ("SMF musical bridge: notes pair, controls normalize, the head is read, the identity holds", "[smf]")
{
    SmfFile file;
    file.ticksPerQuarter = 480;
    SmfTrack head;
    { SmfEvent e; e.kind = SmfEventKind::Tempo; e.tempoMicrosPerQuarter = 600000; head.events.push_back (e); }   // 100 BPM
    { SmfEvent e; e.kind = SmfEventKind::TimeSignature; e.numerator = 6; e.denominatorPow2 = 3; head.events.push_back (e); }
    file.tracks.push_back (head);
    SmfTrack track;
    { SmfEvent e; e.kind = SmfEventKind::TrackName; e.text = "Keys"; track.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::NoteOn; e.tick = 0; e.data1 = 60; e.data2 = 127; track.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::NoteOn; e.tick = 240; e.channel = 1; e.data1 = 60; e.data2 = 64; track.events.push_back (e); }   // another channel: its own note
    { SmfEvent e; e.kind = SmfEventKind::NoteOn; e.tick = 480; e.data1 = 60; e.data2 = 100; track.events.push_back (e); }   // re-struck: ends the first
    { SmfEvent e; e.kind = SmfEventKind::ControlChange; e.tick = 480; e.data1 = 64; e.data2 = 127; track.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::PitchBend; e.tick = 600; e.data1 = 0x00; e.data2 = 0x40; track.events.push_back (e); }   // 8192: centre
    { SmfEvent e; e.kind = SmfEventKind::ProgramChange; e.tick = 700; e.data1 = 5; track.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::NoteOff; e.tick = 720; e.data1 = 60; track.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::NoteOff; e.tick = 960; e.channel = 1; e.data1 = 60; track.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::NoteOn; e.tick = 960; e.data1 = 72; e.data2 = 50; track.events.push_back (e); }   // left open
    { SmfEvent e; e.kind = SmfEventKind::EndOfTrack; e.tick = 1920; track.events.push_back (e); }
    file.tracks.push_back (track);

    const SmfHead h = smfHead (file);
    REQUIRE (h.bpm == Catch::Approx (100.0));
    REQUIRE (h.numerator == 6);
    REQUIRE (h.denominator == 8);

    const std::vector<SmfMusicalTrack> musical = smfMusicalTracks (file);
    REQUIRE (musical.size() == 1u);   // the tempo track carries no notes: dropped
    const SmfMusicalTrack& keys = musical.front();
    REQUIRE (keys.name == "Keys");
    REQUIRE (keys.lengthQuarters == Catch::Approx (4.0));
    REQUIRE (keys.notes.size() == 4u);
    REQUIRE (keys.notes[0].key == 60);
    REQUIRE (keys.notes[0].startQuarters == Catch::Approx (0.0));
    REQUIRE (keys.notes[0].lengthQuarters == Catch::Approx (1.0));   // ended by the re-strike at 480
    REQUIRE (keys.notes[0].normalizedVelocity == Catch::Approx (1.0));
    REQUIRE (keys.notes[1].channel == 1);
    REQUIRE (keys.notes[1].startQuarters == Catch::Approx (0.5));
    REQUIRE (keys.notes[1].lengthQuarters == Catch::Approx (1.5));
    REQUIRE (keys.notes[2].startQuarters == Catch::Approx (1.0));
    REQUIRE (keys.notes[2].lengthQuarters == Catch::Approx (0.5));
    REQUIRE (keys.notes[3].key == 72);
    REQUIRE (keys.notes[3].lengthQuarters == Catch::Approx (2.0));   // open: ends at the track's end
    REQUIRE (keys.controls.size() == 3u);
    REQUIRE (keys.controls[0].kind == MidiControlKind::ControlChange);
    REQUIRE (keys.controls[0].number == 64);
    REQUIRE (keys.controls[0].value == Catch::Approx (1.0));
    REQUIRE (keys.controls[1].kind == MidiControlKind::PitchBend);
    REQUIRE (keys.controls[1].value == Catch::Approx (0.0));
    REQUIRE (keys.controls[1].quarters == Catch::Approx (1.25));
    REQUIRE (keys.controls[2].kind == MidiControlKind::ProgramChange);
    REQUIRE (keys.controls[2].number == 5);

    // musical → file → musical is the identity at 960 PPQ (every value above sits on a 1/960 grid).
    const SmfFile rebuilt = smfFromMusicalTracks (musical, h.bpm, h.numerator, h.denominator);
    REQUIRE (rebuilt.tracks.size() == 2u);
    REQUIRE (smfHead (rebuilt).bpm == Catch::Approx (100.0));
    REQUIRE (smfHead (rebuilt).numerator == 6);
    REQUIRE (smfHead (rebuilt).denominator == 8);
    SmfFile reread;
    REQUIRE (readSmf (writeSmf (rebuilt), reread) == SmfStatus::Ok);
    const std::vector<SmfMusicalTrack> again = smfMusicalTracks (reread);
    REQUIRE (again.size() == 1u);
    REQUIRE (again.front().name == "Keys");
    REQUIRE (again.front().notes.size() == 4u);
    for (std::size_t i = 0; i < 4; ++i)
    {
        REQUIRE (again.front().notes[i].key == keys.notes[i].key);
        REQUIRE (again.front().notes[i].channel == keys.notes[i].channel);
        REQUIRE (again.front().notes[i].startQuarters == Catch::Approx (keys.notes[i].startQuarters));
        REQUIRE (again.front().notes[i].lengthQuarters == Catch::Approx (keys.notes[i].lengthQuarters));
        REQUIRE (again.front().notes[i].normalizedVelocity == Catch::Approx (keys.notes[i].normalizedVelocity).margin (0.5 / 127.0));
    }
    REQUIRE (again.front().controls.size() == 3u);
    for (std::size_t i = 0; i < 3; ++i)
    {
        REQUIRE (again.front().controls[i].kind == keys.controls[i].kind);
        REQUIRE (again.front().controls[i].number == keys.controls[i].number);
        REQUIRE (again.front().controls[i].quarters == Catch::Approx (keys.controls[i].quarters));
        REQUIRE (again.front().controls[i].value == Catch::Approx (keys.controls[i].value).margin (1.0 / 127.0));
    }
    REQUIRE (again.front().lengthQuarters == Catch::Approx (4.0));
}
