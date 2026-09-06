// YES DAW — the app shell: the inspector.
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): member bodies of MainComponent, carved
// verbatim from the inline class. The declaration is ui/MainComponentShell.h.

#include "ui/MainComponentShell.h"

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

// V7: the inspector's CLIP/TRACK tabs become real buttons — each dispatches a genuine
// UiActionId, the model owns the active-tab state, and layout/paint follow it.
void MainComponent::configureInspectorTabs()
{
    const auto configureTab = [this] (juce::TextButton& button,
                                      yesdaw::ui::UiActionId action,
                                      const char* fallbackText)
    {
        configureActionComponent (button, action, fallbackText);
        if (const auto* descriptor = appModel.registry().descriptor (action))
            button.setButtonText (descriptor->label);
        else
            button.setButtonText (fallbackText);
        button.setColour (juce::TextButton::buttonColourId,
                          yesdaw::ui::UiTheme::Color::buttonSurface());
        button.setColour (juce::TextButton::buttonOnColourId,
                          yesdaw::ui::UiTheme::Color::inspectorTab());
        button.setColour (juce::TextButton::textColourOffId, kMutedText);
        button.setColour (juce::TextButton::textColourOnId, kText);
        button.onClick = [this, action] {
            (void) appModel.dispatch (action);
            refreshActionState();
            resized();
            repaintAll();
        };
        addAndMakeVisible (button);
    };
    configureTab (inspectorClipTab, yesdaw::ui::UiActionId::InspectorShowClipTab, "Clip");
    configureTab (inspectorTrackTab, yesdaw::ui::UiActionId::InspectorShowTrackTab, "Track");
}

