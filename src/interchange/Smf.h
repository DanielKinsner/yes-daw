// YES DAW — Standard MIDI File (SMF) read / write and the bridge to the Project's edit model (G3.7).
//
// Pure C++, no JUCE: a byte-exact, deterministic writer (explicit status bytes, events in tick
// order, one End of Track per track) and a tolerant reader (formats 0 and 1, running status, every
// meta and SysEx event skipped or kept as Other). The bridge speaks MUSICAL units — quarter notes as
// doubles — so the shell decides the tempo law: an imported file lands on the project's own beats
// (Logic's law), and an export takes the project's head tempo and meter.
//
// Facts this file relies on (the MIDI 1.0 / SMF spec): header "MThd", chunk length 6, format / ntrks
// / division (ticks per quarter when the top bit is clear); "MTrk" chunks of <delta VLQ><event>;
// meta 0xFF <type> <length VLQ> <bytes> (0x03 track name, 0x51 tempo in µs per quarter, 0x58 time
// signature, 0x2F end of track); SysEx 0xF0 / 0xF7 <length VLQ>; channel messages 0x8n..0xEn with
// one or two data bytes (0xCn / 0xDn take one); a NoteOn with velocity 0 is a NoteOff.

#pragma once

#include "engine/Project.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace yesdaw::interchange {

enum class SmfStatus : std::uint8_t
{
    Ok = 0,
    InvalidHeader,
    UnsupportedFormat,     // format 2, or a SMPTE division
    Truncated,
    BadVlq,
    BadTrack
};

enum class SmfEventKind : std::uint8_t
{
    NoteOn,
    NoteOff,
    ControlChange,
    ProgramChange,
    ChannelPressure,
    PolyPressure,
    PitchBend,
    Tempo,            // meta 0x51: tempoMicrosPerQuarter
    TimeSignature,    // meta 0x58: numerator, denominatorPow2
    TrackName,        // meta 0x03: text
    EndOfTrack,       // meta 0x2F
    Other             // any other meta / SysEx (kept for the tick, never written back)
};

struct SmfEvent
{
    std::uint32_t tick = 0;   // absolute, in the file's ticks per quarter
    SmfEventKind  kind = SmfEventKind::Other;
    std::uint8_t  channel = 0;   // 0..15
    std::uint8_t  data1 = 0;
    std::uint8_t  data2 = 0;
    std::uint32_t tempoMicrosPerQuarter = 500000;
    std::uint8_t  numerator = 4;
    std::uint8_t  denominatorPow2 = 2;
    std::string   text;

    friend bool operator== (const SmfEvent&, const SmfEvent&) = default;
};

struct SmfTrack
{
    std::vector<SmfEvent> events;   // in tick order (the writer sorts stably; the reader keeps file order)

    friend bool operator== (const SmfTrack&, const SmfTrack&) = default;
};

struct SmfFile
{
    std::uint16_t format = 1;
    std::uint16_t ticksPerQuarter = 960;
    std::vector<SmfTrack> tracks;

    friend bool operator== (const SmfFile&, const SmfFile&) = default;
};

namespace detail {

inline void putU16 (std::vector<std::uint8_t>& out, std::uint16_t v)
{
    out.push_back (static_cast<std::uint8_t> (v >> 8));
    out.push_back (static_cast<std::uint8_t> (v & 0xFF));
}

inline void putU32 (std::vector<std::uint8_t>& out, std::uint32_t v)
{
    out.push_back (static_cast<std::uint8_t> (v >> 24));
    out.push_back (static_cast<std::uint8_t> ((v >> 16) & 0xFF));
    out.push_back (static_cast<std::uint8_t> ((v >> 8) & 0xFF));
    out.push_back (static_cast<std::uint8_t> (v & 0xFF));
}

// A variable-length quantity: 7 bits per byte, high bit set on every byte but the last, big-endian.
inline void putVlq (std::vector<std::uint8_t>& out, std::uint32_t value)
{
    std::uint8_t bytes[5];
    int count = 0;
    do
    {
        bytes[count++] = static_cast<std::uint8_t> (value & 0x7F);
        value >>= 7;
    } while (value != 0 && count < 5);
    for (int i = count - 1; i >= 0; --i)
        out.push_back (static_cast<std::uint8_t> (bytes[i] | (i > 0 ? 0x80 : 0x00)));
}

struct Cursor
{
    std::span<const std::uint8_t> bytes;
    std::size_t at = 0;

