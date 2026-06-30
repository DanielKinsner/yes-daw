// YES DAW - H13 recording UX shipped-shell harness skeleton.

#include "io/WavFile.h"
#include "ui/MainComponent.h"
#include "persistence/ProjectBundle.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

using yesdaw::ui::MainComponentFileChoices;
using yesdaw::ui::MainComponentSnapshot;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiAppModel;
using yesdaw::ui::findMainComponentChildForAction;
using yesdaw::ui::snapshotMainComponent;
using yesdaw::persistence::ProjectBundleDb;

namespace {

std::filesystem::path makeTempBundlePath (std::string label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("yesdaw-recording-ux-" + std::move (label) + "-" + std::to_string (ticks) + ".yesdaw");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

std::unique_ptr<juce::Component> makeShell (MainComponentFileChoices fileChoices = {})
{
    juce::MessageManager::getInstance();
    auto shell = yesdaw::ui::createMainComponent (std::move (fileChoices));
    REQUIRE (shell != nullptr);
    REQUIRE (shell->getWidth() == 1536);
    REQUIRE (shell->getHeight() == 960);
    return shell;
}

juce::Button& requireButtonForAction (juce::Component& shell, UiActionId action)
{
    juce::Component* component = findMainComponentChildForAction (shell, action);
    REQUIRE (component != nullptr);

    auto* button = dynamic_cast<juce::Button*> (component);
    REQUIRE (button != nullptr);
    REQUIRE (button->isVisible());
    REQUIRE (button->getWidth() > 0);
    REQUIRE (button->getHeight() > 0);
    return *button;
}

void clickButton (juce::Button& button)
{
    button.triggerClick();
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

yesdaw::engine::Project readProjectSnapshot (const std::filesystem::path& bundlePath)
{
    ProjectBundleDb db;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, db).ok());

    yesdaw::engine::Project project;
    REQUIRE (db.readProjectSnapshot (project).ok());
    return project;
}

float expectedRecordedSample (std::uint64_t frame) noexcept
{
    const int phase = static_cast<int> (frame % 16u);
    return (static_cast<float> (phase) - 7.5f) / 16.0f;
}

} // namespace

TEST_CASE ("H13 recording UX harness targets shipped MainComponent recording controls",
           "[recording][ux][shell]")
{
    auto shell = makeShell();
    const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);

    REQUIRE (snapshot.isMainComponent);

    juce::Button& refreshDevice = requireButtonForAction (*shell, UiActionId::DeviceRefreshAudio);
    juce::Button& testDevice = requireButtonForAction (*shell, UiActionId::DeviceSelectTestAudio);
    juce::Button& armTrack = requireButtonForAction (*shell, UiActionId::RecordingArmTrack);
    juce::Button& monitor = requireButtonForAction (*shell, UiActionId::RecordingSetMonitoringPolicy);
    juce::Button& record = requireButtonForAction (*shell, UiActionId::TransportRecord);

    REQUIRE (refreshDevice.getComponentID() == "device.refresh_audio");
    REQUIRE (testDevice.getComponentID() == "device.select_test_audio");
    REQUIRE (testDevice.getButtonText() == "Test Device");
    REQUIRE (armTrack.getComponentID() == "record.track.arm");
    REQUIRE (monitor.getComponentID() == "record.monitoring_policy");
    REQUIRE (record.getComponentID() == "transport.record");
}

