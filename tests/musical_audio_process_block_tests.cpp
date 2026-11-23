#include "engine/MusicalAudioProcessBlock.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <memory>
#include <span>

TEST_CASE ("A Musical stretch call distinguishes copied quiet input from synthesized output",
           "[musical-audio][adr0083]")
{
    using namespace yesdaw::engine;
    namespace dsp = musical_process_detail;
    const std::array<TempoChange, 1> map {{ { 0, 120.0 } }};
    std::shared_ptr<const MusicalAudioClock> clock;
    REQUIRE (MusicalAudioClock::build ({ map.data(), map.size() }, { 48000.0 }, {}, clock) == MusicalAudioStatus::Ok);
    Clip clip;
    clip.id = EntityId::fromBigEndianParts (1, 1);
    clip.assetId = EntityId::fromBigEndianParts (1, 2);
    clip.trackId = EntityId::fromBigEndianParts (1, 3);
    clip.timeBase = TimeBase::TempoLocked;
    clip.timelineLength = 4;
    clip.srcOffset = 1;
    clip.srcLen = 6;
    clip.reversed = true;
    MusicalAudioWindow window;
    REQUIRE (resolveMusicalAudioWindow (clock, 0, clip.timelineLength, clip.srcLen, window)
             == MusicalAudioStatus::Ok);
    Asset asset;
    asset.id = clip.assetId;
    asset.contentHash.bytes[0] = 1;
    asset.frames = 8;
    asset.channels = 1;
    asset.sampleRate = { 48000.0 };
    auto samples = std::make_shared<AssetSamples>();
    samples->channels = 1;
    samples->frames = 8;
    const float tiny = std::bit_cast<float> (std::uint32_t { 1 });
    samples->interleaved = { 0.9f, -0.0f, 0.0f, 0.5f, tiny, 0.0f, -0.0f, 0.9f };
    std::shared_ptr<const MusicalAudioChannelRequest> request;
    REQUIRE (makeMusicalAudioChannelRequest (clip, asset, samples, samples, { 1, 6 },
        ResampleQuality::LivePlayback, window, { 1, 1, 1, { 1, 1, 0 } }, {}, {}, request).status
        == MusicalAudioChannelStatus::Ok);
    std::shared_ptr<const MusicalAudioChannelPlan> plan;
    REQUIRE (buildMusicalAudioChannelPlan (request, {}, plan).status == MusicalAudioChannelStatus::Ok);

    dsp::Stretcher stretch (0);
    stretch.presetDefault (1, 48000.0f);
    stretch.reset();
    std::array<float, 3> working {};
    using enum dsp::BlockStatus;
    // Reversed selected source starts at -0, +0. The real quiet path wraps the two frames.
    REQUIRE (dsp::processBlock (stretch, *plan, 0, 2, 3, working) == ExactZeroFastReturn);
    CHECK (std::bit_cast<std::uint32_t> (working[2]) == 0x80000000u);
    SECTION ("subnormal on the fast path refuses, including when a shorter output omits it")
    {
        REQUIRE (dsp::processBlock (stretch, *plan, 1, 2, 1, std::span (working).first (1))
                 == MusicalQuietPathUnsupported);
        CHECK (working[0] == 0.0f);
    }
    SECTION ("a different DSP channel count refuses within the working block")
    {
        stretch.presetDefault (2, 48000.0f);
        stretch.reset();
        REQUIRE (dsp::processBlock (stretch, *plan, 0, 2, 3, working) == InvalidBlock);
    }
    SECTION ("source signal uses the spectral path and trailing context is zero")
    {
        REQUIRE (dsp::processBlock (stretch, *plan, 3, 1, 3, working) == Processed);
        std::uint64_t signalEnd = 0;
        dsp::Input tail { *plan, 6, signalEnd };
        CHECK (tail[0][0].value == 0.0f);
        CHECK (signalEnd == 0);
    }
}
