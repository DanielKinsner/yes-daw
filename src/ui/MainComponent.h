// YES DAW - shipped JUCE app shell surface.
//
// H12's input harness constructs this shell directly so CI can prove real Components,
// not only the headless model underneath them.

#pragma once

#include "ui/UiActions.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

namespace yesdaw::ui {

struct MainComponentSnapshot
{
    bool isMainComponent = false;
    int width = 0;
    int height = 0;
    int childCount = 0;
    UiActionContext context;
};

[[nodiscard]] std::unique_ptr<juce::Component> createMainComponent();
[[nodiscard]] MainComponentSnapshot snapshotMainComponent (const juce::Component& component);

[[nodiscard]] juce::Component* findMainComponentChildForAction (juce::Component& component, UiActionId action);
[[nodiscard]] const juce::Component* findMainComponentChildForAction (const juce::Component& component, UiActionId action);

} // namespace yesdaw::ui
