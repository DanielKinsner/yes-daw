#pragma once

#include "engine/MusicalAudioWindow.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <utility>

namespace yesdaw::engine::waveform_private_swap_stored_channels {

enum class MusicalAudioChannelStatus : std::uint8_t
{
    Ok, InvalidInput, GeometryRefused, UnsupportedLayout, UnsupportedPolicy,
    NonFiniteSource, ResourceLimit
};

struct MusicalAudioChannelResult
{
    MusicalAudioChannelStatus status = MusicalAudioChannelStatus::Ok;
    MusicalAudioStatus geometryStatus = MusicalAudioStatus::Ok;
};

struct MusicalAudioChannelPolicy
{
    std::uint32_t channelRevision = 1, materializationRevision = 1;
    friend bool operator== (const MusicalAudioChannelPolicy&, const MusicalAudioChannelPolicy&) = default;
};

// Declared object/clock/scan subtotal; complete wrapper allocation and DSP work remain separate.
struct MusicalAudioChannelAccounting
{
    std::uint64_t clockReservedBytes = 0, keyReservedBytes = 0;
    std::uint64_t requestReservedBytes = 0, planReservedBytes = 0;
    std::uint64_t reservedBytes = 0, constructionPeakReservedBytes = 0;
    std::uint64_t constructionWorkUnits = 0;
    std::uint64_t framesVisited = 0, samplesVisited = 0;
};

namespace musical_channel_detail {

[[nodiscard]] inline bool knownPolicy (MusicalAudioChannelPolicy policy) noexcept
{ return policy.channelRevision == 1 && policy.materializationRevision == 1; }

template<class T>
[[nodiscard]] inline bool sameOwner (const std::shared_ptr<const T>& a,
                                    const std::shared_ptr<const T>& b) noexcept
{ return a.get() == b.get() && ! a.owner_before (b) && ! b.owner_before (a); }

[[nodiscard]] inline MusicalAudioLimits effectiveLimits (MusicalAudioLimits requested,
                                                        MusicalAudioLimits clock,
                                                        MusicalAudioLimits inherited = {}) noexcept
{
    return { std::min ({ requested.maxReservedBytes, clock.maxReservedBytes, inherited.maxReservedBytes,
                        kMusicalAudioMaxReservedBytes }),
             std::min ({ requested.maxWorkUnits, clock.maxWorkUnits, inherited.maxWorkUnits,
                        kMusicalAudioMaxWorkUnits }) };
}

[[nodiscard]] inline bool addWithin (std::uint64_t& total, std::uint64_t amount, std::uint64_t limit) noexcept
{
    if (total > limit || amount > limit - total) return false;
    total += amount;
    return true;
}

} // namespace musical_channel_detail

class MusicalAudioChannelRequest;
class MusicalAudioChannelPlan;

[[nodiscard]] inline MusicalAudioChannelResult makeMusicalAudioChannelRequest (
    const Clip&, const Asset&, const std::shared_ptr<const AssetSamples>&,
    const std::shared_ptr<const AssetSamples>&, ViewWindow, ResampleQuality,
    const MusicalAudioWindow&, MusicalAudioPreparationIdentity, MusicalAudioChannelPolicy,
    MusicalAudioLimits, std::shared_ptr<const MusicalAudioChannelRequest>&);

[[nodiscard]] inline MusicalAudioChannelResult buildMusicalAudioChannelPlan (
    const std::shared_ptr<const MusicalAudioChannelRequest>&, MusicalAudioLimits,
    std::shared_ptr<const MusicalAudioChannelPlan>&);

class MusicalAudioChannelRequest final
{
public:
    ~MusicalAudioChannelRequest() = default;
    MusicalAudioChannelRequest (const MusicalAudioChannelRequest&) = delete;
    MusicalAudioChannelRequest (MusicalAudioChannelRequest&&) = delete;
    MusicalAudioChannelRequest& operator= (const MusicalAudioChannelRequest&) = delete;
    MusicalAudioChannelRequest& operator= (MusicalAudioChannelRequest&&) = delete;

    [[nodiscard]] const std::shared_ptr<const MusicalAudioKey>& key() const noexcept { return key_; }
    [[nodiscard]] const std::shared_ptr<const AssetSamples>& originalOwner() const noexcept { return original_; }
    [[nodiscard]] const std::shared_ptr<const AssetSamples>& mappedOwner() const noexcept { return mapped_; }
    [[nodiscard]] const MusicalAudioWindow& window() const noexcept { return window_; }
    [[nodiscard]] ViewWindow mappedWindow() const noexcept { return mappedWindow_; }
    [[nodiscard]] MusicalAudioChannelPolicy policy() const noexcept { return policy_; }
    [[nodiscard]] std::uint16_t logicalChannels() const noexcept { return logicalChannels_; }
    [[nodiscard]] bool reversed() const noexcept { return reversed_; }
    [[nodiscard]] MusicalAudioLimits limits() const noexcept { return limits_; }
    [[nodiscard]] const MusicalAudioChannelAccounting& accounting() const noexcept { return accounting_; }

