// YES DAW — the FX editor floating panel.
//
// Plan §5.1 (the shell topology), checkpoint 1 (2026-09-05): carved verbatim out of MainComponent.cpp,
// where it sat above the shell class. The shell owns the instance and wires the std::function hooks;
// this file owns the class. Behaviour unchanged; the gate is [shell-topology] in the theme audit.

#pragma once

#include "engine/Time.h"
#include "ui/ContextMenus.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiAppModel.h"
#include "ui/UiMixerSurface.h"
#include "ui/UiPianoRollSurface.h"
#include "ui/UiTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace yesdaw::ui {

// G4.1 cp2: the FX editor — the floating panel a filled insert slot's double-click (or its menu's
// Open Editor) opens over the arrangement, the keymap editor's placement law. It hosts the effect's
// parameter rows (the shell owns those widgets and lays them out in the content area — their ids are
// unchanged from the lane they left) under a title band naming the effect, the strip and the slot,
// with Bypass and Close. G4.2 gives each built-in its own face (EQ curve, GR meter …) in this frame.
class FxEditorComponent final : public juce::Component,
                                public juce::SettableTooltipClient
{
public:
    std::function<void()> onClose;
    std::function<void()> onBypass;

    FxEditorComponent()
    {
        setName ("FX editor");
        setComponentID ("mixer.fx.editor");
        setTooltip ("The insert's parameters: drag a row (rides Touch / Latch automation); Escape closes");
        bypassButton.setComponentID ("mixer.fx.editor.bypass");
        bypassButton.setName ("Bypass");
        bypassButton.setButtonText ("Bypass");
        bypassButton.setTooltip ("Bypass this insert (the slot's dot goes grey)");
        bypassButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        bypassButton.setColour (juce::TextButton::buttonOnColourId, yesdaw::ui::UiTheme::Color::accentPurpleDeep());
        bypassButton.setColour (juce::TextButton::textColourOffId, yesdaw::ui::UiTheme::Color::text());
        bypassButton.setColour (juce::TextButton::textColourOnId, yesdaw::ui::UiTheme::Color::text());
        bypassButton.onClick = [this] { if (onBypass) onBypass(); };
        addAndMakeVisible (bypassButton);
        closeButton.setComponentID ("mixer.fx.editor.close");
        closeButton.setName ("Close");
        closeButton.setButtonText ("Close");
        closeButton.setTooltip ("Close the editor (Escape)");
        closeButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        closeButton.setColour (juce::TextButton::textColourOffId, yesdaw::ui::UiTheme::Color::text());
        closeButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeButton);
    }

    void setTitleText (const juce::String& text)
    {
        if (title == text)
            return;
        title = text;
        repaint();
    }
    [[nodiscard]] const juce::String& titleText() const noexcept { return title; }
    void setBypassed (bool bypassed) { bypassButton.setToggleState (bypassed, juce::dontSendNotification); }
    [[nodiscard]] bool isBypassed() const noexcept { return bypassButton.getToggleState(); }

    // The content area the shell lays the parameter rows into (editor-local).
    [[nodiscard]] juce::Rectangle<int> contentArea() const
    {
        using L = yesdaw::ui::UiTheme::Layout;
        auto area = getLocalBounds().reduced (L::keymapEditorInset);
        area.removeFromTop (L::keymapEditorTopRowHeight + L::keymapEditorGap);
        return area;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (yesdaw::ui::UiTheme::Color::panelRaised());
        g.setColour (yesdaw::ui::UiTheme::Color::separator());
        g.drawRect (getLocalBounds(), yesdaw::ui::UiTheme::Space::hairline);
        g.setColour (yesdaw::ui::UiTheme::Color::text());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::title, juce::Font::bold));
        g.drawFittedText (title,
                          getLocalBounds().withTrimmedLeft (yesdaw::ui::UiTheme::Layout::keymapEditorInset)
                              .withHeight (yesdaw::ui::UiTheme::Layout::keymapEditorTopRowHeight)
                              .withTrimmedRight (yesdaw::ui::UiTheme::Layout::fxEditorTitleTrimRight),
                          juce::Justification::centredLeft, 1);
    }

    void resized() override
    {
        using L = yesdaw::ui::UiTheme::Layout;
        auto top = getLocalBounds().reduced (L::keymapEditorInset).removeFromTop (L::keymapEditorTopRowHeight)
                       .reduced (yesdaw::ui::UiTheme::Space::none, L::keymapEditorSearchInsetY);
        closeButton.setBounds (top.removeFromRight (L::keymapEditorCloseWidth));
        top.removeFromRight (L::keymapEditorGap);
        bypassButton.setBounds (top.removeFromRight (L::fxEditorBypassWidth));
    }

private:
    juce::String title;
    juce::TextButton bypassButton, closeButton;
};

} // namespace yesdaw::ui
