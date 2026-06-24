// YES DAW — Runtime: the real-time boundary between the control thread and the one audio thread.
// ADR-0006 (immutable compiled-snapshot concurrency). Pure C++ — no JUCE — so the RTSan/TSan legs cover
// the whole publish/process/reclaim path.
//
// Model (the winning design of the H1 concurrency panel, with the panel's must-fix grafts):
//   * The control thread builds an immutable CompiledGraph and publish()es it. The swap travels as a
//     variant in ONE ordered SPSC command queue (so future scalar ops stay ordered with it — ADR-0006).
//   * The audio thread owns `current_` (a plain, audio-thread-local pointer). Each block it drains the
//     command queue IN ORDER, applies any swap, renders, then release-stores an end-of-block generation.
//   * Retiring a graph hands {old, gen} to an audio->control retirement queue. A generation-counter
//     janitor (reclaim(), called off the audio thread) frees `old` only once `processedGen > retiredAtGen`
//     — i.e. the audio thread has completed >=1 whole block on the new graph and can never touch the old.
//
// Threading contract — DO NOT VIOLATE:
//   * processBlock()      : AUDIO THREAD ONLY. Real-time safe (YESDAW_RT_HOT): no alloc/lock/syscall/free.
//   * publish()/postSet*(): CONTROL THREAD ONLY. May allocate. Bounded; return false on a full queue so
//                           the CONTROL side backpressures — never the audio thread.
//   * reclaim()           : the JANITOR — exactly ONE non-audio thread (the control/test thread, or a
//                           dedicated janitor thread). Sole consumer of the retirement queue.
//   * ~Runtime()          : the audio thread MUST already be stopped. Frees everything still owned.
//
// Why no atomic<const CompiledGraph*> on the hot path (cf. ADR-0006's wording): the swap is applied on
// the audio thread from the ordered queue, so `current_` is audio-thread-local and needs no atomic.
// choc's pop provides the acquire that pairs with publish()'s release on push, so the new graph's
// contents are fully visible the moment the audio thread installs it. An observer channel for meters/UI
// (which would need a pin/hazard protocol to be UAF-safe) is intentionally deferred to a later chunk.

#pragma once

#include "engine/CompiledGraph.h"
#include "engine/Command.h"
#include "rt/RtHot.h"

#include "choc/containers/choc_SingleReaderSingleWriterFIFO.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace yesdaw::engine {

// A retired graph awaiting reclamation, tagged with the generation at which the audio thread stopped
// using it. Trivially copyable so it passes through the lock-free retirement FIFO bit-for-bit.
struct Retired
{
    const CompiledGraph* graph       = nullptr;
    std::uint64_t        retiredAtGen = 0;
};
static_assert (std::is_trivially_copyable_v<Retired>, "Retired must pass losslessly through the FIFO");

class Runtime
{
public:
    struct Config
    {
        std::uint32_t commandCapacity    = 256;   // control->audio queue depth
        std::uint32_t retireCapacity     = 64;    // audio->control retirement queue depth
        int           maxCommandsPerBlock = 64;   // bound the per-block drain so a block stays O(1)-ish
    };

    explicit Runtime (Config cfg = {})
        : cfg_ (cfg)
    {
        cmdFifo_.reset (cfg.commandCapacity);
        retireFifo_.reset (cfg.retireCapacity);
        pending_.reserve (cfg.retireCapacity);
    }

    ~Runtime()
    {
        // Precondition: the audio thread is stopped (no concurrent processBlock). With it gone, every
        // retired graph is trivially safe to free regardless of generation. A graph lives in exactly one
        // place at a time — the command queue (published, not yet installed), `current_` (installed), or
        // the retirement queue / `pending_` (retired) — so each bucket is freed exactly once.
        Retired r;
        while (retireFifo_.pop (r))
            pending_.push_back (r);

        for (const Retired& p : pending_)
            delete p.graph;

        pending_.clear();

        delete current_;
        current_ = nullptr;

        Command c;
        while (cmdFifo_.pop (c))
            if (c.type == CommandType::SwapGraph)
                delete c.graph;
    }

    Runtime (const Runtime&)            = delete;
    Runtime& operator= (const Runtime&) = delete;

    // ---- CONTROL THREAD ------------------------------------------------------------------------------
    // Hand a freshly-built graph to the engine. On success, ownership transfers (the audio thread retires
    // it; the janitor frees it). On a full command queue, returns false and the graph is freed here (the
    // unique_ptr destructs) — nothing entered the engine, the control side backpressures.
    [[nodiscard]] bool publish (std::unique_ptr<CompiledGraph> next) noexcept
    {
        const Command c { CommandType::SwapGraph, next.get(), 0, 0.0f };

        if (cmdFifo_.push (c))
        {
            (void) next.release();   // ownership now lives in the queue -> audio thread -> janitor
            return true;
        }

        return false;                // queue full: `next` destructs on return; nothing leaked
    }

    [[nodiscard]] bool postSetGain (NodeId node, float linearGain) noexcept
    {
        return cmdFifo_.push (Command { CommandType::SetGain, nullptr, node, linearGain });
    }

