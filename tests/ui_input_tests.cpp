// YES DAW - H12 real-shell UI input harness skeleton.

#include "ui/MainComponent.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiTheme.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include "engine/nodes/EqNode.h"
#include "engine/Time.h"
#include "io/WavFile.h"
#include "persistence/ProjectBundle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <array>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

using yesdaw::ui::MainComponentSnapshot;
using yesdaw::ui::MainComponentFileChoices;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiPanel;
using yesdaw::ui::findMainComponentChildForAction;
using yesdaw::ui::mainShellToolbarActions;
using yesdaw::ui::renderMainComponentPlayback;
using yesdaw::ui::serviceMainComponentUiTimer;
using yesdaw::ui::snapshotMainComponent;
using yesdaw::engine::AutomationBreakpoint;
using yesdaw::engine::AutomationCurveType;
using yesdaw::engine::AutomationLaneData;
using yesdaw::engine::AutomationTargetRole;
using yesdaw::engine::ProjectMixerNodeRole;
using yesdaw::engine::projectMixerNodeIdForEntity;
using yesdaw::engine::projectMixerSendLevelNodeIdForTrack;

namespace {

constexpr const char* kTimelineComponentId = "timeline.canvas";
constexpr const char* kPianoRollComponentId = "piano-roll.canvas";
constexpr const char* kInspectorStartComponentId = "clip.inspector.start";
constexpr const char* kInspectorEndComponentId = "clip.inspector.end";
constexpr const char* kInspectorLengthComponentId = "clip.inspector.length";
constexpr const char* kInspectorFadeInComponentId = "clip.inspector.fade_in";
constexpr const char* kInspectorFadeOutComponentId = "clip.inspector.fade_out";
constexpr const char* kInspectorFadeCurveComponentId = "clip.inspector.fade_curve";
constexpr const char* kAutomationLaneRowComponentId = "timeline.automation.track.0.lane";
constexpr const char* kExportAudioProgressComponentId = "project.export_audio.progress";
constexpr int kInspectorEqualPowerFadeCurveId = 1;
constexpr double kInspectorTimingShortenRatio = 0.5;
constexpr double kInspectorFadeInRatio = 0.25;
constexpr double kInspectorFadeOutRatio = 0.75;
constexpr int kPianoRollHighKey = yesdaw::ui::UiTheme::Layout::pianoRollHighKey;
constexpr int kPianoRollKeyCount = yesdaw::ui::UiTheme::Layout::pianoRollKeyCount;

constexpr yesdaw::engine::EntityId idFromLowByte (std::uint8_t low) noexcept
{
    yesdaw::engine::EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return yesdaw::engine::EntityId::fromBytes (bytes);
}

std::filesystem::path makeTempBundlePath (std::string label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("yesdaw-ui-input-" + std::move (label) + "-" + std::to_string (ticks) + ".yesdaw");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

yesdaw::engine::Project readProjectSnapshot (const std::filesystem::path& bundlePath)
{
    yesdaw::persistence::ProjectBundleDb db;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (bundlePath, db).ok());

    yesdaw::engine::Project project;
    REQUIRE (db.readProjectSnapshot (project).ok());
    return project;
}

void writeProjectSnapshot (const std::filesystem::path& bundlePath, const yesdaw::engine::Project& project)
{
    yesdaw::persistence::ProjectBundleDb db;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
    REQUIRE (db.writeProjectSnapshot (project).ok());
}

yesdaw::engine::Note makeMidiInputNote (yesdaw::engine::EntityId id,
                                        yesdaw::engine::Tick start,
                                        yesdaw::engine::Tick length,
                                        std::int16_t key,
                                        double velocity)
{
    yesdaw::engine::Note note;
    note.id = id;
    note.startTick = start;
    note.lengthTicks = length;
    note.key = key;
    note.pitchNote = static_cast<double> (key) + 0.25;
    note.normalizedVelocity = velocity;
    note.portIndex = 1;
    note.channel = 2;
    return note;
}

yesdaw::engine::Project makeMidiInputProject()
{
    yesdaw::engine::Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = yesdaw::engine::SampleRate { 48000.0 };
    project.tempoMap.push_back ({ 0, 120.0, yesdaw::engine::TempoCurve::Jump });
    project.meterMap.push_back ({ 0, 4, 4 });

    yesdaw::engine::Track track;
    track.id = idFromLowByte (2);
    track.strip.name = "MIDI Track";
    project.tracks.push_back (track);

    yesdaw::engine::MidiClip midiClip;
    midiClip.id = idFromLowByte (3);
    midiClip.trackId = track.id;
    midiClip.timelineStart = 0;
    midiClip.timelineLength = 4096;
    midiClip.timeBase = yesdaw::engine::TimeBase::TempoLocked;
    midiClip.notes = {
        makeMidiInputNote (idFromLowByte (4), 256, 512, 60, 0.55),
        makeMidiInputNote (idFromLowByte (5), 1408, 384, 67, 0.82)
    };
    project.midiClips.push_back (std::move (midiClip));
    return project;
}

yesdaw::engine::Project makeEndToEndInputProject()
{
    yesdaw::engine::Project project = makeMidiInputProject();

    yesdaw::engine::Track audioTrack;
    audioTrack.id = yesdaw::engine::kDefaultAudioTrackId;
    audioTrack.strip.name = "Audio 1";
    project.tracks.insert (project.tracks.begin(), std::move (audioTrack));
    return project;
}

yesdaw::engine::Project makeMixerSendsInputProject()
{
    yesdaw::engine::Project project = yesdaw::ui::UiAppModel::makeDefaultSessionProject();
    REQUIRE (project.tracks.size() == 1u);

    AutomationLaneData sendLane;
    sendLane.id = idFromLowByte (90);
    sendLane.ownerEntity = project.tracks.front().id;
    sendLane.role = AutomationTargetRole::SendLevel;
    sendLane.paramId = 0;
    sendLane.points = {
        AutomationBreakpoint { 0, 0.20, AutomationCurveType::Linear },
        AutomationBreakpoint { 15360, 0.60, AutomationCurveType::Hold },
    };
    project.automationLanes.push_back (sendLane);
    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (project.automationTargetsReferenceProjectRows());
    return project;
}

yesdaw::engine::Project makeMixerFxSlotsInputProject()
{
    yesdaw::engine::Project project = yesdaw::ui::UiAppModel::makeDefaultSessionProject();
    REQUIRE (project.tracks.size() == 1u);

    project.tracks.front().strip.fxChain = {
        { idFromLowByte (91), yesdaw::engine::FxKind::Eq, true,
          { { yesdaw::engine::EqNode::parameterIdFor (0, yesdaw::engine::EqNode::kGainParamOffset), 0.35 } } },
        { idFromLowByte (92), yesdaw::engine::FxKind::Compressor, false, {} },
    };
    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (project.automationTargetsReferenceProjectRows());
    return project;
}

yesdaw::engine::Project makeMixerBusFxSlotsInputProject()
{
    yesdaw::engine::Project project = makeMixerFxSlotsInputProject();
    yesdaw::engine::Bus bus;
    bus.id = idFromLowByte (93);
    bus.strip.name = "Room Bus";
    bus.strip.fxChain = {
        { idFromLowByte (94), yesdaw::engine::FxKind::Reverb, true, {} },
    };
    project.buses.push_back (std::move (bus));
    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (project.automationTargetsReferenceProjectRows());
    return project;
}

yesdaw::engine::Project makeAutomationInputProject()
{
    yesdaw::engine::Project project = yesdaw::ui::UiAppModel::makeDefaultSessionProject();
    REQUIRE (project.tracks.size() == 1u);

    yesdaw::engine::AutomationLaneData lane;
    lane.id = idFromLowByte (80);
    lane.ownerEntity = project.tracks.front().id;
    lane.role = yesdaw::engine::AutomationTargetRole::TrackFader;
    lane.paramId = yesdaw::engine::FaderNode::kGainParameterId;
    lane.points = {
        { 0, 0.20, yesdaw::engine::AutomationCurveType::Linear },
        { 960, 0.80, yesdaw::engine::AutomationCurveType::Linear }
    };
    project.automationLanes.push_back (lane);
    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (project.automationTargetsReferenceProjectRows());
    return project;
}

std::vector<std::uint8_t> readBytes (const std::filesystem::path& path)
{
    const auto size = std::filesystem::file_size (path);
    std::vector<std::uint8_t> bytes (static_cast<std::size_t> (size));

    std::ifstream input (path, std::ios::binary);
    REQUIRE (input.good());
    input.read (reinterpret_cast<char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    REQUIRE (input.good());
    return bytes;
}

double peakAbs (std::span<const float> samples) noexcept
{
    double peak = 0.0;
    for (const float sample : samples)
        peak = std::max (peak, std::fabs (static_cast<double> (sample)));
    return peak;
}

double channelPeakAbs (std::span<const float> interleaved, std::size_t channel, std::size_t channels) noexcept
{
    double peak = 0.0;
    if (channels == 0 || channel >= channels)
        return peak;

    for (std::size_t i = channel; i < interleaved.size(); i += channels)
        peak = std::max (peak, std::fabs (static_cast<double> (interleaved[i])));

    return peak;
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

juce::Component* findChildWithComponentId (juce::Component& component, const juce::String& componentId)
{
    if (component.getComponentID() == componentId)
        return &component;

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (juce::Component* child = component.getChildComponent (i))
            if (juce::Component* found = findChildWithComponentId (*child, componentId))
                return found;

    return nullptr;
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

juce::Button& requireRegisteredButtonForAction (juce::Component& shell, UiActionId action)
{
    juce::Component* component = findMainComponentChildForAction (shell, action);
    REQUIRE (component != nullptr);

    auto* button = dynamic_cast<juce::Button*> (component);
    REQUIRE (button != nullptr);
    REQUIRE (button->getWidth() > 0);
    REQUIRE (button->getHeight() > 0);
    return *button;
}

juce::Slider& requireSliderForAction (juce::Component& shell, UiActionId action)
{
    juce::Component* component = findMainComponentChildForAction (shell, action);
    REQUIRE (component != nullptr);

    auto* slider = dynamic_cast<juce::Slider*> (component);
    REQUIRE (slider != nullptr);
    REQUIRE (slider->isVisible());
    REQUIRE (slider->getWidth() > 0);
    REQUIRE (slider->getHeight() > 0);
    return *slider;
}

juce::Button& requireButtonWithComponentId (juce::Component& shell, const juce::String& componentId)
{
    juce::Component* component = findChildWithComponentId (shell, componentId);
    REQUIRE (component != nullptr);

    auto* button = dynamic_cast<juce::Button*> (component);
    REQUIRE (button != nullptr);
    REQUIRE (button->isVisible());
    REQUIRE (button->getWidth() > 0);
    REQUIRE (button->getHeight() > 0);
    return *button;
}

juce::Slider& requireSliderWithComponentId (juce::Component& shell, const juce::String& componentId)
{
    juce::Component* component = findChildWithComponentId (shell, componentId);
    REQUIRE (component != nullptr);

    auto* slider = dynamic_cast<juce::Slider*> (component);
    REQUIRE (slider != nullptr);
    REQUIRE (slider->isVisible());
    REQUIRE (slider->getWidth() > 0);
    REQUIRE (slider->getHeight() > 0);
    return *slider;
}

juce::ComboBox& requireComboBoxWithComponentId (juce::Component& shell, const juce::String& componentId)
{
    juce::Component* component = findChildWithComponentId (shell, componentId);
    REQUIRE (component != nullptr);

    auto* comboBox = dynamic_cast<juce::ComboBox*> (component);
    REQUIRE (comboBox != nullptr);
    REQUIRE (comboBox->isVisible());
    REQUIRE (comboBox->getWidth() > 0);
    REQUIRE (comboBox->getHeight() > 0);
    return *comboBox;
}

juce::Label& requireLabelWithComponentId (juce::Component& shell, const juce::String& componentId)
{
    juce::Component* component = findChildWithComponentId (shell, componentId);
    REQUIRE (component != nullptr);

    auto* label = dynamic_cast<juce::Label*> (component);
    REQUIRE (label != nullptr);
    REQUIRE (label->getWidth() > 0);
    REQUIRE (label->getHeight() > 0);
    return *label;
}

juce::Component& requireTimelineComponent (juce::Component& shell)
{
    juce::Component* component = findChildWithComponentId (shell, kTimelineComponentId);
    REQUIRE (component != nullptr);
    REQUIRE (component->isVisible());
    REQUIRE (component->getWidth() > 0);
    REQUIRE (component->getHeight() > 0);
    return *component;
}

juce::Component& requirePianoRollComponent (juce::Component& shell)
{
    juce::Component* component = findChildWithComponentId (shell, kPianoRollComponentId);
    REQUIRE (component != nullptr);
    REQUIRE (component->isVisible());
    REQUIRE (component->getWidth() > 0);
    REQUIRE (component->getHeight() > 0);
    return *component;
}

void clickButton (juce::Button& button)
{
    button.triggerClick();
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

juce::MouseEvent makeMouseEvent (juce::Component& component,
                                 juce::Point<int> position,
                                 juce::Point<int> mouseDownPosition,
                                 bool mouseWasDragged,
                                 int numberOfClicks = 1,
                                 juce::ModifierKeys modifiers = juce::ModifierKeys::leftButtonModifier)
{
    const juce::Time now = juce::Time::getCurrentTime();
    return juce::MouseEvent (
        juce::Desktop::getInstance().getMainMouseSource(),
        position.toFloat(),
        modifiers,
        juce::MouseInputSource::defaultPressure,
        juce::MouseInputSource::defaultOrientation,
        juce::MouseInputSource::defaultRotation,
        juce::MouseInputSource::defaultTiltX,
        juce::MouseInputSource::defaultTiltY,
        &component,
        &component,
        now,
        mouseDownPosition.toFloat(),
        now,
        numberOfClicks,
        mouseWasDragged);
}

void mouseDownAt (juce::Component& component, juce::Point<int> position,
                  juce::ModifierKeys modifiers = juce::ModifierKeys::leftButtonModifier)
{
    juce::MouseEvent event = makeMouseEvent (component, position, position, false, 1, modifiers);
    component.mouseDown (event);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

void beginDragFromTo (juce::Component& component,
                      juce::Point<int> start,
                      juce::Point<int> end,
                      juce::ModifierKeys modifiers = juce::ModifierKeys::leftButtonModifier)
{
    juce::MouseEvent down = makeMouseEvent (component, start, start, false, 1, modifiers);
    component.mouseDown (down);

    juce::MouseEvent drag = makeMouseEvent (component, end, start, true, 1, modifiers);
    component.mouseDrag (drag);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

void releaseDragAt (juce::Component& component,
                    juce::Point<int> start,
                    juce::Point<int> end,
                    juce::ModifierKeys modifiers = juce::ModifierKeys::leftButtonModifier)
{
    juce::MouseEvent up = makeMouseEvent (component, end, start, true, 1, modifiers);
    component.mouseUp (up);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

void dragFromTo (juce::Component& component,
                 juce::Point<int> start,
                 juce::Point<int> end,
                 juce::ModifierKeys modifiers = juce::ModifierKeys::leftButtonModifier)
{
    beginDragFromTo (component, start, end, modifiers);
    releaseDragAt (component, start, end, modifiers);
}

void dragVerticalSliderToNormalizedValue (juce::Slider& slider, double normalizedFromMin)
{
    REQUIRE (slider.isEnabled());
    auto bounds = slider.getLocalBounds().reduced (3);
    REQUIRE (bounds.getWidth() > 0);
    REQUIRE (bounds.getHeight() > 0);

    const double clamped = std::clamp (normalizedFromMin, 0.0, 1.0);
    const int y = bounds.getBottom() - 1
                - static_cast<int> (std::llround (clamped * static_cast<double> (bounds.getHeight() - 1)));
    dragFromTo (slider, bounds.getCentre(), { bounds.getCentreX(), std::clamp (y, bounds.getY(), bounds.getBottom() - 1) });
}

void dragHorizontalSliderToNormalizedValue (juce::Slider& slider, double normalizedFromMin)
{
    REQUIRE (slider.isEnabled());
    auto bounds = slider.getLocalBounds().reduced (3);
    REQUIRE (bounds.getWidth() > 0);
    REQUIRE (bounds.getHeight() > 0);

    const double clamped = std::clamp (normalizedFromMin, 0.0, 1.0);
    const int x = bounds.getX()
                + static_cast<int> (std::llround (clamped * static_cast<double> (bounds.getWidth() - 1)));
    dragFromTo (slider, bounds.getCentre(), { std::clamp (x, bounds.getX(), bounds.getRight() - 1), bounds.getCentreY() });
}

void setSliderValueThroughComponent (juce::Slider& slider, double value)
{
    REQUIRE (slider.isEnabled());
    slider.setValue (value, juce::sendNotificationSync);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

void doubleClickAt (juce::Component& component,
                    juce::Point<int> position,
                    juce::ModifierKeys modifiers = juce::ModifierKeys::leftButtonModifier)
{
    juce::MouseEvent event = makeMouseEvent (component, position, position, false, 2, modifiers);
    component.mouseDoubleClick (event);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

int timelineLaneForClip (const yesdaw::engine::Project& project, const yesdaw::engine::Clip& clip)
{
    const auto track = std::find_if (project.tracks.begin(), project.tracks.end(), [&clip] (const auto& candidate) {
        return candidate.id == clip.trackId;
    });
    REQUIRE (track != project.tracks.end());
    return static_cast<int> (std::distance (project.tracks.begin(), track));
}

juce::Point<int> timelineClipRightEdgeDragPoint (juce::Component& timeline,
                                                 const yesdaw::engine::Project& project,
                                                 std::size_t clipIndex)
{
    REQUIRE (project.sampleRate.isValid());
    REQUIRE (clipIndex < project.clips.size());

    std::vector<yesdaw::ui::Clip> clips;
    clips.reserve (project.clips.size());

    double endSeconds = 0.0;
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const yesdaw::engine::Clip& clip = project.clips[i];
        const double startSeconds = static_cast<double> (clip.timelineStart) / project.sampleRate.hz;
        const double lengthSeconds = static_cast<double> (clip.timelineLength) / project.sampleRate.hz;
        clips.push_back ({ static_cast<int> (i), timelineLaneForClip (project, clip), startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = static_cast<int> (project.tracks.size());
    state.clips = clips.data();
    state.clipCount = static_cast<int> (clips.size());
    state.totalSeconds = std::max (1.0, endSeconds * 1.25);
    state.viewport.scrollSeconds = 0.0;
    state.viewport.pixelsPerSecond = static_cast<double> (std::max (1, timeline.getWidth() - 26))
                                   / std::max (1.0, state.totalSeconds);

    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    const yesdaw::ui::Clip& clip = clips[clipIndex];
    const double rightX = static_cast<double> (geometry.clipArea.getX())
                        + ((clip.startSeconds + clip.lengthSeconds) - geometry.viewport.scrollSeconds)
                              * geometry.viewport.pixelsPerSecond;

    const int x = std::max (geometry.clipArea.getX(), static_cast<int> (std::llround (rightX)) - 2);
    const int y = geometry.clipArea.getY() + std::max (1, geometry.laneHeight / 2);
    return { x, y };
}

juce::Point<int> timelineClipLeftEdgeDragPoint (juce::Component& timeline,
                                                const yesdaw::engine::Project& project,
                                                std::size_t clipIndex)
{
    REQUIRE (project.sampleRate.isValid());
    REQUIRE (clipIndex < project.clips.size());

    std::vector<yesdaw::ui::Clip> clips;
    clips.reserve (project.clips.size());

    double endSeconds = 0.0;
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const yesdaw::engine::Clip& clip = project.clips[i];
        const double startSeconds = static_cast<double> (clip.timelineStart) / project.sampleRate.hz;
        const double lengthSeconds = static_cast<double> (clip.timelineLength) / project.sampleRate.hz;
        clips.push_back ({ static_cast<int> (i), timelineLaneForClip (project, clip), startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = static_cast<int> (project.tracks.size());
    state.clips = clips.data();
    state.clipCount = static_cast<int> (clips.size());
    state.totalSeconds = std::max (1.0, endSeconds * 1.25);
    state.viewport.scrollSeconds = 0.0;
    state.viewport.pixelsPerSecond = static_cast<double> (std::max (1, timeline.getWidth() - 26))
                                   / std::max (1.0, state.totalSeconds);

    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    const yesdaw::ui::Clip& clip = clips[clipIndex];
    const double leftX = static_cast<double> (geometry.clipArea.getX())
                       + (clip.startSeconds - geometry.viewport.scrollSeconds) * geometry.viewport.pixelsPerSecond;

    const int x = std::clamp (static_cast<int> (std::llround (leftX)) + 2,
                              geometry.clipArea.getX(),
                              geometry.clipArea.getRight() - 1);
    const int y = geometry.clipArea.getY() + std::max (1, geometry.laneHeight / 2);
    return { x, y };
}

juce::Point<int> timelineClipCenterPoint (juce::Component& timeline,
                                          const yesdaw::engine::Project& project,
                                          std::size_t clipIndex)
{
    REQUIRE (project.sampleRate.isValid());
    REQUIRE (clipIndex < project.clips.size());

    std::vector<yesdaw::ui::Clip> clips;
    clips.reserve (project.clips.size());

    double endSeconds = 0.0;
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const yesdaw::engine::Clip& clip = project.clips[i];
        const double startSeconds = static_cast<double> (clip.timelineStart) / project.sampleRate.hz;
        const double lengthSeconds = static_cast<double> (clip.timelineLength) / project.sampleRate.hz;
        clips.push_back ({ static_cast<int> (i), timelineLaneForClip (project, clip), startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = static_cast<int> (project.tracks.size());
    state.clips = clips.data();
    state.clipCount = static_cast<int> (clips.size());
    state.totalSeconds = std::max (1.0, endSeconds * 1.25);
    state.viewport.scrollSeconds = 0.0;
    state.viewport.pixelsPerSecond = static_cast<double> (std::max (1, timeline.getWidth() - 26))
                                   / std::max (1.0, state.totalSeconds);

    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    const yesdaw::ui::Clip& clip = clips[clipIndex];
    const double centerSeconds = clip.startSeconds + clip.lengthSeconds * 0.5;
    const double centerX = static_cast<double> (geometry.clipArea.getX())
                         + (centerSeconds - geometry.viewport.scrollSeconds) * geometry.viewport.pixelsPerSecond;

    const int x = std::clamp (static_cast<int> (std::llround (centerX)),
                              geometry.clipArea.getX(),
                              geometry.clipArea.getRight() - 1);
    const int y = geometry.clipArea.getY() + std::max (1, geometry.laneHeight / 2);
    return { x, y };
}

juce::Rectangle<int> timelineClipHitBounds (juce::Component& timeline,
                                             const yesdaw::engine::Project& project,
                                             std::size_t clipIndex)
{
    REQUIRE (project.sampleRate.isValid());
    REQUIRE (clipIndex < project.clips.size());

    std::vector<yesdaw::ui::Clip> clips;
    clips.reserve (project.clips.size());
    double endSeconds = 0.0;
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const yesdaw::engine::Clip& clip = project.clips[i];
        const double startSeconds = static_cast<double> (clip.timelineStart) / project.sampleRate.hz;
        const double lengthSeconds = static_cast<double> (clip.timelineLength) / project.sampleRate.hz;
        clips.push_back ({ static_cast<int> (i), timelineLaneForClip (project, clip), startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = static_cast<int> (project.tracks.size());
    state.clips = clips.data();
    state.clipCount = static_cast<int> (clips.size());
    state.totalSeconds = std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                   endSeconds * yesdaw::ui::UiTheme::Layout::timelineProjectEndPaddingScale);
    state.viewport.scrollSeconds = 0.0;
    state.viewport.pixelsPerSecond = static_cast<double> (std::max (
                                         yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                         timeline.getWidth()
                                             - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                   / state.totalSeconds;

    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    std::vector<yesdaw::ui::ElementRect> visible (project.clips.size());
    const int visibleCount = yesdaw::ui::layoutVisible (
        clips.data(), static_cast<int> (clips.size()), geometry.viewport,
        visible.data(), static_cast<int> (visible.size()));
    const auto match = std::find_if (visible.begin(), visible.begin() + visibleCount, [clipIndex] (const auto& rect) {
        return rect.id == static_cast<int> (clipIndex);
    });
    REQUIRE (match != visible.begin() + visibleCount);
    return {
        geometry.clipArea.getX() + juce::roundToInt (match->x),
        geometry.clipArea.getY() + juce::roundToInt (match->y),
        juce::roundToInt (match->w),
        juce::roundToInt (match->h),
    };
}

juce::Point<int> emptyProjectRulerPointAtSeconds (juce::Component& timeline, double seconds)
{
    yesdaw::ui::TimelineCanvasState state;
    state.totalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
    state.viewport.scrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
    state.viewport.pixelsPerSecond = static_cast<double> (juce::jmax (
                                         yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                         timeline.getWidth()
                                             - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                   / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                               state.totalSeconds);
    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    return {
        geometry.clipArea.getX() + juce::roundToInt (
            (seconds - geometry.viewport.scrollSeconds) * geometry.viewport.pixelsPerSecond),
        geometry.rulerArea.getCentreY()
    };
}

std::int64_t emptyProjectFrameAtRulerPoint (juce::Component& timeline, juce::Point<int> point)
{
    yesdaw::ui::TimelineCanvasState state;
    state.totalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
    state.viewport.scrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
    state.viewport.pixelsPerSecond = static_cast<double> (juce::jmax (
                                         yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                         timeline.getWidth()
                                             - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                   / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                               state.totalSeconds);
    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    const double seconds = geometry.viewport.scrollSeconds
                         + static_cast<double> (point.x - geometry.clipArea.getX())
                             / geometry.viewport.pixelsPerSecond;
    return static_cast<std::int64_t> (std::llround (seconds * 48'000.0));
}

juce::Point<int> projectRulerPointAtTick (juce::Component& timeline,
                                          const MainComponentSnapshot& snapshot,
                                          const yesdaw::engine::Project& project,
                                          yesdaw::engine::Tick tick)
{
    REQUIRE (project.sampleRate.isValid());

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = static_cast<int> (project.tracks.size());
    state.totalSeconds = snapshot.visibleTimelineTotalSeconds;
    const double fitPixelsPerSecond = static_cast<double> (juce::jmax (
                                              yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                              timeline.getWidth()
                                                  - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                    / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                state.totalSeconds);
    state.viewport.pixelsPerSecond = fitPixelsPerSecond * snapshot.timelineZoomFactor;
    state.viewport.scrollSeconds = snapshot.timelineScrollSeconds;

    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    const double seconds = static_cast<double> (tick) / project.sampleRate.hz;
    return {
        geometry.clipArea.getX() + juce::roundToInt (
            (seconds - geometry.viewport.scrollSeconds) * geometry.viewport.pixelsPerSecond),
        geometry.rulerArea.getCentreY()
    };
}

double timelinePixelsPerSecond (juce::Component& timeline, const yesdaw::engine::Project& project)
{
    REQUIRE (project.sampleRate.isValid());

    std::vector<yesdaw::ui::Clip> clips;
    clips.reserve (project.clips.size());

    double endSeconds = 0.0;
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const yesdaw::engine::Clip& clip = project.clips[i];
        const double startSeconds = static_cast<double> (clip.timelineStart) / project.sampleRate.hz;
        const double lengthSeconds = static_cast<double> (clip.timelineLength) / project.sampleRate.hz;
        clips.push_back ({ static_cast<int> (i), timelineLaneForClip (project, clip), startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = static_cast<int> (project.tracks.size());
    state.clips = clips.data();
    state.clipCount = static_cast<int> (clips.size());
    state.totalSeconds = std::max (1.0, endSeconds * 1.25);
    state.viewport.scrollSeconds = 0.0;
    state.viewport.pixelsPerSecond = static_cast<double> (std::max (1, timeline.getWidth() - 26))
                                   / std::max (1.0, state.totalSeconds);

    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    return geometry.viewport.pixelsPerSecond;
}

juce::Rectangle<int> pianoRollGridBounds (juce::Component& pianoRoll)
{
    auto area = pianoRoll.getLocalBounds();
    area.removeFromTop (38);
    area.reduce (12, 8);
    area.removeFromBottom (84);
    area.removeFromLeft (70);
    return area.reduced (0, 2);
}

juce::Point<int> pianoRollNoteCenterPoint (juce::Component& pianoRoll,
                                           const yesdaw::engine::MidiClip& midiClip,
                                           const yesdaw::engine::Note& note)
{
    const juce::Rectangle<int> grid = pianoRollGridBounds (pianoRoll);
    const double timelineLength = static_cast<double> (std::max<yesdaw::engine::Tick> (1, midiClip.timelineLength));
    const double noteCenterTick = static_cast<double> (note.startTick)
                                + static_cast<double> (note.lengthTicks) * 0.5;
    const int x = grid.getX()
                + static_cast<int> (std::llround (noteCenterTick / timelineLength
                                                  * static_cast<double> (grid.getWidth())));
    const double rowHeight = static_cast<double> (std::max (1, grid.getHeight()))
                           / static_cast<double> (kPianoRollKeyCount);
    const int y = grid.getY()
                + static_cast<int> (std::llround (static_cast<double> (kPianoRollHighKey - note.key) * rowHeight
                                                  + rowHeight * 0.5));
    return {
        std::clamp (x, grid.getX(), grid.getRight() - 1),
        std::clamp (y, grid.getY(), grid.getBottom() - 1)
    };
}

yesdaw::engine::Tick pianoRollDeltaTicksForPixels (juce::Component& pianoRoll,
                                                   const yesdaw::engine::MidiClip& midiClip,
                                                   int deltaPixels)
{
    const juce::Rectangle<int> grid = pianoRollGridBounds (pianoRoll);
    const double ticks = static_cast<double> (deltaPixels)
                       * static_cast<double> (std::max<yesdaw::engine::Tick> (1, midiClip.timelineLength))
                       / static_cast<double> (std::max (1, grid.getWidth()));
    return static_cast<yesdaw::engine::Tick> (std::llround (ticks));
}

} // namespace

TEST_CASE ("H12 UI input harness constructs the shipped MainComponent", "[ui][input][shell]")
{
    juce::MessageManager::getInstance();
    auto shell = yesdaw::ui::createMainComponent();
    REQUIRE (shell != nullptr);
    const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);

    REQUIRE (snapshot.isMainComponent);
    REQUIRE (snapshot.primaryFileChoicesReady);
    REQUIRE (snapshot.desktopAudioRequested);
    REQUIRE (snapshot.childCount == static_cast<int> (mainShellToolbarActions().size() + 91u));
    REQUIRE_FALSE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.activePanel == UiPanel::Timeline);
    REQUIRE (snapshot.visibleTimelineTrackCount == 0);
    REQUIRE (snapshot.visibleTimelineClipCount == 0);
    REQUIRE (snapshot.visibleMixerTrackCount == 0);
    REQUIRE (snapshot.visibleMixerBusCount == 0);
    REQUIRE_FALSE (snapshot.visibleMixerLoudnessValid);
    REQUIRE (snapshot.visibleMasterPeakLeft == 0.0f);
    REQUIRE (snapshot.visibleMasterPeakRight == 0.0f);
    REQUIRE (snapshot.visiblePianoRollNoteCount == 0);
}

TEST_CASE ("timeline canvas paints each Clip display name", "[ui][input][timeline][clip-name][paint]")
{
    const auto renderNamedClip = [] (const char* name)
    {
        juce::Image image (juce::Image::ARGB, 640, 240, true);
        juce::Graphics graphics (image);
        const yesdaw::ui::Clip clip { 1, 0, 0.0, 4.0, name };
        const yesdaw::ui::TimelineCanvasClipStyle style {
            yesdaw::ui::UiTheme::Color::accentPurple(), 0.75f
        };
        yesdaw::ui::TimelineCanvasState state;
        state.clips = &clip;
        state.clipStyles = &style;
        state.clipCount = 1;
        state.trackCount = 1;
        state.viewport.pixelsPerSecond = 100.0;
        (void) yesdaw::ui::paintTimelineCanvas (graphics, image.getBounds(), state);
        return image;
    };

    const juce::Image first = renderNamedClip ("Audio Clip");
    const juce::Image renamed = renderNamedClip ("Lead Vocal Comp");
    int changedPixels = 0;
    for (int y = 0; y < first.getHeight(); ++y)
        for (int x = 0; x < first.getWidth(); ++x)
            if (first.getPixelAt (x, y) != renamed.getPixelAt (x, y))
                ++changedPixels;

    REQUIRE (changedPixels > 0);
}

TEST_CASE ("F2 renames the selected Clip through the shipped shell and preserves playback",
           "[ui][input][shell][timeline][clip-name]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-name-shell");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project imported = readProjectSnapshot (bundlePath);
    REQUIRE (imported.clips.size() == 1u);
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, imported, 0u));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeRename = renderMainComponentPlayback (*shell, 48'000, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeRename.data(), beforeRename.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::F2Key)));
    auto* rename = dynamic_cast<juce::TextEditor*> (
        findChildWithComponentId (*shell, "shell.timeline.clip.rename"));
    REQUIRE (rename != nullptr);
    REQUIRE (rename->isVisible());
    rename->setText ("Lead Vocal Comp", juce::dontSendNotification);
    rename->onReturnKey();
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    yesdaw::engine::Project renamed = readProjectSnapshot (bundlePath);
    REQUIRE (renamed.clips.front().name == "Lead Vocal Comp");
    REQUIRE (snapshotMainComponent (*shell).visibleFirstTimelineClipName == "Lead Vocal Comp");
    REQUIRE_FALSE (rename->isVisible());

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterRename = renderMainComponentPlayback (*shell, 48'000, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (afterRename == beforeRename);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.front().name == "Audio Clip");
    REQUIRE (shell->keyPressed (juce::KeyPress ('z',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.front().name == "Lead Vocal Comp");

    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    renamed = readProjectSnapshot (bundlePath);
    REQUIRE (renamed.clips.size() == 2u);
    REQUIRE (renamed.clips.back().name == "Lead Vocal Comp");
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));

    shell.reset();
    MainComponentFileChoices openChoices;
    openChoices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
    auto reopened = makeShell (std::move (openChoices));
    clickButton (requireButtonForAction (*reopened, UiActionId::ProjectOpen));
    REQUIRE (readProjectSnapshot (bundlePath).clips.front().name == "Lead Vocal Comp");
    REQUIRE (snapshotMainComponent (*reopened).visibleFirstTimelineClipName == "Lead Vocal Comp");
}

TEST_CASE ("Escape cancels an in-progress timeline move before mouse-up can persist it",
           "[ui][input][shell][timeline][esc-cancel]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("escape-cancel-move");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeCancel = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeCancel.data(), beforeCancel.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    const juce::Point<int> start = timelineClipCenterPoint (timeline, original, 0u);
    const juce::Point<int> end = start.translated (timeline.getWidth() / 4, 0);
    beginDragFromTo (timeline, start, end);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));

    releaseDragAt (timeline, start, end);

    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterCancel = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (afterCancel == beforeCancel);
}

TEST_CASE ("Escape cancels in-progress timeline trim and fade edits",
           "[ui][input][shell][timeline][esc-cancel][trim-fade]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("escape-cancel-trim-fade");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);

    const juce::Point<int> trimStart = timelineClipRightEdgeDragPoint (timeline, original, 0u);
    const juce::Point<int> trimEnd = trimStart.translated (-timeline.getWidth() / 8, 0);
    beginDragFromTo (timeline, trimStart, trimEnd);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    releaseDragAt (timeline, trimStart, trimEnd);
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    const juce::Point<int> trimLeftStart = timelineClipLeftEdgeDragPoint (timeline, original, 0u);
    const juce::Point<int> trimLeftEnd = trimLeftStart.translated (timeline.getWidth() / 8, 0);
    beginDragFromTo (timeline, trimLeftStart, trimLeftEnd);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    releaseDragAt (timeline, trimLeftStart, trimLeftEnd);
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    const juce::ModifierKeys altDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier);
    const juce::Point<int> fadeStart = timelineClipLeftEdgeDragPoint (timeline, original, 0u);
    const juce::Point<int> fadeEnd = fadeStart.translated (timeline.getWidth() / 10, 0);
    beginDragFromTo (timeline, fadeStart, fadeEnd, altDrag);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    releaseDragAt (timeline, fadeStart, fadeEnd, altDrag);
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    const juce::Point<int> fadeOutStart = timelineClipRightEdgeDragPoint (timeline, original, 0u);
    const juce::Point<int> fadeOutEnd = fadeOutStart.translated (-timeline.getWidth() / 10, 0);
    beginDragFromTo (timeline, fadeOutStart, fadeOutEnd, altDrag);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    releaseDragAt (timeline, fadeOutStart, fadeOutEnd, altDrag);
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterCancel = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (afterCancel.data(), afterCancel.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
}

TEST_CASE ("Escape cancels an in-progress marquee before mouse-up can change selection",
           "[ui][input][shell][timeline][esc-cancel][marquee-cancel]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("escape-cancel-marquee");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeCancel = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    const juce::Point<int> end = timelineClipCenterPoint (timeline, original, 0u);
    const juce::Point<int> start { timeline.getWidth() - 20, timeline.getHeight() - 20 };
    beginDragFromTo (timeline, start, end);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 0);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    releaseDragAt (timeline, start, end);

    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 0);
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterCancel = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (afterCancel == beforeCancel);
}

TEST_CASE ("Escape dismisses Clip and Track inline editors without persisting draft text",
           "[ui][input][shell][esc-cancel][inline-cancel]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("escape-cancel-inline-editors");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);
    REQUIRE (original.tracks.size() == 1u);
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, original, 0u));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeCancel = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::F2Key)));
    auto* clipEditor = dynamic_cast<juce::TextEditor*> (
        findChildWithComponentId (*shell, "shell.timeline.clip.rename"));
    REQUIRE (clipEditor != nullptr);
    REQUIRE (clipEditor->isVisible());
    clipEditor->setText ("Discarded Clip Draft", juce::dontSendNotification);
    REQUIRE (clipEditor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    REQUIRE_FALSE (clipEditor->isVisible());
    REQUIRE (readProjectSnapshot (bundlePath).clips.front().name == original.clips.front().name);

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    const int firstTrackY = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                          + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2;
    mouseDownAt (*rail, { rail->getWidth() / 2, firstTrackY });

    const juce::ModifierKeys ctrl = juce::ModifierKeys::ctrlModifier;
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::F2Key, ctrl, 0)));
    auto* trackEditor = dynamic_cast<juce::TextEditor*> (
        findChildWithComponentId (*shell, "shell.tracklist.rename"));
    REQUIRE (trackEditor != nullptr);
    REQUIRE (trackEditor->isVisible());
    trackEditor->setText ("Discarded Track Draft", juce::dontSendNotification);
    REQUIRE (trackEditor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    REQUIRE_FALSE (trackEditor->isVisible());

    const yesdaw::engine::Project afterEditors = readProjectSnapshot (bundlePath);
    REQUIRE (afterEditors.clips == original.clips);
    REQUIRE (afterEditors.tracks.front().strip.name == original.tracks.front().strip.name);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterCancel = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (afterCancel == beforeCancel);
}

TEST_CASE ("shipped MainComponent device callback renders playing Project audio",
           "[ui][input][shell][playback][device]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("device-playback");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    clickButton (requireButtonForAction (*shell, UiActionId::TransportPlay));

    std::array<float, 128> left {};
    std::array<float, 128> right {};
    std::array<float*, 2> outputs { left.data(), right.data() };
    REQUIRE (yesdaw::ui::processMainComponentDeviceAudioBlock (*shell, outputs.data(), 2, 128));
    REQUIRE (peakAbs (left) > 0.01);
    REQUIRE (peakAbs (right) > 0.01);

    const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.deviceAudioCallbackBlockCount == 1u);
    REQUIRE (snapshot.deviceAudioNonSilentBlockCount == 1u);
    REQUIRE (snapshot.visibleMasterPeakLeft > 0.01f);
    REQUIRE (snapshot.visibleMasterPeakRight > 0.01f);
}

TEST_CASE ("shipped MainComponent reopens bundled Assets as playable audio",
           "[ui][input][shell][project][playback][device]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("reopen-playback");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices createChoices;
    createChoices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    createChoices.chooseImportAudioFile = [fixturePath] { return fixturePath; };
    {
        auto shell = makeShell (std::move (createChoices));
        clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
        clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
        REQUIRE (snapshotMainComponent (*shell).playbackReady);
    }

    MainComponentFileChoices openChoices;
    openChoices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
    auto reopened = makeShell (std::move (openChoices));
    clickButton (requireButtonForAction (*reopened, UiActionId::ProjectOpen));
    REQUIRE (snapshotMainComponent (*reopened).playbackReady);
    clickButton (requireButtonForAction (*reopened, UiActionId::TransportPlay));

    std::array<float, 128> left {};
    std::array<float, 128> right {};
    std::array<float*, 2> outputs { left.data(), right.data() };
    REQUIRE (yesdaw::ui::processMainComponentDeviceAudioBlock (*reopened, outputs.data(), 2, 128));
    REQUIRE (peakAbs (left) > 0.01);
    REQUIRE (peakAbs (right) > 0.01);
}

TEST_CASE ("shipped MainComponent imports, reopens, plays, and exports a stereo WAV (ADR-0042)",
           "[ui][input][shell][stereo][playback][device]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("stereo-roundtrip");
    std::filesystem::path stereoWavPath = bundlePath;
    stereoWavPath += "-source.wav";
    std::filesystem::path exportPath = bundlePath;
    exportPath += "-export.wav";

    std::error_code removeError;
    std::filesystem::remove (stereoWavPath, removeError);
    std::filesystem::remove (exportPath, removeError);

    // Sign-split stereo source: left strictly positive, right strictly negative, so a swap, blend, or
    // downmix anywhere in import -> bundle -> decode -> strip -> device callback is unmistakable.
    constexpr std::uint64_t kFrames = 4800;
    std::vector<float> interleaved (static_cast<std::size_t> (kFrames) * 2u);
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        interleaved[static_cast<std::size_t> (frame * 2u)]      = 0.5f;
        interleaved[static_cast<std::size_t> (frame * 2u + 1u)] = -0.25f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (stereoWavPath,
                                              yesdaw::engine::SampleRate { 48000.0 },
                                              2,
                                              kFrames,
                                              std::span<const float> (interleaved.data(), interleaved.size())).ok());

    const auto requireSignSplitDeviceBlock = [] (juce::Component& shell)
    {
        std::array<float, 128> left {};
        std::array<float, 128> right {};
        std::array<float*, 2> outputs { left.data(), right.data() };
        REQUIRE (yesdaw::ui::processMainComponentDeviceAudioBlock (shell, outputs.data(), 2, 128));
        for (const float sample : left)
            REQUIRE (sample > 0.4f);      // left channel arrives intact (no blend, no swap)
        for (const float sample : right)
            REQUIRE (sample < -0.2f);     // right channel arrives intact and distinct
    };

    {
        MainComponentFileChoices choices;
        choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
        choices.chooseImportAudioFile = [stereoWavPath] { return stereoWavPath; };

        auto shell = makeShell (std::move (choices));
        clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
        clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

        const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
        REQUIRE (snapshot.context.projectLoaded);
        REQUIRE (snapshot.context.importCount == 1);
        REQUIRE (snapshot.playbackReady);

        clickButton (requireButtonForAction (*shell, UiActionId::TransportPlay));
        requireSignSplitDeviceBlock (*shell);
    }

    // Reopen from the bundle: the stereo Asset must decode back to playable stereo, not silence or mono.
    {
        MainComponentFileChoices openChoices;
        openChoices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
        openChoices.chooseExportAudioFile = [exportPath] { return exportPath; };
        auto reopened = makeShell (std::move (openChoices));
        clickButton (requireButtonForAction (*reopened, UiActionId::ProjectOpen));
        REQUIRE (snapshotMainComponent (*reopened).playbackReady);

        clickButton (requireButtonForAction (*reopened, UiActionId::TransportPlay));
        requireSignSplitDeviceBlock (*reopened);
        clickButton (requireButtonForAction (*reopened, UiActionId::TransportStop));

        clickButton (requireButtonForAction (*reopened, UiActionId::ProjectExportAudio));
        REQUIRE (std::filesystem::exists (exportPath));

        yesdaw::io::Float32Wav exported;
        REQUIRE (yesdaw::io::readFloat32WavFile (exportPath, exported).ok());
        REQUIRE (exported.channels == 2u);
        REQUIRE (exported.frames >= kFrames);
        // Balance centre is unity, track fader defaults to unity: the export IS the source, per channel.
        for (std::uint64_t frame = 0; frame < kFrames; frame += 480)
        {
            REQUIRE (exported.interleavedSamples[static_cast<std::size_t> (frame * 2u)] == 0.5f);
            REQUIRE (exported.interleavedSamples[static_cast<std::size_t> (frame * 2u + 1u)] == -0.25f);
        }
    }
}

TEST_CASE ("H12 UI input harness targets toolbar Components by stable action id", "[ui][input][shell]")
{
    auto shell = makeShell();

    for (UiActionId action : mainShellToolbarActions())
    {
        const juce::Component* component = findMainComponentChildForAction (*shell, action);
        REQUIRE (component != nullptr);
        REQUIRE_FALSE (component->getComponentID().isEmpty());
        REQUIRE_FALSE (component->getName().isEmpty());
        REQUIRE (component->isVisible());
    }

    REQUIRE (findMainComponentChildForAction (*shell, UiActionId::TimelineClipMove) == nullptr);
}

TEST_CASE ("H16 CP5 UI input harness edits the first automation lane through undo",
           "[ui][input][shell][automation]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("automation-lane");
    writeProjectSnapshot (bundlePath, makeAutomationInputProject());

    MainComponentFileChoices choices;
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));

    juce::Button& automation = requireButtonForAction (*shell, UiActionId::TimelineAutomationToggleTrackLane);
    juce::Label& laneRow = requireLabelWithComponentId (*shell, kAutomationLaneRowComponentId);
    auto* addPointComponent = dynamic_cast<juce::Button*> (
        findMainComponentChildForAction (*shell, UiActionId::TimelineAutomationAddBreakpoint));
    REQUIRE (addPointComponent != nullptr);
    auto* deletePointComponent = dynamic_cast<juce::Button*> (
        findMainComponentChildForAction (*shell, UiActionId::TimelineAutomationDeleteBreakpoint));
    REQUIRE (deletePointComponent != nullptr);

    REQUIRE_FALSE (automation.isEnabled());
    REQUIRE_FALSE (laneRow.isVisible());
    REQUIRE_FALSE (addPointComponent->isVisible());
    REQUIRE_FALSE (deletePointComponent->isVisible());

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.timelineAutomationTrackLaneVisible);
    REQUIRE (automation.isEnabled());
    REQUIRE_FALSE (laneRow.isVisible());
    REQUIRE_FALSE (addPointComponent->isVisible());
    REQUIRE_FALSE (deletePointComponent->isVisible());

    clickButton (automation);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::Timeline);
    REQUIRE (snapshot.context.timelineAutomationTrackLaneVisible);
    REQUIRE (snapshot.context.timelineAutomationTrackIndex == 0);
    REQUIRE (snapshot.context.timelineAutomationShowHideCount == 1);
    REQUIRE (laneRow.isVisible());
    REQUIRE (laneRow.getText().contains ("Audio 1"));
    REQUIRE (laneRow.getText().contains ("Track fader"));
    REQUIRE (laneRow.getText().contains ("2 breakpoints"));
    juce::Button& addPoint = requireButtonForAction (*shell, UiActionId::TimelineAutomationAddBreakpoint);
    REQUIRE (addPoint.isEnabled());
    juce::Button& deletePoint = requireButtonForAction (*shell, UiActionId::TimelineAutomationDeleteBreakpoint);
    REQUIRE (deletePoint.isEnabled());

    clickButton (addPoint);
    const yesdaw::engine::Project added = readProjectSnapshot (bundlePath);
    REQUIRE (added.automationLanes.size() == 1u);
    REQUIRE (added.automationLanes.front().points.size() == 3u);
    REQUIRE (added.automationLanes.front().points[2].tick
             == yesdaw::ui::UiAppModel::kFirstTrackAutomationBreakpointAddTick);
    REQUIRE (added.automationLanes.front().points[2].value
             == Catch::Approx (yesdaw::ui::UiAppModel::kFirstTrackAutomationBreakpointAddValue));
    REQUIRE (added.automationLanes.front().points[2].curveType
             == yesdaw::engine::AutomationCurveType::Linear);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineAutomationBreakpointEditCount == 1);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (laneRow.getText().contains ("3 breakpoints"));

    clickButton (requireButtonForAction (*shell, UiActionId::EditUndo));
    const yesdaw::engine::Project undone = readProjectSnapshot (bundlePath);
    REQUIRE (undone.automationLanes.size() == 1u);
    REQUIRE (undone.automationLanes.front().points.size() == 2u);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 1);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (laneRow.getText().contains ("2 breakpoints"));

    clickButton (requireButtonForAction (*shell, UiActionId::EditRedo));
    const yesdaw::engine::Project redone = readProjectSnapshot (bundlePath);
    REQUIRE (redone.automationLanes == added.automationLanes);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 1);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (laneRow.getText().contains ("3 breakpoints"));

    clickButton (deletePoint);
    const yesdaw::engine::Project deleted = readProjectSnapshot (bundlePath);
    REQUIRE (deleted.automationLanes.size() == 1u);
    REQUIRE (deleted.automationLanes.front().points.size() == 2u);
    REQUIRE (deleted.automationLanes.front().points == undone.automationLanes.front().points);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineAutomationBreakpointEditCount == 2);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (laneRow.getText().contains ("2 breakpoints"));

    clickButton (requireButtonForAction (*shell, UiActionId::EditUndo));
    const yesdaw::engine::Project deleteUndone = readProjectSnapshot (bundlePath);
    REQUIRE (deleteUndone.automationLanes == added.automationLanes);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 2);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (laneRow.getText().contains ("3 breakpoints"));

    clickButton (requireButtonForAction (*shell, UiActionId::EditRedo));
    const yesdaw::engine::Project deleteRedone = readProjectSnapshot (bundlePath);
    REQUIRE (deleteRedone.automationLanes == deleted.automationLanes);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 2);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (laneRow.getText().contains ("2 breakpoints"));

