// YES DAW — the keymap editor.
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

class KeymapEditorComponent final : public juce::Component,
                                    public juce::SettableTooltipClient,
                                    public juce::ListBoxModel
{
public:
    std::function<std::vector<yesdaw::ui::UiActionId> (const juce::String& filter)> rowsProvider;
    std::function<const yesdaw::ui::Keymap&()> keymapProvider;
    std::function<juce::String (yesdaw::ui::UiActionId, const juce::String& chord)> onRebind;   // returns the status
    std::function<void (yesdaw::ui::UiActionId)> onUnbind;
    std::function<void()> onRestoreDefaults;
    std::function<void()> onClose;

    KeymapEditorComponent()
    {
        setName ("Keymap editor");
        setComponentID ("keymap.editor");
        setTooltip ("Keymap editor (Alt+K): search, select a verb, type a chord, Enter binds it");
        search.setComponentID ("keymap.editor.search");
        search.setName ("Keymap search");
        search.setTooltip ("Type to filter the verbs by name, chord or id");
        search.setTextToShowWhenEmpty ("Search verbs", yesdaw::ui::UiTheme::Color::mutedText());
        search.onTextChange = [this] { refreshRows(); };
        addAndMakeVisible (search);
        list.setComponentID ("keymap.editor.list");
        list.setName ("Keymap verbs");
        list.setTooltip ("Every verb with its context and chord; select one to rebind it");
        list.setModel (this);
        list.setRowHeight (yesdaw::ui::UiTheme::Layout::keymapEditorRowHeight);
        addAndMakeVisible (list);
        chord.setComponentID ("keymap.editor.chord");
        chord.setName ("Chord");
        chord.setTooltip ("The chord for the selected verb, e.g. Ctrl+Shift+K — Enter binds it");
        chord.setTextToShowWhenEmpty ("Chord (Enter binds)", yesdaw::ui::UiTheme::Color::mutedText());
        chord.onReturnKey = [this] { applyChord(); };
        addAndMakeVisible (chord);
        unbindButton.setComponentID ("keymap.editor.unbind");
        unbindButton.setName ("Unbind");
        unbindButton.setButtonText ("Unbind");
        unbindButton.setTooltip ("Remove the selected verb's chord");
        unbindButton.onClick = [this] {
            if (const auto action = selectedAction(); action && onUnbind)
            {
                onUnbind (*action);
                refreshRows();
            }
        };
        addAndMakeVisible (unbindButton);
        restoreButton.setComponentID ("keymap.editor.restore");
        restoreButton.setName ("Restore defaults");
        restoreButton.setButtonText ("Restore defaults");
        restoreButton.setTooltip ("Every verb back to its default chord");
        restoreButton.onClick = [this] {
            if (onRestoreDefaults)
                onRestoreDefaults();
            refreshRows();
        };
        addAndMakeVisible (restoreButton);
        closeButton.setComponentID ("keymap.editor.close");
        closeButton.setName ("Close keymap editor");
        closeButton.setButtonText ("Close");
        closeButton.setTooltip ("Close the keymap editor (Alt+K)");
        closeButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeButton);
        status.setComponentID ("keymap.editor.status");
        status.setName ("Keymap status");
        status.setTooltip ("What the last rebind did");
        status.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (status);
    }

    void refreshRows()
    {
        rows = rowsProvider ? rowsProvider (search.getText()) : std::vector<yesdaw::ui::UiActionId> {};
        list.updateContent();
        list.repaint();
        const auto action = selectedAction();
        chord.setText (action && keymapProvider ? juce::String (keymapProvider().chordFor (*action)) : juce::String(),
                       juce::dontSendNotification);
    }

    [[nodiscard]] std::optional<yesdaw::ui::UiActionId> selectedAction() const
    {
        const int row = list.getSelectedRow();
        if (row < 0 || row >= static_cast<int> (rows.size()))
            return std::nullopt;
        return rows[static_cast<std::size_t> (row)];
    }

    void applyChord()
    {
        const auto action = selectedAction();
        if (! action || ! onRebind)
            return;
        status.setText (onRebind (*action, chord.getText().trim()), juce::dontSendNotification);
        refreshRows();
    }

    // Harness seams (no popups, no focus needed).
    void harnessSearch (const juce::String& text) { search.setText (text, juce::dontSendNotification); refreshRows(); }
    void harnessSelectRow (int row) { list.selectRow (row); refreshRows(); }
    void harnessBind (const juce::String& text) { chord.setText (text, juce::dontSendNotification); applyChord(); }
    [[nodiscard]] const std::vector<yesdaw::ui::UiActionId>& currentRows() const noexcept { return rows; }
    [[nodiscard]] juce::String statusText() const { return status.getText(); }

    int getNumRows() override { return static_cast<int> (rows.size()); }

    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int> (rows.size()))
            return;
        const yesdaw::ui::UiActionId action = rows[static_cast<std::size_t> (rowNumber)];
        const auto& descriptor = yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (action)];
        g.fillAll (rowIsSelected ? yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (yesdaw::ui::UiTheme::Tone::pressedHighlightAlpha)
                                 : yesdaw::ui::UiTheme::Color::controlInset());
        g.setColour (yesdaw::ui::UiTheme::Color::text());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::body));
        const int contextWidth = yesdaw::ui::UiTheme::Layout::keymapEditorContextColumnWidth;
        const int chordWidth = yesdaw::ui::UiTheme::Layout::keymapEditorChordColumnWidth;
        const int inset = yesdaw::ui::UiTheme::Layout::keymapEditorTextInset;
        g.drawText (yesdaw::ui::focusContextName (yesdaw::ui::defaultFocusContext (action)),
                    inset, 0, contextWidth, height, juce::Justification::centredLeft, true);
        g.drawText (descriptor.label, contextWidth + yesdaw::ui::UiTheme::Layout::keymapEditorGap, 0, width - contextWidth - chordWidth - 2 * yesdaw::ui::UiTheme::Layout::keymapEditorGap, height,
                    juce::Justification::centredLeft, true);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (yesdaw::ui::UiTheme::Type::readout));
        g.drawText (keymapProvider ? juce::String (keymapProvider().chordFor (action)) : juce::String(),
                    width - chordWidth - inset, 0, chordWidth, height, juce::Justification::centredRight, true);
    }

    void selectedRowsChanged (int) override { refreshRows(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (yesdaw::ui::UiTheme::Color::panelRaised());
        g.setColour (yesdaw::ui::UiTheme::Color::separator());
        g.drawRect (getLocalBounds(), yesdaw::ui::UiTheme::Space::hairline);
        g.setColour (yesdaw::ui::UiTheme::Color::text());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::title, juce::Font::bold));
        g.drawText ("Keymap", getLocalBounds().withTrimmedLeft (yesdaw::ui::UiTheme::Layout::keymapEditorInset).withHeight (yesdaw::ui::UiTheme::Layout::keymapEditorTopRowHeight), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        using L = yesdaw::ui::UiTheme::Layout;
        auto area = getLocalBounds().reduced (L::keymapEditorInset);
        auto top = area.removeFromTop (L::keymapEditorTopRowHeight);
        top.removeFromLeft (L::keymapEditorTitleWidth);
        closeButton.setBounds (top.removeFromRight (L::keymapEditorCloseWidth));
        top.removeFromRight (L::keymapEditorGap);
        restoreButton.setBounds (top.removeFromRight (L::keymapEditorRestoreWidth));
        top.removeFromRight (L::keymapEditorGap);
        search.setBounds (top.reduced (yesdaw::ui::UiTheme::Space::none, L::keymapEditorSearchInsetY));
        area.removeFromTop (L::keymapEditorGap);
        auto bottom = area.removeFromBottom (L::keymapEditorBottomRowHeight);
        unbindButton.setBounds (bottom.removeFromRight (L::keymapEditorUnbindWidth));
        bottom.removeFromRight (L::keymapEditorGap);
        chord.setBounds (bottom.removeFromLeft (L::keymapEditorChordWidth));
        bottom.removeFromLeft (L::keymapEditorGap);
        status.setBounds (bottom);
        area.removeFromBottom (L::keymapEditorGap);
        list.setBounds (area);
    }

private:
    juce::TextEditor search, chord;
    juce::ListBox list;
    juce::TextButton unbindButton, restoreButton, closeButton;
    juce::Label status;
    std::vector<yesdaw::ui::UiActionId> rows;
};

} // namespace yesdaw::ui
