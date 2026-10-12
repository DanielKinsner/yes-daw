#pragma once

// YES DAW - ADR-0076: the status line's text. One shortening rule for user and OS text (device names, file names,
// paths, joined refusals), and the label that paints it. The label's own text, the probe and accessibility keep the
// whole message; only the paint shortens.

#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace yesdaw::ui {

// ADR-0076's rule, pure: the text a status line shows for a message made of `parts` (one part for a single message - its
// own "; " is never a join), where `fits` answers whether a candidate paints inside the line's budget. Parts are never
// mixed: as many leading whole parts as fit are shown, then "; … N more"; when even the first part does not fit, that
// part alone is shortened in its middle, its lead and its end kept. Shortening is by code point, with U+2026.
[[nodiscard]] inline juce::String statusLineShownText (const juce::StringArray& parts,
                                                       const std::function<bool (const juce::String&)>& fits)
{
    const juce::String whole = parts.joinIntoString ("; ");
    if (whole.isEmpty() || fits (whole))
        return whole;
    const juce::String ellipsis = juce::String::charToString (static_cast<juce::juce_wchar> (0x2026));
    const auto moreMarker = [&ellipsis] (int dropped) {
        return dropped > 0 ? "; " + ellipsis + " " + juce::String (dropped) + " more" : juce::String();
    };
    for (int kept = parts.size() - 1; kept >= 1; --kept)
    {
        juce::StringArray head;
        for (int i = 0; i < kept; ++i)
            head.add (parts[i]);
        const juce::String candidate = head.joinIntoString ("; ") + moreMarker (parts.size() - kept);
        if (fits (candidate))
            return candidate;
    }
    // Even the first part does not fit (with its marker): shorten it alone, keeping the most code points that fit.
    const juce::String first = parts[0];
    const juce::String marker = moreMarker (parts.size() - 1);
    const int length = first.length();
    juce::String best = ellipsis + marker;
    for (int low = 0, high = length - 1; low <= high;)
    {
        const int keep = (low + high) / 2;
        const juce::String candidate = first.substring (0, (keep + 1) / 2) + ellipsis + first.substring (length - keep / 2) + marker;
        if (fits (candidate))
        {
            best = candidate;
            low = keep + 1;
        }
        else
        {
            high = keep - 1;
        }
    }
    return best;
}

// One message, one part.
[[nodiscard]] inline juce::String statusLineShownText (const juce::String& text, const std::function<bool (const juce::String&)>& fits)
{
    return statusLineShownText (juce::StringArray { text }, fits);
}

// ADR-0076: the status line. It paints its text whole when it fits the budget (the text area less
// statusLineTextSpare), else statusLineShownText's form, measured with the routine the paint uses; never squashed.
// While shortened, its tooltip is the whole text and it takes the mouse so the tooltip can show.
class StatusLineLabel final : public juce::Label
{
public:
    StatusLineLabel()
    {
        setMinimumHorizontalScale (1.0f);
        setMouseClickGrabsKeyboardFocus (false);
        setInterceptsMouseClicks (false, false);
    }

    // A message and the parts it was joined from (ADR-0076): the text is their "; " join.
    void setMessage (const juce::String& whole, const juce::StringArray& messageParts)
    {
        settingMessage = true;
        parts = messageParts;
        setText (whole, juce::dontSendNotification);
        settingMessage = false;
        refreshShortening();
    }

    // The tooltip while the text is whole.
    void setRestingTooltip (const juce::String& tooltip)
    {
        restingTooltip = tooltip;
        refreshShortening();
    }

    // The text as painted: the whole message, or its shortened form.
    [[nodiscard]] const juce::String& paintedText() const noexcept { return painted; }
    [[nodiscard]] bool isShortened() const noexcept { return painted != getText(); }
    [[nodiscard]] const juce::String& currentTooltip() const noexcept { return tooltipNow; }

    void paint (juce::Graphics& g) override
    {
        g.setColour (findColour (juce::Label::textColourId));
        g.setFont (getLookAndFeel().getLabelFont (*this));
        g.drawFittedText (painted, getBorderSize().subtractedFrom (getLocalBounds()), getJustificationType(), 1, 1.0f);
    }

    void resized() override
    {
        juce::Label::resized();
        refreshShortening();
    }

protected:
    void textWasChanged() override
    {
        if (! settingMessage)
            parts = juce::StringArray { getText() };   // set as one text: one part
        refreshShortening();
    }

private:
    void refreshShortening()
    {
        const juce::Font labelFont = getLookAndFeel().getLabelFont (*this);
        const float budget = static_cast<float> (getBorderSize().subtractedFrom (getLocalBounds()).getWidth()
                                                 - UiTheme::Layout::statusLineTextSpare);
        painted = statusLineShownText (parts.joinIntoString ("; ") == getText() ? parts : juce::StringArray { getText() },
                                       [&labelFont, budget] (const juce::String& candidate) {
            return juce::GlyphArrangement::getStringWidth (labelFont, candidate) <= budget;
        });
        const bool shortened = isShortened();
        tooltipNow = shortened ? getText() : restingTooltip;
        setTooltip (tooltipNow);
        setInterceptsMouseClicks (shortened, false);
    }

    juce::String painted;
    juce::String restingTooltip;
    juce::String tooltipNow;
    juce::StringArray parts;
    bool settingMessage = false;
};

} // namespace yesdaw::ui
