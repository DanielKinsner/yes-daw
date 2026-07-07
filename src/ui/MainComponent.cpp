// YES DAW - H11 app shell.
//
// The visible JUCE shell and the headless tests share ui/UiActions.h. This checkpoint keeps the shell
// image-light and model-backed: later H11 slices wire Project loading, transport, timeline drawing,
// accessibility traversal, and editing through the same action IDs.

#include "ui/MainComponent.h"
#include "engine/Time.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiAppModel.h"
#include "ui/UiMixerSurface.h"
#include "ui/UiPianoRollSurface.h"
#include "ui/UiTheme.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
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
constexpr yesdaw::engine::Tick kTimelineSnapGridTicks = 512;
constexpr yesdaw::engine::Tick kPianoRollSnapGridTicks =
    yesdaw::ui::UiTheme::Layout::pianoRollGridTickStep;
constexpr const char* kTimelineComponentId = "timeline.canvas";
constexpr const char* kPianoRollComponentId = "piano-roll.canvas";
constexpr const char* kInspectorFadeInComponentId = "clip.inspector.fade_in";
constexpr const char* kInspectorFadeOutComponentId = "clip.inspector.fade_out";
constexpr double kMaxInspectorFadeSeconds = 1.0;

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

constexpr yesdaw::engine::EntityId demoEntityId (std::uint8_t low) noexcept
{
    return yesdaw::engine::EntityId::fromBigEndianParts (0, low);
}

constexpr bool isBlackMidiKey (int key) noexcept
{
    const int octaveKey = key % 12;
    return octaveKey == 1 || octaveKey == 3 || octaveKey == 6 || octaveKey == 8 || octaveKey == 10;
}

struct MixerStrip
{
    const char* name;
    juce::Colour colour;
    float fader;
    float meter;
    bool selected;
};

const std::array<TrackRow, 8> kTracks {{
    { "Drums", kBlue, 0.86f },
    { "Bass DI", kTeal, 0.72f },
    { "Acoustic GTR", kAmber, 0.68f },
    { "Vocal Lead", kPurple, 0.82f },
    { "Vocal Double", kPurple.darker (0.15f), 0.58f },
    { "Keys", kCyan, 0.70f },
    { "Ambience", kBlue.darker (0.2f), 0.76f },
    { "FX Risers", kPurple.darker (0.35f), 0.48f }
}};

const std::array<yesdaw::ui::Clip, 23> kClips {{
    { 0, 0, 0.0, 17.0 }, { 1, 0, 17.0, 10.0 }, { 2, 0, 27.0, 17.0 },
    { 3, 0, 48.0, 17.0 }, { 4, 0, 65.0, 12.0 }, { 5, 0, 77.0, 14.0 },
    { 6, 1, 4.0, 18.0 }, { 7, 1, 22.0, 20.0 }, { 8, 1, 47.0, 30.0 },
    { 9, 2, 3.0, 18.0 }, { 10, 2, 25.0, 18.0 }, { 11, 2, 41.0, 26.0 },
    { 12, 2, 69.0, 16.0 },
    { 13, 3, 11.0, 28.0 }, { 14, 3, 39.0, 18.0 }, { 15, 3, 57.0, 30.0 },
    { 16, 4, 11.0, 24.0 }, { 17, 4, 37.0, 22.0 }, { 18, 4, 59.0, 28.0 },
    { 19, 5, 7.0, 26.0 }, { 20, 5, 33.0, 22.0 }, { 21, 5, 65.0, 24.0 },
    { 22, 6, 0.0, 38.0 }
}};

const std::array<TimelineClipStyle, 23> kClipStyles {{
    { kBlue, 0.82f }, { kBlue, 0.78f }, { kBlue, 0.80f },
    { kBlue, 0.70f }, { kBlue, 0.76f }, { kBlue, 0.85f },
    { kTeal, 0.72f }, { kTeal, 0.75f }, { kTeal, 0.70f },
    { kAmber, 0.64f }, { kAmber, 0.67f }, { kAmber, 0.70f },
    { kAmber, 0.62f },
    { kPurple, 0.88f }, { kPurple, 0.90f }, { kPurple, 0.86f },
    { kPurple.darker (0.2f), 0.62f }, { kPurple.darker (0.2f), 0.66f },
    { kPurple.darker (0.2f), 0.58f },
    { kCyan, 0.50f }, { kCyan, 0.52f }, { kCyan, 0.48f },
    { kBlue.darker (0.35f), 0.68f }
}};

const std::array<yesdaw::ui::TimelineMarker, 5> kTimelineMarkers {{
    { 8.0, "Intro" },
    { 24.0, "Verse" },
    { 32.0, "Chorus" },
    { 64.0, "Bridge" },
    { 80.0, "Outro" }
}};

