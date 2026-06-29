#include "ui/UiActions.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string_view>

using yesdaw::ui::AccessibilityRole;
using yesdaw::ui::KeymapRebindStatus;
using yesdaw::ui::UiActionContext;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiActionKind;
using yesdaw::ui::UiActionRegistry;
using yesdaw::ui::UiPanel;
using yesdaw::ui::descriptorForStableId;
using yesdaw::ui::kUiActionCount;
using yesdaw::ui::mainShellToolbarActions;
using yesdaw::ui::roleName;
using yesdaw::ui::uiActionDescriptors;

TEST_CASE ("H11 action registry exposes stable action ids, labels, keys, and accessible names",
           "[ui][actions]")
{
    const auto& actions = uiActionDescriptors();
    REQUIRE (actions.size() == kUiActionCount);

    REQUIRE (static_cast<int> (UiActionId::ProjectNew) == 0);
    REQUIRE (actions[0].stableId == std::string_view ("project.new"));
    REQUIRE (actions[1].stableId == std::string_view ("project.open"));
    REQUIRE (actions[3].stableId == std::string_view ("transport.play"));
    REQUIRE (actions[12].stableId == std::string_view ("help.show_keymap"));

    std::set<std::string_view> stableIds;
    std::set<std::string_view> defaultKeys;
    for (const auto& action : actions)
    {
        REQUIRE (action.stableId != nullptr);
        REQUIRE (std::string_view (action.stableId).find ('.') != std::string_view::npos);
        REQUIRE (stableIds.insert (action.stableId).second);
        REQUIRE (action.label != nullptr);
        REQUIRE_FALSE (std::string_view (action.label).empty());
        REQUIRE (action.defaultKey != nullptr);
        REQUIRE_FALSE (std::string_view (action.defaultKey).empty());
        REQUIRE (defaultKeys.insert (action.defaultKey).second);
        REQUIRE (action.accessibleName != nullptr);
        REQUIRE_FALSE (std::string_view (action.accessibleName).empty());
        REQUIRE_FALSE (std::string_view (roleName (action.accessibleRole)).empty());
        REQUIRE ((action.kind == UiActionKind::Command
                  || action.kind == UiActionKind::Toggle
                  || action.kind == UiActionKind::Query));
    }

    REQUIRE (descriptorForStableId ("transport.toggle_loop")->id == UiActionId::TransportToggleLoop);
    REQUIRE (descriptorForStableId ("missing.action") == nullptr);
}

TEST_CASE ("H11 default shell toolbar actions all resolve through the registry", "[ui][actions]")
{
    const UiActionRegistry registry;
    std::set<UiActionId> seen;

    for (UiActionId id : mainShellToolbarActions())
    {
        const auto* descriptor = registry.descriptor (id);
        REQUIRE (descriptor != nullptr);
        REQUIRE (seen.insert (id).second);
        REQUIRE_FALSE (std::string_view (descriptor->stableId).empty());
        REQUIRE_FALSE (std::string_view (descriptor->label).empty());
        REQUIRE (registry.keymap().actionForChord (descriptor->defaultKey) == id);
    }
}

TEST_CASE ("H11 keymap remapping is stable and rejects duplicate or empty chords", "[ui][keymap]")
{
    UiActionRegistry registry;

    REQUIRE (registry.keymap().actionForChord ("Space") == UiActionId::TransportPlay);
    REQUIRE (registry.keymap().rebind (UiActionId::TransportPlay, "P") == KeymapRebindStatus::Ok);
    REQUIRE (registry.keymap().actionForChord ("P") == UiActionId::TransportPlay);
    REQUIRE (registry.keymap().actionForChord ("Space") == UiActionId::Count);

    REQUIRE (registry.keymap().rebind (UiActionId::TransportStop, "P")
             == KeymapRebindStatus::DuplicateChord);
    REQUIRE (registry.keymap().chordFor (UiActionId::TransportStop) == "K");

    REQUIRE (registry.keymap().rebind (UiActionId::TransportStop, "")
             == KeymapRebindStatus::EmptyChord);
    REQUIRE (registry.keymap().rebind (UiActionId::Count, "X")
             == KeymapRebindStatus::UnknownAction);
}

