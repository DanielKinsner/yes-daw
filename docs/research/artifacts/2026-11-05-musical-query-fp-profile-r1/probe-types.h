#pragma once

#include "engine/MusicalAudioWindow.h"
#include <cstdint>
#include <memory>

#if defined(_MSC_VER)
 #define YESDAW_PROBE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
 #define YESDAW_PROBE_NOINLINE __attribute__((noinline))
#else
 #define YESDAW_PROBE_NOINLINE
#endif

namespace yesdaw::query_fp_probe {
namespace e = yesdaw::engine;
using U64 = std::uint64_t;
using ClockOwner = std::shared_ptr<const e::MusicalAudioClock>;

struct NativeSnapshot final
{
    U64 control0 {}, control1 {}, status0 {}, status1 {};
    int rounding {}, exceptions {}, errorNumber {};
    bool supported {};
};

YESDAW_PROBE_NOINLINE NativeSnapshot snapshot() noexcept;
YESDAW_PROBE_NOINLINE e::MusicalAudioStatus buildClock (
    const e::TempoChange* rows, std::size_t count, e::SampleRate rate, ClockOwner& out);
YESDAW_PROBE_NOINLINE e::MusicalAudioStatus frameAt (
    const ClockOwner& clock, e::Tick tick, e::MusicalAudioFramePosition& out) noexcept;
}
