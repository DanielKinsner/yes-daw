// YES DAW - H11 app shell.
//
// The visible JUCE shell and the headless tests share ui/UiActions.h. This checkpoint keeps the shell
// image-light and model-backed: later H11 slices wire Project loading, transport, timeline drawing,
// accessibility traversal, and editing through the same action IDs.

#include "ui/MainComponent.h"
#include "ui/UiIcons.h"
#include "engine/Time.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiAppModel.h"
#include "ui/UiMixerSurface.h"
#include "ui/UiPianoRollSurface.h"
#include "ui/UiTheme.h"
#include "ui/YesDawLookAndFeel.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kHeaderHeight = yesdaw::ui::UiTheme::Layout::headerHeight;
constexpr int kLeftRailWidth = yesdaw::ui::UiTheme::Layout::leftRailWidth;
constexpr int kInspectorWidth = yesdaw::ui::UiTheme::Layout::inspectorWidth;
constexpr int kMixerHeight = yesdaw::ui::UiTheme::Layout::mixerHeight;
constexpr int kUiRefreshIntervalMs = 33;
// Open Recent menu item ids live above the action-id range (B39).
constexpr int kRecentMenuBaseId = 1000;

// The menu bar names its menus itself; the tooltip mixin satisfies the every-control law (B40).
class TooltippedMenuBar final : public juce::MenuBarComponent,
                                public juce::SettableTooltipClient
{
public:
    using juce::MenuBarComponent::MenuBarComponent;
};

constexpr yesdaw::engine::Tick kPianoRollSnapGridTicks =
    yesdaw::ui::UiTheme::Layout::pianoRollGridTickStep;
constexpr const char* kTimelineComponentId = "timeline.canvas";
constexpr std::array<std::pair<std::uint16_t, std::uint16_t>, 6> kHeaderMeterChoices {{
    { 4, 4 }, { 3, 4 }, { 6, 8 }, { 2, 4 }, { 5, 4 }, { 7, 8 }
}};
constexpr const char* kPianoRollComponentId = "piano-roll.canvas";
constexpr const char* kInspectorStartComponentId = "clip.inspector.start";
constexpr const char* kInspectorEndComponentId = "clip.inspector.end";
constexpr const char* kInspectorLengthComponentId = "clip.inspector.length";
constexpr const char* kInspectorFadeInComponentId = "clip.inspector.fade_in";
constexpr const char* kInspectorFadeOutComponentId = "clip.inspector.fade_out";
constexpr const char* kInspectorFadeCurveComponentId = "clip.inspector.fade_curve";
constexpr const char* kAutomationLaneRowComponentId = "timeline.automation.track.0.lane";
constexpr const char* kExportAudioProgressComponentId = "project.export_audio.progress";
constexpr int kInspectorEqualPowerFadeCurveId = 1;

const juce::Colour kBackground = yesdaw::ui::UiTheme::Color::appBackground();
const juce::Colour kPanel = yesdaw::ui::UiTheme::Color::panel();
const juce::Colour kPanelRaised = yesdaw::ui::UiTheme::Color::panelRaised();
const juce::Colour kPanelStroke = yesdaw::ui::UiTheme::Color::panelStroke();
const juce::Colour kText = yesdaw::ui::UiTheme::Color::text();
const juce::Colour kMutedText = yesdaw::ui::UiTheme::Color::mutedText();
const juce::Colour kBlue = yesdaw::ui::UiTheme::Color::accentBlue();
const juce::Colour kTeal = yesdaw::ui::UiTheme::Color::accentTeal();
const juce::Colour kAmber = yesdaw::ui::UiTheme::Color::accentAmber();
const juce::Colour kPurple = yesdaw::ui::UiTheme::Color::accentPurple();
const juce::Colour kCyan = yesdaw::ui::UiTheme::Color::accentCyan();
const juce::Colour kRed = yesdaw::ui::UiTheme::Color::dangerRed();

using TrackRow = yesdaw::ui::TimelineCanvasTrack;
using TimelineClipStyle = yesdaw::ui::TimelineCanvasClipStyle;

constexpr bool isBlackMidiKey (int key) noexcept
{
    const int octaveKey = key % 12;
    return octaveKey == 1 || octaveKey == 3 || octaveKey == 6 || octaveKey == 8 || octaveKey == 10;
}

juce::String actionButtonText (yesdaw::ui::UiActionId id)
{
    switch (id)
    {
        case yesdaw::ui::UiActionId::ProjectNew: return "New";
        case yesdaw::ui::UiActionId::ProjectOpen: return "Open";
        case yesdaw::ui::UiActionId::ProjectSave: return "Save";
        case yesdaw::ui::UiActionId::ProjectImportAudio: return "Import";
        case yesdaw::ui::UiActionId::EditUndo: return "Undo";
        case yesdaw::ui::UiActionId::EditRedo: return "Redo";
        case yesdaw::ui::UiActionId::TransportPlay: return "Play";
        case yesdaw::ui::UiActionId::TransportStop: return "Stop";
        case yesdaw::ui::UiActionId::TransportLocateStart: return "|<";
        case yesdaw::ui::UiActionId::TransportToggleLoop: return "Loop";
        case yesdaw::ui::UiActionId::DeviceRefreshAudio: return "Refresh";
        case yesdaw::ui::UiActionId::DeviceSelectTestAudio: return "Test Device";
        case yesdaw::ui::UiActionId::RecordingArmTrack: return "Arm";
        case yesdaw::ui::UiActionId::RecordingSetMonitoringPolicy: return "Monitor";
        case yesdaw::ui::UiActionId::TransportRecord: return "Record";
        case yesdaw::ui::UiActionId::RecordingAssembleComp: return "Comp";
        case yesdaw::ui::UiActionId::AutosaveRecoveryRestore: return "Restore Autosave";
        case yesdaw::ui::UiActionId::AutosaveRecoveryDiscard: return "Discard Autosave";
        case yesdaw::ui::UiActionId::ViewMixer: return "Mixer";
        case yesdaw::ui::UiActionId::ViewPianoRoll: return "Piano";
        default: break;
    }
    return "?";
}

constexpr bool toolbarActionRequiresPlayback (yesdaw::ui::UiActionId id) noexcept
{
    return id == yesdaw::ui::UiActionId::TransportPlay
        || id == yesdaw::ui::UiActionId::TransportStop
        || id == yesdaw::ui::UiActionId::TransportLocateStart
        || id == yesdaw::ui::UiActionId::TransportToggleLoop;
}

void fillPanel (juce::Graphics& g,
                juce::Rectangle<int> area,
                float radius = yesdaw::ui::UiTheme::Radius::lg)
{
    g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
        yesdaw::ui::UiTheme::Tone::shadowAlpha));
    g.fillRoundedRectangle (
        area.toFloat().translated (
            0.0f,
            static_cast<float> (yesdaw::ui::UiTheme::Layout::controlShadowOffset)),
        radius);
    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat(), radius);
    g.setColour (kPanelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (yesdaw::ui::UiTheme::Layout::panelOutlineInset),
                            radius,
                            yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
    g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
        yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
    g.drawHorizontalLine (
        area.getY() + yesdaw::ui::UiTheme::Layout::controlInnerHighlightHeight,
        static_cast<float> (area.getX()) + radius,
        static_cast<float> (area.getRight()) - radius);
}

void drawSmallLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                     juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour (kMutedText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
    g.drawText (text, area, justification, false);
}

void drawMeter (juce::Graphics& g, juce::Rectangle<int> area, float value)
{
    g.setColour (yesdaw::ui::UiTheme::Color::meterTrack());
    g.fillRoundedRectangle (area.toFloat(), yesdaw::ui::UiTheme::Radius::xs);

    const auto fill = area.reduced (yesdaw::ui::UiTheme::Layout::meterFillInset);
    const int height = juce::roundToInt (
        static_cast<float> (fill.getHeight()) * juce::jlimit (0.0f, 1.0f, value));
    const int liveTop = fill.getBottom() - height;
    const int hotBottom = liveTop + juce::roundToInt (
        static_cast<float> (height) * yesdaw::ui::UiTheme::Meter::verticalHotBand);
    const int segmentStep = yesdaw::ui::UiTheme::Layout::meterSegmentSize
                          + yesdaw::ui::UiTheme::Layout::meterSegmentGap;
    for (int bottom = fill.getBottom(); bottom > liveTop; bottom -= segmentStep)
    {
        const int top = juce::jmax (liveTop,
                                    bottom - yesdaw::ui::UiTheme::Layout::meterSegmentSize);
        g.setColour (top < hotBottom ? yesdaw::ui::UiTheme::Meter::hotFill()
                                    : yesdaw::ui::UiTheme::Meter::nominalFill());
        g.fillRect (fill.getX(), top, fill.getWidth(), bottom - top);
    }
}

// B32: vertical meter with a peak-hold marker and a latched clip light. The live bar paints
// exactly like drawMeter; the held peak paints as a marker line while its hold lasts; the clip
// light fills the meter's top cell while latched.
void drawMeterWithHold (juce::Graphics& g,
                        juce::Rectangle<int> area,
                        float liveValue,
                        float heldValue,
                        bool clipLatched)
{
    drawMeter (g, area, liveValue);

    const auto fill = area.reduced (yesdaw::ui::UiTheme::Layout::meterFillInset);
    if (heldValue > 0.0f)
    {
        const int heldHeight = juce::roundToInt (
            static_cast<float> (fill.getHeight()) * juce::jlimit (0.0f, 1.0f, heldValue));
        g.setColour (yesdaw::ui::UiTheme::Meter::hotFill());
        g.fillRect (fill.getX(),
                    fill.getBottom() - heldHeight,
                    fill.getWidth(),
                    yesdaw::ui::UiTheme::Meter::peakTickThickness);
    }

    if (clipLatched)
    {
        g.setColour (yesdaw::ui::UiTheme::Meter::clipFill());
        g.fillRect (fill.getX(), area.getY(), fill.getWidth(),
                    yesdaw::ui::UiTheme::Meter::clipLightSize);
    }
}

void drawHorizontalMeter (juce::Graphics& g, juce::Rectangle<int> area, float value)
{
    g.setColour (yesdaw::ui::UiTheme::Color::meterTrack());
    g.fillRoundedRectangle (area.toFloat(), yesdaw::ui::UiTheme::Radius::xs);
    const auto fill = area.reduced (yesdaw::ui::UiTheme::Layout::meterFillInset);
    const int width = juce::roundToInt (
        static_cast<float> (fill.getWidth()) * juce::jlimit (0.0f, 1.0f, value));
    const int liveRight = fill.getX() + width;
    const int hotLeft = liveRight - juce::roundToInt (
        static_cast<float> (width) * yesdaw::ui::UiTheme::Meter::horizontalHotBand);
    const int segmentStep = yesdaw::ui::UiTheme::Layout::meterSegmentSize
                          + yesdaw::ui::UiTheme::Layout::meterSegmentGap;
    for (int left = fill.getX(); left < liveRight; left += segmentStep)
    {
        const int right = juce::jmin (liveRight,
                                      left + yesdaw::ui::UiTheme::Layout::meterSegmentSize);
        g.setColour (left >= hotLeft ? yesdaw::ui::UiTheme::Meter::hotFill()
                                    : yesdaw::ui::UiTheme::Meter::nominalFill());
        g.fillRect (left, fill.getY(), right - left, fill.getHeight());
    }
}

juce::Colour stripColourForIndex (std::size_t index)
{
    const std::array colours { kBlue, kTeal, kAmber, kPurple, kCyan };
    return colours[index % colours.size()];
}

// Translate a JUCE KeyPress into the keymap's chord vocabulary ("Ctrl+Alt+Shift+B", "Space", "Del",
// "F2", "Ctrl+/"). Modifier order matches the descriptor table: Ctrl, Alt, Shift.
std::string chordForKeyPress (const juce::KeyPress& key)
{
    std::string chord;
    const juce::ModifierKeys mods = key.getModifiers();
    const bool isShiftedPlus = key.getTextCharacter() == '+';
    if (mods.isCtrlDown() || mods.isCommandDown())
        chord += "Ctrl+";
    if (mods.isAltDown())
        chord += "Alt+";
    if (mods.isShiftDown() && ! isShiftedPlus)
        chord += "Shift+";

    const int code = key.getKeyCode();
    if (isShiftedPlus)
        chord += "+";
    else if (code == juce::KeyPress::spaceKey)
        chord += "Space";
    else if (code == juce::KeyPress::homeKey)
        chord += "Home";
    else if (code == juce::KeyPress::returnKey)
        chord += "Enter";
    else if (code == juce::KeyPress::upKey)
        chord += "Up";
    else if (code == juce::KeyPress::downKey)
        chord += "Down";
    else if (code == juce::KeyPress::leftKey)
        chord += "Left";
    else if (code == juce::KeyPress::rightKey)
        chord += "Right";
    else if (code == juce::KeyPress::escapeKey)
        chord += "Esc";
    else if (code == juce::KeyPress::deleteKey)
        chord += "Del";
    else if (code == juce::KeyPress::backspaceKey)
        chord += "Backspace";
    else if (code >= juce::KeyPress::F1Key && code <= juce::KeyPress::F12Key)
        chord += "F" + std::to_string (1 + code - juce::KeyPress::F1Key);
    else if (code > 32 && code < 127)
        chord += static_cast<char> (juce::CharacterFunctions::toUpperCase (static_cast<juce::juce_wchar> (code)));
    else
        return {};   // unmapped key: no chord, no dispatch

    return chord;
}

juce::File juceFileFromPath (const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return juce::File { juce::String::fromUTF8 (
        reinterpret_cast<const char*> (utf8.data()),
        static_cast<int> (utf8.size())) };
}

// Decode a mono or stereo WAV into an interleaved UiDecodedAsset (ADR-0042). Wider-than-stereo files
// are rejected — never silently downmixed.
std::optional<yesdaw::ui::UiDecodedAsset> decodeProjectWav (const std::filesystem::path& sourcePath)
{
    juce::WavAudioFormat wav;
    const juce::File file = juceFileFromPath (sourcePath);
    std::unique_ptr<juce::AudioFormatReader> reader (
        wav.createReaderFor (new juce::FileInputStream (file), true));
    if (reader == nullptr)
        return std::nullopt;

    if (reader->sampleRate <= 0.0
        || reader->numChannels < 1u
        || reader->numChannels > 2u
        || reader->lengthInSamples <= 0
        || reader->lengthInSamples > static_cast<juce::int64> (std::numeric_limits<int>::max()))
        return std::nullopt;

    const int frames = static_cast<int> (reader->lengthInSamples);
    const int channels = static_cast<int> (reader->numChannels);
    juce::AudioBuffer<float> decodedBuffer (channels, frames);
    if (! reader->read (&decodedBuffer, 0, frames, 0, true, channels > 1))
        return std::nullopt;

    yesdaw::ui::UiDecodedAsset decoded;
    decoded.sampleRate = yesdaw::engine::SampleRate { reader->sampleRate };
    decoded.frames = static_cast<std::uint64_t> (frames);
    decoded.channels = static_cast<std::uint16_t> (channels);
    decoded.interleavedSamples.resize (static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels));
    for (int channel = 0; channel < channels; ++channel)
    {
        const float* const source = decodedBuffer.getReadPointer (channel);
        for (int frame = 0; frame < frames; ++frame)
            decoded.interleavedSamples[static_cast<std::size_t> (frame) * static_cast<std::size_t> (channels)
                                       + static_cast<std::size_t> (channel)] = source[frame];
    }
    return decoded;
}

std::optional<std::vector<yesdaw::ui::UiDecodedAsset>> decodeStoredProjectAssets (
    const std::filesystem::path& bundlePath)
{
    yesdaw::persistence::ProjectBundleDb db;
    if (! yesdaw::persistence::ProjectBundleDb::openExistingBundle (bundlePath, db).ok())
        return std::nullopt;

    yesdaw::engine::Project project;
    if (! db.readProjectSnapshot (project).ok())
        return std::nullopt;

    std::vector<yesdaw::ui::UiDecodedAsset> decodedAssets;
    decodedAssets.reserve (project.assets.size());
    for (const yesdaw::engine::Asset& asset : project.assets)
    {
        auto decoded = decodeProjectWav (
            yesdaw::persistence::storedAssetPathForHash (bundlePath, asset.contentHash));
        if (! decoded
            || decoded->frames != asset.frames
            || decoded->sampleRate != asset.sampleRate
            || decoded->channels != asset.channels)
            return std::nullopt;

        decoded->assetId = asset.id;
        decodedAssets.push_back (std::move (*decoded));
    }

    return decodedAssets;
}

} // namespace

class TimelineInputComponent final : public juce::Component,
                                     public juce::SettableTooltipClient
{
public:
    std::function<yesdaw::ui::TimelineCanvasState()> stateProvider;
    std::function<yesdaw::ui::TimelineTool()> activeToolProvider;
    std::function<void (int, bool)> onClipClicked;
    std::function<void()> onEmptyClicked;
    std::function<void (std::span<const int>)> onMarqueeSelection;
    std::function<void (int, double, bool)> onClipMoved;
    std::function<void (int, int, double, bool)> onClipMovedToLane;   // layoutClipId, targetLane, startSeconds, snap
    std::function<void (int, int, double, bool)> onClipCopied;        // layoutClipId, targetLane (-1 = same), startSeconds, snap
    // Time-gestures carry the gesture's Ctrl flag so the shell can apply the snap chooser with
    // Ctrl inversion (E4); fades are durations and stay honestly unsnapped.
    std::function<void (int, double, bool)> onClipSplit;         // layoutClipId, seconds, snapInvert
    std::function<void (int, double, bool)> onClipTrimmedRight;  // layoutClipId, seconds, snapInvert
    std::function<void (int, double, bool)> onClipTrimmedLeft;   // layoutClipId, seconds, snapInvert
    std::function<void (int, int)> onClipGainAdjusted;
    std::function<void (int, bool, double)> onClipFadeAdjusted;
    std::function<void (double)> onTimelineLocated;
    std::function<void (double, double, bool)> onLoopRegionDragged;   // startSeconds, endSeconds, snapInvert
    std::function<void (double, double, bool)> onRulerRangeSelected;  // startSeconds, endSeconds, snapInvert (plain drag)
    std::function<void()> onRulerRangeCleared;                   // plain ruler click collapses the range
    std::function<void (double, double)> onZoomWheel;            // anchorSeconds, wheelDelta
    std::function<void (double)> onRulerAltClicked;              // seconds: remove nearest marker
    std::function<void (double)> onScrollWheel;                  // wheelDelta (view-widths per notch)
    std::function<void (double, bool)> onZoomToolClicked;        // anchorSeconds, zoomOut (Alt) — E3
    std::function<void (double)> onHandToolScrolled;             // secondsDelta from a Hand drag — E3
    std::function<void (int, double)> onPencilEmptyLane;         // lane, seconds: pencil a MIDI clip — E3
    std::function<void (int)> onVerticalScrollRows;              // +1 down / -1 up, plain wheel — E5

    [[nodiscard]] bool cancelInProgressEdit()
    {
        if (! dragState.active && ! marqueeState.active && ! rulerRangeDragActive && ! handDragActive)
            return false;

        dragState = {};
        marqueeState = {};
        rulerRangeDragActive = false;
        handDragActive = false;
        repaint();
        return true;
    }

    void paint (juce::Graphics& g) override
    {
        if (stateProvider)
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            (void) yesdaw::ui::paintTimelineCanvas (g, getLocalBounds(), state);
            if (state.trackCount == 0 && state.clipCount == 0)
            {
                const auto geometry = yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
                g.setColour (kText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (
                    yesdaw::ui::UiTheme::Type::title,
                    juce::Font::bold));
                const int centreY = geometry.clipArea.getCentreY();
                g.drawText ("Create or open a Project",
                            juce::Rectangle<int> { geometry.clipArea.getX(), centreY - 30,
                                                   geometry.clipArea.getWidth(), 24 },
                            juce::Justification::centred,
                            false);
                g.setColour (kMutedText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::body));
                g.drawText ("Use New or Open in the top-left toolbar",
                            juce::Rectangle<int> { geometry.clipArea.getX(), centreY + 2,
                                                   geometry.clipArea.getWidth(), 20 },
                            juce::Justification::centred,
                            false);
            }

            if (marqueeState.active)
            {
                const auto marquee = marqueeBounds().getIntersection (
                    yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state).clipArea);
                g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
                    yesdaw::ui::UiTheme::Tone::pressedHighlightAlpha));
                g.fillRect (marquee);
                g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
                    yesdaw::ui::UiTheme::Tone::focusRingAlpha));
                g.drawRect (marquee.toFloat(), yesdaw::ui::UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
            }

            if (rulerRangeDragActive)
            {
                const auto geometry = yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
                const int left = std::max (std::min (rulerRangeDownPosition.x, rulerRangeCurrentPosition.x),
                                           geometry.clipArea.getX());
                const int right = std::min (std::max (rulerRangeDownPosition.x, rulerRangeCurrentPosition.x),
                                            geometry.clipArea.getRight());
                if (right > left)
                {
                    const juce::Rectangle<int> band { left, geometry.rulerArea.getY(), right - left,
                                                      geometry.clipArea.getBottom() - geometry.rulerArea.getY() };
                    g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
                        yesdaw::ui::UiTheme::Tone::pressedHighlightAlpha));
                    g.fillRect (band);
                    g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
                        yesdaw::ui::UiTheme::Tone::focusRingAlpha));
                    g.drawRect (band.toFloat(), yesdaw::ui::UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        playheadLocateActive = false;
        rulerRangeDragActive = false;
        handDragActive = false;
        marqueeState = {};
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, event.getPosition());

        // The tool palette owns the clip-area gesture (E3): Hand pans, Zoom clicks zoom, Scissors
        // splits the hit clip, Pencil creates a MIDI clip on the clicked empty lane (or just
        // selects a hit clip). The ruler keeps its locate/loop/range behavior for every tool, and
        // Pointer keeps the full historical gesture map below.
        const yesdaw::ui::TimelineTool tool =
            activeToolProvider ? activeToolProvider() : yesdaw::ui::TimelineTool::Pointer;
        if (tool != yesdaw::ui::TimelineTool::Pointer)
        {
            const yesdaw::ui::TimelineCanvasGeometry toolGeometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            if (toolGeometry.clipArea.contains (event.getPosition()))
            {
                if (tool == yesdaw::ui::TimelineTool::Hand)
                {
                    handDragActive = true;
                    handDragLastX = event.getPosition().x;
                    return;
                }

                if (tool == yesdaw::ui::TimelineTool::Zoom)
                {
                    if (const std::optional<double> seconds =
                            timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                        if (onZoomToolClicked)
                            onZoomToolClicked (*seconds, event.mods.isAltDown());
                    return;
                }

                if (tool == yesdaw::ui::TimelineTool::Scissors)
                {
                    if (hit.hit)
                        if (const std::optional<double> seconds =
                                timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                            if (onClipSplit)
                                onClipSplit (hit.id, *seconds, event.mods.isCtrlDown());
                    return;
                }

                if (tool == yesdaw::ui::TimelineTool::Pencil)
                {
                    if (hit.hit)
                    {
                        if (onClipClicked)
                            onClipClicked (hit.id, event.mods.isShiftDown());
                        return;
                    }

                    if (state.trackCount > 0 && toolGeometry.laneHeight > 0)
                    {
                        const int lane = std::clamp (
                            (event.getPosition().y - toolGeometry.clipArea.getY()
                             + juce::roundToInt (toolGeometry.viewport.laneScrollPixels))
                                / toolGeometry.laneHeight,
                            0, state.trackCount - 1);
                        if (const std::optional<double> seconds =
                                timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                            if (onPencilEmptyLane)
                                onPencilEmptyLane (lane, *seconds);
                    }
                    return;
                }
            }
        }

        if (hit.hit)
        {
            if (onClipClicked)
                onClipClicked (hit.id, event.mods.isShiftDown());

            dragState = {};
            dragState.active = true;
            dragState.layoutClipId = hit.id;
            dragState.downPosition = event.getPosition();
            dragState.mode = dragModeForPointer (state, getLocalBounds(), hit.id, event.getPosition(), event.mods);
            dragState.copy = event.mods.isAltDown()
                          && (dragState.mode == TimelineDragMode::Move
                              || dragState.mode == TimelineDragMode::SnapMove);
            if (const yesdaw::ui::Clip* clip = findClipByLayoutId (state, hit.id))
            {
                dragState.startSeconds = clip->startSeconds;
                dragState.lengthSeconds = clip->lengthSeconds;
            }
            return;
        }

        dragState = {};
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        if (geometry.rulerArea.contains (event.getPosition()))
        {
            // Alt+click removes the nearest marker; Shift-drag defines a loop region; a plain click
            // locates while a plain drag selects a painted time range (parity item 25).
            if (event.mods.isAltDown())
            {
                if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                    if (onRulerAltClicked)
                        onRulerAltClicked (*seconds);
                return;
            }

            if (event.mods.isShiftDown())
            {
                loopDragActive = true;
                loopDragStartSeconds =
                    timelineSecondsAt (state, getLocalBounds(), event.getPosition()).value_or (0.0);
                return;
            }

            playheadLocateActive = true;
            rulerRangeDownPosition = event.getPosition();
            rulerRangeCurrentPosition = event.getPosition();
            rulerRangeDragStartSeconds =
                timelineSecondsAt (state, getLocalBounds(), event.getPosition()).value_or (0.0);
            if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                if (onTimelineLocated)
                    onTimelineLocated (*seconds);
            return;
        }

        playheadLocateActive = false;
        if (geometry.clipArea.contains (event.getPosition()))
        {
            marqueeState.active = true;
            marqueeState.downPosition = event.getPosition();
            marqueeState.currentPosition = event.getPosition();
            if (onEmptyClicked)
                onEmptyClicked();
            repaint();
            return;
        }

        if (onEmptyClicked)
            onEmptyClicked();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (marqueeState.active && stateProvider)
        {
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), stateProvider());
            marqueeState.currentPosition = geometry.clipArea.getConstrainedPoint (event.getPosition());
            const int deltaX = marqueeState.currentPosition.x - marqueeState.downPosition.x;
            const int deltaY = marqueeState.currentPosition.y - marqueeState.downPosition.y;
            marqueeState.moved = std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                              || std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            repaint();
            return;
        }

        if (playheadLocateActive && stateProvider)
        {
            // A plain ruler drag past the dead zone becomes a range selection; the playhead stays at
            // the mouse-down locate instead of scrubbing (parity item 25).
            const int deltaX = event.getPosition().x - rulerRangeDownPosition.x;
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            playheadLocateActive = false;
            rulerRangeDragActive = true;
        }

        if (rulerRangeDragActive)
        {
            rulerRangeCurrentPosition = event.getPosition();
            repaint();
            return;
        }

        if (handDragActive && stateProvider)
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            const double pixelsPerSecond = std::max (
                yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
                geometry.viewport.pixelsPerSecond);
            const int deltaX = event.getPosition().x - handDragLastX;
            handDragLastX = event.getPosition().x;
            if (deltaX != 0 && onHandToolScrolled)
                onHandToolScrolled (-static_cast<double> (deltaX) / pixelsPerSecond);
            return;
        }

        if (dragState.active)
            dragState.moved = true;
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (handDragActive)
        {
            handDragActive = false;
            return;
        }

        if (marqueeState.active)
        {
            const TimelineMarqueeState marquee = marqueeState;
            marqueeState = {};
            repaint();

            if (marquee.moved && stateProvider && onMarqueeSelection)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                std::array<int, yesdaw::ui::UiTheme::Layout::timelineCanvasVisibleClipCapacity> selectedIds {};
                const int selectedCount = clipIdsIntersectingMarquee (
                    getLocalBounds(), state, marqueeBounds (marquee), selectedIds.data(),
                    static_cast<int> (selectedIds.size()));
                onMarqueeSelection (std::span<const int> (selectedIds.data(),
                                                         static_cast<std::size_t> (selectedCount)));
            }
            return;
        }

        if (loopDragActive)
        {
            loopDragActive = false;
            if (stateProvider && onLoopRegionDragged)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                if (const std::optional<double> endSeconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    const double first = std::min (loopDragStartSeconds, *endSeconds);
                    const double second = std::max (loopDragStartSeconds, *endSeconds);
                    if (second > first)
                        onLoopRegionDragged (first, second, event.mods.isCtrlDown());
                }
            }
            return;
        }

        if (rulerRangeDragActive)
        {
            rulerRangeDragActive = false;
            repaint();
            if (stateProvider && onRulerRangeSelected)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                if (const std::optional<double> endSeconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    const double first = std::min (rulerRangeDragStartSeconds, *endSeconds);
                    const double second = std::max (rulerRangeDragStartSeconds, *endSeconds);
                    if (second > first)
                        onRulerRangeSelected (first, second, event.mods.isCtrlDown());
                }
            }
            return;
        }

        if (playheadLocateActive)
        {
            playheadLocateActive = false;
            // A plain ruler click collapses any committed range selection (the locate already
            // happened on mouse-down).
            if (onRulerRangeCleared)
                onRulerRangeCleared();
            return;
        }

        if (! dragState.active)
            return;

        const TimelineDragState drag = dragState;
        dragState = {};

        const int deltaX = event.getPosition().x - drag.downPosition.x;
        const int deltaY = event.getPosition().y - drag.downPosition.y;
        if (! drag.moved || ! stateProvider)
            return;

        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const std::optional<double> eventSeconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition());
        if (drag.mode == TimelineDragMode::TrimRight)
        {
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            if (eventSeconds)
                if (onClipTrimmedRight)
                    onClipTrimmedRight (drag.layoutClipId, *eventSeconds, event.mods.isCtrlDown());
            return;
        }

        if (drag.mode == TimelineDragMode::TrimLeft)
        {
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            if (eventSeconds)
                if (onClipTrimmedLeft)
                    onClipTrimmedLeft (drag.layoutClipId, *eventSeconds, event.mods.isCtrlDown());
            return;
        }

        if (drag.mode == TimelineDragMode::Gain)
        {
            if (std::abs (deltaY) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            if (onClipGainAdjusted)
                onClipGainAdjusted (drag.layoutClipId, -deltaY);
            return;
        }

        if (drag.mode == TimelineDragMode::FadeIn || drag.mode == TimelineDragMode::FadeOut)
        {
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels || ! eventSeconds)
                return;

            const double fadeSeconds = drag.mode == TimelineDragMode::FadeIn
                ? *eventSeconds - drag.startSeconds
                : (drag.startSeconds + drag.lengthSeconds) - *eventSeconds;

            if (onClipFadeAdjusted)
                onClipFadeAdjusted (
                    drag.layoutClipId,
                    drag.mode == TimelineDragMode::FadeIn,
                    std::clamp (fadeSeconds, 0.0, drag.lengthSeconds));
            return;
        }

        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);

        // A Move drag is two-dimensional: horizontal drag repositions in time, and a vertical drag past
        // the dead zone drops the Clip on another Track lane (the Pro Tools/Logic cross-track move).
        int targetLane = -1;
        if (state.trackCount > 0 && geometry.laneHeight > 0
            && std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
        {
            const int lane = (event.getPosition().y - geometry.clipArea.getY()
                              + juce::roundToInt (geometry.viewport.laneScrollPixels))
                           / geometry.laneHeight;
            targetLane = std::clamp (lane, 0, state.trackCount - 1);
            if (const yesdaw::ui::Clip* clip = findClipByLayoutId (state, drag.layoutClipId))
                if (clip->lane == targetLane)
                    targetLane = -1;   // dropped back on its own lane: a plain horizontal move
        }

        if (targetLane < 0 && std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
            return;

        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double nextStartSeconds = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinateSecondsFloor,
            drag.startSeconds + static_cast<double> (deltaX) / pixelsPerSecond);

        if (drag.copy)
        {
            if (onClipCopied)
                onClipCopied (drag.layoutClipId, targetLane, nextStartSeconds,
                              drag.mode == TimelineDragMode::SnapMove);
            return;
        }

        if (targetLane >= 0)
        {
            if (onClipMovedToLane)
                onClipMovedToLane (drag.layoutClipId, targetLane, nextStartSeconds,
                                   drag.mode == TimelineDragMode::SnapMove);
            return;
        }

        if (onClipMoved)
            onClipMoved (drag.layoutClipId, nextStartSeconds, drag.mode == TimelineDragMode::SnapMove);
    }

    // E5 wheel map: Ctrl zooms, Shift scrolls horizontally, and the plain wheel scrolls the
    // shared track-row offset vertically.
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (! stateProvider)
            return;

        const double delta = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (delta == 0.0)
            return;

        if (event.mods.isCtrlDown() || event.mods.isCommandDown())
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            const double anchor =
                timelineSecondsAt (state, getLocalBounds(), event.getPosition()).value_or (state.viewport.scrollSeconds);
            if (onZoomWheel)
                onZoomWheel (anchor, static_cast<double> (wheel.deltaY));
            return;
        }

        if (event.mods.isShiftDown())
        {
            if (onScrollWheel)
                onScrollWheel (delta);
            return;
        }

        if (onVerticalScrollRows)
            onVerticalScrollRows (delta > 0.0 ? -1 : 1);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, event.getPosition());
        if (! hit.hit)
        {
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            if (geometry.rulerArea.contains (event.getPosition()))
                if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                    if (onTimelineLocated)
                        onTimelineLocated (*seconds);
            return;
        }

        if (onClipClicked)
            onClipClicked (hit.id, false);

        if (const std::optional<double> splitSeconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
            if (onClipSplit)
                onClipSplit (hit.id, *splitSeconds, event.mods.isCtrlDown());
    }

private:
    enum class TimelineDragMode
    {
        Move,
        SnapMove,
        TrimLeft,
        TrimRight,
        Gain,
        FadeIn,
        FadeOut
    };

    struct TimelineDragState
    {
        bool active = false;
        bool moved = false;
        bool copy = false;
        int layoutClipId = -1;
        double startSeconds = 0.0;
        double lengthSeconds = 0.0;
        TimelineDragMode mode = TimelineDragMode::Move;
        juce::Point<int> downPosition;
    };

    struct TimelineMarqueeState
    {
        bool active = false;
        bool moved = false;
        juce::Point<int> downPosition;
        juce::Point<int> currentPosition;
    };

    [[nodiscard]] static juce::Rectangle<int> marqueeBounds (const TimelineMarqueeState& marquee) noexcept
    {
        return juce::Rectangle<int>::leftTopRightBottom (
            std::min (marquee.downPosition.x, marquee.currentPosition.x),
            std::min (marquee.downPosition.y, marquee.currentPosition.y),
            std::max (marquee.downPosition.x, marquee.currentPosition.x),
            std::max (marquee.downPosition.y, marquee.currentPosition.y));
    }

    [[nodiscard]] juce::Rectangle<int> marqueeBounds() const noexcept
    {
        return marqueeBounds (marqueeState);
    }

    [[nodiscard]] static int clipIdsIntersectingMarquee (juce::Rectangle<int> area,
                                                         const yesdaw::ui::TimelineCanvasState& state,
                                                         juce::Rectangle<int> marquee,
                                                         int* outIds,
                                                         int outCapacity)
    {
        if (state.clips == nullptr || state.clipCount <= 0 || outIds == nullptr || outCapacity <= 0)
            return 0;

        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (area, state);
        marquee = marquee.getIntersection (geometry.clipArea);
        if (marquee.isEmpty())
            return 0;

        std::array<yesdaw::ui::ElementRect,
                   yesdaw::ui::UiTheme::Layout::timelineCanvasVisibleClipCapacity> visible {};
        const int visibleCount = yesdaw::ui::layoutVisible (
            state.clips, state.clipCount, geometry.viewport,
            visible.data(), static_cast<int> (visible.size()));

        int count = 0;
        for (int i = 0; i < visibleCount && count < outCapacity; ++i)
        {
            const yesdaw::ui::ElementRect& rect = visible[static_cast<std::size_t> (i)];
            const auto hitBounds = juce::Rectangle<int> (
                                       geometry.clipArea.getX() + juce::roundToInt (rect.x),
                                       geometry.clipArea.getY() + juce::roundToInt (rect.y),
                                       juce::roundToInt (rect.w),
                                       juce::roundToInt (rect.h))
                                       .getIntersection (geometry.clipArea);
            if (hitBounds.intersects (marquee))
                outIds[count++] = rect.id;
        }

        return count;
    }

    [[nodiscard]] static const yesdaw::ui::Clip* findClipByLayoutId (const yesdaw::ui::TimelineCanvasState& state,
                                                                     int layoutClipId) noexcept
    {
        if (state.clips == nullptr)
            return nullptr;

        for (int i = 0; i < state.clipCount; ++i)
            if (state.clips[i].id == layoutClipId)
                return &state.clips[i];

        return nullptr;
    }

    [[nodiscard]] static std::optional<double> timelineSecondsAt (const yesdaw::ui::TimelineCanvasState& state,
                                                                  juce::Rectangle<int> bounds,
                                                                  juce::Point<int> position) noexcept
    {
        const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (bounds, state);
        if (! geometry.clipArea.getHorizontalRange().contains (position.x)
            || (! geometry.clipArea.contains (position) && ! geometry.rulerArea.contains (position)))
            return std::nullopt;

        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double seconds = geometry.viewport.scrollSeconds
                             + static_cast<double> (position.x - geometry.clipArea.getX()) / pixelsPerSecond;
        return std::max (yesdaw::ui::UiTheme::Layout::timelineCoordinateSecondsFloor, seconds);
    }

    [[nodiscard]] static TimelineDragMode dragModeForPointer (const yesdaw::ui::TimelineCanvasState& state,
                                                              juce::Rectangle<int> bounds,
                                                              int layoutClipId,
                                                              juce::Point<int> position,
                                                              juce::ModifierKeys modifiers) noexcept
    {
        const yesdaw::ui::Clip* const clip = findClipByLayoutId (state, layoutClipId);
        if (clip == nullptr)
            return TimelineDragMode::Move;

        const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (bounds, state);
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double clipLeftX = static_cast<double> (geometry.clipArea.getX())
                               + (clip->startSeconds - geometry.viewport.scrollSeconds) * pixelsPerSecond;
        const double clipRightX = static_cast<double> (geometry.clipArea.getX())
                                + ((clip->startSeconds + clip->lengthSeconds) - geometry.viewport.scrollSeconds)
                                      * pixelsPerSecond;

        if (modifiers.isAltDown())
        {
            if (std::fabs (static_cast<double> (position.x) - clipLeftX)
                <= static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineClipEdgeHitWidth))
                return TimelineDragMode::FadeIn;

            if (std::fabs (static_cast<double> (position.x) - clipRightX)
                <= static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineClipEdgeHitWidth))
                return TimelineDragMode::FadeOut;
        }

        if (std::fabs (static_cast<double> (position.x) - clipRightX)
            <= static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineClipEdgeHitWidth))
            return TimelineDragMode::TrimRight;

        if (std::fabs (static_cast<double> (position.x) - clipLeftX)
            <= static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineClipEdgeHitWidth))
            return TimelineDragMode::TrimLeft;

        if (modifiers.isShiftDown())
            return TimelineDragMode::Gain;

        if (modifiers.isCtrlDown())
            return TimelineDragMode::SnapMove;

        return TimelineDragMode::Move;
    }

    TimelineDragState dragState;
    TimelineMarqueeState marqueeState;
    // Hand tool (E3): a press-drag pans the viewport horizontally; transient view state only.
    bool handDragActive = false;
    int handDragLastX = 0;
    bool playheadLocateActive = false;
    bool loopDragActive = false;
    double loopDragStartSeconds = 0.0;
    bool rulerRangeDragActive = false;
    double rulerRangeDragStartSeconds = 0.0;
    juce::Point<int> rulerRangeDownPosition;
    juce::Point<int> rulerRangeCurrentPosition;
};

