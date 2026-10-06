// G4.2 cp4: the reverb's face — the tail a private copy of the real ReverbNode makes from a centred click,
// as an energy decay in dB (10 ms bins, wet only so the shape shows at any mix). Drawn from the DSP itself
// (the EQ face's law): pre-delay, decay, size and damping show exactly as they ring.
#pragma once

#include "engine/Project.h"
#include "engine/nodes/ReverbNode.h"
#include "ui/UiTheme.h"
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace yesdaw::ui {

class ReverbDecayComponent final : public juce::Component,
                                   public juce::SettableTooltipClient
{
public:
    ReverbDecayComponent()
    {
        setComponentID ("mixer.fx.editor.reverb.decay");
        setName ("Reverb decay");
        setTooltip ("How the reverb's tail rings out from a click (wet, in dB); edit Pre-delay, Decay, Size, Damping and Mix below");
        setWantsKeyboardFocus (false);
    }

    void setInsert (const engine::FxInsert& insert, double sampleRate)
    {
        const double rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        if (configured && rate == rateHz && enabled == insert.enabled && params == insert.normalizedParams)
            return;
        const bool tailChanged = ! configured || rate != rateHz || params != insert.normalizedParams;
        rateHz = rate;
        params = insert.normalizedParams;
        enabled = insert.enabled;
        configured = true;
        if (tailChanged)
            simulate();
        repaint();
    }

    // The envelope: one level per kBinMs bin from the click, 0 dB at the loudest bin, floored at -90 dB.
    [[nodiscard]] const std::vector<float>& envelopeDb() const noexcept { return envelope; }
    [[nodiscard]] static constexpr double binMs() noexcept { return kBinMs; }
    [[nodiscard]] double windowMs() const noexcept { return window; }
    [[nodiscard]] std::size_t simulatedFrames() const noexcept { return lastFrames; }
    [[nodiscard]] static constexpr double maxWindowMs() noexcept { return kMaxWindowMs; }
    [[nodiscard]] bool isBypassedFace() const noexcept { return ! enabled; }
    [[nodiscard]] double configuredPreDelayMs() const { return realParam (engine::ReverbNode::kPreDelayParamId); }
    [[nodiscard]] double configuredDecaySeconds() const { return realParam (engine::ReverbNode::kRt60ParamId); }

    // When the tail first rises out of the floor, and how long it then takes to fall 60 dB from its peak
    // (nullopt when that is past the window): the measured counterparts of Pre-delay and Decay.
    [[nodiscard]] std::optional<double> onsetMs() const noexcept { return onset; }
    [[nodiscard]] std::optional<double> fall60Ms() const noexcept { return fall60; }

    [[nodiscard]] juce::Rectangle<int> plotBounds() const
    {
        using L = UiTheme::Layout;
        return getLocalBounds().withTrimmedLeft (L::eqResponseLabelWidth)
                               .withTrimmedRight (L::keymapEditorGap)
                               .withTrimmedTop (L::eqResponseLabelHeight)
                               .withTrimmedBottom (L::eqResponseLabelHeight);
    }

    void paint (juce::Graphics& g) override
    {
        using L = UiTheme::Layout;
        g.fillAll (UiTheme::Color::controlInset());
        const auto plot = plotBounds();
        if (plot.isEmpty())
            return;
        g.setFont (UiTheme::Type::font (UiTheme::Type::small));
        g.setColour (UiTheme::Color::mutedText());
        g.drawText (enabled ? "Reverb tail (dB, wet)" : "Reverb tail (dB, wet, bypassed)",
                    getLocalBounds().withHeight (L::eqResponseLabelHeight), juce::Justification::centred);
        for (const double db : { 0.0, -30.0, -60.0 })
        {
            const int y = yForDb (db);
            g.setColour (UiTheme::Color::separator());
            g.drawHorizontalLine (y, static_cast<float> (plot.getX()), static_cast<float> (plot.getRight()));
            g.setColour (UiTheme::Color::mutedText());
            g.drawText (juce::String (static_cast<int> (db)),
                        juce::Rectangle<int> (0, y - L::eqResponseLabelHeight / 2, L::eqResponseLabelWidth - L::keymapEditorGap,
                                              L::eqResponseLabelHeight),
                        juce::Justification::centredRight);
        }
        for (const double fraction : { 0.0, 0.5, 1.0 })
        {
            const double ms = window * fraction;
            const int x = xForMs (ms);
            g.setColour (UiTheme::Color::mutedText());
            g.drawText (juce::String (ms / 1000.0, 1) + (fraction == 1.0 ? " s" : ""),
                        juce::Rectangle<int> (x - L::eqResponseLabelWidth / 2, plot.getBottom(), L::eqResponseLabelWidth, L::eqResponseLabelHeight),
                        juce::Justification::centred);
        }
        juce::Path tail;
        for (std::size_t bin = 0; bin < envelope.size(); ++bin)
        {
            const juce::Point<float> point (static_cast<float> (xForMs ((static_cast<double> (bin) + 0.5) * kBinMs)),
                                            static_cast<float> (yForDb (envelope[bin])));
            if (bin == 0) tail.startNewSubPath (point);
            else tail.lineTo (point);
        }
        g.setColour (enabled ? UiTheme::Color::accentPurple() : UiTheme::Color::mutedText());
        g.strokePath (tail, juce::PathStrokeType (L::eqResponseStrokeWidth));
    }

private:
    static constexpr double kBinMs = 10.0;
    static constexpr double kFloorDb = -90.0;
    // The window's cap bounds the work an edit pays for (B1: action to paint <= 16 ms).
    static constexpr double kMaxWindowMs = 2000.0;
    static constexpr int kBlockFrames = 512;

    [[nodiscard]] int xForMs (double ms) const
    {
        const auto plot = plotBounds();
        const double fraction = window > 0.0 ? std::clamp (ms / window, 0.0, 1.0) : 0.0;
        return plot.getX() + static_cast<int> (std::lround (fraction * static_cast<double> (plot.getWidth() - 1)));
    }

    [[nodiscard]] int yForDb (double db) const
    {
        const auto plot = plotBounds();
        const double fraction = std::clamp (-db / 60.0, 0.0, 1.0);
        return plot.getY() + static_cast<int> (std::lround (fraction * static_cast<double> (plot.getHeight() - 1)));
    }

    [[nodiscard]] double realParam (engine::ParameterId id) const
    {
        const engine::ParamSpec spec = engine::ReverbNode::parameterSpec (id);
        double normalized = engine::normalizedDefault (spec);
        for (const auto& [paramId, value] : params)
            if (paramId == id)
                normalized = value;
        return engine::ReverbNode::mapNormalizedParameter (id, normalized);
    }

    void simulate()
    {
        envelope.clear();
        onset.reset();
        fall60.reset();
        window = std::clamp (configuredPreDelayMs() + 1200.0 * configuredDecaySeconds(), 200.0, kMaxWindowMs);

        engine::ReverbNode node;
        node.prepare (rateHz, kBlockFrames);
        for (const auto& [id, value] : params)
            node.setNormalizedParameter (id, value);
        node.setNormalizedParameter (engine::ReverbNode::kMixParamId, 1.0);   // the tail alone

        const auto frames = static_cast<std::size_t> (std::ceil (window * rateHz / 1000.0));
        lastFrames = frames;
        std::vector<float> left (frames, 0.0f), right (frames, 0.0f);
        left[0] = right[0] = 1.0f;
        engine::EventStream events;
        engine::Transport transport;
        transport.hasTimelineFrame = true;
        engine::Node& iface = node;
        for (std::size_t offset = 0; offset < frames; offset += kBlockFrames)
        {
            const int count = static_cast<int> (std::min<std::size_t> (kBlockFrames, frames - offset));
            transport.timelineFrame = static_cast<std::int64_t> (offset);
            float* channels[2] = { left.data() + offset, right.data() + offset };
            iface.process (engine::ProcessArgs { engine::AudioBlock { channels, 2 }, events, transport, count });
        }

        const auto binFrames = std::max<std::size_t> (1, static_cast<std::size_t> (std::lround (kBinMs * rateHz / 1000.0)));
        std::vector<double> energy;
        for (std::size_t start = 0; start < frames; start += binFrames)
        {
            const std::size_t end = std::min (frames, start + binFrames);
            double sum = 0.0;
            for (std::size_t i = start; i < end; ++i)
                sum += static_cast<double> (left[i]) * left[i] + static_cast<double> (right[i]) * right[i];
            energy.push_back (sum / static_cast<double> (2 * (end - start)));
        }
        const double peak = energy.empty() ? 0.0 : *std::max_element (energy.begin(), energy.end());
        std::size_t peakBin = 0;
        for (std::size_t bin = 0; bin < energy.size(); ++bin)
        {
            const double db = peak > 0.0 && energy[bin] > 0.0 ? 10.0 * std::log10 (energy[bin] / peak) : kFloorDb;
            envelope.push_back (static_cast<float> (std::max (kFloorDb, db)));
            if (energy[bin] == peak)
                peakBin = bin;
        }
        for (std::size_t bin = 0; bin < envelope.size() && ! onset; ++bin)
            if (envelope[bin] > -60.0f)
                onset = static_cast<double> (bin) * kBinMs;
        for (std::size_t bin = peakBin; bin < envelope.size() && ! fall60; ++bin)
            if (envelope[bin] <= -60.0f)
                fall60 = static_cast<double> (bin - peakBin) * kBinMs;
    }

    std::vector<std::pair<std::uint32_t, double>> params;
    std::vector<float> envelope;
    std::optional<double> onset, fall60;
    double rateHz = 48000.0;
    double window = 1000.0;
    std::size_t lastFrames = 0;
    bool configured = false;
    bool enabled = true;
};

} // namespace yesdaw::ui
