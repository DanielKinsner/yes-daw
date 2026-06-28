// YES DAW - H5 recording spine.
//
// Audio callback input is copied into a bounded SPSC FIFO on the audio thread. A writer thread drains
// those chunks to disk. Timeline placement is derived from the device input frame minus the selected
// latency model, so a looped-back click lands back on its original project frame.

#pragma once

#include "engine/Node.h"
#include "rt/RtHot.h"

#include "choc/containers/choc_SingleReaderSingleWriterFIFO.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <iterator>
#include <span>
#include <vector>

namespace yesdaw::engine {

constexpr std::uint32_t kRecordingFileVersion = 1;
constexpr int kMaxRecordingChannels = 2;
constexpr int kMaxRecordingFramesPerChunk = 256;

struct RecordingLatencyModel
{
    std::int64_t inputLatencyFrames = 0;
    std::int64_t outputLatencyFrames = 0;
    bool includeOutputLatency = true;

    [[nodiscard]] bool isValid() const noexcept
    {
        return inputLatencyFrames >= 0 && outputLatencyFrames >= 0;
    }

    [[nodiscard]] bool compensatedLatencyFrames (std::int64_t& out) const noexcept
    {
        out = 0;
        if (! isValid())
            return false;

        if (includeOutputLatency
            && inputLatencyFrames > std::numeric_limits<std::int64_t>::max() - outputLatencyFrames)
            return false;

        out = includeOutputLatency ? inputLatencyFrames + outputLatencyFrames : inputLatencyFrames;
        return true;
    }
};

struct RecordingWindow
{
    std::int64_t punchStartFrame = 0;
    std::int64_t punchEndFrame = std::numeric_limits<std::int64_t>::max();
    bool loopEnabled = false;
    std::int64_t loopStartFrame = 0;
    std::int64_t loopEndFrame = 0;
    std::uint32_t maxLoopTakes = 0; // 0 = unlimited; tests use a finite count.

    [[nodiscard]] bool isValid() const noexcept
    {
        if (punchStartFrame < 0 || punchEndFrame <= punchStartFrame)
            return false;

        if (! loopEnabled)
            return true;

        return loopStartFrame >= 0 && loopEndFrame > loopStartFrame;
    }
};

struct RecordingConfig
{
    double sampleRateHz = 48000.0;
    int channels = 1;
    RecordingLatencyModel latency;
    RecordingWindow window;

    [[nodiscard]] bool isValid() const noexcept
    {
        return sampleRateHz > 0.0
            && channels > 0
            && channels <= kMaxRecordingChannels
            && latency.isValid()
            && window.isValid();
    }
};

struct RecordingChunk
{
    std::int64_t timelineStartFrame = 0;
    std::uint32_t frameCount = 0;
    std::uint16_t channels = 0;
    std::uint32_t takeOrdinal = 0;
    std::array<float, static_cast<std::size_t> (kMaxRecordingFramesPerChunk * kMaxRecordingChannels)> samples {};

    void clear() noexcept
    {
        timelineStartFrame = 0;
        frameCount = 0;
        channels = 0;
        takeOrdinal = 0;
    }
};

struct RecordingCaptureResult
{
    std::uint32_t framesAccepted = 0;
    std::uint32_t framesDropped = 0;
    std::uint32_t chunksPushed = 0;
    bool inputInvalid = false;
    bool fifoFull = false;
};

class RecordingChunkFifo final
{
public:
    explicit RecordingChunkFifo (std::uint32_t capacity = 64) { fifo_.reset (capacity); }