    [[nodiscard]] bool postSetPan (NodeId node, float pan) noexcept
    {
        return cmdFifo_.push (Command { CommandType::SetPan, nullptr, node, pan });
    }

    // ---- AUDIO THREAD --------------------------------------------------------------------------------
    void processBlock (float* out, int numFrames) noexcept YESDAW_RT_HOT
    {
        // (1) Drain the command queue IN ORDER.
        //     INVARIANT: this drain MUST run before the end-of-block release-store at (3), inside this
        //     same call. The reclamation fence-post relies on the generation captured at swap time being
        //     the PREVIOUS block's published value. Do not hoist the drain out of processBlock, and do
        //     not move the (3) store before it.
        //     BACKPRESSURE: only apply a swap when the retirement queue has room for the outgoing graph,
        //     so the audio thread never drops (leaks) a retirement. A full retirement queue stops the
        //     drain, leaving the swap in the command queue for next block, which backpressures the
        //     control thread (publish() -> false). We gate on getUsedSlots() (NOT getFreeSlots(), which
        //     choc over-reports by one — it counts the ring's sentinel slot). Used can only be
        //     OVER-reported here (the janitor's pops may not be visible yet), which is conservative-safe.
        Command c;
        for (int i = 0; i < cfg_.maxCommandsPerBlock; ++i)
        {
            if (retireFifo_.getUsedSlots() >= cfg_.retireCapacity)   // retire queue full -> defer to next block
                break;

            if (! cmdFifo_.pop (c))                // choc pop provides the acquire paired with push's release
                break;

            applyCommand (c);
        }

        // (2) Render. Empty graph (nothing published yet) -> silence (ADR-0007 first-callback safety).
        if (current_ == nullptr)
        {
            for (int i = 0; i < numFrames; ++i)
                out[i] = 0.0f;
        }
        else
        {
            current_->process (out, numFrames);
        }

        // (3) Publish the end-of-block generation LAST (release). Pairs with reclaim()'s acquire-load.
        //     INVARIANT: this MUST be the last statement in processBlock (see (1)).
        processedGen_.fetch_add (1, std::memory_order_release);
    }

    // ---- JANITOR (one non-audio thread) --------------------------------------------------------------
    // Free every retired graph the audio thread has provably finished with. Returns how many were freed.
    // May allocate (pending_) — that is fine, it is never the audio thread.
    std::size_t reclaim() noexcept
    {
        Retired r;
        while (retireFifo_.pop (r))
            pending_.push_back (r);

        const std::uint64_t gen = processedGen_.load (std::memory_order_acquire);

        std::size_t freed = 0;
        std::size_t keep  = 0;
        for (std::size_t i = 0; i < pending_.size(); ++i)
        {
            if (gen > pending_[i].retiredAtGen)   // strict-greater: ADR-0006 verbatim, the airtight fence-post
            {
                delete pending_[i].graph;
                ++freed;
            }
            else
            {
                pending_[keep++] = pending_[i];   // compact survivors to the front
            }
        }
        pending_.resize (keep);
        return freed;
    }

    // ---- Diagnostics (control thread) ----------------------------------------------------------------
    std::uint64_t processedGen()   const noexcept { return processedGen_.load (std::memory_order_acquire); }
    std::uint64_t scalarsApplied() const noexcept { return scalarsApplied_.load (std::memory_order_acquire); }
    std::size_t   pendingCount()   const noexcept { return pending_.size(); }   // janitor-thread view

private:
    void applyCommand (const Command& c) noexcept YESDAW_RT_HOT
    {
        switch (c.type)
        {
            case CommandType::SwapGraph:
                if (current_ != nullptr)
                {
                    const bool retired = retireFifo_.push (
                        Retired { current_, processedGen_.load (std::memory_order_relaxed) });
                    YESDAW_RT_ASSERT (retired);   // guaranteed by the getFreeSlots() gate in processBlock
                    (void) retired;
                }
                current_ = c.graph;
                break;

            case CommandType::SetGain:
            case CommandType::SetPan:
                // Seam only (ADR-0006): drained in order with swaps, observable by the ordering test, no
                // consumer yet. Adding one is a new arm here, not a re-plumb of the queue.
                scalarsApplied_.fetch_add (1, std::memory_order_relaxed);
                break;
        }
    }

    static_assert (std::atomic<std::uint64_t>::is_always_lock_free,
                   "processedGen must be lock-free, else the audio thread could block (priority inversion)");

    Config cfg_;
    choc::fifo::SingleReaderSingleWriterFIFO<Command> cmdFifo_;
    choc::fifo::SingleReaderSingleWriterFIFO<Retired> retireFifo_;
    const CompiledGraph*       current_ = nullptr;        // AUDIO-THREAD-LOCAL — never touched elsewhere
    std::atomic<std::uint64_t> processedGen_   { 0 };
    std::atomic<std::uint64_t> scalarsApplied_ { 0 };
    std::vector<Retired>       pending_;                  // JANITOR-THREAD-LOCAL
};

} // namespace yesdaw::engine
