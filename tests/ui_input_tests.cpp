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

// E13: the velocity lane rect and its two mapping laws, mirroring the shipped inset chain.
juce::Rectangle<int> pianoRollVelocityLaneBounds (juce::Component& pianoRoll)
{
    auto area = pianoRoll.getLocalBounds();
    area.removeFromTop (38);
    area.reduce (12, 8);
    auto expression = area.removeFromBottom (84);
    expression = expression.reduced (0, 6);
    return expression.removeFromTop (36).reduced (0, 2);
}

yesdaw::engine::Tick pianoRollTickForLaneX (juce::Component& pianoRoll,
                                            const yesdaw::engine::MidiClip& midiClip,
                                            int x)
{
    const juce::Rectangle<int> grid = pianoRollGridBounds (pianoRoll);
    const double normalized = std::clamp (
        static_cast<double> (x - grid.getX()) / static_cast<double> (std::max (1, grid.getWidth())),
        0.0, 1.0);
    return static_cast<yesdaw::engine::Tick> (
        std::llround (normalized * static_cast<double> (std::max<yesdaw::engine::Tick> (1, midiClip.timelineLength))));
}

double pianoRollVelocityForLaneYPixel (juce::Rectangle<int> lane, int y)
{
    const double usable = static_cast<double> (std::max (1, lane.getHeight() - 10));
    const double bottom = static_cast<double> (lane.getBottom() - 5);
    return std::clamp ((bottom - static_cast<double> (y)) / usable, 0.0, 1.0);
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
    // 91 shell children + the B31 drag dB readout label (hidden until a gain drag) + the E7
    // marker rename editor (hidden until a marker double-click) + the E14 per-slot FX up/down
    // pairs (5 slots x 2, hidden until inserts exist) + the E15 per-row FX param choice
    // choosers (8) and the param page chooser + the E17 bus remove button and bus rename
    // editor + the E18 per-row send tap toggles and destination choosers (4 rows x 2) + the
    // E19 master fader + the E20 automation target chooser — bumped deliberately.
    REQUIRE (snapshot.childCount == static_cast<int> (mainShellToolbarActions().size() + 124u));
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
    // while the playhead stays at the mouse-down locate. E4: the committed range endpoints snap
    // through the snap chooser (the locate itself stays raw).
    const juce::Point<int> threeSeconds = emptyProjectRulerPointAtSeconds (timeline, 3.0);
    dragFromTo (timeline, twoSeconds, threeSeconds);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.playheadFrame == emptyProjectFrameAtRulerPoint (timeline, twoSeconds));
    REQUIRE (snapshot.context.timelineRangeSelected);
    REQUIRE (snapshot.context.snapEnabled);
    yesdaw::engine::Tick expectedRangeStart = 0;
    yesdaw::engine::Tick expectedRangeEnd = 0;
    REQUIRE (yesdaw::engine::snapTick (
        static_cast<yesdaw::engine::Tick> (emptyProjectFrameAtRulerPoint (timeline, twoSeconds)),
        yesdaw::engine::SnapGrid { snapshot.context.snapGridTicks }, expectedRangeStart));
    REQUIRE (yesdaw::engine::snapTick (
        static_cast<yesdaw::engine::Tick> (emptyProjectFrameAtRulerPoint (timeline, threeSeconds)),
        yesdaw::engine::SnapGrid { snapshot.context.snapGridTicks }, expectedRangeEnd));
    REQUIRE (snapshot.timelineRangeStartFrame == expectedRangeStart);
    REQUIRE (snapshot.timelineRangeEndFrame == expectedRangeEnd);

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
    // E12 re-pin: roll drags follow the REAL snap chooser now; this gate asserts the raw
    // pixel-exact laws, so it runs with the chooser Off.
    auto* snapChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (snapChooser != nullptr);
    snapChooser->setSelectedId (1, juce::sendNotificationSync);
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

    // E23: the inspector is not first-track-only — a clip imported onto the THIRD track edits
    // through the SAME controls, and the first track's clip stays untouched.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    const int thirdRowHeight = juce::jmax (
        yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
        (railComponent->getHeight() - yesdaw::ui::UiTheme::Layout::trackListHeaderHeight) / 3);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + 2 * thirdRowHeight + thirdRowHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    yesdaw::engine::Project thirdTrackProject = readProjectSnapshot (bundlePath);
    REQUIRE (thirdTrackProject.tracks.size() == 3u);
    REQUIRE (thirdTrackProject.clips.size() == 2u);
    REQUIRE (thirdTrackProject.clips.back().trackId == thirdTrackProject.tracks[2].id);
    REQUIRE (snapshotMainComponent (*shell).context.timelineClipSelected);

    dragHorizontalSliderToNormalizedValue (gain, 0.3);
    thirdTrackProject = readProjectSnapshot (bundlePath);
    REQUIRE (thirdTrackProject.clips.back().gain < 1.0f);
    REQUIRE (thirdTrackProject.clips.front() == edited.clips.front());
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips.back().gain == 1.0f);
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

    // E4: the double-click split tick snaps through the snap chooser; with the default grid far
    // coarser than this short clip, Ctrl inverts the grid so the raw click point splits.
    doubleClickAt (timeline, { 80, 100 },
                   juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                       | juce::ModifierKeys::ctrlModifier));

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

    // E4: the trim edge snaps through the snap chooser; Ctrl inverts so this exact -6px trim
    // keeps its raw semantics.
    const juce::Point<int> trimStart = timelineClipRightEdgeDragPoint (timeline, splitRedone, 1u);
    dragFromTo (timeline, trimStart, trimStart.translated (-6, 0),
                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                    | juce::ModifierKeys::ctrlModifier));

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
    // E12 re-pin: roll drags follow the REAL snap chooser now; these raw pixel-exact
    // expectations need the chooser Off.
    auto* pianoSnapChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (pianoSnapChooser != nullptr);
    pianoSnapChooser->setSelectedId (1, juce::sendNotificationSync);
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

TEST_CASE ("every direct strip edit is its own undo step: drags coalesce, toggles do not",
           "[ui][input][shell][mixer][strip-undo]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("strip-undo");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    // Sentinel edit: a marker. If any strip edit were still un-undoable, its Ctrl+Z would
    // silently eat THIS instead.
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));
    REQUIRE (readProjectSnapshot (bundlePath).markers.size() == 1u);

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    juce::Component* strips = findChildWithComponentId (*shell, "shell.mixer.strips.input");
    REQUIRE (strips != nullptr);
    mouseDownAt (*strips, { juce::jmax (yesdaw::ui::UiTheme::Layout::mixerStripMinWidth,
                                        strips->getWidth() / 2) / 2,
                            strips->getHeight() / 2 });

    auto* fader = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_fader"));
    auto* pan = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_pan"));
    auto* mute = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.target.toggle_mute"));
    auto* solo = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.target.toggle_solo"));
    REQUIRE (fader != nullptr);
    REQUIRE (pan != nullptr);
    REQUIRE (mute != nullptr);
    REQUIRE (solo != nullptr);

    // A REAL fader drag (down + drag + up, many value events) is ONE undo step.
    dragFromTo (*fader, { fader->getWidth() / 2, fader->getHeight() - 2 },
                { fader->getWidth() / 2, 2 });
    const float draggedGain = readProjectSnapshot (bundlePath).tracks.front().strip.linearGain;
    REQUIRE (draggedGain != 1.0f);
    // A second, separate drag is its OWN step.
    dragFromTo (*fader, { fader->getWidth() / 2, 2 },
                { fader->getWidth() / 2, fader->getHeight() / 2 });
    const float secondGain = readProjectSnapshot (bundlePath).tracks.front().strip.linearGain;
    REQUIRE (secondGain != draggedGain);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.linearGain == draggedGain);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.linearGain == 1.0f);
    // The sentinel survived both undos: the drags really were their own steps.
    REQUIRE (readProjectSnapshot (bundlePath).markers.size() == 1u);

    // A single pan set is one step.
    pan->setValue (0.25, juce::sendNotificationSync);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.pan == 0.25f);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.pan == 0.0f);

    // Two mute toggles NEVER coalesce — each is its own step.
    clickButton (*mute);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.muted);
    clickButton (*mute);
    REQUIRE_FALSE (readProjectSnapshot (bundlePath).tracks.front().strip.muted);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.muted);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE_FALSE (readProjectSnapshot (bundlePath).tracks.front().strip.muted);

    // Solo toggles undo the same way.
    clickButton (*solo);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.soloed);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE_FALSE (readProjectSnapshot (bundlePath).tracks.front().strip.soloed);

    // Rail mini edits (panel-preserving path) are undoable too: a VOL drag is one step, and a
    // rail mute click undoes without touching anything else.
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    using L = yesdaw::ui::UiTheme::Layout;
    juce::Rectangle<int> row = rail->getLocalBounds();
    row.removeFromTop (L::trackListHeaderHeight);
    row = row.withHeight (juce::jmax (L::trackListRowMinHeight, row.getHeight()));
    row.removeFromBottom (L::trackListSeparatorHeight);
    const juce::Rectangle<int> level =
        row.withRight (row.getRight() - L::trackListLevelRightInset)
            .removeFromRight (L::trackListLevelWidth)
            .withBottom (row.getBottom() - L::trackListLevelBottomInset)
            .withHeight (L::trackListLevelHeight);
    dragFromTo (*rail, { level.getCentreX(), level.getCentreY() },
                { level.getX() + 2, level.getCentreY() });
    const float railGain = readProjectSnapshot (bundlePath).tracks.front().strip.linearGain;
    REQUIRE (railGain != 1.0f);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.linearGain == 1.0f);

    juce::Rectangle<int> buttonsArea =
        row.withTrimmedLeft (L::trackListNameLeftInset)
            .withTrimmedTop (L::trackListButtonsTop)
            .withHeight (L::trackListButtonsHeight);
    const juce::Rectangle<int> muteCell = buttonsArea.removeFromLeft (L::trackListButtonWidth);
    mouseDownAt (*rail, muteCell.getCentre());
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.muted);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE_FALSE (readProjectSnapshot (bundlePath).tracks.front().strip.muted);

    // Everything above undone; exactly the sentinel remains, and one more undo removes it.
    REQUIRE (readProjectSnapshot (bundlePath).markers.size() == 1u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).markers.empty());
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

    // E23: sends are not first-track-only — the SAME controls route the THIRD track.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    const int thirdRowHeight = juce::jmax (
        yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
        (railComponent->getHeight() - yesdaw::ui::UiTheme::Layout::trackListHeaderHeight) / 3);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + 2 * thirdRowHeight + thirdRowHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    sendChooser->setSelectedId (1, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 3u);
    REQUIRE (project.tracks[2].sends.size() == 1u);
    REQUIRE (project.tracks[0].sends.empty());
    REQUIRE (project.tracks[1].sends.empty());
    level0->setValue (0.4, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[2].sends.front().linearGain == 0.4f);
    clickButton (*remove0);
    REQUIRE (readProjectSnapshot (bundlePath).tracks[2].sends.empty());

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
    // Six action items + the B39 Open Recent submenu.
    REQUIRE (model->getMenuForIndex (0, "File").getNumItems() == 7);
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

    // E23: param editing is not first-index-only — the SAME controls edit a NON-ZERO strip's
    // NON-ZERO slot. The third track gets an EQ then a Compressor; slot 1's threshold edit
    // persists on THAT track's slot 1 and nowhere else.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    const int thirdRowHeight = juce::jmax (
        yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
        (railComponent->getHeight() - yesdaw::ui::UiTheme::Layout::trackListHeaderHeight) / 3);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + 2 * thirdRowHeight + thirdRowHeight / 2 });
    chooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Eq) + 1, juce::sendNotificationSync);
    chooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Compressor) + 1,
                            juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 3u);
    REQUIRE (project.tracks[2].strip.fxChain.size() == 2u);
    REQUIRE (project.tracks[2].strip.fxChain[1].kind == yesdaw::engine::FxKind::Compressor);

    auto* edit1 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.1.edit"));
    REQUIRE (edit1 != nullptr);
    REQUIRE (edit1->isVisible());
    clickButton (*edit1);
    REQUIRE (param0->isVisible());
    param0->setValue (0.4, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks[2].strip.fxChain[1].normalizedParams.size() == 1u);
    REQUIRE (project.tracks[2].strip.fxChain[1].normalizedParams.front().first == 0u);
    REQUIRE (project.tracks[2].strip.fxChain[1].normalizedParams.front().second == Catch::Approx (0.4));
    REQUIRE (project.tracks[2].strip.fxChain[0].normalizedParams.empty());
    REQUIRE (project.tracks[0].strip.fxChain.front().normalizedParams.empty());
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks[2].strip.fxChain[1].normalizedParams.empty());
}

TEST_CASE ("FX slot up/down reorder the chain undoably and audibly for a non-commuting chain",
           "[ui][input][shell][mixer][fx-reorder]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("fx-reorder");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });

    auto* chooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.insert.add"));
    REQUIRE (chooser != nullptr);
    chooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Eq) + 1, juce::sendNotificationSync);
    chooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Limiter) + 1, juce::sendNotificationSync);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain.size() == 2u);
    REQUIRE (project.tracks.front().strip.fxChain[0].kind == yesdaw::engine::FxKind::Eq);
    REQUIRE (project.tracks.front().strip.fxChain[1].kind == yesdaw::engine::FxKind::Limiter);

    // Make the chain audibly non-commuting: a +24 dB band-0 gain boost (param 2; type, freq and
    // Q stay at their bell/1kHz defaults) drives the limiter, whose ceiling (param 0) drops to
    // -9 dBFS so it engages hard on the boosted signal.
    auto* edit0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.edit"));
    REQUIRE (edit0 != nullptr);
    clickButton (*edit0);
    auto* eqGainParam = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.fx.param.2"));
    REQUIRE (eqGainParam != nullptr);
    REQUIRE (eqGainParam->isVisible());
    eqGainParam->setValue (1.0, juce::sendNotificationSync);
    clickButton (*edit0);
    auto* edit1 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.1.edit"));
    REQUIRE (edit1 != nullptr);
    clickButton (*edit1);
    auto* limiterParam = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.fx.param.0"));
    REQUIRE (limiterParam != nullptr);
    REQUIRE (limiterParam->isVisible());
    limiterParam->setValue (0.25, juce::sendNotificationSync);
    clickButton (*edit1);

    // Renders happen from a playing transport at frame zero, stopped again after each capture.
    const auto renderFromStart = [&shell] {
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
        const std::vector<float> rendered = renderMainComponentPlayback (*shell, 48'000, 128);
        REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
        return rendered;
    };
    const std::vector<float> eqFirstRender = renderFromStart();
    REQUIRE (peakAbs (std::span<const float> (eqFirstRender.data(), eqFirstRender.size())) > 0.05);

    // Slot 0 cannot move earlier; slot 1's up button swaps the pair, params travel with inserts.
    auto* up0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.up"));
    auto* up1 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.1.up"));
    REQUIRE (up0 != nullptr);
    REQUIRE (up1 != nullptr);
    REQUIRE_FALSE (up0->isEnabled());
    REQUIRE (up1->isEnabled());
    clickButton (*up1);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain[0].kind == yesdaw::engine::FxKind::Limiter);
    REQUIRE (project.tracks.front().strip.fxChain[1].kind == yesdaw::engine::FxKind::Eq);
    REQUIRE (project.tracks.front().strip.fxChain[0].normalizedParams.size() == 1u);
    REQUIRE (project.tracks.front().strip.fxChain[0].normalizedParams.front().second
             == Catch::Approx (0.25));
    REQUIRE (snapshotMainComponent (*shell).playbackReady);

    // The reordered chain renders audibly differently, and one undo restores order AND audio.
    const std::vector<float> limiterFirstRender = renderFromStart();
    REQUIRE (limiterFirstRender != eqFirstRender);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain[0].kind == yesdaw::engine::FxKind::Eq);
    REQUIRE (project.tracks.front().strip.fxChain[1].kind == yesdaw::engine::FxKind::Limiter);
    const std::vector<float> restoredRender = renderFromStart();
    REQUIRE (restoredRender == eqFirstRender);

    // The down button on slot 0 makes the same swap from the other side.
    auto* down0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.down"));
    auto* down1 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.1.down"));
    REQUIRE (down0 != nullptr);
    REQUIRE (down1 != nullptr);
    REQUIRE (down0->isEnabled());
    REQUIRE_FALSE (down1->isEnabled());
    clickButton (*down0);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.fxChain[0].kind == yesdaw::engine::FxKind::Limiter);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.fxChain[0].kind
             == yesdaw::engine::FxKind::Eq);
}

TEST_CASE ("every FX param of every kind is reachable, with choosers for choice params",
           "[ui][input][shell][mixer][fx-params-all]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("fx-params-all");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });

    auto* chooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.insert.add"));
    REQUIRE (chooser != nullptr);
    const std::array<std::pair<yesdaw::engine::FxKind, int>, 5> kindsAndParamCounts {{
        { yesdaw::engine::FxKind::Eq, 24 },
        { yesdaw::engine::FxKind::Compressor, 6 },
        { yesdaw::engine::FxKind::Delay, 6 },
        { yesdaw::engine::FxKind::Reverb, 5 },
        { yesdaw::engine::FxKind::Limiter, 3 }
    }};
    for (const auto& [kind, count] : kindsAndParamCounts)
        chooser->setSelectedId (static_cast<int> (kind) + 1, juce::sendNotificationSync);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.fxChain.size() == 5u);

    auto* pager = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.param.page"));
    REQUIRE (pager != nullptr);

    // Every kind exposes its FULL param inventory across the pager's pages.
    for (std::size_t slot = 0; slot < kindsAndParamCounts.size(); ++slot)
    {
        auto* edit = dynamic_cast<juce::Button*> (findChildWithComponentId (
            *shell, "mixer.fx.slot." + juce::String (static_cast<int> (slot)) + ".edit"));
        REQUIRE (edit != nullptr);
        clickButton (*edit);

        const int pages = pager->isVisible() ? pager->getNumItems() : 1;
        int visibleParams = 0;
        for (int page = 0; page < pages; ++page)
        {
            if (page > 0)
                pager->setSelectedId (page + 1, juce::sendNotificationSync);
            for (std::size_t row = 0; row < yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount; ++row)
            {
                auto* label = findChildWithComponentId (
                    *shell, "mixer.fx.param." + juce::String (static_cast<int> (row)) + ".label");
                REQUIRE (label != nullptr);
                if (label->isVisible())
                    ++visibleParams;
            }
        }
        INFO ("slot " << slot << " expected " << kindsAndParamCounts[slot].second
              << " params, saw " << visibleParams);
        REQUIRE (visibleParams == kindsAndParamCounts[slot].second);
        clickButton (*edit);   // close before the next slot
    }

    // EQ band 0 TYPE is a real chooser: picking HPF persists the exact choice normalization.
    auto* edit0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.edit"));
    REQUIRE (edit0 != nullptr);
    clickButton (*edit0);
    auto* typeChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.param.0.choice"));
    auto* slider0 = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.fx.param.0"));
    REQUIRE (typeChooser != nullptr);
    REQUIRE (slider0 != nullptr);
    REQUIRE (typeChooser->isVisible());
    REQUIRE_FALSE (slider0->isVisible());
    REQUIRE (typeChooser->getNumItems() == 6);
    typeChooser->setSelectedId (4, juce::sendNotificationSync);   // HPF = choice index 3
    const auto paramValue = [&] (std::size_t chainSlot, std::uint32_t paramId) {
        const std::vector<std::pair<std::uint32_t, double>> params =
            readProjectSnapshot (bundlePath).tracks.front().strip.fxChain[chainSlot].normalizedParams;
        for (const auto& [id, value] : params)
            if (id == paramId)
                return value;
        FAIL ("param not persisted");
        return -1.0;
    };
    REQUIRE (paramValue (0, 0) == Catch::Approx (0.6));

    // The previously-unreachable EQ band 5 gain (id 82) is editable on the last page.
    REQUIRE (pager->isVisible());
    REQUIRE (pager->getNumItems() == 3);
    pager->setSelectedId (3, juce::sendNotificationSync);
    auto* band5Gain = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.fx.param.6"));
    REQUIRE (band5Gain != nullptr);
    REQUIRE (band5Gain->isVisible());
    band5Gain->setValue (0.75, juce::sendNotificationSync);
    REQUIRE (paramValue (0, 82) == Catch::Approx (0.75));
    clickButton (*edit0);

    // Delay ping-pong is a two-state chooser persisting exactly 1.0 for On.
    auto* edit2 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.2.edit"));
    REQUIRE (edit2 != nullptr);
    clickButton (*edit2);
    auto* pingPong = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.param.4.choice"));
    REQUIRE (pingPong != nullptr);
    REQUIRE (pingPong->isVisible());
    REQUIRE (pingPong->getNumItems() == 2);
    pingPong->setSelectedId (2, juce::sendNotificationSync);
    REQUIRE (paramValue (2, 4) == Catch::Approx (1.0));

    // Three param edits, three undos — each restores in reverse order.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    for (const auto& [id, value] : project.tracks.front().strip.fxChain[2].normalizedParams)
        REQUIRE (id != 4u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    for (const auto& [id, value] : project.tracks.front().strip.fxChain[0].normalizedParams)
        REQUIRE (id != 82u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().strip.fxChain[0].normalizedParams.empty());
}

