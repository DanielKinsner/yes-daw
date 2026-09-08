// G4.2: the configured EQ response, calculated on the message thread from the DSP coefficients.
#pragma once

#include "engine/Project.h"
#include "engine/nodes/EqNode.h"
#include "ui/UiTheme.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace yesdaw::ui {

class EqResponseComponent final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    EqResponseComponent()
    {
        setComponentID ("mixer.fx.editor.eq.response");
        setName ("EQ response");
        setTooltip ("Configured EQ response of all six bands; edit Frequency, Gain, Q and Type below");
        setWantsKeyboardFocus (false);
    }

    void setInsert (const engine::FxInsert& insert, double sampleRate)
    {
        const double rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        if (configured && rate == rateHz && enabled == insert.enabled && params == insert.normalizedParams)
            return;
        if (! configured || rate != rateHz || params != insert.normalizedParams)
        {
            rateHz = rate;
            params = insert.normalizedParams;
            // A fresh private copy restores omitted parameters on slot changes / undo.
            response = engine::EqNode {};
            response.prepare (rateHz, 1);
            for (const auto& [id, value] : params)
                response.setNormalizedParameter (id, value);
        }
        configured = true;
        enabled = insert.enabled;
        rebuildCurve();
        repaint();
    }

    [[nodiscard]] double responseDb (double frequencyHz) const noexcept
    {
        return enabled ? 20.0 * std::log10 (std::max (1.0e-12, response.magnitudeAtFrequency (frequencyHz))) : 0.0;
    }

    [[nodiscard]] double maximumFrequencyHz() const noexcept { return std::min (20000.0, rateHz * 0.49); }
    [[nodiscard]] const juce::Path& curvePath() const noexcept { return curve; }

    [[nodiscard]] juce::Rectangle<int> plotBounds() const
    {
        using L = UiTheme::Layout;
        return getLocalBounds().withTrimmedLeft (L::eqResponseLabelWidth)
                               .withTrimmedRight (L::keymapEditorGap)
                               .withTrimmedTop (L::eqResponseLabelHeight)
                               .withTrimmedBottom (L::eqResponseLabelHeight);
    }

    void resized() override { rebuildCurve(); }

    void paint (juce::Graphics& g) override
    {
        using L = UiTheme::Layout;
        g.fillAll (UiTheme::Color::controlInset());
        const auto plot = plotBounds();
        if (plot.isEmpty()) return;
        g.setFont (UiTheme::Type::font (UiTheme::Type::small));
        g.setColour (UiTheme::Color::mutedText());
        g.drawText (enabled ? "EQ response (dB)" : "EQ response (dB, bypassed)",
                    getLocalBounds().withHeight (L::eqResponseLabelHeight), juce::Justification::centred);
        for (const double db : { -24.0, -12.0, 0.0, 12.0, 24.0 })
        {
            const float y = yForDb (db);
            g.setColour (db == 0.0 ? UiTheme::Color::mutedText() : UiTheme::Color::separator());
            g.drawHorizontalLine (static_cast<int> (y), static_cast<float> (plot.getX()), static_cast<float> (plot.getRight()));
            g.setColour (UiTheme::Color::mutedText());
            g.drawText (juce::String (db > 0.0 ? "+" : "") + juce::String (db, 0),
                        juce::Rectangle<int> (0, static_cast<int> (y) - L::eqResponseLabelHeight / 2,
                                              L::eqResponseLabelWidth - L::keymapEditorGap, L::eqResponseLabelHeight),
                        juce::Justification::centredRight);
        }
        for (const double hz : { 20.0, 100.0, 1000.0, 10000.0 })
        {
            if (hz > maximumFrequencyHz()) continue;
            const float x = xForFrequency (hz);
            g.setColour (UiTheme::Color::separator());
            g.drawVerticalLine (static_cast<int> (x), static_cast<float> (plot.getY()), static_cast<float> (plot.getBottom()));
            g.setColour (UiTheme::Color::mutedText());
            g.drawText (hz >= 1000.0 ? juce::String (hz / 1000.0, 0) + "k" : juce::String (hz, 0),
                        juce::Rectangle<int> (static_cast<int> (x) - L::eqResponseLabelWidth / 2, plot.getBottom(),
                                              L::eqResponseLabelWidth, L::eqResponseLabelHeight),
                        juce::Justification::centred);
        }
        g.setColour (enabled ? UiTheme::Color::accentPurple() : UiTheme::Color::mutedText());
        g.strokePath (curve, juce::PathStrokeType (L::eqResponseStrokeWidth));
    }

private:
    [[nodiscard]] float xForFrequency (double hz) const
    {
        const auto plot = plotBounds();
        return static_cast<float> (plot.getX() + (plot.getWidth() - 1) * std::log (hz / 20.0) / std::log (maximumFrequencyHz() / 20.0));
    }

    [[nodiscard]] float yForDb (double db) const
    {
        const auto plot = plotBounds();
        return static_cast<float> (plot.getY() + (plot.getHeight() - 1) * (24.0 - std::clamp (db, -24.0, 24.0)) / 48.0);
    }

    void rebuildCurve()
    {
        curve.clear();
        const auto plot = plotBounds();
        if (plot.getWidth() < 2 || plot.getHeight() < 2 || maximumFrequencyHz() <= 20.0) return;
        // At most one evaluation per visible column, cached until settings or geometry change.
        for (int x = 0; x < plot.getWidth(); ++x)
        {
            const double hz = 20.0 * std::pow (maximumFrequencyHz() / 20.0, static_cast<double> (x) / (plot.getWidth() - 1));
            const auto point = juce::Point<float> (static_cast<float> (plot.getX() + x), yForDb (responseDb (hz)));
            if (x == 0) curve.startNewSubPath (point);
            else curve.lineTo (point);
        }
    }

    engine::EqNode response;
    std::vector<std::pair<std::uint32_t, double>> params;
    juce::Path curve;
    double rateHz = 48000.0;
    bool configured = false;
    bool enabled = true;
};

} // namespace yesdaw::ui
