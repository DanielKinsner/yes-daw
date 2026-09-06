// YES DAW — the app shell: the piano roll.
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): member bodies of MainComponent, carved
// verbatim from the inline class. The declaration is ui/MainComponentShell.h.

#include "ui/MainComponentShell.h"

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

void MainComponent::handleCapturedMidiNote (std::int64_t frame, bool noteOn, int note, float velocity)
{
    if (noteOn)
    {
        pendingMidiNoteOns[note] = { frame, velocity };
        return;
    }

    const auto pending = pendingMidiNoteOns.find (note);
    if (pending == pendingMidiNoteOns.end())
        return;

    const auto [startFrame, onVelocity] = pending->second;
    pendingMidiNoteOns.erase (pending);
    if (frame <= startFrame)
        return;

    (void) appModel.captureMidiEventDuringRecording (
        { startFrame, static_cast<std::uint8_t> (note), onVelocity, frame - startFrame });
}

bool MainComponent::dockShowsPianoRoll() const noexcept
{
    return appModel.context().mixerDockVisible
        && appModel.context().editorDockTab == yesdaw::ui::UiEditorDockTab::PianoRoll;
}

// G3.2: the roll pages after the playhead while it shows and the transport rolls — the same
// page law as the arrangement, in the clip's ticks. Returns true when the view moved.
bool MainComponent::followPianoRollPlayhead()
{
    if (! dockShowsPianoRoll() || ! appModel.context().playheadFollowEnabled || ! appModel.context().isPlaying)
        return false;
    const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = currentPianoRollSurface();
    if (! surface.midiClipSelected || surface.playheadTick < 0 || surface.playheadTick > surface.timelineLength)
        return false;
    const yesdaw::engine::Tick visible = pianoRollVisibleTicks (surface);
    if (surface.playheadTick >= surface.viewScrollTicks && surface.playheadTick < surface.viewScrollTicks + visible)
        return false;
    const yesdaw::engine::Tick lead = visible * yesdaw::ui::UiTheme::Layout::pianoRollFollowLeadPercent / 100;
    const yesdaw::engine::Tick maxScroll = std::max<yesdaw::engine::Tick> (0, surface.timelineLength - visible);
    pianoRollViewScrollTicks = std::clamp<yesdaw::engine::Tick> (surface.playheadTick - lead, 0, maxScroll);
    return true;
}

