// G4.2 cp2: the compressor / limiter face's gain-reduction meter. It shows what the running node
// published (the engine's acquire-load tap, sampled once per UI tick), never an estimate: no running
// reduction node reads "—", a bypassed insert reads its honest zero.
#pragma once

#include "ui/UiTheme.h"
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace yesdaw::ui {

class GainReductionMeterComponent final : public juce::Component,
                                          public juce::SettableTooltipClient
{
public:
    GainReductionMeterComponent()
    {
        setComponentID ("mixer.fx.editor.gr");
        setName ("Gain reduction");
        setTooltip ("Gain reduction the running insert applies now (dB); the line holds the recent peak");
        setWantsKeyboardFocus (false);
    }

    // A new insert: nothing measured yet.
    void reset()
    {
        if (! reading && current == 0.0f && held == 0.0f)
            return;
        reading = false;
        current = held = 0.0f;
        holdTicks = 0;
        repaint();
    }

    // One UI tick's sample. nullopt: the engine runs no reduction node for this insert.
    void pushReading (std::optional<float> gainReductionDb, bool insertBypassed)
    {
        using L = UiTheme::Layout;
        const bool nowReading = gainReductionDb.has_value();
        const float sample = nowReading && std::isfinite (*gainReductionDb) ? std::max (0.0f, *gainReductionDb) : 0.0f;
        float nextHeld = held;
        int nextHoldTicks = holdTicks;
        if (sample >= held)
        {
            nextHeld = sample;
            nextHoldTicks = L::grMeterHoldTicks;
        }
        else if (holdTicks > 0)
        {
            --nextHoldTicks;
        }
        else
        {
            nextHeld = std::max (sample, held - L::grMeterFallDbPerTick);
        }
        holdTicks = nextHoldTicks;
        if (nowReading == reading && sample == current && nextHeld == held && insertBypassed == bypassed)
            return;
        reading = nowReading;
        current = sample;
        held = nextHeld;
        bypassed = insertBypassed;
        repaint();
    }

    [[nodiscard]] bool hasReading() const noexcept { return reading; }
    [[nodiscard]] float currentDb() const noexcept { return current; }
    [[nodiscard]] float heldDb() const noexcept { return held; }

    [[nodiscard]] juce::Rectangle<int> barBounds() const
    {
        using L = UiTheme::Layout;
        auto area = getLocalBounds().withTrimmedTop (L::eqResponseLabelHeight)
                                    .withTrimmedLeft (L::eqResponseLabelWidth)
                                    .withTrimmedRight (L::eqResponseLabelWidth);
        return area.withHeight (L::grMeterBarHeight);
    }

    void paint (juce::Graphics& g) override
    {
        using L = UiTheme::Layout;
        g.fillAll (UiTheme::Color::controlInset());
        g.setFont (UiTheme::Type::font (UiTheme::Type::small));
        g.setColour (UiTheme::Color::mutedText());
        const juce::String state = ! reading ? "Gain reduction (dB) — not running"
                                 : bypassed  ? "Gain reduction (dB, bypassed)"
                                             : "Gain reduction (dB)";
        g.drawText (state, getLocalBounds().withHeight (L::eqResponseLabelHeight), juce::Justification::centred);

        const auto bar = barBounds();
        if (bar.isEmpty())
            return;
        g.setColour (UiTheme::Color::panel());
        g.fillRect (bar);
        g.setColour (UiTheme::Color::accentAmber());
        g.fillRect (bar.withWidth (xForDb (current) - bar.getX()));
        if (held > 0.0f)
        {
            g.setColour (UiTheme::Color::text());
            const float x = static_cast<float> (xForDb (held));
            g.drawLine (x, static_cast<float> (bar.getY()), x, static_cast<float> (bar.getBottom()), L::grMeterPeakStrokeWidth);
        }

        // The scale under the bar: the same marks a hardware GR meter carries.
        g.setColour (UiTheme::Color::mutedText());
        for (const float db : { 0.0f, 3.0f, 6.0f, 12.0f, 24.0f })
        {
            const int x = xForDb (db);
            g.drawVerticalLine (x, static_cast<float> (bar.getBottom()), static_cast<float> (bar.getBottom() + L::grMeterTickLength));
            g.drawText (juce::String (static_cast<int> (db)),
                        juce::Rectangle<int> (x - L::eqResponseLabelWidth / 2, bar.getBottom(), L::eqResponseLabelWidth, L::eqResponseLabelHeight),
                        juce::Justification::centred);
        }
        // The number, left of the bar: what the bar is showing right now.
        g.setColour (UiTheme::Color::text());
        g.drawText (reading ? juce::String (current > 0.0f ? -current : 0.0f, 1) : juce::String ("—"),
                    juce::Rectangle<int> (0, bar.getY(), L::eqResponseLabelWidth - L::grMeterValueInset, bar.getHeight()),
                    juce::Justification::centredRight);
    }

private:
    [[nodiscard]] int xForDb (float db) const
    {
        const auto bar = barBounds();
        const float fraction = std::clamp (db / UiTheme::Layout::grMeterScaleMaxDb, 0.0f, 1.0f);
        return bar.getX() + static_cast<int> (std::lround (fraction * static_cast<float> (bar.getWidth() - 1)));
    }

    float current = 0.0f;
    float held = 0.0f;
    int holdTicks = 0;
    bool reading = false;
    bool bypassed = false;
};

} // namespace yesdaw::ui