TEST_CASE ("bus strips select and edit like real strips: undoable scalars and a working FX chain",
           "[ui][input][shell][mixer][bus-strip]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("bus-strip");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));
    REQUIRE (readProjectSnapshot (bundlePath).buses.size() == 1u);

    // Click the BUS strip (display order: track 0, then bus 0) through the strips overlay.
    juce::Component* strips = findChildWithComponentId (*shell, "shell.mixer.strips.input");
    REQUIRE (strips != nullptr);
    const int stripWidth = juce::jmax (yesdaw::ui::UiTheme::Layout::mixerStripMinWidth,
                                       strips->getWidth() / 3);   // 1 track + 1 bus + master headroom
    mouseDownAt (*strips, { stripWidth + stripWidth / 2, strips->getHeight() / 2 });

    // Honest scope: buses cannot originate sends — the send chooser refuses a bus target.
    auto* sendChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.add"));
    REQUIRE (sendChooser != nullptr);
    REQUIRE_FALSE (sendChooser->isEnabled());
    // E23: the painted mixer highlights the SELECTED strip by display ordinal — the bus strip
    // (ordinal 1 = after the single track) highlights exactly like a selected track.
    REQUIRE (snapshotMainComponent (*shell).selectedMixerStripOrdinal == 1);

    // Fader, pan, mute, solo hit the BUS strip persistently AND undoably (the new bus law).
    auto* fader = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_fader"));
    auto* pan = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_pan"));
    auto* mute = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.target.toggle_mute"));
    auto* solo = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.target.toggle_solo"));
    REQUIRE (fader != nullptr);
    REQUIRE (pan != nullptr);
    REQUIRE (mute != nullptr);
    REQUIRE (solo != nullptr);
    fader->setValue (0.5, juce::sendNotificationSync);
    pan->setValue (0.25, juce::sendNotificationSync);
    clickButton (*mute);
    clickButton (*solo);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.buses.front().strip.linearGain == 0.5f);
    REQUIRE (project.buses.front().strip.pan == 0.25f);
    REQUIRE (project.buses.front().strip.muted);
    REQUIRE (project.buses.front().strip.soloed);
    // The track strip stayed untouched — the edits really landed on the bus.
    REQUIRE (project.tracks.front().strip.linearGain == 1.0f);
    REQUIRE_FALSE (project.tracks.front().strip.muted);

    // Four bus edits, four undos, each restoring in reverse order.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE_FALSE (readProjectSnapshot (bundlePath).buses.front().strip.soloed);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE_FALSE (readProjectSnapshot (bundlePath).buses.front().strip.muted);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).buses.front().strip.pan == 0.0f);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.buses.front().strip.linearGain == 1.0f);

    // The FX chain works on the selected bus: add, bypass, param, remove — all persisted there.
    auto* fxChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.insert.add"));
    REQUIRE (fxChooser != nullptr);
    REQUIRE (fxChooser->isEnabled());
    fxChooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Eq) + 1, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.buses.front().strip.fxChain.size() == 1u);
    REQUIRE (project.tracks.front().strip.fxChain.empty());

    auto* toggle0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.toggle"));
    REQUIRE (toggle0 != nullptr);
    REQUIRE (toggle0->isVisible());
    clickButton (*toggle0);
    REQUIRE_FALSE (readProjectSnapshot (bundlePath).buses.front().strip.fxChain.front().enabled);

    auto* edit0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.edit"));
    REQUIRE (edit0 != nullptr);
    clickButton (*edit0);
    auto* gainParam = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.fx.param.2"));
    REQUIRE (gainParam != nullptr);
    REQUIRE (gainParam->isVisible());
    gainParam->setValue (0.9, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.buses.front().strip.fxChain.front().normalizedParams.size() == 1u);
    REQUIRE (project.buses.front().strip.fxChain.front().normalizedParams.front().second
             == Catch::Approx (0.9));
    clickButton (*edit0);

    auto* remove0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.fx.slot.0.remove"));
    REQUIRE (remove0 != nullptr);
    clickButton (*remove0);
    REQUIRE (readProjectSnapshot (bundlePath).buses.front().strip.fxChain.empty());
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).buses.front().strip.fxChain.size() == 1u);
}

TEST_CASE ("bus rename edits inline and bus remove honestly refuses while sends route to it",
           "[ui][input][shell][mixer][bus-rename-remove]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("bus-rename-remove");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));
    REQUIRE (readProjectSnapshot (bundlePath).buses.size() == 1u);

    // Inline rename: double-click the bus strip, type, Enter — persisted and undoable.
    juce::Component* strips = findChildWithComponentId (*shell, "shell.mixer.strips.input");
    REQUIRE (strips != nullptr);
    const int stripWidth = juce::jmax (yesdaw::ui::UiTheme::Layout::mixerStripMinWidth,
                                       strips->getWidth() / 3);
    auto* renameEditor = dynamic_cast<juce::TextEditor*> (
        findChildWithComponentId (*shell, "shell.mixer.bus.rename"));
    REQUIRE (renameEditor != nullptr);
    REQUIRE_FALSE (renameEditor->isVisible());
    doubleClickAt (*strips, { stripWidth + stripWidth / 2, strips->getHeight() / 2 });
    REQUIRE (renameEditor->isVisible());
    renameEditor->setText ("Drum Verb", juce::dontSendNotification);
    renameEditor->onReturnKey();
    REQUIRE_FALSE (renameEditor->isVisible());
    REQUIRE (readProjectSnapshot (bundlePath).buses.front().strip.name == "Drum Verb");
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).buses.front().strip.name == "Bus 1");

    // Route a send to the bus from the TRACK, then try to remove the bus: the engine refuses
    // and the bus honestly stays.
    mouseDownAt (*strips, { stripWidth / 2, strips->getHeight() / 2 });   // select the track strip
    auto* sendChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.add"));
    REQUIRE (sendChooser != nullptr);
    REQUIRE (sendChooser->isEnabled());
    sendChooser->setSelectedId (1, juce::sendNotificationSync);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().sends.size() == 1u);

    mouseDownAt (*strips, { stripWidth + stripWidth / 2, strips->getHeight() / 2 });   // select the bus
    auto* removeBus = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.bus.remove"));
    REQUIRE (removeBus != nullptr);
    REQUIRE (removeBus->isEnabled());
    clickButton (*removeBus);
    REQUIRE (readProjectSnapshot (bundlePath).buses.size() == 1u);   // refused, not silently dropped

    // Drop the routed send, then the removal goes through — and one undo restores the bus.
    mouseDownAt (*strips, { stripWidth / 2, strips->getHeight() / 2 });
    auto* sendRemove = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.send.0.remove"));
    REQUIRE (sendRemove != nullptr);
    clickButton (*sendRemove);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().sends.empty());

    mouseDownAt (*strips, { stripWidth + stripWidth / 2, strips->getHeight() / 2 });
    clickButton (*removeBus);
    REQUIRE (readProjectSnapshot (bundlePath).buses.empty());
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).buses.size() == 1u);
    REQUIRE (readProjectSnapshot (bundlePath).buses.front().strip.name == "Bus 1");
}

TEST_CASE ("send tap toggles pre/post undoably and the destination chooser re-routes as one undo",
           "[ui][input][shell][mixer][send-tap-dest]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("send-tap-dest");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));

    auto* sendChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.add"));
    REQUIRE (sendChooser != nullptr);
    sendChooser->setSelectedId (1, juce::sendNotificationSync);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.size() == 1u);
    REQUIRE (project.tracks.front().sends.front().tap == yesdaw::engine::SendTap::PostFader);
    auto* level0 = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.send.0"));
    REQUIRE (level0 != nullptr);
    level0->setValue (0.5, juce::sendNotificationSync);

    // With one bus the destination chooser has nowhere else to route — disabled.
    auto* dest0 = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.0.dest"));
    REQUIRE (dest0 != nullptr);
    REQUIRE (dest0->isVisible());
    REQUIRE_FALSE (dest0->isEnabled());

    // The tap toggle flips Post -> Pre undoably; the button names the CURRENT tap.
    auto* tap0 = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "mixer.send.0.tap"));
    REQUIRE (tap0 != nullptr);
    REQUIRE (tap0->isVisible());
    REQUIRE (tap0->getButtonText() == "Post");
    clickButton (*tap0);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.front().tap == yesdaw::engine::SendTap::PreFader);
    REQUIRE (tap0->getButtonText() == "Pre");
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().sends.front().tap
             == yesdaw::engine::SendTap::PostFader);

    // A second bus enables re-routing; the chooser re-routes preserving id, tap, and level,
    // and ONE Ctrl+Z restores the original route (remove+add rode one undo group).
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.buses.size() == 2u);
    const yesdaw::engine::EntityId firstBusId = project.buses[0].id;
    const yesdaw::engine::EntityId secondBusId = project.buses[1].id;
    const yesdaw::engine::EntityId sendId = project.tracks.front().sends.front().id;
    REQUIRE (dest0->isEnabled());
    REQUIRE (dest0->getNumItems() == 2);
    dest0->setSelectedId (2, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.size() == 1u);
    REQUIRE (project.tracks.front().sends.front().busId == secondBusId);
    REQUIRE (project.tracks.front().sends.front().id == sendId);
    REQUIRE (project.tracks.front().sends.front().tap == yesdaw::engine::SendTap::PostFader);
    REQUIRE (project.tracks.front().sends.front().linearGain == 0.5f);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.size() == 1u);
    REQUIRE (project.tracks.front().sends.front().busId == firstBusId);
    REQUIRE (project.tracks.front().sends.front().linearGain == 0.5f);
}

TEST_CASE ("the master fader persists an undoable master gain that scales the whole mix",
           "[ui][input][shell][mixer][master-fader]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("master-fader");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    auto* masterFader = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.master.fader"));
    REQUIRE (masterFader != nullptr);
    REQUIRE (masterFader->isEnabled());
    REQUIRE (masterFader->getValue() == 1.0);

    const auto renderFromStart = [&shell] {
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
        const std::vector<float> rendered = renderMainComponentPlayback (*shell, 48'000, 128);
        REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
        return rendered;
    };
    const std::vector<float> atUnity = renderFromStart();
    const double unityPeak = peakAbs (std::span<const float> (atUnity.data(), atUnity.size()));
    REQUIRE (unityPeak > 0.05);

    // Half gain persists (readProjectSnapshot opens the bundle independently — the round trip)
    // and audibly halves the mix exactly.
    masterFader->setValue (0.5, juce::sendNotificationSync);
    REQUIRE (readProjectSnapshot (bundlePath).masterLinearGain == 0.5f);
    const std::vector<float> atHalf = renderFromStart();
    REQUIRE (peakAbs (std::span<const float> (atHalf.data(), atHalf.size()))
             == Catch::Approx (unityPeak * 0.5));
    REQUIRE (atHalf != atUnity);

    // One undo restores the persisted unity default AND bit-identical audio.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).masterLinearGain == 1.0f);
    const std::vector<float> restored = renderFromStart();
    REQUIRE (restored == atUnity);
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

    // E5 wheel map: Shift+wheel scrolls horizontally (the plain wheel scrolls track rows, which
    // clamps to a no-op on this single-track project); scroll clamps to zero at the left edge
    // after zooming back out.
    juce::MouseWheelDetails wheelDown {};
    wheelDown.deltaY = -0.4f;
    juce::MouseEvent shiftWheel = makeMouseEvent (timeline, centre, centre, false, 1,
                                                  juce::ModifierKeys (juce::ModifierKeys::shiftModifier));
    timeline.mouseWheelMove (shiftWheel, wheelDown);
    snapshot = snapshotMainComponent (*shell);
    const double scrolled = snapshot.timelineScrollSeconds;
    REQUIRE (scrolled >= 0.0);
    juce::MouseEvent plainWheel = makeMouseEvent (timeline, centre, centre, false, 1, juce::ModifierKeys {});
    timeline.mouseWheelMove (plainWheel, wheelDown);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.timelineTrackScrollRows == 0);
    REQUIRE (snapshot.timelineScrollSeconds == scrolled);

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
    const juce::MouseEvent shiftWheel = makeMouseEvent (
        timeline, centre, centre, false, 1,
        juce::ModifierKeys (juce::ModifierKeys::shiftModifier));
    timeline.mouseWheelMove (shiftWheel, wheelDown);

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
    const juce::MouseEvent shiftWheel = makeMouseEvent (
        timeline, centre, centre, false, 1,
        juce::ModifierKeys (juce::ModifierKeys::shiftModifier));
    timeline.mouseWheelMove (shiftWheel, wheelDown);
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

    // E4: trim edges snap through the snap chooser; the default grid is far coarser than this
    // short fixture, so Ctrl inverts to keep the raw quarter-clip trim.
    const juce::Point<int> leftEdge = timelineClipLeftEdgeDragPoint (timeline, imported, 0u);
    const int quarterClipPixels = juce::jmax (
        yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels + 2,
        juce::roundToInt (timelinePixelsPerSecond (timeline, imported)
                          * (static_cast<double> (before.timelineLength) / 48'000.0) * 0.25));
    dragFromTo (timeline, leftEdge, { leftEdge.x + quarterClipPixels, leftEdge.y },
                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                    | juce::ModifierKeys::ctrlModifier));

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

TEST_CASE ("three-track arrangement is first-class at the shipped boundary",
           "[ui][input][shell][timeline][three-track]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("three-track");
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
    clickButton (*addTrack);

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (rail->getHeight() - headerHeight) / 3);
    const auto railRowCenter = [&] (int row) {
        return juce::Point<int> { rail->getWidth() / 2, headerHeight + row * rowHeight + rowHeight / 2 };
    };

    // Import lands on the SELECTED middle track at the playhead (zero), then on the SELECTED third
    // track at a nonzero located playhead.
    mouseDownAt (*rail, railRowCenter (1));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    mouseDownAt (*rail, railRowCenter (2));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    const auto locatedFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (locatedFrame > 0);
    const yesdaw::engine::Tick gridTick = static_cast<yesdaw::engine::Tick> (locatedFrame);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.size() == 3u);
    REQUIRE (project.clips.size() == 3u);
    REQUIRE (project.clips[0].trackId == project.tracks[0].id);
    REQUIRE (project.clips[0].timelineStart == 0);
    REQUIRE (project.clips[1].trackId == project.tracks[1].id);
    REQUIRE (project.clips[1].timelineStart == 0);
    REQUIRE (project.clips[2].trackId == project.tracks[2].id);
    REQUIRE (project.clips[2].timelineStart == gridTick);

    const std::uint64_t renderFrames = static_cast<std::uint64_t> (
        2 * gridTick + project.clips[2].timelineLength + 256);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeEdits = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeEdits.data(), beforeEdits.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    // A pointer marquee spans all three lanes.
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    juce::Component& timeline = requireTimelineComponent (*shell);
    const juce::Rectangle<int> topBounds = timelineClipHitBounds (timeline, project, 0u);
    const juce::Rectangle<int> midBounds = timelineClipHitBounds (timeline, project, 1u);
    const juce::Rectangle<int> lowBounds = timelineClipHitBounds (timeline, project, 2u);
    const juce::Point<int> emptyStart {
        juce::jmin (lowBounds.getRight() + 2 * yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels,
                    timeline.getWidth() - 2),
        lowBounds.getCentreY()
    };
    dragFromTo (timeline, emptyStart, { topBounds.getX(), topBounds.getY() });
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 3);

    // A vertical drag of the middle-lane member clamps: the selection already spans every lane, so
    // the group must not change tracks or times in either direction.
    const yesdaw::engine::Project beforeClamp = readProjectSnapshot (bundlePath);
    dragFromTo (timeline, midBounds.getCentre(), { midBounds.getCentreX(), lowBounds.getCentreY() });
    REQUIRE (readProjectSnapshot (bundlePath).clips == beforeClamp.clips);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 3);
    dragFromTo (timeline, midBounds.getCentre(), { midBounds.getCentreX(), topBounds.getCentreY() });
    REQUIRE (readProjectSnapshot (bundlePath).clips == beforeClamp.clips);

    // A two-clip selection on the top two lanes moves down one lane THROUGH the middle lane as one
    // persisted undo step, preserving relative track offsets and times. A plain click on a selected
    // member keeps the group (the drag law), so clear via a real empty click first.
    mouseDownAt (timeline, emptyStart);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 0);
    mouseDownAt (timeline, topBounds.getCentre());
    mouseDownAt (timeline, midBounds.getCentre(),
                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                     | juce::ModifierKeys::shiftModifier));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);
    const yesdaw::engine::EntityId topClipId = beforeClamp.clips[0].id;
    const yesdaw::engine::EntityId midClipId = beforeClamp.clips[1].id;
    dragFromTo (timeline, topBounds.getCentre(), { topBounds.getCentreX(), midBounds.getCentreY() });
    const yesdaw::engine::Project movedDown = readProjectSnapshot (bundlePath);
    const auto clipById = [] (const yesdaw::engine::Project& snapshot, yesdaw::engine::EntityId clipId)
        -> const yesdaw::engine::Clip* {
        const auto match = std::find_if (snapshot.clips.begin(), snapshot.clips.end(), [clipId] (const auto& clip) {
            return clip.id == clipId;
        });
        return match == snapshot.clips.end() ? nullptr : &(*match);
    };
    const yesdaw::engine::Clip* const movedTop = clipById (movedDown, topClipId);
    const yesdaw::engine::Clip* const movedMid = clipById (movedDown, midClipId);
    REQUIRE (movedTop != nullptr);
    REQUIRE (movedMid != nullptr);
    REQUIRE (movedTop->trackId == movedDown.tracks[1].id);
    REQUIRE (movedMid->trackId == movedDown.tracks[2].id);
    REQUIRE (movedTop->timelineStart == beforeClamp.clips[0].timelineStart);
    REQUIRE (movedMid->timelineStart == beforeClamp.clips[1].timelineStart);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == beforeClamp.clips);

    // Project-wide copy/paste at a located playhead preserves each clip's track and relative time.
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 3);
    REQUIRE (shell->keyPressed (juce::KeyPress ('c', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    const auto pasteFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (pasteFrame > 0);
    const yesdaw::engine::Tick pasteTick = static_cast<yesdaw::engine::Tick> (pasteFrame);
    REQUIRE (shell->keyPressed (juce::KeyPress ('v', juce::ModifierKeys::ctrlModifier, 0)));

    const yesdaw::engine::Project pasted = readProjectSnapshot (bundlePath);
    REQUIRE (pasted.clips.size() == 6u);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 3);
    for (const yesdaw::engine::Clip& original : beforeClamp.clips)
    {
        const auto copy = std::find_if (pasted.clips.begin(), pasted.clips.end(), [&] (const auto& clip) {
            return clip.id != original.id
                && clip.trackId == original.trackId
                && clip.timelineStart == pasteTick + original.timelineStart
                && clip.timelineLength == original.timelineLength
                && clip.assetId == original.assetId;
        });
        REQUIRE (copy != pasted.clips.end());
    }

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterPaste = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterPaste != beforeEdits);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == beforeClamp.clips);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterUndo = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterUndo == beforeEdits);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
}