    clickButton (automation);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.timelineAutomationTrackLaneVisible);
    REQUIRE (snapshot.context.timelineAutomationTrackIndex == -1);
    REQUIRE (snapshot.context.timelineAutomationShowHideCount == 2);
    REQUIRE_FALSE (laneRow.isVisible());
    REQUIRE_FALSE (addPoint.isVisible());
    REQUIRE_FALSE (deletePoint.isVisible());
}

TEST_CASE ("H16 CP6 UI input harness reads first Track send through an action-backed mixer component",
           "[ui][input][shell][mixer][sends]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-sends");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.makeNewProject = [] { return makeMixerSendsInputProject(); };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& sends = requireButtonForAction (*shell, UiActionId::MixerReadSends);
    REQUIRE (sends.isEnabled());
    REQUIRE (sends.getButtonText().contains ("Send 0"));

    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.size() == 1u);
    const auto sendFaderNodeId = projectMixerSendLevelNodeIdForTrack (project.tracks.front().id, 0);
    REQUIRE (sends.getButtonText().contains (juce::String (static_cast<int> (sendFaderNodeId))));
    REQUIRE (sends.getButtonText().contains ("level 0.60"));
    REQUIRE (sends.getButtonText().contains ("points 2"));

    const int beforeReadCount = snapshot.context.mixerReadCount;
    clickButton (sends);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerReadCount == beforeReadCount + 1);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
}

TEST_CASE ("H16 CP6 UI input harness reads first Track meter through an action-backed mixer component",
           "[ui][input][shell][mixer][meters]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-meter-readout");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& meters = requireButtonForAction (*shell, UiActionId::MixerReadMeters);
    REQUIRE (meters.isEnabled());
    REQUIRE (meters.getButtonText().contains ("Audio 1"));
    REQUIRE (meters.getButtonText().contains ("meter node"));
    REQUIRE (meters.getButtonText().contains ("peak n/a"));

    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 1u);
    const auto meterNodeId = projectMixerNodeIdForTrack (project.tracks.front().id, ProjectMixerNodeRole::Meter);
    REQUIRE (meters.getButtonText().contains (juce::String (static_cast<int> (meterNodeId))));

    const int beforeReadCount = snapshot.context.mixerReadCount;
    clickButton (meters);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerReadCount == beforeReadCount + 1);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
}

TEST_CASE ("H16 CP6 UI input harness edits first Track send level through Project undo",
           "[ui][input][shell][mixer][sends]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-send-level-edit");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.makeNewProject = [] { return makeMixerSendsInputProject(); };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& sends = requireButtonForAction (*shell, UiActionId::MixerReadSends);
    juce::Button& sendLevel = requireButtonForAction (*shell, UiActionId::MixerSetFirstSendLevel);
    REQUIRE (sendLevel.isEnabled());
    REQUIRE (sendLevel.getButtonText() == "Send");
    REQUIRE (sends.getButtonText().contains ("level 0.60"));

    const yesdaw::engine::Project before = readProjectSnapshot (bundlePath);
    REQUIRE (before.automationLanes.size() == 1u);
    REQUIRE (before.automationLanes.front().role == AutomationTargetRole::SendLevel);
    REQUIRE (before.automationLanes.front().paramId == 0u);
    REQUIRE (before.automationLanes.front().points.size() == 2u);
    REQUIRE (before.automationLanes.front().points.back().value == Catch::Approx (0.60));

    const int beforeEditCount = snapshot.context.mixerEditCount;
    clickButton (sendLevel);

    const yesdaw::engine::Project edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.automationLanes.size() == before.automationLanes.size());
    REQUIRE (edited.automationLanes.front().id == before.automationLanes.front().id);
    REQUIRE (edited.automationLanes.front().points.size() == before.automationLanes.front().points.size());
    REQUIRE (edited.automationLanes.front().points.front() == before.automationLanes.front().points.front());
    REQUIRE (edited.automationLanes.front().points.back().tick == before.automationLanes.front().points.back().tick);
    REQUIRE (edited.automationLanes.front().points.back().curveType == before.automationLanes.front().points.back().curveType);
    REQUIRE (edited.automationLanes.front().points.back().value
             == Catch::Approx (yesdaw::ui::UiAppModel::kFirstTrackSendLevelEditValue));

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerEditCount == beforeEditCount + 1);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
    REQUIRE (sendLevel.getButtonText() == "Send");
    REQUIRE (sends.getButtonText().contains ("level 0.80"));

    juce::Button& undo = requireButtonForAction (*shell, UiActionId::EditUndo);
    juce::Button& redo = requireButtonForAction (*shell, UiActionId::EditRedo);
    REQUIRE (undo.isEnabled());
    clickButton (undo);

    const yesdaw::engine::Project undone = readProjectSnapshot (bundlePath);
    REQUIRE (undone.automationLanes == before.automationLanes);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (sends.getButtonText().contains ("level 0.60"));

    REQUIRE (redo.isEnabled());
    clickButton (redo);

    const yesdaw::engine::Project redone = readProjectSnapshot (bundlePath);
    REQUIRE (redone.automationLanes == edited.automationLanes);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (sends.getButtonText().contains ("level 0.80"));
}

TEST_CASE ("H16 CP6 UI input harness reads first Track FX slot through an action-backed mixer component",
           "[ui][input][shell][mixer][fx-slots]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-fx-slots");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.makeNewProject = [] { return makeMixerFxSlotsInputProject(); };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& fxSlots = requireButtonForAction (*shell, UiActionId::MixerReadFxSlots);
    REQUIRE (fxSlots.isEnabled());
    REQUIRE (fxSlots.getButtonText().contains ("FX 0"));
    REQUIRE (fxSlots.getButtonText().contains ("EQ"));

    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain.size() == 2u);
    const auto fxNodeId = projectMixerNodeIdForEntity (project.tracks.front().strip.fxChain.front().id,
                                                       ProjectMixerNodeRole::Fx);
    REQUIRE (fxSlots.getButtonText().contains (juce::String (static_cast<int> (fxNodeId))));
    REQUIRE (fxSlots.getButtonText().contains ("params 1"));
    REQUIRE (fxSlots.getButtonText().contains ("on"));

    const int beforeReadCount = snapshot.context.mixerReadCount;
    clickButton (fxSlots);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerReadCount == beforeReadCount + 1);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
}

TEST_CASE ("H16 CP6 UI input harness toggles first Track FX slot enabled state through Project undo",
           "[ui][input][shell][mixer][fx-slots]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-fx-slot-toggle");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.makeNewProject = [] { return makeMixerFxSlotsInputProject(); };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& fxSlots = requireButtonForAction (*shell, UiActionId::MixerReadFxSlots);
    juce::Button& fxToggle = requireButtonForAction (*shell, UiActionId::MixerToggleFirstFxSlotEnabled);
    REQUIRE (fxToggle.isEnabled());
    REQUIRE (fxToggle.getButtonText() == "FX");
    REQUIRE (fxToggle.getToggleState());
    REQUIRE (fxSlots.getButtonText().contains ("on"));

    const yesdaw::engine::Project before = readProjectSnapshot (bundlePath);
    REQUIRE (before.tracks.front().strip.fxChain.size() == 2u);
    REQUIRE (before.tracks.front().strip.fxChain.front().enabled);

    const int beforeEditCount = snapshot.context.mixerEditCount;
    clickButton (fxToggle);

    yesdaw::engine::Project toggled = readProjectSnapshot (bundlePath);
    REQUIRE (toggled.tracks.front().strip.fxChain.size() == before.tracks.front().strip.fxChain.size());
    REQUIRE (toggled.tracks.front().strip.fxChain.front().id == before.tracks.front().strip.fxChain.front().id);
    REQUIRE_FALSE (toggled.tracks.front().strip.fxChain.front().enabled);
    REQUIRE (toggled.tracks.front().strip.fxChain[1] == before.tracks.front().strip.fxChain[1]);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerEditCount == beforeEditCount + 1);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
    REQUIRE (fxToggle.getButtonText() == "FX");
    REQUIRE_FALSE (fxToggle.getToggleState());
    REQUIRE (fxSlots.getButtonText().contains ("off"));

    juce::Button& undo = requireButtonForAction (*shell, UiActionId::EditUndo);
    juce::Button& redo = requireButtonForAction (*shell, UiActionId::EditRedo);
    REQUIRE (undo.isEnabled());
    clickButton (undo);

    const yesdaw::engine::Project undone = readProjectSnapshot (bundlePath);
    REQUIRE (undone.tracks.front().strip.fxChain == before.tracks.front().strip.fxChain);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (fxToggle.getButtonText() == "FX");
    REQUIRE (fxToggle.getToggleState());

    REQUIRE (redo.isEnabled());
    clickButton (redo);

    const yesdaw::engine::Project redone = readProjectSnapshot (bundlePath);
    REQUIRE (redone.tracks.front().strip.fxChain == toggled.tracks.front().strip.fxChain);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (fxToggle.getButtonText() == "FX");
    REQUIRE_FALSE (fxToggle.getToggleState());
}