    [[nodiscard]] bool has (std::size_t n) const noexcept { return at + n <= bytes.size(); }
    [[nodiscard]] std::uint8_t u8() noexcept { return bytes[at++]; }
    [[nodiscard]] std::uint16_t u16() noexcept { const std::uint16_t v = static_cast<std::uint16_t> ((bytes[at] << 8) | bytes[at + 1]); at += 2; return v; }
    [[nodiscard]] std::uint32_t u32() noexcept
    {
        const std::uint32_t v = (static_cast<std::uint32_t> (bytes[at]) << 24) | (static_cast<std::uint32_t> (bytes[at + 1]) << 16)
                              | (static_cast<std::uint32_t> (bytes[at + 2]) << 8) | bytes[at + 3];
        at += 4;
        return v;
    }
    [[nodiscard]] bool vlq (std::uint32_t& out) noexcept
    {
        out = 0;
        for (int i = 0; i < 5; ++i)
        {
            if (! has (1))
                return false;
            const std::uint8_t b = u8();
            out = (out << 7) | (b & 0x7F);
            if ((b & 0x80) == 0)
                return true;
        }
        return false;   // more than five bytes: not a VLQ
    }
};

[[nodiscard]] inline int channelDataBytes (std::uint8_t status) noexcept
{
    const std::uint8_t high = static_cast<std::uint8_t> (status & 0xF0);
    return (high == 0xC0 || high == 0xD0) ? 1 : 2;
}

} // namespace detail

