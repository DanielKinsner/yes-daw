// YES DAW - Project data-model value surface (ADR-0011).
//
// Storage-facing, JUCE-free value types for the non-destructive Asset -> Clip -> Project indirection.
// Persistence code will serialize these later; this header only locks the ID shape and semantic
// invariants that schema v1 will depend on.

#pragma once

#include "engine/Automation.h"
#include "engine/MixerValue.h"
#include "engine/ParamSpec.h"
#include "engine/Time.h"
#include "engine/nodes/CompressorNode.h"
#include "engine/nodes/EqNode.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/FxDelayNode.h"
#include "engine/nodes/LimiterNode.h"
#include "engine/nodes/PanNode.h"
#include "engine/nodes/ReverbNode.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace yesdaw::engine {

struct EntityId
{
    static constexpr std::size_t kNumBytes = 16;
    using StorageBytes = std::array<std::uint8_t, kNumBytes>;

    StorageBytes bytes {};

    static constexpr EntityId fromBytes (StorageBytes value) noexcept
    {
        return EntityId { value };
    }

    static constexpr EntityId fromBigEndianParts (std::uint64_t high, std::uint64_t low) noexcept
    {
        StorageBytes out {};

        for (std::size_t i = 0; i < 8; ++i)
            out[i] = static_cast<std::uint8_t> ((high >> ((7u - i) * 8u)) & 0xFFu);

        for (std::size_t i = 0; i < 8; ++i)
            out[8u + i] = static_cast<std::uint8_t> ((low >> ((7u - i) * 8u)) & 0xFFu);

        return EntityId { out };
    }

    [[nodiscard]] constexpr bool isZero() const noexcept
    {
        for (const std::uint8_t b : bytes)
            if (b != 0u)
                return false;

        return true;
    }

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return ! isZero();
    }

    [[nodiscard]] constexpr std::uint64_t ulidTimestampMs() const noexcept
    {
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < 6; ++i)
            value = (value << 8u) | static_cast<std::uint64_t> (bytes[i]);

        return value;
    }

    friend constexpr bool operator== (const EntityId&, const EntityId&) noexcept = default;

    friend constexpr bool operator< (const EntityId& a, const EntityId& b) noexcept
    {
        for (std::size_t i = 0; i < kNumBytes; ++i)
        {
            if (a.bytes[i] < b.bytes[i])
                return true;
            if (a.bytes[i] > b.bytes[i])
                return false;
        }

        return false;
    }
};

static_assert (sizeof (EntityId) == EntityId::kNumBytes, "EntityId must stay a fixed 16-byte value");

using UlidEntropy = std::array<std::uint8_t, 10>;
constexpr std::uint64_t kMaxUlidTimestampMs = (1ull << 48u) - 1ull;

class EntityIdAllocator final
{
public:
    explicit constexpr EntityIdAllocator (UlidEntropy seed = {}) noexcept : entropy_ (seed) {}

    [[nodiscard]] EntityId allocate (std::uint64_t unixTimeMs) noexcept
    {
        if (unixTimeMs > kMaxUlidTimestampMs)
            return {};

        std::uint64_t timestamp = unixTimeMs;

        if (! haveLastTimestamp_)
        {
            haveLastTimestamp_ = true;
            lastTimestampMs_ = timestamp;
        }
        else if (timestamp <= lastTimestampMs_)
        {
            timestamp = lastTimestampMs_;
            if (! incrementEntropy())
                return {};
        }
        else
        {
            lastTimestampMs_ = timestamp;
        }

        EntityId id = makeUlid (timestamp, entropy_);
        if (! id.isValid())
        {
            if (! incrementEntropy())
                return {};

            id = makeUlid (timestamp, entropy_);
        }

        return id;
    }

private:
    static constexpr EntityId makeUlid (std::uint64_t timestampMs, const UlidEntropy& entropy) noexcept
    {
        EntityId::StorageBytes out {};

        for (std::size_t i = 0; i < 6; ++i)
            out[i] = static_cast<std::uint8_t> ((timestampMs >> ((5u - i) * 8u)) & 0xFFu);

        for (std::size_t i = 0; i < entropy.size(); ++i)
            out[6u + i] = entropy[i];

        return EntityId { out };
    }

    bool incrementEntropy() noexcept
    {
        std::size_t index = entropy_.size();
        while (index > 0 && entropy_[index - 1u] == 0xFFu)
            --index;

        if (index == 0)
            return false;

        ++entropy_[index - 1u];
        for (std::size_t i = index; i < entropy_.size(); ++i)
        {
            entropy_[i] = 0u;
        }

        return true;
    }

    bool          haveLastTimestamp_ = false;
    std::uint64_t lastTimestampMs_ = 0;
    UlidEntropy   entropy_ {};
};

struct AssetContentHash
{
    static constexpr std::size_t kNumBytes = 32;
    using StorageBytes = std::array<std::uint8_t, kNumBytes>;

    StorageBytes bytes {};

    friend constexpr bool operator== (const AssetContentHash&, const AssetContentHash&) noexcept = default;
};

struct Asset
{
    EntityId   id;
    AssetContentHash contentHash;
    std::uint64_t frames = 0;
    SampleRate    sampleRate;
    std::uint16_t channels = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return id.isValid() && frames > 0 && sampleRate.isValid() && channels > 0;
    }

    friend constexpr bool operator== (const Asset&, const Asset&) noexcept = default;
};

enum class FxKind : std::uint8_t
{
    Eq = 0,
    Compressor,
    Delay,
    Reverb,
    Limiter
};

[[nodiscard]] constexpr bool fxKindIsKnown (FxKind kind) noexcept
{
    return kind == FxKind::Eq
           || kind == FxKind::Compressor
           || kind == FxKind::Delay
           || kind == FxKind::Reverb
           || kind == FxKind::Limiter;
}

[[nodiscard]] inline ParamSpec fxParamSpecForKind (FxKind kind, std::uint32_t paramId) noexcept
{
    switch (kind)
    {
        case FxKind::Eq:
            return EqNode::parameterSpec (paramId);
        case FxKind::Compressor:
            return CompressorNode::parameterSpec (paramId);
        case FxKind::Delay:
            return FxDelayNode::parameterSpec (paramId);
        case FxKind::Reverb:
            return ReverbNode::parameterSpec (paramId);
        case FxKind::Limiter:
            return LimiterNode::parameterSpec (paramId);
    }

    return {};
}

[[nodiscard]] inline bool fxKindAcceptsParameterId (FxKind kind, std::uint32_t paramId) noexcept
{
    if (! fxKindIsKnown (kind))
        return false;

    const ParamSpec spec = fxParamSpecForKind (kind, paramId);
    return spec.id == paramId
           && spec.name != nullptr
           && spec.name[0] != '\0'
           && paramSpecHasUsableRange (spec);
}

[[nodiscard]] inline bool normalizedFxParamValueIsValid (double value) noexcept
{
    return std::isfinite (value) && value >= 0.0 && value <= 1.0;
}

struct FxInsert
{
    EntityId id;
    FxKind kind = FxKind::Eq;
    bool enabled = true;
    std::vector<std::pair<std::uint32_t, double>> normalizedParams;

    [[nodiscard]] bool isValid() const noexcept
    {
        if (! id.isValid() || ! fxKindIsKnown (kind))
            return false;

        for (std::size_t i = 0; i < normalizedParams.size(); ++i)
        {
            if (! normalizedFxParamValueIsValid (normalizedParams[i].second))
                return false;

            for (std::size_t j = 0; j < i; ++j)
                if (normalizedParams[i].first == normalizedParams[j].first)
                    return false;
        }

        return true;
    }

    friend bool operator== (const FxInsert&, const FxInsert&) = default;
};

struct MixerStripState
{
    std::string name;
    float linearGain = 1.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;
    bool soloSafe = false;
    std::vector<FxInsert> fxChain;

    [[nodiscard]] bool isValid() const noexcept
    {
        if (! mixerGainIsValid (linearGain) || ! mixerPanIsValid (pan))
            return false;

        for (const FxInsert& insert : fxChain)
            if (! insert.isValid())
                return false;

        return true;
    }

    friend bool operator== (const MixerStripState&, const MixerStripState&) = default;
};

// ADR-0044: persisted send routing. Sends are rows on the owning Track; the mixer projection's
// runtime routes derive from these (tap maps 1:1 onto MixerSendTap at the projection boundary).
enum class SendTap : std::uint8_t
{
    PreFader = 0,
    PostFader
};

[[nodiscard]] constexpr bool sendTapIsKnown (SendTap tap) noexcept
{
    return tap == SendTap::PreFader || tap == SendTap::PostFader;
}

struct SendRow
{
    EntityId id;
    EntityId busId;
    SendTap tap = SendTap::PostFader;
    float linearGain = 1.0f;

    [[nodiscard]] bool isValid() const noexcept
    {
        return id.isValid() && busId.isValid() && sendTapIsKnown (tap) && mixerGainIsValid (linearGain);
    }

    friend bool operator== (const SendRow&, const SendRow&) = default;
};

// N6: the legible band a persisted track height must fall in. 0 is reserved (not in-band) to
// mean "no override — this row shares equally in its panel's available space", so a Project with
// no customized track ever diverges from the historical stretch-to-fill layout.
inline constexpr int kTrackHeightMinPx = 56;
inline constexpr int kTrackHeightMaxPx = 400;

[[nodiscard]] constexpr bool trackHeightIsValid (int heightPx) noexcept
{
    return heightPx == 0 || (heightPx >= kTrackHeightMinPx && heightPx <= kTrackHeightMaxPx);
}

// N7: persisted per-track colour, applied to the rail row, the mixer strip nameplate, and the
// clips on that track. Packed 0xAARRGGBB. 0 (the default, fully transparent) is reserved to mean
// "no override — every surface keeps painting its historical, colour-less appearance", so a
// Project with no customized track colour never diverges from today's rendering. A SET colour
// must be fully opaque (alpha byte 0xFF) so it can never collide with the unset sentinel and can
// never paint semi-transparent by accident.
inline constexpr std::uint32_t kTrackColourUnset = 0;

[[nodiscard]] constexpr bool trackColourIsValid (std::uint32_t colour) noexcept
{
    return colour == kTrackColourUnset || (colour >> 24) == 0xFFu;
}

struct Track
{
    EntityId id;
    MixerStripState strip;
    std::vector<SendRow> sends;   // ordered; ordinal = index (ADR-0039 SendLevel paramId)
    // M3: where this Track's MAIN output lands. Invalid (the default) = straight to master, the
    // historical law; a valid id must name a Bus, and the Track's post-pan signal feeds THAT Bus's
    // sum instead — the submix/group workflow sends alone cannot express.
    EntityId outputBusId;
    // N6: persisted row height, honoured by the rail, the timeline lanes, the clip geometry, and
    // the automation lane. 0 (the default) means no override — the row keeps sharing equally in
    // its panel's available space exactly like every Track did before this field existed.
    int heightPx = 0;
    // N7: persisted row colour, honoured by the rail, the mixer strip nameplate, and this
    // Track's clips. kTrackColourUnset (the default) means no override.
    std::uint32_t colour = kTrackColourUnset;

    [[nodiscard]] bool isValid() const noexcept
    {
        if (! id.isValid() || ! strip.isValid())
            return false;

        if (! trackHeightIsValid (heightPx))
            return false;

        if (! trackColourIsValid (colour))
            return false;

        for (const SendRow& send : sends)
            if (! send.isValid())
                return false;

        return true;
    }

    friend bool operator== (const Track&, const Track&) = default;
};

struct Bus
{
    EntityId id;
    MixerStripState strip;

    [[nodiscard]] bool isValid() const noexcept
    {
        return id.isValid() && strip.isValid();
    }

    friend bool operator== (const Bus&, const Bus&) = default;
};

inline constexpr EntityId kDefaultAudioTrackId = EntityId::fromBytes ({
    0x59u, 0x45u, 0x53u, 0x44u, 0x41u, 0x57u, 0x5Fu, 0x41u,
    0x55u, 0x44u, 0x49u, 0x4Fu, 0x5Fu, 0x30u, 0x30u, 0x31u,
});

