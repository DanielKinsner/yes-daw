#include "engine/MusicalAudioChannelPlan.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

TEST_CASE ("Musical channel plans own exact source equivalence and ready identity",
           "[musical-audio-channels][adr0084]")
{
    using namespace yesdaw::engine;
    using Status = MusicalAudioChannelStatus;
    using Request = MusicalAudioChannelRequest;
    using Plan = MusicalAudioChannelPlan;
    using U = std::uint32_t;
    using Samples = std::array<U, 4>;

    static_assert (! std::is_default_constructible_v<Request>);
    static_assert (! std::is_default_constructible_v<Plan>);
    static_assert (! std::is_copy_constructible_v<Request>);
    static_assert (! std::is_move_constructible_v<Request>);
    static_assert (! std::is_copy_constructible_v<Plan>);
    static_assert (! std::is_move_constructible_v<Plan>);

    constexpr Samples leftBits { 0x00000000u, 0x80000000u, 0x3f000000u, 0xbe800000u };
    constexpr Samples equivalentBits { 0x80000000u, 0x00000000u, 0x3f000000u, 0xbe800000u };
    constexpr Samples distinctBits { 0x00000000u, 0x80000000u, 0x3f000000u, 0xbe800001u };
    constexpr MusicalAudioPreparationIdentity baseIdentity { 1, 1, 1, { 1, 1, 0 } };

    const auto clockFor = [] (std::vector<TempoChange> rows, MusicalAudioLimits limits = {}) {
        std::shared_ptr<const MusicalAudioClock> clock;
        REQUIRE (MusicalAudioClock::build ({ rows.data(), rows.size() }, { 48000.0 }, limits, clock)
                 == MusicalAudioStatus::Ok);
        REQUIRE (clock);
        return clock;
    };
    const auto windowFor = [] (const std::shared_ptr<const MusicalAudioClock>& clock,
                              Tick start = 0, Tick length = 3, std::uint64_t frames = 4) {
        MusicalAudioWindow window;
        REQUIRE (resolveMusicalAudioWindow (clock, start, length, frames, window) == MusicalAudioStatus::Ok);
        return window;
    };
    const auto ownerFor = [] (int channels, const Samples& left, const Samples& right,
                             bool outsideNonfinite = false) -> std::shared_ptr<const AssetSamples> {
        auto owner = std::make_shared<AssetSamples>();
        owner->channels = channels;
        owner->frames = 6;
        owner->interleaved.assign (6u * static_cast<std::size_t> (channels), 0.0f);
        for (int c = 0; c < channels; ++c)
        {
            for (std::size_t i = 0; i < 4; ++i)
                owner->interleaved[(i + 1u) * static_cast<std::size_t> (channels) + static_cast<std::size_t> (c)]
                    = std::bit_cast<float> (c == 0 ? left[i] : right[i]);
            if (outsideNonfinite)
            {
                owner->interleaved[static_cast<std::size_t> (c)]
                    = std::bit_cast<float> (c == 0 ? 0x7fc00011u : 0xff800000u);
                owner->interleaved[5u * static_cast<std::size_t> (channels) + static_cast<std::size_t> (c)]
                    = std::bit_cast<float> (c == 0 ? 0x7f800000u : 0x7fc00022u);
            }
        }
        return owner;
    };
    struct Fixture
    {
        Clip clip;
        Asset asset;
        std::shared_ptr<const AssetSamples> original, mapped;
        ViewWindow mappedWindow { 1, 4 };
        MusicalAudioWindow window;
    };
    const auto fixtureFor = [&] (int channels, const Samples& left, const Samples& right,
                                bool outsideNonfinite = false) {
        Fixture fixture;
        fixture.clip.id = EntityId::fromBigEndianParts (0x841, 1);
        fixture.clip.assetId = EntityId::fromBigEndianParts (0x842, 2);
        fixture.clip.trackId = EntityId::fromBigEndianParts (0x843, 3);
        fixture.clip.timeBase = TimeBase::TempoLocked;
        fixture.clip.timelineStart = 0;
        fixture.clip.timelineLength = 3;
        fixture.clip.srcOffset = 1;
        fixture.clip.srcLen = 4;
        fixture.asset.id = fixture.clip.assetId;
        fixture.asset.contentHash.bytes[0] = 0x84;
        fixture.asset.frames = 6;
        fixture.asset.sampleRate = { 48000.0 };
        fixture.asset.channels = static_cast<std::uint16_t> (channels);
        fixture.original = ownerFor (channels, left, right, outsideNonfinite);
        fixture.mapped = fixture.original;
        fixture.window = windowFor (clockFor ({ { 0, 120.0 } }));
        return fixture;
    };
    const auto makeRequest = [] (const Fixture& fixture, std::shared_ptr<const Request>& out,
                                MusicalAudioChannelPolicy policy = {}, MusicalAudioLimits limits = {},
                                ResampleQuality tier = ResampleQuality::LivePlayback,
                                MusicalAudioPreparationIdentity identity = { 1, 1, 1, { 1, 1, 0 } }) {
        return makeMusicalAudioChannelRequest (fixture.clip, fixture.asset, fixture.original, fixture.mapped,
                                              fixture.mappedWindow, tier, fixture.window, identity, policy, limits, out);
    };
    const auto requestFor = [&] (const Fixture& fixture, MusicalAudioLimits limits = {},
                                ResampleQuality tier = ResampleQuality::LivePlayback,
                                MusicalAudioPreparationIdentity identity = { 1, 1, 1, { 1, 1, 0 } }) {
        std::shared_ptr<const Request> request;
        const auto result = makeRequest (fixture, request, {}, limits, tier, identity);
        REQUIRE (result.status == Status::Ok);
        REQUIRE (result.geometryStatus == MusicalAudioStatus::Ok);
        REQUIRE (request);
        return request;
    };
    const auto planFor = [] (const std::shared_ptr<const Request>& request, MusicalAudioLimits limits = {}) {
        std::shared_ptr<const Plan> plan;
        const auto result = buildMusicalAudioChannelPlan (request, limits, plan);
        REQUIRE (result.status == Status::Ok);
        REQUIRE (result.geometryStatus == MusicalAudioStatus::Ok);
        REQUIRE (plan);
        return plan;
    };
    const auto layoutIs = [] (const std::shared_ptr<const Plan>& plan, std::uint16_t logical,
                             std::uint16_t physical, std::array<std::uint16_t, 2> projection) {
        REQUIRE (plan);
        CHECK (plan->logicalChannels() == logical);
        CHECK (plan->physicalChannels() == physical);
        REQUIRE (plan->projection().size() == logical);
        for (std::size_t i = 0; i < logical; ++i)
            CHECK (plan->projection()[i] == projection[i]);
    };
    const auto readBits = [] (const std::shared_ptr<const Plan>& plan, std::uint16_t channel,
                             std::uint64_t frame) {
        float sample = 0.75f;
        REQUIRE (plan->physicalSample (channel, frame, sample) == Status::Ok);
        return std::bit_cast<U> (sample);
    };
    const auto ownerBits = [] (const std::shared_ptr<const AssetSamples>& owner) {
        std::vector<U> result;
        result.reserve (owner->interleaved.size());
        for (const auto& sample : owner->interleaved) result.push_back (std::bit_cast<U> (sample));
        return result;
    };

    SECTION ("request factories bind complete geometry and refuse unknown policy or layout")
    {
        const auto fixture = fixtureFor (2, leftBits, equivalentBits);
        const auto request = requestFor (fixture);
        REQUIRE (request->key());
        CHECK (request->originalOwner().get() == fixture.original.get());
        CHECK (request->mappedOwner().get() == fixture.mapped.get());
        CHECK (request->mappedWindow().srcOffset == 1u);
        CHECK (request->mappedWindow().srcLen == 4u);
        CHECK (request->logicalChannels() == 2u);
        CHECK_FALSE (request->reversed());
        CHECK (request->window().startFrame() == 0);
        CHECK (request->window().endFrame() == 4);
        CHECK (request->window().frameCount() == 4);
        CHECK (request->policy().channelRevision == 1u);
        CHECK (request->policy().materializationRevision == 1u);

        const std::array<MusicalAudioChannelPolicy, 4> unknownPolicies {{ { 0, 1 }, { 2, 1 }, { 1, 0 }, { 1, 2 } }};
        for (const auto& policy : unknownPolicies)
        {
            auto out = request;
            CHECK_FALSE (policy == MusicalAudioChannelPolicy {});
            const auto result = makeRequest (fixture, out, policy);
            CHECK (result.status == Status::UnsupportedPolicy);
            CHECK (result.geometryStatus == MusicalAudioStatus::Ok);
            CHECK_FALSE (out);
        }
        for (const auto& channels : std::array<int, 2> { 0, 3 })
        {
            const auto unsupported = fixtureFor (channels, leftBits, equivalentBits);
            auto out = request;
            CHECK (makeRequest (unsupported, out).status == Status::UnsupportedLayout);
            CHECK_FALSE (out);
        }
        const auto geometryRefuses = [&] (const Fixture& invalid, MusicalAudioStatus expected) {
            auto out = request;
            const auto result = makeRequest (invalid, out);
            CHECK (result.status == Status::GeometryRefused);
            CHECK (result.geometryStatus == expected);
            CHECK_FALSE (out);
        };
        auto invalid = fixture;
        invalid.window = {};
        geometryRefuses (invalid, MusicalAudioStatus::InvalidClock);
        invalid = fixture;
        invalid.mappedWindow.srcLen = 3;
        geometryRefuses (invalid, MusicalAudioStatus::InvalidOwnerWindow);
        invalid = fixture;
        invalid.clip.assetId = EntityId::fromBigEndianParts (0x842, 99);
        geometryRefuses (invalid, MusicalAudioStatus::InvalidOwnerWindow);
        invalid = fixture;
        auto shortOwner = std::make_shared<AssetSamples>();
        shortOwner->channels = 2;
        shortOwner->frames = 6;
        shortOwner->interleaved.assign (11, 0.0f);
        invalid.mapped = shortOwner;
        geometryRefuses (invalid, MusicalAudioStatus::InvalidOwnerWindow);
        std::shared_ptr<const Plan> noPlan;
        const auto result = buildMusicalAudioChannelPlan ({}, {}, noPlan);
        CHECK (result.status == Status::InvalidInput);
        CHECK (result.geometryStatus == MusicalAudioStatus::Ok);
        CHECK_FALSE (noPlan);
    }
    SECTION ("complete numeric equivalence includes every signed zero pairing")
    {
        const std::array<std::array<U, 2>, 4> zeroPairs {{
            { 0x00000000u, 0x00000000u }, { 0x00000000u, 0x80000000u },
            { 0x80000000u, 0x00000000u }, { 0x80000000u, 0x80000000u }
        }};
        for (const auto& pair : zeroPairs)
        {
            Samples left {}, right {};
            left.fill (pair[0]);
            right.fill (pair[1]);
            const auto plan = planFor (requestFor (fixtureFor (2, left, right)));
            layoutIs (plan, 2, 1, { 0, 0 });
            CHECK (plan->accounting().framesVisited == 4u);
            CHECK (plan->accounting().samplesVisited == 8u);
            for (std::uint64_t i = 0; i < 4; ++i) CHECK (readBits (plan, 0, i) == pair[0]);
        }
        for (const auto& right : std::array<Samples, 2> { leftBits, equivalentBits })
        {
            const auto plan = planFor (requestFor (fixtureFor (2, leftBits, right)));
            layoutIs (plan, 2, 1, { 0, 0 });
            for (std::uint64_t i = 0; i < 4; ++i) CHECK (readBits (plan, 0, i) == leftBits[i]);
        }
        const auto mono = planFor (requestFor (fixtureFor (1, leftBits, leftBits)));
        layoutIs (mono, 1, 1, { 0, 0 });
        CHECK (mono->accounting().framesVisited == 4u);
        CHECK (mono->accounting().samplesVisited == 4u);
    }
    SECTION ("last frame mismatch stays distinct and late nonfinite samples cannot escape the scan")
    {
        const auto distinct = planFor (requestFor (fixtureFor (2, leftBits, distinctBits)));
        layoutIs (distinct, 2, 2, { 0, 1 });
        CHECK (readBits (distinct, 0, 3) == 0xbe800000u);
        CHECK (readBits (distinct, 1, 3) == 0xbe800001u);
        const std::array<U, 4> nonfiniteBits { 0x7f800000u, 0xff800000u, 0x7fc00001u, 0x7f800001u };
        for (const auto& poison : nonfiniteBits)
        {
            auto right = equivalentBits;
            right[0] = 0x3e800000u;
            right[3] = poison;
            const auto request = requestFor (fixtureFor (2, leftBits, right));
            auto out = distinct;
            const auto result = buildMusicalAudioChannelPlan (request, {}, out);
            CHECK (result.status == Status::NonFiniteSource);
            CHECK (result.geometryStatus == MusicalAudioStatus::Ok);
            CHECK_FALSE (out);
            auto both = leftBits;
            both[3] = poison;
            out = distinct;
            CHECK (buildMusicalAudioChannelPlan (requestFor (fixtureFor (2, both, both)), {}, out).status
                   == Status::NonFiniteSource);
            CHECK_FALSE (out);
            CHECK (buildMusicalAudioChannelPlan (requestFor (fixtureFor (1, both, both)), {}, out).status
                   == Status::NonFiniteSource);
            CHECK_FALSE (out);
        }
    }
    SECTION ("selected window and reversal preserve representative bits and ignore outside sentinels")
    {
        auto fixture = fixtureFor (2, leftBits, equivalentBits, true);
        const auto before = ownerBits (fixture.original);
        const auto forwardRequest = requestFor (fixture);
        const auto forward = planFor (forwardRequest);
        fixture.clip.reversed = true;
        const auto reverseRequest = requestFor (fixture);
        const auto reversed = planFor (reverseRequest);
        CHECK_FALSE (*forwardRequest == *reverseRequest);
        layoutIs (forward, 2, 1, { 0, 0 });
        layoutIs (reversed, 2, 1, { 0, 0 });
        constexpr Samples reverseBits { 0xbe800000u, 0x3f000000u, 0x80000000u, 0x00000000u };
        for (std::uint64_t i = 0; i < 4; ++i)
        {
            CHECK (readBits (forward, 0, i) == leftBits[i]);
            CHECK (readBits (reversed, 0, i) == reverseBits[i]);
        }
        CHECK (ownerBits (fixture.original) == before);
        CHECK (ownerBits (fixture.mapped) == before);
    }
    SECTION ("physical reads are bounded and descriptor layouts stay immutable")
    {
        struct Layout
        {
            int channels;
            Samples right;
            std::uint16_t physical;
            std::array<std::uint16_t, 2> projection;
        };
        const std::array<Layout, 3> layouts {{ { 1, leftBits, 1, { 0, 0 } },
                                             { 2, equivalentBits, 1, { 0, 0 } },
                                             { 2, distinctBits, 2, { 0, 1 } } }};
        for (const auto& layout : layouts)
        {
            const auto plan = planFor (requestFor (fixtureFor (layout.channels, leftBits, layout.right)));
            layoutIs (plan, static_cast<std::uint16_t> (layout.channels), layout.physical, layout.projection);
            float value = -0.75f;
            CHECK (plan->physicalSample (layout.physical, 0, value) == Status::InvalidInput);
            CHECK (std::bit_cast<U> (value) == 0u);
            value = -0.75f;
            CHECK (plan->physicalSample (std::numeric_limits<std::uint16_t>::max(), 0, value) == Status::InvalidInput);
            CHECK (std::bit_cast<U> (value) == 0u);
            for (const auto& frame : std::array<std::uint64_t, 2> { 4, std::numeric_limits<std::uint64_t>::max() })
            {
                value = -0.75f;
                CHECK (plan->physicalSample (0, frame, value) == Status::InvalidInput);
                CHECK (std::bit_cast<U> (value) == 0u);
            }
            if (layout.physical == 2)
                for (std::uint64_t i = 0; i < 4; ++i) CHECK (readBits (plan, 1, i) == distinctBits[i]);
        }
    }
    SECTION ("ready identity includes complete dependencies but excludes gain name and resource ceilings")
    {
        const auto fixture = fixtureFor (2, leftBits, equivalentBits);
        const auto request = requestFor (fixture);
        const auto plan = planFor (request);
        const auto equal = requestFor (fixture);
        CHECK (request.get() != equal.get());
        CHECK (*request == *equal);
        CHECK (plan->matches (equal));
        CHECK_FALSE (plan->matches ({}));
        auto changed = fixture;
        changed.clip.gain = 0.5f;
        changed.clip.name = "renamed";
        const auto unrelated = requestFor (changed);
        CHECK (*request == *unrelated);
        CHECK (plan->matches (unrelated));
        MusicalAudioLimits lower;
        lower.maxWorkUnits = 4103;
        const auto limited = requestFor (fixture, lower);
        CHECK (*request == *limited);
        CHECK (plan->matches (limited));
        // matches certifies identity only; adoption must separately enforce its current resource ceiling.
        CHECK (plan->accounting().constructionWorkUnits > limited->limits().maxWorkUnits);
        const auto differs = [&] (const Fixture& value, ResampleQuality tier = ResampleQuality::LivePlayback,
                                 MusicalAudioPreparationIdentity identity = { 1, 1, 1, { 1, 1, 0 } }) {
            const auto other = requestFor (value, {}, tier, identity);
            CHECK_FALSE (*request == *other);
            CHECK_FALSE (plan->matches (other));
        };
        changed = fixture;
        changed.original = ownerFor (2, leftBits, equivalentBits);
        differs (changed);
        changed = fixture;
        changed.mapped = ownerFor (2, leftBits, equivalentBits);
        differs (changed);
        changed = fixture;
        changed.clip.srcOffset = 0;
        changed.mappedWindow.srcOffset = 0;
        differs (changed);
        changed = fixture;
        changed.clip.srcLen = 3;
        changed.mappedWindow.srcLen = 3;
        changed.window = windowFor (fixture.window.clock(), 0, 3, 3);
        differs (changed);
        changed = fixture;
        changed.clip.reversed = true;
        differs (changed);
        changed = fixture;
        changed.clip.fadeIn = 7;
        differs (changed);
        changed = fixture;
        changed.clip.fadeOut = 8;
        differs (changed);
        changed = fixture;
        changed.clip.fadeOutShape = FadeShape::Linear;
        changed.clip.fadeOutCurve = 0.5f;
        differs (changed);
        changed = fixture;
        changed.clip.fadeInCurve = -0.5f;
        differs (changed);
        changed = fixture;
        ++changed.asset.contentHash.bytes[1];
        differs (changed);
        differs (fixture, ResampleQuality::OfflineRender);
        for (const auto& index : std::array<int, 4> { 0, 1, 2, 3 })
        {
            auto identity = baseIdentity;
            if (index == 0) ++identity.plannerRevision;
            if (index == 1) ++identity.envelopeRevision;
            if (index == 2) ++identity.qualityRevision;
            if (index == 3) ++identity.libraryVersion[2];
            differs (fixture, ResampleQuality::LivePlayback, identity);
        }
        auto prefixA = fixture;
        prefixA.clip.timelineStart = 30720;
        prefixA.window = windowFor (clockFor ({ { 0, 120.0 } }), 30720);
        auto prefixB = prefixA;
        prefixB.window = windowFor (clockFor ({ { 0, 90.0 }, { 30720, 120.0 } }), 30720);
        CHECK (prefixA.window.startFrame() == 48000);
        CHECK (prefixB.window.startFrame() == 64000);
        CHECK (prefixA.window.frameCount() == 4);
        CHECK (prefixB.window.frameCount() == 4);
        const auto requestA = requestFor (prefixA);
        const auto requestB = requestFor (prefixB);
        CHECK_FALSE (*requestA == *requestB);
        CHECK_FALSE (planFor (requestA)->matches (requestB));
    }
    SECTION ("plans retain owners and distinguish safe same-address different-control-block identities")
    {
        std::weak_ptr<const AssetSamples> originalWeak, mappedWeak;
        std::weak_ptr<const MusicalAudioClock> clockWeak;
        std::shared_ptr<const Plan> retained;
        {
            auto fixture = fixtureFor (2, leftBits, equivalentBits);
            fixture.mapped = ownerFor (2, leftBits, equivalentBits);
            originalWeak = fixture.original;
            mappedWeak = fixture.mapped;
            clockWeak = fixture.window.clock();
            retained = planFor (requestFor (fixture));
        }
        CHECK_FALSE (originalWeak.expired());
        CHECK_FALSE (mappedWeak.expired());
        CHECK_FALSE (clockWeak.expired());
        CHECK (readBits (retained, 0, 2) == 0x3f000000u);
        retained.reset();
        CHECK (originalWeak.expired());
        CHECK (mappedWeak.expired());
        CHECK (clockWeak.expired());
        const auto backing = ownerFor (2, leftBits, equivalentBits);
        const std::shared_ptr<const AssetSamples> first (backing.get(), [keep = backing] (const AssetSamples*) { (void) keep; });
        const std::shared_ptr<const AssetSamples> second (backing.get(), [keep = backing] (const AssetSamples*) { (void) keep; });
        CHECK (first.get() == second.get());
        CHECK ((first.owner_before (second) || second.owner_before (first)));
        auto fixtureA = fixtureFor (2, leftBits, equivalentBits);
        fixtureA.original = fixtureA.mapped = first;
        auto fixtureB = fixtureA;
        fixtureB.original = fixtureB.mapped = second;
        const auto requestA = requestFor (fixtureA);
        const auto requestB = requestFor (fixtureB);
        CHECK_FALSE (*requestA == *requestB);
        CHECK_FALSE (planFor (requestA)->matches (requestB));
    }
    SECTION ("borrowed request and plan output aliases retain their inputs until the call finishes")
    {
        auto fixture = fixtureFor (2, leftBits, equivalentBits);
        auto request = requestFor (fixture);
        std::weak_ptr<const Request> oldRequest = request;
        std::weak_ptr<const AssetSamples> source = request->mappedOwner();
        fixture.original.reset();
        fixture.mapped.reset();
        fixture.window = {};
        const auto result = makeMusicalAudioChannelRequest (
            fixture.clip, fixture.asset, request->originalOwner(), request->mappedOwner(), request->mappedWindow(),
            ResampleQuality::LivePlayback, request->window(), baseIdentity, request->policy(), {}, request);
        REQUIRE (result.status == Status::Ok);
        REQUIRE (request);
        CHECK (oldRequest.expired());
        CHECK_FALSE (source.expired());
        auto plan = planFor (request);
        std::weak_ptr<const Plan> oldPlan = plan;
        request.reset();
        REQUIRE (buildMusicalAudioChannelPlan (plan->request(), {}, plan).status == Status::Ok);
        REQUIRE (plan);
        CHECK (oldPlan.expired());
        CHECK (readBits (plan, 0, 3) == 0xbe800000u);
        CHECK_FALSE (source.expired());
        CHECK (buildMusicalAudioChannelPlan ({}, {}, plan).status == Status::InvalidInput);
        CHECK_FALSE (plan);
        CHECK (source.expired());

        fixture = fixtureFor (2, leftBits, equivalentBits);
        request = requestFor (fixture);
        source = request->mappedOwner();
        fixture.original.reset();
        fixture.mapped.reset();
        fixture.window = {};
        CHECK (makeMusicalAudioChannelRequest (
            fixture.clip, fixture.asset, request->originalOwner(), request->mappedOwner(), request->mappedWindow(),
            ResampleQuality::LivePlayback, request->window(), baseIdentity, { 2, 1 }, {}, request).status
               == Status::UnsupportedPolicy);
        CHECK_FALSE (request);
        CHECK (source.expired());
    }
    SECTION ("work limits prepay the complete scan and clamp to request clock and accepted ceilings")
    {
        const auto fixture = fixtureFor (2, leftBits, equivalentBits);
        MusicalAudioLimits exact;
        exact.maxWorkUnits = 5287;
        const auto request = requestFor (fixture, exact);
        CHECK (request->accounting().constructionWorkUnits == 4103u);
        CHECK (request->accounting().framesVisited == 0u);
        CHECK (request->accounting().samplesVisited == 0u);
        const auto plan = planFor (request, exact);
        CHECK (plan->accounting().constructionWorkUnits == 5287u);
        CHECK (plan->accounting().framesVisited == 4u);
        CHECK (plan->accounting().samplesVisited == 8u);
        MusicalAudioLimits below = exact;
        --below.maxWorkUnits;
        std::shared_ptr<const Plan> out;
        CHECK (buildMusicalAudioChannelPlan (request, below, out).status == Status::ResourceLimit);
        CHECK_FALSE (out);
        auto poisoned = equivalentBits;
        poisoned[3] = 0x7fc00001u;
        CHECK (buildMusicalAudioChannelPlan (requestFor (fixtureFor (2, leftBits, poisoned)), below, out).status
               == Status::ResourceLimit);
        CHECK_FALSE (out);
        MusicalAudioLimits monoExact;
        monoExact.maxWorkUnits = 5223;
        const auto monoRequest = requestFor (fixtureFor (1, leftBits, leftBits), monoExact);
        CHECK (planFor (monoRequest, monoExact)->accounting().constructionWorkUnits == 5223u);
        --monoExact.maxWorkUnits;
        CHECK (buildMusicalAudioChannelPlan (monoRequest, monoExact, out).status == Status::ResourceLimit);
        CHECK_FALSE (out);
        MusicalAudioLimits requestExact;
        requestExact.maxWorkUnits = 4103;
        std::shared_ptr<const Request> requestOut;
        REQUIRE (makeRequest (fixture, requestOut, {}, requestExact).status == Status::Ok);
        --requestExact.maxWorkUnits;
        CHECK (makeRequest (fixture, requestOut, {}, requestExact).status == Status::ResourceLimit);
        CHECK_FALSE (requestOut);
        MusicalAudioLimits unlimited { std::numeric_limits<std::uint64_t>::max(),
                                       std::numeric_limits<std::uint64_t>::max() };
        const auto clamped = requestFor (fixture, unlimited);
        CHECK (clamped->limits().maxReservedBytes == kMusicalAudioMaxReservedBytes);
        CHECK (clamped->limits().maxWorkUnits == kMusicalAudioMaxWorkUnits);
        CHECK (planFor (clamped, unlimited)->accounting().constructionWorkUnits == 5287u);
        auto clockLimited = fixture;
        clockLimited.window = windowFor (clockFor ({ { 0, 120.0 } }, below));
        CHECK (buildMusicalAudioChannelPlan (requestFor (clockLimited, unlimited), unlimited, out).status
               == Status::ResourceLimit);
        CHECK_FALSE (out);
        CHECK (buildMusicalAudioChannelPlan (requestFor (fixture, { kMusicalAudioMaxReservedBytes, 4103 }),
                                             unlimited, out).status == Status::ResourceLimit);
        CHECK_FALSE (out);
    }
    SECTION ("byte limits include exact stable objects and temporary replacement overlap")
    {
        constexpr std::uint64_t clockBytes = sizeof (MusicalAudioClock) + 256u
            + 2u * sizeof (TempoChange) + sizeof (double);
        constexpr std::uint64_t keyBytes = sizeof (MusicalAudioKey) + 64u;
        constexpr std::uint64_t requestBytes = sizeof (Request) + 64u;
        constexpr std::uint64_t planBytes = sizeof (Plan) + 64u;
        constexpr std::uint64_t stableRequest = clockBytes + keyBytes + requestBytes;
        constexpr std::uint64_t stablePlan = stableRequest + planBytes;
        const auto fixture = fixtureFor (2, leftBits, equivalentBits);
        CHECK (fixture.window.clock()->reservedBytes() == clockBytes);
        MusicalAudioLimits limit { stableRequest, kMusicalAudioMaxWorkUnits };
        std::shared_ptr<const Request> request;
        REQUIRE (makeRequest (fixture, request, {}, limit).status == Status::Ok);
        CHECK (request->accounting().clockReservedBytes == clockBytes);
        CHECK (request->accounting().keyReservedBytes == keyBytes);
        CHECK (request->accounting().requestReservedBytes == requestBytes);
        CHECK (request->accounting().planReservedBytes == 0u);
        CHECK (request->accounting().reservedBytes == stableRequest);
        CHECK (request->accounting().constructionPeakReservedBytes == stableRequest);
        request.reset();
        --limit.maxReservedBytes;
        CHECK (makeRequest (fixture, request, {}, limit).status == Status::ResourceLimit);
        CHECK_FALSE (request);

        limit.maxReservedBytes = stablePlan;
        request = requestFor (fixture, limit);
        std::shared_ptr<const Plan> plan;
        REQUIRE (buildMusicalAudioChannelPlan (request, limit, plan).status == Status::Ok);
        CHECK (plan->accounting().reservedBytes == stablePlan);
        CHECK (plan->accounting().planReservedBytes == planBytes);
        CHECK (plan->accounting().constructionPeakReservedBytes == stablePlan);
        plan.reset();
        --limit.maxReservedBytes;
        CHECK (buildMusicalAudioChannelPlan (request, limit, plan).status == Status::ResourceLimit);
        CHECK_FALSE (plan);

        constexpr std::uint64_t replaceRequest = clockBytes + 2u * (keyBytes + requestBytes);
        limit.maxReservedBytes = replaceRequest;
        request = requestFor (fixture, limit);
        REQUIRE (makeRequest (fixture, request, {}, limit).status == Status::Ok);
        CHECK (request->accounting().reservedBytes == stableRequest);
        CHECK (request->accounting().constructionPeakReservedBytes == replaceRequest);
        --limit.maxReservedBytes;
        CHECK (makeRequest (fixture, request, {}, limit).status == Status::ResourceLimit);
        CHECK_FALSE (request);

        constexpr std::uint64_t replacePlan = stableRequest + 2u * planBytes;
        limit.maxReservedBytes = replacePlan;
        request = requestFor (fixture, limit);
        plan = planFor (request, limit);
        request.reset();
        REQUIRE (buildMusicalAudioChannelPlan (plan->request(), limit, plan).status == Status::Ok);
        CHECK (plan->accounting().reservedBytes == stablePlan);
        CHECK (plan->accounting().constructionPeakReservedBytes == replacePlan);
        --limit.maxReservedBytes;
        CHECK (buildMusicalAudioChannelPlan (plan->request(), limit, plan).status == Status::ResourceLimit);
        CHECK_FALSE (plan);

        constexpr std::uint64_t distinctRequests = clockBytes + 2u * (keyBytes + requestBytes + planBytes);
        limit.maxReservedBytes = distinctRequests;
        const auto requestA = requestFor (fixture, limit);
        const auto requestB = requestFor (fixture, limit);
        REQUIRE (*requestA == *requestB);
        REQUIRE (requestA.get() != requestB.get());
        plan = planFor (requestA, limit);
        REQUIRE (buildMusicalAudioChannelPlan (requestB, limit, plan).status == Status::Ok);
        CHECK (plan->request().get() == requestB.get());
        CHECK (plan->accounting().constructionPeakReservedBytes == distinctRequests);
        plan = planFor (requestA, limit);
        --limit.maxReservedBytes;
        CHECK (buildMusicalAudioChannelPlan (requestB, limit, plan).status == Status::ResourceLimit);
        CHECK_FALSE (plan);

        constexpr std::uint64_t distinctClocks = 2u * (clockBytes + keyBytes + requestBytes);
        limit.maxReservedBytes = distinctClocks;
        auto otherClock = fixture;
        otherClock.window = windowFor (clockFor ({ { 0, 120.0 } }));
        REQUIRE (fixture.window.clock().get() != otherClock.window.clock().get());
        request = requestFor (fixture, limit);
        REQUIRE (makeRequest (otherClock, request, {}, limit).status == Status::Ok);
        CHECK (request->accounting().constructionPeakReservedBytes == distinctClocks);
        request = requestFor (fixture, limit);
        --limit.maxReservedBytes;
        CHECK (makeRequest (otherClock, request, {}, limit).status == Status::ResourceLimit);
        CHECK_FALSE (request);
    }
}
