// YES DAW - H16 CP8 mechanical UI screenshot harness.

#include "app/SongFixture.h"
#include "engine/Project.h"
#include "io/WavFile.h"   // G5.2: the browser shot writes its own files
#include "ui/MainComponent.h"
#include "ui/UiIcons.h"
#include "ui/UiTheme.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <utility>

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

// Full-resolution variant for thin structure (the piano roll's 4 px key bars): a 17/19 px
// sampling grid can miss them entirely depending on the surface's vertical phase.
std::uint64_t fullDifferentPixelCount (const juce::Image& image, juce::Rectangle<int> region)
{
    region = region.getIntersection (image.getBounds());
    REQUIRE_FALSE (region.isEmpty());
    const auto first = image.getPixelAt (region.getX(), region.getY()).getARGB();
    std::uint64_t count = 0;
    for (int y = region.getY(); y < region.getBottom(); ++y)
        for (int x = region.getX(); x < region.getRight(); ++x)
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

// V1: theme-legibility contrast law, with the WCAG 2.x relative luminance (gamma-correct sRGB, ADR-0063). The token
// table ([tokens]) holds text to 4.5:1; this rendered net samples real pixels, where anti-aliasing lowers the measured
// peak, so its floor stays 3:1.
double relativeLuminance (juce::Colour colour) noexcept
{
    const auto channel = [] (float value) {
        const double v = value;
        return v <= 0.03928 ? v / 12.92 : std::pow ((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel (colour.getFloatRed()) + 0.7152 * channel (colour.getFloatGreen())
         + 0.0722 * channel (colour.getFloatBlue());
}

double contrastRatio (juce::Colour a, juce::Colour b) noexcept
{
    const double lighter = std::max (relativeLuminance (a), relativeLuminance (b));
    const double darker = std::min (relativeLuminance (a), relativeLuminance (b));
    return (lighter + 0.05) / (darker + 0.05);
}

// The strongest contrast between any pixel in `region` and the region's OWN top-left corner
// (assumed background — the same assumption sampledDifferentPixelCount above already makes).
// Proves real painted text achieves legible contrast against its own live-rendered panel, not a
// guessed or hardcoded background colour.
double maxContrastInRegion (const juce::Image& image, juce::Rectangle<int> region)
{
    region = region.getIntersection (image.getBounds());
    REQUIRE_FALSE (region.isEmpty());
    const juce::Colour background = image.getPixelAt (region.getX(), region.getY());
    double best = 0.0;
    for (int y = region.getY(); y < region.getBottom(); ++y)
        for (int x = region.getX(); x < region.getRight(); ++x)
            best = std::max (best, contrastRatio (image.getPixelAt (x, y), background));
    return best;
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
    // G0.7: the device/recording cluster lives in the collapsible settings row — a test that
    // uses one of those buttons shows the row first, through the real toggle action.
    yesdaw::ui::mainComponentRevealSettingsRowFor (shell, action);
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

// M4: the mixer capture needs the FX chooser by id — same recursive walk the input harness uses.
juce::Component* findChildWithComponentId (juce::Component& component, const juce::String& componentId)
{
    if (component.getComponentID() == componentId)
        return &component;

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (juce::Component* child = component.getChildComponent (i))
            if (juce::Component* found = findChildWithComponentId (*child, componentId))
                return found;

    return nullptr;
}

// M4: click a painted mixer strip so the capture can seed a real FX chain on it.
void mouseDownAtPoint (juce::Component& component, juce::Point<int> position)
{
    const juce::Time now = juce::Time::getCurrentTime();
    juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(),
                            position.toFloat(),
                            juce::ModifierKeys::leftButtonModifier,
                            juce::MouseInputSource::defaultPressure,
                            juce::MouseInputSource::defaultOrientation,
                            juce::MouseInputSource::defaultRotation,
                            juce::MouseInputSource::defaultTiltX,
                            juce::MouseInputSource::defaultTiltY,
                            &component,
                            &component,
                            now,
                            position.toFloat(),
                            now,
                            1,
                            false);
    component.mouseDown (event);
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
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

// G0.7: the three header sections come from the shell's layout law (no fixed x); each is
// sampled just inside its top-left corner, where no control sits.
bool hasHeaderSectionHierarchy (const juce::Image& image, const juce::Component& shell)
{
    const auto sectionFill = yesdaw::ui::UiTheme::Color::controlInset();
    for (int section = 0; section < 3; ++section)
    {
        const juce::Rectangle<int> bounds = yesdaw::ui::mainComponentHeaderSectionBounds (shell, section);
        // Top edge, horizontal centre: inside the rounded fill, below the outline, above every control.
        if (bounds.isEmpty() || image.getPixelAt (bounds.getCentreX(), bounds.getY() + 3) != sectionFill)
            return false;
    }
    return true;
}

bool hasTrackMixSummaryCoverage (const juce::Image& image)
{
    const auto summaryFill = yesdaw::ui::UiTheme::Color::controlInset();
    return image.getPixelAt (220, 190) == summaryFill
        && image.getPixelAt (220, 631) == summaryFill;
}

bool hasInspectorSectionHierarchy (const juce::Image& image)
{
    const auto sectionFill = yesdaw::ui::UiTheme::Color::panelRaised();
    return image.getPixelAt (1244, 273) == sectionFill
        && image.getPixelAt (1244, 369) == sectionFill
        && image.getPixelAt (1244, 515) == sectionFill
        && image.getPixelAt (1244, 621) == sectionFill;
}

void requireHonestEmptyArrangementCoverage (const juce::Image& image, const juce::Component& shell)
{
    REQUIRE (hasHeaderCoverage (image));
    REQUIRE (hasHeaderSectionHierarchy (image, shell));
    using L = yesdaw::ui::UiTheme::Layout;
    const int work = image.getHeight() - L::headerHeight - L::mixerHeight;
    REQUIRE (sampledDifferentPixelCount (image, { 0, L::headerHeight, L::leftRailWidth, work }) > 20u);
    REQUIRE (sampledDifferentPixelCount (image, { L::leftRailWidth, L::headerHeight,
                                                  image.getWidth() - L::leftRailWidth - L::inspectorWidth, work }) > 100u);
    REQUIRE (sampledDifferentPixelCount (image, { image.getWidth() - L::inspectorWidth, L::headerHeight, L::inspectorWidth, work }) > 10u);
    REQUIRE (sampledDifferentPixelCount (image, { 0, image.getHeight() - L::mixerHeight, image.getWidth(), L::mixerHeight }) > 40u);
}

bool hasMixerSurfaceCoverage (const juce::Image& image)
{
    return hasHeaderCoverage (image)
        && sampledDifferentPixelCount (image, { 0, 88, 180, image.getHeight() - 88 }) > 80u
        && sampledDifferentPixelCount (image,
                                       { 180, 88, image.getWidth() - 180, image.getHeight() - 88 })
               > 300u;
}

// N3: master no longer pins to the window's right edge — it is the strip immediately after
// the last track/bus strip (0 strips here, so master paints at the FIRST lane, right after the
// tools column). The caller passes the real painted master rect (mainComponentPaintedMixerMasterBounds)
// instead of a hardcoded right-edge guess, so this can never drift from where master actually paints.
bool hasMixerMasterSummaryCoverage (const juce::Image& image, juce::Rectangle<int> masterRegion)
{
    return sampledDifferentPixelCount (image, masterRegion) > 20u;
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

    auto shell = yesdaw::ui::createMainComponent (yesdaw::ui::MainComponentFileChoices {});
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    REQUIRE (shell->getWidth() == yesdaw::ui::snapshotMainComponent (*shell).width);
    REQUIRE (shell->getHeight() == yesdaw::ui::snapshotMainComponent (*shell).height);
    REQUIRE (shell->getWidth() == 1536);
    REQUIRE (shell->getHeight() == 960);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::Timeline);
    const yesdaw::ui::MainComponentSnapshot startup = yesdaw::ui::snapshotMainComponent (*shell);
    REQUIRE_FALSE (startup.context.projectLoaded);
    REQUIRE (startup.visibleTimelineTrackCount == 0);
    REQUIRE (startup.visibleTimelineClipCount == 0);
    REQUIRE (startup.visibleMixerTrackCount == 0);
    REQUIRE (startup.visibleMixerBusCount == 0);
    REQUIRE_FALSE (startup.visibleMixerLoudnessValid);
    REQUIRE (startup.visibleMasterPeakLeft == 0.0f);
    REQUIRE (startup.visibleMasterPeakRight == 0.0f);
    REQUIRE (startup.visiblePianoRollNoteCount == 0);

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
    // G0.7: the device + recording cluster lives in the collapsible settings row under the
    // toolbar — shown, its buttons are disjoint and inside the (taller) header; then hidden again.
    yesdaw::ui::mainComponentSetSettingsRowVisible (*shell, true);
    requireDisjointActionBounds (
        *shell,
        std::array {
            UiActionId::RecordingArmTrack,
            UiActionId::RecordingSetMonitoringPolicy
        },
        juce::Rectangle<int> { 0, 0, shell->getWidth(), yesdaw::ui::mainComponentHeaderHeight (*shell) });
    yesdaw::ui::mainComponentSetSettingsRowVisible (*shell, false);

    const juce::Image timelineImage = renderShell (*shell);
    requireHonestEmptyArrangementCoverage (timelineImage, *shell);
    const std::uint64_t timelineFingerprint = captureShellPng (timelineImage, "yesdaw-timeline-shell.png");

    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ViewMixer);   // G2.1 cp3: the menu verb (the cluster's X toggles the dock)
    yesdaw::ui::mainComponentSetDockHeight (*shell, yesdaw::ui::UiTheme::Layout::windowMaxHeight);   // G2.1 cp2: a dock tab — grow it for the full lane
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::Mixer);
    const juce::Rectangle<int> masterRegion = yesdaw::ui::mainComponentPaintedMixerMasterBounds (*shell);
    REQUIRE_FALSE (masterRegion.isEmpty());
    const juce::Image mixerImage = renderShell (*shell);
    REQUIRE (hasMixerSurfaceCoverage (mixerImage));
    REQUIRE (hasMixerMasterSummaryCoverage (mixerImage, masterRegion));
    const std::uint64_t mixerFingerprint = captureShellPng (mixerImage, "yesdaw-mixer-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    yesdaw::ui::mainComponentSetDockHeight (*shell, yesdaw::ui::UiTheme::Layout::windowMaxHeight);   // G2.1 cp2: a dock tab — grow it for the full lane
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::PianoRoll);
    const juce::Image pianoRollImage = renderShell (*shell);
    requireHonestEmptyArrangementCoverage (pianoRollImage, *shell);
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

TEST_CASE ("Timeline renders honestly at laptop, default, and large window sizes with real content",
           "[ui][screenshot][timeline-sizes]")
{
    juce::MessageManager::getInstance();

    const std::filesystem::path bundlePath =
        std::filesystem::temp_directory_path() / "yesdaw-ui-screenshot-timeline-sizes.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);

    // Real content through real controls: two audio tracks with clips, a third track with a
    // MIDI clip, and a marker — the E24 judging fixture. Each import/clip lands on ITS track
    // via a real rail row click.
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    juce::Component* rail = nullptr;
    for (int child = 0; child < shell->getNumChildComponents(); ++child)
        if (shell->getChildComponent (child)->getComponentID() == "shell.tracklist.input")
            rail = shell->getChildComponent (child);
    REQUIRE (rail != nullptr);
    const auto selectRailRow = [&shell, rail] (int row, int rowCount)
    {
        (void) rowCount;
        const int rowHeight = yesdaw::ui::UiTheme::Layout::trackListRowMinHeight;   // G0.7: fixed rows
        const juce::Point<int> point { yesdaw::ui::UiTheme::Layout::trackListNameLeftInset + 8,
                                       yesdaw::ui::UiTheme::Layout::trackListHeaderHeight
                                           + row * rowHeight + rowHeight / 2 };
        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(),
                                      point.toFloat(), juce::ModifierKeys::leftButtonModifier,
                                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      rail, rail, juce::Time::getCurrentTime(),
                                      point.toFloat(), juce::Time::getCurrentTime(), 1, false);
        rail->mouseDown (event);
        (void) shell;
    };
    REQUIRE (shell->keyPressed (juce::KeyPress ('n', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    selectRailRow (1, 2);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('n', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));
    selectRailRow (2, 3);
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::TimelineMidiClipAdd);   // G1.1: no default chord
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ViewTimeline);   // G1.1: no default chord   // back to the Timeline view
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));   // marker at the playhead

    const yesdaw::ui::MainComponentSnapshot content = yesdaw::ui::snapshotMainComponent (*shell);
    REQUIRE (content.context.projectLoaded);
    REQUIRE (content.visibleTimelineTrackCount == 3);
    REQUIRE (content.visibleTimelineClipCount >= 2);
    REQUIRE (content.context.activePanel == UiPanel::Timeline);

    // N7/CP-A evidence: give each track a DIFFERENT colour (row N gets N+1 swatch clicks) so
    // this screenshot actually shows the colourised-arrangement surface, not the historical
    // uniform purple.
    for (int row = 0; row < 3; ++row)
    {
        const juce::Rectangle<int> swatch = yesdaw::ui::mainComponentPaintedColourSwatchBounds (*shell, row);
        REQUIRE_FALSE (swatch.isEmpty());
        for (int click = 0; click <= row; ++click)
            mouseDownAtPoint (*rail, swatch.getCentre() - rail->getPosition());
    }

    const auto renderAtSize = [&shell] (int width, int height, const char* filename)
    {
        shell->setSize (width, height);
        const juce::Image image = renderShell (*shell);
        REQUIRE (image.getWidth() == width);
        REQUIRE (image.getHeight() == height);
        // Size-relative honesty: the header row, the rail column, the arrangement body, and
        // the bottom section all paint real structure at EVERY size.
        REQUIRE (sampledDifferentPixelCount (image, { 0, 0, width, 88 }) > 60u);
        {
            using L = yesdaw::ui::UiTheme::Layout;
            const int work = height - L::headerHeight - L::mixerHeight;
            REQUIRE (sampledDifferentPixelCount (image, { 0, L::headerHeight, L::leftRailWidth, work }) > 20u);
            REQUIRE (sampledDifferentPixelCount (image, { L::leftRailWidth, L::headerHeight,
                                                          width - L::leftRailWidth - L::inspectorWidth, work }) > 100u);
            REQUIRE (sampledDifferentPixelCount (image, { 0, height - L::mixerHeight, width, L::mixerHeight }) > 40u);
        }
        // E24: NO inspector control may bleed into the bottom mixer panel — small windows drop
        // the sections that no longer fit instead of overlapping.
        const int bottomPanelTop = height - yesdaw::ui::UiTheme::Layout::mixerHeight;
        for (const char* id : { "clip.inspector.start", "clip.inspector.fade_curve" })
        {
            juce::Component* control = nullptr;
            for (int child = 0; child < shell->getNumChildComponents(); ++child)
                if (shell->getChildComponent (child)->getComponentID() == id)
                    control = shell->getChildComponent (child);
            REQUIRE (control != nullptr);
            INFO ("control " << id << " bounds " << control->getBounds().toString().toStdString());
            REQUIRE ((control->getBounds().isEmpty()
                      || control->getBounds().getBottom() <= bottomPanelTop));
        }
        (void) captureShellPng (image, filename);
    };

    renderAtSize (1152, 720, "yesdaw-timeline-laptop.png");
    renderAtSize (1536, 960, "yesdaw-timeline-default.png");
    renderAtSize (1920, 1080, "yesdaw-timeline-large.png");

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Mixer renders honestly at laptop, default, and large window sizes with real strips",
           "[ui][screenshot][mixer-sizes]")
{
    juce::MessageManager::getInstance();

    const std::filesystem::path bundlePath =
        std::filesystem::temp_directory_path() / "yesdaw-ui-screenshot-mixer-sizes.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    juce::KeyPress addTrack ('t', juce::ModifierKeys::ctrlModifier, 0);
    REQUIRE (shell->keyPressed (addTrack));
    REQUIRE (shell->keyPressed (addTrack));
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ViewMixer);   // G2.1 cp3: the menu verb (the cluster's X toggles the dock)
    yesdaw::ui::mainComponentSetDockHeight (*shell, yesdaw::ui::UiTheme::Layout::windowMaxHeight);   // G2.1 cp2: a dock tab — grow it for the full lane

    // M4: the strips paint their FX chains, so the mixer capture carries a REAL chain — an empty
    // mixer would hide the very thing these screenshots are for.
    {
        auto* strips = findChildWithComponentId (*shell, "shell.mixer.strips.input");
        REQUIRE (strips != nullptr);
        mouseDownAtPoint (*strips, { strips->getWidth() / 8, strips->getHeight() / 2 });
        // G4.1 cp2: the lane's chooser is gone — the strip header's menu (Add Insert ▸) adds the EQ.
        const juce::Rectangle<int> lane = yesdaw::ui::mainComponentPaintedMixerStripBounds (*shell, 0);
        REQUIRE_FALSE (lane.isEmpty());
        for (const yesdaw::engine::FxKind kind : { yesdaw::engine::FxKind::Eq, yesdaw::engine::FxKind::Compressor,
                                                    yesdaw::engine::FxKind::Limiter })
        {
            (void) yesdaw::ui::mainComponentRequestContextMenu (
                *shell, lane.withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedHeaderHeight).getCentre());
            yesdaw::ui::mainComponentInvokeContextMenuItem (*shell, yesdaw::ui::UiActionId::MixerFxInsertAdd,
                                                            static_cast<int> (kind));
        }
    }

    const auto renderAtSize = [&shell] (int width, int height, const char* filename)
    {
        shell->setSize (width, height);
        const juce::Image image = renderShell (*shell);
        REQUIRE (hasMixerSurfaceCoverage (image));
        (void) captureShellPng (image, filename);
    };

    renderAtSize (1152, 720, "yesdaw-mixer-laptop.png");
    renderAtSize (1536, 960, "yesdaw-mixer-default.png");
    renderAtSize (1920, 1080, "yesdaw-mixer-large.png");

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Piano roll and automation lane render honestly with real notes and breakpoints",
           "[ui][screenshot][roll-sizes]")
{
    juce::MessageManager::getInstance();

    const std::filesystem::path bundlePath =
        std::filesystem::temp_directory_path() / "yesdaw-ui-screenshot-roll-sizes.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);

    const auto findChildById = [&shell] (const char* id) -> juce::Component*
    {
        for (int child = 0; child < shell->getNumChildComponents(); ++child)
            if (shell->getChildComponent (child)->getComponentID() == id)
                return shell->getChildComponent (child);
        return nullptr;
    };
    const auto mouseDownUpAt = [] (juce::Component& component, juce::Point<int> point)
    {
        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(),
                                      point.toFloat(), juce::ModifierKeys::leftButtonModifier,
                                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      &component, &component, juce::Time::getCurrentTime(),
                                      point.toFloat(), juce::Time::getCurrentTime(), 1, false);
        component.mouseDown (event);
        component.mouseUp (event);
    };

    // Real content through real controls: an audio track with a clip, plus a MIDI clip
    // pencilled with a phrase of notes across the key range.
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::TimelineMidiClipAdd);   // G1.1: no default chord

    const yesdaw::ui::MainComponentSnapshot opened = yesdaw::ui::snapshotMainComponent (*shell);
    REQUIRE (opened.context.projectLoaded);
    REQUIRE (opened.context.activePanel == UiPanel::PianoRoll);
    REQUIRE (opened.context.midiClipSelected);

    juce::Component* pianoRoll = findChildById ("piano-roll.canvas");
    REQUIRE (pianoRoll != nullptr);
    const auto pencilGrid = [&] ()
    {
        // The shipped grid inset chain (header 38, frame 12/8, expression 84, keys 70).
        auto grid = pianoRoll->getLocalBounds();
        grid.removeFromTop (38);
        grid.reduce (12, 8);
        grid.removeFromBottom (84);
        grid.removeFromLeft (70);
        return grid.reduced (0, 2);
    };
    REQUIRE (shell->keyPressed (juce::KeyPress ('2')));
    const juce::Rectangle<int> grid = pencilGrid();
    for (const auto& [fx, fy] : { std::pair { 0.08, 0.62 }, { 0.22, 0.55 }, { 0.36, 0.48 },
                                  { 0.52, 0.55 }, { 0.68, 0.42 }, { 0.84, 0.35 } })
        mouseDownUpAt (*pianoRoll,
                       { grid.getX() + juce::roundToInt (grid.getWidth() * fx),
                         grid.getY() + juce::roundToInt (grid.getHeight() * fy) });
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));

    const auto renderRollAtSize = [&] (int width, int height, const char* filename)
    {
        shell->setSize (width, height);
        const juce::Image image = renderShell (*shell);
        juce::Component* canvas = findChildById ("piano-roll.canvas");
        REQUIRE (canvas != nullptr);
        const juce::Rectangle<int> bounds = canvas->getBounds();
        // Size-relative honesty: the key column, the note grid, and the expression lane all
        // paint real structure at every size.
        auto local = bounds;
        local.removeFromTop (38);
        local.reduce (12, 8);
        const auto expression = local.removeFromBottom (84);
        const auto keys = local.removeFromLeft (70);
        // The PNG is written BEFORE the judgments so a red leaves its evidence.
        (void) captureShellPng (image, filename);
        INFO ("keys " << keys.toString().toStdString() << " canvas " << bounds.toString().toStdString());
        REQUIRE (fullDifferentPixelCount (image, keys) > 400u);   // G0.7: full scan (4 px key bars)
        REQUIRE (sampledDifferentPixelCount (image, local) > 60u);
        REQUIRE (sampledDifferentPixelCount (image, expression) > 10u);
    };

    renderRollAtSize (1152, 720, "yesdaw-roll-laptop.png");
    renderRollAtSize (1536, 960, "yesdaw-roll-default.png");
    renderRollAtSize (1920, 1080, "yesdaw-roll-large.png");

    // M8: the key column is a KEYBOARD and the velocity lane is BARS.
    {
        shell->setSize (1536, 960);
        const juce::Image image = renderShell (*shell);
        juce::Component* canvas = findChildById ("piano-roll.canvas");
        REQUIRE (canvas != nullptr);
        auto local = canvas->getBounds();
        local.removeFromTop (38);
        local.reduce (12, 8);
        const auto expression = local.removeFromBottom (84);
        const auto keys = local.removeFromLeft (70);

        // White keys are genuinely light and black keys genuinely dark — before M8 the whole
        // column was painted in the panel's raised grey, so it had no light pixels at all.
        int lightKeyPixels = 0;
        int darkKeyPixels = 0;
        for (int y = keys.getY(); y < keys.getBottom(); ++y)
            for (int x = keys.getX(); x < keys.getRight(); ++x)
            {
                const float brightness = image.getPixelAt (x, y).getBrightness();
                if (brightness > 0.70f)
                    ++lightKeyPixels;
                else if (brightness < 0.12f)
                    ++darkKeyPixels;
            }
        REQUIRE (lightKeyPixels > 400);
        REQUIRE (darkKeyPixels > 200);

        // Velocity paints one bar per note: the lane's painted columns are ISOLATED, not a
        // continuous stroke joining every note (which is what the old line graph drew).
        const auto velocityLane = expression.reduced (0, 6).removeFromTop (36).reduced (0, 2);
        int paintedColumns = 0;
        int longestRun = 0;
        int run = 0;
        for (int x = velocityLane.getX(); x < velocityLane.getRight(); ++x)
        {
            bool painted = false;
            for (int y = velocityLane.getY(); y < velocityLane.getBottom() && ! painted; ++y)
            {
                const juce::Colour pixel = image.getPixelAt (x, y);
                painted = pixel.getGreen() > 140 && pixel.getRed() < 140;
            }

            if (painted)
            {
                ++paintedColumns;
                longestRun = std::max (longestRun, ++run);
            }
            else
            {
                run = 0;
            }
        }
        REQUIRE (paintedColumns > 0);
        REQUIRE (paintedColumns < velocityLane.getWidth() / 4);   // bars, not a joined line
        REQUIRE (longestRun <= 8);                                // no stroke spanning the lane
    }

    // The automation lane, open on the timeline with real breakpoints clicked into the canvas.
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ViewTimeline);   // G1.1: no default chord
    clickButton (requireButtonForAction (*shell, UiActionId::TimelineAutomationToggleTrackLane));
    const yesdaw::ui::MainComponentSnapshot laneOpen = yesdaw::ui::snapshotMainComponent (*shell);
    REQUIRE (laneOpen.context.timelineAutomationTrackLaneVisible);

    juce::Component* automationCanvas = findChildById ("timeline.automation.canvas");
    REQUIRE (automationCanvas != nullptr);
    REQUIRE (automationCanvas->isVisible());
    for (const auto& [fx, fy] : { std::pair { 0.15, 0.75 }, { 0.45, 0.25 }, { 0.8, 0.55 } })
        mouseDownUpAt (*automationCanvas,
                       { juce::roundToInt (automationCanvas->getWidth() * fx),
                         juce::roundToInt (automationCanvas->getHeight() * fy) });

    // Clicked breakpoints are REAL: the delete-breakpoint action only arms once the target
    // lane holds points.
    {
        juce::Component* deleteButton = yesdaw::ui::findMainComponentChildForAction (
            *shell, UiActionId::TimelineAutomationDeleteBreakpoint);
        REQUIRE (deleteButton != nullptr);
        REQUIRE (deleteButton->isEnabled());
    }

    shell->setSize (1536, 960);
    const juce::Image automationImage = renderShell (*shell);
    juce::Component* canvasAfter = findChildById ("timeline.automation.canvas");
    REQUIRE (canvasAfter != nullptr);
    // Dense scan: the lane's curve line and handles are thin — count every pixel that differs
    // from the lane background instead of the sparse stride.
    {
        const juce::Rectangle<int> lane =
            canvasAfter->getBounds().getIntersection (automationImage.getBounds());
        REQUIRE_FALSE (lane.isEmpty());
        const auto background = automationImage.getPixelAt (lane.getX(), lane.getY()).getARGB();
        std::uint64_t structure = 0;
        for (int y = lane.getY(); y < lane.getBottom(); ++y)
            for (int x = lane.getX(); x < lane.getRight(); ++x)
                if (automationImage.getPixelAt (x, y).getARGB() != background)
                    ++structure;
        REQUIRE (structure > 400u);
    }
    (void) captureShellPng (automationImage, "yesdaw-automation-default.png");

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

