#pragma once

#include "engine/Midi.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace yesdaw::interchange {

enum class ProjectSmfStatus : std::uint8_t
{
    Ok, InvalidInput, NoUnmutedClips, UnsupportedMeter, NumericPrecision, ResourceLimit, FileLimit
};

struct ProjectSmfLimits
{
    std::size_t maxOccurrenceEvents = 1000000;
    std::size_t maxClockEvents = 262144;
    std::size_t maxFileBytes = 256u * 1024u * 1024u;
    std::size_t maxWorkingBytes = 512u * 1024u * 1024u;
    std::size_t maxWork = 256u * 1024u * 1024u;
};

struct ProjectSmfResult
{
    ProjectSmfStatus status = ProjectSmfStatus::InvalidInput;
    std::vector<std::uint8_t> bytes;
    std::size_t exportedClips = 0;
};

namespace project_smf_detail {

using engine::Tick;
constexpr std::uint32_t maxVlq = 0x0FFFFFFFu;
constexpr double microsPerTickAtOneBpm = 60000000.0 / engine::kTicksPerQuarter;

struct ResourceExceeded {};

struct ResourceBudget
{
    std::size_t memory = 0, work = 0;

    void storage (std::size_t count, std::size_t width)
    {
        if (width != 0 && count > memory / width) throw ResourceExceeded {};
        memory -= count * width;
    }

    void container (std::size_t count, std::size_t width)
    {
        storage (count, width);
        // Covers the supported STL's vector allocation padding (MSVC x64: 39 Release/47 Debug).
        // Allocator request bytes are separate from the bounded stack allowance and from RSS.
        if (count != 0) storage (64u, 1u);
    }

    void visit (std::size_t count = 1, std::size_t factor = 1)
    {
        if (factor != 0 && count > work / factor) throw ResourceExceeded {};
        work -= count * factor;
    }
};

[[nodiscard]] inline bool addSize (std::size_t& value, std::size_t extra) noexcept
{
    if (extra > std::numeric_limits<std::size_t>::max() - value)
        return false;
    value += extra;
    return true;
}

[[nodiscard]] inline bool before (engine::MusicalTime a, engine::MusicalTime b) noexcept
{
    return a.tick < b.tick || (a.tick == b.tick && a.frac < b.frac);
}

enum class WireGroup : std::uint8_t
{
    Metadata, Controllers, OrdinaryOffs, ZeroLengthPairs, OrdinaryOns, EndOfTrack
};

struct WireEvent
{
    Tick tick = 0;
    engine::EntityId clipId, sourceId;
    std::uint64_t repeat = 0;
    std::string_view text;
    std::uint32_t value = 0;
    std::uint8_t status = 0, data1 = 0, data2 = 0, meta = 0;
    WireGroup group = WireGroup::Metadata;
    bool on = false;
};

[[nodiscard]] inline bool identityLess (const WireEvent& a, const WireEvent& b) noexcept
{
    if (a.clipId != b.clipId) return a.clipId < b.clipId;
    if (a.sourceId != b.sourceId) return a.sourceId < b.sourceId;
    return a.repeat < b.repeat;
}

[[nodiscard]] inline bool wireLess (const WireEvent& a, const WireEvent& b) noexcept
{
    if (a.tick != b.tick) return a.tick < b.tick;
    if (a.group != b.group) return a.group < b.group;
    if (a.group == WireGroup::ZeroLengthPairs)
    {
        if (identityLess (a, b)) return true;
        if (identityLess (b, a)) return false;
        return a.on && ! b.on;
    }
    if (a.meta != b.meta) return a.meta < b.meta;
    if (a.status != b.status) return a.status < b.status;
    if (a.data1 != b.data1) return a.data1 < b.data1;
    if (a.group == WireGroup::OrdinaryOffs || a.group == WireGroup::OrdinaryOns) return identityLess (a, b);
    if (a.data2 != b.data2) return a.data2 < b.data2;
    return identityLess (a, b);
}

struct TrackPlan
{
    std::vector<WireEvent> events;
    std::size_t bodyBytes = 0;
};

[[nodiscard]] inline WireEvent metadata (Tick tick, std::uint8_t kind, std::uint32_t value = 0)
{
    WireEvent event;
    event.tick = tick;
    event.meta = kind;
    event.value = value;
    event.group = kind == 0x2Fu ? WireGroup::EndOfTrack : WireGroup::Metadata;
    return event;
}

struct ClockSegment
{
    Tick origin = 0, end = 0;
    double startBpm = 120.0, endBpm = 120.0;
    bool ramp = false;

