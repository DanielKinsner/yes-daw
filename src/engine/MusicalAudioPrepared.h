// Musical audio for the opt-in listening build. Preparation is control-side only.
#pragma once

#include "engine/MusicalAudioReady.h"
#include "engine/MusicalAudioEnvelope.h"
#include "engine/MusicalAudioSequence.h"

#include <bit>
#include <stdexcept>

namespace yesdaw::engine {

enum class ListeningPrepareStatus
{
    Ok, InvalidInput, GeometryRefused, ChannelRefused, CutRefused, ConfigurationUnsupported,
    ResourceLimit, MusicalQuietPathUnsupported, MusicalBoundaryContextUnsupported, EnvelopeRefused
};

inline constexpr MusicalAudioPreparationIdentity listeningIdentity { 1, 1, 1, { 1, 1, 0 } };

// All newly owned application PCM and both planners share the per-Clip allowance. Internal
// allocations of Signalsmith 1.1.0 are not observable via its API; full-library admission is pending.
inline ListeningPrepareStatus prepareListeningClip (const Clip& clip, const Asset& asset,
    const std::shared_ptr<const AssetSamples>& original, const std::shared_ptr<const AssetSamples>& mapped,
    const std::shared_ptr<const MusicalAudioClock>& clock,
    MusicalAudioLimits limits, ListeningClipAudio& out)
{
    using Status = ListeningPrepareStatus;
    using namespace musical_process_detail;
    if (out.request_ || out.samples_ || !clock || !original || !mapped || !asset.isValid() || !clip.sourceWindowFits (asset)
        || clip.timeBase != TimeBase::TempoLocked)
        return Status::InvalidInput;
    const double rate = clock->sampleRate().hz;
    if (!asset.sampleRate.isValid() || !std::isfinite (rate) || rate > (std::numeric_limits<float>::max)())
        return Status::ConfigurationUnsupported;
    const double ratio = rateRatio (asset.sampleRate.hz, rate);
    const double total = static_cast<double> (asset.frames) * rate / asset.sampleRate.hz;
    const double last = static_cast<double> (clip.srcOffset + clip.srcLen) * ratio;
    if (!std::isfinite (ratio) || ratio <= 0 || !std::isfinite (total) || total >= 0x1p63
        || !std::isfinite (last) || last >= 0x1p63) return Status::GeometryRefused;
    const auto view = mapWindowToView (clip.srcOffset, clip.srcLen, ratio, mapped->frames);
    MusicalAudioWindow window;
    if (resolveMusicalAudioWindow (clock, clip.timelineStart, clip.timelineLength, view.srcLen, window)
        != MusicalAudioStatus::Ok) return Status::GeometryRefused;
    const double presetRate = static_cast<float> (rate);
    const double rawBlock = presetRate * 0.12, rawHop = presetRate * 0.03;
    constexpr auto intMax = (std::numeric_limits<int>::max)();
    if (rawHop < 1 || rawBlock >= intMax || asset.channels == 0 || asset.channels > 2)
        return Status::ConfigurationUnsupported;
    const int block = static_cast<int> (rawBlock), hop = static_cast<int> (rawHop);
    const auto history = static_cast<std::uint64_t> (block) + static_cast<unsigned> (hop) + 1;
    if (history > static_cast<std::uint64_t> (intMax) || block > (intMax - hop) / 2
        || window.inputFrames() > static_cast<std::uint64_t> (intMax) - history
        || window.frameCount() > intMax - block) return Status::ConfigurationUnsupported;

    try
    {
        std::shared_ptr<const MusicalAudioChannelRequest> request;
        const auto captured = makeMusicalAudioChannelRequest (clip, asset, original, mapped, view,
            ResampleQuality::LivePlayback, window, listeningIdentity, {}, limits, request);
        if (captured.status == MusicalAudioChannelStatus::ResourceLimit) return Status::ResourceLimit;
        if (captured.status != MusicalAudioChannelStatus::Ok) return Status::ChannelRefused;
        std::shared_ptr<const MusicalAudioChannelPlan> channels;
        const auto classified = buildMusicalAudioChannelPlan (request, limits, channels);
        if (classified.status == MusicalAudioChannelStatus::ResourceLimit) return Status::ResourceLimit;
        if (classified.status != MusicalAudioChannelStatus::Ok) return Status::ChannelRefused;
        const auto physical = channels->physicalChannels();
        const auto logical = channels->logicalChannels();
        const auto frames = static_cast<std::uint64_t> (window.frameCount());
        const auto scratchCount = (frames + static_cast<unsigned> (block)) * physical;
        const auto finalCount = frames * logical;
        const auto ring = std::bit_ceil (history * physical);
        if (ring > static_cast<std::uint64_t> (intMax)) return Status::ConfigurationUnsupported;
        const auto fft = signalsmith::spectral::WindowedFFT<float>::fastSizeAbove (block);
        if (fft < block || fft > intMax / physical) return Status::ConfigurationUnsupported;

        // Subtract the channel plan, the known input/output rings and working/final PCM before
        // asking the cut planner to allocate. The two planners share the exact same clock.
        limits = request->limits();
        const auto& used = channels->accounting();
        const auto workingBytes = (scratchCount + finalCount + 2 * ring) * sizeof (float);
        const auto rows = clock->rows();
        const bool faded = clip.fadeIn != 0 || clip.fadeOut != 0;
        const auto inverseBytes = faded ? rows.size_bytes() + rows.size() * sizeof (double) : 0;
        const auto bytes = inverseBytes + used.reservedBytes - clock->reservedBytes() + workingBytes;
        const auto work = used.constructionWorkUnits - clock->constructionWorkUnits() + scratchCount + finalCount;
        if (bytes >= limits.maxReservedBytes || work >= limits.maxWorkUnits) return Status::ResourceLimit;
        std::shared_ptr<const MusicalAudioCutPlan> cuts;
        const auto cut = buildMusicalAudioCutPlan (window, {},
            { limits.maxReservedBytes - bytes, limits.maxWorkUnits - work }, cuts);
        if (cut.status == MusicalAudioCutStatus::ResourceLimit) return Status::ResourceLimit;
        if (cut.status != MusicalAudioCutStatus::Ok) return Status::CutRefused;

        Stretcher stretch (0);
        stretch.presetDefault (physical, static_cast<float> (rate));
        if (stretch.blockSamples() != block || stretch.intervalSamples() != hop)
            return Status::ConfigurationUnsupported;
        std::vector<float> scratch (static_cast<std::size_t> (scratchCount));
        const auto sequence = runSequence (stretch, *channels, *cuts, scratch);
        if (sequence.status == SequenceStatus::MusicalQuietPathUnsupported) return Status::MusicalQuietPathUnsupported;
        if (sequence.status == SequenceStatus::GeometryRefused) return Status::GeometryRefused;
        if (sequence.status == SequenceStatus::MusicalBoundaryContextUnsupported)
            return Status::MusicalBoundaryContextUnsupported;
        if (sequence.status != SequenceStatus::Ok) return Status::ConfigurationUnsupported;

        auto pcm = std::make_shared<AssetSamples>();
        pcm->frames = frames;
        pcm->channels = logical;
        pcm->interleaved.resize (static_cast<std::size_t> (finalCount));
        const auto projection = channels->projection();
        for (std::size_t i = 0; i < frames; ++i)
            for (unsigned c = 0; c < logical; ++c)
                pcm->interleaved[i * logical + c] = sequence.retained[i * physical + projection[c]];
        CompiledTempoMap inverse;
        if (faded && !CompiledTempoMap::build ({ rows.data(), rows.size() }, clock->sampleRate(), inverse))
            return Status::EnvelopeRefused;
        if (applyMusicalAudioEnvelope (clip, window, inverse, logical, pcm->interleaved) != MusicalEnvelopeStatus::Ok)
            return Status::EnvelopeRefused;
        out.clip_ = clip;
        out.asset_ = asset;
        out.request_ = request;
        out.samples_ = std::move (pcm);
        out.retainedBytes_ = request->accounting().reservedBytes + finalCount * sizeof (float);
        return Status::Ok;
    }
    catch (const std::bad_alloc&) { return Status::ResourceLimit; }
    catch (const std::length_error&) { return Status::ResourceLimit; }
}

} // namespace yesdaw::engine
