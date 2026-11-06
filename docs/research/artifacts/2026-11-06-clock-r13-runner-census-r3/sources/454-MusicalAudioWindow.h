#pragma once

#include "engine/RateMatchedView.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace yesdaw::engine {

enum class MusicalAudioStatus : std::uint8_t
{
    Ok, InvalidInput, InvalidClock, NumericDomain, Overflow, BoundaryAmbiguous,
    WindowCollapsed, InvalidOwnerWindow, ResourceLimit
};

inline constexpr std::uint64_t kMusicalAudioMaxReservedBytes = 512ull * 1024 * 1024;
inline constexpr std::uint64_t kMusicalAudioMaxWorkUnits = std::uint64_t { 1 } << 28;

struct MusicalAudioLimits
{
    std::uint64_t maxReservedBytes = kMusicalAudioMaxReservedBytes;
    std::uint64_t maxWorkUnits = kMusicalAudioMaxWorkUnits;
};

struct MusicalAudioPosition
{
    Tick whole = 0;
    std::uint64_t numerator = 0, denominator = 1;
    friend bool operator== (const MusicalAudioPosition&, const MusicalAudioPosition&) = default;
};

struct MusicalAudioFramePosition
{
    double low = 0.0, high = 0.0;
    std::int64_t absoluteFrame = 0;
    friend bool operator== (const MusicalAudioFramePosition&, const MusicalAudioFramePosition&) = default;
};

namespace musical_audio_detail {

[[nodiscard]] inline MusicalAudioStatus statusFor (TimeLookupStatus status) noexcept
{
    switch (status)
    {
        case TimeLookupStatus::Ok: return MusicalAudioStatus::Ok;
        case TimeLookupStatus::InvalidInput: return MusicalAudioStatus::InvalidInput;
        case TimeLookupStatus::InvalidMap: return MusicalAudioStatus::InvalidClock;
        case TimeLookupStatus::Overflow: return MusicalAudioStatus::Overflow;
        case TimeLookupStatus::NumericDomain: return MusicalAudioStatus::NumericDomain;
    }
    return MusicalAudioStatus::NumericDomain;
}

[[nodiscard]] inline bool nextFractionBit (std::uint64_t& remainder, std::uint64_t denominator) noexcept
{
    if (remainder >= denominator - remainder)
    {
        remainder -= denominator - remainder;
        return true;
    }
    remainder += remainder;
    return false;
}

// Division of a potentially 127-bit product. Each prefix quotient is bounded by
// that prefix of length, and each remainder stays below denominator.
inline void sourceQuotient (std::uint64_t length, std::uint64_t input,
                            std::uint64_t denominator, std::uint64_t& quotient,
                            std::uint64_t& remainder) noexcept
{
    quotient = remainder = 0;
    for (int bit = 62; bit >= 0; --bit)
    {
        quotient *= 2;
        if (nextFractionBit (remainder, denominator)) ++quotient;
        if ((length & (std::uint64_t { 1 } << bit)) != 0)
        {
            if (remainder >= denominator - input)
            {
                remainder -= denominator - input;
                ++quotient;
            }
            else remainder += input;
        }
    }
}

inline void fractionBounds (std::uint64_t numerator, std::uint64_t denominator,
                             double& low, double& high) noexcept
{
    static_assert (sizeof (double) == sizeof (std::uint64_t)
                   && std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);
    if (numerator == 0) { low = high = 0.0; return; }
    int exponent = -1;
    while (! nextFractionBit (numerator, denominator)) --exponent;
    std::uint64_t significand = std::uint64_t { 1 } << 52;
    for (int bit = 51; bit >= 0; --bit)
        if (nextFractionBit (numerator, denominator)) significand |= std::uint64_t { 1 } << bit;
    low = std::ldexp (static_cast<double> (significand), exponent - 52);
    high = numerator == 0 ? low : std::bit_cast<double> (std::bit_cast<std::uint64_t> (low) + 1);
}

[[nodiscard]] inline bool validOwner (const std::shared_ptr<const AssetSamples>& owner,
                                     std::uint64_t frames, std::uint16_t channels) noexcept
{
    return owner && channels > 0 && owner->channels == static_cast<int> (channels)
        && owner->frames == frames && frames <= std::numeric_limits<std::size_t>::max() / channels
        && owner->interleaved.size() == static_cast<std::size_t> (frames) * channels;
}

[[nodiscard]] inline bool sameOwner (const std::shared_ptr<const AssetSamples>& a,
                                    const std::shared_ptr<const AssetSamples>& b) noexcept
{
    return a.get() == b.get() && ! a.owner_before (b) && ! b.owner_before (a);
}

} // namespace musical_audio_detail

