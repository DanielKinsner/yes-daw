#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <functional>

#include "ui/UiTheme.h"

namespace yesdaw::ui
{

// G2.1 (plan §3.1, ADR-0046): a draggable divider between two Arrange-window panels. It owns no
// size — the shell's view state does; the splitter reports where the pointer is (in the parent's
// coordinates) and the shell clamps, lays out again and persists.
class SplitterComponent final : public juce::Component,
                                public juce::SettableTooltipClient   // every id-bearing control explains itself (G1.6)
{
public:
    enum class Axis : std::uint8_t
    {
        Vertical,     // a left/right divider (drags along x)
        Horizontal    // a top/bottom divider (drags along y)
    };

    explicit SplitterComponent (Axis axis) : axis_ (axis)
    {
        setMouseCursor (axis == Axis::Vertical ? juce::MouseCursor::LeftRightResizeCursor
                                               : juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (false);
        setRepaintsOnMouseActivity (true);
    }

    [[nodiscard]] Axis axis() const noexcept { return axis_; }

    std::function<void (juce::Point<int>)> onDrag;   // the pointer, in the parent's coordinates
    std::function<void()> onDragEnd;

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (onDrag != nullptr && getParentComponent() != nullptr)
            onDrag (event.getEventRelativeTo (getParentComponent()).getPosition());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (onDragEnd != nullptr)
            onDragEnd();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (isMouseOverOrDragging() ? UiTheme::Color::mutedText() : UiTheme::Color::panelStroke());
        const juce::Rectangle<int> bounds = getLocalBounds();
        if (axis_ == Axis::Vertical)
            g.fillRect (bounds.withX (bounds.getCentreX()).withWidth (UiTheme::Space::hairline));
        else
            g.fillRect (bounds.withY (bounds.getCentreY()).withHeight (UiTheme::Space::hairline));
    }

private:
    Axis axis_;
};

} // namespace yesdaw::ui
