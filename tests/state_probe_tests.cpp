// YES DAW - G0.1 State probe gate (ADR-0046 §10; plan §7.2).
//
// The State probe is the debug-only JSON document the Session drive reads to assert what the real
// shell did. This gate pins its contract on the SHIPPED MainComponent (the same construction the
// H12 UI input harness uses): schema version, the required sections, the element ids in the
// `layout` map (widgets by component id, lanes, rail rows, clips by hex id), that the published
// clip rect is click-accurate (clicking its centre selects that clip through the real hit path),
// that transport/lastAction/timing counters move, and the "never in a normal launch" law.

#include "engine/Project.h"
#include "ui/MainComponent.h"
#include "ui/UiActions.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

using yesdaw::ui::MainComponentFileChoices;
using yesdaw::ui::UiActionId;

namespace {

std::filesystem::path makeTempPath (std::string label, const char* extension)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("yesdaw-state-probe-" + std::move (label) + "-" + std::to_string (ticks) + extension);
    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

std::unique_ptr<juce::Component> makeShell (MainComponentFileChoices choices)
{
    juce::MessageManager::getInstance();
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    return shell;
}

void tick (juce::Component& shell, int times = 1)
{
    for (int i = 0; i < times; ++i)
        REQUIRE (yesdaw::ui::serviceMainComponentUiTimer (shell));
}

juce::var probeOf (juce::Component& shell)
{
    const std::string json = yesdaw::ui::mainComponentStateProbeJson (shell);
    REQUIRE_FALSE (json.empty());
    juce::var parsed;
    REQUIRE (juce::JSON::parse (juce::String (json), parsed).wasOk());
    REQUIRE (parsed.isObject());
    return parsed;
}

juce::var probeFromFile (const std::filesystem::path& path)
{
    juce::var parsed;
    const juce::File file (juce::String (path.string()));
    REQUIRE (file.existsAsFile());
    REQUIRE (juce::JSON::parse (file.loadFileAsString(), parsed).wasOk());
    REQUIRE (parsed.isObject());
    return parsed;
}

bool hasKey (const juce::var& object, const char* key)
{
    return object.isObject() && object.getDynamicObject()->hasProperty (key);
}

juce::Rectangle<int> rectOf (const juce::var& layout, const juce::String& key)
{
    INFO ("layout key: " << key);
    REQUIRE (layout.hasProperty (key));
    const juce::var rect = layout[juce::Identifier (key)];
    REQUIRE (rect.isArray());
    REQUIRE (rect.size() == 4);
    return { static_cast<int> (rect[0]), static_cast<int> (rect[1]),
             static_cast<int> (rect[2]), static_cast<int> (rect[3]) };
}

std::string hexOf (const yesdaw::engine::EntityId& id)
{
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    for (const std::uint8_t byte : id.bytes)
    {
        out.push_back (kDigits[byte >> 4u]);
        out.push_back (kDigits[byte & 0x0Fu]);
    }
    return out;
}

juce::Button& requireButtonForAction (juce::Component& shell, UiActionId action)
{
    juce::Component* component = yesdaw::ui::findMainComponentChildForAction (shell, action);
    REQUIRE (component != nullptr);
    auto* button = dynamic_cast<juce::Button*> (component);
    REQUIRE (button != nullptr);
    return *button;
}

void clickButton (juce::Button& button)
{
    button.triggerClick();
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

MainComponentFileChoices projectChoices (const std::filesystem::path& bundlePath,
                                         const std::filesystem::path& probePath)
{
    MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [] { return std::filesystem::path { YESDAW_WAV_FIXTURE_PATH }; };
    choices.sessionStateDirectory = makeTempPath ("session", "");
    choices.stateProbePath = probePath;
    return choices;
}

} // namespace

TEST_CASE ("state probe: schema v1 with the required sections and element ids",
           "[ui][state-probe][schema]")
{
    const std::filesystem::path probePath = makeTempPath ("schema", ".json");
    MainComponentFileChoices choices;
    choices.sessionStateDirectory = makeTempPath ("session", "");
    choices.stateProbePath = probePath;
    auto shell = makeShell (std::move (choices));

    // The timer tick writes the file; the in-process document is the SAME string.
    tick (*shell, 3);
    const juce::var fromFile = probeFromFile (probePath);
    const juce::var probe = probeOf (*shell);

    REQUIRE (static_cast<int> (probe["version"]) == 1);
    REQUIRE (static_cast<int> (fromFile["version"]) == 1);
    REQUIRE (static_cast<juce::int64> (fromFile["tick"]) == 3);
    for (const char* key : { "version", "tick", "uptimeMs", "renderer", "windowTitle", "projectLoaded",
                             "bundlePath", "window", "displayScale", "transport", "selection",
                             "focusContext", "focusOwner", "textEditorActive", "lastAction",
                             "commandDispatchCount", "status", "view", "frame", "audio", "layout" })
    {
        INFO ("required key: " << key);
        REQUIRE (hasKey (probe, key));
    }

    // Headless harness: no peer, no desktop audio — the probe says so instead of guessing.
    REQUIRE (probe["renderer"].toString() == "none");
    REQUIRE (probe["focusContext"].toString() == "Arrange");
    REQUIRE_FALSE (static_cast<bool> (probe["projectLoaded"]));
    REQUIRE_FALSE (static_cast<bool> (probe["transport"]["isPlaying"]));
    REQUIRE (static_cast<juce::int64> (probe["transport"]["playheadFrame"]) == 0);
    REQUIRE (probe["selection"]["clips"].isArray());
    REQUIRE (probe["selection"]["clips"].size() == 0);
    REQUIRE (probe["selection"]["timeRange"].isVoid());
    REQUIRE (static_cast<juce::int64> (probe["audio"]["callbackAdds"]) == 0);
    REQUIRE (static_cast<juce::int64> (probe["audio"]["callbackRemovals"]) == 0);
    // G0.3 fields: suspend requests, retired audio objects awaiting the janitor, device blocks.
    REQUIRE (static_cast<juce::int64> (probe["audio"]["suspendRequests"]) == 0);
    REQUIRE (static_cast<juce::int64> (probe["audio"]["retiredObjects"]) == 0);
    REQUIRE (static_cast<juce::int64> (probe["audio"]["deviceBlocks"]) == 0);
    REQUIRE (probe["view"]["activePanel"].toString() == "Arrange");
    REQUIRE (probe["view"]["tool"].toString() == "Pointer");
    REQUIRE (static_cast<int> (probe["view"]["width"]) == shell->getWidth());
    REQUIRE (static_cast<int> (probe["view"]["height"]) == shell->getHeight());

    // The layout map: panels, and every visible identified widget by its component id. Toolbar
    // buttons publish their action's stable id, so a Session script clicks `widget.transport.play`.
    const juce::var layout = probe["layout"];
    REQUIRE (layout.isObject());
    const juce::Rectangle<int> shellBounds = shell->getLocalBounds();
    for (const char* key : { "header", "rail", "timeline", "inspector", "dock",
                             "widget.transport.play", "widget.transport.stop",
                             "widget.project.import_audio", "widget.edit.undo",
                             "widget.timeline.snap.chooser", "widget.shell.menubar" })
    {
        const juce::Rectangle<int> rect = rectOf (layout, key);
        INFO ("layout key: " << key << " rect " << rect.toString());
        REQUIRE (rect.getWidth() > 0);
        REQUIRE (rect.getHeight() > 0);
        REQUIRE (shellBounds.contains (rect));
    }

    // Every published widget rect is the SAME rect the child component occupies (no drift).
    const juce::Rectangle<int> play = rectOf (layout, "widget.transport.play");
    REQUIRE (play == requireButtonForAction (*shell, UiActionId::TransportPlay).getBounds());

    // No project: no lanes, no clips, no ruler in the map (nothing is invented — D3).
    REQUIRE_FALSE (layout.hasProperty ("lane.0"));
    REQUIRE_FALSE (layout.hasProperty ("ruler"));
}

TEST_CASE ("state probe: lanes and clips by id, and the published clip rect is click-accurate",
           "[ui][state-probe][layout]")
{
    const std::filesystem::path bundlePath = makeTempPath ("layout", ".yesdaw");
    const std::filesystem::path probePath = makeTempPath ("layout", ".json");
    auto shell = makeShell (projectChoices (bundlePath, probePath));

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    tick (*shell);

    juce::var probe = probeOf (*shell);
    REQUIRE (static_cast<bool> (probe["projectLoaded"]));
    REQUIRE (static_cast<int> (probe["view"]["trackCount"]) == 1);
    REQUIRE (static_cast<int> (probe["view"]["clipCount"]) == 1);
    REQUIRE (probe["lastAction"].toString() == "project.import_audio");
    REQUIRE (static_cast<juce::int64> (probe["audio"]["rebuilds"]) >= 1);

    // The imported clip's id, from the model the shell actually holds.
    const yesdaw::ui::MainComponentSnapshot snapshot = yesdaw::ui::snapshotMainComponent (*shell);
    REQUIRE (snapshot.visibleTimelineClipCount == 1);
    yesdaw::engine::Project project;
    {
        // Read the bundle the shell wrote so the id is the persisted one, never a guess.
        yesdaw::ui::UiAppModel model;
        REQUIRE (model.openProjectBundle (bundlePath).ok());
        project = model.project();
    }
    REQUIRE (project.clips.size() == 1u);
    const std::string clipKey = "clip." + hexOf (project.clips.front().id);

    juce::var layout = probe["layout"];
    const juce::Rectangle<int> timeline = rectOf (layout, "timeline");
    const juce::Rectangle<int> ruler = rectOf (layout, "ruler");
    const juce::Rectangle<int> clipArea = rectOf (layout, "clipArea");
    const juce::Rectangle<int> lane0 = rectOf (layout, "lane.0");
    const juce::Rectangle<int> railRow0 = rectOf (layout, "rail.row.0");
    const juce::Rectangle<int> clipRect = rectOf (layout, clipKey.c_str());
    REQUIRE (timeline.contains (ruler));
    REQUIRE (timeline.contains (clipArea));
    REQUIRE (clipArea.contains (lane0));
    REQUIRE (lane0.contains (clipRect));
    REQUIRE (rectOf (layout, "rail").contains (railRow0));
    REQUIRE (railRow0.getHeight() > 0);
    REQUIRE_FALSE (layout.hasProperty ("lane.1"));

    // Import selects what it placed (shipped behaviour); the probe reports exactly that.
    REQUIRE (probe["selection"]["clips"].size() == 1);
    REQUIRE (probe["selection"]["clips"][0].toString().toStdString() == hexOf (project.clips.front().id));

    // Clicks go through the real hit path: the deepest visible child at the published shell
    // point, walked the way JUCE's own getComponentAt walks (top-most child first, hitTest
    // honoured) — the harness shell has no parent and no peer, so Component::contains() is
    // false at the top and getComponentAt() itself cannot be used here.
    const auto clickAt = [&shell] (juce::Point<int> shellPoint) {
        juce::Component* target = shell.get();
        juce::Point<int> walkPoint = shellPoint;
        for (bool descended = true; descended;)
        {
            descended = false;
            for (int i = target->getNumChildComponents(); --i >= 0;)
            {
                juce::Component* child = target->getChildComponent (i);
                if (child == nullptr || ! child->isVisible() || ! child->getBounds().contains (walkPoint))
                    continue;
                const juce::Point<int> childLocal = walkPoint - child->getPosition();
                if (! child->hitTest (childLocal.x, childLocal.y))
                    continue;
                target = child;
                walkPoint = childLocal;
                descended = true;
                break;
            }
        }
        REQUIRE (target != shell.get());
        const juce::Point<float> local = target->getLocalPoint (shell.get(), shellPoint.toFloat());
        const juce::MouseEvent down (juce::Desktop::getInstance().getMainMouseSource(), local,
                                     juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     target, target, juce::Time::getCurrentTime(), local,
                                     juce::Time::getCurrentTime(), 1, false);
        target->mouseDown (down);
        const juce::MouseEvent up (juce::Desktop::getInstance().getMainMouseSource(), local,
                                   juce::ModifierKeys(), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                   target, target, juce::Time::getCurrentTime(), local,
                                   juce::Time::getCurrentTime(), 1, false);
        target->mouseUp (up);
        (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    };

    // Empty lane (the published lane rect, right of the clip) clears the selection ...
    REQUIRE (lane0.getRight() - clipRect.getRight() > 8);
    clickAt ({ lane0.getRight() - 4, lane0.getCentreY() });
    tick (*shell);
    probe = probeOf (*shell);
    REQUIRE (probe["selection"]["clips"].size() == 0);

    // ... and the CENTRE of the published clip rect selects exactly that clip again.
    clickAt (clipRect.getCentre());
    tick (*shell);
    probe = probeOf (*shell);
    REQUIRE (probe["selection"]["clips"].size() == 1);
    REQUIRE (probe["selection"]["clips"][0].toString().toStdString() == hexOf (project.clips.front().id));

    // Transport and lastAction follow the shipped key path (the SAME keyPressed the window uses).
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::spaceKey)));
    tick (*shell);
    probe = probeOf (*shell);
    REQUIRE (static_cast<bool> (probe["transport"]["isPlaying"]));
    REQUIRE (probe["lastAction"].toString() == "transport.toggle_play_stop");   // G0.2: Space toggles
    REQUIRE (shell->keyPressed (juce::KeyPress ('k')));
    tick (*shell);
    probe = probeOf (*shell);
    REQUIRE_FALSE (static_cast<bool> (probe["transport"]["isPlaying"]));
    REQUIRE (probe["lastAction"].toString() == "transport.stop");

    // The file on disk carries the same document the in-process call returned.
    const juce::var fromFile = probeFromFile (probePath);
    REQUIRE (fromFile["lastAction"].toString() == "transport.stop");
    REQUIRE (fromFile["layout"].hasProperty (clipKey.c_str()));
}