class MusicalAudioClock final
{
public:
    [[nodiscard]] static MusicalAudioStatus build (TempoMapView map, SampleRate rate, MusicalAudioLimits limits,
                                                   std::shared_ptr<const MusicalAudioClock>& out)
    {
        [[maybe_unused]] const auto inputOwner = out;
        out.reset();
        if (! rate.isValid() || (map.count > 0 && map.changes == nullptr)) return MusicalAudioStatus::InvalidClock;
        const auto byteLimit = std::min (limits.maxReservedBytes, kMusicalAudioMaxReservedBytes);
        const auto workLimit = std::min (limits.maxWorkUnits, kMusicalAudioMaxWorkUnits);
        constexpr std::uint64_t baseBytes = sizeof (MusicalAudioClock) + 4 * 64;
        constexpr std::uint64_t rowBytes = 2 * sizeof (TempoChange) + sizeof (double);
        if (byteLimit < baseBytes || map.count > (byteLimit - baseBytes) / rowBytes
            || workLimit < 1 || map.count > (workLimit - 1) / 6)
            return MusicalAudioStatus::ResourceLimit;
        for (std::size_t index = 0; index < map.count; ++index)
            if (! map.changes[index].hasValidBpm() || (index == 0 && map.changes[index].tick != 0)
                || (index > 0 && map.changes[index].tick <= map.changes[index - 1].tick))
                return MusicalAudioStatus::InvalidClock;
        try
        {
            auto clock = std::shared_ptr<MusicalAudioClock> (new MusicalAudioClock);
            clock->rate_ = rate;
            clock->limits_ = { byteLimit, workLimit };
            clock->reservedBytes_ = baseBytes + map.count * rowBytes;
            clock->workUnits_ = 1 + map.count * 6;
            if (map.count > 0) clock->rows_.assign (map.changes, map.changes + map.count);
            if (! CompiledTempoMap::build ({ clock->rows_.data(), clock->rows_.size() }, rate, clock->compiled_))
                return MusicalAudioStatus::NumericDomain;
            out = std::move (clock);
            return MusicalAudioStatus::Ok;
        }
        catch (const std::bad_alloc&) { return MusicalAudioStatus::ResourceLimit; }
    }

    [[nodiscard]] SampleRate sampleRate() const noexcept { return rate_; }
    [[nodiscard]] std::span<const TempoChange> rows() const noexcept { return rows_; }
    [[nodiscard]] MusicalAudioLimits limits() const noexcept { return limits_; }
    [[nodiscard]] std::uint64_t reservedBytes() const noexcept { return reservedBytes_; }
    [[nodiscard]] std::uint64_t constructionWorkUnits() const noexcept { return workUnits_; }

    [[nodiscard]] bool equals (const MusicalAudioClock& other) const noexcept
    {
        if (this == &other) return true;
        return rate_ == other.rate_ && rows_ == other.rows_;
    }