// Write a file: header, then each track as one MTrk of delta-timed events in tick order (a stable
// sort keeps same-tick events in the given order), every event with an explicit status byte, and
// an End of Track appended when the track has none. Deterministic: the same SmfFile gives the same
// bytes.
[[nodiscard]] inline std::vector<std::uint8_t> writeSmf (const SmfFile& file)
{
    std::vector<std::uint8_t> out;
    out.insert (out.end(), { 'M', 'T', 'h', 'd' });
    detail::putU32 (out, 6);
    detail::putU16 (out, file.format);
    detail::putU16 (out, static_cast<std::uint16_t> (file.tracks.size()));
    detail::putU16 (out, static_cast<std::uint16_t> (file.ticksPerQuarter & 0x7FFF));

    for (const SmfTrack& track : file.tracks)
    {
        std::vector<SmfEvent> events = track.events;
        std::stable_sort (events.begin(), events.end(), [] (const SmfEvent& a, const SmfEvent& b) { return a.tick < b.tick; });
        // Exactly one End of Track, last, at the latest tick.
        std::uint32_t lastTick = 0;
        for (const SmfEvent& e : events)
            lastTick = std::max (lastTick, e.tick);
        events.erase (std::remove_if (events.begin(), events.end(), [] (const SmfEvent& e) { return e.kind == SmfEventKind::EndOfTrack; }), events.end());
        SmfEvent end;
        end.kind = SmfEventKind::EndOfTrack;
        end.tick = lastTick;
        events.push_back (end);

        std::vector<std::uint8_t> body;
        std::uint32_t previousTick = 0;
        for (const SmfEvent& e : events)
        {
            detail::putVlq (body, e.tick >= previousTick ? e.tick - previousTick : 0);
            previousTick = std::max (previousTick, e.tick);
            const std::uint8_t channel = static_cast<std::uint8_t> (e.channel & 0x0F);
            switch (e.kind)
            {
                case SmfEventKind::NoteOn:          body.push_back (static_cast<std::uint8_t> (0x90 | channel)); body.push_back (e.data1 & 0x7F); body.push_back (e.data2 & 0x7F); break;
                case SmfEventKind::NoteOff:         body.push_back (static_cast<std::uint8_t> (0x80 | channel)); body.push_back (e.data1 & 0x7F); body.push_back (e.data2 & 0x7F); break;
                case SmfEventKind::ControlChange:   body.push_back (static_cast<std::uint8_t> (0xB0 | channel)); body.push_back (e.data1 & 0x7F); body.push_back (e.data2 & 0x7F); break;
                case SmfEventKind::ProgramChange:   body.push_back (static_cast<std::uint8_t> (0xC0 | channel)); body.push_back (e.data1 & 0x7F); break;
                case SmfEventKind::ChannelPressure: body.push_back (static_cast<std::uint8_t> (0xD0 | channel)); body.push_back (e.data1 & 0x7F); break;
                case SmfEventKind::PolyPressure:    body.push_back (static_cast<std::uint8_t> (0xA0 | channel)); body.push_back (e.data1 & 0x7F); body.push_back (e.data2 & 0x7F); break;
                case SmfEventKind::PitchBend:       body.push_back (static_cast<std::uint8_t> (0xE0 | channel)); body.push_back (e.data1 & 0x7F); body.push_back (e.data2 & 0x7F); break;
                case SmfEventKind::Tempo:
                    body.insert (body.end(), { 0xFF, 0x51, 0x03 });
                    body.push_back (static_cast<std::uint8_t> ((e.tempoMicrosPerQuarter >> 16) & 0xFF));
                    body.push_back (static_cast<std::uint8_t> ((e.tempoMicrosPerQuarter >> 8) & 0xFF));
                    body.push_back (static_cast<std::uint8_t> (e.tempoMicrosPerQuarter & 0xFF));
                    break;
                case SmfEventKind::TimeSignature:
                    body.insert (body.end(), { 0xFF, 0x58, 0x04, e.numerator, e.denominatorPow2, 24, 8 });
                    break;
                case SmfEventKind::TrackName:
                    body.insert (body.end(), { 0xFF, 0x03 });
                    detail::putVlq (body, static_cast<std::uint32_t> (e.text.size()));
                    body.insert (body.end(), e.text.begin(), e.text.end());
                    break;
                case SmfEventKind::EndOfTrack:
                    body.insert (body.end(), { 0xFF, 0x2F, 0x00 });
                    break;
                case SmfEventKind::Other:
                    // Unknown meta / SysEx is never re-emitted: write a zero-length sequencer meta so the
                    // delta time is kept (a tick placeholder), which every reader skips.
                    body.insert (body.end(), { 0xFF, 0x7F, 0x00 });
                    break;
            }
        }
        out.insert (out.end(), { 'M', 'T', 'r', 'k' });
        detail::putU32 (out, static_cast<std::uint32_t> (body.size()));
        out.insert (out.end(), body.begin(), body.end());
    }
    return out;
}