    [[nodiscard]] bool push (const RecordingChunk& chunk) noexcept { return fifo_.push (chunk); }
    [[nodiscard]] bool pop (RecordingChunk& chunk) noexcept { return fifo_.pop (chunk); }
    [[nodiscard]] std::uint32_t usedSlots() const noexcept { return fifo_.getUsedSlots(); }

private:
    choc::fifo::SingleReaderSingleWriterFIFO<RecordingChunk> fifo_;
};

namespace detail {

[[nodiscard]] inline bool writeBytes (std::ofstream& out, const void* data, std::size_t bytes)
{
    out.write (static_cast<const char*> (data), static_cast<std::streamsize> (bytes));
    return static_cast<bool> (out);
}

template <typename T>
[[nodiscard]] inline bool writePod (std::ofstream& out, const T& value)
{
    return writeBytes (out, &value, sizeof (T));
}

template <typename T>
[[nodiscard]] inline bool readPod (std::ifstream& in, T& value)
{
    in.read (reinterpret_cast<char*> (&value), static_cast<std::streamsize> (sizeof (T)));
    return static_cast<bool> (in);
}

[[nodiscard]] inline bool normalizeRecordingFrame (const RecordingWindow& window,
                                                   std::int64_t compensatedFrame,
                                                   std::int64_t& timelineFrame,
                                                   std::uint32_t& takeOrdinal) noexcept
{
    timelineFrame = 0;
    takeOrdinal = 0;

    if (! window.isValid())
        return false;

    if (! window.loopEnabled)
    {
        if (compensatedFrame < window.punchStartFrame || compensatedFrame >= window.punchEndFrame)
            return false;

        timelineFrame = compensatedFrame;
        return true;
    }

    if (compensatedFrame < window.loopStartFrame)
        return false;

    const std::int64_t loopLength = window.loopEndFrame - window.loopStartFrame;
    const std::int64_t relative = compensatedFrame - window.loopStartFrame;
    const std::int64_t loopIndex = relative / loopLength;
    if (loopIndex < 0 || loopIndex > static_cast<std::int64_t> (std::numeric_limits<std::uint32_t>::max()))
        return false;

    if (window.maxLoopTakes > 0 && static_cast<std::uint32_t> (loopIndex) >= window.maxLoopTakes)
        return false;

    const std::int64_t loopFrame = window.loopStartFrame + (relative % loopLength);
    if (loopFrame < window.punchStartFrame || loopFrame >= window.punchEndFrame)
        return false;

    timelineFrame = loopFrame;
    takeOrdinal = static_cast<std::uint32_t> (loopIndex);
    return true;
}

} // namespace detail

[[nodiscard]] inline bool mapDeviceInputFrameToRecordingFrame (const RecordingConfig& config,
                                                               std::int64_t deviceInputFrame,
                                                               std::int64_t& timelineFrame,
                                                               std::uint32_t& takeOrdinal) noexcept
{
    std::int64_t latency = 0;
    if (! config.isValid() || ! config.latency.compensatedLatencyFrames (latency))
        return false;

    const std::int64_t compensatedFrame = deviceInputFrame - latency;
    return detail::normalizeRecordingFrame (config.window, compensatedFrame, timelineFrame, takeOrdinal);
}

[[nodiscard]] inline RecordingCaptureResult captureRecordingInputBlock (
    RecordingChunkFifo& fifo,
    const RecordingConfig& config,
    std::int64_t deviceInputStartFrame,
    const float* const* inputChannels,
    int inputChannelCount,
    int numFrames) noexcept YESDAW_RT_HOT
{
    RecordingCaptureResult result;

    if (! config.isValid()
        || inputChannels == nullptr
        || inputChannelCount != config.channels
        || numFrames < 0)
    {
        result.inputInvalid = true;
        return result;
    }

    for (int c = 0; c < inputChannelCount; ++c)
    {
        if (inputChannels[c] == nullptr)
        {
            result.inputInvalid = true;
            return result;
        }
    }

    RecordingChunk chunk;
    const auto flush = [&] () noexcept
    {
        if (chunk.frameCount == 0)
            return;

        if (fifo.push (chunk))
        {
            ++result.chunksPushed;
        }
        else
        {
            result.fifoFull = true;
            result.framesDropped += chunk.frameCount;
        }

        chunk.clear();
    };

    for (int frame = 0; frame < numFrames; ++frame)
    {
        std::int64_t timelineFrame = 0;
        std::uint32_t takeOrdinal = 0;
        if (! mapDeviceInputFrameToRecordingFrame (config,
                                                   deviceInputStartFrame + static_cast<std::int64_t> (frame),
                                                   timelineFrame,
                                                   takeOrdinal))
            continue;

        const bool startsNewChunk =
            chunk.frameCount == 0
            || chunk.channels != static_cast<std::uint16_t> (config.channels)
            || chunk.takeOrdinal != takeOrdinal
            || timelineFrame != chunk.timelineStartFrame + static_cast<std::int64_t> (chunk.frameCount)
            || chunk.frameCount >= static_cast<std::uint32_t> (kMaxRecordingFramesPerChunk);

        if (startsNewChunk)
        {
            flush();
            chunk.timelineStartFrame = timelineFrame;
            chunk.channels = static_cast<std::uint16_t> (config.channels);
            chunk.takeOrdinal = takeOrdinal;
        }

        const std::size_t dstFrame = static_cast<std::size_t> (chunk.frameCount);
        for (int c = 0; c < config.channels; ++c)
            chunk.samples[dstFrame * static_cast<std::size_t> (config.channels) + static_cast<std::size_t> (c)] =
                inputChannels[c][frame];

        ++chunk.frameCount;
        ++result.framesAccepted;
    }

    flush();
    return result;
}

class RecordingTakeFileWriter final
{
public:
    [[nodiscard]] bool open (const std::filesystem::path& path, const RecordingConfig& config)
    {
        if (! config.isValid())
            return false;

        out_.open (path, std::ios::binary | std::ios::trunc);
        if (! out_)
            return false;

        static constexpr char magic[8] = { 'Y', 'S', 'D', 'W', 'R', 'E', 'C', '1' };
        const std::uint32_t version = kRecordingFileVersion;
        const std::uint32_t channels = static_cast<std::uint32_t> (config.channels);
        ok_ = detail::writeBytes (out_, magic, sizeof (magic))
           && detail::writePod (out_, version)
           && detail::writePod (out_, config.sampleRateHz)
           && detail::writePod (out_, channels);
        return ok_;
    }

