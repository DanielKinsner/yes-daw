// YES DAW - ADR-0058 cp1 gates through the model: the export job runs off the message thread over a snapshot it owns.
//
// With the job held at its latch mid-render: the UI tick services it (Rendering, progress inside the render half),
// an edit, an import, an undo and a recording commit all complete, and the released job's file still equals the
// export made before any of them; a second export is refused with its reason; Cancel ends it with no file and no
// count; opening another project retires it silently (the new project's status line is untouched); destroying the
// model with a held job returns.

#include "io/WavFile.h"
#include "ui/MainComponentInternal.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

using yesdaw::ui::UiActionId;

namespace {

std::filesystem::path exportScratch (const std::string& label)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-export-" + label + "-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

void writeTone (const std::filesystem::path& path, std::size_t frames, float level)
{
    std::vector<float> samples (frames);
    for (std::size_t i = 0; i < frames; ++i)
        samples[i] = level * static_cast<float> ((i % 100u) < 50u ? 1.0 : -1.0);
    REQUIRE (yesdaw::io::writeFloat32WavFile (path, yesdaw::engine::SampleRate { 48'000.0 }, 1, samples.size(), samples).ok());
}

std::string bytesOf (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    return std::string ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
}

void import (yesdaw::ui::UiAppModel& model, const std::filesystem::path& path)
{
    yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (path);
    REQUIRE (decoded.decoded.has_value());
    REQUIRE (model.importAudioFile (path, std::move (*decoded.decoded)).ok());
}

void waitHeld (const yesdaw::engine::OfflineRenderLatch& latch)
{
    for (int i = 0; i < 20'000 && ! latch.held.load(); ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    REQUIRE (latch.held.load());
}

// A model with a project holding a few seconds of audio on one track.
struct ExportModel
{
    std::filesystem::path directory;
    yesdaw::ui::UiAppModel model;

    explicit ExportModel (const std::string& label) : directory (exportScratch (label))
    {
        REQUIRE (model.createProjectBundle (directory / "song.yesdaw").ok());
        writeTone (directory / "song.wav", 240'000, 0.3f);   // five seconds
        import (model, directory / "song.wav");
    }
};

} // namespace

TEST_CASE ("ADR-0058 a held export job leaves the message thread free and its file owns its snapshot", "[export-job]")
{
    ExportModel f ("snapshot");
    const auto reference = f.directory / "reference.wav";
    REQUIRE (f.model.exportAudioFile (reference).dispatched);   // the synchronous export of the same project
    REQUIRE (f.model.context().audioExportCount == 1);

    yesdaw::engine::OfflineRenderLatch latch;
    latch.holdAfterFrames = 48'000;
    f.model.setExportLatchForTest (&latch);
    const auto exported = f.directory / "job.wav";
    REQUIRE (f.model.startAudioExport (exported).dispatched);
    waitHeld (latch);

    // The UI tick services it: still Rendering, progress inside the render half.
    for (int tick = 0; tick < 5; ++tick)
        f.model.serviceExport();
    REQUIRE (f.model.exportRunning());
    REQUIRE (f.model.context().audioExportInProgress);
    REQUIRE (f.model.context().audioExportProgressPercent > 0);
    REQUIRE (f.model.context().audioExportProgressPercent < 50);

    // A second export is refused with its reason.
    const auto second = f.model.startAudioExport (f.directory / "second.wav");
    REQUIRE_FALSE (second.dispatched);
    REQUIRE (f.model.statusLineText().find ("an export is already running") != std::string::npos);

    // Edits, an import, an undo and a recording commit all complete while the job is mid-render.
    REQUIRE (f.model.selectTimelineClip (f.model.project().clips.at (0).id));
    REQUIRE (f.model.setSelectedTimelineClipGain (0.5f).dispatched);
    writeTone (f.directory / "late.wav", 96'000, 0.5f);
    import (f.model, f.directory / "late.wav");
    REQUIRE (f.model.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (f.model.dispatch (UiActionId::DeviceSelectTestAudio).dispatched);
    REQUIRE (f.model.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (f.model.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);
    REQUIRE (f.model.dispatch (UiActionId::TransportRecord).dispatched);
    REQUIRE (f.model.exportRunning());
    REQUIRE_FALSE (std::filesystem::exists (exported));

    // Released: the job's file is the export of the project as it was when the job started.
    latch.released.store (true);
    f.model.waitForExport();
    REQUIRE_FALSE (f.model.exportRunning());
    REQUIRE (f.model.context().audioExportCount == 2);
    REQUIRE (f.model.context().audioExportProgressPercent == 100);
    REQUIRE_FALSE (f.model.context().audioExportInProgress);
    REQUIRE (bytesOf (exported) == bytesOf (reference));
    f.model.setExportLatchForTest (nullptr);
}

TEST_CASE ("ADR-0058 Cancel ends a running export with no file and no count", "[export-job]")
{
    ExportModel f ("cancel");
    yesdaw::engine::OfflineRenderLatch latch;
    latch.holdAfterFrames = 48'000;
    f.model.setExportLatchForTest (&latch);
    const auto destination = f.directory / "cancelled.wav";
    REQUIRE (f.model.startAudioExport (destination).dispatched);
    waitHeld (latch);
    REQUIRE (f.model.dispatch (UiActionId::ProjectExportAudioCancel).dispatched);
    REQUIRE (f.model.context().audioExportCancelRequested);
    REQUIRE (f.model.context().audioExportInProgress);   // "Cancelling…" until the job stops
    f.model.waitForExport();
    REQUIRE_FALSE (f.model.exportRunning());
    REQUIRE_FALSE (f.model.context().audioExportInProgress);
    REQUIRE (f.model.context().audioExportCount == 0);
    REQUIRE (f.model.context().audioExportCancelCount == 1);
    REQUIRE_FALSE (std::filesystem::exists (destination));
    REQUIRE (f.model.statusLineText() == "Export cancelled");
    REQUIRE_FALSE (f.model.statusLineIsError());

    // With no job running, Cancel (Esc) keeps its Pointer meaning.
    REQUIRE (f.model.dispatch (UiActionId::ProjectExportAudioCancel).dispatched);
    REQUIRE (f.model.context().audioExportCancelCount == 1);
}

TEST_CASE ("ADR-0058 opening another project retires a running export silently; destroying the model joins it",
           "[export-job]")
{
    yesdaw::engine::OfflineRenderLatch latch;
    latch.holdAfterFrames = 48'000;
    yesdaw::engine::OfflineRenderLatch second;   // outlives the model (its job's worker reads it until joined)
    second.holdAfterFrames = 0;
    std::filesystem::path destination;
    {
        ExportModel f ("stale");
        f.model.setExportLatchForTest (&latch);
        destination = f.directory / "stale.wav";
        REQUIRE (f.model.startAudioExport (destination).dispatched);
        waitHeld (latch);

        REQUIRE (f.model.createProjectBundle (f.directory / "replacement.yesdaw").ok());
        REQUIRE_FALSE (f.model.exportRunning());
        REQUIRE_FALSE (f.model.context().audioExportInProgress);
        REQUIRE (f.model.retiringExportCount() == 1u);
        const std::string statusBefore = f.model.statusLineText();
        for (int i = 0; i < 20'000 && f.model.retiringExportCount() > 0u; ++i)
        {
            f.model.serviceExport();
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
        REQUIRE (f.model.retiringExportCount() == 0u);
        REQUIRE (f.model.statusLineText() == statusBefore);   // the stale job reports nothing
        REQUIRE (f.model.context().audioExportCount == 0);
        REQUIRE_FALSE (std::filesystem::exists (destination));

        // A held job when the model goes away: cancelled and joined by its destructor.
        f.model.setExportLatchForTest (&second);
        import (f.model, f.directory / "song.wav");
        REQUIRE (f.model.startAudioExport (f.directory / "at-exit.wav").dispatched);
        waitHeld (second);
    }   // ~UiAppModel returns
    REQUIRE_FALSE (std::filesystem::exists (destination));
}
