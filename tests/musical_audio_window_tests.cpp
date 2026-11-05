#include "engine/MusicalAudioWindow.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

TEST_CASE ("Musical audio windows retain exact source progress and complete prepared identity",
           "[musical-audio-window][adr0083]")
{
    using namespace yesdaw::engine;
    using Status = MusicalAudioStatus;
    const auto clockFor = [] (std::vector<TempoChange> rows, double rate = 48000.0) {
        std::shared_ptr<const MusicalAudioClock> clock;
        REQUIRE (MusicalAudioClock::build ({ rows.data(), rows.size() }, { rate }, {}, clock) == Status::Ok);
        REQUIRE (clock);
        return clock;
    };
    const auto windowFor = [] (const std::shared_ptr<const MusicalAudioClock>& clock,
                              Tick start, Tick length, std::uint64_t inputFrames) {
        MusicalAudioWindow window;
        REQUIRE (resolveMusicalAudioWindow (clock, start, length, inputFrames, window) == Status::Ok);
        REQUIRE (window.valid());
        return window;
    };
    const auto cutIs = [] (const MusicalAudioWindow& window, std::uint64_t input, std::int64_t expected) {
        std::int64_t cut = -99;
        REQUIRE (window.cutAtSource (input, cut) == Status::Ok);
        CHECK (cut == expected);
    };
    const auto ownerFor = [] (std::uint64_t frames, int channels = 1) -> std::shared_ptr<const AssetSamples> {
        auto owner = std::make_shared<AssetSamples>();
        owner->frames = frames;
        owner->channels = channels;
        owner->interleaved.assign (static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels), 0.125f);
        return owner;
    };
    const auto clipFor = [] {
        Clip clip;
        clip.id = EntityId::fromBigEndianParts (0x81, 1);
        clip.assetId = EntityId::fromBigEndianParts (0x82, 2);
        clip.trackId = EntityId::fromBigEndianParts (0x83, 3);
        clip.timelineStart = 30720;
        clip.timelineLength = 15360;
        clip.srcLen = 16;
        clip.fadeIn = 5;
        clip.fadeOut = 7;
        clip.timeBase = TimeBase::TempoLocked;
        return clip;
    };
    const auto assetFor = [] (const Clip& clip) {
        Asset asset;
        asset.id = clip.assetId;
        asset.contentHash.bytes[0] = 0x91;
        asset.frames = 16;
        asset.sampleRate = { 44100.0 };
        asset.channels = 1;
        return asset;
    };
    const MusicalAudioPreparationIdentity identity { 1, 1, 1, { 1, 1, 0 } };

    SECTION ("absolute endpoint floors retain phase and lazy source fractions")
    {
        const auto window = windowFor (clockFor ({ { 0, 120.0 } }), 1, 1, 16);
        CHECK (window.startFrame() == 1);
        CHECK (window.endFrame() == 3);
        CHECK (window.frameCount() == 2);
        cutIs (window, 0, 0);
        cutIs (window, 8, 1);
        cutIs (window, 16, 2);
        MusicalAudioPosition position;
        REQUIRE (window.positionAtSource (8, position) == Status::Ok);
        CHECK (position.whole == 1);
        CHECK (position.numerator == 8);
        CHECK (position.denominator == 16);
        REQUIRE (window.positionAtSource (16, position) == Status::Ok);
        CHECK (position.whole == 2);
        CHECK (position.numerator == 0);
        CHECK (position.denominator == 1);
    }
    SECTION ("jump timing retains the complete prefix and interior cuts")
    {
        const auto window = windowFor (clockFor ({ { 0, 120.0 }, { 30720, 60.0 } }), 23040, 30720, 49152);
        CHECK (window.startFrame() == 36000);
        CHECK (window.endFrame() == 120000);
        CHECK (window.frameCount() == 84000);
        cutIs (window, 0, 0);
        cutIs (window, 12288, 12000);
        cutIs (window, 24576, 36000);
        cutIs (window, 49152, 84000);
    }
    SECTION ("ramp queries match independently frozen Decimal values and a preceding quarter")
    {
        const auto window = windowFor (clockFor ({ { 0, 120.0 }, { 15360, 120.0, TempoCurve::LinearRamp },
                                                   { 76800, 240.0 } }), 15360, 30720, 49152);
        CHECK (window.startFrame() == 24000);
        CHECK (window.endFrame() == 62924);
        CHECK (window.frameCount() == 38924);
        struct Expected { std::uint64_t input; double absolute; std::int64_t cut; };
        // Decimal160: 24000 + 96000*ln(1+localTick/61440), generated before candidate execution.
        const std::array<Expected, 5> expected {{
            { 0, 24000.0, 0 },
            { 12288, 35307.17142301281163572423450917008369, 11307 },
            { 24576, 45421.78092616413655356432866974411232, 21421 },
            { 36864, 54571.55818737932311778373250469756118, 30571 },
            { 49152, 62924.65037838378066988925908457751711, 38924 }
        }};
        for (const auto& row : expected)
        {
            MusicalAudioFramePosition frame;
            REQUIRE (window.frameAtSource (row.input, frame) == Status::Ok);
            CHECK (frame.low <= frame.high);
            CHECK (frame.high - frame.low <= 1.0 / 1024.0);
            CHECK (std::abs (frame.low - row.absolute) <= 1.0 / 1024.0);
            CHECK (std::abs (frame.high - row.absolute) <= 1.0 / 1024.0);
            CHECK (frame.absoluteFrame == 24000 + row.cut);
            cutIs (window, row.input, row.cut);
        }
    }
    SECTION ("uint64 source denominators survive without allocating source frames")
    {
        constexpr std::uint64_t count = 4294967297ull;
        const auto window = windowFor (clockFor ({ { 0, 120.0 } }), 0, 1, count);
        MusicalAudioPosition position;
        REQUIRE (window.positionAtSource (1, position) == Status::Ok);
        CHECK (position.whole == 0);
        CHECK (position.numerator == 1);
        CHECK (position.denominator == count);
        cutIs (window, 1, 0);
        REQUIRE (window.positionAtSource (count - 1, position) == Status::Ok);
        CHECK (position.whole == 0);
        CHECK (position.numerator == count - 1);
        CHECK (position.denominator == count);
        cutIs (window, count - 1, 1);
        cutIs (window, count, 1);
    }
    SECTION ("wide multiply divide keeps quotient and remainder exact")
    {
        constexpr Tick length = (Tick { 1 } << 62) + 1;
        constexpr std::uint64_t count = (std::uint64_t { 1 } << 63) + 1;
        const auto window = windowFor (clockFor ({ { 0, 60.0 } }, 15360.0), 0, length, count);
        MusicalAudioPosition position;
        REQUIRE (window.positionAtSource (count - 1, position) == Status::Ok);
        CHECK (position.whole == (Tick { 1 } << 62));
        CHECK (position.numerator == (std::uint64_t { 1 } << 62));
        CHECK (position.denominator == count);
    }
    SECTION ("fraction brackets crossing an integer cut refuse without epsilon snapping")
    {
        constexpr auto count = std::numeric_limits<std::uint64_t>::max();
        const auto window = windowFor (clockFor ({ { 0, 60.0 } }, 15360.0), 0, 1, count);
        std::int64_t cut = 88;
        CHECK (window.cutAtSource (count - 1, cut) == Status::BoundaryAmbiguous);
        CHECK (cut == 0);
        MusicalAudioFramePosition frame { 2.0, 3.0, 4 };
        CHECK (window.frameAtSource (count - 1, frame) == Status::BoundaryAmbiguous);
        CHECK (frame == MusicalAudioFramePosition {});
        cutIs (window, 0, 0);
        cutIs (window, count, 1);
    }
    SECTION ("large representable integer cuts are admitted beyond both precision landmarks")
    {
        constexpr std::uint64_t count = 4294967298ull;
        const auto clock = clockFor ({ { 0, 60.0 } }, 15360.0);
        const auto lower = windowFor (clock, Tick { 1 } << 41, 8, count);
        CHECK (lower.startFrame() == (std::int64_t { 1 } << 41));
        CHECK (lower.endFrame() == (std::int64_t { 1 } << 41) + 8);
        cutIs (lower, count / 2, 4);
        // Exact Fraction/RNE derivation preserves source Time.h's delta*k/bpm operation order.
        const auto upper = windowFor (clock, Tick { 1 } << 54, 16, count);
        CHECK (upper.startFrame() == 18014398509481984ll);
        CHECK (upper.endFrame() == 18014398509482000ll);
        CHECK (upper.frameCount() == 16);
        cutIs (upper, count / 2, 8);
    }
    SECTION ("invalid geometry clocks and resource limits reset their outputs")
    {
        const auto clock = clockFor ({ { 0, 120.0 } });
        auto window = windowFor (clock, 1, 1, 16);
        CHECK (resolveMusicalAudioWindow (clock, -1, 1, 16, window) == Status::InvalidInput);
        CHECK_FALSE (window.valid());
        CHECK (window.frameCount() == 0);
        CHECK (resolveMusicalAudioWindow (clock, 0, 0, 16, window) == Status::InvalidInput);
        CHECK (resolveMusicalAudioWindow (clock, 0, 1, 0, window) == Status::InvalidInput);
        CHECK (resolveMusicalAudioWindow ({}, 0, 1, 16, window) == Status::InvalidClock);
        CHECK (resolveMusicalAudioWindow (clock, std::numeric_limits<Tick>::max(), 1, 16, window) == Status::Overflow);
        const auto overflowClock = clockFor ({ { 0, 60.0 } }, 30720.0);
        CHECK (resolveMusicalAudioWindow (overflowClock, 0, Tick { 1 } << 62, 16, window) == Status::Overflow);
        const auto collapsedClock = clockFor ({ { 0, 120.0 }, { 15360, 1.0e300 }, { 15361, 120.0 } });
        CHECK (resolveMusicalAudioWindow (collapsedClock, 15360, 1, 16, window) == Status::WindowCollapsed);
        CHECK_FALSE (window.valid());
        window = windowFor (clock, 0, 1, 16);
        MusicalAudioPosition position { 9, 3, 4 };
        CHECK (window.positionAtSource (17, position) == Status::InvalidInput);
        CHECK (position == MusicalAudioPosition {});
        std::int64_t cut = 55;
        CHECK (window.cutAtSource (17, cut) == Status::InvalidInput);
        CHECK (cut == 0);
        std::shared_ptr<const MusicalAudioClock> refused = clock;
        const std::array<TempoChange, 1> badRows {{ { 1, 120.0 } }};
        CHECK (MusicalAudioClock::build ({ badRows.data(), badRows.size() }, { 48000.0 }, {}, refused) == Status::InvalidClock);
        CHECK_FALSE (refused);
        CHECK (MusicalAudioClock::build ({}, { 0.0 }, {}, refused) == Status::InvalidClock);
        MusicalAudioLimits limits;
        limits.maxReservedBytes = 1;
        CHECK (MusicalAudioClock::build ({}, { 48000.0 }, limits, refused) == Status::ResourceLimit);
        CHECK_FALSE (refused);
        limits = {};
        limits.maxWorkUnits = 0;
        CHECK (MusicalAudioClock::build ({}, { 48000.0 }, limits, refused) == Status::ResourceLimit);
    }
    SECTION ("clock state owns its rows and admits a budget fitting large map")
    {
        std::vector<TempoChange> rows { { 0, 120.0 } };
        const auto clock = clockFor (rows);
        rows[0].bpm = 240.0;
        CHECK (windowFor (clock, 0, 15360, 16).endFrame() == 24000);
        rows.clear();
        for (Tick index = 0; index < 5000; ++index) rows.push_back ({ index * 15360, 120.0 });
        const auto large = windowFor (clockFor (rows), 4999 * 15360, 15360, 16);
        CHECK (large.startFrame() == 119976000);
        CHECK (large.endFrame() == 120000000);
        CHECK (large.frameCount() == 24000);
    }
    SECTION ("complete keys retain prefix rows owners tiers and native preparation dependencies")
    {
        const auto clip = clipFor();
        const auto asset = assetFor (clip);
        const auto original = ownerFor (16);
        const auto mapped = ownerFor (17);
        const auto clockA = clockFor ({ { 0, 120.0 } });
        const auto windowA = windowFor (clockA, 30720, 15360, 17);
        const auto keyFor = [&] (const Clip& value, const Asset& source,
                                const std::shared_ptr<const AssetSamples>& originalOwner,
                                const std::shared_ptr<const AssetSamples>& viewOwner,
                                ResampleQuality tier, const MusicalAudioWindow& window,
                                MusicalAudioPreparationIdentity version) {
            std::shared_ptr<const MusicalAudioKey> key;
            REQUIRE (makeMusicalAudioKey (value, source, originalOwner, viewOwner, { 0, 17 }, tier,
                                         window, version, key) == Status::Ok);
            REQUIRE (key);
            return key;
        };
        const auto key = keyFor (clip, asset, original, mapped, ResampleQuality::LivePlayback, windowA, identity);
        const auto equalWindow = windowFor (clockFor ({ { 0, 120.0 } }), 30720, 15360, 17);
        CHECK (*key == *keyFor (clip, asset, original, mapped, ResampleQuality::LivePlayback, equalWindow, identity));
        const auto clockB = clockFor ({ { 0, 240.0 }, { 15360, 80.0 }, { 30720, 120.0 } });
        const auto windowB = windowFor (clockB, 30720, 15360, 17);
        CHECK (windowA.startFrame() == 48000);
        CHECK (windowA.endFrame() == 72000);
        CHECK (windowB.startFrame() == 48000);
        CHECK (windowB.endFrame() == 72000);
        CHECK_FALSE (*key == *keyFor (clip, asset, original, mapped, ResampleQuality::LivePlayback, windowB, identity));
        const auto later = windowFor (clockFor ({ { 0, 120.0 }, { 100000, 180.0, TempoCurve::LinearRamp },
                                                 { 200000, 240.0 } }), 30720, 15360, 17);
        const auto laterChanged = windowFor (clockFor ({ { 0, 120.0 }, { 100000, 180.0, TempoCurve::LinearRamp },
                                                        { 200000, 250.0 } }), 30720, 15360, 17);
        const auto laterKey = keyFor (clip, asset, original, mapped, ResampleQuality::LivePlayback, later, identity);
        CHECK_FALSE (*laterKey == *keyFor (clip, asset, original, mapped, ResampleQuality::LivePlayback, laterChanged, identity));
        CHECK_FALSE (*key == *keyFor (clip, asset, ownerFor (16), mapped, ResampleQuality::LivePlayback, windowA, identity));
        CHECK_FALSE (*key == *keyFor (clip, asset, original, ownerFor (17), ResampleQuality::LivePlayback, windowA, identity));
        CHECK_FALSE (*key == *keyFor (clip, asset, original, mapped, ResampleQuality::OfflineRender, windowA, identity));
        auto changed = clip;
        changed.reversed = true;
        CHECK_FALSE (*key == *keyFor (changed, asset, original, mapped, ResampleQuality::LivePlayback, windowA, identity));
        changed = clip;
        changed.fadeIn = 20000;
        CHECK_FALSE (*key == *keyFor (changed, asset, original, mapped, ResampleQuality::LivePlayback, windowA, identity));
        changed = clip;
        changed.fadeOutShape = FadeShape::Linear;
        changed.fadeOutCurve = 0.5f;
        CHECK_FALSE (*key == *keyFor (changed, asset, original, mapped, ResampleQuality::LivePlayback, windowA, identity));
        auto version = identity;
        ++version.plannerRevision;
        CHECK_FALSE (*key == *keyFor (clip, asset, original, mapped, ResampleQuality::LivePlayback, windowA, version));
        version = identity;
        ++version.libraryVersion[2];
        CHECK_FALSE (*key == *keyFor (clip, asset, original, mapped, ResampleQuality::LivePlayback, windowA, version));
        auto changedAsset = asset;
        ++changedAsset.contentHash.bytes[1];
        CHECK_FALSE (*key == *keyFor (clip, changedAsset, original, mapped, ResampleQuality::LivePlayback, windowA, identity));
        changed = clip;
        changed.gain = 0.5f;
        changed.name = "new name";
        changed.colour = 0xff123456u;
        changed.trackId = EntityId::fromBigEndianParts (0x83, 99);
        changed.muted = true;
        changed.stretchFactor = 1.5f;
        CHECK (*key == *keyFor (changed, asset, original, mapped, ResampleQuality::LivePlayback, windowA, identity));
    }
    SECTION ("keys preserve original versus mapped ends and reject incoherent inputs")
    {
        auto clip = clipFor();
        clip.srcOffset = 1;
        clip.srcLen = 5;
        auto asset = assetFor (clip);
        asset.frames = 8;
        const auto original = ownerFor (8);
        const auto mapped = ownerFor (9);
        const auto window = windowFor (clockFor ({ { 0, 120.0 } }), 30720, 15360, 6);
        std::shared_ptr<const MusicalAudioKey> key;
        REQUIRE (makeMusicalAudioKey (clip, asset, original, mapped, { 1, 6 }, ResampleQuality::LivePlayback,
                                     window, identity, key) == Status::Ok);
        REQUIRE (key);
        CHECK (key->originalWindow().srcOffset == 1);
        CHECK (key->originalWindow().srcLen == 5);
        CHECK (key->mappedWindow().srcOffset == 1);
        CHECK (key->mappedWindow().srcLen == 6);
        CHECK (makeMusicalAudioKey (clip, asset, original, mapped, { 1, 5 }, ResampleQuality::LivePlayback,
                                   window, identity, key) == Status::InvalidOwnerWindow);
        CHECK_FALSE (key);
        CHECK (makeMusicalAudioKey (clip, asset, ownerFor (7), mapped, { 1, 6 }, ResampleQuality::LivePlayback,
                                   window, identity, key) == Status::InvalidOwnerWindow);
        CHECK_FALSE (key);
        CHECK (makeMusicalAudioKey (clip, asset, original, ownerFor (9, 2), { 1, 6 }, ResampleQuality::LivePlayback,
                                   window, identity, key) == Status::InvalidOwnerWindow);
        CHECK_FALSE (key);
        auto shortStorage = std::make_shared<AssetSamples>();
        shortStorage->frames = 8;
        shortStorage->channels = 1;
        shortStorage->interleaved.assign (7, 0.125f);
        CHECK (makeMusicalAudioKey (clip, asset, shortStorage, mapped, { 1, 6 }, ResampleQuality::LivePlayback,
                                   window, identity, key) == Status::InvalidOwnerWindow);
        CHECK_FALSE (key);
        clip.timeBase = TimeBase::SampleLocked;
        CHECK (makeMusicalAudioKey (clip, asset, original, mapped, { 1, 6 }, ResampleQuality::LivePlayback,
                                   window, identity, key) == Status::InvalidInput);
        CHECK_FALSE (key);
    }
    SECTION ("key lifetime retains original and mapped owners without address reuse")
    {
        std::weak_ptr<const AssetSamples> originalWeak, mappedWeak;
        std::shared_ptr<const MusicalAudioKey> key;
        {
            const auto clip = clipFor();
            const auto asset = assetFor (clip);
            const auto original = ownerFor (16);
            const auto mapped = ownerFor (17);
            originalWeak = original;
            mappedWeak = mapped;
            const auto window = windowFor (clockFor ({ { 0, 120.0 } }), 30720, 15360, 17);
            REQUIRE (makeMusicalAudioKey (clip, asset, original, mapped, { 0, 17 }, ResampleQuality::LivePlayback,
                                         window, identity, key) == Status::Ok);
        }
        REQUIRE (key);
        CHECK_FALSE (originalWeak.expired());
        CHECK_FALSE (mappedWeak.expired());
        key.reset();
        CHECK (originalWeak.expired());
        CHECK (mappedWeak.expired());
    }
}