struct ClipName
{
    static constexpr std::size_t kMaxLength = 127;

    constexpr ClipName() noexcept = default;
    constexpr ClipName (std::string_view text) noexcept : value {} { (void) assign (text); }

    [[nodiscard]] constexpr bool assign (std::string_view text) noexcept
    {
        if (text.empty() || text.size() > kMaxLength)
            return false;

        value = {};
        for (std::size_t i = 0; i < text.size(); ++i)
            value[i] = text[i];
        return true;
    }

    constexpr ClipName& operator= (std::string_view text) noexcept
    {
        (void) assign (text);
        return *this;
    }

    [[nodiscard]] constexpr std::string_view asView() const noexcept
    {
        std::size_t length = 0;
        while (length < value.size() && value[length] != '\0')
            ++length;
        return { value.data(), length };
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return asView().empty(); }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return asView().size(); }
    [[nodiscard]] constexpr const char* c_str() const noexcept { return value.data(); }
    [[nodiscard]] constexpr operator std::string_view() const noexcept { return asView(); }

    friend constexpr bool operator== (const ClipName&, const ClipName&) noexcept = default;
    friend constexpr bool operator== (const ClipName& name, std::string_view text) noexcept { return name.asView() == text; }
    friend constexpr bool operator== (std::string_view text, const ClipName& name) noexcept { return text == name.asView(); }

    std::array<char, kMaxLength + 1u> value { 'A', 'u', 'd', 'i', 'o', ' ', 'C', 'l', 'i', 'p' };
};

struct Clip
{
    EntityId id;
    EntityId assetId;
    EntityId trackId;
    Tick timelineStart = 0;
    Tick timelineLength = 0;
    std::uint64_t srcOffset = 0; // schema: clip.src_offset, measured in Asset frames
    std::uint64_t srcLen = 0;    // schema: clip.src_len, measured in Asset frames
    float gain = 1.0f;
    Tick fadeIn = 0;
    Tick fadeOut = 0;
    TimeBase timeBase = TimeBase::SampleLocked;
    ClipName name;

    [[nodiscard]] constexpr bool references (const Asset& asset) const noexcept
    {
        return assetId.isValid() && asset.id == assetId;
    }

    [[nodiscard]] constexpr bool sourceWindowFits (const Asset& asset) const noexcept
    {
        return references (asset) && srcOffset <= asset.frames && srcLen <= asset.frames - srcOffset;
    }

    friend constexpr bool operator== (const Clip&, const Clip&) noexcept = default;
};

struct Marker
{
    EntityId    id;
    Tick        tick = 0;
    std::string name;

    [[nodiscard]] bool isValid() const noexcept { return id.isValid(); }

    friend bool operator== (const Marker&, const Marker&) = default;
};

struct Note
{
    EntityId     id;
    Tick         startTick = 0;          // clip-relative
    Tick         lengthTicks = 0;        // zero-length is legal: On and Off at the same sample
    std::int16_t key = 60;
    double       pitchNote = 60.0;
    double       normalizedVelocity = 1.0;
    std::int16_t portIndex = -1;
    std::int16_t channel = -1;

    [[nodiscard]] bool isValid() const noexcept
    {
        return id.isValid()
            && startTick >= 0
            && lengthTicks >= 0
            && lengthTicks <= std::numeric_limits<Tick>::max() - startTick
            && key >= 0
            && key <= 127
            && std::isfinite (pitchNote)
            && std::isfinite (normalizedVelocity)
            && normalizedVelocity >= 0.0
            && normalizedVelocity <= 1.0
            && portIndex >= -1
            && channel >= -1
            && channel <= 15;
    }

    friend constexpr bool operator== (const Note&, const Note&) noexcept = default;
};

struct MidiClip
{
    EntityId          id;
    EntityId          trackId;
    Tick              timelineStart = 0;
    Tick              timelineLength = 0;
    TimeBase          timeBase = TimeBase::TempoLocked;
    std::vector<Note> notes;

    [[nodiscard]] bool isValid() const noexcept
    {
        if (! id.isValid()
            || ! trackId.isValid()
            || timelineLength < 0
            || (timeBase != TimeBase::TempoLocked && timeBase != TimeBase::SampleLocked))
            return false;

        for (const Note& note : notes)
        {
            if (! note.isValid() || note.startTick > timelineLength)
                return false;

            const Tick maxLength = timelineLength - note.startTick;
            if (note.lengthTicks > maxLength)
                return false;
        }

        return true;
    }

    friend bool operator== (const MidiClip&, const MidiClip&) = default;
};

enum class RecordingMonitoringPolicy : std::uint8_t
{
    Off = 0,
    DirectInput,
    LatencyCompensated
};

struct RecordingTake
{
    EntityId id;
    EntityId assetId;
    EntityId trackId;
    EntityId clipId;
    Tick timelineStart = 0;
    std::uint64_t frameCount = 0;
    std::uint32_t takeOrdinal = 0;
    std::uint16_t inputChannel = 0;
    std::uint32_t deviceStableId = 0;
    RecordingMonitoringPolicy monitoringPolicy = RecordingMonitoringPolicy::Off;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return id.isValid()
            && assetId.isValid()
            && trackId.isValid()
            && clipId.isValid()
            && timelineStart >= 0
            && frameCount > 0
            && (monitoringPolicy == RecordingMonitoringPolicy::Off
                || monitoringPolicy == RecordingMonitoringPolicy::DirectInput
                || monitoringPolicy == RecordingMonitoringPolicy::LatencyCompensated);
    }

    friend constexpr bool operator== (const RecordingTake&, const RecordingTake&) noexcept = default;
};

struct ProjectRecordingCompSegment
{
    EntityId id;
    EntityId takeId;
    Tick timelineStart = 0;
    Tick timelineLength = 0;
    std::uint64_t sourceOffset = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return id.isValid()
            && takeId.isValid()
            && timelineStart >= 0
            && timelineLength > 0;
    }

    friend constexpr bool operator== (const ProjectRecordingCompSegment&, const ProjectRecordingCompSegment&) noexcept = default;
};

enum class AutomationTargetRole : std::uint8_t
{
    TrackFader = 0,
    TrackPan,
    SendLevel,
    BusFader,
    BusPan,
    FxInsertParam
};

[[nodiscard]] constexpr bool automationTargetRoleIsKnown (AutomationTargetRole role) noexcept
{
    return role == AutomationTargetRole::TrackFader
           || role == AutomationTargetRole::TrackPan
           || role == AutomationTargetRole::SendLevel
           || role == AutomationTargetRole::BusFader
           || role == AutomationTargetRole::BusPan
           || role == AutomationTargetRole::FxInsertParam;
}

// N5: a per-project automation write mode. Read (the historical, only behaviour) plays back
// existing breakpoints and never writes; Touch/Latch write new breakpoints from a live control
// move while the transport rolls. Latch shares Touch's write-while-moving semantics today — full
// "keep writing after release until playback stops" behaviour is deferred (honest subset, logged
// in STATUS.md rather than faked).
enum class AutomationMode : std::uint8_t
{
    Read = 0,
    Touch = 1,
    Latch = 2
};

[[nodiscard]] constexpr bool automationModeIsKnown (AutomationMode mode) noexcept
{
    return mode == AutomationMode::Read
           || mode == AutomationMode::Touch
           || mode == AutomationMode::Latch;
}

// N8: a persisted punch region for recording. Disabled (the default) reproduces today's
// behaviour bit-identically: no punch bounds gate the capture window beyond count-in. Frames are
// raw sample frames at the project's sample rate (the same domain RecordingWindow already uses),
// NOT musical ticks — Tick is reused here as the codebase's existing plain int64 alias, exactly
// how a SampleLocked Clip's timelineStart already reuses it for raw frame positions.
struct PunchRegion
{
    bool enabled = false;
    Tick startFrame = 0;
    Tick endFrame = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        if (! enabled)
            return true;
        return startFrame >= 0 && endFrame > startFrame;
    }

    friend bool operator== (const PunchRegion&, const PunchRegion&) = default;
};

[[nodiscard]] constexpr bool automationStripParameterIdIsValid (AutomationTargetRole role, std::uint32_t paramId) noexcept
{
    switch (role)
    {
        case AutomationTargetRole::TrackFader:
        case AutomationTargetRole::BusFader:
            return paramId == FaderNode::kGainParameterId;
        case AutomationTargetRole::TrackPan:
        case AutomationTargetRole::BusPan:
            return paramId == PanNode::kPanParameterId;
        case AutomationTargetRole::SendLevel:
            // Send rows do not exist yet in CP1. For now paramId remains the target send ordinal.
            return true;
        case AutomationTargetRole::FxInsertParam:
            return true;
    }

    return false;
}

[[nodiscard]] inline bool automationBreakpointValueIsValid (double value) noexcept
{
    return std::isfinite (value) && value >= 0.0 && value <= 1.0;
}

[[nodiscard]] constexpr bool automationCurveIsStorageSafe (AutomationCurveType curve) noexcept
{
    return curve == AutomationCurveType::Linear || curve == AutomationCurveType::Hold;
}

struct AutomationBreakpoint
{
    Tick tick = 0;
    double value = 0.0;
    AutomationCurveType curveType = AutomationCurveType::Linear;

    [[nodiscard]] bool isValid() const noexcept
    {
        return tick >= 0
               && automationBreakpointValueIsValid (value)
               && automationCurveIsStorageSafe (curveType);
    }

    friend constexpr bool operator== (const AutomationBreakpoint&, const AutomationBreakpoint&) noexcept = default;
};

struct AutomationLaneData
{
    EntityId id;
    EntityId ownerEntity;
    AutomationTargetRole role = AutomationTargetRole::TrackFader;
    std::uint32_t paramId = 0;
    std::vector<AutomationBreakpoint> points;

    [[nodiscard]] bool isValid() const noexcept
    {
        if (! id.isValid() || ! ownerEntity.isValid() || ! automationTargetRoleIsKnown (role))
            return false;

        for (std::size_t i = 0; i < points.size(); ++i)
        {
            if (! points[i].isValid())
                return false;

            if (i > 0 && points[i].tick <= points[i - 1u].tick)
                return false;
        }

        return true;
    }

    friend bool operator== (const AutomationLaneData&, const AutomationLaneData&) = default;
};

enum class ProjectEditStatus : std::uint8_t
{
    Applied = 0,
    InvalidProject,
    InvalidClipId,
    ClipNotFound,
    DuplicateEntityId,
    InvalidTimelineWindow,
    InvalidSourceWindow,
    InvalidClipEnvelope,
    InvalidMidiClipId,
    MidiClipNotFound,
    InvalidNoteId,
    NoteNotFound,
    InvalidNoteWindow,
    InvalidNoteValue,
    InvalidSnapGrid,
    InvalidRecordingTakeId,
    RecordingTakeNotFound,
    InvalidRecordingCompSegmentId,
    InvalidRecordingCompWindow,
    InvalidFxOwnerId,
    FxOwnerNotFound,
    InvalidFxInsertId,
    FxInsertNotFound,
    InvalidFxKind,
    InvalidFxPosition,
    InvalidFxParamValue,
    InvalidAutomationLaneId,
    AutomationLaneNotFound,
    InvalidAutomationTarget,
    InvalidAutomationBreakpoint,
    InvalidTrackId,
    TrackNotFound,
    TrackNotEmpty,
    InvalidTrackPosition,
    InvalidTrackName,
    InvalidTempo,
    InvalidMeter,
    InvalidMarkerId,
    MarkerNotFound,
    InvalidBusId,
    BusNotFound,
    BusInUse,
    InvalidSendId,
    SendNotFound,
    DuplicateSendRoute,
    InvalidSendLevel,
    InvalidClipName,
    InvalidTrackMixState
};

struct Project
{
    static constexpr std::size_t kLocatePointCount = 5;

