// YES DAW - scalable native vector asset set for shipped chrome.

#pragma once

#include "ui/UiActions.h"
#include "ui/UiTheme.h"

#include <juce_graphics/juce_graphics.h>

#include <array>
#include <cmath>
#include <cstddef>

namespace yesdaw::ui {

inline void strokeIconPath (juce::Graphics& g, const juce::Path& path, juce::Colour colour,
                            float stroke = UiTheme::Layout::iconStrokeWidth)
{
    g.setColour (colour);
    g.strokePath (path,
                  juce::PathStrokeType (stroke,
                                        juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded));
}

inline bool hasActionIcon (UiActionId action) noexcept
{
    switch (action)
    {
        case UiActionId::ProjectNew:
        case UiActionId::ProjectOpen:
        case UiActionId::ProjectSave:
        case UiActionId::ProjectImportAudio:
        case UiActionId::DeviceRefreshAudio:
        case UiActionId::DeviceSelectTestAudio:
        case UiActionId::RecordingArmTrack:
        case UiActionId::RecordingSetMonitoringPolicy:
        case UiActionId::TransportRecord:
        case UiActionId::RecordingAssembleComp:
        case UiActionId::EditUndo:
        case UiActionId::EditRedo:
        case UiActionId::TransportPlay:
        case UiActionId::TransportStop:
        case UiActionId::TransportLocateStart:
        case UiActionId::TransportToggleLoop:
        case UiActionId::ViewMixer:
        case UiActionId::ViewPianoRoll:
        case UiActionId::ViewToggleInspector:                 // the layout pass, step 7: the view cluster's icons
        case UiActionId::TimelineToggleMixerDock:
        case UiActionId::TimelineAutomationToggleTrackLane:
        case UiActionId::ViewMixWindow:                       // the layout pass, step 10: the header's Mix button
        case UiActionId::ProjectExportAudioCancel:   // UI correctness pass unit 10: a cross in the export's 28 px slot
            return true;
        default:
            return false;
    }
}

inline bool actionUsesIconOnlyChrome (UiActionId action) noexcept
{
    switch (action)
    {
        case UiActionId::ProjectNew:
        case UiActionId::ProjectOpen:
        case UiActionId::ProjectSave:
        case UiActionId::ProjectImportAudio:
        case UiActionId::TransportRecord:
        case UiActionId::EditUndo:
        case UiActionId::EditRedo:
        case UiActionId::TransportPlay:
        case UiActionId::TransportStop:
        case UiActionId::TransportLocateStart:
        case UiActionId::TransportToggleLoop:
        case UiActionId::ProjectExportAudioCancel:
            return true;
        default:
            return false;
    }
}

inline bool drawActionIcon (juce::Graphics& g,
                            UiActionId action,
                            juce::Rectangle<float> bounds,
                            juce::Colour colour)
{
    if (! hasActionIcon (action) || bounds.isEmpty())
        return false;

    const float left = bounds.getX();
    const float top = bounds.getY();
    const float right = bounds.getRight();
    const float bottom = bounds.getBottom();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    juce::Path path;

    switch (action)
    {
        case UiActionId::ProjectNew:
            path.addRoundedRectangle (left + w * 0.18f, top + h * 0.10f, w * 0.58f, h * 0.76f,
                                      UiTheme::Radius::xs);
            path.startNewSubPath (cx + w * 0.08f, top + h * 0.58f);
            path.lineTo (cx + w * 0.08f, bottom - h * 0.04f);
            path.startNewSubPath (cx - w * 0.11f, top + h * 0.77f);
            path.lineTo (cx + w * 0.27f, top + h * 0.77f);
            break;

        case UiActionId::ProjectOpen:
            path.startNewSubPath (left + w * 0.08f, top + h * 0.32f);
            path.lineTo (left + w * 0.36f, top + h * 0.32f);
            path.lineTo (left + w * 0.46f, top + h * 0.18f);
            path.lineTo (right - w * 0.08f, top + h * 0.18f);
            path.lineTo (right - w * 0.08f, bottom - h * 0.12f);
            path.lineTo (left + w * 0.08f, bottom - h * 0.12f);
            path.closeSubPath();
            path.startNewSubPath (left + w * 0.10f, top + h * 0.43f);
            path.lineTo (right - w * 0.03f, top + h * 0.43f);
            break;

        case UiActionId::ProjectSave:
            path.addRoundedRectangle (left + w * 0.12f, top + h * 0.10f, w * 0.76f, h * 0.80f,
                                      UiTheme::Radius::xs);
            path.addRectangle (left + w * 0.29f, top + h * 0.12f, w * 0.39f, h * 0.25f);
            path.addRoundedRectangle (left + w * 0.28f, top + h * 0.57f, w * 0.44f, h * 0.31f,
                                      UiTheme::Radius::xs);
            break;

        case UiActionId::ProjectImportAudio:
            path.startNewSubPath (left + w * 0.10f, bottom - h * 0.14f);
            path.lineTo (right - w * 0.10f, bottom - h * 0.14f);
            path.startNewSubPath (cx, top + h * 0.08f);
            path.lineTo (cx, bottom - h * 0.34f);
            path.startNewSubPath (left + w * 0.28f, cy);
            path.lineTo (cx, bottom - h * 0.28f);
            path.lineTo (right - w * 0.28f, cy);
            break;

        case UiActionId::TransportPlay:
            path.startNewSubPath (left + w * 0.34f, top + h * 0.22f);
            path.lineTo (right - w * 0.22f, cy);
            path.lineTo (left + w * 0.34f, bottom - h * 0.22f);
            path.closeSubPath();
            g.setColour (colour);
            g.fillPath (path);
            return true;

        case UiActionId::TransportStop:
            g.setColour (colour);
            g.fillRoundedRectangle (bounds.reduced (w * 0.28f, h * 0.28f), UiTheme::Radius::xs);
            return true;

        case UiActionId::TransportLocateStart:
            path.startNewSubPath (left + w * 0.28f, top + h * 0.24f);
            path.lineTo (left + w * 0.28f, bottom - h * 0.24f);
            path.startNewSubPath (right - w * 0.22f, top + h * 0.24f);
            path.lineTo (left + w * 0.38f, cy);
            path.lineTo (right - w * 0.22f, bottom - h * 0.24f);
            path.closeSubPath();
            break;

        case UiActionId::TransportRecord:
            g.setColour (UiTheme::Color::dangerRed());
            g.fillEllipse (bounds.reduced (w * 0.27f, h * 0.27f));
            g.setColour (UiTheme::Color::dangerRed().withAlpha (UiTheme::Tone::recordIconHaloAlpha));
            g.drawEllipse (bounds.reduced (w * 0.18f, h * 0.18f), UiTheme::Layout::iconFineStrokeWidth);
            return true;

        case UiActionId::TransportToggleLoop:
        case UiActionId::DeviceRefreshAudio:
        {
            const bool loop = action == UiActionId::TransportToggleLoop;
            path.addCentredArc (cx, cy, w * 0.31f, h * 0.31f, 0.0f,
                                juce::MathConstants<float>::pi * 0.20f,
                                juce::MathConstants<float>::pi * 1.62f, true);
            path.startNewSubPath (right - w * 0.23f, top + h * 0.25f);
            path.lineTo (right - w * 0.08f, top + h * 0.31f);
            path.lineTo (right - w * 0.20f, top + h * 0.43f);
            if (loop)
            {
                path.addCentredArc (cx, cy, w * 0.31f, h * 0.31f, 0.0f,
                                    juce::MathConstants<float>::pi * 1.20f,
                                    juce::MathConstants<float>::pi * 2.62f, true);
            }
            break;
        }

        case UiActionId::EditUndo:
        case UiActionId::EditRedo:
        {
            const bool redo = action == UiActionId::EditRedo;
            const float direction = redo ? 1.0f : -1.0f;
            path.addCentredArc (cx, cy + h * 0.06f, w * 0.29f, h * 0.29f, 0.0f,
                                redo ? juce::MathConstants<float>::pi * 1.15f
                                     : juce::MathConstants<float>::pi * 1.85f,
                                redo ? juce::MathConstants<float>::pi * 2.85f
                                     : juce::MathConstants<float>::pi * 0.15f,
                                true);
            const float tipX = cx + direction * w * 0.28f;
            const float tipY = cy - h * 0.13f;
            path.startNewSubPath (tipX, tipY);
            path.lineTo (tipX - direction * w * 0.16f, tipY - h * 0.05f);
            path.lineTo (tipX - direction * w * 0.04f, tipY + h * 0.15f);
            break;
        }

        case UiActionId::DeviceSelectTestAudio:
            path.startNewSubPath (cx - w * 0.06f, top + h * 0.08f);
            path.lineTo (cx - w * 0.06f, cy - h * 0.10f);
            path.startNewSubPath (cx + w * 0.16f, top + h * 0.08f);
            path.lineTo (cx + w * 0.16f, cy - h * 0.10f);
            path.addRoundedRectangle (left + w * 0.25f, top + h * 0.31f, w * 0.50f, h * 0.32f,
                                      UiTheme::Radius::sm);
            path.startNewSubPath (cx + w * 0.05f, top + h * 0.63f);
            path.lineTo (cx + w * 0.05f, bottom - h * 0.08f);
            break;

        case UiActionId::RecordingArmTrack:
            path.addEllipse (left + w * 0.12f, top + h * 0.12f, w * 0.76f, h * 0.76f);
            path.addEllipse (left + w * 0.36f, top + h * 0.36f, w * 0.28f, h * 0.28f);
            break;

        case UiActionId::RecordingSetMonitoringPolicy:
            path.addCentredArc (cx, cy, w * 0.31f, h * 0.31f, 0.0f,
                                juce::MathConstants<float>::pi * 1.12f,
                                juce::MathConstants<float>::pi * 2.88f, true);
            path.addRoundedRectangle (left + w * 0.15f, cy, w * 0.18f, h * 0.31f, UiTheme::Radius::xs);
            path.addRoundedRectangle (right - w * 0.33f, cy, w * 0.18f, h * 0.31f, UiTheme::Radius::xs);
            break;

        case UiActionId::RecordingAssembleComp:
            path.startNewSubPath (left + w * 0.12f, top + h * 0.25f);
            path.lineTo (cx, top + h * 0.43f);
            path.lineTo (right - w * 0.12f, top + h * 0.25f);
            path.startNewSubPath (left + w * 0.12f, cy);
            path.lineTo (cx, bottom - h * 0.26f);
            path.lineTo (right - w * 0.12f, cy);
            break;

        case UiActionId::ViewMixer:
        case UiActionId::TimelineToggleMixerDock:   // the layout pass, step 7: the view cluster's mixer toggle
        case UiActionId::ViewMixWindow:             // step 10: the header's Mix button
            for (const float column : std::array<float, 3> { 0.24f, 0.50f, 0.76f })
            {
                path.startNewSubPath (left + w * column, top + h * 0.12f);
                path.lineTo (left + w * column, bottom - h * 0.12f);
            }
            path.addRoundedRectangle (left + w * 0.16f, top + h * 0.31f, w * 0.16f, h * 0.16f,
                                      UiTheme::Radius::xs);
            path.addRoundedRectangle (left + w * 0.42f, top + h * 0.55f, w * 0.16f, h * 0.16f,
                                      UiTheme::Radius::xs);
            path.addRoundedRectangle (left + w * 0.68f, top + h * 0.23f, w * 0.16f, h * 0.16f,
                                      UiTheme::Radius::xs);
            break;

        case UiActionId::ViewPianoRoll:
            // The layout pass, step 7: a piano roll's notes - three staggered bars (the keys-and-lines picture read as a
            // battery at the view cluster's size).
            path.startNewSubPath (left + w * 0.12f, top + h * 0.24f);
            path.lineTo (left + w * 0.46f, top + h * 0.24f);
            path.startNewSubPath (left + w * 0.38f, cy);
            path.lineTo (right - w * 0.12f, cy);
            path.startNewSubPath (left + w * 0.20f, bottom - h * 0.24f);
            path.lineTo (left + w * 0.62f, bottom - h * 0.24f);
            break;

        // The layout pass, step 7: the view cluster's inspector (a window with its right-hand panel) and automation lanes
        // (a line through three breakpoints), in place of the letters I and A.
        case UiActionId::ViewToggleInspector:
            path.addRoundedRectangle (left + w * 0.08f, top + h * 0.12f, w * 0.84f, h * 0.76f, UiTheme::Radius::xs);
            path.startNewSubPath (left + w * 0.60f, top + h * 0.12f);
            path.lineTo (left + w * 0.60f, bottom - h * 0.12f);
            break;

        case UiActionId::TimelineAutomationToggleTrackLane:
        {
            const std::array<juce::Point<float>, 4> points { juce::Point<float> { left + w * 0.10f, bottom - h * 0.28f },
                                                             juce::Point<float> { left + w * 0.38f, top + h * 0.30f },
                                                             juce::Point<float> { left + w * 0.64f, top + h * 0.56f },
                                                             juce::Point<float> { right - w * 0.10f, top + h * 0.22f } };
            path.startNewSubPath (points[0]);
            for (std::size_t i = 1; i < points.size(); ++i)
                path.lineTo (points[i]);
            const float dot = juce::jmin (w, h) * 0.07f;
            for (std::size_t i = 1; i < points.size() - 1; ++i)
                path.addEllipse (points[i].x - dot, points[i].y - dot, dot * 2.0f, dot * 2.0f);
            break;
        }

        case UiActionId::ProjectExportAudioCancel:
        {
            // UI correctness pass unit 10: the export's Cancel - a cross, centred in the largest square that fits (the
            // header's 28 px slot cannot hold the word; plan §7.4 item 3 allows an unambiguous icon with a tooltip).
            const float half = juce::jmin (w, h) * 0.28f;
            path.startNewSubPath (cx - half, cy - half);
            path.lineTo (cx + half, cy + half);
            path.startNewSubPath (cx + half, cy - half);
            path.lineTo (cx - half, cy + half);
            break;
        }

        default:
            return false;
    }

    strokeIconPath (g, path, colour);
    return true;
}

inline void drawTimelineToolIcon (juce::Graphics& g,
                                  TimelineTool tool,
                                  juce::Rectangle<float> bounds,
                                  juce::Colour colour)
{
    const float left = bounds.getX();
    const float top = bounds.getY();
    const float right = bounds.getRight();
    const float bottom = bounds.getBottom();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    juce::Path path;

    switch (tool)
    {
        case TimelineTool::Pointer:
            path.startNewSubPath (left + w * 0.25f, top + h * 0.13f);
            path.lineTo (right - w * 0.21f, cy);
            path.lineTo (cx, cy + h * 0.08f);
            path.lineTo (right - w * 0.31f, bottom - h * 0.12f);
            path.lineTo (cx - w * 0.03f, bottom - h * 0.05f);
            path.lineTo (left + w * 0.25f, top + h * 0.13f);
            break;

        case TimelineTool::Pencil:
            path.startNewSubPath (left + w * 0.20f, bottom - h * 0.22f);
            path.lineTo (right - w * 0.22f, top + h * 0.20f);
            path.lineTo (right - w * 0.08f, top + h * 0.34f);
            path.lineTo (left + w * 0.34f, bottom - h * 0.08f);
            path.lineTo (left + w * 0.16f, bottom - h * 0.04f);
            path.closeSubPath();
            break;

        case TimelineTool::Scissors:
            path.addEllipse (left + w * 0.08f, top + h * 0.12f, w * 0.28f, h * 0.28f);
            path.addEllipse (left + w * 0.08f, bottom - h * 0.40f, w * 0.28f, h * 0.28f);
            path.startNewSubPath (left + w * 0.32f, cy - h * 0.08f);
            path.lineTo (right - w * 0.08f, top + h * 0.14f);
            path.startNewSubPath (left + w * 0.32f, cy + h * 0.08f);
            path.lineTo (right - w * 0.08f, bottom - h * 0.14f);
            break;

        case TimelineTool::Hand:
            path.startNewSubPath (left + w * 0.18f, cy);
            path.lineTo (left + w * 0.35f, bottom - h * 0.13f);
            path.lineTo (right - w * 0.24f, bottom - h * 0.13f);
            path.lineTo (right - w * 0.12f, cy - h * 0.10f);
            path.startNewSubPath (left + w * 0.35f, cy);
            path.lineTo (left + w * 0.35f, top + h * 0.16f);
            path.startNewSubPath (cx, cy);
            path.lineTo (cx, top + h * 0.08f);
            path.startNewSubPath (right - w * 0.35f, cy);
            path.lineTo (right - w * 0.35f, top + h * 0.17f);
            break;

        case TimelineTool::Zoom:
            path.addEllipse (left + w * 0.12f, top + h * 0.08f, w * 0.55f, h * 0.55f);
            path.startNewSubPath (cx + w * 0.10f, cy + h * 0.10f);
            path.lineTo (right - w * 0.08f, bottom - h * 0.08f);
            break;

        case TimelineTool::Eraser:   // G3.2: a slanted block with its rubbing edge
            path.startNewSubPath (left + w * 0.15f, bottom - h * 0.30f);
            path.lineTo (right - w * 0.35f, top + h * 0.12f);
            path.lineTo (right - w * 0.10f, top + h * 0.38f);
            path.lineTo (left + w * 0.40f, bottom - h * 0.05f);
            path.closeSubPath();
            path.startNewSubPath (left + w * 0.30f, cy + h * 0.05f);
            path.lineTo (cx + w * 0.10f, bottom - h * 0.22f);
            break;

        case TimelineTool::Velocity:   // G3.2: three rising bars
            path.startNewSubPath (left + w * 0.18f, bottom - h * 0.10f);
            path.lineTo (left + w * 0.18f, cy + h * 0.10f);
            path.startNewSubPath (cx, bottom - h * 0.10f);
            path.lineTo (cx, cy - h * 0.15f);
            path.startNewSubPath (right - w * 0.18f, bottom - h * 0.10f);
            path.lineTo (right - w * 0.18f, top + h * 0.12f);
            break;
    }

    strokeIconPath (g, path, colour, UiTheme::Layout::iconFineStrokeWidth);
}

// G2.17: the track KIND badge — a three-key keyboard for a MIDI track, a three-bar waveform for an
// audio track — painted beside the rail row's icon square so the kind reads at a glance (Logic /
// Pro Tools). The square itself is empty: the rail shows no instrument picture until a
// track-category model exists (UI correctness pass unit 8).
inline void drawTrackKindBadge (juce::Graphics& g, bool midi, juce::Rectangle<float> bounds, juce::Colour colour)
{
    g.setColour (colour);
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    if (midi)
    {
        const float keyW = w / 3.0f;
        for (int k = 0; k < 3; ++k)
            g.drawRect (juce::Rectangle<float> (bounds.getX() + keyW * static_cast<float> (k), bounds.getY(), keyW, h), 1.0f);
        g.fillRect (juce::Rectangle<float> (bounds.getX() + keyW * 0.7f, bounds.getY(), keyW * 0.6f, h * 0.55f));
        g.fillRect (juce::Rectangle<float> (bounds.getX() + keyW * 1.7f, bounds.getY(), keyW * 0.6f, h * 0.55f));
        return;
    }
    const float barW = w / 5.0f;
    const float heights[3] { 0.45f, 1.0f, 0.65f };
    for (int b = 0; b < 3; ++b)
    {
        const float barH = h * heights[b];
        g.fillRect (juce::Rectangle<float> (bounds.getX() + barW * (2.0f * static_cast<float> (b)), bounds.getCentreY() - barH * 0.5f, barW, barH));
    }
}

inline void drawSettingsIcon (juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              juce::Colour colour)
{
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float outer = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.36f;
    const float inner = outer * 0.38f;
    juce::Path path;
    path.addEllipse (cx - inner, cy - inner, inner * 2.0f, inner * 2.0f);
    path.addEllipse (cx - outer * 0.78f,
                     cy - outer * 0.78f,
                     outer * 1.56f,
                     outer * 1.56f);
    for (int spoke = 0; spoke < 8; ++spoke)
    {
        const float angle = static_cast<float> (spoke) * juce::MathConstants<float>::pi * 0.25f;
        path.startNewSubPath (cx + std::cos (angle) * outer * 0.72f,
                              cy + std::sin (angle) * outer * 0.72f);
        path.lineTo (cx + std::cos (angle) * outer,
                     cy + std::sin (angle) * outer);
    }
    strokeIconPath (g, path, colour, UiTheme::Layout::iconFineStrokeWidth);
}

} // namespace yesdaw::ui
