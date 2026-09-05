// YES DAW — MidiInputQueue (G3.10): the lock-free lane from a MIDI input device to the running engine.
//
// The law: the device's callback thread PUSHES (one producer), the live engine's audio thread POPS
// (one consumer) at the top of every block — no message-thread hop, no allocation, no lock (ADR-0002).
// The queue outlives every engine (the model owns it; an engine holds a pointer it was given at
// creation, so a rebuild swaps engines under the same queue). The "thru" target — which Instrument a
// played note reaches — is an atomic the control thread sets (the selected Track's instrument, 0 =
// nothing selected: the note is counted for the input lamp and dropped).
//
// Bounded: a full queue drops the newest event (push returns false; the caller never blocks).

#pragma once

#include "engine/Node.h"
#include "rt/RtHot.h"

#include "choc/containers/choc_SingleReaderSingleWriterFIFO.h"

#include <atomic>
#include <cstdint>

namespace yesdaw::engine {

class MidiInputQueue
{
public:
    static constexpr std::uint32_t kCapacity = 512;

    MidiInputQueue() { fifo_.reset (kCapacity); }

    // The device thread: a NoteOn / NoteOff (velocity 0 on a NoteOn is the device's NoteOff — the
    // caller maps it). Any other event type is refused, and so is a push into a full queue (false,
    // never a block). Every ACCEPTED note is counted for the input indicator whether or not a target
    // will take it.
    [[nodiscard]] bool push (const Event& event) noexcept
    {
        if (event.type != EventType::NoteOn && event.type != EventType::NoteOff)
            return false;
        if (! fifo_.push (event))
            return false;
        seen_.fetch_add (1u, std::memory_order_relaxed);
        return true;
    }

    // The audio thread: the next event, its target stamped with the thru Instrument (an event that
    // already names a target keeps it). False when the queue is empty. An event with no target to go
    // to is skipped (popped and dropped) so a stale note never waits for the next selection.
    [[nodiscard]] bool pop (Event& out) noexcept YESDAW_RT_HOT
    {
        while (fifo_.pop (out))
        {
            if (out.payload.note.targetNode == 0)
                out.payload.note.targetNode = thruTarget_.load (std::memory_order_acquire);
            if (out.payload.note.targetNode != 0)
                return true;
            dropped_.fetch_add (1u, std::memory_order_relaxed);
        }
        return false;
    }

    // The control thread: where played notes go (0 = nowhere).
    void setThruTarget (NodeId target) noexcept { thruTarget_.store (target, std::memory_order_release); }
    [[nodiscard]] NodeId thruTarget() const noexcept { return thruTarget_.load (std::memory_order_acquire); }

    // Counters for the input indicator and the gates (relaxed; never on the audio path's hot loop).
    [[nodiscard]] std::uint32_t seen() const noexcept { return seen_.load (std::memory_order_relaxed); }
    [[nodiscard]] std::uint32_t dropped() const noexcept { return dropped_.load (std::memory_order_relaxed); }

private:
    choc::fifo::SingleReaderSingleWriterFIFO<Event> fifo_;
    std::atomic<NodeId> thruTarget_ { 0 };
    std::atomic<std::uint32_t> seen_ { 0 };
    std::atomic<std::uint32_t> dropped_ { 0 };
};

} // namespace yesdaw::engine
