// YES DAW — the New Project dialog (ADR-0060 cp1): sample rate, tempo, meter and template, then Create or Cancel.
//
// An overlay the shell owns: its controls are ordinary JUCE widgets the Control target walks (Tab stays inside the
// overlay while it is open, ADR-0049); no widget takes keyboard focus. The shell decides what Create does (asks for
// the bundle's location, then creates the project) through the hooks; the dialog only holds the choices.

#pragma once

#include "ui/UiAppModel.h"
#include "ui/UiTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace yesdaw::ui {

class NewProjectDialogComponent final : public juce::Component,
                                        public juce::SettableTooltipClient
{
public:
    static constexpr std::array<double, 4> kRates { 44'100.0, 48'000.0, 88'200.0, 96'000.0 };
    static constexpr std::array<std::uint16_t, 4> kDenominators { 2, 4, 8, 16 };

    juce::Label heading;
    juce::Label rateLabel;
    juce::ComboBox rate;
    juce::Label tempoLabel;
    juce::Slider tempo;
    juce::Label meterLabel;
    juce::ComboBox meterNumerator;
    juce::ComboBox meterDenominator;
    juce::Label templateLabel;
    juce::ComboBox templateChooser;
    juce::TextButton create { "Create" };
    juce::TextButton cancel { "Cancel" };

    std::function<void (UiNewProjectChoices)> onCreate;
    std::function<void()> onCancel;

    NewProjectDialogComponent()
    {
        setComponentID ("newproject.dialog");
        setTitle ("New Project");
        setTooltip ("New Project: sample rate, tempo, meter and template");
        heading.setText ("New Project", juce::dontSendNotification);
        heading.setFont (UiTheme::Type::font (UiTheme::Type::title, juce::Font::bold));
        heading.setColour (juce::Label::textColourId, UiTheme::Color::text());
        for (auto* label : { &rateLabel, &tempoLabel, &meterLabel, &templateLabel })
        {
            label->setFont (UiTheme::Type::font (UiTheme::Type::small));
            label->setColour (juce::Label::textColourId, UiTheme::Color::mutedText());
        }
        rateLabel.setText ("Sample rate", juce::dontSendNotification);
        tempoLabel.setText ("Tempo (BPM)", juce::dontSendNotification);
        meterLabel.setText ("Meter", juce::dontSendNotification);
        templateLabel.setText ("Template", juce::dontSendNotification);

        rate.setComponentID ("newproject.rate");
        rate.setTitle ("Sample rate");
        rate.setTooltip ("The project's sample rate (the audio device is asked to run at it)");
        for (std::size_t i = 0; i < kRates.size(); ++i)
            rate.addItem (juce::String (kRates[i] / 1'000.0, kRates[i] == 44'100.0 || kRates[i] == 88'200.0 ? 1 : 0) + " kHz",
                          static_cast<int> (i) + 1);

        tempo.setComponentID ("newproject.tempo");
        tempo.setTitle ("Tempo");
        tempo.setTooltip ("The project's tempo in beats per minute (20 to 300)");
        tempo.setSliderStyle (juce::Slider::LinearHorizontal);
        tempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, UiTheme::Layout::newProjectTempoTextWidth,
                               UiTheme::Layout::newProjectControlHeight);
        tempo.setRange (20.0, 300.0, 1.0);
        tempo.setWantsKeyboardFocus (false);

        meterNumerator.setComponentID ("newproject.meter.numerator");
        meterNumerator.setTitle ("Beats per bar");
        meterNumerator.setTooltip ("Beats per bar");
        for (int beats = 1; beats <= 32; ++beats)
            meterNumerator.addItem (juce::String (beats), beats);
        meterDenominator.setComponentID ("newproject.meter.denominator");
        meterDenominator.setTitle ("Beat value");
        meterDenominator.setTooltip ("The note value of one beat");
        for (std::size_t i = 0; i < kDenominators.size(); ++i)
            meterDenominator.addItem (juce::String (kDenominators[i]), static_cast<int> (i) + 1);

        templateChooser.setComponentID ("newproject.template");
        templateChooser.setTitle ("Template");
        templateChooser.setTooltip ("Start from the default (one audio track) or one of your templates");

        create.setComponentID ("newproject.create");
        create.setTitle ("Create the project");
        create.setTooltip ("Create the project (then choose where to save it)");
        cancel.setComponentID ("newproject.cancel");
        cancel.setTitle ("Cancel");
        cancel.setTooltip ("Close without creating a project (Esc)");
        for (auto* widget : { static_cast<juce::Component*> (&rate), static_cast<juce::Component*> (&meterNumerator),
                              static_cast<juce::Component*> (&meterDenominator), static_cast<juce::Component*> (&templateChooser),
                              static_cast<juce::Component*> (&create), static_cast<juce::Component*> (&cancel) })
            widget->setWantsKeyboardFocus (false);
        create.onClick = [this] {
            if (onCreate)
                onCreate (choices());
        };
        cancel.onClick = [this] {
            if (onCancel)
                onCancel();
        };
        for (juce::Component* child : { static_cast<juce::Component*> (&heading), static_cast<juce::Component*> (&rateLabel),
                                        static_cast<juce::Component*> (&rate), static_cast<juce::Component*> (&tempoLabel),
                                        static_cast<juce::Component*> (&tempo), static_cast<juce::Component*> (&meterLabel),
                                        static_cast<juce::Component*> (&meterNumerator), static_cast<juce::Component*> (&meterDenominator),
                                        static_cast<juce::Component*> (&templateLabel), static_cast<juce::Component*> (&templateChooser),
                                        static_cast<juce::Component*> (&create), static_cast<juce::Component*> (&cancel) })
            addAndMakeVisible (child);
    }

    // Fill the controls from `initial`; `templates` are the user's template names (Default is always first).
    void show (const UiNewProjectChoices& initial, const std::vector<std::string>& templates)
    {
        int rateId = 2;
        for (std::size_t i = 0; i < kRates.size(); ++i)
            if (kRates[i] == initial.sampleRateHz)
                rateId = static_cast<int> (i) + 1;
        rate.setSelectedId (rateId, juce::dontSendNotification);
        tempo.setValue (initial.bpm, juce::dontSendNotification);
        meterNumerator.setSelectedId (std::clamp<int> (initial.meterNumerator, 1, 32), juce::dontSendNotification);
        int denominatorId = 2;
        for (std::size_t i = 0; i < kDenominators.size(); ++i)
            if (kDenominators[i] == initial.meterDenominator)
                denominatorId = static_cast<int> (i) + 1;
        meterDenominator.setSelectedId (denominatorId, juce::dontSendNotification);
        templateChooser.clear (juce::dontSendNotification);
        templateChooser.addItem ("Default (one audio track)", 1);
        int templateId = 1;
        for (std::size_t i = 0; i < templates.size(); ++i)
        {
            templateChooser.addItem (juce::String::fromUTF8 (templates[i].c_str()), static_cast<int> (i) + 2);
            if (templates[i] == initial.templateName)
                templateId = static_cast<int> (i) + 2;
        }
        templateChooser.setSelectedId (templateId, juce::dontSendNotification);
        templateNames_ = templates;
        setVisible (true);
        toFront (false);
    }

    [[nodiscard]] UiNewProjectChoices choices() const
    {
        UiNewProjectChoices out;
        const int rateIndex = std::clamp (rate.getSelectedId() - 1, 0, static_cast<int> (kRates.size()) - 1);
        out.sampleRateHz = kRates[static_cast<std::size_t> (rateIndex)];
        out.bpm = std::clamp (tempo.getValue(), 20.0, 300.0);
        out.meterNumerator = static_cast<std::uint16_t> (std::clamp (meterNumerator.getSelectedId(), 1, 32));
        const int denominatorIndex = std::clamp (meterDenominator.getSelectedId() - 1, 0, static_cast<int> (kDenominators.size()) - 1);
        out.meterDenominator = kDenominators[static_cast<std::size_t> (denominatorIndex)];
        const int templateIndex = templateChooser.getSelectedId() - 2;
        if (templateIndex >= 0 && templateIndex < static_cast<int> (templateNames_.size()))
            out.templateName = templateNames_[static_cast<std::size_t> (templateIndex)];
        return out;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (UiTheme::Color::panelRaised());
        g.setColour (UiTheme::Color::panelStroke());
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        using L = UiTheme::Layout;
        auto area = getLocalBounds().reduced (L::newProjectInset);
        heading.setBounds (area.removeFromTop (L::newProjectHeadingHeight));
        area.removeFromTop (L::newProjectRowGap);
        const auto row = [&area] {
            auto r = area.removeFromTop (L::newProjectControlHeight);
            area.removeFromTop (L::newProjectRowGap);
            return r;
        };
        auto rateRow = row();
        rateLabel.setBounds (rateRow.removeFromLeft (L::newProjectLabelWidth));
        rate.setBounds (rateRow);
        auto tempoRow = row();
        tempoLabel.setBounds (tempoRow.removeFromLeft (L::newProjectLabelWidth));
        tempo.setBounds (tempoRow);
        auto meterRow = row();
        meterLabel.setBounds (meterRow.removeFromLeft (L::newProjectLabelWidth));
        const int half = (meterRow.getWidth() - L::newProjectRowGap) / 2;
        meterNumerator.setBounds (meterRow.removeFromLeft (half));
        meterRow.removeFromLeft (L::newProjectRowGap);
        meterDenominator.setBounds (meterRow);
        auto templateRow = row();
        templateLabel.setBounds (templateRow.removeFromLeft (L::newProjectLabelWidth));
        templateChooser.setBounds (templateRow);
        auto buttons = area.removeFromBottom (L::newProjectControlHeight);
        create.setBounds (buttons.removeFromRight (L::newProjectButtonWidth));
        buttons.removeFromRight (L::newProjectRowGap);
        cancel.setBounds (buttons.removeFromRight (L::newProjectButtonWidth));
    }

private:
    std::vector<std::string> templateNames_;
};

} // namespace yesdaw::ui
