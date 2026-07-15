// YES DAW - JUCE application wrapper.

#include "ui/MainComponent.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

// Build-time version stamp (git-describe), injected by CMake (YESDAW_GIT_VERSION) exactly as the
// YesDawSelfCheck CLI is stamped. Falls back to a dev marker for an unstamped/loose build so the
// GUI never reports the bare "0.0.0" placeholder again. (H17: kill the 0.0.0 version.)
#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "0.0.0-dev"
#endif

class YesDawApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "YesDaw"; }
    const juce::String getApplicationVersion() override { return YESDAW_VERSION_STRING; }

    void initialise (const juce::String&) override
    {
        const juce::String title = "YES DAW " + getApplicationVersion();
        mainWindow.reset (new MainWindow (title, yesdaw::ui::createMainComponent().release(), *this));
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