// Read a file (formats 0 and 1; running status; every meta / SysEx event at least kept as Other).
[[nodiscard]] inline SmfStatus readSmf (std::span<const std::uint8_t> bytes, SmfFile& out)
{
    out = {};
    detail::Cursor in { bytes, 0 };
    if (! in.has (14) || bytes[0] != 'M' || bytes[1] != 'T' || bytes[2] != 'h' || bytes[3] != 'd')
        return SmfStatus::InvalidHeader;
    in.at = 4;
    const std::uint32_t headerLength = in.u32();
    if (headerLength < 6 || ! in.has (headerLength))
        return SmfStatus::InvalidHeader;
    out.format = in.u16();
    const std::uint16_t trackCount = in.u16();
    const std::uint16_t division = in.u16();
    in.at += headerLength - 6;
    if (out.format > 1)
        return SmfStatus::UnsupportedFormat;
    if ((division & 0x8000) != 0 || division == 0)
        return SmfStatus::UnsupportedFormat;
    out.ticksPerQuarter = division;

    for (std::uint16_t t = 0; t < trackCount; ++t)
    {
        if (! in.has (8))
            return SmfStatus::Truncated;
        const bool isTrack = bytes[in.at] == 'M' && bytes[in.at + 1] == 'T' && bytes[in.at + 2] == 'r' && bytes[in.at + 3] == 'k';
        in.at += 4;
        const std::uint32_t length = in.u32();
        if (! in.has (length))
            return SmfStatus::Truncated;
        if (! isTrack)
        {
            in.at += length;   // an alien chunk: skipped (the spec asks readers to)
            continue;
        }
        detail::Cursor body { bytes.subspan (in.at, length), 0 };
        in.at += length;

        SmfTrack track;
        std::uint32_t tick = 0;
        std::uint8_t runningStatus = 0;
        bool ended = false;
        while (! ended && body.has (1))
        {
            std::uint32_t delta = 0;
            if (! body.vlq (delta))
                return SmfStatus::BadVlq;
            tick += delta;
            if (! body.has (1))
                return SmfStatus::Truncated;
            std::uint8_t status = body.u8();
            if (status == 0xFF)
            {
                if (! body.has (1))
                    return SmfStatus::Truncated;
                const std::uint8_t type = body.u8();
                std::uint32_t metaLength = 0;
                if (! body.vlq (metaLength) || ! body.has (metaLength))
                    return SmfStatus::Truncated;
                SmfEvent e;
                e.tick = tick;
                const std::span<const std::uint8_t> data = body.bytes.subspan (body.at, metaLength);
                body.at += metaLength;
                if (type == 0x2F) { e.kind = SmfEventKind::EndOfTrack; ended = true; }
                else if (type == 0x51 && metaLength == 3)
                {
                    e.kind = SmfEventKind::Tempo;
                    e.tempoMicrosPerQuarter = (static_cast<std::uint32_t> (data[0]) << 16) | (static_cast<std::uint32_t> (data[1]) << 8) | data[2];
                }
                else if (type == 0x58 && metaLength >= 2)
                {
                    e.kind = SmfEventKind::TimeSignature;
                    e.numerator = data[0];
                    e.denominatorPow2 = data[1];
                }
                else if (type == 0x03)
                {
                    e.kind = SmfEventKind::TrackName;
                    e.text.assign (data.begin(), data.end());
                }
                else
                    e.kind = SmfEventKind::Other;
                track.events.push_back (e);
                continue;
            }
            if (status == 0xF0 || status == 0xF7)
            {
                std::uint32_t sysexLength = 0;
                if (! body.vlq (sysexLength) || ! body.has (sysexLength))
                    return SmfStatus::Truncated;
                body.at += sysexLength;
                SmfEvent e;
                e.tick = tick;
                e.kind = SmfEventKind::Other;
                track.events.push_back (e);
                runningStatus = 0;
                continue;
            }
            std::uint8_t data1 = 0;
            if (status < 0x80)
            {
                // Running status: this byte is the first data byte of a repeat of the last channel message.
                if (runningStatus == 0)
                    return SmfStatus::BadTrack;
                data1 = status;
                status = runningStatus;
            }
            else
            {
                runningStatus = status;
                if (! body.has (1))
                    return SmfStatus::Truncated;
                data1 = body.u8();
            }
            std::uint8_t data2 = 0;
            if (detail::channelDataBytes (status) == 2)
            {
                if (! body.has (1))
                    return SmfStatus::Truncated;
                data2 = body.u8();
            }
            SmfEvent e;
            e.tick = tick;
            e.channel = static_cast<std::uint8_t> (status & 0x0F);
            e.data1 = static_cast<std::uint8_t> (data1 & 0x7F);
            e.data2 = static_cast<std::uint8_t> (data2 & 0x7F);
            switch (status & 0xF0)
            {
                case 0x80: e.kind = SmfEventKind::NoteOff; break;
                case 0x90: e.kind = e.data2 == 0 ? SmfEventKind::NoteOff : SmfEventKind::NoteOn; break;   // velocity 0 is a NoteOff
                case 0xA0: e.kind = SmfEventKind::PolyPressure; break;
                case 0xB0: e.kind = SmfEventKind::ControlChange; break;
                case 0xC0: e.kind = SmfEventKind::ProgramChange; break;
                case 0xD0: e.kind = SmfEventKind::ChannelPressure; break;
                case 0xE0: e.kind = SmfEventKind::PitchBend; break;
                default: return SmfStatus::BadTrack;
            }
            track.events.push_back (e);
        }
        out.tracks.push_back (std::move (track));
    }
    return SmfStatus::Ok;
}