namespace {

// ADR-0064 cp2: one cell of the scaling matrix — a logical window size and the scale it is rasterised at.
struct ScalingCell
{
    int width = 0;
    int height = 0;
    float scale = 1.0f;
    juce::String kind;
};

// The plan's cells (each window size at each Windows scale) and the display cells (the maximized client of each of
// the plan's displays at each scale whose client the window minimum fits: floor(W/s) x floor((H - 48)/s - 32), a
// 48 px physical taskbar and a 32 px logical title bar).
std::vector<ScalingCell> scalingMatrix()
{
    using L = yesdaw::ui::UiTheme::Layout;
    std::vector<ScalingCell> cells;
    constexpr std::array<std::pair<int, int>, 3> kSizes {{ { 1280, 720 }, { 1920, 1080 }, { 2560, 1440 } }};
    constexpr std::array<float, 4> kScales {{ 1.0f, 1.25f, 1.5f, 2.0f }};
    for (const auto& [w, h] : kSizes)
        for (const float scale : kScales)
            cells.push_back ({ w, h, scale, "window" });
    for (const auto& [w, h] : kSizes)
        for (const float scale : kScales)
        {
            const int clientWidth = static_cast<int> (std::floor (static_cast<float> (w) / scale));
            const int clientHeight = static_cast<int> (std::floor (static_cast<float> (h - 48) / scale - 32.0f));
            if (clientWidth >= L::windowMinWidth && clientHeight >= L::windowMinHeight)
                cells.push_back ({ clientWidth, clientHeight, scale, "display" });
        }
    return cells;
}

juce::Image renderShellAtScale (juce::Component& shell, float scale)
{
    shell.repaint();
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    const int width = static_cast<int> (std::ceil (static_cast<float> (shell.getWidth()) * scale));
    const int height = static_cast<int> (std::ceil (static_cast<float> (shell.getHeight()) * scale));
    // JUCE's own software rasteriser, not the platform image type (Direct2D on Windows): the same pixels on a GPU-less
    // CI runner as on a desktop, so the raster bounds below hold everywhere they run.
    juce::Image image (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    {
        juce::Graphics graphics (image);
        graphics.addTransform (juce::AffineTransform::scale (scale));
        shell.paintEntireComponent (graphics, true);
    }
    return image;
}

// Mean absolute channel difference (0..255) of two same-size images over a region (the raster gates run on Windows).
[[maybe_unused]] double meanAbsDifference (const juce::Image& a, const juce::Image& b, juce::Rectangle<int> region)
{
    region = region.getIntersection (a.getBounds()).getIntersection (b.getBounds());
    if (region.isEmpty())
        return 0.0;
    const juce::Image::BitmapData da (a, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData db (b, juce::Image::BitmapData::readOnly);
    double total = 0.0;
    for (int y = region.getY(); y < region.getBottom(); ++y)
        for (int x = region.getX(); x < region.getRight(); ++x)
        {
            const juce::Colour ca = da.getPixelColour (x, y);
            const juce::Colour cb = db.getPixelColour (x, y);
            total += std::abs (static_cast<int> (ca.getRed()) - static_cast<int> (cb.getRed()))
                   + std::abs (static_cast<int> (ca.getGreen()) - static_cast<int> (cb.getGreen()))
                   + std::abs (static_cast<int> (ca.getBlue()) - static_cast<int> (cb.getBlue()));
        }
    return total / (3.0 * static_cast<double> (region.getWidth()) * static_cast<double> (region.getHeight()));
}

bool matrixReport() { return juce::SystemStats::getEnvironmentVariable ("YESDAW_MATRIX_REPORT", {}).isNotEmpty(); }

} // namespace

// ADR-0064 cp2: every cell of the scaling matrix — the logical size's geometry and reachability, and the shell
// rendered at the cell's scale.
TEST_CASE ("ADR-0064 the scaling matrix: geometry, reachable controls and renders at every cell",
           "[ui][screenshot][layout][scaling]")
{
    using L = yesdaw::ui::UiTheme::Layout;
    juce::MessageManager::getInstance();
    const std::vector<ScalingCell> cells = scalingMatrix();
    REQUIRE (std::count_if (cells.begin(), cells.end(), [] (const ScalingCell& c) { return c.kind == "display"; }) == 8);

    const std::filesystem::path fixtureDir = std::filesystem::temp_directory_path() / "yesdaw-g62-matrix-fixture";
    {
        std::error_code ec;
        std::filesystem::remove_all (fixtureDir, ec);
    }
    yesdaw::app::fixture::SongFixtureSpec spec;
    spec.tracks = 16;
    spec.seconds = 6.0;
    spec.sampleRateHz = 48000;
    spec.channels = 2;
    spec.midiTracks = 4;
    const yesdaw::app::fixture::SongFixtureResult fixture = yesdaw::app::fixture::buildSongFixture (fixtureDir, spec);
    INFO (fixture.error);
    REQUIRE (fixture.ok);
    yesdaw::ui::MainComponentFileChoices choices;
    const std::filesystem::path bundlePath = fixture.bundlePath;
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    shell->setSize (L::defaultWindowWidth, L::defaultWindowHeight);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.projectLoaded);
    // A clip selected, so the inspector's clip sections are part of what must stay reachable.
    {
        const juce::var layout = juce::JSON::parse (yesdaw::ui::mainComponentStateProbeJson (*shell)).getProperty ("layout", {});
        juce::Rectangle<int> clip;
        if (auto* object = layout.getDynamicObject())
            for (const auto& property : object->getProperties())
                if (clip.isEmpty() && property.name.toString().startsWith ("clip.") && property.value.isArray() && property.value.size() == 4)
                    clip = { static_cast<int> (property.value[0]), static_cast<int> (property.value[1]),
                             static_cast<int> (property.value[2]), static_cast<int> (property.value[3]) };
        REQUIRE_FALSE (clip.isEmpty());
        juce::Component* timeline = findChildWithComponentId (*shell, "timeline.canvas");
        REQUIRE (timeline != nullptr);
        const juce::Point<int> local = timeline->getLocalPoint (shell.get(), clip.getCentre());
        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(), local.toFloat(),
                                      juce::ModifierKeys::leftButtonModifier, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      timeline, timeline, juce::Time::getCurrentTime(), local.toFloat(),
                                      juce::Time::getCurrentTime(), 1, false);
        timeline->mouseDown (event);
        timeline->mouseUp (event);
        REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.timelineClipSelected);
    }

    // Every action a menu reaches (item ids are the action + 1, submenus included).
    std::set<int> menuActions;
    {
        auto* model = dynamic_cast<juce::MenuBarModel*> (shell.get());
        REQUIRE (model != nullptr);
        const juce::StringArray names = model->getMenuBarNames();
        for (int index = 0; index < names.size(); ++index)
        {
            juce::PopupMenu menu = model->getMenuForIndex (index, names[index]);
            for (juce::PopupMenu::MenuItemIterator it (menu, true); it.next();)
                if (it.getItem().itemID > 0)
                    menuActions.insert (it.getItem().itemID - 1);
        }
    }
    const auto actionForId = [] (const juce::String& id) -> int
    {
        const auto& descriptors = yesdaw::ui::uiActionDescriptors();
        for (std::size_t i = 0; i < descriptors.size(); ++i)
            if (id == descriptors[i].stableId)
                return static_cast<int> (i);
        return -1;
    };
    // The toolbar row's choosers: each of their choices is an action a menu carries.
    const std::map<juce::String, std::vector<UiActionId>> chooserActions {
        { "timeline.snap.chooser", { UiActionId::TimelineSnapDisable, UiActionId::TimelineSnapSetBar,
                                     UiActionId::TimelineSnapSetBeat, UiActionId::TimelineSnapSetSixteenth } },
        { "timeline.snap_mode.chooser", { UiActionId::TimelineSnapModeGrid, UiActionId::TimelineSnapModeRelative,
                                          UiActionId::TimelineSnapModeEvents, UiActionId::TimelineSnapModeOff } },
        { "timeline.edit_mode.chooser", { UiActionId::EditModeOverlap, UiActionId::EditModeNoOverlap, UiActionId::EditModeShuffle } },
        { "timeline.nudge.chooser", { UiActionId::EditNudgeValueGrid, UiActionId::EditNudgeValueBar, UiActionId::EditNudgeValueBeat,
                                      UiActionId::EditNudgeValueSixteenth, UiActionId::EditNudgeValueMs1, UiActionId::EditNudgeValueMs10,
                                      UiActionId::EditNudgeValueFrame, UiActionId::EditNudgeValueSample } },
        { "timeline.zoom.readout", { UiActionId::TimelineZoomIn, UiActionId::TimelineZoomOut } },
        { "timeline.zoom.slider", { UiActionId::TimelineZoomIn, UiActionId::TimelineZoomOut } },
        // The inspector's marker list moves the playhead to a marker: Transport > Previous / Next Marker do that where the
        // card cannot fit even with the dock collapsed (1280x720: it needs 640 px of inspector, the window leaves 556).
        { "clip.inspector.markers", { UiActionId::TransportLocatePreviousMarker, UiActionId::TransportLocateNextMarker } },
    };

    // Every shown, identified control at any depth (the FX editor's rows, a panel's buttons), in shell coordinates.
    struct ShownControl
    {
        juce::Component* component = nullptr;
        juce::Rectangle<int> bounds;
    };
    const auto visibleChildren = [&shell]
    {
        std::map<juce::String, ShownControl> shown;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& parent)
        {
            for (int i = 0; i < parent.getNumChildComponents(); ++i)
            {
                juce::Component* child = parent.getChildComponent (i);
                if (! child->isVisible())
                    continue;
                const juce::Rectangle<int> bounds = shell->getLocalArea (child, child->getLocalBounds());
                if (! bounds.isEmpty() && child->getComponentID().isNotEmpty())
                    shown[child->getComponentID()] = { child, bounds };
                walk (*child);
            }
        };
        walk (*shell);
        return shown;
    };
    shell->setSize (2560, 1440);
    const std::map<juce::String, ShownControl> widest = visibleChildren();
    REQUIRE (widest.count ("clip.inspector.fade_in") == 1);   // the selected clip's sections are in the reference

    std::map<std::pair<int, int>, juce::Image> logicalRenders;
    for (const ScalingCell& cell : cells)
    {
        const juce::String name = cell.kind + " " + juce::String (cell.width) + "x" + juce::String (cell.height)
                                + " @" + juce::String (juce::roundToInt (cell.scale * 100.0f)) + "%";
        INFO (name.toStdString());
        shell->setSize (cell.width, cell.height);
        const std::map<juce::String, ShownControl> shown = visibleChildren();

        // Geometry: the header's rects inside it and disjoint; the dock and the master inside the window.
        {
            const std::vector<juce::Rectangle<int>> rects = yesdaw::ui::mainComponentHeaderRects (*shell);
            const juce::Rectangle<int> header { 0, 0, cell.width, yesdaw::ui::mainComponentHeaderHeight (*shell) };
            for (std::size_t i = 0; i < rects.size(); ++i)
            {
                REQUIRE (header.contains (rects[i]));
                for (std::size_t j = i + 1; j < rects.size(); ++j)
                    REQUIRE_FALSE (rects[i].intersects (rects[j]));
            }
            REQUIRE (shell->getLocalBounds().contains (yesdaw::ui::mainComponentMixerPanelBounds (*shell)));
            REQUIRE (yesdaw::ui::mainComponentMixerPanelBounds (*shell).contains (yesdaw::ui::mainComponentPaintedMixerMasterBounds (*shell)));
            const auto row = yesdaw::ui::mainComponentTimelineToolbarControls (*shell);
            for (std::size_t i = 0; i < row.size(); ++i)
                for (std::size_t j = i + 1; j < row.size(); ++j)
                    if (! row[i].second.isEmpty() && ! row[j].second.isEmpty())
                        REQUIRE_FALSE (row[i].second.intersects (row[j].second));
        }

        // Reachable: every shown control is inside the window and hit-tests to itself at its centre.
        for (const auto& [id, control] : shown)
        {
            const juce::Rectangle<int> bounds = control.bounds;
            INFO ("control " << id.toStdString() << " " << bounds.toString().toStdString());
            juce::Component* child = control.component;
            REQUIRE (shell->getLocalBounds().contains (bounds));
            bool clicksSelf = true, clicksChildren = true;
            child->getInterceptsMouseClicks (clicksSelf, clicksChildren);
            if (! clicksSelf && ! clicksChildren)
                continue;   // a paint layer: clicks pass through it by design
            juce::Component* hit = shell->getComponentAt (bounds.getCentre());
            if (matrixReport() && ! (hit == child || child->isParentOf (hit)))
                std::cout << "[matrix] " << name << " covered: " << id << " by " << (hit != nullptr ? hit->getComponentID() + "/" + hit->getName() : juce::String ("nothing")) << "\n";
            REQUIRE ((hit == child || child->isParentOf (hit)));
        }
        // ... and every control shown in the widest window but dropped here has its action(s) in a menu.
        std::vector<juce::String> droppedInspector;
        juce::StringArray dropped;
        for (const auto& [id, control] : widest)
        {
            if (shown.count (id) > 0)
                continue;
            INFO ("dropped " << id.toStdString());
            dropped.add (id);
            if (matrixReport())
                std::cout << "[matrix] " << name << " dropped: " << id << "\n";
            if ((id.startsWith ("clip.inspector.") || id.startsWith ("track.inspector.")) && chooserActions.count (id) == 0)
            {
                droppedInspector.push_back (id);   // the inspector's whole-section law: back with the dock collapsed (below)
                continue;
            }
            if (const int action = actionForId (id); action >= 0)
            {
                REQUIRE (menuActions.count (action) == 1);
                continue;
            }
            const auto chooser = chooserActions.find (id);
            REQUIRE (chooser != chooserActions.end());
            for (const UiActionId action : chooser->second)
                REQUIRE (menuActions.count (static_cast<int> (action)) == 1);
        }

        // A dropped inspector section is one X away (plan §3.4 "dock collapsible"): collapsed, every one is shown.
        if (! droppedInspector.empty())
        {
            yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::TimelineToggleMixerDock);
            const std::map<juce::String, ShownControl> collapsed = visibleChildren();
            for (const juce::String& id : droppedInspector)
            {
                INFO ("inspector control with the dock collapsed: " << id.toStdString());
                REQUIRE (collapsed.count (id) == 1);
                REQUIRE (shell->getLocalBounds().contains (collapsed.at (id).bounds));
            }
            yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::TimelineToggleMixerDock);
            REQUIRE (yesdaw::ui::mainComponentMixerPanelBounds (*shell).getHeight() > 0);
        }
        if (matrixReport())
            std::cout << "[matrix] " << name << " DROPSET " << dropped.joinIntoString (" ") << "\n";
        // Exactly these drop, by size (2026-10-08, the 16-track fixture with a clip selected, the dock at 300): a control
        // that starts dropping where it fits, or a law that stops dropping one, fails here instead of passing silently.
        {
            const juce::String fades = "clip.inspector.fade_curve clip.inspector.fade_curve_amount clip.inspector.fade_in clip.inspector.fade_out ";
            const juce::String zoom = "timeline.zoom.in timeline.zoom.out timeline.zoom.readout timeline.zoom.slider";
            const juce::String modes = "timeline.edit_mode.chooser timeline.nudge.chooser timeline.snap_mode.chooser";
            const juce::String shortWindow = fades + "clip.inspector.markers clip.inspector.stretch timeline.clip.set_gain " + modes + " " + zoom;
            const std::map<juce::String, juce::String> expected {
                { "2560x1440", "" }, { "2560x1360", "" },
                { "2048x1081", "clip.inspector.markers" },
                { "1920x1080", "clip.inspector.markers" }, { "1920x1000", "clip.inspector.markers" },
                { "1706x896", "clip.inspector.markers timeline.edit_mode.chooser timeline.nudge.chooser" },
                { "1536x793", fades + "clip.inspector.markers " + modes },
                { "1280x720", fades + "clip.inspector.markers " + modes + " " + zoom },
                { "1280x664", shortWindow }, { "1280x656", shortWindow }, { "1280x640", shortWindow },
            };
            const auto pinned = expected.find (juce::String (cell.width) + "x" + juce::String (cell.height));
            REQUIRE (pinned != expected.end());
            REQUIRE (dropped.joinIntoString (" ") == pinned->second);
        }

        // Rendered at the scale: the image is the physical size.
        auto logical = logicalRenders.find ({ cell.width, cell.height });
        if (logical == logicalRenders.end())
            logical = logicalRenders.emplace (std::make_pair (cell.width, cell.height), renderShellAtScale (*shell, 1.0f)).first;
        const juce::Image scaled = cell.scale == 1.0f ? logical->second : renderShellAtScale (*shell, cell.scale);
        REQUIRE (scaled.getWidth() == static_cast<int> (std::ceil (static_cast<float> (cell.width) * cell.scale)));
        REQUIRE (scaled.getHeight() == static_cast<int> (std::ceil (static_cast<float> (cell.height) * cell.scale)));
        if (juce::SystemStats::getEnvironmentVariable ("YESDAW_UI_SCREENSHOT_DIR", {}).isNotEmpty())
            (void) captureShellPng (scaled, ("yesdaw-g62-" + cell.kind + "-" + juce::String (cell.width) + "x" + juce::String (cell.height)
                                             + "-" + juce::String (juce::roundToInt (cell.scale * 100.0f)) + ".png").toRawUTF8());
       #if JUCE_WINDOWS
        // Windows (the drives' platform; JUCE's software rasteriser, measured 2026-10-08): the scaled render box-filtered
        // back to the logical size matches the 100 % render per panel (measured max 3.13; bound 6), and at 200 % each
        // panel carries detail a 1x image drawn at 2x does not: such an image IS the bilinear upscale of the 1x render
        // (difference 0, asserted below as the metric's own calibration), the native render differs from it by
        // 1.63-3.75 (floor 0.8) — a cache rasterised at 1x fails here.
        if (cell.scale != 1.0f)
        {
            const juce::Image down = scaled.rescaled (cell.width, cell.height, juce::Graphics::highResamplingQuality);
            const juce::Rectangle<int> header { 0, 0, cell.width, yesdaw::ui::mainComponentHeaderHeight (*shell) };
            const std::array<std::pair<const char*, juce::Rectangle<int>>, 3> panels {{
                { "header", header },
                { "timeline", yesdaw::ui::mainComponentTimelineBounds (*shell) },
                { "dock", yesdaw::ui::mainComponentMixerPanelBounds (*shell) } }};
            const juce::Image up = cell.scale == 2.0f
                ? logical->second.rescaled (scaled.getWidth(), scaled.getHeight(), juce::Graphics::highResamplingQuality)
                : juce::Image();
            juce::Image blurred;   // a 1x cache drawn at 2x
            if (cell.scale == 2.0f)
            {
                blurred = juce::Image (juce::Image::ARGB, scaled.getWidth(), scaled.getHeight(), true, juce::SoftwareImageType());
                juce::Graphics g (blurred);
                g.drawImageTransformed (logical->second, juce::AffineTransform::scale (2.0f));
            }
            for (const auto& [panel, rect] : panels)
            {
                const double fidelity = meanAbsDifference (down, logical->second, rect);
                INFO ("panel " << panel << " fidelity " << fidelity);
                if (matrixReport())
                    std::cout << "[matrix] " << name << " " << panel << " fidelity " << fidelity;
                REQUIRE (fidelity <= 6.0);
                if (cell.scale == 2.0f)
                {
                    const double native = meanAbsDifference (scaled, up, rect * 2);
                    const double cached = meanAbsDifference (blurred, up, rect * 2);
                    INFO ("resolution " << native << " vs a 1x cache " << cached);
                    if (matrixReport())
                        std::cout << " resolution " << native << " vs 1x cache " << cached;
                    REQUIRE (cached <= 0.5);   // the metric can tell: a 1x image drawn at 2x scores ~0
                    REQUIRE (native >= 0.8);
                }
                if (matrixReport())
                    std::cout << "\n";
            }
        }
       #endif
    }

    std::error_code ec;
    std::filesystem::remove_all (fixtureDir, ec);
}

