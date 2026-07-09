// YES DAW - H16 CP8 mechanical UI screenshot harness.

#include "ui/MainComponent.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

using yesdaw::ui::UiActionId;
using yesdaw::ui::UiPanel;

namespace {

std::filesystem::path screenshotOutputDir()
{
    const juce::String raw = juce::SystemStats::getEnvironmentVariable ("YESDAW_UI_SCREENSHOT_DIR", {});
    if (raw.isNotEmpty())
        return std::filesystem::path (raw.toStdString());

    return std::filesystem::temp_directory_path() / "yesdaw-ui-screenshots";
}

std::uint64_t sampledNonZeroPixelCount (const juce::Image& image)
{
    std::uint64_t count = 0;
    for (int y = 0; y < image.getHeight(); y += 17)
        for (int x = 0; x < image.getWidth(); x += 19)
            if (image.getPixelAt (x, y).getARGB() != 0)
                ++count;

    return count;
}

std::uint64_t sampledDifferentPixelCount (const juce::Image& image)
{
    const auto first = image.getPixelAt (0, 0).getARGB();
    std::uint64_t count = 0;

    for (int y = 0; y < image.getHeight(); y += 17)
        for (int x = 0; x < image.getWidth(); x += 19)
            if (image.getPixelAt (x, y).getARGB() != first)
                ++count;

    return count;
}

std::uint64_t sampledArgbFingerprint (const juce::Image& image)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (int y = 0; y < image.getHeight(); y += 17)
    {
        for (int x = 0; x < image.getWidth(); x += 19)
        {
            hash ^= static_cast<std::uint64_t> (image.getPixelAt (x, y).getARGB());
            hash *= 1099511628211ull;
        }
    }

    return hash;
}

std::filesystem::path writePng (const juce::Image& image, const std::filesystem::path& outputPath)
{
    std::filesystem::create_directories (outputPath.parent_path());
    std::error_code ec;
    std::filesystem::remove (outputPath, ec);

    juce::File file (outputPath.string());
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    REQUIRE (stream != nullptr);
    REQUIRE (stream->openedOk());

    juce::PNGImageFormat format;
    REQUIRE (format.writeImageToStream (image, *stream));
    stream->flush();

    REQUIRE (std::filesystem::exists (outputPath));
    REQUIRE (std::filesystem::file_size (outputPath) > 4096u);
    return outputPath;
}

juce::Button& requireButtonForAction (juce::Component& shell, UiActionId action)
{
    juce::Component* component = yesdaw::ui::findMainComponentChildForAction (shell, action);
    REQUIRE (component != nullptr);

    auto* button = dynamic_cast<juce::Button*> (component);
    REQUIRE (button != nullptr);
    REQUIRE (button->isVisible());
    REQUIRE (button->isEnabled());
    REQUIRE (button->getWidth() > 0);
    REQUIRE (button->getHeight() > 0);
    return *button;
}

void clickButton (juce::Button& button)
{
    button.triggerClick();
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
}

std::uint64_t captureShellPng (juce::Component& shell, const char* filename)
{
    const juce::Image image = shell.createComponentSnapshot (shell.getLocalBounds(), true, 1.0f);
    REQUIRE (image.getWidth() == shell.getWidth());
    REQUIRE (image.getHeight() == shell.getHeight());
    REQUIRE (sampledNonZeroPixelCount (image) > 1000u);
    REQUIRE (sampledDifferentPixelCount (image) > 100u);

    const auto outputPath = screenshotOutputDir() / filename;
    INFO ("screenshot: " << outputPath.string());
    REQUIRE (writePng (image, outputPath) == outputPath);
    return sampledArgbFingerprint (image);
}

} // namespace

TEST_CASE ("MainComponent renders nonblank screenshot PNGs for shipped surface states", "[ui][screenshot]")
{
    juce::MessageManager::getInstance();

    auto shell = yesdaw::ui::createMainComponent();
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    REQUIRE (shell->getWidth() == yesdaw::ui::snapshotMainComponent (*shell).width);
    REQUIRE (shell->getHeight() == yesdaw::ui::snapshotMainComponent (*shell).height);
    REQUIRE (shell->getWidth() == 1536);
    REQUIRE (shell->getHeight() == 960);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::Timeline);

    const std::uint64_t timelineFingerprint = captureShellPng (*shell, "yesdaw-timeline-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::Mixer);
    const std::uint64_t mixerFingerprint = captureShellPng (*shell, "yesdaw-mixer-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::PianoRoll);
    const std::uint64_t pianoRollFingerprint = captureShellPng (*shell, "yesdaw-piano-roll-shell.png");

    REQUIRE (timelineFingerprint != mixerFingerprint);
    REQUIRE (timelineFingerprint != pianoRollFingerprint);
    REQUIRE (mixerFingerprint != pianoRollFingerprint);
}
