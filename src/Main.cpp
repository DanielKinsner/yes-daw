// YES DAW - JUCE application wrapper.

#include "ui/MainComponent.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

class YesDawApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "YesDaw"; }
    const juce::String getApplicationVersion() override { return "0.0.0"; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow ("YES DAW", yesdaw::ui::createMainComponent().release(), *this));
    }

    void shutdown() override { mainWindow = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, juce::Component* content, JUCEApplication& a)
            : DocumentWindow (name, juce::Colour (0xff0b0f14), DocumentWindow::allButtons), app (a)
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