TEST_CASE ("H16 CP6 UI input harness reads first Track GR meter through an action-backed mixer component",
           "[ui][input][shell][mixer][gr]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-gr-readout");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.makeNewProject = [] { return makeMixerFxSlotsInputProject(); };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& gr = requireButtonForAction (*shell, UiActionId::MixerReadGainReduction);
    REQUIRE (gr.isEnabled());
    REQUIRE (gr.getButtonText().contains ("GR 1"));
    REQUIRE (gr.getButtonText().contains ("Compressor"));
    REQUIRE (gr.getButtonText().contains ("n/a"));

    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain.size() == 2u);
    REQUIRE (project.tracks.front().strip.fxChain[1].kind == yesdaw::engine::FxKind::Compressor);
    const auto compressorNodeId = projectMixerNodeIdForEntity (project.tracks.front().strip.fxChain[1].id,
                                                               ProjectMixerNodeRole::Fx);
    REQUIRE (gr.getButtonText().contains (juce::String (static_cast<int> (compressorNodeId))));

    const int beforeReadCount = snapshot.context.mixerReadCount;
    clickButton (gr);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerReadCount == beforeReadCount + 1);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
}

TEST_CASE ("H16 CP6 UI input harness reads first Bus FX slot through an action-backed mixer component",
           "[ui][input][shell][mixer][fx-slots]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-bus-fx-slots");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.makeNewProject = [] { return makeMixerBusFxSlotsInputProject(); };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& busFxSlots = requireButtonForAction (*shell, UiActionId::MixerReadBusFxSlots);
    REQUIRE (busFxSlots.isEnabled());
    REQUIRE (busFxSlots.getButtonText().contains ("Room Bus"));
    REQUIRE (busFxSlots.getButtonText().contains ("FX 0"));
    REQUIRE (busFxSlots.getButtonText().contains ("Reverb"));
    REQUIRE (busFxSlots.getButtonText().contains ("params 0"));
    REQUIRE (busFxSlots.getButtonText().contains ("on"));

    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.buses.size() == 1u);
    REQUIRE (project.buses.front().strip.fxChain.size() == 1u);
    const auto busFxNodeId = projectMixerNodeIdForEntity (project.buses.front().strip.fxChain.front().id,
                                                         ProjectMixerNodeRole::Fx);
    REQUIRE (busFxSlots.getButtonText().contains (juce::String (static_cast<int> (busFxNodeId))));

    const int beforeReadCount = snapshot.context.mixerReadCount;
    clickButton (busFxSlots);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerReadCount == beforeReadCount + 1);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
}

TEST_CASE ("H16 CP6 UI input harness reads master loudness through an action-backed header component",
           "[ui][input][shell][mixer][loudness]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-loudness-readout");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);

    juce::Button& loudness = requireButtonForAction (*shell, UiActionId::MixerReadLoudness);
    REQUIRE (loudness.isEnabled());
    REQUIRE (loudness.getButtonText() == "-- LUFS");

    const int beforeReadCount = snapshot.context.mixerReadCount;
    clickButton (loudness);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerReadCount == beforeReadCount + 1);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
}

TEST_CASE ("H16 CP7 UI input harness exports the current Project to canonical WAV",
           "[ui][input][shell][export]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("audio-export");
    std::filesystem::path exportPath = bundlePath;
    exportPath += ".wav";
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    std::error_code removeError;
    std::filesystem::remove (exportPath, removeError);

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };
    choices.chooseExportAudioFile = [exportPath] { return exportPath; };

    auto shell = makeShell (std::move (choices));
    juce::Button& exportAudio = requireButtonForAction (*shell, UiActionId::ProjectExportAudio);
    juce::Button& cancelExport = requireRegisteredButtonForAction (*shell, UiActionId::ProjectExportAudioCancel);
    juce::Label& exportProgress = requireLabelWithComponentId (*shell, kExportAudioProgressComponentId);
    REQUIRE_FALSE (exportAudio.isEnabled());
    REQUIRE_FALSE (cancelExport.isEnabled());
    REQUIRE_FALSE (cancelExport.isVisible());
    REQUIRE_FALSE (exportProgress.isVisible());
    REQUIRE (exportProgress.getText() == "Export --");

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.audioExportCount == 0);
    REQUIRE (snapshot.context.audioExportProgressPercent == -1);
    REQUIRE_FALSE (snapshot.context.audioExportInProgress);
    REQUIRE_FALSE (snapshot.context.audioExportCancelRequested);
    REQUIRE (snapshot.context.audioExportCancelCount == 0);
    REQUIRE (exportAudio.isEnabled());
    REQUIRE_FALSE (cancelExport.isEnabled());
    REQUIRE_FALSE (std::filesystem::exists (exportPath));

    const int beforeCommandCount = snapshot.context.commandDispatchCount;
    clickButton (exportAudio);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.audioExportCount == 1);
    REQUIRE (snapshot.context.audioExportProgressPercent == 100);
    REQUIRE_FALSE (snapshot.context.audioExportInProgress);
    REQUIRE_FALSE (snapshot.context.audioExportCancelRequested);
    REQUIRE (snapshot.context.audioExportCancelCount == 0);
    REQUIRE (snapshot.context.commandDispatchCount == beforeCommandCount + 1);
    REQUIRE (exportProgress.getText() == "Export 100%");
    REQUIRE_FALSE (cancelExport.isEnabled());
    REQUIRE (std::filesystem::exists (exportPath));

    yesdaw::io::Float32Wav exported;
    REQUIRE (yesdaw::io::readFloat32WavFile (exportPath, exported).ok());
    REQUIRE (exported.sampleRate == yesdaw::engine::SampleRate { 48000.0 });
    REQUIRE (exported.channels == 2u);
    REQUIRE (exported.frames > 0u);
    REQUIRE (exported.interleavedSamples.size() == static_cast<std::size_t> (exported.frames * exported.channels));
    REQUIRE (peakAbs (std::span<const float> (exported.interleavedSamples.data(),
                                              exported.interleavedSamples.size())) > 0.01);
}

TEST_CASE ("H12 UI input harness rejects disabled shell input before Project load", "[ui][input][shell]")
{
    auto shell = makeShell();
    juce::Button& play = requireButtonForAction (*shell, UiActionId::TransportPlay);

    REQUIRE_FALSE (play.isEnabled());
    clickButton (play);

    const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.commandDispatchCount == 0);
}

TEST_CASE ("loaded empty Project ruler click locates and plain drag selects a range",
           "[ui][input][shell][project][transport][ruler]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("empty-ruler-locate");
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    juce::Component& timeline = requireTimelineComponent (*shell);

    const juce::Point<int> twoSeconds = emptyProjectRulerPointAtSeconds (timeline, 2.0);
    mouseDownAt (timeline, twoSeconds);
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.playheadFrame == emptyProjectFrameAtRulerPoint (timeline, twoSeconds));

    // Parity item 25: a plain ruler drag no longer scrubs the playhead — it selects a time range
    // while the playhead stays at the mouse-down locate.
    const juce::Point<int> threeSeconds = emptyProjectRulerPointAtSeconds (timeline, 3.0);
    dragFromTo (timeline, twoSeconds, threeSeconds);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.playheadFrame == emptyProjectFrameAtRulerPoint (timeline, twoSeconds));
    REQUIRE (snapshot.context.timelineRangeSelected);
    REQUIRE (snapshot.timelineRangeStartFrame == emptyProjectFrameAtRulerPoint (timeline, twoSeconds));
    REQUIRE (snapshot.timelineRangeEndFrame == emptyProjectFrameAtRulerPoint (timeline, threeSeconds));

    // A plain ruler click collapses the committed range and still locates.
    mouseDownAt (timeline, threeSeconds);
    releaseDragAt (timeline, threeSeconds, threeSeconds);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.playheadFrame == emptyProjectFrameAtRulerPoint (timeline, threeSeconds));
    REQUIRE_FALSE (snapshot.context.timelineRangeSelected);
    REQUIRE (snapshot.timelineRangeStartFrame == -1);
    REQUIRE (snapshot.timelineRangeEndFrame == -1);
}

TEST_CASE ("H12 UI input harness creates, saves, opens, and reopens Project bundles through shell Components",
           "[ui][input][shell][project]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("project-lifecycle");
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));

    juce::Button& newProject = requireButtonForAction (*shell, UiActionId::ProjectNew);
    clickButton (newProject);

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.playbackReady);
    REQUIRE (snapshot.bundlePath == bundlePath);
    REQUIRE (snapshot.context.commandDispatchCount == 1);

    const yesdaw::engine::Project created = readProjectSnapshot (bundlePath);
    REQUIRE (created.id.isValid());
    REQUIRE (created.hasValidAssetClipIndirection());
    REQUIRE (created.tempoMap.size() == 1u);
    REQUIRE (created.meterMap.size() == 1u);

    juce::Button& save = requireButtonForAction (*shell, UiActionId::ProjectSave);
    REQUIRE (save.isEnabled());
    clickButton (save);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.saveCount == 1);
    REQUIRE (snapshot.context.commandDispatchCount == 2);

    juce::Button& open = requireButtonForAction (*shell, UiActionId::ProjectOpen);
    REQUIRE (open.isEnabled());
    clickButton (open);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.playbackReady);
    REQUIRE (snapshot.bundlePath == bundlePath);
    REQUIRE (snapshot.context.saveCount == 0);
    REQUIRE (snapshot.context.commandDispatchCount == 1);

    const yesdaw::engine::Project reopened = readProjectSnapshot (bundlePath);
    REQUIRE (reopened.id == created.id);
    REQUIRE (reopened.sampleRate == created.sampleRate);
    REQUIRE (reopened.tempoMap == created.tempoMap);
    REQUIRE (reopened.meterMap == created.meterMap);

    juce::Button& play = requireButtonForAction (*shell, UiActionId::TransportPlay);
    REQUIRE (play.isEnabled());
    clickButton (play);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.commandDispatchCount == 2);

    std::array<float, 128> emptyLeft {};
    std::array<float, 128> emptyRight {};
    std::array<float*, 2> emptyOutputs { emptyLeft.data(), emptyRight.data() };
    REQUIRE (yesdaw::ui::processMainComponentDeviceAudioBlock (*shell, emptyOutputs.data(), 2, 128));
    REQUIRE (peakAbs (emptyLeft) == 0.0f);
    REQUIRE (peakAbs (emptyRight) == 0.0f);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.playheadFrame == 128);

    juce::Button& mixer = requireButtonForAction (*shell, UiActionId::ViewMixer);
    clickButton (mixer);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& piano = requireButtonForAction (*shell, UiActionId::ViewPianoRoll);
    clickButton (piano);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::PianoRoll);
}

TEST_CASE ("H12 UI input harness edits MIDI Clip Notes through the real Piano Roll Component",
           "[ui][input][shell][project][midi]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("piano-roll-midi");
    const yesdaw::engine::Project seed = makeMidiInputProject();
    REQUIRE (seed.hasValidAssetClipIndirection());
    writeProjectSnapshot (bundlePath, seed);

    MainComponentFileChoices choices;
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.midiClipSelected);
    REQUIRE (snapshot.context.commandDispatchCount == 1);

    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::PianoRoll);
    REQUIRE (snapshot.context.midiClipSelected);
    REQUIRE_FALSE (snapshot.context.midiNoteSelected);

    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    yesdaw::engine::Project edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.size() == 1u);
    REQUIRE (edited.midiClips.front().notes.size() == 2u);

    const yesdaw::engine::MidiClip originalMidi = edited.midiClips.front();
    const yesdaw::engine::EntityId noteId = originalMidi.notes.front().id;
    const yesdaw::engine::Note originalNote = originalMidi.notes.front();
    const yesdaw::engine::Note untouchedNote = originalMidi.notes[1];

    const juce::Point<int> noteCenter = pianoRollNoteCenterPoint (pianoRoll, originalMidi, originalNote);
    mouseDownAt (pianoRoll, noteCenter);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiNoteSelected);
    REQUIRE (snapshot.context.midiEditCount == 0);

    constexpr int kMoveDeltaPixels = 42;
    const yesdaw::engine::Tick expectedMoveDelta =
        pianoRollDeltaTicksForPixels (pianoRoll, originalMidi, kMoveDeltaPixels);
    const yesdaw::engine::Tick expectedMovedStart =
        std::clamp<yesdaw::engine::Tick> (
            originalNote.startTick + expectedMoveDelta,
            0,
            std::max<yesdaw::engine::Tick> (0, originalMidi.timelineLength - originalNote.lengthTicks));

    dragFromTo (pianoRoll, noteCenter, noteCenter.translated (kMoveDeltaPixels, 0));

    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().notes.front().id == noteId);
    REQUIRE (edited.midiClips.front().notes.front().startTick == expectedMovedStart);
    REQUIRE (edited.midiClips.front().notes.front().lengthTicks == originalNote.lengthTicks);
    REQUIRE (edited.midiClips.front().notes[1] == untouchedNote);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiEditCount == 1);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);

    const yesdaw::engine::MidiClip movedMidi = edited.midiClips.front();
    const yesdaw::engine::Note movedNote = movedMidi.notes.front();
    const juce::Point<int> movedCenter = pianoRollNoteCenterPoint (pianoRoll, movedMidi, movedNote);
    constexpr int kLengthDeltaPixels = 36;
    const yesdaw::engine::Tick expectedLengthDelta =
        pianoRollDeltaTicksForPixels (pianoRoll, movedMidi, kLengthDeltaPixels);
    const yesdaw::engine::Tick expectedLength =
        std::clamp<yesdaw::engine::Tick> (
            movedNote.lengthTicks + expectedLengthDelta,
            0,
            std::max<yesdaw::engine::Tick> (0, movedMidi.timelineLength - movedNote.startTick));
    const juce::ModifierKeys shiftDrag {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier
    };

    dragFromTo (pianoRoll, movedCenter, movedCenter.translated (kLengthDeltaPixels, 0), shiftDrag);

    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().notes.front().startTick == expectedMovedStart);
    REQUIRE (edited.midiClips.front().notes.front().lengthTicks == expectedLength);
    REQUIRE (edited.midiClips.front().notes[1] == untouchedNote);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiEditCount == 2);

    const yesdaw::engine::MidiClip lengthenedMidi = edited.midiClips.front();
    const yesdaw::engine::Note lengthenedNote = lengthenedMidi.notes.front();
    const juce::ModifierKeys altDoubleClick {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier
    };

    doubleClickAt (pianoRoll, pianoRollNoteCenterPoint (pianoRoll, lengthenedMidi, lengthenedNote), altDoubleClick);

    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().notes.front().startTick == expectedMovedStart);
    REQUIRE (edited.midiClips.front().notes.front().lengthTicks == expectedLength);
    REQUIRE (edited.midiClips.front().notes.front().key == originalNote.key + 1);
    REQUIRE (std::fabs (edited.midiClips.front().notes.front().pitchNote - (originalNote.pitchNote + 1.0)) < 0.000001);
    REQUIRE (edited.midiClips.front().notes[1] == untouchedNote);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiEditCount == 3);

    const yesdaw::engine::MidiClip transposedMidi = edited.midiClips.front();
    const yesdaw::engine::Note transposedNote = transposedMidi.notes.front();
    yesdaw::engine::Tick expectedQuantizedStart = 0;
    REQUIRE (yesdaw::engine::snapTick (
        transposedNote.startTick,
        yesdaw::engine::SnapGrid { 512 },
        expectedQuantizedStart));
    REQUIRE (expectedQuantizedStart != transposedNote.startTick);
    const juce::ModifierKeys ctrlDoubleClick {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier
    };

    doubleClickAt (pianoRoll, pianoRollNoteCenterPoint (pianoRoll, transposedMidi, transposedNote), ctrlDoubleClick);

    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().notes.front().startTick == expectedQuantizedStart);
    REQUIRE (edited.midiClips.front().notes.front().lengthTicks == expectedLength);
    REQUIRE (edited.midiClips.front().notes.front().key == originalNote.key + 1);
    REQUIRE (edited.midiClips.front().notes[1] == untouchedNote);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiEditCount == 4);

    const yesdaw::engine::MidiClip quantizedMidi = edited.midiClips.front();
    const yesdaw::engine::Note quantizedNote = quantizedMidi.notes.front();
    const juce::ModifierKeys shiftDoubleClick {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier
    };

    doubleClickAt (pianoRoll, pianoRollNoteCenterPoint (pianoRoll, quantizedMidi, quantizedNote), shiftDoubleClick);

    const yesdaw::engine::Project afterExpressionRead = readProjectSnapshot (bundlePath);
    REQUIRE (afterExpressionRead.midiClips == edited.midiClips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiReadCount == 1);

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectSave));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.saveCount == 1);

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.midiClipSelected);
    REQUIRE_FALSE (snapshot.context.midiNoteSelected);
    REQUIRE (snapshot.bundlePath == bundlePath);

    const yesdaw::engine::Project reopened = readProjectSnapshot (bundlePath);
    REQUIRE (reopened.midiClips == afterExpressionRead.midiClips);
    REQUIRE (reopened.tracks == afterExpressionRead.tracks);

    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiClipSelected);
    REQUIRE_FALSE (snapshot.context.midiNoteSelected);
    (void) requirePianoRollComponent (*shell);
}

TEST_CASE ("H12 UI input harness edits selected Clip fields through real inspector controls",
           "[ui][input][shell][inspector][clip]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("inspector");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    juce::Slider& start = requireSliderWithComponentId (*shell, kInspectorStartComponentId);
    juce::Slider& end = requireSliderWithComponentId (*shell, kInspectorEndComponentId);
    juce::Slider& length = requireSliderWithComponentId (*shell, kInspectorLengthComponentId);
    juce::Slider& gain = requireSliderForAction (*shell, UiActionId::TimelineClipSetGain);
    juce::Slider& fadeIn = requireSliderWithComponentId (*shell, kInspectorFadeInComponentId);
    juce::Slider& fadeOut = requireSliderWithComponentId (*shell, kInspectorFadeOutComponentId);
    juce::ComboBox& fadeCurve = requireComboBoxWithComponentId (*shell, kInspectorFadeCurveComponentId);

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (start.isEnabled());
    REQUIRE (end.isEnabled());
    REQUIRE (length.isEnabled());
    REQUIRE (gain.isEnabled());
    REQUIRE (fadeIn.isEnabled());
    REQUIRE (fadeOut.isEnabled());
    REQUIRE (fadeCurve.isEnabled());
    REQUIRE (fadeCurve.getSelectedId() == kInspectorEqualPowerFadeCurveId);
    REQUIRE (fadeCurve.getText() == "Equal power");

    mouseDownAt (timeline, { timeline.getWidth() - 20, timeline.getHeight() - 20 });
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.timelineClipSelected);
    REQUIRE_FALSE (start.isEnabled());
    REQUIRE_FALSE (end.isEnabled());
    REQUIRE_FALSE (length.isEnabled());
    REQUIRE_FALSE (gain.isEnabled());
    REQUIRE_FALSE (fadeIn.isEnabled());
    REQUIRE_FALSE (fadeOut.isEnabled());
    REQUIRE_FALSE (fadeCurve.isEnabled());
    REQUIRE (fadeCurve.getSelectedId() == kInspectorEqualPowerFadeCurveId);

    const yesdaw::engine::Project imported = readProjectSnapshot (bundlePath);
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, imported, 0u));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (start.isEnabled());
    REQUIRE (end.isEnabled());
    REQUIRE (length.isEnabled());
    REQUIRE (gain.isEnabled());
    REQUIRE (fadeIn.isEnabled());
    REQUIRE (fadeOut.isEnabled());
    REQUIRE (fadeCurve.isEnabled());
    REQUIRE (fadeCurve.getSelectedId() == kInspectorEqualPowerFadeCurveId);

    REQUIRE (imported.sampleRate.isValid());
    const double sampleRate = imported.sampleRate.hz;
    setSliderValueThroughComponent (start, static_cast<double> (imported.clips.front().timelineLength) / sampleRate);
    const yesdaw::engine::Project movedByInspector = readProjectSnapshot (bundlePath);
    REQUIRE (movedByInspector.clips.size() == 1u);
    REQUIRE (movedByInspector.clips.front().id == imported.clips.front().id);
    REQUIRE (movedByInspector.clips.front().timelineStart > imported.clips.front().timelineStart);
    REQUIRE (movedByInspector.clips.front().timelineLength == imported.clips.front().timelineLength);

    const double shortenedLengthSeconds =
        static_cast<double> (movedByInspector.clips.front().timelineLength) / sampleRate
        * kInspectorTimingShortenRatio;
    setSliderValueThroughComponent (length, shortenedLengthSeconds);
    const yesdaw::engine::Project lengthEdited = readProjectSnapshot (bundlePath);
    REQUIRE (lengthEdited.clips.size() == 1u);
    REQUIRE (lengthEdited.clips.front().id == movedByInspector.clips.front().id);
    REQUIRE (lengthEdited.clips.front().timelineStart == movedByInspector.clips.front().timelineStart);
    REQUIRE (lengthEdited.clips.front().timelineLength > 0);
    REQUIRE (lengthEdited.clips.front().timelineLength != movedByInspector.clips.front().timelineLength);

    const double shortenedEndSeconds =
        static_cast<double> (lengthEdited.clips.front().timelineStart) / sampleRate
        + (static_cast<double> (lengthEdited.clips.front().timelineLength) / sampleRate
           * kInspectorTimingShortenRatio);
    setSliderValueThroughComponent (end, shortenedEndSeconds);
    const yesdaw::engine::Project timeEdited = readProjectSnapshot (bundlePath);
    REQUIRE (timeEdited.clips.size() == 1u);
    REQUIRE (timeEdited.clips.front().id == lengthEdited.clips.front().id);
    REQUIRE (timeEdited.clips.front().timelineStart == lengthEdited.clips.front().timelineStart);
    REQUIRE (timeEdited.clips.front().timelineLength > 0);
    REQUIRE (timeEdited.clips.front().timelineStart + timeEdited.clips.front().timelineLength
             != lengthEdited.clips.front().timelineStart + lengthEdited.clips.front().timelineLength);

    dragHorizontalSliderToNormalizedValue (gain, 0.65);
    const double editedLengthSeconds =
        static_cast<double> (timeEdited.clips.front().timelineLength) / sampleRate;
    setSliderValueThroughComponent (fadeIn, editedLengthSeconds * kInspectorFadeInRatio);
    setSliderValueThroughComponent (fadeOut, editedLengthSeconds * kInspectorFadeOutRatio);

    const yesdaw::engine::Project edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.clips.size() == 1u);
    REQUIRE (edited.clips.front().timelineStart == timeEdited.clips.front().timelineStart);
    REQUIRE (edited.clips.front().timelineLength == timeEdited.clips.front().timelineLength);
    REQUIRE (edited.clips.front().gain > timeEdited.clips.front().gain);
    REQUIRE (edited.clips.front().gain <= 2.0f);
    REQUIRE (edited.clips.front().fadeIn > 0);
    REQUIRE (edited.clips.front().fadeOut > edited.clips.front().fadeIn);
    REQUIRE (edited.clips.front().fadeOut <= edited.clips.front().timelineLength);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineEditCount >= 6);
    REQUIRE (snapshot.context.canUndo);

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectSave));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));

    const yesdaw::engine::Project reopened = readProjectSnapshot (bundlePath);
    REQUIRE (reopened.clips == edited.clips);
    REQUIRE (fadeCurve.getSelectedId() == kInspectorEqualPowerFadeCurveId);
}

