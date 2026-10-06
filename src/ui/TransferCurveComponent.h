// G4.2 cp5: the dynamics faces' transfer curve — output level against input level, from the DSP itself.
// The compressor's static law is the node's own closed form (the function its detector path calls); the
// limiter's is measured by running a private copy of the real node on steady levels until it settles.
#pragma once

#include "engine/Project.h"
#include "engine/nodes/CompressorNode.h"
#include "engine/nodes/LimiterNode.h"
#include "ui/UiTheme.h"
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace yesdaw::ui {

class TransferCurveComponent final : public juce::Component,
                                     public juce::SettableTooltipClient
{
public:
    struct Point
    {
        double inputDb = 0.0;
        double outputDb = 0.0;
    };

    TransferCurveComponent()
    {
        setComponentID ("mixer.fx.editor.transfer");
        setName ("Transfer curve");
        setTooltip ("Output level for each steady input level (the dashed line is unity); edit the parameters below");
        setWantsKeyboardFocus (false);
    }

    void setInsert (const engine::FxInsert& insert, double sampleRate)
    {
        const double rate = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        if (configured && kind == insert.kind && rate == rateHz && enabled == insert.enabled && params == insert.normalizedParams)
            return;
        const bool curveChanged = ! configured || kind != insert.kind || rate != rateHz || params != insert.normalizedParams;
        kind = insert.kind;
        rateHz = rate;
        params = insert.normalizedParams;
        enabled = insert.enabled;
        configured = true;
        if (curveChanged)
            rebuild();
        repaint();
    }

    [[nodiscard]] const std::vector<Point>& points() const noexcept { return curve; }
    [[nodiscard]] static constexpr double minDb() noexcept { return kMinDb; }
    [[nodiscard]] static constexpr double maxDb() noexcept { return kMaxDb; }
    [[nodiscard]] bool isBypassedFace() const noexcept { return ! enabled; }

    // The curve's output at an input level (linear between the computed points).
    [[nodiscard]] double outputDbAt (double inputDb) const noexcept
    {
        if (curve.empty())
            return inputDb;
        if (inputDb <= curve.front().inputDb)
            return curve.front().outputDb;
        for (std::size_t i = 1; i < curve.size(); ++i)
            if (inputDb <= curve[i].inputDb)
            {
                const double t = (inputDb - curve[i - 1].inputDb) / (curve[i].inputDb - curve[i - 1].inputDb);
                return curve[i - 1].outputDb + t * (curve[i].outputDb - curve[i - 1].outputDb);
            }
        return curve.back().outputDb;
    }

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
        g.drawText (enabled ? "Output vs input (dB)" : "Output vs input (dB, bypassed)",
                    getLocalBounds().withHeight (L::eqResponseLabelHeight), juce::Justification::centred);
        for (const double db : { kMinDb, -24.0, 0.0, kMaxDb })
        {
            const int x = xForDb (db), y = yForDb (db);
            g.setColour (db == 0.0 ? UiTheme::Color::mutedText() : UiTheme::Color::separator());
            g.drawVerticalLine (x, static_cast<float> (plot.getY()), static_cast<float> (plot.getBottom()));
            g.drawHorizontalLine (y, static_cast<float> (plot.getX()), static_cast<float> (plot.getRight()));
            g.setColour (UiTheme::Color::mutedText());
            g.drawText (juce::String (static_cast<int> (db)),
                        juce::Rectangle<int> (0, y - L::eqResponseLabelHeight / 2, L::eqResponseLabelWidth - L::keymapEditorGap,
                                              L::eqResponseLabelHeight),
                        juce::Justification::centredRight);
            g.drawText (juce::String (static_cast<int> (db)),
                        juce::Rectangle<int> (x - L::eqResponseLabelWidth / 2, plot.getBottom(), L::eqResponseLabelWidth, L::eqResponseLabelHeight),
                        juce::Justification::centred);
        }
        juce::Path unity;
        unity.startNewSubPath (static_cast<float> (xForDb (kMinDb)), static_cast<float> (yForDb (kMinDb)));
        unity.lineTo (static_cast<float> (xForDb (kMaxDb)), static_cast<float> (yForDb (kMaxDb)));
        juce::Path dashed;
        const float dashes[] = { L::grMeterPeakStrokeWidth * 2.0f, L::grMeterPeakStrokeWidth * 2.0f };
        juce::PathStrokeType (L::controlOutlineStrokeWidth).createDashedStroke (dashed, unity, dashes, 2);
        g.setColour (UiTheme::Color::separator());
        g.fillPath (dashed);

        juce::Path line;
        for (std::size_t i = 0; i < curve.size(); ++i)
        {
            const juce::Point<float> p (static_cast<float> (xForDb (curve[i].inputDb)), static_cast<float> (yForDb (curve[i].outputDb)));
            if (i == 0) line.startNewSubPath (p);
            else line.lineTo (p);
        }
        g.setColour (enabled ? UiTheme::Color::accentAmber() : UiTheme::Color::mutedText());
        g.strokePath (line, juce::PathStrokeType (L::eqResponseStrokeWidth));
    }

private:
    static constexpr double kMinDb = -48.0;
    static constexpr double kMaxDb = 12.0;
    static constexpr double kStepDb = 1.0;
    static constexpr double kLimiterStepDb = 3.0;
    static constexpr double kLimiterSettleMs = 60.0;
    static constexpr int kBlockFrames = 512;

    [[nodiscard]] int xForDb (double db) const
    {
        const auto plot = plotBounds();
        const double fraction = std::clamp ((db - kMinDb) / (kMaxDb - kMinDb), 0.0, 1.0);
        return plot.getX() + static_cast<int> (std::lround (fraction * static_cast<double> (plot.getWidth() - 1)));
    }

    [[nodiscard]] int yForDb (double db) const
    {
        const auto plot = plotBounds();
        const double fraction = std::clamp ((kMaxDb - db) / (kMaxDb - kMinDb), 0.0, 1.0);
        return plot.getY() + static_cast<int> (std::lround (fraction * static_cast<double> (plot.getHeight() - 1)));
    }

    template <typename NodeType>
    [[nodiscard]] double realParam (engine::ParameterId id) const
    {
        const engine::ParamSpec spec = NodeType::parameterSpec (id);
        double normalized = engine::normalizedDefault (spec);
        for (const auto& [paramId, value] : params)
            if (paramId == id)
                normalized = value;
        return NodeType::mapNormalizedParameter (id, normalized);
    }

    void rebuild()
    {
        curve.clear();
        if (kind == engine::FxKind::Compressor)
        {
            using C = engine::CompressorNode;
            const double threshold = realParam<C> (C::kThresholdParamId);
            const double ratio = realParam<C> (C::kRatioParamId);
            const double knee = realParam<C> (C::kKneeParamId);
            const double makeup = realParam<C> (C::kMakeupParamId);
            const int steps = static_cast<int> (std::lround ((kMaxDb - kMinDb) / kStepDb));
            for (int step = 0; step <= steps; ++step)
            {
                const double in = kMinDb + kStepDb * step;
                curve.push_back ({ in, in - C::closedFormGainReductionDb (in, threshold, ratio, knee) + makeup });
            }
        }
        else if (kind == engine::FxKind::Limiter)
        {
            // A steady level held until the lookahead and gain smoothing settle; the last sample is the answer.
            const auto frames = static_cast<std::size_t> (std::ceil (kLimiterSettleMs * rateHz / 1000.0));
            std::vector<float> left (frames), right (frames);
            const int steps = static_cast<int> (std::lround ((kMaxDb - kMinDb) / kLimiterStepDb));
            for (int step = 0; step <= steps; ++step)
            {
                const double in = kMinDb + kLimiterStepDb * step;
                engine::LimiterNode node;
                node.prepare (rateHz, kBlockFrames);
                for (const auto& [id, value] : params)
                    node.setNormalizedParameter (id, value);
                const auto level = static_cast<float> (std::pow (10.0, in / 20.0));
                std::fill (left.begin(), left.end(), level);
                std::fill (right.begin(), right.end(), level);
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
                const double out = std::fabs (static_cast<double> (left.back()));
                curve.push_back ({ in, out > 0.0 ? 20.0 * std::log10 (out) : kMinDb });
            }
        }
    }

    std::vector<std::pair<std::uint32_t, double>> params;
    std::vector<Point> curve;
    engine::FxKind kind = engine::FxKind::Compressor;
    double rateHz = 48000.0;
    bool configured = false;
    bool enabled = true;
};

} // namespace yesdaw::ui
