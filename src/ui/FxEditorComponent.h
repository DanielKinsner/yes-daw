// YES DAW — the FX editor floating panel.
//
// Plan §5.1 (the shell topology), checkpoint 1 (2026-09-05): carved verbatim out of MainComponent.cpp,
// where it sat above the shell class. The shell owns the instance and wires the std::function hooks;
// this file owns the class. Behaviour unchanged; the gate is [shell-topology] in the theme audit.

#pragma once

#include "engine/Time.h"
#include "ui/EqResponseComponent.h"
#include "ui/DelayTapsComponent.h"
#include "ui/GainReductionMeterComponent.h"
#include "ui/ReverbDecayComponent.h"
#include "ui/TransferCurveComponent.h"
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

// G4.1 cp2: the FX editor — the floating panel a filled insert slot's double-click (or its menu's
// Open Editor) opens over the arrangement, the keymap editor's placement law. It hosts the effect's
// parameter rows (the shell owns those widgets and lays them out in the content area — their ids are
// unchanged from the lane they left) under a title band naming the effect, the strip and the slot,
// with Bypass and Close. G4.2 gives each built-in its own face (EQ curve, GR meter …) in this frame.
class FxEditorComponent final : public juce::Component,
                                public juce::SettableTooltipClient
{
public:
    std::function<void()> onClose;
    std::function<void()> onBypass;
    std::function<void()> onPresets;   // G4.2 cp7: the Presets menu (ADR-0050)

    FxEditorComponent()
    {
        addChildComponent (eqResponse);
        addChildComponent (gainReduction);   // G4.2 cp2: the compressor / limiter face
        addChildComponent (transferCurve);   // G4.2 cp5: its transfer curve, above the meter
        addChildComponent (delayTaps);       // G4.2 cp3: the delay face
        addChildComponent (reverbDecay);     // G4.2 cp4: the reverb face
        setName ("FX editor");
        setComponentID ("mixer.fx.editor");
        setTooltip ("The insert's parameters: drag a row (rides Touch / Latch automation); Escape closes");
        bypassButton.setComponentID ("mixer.fx.editor.bypass");
        bypassButton.setName ("Bypass");
        bypassButton.setButtonText ("Bypass");
        bypassButton.setTooltip ("Bypass this insert (the slot's dot goes grey)");
        bypassButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        bypassButton.setColour (juce::TextButton::buttonOnColourId, yesdaw::ui::UiTheme::Color::accentPurpleDeep());
        bypassButton.setColour (juce::TextButton::textColourOffId, yesdaw::ui::UiTheme::Color::text());
        bypassButton.setColour (juce::TextButton::textColourOnId, yesdaw::ui::UiTheme::Color::text());
        bypassButton.onClick = [this] { if (onBypass) onBypass(); };
        addAndMakeVisible (bypassButton);
        closeButton.setComponentID ("mixer.fx.editor.close");
        closeButton.setName ("Close");
        closeButton.setButtonText ("Close");
        closeButton.setTooltip ("Close the editor (Escape)");
        closeButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        closeButton.setColour (juce::TextButton::textColourOffId, yesdaw::ui::UiTheme::Color::text());
        closeButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (closeButton);
        presetsButton.setComponentID ("mixer.fx.editor.presets");
        presetsButton.setName ("Presets");
        presetsButton.setButtonText ("Presets");
        presetsButton.setTooltip ("Save this setting as a preset, or load one saved for this effect");
        presetsButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        presetsButton.setColour (juce::TextButton::textColourOffId, yesdaw::ui::UiTheme::Color::text());
        presetsButton.onClick = [this] { if (onPresets) onPresets(); };
        addAndMakeVisible (presetsButton);
    }

    void setTitleText (const juce::String& text)
    {
        if (title == text)
            return;
        title = text;
        repaint();
    }
    [[nodiscard]] const juce::String& titleText() const noexcept { return title; }
    void setBypassed (bool bypassed) { bypassButton.setToggleState (bypassed, juce::dontSendNotification); }
    [[nodiscard]] bool isBypassed() const noexcept { return bypassButton.getToggleState(); }
    [[nodiscard]] juce::Component& presetsAnchor() noexcept { return presetsButton; }

    void setInsert (const engine::FxInsert& insert, double sampleRate)
    {
        const bool eq = insert.kind == engine::FxKind::Eq;
        const bool dynamics = insert.kind == engine::FxKind::Compressor || insert.kind == engine::FxKind::Limiter;
        const bool delay = insert.kind == engine::FxKind::Delay;
        const bool reverb = insert.kind == engine::FxKind::Reverb;
        if (eq) eqResponse.setInsert (insert, sampleRate);
        if (delay) delayTaps.setInsert (insert, sampleRate);
        if (reverb) reverbDecay.setInsert (insert, sampleRate);
        if (dynamics) transferCurve.setInsert (insert, sampleRate);
        if (insert.id != shownInsertId)
        {
            shownInsertId = insert.id;
            gainReduction.reset();   // another insert: nothing measured for it yet
        }
        if (eq != wantsEq || dynamics != wantsDynamics || delay != wantsDelay || reverb != wantsReverb)
        {
            wantsEq = eq;
            wantsDynamics = dynamics;
            wantsDelay = delay;
            wantsReverb = reverb;
            resized();
        }
    }
    // G4.2 cp2: one UI tick's gain-reduction sample for the shown insert (nullopt: no running node).
    void pushGainReduction (std::optional<float> gainReductionDb, bool bypassed)
    {
        gainReduction.pushReading (gainReductionDb, bypassed);
    }
    // What is on screen now: a face drops whole when the editor is too short to keep its rows (below).
    [[nodiscard]] bool showsGainReduction() const noexcept { return gainReduction.isVisible(); }
    [[nodiscard]] bool showsTransferCurve() const noexcept { return transferCurve.isVisible(); }
    [[nodiscard]] bool showsDelayTaps() const noexcept { return delayTaps.isVisible(); }
    [[nodiscard]] bool showsReverbDecay() const noexcept { return reverbDecay.isVisible(); }
    [[nodiscard]] bool showsEqResponse() const noexcept { return eqResponse.isVisible(); }
    // What the shown effect asks for (its kind), independent of the space it got.
    [[nodiscard]] bool isEqEditor() const noexcept { return wantsEq; }
    [[nodiscard]] bool hasFace() const noexcept { return wantsEq || wantsDynamics || wantsDelay || wantsReverb; }
    [[nodiscard]] const DelayTapsComponent& delayTapsFace() const noexcept { return delayTaps; }
    [[nodiscard]] const GainReductionMeterComponent& gainReductionMeter() const noexcept { return gainReduction; }
    [[nodiscard]] const TransferCurveComponent& transferCurveFace() const noexcept { return transferCurve; }
    [[nodiscard]] double eqResponseDb (double hz) const noexcept { return eqResponse.responseDb (hz); }
    [[nodiscard]] int preferredWidth() const noexcept
    {
        return wantsEq || wantsDelay || wantsReverb ? UiTheme::Layout::eqEditorMaxWidth : UiTheme::Layout::fxEditorMaxWidth;
    }
    [[nodiscard]] int preferredHeight() const noexcept
    {
        return wantsEq       ? UiTheme::Layout::eqEditorMaxHeight
             : wantsDynamics ? UiTheme::Layout::grEditorMaxHeight
             : wantsDelay    ? UiTheme::Layout::delayEditorMaxHeight
             : wantsReverb   ? UiTheme::Layout::reverbEditorMaxHeight
                             : UiTheme::Layout::fxEditorMaxHeight;
    }

    // The content area the shell lays the parameter rows into (editor-local).
    [[nodiscard]] juce::Rectangle<int> contentArea() const
    {
        auto area = bodyArea();
        area.removeFromTop (faceBandHeight (fittedFaces()));
        return area;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (yesdaw::ui::UiTheme::Color::panelRaised());
        g.setColour (yesdaw::ui::UiTheme::Color::separator());
        g.drawRect (getLocalBounds(), yesdaw::ui::UiTheme::Space::hairline);
        g.setColour (yesdaw::ui::UiTheme::Color::text());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::title, juce::Font::bold));
        g.drawFittedText (title,
                          getLocalBounds().withTrimmedLeft (yesdaw::ui::UiTheme::Layout::keymapEditorInset)
                              .withHeight (yesdaw::ui::UiTheme::Layout::keymapEditorTopRowHeight)
                              .withTrimmedRight (yesdaw::ui::UiTheme::Layout::fxEditorTitleTrimRight),
                          juce::Justification::centredLeft, 1);
    }

    void resized() override
    {
        using L = yesdaw::ui::UiTheme::Layout;
        auto top = getLocalBounds().reduced (L::keymapEditorInset).removeFromTop (L::keymapEditorTopRowHeight)
                       .reduced (yesdaw::ui::UiTheme::Space::none, L::keymapEditorSearchInsetY);
        closeButton.setBounds (top.removeFromRight (L::keymapEditorCloseWidth));
        top.removeFromRight (L::keymapEditorGap);
        bypassButton.setBounds (top.removeFromRight (L::fxEditorBypassWidth));
        top.removeFromRight (L::keymapEditorGap);
        presetsButton.setBounds (top.removeFromRight (L::fxEditorPresetsWidth));

        const Faces faces = fittedFaces();
        eqResponse.setVisible (faces.eq);
        transferCurve.setVisible (faces.curve);
        gainReduction.setVisible (faces.meter);
        delayTaps.setVisible (faces.delay);
        reverbDecay.setVisible (faces.reverb);
        auto content = bodyArea();
        const auto place = [&content] (juce::Component& face, bool shown, int height) {
            face.setBounds (shown ? content.removeFromTop (height) : juce::Rectangle<int> {});
            if (shown)
                content.removeFromTop (L::keymapEditorGap);
        };
        place (eqResponse, faces.eq, L::eqResponseHeight);
        place (transferCurve, faces.curve, L::transferCurveHeight);
        place (gainReduction, faces.meter, L::grMeterHeight);
        place (delayTaps, faces.delay, L::delayTapsHeight);
        place (reverbDecay, faces.reverb, L::reverbDecayHeight);
    }