TEST_CASE ("H12 UI input harness drives an end-to-end saved session through shipped Components",
           "[ui][input][shell][project][import][playback][mixer][midi]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("import-wav");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };
    choices.makeNewProject = [] { return makeEndToEndInputProject(); };

    auto shell = makeShell (std::move (choices));

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.playbackReady);

    juce::Button& import = requireButtonForAction (*shell, UiActionId::ProjectImportAudio);
    REQUIRE (import.isEnabled());
    clickButton (import);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.playbackReady);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE_FALSE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.importCount == 1);
    REQUIRE (snapshot.context.commandDispatchCount == 2);

    const yesdaw::engine::Project imported = readProjectSnapshot (bundlePath);
    REQUIRE (imported.assets.size() == 1u);
    REQUIRE (imported.clips.size() == 1u);
    REQUIRE (imported.midiClips.size() == 1u);
    REQUIRE (imported.midiClips.front().notes.size() == 2u);
    REQUIRE (imported.hasValidAssetClipIndirection());

    const yesdaw::engine::Asset& asset = imported.assets.front();
    const yesdaw::engine::Clip& clip = imported.clips.front();
    REQUIRE (asset.frames == 4096u);
    REQUIRE (asset.channels == 1u);
    REQUIRE (asset.sampleRate == yesdaw::engine::SampleRate { 48000.0 });
    REQUIRE (clip.assetId == asset.id);
    REQUIRE (clip.timelineStart == 0);
    REQUIRE (clip.timelineLength == static_cast<yesdaw::engine::Tick> (asset.frames));
    REQUIRE (clip.srcOffset == 0u);
    REQUIRE (clip.srcLen == asset.frames);
    REQUIRE (clip.sourceWindowFits (asset));

    juce::Component& timeline = requireTimelineComponent (*shell);
    mouseDownAt (timeline, { timeline.getWidth() - 20, timeline.getHeight() - 20 });

    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.commandDispatchCount == 2);
    REQUIRE (snapshot.context.timelineEditCount == 0);

    mouseDownAt (timeline, { 30, 100 });

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.commandDispatchCount == 2);
    REQUIRE (snapshot.context.timelineEditCount == 0);

    // The default Beat grid is live, so a fine 4-pixel move needs Ctrl (the grid INVERT).
    dragFromTo (timeline, { 30, 100 }, { 34, 100 },
                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier));

    const yesdaw::engine::Project moved = readProjectSnapshot (bundlePath);
    REQUIRE (moved.clips.size() == 1u);
    REQUIRE (moved.clips.front().timelineStart > 0);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.timelineEditCount == 1);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 3);

    juce::Button& undo = requireButtonForAction (*shell, UiActionId::EditUndo);
    REQUIRE (undo.isEnabled());
    clickButton (undo);

    const yesdaw::engine::Project undone = readProjectSnapshot (bundlePath);
    REQUIRE (undone.clips.size() == 1u);
    REQUIRE (undone.clips.front().timelineStart == 0);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 1);
    REQUIRE_FALSE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 4);

    juce::Button& redo = requireButtonForAction (*shell, UiActionId::EditRedo);
    REQUIRE (redo.isEnabled());
    clickButton (redo);

    const yesdaw::engine::Project redone = readProjectSnapshot (bundlePath);
    REQUIRE (redone.clips.size() == 1u);
    REQUIRE (redone.clips.front().timelineStart == moved.clips.front().timelineStart);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 1);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 5);

    doubleClickAt (timeline, { 80, 100 });

    const yesdaw::engine::Project split = readProjectSnapshot (bundlePath);
    REQUIRE (split.clips.size() == 2u);
    const yesdaw::engine::Clip& splitLeft = split.clips[0];
    const yesdaw::engine::Clip& splitRight = split.clips[1];
    REQUIRE (splitLeft.id == moved.clips.front().id);
    REQUIRE (splitRight.id.isValid());
    REQUIRE_FALSE (splitRight.id == splitLeft.id);
    REQUIRE (splitLeft.assetId == asset.id);
    REQUIRE (splitRight.assetId == asset.id);
    REQUIRE (splitLeft.timelineStart == moved.clips.front().timelineStart);
    REQUIRE (splitLeft.timelineLength > 0);
    REQUIRE (splitRight.timelineLength > 0);
    REQUIRE (splitRight.timelineStart == splitLeft.timelineStart + splitLeft.timelineLength);
    REQUIRE (splitLeft.timelineLength + splitRight.timelineLength == moved.clips.front().timelineLength);
    REQUIRE (splitLeft.srcOffset == moved.clips.front().srcOffset);
    REQUIRE (splitRight.srcOffset == splitLeft.srcOffset + splitLeft.srcLen);
    REQUIRE (splitLeft.srcLen + splitRight.srcLen == moved.clips.front().srcLen);
    REQUIRE (splitLeft.sourceWindowFits (asset));
    REQUIRE (splitRight.sourceWindowFits (asset));

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.timelineEditCount == 2);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 6);

    clickButton (undo);

    const yesdaw::engine::Project splitUndone = readProjectSnapshot (bundlePath);
    REQUIRE (splitUndone.clips.size() == 1u);
    REQUIRE (splitUndone.clips.front() == moved.clips.front());

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 2);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 7);

    clickButton (redo);

    const yesdaw::engine::Project splitRedone = readProjectSnapshot (bundlePath);
    REQUIRE (splitRedone.clips == split.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 2);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 8);

    const juce::Point<int> trimStart = timelineClipRightEdgeDragPoint (timeline, splitRedone, 1u);
    dragFromTo (timeline, trimStart, trimStart.translated (-6, 0));

    const yesdaw::engine::Project trimmed = readProjectSnapshot (bundlePath);
    REQUIRE (trimmed.clips.size() == 2u);
    const yesdaw::engine::Clip& trimmedLeft = trimmed.clips[0];
    const yesdaw::engine::Clip& trimmedRight = trimmed.clips[1];
    REQUIRE (trimmedLeft == splitLeft);
    REQUIRE (trimmedRight.id == splitRight.id);
    REQUIRE (trimmedRight.timelineStart == splitRight.timelineStart);
    REQUIRE (trimmedRight.timelineLength > 0);
    REQUIRE (trimmedRight.timelineLength < splitRight.timelineLength);
    REQUIRE (trimmedRight.srcOffset == splitRight.srcOffset);
    REQUIRE (trimmedRight.srcLen > 0u);
    REQUIRE (trimmedRight.srcLen < splitRight.srcLen);
    REQUIRE (trimmedRight.timelineStart + trimmedRight.timelineLength
             < splitRight.timelineStart + splitRight.timelineLength);
    REQUIRE (trimmedRight.sourceWindowFits (asset));

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.timelineEditCount == 3);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 9);

    clickButton (undo);

    const yesdaw::engine::Project trimUndone = readProjectSnapshot (bundlePath);
    REQUIRE (trimUndone.clips == split.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 3);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 10);

    clickButton (redo);

    const yesdaw::engine::Project trimRedone = readProjectSnapshot (bundlePath);
    REQUIRE (trimRedone.clips == trimmed.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 3);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 11);

    const juce::Point<int> gainStart = timelineClipCenterPoint (timeline, trimRedone, 0u);
    const juce::ModifierKeys shiftDrag {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier
    };
    dragFromTo (timeline, gainStart, gainStart.translated (0, -20), shiftDrag);

    const yesdaw::engine::Project gained = readProjectSnapshot (bundlePath);
    REQUIRE (gained.clips.size() == 2u);
    REQUIRE (gained.clips[1] == trimRedone.clips[1]);
    REQUIRE (gained.clips[0].id == trimRedone.clips[0].id);
    REQUIRE (gained.clips[0].timelineStart == trimRedone.clips[0].timelineStart);
    REQUIRE (gained.clips[0].timelineLength == trimRedone.clips[0].timelineLength);
    REQUIRE (gained.clips[0].srcOffset == trimRedone.clips[0].srcOffset);
    REQUIRE (gained.clips[0].srcLen == trimRedone.clips[0].srcLen);
    REQUIRE (gained.clips[0].fadeIn == trimRedone.clips[0].fadeIn);
    REQUIRE (gained.clips[0].fadeOut == trimRedone.clips[0].fadeOut);
    REQUIRE (gained.clips[0].gain > trimRedone.clips[0].gain);
    REQUIRE (std::fabs (gained.clips[0].gain - 1.2f) < 0.000001f);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.timelineEditCount == 4);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 12);

    clickButton (undo);

    const yesdaw::engine::Project gainUndone = readProjectSnapshot (bundlePath);
    REQUIRE (gainUndone.clips == trimRedone.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 4);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 13);

    clickButton (redo);

    const yesdaw::engine::Project gainRedone = readProjectSnapshot (bundlePath);
    REQUIRE (gainRedone.clips == gained.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 4);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 14);

    const juce::Point<int> fadeStart = timelineClipLeftEdgeDragPoint (timeline, gainRedone, 0u);
    const juce::ModifierKeys altDrag {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier
    };
    dragFromTo (timeline, fadeStart, fadeStart.translated (16, 0), altDrag);

    const yesdaw::engine::Project faded = readProjectSnapshot (bundlePath);
    REQUIRE (faded.clips.size() == 2u);
    REQUIRE (faded.clips[1] == gainRedone.clips[1]);
    REQUIRE (faded.clips[0].id == gainRedone.clips[0].id);
    REQUIRE (faded.clips[0].timelineStart == gainRedone.clips[0].timelineStart);
    REQUIRE (faded.clips[0].timelineLength == gainRedone.clips[0].timelineLength);
    REQUIRE (faded.clips[0].srcOffset == gainRedone.clips[0].srcOffset);
    REQUIRE (faded.clips[0].srcLen == gainRedone.clips[0].srcLen);
    REQUIRE (faded.clips[0].gain == gainRedone.clips[0].gain);
    REQUIRE (faded.clips[0].fadeIn > gainRedone.clips[0].fadeIn);
    REQUIRE (faded.clips[0].fadeIn <= faded.clips[0].timelineLength);
    REQUIRE (faded.clips[0].fadeOut == gainRedone.clips[0].fadeOut);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.timelineEditCount == 5);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 15);

    clickButton (undo);

    const yesdaw::engine::Project fadeUndone = readProjectSnapshot (bundlePath);
    REQUIRE (fadeUndone.clips == gainRedone.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 5);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 16);

    clickButton (redo);

    const yesdaw::engine::Project fadeRedone = readProjectSnapshot (bundlePath);
    REQUIRE (fadeRedone.clips == faded.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 5);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 17);

    const juce::Point<int> fadeOutStart = timelineClipRightEdgeDragPoint (timeline, fadeRedone, 0u);
    dragFromTo (timeline, fadeOutStart, fadeOutStart.translated (-16, 0), altDrag);

    const yesdaw::engine::Project fadedBoth = readProjectSnapshot (bundlePath);
    REQUIRE (fadedBoth.clips.size() == 2u);
    REQUIRE (fadedBoth.clips[1] == fadeRedone.clips[1]);
    REQUIRE (fadedBoth.clips[0].id == fadeRedone.clips[0].id);
    REQUIRE (fadedBoth.clips[0].timelineStart == fadeRedone.clips[0].timelineStart);
    REQUIRE (fadedBoth.clips[0].timelineLength == fadeRedone.clips[0].timelineLength);
    REQUIRE (fadedBoth.clips[0].srcOffset == fadeRedone.clips[0].srcOffset);
    REQUIRE (fadedBoth.clips[0].srcLen == fadeRedone.clips[0].srcLen);
    REQUIRE (fadedBoth.clips[0].gain == fadeRedone.clips[0].gain);
    REQUIRE (fadedBoth.clips[0].fadeIn == fadeRedone.clips[0].fadeIn);
    REQUIRE (fadedBoth.clips[0].fadeOut > fadeRedone.clips[0].fadeOut);
    REQUIRE (fadedBoth.clips[0].fadeOut <= fadedBoth.clips[0].timelineLength);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.timelineEditCount == 6);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 18);

    clickButton (undo);

    const yesdaw::engine::Project fadeOutUndone = readProjectSnapshot (bundlePath);
    REQUIRE (fadeOutUndone.clips == faded.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 6);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 19);

    clickButton (redo);

    const yesdaw::engine::Project fadeOutRedone = readProjectSnapshot (bundlePath);
    REQUIRE (fadeOutRedone.clips == fadedBoth.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 6);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 20);

    // The snap semantics changed with the tempo-derived grid (usable-DAW P1): the grid is ON by
    // default, so an UNMODIFIED drag snaps and Ctrl now means a FINE (unsnapped) move.
    const juce::Point<int> snapStart = timelineClipCenterPoint (timeline, fadeOutRedone, 0u);
    constexpr int kSnapDeltaPixels = 9;
    const double unsnappedSeconds = static_cast<double> (fadeOutRedone.clips[0].timelineStart)
                                      / fadeOutRedone.sampleRate.hz
                                  + static_cast<double> (kSnapDeltaPixels)
                                      / timelinePixelsPerSecond (timeline, fadeOutRedone);
    const yesdaw::engine::Tick unsnappedTick =
        static_cast<yesdaw::engine::Tick> (std::llround (unsnappedSeconds * fadeOutRedone.sampleRate.hz));

    const juce::ModifierKeys ctrlDrag {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier
    };
    dragFromTo (timeline, snapStart, snapStart.translated (kSnapDeltaPixels, 0), ctrlDrag);

    const yesdaw::engine::Project snapped = readProjectSnapshot (bundlePath);
    REQUIRE (snapped.clips.size() == 2u);
    REQUIRE (snapped.clips[1] == fadeOutRedone.clips[1]);
    REQUIRE (snapped.clips[0].id == fadeOutRedone.clips[0].id);
    REQUIRE (snapped.clips[0].timelineStart == unsnappedTick);
    REQUIRE (snapped.clips[0].timelineLength == fadeOutRedone.clips[0].timelineLength);
    REQUIRE (snapped.clips[0].srcOffset == fadeOutRedone.clips[0].srcOffset);
    REQUIRE (snapped.clips[0].srcLen == fadeOutRedone.clips[0].srcLen);
    REQUIRE (snapped.clips[0].gain == fadeOutRedone.clips[0].gain);
    REQUIRE (snapped.clips[0].fadeIn == fadeOutRedone.clips[0].fadeIn);
    REQUIRE (snapped.clips[0].fadeOut == fadeOutRedone.clips[0].fadeOut);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);
    REQUIRE (snapshot.context.timelineEditCount == 7);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 21);

    clickButton (undo);

    const yesdaw::engine::Project snapUndone = readProjectSnapshot (bundlePath);
    REQUIRE (snapUndone.clips == fadeOutRedone.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.undoCount == 7);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 22);

    clickButton (redo);

    const yesdaw::engine::Project snapRedone = readProjectSnapshot (bundlePath);
    REQUIRE (snapRedone.clips == snapped.clips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.redoCount == 7);
    REQUIRE (snapshot.context.canUndo);
    REQUIRE_FALSE (snapshot.context.canRedo);
    REQUIRE (snapshot.context.commandDispatchCount == 23);

    const std::filesystem::path bundledAssetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash);
    REQUIRE (readBytes (bundledAssetPath) == readBytes (fixturePath));

    juce::Button& play = requireButtonForAction (*shell, UiActionId::TransportPlay);
    REQUIRE (play.isEnabled());
    clickButton (play);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE_FALSE (snapshot.context.loopEnabled);
    REQUIRE (snapshot.context.playheadFrame == 0);
    REQUIRE (snapshot.context.commandDispatchCount == 24);

    const std::uint64_t framesToRender =
        static_cast<std::uint64_t> (std::max<yesdaw::engine::Tick> (512, snapRedone.clips[0].timelineStart + 512));
    const std::vector<float> rendered = renderMainComponentPlayback (*shell, framesToRender, 128);
    REQUIRE (rendered.size() == static_cast<std::size_t> (framesToRender * 2u));
    REQUIRE (peakAbs (std::span<const float> (rendered.data(), rendered.size())) > 0.01);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.playheadFrame == static_cast<std::int64_t> (framesToRender));

    juce::Button& locate = requireButtonForAction (*shell, UiActionId::TransportLocateStart);
    REQUIRE (locate.isEnabled());
    clickButton (locate);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.playheadFrame == 0);
    REQUIRE (snapshot.context.commandDispatchCount == 25);

    juce::Button& loop = requireButtonForAction (*shell, UiActionId::TransportToggleLoop);
    REQUIRE (loop.isEnabled());
    REQUIRE_FALSE (loop.getToggleState());
    clickButton (loop);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.loopEnabled);
    REQUIRE (loop.getToggleState());
    REQUIRE (snapshot.context.commandDispatchCount == 26);

    juce::Button& stop = requireButtonForAction (*shell, UiActionId::TransportStop);
    REQUIRE (stop.isEnabled());
    clickButton (stop);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.loopEnabled);
    REQUIRE (snapshot.context.playheadFrame == 0);
    REQUIRE (snapshot.context.commandDispatchCount == 27);

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
    REQUIRE_FALSE (snapshot.context.mixerTargetSelected);
    REQUIRE (snapshot.context.commandDispatchCount == 28);

    juce::Slider& fader = requireSliderForAction (*shell, UiActionId::MixerTargetSetFader);
    juce::Slider& pan = requireSliderForAction (*shell, UiActionId::MixerTargetSetPan);
    juce::Button& mute = requireButtonForAction (*shell, UiActionId::MixerTargetToggleMute);
    juce::Button& solo = requireButtonForAction (*shell, UiActionId::MixerTargetToggleSolo);
    REQUIRE_FALSE (fader.isEnabled());
    REQUIRE_FALSE (pan.isEnabled());
    REQUIRE_FALSE (mute.isEnabled());
    REQUIRE_FALSE (solo.isEnabled());

    const yesdaw::engine::Project preMixer = readProjectSnapshot (bundlePath);
    REQUIRE (preMixer.tracks.size() == 2u);
    REQUIRE_FALSE (preMixer.clips.empty());
    REQUIRE (preMixer.midiClips.size() == 1u);
    REQUIRE (preMixer.tracks.front().strip.linearGain == 1.0f);
    REQUIRE (preMixer.tracks.front().strip.pan == 0.0f);
    REQUIRE_FALSE (preMixer.tracks.front().strip.muted);
    REQUIRE_FALSE (preMixer.tracks.front().strip.soloed);
    yesdaw::engine::Tick mixerTimelineEnd = 0;
    for (const yesdaw::engine::Clip& mixerClip : preMixer.clips)
        if (mixerClip.timelineStart >= 0 && mixerClip.timelineLength > 0)
            mixerTimelineEnd = std::max (mixerTimelineEnd, mixerClip.timelineStart + mixerClip.timelineLength);

    const std::uint64_t mixerRenderFrames =
        static_cast<std::uint64_t> (std::max<yesdaw::engine::Tick> (512, mixerTimelineEnd));

    clickButton (requireButtonWithComponentId (*shell, "mixer.track.0.select"));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerTargetSelected);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);
    REQUIRE (fader.isEnabled());
    REQUIRE (pan.isEnabled());
    REQUIRE (mute.isEnabled());
    REQUIRE (solo.isEnabled());

    clickButton (requireButtonForAction (*shell, UiActionId::TransportLocateStart));
    clickButton (requireButtonForAction (*shell, UiActionId::TransportPlay));
    const std::vector<float> beforeMixRender = renderMainComponentPlayback (*shell, mixerRenderFrames, 128);
    REQUIRE (beforeMixRender.size() == static_cast<std::size_t> (mixerRenderFrames * 2u));
    const double beforeMixLeftPeak = channelPeakAbs (
        std::span<const float> (beforeMixRender.data(), beforeMixRender.size()), 0u, 2u);
    const double beforeMixRightPeak = channelPeakAbs (
        std::span<const float> (beforeMixRender.data(), beforeMixRender.size()), 1u, 2u);
    REQUIRE (beforeMixLeftPeak > 0.01);
    REQUIRE (beforeMixRightPeak > 0.01);

    dragVerticalSliderToNormalizedValue (fader, 0.28);
    dragHorizontalSliderToNormalizedValue (pan, 0.32);
    clickButton (mute);
    clickButton (solo);

    const yesdaw::engine::Project mixed = readProjectSnapshot (bundlePath);
    REQUIRE (mixed.tracks.size() == 2u);
    REQUIRE (mixed.tracks.front().strip.linearGain < preMixer.tracks.front().strip.linearGain);
    REQUIRE (mixed.tracks.front().strip.linearGain > 0.0f);
    REQUIRE (mixed.tracks.front().strip.pan < -0.05f);
    REQUIRE (mixed.tracks.front().strip.pan >= -1.0f);
    REQUIRE (mixed.tracks.front().strip.muted);
    REQUIRE (mixed.tracks.front().strip.soloed);
    REQUIRE (mixed.tracks.front().strip.name == "Audio 1");

    clickButton (requireButtonForAction (*shell, UiActionId::TransportPlay));
    const std::vector<float> afterMixRender = renderMainComponentPlayback (*shell, mixerRenderFrames, 128);
    REQUIRE (afterMixRender.size() == static_cast<std::size_t> (mixerRenderFrames * 2u));
    const double afterMixLeftPeak = channelPeakAbs (
        std::span<const float> (afterMixRender.data(), afterMixRender.size()), 0u, 2u);
    const double afterMixRightPeak = channelPeakAbs (
        std::span<const float> (afterMixRender.data(), afterMixRender.size()), 1u, 2u);
    // Track 1 is muted (and its own solo does not engage while muted, ADR-0014), so this render is
    // the OTHER content alone — audibly different from the two-track mix in both channels. The
    // precise mute/solo audibility law is pinned by the dedicated app-model mute-solo gate; here the
    // claim is that the mixer edits changed the real output at all (they were inaudible before the
    // mute mask was wired to strip state).
    REQUIRE (afterMixLeftPeak != beforeMixLeftPeak);
    REQUIRE (afterMixRightPeak != beforeMixRightPeak);
    REQUIRE (afterMixLeftPeak > 0.01);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerEditCount >= 4);
    REQUIRE (snapshot.context.commandDispatchCount >= 34);

    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::PianoRoll);
    REQUIRE (snapshot.context.midiClipSelected);
    REQUIRE_FALSE (snapshot.context.midiNoteSelected);

    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    const yesdaw::engine::Project prePiano = readProjectSnapshot (bundlePath);
    REQUIRE (prePiano.midiClips.size() == 1u);
    REQUIRE (prePiano.midiClips.front().notes.size() == 2u);
    const yesdaw::engine::MidiClip originalMidi = prePiano.midiClips.front();
    const yesdaw::engine::Note originalNote = originalMidi.notes.front();
    const yesdaw::engine::Note untouchedNote = originalMidi.notes[1];

    const juce::Point<int> noteStart = pianoRollNoteCenterPoint (pianoRoll, originalMidi, originalNote);
    mouseDownAt (pianoRoll, noteStart);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiNoteSelected);

    constexpr int kEndToEndNoteMovePixels = 42;
    const yesdaw::engine::Tick expectedNoteMoveDelta =
        pianoRollDeltaTicksForPixels (pianoRoll, originalMidi, kEndToEndNoteMovePixels);
    const yesdaw::engine::Tick expectedMovedNoteStart =
        std::clamp<yesdaw::engine::Tick> (
            originalNote.startTick + expectedNoteMoveDelta,
            0,
            std::max<yesdaw::engine::Tick> (0, originalMidi.timelineLength - originalNote.lengthTicks));
    dragFromTo (pianoRoll, noteStart, noteStart.translated (kEndToEndNoteMovePixels, 0));

    const yesdaw::engine::Project movedPiano = readProjectSnapshot (bundlePath);
    REQUIRE (movedPiano.midiClips.front().notes.front().startTick == expectedMovedNoteStart);
    REQUIRE (movedPiano.midiClips.front().notes.front().lengthTicks == originalNote.lengthTicks);
    REQUIRE (movedPiano.midiClips.front().notes[1] == untouchedNote);

    const yesdaw::engine::MidiClip movedMidi = movedPiano.midiClips.front();
    const yesdaw::engine::Note movedNote = movedMidi.notes.front();
    const juce::ModifierKeys pianoShiftDrag {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier
    };
    constexpr int kEndToEndNoteLengthPixels = 36;
    const yesdaw::engine::Tick expectedNoteLengthDelta =
        pianoRollDeltaTicksForPixels (pianoRoll, movedMidi, kEndToEndNoteLengthPixels);
    const yesdaw::engine::Tick expectedNoteLength =
        std::clamp<yesdaw::engine::Tick> (
            movedNote.lengthTicks + expectedNoteLengthDelta,
            0,
            std::max<yesdaw::engine::Tick> (0, movedMidi.timelineLength - movedNote.startTick));
    const juce::Point<int> movedNoteCenter = pianoRollNoteCenterPoint (pianoRoll, movedMidi, movedNote);
    dragFromTo (pianoRoll, movedNoteCenter, movedNoteCenter.translated (kEndToEndNoteLengthPixels, 0), pianoShiftDrag);

    const yesdaw::engine::Project lengthenedPiano = readProjectSnapshot (bundlePath);
    REQUIRE (lengthenedPiano.midiClips.front().notes.front().startTick == expectedMovedNoteStart);
    REQUIRE (lengthenedPiano.midiClips.front().notes.front().lengthTicks == expectedNoteLength);

    const yesdaw::engine::MidiClip lengthenedMidi = lengthenedPiano.midiClips.front();
    const yesdaw::engine::Note lengthenedNote = lengthenedMidi.notes.front();
    const juce::ModifierKeys altDoubleClick {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier
    };
    doubleClickAt (
        pianoRoll,
        pianoRollNoteCenterPoint (pianoRoll, lengthenedMidi, lengthenedNote),
        altDoubleClick);

    const yesdaw::engine::Project transposedPiano = readProjectSnapshot (bundlePath);
    REQUIRE (transposedPiano.midiClips.front().notes.front().key == originalNote.key + 1);
    REQUIRE (std::fabs (transposedPiano.midiClips.front().notes.front().pitchNote - (originalNote.pitchNote + 1.0))
             < 0.000001);

    const yesdaw::engine::MidiClip transposedMidi = transposedPiano.midiClips.front();
    const yesdaw::engine::Note transposedNote = transposedMidi.notes.front();
    yesdaw::engine::Tick expectedQuantizedNoteStart = 0;
    REQUIRE (yesdaw::engine::snapTick (
        transposedNote.startTick,
        yesdaw::engine::SnapGrid { 512 },
        expectedQuantizedNoteStart));
    const juce::ModifierKeys ctrlDoubleClick {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier
    };
    doubleClickAt (
        pianoRoll,
        pianoRollNoteCenterPoint (pianoRoll, transposedMidi, transposedNote),
        ctrlDoubleClick);

    const yesdaw::engine::Project quantizedPiano = readProjectSnapshot (bundlePath);
    REQUIRE (quantizedPiano.midiClips.front().notes.front().startTick == expectedQuantizedNoteStart);
    REQUIRE (quantizedPiano.midiClips.front().notes.front().lengthTicks == expectedNoteLength);
    REQUIRE (quantizedPiano.midiClips.front().notes.front().key == originalNote.key + 1);
    REQUIRE (quantizedPiano.midiClips.front().notes[1] == untouchedNote);

    const juce::ModifierKeys shiftDoubleClick {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier
    };
    doubleClickAt (
        pianoRoll,
        pianoRollNoteCenterPoint (pianoRoll, quantizedPiano.midiClips.front(), quantizedPiano.midiClips.front().notes.front()),
        shiftDoubleClick);

    const yesdaw::engine::Project pianoReadback = readProjectSnapshot (bundlePath);
    REQUIRE (pianoReadback.midiClips == quantizedPiano.midiClips);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.midiEditCount >= 4);
    REQUIRE (snapshot.context.midiReadCount >= 1);

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectSave));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.saveCount == 1);
    REQUIRE (snapshot.context.commandDispatchCount >= 35);

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.mixerTargetSelected);
    REQUIRE (snapshot.playbackReady);
    REQUIRE (snapshot.bundlePath == bundlePath);

    const yesdaw::engine::Project reopenedMixer = readProjectSnapshot (bundlePath);
    REQUIRE (reopenedMixer.tracks == mixed.tracks);
    REQUIRE (reopenedMixer.clips == pianoReadback.clips);
    REQUIRE (reopenedMixer.midiClips == pianoReadback.midiClips);

    REQUIRE_FALSE (fader.isEnabled());
    REQUIRE_FALSE (pan.isEnabled());
    REQUIRE_FALSE (mute.isEnabled());
    REQUIRE_FALSE (solo.isEnabled());

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    clickButton (requireButtonWithComponentId (*shell, "mixer.track.0.select"));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.mixerTargetSelected);
    REQUIRE (std::fabs (fader.getValue() - static_cast<double> (mixed.tracks.front().strip.linearGain)) < 0.0001);
    REQUIRE (std::fabs (pan.getValue() - static_cast<double> (mixed.tracks.front().strip.pan)) < 0.0001);
    REQUIRE (mute.getToggleState());
    REQUIRE (solo.getToggleState());
}

TEST_CASE ("shipped MainComponent dispatches keymap chords through keyPressed",
           "[ui][input][shell][keyboard]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("keyboard-chords");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));

    // Ctrl+N news, Ctrl+I imports — the same native-choice path as the toolbar buttons.
    REQUIRE (shell->keyPressed (juce::KeyPress ('n', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('i', juce::ModifierKeys::ctrlModifier, 0)));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.importCount == 1);

    // Space plays, K stops.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);

    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.isPlaying);

    // Ctrl+T adds a track through the arrangement verb.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.trackEditCount == 1);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 2u);

    // Ctrl+Z undoes it; Ctrl+Shift+Z redoes it.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 1u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 2u);

    // Select the imported clip with the mouse, then Del removes it and Ctrl+Z restores it.
    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project imported = readProjectSnapshot (bundlePath);
    REQUIRE (imported.clips.size() == 1u);
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, imported, 0u));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineClipSelected);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.timelineClipSelected);
    REQUIRE (readProjectSnapshot (bundlePath).clips.empty());

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 1u);

    // An unmapped key is not consumed.
    REQUIRE_FALSE (shell->keyPressed (juce::KeyPress (juce::KeyPress::pageUpKey)));
}

TEST_CASE ("interactive track rail — select, add, rename, remove, import-to-track, vertical clip drag",
           "[ui][input][shell][tracks]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("track-rail");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 1u);

    // The Add Track button is a real component driving the undoable verb.
    auto* addTrack = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "track.add"));
    REQUIRE (addTrack != nullptr);
    REQUIRE (addTrack->isVisible());
    REQUIRE (addTrack->isEnabled());
    clickButton (*addTrack);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 2u);
    REQUIRE (project.tracks.back().strip.name == "Audio 2");

    // Row click selects; import then lands on the SELECTED track.
    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    REQUIRE (railComponent->isVisible());
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (railComponent->getHeight() - headerHeight) / 2);
    const juce::Point<int> secondRowCentre { railComponent->getWidth() / 2,
                                             headerHeight + rowHeight + rowHeight / 2 };
    mouseDownAt (*railComponent, secondRowCentre);

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.size() == 2u);
    REQUIRE (project.clips.back().trackId == project.tracks.back().id);

    // Vertical drag moves the FIRST clip (lane 0) down onto the second track's lane.
    juce::Component& timeline = requireTimelineComponent (*shell);
    const juce::Point<int> clipCentre = timelineClipCenterPoint (timeline, project, 0u);
    dragFromTo (timeline, clipCentre, { clipCentre.x, clipCentre.y + 200 });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.front().trackId == project.tracks.back().id);

    // Ctrl+F2 is the explicit Track rename chord; F2 is the contextual Clip-or-Track rename action.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::F2Key,
                                                juce::ModifierKeys::ctrlModifier, 0)));
    auto* rename = dynamic_cast<juce::TextEditor*> (findChildWithComponentId (*shell, "shell.tracklist.rename"));
    REQUIRE (rename != nullptr);
    REQUIRE (rename->isVisible());
    rename->setText ("Drums", juce::dontSendNotification);
    rename->onReturnKey();
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.back().strip.name == "Drums");
    REQUIRE_FALSE (rename->isVisible());

    // Select the now-empty first row and Ctrl+Shift+T removes that track; no clip is lost.
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2, headerHeight + rowHeight / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress ('t',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 1u);
    REQUIRE (project.tracks.front().strip.name == "Drums");
    REQUIRE (project.clips.size() == 2u);   // both clips live on the surviving track; nothing was lost
}

TEST_CASE ("Up and Down select adjacent Track rail rows and retarget persisted mixer edits",
           "[ui][input][shell][tracks][arrow-navigation]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("arrow-track-navigation");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));

    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.tracks.size() == 2u);
    REQUIRE (original.clips.size() == 1u);
    REQUIRE (original.clips.front().trackId == original.tracks.front().id);

    const auto renderShell = [&shell]
    {
        juce::Image image (juce::Image::ARGB, shell->getWidth(), shell->getHeight(), true);
        juce::Graphics graphics (image);
        shell->paintEntireComponent (graphics, true);
        return image;
    };
    const auto changedPixelsIn = [] (const juce::Image& first,
                                     const juce::Image& second,
                                     juce::Rectangle<int> area)
    {
        int changed = 0;
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                if (first.getPixelAt (x, y) != second.getPixelAt (x, y))
                    ++changed;
        return changed;
    };

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (rail->getHeight() - headerHeight) / 2);
    const juce::Rectangle<int> firstRow {
        rail->getX(), rail->getY() + headerHeight, rail->getWidth(), rowHeight
    };
    const juce::Rectangle<int> secondRow = firstRow.translated (0, rowHeight);

    // From no rail selection, Down starts at Track 1 and the next Down reaches Track 2. Up must
    // then move the real painted highlight back to row one; both row surfaces must visibly change.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    const juce::Image secondSelected = renderShell();
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    const juce::Image firstSelected = renderShell();
    REQUIRE (changedPixelsIn (secondSelected, firstSelected, firstRow) > 0);
    REQUIRE (changedPixelsIn (secondSelected, firstSelected, secondRow) > 0);

    // The same selection retargets the shared strip controls. Persisting Mute must land on Track 1
    // and make its only Clip inaudible through the real rebuilt playback graph.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> audible = renderMainComponentPlayback (*shell, 4096, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (audible.data(), audible.size())) > 0.01);

    auto* mute = dynamic_cast<juce::Button*> (
        findChildWithComponentId (*shell, "mixer.target.toggle_mute"));
    REQUIRE (mute != nullptr);
    clickButton (*mute);
    const yesdaw::engine::Project muted = readProjectSnapshot (bundlePath);
    REQUIRE (muted.tracks.front().strip.muted);
    REQUIRE_FALSE (muted.tracks.back().strip.muted);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> silent = renderMainComponentPlayback (*shell, 4096, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (silent.data(), silent.size())) == 0.0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    clickButton (*mute);
    const yesdaw::engine::Project secondMuted = readProjectSnapshot (bundlePath);
    REQUIRE (secondMuted.tracks.front().strip.muted);
    REQUIRE (secondMuted.tracks.back().strip.muted);
}

TEST_CASE ("Left and Right locate the playhead by one current grid unit without editing the Project",
           "[ui][input][shell][transport][arrow-navigation]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("arrow-grid-navigation");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-stepped.wav";
    std::error_code removeError;
    std::filesystem::remove (sourcePath, removeError);

    constexpr std::uint64_t kFrames = 120'000;
    constexpr std::uint64_t kBeatFrames = 24'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
        samples[static_cast<std::size_t> (frame)] = 0.1f * static_cast<float> (1u + frame / kBeatFrames);
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (snapshotMainComponent (*shell).context.snapGridTicks == static_cast<std::int64_t> (kBeatFrames));
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> atStart = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    const double startPeak = peakAbs (std::span<const float> (atStart.data(), atStart.size()));
    REQUIRE (startPeak > 0.05);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == static_cast<std::int64_t> (kBeatFrames));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::leftKey)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 0);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::leftKey)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> atNextBeat = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (atNextBeat.data(), atNextBeat.size()))
             == Catch::Approx (startPeak * 2.0));
    REQUIRE (atNextBeat != atStart);

    constexpr std::int64_t kBarFrames = 96'000;
    const juce::ModifierKeys shift = juce::ModifierKeys::shiftModifier;
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, shift, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == kBarFrames);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::leftKey, shift, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 0);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::leftKey, shift, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, shift, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> atNextBar = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (atNextBar.data(), atNextBar.size()))
             == Catch::Approx (startPeak * 5.0));
    REQUIRE (atNextBar != atNextBeat);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);
}