    EntityId id;
    SampleRate sampleRate;
    std::vector<Asset> assets;
    std::vector<Track> tracks;
    std::vector<Bus> buses;
    std::vector<Clip> clips;
    // Time-model surface (ADR-0010): tempo map, meter map, and markers round-trip with the Project (H1
    // exit clause). Stored in canonical tick order; the bundle reads them back ORDER BY tick.
    std::vector<TempoChange> tempoMap;
    std::vector<MeterChange> meterMap;
    std::vector<Marker>      markers;
    // Persisted memory locations for the transport. Empty slots migrate additively and do not appear
    // as Timeline Markers; the UI stores/recalls them through the declared locate-point actions.
    std::array<std::optional<Tick>, kLocatePointCount> locatePoints {};
    // H4 MIDI edit surface (ADR-0017): MIDI Clips own editable Notes. They flatten to Events only at the
    // render boundary; persistence keeps the stable Note IDs intact for piano-roll edits and MPE.
    std::vector<MidiClip>    midiClips;
    // H13 recording surface (ADR-0036): Takes identify recorded passes and link immutable recorded audio
    // Assets to their Track/Clip placement without making Clip identity carry recording history.
    std::vector<RecordingTake> recordingTakes;
    // H13 basic Comp surface (ADR-0036): ordered non-destructive segments choose source windows from Takes.
    std::vector<ProjectRecordingCompSegment> recordingCompSegments;
    // H15 automation surface (ADR-0039): normalized Breakpoints target stable Project entities by role.
    // Runtime NodeId resolution, schema v8 persistence, and undo verbs land in later CP1/CP3 slices.
    std::vector<AutomationLaneData> automationLanes;
    // E19: persisted master strip gain (schema v12, locate-points pattern: a missing row means
    // the historical unity default). Honest scope: gain only — no master FX chain, no master pan.
    float masterLinearGain = 1.0f;
    // N5: persisted automation write mode (schema v14, locate-points pattern: a missing row
    // means the historical Read-only default).
    AutomationMode automationMode = AutomationMode::Read;
    // N8: persisted punch region (schema v17, locate-points pattern: a missing row means
    // disabled, the historical no-punch default).
    PunchRegion punchRegion;

    [[nodiscard]] const Asset* findAsset (EntityId assetId) const noexcept
    {
        for (const Asset& asset : assets)
            if (asset.id == assetId)
                return &asset;

        return nullptr;
    }

    [[nodiscard]] const Track* findTrack (EntityId trackId) const noexcept
    {
        for (const Track& track : tracks)
            if (track.id == trackId)
                return &track;

        return nullptr;
    }

    [[nodiscard]] const Bus* findBus (EntityId busId) const noexcept
    {
        for (const Bus& bus : buses)
            if (bus.id == busId)
                return &bus;

        return nullptr;
    }

    [[nodiscard]] const FxInsert* findFxInsert (EntityId insertId) const noexcept
    {
        for (const Track& track : tracks)
            for (const FxInsert& insert : track.strip.fxChain)
                if (insert.id == insertId)
                    return &insert;

        for (const Bus& bus : buses)
            for (const FxInsert& insert : bus.strip.fxChain)
                if (insert.id == insertId)
                    return &insert;

        return nullptr;
    }

    [[nodiscard]] const RecordingTake* findRecordingTake (EntityId takeId) const noexcept
    {
        for (const RecordingTake& take : recordingTakes)
            if (take.id == takeId)
                return &take;

        return nullptr;
    }

    [[nodiscard]] bool hasValidEntityIds() const noexcept
    {
        if (! id.isValid())
            return false;

        for (const Asset& asset : assets)
            if (! asset.id.isValid())
                return false;

        for (const Track& track : tracks)
            if (! track.isValid())
                return false;

        for (const Bus& bus : buses)
            if (! bus.isValid())
                return false;

        for (const Clip& clip : clips)
            if (! clip.id.isValid() || ! clip.assetId.isValid() || ! clip.trackId.isValid())
                return false;

        for (const MidiClip& midiClip : midiClips)
            if (! midiClip.isValid())
                return false;

        for (const RecordingTake& take : recordingTakes)
            if (! take.isValid())
                return false;

        for (const ProjectRecordingCompSegment& segment : recordingCompSegments)
            if (! segment.isValid())
                return false;

        for (const AutomationLaneData& lane : automationLanes)
            if (! lane.isValid())
                return false;

        return true;
    }

    [[nodiscard]] bool tracksAreValid() const noexcept
    {
        for (const Track& track : tracks)
            if (! track.isValid())
                return false;

        return true;
    }

    [[nodiscard]] bool busesAreValid() const noexcept
    {
        for (const Bus& bus : buses)
            if (! bus.isValid())
                return false;

        return true;
    }

    [[nodiscard]] bool assetsAreValid() const noexcept
    {
        for (const Asset& asset : assets)
            if (! asset.isValid())
                return false;

        return true;
    }

    [[nodiscard]] bool hasUniqueEntityIds() const noexcept
    {
        for (std::size_t i = 0; i < assets.size(); ++i)
        {
            if (assets[i].id == id)
                return false;

            for (std::size_t j = 0; j < i; ++j)
                if (assets[i].id == assets[j].id)
                    return false;
        }

        for (std::size_t i = 0; i < tracks.size(); ++i)
        {
            if (tracks[i].id == id)
                return false;

            for (const Asset& asset : assets)
                if (tracks[i].id == asset.id)
                    return false;

            for (std::size_t j = 0; j < i; ++j)
                if (tracks[i].id == tracks[j].id)
                    return false;
        }

        for (std::size_t i = 0; i < buses.size(); ++i)
        {
            if (buses[i].id == id)
                return false;

            for (const Asset& asset : assets)
                if (buses[i].id == asset.id)
                    return false;

            for (const Track& track : tracks)
                if (buses[i].id == track.id)
                    return false;

            for (std::size_t j = 0; j < i; ++j)
                if (buses[i].id == buses[j].id)
                    return false;
        }

        for (std::size_t i = 0; i < clips.size(); ++i)
        {
            if (clips[i].id == id)
                return false;

            for (const Asset& asset : assets)
                if (clips[i].id == asset.id)
                    return false;

            for (const Track& track : tracks)
                if (clips[i].id == track.id)
                    return false;

            for (const Bus& bus : buses)
                if (clips[i].id == bus.id)
                    return false;

            for (std::size_t j = 0; j < i; ++j)
                if (clips[i].id == clips[j].id)
                    return false;
        }

        for (std::size_t i = 0; i < midiClips.size(); ++i)
        {
            const MidiClip& midiClip = midiClips[i];

            if (midiClip.id == id)
                return false;

            for (const Asset& asset : assets)
                if (midiClip.id == asset.id)
                    return false;

            for (const Track& track : tracks)
                if (midiClip.id == track.id)
                    return false;

            for (const Bus& bus : buses)
                if (midiClip.id == bus.id)
                    return false;

            for (const Clip& clip : clips)
                if (midiClip.id == clip.id)
                    return false;

            for (std::size_t j = 0; j < i; ++j)
                if (midiClip.id == midiClips[j].id)
                    return false;
        }

        for (std::size_t i = 0; i < recordingTakes.size(); ++i)
        {
            const RecordingTake& take = recordingTakes[i];

            if (take.id == id)
                return false;

            for (const Asset& asset : assets)
                if (take.id == asset.id)
                    return false;

            for (const Track& track : tracks)
                if (take.id == track.id)
                    return false;

            for (const Bus& bus : buses)
                if (take.id == bus.id)
                    return false;

            for (const Clip& clip : clips)
                if (take.id == clip.id)
                    return false;

            for (const MidiClip& midiClip : midiClips)
                if (take.id == midiClip.id)
                    return false;

            for (std::size_t j = 0; j < i; ++j)
                if (take.id == recordingTakes[j].id)
                    return false;
        }

        for (std::size_t i = 0; i < recordingCompSegments.size(); ++i)
        {
            const ProjectRecordingCompSegment& segment = recordingCompSegments[i];

            if (segment.id == id)
                return false;

            for (const Asset& asset : assets)
                if (segment.id == asset.id)
                    return false;

            for (const Track& track : tracks)
                if (segment.id == track.id)
                    return false;

            for (const Bus& bus : buses)
                if (segment.id == bus.id)
                    return false;

            for (const Clip& clip : clips)
                if (segment.id == clip.id)
                    return false;

            for (const MidiClip& midiClip : midiClips)
                if (segment.id == midiClip.id)
                    return false;

            for (const RecordingTake& take : recordingTakes)
                if (segment.id == take.id)
                    return false;

            for (std::size_t j = 0; j < i; ++j)
                if (segment.id == recordingCompSegments[j].id)
                    return false;
        }

        for (std::size_t i = 0; i < automationLanes.size(); ++i)
        {
            const AutomationLaneData& lane = automationLanes[i];

            if (lane.id == id)
                return false;

            for (const Asset& asset : assets)
                if (lane.id == asset.id)
                    return false;

            for (const Track& track : tracks)
                if (lane.id == track.id)
                    return false;

            for (const Bus& bus : buses)
                if (lane.id == bus.id)
                    return false;

            for (const Clip& clip : clips)
                if (lane.id == clip.id)
                    return false;

            for (const MidiClip& midiClip : midiClips)
                if (lane.id == midiClip.id)
                    return false;

            for (const RecordingTake& take : recordingTakes)
                if (lane.id == take.id)
                    return false;

            for (const ProjectRecordingCompSegment& segment : recordingCompSegments)
                if (lane.id == segment.id)
                    return false;

            for (std::size_t j = 0; j < i; ++j)
                if (lane.id == automationLanes[j].id)
                    return false;
        }

        const auto isNonFxProjectEntity = [this] (EntityId entity) noexcept
        {
            if (entity == id)
                return true;

            for (const Asset& asset : assets)
                if (entity == asset.id)
                    return true;

            for (const Track& track : tracks)
                if (entity == track.id)
                    return true;

            for (const Bus& bus : buses)
                if (entity == bus.id)
                    return true;

            for (const Clip& clip : clips)
                if (entity == clip.id)
                    return true;

            for (const MidiClip& midiClip : midiClips)
                if (entity == midiClip.id)
                    return true;

            for (const RecordingTake& take : recordingTakes)
                if (entity == take.id)
                    return true;

            for (const ProjectRecordingCompSegment& segment : recordingCompSegments)
                if (entity == segment.id)
                    return true;

            for (const AutomationLaneData& lane : automationLanes)
                if (entity == lane.id)
                    return true;

            return false;
        };

        const auto fxInsertIdSeenBefore = [this] (bool busOwner, std::size_t ownerIndex, std::size_t insertIndex, EntityId insertId) noexcept
        {
            for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
            {
                const std::vector<FxInsert>& chain = tracks[trackIndex].strip.fxChain;
                for (std::size_t fxIndex = 0; fxIndex < chain.size(); ++fxIndex)
                {
                    if (! busOwner && trackIndex == ownerIndex && fxIndex >= insertIndex)
                        return false;

                    if (chain[fxIndex].id == insertId)
                        return true;
                }
            }

            for (std::size_t busIndex = 0; busIndex < buses.size(); ++busIndex)
            {
                const std::vector<FxInsert>& chain = buses[busIndex].strip.fxChain;
                for (std::size_t fxIndex = 0; fxIndex < chain.size(); ++fxIndex)
                {
                    if (busOwner && busIndex == ownerIndex && fxIndex >= insertIndex)
                        return false;

                    if (chain[fxIndex].id == insertId)
                        return true;
                }
            }

            return false;
        };

        for (std::size_t trackIndex = 0; trackIndex < tracks.size(); ++trackIndex)
        {
            const std::vector<FxInsert>& chain = tracks[trackIndex].strip.fxChain;
            for (std::size_t fxIndex = 0; fxIndex < chain.size(); ++fxIndex)
            {
                const FxInsert& insert = chain[fxIndex];
                if (! insert.isValid()
                    || isNonFxProjectEntity (insert.id)
                    || fxInsertIdSeenBefore (false, trackIndex, fxIndex, insert.id))
                    return false;
            }
        }

        for (std::size_t busIndex = 0; busIndex < buses.size(); ++busIndex)
        {
            const std::vector<FxInsert>& chain = buses[busIndex].strip.fxChain;
            for (std::size_t fxIndex = 0; fxIndex < chain.size(); ++fxIndex)
            {
                const FxInsert& insert = chain[fxIndex];
                if (! insert.isValid()
                    || isNonFxProjectEntity (insert.id)
                    || fxInsertIdSeenBefore (true, busIndex, fxIndex, insert.id))
                    return false;
            }
        }

        const auto isExistingProjectEntity = [this, isNonFxProjectEntity] (EntityId entity) noexcept
        {
            if (isNonFxProjectEntity (entity))
                return true;

            for (const Track& track : tracks)
                for (const FxInsert& insert : track.strip.fxChain)
                    if (entity == insert.id)
                        return true;

            for (const Bus& bus : buses)
                for (const FxInsert& insert : bus.strip.fxChain)
                    if (entity == insert.id)
                        return true;

            return false;
        };

        for (std::size_t clipIndex = 0; clipIndex < midiClips.size(); ++clipIndex)
        {
            const MidiClip& midiClip = midiClips[clipIndex];
            for (std::size_t noteIndex = 0; noteIndex < midiClip.notes.size(); ++noteIndex)
            {
                const Note& note = midiClip.notes[noteIndex];
                if (isExistingProjectEntity (note.id))
                    return false;

                for (std::size_t earlierClip = 0; earlierClip <= clipIndex; ++earlierClip)
                {
                    const MidiClip& otherClip = midiClips[earlierClip];
                    const std::size_t endNote = earlierClip == clipIndex ? noteIndex : otherClip.notes.size();
                    for (std::size_t earlierNote = 0; earlierNote < endNote; ++earlierNote)
                        if (note.id == otherClip.notes[earlierNote].id)
                            return false;
                }
            }
        }

        return true;
    }