private:
    struct Faces
    {
        bool eq = false, curve = false, meter = false, delay = false, reverb = false;
    };

    // The parameter rows are the controls; a face is their picture. A face shows only while the rows keep
    // room for fxEditorRowsKeptUnderFaces of them beneath it, and drops whole otherwise (the codebase's
    // section-fit law), least essential first: a dynamics editor keeps its meter longer than its curve.
    // A squeezed editor once laid its first row out at 0 x 0 under the faces.
    [[nodiscard]] Faces fittedFaces() const
    {
        using L = UiTheme::Layout;
        int room = bodyArea().getHeight()
                 - L::fxEditorRowsKeptUnderFaces * (L::mixerFxParamRowHeight + L::mixerFxParamRowGap);
        const auto take = [&room] (bool wanted, int height) {
            if (! wanted || room < height + L::keymapEditorGap)
                return false;
            room -= height + L::keymapEditorGap;
            return true;
        };
        Faces faces;
        faces.eq = take (wantsEq, L::eqResponseHeight);
        faces.meter = take (wantsDynamics, L::grMeterHeight);
        faces.curve = take (wantsDynamics, L::transferCurveHeight);
        faces.delay = take (wantsDelay, L::delayTapsHeight);
        faces.reverb = take (wantsReverb, L::reverbDecayHeight);
        return faces;
    }

    [[nodiscard]] static int faceBandHeight (const Faces& faces) noexcept
    {
        using L = UiTheme::Layout;
        int height = 0;
        if (faces.eq) height += L::eqResponseHeight + L::keymapEditorGap;
        if (faces.curve) height += L::transferCurveHeight + L::keymapEditorGap;
        if (faces.meter) height += L::grMeterHeight + L::keymapEditorGap;
        if (faces.delay) height += L::delayTapsHeight + L::keymapEditorGap;
        if (faces.reverb) height += L::reverbDecayHeight + L::keymapEditorGap;
        return height;
    }

    [[nodiscard]] juce::Rectangle<int> bodyArea() const
    {
        using L = UiTheme::Layout;
        auto area = getLocalBounds().reduced (L::keymapEditorInset);
        area.removeFromTop (L::keymapEditorTopRowHeight + L::keymapEditorGap);
        return area;
    }

    EqResponseComponent eqResponse;
    GainReductionMeterComponent gainReduction;
    TransferCurveComponent transferCurve;
    bool wantsEq = false, wantsDynamics = false, wantsDelay = false, wantsReverb = false;
    DelayTapsComponent delayTaps;
    ReverbDecayComponent reverbDecay;
    engine::EntityId shownInsertId {};
    juce::String title;
    juce::TextButton bypassButton, closeButton, presetsButton;
};

} // namespace yesdaw::ui
