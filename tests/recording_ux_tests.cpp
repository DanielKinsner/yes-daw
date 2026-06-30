// YES DAW - H13 recording UX shipped-shell harness skeleton.

#include "engine/Recording.h"
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
using yesdaw::ui::UiRecordingMonitoringPolicy;
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

yesdaw::engine::RecordingConfig makeRecordingConfigForSnapshot (
    const MainComponentSnapshot& snapshot,
    UiRecordingMonitoringPolicy policy)
{
    yesdaw::engine::RecordingConfig config;
    config.sampleRateHz = snapshot.recordingDevice.sampleRate.hz;
    config.channels = 1;
    config.latency.inputLatencyFrames = snapshot.recordingDevice.inputLatencyFrames;
    config.latency.outputLatencyFrames = snapshot.recordingDevice.outputLatencyFrames;
    config.latency.includeOutputLatency = policy == UiRecordingMonitoringPolicy::LatencyCompensated;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 512;
    return config;
}

std::int64_t mappedTimelineFrame (const yesdaw::engine::RecordingConfig& config,
                                  std::int64_t deviceInputFrame)
{
    std::int64_t timelineFrame = -1;
    std::uint32_t takeOrdinal = 99;
    REQUIRE (yesdaw::engine::mapDeviceInputFrameToRecordingFrame (
        config,
        deviceInputFrame,
        timelineFrame,
        takeOrdinal));
    REQUIRE (takeOrdinal == 0u);
    return timelineFrame;
}

