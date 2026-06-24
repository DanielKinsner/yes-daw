// YES DAW — CompiledGraph (H1 stub).
//
// The immutable, read-only thing the audio thread runs (ADR-0006 publishes it via atomic swap; ADR-0007
// is the real 5-pass compiler that fills it with nodes/edges/PDC delays later). This chunk is the
// NARROWEST stub that lets the swap + reclamation machinery be built and tested: it is immutable after
// construction and carries ONE observable identity value so a test can mechanically prove the audio
// thread switched from graph A to graph B by reading the output buffer.
//
// Pure C++ — no JUCE — so it builds on the RTSan/TSan legs (YESDAW_BUILD_APPS=OFF) where the real-time
// guarantees are actually verified.

#pragma once

#include "rt/RtHot.h"

#include <atomic>
#include <cstdint>

namespace yesdaw::engine {

using NodeId  = std::uint32_t;
using GraphId = std::uint64_t;

class CompiledGraph
{
public:
    CompiledGraph (GraphId id, float identityDc) noexcept
        : id_ (id), identityDc_ (identityDc)
    {
        alive_.fetch_add (1, std::memory_order_relaxed);
    }

    ~CompiledGraph()
    {
        // Poison the canary on the way out: if a use-after-free ever lets the audio thread call
        // process() on this freed object before the memory is reused, the canary check below traps.
        canary_ = kPoison;
        alive_.fetch_sub (1, std::memory_order_relaxed);
    }

    CompiledGraph (const CompiledGraph&)            = delete;
    CompiledGraph& operator= (const CompiledGraph&) = delete;

    // The audio hot path. Immutable read; allocation/lock free; RTSan-covered. Fills `out` with this
    // graph's identity DC. (ADR-0007's real per-node processing replaces the body in a later chunk —
    // the real graph must keep SOME way for a test to tell two graphs apart in the rendered buffer.)
    void process (float* out, int numFrames) const noexcept YESDAW_RT_HOT
    {
        YESDAW_RT_FATAL (canary_ == kCanary);   // UAF tripwire — ALWAYS live (incl. RTSan/TSan/Release);
        for (int i = 0; i < numFrames; ++i)     // traps, never syscalls, so it stays RT-safe
            out[i] = identityDc_;
    }

    GraphId id()         const noexcept { return id_; }
    float   identityDc() const noexcept { return identityDc_; }

    // Liveness instrumentation (cheap, always compiled — also a legitimate diagnostic). Every
    // construction/destruction adjusts this, so a test can assert it returns to its baseline (no leak)
    // and that exactly one graph survives steady state (reclamation actually ran).
    static std::uint64_t aliveCount() noexcept { return alive_.load (std::memory_order_relaxed); }

private:
    static constexpr std::uint32_t kCanary = 0xC0DEFACEu;
    static constexpr std::uint32_t kPoison = 0xDEADBEEFu;

    GraphId       id_;
    float         identityDc_;
    std::uint32_t canary_ = kCanary;

    static inline std::atomic<std::uint64_t> alive_ { 0 };
};

} // namespace yesdaw::engine