    [[nodiscard]] bool duration (Tick a, double span, double& micros, double& condition) const noexcept
    {
        const Tick length = end - origin;
        const Tick offset = a - origin;
        double bpm = startBpm;
        double slope = 0.0;
        if (ramp)
        {
            slope = (endBpm - startBpm) / static_cast<double> (length);
            if (! std::isfinite (slope) || slope == 0.0)
                return false;
            bpm = offset <= length / 2 ? startBpm + slope * static_cast<double> (offset)
                : endBpm - slope * static_cast<double> (length - offset);
        }
        const double lastBpm = bpm + slope * span;
        if (! std::isfinite (bpm) || bpm <= 0.0 || ! std::isfinite (lastBpm) || lastBpm <= 0.0)
            return false;
        condition = std::max (1.0, std::max (startBpm, endBpm) / std::min (bpm, lastBpm));
        micros = span * (microsPerTickAtOneBpm / bpm);
        if (ramp && span != 0.0)
        {
            const double relative = (slope * span) / bpm;
            if (! std::isfinite (relative) || relative <= -1.0 || relative == 0.0)
                return false;
            micros *= std::log1p (relative) / relative;
        }
        return std::isfinite (micros) && micros >= 0.0 && std::isfinite (condition);
    }
};

template <typename Emit>
[[nodiscard]] inline ProjectSmfStatus clockPlan (std::span<const engine::TempoChange> changes,
                                                Tick horizon, bool includeEndChange, std::size_t budget,
                                                ResourceBudget& resources, Emit emit)
{
    std::size_t count = 0;
    double residual = 0.0, uncertainty = 0.0;
    const auto append = [&] (Tick tick, std::uint32_t tempo)
    {
        if (count == budget) return false;
        ++count;
        return emit (tick, tempo);
    };
    const auto interval = [&] (auto&& self, const ClockSegment& segment, Tick a, Tick b) -> ProjectSmfStatus
    {
        resources.visit();
        const Tick ticks = b - a;
        const double span = static_cast<double> (ticks);
        double original = 0.0, condition = 0.0;
        bool valid = segment.duration (a, span, original, condition);
        const double ideal = valid ? (original - residual) * engine::kTicksPerQuarter / span : 0.0;
        std::uint32_t tempo = 1;
        if (std::isfinite (ideal))
            tempo = static_cast<std::uint32_t> (std::clamp (std::round (ideal), 1.0, 16777215.0));
        else
            valid = false;
        const double encoded = span * (static_cast<double> (tempo) / engine::kTicksPerQuarter);
        const double nextResidual = residual + (encoded - original);
        const double allowance = 256.0 * std::numeric_limits<double>::epsilon()
            * (std::abs (original) + std::abs (encoded) + std::abs (residual) + 1.0) * condition;
        double worst = std::max (std::abs (residual), std::abs (nextResidual));
        if (valid && segment.ramp)
        {
            const double slope = (segment.endBpm - segment.startBpm) / static_cast<double> (segment.end - segment.origin);
            const Tick offset = a - segment.origin;
            const Tick length = segment.end - segment.origin;
            const double bpm = offset <= length / 2 ? segment.startBpm + slope * static_cast<double> (offset)
                : segment.endBpm - slope * static_cast<double> (length - offset);
            const double extremum = ((60000000.0 / tempo) - bpm) / slope;
            if (extremum > 0.0 && extremum < span)
            {
                double at = 0.0, atCondition = 0.0;
                valid = segment.duration (a, extremum, at, atCondition);
                worst = std::max (worst, std::abs (residual + extremum * tempo / engine::kTicksPerQuarter - at));
            }
        }
        if (valid && std::isfinite (allowance) && std::isfinite (worst)
            && worst + uncertainty + allowance <= 100.0)
        {
            if (! append (a, tempo)) return ProjectSmfStatus::ResourceLimit;
            residual = nextResidual;
            uncertainty += allowance;
            return ProjectSmfStatus::Ok;
        }
        if (ticks <= 1) return ProjectSmfStatus::NumericPrecision;
        const Tick middle = a + ticks / 2;
        const auto left = self (self, segment, a, middle);
        return left == ProjectSmfStatus::Ok ? self (self, segment, middle, b) : left;
    };
    Tick cursor = 0;
    const std::size_t segmentCount = changes.empty() ? 1u : changes.size();
    for (std::size_t i = 0; i < segmentCount && cursor < horizon; ++i)
    {
        resources.visit();
        const bool next = ! changes.empty() && i + 1u < changes.size();
        ClockSegment segment;
        segment.origin = changes.empty() ? 0 : changes[i].tick;
        segment.end = next ? changes[i + 1u].tick : horizon;
        segment.startBpm = changes.empty() ? 120.0 : changes[i].bpm;
        segment.endBpm = next ? changes[i + 1u].bpm : segment.startBpm;
        segment.ramp = next && changes[i].curveToNext == engine::TempoCurve::LinearRamp
            && segment.startBpm != segment.endBpm;
        const Tick end = std::min (horizon, segment.end);
        const auto status = interval (interval, segment, cursor, end);
        if (status != ProjectSmfStatus::Ok) return status;
        cursor = end;
    }
    const auto atEnd = std::find_if (changes.begin(), changes.end(), [&] (const auto& c) { resources.visit(); return c.tick == horizon; });
    if (horizon == 0 || (includeEndChange && atEnd != changes.end()))
    {
        const double bpm = atEnd == changes.end() ? 120.0 : atEnd->bpm;
        const double micros = std::round (60000000.0 / bpm);
        if (! std::isfinite (micros) || micros < 1.0 || micros > 16777215.0)
            return ProjectSmfStatus::NumericPrecision;
        if (! append (horizon, static_cast<std::uint32_t> (micros))) return ProjectSmfStatus::ResourceLimit;
    }
    return ProjectSmfStatus::Ok;
}

[[nodiscard]] inline std::size_t vlqSize (std::uint32_t value) noexcept
{
    std::size_t bytes = 1;
    while ((value >>= 7u) != 0u) ++bytes;
    return bytes;
}

inline void putNumber (std::vector<std::uint8_t>& bytes, std::uint32_t value, unsigned width)
{
    for (unsigned i = width; i > 0; --i)
        bytes.push_back (static_cast<std::uint8_t> (value >> ((i - 1u) * 8u)));
}

inline void putVlq (std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
    const auto width = vlqSize (value);
    for (std::size_t i = width; i > 0; --i)
        bytes.push_back (static_cast<std::uint8_t> (((value >> ((i - 1u) * 7u)) & 0x7Fu) | (i > 1u ? 0x80u : 0u)));
}

[[nodiscard]] inline ProjectSmfStatus measureTrack (TrackPlan& track, std::size_t byteBudget, ResourceBudget& resources)
{
    std::sort (track.events.begin(), track.events.end(), [&] (const auto& a, const auto& b)
        { resources.visit(); return wireLess (a, b); });
    Tick previous = 0;
    for (const auto& event : track.events)
    {
        resources.visit();
        if (event.tick < previous || event.tick - previous > maxVlq) return ProjectSmfStatus::FileLimit;
        std::size_t size = vlqSize (static_cast<std::uint32_t> (event.tick - previous));
        previous = event.tick;
        if (event.meta == 0x03u)
        {
            if (event.text.size() > maxVlq) return ProjectSmfStatus::FileLimit;
            if (! addSize (size, 2u + vlqSize (static_cast<std::uint32_t> (event.text.size())))
                || ! addSize (size, event.text.size())) return ProjectSmfStatus::FileLimit;
        }
        else
        {
            std::size_t payloadBytes = 0;
            switch (event.meta)
            {
                case 0x51u: payloadBytes = 6u; break;
                case 0x58u: payloadBytes = 7u; break;
                case 0x2Fu: payloadBytes = 3u; break;
                default: payloadBytes = (event.status & 0xF0u) == 0xC0u || (event.status & 0xF0u) == 0xD0u ? 2u : 3u; break;
            }
            if (! addSize (size, payloadBytes)) return ProjectSmfStatus::FileLimit;
        }
        if (! addSize (track.bodyBytes, size) || track.bodyBytes > std::numeric_limits<std::uint32_t>::max())
            return ProjectSmfStatus::FileLimit;
        if (track.bodyBytes > byteBudget) return ProjectSmfStatus::ResourceLimit;
    }
    return ProjectSmfStatus::Ok;
}

inline void writeTrack (const TrackPlan& track, std::vector<std::uint8_t>& bytes)
{
    bytes.insert (bytes.end(), { 'M', 'T', 'r', 'k' });
    putNumber (bytes, static_cast<std::uint32_t> (track.bodyBytes), 4);
    Tick previous = 0;
    for (const auto& event : track.events)
    {
        putVlq (bytes, static_cast<std::uint32_t> (event.tick - previous));
        previous = event.tick;
        if (event.meta != 0)
        {
            bytes.push_back (0xFFu);
            bytes.push_back (event.meta);
            switch (event.meta)
            {
                case 0x03u:
                    putVlq (bytes, static_cast<std::uint32_t> (event.text.size()));
                    bytes.insert (bytes.end(), event.text.begin(), event.text.end());
                    break;
                case 0x51u: bytes.push_back (3u); putNumber (bytes, event.value, 3); break;
                case 0x58u: bytes.insert (bytes.end(), { 4u, event.data1, event.data2, 24u, 8u }); break;
                default: bytes.push_back (0u); break;
            }
        }
        else
        {
            bytes.push_back (event.status);
            bytes.push_back (event.data1);
            if ((event.status & 0xF0u) != 0xC0u && (event.status & 0xF0u) != 0xD0u)
                bytes.push_back (event.data2);
        }
    }
}

}