struct PianoRollCanvasGeometry
{
    juce::Rectangle<int> expression;
    juce::Rectangle<int> keyboard;
    juce::Rectangle<int> grid;
    float rowHeight = 1.0f;
};

[[nodiscard]] PianoRollCanvasGeometry pianoRollCanvasGeometry (juce::Rectangle<int> area) noexcept
{
    area.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollHeaderHeight);
    area.reduce (yesdaw::ui::UiTheme::Layout::pianoRollPanelInsetX,
                 yesdaw::ui::UiTheme::Layout::pianoRollPanelInsetY);
    PianoRollCanvasGeometry geometry;
    geometry.expression = area.removeFromBottom (yesdaw::ui::UiTheme::Layout::pianoRollExpressionHeight);
    geometry.keyboard = area.removeFromLeft (yesdaw::ui::UiTheme::Layout::pianoRollKeyboardWidth);
    geometry.grid = area.reduced (yesdaw::ui::UiTheme::Layout::pianoRollGridInsetX,
                                  yesdaw::ui::UiTheme::Layout::pianoRollGridInsetY);
    geometry.rowHeight = static_cast<float> (juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollGridMinHeight,
                                                         geometry.grid.getHeight()))
                       / static_cast<float> (yesdaw::ui::UiTheme::Layout::pianoRollKeyCount);
    return geometry;
}

[[nodiscard]] yesdaw::engine::Tick pianoRollTimelineLength (
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface) noexcept
{
    return juce::jmax<yesdaw::engine::Tick> (1, surface.timelineLength);
}

[[nodiscard]] int pianoRollKeyY (const PianoRollCanvasGeometry& geometry, int key) noexcept
{
    return geometry.grid.getY()
         + juce::roundToInt (
             static_cast<float> (yesdaw::ui::UiTheme::Layout::pianoRollHighKey - key) * geometry.rowHeight);
}

[[nodiscard]] int pianoRollTickX (const PianoRollCanvasGeometry& geometry,
                                  const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                  yesdaw::engine::Tick tick) noexcept
{
    const double timelineLength = static_cast<double> (pianoRollTimelineLength (surface));
    const double normalized = static_cast<double> (tick) / timelineLength;
    return geometry.grid.getX()
         + juce::roundToInt (static_cast<float> (normalized) * static_cast<float> (geometry.grid.getWidth()));
}

[[nodiscard]] yesdaw::engine::Tick pianoRollTickDeltaForPixels (
    const PianoRollCanvasGeometry& geometry,
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
    int deltaPixels) noexcept
{
    const int gridWidth = juce::jmax (1, geometry.grid.getWidth());
    const double ticks = static_cast<double> (deltaPixels)
                       * static_cast<double> (pianoRollTimelineLength (surface))
                       / static_cast<double> (gridWidth);
    return static_cast<yesdaw::engine::Tick> (std::llround (ticks));
}

[[nodiscard]] juce::Rectangle<int> pianoRollNoteBounds (
    const PianoRollCanvasGeometry& geometry,
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
    const yesdaw::ui::UiPianoRollNoteView& note) noexcept
{
    const int x = pianoRollTickX (geometry, surface, note.startTick);
    const int width = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollNoteMinWidth,
                                  pianoRollTickX (geometry, surface, note.startTick + note.lengthTicks) - x);
    const int y = pianoRollKeyY (geometry, note.key)
                + yesdaw::ui::UiTheme::Layout::pianoRollNoteTopInset;
    const int height = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollNoteMinHeight,
                                   juce::roundToInt (geometry.rowHeight)
                                       - yesdaw::ui::UiTheme::Layout::pianoRollNoteHeightTrim);
    return juce::Rectangle<int> (x, y, width, height)
        .reduced (yesdaw::ui::UiTheme::Layout::pianoRollNoteInsetX,
                  yesdaw::ui::UiTheme::Layout::pianoRollNoteInsetY);
}

class PianoRollInputComponent final : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    std::function<yesdaw::ui::UiPianoRollSurfaceSnapshot()> stateProvider;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onNoteClicked;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteMoved;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteLengthChanged;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, std::int32_t)> onNoteTransposed;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteQuantized;
    std::function<void()> onExpressionRead;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::Tick, std::int16_t)> onNoteAdded;
    // Alt+wheel on a note adjusts its velocity (B33): clip, note, new normalized velocity.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, double)> onNoteVelocityAdjusted;
    // Ctrl+drag copy-drags a note (B35): clip, source note, copy's start tick.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteCopyDragged;

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (! event.mods.isAltDown() || ! stateProvider || ! onNoteVelocityAdjusted)
            return;

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const auto hit = noteAt (surface, event.getPosition());
        if (! hit)
            return;

        const double adjusted = juce::jlimit (
            0.0,
            1.0,
            hit->normalizedVelocity
                + static_cast<double> (wheel.deltaY)
                      * yesdaw::ui::UiTheme::Layout::pianoRollVelocityWheelScale);
        if (adjusted != hit->normalizedVelocity)
            onNoteVelocityAdjusted (surface.midiClipId, hit->noteId, adjusted);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const auto hit = noteAt (surface, event.getPosition());
        if (! hit)
        {
            dragState = {};
            // Pencil (usable-DAW P1): a click on EMPTY grid adds a note at the clicked tick and key,
            // snapped to the piano-roll grid.
            if (surface.midiClipSelected && onNoteAdded)
            {
                const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
                if (geometry.grid.contains (event.getPosition()) && geometry.grid.getWidth() > 0)
                {
                    const double normalized =
                        static_cast<double> (event.getPosition().x - geometry.grid.getX())
                        / static_cast<double> (geometry.grid.getWidth());
                    const auto timelineLength = static_cast<double> (pianoRollTimelineLength (surface));
                    yesdaw::engine::Tick tick =
                        static_cast<yesdaw::engine::Tick> (normalized * timelineLength);
                    tick -= tick % kPianoRollSnapGridTicks;

                    const float rowHeight = geometry.rowHeight > 1.0f ? geometry.rowHeight : 1.0f;
                    const int key = yesdaw::ui::UiTheme::Layout::pianoRollHighKey
                        - static_cast<int> ((event.getPosition().y - geometry.grid.getY()) / rowHeight);
                    if (key >= yesdaw::ui::UiTheme::Layout::pianoRollLowKey
                        && key <= yesdaw::ui::UiTheme::Layout::pianoRollHighKey)
                        onNoteAdded (surface.midiClipId, tick, static_cast<std::int16_t> (key));
                }
            }
            return;
        }

        if (onNoteClicked)
            onNoteClicked (surface.midiClipId, hit->noteId);

        dragState = {};
        dragState.active = true;
        dragState.noteId = hit->noteId;
        dragState.midiClipId = surface.midiClipId;
        dragState.startTick = hit->startTick;
        dragState.lengthTicks = hit->lengthTicks;
        dragState.downPosition = event.getPosition();
        dragState.mode = dragModeForPointer (surface, *hit, event.getPosition(), event.mods);
        // Ctrl+drag copy-drags the note (B35), mirroring the timeline's copy-drag law. Ctrl is an
        // explicit copy request, so it wins over the narrow note's resize-edge zone.
        dragState.copy = event.mods.isCtrlDown() && ! event.mods.isShiftDown();
        if (dragState.copy)
            dragState.mode = PianoDragMode::Move;
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (dragState.active)
            dragState.moved = true;
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! dragState.active)
            return;

        const PianoDragState drag = dragState;
        dragState = {};

        if (! drag.moved || ! stateProvider)
            return;

        const int deltaX = event.getPosition().x - drag.downPosition.x;
        if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
            return;

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
        const yesdaw::engine::Tick deltaTicks = pianoRollTickDeltaForPixels (geometry, surface, deltaX);

        if (drag.mode == PianoDragMode::SetLength)
        {
            const yesdaw::engine::Tick maxLength =
                juce::jmax<yesdaw::engine::Tick> (0, surface.timelineLength - drag.startTick);
            const yesdaw::engine::Tick nextLength =
                std::clamp<yesdaw::engine::Tick> (drag.lengthTicks + deltaTicks, 0, maxLength);
            if (nextLength != drag.lengthTicks && onNoteLengthChanged)
                onNoteLengthChanged (drag.midiClipId, drag.noteId, nextLength);
            return;
        }

        const yesdaw::engine::Tick maxStart =
            juce::jmax<yesdaw::engine::Tick> (0, surface.timelineLength - drag.lengthTicks);
        const yesdaw::engine::Tick nextStart =
            std::clamp<yesdaw::engine::Tick> (drag.startTick + deltaTicks, 0, maxStart);
        if (nextStart == drag.startTick)
            return;

        if (drag.copy)
        {
            if (onNoteCopyDragged)
                onNoteCopyDragged (drag.midiClipId, drag.noteId, nextStart);
            return;
        }

        if (onNoteMoved)
            onNoteMoved (drag.midiClipId, drag.noteId, nextStart);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const auto hit = noteAt (surface, event.getPosition());
        if (! hit)
            return;

        if (onNoteClicked)
            onNoteClicked (surface.midiClipId, hit->noteId);

        if (event.mods.isShiftDown())
        {
            if (onExpressionRead)
                onExpressionRead();
            return;
        }

        if (event.mods.isCtrlDown())
        {
            if (onNoteQuantized)
                onNoteQuantized (surface.midiClipId, hit->noteId, kPianoRollSnapGridTicks);
            return;
        }

        if (event.mods.isAltDown())
        {
            if (onNoteTransposed)
                onNoteTransposed (surface.midiClipId, hit->noteId, 1);
        }
    }

private:
    enum class PianoDragMode
    {
        Move,
        SetLength
    };

    struct PianoDragState
    {
        bool active = false;
        bool moved = false;
        yesdaw::engine::EntityId midiClipId {};
        yesdaw::engine::EntityId noteId {};
        yesdaw::engine::Tick startTick = 0;
        yesdaw::engine::Tick lengthTicks = 0;
        PianoDragMode mode = PianoDragMode::Move;
        bool copy = false;   // Ctrl+drag copy-drag (B35)
        juce::Point<int> downPosition;
    };

    [[nodiscard]] std::optional<yesdaw::ui::UiPianoRollNoteView> noteAt (
        const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
        juce::Point<int> position) const noexcept
    {
        if (! surface.midiClipSelected)
            return std::nullopt;

        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
        for (auto it = surface.notes.rbegin(); it != surface.notes.rend(); ++it)
        {
            if (it->key < yesdaw::ui::UiTheme::Layout::pianoRollLowKey
                || it->key > yesdaw::ui::UiTheme::Layout::pianoRollHighKey)
                continue;

            if (pianoRollNoteBounds (geometry, surface, *it).contains (position))
                return *it;
        }

        return std::nullopt;
    }

    [[nodiscard]] PianoDragMode dragModeForPointer (
        const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
        const yesdaw::ui::UiPianoRollNoteView& note,
        juce::Point<int> position,
        juce::ModifierKeys modifiers) const noexcept
    {
        if (modifiers.isShiftDown())
            return PianoDragMode::SetLength;

        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
        const int rightEdge = pianoRollNoteBounds (geometry, surface, note).getRight();
        if (std::abs (position.x - rightEdge)
            <= yesdaw::ui::UiTheme::Layout::pianoRollNoteEdgeHitWidth)
            return PianoDragMode::SetLength;

        return PianoDragMode::Move;
    }

    PianoDragState dragState;
};

class ToolbarActionButton final : public juce::TextButton
{
public:
    void setAction (yesdaw::ui::UiActionId value) noexcept { action = value; }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        if (! yesdaw::ui::hasActionIcon (action))
        {
            juce::TextButton::paintButton (g, highlighted, down);
            return;
        }

        getLookAndFeel().drawButtonBackground (
            g,
            *this,
            findColour (getToggleState() ? juce::TextButton::buttonOnColourId
                                         : juce::TextButton::buttonColourId),
            highlighted,
            down);

        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (yesdaw::ui::UiTheme::Tone::componentVisibleAlpha);
        const auto iconColour = findColour (getToggleState() ? juce::TextButton::textColourOnId
                                                              : juce::TextButton::textColourOffId)
                                    .withMultipliedAlpha (
                                        isEnabled() ? yesdaw::ui::UiTheme::Tone::componentVisibleAlpha
                                                    : yesdaw::ui::UiTheme::Tone::disabledAlpha);
        auto content = getLocalBounds();
        if (yesdaw::ui::actionUsesIconOnlyChrome (action))
        {
            (void) yesdaw::ui::drawActionIcon (
                g,
                action,
                content.toFloat().reduced (
                    static_cast<float> (yesdaw::ui::UiTheme::Layout::controlIconInset)),
                iconColour);
            return;
        }

        auto iconArea = content.removeFromLeft (content.getHeight())
                           .reduced (yesdaw::ui::UiTheme::Layout::controlIconInset);
        (void) yesdaw::ui::drawActionIcon (g, action, iconArea.toFloat(), iconColour);
        g.setColour (iconColour);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::body,
            juce::Font::bold));
        g.drawFittedText (getButtonText(),
                          content.reduced (yesdaw::ui::UiTheme::Layout::controlIconTextGap, 0),
                          juce::Justification::centredLeft,
                          1);
    }

private:
    yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::ProjectNew;
};

// Transparent overlay across the left Track rail. Shares drawTrackList's exact row math (header
// strip, then equal rows floored at trackListRowMinHeight) so hit-testing and paint cannot drift.
// Slider with an exact Shift fine-drag mode (B30): while Shift is held, pointer movement counts
// for exactly UiTheme::Layout::fineDragScale of its plain effect. Fine mode anchors at the value
// when it engages (no jump-to-pointer), accumulates unsnapped so tiny moves add up, latches until
// mouse-up, and never engages while Alt is down so the Alt+click reset law is untouched.
class FineDragSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;

    void mouseDown (const juce::MouseEvent& event) override
    {
        fineActive = false;
        lastFinePosition = event.position;
        if (isEnabled() && wantsFineDrag (event) && supportsFineDrag())
        {
            fineActive = true;
            fineStartedDrag = true;   // the base drag was swallowed, so fire the callbacks here
            fineValue = getValue();
            if (onDragStart)
                onDragStart();
            return;   // no jump-to-pointer; the anchor is the current value
        }

        juce::Slider::mouseDown (event);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (! fineActive && wantsFineDrag (event) && supportsFineDrag())
        {
            fineActive = true;   // Shift pressed mid-drag: anchor at the value reached so far
            fineValue = getValue();
        }

        if (! fineActive)
        {
            lastFinePosition = event.position;
            juce::Slider::mouseDrag (event);
            return;
        }

        const double proportionDelta = axisProportionDelta (event.position);
        lastFinePosition = event.position;
        fineValue = juce::jlimit (getMinimum(),
                                  getMaximum(),
                                  fineValue
                                      + proportionDelta * (getMaximum() - getMinimum())
                                            * yesdaw::ui::UiTheme::Layout::fineDragScale);
        setValue (fineValue, juce::sendNotificationSync);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        fineActive = false;
        juce::Slider::mouseUp (event);
        if (fineStartedDrag)
        {
            fineStartedDrag = false;
            if (onDragEnd)
                onDragEnd();
        }
    }

private:
    [[nodiscard]] static bool wantsFineDrag (const juce::MouseEvent& event)
    {
        return event.mods.isShiftDown() && ! event.mods.isAltDown();
    }

    [[nodiscard]] bool supportsFineDrag() const
    {
        const auto style = getSliderStyle();
        return style == juce::Slider::LinearHorizontal
            || style == juce::Slider::LinearVertical
            || style == juce::Slider::RotaryHorizontalVerticalDrag;
    }

    // Pointer movement as a proportion of the control's own span, matching each style's plain
    // drag direction: horizontal tracks +x, vertical tracks -y, rotary tracks (+x, -y) combined.
    [[nodiscard]] double axisProportionDelta (juce::Point<float> position) const
    {
        const double dx = static_cast<double> (position.x - lastFinePosition.x);
        const double dy = static_cast<double> (position.y - lastFinePosition.y);
        switch (getSliderStyle())
        {
            case juce::Slider::LinearHorizontal:
                return dx / std::max (1, getWidth());
            case juce::Slider::LinearVertical:
                return -dy / std::max (1, getHeight());
            case juce::Slider::RotaryHorizontalVerticalDrag:
                return (dx - dy) / std::max (1, getWidth());
            default:
                return 0.0;
        }
    }

    bool fineActive = false;
    bool fineStartedDrag = false;
    double fineValue = 0.0;
    juce::Point<float> lastFinePosition;
};

class TrackListInputComponent final : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    std::function<int()> rowCountProvider;
    std::function<void (int)> onRowClicked;
    std::function<void (int)> onRowDoubleClicked;
    // Mini controls (usable-DAW P2): the painted PAN knob, VOL slider, and M/S cells become live.
    std::function<void (int, float)> onPanEdited;      // row, pan in [-1, 1]
    std::function<void (int, float)> onVolumeEdited;   // row, linear gain in [0, 1]
    std::function<void (int)> onMuteToggled;
    std::function<void (int)> onSoloToggled;
    // Current strip values, used to anchor Shift fine drags without a jump (B30).
    std::function<float (int)> panValueProvider;
    std::function<float (int)> volumeValueProvider;
    // Fired on mouse-up after any mini-control gesture, so transient drag readouts can hide (B31).
    std::function<void()> onMiniDragEnded;
    // Click on the row's meter clears its held peak and latched clip light (B32).
    std::function<void (int)> onMeterClicked;

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragRow = -1;
        dragZone = MiniZone::None;
        const int row = rowAt (event.getPosition());
        if (row < 0)
            return;

        switch (zoneAt (row, event.getPosition()))
        {
            case MiniZone::Pan:
                if (event.mods.isAltDown())
                {
                    if (onPanEdited)
                        onPanEdited (row, 0.0f);   // Alt+click recentres, matching double-click
                    return;
                }
                dragRow = row;
                dragZone = MiniZone::Pan;
                if (beginFineDragIfWanted (event))
                    return;   // fine mode anchors at the current value; no jump
                applyPan (row, event.getPosition());
                return;

            case MiniZone::Volume:
                if (event.mods.isAltDown())
                {
                    if (onVolumeEdited)
                        onVolumeEdited (row, 1.0f);   // Alt+click resets the mini VOL to unity
                    return;
                }
                dragRow = row;
                dragZone = MiniZone::Volume;
                if (beginFineDragIfWanted (event))
                    return;
                applyVolume (row, event.getPosition());
                return;

            case MiniZone::Mute:
                if (onMuteToggled)
                    onMuteToggled (row);
                return;

            case MiniZone::Solo:
                if (onSoloToggled)
                    onSoloToggled (row);
                return;

            case MiniZone::Meter:
                if (onMeterClicked)
                    onMeterClicked (row);
                return;

            case MiniZone::None:
                break;
        }

        if (onRowClicked)
            onRowClicked (row);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragRow < 0)
            return;

        if (! fineDragActive && event.mods.isShiftDown() && ! event.mods.isAltDown())
            (void) beginFineDragIfWanted (event);   // Shift pressed mid-drag: anchor here

        if (fineDragActive)
        {
            applyFineDrag (event);
            return;
        }

        if (dragZone == MiniZone::Pan)
            applyPan (dragRow, event.getPosition());
        else if (dragZone == MiniZone::Volume)
            applyVolume (dragRow, event.getPosition());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragRow = -1;
        dragZone = MiniZone::None;
        fineDragActive = false;
        if (onMiniDragEnded)
            onMiniDragEnded();
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const int row = rowAt (event.getPosition());
        if (row < 0)
            return;

        // Double-click on the pan knob recentres; elsewhere the row rename applies.
        if (zoneAt (row, event.getPosition()) == MiniZone::Pan)
        {
            if (onPanEdited)
                onPanEdited (row, 0.0f);
            return;
        }

        if (onRowDoubleClicked)
            onRowDoubleClicked (row);
    }

    [[nodiscard]] juce::Rectangle<int> rowBounds (int row) const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0 || row < 0 || row >= rows)
            return {};

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                          area.getHeight() / rows);
        const int scrollRows = effectiveScrollRows();
        return { area.getX(), area.getY() + (row - scrollRows) * rowHeight, area.getWidth(), rowHeight };
    }

    // Shared row-geometry law: these mirror drawTrackList's control rectangles exactly, so
    // hit-testing and paint cannot drift. The paint code trims the separator first.
    [[nodiscard]] juce::Rectangle<int> panKnobBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withRight (bounds.getRight() - yesdaw::ui::UiTheme::Layout::trackListPanRightInset)
                     .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListPanDiameter)
                     .withY (bounds.getY() + yesdaw::ui::UiTheme::Layout::trackListPanTopInset)
                     .withHeight (yesdaw::ui::UiTheme::Layout::trackListPanDiameter);
    }

    [[nodiscard]] juce::Rectangle<int> volumeSliderBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withRight (bounds.getRight() - yesdaw::ui::UiTheme::Layout::trackListLevelRightInset)
                     .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListLevelWidth)
                     .withBottom (bounds.getBottom() - yesdaw::ui::UiTheme::Layout::trackListLevelBottomInset)
                     .withHeight (yesdaw::ui::UiTheme::Layout::trackListLevelHeight);
    }

    [[nodiscard]] juce::Rectangle<int> muteCellBounds (int row) const { return buttonCellBounds (row, 0); }
    [[nodiscard]] juce::Rectangle<int> soloCellBounds (int row) const { return buttonCellBounds (row, 1); }

    [[nodiscard]] juce::Rectangle<int> meterZoneBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withRight (bounds.getRight() - yesdaw::ui::UiTheme::Layout::trackListMeterRightInset)
                     .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListMeterWidth)
                     .reduced (yesdaw::ui::UiTheme::Layout::trackListMeterHorizontalInset,
                               yesdaw::ui::UiTheme::Layout::trackListMeterVerticalInset);
    }

private:
    enum class MiniZone : std::uint8_t { None, Pan, Volume, Mute, Solo, Meter };

    [[nodiscard]] juce::Rectangle<int> buttonCellBounds (int row, int cellIndex) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        auto buttonsArea = bounds.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                               .withTrimmedTop (yesdaw::ui::UiTheme::Layout::trackListButtonsTop)
                               .withHeight (yesdaw::ui::UiTheme::Layout::trackListButtonsHeight);
        for (int cell = 0; cell < cellIndex; ++cell)
            buttonsArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::trackListButtonWidth);
        return buttonsArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::trackListButtonWidth);
    }

    [[nodiscard]] MiniZone zoneAt (int row, juce::Point<int> position) const
    {
        if (panKnobBounds (row).contains (position))
            return MiniZone::Pan;
        if (volumeSliderBounds (row).expanded (yesdaw::ui::UiTheme::Space::none,
                                               yesdaw::ui::UiTheme::Layout::trackListRowVerticalInset)
                .contains (position))
            return MiniZone::Volume;
        if (muteCellBounds (row).contains (position))
            return MiniZone::Mute;
        if (soloCellBounds (row).contains (position))
            return MiniZone::Solo;
        if (meterZoneBounds (row).contains (position))
            return MiniZone::Meter;
        return MiniZone::None;
    }

    void applyPan (int row, juce::Point<int> position)
    {
        if (! onPanEdited)
            return;

        const auto knob = panKnobBounds (row);
        if (knob.getWidth() <= 0)
            return;

        const float normalized = static_cast<float> (position.x - knob.getX())
                               / static_cast<float> (knob.getWidth());
        onPanEdited (row, juce::jlimit (-1.0f, 1.0f, normalized + normalized - 1.0f));
    }

    void applyVolume (int row, juce::Point<int> position)
    {
        if (! onVolumeEdited)
            return;

        const auto slider = volumeSliderBounds (row);
        if (slider.getWidth() <= 0)
            return;

        const float normalized = static_cast<float> (position.x - slider.getX())
                               / static_cast<float> (slider.getWidth());
        onVolumeEdited (row, juce::jlimit (0.0f, 1.0f, normalized));
    }

    // Shift fine drag (B30): anchor at the strip's current value and scale pointer movement by
    // the shared fine-drag token; the value never jumps to the pointer while fine mode is active.
    [[nodiscard]] bool beginFineDragIfWanted (const juce::MouseEvent& event)
    {
        if (! event.mods.isShiftDown() || event.mods.isAltDown())
            return false;

        const auto* provider = dragZone == MiniZone::Pan
                                   ? &panValueProvider
                                   : &volumeValueProvider;
        if (! (*provider))
            return false;

        fineDragActive = true;
        fineDragValue = (*provider) (dragRow);
        fineDragLastX = event.getPosition().x;
        return true;
    }

    void applyFineDrag (const juce::MouseEvent& event)
    {
        const auto bounds = dragZone == MiniZone::Pan ? panKnobBounds (dragRow)
                                                      : volumeSliderBounds (dragRow);
        if (bounds.getWidth() <= 0)
            return;

        const int x = event.getPosition().x;
        const double proportionDelta = static_cast<double> (x - fineDragLastX)
                                     / static_cast<double> (bounds.getWidth());
        fineDragLastX = x;

        const bool isPan = dragZone == MiniZone::Pan;
        const double span = isPan ? 2.0 : 1.0;   // pan covers [-1, 1]; VOL covers [0, 1]
        fineDragValue = static_cast<float> (juce::jlimit (
            isPan ? -1.0 : 0.0,
            1.0,
            static_cast<double> (fineDragValue)
                + proportionDelta * span * yesdaw::ui::UiTheme::Layout::fineDragScale));

        if (isPan && onPanEdited)
            onPanEdited (dragRow, fineDragValue);
        else if (! isPan && onVolumeEdited)
            onVolumeEdited (dragRow, fineDragValue);
    }

    int dragRow = -1;
    MiniZone dragZone = MiniZone::None;
    bool fineDragActive = false;
    float fineDragValue = 0.0f;
    int fineDragLastX = 0;

    [[nodiscard]] int rowAt (juce::Point<int> position) const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0)
            return -1;

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        if (! area.contains (position))
            return -1;

        const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                          area.getHeight() / rows);
        const int row = (position.y - area.getY()) / rowHeight + effectiveScrollRows();
        return row >= 0 && row < rows ? row : -1;
    }

public:
    // Vertical track scroll (E5): the rail shares the timeline's whole-row offset, clamped to its
    // own overflow so its last row always pins to the window bottom.
    std::function<int()> rowScrollProvider;
    std::function<void (int)> onVerticalScrollRows;

    [[nodiscard]] int maxScrollRows() const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0)
            return 0;

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                          area.getHeight() / rows);
        const int visibleRows = std::max (1, area.getHeight() / rowHeight);
        return std::max (0, rows - visibleRows);
    }

    [[nodiscard]] int effectiveScrollRows() const
    {
        return std::clamp (rowScrollProvider ? rowScrollProvider() : 0, 0, maxScrollRows());
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        const double delta = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (delta == 0.0 || ! onVerticalScrollRows)
            return;

        onVerticalScrollRows (delta > 0.0 ? -1 : 1);
    }
};

// The automation lane canvas (usable-DAW P1): paints the target lane's breakpoints against the SAME
// timeline viewport math as the arrangement, and turns clicks into real breakpoint edits. Click empty
// lane = add at (time, value); drag a handle = move; double-click a handle = delete.
class AutomationLaneCanvasComponent final : public juce::Component,
                                            public juce::SettableTooltipClient
{
public:
    std::function<std::vector<std::pair<double, double>>()> pointsProvider;   // (seconds, normalized value)
    std::function<double (int)> secondsForLocalX;
    std::function<int (double)> localXForSeconds;
    std::function<void (double, double)> onAddPoint;
    std::function<void (double, double, double)> onMovePoint;   // oldSeconds, newSeconds, newValue
    std::function<void (double)> onDeletePoint;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (yesdaw::ui::UiTheme::Color::controlInset());
        if (! pointsProvider || ! localXForSeconds)
            return;

        const std::vector<std::pair<double, double>> points = pointsProvider();
        g.setColour (yesdaw::ui::UiTheme::Color::accentPurple());
        juce::Path line;
        bool started = false;
        for (const auto& [seconds, value] : points)
        {
            const juce::Point<float> at { static_cast<float> (localXForSeconds (seconds)),
                                          yForValue (value) };
            if (! started)
            {
                line.startNewSubPath (at);
                started = true;
            }
            else
            {
                line.lineTo (at);
            }
        }
        g.strokePath (line, juce::PathStrokeType (yesdaw::ui::UiTheme::Layout::automationCanvasLineWidth));

        for (const auto& [seconds, value] : points)
        {
            const float radius = static_cast<float> (yesdaw::ui::UiTheme::Layout::automationCanvasHandleRadius);
            g.fillEllipse (static_cast<float> (localXForSeconds (seconds)) - radius,
                           yForValue (value) - radius,
                           radius + radius,
                           radius + radius);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragOldSeconds.reset();
        if (const std::optional<double> hit = handleSecondsAt (event.getPosition()))
        {
            dragOldSeconds = hit;
            return;
        }

        if (onAddPoint && secondsForLocalX)
            onAddPoint (secondsForLocalX (event.getPosition().x), valueForY (event.getPosition().y));
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (! dragOldSeconds)
            return;

        const double oldSeconds = *dragOldSeconds;
        dragOldSeconds.reset();
        if (! event.mouseWasDraggedSinceMouseDown() || ! onMovePoint || ! secondsForLocalX)
            return;

        onMovePoint (oldSeconds,
                     secondsForLocalX (event.getPosition().x),
                     valueForY (event.getPosition().y));
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (const std::optional<double> hit = handleSecondsAt (event.getPosition()))
            if (onDeletePoint)
                onDeletePoint (*hit);
    }

private:
    [[nodiscard]] float yForValue (double value) const
    {
        const float height = static_cast<float> (juce::jmax (1, getHeight()));
        return height * static_cast<float> (1.0 - std::clamp (value, 0.0, 1.0));
    }

    [[nodiscard]] double valueForY (int y) const
    {
        const double height = static_cast<double> (juce::jmax (1, getHeight()));
        return std::clamp (1.0 - static_cast<double> (y) / height, 0.0, 1.0);
    }

    [[nodiscard]] std::optional<double> handleSecondsAt (juce::Point<int> position) const
    {
        if (! pointsProvider || ! localXForSeconds)
            return std::nullopt;

        const int hitRadius = yesdaw::ui::UiTheme::Layout::automationCanvasHandleHitRadius;
        for (const auto& [seconds, value] : pointsProvider())
        {
            const juce::Point<int> at { localXForSeconds (seconds),
                                        static_cast<int> (yForValue (value)) };
            if (position.getDistanceFrom (at) <= hitRadius)
                return seconds;
        }

        return std::nullopt;
    }

    std::optional<double> dragOldSeconds;
};

// Transparent overlay across the mixer strip region: forwards a click to the strip index under the
// pointer (geometry owned by MainComponent so paint and hits share one source of truth).
class MixerStripsInputComponent final : public juce::Component,
                                        public juce::SettableTooltipClient
{
public:
    std::function<int (juce::Point<int>)> stripAtPosition;   // position in SHELL coordinates
    std::function<void (int)> onStripClicked;
    // Painted meter hit test (B32): a click inside a track strip's painted meter clears its
    // held peak and latched clip light instead of retargeting the strip.
    std::function<int (juce::Point<int>)> meterStripAtPosition;
    std::function<void (int)> onMeterClicked;

    void mouseDown (const juce::MouseEvent& event) override
    {
        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();

        if (meterStripAtPosition && onMeterClicked)
        {
            const int meterStrip = meterStripAtPosition (shellPosition);
            if (meterStrip >= 0)
            {
                onMeterClicked (meterStrip);
                return;
            }
        }

        if (! stripAtPosition || ! onStripClicked)
            return;

        const int strip = stripAtPosition (shellPosition);
        if (strip >= 0)
            onStripClicked (strip);
    }
};