void MainComponent::configureInspectorControls()
{
    // E33: the take stack — a real TAKES section replaces the old always-"No automation"
    // placeholder. The chooser switches the AUDIBLE take; Delete Take removes one.
    inspectorTakeChooser.setComponentID ("clip.inspector.take.chooser");
    inspectorTakeChooser.setTooltip ("Switch the audible take for this clip's window");
    inspectorTakeChooser.setName ("Take chooser");
    inspectorTakeChooser.setTitle ("Take chooser");
    inspectorTakeChooser.setTextWhenNothingSelected ("Takes");
    inspectorTakeChooser.setTextWhenNoChoicesAvailable ("No takes");
    inspectorTakeChooser.onChange = [this] {
        if (refreshingInspectorControls)
            return;

        const int selected = inspectorTakeChooser.getSelectedId();
        if (selected <= 0
            || static_cast<std::size_t> (selected - 1) >= inspectorTakeViews.size())
            return;

        (void) appModel.switchAudibleTakeForSelectedClip (
            inspectorTakeViews[static_cast<std::size_t> (selected - 1)].takeId);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (inspectorTakeChooser);

    // G2.14: the marker list (every marker, tick order, seconds readout); a click locates.
    inspectorMarkerList.setComponentID ("clip.inspector.markers");
    inspectorMarkerList.setName ("Markers");
    inspectorMarkerList.setTitle ("Markers");
    inspectorMarkerList.setTooltip ("Markers: click one to move the playhead there");
    inspectorMarkerList.setRowHeight (yesdaw::ui::UiTheme::Layout::inspectorMarkerRowHeight);
    inspectorMarkerListModel.rowCount = [this] { return static_cast<int> (appModel.project().markers.size()); };
    inspectorMarkerListModel.rowText = [this] (int row) -> juce::String
    {
        const auto& markers = appModel.project().markers;
        if (row < 0 || row >= static_cast<int> (markers.size()) || ! appModel.project().sampleRate.isValid())
            return {};
        const yesdaw::engine::Marker& marker = markers[static_cast<std::size_t> (row)];
        const double seconds = static_cast<double> (marker.tick) / appModel.project().sampleRate.hz;
        return juce::String (marker.name) + "   " + juce::String (seconds, 3) + " s";
    };
    inspectorMarkerListModel.onRowClicked = [this] (int row)
    {
        const auto& markers = appModel.project().markers;
        if (row < 0 || row >= static_cast<int> (markers.size()))
            return;
        (void) appModel.locatePlaybackFrame (markers[static_cast<std::size_t> (row)].tick);
        refreshActionState();
        repaintAll();
    };
    inspectorMarkerList.setModel (&inspectorMarkerListModel);
    addAndMakeVisible (inspectorMarkerList);

    inspectorTakeDelete.setComponentID ("clip.inspector.take.delete");
    inspectorTakeDelete.setButtonText ("Delete Take");
    inspectorTakeDelete.setTooltip ("Delete the chosen take (its clip goes with it)");
    inspectorTakeDelete.setName ("Delete take");
    inspectorTakeDelete.setTitle ("Delete take");
    inspectorTakeDelete.onClick = [this] {
        const int selected = inspectorTakeChooser.getSelectedId();
        if (selected <= 0
            || static_cast<std::size_t> (selected - 1) >= inspectorTakeViews.size())
            return;

        (void) appModel.deleteRecordingTake (
            inspectorTakeViews[static_cast<std::size_t> (selected - 1)].takeId);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (inspectorTakeDelete);

    configureInspectorTimeSlider (inspectorStart, kInspectorStartComponentId, "Clip start");
    inspectorStart.onValueChange = [this] {
        if (refreshingInspectorControls || ! inspectorStart.isEnabled())
            return;

        setSelectedInspectorStartFromSlider();
    };
    addAndMakeVisible (inspectorStart);

    configureInspectorTimeSlider (inspectorEnd, kInspectorEndComponentId, "Clip end");
    inspectorEnd.onValueChange = [this] {
        if (refreshingInspectorControls || ! inspectorEnd.isEnabled())
            return;

        setSelectedInspectorEndFromSlider();
    };
    addAndMakeVisible (inspectorEnd);

    configureInspectorTimeSlider (inspectorLength, kInspectorLengthComponentId, "Clip length");
    inspectorLength.onValueChange = [this] {
        if (refreshingInspectorControls || ! inspectorLength.isEnabled())
            return;

        setSelectedInspectorLengthFromSlider();
    };
    addAndMakeVisible (inspectorLength);

    configureActionComponent (inspectorGain, yesdaw::ui::UiActionId::TimelineClipSetGain, "Clip gain");
    inspectorGain.setSliderStyle (juce::Slider::LinearHorizontal);
    inspectorGain.setTextBoxStyle (juce::Slider::NoTextBox,
                                   false,
                                   yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                   yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
    inspectorGain.setRange (yesdaw::ui::UiTheme::Layout::inspectorGainSliderMin,
                            yesdaw::ui::UiTheme::Layout::inspectorGainSliderMax,
                            yesdaw::ui::UiTheme::Layout::inspectorGainSliderInterval);
    inspectorGain.setValue (yesdaw::ui::UiTheme::Layout::inspectorGainSliderDefault,
                            juce::dontSendNotification);
    inspectorGain.onValueChange = [this] {
        if (refreshingInspectorControls || ! inspectorGain.isEnabled())
            return;

        (void) appModel.setSelectedTimelineClipGain (static_cast<float> (inspectorGain.getValue()));
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (inspectorGain);

    // G2.9b: the Stretch field — percent of the source length (100 = unstretched).
    inspectorStretch.setComponentID (kInspectorStretchComponentId);
    inspectorStretch.setName ("Clip stretch");
    inspectorStretch.setTitle ("Clip stretch");
    inspectorStretch.setTooltip ("Clip stretch: percent of the source length (50..200)");
    inspectorStretch.setSliderStyle (juce::Slider::LinearHorizontal);
    inspectorStretch.setTextBoxStyle (juce::Slider::NoTextBox,
                                      false,
                                      yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                      yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
    inspectorStretch.setRange (yesdaw::ui::UiTheme::Layout::inspectorStretchSliderMin,
                               yesdaw::ui::UiTheme::Layout::inspectorStretchSliderMax,
                               yesdaw::ui::UiTheme::Layout::inspectorStretchSliderInterval);
    inspectorStretch.setValue (yesdaw::ui::UiTheme::Layout::inspectorStretchSliderDefault,
                               juce::dontSendNotification);
    inspectorStretch.onValueChange = [this] {
        if (refreshingInspectorControls || ! inspectorStretch.isEnabled())
            return;

        (void) appModel.setSelectedTimelineClipStretchFactor (static_cast<float> (inspectorStretch.getValue() / 100.0));
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (inspectorStretch);

    configureInspectorFadeSlider (inspectorFadeIn, kInspectorFadeInComponentId, "Clip fade in");
    inspectorFadeIn.onValueChange = [this] {
        if (refreshingInspectorControls || ! inspectorFadeIn.isEnabled())
            return;

        setSelectedInspectorFadesFromSliders();
    };
    addAndMakeVisible (inspectorFadeIn);

    configureInspectorFadeSlider (inspectorFadeOut, kInspectorFadeOutComponentId, "Clip fade out");
    inspectorFadeOut.onValueChange = [this] {
        if (refreshingInspectorControls || ! inspectorFadeOut.isEnabled())
            return;

        setSelectedInspectorFadesFromSliders();
    };
    addAndMakeVisible (inspectorFadeOut);

    inspectorFadeCurve.setComponentID (kInspectorFadeCurveComponentId);
    inspectorFadeCurve.setTooltip ("Clip fade curve shape");
    inspectorFadeCurve.setName ("Clip fade curve");
    inspectorFadeCurve.setTitle ("Clip fade curve");
    inspectorFadeCurve.setTooltip ("H14 canonical fade law");
    inspectorFadeCurve.addItem ("Equal power", kInspectorEqualPowerFadeCurveId);
    inspectorFadeCurve.addItem ("Linear", kInspectorLinearFadeCurveId);      // G2.10
    inspectorFadeCurve.addItem ("S-curve", kInspectorSCurveFadeCurveId);
    inspectorFadeCurve.addItem ("Log", kInspectorLogFadeCurveId);
    inspectorFadeCurve.setSelectedId (kInspectorEqualPowerFadeCurveId, juce::dontSendNotification);
    inspectorFadeCurve.onChange = [this] {
        if (refreshingInspectorControls || ! inspectorFadeCurve.isEnabled())
            return;

        // G2.10: the chooser sets BOTH ends' shape; the curve amounts stay.
        const yesdaw::engine::FadeShape shape = fadeShapeForInspectorId (inspectorFadeCurve.getSelectedId());
        if (const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId()))
            (void) appModel.setSelectedTimelineClipFadeShapes (shape, clip->fadeInCurve, shape, clip->fadeOutCurve);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (inspectorFadeCurve);

    // G2.10: the curve amount — one row for both ends (-100..100 = the engine's -1..1).
    inspectorFadeCurveAmount.setComponentID (kInspectorFadeCurveAmountComponentId);
    inspectorFadeCurveAmount.setName ("Clip fade curve amount");
    inspectorFadeCurveAmount.setTitle ("Clip fade curve amount");
    inspectorFadeCurveAmount.setTooltip ("Clip fade curve amount: bends both fades (-100 slow rise .. 100 fast rise)");
    inspectorFadeCurveAmount.setSliderStyle (juce::Slider::LinearHorizontal);
    inspectorFadeCurveAmount.setTextBoxStyle (juce::Slider::NoTextBox,
                                              false,
                                              yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                              yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
    inspectorFadeCurveAmount.setRange (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountMin,
                                       yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountMax,
                                       yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountInterval);
    inspectorFadeCurveAmount.setValue (0.0, juce::dontSendNotification);
    inspectorFadeCurveAmount.onValueChange = [this] {
        if (refreshingInspectorControls || ! inspectorFadeCurveAmount.isEnabled())
            return;

        const auto amount = static_cast<float> (inspectorFadeCurveAmount.getValue() / 100.0);
        if (const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId()))
            (void) appModel.setSelectedTimelineClipFadeShapes (clip->fadeInShape, amount, clip->fadeOutShape, amount);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (inspectorFadeCurveAmount);
}

void MainComponent::configureInspectorTimeSlider (juce::Slider& slider, const char* componentId, const juce::String& name)
{
    slider.setComponentID (componentId);
    slider.setName (name);
    slider.setTitle (name);
    slider.setTooltip (componentId);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox,
                            false,
                            yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                            yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
    slider.setRange (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                     yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback,
                     yesdaw::ui::UiTheme::Layout::inspectorTimeSliderIntervalSeconds);
    slider.setValue (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderDefaultSeconds,
                     juce::dontSendNotification);
    slider.setColour (juce::Slider::backgroundColourId, yesdaw::ui::UiTheme::Color::transparent());
    slider.setColour (juce::Slider::trackColourId, yesdaw::ui::UiTheme::Color::transparent());
    slider.setColour (juce::Slider::thumbColourId, yesdaw::ui::UiTheme::Color::transparent());
}

// G2.10: the chooser id <-> engine shape.
yesdaw::engine::FadeShape MainComponent::fadeShapeForInspectorId (int id) noexcept
{
    switch (id)
    {
        case kInspectorLinearFadeCurveId: return yesdaw::engine::FadeShape::Linear;
        case kInspectorSCurveFadeCurveId: return yesdaw::engine::FadeShape::SCurve;
        case kInspectorLogFadeCurveId:    return yesdaw::engine::FadeShape::Log;
        default:                          return yesdaw::engine::FadeShape::EqualPower;
    }
}

int MainComponent::inspectorIdForFadeShape (yesdaw::engine::FadeShape shape) noexcept
{
    switch (shape)
    {
        case yesdaw::engine::FadeShape::Linear:     return kInspectorLinearFadeCurveId;
        case yesdaw::engine::FadeShape::SCurve:     return kInspectorSCurveFadeCurveId;
        case yesdaw::engine::FadeShape::Log:        return kInspectorLogFadeCurveId;
        case yesdaw::engine::FadeShape::EqualPower: break;
    }
    return kInspectorEqualPowerFadeCurveId;
}

void MainComponent::configureInspectorFadeSlider (juce::Slider& slider, const char* componentId, const juce::String& name)
{
    slider.setComponentID (componentId);
    slider.setName (name);
    slider.setTitle (name);
    slider.setTooltip (componentId);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox,
                            false,
                            yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                            yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
    slider.setRange (yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMinSeconds,
                     yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMaxSeconds,
                     yesdaw::ui::UiTheme::Layout::inspectorFadeSliderIntervalSeconds);
    slider.setValue (yesdaw::ui::UiTheme::Layout::inspectorFadeSliderDefaultSeconds,
                     juce::dontSendNotification);
}

// G1.4: the inspector's width right now — the token, or nothing while it is hidden (I).
int MainComponent::inspectorWidthNow() const noexcept
{
    return appModel.context().inspectorVisible ? viewState.inspectorWidth : 0;
}

void MainComponent::setInspectorWidth (int width)
{
    viewState.inspectorWidth = juce::jlimit (yesdaw::ui::UiTheme::Layout::inspectorMinWidth,
                                             yesdaw::ui::UiTheme::Layout::inspectorMaxWidth, width);
    resized();
    repaintAll();
}

// G3.4: the quantize panel shows on the CLIP tab when a MIDI clip is selected and no audio clip
// is (an audio clip's own card wins; the TRACK tab is the track's).
bool MainComponent::inspectorShowsQuantizePanel() const
{
    return ! appModel.context().inspectorTrackTabActive
        && appModel.context().projectLoaded
        && appModel.selectedMidiClipId().isValid()
        && findProjectClipById (appModel.selectedTimelineClipId()) == nullptr;
}

std::array<juce::Rectangle<int>, MainComponent::kMidiClipInspectorRows> MainComponent::midiClipInspectorRows (juce::Rectangle<int> content) noexcept
{
    std::array<juce::Rectangle<int>, kMidiClipInspectorRows> rows {};
    auto body = content.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop);
    for (juce::Rectangle<int>& row : rows)
        row = body.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorQuantizeRowHeight)
                  .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetX,
                            yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetY);
    return rows;
}

juce::Rectangle<int> MainComponent::inspectorBounds() const
{
    auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
    work.removeFromBottom (dockedMixerHeight());
    if (! appModel.context().inspectorVisible)
        return {};
    return work.removeFromRight (viewState.inspectorWidth)
        .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                  yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
}

void MainComponent::layoutInspectorControls()
{
    auto area = inspectorBounds();
    // V7: the tab strip is owned by the two REAL tab buttons — one shared cell law for
    // layout and (absence of) paint, so the clickable tabs can never drift from the strip.
    auto tabStrip = area.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorTabHeight);
    inspectorClipTab.setBounds (
        tabStrip.removeFromLeft (tabStrip.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorTabCount));
    inspectorTrackTab.setBounds (tabStrip);
    area.reduce (yesdaw::ui::UiTheme::Layout::inspectorContentInsetX,
                 yesdaw::ui::UiTheme::Layout::inspectorContentInsetY);

    // V7: the TRACK tab shows track-scoped painted content only — every clip-scoped overlay
    // control drops WHOLE (the same empty-bounds law the section-fit drop already uses).
    if (appModel.context().inspectorTrackTabActive)
    {
        inspectorMidiMute.setBounds ({});   // G3.5: CLIP-tab content
        inspectorMidiTranspose.setBounds ({});
        inspectorMidiVelocity.setBounds ({});
        inspectorMidiLoop.setBounds ({});
        inspectorQuantizeGrid.setBounds ({});   // G3.4: the panel is CLIP-tab content
        inspectorQuantizeStrength.setBounds ({});
        inspectorQuantizeSwing.setBounds ({});
        inspectorQuantizeEnds.setBounds ({});
        inspectorQuantizeHumanize.setBounds ({});
        inspectorQuantizeApply.setBounds ({});
        inspectorStart.setBounds ({});
        inspectorEnd.setBounds ({});
        inspectorLength.setBounds ({});
        inspectorGain.setBounds ({});
        inspectorStretch.setBounds ({});
        inspectorFadeIn.setBounds ({});
        inspectorFadeOut.setBounds ({});
        inspectorFadeCurve.setBounds ({});
        inspectorFadeCurveAmount.setBounds ({});
        inspectorTakeChooser.setBounds ({});
        inspectorTakeDelete.setBounds ({});
        // G3.1: the instrument row is the TRACK tab's first row — the chooser and Edit sit on
        // its right, the painted "Instrument" label on its left (drawTrackInspector).
        auto instrumentRow = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                                 .removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeRowHeight)
                                 .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetX,
                                           yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetY);
        const bool rowFits = area.contains (instrumentRow) && appModel.selectedTrackForInstrument() != nullptr;
        inspectorInstrumentEdit.setBounds (rowFits ? instrumentRow.removeFromRight (yesdaw::ui::UiTheme::Layout::inspectorInstrumentEditWidth) : juce::Rectangle<int> {});
        inspectorInstrumentChooser.setBounds (rowFits ? instrumentRow.removeFromRight (yesdaw::ui::UiTheme::Layout::inspectorInstrumentChooserWidth) : juce::Rectangle<int> {});
        return;
    }
    inspectorInstrumentEdit.setBounds ({});
    inspectorInstrumentChooser.setBounds ({});
    // G3.4: the CLIP tab of a MIDI clip (no audio clip selected) is the quantize panel; its
    // rows share one law with drawMidiClipInspector (quantizeInspectorRows).
    {
        const bool showQuantize = inspectorShowsQuantizePanel();
        const std::array<juce::Rectangle<int>, kMidiClipInspectorRows> rows = midiClipInspectorRows (area);
        const auto control = [&] (std::size_t row, int width) {
            juce::Rectangle<int> rect = rows[row];
            return showQuantize && area.contains (rect) ? rect.removeFromRight (width) : juce::Rectangle<int> {};
        };
        // G3.5: the clip's own rows first, then G3.4's quantize rows.
        inspectorMidiMute.setBounds (control (0, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorMidiTranspose.setBounds (control (1, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorMidiVelocity.setBounds (control (2, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorMidiLoop.setBounds (control (3, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorQuantizeGrid.setBounds (control (4, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorQuantizeStrength.setBounds (control (5, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorQuantizeSwing.setBounds (control (6, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorQuantizeEnds.setBounds (control (7, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorQuantizeHumanize.setBounds (control (8, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
        inspectorQuantizeApply.setBounds (control (9, yesdaw::ui::UiTheme::Layout::inspectorQuantizeApplyWidth));
    }
    // E24/E27: ONE law for paint and controls — an inspector section that no longer fits
    // the column is dropped WHOLE (card, labels, and controls), never split across the
    // panel edge or bled over the bottom mixer panel.
    const juce::Rectangle<int> content = area;
    const auto sectionFits = [content] (juce::Rectangle<int> section)
    {
        return content.contains (section);
    };
    const auto statsSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                                  .withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionHeight);
    const auto statsCells = yesdaw::ui::UiTheme::Layout::inspectorStatsCells (statsSection);   // G3.1 rubric FIX 1
    const auto startCell = statsCells[0].reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                                  yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
    const auto endCell = statsCells[1].reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                                yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
    const auto lengthCell = statsCells[2].reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                                   yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
    const bool statsFit = sectionFits (statsSection);
    inspectorStart.setBounds (statsFit ? startCell : juce::Rectangle<int>());
    inspectorEnd.setBounds (statsFit ? endCell : juce::Rectangle<int>());
    inspectorLength.setBounds (statsFit ? lengthCell : juce::Rectangle<int>());

    // G3.7 rubric FIX (a G3.5 defect the MIDI-import shot exposed): the audio clip's gain, stretch
    // and fade controls are audio-only — with a MIDI clip in the CLIP tab they stayed laid out under
    // the MIDI rows and overlapped the loop chooser and Apply (Q). One law: they exist only when the
    // tab shows an audio clip.
    const bool audioClipControls = ! inspectorShowsQuantizePanel();
    const auto audioSectionFits = [&] (juce::Rectangle<int> section) { return audioClipControls && sectionFits (section); };
    const auto gainSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorGainSectionTop)
                                 .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainSectionHeight);
    auto gain = gainSection;
    gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlTopInset);
    inspectorGain.setBounds (audioSectionFits (gainSection)
        ? gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlHeight)
              .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainControlLeftInset)
        : juce::Rectangle<int>());
    gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorStretchControlTopGap);   // G2.9b
    inspectorStretch.setBounds (audioSectionFits (gainSection)
        ? gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlHeight)
              .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainControlLeftInset)
        : juce::Rectangle<int>());

    const auto fadesSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionTop)
                                  .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionHeight);
    auto fades = fadesSection;
    fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadesControlTopInset);
    const bool fadesFit = audioSectionFits (fadesSection);
    inspectorFadeIn.setBounds (fadesFit
        ? fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHeight)
              .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset)
              .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHorizontalInset,
                        yesdaw::ui::UiTheme::Layout::inspectorFadeControlVerticalInset)
        : juce::Rectangle<int>());
    inspectorFadeOut.setBounds (fadesFit
        ? fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHeight)
              .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset)
              .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHorizontalInset,
                        yesdaw::ui::UiTheme::Layout::inspectorFadeControlVerticalInset)
        : juce::Rectangle<int>());
    fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveControlTopGap);
    // G2.10: the Curve row carries the shape chooser (left half) and the amount slider (right half).
    juce::Rectangle<int> curveRow = fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveControlHeight)
                                         .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset);
    const juce::Rectangle<int> chooserCell = curveRow.removeFromLeft (curveRow.getWidth() / 2);
    curveRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountGap);
    inspectorFadeCurve.setBounds (fadesFit ? chooserCell : juce::Rectangle<int>());
    inspectorFadeCurveAmount.setBounds (fadesFit ? curveRow : juce::Rectangle<int>());

    // E33: the TAKES section (the old automation placeholder's area) — the chooser and
    // the delete button share its whole-section drop law.
    // G2.14: the MARKERS card below the takes — whole-section drop like every card.
    {
        auto markersSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorMarkersSectionTop)
                                  .withHeight (yesdaw::ui::UiTheme::Layout::inspectorMarkersSectionHeight);
        if (sectionFits (markersSection))
        {
            markersSection.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight);
            inspectorMarkerList.setBounds (markersSection.reduced (yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetX,
                                                                   yesdaw::ui::UiTheme::Space::none));
        }
        else
            inspectorMarkerList.setBounds ({});
        inspectorMarkerList.updateContent();
    }
    auto takesSection = area.withTrimmedTop (
        yesdaw::ui::UiTheme::Layout::inspectorAutomationSectionTop);
    const int takesNeededHeight = yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight
                                + yesdaw::ui::UiTheme::Layout::inspectorTakeRowHeight
                                + yesdaw::ui::UiTheme::Layout::inspectorTakeRowGap;
    if (sectionFits (takesSection) && takesSection.getHeight() >= takesNeededHeight)
    {
        takesSection.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight);
        takesSection.reduce (yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetX,
                             yesdaw::ui::UiTheme::Space::none);
        auto takesRow = takesSection.removeFromTop (
            yesdaw::ui::UiTheme::Layout::inspectorTakeRowHeight);
        inspectorTakeDelete.setBounds (
            takesRow.removeFromRight (yesdaw::ui::UiTheme::Layout::inspectorTakeDeleteWidth));
        takesRow.removeFromRight (yesdaw::ui::UiTheme::Layout::inspectorTakeRowGap);
        inspectorTakeChooser.setBounds (takesRow);
    }
    else
    {
        inspectorTakeChooser.setBounds ({});
        inspectorTakeDelete.setBounds ({});
    }
    // G2.1 cp2: the takes law reads these bounds — re-evaluate after layout, so a layout that
    // makes room (the settings row collapsing, the dock shrinking) shows the chooser at once.
    refreshInspectorTakesVisibility();
}