    [[nodiscard]] MusicalAudioStatus frameAt (MusicalAudioPosition position, MusicalAudioFramePosition& out) const noexcept
    {
        out = {};
        if (position.whole < 0 || position.denominator == 0 || position.numerator >= position.denominator)
            return MusicalAudioStatus::InvalidInput;
        if (limits_.maxWorkUnits < 512u + 2u * static_cast<std::uint64_t> (std::bit_width (rows_.size())))
            return MusicalAudioStatus::ResourceLimit;
        double lowFraction = 0.0, highFraction = 0.0;
        musical_audio_detail::fractionBounds (position.numerator, position.denominator, lowFraction, highFraction);
        MusicalTime low { position.whole, lowFraction }, high { position.whole, highFraction };
        if (highFraction == 1.0)
        {
            if (high.tick == std::numeric_limits<Tick>::max()) return MusicalAudioStatus::Overflow;
            ++high.tick;
            high.frac = 0.0;
        }
        double lowFrame = 0.0;
        auto status = compiled_.frameForMusicalTime (low, lowFrame);
        if (status != TimeLookupStatus::Ok) return musical_audio_detail::statusFor (status);
        double highFrame = lowFrame;
        if (high != low)
        {
            status = compiled_.frameForMusicalTime (high, highFrame);
            if (status != TimeLookupStatus::Ok) return musical_audio_detail::statusFor (status);
        }
        if (highFrame < lowFrame || highFrame - lowFrame > 1.0 / 1024.0)
            return MusicalAudioStatus::BoundaryAmbiguous;
        const auto lowFloor = static_cast<std::int64_t> (std::floor (lowFrame));
        const auto highFloor = static_cast<std::int64_t> (std::floor (highFrame));
        if (lowFloor != highFloor) return MusicalAudioStatus::BoundaryAmbiguous;
        out = { lowFrame, highFrame, lowFloor };
        return MusicalAudioStatus::Ok;
    }

private:
    MusicalAudioClock() = default;
    SampleRate rate_;
    MusicalAudioLimits limits_;
    std::vector<TempoChange> rows_;
    CompiledTempoMap compiled_;
    std::uint64_t reservedBytes_ = 0, workUnits_ = 0;
};

class MusicalAudioWindow;
[[nodiscard]] inline MusicalAudioStatus resolveMusicalAudioWindow (
    const std::shared_ptr<const MusicalAudioClock>&, Tick, Tick, std::uint64_t, MusicalAudioWindow&) noexcept;

class MusicalAudioWindow final
{
public:
    [[nodiscard]] bool valid() const noexcept { return clock_ != nullptr; }
    [[nodiscard]] Tick startTick() const noexcept { return start_; }
    [[nodiscard]] Tick lengthTicks() const noexcept { return length_; }
    [[nodiscard]] std::uint64_t inputFrames() const noexcept { return inputFrames_; }
    [[nodiscard]] std::int64_t startFrame() const noexcept { return startFrame_; }
    [[nodiscard]] std::int64_t endFrame() const noexcept { return endFrame_; }
    [[nodiscard]] std::int64_t frameCount() const noexcept { return endFrame_ - startFrame_; }
    [[nodiscard]] const std::shared_ptr<const MusicalAudioClock>& clock() const noexcept { return clock_; }

    [[nodiscard]] MusicalAudioStatus positionAtSource (std::uint64_t input, MusicalAudioPosition& out) const noexcept
    {
        out = {};
        if (! valid()) return MusicalAudioStatus::InvalidClock;
        if (input > inputFrames_) return MusicalAudioStatus::InvalidInput;
        std::uint64_t quotient = 0, remainder = 0;
        musical_audio_detail::sourceQuotient (static_cast<std::uint64_t> (length_), input, inputFrames_, quotient, remainder);
        out = { start_ + static_cast<Tick> (quotient), remainder, remainder == 0 ? 1 : inputFrames_ };
        return MusicalAudioStatus::Ok;
    }

    [[nodiscard]] MusicalAudioStatus frameAtSource (std::uint64_t input, MusicalAudioFramePosition& out) const noexcept
    {
        out = {};
        MusicalAudioPosition position;
        const auto status = positionAtSource (input, position);
        return status == MusicalAudioStatus::Ok ? clock_->frameAt (position, out) : status;
    }

    [[nodiscard]] MusicalAudioStatus cutAtSource (std::uint64_t input, std::int64_t& out) const noexcept
    {
        out = 0;
        MusicalAudioFramePosition frame;
        const auto status = frameAtSource (input, frame);
        if (status != MusicalAudioStatus::Ok) return status;
        if (frame.absoluteFrame < startFrame_ || frame.absoluteFrame > endFrame_)
            return MusicalAudioStatus::NumericDomain;
        out = frame.absoluteFrame - startFrame_;
        return MusicalAudioStatus::Ok;
    }

private:
    friend MusicalAudioStatus resolveMusicalAudioWindow (const std::shared_ptr<const MusicalAudioClock>&,
                                                         Tick, Tick, std::uint64_t, MusicalAudioWindow&) noexcept;
    std::shared_ptr<const MusicalAudioClock> clock_;
    Tick start_ = 0, length_ = 0;
    std::uint64_t inputFrames_ = 0;
    std::int64_t startFrame_ = 0, endFrame_ = 0;
};

