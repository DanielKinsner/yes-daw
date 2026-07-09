// YES DAW - small native vector icon set for shipped chrome.

#pragma once

#include "ui/UiActions.h"

#include <juce_graphics/juce_graphics.h>

namespace yesdaw::ui {

inline bool drawCompactActionIcon (juce::Graphics& g,
                                   UiActionId action,
                                   juce::Rectangle<float> bounds,
                                   juce::Colour colour)
{
    if (action != UiActionId::EditUndo && action != UiActionId::EditRedo)
        return false;

    const bool redo = action == UiActionId::EditRedo;
    const float stroke = juce::jmax (1.25f, bounds.getWidth() * 0.075f);
    const float direction = redo ? 1.0f : -1.0f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY() + bounds.getHeight() * 0.06f;
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.27f;

    juce::Path arc;
    const float start = redo ? juce::MathConstants<float>::pi * 1.15f
                             : juce::MathConstants<float>::pi * 1.85f;
    const float end = redo ? juce::MathConstants<float>::pi * 2.85f
                           : juce::MathConstants<float>::pi * 0.15f;
    arc.addCentredArc (centreX, centreY, radius, radius, 0.0f, start, end, true);

    g.setColour (colour);
    g.strokePath (arc, juce::PathStrokeType (stroke,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    const float tipX = centreX + direction * radius * 0.92f;
    const float tipY = centreY - radius * 0.52f;
    juce::Path arrow;
    arrow.startNewSubPath (tipX, tipY);
    arrow.lineTo (tipX - direction * radius * 0.48f, tipY - radius * 0.10f);
    arrow.lineTo (tipX - direction * radius * 0.10f, tipY + radius * 0.44f);
    arrow.closeSubPath();
    g.fillPath (arrow);
    return true;
}

} // namespace yesdaw::ui
