// YES DAW — the shell's small widgets: the toolbar action button, the Shift-fine slider, the playhead layer.
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

class ToolbarActionButton final : public juce::TextButton
{
public:
    void setAction (yesdaw::ui::UiActionId value) noexcept { action = value; }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        // G2.1 cp3: a cluster-narrow button ([I][X][P][A]) paints its letter, never its icon —
        // but icon-only chrome (the header's New / Open / Save / transport) stays an icon at any width.
        if (! yesdaw::ui::hasActionIcon (action)
            || (getWidth() <= yesdaw::ui::UiTheme::Layout::inspectorToggleWidth
                && ! yesdaw::ui::actionUsesIconOnlyChrome (action)))
        {
            juce::TextButton::paintButton (g, highlighted, down);
            return;
        }

        getLookAndFeel().drawButtonBackground (
            g,
            *this,
            findColour (getToggleState() ? juce::TextButton::buttonOnColourId
                                         : juce::TextButton::buttonColourId),
            highlighted,
            down);

        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (yesdaw::ui::UiTheme::Tone::componentVisibleAlpha);
        const auto iconColour = findColour (getToggleState() ? juce::TextButton::textColourOnId
                                                              : juce::TextButton::textColourOffId)
                                    .withMultipliedAlpha (
                                        isEnabled() ? yesdaw::ui::UiTheme::Tone::componentVisibleAlpha
                                                    : yesdaw::ui::UiTheme::Tone::disabledAlpha);
        auto content = getLocalBounds();
        if (yesdaw::ui::actionUsesIconOnlyChrome (action))
        {
            (void) yesdaw::ui::drawActionIcon (
                g,
                action,
                content.toFloat().reduced (
                    static_cast<float> (yesdaw::ui::UiTheme::Layout::controlIconInset)),
                iconColour);
            return;
        }

        auto iconArea = content.removeFromLeft (content.getHeight())
                           .reduced (yesdaw::ui::UiTheme::Layout::controlIconInset);
        (void) yesdaw::ui::drawActionIcon (g, action, iconArea.toFloat(), iconColour);
        g.setColour (iconColour);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::body,
            juce::Font::bold));
        g.drawFittedText (getButtonText(),
                          content.reduced (yesdaw::ui::UiTheme::Layout::controlIconTextGap, 0),
                          juce::Justification::centredLeft,
                          1);
    }

private:
    yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::ProjectNew;
};

// Transparent overlay across the left Track rail. Shares drawTrackList's exact row math (header
// strip, then equal rows floored at trackListRowMinHeight) so hit-testing and paint cannot drift.
// Slider with an exact Shift fine-drag mode (B30): while Shift is held, pointer movement counts
// for exactly UiTheme::Layout::fineDragScale of its plain effect. Fine mode anchors at the value
// when it engages (no jump-to-pointer), accumulates unsnapped so tiny moves add up, latches until
// mouse-up, and never engages while Alt is down so the Alt+click reset law is untouched.
class FineDragSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;

    void mouseDown (const juce::MouseEvent& event) override
    {
        fineActive = false;
        lastFinePosition = event.position;
        if (isEnabled() && wantsFineDrag (event) && supportsFineDrag())
        {
            fineActive = true;
            fineStartedDrag = true;   // the base drag was swallowed, so fire the callbacks here
            fineValue = getValue();
            if (onDragStart)
                onDragStart();
            return;   // no jump-to-pointer; the anchor is the current value
        }

        juce::Slider::mouseDown (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! fineActive && wantsFineDrag (event) && supportsFineDrag())
        {
            fineActive = true;   // Shift pressed mid-drag: anchor at the value reached so far
            fineValue = getValue();
        }

        if (! fineActive)
        {
            lastFinePosition = event.position;
            juce::Slider::mouseDrag (event);
            return;
        }

        const double proportionDelta = axisProportionDelta (event.position);
        lastFinePosition = event.position;
        fineValue = juce::jlimit (getMinimum(),
                                  getMaximum(),
                                  fineValue
                                      + proportionDelta * (getMaximum() - getMinimum())
                                            * yesdaw::ui::UiTheme::Layout::fineDragScale);
        setValue (fineValue, juce::sendNotificationSync);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        fineActive = false;
        juce::Slider::mouseUp (event);
        if (fineStartedDrag)
        {
            fineStartedDrag = false;
            if (onDragEnd)
                onDragEnd();
        }
    }

private:
    [[nodiscard]] static bool wantsFineDrag (const juce::MouseEvent& event)
    {
        return event.mods.isShiftDown() && ! event.mods.isAltDown();
    }

    [[nodiscard]] bool supportsFineDrag() const
    {
        const auto style = getSliderStyle();
        return style == juce::Slider::LinearHorizontal
            || style == juce::Slider::LinearVertical
            || style == juce::Slider::RotaryHorizontalVerticalDrag;
    }

    // Pointer movement as a proportion of the control's own span, matching each style's plain
    // drag direction: horizontal tracks +x, vertical tracks -y, rotary tracks (+x, -y) combined.
    [[nodiscard]] double axisProportionDelta (juce::Point<float> position) const
    {
        const double dx = static_cast<double> (position.x - lastFinePosition.x);
        const double dy = static_cast<double> (position.y - lastFinePosition.y);
        switch (getSliderStyle())
        {
            case juce::Slider::LinearHorizontal:
                return dx / std::max (1, getWidth());
            case juce::Slider::LinearVertical:
                return -dy / std::max (1, getHeight());
            case juce::Slider::RotaryHorizontalVerticalDrag:
                return (dx - dy) / std::max (1, getWidth());
            default:
                return 0.0;
        }
    }

    bool fineActive = false;
    bool fineStartedDrag = false;
    double fineValue = 0.0;
    juce::Point<float> lastFinePosition;
};

class PlayheadLayerComponent final : public juce::Component
{
public:
    std::function<yesdaw::ui::TimelineCanvasState()> stateProvider;

    PlayheadLayerComponent()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
        setWantsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override
    {
        if (! stateProvider)
            return;
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        yesdaw::ui::timeline_canvas_detail::drawPlayhead (g, geometry.rulerArea, geometry.clipArea, state, geometry.viewport);
    }
};

} // namespace yesdaw::ui