class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      private juce::Timer,
                      private juce::AudioIODeviceCallback
{
public:
    explicit MainComponent (yesdaw::ui::MainComponentFileChoices choices, bool enableDesktopAudio)
        : fileChoices (std::move (choices)), desktopAudioRequested (enableDesktopAudio)
    {
        appModel.setPlaybackReplacementCallbacks (
            [this] { suspendDesktopAudioCallback(); },
            [this] { resumeDesktopAudioCallback(); });

        if (! fileChoices.sessionStateDirectory.empty())
            appModel.setSessionStateDirectory (fileChoices.sessionStateDirectory);

        setOpaque (true);
        setLookAndFeel (&lookAndFeel);
        setWantsKeyboardFocus (true);   // the declared keymap chords dispatch through keyPressed
        setSize (yesdaw::ui::UiTheme::Layout::defaultWindowWidth,
                 yesdaw::ui::UiTheme::Layout::defaultWindowHeight);

        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const yesdaw::ui::UiActionId action = toolbarActions[i];
            const auto* descriptor = appModel.registry().descriptor (action);
            if (descriptor == nullptr)
                continue;

            auto& button = buttons[i];
            button.setAction (action);
            button.setButtonText (actionButtonText (action));
            button.setComponentID (descriptor->stableId);
            button.setName (descriptor->accessibleName);
            button.setTooltip (juce::String (descriptor->stableId) + "  " + descriptor->defaultKey);
            button.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
            button.setColour (juce::TextButton::buttonOnColourId, descriptor->accessibleRole == yesdaw::ui::AccessibilityRole::ToggleButton
                                                                  ? kPurple.darker (0.45f)
                                                                  : kBlue.darker (0.25f));
            button.setColour (juce::TextButton::textColourOffId, kText);
            button.setColour (juce::TextButton::textColourOnId, kText);
            button.onClick = [this, action] {
                handleAction (action);
                refreshActionState();
                resized();
                repaint();
            };
            addAndMakeVisible (button);
        }

        configureAutosaveRecoveryButton (autosaveRestoreButton, yesdaw::ui::UiActionId::AutosaveRecoveryRestore);
        configureAutosaveRecoveryButton (autosaveDiscardButton, yesdaw::ui::UiActionId::AutosaveRecoveryDiscard);

        configureActionComponent (exportAudioButton, yesdaw::ui::UiActionId::ProjectExportAudio, "Export audio");
        exportAudioButton.setButtonText ("Export WAV");
        exportAudioButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        exportAudioButton.setColour (juce::TextButton::textColourOffId, kText);
        exportAudioButton.onClick = [this] {
            handleAction (yesdaw::ui::UiActionId::ProjectExportAudio);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (exportAudioButton);

        // Export options (usable-DAW P1): bit depth and range feed the model before Export runs.
        exportBitDepthChooser.setComponentID ("shell.export.bitdepth");
        exportBitDepthChooser.setTooltip ("Export bit depth");
        exportBitDepthChooser.setName ("Export bit depth");
        exportBitDepthChooser.setTitle ("Export bit depth");
        exportBitDepthChooser.addItem ("32-bit float", 1);
        exportBitDepthChooser.addItem ("24-bit PCM", 2);
        exportBitDepthChooser.addItem ("16-bit PCM", 3);
        exportBitDepthChooser.setSelectedId (1, juce::dontSendNotification);
        exportBitDepthChooser.onChange = [this] {
            const int selected = exportBitDepthChooser.getSelectedId();
            appModel.setExportBitDepth (selected == 2 ? yesdaw::ui::UiAppModel::UiExportBitDepth::Int24
                                        : selected == 3 ? yesdaw::ui::UiAppModel::UiExportBitDepth::Int16
                                                        : yesdaw::ui::UiAppModel::UiExportBitDepth::Float32);
        };
        addAndMakeVisible (exportBitDepthChooser);

        exportRangeChooser.setComponentID ("shell.export.range");
        exportRangeChooser.setTooltip ("Export range: whole project or the loop/range selection");
        exportRangeChooser.setName ("Export range");
        exportRangeChooser.setTitle ("Export range");
        exportRangeChooser.addItem ("Whole Project", 1);
        exportRangeChooser.addItem ("Loop Region", 2);
        exportRangeChooser.setSelectedId (1, juce::dontSendNotification);
        exportRangeChooser.onChange = [this] {
            appModel.setExportLoopRangeOnly (exportRangeChooser.getSelectedId() == 2);
        };
        addAndMakeVisible (exportRangeChooser);

        exportAudioProgress.setComponentID (kExportAudioProgressComponentId);
        exportAudioProgress.setTooltip ("Audio export progress");
        exportAudioProgress.setName ("Export audio progress");
        exportAudioProgress.setText ("Export --", juce::dontSendNotification);
        exportAudioProgress.setJustificationType (juce::Justification::centred);
        exportAudioProgress.setColour (juce::Label::backgroundColourId, yesdaw::ui::UiTheme::Color::darkControl());
        exportAudioProgress.setColour (juce::Label::textColourId, kMutedText);
        addAndMakeVisible (exportAudioProgress);

        configureActionComponent (exportAudioCancelButton,
                                  yesdaw::ui::UiActionId::ProjectExportAudioCancel,
                                  "Cancel audio export");
        exportAudioCancelButton.setButtonText ("Cancel");
        exportAudioCancelButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        exportAudioCancelButton.setColour (juce::TextButton::textColourOffId, kText);
        exportAudioCancelButton.onClick = [this] {
            handleAction (yesdaw::ui::UiActionId::ProjectExportAudioCancel);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (exportAudioCancelButton);

        configureActionComponent (masterLoudnessReadout, yesdaw::ui::UiActionId::MixerReadLoudness, "Master loudness");
        masterLoudnessReadout.setButtonText ("-- LUFS");
        masterLoudnessReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        masterLoudnessReadout.setColour (juce::TextButton::textColourOffId, kText);
        masterLoudnessReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadLoudness);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (masterLoudnessReadout);

        timelineInput.setComponentID (kTimelineComponentId);
        timelineInput.setTooltip ("Timeline: drag clips, drag the ruler to select a range, Shift-drag for the loop");
        timelineInput.setName ("Timeline");
        timelineInput.setTitle ("Timeline");
        timelineInput.stateProvider = [this] { return makeTimelineState(); };
        timelineInput.activeToolProvider = [this] {
            return appModel.context().activeTimelineTool;
        };
        timelineInput.onZoomToolClicked = [this] (double anchorSeconds, bool zoomOut) {
            const double factor = yesdaw::ui::UiTheme::Layout::timelineZoomToolClickFactor;
            zoomTimelineAtAnchor (anchorSeconds, zoomOut ? 1.0 / factor : factor);
            repaint();
        };
        timelineInput.onHandToolScrolled = [this] (double secondsDelta) {
            timelineScrollSeconds += secondsDelta;
            repaint();
        };
        timelineInput.onVerticalScrollRows = [this] (int rowDelta) {
            scrollTrackRowsBy (rowDelta);
        };
        timelineInput.onPencilEmptyLane = [this] (int lane, double seconds) {
            const auto& tracks = appModel.project().tracks;
            if (lane < 0 || lane >= static_cast<int> (tracks.size()))
                return;
            if (const auto tick = timelineTickFromSeconds (seconds))
                (void) appModel.addMidiClipOnTrackAt (
                    tracks[static_cast<std::size_t> (lane)].id,
                    snappedTimelineTick (*tick, false));
            refreshActionState();
            repaint();
        };
        timelineInput.onClipClicked = [this] (int timelineClipId, bool toggle) {
            selectTimelineClipByLayoutId (timelineClipId, toggle);
        };
        timelineInput.onEmptyClicked = [this] {
            appModel.clearTimelineClipSelection();
            refreshActionState();
            repaint();
        };
        timelineInput.onMarqueeSelection = [this] (std::span<const int> timelineClipLayoutIds) {
            std::vector<yesdaw::engine::EntityId> selectedClipIds;
            selectedClipIds.reserve (timelineClipLayoutIds.size());
            for (const int timelineClipLayoutId : timelineClipLayoutIds)
            {
                if (timelineClipLayoutId < 0
                    || timelineClipLayoutId >= static_cast<int> (timelineClipIds.size()))
                    return;
                selectedClipIds.push_back (timelineClipIds[static_cast<std::size_t> (timelineClipLayoutId)]);
            }

            (void) appModel.selectTimelineClips (
                std::span<const yesdaw::engine::EntityId> (selectedClipIds.data(), selectedClipIds.size()));
            refreshActionState();
            repaint();
        };
        timelineInput.onClipMoved = [this] (int timelineClipId, double startSeconds, bool snapToGrid) {
            moveTimelineClipByLayoutId (timelineClipId, startSeconds, snapToGrid);
        };
        timelineInput.onClipMovedToLane = [this] (int timelineClipId, int targetLane, double startSeconds, bool snapToGrid) {
            moveTimelineClipToLaneByLayoutId (timelineClipId, targetLane, startSeconds, snapToGrid);
        };
        timelineInput.onClipCopied = [this] (int timelineClipId, int targetLane, double startSeconds, bool snapToGrid) {
            copyTimelineClipByLayoutId (timelineClipId, targetLane, startSeconds, snapToGrid);
        };
        timelineInput.onClipSplit = [this] (int timelineClipId, double splitSeconds, bool snapInvert) {
            splitTimelineClipByLayoutId (timelineClipId, splitSeconds, snapInvert);
        };
        timelineInput.onClipTrimmedRight = [this] (int timelineClipId, double endSeconds, bool snapInvert) {
            trimTimelineClipRightByLayoutId (timelineClipId, endSeconds, snapInvert);
        };
        timelineInput.onClipTrimmedLeft = [this] (int timelineClipId, double startSeconds, bool snapInvert) {
            if (timelineClipId < 0 || timelineClipId >= static_cast<int> (timelineClipIds.size()))
                return;

            (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (timelineClipId)]);
            if (const auto tick = timelineTickFromSeconds (startSeconds))
                (void) appModel.trimSelectedTimelineClipLeftTo (snappedTimelineTick (*tick, snapInvert));

            refreshActionState();
            repaint();
        };
        timelineInput.onClipGainAdjusted = [this] (int timelineClipId, int deltaPixels) {
            adjustTimelineClipGainByLayoutId (timelineClipId, deltaPixels);
        };
        timelineInput.onClipFadeAdjusted = [this] (int timelineClipId, bool fadeIn, double fadeSeconds) {
            adjustTimelineClipFadeByLayoutId (timelineClipId, fadeIn, fadeSeconds);
        };
        timelineInput.onZoomWheel = [this] (double anchorSeconds, double wheelDelta) {
            const double factor = wheelDelta > 0.0
                ? yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep
                : 1.0 / yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep;
            zoomTimelineAtAnchor (anchorSeconds, factor);
            repaint();
        };
        timelineInput.onScrollWheel = [this] (double wheelDelta) {
            const double visibleSeconds = std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                    timelineTotalSeconds / std::max (1.0, timelineZoomFactor));
            timelineScrollSeconds -= wheelDelta * visibleSeconds
                                   * yesdaw::ui::UiTheme::Layout::timelineScrollWheelFraction;
            repaint();
        };
        timelineInput.onRulerAltClicked = [this] (double seconds) {
            if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds))
            {
                (void) appModel.removeTimelineMarkerNearestTick (*tick);
                refreshActionState();
                repaint();
            }
        };
        timelineInput.onLoopRegionDragged = [this] (double startSeconds, double endSeconds, bool snapInvert) {
            const std::optional<yesdaw::engine::Tick> startFrame = timelineTickFromSeconds (startSeconds);
            const std::optional<yesdaw::engine::Tick> endFrame = timelineTickFromSeconds (endSeconds);
            if (startFrame && endFrame)
            {
                const yesdaw::engine::Tick snappedStart = snappedTimelineTick (*startFrame, snapInvert);
                const yesdaw::engine::Tick snappedEnd = snappedTimelineTick (*endFrame, snapInvert);
                if (snappedEnd > snappedStart)
                {
                    (void) appModel.setPlaybackLoopRegion (snappedStart, snappedEnd);
                    refreshActionState();
                    repaint();
                }
            }
        };
        timelineInput.onTimelineLocated = [this] (double seconds) {
            if (const std::optional<yesdaw::engine::Tick> frame = timelineTickFromSeconds (seconds))
            {
                (void) appModel.locatePlaybackFrame (*frame);
                refreshActionState();
                repaint();
            }
        };
        timelineInput.onRulerRangeSelected = [this] (double startSeconds, double endSeconds, bool snapInvert) {
            const std::optional<yesdaw::engine::Tick> startFrame = timelineTickFromSeconds (startSeconds);
            const std::optional<yesdaw::engine::Tick> endFrame = timelineTickFromSeconds (endSeconds);
            if (startFrame && endFrame)
            {
                const yesdaw::engine::Tick snappedStart = snappedTimelineTick (*startFrame, snapInvert);
                const yesdaw::engine::Tick snappedEnd = snappedTimelineTick (*endFrame, snapInvert);
                if (snappedEnd > snappedStart)
                {
                    (void) appModel.setTimelineRangeSelection (snappedStart, snappedEnd);
                    refreshActionState();
                    repaint();
                }
            }
        };
        timelineInput.onRulerRangeCleared = [this] {
            appModel.clearTimelineRangeSelection();
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (timelineInput);

        // Interactive Track rail (usable-DAW P0): row click selects the Track for import/mixer/remove
        // targeting, double-click (or F2) opens the inline rename editor, and the Add Track button
        // drives the same undoable verb as Ctrl+T.
        trackListInput.setComponentID ("shell.tracklist.input");
        trackListInput.setName ("Track List");
        trackListInput.setTitle ("Track List");
        trackListInput.setTooltip ("Track rail: click to select, drag PAN/VOL minis, click M/S/meter");
        trackListInput.rowCountProvider = [this] {
            return appModel.context().projectLoaded ? static_cast<int> (appModel.project().tracks.size()) : 0;
        };
        trackListInput.rowScrollProvider = [this] { return timelineTrackScrollRows; };
        trackListInput.onVerticalScrollRows = [this] (int rowDelta) { scrollTrackRowsBy (rowDelta); };
        trackListInput.onRowClicked = [this] (int row) { selectTrackLane (row); };
        trackListInput.panValueProvider = [this] (int row) {
            const auto& tracks = appModel.project().tracks;
            return row >= 0 && row < static_cast<int> (tracks.size())
                       ? tracks[static_cast<std::size_t> (row)].strip.pan
                       : 0.0f;
        };
        trackListInput.volumeValueProvider = [this] (int row) {
            const auto& tracks = appModel.project().tracks;
            return row >= 0 && row < static_cast<int> (tracks.size())
                       ? juce::jlimit (0.0f, 1.0f, tracks[static_cast<std::size_t> (row)].strip.linearGain)
                       : 0.0f;
        };
        trackListInput.onRowDoubleClicked = [this] (int row) {
            selectTrackLane (row);
            openTrackRenameEditor();
        };
        // Rail mini controls (usable-DAW P2): the painted PAN/VOL/M/S become live per-track edits
        // through the same selected-strip verbs the mixer uses (rail selection stays on the rail).
        trackListInput.onPanEdited = [this] (int row, float pan) {
            selectTrackLane (row);
            if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
                (void) appModel.setSelectedMixerPan (pan);
            refreshActionState();
            repaint();
        };
        trackListInput.onVolumeEdited = [this] (int row, float linearGain) {
            selectTrackLane (row);
            if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
                (void) appModel.setSelectedMixerFader (linearGain);
            showDragDbReadout (trackListInput.volumeSliderBounds (row)
                                   .translated (trackListInput.getX(), trackListInput.getY()),
                               linearGain);
            refreshActionState();
            repaint();
        };
        trackListInput.onMiniDragEnded = [this] { hideDragDbReadout(); };
        trackListInput.onMeterClicked = [this] (int row) { clearTrackMeterHold (row); };
        trackListInput.onMuteToggled = [this] (int row) {
            selectTrackLane (row);
            if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
                (void) appModel.toggleSelectedMixerMute();
            refreshActionState();
            repaint();
        };
        trackListInput.onSoloToggled = [this] (int row) {
            selectTrackLane (row);
            if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
                (void) appModel.toggleSelectedMixerSolo();
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (trackListInput);

        // Header tempo + time-signature editing (usable-DAW P0): the painted readouts become real
        // undoable controls. Tempo is a drag/scrub bar over the TEMPO cell; meter picks common signatures.
        configureActionComponent (headerTempoControl, yesdaw::ui::UiActionId::TransportSetTempo, "Set tempo");
        headerTempoControl.setSliderStyle (juce::Slider::LinearBar);
        headerTempoControl.setTextBoxStyle (juce::Slider::TextBoxLeft,
                                            false,
                                            yesdaw::ui::UiTheme::Layout::headerTempoTextWidth,
                                            yesdaw::ui::UiTheme::Layout::headerTempoTextHeight);
        headerTempoControl.setRange (yesdaw::ui::UiTheme::Layout::headerTempoMinBpm,
                                     yesdaw::ui::UiTheme::Layout::headerTempoMaxBpm,
                                     yesdaw::ui::UiTheme::Layout::headerTempoStepBpm);
        headerTempoControl.setValue (yesdaw::ui::UiTheme::Layout::headerTempoDefaultBpm,
                                     juce::dontSendNotification);
        headerTempoControl.setColour (juce::Slider::trackColourId, yesdaw::ui::UiTheme::Color::darkControl());
        headerTempoControl.setColour (juce::Slider::textBoxTextColourId, kText);
        headerTempoControl.onValueChange = [this] {
            if (refreshingTimeMapControls || ! headerTempoControl.isEnabled())
                return;

            (void) appModel.setProjectTempoBpm (headerTempoControl.getValue());
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (headerTempoControl);

        configureActionComponent (headerMeterChooser, yesdaw::ui::UiActionId::TransportSetMeter, "Set time signature");
        for (std::size_t i = 0; i < kHeaderMeterChoices.size(); ++i)
            headerMeterChooser.addItem (juce::String (kHeaderMeterChoices[i].first)
                                            + "/" + juce::String (kHeaderMeterChoices[i].second),
                                        static_cast<int> (i) + 1);
        headerMeterChooser.onChange = [this] {
            if (refreshingTimeMapControls)
                return;

            const int selected = headerMeterChooser.getSelectedId();
            if (selected <= 0)
                return;

            const auto& choice = kHeaderMeterChoices[static_cast<std::size_t> (selected - 1)];
            (void) appModel.setProjectMeterSignature (choice.first, choice.second);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (headerMeterChooser);

        configureActionComponent (trackAddButton, yesdaw::ui::UiActionId::TrackAdd, "Add audio track");
        trackAddButton.setButtonText ("+ Track");
        trackAddButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        trackAddButton.setColour (juce::TextButton::textColourOffId, kText);
        trackAddButton.onClick = [this] {
            if (appModel.addAudioTrack().dispatched)
                selectedTrackLane = static_cast<int> (appModel.project().tracks.size()) - 1;
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (trackAddButton);

        trackRenameEditor.setComponentID ("shell.tracklist.rename");
        trackRenameEditor.setTooltip ("Rename track: Enter commits, Escape cancels");
        trackRenameEditor.setName ("Rename track");
        trackRenameEditor.setSelectAllWhenFocused (true);
        trackRenameEditor.onReturnKey = [this] { commitTrackRenameEditor(); };
        trackRenameEditor.onEscapeKey = [this] { dismissTrackRenameEditor(); };
        trackRenameEditor.onFocusLost = [this] { dismissTrackRenameEditor(); };
        addChildComponent (trackRenameEditor);

        clipRenameEditor.setComponentID ("shell.timeline.clip.rename");
        clipRenameEditor.setTooltip ("Rename clip: Enter commits, Escape cancels");
        clipRenameEditor.setName ("Rename clip");
        clipRenameEditor.setSelectAllWhenFocused (true);
        clipRenameEditor.onReturnKey = [this] { commitClipRenameEditor(); };
        clipRenameEditor.onEscapeKey = [this] { dismissClipRenameEditor(); };
        clipRenameEditor.onFocusLost = [this] { dismissClipRenameEditor(); };
        addChildComponent (clipRenameEditor);

        // Snap grid picker (usable-DAW P1): the four registered snap actions surfaced as one control;
        // the model derives real frame grids from the head tempo/meter.
        configureActionComponent (timelineSnapChooser, yesdaw::ui::UiActionId::TimelineSnapSetBeat, "Snap grid");
        timelineSnapChooser.setComponentID ("timeline.snap.chooser");
        timelineSnapChooser.addItem ("Snap Off", 1);
        timelineSnapChooser.addItem ("Bar", 2);
        timelineSnapChooser.addItem ("Beat", 3);
        timelineSnapChooser.addItem ("1/16", 4);
        timelineSnapChooser.setSelectedId (3, juce::dontSendNotification);
        timelineSnapChooser.onChange = [this] {
            if (refreshingSnapChooser)
                return;

            const int selected = timelineSnapChooser.getSelectedId();
            const yesdaw::ui::UiActionId action =
                selected == 1 ? yesdaw::ui::UiActionId::TimelineSnapDisable
                : selected == 2 ? yesdaw::ui::UiActionId::TimelineSnapSetBar
                : selected == 4 ? yesdaw::ui::UiActionId::TimelineSnapSetSixteenth
                : yesdaw::ui::UiActionId::TimelineSnapSetBeat;
            (void) appModel.dispatch (action);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (timelineSnapChooser);

        configureActionComponent (
            timelineRepeatPasteChooser,
            yesdaw::ui::UiActionId::TimelineClipRepeatPaste,
            "Repeat paste count");
        timelineRepeatPasteChooser.setComponentID ("timeline.repeat-paste.chooser");
        timelineRepeatPasteChooser.addItem ("2x", 2);
        timelineRepeatPasteChooser.addItem ("3x", 3);
        timelineRepeatPasteChooser.addItem ("4x", 4);
        timelineRepeatPasteChooser.addItem ("8x", 8);
        timelineRepeatPasteChooser.setSelectedId (
            yesdaw::ui::UiAppModel::kDefaultRepeatPasteCount,
            juce::dontSendNotification);
        timelineRepeatPasteChooser.onChange = [this] {
            if (refreshingRepeatPasteChooser)
                return;

            appModel.setRepeatPasteCount (timelineRepeatPasteChooser.getSelectedId());
            refreshActionState();
        };
        addAndMakeVisible (timelineRepeatPasteChooser);

        configureAutomationLaneControls();

        // Automation lane canvas (usable-DAW P1): breakpoints drawn and edited against the SAME
        // timeline viewport math as the arrangement; targets the selected track's fader lane.
        automationLaneCanvas.setComponentID ("timeline.automation.canvas");
        automationLaneCanvas.setTooltip ("Automation lane: click to add a breakpoint, drag to move it");
        automationLaneCanvas.setName ("Automation Lane");
        automationLaneCanvas.setTitle ("Automation Lane");
        automationLaneCanvas.pointsProvider = [this] {
            std::vector<std::pair<double, double>> points;
            const yesdaw::engine::EntityId trackId = automationTargetTrackId();
            if (! trackId.isValid() || ! appModel.project().sampleRate.isValid())
                return points;

            if (const yesdaw::engine::AutomationLaneData* const lane = appModel.trackFaderAutomationLane (trackId))
            {
                const double sampleRateHz = appModel.project().sampleRate.hz;
                points.reserve (lane->points.size());
                for (const yesdaw::engine::AutomationBreakpoint& point : lane->points)
                    points.emplace_back (static_cast<double> (point.tick) / sampleRateHz, point.value);
            }
            return points;
        };
        automationLaneCanvas.secondsForLocalX = [this] (int localX) {
            return automationCanvasSecondsForLocalX (localX);
        };
        automationLaneCanvas.localXForSeconds = [this] (double seconds) {
            return automationCanvasLocalXForSeconds (seconds);
        };
        automationLaneCanvas.onAddPoint = [this] (double seconds, double value) {
            const yesdaw::engine::EntityId trackId = automationTargetTrackId();
            if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds);
                tick && trackId.isValid())
            {
                (void) appModel.addAutomationBreakpointToTrackLane (trackId, *tick, value);
                refreshActionState();
                repaint();
            }
        };
        automationLaneCanvas.onMovePoint = [this] (double oldSeconds, double newSeconds, double newValue) {
            const yesdaw::engine::EntityId trackId = automationTargetTrackId();
            const yesdaw::engine::AutomationLaneData* const lane =
                trackId.isValid() ? appModel.trackFaderAutomationLane (trackId) : nullptr;
            const std::optional<yesdaw::engine::Tick> oldTick = timelineTickFromSeconds (oldSeconds);
            const std::optional<yesdaw::engine::Tick> newTick = timelineTickFromSeconds (newSeconds);
            if (lane != nullptr && oldTick && newTick)
            {
                (void) appModel.moveAutomationBreakpointTo (lane->id, *oldTick, *newTick, newValue);
                refreshActionState();
                repaint();
            }
        };
        automationLaneCanvas.onDeletePoint = [this] (double seconds) {
            const yesdaw::engine::EntityId trackId = automationTargetTrackId();
            const yesdaw::engine::AutomationLaneData* const lane =
                trackId.isValid() ? appModel.trackFaderAutomationLane (trackId) : nullptr;
            if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds);
                lane != nullptr && tick)
            {
                (void) appModel.removeAutomationBreakpointAtTick (lane->id, *tick);
                refreshActionState();
                repaint();
            }
        };
        addChildComponent (automationLaneCanvas);

        pianoRollInput.setComponentID (kPianoRollComponentId);
        pianoRollInput.setTooltip ("Piano roll: click to pencil a note, drag to move, Ctrl+drag to copy, Alt+wheel for velocity");
        pianoRollInput.setName ("Piano Roll");
        pianoRollInput.setTitle ("Piano Roll");
        pianoRollInput.stateProvider = [this] { return currentPianoRollSurface(); };
        pianoRollInput.onNoteClicked = [this] (yesdaw::engine::EntityId midiClipId,
                                               yesdaw::engine::EntityId noteId) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            refreshActionState();
            repaint();
        };
        pianoRollInput.onNoteMoved = [this] (yesdaw::engine::EntityId midiClipId,
                                             yesdaw::engine::EntityId noteId,
                                             yesdaw::engine::Tick startTick) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.moveSelectedPianoRollNoteTo (startTick);
            refreshActionState();
            repaint();
        };
        pianoRollInput.onNoteLengthChanged = [this] (yesdaw::engine::EntityId midiClipId,
                                                     yesdaw::engine::EntityId noteId,
                                                     yesdaw::engine::Tick lengthTicks) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.setSelectedPianoRollNoteLength (lengthTicks);
            refreshActionState();
            repaint();
        };
        pianoRollInput.onNoteTransposed = [this] (yesdaw::engine::EntityId midiClipId,
                                                  yesdaw::engine::EntityId noteId,
                                                  std::int32_t semitones) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.transposeSelectedPianoRollNote (semitones);
            refreshActionState();
            repaint();
        };
        pianoRollInput.onNoteQuantized = [this] (yesdaw::engine::EntityId midiClipId,
                                                 yesdaw::engine::EntityId noteId,
                                                 yesdaw::engine::Tick snapGridTicks) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.quantizeSelectedPianoRollNoteTo (yesdaw::engine::SnapGrid { snapGridTicks });
            refreshActionState();
            repaint();
        };
        pianoRollInput.onNoteAdded = [this] (yesdaw::engine::EntityId midiClipId, yesdaw::engine::Tick tick, std::int16_t key) {
            (void) midiClipId;
            (void) appModel.addPianoRollNoteAt (tick, kPianoRollSnapGridTicks, key);
            refreshActionState();
            repaint();
        };
        pianoRollInput.onExpressionRead = [this] {
            (void) appModel.readPianoRollExpressionLanes();
            refreshActionState();
            repaint();
        };
        pianoRollInput.onNoteVelocityAdjusted = [this] (yesdaw::engine::EntityId midiClipId,
                                                        yesdaw::engine::EntityId noteId,
                                                        double normalizedVelocity) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.setSelectedPianoRollNoteVelocity (normalizedVelocity);
            refreshActionState();
            repaint();
        };
        pianoRollInput.onNoteCopyDragged = [this] (yesdaw::engine::EntityId midiClipId,
                                                   yesdaw::engine::EntityId noteId,
                                                   yesdaw::engine::Tick newStartTick) {
            (void) appModel.duplicatePianoRollNote (midiClipId, noteId, newStartTick);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (pianoRollInput);

        menuBar.setModel (this);
        menuBar.setComponentID ("shell.menubar");
        menuBar.setName ("Menu bar");
        menuBar.setTitle ("Menu bar");
        menuBar.setTooltip ("Application menus: File, Edit, View, Options, Help");
        addAndMakeVisible (menuBar);

        // Real audio device chooser (usable-DAW P1): lists the machine's output devices and switches
        // the live device on selection. The harness injects deterministic device seams; the native
        // shell enumerates and switches through the JUCE device manager.
        audioDeviceChooser.setComponentID ("shell.device.chooser");
        audioDeviceChooser.setTooltip ("Audio output device");
        audioDeviceChooser.setName ("Audio output device");
        audioDeviceChooser.setTitle ("Audio output device");
        audioDeviceChooser.setTextWhenNothingSelected ("Audio Device");
        audioDeviceChooser.setTextWhenNoChoicesAvailable ("No Devices");
        audioDeviceChooser.onChange = [this] {
            if (refreshingAudioDeviceChooser)
                return;

            const int selected = audioDeviceChooser.getSelectedId();
            if (selected <= 0
                || static_cast<std::size_t> (selected - 1) >= audioDeviceChooserNames.size())
                return;

            suspendDesktopAudioCallback();
            const bool switched =
                selectAudioOutputDeviceByName (audioDeviceChooserNames[static_cast<std::size_t> (selected - 1)]);
            resumeDesktopAudioCallback();
            if (switched)
                if (juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
                    appModel.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
            refreshAudioDeviceChooser();
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (audioDeviceChooser);
        refreshAudioDeviceChooser();

        configureInspectorControls();
        configureMixerControls();
        resized();
        refreshActionState();

        if (desktopAudioRequested)
        {
            // Native shell only: remember and reopen the last project so a crash-then-relaunch reaches
            // the autosave recovery prompt with no manual navigation (usable-DAW P1). The harness never
            // takes this path, so injected-choice tests stay deterministic.
            {
                const std::string sessionUtf8 =
                    juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("YES DAW").getFullPathName().toStdString();
                const auto* sessionBytes = reinterpret_cast<const char8_t*> (sessionUtf8.data());
                appModel.setSessionStateDirectory (
                    std::filesystem::path { std::u8string (sessionBytes, sessionBytes + sessionUtf8.size()) });
            }
            const std::filesystem::path lastProject = appModel.readLastProjectRecord();
            if (! lastProject.empty())
            {
                if (auto decodedAssets = decodeStoredProjectAssets (lastProject); decodedAssets && ! decodedAssets->empty())
                    (void) appModel.loadProjectBundle (
                        lastProject,
                        std::span<const yesdaw::ui::UiDecodedAsset> (
                            decodedAssets->data(), decodedAssets->size()));
                else if (decodedAssets)
                    (void) appModel.openProjectBundle (lastProject);
            }

            // Request stereo input so the shipped Record button can capture real audio (P0-1); fall
            // back to output-only when no input device exists so playback never regresses.
            juce::String error = audioDeviceManager.initialiseWithDefaultDevices (2, 2);
            if (! error.isEmpty() || audioDeviceManager.getCurrentAudioDevice() == nullptr)
                error = audioDeviceManager.initialiseWithDefaultDevices (0, 2);
            if (error.isEmpty())
            {
                if (juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
                    appModel.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
                audioDeviceManager.addAudioCallback (this);
                desktopAudioCallbackRegistered = true;
                desktopAudioOpen.store (true, std::memory_order_release);
                refreshAudioDeviceChooser();   // now the current device can be marked selected
            }
        }

        // H17 CP4: scheduled autosave is ON by default (policy lives in the headless app model, so the
        // default is covered by a headless test). The Timer fires on the message thread — which is this
        // app's control thread — so writeAutosaveTick()'s heavy SQLite/asset I/O is on the right thread.
        startTimer (kUiRefreshIntervalMs);
    }

    ~MainComponent() override
    {
        menuBar.setModel (nullptr);
        stopTimer();
        if (desktopAudioCallbackRegistered)
            audioDeviceManager.removeAudioCallback (this);
        audioDeviceManager.closeAudioDevice();
        setLookAndFeel (nullptr);
    }

    // The UI polls the lock-free audio-thread transport snapshot at ~30 Hz. Autosave remains on its
    // independent slow schedule and never runs in the device callback.
    void timerCallback() override
    {
        appModel.refreshTransportSnapshot();
        appModel.serviceRecordingCountIn();
        if (appModel.realRecordingCaptureActive())
            appModel.drainRealRecordingCapture();
        updateTrackMeterHoldStates();
        pushWindowTitle();
        refreshActionState();
        followPlaybackPlayhead();
        repaint();

        if (! appModel.autosaveSchedule().enabled)
            return;

        autosaveElapsedMs += kUiRefreshIntervalMs;
        if (autosaveElapsedMs >= appModel.autosaveSchedule().intervalMs)
        {
            autosaveElapsedMs = 0;
            (void) appModel.writeAutosaveTick();
        }
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        if (device != nullptr)
            appModel.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
        desktopAudioOpen.store (device != nullptr, std::memory_order_release);
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannels,
                                           int numInputChannels,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        (void) appModel.processDeviceAudioBlock (
            inputChannels, numInputChannels, outputChannels, numOutputChannels, numFrames);
        accountDeviceBlockPeaks (outputChannels, numOutputChannels, numFrames);
    }

    void audioDeviceStopped() override
    {
        desktopAudioOpen.store (false, std::memory_order_release);
    }

    void audioDeviceError (const juce::String&) override
    {
        desktopAudioOpen.store (false, std::memory_order_release);
    }

    [[nodiscard]] bool processDeviceAudioBlock (float* const* outputChannels,
                                                int numOutputChannels,
                                                int numFrames) noexcept
    {
        const bool processed = appModel.processDeviceAudioBlock (
            outputChannels, numOutputChannels, numFrames);
        accountDeviceBlockPeaks (outputChannels, numOutputChannels, numFrames);
        return processed;
    }

    void accountDeviceBlockPeaks (float* const* outputChannels,
                                  int numOutputChannels,
                                  int numFrames) noexcept
    {
        float peak = 0.0f;
        float leftPeak = 0.0f;
        float rightPeak = 0.0f;
        if (outputChannels != nullptr && numFrames > 0)
        {
            for (int channel = 0; channel < numOutputChannels; ++channel)
                if (outputChannels[channel] != nullptr)
                    for (int frame = 0; frame < numFrames; ++frame)
                    {
                        const float samplePeak = std::fabs (outputChannels[channel][frame]);
                        peak = std::max (peak, samplePeak);
                        if (channel == 0)
                            leftPeak = std::max (leftPeak, samplePeak);
                        else if (channel == 1)
                            rightPeak = std::max (rightPeak, samplePeak);
                    }
        }

        liveMasterPeakLeft.store (leftPeak, std::memory_order_release);
        liveMasterPeakRight.store (rightPeak, std::memory_order_release);

        deviceAudioCallbackBlockCount.fetch_add (1u, std::memory_order_relaxed);
        if (peak > 0.000001f)
            deviceAudioNonSilentBlockCount.fetch_add (1u, std::memory_order_relaxed);
    }

    [[nodiscard]] yesdaw::ui::UiActionContext harnessContext() const noexcept { return appModel.contextSnapshot(); }
    [[nodiscard]] const yesdaw::ui::UiRecordingDeviceSelection& harnessRecordingDevice() const noexcept
    {
        return appModel.recordingDeviceSelection();
    }
    [[nodiscard]] const yesdaw::ui::UiRecordingTrackInputSelection& harnessRecordingTrackInput() const noexcept
    {
        return appModel.recordingTrackInputSelection();
    }
    [[nodiscard]] const yesdaw::ui::UiRecordedAudioTake& harnessLastRecordedAudioTake() const noexcept
    {
        return appModel.lastRecordedAudioTake();
    }
    [[nodiscard]] const yesdaw::ui::UiRecordedMidiTake& harnessLastRecordedMidiTake() const noexcept
    {
        return appModel.lastRecordedMidiTake();
    }
    [[nodiscard]] const yesdaw::ui::UiRecordingCompSelection& harnessRecordingComp() const noexcept
    {
        return appModel.recordingCompSelection();
    }
    [[nodiscard]] const yesdaw::ui::UiAutosaveRecoveryPrompt& harnessAutosaveRecovery() const noexcept
    {
        return appModel.autosaveRecoveryPrompt();
    }
    [[nodiscard]] const std::filesystem::path& harnessBundlePath() const noexcept { return appModel.bundlePath(); }
    [[nodiscard]] bool harnessPrimaryFileChoicesReady() const noexcept
    {
        return static_cast<bool> (fileChoices.chooseNewProjectBundle)
            && static_cast<bool> (fileChoices.chooseOpenProjectBundle)
            && static_cast<bool> (fileChoices.chooseImportAudioFile)
            && static_cast<bool> (fileChoices.chooseExportAudioFile);
    }
    [[nodiscard]] bool harnessPlaybackReady() const noexcept { return appModel.playbackReady(); }
    [[nodiscard]] long long harnessPlaybackLoopStartFrame() const noexcept { return appModel.playbackLoopStartFrame(); }
    [[nodiscard]] long long harnessPlaybackLoopEndFrame() const noexcept { return appModel.playbackLoopEndFrame(); }
    [[nodiscard]] long long harnessTimelineRangeStartFrame() const noexcept { return appModel.timelineRangeStartFrame(); }
    [[nodiscard]] long long harnessTimelineRangeEndFrame() const noexcept { return appModel.timelineRangeEndFrame(); }
    [[nodiscard]] double harnessTimelineZoomFactor() const noexcept { return timelineZoomFactor; }
    [[nodiscard]] double harnessTimelineScrollSeconds() const noexcept { return timelineScrollSeconds; }
    [[nodiscard]] int harnessTimelineTrackScrollRows() const noexcept { return timelineTrackScrollRows; }
    [[nodiscard]] int harnessTimelineMaxTrackScrollRows() const
    {
        // The scroll clamp depends only on the lane count and the surface heights.
        yesdaw::ui::TimelineCanvasState state;
        state.trackCount = appModel.context().projectLoaded
            ? static_cast<int> (appModel.project().tracks.size())
            : 0;
        return std::max (
            yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state).maxTrackScrollRows,
            trackListInput.maxScrollRows());
    }
    [[nodiscard]] int harnessVisibleTimelineTrackCount() const
    {
        return appModel.context().projectLoaded ? static_cast<int> (projectTimelineTracks.size()) : 0;
    }
    [[nodiscard]] int harnessVisibleTimelineClipCount() const
    {
        return appModel.context().projectLoaded ? static_cast<int> (timelineClips.size()) : 0;
    }
    [[nodiscard]] std::string harnessVisibleFirstTimelineClipName() const
    {
        if (! appModel.context().projectLoaded || timelineClips.empty() || timelineClips.front().name == nullptr)
            return {};
        return timelineClips.front().name;
    }
    [[nodiscard]] int harnessSelectedTimelineClipCount() const
    {
        return static_cast<int> (appModel.selectedTimelineClipCount());
    }
    [[nodiscard]] double harnessVisibleTimelineTotalSeconds() const noexcept
    {
        return timelineTotalSeconds;
    }
    [[nodiscard]] int harnessVisibleMixerTrackCount() const
    {
        return static_cast<int> (currentMixerSurface().tracks.size());
    }
    [[nodiscard]] int harnessVisibleMixerBusCount() const
    {
        return static_cast<int> (currentMixerSurface().buses.size());
    }
    [[nodiscard]] bool harnessVisibleMixerLoudnessValid() const
    {
        return currentMixerSurface().loudness.valid;
    }
    [[nodiscard]] int harnessVisiblePianoRollNoteCount() const
    {
        return static_cast<int> (currentPianoRollSurface().notes.size());
    }
    [[nodiscard]] float harnessVisibleMasterPeakLeft() const noexcept
    {
        return liveMasterPeakLeft.load (std::memory_order_acquire);
    }
    [[nodiscard]] float harnessVisibleMasterPeakRight() const noexcept
    {
        return liveMasterPeakRight.load (std::memory_order_acquire);
    }
    [[nodiscard]] bool harnessDesktopAudioRequested() const noexcept { return desktopAudioRequested; }
    [[nodiscard]] bool harnessDesktopAudioOpen() const noexcept
    {
        return desktopAudioOpen.load (std::memory_order_acquire);
    }
    [[nodiscard]] std::uint64_t harnessDeviceAudioCallbackBlockCount() const noexcept
    {
        return deviceAudioCallbackBlockCount.load (std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t harnessDeviceAudioNonSilentBlockCount() const noexcept
    {
        return deviceAudioNonSilentBlockCount.load (std::memory_order_relaxed);
    }
    [[nodiscard]] bool harnessProcessDeviceAudioBlock (float* const* outputChannels,
                                                       int numOutputChannels,
                                                       int numFrames) noexcept
    {
        return processDeviceAudioBlock (outputChannels, numOutputChannels, numFrames);
    }
    [[nodiscard]] std::vector<float> harnessRenderPlaybackFrames (std::uint64_t frames, int blockSize)
    {
        return appModel.renderPlaybackFrames (frames, blockSize);
    }

    // Window title with the dirty marker (B38): "<bundle stem>[*] - YES DAW" once a project is
    // open; empty otherwise so the app keeps its versioned startup title. State-derived, so the
    // harness snapshot reads it directly and the UI tick pushes it to the native window.
    [[nodiscard]] juce::String computedWindowTitle() const
    {
        if (! appModel.context().projectLoaded || appModel.bundlePath().empty())
            return {};

        const juce::String stem (appModel.bundlePath().stem().string());
        return stem + (appModel.hasUnsavedChanges() ? "*" : "") + " - YES DAW";
    }

    void pushWindowTitle()
    {
        const juce::String title = computedWindowTitle();
        if (title.isEmpty() || title == lastPushedWindowTitle)
            return;

        lastPushedWindowTitle = title;
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setName (title);
    }

    // Close-confirm flow (B37): a clean session closes silently; edits since the last explicit
    // Save ask through the injectable seam (native three-way box otherwise). Closing never rolls
    // back the always-persisted bundle; Save records this state as the saved version.
    [[nodiscard]] bool confirmClose()
    {
        if (! appModel.hasUnsavedChanges())
            return true;

        int choice = yesdaw::ui::kCloseChoiceCancel;
        if (fileChoices.confirmCloseUnsavedChanges)
        {
            choice = fileChoices.confirmCloseUnsavedChanges();
        }
        else
        {
            const int native = juce::AlertWindow::showYesNoCancelBox (
                juce::MessageBoxIconType::QuestionIcon,
                "Unsaved changes",
                "Save this state as your saved version before closing?\n"
                "(Every edit is already stored in the project bundle.)",
                "Save",
                "Close without saving",
                "Cancel");
            choice = native == 1 ? yesdaw::ui::kCloseChoiceSave
                   : native == 2 ? yesdaw::ui::kCloseChoiceClose
                                 : yesdaw::ui::kCloseChoiceCancel;
        }

        if (choice == yesdaw::ui::kCloseChoiceSave)
        {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::ProjectSave);
            return ! appModel.hasUnsavedChanges();   // a failed save keeps the app open
        }

        return choice == yesdaw::ui::kCloseChoiceClose;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBackground);
        drawHeader (g);

        const auto bounds = getLocalBounds();
        const auto top = bounds.withHeight (kHeaderHeight);
        g.setColour (yesdaw::ui::UiTheme::Color::separator());
        g.fillRect (top.withBottom (kHeaderHeight)
                        .removeFromBottom (yesdaw::ui::UiTheme::Layout::shellHeaderSeparatorHeight));

        auto work = bounds.withTrimmedTop (kHeaderHeight);
        if (appModel.context().activePanel == yesdaw::ui::UiPanel::Mixer)
        {
            drawMixer (g, mixerPanelBounds());
            return;
        }

        work.removeFromBottom (kMixerHeight);
        auto left = work.removeFromLeft (kLeftRailWidth)
                        .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                  yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
        auto inspector = work.removeFromRight (kInspectorWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
        auto timeline = work.reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                      yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);

        drawTrackList (g, left);
        if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll)
            drawPianoRoll (g, timeline);
        drawInspector (g, inspector);
        drawMixer (g, mixerPanelBounds());
    }

    void resized() override
    {
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();

        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto action = toolbarActions[i];
            switch (action)
            {
                case yesdaw::ui::UiActionId::ProjectNew:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::projectNewButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::ProjectOpen:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::projectOpenButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::ProjectSave:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::projectSaveButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::ProjectImportAudio:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::projectImportAudioButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::DeviceRefreshAudio:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::deviceRefreshAudioButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::DeviceSelectTestAudio:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::deviceSelectTestAudioButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::RecordingArmTrack:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::recordingArmTrackButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::RecordingSetMonitoringPolicy:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::recordingSetMonitoringPolicyButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::TransportRecord:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::transportRecordButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::RecordingAssembleComp:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::recordingAssembleCompButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::EditUndo:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::editUndoButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::EditRedo:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::editRedoButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::TransportLocateStart:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::transportLocateStartButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::TransportPlay:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::transportPlayButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::TransportStop:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::transportStopButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::TransportToggleLoop:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::transportToggleLoopButtonBounds());
                    break;
                case yesdaw::ui::UiActionId::ViewMixer:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::viewMixerButtonBounds (mixerPanelBounds()));
                    break;
                case yesdaw::ui::UiActionId::ViewPianoRoll:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::viewPianoRollButtonBounds (mixerPanelBounds()));
                    break;
                default: buttons[i].setBounds ({});
            }
        }

        autosaveRestoreButton.setBounds (yesdaw::ui::UiTheme::Layout::autosaveRestoreButtonBounds());
        autosaveDiscardButton.setBounds (yesdaw::ui::UiTheme::Layout::autosaveDiscardButtonBounds());
        audioDeviceChooser.setBounds (yesdaw::ui::UiTheme::Layout::audioDeviceChooserBounds());
        exportAudioButton.setBounds (yesdaw::ui::UiTheme::Layout::projectExportAudioButtonBounds());
        exportAudioProgress.setBounds (yesdaw::ui::UiTheme::Layout::projectExportAudioProgressBounds());
        exportAudioCancelButton.setBounds (yesdaw::ui::UiTheme::Layout::projectExportAudioCancelButtonBounds());
        exportBitDepthChooser.setBounds (yesdaw::ui::UiTheme::Layout::exportBitDepthChooserBounds());
        exportRangeChooser.setBounds (yesdaw::ui::UiTheme::Layout::exportRangeChooserBounds());
        menuBar.setBounds (yesdaw::ui::UiTheme::Layout::headerMenuBarBounds());
        masterLoudnessReadout.setBounds (juce::Rectangle<int> (yesdaw::ui::UiTheme::Layout::headerMasterLufsX,
                                                               yesdaw::ui::UiTheme::Layout::headerMasterLufsY,
                                                               yesdaw::ui::UiTheme::Layout::headerMasterLufsWidth,
                                                               yesdaw::ui::UiTheme::Layout::headerMasterLufsHeight));
        timelineInput.setBounds (timelineBounds());
        pianoRollInput.setBounds (timelineBounds());
        trackListInput.setBounds (leftRailPanelBounds());
        {
            auto strips = mixerPanelBounds();
            strips.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerToolsWidth);
            mixerStripsInput.setBounds (strips);
        }
        {
            auto box = juce::Rectangle<int> (
                yesdaw::ui::UiTheme::Layout::headerTransportBoxX,
                yesdaw::ui::UiTheme::Layout::headerTransportReadoutY,
                yesdaw::ui::UiTheme::Layout::headerTransportBoxWidth,
                yesdaw::ui::UiTheme::Layout::headerTransportReadoutHeight);
            auto tempoCell = box.removeFromLeft (yesdaw::ui::UiTheme::Layout::headerTransportCellWidth);
            headerTempoControl.setBounds (
                tempoCell.reduced (yesdaw::ui::UiTheme::Layout::headerTransportCellInsetX,
                                   yesdaw::ui::UiTheme::Layout::headerTransportValueInsetY)
                    .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportValueHeight));
            auto meterCell = box.removeFromLeft (yesdaw::ui::UiTheme::Layout::headerTransportCellWidth);
            headerMeterChooser.setBounds (
                meterCell.reduced (yesdaw::ui::UiTheme::Layout::headerTransportCellInsetX,
                                   yesdaw::ui::UiTheme::Layout::headerTransportValueInsetY)
                    .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportValueHeight));
        }
        {
            const auto rail = leftRailPanelBounds();
            trackAddButton.setBounds (
                rail.getRight() - yesdaw::ui::UiTheme::Layout::trackListAddButtonWidth
                    - yesdaw::ui::UiTheme::Layout::trackListAddButtonInset,
                rail.getY() + yesdaw::ui::UiTheme::Layout::trackListAddButtonInset,
                yesdaw::ui::UiTheme::Layout::trackListAddButtonWidth,
                yesdaw::ui::UiTheme::Layout::trackListAddButtonHeight);
        }
        {
            const auto automationBounds =
                yesdaw::ui::UiTheme::Layout::automationLaneToggleBounds (timelineBounds());
            const juce::Rectangle<int> snapBounds {
                automationBounds.getX() - yesdaw::ui::UiTheme::Layout::timelineSnapChooserWidth
                    - yesdaw::ui::UiTheme::Layout::timelineSnapChooserGap,
                automationBounds.getY(),
                yesdaw::ui::UiTheme::Layout::timelineSnapChooserWidth,
                automationBounds.getHeight()
            };
            timelineSnapChooser.setBounds (snapBounds);
            timelineRepeatPasteChooser.setBounds (
                snapBounds.getX() - yesdaw::ui::UiTheme::Layout::timelineRepeatPasteChooserWidth
                    - yesdaw::ui::UiTheme::Layout::timelineRepeatPasteChooserGap,
                snapBounds.getY(),
                yesdaw::ui::UiTheme::Layout::timelineRepeatPasteChooserWidth,
                snapBounds.getHeight());
        }
        layoutAutomationLaneControls();
        layoutInspectorControls();
        layoutMixerControls();
    }

