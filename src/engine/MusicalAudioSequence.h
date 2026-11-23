// Control-side call sequence for the pinned Musical stretcher. All output is private working audio.
#pragma once

#include "engine/MusicalAudioCutPlan.h"
#include "engine/MusicalAudioProcessBlock.h"

#include <algorithm>
#include <cfenv>
#include <cmath>
#include <limits>

namespace yesdaw::engine::musical_process_detail {

enum class SequenceStatus { Ok, InvalidInput, MusicalQuietPathUnsupported, MusicalBoundaryContextUnsupported,
                            GeometryRefused, UnsupportedArithmetic };

struct SequenceResult
{
    SequenceStatus status;
    MusicalAudioStatus geometry = MusicalAudioStatus::Ok;
    std::span<const float> retained {};
};

// Measure the centre of each half-open sample interval [begin, begin + W) from its leading edge.
// The vendor's analysis and synthesis windows have the same centre. An odd window places
// both half a sample earlier than an even one. Use the exact source fraction for that query.
inline SequenceResult checkCentre (const MusicalAudioWindow& window, std::int64_t sourceTwice,
                                   std::int64_t outputTwice)
{
    if (sourceTwice < 0)
    {
        // Only the first (odd-window) centre precedes the source; both centres are -1/2.
        return { sourceTwice == -1 && outputTwice == -1 ? SequenceStatus::Ok
                                                       : SequenceStatus::MusicalBoundaryContextUnsupported };
    }
    const auto denominator = 2 * window.inputFrames();
    if (static_cast<std::uint64_t> (sourceTwice) > denominator)
        return { SequenceStatus::MusicalBoundaryContextUnsupported };
    std::uint64_t ticks = 0, remainder = 0;
    musical_audio_detail::sourceQuotient (static_cast<std::uint64_t> (window.lengthTicks()),
        static_cast<std::uint64_t> (sourceTwice), denominator, ticks, remainder);
    MusicalAudioFramePosition mapped;
    const auto status = window.clock()->frameAt (
        { window.startTick() + static_cast<Tick> (ticks), remainder, remainder == 0 ? 1 : denominator }, mapped);
    if (status != MusicalAudioStatus::Ok) return { SequenceStatus::GeometryRefused, status };
    const auto residual = outputTwice - 2 * (mapped.absoluteFrame - window.startFrame());
    return { std::abs (residual) <= 4 ? SequenceStatus::Ok : SequenceStatus::MusicalBoundaryContextUnsupported };
}

// The cut plan owns contiguous logical calls. Seek supplies lookahead once. Logical input
// frame zero then starts at inputLatency in the selected source, with zero beyond its end.
// Flush the full W after D so the vendor never reflects future samples into the tail.
// Only [outputLatency, outputLatency + D) is audible.
inline SequenceResult runSequence (Stretcher& stretch, const MusicalAudioChannelPlan& channels,
                                   const MusicalAudioCutPlan& cuts, std::span<float> working)
{
    using Status = SequenceStatus;
    const auto& window = cuts.window();
    const auto& sourceWindow = channels.request()->window();
    const int block = stretch.blockSamples(), hop = stretch.intervalSamples();
    const int lookahead = stretch.inputLatency(), latency = stretch.outputLatency();
    const auto width = channels.physicalChannels();
    constexpr auto intMax = (std::numeric_limits<int>::max)();
    if (window.clock() != sourceWindow.clock() || window.startTick() != sourceWindow.startTick()
        || window.lengthTicks() != sourceWindow.lengthTicks() || window.inputFrames() != sourceWindow.inputFrames()
        || block <= 0 || hop <= 0 || hop > block || block > (intMax - hop) / 2
        || window.inputFrames() > static_cast<std::uint64_t> (intMax - block - hop)
        || window.frameCount() > intMax - block || cuts.calls().empty())
        return { Status::InvalidInput };
    if (std::fegetround() != FE_TONEAREST) return { Status::UnsupportedArithmetic };
    const auto totalSamples = static_cast<std::uint64_t> (window.frameCount() + block) * width;
    if (totalSamples != working.size()) return { Status::InvalidInput };

    std::uint64_t lastSignalEnd = 0;
    Input seek { channels, 0, lastSignalEnd };
    const auto& first = cuts.calls().front();
    stretch.reset();
    stretch.seek (seek, lookahead, static_cast<double> (first.inputFrames) / first.outputFrames);

    std::int64_t nextHop = 0;
    std::int64_t tailEnd = 0;
    std::int64_t regularInput = 0, lastAnalysis = -1;
    for (const auto& call : cuts.calls())
    {
        // The normal path retains only the last W inputs per call. With I <= W the entire
        // stream is contiguous in its W+H+1 history ring, including the previous analysis window.
        if (call.inputFrames > block) return { Status::MusicalBoundaryContextUnsupported };
        const auto begin = static_cast<std::size_t> (call.outputBegin) * width;
        const auto count = static_cast<std::size_t> (call.outputFrames) * width;
        const auto processed = processBlock (stretch, channels, call.inputBegin + static_cast<unsigned> (lookahead),
                                             call.inputFrames, call.outputFrames, working.subspan (begin, count), &lastSignalEnd);
        if (processed == BlockStatus::InvalidBlock) return { Status::InvalidInput };
        if (processed == BlockStatus::MusicalQuietPathUnsupported) return { Status::MusicalQuietPathUnsupported };
        if (processed == BlockStatus::ExactZeroFastReturn)
        {
            // Copied zeros advance the input ring but leave the STFT's hop and phase state still.
            // Before pausing its STFT the vendor clears the bands, but leaves any prior
            // overlap-add output in the ring. Accept the pause only after that signal drained.
            if (call.outputBegin < tailEnd || (lastAnalysis < 0 && lastSignalEnd != 0))
                return { Status::MusicalBoundaryContextUnsupported };
            nextHop += call.outputFrames;
            continue;
        }
        const auto end = call.outputBegin + call.outputFrames;
        for (; nextHop < end; nextHop += hop)
        {
            const auto offset = static_cast<int> (nextHop - call.outputBegin);
            const int numerator = offset * call.inputFrames;
            const int advance = (2 * numerator + call.outputFrames) / (2 * call.outputFrames);
            const float vendorOffset = static_cast<float> (offset) * call.inputFrames / call.outputFrames;
            if (std::round (vendorOffset) != static_cast<float> (advance))
                return { Status::UnsupportedArithmetic };
            const auto source = static_cast<std::int64_t> (call.inputBegin) + advance;
            // prevInputOffset advances only on ordinary process calls. A positive interval
            // means the backend really analyses this window, including the first after silence.
            if (regularInput + advance <= lastAnalysis) return { Status::MusicalBoundaryContextUnsupported };
            lastAnalysis = regularInput + advance;
            if (lastSignalEnd != 0 && source + lookahead - block < static_cast<std::int64_t> (lastSignalEnd))
                tailEnd = nextHop + block;
            const auto checked = checkCentre (window, 2 * source - block % 2, 2 * nextHop - block % 2);
            if (checked.status != Status::Ok) return checked;
        }
        regularInput += call.inputFrames;
    }

    struct Tail
    {
        std::span<float> data;
        int width;
        struct Channel
        {
            float* first;
            int stride;
            float& operator[] (int frame) const noexcept { return first[static_cast<std::size_t> (frame) * stride]; }
        };
        Channel operator[] (int channel) const noexcept { return { data.data() + channel, width }; }
    };
    Tail tail { working.subspan (static_cast<std::size_t> (window.frameCount()) * width), width };
    stretch.flush (tail, block);
    return { Status::Ok, MusicalAudioStatus::Ok, working.subspan (static_cast<std::size_t> (latency) * width,
                                                                  static_cast<std::size_t> (window.frameCount()) * width) };
}

} // namespace yesdaw::engine::musical_process_detail
