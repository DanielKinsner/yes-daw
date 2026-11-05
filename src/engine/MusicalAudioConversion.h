#pragma once

#include "engine/MusicalAudioWindow.h"
#include "engine/MusicalClick.h"

namespace yesdaw::engine {

enum class MusicalAudioConversionStatus : std::uint8_t
{
    Ok, NoChange, InvalidInput, InvalidProject, InvalidClock, ClipNotFound, WrongTimeBase,
    InvalidSourceTempo, InvalidRange, LinkedTakeUnsupported, InversePrecision,
    Precision, Overflow, WindowCollapsed, BoundaryAmbiguous, ResourceLimit
};

enum class MusicalAudioConversionKind : std::uint8_t
{
    FixedToMusical, FitSourceTempo, FitMusicalRange, FitFrameRange, MusicalToFixedOriginalSpeed
};

struct MusicalAudioConversionIntent
{
    MusicalAudioConversionKind kind = MusicalAudioConversionKind::FixedToMusical;
    double sourceBpm = 120.0;
    Tick start = 0, end = 0;

    [[nodiscard]] static constexpr MusicalAudioConversionIntent fixedToMusical() noexcept { return {}; }
    [[nodiscard]] static constexpr MusicalAudioConversionIntent fitSourceTempo (double bpm) noexcept
    { return { MusicalAudioConversionKind::FitSourceTempo, bpm }; }
    [[nodiscard]] static constexpr MusicalAudioConversionIntent fitMusicalRange (Tick first, Tick last) noexcept
    { return { MusicalAudioConversionKind::FitMusicalRange, 120.0, first, last }; }
    [[nodiscard]] static constexpr MusicalAudioConversionIntent fitFrameRange (std::int64_t first, std::int64_t last) noexcept
    { return { MusicalAudioConversionKind::FitFrameRange, 120.0, first, last }; }
    [[nodiscard]] static constexpr MusicalAudioConversionIntent musicalToFixedOriginalSpeed() noexcept
    { return { MusicalAudioConversionKind::MusicalToFixedOriginalSpeed }; }
};

struct MusicalAudioClipProposal
{
    Clip before, after;
    std::int64_t resolvedStartFrame = 0, resolvedEndFrame = 0;
};

namespace musical_audio_conversion_detail {

using Status = MusicalAudioConversionStatus;

[[nodiscard]] inline Status statusFor (MusicalAudioStatus status) noexcept
{
    switch (status)
    {
        case MusicalAudioStatus::Ok: return Status::Ok;
        case MusicalAudioStatus::InvalidInput:
        case MusicalAudioStatus::InvalidOwnerWindow: return Status::InvalidInput;
        case MusicalAudioStatus::InvalidClock: return Status::InvalidClock;
        case MusicalAudioStatus::NumericDomain: return Status::Precision;
        case MusicalAudioStatus::Overflow: return Status::Overflow;
        case MusicalAudioStatus::BoundaryAmbiguous: return Status::BoundaryAmbiguous;
        case MusicalAudioStatus::WindowCollapsed: return Status::WindowCollapsed;
        case MusicalAudioStatus::ResourceLimit: return Status::ResourceLimit;
    }
    return Status::Precision;
}

[[nodiscard]] inline Status statusFor (TimeLookupStatus status) noexcept
{
    switch (status)
    {
        case TimeLookupStatus::Ok: return Status::Ok;
        case TimeLookupStatus::InvalidInput: return Status::InvalidInput;
        case TimeLookupStatus::InvalidMap: return Status::InvalidClock;
        case TimeLookupStatus::Overflow: return Status::Overflow;
        case TimeLookupStatus::NumericDomain: return Status::InversePrecision;
    }
    return Status::InversePrecision;
}

[[nodiscard]] inline bool positiveWindow (Tick start, Tick length) noexcept
{
    return start >= 0 && length > 0 && length <= std::numeric_limits<Tick>::max() - start;
}

[[nodiscard]] inline Status nearest (click_detail::Enclosure value, Tick& out) noexcept
{
    out = 0;
    if (value.numericFailure || value.low < 0.0 || ! std::isfinite (value.low)
        || ! std::isfinite (value.high) || value.high < value.low)
        return Status::Precision;
    if (value.high >= 0x1p63) return Status::Overflow;
    const auto rounded = [] (double point, Tick& result) {
        const double whole = std::floor (point);
        result = static_cast<Tick> (whole);
        if (point - whole >= 0.5)
        {
            if (result == std::numeric_limits<Tick>::max()) return false;
            ++result;
        }
        return true;
    };
    Tick low = 0, high = 0;
    if (! rounded (value.low, low) || ! rounded (value.high, high)) return Status::Overflow;
    if (low != high) return Status::Precision;
    out = low;
    return Status::Ok;
}

[[nodiscard]] inline Status scaleFade (Tick fade, Tick oldLength, Tick newLength, Tick& out) noexcept
{
    out = 0;
    if (fade < 0 || oldLength <= 0 || newLength <= 0) return Status::InvalidInput;
    if (fade == 0 || oldLength == newLength) { out = fade; return Status::Ok; }
    const auto denominator = static_cast<std::uint64_t> (oldLength);
    const auto destination = static_cast<std::uint64_t> (newLength);
    const auto amount = static_cast<std::uint64_t> (fade);
    const auto maximum = static_cast<std::uint64_t> (std::numeric_limits<Tick>::max());
    const auto wholeScale = destination / denominator;
    if (wholeScale != 0 && amount > maximum / wholeScale) return Status::Overflow;
    std::uint64_t quotient = 0, remainder = 0;
    musical_audio_detail::sourceQuotient (amount, destination % denominator, denominator, quotient, remainder);
    const auto base = amount * wholeScale;
    if (quotient > maximum - base) return Status::Overflow;
    auto result = base + quotient;
    if (remainder >= denominator - remainder)
    {
        if (result == maximum) return Status::Overflow;
        ++result;
    }
    out = static_cast<Tick> (result);
    return Status::Ok;
}

[[nodiscard]] inline Status prepareInverse (const MusicalAudioClock& clock, MusicalClickMap& out)
{
    const auto rows = clock.rows();
    const auto count = std::max<std::uint64_t> (1, rows.size());
    // Includes a second compiled map, tempo rows, geometric join-barrier capacity,
    // the default Meter index and temporary sorting/query work; no PCM is prepared.
    constexpr std::uint64_t fixedBytes = sizeof (MusicalClickMap) + 4096;
    constexpr std::uint64_t bytesPerRow = 256;
    const auto limits = clock.limits();
    if (clock.reservedBytes() > limits.maxReservedBytes
        || fixedBytes > limits.maxReservedBytes - clock.reservedBytes()
        || count > (limits.maxReservedBytes - clock.reservedBytes() - fixedBytes) / bytesPerRow)
        return Status::ResourceLimit;
    const std::uint64_t workPerRow = 64 + 16 * std::bit_width (count);
    constexpr std::uint64_t fixedWork = 2048;
    if (clock.constructionWorkUnits() > limits.maxWorkUnits
        || fixedWork > limits.maxWorkUnits - clock.constructionWorkUnits()
        || count > (limits.maxWorkUnits - clock.constructionWorkUnits() - fixedWork) / workPerRow)
        return Status::ResourceLimit;
    try
    {
        const auto status = MusicalClickMap::build ({ rows.data(), rows.size() }, {}, clock.sampleRate(), out);
        return statusFor (status);
    }
    catch (const std::bad_alloc&) { return Status::ResourceLimit; }
}

} // namespace musical_audio_conversion_detail

[[nodiscard]] inline MusicalAudioConversionStatus proposeMusicalAudioConversion (
    const Project& project, const std::shared_ptr<const MusicalAudioClock>& clock,
    EntityId clipId, MusicalAudioConversionIntent intent, MusicalAudioClipProposal& out)
{
    using namespace musical_audio_conversion_detail;
    out = {};
    if (! clock || clock->sampleRate() != project.sampleRate || clock->rows().size() != project.tempoMap.size())
        return Status::InvalidClock;
    for (std::size_t index = 0; index < project.tempoMap.size(); ++index)
        if (clock->rows()[index] != project.tempoMap[index]) return Status::InvalidClock;
    if (! detail::projectCanApplyClipEdit (project)) return Status::InvalidProject;
    const Clip* selected = nullptr;
    for (const auto& clip : project.clips)
        if (clip.id == clipId) { selected = &clip; break; }
    if (selected == nullptr) return Status::ClipNotFound;
    for (const auto& take : project.recordingTakes)
        if (take.clipId == clipId) return Status::LinkedTakeUnsupported;
    const Clip before = *selected;
    const auto* asset = project.findAsset (before.assetId);
    if (asset == nullptr || before.srcLen == 0 || ! before.sourceWindowFits (*asset)
        || ! positiveWindow (before.timelineStart, before.timelineLength))
        return Status::InvalidInput;

    MusicalClickMap inverse;
    bool inverseReady = false;
    const auto invertFrame = [&] (std::int64_t frame, Tick& tick) {
        tick = 0;
        if (frame < 0) return Status::InvalidRange;
        if (! inverseReady)
        {
            const auto status = prepareInverse (*clock, inverse);
            if (status != Status::Ok) return status;
            inverseReady = true;
        }
        const auto status = inverse.tickAtFramePosition (click_detail::integer (static_cast<std::uint64_t> (frame)), tick);
        return statusFor (status);
    };
    Clip after = before;
    after.stretchFactor = 1.0f;
    Status status = Status::Ok;
    switch (intent.kind)
    {
        case MusicalAudioConversionKind::FixedToMusical:
        {
            if (before.timeBase != TimeBase::SampleLocked) return Status::WrongTimeBase;
            Tick end = 0;
            status = invertFrame (before.timelineStart, after.timelineStart);
            if (status == Status::Ok) status = invertFrame (before.timelineStart + before.timelineLength, end);
            if (status != Status::Ok) return status;
            if (end <= after.timelineStart) return Status::WindowCollapsed;
            after.timelineLength = end - after.timelineStart;
            after.timeBase = TimeBase::TempoLocked;
            break;
        }
        case MusicalAudioConversionKind::FitSourceTempo:
        {
            if (! std::isfinite (intent.sourceBpm) || intent.sourceBpm <= 0.0) return Status::InvalidSourceTempo;
            auto duration = click_detail::divide (click_detail::integer (before.srcLen),
                { asset->sampleRate.hz, asset->sampleRate.hz, false });
            duration = click_detail::multiply (duration, { intent.sourceBpm, intent.sourceBpm, false });
            duration = click_detail::divide (duration, { 60.0, 60.0, false });
            duration = click_detail::multiply (duration,
                { static_cast<double> (kTicksPerQuarter), static_cast<double> (kTicksPerQuarter), false });
            status = nearest (duration, after.timelineLength);
            if (status != Status::Ok) return status;
            if (after.timelineLength == 0) return Status::WindowCollapsed;
            if (before.timeBase == TimeBase::SampleLocked)
            {
                status = invertFrame (before.timelineStart, after.timelineStart);
                if (status != Status::Ok) return status;
            }
            after.timeBase = TimeBase::TempoLocked;
            break;
        }
        case MusicalAudioConversionKind::FitMusicalRange:
        case MusicalAudioConversionKind::FitFrameRange:
        {
            if (intent.start < 0 || intent.end <= intent.start) return Status::InvalidRange;
            Tick first = intent.start, last = intent.end;
            if (intent.kind == MusicalAudioConversionKind::FitFrameRange)
            {
                status = invertFrame (intent.start, first);
                if (status == Status::Ok) status = invertFrame (intent.end, last);
                if (status != Status::Ok) return status;
            }
            if (last <= first) return Status::WindowCollapsed;
            after.timelineStart = first;
            after.timelineLength = last - first;
            after.timeBase = TimeBase::TempoLocked;
            break;
        }
        case MusicalAudioConversionKind::MusicalToFixedOriginalSpeed:
        {
            if (before.timeBase != TimeBase::TempoLocked) return Status::WrongTimeBase;
            MusicalAudioFramePosition first;
            status = statusFor (clock->frameAt ({ before.timelineStart, 0, 1 }, first));
            if (status != Status::Ok) return status;
            after.timelineStart = first.absoluteFrame;
            if (asset->sampleRate == project.sampleRate)
            {
                if (before.srcLen > static_cast<std::uint64_t> (std::numeric_limits<Tick>::max())) return Status::Overflow;
                after.timelineLength = static_cast<Tick> (before.srcLen);
            }
            else
            {
                const auto natural = click_detail::divide (click_detail::multiply (click_detail::integer (before.srcLen),
                    { project.sampleRate.hz, project.sampleRate.hz, false }),
                    { asset->sampleRate.hz, asset->sampleRate.hz, false });
                status = nearest (natural, after.timelineLength);
                if (status != Status::Ok) return status;
            }
            if (after.timelineLength == 0) return Status::WindowCollapsed;
            after.timeBase = TimeBase::SampleLocked;
            break;
        }
        default: return Status::InvalidInput;
    }
    if (! positiveWindow (after.timelineStart, after.timelineLength)) return Status::Overflow;
    status = scaleFade (before.fadeIn, before.timelineLength, after.timelineLength, after.fadeIn);
    if (status == Status::Ok)
        status = scaleFade (before.fadeOut, before.timelineLength, after.timelineLength, after.fadeOut);
    if (status != Status::Ok) return status;
    if (after == before) return Status::NoChange;

    std::int64_t firstFrame = after.timelineStart, lastFrame = after.timelineStart + after.timelineLength;
    if (after.timeBase == TimeBase::TempoLocked)
    {
        MusicalAudioFramePosition first, last;
        status = statusFor (clock->frameAt ({ after.timelineStart, 0, 1 }, first));
        if (status == Status::Ok)
            status = statusFor (clock->frameAt ({ after.timelineStart + after.timelineLength, 0, 1 }, last));
        if (status != Status::Ok) return status;
        firstFrame = first.absoluteFrame;
        lastFrame = last.absoluteFrame;
        if (lastFrame <= firstFrame) return Status::WindowCollapsed;
    }
    out = { before, after, firstFrame, lastFrame };
    return Status::Ok;
}

} // namespace yesdaw::engine
