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
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kHeaderHeight = yesdaw::ui::UiTheme::Layout::headerHeight;
constexpr int kLeftRailWidth = yesdaw::ui::UiTheme::Layout::leftRailWidth;
constexpr int kInspectorWidth = yesdaw::ui::UiTheme::Layout::inspectorWidth;
constexpr int kMixerHeight = yesdaw::ui::UiTheme::Layout::mixerHeight;
constexpr int kUiRefreshIntervalMs = 33;
constexpr yesdaw::engine::Tick kTimelineSnapGridTicks =
    yesdaw::ui::UiTheme::Layout::timelineSnapGridTicks;
constexpr yesdaw::engine::Tick kPianoRollSnapGridTicks =
    yesdaw::ui::UiTheme::Layout::pianoRollGridTickStep;
constexpr const char* kTimelineComponentId = "timeline.canvas";
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
    if (mods.isCtrlDown() || mods.isCommandDown())
        chord += "Ctrl+";
    if (mods.isAltDown())
        chord += "Alt+";
    if (mods.isShiftDown())
        chord += "Shift+";

    const int code = key.getKeyCode();
    if (code == juce::KeyPress::spaceKey)
        chord += "Space";
    else if (code == juce::KeyPress::homeKey)
        chord += "Home";
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

class TimelineInputComponent final : public juce::Component
{
public:
    std::function<yesdaw::ui::TimelineCanvasState()> stateProvider;
    std::function<void (int)> onClipClicked;
    std::function<void()> onEmptyClicked;
    std::function<void (int, double, bool)> onClipMoved;
    std::function<void (int, double)> onClipSplit;
    std::function<void (int, double)> onClipTrimmedRight;
    std::function<void (int, int)> onClipGainAdjusted;
    std::function<void (int, bool, double)> onClipFadeAdjusted;
    std::function<void (double)> onTimelineLocated;

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
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        playheadLocateActive = false;
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, event.getPosition());

        if (hit.hit)
        {
            if (onClipClicked)
                onClipClicked (hit.id);

            dragState = {};
            dragState.active = true;
            dragState.layoutClipId = hit.id;
            dragState.downPosition = event.getPosition();
            dragState.mode = dragModeForPointer (state, getLocalBounds(), hit.id, event.getPosition(), event.mods);
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
            playheadLocateActive = true;
            if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                if (onTimelineLocated)
                    onTimelineLocated (*seconds);
            return;
        }

        playheadLocateActive = false;
        if (onEmptyClicked)
            onEmptyClicked();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (playheadLocateActive && stateProvider)
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                if (onTimelineLocated)
                    onTimelineLocated (*seconds);
            return;
        }

        if (dragState.active)
            dragState.moved = true;
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (playheadLocateActive)
        {
            playheadLocateActive = false;
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
                    onClipTrimmedRight (drag.layoutClipId, *eventSeconds);
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

        if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
            return;

        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double nextStartSeconds = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinateSecondsFloor,
            drag.startSeconds + static_cast<double> (deltaX) / pixelsPerSecond);

        if (onClipMoved)
            onClipMoved (drag.layoutClipId, nextStartSeconds, drag.mode == TimelineDragMode::SnapMove);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, event.getPosition());
        if (! hit.hit)
            return;

        if (onClipClicked)
            onClipClicked (hit.id);

        if (const std::optional<double> splitSeconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
            if (onClipSplit)
                onClipSplit (hit.id, *splitSeconds);
    }

private:
    enum class TimelineDragMode
    {
        Move,
        SnapMove,
        TrimRight,
        Gain,
        FadeIn,
        FadeOut
    };

    struct TimelineDragState
    {
        bool active = false;
        bool moved = false;
        int layoutClipId = -1;
        double startSeconds = 0.0;
        double lengthSeconds = 0.0;
        TimelineDragMode mode = TimelineDragMode::Move;
        juce::Point<int> downPosition;
    };

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

        if (modifiers.isShiftDown())
            return TimelineDragMode::Gain;

        if (modifiers.isCtrlDown())
            return TimelineDragMode::SnapMove;

        return TimelineDragMode::Move;
    }

    TimelineDragState dragState;
    bool playheadLocateActive = false;
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

