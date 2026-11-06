#pragma once

#include "MusicalAudioChannelPlan.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace yesdaw::engine::waveform_private_swap_stored_channels {

enum class MusicalAudioChannelMaterializationStatus : std::uint8_t
{
    Ok, InvalidInput, UnsupportedPolicy, UnsupportedLayout,
    InvalidPhysicalGeometry, NonFinitePhysical, ResourceLimit
};

// Declared new allocation, retained input capacity and owner-overlap subtotal.
// Legacy allocator overhead and full source/DSP/cache resources remain unqualified.
struct MusicalAudioChannelMaterializationAccounting
{
    std::uint64_t planReservedBytes = 0, physicalReservedBytes = 0;
    std::uint64_t objectRequestedBytes = 0, sampleRequestedBytes = 0;
    std::uint64_t reservedBytes = 0, constructionPeakReservedBytes = 0;
    std::uint64_t constructionWorkUnits = 0;
    std::uint64_t framesMaterialized = 0, physicalSamplesVisited = 0, logicalSamplesConstructed = 0;
};

namespace musical_channel_materialization_detail {

[[nodiscard]] inline bool validLayout (std::uint16_t logical, std::uint16_t physical,
                                       std::span<const std::uint16_t> projection) noexcept
{
    if (projection.size() != logical) return false;
    if (logical == 1)
        return physical == 1 && projection[0] == 0;
    if (logical != 2) return false;
    return projection[0] == 0
        && ((physical == 1 && projection[1] == 0) || (physical == 2 && projection[1] == 1));
}

} // namespace musical_channel_materialization_detail

class MusicalAudioChannelMaterialization;

[[nodiscard]] inline MusicalAudioChannelMaterializationStatus materializeMusicalAudioChannels (
    const std::shared_ptr<const MusicalAudioChannelPlan>&, const std::shared_ptr<const AssetSamples>&,
    MusicalAudioLimits, std::unique_ptr<const MusicalAudioChannelMaterialization>&);

// Private immutable WORKING PCM, never a Ready or quality certificate.
// Binding arbitrary physical PCM to a plan proves no DSP/source/context/envelope provenance.
class MusicalAudioChannelMaterialization final
{
public:
    ~MusicalAudioChannelMaterialization() noexcept
    {
        // Every constructed float is trivially destructible, including a partially built result.
        if (samples_ != nullptr) ::operator delete (static_cast<void*> (samples_));
    }
    MusicalAudioChannelMaterialization (const MusicalAudioChannelMaterialization&) = delete;
    MusicalAudioChannelMaterialization (MusicalAudioChannelMaterialization&&) = delete;
    MusicalAudioChannelMaterialization& operator= (const MusicalAudioChannelMaterialization&) = delete;
    MusicalAudioChannelMaterialization& operator= (MusicalAudioChannelMaterialization&&) = delete;

    [[nodiscard]] const std::shared_ptr<const MusicalAudioChannelPlan>& plan() const noexcept { return plan_; }
    [[nodiscard]] const std::shared_ptr<const AssetSamples>& physicalOwner() const noexcept { return physical_; }
    [[nodiscard]] std::span<const float> samples() const noexcept
    { return { samples_, static_cast<std::size_t> (accounting_.logicalSamplesConstructed) }; }
    [[nodiscard]] std::uint16_t logicalChannels() const noexcept { return plan_->logicalChannels(); }
    [[nodiscard]] std::uint16_t physicalChannels() const noexcept { return plan_->physicalChannels(); }
    [[nodiscard]] std::uint64_t frameCount() const noexcept { return accounting_.framesMaterialized; }
    [[nodiscard]] const MusicalAudioChannelMaterializationAccounting& accounting() const noexcept { return accounting_; }

private:
    friend MusicalAudioChannelMaterializationStatus materializeMusicalAudioChannels (
        const std::shared_ptr<const MusicalAudioChannelPlan>&, const std::shared_ptr<const AssetSamples>&,
        MusicalAudioLimits, std::unique_ptr<const MusicalAudioChannelMaterialization>&);
    MusicalAudioChannelMaterialization() = default;
    std::shared_ptr<const MusicalAudioChannelPlan> plan_;
    std::shared_ptr<const AssetSamples> physical_;
    MusicalAudioChannelMaterializationAccounting accounting_;
    float* samples_ = nullptr;
};