TEST_CASE ("H11 action enabled state explains disabled project, undo, and redo commands",
           "[ui][actions]")
{
    const UiActionRegistry registry;
    UiActionContext context;

    REQUIRE (registry.stateFor (UiActionId::ProjectOpen, context).enabled);

    const auto playBeforeProject = registry.stateFor (UiActionId::TransportPlay, context);
    REQUIRE_FALSE (playBeforeProject.enabled);
    REQUIRE (playBeforeProject.disabledReason == std::string_view ("no project loaded"));

    context.projectLoaded = true;
    REQUIRE (registry.stateFor (UiActionId::TransportPlay, context).enabled);

    const auto undoWithoutStack = registry.stateFor (UiActionId::EditUndo, context);
    REQUIRE_FALSE (undoWithoutStack.enabled);
    REQUIRE (undoWithoutStack.disabledReason == std::string_view ("nothing to undo"));

    context.canUndo = true;
    REQUIRE (registry.stateFor (UiActionId::EditUndo, context).enabled);

    const auto redoWithoutStack = registry.stateFor (UiActionId::EditRedo, context);
    REQUIRE_FALSE (redoWithoutStack.enabled);
    REQUIRE (redoWithoutStack.disabledReason == std::string_view ("nothing to redo"));
}

TEST_CASE ("H11 action dispatch mutates only the headless app model behind action ids",
           "[ui][actions]")
{
    const UiActionRegistry registry;
    UiActionContext context;

    const auto disabledPlay = registry.dispatch (UiActionId::TransportPlay, context);
    REQUIRE_FALSE (disabledPlay.dispatched);
    REQUIRE_FALSE (context.isPlaying);
    REQUIRE (context.commandDispatchCount == 0);

    REQUIRE (registry.dispatch (UiActionId::ProjectNew, context).dispatched);
    REQUIRE (context.projectLoaded);
    REQUIRE (context.activePanel == UiPanel::Timeline);
    REQUIRE (context.commandDispatchCount == 1);

    REQUIRE (registry.dispatch (UiActionId::TransportPlay, context).dispatched);
    REQUIRE (context.isPlaying);
    REQUIRE (registry.dispatch (UiActionId::TransportStop, context).dispatched);
    REQUIRE_FALSE (context.isPlaying);

    context.playheadFrame = 4096;
    REQUIRE (registry.dispatch (UiActionId::TransportLocateStart, context).dispatched);
    REQUIRE (context.playheadFrame == 0);

    REQUIRE (registry.dispatch (UiActionId::TransportToggleLoop, context).dispatched);
    REQUIRE (context.loopEnabled);

    context.canUndo = true;
    REQUIRE (registry.dispatch (UiActionId::EditUndo, context).dispatched);
    REQUIRE (context.undoCount == 1);
    REQUIRE_FALSE (context.canUndo);
    REQUIRE (context.canRedo);
    REQUIRE (registry.dispatch (UiActionId::EditRedo, context).dispatched);
    REQUIRE (context.redoCount == 1);
    REQUIRE (context.canUndo);
    REQUIRE_FALSE (context.canRedo);

    REQUIRE (registry.dispatch (UiActionId::ViewMixer, context).dispatched);
    REQUIRE (context.activePanel == UiPanel::Mixer);
    REQUIRE (registry.dispatch (UiActionId::ViewPianoRoll, context).dispatched);
    REQUIRE (context.activePanel == UiPanel::PianoRoll);
    REQUIRE (registry.dispatch (UiActionId::HelpShowKeymap, context).dispatched);
    REQUIRE (context.keymapVisible);
}