float compMaskSampleAt (const yesdaw::engine::Project& project, yesdaw::engine::Tick frame)
{
    for (const yesdaw::engine::ProjectRecordingCompSegment& segment : project.recordingCompSegments)
    {
        if (frame >= segment.timelineStart && frame < segment.timelineStart + segment.timelineLength)
            return 1.0f;
    }

    return 0.0f;
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
    juce::Button& comp = requireButtonForAction (*shell, UiActionId::RecordingAssembleComp);

    REQUIRE (refreshDevice.getComponentID() == "device.refresh_audio");
    REQUIRE (testDevice.getComponentID() == "device.select_test_audio");
    REQUIRE (testDevice.getButtonText() == "Test Device");
    REQUIRE (armTrack.getComponentID() == "record.track.arm");
    REQUIRE (monitor.getComponentID() == "record.monitoring_policy");
    REQUIRE (record.getComponentID() == "transport.record");
    REQUIRE (comp.getComponentID() == "record.comp.assemble");
    REQUIRE (comp.getButtonText() == "Comp");
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
    REQUIRE (snapshot.recordingDevice.latencyCalibrated);
    REQUIRE (snapshot.recordingDevice.inputLatencyFrames == 40);
    REQUIRE (snapshot.recordingDevice.outputLatencyFrames == 60);
    REQUIRE (snapshot.context.deviceSelectCount == 1);
    REQUIRE_FALSE (snapshot.context.recordingTrackArmed);
    REQUIRE_FALSE (record.isEnabled());

    clickButton (monitor);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingMonitoringSelected);
    REQUIRE (snapshot.context.selectedRecordingMonitoringPolicy == UiRecordingMonitoringPolicy::DirectInput);
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
    REQUIRE (snapshot.recordingDevice.latencyCalibrated);
    REQUIRE (snapshot.recordingDevice.inputLatencyFrames == 40);
    REQUIRE (snapshot.recordingDevice.outputLatencyFrames == 60);
    REQUIRE (snapshot.context.recordingTrackArmed);
    REQUIRE (snapshot.context.recordingInputSelected);
    REQUIRE (record.isEnabled());

    clickButton (record);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isRecording);
    REQUIRE (snapshot.context.recordingCommandCount == 1);
    REQUIRE (snapshot.playbackReady);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.midiClipSelected);
    REQUIRE (snapshot.lastRecordedAudioTake.assetId.isValid());
    REQUIRE (snapshot.lastRecordedAudioTake.clipId.isValid());
    REQUIRE (snapshot.lastRecordedAudioTake.takeId.isValid());
    REQUIRE (snapshot.lastRecordedAudioTake.trackId == snapshot.recordingTrackInput.trackId);
    REQUIRE (snapshot.lastRecordedAudioTake.timelineStart == 0);
    REQUIRE (snapshot.lastRecordedAudioTake.frames == 256u);
    REQUIRE (snapshot.lastRecordedAudioTake.channels == 1u);
    REQUIRE (snapshot.lastRecordedMidiTake.midiClipId.isValid());
    REQUIRE (snapshot.lastRecordedMidiTake.trackId == snapshot.recordingTrackInput.trackId);
    REQUIRE (snapshot.lastRecordedMidiTake.timelineStart == snapshot.lastRecordedAudioTake.timelineStart);
    REQUIRE (snapshot.lastRecordedMidiTake.timelineLength == static_cast<yesdaw::engine::Tick> (snapshot.lastRecordedAudioTake.frames));
    REQUIRE (snapshot.lastRecordedMidiTake.noteCount == 2u);

    const yesdaw::engine::Project recorded = readProjectSnapshot (bundlePath);
    REQUIRE (recorded.assets.size() == 1u);
    REQUIRE (recorded.clips.size() == 1u);
    REQUIRE (recorded.tracks.size() == 1u);
    REQUIRE (recorded.recordingTakes.size() == 1u);
    REQUIRE (recorded.midiClips.size() == 1u);

    const yesdaw::engine::Asset& asset = recorded.assets.front();
    const yesdaw::engine::Clip& clip = recorded.clips.front();
    const yesdaw::engine::RecordingTake& take = recorded.recordingTakes.front();
    const yesdaw::engine::MidiClip& midiClip = recorded.midiClips.front();
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
    REQUIRE (take.id == snapshot.lastRecordedAudioTake.takeId);
    REQUIRE (take.assetId == asset.id);
    REQUIRE (take.clipId == clip.id);
    REQUIRE (take.trackId == snapshot.recordingTrackInput.trackId);
    REQUIRE (take.timelineStart == clip.timelineStart);
    REQUIRE (take.frameCount == clip.srcLen);
    REQUIRE (take.takeOrdinal == 0u);
    REQUIRE (take.inputChannel == snapshot.recordingTrackInput.inputChannel);
    REQUIRE (take.deviceStableId == snapshot.recordingDevice.stableDeviceId);
    REQUIRE (take.monitoringPolicy == yesdaw::engine::RecordingMonitoringPolicy::DirectInput);
    REQUIRE (midiClip.id == snapshot.lastRecordedMidiTake.midiClipId);
    REQUIRE (midiClip.trackId == snapshot.recordingTrackInput.trackId);
    REQUIRE (midiClip.timelineStart == clip.timelineStart);
    REQUIRE (midiClip.timelineLength == clip.timelineLength);
    REQUIRE (midiClip.timeBase == yesdaw::engine::TimeBase::SampleLocked);
    REQUIRE (midiClip.notes.size() == 2u);
    REQUIRE (midiClip.notes[0].startTick == 32);
    REQUIRE (midiClip.notes[0].lengthTicks == 48);
    REQUIRE (midiClip.notes[0].key == 60);
    REQUIRE (midiClip.notes[0].pitchNote == 60.0);
    REQUIRE (midiClip.notes[0].normalizedVelocity == 0.75);
    REQUIRE (midiClip.notes[0].portIndex == 0);
    REQUIRE (midiClip.notes[0].channel == snapshot.recordingTrackInput.inputChannel);
    REQUIRE (midiClip.notes[1].startTick == 128);
    REQUIRE (midiClip.notes[1].lengthTicks == 64);
    REQUIRE (midiClip.notes[1].key == 67);
    REQUIRE (midiClip.notes[1].pitchNote == 67.0);
    REQUIRE (midiClip.notes[1].normalizedVelocity == 0.5);
    REQUIRE (midiClip.notes[1].portIndex == 0);
    REQUIRE (midiClip.notes[1].channel == snapshot.recordingTrackInput.inputChannel);

    UiAppModel reopened;
    REQUIRE (reopened.openProjectBundle (bundlePath).ok());
    REQUIRE (reopened.project().midiClips == recorded.midiClips);

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

