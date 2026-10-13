// YES DAW - native premium control chrome.

#pragma once

#include "ui/PointerStroke.h"   // ADR-0072 §8: the native widgets' hover and pressed forms
#include "ui/StatusLineText.h"  // UI pass unit 19: ADR-0076's shortening rule for OS text
#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace yesdaw::ui {

// UI pass unit 19: the text box of a chooser of OS text (a device's name). The theme draws it whole or shortened in its
// middle, never squashed. A chooser's tooltip is its text box's (JUCE's ComboBox::getTooltip), and this one names the
// whole chosen item after the subject the chooser was given: "Audio output device: Speakers (Focusrite USB Audio)".
class OsTextLabel final : public juce::Label
{
public:
    juce::String getTooltip() override
    {
        auto* box = dynamic_cast<juce::ComboBox*> (getParentComponent());
        if (box == nullptr)
            return juce::Label::getTooltip();
        // The subject is the chooser's own (ComboBox::setTooltip stores it there too), never this text box's: JUCE copies a
        // text box's tooltip into the one it rebuilds, so the name would join the subject again at every rebuild.
        const juce::String subject = box->juce::SettableTooltipClient::getTooltip();
        const juce::String name = box->getSelectedId() > 0 ? box->getText() : juce::String();
        return name.isEmpty() ? subject : subject + ": " + name;
    }
};

class YesDawLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    // ADR-0053: a TextButton whose properties carry this flag (true) paints the compact face — the small type
    // and a narrow inset — for words in a tight pill (the header card's DIM / MUTE). Opt-in, never inferred.
    static constexpr const char* kCompactTextButton = "yesdaw.compactTextButton";
    // A label that draws in its own font (UI correctness pass unit 13: the strip drag readout matches its cell's numeric
    // caption); every other label keeps the theme's body font.
    static constexpr const char* kOwnFontLabel = "yesdaw.ownFontLabel";
    // A ComboBox whose properties carry this flag (true) shows OS text - a device's name (UI pass unit 19): its text box is
    // an OsTextLabel, never squashed; a name that does not fit is shortened in its middle by ADR-0076's rule.
    static constexpr const char* kOsTextComboBox = "yesdaw.osTextComboBox";
    [[nodiscard]] static juce::String osTextShown (const juce::String& text, const juce::Font& font, int width)
    {
        const float budget = static_cast<float> (width - UiTheme::Layout::statusLineTextSpare);
        return statusLineShownText (text, [&font, budget] (const juce::String& candidate) {
            return juce::GlyphArrangement::getStringWidth (font, candidate) <= budget;
        });
    }
    [[nodiscard]] static bool isCompactTextButton (const juce::Button& button)
    {
        return static_cast<bool> (button.getProperties()[kCompactTextButton]);
    }

    YesDawLookAndFeel()
    {
        setColour (juce::TextButton::buttonColourId, UiTheme::Color::buttonSurface());
        setColour (juce::TextButton::buttonOnColourId, UiTheme::Color::accentPurpleDeep());
        setColour (juce::TextButton::textColourOffId, UiTheme::Color::text());
        setColour (juce::TextButton::textColourOnId, UiTheme::Color::text());
        setColour (juce::ComboBox::backgroundColourId, UiTheme::Color::buttonSurface());
        setColour (juce::ComboBox::outlineColourId, UiTheme::Color::buttonBorder());
        setColour (juce::ComboBox::textColourId, UiTheme::Color::text());
        setColour (juce::ComboBox::arrowColourId, UiTheme::Color::buttonTextMuted());
        setColour (juce::Label::textColourId, UiTheme::Color::text());
        setColour (juce::Slider::backgroundColourId, UiTheme::Color::meterTrack());
        setColour (juce::Slider::trackColourId, UiTheme::Color::accentPurple());
        setColour (juce::Slider::thumbColourId, UiTheme::Color::faderThumb());
        // UI pass unit 18: alerts in the theme's raised panel, text and stroke.
        setColour (juce::AlertWindow::backgroundColourId, UiTheme::Color::panelRaised());
        setColour (juce::AlertWindow::textColourId, UiTheme::Color::text());
        setColour (juce::AlertWindow::outlineColourId, UiTheme::Color::panelStroke());
    }

    juce::Font getTextButtonFont (juce::TextButton& button, int) override
    {
        return UiTheme::Type::font (isCompactTextButton (button) ? UiTheme::Type::small : UiTheme::Type::body,
                                    juce::Font::bold);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return UiTheme::Type::font (UiTheme::Type::body);
    }

    juce::Label* createComboBoxTextBox (juce::ComboBox& box) override
    {
        if (static_cast<bool> (box.getProperties()[kOsTextComboBox]))
            return new OsTextLabel();
        return juce::LookAndFeel_V4::createComboBoxTextBox (box);
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        if (dynamic_cast<const OsTextLabel*> (&label) == nullptr || label.isBeingEdited())
        {
            juce::LookAndFeel_V4::drawLabel (g, label);
            return;
        }
        // As JUCE's label draws, but at full width: the whole name, or ADR-0076's shortened form of it.
        g.fillAll (label.findColour (juce::Label::backgroundColourId));
        const juce::Font font = getLabelFont (label);
        const auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
        g.setFont (font);
        g.drawFittedText (osTextShown (label.getText(), font, area.getWidth()), area, label.getJustificationType(), 1, 1.0f);
        g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f));
        g.drawRect (label.getLocalBounds());
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        if (static_cast<bool> (label.getProperties()[kOwnFontLabel]))
            return label.getFont();
        return UiTheme::Type::font (UiTheme::Type::body);
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool highlighted,
                               bool down) override
    {
        juce::Graphics::ScopedSaveState state (g);
        const float opacity = button.isEnabled() ? 1.0f : UiTheme::Tone::disabledAlpha;
        g.setOpacity (opacity);

        auto bounds = button.getLocalBounds().toFloat();
        auto shadow = bounds.translated (0.0f, static_cast<float> (UiTheme::Layout::controlShadowOffset));
        g.setColour (UiTheme::Color::panelShadow().withAlpha (UiTheme::Tone::shadowAlpha));
        g.fillRoundedRectangle (shadow, UiTheme::Radius::md);

        const juce::Colour base = down ? UiTheme::Color::buttonPressed() : backgroundColour;
        juce::ColourGradient gradient (down ? base : UiTheme::Color::buttonSurfaceTop(),
                                       bounds.getCentreX(), bounds.getY(),
                                       base, bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (bounds, UiTheme::Radius::md);

        g.setColour (UiTheme::Color::buttonBorder());
        g.drawRoundedRectangle (bounds.reduced (UiTheme::Layout::controlOutlineInset),
                                UiTheme::Radius::md,
                                UiTheme::Layout::controlOutlineStrokeWidth);

        g.setColour (UiTheme::Color::panelInnerHighlight().withAlpha (UiTheme::Tone::innerHighlightAlpha));
        g.drawHorizontalLine (UiTheme::Layout::controlInnerHighlightHeight,
                              bounds.getX() + UiTheme::Radius::md,
                              bounds.getRight() - UiTheme::Radius::md);

        // ADR-0072 §8: hover a 1 px inner stroke, pressed a 2 px one (over the pressed background), never both.
        paintPointerStroke (g, bounds, pointerStrokeForm (highlighted, down, pointerPressedStrokeWidthForHeight (button.getHeight())),
                            UiTheme::Radius::md);

        if (button.hasKeyboardFocus (true))
        {
            g.setColour (UiTheme::Color::focusRing().withAlpha (UiTheme::Tone::focusRingAlpha));
            g.drawRoundedRectangle (bounds.reduced (static_cast<float> (UiTheme::Layout::controlFocusInset)),
                                    UiTheme::Radius::md,
                                    UiTheme::Layout::controlFocusStrokeWidth);
        }
    }

    // ADR-0072 §8: JUCE's toggle (tick box and label), with the same hover and pressed forms over it.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) override
    {
        juce::LookAndFeel_V4::drawToggleButton (g, button, highlighted, down);
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (button.isEnabled() ? 1.0f : UiTheme::Tone::disabledAlpha);
        paintPointerStroke (g, button.getLocalBounds().toFloat(),
                            pointerStrokeForm (highlighted, down, pointerPressedStrokeWidthForHeight (button.getHeight())),
                            UiTheme::Radius::md);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool,
                         bool) override
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (button.isEnabled() ? 1.0f : UiTheme::Tone::disabledAlpha);
        g.setColour (button.findColour (button.getToggleState()
                                            ? juce::TextButton::textColourOnId
                                            : juce::TextButton::textColourOffId));
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (isCompactTextButton (button)
                                                               ? UiTheme::Layout::compactTextHorizontalInset
                                                               : UiTheme::Layout::controlTextHorizontalInset,
                                                           0),
                          juce::Justification::centred,
                          1);
    }

    void drawLinearSlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float minSliderPos,
                           float,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (slider.findColour (juce::Slider::backgroundColourId).isTransparent()
            && slider.findColour (juce::Slider::trackColourId).isTransparent()
            && slider.findColour (juce::Slider::thumbColourId).isTransparent())
        {
            return;
        }

        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (slider.isEnabled() ? 1.0f : UiTheme::Tone::disabledAlpha);
        const bool vertical = style == juce::Slider::LinearVertical
                           || style == juce::Slider::LinearBarVertical;
        const float trackThickness = static_cast<float> (UiTheme::Layout::sliderTrackThickness);
        const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();

        // E24: a horizontal LinearBar is a value SCRUB CELL — a quiet filled bar with the value
        // text drawn over it, and NO round thumb (the thumb used to paint over the digits).
        if (style == juce::Slider::LinearBar)
        {
            const auto cell = juce::Rectangle<float> (static_cast<float> (x),
                                                      static_cast<float> (y),
                                                      static_cast<float> (width),
                                                      static_cast<float> (height));
            g.setColour (slider.findColour (juce::Slider::backgroundColourId));
            g.fillRoundedRectangle (cell, UiTheme::Radius::sm);
            auto fill = cell.withRight (juce::jlimit (cell.getX(), cell.getRight(), sliderPos));
            g.setColour (slider.findColour (juce::Slider::trackColourId)
                             .withAlpha (UiTheme::Tone::trackSliderFillAlpha));
            g.fillRoundedRectangle (fill, UiTheme::Radius::sm);
            paintPointerStroke (g, cell, sliderStrokeForm (slider), UiTheme::Radius::sm);
            g.setColour (slider.findColour (juce::Slider::textBoxTextColourId));
            g.setFont (UiTheme::Type::numericFont (UiTheme::Type::small));
            g.drawText (slider.getTextFromValue (slider.getValue()),
                        cell.toNearestInt(),
                        juce::Justification::centred,
                        false);
            return;
        }

        if (vertical)
        {
            const auto rail = juce::Rectangle<float> (
                static_cast<float> (x) + (static_cast<float> (width) - trackThickness) * 0.5f,
                static_cast<float> (y),
                trackThickness,
                static_cast<float> (height));
            g.setColour (slider.findColour (juce::Slider::backgroundColourId));
            g.fillRoundedRectangle (rail, UiTheme::Radius::pill);

            auto active = rail.withY (sliderPos).withBottom (rail.getBottom());
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.fillRoundedRectangle (active, UiTheme::Radius::pill);
            paintPointerStroke (g, area, sliderStrokeForm (slider), UiTheme::Radius::sm);   // under the thumb

            auto thumb = juce::Rectangle<float> (
                static_cast<float> (x + width / 2 - UiTheme::Layout::sliderThumbLongSide / 2),
                sliderPos - static_cast<float> (UiTheme::Layout::sliderThumbShortSide) * 0.5f,
                static_cast<float> (UiTheme::Layout::sliderThumbLongSide),
                static_cast<float> (UiTheme::Layout::sliderThumbShortSide));
            drawFaderThumb (g, thumb, slider.findColour (juce::Slider::thumbColourId));
            return;
        }

        const auto rail = juce::Rectangle<float> (
            static_cast<float> (x),
            static_cast<float> (y) + (static_cast<float> (height) - trackThickness) * 0.5f,
            static_cast<float> (width),
            trackThickness);
        g.setColour (slider.findColour (juce::Slider::backgroundColourId));
        g.fillRoundedRectangle (rail, UiTheme::Radius::pill);

        const float left = juce::jmin (minSliderPos, sliderPos);
        const float right = juce::jmax (minSliderPos, sliderPos);
        auto active = rail.withX (left).withRight (right);
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.fillRoundedRectangle (active, UiTheme::Radius::pill);
        paintPointerStroke (g, area, sliderStrokeForm (slider), UiTheme::Radius::sm);   // under the thumb

        const float thumbDiameter = static_cast<float> (UiTheme::Layout::sliderThumbDiameter);
        auto thumb = juce::Rectangle<float> (sliderPos - thumbDiameter * 0.5f,
                                             rail.getCentreY() - thumbDiameter * 0.5f,
                                             thumbDiameter,
                                             thumbDiameter);
        g.setColour (UiTheme::Color::panelShadow().withAlpha (UiTheme::Tone::shadowAlpha));
        g.fillEllipse (thumb.translated (0.0f, static_cast<float> (UiTheme::Layout::controlShadowOffset)));
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillEllipse (thumb);
        g.setColour (UiTheme::Color::faderThumbTop());
        g.drawEllipse (thumb.reduced (UiTheme::Layout::controlOutlineInset),
                       UiTheme::Layout::controlOutlineStrokeWidth);
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (slider.isEnabled() ? 1.0f : UiTheme::Tone::disabledAlpha);
        const float diameter = static_cast<float> (juce::jmin (width, height));
        auto bounds = juce::Rectangle<float> (
            static_cast<float> (x) + (static_cast<float> (width) - diameter) * 0.5f,
            static_cast<float> (y) + (static_cast<float> (height) - diameter) * 0.5f,
            diameter,
            diameter);
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour (UiTheme::Color::panelShadow().withAlpha (UiTheme::Tone::shadowAlpha));
        g.fillEllipse (bounds.translated (
            0.0f,
            static_cast<float> (UiTheme::Layout::controlShadowOffset)));
        g.setColour (UiTheme::Color::knobFace());
        g.fillEllipse (bounds);
        g.setColour (UiTheme::Color::knobArc());
        g.drawEllipse (bounds.reduced (UiTheme::Layout::controlOutlineInset),
                       UiTheme::Layout::controlOutlineStrokeWidth);

        juce::Path activeArc;
        activeArc.addCentredArc (bounds.getCentreX(),
                                 bounds.getCentreY(),
                                 bounds.getWidth() * 0.43f,
                                 bounds.getHeight() * 0.43f,
                                 0.0f,
                                 rotaryStartAngle,
                                 angle,
                                 true);
        g.setColour (slider.findColour (juce::Slider::trackColourId));
        g.strokePath (activeArc,
                      juce::PathStrokeType (UiTheme::Layout::iconBoldStrokeWidth,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

        const float radius = bounds.getWidth() * 0.31f;
        g.drawLine (bounds.getCentreX(),
                    bounds.getCentreY(),
                    bounds.getCentreX() + std::sin (angle) * radius,
                    bounds.getCentreY() - std::cos (angle) * radius,
                    UiTheme::Layout::iconBoldStrokeWidth);
        paintPointerStroke (g, bounds, sliderStrokeForm (slider), UiTheme::Radius::sm);   // round the knob, as a painted one
    }

    void drawComboBox (juce::Graphics& g,
                       int width,
                       int height,
                       bool down,
                       int,
                       int,
                       int,
                       int,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (static_cast<float> (width), static_cast<float> (height));
        const juce::Colour base = down ? UiTheme::Color::buttonPressed()
                                      : box.findColour (juce::ComboBox::backgroundColourId);
        juce::ColourGradient gradient (UiTheme::Color::buttonSurfaceTop(), bounds.getCentreX(), bounds.getY(),
                                       base, bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (bounds, UiTheme::Radius::md);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds.reduced (UiTheme::Layout::controlOutlineInset),
                                UiTheme::Radius::md,
                                UiTheme::Layout::controlOutlineStrokeWidth);
        paintPointerStroke (g, bounds,
                            pointerStrokeForm (box.isEnabled() && box.isMouseOver (true), down, pointerPressedStrokeWidthForHeight (height)),
                            UiTheme::Radius::md);

        const float arrowRight = bounds.getRight() - static_cast<float> (UiTheme::Layout::comboArrowRightInset);
        const float arrowTop = bounds.getCentreY()
                             - static_cast<float> (UiTheme::Layout::comboArrowHeight) * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath (arrowRight - static_cast<float> (UiTheme::Layout::comboArrowWidth), arrowTop);
        arrow.lineTo (arrowRight - static_cast<float> (UiTheme::Layout::comboArrowWidth) * 0.5f,
                      arrowTop + static_cast<float> (UiTheme::Layout::comboArrowHeight));
        arrow.lineTo (arrowRight, arrowTop);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (arrow,
                      juce::PathStrokeType (UiTheme::Layout::iconStrokeWidth,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    }

    // UI pass unit 18: the alerts. JUCE's V4 alert draws a 130 px translucent icon bleeding off its top-left corner and
    // under the title; ours draws the icon whole in the alert's left gutter, beside the title, in the theme's type.
    juce::Font getAlertWindowTitleFont() override { return UiTheme::Type::font (UiTheme::Type::title, juce::Font::bold); }
    juce::Font getAlertWindowMessageFont() override { return UiTheme::Type::font (UiTheme::Type::body); }
    juce::Font getAlertWindowFont() override { return UiTheme::Type::font (UiTheme::Type::body); }

    // Where an alert of this kind draws its icon (empty for none), in the alert's own coordinates.
    [[nodiscard]] static juce::Rectangle<int> alertIconBounds (juce::MessageBoxIconType type) noexcept
    {
        if (type == juce::MessageBoxIconType::NoIcon)
            return {};
        return { UiTheme::Layout::alertIconLeft, UiTheme::Layout::alertIconTop,
                 UiTheme::Layout::alertIconSize, UiTheme::Layout::alertIconSize };
    }
    // The icon's fill: amber for a warning, blue for a question or a note.
    [[nodiscard]] static juce::Colour alertIconColour (juce::MessageBoxIconType type) noexcept
    {
        return type == juce::MessageBoxIconType::WarningIcon ? UiTheme::Color::accentAmber() : UiTheme::Color::accentBlue();
    }

    void drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>&,
                       juce::TextLayout& textLayout) override
    {
        const auto bounds = alert.getLocalBounds();
        g.setColour (alert.findColour (juce::AlertWindow::backgroundColourId));
        g.fillRect (bounds);
        g.setColour (alert.findColour (juce::AlertWindow::outlineColourId));
        g.drawRect (bounds, 1);

        const auto type = alert.getAlertType();
        if (const auto icon = alertIconBounds (type).toFloat(); ! icon.isEmpty())
        {
            const bool warning = type == juce::MessageBoxIconType::WarningIcon;
            juce::Path shape;
            if (warning)
            {
                shape.addTriangle (icon.getCentreX(), icon.getY(), icon.getRight(), icon.getBottom(), icon.getX(), icon.getBottom());
                shape = shape.createPathWithRoundedCorners (UiTheme::Radius::sm);
            }
            else
            {
                shape.addEllipse (icon);
            }
            g.setColour (alertIconColour (type));
            g.fillPath (shape);
            g.setColour (UiTheme::Color::textOnFill());
            g.setFont (UiTheme::Type::font (UiTheme::Type::title, juce::Font::bold));
            const juce::String mark = warning ? "!" : type == juce::MessageBoxIconType::InfoIcon ? "i" : "?";
            g.drawText (mark, warning ? icon.withTrimmedTop (icon.getHeight() * 0.3f) : icon, juce::Justification::centred, false);
        }

        // The text where JUCE laid it out: past the icon's gutter when there is an icon, above the buttons.
        const int textLeft = type == juce::MessageBoxIconType::NoIcon ? 0 : UiTheme::Layout::alertTextLeftWithIcon;
        g.setColour (alert.findColour (juce::AlertWindow::textColourId));
        textLayout.draw (g, juce::Rectangle<int> (bounds.getX() + textLeft, 30, bounds.getWidth(),
                                                  bounds.getHeight() - getAlertWindowButtonHeight() - 20).toFloat());
    }

private:
    // A slider is hovered while the pointer is over it (or it is being dragged) and pressed while a button is down on it -
    // any button: ADR-0072 §8 reads a native widget's own state (JUCE's isMouseButtonDown), where §5's "a right or middle
    // press presses nothing" is the painted controls' seam.
    [[nodiscard]] static PointerStrokeForm sliderStrokeForm (const juce::Slider& slider)
    {
        return pointerStrokeForm (slider.isEnabled() && slider.isMouseOverOrDragging(), slider.isEnabled() && slider.isMouseButtonDown(),
                                  pointerPressedStrokeWidthForHeight (slider.getHeight()));
    }

    static void drawFaderThumb (juce::Graphics& g,
                                juce::Rectangle<float> thumb,
                                juce::Colour colour)
    {
        g.setColour (UiTheme::Color::panelShadow().withAlpha (UiTheme::Tone::shadowAlpha));
        g.fillRoundedRectangle (
            thumb.translated (0.0f, static_cast<float> (UiTheme::Layout::controlShadowOffset)),
            UiTheme::Radius::md);
        juce::ColourGradient gradient (UiTheme::Color::faderThumbTop(), thumb.getCentreX(), thumb.getY(),
                                       colour, thumb.getCentreX(), thumb.getBottom(), false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (thumb, UiTheme::Radius::md);
        g.setColour (UiTheme::Color::panelInnerHighlight());
        g.fillRoundedRectangle (
            thumb.withHeight (static_cast<float> (UiTheme::Layout::sliderThumbHighlightHeight)),
            UiTheme::Radius::sm);
    }
};

// UI pass unit 19: a chooser of OS text (a device's name): its text box is an OsTextLabel under the theme. (JUCE builds a
// first text box inside ComboBox's constructor, before this flag exists; the theme builds the one that shows when the
// chooser joins the shell, whose look is its own, so that change always comes.)
class OsTextComboBox final : public juce::ComboBox
{
public:
    OsTextComboBox() { getProperties().set (YesDawLookAndFeel::kOsTextComboBox, true); }
};

} // namespace yesdaw::ui
