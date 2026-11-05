// A retained terminal failure must publish its named result again after a public reset.
#include "engine/PlaybackEngine.h"
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>

TEST_CASE ("repeated click failures republish status after an explicit public reset",
           "[playback][musical-click][click-failure-replay][rtsan]")
{
    using namespace yesdaw::engine;

    // Published click law: a downbeat is a five-millisecond, decaying1500Hz sine at gain0.5.
    // Only its first32 samples are needed; no previous callback supplies an expected sample.
    std::array<std::uint32_t, 32> expectedBits {};
    bool audiblePrefix = false;
    for (std::size_t sample = 0; sample < expectedBits.size(); ++sample)
    {
        const double elapsed = static_cast<double> (sample) / 48000.0;
        const double angle = 6.283185307179586 * 1500.0 * elapsed;
        const float value = static_cast<float> (std::sin (angle) * std::exp (-600.0 * elapsed) * 0.5);
        expectedBits[sample] = std::bit_cast<std::uint32_t> (value);
        audiblePrefix = audiblePrefix || value != 0.0f;
    }
    REQUIRE (audiblePrefix);

    for (const int preparedBlock : { 128, 257 })
    {
        CAPTURE (preparedBlock);
        Project project;
        project.sampleRate = { 48000.0 };
        project.tempoMap = { { 0, std::numeric_limits<double>::denorm_min(), TempoCurve::Jump } };
        auto playback = PlaybackEngine::createTransportOnly (project, preparedBlock);
        REQUIRE (playback != nullptr);
        REQUIRE (playback->metronomePreparationStatus() == TimeLookupStatus::Ok);
        const auto retainedClock = playback->metronomeClock();
        REQUIRE (retainedClock != nullptr);

        // Establish the fixture through the public uncached cursor: one accented prefix,
        // then a named refusal, with no second onset available to explain extra PCM.
        auto cursor = retainedClock->onsetsInRange (0, 32);
        const auto opening = cursor.next();
        REQUIRE (opening.status == TimeLookupStatus::Ok);
        REQUIRE (opening.found);
        REQUIRE (opening.frame == 0);
        REQUIRE (opening.downbeat);
        const auto refusal = cursor.next();
        REQUIRE (refusal.status == TimeLookupStatus::NumericDomain);
        REQUIRE_FALSE (refusal.found);
        REQUIRE_FALSE (cursor.next().found);

        REQUIRE (playback->setLoop (0, 32));
        REQUIRE (playback->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
        REQUIRE (playback->metronomeLookupStatus() == TimeLookupStatus::Ok);
        std::array<float, 32> channelA {}, channelB {};
        float* outputs[] { channelA.data(), channelB.data() };

        // Three identical advancing loop queries establish the ordinary warm path.
        // The last two iterations reset only the public result before demanding it anew.
        for (int invocation = 0; invocation < 5; ++invocation)
        {
            CAPTURE (invocation);
            if (invocation >= 3)
            {
                REQUIRE (playback->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
                REQUIRE (playback->metronomeLookupStatus() == TimeLookupStatus::Ok);
            }
            channelA.fill (19.0f);
            channelB.fill (-23.0f);
            playback->processBlock (outputs, 2, 32);
            REQUIRE (playback->metronomeLookupStatus() == refusal.status);
            REQUIRE (playback->playheadFrame() == 0);
            for (std::size_t sample = 0; sample < expectedBits.size(); ++sample)
            {
                REQUIRE (std::bit_cast<std::uint32_t> (channelA[sample]) == expectedBits[sample]);
                REQUIRE (std::bit_cast<std::uint32_t> (channelB[sample]) == expectedBits[sample]);
            }
        }

        // This seek retains the same [0,32) onset query while reading the prefix from age1.
        REQUIRE (playback->locate (1));
        REQUIRE (playback->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
        REQUIRE (playback->metronomeLookupStatus() == TimeLookupStatus::Ok);
        channelA.fill (19.0f);
        channelB.fill (-23.0f);
        playback->processBlock (outputs, 2, 31);
        REQUIRE (playback->metronomeLookupStatus() == TimeLookupStatus::NumericDomain);
        REQUIRE (playback->playheadFrame() == 0);
        for (std::size_t sample = 0; sample < 31; ++sample)
        {
            REQUIRE (std::bit_cast<std::uint32_t> (channelA[sample]) == expectedBits[sample + 1]);
            REQUIRE (std::bit_cast<std::uint32_t> (channelB[sample]) == expectedBits[sample + 1]);
        }
        REQUIRE (channelA.back() == 19.0f);
        REQUIRE (channelB.back() == -23.0f);
    }
}