TEST_CASE ("group duplicate and group copy-drag preserve the whole selection's offsets",
           "[ui][input][shell][timeline][group-duplicate]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("group-duplicate");
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
    clickButton (*addTrack);

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (rail->getHeight() - headerHeight) / 3);

    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + rowHeight + rowHeight / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + 2 * rowHeight + rowHeight / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    const auto locatedFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (locatedFrame > 0);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.tracks.size() == 3u);
    REQUIRE (original.clips.size() == 3u);
    yesdaw::engine::Tick selectionSpan = 0;
    for (const yesdaw::engine::Clip& clip : original.clips)
        selectionSpan = std::max (selectionSpan, clip.timelineStart + clip.timelineLength);

    const std::uint64_t renderFrames = static_cast<std::uint64_t> (
        2 * selectionSpan + original.clips[2].timelineLength + 256);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeEdits = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeEdits.data(), beforeEdits.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    const auto matchingCopy = [] (const yesdaw::engine::Project& snapshot,
                                  const yesdaw::engine::Clip& source,
                                  yesdaw::engine::EntityId sourceTrackId,
                                  yesdaw::engine::Tick expectedStart) {
        return std::find_if (snapshot.clips.begin(), snapshot.clips.end(), [&] (const auto& clip) {
            return clip.id != source.id
                && clip.trackId == sourceTrackId
                && clip.timelineStart == expectedStart
                && clip.timelineLength == source.timelineLength
                && clip.assetId == source.assetId
                && clip.srcOffset == source.srcOffset
                && clip.srcLen == source.srcLen
                && clip.gain == source.gain
                && clip.fadeIn == source.fadeIn
                && clip.fadeOut == source.fadeOut;
        }) != snapshot.clips.end();
    };

    // Ctrl+D duplicates the WHOLE selection after its span, preserving relative time and track.
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 3);
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));

    const yesdaw::engine::Project duplicated = readProjectSnapshot (bundlePath);
    REQUIRE (duplicated.clips.size() == 6u);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 3);
    for (const yesdaw::engine::Clip& source : original.clips)
        REQUIRE (matchingCopy (duplicated, source, source.trackId,
                               selectionSpan + source.timelineStart));

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterDuplicate = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterDuplicate != beforeEdits);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    // A center Alt+drag on a selected member copies the WHOLE selection by the anchor's exact
    // delta — here one lane down with zero time delta — leaving every original untouched.
    juce::Component& timeline = requireTimelineComponent (*shell);
    const juce::Rectangle<int> topBounds = timelineClipHitBounds (timeline, original, 0u);
    const juce::Rectangle<int> midBounds = timelineClipHitBounds (timeline, original, 1u);
    mouseDownAt (timeline, topBounds.getCentre());
    mouseDownAt (timeline, midBounds.getCentre(),
                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                     | juce::ModifierKeys::shiftModifier));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);
    dragFromTo (timeline, topBounds.getCentre(), { topBounds.getCentreX(), midBounds.getCentreY() },
                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                    | juce::ModifierKeys::altModifier));
    const yesdaw::engine::Project copied = readProjectSnapshot (bundlePath);
    REQUIRE (copied.clips.size() == 5u);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);
    for (const yesdaw::engine::Clip& source : original.clips)
    {
        const auto match = std::find_if (copied.clips.begin(), copied.clips.end(), [&source] (const auto& clip) {
            return clip.id == source.id;
        });
        REQUIRE (match != copied.clips.end());
        REQUIRE (*match == source);
    }
    REQUIRE (matchingCopy (copied, original.clips[0], copied.tracks[1].id,
                           original.clips[0].timelineStart));
    REQUIRE (matchingCopy (copied, original.clips[1], copied.tracks[2].id,
                           original.clips[1].timelineStart));

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    // The historical single-clip duplicate law is unchanged: the copy lands directly after the
    // source clip on the same track.
    const juce::Point<int> emptyStart {
        juce::jmin (timelineClipHitBounds (timeline, original, 2u).getRight()
                        + 2 * yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels,
                    timeline.getWidth() - 2),
        timelineClipHitBounds (timeline, original, 2u).getCentreY()
    };
    mouseDownAt (timeline, emptyStart);
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 0);
    mouseDownAt (timeline, topBounds.getCentre());
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    const yesdaw::engine::Project single = readProjectSnapshot (bundlePath);
    REQUIRE (single.clips.size() == 4u);
    REQUIRE (matchingCopy (single, original.clips[0], original.clips[0].trackId,
                           original.clips[0].timelineStart + original.clips[0].timelineLength));
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterUndo = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterUndo == beforeEdits);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
}

TEST_CASE ("the tool palette drives real timeline behavior per tool",
           "[ui][input][shell][timeline][tool-palette]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("tool-palette");
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
    clickButton (*addTrack);

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (rail->getHeight() - headerHeight) / 3);
    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + rowHeight + rowHeight / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + 2 * rowHeight + rowHeight / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    const auto locatedFrame = snapshotMainComponent (*shell).context.playheadFrame;
    REQUIRE (locatedFrame > 0);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));

    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.tracks.size() == 3u);
    REQUIRE (original.clips.size() == 3u);
    REQUIRE (original.midiClips.empty());
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    juce::Component& timeline = requireTimelineComponent (*shell);
    const juce::Rectangle<int> topBounds = timelineClipHitBounds (timeline, original, 0u);
    const juce::Rectangle<int> midBounds = timelineClipHitBounds (timeline, original, 1u);

    // The exact pixel->seconds law of the timeline input component, rebuilt from the snapshot.
    const auto viewportAt = [&] (const MainComponentSnapshot& snapshot) {
        yesdaw::ui::TimelineCanvasState state;
        state.trackCount = static_cast<int> (original.tracks.size());
        state.totalSeconds = snapshot.visibleTimelineTotalSeconds;
        const double fitPixelsPerSecond = static_cast<double> (juce::jmax (
                                                  yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                                  timeline.getWidth()
                                                      - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                        / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                    state.totalSeconds);
        state.viewport.pixelsPerSecond = fitPixelsPerSecond * snapshot.timelineZoomFactor;
        state.viewport.scrollSeconds = snapshot.timelineScrollSeconds;
        return yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    };
    const auto secondsAtX = [&] (const MainComponentSnapshot& snapshot, int x) {
        const yesdaw::ui::TimelineCanvasGeometry geometry = viewportAt (snapshot);
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        return geometry.viewport.scrollSeconds
             + static_cast<double> (x - geometry.clipArea.getX()) / pixelsPerSecond;
    };

    // ZOOM tool: a click doubles the zoom around the click, Alt+click halves it; transient only.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Zoom);
    const juce::Point<int> zoomPoint { topBounds.getCentreX(), topBounds.getCentreY() };
    const MainComponentSnapshot zoomBase = snapshotMainComponent (*shell);
    REQUIRE (zoomBase.timelineZoomFactor == 1.0);
    const double anchorSeconds = secondsAtX (zoomBase, zoomPoint.x);
    mouseDownAt (timeline, zoomPoint);
    const MainComponentSnapshot zoomOnce = snapshotMainComponent (*shell);
    REQUIRE (zoomOnce.timelineZoomFactor
             == Catch::Approx (yesdaw::ui::UiTheme::Layout::timelineZoomToolClickFactor));
    REQUIRE (zoomOnce.timelineScrollSeconds
             == Catch::Approx (anchorSeconds
                               * (1.0 - 1.0 / yesdaw::ui::UiTheme::Layout::timelineZoomToolClickFactor)));
    mouseDownAt (timeline, zoomPoint);
    REQUIRE (snapshotMainComponent (*shell).timelineZoomFactor == Catch::Approx (4.0));
    mouseDownAt (timeline, zoomPoint,
                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier));
    REQUIRE (snapshotMainComponent (*shell).timelineZoomFactor == Catch::Approx (2.0));
    mouseDownAt (timeline, zoomPoint,
                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier));
    const MainComponentSnapshot zoomReset = snapshotMainComponent (*shell);
    REQUIRE (zoomReset.timelineZoomFactor == 1.0);
    REQUIRE (zoomReset.timelineScrollSeconds == yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    // HAND tool: a drag that STARTS ON A CLIP pans the viewport by the exact pixel delta and never
    // moves the clip; the reverse drag lands back at exactly zero scroll.
    REQUIRE (shell->keyPressed (juce::KeyPress ('h')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Hand);
    const MainComponentSnapshot handBase = snapshotMainComponent (*shell);
    const yesdaw::ui::TimelineCanvasGeometry handGeometry = viewportAt (handBase);
    const double handPixelsPerSecond = std::max (
        yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
        handGeometry.viewport.pixelsPerSecond);
    const int handDragPixels = 120;
    dragFromTo (timeline, topBounds.getCentre(),
                { topBounds.getCentreX() - handDragPixels, topBounds.getCentreY() });
    const MainComponentSnapshot handMoved = snapshotMainComponent (*shell);
    REQUIRE (handMoved.timelineScrollSeconds
             == Catch::Approx (handBase.timelineScrollSeconds
                               + static_cast<double> (handDragPixels) / handPixelsPerSecond));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    dragFromTo (timeline, { topBounds.getCentreX() - handDragPixels, topBounds.getCentreY() },
                topBounds.getCentre());
    REQUIRE (snapshotMainComponent (*shell).timelineScrollSeconds == 0.0);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    // SCISSORS tool: with the snap chooser on and the grid coarser than this short clip, the
    // snapped split tick lands outside the clip body and is honestly refused (E4); Ctrl inverts
    // the grid, and the raw click splits as a persisted undoable edit.
    REQUIRE (shell->keyPressed (juce::KeyPress ('s')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Scissors);
    REQUIRE (snapshotMainComponent (*shell).context.snapEnabled);
    REQUIRE (snapshotMainComponent (*shell).context.snapGridTicks > original.clips[0].timelineLength);
    mouseDownAt (timeline, { topBounds.getX() + (topBounds.getWidth() * 3) / 5, topBounds.getCentreY() });
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 3u);
    mouseDownAt (timeline, { topBounds.getX() + (topBounds.getWidth() * 3) / 5, topBounds.getCentreY() },
                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier));
    const yesdaw::engine::Project split = readProjectSnapshot (bundlePath);
    REQUIRE (split.clips.size() == 4u);
    const auto splitLeft = std::find_if (split.clips.begin(), split.clips.end(), [&] (const auto& clip) {
        return clip.id == original.clips[0].id;
    });
    REQUIRE (splitLeft != split.clips.end());
    REQUIRE (splitLeft->timelineStart == original.clips[0].timelineStart);
    REQUIRE (splitLeft->timelineLength > 0);
    REQUIRE (splitLeft->timelineLength < original.clips[0].timelineLength);
    const auto splitRight = std::find_if (split.clips.begin(), split.clips.end(), [&] (const auto& clip) {
        return clip.id != original.clips[0].id
            && clip.trackId == original.clips[0].trackId
            && clip.timelineStart == splitLeft->timelineStart + splitLeft->timelineLength;
    });
    REQUIRE (splitRight != split.clips.end());
    REQUIRE (splitLeft->timelineLength + splitRight->timelineLength == original.clips[0].timelineLength);
    REQUIRE (splitRight->srcOffset == splitLeft->srcOffset + splitLeft->srcLen);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    // PENCIL tool: a click on a clip only selects it; a click on an empty lane creates a snapped
    // one-bar MIDI clip on THAT lane through the Ctrl+M law and opens the piano roll.
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Pencil);
    const std::vector<std::uint8_t> persistedAfterSplitUndo = readBytes (bundlePath / "project.db");
    mouseDownAt (timeline, midBounds.getCentre());
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedAfterSplitUndo);

    const yesdaw::engine::Tick emptyLaneTick =
        original.clips[1].timelineLength + static_cast<yesdaw::engine::Tick> (locatedFrame) / 2;
    const MainComponentSnapshot pencilBase = snapshotMainComponent (*shell);
    const juce::Point<int> pencilPoint {
        projectRulerPointAtTick (timeline, pencilBase, original, emptyLaneTick).x,
        midBounds.getCentreY()
    };
    const double pencilSeconds = secondsAtX (pencilBase, pencilPoint.x);
    const auto pencilRawTick = static_cast<yesdaw::engine::Tick> (
        std::llround (pencilSeconds * original.sampleRate.hz));
    yesdaw::engine::Tick pencilExpectedTick = pencilRawTick;
    REQUIRE (pencilBase.context.snapEnabled);
    REQUIRE (pencilBase.context.snapGridTicks > 0);
    REQUIRE (yesdaw::engine::snapTick (pencilRawTick,
                                       yesdaw::engine::SnapGrid { pencilBase.context.snapGridTicks },
                                       pencilExpectedTick));
    mouseDownAt (timeline, pencilPoint);
    const yesdaw::engine::Project penciled = readProjectSnapshot (bundlePath);
    REQUIRE (penciled.midiClips.size() == 1u);
    REQUIRE (penciled.midiClips.front().trackId == original.tracks[1].id);
    REQUIRE (penciled.midiClips.front().timelineStart == pencilExpectedTick);
    const double barSeconds = 60.0 / penciled.tempoMap.front().bpm
                            * static_cast<double> (penciled.meterMap.front().numerator);
    REQUIRE (penciled.midiClips.front().timelineLength
             == static_cast<yesdaw::engine::Tick> (barSeconds * penciled.sampleRate.hz + 0.5));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);

    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::Timeline);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.empty());

    // POINTER law preserved: the plain move drag still persists.
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    REQUIRE (snapshotMainComponent (*shell).context.activeTimelineTool == yesdaw::ui::TimelineTool::Pointer);
    dragFromTo (timeline, topBounds.getCentre(),
                { topBounds.getCentreX() + timeline.getWidth() / 4, topBounds.getCentreY() });
    const yesdaw::engine::Project moved = readProjectSnapshot (bundlePath);
    const auto movedClip = std::find_if (moved.clips.begin(), moved.clips.end(), [&] (const auto& clip) {
        return clip.id == original.clips[0].id;
    });
    REQUIRE (movedClip != moved.clips.end());
    REQUIRE (movedClip->timelineStart > original.clips[0].timelineStart);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
}

TEST_CASE ("every timeline time-gesture consults the snap chooser with Ctrl inversion",
           "[ui][input][shell][timeline][snap-gestures]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("snap-gestures");
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
    const yesdaw::engine::Clip baseClip = original.clips.front();
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    const MainComponentSnapshot base = snapshotMainComponent (*shell);
    REQUIRE (base.context.snapEnabled);
    const std::int64_t grid = base.context.snapGridTicks;
    // The default beat grid is far coarser than the short fixture clip: snapped edge/split ticks
    // land outside the clip's legality window, so the verbs refuse honestly (legality wins).
    REQUIRE (grid > baseClip.timelineLength);

    const auto secondsAtX = [&] (int x) {
        const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
        yesdaw::ui::TimelineCanvasState state;
        state.trackCount = static_cast<int> (original.tracks.size());
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
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        return geometry.viewport.scrollSeconds
             + static_cast<double> (x - geometry.clipArea.getX()) / pixelsPerSecond;
    };
    const auto rawTickAtX = [&] (int x) {
        return static_cast<yesdaw::engine::Tick> (
            std::llround (secondsAtX (x) * original.sampleRate.hz));
    };
    const auto snappedTickAtX = [&] (int x) {
        yesdaw::engine::Tick snapped = rawTickAtX (x);
        REQUIRE (yesdaw::engine::snapTick (rawTickAtX (x),
                                           yesdaw::engine::SnapGrid { grid }, snapped));
        return std::max<yesdaw::engine::Tick> (0, snapped);
    };
    const auto xAtTick = [&] (yesdaw::engine::Tick tick) {
        return projectRulerPointAtTick (timeline, snapshotMainComponent (*shell), original, tick).x;
    };
    const int rulerY = projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), original, 0).y;
    const juce::ModifierKeys ctrlDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier);
    const juce::ModifierKeys shiftDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
    const juce::ModifierKeys ctrlShiftDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier
        | juce::ModifierKeys::shiftModifier);

    // LOOP drag on the ruler: endpoints snap to the beat grid; Ctrl inverts to raw.
    const int loopFromX = xAtTick (grid * 2 / 5);
    const int loopToX = xAtTick (grid * 8 / 5);
    dragFromTo (timeline, { loopFromX, rulerY }, { loopToX, rulerY }, shiftDrag);
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.loopEnabled);
    REQUIRE (snapshot.playbackLoopStartFrame == snappedTickAtX (loopFromX));
    REQUIRE (snapshot.playbackLoopEndFrame == snappedTickAtX (loopToX));
    REQUIRE (snapshot.playbackLoopStartFrame % grid == 0);
    REQUIRE (snapshot.playbackLoopEndFrame % grid == 0);

    const int rawFromX = xAtTick (grid / 2);
    const int rawToX = xAtTick (grid * 5 / 4);
    dragFromTo (timeline, { rawFromX, rulerY }, { rawToX, rulerY }, ctrlShiftDrag);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.playbackLoopStartFrame == rawTickAtX (rawFromX));
    REQUIRE (snapshot.playbackLoopEndFrame == rawTickAtX (rawToX));
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    // RANGE drag on the ruler: endpoints snap while the mouse-down locate stays raw.
    dragFromTo (timeline, { loopFromX, rulerY }, { loopToX, rulerY });
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.timelineRangeSelected);
    REQUIRE (snapshot.timelineRangeStartFrame == snappedTickAtX (loopFromX));
    REQUIRE (snapshot.timelineRangeEndFrame == snappedTickAtX (loopToX));
    REQUIRE (snapshot.context.playheadFrame == rawTickAtX (loopFromX));
    dragFromTo (timeline, { rawFromX, rulerY }, { rawToX, rulerY }, ctrlDrag);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.timelineRangeStartFrame == rawTickAtX (rawFromX));
    REQUIRE (snapshot.timelineRangeEndFrame == rawTickAtX (rawToX));
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    // TRIM RIGHT: snap-on lands the snapped end outside the legal window -> honest refusal;
    // Ctrl inverts and the raw target trims exactly.
    const juce::Point<int> rightEdge = timelineClipRightEdgeDragPoint (timeline, original, 0u);
    const int trimTargetX = xAtTick (baseClip.timelineLength * 3 / 4);
    dragFromTo (timeline, rightEdge, { trimTargetX, rightEdge.y });
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    dragFromTo (timeline, rightEdge, { trimTargetX, rightEdge.y }, ctrlDrag);
    {
        const yesdaw::engine::Clip trimmed = readProjectSnapshot (bundlePath).clips.front();
        REQUIRE (trimmed.timelineLength == rawTickAtX (trimTargetX) - baseClip.timelineStart);
        REQUIRE (trimmed.timelineStart == baseClip.timelineStart);
    }
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    // TRIM LEFT: same law on the head.
    const juce::Point<int> leftEdge = timelineClipLeftEdgeDragPoint (timeline, original, 0u);
    const int trimLeftTargetX = xAtTick (baseClip.timelineLength / 4);
    dragFromTo (timeline, leftEdge, { trimLeftTargetX, leftEdge.y });
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
    dragFromTo (timeline, leftEdge, { trimLeftTargetX, leftEdge.y }, ctrlDrag);
    {
        const yesdaw::engine::Clip trimmed = readProjectSnapshot (bundlePath).clips.front();
        const yesdaw::engine::Tick consumed = rawTickAtX (trimLeftTargetX) - baseClip.timelineStart;
        REQUIRE (trimmed.timelineStart == baseClip.timelineStart + consumed);
        REQUIRE (trimmed.timelineLength == baseClip.timelineLength - consumed);
        REQUIRE (trimmed.srcOffset > baseClip.srcOffset);
        REQUIRE (trimmed.timelineStart + trimmed.timelineLength
                 == baseClip.timelineStart + baseClip.timelineLength);
    }
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    // SPLIT: the snapped double-click tick lands outside the clip body -> honest refusal; the
    // Ctrl-inverted double-click splits at the exact raw tick.
    const juce::Rectangle<int> clipBounds = timelineClipHitBounds (timeline, original, 0u);
    doubleClickAt (timeline, clipBounds.getCentre());
    REQUIRE (readProjectSnapshot (bundlePath).clips.size() == 1u);
    doubleClickAt (timeline, clipBounds.getCentre(), ctrlDrag);
    {
        const yesdaw::engine::Project split = readProjectSnapshot (bundlePath);
        REQUIRE (split.clips.size() == 2u);
        const yesdaw::engine::Tick splitTick = rawTickAtX (clipBounds.getCentreX());
        const auto left = std::find_if (split.clips.begin(), split.clips.end(), [&] (const auto& clip) {
            return clip.id == baseClip.id;
        });
        REQUIRE (left != split.clips.end());
        REQUIRE (left->timelineLength == splitTick - baseClip.timelineStart);
        const auto right = std::find_if (split.clips.begin(), split.clips.end(), [&] (const auto& clip) {
            return clip.id != baseClip.id && clip.timelineStart == splitTick;
        });
        REQUIRE (right != split.clips.end());
        REQUIRE (left->timelineLength + right->timelineLength == baseClip.timelineLength);
    }
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);

    // FADES stay honestly unsnapped: with snap ON, an Alt edge drag persists a raw, sub-grid fade
    // (a snapped duration would have collapsed to zero and changed nothing).
    const juce::ModifierKeys altDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier);
    const juce::Point<int> fadeOutStart = timelineClipRightEdgeDragPoint (timeline, original, 0u);
    dragFromTo (timeline, fadeOutStart, { trimTargetX, fadeOutStart.y }, altDrag);
    {
        const yesdaw::engine::Clip faded = readProjectSnapshot (bundlePath).clips.front();
        REQUIRE (faded.fadeOut > 0);
        REQUIRE (faded.fadeOut < baseClip.timelineLength);
        REQUIRE (faded.fadeOut % grid != 0);
    }
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == original.clips);
}

