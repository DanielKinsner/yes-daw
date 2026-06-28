// YES DAW - H5 recording gate.
//
// Deterministic loopback checks for the recording spine: audio callback input enters a bounded FIFO,
// a writer thread drains it to disk, and the recorded take aligns back to the click reference after
// non-zero input + output latency compensation.

#include "engine/Recording.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using Catch::Approx;
using yesdaw::engine::Event;
using yesdaw::engine::EventType;
using yesdaw::engine::IncomingRecordedMidiEvent;
using yesdaw::engine::RecordingChunkFifo;
using yesdaw::engine::RecordingCompSegment;
using yesdaw::engine::RecordingConfig;
using yesdaw::engine::RecordingMidiStatus;
using yesdaw::engine::RecordingTakeFileStatus;
using yesdaw::engine::RecordingTakeFileWriter;
using yesdaw::engine::RecordedMidiEvent;
using yesdaw::engine::captureRecordingInputBlock;
using yesdaw::engine::findRecordedSample;
using yesdaw::engine::mapDeviceInputFrameToRecordingFrame;
using yesdaw::engine::readRecordingTakeFile;
using yesdaw::engine::recordMidiEventsToTimeline;
using yesdaw::engine::renderRecordingComp;

namespace {

std::filesystem::path tempTakePath (const char* name)
{
    static std::atomic<unsigned> counter { 0 };
    return std::filesystem::temp_directory_path()
         / (std::string { name } + "_" + std::to_string (counter.fetch_add (1)) + ".ysdtake");
}

struct CaptureRunResult
{
    bool writerOk = false;
    std::uint32_t framesAccepted = 0;
    std::uint32_t framesDropped = 0;
};

CaptureRunResult captureSyntheticInputToDisk (const RecordingConfig& config,
                                               const std::filesystem::path& path,
                                               const std::vector<std::int64_t>& impulseDeviceFrames,
                                               const std::vector<float>& impulseAmplitudes,
                                               std::int64_t totalDeviceFrames)
{
    REQUIRE (impulseDeviceFrames.size() == impulseAmplitudes.size());

    RecordingChunkFifo fifo { 64 };
    RecordingTakeFileWriter writer;
    REQUIRE (writer.open (path, config));

    std::atomic<bool> done { false };
    bool writerOk = true;
    std::thread writerThread (
        [&]
        {
            while (! done.load (std::memory_order_acquire))
            {
                writerOk = writer.drain (fifo) && writerOk;
                std::this_thread::yield();
            }

            writerOk = writer.drain (fifo) && writer.close() && writerOk;
        });

    CaptureRunResult result;
    constexpr int kBlock = 37;
    for (std::int64_t deviceStart = 0; deviceStart < totalDeviceFrames; deviceStart += kBlock)
    {
        const int frames = static_cast<int> (std::min<std::int64_t> (kBlock, totalDeviceFrames - deviceStart));
        std::vector<float> input (static_cast<std::size_t> (frames), 0.0f);
        for (std::size_t i = 0; i < impulseDeviceFrames.size(); ++i)
        {
            const std::int64_t impulse = impulseDeviceFrames[i];
            if (impulse >= deviceStart && impulse < deviceStart + frames)
                input[static_cast<std::size_t> (impulse - deviceStart)] = impulseAmplitudes[i];
        }

        const float* channels[1] = { input.data() };
        const auto capture = captureRecordingInputBlock (fifo, config, deviceStart, channels, 1, frames);
        REQUIRE_FALSE (capture.inputInvalid);
        result.framesAccepted += capture.framesAccepted;
        result.framesDropped += capture.framesDropped;
    }

    done.store (true, std::memory_order_release);
    writerThread.join();
    result.writerOk = writerOk;
    return result;
}

} // namespace