TEST_CASE ("state probe: frame and action-to-paint counters move when the shell paints",
           "[ui][state-probe][frame]")
{
    const std::filesystem::path probePath = makeTempPath ("frame", ".json");
    MainComponentFileChoices choices;
    choices.sessionStateDirectory = makeTempPath ("session", "");
    choices.stateProbePath = probePath;
    auto shell = makeShell (std::move (choices));

    juce::var probe = probeOf (*shell);
    REQUIRE (static_cast<juce::int64> (probe["frame"]["paintCount"]) == 0);
    REQUIRE (static_cast<double> (probe["frame"]["actionToPaintMs"]) < 0.0);   // no action yet

    // An action opens the stamp; the next completed paint closes it.
    REQUIRE (shell->keyPressed (juce::KeyPress (juce::KeyPress::homeKey)));
    {
        juce::Image image (juce::Image::ARGB, shell->getWidth(), shell->getHeight(), true);
        juce::Graphics g (image);
        shell->paintEntireComponent (g, false);
    }
    tick (*shell);
    probe = probeOf (*shell);
    REQUIRE (static_cast<juce::int64> (probe["frame"]["paintCount"]) == 1);
    REQUIRE (static_cast<double> (probe["frame"]["paintMs"]) >= 0.0);
    REQUIRE (static_cast<double> (probe["frame"]["paintP95Ms"]) >= 0.0);
    REQUIRE (static_cast<double> (probe["frame"]["actionToPaintMs"]) >= 0.0);
    REQUIRE (static_cast<double> (probe["frame"]["tickMs"]) >= 0.0);
    REQUIRE (probe["lastAction"].toString() == "transport.locate_start");
}

TEST_CASE ("state probe: never written in a normal launch, and empty for a non-shell component",
           "[ui][state-probe][negative]")
{
    const std::filesystem::path sessionDir = makeTempPath ("session", "");
    MainComponentFileChoices choices;
    choices.sessionStateDirectory = sessionDir;
    auto shell = makeShell (std::move (choices));   // stateProbePath left empty
    tick (*shell, 5);

    // Nothing named like a probe appears anywhere the shell writes.
    std::error_code ec;
    std::filesystem::create_directories (sessionDir, ec);
    for (const auto& entry : std::filesystem::directory_iterator (sessionDir, ec))
        REQUIRE (entry.path().extension() != ".json");

    // Negative control for the seam itself: a plain component has no probe document.
    juce::Component plain;
    REQUIRE (yesdaw::ui::mainComponentStateProbeJson (plain).empty());
}
