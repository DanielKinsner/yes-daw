// YES DAW — the control->audio command (ADR-0006).
//
// One ordered command travels control->audio through a single lock-free SPSC queue. ADOPT #2 / ADR-0006:
// SwapGraph rides the SAME queue as the scalar ops, so a SetGain authored against the new topology can
// never be applied to the old graph and silently dropped (the bug class two separate channels create).
//
// It is a FLAT trivially-copyable POD — deliberately NOT std::variant — so it copies bit-for-bit through
// the FIFO on every compiler (MSVC/Clang/gcc) with no constructor or discriminator surprises.
//
// In THIS chunk only SwapGraph carries meaning. SetGain/SetPan are the SEAM: the audio thread drains
// them in order (proven by the ordering test) but no node consumes them yet. Giving them a real consumer
// later is a new switch arm, not a re-plumb.

#pragma once

#include "engine/CompiledGraph.h"

#include <cstdint>
#include <type_traits>

namespace yesdaw::engine {

enum class CommandType : std::uint32_t { SwapGraph = 0, SetGain = 1, SetPan = 2 };

struct Command
{
    CommandType          type  = CommandType::SwapGraph;
    const CompiledGraph* graph = nullptr;   // SwapGraph: the next graph (ownership transfers to the engine)
    NodeId               node  = 0;         // SetGain/SetPan: target node  (seam; unused this chunk)
    float                value = 0.0f;      // SetGain: linear gain / SetPan: -1..+1 (seam; unused)
};

static_assert (std::is_trivially_copyable_v<Command>,
               "Command must be trivially copyable to pass losslessly through the lock-free SPSC FIFO");

} // namespace yesdaw::engine