// The caller resolves selection-else-all. Empty IDs select nothing; no destination is opened here.
[[nodiscard]] inline ProjectSmfResult projectSmfBytes (const engine::Project& project,
                                                       std::span<const engine::EntityId> chosenMidiClipIds,
                                                       std::string_view sequenceName,
                                                       ProjectSmfLimits limits = {})
{
    using namespace project_smf_detail;
    const auto failure = [] (ProjectSmfStatus status) { return ProjectSmfResult { status, {}, 0 }; };
    try
    {
        if (chosenMidiClipIds.empty()) return failure (ProjectSmfStatus::NoUnmutedClips);
        // Header, two chunk headers, two empty names, Tempo, Meter and two EOTs.
        if (limits.maxFileBytes < 61u) return failure (ProjectSmfStatus::ResourceLimit);
        ResourceBudget resources { limits.maxWorkingBytes, limits.maxWork };
        constexpr std::size_t stackAllowanceBytes = 65536u;
        resources.storage (stackAllowanceBytes, 1u);
        resources.visit (project.tempoMap.size());
        for (std::size_t i = 0; i < project.tempoMap.size(); ++i)
            if (! project.tempoMap[i].hasValidBpm() || (i == 0 ? project.tempoMap[i].tick != 0
                : project.tempoMap[i].tick <= project.tempoMap[i - 1u].tick)) return failure (ProjectSmfStatus::InvalidInput);
        resources.visit (chosenMidiClipIds.size());
        resources.container (chosenMidiClipIds.size(), sizeof (engine::EntityId));
        std::vector<engine::EntityId> ids (chosenMidiClipIds.begin(), chosenMidiClipIds.end());
        std::sort (ids.begin(), ids.end(), [&] (const auto& a, const auto& b) { resources.visit(); return a < b; });
        resources.visit (ids.size());
        ids.erase (std::unique (ids.begin(), ids.end()), ids.end());
        resources.container (ids.size(), sizeof (std::uint8_t));
        std::vector<std::uint8_t> matches (ids.size(), 0u);
        resources.container (ids.size(), sizeof (const engine::MidiClip*));
        std::vector<const engine::MidiClip*> clips;
        clips.reserve (ids.size());
        std::size_t rawCount = 0;
        bool fixed = false;
        for (const auto& clip : project.midiClips)
        {
            resources.visit();
            const auto requested = std::lower_bound (ids.begin(), ids.end(), clip.id,
                [&] (const auto& a, const auto& b) { resources.visit(); return a < b; });
            if (requested != ids.end() && *requested == clip.id)
            {
                const auto index = static_cast<std::size_t> (requested - ids.begin());
                if (matches[index] != 0u) return failure (ProjectSmfStatus::InvalidInput);
                matches[index] = 1u;
                resources.visit (clip.notes.size(), 4u);
                resources.visit (clip.controlEvents.size(), 4u);
                resources.visit (project.tracks.size());
                const auto owners = std::count_if (project.tracks.begin(), project.tracks.end(),
                    [&] (const auto& track) { return track.id == clip.trackId; });
                if (! clip.isValid() || clip.timelineStart < 0 || owners != 1
                    || clip.timelineLength > std::numeric_limits<Tick>::max() - clip.timelineStart)
                    return failure (ProjectSmfStatus::InvalidInput);
                const auto preflight = engine::preflightMidiClipOccurrences (clip, limits.maxOccurrenceEvents - rawCount);
                if (preflight.status != engine::MidiFlattenStatus::Ok)
                    return failure (preflight.status == engine::MidiFlattenStatus::OutputTooSmall
                        ? ProjectSmfStatus::ResourceLimit : ProjectSmfStatus::InvalidInput);
                rawCount += preflight.events;
                if (! clip.muted)
                {
                    clips.push_back (&clip);
                    fixed = fixed || clip.timeBase == engine::TimeBase::SampleLocked;
                }
            }
        }
        resources.visit (matches.size());
        if (std::find (matches.begin(), matches.end(), 0u) != matches.end()) return failure (ProjectSmfStatus::InvalidInput);
        if (clips.empty()) return failure (ProjectSmfStatus::NoUnmutedClips);
        engine::CompiledTempoMap inverse;
        if (fixed)
        {
            resources.container (project.tempoMap.size(), sizeof (engine::TempoChange));
            resources.container (project.tempoMap.size(), sizeof (double));
            resources.visit (project.tempoMap.size(), 4u);
            if (! engine::CompiledTempoMap::build ({ project.tempoMap.data(), project.tempoMap.size() }, project.sampleRate, inverse))
                return failure (ProjectSmfStatus::NumericPrecision);
        }
        const auto musical = [&] (engine::TimeBase base, Tick native, engine::MusicalTime& out)
        {
            resources.visit();
            if (native < 0) return false;
            if (base == engine::TimeBase::TempoLocked) { out = { native, 0.0 }; return true; }
            // A wider fixed-Frame domain needs an interval certificate, not a lossy cast.
            if (native > 9007199254740992LL) return false;
            return inverse.musicalTimeForFrame (static_cast<double> (native), out) == engine::TimeLookupStatus::Ok;
        };
        engine::MusicalTime horizon;
        std::size_t trackCount = 1;
        for (const auto& track : project.tracks)
            if (std::any_of (clips.begin(), clips.end(), [&] (const auto* c) { resources.visit(); return c->trackId == track.id; })) ++trackCount;
        if (trackCount > 65535u) return failure (ProjectSmfStatus::FileLimit);
        if (rawCount > std::vector<WireEvent> {}.max_size()) return failure (ProjectSmfStatus::ResourceLimit);
        for (const auto* clip : clips)
        {
            engine::MusicalTime end;
            if (! musical (clip->timeBase, clip->timelineStart + clip->timelineLength, end))
                return failure (ProjectSmfStatus::NumericPrecision);
            if (before (horizon, end)) horizon = end;
        }
        Tick wireEnd = 0;
        if (engine::roundMusicalTime (horizon, engine::TimeRounding::Ceil, wireEnd) != engine::TimeLookupStatus::Ok)
            return failure (ProjectSmfStatus::NumericPrecision);
        std::size_t meterCount = project.meterMap.empty() ? 1u : 0u;
        for (std::size_t i = 0; i < project.meterMap.size(); ++i)
        {
            resources.visit();
            const auto& meter = project.meterMap[i];
            if (i == 0 ? meter.tick != 0 : meter.tick <= project.meterMap[i - 1u].tick) return failure (ProjectSmfStatus::InvalidInput);
            if (before (horizon, { meter.tick, 0.0 })) continue;
            if (meter.numerator == 0 || meter.numerator > 255 || meter.denominator == 0
                || (meter.denominator & (meter.denominator - 1u)) != 0u) return failure (ProjectSmfStatus::UnsupportedMeter);
            if (meterCount == limits.maxClockEvents) return failure (ProjectSmfStatus::ResourceLimit);
            ++meterCount;
        }
        if (meterCount > limits.maxClockEvents) return failure (ProjectSmfStatus::ResourceLimit);
        std::size_t clockCount = 0;
        const auto clockStatus = clockPlan (project.tempoMap, wireEnd, horizon.frac == 0.0,
            limits.maxClockEvents - meterCount, resources, [&] (Tick, std::uint32_t) { ++clockCount; return true; });
        if (clockStatus != ProjectSmfStatus::Ok) return failure (clockStatus);
        std::size_t minimumBytes = 14u;
        const auto minimum = [&] (std::size_t count, std::size_t width)
        {
            if (width != 0 && count > (std::numeric_limits<std::size_t>::max() - minimumBytes) / width)
                return false;
            minimumBytes += count * width;
            return true;
        };
        const auto nameMinimum = [&] (std::string_view text)
        {
            return text.size() <= maxVlq && minimum (1u, 3u + vlqSize (static_cast<std::uint32_t> (text.size())))
                && minimum (text.size(), 1u);
        };
        if (! minimum (trackCount, 12u) || ! minimum (clockCount, 7u) || ! minimum (meterCount, 8u)
            || ! nameMinimum (sequenceName)) return failure (ProjectSmfStatus::FileLimit);
        for (const auto& track : project.tracks)
            if (std::any_of (clips.begin(), clips.end(), [&] (const auto* c) { resources.visit(); return c->trackId == track.id; })
                && ! nameMinimum (track.strip.name)) return failure (ProjectSmfStatus::FileLimit);
        if (minimumBytes > limits.maxFileBytes) return failure (ProjectSmfStatus::ResourceLimit);
        for (const auto* clip : clips)
        {
            resources.visit (clip->notes.size(), 3u);
            resources.visit (clip->controlEvents.size(), 3u);
            bool silent = false, sizeFailed = false;
            const auto status = engine::visitMidiClipOccurrences (*clip, limits.maxOccurrenceEvents,
                [&] (const engine::MidiNativeOccurrence& occurrence)
                {
                    resources.visit();
                    const auto& event = occurrence.event;
                    if (event.type == engine::EventType::NoteOn) silent = event.payload.note.normalizedVelocity == 0.0;
                    if ((event.type == engine::EventType::NoteOn || event.type == engine::EventType::NoteOff) && silent) return true;
                    std::size_t bytes = 4u;
                    if (event.type == engine::EventType::Midi1)
                    {
                        const auto high = event.payload.midi1.status & 0xF0u;
                        if (high == 0xC0u || high == 0xD0u) bytes = 3u;
                    }
                    sizeFailed = ! minimum (1u, bytes) || minimumBytes > limits.maxFileBytes;
                    return ! sizeFailed;
                });
            if (status != engine::MidiFlattenStatus::Ok)
                return failure (sizeFailed ? ProjectSmfStatus::ResourceLimit : ProjectSmfStatus::InvalidInput);
        }
        resources.container (meterCount, sizeof (WireEvent));
        std::vector<WireEvent> meters;
        meters.reserve (meterCount);
        const auto appendMeter = [&] (const engine::MeterChange& meter)
        {
            auto event = metadata (meter.tick, 0x58u);
            event.data1 = static_cast<std::uint8_t> (meter.numerator);
            auto denominator = meter.denominator;
            while (denominator > 1u) { ++event.data2; denominator = static_cast<std::uint16_t> (denominator >> 1u); }
            meters.push_back (event);
        };
        if (project.meterMap.empty())
            appendMeter ({});
        for (const auto& meter : project.meterMap)
        {
            resources.visit();
            if (! before (horizon, { meter.tick, 0.0 })) appendMeter (meter);
        }
        std::size_t metadataCount = clockCount;
        if (! addSize (metadataCount, meters.size()) || ! addSize (metadataCount, 2u)
            || metadataCount > std::vector<WireEvent> {}.max_size()) return failure (ProjectSmfStatus::ResourceLimit);
        if (sequenceName.size() > maxVlq) return failure (ProjectSmfStatus::FileLimit);
        if (sequenceName.size() > limits.maxFileBytes) return failure (ProjectSmfStatus::ResourceLimit);
        resources.container (trackCount, sizeof (TrackPlan));
        std::vector<TrackPlan> tracks (trackCount);
        auto& conductor = tracks.front().events;
        resources.container (metadataCount, sizeof (WireEvent));
        conductor.reserve (metadataCount);
        auto name = metadata (0, 0x03u); name.text = sequenceName; conductor.push_back (name);
        const auto replay = clockPlan (project.tempoMap, wireEnd, horizon.frac == 0.0, limits.maxClockEvents - meterCount, resources,
            [&] (Tick tick, std::uint32_t tempo) { conductor.push_back (metadata (tick, 0x51u, tempo)); return true; });
        if (replay != ProjectSmfStatus::Ok) return failure (replay);
        conductor.insert (conductor.end(), meters.begin(), meters.end());
        conductor.push_back (metadata (wireEnd, 0x2Fu));
        std::size_t trackIndex = 1;
        for (const auto& track : project.tracks)
        {
            std::size_t count = 0, clipCount = 0;
            for (const auto* clip : clips)
            {
                resources.visit();
                if (clip->trackId == track.id)
                {
                    ++clipCount;
                    resources.visit (clip->notes.size(), 3u);
                    resources.visit (clip->controlEvents.size(), 3u);
                    const auto preflight = engine::preflightMidiClipOccurrences (*clip, limits.maxOccurrenceEvents - count);
                    if (preflight.status != engine::MidiFlattenStatus::Ok) return failure (ProjectSmfStatus::InvalidInput);
                    count += preflight.events;
                }
            }
            if (clipCount == 0) continue;
            if (track.strip.name.size() > maxVlq) return failure (ProjectSmfStatus::FileLimit);
            if (track.strip.name.size() > limits.maxFileBytes) return failure (ProjectSmfStatus::ResourceLimit);
            if (count > tracks[trackIndex].events.max_size() - 2u) return failure (ProjectSmfStatus::ResourceLimit);
            auto& events = tracks[trackIndex++].events;
            resources.container (count + 2u, sizeof (WireEvent));
            events.reserve (count + 2u);
            name.text = track.strip.name; events.push_back (name);
            Tick trackEnd = 0;
            for (const auto* clip : clips)
            {
                resources.visit();
                if (clip->trackId == track.id)
                {
                    resources.visit (clip->notes.size(), 3u);
                    resources.visit (clip->controlEvents.size(), 3u);
                    engine::MusicalTime end;
                    Tick roundedEnd = 0;
                    if (! musical (clip->timeBase, clip->timelineStart + clip->timelineLength, end)
                        || engine::roundMusicalTime (end, engine::TimeRounding::Ceil, roundedEnd) != engine::TimeLookupStatus::Ok)
                        return failure (ProjectSmfStatus::NumericPrecision);
                    trackEnd = std::max (trackEnd, roundedEnd);
                    bool pending = false, silent = false;
                    std::size_t onIndex = 0;
                    ProjectSmfStatus callbackStatus = ProjectSmfStatus::Ok;
                    const auto status = engine::visitMidiClipOccurrences (*clip, limits.maxOccurrenceEvents,
                        [&] (const engine::MidiNativeOccurrence& occurrence)
                        {
                            resources.visit();
                            const auto& source = occurrence.event;
                            if (source.type == engine::EventType::NoteOn)
                            {
                                pending = true;
                                silent = source.payload.note.normalizedVelocity == 0.0;
                                if (silent) return true;
                            }
                            else if (source.type == engine::EventType::NoteOff && silent) { pending = false; return true; }
                            engine::MusicalTime time;
                            WireEvent event;
                            if (! musical (occurrence.timeBase, occurrence.nativePosition, time)
                                || engine::roundMusicalTime (time, engine::TimeRounding::Nearest, event.tick) != engine::TimeLookupStatus::Ok)
                            { callbackStatus = ProjectSmfStatus::NumericPrecision; return false; }
                            event.clipId = occurrence.clipId; event.sourceId = occurrence.sourceId; event.repeat = occurrence.repeatOrdinal;
                            if (source.type == engine::EventType::Midi1)
                            {
                                event.group = WireGroup::Controllers; event.status = source.payload.midi1.status;
                                event.data1 = source.payload.midi1.data1; event.data2 = source.payload.midi1.data2;
                            }
                            else
                            {
                                event.on = source.type == engine::EventType::NoteOn;
                                event.group = event.on ? WireGroup::OrdinaryOns : WireGroup::OrdinaryOffs;
                                event.status = static_cast<std::uint8_t> ((event.on ? 0x90u : 0x80u)
                                    | static_cast<unsigned> (std::max<std::int16_t> (0, source.voice.channel)));
                                event.data1 = static_cast<std::uint8_t> (source.voice.key);
                                event.data2 = event.on ? static_cast<std::uint8_t> (std::clamp (std::lround (source.payload.note.normalizedVelocity * 127.0), 1L, 127L)) : 64u;
                                if (event.on) onIndex = events.size();
                                else
                                {
                                    if (! pending || events[onIndex].clipId != event.clipId || events[onIndex].sourceId != event.sourceId
                                        || events[onIndex].repeat != event.repeat || event.tick < events[onIndex].tick)
                                    { callbackStatus = ProjectSmfStatus::NumericPrecision; return false; }
                                    if (event.tick == events[onIndex].tick) event.group = events[onIndex].group = WireGroup::ZeroLengthPairs;
                                    pending = false;
                                }
                            }
                            events.push_back (event);
                            return true;
                        });
                    if (status != engine::MidiFlattenStatus::Ok)
                        return failure (callbackStatus == ProjectSmfStatus::Ok ? ProjectSmfStatus::InvalidInput : callbackStatus);
                }
            }
            events.push_back (metadata (trackEnd, 0x2Fu));
        }
        std::size_t size = 14;
        for (auto& track : tracks)
        {
            if (const auto status = measureTrack (track, limits.maxFileBytes, resources); status != ProjectSmfStatus::Ok) return failure (status);
            if (! addSize (size, 8u) || ! addSize (size, track.bodyBytes)) return failure (ProjectSmfStatus::FileLimit);
            if (size > limits.maxFileBytes) return failure (ProjectSmfStatus::ResourceLimit);
        }
        ProjectSmfResult result;
        if (size > result.bytes.max_size()) return failure (ProjectSmfStatus::ResourceLimit);
        resources.visit (size);
        resources.container (size, 1u);
        result.bytes.reserve (size);
        result.bytes.insert (result.bytes.end(), { 'M', 'T', 'h', 'd' });
        putNumber (result.bytes, 6, 4); putNumber (result.bytes, 1, 2);
        putNumber (result.bytes, static_cast<std::uint32_t> (tracks.size()), 2);
        putNumber (result.bytes, engine::kTicksPerQuarter, 2);
        for (const auto& track : tracks) writeTrack (track, result.bytes);
        if (result.bytes.size() != size) return failure (ProjectSmfStatus::FileLimit);
        result.status = ProjectSmfStatus::Ok;
        result.exportedClips = clips.size();
        return result;
    }
    catch (const ResourceExceeded&) { return failure (ProjectSmfStatus::ResourceLimit); }
    catch (const std::bad_alloc&) { return failure (ProjectSmfStatus::ResourceLimit); }
    catch (const std::length_error&) { return failure (ProjectSmfStatus::ResourceLimit); }
}

}