const std::array<MixerStrip, 11> kMixer {{
    { "Drums", kBlue, 0.64f, 0.86f, false },
    { "Bass DI", kTeal, 0.58f, 0.70f, false },
    { "Acoustic GTR", kAmber, 0.54f, 0.63f, false },
    { "Elec GTR", kAmber.darker (0.25f), 0.52f, 0.66f, false },
    { "Vocal Lead", kPurple, 0.66f, 0.84f, true },
    { "Vocal Double", kPurple.darker (0.2f), 0.60f, 0.61f, false },
    { "Keys", kCyan, 0.50f, 0.68f, false },
    { "Ambience", kBlue.darker (0.2f), 0.42f, 0.73f, false },
    { "FX Risers", kPurple.darker (0.35f), 0.48f, 0.44f, false },
    { "Room Verb", kPurple.darker (0.15f), 0.55f, 0.57f, false },
    { "Delay", kBlue.darker (0.3f), 0.50f, 0.52f, false }
}};

yesdaw::ui::UiMixerSurfaceSnapshot makeDemoMixerSurface()
{
    yesdaw::ui::UiMixerSurfaceSnapshot surface;
    surface.projectLoaded = true;
    surface.loudness = yesdaw::ui::UiMixerLoudnessReadout { -7.2, -9.4, -8.8, 5.0, -1.0, true };

    for (std::size_t i = 0; i < kMixer.size(); ++i)
    {
        const bool isBus = i >= 9;
        const auto& source = kMixer[i];

        yesdaw::ui::UiMixerStrip strip;
        strip.kind = isBus ? yesdaw::ui::UiMixerTargetKind::Bus : yesdaw::ui::UiMixerTargetKind::Track;
        strip.index = isBus ? i - 9u : i;
        strip.name = source.name;
        strip.linearGain = source.fader;
        strip.pan = source.selected ? -0.08f : 0.0f;
        strip.muted = false;
        strip.soloed = source.selected;
        strip.soloSafe = isBus;
        strip.sidechainVisible = i == 1 || isBus;
        strip.meter = yesdaw::ui::UiMixerMeterReadout { source.meter, source.meter * 0.92f,
                                                        source.meter * 0.58f, source.meter * 0.52f, true };

        if (isBus)
            surface.buses.push_back (std::move (strip));
        else
            surface.tracks.push_back (std::move (strip));
    }

    return surface;
}

yesdaw::ui::UiPianoRollSurfaceSnapshot makeDemoPianoRollSurface()
{
    yesdaw::ui::UiPianoRollSurfaceSnapshot surface;
    surface.projectLoaded = true;
    surface.midiClipSelected = true;
    surface.midiClipId = demoEntityId (80);
    surface.timelineStart = 0;
    surface.timelineLength = 4096;
    surface.notes = {
        { demoEntityId (81), 0, 512, 60, 60.25, 0.70, 0, 1, true },
        { demoEntityId (82), 512, 384, 64, 64.10, 0.58, 0, 1, false },
        { demoEntityId (83), 1024, 512, 67, 67.35, 0.82, 0, 1, false },
        { demoEntityId (84), 1792, 768, 72, 72.00, 0.64, 0, 1, false },
        { demoEntityId (85), 2560, 512, 69, 69.20, 0.90, 0, 1, false },
        { demoEntityId (86), 3328, 512, 67, 66.85, 0.74, 0, 1, false }
    };

    for (const auto kind : { yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity,
                             yesdaw::ui::UiPianoRollExpressionLaneKind::Pitch })
    {
        yesdaw::ui::UiPianoRollExpressionLaneReadout lane;
        lane.kind = kind;
        lane.valid = true;
        lane.points.reserve (surface.notes.size());

        for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
        {
            const double value = kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity
                ? note.normalizedVelocity
                : note.pitchNote;
            lane.points.push_back ({ note.noteId, note.startTick, value });
        }

        surface.expressionLanes.push_back (std::move (lane));
    }

    return surface;
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
    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat(), radius);
    g.setColour (kPanelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (yesdaw::ui::UiTheme::Layout::panelOutlineInset),
                            radius,
                            yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
}

void drawSmallLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                     juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour (kMutedText);
    g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::small)));
    g.drawText (text, area, justification, false);
}

void drawMeter (juce::Graphics& g, juce::Rectangle<int> area, float value)
{
    g.setColour (yesdaw::ui::UiTheme::Color::controlInsetDeep());
    g.fillRoundedRectangle (area.toFloat(), yesdaw::ui::UiTheme::Radius::xs);

    auto fill = area.reduced (yesdaw::ui::UiTheme::Layout::meterFillInset);
    const int height = juce::roundToInt (static_cast<float> (fill.getHeight()) * juce::jlimit (0.0f, 1.0f, value));
    auto live = fill.removeFromBottom (height);
    auto hot = live.removeFromTop (juce::roundToInt (static_cast<float> (live.getHeight())
                                                    * yesdaw::ui::UiTheme::Meter::verticalHotBand));

    g.setColour (yesdaw::ui::UiTheme::Meter::nominalFill());
    g.fillRect (live);
    g.setColour (yesdaw::ui::UiTheme::Meter::hotFill());
    g.fillRect (hot);
}