TEST_CASE ("the shell renders honestly at the resize-limit extremes",
           "[ui][screenshot][shell-sizes]")
{
    juce::MessageManager::getInstance();

    const std::filesystem::path bundlePath =
        std::filesystem::temp_directory_path() / "yesdaw-ui-screenshot-shell-sizes.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('n', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)));

    // The window's resize limits ARE the layout contract: every shipped panel must render
    // honestly at the floor and at a beyond-default wide size (E27; B41 gate model).
    const auto controlById = [&shell] (const char* id) -> juce::Component*
    {
        for (int child = 0; child < shell->getNumChildComponents(); ++child)
            if (shell->getChildComponent (child)->getComponentID() == id)
                return shell->getChildComponent (child);
        return nullptr;
    };
    const auto renderAtSize = [&shell, &controlById] (int width, int height,
                                                      bool expectFadesDropped,
                                                      const char* filename)
    {
        shell->setSize (width, height);
        const juce::Image image = renderShell (*shell);
        REQUIRE (hasHeaderCoverage (image));
        {
            using L = yesdaw::ui::UiTheme::Layout;
            const int work = height - L::headerHeight - L::mixerHeight;
            REQUIRE (sampledDifferentPixelCount (image, { 0, L::headerHeight, L::leftRailWidth, work }) > 20u);
            REQUIRE (sampledDifferentPixelCount (image, { L::leftRailWidth, L::headerHeight,
                                                          width - L::leftRailWidth - L::inspectorWidth, work }) > 60u);
            REQUIRE (sampledDifferentPixelCount (image, { 0, height - L::mixerHeight, width, L::mixerHeight }) > 40u);
        }
        // E27: the WHOLE-SECTION drop law — an inspector section fits entirely (all its
        // controls laid out above the bottom mixer panel) or is dropped entirely (all its
        // controls empty). The FADES section is the witness: dropped at the floor, present
        // at the wide size. The stats section fits at both.
        const int bottomPanelTop = height - yesdaw::ui::UiTheme::Layout::mixerHeight;
        for (const char* id : { "clip.inspector.start", "clip.inspector.end",
                                "clip.inspector.length" })
        {
            juce::Component* control = controlById (id);
            REQUIRE (control != nullptr);
            INFO ("control " << id << " bounds " << control->getBounds().toString().toStdString());
            REQUIRE_FALSE (control->getBounds().isEmpty());
            REQUIRE (control->getBounds().getBottom() <= bottomPanelTop);
        }
        for (const char* id : { "clip.inspector.fade_in", "clip.inspector.fade_out",
                                "clip.inspector.fade_curve" })
        {
            juce::Component* control = controlById (id);
            REQUIRE (control != nullptr);
            INFO ("control " << id << " bounds " << control->getBounds().toString().toStdString());
            REQUIRE (control->getBounds().isEmpty() == expectFadesDropped);
            REQUIRE ((control->getBounds().isEmpty()
                      || control->getBounds().getBottom() <= bottomPanelTop));
        }
        // A dropped section paints NOTHING: the inspector column's slice of the mixer's top
        // edge holds no bright row text (the old paint stamped "Fade Out ..." over it).
        if (expectFadesDropped)
        {
            for (int y = bottomPanelTop + 2; y < bottomPanelTop + 10; ++y)
                for (int x = width - 312; x < width - 122; ++x)
                    REQUIRE (image.getPixelAt (x, y).getBrightness() < 0.5f);
        }
        // M9: the HEADER's master card obeys the same whole-section law. At the floor the card
        // used to keep its "MASTER" label while its meter and LUFS readout were clipped off the
        // window; now the card is right-anchored and drops WHOLE. The LUFS readout is the witness:
        // present and inside the window when the card fits, empty bounds when it does not — never
        // placed past the right edge.
        {
            juce::Component* const lufs = yesdaw::ui::findMainComponentChildForAction (
                *shell, UiActionId::MixerReadLoudness);
            REQUIRE (lufs != nullptr);
            INFO ("LUFS bounds " << lufs->getBounds().toString().toStdString()
                  << " in width " << width);
            REQUIRE ((lufs->getBounds().isEmpty() || lufs->getBounds().getRight() <= width));
            // The card itself is the law: empty (dropped whole, label included) or entirely inside
            // the window with the LUFS readout sitting on its right edge.
            const juce::Rectangle<int> card =
                yesdaw::ui::mainComponentHeaderMasterCardBounds (*shell);
            INFO ("master card " << card.toString().toStdString());
            REQUIRE (card.isEmpty() == lufs->getBounds().isEmpty());
            if (! card.isEmpty())
            {
                REQUIRE (card.getRight() <= width);
                REQUIRE (card.getX() >= 0);
                REQUIRE (lufs->getBounds().getRight() == card.getRight());
            }
        }

        // M9's utility rows are gone with the tools lane (G4.1 cp2); the master fader is the one live
        // strip control left and it never hangs past the panel's bottom edge.
        if (juce::Component* const masterFader = controlById ("mixer.master.fader"))
            REQUIRE ((masterFader->getBounds().isEmpty() || masterFader->getBounds().getBottom() <= height));

        (void) captureShellPng (image, filename);
    };

    renderAtSize (yesdaw::ui::UiTheme::Layout::windowMinWidth,
                  yesdaw::ui::UiTheme::Layout::windowMinHeight,
                  true,
                  "yesdaw-shell-min.png");
    // ADR-0064: at the window minimum a dropped inspector section is one X away (plan §3.4 "dock collapsible"): with
    // the dock collapsed the FADES section is shown whole, inside the window.
    {
        yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::TimelineToggleMixerDock);
        REQUIRE (yesdaw::ui::mainComponentMixerPanelBounds (*shell).getHeight() <= 0);
        for (const char* id : { "clip.inspector.fade_in", "clip.inspector.fade_out", "clip.inspector.fade_curve" })
        {
            juce::Component* control = controlById (id);
            REQUIRE (control != nullptr);
            INFO ("control " << id << " bounds " << control->getBounds().toString().toStdString());
            REQUIRE_FALSE (control->getBounds().isEmpty());
            REQUIRE (shell->getLocalBounds().contains (control->getBounds()));
        }
        yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::TimelineToggleMixerDock);
        REQUIRE (yesdaw::ui::mainComponentMixerPanelBounds (*shell).getHeight() > 0);
    }
    renderAtSize (2560, 1440, false, "yesdaw-shell-wide.png");

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the rail arm badge lights red on the armed track", "[ui][screenshot][arm-badge]")
{
    juce::MessageManager::getInstance();

    const std::filesystem::path bundlePath =
        std::filesystem::temp_directory_path() / "yesdaw-ui-screenshot-arm-badge.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));

    juce::Component* rail = nullptr;
    for (int child = 0; child < shell->getNumChildComponents(); ++child)
        if (shell->getChildComponent (child)->getComponentID() == "shell.tracklist.input")
            rail = shell->getChildComponent (child);
    REQUIRE (rail != nullptr);

    // G0.7: the device/recording cluster lives in the settings row; showing it moves the rail,
    // so it is shown BEFORE the badge centre is computed.
    yesdaw::ui::mainComponentSetSettingsRowVisible (*shell, true);
    // Row 0's third rail cell ("O") center, in shell space — the shared row/cell token law.
    using L = yesdaw::ui::UiTheme::Layout;
    const juce::Point<int> badgeCentre {
        rail->getX() + L::trackListNameLeftInset + 2 * L::trackListButtonWidth
            + L::trackListButtonWidth / 2,
        rail->getY() + L::trackListHeaderHeight + L::trackListButtonsTop
            + L::trackListButtonsHeight / 2
    };

    const juce::Image before = renderShell (*shell);
    REQUIRE (before.getPixelAt (badgeCentre.x, badgeCentre.y)
             == yesdaw::ui::UiTheme::Color::mixerBack());

    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::DeviceSelectTestAudio);   // G0.8: harness-only verb
    clickButton (requireButtonForAction (*shell, UiActionId::RecordingArmTrack));

    const juce::Image armed = renderShell (*shell);
    REQUIRE (armed.getPixelAt (badgeCentre.x, badgeCentre.y)
             == yesdaw::ui::UiTheme::Color::dangerRed());

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("H16 screenshot coverage gate rejects a blank mixer surface", "[ui][screenshot][negative]")
{
    const juce::Image blank (juce::Image::ARGB,
                             yesdaw::ui::UiTheme::Layout::defaultWindowWidth,
                             yesdaw::ui::UiTheme::Layout::defaultWindowHeight,
                             true);
    REQUIRE_FALSE (hasMixerSurfaceCoverage (blank));
    REQUIRE_FALSE (hasMixerMasterSummaryCoverage (
        blank,
        juce::Rectangle<int> { blank.getWidth() - 104, 88, 104, blank.getHeight() - 88 }));
    // G0.7: the section probe reads the shell's layout law for WHERE to sample; a blank image
    // still fails it.
    auto shell = yesdaw::ui::createMainComponent (yesdaw::ui::MainComponentFileChoices {});
    shell->setSize (blank.getWidth(), blank.getHeight());
    REQUIRE_FALSE (hasHeaderSectionHierarchy (blank, *shell));
    REQUIRE_FALSE (hasTrackMixSummaryCoverage (blank));
    REQUIRE_FALSE (hasInspectorSectionHierarchy (blank));
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

// ADR-0063 (G6.1): what the rubric shots showed, made mechanical — the zoom trio slid under the view cluster on a
// narrow timeline, and the piano roll's Scale chooser cut "Scale: Off" to "Scal...".
TEST_CASE ("ADR-0063 the timeline toolbar row never overlaps, and the piano roll's choosers show their items whole",
           "[ui][screenshot][tokens]")
{
    juce::MessageManager::getInstance();
    const std::filesystem::path bundlePath = std::filesystem::temp_directory_path() / "yesdaw-g61-toolbar-row.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    for (const bool inspectorOpen : { true, false })
    {
        if (inspectorOpen != yesdaw::ui::snapshotMainComponent (*shell).context.inspectorVisible)
            yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ViewToggleInspector);
        for (const auto& size : { std::pair<int, int> { 1152, 720 }, std::pair<int, int> { 1280, 720 },
                                  std::pair<int, int> { 1366, 768 }, std::pair<int, int> { 1920, 1080 },
                                  std::pair<int, int> { 2560, 1440 } })
        {
            shell->setSize (size.first, size.second);
            const auto controls = yesdaw::ui::mainComponentTimelineToolbarControls (*shell);
            for (const char* view : { "view.inspector", "view.mixer", "view.pianoroll", "view.automation" })   // never dropped
                REQUIRE (std::any_of (controls.begin(), controls.end(), [view] (const auto& control) { return control.first == view; }));
            for (std::size_t i = 0; i < controls.size(); ++i)
                for (std::size_t j = i + 1; j < controls.size(); ++j)
                {
                    INFO (controls[i].first << " and " << controls[j].first << " at " << size.first << "x" << size.second
                                            << (inspectorOpen ? " (inspector open)" : " (inspector closed)"));
                    REQUIRE_FALSE (controls[i].second.intersects (controls[j].second));
                }
        }
    }

    // The piano roll's Key and Scale choosers: every item's text fits the chooser's own text box.
    shell->setSize (1152, 720);
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ViewPianoRoll);
    for (const char* id : { "pianoroll.key", "pianoroll.scale" })
    {
        auto* chooser = dynamic_cast<juce::ComboBox*> (findChildWithComponentId (*shell, id));
        REQUIRE (chooser != nullptr);
        juce::Label* text = nullptr;
        for (int i = 0; i < chooser->getNumChildComponents() && text == nullptr; ++i)
            text = dynamic_cast<juce::Label*> (chooser->getChildComponent (i));
        REQUIRE (text != nullptr);
        for (int i = 0; i < chooser->getNumItems(); ++i)
        {
            const juce::String item = chooser->getItemText (i);
            const float width = juce::GlyphArrangement::getStringWidth (text->getFont(), item);
            INFO (id << " item \"" << item << "\": " << width << " px in "
                      << text->getWidth() - text->getBorderSize().getLeftAndRight() << " px");
            // 2 px to spare: text measured exactly at the box's width can still be ellipsised by drawText.
            REQUIRE (width + 2.0f <= static_cast<float> (text->getWidth() - text->getBorderSize().getLeftAndRight()));
        }
    }
}

