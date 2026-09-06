// YES DAW — the instrument panel (an Editor-dock tab).
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

// G0.4 (ADR-0046 §6 "nothing is blind", plan §5.2 layered rendering): the playhead lives on its
// own transparent layer ABOVE the buffered timeline canvas, painted by the SAME drawPlayhead law
// and the SAME geometry the canvas would use. A moving playhead therefore repaints a cheap
// overlay thirty times a second instead of re-rendering every clip and waveform.
// G1.5: the keymap editor (Alt+K). A searchable list of every verb per Focus context, a chord
// field that rebinds the selected row on Enter (conflicts refused with the owner named in the
// status line), Unbind, Restore defaults. Rebinds persist beside the last-project record.
// G3.1 / ADR-0047: the instrument panel — an Editor-dock tab (never a modal, ADR-0046) showing the
// selected Track's instrument: the kind chooser and one row per ParamSpec (label, slider, readout).
// Every committed slider value is one undoable SetTrackInstrumentParam through the model; a drag
// rides Touch / Latch automation exactly like an FX parameter drag (the InstrumentParam lane law).
class InstrumentPanelComponent final : public juce::Component,
                                       public juce::SettableTooltipClient,
                                       public juce::FileDragAndDropTarget   // G3.9: a WAV onto a pad cell
{
public:
    static constexpr std::size_t kMaxRows = 12;
    // G3.9: one Sampler pad as the panel paints it (a loaded pad has a name; an empty cell none).
    struct Pad
    {
        int key = -1;
        juce::String name;
        bool oneShot = true;
        bool loaded = false;
    };

    struct Row
    {
        std::uint32_t paramId = 0;
        juce::String label;
        juce::String readout;
        double normalized = 0.0;
    };

    std::function<juce::String()> kindProvider;
    std::function<std::vector<juce::String>()> kindChoicesProvider;
    std::function<int()> kindIndexProvider;
    std::function<void (int)> onKindChosen;
    std::function<std::vector<Row>()> rowsProvider;
    std::function<void (std::uint32_t)> onRowDragStart;
    std::function<void()> onRowDragEnd;
    std::function<void (std::uint32_t, double)> onRowValue;
    std::function<bool()> padsProvider;                          // G3.9: true when the kind is the Sampler
    std::function<std::vector<Pad>()> padRowsProvider;           // G3.9: the loaded pads
    std::function<void (int, bool, bool)> onPadClicked;          // G3.9: key, shift, ctrl
    std::function<void (int, const juce::StringArray&)> onPadFilesDropped;   // G3.9: key, files

    InstrumentPanelComponent()
    {
        setName ("Instrument panel");
        setComponentID ("instrument.panel");
        setTooltip ("The selected Track's instrument: choose the kind, drag a parameter (rides Touch / Latch automation)");
        kindChooser.setComponentID ("instrument.panel.kind");
        kindChooser.setName ("Instrument kind");
        kindChooser.setTooltip ("Which instrument the selected Track plays");
        kindChooser.onChange = [this] {
            if (refreshing || ! onKindChosen)
                return;
            const int selected = kindChooser.getSelectedId();
            if (selected > 0)
                onKindChosen (selected - 1);
        };
        addAndMakeVisible (kindChooser);
        for (std::size_t i = 0; i < kMaxRows; ++i)
        {
            auto& label = labels[i];
            label.setComponentID ("instrument.panel.row." + juce::String (static_cast<int> (i)) + ".label");
            label.setTooltip ("Instrument parameter " + juce::String (static_cast<int> (i) + 1));
            label.setInterceptsMouseClicks (false, false);
            addChildComponent (label);
            auto& slider = sliders[i];
            slider.setComponentID ("instrument.panel.row." + juce::String (static_cast<int> (i)));
            slider.setName ("Instrument parameter " + juce::String (static_cast<int> (i) + 1));
            slider.setTooltip ("Drag to set the parameter; one undo step per drag");
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
            slider.setRange (0.0, 1.0, 0.0);
            slider.onDragStart = [this, i] { if (onRowDragStart && i < rows.size()) onRowDragStart (rows[i].paramId); };
            slider.onDragEnd = [this] { if (onRowDragEnd) onRowDragEnd(); };
            slider.onValueChange = [this, i] {
                if (refreshing || ! onRowValue || i >= rows.size())
                    return;
                onRowValue (rows[i].paramId, sliders[i].getValue());
            };
            addChildComponent (slider);
            auto& readout = readouts[i];
            readout.setComponentID ("instrument.panel.row." + juce::String (static_cast<int> (i)) + ".readout");
            readout.setTooltip ("The parameter's value in its unit");
            readout.setInterceptsMouseClicks (false, false);
            readout.setJustificationType (juce::Justification::centredRight);
            addChildComponent (readout);
        }
    }

    void refresh()
    {
        refreshing = true;
        kindChooser.clear (juce::dontSendNotification);
        const std::vector<juce::String> choices = kindChoicesProvider ? kindChoicesProvider() : std::vector<juce::String> {};
        for (std::size_t i = 0; i < choices.size(); ++i)
            kindChooser.addItem (choices[i], static_cast<int> (i) + 1);
        const int kindIndex = kindIndexProvider ? kindIndexProvider() : -1;
        if (kindIndex >= 0)
            kindChooser.setSelectedId (kindIndex + 1, juce::dontSendNotification);
        rows = rowsProvider ? rowsProvider() : std::vector<Row> {};
        showPads = padsProvider && padsProvider();   // G3.9
        pads = showPads && padRowsProvider ? padRowsProvider() : std::vector<Pad> {};
        for (std::size_t i = 0; i < kMaxRows; ++i)
        {
            const bool used = i < rows.size();
            labels[i].setVisible (used);
            sliders[i].setVisible (used);
            readouts[i].setVisible (used);
            if (! used)
                continue;
            labels[i].setText (rows[i].label, juce::dontSendNotification);
            labels[i].setColour (juce::Label::textColourId, yesdaw::ui::UiTheme::Color::text());
            labels[i].setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
            sliders[i].setValue (rows[i].normalized, juce::dontSendNotification);
            readouts[i].setText (rows[i].readout, juce::dontSendNotification);
            readouts[i].setColour (juce::Label::textColourId, yesdaw::ui::UiTheme::Color::mutedText());
            readouts[i].setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
        }
        refreshing = false;
        resized();
        repaint();
    }

    [[nodiscard]] const std::vector<Row>& currentRows() const noexcept { return rows; }
    [[nodiscard]] juce::String currentKind() const { return kindProvider ? kindProvider() : juce::String(); }
    [[nodiscard]] bool padsShown() const noexcept { return showPads && ! padGrid.isEmpty(); }   // G3.9
    [[nodiscard]] const std::vector<Pad>& currentPads() const noexcept { return pads; }
    [[nodiscard]] juce::Rectangle<int> currentPadGrid() const noexcept { return padGrid; }

    // G3.9: the pad grid's cells — two rows of eight from instrumentPanelPadFirstKey, left to right,
    // bottom row first (the lower keys nearer the keyboard's bottom, as Logic's Drum Machine Designer).
    [[nodiscard]] juce::Rectangle<int> padCellBounds (int key) const noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const int index = key - L::instrumentPanelPadFirstKey;
        if (padGrid.isEmpty() || index < 0 || index >= L::instrumentPanelPadCount)
            return {};
        const int column = index % L::instrumentPanelPadColumns;
        const int rowFromBottom = index / L::instrumentPanelPadColumns;
        const int row = L::instrumentPanelPadRows - 1 - rowFromBottom;
        const int cellWidth = (padGrid.getWidth() - L::instrumentPanelPadGap * (L::instrumentPanelPadColumns - 1)) / L::instrumentPanelPadColumns;
        return juce::Rectangle<int> (padGrid.getX() + column * (cellWidth + L::instrumentPanelPadGap),
                                     padGrid.getY() + row * (padCellHeight + L::instrumentPanelPadGap),
                                     cellWidth, padCellHeight);
    }
    [[nodiscard]] int padKeyAt (juce::Point<int> point) const noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        for (int index = 0; index < L::instrumentPanelPadCount; ++index)
            if (padCellBounds (L::instrumentPanelPadFirstKey + index).contains (point))
                return L::instrumentPanelPadFirstKey + index;
        return -1;
    }
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! padsShown() || ! onPadClicked)
            return;
        const int key = padKeyAt (event.getPosition());
        if (key >= 0)
            onPadClicked (key, event.mods.isShiftDown(), event.mods.isCtrlDown());
    }
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        if (! padsShown())
            return false;
        for (const juce::String& file : files)
            if (juce::File (file).hasFileExtension ("wav;wave"))
                return true;
        return false;
    }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        const int key = padKeyAt ({ x, y });
        if (key >= 0 && onPadFilesDropped)
            onPadFilesDropped (key, files);
    }
    void harnessClickPad (int key, bool shift, bool ctrl) { if (onPadClicked) onPadClicked (key, shift, ctrl); }
    void harnessDropOnPad (int key, const juce::StringArray& files) { if (onPadFilesDropped) onPadFilesDropped (key, files); }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! padsShown())
            return;
        using L = yesdaw::ui::UiTheme::Layout;
        g.setColour (yesdaw::ui::UiTheme::Color::mutedText());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
        g.drawText ("PADS  click: load  \u00b7  shift+click: one-shot / pitched  \u00b7  ctrl+click: clear  \u00b7  drop a WAV on a pad",
                    padCaption, juce::Justification::centredLeft, true);
        for (int index = 0; index < L::instrumentPanelPadCount; ++index)
        {
            const int key = L::instrumentPanelPadFirstKey + index;
            const juce::Rectangle<int> cell = padCellBounds (key);
            const Pad* pad = nullptr;
            for (const Pad& candidate : pads)
                if (candidate.key == key)
                    pad = &candidate;
            g.setColour (pad != nullptr ? yesdaw::ui::UiTheme::Color::samplerPadLoaded() : yesdaw::ui::UiTheme::Color::samplerPadEmpty());
            g.fillRect (cell);
            g.setColour (yesdaw::ui::UiTheme::Color::panelStroke());
            g.drawRect (cell, 1);
            g.setColour (pad != nullptr ? yesdaw::ui::UiTheme::Color::text() : yesdaw::ui::UiTheme::Color::mutedText());
            g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny, pad != nullptr ? juce::Font::bold : juce::Font::plain));
            const juce::Rectangle<int> inner = cell.reduced (L::instrumentPanelPadLabelInset);
            g.drawText (juce::String (yesdaw::ui::pianoRollKeyName (key)), inner, juce::Justification::topLeft, false);
            if (pad != nullptr)
            {
                g.drawFittedText (pad->name, inner, juce::Justification::centred, padCellHeight >= L::instrumentPanelPadCellHeight ? 2 : 1);
                if (padCellHeight >= L::instrumentPanelPadCellModeMinHeight)
                    g.drawText (pad->oneShot ? "1-shot" : "pitched", inner, juce::Justification::bottomRight, false);
            }
        }
    }
    void harnessSetRow (int row, double normalized)
    {
        if (row < 0 || static_cast<std::size_t> (row) >= rows.size())
            return;
        sliders[static_cast<std::size_t> (row)].setValue (normalized, juce::sendNotificationSync);
    }
    // A whole drag: start, the values in order, end — the shape a real slider drag has.
    void harnessDragRow (int row, std::initializer_list<double> values)
    {
        if (row < 0 || static_cast<std::size_t> (row) >= rows.size())
            return;
        if (onRowDragStart)
            onRowDragStart (rows[static_cast<std::size_t> (row)].paramId);
        for (const double value : values)
            sliders[static_cast<std::size_t> (row)].setValue (value, juce::sendNotificationSync);
        if (onRowDragEnd)
            onRowDragEnd();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (yesdaw::ui::UiTheme::Color::panel());
        g.setColour (yesdaw::ui::UiTheme::Color::text());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::body, juce::Font::bold));
        g.drawText ("Instrument  " + currentKind(),
                    getLocalBounds().reduced (yesdaw::ui::UiTheme::Layout::instrumentPanelInset, 0)
                                    .withHeight (yesdaw::ui::UiTheme::Layout::instrumentPanelTitleHeight),
                    juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        using L = yesdaw::ui::UiTheme::Layout;
        auto area = getLocalBounds().reduced (L::instrumentPanelInset);
        auto title = area.removeFromTop (L::instrumentPanelTitleHeight);
        kindChooser.setBounds (title.removeFromRight (L::instrumentPanelKindWidth));
        area.removeFromTop (yesdaw::ui::UiTheme::Space::sm);
        // G3.9: the pad grid takes the panel's bottom when the Sampler is shown — whole or not at all
        // (the section-fit law) and BEFORE the parameter rows: a kit's pads matter more than its ADSR
        // (ss4 found the default dock height dropping the grid); the rows take what is left, each
        // dropping whole when it no longer fits.
        padGrid = {};
        padCaption = {};
        if (showPads)
        {
            // The cell height adapts to the dock (the minimum dock still shows the whole grid): from
            // instrumentPanelPadCellMinHeight up to instrumentPanelPadCellHeight.
            const int spare = area.getHeight() - L::instrumentPanelPadCaptionHeight - yesdaw::ui::UiTheme::Space::sm
                            - (L::instrumentPanelPadRows - 1) * L::instrumentPanelPadGap;
            padCellHeight = juce::jlimit (L::instrumentPanelPadCellMinHeight, L::instrumentPanelPadCellHeight,
                                          spare / L::instrumentPanelPadRows);
            const int gridHeight = L::instrumentPanelPadRows * padCellHeight + (L::instrumentPanelPadRows - 1) * L::instrumentPanelPadGap;
            const int needed = L::instrumentPanelPadCaptionHeight + gridHeight + yesdaw::ui::UiTheme::Space::sm;
            if (area.getHeight() >= needed)
            {
                padGrid = area.removeFromBottom (gridHeight);
                padCaption = area.removeFromBottom (L::instrumentPanelPadCaptionHeight);
                area.removeFromBottom (yesdaw::ui::UiTheme::Space::sm);
            }
        }
        for (std::size_t i = 0; i < kMaxRows; ++i)
        {
            if (! sliders[i].isVisible())
                continue;
            if (area.getHeight() < L::instrumentPanelRowHeight)
            {
                labels[i].setBounds ({});
                sliders[i].setBounds ({});
                readouts[i].setBounds ({});
                continue;   // drop whole (the section-fit law)
            }
            auto row = area.removeFromTop (L::instrumentPanelRowHeight);
            labels[i].setBounds (row.removeFromLeft (L::instrumentPanelLabelWidth));
            readouts[i].setBounds (row.removeFromRight (L::instrumentPanelReadoutWidth));
            sliders[i].setBounds (row);
            area.removeFromTop (yesdaw::ui::UiTheme::Space::xs);
        }
    }

private:
    juce::ComboBox kindChooser;
    std::array<juce::Label, kMaxRows> labels;
    std::array<juce::Slider, kMaxRows> sliders;
    std::array<juce::Label, kMaxRows> readouts;
    std::vector<Row> rows;
    std::vector<Pad> pads;               // G3.9
    bool showPads = false;
    juce::Rectangle<int> padGrid;
    juce::Rectangle<int> padCaption;
    int padCellHeight = yesdaw::ui::UiTheme::Layout::instrumentPanelPadCellHeight;
    bool refreshing = false;
};

} // namespace yesdaw::ui