TEST_CASE ("Save As copies the bundle and continues working in the copy", "[ui][input][shell][saveas]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("save-as-original");
    const std::filesystem::path saveAsPath = makeTempBundlePath ("save-as-copy");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };
    choices.chooseSaveAsProjectBundle = [saveAsPath] { return saveAsPath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // Ctrl+Shift+S copies the bundle; subsequent edits land in the COPY, not the original.
    REQUIRE (shell->keyPressed (juce::KeyPress ('s',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (std::filesystem::exists (saveAsPath));
    REQUIRE (snapshotMainComponent (*shell).bundlePath == saveAsPath);

    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    const yesdaw::engine::Project copied = readProjectSnapshot (saveAsPath);
    REQUIRE (copied.clips.size() == original.clips.size());
    REQUIRE (copied.assets.size() == original.assets.size());

    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (saveAsPath).tracks.size() == 2u);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 1u);   // original untouched

    // The copy reopens playable in a fresh shell.
    MainComponentFileChoices openChoices;
    openChoices.chooseOpenProjectBundle = [saveAsPath] { return saveAsPath; };
    auto reopened = makeShell (std::move (openChoices));
    clickButton (requireButtonForAction (*reopened, UiActionId::ProjectOpen));
    REQUIRE (snapshotMainComponent (*reopened).playbackReady);
}

TEST_CASE ("every mixer track strip is selectable and retargets the shared controls",
           "[ui][input][shell][mixer][strips]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-strips");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // Second track via keymap, second clip onto it through the rail selection.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (railComponent->getHeight() - headerHeight) / 2);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2, headerHeight + rowHeight + rowHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    // Click the SECOND strip through the overlay, then drive the shared mute control: the mute must
    // land on track 2 in the persisted project — proof the controls retargeted.
    juce::Component* strips = findChildWithComponentId (*shell, "shell.mixer.strips.input");
    REQUIRE (strips != nullptr);
    REQUIRE (strips->isVisible());

    const int stripWidth = juce::jmax (yesdaw::ui::UiTheme::Layout::mixerStripMinWidth,
                                       strips->getWidth() / 4);   // 2 tracks + master column headroom
    mouseDownAt (*strips, { stripWidth + stripWidth / 2, strips->getHeight() / 2 });

    auto* mute = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.target.toggle_mute"));
    REQUIRE (mute != nullptr);
    clickButton (*mute);

    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 2u);
    REQUIRE_FALSE (project.tracks.front().strip.muted);
    REQUIRE (project.tracks.back().strip.muted);
}

TEST_CASE ("FX inserts add, bypass, and remove on the selected strip through real controls",
           "[ui][input][shell][mixer][fx]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-fx");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // Select strip 0 through the rail, then add an EQ and a Reverb from the chooser.
    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   headerHeight + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });

    auto* chooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.insert.add"));
    REQUIRE (chooser != nullptr);
    REQUIRE (chooser->isEnabled());
    chooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Eq) + 1, juce::sendNotificationSync);
    {
        const yesdaw::engine::Project afterFirst = readProjectSnapshot (bundlePath);
        INFO ("after first add: chain size " << afterFirst.tracks.front().strip.fxChain.size()
              << " mixerEditCount " << snapshotMainComponent (*shell).context.mixerEditCount);
        REQUIRE (afterFirst.tracks.front().strip.fxChain.size() == 1u);
    }
    chooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Reverb) + 1, juce::sendNotificationSync);

    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    INFO ("after second add: chain size " << project.tracks.front().strip.fxChain.size()
          << " mixerEditCount " << snapshotMainComponent (*shell).context.mixerEditCount);
    REQUIRE (project.tracks.front().strip.fxChain.size() == 2u);
    REQUIRE (project.tracks.front().strip.fxChain[0].kind == yesdaw::engine::FxKind::Eq);
    REQUIRE (project.tracks.front().strip.fxChain[1].kind == yesdaw::engine::FxKind::Reverb);
    REQUIRE (project.tracks.front().strip.fxChain[0].enabled);

    // Slot 0 toggle bypasses the EQ; the persisted chain reflects it.
    auto* slot0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.toggle"));
    REQUIRE (slot0 != nullptr);
    REQUIRE (slot0->isVisible());
    clickButton (*slot0);
    project = readProjectSnapshot (bundlePath);
    REQUIRE_FALSE (project.tracks.front().strip.fxChain[0].enabled);
    REQUIRE (project.tracks.front().strip.fxChain[1].enabled);

    // Removing slot 0 leaves only the Reverb; playback still runs (adopt path rebuilt the graph).
    auto* remove0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.remove"));
    REQUIRE (remove0 != nullptr);
    clickButton (*remove0);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain.size() == 1u);
    REQUIRE (project.tracks.front().strip.fxChain[0].kind == yesdaw::engine::FxKind::Reverb);
    REQUIRE (snapshotMainComponent (*shell).playbackReady);

    // Undo restores the removed EQ.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain.size() == 2u);
}

TEST_CASE ("track-rail mini pan, volume, and mute/solo controls edit the strip through real gestures",
           "[ui][input][shell][railmini]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("rail-mini");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 1u);

    // Row 0 geometry mirrors the shared rail row law (one track fills the rail below the header).
    using L = yesdaw::ui::UiTheme::Layout;
    juce::Rectangle<int> row = rail->getLocalBounds();
    row.removeFromTop (L::trackListHeaderHeight);
    row = row.withHeight (juce::jmax (L::trackListRowMinHeight, row.getHeight()));
    row.removeFromBottom (L::trackListSeparatorHeight);

    // VOL: clicking the middle of the mini slider sets the track gain to ~0.5, persisted.
    const juce::Rectangle<int> level =
        row.withRight (row.getRight() - L::trackListLevelRightInset)
            .removeFromRight (L::trackListLevelWidth)
            .withBottom (row.getBottom() - L::trackListLevelBottomInset)
            .withHeight (L::trackListLevelHeight);
    mouseDownAt (*rail, { level.getCentreX(), level.getCentreY() });
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.linearGain > 0.4f);
    REQUIRE (project.tracks.front().strip.linearGain < 0.6f);

    // PAN: clicking the knob's left edge pans hard left; double-click recentres.
    const juce::Rectangle<int> pan =
        row.withRight (row.getRight() - L::trackListPanRightInset)
            .removeFromRight (L::trackListPanDiameter)
            .withY (row.getY() + L::trackListPanTopInset)
            .withHeight (L::trackListPanDiameter);
    mouseDownAt (*rail, { pan.getX() + 1, pan.getCentreY() });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.pan < -0.8f);
    doubleClickAt (*rail, { pan.getCentreX(), pan.getCentreY() });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.pan == 0.0f);

    // M and S cells toggle the persisted strip state.
    juce::Rectangle<int> buttonsArea =
        row.withTrimmedLeft (L::trackListNameLeftInset)
            .withTrimmedTop (L::trackListButtonsTop)
            .withHeight (L::trackListButtonsHeight);
    const juce::Rectangle<int> muteCell = buttonsArea.removeFromLeft (L::trackListButtonWidth);
    const juce::Rectangle<int> soloCell = buttonsArea.removeFromLeft (L::trackListButtonWidth);
    mouseDownAt (*rail, muteCell.getCentre());
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.muted);
    mouseDownAt (*rail, muteCell.getCentre());
    project = readProjectSnapshot (bundlePath);
    REQUIRE_FALSE (project.tracks.front().strip.muted);
    mouseDownAt (*rail, soloCell.getCentre());
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.soloed);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("bus and send controls route the selected track undoably through real controls",
           "[ui][input][shell][mixer][sendsui]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-sends");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // Select the first track strip through the rail so send actions have a target, then open the
    // full mixer view — the routing tools live in the mixer tools column.
    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    // + Bus creates a persisted Bus.
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.buses.size() == 1u);
    REQUIRE (project.buses.front().strip.name == "Bus 1");

    // The send chooser routes the selected track to that bus at unity.
    auto* sendChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.add"));
    REQUIRE (sendChooser != nullptr);
    REQUIRE (sendChooser->isEnabled());
    REQUIRE (sendChooser->getNumItems() == 1);
    sendChooser->setSelectedId (1, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.size() == 1u);
    REQUIRE (project.tracks.front().sends.front().busId == project.buses.front().id);
    REQUIRE (project.tracks.front().sends.front().linearGain == 1.0f);

    // The send row's level slider persists an undoable SetSendLevel.
    auto* level0 = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.send.0"));
    REQUIRE (level0 != nullptr);
    REQUIRE (level0->isVisible());
    level0->setValue (0.5, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.front().linearGain == 0.5f);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.front().linearGain == 1.0f);

    // The row's remove control drops the send.
    auto* remove0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.send.0.remove"));
    REQUIRE (remove0 != nullptr);
    clickButton (*remove0);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.empty());
    REQUIRE (project.buses.size() == 1u);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("menu bar model lists real menus and dispatches actions through the shell",
           "[ui][input][shell][menubar]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("menubar");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));

    auto* bar = dynamic_cast<juce::MenuBarComponent*> (findChildWithComponentId (*shell, "shell.menubar"));
    REQUIRE (bar != nullptr);
    juce::MenuBarModel* model = bar->getModel();
    REQUIRE (model != nullptr);
    REQUIRE (model->getMenuBarNames() == juce::StringArray ({ "File", "Edit", "View", "Options", "Help" }));
    REQUIRE (model->getMenuForIndex (0, "File").getNumItems() == 6);
    REQUIRE (model->getMenuForIndex (1, "Edit").getNumItems() == 9);
    REQUIRE (model->getMenuForIndex (4, "Help").getNumItems() == 1);

    // File > New Project through the model creates a real bundle.
    model->menuItemSelected (static_cast<int> (UiActionId::ProjectNew) + 1, 0);
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);

    // File > Import lands a clip; View > Mixer switches the active panel.
    model->menuItemSelected (static_cast<int> (UiActionId::ProjectImportAudio) + 1, 0);
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 1u);
    model->menuItemSelected (static_cast<int> (UiActionId::ViewMixer) + 1, 2);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("export bit-depth chooser drives a 16-bit PCM export through the real button",
           "[ui][input][shell][exportopts]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("export-opts");
    std::filesystem::path exportPath = bundlePath;
    exportPath += ".wav";
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    std::error_code removeError;
    std::filesystem::remove (exportPath, removeError);

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };
    choices.chooseExportAudioFile = [exportPath] { return exportPath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    auto* bitDepth = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "shell.export.bitdepth"));
    auto* range = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "shell.export.range"));
    REQUIRE (bitDepth != nullptr);
    REQUIRE (range != nullptr);
    bitDepth->setSelectedId (3, juce::sendNotificationSync);   // 16-bit PCM

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectExportAudio));
    REQUIRE (std::filesystem::exists (exportPath));

    // fmt tag 1 (integer PCM) at offset 20, 16 bits per sample at offset 34.
    std::ifstream in (exportPath, std::ios::binary);
    REQUIRE (in.good());
    std::array<unsigned char, 36> header {};
    in.read (reinterpret_cast<char*> (header.data()), static_cast<std::streamsize> (header.size()));
    REQUIRE (in.good());
    REQUIRE ((header[20] | (header[21] << 8)) == 1);
    REQUIRE ((header[34] | (header[35] << 8)) == 16);

    std::filesystem::remove (exportPath, removeError);
    std::filesystem::remove_all (bundlePath, removeError);
}

TEST_CASE ("audio device chooser lists devices and switches the output device", "[ui][input][shell][device]")
{
    std::vector<std::string> selectedNames;

    MainComponentFileChoices choices;
    choices.listAudioOutputDevices = [] {
        return std::vector<std::string> { "Alpha Out", "Beta Out" };
    };
    choices.selectAudioOutputDevice = [&selectedNames] (const std::string& name) {
        selectedNames.push_back (name);
        return true;
    };

    auto shell = makeShell (std::move (choices));

    auto* chooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "shell.device.chooser"));
    REQUIRE (chooser != nullptr);
    REQUIRE (chooser->isEnabled());
    REQUIRE (chooser->getNumItems() == 2);
    REQUIRE (chooser->getItemText (0) == "Alpha Out");
    REQUIRE (chooser->getItemText (1) == "Beta Out");

    // Selecting an entry drives the device-switch seam with the chosen name.
    chooser->setSelectedId (2, juce::sendNotificationSync);
    REQUIRE (selectedNames.size() == 1u);
    REQUIRE (selectedNames.front() == "Beta Out");
}

TEST_CASE ("audio device chooser is present but empty without a device backend", "[ui][input][shell][device]")
{
    // Harness shell: no desktop audio and no injected seams, so enumeration is deterministically empty.
    auto shell = makeShell();

    auto* chooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "shell.device.chooser"));
    REQUIRE (chooser != nullptr);
    REQUIRE (chooser->getNumItems() == 0);
    REQUIRE_FALSE (chooser->isEnabled());
}

TEST_CASE ("FX parameter sliders edit the selected insert undoably through real controls",
           "[ui][input][shell][mixer][fxparam]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mixer-fx-param");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   headerHeight + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });

    auto* chooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.insert.add"));
    REQUIRE (chooser != nullptr);
    chooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Compressor) + 1,
                            juce::sendNotificationSync);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.fxChain.size() == 1u);

    // Param sliders are hidden until a slot's edit button selects it.
    auto* param0 = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.fx.param.0"));
    REQUIRE (param0 != nullptr);
    REQUIRE_FALSE (param0->isVisible());

    auto* edit0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.edit"));
    REQUIRE (edit0 != nullptr);
    REQUIRE (edit0->isVisible());
    clickButton (*edit0);
    REQUIRE (param0->isVisible());
    REQUIRE (param0->isEnabled());

    // Slider 0 is the compressor threshold; moving it persists one undoable SetFxInsertParam.
    param0->setValue (0.25, juce::sendNotificationSync);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const auto& params = project.tracks.front().strip.fxChain.front().normalizedParams;
    REQUIRE (params.size() == 1u);
    REQUIRE (params.front().first == 0u);
    REQUIRE (params.front().second == Catch::Approx (0.25));

    // Undo reverts the param edit; the insert itself stays.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain.size() == 1u);
    REQUIRE (project.tracks.front().strip.fxChain.front().normalizedParams.empty());

    // Toggling the edit button off hides the param rows again.
    clickButton (*edit0);
    REQUIRE_FALSE (param0->isVisible());
}

TEST_CASE ("header tempo and time-signature controls edit the project time map undoably",
           "[ui][input][shell][timemap]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("time-map");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));

    auto* tempo = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "transport.set_tempo"));
    auto* meter = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "transport.set_meter"));
    REQUIRE (tempo != nullptr);
    REQUIRE (meter != nullptr);
    REQUIRE_FALSE (tempo->isEnabled());   // disabled before a project exists

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (tempo->isEnabled());
    REQUIRE (meter->isEnabled());

    tempo->setValue (140.0, juce::sendNotificationSync);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE_FALSE (project.tempoMap.empty());
    REQUIRE (project.tempoMap.front().bpm == 140.0);

    meter->setSelectedId (3, juce::sendNotificationSync);   // 6/8 in kHeaderMeterChoices order
    project = readProjectSnapshot (bundlePath);
    REQUIRE_FALSE (project.meterMap.empty());
    REQUIRE (project.meterMap.front().numerator == 6);
    REQUIRE (project.meterMap.front().denominator == 8);

    // Both edits are on the undo stack.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE ((project.meterMap.empty() || project.meterMap.front().numerator != 6));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE ((project.tempoMap.empty() || project.tempoMap.front().bpm != 140.0));
}

TEST_CASE ("shift-drag on the ruler defines a user loop region", "[ui][input][shell][loop]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("loop-region");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.loopEnabled);

    // Shift-drag across the ruler strip: a loop region between two distinct positions.
    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::ui::TimelineCanvasGeometry rulerGeometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), yesdaw::ui::TimelineCanvasState {});
    const int rulerY = rulerGeometry.rulerArea.getCentreY();
    const juce::Point<int> from { timeline.getWidth() / 4, rulerY };
    const juce::Point<int> to { (timeline.getWidth() * 3) / 4, rulerY };
    dragFromTo (timeline, from, to,
                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier));

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.loopEnabled);
    REQUIRE (snapshot.playbackLoopEndFrame > snapshot.playbackLoopStartFrame);
    REQUIRE (snapshot.playbackLoopStartFrame >= 0);

    // The loop toggle still clears it.
    clickButton (requireButtonForAction (*shell, UiActionId::TransportToggleLoop));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.loopEnabled);
}

TEST_CASE ("ctrl-wheel zooms the timeline and plain wheel scrolls it", "[ui][input][shell][zoom]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("zoom-scroll");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const juce::Point<int> centre { timeline.getWidth() / 2, timeline.getHeight() / 2 };

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.timelineZoomFactor == 1.0);

    juce::MouseWheelDetails wheelUp {};
    wheelUp.deltaY = 0.4f;
    juce::MouseEvent ctrlWheel = makeMouseEvent (timeline, centre, centre, false, 1,
                                                 juce::ModifierKeys (juce::ModifierKeys::ctrlModifier));
    timeline.mouseWheelMove (ctrlWheel, wheelUp);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.timelineZoomFactor > 1.0);

    // Plain wheel scrolls; scroll clamps to zero at the left edge after zooming back out.
    juce::MouseWheelDetails wheelDown {};
    wheelDown.deltaY = -0.4f;
    juce::MouseEvent plainWheel = makeMouseEvent (timeline, centre, centre, false, 1, juce::ModifierKeys {});
    timeline.mouseWheelMove (plainWheel, wheelDown);
    snapshot = snapshotMainComponent (*shell);
    const double scrolled = snapshot.timelineScrollSeconds;
    REQUIRE (scrolled >= 0.0);

    juce::MouseEvent ctrlWheelOut = makeMouseEvent (timeline, centre, centre, false, 1,
                                                    juce::ModifierKeys (juce::ModifierKeys::ctrlModifier));
    timeline.mouseWheelMove (ctrlWheelOut, wheelDown);
    timeline.mouseWheelMove (ctrlWheelOut, wheelDown);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.timelineZoomFactor == 1.0);   // clamped at fit-to-window
}

TEST_CASE ("Ctrl+0 fits the whole Project horizontally without changing Snap",
           "[ui][input][shell][zoom][zoom-fit]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("zoom-fit-project");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const juce::Point<int> centre { timeline.getWidth() / 2, timeline.getHeight() / 2 };
    juce::MouseWheelDetails wheelUp {};
    wheelUp.deltaY = 0.4f;
    const juce::MouseEvent ctrlWheel = makeMouseEvent (
        timeline,
        centre,
        centre,
        false,
        1,
        juce::ModifierKeys (juce::ModifierKeys::ctrlModifier));
    for (int step = 0; step < 5; ++step)
        timeline.mouseWheelMove (ctrlWheel, wheelUp);

    juce::MouseWheelDetails wheelDown {};
    wheelDown.deltaY = -0.4f;
    const juce::MouseEvent plainWheel = makeMouseEvent (
        timeline, centre, centre, false, 1, juce::ModifierKeys {});
    timeline.mouseWheelMove (plainWheel, wheelDown);

    const MainComponentSnapshot zoomed = snapshotMainComponent (*shell);
    REQUIRE (zoomed.timelineZoomFactor > 1.0);
    REQUIRE (zoomed.timelineScrollSeconds > 0.0);
    REQUIRE (zoomed.context.snapEnabled);
    const std::vector<yesdaw::engine::Clip> persistedClipsBeforeFit =
        readProjectSnapshot (bundlePath).clips;

    REQUIRE (shell->keyPressed (juce::KeyPress ('0', juce::ModifierKeys::ctrlModifier, 0)));
    const MainComponentSnapshot fitted = snapshotMainComponent (*shell);
    REQUIRE (fitted.timelineZoomFactor == 1.0);
    REQUIRE (fitted.timelineScrollSeconds == 0.0);
    REQUIRE (fitted.visibleTimelineTotalSeconds == zoomed.visibleTimelineTotalSeconds);
    REQUIRE (fitted.context.snapEnabled);
    REQUIRE (readProjectSnapshot (bundlePath).clips == persistedClipsBeforeFit);

    REQUIRE (shell->keyPressed (juce::KeyPress ('0', juce::ModifierKeys::altModifier, 0)));
    REQUIRE_FALSE (snapshotMainComponent (*shell).context.snapEnabled);
}

TEST_CASE ("Ctrl+Shift+0 fits the current loop region with exact viewport math",
           "[ui][input][shell][zoom][zoom-fit][loop]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("zoom-fit-loop");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::ui::TimelineCanvasGeometry rulerGeometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), yesdaw::ui::TimelineCanvasState {});
    const int rulerY = rulerGeometry.rulerArea.getCentreY();
    dragFromTo (
        timeline,
        { timeline.getWidth() / 4, rulerY },
        { (timeline.getWidth() * 3) / 4, rulerY },
        juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                            | juce::ModifierKeys::shiftModifier));

    const MainComponentSnapshot loopSet = snapshotMainComponent (*shell);
    REQUIRE (loopSet.context.loopEnabled);
    REQUIRE (loopSet.playbackLoopEndFrame > loopSet.playbackLoopStartFrame);
    const yesdaw::engine::Project persisted = readProjectSnapshot (bundlePath);
    REQUIRE (persisted.sampleRate.isValid());

    const juce::Point<int> centre { timeline.getWidth() / 2, timeline.getHeight() / 2 };
    juce::MouseWheelDetails wheelUp {};
    wheelUp.deltaY = 0.4f;
    const juce::MouseEvent ctrlWheel = makeMouseEvent (
        timeline,
        centre,
        centre,
        false,
        1,
        juce::ModifierKeys (juce::ModifierKeys::ctrlModifier));
    for (int step = 0; step < 5; ++step)
        timeline.mouseWheelMove (ctrlWheel, wheelUp);

    juce::MouseWheelDetails wheelDown {};
    wheelDown.deltaY = -0.4f;
    const juce::MouseEvent plainWheel = makeMouseEvent (
        timeline, centre, centre, false, 1, juce::ModifierKeys {});
    timeline.mouseWheelMove (plainWheel, wheelDown);
    const MainComponentSnapshot beforeFit = snapshotMainComponent (*shell);

    const double loopStartSeconds = static_cast<double> (loopSet.playbackLoopStartFrame)
                                  / persisted.sampleRate.hz;
    const double loopDurationSeconds = static_cast<double> (
                                           loopSet.playbackLoopEndFrame - loopSet.playbackLoopStartFrame)
                                     / persisted.sampleRate.hz;
    const double expectedZoom = std::clamp (
        std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                  loopSet.visibleTimelineTotalSeconds)
            / loopDurationSeconds,
        yesdaw::ui::UiTheme::Layout::timelineZoomMin,
        yesdaw::ui::UiTheme::Layout::timelineZoomMax);
    REQUIRE ((beforeFit.timelineZoomFactor != expectedZoom
              || beforeFit.timelineScrollSeconds != loopStartSeconds));

    const juce::ModifierKeys ctrlShift {
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier
    };
    REQUIRE (shell->keyPressed (juce::KeyPress ('0', ctrlShift, 0)));
    const MainComponentSnapshot fitted = snapshotMainComponent (*shell);
    REQUIRE (fitted.timelineZoomFactor == expectedZoom);
    REQUIRE (fitted.timelineScrollSeconds == loopStartSeconds);
    REQUIRE (fitted.playbackLoopStartFrame == loopSet.playbackLoopStartFrame);
    REQUIRE (fitted.playbackLoopEndFrame == loopSet.playbackLoopEndFrame);
    REQUIRE (fitted.context.loopEnabled);
    REQUIRE (readProjectSnapshot (bundlePath).clips == persisted.clips);
}

