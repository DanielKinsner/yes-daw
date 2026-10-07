// YES DAW - ADR-0059 gates: an Asset's decoded samples are one immutable buffer, shared by reference.
//
// The buffer is the same object across an import of another file, a recording, an undo, a refused drop and a save;
// ten clips on one Asset hold one buffer and the engine shares it (no copy); an export job holds the model's buffers
// while it runs and keeps a closed project's audio alive until it ends; closing a project frees its audio once its
// engines are reclaimed.

#include "io/WavFile.h"
#include "ui/MainComponentInternal.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using yesdaw::ui::UiActionId;

namespace {

std::filesystem::path sharingScratch (const std::string& label)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-asset-sharing-" + label + "-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

void writeTone (const std::filesystem::path& path, std::size_t frames, float level, double rate = 48'000.0)
{
    std::vector<float> samples (frames);
    for (std::size_t i = 0; i < frames; ++i)
        samples[i] = level * static_cast<float> ((i % 120u) < 60u ? 1.0 : -1.0);
    REQUIRE (yesdaw::io::writeFloat32WavFile (path, yesdaw::engine::SampleRate { rate }, 1, samples.size(), samples).ok());
}

yesdaw::ui::UiDecodedAsset decode (const std::filesystem::path& path)
{
    yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (path);
    REQUIRE (decoded.decoded.has_value());
    return std::move (*decoded.decoded);
}

const void* bufferOf (const yesdaw::ui::UiAppModel& model, yesdaw::engine::EntityId assetId)
{
    const yesdaw::ui::UiDecodedAsset* decoded = model.findDecodedAsset (assetId);
    REQUIRE (decoded != nullptr);
    return decoded->samples.get();
}

} // namespace

TEST_CASE ("ADR-0059 an Asset's buffer is the same object across imports, a recording, an undo, a refused drop and a save",
           "[asset-sharing]")
{
    const auto directory = sharingScratch ("identity");
    writeTone (directory / "first.wav", 48'000, 0.3f);
    writeTone (directory / "second.wav", 24'000, 0.2f);
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "song.yesdaw").ok());
    REQUIRE (model.importAudioFile (directory / "first.wav", decode (directory / "first.wav")).ok());
    const yesdaw::engine::EntityId first = model.project().assets.at (0).id;
    const void* const buffer = bufferOf (model, first);
    REQUIRE (buffer != nullptr);

    REQUIRE (model.importAudioFile (directory / "second.wav", decode (directory / "second.wav")).ok());
    REQUIRE (bufferOf (model, first) == buffer);

    REQUIRE (model.dispatch (UiActionId::EditUndo).dispatched);   // undo the second import
    REQUIRE (bufferOf (model, first) == buffer);

    REQUIRE (model.dispatch (UiActionId::DeviceSelectTestAudio).dispatched);   // a recording commit
    REQUIRE (model.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (model.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);
    REQUIRE (model.dispatch (UiActionId::TransportRecord).dispatched);
    REQUIRE (bufferOf (model, first) == buffer);

    // A drop that lands nothing (an unsupported rate) rolls back without touching the buffers.
    std::vector<yesdaw::ui::UiAudioImportItem> refused;
    yesdaw::ui::UiDecodedAsset odd = decode (directory / "second.wav");
    odd.sampleRate = yesdaw::engine::SampleRate { 4'000.0 };
    refused.push_back ({ directory / "second.wav", std::move (odd) });
    const yesdaw::ui::UiAudioDropResult dropped = model.importAudioFilesAt (std::move (refused), 0, 0);
    REQUIRE (dropped.landed == 0u);
    REQUIRE (bufferOf (model, first) == buffer);

    REQUIRE (model.saveProjectBundle().ok());
    REQUIRE (bufferOf (model, first) == buffer);
}

TEST_CASE ("ADR-0059 ten clips on one Asset share one buffer, and the engine holds that buffer (no copy)", "[asset-sharing]")
{
    const auto directory = sharingScratch ("ten-clips");
    writeTone (directory / "loop.wav", 12'000, 0.3f);
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "song.yesdaw").ok());
    REQUIRE (model.importAudioFile (directory / "loop.wav", decode (directory / "loop.wav")).ok());
    for (int copy = 0; copy < 9; ++copy)
    {
        REQUIRE (model.selectTimelineClip (model.project().clips.back().id));
        REQUIRE (model.dispatch (UiActionId::TimelineClipDuplicate).dispatched);
    }
    REQUIRE (model.project().clips.size() == 10u);
    REQUIRE (model.project().assets.size() == 1u);

    // The fixture memory assertion: one Asset, one buffer — the model's, the engine owners' and nothing else.
    const std::vector<const void*> buffers = model.distinctDecodedBuffersForTest();
    REQUIRE (buffers.size() == 1u);
    const yesdaw::ui::UiDecodedAsset* decoded = model.findDecodedAsset (model.project().assets.at (0).id);
    REQUIRE (decoded != nullptr);
    REQUIRE (buffers.front() == decoded->samples.get());
    // The live engine's build copied nothing: every clip reads the model's buffer.
    REQUIRE (model.engineCopiedAssetsForTest() == 0u);
    // With the peak builder done, the only holders are the model and the engine's schedules.
    for (int i = 0; i < 20'000 && model.waveformRequestsPendingForTest() > 0u; ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    REQUIRE (model.waveformRequestsPendingForTest() == 0u);
    REQUIRE (decoded->samples.use_count() > 1);

    // And it plays.
    REQUIRE (model.locatePlaybackFrame (0));
    REQUIRE (model.dispatch (UiActionId::TransportPlay).dispatched);
    const std::vector<float> rendered = model.renderPlaybackFrames (4'096, 128);
    REQUIRE (std::any_of (rendered.begin(), rendered.end(), [] (float s) { return s != 0.0f; }));
}

TEST_CASE ("ADR-0059 a cross-rate Asset has its buffer and one live view, nothing more", "[asset-sharing]")
{
    const auto directory = sharingScratch ("cross-rate");
    writeTone (directory / "cd.wav", 44'100, 0.3f, 44'100.0);
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "song.yesdaw").ok());
    REQUIRE (model.importAudioFile (directory / "cd.wav", decode (directory / "cd.wav")).ok());
    REQUIRE (model.selectTimelineClip (model.project().clips.back().id));
    REQUIRE (model.dispatch (UiActionId::TimelineClipDuplicate).dispatched);
    REQUIRE (model.distinctDecodedBuffersForTest().size() == 2u);   // the decode and its live-tier view
}

TEST_CASE ("ADR-0059 an export job holds the model's buffers; a closed project's audio lives until its job ends, then goes",
           "[asset-sharing]")
{
    const auto directory = sharingScratch ("lifetime");
    writeTone (directory / "song.wav", 96'000, 0.3f);
    std::weak_ptr<const yesdaw::engine::AssetSamples> weak;
    yesdaw::engine::OfflineRenderLatch latch;
    latch.holdAfterFrames = 4'096;
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "song.yesdaw").ok());
    REQUIRE (model.importAudioFile (directory / "song.wav", decode (directory / "song.wav")).ok());
    {
        const yesdaw::ui::UiDecodedAsset* decoded = model.findDecodedAsset (model.project().assets.at (0).id);
        REQUIRE (decoded != nullptr);
        weak = decoded->samples;
    }
    for (int i = 0; i < 20'000 && model.waveformRequestsPendingForTest() > 0u; ++i)   // no peak request holds it now
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    REQUIRE (model.waveformRequestsPendingForTest() == 0u);
    const long heldBefore = weak.use_count();

    model.setExportLatchForTest (&latch);
    REQUIRE (model.startAudioExport (directory / "mix.wav").dispatched);
    for (int i = 0; i < 20'000 && ! latch.held.load(); ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    REQUIRE (latch.held.load());
    REQUIRE (weak.use_count() > heldBefore);   // the job holds the model's buffer (a copy would not raise it)

    // Another project: the model lets go; the retiring job still holds the audio, so it is alive.
    REQUIRE (model.createProjectBundle (directory / "other.yesdaw").ok());
    model.reclaimRetiredAudioObjects();
    REQUIRE_FALSE (weak.expired());

    // The job winds down (opening the other project cancelled it); then nothing holds the closed project's audio.
    latch.released.store (true);
    for (int i = 0; i < 20'000 && model.retiringExportCount() > 0u; ++i)
    {
        model.serviceExport();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    REQUIRE (model.retiringExportCount() == 0u);
    for (int i = 0; i < 2'000 && ! weak.expired(); ++i)   // the peak builder may still hold it for a moment
    {
        model.reclaimRetiredAudioObjects();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
    REQUIRE (weak.expired());
    model.setExportLatchForTest (nullptr);
}
