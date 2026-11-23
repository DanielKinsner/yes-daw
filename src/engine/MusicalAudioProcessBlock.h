// Control-side input and output for one call on the Clip's persistent Signalsmith instance.
#pragma once

#include "engine/MusicalAudioChannelPlan.h"
#include "signalsmith-stretch.h"

#include <bit>
#include <cstdint>
#include <span>

namespace yesdaw::engine::musical_process_detail {

using Stretcher = signalsmith::stretch::SignalsmithStretch<float>;
static_assert (Stretcher::version[0] == 1 && Stretcher::version[1] == 1 && Stretcher::version[2] == 0);

// The real fast-return copies an input operand directly to its output. Keeping its type
// until assignment distinguishes this from a synthesized float, even when both are zero.
struct InputSample
{
    float value;
    operator float() const noexcept { return value; }
};

struct Input
{
    const MusicalAudioChannelPlan& plan;
    std::uint64_t begin;
    std::uint64_t& signalEnd;

    struct Channel
    {
        const Input& input;
        std::uint16_t channel;

        InputSample operator[] (int index) const noexcept
        {
            const auto frames = input.plan.request()->mappedWindow().srcLen;
            const auto offset = static_cast<std::uint64_t> (index);
            float sample = 0.0f;
            if (input.begin < frames && offset < frames - input.begin)
                (void) input.plan.physicalSample (channel, input.begin + offset, sample);
            if ((std::bit_cast<std::uint32_t> (sample) & 0x7fffffffu) != 0)
                input.signalEnd = std::max (input.signalEnd, input.begin + offset + 1);
            return { sample };
        }
    };

    Channel operator[] (int channel) const noexcept { return { *this, static_cast<std::uint16_t> (channel) }; }
};

struct Output
{
    std::span<float> samples;
    int channels;
    bool copiedInput = false;
    int channelsWritten = 0;
    bool invalidChannel = false;

    struct Sample
    {
        float& destination;
        bool& copied;

        void operator= (float sample) const noexcept { destination = sample; }
        void operator= (InputSample sample) const noexcept
        {
            copied = true;
            destination = sample.value;
        }
    };

    struct Channel
    {
        Output& output;
        int channel;

        Sample operator[] (int frame) const noexcept
        {
            return { output.samples[static_cast<std::size_t> (frame * output.channels + channel)],
                     output.copiedInput };
        }
    };

    Channel operator[] (int channel) noexcept
    {
        if (channel >= channels)
        {
            // Vendor channel count is private. Discard this working block on mismatch.
            invalidChannel = true;
            return { *this, 0 };
        }
        channelsWritten |= 1 << channel;
        return { *this, channel };
    }
};

enum class BlockStatus { Processed, ExactZeroFastReturn, MusicalQuietPathUnsupported, InvalidBlock };

// Called only during preparation. The caller configures its one instance with plan.physicalChannels(),
// seeks it once, and supplies private working PCM. A quiet refusal discards that preparation generation.
inline BlockStatus processBlock (Stretcher& stretch, const MusicalAudioChannelPlan& plan,
                                 std::uint64_t sourceBegin, int inputFrames, int outputFrames,
                                 std::span<float> working, std::uint64_t* lastSignalEnd = nullptr)
{
    if (inputFrames <= 0 || inputFrames > 2048 || outputFrames <= 0 || outputFrames > 2048
        || working.size() != static_cast<std::size_t> (outputFrames * plan.physicalChannels()))
        return BlockStatus::InvalidBlock;

    std::uint64_t signalEnd = 0;
    Input input { plan, sourceBegin, signalEnd };
    Output output { working, plan.physicalChannels() };
    stretch.process (input, inputFrames, output, outputFrames);
    if (lastSignalEnd != nullptr) *lastSignalEnd = std::max (*lastSignalEnd, signalEnd);
    if (output.invalidChannel || output.channelsWritten != ((1 << plan.physicalChannels()) - 1))
        return BlockStatus::InvalidBlock;
    if (! output.copiedInput) return BlockStatus::Processed;
    return signalEnd != 0 ? BlockStatus::MusicalQuietPathUnsupported : BlockStatus::ExactZeroFastReturn;
}

} // namespace yesdaw::engine::musical_process_detail
