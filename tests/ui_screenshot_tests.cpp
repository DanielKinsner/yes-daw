// YES DAW - H16 CP8 mechanical UI screenshot harness.

#include "app/SongFixture.h"
#include "engine/Project.h"
#include "ui/MainComponent.h"
#include "ui/UiIcons.h"
#include "ui/UiTheme.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <cstdint>
#include <array>
#include <filesystem>
#include <memory>
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

// V1: theme-legibility contrast law. A simple (non-gamma-corrected) relative-luminance metric is
// enough for an internal "is this text visibly distinct from its panel" gate — not a WCAG legal
// audit, just a real mechanical floor under the D7 judgment pass.
double relativeLuminance (juce::Colour colour) noexcept
{
    return 0.2126 * colour.getFloatRed() + 0.7152 * colour.getFloatGreen() + 0.0722 * colour.getFloatBlue();
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
            UiActionId::RecordingSetMonitoringPolicy,
            UiActionId::RecordingAssembleComp
        },
        juce::Rectangle<int> { 0, 0, shell->getWidth(), yesdaw::ui::mainComponentHeaderHeight (*shell) });
    yesdaw::ui::mainComponentSetSettingsRowVisible (*shell, false);

    const juce::Image timelineImage = renderShell (*shell);
    requireHonestEmptyArrangementCoverage (timelineImage, *shell);
    const std::uint64_t timelineFingerprint = captureShellPng (timelineImage, "yesdaw-timeline-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::Mixer);
    const juce::Rectangle<int> masterRegion = yesdaw::ui::mainComponentPaintedMixerMasterBounds (*shell);
    REQUIRE_FALSE (masterRegion.isEmpty());
    const juce::Image mixerImage = renderShell (*shell);
    REQUIRE (hasMixerSurfaceCoverage (mixerImage));
    REQUIRE (hasMixerMasterSummaryCoverage (mixerImage, masterRegion));
    const std::uint64_t mixerFingerprint = captureShellPng (mixerImage, "yesdaw-mixer-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
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
    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));

    // M4: the strips paint their FX chains, so the mixer capture carries a REAL chain — an empty
    // mixer would hide the very thing these screenshots are for.
    {
        auto* strips = findChildWithComponentId (*shell, "shell.mixer.strips.input");
        auto* fxChooser = dynamic_cast<juce::ComboBox*> (
            findChildWithComponentId (*shell, "mixer.fx.insert.add"));
        REQUIRE (strips != nullptr);
        REQUIRE (fxChooser != nullptr);
        mouseDownAtPoint (*strips, { strips->getWidth() / 8, strips->getHeight() / 2 });
        REQUIRE (fxChooser->isEnabled());
        fxChooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Eq) + 1,
                                  juce::sendNotificationSync);
        fxChooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Compressor) + 1,
                                  juce::sendNotificationSync);
        fxChooser->setSelectedId (static_cast<int> (yesdaw::engine::FxKind::Limiter) + 1,
                                  juce::sendNotificationSync);
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

        // M9: no mixer utility row may hang past the panel's bottom edge — a row that does not
        // fit drops to empty bounds instead of being painted half-off.
        for (const char* id : { "mixer.bus.add", "mixer.bus.remove", "mixer.track.output",
                                "mixer.send.add", "mixer.fx.insert.add" })
        {
            juce::Component* const control = controlById (id);
            if (control == nullptr)
                continue;

            INFO ("utility row " << id << " bounds " << control->getBounds().toString().toStdString());
            REQUIRE ((control->getBounds().isEmpty() || control->getBounds().getBottom() <= height));
        }

        (void) captureShellPng (image, filename);
    };

    renderAtSize (yesdaw::ui::UiTheme::Layout::windowMinWidth,
                  yesdaw::ui::UiTheme::Layout::windowMinHeight,
                  true,
                  "yesdaw-shell-min.png");
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
    STATIC_REQUIRE (L::mixerHeight == 260);   // plan §3.4 says 300: held until the inspector stack scrolls (STATUS D27)
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
        }
        INFO ("whole lanes at " << width << "x" << height << ": " << wholeLanes);
        // 720p with the 260 px mixer dock open leaves ~300 px of lanes: three whole rows (the
        // rubric records it; the dock's height is a later item's call).
        if (height >= 1080)
            REQUIRE (wholeLanes >= 8);
        else
            REQUIRE (wholeLanes >= 3);
    }

    std::error_code ec;
    std::filesystem::remove_all (fixtureDir, ec);
}
