// YES DAW - native premium control chrome.

#pragma once

#include "ui/PointerStroke.h"   // ADR-0072 §8: the native widgets' hover and pressed forms
#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace yesdaw::ui {

class YesDawLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    // ADR-0053: a TextButton whose properties carry this flag (true) paints the compact face — the small type
    // and a narrow inset — for words in a tight pill (the header card's DIM / MUTE). Opt-in, never inferred.
    static constexpr const char* kCompactTextButton = "yesdaw.compactTextButton";
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

    juce::Font getLabelFont (juce::Label&) override
    {
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

} // namespace yesdaw::ui