TEST_CASE ("V1 painted text achieves legible contrast against its panel at every D7 size",
           "[ui][screenshot][theme-legibility]")
{
    juce::MessageManager::getInstance();

    const std::filesystem::path bundlePath =
        std::filesystem::temp_directory_path() / "yesdaw-ui-screenshot-theme-legibility.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };

    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::TrackToggleMute);   // ADR-0063: a lit rail cell to sample

    using L = yesdaw::ui::UiTheme::Layout;
    // A dark-mode DAW UI legitimately carries a lot of secondary/muted text (labels, units) that
    // reads fine to the eye without hitting the 4.5:1 WCAG body-text bar; 3.0:1 is a real floor
    // under the D7 judgment pass, not a rubber stamp — a genuinely invisible/near-invisible label
    // (the failure mode this gate exists to catch) will not clear it.
    constexpr double kMinContrastRatio = 3.0;

    for (const auto& size : { std::pair<int, int> { 1152, 720 },
                              std::pair<int, int> { 1536, 960 },
                              std::pair<int, int> { 1920, 1080 } })
    {
        shell->setSize (size.first, size.second);
        const juce::Image image = renderShell (*shell);

        // Header time readout: the largest, most load-bearing numeric readout in the shell.
        const juce::Rectangle<int> timeReadout = yesdaw::ui::mainComponentHeaderTimeReadoutBounds (*shell);
        REQUIRE_FALSE (timeReadout.isEmpty());
        INFO ("time readout contrast at " << size.first << "x" << size.second);
        REQUIRE (maxContrastInRegion (image, timeReadout) >= kMinContrastRatio);

        // Rail track name: the primary identifying label for every track.
        juce::Component* rail = findChildWithComponentId (*shell, "shell.tracklist.input");
        REQUIRE (rail != nullptr);
        const juce::Rectangle<int> trackName {
            rail->getX() + L::trackListNameLeftInset, rail->getY() + L::trackListHeaderHeight,
            160, L::trackListNameHeight
        };
        INFO ("track name contrast at " << size.first << "x" << size.second);
        REQUIRE (maxContrastInRegion (image, trackName) >= kMinContrastRatio);

        // ADR-0063: a clip's name on its body, and a lit rail cell (dark ink on the track colour).
        const juce::var probe = juce::JSON::parse (yesdaw::ui::mainComponentStateProbeJson (*shell));
        const juce::var layout = probe.getProperty ("layout", juce::var());
        REQUIRE (layout.isObject());
        const auto rectOf = [&layout] (const juce::Identifier& key) {
            const juce::var value = layout.getProperty (key, juce::var());
            return value.isArray() && value.size() == 4
                       ? juce::Rectangle<int> (static_cast<int> (value[0]), static_cast<int> (value[1]),
                                               static_cast<int> (value[2]), static_cast<int> (value[3]))
                       : juce::Rectangle<int> {};
        };
        juce::Rectangle<int> clip;
        if (auto* properties = layout.getDynamicObject())
            for (const auto& property : properties->getProperties())
                if (property.name.toString().startsWith ("clip.") && clip.isEmpty())
                    clip = rectOf (property.name);
        REQUIRE_FALSE (clip.isEmpty());
        const juce::Rectangle<int> clipName = clip.reduced (yesdaw::ui::UiTheme::Space::sm)
                                                  .withHeight (static_cast<int> (yesdaw::ui::UiTheme::Type::small) + 4)
                                                  .withWidth (std::min (120, clip.getWidth() - 2 * static_cast<int> (yesdaw::ui::UiTheme::Space::sm)));
        INFO ("clip name contrast at " << size.first << "x" << size.second);
        REQUIRE (maxContrastInRegion (image, clipName) >= kMinContrastRatio);
        const juce::Rectangle<int> muteCell = rectOf ("rail.row.0.mute").reduced (3);
        REQUIRE_FALSE (muteCell.isEmpty());
        INFO ("lit rail cell contrast at " << size.first << "x" << size.second);
        REQUIRE (maxContrastInRegion (image, muteCell) >= kMinContrastRatio);
    }

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

