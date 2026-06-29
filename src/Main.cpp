// YES DAW - H11 app shell.
//
// The visible JUCE shell and the headless tests share ui/UiActions.h. Buttons are only placeholders for
// the first H11 checkpoint; later slices wire Project loading, transport, timeline drawing, and editing.

#include "ui/UiActions.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <string>

namespace {

const char* panelName (yesdaw::ui::UiPanel panel)
{
    switch (panel)
    {
        case yesdaw::ui::UiPanel::Timeline: return "Timeline";
        case yesdaw::ui::UiPanel::Mixer: return "Mixer";
        case yesdaw::ui::UiPanel::PianoRoll: return "Piano Roll";
    }
    return "Timeline";
}

} // namespace

class MainComponent : public juce::Component
{
public:
    MainComponent()
    {
        setSize (1180, 720);

        title.setText ("YES DAW", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centredLeft);
        title.setColour (juce::Label::textColourId, juce::Colours::white);
        title.setFont (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)));
        addAndMakeVisible (title);

        status.setJustificationType (juce::Justification::centredRight);
        status.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (status);

        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const yesdaw::ui::UiActionId action = toolbarActions[i];
            const auto* descriptor = registry.descriptor (action);
            if (descriptor == nullptr)
                continue;

            auto& button = buttons[i];
            button.setButtonText (descriptor->label);
            button.setTooltip (descriptor->stableId);
            button.onClick = [this, action] {
                (void) registry.dispatch (action, context);
                refreshActionState();
                repaint();
            };
            addAndMakeVisible (button);
        }

        timelineLabel.setText ("Timeline", juce::dontSendNotification);
        mixerLabel.setText ("Mixer", juce::dontSendNotification);
        pianoRollLabel.setText ("Piano Roll", juce::dontSendNotification);
        for (auto* label : { &timelineLabel, &mixerLabel, &pianoRollLabel })
        {
            label->setJustificationType (juce::Justification::centred);
            label->setColour (juce::Label::textColourId, juce::Colours::white);
            addAndMakeVisible (*label);
        }

        refreshActionState();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff15171c));

        auto bounds = getLocalBounds().reduced (16);
        bounds.removeFromTop (56);

        auto timeline = bounds.removeFromTop (bounds.getHeight() * 2 / 3).reduced (0, 8);
        auto bottom = bounds.reduced (0, 8);
        auto mixer = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (0, 0);
        auto piano = bottom.reduced (8, 0);

        drawPanel (g, timeline, yesdaw::ui::UiPanel::Timeline);
        drawPanel (g, mixer, yesdaw::ui::UiPanel::Mixer);
        drawPanel (g, piano, yesdaw::ui::UiPanel::PianoRoll);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (16);
        auto top = bounds.removeFromTop (44);
        title.setBounds (top.removeFromLeft (160));
        status.setBounds (top.removeFromRight (360));

        auto toolbar = top;
        for (auto& button : buttons)
            button.setBounds (toolbar.removeFromLeft (94).reduced (4, 4));

        bounds.removeFromTop (12);
        auto timeline = bounds.removeFromTop (bounds.getHeight() * 2 / 3).reduced (12);
        auto bottom = bounds.reduced (12);
        auto mixer = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (0, 0);
        auto piano = bottom.reduced (12, 0);

        timelineLabel.setBounds (timeline);
        mixerLabel.setBounds (mixer);
        pianoRollLabel.setBounds (piano);
    }

private:
    void refreshActionState()
    {
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto state = registry.stateFor (toolbarActions[i], context);
            buttons[i].setEnabled (state.enabled);
        }

        std::string text = context.projectLoaded ? "Project loaded" : "No project";
        text += " | ";
        text += context.isPlaying ? "Playing" : "Stopped";
        text += " | ";
        text += panelName (context.activePanel);
        if (context.loopEnabled)
            text += " | Loop";
        if (context.keymapVisible)
            text += " | Keymap";
        status.setText (text, juce::dontSendNotification);
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, yesdaw::ui::UiPanel panel) const
    {
        const bool active = context.activePanel == panel;
        g.setColour (active ? juce::Colour (0xff263247) : juce::Colour (0xff20242d));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
        g.setColour (active ? juce::Colour (0xff79a8ff) : juce::Colour (0xff3a414d));
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 6.0f, active ? 2.0f : 1.0f);
    }

    yesdaw::ui::UiActionRegistry registry;
    yesdaw::ui::UiActionContext context;
    juce::Label title;
    juce::Label status;
    juce::Label timelineLabel;
    juce::Label mixerLabel;
    juce::Label pianoRollLabel;
    std::array<juce::TextButton, yesdaw::ui::kMainShellToolbarActions.size()> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

class YesDawApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "YesDaw"; }
    const juce::String getApplicationVersion() override { return "0.0.0"; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow ("YES DAW", new MainComponent(), *this));
    }

    void shutdown() override { mainWindow = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, juce::Component* content, JUCEApplication& a)
            : DocumentWindow (name, juce::Colours::darkgrey, DocumentWindow::allButtons), app (a)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { app.systemRequestedQuit(); }

    private:
        JUCEApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (YesDawApplication)