TEST_CASE ("vertical track scroll reaches and edits the last track of a deep project",
           "[ui][input][shell][timeline][vertical-scroll]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("vertical-scroll");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    auto* addTrack = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "track.add"));
    REQUIRE (addTrack != nullptr);
    constexpr int kTrackCount = 18;
    for (int i = 1; i < kTrackCount; ++i)
        clickButton (*addTrack);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == static_cast<std::size_t> (kTrackCount));

    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");
    juce::Component& timeline = requireTimelineComponent (*shell);
    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);

    const MainComponentSnapshot base = snapshotMainComponent (*shell);
    const int maxRows = base.timelineMaxTrackScrollRows;
    REQUIRE (maxRows > 0);
    REQUIRE (base.timelineTrackScrollRows == 0);

    // The timeline lane law: fixed rows once the viewport overflows. Replicate the geometry to
    // prove the last lane is OFF-SCREEN before scrolling and exactly reachable after.
    yesdaw::ui::TimelineCanvasState laneState;
    laneState.trackCount = kTrackCount;
    const yesdaw::ui::TimelineCanvasGeometry unscrolled =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), laneState);
    REQUIRE (unscrolled.laneHeight >= yesdaw::ui::UiTheme::Layout::timelineCanvasLaneRowHeight);
    REQUIRE (unscrolled.clipArea.getY() + kTrackCount * unscrolled.laneHeight
             > unscrolled.clipArea.getBottom());

    // Plain wheel scrolls vertically and clamps at both ends; the project file never changes.
    juce::MouseWheelDetails wheelDown {};
    wheelDown.deltaY = -0.4f;
    juce::MouseWheelDetails wheelUp {};
    wheelUp.deltaY = 0.4f;
    const juce::Point<int> centre { timeline.getWidth() / 2, timeline.getHeight() / 2 };
    const juce::MouseEvent plainWheel = makeMouseEvent (timeline, centre, centre, false, 1, juce::ModifierKeys {});
    for (int i = 0; i < maxRows + 3; ++i)
        timeline.mouseWheelMove (plainWheel, wheelDown);
    REQUIRE (snapshotMainComponent (*shell).timelineTrackScrollRows == maxRows);
    for (int i = 0; i < maxRows + 3; ++i)
        timeline.mouseWheelMove (plainWheel, wheelUp);
    REQUIRE (snapshotMainComponent (*shell).timelineTrackScrollRows == 0);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    // The rail shares the offset: wheel the RAIL to the bottom, click the last row, and the next
    // import lands on the LAST track at the playhead.
    const juce::MouseEvent railWheel = makeMouseEvent (
        *rail, { rail->getWidth() / 2, rail->getHeight() / 2 },
        { rail->getWidth() / 2, rail->getHeight() / 2 }, false, 1, juce::ModifierKeys {});
    for (int i = 0; i < maxRows + 3; ++i)
        rail->mouseWheelMove (railWheel, wheelDown);
    REQUIRE (snapshotMainComponent (*shell).timelineTrackScrollRows == maxRows);

    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int railRowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                          (rail->getHeight() - headerHeight) / kTrackCount);
    const int railVisibleRows = std::max (1, (rail->getHeight() - headerHeight) / railRowHeight);
    const int railMaxRows = std::max (0, kTrackCount - railVisibleRows);
    const int railEffectiveRows = std::min (maxRows, railMaxRows);
    const int lastRowY = headerHeight + (kTrackCount - 1 - railEffectiveRows) * railRowHeight
                       + railRowHeight / 2;
    REQUIRE (lastRowY < rail->getHeight());
    mouseDownAt (*rail, { rail->getWidth() / 2, lastRowY });
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    const yesdaw::engine::Project imported = readProjectSnapshot (bundlePath);
    REQUIRE (imported.clips.size() == 2u);
    REQUIRE (imported.clips[1].trackId == imported.tracks[kTrackCount - 1].id);
    REQUIRE (imported.clips[1].timelineStart == 0);

    // The scrolled timeline hit-tests the last lane: drag its clip and the move persists snapped.
    const int timelineEffectiveRows = std::min (
        snapshotMainComponent (*shell).timelineTrackScrollRows, unscrolled.maxTrackScrollRows);
    const int lastLaneY = unscrolled.clipArea.getY()
                        + (kTrackCount - 1 - timelineEffectiveRows) * unscrolled.laneHeight
                        + unscrolled.laneHeight / 2;
    REQUIRE (lastLaneY < unscrolled.clipArea.getBottom());
    // The clip's horizontal extent is untouched by vertical scroll; only its lane row moved.
    const juce::Point<int> lastClipPoint {
        timelineClipHitBounds (timeline, imported, 1u).getCentreX(), lastLaneY
    };
    dragFromTo (timeline, lastClipPoint,
                { lastClipPoint.x + timeline.getWidth() / 3, lastClipPoint.y });
    const yesdaw::engine::Project moved = readProjectSnapshot (bundlePath);
    const auto movedClip = std::find_if (moved.clips.begin(), moved.clips.end(), [&] (const auto& clip) {
        return clip.id == imported.clips[1].id;
    });
    REQUIRE (movedClip != moved.clips.end());
    REQUIRE (movedClip->trackId == imported.tracks[kTrackCount - 1].id);
    REQUIRE (movedClip->timelineStart > 0);
    const auto movedSnapshot = snapshotMainComponent (*shell);
    REQUIRE (movedSnapshot.context.snapEnabled);
    REQUIRE (movedClip->timelineStart % movedSnapshot.context.snapGridTicks == 0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).clips == imported.clips);
}

TEST_CASE ("the loop brace resizes and moves on the ruler with snap and exact spans",
           "[ui][input][shell][timeline][loop-brace]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("loop-brace");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");
    const double sampleRateHz = project.sampleRate.hz;

    const MainComponentSnapshot base = snapshotMainComponent (*shell);
    REQUIRE (base.context.snapEnabled);
    const std::int64_t grid = base.context.snapGridTicks;

    const auto xAtTick = [&] (yesdaw::engine::Tick tick) {
        return projectRulerPointAtTick (timeline, snapshotMainComponent (*shell), project, tick).x;
    };
    const int rulerY = projectRulerPointAtTick (
        timeline, snapshotMainComponent (*shell), project, 0).y;
    const auto braceRects = [&] {
        const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
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
        state.loopActive = snapshot.context.loopEnabled;
        state.loopStartSeconds = static_cast<double> (snapshot.playbackLoopStartFrame) / sampleRateHz;
        state.loopEndSeconds = static_cast<double> (snapshot.playbackLoopEndFrame) / sampleRateHz;
        return yesdaw::ui::timelineLoopBraceRects (timeline.getLocalBounds(), state);
    };
    const juce::ModifierKeys shiftDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
    const juce::ModifierKeys ctrlDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier);

    // Create the loop: Shift+drag snaps both endpoints (E4) to [0, grid*2].
    dragFromTo (timeline, { xAtTick (grid * 2 / 5), rulerY }, { xAtTick (grid * 8 / 5), rulerY },
                shiftDrag);
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.loopEnabled);
    REQUIRE (snapshot.playbackLoopStartFrame == 0);
    REQUIRE (snapshot.playbackLoopEndFrame == grid * 2);

    // Drag the END handle inward: the dragged edge snaps, the start keeps its exact frames.
    yesdaw::ui::TimelineLoopBraceRects rects = braceRects();
    REQUIRE (rects.valid);
    dragFromTo (timeline, rects.endHandle.getCentre(),
                { xAtTick (grid * 2 / 3), rects.endHandle.getCentreY() });
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.playbackLoopStartFrame == 0);
    REQUIRE (snapshot.playbackLoopEndFrame == grid);

    // Drag the BAND right by exactly one grid: the span is preserved exactly.
    rects = braceRects();
    REQUIRE (rects.valid);
    const juce::Point<int> bandGrab { (rects.startHandle.getRight() + rects.endHandle.getX()) / 2,
                                      rects.band.getCentreY() };
    dragFromTo (timeline, bandGrab, { bandGrab.x + (xAtTick (grid) - xAtTick (0)), bandGrab.y });
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.playbackLoopStartFrame == grid);
    REQUIRE (snapshot.playbackLoopEndFrame == grid * 2);

    // Drag the START handle outward to before zero: it snaps and clamps to zero.
    rects = braceRects();
    REQUIRE (rects.valid);
    dragFromTo (timeline, rects.startHandle.getCentre(),
                { xAtTick (grid / 3), rects.startHandle.getCentreY() });
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.playbackLoopStartFrame == 0);
    REQUIRE (snapshot.playbackLoopEndFrame == grid * 2);

    // Ctrl inverts the grid: the end handle lands on the exact raw tick.
    rects = braceRects();
    REQUIRE (rects.valid);
    const int rawTargetX = xAtTick (grid * 5 / 4);
    dragFromTo (timeline, rects.endHandle.getCentre(), { rawTargetX, rects.endHandle.getCentreY() },
                ctrlDrag);
    snapshot = snapshotMainComponent (*shell);
    const MainComponentSnapshot rawSnapshot = snapshot;
    {
        yesdaw::ui::TimelineCanvasState state;
        state.trackCount = static_cast<int> (project.tracks.size());
        state.totalSeconds = rawSnapshot.visibleTimelineTotalSeconds;
        const double fitPixelsPerSecond = static_cast<double> (juce::jmax (
                                                  yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                                  timeline.getWidth()
                                                      - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                        / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                    state.totalSeconds);
        state.viewport.pixelsPerSecond = fitPixelsPerSecond * rawSnapshot.timelineZoomFactor;
        state.viewport.scrollSeconds = rawSnapshot.timelineScrollSeconds;
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double seconds = geometry.viewport.scrollSeconds
                             + static_cast<double> (rawTargetX - geometry.clipArea.getX()) / pixelsPerSecond;
        REQUIRE (snapshot.playbackLoopEndFrame
                 == static_cast<long long> (std::llround (seconds * sampleRateHz)));
    }
    REQUIRE (snapshot.playbackLoopStartFrame == 0);

    // Escape cancels an in-flight brace drag without committing.
    rects = braceRects();
    REQUIRE (rects.valid);
    const long long endBeforeCancel = snapshot.playbackLoopEndFrame;
    beginDragFromTo (timeline, rects.endHandle.getCentre(),
                     { xAtTick (grid / 2), rects.endHandle.getCentreY() });
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    releaseDragAt (timeline, rects.endHandle.getCentre(),
                   { xAtTick (grid / 2), rects.endHandle.getCentreY() });
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.playbackLoopEndFrame == endBeforeCancel);

    // A plain ruler press below the brace band still locates and leaves the loop alone.
    const juce::Point<int> locatePoint { xAtTick (grid / 2), rulerY };
    REQUIRE (locatePoint.y > braceRects().band.getBottom());
    mouseDownAt (timeline, locatePoint);
    releaseDragAt (timeline, locatePoint, locatePoint);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.playheadFrame > 0);
    REQUIRE (snapshot.playbackLoopEndFrame == endBeforeCancel);
    REQUIRE (snapshot.playbackLoopStartFrame == 0);

    // The loop brace is honestly transient transport state.
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);
}

TEST_CASE ("markers drag-move on the ruler and rename inline, all undoable",
           "[ui][input][shell][timeline][marker-edit]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("marker-edit");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component& timeline = requireTimelineComponent (*shell);
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const double sampleRateHz = project.sampleRate.hz;
    const MainComponentSnapshot base = snapshotMainComponent (*shell);
    REQUIRE (base.context.snapEnabled);
    const std::int64_t grid = base.context.snapGridTicks;

    // Two real markers via the M chord at located playheads.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::rightKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));
    yesdaw::engine::Project marked = readProjectSnapshot (bundlePath);
    REQUIRE (marked.markers.size() == 2u);
    REQUIRE (marked.markers[0].tick == 0);
    REQUIRE (marked.markers[1].tick == grid);
    const yesdaw::engine::EntityId movedMarkerId = marked.markers[1].id;

    // Shared pixel/label-law replication.
    std::vector<std::string> markerLabels;
    std::vector<yesdaw::ui::TimelineMarker> markerViews;
    const auto rebuildViews = [&] (const yesdaw::engine::Project& snapshotProject) {
        markerLabels.clear();
        markerViews.clear();
        for (const yesdaw::engine::Marker& marker : snapshotProject.markers)
        {
            markerLabels.push_back (marker.name);
            markerViews.push_back ({ static_cast<double> (marker.tick) / sampleRateHz,
                                     markerLabels.back().c_str() });
        }
    };
    const auto canvasState = [&] {
        const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
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
        state.markers = markerViews.data();
        state.markerCount = static_cast<int> (markerViews.size());
        return state;
    };
    const auto rawTickAtX = [&] (int x) {
        const yesdaw::ui::TimelineCanvasState state = canvasState();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double seconds = geometry.viewport.scrollSeconds
                             + static_cast<double> (x - geometry.clipArea.getX()) / pixelsPerSecond;
        return static_cast<yesdaw::engine::Tick> (std::llround (seconds * sampleRateHz));
    };
    const auto xAtTick = [&] (yesdaw::engine::Tick tick) {
        return projectRulerPointAtTick (timeline, snapshotMainComponent (*shell), project, tick).x;
    };
    const auto markerById = [] (const yesdaw::engine::Project& snapshotProject,
                                yesdaw::engine::EntityId markerId) -> const yesdaw::engine::Marker* {
        for (const yesdaw::engine::Marker& marker : snapshotProject.markers)
            if (marker.id == markerId)
                return &marker;
        return nullptr;
    };
    const juce::ModifierKeys ctrlDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier);

    // Ctrl drag moves the second marker to the exact raw tick.
    rebuildViews (marked);
    juce::Rectangle<int> label = yesdaw::ui::timelineMarkerLabelRect (
        timeline.getLocalBounds(), canvasState(), 1);
    REQUIRE_FALSE (label.isEmpty());
    const int rawTargetX = xAtTick (grid * 5 / 4);
    dragFromTo (timeline, label.getCentre(), { rawTargetX, label.getCentreY() }, ctrlDrag);
    yesdaw::engine::Project moved = readProjectSnapshot (bundlePath);
    const yesdaw::engine::Marker* movedMarker = markerById (moved, movedMarkerId);
    REQUIRE (movedMarker != nullptr);
    REQUIRE (movedMarker->tick == rawTickAtX (rawTargetX));

    // A plain (snap-on) drag lands the marker exactly on the grid.
    rebuildViews (moved);
    const auto movedIndex = static_cast<int> (std::distance (
        moved.markers.begin(),
        std::find_if (moved.markers.begin(), moved.markers.end(), [&] (const auto& marker) {
            return marker.id == movedMarkerId;
        })));
    label = yesdaw::ui::timelineMarkerLabelRect (timeline.getLocalBounds(), canvasState(), movedIndex);
    REQUIRE_FALSE (label.isEmpty());
    dragFromTo (timeline, label.getCentre(), { xAtTick (grid * 5 / 3), label.getCentreY() });
    moved = readProjectSnapshot (bundlePath);
    movedMarker = markerById (moved, movedMarkerId);
    REQUIRE (movedMarker != nullptr);
    REQUIRE (movedMarker->tick == grid * 2);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (markerById (readProjectSnapshot (bundlePath), movedMarkerId)->tick == rawTickAtX (rawTargetX));

    // Double-click the first marker's label: inline rename commits on Enter and is undoable.
    yesdaw::engine::Project current = readProjectSnapshot (bundlePath);
    rebuildViews (current);
    label = yesdaw::ui::timelineMarkerLabelRect (timeline.getLocalBounds(), canvasState(), 0);
    doubleClickAt (timeline, label.getCentre());
    auto* renameEditor = dynamic_cast<juce::TextEditor*> (
        findChildWithComponentId (*shell, "shell.timeline.marker.rename"));
    REQUIRE (renameEditor != nullptr);
    REQUIRE (renameEditor->isVisible());
    renameEditor->setText ("Chorus", juce::dontSendNotification);
    REQUIRE (renameEditor->keyPressed (juce::KeyPress (juce::KeyPress::returnKey)));
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    REQUIRE_FALSE (renameEditor->isVisible());
    current = readProjectSnapshot (bundlePath);
    REQUIRE (current.markers[0].name == "Chorus");
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).markers[0].name == "Marker 1");
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));

    // Escape discards a rename draft without persisting.
    current = readProjectSnapshot (bundlePath);
    rebuildViews (current);
    label = yesdaw::ui::timelineMarkerLabelRect (timeline.getLocalBounds(), canvasState(), 0);
    doubleClickAt (timeline, label.getCentre());
    REQUIRE (renameEditor->isVisible());
    renameEditor->setText ("Discarded Draft", juce::dontSendNotification);
    REQUIRE (renameEditor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    REQUIRE_FALSE (renameEditor->isVisible());
    REQUIRE (readProjectSnapshot (bundlePath).markers[0].name == "Chorus");

    // A plain click on a marker label keeps the historical ruler-click locate.
    rebuildViews (readProjectSnapshot (bundlePath));
    label = yesdaw::ui::timelineMarkerLabelRect (timeline.getLocalBounds(), canvasState(), 0);
    mouseDownAt (timeline, label.getCentre());
    releaseDragAt (timeline, label.getCentre(), label.getCentre());
    REQUIRE (snapshotMainComponent (*shell).context.playheadFrame
             == rawTickAtX (label.getCentreX()));
}