// G0.7 (plan §3.4, ADR-0046): the header is a flex row — tools left, transport centred on the
// window, master card right-anchored against the gear — from ONE layout law, at every supported
// size, with and without the settings row. Nothing in the header overlaps anything else.
TEST_CASE ("header flex row: tools left, transport centred, master right, nothing overlaps",
           "[ui][screenshot][g0][header-flex]")
{
    using L = yesdaw::ui::UiTheme::Layout;
    STATIC_REQUIRE (L::menuBarHeight == 28);
    STATIC_REQUIRE (L::toolbarHeight == 60);
    STATIC_REQUIRE (L::headerHeight == 88);
    STATIC_REQUIRE (L::headerMasterWidth == 260);
    STATIC_REQUIRE (L::timelineCanvasLaneRowHeight == 72);
    STATIC_REQUIRE (L::trackListRowMinHeight == L::timelineCanvasLaneRowHeight);
    STATIC_REQUIRE (L::leftRailWidth == 260);
    STATIC_REQUIRE (L::inspectorWidth == 300);
    STATIC_REQUIRE (L::timelineRulerBarsRowHeight == 22);
    STATIC_REQUIRE (L::timelineRulerTimeRowHeight == 22);
    STATIC_REQUIRE (L::timelineRulerMarkerLaneHeight == 20);
    STATIC_REQUIRE (L::timelineCanvasRulerHeight == 64);
    STATIC_REQUIRE (L::mixerHeight == 300);   // plan §3.4: 300 — G2.1 cp2 lifted D27 (the dock is draggable; the mixer is a tab)
    // The rail row at 260: number · icon · name · M S O left of the PAN/VOL cluster, no overlap.
    STATIC_REQUIRE (L::trackListNameLeftInset + 3 * L::trackListButtonWidth
                    <= L::leftRailWidth - L::trackListMixSummaryRightInset - L::trackListMixSummaryWidth);
    STATIC_REQUIRE (L::trackListIconLeftInset + L::trackListIconSize <= L::trackListNameLeftInset);

    auto shell = yesdaw::ui::createMainComponent (yesdaw::ui::MainComponentFileChoices {});
    for (const auto& size : { std::pair<int, int> { L::windowMinWidth, L::windowMinHeight },
                              std::pair<int, int> { 1280, 720 },
                              std::pair<int, int> { L::defaultWindowWidth, L::defaultWindowHeight },
                              std::pair<int, int> { 1920, 1080 },
                              std::pair<int, int> { 2560, 1440 } })
    {
        const int width = size.first;
        shell->setSize (width, size.second);
        for (const bool settings : { false, true })
        {
            INFO ("size " << width << "x" << size.second << " settings row " << (settings ? "shown" : "hidden"));
            yesdaw::ui::mainComponentSetSettingsRowVisible (*shell, settings);
            const int headerHeight = yesdaw::ui::mainComponentHeaderHeight (*shell);
            REQUIRE (headerHeight == L::headerHeight + (settings ? L::settingsRowHeight : 0));
            const juce::Rectangle<int> header { 0, 0, width, headerHeight };

            // Every laid-out rect is inside the header and pairwise disjoint; every visible child
            // that lives in the header IS one of those rects or sits inside one (the tempo/meter
            // controls in their box, the LUFS readout on the master card).
            const std::vector<juce::Rectangle<int>> rects = yesdaw::ui::mainComponentHeaderRects (*shell);
            REQUIRE (rects.size() >= 17u);
            for (std::size_t i = 0; i < rects.size(); ++i)
            {
                INFO ("rect " << rects[i].toString().toStdString());
                REQUIRE (header.contains (rects[i]));
                for (std::size_t j = i + 1; j < rects.size(); ++j)
                {
                    INFO ("vs " << rects[j].toString().toStdString());
                    REQUIRE_FALSE (rects[i].intersects (rects[j]));
                }
            }
            for (int i = 0; i < shell->getNumChildComponents(); ++i)
            {
                const juce::Component* child = shell->getChildComponent (i);
                const juce::Rectangle<int> bounds = child->getBounds();
                if (! child->isVisible() || bounds.isEmpty() || bounds.getBottom() > headerHeight)
                    continue;
                bool clicksSelf = true, clicksChildren = true;
                child->getInterceptsMouseClicks (clicksSelf, clicksChildren);
                if (! clicksSelf && ! clicksChildren && child->getComponentID().isEmpty())
                    continue;   // not a control: a layer that takes nothing (ADR-0066: the header's accessible elements' host)
                INFO ("child " << child->getName().toStdString() << " " << bounds.toString().toStdString());
                bool placed = false;
                for (const juce::Rectangle<int>& rect : rects)
                    placed = placed || rect.contains (bounds);
                REQUIRE (placed);
            }

            // The master card keeps its full width from 1280 up and is right-anchored against the
            // gear; at the floor it may shrink but never drops.
            const juce::Rectangle<int> card = yesdaw::ui::mainComponentHeaderMasterCardBounds (*shell);
            REQUIRE_FALSE (card.isEmpty());
            REQUIRE (card.getRight() == width - L::headerStatusIconRightInset - L::headerMasterGearGap);
            if (width >= 1280)
                REQUIRE (card.getWidth() == L::headerMasterWidth);

            // Sections in order, disjoint; the transport group centred on the window once there
            // is room for it (the default size and up).
            const juce::Rectangle<int> tools = yesdaw::ui::mainComponentHeaderSectionBounds (*shell, 0);
            const juce::Rectangle<int> transport = yesdaw::ui::mainComponentHeaderSectionBounds (*shell, 1);
            const juce::Rectangle<int> master = yesdaw::ui::mainComponentHeaderSectionBounds (*shell, 2);
            REQUIRE_FALSE (tools.isEmpty());
            REQUIRE_FALSE (transport.isEmpty());
            REQUIRE_FALSE (master.isEmpty());
            REQUIRE (tools.getRight() <= transport.getX());
            REQUIRE (transport.getRight() <= master.getX());
            if (width >= L::defaultWindowWidth)
                REQUIRE (std::abs (transport.getCentreX() - width / 2) <= 1);

            const juce::Image image = renderShell (*shell);
            REQUIRE (hasHeaderSectionHierarchy (image, *shell));
        }
        yesdaw::ui::mainComponentSetSettingsRowVisible (*shell, false);
    }
}