void MainComponent::refreshInspectorTakesVisibility()
{
    const bool selected = appModel.context().timelineClipSelected
                       && findProjectClipById (appModel.selectedTimelineClipId()) != nullptr;
    const bool takesVisible = selected && ! inspectorTakeViews.empty()
                           && ! inspectorTakeChooser.getBounds().isEmpty();
    inspectorTakeChooser.setVisible (takesVisible);
    inspectorTakeDelete.setVisible (takesVisible);
    inspectorTakeChooser.setEnabled (takesVisible);
    inspectorTakeDelete.setEnabled (takesVisible && inspectorTakeChooser.getSelectedId() > 0);
}

void MainComponent::refreshInspectorControls()
{
    const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId());
    const bool selected = appModel.context().timelineClipSelected && clip != nullptr;

    inspectorStart.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipMove,
                                                             appModel.context()).enabled);
    inspectorEnd.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipTrim,
                                                           appModel.context()).enabled);
    inspectorLength.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipTrim,
                                                              appModel.context()).enabled);
    inspectorGain.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetGain,
                                                            appModel.context()).enabled);
    inspectorStretch.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipTimeStretch,
                                                               appModel.context()).enabled);   // G2.9b
    inspectorFadeIn.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                              appModel.context()).enabled);
    inspectorFadeOut.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                               appModel.context()).enabled);
    inspectorFadeCurve.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                 appModel.context()).enabled);
    inspectorFadeCurveAmount.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                       appModel.context()).enabled);   // G2.10

    refreshingInspectorControls = true;
    {
        // G3.4: the quantize panel follows the context's settings; it shows only for a MIDI clip on the CLIP tab.
        const bool showQuantize = inspectorShowsQuantizePanel();
        const auto& context = appModel.context();
        inspectorQuantizeGrid.setSelectedId (context.quantizeGridChoice + 1, juce::dontSendNotification);
        inspectorQuantizeStrength.setValue (context.quantizeStrengthPercent, juce::dontSendNotification);
        inspectorQuantizeSwing.setValue (context.quantizeSwingPercent, juce::dontSendNotification);
        inspectorQuantizeEnds.setToggleState (context.quantizeNoteEnds, juce::dontSendNotification);
        inspectorQuantizeHumanize.setValue (context.quantizeHumanizePercent, juce::dontSendNotification);
        // G3.5: the clip's own rows follow the selected MIDI clip.
        if (const yesdaw::engine::MidiClip* const midiClip = appModel.selectedMidiClip())
        {
            inspectorMidiMute.setToggleState (midiClip->muted, juce::dontSendNotification);
            inspectorMidiTranspose.setValue (midiClip->transposeSemitones, juce::dontSendNotification);
            inspectorMidiVelocity.setValue (juce::roundToInt (midiClip->velocityOffset * 100.0), juce::dontSendNotification);
            const int loopChoice = appModel.midiClipLoopChoiceFor (*midiClip);
            inspectorMidiLoop.setSelectedId (loopChoice >= 0 ? loopChoice + 1 : 0, juce::dontSendNotification);
        }
        for (juce::Component* component : { static_cast<juce::Component*> (&inspectorMidiMute),
                                            static_cast<juce::Component*> (&inspectorMidiTranspose),
                                            static_cast<juce::Component*> (&inspectorMidiVelocity),
                                            static_cast<juce::Component*> (&inspectorMidiLoop),
                                            static_cast<juce::Component*> (&inspectorQuantizeGrid),
                                            static_cast<juce::Component*> (&inspectorQuantizeStrength),
                                            static_cast<juce::Component*> (&inspectorQuantizeSwing),
                                            static_cast<juce::Component*> (&inspectorQuantizeEnds),
                                            static_cast<juce::Component*> (&inspectorQuantizeHumanize),
                                            static_cast<juce::Component*> (&inspectorQuantizeApply) })
        {
            component->setVisible (showQuantize);
            component->setEnabled (showQuantize);
        }
        inspectorQuantizeApply.setEnabled (showQuantize
            && appModel.registry().stateFor (yesdaw::ui::UiActionId::PianoRollNoteQuantizeSelection, context).enabled);
    }
    if (selected && appModel.project().sampleRate.isValid())
    {
        const double sampleRate = appModel.project().sampleRate.hz;
        const double startSeconds = static_cast<double> (clip->timelineStart) / sampleRate;
        const double lengthSeconds = static_cast<double> (clip->timelineLength) / sampleRate;
        const double endSeconds = startSeconds + lengthSeconds;
        const double maxSeconds = std::max (
            yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback,
            endSeconds * yesdaw::ui::UiTheme::Layout::inspectorTimeSliderRangePaddingScale);
        setInspectorTimeSliderRange (inspectorStart, maxSeconds);
        setInspectorTimeSliderRange (inspectorEnd, maxSeconds);
        setInspectorTimeSliderRange (inspectorLength, maxSeconds);
        inspectorStart.setValue (std::clamp (startSeconds,
                                             yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                                             maxSeconds),
                                 juce::dontSendNotification);
        inspectorEnd.setValue (std::clamp (endSeconds,
                                           yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                                           maxSeconds),
                               juce::dontSendNotification);
        inspectorLength.setValue (std::clamp (lengthSeconds,
                                              yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                                              maxSeconds),
                                  juce::dontSendNotification);
        inspectorGain.setValue (clip->gain, juce::dontSendNotification);
        inspectorStretch.setValue (std::clamp (static_cast<double> (clip->stretchFactor) * 100.0,
                                               yesdaw::ui::UiTheme::Layout::inspectorStretchSliderMin,
                                               yesdaw::ui::UiTheme::Layout::inspectorStretchSliderMax),
                                   juce::dontSendNotification);   // G2.9b
        inspectorFadeIn.setValue (std::clamp (static_cast<double> (clip->fadeIn) / sampleRate,
                                              yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMinSeconds,
                                              yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMaxSeconds),
                                  juce::dontSendNotification);
        inspectorFadeOut.setValue (std::clamp (static_cast<double> (clip->fadeOut) / sampleRate,
                                               yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMinSeconds,
                                               yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMaxSeconds),
                                    juce::dontSendNotification);
        inspectorFadeCurve.setSelectedId (inspectorIdForFadeShape (clip->fadeInShape), juce::dontSendNotification);   // G2.10
        inspectorFadeCurveAmount.setValue (std::clamp (static_cast<double> (clip->fadeInCurve) * 100.0,
                                                       yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountMin,
                                                       yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountMax),
                                           juce::dontSendNotification);
    }
    else
    {
        setInspectorTimeSliderRange (inspectorStart,
                                     yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback);
        setInspectorTimeSliderRange (inspectorEnd,
                                     yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback);
        setInspectorTimeSliderRange (inspectorLength,
                                     yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback);
        inspectorStart.setValue (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderDefaultSeconds,
                                 juce::dontSendNotification);
        inspectorEnd.setValue (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderDefaultSeconds,
                               juce::dontSendNotification);
        inspectorLength.setValue (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderDefaultSeconds,
                                  juce::dontSendNotification);
        inspectorGain.setValue (yesdaw::ui::UiTheme::Layout::inspectorGainSliderDefault,
                                juce::dontSendNotification);
        inspectorFadeIn.setValue (yesdaw::ui::UiTheme::Layout::inspectorFadeSliderDefaultSeconds,
                                  juce::dontSendNotification);
        inspectorFadeOut.setValue (yesdaw::ui::UiTheme::Layout::inspectorFadeSliderDefaultSeconds,
                                   juce::dontSendNotification);
        inspectorFadeCurve.setSelectedId (kInspectorEqualPowerFadeCurveId, juce::dontSendNotification);
        inspectorFadeCurveAmount.setValue (0.0, juce::dontSendNotification);
    }

    // E33: rebuild the take stack for the selected clip's window.
    inspectorTakeViews = appModel.takesForSelectedClipWindow();
    inspectorTakeChooser.clear (juce::dontSendNotification);
    for (std::size_t view = 0; view < inspectorTakeViews.size(); ++view)
    {
        inspectorTakeChooser.addItem (
            "Take " + juce::String (inspectorTakeViews[view].takeOrdinal + 1u)
                + (inspectorTakeViews[view].audible
                       ? juce::String (juce::CharPointer_UTF8 (" \xe2\x97\x8f"))
                       : juce::String()),
            static_cast<int> (view) + 1);
        if (inspectorTakeViews[view].audible
            && inspectorTakeChooser.getSelectedId() == 0)
            inspectorTakeChooser.setSelectedId (static_cast<int> (view) + 1,
                                                juce::dontSendNotification);
    }
    const bool takesVisible = selected && ! inspectorTakeViews.empty()
                           && ! inspectorTakeChooser.getBounds().isEmpty();
    inspectorTakeChooser.setVisible (takesVisible);
    inspectorTakeDelete.setVisible (takesVisible);
    inspectorTakeChooser.setEnabled (takesVisible);
    inspectorTakeDelete.setEnabled (takesVisible && inspectorTakeChooser.getSelectedId() > 0);
    refreshingInspectorControls = false;
}

