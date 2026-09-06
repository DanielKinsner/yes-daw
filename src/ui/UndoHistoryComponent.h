// YES DAW — the undo history window.
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

// G2.18: the undo history window (Alt+Z; Logic's Undo History) — every step as a row, oldest first,
// the current position marked; a click jumps there (N undo or redo steps). Never modal.
class UndoHistoryComponent final : public juce::Component,
                                   public juce::SettableTooltipClient,
                                   public juce::ListBoxModel
{
public:
    std::function<std::vector<juce::String>()> rowsProvider;
    std::function<int()> currentProvider;
    std::function<void (int)> onRowClicked;
    std::function<void()> onClose;

    UndoHistoryComponent()
    {
        setName ("Undo history");
        setComponentID ("undo.history");
        setTooltip ("Undo history (Alt+Z): every edit as a step; click a step to jump there");
        list.setComponentID ("undo.history.list");
        list.setName ("Undo history steps");
        list.setTooltip ("The edits in order; the highlighted row is now — click a row to undo or redo to it");
        list.setModel (this);
        list.setRowHeight (yesdaw::ui::UiTheme::Layout::undoHistoryRowHeight);
        addAndMakeVisible (list);
        closeButton.setComponentID ("undo.history.close");
        closeButton.setButtonText ("Close");
        closeButton.setTooltip ("Hide the undo history (Esc)");
        closeButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeButton);
    }

    void refreshRows()
    {
        rows = rowsProvider ? rowsProvider() : std::vector<juce::String> {};
        current = currentProvider ? currentProvider() : 0;
        list.updateContent();
        list.repaint();
    }

    [[nodiscard]] const std::vector<juce::String>& currentRows() const noexcept { return rows; }
    [[nodiscard]] int currentRow() const noexcept { return current; }
    void clickRow (int row) { if (onRowClicked) onRowClicked (row); }   // the harness's click, the same path

    int getNumRows() override { return static_cast<int> (rows.size()) + 1; }   // row 0 is "Project opened"

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool) override
    {
        const bool isNow = row == current;
        const bool ahead = row > current;   // a redo step
        if (isNow)
        {
            g.setColour (yesdaw::ui::UiTheme::Color::selectedLane());
            g.fillRect (0, 0, width, height);
        }
        g.setColour (ahead ? yesdaw::ui::UiTheme::Color::mutedText() : yesdaw::ui::UiTheme::Color::text());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
        const juce::String text = row == 0 ? juce::String ("Project opened") : rows[static_cast<std::size_t> (row - 1)];
        g.drawText (juce::String (row) + "  " + text, yesdaw::ui::UiTheme::Space::sm, 0,
                    width - 2 * yesdaw::ui::UiTheme::Space::sm, height, juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (onRowClicked)
            onRowClicked (row);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (yesdaw::ui::UiTheme::Color::panelRaised());
        g.fillRoundedRectangle (getLocalBounds().toFloat(), yesdaw::ui::UiTheme::Radius::panel);
        g.setColour (yesdaw::ui::UiTheme::Color::panelStroke());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), yesdaw::ui::UiTheme::Radius::panel, 1.0f);
        g.setColour (yesdaw::ui::UiTheme::Color::text());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::body));
        g.drawText ("Undo History", getLocalBounds().withHeight (yesdaw::ui::UiTheme::Layout::keymapEditorTopRowHeight)
                                        .reduced (yesdaw::ui::UiTheme::Layout::keymapEditorInset, 0),
                    juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        using L = yesdaw::ui::UiTheme::Layout;
        auto area = getLocalBounds().reduced (L::keymapEditorInset);
        auto top = area.removeFromTop (L::keymapEditorTopRowHeight);
        closeButton.setBounds (top.removeFromRight (L::keymapEditorCloseWidth));
        area.removeFromTop (L::keymapEditorGap);
        list.setBounds (area);
    }

private:
    juce::ListBox list;
    juce::TextButton closeButton;
    std::vector<juce::String> rows;
    int current = 0;
};

} // namespace yesdaw::ui
