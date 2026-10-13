// YES DAW - JUCE application wrapper.

#include "ui/UiColourValues.h"   // ADR-0063: the window background is the theme's
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

    // ADR-0077 rule 7 (B37): every quit request - the close button, macOS Cmd+Q, a logoff - asks about unsaved changes
    // first, without a nested loop, and quits from the answer (refused while another question is open).
    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr && mainWindow->getContentComponent() != nullptr)
            yesdaw::ui::mainComponentRequestQuit (*mainWindow->getContentComponent(), [] { juce::JUCEApplicationBase::quit(); });
        else
            quit();
    }

    void initialise (const juce::String&) override
    {
        const auto probe = juce::SystemStats::getEnvironmentVariable ("YESDAW_STATE_PROBE", {});
        const auto startUnixMs = probe.isNotEmpty() ? juce::Time::currentTimeMillis() : 0;
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
        auto content = yesdaw::ui::createNativeMainComponent (std::move (openBundleAtLaunch));
        const auto contentUnixMs = probe.isNotEmpty() ? juce::Time::currentTimeMillis() : 0;
        mainWindow.reset (new MainWindow (title, content.release(), *this));
        if (probe.isNotEmpty())
            (void) juce::File (probe + ".application.tsv").replaceWithText (
                "initialiseUnixMs\t" + juce::String (startUnixMs)
                + "\ncontentReadyUnixMs\t" + juce::String (contentUnixMs)
                + "\nwindowReadyUnixMs\t" + juce::String (juce::Time::currentTimeMillis()) + "\n");
    }

    void shutdown() override
    {
        // The layout pass, step 9: the window's place and size are remembered for the next launch.
        if (mainWindow != nullptr && mainWindow->getContentComponent() != nullptr)
            yesdaw::ui::mainComponentSetWindowState (*mainWindow->getContentComponent(),
                                                     mainWindow->getWindowStateAsString().toStdString());
        mainWindow = nullptr;
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, juce::Component* content, JUCEApplication& a)
            : DocumentWindow (name, juce::Colour (yesdaw::ui::colours::appBackground), DocumentWindow::allButtons, false), app (a)
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
            // Configure the final styles before creating the native peer. Changing the title
            // bar and resizability on an existing peer recreates the Windows window repeatedly.
            addToDesktop();
            // The layout pass, step 9 (Dan, 2026-10-13): the window comes back where and as big as it was left; the
            // first launch opens maximised (the fixed default was about 40% of a 1440p screen).
            const std::string saved = yesdaw::ui::mainComponentWindowState (*content);
            if (saved.empty() || ! restoreWindowStateFromString (juce::String (saved)))
                setFullScreen (true);
            setVisible (true);
        }

        // Confirm on close (B37): edits since the last explicit Save prompt Save / Close without
        // saving / Cancel before the app quits. The bundle on disk stays current either way.
        // ADR-0077 rule 7: the close button asks as a quit request does.
        void closeButtonPressed() override
        {
            app.systemRequestedQuit();
        }

    private:
        JUCEApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (YesDawApplication)
