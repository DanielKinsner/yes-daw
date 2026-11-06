#include "engine/MusicalAudioChannelMaterialization.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

TEST_CASE ("Working Musical channels materialize exact logical samples before adoption",
           "[musical-audio-materialization][adr0084]")
{
    using namespace yesdaw::engine;
    using Word = std::uint32_t;
    using Buffer = std::shared_ptr<const AssetSamples>;
    using Description = std::shared_ptr<const MusicalAudioChannelPlan>;
    using Working = MusicalAudioChannelMaterialization;
    using Handle = std::unique_ptr<const Working>;
    using Refusal = MusicalAudioChannelMaterializationStatus;

    static_assert (! std::is_default_constructible_v<Working>);
    static_assert (! std::is_copy_constructible_v<Working>);
    static_assert (! std::is_move_constructible_v<Working>);
    static_assert (std::is_move_constructible_v<Handle>);
    static_assert (! std::is_convertible_v<const Working&, AssetSamples>);
    static_assert (! std::is_convertible_v<Handle, Buffer>);

    // These are fixture and expectation literals, never samples read from the producer.
    constexpr std::array<Word, 6> monoSource {
        0x3d800000u, 0x00000000u, 0x80000000u, 0x3f000000u, 0xbe800000u, 0xbd800000u
    };
    constexpr std::array<Word, 12> equivalentSource {
        0x3d800000u, 0x3e000000u,
        0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u,
        0x3f000000u, 0x3f000000u, 0xbe800000u, 0xbe800000u,
        0xbd800000u, 0xbe000000u
    };
    constexpr std::array<Word, 12> distinctSource {
        0x3d800000u, 0x3e000000u,
        0x00000000u, 0x80000000u, 0x80000000u, 0x00000000u,
        0x3f000000u, 0x3f000000u, 0xbe800000u, 0xbe800001u,
        0xbd800000u, 0xbe000000u
    };
    constexpr std::array<Word, 4> physicalOne {
        0x3e800000u, 0x80000000u, 0x00000001u, 0xbf000000u
    };
    constexpr std::array<Word, 4> expectedMono {
        0x3e800000u, 0x80000000u, 0x00000001u, 0xbf000000u
    };
    constexpr std::array<Word, 8> expectedEquivalent {
        0x3e800000u, 0x3e800000u, 0x80000000u, 0x80000000u,
        0x00000001u, 0x00000001u, 0xbf000000u, 0xbf000000u
    };
    constexpr std::array<Word, 8> physicalTwo {
        0x3e800000u, 0xbf400000u, 0x80000000u, 0x00000000u,
        0x00000001u, 0x80000001u, 0xbf000000u, 0x3f000000u
    };
    constexpr std::array<Word, 8> expectedDistinct {
        0x3e800000u, 0xbf400000u, 0x80000000u, 0x00000000u,
        0x00000001u, 0x80000001u, 0xbf000000u, 0x3f000000u
    };
    constexpr std::array<Word, 4> normalPhysical {
        0x3e800000u, 0xbf000000u, 0x3f400000u, 0x3f000000u
    };
    constexpr std::array<Word, 8> normalExpected {
        0x3e800000u, 0x3e800000u, 0xbf000000u, 0xbf000000u,
        0x3f400000u, 0x3f400000u, 0x3f000000u, 0x3f000000u
    };
    constexpr std::uint64_t destinationFrames = 4;
    constexpr std::uint64_t monoWork = 6343, equivalentWork = 6439, distinctWork = 6471;
    constexpr std::uint64_t clockBytes = sizeof (MusicalAudioClock) + 4u * 64u
                                     + 2u * sizeof (TempoChange) + sizeof (double);
    constexpr std::uint64_t requestBytes = clockBytes + sizeof (MusicalAudioKey) + 64u
                                       + sizeof (MusicalAudioChannelRequest) + 64u;
    constexpr std::uint64_t descriptionBytes = requestBytes + sizeof (MusicalAudioChannelPlan) + 64u;
    constexpr std::uint64_t workingObjectBytes = sizeof (Working);
    constexpr std::uint64_t monoSampleBytes = 4u * sizeof (float);
    constexpr std::uint64_t stereoSampleBytes = 8u * sizeof (float);

    const auto ownedSamples = [] (std::span<const Word> words, int channels, std::uint64_t frames,
                                  std::size_t reserveSlots = 0) -> Buffer {
        auto storage = std::make_shared<AssetSamples>();
        storage->channels = channels;
        storage->frames = frames;
        storage->interleaved.reserve (std::max (reserveSlots, words.size()));
        for (const auto word : words)
            storage->interleaved.push_back (std::bit_cast<float> (word));
        return storage;
    };
    const auto sameBits = [] (std::span<const float> samples, std::span<const Word> expected) {
        if (samples.size() != expected.size()) return false;
        for (std::size_t index = 0; index < expected.size(); ++index)
            if (std::bit_cast<Word> (samples[index]) != expected[index]) return false;
        return true;
    };
    const auto snapshot = [] (std::span<const float> samples) {
        std::vector<Word> words;
        words.reserve (samples.size());
        for (const auto sample : samples) words.push_back (std::bit_cast<Word> (sample));
        return words;
    };
    const auto newClock = [] (MusicalAudioLimits limits = {}) {
        constexpr std::array<TempoChange, 1> rows {{ { 0, 120.0 } }};
        std::shared_ptr<const MusicalAudioClock> clock;
        REQUIRE (MusicalAudioClock::build ({ rows.data(), rows.size() }, { 48000.0 }, limits, clock)
                 == MusicalAudioStatus::Ok);
        REQUIRE (clock);
        return clock;
    };
    struct Preparation
    {
        Clip clip;
        Asset asset;
        Buffer source;
        MusicalAudioWindow window;
        std::shared_ptr<const MusicalAudioChannelRequest> request;
        Description description;
    };
    const auto compileDescription = [] (const std::shared_ptr<const MusicalAudioChannelRequest>& request) {
        Description description;
        const auto status = buildMusicalAudioChannelPlan (request, {}, description);
        REQUIRE (status.status == MusicalAudioChannelStatus::Ok);
        REQUIRE (status.geometryStatus == MusicalAudioStatus::Ok);
        REQUIRE (description);
        return description;
    };
    const auto preparation = [&] (std::span<const Word> fullSource, std::uint16_t channels,
                                  std::shared_ptr<const MusicalAudioClock> clock = {},
                                  MusicalAudioLimits requestLimits = {}, bool reversed = false) {
        Preparation value;
        value.clip.id = EntityId::fromBigEndianParts (0x8501, 0x101);
        value.clip.assetId = EntityId::fromBigEndianParts (0x8502, 0x102);
        value.clip.trackId = EntityId::fromBigEndianParts (0x8503, 0x103);
        value.clip.timelineStart = 0;
        value.clip.timelineLength = 3;
        value.clip.srcOffset = 1;
        value.clip.srcLen = 4;
        value.clip.timeBase = TimeBase::TempoLocked;
        value.clip.reversed = reversed;
        value.clip.fadeIn = 1;
        value.clip.fadeOut = 1;
        value.clip.gain = 0.25f;
        value.asset.id = value.clip.assetId;
        value.asset.contentHash.bytes[2] = 0x85;
        value.asset.frames = 6;
        value.asset.sampleRate = { 48000.0 };
        value.asset.channels = channels;
        value.source = ownedSamples (fullSource, static_cast<int> (channels), 6);
        if (! clock) clock = newClock();
        REQUIRE (resolveMusicalAudioWindow (clock, 0, 3, 4, value.window) == MusicalAudioStatus::Ok);
        REQUIRE (value.window.startFrame() == 0);
        REQUIRE (value.window.endFrame() == 4);
        REQUIRE (value.window.frameCount() == 4);
        const auto requestStatus = makeMusicalAudioChannelRequest (
            value.clip, value.asset, value.source, value.source, { 1, 4 },
            ResampleQuality::LivePlayback, value.window, { 1, 1, 1, { 1, 1, 0 } }, {},
            requestLimits, value.request);
        REQUIRE (requestStatus.status == MusicalAudioChannelStatus::Ok);
        REQUIRE (requestStatus.geometryStatus == MusicalAudioStatus::Ok);
        REQUIRE (value.request);
        value.description = compileDescription (value.request);
        return value;
    };
    const auto freshWorking = [] (const Description& description, const Buffer& physical) {
        Handle result;
        REQUIRE (materializeMusicalAudioChannels (description, physical, {}, result) == Refusal::Ok);
        REQUIRE (result);
        return result;
    };
    const auto physicalSubtotal = [] (const Buffer& physical) {
        return static_cast<std::uint64_t> (sizeof (AssetSamples)) + 64u
             + static_cast<std::uint64_t> (physical->interleaved.capacity()) * sizeof (float);
    };
    const auto declaredStable = [&] (const Buffer& physical, std::uint16_t logicalChannels) {
        const auto outputBytes = logicalChannels == 1 ? monoSampleBytes : stereoSampleBytes;
        return descriptionBytes + physicalSubtotal (physical) + workingObjectBytes + outputBytes;
    };
    const auto stereoReadMatches = [] (const ClipSchedule& schedule, std::span<const Word> expected) {
        // Normal-valued consumer checks deliberately do not certify signed-zero/denormal RT behavior.
        for (const int block : { 1, 7, 128, 257 })
        {
            std::vector<double> accumulation (2u * static_cast<std::size_t> (block), 0.0);
            for (std::int64_t start = -3; start < 8; start += block)
            {
                std::fill (accumulation.begin(), accumulation.end(), 0.0);
                accumulateClipSchedule (schedule, start, block, 2, block, accumulation.data());
                for (int channel = 0; channel < 2; ++channel)
                    for (int local = 0; local < block; ++local)
                    {
                        const auto frame = start + local;
                        double wanted = 0.0;
                        if (frame >= 0 && frame < 4)
                            wanted = static_cast<double> (std::bit_cast<float> (
                                expected[static_cast<std::size_t> (frame) * 2u
                                         + static_cast<std::size_t> (channel)]));
                        const auto actual = accumulation[static_cast<std::size_t> (channel)
                                                         * static_cast<std::size_t> (block)
                                                         + static_cast<std::size_t> (local)];
                        if (std::bit_cast<std::uint64_t> (actual) != std::bit_cast<std::uint64_t> (wanted))
                            return false;
                    }
            }
        }
        return true;
    };
    const auto borrowedSchedule = [] (const Working& working, int sourceChannels) {
        ScheduledClip clip;
        clip.samples = working.samples().data();
        clip.sourceFrames = 4;
        clip.sourceChannels = sourceChannels;
        clip.startFrame = 0;
        clip.windowFrames = 4;
        clip.gain = 1.0f;
        clip.fadeInFrames = 0;
        clip.fadeOutFrames = 0;
        clip.reversed = false;
        ClipSchedule schedule;
        schedule.clips.push_back (clip);
        return schedule;
    };

    auto mono = preparation (monoSource, 1);
    auto equivalent = preparation (equivalentSource, 2);
    auto distinct = preparation (distinctSource, 2);
    const auto one = ownedSamples (physicalOne, 1, destinationFrames);
    const auto two = ownedSamples (physicalTwo, 2, destinationFrames);

    SECTION ("All legal rows retain their exact request and materialize independent physical literals")
    {
        const auto check = [&] (const Preparation& prepared, const Buffer& physical,
                                 std::uint16_t logical, std::uint16_t actualPhysical,
                                 std::span<const Word> expected, std::span<const Word> sourceWords) {
            const auto before = snapshot (physical->interleaved);
            auto result = freshWorking (prepared.description, physical);
            REQUIRE (result->samples().size() == expected.size());
            CHECK (sameBits (result->samples(), expected));
            CHECK (result->logicalChannels() == logical);
            CHECK (result->physicalChannels() == actualPhysical);
            CHECK (result->frameCount() == 4u);
            CHECK (musical_channel_detail::sameOwner (result->plan(), prepared.description));
            CHECK (musical_channel_detail::sameOwner (result->plan()->request(), prepared.request));
            CHECK (musical_channel_detail::sameOwner (result->physicalOwner(), physical));
            CHECK (prepared.request->mappedWindow().srcOffset == 1u);
            CHECK (prepared.request->mappedWindow().srcLen == 4u);
            CHECK (prepared.asset.channels == logical);
            CHECK (prepared.source->channels == static_cast<int> (logical));
            CHECK (prepared.source->frames == 6u);
            CHECK (sameBits (physical->interleaved, before));
            CHECK (sameBits (prepared.source->interleaved, sourceWords));
        };
        check (mono, one, 1, 1, expectedMono, monoSource);
        check (equivalent, one, 2, 1, expectedEquivalent, equivalentSource);
        check (distinct, two, 2, 2, expectedDistinct, distinctSource);

        // The helper accepts arbitrary working PCM; a plan does not certify its DSP/envelope provenance.
        const auto reversed = preparation (equivalentSource, 2, {}, {}, true);
        CHECK (reversed.request->reversed());
        auto unchanged = freshWorking (reversed.description, one);
        CHECK (sameBits (unchanged->samples(), expectedEquivalent));
        constexpr std::array<Word, 4> maximumPhysical {
            0x3e800000u, 0x80000000u, 0x00000001u, 0x7f7fffffu
        };
        constexpr std::array<Word, 8> maximumExpected {
            0x3e800000u, 0x3e800000u, 0x80000000u, 0x80000000u,
            0x00000001u, 0x00000001u, 0x7f7fffffu, 0x7f7fffffu
        };
        auto maximum = freshWorking (equivalent.description, ownedSamples (maximumPhysical, 1, 4));
        CHECK (sameBits (maximum->samples(), maximumExpected));
    }

    SECTION ("The production layout gate rejects literal illegal projections without forging plans")
    {
        using musical_channel_materialization_detail::validLayout;
        constexpr std::array<std::uint16_t, 1> zero { 0 };
        constexpr std::array<std::uint16_t, 1> oneIndex { 1 };
        constexpr std::array<std::uint16_t, 2> duplicate { 0, 0 };
        constexpr std::array<std::uint16_t, 2> identity { 0, 1 };
        constexpr std::array<std::uint16_t, 2> swapped { 1, 0 };
        constexpr std::array<std::uint16_t, 2> outside { 0, 2 };
        constexpr std::array<std::uint16_t, 3> extra { 0, 0, 0 };
        CHECK (validLayout (1, 1, zero));
        CHECK (validLayout (2, 1, duplicate));
        CHECK (validLayout (2, 2, identity));
        CHECK_FALSE (validLayout (0, 1, zero));
        CHECK_FALSE (validLayout (3, 2, extra));
        CHECK_FALSE (validLayout (1, 0, zero));
        CHECK_FALSE (validLayout (1, 2, zero));
        CHECK_FALSE (validLayout (1, 1, oneIndex));
        CHECK_FALSE (validLayout (2, 1, zero));
        CHECK_FALSE (validLayout (2, 1, identity));
        CHECK_FALSE (validLayout (2, 1, extra));
        CHECK_FALSE (validLayout (2, 2, duplicate));
        CHECK_FALSE (validLayout (2, 2, swapped));
        CHECK_FALSE (validLayout (2, 2, outside));
        CHECK_FALSE (validLayout (2, 2, std::span<const std::uint16_t> {}));
    }

    SECTION ("Null and physical geometry failures clear staging and preserve live sources")
    {
        constexpr std::array<Word, 5> five {
            0x3e800000u, 0x80000000u, 0x00000001u, 0xbf000000u, 0x3f000000u
        };
        const auto reject = [&] (const Description& description, const Buffer& physical, Refusal wanted) {
            auto result = freshWorking (equivalent.description, one);
            const auto sourceBefore = snapshot (equivalent.source->interleaved);
            const auto oneBefore = snapshot (one->interleaved);
            const auto candidateBefore = physical ? snapshot (physical->interleaved) : std::vector<Word> {};
            CHECK (materializeMusicalAudioChannels (description, physical, {}, result) == wanted);
            CHECK_FALSE (result);
            CHECK (sameBits (equivalent.source->interleaved, sourceBefore));
            CHECK (sameBits (one->interleaved, oneBefore));
            if (physical) CHECK (sameBits (physical->interleaved, candidateBefore));
        };
        reject ({}, one, Refusal::InvalidInput);
        reject (equivalent.description, {}, Refusal::InvalidInput);
        reject (equivalent.description, ownedSamples (std::span<const Word> (physicalOne).first (3), 1, 3),
                Refusal::InvalidPhysicalGeometry);
        reject (equivalent.description, ownedSamples (five, 1, 5), Refusal::InvalidPhysicalGeometry);
        reject (equivalent.description, ownedSamples (physicalOne, 0, 4), Refusal::InvalidPhysicalGeometry);
        reject (equivalent.description, two, Refusal::InvalidPhysicalGeometry);
        reject (distinct.description, one, Refusal::InvalidPhysicalGeometry);
        reject (equivalent.description, ownedSamples (std::span<const Word> (physicalOne).first (3), 1, 4),
                Refusal::InvalidPhysicalGeometry);
        reject (equivalent.description, ownedSamples (five, 1, 4), Refusal::InvalidPhysicalGeometry);
        reject (equivalent.description, ownedSamples (physicalOne, 1, std::numeric_limits<std::uint64_t>::max()),
                Refusal::InvalidPhysicalGeometry);
    }

    SECTION ("Late nonfinite physical representations refuse without changing input bytes")
    {
        constexpr std::array<Word, 4> invalidWords { 0x7f800000u, 0xff800000u, 0x7fc00035u, 0x7f800001u };
        for (const auto invalid : invalidWords)
        {
            auto damagedOne = physicalOne;
            damagedOne[3] = invalid;
            const auto physical = ownedSamples (damagedOne, 1, 4);
            auto result = freshWorking (equivalent.description, one);
            CHECK (materializeMusicalAudioChannels (equivalent.description, physical, {}, result)
                   == Refusal::NonFinitePhysical);
            CHECK_FALSE (result);
            CHECK (sameBits (physical->interleaved, damagedOne));
            CHECK (sameBits (equivalent.source->interleaved, equivalentSource));
            for (std::size_t channel = 0; channel < 2; ++channel)
            {
                auto damagedTwo = physicalTwo;
                damagedTwo[6u + channel] = invalid;
                const auto stereo = ownedSamples (damagedTwo, 2, 4);
                result = freshWorking (distinct.description, two);
                CHECK (materializeMusicalAudioChannels (distinct.description, stereo, {}, result)
                       == Refusal::NonFinitePhysical);
                CHECK_FALSE (result);
                CHECK (sameBits (stereo->interleaved, damagedTwo));
                CHECK (sameBits (distinct.source->interleaved, distinctSource));
            }
        }
    }

    SECTION ("Borrowed replacement arguments and the unique working handle retain ownership")
    {
        Handle result;
        std::weak_ptr<const MusicalAudioChannelPlan> weakDescription;
        std::weak_ptr<const AssetSamples> weakPhysical;
        {
            auto local = preparation (equivalentSource, 2);
            auto physical = ownedSamples (physicalOne, 1, 4);
            weakDescription = local.description;
            weakPhysical = physical;
            result = freshWorking (local.description, physical);
            local.description.reset();
            local.request.reset();
            local.source.reset();
            local.window = {};
            physical.reset();
            CHECK_FALSE (weakDescription.expired());
            CHECK_FALSE (weakPhysical.expired());
            CHECK (sameBits (result->samples(), expectedEquivalent));
            const auto oldAddress = reinterpret_cast<std::uintptr_t> (result->samples().data());
            REQUIRE (materializeMusicalAudioChannels (result->plan(), result->physicalOwner(), {}, result)
                     == Refusal::Ok);
            REQUIRE (result);
            CHECK (reinterpret_cast<std::uintptr_t> (result->samples().data()) != oldAddress);
            CHECK (sameBits (result->samples(), expectedEquivalent));
            CHECK_FALSE (weakDescription.expired());
            CHECK_FALSE (weakPhysical.expired());
        }
        auto moved = std::move (result);
        CHECK_FALSE (result);
        REQUIRE (moved);
        CHECK (sameBits (moved->samples(), expectedEquivalent));
        moved.reset();
        CHECK (weakDescription.expired());
        CHECK (weakPhysical.expired());
    }

    SECTION ("Declared byte limits include every new request and retained input capacity")
    {
        const auto physical = ownedSamples (physicalOne, 1, 4, 32);
        REQUIRE (physical->interleaved.capacity() >= 32u);
        REQUIRE (physical->interleaved.capacity() > physical->interleaved.size());
        const auto stable = declaredStable (physical, 2);
        MusicalAudioLimits exact;
        exact.maxReservedBytes = stable;
        Handle result;
        REQUIRE (materializeMusicalAudioChannels (equivalent.description, physical, exact, result) == Refusal::Ok);
        REQUIRE (result);
        const auto& accounting = result->accounting();
        CHECK (accounting.planReservedBytes == descriptionBytes);
        CHECK (accounting.physicalReservedBytes == physicalSubtotal (physical));
        CHECK (accounting.objectRequestedBytes == workingObjectBytes);
        CHECK (accounting.sampleRequestedBytes == stereoSampleBytes);
        CHECK (accounting.reservedBytes == stable);
        CHECK (accounting.constructionPeakReservedBytes == stable);
        CHECK (accounting.framesMaterialized == 4u);
        CHECK (accounting.physicalSamplesVisited == 4u);
        CHECK (accounting.logicalSamplesConstructed == 8u);
        CHECK (sameBits (result->samples(), expectedEquivalent));
        result.reset();
        --exact.maxReservedBytes;
        CHECK (materializeMusicalAudioChannels (equivalent.description, physical, exact, result)
               == Refusal::ResourceLimit);
        CHECK_FALSE (result);
        CHECK (sameBits (physical->interleaved, physicalOne));

        auto monoResult = freshWorking (mono.description, one);
        CHECK (monoResult->accounting().sampleRequestedBytes == monoSampleBytes);
        CHECK (monoResult->accounting().reservedBytes == declaredStable (one, 1));
        auto distinctResult = freshWorking (distinct.description, two);
        CHECK (distinctResult->accounting().physicalSamplesVisited == 8u);
        CHECK (distinctResult->accounting().logicalSamplesConstructed == 8u);
        // capacity bytes and the 64-byte allowance are a declared subtotal, not allocator/RSS proof.
    }

    SECTION ("Replacement peaks deduplicate only proved plan request clock and physical owners")
    {
        const auto stable = declaredStable (one, 2);
        const auto checkPeak = [&] (const Description& oldDescription, const Buffer& oldPhysical,
                                    const Description& nextDescription, const Buffer& nextPhysical,
                                    std::uint64_t peak) {
            const auto oldBefore = snapshot (oldPhysical->interleaved);
            const auto nextBefore = snapshot (nextPhysical->interleaved);
            auto result = freshWorking (oldDescription, oldPhysical);
            MusicalAudioLimits limits;
            limits.maxReservedBytes = peak - 1u;
            CHECK (materializeMusicalAudioChannels (nextDescription, nextPhysical, limits, result)
                   == Refusal::ResourceLimit);
            CHECK_FALSE (result);
            CHECK (sameBits (oldPhysical->interleaved, oldBefore));
            CHECK (sameBits (nextPhysical->interleaved, nextBefore));
            result = freshWorking (oldDescription, oldPhysical);
            const auto oldAddress = reinterpret_cast<std::uintptr_t> (result->samples().data());
            limits.maxReservedBytes = peak;
            REQUIRE (materializeMusicalAudioChannels (nextDescription, nextPhysical, limits, result) == Refusal::Ok);
            REQUIRE (result);
            CHECK (result->accounting().reservedBytes == declaredStable (nextPhysical, 2));
            CHECK (result->accounting().constructionPeakReservedBytes == peak);
            CHECK (reinterpret_cast<std::uintptr_t> (result->samples().data()) != oldAddress);
            CHECK (sameBits (result->samples(), expectedEquivalent));
            CHECK (sameBits (oldPhysical->interleaved, oldBefore));
            CHECK (sameBits (nextPhysical->interleaved, nextBefore));
        };
        checkPeak (equivalent.description, one, equivalent.description, one,
                   2u * stable - descriptionBytes - physicalSubtotal (one));

        const auto separatePhysical = ownedSamples (physicalOne, 1, 4);
        checkPeak (equivalent.description, one, equivalent.description, separatePhysical,
                   stable + declaredStable (separatePhysical, 2) - descriptionBytes);
        const auto sameRequest = compileDescription (equivalent.request);
        checkPeak (equivalent.description, one, sameRequest, one,
                   2u * stable - requestBytes - physicalSubtotal (one));
        const auto sharedClock = preparation (equivalentSource, 2, equivalent.window.clock());
        checkPeak (equivalent.description, one, sharedClock.description, one,
                   2u * stable - clockBytes - physicalSubtotal (one));
        const auto unrelated = preparation (equivalentSource, 2);
        checkPeak (equivalent.description, one, unrelated.description, one,
                   2u * stable - physicalSubtotal (one));

        // Each diagnostic control owns a lifetime-preserving deleter; neither deletes the raw pointee.
        const Buffer otherPhysicalControl (one.get(), [keep = one] (const AssetSamples*) { (void) keep; });
        REQUIRE (otherPhysicalControl.get() == one.get());
        REQUIRE_FALSE (musical_channel_detail::sameOwner (otherPhysicalControl, one));
        checkPeak (equivalent.description, one, equivalent.description, otherPhysicalControl,
                   2u * stable - descriptionBytes);
        const Description otherPlanControl (
            equivalent.description.get(), [keep = equivalent.description] (const MusicalAudioChannelPlan*) {
                (void) keep;
            });
        REQUIRE (otherPlanControl.get() == equivalent.description.get());
        REQUIRE_FALSE (musical_channel_detail::sameOwner (otherPlanControl, equivalent.description));
        checkPeak (equivalent.description, one, otherPlanControl, one,
                   2u * stable - requestBytes - physicalSubtotal (one));
    }

    SECTION ("Work and current inherited ceilings refuse before any physical finite scan")
    {
        const auto checkWork = [&] (const Description& description, const Buffer& physical,
                                    std::uint64_t literalWork) {
            MusicalAudioLimits limits;
            limits.maxWorkUnits = literalWork;
            Handle result;
            REQUIRE (materializeMusicalAudioChannels (description, physical, limits, result) == Refusal::Ok);
            REQUIRE (result);
            CHECK (result->accounting().constructionWorkUnits == literalWork);
            result.reset();
            --limits.maxWorkUnits;
            CHECK (materializeMusicalAudioChannels (description, physical, limits, result)
                   == Refusal::ResourceLimit);
            CHECK_FALSE (result);
        };
        CHECK (mono.description->accounting().constructionWorkUnits == 5223u);
        CHECK (equivalent.description->accounting().constructionWorkUnits == 5287u);
        CHECK (distinct.description->accounting().constructionWorkUnits == 5287u);
        checkWork (mono.description, one, monoWork);
        checkWork (equivalent.description, one, equivalentWork);
        checkWork (distinct.description, two, distinctWork);

        auto lateNonfinite = physicalOne;
        lateNonfinite[3] = 0x7fc00045u;
        const auto invalid = ownedSamples (lateNonfinite, 1, 4);
        MusicalAudioLimits lowWork;
        lowWork.maxWorkUnits = 6438;
        Handle result;
        CHECK (materializeMusicalAudioChannels (equivalent.description, invalid, lowWork, result)
               == Refusal::ResourceLimit);
        CHECK_FALSE (result);
        CHECK (sameBits (invalid->interleaved, lateNonfinite));
        MusicalAudioLimits lowBytes;
        lowBytes.maxReservedBytes = declaredStable (invalid, 2) - 1u;
        CHECK (materializeMusicalAudioChannels (equivalent.description, invalid, lowBytes, result)
               == Refusal::ResourceLimit);
        CHECK_FALSE (result);
        CHECK (sameBits (invalid->interleaved, lateNonfinite));
        CHECK (materializeMusicalAudioChannels (equivalent.description, one, { 0, 0 }, result)
               == Refusal::ResourceLimit);
        CHECK_FALSE (result);

        MusicalAudioLimits inheritedWork;
        inheritedWork.maxWorkUnits = 6438;
        const auto requestLimited = preparation (equivalentSource, 2, {}, inheritedWork);
        CHECK (materializeMusicalAudioChannels (requestLimited.description, one, {}, result)
               == Refusal::ResourceLimit);
        CHECK_FALSE (result);
        const auto clockLimited = preparation (equivalentSource, 2, newClock (inheritedWork));
        CHECK (materializeMusicalAudioChannels (clockLimited.description, one, {}, result)
               == Refusal::ResourceLimit);
        CHECK_FALSE (result);

        MusicalAudioLimits inheritedBytes;
        inheritedBytes.maxReservedBytes = declaredStable (one, 2) - 1u;
        const auto requestByteLimited = preparation (equivalentSource, 2, {}, inheritedBytes);
        CHECK (materializeMusicalAudioChannels (requestByteLimited.description, one, {}, result)
               == Refusal::ResourceLimit);
        CHECK_FALSE (result);
        const auto clockByteLimited = preparation (equivalentSource, 2, newClock (inheritedBytes));
        CHECK (materializeMusicalAudioChannels (clockByteLimited.description, one, {}, result)
               == Refusal::ResourceLimit);
        CHECK_FALSE (result);

        constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
        const auto clamped = musical_channel_detail::effectiveLimits ({ maximum, maximum },
                                                                     { maximum, maximum },
                                                                     { maximum, maximum });
        CHECK (clamped.maxReservedBytes == 536870912u);
        CHECK (clamped.maxWorkUnits == 268435456u);
        REQUIRE (materializeMusicalAudioChannels (equivalent.description, one, { maximum, maximum }, result)
                 == Refusal::Ok);
        CHECK (sameBits (result->samples(), expectedEquivalent));
    }

    SECTION ("The actual temporary schedule reads logical stereo at unity across block sizes")
    {
        const auto physical = ownedSamples (normalPhysical, 1, 4);
        auto result = freshWorking (equivalent.description, physical);
        REQUIRE (sameBits (result->samples(), normalExpected));
        const auto schedule = borrowedSchedule (*result, 2);
        REQUIRE (schedule.clips.size() == 1u);
        CHECK (schedule.clips[0].samples == result->samples().data());
        CHECK (schedule.clips[0].sourceChannels == 2);
        CHECK (schedule.clips[0].sourceFrames == 4);
        CHECK (schedule.clips[0].startFrame == 0);
        CHECK (schedule.clips[0].windowFrames == 4);
        CHECK (schedule.clips[0].fadeInFrames == 0);
        CHECK (schedule.clips[0].fadeOutFrames == 0);
        CHECK_FALSE (schedule.clips[0].reversed);
        CHECK (schedule.clips[0].gain == 1.0f);
        CHECK (schedule.keepAlive.empty());
        CHECK (stereoReadMatches (schedule, normalExpected));
        CHECK (sameBits (physical->interleaved, normalPhysical));
        // result stays alive throughout this borrowed read; this is not production schedule installation.
    }

    SECTION ("Independent sample-array and actual consumer gates reject reached negative controls")
    {
        auto equivalentResult = freshWorking (equivalent.description, one);
        std::vector<float> corrupted (equivalentResult->samples().begin(), equivalentResult->samples().end());
        REQUIRE (sameBits (corrupted, expectedEquivalent));
        corrupted[0] = std::bit_cast<float> (std::bit_cast<Word> (corrupted[0]) ^ 1u);
        CHECK_FALSE (sameBits (corrupted, expectedEquivalent));
        CHECK (sameBits (equivalentResult->samples(), expectedEquivalent));

        auto distinctResult = freshWorking (distinct.description, two);
        std::vector<float> swapped (distinctResult->samples().begin(), distinctResult->samples().end());
        REQUIRE (sameBits (swapped, expectedDistinct));
        for (std::size_t frame = 0; frame < 4; ++frame)
            std::swap (swapped[frame * 2u], swapped[frame * 2u + 1u]);
        CHECK_FALSE (sameBits (swapped, expectedDistinct));
        CHECK (sameBits (distinctResult->samples(), expectedDistinct));

        auto normalResult = freshWorking (equivalent.description, ownedSamples (normalPhysical, 1, 4));
        const auto actualStereo = borrowedSchedule (*normalResult, 2);
        const auto actualMonoWidening = borrowedSchedule (*normalResult, 1);
        REQUIRE (stereoReadMatches (actualStereo, normalExpected));
        CHECK_FALSE (stereoReadMatches (actualMonoWidening, normalExpected));
        CHECK (sameBits (one->interleaved, physicalOne));
        CHECK (sameBits (two->interleaved, physicalTwo));
        // These are sample/consumer controls, not altered DSP or materializer execution certificates.
    }
}
