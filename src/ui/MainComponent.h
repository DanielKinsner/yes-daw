// YES DAW - shipped JUCE app shell surface.
//
// H12's input harness constructs this shell directly so CI can prove real Components,
// not only the headless model underneath them.

#pragma once

#include "engine/Project.h"
#include "ui/UiActions.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace yesdaw::ui {

struct MainComponentFileChoices
{
    std::function<std::filesystem::path()> chooseNewProjectBundle;
    std::function<std::filesystem::path()> chooseOpenProjectBundle;
    std::function<std::filesystem::path()> chooseImportAudioFile;
    std::function<engine::Project()> makeNewProject;
};

struct MainComponentSnapshot
{
    bool isMainComponent = false;
    bool playbackReady = false;
    int width = 0;
    int height = 0;
    int childCount = 0;
    std::filesystem::path bundlePath;
    UiActionContext context;
};

[[nodiscard]] std::unique_ptr<juce::Component> createMainComponent (MainComponentFileChoices fileChoices = {});
[[nodiscard]] MainComponentSnapshot snapshotMainComponent (const juce::Component& component);
[[nodiscard]] std::vector<float> renderMainComponentPlayback (juce::Component& component,
                                                              std::uint64_t frames,
                                                              int blockSize);

[[nodiscard]] juce::Component* findMainComponentChildForAction (juce::Component& component, UiActionId action);
[[nodiscard]] const juce::Component* findMainComponentChildForAction (const juce::Component& component, UiActionId action);

} // namespace yesdaw::ui