void drawHorizontalMeter (juce::Graphics& g, juce::Rectangle<int> area, float value)
{
    g.setColour (yesdaw::ui::UiTheme::Color::controlInsetDeep());
    g.fillRoundedRectangle (area.toFloat(), yesdaw::ui::UiTheme::Radius::xs);
    auto fill = area.reduced (yesdaw::ui::UiTheme::Layout::meterFillInset);
    const int width = juce::roundToInt (static_cast<float> (fill.getWidth()) * juce::jlimit (0.0f, 1.0f, value));
    auto live = fill.withWidth (width);
    auto hot = live.removeFromRight (juce::roundToInt (static_cast<float> (live.getWidth())
                                                      * yesdaw::ui::UiTheme::Meter::horizontalHotBand));
    g.setColour (yesdaw::ui::UiTheme::Meter::nominalFill());
    g.fillRect (live);
    g.setColour (yesdaw::ui::UiTheme::Meter::hotFill());
    g.fillRect (hot);
}

std::optional<yesdaw::ui::UiDecodedAsset> decodeMonoWavForImport (const std::filesystem::path& sourcePath)
{
    juce::WavAudioFormat wav;
    const juce::File file { juce::String { sourcePath.string() } };
    std::unique_ptr<juce::AudioFormatReader> reader (
        wav.createReaderFor (new juce::FileInputStream (file), true));
    if (reader == nullptr)
        return std::nullopt;

    if (reader->sampleRate <= 0.0
        || reader->numChannels != 1u
        || reader->lengthInSamples <= 0
        || reader->lengthInSamples > static_cast<juce::int64> (std::numeric_limits<int>::max()))
        return std::nullopt;

    const int frames = static_cast<int> (reader->lengthInSamples);
    juce::AudioBuffer<float> decodedBuffer (1, frames);
    if (! reader->read (&decodedBuffer, 0, frames, 0, true, false))
        return std::nullopt;

    const float* const channel = decodedBuffer.getReadPointer (0);
    yesdaw::ui::UiDecodedAsset decoded;
    decoded.sampleRate = yesdaw::engine::SampleRate { reader->sampleRate };
    decoded.frames = static_cast<std::uint64_t> (frames);
    decoded.channels = 1;
    decoded.interleavedSamples.assign (channel, channel + frames);
    return decoded;
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

    void paint (juce::Graphics& g) override
    {
        if (stateProvider)
            (void) yesdaw::ui::paintTimelineCanvas (g, getLocalBounds(), stateProvider());
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

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
        if (onEmptyClicked)
            onEmptyClicked();
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
        const double pixelsPerSecond = std::max (1.0, geometry.viewport.pixelsPerSecond);
        const double nextStartSeconds = std::max (0.0, drag.startSeconds + static_cast<double> (deltaX) / pixelsPerSecond);

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
        if (! geometry.clipArea.contains (position))
            return std::nullopt;

        const double pixelsPerSecond = std::max (1.0, geometry.viewport.pixelsPerSecond);
        const double seconds = geometry.viewport.scrollSeconds
                             + static_cast<double> (position.x - geometry.clipArea.getX()) / pixelsPerSecond;
        return std::max (0.0, seconds);
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
        const double pixelsPerSecond = std::max (1.0, geometry.viewport.pixelsPerSecond);
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

class MainComponent : public juce::Component
{
public:
    explicit MainComponent (yesdaw::ui::MainComponentFileChoices choices)
        : fileChoices (std::move (choices))
    {
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
                repaint();
            };
            addAndMakeVisible (button);
        }

        configureAutosaveRecoveryButton (autosaveRestoreButton, yesdaw::ui::UiActionId::AutosaveRecoveryRestore);
        configureAutosaveRecoveryButton (autosaveDiscardButton, yesdaw::ui::UiActionId::AutosaveRecoveryDiscard);

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
        addAndMakeVisible (timelineInput);

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
        refreshActionState();
    }

    [[nodiscard]] const yesdaw::ui::UiActionContext& harnessContext() const noexcept { return appModel.context(); }
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
    [[nodiscard]] bool harnessPlaybackReady() const noexcept { return appModel.playbackReady(); }
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
        g.fillRect (top.withBottom (kHeaderHeight).removeFromBottom (1));

        auto work = bounds.withTrimmedTop (kHeaderHeight);
        auto mixer = work.removeFromBottom (kMixerHeight);
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
        drawMixer (g, mixer.reduced (yesdaw::ui::UiTheme::Layout::mixerPanelHorizontalInset,
                                     yesdaw::ui::UiTheme::Layout::mixerPanelVerticalInset));
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
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::viewMixerButtonBounds (getHeight()));
                    break;
                case yesdaw::ui::UiActionId::ViewPianoRoll:
                    buttons[i].setBounds (yesdaw::ui::UiTheme::Layout::viewPianoRollButtonBounds (getHeight()));
                    break;
                default: buttons[i].setBounds ({});
            }
        }

        autosaveRestoreButton.setBounds (yesdaw::ui::UiTheme::Layout::autosaveRestoreButtonBounds());
        autosaveDiscardButton.setBounds (yesdaw::ui::UiTheme::Layout::autosaveDiscardButtonBounds());
        timelineInput.setBounds (timelineBounds());
        pianoRollInput.setBounds (timelineBounds());
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

    void configureInspectorControls()
    {
        configureActionComponent (inspectorGain, yesdaw::ui::UiActionId::TimelineClipSetGain, "Clip gain");
        inspectorGain.setSliderStyle (juce::Slider::LinearHorizontal);
        inspectorGain.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        inspectorGain.setRange (0.0, 2.0, 0.01);
        inspectorGain.setValue (1.0, juce::dontSendNotification);
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
    }

    void configureInspectorFadeSlider (juce::Slider& slider, const char* componentId, const juce::String& name)
    {
        slider.setComponentID (componentId);
        slider.setName (name);
        slider.setTitle (name);
        slider.setTooltip (componentId);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange (0.0, kMaxInspectorFadeSeconds, 0.001);
        slider.setValue (0.0, juce::dontSendNotification);
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
        mixerFader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        mixerFader.setRange (0.0, 2.0, 0.01);
        mixerFader.setValue (1.0, juce::dontSendNotification);
        mixerFader.onValueChange = [this] {
            if (refreshingMixerControls || ! mixerFader.isEnabled())
                return;

            (void) appModel.setSelectedMixerFader (static_cast<float> (mixerFader.getValue()));
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerFader);

        configureActionComponent (mixerPan, yesdaw::ui::UiActionId::MixerTargetSetPan, "Mixer pan");
        mixerPan.setSliderStyle (juce::Slider::LinearHorizontal);
        mixerPan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        mixerPan.setRange (-1.0, 1.0, 0.01);
        mixerPan.setValue (0.0, juce::dontSendNotification);
        mixerPan.onValueChange = [this] {
            if (refreshingMixerControls || ! mixerPan.isEnabled())
                return;

            (void) appModel.setSelectedMixerPan (static_cast<float> (mixerPan.getValue()));
            refreshActionState();
            repaint();
        };
        addAndMakeVisible (mixerPan);

        configureActionComponent (mixerMute, yesdaw::ui::UiActionId::MixerTargetToggleMute, "Mixer mute");
        mixerMute.setButtonText ("M");
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

    [[nodiscard]] juce::Rectangle<int> mixerFirstStripBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (kHeaderHeight);
        auto mixer = work.removeFromBottom (kMixerHeight)
                         .reduced (yesdaw::ui::UiTheme::Layout::mixerPanelHorizontalInset,
                                   yesdaw::ui::UiTheme::Layout::mixerPanelVerticalInset);
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
    }

    void layoutMixerControls()
    {
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
        mixerSolo.setBounds (buttonRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerButtonWidth));
        mixerMute.setBounds (buttonRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerButtonWidth));
        lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerButtonBottomGap);
        auto faderArea = lane.removeFromTop (
            juce::jmax (yesdaw::ui::UiTheme::Layout::mixerFaderMinHeight,
                        lane.getHeight() - yesdaw::ui::UiTheme::Layout::mixerFaderBottomReserve));
        mixerFader.setBounds (faderArea.withWidth (yesdaw::ui::UiTheme::Layout::mixerFaderWidth)
                                  .withCentre ({ faderArea.getCentreX(), faderArea.getCentreY() }));
    }

    void handleAction (yesdaw::ui::UiActionId action)
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
                        (void) appModel.openProjectBundle (path);
                }
                return;

            case yesdaw::ui::UiActionId::ProjectImportAudio:
                if (fileChoices.chooseImportAudioFile)
                {
                    const std::filesystem::path path = fileChoices.chooseImportAudioFile();
                    if (! path.empty())
                        if (auto decoded = decodeMonoWavForImport (path))
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
        timelineInput.setVisible (appModel.context().activePanel != yesdaw::ui::UiPanel::PianoRoll);
        pianoRollInput.setVisible (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll);
        refreshInspectorControls();
        refreshMixerControls();
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

        inspectorGain.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetGain,
                                                                appModel.context()).enabled);
        inspectorFadeIn.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                  appModel.context()).enabled);
        inspectorFadeOut.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                   appModel.context()).enabled);

        refreshingInspectorControls = true;
        if (selected && appModel.project().sampleRate.isValid())
        {
            const double sampleRate = appModel.project().sampleRate.hz;
            inspectorGain.setValue (clip->gain, juce::dontSendNotification);
            inspectorFadeIn.setValue (std::clamp (static_cast<double> (clip->fadeIn) / sampleRate,
                                                  0.0,
                                                  kMaxInspectorFadeSeconds),
                                      juce::dontSendNotification);
            inspectorFadeOut.setValue (std::clamp (static_cast<double> (clip->fadeOut) / sampleRate,
                                                   0.0,
                                                   kMaxInspectorFadeSeconds),
                                       juce::dontSendNotification);
        }
        else
        {
            inspectorGain.setValue (1.0, juce::dontSendNotification);
            inspectorFadeIn.setValue (0.0, juce::dontSendNotification);
            inspectorFadeOut.setValue (0.0, juce::dontSendNotification);
        }
        refreshingInspectorControls = false;
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

        mixerTrackSelect.setEnabled (projectHasTrack);
        mixerFader.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetSetFader,
                                                             appModel.context()).enabled);
        mixerPan.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetSetPan,
                                                           appModel.context()).enabled);
        mixerMute.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetToggleMute,
                                                            appModel.context()).enabled);
        mixerSolo.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetToggleSolo,
                                                            appModel.context()).enabled);

        refreshingMixerControls = true;
        if (projectHasTrack)
        {
            const auto& strip = project.tracks.front().strip;
            mixerTrackSelect.setButtonText (strip.name.empty() ? "Track 1" : juce::String (strip.name));
            mixerFader.setValue (strip.linearGain, juce::dontSendNotification);
            mixerPan.setValue (strip.pan, juce::dontSendNotification);
            mixerMute.setToggleState (selected && strip.muted, juce::dontSendNotification);
            mixerSolo.setToggleState (selected && strip.soloed, juce::dontSendNotification);
        }
        else
        {
            mixerTrackSelect.setButtonText ("Audio 1");
            mixerFader.setValue (1.0, juce::dontSendNotification);
            mixerPan.setValue (0.0, juce::dontSendNotification);
            mixerMute.setToggleState (false, juce::dontSendNotification);
            mixerSolo.setToggleState (false, juce::dontSendNotification);
        }
        refreshingMixerControls = false;
    }

    void drawHeader (juce::Graphics& g) const
    {
        g.setColour (yesdaw::ui::UiTheme::Color::canvasLayer());
        g.fillRect (getLocalBounds().withHeight (kHeaderHeight));

        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::body)));
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
        g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::transportClock)));
        g.drawText ("01:02:45:18",
                    time.reduced (yesdaw::ui::UiTheme::Layout::headerTransportTextInsetX,
                                  yesdaw::ui::UiTheme::Layout::headerTransportClockInsetY)
                        .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportClockHeight),
                    juce::Justification::centred,
                    false);
        drawSmallLabel (g,
                        "BAR | BEAT",
                        time.reduced (yesdaw::ui::UiTheme::Layout::headerTransportTextInsetX,
                                      yesdaw::ui::UiTheme::Layout::headerTransportLabelInsetY),
                        juce::Justification::centred);

        const std::array<std::pair<const char*, const char*>, 3> readouts {{
            { "120.00", "TEMPO" },
            { "4/4", "TIME SIG" },
            { "Cmaj", "KEY" }
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
            g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::readout)));
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

        g.setColour (kRed);
        g.fillEllipse (static_cast<float> (yesdaw::ui::UiTheme::Layout::headerTransportRecordX),
                       static_cast<float> (yesdaw::ui::UiTheme::Layout::headerTransportRecordY),
                       static_cast<float> (yesdaw::ui::UiTheme::Layout::headerTransportRecordSize),
                       static_cast<float> (yesdaw::ui::UiTheme::Layout::headerTransportRecordSize));
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
        drawHorizontalMeter (g, meter, 0.76f);
        const auto surface = currentMixerSurface();
        const juce::String lufs = surface.loudness.valid
            ? juce::String (surface.loudness.integratedLufs, 1) + " LUFS"
            : "-- LUFS";
        drawSmallLabel (g,
                        lufs,
                        juce::Rectangle<int> (yesdaw::ui::UiTheme::Layout::headerMasterLufsX,
                                              yesdaw::ui::UiTheme::Layout::headerMasterLufsY,
                                              yesdaw::ui::UiTheme::Layout::headerMasterLufsWidth,
                                              yesdaw::ui::UiTheme::Layout::headerMasterLufsHeight),
                        juce::Justification::centred);

        g.setColour (kMutedText);
        g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::statusIcon)));
        g.drawText ("*",
                    getWidth() - yesdaw::ui::UiTheme::Layout::headerStatusIconRightInset,
                    yesdaw::ui::UiTheme::Layout::headerStatusIconY,
                    yesdaw::ui::UiTheme::Layout::headerStatusIconSize,
                    yesdaw::ui::UiTheme::Layout::headerStatusIconSize,
                    juce::Justification::centred,
                    false);
    }

    void drawTrackList (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        fillPanel (g, area);
        auto header = area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        drawSmallLabel (g, "TRACKS",
                        header.reduced (yesdaw::ui::UiTheme::Layout::trackListHeaderInsetX,
                                        yesdaw::ui::UiTheme::Layout::trackListHeaderInsetY));

        const int rowHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
                                          area.getHeight() / static_cast<int> (kTracks.size()));
        for (std::size_t i = 0; i < kTracks.size(); ++i)
        {
            auto row = area.removeFromTop (rowHeight);
            const auto& track = kTracks[i];

            g.setColour (i == 3 ? yesdaw::ui::UiTheme::Color::selectedLane()
                                 : yesdaw::ui::UiTheme::Color::darkControl());
            g.fillRect (row.reduced (yesdaw::ui::UiTheme::Layout::trackListRowHorizontalInset,
                                     yesdaw::ui::UiTheme::Layout::trackListRowVerticalInset));
            g.setColour (track.colour);
            g.fillRect (row.withWidth (yesdaw::ui::UiTheme::Layout::trackListAccentWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::trackListAccentHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::trackListAccentVerticalInset));
            g.setColour (kPanelStroke);
            g.fillRect (row.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight));

            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::title, juce::Font::bold)));
            g.drawText (track.name,
                        row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                            .withHeight (yesdaw::ui::UiTheme::Layout::trackListNameHeight)
                            .translated (yesdaw::ui::UiTheme::Layout::trackListNameOffsetX,
                                         yesdaw::ui::UiTheme::Layout::trackListNameOffsetY),
                        juce::Justification::centredLeft, false);

            g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::readout)));
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
                g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::caption)));
                g.drawText (label, cell, juce::Justification::centred, false);
            }

            auto meter = row.withRight (row.getRight() - yesdaw::ui::UiTheme::Layout::trackListMeterRightInset)
                             .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListMeterWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::trackListMeterHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::trackListMeterVerticalInset);
            drawMeter (g, meter, track.meter);
        }
    }

    yesdaw::ui::TimelineCanvasState makeTimelineState()
    {
        rebuildTimelineClipViews();

        yesdaw::ui::TimelineCanvasState state;
        if (timelineClips.empty())
        {
            state.tracks = kTracks.data();
            state.trackCount = static_cast<int> (kTracks.size());
            state.clips = kClips.data();
            state.clipStyles = kClipStyles.data();
            state.clipCount = static_cast<int> (kClips.size());
            state.totalSeconds = 98.0;
            state.playheadSeconds = 32.0;
        }
        else
        {
            state.tracks = projectTimelineTrack.data();
            state.trackCount = static_cast<int> (projectTimelineTrack.size());
            state.clips = timelineClips.data();
            state.clipStyles = timelineClipStyles.data();
            state.clipCount = static_cast<int> (timelineClips.size());
            state.totalSeconds = timelineTotalSeconds;
            state.playheadSeconds = 0.0;
        }

        state.markers = kTimelineMarkers.data();
        state.markerCount = static_cast<int> (kTimelineMarkers.size());
        state.viewport.scrollSeconds = 0.0;
        state.viewport.pixelsPerSecond = static_cast<double> (juce::jmax (
                                          yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                          timelineInput.getWidth()
                                              - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                      / std::max (1.0, state.totalSeconds);
        return state;
    }

    void rebuildTimelineClipViews()
    {
        timelineClips.clear();
        timelineClipStyles.clear();
        timelineClipIds.clear();

        const yesdaw::engine::Project& project = appModel.project();
        if (! appModel.context().projectLoaded || project.clips.empty() || ! project.sampleRate.isValid())
        {
            timelineTotalSeconds = 98.0;
            return;
        }

        double endSeconds = 0.0;
        const double sampleRate = project.sampleRate.hz;

        for (const yesdaw::engine::Clip& clip : project.clips)
        {
            if (! clip.id.isValid()
                || clip.timelineStart < 0
                || clip.timelineLength <= 0
                || project.findAsset (clip.assetId) == nullptr)
            {
                continue;
            }

            const double startSeconds = static_cast<double> (clip.timelineStart) / sampleRate;
            const double lengthSeconds = static_cast<double> (clip.timelineLength) / sampleRate;
            const int id = static_cast<int> (timelineClips.size());
            timelineClips.push_back ({ id, 0, startSeconds, lengthSeconds });
            timelineClipStyles.push_back ({ kPurple, 0.82f });
            timelineClipIds.push_back (clip.id);
            endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
        }

        timelineTotalSeconds = std::max (1.0, endSeconds * 1.25);
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

        constexpr float kGainPerPixel = 0.01f;
        constexpr float kMaxGestureGain = 4.0f;
        const float nextGain = std::clamp (
            clip->gain + static_cast<float> (deltaPixels) * kGainPerPixel,
            0.0f,
            kMaxGestureGain);

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
                g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::caption)));
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
        g.setColour (kPurple);
        g.fillRoundedRectangle (static_cast<float> (area.getX()),
                                static_cast<float> (area.getY()
                                                    + yesdaw::ui::UiTheme::Layout::inspectorTitleAccentTopInset),
                                static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                                static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                                yesdaw::ui::UiTheme::Radius::sm);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::title, juce::Font::bold)));
        g.drawText ("Vocal Lead_03",
                    area.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorTitleTextLeftInset)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTitleTextHeight),
                    juce::Justification::centredLeft,
                    false);

        auto stats = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                         .withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionHeight);
        for (const auto* label : { "Start\n33.1.1.00", "End\n41.1.1.00", "Length\n8.0.0.00" })
        {
            auto cell = stats.removeFromLeft (stats.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorStatsColumnCount)
                            .reduced (yesdaw::ui::UiTheme::Layout::inspectorStatsCellInsetX,
                                      yesdaw::ui::UiTheme::Layout::inspectorStatsCellInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (kMutedText);
            g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::caption)));
            g.drawFittedText (label,
                              cell.reduced (yesdaw::ui::UiTheme::Layout::inspectorStatsTextInset),
                              juce::Justification::centred,
                              2);
        }

        auto gain = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorGainSectionTop)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainSectionHeight);
        drawSmallLabel (g, "GAIN", gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::title)));
        const yesdaw::engine::Clip* const selectedClip = findProjectClipById (appModel.selectedTimelineClipId());
        const float gainValue = selectedClip != nullptr ? selectedClip->gain : 1.0f;
        g.drawText (juce::String (gainValue, 2) + "x",
                    gain.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainReadoutLeftInset)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainReadoutHeight),
                    juce::Justification::centredLeft,
                    false);

        auto fades = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionTop)
                         .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionHeight);
        drawSmallLabel (g, "FADES", fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        const double sampleRate = appModel.project().sampleRate.isValid() ? appModel.project().sampleRate.hz : 48000.0;
        const double fadeInSeconds = selectedClip != nullptr ? static_cast<double> (selectedClip->fadeIn) / sampleRate : 0.0;
        const double fadeOutSeconds = selectedClip != nullptr ? static_cast<double> (selectedClip->fadeOut) / sampleRate : 0.0;
        for (const auto& label : { juce::String ("Fade In     ") + juce::String (fadeInSeconds, 3) + " s",
                                   juce::String ("Fade Out    ") + juce::String (fadeOutSeconds, 3) + " s" })
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

        auto fx = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFxSectionTop);
        drawSmallLabel (g, "CLIP FX", fx.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        for (const auto* label : { "De-Esser", "Compressor", "EQ" })
        {
            auto row = fx.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFxRowHeight)
                           .reduced (yesdaw::ui::UiTheme::Layout::inspectorFxRowInsetX,
                                     yesdaw::ui::UiTheme::Layout::inspectorFxRowInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::body)));
            g.drawText (label,
                        row.reduced (yesdaw::ui::UiTheme::Layout::inspectorFxTextInsetX,
                                     yesdaw::ui::UiTheme::Layout::inspectorFxTextInsetY),
                        juce::Justification::centredLeft,
                        false);
        }
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
        drawSmallLabel (g,
                        "SENDS",
                        leftTools.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerToolsSendsLabelTop)
                            .withHeight (yesdaw::ui::UiTheme::Layout::mixerToolsLabelHeight)
                            .reduced (yesdaw::ui::UiTheme::Layout::mixerToolsLabelInsetX,
                                      yesdaw::ui::UiTheme::Layout::mixerToolsLabelInsetY));
        drawSmallLabel (g,
                        "VIEW",
                        leftTools.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerToolsViewLabelTop)
                            .withHeight (yesdaw::ui::UiTheme::Layout::mixerToolsLabelHeight)
                            .reduced (yesdaw::ui::UiTheme::Layout::mixerToolsLabelInsetX,
                                      yesdaw::ui::UiTheme::Layout::mixerToolsLabelInsetY));
        drawSmallLabel (g,
                        "Short",
                        leftTools.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerToolsModeLabelTop)
                            .withHeight (yesdaw::ui::UiTheme::Layout::mixerToolsModeLabelHeight)
                            .reduced (yesdaw::ui::UiTheme::Layout::mixerToolsLabelInsetX,
                                      yesdaw::ui::UiTheme::Layout::mixerToolsLabelInsetY));

        const int stripWidth = juce::jmax (
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripMinWidth,
            area.getWidth() / (juce::jmax (yesdaw::ui::UiTheme::Layout::mixerPaintedStripMinCount,
                                           static_cast<int> (stripCount))
                               + yesdaw::ui::UiTheme::Layout::mixerPaintedStripExtraSlotCount));
        for (std::size_t stripIndex = 0; stripIndex < stripCount; ++stripIndex)
        {
            const bool isBus = stripIndex >= surface.tracks.size();
            const auto& state = isBus ? surface.buses[stripIndex - surface.tracks.size()]
                                      : surface.tracks[stripIndex];
            const auto& demoStrip = kMixer[std::min (stripIndex, kMixer.size() - 1u)];
            const bool selected = appModel.context().mixerTargetSelected && stripIndex == 0;

            auto lane = area.removeFromLeft (stripWidth)
                            .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetX,
                                      yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetY);
            g.setColour (selected ? yesdaw::ui::UiTheme::Color::selectedStrip() : kPanelRaised);
            g.fillRoundedRectangle (lane.toFloat(), yesdaw::ui::UiTheme::Radius::panel);
            g.setColour (selected ? kPurple : kPanelStroke);
            g.drawRoundedRectangle (lane.toFloat().reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                    yesdaw::ui::UiTheme::Radius::panel,
                                    selected
                                        ? yesdaw::ui::UiTheme::Layout::mixerPaintedStripSelectedStrokeWidth
                                        : yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);

            g.setColour (demoStrip.colour.withAlpha (0.30f));
            g.fillRect (lane.withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedHeaderHeight));
            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::small)));
            g.drawFittedText (state.name,
                              lane.reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedNameInsetX,
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedNameInsetY)
                                  .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedNameHeight),
                              juce::Justification::centred,
                              1);

            auto knob = lane.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerPaintedPanTop)
                            .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedPanHeight);
            const int panDiameter = yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius * 2;
            const int panX = knob.getCentreX() - yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius;
            const int panY = knob.getY() + yesdaw::ui::UiTheme::Layout::mixerPaintedPanTopInset;
            g.setColour (yesdaw::ui::UiTheme::Color::controlInsetBlack());
            g.fillEllipse (static_cast<float> (panX),
                           static_cast<float> (panY),
                           static_cast<float> (panDiameter),
                           static_cast<float> (panDiameter));
            g.setColour (demoStrip.colour.brighter (0.45f));
            g.drawEllipse (static_cast<float> (panX),
                           static_cast<float> (panY),
                           static_cast<float> (panDiameter),
                           static_cast<float> (panDiameter),
                           yesdaw::ui::UiTheme::Layout::mixerPaintedPanStrokeWidth);

            auto buttonsRow = lane.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerPaintedButtonsTop)
                                  .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedButtonsHeight)
                                  .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedButtonsInsetX,
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedButtonsInsetY);
            for (const auto* label : { "S", "M" })
            {
                auto cell = buttonsRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedButtonWidth)
                                .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedButtonInsetX,
                                          yesdaw::ui::UiTheme::Layout::mixerPaintedButtonInsetY);
                g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::md);
                const bool on = (label == std::string ("S") && state.soloed)
                             || (label == std::string ("M") && state.muted);
                g.setColour (on ? demoStrip.colour.brighter (0.55f) : kText);
                g.drawText (label, cell, juce::Justification::centred, false);
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
                g.setFont (juce::Font (juce::FontOptions (yesdaw::ui::UiTheme::Type::tiny)));
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
            g.setColour (yesdaw::ui::UiTheme::Color::controlInsetDeep());
            g.fillRoundedRectangle (rail.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
            const int thumbY =
                rail.getBottom() - juce::roundToInt (state.linearGain * static_cast<float> (rail.getHeight()))
                - yesdaw::ui::UiTheme::Layout::mixerPaintedThumbCenterInset;
            auto thumb = juce::Rectangle<int> (
                rail.getX() - yesdaw::ui::UiTheme::Layout::mixerPaintedThumbWidthOverhang / 2,
                thumbY,
                rail.getWidth() + yesdaw::ui::UiTheme::Layout::mixerPaintedThumbWidthOverhang,
                yesdaw::ui::UiTheme::Layout::mixerPaintedThumbHeight);
            g.setColour (yesdaw::ui::UiTheme::Color::faderThumb());
            g.fillRoundedRectangle (thumb.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
        }
    }

    [[nodiscard]] yesdaw::ui::UiMixerSurfaceSnapshot currentMixerSurface() const
    {
        if (appModel.context().projectLoaded)
            return yesdaw::ui::projectUiMixerSurface (appModel.project());

        return mixerSurface;
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

        return pianoSurface;
    }

    yesdaw::ui::UiAppModel appModel;
    yesdaw::ui::MainComponentFileChoices fileChoices;
    yesdaw::ui::UiMixerSurfaceSnapshot mixerSurface = makeDemoMixerSurface();
    yesdaw::ui::UiPianoRollSurfaceSnapshot pianoSurface = makeDemoPianoRollSurface();
    std::array<TrackRow, 1> projectTimelineTrack {{{ "Audio 1", kPurple, 0.75f }}};
    std::vector<yesdaw::ui::Clip> timelineClips;
    std::vector<TimelineClipStyle> timelineClipStyles;
    std::vector<yesdaw::engine::EntityId> timelineClipIds;
    double timelineTotalSeconds = 98.0;
    TimelineInputComponent timelineInput;
    PianoRollInputComponent pianoRollInput;
    juce::TextButton mixerTrackSelect;
    juce::Slider mixerFader;
    juce::Slider mixerPan;
    juce::ToggleButton mixerMute;
    juce::ToggleButton mixerSolo;
    juce::TextButton autosaveRestoreButton;
    juce::TextButton autosaveDiscardButton;
    juce::Slider inspectorGain;
    juce::Slider inspectorFadeIn;
    juce::Slider inspectorFadeOut;
    std::array<juce::TextButton, yesdaw::ui::kMainShellToolbarActions.size()> buttons;
    bool refreshingInspectorControls = false;
    bool refreshingMixerControls = false;

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

namespace yesdaw::ui {

std::unique_ptr<juce::Component> createMainComponent (MainComponentFileChoices fileChoices)
{
    return std::make_unique<MainComponent> (std::move (fileChoices));
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
        snapshot.playbackReady = mainComponent->harnessPlaybackReady();
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