TEST_CASE ("MIDI clips are first-class timeline citizens",
           "[ui][input][shell][timeline][midi-clip]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("midi-clip-timeline");
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
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);

    // E11: the empty roll grid is tool-aware — the pencil needs the Pencil tool; Pointer is
    // restored before the timeline gestures below.
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    mouseDownAt (pianoRoll, pianoRoll.getLocalBounds().getCentre());
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));

    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.tracks.size() == 2u);
    REQUIRE (original.clips.size() == 1u);
    REQUIRE (original.midiClips.size() == 1u);
    REQUIRE (original.midiClips.front().trackId == original.tracks[1].id);
    REQUIRE (original.midiClips.front().timelineStart == 0);
    REQUIRE (original.midiClips.front().notes.size() == 1u);
    const yesdaw::engine::EntityId midiId = original.midiClips.front().id;
    const double sampleRateHz = original.sampleRate.hz;

    const MainComponentSnapshot base = snapshotMainComponent (*shell);
    REQUIRE (base.context.snapEnabled);
    const std::int64_t grid = base.context.snapGridTicks;

    // Layout replication from the REAL viewport (midi clips extend totalSeconds, so the
    // audio-only test helpers cannot be used here).
    juce::Component& timeline = requireTimelineComponent (*shell);
    const auto geometryNow = [&] {
        const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
        yesdaw::ui::TimelineCanvasState state;
        state.trackCount = 2;
        state.totalSeconds = snapshot.visibleTimelineTotalSeconds;
        const double fitPixelsPerSecond = static_cast<double> (juce::jmax (
                                                  yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                                  timeline.getWidth()
                                                      - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                        / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                    state.totalSeconds);
        state.viewport.pixelsPerSecond = fitPixelsPerSecond * snapshot.timelineZoomFactor;
        state.viewport.scrollSeconds = snapshot.timelineScrollSeconds;
        return yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    };
    const auto pointAt = [&] (yesdaw::engine::Tick tick, int lane) {
        const yesdaw::ui::TimelineCanvasGeometry geometry = geometryNow();
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        return juce::Point<int> {
            geometry.clipArea.getX()
                + juce::roundToInt ((static_cast<double> (tick) / sampleRateHz
                                     - geometry.viewport.scrollSeconds) * pixelsPerSecond),
            geometry.clipArea.getY() + lane * geometry.laneHeight + geometry.laneHeight / 2
        };
    };
    const auto gridPixels = [&] {
        const yesdaw::ui::TimelineCanvasGeometry geometry = geometryNow();
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        return juce::roundToInt (static_cast<double> (grid) / sampleRateHz * pixelsPerSecond);
    };

    const std::uint64_t renderFrames = static_cast<std::uint64_t> (
        original.midiClips.front().timelineLength + 2 * grid + 256);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> beforeEdits = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (peakAbs (std::span<const float> (beforeEdits.data(), beforeEdits.size())) > 0.01);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    // HIT + SELECT: a click on the painted MIDI clip selects it like an audio clip.
    const yesdaw::engine::Tick midiMid = original.midiClips.front().timelineLength / 2;
    mouseDownAt (timeline, pointAt (midiMid, 1));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);

    // MOVE in time: a snapped drag persists through the new MoveMidiClip verb.
    dragFromTo (timeline, pointAt (midiMid, 1),
                { pointAt (midiMid, 1).x + gridPixels(), pointAt (midiMid, 1).y });
    yesdaw::engine::Project edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().timelineStart == grid);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterMove = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterMove != beforeEdits);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().timelineStart == 0);

    // CROSS-TRACK: a vertical drag persists through MoveMidiClipToTrack; undo restores.
    dragFromTo (timeline, pointAt (midiMid, 1), pointAt (midiMid, 0));
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().trackId == edited.tracks[0].id);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().trackId == original.tracks[1].id);

    // GROUP: project-wide selection moves audio and MIDI together as one undo step.
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 2);
    const yesdaw::engine::Tick audioMid = original.clips.front().timelineLength / 2;
    dragFromTo (timeline, pointAt (audioMid, 0),
                { pointAt (audioMid, 0).x + gridPixels(), pointAt (audioMid, 0).y });
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.clips.front().timelineStart == grid);
    REQUIRE (edited.midiClips.front().timelineStart == grid);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.clips.front().timelineStart == 0);
    REQUIRE (edited.midiClips.front().timelineStart == 0);

    // DUPLICATE: Ctrl+D copies both kinds after the selection span, notes included, one undo.
    REQUIRE (shell->keyPressed (juce::KeyPress (
        'a', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.clips.size() == 2u);
    REQUIRE (edited.midiClips.size() == 2u);
    const yesdaw::engine::Tick span = original.midiClips.front().timelineLength;
    const auto midiCopy = std::find_if (edited.midiClips.begin(), edited.midiClips.end(), [&] (const auto& clip) {
        return clip.id != midiId;
    });
    REQUIRE (midiCopy != edited.midiClips.end());
    REQUIRE (midiCopy->trackId == original.tracks[1].id);
    REQUIRE (midiCopy->timelineStart == span);
    REQUIRE (midiCopy->notes.size() == 1u);
    REQUIRE (midiCopy->notes.front().key == original.midiClips.front().notes.front().key);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.clips.size() == 1u);
    REQUIRE (edited.midiClips.size() == 1u);

    // Trim and split honestly refuse on MIDI clips (no verb exists yet by design).
    mouseDownAt (timeline, pointAt (midiMid, 1));
    doubleClickAt (timeline, pointAt (midiMid, 1));
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.size() == 1u);
    REQUIRE (edited.clips.size() == 1u);

    // DELETE: Del removes the selected MIDI clip; the audio clip survives; undo restores audio.
    mouseDownAt (timeline, pointAt (midiMid, 1));
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.empty());
    REQUIRE (edited.clips.size() == 1u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.size() == 1u);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> afterUndo = renderMainComponentPlayback (*shell, renderFrames, 128);
    REQUIRE (afterUndo == beforeEdits);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
}

TEST_CASE ("the piano roll follows the double-clicked timeline MIDI clip",
           "[ui][input][shell][timeline][roll-follow]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("roll-follow");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    auto* addTrack = dynamic_cast<juce::Button*> (findChildWithComponentId (*shell, "track.add"));
    REQUIRE (addTrack != nullptr);
    clickButton (*addTrack);
    clickButton (*addTrack);
    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    const int headerHeight = yesdaw::ui::UiTheme::Layout::trackListHeaderHeight;
    const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                      (rail->getHeight() - headerHeight) / 3);
    const auto selectRailRow = [&] (int row) {
        mouseDownAt (*rail, { rail->getWidth() / 2, headerHeight + row * rowHeight + rowHeight / 2 });
    };

    // One MIDI clip per track, all at frame zero.
    for (int row = 0; row < 3; ++row)
    {
        selectRailRow (row);
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
        REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
        REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    }
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.tracks.size() == 3u);
    REQUIRE (original.midiClips.size() == 3u);
    const double sampleRateHz = original.sampleRate.hz;
    const yesdaw::engine::Tick clipLength = original.midiClips.front().timelineLength;

    const auto notesOnTrack = [&] (std::size_t trackIndex) {
        const yesdaw::engine::Project snapshotProject = readProjectSnapshot (bundlePath);
        for (const yesdaw::engine::MidiClip& midiClip : snapshotProject.midiClips)
            if (midiClip.trackId == snapshotProject.tracks[trackIndex].id)
                return midiClip.notes.size();
        return std::size_t { 0 };
    };
    REQUIRE (notesOnTrack (0) == 0u);
    REQUIRE (notesOnTrack (1) == 0u);
    REQUIRE (notesOnTrack (2) == 0u);

    juce::Component& timeline = requireTimelineComponent (*shell);
    const auto midiClipPointOnLane = [&] (int lane) {
        const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
        yesdaw::ui::TimelineCanvasState state;
        state.trackCount = 3;
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
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        return juce::Point<int> {
            geometry.clipArea.getX()
                + juce::roundToInt ((static_cast<double> (clipLength / 2) / sampleRateHz
                                     - geometry.viewport.scrollSeconds) * pixelsPerSecond),
            geometry.clipArea.getY() + lane * geometry.laneHeight + geometry.laneHeight / 2
        };
    };

    // Double-click the LAST track's MIDI clip: the roll opens on THAT clip and the pencil lands
    // its note there and nowhere else. (E11: the pencil needs the Pencil tool.)
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    doubleClickAt (timeline, midiClipPointOnLane (2));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    mouseDownAt (pianoRoll, pianoRoll.getLocalBounds().getCentre());
    REQUIRE (notesOnTrack (2) == 1u);
    REQUIRE (notesOnTrack (0) == 0u);
    REQUIRE (notesOnTrack (1) == 0u);

    // Switch to the FIRST track's clip the same way.
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    doubleClickAt (timeline, midiClipPointOnLane (0));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);
    mouseDownAt (pianoRoll, pianoRoll.getLocalBounds().getCentre());
    REQUIRE (notesOnTrack (0) == 1u);
    REQUIRE (notesOnTrack (1) == 0u);
    REQUIRE (notesOnTrack (2) == 1u);

    // The View Piano Roll action retains the LAST opened clip instead of resetting to the first.
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);
    mouseDownAt (pianoRoll, pianoRoll.getLocalBounds().getCentre().translated (
                                0, yesdaw::ui::UiTheme::Layout::pianoRollKeyRowMinHeight * 2));
    REQUIRE (notesOnTrack (0) == 2u);
    REQUIRE (notesOnTrack (1) == 0u);
    REQUIRE (notesOnTrack (2) == 1u);
}

TEST_CASE ("the piano roll viewport scrolls all 128 keys and zooms and scrolls time",
           "[ui][input][shell][pianoroll][roll-viewport]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("roll-viewport");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);
    // E11: the empty-grid pencil needs the Pencil tool.
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    const yesdaw::engine::Project original = readProjectSnapshot (bundlePath);
    REQUIRE (original.midiClips.size() == 1u);
    const yesdaw::engine::Tick clipLength = original.midiClips.front().timelineLength;
    const std::vector<std::uint8_t> persistedBefore = readBytes (bundlePath / "project.db");

    // Defaults reproduce the historical fixed view.
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.pianoRollViewLowKey == yesdaw::ui::UiThemeLayout::pianoRollDefaultLowKey);
    REQUIRE (snapshot.pianoRollViewZoom == 1.0);
    REQUIRE (snapshot.pianoRollViewScrollTicks == 0);

    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    const juce::Rectangle<int> grid = pianoRollGridBounds (pianoRoll);
    juce::MouseWheelDetails wheelDown {};
    wheelDown.deltaY = -0.4f;
    juce::MouseWheelDetails wheelUp {};
    wheelUp.deltaY = 0.4f;
    const juce::MouseEvent plainWheel = makeMouseEvent (
        pianoRoll, grid.getCentre(), grid.getCentre(), false, 1, juce::ModifierKeys {});

    // Plain wheel scrolls the key window and clamps at the bottom; the view is transient.
    for (int i = 0; i < 60; ++i)
        pianoRoll.mouseWheelMove (plainWheel, wheelDown);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.pianoRollViewLowKey == yesdaw::ui::UiThemeLayout::pianoRollKeyMin);
    REQUIRE (readBytes (bundlePath / "project.db") == persistedBefore);

    // The bottom row is now key 0: the real pencil proves it end-to-end.
    const auto noteWithKey = [&] (const yesdaw::engine::Project& snapshotProject, int key)
        -> const yesdaw::engine::Note* {
        for (const yesdaw::engine::Note& note : snapshotProject.midiClips.front().notes)
            if (note.key == key)
                return &note;
        return nullptr;
    };
    mouseDownAt (pianoRoll, { grid.getCentreX(), grid.getBottom() - 2 });
    yesdaw::engine::Project edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().notes.size() == 1u);
    REQUIRE (noteWithKey (edited, 0) != nullptr);

    // Scroll to the very top: the top row is key 127.
    for (int i = 0; i < 130; ++i)
        pianoRoll.mouseWheelMove (plainWheel, wheelUp);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.pianoRollViewLowKey
             == yesdaw::ui::UiThemeLayout::pianoRollKeyMax
                    - (yesdaw::ui::UiTheme::Layout::pianoRollKeyCount - 1));
    mouseDownAt (pianoRoll, { grid.getCentreX(), grid.getY() + 2 });
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().notes.size() == 2u);
    REQUIRE (noteWithKey (edited, 127) != nullptr);

    // Ctrl+wheel zooms time; Shift+wheel scrolls it by the exact wheel law.
    const juce::MouseEvent ctrlWheel = makeMouseEvent (
        pianoRoll, grid.getCentre(), grid.getCentre(), false, 1,
        juce::ModifierKeys (juce::ModifierKeys::ctrlModifier));
    for (int i = 0; i < 3; ++i)
        pianoRoll.mouseWheelMove (ctrlWheel, wheelUp);
    snapshot = snapshotMainComponent (*shell);
    const double expectedZoom = yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep
                              * yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep
                              * yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep;
    REQUIRE (snapshot.pianoRollViewZoom == Catch::Approx (expectedZoom));

    const long long scrollBefore = snapshot.pianoRollViewScrollTicks;
    const auto visibleTicks = static_cast<double> (juce::jmax<yesdaw::engine::Tick> (
        1, static_cast<yesdaw::engine::Tick> (
               std::llround (static_cast<double> (clipLength) / snapshot.pianoRollViewZoom))));
    const juce::MouseEvent shiftWheel = makeMouseEvent (
        pianoRoll, grid.getCentre(), grid.getCentre(), false, 1,
        juce::ModifierKeys (juce::ModifierKeys::shiftModifier));
    pianoRoll.mouseWheelMove (shiftWheel, wheelDown);
    snapshot = snapshotMainComponent (*shell);
    const long long expectedScroll = scrollBefore
        + static_cast<long long> (std::llround (
              0.4 * visibleTicks * yesdaw::ui::UiTheme::Layout::timelineScrollWheelFraction));
    REQUIRE (snapshot.pianoRollViewScrollTicks == expectedScroll);

    // The pencil under zoom+scroll lands at the exact snapped viewport tick.
    mouseDownAt (pianoRoll, { grid.getCentreX(), grid.getCentreY() });
    edited = readProjectSnapshot (bundlePath);
    REQUIRE (edited.midiClips.front().notes.size() == 3u);
    const double normalized = static_cast<double> (grid.getCentreX() - grid.getX())
                            / static_cast<double> (grid.getWidth());
    yesdaw::engine::Tick expectedTick = static_cast<yesdaw::engine::Tick> (snapshot.pianoRollViewScrollTicks)
        + static_cast<yesdaw::engine::Tick> (normalized * visibleTicks);
    // E12: the pencil floors to the REAL snap chooser grid.
    REQUIRE (snapshot.context.snapEnabled);
    expectedTick -= expectedTick % static_cast<yesdaw::engine::Tick> (snapshot.context.snapGridTicks);
    const int expectedCentreKey = snapshot.pianoRollViewLowKey
        + yesdaw::ui::UiTheme::Layout::pianoRollKeyCount - 1
        - static_cast<int> (static_cast<double> (grid.getCentreY() - grid.getY())
                            / (static_cast<double> (juce::jmax (1, grid.getHeight()))
                               / static_cast<double> (yesdaw::ui::UiTheme::Layout::pianoRollKeyCount)));
    const yesdaw::engine::Note* const centreNote = noteWithKey (edited, expectedCentreKey);
    REQUIRE (centreNote != nullptr);
    REQUIRE (centreNote->startTick == expectedTick);

    // Zooming back out clamps to 1x and resets the time scroll.
    for (int i = 0; i < 5; ++i)
        pianoRoll.mouseWheelMove (ctrlWheel, wheelDown);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.pianoRollViewZoom == yesdaw::ui::UiThemeLayout::pianoRollZoomMin);
    REQUIRE (snapshot.pianoRollViewScrollTicks == 0);
}

TEST_CASE ("piano roll selection tools: pointer deselect, marquee, shift toggle, mouse delete",
           "[ui][input][shell][pianoroll][roll-select]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("roll-select");

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    const juce::Rectangle<int> grid = pianoRollGridBounds (pianoRoll);
    const int rowStep = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollKeyRowMinHeight,
                                    grid.getHeight() / yesdaw::ui::UiTheme::Layout::pianoRollKeyCount);

    // Three pencilled notes at distinct keys and ticks.
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    mouseDownAt (pianoRoll, grid.getCentre().translated (-80, 3 * rowStep));
    mouseDownAt (pianoRoll, grid.getCentre());
    mouseDownAt (pianoRoll, grid.getCentre().translated (80, -3 * rowStep));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 3u);
    const yesdaw::engine::MidiClip midi = project.midiClips.front();
    const auto sortedByTick = [&] {
        std::vector<yesdaw::engine::Note> notes = readProjectSnapshot (bundlePath).midiClips.front().notes;
        std::sort (notes.begin(), notes.end(), [] (const auto& a, const auto& b) {
            return a.startTick < b.startTick;
        });
        return notes;
    };
    const std::vector<yesdaw::engine::Note> ordered = sortedByTick();
    const yesdaw::engine::Note noteA = ordered[0];   // left-low
    const yesdaw::engine::Note noteB = ordered[1];   // centre
    const yesdaw::engine::Note noteC = ordered[2];   // right-high

    // POINTER: an empty click clears the selection instead of pencilling; Del then refuses.
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    const juce::Point<int> emptySpot { grid.getX() + 2, grid.getBottom() - 2 };
    mouseDownAt (pianoRoll, emptySpot);
    releaseDragAt (pianoRoll, emptySpot, emptySpot);
    REQUIRE_FALSE (snapshotMainComponent (*shell).context.midiNoteSelected);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().notes.size() == 3u);

    // POINTER MARQUEE: a drag over two notes selects exactly those; Del removes them as one
    // undoable group.
    const juce::Point<int> centreA = pianoRollNoteCenterPoint (pianoRoll, midi, noteA);
    const juce::Point<int> centreB = pianoRollNoteCenterPoint (pianoRoll, midi, noteB);
    dragFromTo (pianoRoll,
                { centreA.x - 40, centreA.y + rowStep },
                { centreB.x + 20, centreB.y - rowStep });
    REQUIRE (snapshotMainComponent (*shell).context.midiNoteSelected);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 1u);
    REQUIRE (project.midiClips.front().notes.front().id == noteC.id);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().notes.size() == 3u);

    // SHIFT+CLICK toggle: select all, toggle C OUT, Del keeps exactly C.
    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    const juce::Point<int> centreC = pianoRollNoteCenterPoint (pianoRoll, midi, noteC);
    mouseDownAt (pianoRoll, centreC,
                 juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                     | juce::ModifierKeys::shiftModifier));
    releaseDragAt (pianoRoll, centreC, centreC,
                   juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                       | juce::ModifierKeys::shiftModifier));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 1u);
    REQUIRE (project.midiClips.front().notes.front().id == noteC.id);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().notes.size() == 3u);

    // Plain double-click deletes a single note with the mouse.
    doubleClickAt (pianoRoll, pianoRollNoteCenterPoint (pianoRoll, midi, noteB));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 2u);
    for (const yesdaw::engine::Note& note : project.midiClips.front().notes)
        REQUIRE_FALSE (note.id == noteB.id);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().notes.size() == 3u);

    // The Pencil tool still adds.
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    mouseDownAt (pianoRoll, grid.getCentre().translated (0, -5 * rowStep));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().notes.size() == 4u);
}