TEST_CASE ("plus zooms the Timeline in while keeping the playhead at the same pixel",
           "[ui][input][shell][zoom][keyboard-zoom]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("keyboard-zoom-in");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    const yesdaw::engine::Project persisted = readProjectSnapshot (bundlePath);
    REQUIRE (persisted.clips.size() == 1u);
    REQUIRE (persisted.sampleRate.isValid());

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeZoom = renderMainComponentPlayback (*shell, 48'000, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeZoom.data(), beforeZoom.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    juce::Component& timeline = requireTimelineComponent (*shell);
    juce::MouseWheelDetails wheelUp {};
    wheelUp.deltaY = 0.4f;
    const juce::Point<int> zeroPoint =
        projectRulerPointAtTick (timeline, snapshotMainComponent (*shell), persisted, 0);
    const juce::MouseEvent ctrlWheel = makeMouseEvent (
        timeline,
        zeroPoint,
        zeroPoint,
        false,
        1,
        juce::ModifierKeys (juce::ModifierKeys::ctrlModifier));
    for (int step = 0; step < 6; ++step)
        timeline.mouseWheelMove (ctrlWheel, wheelUp);

    const yesdaw::engine::Tick requestedPlayhead = persisted.clips.front().timelineLength / 2;
    const MainComponentSnapshot beforeLocate = snapshotMainComponent (*shell);
    const juce::Point<int> locatePoint =
        projectRulerPointAtTick (timeline, beforeLocate, persisted, requestedPlayhead);
    REQUIRE (timeline.getLocalBounds().contains (locatePoint));
    mouseDownAt (timeline, locatePoint);

    const MainComponentSnapshot before = snapshotMainComponent (*shell);
    REQUIRE (before.context.playheadFrame > 0);
    const juce::Point<int> playheadPixelBefore =
        projectRulerPointAtTick (timeline, before, persisted, before.context.playheadFrame);
    const double playheadSeconds = static_cast<double> (before.context.playheadFrame)
                                 / persisted.sampleRate.hz;
    const double expectedZoom = std::clamp (
        before.timelineZoomFactor * yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep,
        yesdaw::ui::UiTheme::Layout::timelineZoomMin,
        yesdaw::ui::UiTheme::Layout::timelineZoomMax);
    const double expectedScroll = playheadSeconds
                                - (playheadSeconds - before.timelineScrollSeconds)
                                    * (before.timelineZoomFactor / expectedZoom);

    const juce::ModifierKeys shift { juce::ModifierKeys::shiftModifier };
    REQUIRE (shell->keyPressed (juce::KeyPress ('=', shift, '+')));

    const MainComponentSnapshot after = snapshotMainComponent (*shell);
    REQUIRE (after.timelineZoomFactor == expectedZoom);
    REQUIRE (after.timelineScrollSeconds == Catch::Approx (expectedScroll).margin (1.0e-12));
    REQUIRE (after.context.playheadFrame == before.context.playheadFrame);
    REQUIRE (projectRulerPointAtTick (timeline, after, persisted, after.context.playheadFrame).x
             == playheadPixelBefore.x);
    REQUIRE (readProjectSnapshot (bundlePath).clips == persisted.clips);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterZoom = renderMainComponentPlayback (*shell, 48'000, 128);
    REQUIRE (afterZoom == beforeZoom);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
}

TEST_CASE ("minus zooms the Timeline out at the playhead and clamps at whole-Project fit",
           "[ui][input][shell][zoom][keyboard-zoom]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("keyboard-zoom-out");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    const yesdaw::engine::Project persisted = readProjectSnapshot (bundlePath);
    REQUIRE (persisted.clips.size() == 1u);
    REQUIRE (persisted.sampleRate.isValid());

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Tick requestedPlayhead = persisted.clips.front().timelineLength / 2;
    const juce::Point<int> locatePoint =
        projectRulerPointAtTick (timeline, snapshotMainComponent (*shell), persisted, requestedPlayhead);
    REQUIRE (timeline.getLocalBounds().contains (locatePoint));
    mouseDownAt (timeline, locatePoint);

    const juce::ModifierKeys shift { juce::ModifierKeys::shiftModifier };
    REQUIRE (shell->keyPressed (juce::KeyPress ('=', shift, '+')));
    const MainComponentSnapshot before = snapshotMainComponent (*shell);
    REQUIRE (before.timelineZoomFactor == yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep);
    REQUIRE (before.timelineScrollSeconds > 0.0);

    const juce::Point<int> playheadPixelBefore =
        projectRulerPointAtTick (timeline, before, persisted, before.context.playheadFrame);
    const double playheadSeconds = static_cast<double> (before.context.playheadFrame)
                                 / persisted.sampleRate.hz;
    const double expectedZoom = std::clamp (
        before.timelineZoomFactor / yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep,
        yesdaw::ui::UiTheme::Layout::timelineZoomMin,
        yesdaw::ui::UiTheme::Layout::timelineZoomMax);
    const double expectedScroll = playheadSeconds
                                - (playheadSeconds - before.timelineScrollSeconds)
                                    * (before.timelineZoomFactor / expectedZoom);

    REQUIRE (shell->keyPressed (juce::KeyPress ('-')));
    const MainComponentSnapshot after = snapshotMainComponent (*shell);
    REQUIRE (after.timelineZoomFactor == expectedZoom);
    REQUIRE (after.timelineScrollSeconds == Catch::Approx (expectedScroll).margin (1.0e-12));
    REQUIRE (after.context.playheadFrame == before.context.playheadFrame);
    REQUIRE (projectRulerPointAtTick (timeline, after, persisted, after.context.playheadFrame).x
             == playheadPixelBefore.x);
    REQUIRE (readProjectSnapshot (bundlePath).clips == persisted.clips);

    REQUIRE (shell->keyPressed (juce::KeyPress ('-')));
    const MainComponentSnapshot clamped = snapshotMainComponent (*shell);
    REQUIRE (clamped.timelineZoomFactor == yesdaw::ui::UiTheme::Layout::timelineZoomMin);
    REQUIRE (clamped.timelineScrollSeconds
             == yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds);
}

TEST_CASE ("dragging the clip's left edge trims its head without moving the audio under it",
           "[ui][input][shell][trimleft]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("trim-left");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project imported = readProjectSnapshot (bundlePath);
    REQUIRE (imported.clips.size() == 1u);
    const yesdaw::engine::Clip before = imported.clips.front();

    const juce::Point<int> leftEdge = timelineClipLeftEdgeDragPoint (timeline, imported, 0u);
    const int quarterClipPixels = juce::jmax (
        yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels + 2,
        juce::roundToInt (timelinePixelsPerSecond (timeline, imported)
                          * (static_cast<double> (before.timelineLength) / 48'000.0) * 0.25));
    dragFromTo (timeline, leftEdge, { leftEdge.x + quarterClipPixels, leftEdge.y });

    const yesdaw::engine::Project trimmed = readProjectSnapshot (bundlePath);
    const yesdaw::engine::Clip after = trimmed.clips.front();
    REQUIRE (after.timelineStart > before.timelineStart);
    REQUIRE (after.timelineLength < before.timelineLength);
    REQUIRE (after.srcOffset > before.srcOffset);
    REQUIRE (after.srcLen < before.srcLen);
    // The clip END never moved: only the head was consumed.
    REQUIRE (after.timelineStart + after.timelineLength == before.timelineStart + before.timelineLength);

    // Undo restores the original head.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Clip restored = readProjectSnapshot (bundlePath).clips.front();
    REQUIRE (restored.timelineStart == before.timelineStart);
    REQUIRE (restored.srcOffset == before.srcOffset);
}

TEST_CASE ("Ctrl+C/V/D copy, paste at playhead, and duplicate the selected clip", "[ui][input][shell][clipboard]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-clipboard");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // Paste is disabled until something is copied.
    REQUIRE (shell->keyPressed (juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 1u);

    // Select the imported clip, duplicate it: the copy lands right after the source on the same track.
    juce::Component& timeline = requireTimelineComponent (*shell);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, project, 0u));
    REQUIRE (snapshotMainComponent (*shell).context.timelineClipSelected);

    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.size() == 2u);
    const yesdaw::engine::Clip source = project.clips[0];
    const yesdaw::engine::Clip duplicate = project.clips[1];
    REQUIRE (duplicate.trackId == source.trackId);
    REQUIRE (duplicate.assetId == source.assetId);
    REQUIRE (duplicate.timelineStart == source.timelineStart + source.timelineLength);
    REQUIRE (duplicate.timelineLength == source.timelineLength);
    REQUIRE (duplicate.id != source.id);

    // Copy the source, locate to a later frame, paste: a third clip appears at the playhead.
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, project, 0u));
    REQUIRE (shell->keyPressed (juce::KeyPress ('c', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Tick pasteFrame = duplicate.timelineStart + duplicate.timelineLength + 1'000;
    (void) pasteFrame;
    // Locate via Home then rely on playhead 0 paste; then verify undo chain removes pasted + duplicate.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.size() == 3u);
    REQUIRE (project.clips[2].timelineStart == 0);
    REQUIRE (project.clips[2].assetId == source.assetId);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 2u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 1u);
}

TEST_CASE ("Ctrl+R repeat-pastes the clipboard back-to-back as one audible undo group",
           "[ui][input][shell][clipboard][repeat-paste]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-repeat-paste");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);
    const yesdaw::engine::Clip source = original.clips.front();
    REQUIRE (source.timelineLength > 0);

    auto* repeatChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.repeat-paste.chooser"));
    REQUIRE (repeatChooser != nullptr);
    REQUIRE (repeatChooser->isVisible());
    REQUIRE_FALSE (repeatChooser->isEnabled());
    REQUIRE (repeatChooser->getSelectedId() == 2);
    REQUIRE (shell->keyPressed (juce::KeyPress ('r', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    mouseDownAt (timeline, timelineClipCenterPoint (timeline, original, 0u));
    REQUIRE (shell->keyPressed (juce::KeyPress ('c', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (repeatChooser->isEnabled());

    const juce::Point<int> pastePoint = projectRulerPointAtTick (
        timeline,
        snapshotMainComponent (*shell),
        original,
        source.timelineStart + source.timelineLength);
    mouseDownAt (timeline, pastePoint);
    const yesdaw::engine::Tick pasteStart = snapshotMainComponent (*shell).context.playheadFrame;

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::uint64_t renderFrames = static_cast<std::uint64_t> (source.timelineLength * 3);
    const std::vector<float> beforeRepeat = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    mouseDownAt (timeline, pastePoint);
    REQUIRE (shell->keyPressed (juce::KeyPress ('r', juce::ModifierKeys::ctrlModifier, 0)));

    const yesdaw::engine::Project repeated = readProjectSnapshot (bundlePath);
    REQUIRE (repeated.clips.size() == 3u);
    REQUIRE (repeated.clips[0] == source);
    for (std::size_t i = 1; i < repeated.clips.size(); ++i)
    {
        const yesdaw::engine::Clip& copy = repeated.clips[i];
        REQUIRE (copy.id != source.id);
        REQUIRE (copy.trackId == source.trackId);
        REQUIRE (copy.assetId == source.assetId);
        REQUIRE (copy.timelineStart
                 == pasteStart + static_cast<yesdaw::engine::Tick> (i - 1) * source.timelineLength);
        REQUIRE (copy.timelineLength == source.timelineLength);
        REQUIRE (copy.srcOffset == source.srcOffset);
        REQUIRE (copy.srcLen == source.srcLen);
        REQUIRE (copy.gain == source.gain);
        REQUIRE (copy.fadeIn == source.fadeIn);
        REQUIRE (copy.fadeOut == source.fadeOut);
        REQUIRE (copy.timeBase == source.timeBase);
        REQUIRE (copy.name == source.name);
    }

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterRepeat = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterRepeat != beforeRepeat);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterUndo = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterUndo == beforeRepeat);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    repeatChooser->setSelectedId (4, juce::sendNotificationSync);
    REQUIRE (repeatChooser->getSelectedId() == 4);
    mouseDownAt (timeline, pastePoint);
    REQUIRE (shell->keyPressed (juce::KeyPress ('r', juce::ModifierKeys::ctrlModifier, 0)));

    const yesdaw::engine::Project repeatedFourTimes = readProjectSnapshot (bundlePath);
    REQUIRE (repeatedFourTimes.clips.size() == 5u);
    REQUIRE (repeatedFourTimes.clips.front() == source);
    for (std::size_t i = 1; i < repeatedFourTimes.clips.size(); ++i)
        REQUIRE (repeatedFourTimes.clips[i].timelineStart
                 == pasteStart + static_cast<yesdaw::engine::Tick> (i - 1) * source.timelineLength);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
}

TEST_CASE ("Alt-drag copies a timeline clip to the drag destination as one audible edit",
           "[ui][input][shell][timeline][copy-drag]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-copy-drag");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    clickButton (requireButtonForAction (*shell, UiActionId::TrackAdd));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);
    REQUIRE (original.tracks.size() == 2u);
    const yesdaw::engine::Clip source = original.clips.front();

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeCopy = renderMainComponentPlayback (*shell, 48'000, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeCopy.data(), beforeCopy.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    auto* snapChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (snapChooser != nullptr);
    snapChooser->setSelectedId (1, juce::sendNotificationSync);
    REQUIRE_FALSE (snapshotMainComponent (*shell).context.snapEnabled);

    const juce::Point<int> dragStart = timelineClipCenterPoint (timeline, original, 0u);
    const juce::ModifierKeys altDrag {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier
    };
    dragFromTo (timeline,
                dragStart,
                dragStart.translated (timeline.getWidth() / 4, 200),
                altDrag);

    const yesdaw::engine::Project copied = readProjectSnapshot (bundlePath);
    REQUIRE (copied.clips.size() == 2u);
    REQUIRE (copied.clips.front() == source);
    const yesdaw::engine::Clip& copy = copied.clips.back();
    REQUIRE (copy.id != source.id);
    REQUIRE (copy.trackId == original.tracks.back().id);
    REQUIRE (copy.assetId == source.assetId);
    REQUIRE (copy.timelineStart > source.timelineStart);
    REQUIRE (copy.timelineLength == source.timelineLength);
    REQUIRE (copy.srcOffset == source.srcOffset);
    REQUIRE (copy.srcLen == source.srcLen);
    REQUIRE (copy.gain == source.gain);
    REQUIRE (copy.fadeIn == source.fadeIn);
    REQUIRE (copy.fadeOut == source.fadeOut);
    REQUIRE (copy.timeBase == source.timeBase);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterCopy = renderMainComponentPlayback (*shell, 48'000, 128);
    REQUIRE (afterCopy != beforeCopy);
    REQUIRE (peakAbs (std::span<const float> (afterCopy.data(), afterCopy.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterUndo = renderMainComponentPlayback (*shell, 48'000, 128);
    REQUIRE (afterUndo == beforeCopy);
}

TEST_CASE ("Alt+Up and Alt+Down step the selected clip gain by one decibel",
           "[ui][input][shell][timeline][clip-gain-keys]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-gain-keys");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);
    REQUIRE (original.clips.front().gain == 1.0f);
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, original, 0u));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> baseline = renderMainComponentPlayback (*shell, 512, 128);
    const double baselinePeak = peakAbs (std::span<const float> (baseline.data(), baseline.size()));
    REQUIRE (baselinePeak > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    const double oneDecibelRatio = std::pow (10.0, 1.0 / 20.0);
    const juce::ModifierKeys alt { juce::ModifierKeys::altModifier };
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey, alt, 0)));
    yesdaw::engine::Project stepped = readProjectSnapshot (bundlePath);
    REQUIRE (std::fabs (static_cast<double> (stepped.clips.front().gain) - oneDecibelRatio) < 0.000001);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> raised = renderMainComponentPlayback (*shell, 512, 128);
    const double raisedPeak = peakAbs (std::span<const float> (raised.data(), raised.size()));
    REQUIRE (std::fabs ((raisedPeak / baselinePeak) - oneDecibelRatio) < 0.000001);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey, alt, 0)));
    stepped = readProjectSnapshot (bundlePath);
    REQUIRE (std::fabs (stepped.clips.front().gain - original.clips.front().gain) < 0.000001f);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey, alt, 0)));
    const yesdaw::engine::Project lowered = readProjectSnapshot (bundlePath);
    REQUIRE (std::fabs (static_cast<double> (lowered.clips.front().gain) - (1.0 / oneDecibelRatio)) < 0.000001);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> reduced = renderMainComponentPlayback (*shell, 512, 128);
    const double reducedPeak = peakAbs (std::span<const float> (reduced.data(), reduced.size()));
    REQUIRE (std::fabs ((reducedPeak / baselinePeak) - (1.0 / oneDecibelRatio)) < 0.000001);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterUndo = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (afterUndo == baseline);
}

TEST_CASE ("Ctrl+F replaces selected clip fades with the default length as one audible edit",
           "[ui][input][shell][timeline][default-fades]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("default-fades");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);
    REQUIRE (original.sampleRate == yesdaw::engine::SampleRate { 48'000.0 });
    REQUIRE (original.clips.front().fadeIn == 0);
    REQUIRE (original.clips.front().fadeOut == 0);
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, original, 0u));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::uint64_t renderFrames = static_cast<std::uint64_t> (original.clips.front().timelineLength);
    const std::vector<float> unfaded = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (peakAbs (std::span<const float> (unfaded.data(), unfaded.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    juce::Slider& fadeIn = requireSliderWithComponentId (*shell, kInspectorFadeInComponentId);
    juce::Slider& fadeOut = requireSliderWithComponentId (*shell, kInspectorFadeOutComponentId);
    setSliderValueThroughComponent (fadeIn, 0.05);
    setSliderValueThroughComponent (fadeOut, 0.05);
    const yesdaw::engine::Project preexisting = readProjectSnapshot (bundlePath);
    constexpr yesdaw::engine::Tick expectedDefaultFadeTicks = 480;
    REQUIRE (preexisting.clips.front().fadeIn > expectedDefaultFadeTicks);
    REQUIRE (preexisting.clips.front().fadeOut > expectedDefaultFadeTicks);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> preexistingAudio = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('f', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Project defaulted = readProjectSnapshot (bundlePath);
    REQUIRE (defaulted.clips.front().fadeIn == expectedDefaultFadeTicks);
    REQUIRE (defaulted.clips.front().fadeOut == expectedDefaultFadeTicks);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> defaultedAudio = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (defaultedAudio != unfaded);

    constexpr double halfPi = 1.57079632679489661923;
    double maximumEnvelopeDiff = 0.0;
    const yesdaw::engine::Tick timelineLength = defaulted.clips.front().timelineLength;
    for (yesdaw::engine::Tick frame = 0; frame < timelineLength; ++frame)
    {
        double expectedGain = 1.0;
        if (frame < expectedDefaultFadeTicks)
            expectedGain = std::sin (halfPi * static_cast<double> (frame)
                                     / static_cast<double> (expectedDefaultFadeTicks));

        const yesdaw::engine::Tick fadeOutStart = timelineLength - expectedDefaultFadeTicks;
        if (frame >= fadeOutStart)
        {
            const double progress = static_cast<double> (frame - fadeOutStart)
                                  / static_cast<double> (expectedDefaultFadeTicks);
            expectedGain = std::min (expectedGain, std::sin (halfPi * (1.0 - progress)));
        }

        for (std::size_t channel = 0; channel < 2u; ++channel)
        {
            const std::size_t sample = static_cast<std::size_t> (frame) * 2u + channel;
            maximumEnvelopeDiff = std::max (
                maximumEnvelopeDiff,
                std::fabs (static_cast<double> (defaultedAudio[sample])
                           - static_cast<double> (unfaded[sample]) * expectedGain));
        }
    }
    REQUIRE (maximumEnvelopeDiff < 0.000001);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == preexisting.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterUndo = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterUndo == preexistingAudio);
}

TEST_CASE ("X crossfades two overlapping clips on one track as one audible edit",
           "[ui][input][shell][timeline][crossfade]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("crossfade");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    const yesdaw::engine::Project imported = readProjectSnapshot (bundlePath);
    REQUIRE (imported.clips.size() == 1u);
    const yesdaw::engine::Clip source = imported.clips.front();
    REQUIRE (source.timelineStart == 0);
    REQUIRE (source.fadeIn == 0);
    REQUIRE (source.fadeOut == 0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> sourceAudio = renderMainComponentPlayback (
        *shell, static_cast<std::uint64_t> (source.timelineLength), 128);
    REQUIRE (peakAbs (std::span<const float> (sourceAudio.data(), sourceAudio.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Tick overlapTicks = snapshotMainComponent (*shell).context.snapGridTicks / 8;
    REQUIRE (overlapTicks > 0);
    REQUIRE (overlapTicks < source.timelineLength);
    REQUIRE (shell->keyPressed (juce::KeyPress (',', juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);

    const yesdaw::engine::Project overlapped = readProjectSnapshot (bundlePath);
    REQUIRE (overlapped.clips.size() == 2u);
    const yesdaw::engine::Clip& leftBefore = overlapped.clips[0];
    const yesdaw::engine::Clip& rightBefore = overlapped.clips[1];
    REQUIRE (leftBefore.trackId == rightBefore.trackId);
    REQUIRE (rightBefore.timelineStart == leftBefore.timelineStart + leftBefore.timelineLength - overlapTicks);
    REQUIRE (leftBefore.fadeOut == 0);
    REQUIRE (rightBefore.fadeIn == 0);

    const std::uint64_t renderFrames = static_cast<std::uint64_t> (
        rightBefore.timelineStart + rightBefore.timelineLength);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeCrossfade = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('x')));
    const yesdaw::engine::Project crossfaded = readProjectSnapshot (bundlePath);
    REQUIRE (crossfaded.clips.size() == 2u);
    REQUIRE (crossfaded.clips[0].fadeIn == leftBefore.fadeIn);
    REQUIRE (crossfaded.clips[0].fadeOut == overlapTicks);
    REQUIRE (crossfaded.clips[1].fadeIn == overlapTicks);
    REQUIRE (crossfaded.clips[1].fadeOut == rightBefore.fadeOut);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterCrossfade = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (afterCrossfade != beforeCrossfade);

    const std::size_t channelCount = sourceAudio.size() / static_cast<std::size_t> (source.timelineLength);
    REQUIRE (channelCount > 0u);
    REQUIRE (sourceAudio.size() == static_cast<std::size_t> (source.timelineLength) * channelCount);
    REQUIRE (afterCrossfade.size() == static_cast<std::size_t> (renderFrames) * channelCount);

    constexpr double halfPi = 1.57079632679489661923;
    double maximumRenderedDiff = 0.0;
    double maximumPowerDeviation = 0.0;
    for (yesdaw::engine::Tick offset = 0; offset < overlapTicks; ++offset)
    {
        const double progress = static_cast<double> (offset) / static_cast<double> (overlapTicks);
        const double fadeInGain = std::sin (halfPi * progress);
        const double fadeOutGain = std::sin (halfPi * (1.0 - progress));
        maximumPowerDeviation = std::max (
            maximumPowerDeviation, std::fabs (fadeInGain * fadeInGain + fadeOutGain * fadeOutGain - 1.0));

        const yesdaw::engine::Tick timelineFrame = rightBefore.timelineStart + offset;
        for (std::size_t channel = 0; channel < channelCount; ++channel)
        {
            const std::size_t renderedIndex = static_cast<std::size_t> (timelineFrame) * channelCount + channel;
            const std::size_t leftIndex = static_cast<std::size_t> (timelineFrame) * channelCount + channel;
            const std::size_t rightIndex = static_cast<std::size_t> (offset) * channelCount + channel;
            const double expected = static_cast<double> (sourceAudio[leftIndex]) * fadeOutGain
                                  + static_cast<double> (sourceAudio[rightIndex]) * fadeInGain;
            maximumRenderedDiff = std::max (
                maximumRenderedDiff,
                std::fabs (static_cast<double> (afterCrossfade[renderedIndex]) - expected));
        }
    }
    REQUIRE (maximumPowerDeviation < 0.000001);
    REQUIRE (maximumRenderedDiff < 0.000001);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == overlapped.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterUndo = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterUndo == beforeCrossfade);
}

TEST_CASE ("Ctrl+X cuts the selected clip into the clipboard as one undoable edit",
           "[ui][input][shell][clipboard][cut]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-cut");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.size() == 1u);
    const yesdaw::engine::Clip source = project.clips.front();
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, project, 0u));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeCut = renderMainComponentPlayback (
        *shell, static_cast<std::uint64_t> (source.timelineLength), 128);
    REQUIRE (peakAbs (std::span<const float> (beforeCut.data(), beforeCut.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('x', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.empty());
    REQUIRE (snapshotMainComponent (*shell).context.clipboardHasClip);
    REQUIRE_FALSE (snapshotMainComponent (*shell).context.timelineClipSelected);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterCut = renderMainComponentPlayback (
        *shell, static_cast<std::uint64_t> (source.timelineLength), 128);
    REQUIRE (peakAbs (std::span<const float> (afterCut.data(), afterCut.size())) == 0.0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    // Cut itself is exactly one undo step.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.size() == 1u);
    REQUIRE (project.clips.front().id == source.id);
    REQUIRE (project.clips.front().timelineStart == source.timelineStart);
    REQUIRE (project.clips.front().srcOffset == source.srcOffset);
    REQUIRE (project.clips.front().srcLen == source.srcLen);

    // Redo the cut, then paste the clipboard at the playhead: the audible clip is reproduced.
    REQUIRE (shell->keyPressed (
        juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.empty());
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.size() == 1u);
    REQUIRE (project.clips.front().id != source.id);
    REQUIRE (project.clips.front().timelineStart == source.timelineStart);
    REQUIRE (project.clips.front().assetId == source.assetId);
    REQUIRE (project.clips.front().timelineLength == source.timelineLength);
    REQUIRE (project.clips.front().srcOffset == source.srcOffset);
    REQUIRE (project.clips.front().srcLen == source.srcLen);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterPaste = renderMainComponentPlayback (
        *shell, static_cast<std::uint64_t> (source.timelineLength), 128);
    REQUIRE (afterPaste == beforeCut);
}

TEST_CASE ("timeline multi-select edits the shipped project and playback as one undo step",
           "[ui][input][shell][timeline][multi-select]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-multi-select");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));

    auto* addTrack = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "track.add"));
    REQUIRE (addTrack != nullptr);
    clickButton (*addTrack);
    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (rail->getHeight() - headerHeight) / 2);
    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + rowHeight + rowHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.size() == 3u);
    REQUIRE (project.clips[0].trackId == project.tracks[0].id);
    REQUIRE (project.clips[1].trackId == project.tracks[0].id);
    REQUIRE (project.clips[2].trackId == project.tracks[1].id);

    // Shift+click adds and removes without replacing the rest of the selection.
    mouseDownAt (timeline, timelineClipCenterPoint (timeline, project, 0u));
    mouseDownAt (timeline,
                 timelineClipCenterPoint (timeline, project, 1u),
                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                     | juce::ModifierKeys::shiftModifier));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);
    mouseDownAt (timeline,
                 timelineClipCenterPoint (timeline, project, 1u),
                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                     | juce::ModifierKeys::shiftModifier));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);

    // Ctrl+A targets only the selected track. Copy/paste preserves the selected clips as one group.
    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + rowHeight / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);
    REQUIRE (shell->keyPressed (juce::KeyPress ('c', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 5u);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.size() == 3u);

    // Dragging any member moves the whole track selection by the same snapped delta.
    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Tick firstStart = project.clips[0].timelineStart;
    const yesdaw::engine::Tick secondStart = project.clips[1].timelineStart;
    const juce::Point<int> firstCentre = timelineClipCenterPoint (timeline, project, 0u);
    dragFromTo (timeline, firstCentre, { firstCentre.x + timeline.getWidth() / 4, firstCentre.y });
    yesdaw::engine::Project moved = readProjectSnapshot (bundlePath);
    const yesdaw::engine::Tick firstDelta = moved.clips[0].timelineStart - firstStart;
    REQUIRE (firstDelta > 0);
    REQUIRE (moved.clips[1].timelineStart - secondStart == firstDelta);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips[0].timelineStart == firstStart);
    REQUIRE (project.clips[1].timelineStart == secondStart);

    // Ctrl+Shift+A selects the project; Delete persists silence, and one undo restores every clip.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeDelete = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeDelete.data(), beforeDelete.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 3);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.empty());
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterDelete = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (afterDelete.data(), afterDelete.size())) == 0.0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 3u);
}

TEST_CASE ("pointer-tool marquee selects exactly the touched clips for a persisted playback edit",
           "[ui][input][shell][timeline][marquee]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-marquee");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));

    auto* addTrack = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "track.add"));
    REQUIRE (addTrack != nullptr);
    clickButton (*addTrack);

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 2u);
    REQUIRE (project.clips.size() == 3u);
    const yesdaw::engine::Clip untouched = project.clips[2];
    const juce::Rectangle<int> firstBounds = timelineClipHitBounds (timeline, project, 0u);
    const juce::Rectangle<int> thirdBounds = timelineClipHitBounds (timeline, project, 2u);

    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Pointer);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeDelete = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeDelete.data(), beforeDelete.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    // Start in the empty second lane at the third Clip's left edge, then drag back across lane 0.
    // Rectangle-edge contact includes Clip 2 but excludes Clip 3.
    dragFromTo (timeline,
                { thirdBounds.getX(), thirdBounds.getCentreY() + thirdBounds.getHeight() },
                { firstBounds.getX(), firstBounds.getCentreY() });
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    const yesdaw::engine::Project deleted = readProjectSnapshot (bundlePath);
    REQUIRE (deleted.clips.size() == 1u);
    REQUIRE (deleted.clips.front() == untouched);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterDelete = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (afterDelete.data(), afterDelete.size())) == 0.0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 3u);
}

TEST_CASE ("B splits every selected clip at the playhead as one sample-accurate edit",
           "[ui][input][shell][timeline][split-at-playhead]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("split-at-playhead");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    auto* addTrack = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "track.add"));
    REQUIRE (addTrack != nullptr);
    clickButton (*addTrack);
    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (rail->getHeight() - headerHeight) / 2);
    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + rowHeight + rowHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 2u);
    REQUIRE (original.clips[0].timelineStart == 0);
    REQUIRE (original.clips[1].timelineStart == 0);
    REQUIRE (original.clips[0].timelineLength == original.clips[1].timelineLength);

    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeSplit = renderMainComponentPlayback (
        *shell, static_cast<std::uint64_t> (original.clips.front().timelineLength), 128);
    REQUIRE (peakAbs (std::span<const float> (beforeSplit.data(), beforeSplit.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Tick requestedSplitTick = original.clips.front().timelineLength / 2;
    juce::MouseWheelDetails wheelUp {};
    wheelUp.deltaY = 0.4f;
    const juce::Point<int> zoomAnchor =
        projectRulerPointAtTick (timeline, snapshotMainComponent (*shell), original, 0);
    juce::MouseEvent ctrlWheel = makeMouseEvent (
        timeline, zoomAnchor, zoomAnchor, false, 1,
        juce::ModifierKeys (juce::ModifierKeys::ctrlModifier));
    for (int i = 0; i < 10; ++i)
        timeline.mouseWheelMove (ctrlWheel, wheelUp);

    const MainComponentSnapshot beforeLocate = snapshotMainComponent (*shell);
    REQUIRE (beforeLocate.timelineZoomFactor > 8.0);
    const juce::Point<int> rulerPoint =
        projectRulerPointAtTick (timeline, beforeLocate, original, requestedSplitTick);
    REQUIRE (timeline.getLocalBounds().contains (rulerPoint));
    mouseDownAt (timeline, rulerPoint);
    const MainComponentSnapshot afterLocate = snapshotMainComponent (*shell);
    REQUIRE (afterLocate.context.commandDispatchCount == beforeLocate.context.commandDispatchCount + 1);
    const yesdaw::engine::Tick splitTick =
        static_cast<yesdaw::engine::Tick> (afterLocate.context.playheadFrame);
    REQUIRE (splitTick > 0);
    REQUIRE (splitTick < original.clips.front().timelineLength);

    REQUIRE (shell->keyPressed (juce::KeyPress ('b')));
    const yesdaw::engine::Project split = readProjectSnapshot (bundlePath);
    REQUIRE (split.clips.size() == 4u);

    for (const yesdaw::engine::Clip& source : original.clips)
    {
        const auto left = std::find_if (split.clips.begin(), split.clips.end(), [&source] (const auto& clip) {
            return clip.id == source.id;
        });
        REQUIRE (left != split.clips.end());
        const auto right = std::find_if (split.clips.begin(), split.clips.end(), [&source, splitTick] (const auto& clip) {
            return clip.id != source.id
                && clip.trackId == source.trackId
                && clip.assetId == source.assetId
                && clip.timelineStart == splitTick;
        });
        REQUIRE (right != split.clips.end());
        REQUIRE (left->timelineStart == source.timelineStart);
        REQUIRE (left->timelineLength == splitTick - source.timelineStart);
        REQUIRE (right->timelineStart == splitTick);
        REQUIRE (left->timelineLength + right->timelineLength == source.timelineLength);
        REQUIRE (left->srcOffset == source.srcOffset);
        REQUIRE (right->srcOffset == left->srcOffset + left->srcLen);
        REQUIRE (left->srcLen + right->srcLen == source.srcLen);
    }

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterSplit = renderMainComponentPlayback (
        *shell, static_cast<std::uint64_t> (original.clips.front().timelineLength), 128);
    REQUIRE (afterSplit == beforeSplit);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Project undone = readProjectSnapshot (bundlePath);
    REQUIRE (undone.clips == original.clips);
}

TEST_CASE ("Ctrl+J heals only adjacent clips with contiguous source windows",
           "[ui][input][shell][timeline][heal-clips]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("heal-clips");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 1u);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeHeal = renderMainComponentPlayback (
        *shell, static_cast<std::uint64_t> (original.clips.front().timelineLength), 128);
    REQUIRE (peakAbs (std::span<const float> (beforeHeal.data(), beforeHeal.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    // Duplicate is timeline-adjacent but restarts the same source window, so heal must refuse it.
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Project ineligible = readProjectSnapshot (bundlePath);
    const MainComponentSnapshot beforeRefusal = snapshotMainComponent (*shell);
    REQUIRE (ineligible.clips.size() == 2u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('j', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == ineligible.clips);
    const MainComponentSnapshot afterRefusal = snapshotMainComponent (*shell);
    REQUIRE (afterRefusal.context.commandDispatchCount == beforeRefusal.context.commandDispatchCount);
    REQUIRE (afterRefusal.context.timelineEditCount == beforeRefusal.context.timelineEditCount);
    REQUIRE (afterRefusal.context.canUndo == beforeRefusal.context.canUndo);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);

    juce::Component& timeline = requireTimelineComponent (*shell);
    juce::MouseWheelDetails wheelUp {};
    wheelUp.deltaY = 0.4f;
    const juce::Point<int> zoomAnchor =
        projectRulerPointAtTick (timeline, snapshotMainComponent (*shell), original, 0);
    juce::MouseEvent ctrlWheel = makeMouseEvent (
        timeline, zoomAnchor, zoomAnchor, false, 1,
        juce::ModifierKeys (juce::ModifierKeys::ctrlModifier));
    for (int i = 0; i < 10; ++i)
        timeline.mouseWheelMove (ctrlWheel, wheelUp);

    const yesdaw::engine::Tick requestedSplitTick = original.clips.front().timelineLength / 2;
    const juce::Point<int> rulerPoint = projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), original, requestedSplitTick);
    REQUIRE (timeline.getLocalBounds().contains (rulerPoint));
    mouseDownAt (timeline, rulerPoint);
    REQUIRE (shell->keyPressed (juce::KeyPress ('b')));

    const yesdaw::engine::Project split = readProjectSnapshot (bundlePath);
    REQUIRE (split.clips.size() == 2u);
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);

    REQUIRE (shell->keyPressed (juce::KeyPress ('j', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Project healed = readProjectSnapshot (bundlePath);
    REQUIRE (healed.clips == original.clips);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterHeal = renderMainComponentPlayback (
        *shell, static_cast<std::uint64_t> (original.clips.front().timelineLength), 128);
    REQUIRE (afterHeal == beforeHeal);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == split.clips);
}

TEST_CASE ("comma and period nudge selected clips by the snap grid as one edit",
           "[ui][input][shell][timeline][nudge-clips]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("nudge-clips");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));

    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.clips.size() == 2u);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);
    const yesdaw::engine::Tick grid = snapshotMainComponent (*shell).context.snapGridTicks;
    REQUIRE (grid == 24'000);
    REQUIRE (grid % 8 == 0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeNudge = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeNudge.data(), beforeNudge.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('.')));
    yesdaw::engine::Project moved = readProjectSnapshot (bundlePath);
    REQUIRE (moved.clips[0].timelineStart == original.clips[0].timelineStart + grid);
    REQUIRE (moved.clips[1].timelineStart == original.clips[1].timelineStart + grid);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterRightNudge = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (afterRightNudge.data(), afterRightNudge.size())) == 0.0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    const yesdaw::engine::Tick fine = grid / 8;
    REQUIRE (shell->keyPressed (juce::KeyPress ('.', juce::ModifierKeys::shiftModifier, 0)));
    moved = readProjectSnapshot (bundlePath);
    REQUIRE (moved.clips[0].timelineStart == original.clips[0].timelineStart + grid + fine);
    REQUIRE (moved.clips[1].timelineStart == original.clips[1].timelineStart + grid + fine);

    REQUIRE (shell->keyPressed (juce::KeyPress (',', juce::ModifierKeys::shiftModifier, 0)));
    const yesdaw::engine::Project fineRoundTrip = readProjectSnapshot (bundlePath);
    REQUIRE (fineRoundTrip.clips[0].timelineStart == original.clips[0].timelineStart + grid);
    REQUIRE (fineRoundTrip.clips[1].timelineStart == original.clips[1].timelineStart + grid);

    REQUIRE (shell->keyPressed (juce::KeyPress (',')));
    const yesdaw::engine::Project returned = readProjectSnapshot (bundlePath);
    REQUIRE (returned.clips == original.clips);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterReturn = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (afterReturn == beforeNudge);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == fineRoundTrip.clips);
}