    [[nodiscard]] bool operator== (const MusicalAudioChannelRequest& other) const noexcept
    { return policy_ == other.policy_ && *key_ == *other.key_; }

private:
    friend MusicalAudioChannelResult makeMusicalAudioChannelRequest (
        const Clip&, const Asset&, const std::shared_ptr<const AssetSamples>&,
        const std::shared_ptr<const AssetSamples>&, ViewWindow, ResampleQuality,
        const MusicalAudioWindow&, MusicalAudioPreparationIdentity, MusicalAudioChannelPolicy,
        MusicalAudioLimits, std::shared_ptr<const MusicalAudioChannelRequest>&);
    MusicalAudioChannelRequest() = default;
    std::shared_ptr<const MusicalAudioKey> key_;
    std::shared_ptr<const AssetSamples> original_, mapped_;
    MusicalAudioWindow window_;
    ViewWindow mappedWindow_;
    MusicalAudioChannelPolicy policy_;
    MusicalAudioLimits limits_;
    MusicalAudioChannelAccounting accounting_;
    std::uint16_t logicalChannels_ = 0;
    bool reversed_ = false;
};

class MusicalAudioChannelPlan final
{
public:
    ~MusicalAudioChannelPlan() = default;
    MusicalAudioChannelPlan (const MusicalAudioChannelPlan&) = delete;
    MusicalAudioChannelPlan (MusicalAudioChannelPlan&&) = delete;
    MusicalAudioChannelPlan& operator= (const MusicalAudioChannelPlan&) = delete;
    MusicalAudioChannelPlan& operator= (MusicalAudioChannelPlan&&) = delete;

    [[nodiscard]] const std::shared_ptr<const MusicalAudioChannelRequest>& request() const noexcept { return request_; }
    [[nodiscard]] std::uint16_t logicalChannels() const noexcept { return request_->logicalChannels(); }
    [[nodiscard]] std::uint16_t physicalChannels() const noexcept { return physicalChannels_; }
    [[nodiscard]] std::span<const std::uint16_t> projection() const noexcept
    { return { projection_.data(), logicalChannels() }; }
    [[nodiscard]] const MusicalAudioChannelAccounting& accounting() const noexcept { return accounting_; }

    // Identity binding only. Adoption separately checks current resource ceilings; no source scan occurs here.
    [[nodiscard]] bool matches (const std::shared_ptr<const MusicalAudioChannelRequest>& candidate) const noexcept
    {
        return candidate && musical_channel_detail::knownPolicy (candidate->policy())
            && musical_channel_detail::knownPolicy (request_->policy()) && *request_ == *candidate;
    }