    [[nodiscard]] bool clipsReferenceAssets() const noexcept
    {
        for (const Clip& clip : clips)
        {
            const Asset* const asset = findAsset (clip.assetId);
            if (asset == nullptr || ! clip.sourceWindowFits (*asset))
                return false;
        }

        return true;
    }

    // M3: a Track's main output either goes to master (invalid id, the default) or names a real Bus.
    [[nodiscard]] bool trackOutputsReferenceBuses() const noexcept
    {
        for (const Track& track : tracks)
            if (track.outputBusId.isValid() && findBus (track.outputBusId) == nullptr)
                return false;

        return true;
    }

    [[nodiscard]] bool clipsReferenceTracks() const noexcept
    {
        for (const Clip& clip : clips)
            if (findTrack (clip.trackId) == nullptr)
                return false;

        for (const MidiClip& midiClip : midiClips)
            if (findTrack (midiClip.trackId) == nullptr)
                return false;

        return true;
    }

    [[nodiscard]] bool recordingTakesReferenceProjectRows() const noexcept
    {
        for (const RecordingTake& take : recordingTakes)
        {
            const Asset* const asset = findAsset (take.assetId);
            const Track* const track = findTrack (take.trackId);
            if (asset == nullptr || track == nullptr || take.frameCount > asset->frames)
                return false;

            const Clip* clip = nullptr;
            for (const Clip& candidate : clips)
            {
                if (candidate.id == take.clipId)
                {
                    clip = &candidate;
                    break;
                }
            }

            if (clip == nullptr
                || clip->assetId != take.assetId
                || clip->trackId != take.trackId
                || clip->timelineStart != take.timelineStart
                || clip->srcLen != take.frameCount)
                return false;
        }

        return true;
    }

    [[nodiscard]] bool recordingCompSegmentsReferenceTakes() const noexcept
    {
        for (const ProjectRecordingCompSegment& segment : recordingCompSegments)
        {
            const RecordingTake* const take = findRecordingTake (segment.takeId);
            if (take == nullptr || segment.sourceOffset > take->frameCount)
                return false;

            const auto remaining = take->frameCount - segment.sourceOffset;
            if (static_cast<std::uint64_t> (segment.timelineLength) > remaining)
                return false;
        }

        return true;
    }

    [[nodiscard]] bool automationTargetsReferenceProjectRows() const noexcept
    {
        for (std::size_t i = 0; i < automationLanes.size(); ++i)
        {
            const AutomationLaneData& lane = automationLanes[i];

            switch (lane.role)
            {
                case AutomationTargetRole::TrackFader:
                case AutomationTargetRole::TrackPan:
                case AutomationTargetRole::SendLevel:
                    if (findTrack (lane.ownerEntity) == nullptr)
                        return false;
                    if (! automationStripParameterIdIsValid (lane.role, lane.paramId))
                        return false;
                    break;

                case AutomationTargetRole::BusFader:
                case AutomationTargetRole::BusPan:
                    if (findBus (lane.ownerEntity) == nullptr)
                        return false;
                    if (! automationStripParameterIdIsValid (lane.role, lane.paramId))
                        return false;
                    break;

                case AutomationTargetRole::FxInsertParam:
                {
                    const FxInsert* const insert = findFxInsert (lane.ownerEntity);
                    if (insert == nullptr || ! fxKindAcceptsParameterId (insert->kind, lane.paramId))
                        return false;
                    break;
                }
            }

            for (std::size_t j = 0; j < i; ++j)
            {
                const AutomationLaneData& earlier = automationLanes[j];
                if (lane.ownerEntity == earlier.ownerEntity
                    && lane.role == earlier.role
                    && lane.paramId == earlier.paramId)
                    return false;
            }
        }

        return true;
    }

    [[nodiscard]] bool hasValidAssetClipIndirection() const noexcept
    {
        return sampleRate.isValid()
               && hasValidEntityIds()
               && assetsAreValid()
               && tracksAreValid()
               && busesAreValid()
               && hasUniqueEntityIds()
               && clipsReferenceAssets()
               && clipsReferenceTracks()
               && trackOutputsReferenceBuses()
               && recordingTakesReferenceProjectRows()
               && recordingCompSegmentsReferenceTakes()
               && automationTargetsReferenceProjectRows();
    }
};