void MainComponent::setInspectorTimeSliderRange (juce::Slider& slider, double maxSeconds)
{
    slider.setRange (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                     std::max (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback, maxSeconds),
                     yesdaw::ui::UiTheme::Layout::inspectorTimeSliderIntervalSeconds);
}

std::optional<yesdaw::engine::Tick> MainComponent::inspectorTickFromSeconds (double seconds) const noexcept
{
    return timelineTickFromSeconds (seconds);
}

void MainComponent::setSelectedInspectorStartFromSlider()
{
    if (! findProjectClipById (appModel.selectedTimelineClipId()))
        return;

    if (const auto tick = inspectorTickFromSeconds (inspectorStart.getValue()))
        (void) appModel.moveSelectedTimelineClipTo (*tick);

    refreshActionState();
    repaintAll();
}

void MainComponent::setSelectedInspectorEndFromSlider()
{
    const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId());
    if (clip == nullptr)
        return;

    const std::optional<yesdaw::engine::Tick> endTick = inspectorTickFromSeconds (inspectorEnd.getValue());
    if (! endTick || *endTick <= clip->timelineStart)
        return;

    (void) appModel.trimSelectedTimelineClipRightTo (*endTick);
    refreshActionState();
    repaintAll();
}

void MainComponent::setSelectedInspectorLengthFromSlider()
{
    const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId());
    if (clip == nullptr)
        return;

    const std::optional<yesdaw::engine::Tick> lengthTick = inspectorTickFromSeconds (inspectorLength.getValue());
    if (! lengthTick || *lengthTick <= 0)
        return;

    (void) appModel.trimSelectedTimelineClipRightTo (clip->timelineStart + *lengthTick);
    refreshActionState();
    repaintAll();
}

