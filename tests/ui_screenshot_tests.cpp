// YES DAW - H16 CP8 mechanical UI screenshot harness.

#include "ui/MainComponent.h"
#include "ui/UiIcons.h"
#include "ui/UiTheme.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <cstdint>
#include <array>
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

std::uint64_t nonTransparentPixelCount (const juce::Image& image)
{
    std::uint64_t count = 0;
    for (int y = 0; y < image.getHeight(); ++y)
        for (int x = 0; x < image.getWidth(); ++x)
            if (image.getPixelAt (x, y).getAlpha() != 0)
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

std::uint64_t differentPixelCount (const juce::Image& first,
                                   const juce::Image& second,
                                   juce::Rectangle<int> region)
{
    REQUIRE (first.getBounds() == second.getBounds());
    region = region.getIntersection (first.getBounds());
    std::uint64_t count = 0;
    for (int y = region.getY(); y < region.getBottom(); ++y)
        for (int x = region.getX(); x < region.getRight(); ++x)
            if (first.getPixelAt (x, y) != second.getPixelAt (x, y))
                ++count;
    return count;
}

std::uint64_t sampledDifferentPixelCount (const juce::Image& image, juce::Rectangle<int> region)
{
    region = region.getIntersection (image.getBounds());
    REQUIRE_FALSE (region.isEmpty());

    const auto first = image.getPixelAt (region.getX(), region.getY()).getARGB();
    std::uint64_t count = 0;
    for (int y = region.getY(); y < region.getBottom(); y += 17)
        for (int x = region.getX(); x < region.getRight(); x += 19)
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
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
}

juce::Image renderShell (juce::Component& shell)
{
    shell.repaint();
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

    juce::Image image (juce::Image::ARGB, shell.getWidth(), shell.getHeight(), true);
    {
        juce::Graphics graphics (image);
        shell.paintEntireComponent (graphics, true);
    }
    REQUIRE (image.getWidth() == shell.getWidth());
    REQUIRE (image.getHeight() == shell.getHeight());
    REQUIRE (sampledNonZeroPixelCount (image) > 1000u);
    REQUIRE (sampledDifferentPixelCount (image) > 100u);
    return image;
}

std::uint64_t captureShellPng (const juce::Image& image, const char* filename)
{

    const auto outputPath = screenshotOutputDir() / filename;
    INFO ("screenshot: " << outputPath.string());
    REQUIRE (writePng (image, outputPath) == outputPath);
    return sampledArgbFingerprint (image);
}

bool hasHeaderCoverage (const juce::Image& image)
{
    return sampledDifferentPixelCount (image, { 0, 0, 320, 88 }) > 20u
        && sampledDifferentPixelCount (image, { 320, 0, 760, 88 }) > 60u
        && sampledDifferentPixelCount (image, { 1080, 0, image.getWidth() - 1080, 88 }) > 10u;
}

void requireArrangementSurfaceCoverage (const juce::Image& image)
{
    INFO ("header coverage menu="
          << sampledDifferentPixelCount (image, { 0, 0, 320, 88 })
          << " transport="
          << sampledDifferentPixelCount (image, { 320, 0, 760, 88 })
          << " master="
          << sampledDifferentPixelCount (image, { 1080, 0, image.getWidth() - 1080, 88 }));
    REQUIRE (hasHeaderCoverage (image));
    REQUIRE (sampledDifferentPixelCount (image, { 0, 88, 318, 612 }) > 80u);
    REQUIRE (sampledDifferentPixelCount (image, { 318, 88, image.getWidth() - 638, 612 }) > 200u);
    REQUIRE (sampledDifferentPixelCount (image, { image.getWidth() - 320, 88, 320, 612 }) > 50u);
    REQUIRE (sampledDifferentPixelCount (image, { 0, image.getHeight() - 260, image.getWidth(), 260 }) > 180u);
}

bool hasMixerSurfaceCoverage (const juce::Image& image)
{
    return hasHeaderCoverage (image)
        && sampledDifferentPixelCount (image, { 0, 88, 180, image.getHeight() - 88 }) > 80u
        && sampledDifferentPixelCount (image,
                                       { 180, 88, image.getWidth() - 180, image.getHeight() - 88 })
               > 300u;
}

template <std::size_t N>
void requireDisjointActionBounds (juce::Component& shell,
                                  const std::array<UiActionId, N>& actions,
                                  juce::Rectangle<int> allowedRegion)
{
    std::array<juce::Rectangle<int>, N> bounds {};
    for (std::size_t i = 0; i < actions.size(); ++i)
    {
        juce::Component* component = yesdaw::ui::findMainComponentChildForAction (shell, actions[i]);
        REQUIRE (component != nullptr);
        bounds[i] = component->getBounds();
        REQUIRE (allowedRegion.contains (bounds[i]));
        REQUIRE (bounds[i].getWidth() >= 24);
        REQUIRE (bounds[i].getHeight() >= 24);
    }

    for (std::size_t i = 0; i < bounds.size(); ++i)
        for (std::size_t j = i + 1; j < bounds.size(); ++j)
            REQUIRE_FALSE (bounds[i].intersects (bounds[j]));
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

    requireDisjointActionBounds (
        *shell,
        std::array {
            UiActionId::ProjectNew,
            UiActionId::ProjectOpen,
            UiActionId::ProjectSave,
            UiActionId::ProjectImportAudio,
            UiActionId::ProjectExportAudio,
            UiActionId::EditUndo,
            UiActionId::EditRedo,
            UiActionId::TransportLocateStart,
            UiActionId::TransportPlay,
            UiActionId::TransportStop,
            UiActionId::TransportRecord,
            UiActionId::TransportToggleLoop
        },
        juce::Rectangle<int> { 0, 0, shell->getWidth(), yesdaw::ui::UiTheme::Layout::headerHeight });
    requireDisjointActionBounds (
        *shell,
        std::array {
            UiActionId::DeviceRefreshAudio,
            UiActionId::DeviceSelectTestAudio,
            UiActionId::RecordingArmTrack,
            UiActionId::RecordingSetMonitoringPolicy,
            UiActionId::RecordingAssembleComp
        },
        juce::Rectangle<int> { 0,
                               yesdaw::ui::UiTheme::Layout::headerHeight,
                               yesdaw::ui::UiTheme::Layout::leftRailWidth,
                               yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                   + yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset });

    const juce::Image timelineImage = renderShell (*shell);
    requireArrangementSurfaceCoverage (timelineImage);
    const std::uint64_t timelineFingerprint = captureShellPng (timelineImage, "yesdaw-timeline-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::Mixer);
    const juce::Image mixerImage = renderShell (*shell);
    REQUIRE (hasMixerSurfaceCoverage (mixerImage));
    const std::uint64_t mixerFingerprint = captureShellPng (mixerImage, "yesdaw-mixer-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::PianoRoll);
    const juce::Image pianoRollImage = renderShell (*shell);
    requireArrangementSurfaceCoverage (pianoRollImage);
    const std::uint64_t pianoRollFingerprint = captureShellPng (pianoRollImage, "yesdaw-piano-roll-shell.png");

    REQUIRE (timelineFingerprint != mixerFingerprint);
    REQUIRE (timelineFingerprint != pianoRollFingerprint);
    REQUIRE (mixerFingerprint != pianoRollFingerprint);
    const juce::Rectangle<int> headerRegion {
        0,
        0,
        shell->getWidth(),
        yesdaw::ui::UiTheme::Layout::headerHeight
    };
    REQUIRE (differentPixelCount (timelineImage, mixerImage, headerRegion) == 0u);
    REQUIRE (differentPixelCount (timelineImage, pianoRollImage, headerRegion) == 0u);
}

TEST_CASE ("H16 screenshot coverage gate rejects a blank mixer surface", "[ui][screenshot][negative]")
{
    const juce::Image blank (juce::Image::ARGB,
                             yesdaw::ui::UiTheme::Layout::defaultWindowWidth,
                             yesdaw::ui::UiTheme::Layout::defaultWindowHeight,
                             true);
    REQUIRE_FALSE (hasMixerSurfaceCoverage (blank));
}

TEST_CASE ("H16 theme fonts resolve to real typefaces on every build platform",
           "[ui][screenshot][fonts]")
{
    REQUIRE (yesdaw::ui::UiTheme::Type::font (
                 yesdaw::ui::UiTheme::Type::body).getTypefacePtr()
             != nullptr);
    REQUIRE (yesdaw::ui::UiTheme::Type::numericFont (
                 yesdaw::ui::UiTheme::Type::readout).getTypefacePtr()
             != nullptr);
}

TEST_CASE ("H16 premium vector asset set covers every shipped shell action and tool family",
           "[ui][screenshot][assets]")
{
    const juce::Rectangle<float> iconBounds {
        4.0f,
        4.0f,
        40.0f,
        40.0f
    };

    for (const UiActionId action : yesdaw::ui::mainShellToolbarActions())
    {
        INFO ("action=" << static_cast<int> (action));
        REQUIRE (yesdaw::ui::hasActionIcon (action));
        juce::Image image (juce::Image::ARGB, 48, 48, true);
        {
            juce::Graphics graphics (image);
            REQUIRE (yesdaw::ui::drawActionIcon (
                graphics,
                action,
                iconBounds,
                yesdaw::ui::UiTheme::Color::text()));
        }
        REQUIRE (nonTransparentPixelCount (image) > 8u);
    }

    for (const yesdaw::ui::TimelineTool tool : {
             yesdaw::ui::TimelineTool::Pointer,
             yesdaw::ui::TimelineTool::Pencil,
             yesdaw::ui::TimelineTool::Scissors,
             yesdaw::ui::TimelineTool::Hand,
             yesdaw::ui::TimelineTool::Zoom })
    {
        juce::Image image (juce::Image::ARGB, 48, 48, true);
        {
            juce::Graphics graphics (image);
            yesdaw::ui::drawTimelineToolIcon (
                graphics,
                tool,
                iconBounds,
                yesdaw::ui::UiTheme::Color::text());
        }
        REQUIRE (nonTransparentPixelCount (image) > 8u);
    }

    for (std::size_t track = 0; track < 8u; ++track)
    {
        juce::Image image (juce::Image::ARGB, 48, 48, true);
        {
            juce::Graphics graphics (image);
            yesdaw::ui::drawTrackGlyph (
                graphics,
                track,
                iconBounds,
                yesdaw::ui::UiTheme::Color::accentPurple());
        }
        REQUIRE (nonTransparentPixelCount (image) > 8u);
    }
}
