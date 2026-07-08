// YES DAW - H12 real-shell UI input harness skeleton.

#include "ui/MainComponent.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiTheme.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include "engine/Time.h"
#include "persistence/ProjectBundle.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
using yesdaw::ui::snapshotMainComponent;

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

void mouseDownAt (juce::Component& component, juce::Point<int> position)
{
    juce::MouseEvent event = makeMouseEvent (component, position, position, false);
    component.mouseDown (event);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

void dragFromTo (juce::Component& component,
                 juce::Point<int> start,
                 juce::Point<int> end,
                 juce::ModifierKeys modifiers = juce::ModifierKeys::leftButtonModifier)
{
    juce::MouseEvent down = makeMouseEvent (component, start, start, false, 1, modifiers);
    component.mouseDown (down);

    juce::MouseEvent drag = makeMouseEvent (component, end, start, true, 1, modifiers);
    component.mouseDrag (drag);

    juce::MouseEvent up = makeMouseEvent (component, end, start, true, 1, modifiers);
    component.mouseUp (up);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
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
        clips.push_back ({ static_cast<int> (i), 0, startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = 1;
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
        clips.push_back ({ static_cast<int> (i), 0, startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = 1;
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
        clips.push_back ({ static_cast<int> (i), 0, startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = 1;
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
        clips.push_back ({ static_cast<int> (i), 0, startSeconds, lengthSeconds });
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    yesdaw::ui::TimelineCanvasState state;
    state.trackCount = 1;
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
    auto shell = makeShell();
    const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);

    REQUIRE (snapshot.isMainComponent);
    REQUIRE (snapshot.childCount == static_cast<int> (mainShellToolbarActions().size() + 19u));
    REQUIRE_FALSE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.activePanel == UiPanel::Timeline);
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

    REQUIRE_FALSE (automation.isEnabled());
    REQUIRE_FALSE (laneRow.isVisible());
    REQUIRE_FALSE (addPointComponent->isVisible());

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE_FALSE (snapshot.context.timelineAutomationTrackLaneVisible);
    REQUIRE (automation.isEnabled());
    REQUIRE_FALSE (laneRow.isVisible());
    REQUIRE_FALSE (addPointComponent->isVisible());

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

    clickButton (automation);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.timelineAutomationTrackLaneVisible);
    REQUIRE (snapshot.context.timelineAutomationTrackIndex == -1);
    REQUIRE (snapshot.context.timelineAutomationShowHideCount == 2);
    REQUIRE_FALSE (laneRow.isVisible());
    REQUIRE_FALSE (addPoint.isVisible());
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
    REQUIRE_FALSE (snapshot.playbackReady);
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
    REQUIRE_FALSE (snapshot.playbackReady);
    REQUIRE (snapshot.bundlePath == bundlePath);
    REQUIRE (snapshot.context.saveCount == 0);
    REQUIRE (snapshot.context.commandDispatchCount == 1);

    const yesdaw::engine::Project reopened = readProjectSnapshot (bundlePath);
    REQUIRE (reopened.id == created.id);
    REQUIRE (reopened.sampleRate == created.sampleRate);
    REQUIRE (reopened.tempoMap == created.tempoMap);
    REQUIRE (reopened.meterMap == created.meterMap);

    juce::Button& play = requireButtonForAction (*shell, UiActionId::TransportPlay);
    REQUIRE_FALSE (play.isEnabled());
    clickButton (play);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.commandDispatchCount == 1);

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
    REQUIRE_FALSE (snapshot.playbackReady);

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

    dragFromTo (timeline, { 30, 100 }, { 34, 100 });

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

    const juce::Point<int> snapStart = timelineClipCenterPoint (timeline, fadeOutRedone, 0u);
    constexpr int kSnapDeltaPixels = 9;
    constexpr yesdaw::engine::SnapGrid kSnapGrid { 512 };
    const double unsnappedSeconds = static_cast<double> (fadeOutRedone.clips[0].timelineStart)
                                      / fadeOutRedone.sampleRate.hz
                                  + static_cast<double> (kSnapDeltaPixels)
                                      / timelinePixelsPerSecond (timeline, fadeOutRedone);
    const yesdaw::engine::Tick unsnappedTick =
        static_cast<yesdaw::engine::Tick> (std::llround (unsnappedSeconds * fadeOutRedone.sampleRate.hz));
    yesdaw::engine::Tick expectedSnappedStart = 0;
    REQUIRE (yesdaw::engine::snapTick (unsnappedTick, kSnapGrid, expectedSnappedStart));
    expectedSnappedStart = std::max<yesdaw::engine::Tick> (0, expectedSnappedStart);
    REQUIRE (expectedSnappedStart != fadeOutRedone.clips[0].timelineStart);

    const juce::ModifierKeys ctrlDrag {
        juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::ctrlModifier
    };
    dragFromTo (timeline, snapStart, snapStart.translated (kSnapDeltaPixels, 0), ctrlDrag);

    const yesdaw::engine::Project snapped = readProjectSnapshot (bundlePath);
    REQUIRE (snapped.clips.size() == 2u);
    REQUIRE (snapped.clips[1] == fadeOutRedone.clips[1]);
    REQUIRE (snapped.clips[0].id == fadeOutRedone.clips[0].id);
    REQUIRE (snapped.clips[0].timelineStart == expectedSnappedStart);
    REQUIRE (snapped.clips[0].timelineStart % kSnapGrid.intervalTicks == 0);
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
    REQUIRE (afterMixLeftPeak < beforeMixLeftPeak);
    REQUIRE (afterMixRightPeak < beforeMixRightPeak);
    REQUIRE (afterMixLeftPeak > afterMixRightPeak);

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
    REQUIRE_FALSE (snapshot.playbackReady);
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
