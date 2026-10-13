#pragma once

// YES DAW - UI correctness pass unit 16: the export's progress readout, in the Export button's slot while a job runs.
// The slot is the Export button's own width (the header has no spare width at the window's minimum) and the cancel
// cross takes its right end, so the readout reads the percent alone over a bar along its foot; while the job cancels,
// the cross hides and "Cancelling…" (ADR-0058) takes the whole slot.

#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace yesdaw::ui {

class ExportProgressReadout final : public juce::Label
{
public:
    // The share of the job done, 0 to 1; below 0 draws no bar (no job, or one cancelling).
    void setFraction (double fraction)
    {
        if (fraction == fraction_)
            return;
        fraction_ = fraction;
        repaint();
    }
    [[nodiscard]] double fraction() const noexcept { return fraction_; }

    // The bar as painted: along the readout's foot, under its text, as wide as the share done.
    [[nodiscard]] juce::Rectangle<int> barBounds() const
    {
        if (fraction_ <= 0.0)
            return {};
        const auto foot = getLocalBounds().removeFromBottom (UiTheme::Layout::headerExportProgressBarHeight);
        return foot.withWidth (juce::roundToInt (foot.getWidth() * juce::jmin (1.0, fraction_)));
    }

    void paint (juce::Graphics& g) override
    {
        juce::Label::paint (g);
        if (const auto bar = barBounds(); ! bar.isEmpty())
        {
            g.setColour (UiTheme::Color::accentBlue());
            g.fillRect (bar);
        }
    }

private:
    double fraction_ = -1.0;
};

} // namespace yesdaw::ui
