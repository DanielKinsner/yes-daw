// YES DAW — the Node contract (ADR-0008): one CLAP-shaped, format-neutral processing unit.
//
// Built-in DSP and (at H3) hosted plugins implement this same trait, so adding plugin hosting later is
// an adapter, not an engine rewrite (ADR-0002 #3). The graph compiler (ADR-0007), PDC, the buffer pool,
// and the event router all program against this shape — changing it later touches every Node at once,
// so it is frozen here. process() is the audio hot path; prepare() is the ONLY place a Node allocates.
//
// Pure C++ — no JUCE — so every Node is covered by the RTSan/TSan legs. (PluginNode, the one adapter
// that wraps juce::AudioProcessor, lives behind a layering boundary — ADR-0008 — never in this header.)
//
// NOTE: this replaces the throwaway H0 spike trait that proved block-size independence; that property is
// a real contract rule and is re-asserted against the built-in Nodes (tests/node_tests.cpp).

#pragma once

#include "engine/Time.h"
#include "rt/RtHot.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace yesdaw::engine {

using NodeId = std::uint32_t;
using ParameterId = std::uint32_t;

// What a Node advertises to the compiler. latencySamples drives PDC; tailSamples extends offline render
// length without delaying sibling paths; channels/produces* drive routing.
// blockParallelSafe (ADR-0027) declares the node's process() output is independent of Block dispatch order
// under the scheduler's calling convention (absolute transport frame, empty per-Block events) — i.e. it
// carries NO cross-Block state. Default FALSE (fail-safe): a node must opt in, so a future stateful node
// (delay/reverb/automation/plugin) is refused by the parallel scheduler until proven, never silently
// mis-rendered. GraphBuilder ANDs this across the graph into CompiledGraph::isBlockParallelSafe().
struct NodeProperties
{
    bool         producesAudio     = false;
    bool         producesEvents    = false;
    int          channels          = 0;
    std::int64_t latencySamples    = 0;
    NodeId       id                = 0;
    bool         blockParallelSafe = false;
    std::int64_t tailSamples       = 0;
};

// A view over the per-channel audio buffers a Node reads/writes this Block. The frame count travels in
// ProcessArgs (variable Block, ADR-0010): each channels[c] points to at least ProcessArgs::numFrames floats.
struct AudioBlock
{
    float* const* channels    = nullptr;
    int           numChannels = 0;
};

enum class EventType : std::uint16_t
{
    ParameterChange = 0,
    NoteOn          = 1,
    NoteOff         = 2,
    NoteExpression  = 3,
    Midi1           = 4,
    Midi2           = 5,
    SysEx           = 6
};

// A CLAP-style voice address. -1 means wildcard, so one shape can carry channel-wide, key-wide,
// per-note, and future MPE/MIDI-2 events.
struct VoiceAddress
{
    std::int32_t noteId    = -1;
    std::int16_t portIndex = -1;
    std::int16_t channel   = -1;
    std::int16_t key       = -1;
};

struct ParameterChangePayload
{
    NodeId      targetNode      = 0;
    ParameterId parameterId     = 0;
    double      normalizedValue = 0.0;
};

struct NotePayload
{
    double normalizedVelocity = 0.0;
    double pitchNote          = 0.0;
    NodeId targetNode         = 0;   // G3.2 / ADR-0047: a LIVE note (audition, later G3.10 input) names the one
                                     // Instrument it is for; 0 = a scheduled note, routed by the graph's event edges.
};

struct SysExPayload
{
    std::uint32_t offset   = 0;
    std::uint32_t length   = 0;
    std::uint64_t reserved = 0;
};

// G3.3: a MIDI 1.0 channel message on the wire - control change, pitch bend, pressure, program
// change - carried as its three bytes so a built-in instrument and a hosted plugin read the SAME
// event (the format-neutral node contract). The status byte carries the message kind in its high
// nibble and the channel (0..15, 0 when the voice address is a wildcard) in its low nibble.
struct Midi1Payload
{
    std::uint8_t status = 0;
    std::uint8_t data1  = 0;
    std::uint8_t data2  = 0;
};

union EventPayload
{
    ParameterChangePayload parameter;
    NotePayload            note;
    SysExPayload           sysex;
    Midi1Payload           midi1;
    std::uint64_t          raw[2];
};