TEST_CASE ("H13 monitoring policy and fake-device latency calibration are scriptable through MainComponent",
           "[recording][ux][shell][latency]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("monitoring-latency");
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));

    juce::Button& newProject = requireButtonForAction (*shell, UiActionId::ProjectNew);
    juce::Button& testDevice = requireButtonForAction (*shell, UiActionId::DeviceSelectTestAudio);
    juce::Button& armTrack = requireButtonForAction (*shell, UiActionId::RecordingArmTrack);
    juce::Button& monitor = requireButtonForAction (*shell, UiActionId::RecordingSetMonitoringPolicy);
    juce::Button& record = requireButtonForAction (*shell, UiActionId::TransportRecord);

    clickButton (newProject);
    clickButton (testDevice);
    clickButton (armTrack);

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.recordingDevice.latencyCalibrated);
    REQUIRE (snapshot.recordingDevice.inputLatencyFrames == 40);
    REQUIRE (snapshot.recordingDevice.outputLatencyFrames == 60);
    REQUIRE (snapshot.context.selectedRecordingMonitoringPolicy == UiRecordingMonitoringPolicy::Unselected);
    REQUIRE_FALSE (record.isEnabled());

    clickButton (monitor);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingMonitoringSelected);
    REQUIRE (snapshot.context.selectedRecordingMonitoringPolicy == UiRecordingMonitoringPolicy::DirectInput);
    REQUIRE (record.isEnabled());

    const auto directConfig = makeRecordingConfigForSnapshot (
        snapshot,
        snapshot.context.selectedRecordingMonitoringPolicy);
    REQUIRE (directConfig.latency.includeOutputLatency == false);
    REQUIRE (mappedTimelineFrame (directConfig, 200) == 160);

    clickButton (monitor);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingMonitoringSelected);
    REQUIRE (snapshot.context.selectedRecordingMonitoringPolicy == UiRecordingMonitoringPolicy::LatencyCompensated);
    REQUIRE (record.isEnabled());

    const auto latencyCompensatedConfig = makeRecordingConfigForSnapshot (
        snapshot,
        snapshot.context.selectedRecordingMonitoringPolicy);
    REQUIRE (latencyCompensatedConfig.latency.includeOutputLatency);
    REQUIRE (mappedTimelineFrame (latencyCompensatedConfig, 200) == 100);

    yesdaw::engine::RecordingConfig noCompensation = latencyCompensatedConfig;
    noCompensation.latency.inputLatencyFrames = 0;
    noCompensation.latency.outputLatencyFrames = 0;
    REQUIRE (mappedTimelineFrame (noCompensation, 200) == 200);

    clickButton (monitor);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingMonitoringSelected);
    REQUIRE (snapshot.context.selectedRecordingMonitoringPolicy == UiRecordingMonitoringPolicy::Off);
    REQUIRE (record.isEnabled());

    clickButton (monitor);
    clickButton (monitor);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.selectedRecordingMonitoringPolicy == UiRecordingMonitoringPolicy::LatencyCompensated);

    clickButton (record);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isRecording);
    REQUIRE (snapshot.lastRecordedAudioTake.takeId.isValid());

    const yesdaw::engine::Project recorded = readProjectSnapshot (bundlePath);
    REQUIRE (recorded.recordingTakes.size() == 1u);
    REQUIRE (recorded.recordingTakes.front().monitoringPolicy
             == yesdaw::engine::RecordingMonitoringPolicy::LatencyCompensated);
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