    [[nodiscard]] bool writeChunk (const RecordingChunk& chunk)
    {
        if (! ok_
            || chunk.frameCount == 0
            || chunk.channels == 0
            || chunk.channels > kMaxRecordingChannels
            || chunk.frameCount > kMaxRecordingFramesPerChunk)
        {
            ok_ = false;
            return false;
        }

        const std::uint32_t channels = chunk.channels;
        ok_ = detail::writePod (out_, chunk.takeOrdinal)
           && detail::writePod (out_, chunk.timelineStartFrame)
           && detail::writePod (out_, chunk.frameCount)
           && detail::writePod (out_, channels)
           && detail::writeBytes (
               out_,
               chunk.samples.data(),
               static_cast<std::size_t> (chunk.frameCount) * static_cast<std::size_t> (channels) * sizeof (float));
        return ok_;
    }

    [[nodiscard]] bool drain (RecordingChunkFifo& fifo)
    {
        RecordingChunk chunk;
        while (fifo.pop (chunk))
            if (! writeChunk (chunk))
                return false;

        return ok_;
    }

    [[nodiscard]] bool close()
    {
        if (out_.is_open())
            out_.close();
        return ok_;
    }

private:
    std::ofstream out_;
    bool ok_ = false;
};

enum class RecordingTakeFileStatus : std::uint8_t
{
    Ok = 0,
    OpenFailed,
    FormatInvalid
};

struct RecordingTakeFile
{
    double sampleRateHz = 0.0;
    std::uint32_t channels = 0;
    std::vector<RecordingChunk> chunks;
};

struct RecordingTakeFileReadResult
{
    RecordingTakeFileStatus status = RecordingTakeFileStatus::Ok;
    RecordingTakeFile file;
};

[[nodiscard]] inline RecordingTakeFileReadResult readRecordingTakeFile (const std::filesystem::path& path)
{
    RecordingTakeFileReadResult result;

    std::ifstream in (path, std::ios::binary);
    if (! in)
    {
        result.status = RecordingTakeFileStatus::OpenFailed;
        return result;
    }

    char magic[8] {};
    std::uint32_t version = 0;
    std::uint32_t channels = 0;
    if (! detail::readPod (in, magic)
        || ! detail::readPod (in, version)
        || ! detail::readPod (in, result.file.sampleRateHz)
        || ! detail::readPod (in, channels))
    {
        result.status = RecordingTakeFileStatus::FormatInvalid;
        return result;
    }

    static constexpr char expected[8] = { 'Y', 'S', 'D', 'W', 'R', 'E', 'C', '1' };
    if (! std::equal (std::begin (magic), std::end (magic), std::begin (expected))
        || version != kRecordingFileVersion
        || channels == 0
        || channels > kMaxRecordingChannels
        || result.file.sampleRateHz <= 0.0)
    {
        result.status = RecordingTakeFileStatus::FormatInvalid;
        return result;
    }

    result.file.channels = channels;

    while (in.peek() != std::char_traits<char>::eof())
    {
        RecordingChunk chunk;
        std::uint32_t fileChannels = 0;
        if (! detail::readPod (in, chunk.takeOrdinal)
            || ! detail::readPod (in, chunk.timelineStartFrame)
            || ! detail::readPod (in, chunk.frameCount)
            || ! detail::readPod (in, fileChannels))
        {
            result.status = RecordingTakeFileStatus::FormatInvalid;
            return result;
        }

        if (fileChannels == 0 || fileChannels > kMaxRecordingChannels
            || fileChannels != channels
            || chunk.frameCount == 0
            || chunk.frameCount > kMaxRecordingFramesPerChunk)
        {
            result.status = RecordingTakeFileStatus::FormatInvalid;
            return result;
        }

        chunk.channels = static_cast<std::uint16_t> (fileChannels);
        const std::size_t sampleCount =
            static_cast<std::size_t> (chunk.frameCount) * static_cast<std::size_t> (chunk.channels);
        in.read (reinterpret_cast<char*> (chunk.samples.data()),
                 static_cast<std::streamsize> (sampleCount * sizeof (float)));
        if (! in)
        {
            result.status = RecordingTakeFileStatus::FormatInvalid;
            return result;
        }

        result.file.chunks.push_back (chunk);
    }

    result.status = RecordingTakeFileStatus::Ok;
    return result;
}

[[nodiscard]] inline bool findRecordedSample (const RecordingTakeFile& file,
                                              std::uint32_t takeOrdinal,
                                              std::int64_t timelineFrame,
                                              std::uint32_t channel,
                                              float& out) noexcept
{
    out = 0.0f;
    if (channel >= file.channels)
        return false;

    for (const RecordingChunk& chunk : file.chunks)
    {
        if (chunk.takeOrdinal != takeOrdinal
            || channel >= chunk.channels
            || timelineFrame < chunk.timelineStartFrame
            || timelineFrame >= chunk.timelineStartFrame + static_cast<std::int64_t> (chunk.frameCount))
            continue;

        const std::size_t local = static_cast<std::size_t> (timelineFrame - chunk.timelineStartFrame);
        out = chunk.samples[local * static_cast<std::size_t> (chunk.channels) + channel];
        return true;
    }

    return false;
}

struct RecordingCompSegment
{
    std::uint32_t takeOrdinal = 0;
    std::int64_t timelineStartFrame = 0;
    std::uint32_t frameCount = 0;
};

[[nodiscard]] inline bool renderRecordingComp (const RecordingTakeFile& file,
                                               std::span<const RecordingCompSegment> segments,
                                               std::vector<float>& out)
{
    out.clear();
    if (file.channels == 0 || file.channels > kMaxRecordingChannels)
        return false;

    for (const RecordingCompSegment& segment : segments)
    {
        const std::size_t oldSize = out.size();
        out.resize (oldSize + static_cast<std::size_t> (segment.frameCount) * file.channels, 0.0f);
        for (std::uint32_t frame = 0; frame < segment.frameCount; ++frame)
        {
            for (std::uint32_t channel = 0; channel < file.channels; ++channel)
            {
                float sample = 0.0f;
                (void) findRecordedSample (file,
                                           segment.takeOrdinal,
                                           segment.timelineStartFrame + static_cast<std::int64_t> (frame),
                                           channel,
                                           sample);
                out[oldSize + static_cast<std::size_t> (frame) * file.channels + channel] = sample;
            }
        }
    }

    return true;
}

struct IncomingRecordedMidiEvent
{
    std::int64_t deviceInputFrame = 0;
    Event event;
};

struct RecordedMidiEvent
{
    std::int64_t timelineFrame = 0;
    std::uint32_t takeOrdinal = 0;
    Event event;
};

enum class RecordingMidiStatus : std::uint8_t
{
    Ok = 0,
    InvalidInput,
    OutputTooSmall
};

struct RecordingMidiResult
{
    RecordingMidiStatus status = RecordingMidiStatus::Ok;
    std::size_t eventsWritten = 0;
};

[[nodiscard]] inline RecordingMidiResult recordMidiEventsToTimeline (
    const RecordingConfig& config,
    std::span<const IncomingRecordedMidiEvent> incoming,
    std::span<RecordedMidiEvent> out) noexcept
{
    RecordingMidiResult result;
    if (! config.isValid())
    {
        result.status = RecordingMidiStatus::InvalidInput;
        return result;
    }

    for (const IncomingRecordedMidiEvent& event : incoming)
    {
        std::int64_t timelineFrame = 0;
        std::uint32_t takeOrdinal = 0;
        if (! mapDeviceInputFrameToRecordingFrame (config, event.deviceInputFrame, timelineFrame, takeOrdinal))
            continue;

        if (result.eventsWritten >= out.size())
        {
            result.status = RecordingMidiStatus::OutputTooSmall;
            return result;
        }

        out[result.eventsWritten++] = RecordedMidiEvent { timelineFrame, takeOrdinal, event.event };
    }

    return result;
}

} // namespace yesdaw::engine
