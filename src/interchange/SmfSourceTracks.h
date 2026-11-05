#pragma once

#include "interchange/Smf.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace yesdaw::interchange {

struct SmfSourceNote
{
    std::uint32_t start = 0, end = 0;
    std::int16_t key = 60;
    std::uint8_t velocity = 127;
    std::int16_t channel = 0;

    friend bool operator== (const SmfSourceNote&, const SmfSourceNote&) = default;
};

struct SmfSourceControl
{
    std::uint32_t tick = 0;
    engine::MidiControlKind kind = engine::MidiControlKind::ControlChange;
    std::int16_t number = 0;
    double value = 0.0;
    std::int16_t channel = 0;

    friend bool operator== (const SmfSourceControl&, const SmfSourceControl&) = default;
};

struct SmfSourceTrack
{
    std::string name;
    std::vector<SmfSourceNote> notes;
    std::vector<SmfSourceControl> controls;
    std::uint32_t lastTick = 0;

    friend bool operator== (const SmfSourceTrack&, const SmfSourceTrack&) = default;
};

// Positions are absolute source-PPQ endpoints. Pairing and output ordering match
// smfMusicalTracks; decoded event normalization remains the parser's responsibility.
[[nodiscard]] inline std::vector<SmfSourceTrack> smfSourceTracks (const SmfFile& file)
{
    std::vector<SmfSourceTrack> out;
    for (const auto& track : file.tracks)
    {
        SmfSourceTrack source;
        struct Open { std::uint32_t tick; std::uint8_t velocity; };
        std::vector<std::pair<std::uint16_t, Open>> open;
        for (const auto& event : track.events)
        {
            source.lastTick = std::max (source.lastTick, event.tick);
            const auto slot = static_cast<std::uint16_t> (event.channel * 128 + event.data1);
            const auto closeNote = [&] (std::uint32_t end)
            {
                for (auto it = open.begin(); it != open.end(); ++it)
                    if (it->first == slot)
                    {
                        source.notes.push_back ({ it->second.tick, std::max (it->second.tick, end),
                            static_cast<std::int16_t> (event.data1), it->second.velocity,
                            static_cast<std::int16_t> (event.channel) });
                        open.erase (it);
                        return;
                    }
            };
            switch (event.kind)
            {
                case SmfEventKind::NoteOn:
                    closeNote (event.tick);
                    open.push_back ({ slot, Open { event.tick, event.data2 } });
                    break;
                case SmfEventKind::NoteOff:
                    closeNote (event.tick);
                    break;
                case SmfEventKind::ControlChange:
                    source.controls.push_back ({ event.tick, engine::MidiControlKind::ControlChange,
                        static_cast<std::int16_t> (event.data1), static_cast<double> (event.data2) / 127.0,
                        static_cast<std::int16_t> (event.channel) });
                    break;
                case SmfEventKind::ProgramChange:
                    source.controls.push_back ({ event.tick, engine::MidiControlKind::ProgramChange,
                        static_cast<std::int16_t> (event.data1), 0.0, static_cast<std::int16_t> (event.channel) });
                    break;
                case SmfEventKind::ChannelPressure:
                    source.controls.push_back ({ event.tick, engine::MidiControlKind::ChannelPressure,
                        0, static_cast<double> (event.data1) / 127.0, static_cast<std::int16_t> (event.channel) });
                    break;
                case SmfEventKind::PolyPressure:
                    source.controls.push_back ({ event.tick, engine::MidiControlKind::PolyPressure,
                        static_cast<std::int16_t> (event.data1), static_cast<double> (event.data2) / 127.0,
                        static_cast<std::int16_t> (event.channel) });
                    break;
                case SmfEventKind::PitchBend:
                {
                    const int bend14 = static_cast<int> (event.data1) | (static_cast<int> (event.data2) << 7);
                    source.controls.push_back ({ event.tick, engine::MidiControlKind::PitchBend,
                        0, static_cast<double> (bend14 - 8192) / 8192.0, static_cast<std::int16_t> (event.channel) });
                    break;
                }
                case SmfEventKind::TrackName:
                    if (source.name.empty()) source.name = event.text;
                    break;
                case SmfEventKind::Tempo:
                case SmfEventKind::TimeSignature:
                case SmfEventKind::EndOfTrack:
                case SmfEventKind::Other:
                    break;
            }
        }
        for (const auto& [slot, opened] : open)
            source.notes.push_back ({ opened.tick, std::max (opened.tick, source.lastTick),
                static_cast<std::int16_t> (slot % 128), opened.velocity, static_cast<std::int16_t> (slot / 128) });
        std::stable_sort (source.notes.begin(), source.notes.end(),
            [] (const SmfSourceNote& a, const SmfSourceNote& b) { return a.start < b.start; });
        if (! source.notes.empty() || ! source.controls.empty()) out.push_back (std::move (source));
    }
    return out;
}

}