TEST_CASE ("H13 take lanes and Comp basics persist and undo through shipped MainComponent",
           "[recording][ux][shell][comp]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("take-lane-comp");
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));

    juce::Button& newProject = requireButtonForAction (*shell, UiActionId::ProjectNew);
    juce::Button& testDevice = requireButtonForAction (*shell, UiActionId::DeviceSelectTestAudio);
    juce::Button& armTrack = requireButtonForAction (*shell, UiActionId::RecordingArmTrack);
    juce::Button& monitor = requireButtonForAction (*shell, UiActionId::RecordingSetMonitoringPolicy);
    juce::Button& record = requireButtonForAction (*shell, UiActionId::TransportRecord);
    juce::Button& comp = requireButtonForAction (*shell, UiActionId::RecordingAssembleComp);
    juce::Button& undo = requireButtonForAction (*shell, UiActionId::EditUndo);
    juce::Button& redo = requireButtonForAction (*shell, UiActionId::EditRedo);

    clickButton (newProject);
    clickButton (testDevice);
    clickButton (armTrack);
    clickButton (monitor);

    REQUIRE_FALSE (comp.isEnabled());

    clickButton (record);
    clickButton (record);
    clickButton (record);

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isRecording);
    REQUIRE (snapshot.context.recordingCommandCount == 3);
    REQUIRE (snapshot.context.recordingCompTakesAvailable);
    REQUIRE (comp.isEnabled());

    yesdaw::engine::Project recorded = readProjectSnapshot (bundlePath);
    REQUIRE (recorded.recordingTakes.size() == 2u);
    REQUIRE (recorded.clips.size() == 2u);
    REQUIRE (recorded.midiClips.size() == 2u);
    REQUIRE (recorded.recordingTakes[0].takeOrdinal == 0u);
    REQUIRE (recorded.recordingTakes[1].takeOrdinal == 1u);
    REQUIRE (recorded.recordingCompSegments.empty());

    clickButton (comp);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingCompSelected);
    REQUIRE (snapshot.context.recordingCompSegmentCount == 2);
    REQUIRE (snapshot.context.recordingCompCommandCount == 1);
    REQUIRE (snapshot.recordingComp.selected);
    REQUIRE (snapshot.recordingComp.segmentCount == 2u);
    REQUIRE (snapshot.recordingComp.gapStart == 96);
    REQUIRE (snapshot.recordingComp.gapLength == 64);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (undo.isEnabled());

    yesdaw::engine::Project comped = readProjectSnapshot (bundlePath);
    REQUIRE (comped.recordingTakes == recorded.recordingTakes);
    REQUIRE (comped.clips == recorded.clips);
    REQUIRE (comped.midiClips == recorded.midiClips);
    REQUIRE (comped.recordingCompSegments.size() == 2u);
    REQUIRE (comped.recordingCompSegments[0].takeId == comped.recordingTakes[0].id);
    REQUIRE (comped.recordingCompSegments[0].timelineStart == 0);
    REQUIRE (comped.recordingCompSegments[0].timelineLength == 96);
    REQUIRE (comped.recordingCompSegments[1].takeId == comped.recordingTakes[1].id);
    REQUIRE (comped.recordingCompSegments[1].timelineStart == 160);
    REQUIRE (comped.recordingCompSegments[1].timelineLength == 96);
    REQUIRE (compMaskSampleAt (comped, 32) == 1.0f);
    REQUIRE (compMaskSampleAt (comped, 128) == 0.0f);
    REQUIRE (compMaskSampleAt (comped, 192) == 1.0f);

    UiAppModel reopened;
    REQUIRE (reopened.openProjectBundle (bundlePath).ok());
    REQUIRE (reopened.project().recordingTakes == comped.recordingTakes);
    REQUIRE (reopened.project().recordingCompSegments == comped.recordingCompSegments);
    REQUIRE (reopened.context().recordingCompSelected);
    REQUIRE (reopened.context().recordingCompSegmentCount == 2);

    clickButton (undo);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.recordingCompSelected);
    REQUIRE (snapshot.context.recordingCompSegmentCount == 0);
    REQUIRE_FALSE (snapshot.recordingComp.selected);
    REQUIRE_FALSE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (redo.isEnabled());
    yesdaw::engine::Project undone = readProjectSnapshot (bundlePath);
    REQUIRE (undone.recordingTakes == recorded.recordingTakes);
    REQUIRE (undone.clips == recorded.clips);
    REQUIRE (undone.recordingCompSegments.empty());

    clickButton (redo);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingCompSelected);
    REQUIRE (snapshot.context.recordingCompSegmentCount == 2);
    yesdaw::engine::Project redone = readProjectSnapshot (bundlePath);
    REQUIRE (redone.recordingCompSegments == comped.recordingCompSegments);
    REQUIRE (redone.clips == recorded.clips);
}
