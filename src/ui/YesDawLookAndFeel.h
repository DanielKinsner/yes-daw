// YES DAW - native premium control chrome.

#pragma once

#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace yesdaw::ui {

class YesDawLookAndFeel final : public juce::LookAndFeel_V4
{
public:
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

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return UiTheme::Type::font (UiTheme::Type::body, juce::Font::bold);
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

        if (highlighted || down)
        {
            g.setColour (UiTheme::Color::white().withAlpha (
                down ? UiTheme::Tone::pressedHighlightAlpha : UiTheme::Tone::hoverHighlightAlpha));
            g.fillRoundedRectangle (bounds, UiTheme::Radius::md);
        }

        g.setColour (UiTheme::Color::buttonBorder());
        g.drawRoundedRectangle (bounds.reduced (UiTheme::Layout::controlOutlineInset),
                                UiTheme::Radius::md,
                                UiTheme::Layout::controlOutlineStrokeWidth);

        g.setColour (UiTheme::Color::panelInnerHighlight().withAlpha (UiTheme::Tone::innerHighlightAlpha));
        g.drawHorizontalLine (UiTheme::Layout::controlInnerHighlightHeight,
                              bounds.getX() + UiTheme::Radius::md,
                              bounds.getRight() - UiTheme::Radius::md);

        if (button.hasKeyboardFocus (true))
        {
            g.setColour (UiTheme::Color::focusRing().withAlpha (UiTheme::Tone::focusRingAlpha));
            g.drawRoundedRectangle (bounds.reduced (static_cast<float> (UiTheme::Layout::controlFocusInset)),
                                    UiTheme::Radius::md,
                                    UiTheme::Layout::controlFocusStrokeWidth);
        }
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
                          button.getLocalBounds().reduced (UiTheme::Layout::controlTextHorizontalInset, 0),
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