TEST_CASE ("H13 recording UX harness keeps Record disabled until a test device and armed Track input exist",
           "[recording][ux][shell][negative]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("disabled-record");
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));

    juce::Button& newProject = requireButtonForAction (*shell, UiActionId::ProjectNew);
    juce::Button& testDevice = requireButtonForAction (*shell, UiActionId::DeviceSelectTestAudio);
    juce::Button& armTrack = requireButtonForAction (*shell, UiActionId::RecordingArmTrack);
    juce::Button& monitor = requireButtonForAction (*shell, UiActionId::RecordingSetMonitoringPolicy);
    juce::Button& record = requireButtonForAction (*shell, UiActionId::TransportRecord);

    REQUIRE_FALSE (testDevice.isEnabled());
    REQUIRE_FALSE (armTrack.isEnabled());
    REQUIRE_FALSE (monitor.isEnabled());
    REQUIRE_FALSE (record.isEnabled());

    clickButton (newProject);

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.recordingDeviceSelected);
    REQUIRE (snapshot.context.recordingTrackAvailable);
    REQUIRE_FALSE (snapshot.context.recordingTrackArmed);
    REQUIRE_FALSE (snapshot.context.recordingInputSelected);
    REQUIRE_FALSE (record.isEnabled());

    clickButton (testDevice);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingDeviceSelected);
    REQUIRE (snapshot.context.selectedRecordingDeviceId == 1u);
    REQUIRE (snapshot.context.recordingDeviceGeneration == 1u);
    REQUIRE (snapshot.recordingDevice.selected);
    REQUIRE (snapshot.recordingDevice.stableDeviceId == 1u);
    REQUIRE (snapshot.recordingDevice.inputChannels == 2u);
    REQUIRE (snapshot.recordingDevice.maxBlockSize == 128u);
    REQUIRE (snapshot.context.deviceSelectCount == 1);
    REQUIRE_FALSE (snapshot.context.recordingTrackArmed);
    REQUIRE_FALSE (record.isEnabled());

    clickButton (monitor);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingMonitoringSelected);
    REQUIRE (snapshot.context.recordingMonitoringCount == 1);
    REQUIRE_FALSE (snapshot.context.recordingTrackArmed);
    REQUIRE_FALSE (snapshot.context.recordingInputSelected);
    REQUIRE_FALSE (record.isEnabled());

    clickButton (armTrack);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingTrackArmed);
    REQUIRE (snapshot.context.recordingInputSelected);
    REQUIRE (snapshot.context.selectedRecordingTrackIndex == 0);
    REQUIRE (snapshot.context.selectedRecordingInputChannel == 0);
    REQUIRE (snapshot.recordingTrackInput.armed);
    REQUIRE (snapshot.recordingTrackInput.trackId.isValid());
    REQUIRE (snapshot.recordingTrackInput.trackIndex == 0u);
    REQUIRE (snapshot.recordingTrackInput.inputChannel == 0u);
    REQUIRE (snapshot.context.recordingArmCount == 1);
    REQUIRE (record.isEnabled());

    juce::Button& refreshDevice = requireButtonForAction (*shell, UiActionId::DeviceRefreshAudio);
    clickButton (refreshDevice);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.deviceRefreshCount == 1);
    REQUIRE (snapshot.context.recordingDeviceGeneration == 2u);
    REQUIRE (snapshot.context.recordingDeviceSelected);
    REQUIRE (snapshot.context.recordingTrackArmed);
    REQUIRE (snapshot.context.recordingInputSelected);
    REQUIRE (record.isEnabled());

    clickButton (record);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isRecording);
    REQUIRE (snapshot.context.recordingCommandCount == 1);
    REQUIRE (snapshot.playbackReady);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.lastRecordedAudioTake.assetId.isValid());
    REQUIRE (snapshot.lastRecordedAudioTake.clipId.isValid());
    REQUIRE (snapshot.lastRecordedAudioTake.trackId == snapshot.recordingTrackInput.trackId);
    REQUIRE (snapshot.lastRecordedAudioTake.timelineStart == 0);
    REQUIRE (snapshot.lastRecordedAudioTake.frames == 256u);
    REQUIRE (snapshot.lastRecordedAudioTake.channels == 1u);

    const yesdaw::engine::Project recorded = readProjectSnapshot (bundlePath);
    REQUIRE (recorded.assets.size() == 1u);
    REQUIRE (recorded.clips.size() == 1u);
    REQUIRE (recorded.tracks.size() == 1u);

    const yesdaw::engine::Asset& asset = recorded.assets.front();
    const yesdaw::engine::Clip& clip = recorded.clips.front();
    REQUIRE (asset.id == snapshot.lastRecordedAudioTake.assetId);
    REQUIRE (asset.frames == 256u);
    REQUIRE (asset.sampleRate.hz == 48000.0);
    REQUIRE (asset.channels == 1u);
    REQUIRE (clip.id == snapshot.lastRecordedAudioTake.clipId);
    REQUIRE (clip.assetId == asset.id);
    REQUIRE (clip.trackId == snapshot.recordingTrackInput.trackId);
    REQUIRE (clip.timelineStart == 0);
    REQUIRE (clip.timelineLength == 256);
    REQUIRE (clip.srcOffset == 0u);
    REQUIRE (clip.srcLen == 256u);
    REQUIRE (clip.timeBase == yesdaw::engine::TimeBase::SampleLocked);

    const std::filesystem::path assetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash);
    REQUIRE (std::filesystem::is_regular_file (assetPath));

    yesdaw::io::Float32Wav decoded;
    REQUIRE (yesdaw::io::readFloat32WavFile (assetPath, decoded).ok());
    REQUIRE (decoded.sampleRate == asset.sampleRate);
    REQUIRE (decoded.frames == asset.frames);
    REQUIRE (decoded.channels == asset.channels);
    REQUIRE (decoded.interleavedSamples.size() == 256u);
    for (std::uint64_t frame = 0; frame < decoded.frames; ++frame)
        REQUIRE (decoded.interleavedSamples[static_cast<std::size_t> (frame)] == expectedRecordedSample (frame));
}

TEST_CASE ("H13 recording UX rejects arming when the loaded Project has no Track",
           "[recording][ux][shell][negative]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("no-track");
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.makeNewProject = [] {
        auto project = UiAppModel::makeDefaultSessionProject();
        project.tracks.clear();
        return project;
    };

    auto shell = makeShell (std::move (choices));

    juce::Button& newProject = requireButtonForAction (*shell, UiActionId::ProjectNew);
    juce::Button& testDevice = requireButtonForAction (*shell, UiActionId::DeviceSelectTestAudio);
    juce::Button& armTrack = requireButtonForAction (*shell, UiActionId::RecordingArmTrack);
    juce::Button& record = requireButtonForAction (*shell, UiActionId::TransportRecord);

    clickButton (newProject);
    clickButton (testDevice);

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.recordingDeviceSelected);
    REQUIRE_FALSE (snapshot.context.recordingTrackAvailable);
    REQUIRE_FALSE (armTrack.isEnabled());
    REQUIRE_FALSE (record.isEnabled());

    clickButton (armTrack);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.recordingTrackArmed);
    REQUIRE_FALSE (snapshot.context.recordingInputSelected);
    REQUIRE (snapshot.context.recordingArmCount == 0);
}
