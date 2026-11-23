#include "engine/MusicalAudioSequence.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <memory>

TEST_CASE ("The listening song plans one continuous clip across 120 and 90 BPM", "[musical-audio]")
{
    using namespace yesdaw::engine;
    constexpr Tick change = 4 * kTicksPerQuarter;
    const std::array<TempoChange, 2> map {{ { 0, 120.0 }, { change, 90.0 } }};
    std::shared_ptr<const MusicalAudioClock> clock;
    REQUIRE (MusicalAudioClock::build ({ map.data(), map.size() }, { 48000.0 }, {}, clock)
             == MusicalAudioStatus::Ok);
    MusicalAudioWindow window;
    REQUIRE (resolveMusicalAudioWindow (clock, 0, 2 * change, 192000, window) == MusicalAudioStatus::Ok);
    std::shared_ptr<const MusicalAudioCutPlan> plan;
    const auto result = buildMusicalAudioCutPlan (window, {}, {}, plan);
    CAPTURE (static_cast<int> (result.geometryStatus));
    REQUIRE (result.status == MusicalAudioCutStatus::Ok);
    REQUIRE (plan);
    std::uint64_t input = 0;
    std::int64_t output = 0;
    bool splitAtTempo = false;
    for (const auto& call : plan->calls())
    {
        REQUIRE (call.inputBegin == input);
        REQUIRE (call.outputBegin == output);
        splitAtTempo |= input == 96000 && output == 96000;
        input += call.inputFrames;
        output += call.outputFrames;
    }
    CHECK (splitAtTempo);
    CHECK (input == 192000);
    CHECK (output == 224000);

    Clip clip;
    clip.id = EntityId::fromBigEndianParts (2, 1);
    clip.assetId = EntityId::fromBigEndianParts (2, 2);
    clip.trackId = EntityId::fromBigEndianParts (2, 3);
    clip.timeBase = TimeBase::TempoLocked;
    clip.timelineLength = change * 2;
    clip.srcLen = input;
    Asset asset;
    asset.id = clip.assetId;
    asset.contentHash.bytes[0] = 2;
    asset.frames = input;
    asset.channels = 1;
    asset.sampleRate = { 48000.0 };
    auto samples = std::make_shared<AssetSamples>();
    samples->channels = 1;
    samples->frames = static_cast<std::int64_t> (input);
    samples->interleaved.resize (static_cast<std::size_t> (input));
    for (std::size_t i = 0; i < samples->interleaved.size(); ++i)
        samples->interleaved[i] = static_cast<float> (0.125 * std::sin (
            2 * std::numbers::pi * 220.0 * static_cast<double> (i) / 48000.0));
    enum class Signal { Tone, Silent, LateOnset, EndMarker, Beats };
    double frequency = 220.0;
    Signal signal = Signal::Tone;
    SECTION ("pitched clip") {}
    SECTION ("997 Hz clip")
    {
        frequency = 997.0;
        for (std::size_t i = 0; i < samples->interleaved.size(); ++i)
            samples->interleaved[i] = static_cast<float> (0.125 * std::sin (
                2 * std::numbers::pi * frequency * static_cast<double> (i) / 48000.0));
    }
    SECTION ("short drum hits separated by silence") { signal = Signal::Beats; }
    SECTION ("exact zero throughout") { signal = Signal::Silent; }
    SECTION ("silence before the first spectrum") { signal = Signal::LateOnset; }
    SECTION ("a marker at the end leaves synthesis beyond the retained range") { signal = Signal::EndMarker; }
    if (signal == Signal::Silent || signal == Signal::EndMarker || signal == Signal::Beats)
        std::fill (samples->interleaved.begin(), samples->interleaved.end(), 0.0f);
    if (signal == Signal::LateOnset)
        std::fill_n (samples->interleaved.begin(), 24000, 0.0f);
    if (signal == Signal::Beats)
    {
        // Symmetric energy and exactly cancelling sample pairs in a one-millisecond burst.
        for (std::size_t centre = 24000; centre < input; centre += 24000)
            for (std::size_t offset = 0; offset < 24; ++offset)
            {
                const float tap = (offset * 13 % 17 < 8) ? 0.125f : -0.125f;
                samples->interleaved[centre - 1 - offset] = tap;
                samples->interleaved[centre + offset] = -tap;
            }
    }
    if (signal == Signal::EndMarker)
        samples->interleaved.back() = 0.25f;
    std::shared_ptr<const MusicalAudioChannelRequest> request;
    REQUIRE (makeMusicalAudioChannelRequest (clip, asset, samples, samples, { 0, input },
        ResampleQuality::LivePlayback, window, { 1, 1, 1, { 1, 1, 0 } }, {}, {}, request).status
        == MusicalAudioChannelStatus::Ok);
    std::shared_ptr<const MusicalAudioChannelPlan> channels;
    REQUIRE (buildMusicalAudioChannelPlan (request, {}, channels).status == MusicalAudioChannelStatus::Ok);
    namespace dsp = musical_process_detail;
    dsp::Stretcher stretch (0);
    stretch.presetDefault (1, 48000.0f);
    std::vector<float> scratch (static_cast<std::size_t> (output + stretch.blockSamples()));
    const auto sequence = dsp::runSequence (stretch, *channels, *plan, scratch);
    CAPTURE (static_cast<int> (sequence.geometry));
    REQUIRE (sequence.status == dsp::SequenceStatus::Ok);
    CHECK (sequence.retained.data() == scratch.data() + stretch.outputLatency());
    CHECK (sequence.retained.size() == 224000u);
    CHECK (std::all_of (sequence.retained.begin(), sequence.retained.end(),
        [] (float sample) { return std::isfinite (sample); }));
    if (signal == Signal::Tone)
    {
        // Linear-interpolated positive zero crossings; integrate actual sample power in each window.
        auto measure = [&] (std::span<const float> audio, double pitchLimit, double levelLimit)
        {
            double worstPitch = 0.0, worstLevel = 0.0;
            for (std::size_t start = 5760; start + 12000 + 5760 <= audio.size(); start += 2400)
            {
                double energy = 0.0, firstCross = 0.0, lastCross = 0.0;
                int crossings = 0;
                for (std::size_t k = start; k < start + 12000; ++k)
                {
                    double value = audio[k], previous = audio[k - 1];
                    energy += value * value;
                    if (previous < 0.0 && value >= 0.0)
                    {
                        lastCross = static_cast<double> (k - 1) - previous / (value - previous);
                        if (crossings++ == 0) firstCross = lastCross;
                    }
                }
                REQUIRE (crossings >= 2);
                const double measured = (crossings - 1) * 48000.0 / (lastCross - firstCross);
                worstPitch = std::max (worstPitch, std::abs (1200.0 * std::log2 (measured / frequency)));
                worstLevel = std::max (worstLevel, std::abs (10.0 * std::log10 (energy / (12000 * 0.125 * 0.125 / 2))));
            }
            CAPTURE (frequency, worstPitch, worstLevel);
            CHECK (worstPitch <= pitchLimit);
            CHECK (worstLevel <= levelLimit);
        };
        measure (samples->interleaved, 0.25, 0.01);
        measure (sequence.retained, 5.0, 1.0);
        CHECK (std::all_of (sequence.retained.begin(), sequence.retained.end(),
            [] (float sample) { return std::abs (sample) <= 0.125 * 1.25 + 1e-6; }));
    }
    if (signal == Signal::Beats)
    {
        for (int beat = 1; beat < 8; ++beat)
        {
            const int expected = beat <= 4 ? beat * 24000 : 96000 + (beat - 4) * 32000;
            double energy = 0, moment = 0;
            for (int relative = -11520; relative <= 11520; ++relative)
            {
                const double sample = sequence.retained[static_cast<std::size_t> (expected + relative)];
                energy += sample * sample;
                moment += relative * sample * sample;
            }
            REQUIRE (energy > 0);
            CAPTURE (beat, moment, energy);
            CHECK (std::abs (moment / energy) <= 240.0);
        }
    }
    if (signal == Signal::Silent)
        CHECK (std::all_of (sequence.retained.begin(), sequence.retained.end(), [] (float x) { return x == 0; }));
    else if (signal == Signal::EndMarker)
    {
        const auto pastEnd = scratch.begin() + static_cast<std::ptrdiff_t> (output + stretch.outputLatency());
        CHECK (std::any_of (pastEnd, scratch.end(), [] (float x) { return std::abs (x) > 1e-6f; }));
    }
    else if (signal != Signal::Beats)
        CHECK (std::any_of (sequence.retained.begin() + 96000, sequence.retained.end(),
            [] (float sample) { return std::abs (sample) > 0.05f; }));
}