void MainComponent::setSelectedInspectorFadesFromSliders()
{
    const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId());
    if (clip == nullptr || ! appModel.project().sampleRate.isValid())
        return;

    const double sampleRate = appModel.project().sampleRate.hz;
    const auto toTicks = [sampleRate, clip] (double seconds) {
        return std::clamp<yesdaw::engine::Tick> (
            static_cast<yesdaw::engine::Tick> (std::llround (seconds * sampleRate)),
            0,
            std::max<yesdaw::engine::Tick> (0, clip->timelineLength));
    };

    (void) appModel.setSelectedTimelineClipFades (
        toTicks (inspectorFadeIn.getValue()),
        toTicks (inspectorFadeOut.getValue()));
    refreshActionState();
    repaintAll();
}

// V7: the TRACK tab's painted content — the honest track-scoped subset that already exists
// in the model: name + N7 colour, fader/pan/mute/solo strip state, and the REAL track FX
// chain (a clip-level FX model does not exist, so the old always-"None" CLIP FX stub is
// gone; the reference's FX list maps to this real one).
// V7: the fade-chart card and its inner chart rect — ONE law shared by paint and the
// harness accessor, so a gate can cross-check the painted curve against the shared
// clipFadeCurvePoints law without re-deriving the geometry.
juce::Rectangle<int> MainComponent::inspectorFadeChartCardBounds() const
{
    auto area = inspectorBounds();
    area.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorTabHeight);
    area.reduce (yesdaw::ui::UiTheme::Layout::inspectorContentInsetX,
                 yesdaw::ui::UiTheme::Layout::inspectorContentInsetY);
    return area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFxSectionTop)
               .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFxSectionHeight);
}