TEST_CASE ("Musical audio output aliases retain borrowed clock inputs until completion",
           "[musical-audio-alias][adr0083]")
{
    using namespace yesdaw::engine;
    using Status = MusicalAudioStatus;
    const std::array<TempoChange, 2> rows {{ { 0, 120.0 }, { 30720, 60.0 } }};

    SECTION ("window output alias retains its clock")
    {
        std::shared_ptr<const MusicalAudioClock> clock;
        REQUIRE (MusicalAudioClock::build ({ rows.data(), rows.size() }, { 48000.0 }, {}, clock) == Status::Ok);
        REQUIRE (clock);
        MusicalAudioWindow window;
        REQUIRE (resolveMusicalAudioWindow (clock, 23040, 30720, 49152, window) == Status::Ok);
        REQUIRE (window.valid());
        std::weak_ptr<const MusicalAudioClock> retained = clock;
        clock.reset();
        REQUIRE (resolveMusicalAudioWindow (window.clock(), 23040, 30720, 49152, window) == Status::Ok);
        CHECK (window.valid());
        CHECK (window.startFrame() == 36000);
        CHECK (window.endFrame() == 120000);
        CHECK (window.frameCount() == 84000);
        CHECK_FALSE (retained.expired());
        CHECK (resolveMusicalAudioWindow (window.clock(), -1, 1, 16, window) == Status::InvalidInput);
        CHECK_FALSE (window.valid());
        CHECK (window.frameCount() == 0);
        CHECK (retained.expired());
    }
    SECTION ("clock output alias retains borrowed rows")
    {
        // Source-proven lifetime hazard: never execute this section against the old header.
        std::shared_ptr<const MusicalAudioClock> clock;
        REQUIRE (MusicalAudioClock::build ({ rows.data(), rows.size() }, { 48000.0 }, {}, clock) == Status::Ok);
        REQUIRE (clock);
        std::weak_ptr<const MusicalAudioClock> previous = clock;
        const auto borrowed = clock->rows();
        REQUIRE (MusicalAudioClock::build ({ borrowed.data(), borrowed.size() }, { 48000.0 }, {}, clock) == Status::Ok);
        REQUIRE (clock);
        CHECK (previous.expired());
        REQUIRE (clock->rows().size() == 2);
        CHECK (clock->rows()[0] == rows[0]);
        CHECK (clock->rows()[1] == rows[1]);
        MusicalAudioWindow window;
        REQUIRE (resolveMusicalAudioWindow (clock, 23040, 30720, 49152, window) == Status::Ok);
        CHECK (window.startFrame() == 36000);
        CHECK (window.endFrame() == 120000);
        CHECK (window.frameCount() == 84000);
        window = {};
        std::weak_ptr<const MusicalAudioClock> rejected = clock;
        const auto invalidBorrow = clock->rows().subspan (1);
        CHECK (MusicalAudioClock::build ({ invalidBorrow.data(), invalidBorrow.size() }, { 48000.0 }, {}, clock)
               == Status::InvalidClock);
        CHECK_FALSE (clock);
        CHECK (rejected.expired());
    }
}