[[nodiscard]] inline MusicalAudioStatus resolveMusicalAudioWindow (
    const std::shared_ptr<const MusicalAudioClock>& clock, Tick start, Tick length,
    std::uint64_t inputFrames, MusicalAudioWindow& out) noexcept
{
    const auto retainedClock = clock;
    out = {};
    if (! retainedClock) return MusicalAudioStatus::InvalidClock;
    if (start < 0 || length <= 0 || inputFrames == 0) return MusicalAudioStatus::InvalidInput;
    if (length > std::numeric_limits<Tick>::max() - start) return MusicalAudioStatus::Overflow;
    MusicalAudioFramePosition a, b;
    auto status = retainedClock->frameAt ({ start, 0, 1 }, a);
    if (status != MusicalAudioStatus::Ok) return status;
    status = retainedClock->frameAt ({ start + length, 0, 1 }, b);
    if (status != MusicalAudioStatus::Ok) return status;
    if (b.absoluteFrame <= a.absoluteFrame) return MusicalAudioStatus::WindowCollapsed;
    out.clock_ = retainedClock;
    out.start_ = start;
    out.length_ = length;
    out.inputFrames_ = inputFrames;
    out.startFrame_ = a.absoluteFrame;
    out.endFrame_ = b.absoluteFrame;
    return MusicalAudioStatus::Ok;
}

struct MusicalAudioPreparationIdentity
{
    std::uint32_t plannerRevision = 0, envelopeRevision = 0, qualityRevision = 0;
    std::array<int, 3> libraryVersion {};
    friend bool operator== (const MusicalAudioPreparationIdentity&, const MusicalAudioPreparationIdentity&) = default;
};

class MusicalAudioKey;
[[nodiscard]] inline MusicalAudioStatus makeMusicalAudioKey (
    const Clip&, const Asset&, const std::shared_ptr<const AssetSamples>&, const std::shared_ptr<const AssetSamples>&,
    ViewWindow, ResampleQuality, const MusicalAudioWindow&, MusicalAudioPreparationIdentity,
    std::shared_ptr<const MusicalAudioKey>&);

class MusicalAudioKey final
{
public:
    [[nodiscard]] ViewWindow originalWindow() const noexcept { return originalWindow_; }
    [[nodiscard]] ViewWindow mappedWindow() const noexcept { return mappedWindow_; }

    [[nodiscard]] bool operator== (const MusicalAudioKey& other) const noexcept
    {
        return clip_.id == other.clip_.id && asset_ == other.asset_
            && musical_audio_detail::sameOwner (original_, other.original_)
            && musical_audio_detail::sameOwner (mapped_, other.mapped_)
            && originalWindow_.srcOffset == other.originalWindow_.srcOffset && originalWindow_.srcLen == other.originalWindow_.srcLen
            && mappedWindow_.srcOffset == other.mappedWindow_.srcOffset && mappedWindow_.srcLen == other.mappedWindow_.srcLen
            && tier_ == other.tier_ && window_.startTick() == other.window_.startTick()
            && window_.lengthTicks() == other.window_.lengthTicks() && window_.clock()->equals (*other.window_.clock())
            && window_.startFrame() == other.window_.startFrame() && window_.endFrame() == other.window_.endFrame()
            && clip_.reversed == other.clip_.reversed && clip_.fadeIn == other.clip_.fadeIn && clip_.fadeOut == other.clip_.fadeOut
            && clip_.fadeInShape == other.clip_.fadeInShape && clip_.fadeOutShape == other.clip_.fadeOutShape
            && clip_.fadeInCurve == other.clip_.fadeInCurve && clip_.fadeOutCurve == other.clip_.fadeOutCurve
            && identity_ == other.identity_;
    }

private:
    friend MusicalAudioStatus makeMusicalAudioKey (const Clip&, const Asset&, const std::shared_ptr<const AssetSamples>&,
        const std::shared_ptr<const AssetSamples>&, ViewWindow, ResampleQuality, const MusicalAudioWindow&,
        MusicalAudioPreparationIdentity, std::shared_ptr<const MusicalAudioKey>&);
    MusicalAudioKey() = default;
    Clip clip_;
    Asset asset_;
    std::shared_ptr<const AssetSamples> original_, mapped_;
    ViewWindow originalWindow_, mappedWindow_;
    ResampleQuality tier_ = ResampleQuality::LivePlayback;
    MusicalAudioWindow window_;
    MusicalAudioPreparationIdentity identity_;
};