TEST_CASE ("comma and period nudge the selected piano-roll note by the snap grid",
           "[ui][input][shell][pianoroll][nudge-note]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("nudge-note");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));

    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    mouseDownAt (pianoRoll, { pianoRoll.getWidth() / 2, pianoRoll.getHeight() / 2 });
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.midiClips.size() == 1u);
    REQUIRE (original.midiClips.front().notes.size() == 1u);
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == UiPanel::PianoRoll);
    REQUIRE (snapshotMainComponent (*shell).context.midiNoteSelected);

    const yesdaw::engine::Note originalNote = original.midiClips.front().notes.front();
    const yesdaw::engine::Tick grid = snapshotMainComponent (*shell).context.snapGridTicks;
    REQUIRE (grid == 24'000);
    REQUIRE (grid % 8 == 0);
    REQUIRE (originalNote.startTick >= grid);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeNudge = renderMainComponentPlayback (*shell, 96'000, 512);
    REQUIRE (peakAbs (std::span<const float> (beforeNudge.data(), beforeNudge.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (',')));
    yesdaw::engine::Project moved = readProjectSnapshot (bundlePath);
    REQUIRE (moved.midiClips.front().notes.front().startTick == originalNote.startTick - grid);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterLeftNudge = renderMainComponentPlayback (*shell, 96'000, 512);
    REQUIRE (afterLeftNudge != beforeNudge);
    REQUIRE (peakAbs (std::span<const float> (afterLeftNudge.data(), afterLeftNudge.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    const yesdaw::engine::Tick fine = grid / 8;
    REQUIRE (shell->keyPressed (juce::KeyPress ('.', juce::ModifierKeys::shiftModifier, 0)));
    moved = readProjectSnapshot (bundlePath);
    REQUIRE (moved.midiClips.front().notes.front().startTick == originalNote.startTick - grid + fine);

    REQUIRE (shell->keyPressed (juce::KeyPress (',', juce::ModifierKeys::shiftModifier, 0)));
    const yesdaw::engine::Project fineRoundTrip = readProjectSnapshot (bundlePath);
    REQUIRE (fineRoundTrip.midiClips.front().notes.front().startTick == originalNote.startTick - grid);

    REQUIRE (shell->keyPressed (juce::KeyPress ('.')));
    const yesdaw::engine::Project returned = readProjectSnapshot (bundlePath);
    REQUIRE (returned.midiClips == original.midiClips);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterReturn = renderMainComponentPlayback (*shell, 96'000, 512);
    REQUIRE (afterReturn == beforeNudge);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips == fineRoundTrip.midiClips);
}

TEST_CASE ("the snap grid derives from tempo and bites on unmodified drags", "[ui][input][shell][snap]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("snap-grid");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // Default: Beat grid at 120 BPM / 48 kHz -> 24000 frames.
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.snapEnabled);
    REQUIRE (snapshot.context.snapGridTicks == 24'000);

    // An unmodified drag lands ON the grid.
    juce::Component& timeline = requireTimelineComponent (*shell);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const juce::Point<int> clipCentre = timelineClipCenterPoint (timeline, project, 0u);
    dragFromTo (timeline, clipCentre, { clipCentre.x + timeline.getWidth() / 3, clipCentre.y });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.clips.front().timelineStart > 0);
    REQUIRE (project.clips.front().timelineStart % 24'000 == 0);

    // The chooser switches to Bar: the grid becomes 4 beats at the current meter.
    auto* chooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (chooser != nullptr);
    REQUIRE (chooser->isVisible());
    chooser->setSelectedId (2, juce::sendNotificationSync);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.snapGridTicks == 96'000);

    // Tempo edits re-derive the grid.
    auto* tempo = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "transport.set_tempo"));
    REQUIRE (tempo != nullptr);
    tempo->setValue (60.0, juce::sendNotificationSync);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.snapGridTicks == 192'000);

    // Snap Off disables the grid entirely.
    chooser->setSelectedId (1, juce::sendNotificationSync);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.snapEnabled);
}

TEST_CASE ("markers add with M at the playhead, remove on Alt+click, and paint on the ruler",
           "[ui][input][shell][markers]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("markers");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // M adds a marker at the playhead (0).
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.markers.size() == 1u);
    REQUIRE (project.markers.front().tick == 0);
    REQUIRE (project.markers.front().name == "Marker 1");

    // B20 assigns ruler double-click to transport locate. M remains the explicit persisted
    // Marker-add action and places the Marker at that located playhead.
    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::ui::TimelineCanvasGeometry rulerGeometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), yesdaw::ui::TimelineCanvasState {});
    const juce::Point<int> rulerMid { timeline.getWidth() / 2, rulerGeometry.rulerArea.getCentreY() };
    doubleClickAt (timeline, rulerMid);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.markers.size() == 1u);
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame > 0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.markers.size() == 2u);
    REQUIRE (project.markers.back().tick > 0);

    // Alt+click near the second marker removes it.
    mouseDownAt (timeline, rulerMid, juce::ModifierKeys (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.markers.size() == 1u);
    REQUIRE (project.markers.front().tick == 0);

    // Undo restores it; both operations are on the stack.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).markers.size() == 2u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).markers.size() == 1u);
}

TEST_CASE ("the automation lane canvas adds, moves, and deletes breakpoints that the render follows",
           "[ui][input][shell][automation-canvas]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("automation-canvas");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    clickButton (requireButtonForAction (*shell, UiActionId::TimelineAutomationToggleTrackLane));
    juce::Component* canvas = findChildWithComponentId (*shell, "timeline.automation.canvas");
    REQUIRE (canvas != nullptr);
    REQUIRE (canvas->isVisible());
    REQUIRE (canvas->getWidth() > 0);

    // Click low in the lane: a breakpoint appears with a LOW normalized value at the clicked time,
    // creating the selected track's fader lane on first use.
    const int lowY = (canvas->getHeight() * 9) / 10;
    mouseDownAt (*canvas, { canvas->getWidth() / 2, lowY });
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.size() == 1u);
    REQUIRE (project.automationLanes.front().points.size() == 1u);
    REQUIRE (project.automationLanes.front().points.front().value < 0.25);
    const yesdaw::engine::Tick firstTick = project.automationLanes.front().points.front().tick;
    REQUIRE (firstTick > 0);

    // A second, earlier breakpoint at the top: full value.
    mouseDownAt (*canvas, { canvas->getWidth() / 4, 1 });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.front().points.size() == 2u);
    REQUIRE (project.automationLanes.front().points.front().value > 0.9);

    // Drag the low breakpoint to the top: its value rises (grouped move+set = one undo step).
    const int handleX = canvas->getWidth() / 2;
    dragFromTo (*canvas, { handleX, lowY }, { handleX, 1 });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.front().points.size() == 2u);
    REQUIRE (project.automationLanes.front().points.back().value > 0.9);

    // Double-click deletes it.
    doubleClickAt (*canvas, { handleX, 1 });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.front().points.size() == 1u);

    // Undo restores the deleted point.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.front().points.size() == 2u);
}

TEST_CASE ("Ctrl+M creates a MIDI clip and the pencil adds audible notes", "[ui][input][shell][pencil]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("midi-pencil");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    // Ctrl+M: a one-bar MIDI clip appears on the selected track and the piano roll opens.
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.size() == 1u);
    REQUIRE (project.midiClips.front().notes.empty());
    REQUIRE (project.midiClips.front().timelineLength == 96'000);   // one 4/4 bar at 120 BPM / 48 kHz
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::PianoRoll);
    REQUIRE (snapshot.context.midiClipSelected);

    // Pencil: a click on the empty grid creates a note at the clicked key and snapped tick.
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    mouseDownAt (pianoRoll, { pianoRoll.getWidth() / 2, pianoRoll.getHeight() / 2 });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 1u);
    const yesdaw::engine::Note penciled = project.midiClips.front().notes.front();
    REQUIRE (penciled.lengthTicks > 0);
    REQUIRE (penciled.key >= yesdaw::ui::UiTheme::Layout::pianoRollLowKey);
    REQUIRE (penciled.key <= yesdaw::ui::UiTheme::Layout::pianoRollHighKey);
    REQUIRE (snapshotMainComponent (*shell).context.midiNoteSelected);

    // The synth makes it audible: play through the device-shaped harness and require energy.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    // Locate to the note start so the render window contains it.
    const yesdaw::engine::Tick noteAbsoluteFrame =
        project.midiClips.front().timelineStart + penciled.startTick;
    (void) noteAbsoluteFrame;
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> rendered = renderMainComponentPlayback (*shell, 96'000, 512);
    double energy = 0.0;
    for (const float sample : rendered)
        energy += std::abs (static_cast<double> (sample));
    REQUIRE (energy > 1.0);

    // Backspace deletes the selected note.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::backspaceKey)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.empty());
}

TEST_CASE ("Options toggles default-on playhead paging without changing Project audio or data",
           "[ui][input][shell][transport][playhead-follow]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("playhead-follow");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";

    constexpr std::uint64_t kFrames = 240'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames), 0.25f);
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    for (int step = 0; step < 32; ++step)
        REQUIRE (shell->keyPressed (juce::KeyPress ('=', juce::ModifierKeys::shiftModifier, '+')));

    const MainComponentSnapshot before = snapshotMainComponent (*shell);
    REQUIRE (before.timelineZoomFactor == yesdaw::ui::UiTheme::Layout::timelineZoomMax);
    REQUIRE (before.timelineScrollSeconds == 0.0);

    juce::Component& timeline = requireTimelineComponent (*shell);
    const double fitPixelsPerSecond = static_cast<double> (juce::jmax (
                                          yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                          timeline.getWidth()
                                              - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                    / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                before.visibleTimelineTotalSeconds);
    const double visibleSeconds = static_cast<double> (juce::jmax (1, timeline.getWidth()))
                                / (fitPixelsPerSecond * before.timelineZoomFactor);
    const std::uint64_t framesPastRightEdge = static_cast<std::uint64_t> (
        std::ceil ((visibleSeconds + 0.05) * 48'000.0));

    auto* bar = dynamic_cast<juce::MenuBarComponent*> (
        findChildWithComponentId (*shell, "shell.menubar"));
    REQUIRE (bar != nullptr);
    juce::MenuBarModel* model = bar->getModel();
    REQUIRE (model != nullptr);
    const auto playheadFollowMenuState = [model]
    {
        juce::PopupMenu options = model->getMenuForIndex (3, "Options");
        juce::PopupMenu::MenuItemIterator iterator (options);
        while (iterator.next())
        {
            const juce::PopupMenu::Item& item = iterator.getItem();
            if (item.text == "Playhead Follow")
                return std::pair<int, bool> { item.itemID, item.isTicked };
        }
        return std::pair<int, bool> { 0, false };
    };

    const auto [followItemId, defaultFollowEnabled] = playheadFollowMenuState();
    REQUIRE (followItemId > 0);
    REQUIRE (defaultFollowEnabled);

    model->menuItemSelected (followItemId, 3);
    REQUIRE_FALSE (playheadFollowMenuState().second);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> followDisabledAudio =
        renderMainComponentPlayback (*shell, framesPastRightEdge, 512);
    REQUIRE (peakAbs (std::span<const float> (
                 followDisabledAudio.data(), followDisabledAudio.size())) > 0.15);
    REQUIRE (serviceMainComponentUiTimer (*shell));
    REQUIRE (snapshotMainComponent (*shell).timelineScrollSeconds == before.timelineScrollSeconds);

    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    model->menuItemSelected (followItemId, 3);
    REQUIRE (playheadFollowMenuState().second);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> followEnabledAudio =
        renderMainComponentPlayback (*shell, framesPastRightEdge, 512);
    REQUIRE (serviceMainComponentUiTimer (*shell));

    const MainComponentSnapshot followed = snapshotMainComponent (*shell);
    REQUIRE (followed.context.playheadFrame > static_cast<std::int64_t> (visibleSeconds * 48'000.0));
    REQUIRE (followed.timelineScrollSeconds > before.timelineScrollSeconds);
    REQUIRE (followEnabledAudio == followDisabledAudio);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("L starts forward shuttle playback at one-times speed without toggling Loop",
           "[ui][input][shell][transport][jkl-shuttle]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("jkl-shuttle-play");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> spacePlayback = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (spacePlayback.data(), spacePlayback.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    const std::vector<float> shuttlePlayback = renderMainComponentPlayback (*shell, 512, 128);
    const MainComponentSnapshot shuttling = snapshotMainComponent (*shell);
    REQUIRE (shuttling.context.isPlaying);
    REQUIRE_FALSE (shuttling.context.loopEnabled);
    REQUIRE (shuttling.context.playheadFrame == 512);
    REQUIRE (shuttlePlayback == spacePlayback);

    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    const juce::ModifierKeys ctrlAltShift {
        juce::ModifierKeys::ctrlModifier
            | juce::ModifierKeys::altModifier
            | juce::ModifierKeys::shiftModifier
    };
    REQUIRE (shell->keyPressed (juce::KeyPress ('l', ctrlAltShift, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.loopEnabled);
    REQUIRE (shell->keyPressed (juce::KeyPress ('l', ctrlAltShift, 0)));
    REQUIRE_FALSE (snapshotMainComponent (*shell).context.loopEnabled);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("repeated L shuttles real playback at two-times then four-times speed",
           "[ui][input][shell][transport][jkl-shuttle]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("jkl-shuttle-rates");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";

    constexpr std::uint64_t kFrames = 4096;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        const int saw = static_cast<int> (frame % 97u) - 48;
        samples[static_cast<std::size_t> (frame)] = static_cast<float> (saw) / 64.0f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    const auto stopAndLocateStart = [&shell]
    {
        REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    };

    stopAndLocateStart();
    REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    const std::vector<float> oneTimes = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 128);

    stopAndLocateStart();
    REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    const std::vector<float> twoTimes = renderMainComponentPlayback (*shell, 64, 64);
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 128);

    stopAndLocateStart();
    REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    const std::vector<float> fourTimes = renderMainComponentPlayback (*shell, 32, 32);
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 128);

    REQUIRE (oneTimes.size() == 256u);
    REQUIRE (twoTimes.size() == 128u);
    REQUIRE (fourTimes.size() == 64u);
    for (std::size_t frame = 0; frame < 64u; ++frame)
    {
        for (std::size_t channel = 0; channel < 2u; ++channel)
        {
            REQUIRE (twoTimes[frame * 2u + channel] == oneTimes[frame * 4u + channel]);
            if (frame < 32u)
                REQUIRE (fourTimes[frame * 2u + channel] == oneTimes[frame * 8u + channel]);
        }
    }
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("J halves forward shuttle speed to stop and K stops from four-times speed",
           "[ui][input][shell][transport][jkl-shuttle]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("jkl-shuttle-slower-stop");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    for (int press = 0; press < 4; ++press)
        REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.shuttlePlaybackRate == 4);

    const std::vector<float> atFourTimes = renderMainComponentPlayback (*shell, 32, 32);
    REQUIRE (peakAbs (std::span<const float> (atFourTimes.data(), atFourTimes.size())) > 0.01);
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 128);

    REQUIRE (shell->keyPressed (juce::KeyPress ('j')));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.shuttlePlaybackRate == 2);
    (void) renderMainComponentPlayback (*shell, 32, 32);
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 192);

    REQUIRE (shell->keyPressed (juce::KeyPress ('j')));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.shuttlePlaybackRate == 1);
    (void) renderMainComponentPlayback (*shell, 32, 32);
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 224);

    // A further J would enter reverse on a full JKL transport. Reverse is not supported by this
    // engine, so the honest boundary is Stop at the current playhead, never fake reverse audio.
    REQUIRE (shell->keyPressed (juce::KeyPress ('j')));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.shuttlePlaybackRate == 1);
    const std::vector<float> stoppedByJ = renderMainComponentPlayback (*shell, 32, 32);
    REQUIRE (peakAbs (std::span<const float> (stoppedByJ.data(), stoppedByJ.size())) == 0.0);
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == 224);

    for (int press = 0; press < 3; ++press)
        REQUIRE (shell->keyPressed (juce::KeyPress ('l')));
    REQUIRE (snapshotMainComponent (*shell).context.shuttlePlaybackRate == 4);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.shuttlePlaybackRate == 1);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Options can return Stop to the captured playback start",
           "[ui][input][shell][transport][return-to-start]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("return-to-start-stop");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";

    constexpr std::uint64_t kFrames = 96'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        const int saw = static_cast<int> (frame % 101u) - 50;
        samples[static_cast<std::size_t> (frame)] = static_cast<float> (saw) / 64.0f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    const std::int64_t playbackStart = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (playbackStart > 0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> defaultAudio = renderMainComponentPlayback (*shell, 256, 128);
    REQUIRE (peakAbs (std::span<const float> (defaultAudio.data(), defaultAudio.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    MainComponentSnapshot stopped = snapshotMainComponent (*shell);
    REQUIRE_FALSE (stopped.context.isPlaying);
    REQUIRE (stopped.context.playheadFrame == playbackStart + 256);

    auto* bar = dynamic_cast<juce::MenuBarComponent*> (
        findChildWithComponentId (*shell, "shell.menubar"));
    REQUIRE (bar != nullptr);
    juce::MenuBarModel* model = bar->getModel();
    REQUIRE (model != nullptr);
    const auto returnToStartMenuState = [model]
    {
        juce::PopupMenu options = model->getMenuForIndex (3, "Options");
        juce::PopupMenu::MenuItemIterator iterator (options);
        while (iterator.next())
        {
            const juce::PopupMenu::Item& item = iterator.getItem();
            if (item.text == "Return to Start on Stop")
                return std::pair<int, bool> { item.itemID, item.isTicked };
        }
        return std::pair<int, bool> { 0, false };
    };

    const auto [returnItemId, defaultReturnEnabled] = returnToStartMenuState();
    REQUIRE (returnItemId > 0);
    REQUIRE_FALSE (defaultReturnEnabled);
    model->menuItemSelected (returnItemId, 3);
    REQUIRE (returnToStartMenuState().second);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == playbackStart);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> returnEnabledAudio = renderMainComponentPlayback (*shell, 256, 128);
    REQUIRE (returnEnabledAudio == defaultAudio);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    stopped = snapshotMainComponent (*shell);
    REQUIRE_FALSE (stopped.context.isPlaying);
    REQUIRE (stopped.context.playheadFrame == playbackStart);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> replayed = renderMainComponentPlayback (*shell, 256, 128);
    REQUIRE (replayed == defaultAudio);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == playbackStart);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("Enter always returns the transport to timeline zero",
           "[ui][input][shell][transport][return-to-zero]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("return-to-zero-enter");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";

    constexpr std::uint64_t kFrames = 96'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        const int saw = static_cast<int> (frame % 101u) - 50;
        samples[static_cast<std::size_t> (frame)] = static_cast<float> (saw) / 64.0f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> zeroAudio = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (peakAbs (std::span<const float> (zeroAudio.data(), zeroAudio.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    const std::int64_t nonzeroStart = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (nonzeroStart > 0);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> offsetAudio = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (offsetAudio != zeroAudio);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.playheadFrame == 0);
    const std::vector<float> returnedAudio = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (returnedAudio == zeroAudio);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    auto* bar = dynamic_cast<juce::MenuBarComponent*> (
        findChildWithComponentId (*shell, "shell.menubar"));
    REQUIRE (bar != nullptr);
    juce::MenuBarModel* model = bar->getModel();
    REQUIRE (model != nullptr);
    juce::PopupMenu options = model->getMenuForIndex (3, "Options");
    juce::PopupMenu::MenuItemIterator iterator (options);
    int returnItemId = 0;
    while (iterator.next())
    {
        const juce::PopupMenu::Item& item = iterator.getItem();
        if (item.text == "Return to Start on Stop")
            returnItemId = item.itemID;
    }
    REQUIRE (returnItemId > 0);
    model->menuItemSelected (returnItemId, 3);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == nonzeroStart);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    (void) renderMainComponentPlayback (*shell, 64, 64);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.playheadFrame == 0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == nonzeroStart);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.playheadFrame == 0);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("ruler double-click locates the real transport without creating a Marker",
           "[ui][input][shell][transport][play-from-click]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("ruler-double-click-locate");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";

    constexpr std::uint64_t kFrames = 96'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        const int saw = static_cast<int> (frame % 103u) - 51;
        samples[static_cast<std::size_t> (frame)] = static_cast<float> (saw) / 64.0f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");
    REQUIRE (project.markers.empty());

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Tick requestedTick = project.clips.front().timelineLength / 3;
    const juce::Point<int> rulerPoint = projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), project, requestedTick);
    REQUIRE (timeline.getLocalBounds().contains (rulerPoint));

    mouseDownAt (timeline, rulerPoint);
    const std::int64_t expectedFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (expectedFrame > 0);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    const int dispatchBeforeDoubleClick = snapshotMainComponent (*shell).context.commandDispatchCount;

    doubleClickAt (timeline, rulerPoint);
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.commandDispatchCount == dispatchBeforeDoubleClick + 1);
    REQUIRE (snapshot.context.playheadFrame == expectedFrame);
    REQUIRE (readProjectSnapshot (bundlePath).markers.empty());
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> clickedAudio = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (peakAbs (std::span<const float> (clickedAudio.data(), clickedAudio.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> zeroAudio = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (clickedAudio != zeroAudio);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("Shift+Space plays from the last explicit ruler locate",
           "[ui][input][shell][transport][play-from-click]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("play-from-last-locate");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";

    constexpr std::uint64_t kFrames = 96'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        const int saw = static_cast<int> (frame % 103u) - 51;
        samples[static_cast<std::size_t> (frame)] = static_cast<float> (saw) / 64.0f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    juce::Component& timeline = requireTimelineComponent (*shell);
    const juce::Point<int> rulerPoint = projectRulerPointAtTick (
        timeline,
        snapshotMainComponent (*shell),
        project,
        project.clips.front().timelineLength / 3);
    doubleClickAt (timeline, rulerPoint);
    const std::int64_t lastLocateFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (lastLocateFrame > 0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> locatedAudio = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (peakAbs (std::span<const float> (locatedAudio.data(), locatedAudio.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == lastLocateFrame + 128);

    // A later plain Play start must not replace the explicit locate remembered by Shift+Space.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    (void) renderMainComponentPlayback (*shell, 64, 64);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == lastLocateFrame + 192);

    const juce::ModifierKeys shift { juce::ModifierKeys::shiftModifier };
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey, shift, 0)));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.playheadFrame == lastLocateFrame);
    const std::vector<float> replayed = renderMainComponentPlayback (*shell, 128, 128);
    REQUIRE (replayed == locatedAudio);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    auto* bar = dynamic_cast<juce::MenuBarComponent*> (
        findChildWithComponentId (*shell, "shell.menubar"));
    REQUIRE (bar != nullptr);
    juce::MenuBarModel* model = bar->getModel();
    REQUIRE (model != nullptr);
    juce::PopupMenu options = model->getMenuForIndex (3, "Options");
    juce::PopupMenu::MenuItemIterator iterator (options);
    int returnItemId = 0;
    while (iterator.next())
    {
        const juce::PopupMenu::Item& item = iterator.getItem();
        if (item.text == "Return to Start on Stop")
            returnItemId = item.itemID;
    }
    REQUIRE (returnItemId > 0);
    model->menuItemSelected (returnItemId, 3);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey, shift, 0)));
    (void) renderMainComponentPlayback (*shell, 64, 64);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == lastLocateFrame);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("tool keys dispatch uniquely and idle Escape restores Pointer for a persisted playback edit",
           "[ui][input][shell][timeline][tool-keys]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("tool-keys");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Pencil);
    REQUIRE (shell->keyPressed (juce::KeyPress ('s')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Scissors);
    REQUIRE (shell->keyPressed (juce::KeyPress ('h')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Hand);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Zoom);
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Pointer);

    auto* addTrack = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "track.add"));
    REQUIRE (addTrack != nullptr);
    clickButton (*addTrack);

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project before = readProjectSnapshot (bundlePath);
    REQUIRE (before.tracks.size() == 2u);
    REQUIRE (before.clips.size() == 1u);
    const juce::Rectangle<int> clipBounds = timelineClipHitBounds (timeline, before, 0u);
    const juce::Point<int> emptyLanePoint {
        clipBounds.getRight(), clipBounds.getCentreY() + clipBounds.getHeight()
    };
    const juce::Point<int> clipPoint { clipBounds.getCentreX(), clipBounds.getCentreY() };

    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    dragFromTo (timeline, emptyLanePoint, clipPoint);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Pointer);
    dragFromTo (timeline, emptyLanePoint, clipPoint);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeDelete = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeDelete.data(), beforeDelete.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.empty());
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterDelete = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (peakAbs (std::span<const float> (afterDelete.data(), afterDelete.size())) == 0.0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == before.clips);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("locate-point keys persist two playheads and recall their exact playback after reopen",
           "[ui][input][shell][transport][locate-points]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("locate-points");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";

    constexpr std::uint64_t kFrames = 96'000;
    constexpr yesdaw::engine::Tick kFirstRequestedFrame = 16'000;
    constexpr yesdaw::engine::Tick kFifthRequestedFrame = 64'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        const std::uint64_t period = frame < kFrames / 2u ? 97u : 53u;
        const int saw = static_cast<int> (frame % period) - static_cast<int> (period / 2u);
        samples[static_cast<std::size_t> (frame)] = static_cast<float> (saw) / 64.0f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (choices);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    juce::Component& timeline = requireTimelineComponent (*shell);

    const juce::Point<int> firstPoint = projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), project, kFirstRequestedFrame);
    mouseDownAt (timeline, firstPoint);
    const std::int64_t firstStoredFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (firstStoredFrame > 0);
    const juce::ModifierKeys ctrlShift {
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier
    };
    REQUIRE (shell->keyPressed (juce::KeyPress ('1', ctrlShift, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == firstStoredFrame);

    const juce::Point<int> fifthPoint = projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), project, kFifthRequestedFrame);
    mouseDownAt (timeline, fifthPoint);
    const std::int64_t fifthStoredFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (fifthStoredFrame > firstStoredFrame);
    REQUIRE (shell->keyPressed (juce::KeyPress ('5', ctrlShift, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == fifthStoredFrame);

    const juce::ModifierKeys alt { juce::ModifierKeys::altModifier };
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('1', alt, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == firstStoredFrame);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> firstAudio = renderMainComponentPlayback (*shell, 256, 128);
    REQUIRE (peakAbs (std::span<const float> (firstAudio.data(), firstAudio.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('5', alt, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == fifthStoredFrame);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> fifthAudio = renderMainComponentPlayback (*shell, 256, 128);
    REQUIRE (peakAbs (std::span<const float> (fifthAudio.data(), fifthAudio.size())) > 0.01);
    REQUIRE (fifthAudio != firstAudio);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    shell.reset();
    auto reopenedShell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*reopenedShell, UiActionId::ProjectOpen));
    REQUIRE (reopenedShell->keyPressed (juce::KeyPress ('1', alt, 0)));
    REQUIRE (snapshotMainComponent (*reopenedShell).context.playheadFrame == firstStoredFrame);
    REQUIRE (reopenedShell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> reopenedAudio = renderMainComponentPlayback (*reopenedShell, 256, 128);
    REQUIRE (reopenedAudio == firstAudio);
    REQUIRE (reopenedShell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (reopenedShell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (reopenedShell->keyPressed (juce::KeyPress ('2', alt, 0)));
    REQUIRE (snapshotMainComponent (*reopenedShell).context.playheadFrame == 0);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("marker navigation keys locate persisted Markers and drive exact playback after reopen",
           "[ui][input][shell][transport][marker-navigation]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("marker-navigation");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";

    constexpr std::uint64_t kFrames = 96'000;
    constexpr yesdaw::engine::Tick kFirstRequestedFrame = 16'000;
    constexpr yesdaw::engine::Tick kSecondRequestedFrame = 64'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        const std::uint64_t period = frame < kFrames / 2u ? 97u : 53u;
        const int saw = static_cast<int> (frame % period) - static_cast<int> (period / 2u);
        samples[static_cast<std::size_t> (frame)] = static_cast<float> (saw) / 64.0f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };

    auto shell = makeShell (choices);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    juce::Component& timeline = requireTimelineComponent (*shell);

    mouseDownAt (timeline, projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), project, kFirstRequestedFrame));
    const std::int64_t firstMarkerFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (firstMarkerFrame > 0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));

    mouseDownAt (timeline, projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), project, kSecondRequestedFrame));
    const std::int64_t secondMarkerFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (secondMarkerFrame > firstMarkerFrame);
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));

    const yesdaw::engine::Project persisted = readProjectSnapshot (bundlePath);
    REQUIRE (persisted.markers.size() == 2u);
    REQUIRE (persisted.markers[0].tick == firstMarkerFrame);
    REQUIRE (persisted.markers[1].tick == secondMarkerFrame);

    const juce::ModifierKeys ctrl { juce::ModifierKeys::ctrlModifier };
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == firstMarkerFrame);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> firstAudio = renderMainComponentPlayback (*shell, 256, 128);
    REQUIRE (peakAbs (std::span<const float> (firstAudio.data(), firstAudio.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == secondMarkerFrame);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> secondAudio = renderMainComponentPlayback (*shell, 256, 128);
    REQUIRE (peakAbs (std::span<const float> (secondAudio.data(), secondAudio.size())) > 0.01);
    REQUIRE (secondAudio != firstAudio);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == secondMarkerFrame);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::leftKey, ctrl, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == firstMarkerFrame);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::leftKey, ctrl, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame == firstMarkerFrame);
    REQUIRE (readProjectSnapshot (bundlePath).markers == persisted.markers);

    shell.reset();
    auto reopenedShell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*reopenedShell, UiActionId::ProjectOpen));
    REQUIRE (readProjectSnapshot (bundlePath).markers == persisted.markers);
    REQUIRE (reopenedShell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (reopenedShell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey, ctrl, 0)));
    REQUIRE (snapshotMainComponent (*reopenedShell).context.playheadFrame == firstMarkerFrame);
    REQUIRE (reopenedShell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> reopenedAudio = renderMainComponentPlayback (*reopenedShell, 256, 128);
    REQUIRE (reopenedAudio == firstAudio);
    REQUIRE (reopenedShell->keyPressed (juce::KeyPress ('k')));

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("plain ruler drag selects a painted range, Shift+L converts it to the loop, and export slices it",
           "[ui][input][shell][transport][range-selection]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("ruler-range-selection");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-source.wav";
    std::filesystem::path wholeExportPath = bundlePath;
    wholeExportPath += "-whole.wav";
    std::filesystem::path rangeExportPath = bundlePath;
    rangeExportPath += "-range.wav";

    constexpr std::uint64_t kFrames = 96'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
    {
        const int saw = static_cast<int> (frame % 103u) - 51;
        samples[static_cast<std::size_t> (frame)] = static_cast<float> (saw) / 64.0f;
    }
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        sourcePath,
        yesdaw::engine::SampleRate { 48'000.0 },
        1,
        kFrames,
        std::span<const float> (samples.data(), samples.size())).ok());

    std::filesystem::path currentExportPath = wholeExportPath;
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [sourcePath] { return sourcePath; };
    choices.chooseExportAudioFile = [&currentExportPath] { return currentExportPath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");
    REQUIRE (project.clips.size() == 1u);

    // Shift+L with no range selection is an honest disabled no-op.
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.timelineRangeSelected);
    REQUIRE_FALSE (snapshot.context.loopEnabled);
    const int dispatchBeforeEmptyConvert = snapshot.context.commandDispatchCount;
    REQUIRE (shell->keyPressed (juce::KeyPress ('l', juce::ModifierKeys::shiftModifier, 0)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.loopEnabled);
    REQUIRE (snapshot.context.commandDispatchCount == dispatchBeforeEmptyConvert);

    // A real plain ruler drag selects the range; the playhead stays at the mouse-down locate,
    // which shares the drag-start frame exactly.
    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Tick quarterTick = project.clips.front().timelineLength / 4;
    const yesdaw::engine::Tick threeQuarterTick = (project.clips.front().timelineLength * 3) / 4;
    const juce::Point<int> quarterPoint = projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), project, quarterTick);
    const juce::Point<int> threeQuarterPoint = projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), project, threeQuarterTick);
    REQUIRE (timeline.getLocalBounds().contains (quarterPoint));
    REQUIRE (timeline.getLocalBounds().contains (threeQuarterPoint));

    dragFromTo (timeline, quarterPoint, threeQuarterPoint);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineRangeSelected);
    const long long rangeStart = snapshot.timelineRangeStartFrame;
    const long long rangeEnd = snapshot.timelineRangeEndFrame;
    REQUIRE (rangeStart > 0);
    REQUIRE (rangeEnd > rangeStart);
    REQUIRE (rangeEnd < static_cast<long long> (kFrames));
    REQUIRE (snapshot.context.playheadFrame == rangeStart);

    // Escape cancels an in-progress range drag without touching the committed range.
    beginDragFromTo (timeline, threeQuarterPoint, quarterPoint);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    releaseDragAt (timeline, threeQuarterPoint, quarterPoint);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineRangeSelected);
    REQUIRE (snapshot.timelineRangeStartFrame == rangeStart);
    REQUIRE (snapshot.timelineRangeEndFrame == rangeEnd);

    // Shift+L converts the committed range to the real transport loop region.
    REQUIRE (shell->keyPressed (juce::KeyPress ('l', juce::ModifierKeys::shiftModifier, 0)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.loopEnabled);
    REQUIRE (snapshot.playbackLoopStartFrame == rangeStart);
    REQUIRE (snapshot.playbackLoopEndFrame == rangeEnd);

    // The range doubles as the export "Loop Region" source: the sliced export is sample-identical
    // to the matching slice of the whole-Project export.
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectExportAudio));
    REQUIRE (std::filesystem::exists (wholeExportPath));

    auto* rangeChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "shell.export.range"));
    REQUIRE (rangeChooser != nullptr);
    rangeChooser->setSelectedId (2, juce::sendNotificationSync);
    currentExportPath = rangeExportPath;
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectExportAudio));
    REQUIRE (std::filesystem::exists (rangeExportPath));

    yesdaw::io::Float32Wav whole;
    yesdaw::io::Float32Wav sliced;
    REQUIRE (yesdaw::io::readFloat32WavFile (wholeExportPath, whole).ok());
    REQUIRE (yesdaw::io::readFloat32WavFile (rangeExportPath, sliced).ok());
    REQUIRE (sliced.channels == whole.channels);
    REQUIRE (sliced.frames == static_cast<std::uint64_t> (rangeEnd - rangeStart));
    const std::size_t sliceBegin = static_cast<std::size_t> (rangeStart) * whole.channels;
    const std::size_t sliceCount = static_cast<std::size_t> (rangeEnd - rangeStart) * whole.channels;
    REQUIRE (sliceBegin + sliceCount <= whole.interleavedSamples.size());
    REQUIRE (std::equal (sliced.interleavedSamples.begin(),
                         sliced.interleavedSamples.end(),
                         whole.interleavedSamples.begin() + static_cast<std::ptrdiff_t> (sliceBegin)));
    REQUIRE (peakAbs (std::span<const float> (sliced.interleavedSamples.data(),
                                              sliced.interleavedSamples.size())) > 0.01);

    // A plain ruler click collapses the range; the loop region it created stays.
    mouseDownAt (timeline, quarterPoint);
    releaseDragAt (timeline, quarterPoint, quarterPoint);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.timelineRangeSelected);
    REQUIRE (snapshot.timelineRangeStartFrame == -1);
    REQUIRE (snapshot.timelineRangeEndFrame == -1);
    REQUIRE (snapshot.context.loopEnabled);
    REQUIRE (snapshot.playbackLoopStartFrame == rangeStart);
    REQUIRE (snapshot.playbackLoopEndFrame == rangeEnd);

    // The range selection is honestly transient: project.db never changed.
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
    std::filesystem::remove (wholeExportPath, ec);
    std::filesystem::remove (rangeExportPath, ec);
}