TEST_CASE ("piano roll drags: pitch, group move, left-edge trim, real snap",
           "[ui][input][shell][pianoroll][roll-drag]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("roll-drag");

    // Frame-scale seed: 48kHz at 120bpm makes the Beat chooser grid 24000 ticks. Both notes sit
    // OFF that grid so every snap assertion bites, and both are wide enough to grab by the middle.
    yesdaw::engine::Project seed = makeMidiInputProject();
    seed.midiClips.front().timelineLength = 384000;
    seed.midiClips.front().notes = {
        makeMidiInputNote (idFromLowByte (4), 50000, 48000, 60, 0.55),
        makeMidiInputNote (idFromLowByte (5), 200000, 48000, 64, 0.82)
    };
    writeProjectSnapshot (bundlePath, seed);

    MainComponentFileChoices choices;
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    auto* snapChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (snapChooser != nullptr);
    snapChooser->setSelectedId (1, juce::sendNotificationSync);   // raw drags first
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);

    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    const juce::Rectangle<int> grid = pianoRollGridBounds (pianoRoll);

    const yesdaw::engine::MidiClip midi = readProjectSnapshot (bundlePath).midiClips.front();
    REQUIRE (midi.notes.size() == 2u);
    const yesdaw::engine::Note noteA = midi.notes[0];
    const yesdaw::engine::Note noteB = midi.notes[1];
    REQUIRE (noteA.startTick == 50000);
    REQUIRE (noteB.startTick == 200000);
    const auto noteById = [&] (yesdaw::engine::EntityId noteId) {
        // Copy the notes out: ranging over readProjectSnapshot(...).midiClips.front().notes
        // would dangle (front() breaks the temporary's lifetime extension).
        const std::vector<yesdaw::engine::Note> notes =
            readProjectSnapshot (bundlePath).midiClips.front().notes;
        for (const yesdaw::engine::Note& note : notes)
            if (note.id == noteId)
                return note;
        FAIL ("note missing");
        return yesdaw::engine::Note {};
    };
    const double rowHeight = static_cast<double> (std::max (1, grid.getHeight()))
                           / static_cast<double> (kPianoRollKeyCount);

    // PITCH drag (chooser Off): a pure vertical drag transposes without moving the start.
    const juce::Point<int> centreA = pianoRollNoteCenterPoint (pianoRoll, midi, noteA);
    dragFromTo (pianoRoll, centreA,
                { centreA.x, centreA.y + juce::roundToInt (2.0 * rowHeight) });
    yesdaw::engine::Note edited = noteById (noteA.id);
    REQUIRE (edited.key == noteA.key - 2);
    REQUIRE (edited.startTick == noteA.startTick);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (noteById (noteA.id).key == noteA.key);

    // GROUP drag (chooser Beat): marquee both notes; dragging one moves BOTH by the anchor's
    // snapped tick delta and one semitone up, as one undo step.
    snapChooser->setSelectedId (3, juce::sendNotificationSync);
    const std::int64_t gridTicks = snapshotMainComponent (*shell).context.snapGridTicks;
    REQUIRE (gridTicks == 24000);
    dragFromTo (pianoRoll,
                { grid.getX() + 2, grid.getBottom() - 2 },
                { grid.getRight() - 2, grid.getY() + 2 });
    REQUIRE (snapshotMainComponent (*shell).context.midiNoteSelected);
    const int dragPixels = grid.getWidth() / 6;
    const yesdaw::engine::Tick rawDelta = pianoRollDeltaTicksForPixels (pianoRoll, midi, dragPixels);
    yesdaw::engine::Tick snappedTarget = noteA.startTick + rawDelta;
    REQUIRE (yesdaw::engine::snapTick (noteA.startTick + rawDelta,
                                       yesdaw::engine::SnapGrid { gridTicks }, snappedTarget));
    const yesdaw::engine::Tick expectedDelta = snappedTarget - noteA.startTick;
    const juce::Point<int> centreA2 = pianoRollNoteCenterPoint (pianoRoll, midi, noteA);
    dragFromTo (pianoRoll, centreA2,
                { centreA2.x + dragPixels, centreA2.y - juce::roundToInt (rowHeight) });
    edited = noteById (noteA.id);
    REQUIRE (edited.startTick == noteA.startTick + expectedDelta);
    REQUIRE (edited.key == noteA.key + 1);
    yesdaw::engine::Note editedB = noteById (noteB.id);
    REQUIRE (editedB.startTick == noteB.startTick + expectedDelta);
    REQUIRE (editedB.key == noteB.key + 1);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (noteById (noteA.id).startTick == noteA.startTick);
    REQUIRE (noteById (noteB.id).startTick == noteB.startTick);

    // LEFT-EDGE trim (chooser Off): the head moves, the end stays fixed.
    snapChooser->setSelectedId (1, juce::sendNotificationSync);
    const yesdaw::engine::Tick noteBEnd = noteB.startTick + noteB.lengthTicks;
    const int leftEdgeX = grid.getX()
        + juce::roundToInt (static_cast<double> (noteB.startTick)
                            / static_cast<double> (midi.timelineLength)
                            * static_cast<double> (grid.getWidth()));
    const juce::Point<int> edgePoint = {
        leftEdgeX + 1, pianoRollNoteCenterPoint (pianoRoll, midi, noteB).y
    };
    const int trimPixels = 30;
    const yesdaw::engine::Tick trimDelta = pianoRollDeltaTicksForPixels (pianoRoll, midi, trimPixels);
    dragFromTo (pianoRoll, edgePoint, { edgePoint.x + trimPixels, edgePoint.y });
    editedB = noteById (noteB.id);
    REQUIRE (editedB.startTick == noteB.startTick + trimDelta);
    REQUIRE (editedB.startTick + editedB.lengthTicks == noteBEnd);
    REQUIRE (editedB.lengthTicks < noteB.lengthTicks);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (noteById (noteB.id).startTick == noteB.startTick);

    // SNAPPED horizontal move (chooser Beat): the landed start is a grid multiple.
    snapChooser->setSelectedId (3, juce::sendNotificationSync);
    mouseDownAt (pianoRoll, { grid.getX() + 2, grid.getBottom() - 2 });
    releaseDragAt (pianoRoll, { grid.getX() + 2, grid.getBottom() - 2 },
                   { grid.getX() + 2, grid.getBottom() - 2 });
    const juce::Point<int> centreB = pianoRollNoteCenterPoint (pianoRoll, midi, noteB);
    mouseDownAt (pianoRoll, centreB);
    dragFromTo (pianoRoll, centreB, { centreB.x + grid.getWidth() / 8, centreB.y });
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().notes.size() == 2u);
    editedB = noteById (noteB.id);
    REQUIRE (editedB.startTick % gridTicks == 0);
    REQUIRE (editedB.startTick != noteB.startTick);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.front().notes.size() == 2u);
    REQUIRE (noteById (noteB.id).startTick == noteB.startTick);

    // PENCIL floors to the real chooser grid (Beat here), and the pencilled note is far too
    // narrow for the edge zones — the pointer must still MOVE it, never resize it.
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    const int pencilRow = kPianoRollHighKey - 55;
    mouseDownAt (pianoRoll, { grid.getX() + juce::roundToInt (grid.getWidth() * 5.0 / 8.0),
                              grid.getY() + juce::roundToInt ((pencilRow + 0.5) * rowHeight) });
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    const std::vector<yesdaw::engine::Note> afterPencil =
        readProjectSnapshot (bundlePath).midiClips.front().notes;
    REQUIRE (afterPencil.size() == 3u);
    const auto pencilledIt = std::find_if (afterPencil.begin(), afterPencil.end(), [&] (const auto& note) {
        return note.id != noteA.id && note.id != noteB.id;
    });
    REQUIRE (pencilledIt != afterPencil.end());
    const yesdaw::engine::Note pencilled = *pencilledIt;
    REQUIRE (pencilled.startTick % gridTicks == 0);
    REQUIRE (pencilled.lengthTicks == yesdaw::ui::UiTheme::Layout::pianoRollGridTickStep);
    REQUIRE (pencilled.key == 55);

    const juce::Point<int> pencilCentre = pianoRollNoteCenterPoint (pianoRoll, midi, pencilled);
    dragFromTo (pianoRoll, pencilCentre, { pencilCentre.x + 40, pencilCentre.y });
    const yesdaw::engine::Note movedPencilled = noteById (pencilled.id);
    REQUIRE (movedPencilled.startTick % gridTicks == 0);
    REQUIRE (movedPencilled.startTick != pencilled.startTick);
    REQUIRE (movedPencilled.lengthTicks == pencilled.lengthTicks);
    REQUIRE (movedPencilled.key == pencilled.key);
}

TEST_CASE ("velocity lane drags paint crossed notes and the selection paints together",
           "[ui][input][shell][pianoroll][roll-velocity]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("roll-velocity");

    yesdaw::engine::Project seed = makeMidiInputProject();
    seed.midiClips.front().timelineLength = 384000;
    seed.midiClips.front().notes = {
        makeMidiInputNote (idFromLowByte (4), 50000, 48000, 60, 0.55),
        makeMidiInputNote (idFromLowByte (5), 200000, 48000, 64, 0.82)
    };
    writeProjectSnapshot (bundlePath, seed);

    MainComponentFileChoices choices;
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);

    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    const juce::Rectangle<int> grid = pianoRollGridBounds (pianoRoll);
    const juce::Rectangle<int> lane = pianoRollVelocityLaneBounds (pianoRoll);
    const yesdaw::engine::MidiClip midi = readProjectSnapshot (bundlePath).midiClips.front();
    REQUIRE (midi.notes.size() == 2u);
    const yesdaw::engine::Note noteA = midi.notes[0];
    const yesdaw::engine::Note noteB = midi.notes[1];
    const auto noteById = [&] (yesdaw::engine::EntityId noteId) {
        const std::vector<yesdaw::engine::Note> notes =
            readProjectSnapshot (bundlePath).midiClips.front().notes;
        for (const yesdaw::engine::Note& note : notes)
            if (note.id == noteId)
                return note;
        FAIL ("note missing");
        return yesdaw::engine::Note {};
    };
    const auto rampVelocity = [] (yesdaw::engine::Tick tick,
                                  yesdaw::engine::Tick tickFrom, double velocityFrom,
                                  yesdaw::engine::Tick tickTo, double velocityTo) {
        if (tickTo == tickFrom)
            return velocityTo;
        const double t = std::clamp (static_cast<double> (tick - tickFrom)
                                         / static_cast<double> (tickTo - tickFrom),
                                     0.0, 1.0);
        return velocityFrom + t * (velocityTo - velocityFrom);
    };

    // RAMP paint with NO selection: the drag crosses both note columns; each note takes the
    // ramp's velocity at its own start tick — one undo transaction.
    const juce::Point<int> rampFrom = { grid.getX() + grid.getWidth() / 8, lane.getBottom() - 6 };
    const juce::Point<int> rampTo = { grid.getX() + grid.getWidth() * 5 / 8, lane.getY() + 6 };
    const yesdaw::engine::Tick rampFromTick = pianoRollTickForLaneX (pianoRoll, midi, rampFrom.x);
    const yesdaw::engine::Tick rampToTick = pianoRollTickForLaneX (pianoRoll, midi, rampTo.x);
    const double rampFromVelocity = pianoRollVelocityForLaneYPixel (lane, rampFrom.y);
    const double rampToVelocity = pianoRollVelocityForLaneYPixel (lane, rampTo.y);
    REQUIRE (rampFromTick < noteA.startTick);
    REQUIRE (rampToTick > noteB.startTick);
    dragFromTo (pianoRoll, rampFrom, rampTo);
    yesdaw::engine::Note painted = noteById (noteA.id);
    REQUIRE (painted.normalizedVelocity
             == Catch::Approx (rampVelocity (noteA.startTick, rampFromTick, rampFromVelocity,
                                             rampToTick, rampToVelocity)).margin (1e-9));
    REQUIRE (painted.normalizedVelocity != Catch::Approx (noteA.normalizedVelocity).margin (1e-9));
    yesdaw::engine::Note paintedB = noteById (noteB.id);
    REQUIRE (paintedB.normalizedVelocity
             == Catch::Approx (rampVelocity (noteB.startTick, rampFromTick, rampFromVelocity,
                                             rampToTick, rampToVelocity)).margin (1e-9));
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (noteById (noteA.id).normalizedVelocity == Catch::Approx (0.55).margin (1e-9));
    REQUIRE (noteById (noteB.id).normalizedVelocity == Catch::Approx (0.82).margin (1e-9));

    // GROUP paint: marquee both notes, then drag ONLY over noteA's column — because a crossed
    // note is selected the WHOLE selection paints together, undone by one Ctrl+Z.
    dragFromTo (pianoRoll,
                { grid.getX() + 2, grid.getBottom() - 2 },
                { grid.getRight() - 2, grid.getY() + 2 });
    REQUIRE (snapshotMainComponent (*shell).context.midiNoteSelected);
    const juce::Point<int> groupFrom = { grid.getX() + grid.getWidth() / 6, lane.getBottom() - 8 };
    const juce::Point<int> groupTo = { grid.getX() + grid.getWidth() / 5, lane.getY() + 8 };
    const yesdaw::engine::Tick groupFromTick = pianoRollTickForLaneX (pianoRoll, midi, groupFrom.x);
    const yesdaw::engine::Tick groupToTick = pianoRollTickForLaneX (pianoRoll, midi, groupTo.x);
    const double groupFromVelocity = pianoRollVelocityForLaneYPixel (lane, groupFrom.y);
    const double groupToVelocity = pianoRollVelocityForLaneYPixel (lane, groupTo.y);
    REQUIRE (groupFromTick > noteA.startTick);
    REQUIRE (groupToTick < noteA.startTick + noteA.lengthTicks);
    REQUIRE (groupToTick < noteB.startTick);
    dragFromTo (pianoRoll, groupFrom, groupTo);
    painted = noteById (noteA.id);
    REQUIRE (painted.normalizedVelocity
             == Catch::Approx (rampVelocity (noteA.startTick, groupFromTick, groupFromVelocity,
                                             groupToTick, groupToVelocity)).margin (1e-9));
    paintedB = noteById (noteB.id);
    REQUIRE (paintedB.normalizedVelocity
             == Catch::Approx (rampVelocity (noteB.startTick, groupFromTick, groupFromVelocity,
                                             groupToTick, groupToVelocity)).margin (1e-9));
    REQUIRE (paintedB.normalizedVelocity == Catch::Approx (groupToVelocity).margin (1e-9));
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (noteById (noteA.id).normalizedVelocity == Catch::Approx (0.55).margin (1e-9));
    REQUIRE (noteById (noteB.id).normalizedVelocity == Catch::Approx (0.82).margin (1e-9));
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

    // E11: the empty-grid pencil needs the Pencil tool (the pencil selects its new note).
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
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

    // E20 re-pin: breakpoints follow the REAL snap chooser now; this gate asserts the raw
    // pixel-exact laws, so it runs with the chooser Off.
    auto* snapChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (snapChooser != nullptr);
    snapChooser->setSelectedId (1, juce::sendNotificationSync);

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

TEST_CASE ("the automation target chooser drives pan and FX-param lanes the render follows",
           "[ui][input][shell][automation-target]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("automation-target");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    clickButton (requireButtonForAction (*shell, UiActionId::TimelineAutomationToggleTrackLane));

    juce::Component* canvas = findChildWithComponentId (*shell, "timeline.automation.canvas");
    auto* targetChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.automation.target"));
    REQUIRE (canvas != nullptr);
    REQUIRE (targetChooser != nullptr);
    REQUIRE (targetChooser->isVisible());
    REQUIRE (targetChooser->getNumItems() == 2);   // Fader + Pan before any sends or FX
    REQUIRE (targetChooser->getItemText (0) == "Fader");
    REQUIRE (targetChooser->getItemText (1) == "Pan");

    const auto renderFromStart = [&shell] {
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
        const std::vector<float> rendered = renderMainComponentPlayback (*shell, 48'000, 128);
        REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
        return rendered;
    };
    const std::vector<float> baseline = renderFromStart();

    // PAN lane on demand: with Pan targeted, a canvas click creates a TrackPan lane whose
    // breakpoint lands on the default Beat grid, and the render audibly follows it.
    targetChooser->setSelectedId (2, juce::sendNotificationSync);
    mouseDownAt (*canvas, { canvas->getWidth() / 3, 1 });   // pan hard toward one side
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.size() == 1u);
    REQUIRE (project.automationLanes.front().role == yesdaw::engine::AutomationTargetRole::TrackPan);
    REQUIRE (project.automationLanes.front().ownerEntity == project.tracks.front().id);
    REQUIRE (project.automationLanes.front().points.size() == 1u);
    REQUIRE (project.automationLanes.front().points.front().tick % 24'000 == 0);
    const std::vector<float> panned = renderFromStart();
    REQUIRE (panned != baseline);

    // FX-PARAM lane on demand: add an EQ, target its band-0 gain (Fader, Pan, type, freq,
    // gain, ...), pull it low — the lane owns the INSERT and the render changes again.
    juce::Component* railComponent = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (railComponent != nullptr);
    mouseDownAt (*railComponent, { railComponent->getWidth() / 2,
                                   yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                       + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });
    auto* fxChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.fx.insert.add"));
    REQUIRE (fxChooser != nullptr);
    fxChooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Eq) + 1, juce::sendNotificationSync);
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));   // back to the Timeline view
    REQUIRE (targetChooser->getNumItems() == 2 + 24);   // the EQ's full param inventory appears
    REQUIRE (targetChooser->getItemText (4).contains ("eq.band.gain"));
    targetChooser->setSelectedId (5, juce::sendNotificationSync);   // FX1 eq.band.gain
    mouseDownAt (*canvas, { (canvas->getWidth() * 2) / 3, canvas->getHeight() - 2 });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.size() == 2u);
    REQUIRE (project.automationLanes.back().role == yesdaw::engine::AutomationTargetRole::FxInsertParam);
    REQUIRE (project.automationLanes.back().ownerEntity
             == project.tracks.front().strip.fxChain.front().id);
    REQUIRE (project.automationLanes.back().paramId == 2u);
    const std::vector<float> eqAutomated = renderFromStart();
    REQUIRE (eqAutomated != panned);

    // SNAP law: chooser Off lands the breakpoint on the raw tick, off the Beat grid.
    auto* snapChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (snapChooser != nullptr);
    snapChooser->setSelectedId (1, juce::sendNotificationSync);
    mouseDownAt (*canvas, { canvas->getWidth() / 5 + 3, canvas->getHeight() / 2 });
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.automationLanes.back().points.size() == 2u);
    bool sawOffGridTick = false;
    for (const yesdaw::engine::AutomationBreakpoint& point : project.automationLanes.back().points)
        sawOffGridTick = sawOffGridTick || (point.tick % 24'000 != 0);
    REQUIRE (sawOffGridTick);

    // The on-demand FX lane creation was ONE undo group: two undos drop both new breakpoints
    // and the lane itself.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).automationLanes.back().points.size() == 1u);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).automationLanes.size() == 1u);
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

    // Pencil: with the Pencil tool (E11), a click on the empty grid creates a note at the
    // clicked key and snapped tick.
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
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
    // Re-pinned to the E3 tool semantics: the Pencil press on an empty lane creates a MIDI clip
    // on that lane and opens the piano roll; the timeline clip selection is untouched (the
    // imported clip stays selected — deselection belongs to the Pointer empty click).
    REQUIRE (snapshotMainComponent (*shell).selectedTimelineClipCount == 1);
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.size() == 1u);
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::PianoRoll);
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    REQUIRE (snapshotMainComponent (*shell).context.activePanel == yesdaw::ui::UiPanel::Timeline);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (readProjectSnapshot (bundlePath).midiClips.empty());

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
    // E4: the committed range endpoints snap through the snap chooser while the mouse-down locate
    // stays raw — the snapped locate IS the range start.
    REQUIRE (snapshot.context.snapEnabled);
    yesdaw::engine::Tick snappedLocate = 0;
    REQUIRE (yesdaw::engine::snapTick (
        static_cast<yesdaw::engine::Tick> (snapshot.context.playheadFrame),
        yesdaw::engine::SnapGrid { snapshot.context.snapGridTicks }, snappedLocate));
    REQUIRE (snappedLocate == rangeStart);

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
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));   // E11: the empty-grid pencil is tool-aware
    mouseDownAt (pianoRoll, { pianoRoll.getWidth() / 2, pianoRoll.getHeight() / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));

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