class PianoRollInputComponent final : public juce::Component
{
public:
    std::function<yesdaw::ui::UiPianoRollSurfaceSnapshot()> stateProvider;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onNoteClicked;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteMoved;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteLengthChanged;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, std::int32_t)> onNoteTransposed;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteQuantized;
    std::function<void()> onExpressionRead;

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const auto hit = noteAt (surface, event.getPosition());
        if (! hit)
        {
            dragState = {};
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
        if (nextStart != drag.startTick && onNoteMoved)
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

class MainComponent : public juce::Component,
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

        exportAudioProgress.setComponentID (kExportAudioProgressComponentId);
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
        timelineInput.setName ("Timeline");
        timelineInput.setTitle ("Timeline");
        timelineInput.stateProvider = [this] { return makeTimelineState(); };
        timelineInput.onClipClicked = [this] (int timelineClipId) {
            selectTimelineClipByLayoutId (timelineClipId);
        };
        timelineInput.onEmptyClicked = [this] {
            appModel.clearTimelineClipSelection();
            refreshActionState();
            repaint();
        };
        timelineInput.onClipMoved = [this] (int timelineClipId, double startSeconds, bool snapToGrid) {
            moveTimelineClipByLayoutId (timelineClipId, startSeconds, snapToGrid);
        };
        timelineInput.onClipSplit = [this] (int timelineClipId, double splitSeconds) {
            splitTimelineClipByLayoutId (timelineClipId, splitSeconds);
        };
        timelineInput.onClipTrimmedRight = [this] (int timelineClipId, double endSeconds) {
            trimTimelineClipRightByLayoutId (timelineClipId, endSeconds);
        };
        timelineInput.onClipGainAdjusted = [this] (int timelineClipId, int deltaPixels) {
            adjustTimelineClipGainByLayoutId (timelineClipId, deltaPixels);
        };
        timelineInput.onClipFadeAdjusted = [this] (int timelineClipId, bool fadeIn, double fadeSeconds) {
            adjustTimelineClipFadeByLayoutId (timelineClipId, fadeIn, fadeSeconds);
        };
        timelineInput.onTimelineLocated = [this] (double seconds) {
            if (const std::optional<yesdaw::engine::Tick> frame = timelineTickFromSeconds (seconds))
            {
                (void) appModel.locatePlaybackFrame (*frame);
                refreshActionState();
                repaint();
            }
        };
        addAndMakeVisible (timelineInput);

        configureAutomationLaneControls();

        pianoRollInput.setComponentID (kPianoRollComponentId);
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
        pianoRollInput.onExpressionRead = [this] {
            (void) appModel.readPianoRollExpressionLanes();
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (pianoRollInput);

        configureInspectorControls();
        configureMixerControls();
        resized();
        refreshActionState();

        if (desktopAudioRequested)
        {
            const juce::String error = audioDeviceManager.initialiseWithDefaultDevices (0, 2);
            if (error.isEmpty())
            {
                if (juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
                    appModel.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
                audioDeviceManager.addAudioCallback (this);
                desktopAudioCallbackRegistered = true;
                desktopAudioOpen.store (true, std::memory_order_release);
            }
        }

        // H17 CP4: scheduled autosave is ON by default (policy lives in the headless app model, so the
        // default is covered by a headless test). The Timer fires on the message thread — which is this
        // app's control thread — so writeAutosaveTick()'s heavy SQLite/asset I/O is on the right thread.
        startTimer (kUiRefreshIntervalMs);
    }

    ~MainComponent() override
    {
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
        refreshActionState();
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

    void audioDeviceIOCallbackWithContext (const float* const*,
                                           int,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        (void) processDeviceAudioBlock (outputChannels, numOutputChannels, numFrames);
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
        return processed;
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
    [[nodiscard]] int harnessVisibleTimelineTrackCount() const
    {
        return appModel.context().projectLoaded ? static_cast<int> (projectTimelineTracks.size()) : 0;
    }
    [[nodiscard]] int harnessVisibleTimelineClipCount() const
    {
        return appModel.context().projectLoaded ? static_cast<int> (timelineClips.size()) : 0;
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
        exportAudioButton.setBounds (yesdaw::ui::UiTheme::Layout::projectExportAudioButtonBounds());
        exportAudioProgress.setBounds (yesdaw::ui::UiTheme::Layout::projectExportAudioProgressBounds());
        exportAudioCancelButton.setBounds (yesdaw::ui::UiTheme::Layout::projectExportAudioCancelButtonBounds());
        masterLoudnessReadout.setBounds (juce::Rectangle<int> (yesdaw::ui::UiTheme::Layout::headerMasterLufsX,
                                                               yesdaw::ui::UiTheme::Layout::headerMasterLufsY,
                                                               yesdaw::ui::UiTheme::Layout::headerMasterLufsWidth,
                                                               yesdaw::ui::UiTheme::Layout::headerMasterLufsHeight));
        timelineInput.setBounds (timelineBounds());
        pianoRollInput.setBounds (timelineBounds());
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
            component.setTooltip (juce::String (descriptor->stableId) + "  " + descriptor->defaultKey);
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
        mixerTrackSelect.setTooltip ("mixer.track.0.select");
        mixerTrackSelect.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerTrackSelect.setColour (juce::TextButton::textColourOffId, kText);
        mixerTrackSelect.onClick = [this] {
            (void) appModel.selectMixerTrack (0);
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerTrackSelect);

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
        mixerFader.onValueChange = [this] {
            if (refreshingMixerControls || ! mixerFader.isEnabled())
                return;

            (void) appModel.setSelectedMixerFader (static_cast<float> (mixerFader.getValue()));
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerFader);

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
        mixerPan.onValueChange = [this] {
            if (refreshingMixerControls || ! mixerPan.isEnabled())
                return;

            (void) appModel.setSelectedMixerPan (static_cast<float> (mixerPan.getValue()));
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

    [[nodiscard]] juce::Rectangle<int> mixerPanelBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (kHeaderHeight);
        auto mixer = appModel.context().activePanel == yesdaw::ui::UiPanel::Mixer
                         ? work
                         : work.removeFromBottom (kMixerHeight);
        return mixer.reduced (yesdaw::ui::UiTheme::Layout::mixerPanelHorizontalInset,
                              yesdaw::ui::UiTheme::Layout::mixerPanelVerticalInset);
    }

    [[nodiscard]] juce::Rectangle<int> mixerFirstStripBounds() const
    {
        auto mixer = mixerPanelBounds();
        mixer.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerToolsWidth);

        const auto surface = currentMixerSurface();
        const int stripCount = juce::jmax (1, static_cast<int> (surface.tracks.size() + surface.buses.size()));
        const int stripWidth = juce::jmax (yesdaw::ui::UiTheme::Layout::mixerStripMinWidth,
                                           mixer.getWidth() / (stripCount + 1));
        return mixer.withWidth (stripWidth)
            .reduced (yesdaw::ui::UiTheme::Layout::mixerStripHorizontalInset,
                      yesdaw::ui::UiTheme::Layout::mixerStripVerticalInset);
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

        auto lane = mixerFirstStripBounds()
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

    void handleAction (yesdaw::ui::UiActionId action)
    {
        suspendDesktopAudioCallback();
        handleActionWhileAudioStopped (action);
        resumeDesktopAudioCallback();
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
                    {
                        if (auto decodedAssets = decodeStoredProjectAssets (path); decodedAssets && ! decodedAssets->empty())
                            (void) appModel.loadProjectBundle (
                                path,
                                std::span<const yesdaw::ui::UiDecodedAsset> (
                                    decodedAssets->data(), decodedAssets->size()));
                        else if (decodedAssets)
                            (void) appModel.openProjectBundle (path);
                    }
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
                        if (auto decoded = decodeProjectWav (path))
                            (void) appModel.importAudioFile (path, std::move (*decoded));
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
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto action = toolbarActions[i];
            const bool arrangementVisible = appModel.context().activePanel != yesdaw::ui::UiPanel::Mixer;
            const bool arrangementOnlyAction = action == yesdaw::ui::UiActionId::DeviceRefreshAudio
                                            || action == yesdaw::ui::UiActionId::DeviceSelectTestAudio
                                            || action == yesdaw::ui::UiActionId::RecordingArmTrack
                                            || action == yesdaw::ui::UiActionId::RecordingSetMonitoringPolicy
                                            || action == yesdaw::ui::UiActionId::RecordingAssembleComp;
            buttons[i].setVisible (! arrangementOnlyAction || arrangementVisible);
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
            appModel.registry().stateFor (yesdaw::ui::UiActionId::ProjectExportAudioCancel,
                                          appModel.context()).enabled);
        exportAudioProgress.setText (exportAudioProgressText(), juce::dontSendNotification);
        masterLoudnessReadout.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadLoudness,
                                          appModel.context()).enabled);
        masterLoudnessReadout.setButtonText (masterLoudnessReadoutText());
        timelineInput.setVisible (appModel.context().activePanel == yesdaw::ui::UiPanel::Timeline);
        pianoRollInput.setVisible (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll);
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

        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::body));
        int menuX = yesdaw::ui::UiTheme::Layout::headerMenuStartX;
        for (const auto* menu : { "FILE", "EDIT", "VIEW", "OPTIONS", "HELP" })
        {
            g.drawText (menu,
                        menuX,
                        yesdaw::ui::UiTheme::Layout::headerMenuY,
                        yesdaw::ui::UiTheme::Layout::headerMenuWidth,
                        yesdaw::ui::UiTheme::Layout::headerMenuHeight,
                        juce::Justification::centredLeft,
                        false);
            menuX += menu == std::string ("OPTIONS")
                         ? yesdaw::ui::UiTheme::Layout::headerOptionsMenuStep
                         : yesdaw::ui::UiTheme::Layout::headerMenuStep;
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
        for (std::size_t i = 0; i < appModel.project().tracks.size(); ++i)
        {
            auto row = area.removeFromTop (rowHeight);
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
                i == 3 ? yesdaw::ui::UiTheme::Color::selectedLane()
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
            g.setColour (trackColour);
            g.drawLine (static_cast<float> (pan.getCentreX()),
                        static_cast<float> (pan.getY()
                                            + yesdaw::ui::UiTheme::Layout::trackListPanIndicatorInset),
                        static_cast<float> (pan.getCentreX()),
                        static_cast<float> (pan.getCentreY()),
                        yesdaw::ui::UiTheme::Layout::iconBoldStrokeWidth);
            g.setColour (kMutedText);
            g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
                yesdaw::ui::UiTheme::Type::tiny));
            g.drawText ("C",
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

            auto buttonsArea = row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                                   .withTrimmedTop (yesdaw::ui::UiTheme::Layout::trackListButtonsTop)
                                   .withHeight (yesdaw::ui::UiTheme::Layout::trackListButtonsHeight);
            for (const auto* label : { "M", "S", "O" })
            {
                auto cell = buttonsArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::trackListButtonWidth)
                                .reduced (yesdaw::ui::UiTheme::Layout::trackListButtonInsetX,
                                          yesdaw::ui::UiTheme::Layout::trackListButtonInsetY);
                g.setColour (yesdaw::ui::UiTheme::Color::mixerBack());
                g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                g.setColour (label == std::string ("O") ? kRed : kMutedText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (
                    yesdaw::ui::UiTheme::Type::caption,
                    juce::Font::bold));
                g.drawText (label, cell, juce::Justification::centred, false);
            }

            auto meter = row.withRight (row.getRight() - yesdaw::ui::UiTheme::Layout::trackListMeterRightInset)
                             .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListMeterWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::trackListMeterHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::trackListMeterVerticalInset);
            drawMeter (g, meter, 0.0f);
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

        state.markers = nullptr;
        state.markerCount = 0;
        state.viewport.scrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
        state.viewport.pixelsPerSecond = static_cast<double> (juce::jmax (
                                           yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                           timelineInput.getWidth()
                                               - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                      / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                  state.totalSeconds);
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
            timelineClips.push_back ({ id, lane, startSeconds, lengthSeconds });
            timelineClipStyles.push_back ({ kPurple, yesdaw::ui::UiTheme::Tone::mainComponentProjectClipAlpha });
            timelineClipIds.push_back (clip.id);
            timelineClipAssetHashes.push_back (asset->contentHash);
            endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
        }

        timelineTotalSeconds = timelineClips.empty()
            ? yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds
            : std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                        endSeconds * yesdaw::ui::UiTheme::Layout::timelineProjectEndPaddingScale);
    }

    void selectTimelineClipByLayoutId (int layoutClipId)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        {
            appModel.clearTimelineClipSelection();
        }
        else
        {
            (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        }

        refreshActionState();
        repaint();
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

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        if (const auto tick = timelineTickFromSeconds (startSeconds))
        {
            yesdaw::engine::Tick moveTick = *tick;
            if (snapToGrid)
            {
                yesdaw::engine::Tick snapped = 0;
                if (! yesdaw::engine::snapTick (moveTick, yesdaw::engine::SnapGrid { kTimelineSnapGridTicks }, snapped))
                    return;

                moveTick = std::max<yesdaw::engine::Tick> (0, snapped);
            }

            (void) appModel.moveSelectedTimelineClipTo (moveTick);
        }

        refreshActionState();
        repaint();
    }

    void splitTimelineClipByLayoutId (int layoutClipId, double splitSeconds)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        if (const auto tick = timelineTickFromSeconds (splitSeconds))
            (void) appModel.splitSelectedTimelineClipAt (*tick);

        refreshActionState();
        repaint();
    }

    void trimTimelineClipRightByLayoutId (int layoutClipId, double endSeconds)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        if (const auto tick = timelineTickFromSeconds (endSeconds))
            (void) appModel.trimSelectedTimelineClipRightTo (*tick);

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
            g.setColour (note.selected ? kPurple.brighter (0.35f) : kCyan);
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
        g.drawText ("Audio Clip",
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
            const bool selected = appModel.context().mixerTargetSelected && stripIndex == 0;
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
            auto meter = faderArea.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetX,
                                       yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetY);
            drawMeter (g, meter, state.meter.valid ? state.meter.peakLeft : 0.0f);

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
                appModel.selectedMidiNoteId());
        }

        return {};
    }

    yesdaw::ui::YesDawLookAndFeel lookAndFeel;
    yesdaw::ui::UiAppModel appModel;
    yesdaw::ui::MainComponentFileChoices fileChoices;
    juce::AudioDeviceManager audioDeviceManager;
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
    TimelineInputComponent timelineInput;
    PianoRollInputComponent pianoRollInput;
    juce::TextButton exportAudioButton;
    juce::Label exportAudioProgress;
    juce::TextButton exportAudioCancelButton;
    juce::TextButton mixerTrackSelect;
    juce::Slider mixerFader;
    juce::Slider mixerPan;
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
    juce::TextButton automationLaneToggle;
    juce::Label automationLaneRow;
    juce::TextButton automationBreakpointAddButton;
    juce::TextButton automationBreakpointDeleteButton;
    juce::Slider inspectorStart;
    juce::Slider inspectorEnd;
    juce::Slider inspectorLength;
    juce::Slider inspectorGain;
    juce::Slider inspectorFadeIn;
    juce::Slider inspectorFadeOut;
    juce::ComboBox inspectorFadeCurve;
    std::array<ToolbarActionButton, yesdaw::ui::kMainShellToolbarActions.size()> buttons;
    bool refreshingInspectorControls = false;
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
        snapshot.primaryFileChoicesReady = mainComponent->harnessPrimaryFileChoicesReady();
        snapshot.desktopAudioRequested = mainComponent->harnessDesktopAudioRequested();
        snapshot.desktopAudioOpen = mainComponent->harnessDesktopAudioOpen();
        snapshot.deviceAudioCallbackBlockCount = mainComponent->harnessDeviceAudioCallbackBlockCount();
        snapshot.deviceAudioNonSilentBlockCount = mainComponent->harnessDeviceAudioNonSilentBlockCount();
        snapshot.playbackReady = mainComponent->harnessPlaybackReady();
        snapshot.visibleTimelineTrackCount = mainComponent->harnessVisibleTimelineTrackCount();
        snapshot.visibleTimelineClipCount = mainComponent->harnessVisibleTimelineClipCount();
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