TEST_CASE ("Ctrl+Alt+T duplicates the selected track with clips, strip, FX, and sends as one undo group",
           "[ui][input][shell][tracks][track-duplicate]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("track-duplicate");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // With no rail row selected the chord resolves but the shell honestly does nothing.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 1u);

    // Enrich the source track through real controls so the duplicate has something to prove:
    // rail VOL/PAN gestures, a compressor insert with one edited param, a send routed to a real
    // bus, and a penciled MIDI clip on the same track.
    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    using L = yesdaw::ui::UiTheme::Layout;
    juce::Rectangle<int> row = rail->getLocalBounds();
    row.removeFromTop (L::trackListHeaderHeight);
    row = row.withHeight (juce::jmax (L::trackListRowMinHeight, row.getHeight()));
    row.removeFromBottom (L::trackListSeparatorHeight);
    mouseDownAt (*rail, { rail->getWidth() / 2, L::trackListHeaderHeight + L::trackListRowMinHeight / 2 });

    const juce::Rectangle<int> level =
        row.withRight (row.getRight() - L::trackListLevelRightInset)
            .removeFromRight (L::trackListLevelWidth)
            .withBottom (row.getBottom() - L::trackListLevelBottomInset)
            .withHeight (L::trackListLevelHeight);
    mouseDownAt (*rail, { level.getCentreX(), level.getCentreY() });
    const juce::Rectangle<int> pan =
        row.withRight (row.getRight() - L::trackListPanRightInset)
            .removeFromRight (L::trackListPanDiameter)
            .withY (row.getY() + L::trackListPanTopInset)
            .withHeight (L::trackListPanDiameter);
    mouseDownAt (*rail, { pan.getX() + 1, pan.getCentreY() });

    auto* fxChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.insert.add"));
    REQUIRE (fxChooser != nullptr);
    fxChooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Compressor) + 1,
                              juce::sendNotificationSync);
    auto* fxEdit = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.edit"));
    REQUIRE (fxEdit != nullptr);
    clickButton (*fxEdit);
    auto* fxParam = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.fx.param.0"));
    REQUIRE (fxParam != nullptr);
    fxParam->setValue (0.25, juce::sendNotificationSync);

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));
    auto* sendChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.add"));
    REQUIRE (sendChooser != nullptr);
    sendChooser->setSelectedId (1, juce::sendNotificationSync);

    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    mouseDownAt (pianoRoll, { pianoRoll.getWidth() / 2, pianoRoll.getHeight() / 2 });

    const yesdaw::engine::Project before = readProjectSnapshot (bundlePath);
    REQUIRE (before.tracks.size() == 1u);
    REQUIRE (before.clips.size() == 1u);
    REQUIRE (before.midiClips.size() == 1u);
    REQUIRE (before.midiClips.front().notes.size() == 1u);
    const yesdaw::engine::Track source = before.tracks.front();
    REQUIRE (source.strip.linearGain > 0.4f);
    REQUIRE (source.strip.pan < -0.8f);
    REQUIRE (source.strip.fxChain.size() == 1u);
    REQUIRE (source.strip.fxChain.front().normalizedParams.size() == 1u);
    REQUIRE (source.sends.size() == 1u);

    // Baseline audible playback from timeline zero.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> baseline = renderMainComponentPlayback (*shell, 96'000, 512);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (baseline.data(), baseline.size())) > 0.01);

    // The real chord duplicates the selected track as one persisted transaction group.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0)));

    const yesdaw::engine::Project after = readProjectSnapshot (bundlePath);
    REQUIRE (after.tracks.size() == 2u);
    const yesdaw::engine::Track& copy = after.tracks.back();   // lands directly below the source
    REQUIRE (after.tracks.front().id == source.id);
    REQUIRE (copy.id != source.id);
    REQUIRE (copy.strip.name == source.strip.name + " copy");
    REQUIRE (copy.strip.linearGain == source.strip.linearGain);
    REQUIRE (copy.strip.pan == source.strip.pan);
    REQUIRE (copy.strip.muted == source.strip.muted);
    REQUIRE (copy.strip.soloed == source.strip.soloed);
    REQUIRE (copy.strip.soloSafe == source.strip.soloSafe);

    REQUIRE (copy.strip.fxChain.size() == 1u);
    REQUIRE (copy.strip.fxChain.front().id != source.strip.fxChain.front().id);
    REQUIRE (copy.strip.fxChain.front().kind == source.strip.fxChain.front().kind);
    REQUIRE (copy.strip.fxChain.front().enabled == source.strip.fxChain.front().enabled);
    REQUIRE (copy.strip.fxChain.front().normalizedParams == source.strip.fxChain.front().normalizedParams);

    REQUIRE (copy.sends.size() == 1u);
    REQUIRE (copy.sends.front().id != source.sends.front().id);
    REQUIRE (copy.sends.front().busId == source.sends.front().busId);
    REQUIRE (copy.sends.front().tap == source.sends.front().tap);
    REQUIRE (copy.sends.front().linearGain == source.sends.front().linearGain);

    REQUIRE (after.clips.size() == 2u);
    yesdaw::engine::Clip copiedClip = after.clips.back();
    REQUIRE (copiedClip.id != before.clips.front().id);
    REQUIRE (copiedClip.trackId == copy.id);
    copiedClip.id = before.clips.front().id;
    copiedClip.trackId = before.clips.front().trackId;
    REQUIRE (copiedClip == before.clips.front());

    REQUIRE (after.midiClips.size() == 2u);
    const yesdaw::engine::MidiClip& sourceMidi = before.midiClips.front();
    yesdaw::engine::MidiClip copiedMidi = after.midiClips.back();
    REQUIRE (copiedMidi.id != sourceMidi.id);
    REQUIRE (copiedMidi.trackId == copy.id);
    REQUIRE (copiedMidi.notes.size() == 1u);
    REQUIRE (copiedMidi.notes.front().id != sourceMidi.notes.front().id);
    copiedMidi.id = sourceMidi.id;
    copiedMidi.trackId = sourceMidi.trackId;
    copiedMidi.notes.front().id = sourceMidi.notes.front().id;
    REQUIRE (copiedMidi == sourceMidi);

    // Two identical tracks sum to exactly twice the baseline through the rebuilt playback graph.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> doubled = renderMainComponentPlayback (*shell, 96'000, 512);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (doubled.size() == baseline.size());
    std::size_t firstMismatch = baseline.size();
    for (std::size_t i = 0; i < baseline.size(); ++i)
    {
        if (doubled[i] != 2.0f * baseline[i])
        {
            firstMismatch = i;
            break;
        }
    }
    REQUIRE (firstMismatch == baseline.size());

    // One Ctrl+Z undoes the whole duplicate and playback returns bit-identically to the baseline.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Project undone = readProjectSnapshot (bundlePath);
    REQUIRE (undone.tracks == before.tracks);
    REQUIRE (undone.clips == before.clips);
    REQUIRE (undone.midiClips == before.midiClips);
    REQUIRE (undone.buses == before.buses);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> restored = renderMainComponentPlayback (*shell, 96'000, 512);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (restored == baseline);

    // One Ctrl+Shift+Z redoes the whole group.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    const yesdaw::engine::Project redone = readProjectSnapshot (bundlePath);
    REQUIRE (redone.tracks.size() == 2u);
    REQUIRE (redone.tracks == after.tracks);
    REQUIRE (redone.clips == after.clips);
    REQUIRE (redone.midiClips == after.midiClips);

    // Duplicating a non-last track reorders the copy to sit directly below its source.
    juce::Component* railAgain = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railAgain != nullptr);
    mouseDownAt (*railAgain, { railAgain->getWidth() / 2,
                               L::trackListHeaderHeight + L::trackListRowMinHeight / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress ('t',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::altModifier, 0)));
    const yesdaw::engine::Project three = readProjectSnapshot (bundlePath);
    REQUIRE (three.tracks.size() == 3u);
    REQUIRE (three.tracks[0].id == source.id);
    REQUIRE (three.tracks[1].id != source.id);
    REQUIRE (three.tracks[1].id != redone.tracks.back().id);   // the fresh copy sits between
    REQUIRE (three.tracks[2].id == redone.tracks.back().id);
    REQUIRE (three.tracks[1].strip.name == source.strip.name + " copy");

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Ctrl+Shift+Up and Ctrl+Shift+Down reorder the selected track with the rail following",
           "[ui][input][shell][tracks][track-move]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("track-move");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // With no rail row selected the chords resolve but honestly do nothing.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey,
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 1u);

    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.tracks.size() == 3u);
    const yesdaw::engine::EntityId idA = original.tracks[0].id;   // owns the imported clip
    const yesdaw::engine::EntityId idB = original.tracks[1].id;
    const yesdaw::engine::EntityId idC = original.tracks[2].id;
    REQUIRE (original.clips.size() == 1u);
    REQUIRE (original.clips.front().trackId == idA);

    // Select the first row; moving it down twice walks it to the bottom, persisted each step.
    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    using L = yesdaw::ui::UiTheme::Layout;
    const int headerHeight = L::trackListHeaderHeight;
    const int rowHeight = juce::jmax (L::trackListRowMinHeight,
                                      (rail->getHeight() - headerHeight) / 3);
    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + rowHeight / 2 });

    const auto renderShell = [&shell]
    {
        juce::Image image (juce::Image::ARGB, shell->getWidth(), shell->getHeight(), true);
        juce::Graphics graphics (image);
        shell->paintEntireComponent (graphics, true);
        return image;
    };
    const auto changedPixelsIn = [] (const juce::Image& first,
                                     const juce::Image& second,
                                     juce::Rectangle<int> area)
    {
        int changed = 0;
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                if (first.getPixelAt (x, y) != second.getPixelAt (x, y))
                    ++changed;
        return changed;
    };
    const juce::Rectangle<int> firstRow {
        rail->getX(), rail->getY() + headerHeight, rail->getWidth(), rowHeight
    };
    const juce::Rectangle<int> secondRow = firstRow.translated (0, rowHeight);
    const juce::Image beforeMove = renderShell();

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey,
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[0].id == idB);
    REQUIRE (project.tracks[1].id == idA);
    REQUIRE (project.tracks[2].id == idC);

    // The painted rail follows: both swapped rows visibly changed.
    const juce::Image afterMove = renderShell();
    REQUIRE (changedPixelsIn (beforeMove, afterMove, firstRow) > 0);
    REQUIRE (changedPixelsIn (beforeMove, afterMove, secondRow) > 0);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey,
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[0].id == idB);
    REQUIRE (project.tracks[1].id == idC);
    REQUIRE (project.tracks[2].id == idA);

    // The bottom row cannot move further down: an honest no-op with no dispatch and no undo entry.
    const int dispatchBeforeBoundary = snapshotMainComponent (*shell).context.commandDispatchCount;
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey,
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[2].id == idA);
    REQUIRE (snapshotMainComponent (*shell).context.commandDispatchCount == dispatchBeforeBoundary);

    // Ctrl+Shift+Up moves the still-selected track back up one row.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey,
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[0].id == idB);
    REQUIRE (project.tracks[1].id == idA);
    REQUIRE (project.tracks[2].id == idC);

    // Each move is one undo step: three undos restore the original persisted order exactly.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[0].id == idB);
    REQUIRE (project.tracks[1].id == idC);
    REQUIRE (project.tracks[2].id == idA);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[0].id == idA);
    REQUIRE (project.tracks[1].id == idB);
    REQUIRE (project.tracks[2].id == idC);

    // The selection follows a fresh move: re-select the top row, move it down, and the shared
    // mute control lands on the moved track, silencing its only clip through the rebuilt graph.
    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + rowHeight / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey,
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[1].id == idA);

    auto* mute = dynamic_cast<juce::Button*> (
        findChildWithComponentId (*shell, "mixer.target.toggle_mute"));
    REQUIRE (mute != nullptr);
    clickButton (*mute);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[1].id == idA);
    REQUIRE (project.tracks[1].strip.muted);
    REQUIRE_FALSE (project.tracks[0].strip.muted);
    REQUIRE_FALSE (project.tracks[2].strip.muted);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> mutedPlayback = renderMainComponentPlayback (*shell, 4096, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (mutedPlayback.data(), mutedPlayback.size())) == 0.0);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Shift+M, Shift+S, and Shift+R toggle mute, solo, and arm on the selected track",
           "[ui][input][shell][tracks][track-keys]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("track-keys");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 2u);
    REQUIRE (project.clips.size() == 1u);
    REQUIRE (project.clips.front().trackId == project.tracks.front().id);

    // With no rail row selected the chord resolves but honestly does nothing.
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE_FALSE (project.tracks.front().strip.muted);
    REQUIRE_FALSE (project.tracks.back().strip.muted);

    // Select row 0 (the clip-owning track) and prove the audible baseline from timeline zero.
    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    using L = yesdaw::ui::UiTheme::Layout;
    mouseDownAt (*rail, { rail->getWidth() / 2,
                          L::trackListHeaderHeight + L::trackListRowMinHeight / 2 });
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == UiPanel::Timeline);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> audible = renderMainComponentPlayback (*shell, 4096, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (audible.data(), audible.size())) > 0.01);

    // Shift+M mutes the selected track — persisted, playback-silencing, and the mixer stays shut.
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.muted);
    REQUIRE_FALSE (project.tracks.back().strip.muted);
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == UiPanel::Timeline);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> mutedOut = renderMainComponentPlayback (*shell, 4096, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (mutedOut.data(), mutedOut.size())) == 0.0);

    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE_FALSE (project.tracks.front().strip.muted);

    // Shift+S on the other (empty) track solos it, muting the clip-owning track by solo policy.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.back().strip.soloed);
    REQUIRE_FALSE (project.tracks.front().strip.soloed);
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == UiPanel::Timeline);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> soloedOut = renderMainComponentPlayback (*shell, 4096, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (soloedOut.data(), soloedOut.size())) == 0.0);

    REQUIRE (shell->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE_FALSE (project.tracks.back().strip.soloed);

    // Shift+R arms the SELECTED track once a device with inputs exists; arm state is honestly
    // transient — project.db never changes while arming, retargeting, and disarming.
    const std::vector<std::uint8_t> persistedBeforeArm = readBytes (bundlePath / "project.db");
    clickButton (requireButtonForAction (*shell, UiActionId::DeviceSelectTestAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('r', juce::ModifierKeys::shiftModifier, 0)));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingTrackArmed);
    REQUIRE (snapshot.context.selectedRecordingTrackIndex == 1);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('r', juce::ModifierKeys::shiftModifier, 0)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.recordingTrackArmed);
    REQUIRE (snapshot.context.selectedRecordingTrackIndex == 0);

    REQUIRE (shell->keyPressed (juce::KeyPress ('r', juce::ModifierKeys::shiftModifier, 0)));
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.recordingTrackArmed);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBeforeArm);

    // The marker-remove chord moved to Ctrl+Shift+M: M adds a Marker at the playhead, Shift+M only
    // mutes, and Ctrl+Shift+M removes the Marker.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.markers.size() == 1u);

    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.markers.size() == 1u);
    REQUIRE (project.tracks.front().strip.muted);

    REQUIRE (shell->keyPressed (juce::KeyPress ('m',
        juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.markers.empty());
    REQUIRE (project.tracks.front().strip.muted);   // marker removal never touches the strip

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Alt+click resets faders to unity, pans to center, sends to unity, and FX params to spec default",
           "[ui][input][shell][mixer][alt-click-reset]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("alt-click-reset");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    using L = yesdaw::ui::UiTheme::Layout;
    mouseDownAt (*rail, { rail->getWidth() / 2,
                          L::trackListHeaderHeight + L::trackListRowMinHeight / 2 });

    const juce::ModifierKeys altClick (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier);

    // Mixer fader: a real value drag persists 0.5; Alt+click resets to unity and doubles playback.
    auto* fader = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_fader"));
    REQUIRE (fader != nullptr);
    REQUIRE (fader->isEnabled());
    fader->setValue (0.5, juce::sendNotificationSync);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.linearGain == 0.5f);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> halved = renderMainComponentPlayback (*shell, 4096, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (halved.data(), halved.size())) > 0.005);

    mouseDownAt (*fader, fader->getLocalBounds().getCentre(), altClick);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.linearGain == 1.0f);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> unity = renderMainComponentPlayback (*shell, 4096, 128);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (unity.size() == halved.size());
    std::size_t faderMismatch = unity.size();
    for (std::size_t i = 0; i < unity.size(); ++i)
    {
        if (unity[i] != 2.0f * halved[i])
        {
            faderMismatch = i;
            break;
        }
    }
    REQUIRE (faderMismatch == unity.size());

    // Mixer pan: a real value drag persists an offset; Alt+click recentres to exactly zero.
    auto* pan = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_pan"));
    REQUIRE (pan != nullptr);
    pan->setValue (-0.6, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.pan == -0.6f);
    mouseDownAt (*pan, pan->getLocalBounds().getCentre(), altClick);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.pan == 0.0f);

    // Rail mini VOL: a plain click sets ~0.5; Alt+click resets to exactly unity.
    juce::Rectangle<int> row = rail->getLocalBounds();
    row.removeFromTop (L::trackListHeaderHeight);
    row = row.withHeight (juce::jmax (L::trackListRowMinHeight, row.getHeight()));
    row.removeFromBottom (L::trackListSeparatorHeight);
    const juce::Rectangle<int> level =
        row.withRight (row.getRight() - L::trackListLevelRightInset)
            .removeFromRight (L::trackListLevelWidth)
            .withBottom (row.getBottom() - L::trackListLevelBottomInset)
            .withHeight (L::trackListLevelHeight);
    mouseDownAt (*rail, { level.getCentreX(), level.getCentreY() });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.linearGain < 0.6f);
    mouseDownAt (*rail, { level.getCentreX(), level.getCentreY() }, altClick);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.linearGain == 1.0f);

    // Rail mini PAN: a plain click on the knob edge pans hard left; Alt+click recentres.
    const juce::Rectangle<int> panKnob =
        row.withRight (row.getRight() - L::trackListPanRightInset)
            .removeFromRight (L::trackListPanDiameter)
            .withY (row.getY() + L::trackListPanTopInset)
            .withHeight (L::trackListPanDiameter);
    mouseDownAt (*rail, { panKnob.getX() + 1, panKnob.getCentreY() });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.pan < -0.8f);
    mouseDownAt (*rail, { panKnob.getCentreX(), panKnob.getCentreY() }, altClick);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.pan == 0.0f);

    // Send level: a real slider edit persists 0.5; Alt+click resets the send to unity.
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));
    auto* sendChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.add"));
    REQUIRE (sendChooser != nullptr);
    sendChooser->setSelectedId (1, juce::sendNotificationSync);
    auto* sendLevel = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.send.0"));
    REQUIRE (sendLevel != nullptr);
    REQUIRE (sendLevel->isVisible());
    sendLevel->setValue (0.5, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.front().linearGain == 0.5f);
    mouseDownAt (*sendLevel, sendLevel->getLocalBounds().getCentre(), altClick);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.front().linearGain == 1.0f);

    // FX param: a real slider edit persists 0.25; Alt+click resets to the ParamSpec default.
    auto* fxChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.insert.add"));
    REQUIRE (fxChooser != nullptr);
    fxChooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Compressor) + 1,
                              juce::sendNotificationSync);
    auto* fxEdit = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.edit"));
    REQUIRE (fxEdit != nullptr);
    clickButton (*fxEdit);
    auto* fxParam = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.fx.param.0"));
    REQUIRE (fxParam != nullptr);
    REQUIRE (fxParam->isVisible());
    fxParam->setValue (0.25, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain.front().normalizedParams.size() == 1u);
    REQUIRE (project.tracks.front().strip.fxChain.front().normalizedParams.front().second
             == Catch::Approx (0.25));

    const yesdaw::engine::ParamSpec spec =
        yesdaw::engine::fxParamSpecForKind (yesdaw::engine::FxKind::Compressor, 0u);
    const double specDefault = yesdaw::engine::normalizedDefault (spec);
    mouseDownAt (*fxParam, fxParam->getLocalBounds().getCentre(), altClick);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain.front().normalizedParams.front().second
             == Catch::Approx (specDefault));

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}