private:
    template <typename Component>
    void configureActionComponent (Component& component,
                                   yesdaw::ui::UiActionId action,
                                   const juce::String& fallbackName)
    {
        if (const auto* descriptor = appModel.registry().descriptor (action))
        {
            component.setComponentID (descriptor->stableId);
            component.setName (descriptor->accessibleName);
            component.setTitle (descriptor->label);
            // The tooltip names the action and its chord straight from the descriptor table so it
            // can never drift from the keymap (B40).
            component.setTooltip (juce::String (descriptor->accessibleName)
                                  + "  (" + descriptor->defaultKey + ")");
            return;
        }

        component.setName (fallbackName);
    }

    void configureAutosaveRecoveryButton (juce::TextButton& button, yesdaw::ui::UiActionId action)
    {
        const auto* descriptor = appModel.registry().descriptor (action);
        if (descriptor == nullptr)
            return;

        button.setButtonText (actionButtonText (action));
        button.setComponentID (descriptor->stableId);
        button.setName (descriptor->accessibleName);
        button.setTooltip (juce::String (descriptor->stableId) + "  " + descriptor->defaultKey);
        button.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::warningButton());
        button.setColour (juce::TextButton::textColourOffId, kText);
        button.onClick = [this, action] {
            (void) appModel.dispatch (action);
            refreshActionState();
            repaint();
        };
        button.setVisible (false);
        addAndMakeVisible (button);
    }

    void configureAutomationLaneControls()
    {
        constexpr yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::TimelineAutomationToggleTrackLane;
        configureActionComponent (automationLaneToggle, action, "Automation lanes");
        if (const auto* descriptor = appModel.registry().descriptor (action))
            automationLaneToggle.setButtonText (descriptor->label);
        else
            automationLaneToggle.setButtonText ("Automation");
        automationLaneToggle.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        automationLaneToggle.setColour (juce::TextButton::buttonOnColourId, kPurple.darker (0.45f));
        automationLaneToggle.setColour (juce::TextButton::textColourOffId, kText);
        automationLaneToggle.setColour (juce::TextButton::textColourOnId, kText);
        automationLaneToggle.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::TimelineAutomationToggleTrackLane);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (automationLaneToggle);

        automationLaneRow.setComponentID (kAutomationLaneRowComponentId);
        automationLaneRow.setTooltip ("First Track automation lane row");
        automationLaneRow.setName ("First Track automation lane");
        automationLaneRow.setTitle ("First Track automation lane");
        automationLaneRow.setTooltip (kAutomationLaneRowComponentId);
        automationLaneRow.setJustificationType (juce::Justification::centredLeft);
        automationLaneRow.setColour (juce::Label::backgroundColourId, yesdaw::ui::UiTheme::Color::selectedLane());
        automationLaneRow.setColour (juce::Label::textColourId, kText);
        automationLaneRow.setVisible (false);
        addAndMakeVisible (automationLaneRow);

        constexpr yesdaw::ui::UiActionId addAction = yesdaw::ui::UiActionId::TimelineAutomationAddBreakpoint;
        configureActionComponent (automationBreakpointAddButton, addAction, "Add automation breakpoint");
        if (const auto* descriptor = appModel.registry().descriptor (addAction))
            automationBreakpointAddButton.setButtonText (descriptor->label);
        automationBreakpointAddButton.setColour (juce::TextButton::buttonColourId,
                                                 yesdaw::ui::UiTheme::Color::buttonSurface());
        automationBreakpointAddButton.onClick = [this] {
            (void) appModel.addFirstTrackAutomationBreakpoint();
            refreshActionState();
            repaint();
        };
        automationBreakpointAddButton.setVisible (false);
        addAndMakeVisible (automationBreakpointAddButton);

        constexpr yesdaw::ui::UiActionId deleteAction = yesdaw::ui::UiActionId::TimelineAutomationDeleteBreakpoint;
        configureActionComponent (automationBreakpointDeleteButton, deleteAction, "Delete automation breakpoint");
        if (const auto* descriptor = appModel.registry().descriptor (deleteAction))
            automationBreakpointDeleteButton.setButtonText (descriptor->label);
        automationBreakpointDeleteButton.setColour (juce::TextButton::buttonColourId,
                                                    yesdaw::ui::UiTheme::Color::buttonSurface());
        automationBreakpointDeleteButton.onClick = [this] {
            (void) appModel.deleteLastFirstTrackAutomationBreakpoint();
            refreshActionState();
            repaint();
        };
        automationBreakpointDeleteButton.setVisible (false);
        addAndMakeVisible (automationBreakpointDeleteButton);
    }

    void configureInspectorControls()
    {
        configureInspectorTimeSlider (inspectorStart, kInspectorStartComponentId, "Clip start");
        inspectorStart.onValueChange = [this] {
            if (refreshingInspectorControls || ! inspectorStart.isEnabled())
                return;

            setSelectedInspectorStartFromSlider();
        };
        addAndMakeVisible (inspectorStart);

        configureInspectorTimeSlider (inspectorEnd, kInspectorEndComponentId, "Clip end");
        inspectorEnd.onValueChange = [this] {
            if (refreshingInspectorControls || ! inspectorEnd.isEnabled())
                return;

            setSelectedInspectorEndFromSlider();
        };
        addAndMakeVisible (inspectorEnd);

        configureInspectorTimeSlider (inspectorLength, kInspectorLengthComponentId, "Clip length");
        inspectorLength.onValueChange = [this] {
            if (refreshingInspectorControls || ! inspectorLength.isEnabled())
                return;

            setSelectedInspectorLengthFromSlider();
        };
        addAndMakeVisible (inspectorLength);

        configureActionComponent (inspectorGain, yesdaw::ui::UiActionId::TimelineClipSetGain, "Clip gain");
        inspectorGain.setSliderStyle (juce::Slider::LinearHorizontal);
        inspectorGain.setTextBoxStyle (juce::Slider::NoTextBox,
                                       false,
                                       yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                       yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        inspectorGain.setRange (yesdaw::ui::UiTheme::Layout::inspectorGainSliderMin,
                                yesdaw::ui::UiTheme::Layout::inspectorGainSliderMax,
                                yesdaw::ui::UiTheme::Layout::inspectorGainSliderInterval);
        inspectorGain.setValue (yesdaw::ui::UiTheme::Layout::inspectorGainSliderDefault,
                                juce::dontSendNotification);
        inspectorGain.onValueChange = [this] {
            if (refreshingInspectorControls || ! inspectorGain.isEnabled())
                return;

            (void) appModel.setSelectedTimelineClipGain (static_cast<float> (inspectorGain.getValue()));
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (inspectorGain);

        configureInspectorFadeSlider (inspectorFadeIn, kInspectorFadeInComponentId, "Clip fade in");
        inspectorFadeIn.onValueChange = [this] {
            if (refreshingInspectorControls || ! inspectorFadeIn.isEnabled())
                return;

            setSelectedInspectorFadesFromSliders();
        };
        addAndMakeVisible (inspectorFadeIn);

        configureInspectorFadeSlider (inspectorFadeOut, kInspectorFadeOutComponentId, "Clip fade out");
        inspectorFadeOut.onValueChange = [this] {
            if (refreshingInspectorControls || ! inspectorFadeOut.isEnabled())
                return;

            setSelectedInspectorFadesFromSliders();
        };
        addAndMakeVisible (inspectorFadeOut);

        inspectorFadeCurve.setComponentID (kInspectorFadeCurveComponentId);
        inspectorFadeCurve.setTooltip ("Clip fade curve shape");
        inspectorFadeCurve.setName ("Clip fade curve");
        inspectorFadeCurve.setTitle ("Clip fade curve");
        inspectorFadeCurve.setTooltip ("H14 canonical fade law");
        inspectorFadeCurve.addItem ("Equal power", kInspectorEqualPowerFadeCurveId);
        inspectorFadeCurve.setSelectedId (kInspectorEqualPowerFadeCurveId, juce::dontSendNotification);
        inspectorFadeCurve.onChange = [this] {
            if (refreshingInspectorControls)
                return;

            inspectorFadeCurve.setSelectedId (kInspectorEqualPowerFadeCurveId, juce::dontSendNotification);
            repaint();
        };
        addAndMakeVisible (inspectorFadeCurve);
    }

    void configureInspectorTimeSlider (juce::Slider& slider, const char* componentId, const juce::String& name)
    {
        slider.setComponentID (componentId);
        slider.setName (name);
        slider.setTitle (name);
        slider.setTooltip (componentId);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox,
                                false,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        slider.setRange (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                         yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback,
                         yesdaw::ui::UiTheme::Layout::inspectorTimeSliderIntervalSeconds);
        slider.setValue (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderDefaultSeconds,
                         juce::dontSendNotification);
        slider.setColour (juce::Slider::backgroundColourId, yesdaw::ui::UiTheme::Color::transparent());
        slider.setColour (juce::Slider::trackColourId, yesdaw::ui::UiTheme::Color::transparent());
        slider.setColour (juce::Slider::thumbColourId, yesdaw::ui::UiTheme::Color::transparent());
    }

    void configureInspectorFadeSlider (juce::Slider& slider, const char* componentId, const juce::String& name)
    {
        slider.setComponentID (componentId);
        slider.setName (name);
        slider.setTitle (name);
        slider.setTooltip (componentId);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox,
                                false,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        slider.setRange (yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMinSeconds,
                         yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMaxSeconds,
                         yesdaw::ui::UiTheme::Layout::inspectorFadeSliderIntervalSeconds);
        slider.setValue (yesdaw::ui::UiTheme::Layout::inspectorFadeSliderDefaultSeconds,
                         juce::dontSendNotification);
    }

    void configureMixerControls()
    {
        mixerTrackSelect.setButtonText ("Audio 1");
        mixerTrackSelect.setComponentID ("mixer.track.0.select");
        mixerTrackSelect.setName ("Select first mixer track");
        mixerTrackSelect.setTooltip ("Select the first mixer track strip");
        mixerTrackSelect.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerTrackSelect.setColour (juce::TextButton::textColourOffId, kText);
        mixerTrackSelect.onClick = [this] {
            (void) appModel.selectMixerTrack (0);
            layoutMixerControls();
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerTrackSelect);

        // Every mixer strip is selectable (usable-DAW P0): clicking a Track strip retargets the shared
        // fader/pan/mute/solo controls and moves them onto that strip.
        mixerStripsInput.setComponentID ("shell.mixer.strips.input");
        mixerStripsInput.setName ("Mixer Strips");
        mixerStripsInput.setTitle ("Mixer Strips");
        mixerStripsInput.setTooltip ("Mixer strips: click a strip to retarget the shared controls, click a meter to clear its clip light");
        mixerStripsInput.onStripClicked = [this] (int stripIndex) {
            const int trackCount = static_cast<int> (currentMixerSurface().tracks.size());
            if (stripIndex < 0 || stripIndex >= trackCount)
                return;

            (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
            selectedTrackLane = stripIndex;   // rail selection follows the mixer strip
            layoutMixerControls();
            refreshActionState();
            repaint();
        };
        mixerStripsInput.meterStripAtPosition = [this] (juce::Point<int> positionInShell) {
            const std::size_t trackCount = appModel.context().projectLoaded
                                               ? appModel.project().tracks.size()
                                               : 0u;
            for (std::size_t i = 0; i < trackCount; ++i)
                if (paintedMeterBoundsForLane (paintedMixerLaneBounds (i)).contains (positionInShell))
                    return static_cast<int> (i);
            return -1;
        };
        mixerStripsInput.onMeterClicked = [this] (int stripIndex) { clearTrackMeterHold (stripIndex); };
        mixerStripsInput.stripAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            const int stripCount = juce::jmax (1, static_cast<int> (surface.tracks.size() + surface.buses.size()));
            auto mixer = mixerPanelBounds();
            mixer.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerToolsWidth);
            if (! mixer.contains (positionInShell))
                return -1;

            const int stripWidth = juce::jmax (yesdaw::ui::UiTheme::Layout::mixerStripMinWidth,
                                               mixer.getWidth() / (stripCount + 1));
            return (positionInShell.x - mixer.getX()) / stripWidth;
        };
        addAndMakeVisible (mixerStripsInput);
        mixerStripsInput.toBack();   // the shared strip controls stay on top and keep their own clicks

        // FX insert chain on the selected strip (usable-DAW P0): the chooser adds one of the five
        // built-in FX; each visible slot row toggles bypass or removes the insert — all undoable.
        configureActionComponent (mixerFxAddChooser, yesdaw::ui::UiActionId::MixerFxInsertAdd, "Add FX insert");
        mixerFxAddChooser.setTextWhenNothingSelected ("+ FX");
        mixerFxAddChooser.addItem ("EQ", static_cast<int> (yesdaw::engine::FxKind::Eq) + 1);
        mixerFxAddChooser.addItem ("Compressor", static_cast<int> (yesdaw::engine::FxKind::Compressor) + 1);
        mixerFxAddChooser.addItem ("Delay", static_cast<int> (yesdaw::engine::FxKind::Delay) + 1);
        mixerFxAddChooser.addItem ("Reverb", static_cast<int> (yesdaw::engine::FxKind::Reverb) + 1);
        mixerFxAddChooser.addItem ("Limiter", static_cast<int> (yesdaw::engine::FxKind::Limiter) + 1);
        mixerFxAddChooser.onChange = [this] {
            const int selected = mixerFxAddChooser.getSelectedId();
            if (selected <= 0)
                return;

            mixerFxAddChooser.setSelectedId (0, juce::dontSendNotification);
            (void) appModel.addFxInsertToSelectedStrip (static_cast<yesdaw::engine::FxKind> (selected - 1));
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerFxAddChooser);

        for (std::size_t slot = 0; slot < mixerFxSlotToggles.size(); ++slot)
        {
            auto& toggle = mixerFxSlotToggles[slot];
            toggle.setComponentID ("mixer.fx.slot." + juce::String (static_cast<int> (slot)) + ".toggle");
            toggle.setTooltip ("Bypass FX slot " + juce::String (static_cast<int> (slot) + 1));
            toggle.setName ("Toggle FX slot " + juce::String (static_cast<int> (slot + 1)) + " bypass");
            toggle.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
            toggle.setColour (juce::TextButton::textColourOffId, kText);
            toggle.onClick = [this, slot] {
                (void) appModel.toggleFxInsertEnabledOnSelectedStrip (slot);
                refreshActionState();
                repaint();
            };
            addChildComponent (toggle);

            auto& remove = mixerFxSlotRemoves[slot];
            remove.setButtonText ("x");
            remove.setComponentID ("mixer.fx.slot." + juce::String (static_cast<int> (slot)) + ".remove");
            remove.setTooltip ("Remove FX slot " + juce::String (static_cast<int> (slot) + 1));
            remove.setName ("Remove FX slot " + juce::String (static_cast<int> (slot + 1)));
            remove.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
            remove.setColour (juce::TextButton::textColourOffId, kText);
            remove.onClick = [this, slot] {
                (void) appModel.removeFxInsertFromSelectedStrip (slot);
                refreshActionState();
                repaint();
            };
            addChildComponent (remove);

            auto& edit = mixerFxSlotEdits[slot];
            edit.setButtonText ("e");
            edit.setComponentID ("mixer.fx.slot." + juce::String (static_cast<int> (slot)) + ".edit");
            edit.setTooltip ("Edit FX slot " + juce::String (static_cast<int> (slot) + 1) + " parameters");
            edit.setName ("Edit FX slot " + juce::String (static_cast<int> (slot + 1)) + " parameters");
            edit.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
            edit.setColour (juce::TextButton::textColourOffId, kText);
            edit.onClick = [this, slot] {
                selectedFxParamSlot = selectedFxParamSlot == static_cast<int> (slot) ? -1
                                                                                     : static_cast<int> (slot);
                refreshActionState();
                resized();
                repaint();
            };
            addChildComponent (edit);
        }

        // Send routing (ADR-0044): + Bus creates a persisted Bus; the send chooser routes the
        // selected track to a bus; each visible send row edits its level and removes undoably.
        configureActionComponent (mixerBusAddButton, yesdaw::ui::UiActionId::MixerBusAdd, "Add bus");
        mixerBusAddButton.setButtonText ("+ Bus");
        mixerBusAddButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        mixerBusAddButton.setColour (juce::TextButton::textColourOffId, kText);
        mixerBusAddButton.onClick = [this] {
            (void) appModel.addBusToMixer();
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerBusAddButton);

        configureActionComponent (mixerSendAddChooser, yesdaw::ui::UiActionId::MixerSendAdd, "Add send");
        mixerSendAddChooser.setTextWhenNothingSelected ("+ Send");
        mixerSendAddChooser.setTextWhenNoChoicesAvailable ("No Buses");
        mixerSendAddChooser.onChange = [this] {
            if (refreshingSendControls)
                return;

            const int selected = mixerSendAddChooser.getSelectedId();
            if (selected <= 0)
                return;

            mixerSendAddChooser.setSelectedId (0, juce::dontSendNotification);
            (void) appModel.addSendOnSelectedTrack (static_cast<std::size_t> (selected - 1));
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerSendAddChooser);

        for (std::size_t row = 0; row < mixerSendLevelSliders.size(); ++row)
        {
            auto& label = mixerSendLabels[row];
            label.setComponentID ("mixer.send." + juce::String (static_cast<int> (row)) + ".label");
            label.setTooltip ("Send " + juce::String (static_cast<int> (row) + 1) + " route");
            label.setColour (juce::Label::textColourId, kText);
            label.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
            label.setInterceptsMouseClicks (false, false);
            addChildComponent (label);

            auto& slider = mixerSendLevelSliders[row];
            configureActionComponent (slider, yesdaw::ui::UiActionId::MixerSendSetLevel, "Send level");
            slider.setComponentID ("mixer.send." + juce::String (static_cast<int> (row)));
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox,
                                    false,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
            slider.setRange (0.0, 1.0, 0.0);
            slider.setDoubleClickReturnValue (true, 1.0);   // Alt+click resets the send to unity
            slider.onValueChange = [this, row] {
                if (refreshingSendControls)
                    return;

                (void) appModel.setSendLevelOnSelectedTrack (
                    row, static_cast<float> (mixerSendLevelSliders[row].getValue()));
                refreshActionState();
                repaint();
            };
            addChildComponent (slider);

            auto& remove = mixerSendRemoves[row];
            remove.setButtonText ("x");
            remove.setComponentID ("mixer.send." + juce::String (static_cast<int> (row)) + ".remove");
            remove.setTooltip ("Remove send " + juce::String (static_cast<int> (row) + 1));
            remove.setName ("Remove send " + juce::String (static_cast<int> (row + 1)));
            remove.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
            remove.setColour (juce::TextButton::textColourOffId, kText);
            remove.onClick = [this, row] {
                (void) appModel.removeSendOnSelectedTrack (row);
                refreshActionState();
                repaint();
            };
            addChildComponent (remove);
        }

        // FX parameter editing (usable-DAW P1): the selected slot's ParamSpecs become live sliders;
        // every committed value is one undoable SetFxInsertParam through the model.
        for (std::size_t index = 0; index < mixerFxParamSliders.size(); ++index)
        {
            auto& label = mixerFxParamLabels[index];
            label.setComponentID ("mixer.fx.param." + juce::String (static_cast<int> (index)) + ".label");
            label.setTooltip ("FX parameter " + juce::String (static_cast<int> (index) + 1) + " readout");
            label.setColour (juce::Label::textColourId, kText);
            label.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
            label.setInterceptsMouseClicks (false, false);
            addChildComponent (label);

            auto& slider = mixerFxParamSliders[index];
            configureActionComponent (slider, yesdaw::ui::UiActionId::MixerFxInsertParamSet, "FX parameter");
            slider.setComponentID ("mixer.fx.param." + juce::String (static_cast<int> (index)));
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox,
                                    false,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
            slider.setRange (0.0, 1.0, 0.0);
            slider.onValueChange = [this, index] {
                if (refreshingFxParamControls || selectedFxParamSlot < 0)
                    return;

                (void) appModel.setFxInsertParamOnSelectedStrip (
                    static_cast<std::size_t> (selectedFxParamSlot),
                    mixerFxParamSliderIds[index],
                    mixerFxParamSliders[index].getValue());
                refreshActionState();
                repaint();
            };
            addChildComponent (slider);
        }

        configureActionComponent (mixerFader, yesdaw::ui::UiActionId::MixerTargetSetFader, "Mixer fader");
        mixerFader.setSliderStyle (juce::Slider::LinearVertical);
        mixerFader.setTextBoxStyle (juce::Slider::NoTextBox,
                                    false,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        mixerFader.setRange (yesdaw::ui::UiTheme::Layout::mixerFaderSliderMin,
                             yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax,
                             yesdaw::ui::UiTheme::Layout::mixerFaderSliderInterval);
        mixerFader.setValue (yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault,
                             juce::dontSendNotification);
        // Alt+click (or double-click) resets the fader to unity through the same persisted edit.
        mixerFader.setDoubleClickReturnValue (true, yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault);
        mixerFader.onValueChange = [this] {
            if (refreshingMixerControls || ! mixerFader.isEnabled())
                return;

            (void) appModel.setSelectedMixerFader (static_cast<float> (mixerFader.getValue()));
            if (dragDbReadout.isVisible())
                dragDbReadout.setText (dbReadoutText (mixerFader.getValue()), juce::dontSendNotification);
            refreshActionState();
            repaint();
        };
        // Live dB readout while the fader is dragged (B31); the rail VOL shares the same label.
        mixerFader.onDragStart = [this] {
            showDragDbReadout (mixerFader.getBounds(), mixerFader.getValue());
        };
        mixerFader.onDragEnd = [this] { hideDragDbReadout(); };
        addAndMakeVisible (mixerFader);

        dragDbReadout.setComponentID ("shell.drag.db");
        dragDbReadout.setTooltip ("Live gain in dB while dragging");
        dragDbReadout.setInterceptsMouseClicks (false, false);
        dragDbReadout.setJustificationType (juce::Justification::centred);
        dragDbReadout.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
        dragDbReadout.setColour (juce::Label::textColourId, kText);
        dragDbReadout.setColour (juce::Label::backgroundColourId,
                                 yesdaw::ui::UiTheme::Color::darkControl());
        addChildComponent (dragDbReadout);

        configureActionComponent (mixerPan, yesdaw::ui::UiActionId::MixerTargetSetPan, "Mixer pan");
        mixerPan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mixerPan.setTextBoxStyle (juce::Slider::NoTextBox,
                                  false,
                                  yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                  yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        mixerPan.setRange (yesdaw::ui::UiTheme::Layout::mixerPanSliderMin,
                           yesdaw::ui::UiTheme::Layout::mixerPanSliderMax,
                           yesdaw::ui::UiTheme::Layout::mixerPanSliderInterval);
        mixerPan.setValue (yesdaw::ui::UiTheme::Layout::mixerPanSliderDefault,
                           juce::dontSendNotification);
        // Alt+click (or double-click) recentres the pan through the same persisted edit.
        mixerPan.setDoubleClickReturnValue (true, yesdaw::ui::UiTheme::Layout::mixerPanSliderDefault);
        mixerPan.onValueChange = [this] {
            if (refreshingMixerControls || ! mixerPan.isEnabled())
                return;

            // JUCE snaps values as `rangeStart + interval * n`; with the pan range starting at
            // -1.0, ARM FMA contraction leaves ~2e-17 dust where x64 lands exactly on 0.0. Snap
            // to the same interval grid with cancellation-free arithmetic so dead center
            // persists as exactly 0 on every platform.
            const double snapped = std::round (mixerPan.getValue()
                                               / yesdaw::ui::UiTheme::Layout::mixerPanSliderInterval)
                                 * yesdaw::ui::UiTheme::Layout::mixerPanSliderInterval;
            (void) appModel.setSelectedMixerPan (static_cast<float> (snapped));
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerPan);

        configureActionComponent (mixerMetersReadout, yesdaw::ui::UiActionId::MixerReadMeters, "Mixer meters");
        mixerMetersReadout.setButtonText ("Meters");
        mixerMetersReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerMetersReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerMetersReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadMeters);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerMetersReadout);

        configureActionComponent (mixerSendsReadout, yesdaw::ui::UiActionId::MixerReadSends, "Mixer sends");
        mixerSendsReadout.setButtonText ("Sends");
        mixerSendsReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerSendsReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerSendsReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadSends);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerSendsReadout);

        configureActionComponent (mixerSendLevelEdit, yesdaw::ui::UiActionId::MixerSetFirstSendLevel, "Mixer send level");
        mixerSendLevelEdit.setButtonText ("Set send");
        mixerSendLevelEdit.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerSendLevelEdit.setColour (juce::TextButton::textColourOffId, kText);
        mixerSendLevelEdit.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerSetFirstSendLevel);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerSendLevelEdit);

        configureActionComponent (mixerFxSlotsReadout, yesdaw::ui::UiActionId::MixerReadFxSlots, "Mixer FX slots");
        mixerFxSlotsReadout.setButtonText ("Track FX");
        mixerFxSlotsReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerFxSlotsReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerFxSlotsReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadFxSlots);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerFxSlotsReadout);

        configureActionComponent (mixerGainReductionReadout, yesdaw::ui::UiActionId::MixerReadGainReduction, "Mixer gain reduction");
        mixerGainReductionReadout.setButtonText ("Gain reduction");
        mixerGainReductionReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerGainReductionReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerGainReductionReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadGainReduction);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerGainReductionReadout);

        configureActionComponent (mixerBusFxSlotsReadout, yesdaw::ui::UiActionId::MixerReadBusFxSlots, "Mixer Bus FX slots");
        mixerBusFxSlotsReadout.setButtonText ("Bus FX");
        mixerBusFxSlotsReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerBusFxSlotsReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerBusFxSlotsReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadBusFxSlots);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerBusFxSlotsReadout);

        configureActionComponent (mixerFxSlotToggle, yesdaw::ui::UiActionId::MixerToggleFirstFxSlotEnabled, "Mixer FX slot toggle");
        mixerFxSlotToggle.setButtonText ("Bypass FX");
        mixerFxSlotToggle.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerFxSlotToggle.setColour (juce::TextButton::textColourOffId, kText);
        mixerFxSlotToggle.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerToggleFirstFxSlotEnabled);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerFxSlotToggle);

        configureActionComponent (mixerMute, yesdaw::ui::UiActionId::MixerTargetToggleMute, "Mixer mute");
        mixerMute.setButtonText ("M");
        mixerMute.setColour (juce::TextButton::buttonColourId,
                             yesdaw::ui::UiTheme::Color::buttonSurface());
        mixerMute.setColour (juce::TextButton::buttonOnColourId,
                             yesdaw::ui::UiTheme::Color::accentPurpleDeep());
        mixerMute.setColour (juce::TextButton::textColourOffId, kText);
        mixerMute.setColour (juce::TextButton::textColourOnId, kText);
        mixerMute.onClick = [this] {
            if (refreshingMixerControls || ! mixerMute.isEnabled())
                return;

            (void) appModel.toggleSelectedMixerMute();
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerMute);

        configureActionComponent (mixerSolo, yesdaw::ui::UiActionId::MixerTargetToggleSolo, "Mixer solo");
        mixerSolo.setButtonText ("S");
        mixerSolo.setColour (juce::TextButton::buttonColourId,
                             yesdaw::ui::UiTheme::Color::buttonSurface());
        mixerSolo.setColour (juce::TextButton::buttonOnColourId,
                             yesdaw::ui::UiTheme::Color::accentPurpleDeep());
        mixerSolo.setColour (juce::TextButton::textColourOffId, kText);
        mixerSolo.setColour (juce::TextButton::textColourOnId, kText);
        mixerSolo.onClick = [this] {
            if (refreshingMixerControls || ! mixerSolo.isEnabled())
                return;

            (void) appModel.toggleSelectedMixerSolo();
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerSolo);
    }

    [[nodiscard]] juce::Rectangle<int> timelineBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (kHeaderHeight);
        work.removeFromBottom (kMixerHeight);
        work.removeFromLeft (kLeftRailWidth);
        work.removeFromRight (kInspectorWidth);
        return work.reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                             yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    }

    // The exact rect drawTrackList paints into; the rail input overlay shares it so hits match paint.
    [[nodiscard]] juce::Rectangle<int> leftRailPanelBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (kHeaderHeight);
        work.removeFromBottom (kMixerHeight);
        return work.removeFromLeft (kLeftRailWidth)
                   .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                             yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    }

    void selectTrackLane (int lane)
    {
        const int trackCount = static_cast<int> (appModel.project().tracks.size());
        if (! appModel.context().projectLoaded || lane < 0 || lane >= trackCount)
            return;

        dismissTrackRenameEditor();
        selectedTrackLane = lane;
        (void) appModel.selectMixerTrack (static_cast<std::size_t> (lane), /*showMixerPanel*/ false);
        refreshActionState();
        repaint();
    }

    void selectAdjacentTrackLane (yesdaw::ui::UiActionId action)
    {
        const int trackCount = static_cast<int> (appModel.project().tracks.size());
        if (trackCount <= 0 || ! appModel.dispatch (action).dispatched)
            return;

        const int delta = action == yesdaw::ui::UiActionId::TrackSelectPrevious ? -1 : 1;
        const int initialLane = delta < 0 ? trackCount - 1 : 0;
        const int nextLane = selectedTrackLane < 0 || selectedTrackLane >= trackCount
            ? initialLane
            : std::clamp (selectedTrackLane + delta, 0, trackCount - 1);
        selectTrackLane (nextLane);
    }

    void openTrackRenameEditor()
    {
        dismissClipRenameEditor();
        const auto& tracks = appModel.project().tracks;
        if (selectedTrackLane < 0 || selectedTrackLane >= static_cast<int> (tracks.size()))
            return;

        const juce::Rectangle<int> row = trackListInput.rowBounds (selectedTrackLane);
        if (row.isEmpty())
            return;

        trackRenameEditor.setBounds (row.translated (trackListInput.getX(), trackListInput.getY())
                                        .reduced (yesdaw::ui::UiTheme::Layout::trackListRowHorizontalInset,
                                                  yesdaw::ui::UiTheme::Layout::trackListRowVerticalInset)
                                        .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListIconLeftInset)
                                        .withHeight (yesdaw::ui::UiTheme::Layout::trackListRenameEditorHeight));
        trackRenameEditor.setText (juce::String (tracks[static_cast<std::size_t> (selectedTrackLane)].strip.name),
                                   juce::dontSendNotification);
        trackRenameEditor.setVisible (true);
        trackRenameEditor.grabKeyboardFocus();
    }

    void commitTrackRenameEditor()
    {
        const auto& tracks = appModel.project().tracks;
        if (selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size()))
        {
            const std::string newName = trackRenameEditor.getText().toStdString();
            (void) appModel.renameProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id, newName);
        }

        dismissTrackRenameEditor();
        refreshActionState();
        repaint();
    }

    void dismissTrackRenameEditor()
    {
        trackRenameEditor.setVisible (false);
    }

    void openClipRenameEditor()
    {
        const yesdaw::engine::EntityId selectedId = appModel.selectedTimelineClipId();
        const yesdaw::engine::Clip* const selectedClip = findProjectClipById (selectedId);
        const auto view = std::find (timelineClipIds.begin(), timelineClipIds.end(), selectedId);
        if (selectedClip == nullptr || view == timelineClipIds.end())
            return;

        dismissTrackRenameEditor();
        const std::size_t viewIndex = static_cast<std::size_t> (std::distance (timelineClipIds.begin(), view));
        const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
        const yesdaw::ui::Clip& clip = timelineClips[viewIndex];
        const int left = geometry.clipArea.getX()
                       + juce::roundToInt ((clip.startSeconds - geometry.viewport.scrollSeconds)
                                           * geometry.viewport.pixelsPerSecond);
        const int top = geometry.clipArea.getY() + clip.lane * geometry.laneHeight
                      - juce::roundToInt (geometry.viewport.laneScrollPixels);
        const int width = juce::roundToInt (clip.lengthSeconds * geometry.viewport.pixelsPerSecond);
        juce::Rectangle<int> bounds { left, top, width, geometry.laneHeight };
        bounds = bounds.getIntersection (geometry.clipArea)
                       .reduced (yesdaw::ui::UiTheme::Space::sm)
                       .withHeight (yesdaw::ui::UiTheme::Layout::trackListRenameEditorHeight)
                       .translated (timelineInput.getX(), timelineInput.getY());
        if (bounds.isEmpty())
            return;

        clipRenameEditor.setBounds (bounds);
        clipRenameEditor.setText (juce::String (selectedClip->name.c_str()), juce::dontSendNotification);
        clipRenameEditor.setVisible (true);
        clipRenameEditor.grabKeyboardFocus();
    }

    void commitClipRenameEditor()
    {
        (void) appModel.renameSelectedTimelineClip (clipRenameEditor.getText().toStdString());
        dismissClipRenameEditor();
        refreshActionState();
        repaint();
    }

    void dismissClipRenameEditor()
    {
        clipRenameEditor.setVisible (false);
    }

    void removeSelectedTrack()
    {
        const auto& tracks = appModel.project().tracks;
        if (selectedTrackLane < 0 || selectedTrackLane >= static_cast<int> (tracks.size()))
            return;

        dismissTrackRenameEditor();
        if (appModel.removeProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id).dispatched)
            selectedTrackLane = std::min (selectedTrackLane,
                                          static_cast<int> (appModel.project().tracks.size()) - 1);

        refreshActionState();
        repaint();
    }

    // Selected-track strip/arm toggles (B28): the rail row is the target; the mixer never opens.
    void toggleSelectedTrackKey (yesdaw::ui::UiActionId action)
    {
        const auto& tracks = appModel.project().tracks;
        if (selectedTrackLane < 0 || selectedTrackLane >= static_cast<int> (tracks.size()))
            return;

        const std::size_t lane = static_cast<std::size_t> (selectedTrackLane);
        switch (action)
        {
            case yesdaw::ui::UiActionId::TrackToggleMute:
                (void) appModel.toggleTrackMute (tracks[lane].id);
                break;
            case yesdaw::ui::UiActionId::TrackToggleSolo:
                (void) appModel.toggleTrackSolo (tracks[lane].id);
                break;
            case yesdaw::ui::UiActionId::TrackToggleArm:
                (void) appModel.toggleRecordingArmForTrack (lane);
                break;
            default:
                return;
        }

        refreshActionState();
        repaint();
    }

    void moveSelectedTrack (int delta)
    {
        const auto& tracks = appModel.project().tracks;
        const int trackCount = static_cast<int> (tracks.size());
        if (selectedTrackLane < 0 || selectedTrackLane >= trackCount)
            return;

        const int targetLane = selectedTrackLane + delta;
        if (targetLane < 0 || targetLane >= trackCount)
            return;   // honest boundary no-op: the first row cannot move up, the last cannot move down

        dismissTrackRenameEditor();
        if (appModel.reorderProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id,
                                          static_cast<std::size_t> (targetLane)).dispatched)
            selectTrackLane (targetLane);   // the rail highlight follows the moved row

        refreshActionState();
        repaint();
    }

    // Per-track meter peak-hold and clip-latch state (B32), advanced once per UI refresh tick so
    // gates can drive it deterministically through serviceMainComponentUiTimer.
    struct MeterHoldState
    {
        float livePeak = 0.0f;
        float heldPeak = 0.0f;
        int holdTicksRemaining = 0;
        bool clipLatched = false;
    };

    static void advanceMeterHold (MeterHoldState& state, float livePeak)
    {
        state.livePeak = livePeak;
        if (livePeak >= state.heldPeak || state.holdTicksRemaining <= 0)
        {
            state.heldPeak = livePeak;
            state.holdTicksRemaining = yesdaw::ui::UiTheme::Meter::peakHoldTicks;
        }
        else
        {
            --state.holdTicksRemaining;
        }

        if (livePeak >= yesdaw::ui::UiTheme::Meter::clipThreshold)
            state.clipLatched = true;
    }

    void updateTrackMeterHoldStates()
    {
        // A stopped transport reads live silence: the MeterNode atomics keep the last processed
        // Block's peak, but a meter must fall when playback stops (the held peak still decays on
        // its own ~2 s law and the clip latch stays until clicked).
        const bool playing = appModel.context().isPlaying;
        const auto& tracks = appModel.project().tracks;
        trackMeterHold.resize (tracks.size());
        for (std::size_t i = 0; i < tracks.size(); ++i)
            advanceMeterHold (trackMeterHold[i],
                              playing ? appModel.trackMeterPeak (tracks[i].id) : 0.0f);
    }

    void clearTrackMeterHold (int trackIndex)
    {
        if (trackIndex < 0 || trackIndex >= static_cast<int> (trackMeterHold.size()))
            return;

        MeterHoldState& state = trackMeterHold[static_cast<std::size_t> (trackIndex)];
        state.clipLatched = false;
        state.heldPeak = state.livePeak;
        state.holdTicksRemaining = 0;
        repaint();
    }

    // Live gain readout in dB (B31): 20*log10(linear gain), "-inf dB" at silence.
    [[nodiscard]] static juce::String dbReadoutText (double linearGain)
    {
        if (linearGain <= 0.0)
            return "-inf dB";

        return juce::String (20.0 * std::log10 (linearGain), 1) + " dB";
    }

    void showDragDbReadout (juce::Rectangle<int> anchorBounds, double linearGain)
    {
        dragDbReadout.setText (dbReadoutText (linearGain), juce::dontSendNotification);
        dragDbReadout.setBounds (
            juce::Rectangle<int> (yesdaw::ui::UiTheme::Layout::dbReadoutWidth,
                                  yesdaw::ui::UiTheme::Layout::dbReadoutHeight)
                .withCentre ({ anchorBounds.getCentreX(),
                               anchorBounds.getY()
                                   - yesdaw::ui::UiTheme::Layout::dbReadoutHeight / 2 })
                .constrainedWithin (getLocalBounds()));
        dragDbReadout.setVisible (true);
        dragDbReadout.toFront (false);
    }

    void hideDragDbReadout()
    {
        dragDbReadout.setVisible (false);
    }

    void duplicateSelectedTrack()
    {
        const auto& tracks = appModel.project().tracks;
        if (selectedTrackLane < 0 || selectedTrackLane >= static_cast<int> (tracks.size()))
            return;

        dismissTrackRenameEditor();
        if (appModel.duplicateProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id).dispatched)
            selectTrackLane (selectedTrackLane + 1);   // the copy lands directly below the source

        refreshActionState();
        repaint();
    }

    // Open a project bundle at a known path (B39): shared by File > Open and Open Recent.
    void openProjectBundleAtPath (const std::filesystem::path& path)
    {
        if (auto decodedAssets = decodeStoredProjectAssets (path); decodedAssets && ! decodedAssets->empty())
            (void) appModel.loadProjectBundle (
                path,
                std::span<const yesdaw::ui::UiDecodedAsset> (
                    decodedAssets->data(), decodedAssets->size()));
        else if (decodedAssets)
            (void) appModel.openProjectBundle (path);
    }

    [[nodiscard]] juce::Rectangle<int> mixerPanelBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (kHeaderHeight);
        auto mixer = appModel.context().activePanel == yesdaw::ui::UiPanel::Mixer
                         ? work
                         : work.removeFromBottom (kMixerHeight);
        return mixer.reduced (yesdaw::ui::UiTheme::Layout::mixerPanelHorizontalInset,
                              yesdaw::ui::UiTheme::Layout::mixerPanelVerticalInset);
    }

    [[nodiscard]] juce::Rectangle<int> mixerStripBounds (int stripIndex) const
    {
        auto mixer = mixerPanelBounds();
        mixer.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerToolsWidth);

        const auto surface = currentMixerSurface();
        const int stripCount = juce::jmax (1, static_cast<int> (surface.tracks.size() + surface.buses.size()));
        const int stripWidth = juce::jmax (yesdaw::ui::UiTheme::Layout::mixerStripMinWidth,
                                           mixer.getWidth() / (stripCount + 1));
        return mixer.withWidth (stripWidth)
            .translated (stripWidth * juce::jmax (0, stripIndex), 0)
            .reduced (yesdaw::ui::UiTheme::Layout::mixerStripHorizontalInset,
                      yesdaw::ui::UiTheme::Layout::mixerStripVerticalInset);
    }

    [[nodiscard]] juce::Rectangle<int> mixerFirstStripBounds() const { return mixerStripBounds (0); }

    // Shared painted-strip geometry law (B32): hit-testing must mirror drawMixer's lane math
    // exactly so a meter click can never drift from the painted meter.
    [[nodiscard]] juce::Rectangle<int> paintedMixerLaneBounds (std::size_t stripIndex) const
    {
        auto area = mixerPanelBounds();
        area.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerToolsWidth);

        const auto surface = currentMixerSurface();
        const std::size_t stripCount = surface.tracks.size() + surface.buses.size();
        const int stripWidth = std::clamp (
            area.getWidth() / (juce::jmax (yesdaw::ui::UiTheme::Layout::mixerPaintedStripMinCount,
                                           static_cast<int> (stripCount))
                               + yesdaw::ui::UiTheme::Layout::mixerPaintedStripExtraSlotCount),
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripMinWidth,
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripMaxWidth);
        return juce::Rectangle<int> (area.getX() + static_cast<int> (stripIndex) * stripWidth,
                                     area.getY(),
                                     stripWidth,
                                     area.getHeight())
                   .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetX,
                             yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetY);
    }

    [[nodiscard]] static juce::Rectangle<int> paintedMeterBoundsForLane (juce::Rectangle<int> lane)
    {
        auto faderArea = lane.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderTop)
                             .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderBottomInset);
        return faderArea.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterWidth)
                        .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetX,
                                  yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetY);
    }

    [[nodiscard]] juce::Rectangle<int> inspectorBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (kHeaderHeight);
        work.removeFromBottom (kMixerHeight);
        return work.removeFromRight (kInspectorWidth)
            .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                      yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    }

    void layoutInspectorControls()
    {
        auto area = inspectorBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorTabHeight);
        area.reduce (yesdaw::ui::UiTheme::Layout::inspectorContentInsetX,
                     yesdaw::ui::UiTheme::Layout::inspectorContentInsetY);
        auto stats = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                         .withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionHeight);
        auto startCell = stats.removeFromLeft (stats.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorStatsColumnCount)
                              .reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                        yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        auto endCell = stats.removeFromLeft (stats.getWidth() / (yesdaw::ui::UiTheme::Layout::inspectorStatsColumnCount - 1))
                            .reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                      yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        auto lengthCell = stats.reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                         yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        inspectorStart.setBounds (startCell);
        inspectorEnd.setBounds (endCell);
        inspectorLength.setBounds (lengthCell);

        auto gain = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorGainSectionTop)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainSectionHeight);
        gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlTopInset);
        inspectorGain.setBounds (
            gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlHeight)
                .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainControlLeftInset));

        auto fades = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionTop)
                         .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionHeight);
        fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadesControlTopInset);
        inspectorFadeIn.setBounds (
            fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHeight)
                .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset)
                .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHorizontalInset,
                          yesdaw::ui::UiTheme::Layout::inspectorFadeControlVerticalInset));
        inspectorFadeOut.setBounds (
            fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHeight)
                .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset)
                .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHorizontalInset,
                          yesdaw::ui::UiTheme::Layout::inspectorFadeControlVerticalInset));
        fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveControlTopGap);
        inspectorFadeCurve.setBounds (
            fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveControlHeight)
                .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset));
    }

    void layoutMixerControls()
    {
        auto utility = mixerPanelBounds().withWidth (yesdaw::ui::UiTheme::Layout::mixerToolsWidth)
                           .reduced (yesdaw::ui::UiTheme::Layout::mixerUtilityInsetX,
                                     yesdaw::ui::UiTheme::Space::none);
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerUtilityTop);
        const std::array<juce::Button*, 7> utilityButtons {
            &mixerMetersReadout,
            &mixerSendsReadout,
            &mixerSendLevelEdit,
            &mixerFxSlotsReadout,
            &mixerFxSlotToggle,
            &mixerGainReductionReadout,
            &mixerBusFxSlotsReadout
        };
        for (juce::Button* button : utilityButtons)
        {
            button->setBounds (utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerUtilityHeight));
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerUtilityGap);
        }

        mixerFxAddChooser.setBounds (utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight));
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        for (std::size_t slot = 0; slot < mixerFxSlotToggles.size(); ++slot)
        {
            if (! mixerFxSlotToggles[slot].isVisible())
            {
                mixerFxSlotToggles[slot].setBounds ({});
                mixerFxSlotEdits[slot].setBounds ({});
                mixerFxSlotRemoves[slot].setBounds ({});
                continue;
            }

            auto slotRow = utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotHeight);
            mixerFxSlotRemoves[slot].setBounds (
                slotRow.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerFxSlotRemoveWidth));
            mixerFxSlotEdits[slot].setBounds (
                slotRow.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerFxSlotRemoveWidth));
            mixerFxSlotToggles[slot].setBounds (slotRow);
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        }

        // Hidden rows take no column space — the tools column would otherwise overflow. The refresh
        // path calls resized() whenever a row-visibility count changes.
        mixerBusAddButton.setBounds (utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight));
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        mixerSendAddChooser.setBounds (utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight));
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        for (std::size_t row = 0; row < mixerSendLevelSliders.size(); ++row)
        {
            if (! mixerSendLevelSliders[row].isVisible())
            {
                mixerSendLevelSliders[row].setBounds ({});
                mixerSendLabels[row].setBounds ({});
                mixerSendRemoves[row].setBounds ({});
                continue;
            }

            auto sendRow = utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerSendRowHeight);
            mixerSendRemoves[row].setBounds (
                sendRow.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerFxSlotRemoveWidth));
            mixerSendLabels[row].setBounds (
                sendRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerFxParamLabelWidth));
            mixerSendLevelSliders[row].setBounds (sendRow);
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        }

        for (std::size_t index = 0; index < mixerFxParamSliders.size(); ++index)
        {
            if (! mixerFxParamSliders[index].isVisible())
            {
                mixerFxParamSliders[index].setBounds ({});
                mixerFxParamLabels[index].setBounds ({});
                continue;
            }

            auto paramRow = utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxParamRowHeight);
            mixerFxParamLabels[index].setBounds (
                paramRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerFxParamLabelWidth));
            mixerFxParamSliders[index].setBounds (paramRow);
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        }

        const int selectedStrip = appModel.selectedMixerTrackStripIndex();
        auto lane = mixerStripBounds (selectedStrip > 0 ? selectedStrip : 0)
                        .reduced (yesdaw::ui::UiTheme::Layout::mixerControlLaneInsetX,
                                  yesdaw::ui::UiTheme::Layout::mixerControlLaneInsetY);
        mixerTrackSelect.setBounds (lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerTrackSelectHeight));
        lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerTrackSelectBottomGap);
        mixerPan.setBounds (lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerPanHeight)
                                .reduced (yesdaw::ui::UiTheme::Layout::mixerPanInsetX,
                                          yesdaw::ui::UiTheme::Layout::mixerPanInsetY));
        auto buttonRow = lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerButtonRowHeight)
                             .reduced (yesdaw::ui::UiTheme::Layout::mixerButtonRowInsetX,
                                       yesdaw::ui::UiTheme::Layout::mixerButtonRowInsetY);
        const std::array<juce::Button*, 2> mixerButtons { &mixerSolo, &mixerMute };
        const int mixerButtonWidth = juce::jmin (
            yesdaw::ui::UiTheme::Layout::mixerButtonWidth,
            buttonRow.getWidth() / static_cast<int> (mixerButtons.size()));
        for (juce::Button* button : mixerButtons)
            button->setBounds (buttonRow.removeFromLeft (mixerButtonWidth));
        lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerButtonBottomGap);
        auto faderArea = lane.removeFromTop (
            juce::jmax (yesdaw::ui::UiTheme::Layout::mixerFaderMinHeight,
                        lane.getHeight() - yesdaw::ui::UiTheme::Layout::mixerFaderBottomReserve));
        mixerFader.setBounds (faderArea.withWidth (yesdaw::ui::UiTheme::Layout::mixerFaderWidth)
                                  .withCentre ({ faderArea.getCentreX(), faderArea.getCentreY() }));
    }

    void layoutAutomationLaneControls()
    {
        const auto timeline = timelineBounds();
        automationLaneToggle.setBounds (yesdaw::ui::UiTheme::Layout::automationLaneToggleBounds (timeline));
        automationLaneRow.setBounds (yesdaw::ui::UiTheme::Layout::automationLaneRowBounds (timeline));
        automationLaneCanvas.setBounds (yesdaw::ui::UiTheme::Layout::automationLaneRowBounds (timeline));
        automationBreakpointAddButton.setBounds (
            yesdaw::ui::UiTheme::Layout::automationBreakpointAddButtonBounds (timeline));
        automationBreakpointDeleteButton.setBounds (
            yesdaw::ui::UiTheme::Layout::automationBreakpointDeleteButtonBounds (timeline));
    }

    void suspendDesktopAudioCallback()
    {
        if (desktopAudioCallbackSuspendDepth++ != 0)
            return;

        resumeDesktopAudioAfterSuspend = desktopAudioCallbackRegistered;
        if (desktopAudioCallbackRegistered)
        {
            audioDeviceManager.removeAudioCallback (this);
            desktopAudioCallbackRegistered = false;
        }
    }

    void resumeDesktopAudioCallback()
    {
        if (desktopAudioCallbackSuspendDepth <= 0 || --desktopAudioCallbackSuspendDepth != 0)
            return;

        if (resumeDesktopAudioAfterSuspend && audioDeviceManager.getCurrentAudioDevice() != nullptr)
        {
            audioDeviceManager.addAudioCallback (this);
            desktopAudioCallbackRegistered = true;
        }
        resumeDesktopAudioAfterSuspend = false;
    }

    // The keymap's declared chords are live application shortcuts: any KeyPress whose chord matches a
    // registered action dispatches through the SAME handleAction path the toolbar uses, so Space plays,
    // Ctrl+Z undoes, Del deletes the selected Clip, and every binding stays mechanically listable.
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::escapeKey && cancelInProgressEdit())
            return true;

        const std::string chord = chordForKeyPress (key);
        if (chord.empty())
            return false;

        const yesdaw::ui::UiActionId action = appModel.registry().keymap().actionForChord (chord);
        if (action == yesdaw::ui::UiActionId::Count)
            return false;

        handleAction (action);
        refreshActionState();
        repaint();
        return true;
    }

    [[nodiscard]] bool cancelInProgressEdit()
    {
        bool cancelled = timelineInput.cancelInProgressEdit();
        if (trackRenameEditor.isVisible())
        {
            dismissTrackRenameEditor();
            cancelled = true;
        }
        if (clipRenameEditor.isVisible())
        {
            dismissClipRenameEditor();
            cancelled = true;
        }

        if (cancelled)
        {
            refreshActionState();
            repaint();
        }
        return cancelled;
    }

    void handleAction (yesdaw::ui::UiActionId action)
    {
        suspendDesktopAudioCallback();
        handleActionWhileAudioStopped (action);
        resumeDesktopAudioCallback();
    }

    // Vertical track scroll (E5): one shared whole-row offset moves the timeline lanes and the
    // track rail together. The shared clamp honors WHICHEVER surface overflows more, and each
    // surface pins its own applied offset so its last row never scrolls past the window bottom.
    void scrollTrackRowsBy (int rowDelta)
    {
        const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (
            timelineInput.getLocalBounds(), makeTimelineState());
        const int maxRows = std::max (geometry.maxTrackScrollRows, trackListInput.maxScrollRows());
        timelineTrackScrollRows = std::clamp (timelineTrackScrollRows + rowDelta, 0, maxRows);
        repaint();
    }

    void zoomTimelineAtAnchor (double anchorSeconds, double factor)
    {
        const double previousZoom = timelineZoomFactor;
        timelineZoomFactor = std::clamp (timelineZoomFactor * factor,
                                         yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                                         yesdaw::ui::UiTheme::Layout::timelineZoomMax);
        if (timelineZoomFactor != previousZoom)
        {
            const double zoomRatio = previousZoom / timelineZoomFactor;
            timelineScrollSeconds = anchorSeconds - (anchorSeconds - timelineScrollSeconds) * zoomRatio;
        }
        if (timelineZoomFactor == yesdaw::ui::UiTheme::Layout::timelineZoomMin)
            timelineScrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
    }

    [[nodiscard]] double timelinePixelsPerSecondFor (double totalSeconds) const noexcept
    {
        const double fitPixelsPerSecond = static_cast<double> (juce::jmax (
                                              yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                              timelineInput.getWidth()
                                                  - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                        / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                    totalSeconds);
        return fitPixelsPerSecond * timelineZoomFactor;
    }

    [[nodiscard]] double timelineVisibleSecondsFor (double totalSeconds) const noexcept
    {
        return static_cast<double> (juce::jmax (1, timelineInput.getWidth()))
             / std::max (1.0, timelinePixelsPerSecondFor (totalSeconds));
    }

    void followPlaybackPlayhead()
    {
        if (! appModel.context().playheadFollowEnabled
            || ! appModel.context().isPlaying
            || ! appModel.project().sampleRate.isValid())
            return;

        const double visibleSeconds = timelineVisibleSecondsFor (timelineTotalSeconds);
        if (visibleSeconds <= 0.0)
            return;

        const double playheadSeconds = static_cast<double> (
                                           std::max<std::int64_t> (0, appModel.context().playheadFrame))
                                     / appModel.project().sampleRate.hz;
        if (playheadSeconds >= timelineScrollSeconds + visibleSeconds)
        {
            const double elapsedPages = std::floor (
                (playheadSeconds - timelineScrollSeconds) / visibleSeconds);
            timelineScrollSeconds += std::max (1.0, elapsedPages) * visibleSeconds;
        }
        else if (playheadSeconds < timelineScrollSeconds)
        {
            const double pagesBack = std::ceil (
                (timelineScrollSeconds - playheadSeconds) / visibleSeconds);
            timelineScrollSeconds -= std::max (1.0, pagesBack) * visibleSeconds;
        }

        const double maxScroll = std::max (0.0, timelineTotalSeconds - visibleSeconds);
        timelineScrollSeconds = std::clamp (timelineScrollSeconds, 0.0, maxScroll);
    }

    // Real menu bar (usable-DAW P1): the painted FILE/EDIT/VIEW text is gone; a juce::MenuBarComponent
    // over the same header spot dispatches registered actions through the SAME handleAction path the
    // toolbar and keymap use. The model is mechanically testable without opening popups.
    juce::StringArray getMenuBarNames() override
    {
        return { "File", "Edit", "View", "Options", "Help" };
    }

    [[nodiscard]] static std::span<const yesdaw::ui::UiActionId> menuActionsForIndex (int topLevelMenuIndex)
    {
        using yesdaw::ui::UiActionId;
        static constexpr std::array<UiActionId, 6> kFileMenu {
            UiActionId::ProjectNew,        UiActionId::ProjectOpen,        UiActionId::ProjectSave,
            UiActionId::ProjectSaveAs,     UiActionId::ProjectImportAudio, UiActionId::ProjectExportAudio,
        };
        static constexpr std::array<UiActionId, 9> kEditMenu {
            UiActionId::EditUndo,          UiActionId::EditRedo,           UiActionId::TimelineClipCut,
            UiActionId::TimelineClipCopy,  UiActionId::TimelineClipPaste,  UiActionId::TimelineClipDuplicate,
            UiActionId::TimelineClipDelete, UiActionId::TimelineClipSelectAllTrack,
            UiActionId::TimelineClipSelectAllProject,
        };
        static constexpr std::array<UiActionId, 3> kViewMenu {
            UiActionId::ViewTimeline, UiActionId::ViewMixer, UiActionId::ViewPianoRoll,
        };
        static constexpr std::array<UiActionId, 9> kOptionsMenu {
            UiActionId::TransportToggleMetronome, UiActionId::TransportToggleLoop,
            UiActionId::TimelineSnapDisable,      UiActionId::TimelineSnapSetBar,
            UiActionId::TimelineSnapSetBeat,      UiActionId::TimelineSnapSetSixteenth,
            UiActionId::TimelineTogglePlayheadFollow,
            UiActionId::TransportToggleReturnToStartOnStop,
            UiActionId::TransportToggleRecordCountIn,
        };
        static constexpr std::array<UiActionId, 1> kHelpMenu { UiActionId::HelpShowKeymap };

        switch (topLevelMenuIndex)
        {
            case 0: return kFileMenu;
            case 1: return kEditMenu;
            case 2: return kViewMenu;
            case 3: return kOptionsMenu;
            case 4: return kHelpMenu;
            default: return {};
        }
    }

    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String&) override
    {
        juce::PopupMenu menu;
        for (const yesdaw::ui::UiActionId action : menuActionsForIndex (topLevelMenuIndex))
        {
            const auto& descriptor =
                yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (action)];
            menu.addItem (static_cast<int> (action) + 1,
                          descriptor.label,
                          appModel.registry().stateFor (action, appModel.context()).enabled,
                          (action == yesdaw::ui::UiActionId::TimelineTogglePlayheadFollow
                               && appModel.context().playheadFollowEnabled)
                              || (action == yesdaw::ui::UiActionId::TransportToggleReturnToStartOnStop
                                  && appModel.context().returnToStartOnStopEnabled)
                              || (action == yesdaw::ui::UiActionId::TransportToggleRecordCountIn
                                  && appModel.context().recordCountInEnabled));
        }

        // Open Recent (B39): the File menu lists the MRU bundles, most recent first, on item ids
        // above the action range.
        if (topLevelMenuIndex == 0)
        {
            juce::PopupMenu recent;
            const std::vector<std::filesystem::path> recents = appModel.recentProjectBundles();
            for (std::size_t i = 0; i < recents.size(); ++i)
                recent.addItem (kRecentMenuBaseId + static_cast<int> (i),
                                juce::String (recents[i].stem().string()));
            menu.addSubMenu ("Open Recent", recent, ! recents.empty());
        }

        return menu;
    }

    void menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/) override
    {
        if (menuItemID >= kRecentMenuBaseId
            && menuItemID < kRecentMenuBaseId + static_cast<int> (yesdaw::ui::UiAppModel::kRecentProjectsLimit))
        {
            const std::vector<std::filesystem::path> recents = appModel.recentProjectBundles();
            const std::size_t index = static_cast<std::size_t> (menuItemID - kRecentMenuBaseId);
            if (index < recents.size())
                openProjectBundleAtPath (recents[index]);
            refreshActionState();
            repaint();
            return;
        }

        if (menuItemID <= 0 || menuItemID > static_cast<int> (yesdaw::ui::kUiActionCount))
            return;

        handleAction (static_cast<yesdaw::ui::UiActionId> (menuItemID - 1));
        refreshActionState();
        repaint();
    }

    // Device chooser plumbing (usable-DAW P1): harness seams win when injected; the native shell
    // talks to the JUCE device manager.
    [[nodiscard]] std::vector<std::string> enumerateAudioOutputDeviceNames()
    {
        if (fileChoices.listAudioOutputDevices)
            return fileChoices.listAudioOutputDevices();

        std::vector<std::string> names;
        if (! desktopAudioRequested)
            return names;

        for (juce::AudioIODeviceType* type : audioDeviceManager.getAvailableDeviceTypes())
        {
            if (type == nullptr)
                continue;

            type->scanForDevices();
            for (const juce::String& name : type->getDeviceNames (false))
                names.push_back (name.toStdString());
        }
        return names;
    }

    [[nodiscard]] bool selectAudioOutputDeviceByName (const std::string& name)
    {
        if (fileChoices.selectAudioOutputDevice)
            return fileChoices.selectAudioOutputDevice (name);

        if (! desktopAudioRequested)
            return false;

        juce::AudioDeviceManager::AudioDeviceSetup setup = audioDeviceManager.getAudioDeviceSetup();
        setup.outputDeviceName = juce::String (name);
        return audioDeviceManager.setAudioDeviceSetup (setup, true).isEmpty();
    }

    void refreshAudioDeviceChooser()
    {
        refreshingAudioDeviceChooser = true;
        audioDeviceChooserNames = enumerateAudioOutputDeviceNames();
        audioDeviceChooser.clear (juce::dontSendNotification);

        juce::String current;
        if (juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
            current = device->getName();

        for (std::size_t index = 0; index < audioDeviceChooserNames.size(); ++index)
        {
            audioDeviceChooser.addItem (juce::String (audioDeviceChooserNames[index]),
                                        static_cast<int> (index) + 1);
            if (current.isNotEmpty() && current == juce::String (audioDeviceChooserNames[index]))
                audioDeviceChooser.setSelectedId (static_cast<int> (index) + 1, juce::dontSendNotification);
        }

        audioDeviceChooser.setEnabled (! audioDeviceChooserNames.empty());
        refreshingAudioDeviceChooser = false;
    }

    void handleActionWhileAudioStopped (yesdaw::ui::UiActionId action)
    {
        switch (action)
        {
            case yesdaw::ui::UiActionId::ProjectNew:
                if (fileChoices.chooseNewProjectBundle)
                {
                    const std::filesystem::path path = fileChoices.chooseNewProjectBundle();
                    if (! path.empty())
                    {
                        if (fileChoices.makeNewProject)
                            (void) appModel.createProjectBundle (path, fileChoices.makeNewProject());
                        else
                            (void) appModel.createProjectBundle (path);
                    }
                }
                return;

            case yesdaw::ui::UiActionId::ProjectOpen:
                if (fileChoices.chooseOpenProjectBundle)
                {
                    const std::filesystem::path path = fileChoices.chooseOpenProjectBundle();
                    if (! path.empty())
                        openProjectBundleAtPath (path);
                }
                return;

            case yesdaw::ui::UiActionId::ProjectExportAudio:
                if (fileChoices.chooseExportAudioFile)
                {
                    const std::filesystem::path path = fileChoices.chooseExportAudioFile();
                    if (! path.empty())
                        (void) appModel.exportAudioFile (path);
                }
                return;

            case yesdaw::ui::UiActionId::ProjectImportAudio:
                if (fileChoices.chooseImportAudioFile)
                {
                    const std::filesystem::path path = fileChoices.chooseImportAudioFile();
                    if (! path.empty())
                    {
                        if (auto decoded = decodeProjectWav (path))
                        {
                            // Import lands on the SELECTED Track when the rail has a selection.
                            const auto& tracks = appModel.project().tracks;
                            if (selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size()))
                                (void) appModel.importAudioFileToTrack (
                                    path, std::move (*decoded),
                                    tracks[static_cast<std::size_t> (selectedTrackLane)].id);
                            else
                                (void) appModel.importAudioFile (path, std::move (*decoded));
                        }
                    }
                }
                return;

            case yesdaw::ui::UiActionId::ProjectSaveAs:
                if (fileChoices.chooseSaveAsProjectBundle)
                {
                    const std::filesystem::path path = fileChoices.chooseSaveAsProjectBundle();
                    if (! path.empty())
                        (void) appModel.saveProjectBundleAs (path);
                }
                return;

            case yesdaw::ui::UiActionId::TransportRecord:
            {
                // Real capture when the desktop device has live inputs (P0-1); the deterministic
                // synthetic-take path remains for the injected-choices harness and inputless devices.
                if (appModel.realRecordingCaptureActive())
                {
                    (void) appModel.stopRealRecordingCaptureAndCommit();
                    return;
                }

                juce::AudioIODevice* const device = audioDeviceManager.getCurrentAudioDevice();
                const int activeInputs = device != nullptr
                    ? device->getActiveInputChannels().countNumberOfSetBits()
                    : 0;
                if (desktopAudioCallbackRegistered && device != nullptr && activeInputs > 0)
                {
                    const bool armed = appModel.context().recordingTrackArmed
                                    && appModel.context().recordingInputSelected;
                    if (! armed)
                        (void) appModel.dispatch (yesdaw::ui::UiActionId::RecordingArmTrack);

                    (void) appModel.startRealRecordingCapture (
                        activeInputs,
                        device->getCurrentSampleRate(),
                        static_cast<std::int64_t> (device->getInputLatencyInSamples()),
                        static_cast<std::int64_t> (device->getOutputLatencyInSamples()));
                    return;
                }

                (void) appModel.dispatch (action);
                return;
            }

            case yesdaw::ui::UiActionId::DeviceRefreshAudio:
                refreshAudioDeviceChooser();
                (void) appModel.dispatch (action);
                return;

            case yesdaw::ui::UiActionId::TrackRename:
                if (selectedTrackLane >= 0)
                    openTrackRenameEditor();
                return;

            case yesdaw::ui::UiActionId::EditRenameSelection:
                if (appModel.context().timelineClipSelected)
                    openClipRenameEditor();
                else if (selectedTrackLane >= 0)
                    openTrackRenameEditor();
                return;

            case yesdaw::ui::UiActionId::TrackRemove:
                removeSelectedTrack();
                return;

            case yesdaw::ui::UiActionId::TrackDuplicate:
                duplicateSelectedTrack();
                return;

            case yesdaw::ui::UiActionId::TrackMoveUp:
                moveSelectedTrack (-1);
                return;

            case yesdaw::ui::UiActionId::TrackMoveDown:
                moveSelectedTrack (1);
                return;

            case yesdaw::ui::UiActionId::TrackToggleMute:
            case yesdaw::ui::UiActionId::TrackToggleSolo:
            case yesdaw::ui::UiActionId::TrackToggleArm:
                toggleSelectedTrackKey (action);
                return;

            case yesdaw::ui::UiActionId::TrackSelectPrevious:
            case yesdaw::ui::UiActionId::TrackSelectNext:
                // Context-sensitive (B34): in the Piano Roll with a note selected, Up/Down
                // transpose the selection by one semitone; elsewhere they walk the track rail.
                if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll
                    && appModel.context().midiNoteSelected)
                {
                    (void) appModel.transposeSelectedPianoRollNotes (
                        action == yesdaw::ui::UiActionId::TrackSelectPrevious ? 1 : -1);
                    return;
                }
                selectAdjacentTrackLane (action);
                return;

            case yesdaw::ui::UiActionId::TimelineClipSelectAllTrack:
                // Context-sensitive (B34): in the Piano Roll, Ctrl+A selects every note in the
                // selected MIDI clip; elsewhere it keeps selecting the track's clips.
                if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll
                    && appModel.context().midiClipSelected)
                {
                    (void) appModel.selectAllPianoRollNotes();
                    return;
                }
                (void) appModel.dispatch (action);
                return;

            case yesdaw::ui::UiActionId::TimelineClipDelete:
                // Context-sensitive (B34): in the Piano Roll with a note selection, Del deletes
                // the selected notes; elsewhere it keeps deleting timeline clips.
                if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll
                    && appModel.context().midiNoteSelected)
                {
                    (void) appModel.deleteSelectedPianoRollNotes();
                    return;
                }
                (void) appModel.dispatch (action);
                return;

            case yesdaw::ui::UiActionId::TimelineClipDuplicate:
                // Context-sensitive (B35): in the Piano Roll with a note selected, Ctrl+D lands a
                // fresh copy one grid step later; elsewhere it keeps duplicating timeline clips.
                if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll
                    && appModel.context().midiNoteSelected)
                {
                    (void) appModel.duplicateSelectedPianoRollNote (kPianoRollSnapGridTicks);
                    return;
                }
                (void) appModel.dispatch (action);
                return;

            case yesdaw::ui::UiActionId::PianoRollNoteDuplicate:
                (void) appModel.duplicateSelectedPianoRollNote (kPianoRollSnapGridTicks);
                return;

            case yesdaw::ui::UiActionId::TimelineClipSplit:
                (void) appModel.splitSelectedTimelineClipAt (
                    static_cast<yesdaw::engine::Tick> (
                        std::max<std::int64_t> (0, appModel.context().playheadFrame)));
                return;

            case yesdaw::ui::UiActionId::TimelineZoomFitProject:
                if (appModel.dispatch (action).dispatched)
                {
                    timelineZoomFactor = yesdaw::ui::UiTheme::Layout::timelineZoomMin;
                    timelineScrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
                }
                return;

            case yesdaw::ui::UiActionId::TimelineZoomFitLoop:
                if (appModel.dispatch (action).dispatched && appModel.project().sampleRate.isValid())
                {
                    const std::int64_t loopStart = appModel.playbackLoopStartFrame();
                    const std::int64_t loopEnd = appModel.playbackLoopEndFrame();
                    if (loopStart >= 0 && loopEnd > loopStart)
                    {
                        const double sampleRateHz = appModel.project().sampleRate.hz;
                        const double loopDurationSeconds = static_cast<double> (loopEnd - loopStart)
                                                         / sampleRateHz;
                        timelineZoomFactor = std::clamp (
                            std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                      timelineTotalSeconds)
                                / loopDurationSeconds,
                            yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                            yesdaw::ui::UiTheme::Layout::timelineZoomMax);
                        timelineScrollSeconds = static_cast<double> (loopStart) / sampleRateHz;
                    }
                }
                return;

            case yesdaw::ui::UiActionId::TimelineZoomIn:
                if (appModel.dispatch (action).dispatched && appModel.project().sampleRate.isValid())
                {
                    const double playheadSeconds = static_cast<double> (
                        std::max<std::int64_t> (0, appModel.context().playheadFrame))
                                                 / appModel.project().sampleRate.hz;
                    zoomTimelineAtAnchor (
                        playheadSeconds, yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep);
                }
                return;

            case yesdaw::ui::UiActionId::TimelineZoomOut:
                if (appModel.dispatch (action).dispatched && appModel.project().sampleRate.isValid())
                {
                    const double playheadSeconds = static_cast<double> (
                        std::max<std::int64_t> (0, appModel.context().playheadFrame))
                                                 / appModel.project().sampleRate.hz;
                    zoomTimelineAtAnchor (
                        playheadSeconds, 1.0 / yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep);
                }
                return;

            case yesdaw::ui::UiActionId::ViewPianoRoll:
                (void) appModel.dispatch (action);
                (void) appModel.selectFirstMidiClip();
                return;

            default:
                (void) appModel.dispatch (action);
                return;
        }
    }

    void refreshActionState()
    {
        rebuildTimelineClipViews();
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto action = toolbarActions[i];
            // The device + recording cluster is header chrome (row 3) and stays visible in every
            // view — the old hide-in-mixer rule existed only because it used to float over the rail.
            buttons[i].setVisible (true);
            const auto state = appModel.registry().stateFor (action, appModel.context());
            const bool hasRequiredPlayback = ! toolbarActionRequiresPlayback (action) || appModel.playbackReady();
            buttons[i].setEnabled (state.enabled && hasRequiredPlayback);
            buttons[i].setToggleState ((action == yesdaw::ui::UiActionId::TransportToggleLoop && appModel.context().loopEnabled)
                                           || (action == yesdaw::ui::UiActionId::RecordingArmTrack
                                               && appModel.context().recordingTrackArmed)
                                           || (action == yesdaw::ui::UiActionId::RecordingSetMonitoringPolicy
                                               && appModel.context().recordingMonitoringSelected)
                                           || (action == yesdaw::ui::UiActionId::RecordingAssembleComp
                                               && appModel.context().recordingCompSelected)
                                           || (action == yesdaw::ui::UiActionId::ViewMixer
                                               && appModel.context().activePanel == yesdaw::ui::UiPanel::Mixer)
                                           || (action == yesdaw::ui::UiActionId::ViewPianoRoll
                                               && appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll),
                                       juce::dontSendNotification);
        }

        refreshAutosaveRecoveryControls();
        const bool exportInProgress = appModel.context().audioExportInProgress;
        exportAudioButton.setVisible (! exportInProgress);
        exportAudioProgress.setVisible (exportInProgress);
        exportAudioCancelButton.setVisible (exportInProgress);
        exportAudioButton.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::ProjectExportAudio,
                                          appModel.context()).enabled);
        exportAudioCancelButton.setEnabled (
            exportInProgress
            && appModel.registry().stateFor (yesdaw::ui::UiActionId::ProjectExportAudioCancel,
                                             appModel.context()).enabled);
        exportAudioProgress.setText (exportAudioProgressText(), juce::dontSendNotification);
        masterLoudnessReadout.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadLoudness,
                                          appModel.context()).enabled);
        masterLoudnessReadout.setButtonText (masterLoudnessReadoutText());
        timelineInput.setVisible (appModel.context().activePanel == yesdaw::ui::UiPanel::Timeline);
        pianoRollInput.setVisible (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll);
        {
            refreshingTimeMapControls = true;
            const bool tempoEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::TransportSetTempo, appModel.context()).enabled;
            headerTempoControl.setEnabled (tempoEnabled);
            headerMeterChooser.setEnabled (
                appModel.registry().stateFor (yesdaw::ui::UiActionId::TransportSetMeter, appModel.context()).enabled);
            if (appModel.context().projectLoaded && ! appModel.project().tempoMap.empty())
                headerTempoControl.setValue (appModel.project().tempoMap.front().bpm, juce::dontSendNotification);
            if (appModel.context().projectLoaded && ! appModel.project().meterMap.empty())
            {
                const auto& head = appModel.project().meterMap.front();
                for (std::size_t i = 0; i < kHeaderMeterChoices.size(); ++i)
                    if (kHeaderMeterChoices[i].first == head.numerator
                        && kHeaderMeterChoices[i].second == head.denominator)
                        headerMeterChooser.setSelectedId (static_cast<int> (i) + 1, juce::dontSendNotification);
            }
            refreshingTimeMapControls = false;
        }
        {
            const std::vector<yesdaw::engine::FxInsert> chain = appModel.selectedStripFxChain();
            const bool chooserEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerFxInsertAdd, appModel.context()).enabled;
            mixerFxAddChooser.setEnabled (chooserEnabled);
            for (std::size_t slot = 0; slot < mixerFxSlotToggles.size(); ++slot)
            {
                const bool present = slot < chain.size();
                mixerFxSlotToggles[slot].setVisible (present);
                mixerFxSlotRemoves[slot].setVisible (present);
                mixerFxSlotEdits[slot].setVisible (present);
                if (! present)
                    continue;

                const yesdaw::engine::FxInsert& insert = chain[slot];
                juce::String label;
                switch (insert.kind)
                {
                    case yesdaw::engine::FxKind::Eq:         label = "EQ"; break;
                    case yesdaw::engine::FxKind::Compressor: label = "Comp"; break;
                    case yesdaw::engine::FxKind::Delay:      label = "Delay"; break;
                    case yesdaw::engine::FxKind::Reverb:     label = "Reverb"; break;
                    case yesdaw::engine::FxKind::Limiter:    label = "Limiter"; break;
                }
                mixerFxSlotToggles[slot].setButtonText (insert.enabled ? label : label + " (byp)");
                mixerFxSlotEdits[slot].setToggleState (selectedFxParamSlot == static_cast<int> (slot),
                                                       juce::dontSendNotification);
            }

            if (selectedFxParamSlot >= 0 && static_cast<std::size_t> (selectedFxParamSlot) >= chain.size())
                selectedFxParamSlot = -1;

            const std::size_t visibleFxSlotRows = std::min (chain.size(), mixerFxSlotToggles.size());
            if (visibleFxSlotRows != lastVisibleFxSlotRows)
            {
                lastVisibleFxSlotRows = visibleFxSlotRows;
                resized();
            }

            refreshingFxParamControls = true;
            std::size_t used = 0;
            if (selectedFxParamSlot >= 0)
            {
                const yesdaw::engine::FxKind kind =
                    chain[static_cast<std::size_t> (selectedFxParamSlot)].kind;
                const bool paramEditEnabled =
                    appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerFxInsertParamSet,
                                                  appModel.context()).enabled;
                for (std::uint32_t paramId = 0;
                     paramId < yesdaw::ui::UiTheme::Layout::mixerFxParamProbeLimit
                     && used < mixerFxParamSliders.size();
                     ++paramId)
                {
                    if (! yesdaw::engine::fxKindAcceptsParameterId (kind, paramId))
                        continue;

                    const yesdaw::engine::ParamSpec spec = yesdaw::engine::fxParamSpecForKind (kind, paramId);
                    const double normalized = appModel.fxInsertParamValueOnSelectedStrip (
                        static_cast<std::size_t> (selectedFxParamSlot), paramId);
                    mixerFxParamSliderIds[used] = paramId;
                    // Alt+click resets the bound parameter to its ParamSpec default.
                    mixerFxParamSliders[used].setDoubleClickReturnValue (
                        true, yesdaw::engine::normalizedDefault (spec));
                    mixerFxParamSliders[used].setValue (normalized, juce::dontSendNotification);
                    mixerFxParamSliders[used].setEnabled (paramEditEnabled);
                    mixerFxParamSliders[used].setVisible (true);
                    mixerFxParamLabels[used].setText (
                        juce::String (spec.name)
                            + " " + juce::String (yesdaw::engine::mapNormalized (spec, normalized), 1)
                            + spec.unit,
                        juce::dontSendNotification);
                    mixerFxParamLabels[used].setVisible (true);
                    ++used;
                }
            }
            for (std::size_t index = used; index < mixerFxParamSliders.size(); ++index)
            {
                mixerFxParamSliders[index].setVisible (false);
                mixerFxParamLabels[index].setVisible (false);
            }
            refreshingFxParamControls = false;
            if (used != lastVisibleFxParamRows)
            {
                lastVisibleFxParamRows = used;
                resized();
            }
        }
        {
            refreshingSendControls = true;
            const auto& project = appModel.project();
            mixerBusAddButton.setEnabled (
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerBusAdd, appModel.context()).enabled);

            mixerSendAddChooser.clear (juce::dontSendNotification);
            for (std::size_t busIndex = 0; busIndex < project.buses.size(); ++busIndex)
                mixerSendAddChooser.addItem (juce::String (project.buses[busIndex].strip.name),
                                             static_cast<int> (busIndex) + 1);
            const bool sendAddEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerSendAdd, appModel.context()).enabled
                && ! project.buses.empty();
            mixerSendAddChooser.setEnabled (sendAddEnabled);

            const std::vector<yesdaw::engine::SendRow> sends = appModel.selectedTrackSends();
            const bool sendEditEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerSendSetLevel,
                                              appModel.context()).enabled;
            for (std::size_t row = 0; row < mixerSendLevelSliders.size(); ++row)
            {
                const bool present = row < sends.size();
                mixerSendLevelSliders[row].setVisible (present);
                mixerSendLabels[row].setVisible (present);
                mixerSendRemoves[row].setVisible (present);
                if (! present)
                    continue;

                juce::String busName ("Bus?");
                for (const auto& bus : project.buses)
                    if (bus.id == sends[row].busId)
                        busName = juce::String (bus.strip.name);
                mixerSendLabels[row].setText (busName, juce::dontSendNotification);
                mixerSendLevelSliders[row].setValue (sends[row].linearGain, juce::dontSendNotification);
                mixerSendLevelSliders[row].setEnabled (sendEditEnabled);
            }
            refreshingSendControls = false;
            const std::size_t visibleSendRows = std::min (sends.size(), mixerSendLevelSliders.size());
            if (visibleSendRows != lastVisibleSendRows)
            {
                lastVisibleSendRows = visibleSendRows;
                resized();
            }
        }
        {
            refreshingSnapChooser = true;
            timelineSnapChooser.setVisible (appModel.context().activePanel == yesdaw::ui::UiPanel::Timeline);
            timelineSnapChooser.setEnabled (appModel.context().projectLoaded);
            const int snapId = appModel.snapUnit() == yesdaw::ui::UiAppModel::UiSnapUnit::Off ? 1
                             : appModel.snapUnit() == yesdaw::ui::UiAppModel::UiSnapUnit::Bar ? 2
                             : appModel.snapUnit() == yesdaw::ui::UiAppModel::UiSnapUnit::Sixteenth ? 4
                             : 3;
            timelineSnapChooser.setSelectedId (snapId, juce::dontSendNotification);
            refreshingSnapChooser = false;
        }
        {
            refreshingRepeatPasteChooser = true;
            const bool timelineVisible = appModel.context().activePanel == yesdaw::ui::UiPanel::Timeline;
            const auto repeatState = appModel.registry().stateFor (
                yesdaw::ui::UiActionId::TimelineClipRepeatPaste,
                appModel.context());
            timelineRepeatPasteChooser.setVisible (timelineVisible);
            timelineRepeatPasteChooser.setEnabled (repeatState.enabled);
            timelineRepeatPasteChooser.setSelectedId (
                appModel.repeatPasteCount(), juce::dontSendNotification);
            refreshingRepeatPasteChooser = false;
        }
        const bool railVisible = appModel.context().activePanel != yesdaw::ui::UiPanel::Mixer;
        trackListInput.setVisible (railVisible);
        trackAddButton.setVisible (railVisible);
        trackAddButton.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::TrackAdd, appModel.context()).enabled);
        if (! railVisible)
            dismissTrackRenameEditor();
        if (! appModel.context().timelineClipSelected)
            dismissClipRenameEditor();
        if (selectedTrackLane >= static_cast<int> (appModel.project().tracks.size()))
            selectedTrackLane = static_cast<int> (appModel.project().tracks.size()) - 1;
        const bool inspectorVisible = appModel.context().activePanel != yesdaw::ui::UiPanel::Mixer
                                   && appModel.context().timelineClipSelected;
        inspectorStart.setVisible (inspectorVisible);
        inspectorEnd.setVisible (inspectorVisible);
        inspectorLength.setVisible (inspectorVisible);
        inspectorGain.setVisible (inspectorVisible);
        inspectorFadeIn.setVisible (inspectorVisible);
        inspectorFadeOut.setVisible (inspectorVisible);
        inspectorFadeCurve.setVisible (inspectorVisible);
        refreshAutomationLaneControls();
        refreshInspectorControls();
        refreshMixerControls();
    }

    void refreshAutomationLaneControls()
    {
        constexpr yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::TimelineAutomationToggleTrackLane;
        const auto state = appModel.registry().stateFor (action, appModel.context());
        const bool timelineVisible = appModel.context().activePanel == yesdaw::ui::UiPanel::Timeline;
        automationLaneToggle.setVisible (timelineVisible);
        automationLaneToggle.setEnabled (state.enabled);
        automationLaneToggle.setToggleState (appModel.context().timelineAutomationTrackLaneVisible,
                                             juce::dontSendNotification);

        automationLaneRow.setText (automationLaneRowText(), juce::dontSendNotification);
        const bool laneVisible = timelineVisible && appModel.context().timelineAutomationTrackLaneVisible;
        automationLaneRow.setVisible (laneVisible);
        automationLaneCanvas.setVisible (laneVisible);
        if (laneVisible)
            automationLaneCanvas.repaint();

        const auto addState = appModel.registry().stateFor (
            yesdaw::ui::UiActionId::TimelineAutomationAddBreakpoint,
            appModel.context());
        automationBreakpointAddButton.setVisible (laneVisible);
        automationBreakpointAddButton.setEnabled (laneVisible
                                                  && addState.enabled
                                                  && appModel.firstTrackFaderAutomationLane() != nullptr);

        const yesdaw::engine::AutomationLaneData* const lane = appModel.firstTrackFaderAutomationLane();
        const auto deleteState = appModel.registry().stateFor (
            yesdaw::ui::UiActionId::TimelineAutomationDeleteBreakpoint,
            appModel.context());
        automationBreakpointDeleteButton.setVisible (laneVisible);
        automationBreakpointDeleteButton.setEnabled (laneVisible
                                                     && deleteState.enabled
                                                     && lane != nullptr
                                                     && ! lane->points.empty());
    }

    [[nodiscard]] juce::String automationLaneRowText() const
    {
        const yesdaw::engine::Project& project = appModel.project();
        if (! appModel.context().projectLoaded || project.tracks.empty())
            return "No Track automation";

        const yesdaw::engine::Track& track = project.tracks.front();
        const yesdaw::engine::AutomationLaneData* visibleLane = nullptr;
        for (const yesdaw::engine::AutomationLaneData& lane : project.automationLanes)
        {
            if (lane.ownerEntity == track.id
                && lane.role == yesdaw::engine::AutomationTargetRole::TrackFader
                && lane.paramId == yesdaw::engine::FaderNode::kGainParameterId)
            {
                visibleLane = &lane;
                break;
            }
        }

        const juce::String trackName = track.strip.name.empty() ? "Track 1" : juce::String (track.strip.name);
        const int breakpointCount = visibleLane == nullptr ? 0 : static_cast<int> (visibleLane->points.size());
        return trackName + " - Track fader - " + juce::String (breakpointCount) + " breakpoints";
    }

    void refreshAutosaveRecoveryControls()
    {
        const bool visible = appModel.context().autosaveRecoveryPending;
        const auto restoreState = appModel.registry().stateFor (yesdaw::ui::UiActionId::AutosaveRecoveryRestore,
                                                                appModel.context());
        const auto discardState = appModel.registry().stateFor (yesdaw::ui::UiActionId::AutosaveRecoveryDiscard,
                                                                appModel.context());

        autosaveRestoreButton.setVisible (visible);
        autosaveDiscardButton.setVisible (visible);
        autosaveRestoreButton.setEnabled (visible && restoreState.enabled);
        autosaveDiscardButton.setEnabled (visible && discardState.enabled);
    }

    void refreshInspectorControls()
    {
        const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId());
        const bool selected = appModel.context().timelineClipSelected && clip != nullptr;

        inspectorStart.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipMove,
                                                                 appModel.context()).enabled);
        inspectorEnd.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipTrim,
                                                               appModel.context()).enabled);
        inspectorLength.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipTrim,
                                                                  appModel.context()).enabled);
        inspectorGain.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetGain,
                                                                appModel.context()).enabled);
        inspectorFadeIn.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                  appModel.context()).enabled);
        inspectorFadeOut.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                   appModel.context()).enabled);
        inspectorFadeCurve.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                     appModel.context()).enabled);

        refreshingInspectorControls = true;
        if (selected && appModel.project().sampleRate.isValid())
        {
            const double sampleRate = appModel.project().sampleRate.hz;
            const double startSeconds = static_cast<double> (clip->timelineStart) / sampleRate;
            const double lengthSeconds = static_cast<double> (clip->timelineLength) / sampleRate;
            const double endSeconds = startSeconds + lengthSeconds;
            const double maxSeconds = std::max (
                yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback,
                endSeconds * yesdaw::ui::UiTheme::Layout::inspectorTimeSliderRangePaddingScale);
            setInspectorTimeSliderRange (inspectorStart, maxSeconds);
            setInspectorTimeSliderRange (inspectorEnd, maxSeconds);
            setInspectorTimeSliderRange (inspectorLength, maxSeconds);
            inspectorStart.setValue (std::clamp (startSeconds,
                                                 yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                                                 maxSeconds),
                                     juce::dontSendNotification);
            inspectorEnd.setValue (std::clamp (endSeconds,
                                               yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                                               maxSeconds),
                                   juce::dontSendNotification);
            inspectorLength.setValue (std::clamp (lengthSeconds,
                                                  yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                                                  maxSeconds),
                                      juce::dontSendNotification);
            inspectorGain.setValue (clip->gain, juce::dontSendNotification);
            inspectorFadeIn.setValue (std::clamp (static_cast<double> (clip->fadeIn) / sampleRate,
                                                  yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMinSeconds,
                                                  yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMaxSeconds),
                                      juce::dontSendNotification);
            inspectorFadeOut.setValue (std::clamp (static_cast<double> (clip->fadeOut) / sampleRate,
                                                   yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMinSeconds,
                                                   yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMaxSeconds),
                                        juce::dontSendNotification);
            inspectorFadeCurve.setSelectedId (kInspectorEqualPowerFadeCurveId, juce::dontSendNotification);
        }
        else
        {
            setInspectorTimeSliderRange (inspectorStart,
                                         yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback);
            setInspectorTimeSliderRange (inspectorEnd,
                                         yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback);
            setInspectorTimeSliderRange (inspectorLength,
                                         yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback);
            inspectorStart.setValue (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderDefaultSeconds,
                                     juce::dontSendNotification);
            inspectorEnd.setValue (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderDefaultSeconds,
                                   juce::dontSendNotification);
            inspectorLength.setValue (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderDefaultSeconds,
                                      juce::dontSendNotification);
            inspectorGain.setValue (yesdaw::ui::UiTheme::Layout::inspectorGainSliderDefault,
                                    juce::dontSendNotification);
            inspectorFadeIn.setValue (yesdaw::ui::UiTheme::Layout::inspectorFadeSliderDefaultSeconds,
                                      juce::dontSendNotification);
            inspectorFadeOut.setValue (yesdaw::ui::UiTheme::Layout::inspectorFadeSliderDefaultSeconds,
                                       juce::dontSendNotification);
            inspectorFadeCurve.setSelectedId (kInspectorEqualPowerFadeCurveId, juce::dontSendNotification);
        }
        refreshingInspectorControls = false;
    }

    void setInspectorTimeSliderRange (juce::Slider& slider, double maxSeconds)
    {
        slider.setRange (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMinSeconds,
                         std::max (yesdaw::ui::UiTheme::Layout::inspectorTimeSliderMaxSecondsFallback, maxSeconds),
                         yesdaw::ui::UiTheme::Layout::inspectorTimeSliderIntervalSeconds);
    }

    [[nodiscard]] std::optional<yesdaw::engine::Tick> inspectorTickFromSeconds (double seconds) const noexcept
    {
        return timelineTickFromSeconds (seconds);
    }

    void setSelectedInspectorStartFromSlider()
    {
        if (! findProjectClipById (appModel.selectedTimelineClipId()))
            return;

        if (const auto tick = inspectorTickFromSeconds (inspectorStart.getValue()))
            (void) appModel.moveSelectedTimelineClipTo (*tick);

        refreshActionState();
        repaint();
    }

    void setSelectedInspectorEndFromSlider()
    {
        const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId());
        if (clip == nullptr)
            return;

        const std::optional<yesdaw::engine::Tick> endTick = inspectorTickFromSeconds (inspectorEnd.getValue());
        if (! endTick || *endTick <= clip->timelineStart)
            return;

        (void) appModel.trimSelectedTimelineClipRightTo (*endTick);
        refreshActionState();
        repaint();
    }

    void setSelectedInspectorLengthFromSlider()
    {
        const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId());
        if (clip == nullptr)
            return;

        const std::optional<yesdaw::engine::Tick> lengthTick = inspectorTickFromSeconds (inspectorLength.getValue());
        if (! lengthTick || *lengthTick <= 0)
            return;

        (void) appModel.trimSelectedTimelineClipRightTo (clip->timelineStart + *lengthTick);
        refreshActionState();
        repaint();
    }

    void setSelectedInspectorFadesFromSliders()
    {
        const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId());
        if (clip == nullptr || ! appModel.project().sampleRate.isValid())
            return;

        const double sampleRate = appModel.project().sampleRate.hz;
        const auto toTicks = [sampleRate, clip] (double seconds) {
            return std::clamp<yesdaw::engine::Tick> (
                static_cast<yesdaw::engine::Tick> (std::llround (seconds * sampleRate)),
                0,
                std::max<yesdaw::engine::Tick> (0, clip->timelineLength));
        };

        (void) appModel.setSelectedTimelineClipFades (
            toTicks (inspectorFadeIn.getValue()),
            toTicks (inspectorFadeOut.getValue()));
        refreshActionState();
        repaint();
    }

    void refreshMixerControls()
    {
        const yesdaw::engine::Project& project = appModel.project();
        const bool projectHasTrack = appModel.context().projectLoaded && ! project.tracks.empty();
        const bool selected = appModel.context().mixerTargetSelected;

        for (juce::Component* control : std::array<juce::Component*, 12> {
                 &mixerTrackSelect,
                 &mixerFader,
                 &mixerPan,
                 &mixerMetersReadout,
                 &mixerSendsReadout,
                 &mixerSendLevelEdit,
                 &mixerFxSlotsReadout,
                 &mixerGainReductionReadout,
                 &mixerBusFxSlotsReadout,
                 &mixerFxSlotToggle,
                 &mixerMute,
                 &mixerSolo })
        {
            control->setVisible (projectHasTrack);
        }

        const float interactiveAlpha = selected
                                           ? yesdaw::ui::UiTheme::Tone::componentVisibleAlpha
                                           : yesdaw::ui::UiTheme::Tone::componentHiddenAlpha;
        mixerFader.setAlpha (interactiveAlpha);
        mixerPan.setAlpha (interactiveAlpha);
        mixerMute.setAlpha (interactiveAlpha);
        mixerSolo.setAlpha (interactiveAlpha);

        mixerTrackSelect.setEnabled (projectHasTrack);
        mixerFader.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetSetFader,
                                                             appModel.context()).enabled);
        mixerPan.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetSetPan,
                                                           appModel.context()).enabled);
        mixerMute.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetToggleMute,
                                                            appModel.context()).enabled);
        mixerSolo.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetToggleSolo,
                                                            appModel.context()).enabled);
        mixerMetersReadout.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadMeters,
                                                                     appModel.context()).enabled);
        mixerSendsReadout.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadSends,
                                                                    appModel.context()).enabled);
        const bool firstSendAvailable = appModel.firstTrackFirstSendAutomationLane() != nullptr;
        mixerSendLevelEdit.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerSetFirstSendLevel,
                                          appModel.context()).enabled
            && firstSendAvailable);
        mixerFxSlotsReadout.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadFxSlots,
                                                                      appModel.context()).enabled);
        mixerGainReductionReadout.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadGainReduction,
                                          appModel.context()).enabled);
        mixerBusFxSlotsReadout.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadBusFxSlots,
                                          appModel.context()).enabled);
        const bool firstFxSlotAvailable = projectHasTrack && ! project.tracks.front().strip.fxChain.empty();
        mixerFxSlotToggle.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerToggleFirstFxSlotEnabled,
                                          appModel.context()).enabled
            && firstFxSlotAvailable);

        refreshingMixerControls = true;
        if (projectHasTrack)
        {
            const auto& strip = project.tracks.front().strip;
            mixerTrackSelect.setButtonText (strip.name.empty() ? "Track 1" : juce::String (strip.name));
            mixerFader.setValue (strip.linearGain, juce::dontSendNotification);
            mixerPan.setValue (strip.pan, juce::dontSendNotification);
            mixerMute.setToggleState (selected && strip.muted, juce::dontSendNotification);
            mixerSolo.setToggleState (selected && strip.soloed, juce::dontSendNotification);
            mixerMetersReadout.setButtonText (mixerMetersReadoutText());
            mixerSendsReadout.setButtonText (mixerSendsReadoutText());
            mixerSendLevelEdit.setButtonText ("Send");
            mixerFxSlotsReadout.setButtonText (mixerFxSlotsReadoutText());
            mixerGainReductionReadout.setButtonText (mixerGainReductionReadoutText());
            mixerBusFxSlotsReadout.setButtonText (mixerBusFxSlotsReadoutText());
            mixerFxSlotToggle.setButtonText ("FX");
            mixerFxSlotToggle.setToggleState (firstFxSlotAvailable && strip.fxChain.front().enabled,
                                              juce::dontSendNotification);
        }
        else
        {
            mixerTrackSelect.setButtonText ("No Track");
            mixerFader.setValue (yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault,
                                 juce::dontSendNotification);
            mixerPan.setValue (yesdaw::ui::UiTheme::Layout::mixerPanSliderDefault,
                               juce::dontSendNotification);
            mixerMute.setToggleState (false, juce::dontSendNotification);
            mixerSolo.setToggleState (false, juce::dontSendNotification);
            mixerMetersReadout.setButtonText ("Meters");
            mixerSendsReadout.setButtonText ("Sends");
            mixerSendLevelEdit.setButtonText ("Set send");
            mixerFxSlotsReadout.setButtonText ("Track FX");
            mixerFxSlotToggle.setButtonText ("Bypass FX");
            mixerGainReductionReadout.setButtonText ("Gain reduction");
            mixerBusFxSlotsReadout.setButtonText ("Bus FX");
            mixerFxSlotToggle.setToggleState (false, juce::dontSendNotification);
        }
        refreshingMixerControls = false;
    }

    [[nodiscard]] juce::String mixerMetersReadoutText() const
    {
        const auto surface = currentMixerSurface();
        if (surface.tracks.empty())
            return "Meters: no Track";

        const auto& track = surface.tracks.front();
        juce::String text = juce::String (track.name.empty() ? "Track 1" : track.name)
            + " meter node " + juce::String (static_cast<int> (track.meterNodeId));

        if (! track.meter.valid)
            return text + " peak n/a";

        return text
            + " peak L " + juce::String (track.meter.peakLeft, 2)
            + " R " + juce::String (track.meter.peakRight, 2);
    }

    [[nodiscard]] juce::String mixerSendsReadoutText() const
    {
        const auto surface = currentMixerSurface();
        if (surface.tracks.empty())
            return "Sends: no Track";

        const auto& track = surface.tracks.front();
        if (track.sends.empty())
            return juce::String (track.name.empty() ? "Track 1" : track.name) + " sends: none";

        const yesdaw::ui::UiMixerSendReadout& send = track.sends.front();
        return juce::String (track.name.empty() ? "Track 1" : track.name)
             + " Send " + juce::String (static_cast<int> (send.sendOrdinal))
             + " node " + juce::String (static_cast<int> (send.faderNodeId))
             + " level " + juce::String (send.normalizedLevel, 2)
             + " points " + juce::String (static_cast<int> (send.breakpointCount));
    }

    [[nodiscard]] static const char* fxKindName (yesdaw::engine::FxKind kind) noexcept
    {
        switch (kind)
        {
            case yesdaw::engine::FxKind::Eq: return "EQ";
            case yesdaw::engine::FxKind::Compressor: return "Compressor";
            case yesdaw::engine::FxKind::Delay: return "Delay";
            case yesdaw::engine::FxKind::Reverb: return "Reverb";
            case yesdaw::engine::FxKind::Limiter: return "Limiter";
        }

        return "Unknown";
    }

    [[nodiscard]] juce::String mixerFxSlotsReadoutText() const
    {
        const auto surface = currentMixerSurface();
        if (surface.tracks.empty())
            return "FX: no Track";

        const auto& track = surface.tracks.front();
        if (track.fxSlots.empty())
            return juce::String (track.name.empty() ? "Track 1" : track.name) + " FX: none";

        const yesdaw::ui::UiMixerFxSlotReadout& slot = track.fxSlots.front();
        return juce::String (track.name.empty() ? "Track 1" : track.name)
             + " FX " + juce::String (static_cast<int> (slot.slotOrdinal))
             + " " + juce::String (fxKindName (slot.kind))
             + " node " + juce::String (static_cast<int> (slot.fxNodeId))
             + " params " + juce::String (static_cast<int> (slot.parameterCount))
             + (slot.enabled ? " on" : " off");
    }

    [[nodiscard]] juce::String mixerGainReductionReadoutText() const
    {
        const auto surface = currentMixerSurface();
        if (surface.tracks.empty())
            return "GR: no Track";

        const auto& track = surface.tracks.front();
        const yesdaw::ui::UiMixerFxSlotReadout* readout = nullptr;
        for (const yesdaw::ui::UiMixerFxSlotReadout& slot : track.fxSlots)
        {
            if (slot.gainReductionValid || slot.gainReductionAvailable)
            {
                readout = &slot;
                break;
            }
        }

        if (readout == nullptr)
            return juce::String (track.name.empty() ? "Track 1" : track.name) + " GR: none";

        juce::String text = juce::String (track.name.empty() ? "Track 1" : track.name)
            + " GR " + juce::String (static_cast<int> (readout->slotOrdinal))
            + " " + juce::String (fxKindName (readout->kind))
            + " node " + juce::String (static_cast<int> (readout->fxNodeId));

        if (readout->gainReductionValid)
            return text + " " + juce::String (readout->gainReductionDb, 2) + " dB";

        return text + " n/a";
    }

    [[nodiscard]] juce::String mixerBusFxSlotsReadoutText() const
    {
        const auto surface = currentMixerSurface();
        if (surface.buses.empty())
            return "Bus FX: no Bus";

        const auto& bus = surface.buses.front();
        if (bus.fxSlots.empty())
            return juce::String (bus.name.empty() ? "Bus 1" : bus.name) + " FX: none";

        const yesdaw::ui::UiMixerFxSlotReadout& slot = bus.fxSlots.front();
        return juce::String (bus.name.empty() ? "Bus 1" : bus.name)
             + " FX " + juce::String (static_cast<int> (slot.slotOrdinal))
             + " " + juce::String (fxKindName (slot.kind))
             + " node " + juce::String (static_cast<int> (slot.fxNodeId))
             + " params " + juce::String (static_cast<int> (slot.parameterCount))
             + (slot.enabled ? " on" : " off");
    }

    [[nodiscard]] juce::String masterLoudnessReadoutText() const
    {
        const auto surface = currentMixerSurface();
        if (! surface.loudness.valid)
            return "-- LUFS";

        return juce::String (surface.loudness.integratedLufs, 1) + " LUFS";
    }

    [[nodiscard]] juce::String exportAudioProgressText() const
    {
        const int percent = appModel.context().audioExportProgressPercent;
        if (percent < 0)
            return "Export --";

        return "Export " + juce::String (percent) + "%";
    }

    void drawHeader (juce::Graphics& g) const
    {
        const auto headerBounds = getLocalBounds().withHeight (kHeaderHeight);
        juce::ColourGradient headerGradient (
            yesdaw::ui::UiTheme::Color::panelRaised(),
            static_cast<float> (headerBounds.getCentreX()),
            static_cast<float> (headerBounds.getY()),
            yesdaw::ui::UiTheme::Color::canvasLayer(),
            static_cast<float> (headerBounds.getCentreX()),
            static_cast<float> (headerBounds.getBottom()),
            false);
        g.setGradientFill (headerGradient);
        g.fillRect (headerBounds);
        g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
            yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
        g.fillRect (headerBounds.withHeight (
            yesdaw::ui::UiTheme::Layout::controlInnerHighlightHeight));

        const std::array headerSections {
            yesdaw::ui::UiTheme::Layout::headerProjectSectionBounds(),
            yesdaw::ui::UiTheme::Layout::headerTransportSectionBounds(),
            yesdaw::ui::UiTheme::Layout::headerMasterSectionBounds()
        };
        for (const auto section : headerSections)
        {
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (section.toFloat(), yesdaw::ui::UiTheme::Radius::panel);
            g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
                yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
            g.drawRoundedRectangle (
                section.toFloat().reduced (
                    yesdaw::ui::UiTheme::Layout::panelOutlineInset),
                yesdaw::ui::UiTheme::Radius::panel,
                yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
        }

        drawTransportReadouts (g);
        drawMasterMeter (g);
        g.setColour (kPanelStroke);
        g.fillRect (getLocalBounds()
                        .withHeight (kHeaderHeight)
                        .removeFromBottom (yesdaw::ui::UiTheme::Space::hairline));
    }

    void drawTransportReadouts (juce::Graphics& g) const
    {
        auto time = juce::Rectangle<int> (
            yesdaw::ui::UiTheme::Layout::headerTransportTimeX,
            yesdaw::ui::UiTheme::Layout::headerTransportReadoutY,
            yesdaw::ui::UiTheme::Layout::headerTransportTimeWidth,
            yesdaw::ui::UiTheme::Layout::headerTransportReadoutHeight);
        fillPanel (g, time, yesdaw::ui::UiTheme::Radius::panel);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::transportClock));
        const double sampleRate = appModel.project().sampleRate.isValid()
                                      ? appModel.project().sampleRate.hz
                                      : 48000.0;
        const double seconds = static_cast<double> (std::max<std::int64_t> (0, appModel.context().playheadFrame))
                             / sampleRate;
        const int totalMilliseconds = static_cast<int> (std::floor (seconds * 1000.0));
        const int hours = totalMilliseconds / 3'600'000;
        const int minutes = (totalMilliseconds / 60'000) % 60;
        const int wholeSeconds = (totalMilliseconds / 1'000) % 60;
        const int milliseconds = totalMilliseconds % 1'000;
        const juce::String clock = juce::String::formatted (
            "%02d:%02d:%02d:%03d", hours, minutes, wholeSeconds, milliseconds);
        g.drawText (clock,
                    time.reduced (yesdaw::ui::UiTheme::Layout::headerTransportTextInsetX,
                                  yesdaw::ui::UiTheme::Layout::headerTransportClockInsetY)
                        .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportClockHeight),
                    juce::Justification::centred,
                    false);
        drawSmallLabel (g,
                        "TIME",
                        time.reduced (yesdaw::ui::UiTheme::Layout::headerTransportTextInsetX,
                                      yesdaw::ui::UiTheme::Layout::headerTransportLabelInsetY),
                        juce::Justification::centred);

        const juce::String tempo = appModel.context().projectLoaded && ! appModel.project().tempoMap.empty()
                                     ? juce::String (appModel.project().tempoMap.front().bpm, 2)
                                     : juce::String ("--");
        const juce::String meter = appModel.context().projectLoaded && ! appModel.project().meterMap.empty()
                                     ? juce::String (appModel.project().meterMap.front().numerator)
                                         + "/" + juce::String (appModel.project().meterMap.front().denominator)
                                     : juce::String ("--");
        const std::array<std::pair<juce::String, const char*>, 3> readouts {{
            { tempo, "TEMPO" },
            { meter, "TIME SIG" },
            { "--", "KEY" }
        }};

        auto box = juce::Rectangle<int> (
            yesdaw::ui::UiTheme::Layout::headerTransportBoxX,
            yesdaw::ui::UiTheme::Layout::headerTransportReadoutY,
            yesdaw::ui::UiTheme::Layout::headerTransportBoxWidth,
            yesdaw::ui::UiTheme::Layout::headerTransportReadoutHeight);
        for (const auto& readout : readouts)
        {
            auto cell = box.removeFromLeft (yesdaw::ui::UiTheme::Layout::headerTransportCellWidth);
            fillPanel (g, cell, yesdaw::ui::UiTheme::Radius::none);
            g.setColour (kText);
            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::readout));
            g.drawText (readout.first,
                        cell.reduced (yesdaw::ui::UiTheme::Layout::headerTransportCellInsetX,
                                      yesdaw::ui::UiTheme::Layout::headerTransportValueInsetY)
                            .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportValueHeight),
                        juce::Justification::centred,
                        false);
            drawSmallLabel (g,
                            readout.second,
                            cell.reduced (yesdaw::ui::UiTheme::Layout::headerTransportCellInsetX,
                                          yesdaw::ui::UiTheme::Layout::headerTransportLabelInsetY),
                            juce::Justification::centred);
        }

    }

    void drawMasterMeter (juce::Graphics& g) const
    {
        auto master = juce::Rectangle<int> (yesdaw::ui::UiTheme::Layout::headerMasterX,
                                            yesdaw::ui::UiTheme::Layout::headerMasterY,
                                            yesdaw::ui::UiTheme::Layout::headerMasterWidth,
                                            yesdaw::ui::UiTheme::Layout::headerMasterHeight);
        drawSmallLabel (g, "MASTER", master.removeFromTop (yesdaw::ui::UiTheme::Layout::headerMasterLabelHeight));
        auto meter = master.removeFromTop (yesdaw::ui::UiTheme::Layout::headerMasterMeterHeight)
                         .withWidth (yesdaw::ui::UiTheme::Layout::headerMasterMeterWidth);
        drawHorizontalMeter (g, meter, liveMasterPeakLeft.load (std::memory_order_acquire));

        yesdaw::ui::drawSettingsIcon (
            g,
            juce::Rectangle<float> (
                static_cast<float> (getWidth() - yesdaw::ui::UiTheme::Layout::headerStatusIconRightInset),
                static_cast<float> (yesdaw::ui::UiTheme::Layout::headerStatusIconY),
                static_cast<float> (yesdaw::ui::UiTheme::Layout::headerStatusIconSize),
                static_cast<float> (yesdaw::ui::UiTheme::Layout::headerStatusIconSize)),
            kMutedText);
    }

    void drawTrackList (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        fillPanel (g, area);
        auto header = area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        drawSmallLabel (g,
                        "TRACKS",
                        header.reduced (yesdaw::ui::UiTheme::Layout::trackListHeaderInsetX,
                                        yesdaw::ui::UiTheme::Layout::trackListHeaderInsetY)
                            .withHeight (yesdaw::ui::UiTheme::Layout::trackListHeaderLabelHeight));

        if (! appModel.context().projectLoaded || appModel.project().tracks.empty())
        {
            drawSmallLabel (g,
                            "No Project",
                            area.reduced (yesdaw::ui::UiTheme::Layout::trackListEmptyLabelInset),
                            juce::Justification::centred);
            return;
        }

        const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                          area.getHeight() / static_cast<int> (appModel.project().tracks.size()));
        // Vertical track scroll (E5): the rail paints from its effective (clamped) shared row
        // offset; scrolled-out rows above the window are skipped so paint matches rowBounds/rowAt.
        for (std::size_t i = static_cast<std::size_t> (trackListInput.effectiveScrollRows());
             i < appModel.project().tracks.size(); ++i)
        {
            auto row = area.removeFromTop (rowHeight);
            if (row.getHeight() < rowHeight)
                break;
            const auto& projectTrack = appModel.project().tracks[i];
            const juce::String fallbackName = "Track " + juce::String (static_cast<int> (i + 1));
            const juce::String trackName = projectTrack.strip.name.empty()
                                               ? fallbackName
                                               : juce::String (projectTrack.strip.name);
            const juce::Colour trackColour = kPurple;

            const auto rowSurface = row.reduced (
                yesdaw::ui::UiTheme::Layout::trackListRowHorizontalInset,
                yesdaw::ui::UiTheme::Layout::trackListRowVerticalInset);
            juce::ColourGradient rowGradient (
                static_cast<int> (i) == selectedTrackLane
                    ? yesdaw::ui::UiTheme::Color::selectedLane()
                    : yesdaw::ui::UiTheme::Color::panelRaised(),
                static_cast<float> (rowSurface.getX()),
                static_cast<float> (rowSurface.getCentreY()),
                yesdaw::ui::UiTheme::Color::darkControl(),
                static_cast<float> (rowSurface.getRight()),
                static_cast<float> (rowSurface.getCentreY()),
                false);
            g.setGradientFill (rowGradient);
            g.fillRect (rowSurface);
            g.setColour (trackColour);
            g.fillRect (row.withWidth (yesdaw::ui::UiTheme::Layout::trackListAccentWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::trackListAccentHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::trackListAccentVerticalInset));
            g.setColour (kPanelStroke);
            g.fillRect (row.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight));

            yesdaw::ui::drawTrackGlyph (
                g,
                i,
                juce::Rectangle<float> (
                    static_cast<float> (row.getX() + yesdaw::ui::UiTheme::Layout::trackListIconLeftInset),
                    static_cast<float> (row.getY() + yesdaw::ui::UiTheme::Layout::trackListIconTopInset),
                    static_cast<float> (yesdaw::ui::UiTheme::Layout::trackListIconSize),
                    static_cast<float> (yesdaw::ui::UiTheme::Layout::trackListIconSize)),
                trackColour.withAlpha (yesdaw::ui::UiTheme::Tone::trackIconAlpha));

            auto mixSummary = row.withRight (
                                     row.getRight()
                                     - yesdaw::ui::UiTheme::Layout::trackListMixSummaryRightInset)
                                  .removeFromRight (
                                      yesdaw::ui::UiTheme::Layout::trackListMixSummaryWidth)
                                  .reduced (
                                      yesdaw::ui::UiTheme::Space::none,
                                      yesdaw::ui::UiTheme::Layout::trackListMixSummaryVerticalInset);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (mixSummary.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
            g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
                yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
            g.drawRoundedRectangle (
                mixSummary.toFloat().reduced (
                    yesdaw::ui::UiTheme::Layout::panelOutlineInset),
                yesdaw::ui::UiTheme::Radius::sm,
                yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
            g.setColour (yesdaw::ui::UiTheme::Color::faintText());
            g.setFont (yesdaw::ui::UiTheme::Type::font (
                yesdaw::ui::UiTheme::Type::tiny,
                juce::Font::bold));
            auto mixLabel = mixSummary.withTrimmedLeft (
                                          yesdaw::ui::UiTheme::Layout::trackListMixLabelLeftInset)
                                .withWidth (
                                    yesdaw::ui::UiTheme::Layout::trackListMixLabelWidth)
                                .withHeight (
                                    yesdaw::ui::UiTheme::Layout::trackListMixLabelHeight);
            g.drawText ("PAN",
                        mixLabel.translated (
                            yesdaw::ui::UiTheme::Space::none,
                            yesdaw::ui::UiTheme::Layout::trackListPanLabelTopInset),
                        juce::Justification::centredLeft,
                        false);
            g.drawText ("VOL",
                        mixLabel.translated (
                            yesdaw::ui::UiTheme::Space::none,
                            yesdaw::ui::UiTheme::Layout::trackListVolumeLabelTopInset),
                        juce::Justification::centredLeft,
                        false);

            auto pan = row.withRight (
                              row.getRight() - yesdaw::ui::UiTheme::Layout::trackListPanRightInset)
                           .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListPanDiameter)
                           .withY (row.getY() + yesdaw::ui::UiTheme::Layout::trackListPanTopInset)
                           .withHeight (yesdaw::ui::UiTheme::Layout::trackListPanDiameter);
            g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
                yesdaw::ui::UiTheme::Tone::shadowAlpha));
            g.fillEllipse (pan.toFloat().translated (
                0.0f,
                static_cast<float> (yesdaw::ui::UiTheme::Layout::controlShadowOffset)));
            g.setColour (yesdaw::ui::UiTheme::Color::knobFace());
            g.fillEllipse (pan.toFloat());
            g.setColour (yesdaw::ui::UiTheme::Color::knobArc());
            g.drawEllipse (pan.toFloat().reduced (yesdaw::ui::UiTheme::Layout::controlOutlineInset),
                           yesdaw::ui::UiTheme::Layout::iconFineStrokeWidth);
            // Live pan (usable-DAW P2): the knob indicator swings with strip.pan and the readout
            // shows C / L% / R%.
            const float panValue = juce::jlimit (-1.0f, 1.0f, projectTrack.strip.pan);
            const float panAngle = panValue * yesdaw::ui::UiTheme::Layout::trackListPanArcRadians;
            const float panRadius =
                static_cast<float> (pan.getCentreY()
                                    - pan.getY()
                                    - yesdaw::ui::UiTheme::Layout::trackListPanIndicatorInset);
            const juce::Point<float> panCentre = pan.toFloat().getCentre();
            g.setColour (trackColour);
            g.drawLine (panCentre.x,
                        panCentre.y,
                        panCentre.x + panRadius * std::sin (panAngle),
                        panCentre.y - panRadius * std::cos (panAngle),
                        yesdaw::ui::UiTheme::Layout::iconBoldStrokeWidth);
            const int panPercent = juce::roundToInt (std::abs (panValue) * 100.0f);
            const juce::String panText =
                panPercent == 0 ? juce::String ("C")
                                : (panValue < 0.0f ? juce::String ("L") : juce::String ("R"))
                                      + juce::String (panPercent);
            g.setColour (kMutedText);
            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::tiny));
            g.drawText (panText,
                        pan.withY (
                               row.getY()
                               + yesdaw::ui::UiTheme::Layout::trackListPanValueTopInset)
                            .withHeight (
                                yesdaw::ui::UiTheme::Layout::trackListMixLabelHeight),
                        juce::Justification::centred,
                        false);

            auto level = row.withRight (
                                row.getRight() - yesdaw::ui::UiTheme::Layout::trackListLevelRightInset)
                             .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListLevelWidth)
                             .withBottom (row.getBottom()
                                         - yesdaw::ui::UiTheme::Layout::trackListLevelBottomInset)
                             .withHeight (yesdaw::ui::UiTheme::Layout::trackListLevelHeight);
            g.setColour (yesdaw::ui::UiTheme::Color::meterTrack().withAlpha (
                yesdaw::ui::UiTheme::Tone::trackSliderRailAlpha));
            g.fillRoundedRectangle (level.toFloat(), yesdaw::ui::UiTheme::Radius::pill);
            const int liveWidth = juce::roundToInt (
                static_cast<float> (level.getWidth()) * projectTrack.strip.linearGain);
            g.setColour (trackColour.withAlpha (yesdaw::ui::UiTheme::Tone::trackSliderFillAlpha));
            g.fillRoundedRectangle (level.withWidth (liveWidth).toFloat(), yesdaw::ui::UiTheme::Radius::pill);
            auto levelThumb = level.withWidth (yesdaw::ui::UiTheme::Layout::trackListLevelThumbWidth)
                                  .withX (level.getX() + liveWidth
                                          - yesdaw::ui::UiTheme::Layout::trackListLevelThumbWidth / 2);
            g.setColour (yesdaw::ui::UiTheme::Color::faderThumbTop());
            g.fillRoundedRectangle (levelThumb.toFloat(), yesdaw::ui::UiTheme::Radius::sm);

            g.setColour (kText);
            g.setFont (yesdaw::ui::UiTheme::Type::font (
                yesdaw::ui::UiTheme::Type::title,
                juce::Font::bold));
            g.drawText (trackName,
                        row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                            .withHeight (yesdaw::ui::UiTheme::Layout::trackListNameHeight)
                            .translated (yesdaw::ui::UiTheme::Layout::trackListNameOffsetX,
                                         yesdaw::ui::UiTheme::Layout::trackListNameOffsetY),
                        juce::Justification::centredLeft, false);

            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::readout));
            g.drawText (juce::String (static_cast<int> (i + 1)),
                        row.withWidth (yesdaw::ui::UiTheme::Layout::trackListNumberWidth),
                        juce::Justification::centred,
                        false);

            // Live M/S cells (usable-DAW P2): the painted cells reflect the strip state; the rail
            // input layer toggles them through the same verbs as the mixer.
            auto buttonsArea = row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                                   .withTrimmedTop (yesdaw::ui::UiTheme::Layout::trackListButtonsTop)
                                   .withHeight (yesdaw::ui::UiTheme::Layout::trackListButtonsHeight);
            const std::array<std::pair<const char*, bool>, 3> railCells {{
                { "M", projectTrack.strip.muted },
                { "S", projectTrack.strip.soloed },
                { "O", false },
            }};
            for (const auto& [label, active] : railCells)
            {
                auto cell = buttonsArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::trackListButtonWidth)
                                .reduced (yesdaw::ui::UiTheme::Layout::trackListButtonInsetX,
                                          yesdaw::ui::UiTheme::Layout::trackListButtonInsetY);
                g.setColour (active ? trackColour : yesdaw::ui::UiTheme::Color::mixerBack());
                g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                g.setColour (active ? kText
                                    : (label == std::string ("O") ? kRed : kMutedText));
                g.setFont (yesdaw::ui::UiTheme::Type::font (
                    yesdaw::ui::UiTheme::Type::caption,
                    juce::Font::bold));
                g.drawText (label, cell, juce::Justification::centred, false);
            }

            // Live meter (usable-DAW P2 + B32): the rail meter renders the live MeterNode peak with
            // the shared per-track hold/clip-latch state; a click on it clears the latch.
            auto meter = row.withRight (row.getRight() - yesdaw::ui::UiTheme::Layout::trackListMeterRightInset)
                             .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListMeterWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::trackListMeterHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::trackListMeterVerticalInset);
            const MeterHoldState railHold = i < trackMeterHold.size() ? trackMeterHold[i]
                                                                      : MeterHoldState {};
            drawMeterWithHold (g, meter, railHold.livePeak, railHold.heldPeak, railHold.clipLatched);
        }
    }

    yesdaw::ui::TimelineCanvasState makeTimelineState()
    {
        rebuildTimelineClipViews();

        yesdaw::ui::TimelineCanvasState state;
        if (! appModel.context().projectLoaded)
        {
            state.tracks = nullptr;
            state.trackCount = 0;
            state.clips = nullptr;
            state.clipStyles = nullptr;
            state.clipCount = 0;
            state.totalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
            state.playheadSeconds = yesdaw::ui::UiTheme::Layout::timelineInitialPlayheadSeconds;
        }
        else
        {
            state.tracks = projectTimelineTracks.data();
            state.trackCount = static_cast<int> (projectTimelineTracks.size());
            state.clips = timelineClips.data();
            state.clipStyles = timelineClipStyles.data();
            state.clipCount = static_cast<int> (timelineClips.size());
            state.waveformCacheLookup = [this] (int layoutClipId)
                -> std::shared_ptr<const yesdaw::persistence::WaveformPeakCache>
            {
                if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipAssetHashes.size()))
                    return {};

                return appModel.waveformService().tryGetReady (
                    timelineClipAssetHashes[static_cast<std::size_t> (layoutClipId)]);
            };
            state.totalSeconds = timelineTotalSeconds;
            state.playheadSeconds = appModel.project().sampleRate.isValid()
                                        ? static_cast<double> (appModel.context().playheadFrame)
                                            / appModel.project().sampleRate.hz
                                        : yesdaw::ui::UiTheme::Layout::timelineInitialPlayheadSeconds;
        }

        timelineMarkerLabels.clear();
        timelineMarkerViews.clear();
        if (appModel.context().projectLoaded && appModel.project().sampleRate.isValid())
        {
            const double sampleRateHz = appModel.project().sampleRate.hz;
            timelineMarkerLabels.reserve (appModel.project().markers.size());
            timelineMarkerViews.reserve (appModel.project().markers.size());
            for (const yesdaw::engine::Marker& marker : appModel.project().markers)
            {
                timelineMarkerLabels.push_back (marker.name);
                timelineMarkerViews.push_back ({ static_cast<double> (marker.tick) / sampleRateHz,
                                                 timelineMarkerLabels.back().c_str() });
            }
        }
        state.markers = timelineMarkerViews.empty() ? nullptr : timelineMarkerViews.data();
        state.markerCount = static_cast<int> (timelineMarkerViews.size());

        // Ruler range selection (parity item 25): painted from the model's transient range frames.
        if (appModel.context().timelineRangeSelected
            && appModel.context().projectLoaded
            && appModel.project().sampleRate.isValid())
        {
            const double sampleRateHz = appModel.project().sampleRate.hz;
            state.rangeSelectionActive = true;
            state.rangeStartSeconds = static_cast<double> (appModel.timelineRangeStartFrame()) / sampleRateHz;
            state.rangeEndSeconds = static_cast<double> (appModel.timelineRangeEndFrame()) / sampleRateHz;
        }

        // Live zoom + horizontal scroll (usable-DAW P1): zoom scales the fit-to-window density and
        // the scroll offset is clamped so the view never runs past the timeline end.
        state.viewport.pixelsPerSecond = timelinePixelsPerSecondFor (state.totalSeconds);
        const double visibleSeconds = timelineVisibleSecondsFor (state.totalSeconds);
        const double maxScroll = std::max (0.0, state.totalSeconds - visibleSeconds);
        timelineScrollSeconds = std::clamp (timelineScrollSeconds, 0.0, maxScroll);
        state.viewport.scrollSeconds = timelineScrollSeconds;
        // Vertical track scroll (E5): geometry clamps the shared row offset per paint/gesture.
        state.trackScrollRows = timelineTrackScrollRows;
        return state;
    }

    void rebuildTimelineClipViews()
    {
        timelineClips.clear();
        timelineClipStyles.clear();
        timelineClipIds.clear();
        timelineClipAssetHashes.clear();
        projectTimelineTracks.clear();

        const yesdaw::engine::Project& project = appModel.project();
        if (! appModel.context().projectLoaded || ! project.sampleRate.isValid())
        {
            timelineTotalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
            return;
        }

        projectTimelineTracks.reserve (project.tracks.size());
        for (const yesdaw::engine::Track& track : project.tracks)
            projectTimelineTracks.push_back ({ track.strip.name.empty() ? "Track" : track.strip.name.c_str(),
                                               kPurple,
                                               0.0f });

        double endSeconds = 0.0;
        const double sampleRate = project.sampleRate.hz;

        for (const yesdaw::engine::Clip& clip : project.clips)
        {
            const yesdaw::engine::Asset* const asset = project.findAsset (clip.assetId);
            if (! clip.id.isValid()
                || clip.timelineStart < 0
                || clip.timelineLength <= 0
                || asset == nullptr)
            {
                continue;
            }

            const auto track = std::find_if (project.tracks.begin(), project.tracks.end(), [&clip] (const auto& candidate) {
                return candidate.id == clip.trackId;
            });
            if (track == project.tracks.end())
                continue;

            const int lane = static_cast<int> (std::distance (project.tracks.begin(), track));
            const double startSeconds = static_cast<double> (clip.timelineStart) / sampleRate;
            const double lengthSeconds = static_cast<double> (clip.timelineLength) / sampleRate;
            const int id = static_cast<int> (timelineClips.size());
            timelineClips.push_back ({ id, lane, startSeconds, lengthSeconds, clip.name.c_str() });
            timelineClipStyles.push_back ({ appModel.isTimelineClipSelected (clip.id)
                                                ? yesdaw::ui::UiTheme::Color::accentBlue()
                                                : kPurple,
                                            yesdaw::ui::UiTheme::Tone::mainComponentProjectClipAlpha });
            timelineClipIds.push_back (clip.id);
            timelineClipAssetHashes.push_back (asset->contentHash);
            endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
        }

        timelineTotalSeconds = timelineClips.empty()
            ? yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds
            : std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                        endSeconds * yesdaw::ui::UiTheme::Layout::timelineProjectEndPaddingScale);
    }

    void selectTimelineClipByLayoutId (int layoutClipId, bool toggle)
    {
        dismissClipRenameEditor();
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        {
            appModel.clearTimelineClipSelection();
        }
        else
        {
            (void) appModel.selectTimelineClipForGesture (
                timelineClipIds[static_cast<std::size_t> (layoutClipId)], toggle);
        }

        refreshActionState();
        repaint();
    }

    [[nodiscard]] yesdaw::engine::EntityId automationTargetTrackId() const noexcept
    {
        const auto& tracks = appModel.project().tracks;
        if (tracks.empty())
            return {};

        const int lane = selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size())
            ? selectedTrackLane
            : int {};
        return tracks[static_cast<std::size_t> (lane)].id;
    }

    [[nodiscard]] double automationCanvasSecondsForLocalX (int localX)
    {
        const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const int timelineLocalX = localX + automationLaneCanvas.getX() - timelineInput.getX();
        return std::max (0.0,
                         state.viewport.scrollSeconds
                             + static_cast<double> (timelineLocalX - geometry.clipArea.getX()) / pixelsPerSecond);
    }

    [[nodiscard]] int automationCanvasLocalXForSeconds (double seconds)
    {
        const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const int timelineLocalX = geometry.clipArea.getX()
            + juce::roundToInt ((seconds - state.viewport.scrollSeconds) * pixelsPerSecond);
        return timelineLocalX - (automationLaneCanvas.getX() - timelineInput.getX());
    }

    [[nodiscard]] std::optional<yesdaw::engine::Tick> timelineTickFromSeconds (double seconds) const noexcept
    {
        const yesdaw::engine::Project& project = appModel.project();
        if (! project.sampleRate.isValid() || ! std::isfinite (seconds) || seconds < 0.0)
            return std::nullopt;

        const double ticks = seconds * project.sampleRate.hz;
        if (ticks > static_cast<double> (std::numeric_limits<yesdaw::engine::Tick>::max()))
            return std::nullopt;

        return static_cast<yesdaw::engine::Tick> (std::llround (ticks));
    }

    void moveTimelineClipByLayoutId (int layoutClipId, double startSeconds, bool snapToGrid)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        const yesdaw::engine::EntityId draggedClipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
        if (! appModel.isTimelineClipSelected (draggedClipId))
            (void) appModel.selectTimelineClip (draggedClipId);
        else
            (void) appModel.selectTimelineClipForGesture (draggedClipId, false);
        if (const auto tick = timelineTickFromSeconds (startSeconds))
            (void) appModel.moveSelectedTimelineClipTo (snappedTimelineTick (*tick, snapToGrid));

        refreshActionState();
        repaint();
    }

    // The active snap grid applied to a gesture tick. The gesture's Ctrl flag INVERTS the global
    // grid: grid on -> Ctrl drags fine; grid off -> Ctrl snaps one-shot.
    [[nodiscard]] yesdaw::engine::Tick snappedTimelineTick (yesdaw::engine::Tick tick, bool invertSnap) const
    {
        const bool shouldSnap = appModel.context().snapEnabled != invertSnap;
        const std::int64_t gridTicks = appModel.context().snapGridTicks;
        if (! shouldSnap || gridTicks <= 0)
            return tick;

        yesdaw::engine::Tick snapped = 0;
        if (! yesdaw::engine::snapTick (tick, yesdaw::engine::SnapGrid { gridTicks }, snapped))
            return tick;

        return std::max<yesdaw::engine::Tick> (0, snapped);
    }

    void moveTimelineClipToLaneByLayoutId (int layoutClipId, int targetLane, double startSeconds, bool snapToGrid)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        const yesdaw::engine::Project& project = appModel.project();
        if (targetLane < 0 || targetLane >= static_cast<int> (project.tracks.size()))
            return;

        const yesdaw::engine::EntityId draggedClipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
        if (! appModel.isTimelineClipSelected (draggedClipId))
            (void) appModel.selectTimelineClip (draggedClipId);
        else
            (void) appModel.selectTimelineClipForGesture (draggedClipId, false);
        if (const auto tick = timelineTickFromSeconds (startSeconds))
            (void) appModel.moveSelectedTimelineClipToTrack (
                project.tracks[static_cast<std::size_t> (targetLane)].id,
                snappedTimelineTick (*tick, snapToGrid));

        refreshActionState();
        repaint();
    }

    void copyTimelineClipByLayoutId (int layoutClipId, int targetLane, double startSeconds, bool snapToGrid)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        const yesdaw::engine::Project& project = appModel.project();
        const yesdaw::engine::EntityId sourceClipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
        const yesdaw::engine::Clip* const sourceClip = findProjectClipById (sourceClipId);
        if (sourceClip == nullptr)
            return;

        yesdaw::engine::EntityId targetTrackId = sourceClip->trackId;
        if (targetLane >= 0)
        {
            if (targetLane >= static_cast<int> (project.tracks.size()))
                return;
            targetTrackId = project.tracks[static_cast<std::size_t> (targetLane)].id;
        }

        // The dragged clip is the gesture anchor; a copy-drag on a selected member carries the
        // whole selection, exactly like the move gesture (E2).
        if (! appModel.isTimelineClipSelected (sourceClipId))
            (void) appModel.selectTimelineClip (sourceClipId);
        else
            (void) appModel.selectTimelineClipForGesture (sourceClipId, false);
        if (const auto tick = timelineTickFromSeconds (startSeconds))
            (void) appModel.copySelectedTimelineClipsTo (
                targetTrackId, snappedTimelineTick (*tick, snapToGrid));

        refreshActionState();
        repaint();
    }

    // Snap law for edge gestures (E4): the snapped tick goes straight to the verb, whose legality
    // rules (positive length, in-body split, source-window bounds) win by honest refusal.
    void splitTimelineClipByLayoutId (int layoutClipId, double splitSeconds, bool snapInvert = false)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        if (const auto tick = timelineTickFromSeconds (splitSeconds))
            (void) appModel.splitSelectedTimelineClipAt (snappedTimelineTick (*tick, snapInvert));

        refreshActionState();
        repaint();
    }

    void trimTimelineClipRightByLayoutId (int layoutClipId, double endSeconds, bool snapInvert = false)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        if (const auto tick = timelineTickFromSeconds (endSeconds))
            (void) appModel.trimSelectedTimelineClipRightTo (snappedTimelineTick (*tick, snapInvert));

        refreshActionState();
        repaint();
    }

    void adjustTimelineClipGainByLayoutId (int layoutClipId, int deltaPixels)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        const yesdaw::engine::EntityId clipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
        const yesdaw::engine::Clip* const clip = findProjectClipById (clipId);
        if (clip == nullptr)
            return;

        const float nextGain = std::clamp (
            clip->gain + static_cast<float> (deltaPixels) * yesdaw::ui::UiTheme::Layout::timelineClipGainPerDragPixel,
            0.0f,
            yesdaw::ui::UiTheme::Layout::timelineClipMaxGestureGain);

        if (std::fabs (nextGain - clip->gain) <= 0.000001f)
            return;

        (void) appModel.selectTimelineClip (clipId);
        (void) appModel.setSelectedTimelineClipGain (nextGain);

        refreshActionState();
        repaint();
    }

    void adjustTimelineClipFadeByLayoutId (int layoutClipId, bool fadeIn, double fadeSeconds)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        const yesdaw::engine::EntityId clipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
        const yesdaw::engine::Clip* const clip = findProjectClipById (clipId);
        if (clip == nullptr)
            return;

        const std::optional<yesdaw::engine::Tick> fadeTicks = timelineTickFromSeconds (fadeSeconds);
        if (! fadeTicks)
            return;

        const yesdaw::engine::Tick clampedFade =
            std::clamp<yesdaw::engine::Tick> (*fadeTicks, 0, std::max<yesdaw::engine::Tick> (0, clip->timelineLength));
        const yesdaw::engine::Tick nextFadeIn = fadeIn ? clampedFade : clip->fadeIn;
        const yesdaw::engine::Tick nextFadeOut = fadeIn ? clip->fadeOut : clampedFade;
        if (nextFadeIn == clip->fadeIn && nextFadeOut == clip->fadeOut)
            return;

        (void) appModel.selectTimelineClip (clipId);
        (void) appModel.setSelectedTimelineClipFades (nextFadeIn, nextFadeOut);

        refreshActionState();
        repaint();
    }

    [[nodiscard]] const yesdaw::engine::Clip* findProjectClipById (yesdaw::engine::EntityId clipId) const noexcept
    {
        for (const yesdaw::engine::Clip& candidate : appModel.project().clips)
            if (candidate.id == clipId)
                return &candidate;

        return nullptr;
    }

    void drawPianoRoll (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        const auto surface = currentPianoRollSurface();
        const auto panelArea = area;

        fillPanel (g, area);
        auto header = area.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollHeaderHeight);
        drawSmallLabel (g,
                        "PIANO ROLL",
                        header.reduced (yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetX,
                                        yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetY));
        drawSmallLabel (g, surface.midiClipSelected
                            ? "MIDI Clip  |  Note edits: select move length transpose quantize"
                            : "No MIDI Clip selected",
                        header.reduced (yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetX,
                                        yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetY),
                        juce::Justification::centredRight);

        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (panelArea);

        g.setColour (yesdaw::ui::UiTheme::Color::controlInsetBlack());
        g.fillRect (geometry.grid);

        for (int key = yesdaw::ui::UiTheme::Layout::pianoRollHighKey;
             key >= yesdaw::ui::UiTheme::Layout::pianoRollLowKey;
             --key)
        {
            const int y = pianoRollKeyY (geometry, key);
            auto keyRow = juce::Rectangle<int> (geometry.keyboard.getX(),
                                                y,
                                                geometry.keyboard.getWidth(),
                                                juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollKeyRowMinHeight,
                                                            juce::roundToInt (geometry.rowHeight)));
            g.setColour (isBlackMidiKey (key) ? yesdaw::ui::UiTheme::Color::pianoBlackKey()
                                               : yesdaw::ui::UiTheme::Color::panelRaised());
            g.fillRect (keyRow.reduced (yesdaw::ui::UiTheme::Layout::pianoRollKeyRowInsetX,
                                        yesdaw::ui::UiTheme::Layout::pianoRollKeyRowInsetY));
            g.setColour (kPanelStroke.withAlpha (0.72f));
            g.fillRect (juce::Rectangle<int> (geometry.grid.getX(),
                                             y,
                                             geometry.grid.getWidth(),
                                             yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth));

            if (key % 12 == 0)
            {
                g.setColour (kMutedText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (
                    yesdaw::ui::UiTheme::Type::caption,
                    juce::Font::bold));
                g.drawText ("C" + juce::String (key / 12 - 1),
                            keyRow.reduced (yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetX,
                                            yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetY),
                            juce::Justification::centredLeft, false);
            }
        }

        for (yesdaw::engine::Tick tick = 0;
             tick <= surface.timelineLength;
             tick += yesdaw::ui::UiTheme::Layout::pianoRollGridTickStep)
        {
            const int x = pianoRollTickX (geometry, surface, tick);
            g.setColour ((tick % yesdaw::ui::UiTheme::Layout::pianoRollGridStrongTickStep) == 0
                              ? yesdaw::ui::UiTheme::Color::pianoGridStrong()
                              : yesdaw::ui::UiTheme::Color::pianoGridWeak());
            g.fillRect (x,
                        geometry.grid.getY(),
                        yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth,
                        geometry.grid.getHeight());
        }

        for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
        {
            if (note.key < yesdaw::ui::UiTheme::Layout::pianoRollLowKey
                || note.key > yesdaw::ui::UiTheme::Layout::pianoRollHighKey)
                continue;

            const auto noteRect = pianoRollNoteBounds (geometry, surface, note);

            g.setColour ((note.selected ? kPurple : kCyan).withAlpha (0.34f));
            g.fillRoundedRectangle (noteRect.expanded (yesdaw::ui::UiTheme::Layout::pianoRollSelectedNoteHalo).toFloat(),
                                    yesdaw::ui::UiTheme::Radius::md);
            // Velocity tints the note body (B33): quiet notes darken toward the tint floor.
            g.setColour ((note.selected ? kPurple.brighter (0.35f) : kCyan)
                             .withMultipliedBrightness (
                                 yesdaw::ui::UiTheme::Tone::noteVelocityTintFloor
                                 + static_cast<float> (note.normalizedVelocity)
                                       * (1.0f - yesdaw::ui::UiTheme::Tone::noteVelocityTintFloor)));
            g.fillRoundedRectangle (noteRect.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
        }

        auto expression = geometry.expression;
        expression.reduce (yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetX,
                           yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetY);
        for (const yesdaw::ui::UiPianoRollExpressionLaneReadout& lane : surface.expressionLanes)
        {
            auto laneArea = expression.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneHeight)
                                .reduced (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetX,
                                          yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRect (laneArea);
            drawSmallLabel (g,
                            lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity ? "Velocity" : "Pitch",
                            laneArea.reduced (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLabelInsetX,
                                              yesdaw::ui::UiTheme::Layout::pianoRollExpressionLabelInsetY));

            const double minValue = lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity ? 0.0 : 48.0;
            const double maxValue = lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity ? 1.0 : 76.0;
            juce::Path path;

            for (std::size_t i = 0; i < lane.points.size(); ++i)
            {
                const auto& point = lane.points[i];
                const double normalized = juce::jlimit (0.0, 1.0, (point.value - minValue) / (maxValue - minValue));
                const float x = static_cast<float> (pianoRollTickX (geometry, surface, point.tick));
                const float y = static_cast<float> (laneArea.getBottom()
                                                    - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathBottomInset)
                    - static_cast<float> (normalized)
                        * static_cast<float> (laneArea.getHeight()
                                              - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathVerticalInset);
                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);

                g.setColour (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity
                                  ? yesdaw::ui::UiTheme::Meter::nominalFill()
                                  : kPurple);
                g.fillEllipse (x - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPointRadius,
                               y - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPointRadius,
                               yesdaw::ui::UiTheme::Layout::pianoRollExpressionPointDiameter,
                               yesdaw::ui::UiTheme::Layout::pianoRollExpressionPointDiameter);
            }

            g.setColour (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity
                              ? yesdaw::ui::UiTheme::Meter::nominalFill()
                              : kPurple);
            g.strokePath (path,
                          juce::PathStrokeType (
                              yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathStrokeWidth));
        }
    }

    void drawInspector (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        fillPanel (g, area);
        auto tabs = area.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorTabHeight);
        g.setColour (yesdaw::ui::UiTheme::Color::inspectorTab());
        g.fillRect (tabs.removeFromLeft (area.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorTabCount));
        drawSmallLabel (g,
                        "CLIP",
                        area.withY (tabs.getY())
                            .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTabHeight)
                            .withWidth (area.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorTabCount),
                        juce::Justification::centred);
        drawSmallLabel (g,
                        "TRACK",
                        area.withY (tabs.getY())
                            .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTabHeight)
                            .withTrimmedLeft (area.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorTabCount),
                        juce::Justification::centred);

        area.reduce (yesdaw::ui::UiTheme::Layout::inspectorContentInsetX,
                     yesdaw::ui::UiTheme::Layout::inspectorContentInsetY);
        const yesdaw::engine::Clip* const selectedClip = findProjectClipById (appModel.selectedTimelineClipId());
        if (selectedClip == nullptr)
        {
            drawSmallLabel (g, "No clip selected", area, juce::Justification::centred);
            return;
        }

        g.setColour (kPurple);
        g.fillRoundedRectangle (static_cast<float> (area.getX()),
                                static_cast<float> (area.getY()
                                                    + yesdaw::ui::UiTheme::Layout::inspectorTitleAccentTopInset),
                                static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                                static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                                yesdaw::ui::UiTheme::Radius::sm);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::title,
            juce::Font::bold));
        g.drawText (selectedClip->name.c_str(),
                    area.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorTitleTextLeftInset)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTitleTextHeight),
                    juce::Justification::centredLeft,
                    false);

        const auto drawInspectorSectionCard = [&g] (juce::Rectangle<int> section)
        {
            g.setColour (yesdaw::ui::UiTheme::Color::panelRaised());
            g.fillRoundedRectangle (section.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
                yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
            g.drawRoundedRectangle (
                section.toFloat().reduced (
                    yesdaw::ui::UiTheme::Layout::panelOutlineInset),
                yesdaw::ui::UiTheme::Radius::md,
                yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
        };

        auto stats = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                         .withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionHeight);
        const double selectedSampleRate = appModel.project().sampleRate.hz;
        const double selectedStartSeconds = static_cast<double> (selectedClip->timelineStart) / selectedSampleRate;
        const double selectedLengthSeconds = static_cast<double> (selectedClip->timelineLength) / selectedSampleRate;
        const std::array<std::pair<const char*, juce::String>, 3> statsText {{
            { "Start", juce::String (selectedStartSeconds, 3) + " s" },
            { "End", juce::String (selectedStartSeconds + selectedLengthSeconds, 3) + " s" },
            { "Length", juce::String (selectedLengthSeconds, 3) + " s" }
        }};
        for (const auto& [label, value] : statsText)
        {
            auto cell = stats.removeFromLeft (stats.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorStatsColumnCount)
                            .reduced (yesdaw::ui::UiTheme::Layout::inspectorStatsCellInsetX,
                                      yesdaw::ui::UiTheme::Layout::inspectorStatsCellInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (kMutedText);
            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::caption));
            auto textArea = cell.reduced (yesdaw::ui::UiTheme::Layout::inspectorStatsTextInset);
            g.drawText (label,
                        textArea.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorStatsLabelHeight),
                        juce::Justification::centred,
                        false);
            g.setColour (kText);
            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::body,
                juce::Font::bold));
            g.drawFittedText (value,
                              textArea.withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsValueHeight),
                              juce::Justification::centred,
                              1);
        }

        auto gain = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorGainSectionTop)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainSectionHeight);
        drawInspectorSectionCard (gain);
        drawSmallLabel (g, "GAIN", gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::title));
        const float gainValue = selectedClip->gain;
        const float gainDb = 20.0f * std::log10 (std::max (
            yesdaw::ui::UiTheme::Mixer::paintedReadoutGainFloor,
            gainValue));
        g.drawText ((gainDb >= 0.0f ? "+" : "") + juce::String (gainDb, 1) + " dB",
                    gain.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainReadoutLeftInset)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainReadoutHeight),
                    juce::Justification::centredLeft,
                    false);

        auto fades = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionTop)
                         .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionHeight);
        drawInspectorSectionCard (fades);
        drawSmallLabel (g, "FADES", fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        const double sampleRate = appModel.project().sampleRate.isValid()
                                      ? appModel.project().sampleRate.hz
                                      : yesdaw::ui::UiTheme::Layout::inspectorReadoutFallbackSampleRate;
        const double fadeInSeconds = selectedClip != nullptr
                                         ? static_cast<double> (selectedClip->fadeIn) / sampleRate
                                         : yesdaw::ui::UiTheme::Layout::inspectorFadeReadoutDefaultSeconds;
        const double fadeOutSeconds = selectedClip != nullptr
                                          ? static_cast<double> (selectedClip->fadeOut) / sampleRate
                                          : yesdaw::ui::UiTheme::Layout::inspectorFadeReadoutDefaultSeconds;
        for (const auto& label : { juce::String ("Fade In     ") + juce::String (fadeInSeconds, 3) + " s",
                                   juce::String ("Fade Out    ") + juce::String (fadeOutSeconds, 3) + " s",
                                   juce::String ("Curve       Equal power") })
        {
            auto row = fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeRowHeight)
                           .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetX,
                                     yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (kText);
            g.drawText (label,
                        row.reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeTextInsetX,
                                     yesdaw::ui::UiTheme::Layout::inspectorFadeTextInsetY),
                        juce::Justification::centredLeft,
                        false);
        }

        auto fx = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFxSectionTop)
                      .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFxSectionHeight);
        drawInspectorSectionCard (fx);
        drawSmallLabel (g, "CLIP FX", fx.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        drawSmallLabel (g, "None", fx.reduced (yesdaw::ui::UiTheme::Layout::inspectorFxTextInsetX,
                                                yesdaw::ui::UiTheme::Layout::inspectorFxTextInsetY));

        auto automation = area.withTrimmedTop (
            yesdaw::ui::UiTheme::Layout::inspectorAutomationSectionTop);
        drawInspectorSectionCard (automation);
        drawSmallLabel (
            g,
            "AUTOMATION  -  VOLUME",
            automation.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        auto chart = automation.withTrimmedTop (
                                   yesdaw::ui::UiTheme::Layout::inspectorAutomationChartTop)
                         .withHeight (
                             yesdaw::ui::UiTheme::Layout::inspectorAutomationChartHeight)
                         .reduced (
                             yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetX,
                             yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetY);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (chart.toFloat(), yesdaw::ui::UiTheme::Radius::md);

        drawSmallLabel (g, "No automation", chart, juce::Justification::centred);
    }

    void drawMixer (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        const auto surface = currentMixerSurface();
        const std::size_t stripCount = surface.tracks.size() + surface.buses.size();

        g.setColour (yesdaw::ui::UiTheme::Color::mixerBack());
        g.fillRect (area);

        auto leftTools = area.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerToolsWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::mixerToolsInsetX,
                                       yesdaw::ui::UiTheme::Layout::mixerToolsInsetY);
        fillPanel (g, leftTools, yesdaw::ui::UiTheme::Radius::md);

        const int stripWidth = std::clamp (
            area.getWidth() / (juce::jmax (yesdaw::ui::UiTheme::Layout::mixerPaintedStripMinCount,
                                           static_cast<int> (stripCount))
                               + yesdaw::ui::UiTheme::Layout::mixerPaintedStripExtraSlotCount),
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripMinWidth,
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripMaxWidth);
        for (std::size_t stripIndex = 0; stripIndex < stripCount; ++stripIndex)
        {
            const bool isBus = stripIndex >= surface.tracks.size();
            const auto& state = isBus ? surface.buses[stripIndex - surface.tracks.size()]
                                      : surface.tracks[stripIndex];
            const juce::Colour stripColour = stripColourForIndex (stripIndex);
            const int selectedTrackStrip = appModel.selectedMixerTrackStripIndex();
            const bool selected = appModel.context().mixerTargetSelected
                               && selectedTrackStrip >= 0
                               && stripIndex == static_cast<std::size_t> (selectedTrackStrip);
            const bool interactiveStrip = selected;

            auto lane = area.removeFromLeft (stripWidth)
                            .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetX,
                                      yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
                yesdaw::ui::UiTheme::Tone::shadowAlpha));
            g.fillRoundedRectangle (
                lane.toFloat().translated (
                    0.0f,
                    static_cast<float> (yesdaw::ui::UiTheme::Layout::controlShadowOffset)),
                yesdaw::ui::UiTheme::Radius::panel);
            juce::ColourGradient laneGradient (
                selected ? yesdaw::ui::UiTheme::Color::selectedStrip()
                         : yesdaw::ui::UiTheme::Color::panelRaised(),
                static_cast<float> (lane.getCentreX()),
                static_cast<float> (lane.getY()),
                yesdaw::ui::UiTheme::Color::panel(),
                static_cast<float> (lane.getCentreX()),
                static_cast<float> (lane.getBottom()),
                false);
            g.setGradientFill (laneGradient);
            g.fillRoundedRectangle (lane.toFloat(), yesdaw::ui::UiTheme::Radius::panel);
            g.setColour (selected ? kPurple : kPanelStroke);
            g.drawRoundedRectangle (lane.toFloat().reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                    yesdaw::ui::UiTheme::Radius::panel,
                                    selected
                                        ? yesdaw::ui::UiTheme::Layout::mixerPaintedStripSelectedStrokeWidth
                                        : yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);

            g.setColour (stripColour.withAlpha (yesdaw::ui::UiTheme::Tone::mixerHeaderAlpha));
            g.fillRect (lane.withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedHeaderHeight));
            g.setColour (kText);
            g.setFont (yesdaw::ui::UiTheme::Type::font (
                yesdaw::ui::UiTheme::Type::small,
                juce::Font::bold));
            g.drawFittedText (state.name,
                              lane.reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedNameInsetX,
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedNameInsetY)
                                  .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedNameHeight),
                              juce::Justification::centred,
                              1);

            if (! interactiveStrip)
            {
                auto knob = lane.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerPaintedPanTop)
                                .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedPanHeight);
                const int panDiameter = yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius * 2;
                const int panX = knob.getCentreX() - yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius;
                const int panY = knob.getY() + yesdaw::ui::UiTheme::Layout::mixerPaintedPanTopInset;
                g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
                    yesdaw::ui::UiTheme::Tone::shadowAlpha));
                g.fillEllipse (static_cast<float> (panX),
                               static_cast<float> (panY + yesdaw::ui::UiTheme::Layout::controlShadowOffset),
                               static_cast<float> (panDiameter),
                               static_cast<float> (panDiameter));
                g.setColour (yesdaw::ui::UiTheme::Color::knobFace());
                g.fillEllipse (static_cast<float> (panX),
                               static_cast<float> (panY),
                               static_cast<float> (panDiameter),
                               static_cast<float> (panDiameter));
                g.setColour (stripColour.withAlpha (
                    yesdaw::ui::UiTheme::Tone::mixerKnobHighlightAlpha));
                g.drawEllipse (static_cast<float> (panX),
                               static_cast<float> (panY),
                               static_cast<float> (panDiameter),
                               static_cast<float> (panDiameter),
                               yesdaw::ui::UiTheme::Layout::mixerPaintedPanStrokeWidth);
                const float panAngle = juce::MathConstants<float>::pi
                                     * (0.5f + state.pan * 0.35f);
                const float panCentreX = static_cast<float> (
                    panX + yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius);
                const float panCentreY = static_cast<float> (
                    panY + yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius);
                const float panIndicatorRadius = static_cast<float> (
                    yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius
                    - yesdaw::ui::UiTheme::Layout::trackListPanIndicatorInset);
                g.drawLine (panCentreX,
                            panCentreY,
                            panCentreX + std::cos (panAngle) * panIndicatorRadius,
                            panCentreY - std::sin (panAngle) * panIndicatorRadius,
                            yesdaw::ui::UiTheme::Layout::iconBoldStrokeWidth);
            }

            if (! interactiveStrip)
            {
                auto buttonsRow = lane.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerPaintedButtonsTop)
                                      .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedButtonsHeight)
                                      .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedButtonsInsetX,
                                                yesdaw::ui::UiTheme::Layout::mixerPaintedButtonsInsetY);
                for (const auto* label : { "S", "M" })
                {
                    auto cell = buttonsRow.removeFromLeft (
                                              yesdaw::ui::UiTheme::Layout::mixerPaintedButtonWidth)
                                    .reduced (
                                        yesdaw::ui::UiTheme::Layout::mixerPaintedButtonInsetX,
                                        yesdaw::ui::UiTheme::Layout::mixerPaintedButtonInsetY);
                    g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                    g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::md);
                    const bool on = (label == std::string ("S") && state.soloed)
                                 || (label == std::string ("M") && state.muted);
                    g.setColour (on ? stripColour.brighter (0.55f) : kText);
                    g.drawText (label, cell, juce::Justification::centred, false);
                }
            }

            if (state.sidechainVisible)
            {
                auto badge = lane.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerPaintedSidechainTop)
                                 .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedSidechainHeight)
                                 .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedSidechainLeftInset)
                                 .withWidth (yesdaw::ui::UiTheme::Layout::mixerPaintedSidechainWidth);
                g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                g.fillRoundedRectangle (badge.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                g.setColour (kMutedText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (
                    yesdaw::ui::UiTheme::Type::tiny,
                    juce::Font::bold));
                g.drawText ("SC", badge, juce::Justification::centred, false);
            }

            auto faderArea =
                lane.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderTop)
                    .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderBottomInset);
            const auto meter = paintedMeterBoundsForLane (lane);
            if (! isBus && stripIndex < trackMeterHold.size())
            {
                const MeterHoldState& hold = trackMeterHold[stripIndex];
                drawMeterWithHold (g, meter, hold.livePeak, hold.heldPeak, hold.clipLatched);
            }
            else
            {
                drawMeter (g, meter, state.meter.valid ? state.meter.peakLeft : 0.0f);
            }

            auto rail = faderArea.withWidth (yesdaw::ui::UiTheme::Layout::mixerPaintedRailWidth)
                            .withCentre ({ lane.getCentreX()
                                               - yesdaw::ui::UiTheme::Layout::mixerPaintedRailCenterOffsetX,
                                           faderArea.getCentreY() });
            if (! interactiveStrip)
            {
                g.setColour (yesdaw::ui::UiTheme::Color::controlInsetDeep());
                g.fillRoundedRectangle (rail.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                g.setColour (yesdaw::ui::UiTheme::Color::faintText());
                for (int tick = 0;
                     tick < yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickCount;
                     ++tick)
                {
                    const float fraction = static_cast<float> (tick)
                                         / static_cast<float> (
                                               yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickCount - 1);
                    const float tickY = static_cast<float> (rail.getY())
                                      + fraction * static_cast<float> (rail.getHeight());
                    g.drawHorizontalLine (
                        juce::roundToInt (tickY),
                        static_cast<float> (rail.getX()
                                            - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickGap
                                            - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickWidth),
                        static_cast<float> (rail.getX()
                                            - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickGap));
                }
                const int thumbY =
                    rail.getBottom() - juce::roundToInt (state.linearGain * static_cast<float> (rail.getHeight()))
                    - yesdaw::ui::UiTheme::Layout::mixerPaintedThumbCenterInset;
                auto thumb = juce::Rectangle<int> (
                    rail.getX() - yesdaw::ui::UiTheme::Layout::mixerPaintedThumbWidthOverhang / 2,
                    thumbY,
                    rail.getWidth() + yesdaw::ui::UiTheme::Layout::mixerPaintedThumbWidthOverhang,
                    yesdaw::ui::UiTheme::Layout::mixerPaintedThumbHeight);
                juce::ColourGradient thumbGradient (
                    yesdaw::ui::UiTheme::Color::faderThumbTop(),
                    static_cast<float> (thumb.getCentreX()),
                    static_cast<float> (thumb.getY()),
                    yesdaw::ui::UiTheme::Color::faderThumb(),
                    static_cast<float> (thumb.getCentreX()),
                    static_cast<float> (thumb.getBottom()),
                    false);
                g.setGradientFill (thumbGradient);
                g.fillRoundedRectangle (thumb.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
            }

            auto readout = lane.removeFromBottom (
                                    yesdaw::ui::UiTheme::Layout::mixerPaintedReadoutBottomInset)
                               .translated (0,
                                           -yesdaw::ui::UiTheme::Layout::mixerPaintedReadoutHeight)
                               .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedReadoutHeight)
                               .reduced (
                                   yesdaw::ui::UiTheme::Layout::mixerPaintedReadoutHorizontalInset,
                                   yesdaw::ui::UiTheme::Space::xxs);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (readout.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
            g.setColour (kText);
            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::caption));
            const float gainDb = 20.0f * std::log10 (std::max (
                yesdaw::ui::UiTheme::Mixer::paintedReadoutGainFloor,
                state.linearGain));
            g.drawText (juce::String (gainDb, 1), readout, juce::Justification::centred, false);
        }

        auto masterLane = area.removeFromRight (stripWidth)
                              .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetX,
                                        yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetY);
        g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
            yesdaw::ui::UiTheme::Tone::shadowAlpha));
        g.fillRoundedRectangle (
            masterLane.toFloat().translated (
                0.0f,
                static_cast<float> (yesdaw::ui::UiTheme::Layout::controlShadowOffset)),
            yesdaw::ui::UiTheme::Radius::panel);
        juce::ColourGradient masterGradient (
            yesdaw::ui::UiTheme::Color::panelInnerHighlight(),
            static_cast<float> (masterLane.getCentreX()),
            static_cast<float> (masterLane.getY()),
            yesdaw::ui::UiTheme::Color::panel(),
            static_cast<float> (masterLane.getCentreX()),
            static_cast<float> (masterLane.getBottom()),
            false);
        g.setGradientFill (masterGradient);
        g.fillRoundedRectangle (masterLane.toFloat(), yesdaw::ui::UiTheme::Radius::panel);
        g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight());
        g.drawRoundedRectangle (
            masterLane.toFloat().reduced (
                yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
            yesdaw::ui::UiTheme::Radius::panel,
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);

        g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
            yesdaw::ui::UiTheme::Tone::mixerHeaderAlpha));
        g.fillRect (masterLane.withHeight (
            yesdaw::ui::UiTheme::Layout::mixerPaintedHeaderHeight));
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::small,
            juce::Font::bold));
        g.drawText ("MASTER",
                    masterLane.withHeight (
                        yesdaw::ui::UiTheme::Layout::mixerPaintedHeaderHeight),
                    juce::Justification::centred,
                    false);

        auto masterContent = masterLane.reduced (
            yesdaw::ui::UiTheme::Layout::mixerMasterContentInsetX,
            yesdaw::ui::UiTheme::Space::none);
        masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterContentTop);

        auto loudnessCard = masterContent.removeFromTop (
            yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessCardHeight);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (loudnessCard.toFloat(), yesdaw::ui::UiTheme::Radius::md);
        g.setColour (kMutedText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::tiny,
            juce::Font::bold));
        g.drawText ("INTEGRATED", loudnessCard.withHeight (
                        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop),
                    juce::Justification::centred,
                    false);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::readout,
            juce::Font::bold));
        const juce::String integrated = surface.loudness.valid
            ? juce::String (surface.loudness.integratedLufs, 1)
            : juce::String ("--");
        g.drawText (integrated,
                    loudnessCard.withTrimmedTop (
                        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop)
                        .withHeight (
                            yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueHeight),
                    juce::Justification::centred,
                    false);
        g.setColour (kMutedText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
        g.drawText ("LUFS-I",
                    loudnessCard.withTrimmedTop (
                        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop
                        + yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueHeight)
                        .withHeight (
                            yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessUnitHeight),
                    juce::Justification::centred,
                    false);

        masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterSectionGap);
        auto peakCard = masterContent.removeFromTop (
            yesdaw::ui::UiTheme::Layout::mixerMasterPeakCardHeight);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (peakCard.toFloat(), yesdaw::ui::UiTheme::Radius::md);
        g.setColour (kMutedText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::tiny,
            juce::Font::bold));
        g.drawText ("TRUE PEAK",
                    peakCard.withHeight (
                        yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueTop),
                    juce::Justification::centred,
                    false);
        g.setColour (surface.loudness.valid && surface.loudness.truePeakDbtp > 0.0
                         ? yesdaw::ui::UiTheme::Color::dangerRed()
                         : kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::body,
            juce::Font::bold));
        const juce::String truePeak = surface.loudness.valid
            ? juce::String (surface.loudness.truePeakDbtp, 1) + " dBTP"
            : juce::String ("-- dBTP");
        g.drawText (truePeak,
                    peakCard.withTrimmedTop (
                        yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueTop)
                        .withHeight (
                            yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueHeight),
                    juce::Justification::centred,
                    false);

        const float masterPeakLeft = liveMasterPeakLeft.load (std::memory_order_acquire);
        const float masterPeakRight = liveMasterPeakRight.load (std::memory_order_acquire);

        masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterMeterTopGap);
        auto meterArea = masterContent.withTrimmedBottom (
            yesdaw::ui::UiTheme::Layout::mixerMasterMeterBottomInset);
        auto scale = meterArea.removeFromLeft (
            yesdaw::ui::UiTheme::Layout::mixerMasterScaleWidth);
        g.setColour (yesdaw::ui::UiTheme::Color::faintText());
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::tiny));
        for (std::size_t i = 0;
             i < yesdaw::ui::UiTheme::Layout::mixerMasterScaleDb.size();
             ++i)
        {
            const float fraction = static_cast<float> (i)
                                 / static_cast<float> (
                                       yesdaw::ui::UiTheme::Layout::mixerMasterScaleDb.size() - 1u);
            const int y = scale.getY()
                        + juce::roundToInt (fraction * static_cast<float> (
                              scale.getHeight()
                              - yesdaw::ui::UiTheme::Layout::mixerMasterScaleLabelHeight));
            g.drawText (juce::String (yesdaw::ui::UiTheme::Layout::mixerMasterScaleDb[i]),
                        juce::Rectangle<int> {
                            scale.getX(), y, scale.getWidth(),
                            yesdaw::ui::UiTheme::Layout::mixerMasterScaleLabelHeight },
                        juce::Justification::centredRight,
                        false);
        }

        const int meterPairWidth = 2 * yesdaw::ui::UiTheme::Layout::mixerMasterMeterWidth
                                 + yesdaw::ui::UiTheme::Layout::mixerMasterMeterGap;
        auto meterPair = meterArea.withWidth (meterPairWidth)
                             .withCentre ({ meterArea.getCentreX(), meterArea.getCentreY() });
        auto leftMeter = meterPair.removeFromLeft (
            yesdaw::ui::UiTheme::Layout::mixerMasterMeterWidth);
        meterPair.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerMasterMeterGap);
        auto rightMeter = meterPair.removeFromLeft (
            yesdaw::ui::UiTheme::Layout::mixerMasterMeterWidth);
        drawMeter (g, leftMeter, masterPeakLeft);
        drawMeter (g, rightMeter, masterPeakRight);
        g.setColour (kMutedText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
        auto channelLabels = masterLane.withTrimmedTop (
                                 masterLane.getHeight()
                                 - yesdaw::ui::UiTheme::Layout::mixerMasterMeterChannelLabelHeight)
                                 .reduced (
                                     yesdaw::ui::UiTheme::Layout::mixerMasterContentInsetX,
                                     yesdaw::ui::UiTheme::Space::none);
        g.drawText ("L     R", channelLabels, juce::Justification::centred, false);
    }

    [[nodiscard]] yesdaw::ui::UiMixerSurfaceSnapshot currentMixerSurface() const
    {
        if (appModel.context().projectLoaded)
            return yesdaw::ui::projectUiMixerSurface (appModel.project());

        return {};
    }

    [[nodiscard]] yesdaw::ui::UiPianoRollSurfaceSnapshot currentPianoRollSurface() const
    {
        if (appModel.context().projectLoaded)
        {
            yesdaw::engine::EntityId midiClipId = appModel.selectedMidiClipId();
            if (! midiClipId.isValid() && ! appModel.project().midiClips.empty())
                midiClipId = appModel.project().midiClips.front().id;

            return yesdaw::ui::projectUiPianoRollSurface (
                appModel.project(),
                midiClipId,
                appModel.selectedMidiNoteId(),
                appModel.selectedMidiNoteIds());
        }

        return {};
    }

    yesdaw::ui::YesDawLookAndFeel lookAndFeel;
    yesdaw::ui::UiAppModel appModel;
    yesdaw::ui::MainComponentFileChoices fileChoices;
    juce::AudioDeviceManager audioDeviceManager;
    TooltippedMenuBar menuBar;
    juce::TooltipWindow tooltipWindow { nullptr };   // native tooltip display (B40)
    juce::ComboBox audioDeviceChooser;
    std::vector<std::string> audioDeviceChooserNames;
    bool refreshingAudioDeviceChooser = false;
    const bool desktopAudioRequested = false;
    bool desktopAudioCallbackRegistered = false;
    int desktopAudioCallbackSuspendDepth = 0;
    bool resumeDesktopAudioAfterSuspend = false;
    std::atomic<bool> desktopAudioOpen { false };
    std::atomic<std::uint32_t> deviceAudioCallbackBlockCount { 0u };
    std::atomic<std::uint32_t> deviceAudioNonSilentBlockCount { 0u };
    std::atomic<float> liveMasterPeakLeft { 0.0f };
    std::atomic<float> liveMasterPeakRight { 0.0f };
    std::vector<TrackRow> projectTimelineTracks;
    std::vector<yesdaw::ui::Clip> timelineClips;
    std::vector<TimelineClipStyle> timelineClipStyles;
    std::vector<yesdaw::engine::EntityId> timelineClipIds;
    std::vector<yesdaw::engine::AssetContentHash> timelineClipAssetHashes;
    double timelineTotalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
    std::vector<std::string> timelineMarkerLabels;
    std::vector<yesdaw::ui::TimelineMarker> timelineMarkerViews;
    double timelineZoomFactor = 1.0;   // 1.0 == whole timeline fits the window
    mutable double timelineScrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
    // Vertical track scroll (E5): whole lane rows above the viewport, shared by the timeline
    // lanes and the track rail; geometry clamps it against the current lane count.
    int timelineTrackScrollRows = 0;
    TimelineInputComponent timelineInput;
    PianoRollInputComponent pianoRollInput;
    TrackListInputComponent trackListInput;
    MixerStripsInputComponent mixerStripsInput;
    FineDragSlider headerTempoControl;
    juce::ComboBox headerMeterChooser;
    juce::ComboBox mixerFxAddChooser;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerFxVisibleSlotCount> mixerFxSlotToggles;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerFxVisibleSlotCount> mixerFxSlotRemoves;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerFxVisibleSlotCount> mixerFxSlotEdits;
    std::array<FineDragSlider, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamSliders;
    std::array<juce::Label, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamLabels;
    std::array<std::uint32_t, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamSliderIds {};
    int selectedFxParamSlot = -1;
    bool refreshingFxParamControls = false;
    juce::TextButton mixerBusAddButton;
    juce::ComboBox mixerSendAddChooser;
    std::array<FineDragSlider, yesdaw::ui::UiTheme::Layout::mixerSendVisibleRowCount> mixerSendLevelSliders;
    std::array<juce::Label, yesdaw::ui::UiTheme::Layout::mixerSendVisibleRowCount> mixerSendLabels;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerSendVisibleRowCount> mixerSendRemoves;
    bool refreshingSendControls = false;
    std::size_t lastVisibleFxParamRows = 0;
    std::size_t lastVisibleSendRows = 0;
    std::size_t lastVisibleFxSlotRows = 0;
    juce::TextButton trackAddButton;
    juce::TextEditor trackRenameEditor;
    juce::TextEditor clipRenameEditor;
    int selectedTrackLane = -1;
    juce::TextButton exportAudioButton;
    juce::ComboBox exportBitDepthChooser;
    juce::ComboBox exportRangeChooser;
    juce::Label exportAudioProgress;
    juce::TextButton exportAudioCancelButton;
    juce::TextButton mixerTrackSelect;
    FineDragSlider mixerFader;
    FineDragSlider mixerPan;
    juce::Label dragDbReadout;
    std::vector<MeterHoldState> trackMeterHold;   // by Track index; advanced per UI tick (B32)
    juce::String lastPushedWindowTitle;           // dirty-title push dedupe (B38)
    juce::TextButton mixerMetersReadout;
    juce::TextButton mixerSendsReadout;
    juce::TextButton mixerSendLevelEdit;
    juce::TextButton mixerFxSlotsReadout;
    juce::TextButton mixerGainReductionReadout;
    juce::TextButton mixerBusFxSlotsReadout;
    juce::TextButton mixerFxSlotToggle;
    juce::ToggleButton mixerMute;
    juce::ToggleButton mixerSolo;
    juce::TextButton masterLoudnessReadout;
    juce::TextButton autosaveRestoreButton;
    juce::TextButton autosaveDiscardButton;
    juce::ComboBox timelineSnapChooser;
    juce::ComboBox timelineRepeatPasteChooser;
    AutomationLaneCanvasComponent automationLaneCanvas;
    juce::TextButton automationLaneToggle;
    juce::Label automationLaneRow;
    juce::TextButton automationBreakpointAddButton;
    juce::TextButton automationBreakpointDeleteButton;
    FineDragSlider inspectorStart;
    FineDragSlider inspectorEnd;
    FineDragSlider inspectorLength;
    FineDragSlider inspectorGain;
    FineDragSlider inspectorFadeIn;
    FineDragSlider inspectorFadeOut;
    juce::ComboBox inspectorFadeCurve;
    std::array<ToolbarActionButton, yesdaw::ui::kMainShellToolbarActions.size()> buttons;
    bool refreshingInspectorControls = false;
    bool refreshingTimeMapControls = false;
    bool refreshingSnapChooser = false;
    bool refreshingRepeatPasteChooser = false;
    bool refreshingMixerControls = false;
    int autosaveElapsedMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

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