[[nodiscard]] inline MusicalAudioChannelMaterializationStatus materializeMusicalAudioChannels (
    const std::shared_ptr<const MusicalAudioChannelPlan>& plan, const std::shared_ptr<const AssetSamples>& physical,
    MusicalAudioLimits requested, std::unique_ptr<const MusicalAudioChannelMaterialization>& out)
{
    // Either borrowed handle may refer to a member of the previous out.
    const auto retainedPlan = plan;
    const auto retainedPhysical = physical;
    const auto previous = std::move (out);
    using Status = MusicalAudioChannelMaterializationStatus;
    if (! retainedPlan || ! retainedPhysical) return Status::InvalidInput;
    const auto& request = retainedPlan->request();
    if (! request || ! request->window().valid() || request->window().frameCount() <= 0)
        return Status::InvalidInput;
    if (! musical_channel_detail::knownPolicy (request->policy())) return Status::UnsupportedPolicy;
    const auto logical = retainedPlan->logicalChannels();
    const auto physicalChannels = retainedPlan->physicalChannels();
    const auto projection = retainedPlan->projection();
    if (! musical_channel_materialization_detail::validLayout (logical, physicalChannels, projection))
        return Status::UnsupportedLayout;

    const auto frames = static_cast<std::uint64_t> (request->window().frameCount());
    if (! musical_audio_detail::validOwner (retainedPhysical, frames, physicalChannels))
        return Status::InvalidPhysicalGeometry;
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    constexpr auto sizeMaximum = std::numeric_limits<std::size_t>::max();
    if (frames > maximum / logical || frames > sizeMaximum / logical) return Status::ResourceLimit;
    const auto logicalSamples = frames * logical;
    if (logicalSamples > maximum / sizeof (float) || logicalSamples > sizeMaximum / sizeof (float)
        || retainedPhysical->interleaved.capacity() > maximum / sizeof (float))
        return Status::ResourceLimit;

    const auto limits = musical_channel_detail::effectiveLimits (
        requested, request->window().clock()->limits(), request->limits());
    MusicalAudioChannelMaterializationAccounting accounting;
    accounting.planReservedBytes = retainedPlan->accounting().reservedBytes;
    // capacity is an input payload operand, not a portable allocator/control-block bound.
    accounting.physicalReservedBytes = sizeof (AssetSamples) + 64u;
    const auto physicalCapacityBytes = static_cast<std::uint64_t> (retainedPhysical->interleaved.capacity())
                                     * sizeof (float);
    if (! musical_channel_detail::addWithin (accounting.physicalReservedBytes, physicalCapacityBytes,
                                             limits.maxReservedBytes))
        return Status::ResourceLimit;
    accounting.objectRequestedBytes = sizeof (MusicalAudioChannelMaterialization);
    accounting.sampleRequestedBytes = logicalSamples * sizeof (float);
    if (! musical_channel_detail::addWithin (accounting.reservedBytes, accounting.planReservedBytes,
                                             limits.maxReservedBytes)
        || ! musical_channel_detail::addWithin (accounting.reservedBytes, accounting.physicalReservedBytes,
                                                limits.maxReservedBytes)
        || ! musical_channel_detail::addWithin (accounting.reservedBytes, accounting.objectRequestedBytes,
                                                limits.maxReservedBytes)
        || ! musical_channel_detail::addWithin (accounting.reservedBytes, accounting.sampleRequestedBytes,
                                                limits.maxReservedBytes))
        return Status::ResourceLimit;

    accounting.constructionPeakReservedBytes = accounting.reservedBytes;
    if (previous)
    {
        auto retainedBytes = previous->accounting().reservedBytes;
        std::uint64_t sharedGraphBytes = 0;
        if (musical_channel_detail::sameOwner (previous->plan(), retainedPlan))
            sharedGraphBytes = accounting.planReservedBytes;
        else if (musical_channel_detail::sameOwner (previous->plan()->request(), request))
            sharedGraphBytes = request->accounting().reservedBytes;
        else if (musical_channel_detail::sameOwner (previous->plan()->request()->window().clock(),
                                                    request->window().clock()))
            sharedGraphBytes = retainedPlan->accounting().clockReservedBytes;
        if (sharedGraphBytes > retainedBytes) return Status::ResourceLimit;
        retainedBytes -= sharedGraphBytes;
        if (musical_channel_detail::sameOwner (previous->physicalOwner(), retainedPhysical))
        {
            if (accounting.physicalReservedBytes > retainedBytes) return Status::ResourceLimit;
            retainedBytes -= accounting.physicalReservedBytes;
        }
        if (! musical_channel_detail::addWithin (accounting.constructionPeakReservedBytes, retainedBytes,
                                                 limits.maxReservedBytes))
            return Status::ResourceLimit;
    }
    accounting.constructionWorkUnits = retainedPlan->accounting().constructionWorkUnits;
    if (! musical_channel_detail::addWithin (accounting.constructionWorkUnits, 1024u, limits.maxWorkUnits))
        return Status::ResourceLimit;
    const std::uint64_t perFrame = 8u + 8u * physicalChannels + 8u * logical;
    if (frames > (limits.maxWorkUnits - accounting.constructionWorkUnits) / perFrame)
        return Status::ResourceLimit;
    accounting.constructionWorkUnits += frames * perFrame;

    static_assert (sizeof (float) == sizeof (std::uint32_t) && std::numeric_limits<float>::is_iec559
                   && std::numeric_limits<float>::digits == 24 && std::is_trivially_destructible_v<float>);
    static_assert (alignof (float) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__
                   && alignof (MusicalAudioChannelMaterialization) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__);
    try
    {
        auto working = std::unique_ptr<MusicalAudioChannelMaterialization> (new MusicalAudioChannelMaterialization);
        working->plan_ = retainedPlan;
        working->physical_ = retainedPhysical;
        // C++20 explicit operator new implicitly creates a suitable float array in this region.
        // construct_at starts each float lifetime; scalar delete matches this requested-byte allocation.
        working->samples_ = static_cast<float*> (::operator new (
            static_cast<std::size_t> (accounting.sampleRequestedBytes)));
        std::array<std::uint32_t, 2> bits {};
        for (std::uint64_t frame = 0; frame < frames; ++frame)
        {
            const auto physicalBase = static_cast<std::size_t> (frame) * physicalChannels;
            for (std::uint16_t channel = 0; channel < physicalChannels; ++channel)
            {
                bits[channel] = std::bit_cast<std::uint32_t> (retainedPhysical->interleaved[physicalBase + channel]);
                ++accounting.physicalSamplesVisited;
                if ((bits[channel] & 0x7f800000u) == 0x7f800000u) return Status::NonFinitePhysical;
            }
            const auto logicalBase = static_cast<std::size_t> (frame) * logical;
            for (std::uint16_t channel = 0; channel < logical; ++channel)
            {
                std::construct_at (working->samples_ + logicalBase + channel,
                                   std::bit_cast<float> (bits[projection[physicalChannels == 2 ? 1 - channel : channel]]));
                ++accounting.logicalSamplesConstructed;
            }
            ++accounting.framesMaterialized;
        }
        working->accounting_ = accounting;
        out = std::move (working);
        return Status::Ok;
    }
    catch (const std::bad_alloc&) { return Status::ResourceLimit; }
}

} // namespace yesdaw::engine
