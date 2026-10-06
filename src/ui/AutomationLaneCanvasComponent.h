// YES DAW — the automation lane canvas.
//
// Plan §5.1 (the shell topology), checkpoint 1 (2026-09-05): carved verbatim out of MainComponent.cpp,
// where it sat above the shell class. The shell owns the instance and wires the std::function hooks;
// this file owns the class. Behaviour unchanged; the gate is [shell-topology] in the theme audit.

#pragma once

#include "engine/Time.h"
#include "ui/ContextMenus.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiAppModel.h"
#include "ui/UiMixerSurface.h"
#include "ui/UiPianoRollSurface.h"
#include "ui/UiTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace yesdaw::ui {

// The automation lane canvas (usable-DAW P1): paints the target lane's breakpoints against the SAME
// timeline viewport math as the arrangement, and turns clicks into real breakpoint edits. Click empty
// lane = add at (time, value); drag a handle = move; double-click a handle = delete.
class AutomationLaneCanvasComponent final : public juce::Component,
                                            public juce::SettableTooltipClient
{
public:
    // R16: each point carries its own curve shape so the painted line shows the REAL law the
    // engine evaluates (a segment's shape belongs to its LEFT point, the evaluator's rule).
    struct CanvasPoint
    {
        double seconds = 0.0;
        double value = 0.0;
        yesdaw::engine::AutomationCurveType curve = yesdaw::engine::AutomationCurveType::Linear;
    };

    std::function<std::vector<CanvasPoint>()> pointsProvider;
    std::function<double (int)> secondsForLocalX;
    std::function<int (double)> localXForSeconds;
    std::function<void (double, double)> onAddPoint;
    std::function<void (double, double, double)> onMovePoint;   // oldSeconds, newSeconds, newValue
    std::function<void (double)> onDeletePoint;
    // R16: Alt+click a handle cycles its curve Linear→Hold→Bezier→Log.
    std::function<void (double)> onCycleCurvePoint;
    // G4.6 / ADR-0052: a stacked lane names its target (top-left); the chooser lane's header names its own.
    juce::String laneLabel;

    // G4.6 / ADR-0052: the arrangement's tool decides the gesture. Pointer: click adds, drag moves, double-click
    // deletes. Pencil: a drag draws the swept span (Shift: a straight line from press to release). Eraser: a
    // drag deletes the points it sweeps (a click on a point deletes it). Each stroke is one edit.
    enum class Tool : std::uint8_t { Pointer, Pencil, Eraser };
    struct StrokePoint
    {
        double seconds = 0.0;
        double value = 0.0;
    };
    std::function<Tool()> toolProvider;
    std::function<void (const std::vector<StrokePoint>&, bool)> onPencilStroke;   // points in drawing order, line
    std::function<void (double, double)> onEraseSpan;                              // from, to seconds

    void paint (juce::Graphics& g) override
    {
        g.fillAll (yesdaw::ui::UiTheme::Color::controlInset());
        if (laneLabel.isNotEmpty())
        {
            // A stacked lane's top edge parts it from the clip row or the lane above.
            g.setColour (yesdaw::ui::UiTheme::Color::panelStroke());
            g.fillRect (getLocalBounds().withHeight (yesdaw::ui::UiTheme::Layout::automationCanvasSeparatorHeight));
            g.setColour (yesdaw::ui::UiTheme::Color::mutedText());
            g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
            g.drawText (laneLabel,
                        getLocalBounds().reduced (yesdaw::ui::UiTheme::Layout::automationCanvasLabelInset)
                            .withHeight (yesdaw::ui::UiTheme::Layout::automationCanvasLabelHeight),
                        juce::Justification::centredLeft, true);
        }
        if (! pointsProvider || ! localXForSeconds)
            return;

        const std::vector<CanvasPoint> points = pointsProvider();
        g.setColour (yesdaw::ui::UiTheme::Color::accentPurple());
        juce::Path line;
        for (std::size_t i = 0; i + 1u < points.size(); ++i)
        {
            const juce::Point<float> from { static_cast<float> (localXForSeconds (points[i].seconds)),
                                            yForValue (points[i].value) };
            const juce::Point<float> to { static_cast<float> (localXForSeconds (points[i + 1u].seconds)),
                                          yForValue (points[i + 1u].value) };
            line.startNewSubPath (from);
            if (points[i].curve == yesdaw::engine::AutomationCurveType::Hold)
            {
                // A hold paints as the step it is: flat to the next tick, then vertical.
                line.lineTo (to.x, from.y);
                line.lineTo (to);
            }
            else
            {
                // Linear/Bezier/Log sample the engine's own curve law so the picture can never
                // drift from what actually renders.
                constexpr int kSteps = 16;
                for (int step = 1; step <= kSteps; ++step)
                {
                    const double t = static_cast<double> (step) / kSteps;
                    const double u = yesdaw::engine::automationCurveProgress (points[i].curve, t);
                    line.lineTo (from.x + (to.x - from.x) * static_cast<float> (t),
                                 from.y + (to.y - from.y) * static_cast<float> (u));
                }
            }
        }
        g.strokePath (line, juce::PathStrokeType (yesdaw::ui::UiTheme::Layout::automationCanvasLineWidth));

        // G4.6: the stroke being drawn (Pencil) or the span being swept (Eraser).
        if (stroke.size() > 1u && strokeTool == Tool::Pencil)
        {
            juce::Path preview;
            preview.startNewSubPath (stroke.front().toFloat());
            if (strokeLine)
                preview.lineTo (stroke.back().toFloat());
            else
                for (std::size_t i = 1; i < stroke.size(); ++i)
                    preview.lineTo (stroke[i].toFloat());
            g.setColour (yesdaw::ui::UiTheme::Color::accentPurpleGlow());
            g.strokePath (preview, juce::PathStrokeType (yesdaw::ui::UiTheme::Layout::automationCanvasLineWidth));
        }
        else if (stroke.size() > 1u && strokeTool == Tool::Eraser)
        {
            int from = stroke.front().x;
            int to = stroke.front().x;
            for (const juce::Point<int>& at : stroke)
            {
                from = std::min (from, at.x);
                to = std::max (to, at.x);
            }
            g.setColour (yesdaw::ui::UiTheme::Color::dangerRed().withAlpha (yesdaw::ui::UiTheme::Layout::automationCanvasEraseAlpha));
            g.fillRect (juce::Rectangle<int> (from, 0, to - from, getHeight()));
        }
        g.setColour (yesdaw::ui::UiTheme::Color::accentPurple());

        for (const CanvasPoint& point : points)
        {
            const float radius = static_cast<float> (yesdaw::ui::UiTheme::Layout::automationCanvasHandleRadius);
            g.fillEllipse (static_cast<float> (localXForSeconds (point.seconds)) - radius,
                           yForValue (point.value) - radius,
                           radius + radius,
                           radius + radius);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragOldSeconds.reset();
        stroke.clear();
        strokeTool = toolProvider ? toolProvider() : Tool::Pointer;
        if (strokeTool == Tool::Pencil || strokeTool == Tool::Eraser)
        {
            stroke.push_back (event.getPosition());
            strokeLine = event.mods.isShiftDown();
            return;
        }

        if (const std::optional<double> hit = handleSecondsAt (event.getPosition()))
        {
            // R16: Alt+click cycles the hit point's curve shape instead of starting a drag.
            if (event.mods.isAltDown())
            {
                if (onCycleCurvePoint)
                    onCycleCurvePoint (*hit);
                return;
            }

            dragOldSeconds = hit;
            return;
        }

        if (event.mods.isAltDown())
            return;   // Alt is the curve gesture — never an accidental add

        if (onAddPoint && secondsForLocalX)
            onAddPoint (secondsForLocalX (event.getPosition().x), valueForY (event.getPosition().y));
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (stroke.empty())
            return;
        if (event.getPosition().x != stroke.back().x)
            stroke.push_back (event.getPosition());
        else
            stroke.back() = event.getPosition();
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! stroke.empty())
        {
            finishStroke (event);
            return;
        }

        if (! dragOldSeconds)
            return;

        const double oldSeconds = *dragOldSeconds;
        dragOldSeconds.reset();
        if (! event.mouseWasDraggedSinceMouseDown() || ! onMovePoint || ! secondsForLocalX)
            return;

        onMovePoint (oldSeconds,
                     secondsForLocalX (event.getPosition().x),
                     valueForY (event.getPosition().y));
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (const std::optional<double> hit = handleSecondsAt (event.getPosition()))
            if (onDeletePoint)
                onDeletePoint (*hit);
    }

private:
    void finishStroke (const juce::MouseEvent& event)
    {
        std::vector<juce::Point<int>> swept;
        swept.swap (stroke);
        if (event.getPosition().x != swept.back().x)
            swept.push_back (event.getPosition());
        else
            swept.back() = event.getPosition();
        const bool line = strokeLine || event.mods.isShiftDown();
        repaint();
        if (! secondsForLocalX)
            return;

        if (strokeTool == Tool::Eraser)
        {
            if (swept.size() == 1u || ! event.mouseWasDraggedSinceMouseDown())
            {
                if (const std::optional<double> hit = handleSecondsAt (swept.front()); hit && onDeletePoint)
                    onDeletePoint (*hit);   // a click on a point deletes it
                return;
            }
            // Everything the drag swept — a stroke that doubles back past its start erases that too.
            int from = swept.front().x;
            int to = swept.front().x;
            for (const juce::Point<int>& at : swept)
            {
                from = std::min (from, at.x);
                to = std::max (to, at.x);
            }
            if (onEraseSpan)
                onEraseSpan (secondsForLocalX (from), secondsForLocalX (to));
            return;
        }

        if (! onPencilStroke)
            return;
        std::vector<StrokePoint> points;
        if (line && swept.size() > 1u)
        {
            points.push_back ({ secondsForLocalX (swept.front().x), valueForY (swept.front().y) });
            points.push_back ({ secondsForLocalX (swept.back().x), valueForY (swept.back().y) });
        }
        else
        {
            for (const juce::Point<int>& at : swept)
                points.push_back ({ secondsForLocalX (at.x), valueForY (at.y) });
        }
        onPencilStroke (points, line);
    }

    [[nodiscard]] float yForValue (double value) const
    {
        const float height = static_cast<float> (juce::jmax (1, getHeight()));
        return height * static_cast<float> (1.0 - std::clamp (value, 0.0, 1.0));
    }

    [[nodiscard]] double valueForY (int y) const
    {
        const double height = static_cast<double> (juce::jmax (1, getHeight()));
        return std::clamp (1.0 - static_cast<double> (y) / height, 0.0, 1.0);
    }

    [[nodiscard]] std::optional<double> handleSecondsAt (juce::Point<int> position) const
    {
        if (! pointsProvider || ! localXForSeconds)
            return std::nullopt;

        const int hitRadius = yesdaw::ui::UiTheme::Layout::automationCanvasHandleHitRadius;
        for (const CanvasPoint& point : pointsProvider())
        {
            const juce::Point<int> at { localXForSeconds (point.seconds),
                                        static_cast<int> (yForValue (point.value)) };
            if (position.getDistanceFrom (at) <= hitRadius)
                return point.seconds;
        }

        return std::nullopt;
    }

    std::optional<double> dragOldSeconds;
    std::vector<juce::Point<int>> stroke;   // G4.6: a Pencil or Eraser stroke in progress
    Tool strokeTool = Tool::Pointer;
    bool strokeLine = false;
};

} // namespace yesdaw::ui