namespace detail {

[[nodiscard]] inline bool addTickChecked (Tick a, Tick b, Tick& out) noexcept
{
    if (b > 0 && a > std::numeric_limits<Tick>::max() - b)
        return false;

    if (b < 0 && a < std::numeric_limits<Tick>::min() - b)
        return false;

    out = a + b;
    return true;
}

[[nodiscard]] inline Clip* findClip (Project& project, EntityId clipId) noexcept
{
    for (Clip& clip : project.clips)
        if (clip.id == clipId)
            return &clip;

    return nullptr;
}

[[nodiscard]] inline MidiClip* findMidiClip (Project& project, EntityId midiClipId) noexcept
{
    for (MidiClip& midiClip : project.midiClips)
        if (midiClip.id == midiClipId)
            return &midiClip;

    return nullptr;
}

[[nodiscard]] inline MixerStripState* findMixerStrip (Project& project, EntityId ownerId) noexcept
{
    for (Track& track : project.tracks)
        if (track.id == ownerId)
            return &track.strip;

    for (Bus& bus : project.buses)
        if (bus.id == ownerId)
            return &bus.strip;

    return nullptr;
}

[[nodiscard]] inline const MixerStripState* findMixerStrip (const Project& project, EntityId ownerId) noexcept
{
    for (const Track& track : project.tracks)
        if (track.id == ownerId)
            return &track.strip;

    for (const Bus& bus : project.buses)
        if (bus.id == ownerId)
            return &bus.strip;

    return nullptr;
}

[[nodiscard]] inline FxInsert* findFxInsert (MixerStripState& strip, EntityId insertId) noexcept
{
    for (FxInsert& insert : strip.fxChain)
        if (insert.id == insertId)
            return &insert;

    return nullptr;
}

[[nodiscard]] inline bool findFxInsertIndex (const MixerStripState& strip, EntityId insertId, std::size_t& out) noexcept
{
    for (std::size_t i = 0; i < strip.fxChain.size(); ++i)
    {
        if (strip.fxChain[i].id == insertId)
        {
            out = i;
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline AutomationLaneData* findAutomationLane (Project& project, EntityId laneId) noexcept
{
    for (AutomationLaneData& lane : project.automationLanes)
        if (lane.id == laneId)
            return &lane;

    return nullptr;
}

[[nodiscard]] inline bool findAutomationLaneIndex (const Project& project, EntityId laneId, std::size_t& out) noexcept
{
    for (std::size_t i = 0; i < project.automationLanes.size(); ++i)
    {
        if (project.automationLanes[i].id == laneId)
        {
            out = i;
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline bool findAutomationBreakpointIndex (const AutomationLaneData& lane, Tick tick, std::size_t& out) noexcept
{
    for (std::size_t i = 0; i < lane.points.size(); ++i)
    {
        if (lane.points[i].tick == tick)
        {
            out = i;
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline Note* findNote (MidiClip& midiClip, EntityId noteId) noexcept
{
    for (Note& note : midiClip.notes)
        if (note.id == noteId)
            return &note;

    return nullptr;
}

[[nodiscard]] inline bool findNoteIndex (const MidiClip& midiClip, EntityId noteId, std::size_t& out) noexcept
{
    for (std::size_t i = 0; i < midiClip.notes.size(); ++i)
    {
        if (midiClip.notes[i].id == noteId)
        {
            out = i;
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline bool projectContainsEntityId (const Project& project, EntityId id) noexcept
{
    if (! id.isValid())
        return false;

    if (project.id == id)
        return true;

    for (const Asset& asset : project.assets)
        if (asset.id == id)
            return true;

    for (const Track& track : project.tracks)
        if (track.id == id)
            return true;

    for (const Bus& bus : project.buses)
        if (bus.id == id)
            return true;

    for (const Clip& clip : project.clips)
        if (clip.id == id)
            return true;

    for (const MidiClip& midiClip : project.midiClips)
    {
        if (midiClip.id == id)
            return true;

        for (const Note& note : midiClip.notes)
            if (note.id == id)
                return true;
    }

    for (const RecordingTake& take : project.recordingTakes)
        if (take.id == id)
            return true;

    for (const ProjectRecordingCompSegment& segment : project.recordingCompSegments)
        if (segment.id == id)
            return true;

    for (const AutomationLaneData& lane : project.automationLanes)
        if (lane.id == id)
            return true;

    for (const Marker& marker : project.markers)
        if (marker.id == id)
            return true;

    for (const Track& track : project.tracks)
        for (const FxInsert& insert : track.strip.fxChain)
            if (insert.id == id)
                return true;

    for (const Bus& bus : project.buses)
        for (const FxInsert& insert : bus.strip.fxChain)
            if (insert.id == id)
                return true;

    for (const Track& track : project.tracks)
        for (const SendRow& send : track.sends)
            if (send.id == id)
                return true;

    return false;
}

[[nodiscard]] inline const RecordingTake* findRecordingTake (const Project& project, EntityId takeId) noexcept
{
    return project.findRecordingTake (takeId);
}

[[nodiscard]] inline bool clipSourceWindowFitsProject (const Project& project, const Clip& clip) noexcept
{
    const Asset* const asset = project.findAsset (clip.assetId);
    return asset != nullptr && clip.sourceWindowFits (*asset);
}

[[nodiscard]] inline bool clipGainIsStorageSafe (float gain) noexcept
{
    return gain >= 0.0f && gain <= std::numeric_limits<float>::max();
}

[[nodiscard]] inline bool clipFadesAreStorageSafe (Tick fadeIn, Tick fadeOut) noexcept
{
    return fadeIn >= 0 && fadeOut >= 0;
}

[[nodiscard]] inline bool clipEditMetadataIsStorageSafe (const Clip& clip) noexcept
{
    return ! clip.name.empty()
           && clip.name.size() <= ClipName::kMaxLength
           && clip.timelineLength >= 0
           && clipGainIsStorageSafe (clip.gain)
           && clipFadesAreStorageSafe (clip.fadeIn, clip.fadeOut)
           && (clip.timeBase == TimeBase::TempoLocked || clip.timeBase == TimeBase::SampleLocked);
}

[[nodiscard]] inline bool projectCanApplyClipEdit (const Project& project) noexcept
{
    if (! project.hasValidAssetClipIndirection())
        return false;

    for (const Clip& clip : project.clips)
        if (! clipEditMetadataIsStorageSafe (clip))
            return false;

    return true;
}

[[nodiscard]] inline bool noteFitsMidiClip (const MidiClip& midiClip, const Note& note) noexcept
{
    if (! note.isValid() || note.startTick > midiClip.timelineLength)
        return false;

    return note.lengthTicks <= midiClip.timelineLength - note.startTick;
}

[[nodiscard]] inline bool projectCanApplyMidiEdit (const Project& project) noexcept
{
    return project.hasValidAssetClipIndirection();
}

[[nodiscard]] inline bool projectCanApplyFxEdit (const Project& project) noexcept
{
    return project.hasValidAssetClipIndirection();
}

[[nodiscard]] inline bool projectCanApplyAutomationEdit (const Project& project) noexcept
{
    return project.hasValidAssetClipIndirection();
}

[[nodiscard]] inline bool findNormalizedFxParamIndex (const FxInsert& insert, std::uint32_t paramId, std::size_t& out) noexcept
{
    for (std::size_t i = 0; i < insert.normalizedParams.size(); ++i)
    {
        if (insert.normalizedParams[i].first == paramId)
        {
            out = i;
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline bool compSegmentWindowFitsTake (const ProjectRecordingCompSegment& segment,
                                                     const RecordingTake& take) noexcept
{
    return segment.isValid()
           && segment.sourceOffset <= take.frameCount
           && static_cast<std::uint64_t> (segment.timelineLength) <= take.frameCount - segment.sourceOffset;
}

} // namespace detail

[[nodiscard]] inline ProjectEditStatus moveClip (Project& project, EntityId clipId, Tick newTimelineStart) noexcept
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    Clip* const clip = detail::findClip (project, clipId);
    if (clip == nullptr)
        return ProjectEditStatus::ClipNotFound;

    clip->timelineStart = newTimelineStart;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus renameClip (Project& project,
                                                    EntityId clipId,
                                                    std::string_view newName) noexcept
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    if (newName.empty() || newName.size() > ClipName::kMaxLength)
        return ProjectEditStatus::InvalidClipName;

    Clip* const clip = detail::findClip (project, clipId);
    if (clip == nullptr)
        return ProjectEditStatus::ClipNotFound;

    (void) clip->name.assign (newName);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setClipGain (Project& project, EntityId clipId, float newGain) noexcept
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    Clip* const clip = detail::findClip (project, clipId);
    if (clip == nullptr)
        return ProjectEditStatus::ClipNotFound;

    if (! detail::clipGainIsStorageSafe (newGain))
        return ProjectEditStatus::InvalidClipEnvelope;

    clip->gain = newGain;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setClipFades (Project& project,
                                                     EntityId clipId,
                                                     Tick newFadeIn,
                                                     Tick newFadeOut) noexcept
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    Clip* const clip = detail::findClip (project, clipId);
    if (clip == nullptr)
        return ProjectEditStatus::ClipNotFound;

    if (! detail::clipFadesAreStorageSafe (newFadeIn, newFadeOut))
        return ProjectEditStatus::InvalidClipEnvelope;

    clip->fadeIn = newFadeIn;
    clip->fadeOut = newFadeOut;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus trimClip (Project& project,
                                                 EntityId clipId,
                                                 Tick newTimelineStart,
                                                 Tick newTimelineLength,
                                                 std::uint64_t newSrcOffset,
                                                 std::uint64_t newSrcLen) noexcept
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    Clip* const clip = detail::findClip (project, clipId);
    if (clip == nullptr)
        return ProjectEditStatus::ClipNotFound;

    if (newTimelineLength < 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    Clip edited = *clip;
    edited.timelineStart = newTimelineStart;
    edited.timelineLength = newTimelineLength;
    edited.srcOffset = newSrcOffset;
    edited.srcLen = newSrcLen;

    if (! detail::clipSourceWindowFitsProject (project, edited))
        return ProjectEditStatus::InvalidSourceWindow;

    *clip = edited;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus splitClip (Project& project,
                                                  EntityId clipId,
                                                  EntityId rightClipId,
                                                  Tick leftTimelineLength,
                                                  std::uint64_t leftSourceLength)
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid() || ! rightClipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    if (detail::projectContainsEntityId (project, rightClipId))
        return ProjectEditStatus::DuplicateEntityId;

    std::size_t clipIndex = project.clips.size();
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        if (project.clips[i].id == clipId)
        {
            clipIndex = i;
            break;
        }
    }

    if (clipIndex == project.clips.size())
        return ProjectEditStatus::ClipNotFound;

    const Clip original = project.clips[clipIndex];
    if (leftTimelineLength <= 0 || leftTimelineLength >= original.timelineLength)
        return ProjectEditStatus::InvalidTimelineWindow;

    Tick rightTimelineStart = 0;
    if (! detail::addTickChecked (original.timelineStart, leftTimelineLength, rightTimelineStart))
        return ProjectEditStatus::InvalidTimelineWindow;

    if (leftSourceLength == 0 || leftSourceLength >= original.srcLen)
        return ProjectEditStatus::InvalidSourceWindow;

    Clip left = original;
    left.timelineLength = leftTimelineLength;
    left.srcLen = leftSourceLength;

    Clip right = original;
    right.id = rightClipId;
    right.timelineStart = rightTimelineStart;
    right.timelineLength = original.timelineLength - leftTimelineLength;
    right.srcOffset = original.srcOffset + leftSourceLength;
    right.srcLen = original.srcLen - leftSourceLength;

    if (! detail::clipSourceWindowFitsProject (project, left) || ! detail::clipSourceWindowFitsProject (project, right))
        return ProjectEditStatus::InvalidSourceWindow;

    project.clips.insert (project.clips.begin() + static_cast<std::ptrdiff_t> (clipIndex + 1u), right);
    project.clips[clipIndex] = left;
    project.clips[clipIndex + 1u] = right;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus moveNote (Project& project,
                                                 EntityId midiClipId,
                                                 EntityId noteId,
                                                 Tick newStartTick) noexcept
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (! noteId.isValid())
        return ProjectEditStatus::InvalidNoteId;

    MidiClip* const midiClip = detail::findMidiClip (project, midiClipId);
    if (midiClip == nullptr)
        return ProjectEditStatus::MidiClipNotFound;

    Note* const note = detail::findNote (*midiClip, noteId);
    if (note == nullptr)
        return ProjectEditStatus::NoteNotFound;

    Note edited = *note;
    edited.startTick = newStartTick;
    if (! detail::noteFitsMidiClip (*midiClip, edited))
        return ProjectEditStatus::InvalidNoteWindow;

    *note = edited;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setNoteLength (Project& project,
                                                      EntityId midiClipId,
                                                      EntityId noteId,
                                                      Tick newLengthTicks) noexcept
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (! noteId.isValid())
        return ProjectEditStatus::InvalidNoteId;

    MidiClip* const midiClip = detail::findMidiClip (project, midiClipId);
    if (midiClip == nullptr)
        return ProjectEditStatus::MidiClipNotFound;

    Note* const note = detail::findNote (*midiClip, noteId);
    if (note == nullptr)
        return ProjectEditStatus::NoteNotFound;

    Note edited = *note;
    edited.lengthTicks = newLengthTicks;
    if (! detail::noteFitsMidiClip (*midiClip, edited))
        return ProjectEditStatus::InvalidNoteWindow;

    *note = edited;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus splitNote (Project& project,
                                                  EntityId midiClipId,
                                                  EntityId noteId,
                                                  EntityId rightNoteId,
                                                  Tick leftLengthTicks)
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (! noteId.isValid() || ! rightNoteId.isValid())
        return ProjectEditStatus::InvalidNoteId;

    if (detail::projectContainsEntityId (project, rightNoteId))
        return ProjectEditStatus::DuplicateEntityId;

    MidiClip* const midiClip = detail::findMidiClip (project, midiClipId);
    if (midiClip == nullptr)
        return ProjectEditStatus::MidiClipNotFound;

    std::size_t noteIndex = midiClip->notes.size();
    if (! detail::findNoteIndex (*midiClip, noteId, noteIndex))
        return ProjectEditStatus::NoteNotFound;

    const Note original = midiClip->notes[noteIndex];
    if (leftLengthTicks <= 0 || leftLengthTicks >= original.lengthTicks)
        return ProjectEditStatus::InvalidNoteWindow;

    Tick rightStartTick = 0;
    if (! detail::addTickChecked (original.startTick, leftLengthTicks, rightStartTick))
        return ProjectEditStatus::InvalidNoteWindow;

    Note left = original;
    left.lengthTicks = leftLengthTicks;

    Note right = original;
    right.id = rightNoteId;
    right.startTick = rightStartTick;
    right.lengthTicks = original.lengthTicks - leftLengthTicks;

    if (! detail::noteFitsMidiClip (*midiClip, left) || ! detail::noteFitsMidiClip (*midiClip, right))
        return ProjectEditStatus::InvalidNoteWindow;

    midiClip->notes.insert (midiClip->notes.begin() + static_cast<std::ptrdiff_t> (noteIndex + 1u), right);
    midiClip->notes[noteIndex] = left;
    midiClip->notes[noteIndex + 1u] = right;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus cutNote (Project& project,
                                                EntityId midiClipId,
                                                EntityId noteId)
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (! noteId.isValid())
        return ProjectEditStatus::InvalidNoteId;

    MidiClip* const midiClip = detail::findMidiClip (project, midiClipId);
    if (midiClip == nullptr)
        return ProjectEditStatus::MidiClipNotFound;

    std::size_t noteIndex = midiClip->notes.size();
    if (! detail::findNoteIndex (*midiClip, noteId, noteIndex))
        return ProjectEditStatus::NoteNotFound;

    midiClip->notes.erase (midiClip->notes.begin() + static_cast<std::ptrdiff_t> (noteIndex));
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus deleteNote (Project& project,
                                                   EntityId midiClipId,
                                                   EntityId noteId)
{
    return cutNote (project, midiClipId, noteId);
}

[[nodiscard]] inline ProjectEditStatus quantizeNote (Project& project,
                                                     EntityId midiClipId,
                                                     EntityId noteId,
                                                     SnapGrid grid) noexcept
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (! noteId.isValid())
        return ProjectEditStatus::InvalidNoteId;

    if (! grid.isValid())
        return ProjectEditStatus::InvalidSnapGrid;

    MidiClip* const midiClip = detail::findMidiClip (project, midiClipId);
    if (midiClip == nullptr)
        return ProjectEditStatus::MidiClipNotFound;

    Note* const note = detail::findNote (*midiClip, noteId);
    if (note == nullptr)
        return ProjectEditStatus::NoteNotFound;

    Tick snappedStart = 0;
    if (! snapTick (note->startTick, grid, snappedStart))
        return ProjectEditStatus::InvalidNoteWindow;

    Note edited = *note;
    edited.startTick = snappedStart;
    if (! detail::noteFitsMidiClip (*midiClip, edited))
        return ProjectEditStatus::InvalidNoteWindow;

    *note = edited;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus transposeNote (Project& project,
                                                      EntityId midiClipId,
                                                      EntityId noteId,
                                                      int semitones) noexcept
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (! noteId.isValid())
        return ProjectEditStatus::InvalidNoteId;

    MidiClip* const midiClip = detail::findMidiClip (project, midiClipId);
    if (midiClip == nullptr)
        return ProjectEditStatus::MidiClipNotFound;

    Note* const note = detail::findNote (*midiClip, noteId);
    if (note == nullptr)
        return ProjectEditStatus::NoteNotFound;

    const int transposedKey = static_cast<int> (note->key) + semitones;
    if (transposedKey < 0 || transposedKey > 127)
        return ProjectEditStatus::InvalidNoteValue;

    Note edited = *note;
    edited.key = static_cast<std::int16_t> (transposedKey);
    edited.pitchNote += static_cast<double> (semitones);
    if (! edited.isValid())
        return ProjectEditStatus::InvalidNoteValue;

    *note = edited;
    return ProjectEditStatus::Applied;
}

// Set one Note's normalized velocity (B33 piano-roll velocity editing). Same contract as every
// note edit helper above: validate, mutate, Applied.
[[nodiscard]] inline ProjectEditStatus setNoteVelocity (Project& project,
                                                        EntityId midiClipId,
                                                        EntityId noteId,
                                                        double normalizedVelocity) noexcept
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (! noteId.isValid())
        return ProjectEditStatus::InvalidNoteId;

    if (! std::isfinite (normalizedVelocity) || normalizedVelocity < 0.0 || normalizedVelocity > 1.0)
        return ProjectEditStatus::InvalidNoteValue;

    MidiClip* const midiClip = detail::findMidiClip (project, midiClipId);
    if (midiClip == nullptr)
        return ProjectEditStatus::MidiClipNotFound;

    Note* const note = detail::findNote (*midiClip, noteId);
    if (note == nullptr)
        return ProjectEditStatus::NoteNotFound;

    note->normalizedVelocity = normalizedVelocity;
    return ProjectEditStatus::Applied;
}

// --- Arrangement verbs (usable-DAW P0, 2026-08-09): clip delete / cross-track move, note add, and
// --- the Track lifecycle. Same contract as every edit helper above: validate, mutate, Applied.

// Insert a fully-specified Clip row referencing an EXISTING Asset and Track (paste/duplicate,
// usable-DAW P1). The same storage-safety rules as import apply; identity must be fresh.
// Insert an empty (or pre-filled) MIDI Clip on an existing Track (usable-DAW P1: sequence MIDI
// without recording).
[[nodiscard]] inline ProjectEditStatus addMidiClip (Project& project, const MidiClip& midiClip)
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClip.id.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (detail::projectContainsEntityId (project, midiClip.id))
        return ProjectEditStatus::DuplicateEntityId;

    if (project.findTrack (midiClip.trackId) == nullptr)
        return ProjectEditStatus::TrackNotFound;

    if (! midiClip.isValid() || midiClip.timelineStart < 0 || midiClip.timelineLength <= 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    project.midiClips.push_back (midiClip);
    return ProjectEditStatus::Applied;
}

// E8: MIDI clips move, change tracks, and delete like audio clips.
[[nodiscard]] inline ProjectEditStatus moveMidiClip (Project& project, EntityId midiClipId, Tick newTimelineStart)
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (newTimelineStart < 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    for (MidiClip& midiClip : project.midiClips)
    {
        if (midiClip.id == midiClipId)
        {
            midiClip.timelineStart = newTimelineStart;
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::MidiClipNotFound;
}

[[nodiscard]] inline ProjectEditStatus moveMidiClipToTrack (Project& project,
                                                            EntityId midiClipId,
                                                            EntityId targetTrackId,
                                                            Tick newTimelineStart)
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (project.findTrack (targetTrackId) == nullptr)
        return ProjectEditStatus::TrackNotFound;

    if (newTimelineStart < 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    for (MidiClip& midiClip : project.midiClips)
    {
        if (midiClip.id == midiClipId)
        {
            midiClip.trackId = targetTrackId;
            midiClip.timelineStart = newTimelineStart;
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::MidiClipNotFound;
}

[[nodiscard]] inline ProjectEditStatus removeMidiClip (Project& project, EntityId midiClipId)
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    for (std::size_t i = 0; i < project.midiClips.size(); ++i)
    {
        if (project.midiClips[i].id == midiClipId)
        {
            project.midiClips.erase (project.midiClips.begin() + static_cast<std::ptrdiff_t> (i));
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::MidiClipNotFound;
}

[[nodiscard]] inline ProjectEditStatus addClip (Project& project, const Clip& clip)
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clip.id.isValid())
        return ProjectEditStatus::InvalidClipId;

    if (detail::projectContainsEntityId (project, clip.id))
        return ProjectEditStatus::DuplicateEntityId;

    if (project.findAsset (clip.assetId) == nullptr)
        return ProjectEditStatus::InvalidProject;

    if (project.findTrack (clip.trackId) == nullptr)
        return ProjectEditStatus::TrackNotFound;

    if (clip.timelineStart < 0 || clip.timelineLength <= 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    if (clip.name.empty() || clip.name.size() > ClipName::kMaxLength)
        return ProjectEditStatus::InvalidClipName;

    if (! detail::clipEditMetadataIsStorageSafe (clip))
        return ProjectEditStatus::InvalidClipEnvelope;

    if (! detail::clipSourceWindowFitsProject (project, clip))
        return ProjectEditStatus::InvalidSourceWindow;

    project.clips.push_back (clip);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus deleteClip (Project& project, EntityId clipId)
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        if (project.clips[i].id == clipId)
        {
            project.clips.erase (project.clips.begin() + static_cast<std::ptrdiff_t> (i));
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::ClipNotFound;
}

[[nodiscard]] inline ProjectEditStatus moveClipToTrack (Project& project,
                                                        EntityId clipId,
                                                        EntityId targetTrackId,
                                                        Tick newTimelineStart)
{
    if (! detail::projectCanApplyClipEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! clipId.isValid())
        return ProjectEditStatus::InvalidClipId;

    if (! targetTrackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    Clip* const clip = detail::findClip (project, clipId);
    if (clip == nullptr)
        return ProjectEditStatus::ClipNotFound;

    if (project.findTrack (targetTrackId) == nullptr)
        return ProjectEditStatus::TrackNotFound;

    if (newTimelineStart < 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    clip->trackId = targetTrackId;
    clip->timelineStart = newTimelineStart;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus addNote (Project& project,
                                                EntityId midiClipId,
                                                const Note& note)
{
    if (! detail::projectCanApplyMidiEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! midiClipId.isValid())
        return ProjectEditStatus::InvalidMidiClipId;

    if (! note.id.isValid())
        return ProjectEditStatus::InvalidNoteId;

    MidiClip* const midiClip = detail::findMidiClip (project, midiClipId);
    if (midiClip == nullptr)
        return ProjectEditStatus::MidiClipNotFound;

    if (detail::projectContainsEntityId (project, note.id))
        return ProjectEditStatus::DuplicateEntityId;

    if (! note.isValid())
        return ProjectEditStatus::InvalidNoteValue;

    if (! detail::noteFitsMidiClip (*midiClip, note))
        return ProjectEditStatus::InvalidNoteWindow;

    midiClip->notes.push_back (note);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus addTrack (Project& project, const Track& track)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! track.id.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (detail::projectContainsEntityId (project, track.id))
        return ProjectEditStatus::DuplicateEntityId;

    if (! track.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (track.strip.name.empty())
        return ProjectEditStatus::InvalidTrackName;

    project.tracks.push_back (track);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus renameTrack (Project& project,
                                                    EntityId trackId,
                                                    const std::string& newName)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (newName.empty())
        return ProjectEditStatus::InvalidTrackName;

    for (Track& track : project.tracks)
    {
        if (track.id == trackId)
        {
            track.strip.name = newName;
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::TrackNotFound;
}

// Set the scalar mixer strip state of one Track as an undoable edit. Interactive fader/pan/mute
// gestures stay direct strip edits; this verb exists for whole-strip copies (duplicate track).
[[nodiscard]] inline ProjectEditStatus setTrackMixScalars (Project& project,
                                                           EntityId trackId,
                                                           float linearGain,
                                                           float pan,
                                                           bool muted,
                                                           bool soloed,
                                                           bool soloSafe)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (! mixerGainIsValid (linearGain) || ! mixerPanIsValid (pan))
        return ProjectEditStatus::InvalidTrackMixState;

    for (Track& track : project.tracks)
    {
        if (track.id == trackId)
        {
            track.strip.linearGain = linearGain;
            track.strip.pan = pan;
            track.strip.muted = muted;
            track.strip.soloed = soloed;
            track.strip.soloSafe = soloSafe;
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::TrackNotFound;
}

// E19: the persisted master strip gain — one scalar, validated like every strip gain.
[[nodiscard]] inline ProjectEditStatus setMasterGain (Project& project, float linearGain)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! mixerGainIsValid (linearGain))
        return ProjectEditStatus::InvalidTrackMixState;

    project.masterLinearGain = linearGain;
    return ProjectEditStatus::Applied;
}

// N5: the persisted automation write mode — one scalar, like the master strip gain.
[[nodiscard]] inline ProjectEditStatus setAutomationMode (Project& project, AutomationMode mode)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! automationModeIsKnown (mode))
        return ProjectEditStatus::InvalidProject;

    project.automationMode = mode;
    return ProjectEditStatus::Applied;
}

// N8: the persisted punch region — one scalar-ish struct, like the automation mode.
[[nodiscard]] inline ProjectEditStatus setPunchRegion (Project& project, PunchRegion region)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! region.isValid())
        return ProjectEditStatus::InvalidProject;

    project.punchRegion = region;
    return ProjectEditStatus::Applied;
}

// E17: the bus twin of renameTrack — empty names are refused before dispatch reaches here.
[[nodiscard]] inline ProjectEditStatus renameBus (Project& project,
                                                  EntityId busId,
                                                  const std::string& newName)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! busId.isValid())
        return ProjectEditStatus::InvalidBusId;

    if (newName.empty())
        return ProjectEditStatus::InvalidTrackName;

    for (Bus& bus : project.buses)
    {
        if (bus.id == busId)
        {
            bus.strip.name = newName;
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::BusNotFound;
}

// E16: the bus twin of setTrackMixScalars — same validation, targeting a Bus strip.
[[nodiscard]] inline ProjectEditStatus setBusMixScalars (Project& project,
                                                         EntityId busId,
                                                         float linearGain,
                                                         float pan,
                                                         bool muted,
                                                         bool soloed,
                                                         bool soloSafe)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! busId.isValid())
        return ProjectEditStatus::InvalidBusId;

    if (! mixerGainIsValid (linearGain) || ! mixerPanIsValid (pan))
        return ProjectEditStatus::InvalidTrackMixState;

    for (Bus& bus : project.buses)
    {
        if (bus.id == busId)
        {
            bus.strip.linearGain = linearGain;
            bus.strip.pan = pan;
            bus.strip.muted = muted;
            bus.strip.soloed = soloed;
            bus.strip.soloSafe = soloSafe;
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::BusNotFound;
}

[[nodiscard]] inline ProjectEditStatus reorderTrack (Project& project,
                                                     EntityId trackId,
                                                     std::size_t newIndex)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (newIndex >= project.tracks.size())
        return ProjectEditStatus::InvalidTrackPosition;

    std::size_t currentIndex = project.tracks.size();
    for (std::size_t i = 0; i < project.tracks.size(); ++i)
    {
        if (project.tracks[i].id == trackId)
        {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex >= project.tracks.size())
        return ProjectEditStatus::TrackNotFound;

    if (currentIndex == newIndex)
        return ProjectEditStatus::Applied;

    Track moved = project.tracks[currentIndex];
    project.tracks.erase (project.tracks.begin() + static_cast<std::ptrdiff_t> (currentIndex));
    project.tracks.insert (project.tracks.begin() + static_cast<std::ptrdiff_t> (newIndex), std::move (moved));
    return ProjectEditStatus::Applied;
}

// Removing a Track requires it to be empty: no Clip, MIDI Clip, recording Take, or automation lane may
// still reference it. Callers that want Pro Tools-style delete-with-contents issue the owned deletions
// first (each its own undoable command), then remove the Track.
[[nodiscard]] inline ProjectEditStatus removeTrack (Project& project, EntityId trackId)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    std::size_t trackIndex = project.tracks.size();
    for (std::size_t i = 0; i < project.tracks.size(); ++i)
    {
        if (project.tracks[i].id == trackId)
        {
            trackIndex = i;
            break;
        }
    }

    if (trackIndex >= project.tracks.size())
        return ProjectEditStatus::TrackNotFound;

    for (const Clip& clip : project.clips)
        if (clip.trackId == trackId)
            return ProjectEditStatus::TrackNotEmpty;

    for (const MidiClip& midiClip : project.midiClips)
        if (midiClip.trackId == trackId)
            return ProjectEditStatus::TrackNotEmpty;

    for (const RecordingTake& take : project.recordingTakes)
        if (take.trackId == trackId)
            return ProjectEditStatus::TrackNotEmpty;

    for (const AutomationLaneData& lane : project.automationLanes)
        if (lane.ownerEntity == trackId)
            return ProjectEditStatus::TrackNotEmpty;

    project.tracks.erase (project.tracks.begin() + static_cast<std::ptrdiff_t> (trackIndex));
    return ProjectEditStatus::Applied;
}

// ADR-0044: persisted send routing verbs. Sends are rows on the owning Track; Buses are removable
// only while nothing routes to or targets them (mirror of empty-only Track removal).
[[nodiscard]] inline ProjectEditStatus addBus (Project& project, const Bus& bus)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! bus.id.isValid())
        return ProjectEditStatus::InvalidBusId;

    if (detail::projectContainsEntityId (project, bus.id))
        return ProjectEditStatus::DuplicateEntityId;

    if (! bus.isValid() || bus.strip.name.empty())
        return ProjectEditStatus::InvalidBusId;

    project.buses.push_back (bus);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus removeBus (Project& project, EntityId busId)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! busId.isValid())
        return ProjectEditStatus::InvalidBusId;

    std::size_t busIndex = project.buses.size();
    for (std::size_t i = 0; i < project.buses.size(); ++i)
    {
        if (project.buses[i].id == busId)
        {
            busIndex = i;
            break;
        }
    }

    if (busIndex >= project.buses.size())
        return ProjectEditStatus::BusNotFound;

    for (const Track& track : project.tracks)
        for (const SendRow& send : track.sends)
            if (send.busId == busId)
                return ProjectEditStatus::BusInUse;

    // M3: a Bus that a Track's MAIN output lands on is in use exactly like a send destination.
    for (const Track& track : project.tracks)
        if (track.outputBusId == busId)
            return ProjectEditStatus::BusInUse;

    for (const AutomationLaneData& lane : project.automationLanes)
        if (lane.ownerEntity == busId)
            return ProjectEditStatus::BusInUse;

    project.buses.erase (project.buses.begin() + static_cast<std::ptrdiff_t> (busIndex));
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus addSend (Project& project,
                                                EntityId trackId,
                                                const SendRow& send)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (! send.id.isValid() || ! send.isValid())
        return ProjectEditStatus::InvalidSendId;

    if (detail::projectContainsEntityId (project, send.id))
        return ProjectEditStatus::DuplicateEntityId;

    if (project.findBus (send.busId) == nullptr)
        return ProjectEditStatus::BusNotFound;

    for (Track& track : project.tracks)
    {
        if (track.id != trackId)
            continue;

        for (const SendRow& existing : track.sends)
            if (existing.busId == send.busId)
                return ProjectEditStatus::DuplicateSendRoute;

        track.sends.push_back (send);
        return ProjectEditStatus::Applied;
    }

    return ProjectEditStatus::TrackNotFound;
}

[[nodiscard]] inline ProjectEditStatus removeSend (Project& project,
                                                   EntityId trackId,
                                                   EntityId sendId)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (! sendId.isValid())
        return ProjectEditStatus::InvalidSendId;

    for (Track& track : project.tracks)
    {
        if (track.id != trackId)
            continue;

        for (std::size_t i = 0; i < track.sends.size(); ++i)
        {
            if (track.sends[i].id == sendId)
            {
                track.sends.erase (track.sends.begin() + static_cast<std::ptrdiff_t> (i));
                return ProjectEditStatus::Applied;
            }
        }

        return ProjectEditStatus::SendNotFound;
    }

    return ProjectEditStatus::TrackNotFound;
}

// E18: change a send's tap point (pre/post fader) — the tap column has been persisted since
// schema v9; this is its first mutating verb.
[[nodiscard]] inline ProjectEditStatus setSendTap (Project& project,
                                                   EntityId trackId,
                                                   EntityId sendId,
                                                   SendTap tap)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (! sendId.isValid())
        return ProjectEditStatus::InvalidSendId;

    for (Track& track : project.tracks)
    {
        if (track.id != trackId)
            continue;

        for (SendRow& send : track.sends)
        {
            if (send.id == sendId)
            {
                send.tap = tap;
                return ProjectEditStatus::Applied;
            }
        }

        return ProjectEditStatus::SendNotFound;
    }

    return ProjectEditStatus::TrackNotFound;
}

// M3: route a Track's MAIN output. An invalid busId means master (the historical default); a valid
// one must name an existing Bus. Buses feed the master directly and nothing feeds a Track, so this
// routing cannot make a cycle — the only refusals are unknown Track and unknown Bus.
[[nodiscard]] inline ProjectEditStatus setTrackOutput (Project& project,
                                                       EntityId trackId,
                                                       EntityId busId)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (busId.isValid() && project.findBus (busId) == nullptr)
        return ProjectEditStatus::BusNotFound;

    for (Track& track : project.tracks)
    {
        if (track.id != trackId)
            continue;

        track.outputBusId = busId;
        return ProjectEditStatus::Applied;
    }

    return ProjectEditStatus::TrackNotFound;
}

// N6: persisted row height — 0 clears back to the auto-shared default.
[[nodiscard]] inline ProjectEditStatus setTrackHeight (Project& project,
                                                       EntityId trackId,
                                                       int heightPx)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (! trackHeightIsValid (heightPx))
        return ProjectEditStatus::InvalidTrackMixState;

    for (Track& track : project.tracks)
    {
        if (track.id != trackId)
            continue;

        track.heightPx = heightPx;
        return ProjectEditStatus::Applied;
    }

    return ProjectEditStatus::TrackNotFound;
}

// N7: persisted row colour — kTrackColourUnset clears back to the historical colour-less default.
[[nodiscard]] inline ProjectEditStatus setTrackColour (Project& project,
                                                       EntityId trackId,
                                                       std::uint32_t colour)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (! trackColourIsValid (colour))
        return ProjectEditStatus::InvalidTrackMixState;

    for (Track& track : project.tracks)
    {
        if (track.id != trackId)
            continue;

        track.colour = colour;
        return ProjectEditStatus::Applied;
    }

    return ProjectEditStatus::TrackNotFound;
}

[[nodiscard]] inline ProjectEditStatus setSendLevel (Project& project,
                                                     EntityId trackId,
                                                     EntityId sendId,
                                                     float linearGain)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! trackId.isValid())
        return ProjectEditStatus::InvalidTrackId;

    if (! sendId.isValid())
        return ProjectEditStatus::InvalidSendId;

    if (! mixerGainIsValid (linearGain))
        return ProjectEditStatus::InvalidSendLevel;

    for (Track& track : project.tracks)
    {
        if (track.id != trackId)
            continue;

        for (SendRow& send : track.sends)
        {
            if (send.id == sendId)
            {
                send.linearGain = linearGain;
                return ProjectEditStatus::Applied;
            }
        }

        return ProjectEditStatus::SendNotFound;
    }

    return ProjectEditStatus::TrackNotFound;
}

// Timeline markers (usable-DAW P1): named positions on the ruler, stored in canonical tick order.
[[nodiscard]] inline ProjectEditStatus addMarker (Project& project, const Marker& marker)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! marker.id.isValid())
        return ProjectEditStatus::InvalidMarkerId;

    if (detail::projectContainsEntityId (project, marker.id))
        return ProjectEditStatus::DuplicateEntityId;

    if (marker.tick < 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    const auto insertAt = std::find_if (project.markers.begin(), project.markers.end(),
                                        [&marker] (const Marker& existing) { return existing.tick > marker.tick; });
    project.markers.insert (insertAt, marker);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus removeMarker (Project& project, EntityId markerId)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! markerId.isValid())
        return ProjectEditStatus::InvalidMarkerId;

    for (std::size_t i = 0; i < project.markers.size(); ++i)
    {
        if (project.markers[i].id == markerId)
        {
            project.markers.erase (project.markers.begin() + static_cast<std::ptrdiff_t> (i));
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::MarkerNotFound;
}

// E7: move a marker to a new tick, preserving addMarker's sorted-insert law.
[[nodiscard]] inline ProjectEditStatus moveMarker (Project& project, EntityId markerId, Tick newTick)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! markerId.isValid())
        return ProjectEditStatus::InvalidMarkerId;

    if (newTick < 0)
        return ProjectEditStatus::InvalidTimelineWindow;

    for (std::size_t i = 0; i < project.markers.size(); ++i)
    {
        if (project.markers[i].id == markerId)
        {
            Marker moved = project.markers[i];
            moved.tick = newTick;
            project.markers.erase (project.markers.begin() + static_cast<std::ptrdiff_t> (i));
            const auto insertAt = std::find_if (project.markers.begin(), project.markers.end(),
                                                [&moved] (const Marker& existing) { return existing.tick > moved.tick; });
            project.markers.insert (insertAt, moved);
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::MarkerNotFound;
}

// E7: rename a marker; like renameTrack, the length cap lives at the command factory's shared
// name buffer, so the engine only refuses empty names.
[[nodiscard]] inline ProjectEditStatus renameMarker (Project& project, EntityId markerId, std::string_view name)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! markerId.isValid())
        return ProjectEditStatus::InvalidMarkerId;

    if (name.empty())
        return ProjectEditStatus::InvalidTrackName;

    for (Marker& marker : project.markers)
    {
        if (marker.id == markerId)
        {
            marker.name = std::string { name };
            return ProjectEditStatus::Applied;
        }
    }

    return ProjectEditStatus::MarkerNotFound;
}

// Alpha time-map editing (usable-DAW P0): a single Project-wide tempo and meter — the head of each
// map. Multi-segment tempo/meter editing arrives with the tempo-map UI later.
[[nodiscard]] inline ProjectEditStatus setProjectTempo (Project& project, double bpm)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! std::isfinite (bpm) || bpm < 20.0 || bpm > 400.0)
        return ProjectEditStatus::InvalidTempo;

    if (project.tempoMap.empty())
        project.tempoMap.push_back (TempoChange { 0, bpm, TempoCurve::Jump });
    else
        project.tempoMap.front().bpm = bpm;

    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setProjectMeter (Project& project,
                                                        std::uint16_t numerator,
                                                        std::uint16_t denominator)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    const bool denominatorIsPowerOfTwo = denominator > 0 && (denominator & (denominator - 1u)) == 0u;
    if (numerator == 0 || numerator > 32 || ! denominatorIsPowerOfTwo || denominator > 32)
        return ProjectEditStatus::InvalidMeter;

    if (project.meterMap.empty())
        project.meterMap.push_back (MeterChange { 0, numerator, denominator });
    else
    {
        project.meterMap.front().numerator = numerator;
        project.meterMap.front().denominator = denominator;
    }

    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setRecordingCompSelection (
    Project& project,
    EntityId firstSegmentId,
    EntityId firstTakeId,
    Tick firstTimelineStart,
    Tick firstTimelineLength,
    std::uint64_t firstSourceOffset,
    EntityId secondSegmentId,
    EntityId secondTakeId,
    Tick secondTimelineStart,
    Tick secondTimelineLength,
    std::uint64_t secondSourceOffset)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! firstSegmentId.isValid() || ! secondSegmentId.isValid() || firstSegmentId == secondSegmentId)
        return ProjectEditStatus::InvalidRecordingCompSegmentId;

    if (detail::projectContainsEntityId (project, firstSegmentId)
        || detail::projectContainsEntityId (project, secondSegmentId))
        return ProjectEditStatus::DuplicateEntityId;

    if (! firstTakeId.isValid() || ! secondTakeId.isValid())
        return ProjectEditStatus::InvalidRecordingTakeId;

    const RecordingTake* const firstTake = detail::findRecordingTake (project, firstTakeId);
    const RecordingTake* const secondTake = detail::findRecordingTake (project, secondTakeId);
    if (firstTake == nullptr || secondTake == nullptr)
        return ProjectEditStatus::RecordingTakeNotFound;

    const ProjectRecordingCompSegment first {
        firstSegmentId,
        firstTakeId,
        firstTimelineStart,
        firstTimelineLength,
        firstSourceOffset
    };
    const ProjectRecordingCompSegment second {
        secondSegmentId,
        secondTakeId,
        secondTimelineStart,
        secondTimelineLength,
        secondSourceOffset
    };

    if (! detail::compSegmentWindowFitsTake (first, *firstTake)
        || ! detail::compSegmentWindowFitsTake (second, *secondTake))
        return ProjectEditStatus::InvalidRecordingCompWindow;

    Project edited = project;
    edited.recordingCompSegments = { first, second };
    if (! edited.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    project = std::move (edited);
    return ProjectEditStatus::Applied;
}

// E33: removes the take, its clip, and every comp segment referencing it in ONE edit, so
// `recordingTakesReferenceProjectRows` and `recordingCompSegmentsReferenceTakes` stay green.
// The take's asset stays (assets may be unreferenced); other takes are untouched.
[[nodiscard]] inline ProjectEditStatus removeRecordingTake (Project& project, EntityId takeId)
{
    if (! project.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    if (! takeId.isValid())
        return ProjectEditStatus::InvalidRecordingTakeId;

    const RecordingTake* const take = detail::findRecordingTake (project, takeId);
    if (take == nullptr)
        return ProjectEditStatus::RecordingTakeNotFound;

    Project edited = project;
    const EntityId clipId = take->clipId;
    std::erase_if (edited.recordingTakes,
                   [takeId] (const RecordingTake& row) { return row.id == takeId; });
    std::erase_if (edited.clips,
                   [clipId] (const Clip& row) { return row.id == clipId; });
    std::erase_if (edited.recordingCompSegments,
                   [takeId] (const ProjectRecordingCompSegment& row) { return row.takeId == takeId; });
    if (! edited.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    project = std::move (edited);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus addFxInsert (Project& project,
                                                    EntityId ownerId,
                                                    FxInsert insert,
                                                    std::size_t position)
{
    if (! detail::projectCanApplyFxEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! ownerId.isValid())
        return ProjectEditStatus::InvalidFxOwnerId;

    MixerStripState* const strip = detail::findMixerStrip (project, ownerId);
    if (strip == nullptr)
        return ProjectEditStatus::FxOwnerNotFound;

    if (! insert.id.isValid())
        return ProjectEditStatus::InvalidFxInsertId;

    if (! fxKindIsKnown (insert.kind))
        return ProjectEditStatus::InvalidFxKind;

    if (! insert.isValid())
        return ProjectEditStatus::InvalidFxParamValue;

    if (detail::projectContainsEntityId (project, insert.id))
        return ProjectEditStatus::DuplicateEntityId;

    if (position > strip->fxChain.size())
        return ProjectEditStatus::InvalidFxPosition;

    strip->fxChain.insert (strip->fxChain.begin() + static_cast<std::ptrdiff_t> (position), std::move (insert));
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus removeFxInsert (Project& project, EntityId ownerId, EntityId insertId)
{
    if (! detail::projectCanApplyFxEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! ownerId.isValid())
        return ProjectEditStatus::InvalidFxOwnerId;

    if (! insertId.isValid())
        return ProjectEditStatus::InvalidFxInsertId;

    MixerStripState* const strip = detail::findMixerStrip (project, ownerId);
    if (strip == nullptr)
        return ProjectEditStatus::FxOwnerNotFound;

    std::size_t index = 0;
    if (! detail::findFxInsertIndex (*strip, insertId, index))
        return ProjectEditStatus::FxInsertNotFound;

    strip->fxChain.erase (strip->fxChain.begin() + static_cast<std::ptrdiff_t> (index));
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus reorderFxInsert (Project& project,
                                                        EntityId ownerId,
                                                        EntityId insertId,
                                                        std::size_t newPosition)
{
    if (! detail::projectCanApplyFxEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! ownerId.isValid())
        return ProjectEditStatus::InvalidFxOwnerId;

    if (! insertId.isValid())
        return ProjectEditStatus::InvalidFxInsertId;

    MixerStripState* const strip = detail::findMixerStrip (project, ownerId);
    if (strip == nullptr)
        return ProjectEditStatus::FxOwnerNotFound;

    if (newPosition >= strip->fxChain.size())
        return ProjectEditStatus::InvalidFxPosition;

    std::size_t oldPosition = 0;
    if (! detail::findFxInsertIndex (*strip, insertId, oldPosition))
        return ProjectEditStatus::FxInsertNotFound;

    FxInsert insert = std::move (strip->fxChain[oldPosition]);
    strip->fxChain.erase (strip->fxChain.begin() + static_cast<std::ptrdiff_t> (oldPosition));
    strip->fxChain.insert (strip->fxChain.begin() + static_cast<std::ptrdiff_t> (newPosition), std::move (insert));
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setFxInsertEnabled (Project& project,
                                                           EntityId ownerId,
                                                           EntityId insertId,
                                                           bool enabled)
{
    if (! detail::projectCanApplyFxEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! ownerId.isValid())
        return ProjectEditStatus::InvalidFxOwnerId;

    if (! insertId.isValid())
        return ProjectEditStatus::InvalidFxInsertId;

    MixerStripState* const strip = detail::findMixerStrip (project, ownerId);
    if (strip == nullptr)
        return ProjectEditStatus::FxOwnerNotFound;

    FxInsert* const insert = detail::findFxInsert (*strip, insertId);
    if (insert == nullptr)
        return ProjectEditStatus::FxInsertNotFound;

    insert->enabled = enabled;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setFxInsertParam (Project& project,
                                                         EntityId ownerId,
                                                         EntityId insertId,
                                                         std::uint32_t paramId,
                                                         double normalizedValue)
{
    if (! detail::projectCanApplyFxEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! ownerId.isValid())
        return ProjectEditStatus::InvalidFxOwnerId;

    if (! insertId.isValid())
        return ProjectEditStatus::InvalidFxInsertId;

    if (! normalizedFxParamValueIsValid (normalizedValue))
        return ProjectEditStatus::InvalidFxParamValue;

    MixerStripState* const strip = detail::findMixerStrip (project, ownerId);
    if (strip == nullptr)
        return ProjectEditStatus::FxOwnerNotFound;

    FxInsert* const insert = detail::findFxInsert (*strip, insertId);
    if (insert == nullptr)
        return ProjectEditStatus::FxInsertNotFound;

    std::size_t paramIndex = 0;
    if (detail::findNormalizedFxParamIndex (*insert, paramId, paramIndex))
        insert->normalizedParams[paramIndex].second = normalizedValue;
    else
        insert->normalizedParams.push_back ({ paramId, normalizedValue });

    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus addAutomationLane (Project& project, AutomationLaneData lane)
{
    if (! detail::projectCanApplyAutomationEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! lane.id.isValid())
        return ProjectEditStatus::InvalidAutomationLaneId;

    if (! lane.ownerEntity.isValid() || ! automationTargetRoleIsKnown (lane.role))
        return ProjectEditStatus::InvalidAutomationTarget;

    if (! lane.isValid())
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    if (detail::projectContainsEntityId (project, lane.id))
        return ProjectEditStatus::DuplicateEntityId;

    Project edited = project;
    edited.automationLanes.push_back (std::move (lane));
    if (! edited.automationTargetsReferenceProjectRows())
        return ProjectEditStatus::InvalidAutomationTarget;

    project = std::move (edited);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus removeAutomationLane (Project& project, EntityId laneId)
{
    if (! detail::projectCanApplyAutomationEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! laneId.isValid())
        return ProjectEditStatus::InvalidAutomationLaneId;

    std::size_t index = 0;
    if (! detail::findAutomationLaneIndex (project, laneId, index))
        return ProjectEditStatus::AutomationLaneNotFound;

    project.automationLanes.erase (project.automationLanes.begin() + static_cast<std::ptrdiff_t> (index));
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus addAutomationBreakpoint (Project& project,
                                                                EntityId laneId,
                                                                AutomationBreakpoint point)
{
    if (! detail::projectCanApplyAutomationEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! laneId.isValid())
        return ProjectEditStatus::InvalidAutomationLaneId;

    if (! point.isValid())
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    std::size_t laneIndex = 0;
    if (! detail::findAutomationLaneIndex (project, laneId, laneIndex))
        return ProjectEditStatus::AutomationLaneNotFound;

    const AutomationLaneData& lane = project.automationLanes[laneIndex];
    std::size_t existing = 0;
    if (detail::findAutomationBreakpointIndex (lane, point.tick, existing))
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    Project edited = project;
    std::vector<AutomationBreakpoint>& points = edited.automationLanes[laneIndex].points;
    std::size_t insertAt = 0;
    while (insertAt < points.size() && points[insertAt].tick < point.tick)
        ++insertAt;

    points.insert (points.begin() + static_cast<std::ptrdiff_t> (insertAt), point);
    if (! edited.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    project = std::move (edited);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus moveAutomationBreakpoint (Project& project,
                                                                 EntityId laneId,
                                                                 Tick oldTick,
                                                                 Tick newTick)
{
    if (! detail::projectCanApplyAutomationEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! laneId.isValid())
        return ProjectEditStatus::InvalidAutomationLaneId;

    if (oldTick < 0 || newTick < 0)
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    std::size_t laneIndex = 0;
    if (! detail::findAutomationLaneIndex (project, laneId, laneIndex))
        return ProjectEditStatus::AutomationLaneNotFound;

    const AutomationLaneData& lane = project.automationLanes[laneIndex];
    std::size_t pointIndex = 0;
    if (! detail::findAutomationBreakpointIndex (lane, oldTick, pointIndex))
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    std::size_t duplicateIndex = 0;
    if (oldTick != newTick && detail::findAutomationBreakpointIndex (lane, newTick, duplicateIndex))
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    AutomationBreakpoint moved = lane.points[pointIndex];
    moved.tick = newTick;

    Project edited = project;
    std::vector<AutomationBreakpoint>& points = edited.automationLanes[laneIndex].points;
    points.erase (points.begin() + static_cast<std::ptrdiff_t> (pointIndex));

    std::size_t insertAt = 0;
    while (insertAt < points.size() && points[insertAt].tick < moved.tick)
        ++insertAt;

    points.insert (points.begin() + static_cast<std::ptrdiff_t> (insertAt), moved);
    if (! edited.hasValidAssetClipIndirection())
        return ProjectEditStatus::InvalidProject;

    project = std::move (edited);
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setAutomationBreakpointValue (Project& project,
                                                                     EntityId laneId,
                                                                     Tick tick,
                                                                     double value)
{
    if (! detail::projectCanApplyAutomationEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! laneId.isValid())
        return ProjectEditStatus::InvalidAutomationLaneId;

    if (! automationBreakpointValueIsValid (value))
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    AutomationLaneData* const lane = detail::findAutomationLane (project, laneId);
    if (lane == nullptr)
        return ProjectEditStatus::AutomationLaneNotFound;

    std::size_t pointIndex = 0;
    if (! detail::findAutomationBreakpointIndex (*lane, tick, pointIndex))
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    lane->points[pointIndex].value = value;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus setAutomationBreakpointCurve (Project& project,
                                                                     EntityId laneId,
                                                                     Tick tick,
                                                                     AutomationCurveType curve)
{
    if (! detail::projectCanApplyAutomationEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! laneId.isValid())
        return ProjectEditStatus::InvalidAutomationLaneId;

    if (! automationCurveIsStorageSafe (curve))
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    AutomationLaneData* const lane = detail::findAutomationLane (project, laneId);
    if (lane == nullptr)
        return ProjectEditStatus::AutomationLaneNotFound;

    std::size_t pointIndex = 0;
    if (! detail::findAutomationBreakpointIndex (*lane, tick, pointIndex))
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    lane->points[pointIndex].curveType = curve;
    return ProjectEditStatus::Applied;
}

[[nodiscard]] inline ProjectEditStatus removeAutomationBreakpoint (Project& project,
                                                                   EntityId laneId,
                                                                   Tick tick)
{
    if (! detail::projectCanApplyAutomationEdit (project))
        return ProjectEditStatus::InvalidProject;

    if (! laneId.isValid())
        return ProjectEditStatus::InvalidAutomationLaneId;

    AutomationLaneData* const lane = detail::findAutomationLane (project, laneId);
    if (lane == nullptr)
        return ProjectEditStatus::AutomationLaneNotFound;

    std::size_t pointIndex = 0;
    if (! detail::findAutomationBreakpointIndex (*lane, tick, pointIndex))
        return ProjectEditStatus::InvalidAutomationBreakpoint;

    lane->points.erase (lane->points.begin() + static_cast<std::ptrdiff_t> (pointIndex));
    return ProjectEditStatus::Applied;
}

} // namespace yesdaw::engine
