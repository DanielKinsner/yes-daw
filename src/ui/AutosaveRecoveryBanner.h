#pragma once

#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <set>

namespace yesdaw::ui {

// ADR-0068 §6 / ADR-0035: the autosave recovery question while it is unanswered - a card over the top of the arrange
// (the ruler; the lanes stay in view), never modal: every other control stays live. Its first line is the question;
// under it, "Autosaved" and "Saved" each name what that side holds. The shell's Restore and Discard buttons sit in its
// right-hand end; the card is as tall as its text needs at the width it is given (heightFor).
class AutosaveRecoveryBannerComponent final : public juce::Component,
                                              public juce::SettableTooltipClient
{
public:
    AutosaveRecoveryBannerComponent()
    {
        setComponentID ("autosave.recovery.banner");
        setTitle ("Autosave recovery");
        setTooltip ("An autosave holds work this project lacks: Restore it, or Discard it and keep the saved version");
        setWantsKeyboardFocus (false);
        // Weight is not the cue: a bold face the platform lacks is measured bold but drawn regular, and the wrap below
        // would then reserve a line the question never uses. The question is the brighter, larger line; the keys of
        // the two rows are bright, their counts muted.
        question.setFont (UiTheme::Type::font (UiTheme::Type::body));
        question.setColour (juce::Label::textColourId, UiTheme::Color::text());
        for (juce::Label* key : { &rows[0].key, &rows[1].key })
        {
            key->setFont (UiTheme::Type::font (UiTheme::Type::small));
            key->setColour (juce::Label::textColourId, UiTheme::Color::text());
        }
        for (juce::Label* value : { &rows[0].value, &rows[1].value })
        {
            value->setFont (UiTheme::Type::font (UiTheme::Type::small));
            value->setColour (juce::Label::textColourId, UiTheme::Color::mutedText());
        }
        for (juce::Label* label : { &question, &rows[0].key, &rows[0].value, &rows[1].key, &rows[1].value })
        {
            label->setJustificationType (juce::Justification::topLeft);
            label->setMinimumHorizontalScale (1.0f);   // wrap, never squash
            label->setBorderSize ({});
            addAndMakeVisible (*label);
        }
    }

    // The whole question: its first line, then "Autosaved: ..." and "Saved: ..." (autosaveRecoveryPromptText).
    void setMessage (const juce::String& message)
    {
        if (whole == message)
            return;
        whole = message;
        juce::StringArray lines;
        lines.addLines (message);
        question.setText (lines[0], juce::dontSendNotification);
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            const juce::String line = lines[static_cast<int> (i) + 1];
            rows[i].key.setText (line.upToFirstOccurrenceOf (":", false, false), juce::dontSendNotification);
            rows[i].value.setText (line.fromFirstOccurrenceOf (":", false, false).trimStart(), juce::dontSendNotification);
        }
        setDescription (message);
        resized();
    }

    [[nodiscard]] juce::String message() const { return whole; }

    // The height the card needs at `width`: the question's wrapped lines, then the two rows.
    [[nodiscard]] int heightFor (int width) const
    {
        using L = UiTheme::Layout;
        const int textWidth = textAreaWidth (width);
        return 2 * L::autosaveBannerInset + linesOf (question, textWidth) * L::autosaveBannerLineHeight
             + L::autosaveBannerLineGap + rowsHeight (textWidth);
    }

    // Where the shell places Restore and Discard: the card's right-hand end, vertically centred.
    [[nodiscard]] juce::Rectangle<int> buttonArea() const noexcept
    {
        using L = UiTheme::Layout;
        const auto inner = getLocalBounds().reduced (L::autosaveBannerInset);
        return inner.withLeft (inner.getRight() - L::autosaveBannerButtonsWidth)
            .withSizeKeepingCentre (L::autosaveBannerButtonsWidth, L::autosaveBannerButtonHeight);
    }

    void paint (juce::Graphics& g) override
    {
        using L = UiTheme::Layout;
        g.fillAll (UiTheme::Color::panelRaised());
        g.setColour (UiTheme::Color::accentAmber().withAlpha (L::autosaveBannerOutlineAlpha));
        g.drawRect (getLocalBounds(), 1);
        g.setColour (UiTheme::Color::accentAmber());
        g.fillRect (getLocalBounds().removeFromLeft (L::autosaveBannerAccentWidth));
    }

    void resized() override
    {
        using L = UiTheme::Layout;
        const int textWidth = textAreaWidth (getWidth());
        auto block = getLocalBounds().reduced (L::autosaveBannerInset)
                         .withTrimmedLeft (L::autosaveBannerAccentWidth)
                         .withWidth (textWidth);
        const int questionHeight = linesOf (question, textWidth) * L::autosaveBannerLineHeight;
        block = block.withSizeKeepingCentre (block.getWidth(),
                                             std::min (block.getHeight(), questionHeight + L::autosaveBannerLineGap + rowsHeight (textWidth)));
        question.setBounds (block.removeFromTop (questionHeight));
        block.removeFromTop (L::autosaveBannerLineGap);
        for (Row& row : rows)
        {
            auto line = block.removeFromTop (linesOf (row.value, textWidth - L::autosaveBannerKeyWidth) * L::autosaveBannerLineHeight);
            row.key.setBounds (line.removeFromLeft (L::autosaveBannerKeyWidth));
            row.value.setBounds (line);
        }
    }

private:
    struct Row
    {
        juce::Label key;
        juce::Label value;
    };

    [[nodiscard]] static int textAreaWidth (int cardWidth) noexcept
    {
        using L = UiTheme::Layout;
        return std::max (1, cardWidth - 2 * L::autosaveBannerInset - L::autosaveBannerAccentWidth - L::autosaveBannerButtonsWidth
                                - L::autosaveBannerInset);
    }

    // Counted with the routine the label paints with (drawFittedText's), so the slot matches what is drawn.
    [[nodiscard]] static int linesOf (const juce::Label& label, int width)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addFittedText (label.getFont(), label.getText(), 0.0f, 0.0f, static_cast<float> (std::max (1, width)), 1000.0f,
                              juce::Justification::topLeft, UiTheme::Layout::autosaveBannerMaxLines,
                              label.getMinimumHorizontalScale());
        std::set<int> baselines;
        for (int i = 0; i < glyphs.getNumGlyphs(); ++i)
            baselines.insert (juce::roundToInt (glyphs.getGlyph (i).getBaselineY()));
        return std::max (1, static_cast<int> (baselines.size()));
    }

    [[nodiscard]] int rowsHeight (int textWidth) const
    {
        int lines = 0;
        for (const Row& row : rows)
            lines += linesOf (row.value, textWidth - UiTheme::Layout::autosaveBannerKeyWidth);
        return lines * UiTheme::Layout::autosaveBannerLineHeight;
    }

    juce::String whole;
    juce::Label question;
    std::array<Row, 2> rows;
};

} // namespace yesdaw::ui