// --- The bridge: SMF tracks <-> notes and control points in quarter notes (the shell applies the
// --- tempo law; the engine's MidiControlKind and normalized values are the same as the Clip's).

struct SmfNote
{
    double startQuarters = 0.0;
    double lengthQuarters = 0.0;
    std::int16_t key = 60;
    double normalizedVelocity = 1.0;
    std::int16_t channel = 0;

    friend bool operator== (const SmfNote&, const SmfNote&) = default;
};

struct SmfControl
{
    double quarters = 0.0;
    engine::MidiControlKind kind = engine::MidiControlKind::ControlChange;
    std::int16_t number = 0;
    double value = 0.0;
    std::int16_t channel = 0;

    friend bool operator== (const SmfControl&, const SmfControl&) = default;
};

struct SmfMusicalTrack
{
    std::string name;
    std::vector<SmfNote> notes;
    std::vector<SmfControl> controls;
    double lengthQuarters = 0.0;   // the End of Track (or the last event's end)

    friend bool operator== (const SmfMusicalTrack&, const SmfMusicalTrack&) = default;
};

// The file's musical tracks: notes paired per channel + key (a NoteOn with velocity 0 is an off; a
// note left open ends at the track's end), control points from the channel messages, the name from
// the track-name meta. A track with neither notes nor controls is dropped (format 1's tempo track).
[[nodiscard]] inline std::vector<SmfMusicalTrack> smfMusicalTracks (const SmfFile& file)
{
    std::vector<SmfMusicalTrack> out;
    const double ppq = static_cast<double> (file.ticksPerQuarter > 0 ? file.ticksPerQuarter : 960);
    for (const SmfTrack& track : file.tracks)
    {
        SmfMusicalTrack musical;
        struct Open { std::uint32_t tick; std::uint8_t velocity; };
        std::vector<std::pair<std::uint16_t, Open>> open;   // key: channel * 128 + note
        std::uint32_t lastTick = 0;
        for (const SmfEvent& e : track.events)
        {
            lastTick = std::max (lastTick, e.tick);
            const std::uint16_t slot = static_cast<std::uint16_t> (e.channel * 128 + e.data1);
            const auto closeNote = [&] (std::uint32_t endTick)
            {
                for (auto it = open.begin(); it != open.end(); ++it)
                    if (it->first == slot)
                    {
                        SmfNote note;
                        note.startQuarters = static_cast<double> (it->second.tick) / ppq;
                        note.lengthQuarters = static_cast<double> (endTick >= it->second.tick ? endTick - it->second.tick : 0) / ppq;
                        note.key = static_cast<std::int16_t> (e.data1);
                        note.normalizedVelocity = static_cast<double> (it->second.velocity) / 127.0;
                        note.channel = static_cast<std::int16_t> (e.channel);
                        musical.notes.push_back (note);
                        open.erase (it);
                        return;
                    }
            };
            switch (e.kind)
            {
                case SmfEventKind::NoteOn:
                    closeNote (e.tick);   // a re-struck key ends the earlier note
                    open.push_back ({ slot, Open { e.tick, e.data2 } });
                    break;
                case SmfEventKind::NoteOff:
                    closeNote (e.tick);
                    break;
                case SmfEventKind::ControlChange:
                    musical.controls.push_back ({ static_cast<double> (e.tick) / ppq, engine::MidiControlKind::ControlChange, static_cast<std::int16_t> (e.data1), static_cast<double> (e.data2) / 127.0, static_cast<std::int16_t> (e.channel) });
                    break;
                case SmfEventKind::ProgramChange:
                    musical.controls.push_back ({ static_cast<double> (e.tick) / ppq, engine::MidiControlKind::ProgramChange, static_cast<std::int16_t> (e.data1), 0.0, static_cast<std::int16_t> (e.channel) });
                    break;
                case SmfEventKind::ChannelPressure:
                    musical.controls.push_back ({ static_cast<double> (e.tick) / ppq, engine::MidiControlKind::ChannelPressure, 0, static_cast<double> (e.data1) / 127.0, static_cast<std::int16_t> (e.channel) });
                    break;
                case SmfEventKind::PolyPressure:
                    musical.controls.push_back ({ static_cast<double> (e.tick) / ppq, engine::MidiControlKind::PolyPressure, static_cast<std::int16_t> (e.data1), static_cast<double> (e.data2) / 127.0, static_cast<std::int16_t> (e.channel) });
                    break;
                case SmfEventKind::PitchBend:
                {
                    const int bend14 = static_cast<int> (e.data1) | (static_cast<int> (e.data2) << 7);
                    musical.controls.push_back ({ static_cast<double> (e.tick) / ppq, engine::MidiControlKind::PitchBend, 0, static_cast<double> (bend14 - 8192) / 8192.0, static_cast<std::int16_t> (e.channel) });
                    break;
                }
                case SmfEventKind::TrackName:
                    if (musical.name.empty())
                        musical.name = e.text;
                    break;
                case SmfEventKind::Tempo:
                case SmfEventKind::TimeSignature:
                case SmfEventKind::EndOfTrack:
                case SmfEventKind::Other:
                    break;
            }
        }
        // Notes still open end at the track's end.
        for (const auto& [slot, opened] : open)
        {
            SmfNote note;
            note.startQuarters = static_cast<double> (opened.tick) / ppq;
            note.lengthQuarters = static_cast<double> (lastTick >= opened.tick ? lastTick - opened.tick : 0) / ppq;
            note.key = static_cast<std::int16_t> (slot % 128);
            note.normalizedVelocity = static_cast<double> (opened.velocity) / 127.0;
            note.channel = static_cast<std::int16_t> (slot / 128);
            musical.notes.push_back (note);
        }
        std::stable_sort (musical.notes.begin(), musical.notes.end(), [] (const SmfNote& a, const SmfNote& b) { return a.startQuarters < b.startQuarters; });
        musical.lengthQuarters = static_cast<double> (lastTick) / ppq;
        if (! musical.notes.empty() || ! musical.controls.empty())
            out.push_back (std::move (musical));
    }
    return out;
}