// ADR-0009: fixed-size, trivially-copyable, sample-accurate events sorted ascending by timeInBlock.
struct Event
{
    std::uint32_t timeInBlock = 0; // half-open Block offset: [0, ProcessArgs::numFrames)
    EventType     type        = EventType::ParameterChange;
    std::uint16_t flags       = 0;
    VoiceAddress  voice;
    EventPayload  payload     = {};
};

static_assert (std::is_trivially_copyable_v<VoiceAddress>, "VoiceAddress must stay flat");
static_assert (std::is_trivially_copyable_v<ParameterChangePayload>, "Parameter payload must stay flat");
static_assert (std::is_trivially_copyable_v<EventPayload>, "Event payload must stay flat");
static_assert (std::is_trivially_copyable_v<Event>, "Event must pass through fixed-size RT buffers");
static_assert (sizeof (Event) <= 64, "Event must stay cache-small and fixed-size");

[[nodiscard]] inline Event makeParameterChangeEvent (std::uint32_t timeInBlock,
                                                     NodeId targetNode,
                                                     ParameterId parameterId,
                                                     double normalizedValue) noexcept
{
    Event event {};
    event.timeInBlock                     = timeInBlock;
    event.type                            = EventType::ParameterChange;
    event.payload.parameter.targetNode      = targetNode;
    event.payload.parameter.parameterId     = parameterId;
    event.payload.parameter.normalizedValue = normalizedValue;
    return event;
}

[[nodiscard]] inline Event makeMidi1Event (std::uint32_t timeInBlock,
                                           std::uint8_t status,
                                           std::uint8_t data1,
                                           std::uint8_t data2,
                                           std::int16_t portIndex = -1,
                                           std::int16_t channel = -1) noexcept
{
    Event event {};
    event.timeInBlock          = timeInBlock;
    event.type                 = EventType::Midi1;
    event.voice.portIndex      = portIndex;
    event.voice.channel        = channel;
    event.payload.midi1.status = status;
    event.payload.midi1.data1  = data1;
    event.payload.midi1.data2  = data2;
    return event;
}

// The MIDI 1.0 status nibbles and controller numbers the engine speaks (G3.3).
inline constexpr std::uint8_t kMidiStatusPolyPressure    = 0xA0;
inline constexpr std::uint8_t kMidiStatusControlChange   = 0xB0;
inline constexpr std::uint8_t kMidiStatusProgramChange   = 0xC0;
inline constexpr std::uint8_t kMidiStatusChannelPressure = 0xD0;
inline constexpr std::uint8_t kMidiStatusPitchBend       = 0xE0;
inline constexpr std::uint8_t kMidiControllerModWheel    = 1;
inline constexpr std::uint8_t kMidiControllerSustain     = 64;

// A non-owning per-Block view. The producer owns/sorts storage; Nodes read this view on the audio thread.
class EventStream
{
public:
    EventStream() noexcept = default;

    explicit EventStream (std::span<const Event> events,
                          std::span<const std::byte> sysexBytes = {}) noexcept
        : events_ (events), sysexBytes_ (sysexBytes)
    {
    }

    explicit EventStream (std::span<Event> events,
                          std::span<const std::byte> sysexBytes = {}) noexcept
        : events_ (events), mutableEvents_ (events), sysexBytes_ (sysexBytes)
    {
    }

    EventStream (std::span<Event> eventStorage,
                 std::size_t eventCount,
                 std::span<const std::byte> sysexBytes = {}) noexcept
        : events_ (eventStorage.data(), eventCount <= eventStorage.size() ? eventCount : eventStorage.size()),
          mutableEvents_ (eventStorage),
          sysexBytes_ (sysexBytes)
    {
    }

