#include "probe-types.h"

namespace yesdaw::query_fp_probe {
YESDAW_PROBE_NOINLINE e::MusicalAudioStatus buildClock (
    const e::TempoChange* rows, std::size_t count, e::SampleRate rate, ClockOwner& out)
{
    return e::MusicalAudioClock::build ({ rows, count }, rate, {}, out);
}

YESDAW_PROBE_NOINLINE e::MusicalAudioStatus frameAt (
    const ClockOwner& clock, e::Tick tick, e::MusicalAudioFramePosition& out) noexcept
{
    return clock->frameAt ({ tick, 0, 1 }, out);
}
}
