// YES DAW - H12 real-shell UI input harness skeleton.

#include "ui/MainComponent.h"
#include "ui/TimelineCanvas.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

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

juce::Component& requireTimelineComponent (juce::Component& shell)
{
    juce::Component* component = findChildWithComponentId (shell, kTimelineComponentId);
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
                                 int numberOfClicks = 1)
{
    const juce::Time now = juce::Time::getCurrentTime();
    return juce::MouseEvent (
        juce::Desktop::getInstance().getMainMouseSource(),
        position.toFloat(),
        juce::ModifierKeys::leftButtonModifier,
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

void dragFromTo (juce::Component& component, juce::Point<int> start, juce::Point<int> end)
{
    juce::MouseEvent down = makeMouseEvent (component, start, start, false);
    component.mouseDown (down);

    juce::MouseEvent drag = makeMouseEvent (component, end, start, true);
    component.mouseDrag (drag);

    juce::MouseEvent up = makeMouseEvent (component, end, start, true);
    component.mouseUp (up);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

void doubleClickAt (juce::Component& component, juce::Point<int> position)
{
    juce::MouseEvent event = makeMouseEvent (component, position, position, false, 2);
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

} // namespace

TEST_CASE ("H12 UI input harness constructs the shipped MainComponent", "[ui][input][shell]")
{
    auto shell = makeShell();
    const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);

    REQUIRE (snapshot.isMainComponent);
    REQUIRE (snapshot.childCount == static_cast<int> (mainShellToolbarActions().size() + 1u));
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

TEST_CASE ("H12 UI input harness imports WAV into the Project bundle and proves audible playback",
           "[ui][input][shell][project][import][playback]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("import-wav");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

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

    const std::filesystem::path bundledAssetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash);
    REQUIRE (readBytes (bundledAssetPath) == readBytes (fixturePath));

    juce::Button& play = requireButtonForAction (*shell, UiActionId::TransportPlay);
    REQUIRE (play.isEnabled());
    clickButton (play);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);

    const std::vector<float> rendered = renderMainComponentPlayback (*shell, 512, 128);
    REQUIRE (rendered.size() == 1024u);
    REQUIRE (peakAbs (std::span<const float> (rendered.data(), rendered.size())) > 0.01);
}
