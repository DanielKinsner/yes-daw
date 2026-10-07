// G4.2 cp3: the delay's face — the echoes the configured delay produces, found by running a private
// copy of the real FxDelayNode on a centred click (the EQ face's law: drawn from the DSP itself, never
// a parallel formula, so feedback, damping and ping-pong routing show exactly as they sound).
#pragma once

#include "engine/Project.h"
#include "engine/nodes/FxDelayNode.h"
#include "ui/UiTheme.h"
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace yesdaw::ui {

class DelayTapsComponent final : public juce::Component,
                                 public juce::SettableTooltipClient
{
public:
    struct Tap
    {
        double ms = 0.0;
        float levelDb = 0.0f;
        int channel = 0;   // 0 = left, 1 = right
    };

    DelayTapsComponent()
    {
        setComponentID ("mixer.fx.editor.delay.taps");
        setName ("Delay echoes");
        setTooltip ("The echoes this delay produces from a click (left above, right below); edit Time, Feedback, Damping, Ping-pong and Mix below");
        setWantsKeyboardFocus (false);
    }

    void setInsert (const engine::FxInsert& insert, double sampleRate)
    {
        const double rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        if (configured && rate == rateHz && enabled == insert.enabled && params == insert.normalizedParams)
            return;
        const bool echoesChanged = ! configured || rate != rateHz || params != insert.normalizedParams;
        rateHz = rate;
        params = insert.normalizedParams;
        enabled = insert.enabled;
        configured = true;
        if (echoesChanged)
            simulate();
        repaint();
    }

    [[nodiscard]] const std::vector<Tap>& taps() const noexcept { return echoes; }
    [[nodiscard]] double windowMs() const noexcept { return window; }
    [[nodiscard]] float dryLevelDb() const noexcept { return dryDb; }
    [[nodiscard]] bool isBypassedFace() const noexcept { return ! enabled; }
    [[nodiscard]] std::size_t simulatedFrames() const noexcept { return lastFrames; }
    [[nodiscard]] static constexpr double maxWindowMs() noexcept { return kMaxWindowMs; }

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
        g.drawText (enabled ? "Delay echoes (dB)" : "Delay echoes (dB, bypassed)",
                    getLocalBounds().withHeight (L::eqResponseLabelHeight), juce::Justification::centred);

        // Two lanes, left above right, each 0 dB at its outer edge and -60 dB at the centre line.
        const int mid = plot.getCentreY();
        g.setColour (UiTheme::Color::separator());
        g.drawHorizontalLine (mid, static_cast<float> (plot.getX()), static_cast<float> (plot.getRight()));
        g.setColour (UiTheme::Color::mutedText());
        g.drawText ("L", juce::Rectangle<int> (0, plot.getY(), L::eqResponseLabelWidth - L::keymapEditorGap, L::eqResponseLabelHeight),
                    juce::Justification::centredRight);
        g.drawText ("R", juce::Rectangle<int> (0, plot.getBottom() - L::eqResponseLabelHeight, L::eqResponseLabelWidth - L::keymapEditorGap,
                                               L::eqResponseLabelHeight),
                    juce::Justification::centredRight);
        for (const double fraction : { 0.0, 0.25, 0.5, 0.75, 1.0 })
        {
            const double ms = window * fraction;
            const int x = xForMs (ms);
            g.setColour (UiTheme::Color::separator());
            g.drawVerticalLine (x, static_cast<float> (plot.getY()), static_cast<float> (plot.getBottom()));
            g.setColour (UiTheme::Color::mutedText());
            g.drawText (juce::String (static_cast<int> (std::lround (ms))) + (fraction == 1.0 ? " ms" : ""),
                        juce::Rectangle<int> (x - L::eqResponseLabelWidth / 2, plot.getBottom(), L::eqResponseLabelWidth, L::eqResponseLabelHeight),
                        juce::Justification::centred);
        }

        const juce::Colour wet = enabled ? UiTheme::Color::accentPurple() : UiTheme::Color::mutedText();
        g.setColour (enabled ? UiTheme::Color::text() : UiTheme::Color::mutedText());   // ADR-0063: opaque
        drawStem (g, 0.0, dryDb, 0);
        drawStem (g, 0.0, dryDb, 1);
        g.setColour (wet);
        for (const Tap& tap : echoes)
            drawStem (g, tap.ms, tap.levelDb, tap.channel);
    }

private:
    static constexpr double kFloorDb = -60.0;
    // The window's cap bounds the work an edit pays for (B1: action to paint <= 16 ms): two seconds of
    // the node is ~3 ms here and holds eight echoes of the default 250 ms.
    static constexpr double kMaxWindowMs = 2000.0;
    static constexpr int kBlockFrames = 512;

    [[nodiscard]] int xForMs (double ms) const
    {
        const auto plot = plotBounds();
        const double fraction = window > 0.0 ? std::clamp (ms / window, 0.0, 1.0) : 0.0;
        return plot.getX() + static_cast<int> (std::lround (fraction * static_cast<double> (plot.getWidth() - 1)));
    }

    void drawStem (juce::Graphics& g, double ms, float levelDb, int channel) const
    {
        const auto plot = plotBounds();
        const int mid = plot.getCentreY();
        const double height = std::clamp ((static_cast<double> (levelDb) - kFloorDb) / -kFloorDb, 0.0, 1.0)
                            * static_cast<double> (plot.getHeight() / 2);
        const float x = static_cast<float> (xForMs (ms));
        const float tip = static_cast<float> (channel == 0 ? mid - height : mid + height);
        g.drawLine (x, static_cast<float> (mid), x, tip, UiTheme::Layout::eqResponseStrokeWidth);
    }

    [[nodiscard]] double realParam (engine::ParameterId id) const
    {
        const engine::ParamSpec spec = engine::FxDelayNode::parameterSpec (id);
        double normalized = engine::normalizedDefault (spec);
        for (const auto& [paramId, value] : params)
            if (paramId == id)
                normalized = value;
        return engine::FxDelayNode::mapNormalizedParameter (id, normalized);
    }

    // The node's own output for a centred click, its echoes picked as the loudest sample within each
    // neighbourhood of half the shorter delay (a damped echo is a smeared bump, one tap all the same).
    void simulate()
    {
        echoes.clear();
        const double timeL = realParam (engine::FxDelayNode::kTimeLeftParamId);
        const double timeR = realParam (engine::FxDelayNode::kTimeRightParamId);
        const double feedback = realParam (engine::FxDelayNode::kFeedbackParamId);
        const double longest = std::max (timeL, timeR);
        const double echoesToShow = feedback > 0.0 ? std::ceil (std::log (std::pow (10.0, kFloorDb / 20.0)) / std::log (feedback)) + 1.0 : 1.0;
        window = std::clamp (longest * std::max (1.0, echoesToShow) * 1.1, longest * 1.1, kMaxWindowMs);

        engine::FxDelayNode node;
        node.prepare (rateHz, kBlockFrames);
        for (const auto& [id, value] : params)
            node.setNormalizedParameter (id, value);

        const auto frames = static_cast<std::size_t> (std::ceil (window * rateHz / 1000.0)) + 1u;
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

        dryDb = levelDb (left[0]);
        const auto neighbourhood = static_cast<std::size_t> (std::max (1.0, 0.5 * std::min (timeL, timeR) * rateHz / 1000.0));
        const float floorLevel = static_cast<float> (std::pow (10.0, kFloorDb / 20.0));
        int channel = 0;
        for (const std::vector<float>* signal : { &left, &right })
        {
            // Local maxima above the floor, loudest first; each one claims its neighbourhood.
            const std::vector<float>& s = *signal;
            std::vector<std::size_t> candidates;
            for (std::size_t i = 1; i + 1 < s.size(); ++i)
            {
                const float level = std::fabs (s[i]);
                if (level >= floorLevel && level > std::fabs (s[i - 1]) && level >= std::fabs (s[i + 1]))
                    candidates.push_back (i);
            }
            if (s.size() > 1 && std::fabs (s.back()) >= floorLevel && std::fabs (s.back()) > std::fabs (s[s.size() - 2]))
                candidates.push_back (s.size() - 1);
            std::stable_sort (candidates.begin(), candidates.end(), [&s] (std::size_t a, std::size_t b) {
                return std::fabs (s[a]) > std::fabs (s[b]);
            });
            std::vector<std::size_t> accepted;
            for (const std::size_t i : candidates)
            {
                bool claimed = false;
                for (const std::size_t j : accepted)
                    claimed = claimed || (i > j ? i - j : j - i) <= neighbourhood;
                if (! claimed)
                    accepted.push_back (i);
            }
            std::sort (accepted.begin(), accepted.end());
            for (const std::size_t i : accepted)
                echoes.push_back ({ static_cast<double> (i) * 1000.0 / rateHz, levelDb (s[i]), channel });
            ++channel;
        }
    }

    [[nodiscard]] static float levelDb (float sample) noexcept
    {
        const double magnitude = std::fabs (static_cast<double> (sample));
        return magnitude > 0.0 ? static_cast<float> (std::max (kFloorDb, 20.0 * std::log10 (magnitude))) : static_cast<float> (kFloorDb);
    }

    std::vector<std::pair<std::uint32_t, double>> params;
    std::vector<Tap> echoes;
    double rateHz = 48000.0;
    double window = 1000.0;
    std::size_t lastFrames = 0;
    float dryDb = 0.0f;
    bool configured = false;
    bool enabled = true;
};

} // namespace yesdaw::ui