// G0.7: the rubric shots — the song fixture (16 tracks, six seconds) opened through the real Open
// action and rendered at the three judged sizes into YESDAW_UI_SCREENSHOT_DIR (or the temp dir).
// The mechanical part: with a real project loaded, the header law still holds and 1080p shows at
// least eight whole 72 px lanes; the judgment part is the rubric in STATUS.md.
TEST_CASE ("G0.7 rubric shots: the song fixture at 1280x720, 1920x1080 and 2560x1440",
           "[ui][screenshot][g0][rubric-shots]")
{
    using L = yesdaw::ui::UiTheme::Layout;
    const std::filesystem::path fixtureDir = std::filesystem::temp_directory_path() / "yesdaw-g07-rubric-fixture";
    {
        std::error_code ec;
        std::filesystem::remove_all (fixtureDir, ec);
    }
    yesdaw::app::fixture::SongFixtureSpec spec;
    spec.tracks = 16;
    spec.seconds = 6.0;
    spec.sampleRateHz = 48000;
    spec.channels = 2;
    spec.midiTracks = 4;
    const yesdaw::app::fixture::SongFixtureResult fixture = yesdaw::app::fixture::buildSongFixture (fixtureDir, spec);
    INFO (fixture.error);
    REQUIRE (fixture.ok);

    yesdaw::ui::MainComponentFileChoices choices;
    const std::filesystem::path bundlePath = fixture.bundlePath;
    choices.chooseOpenProjectBundle = [bundlePath] { return bundlePath; };
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    shell->setSize (L::defaultWindowWidth, L::defaultWindowHeight);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectOpen));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.projectLoaded);

    for (const auto& size : { std::pair<int, int> { 1280, 720 },
                              std::pair<int, int> { 1920, 1080 },
                              std::pair<int, int> { 2560, 1440 } })
    {
        const int width = size.first;
        const int height = size.second;
        shell->setSize (width, height);
        const juce::Image image = renderShell (*shell);
        const juce::String name = "yesdaw-g07-" + juce::String (width) + "x" + juce::String (height) + ".png";
        (void) captureShellPng (image, name.toRawUTF8());

        REQUIRE (hasHeaderCoverage (image));
        REQUIRE (hasHeaderSectionHierarchy (image, *shell));
        // G1.4: the counter's caption row (the secondary time) really paints, and the menu bar
        // shows all nine menus (its last name paints inside its bounds).
        {
            const juce::Rectangle<int> readout = yesdaw::ui::mainComponentHeaderTimeReadoutBounds (*shell);
            const juce::Rectangle<int> caption = readout.withTrimmedTop (L::headerTransportLabelInsetY).reduced (L::headerTransportTextInsetX, yesdaw::ui::UiTheme::Space::hairline);
            REQUIRE (caption.getHeight() >= 10);
            REQUIRE (fullDifferentPixelCount (image, caption) > 20u);
            const juce::Rectangle<int> menuBar = L::headerMenuBarBounds();
            REQUIRE (fullDifferentPixelCount (image, menuBar.withLeft (menuBar.getRight() - 90)) > 20u);   // "Help" paints
        }
        const juce::Rectangle<int> card = yesdaw::ui::mainComponentHeaderMasterCardBounds (*shell);
        REQUIRE (card.getWidth() == L::headerMasterWidth);
        const juce::Rectangle<int> transport = yesdaw::ui::mainComponentHeaderSectionBounds (*shell, 1);
        if (width >= L::defaultWindowWidth)
            REQUIRE (std::abs (transport.getCentreX() - width / 2) <= 1);

        const juce::var probe = juce::JSON::parse (yesdaw::ui::mainComponentStateProbeJson (*shell));
        const juce::var layout = probe.getProperty ("layout", juce::var());
        REQUIRE (layout.isObject());
        const juce::Rectangle<int> timeline = yesdaw::ui::mainComponentTimelineBounds (*shell);
        int wholeLanes = 0;
        for (int lane = 0; lane < spec.tracks; ++lane)
        {
            const juce::var value = layout.getProperty ("lane." + juce::String (lane), juce::var());
            if (! value.isArray() || value.size() != 4)
                continue;
            const juce::Rectangle<int> rect (static_cast<int> (value[0]), static_cast<int> (value[1]),
                                             static_cast<int> (value[2]), static_cast<int> (value[3]));
            REQUIRE (rect.getHeight() <= L::timelineCanvasLaneRowHeight);
            if (timeline.contains (rect) && rect.getHeight() == L::timelineCanvasLaneRowHeight)
                ++wholeLanes;
        }
        // G0.7 cp3: the ruler's three rows on the real fixture — bar numbers in the bars row,
        // minutes:seconds in the time row (each row: real painted text right of bar 1, judged by
        // a full scan), the marker lane 20 px under them.
        {
            const juce::var rulerVar = layout.getProperty ("ruler", juce::var());
            REQUIRE ((rulerVar.isArray() && rulerVar.size() == 4));
            const juce::Rectangle<int> ruler (static_cast<int> (rulerVar[0]), static_cast<int> (rulerVar[1]),
                                              static_cast<int> (rulerVar[2]), static_cast<int> (rulerVar[3]));
            REQUIRE (ruler.getHeight() == L::timelineCanvasRulerHeight);
            const juce::Rectangle<int> rightHalf = ruler.withLeft (ruler.getCentreX());
            const juce::Rectangle<int> barsRow = rightHalf.withHeight (L::timelineRulerBarsRowHeight - 1);
            const juce::Rectangle<int> timeRow = rightHalf.withY (ruler.getY() + L::timelineRulerBarsRowHeight)
                                                     .withHeight (L::timelineRulerTimeRowHeight - 1);
            INFO ("ruler " << ruler.toString().toStdString());
            REQUIRE (fullDifferentPixelCount (image, barsRow) > 30u);
            REQUIRE (fullDifferentPixelCount (image, timeRow) > 30u);
            // ADR-0064: a bar's number sits right of its tick — the pixel column above each tick (bar 2 on; bar 1 is
            // under the playhead) carries no text ink, and the number itself paints.
            int barsChecked = 0;
            for (int bar = 2; bar < 64; ++bar)
            {
                const juce::var value = layout.getProperty ("ruler.bar." + juce::String (bar), juce::var());
                if (! value.isArray() || value.size() != 4)
                    continue;
                const juce::Rectangle<int> label (static_cast<int> (value[0]), static_cast<int> (value[1]),
                                                  static_cast<int> (value[2]), static_cast<int> (value[3]));
                const int tickX = label.getX() - L::timelineCanvasRulerTickWidth - yesdaw::ui::UiTheme::Space::xxs;
                if (tickX < ruler.getX() + 4 || label.getRight() > ruler.getRight() - 4)
                    continue;
                const juce::Rectangle<int> aboveTick (tickX, ruler.getY() + 1, 1,
                                                      L::timelineRulerBarsRowHeight - L::timelineCanvasRulerTickHeight - 1);
                INFO ("bar " << bar << " label " << label.toString().toStdString());
                REQUIRE (maxContrastInRegion (image, aboveTick) < 1.5);
                REQUIRE (maxContrastInRegion (image, label) >= 3.0);
                ++barsChecked;
            }
            REQUIRE (barsChecked >= 1);
        }
        INFO ("whole lanes at " << width << "x" << height << ": " << wholeLanes);
        // G2.1 cp2: the plan's 300 px dock at 720p leaves two whole rows and a third partial
        // (88 + 28 + 64 + 3×72 + 300 + insets does not fit 720); the dock is one drag from 160,
        // where four fit. ADR-0064: at 1080p the 28 px tool row and 4 px insets leave eight whole lanes (the plan's
        // rubric line 2).
        if (height >= 1080)
            REQUIRE (wholeLanes >= 8);
        else
            REQUIRE (wholeLanes >= 2);
    }

    std::error_code ec;
    std::filesystem::remove_all (fixtureDir, ec);
}