juce::Rectangle<int> MainComponent::inspectorFadeChartBounds() const
{
    auto card = inspectorFadeChartCardBounds();
    card.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight);
    return card.reduced (yesdaw::ui::UiTheme::Layout::inspectorFxTextInsetX,
                         yesdaw::ui::UiTheme::Layout::inspectorFxTextInsetY);
}

void MainComponent::drawTrackInspector (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const auto& tracks = appModel.project().tracks;
    if (! appModel.context().projectLoaded || tracks.empty())
    {
        drawSmallLabel (g, "No track", area, juce::Justification::centred);
        return;
    }

    const int lane = std::clamp (selectedTrackLane, 0, static_cast<int> (tracks.size()) - 1);
    const yesdaw::engine::Track& track = tracks[static_cast<std::size_t> (lane)];

    g.setColour (colourForTrack (track, kPurple));
    g.fillRoundedRectangle (static_cast<float> (area.getX()),
                            static_cast<float> (area.getY()
                                                + yesdaw::ui::UiTheme::Layout::inspectorTitleAccentTopInset),
                            static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                            static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                            yesdaw::ui::UiTheme::Radius::sm);
    g.setColour (kText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (
        yesdaw::ui::UiTheme::Type::title,
        juce::Font::bold));
    g.drawText (track.strip.name.empty() ? "Track" : track.strip.name.c_str(),
                area.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorTitleTextLeftInset)
                    .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTitleTextHeight),
                juce::Justification::centredLeft,
                false);

    auto rows = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop);
    std::vector<juce::String> rowText;
    rowText.push_back ("Instrument");   // G3.1: the chooser + Edit ride this row's right
    rowText.push_back ("Fader   " + dbReadoutText (track.strip.linearGain));
    const int panPercent = juce::roundToInt (std::abs (track.strip.pan) * 100.0f);
    rowText.push_back ("Pan     "
                       + (panPercent == 0 ? juce::String ("C")
                                          : (track.strip.pan < 0.0f ? juce::String ("L")
                                                                    : juce::String ("R"))
                                                + juce::String (panPercent)));
    rowText.push_back (juce::String ("Mute ") + (track.strip.muted ? "on" : "off")
                       + "   Solo " + (track.strip.soloed ? "on" : "off"));
    if (track.strip.fxChain.empty())
        rowText.push_back ("Track FX: none");
    else
        for (std::size_t slot = 0; slot < track.strip.fxChain.size(); ++slot)
            rowText.push_back ("FX " + juce::String (static_cast<int> (slot) + 1) + "  "
                               + fxKindName (track.strip.fxChain[slot].kind));

    for (const juce::String& text : rowText)
    {
        auto row = rows.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeRowHeight)
                       .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetX,
                                 yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetY);
        if (! area.contains (row))
            break;
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::md);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::body));
        g.drawText (text,
                    row.reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeTextInsetX,
                                 yesdaw::ui::UiTheme::Layout::inspectorFadeTextInsetY),
                    juce::Justification::centredLeft,
                    false);
    }
}

