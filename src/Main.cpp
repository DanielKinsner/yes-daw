// YES DAW - JUCE application wrapper.

#include "ui/MainComponent.h"
#include "ui/UiTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

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

    // R9: one instance only (policy pinned by the [single-instance] gate). A second launch —
    // e.g. running tools/run-yesdaw.ps1 twice — hands its command line to THIS instance and
    // exits, instead of silently fighting over the same project bundle last-writer-wins.
    bool moreThanOneInstanceAllowed() override
    {
        return yesdaw::ui::shellAllowsMultipleInstances();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront (true);
    }

    void initialise (const juce::String&) override
    {
        // G0.1: `YesDaw.exe <path.yesdaw>` opens that bundle instead of the last-project record
        // (the Session drive's `Launch [bundle]`). Absolute paths only.
        std::filesystem::path openBundleAtLaunch;
        for (const juce::String& argument : getCommandLineParameterArray())
        {
            const juce::String trimmed = argument.unquoted().trim();
            if (trimmed.endsWithIgnoreCase (".yesdaw") && juce::File::isAbsolutePath (trimmed))
            {
                const std::string utf8 = juce::File (trimmed).getFullPathName().toStdString();
                const auto* bytes = reinterpret_cast<const char8_t*> (utf8.data());
                openBundleAtLaunch = std::filesystem::path { std::u8string (bytes, bytes + utf8.size()) };
                break;
            }
        }

        const juce::String title = "YES DAW " + getApplicationVersion();
        mainWindow.reset (new MainWindow (
            title, yesdaw::ui::createNativeMainComponent (std::move (openBundleAtLaunch)).release(), *this));
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
            // E27: the shell is resizable between the judged layout limits — the floor is the
            // smallest size every shipped panel renders honestly at ([shell-sizes] gate).
            setResizable (true, false);
            setResizeLimits (yesdaw::ui::UiTheme::Layout::windowMinWidth,
                             yesdaw::ui::UiTheme::Layout::windowMinHeight,
                             yesdaw::ui::UiTheme::Layout::windowMaxWidth,
                             yesdaw::ui::UiTheme::Layout::windowMaxHeight);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        // Confirm on close (B37): edits since the last explicit Save prompt Save / Close without
        // saving / Cancel before the app quits. The bundle on disk stays current either way.
        void closeButtonPressed() override
        {
            if (getContentComponent() == nullptr
                || yesdaw::ui::mainComponentConfirmsClose (*getContentComponent()))
                app.systemRequestedQuit();
        }

    private:
        JUCEApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (YesDawApplication)