void MainComponent::drawPianoRoll (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const auto surface = currentPianoRollSurface();
    const auto panelArea = area;

    fillPanel (g, area);
    auto header = area.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollHeaderHeight);
    drawSmallLabel (g,
                    "PIANO ROLL",
                    header.reduced (yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetX,
                                    yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetY));
    // E9: the header names the OPEN clip's owning track so switching clips is legible.
    juce::String rollTitle = "No MIDI Clip selected";
    if (surface.midiClipSelected)
    {
        rollTitle = "MIDI Clip";
        for (const yesdaw::engine::MidiClip& midiClip : appModel.project().midiClips)
        {
            if (midiClip.id != appModel.selectedMidiClipId())
                continue;
            for (const yesdaw::engine::Track& track : appModel.project().tracks)
                if (track.id == midiClip.trackId && ! track.strip.name.empty())
                    rollTitle = juce::String (track.strip.name);
            break;
        }
        rollTitle << "  |  Note edits: select move length transpose quantize";
    }
    drawSmallLabel (g, rollTitle,
                    header.reduced (yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetX,
                                    yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetY),
                    juce::Justification::centredRight);

    const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (panelArea);

    g.setColour (yesdaw::ui::UiTheme::Color::controlInsetBlack());
    g.fillRect (geometry.grid);

    for (int key = pianoRollViewHighKey (surface);
         key >= surface.viewLowKey;
         --key)
    {
        const int y = pianoRollKeyY (geometry, surface, key);
        auto keyRow = juce::Rectangle<int> (geometry.keyboard.getX(),
                                            y,
                                            geometry.keyboard.getWidth(),
                                            juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollKeyRowMinHeight,
                                                        juce::roundToInt (geometry.rowHeight)));
        // M8: a real keyboard — white keys light and full width, black keys dark and narrower,
        // sitting on top from the left edge exactly as they do on a piano.
        const auto keyBody = keyRow.reduced (yesdaw::ui::UiTheme::Layout::pianoRollKeyRowInsetX,
                                             yesdaw::ui::UiTheme::Layout::pianoRollKeyRowInsetY);
        // G3.8: scale assist — a grid row inside the project's scale lifts off the black grid.
        if (surface.scaleChoice != yesdaw::engine::ProjectScale::kScaleOff
            && yesdaw::ui::pianoRollKeyInScale (key, surface.scaleRoot, surface.scaleChoice))
        {
            g.setColour (yesdaw::ui::UiTheme::Color::pianoRollInScaleRow());
            g.fillRect (juce::Rectangle<int> (geometry.grid.getX(), y, geometry.grid.getWidth(), keyRow.getHeight()));
        }
        g.setColour (yesdaw::ui::UiTheme::Color::pianoWhiteKey());
        g.fillRect (keyBody);
        g.setColour (kPanelStroke);
        g.drawRect (keyBody, yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth);
        if (isBlackMidiKey (key))
        {
            g.setColour (yesdaw::ui::UiTheme::Color::pianoBlackKey());
            g.fillRect (keyBody.withWidth (juce::roundToInt (
                static_cast<float> (keyBody.getWidth())
                * yesdaw::ui::UiTheme::Layout::pianoRollBlackKeyWidthScale)));
        }
        g.setColour (kPanelStroke.withAlpha (0.72f));
        g.fillRect (juce::Rectangle<int> (geometry.grid.getX(),
                                         y,
                                         geometry.grid.getWidth(),
                                         yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth));

        // G3.9: drum mode — a Sampler's pad names its key (bold, over the key body); a key
        // without a pad keeps the note name, dimmed.
        const std::string* const padName = surface.drumMode ? surface.padNameForKey (key) : nullptr;
        if (padName != nullptr)
        {
            g.setColour (yesdaw::ui::UiTheme::Color::pianoWhiteKeyText());
            g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::caption, juce::Font::bold));
            g.drawText (juce::String (*padName),
                        keyRow.reduced (yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetX,
                                        yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetY),
                        juce::Justification::centredLeft, true);
        }
        // G3.2: every white key names itself once its row is tall enough; C keeps its bold octave
        // label at any height (the landmark Logic paints).
        else if (key % 12 == 0
            || (! isBlackMidiKey (key)
                && juce::roundToInt (geometry.rowHeight) >= yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelMinRowHeight))
        {
            g.setColour (surface.drumMode ? yesdaw::ui::UiTheme::Color::pianoWhiteKeyText().withAlpha (0.5f)
                                          : yesdaw::ui::UiTheme::Color::pianoWhiteKeyText());
            g.setFont (yesdaw::ui::UiTheme::Type::font (
                yesdaw::ui::UiTheme::Type::caption,
                key % 12 == 0 ? juce::Font::bold : juce::Font::plain));
            g.drawText (juce::String (yesdaw::ui::pianoRollKeyName (key)),
                        keyRow.reduced (yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetX,
                                        yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetY),
                        juce::Justification::centredLeft, false);
        }
    }

    // G3.2: the grid follows the meter and the snap (plan §3.2): bar lines strong, beat lines
    // weak, snap subdivisions fainter and only while their cells are wide enough to read.
    for (const yesdaw::ui::PianoRollGridLine& line : pianoRollGridLines (geometry, surface))
    {
        g.setColour (line.kind == yesdaw::ui::PianoRollGridLineKind::Bar ? yesdaw::ui::UiTheme::Color::pianoGridStrong()
                     : line.kind == yesdaw::ui::PianoRollGridLineKind::Beat ? yesdaw::ui::UiTheme::Color::pianoGridWeak()
                     : yesdaw::ui::UiTheme::Color::pianoGridWeak().withAlpha (0.45f));
        g.fillRect (line.x,
                    geometry.grid.getY(),
                    yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth,
                    geometry.grid.getHeight());
    }

    // G3.2: the shared playhead, clip-relative.
    if (surface.playheadTick >= 0 && surface.playheadTick <= surface.timelineLength)
    {
        const int x = pianoRollTickX (geometry, surface, surface.playheadTick);
        if (x >= geometry.grid.getX() && x <= geometry.grid.getRight())
        {
            g.setColour (kText);
            g.fillRect (x, geometry.grid.getY(), yesdaw::ui::UiTheme::Layout::pianoRollPlayheadWidth, geometry.grid.getHeight());
        }
    }

    for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
    {
        if (note.key < surface.viewLowKey || note.key > pianoRollViewHighKey (surface))
            continue;

        const auto noteRect = pianoRollNoteBounds (geometry, surface, note)
                                  .getIntersection (geometry.grid);
        if (noteRect.isEmpty())
            continue;

        g.setColour ((note.selected ? kPurple : kCyan).withAlpha (0.34f));
        g.fillRoundedRectangle (noteRect.expanded (yesdaw::ui::UiTheme::Layout::pianoRollSelectedNoteHalo).toFloat(),
                                yesdaw::ui::UiTheme::Radius::md);
        // Velocity tints the note body (B33): quiet notes darken toward the tint floor.
        g.setColour ((note.selected ? kPurple.brighter (0.35f) : kCyan)
                         .withMultipliedBrightness (
                             yesdaw::ui::UiTheme::Tone::noteVelocityTintFloor
                             + static_cast<float> (note.normalizedVelocity)
                                   * (1.0f - yesdaw::ui::UiTheme::Tone::noteVelocityTintFloor)));
        g.fillRoundedRectangle (noteRect.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
    }

    auto expression = geometry.expression;
    expression.reduce (yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetX,
                       yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetY);
    for (const yesdaw::ui::UiPianoRollExpressionLaneReadout& lane : surface.expressionLanes)
    {
        auto laneArea = expression.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneHeight)
                            .reduced (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetX,
                                      yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetY);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRect (laneArea);
        // G3.3: the control lane's name is its chooser (a child in the gutter); the velocity lane keeps its label.
        if (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity)
            drawSmallLabel (g,
                            "Velocity",
                            laneArea.reduced (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLabelInsetX,
                                              yesdaw::ui::UiTheme::Layout::pianoRollExpressionLabelInsetY));

        const double minValue = lane.valueMin;
        const double maxValue = lane.valueMax;

        if (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Control)
        {
            // The lane's data area (the grid's x span) and, for a bend, its centre line at 0.
            const juce::Rectangle<int> data = pianoRollControlLaneDataArea (geometry);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInsetBlack());
            g.fillRect (data);
            if (minValue < 0.0)
            {
                const int centreY = pianoRollControlLaneYForValue (data, 0.0, minValue, maxValue);
                g.setColour (kPanelStroke.withAlpha (0.72f));
                g.fillRect (juce::Rectangle<int> (data.getX(), centreY, data.getWidth(),
                                                 yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth));
            }
        }

        // M8: velocity is a BAR per note, anchored at the note's start and rising from the lane
        // floor — the joined line read as an automation curve between notes that never existed.
        if (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity)
        {
            const int floorY = laneArea.getBottom()
                             - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathBottomInset;
            const int span = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollVelocityBarMinHeight,
                                         laneArea.getHeight()
                                             - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathVerticalInset);
            g.setColour (yesdaw::ui::UiTheme::Meter::nominalFill());
            for (const auto& point : lane.points)
            {
                const double normalized = juce::jlimit (0.0, 1.0,
                                                        (point.value - minValue) / (maxValue - minValue));
                const int x = pianoRollTickX (geometry, surface, point.tick);
                const int height = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollVelocityBarMinHeight,
                                               juce::roundToInt (normalized * static_cast<double> (span)));
                g.fillRect (x, floorY - height,
                            yesdaw::ui::UiTheme::Layout::pianoRollVelocityBarWidth, height);
            }
            continue;
        }

        juce::Path path;

        for (std::size_t i = 0; i < lane.points.size(); ++i)
        {
            const auto& point = lane.points[i];
            const double normalized = juce::jlimit (0.0, 1.0, (point.value - minValue) / (maxValue - minValue));
            const float x = static_cast<float> (pianoRollTickX (geometry, surface, point.tick));
            const float y = static_cast<float> (laneArea.getBottom()
                                                - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathBottomInset)
                - static_cast<float> (normalized)
                    * static_cast<float> (laneArea.getHeight()
                                          - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathVerticalInset);
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);

            g.setColour (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity
                              ? yesdaw::ui::UiTheme::Meter::nominalFill()
                              : kPurple);
            g.fillEllipse (x - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPointRadius,
                           y - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPointRadius,
                           yesdaw::ui::UiTheme::Layout::pianoRollExpressionPointDiameter,
                           yesdaw::ui::UiTheme::Layout::pianoRollExpressionPointDiameter);
        }

        g.setColour (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity
                          ? yesdaw::ui::UiTheme::Meter::nominalFill()
                          : kPurple);
        g.strokePath (path,
                      juce::PathStrokeType (
                          yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathStrokeWidth));
    }
}