const juce::Component* findChildWithComponentId (const juce::Component& component, const juce::String& componentId)
{
    if (component.getComponentID() == componentId)
        return &component;

    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (const juce::Component* child = component.getChildComponent (i))
            if (const juce::Component* found = findChildWithComponentId (*child, componentId))
                return found;

    return nullptr;
}

juce::String stableIdForAction (yesdaw::ui::UiActionId action)
{
    const yesdaw::ui::UiActionRegistry registry;
    if (const yesdaw::ui::UiActionDescriptor* descriptor = registry.descriptor (action))
        return descriptor->stableId;

    return {};
}

std::filesystem::path pathFromJuceFile (const juce::File& file)
{
    const std::string utf8 = file.getFullPathName().toStdString();
    const auto* begin = reinterpret_cast<const char8_t*> (utf8.data());
    return std::filesystem::path (std::u8string (begin, begin + utf8.size()));
}

std::filesystem::path withExtension (std::filesystem::path path, const std::filesystem::path& extension)
{
    if (path.extension() != extension)
        path += extension;

    return path;
}

yesdaw::ui::MainComponentFileChoices makeNativeFileChoices()
{
    yesdaw::ui::MainComponentFileChoices choices;

    choices.chooseNewProjectBundle = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Create YES DAW Project",
                                   documents.getChildFile ("Untitled.yesdaw"),
                                   "*.yesdaw",
                                   true);
        if (! chooser.browseForFileToSave (true))
            return std::filesystem::path {};

        return withExtension (pathFromJuceFile (chooser.getResult()), ".yesdaw");
    };

    choices.chooseOpenProjectBundle = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Open YES DAW Project Folder", documents, {}, true);
        if (! chooser.browseForDirectory())
            return std::filesystem::path {};

        return pathFromJuceFile (chooser.getResult());
    };

    choices.chooseSaveAsProjectBundle = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Save YES DAW Project As",
                                   documents.getChildFile ("Untitled.yesdaw"),
                                   "*.yesdaw",
                                   true);
        if (! chooser.browseForFileToSave (true))
            return std::filesystem::path {};

        return withExtension (pathFromJuceFile (chooser.getResult()), ".yesdaw");
    };

    choices.chooseImportAudioFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Import WAV Audio", documents, "*.wav;*.wave", true);
        if (! chooser.browseForFileToOpen())
            return std::filesystem::path {};

        return pathFromJuceFile (chooser.getResult());
    };

    choices.chooseExportAudioFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Export YES DAW Mix",
                                   documents.getChildFile ("YES DAW Mix.wav"),
                                   "*.wav",
                                   true);
        if (! chooser.browseForFileToSave (true))
            return std::filesystem::path {};

        return withExtension (pathFromJuceFile (chooser.getResult()), ".wav");
    };

    return choices;
}

