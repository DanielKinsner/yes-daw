// YES DAW - H16 CP8 mechanical UI screenshot harness.

#include "engine/Project.h"
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

bool hasHeaderSectionHierarchy (const juce::Image& image)
{
    const auto sectionFill = yesdaw::ui::UiTheme::Color::controlInset();
    return image.getPixelAt (20, 46) == sectionFill
        && image.getPixelAt (340, 12) == sectionFill
        && image.getPixelAt (1100, 12) == sectionFill;
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

void requireHonestEmptyArrangementCoverage (const juce::Image& image)
{
    REQUIRE (hasHeaderCoverage (image));
    REQUIRE (hasHeaderSectionHierarchy (image));
    REQUIRE (sampledDifferentPixelCount (image, { 0, 88, 318, 612 }) > 20u);
    REQUIRE (sampledDifferentPixelCount (image, { 318, 88, image.getWidth() - 638, 612 }) > 100u);
    REQUIRE (sampledDifferentPixelCount (image, { image.getWidth() - 320, 88, 320, 612 }) > 10u);
    REQUIRE (sampledDifferentPixelCount (image, { 0, image.getHeight() - 260, image.getWidth(), 260 }) > 40u);
}

bool hasMixerSurfaceCoverage (const juce::Image& image)
{
    return hasHeaderCoverage (image)
        && sampledDifferentPixelCount (image, { 0, 88, 180, image.getHeight() - 88 }) > 80u
        && sampledDifferentPixelCount (image,
                                       { 180, 88, image.getWidth() - 180, image.getHeight() - 88 })
               > 300u;
}

bool hasMixerMasterSummaryCoverage (const juce::Image& image)
{
    const int regionWidth = 104;
    const juce::Rectangle<int> masterRegion {
        image.getWidth() - regionWidth - yesdaw::ui::UiTheme::Layout::mixerPanelHorizontalInset,
        yesdaw::ui::UiTheme::Layout::headerHeight
            + yesdaw::ui::UiTheme::Layout::mixerPanelVerticalInset,
        regionWidth,
        image.getHeight() - yesdaw::ui::UiTheme::Layout::headerHeight
            - 2 * yesdaw::ui::UiTheme::Layout::mixerPanelVerticalInset
    };
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
    // The device + recording cluster lives in header row 3 (it used to float over the track rail
    // and collide with the TRACKS strip — the layout law moved it inside the header).
    requireDisjointActionBounds (
        *shell,
        std::array {
            UiActionId::DeviceRefreshAudio,
            UiActionId::DeviceSelectTestAudio,
            UiActionId::RecordingArmTrack,
            UiActionId::RecordingSetMonitoringPolicy,
            UiActionId::RecordingAssembleComp
        },
        juce::Rectangle<int> { 0, 0, shell->getWidth(), yesdaw::ui::UiTheme::Layout::headerHeight });

    const juce::Image timelineImage = renderShell (*shell);
    requireHonestEmptyArrangementCoverage (timelineImage);
    const std::uint64_t timelineFingerprint = captureShellPng (timelineImage, "yesdaw-timeline-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewMixer));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::Mixer);
    const juce::Image mixerImage = renderShell (*shell);
    REQUIRE (hasMixerSurfaceCoverage (mixerImage));
    REQUIRE (hasMixerMasterSummaryCoverage (mixerImage));
    const std::uint64_t mixerFingerprint = captureShellPng (mixerImage, "yesdaw-mixer-shell.png");

    clickButton (requireButtonForAction (*shell, UiActionId::ViewPianoRoll));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*shell).context.activePanel == UiPanel::PianoRoll);
    const juce::Image pianoRollImage = renderShell (*shell);
    requireHonestEmptyArrangementCoverage (pianoRollImage);
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
        const int rowHeight = juce::jmax (
            yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
            (rail->getHeight() - yesdaw::ui::UiTheme::Layout::trackListHeaderHeight) / rowCount);
        const juce::Point<int> point { rail->getWidth() / 2,
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
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    selectRailRow (1, 2);
    clickButton (requireButtonForAction (*shell, UiActionId::ProjectImportAudio));
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));
    selectRailRow (2, 3);
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));   // back to the Timeline view
    REQUIRE (shell->keyPressed (juce::KeyPress ('m')));   // marker at the playhead

    const yesdaw::ui::MainComponentSnapshot content = yesdaw::ui::snapshotMainComponent (*shell);
    REQUIRE (content.context.projectLoaded);
    REQUIRE (content.visibleTimelineTrackCount == 3);
    REQUIRE (content.visibleTimelineClipCount >= 2);
    REQUIRE (content.context.activePanel == UiPanel::Timeline);

    const auto renderAtSize = [&shell] (int width, int height, const char* filename)
    {
        shell->setSize (width, height);
        const juce::Image image = renderShell (*shell);
        REQUIRE (image.getWidth() == width);
        REQUIRE (image.getHeight() == height);
        // Size-relative honesty: the header row, the rail column, the arrangement body, and
        // the bottom section all paint real structure at EVERY size.
        REQUIRE (sampledDifferentPixelCount (image, { 0, 0, width, 88 }) > 60u);
        REQUIRE (sampledDifferentPixelCount (image, { 0, 88, 318, height - 348 }) > 20u);
        REQUIRE (sampledDifferentPixelCount (image, { 318, 88, width - 638, height - 348 }) > 100u);
        REQUIRE (sampledDifferentPixelCount (image, { 0, height - 260, width, 260 }) > 40u);
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
    REQUIRE (shell->keyPressed (juce::KeyPress ('m', juce::ModifierKeys::ctrlModifier, 0)));

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
    REQUIRE (shell->keyPressed (juce::KeyPress ('p')));
    const juce::Rectangle<int> grid = pencilGrid();
    for (const auto& [fx, fy] : { std::pair { 0.08, 0.62 }, { 0.22, 0.55 }, { 0.36, 0.48 },
                                  { 0.52, 0.55 }, { 0.68, 0.42 }, { 0.84, 0.35 } })
        mouseDownUpAt (*pianoRoll,
                       { grid.getX() + juce::roundToInt (grid.getWidth() * fx),
                         grid.getY() + juce::roundToInt (grid.getHeight() * fy) });
    REQUIRE (shell->keyPressed (juce::KeyPress ('v')));

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
        REQUIRE (sampledDifferentPixelCount (image, keys) > 20u);
        REQUIRE (sampledDifferentPixelCount (image, local) > 60u);
        REQUIRE (sampledDifferentPixelCount (image, expression) > 10u);
        (void) captureShellPng (image, filename);
    };

    renderRollAtSize (1152, 720, "yesdaw-roll-laptop.png");
    renderRollAtSize (1536, 960, "yesdaw-roll-default.png");
    renderRollAtSize (1920, 1080, "yesdaw-roll-large.png");

    // The automation lane, open on the timeline with real breakpoints clicked into the canvas.
    REQUIRE (shell->keyPressed (juce::KeyPress ('1')));
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
    REQUIRE (shell->keyPressed (juce::KeyPress ('t', juce::ModifierKeys::ctrlModifier, 0)));

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
        REQUIRE (sampledDifferentPixelCount (image, { 0, 88, 318, height - 348 }) > 20u);
        REQUIRE (sampledDifferentPixelCount (image, { 318, 88, width - 638, height - 348 }) > 60u);
        REQUIRE (sampledDifferentPixelCount (image, { 0, height - 260, width, 260 }) > 40u);
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

    clickButton (requireButtonForAction (*shell, UiActionId::DeviceSelectTestAudio));
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
    REQUIRE_FALSE (hasMixerMasterSummaryCoverage (blank));
    REQUIRE_FALSE (hasHeaderSectionHierarchy (blank));
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
