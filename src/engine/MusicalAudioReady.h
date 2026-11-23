#pragma once

#include "engine/MusicalAudioChannelPlan.h"

namespace yesdaw::engine {

enum class ListeningPrepareStatus;
class ListeningClipAudio
{
public:
    [[nodiscard]] const auto& request() const noexcept { return request_; }
    [[nodiscard]] const auto& samples() const noexcept { return samples_; }
    [[nodiscard]] const Clip& clip() const noexcept { return clip_; }
    [[nodiscard]] const Asset& asset() const noexcept { return asset_; }
    [[nodiscard]] std::uint64_t retainedBytes() const noexcept { return retainedBytes_; }

private:
    friend ListeningPrepareStatus prepareListeningClip (const Clip&, const Asset&,
        const std::shared_ptr<const AssetSamples>&, const std::shared_ptr<const AssetSamples>&,
        const std::shared_ptr<const MusicalAudioClock>&, MusicalAudioLimits, ListeningClipAudio&);
    std::shared_ptr<const MusicalAudioChannelRequest> request_;
    std::shared_ptr<const AssetSamples> samples_;
    Clip clip_;
    Asset asset_;
    std::uint64_t retainedBytes_ = 0;
};

using ListeningClipOwners = std::vector<ListeningClipAudio>;

} // namespace yesdaw::engine