namespace yesdaw::ui {

std::unique_ptr<juce::Component> createMainComponent()
{
    return std::make_unique<MainComponent> (makeNativeFileChoices(), true);
}

std::unique_ptr<juce::Component> createMainComponent (MainComponentFileChoices fileChoices)
{
    return std::make_unique<MainComponent> (std::move (fileChoices), false);
}

MainComponentSnapshot snapshotMainComponent (const juce::Component& component)
{
    MainComponentSnapshot snapshot;
    snapshot.width = component.getWidth();
    snapshot.height = component.getHeight();
    snapshot.childCount = component.getNumChildComponents();

    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
    {
        snapshot.isMainComponent = true;
        snapshot.windowTitle = mainComponent->computedWindowTitle().toStdString();
        snapshot.primaryFileChoicesReady = mainComponent->harnessPrimaryFileChoicesReady();
        snapshot.desktopAudioRequested = mainComponent->harnessDesktopAudioRequested();
        snapshot.desktopAudioOpen = mainComponent->harnessDesktopAudioOpen();
        snapshot.deviceAudioCallbackBlockCount = mainComponent->harnessDeviceAudioCallbackBlockCount();
        snapshot.deviceAudioNonSilentBlockCount = mainComponent->harnessDeviceAudioNonSilentBlockCount();
        snapshot.playbackReady = mainComponent->harnessPlaybackReady();
        snapshot.playbackLoopStartFrame = mainComponent->harnessPlaybackLoopStartFrame();
        snapshot.playbackLoopEndFrame = mainComponent->harnessPlaybackLoopEndFrame();
        snapshot.timelineRangeStartFrame = mainComponent->harnessTimelineRangeStartFrame();
        snapshot.timelineRangeEndFrame = mainComponent->harnessTimelineRangeEndFrame();
        snapshot.timelineZoomFactor = mainComponent->harnessTimelineZoomFactor();
        snapshot.timelineScrollSeconds = mainComponent->harnessTimelineScrollSeconds();
        snapshot.timelineTrackScrollRows = mainComponent->harnessTimelineTrackScrollRows();
        snapshot.timelineMaxTrackScrollRows = mainComponent->harnessTimelineMaxTrackScrollRows();
        snapshot.visibleTimelineTrackCount = mainComponent->harnessVisibleTimelineTrackCount();
        snapshot.visibleTimelineClipCount = mainComponent->harnessVisibleTimelineClipCount();
        snapshot.visibleFirstTimelineClipName = mainComponent->harnessVisibleFirstTimelineClipName();
        snapshot.selectedTimelineClipCount = mainComponent->harnessSelectedTimelineClipCount();
        snapshot.visibleTimelineTotalSeconds = mainComponent->harnessVisibleTimelineTotalSeconds();
        snapshot.visibleMixerTrackCount = mainComponent->harnessVisibleMixerTrackCount();
        snapshot.visibleMixerBusCount = mainComponent->harnessVisibleMixerBusCount();
        snapshot.visibleMixerLoudnessValid = mainComponent->harnessVisibleMixerLoudnessValid();
        snapshot.visibleMasterPeakLeft = mainComponent->harnessVisibleMasterPeakLeft();
        snapshot.visibleMasterPeakRight = mainComponent->harnessVisibleMasterPeakRight();
        snapshot.visiblePianoRollNoteCount = mainComponent->harnessVisiblePianoRollNoteCount();
        snapshot.bundlePath = mainComponent->harnessBundlePath();
        snapshot.context = mainComponent->harnessContext();
        snapshot.recordingDevice = mainComponent->harnessRecordingDevice();
        snapshot.recordingTrackInput = mainComponent->harnessRecordingTrackInput();
        snapshot.lastRecordedAudioTake = mainComponent->harnessLastRecordedAudioTake();
        snapshot.lastRecordedMidiTake = mainComponent->harnessLastRecordedMidiTake();
        snapshot.recordingComp = mainComponent->harnessRecordingComp();
        snapshot.autosaveRecovery = mainComponent->harnessAutosaveRecovery();
    }

    return snapshot;
}

std::vector<float> renderMainComponentPlayback (juce::Component& component,
                                                std::uint64_t frames,
                                                int blockSize)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRenderPlaybackFrames (frames, blockSize);

    return {};
}

bool serviceMainComponentUiTimer (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        mainComponent->timerCallback();
        return true;
    }

    return false;
}

bool mainComponentConfirmsClose (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->confirmClose();

    return true;
}

bool processMainComponentDeviceAudioBlock (juce::Component& component,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessProcessDeviceAudioBlock (outputChannels, numOutputChannels, numFrames);

    return false;
}

juce::Component* findMainComponentChildForAction (juce::Component& component, UiActionId action)
{
    const juce::String stableId = stableIdForAction (action);
    if (stableId.isEmpty())
        return nullptr;

    return findChildWithComponentId (component, stableId);
}

const juce::Component* findMainComponentChildForAction (const juce::Component& component, UiActionId action)
{
    const juce::String stableId = stableIdForAction (action);
    if (stableId.isEmpty())
        return nullptr;

    return findChildWithComponentId (component, stableId);
}

} // namespace yesdaw::ui