// G4.6 / ADR-0052: stacked automation lanes render honestly — the Fader lane stacked under the clip, the Pan
// lane in the chooser lane under it, both with real curves; the clip keeps its own part of the row.
TEST_CASE ("stacked automation lanes render under their track's clips", "[ui][screenshot][automation-stack]")
{
    juce::MessageManager::getInstance();
    const std::filesystem::path bundlePath =
        std::filesystem::temp_directory_path() / "yesdaw-ui-screenshot-automation-stack.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };
    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    shell->setSize (1536, 960);

    const auto findChildById = [&shell] (const char* id) -> juce::Component*
    {
        for (int child = 0; child < shell->getNumChildComponents(); ++child)
            if (shell->getChildComponent (child)->getComponentID() == id)
                return shell->getChildComponent (child);
        return nullptr;
    };
    const auto mouseDownUpAt = [] (juce::Component& component, juce::Point<int> point)
    {
        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(),
                                      point.toFloat(), juce::ModifierKeys::leftButtonModifier,
                                      0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                      &component, &component, juce::Time::getCurrentTime(),
                                      point.toFloat(), juce::Time::getCurrentTime(), 1, false);
        component.mouseDown (event);
        component.mouseUp (event);
    };

    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    clickButton (requireButtonForAction (*shell, UiActionId::TimelineAutomationToggleTrackLane));
    auto* chooser = dynamic_cast<juce::ComboBox*> (findChildById ("timeline.automation.target"));
    REQUIRE (chooser != nullptr);
    const auto pencil = [&] (std::initializer_list<std::pair<double, double>> points)
    {
        juce::Component* band = findChildById ("timeline.automation.canvas");
        REQUIRE (band != nullptr);
        for (const auto& [fx, fy] : points)
            mouseDownUpAt (*band, { juce::roundToInt (band->getWidth() * fx), juce::roundToInt (band->getHeight() * fy) });
    };
    chooser->setSelectedId (1, juce::sendNotificationSync);   // Fader
    pencil ({ { 0.1, 0.8 }, { 0.4, 0.2 }, { 0.75, 0.6 } });
    chooser->setSelectedId (2, juce::sendNotificationSync);   // Pan
    pencil ({ { 0.2, 0.3 }, { 0.6, 0.7 } });

    const juce::Image image = renderShell (*shell);
    const auto structureIn = [&image] (juce::Rectangle<int> area) {
        const juce::Rectangle<int> lane = area.getIntersection (image.getBounds());
        REQUIRE_FALSE (lane.isEmpty());
        const auto background = image.getPixelAt (lane.getRight() - 2, lane.getBottom() - 2).getARGB();
        std::uint64_t structure = 0;
        for (int y = lane.getY(); y < lane.getBottom(); ++y)
            for (int x = lane.getX(); x < lane.getRight(); ++x)
                if (image.getPixelAt (x, y).getARGB() != background)
                    ++structure;
        return structure;
    };
    juce::Component* faderLane = findChildById ("timeline.automation.lane.0.0");
    juce::Component* panLane = findChildById ("timeline.automation.canvas");
    REQUIRE (faderLane != nullptr);
    REQUIRE (panLane != nullptr);
    REQUIRE (faderLane->getBottom() <= panLane->getY());   // stacked, the chooser lane last
    REQUIRE (structureIn (faderLane->getBounds()) > 300u);   // the Fader curve, handles and name
    REQUIRE (structureIn (panLane->getBounds()) > 200u);     // the Pan curve and handles
    (void) captureShellPng (image, "yesdaw-automation-stacked.png");

    shell.reset();
    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

// ADR-0053: lit, the header card's DIM paints amber and MUTE the danger red — the two words a silent or quiet
// monitor is explained by — and both still fit their pills at the default window.
TEST_CASE ("the header MASTER card lights DIM amber and MUTE red", "[ui][screenshot][g47]")
{
    juce::MessageManager::getInstance();
    const std::filesystem::path bundlePath =
        std::filesystem::temp_directory_path() / "yesdaw-ui-screenshot-monitor.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    shell->setSize (1536, 960);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));

    juce::Button& dim = requireButtonForAction (*shell, UiActionId::MasterMonitorDimToggle);
    juce::Button& mute = requireButtonForAction (*shell, UiActionId::MasterMonitorMuteToggle);
    // The pill's lower fill (its gradient settles on the button colour there), left of the word.
    const auto fillAt = [] (const juce::Image& image, const juce::Button& button) {
        return image.getPixelAt (button.getX() + 3, button.getBottom() - 3);
    };
    const auto distance = [] (juce::Colour a, juce::Colour b) {
        return std::abs (a.getRed() - b.getRed()) + std::abs (a.getGreen() - b.getGreen()) + std::abs (a.getBlue() - b.getBlue());
    };
    const auto nearest = [&distance] (juce::Colour pixel, juce::Colour expected) {
        using yesdaw::ui::UiTheme;
        for (const juce::Colour other : { UiTheme::Color::buttonSurface(), UiTheme::Color::accentAmber(), UiTheme::Color::dangerRed() })
            if (other != expected && distance (pixel, other) <= distance (pixel, expected))
                return false;
        return true;
    };
    const juce::Image unlit = renderShell (*shell);
    REQUIRE (nearest (fillAt (unlit, dim), yesdaw::ui::UiTheme::Color::buttonSurface()));
    REQUIRE (nearest (fillAt (unlit, mute), yesdaw::ui::UiTheme::Color::buttonSurface()));
    // Dispatched as a click does (a triggerClick would leave the pill in its flashed "down" paint).
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::MasterMonitorDimToggle);
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::MasterMonitorMuteToggle);
    const juce::Image lit = renderShell (*shell);
    (void) captureShellPng (lit, "yesdaw-monitor-lit.png");
    REQUIRE (nearest (fillAt (lit, dim), yesdaw::ui::UiTheme::Color::accentAmber()));
    REQUIRE (nearest (fillAt (lit, mute), yesdaw::ui::UiTheme::Color::dangerRed()));

    shell.reset();
    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