TEST_CASE ("Shift while dragging makes every fader, pan, and send exactly ten times finer",
           "[ui][input][shell][mixer][fine-drag]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("fine-drag");
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

    const juce::ModifierKeys shiftDrag (
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier);
    const double fine = L::fineDragScale;

    // Rail mini VOL: a plain click coarsely sets ~0.5, then a Shift drag moves exactly
    // fineDragScale of the pointer's proportional travel from the persisted anchor — no jump.
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
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const double railVolAnchor = static_cast<double> (project.tracks.front().strip.linearGain);
    REQUIRE (railVolAnchor < 0.6);

    dragFromTo (*rail, { level.getCentreX(), level.getCentreY() },
                { level.getCentreX() + 10, level.getCentreY() }, shiftDrag);
    project = readProjectSnapshot (bundlePath);
    const double railVolExpected =
        railVolAnchor + (10.0 / level.getWidth()) * 1.0 * fine;
    REQUIRE (project.tracks.front().strip.linearGain
             == Catch::Approx (railVolExpected).epsilon (1e-6));

    // Rail mini PAN: from center, an 8-pixel Shift drag covers exactly fineDragScale of the
    // proportional travel across the [-1, 1] span.
    const juce::Rectangle<int> panKnob =
        row.withRight (row.getRight() - L::trackListPanRightInset)
            .removeFromRight (L::trackListPanDiameter)
            .withY (row.getY() + L::trackListPanTopInset)
            .withHeight (L::trackListPanDiameter);
    dragFromTo (*rail, panKnob.getCentre(), panKnob.getCentre().translated (8, 0), shiftDrag);
    project = readProjectSnapshot (bundlePath);
    const double railPanExpected = (8.0 / panKnob.getWidth()) * 2.0 * fine;
    REQUIRE (project.tracks.front().strip.pan
             == Catch::Approx (railPanExpected).epsilon (1e-6));

    // Mixer fader (vertical, 0.01 interval): the Shift drag lands on the interval grid at
    // exactly fineDragScale of the plain proportional travel.
    auto* fader = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_fader"));
    REQUIRE (fader != nullptr);
    fader->setValue (0.8, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.linearGain == 0.8f);

    const juce::Point<int> faderCentre = fader->getLocalBounds().getCentre();
    dragFromTo (*fader, faderCentre, faderCentre.translated (0, -30), shiftDrag);
    project = readProjectSnapshot (bundlePath);
    const double faderRaw = 0.8
                          + (30.0 / fader->getHeight())
                                * (L::mixerFaderSliderMax - L::mixerFaderSliderMin) * fine;
    const double faderSnapped = L::mixerFaderSliderMin
                              + L::mixerFaderSliderInterval
                                    * std::floor ((faderRaw - L::mixerFaderSliderMin)
                                                      / L::mixerFaderSliderInterval + 0.5);
    REQUIRE (project.tracks.front().strip.linearGain
             == Catch::Approx (faderSnapped).epsilon (1e-6));

    // Mixer pan (rotary, 0.01 interval): the Shift drag tracks (+x, -y) at fineDragScale.
    auto* pan = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_pan"));
    REQUIRE (pan != nullptr);
    pan->setValue (0.2, juce::sendNotificationSync);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().strip.pan == 0.2f);

    const juce::Point<int> panCentre = pan->getLocalBounds().getCentre();
    dragFromTo (*pan, panCentre, panCentre.translated (25, 0), shiftDrag);
    project = readProjectSnapshot (bundlePath);
    const double panRaw = 0.2
                        + (25.0 / pan->getWidth())
                              * (L::mixerPanSliderMax - L::mixerPanSliderMin) * fine;
    const double panSnapped = std::round ((L::mixerPanSliderMin
                                           + L::mixerPanSliderInterval
                                                 * std::floor ((panRaw - L::mixerPanSliderMin)
                                                                   / L::mixerPanSliderInterval + 0.5))
                                          / L::mixerPanSliderInterval)
                            * L::mixerPanSliderInterval;
    REQUIRE (project.tracks.front().strip.pan == Catch::Approx (panSnapped).epsilon (1e-6));

    // Send level (horizontal, continuous): exact fine math, an anchored no-move Shift press
    // never jumps, and the same plain drag moves far more than five times as much.
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));
    auto* sendChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.add"));
    REQUIRE (sendChooser != nullptr);
    sendChooser->setSelectedId (1, juce::sendNotificationSync);
    auto* sendLevel = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.send.0"));
    REQUIRE (sendLevel != nullptr);
    sendLevel->setValue (0.5, juce::sendNotificationSync);

    const juce::Point<int> sendEdge { sendLevel->getLocalBounds().getX() + 1,
                                      sendLevel->getLocalBounds().getCentreY() };
    dragFromTo (*sendLevel, sendEdge, sendEdge, shiftDrag);   // Shift press far off-value: no jump
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.tracks.front().sends.front().linearGain == 0.5f);

    const juce::Point<int> sendCentre = sendLevel->getLocalBounds().getCentre();
    dragFromTo (*sendLevel, sendCentre, sendCentre.translated (20, 0), shiftDrag);
    project = readProjectSnapshot (bundlePath);
    const double sendFineExpected = 0.5 + (20.0 / sendLevel->getWidth()) * 1.0 * fine;
    const float sendFine = project.tracks.front().sends.front().linearGain;
    REQUIRE (sendFine == Catch::Approx (sendFineExpected).epsilon (1e-6));

    sendLevel->setValue (0.5, juce::sendNotificationSync);
    dragFromTo (*sendLevel, sendCentre, sendCentre.translated (20, 0));
    project = readProjectSnapshot (bundlePath);
    const float sendCoarse = project.tracks.front().sends.front().linearGain;
    REQUIRE (std::abs (sendCoarse - 0.5f) > 5.0f * std::abs (sendFine - 0.5f));

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the fader and rail VOL show a live dB readout while dragging, -inf at zero",
           "[ui][input][shell][mixer][db-readout]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("db-readout");
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

    auto* readout = dynamic_cast<juce::Label*> (findChildWithComponentId (*shell, "shell.drag.db"));
    REQUIRE (readout != nullptr);
    REQUIRE_FALSE (readout->isVisible());

    // A programmatic value change never shows the readout — only a real drag does.
    auto* fader = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_fader"));
    REQUIRE (fader != nullptr);
    fader->setValue (0.5, juce::sendNotificationSync);
    REQUIRE_FALSE (readout->isVisible());

    // While the fader drag is held the readout shows 20*log10(gain) of the live value.
    const juce::Point<int> faderCentre = fader->getLocalBounds().getCentre();
    beginDragFromTo (*fader, faderCentre, faderCentre.translated (0, -10));
    REQUIRE (readout->isVisible());
    const double draggedGain = fader->getValue();
    REQUIRE (draggedGain > 0.0);
    REQUIRE (readout->getText()
             == juce::String (20.0 * std::log10 (draggedGain), 1) + " dB");
    releaseDragAt (*fader, faderCentre, faderCentre.translated (0, -10));
    REQUIRE_FALSE (readout->isVisible());

    // Dragging the fader to the bottom reads exact silence as "-inf dB".
    const juce::Point<int> faderBottom { faderCentre.x, fader->getHeight() + 200 };
    beginDragFromTo (*fader, faderCentre, faderBottom);
    REQUIRE (readout->isVisible());
    REQUIRE (fader->getValue() == 0.0);
    REQUIRE (readout->getText() == "-inf dB");
    releaseDragAt (*fader, faderCentre, faderBottom);
    REQUIRE_FALSE (readout->isVisible());

    // The rail VOL mini shares the same readout while its gesture is held.
    juce::Rectangle<int> row = rail->getLocalBounds();
    row.removeFromTop (L::trackListHeaderHeight);
    row = row.withHeight (juce::jmax (L::trackListRowMinHeight, row.getHeight()));
    row.removeFromBottom (L::trackListSeparatorHeight);
    const juce::Rectangle<int> level =
        row.withRight (row.getRight() - L::trackListLevelRightInset)
            .removeFromRight (L::trackListLevelWidth)
            .withBottom (row.getBottom() - L::trackListLevelBottomInset)
            .withHeight (L::trackListLevelHeight);
    beginDragFromTo (*rail, { level.getCentreX(), level.getCentreY() },
                     { level.getCentreX() + 4, level.getCentreY() });
    REQUIRE (readout->isVisible());
    const yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    const float railGain = project.tracks.front().strip.linearGain;
    REQUIRE (railGain > 0.0f);
    REQUIRE (readout->getText()
             == juce::String (20.0 * std::log10 (static_cast<double> (railGain)), 1) + " dB");
    releaseDragAt (*rail, { level.getCentreX(), level.getCentreY() },
                   { level.getCentreX() + 4, level.getCentreY() });
    REQUIRE_FALSE (readout->isVisible());

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("track meters hold peaks for the tick law and latch a clip light that a click clears",
           "[ui][input][shell][mixer][meter-hold]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("meter-hold");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-fullscale.wav";

    // A full-scale square so the post-fader track meter can reach and exceed 0 dBFS.
    constexpr std::uint64_t kFrames = 48'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
        samples[static_cast<std::size_t> (frame)] = (frame / 64u) % 2u == 0u ? 1.0f : -1.0f;
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
    // A second (silent) track: deselecting the clip-owning strip later moves the interactive
    // overlay off its painted meter so the clip light is observable and clickable.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    using L = yesdaw::ui::UiTheme::Layout;
    mouseDownAt (*rail, { rail->getWidth() / 2,
                          L::trackListHeaderHeight + L::trackListRowMinHeight / 2 });

    const auto renderShell = [&shell]
    {
        juce::Image image (juce::Image::ARGB, shell->getWidth(), shell->getHeight(), true);
        juce::Graphics graphics (image);
        shell->paintEntireComponent (graphics, true);
        return image;
    };
    const auto pixelsOfColour = [] (const juce::Image& image,
                                    juce::Rectangle<int> within,
                                    juce::Colour colour)
    {
        int count = 0;
        for (int y = within.getY(); y < within.getBottom(); ++y)
            for (int x = within.getX(); x < within.getRight(); ++x)
                if (image.getPixelAt (x, y) == colour)
                    ++count;
        return count;
    };
    const juce::Colour clipColour = yesdaw::ui::UiTheme::Meter::clipFill();
    const juce::Colour holdColour = yesdaw::ui::UiTheme::Meter::hotFill();

    // Row 0's rail meter rect mirrors the shared rail geometry law (two tracks split the rail).
    juce::Rectangle<int> railRow = rail->getLocalBounds();
    railRow.removeFromTop (L::trackListHeaderHeight);
    railRow = railRow.withHeight (juce::jmax (L::trackListRowMinHeight, railRow.getHeight() / 2));
    railRow.removeFromBottom (L::trackListSeparatorHeight);
    const juce::Rectangle<int> railMeter =
        railRow.withRight (railRow.getRight() - L::trackListMeterRightInset)
            .removeFromRight (L::trackListMeterWidth)
            .reduced (L::trackListMeterHorizontalInset, L::trackListMeterVerticalInset)
            .translated (rail->getX(), rail->getY());
    const juce::Rectangle<int> shellArea = shell->getLocalBounds();

    // At rest no clip light exists anywhere.
    REQUIRE (pixelsOfColour (renderShell(), shellArea, clipColour) == 0);

    // Push the meter past 0 dBFS: fader x2 over the full-scale source, then a UI tick while the
    // transport is still playing samples the live MeterNode peak, holds it, and latches the light.
    auto* fader = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_fader"));
    REQUIRE (fader != nullptr);
    fader->setValue (2.0, juce::sendNotificationSync);
    // The shared fader edit fronts the Mixer panel; return to the Timeline so the rail paints.
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> loud = renderMainComponentPlayback (*shell, 8192, 128);
    REQUIRE (peakAbs (std::span<const float> (loud.data(), loud.size()))
             >= yesdaw::ui::UiTheme::Meter::clipThreshold);
    REQUIRE (serviceMainComponentUiTimer (*shell));
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    // The rail meter shows the latched clip light, which survives stopped silence across ticks
    // until a real click on the rail meter clears it.
    const juce::Image latched = renderShell();
    REQUIRE (pixelsOfColour (latched, railMeter, clipColour) > 0);
    REQUIRE (serviceMainComponentUiTimer (*shell));
    REQUIRE (pixelsOfColour (renderShell(), railMeter, clipColour) > 0);
    mouseDownAt (*rail, railMeter.getCentre().translated (-rail->getX(), -rail->getY()));
    REQUIRE (pixelsOfColour (renderShell(), shellArea, clipColour) == 0);

    // Re-latch, then clear through the painted mixer strip meter instead. The painted strips only
    // have full height in Mixer view; click any latched clip pixel outside the rail, translated
    // into the strip input surface.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    (void) renderMainComponentPlayback (*shell, 8192, 128);
    REQUIRE (serviceMainComponentUiTimer (*shell));
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey)));   // deselect strip 0
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    // In Mixer view the rail does not paint, so every clip-coloured pixel belongs to the painted
    // strip meter's latched light.
    const juce::Image relatched = renderShell();
    juce::Point<int> stripClipPixel { -1, -1 };
    for (int y = shellArea.getY(); y < shellArea.getBottom() && stripClipPixel.x < 0; ++y)
        for (int x = shellArea.getX(); x < shellArea.getRight(); ++x)
            if (relatched.getPixelAt (x, y) == clipColour)
            {
                stripClipPixel = { x, y };
                break;
            }
    REQUIRE (stripClipPixel.x >= 0);
    auto* strips = findChildWithComponentId (*shell, "shell.mixer.strips.input");
    REQUIRE (strips != nullptr);
    mouseDownAt (*strips, stripClipPixel - strips->getPosition());
    REQUIRE (pixelsOfColour (renderShell(), shellArea, clipColour) == 0);
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));               // back to the Timeline
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey)));   // re-select strip 0

    // Peak-hold decays on the tick law: a sub-clip peak paints only the held marker once the
    // transport stops, and expires after exactly the token's tick count.
    fader->setValue (0.35, juce::sendNotificationSync);
    // The shared fader edit deliberately fronts the Mixer panel; return to the Timeline so the
    // rail (whose meter this phase reads) is the painted surface.
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    (void) renderMainComponentPlayback (*shell, 8192, 128);
    REQUIRE (serviceMainComponentUiTimer (*shell));
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (serviceMainComponentUiTimer (*shell));   // live falls silent; the held marker stays
    REQUIRE (pixelsOfColour (renderShell(), railMeter, holdColour) > 0);
    REQUIRE (pixelsOfColour (renderShell(), railMeter, clipColour) == 0);

    for (int tick = 0; tick < yesdaw::ui::UiTheme::Meter::peakHoldTicks; ++tick)
        REQUIRE (serviceMainComponentUiTimer (*shell));
    REQUIRE (pixelsOfColour (renderShell(), railMeter, holdColour) == 0);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("bus meters read live send audio, latch a clip light, and a meter click clears it",
           "[ui][input][shell][mixer][bus-meter]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("bus-meter");
    std::filesystem::path sourcePath = bundlePath;
    sourcePath += "-fullscale.wav";

    // The same full-scale square as the track meter gate: the unity send clips the bus.
    constexpr std::uint64_t kFrames = 48'000;
    std::vector<float> samples (static_cast<std::size_t> (kFrames));
    for (std::uint64_t frame = 0; frame < kFrames; ++frame)
        samples[static_cast<std::size_t> (frame)] = (frame / 64u) % 2u == 0u ? 1.0f : -1.0f;
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

    juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
    REQUIRE (rail != nullptr);
    mouseDownAt (*rail, { rail->getWidth() / 2,
                          yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                              + yesdaw::ui::UiTheme::Layout::trackListRowMinHeight / 2 });
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    clickButton (requireButtonForAction (*shell, UiActionId::MixerBusAdd));
    auto* sendChooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, "mixer.send.add"));
    REQUIRE (sendChooser != nullptr);
    sendChooser->setSelectedId (1, juce::sendNotificationSync);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.front().sends.size() == 1u);

    // Meters read post-pan (0.707 at center), so unity never crosses the clip threshold; the
    // 2x fader pushes both the track and the post-fader send past 0 dBFS (the B32 law).
    auto* fader = dynamic_cast<juce::Slider*> (findChildWithComponentId (*shell, "mixer.target.set_fader"));
    REQUIRE (fader != nullptr);
    fader->setValue (2.0, juce::sendNotificationSync);

    const auto renderShell = [&shell]
    {
        juce::Image image (juce::Image::ARGB, shell->getWidth(), shell->getHeight(), true);
        juce::Graphics graphics (image);
        shell->paintEntireComponent (graphics, true);
        return image;
    };
    const auto clipPixels = [&renderShell] (juce::Rectangle<int> within)
    {
        const juce::Image image = renderShell();
        const juce::Colour clipColour = yesdaw::ui::UiTheme::Meter::clipFill();
        int count = 0;
        for (int y = within.getY(); y < within.getBottom(); ++y)
            for (int x = within.getX(); x < within.getRight(); ++x)
                if (image.getPixelAt (x, y) == clipColour)
                    ++count;
        return count;
    };
    const juce::Rectangle<int> shellArea = shell->getLocalBounds();
    REQUIRE (clipPixels (shellArea) == 0);

    // Playing the full-scale clip through the unity send clips BOTH meters: the model reads a
    // real bus peak and the painted mixer shows TWO latched clip lights.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    (void) renderMainComponentPlayback (*shell, 8192, 128);
    REQUIRE (serviceMainComponentUiTimer (*shell));
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    const int bothLights = clipPixels (shellArea);
    const int oneLightArea = yesdaw::ui::UiTheme::Meter::clipLightSize
                           * yesdaw::ui::UiTheme::Meter::clipLightSize;
    REQUIRE (bothLights > oneLightArea);   // more than a single light: the BUS light exists

    // The bus light is the rightmost clip pixel (bus strips paint after tracks). Clicking it
    // clears ONLY the bus latch; the track light stays.
    const juce::Image latched = renderShell();
    const juce::Colour clipColour = yesdaw::ui::UiTheme::Meter::clipFill();
    juce::Point<int> busClipPixel { -1, -1 };
    for (int y = shellArea.getY(); y < shellArea.getBottom(); ++y)
        for (int x = shellArea.getX(); x < shellArea.getRight(); ++x)
            if (latched.getPixelAt (x, y) == clipColour && x > busClipPixel.x)
                busClipPixel = { x, y };
    REQUIRE (busClipPixel.x >= 0);
    auto* strips = findChildWithComponentId (*shell, "shell.mixer.strips.input");
    REQUIRE (strips != nullptr);
    mouseDownAt (*strips, busClipPixel - strips->getPosition());
    const int trackLightOnly = clipPixels (shellArea);
    REQUIRE (trackLightOnly > 0);
    REQUIRE (trackLightOnly < bothLights);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (sourcePath, ec);
}