yesdaw::ui::UiPianoRollSurfaceSnapshot MainComponent::currentPianoRollSurface() const
{
    if (appModel.context().projectLoaded)
    {
        yesdaw::engine::EntityId midiClipId = appModel.selectedMidiClipId();
        if (! midiClipId.isValid() && ! appModel.project().midiClips.empty())
            midiClipId = appModel.project().midiClips.front().id;

        yesdaw::ui::UiPianoRollSurfaceSnapshot surface = yesdaw::ui::projectUiPianoRollSurface (
            appModel.project(),
            midiClipId,
            appModel.selectedMidiNoteId(),
            appModel.selectedMidiNoteIds(),
            appModel.context().pianoRollControlLaneChoice);   // G3.3

        // Piano-roll viewport (E10): the surface publishes the CLAMPED view so every paint,
        // hit-test, and gesture consumer shares one law.
        // G3.2 FIX 3: the key window follows the roll's grid height (one law with the geometry).
        surface.viewKeyCount = pianoRollCanvasGeometry (pianoRollInput.getBounds().withZeroOrigin()).visibleKeys;
        pianoRollViewLowKey = std::clamp (
            pianoRollViewLowKey,
            yesdaw::ui::UiThemeLayout::pianoRollKeyMin,
            yesdaw::ui::UiThemeLayout::pianoRollKeyMax - (surface.viewKeyCount - 1));
        pianoRollViewZoom = std::clamp (pianoRollViewZoom,
                                        yesdaw::ui::UiThemeLayout::pianoRollZoomMin,
                                        yesdaw::ui::UiThemeLayout::pianoRollZoomMax);
        surface.viewLowKey = pianoRollViewLowKey;
        surface.viewZoom = pianoRollViewZoom;
        const yesdaw::engine::Tick length = juce::jmax<yesdaw::engine::Tick> (1, surface.timelineLength);
        const yesdaw::engine::Tick visible = juce::jmax<yesdaw::engine::Tick> (
            1, static_cast<yesdaw::engine::Tick> (
                   std::llround (static_cast<double> (length) / pianoRollViewZoom)));
        pianoRollViewScrollTicks = std::clamp<yesdaw::engine::Tick> (
            pianoRollViewScrollTicks, 0, juce::jmax<yesdaw::engine::Tick> (0, length - visible));
        surface.viewScrollTicks = pianoRollViewScrollTicks;
        // E12: note gestures snap through the real chooser.
        surface.snapEnabled = appModel.context().snapEnabled;
        surface.snapGridTicks = static_cast<yesdaw::engine::Tick> (appModel.context().snapGridTicks);
        // G3.2: the grid follows the meter in force at the clip (bars / beats) and the snap; the
        // shared playhead is published clip-relative (-1 when it has no tick).
        {
            // G3.6 (found by the step gate): the grid and the playhead speak the CLIP's ticks — every
            // shell-made MIDI clip is SampleLocked (tick == frame), so a beat is the head tempo's
            // beat in frames and the playhead is the transport frame; G3.2 had used the tempo map's
            // musical ticks for both, which ran 0.64x slow on a 120 BPM / 48 kHz clip.
            const yesdaw::engine::MidiClip* clip = nullptr;
            for (const yesdaw::engine::MidiClip& candidate : appModel.project().midiClips)
                if (candidate.id == midiClipId)
                    clip = &candidate;
            if (clip != nullptr)
            {
                surface.beatTicks = appModel.midiClipBeatTicks (*clip);
                surface.barTicks = appModel.midiClipBarTicks (*clip);
                if (const std::optional<yesdaw::engine::Tick> playhead = appModel.playheadTickForClip (*clip))
                    surface.playheadTick = *playhead - surface.timelineStart;
            }
        }
        return surface;
    }

    return {};
}

} // namespace yesdaw::ui