// The head tempo and meter a file declares (its first tempo / time-signature events), or the defaults.
struct SmfHead
{
    double bpm = 120.0;
    int numerator = 4;
    int denominator = 4;
};

[[nodiscard]] inline SmfHead smfHead (const SmfFile& file)
{
    SmfHead head;
    bool tempoSeen = false, meterSeen = false;
    for (const SmfTrack& track : file.tracks)
        for (const SmfEvent& e : track.events)
        {
            if (! tempoSeen && e.kind == SmfEventKind::Tempo && e.tempoMicrosPerQuarter > 0)
            {
                head.bpm = 60000000.0 / static_cast<double> (e.tempoMicrosPerQuarter);
                tempoSeen = true;
            }
            if (! meterSeen && e.kind == SmfEventKind::TimeSignature && e.numerator > 0)
            {
                head.numerator = e.numerator;
                head.denominator = 1 << e.denominatorPow2;
                meterSeen = true;
            }
        }
    return head;
}

// Build a format-1 file: track 0 carries the tempo, the time signature and a name; one track per
// musical track after it. Ticks round to the nearest tick at the given resolution; a zero-length
// note still gets an Off after its On (the engine allows zero-length notes; the file pairs them).
[[nodiscard]] inline SmfFile smfFromMusicalTracks (std::span<const SmfMusicalTrack> tracks,
                                                    double bpm,
                                                    int numerator,
                                                    int denominator,
                                                    std::uint16_t ticksPerQuarter = 960,
                                                    const std::string& sequenceName = "YES DAW")
{
    SmfFile file;
    file.format = 1;
    file.ticksPerQuarter = ticksPerQuarter;
    const double ppq = static_cast<double> (ticksPerQuarter);
    const auto ticksOf = [ppq] (double quarters) { return static_cast<std::uint32_t> (std::max (0.0, quarters) * ppq + 0.5); };

    SmfTrack head;
    { SmfEvent e; e.kind = SmfEventKind::TrackName; e.text = sequenceName; head.events.push_back (e); }
    { SmfEvent e; e.kind = SmfEventKind::Tempo; e.tempoMicrosPerQuarter = static_cast<std::uint32_t> (60000000.0 / std::clamp (bpm, 20.0, 400.0) + 0.5); head.events.push_back (e); }
    {
        SmfEvent e;
        e.kind = SmfEventKind::TimeSignature;
        e.numerator = static_cast<std::uint8_t> (std::clamp (numerator, 1, 255));
        int pow2 = 0;
        for (int d = std::max (1, denominator); d > 1 && pow2 < 7; d >>= 1)
            ++pow2;
        e.denominatorPow2 = static_cast<std::uint8_t> (pow2);
        head.events.push_back (e);
    }
    file.tracks.push_back (head);

    for (const SmfMusicalTrack& musical : tracks)
    {
        SmfTrack track;
        if (! musical.name.empty())
        {
            SmfEvent e;
            e.kind = SmfEventKind::TrackName;
            e.text = musical.name;
            track.events.push_back (e);
        }
        for (const SmfNote& note : musical.notes)
        {
            const std::uint8_t channel = static_cast<std::uint8_t> (std::clamp<int> (note.channel, 0, 15));
            const std::uint8_t key = static_cast<std::uint8_t> (std::clamp<int> (note.key, 0, 127));
            const std::uint8_t velocity = static_cast<std::uint8_t> (std::clamp<int> (static_cast<int> (note.normalizedVelocity * 127.0 + 0.5), 1, 127));
            SmfEvent on;  on.kind = SmfEventKind::NoteOn;  on.tick = ticksOf (note.startQuarters); on.channel = channel; on.data1 = key; on.data2 = velocity;
            SmfEvent off; off.kind = SmfEventKind::NoteOff; off.tick = ticksOf (note.startQuarters + note.lengthQuarters); off.channel = channel; off.data1 = key; off.data2 = 64;
            track.events.push_back (on);
            track.events.push_back (off);
        }
        for (const SmfControl& control : musical.controls)
        {
            SmfEvent e;
            e.tick = ticksOf (control.quarters);
            e.channel = static_cast<std::uint8_t> (std::clamp<int> (control.channel, 0, 15));
            const auto seven = [] (double v) { return static_cast<std::uint8_t> (std::clamp<int> (static_cast<int> (v * 127.0 + 0.5), 0, 127)); };
            switch (control.kind)
            {
                case engine::MidiControlKind::ControlChange:   e.kind = SmfEventKind::ControlChange;   e.data1 = static_cast<std::uint8_t> (control.number & 0x7F); e.data2 = seven (control.value); break;
                case engine::MidiControlKind::ProgramChange:   e.kind = SmfEventKind::ProgramChange;   e.data1 = static_cast<std::uint8_t> (control.number & 0x7F); break;
                case engine::MidiControlKind::ChannelPressure: e.kind = SmfEventKind::ChannelPressure; e.data1 = seven (control.value); break;
                case engine::MidiControlKind::PolyPressure:    e.kind = SmfEventKind::PolyPressure;    e.data1 = static_cast<std::uint8_t> (control.number & 0x7F); e.data2 = seven (control.value); break;
                case engine::MidiControlKind::PitchBend:
                {
                    const int bend14 = std::clamp<int> (static_cast<int> ((std::clamp (control.value, -1.0, 1.0) + 1.0) * 0.5 * 16383.0 + 0.5), 0, 16383);
                    e.kind = SmfEventKind::PitchBend;
                    e.data1 = static_cast<std::uint8_t> (bend14 & 0x7F);
                    e.data2 = static_cast<std::uint8_t> ((bend14 >> 7) & 0x7F);
                    break;
                }
            }
            track.events.push_back (e);
        }
        // The track's length: an End of Track at the declared length (the writer places it last).
        SmfEvent end;
        end.kind = SmfEventKind::EndOfTrack;
        end.tick = ticksOf (musical.lengthQuarters);
        track.events.push_back (end);
        file.tracks.push_back (std::move (track));
    }
    return file;
}

} // namespace yesdaw::interchange
