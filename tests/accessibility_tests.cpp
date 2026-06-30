#include "ui/UiAccessibility.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

using yesdaw::ui::AccessibilityRole;
using yesdaw::ui::UiActionContext;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiActionKind;
using yesdaw::ui::UiActionRegistry;
using yesdaw::ui::accessibilityRegionForStableId;
using yesdaw::ui::descriptorFor;
using yesdaw::ui::kUiActionCount;
using yesdaw::ui::mainShellToolbarActions;
using yesdaw::ui::roleName;
using yesdaw::ui::uiAccessibleActionIds;
using yesdaw::ui::uiAccessibilityRegions;

namespace {

UiActionContext fullyReachableContext()
{
    UiActionContext context;
    context.projectLoaded = true;
    context.canUndo = true;
    context.canRedo = true;
    context.timelineClipSelected = true;
    context.mixerTargetSelected = true;
    context.midiClipSelected = true;
    context.midiNoteSelected = true;
    context.recordingDeviceSelected = true;
    context.recordingTrackAvailable = true;
    context.recordingTrackArmed = true;
    context.recordingInputSelected = true;
    context.recordingMonitoringSelected = true;
    return context;
}

std::string readTextFile (const std::filesystem::path& path)
{
    std::ifstream file (path);
    REQUIRE (file.good());
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

} // namespace

TEST_CASE ("H11 accessibility action surface covers every shipped UI action",
           "[ui][accessibility]")
{
    const UiActionRegistry registry;
    std::set<UiActionId> seenActions;
    std::set<std::string_view> stableIds;
    std::set<std::string_view> keyboardPaths;

    REQUIRE (uiAccessibleActionIds().size() == kUiActionCount);

    for (const UiActionId action : uiAccessibleActionIds())
    {
        REQUIRE (seenActions.insert (action).second);

        const auto* descriptor = descriptorFor (action);
        REQUIRE (descriptor != nullptr);
        REQUIRE (descriptor->id == action);
        REQUIRE (descriptor->stableId != nullptr);
        REQUIRE (stableIds.insert (descriptor->stableId).second);
        REQUIRE (std::string_view (descriptor->stableId).find ('.') != std::string_view::npos);
        REQUIRE (descriptor->accessibleName != nullptr);
        REQUIRE_FALSE (std::string_view (descriptor->accessibleName).empty());
        REQUIRE_FALSE (std::string_view (roleName (descriptor->accessibleRole)).empty());
        REQUIRE (descriptor->defaultKey != nullptr);
        REQUIRE_FALSE (std::string_view (descriptor->defaultKey).empty());
        REQUIRE (keyboardPaths.insert (descriptor->defaultKey).second);
        REQUIRE (registry.keymap().actionForChord (descriptor->defaultKey) == action);
        REQUIRE ((descriptor->kind == UiActionKind::Command
                  || descriptor->kind == UiActionKind::Toggle
                  || descriptor->kind == UiActionKind::Query));

        UiActionContext context = fullyReachableContext();
        const auto state = registry.stateFor (action, context);
        REQUIRE (state.enabled);

        const auto dispatch = registry.dispatch (action, context);
        REQUIRE (dispatch.dispatched);
    }

    for (const UiActionId action : mainShellToolbarActions())
        REQUIRE (seenActions.contains (action));
}

TEST_CASE ("H11 accessibility regions cover custom-painted shell surfaces",
           "[ui][accessibility]")
{
    const UiActionRegistry registry;
    std::set<std::string_view> stableIds;

    for (const auto& region : uiAccessibilityRegions())
    {
        REQUIRE (region.stableId != nullptr);
        REQUIRE (stableIds.insert (region.stableId).second);
        REQUIRE (accessibilityRegionForStableId (region.stableId) == &region);
        REQUIRE (region.accessibleName != nullptr);
        REQUIRE_FALSE (std::string_view (region.accessibleName).empty());
        REQUIRE (region.role == AccessibilityRole::Panel);
        REQUIRE (region.keyboardPath != nullptr);
        REQUIRE_FALSE (std::string_view (region.keyboardPath).empty());

        if (region.backingAction != UiActionId::Count)
        {
            const auto* descriptor = registry.descriptor (region.backingAction);
            REQUIRE (descriptor != nullptr);
            REQUIRE_FALSE (std::string_view (descriptor->accessibleName).empty());

            UiActionContext context = fullyReachableContext();
            REQUIRE (registry.stateFor (region.backingAction, context).enabled);
        }
    }

    REQUIRE (accessibilityRegionForStableId ("missing.region") == nullptr);
    REQUIRE (accessibilityRegionForStableId ("timeline.canvas") != nullptr);
    REQUIRE (accessibilityRegionForStableId ("mixer.panel") != nullptr);
    REQUIRE (accessibilityRegionForStableId ("piano_roll.panel") != nullptr);
}

TEST_CASE ("H11 visual-feel launch scripts are one-command app entrypoints",
           "[ui][accessibility][launch]")
{
    const std::filesystem::path root = std::filesystem::path (YESDAW_SOURCE_DIR);
    const std::filesystem::path ps1 = root / "tools" / "launch-h11.ps1";
    const std::filesystem::path sh = root / "tools" / "launch-h11.sh";

    REQUIRE (std::filesystem::exists (ps1));
    REQUIRE (std::filesystem::exists (sh));

    const std::string ps1Text = readTextFile (ps1);
    const std::string shText = readTextFile (sh);

    REQUIRE (ps1Text.find ("cmake --build --preset ci --target YesDaw") != std::string::npos);
    REQUIRE (ps1Text.find ("YesDaw_artefacts") != std::string::npos);
    REQUIRE (ps1Text.find ("Start-Process") != std::string::npos);
    REQUIRE (shText.find ("cmake --build --preset ci --target YesDaw") != std::string::npos);
    REQUIRE (shText.find ("YesDaw_artefacts") != std::string::npos);
    REQUIRE (shText.find ("Launched") != std::string::npos);
}
