// YES DAW - ADR-0056 cp2: the audition voice's device-thread read. Pure C++ + Catch2, so it runs on the RTSan leg
// too: sumAuditionVoice (YESDAW_RT_HOT) under -fsanitize=realtime proves the read never allocates, locks or makes a
// syscall.
//
// Gates: a mono voice adds to every output and a stereo voice channel to channel, from its frame 0, at unity, on
// top of what the outputs already hold; it advances across blocks and finishes exactly at its end (a partial last
// block, then nothing); a finished voice adds nothing; a stereo voice leaves outputs past the second untouched.

#include "engine/AuditionVoice.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

namespace {

void fillVoice (yesdaw::engine::AuditionVoice& voice, std::uint16_t channels, std::uint64_t frames)
{
    voice.channels = channels;
    voice.frames = frames;
    voice.interleaved.resize (static_cast<std::size_t> (frames) * channels);
    for (std::size_t i = 0; i < voice.interleaved.size(); ++i)
        voice.interleaved[i] = 0.001f * static_cast<float> (i + 1u);
}

} // namespace

TEST_CASE ("ADR-0056 a mono audition adds to every output from frame 0 and finishes at its end", "[audition]")
{
    yesdaw::engine::AuditionVoice voice;
    fillVoice (voice, 1, 100);
    std::array<std::array<float, 64>, 3> storage {};
    for (auto& channel : storage)
        channel.fill (0.25f);   // what the engine and monitoring already put there
    std::array<float*, 3> outputs { storage[0].data(), storage[1].data(), storage[2].data() };

    yesdaw::engine::sumAuditionVoice (voice, outputs.data(), 3, 64);
    for (const auto& channel : storage)
        for (std::size_t frame = 0; frame < 64; ++frame)
            REQUIRE (channel[frame] == 0.25f + voice.interleaved[frame]);
    REQUIRE (voice.position == 64u);
    REQUIRE_FALSE (voice.finished.load());

    // The last 36 frames, then silence for the rest of the block; the voice is finished.
    for (auto& channel : storage)
        channel.fill (0.0f);
    yesdaw::engine::sumAuditionVoice (voice, outputs.data(), 3, 64);
    for (std::size_t frame = 0; frame < 36; ++frame)
        REQUIRE (storage[1][frame] == voice.interleaved[64 + frame]);
    for (std::size_t frame = 36; frame < 64; ++frame)
        REQUIRE (storage[1][frame] == 0.0f);
    REQUIRE (voice.position == 100u);
    REQUIRE (voice.finished.load());

    // Finished: adds nothing.
    for (auto& channel : storage)
        channel.fill (0.5f);
    yesdaw::engine::sumAuditionVoice (voice, outputs.data(), 3, 64);
    for (const auto& channel : storage)
        for (const float sample : channel)
            REQUIRE (sample == 0.5f);
}

TEST_CASE ("ADR-0056 a stereo audition adds channel to channel and leaves outputs past the second alone", "[audition]")
{
    yesdaw::engine::AuditionVoice voice;
    fillVoice (voice, 2, 32);
    std::array<std::array<float, 32>, 3> storage {};
    std::array<float*, 3> outputs { storage[0].data(), storage[1].data(), storage[2].data() };
    yesdaw::engine::sumAuditionVoice (voice, outputs.data(), 3, 32);
    for (std::size_t frame = 0; frame < 32; ++frame)
    {
        REQUIRE (storage[0][frame] == voice.interleaved[frame * 2u]);
        REQUIRE (storage[1][frame] == voice.interleaved[frame * 2u + 1u]);
        REQUIRE (storage[2][frame] == 0.0f);
    }
    REQUIRE (voice.finished.load());

    // One output: the left channel only. No outputs at all: the index still advances (the voice keeps time).
    yesdaw::engine::AuditionVoice again;
    fillVoice (again, 2, 40);
    std::array<float, 16> mono {};
    float* single[] { mono.data() };
    yesdaw::engine::sumAuditionVoice (again, single, 1, 16);
    for (std::size_t frame = 0; frame < 16; ++frame)
        REQUIRE (mono[frame] == again.interleaved[frame * 2u]);
    yesdaw::engine::sumAuditionVoice (again, nullptr, 0, 16);
    REQUIRE (again.position == 32u);
}
