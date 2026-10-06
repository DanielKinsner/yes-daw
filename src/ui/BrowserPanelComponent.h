// YES DAW — the media browser (ADR-0056): an Editor-dock tab with three sources (Files, Project, Recent).
//
// The panel owns its widgets and its painted list; the shell owns what the rows are and what keeping one does
// (the import verbs), through std::function hooks. The list is a ChooserListControl so ADR-0049's Control target
// drives it like a chooser: Enter starts, the arrows step the rows, Enter keeps (imports a file, opens a folder),
// Esc restores. No widget here takes keyboard focus (G0.2).

#pragma once

#include "engine/Project.h"
#include "ui/UiTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <vector>

namespace yesdaw::ui {

// ADR-0056: a painted list the Control target drives like a chooser.
class ChooserListControl
{
public:
    virtual ~ChooserListControl() = default;
    [[nodiscard]] virtual int chooserSelection() const = 0;
    [[nodiscard]] virtual int chooserCount() const = 0;
    virtual void chooserPreview (int index) = 0;   // select without acting
    virtual void chooserKeep() = 0;                // act on the selection
    [[nodiscard]] virtual juce::String chooserText() const = 0;   // what the Control target reads out
};

struct BrowserRow
{
    enum class Kind : std::uint8_t
    {
        Parent,   // the ".." row
        Folder,
        File,
        Asset
    };
    Kind kind = Kind::File;
    juce::String name;
    std::filesystem::path path;           // Parent / Folder / File
    yesdaw::engine::EntityId assetId;     // Asset
    juce::String facts;                   // filled lazily (header facts) or up front (an Asset's)
    juce::String reason;                  // why it cannot be imported (empty when it can)
    bool factsLoaded = false;
};

class BrowserListComponent final : public juce::Component,
                                   public juce::SettableTooltipClient,
                                   public ChooserListControl
{
public:
    std::function<void (BrowserRow&)> loadFacts;                                  // header facts for a painted row
    std::function<void (const BrowserRow&)> onKeep;                               // import / open
    std::function<void (std::vector<BrowserRow>, juce::Point<int>)> onDragReleased;   // rows, the release point (list-local)
    std::function<void()> onSelectionChanged;

    BrowserListComponent()
    {
        setComponentID ("browser.list");
        setTitle ("Browser list");
        setTooltip ("Click selects, Ctrl+click adds, double-click imports a file or opens a folder, drag places on the lanes");
        setWantsKeyboardFocus (false);
    }

    void setRows (std::vector<BrowserRow> rows)
    {
        rows_ = std::move (rows);
        selected_ = rows_.empty() ? -1 : 0;
        marked_.assign (rows_.size(), false);
        if (selected_ >= 0)
            marked_[0] = true;
        firstVisible_ = 0;
        updateDescription();
        repaint();
    }

    [[nodiscard]] const std::vector<BrowserRow>& rows() const noexcept { return rows_; }
    [[nodiscard]] const BrowserRow* selectedRow() const noexcept
    {
        return selected_ >= 0 && selected_ < static_cast<int> (rows_.size()) ? &rows_[static_cast<std::size_t> (selected_)] : nullptr;
    }

    // Every selected row in list order (a click selects one; Ctrl+click adds or removes one; Shift+click a range).
    [[nodiscard]] std::vector<BrowserRow> selectedRows() const
    {
        std::vector<BrowserRow> out;
        for (std::size_t i = 0; i < rows_.size(); ++i)
            if (i < marked_.size() && marked_[i])
                out.push_back (rows_[i]);
        return out;
    }
    [[nodiscard]] bool isRowSelected (int row) const noexcept
    {
        return row >= 0 && row < static_cast<int> (marked_.size()) && marked_[static_cast<std::size_t> (row)];
    }

    // ChooserListControl
    [[nodiscard]] int chooserSelection() const override { return selected_; }
    [[nodiscard]] juce::String chooserText() const override { return getDescription(); }
    [[nodiscard]] int chooserCount() const override { return static_cast<int> (rows_.size()); }
    void chooserPreview (int index) override { select (index); }
    void chooserKeep() override
    {
        if (const BrowserRow* row = selectedRow(); row != nullptr && onKeep)
            onKeep (*row);
    }

    void select (int index) { selectWith (index, false, false); }

    // A click's selection law: plain selects one row, `toggle` (Ctrl) adds or removes one, `range` (Shift) selects
    // from the current row to this one. The current row (`selected_`) is the one the keyboard and Import act on.
    void selectWith (int index, bool toggle, bool range)
    {
        if (rows_.empty())
            return;
        index = std::clamp (index, 0, static_cast<int> (rows_.size()) - 1);
        marked_.resize (rows_.size(), false);
        if (range && selected_ >= 0)
        {
            const auto [from, to] = std::minmax (selected_, index);
            std::fill (marked_.begin(), marked_.end(), false);
            for (int i = from; i <= to; ++i)
                marked_[static_cast<std::size_t> (i)] = true;
        }
        else if (toggle)
        {
            marked_[static_cast<std::size_t> (index)] = ! marked_[static_cast<std::size_t> (index)];
        }
        else
        {
            std::fill (marked_.begin(), marked_.end(), false);
            marked_[static_cast<std::size_t> (index)] = true;
        }
        selected_ = index;
        const int visible = visibleRowCount();
        if (selected_ < firstVisible_)
            firstVisible_ = selected_;
        else if (visible > 0 && selected_ >= firstVisible_ + visible)
            firstVisible_ = selected_ - visible + 1;
        updateDescription();
        if (onSelectionChanged)
            onSelectionChanged();
        repaint();
    }

    // After a refresh of the same listing: the rows that were selected, the current one, and the scroll, kept.
    void restoreState (const std::vector<int>& marked, int current, int firstVisible)
    {
        if (rows_.empty())
            return;
        std::fill (marked_.begin(), marked_.end(), false);
        for (const int index : marked)
            if (index >= 0 && index < static_cast<int> (rows_.size()))
                marked_[static_cast<std::size_t> (index)] = true;
        if (current >= 0 && current < static_cast<int> (rows_.size()))
            selected_ = current;
        marked_[static_cast<std::size_t> (selected_)] = true;
        firstVisible_ = std::clamp (firstVisible, 0, std::max (0, static_cast<int> (rows_.size()) - visibleRowCount()));
        updateDescription();
        repaint();
    }

    [[nodiscard]] int firstVisibleRow() const noexcept { return firstVisible_; }
    [[nodiscard]] int visibleRowCount() const noexcept
    {
        return std::max (0, getHeight() / UiTheme::Layout::browserRowHeight);
    }
    [[nodiscard]] int rowAt (juce::Point<int> local) const noexcept
    {
        const int row = firstVisible_ + local.y / UiTheme::Layout::browserRowHeight;
        return local.y >= 0 && row < static_cast<int> (rows_.size()) ? row : -1;
    }
    [[nodiscard]] juce::Rectangle<int> rowBounds (int row) const noexcept
    {
        return { 0, (row - firstVisible_) * UiTheme::Layout::browserRowHeight, getWidth(), UiTheme::Layout::browserRowHeight };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (UiTheme::Color::controlInset());
        const int last = std::min (static_cast<int> (rows_.size()), firstVisible_ + visibleRowCount() + 1);
        for (int index = firstVisible_; index < last; ++index)
        {
            BrowserRow& row = rows_[static_cast<std::size_t> (index)];
            if (! row.factsLoaded && loadFacts)   // only a row on screen reads its header
            {
                loadFacts (row);
                row.factsLoaded = true;
            }
            const juce::Rectangle<int> bounds = rowBounds (index);
            if (isRowSelected (index))
            {
                g.setColour (UiTheme::Color::accentPurple().withAlpha (UiTheme::Tone::timelineDragGhostFillAlpha));
                g.fillRect (bounds);
            }
            // Two columns read left to right: the name, then its facts (or its reason) right after it.
            auto text = bounds.reduced (UiTheme::Layout::browserRowTextInset, 0);
            const auto nameArea = text.removeFromLeft (std::min (UiTheme::Layout::browserNameWidth, text.getWidth() / 2));
            text.removeFromLeft (UiTheme::Layout::browserControlGap);
            const auto factsArea = text.removeFromLeft (std::min (UiTheme::Layout::browserFactsWidth, text.getWidth()));
            g.setFont (UiTheme::Type::font (UiTheme::Type::small,
                                            row.kind == BrowserRow::Kind::File || row.kind == BrowserRow::Kind::Asset ? juce::Font::plain
                                                                                                                       : juce::Font::bold));
            g.setColour (row.reason.isEmpty() ? UiTheme::Color::text() : UiTheme::Color::mutedText());
            const juce::String label = row.kind == BrowserRow::Kind::Folder ? row.name + "/" : row.name;
            g.drawFittedText (label, nameArea, juce::Justification::centredLeft, 1);
            g.setFont (UiTheme::Type::font (UiTheme::Type::small));
            g.setColour (row.reason.isEmpty() ? UiTheme::Color::mutedText() : UiTheme::Color::dangerRed());
            g.drawFittedText (row.reason.isEmpty() ? row.facts : row.reason, factsArea, juce::Justification::centredLeft, 1);
        }
        if (rows_.empty())
        {
            g.setColour (UiTheme::Color::mutedText());
            g.setFont (UiTheme::Type::font (UiTheme::Type::small));
            g.drawFittedText ("Nothing here", getLocalBounds(), juce::Justification::centred, 1);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragging_ = false;
        if (const int row = rowAt (event.getPosition()); row >= 0)
            selectWith (row, event.mods.isCtrlDown() || event.mods.isCommandDown(), event.mods.isShiftDown());
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (event.getDistanceFromDragStart() > UiTheme::Layout::browserDragThreshold && selectedRow() != nullptr)
            dragging_ = true;
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! dragging_)
            return;
        dragging_ = false;
        if (onDragReleased && selectedRow() != nullptr)
            onDragReleased (selectedRows(), event.getPosition());   // list-local: the shell maps it onto the lanes
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (const int row = rowAt (event.getPosition()); row >= 0)
        {
            select (row);
            chooserKeep();
        }
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override { harnessWheel (wheel); }

    // The wheel's law (three rows a notch), reachable by the harness without a synthetic mouse event.
    void harnessWheel (const juce::MouseWheelDetails& wheel)
    {
        const int maxFirst = std::max (0, static_cast<int> (rows_.size()) - visibleRowCount());
        firstVisible_ = std::clamp (firstVisible_ + (wheel.deltaY < 0.0f ? 3 : -3), 0, maxFirst);
        repaint();
    }

    // Harness: a drag of the selected rows released at a list-local point (the gesture's own path).
    void harnessDragSelectedTo (juce::Point<int> listPosition)
    {
        if (onDragReleased && selectedRow() != nullptr)
            onDragReleased (selectedRows(), listPosition);
    }

private:
    void updateDescription()
    {
        // The selected row's name and facts reach accessibility as the list's description.
        const BrowserRow* row = selectedRow();
        const juce::String dash = juce::String::fromUTF8 (" \xe2\x80\x94 ");   // an em dash, spelled in bytes
        setDescription (row == nullptr ? juce::String()
                                       : row->name + (row->reason.isNotEmpty() ? dash + row->reason
                                                      : row->facts.isNotEmpty() ? dash + row->facts : juce::String()));
    }

    std::vector<BrowserRow> rows_;
    std::vector<bool> marked_;   // the selected rows (the drag carries them all)
    int selected_ = -1;
    int firstVisible_ = 0;
    bool dragging_ = false;
};

class BrowserPanelComponent final : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
    BrowserListComponent list;
    juce::ComboBox source;
    juce::Label location;
    juce::TextButton upButton { "Up" };
    juce::TextButton importButton { "Import" };

    BrowserPanelComponent()
    {
        setComponentID ("browser.panel");
        setTooltip ("Media browser (Y): files on disk, this project's audio, recent imports");
        source.setComponentID ("browser.source");
        source.setTitle ("Browser source");
        source.setTooltip ("Browse files on disk, this project's audio, or recent imports");
        source.addItem ("Files", 1);
        source.addItem ("Project", 2);
        source.addItem ("Recent", 3);
        source.setWantsKeyboardFocus (false);
        location.setComponentID ("browser.location");
        location.setColour (juce::Label::textColourId, UiTheme::Color::mutedText());
        location.setFont (UiTheme::Type::font (UiTheme::Type::small));
        location.setTooltip ("What the list shows");
        upButton.setComponentID ("browser.up");
        upButton.setTitle ("Up a folder");
        upButton.setTooltip ("Up a folder");
        upButton.setWantsKeyboardFocus (false);
        importButton.setComponentID ("browser.import");
        importButton.setTitle ("Import the selected file");
        importButton.setTooltip ("Import the selected file on the selected track at the playhead");
        importButton.setWantsKeyboardFocus (false);
        for (juce::Component* child : { static_cast<juce::Component*> (&source), static_cast<juce::Component*> (&location),
                                        static_cast<juce::Component*> (&upButton), static_cast<juce::Component*> (&importButton),
                                        static_cast<juce::Component*> (&list) })
            addAndMakeVisible (child);
    }

    void paint (juce::Graphics& g) override { g.fillAll (UiTheme::Color::mixerBack()); }

    void resized() override
    {
        using L = UiTheme::Layout;
        auto area = getLocalBounds().reduced (L::browserInset);
        auto row = area.removeFromTop (L::browserControlRowHeight);
        source.setBounds (row.removeFromLeft (L::browserSourceWidth));
        row.removeFromLeft (L::browserControlGap);
        importButton.setBounds (row.removeFromRight (L::browserButtonWidth));
        row.removeFromRight (L::browserControlGap);
        upButton.setBounds (row.removeFromRight (L::browserButtonWidth));
        row.removeFromRight (L::browserControlGap);
        location.setBounds (row);
        area.removeFromTop (L::browserControlGap);
        list.setBounds (area);
    }
};

} // namespace yesdaw::ui