// G3.4: the MIDI clip's inspector card — the title and the quantize panel's row labels with
// the values in force (the controls themselves are children placed by layoutInspectorControls).
void MainComponent::drawMidiClipInspector (juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour (kCyan);
    g.fillRoundedRectangle (static_cast<float> (area.getX()),
                            static_cast<float> (area.getY() + yesdaw::ui::UiTheme::Layout::inspectorTitleAccentTopInset),
                            static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                            static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                            yesdaw::ui::UiTheme::Radius::sm);
    g.setColour (kText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::title, juce::Font::bold));
    g.drawText ("MIDI Clip",
                area.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorTitleTextLeftInset)
                    .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTitleTextHeight),
                juce::Justification::centredLeft, false);

    const auto& context = appModel.context();
    const yesdaw::engine::MidiClip* const midiClip = appModel.selectedMidiClip();
    const int transpose = midiClip != nullptr ? static_cast<int> (midiClip->transposeSemitones) : 0;
    const int velocity = midiClip != nullptr ? juce::roundToInt (midiClip->velocityOffset * 100.0) : 0;
    const std::array<juce::Rectangle<int>, kMidiClipInspectorRows> rows = midiClipInspectorRows (area);
    const std::array<juce::String, kMidiClipInspectorRows> labels {
        juce::String (midiClip != nullptr && midiClip->muted ? "Mute (muted)" : "Mute"),
        "Transpose " + juce::String (transpose > 0 ? "+" : "") + juce::String (transpose) + " st",
        "Velocity " + juce::String (velocity > 0 ? "+" : "") + juce::String (velocity) + " %",
        juce::String ("Loop"),
        juce::String ("Quantize grid"),
        "Strength " + juce::String (context.quantizeStrengthPercent) + " %",
        "Swing " + juce::String (context.quantizeSwingPercent) + " %",
        juce::String ("Note ends"),
        "Humanize " + juce::String (context.quantizeHumanizePercent) + " %",
        juce::String ("Q applies to the selected notes") };
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        if (! area.contains (rows[i]))
            break;
        drawSmallLabel (g, labels[i],
                        rows[i].withTrimmedRight (i == rows.size() - 1u ? yesdaw::ui::UiTheme::Layout::inspectorQuantizeApplyWidth
                                                                        : yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth),
                        juce::Justification::centredLeft);
    }
}