    [[nodiscard]] MusicalAudioChannelStatus physicalSample (std::uint16_t physicalChannel,
                                                           std::uint64_t inputFrame, float& out) const noexcept
    {
        out = 0.0f;
        const auto selected = request_->mappedWindow();
        if (physicalChannel >= physicalChannels_ || inputFrame >= selected.srcLen)
            return MusicalAudioChannelStatus::InvalidInput;
        const auto local = request_->reversed() ? selected.srcLen - 1 - inputFrame : inputFrame;
        const auto frame = static_cast<std::size_t> (selected.srcOffset + local);
        const auto index = frame * logicalChannels() + physicalChannel;
        out = request_->mappedOwner()->interleaved[index];
        return MusicalAudioChannelStatus::Ok;
    }

private:
    friend MusicalAudioChannelResult buildMusicalAudioChannelPlan (
        const std::shared_ptr<const MusicalAudioChannelRequest>&, MusicalAudioLimits,
        std::shared_ptr<const MusicalAudioChannelPlan>&);
    MusicalAudioChannelPlan() = default;
    std::shared_ptr<const MusicalAudioChannelRequest> request_;
    MusicalAudioChannelAccounting accounting_;
    std::array<std::uint16_t, 2> projection_ {};
    std::uint16_t physicalChannels_ = 0;
};

[[nodiscard]] inline MusicalAudioChannelResult makeMusicalAudioChannelRequest (
    const Clip& clip, const Asset& asset, const std::shared_ptr<const AssetSamples>& original,
    const std::shared_ptr<const AssetSamples>& mapped, ViewWindow mappedWindow, ResampleQuality tier,
    const MusicalAudioWindow& window, MusicalAudioPreparationIdentity identity, MusicalAudioChannelPolicy policy,
    MusicalAudioLimits requested, std::shared_ptr<const MusicalAudioChannelRequest>& out)
{
    const auto previous = out;
    const auto retainedClip = clip;
    const auto retainedAsset = asset;
    const auto retainedOriginal = original;
    const auto retainedMapped = mapped;
    const auto retainedWindow = window;
    out.reset();
    using Status = MusicalAudioChannelStatus;
    if (! musical_channel_detail::knownPolicy (policy)) return { Status::UnsupportedPolicy, MusicalAudioStatus::Ok };
    if (retainedAsset.channels != 1 && retainedAsset.channels != 2)
        return { Status::UnsupportedLayout, MusicalAudioStatus::Ok };
    if (! retainedWindow.valid()) return { Status::GeometryRefused, MusicalAudioStatus::InvalidClock };

    const auto limits = musical_channel_detail::effectiveLimits (requested, retainedWindow.clock()->limits());
    MusicalAudioChannelAccounting accounting;
    accounting.clockReservedBytes = retainedWindow.clock()->reservedBytes();
    accounting.keyReservedBytes = sizeof (MusicalAudioKey) + 64u;
    accounting.requestReservedBytes = sizeof (MusicalAudioChannelRequest) + 64u;
    if (! musical_channel_detail::addWithin (accounting.reservedBytes, accounting.clockReservedBytes, limits.maxReservedBytes)
        || ! musical_channel_detail::addWithin (accounting.reservedBytes, accounting.keyReservedBytes, limits.maxReservedBytes)
        || ! musical_channel_detail::addWithin (accounting.reservedBytes, accounting.requestReservedBytes, limits.maxReservedBytes))
        return { Status::ResourceLimit, MusicalAudioStatus::Ok };
    accounting.constructionPeakReservedBytes = accounting.reservedBytes;
    if (previous)
    {
        auto retainedBytes = previous->accounting().reservedBytes;
        if (musical_channel_detail::sameOwner (previous->window().clock(), retainedWindow.clock()))
        {
            if (accounting.clockReservedBytes > retainedBytes) return { Status::ResourceLimit, MusicalAudioStatus::Ok };
            retainedBytes -= accounting.clockReservedBytes;
        }
        if (! musical_channel_detail::addWithin (accounting.constructionPeakReservedBytes, retainedBytes, limits.maxReservedBytes))
            return { Status::ResourceLimit, MusicalAudioStatus::Ok };
    }
    accounting.constructionWorkUnits = retainedWindow.clock()->constructionWorkUnits();
    if (! musical_channel_detail::addWithin (accounting.constructionWorkUnits, 4096, limits.maxWorkUnits))
        return { Status::ResourceLimit, MusicalAudioStatus::Ok };

    try
    {
        std::shared_ptr<const MusicalAudioKey> key;
        const auto geometry = makeMusicalAudioKey (retainedClip, retainedAsset, retainedOriginal, retainedMapped,
                                                   mappedWindow, tier, retainedWindow, identity, key);
        if (geometry != MusicalAudioStatus::Ok) return { Status::GeometryRefused, geometry };
        auto request = std::shared_ptr<MusicalAudioChannelRequest> (new MusicalAudioChannelRequest);
        request->key_ = std::move (key);
        request->original_ = retainedOriginal;
        request->mapped_ = retainedMapped;
        request->window_ = retainedWindow;
        request->mappedWindow_ = mappedWindow;
        request->policy_ = policy;
        request->limits_ = limits;
        request->accounting_ = accounting;
        request->logicalChannels_ = retainedAsset.channels;
        request->reversed_ = retainedClip.reversed;
        out = std::move (request);
        return {};
    }
    catch (const std::bad_alloc&) { return { Status::ResourceLimit, MusicalAudioStatus::Ok }; }
}

[[nodiscard]] inline MusicalAudioChannelResult buildMusicalAudioChannelPlan (
    const std::shared_ptr<const MusicalAudioChannelRequest>& request, MusicalAudioLimits requested,
    std::shared_ptr<const MusicalAudioChannelPlan>& out)
{
    const auto retained = request;
    const auto previous = out;
    out.reset();
    using Status = MusicalAudioChannelStatus;
    if (! retained) return { Status::InvalidInput, MusicalAudioStatus::Ok };
    if (! musical_channel_detail::knownPolicy (retained->policy()))
        return { Status::UnsupportedPolicy, MusicalAudioStatus::Ok };
    const auto channels = retained->logicalChannels();
    if (channels != 1 && channels != 2) return { Status::UnsupportedLayout, MusicalAudioStatus::Ok };
    if (! retained->window().valid()) return { Status::GeometryRefused, MusicalAudioStatus::InvalidClock };
    const auto& owner = retained->mappedOwner();
    const auto selected = retained->mappedWindow();
    if (! owner || ! musical_audio_detail::validOwner (owner, owner->frames, channels) || selected.srcLen == 0
        || selected.srcOffset > owner->frames || selected.srcLen > owner->frames - selected.srcOffset
        || selected.srcLen != retained->window().inputFrames())
        return { Status::GeometryRefused, MusicalAudioStatus::InvalidOwnerWindow };

    const auto limits = musical_channel_detail::effectiveLimits (requested, retained->window().clock()->limits(), retained->limits());
    auto accounting = retained->accounting();
    accounting.planReservedBytes = sizeof (MusicalAudioChannelPlan) + 64u;
    if (! musical_channel_detail::addWithin (accounting.reservedBytes, accounting.planReservedBytes, limits.maxReservedBytes))
        return { Status::ResourceLimit, MusicalAudioStatus::Ok };
    accounting.constructionPeakReservedBytes = accounting.reservedBytes;
    if (previous)
    {
        auto retainedBytes = previous->accounting().reservedBytes;
        const bool sharedRequest = musical_channel_detail::sameOwner (previous->request(), retained);
        const bool sharedClock = musical_channel_detail::sameOwner (previous->request()->window().clock(), retained->window().clock());
        const auto sharedBytes = sharedRequest ? retained->accounting().reservedBytes : sharedClock ? accounting.clockReservedBytes : 0;
        if (sharedBytes > retainedBytes) return { Status::ResourceLimit, MusicalAudioStatus::Ok };
        retainedBytes -= sharedBytes;
        if (! musical_channel_detail::addWithin (accounting.constructionPeakReservedBytes, retainedBytes, limits.maxReservedBytes))
            return { Status::ResourceLimit, MusicalAudioStatus::Ok };
    }
    if (! musical_channel_detail::addWithin (accounting.constructionWorkUnits, 1024, limits.maxWorkUnits))
        return { Status::ResourceLimit, MusicalAudioStatus::Ok };
    const std::uint64_t perFrame = 8u + 16u * channels;
    if (selected.srcLen > (limits.maxWorkUnits - accounting.constructionWorkUnits) / perFrame)
        return { Status::ResourceLimit, MusicalAudioStatus::Ok };
    accounting.constructionWorkUnits += selected.srcLen * perFrame;

    static_assert (sizeof (float) == sizeof (std::uint32_t) && std::numeric_limits<float>::is_iec559
                   && std::numeric_limits<float>::digits == 24);
    bool equivalent = true;
    for (std::uint64_t i = 0; i < selected.srcLen; ++i)
    {
        const auto local = retained->reversed() ? selected.srcLen - 1 - i : i;
        const auto frame = static_cast<std::size_t> (selected.srcOffset + local);
        const auto index = frame * channels;
        const auto left = std::bit_cast<std::uint32_t> (owner->interleaved[index]);
        ++accounting.samplesVisited;
        if ((left & 0x7f800000u) == 0x7f800000u) return { Status::NonFiniteSource, MusicalAudioStatus::Ok };
        if (channels == 2)
        {
            const auto right = std::bit_cast<std::uint32_t> (owner->interleaved[index + 1]);
            ++accounting.samplesVisited;
            if ((right & 0x7f800000u) == 0x7f800000u) return { Status::NonFiniteSource, MusicalAudioStatus::Ok };
            if (left != right && ((left & 0x7fffffffu) != 0 || (right & 0x7fffffffu) != 0)) equivalent = false;
        }
        ++accounting.framesVisited;
    }
    try
    {
        auto plan = std::shared_ptr<MusicalAudioChannelPlan> (new MusicalAudioChannelPlan);
        plan->request_ = retained;
        plan->accounting_ = accounting;
        plan->physicalChannels_ = channels == 1 || equivalent ? std::uint16_t { 1 } : std::uint16_t { 2 };
        plan->projection_ = plan->physicalChannels_ == 1 ? std::array<std::uint16_t, 2> { 0, 0 }
                                                       : std::array<std::uint16_t, 2> { 0, 1 };
        out = std::move (plan);
        return {};
    }
    catch (const std::bad_alloc&) { return { Status::ResourceLimit, MusicalAudioStatus::Ok }; }
}

} // namespace yesdaw::engine
