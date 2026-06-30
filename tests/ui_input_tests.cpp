// YES DAW - H12 real-shell UI input harness skeleton.

#include "ui/MainComponent.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

using yesdaw::ui::MainComponentSnapshot;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiPanel;
using yesdaw::ui::findMainComponentChildForAction;
using yesdaw::ui::mainShellToolbarActions;
using yesdaw::ui::snapshotMainComponent;

namespace {

std::unique_ptr<juce::Component> makeShell()
{
    juce::MessageManager::getInstance();
    auto shell = yesdaw::ui::createMainComponent();
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

} // namespace

TEST_CASE ("H12 UI input harness constructs the shipped MainComponent", "[ui][input][shell]")
{
    auto shell = makeShell();
    const MainComponentSnapshot snapshot = snapshotMainComponent (*shell);

    REQUIRE (snapshot.isMainComponent);
    REQUIRE (snapshot.childCount == static_cast<int> (mainShellToolbarActions().size()));
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

TEST_CASE ("H12 UI input harness clicks real shell Components for basic session actions", "[ui][input][shell]")
{
    auto shell = makeShell();

    juce::Button& newProject = requireButtonForAction (*shell, UiActionId::ProjectNew);
    clickButton (newProject);

    MainComponentSnapshot snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.projectLoaded);
    REQUIRE (snapshot.context.commandDispatchCount == 1);

    juce::Button& play = requireButtonForAction (*shell, UiActionId::TransportPlay);
    REQUIRE (play.isEnabled());
    clickButton (play);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.commandDispatchCount == 2);

    juce::Button& stop = requireButtonForAction (*shell, UiActionId::TransportStop);
    clickButton (stop);

    snapshot = snapshotMainComponent (*shell);
    REQUIRE_FALSE (snapshot.context.isPlaying);
    REQUIRE (snapshot.context.commandDispatchCount == 3);

    juce::Button& mixer = requireButtonForAction (*shell, UiActionId::ViewMixer);
    clickButton (mixer);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::Mixer);

    juce::Button& piano = requireButtonForAction (*shell, UiActionId::ViewPianoRoll);
    clickButton (piano);
    snapshot = snapshotMainComponent (*shell);
    REQUIRE (snapshot.context.activePanel == UiPanel::PianoRoll);
}