// G5.2 / ADR-0056: the Browser tab as painted — the shot is the agent's visual judgment; the mechanical part is
// that a folder row, a readable file's facts and an unreadable file's reason (in the danger colour) really paint
// inside the list's rows, and the tab fills the dock.
TEST_CASE ("G5.2 the Browser tab paints its rows, a file's facts and a refusal's reason", "[ui][screenshot][browser]")
{
    using L = yesdaw::ui::UiTheme::Layout;
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "yesdaw-g52-browser-shot";
    {
        std::error_code ec;
        std::filesystem::remove_all (root, ec);
    }
    const std::filesystem::path media = root / "Samples";
    std::filesystem::create_directories (media / "Drum Loops");
    std::filesystem::create_directories (media / "Vocals");
    {
        std::vector<float> tone (96'000);
        for (std::size_t i = 0; i < tone.size(); ++i)
            tone[i] = 0.3f * static_cast<float> (std::sin (static_cast<double> (i) * 0.05));
        REQUIRE (yesdaw::io::writeFloat32WavFile (media / "Bass DI.wav", yesdaw::engine::SampleRate { 48'000.0 }, 1, tone.size(), tone).ok());
        REQUIRE (yesdaw::io::writeFloat32WavFile (media / "Guitar Take 3.wav", yesdaw::engine::SampleRate { 44'100.0 }, 1, tone.size(), tone).ok());
        std::ofstream (media / "broken.mp3", std::ios::binary) << std::string (2'048, '\0');
    }
    const std::filesystem::path bundlePath = root / "shot.yesdaw";
    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.sessionStateDirectory = root / "session";
    std::filesystem::create_directories (choices.sessionStateDirectory);
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    shell->setSize (1920, 1080);
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ProjectNew);
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ViewBrowser);
    yesdaw::ui::mainComponentBrowserOpenFolder (*shell, media);
    const yesdaw::ui::MainComponentBrowser browser = yesdaw::ui::mainComponentBrowser (*shell);
    REQUIRE (browser.visible);
    REQUIRE (browser.names.size() == 6u);   // .., Drum Loops, Vocals, Bass DI.wav, broken.mp3, Guitar Take 3.wav
    const juce::Image image = renderShell (*shell);
    (void) captureShellPng (image, "yesdaw-g52-browser-1920x1080.png");

    const auto snapshot = yesdaw::ui::mainComponentBrowser (*shell);   // facts loaded by the paint
    REQUIRE (snapshot.facts[3].contains ("48 kHz"));
    REQUIRE (snapshot.facts[5].contains ("44.1 kHz"));
    REQUIRE (snapshot.reasons[4].isNotEmpty());
    const juce::Rectangle<int> list = snapshot.list;
    REQUIRE (list.getWidth() > 400);
    REQUIRE (list.getHeight() >= L::browserRowHeight * 6);
    const auto rowBand = [&] (int row) {
        return juce::Rectangle<int> (list.getX(), list.getY() + row * L::browserRowHeight, list.getWidth(), L::browserRowHeight);
    };
    const auto countNear = [&image] (juce::Rectangle<int> area, juce::Colour colour) {
        int count = 0;
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
            {
                const juce::Colour pixel = image.getPixelAt (x, y);
                if (std::abs (pixel.getRed() - colour.getRed()) < 40 && std::abs (pixel.getGreen() - colour.getGreen()) < 40
                    && std::abs (pixel.getBlue() - colour.getBlue()) < 40)
                    ++count;
            }
        return count;
    };
    // The name column, then the facts column right after it (not across the whole width).
    constexpr int nameRight = L::browserRowTextInset + L::browserPlayMarkWidth + L::browserNameWidth;
    const auto nameColumn = [] (juce::Rectangle<int> band) { return band.withWidth (nameRight); };
    const auto factsColumn = [] (juce::Rectangle<int> band) {
        return band.withTrimmedLeft (nameRight).withWidth (L::browserControlGap + L::browserFactsWidth);
    };
    REQUIRE (countNear (nameColumn (rowBand (1)), yesdaw::ui::UiTheme::Color::text()) > 20);          // "Drum Loops/"
    REQUIRE (countNear (factsColumn (rowBand (3)), yesdaw::ui::UiTheme::Color::mutedText()) > 20);    // Bass DI's facts
    REQUIRE (countNear (factsColumn (rowBand (4)), yesdaw::ui::UiTheme::Color::dangerRed()) > 20);    // broken.mp3's reason
    REQUIRE (countNear (factsColumn (rowBand (1)), yesdaw::ui::UiTheme::Color::dangerRed()) == 0);    // a folder has no reason
    REQUIRE (countNear (rowBand (3).withTrimmedLeft (nameRight + L::browserControlGap + L::browserFactsWidth),
                        yesdaw::ui::UiTheme::Color::mutedText()) == 0);                              // nothing far right
    // G5.2 cp2: a file row paints its play mark left of its name; a folder row has none.
    const auto playMark = [] (juce::Rectangle<int> band) {
        return band.withTrimmedLeft (L::browserRowTextInset).withWidth (L::browserPlayMarkWidth);
    };
    REQUIRE (countNear (playMark (rowBand (3)), yesdaw::ui::UiTheme::Color::mutedText()) > 10);
    REQUIRE (countNear (playMark (rowBand (1)), yesdaw::ui::UiTheme::Color::mutedText()) == 0);

    shell.reset();
    std::error_code ec;
    std::filesystem::remove_all (root, ec);
}

// G5.3 / ADR-0058: the settings row with the export options (bit depth, range, Dither, Normalize, stems) at the
// narrowest judged size — the shot is the agent's visual judgment; mechanically, each new control is laid out inside
// the window, visible, and paints its label.
TEST_CASE ("G5.3 the settings row shows the export options at 1280x720", "[ui][screenshot][export-settings]")
{
    auto shell = yesdaw::ui::createMainComponent ({});
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    shell->setSize (1280, 720);
    yesdaw::ui::mainComponentSetSettingsRowVisible (*shell, true);
    const juce::Image image = renderShell (*shell);
    (void) captureShellPng (image, "yesdaw-g53-settings-1280x720.png");
    for (const char* id : { "shell.export.dither", "shell.export.normalize", "shell.export.stems" })
    {
        juce::Component* control = nullptr;
        for (int i = 0; i < shell->getNumChildComponents() && control == nullptr; ++i)
            if (shell->getChildComponent (i)->getComponentID() == id)
                control = shell->getChildComponent (i);
        INFO (id);
        REQUIRE (control != nullptr);
        REQUIRE (control->isVisible());
        REQUIRE (shell->getLocalBounds().contains (control->getBounds()));
        int bright = 0;   // its label paints: light pixels inside its bounds
        const juce::Rectangle<int> b = control->getBounds();
        for (int y = b.getY(); y < b.getBottom(); ++y)
            for (int x = b.getX(); x < b.getRight(); ++x)
                bright += image.getPixelAt (x, y).getBrightness() > 0.6f ? 1 : 0;
        REQUIRE (bright > 15);
    }
}

// G5.5 / ADR-0060: the New Project dialog over the shell at the narrowest judged size — the shot is the agent's visual
// judgment; mechanically, the dialog sits inside the window and every control is visible and paints its label.
TEST_CASE ("G5.5 the New Project dialog at 1280x720", "[ui][screenshot][new-project]")
{
    yesdaw::ui::MainComponentFileChoices choices;
    choices.newProjectDialog = true;
    const std::filesystem::path session = std::filesystem::temp_directory_path() / "yesdaw-g55-dialog-session";
    std::filesystem::create_directories (session);
    choices.sessionStateDirectory = session;
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    shell->setSize (1280, 720);
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ProjectNew);
    const juce::Image image = renderShell (*shell);
    (void) captureShellPng (image, "yesdaw-g55-new-project-1280x720.png");
    juce::Component* dialog = nullptr;
    for (int i = 0; i < shell->getNumChildComponents() && dialog == nullptr; ++i)
        if (shell->getChildComponent (i)->getComponentID() == "newproject.dialog")
            dialog = shell->getChildComponent (i);
    REQUIRE (dialog != nullptr);
    REQUIRE (dialog->isVisible());
    REQUIRE (shell->getLocalBounds().contains (dialog->getBounds()));
    // Nothing the shell's layout raised paints over the dialog.
    for (int i = shell->getIndexOfChildComponent (dialog) + 1; i < shell->getNumChildComponents(); ++i)
    {
        juce::Component* above = shell->getChildComponent (i);
        INFO (above->getComponentID() << " " << above->getName());
        REQUIRE_FALSE ((above->isVisible() && above->getBounds().intersects (dialog->getBounds())));
    }
    for (int i = 0; i < dialog->getNumChildComponents(); ++i)
    {
        juce::Component* control = dialog->getChildComponent (i);
        INFO (control->getComponentID() << " " << control->getName());
        REQUIRE (control->isVisible());
        REQUIRE (dialog->getLocalBounds().contains (control->getBounds()));
    }
}

// ADR-0063 (G6.1 cp2): the inspector's stretch slider was the rubric's "unlabeled gain slider" — it now paints a label
// at its left and its value at its right, each legible against the card.
TEST_CASE ("ADR-0063 the inspector's stretch slider has a label and a value", "[ui][screenshot][tokens]")
{
    juce::MessageManager::getInstance();
    const std::filesystem::path bundlePath = std::filesystem::temp_directory_path() / "yesdaw-g61-stretch-label.yesdaw";
    {
        std::error_code ec;
        std::filesystem::remove_all (bundlePath, ec);
    }
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };
    yesdaw::ui::MainComponentFileChoices choices;
    choices.chooseNewProjectBundle = [bundlePath] { return bundlePath; };
    choices.chooseImportAudioFile = [fixturePath] { return fixturePath; };
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    REQUIRE (shell != nullptr);
    shell->setVisible (true);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectNew));
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    for (const auto& size : { std::pair<int, int> { 1280, 720 }, std::pair<int, int> { 1920, 1080 } })
    {
        shell->setSize (size.first, size.second);
        const juce::Image image = renderShell (*shell);
        (void) captureShellPng (image, ("yesdaw-g61-stretch-" + std::to_string (size.first) + "x" + std::to_string (size.second) + ".png").c_str());
        juce::Component* stretch = findChildWithComponentId (*shell, "clip.inspector.stretch");
        REQUIRE (stretch != nullptr);
        REQUIRE (stretch->isVisible());
        const juce::Rectangle<int> slider = stretch->getBounds();
        REQUIRE_FALSE (slider.isEmpty());
        const juce::Rectangle<int> label (slider.getX() - 60, slider.getY(), 56, slider.getHeight());
        const juce::Rectangle<int> value (slider.getRight(), slider.getY(),
                                          yesdaw::ui::UiTheme::Layout::inspectorStretchReadoutWidth, slider.getHeight());
        INFO ("at " << size.first << "x" << size.second);
        REQUIRE (maxContrastInRegion (image, label) >= 3.0);
        REQUIRE (maxContrastInRegion (image, value) >= 3.0);

        // ADR-0064: the GAIN card's caption sits inside the card's edge like its rows (it was drawn at the edge): no
        // caption ink in the card's first columns, the caption itself just after the inset.
        using L = yesdaw::ui::UiTheme::Layout;
        const int cardLeft = size.first - L::inspectorWidth + L::shellPanelHorizontalInset + L::inspectorContentInsetX;
        const int cardTop = L::headerHeight + L::shellPanelVerticalInset + L::inspectorTabHeight + L::inspectorContentInsetY
                          + L::inspectorGainSectionTop;
        const int inset = yesdaw::ui::UiTheme::Space::md;   // the law, not the token: a token of 0 must fail here
        const juce::Rectangle<int> edge (cardLeft + 1, cardTop + 4, inset - 2, L::inspectorSectionLabelHeight - 8);
        const juce::Rectangle<int> caption (cardLeft + inset, cardTop + 2, 40, L::inspectorSectionLabelHeight - 4);
        INFO ("gain caption edge " << edge.toString().toStdString() << " caption " << caption.toString().toStdString());
        REQUIRE (maxContrastInRegion (image, edge) < 1.5);
        REQUIRE (maxContrastInRegion (image, caption) >= 3.0);
    }
}