TEST_CASE ("Alt+wheel on a piano-roll note edits its velocity undoably and tints the painted note",
           "[ui][input][shell][pianoroll][note-velocity]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("note-velocity");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    // Ctrl+M + pencil: one synth note whose loudness follows its velocity.
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));   // E11: the empty-grid pencil is tool-aware
    const juce::Point<int> noteCentre { pianoRoll.getWidth() / 2, pianoRoll.getHeight() / 2 };
    mouseDownAt (pianoRoll, noteCentre);
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 1u);
    const double startVelocity = project.midiClips.front().notes.front().normalizedVelocity;
    REQUIRE (startVelocity > 0.0);
    const juce::Point<int> notePoint = pianoRollNoteCenterPoint (
        pianoRoll, project.midiClips.front(), project.midiClips.front().notes.front());

    const auto renderEnergy = [&shell]
    {
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
        const std::vector<float> rendered = renderMainComponentPlayback (*shell, 96'000, 512);
        REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
        double energy = 0.0;
        for (const float sample : rendered)
            energy += std::abs (static_cast<double> (sample));
        return energy;
    };
    const double loudEnergy = renderEnergy();
    REQUIRE (loudEnergy > 1.0);

    const auto renderShell = [&shell]
    {
        juce::Image image (juce::Image::ARGB, shell->getWidth(), shell->getHeight(), true);
        juce::Graphics graphics (image);
        shell->paintEntireComponent (graphics, true);
        return image;
    };
    const juce::Image beforeTint = renderShell();

    // One Alt+wheel notch down: velocity drops by exactly deltaY * pianoRollVelocityWheelScale,
    // persisted through the undoable SetNoteVelocity verb.
    juce::MouseWheelDetails wheelDown {};
    wheelDown.deltaY = -0.4f;
    juce::MouseEvent altWheel = makeMouseEvent (pianoRoll, notePoint, notePoint, false, 1,
                                                juce::ModifierKeys (juce::ModifierKeys::altModifier));
    pianoRoll.mouseWheelMove (altWheel, wheelDown);
    project = readProjectSnapshot (bundlePath);
    const double reduced = project.midiClips.front().notes.front().normalizedVelocity;
    REQUIRE (reduced == Catch::Approx (
        std::max (0.0, startVelocity
                           - 0.4 * yesdaw::ui::UiTheme::Layout::pianoRollVelocityWheelScale)));
    REQUIRE (reduced < startVelocity);

    // The painted note visibly darkens with its velocity.
    const juce::Image afterTint = renderShell();
    const juce::Rectangle<int> rollArea = pianoRoll.getBounds();
    int changed = 0;
    for (int y = rollArea.getY(); y < rollArea.getBottom(); ++y)
        for (int x = rollArea.getX(); x < rollArea.getRight(); ++x)
            if (beforeTint.getPixelAt (x, y) != afterTint.getPixelAt (x, y))
                ++changed;
    REQUIRE (changed > 0);

    // The quieter velocity is audible through the real synth.
    const double quietEnergy = renderEnergy();
    REQUIRE (quietEnergy > 0.0);
    REQUIRE (quietEnergy < loudEnergy);

    // A huge wheel-down clamps honestly at silence, and each edit is one undo step.
    juce::MouseWheelDetails wheelFloor {};
    wheelFloor.deltaY = -10.0f;
    pianoRoll.mouseWheelMove (altWheel, wheelFloor);
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.front().normalizedVelocity == 0.0);
    const double silentEnergy = renderEnergy();
    REQUIRE (silentEnergy == 0.0);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.front().normalizedVelocity == Catch::Approx (reduced));
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.front().normalizedVelocity == Catch::Approx (startVelocity));

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("piano-roll keys transpose the note selection and Ctrl+A with Del edit every note",
           "[ui][input][shell][pianoroll][note-keys]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("note-keys");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    // Two penciled notes at different keys and ticks; the second stays selected.
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));   // E11: the empty-grid pencil is tool-aware
    mouseDownAt (pianoRoll, { pianoRoll.getWidth() / 3, pianoRoll.getHeight() / 3 });
    mouseDownAt (pianoRoll, { (pianoRoll.getWidth() * 2) / 3, pianoRoll.getHeight() / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 2u);
    const yesdaw::engine::EntityId firstNoteId = project.midiClips.front().notes[0].id;
    const yesdaw::engine::EntityId secondNoteId = project.midiClips.front().notes[1].id;
    const std::int16_t firstKey = project.midiClips.front().notes[0].key;
    const std::int16_t secondKey = project.midiClips.front().notes[1].key;
    REQUIRE (firstKey != secondKey);

    const auto keyOf = [&] (yesdaw::engine::EntityId noteId) {
        const yesdaw::engine::Project current = readProjectSnapshot (bundlePath);
        for (const yesdaw::engine::Note& note : current.midiClips.front().notes)
            if (note.id == noteId)
                return note.key;
        return std::int16_t { -1 };
    };

    // Up/Down transpose only the selected note by one semitone.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    REQUIRE (keyOf (secondNoteId) == secondKey + 1);
    REQUIRE (keyOf (firstNoteId) == firstKey);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey)));
    REQUIRE (keyOf (secondNoteId) == secondKey);

    // Shift+Up/Down transpose by one octave.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey,
                                                juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (keyOf (secondNoteId) == secondKey + 12);
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::downKey,
                                                juce::ModifierKeys::shiftModifier, 0)));
    REQUIRE (keyOf (secondNoteId) == secondKey);

    // Outside the Piano Roll the arrows keep walking the track rail: no note changes.
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    REQUIRE (keyOf (firstNoteId) == firstKey);
    REQUIRE (keyOf (secondNoteId) == secondKey);
    REQUIRE (shell->keyPressed (juce::KeyPress ('3')));   // back to the Piano Roll

    // Ctrl+A selects every note; Up transposes both as ONE undo step, with audibly changed
    // playback through the real synth.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> before = renderMainComponentPlayback (*shell, 96'000, 512);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));

    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::upKey)));
    REQUIRE (keyOf (firstNoteId) == firstKey + 1);
    REQUIRE (keyOf (secondNoteId) == secondKey + 1);

    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> transposed = renderMainComponentPlayback (*shell, 96'000, 512);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (transposed != before);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (keyOf (firstNoteId) == firstKey);
    REQUIRE (keyOf (secondNoteId) == secondKey);

    // Ctrl+A + Del deletes every note as one group; playback falls truly silent; one undo restores.
    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.empty());
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    const std::vector<float> silent = renderMainComponentPlayback (*shell, 48'000, 512);
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    REQUIRE (peakAbs (std::span<const float> (silent.data(), silent.size())) == 0.0);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 2u);

    // Backspace deletes the selection too.
    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::backspaceKey)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.empty());
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 2u);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Ctrl+drag copy-drags a note and Ctrl+D duplicates it one grid step later",
           "[ui][input][shell][pianoroll][note-duplicate]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("note-duplicate");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    // E12: note gestures snap through the real chooser; this gate pins the RAW copy-drag law,
    // so the chooser goes Off first.
    auto* snapChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (snapChooser != nullptr);
    snapChooser->setSelectedId (1, juce::sendNotificationSync);
    REQUIRE_FALSE (snapshotMainComponent (*shell).context.snapEnabled);
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));   // E11: the empty-grid pencil is tool-aware
    mouseDownAt (pianoRoll, { pianoRoll.getWidth() / 3, pianoRoll.getHeight() / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 1u);
    const yesdaw::engine::Note source = project.midiClips.front().notes.front();
    REQUIRE (source.lengthTicks > 0);   // the pencil note is exactly one grid step long

    const auto renderSamples = [&shell]
    {
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
        const std::vector<float> rendered = renderMainComponentPlayback (*shell, 96'000, 512);
        REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
        return rendered;
    };
    const std::vector<float> oneNote = renderSamples();
    REQUIRE (peakAbs (std::span<const float> (oneNote.data(), oneNote.size())) > 0.01);

    // Ctrl+D: a fresh-id copy lands exactly one grid step (= the pencil note's length) later with
    // every payload field preserved. Adjacent same-key playback rides the engine's same-frame
    // event order (deterministic per persisted note ids), so the audible proof below uses the
    // temporally separated drag copy instead.
    REQUIRE (shell->keyPressed (juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 2u);
    const yesdaw::engine::Note copy = project.midiClips.front().notes.back();
    REQUIRE (copy.id != source.id);
    REQUIRE (copy.startTick == source.startTick + source.lengthTicks);
    REQUIRE (copy.lengthTicks == source.lengthTicks);
    REQUIRE (copy.key == source.key);
    REQUIRE (copy.normalizedVelocity == source.normalizedVelocity);
    REQUIRE (project.midiClips.front().notes.front().startTick == source.startTick);

    // One undo removes exactly the copy.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 1u);
    REQUIRE (project.midiClips.front().notes.front().id == source.id);

    // Ctrl+drag: the original stays put and one fresh copy lands at the drag target, far enough
    // from the source that the second hit audibly changes the rendered playback.
    const juce::Point<int> notePoint = pianoRollNoteCenterPoint (
        pianoRoll, project.midiClips.front(), project.midiClips.front().notes.front());
    dragFromTo (pianoRoll, notePoint, notePoint.translated (80, 0),
                juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier
                                    | juce::ModifierKeys::ctrlModifier));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 2u);
    const yesdaw::engine::Note dragged = project.midiClips.front().notes.back();
    REQUIRE (dragged.id != source.id);
    REQUIRE (dragged.startTick > source.startTick + source.lengthTicks);
    REQUIRE (dragged.key == source.key);
    REQUIRE (dragged.lengthTicks == source.lengthTicks);
    REQUIRE (project.midiClips.front().notes.front().startTick == source.startTick);

    const std::vector<float> withDraggedCopy = renderSamples();
    REQUIRE (withDraggedCopy != oneNote);

    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 1u);
    const std::vector<float> restored = renderSamples();
    REQUIRE (restored == oneNote);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Q quantizes the selected notes to the snap grid as one undo group",
           "[ui][input][shell][pianoroll][note-quantize]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("note-quantize");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    juce::Component& pianoRoll = requirePianoRollComponent (*shell);
    // E12: the pencil floors to the real chooser grid, which would pre-align these notes and
    // leave the quantize nothing to do — pencil RAW (chooser Off) first, then restore Beat.
    auto* snapChooser = dynamic_cast<juce::ComboBox*> (
        findChildWithComponentId (*shell, "timeline.snap.chooser"));
    REQUIRE (snapChooser != nullptr);
    snapChooser->setSelectedId (1, juce::sendNotificationSync);
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));   // E11: the empty-grid pencil is tool-aware
    mouseDownAt (pianoRoll, { pianoRoll.getWidth() / 3, pianoRoll.getHeight() / 3 });
    mouseDownAt (pianoRoll, { (pianoRoll.getWidth() * 2) / 3, pianoRoll.getHeight() / 2 });
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));
    snapChooser->setSelectedId (3, juce::sendNotificationSync);
    REQUIRE (snapshotMainComponent (*shell).context.snapEnabled);
    yesdaw::engine::Project project = readProjectSnapshot (bundlePath);
    REQUIRE (project.midiClips.front().notes.size() == 2u);
    const yesdaw::engine::EntityId firstId = project.midiClips.front().notes[0].id;
    const yesdaw::engine::EntityId secondId = project.midiClips.front().notes[1].id;
    const yesdaw::engine::Tick firstStart = project.midiClips.front().notes[0].startTick;
    const yesdaw::engine::Tick secondStart = project.midiClips.front().notes[1].startTick;

    // The current snap grid (Beat by default) must displace both penciled notes so the quantize
    // has real work; expectations come from the same engine snapTick law the verb uses.
    const yesdaw::engine::SnapGrid grid {
        static_cast<yesdaw::engine::Tick> (snapshotMainComponent (*shell).context.snapGridTicks) };
    yesdaw::engine::Tick firstExpected = 0;
    yesdaw::engine::Tick secondExpected = 0;
    REQUIRE (yesdaw::engine::snapTick (firstStart, grid, firstExpected));
    REQUIRE (yesdaw::engine::snapTick (secondStart, grid, secondExpected));
    REQUIRE (firstExpected != firstStart);
    REQUIRE (secondExpected != secondStart);

    const auto startOf = [&] (yesdaw::engine::EntityId noteId) {
        const yesdaw::engine::Project current = readProjectSnapshot (bundlePath);
        for (const yesdaw::engine::Note& note : current.midiClips.front().notes)
            if (note.id == noteId)
                return note.startTick;
        return yesdaw::engine::Tick { -1 };
    };

    // Q on the single pencil selection quantizes only that note.
    REQUIRE (shell->keyPressed (juce::KeyPress ('q')));
    REQUIRE (startOf (secondId) == secondExpected);
    REQUIRE (startOf (firstId) == firstStart);
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (startOf (secondId) == secondStart);

    // Ctrl+A + Q quantizes both notes as ONE undo step with audibly changed playback.
    const auto renderSamples = [&shell]
    {
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
        REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
        const std::vector<float> rendered = renderMainComponentPlayback (*shell, 96'000, 512);
        REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
        return rendered;
    };
    const std::vector<float> before = renderSamples();

    REQUIRE (shell->keyPressed (juce::KeyPress ('a', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('q')));
    REQUIRE (startOf (firstId) == firstExpected);
    REQUIRE (startOf (secondId) == secondExpected);
    const std::vector<float> quantized = renderSamples();
    REQUIRE (quantized != before);

    // A second Q on the now-aligned selection is an honest no-op.
    const int dispatchesBefore = snapshotMainComponent (*shell).context.commandDispatchCount;
    REQUIRE (shell->keyPressed (juce::KeyPress ('q')));
    REQUIRE (startOf (firstId) == firstExpected);
    REQUIRE (startOf (secondId) == secondExpected);
    REQUIRE (snapshotMainComponent (*shell).context.commandDispatchCount == dispatchesBefore);

    // ONE undo restores both original positions and the original playback bit-identically.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (startOf (firstId) == firstStart);
    REQUIRE (startOf (secondId) == secondStart);
    const std::vector<float> restoredQuantize = renderSamples();
    REQUIRE (restoredQuantize == before);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("closing with unsaved changes prompts Save, Close, or Cancel through the seam",
           "[ui][input][shell][confirm-close]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("confirm-close");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    int chooserCalls = 0;
    int nextChoice = yesdaw::ui::kCloseChoiceCancel;
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };
    choices.confirmCloseUnsavedChanges = [&chooserCalls, &nextChoice] {
        ++chooserCalls;
        return nextChoice;
    };

    auto shell = makeShell (std::move (choices));

    // Without a project (and with a freshly attached clean project) closing needs no prompt.
    REQUIRE (yesdaw::ui::mainComponentConfirmsClose (*shell));
    REQUIRE (chooserCalls == 0);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    REQUIRE (yesdaw::ui::mainComponentConfirmsClose (*shell));
    REQUIRE (chooserCalls == 0);

    // A real edit marks the session dirty; Cancel keeps the app open.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    nextChoice = yesdaw::ui::kCloseChoiceCancel;
    REQUIRE_FALSE (yesdaw::ui::mainComponentConfirmsClose (*shell));
    REQUIRE (chooserCalls == 1);

    // Close-without-saving lets the app close; the always-persisted bundle keeps the edit.
    nextChoice = yesdaw::ui::kCloseChoiceClose;
    REQUIRE (yesdaw::ui::mainComponentConfirmsClose (*shell));
    REQUIRE (chooserCalls == 2);
    REQUIRE (readProjectSnapshot (bundlePath).tracks.size() == 2u);

    // Save closes AND marks the session clean: the very next close needs no prompt.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    nextChoice = yesdaw::ui::kCloseChoiceSave;
    const int savesBefore = snapshotMainComponent (*shell).context.saveCount;
    REQUIRE (yesdaw::ui::mainComponentConfirmsClose (*shell));
    REQUIRE (chooserCalls == 3);
    REQUIRE (snapshotMainComponent (*shell).context.saveCount == savesBefore + 1);
    REQUIRE (yesdaw::ui::mainComponentConfirmsClose (*shell));
    REQUIRE (chooserCalls == 3);

    // An explicit Ctrl+S also cleans the session, so closing stays silent.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (yesdaw::ui::mainComponentConfirmsClose (*shell));
    REQUIRE (chooserCalls == 3);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the window title carries the project name and a dirty marker until save",
           "[ui][input][shell][dirty-title]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("dirty-title");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));

    // Without a project the shell keeps the app's versioned startup title (empty here).
    REQUIRE (snapshotMainComponent (*shell).windowTitle.empty());

    // A freshly created project shows its bundle name, clean.
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    const std::string cleanTitle = bundlePath.stem().string() + " - YES DAW";
    const std::string dirtyTitle = bundlePath.stem().string() + "* - YES DAW";
    REQUIRE (snapshotMainComponent (*shell).windowTitle == cleanTitle);

    // A real edit raises the dirty marker; an explicit save clears it.
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).windowTitle == dirtyTitle);
    REQUIRE (shell->keyPressed (juce::KeyPress ('s', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).windowTitle == cleanTitle);

    // Any later edit is dirty again — including through undo, which is itself an edit.
    REQUIRE (shell->keyPressed (juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (snapshotMainComponent (*shell).windowTitle == dirtyTitle);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the File menu lists recent project bundles most recent first and reopens them",
           "[ui][input][shell][open-recent]")
{
    const std::filesystem::path sessionDir = makeTempBundlePath ("open-recent-session");
    const std::filesystem::path bundleA = makeTempBundlePath ("open-recent-a");
    const std::filesystem::path bundleB = makeTempBundlePath ("open-recent-b");

    std::filesystem::path nextNewBundle = bundleA;
    MainComponentFileChoices choices;
    choices.sessionStateDirectory = sessionDir;
    choices.chooseNewProjectBundle = [&nextNewBundle] { return nextNewBundle; };

    auto shell = makeShell (std::move (choices));

    // Create A then B: the MRU lists both, most recent first.
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    nextNewBundle = bundleB;
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    auto* bar = dynamic_cast<juce::MenuBarComponent*> (findChildWithComponentId (*shell, "shell.menubar"));
    REQUIRE (bar != nullptr);
    juce::MenuBarModel* model = bar->getModel();
    REQUIRE (model != nullptr);

    const auto recentEntries = [&model]
    {
        std::vector<juce::String> entries;
        juce::PopupMenu fileMenu = model->getMenuForIndex (0, "File");
        for (juce::PopupMenu::MenuItemIterator it (fileMenu); it.next();)
        {
            if (it.getItem().subMenu != nullptr)
                for (juce::PopupMenu::MenuItemIterator sub (*it.getItem().subMenu); sub.next();)
                    entries.push_back (sub.getItem().text);
        }
        return entries;
    };

    std::vector<juce::String> entries = recentEntries();
    REQUIRE (entries.size() == 2u);
    REQUIRE (entries[0] == juce::String (bundleB.stem().string()));
    REQUIRE (entries[1] == juce::String (bundleA.stem().string()));

    // Selecting the older entry reopens A through the real menu path; the title follows, and A
    // moves back to the top of the MRU.
    model->menuItemSelected (1001, 0);   // second recent row = bundle A
    REQUIRE (snapshotMainComponent (*shell).windowTitle
             == bundleA.stem().string() + " - YES DAW");
    entries = recentEntries();
    REQUIRE (entries.size() == 2u);
    REQUIRE (entries[0] == juce::String (bundleA.stem().string()));
    REQUIRE (entries[1] == juce::String (bundleB.stem().string()));

    std::error_code ec;
    std::filesystem::remove_all (sessionDir, ec);
    std::filesystem::remove_all (bundleA, ec);
    std::filesystem::remove_all (bundleB, ec);
}

TEST_CASE ("every componentID'd control carries a tooltip naming its action and chord",
           "[ui][input][shell][tooltips]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("tooltips");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    int checkedControls = 0;
    std::function<void (juce::Component&)> visit = [&] (juce::Component& parent)
    {
        for (int i = 0; i < parent.getNumChildComponents(); ++i)
        {
            juce::Component& child = *parent.getChildComponent (i);
            const juce::String id = child.getComponentID();
            if (id.isNotEmpty())
            {
                INFO ("component id: " << id.toStdString());
                auto* client = dynamic_cast<juce::TooltipClient*> (&child);
                REQUIRE (client != nullptr);
                REQUIRE (client->getTooltip().isNotEmpty());

                // Drift-proofing: an action-backed control's tooltip carries the LIVE chord from
                // the descriptor table.
                if (const auto* descriptor = yesdaw::ui::descriptorForStableId (id.toStdString()))
                    REQUIRE (client->getTooltip().contains (descriptor->defaultKey));

                ++checkedControls;
            }
            visit (child);
        }
    };
    visit (*shell);

    // The walk saw the real control population, not an empty shell.
    REQUIRE (checkedControls > 50);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the painted SNAP caption clears the snap and repeat-paste choosers",
           "[ui][input][shell][snap-label]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("snap-label");
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };

    auto shell = makeShell (std::move (choices));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    // The painted caption rect comes from the same geometry helper and tokens the painter uses.
    juce::Component& timeline = requireTimelineComponent (*shell);
    yesdaw::ui::TimelineCanvasState state;
    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timeline.getLocalBounds(), state);
    using L = yesdaw::ui::UiTheme::Layout;
    const juce::Rectangle<int> labelInShell =
        geometry.toolbarArea.withTrimmedLeft (L::timelineCanvasSnapLabelX)
            .withWidth (L::timelineCanvasSnapLabelWidth)
            .translated (timeline.getX(), timeline.getY());

    auto* snapChooser = findChildWithComponentId (*shell, "timeline.snap.chooser");
    auto* repeatChooser = findChildWithComponentId (*shell, "timeline.repeat-paste.chooser");
    REQUIRE (snapChooser != nullptr);
    REQUIRE (repeatChooser != nullptr);
    INFO ("label " << labelInShell.toString().toStdString()
          << " snap " << snapChooser->getBounds().toString().toStdString()
          << " repeat " << repeatChooser->getBounds().toString().toStdString());
    REQUIRE_FALSE (labelInShell.intersects (snapChooser->getBounds()));
    REQUIRE_FALSE (labelInShell.intersects (repeatChooser->getBounds()));

    // The caption reads as the snap chooser's label: right of the repeat-paste chooser,
    // left of the snap chooser.
    REQUIRE (labelInShell.getX() >= repeatChooser->getBounds().getRight());
    REQUIRE (labelInShell.getRight() <= snapChooser->getBounds().getX());

    // The repeat-paste chooser clears the painted tool cells on its left.
    const juce::Rectangle<int> toolCellsInShell =
        geometry.toolbarArea.withTrimmedLeft (yesdaw::ui::UiTheme::Space::xl)
            .withWidth (5 * L::timelineCanvasToolCellWidth)
            .translated (timeline.getX(), timeline.getY());
    REQUIRE_FALSE (toolCellsInShell.intersects (repeatChooser->getBounds()));

    // And the whole cluster still fits: the automation toggle sits right of the snap chooser
    // and inside the timeline header.
    const juce::Rectangle<int> toggleBounds = L::automationLaneToggleBounds (timeline.getBounds());
    REQUIRE (toggleBounds.getX() >= snapChooser->getBounds().getRight());
    REQUIRE (toggleBounds.getRight() <= timeline.getBounds().getRight());

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}