TEST_CASE ("recorded take aligns to a click reference after non-trivial round-trip latency", "[recording][h5][gate]")
{
    const std::filesystem::path path = tempTakePath ("yesdaw_recording_align");
    std::filesystem::remove (path);

    RecordingConfig config;
    config.channels = 1;
    config.sampleRateHz = 48000.0;
    config.latency.inputLatencyFrames = 37;
    config.latency.outputLatencyFrames = 73;
    config.latency.includeOutputLatency = true;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 192;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));
    REQUIRE (roundTrip > 1);

    constexpr std::int64_t kClickFrame = 96;
    const std::int64_t impulseDeviceFrame = kClickFrame + roundTrip;

    std::int64_t uncompensatedFrame = 0;
    std::uint32_t uncompensatedTake = 0;
    RecordingConfig noCompensation = config;
    noCompensation.latency.inputLatencyFrames = 0;
    noCompensation.latency.outputLatencyFrames = 0;
    noCompensation.window.punchEndFrame = impulseDeviceFrame + 1;
    REQUIRE (mapDeviceInputFrameToRecordingFrame (
        noCompensation, impulseDeviceFrame, uncompensatedFrame, uncompensatedTake));
    REQUIRE (uncompensatedFrame != kClickFrame);

    const auto run = captureSyntheticInputToDisk (
        config,
        path,
        { impulseDeviceFrame },
        { 1.0f },
        config.window.punchEndFrame + roundTrip + 16);
    REQUIRE (run.writerOk);
    REQUIRE (run.framesDropped == 0u);
    REQUIRE (run.framesAccepted > 0u);

    const auto read = readRecordingTakeFile (path);
    REQUIRE (read.status == RecordingTakeFileStatus::Ok);
    REQUIRE (read.file.channels == 1u);

    float peak = 0.0f;
    std::int64_t peakFrame = -1;
    for (const auto& chunk : read.file.chunks)
    {
        for (std::uint32_t frame = 0; frame < chunk.frameCount; ++frame)
        {
            const float value = std::fabs (chunk.samples[frame]);
            if (value > peak)
            {
                peak = value;
                peakFrame = chunk.timelineStartFrame + static_cast<std::int64_t> (frame);
            }
        }
    }

    REQUIRE (peak == Approx (1.0f));
    REQUIRE (std::llabs (peakFrame - kClickFrame) <= 1);
    std::filesystem::remove (path);
}

TEST_CASE ("punch loop recording creates deterministic take ordinals and comp selection", "[recording][h5][loop][comp]")
{
    const std::filesystem::path path = tempTakePath ("yesdaw_recording_loop");
    std::filesystem::remove (path);

    RecordingConfig config;
    config.channels = 1;
    config.latency.inputLatencyFrames = 5;
    config.latency.outputLatencyFrames = 7;
    config.window.punchStartFrame = 64;
    config.window.punchEndFrame = 96;
    config.window.loopEnabled = true;
    config.window.loopStartFrame = 64;
    config.window.loopEndFrame = 96;
    config.window.maxLoopTakes = 2;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));

    constexpr std::int64_t kLoopClickFrame = 72;
    constexpr std::int64_t kLoopLength = 32;
    const std::int64_t take0DeviceFrame = kLoopClickFrame + roundTrip;
    const std::int64_t take1DeviceFrame = kLoopClickFrame + kLoopLength + roundTrip;

    const auto run = captureSyntheticInputToDisk (
        config,
        path,
        { take0DeviceFrame, take1DeviceFrame },
        { 0.25f, 0.75f },
        config.window.loopEndFrame + kLoopLength + roundTrip + 16);
    REQUIRE (run.writerOk);
    REQUIRE (run.framesDropped == 0u);

    const auto read = readRecordingTakeFile (path);
    REQUIRE (read.status == RecordingTakeFileStatus::Ok);

    float sample = 0.0f;
    REQUIRE (findRecordedSample (read.file, 0, kLoopClickFrame, 0, sample));
    REQUIRE (sample == Approx (0.25f));
    REQUIRE (findRecordedSample (read.file, 1, kLoopClickFrame, 0, sample));
    REQUIRE (sample == Approx (0.75f));

    RecordingCompSegment segment;
    segment.takeOrdinal = 1;
    segment.timelineStartFrame = config.window.loopStartFrame;
    segment.frameCount = static_cast<std::uint32_t> (config.window.loopEndFrame - config.window.loopStartFrame);

    std::vector<float> comped;
    REQUIRE (renderRecordingComp (read.file, std::span<const RecordingCompSegment> (&segment, 1), comped));
    REQUIRE (comped.size() == static_cast<std::size_t> (segment.frameCount));
    REQUIRE (comped[static_cast<std::size_t> (kLoopClickFrame - config.window.loopStartFrame)] == Approx (0.75f));
    std::filesystem::remove (path);
}

TEST_CASE ("MIDI recording uses the same latency compensation as audio recording", "[recording][h5][midi]")
{
    RecordingConfig config;
    config.channels = 1;
    config.latency.inputLatencyFrames = 4;
    config.latency.outputLatencyFrames = 6;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 256;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));

    Event event;
    event.type = EventType::NoteOn;
    event.voice.key = 60;
    event.payload.note.normalizedVelocity = 1.0;

    IncomingRecordedMidiEvent incoming;
    incoming.deviceInputFrame = 100 + roundTrip;
    incoming.event = event;

    RecordedMidiEvent out[1] {};
    const auto result = recordMidiEventsToTimeline (
        config,
        std::span<const IncomingRecordedMidiEvent> (&incoming, 1),
        std::span<RecordedMidiEvent> (out));

    REQUIRE (result.status == RecordingMidiStatus::Ok);
    REQUIRE (result.eventsWritten == 1u);
    REQUIRE (out[0].timelineFrame == 100);
    REQUIRE (out[0].takeOrdinal == 0u);
    REQUIRE (out[0].event.type == EventType::NoteOn);
    REQUIRE (out[0].event.voice.key == 60);
}