    [[nodiscard]] std::span<const Event> events() const noexcept YESDAW_RT_HOT { return events_; }
    [[nodiscard]] std::span<Event> writableEvents() noexcept YESDAW_RT_HOT
    {
        return mutableEvents_.empty() ? std::span<Event> {} : mutableEvents_.first (events_.size());
    }
    [[nodiscard]] const Event* begin() const noexcept YESDAW_RT_HOT { return events_.data(); }
    [[nodiscard]] const Event* end() const noexcept YESDAW_RT_HOT { return events_.data() + events_.size(); }
    [[nodiscard]] bool empty() const noexcept YESDAW_RT_HOT { return events_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept YESDAW_RT_HOT { return events_.size(); }
    [[nodiscard]] std::size_t writableCapacity() const noexcept YESDAW_RT_HOT { return mutableEvents_.size(); }
    [[nodiscard]] bool isWritable() const noexcept YESDAW_RT_HOT { return mutableEvents_.data() != nullptr; }
    [[nodiscard]] std::span<const std::byte> sysexBytes() const noexcept YESDAW_RT_HOT { return sysexBytes_; }

    [[nodiscard]] bool replaceEvents (std::span<const Event> events) noexcept YESDAW_RT_HOT
    {
        if (events.size() > mutableEvents_.size())
            return false;

        for (std::size_t i = 0; i < events.size(); ++i)
            mutableEvents_[i] = events[i];

        events_ = std::span<const Event> (mutableEvents_.data(), events.size());
        return true;
    }

    // Control/test-side validator for ADR-0009's sorted, half-open [0, numFrames) block contract.
    [[nodiscard]] bool isValidForBlock (std::uint32_t numFrames) const noexcept
    {
        std::uint32_t previous = 0;
        bool          havePrevious = false;

        for (const Event& event : events_)
        {
            if (event.timeInBlock >= numFrames)
                return false;

            if (havePrevious && event.timeInBlock < previous)
                return false;

            if (! payloadIsValid (event))
                return false;

            previous     = event.timeInBlock;
            havePrevious = true;
        }

        return true;
    }

private:
    [[nodiscard]] bool payloadIsValid (const Event& event) const noexcept
    {
        switch (event.type)
        {
            case EventType::ParameterChange:
            {
                const double v = event.payload.parameter.normalizedValue;
                return std::isfinite (v) && v >= 0.0 && v <= 1.0;
            }

            case EventType::NoteOn:
            case EventType::NoteOff:
            case EventType::NoteExpression:
            {
                const double velocity = event.payload.note.normalizedVelocity;
                return std::isfinite (velocity) && velocity >= 0.0 && velocity <= 1.0
                       && std::isfinite (event.payload.note.pitchNote);
            }

            case EventType::SysEx:
            {
                const std::size_t offset = event.payload.sysex.offset;
                const std::size_t length = event.payload.sysex.length;
                return offset <= sysexBytes_.size() && length <= sysexBytes_.size() - offset;
            }

            case EventType::Midi1:
            case EventType::Midi2:
                return true;
        }

        return false;
    }

    std::span<const Event>     events_;
    std::span<Event>           mutableEvents_;
    std::span<const std::byte> sysexBytes_;
};

struct ProcessArgs
{
    AudioBlock       audio;
    EventStream&     events;
    const Transport& transport;
    int              numFrames = 0;   // <= the maxBlockSize passed to prepare()
    const EventStream* automationEvents = nullptr; // H15 side-band; null until compiled lanes exist.
    const EventStream* liveEvents = nullptr;       // G3.2 side-band: this Block's LIVE notes, handed to EVERY
                                                   // node; an Instrument takes the ones addressed to it
                                                   // (NotePayload::targetNode). Block-top (timeInBlock 0).
};

// The trait every processing unit implements.
class Node
{
public:
    virtual ~Node() = default;

    // Advertised properties (the compiler reads these every recompile). Cheap, RT-safe.
    virtual NodeProperties properties() const noexcept = 0;

    // The Nodes feeding this one — the graph compiler walks these for topo order + PDC. A source/leaf
    // returns an empty span. Edges are wired by the graph builder (ADR-0007).
    virtual std::span<Node* const> directInputs() const noexcept = 0;

    // Allocate + size everything for a sample rate and maximum Block. The ONLY place a Node may allocate.
    // Not RT-safe; called from the control thread before the Node goes live.
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;

    // The audio hot path: read/transform/write args.audio for args.numFrames frames. RT-safe — no
    // allocation, lock, or syscall (enforced by RTSan). numFrames <= maxBlockSize.
    virtual void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT = 0;

    // Drop transient state (envelopes, delay lines) to zero without reallocating. Intended to run on the
    // audio thread between process() Blocks, so it must be RT-safe (no alloc/lock/syscall) — the built-ins
    // honour this by construction. NB: unlike process(), reset() is NOT marked YESDAW_RT_HOT, so RTSan
    // does not yet *mechanically* enforce it; marking the overrides + an RTSan-exercised reset test is a
    // tracked follow-up from the H1 coverage review.
    virtual void reset() noexcept = 0;

    // Free what prepare() allocated. Not RT-safe; control thread.
    virtual void release() = 0;
};

} // namespace yesdaw::engine