void MainComponent::drawInspector (juce::Graphics& g, juce::Rectangle<int> area) const
{
    fillPanel (g, area);
    // V7: the tab strip itself is two REAL buttons (inspectorClipTab/inspectorTrackTab)
    // placed by layoutInspectorControls over this reserved band — nothing decorative to
    // paint here any more.
    area.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorTabHeight);

    area.reduce (yesdaw::ui::UiTheme::Layout::inspectorContentInsetX,
                 yesdaw::ui::UiTheme::Layout::inspectorContentInsetY);

    if (appModel.context().inspectorTrackTabActive)
    {
        drawTrackInspector (g, area);
        return;
    }

    const yesdaw::engine::Clip* const selectedClip = findProjectClipById (appModel.selectedTimelineClipId());
    if (selectedClip == nullptr)
    {
        if (inspectorShowsQuantizePanel())
        {
            drawMidiClipInspector (g, area);
            return;
        }
        drawSmallLabel (g, "No clip selected", area, juce::Justification::centred);
        return;
    }

    g.setColour (kPurple);
    g.fillRoundedRectangle (static_cast<float> (area.getX()),
                            static_cast<float> (area.getY()
                                                + yesdaw::ui::UiTheme::Layout::inspectorTitleAccentTopInset),
                            static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                            static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                            yesdaw::ui::UiTheme::Radius::sm);
    g.setColour (kText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (
        yesdaw::ui::UiTheme::Type::title,
        juce::Font::bold));
    g.drawText (selectedClip->name.c_str(),
                area.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorTitleTextLeftInset)
                    .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTitleTextHeight),
                juce::Justification::centredLeft,
                false);

    // E24/E27: the SAME whole-section drop law as layoutInspectorControls — a section
    // whose card no longer fits the column paints nothing at all.
    const juce::Rectangle<int> inspectorContent = area;
    const auto drawInspectorSectionCard =
        [&g, inspectorContent] (juce::Rectangle<int> section) -> bool
    {
        if (! inspectorContent.contains (section))
            return false;

        g.setColour (yesdaw::ui::UiTheme::Color::panelRaised());
        g.fillRoundedRectangle (section.toFloat(), yesdaw::ui::UiTheme::Radius::md);
        g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
            yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
        g.drawRoundedRectangle (
            section.toFloat().reduced (
                yesdaw::ui::UiTheme::Layout::panelOutlineInset),
            yesdaw::ui::UiTheme::Radius::md,
            yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
        return true;
    };

    auto stats = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                     .withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionHeight);
    if (inspectorContent.contains (stats))
    {
    const double selectedSampleRate = appModel.project().sampleRate.hz;
    const double selectedStartSeconds = static_cast<double> (selectedClip->timelineStart) / selectedSampleRate;
    const double selectedLengthSeconds = static_cast<double> (selectedClip->timelineLength) / selectedSampleRate;
        const std::array<std::pair<const char*, juce::String>, 3> statsText {{
            { "Start", juce::String (selectedStartSeconds, 3) + " s" },
            { "End", juce::String (selectedStartSeconds + selectedLengthSeconds, 3) + " s" },
            { "Length", juce::String (selectedLengthSeconds, 3) + " s" }
        }};
        const auto statsCells = yesdaw::ui::UiTheme::Layout::inspectorStatsCells (stats);   // G3.1 rubric FIX 1
        std::size_t statIndex = 0;
        for (const auto& [label, value] : statsText)
        {
            auto cell = statsCells[statIndex++]
                            .reduced (yesdaw::ui::UiTheme::Layout::inspectorStatsCellInsetX,
                                      yesdaw::ui::UiTheme::Layout::inspectorStatsCellInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (kMutedText);
            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::caption));
            auto textArea = cell.reduced (yesdaw::ui::UiTheme::Layout::inspectorStatsTextInset);
            g.drawText (label,
                        textArea.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorStatsLabelHeight),
                        juce::Justification::centred,
                        false);
            g.setColour (kText);
            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::body,
                juce::Font::bold));
            g.drawFittedText (value,
                              textArea.withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsValueHeight),
                              juce::Justification::centred,
                              1);
        }
    }

    auto gain = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorGainSectionTop)
                    .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainSectionHeight);
    if (drawInspectorSectionCard (gain))
    {
        drawSmallLabel (g, "GAIN", gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::title));
        const float gainValue = selectedClip->gain;
        const float gainDb = 20.0f * std::log10 (std::max (
            yesdaw::ui::UiTheme::Mixer::paintedReadoutGainFloor,
            gainValue));
        g.drawText ((gainDb >= 0.0f ? "+" : "") + juce::String (gainDb, 1) + " dB",
                    gain.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainReadoutLeftInset)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainReadoutHeight),
                    juce::Justification::centredLeft,
                    false);
    }

    auto fades = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionTop)
                     .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionHeight);
    if (drawInspectorSectionCard (fades))
    {
        drawSmallLabel (g, "FADES", fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        const double sampleRate = appModel.project().sampleRate.isValid()
                                      ? appModel.project().sampleRate.hz
                                      : yesdaw::ui::UiTheme::Layout::inspectorReadoutFallbackSampleRate;
        const double fadeInSeconds = selectedClip != nullptr
                                         ? static_cast<double> (selectedClip->fadeIn) / sampleRate
                                         : yesdaw::ui::UiTheme::Layout::inspectorFadeReadoutDefaultSeconds;
        const double fadeOutSeconds = selectedClip != nullptr
                                          ? static_cast<double> (selectedClip->fadeOut) / sampleRate
                                          : yesdaw::ui::UiTheme::Layout::inspectorFadeReadoutDefaultSeconds;
        // E24: the Curve row paints only its label — the overlaid combo IS the value display
        // (the old painted "Equal power" collided with the combo's own text).
        for (const auto& label : { juce::String ("Fade In     ") + juce::String (fadeInSeconds, 3) + " s",
                                   juce::String ("Fade Out    ") + juce::String (fadeOutSeconds, 3) + " s",
                                   juce::String ("Curve") })   // G2.10: the overlaid chooser + amount slider are the values
        {
            auto row = fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeRowHeight)
                           .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetX,
                                     yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (kText);
            g.drawText (label,
                        row.reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeTextInsetX,
                                     yesdaw::ui::UiTheme::Layout::inspectorFadeTextInsetY),
                        juce::Justification::centredLeft,
                        false);
        }
    }

    // V7: the old CLIP FX card was a hardcoded "None" stub over a model that does not exist
    // (engine::Clip has no FX chain) — removed per D3, like V2's dead KEY cell; the REAL
    // track FX chain lists on the TRACK tab. Its card now shows the clip's FADE CURVE,
    // sampling the SAME clipFadeCurvePoints law the timeline clip body paints with (V6), so
    // the two displays can never disagree about a fade's shape.
    auto fadeChart = inspectorFadeChartCardBounds();
    if (drawInspectorSectionCard (fadeChart))
    {
        drawSmallLabel (g, "FADE CURVE",
                        fadeChart.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        const auto chart = inspectorFadeChartBounds();
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (chart.toFloat(), yesdaw::ui::UiTheme::Radius::md);
        for (const bool fadeOutRegion : { false, true })
        {
            const std::vector<juce::Point<float>> points = yesdaw::ui::clipFadeCurvePoints (
                chart.toFloat().reduced (yesdaw::ui::UiTheme::Layout::panelOutlineInset),
                static_cast<long long> (selectedClip->timelineLength),
                static_cast<long long> (selectedClip->fadeIn),
                static_cast<long long> (selectedClip->fadeOut),
                fadeOutRegion,
                static_cast<int> (selectedClip->fadeInShape), selectedClip->fadeInCurve,
                static_cast<int> (selectedClip->fadeOutShape), selectedClip->fadeOutCurve);
            if (points.size() < 2u)
                continue;
            juce::Path curve;
            curve.startNewSubPath (points.front());
            for (std::size_t i = 1; i < points.size(); ++i)
                curve.lineTo (points[i]);
            g.setColour (kPurple);
            g.strokePath (curve,
                          juce::PathStrokeType (
                              yesdaw::ui::UiTheme::Layout::timelineCanvasFadeCurveStrokeWidth));
        }
    }

    // E33: the TAKES section replaced the old placeholder that ALWAYS said "No automation"
    // — the interactive chooser + delete button overlay this card; the painted text only
    // covers the honest empty case.
    {
        auto markersCard = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorMarkersSectionTop)
                               .withHeight (yesdaw::ui::UiTheme::Layout::inspectorMarkersSectionHeight);
        if (drawInspectorSectionCard (markersCard))   // G2.14
            drawSmallLabel (g, "MARKERS", markersCard.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
    }
    auto takes = area.withTrimmedTop (
        yesdaw::ui::UiTheme::Layout::inspectorAutomationSectionTop);
    if (! drawInspectorSectionCard (takes))
        return;
    drawSmallLabel (
        g,
        "TAKES",
        takes.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
    if (inspectorTakeViews.empty())
    {
        auto chart = takes.withTrimmedTop (
                              yesdaw::ui::UiTheme::Layout::inspectorAutomationChartTop)
                         .withHeight (
                             yesdaw::ui::UiTheme::Layout::inspectorAutomationChartHeight)
                         .reduced (
                             yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetX,
                             yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetY);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (chart.toFloat(), yesdaw::ui::UiTheme::Radius::md);
        drawSmallLabel (g, "No takes", chart, juce::Justification::centred);
    }
}

} // namespace yesdaw::ui