[[nodiscard]] inline MusicalAudioStatus makeMusicalAudioKey (
    const Clip& clip, const Asset& asset, const std::shared_ptr<const AssetSamples>& original,
    const std::shared_ptr<const AssetSamples>& mapped, ViewWindow mappedWindow, ResampleQuality tier,
    const MusicalAudioWindow& window, MusicalAudioPreparationIdentity identity, std::shared_ptr<const MusicalAudioKey>& out)
{
    out.reset();
    if (! window.valid() || ! clip.id.isValid() || clip.timeBase != TimeBase::TempoLocked
        || clip.timelineStart != window.startTick() || clip.timelineLength != window.lengthTicks()
        || clip.fadeIn < 0 || clip.fadeOut < 0
        || static_cast<unsigned> (clip.fadeInShape) > static_cast<unsigned> (FadeShape::Log)
        || static_cast<unsigned> (clip.fadeOutShape) > static_cast<unsigned> (FadeShape::Log)
        || ! std::isfinite (clip.fadeInCurve) || std::abs (clip.fadeInCurve) > 1.0f
        || ! std::isfinite (clip.fadeOutCurve) || std::abs (clip.fadeOutCurve) > 1.0f
        || (tier != ResampleQuality::LivePlayback && tier != ResampleQuality::OfflineRender)
        || identity.plannerRevision == 0 || identity.envelopeRevision == 0 || identity.qualityRevision == 0)
        return MusicalAudioStatus::InvalidInput;
    if (! asset.isValid() || clip.srcLen == 0 || ! clip.sourceWindowFits (asset)
        || ! musical_audio_detail::validOwner (original, asset.frames, asset.channels) || ! mapped
        || ! musical_audio_detail::validOwner (mapped, mapped->frames, asset.channels))
        return MusicalAudioStatus::InvalidOwnerWindow;
    const double projectRate = window.clock()->sampleRate().hz;
    const double ratio = rateRatio (asset.sampleRate.hz, projectRate);
    const double total = static_cast<double> (asset.frames) * projectRate / asset.sampleRate.hz;
    if (! std::isfinite (ratio) || ratio <= 0.0 || ! std::isfinite (total) || total < 0.0 || total >= 0x1p63)
        return MusicalAudioStatus::Overflow;
    if (mapped->frames != resampledFrameCount (asset.frames, asset.sampleRate.hz, projectRate))
        return MusicalAudioStatus::InvalidOwnerWindow;
    const auto safeEnd = [ratio] (std::uint64_t value) {
        const double projected = static_cast<double> (value) * ratio;
        return ratio == 1.0 || (std::isfinite (projected) && projected >= 0.0 && projected < 0x1p63);
    };
    if (! safeEnd (clip.srcOffset) || ! safeEnd (clip.srcOffset + clip.srcLen)) return MusicalAudioStatus::Overflow;
    const auto expected = mapWindowToView (clip.srcOffset, clip.srcLen, ratio, mapped->frames);
    if (expected.srcOffset != mappedWindow.srcOffset || expected.srcLen != mappedWindow.srcLen
        || mappedWindow.srcLen == 0 || mappedWindow.srcLen != window.inputFrames())
        return MusicalAudioStatus::InvalidOwnerWindow;
    constexpr std::uint64_t keyBytes = sizeof (MusicalAudioKey) + 64;
    if (keyBytes > window.clock()->limits().maxReservedBytes
        || window.clock()->reservedBytes() > window.clock()->limits().maxReservedBytes - keyBytes)
        return MusicalAudioStatus::ResourceLimit;
    try
    {
        auto key = std::shared_ptr<MusicalAudioKey> (new MusicalAudioKey);
        key->clip_ = clip;
        key->asset_ = asset;
        key->original_ = original;
        key->mapped_ = mapped;
        key->originalWindow_ = { clip.srcOffset, clip.srcLen };
        key->mappedWindow_ = mappedWindow;
        key->tier_ = tier;
        key->window_ = window;
        key->identity_ = identity;
        out = std::move (key);
        return MusicalAudioStatus::Ok;
    }
    catch (const std::bad_alloc&) { return MusicalAudioStatus::ResourceLimit; }
}

} // namespace yesdaw::engine
