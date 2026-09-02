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
#include <chrono>
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

// G0.7: the controls that live in the collapsible settings row under the toolbar.
[[nodiscard]] constexpr bool isSettingsRowAction (yesdaw::ui::UiActionId action) noexcept
{
    using yesdaw::ui::UiActionId;
    return action == UiActionId::RecordingArmTrack || action == UiActionId::RecordingSetMonitoringPolicy
        || action == UiActionId::RecordingAssembleComp;
}
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
// N1: a mixer strip carries exactly two painted toggle cells — Solo then Mute, left to right.
constexpr std::size_t kMixerPaintedMuteSoloCellCount = 2;
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

// N7: the fixed swatch palette a rail-row colour click cycles through. Position 0 is "no
// override" (kTrackColourUnset); positions 1..5 mirror the SAME five accents
// stripColourForIndex already draws from (kBlue/kTeal/kAmber/kPurple/kCyan), so a customized
// track colour always looks native to this theme instead of introducing a new arbitrary hue.
// Written as raw hex (not the juce::Colour constants above) so the array can be constexpr.
constexpr std::array<std::uint32_t, 6> kTrackColourCycle {
    yesdaw::engine::kTrackColourUnset,
    0xff3b8cffu,   // accentBlue
    0xff1bb5a6u,   // accentTeal
    0xffd29118u,   // accentAmber
    0xffa578ffu,   // accentPurple
    0xff20c8d8u,   // accentCyan
};

[[nodiscard]] std::uint32_t nextTrackColourInCycle (std::uint32_t current) noexcept
{
    const auto it = std::find (kTrackColourCycle.begin(), kTrackColourCycle.end(), current);
    const std::size_t index = it == kTrackColourCycle.end()
                                   ? 0
                                   : static_cast<std::size_t> (it - kTrackColourCycle.begin());
    return kTrackColourCycle[(index + 1) % kTrackColourCycle.size()];
}

// N7: what the rail/mixer/clips actually paint for a track — its own persisted colour when set,
// otherwise the surface's historical fallback (so an untouched Project renders bit-identically
// to before this field existed).
[[nodiscard]] juce::Colour colourForTrack (const yesdaw::engine::Track& track, juce::Colour fallbackColour) noexcept
{
    return track.colour == yesdaw::engine::kTrackColourUnset ? fallbackColour : juce::Colour (track.colour);
}

// Translate a JUCE KeyPress into the keymap's chord vocabulary ("Ctrl+Alt+Shift+B", "Space", "Del",
// "F2", "Ctrl+/"). Modifier order matches the descriptor table: Ctrl, Alt, Shift.
// G0.1 State probe: the shell's JSON schema version. Bumped only when a field changes meaning;
// the [state-probe] gate and tools/session-drive.ps1 pin it.
constexpr int kStateProbeSchemaVersion = 1;
// G0.1: paint-time ring used for the p95 the B2 feel budget reads (about eight seconds at 30 Hz).
constexpr std::size_t kStateProbePaintRingSize = 256;

// G0.1: an EntityId as 32 lowercase hex digits — the id form the State probe publishes and the
// Session drive clicks by (`clip.<hex>`).
std::string entityIdHex (const yesdaw::engine::EntityId& id)
{
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve (id.bytes.size() * 2u);
    for (const std::uint8_t byte : id.bytes)
    {
        out.push_back (kDigits[byte >> 4u]);
        out.push_back (kDigits[byte & 0x0Fu]);
    }
    return out;
}

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
    // G1.1: the numpad spells the same chords as the main keys.
    else if (code >= juce::KeyPress::numberPad0 && code <= juce::KeyPress::numberPad9)
        chord += static_cast<char> ('0' + (code - juce::KeyPress::numberPad0));
    else if (code == juce::KeyPress::numberPadAdd)
        chord += "+";
    else if (code == juce::KeyPress::numberPadSubtract)
        chord += "-";
    else if (code == juce::KeyPress::numberPadDecimalPoint)
        chord += ".";
    else if (code == juce::KeyPress::numberPadMultiply)
        chord += "*";
    else if (code == juce::KeyPress::numberPadDivide)
        chord += "/";
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

// R5: the three ways a stored project can fail to open are distinct facts the user needs —
// never collapsed into one silent nullopt. `failureReason` is set exactly when `assets` is
// empty-optional, and names the first bad audio file where one is the cause.
struct StoredProjectAssetsResult
{
    std::optional<std::vector<yesdaw::ui::UiDecodedAsset>> assets;
    std::string failureReason;
};

StoredProjectAssetsResult decodeStoredProjectAssets (const std::filesystem::path& bundlePath)
{
    StoredProjectAssetsResult out;

    yesdaw::persistence::ProjectBundleDb db;
    const yesdaw::persistence::BundleResult opened =
        yesdaw::persistence::ProjectBundleDb::openExistingBundle (bundlePath, db);
    if (! opened.ok())
    {
        // The bundle layer's own message is the most precise fact available — e.g.
        // "committed asset bytes are missing: <path>" from the open-time integrity check.
        out.failureReason = opened.message.empty() ? "the project file could not be opened"
                                                   : opened.message;
        return out;
    }

    yesdaw::engine::Project project;
    const yesdaw::persistence::BundleResult read = db.readProjectSnapshot (project);
    if (! read.ok())
    {
        out.failureReason = read.message.empty() ? "the project data is invalid or corrupt"
                                                 : read.message;
        return out;
    }

    std::vector<yesdaw::ui::UiDecodedAsset> decodedAssets;
    decodedAssets.reserve (project.assets.size());
    for (const yesdaw::engine::Asset& asset : project.assets)
    {
        const std::filesystem::path assetPath =
            yesdaw::persistence::storedAssetPathForHash (bundlePath, asset.contentHash);
        auto decoded = decodeProjectWav (assetPath);
        if (! decoded
            || decoded->frames != asset.frames
            || decoded->sampleRate != asset.sampleRate
            || decoded->channels != asset.channels)
        {
            out.failureReason =
                "missing or corrupt audio file: " + assetPath.filename().string();
            return out;
        }

        decoded->assetId = asset.id;
        decodedAssets.push_back (std::move (*decoded));
    }

    out.assets = std::move (decodedAssets);
    return out;
}

} // namespace

class TimelineInputComponent final : public juce::Component,
                                     public juce::FileDragAndDropTarget,
                                     public juce::SettableTooltipClient
{
public:
    // M10: dropping files from the OS. The drop POINT picks the track and the start tick; the
    // shell decides what is importable and reports refusals honestly.
    std::function<bool (const juce::StringArray&)> filesAreImportable;
    std::function<void (const juce::StringArray&, int, double)> onFilesDropped;   // files, lane, seconds

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return filesAreImportable && filesAreImportable (files);
    }

    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        if (! onFilesDropped || ! stateProvider)
            return;

        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        if (state.trackCount <= 0 || geometry.laneHeight <= 0)
            return;

        const juce::Point<int> position { x, y };
        const int lane = std::clamp (
            geometry.laneAtPixel (position.y - geometry.clipArea.getY() + geometry.viewport.laneScrollPixels),
            0, state.trackCount - 1);
        const double seconds = timelineSecondsAt (state, getLocalBounds(), position).value_or (0.0);
        onFilesDropped (files, lane, seconds);
    }

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
    // N8: Alt+Shift-drag on the ruler — startSeconds, endSeconds, snapInvert. A degenerate span
    // (end <= start, a click rather than a real drag) means "clear the punch region", not "set a
    // zero-length one".
    std::function<void (double, double, bool)> onPunchRegionDragged;
    std::function<void (double, double, bool)> onRulerRangeSelected;  // startSeconds, endSeconds, snapInvert (plain drag)
    std::function<void()> onRulerRangeCleared;                   // plain ruler click collapses the range
    std::function<void (double, double)> onZoomWheel;            // anchorSeconds, wheelDelta
    std::function<void (double)> onRulerAltClicked;              // seconds: remove nearest marker
    std::function<void (double)> onScrollWheel;                  // wheelDelta (view-widths per notch)
    std::function<void (double, bool)> onZoomToolClicked;        // anchorSeconds, zoomOut (Alt) — E3
    std::function<void (double)> onHandToolScrolled;             // secondsDelta from a Hand drag — E3
    std::function<void (int, double)> onPencilEmptyLane;         // lane, seconds: pencil a MIDI clip — E3
    std::function<void (int)> onVerticalScrollRows;              // +1 down / -1 up, plain wheel — E5

    // Loop brace editing (E6): drag either handle to resize, drag the band to move.
    enum class LoopBraceEdit : std::uint8_t { None, Start, End, Move };
    std::function<void (LoopBraceEdit, double, double, bool)> onLoopBraceEdited;
    // kind, pointerSeconds, grabOffsetSeconds (Move only), snapInvert

    // Marker editing (E7): drag a ruler marker label to move it; double-click to rename.
    std::function<void (int, double, bool)> onMarkerDragged;      // markerIndex, seconds, snapInvert
    std::function<void (int)> onMarkerRenameRequested;            // markerIndex

    // E9: double-click on a clip, fired before the split path; returning true consumes the
    // gesture (a MIDI clip opens its piano roll instead of attempting the audio split).
    std::function<bool (int)> onClipDoubleClicked;                // layoutClipId -> consumed

    [[nodiscard]] bool cancelInProgressEdit()
    {
        if (! dragState.active && ! marqueeState.active && ! rulerRangeDragActive && ! handDragActive
            && loopBraceDrag == LoopBraceEdit::None && markerDragIndex < 0)
            return false;

        dragState = {};
        marqueeState = {};
        rulerRangeDragActive = false;
        handDragActive = false;
        loopBraceDrag = LoopBraceEdit::None;
        markerDragIndex = -1;
        repaint();
        return true;
    }

    void paint (juce::Graphics& g) override
    {
        if (stateProvider)
        {
            yesdaw::ui::TimelineCanvasState state = stateProvider();
            // Loop brace drag preview (E6): the in-flight brace follows the raw pointer; the
            // committed edit applies the snap chooser on release.
            if (loopBraceDrag != LoopBraceEdit::None && state.loopActive)
            {
                const double span = state.loopEndSeconds - state.loopStartSeconds;
                if (loopBraceDrag == LoopBraceEdit::Start && loopBracePointerSeconds < state.loopEndSeconds)
                    state.loopStartSeconds = std::max (0.0, loopBracePointerSeconds);
                else if (loopBraceDrag == LoopBraceEdit::End && loopBracePointerSeconds > state.loopStartSeconds)
                    state.loopEndSeconds = loopBracePointerSeconds;
                else if (loopBraceDrag == LoopBraceEdit::Move)
                {
                    state.loopStartSeconds = std::max (0.0, loopBracePointerSeconds - loopBraceGrabOffsetSeconds);
                    state.loopEndSeconds = state.loopStartSeconds + span;
                }
            }
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

    // G1.3: a right-click classifies what was clicked (clip / marker / ruler / empty lane),
    // makes it the selection, and asks the shell for that target's context menu.
    std::function<void (yesdaw::ui::ContextMenuTarget, int, juce::Point<int>)> onContextMenuRequested;

    void requestContextMenu (juce::Point<int> position)
    {
        if (! stateProvider || ! onContextMenuRequested)
            return;
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        if (geometry.rulerArea.contains (position))
        {
            for (int markerIndex = 0; markerIndex < state.markerCount; ++markerIndex)
            {
                if (yesdaw::ui::timelineMarkerLabelRect (getLocalBounds(), state, markerIndex).contains (position))
                {
                    // The marker's own verbs act on the marker nearest the playhead: go there first.
                    if (onTimelineLocated)
                        onTimelineLocated (state.markers[markerIndex].seconds);
                    onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Marker, markerIndex, position);
                    return;
                }
            }
            onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Ruler, -1, position);
            return;
        }
        if (! geometry.clipArea.contains (position))
            return;
        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, position);
        if (hit.hit)
        {
            // Logic: a right-click on an unselected clip selects it; one on a selected clip keeps
            // the selection (so a multi-selection's menu acts on all of it).
            bool alreadySelected = false;
            for (int c = 0; c < state.clipCount; ++c)
                if (state.clips[c].id == hit.id && state.clipStyles != nullptr && state.clipStyles[c].selected)
                    alreadySelected = true;
            if (! alreadySelected && onClipClicked)
                onClipClicked (hit.id, false);
            onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Clip, hit.id, position);
            return;
        }
        if (state.trackCount <= 0 || geometry.laneHeight <= 0)
            return;
        const int lane = std::clamp (
            geometry.laneAtPixel (position.y - geometry.clipArea.getY() + geometry.viewport.laneScrollPixels),
            0, state.trackCount - 1);
        onContextMenuRequested (yesdaw::ui::ContextMenuTarget::EmptyLane, lane, position);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        if (event.mods.isRightButtonDown())   // the right button itself: on macOS isPopupMenu() also fires for Ctrl+click, which is a gesture modifier here
        {
            requestContextMenu (event.getPosition());
            return;
        }

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
                            toolGeometry.laneAtPixel (event.getPosition().y - toolGeometry.clipArea.getY()
                                                      + toolGeometry.viewport.laneScrollPixels),
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
            // N8: Alt+Shift-drag defines the punch region — checked before plain Alt (marker
            // removal) so the more specific combo wins. Ctrl is already reserved, across every
            // ruler drag gesture below, as a release-time "invert snap" modifier — it is NOT
            // available as a gesture selector at mouse-down, so punch cannot use it alone.
            if (event.mods.isAltDown() && event.mods.isShiftDown())
            {
                punchDragActive = true;
                punchDragStartSeconds =
                    timelineSecondsAt (state, getLocalBounds(), event.getPosition()).value_or (0.0);
                return;
            }

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

            // Loop brace editing (E6): a press on the painted brace edits the loop instead of
            // locating; the handle/band rects come from the same law the painter uses.
            if (const yesdaw::ui::TimelineLoopBraceRects loopRects =
                    yesdaw::ui::timelineLoopBraceRects (getLocalBounds(), state);
                loopRects.valid)
            {
                if (const std::optional<double> seconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    if (loopRects.startHandle.contains (event.getPosition()))
                    {
                        loopBraceDrag = LoopBraceEdit::Start;
                        loopBracePointerSeconds = *seconds;
                        repaint();
                        return;
                    }
                    if (loopRects.endHandle.contains (event.getPosition()))
                    {
                        loopBraceDrag = LoopBraceEdit::End;
                        loopBracePointerSeconds = *seconds;
                        repaint();
                        return;
                    }
                    if (loopRects.band.contains (event.getPosition()))
                    {
                        loopBraceDrag = LoopBraceEdit::Move;
                        loopBracePointerSeconds = *seconds;
                        loopBraceGrabOffsetSeconds = *seconds - state.loopStartSeconds;
                        repaint();
                        return;
                    }
                }
            }

            // Marker editing (E7): a press on a painted marker label starts a marker drag
            // instead of locating; the label rects come from the shared geometry law.
            for (int markerIndex = 0; markerIndex < state.markerCount; ++markerIndex)
            {
                if (yesdaw::ui::timelineMarkerLabelRect (getLocalBounds(), state, markerIndex)
                        .contains (event.getPosition()))
                {
                    markerDragIndex = markerIndex;
                    markerDragDownX = event.getPosition().x;
                    return;
                }
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
        if (markerDragIndex >= 0)
            return;

        if (loopBraceDrag != LoopBraceEdit::None && stateProvider)
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            if (const std::optional<double> seconds =
                    timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                loopBracePointerSeconds = *seconds;
            repaint();
            return;
        }

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
        if (markerDragIndex >= 0)
        {
            const int markerIndex = markerDragIndex;
            markerDragIndex = -1;
            if (stateProvider)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                const std::optional<double> seconds =
                    timelineSecondsAt (state, getLocalBounds(), event.getPosition());
                if (std::abs (event.getPosition().x - markerDragDownX)
                        < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                {
                    // A plain click on a marker label keeps the historical ruler-click locate.
                    if (seconds && onTimelineLocated)
                        onTimelineLocated (*seconds);
                }
                else if (seconds && onMarkerDragged)
                {
                    onMarkerDragged (markerIndex, *seconds, event.mods.isCtrlDown());
                }
            }
            return;
        }

        if (loopBraceDrag != LoopBraceEdit::None)
        {
            const LoopBraceEdit kind = loopBraceDrag;
            loopBraceDrag = LoopBraceEdit::None;
            if (stateProvider && onLoopBraceEdited)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                if (const std::optional<double> seconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                    onLoopBraceEdited (kind, *seconds, loopBraceGrabOffsetSeconds, event.mods.isCtrlDown());
            }
            repaint();
            return;
        }

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

        if (punchDragActive)
        {
            punchDragActive = false;
            if (stateProvider && onPunchRegionDragged)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                if (const std::optional<double> endSeconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    const double first = std::min (punchDragStartSeconds, *endSeconds);
                    const double second = std::max (punchDragStartSeconds, *endSeconds);
                    // second <= first means "clear"
                    onPunchRegionDragged (first, second, event.mods.isCtrlDown());
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
            const int lane = geometry.laneAtPixel (event.getPosition().y - geometry.clipArea.getY()
                                                   + geometry.viewport.laneScrollPixels);
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
            {
                // Marker editing (E7): double-click on a marker label opens the inline rename.
                for (int markerIndex = 0; markerIndex < state.markerCount; ++markerIndex)
                {
                    if (yesdaw::ui::timelineMarkerLabelRect (getLocalBounds(), state, markerIndex)
                            .contains (event.getPosition()))
                    {
                        if (onMarkerRenameRequested)
                            onMarkerRenameRequested (markerIndex);
                        return;
                    }
                }

                if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                    if (onTimelineLocated)
                        onTimelineLocated (*seconds);
            }
            return;
        }

        if (onClipClicked)
            onClipClicked (hit.id, false);

        if (onClipDoubleClicked && onClipDoubleClicked (hit.id))
            return;

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
            state.clips, state.clipCount, yesdaw::ui::viewportForClipLayout (geometry),
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

        // R1: the edge zones only bite on a clip painted wide enough to keep a grabbable
        // middle (the piano roll's E12 law) — otherwise a short clip at a wide zoom is all
        // edge and could never be moved. Modifier gestures (Shift gain, Ctrl snap-invert,
        // Alt body copy-drag) stay available at any width.
        if (clipRightX - clipLeftX
            >= static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineClipEdgeMinGrabWidth))
        {
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
        }

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
    // Loop brace drag (E6): raw-pointer preview state; the commit applies the snap chooser.
    LoopBraceEdit loopBraceDrag = LoopBraceEdit::None;
    double loopBracePointerSeconds = 0.0;
    double loopBraceGrabOffsetSeconds = 0.0;
    // Marker drag (E7): a below-dead-zone release keeps the historical ruler-click locate.
    int markerDragIndex = -1;
    int markerDragDownX = 0;
    bool playheadLocateActive = false;
    bool loopDragActive = false;
    double loopDragStartSeconds = 0.0;
    bool punchDragActive = false;
    double punchDragStartSeconds = 0.0;
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

// Piano-roll viewport (E10): the visible horizontal window is the clip length divided by the
// zoom, offset by the scroll ticks; the vertical window is the 25 keys above viewLowKey. With
// the default view this reproduces the historical stretched full-clip C3-C5 mapping exactly.
[[nodiscard]] yesdaw::engine::Tick pianoRollVisibleTicks (
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface) noexcept
{
    const double zoom = std::clamp (surface.viewZoom,
                                    yesdaw::ui::UiThemeLayout::pianoRollZoomMin,
                                    yesdaw::ui::UiThemeLayout::pianoRollZoomMax);
    return juce::jmax<yesdaw::engine::Tick> (
        1, static_cast<yesdaw::engine::Tick> (
               std::llround (static_cast<double> (pianoRollTimelineLength (surface)) / zoom)));
}

[[nodiscard]] int pianoRollViewHighKey (const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface) noexcept
{
    return surface.viewLowKey + yesdaw::ui::UiTheme::Layout::pianoRollKeyCount - 1;
}

[[nodiscard]] int pianoRollKeyY (const PianoRollCanvasGeometry& geometry,
                                 const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                 int key) noexcept
{
    return geometry.grid.getY()
         + juce::roundToInt (
             static_cast<float> (pianoRollViewHighKey (surface) - key) * geometry.rowHeight);
}

[[nodiscard]] int pianoRollTickX (const PianoRollCanvasGeometry& geometry,
                                  const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                  yesdaw::engine::Tick tick) noexcept
{
    const double visibleTicks = static_cast<double> (pianoRollVisibleTicks (surface));
    const double normalized = static_cast<double> (tick - surface.viewScrollTicks) / visibleTicks;
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
                       * static_cast<double> (pianoRollVisibleTicks (surface))
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
    const int y = pianoRollKeyY (geometry, surface, note.key)
                + yesdaw::ui::UiTheme::Layout::pianoRollNoteTopInset;
    const int height = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollNoteMinHeight,
                                   juce::roundToInt (geometry.rowHeight)
                                       - yesdaw::ui::UiTheme::Layout::pianoRollNoteHeightTrim);
    return juce::Rectangle<int> (x, y, width, height)
        .reduced (yesdaw::ui::UiTheme::Layout::pianoRollNoteInsetX,
                  yesdaw::ui::UiTheme::Layout::pianoRollNoteInsetY);
}

// E13: the velocity lane is the FIRST expression lane; this mirrors the paint loop's inset
// chain exactly so lane input and lane paint share one law.
[[nodiscard]] juce::Rectangle<int> pianoRollVelocityLaneArea (
    const PianoRollCanvasGeometry& geometry) noexcept
{
    juce::Rectangle<int> expression = geometry.expression.reduced (
        yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetX,
        yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetY);
    return expression.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneHeight)
        .reduced (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetX,
                  yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetY);
}

// E13: invert the lane paint's value law — the y that painted a velocity maps back to it.
[[nodiscard]] double pianoRollVelocityForLaneY (juce::Rectangle<int> lane, int y) noexcept
{
    const double usable = static_cast<double> (
        juce::jmax (1, lane.getHeight() - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathVerticalInset));
    const double bottom = static_cast<double> (
        lane.getBottom() - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathBottomInset);
    return juce::jlimit (0.0, 1.0, (bottom - static_cast<double> (y)) / usable);
}

// E13: map a lane x back to a tick with the grid's time law.
[[nodiscard]] yesdaw::engine::Tick pianoRollTickForX (
    const PianoRollCanvasGeometry& geometry,
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
    int x) noexcept
{
    const int gridWidth = juce::jmax (1, geometry.grid.getWidth());
    const double normalized = juce::jlimit (
        0.0, 1.0, static_cast<double> (x - geometry.grid.getX()) / static_cast<double> (gridWidth));
    return surface.viewScrollTicks
         + static_cast<yesdaw::engine::Tick> (
               std::llround (normalized * static_cast<double> (pianoRollVisibleTicks (surface))));
}

class PianoRollInputComponent final : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    std::function<yesdaw::ui::UiPianoRollSurfaceSnapshot()> stateProvider;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onNoteClicked;
    // E12: a drag moves the WHOLE selection by (tickDelta, keyDelta) anchored on the dragged
    // note; the left edge trims the note head with the end fixed.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick, int)> onNotesDragged;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteHeadTrimmed;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteLengthChanged;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, std::int32_t)> onNoteTransposed;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteQuantized;
    std::function<void()> onExpressionRead;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::Tick, std::int16_t)> onNoteAdded;
    // Alt+wheel on a note adjusts its velocity (B33): clip, note, new normalized velocity.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, double)> onNoteVelocityAdjusted;
    // E13: a drag in the velocity lane paints the crossed notes' velocities (the whole selection
    // paints together when a crossed note is selected) as one undo transaction.
    std::function<void (yesdaw::engine::EntityId,
                        std::span<const std::pair<yesdaw::engine::EntityId, double>>)> onVelocityLanePainted;
    // Ctrl+drag copy-drags a note (B35): clip, source note, copy's start tick.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteCopyDragged;
    // Piano-roll selection tools (E11): the empty grid is tool-aware (Pencil adds, Pointer
    // deselects and marquee-selects), Shift+click toggles, plain double-click deletes a note.
    std::function<yesdaw::ui::TimelineTool()> activeToolProvider;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onNoteToggled;
    std::function<void (yesdaw::engine::EntityId, std::span<const yesdaw::engine::EntityId>)> onNotesMarqueeSelected;
    std::function<void()> onSelectionCleared;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onNoteDeleted;
    // Piano-roll viewport (E10): plain wheel scrolls keys, Shift+wheel scrolls time, Ctrl+wheel
    // zooms time anchored at the pointer tick. Alt+wheel keeps the B33 velocity law.
    std::function<void (int)> onViewKeysScrolled;                          // +1 up / -1 down
    std::function<void (yesdaw::engine::Tick, double)> onViewZoomWheel;    // anchorTick, wheelDelta
    std::function<void (double)> onViewTicksScrolled;                      // fraction of the visible window

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (! stateProvider)
            return;

        const double delta = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (delta == 0.0)
            return;

        if (event.mods.isAltDown())
        {
            if (! onNoteVelocityAdjusted)
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
            return;
        }

        if (event.mods.isCtrlDown() || event.mods.isCommandDown())
        {
            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            const int gridWidth = juce::jmax (yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                              geometry.grid.getWidth());
            const double normalized = juce::jlimit (
                0.0, 1.0,
                static_cast<double> (event.getPosition().x - geometry.grid.getX())
                    / static_cast<double> (gridWidth));
            const yesdaw::engine::Tick anchorTick = surface.viewScrollTicks
                + static_cast<yesdaw::engine::Tick> (
                    normalized * static_cast<double> (pianoRollVisibleTicks (surface)));
            if (onViewZoomWheel)
                onViewZoomWheel (anchorTick, delta);
            return;
        }

        if (event.mods.isShiftDown())
        {
            if (onViewTicksScrolled)
                onViewTicksScrolled (delta);
            return;
        }

        if (onViewKeysScrolled)
            onViewKeysScrolled (delta > 0.0 ? 1 : -1);
    }

    // G1.3: a right-click on a note selects it and asks for the Note context menu.
    std::function<void (yesdaw::ui::ContextMenuTarget, int, juce::Point<int>)> onContextMenuRequested;

    void requestContextMenu (juce::Point<int> position)
    {
        if (! stateProvider || ! onContextMenuRequested)
            return;
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const auto hit = noteAt (surface, position);
        if (! hit)
            return;
        if (onNoteClicked)
            onNoteClicked (surface.midiClipId, hit->noteId);
        onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Note, -1, position);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        if (event.mods.isRightButtonDown())   // the right button itself: on macOS isPopupMenu() also fires for Ctrl+click, which is a gesture modifier here
        {
            requestContextMenu (event.getPosition());
            return;
        }

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const auto hit = noteAt (surface, event.getPosition());
        if (! hit)
        {
            dragState = {};
            marqueeState = {};
            velocityDragState = {};
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            // E13: a press in the velocity lane starts a velocity paint drag.
            if (surface.midiClipSelected
                && pianoRollVelocityLaneArea (geometry).contains (event.getPosition()))
            {
                const juce::Rectangle<int> lane = pianoRollVelocityLaneArea (geometry);
                velocityDragState.active = true;
                velocityDragState.midiClipId = surface.midiClipId;
                velocityDragState.downPosition = event.getPosition();
                velocityDragState.downTick = pianoRollTickForX (geometry, surface, event.getPosition().x);
                velocityDragState.downVelocity = pianoRollVelocityForLaneY (lane, event.getPosition().y);
                velocityDragState.currentTick = velocityDragState.downTick;
                velocityDragState.currentVelocity = velocityDragState.downVelocity;
                return;
            }
            if (! surface.midiClipSelected
                || ! geometry.grid.contains (event.getPosition())
                || geometry.grid.getWidth() <= 0)
                return;

            // The empty grid is tool-aware (E11): the Pencil adds a note at the clicked tick and
            // key; the Pointer clears the selection and starts a note marquee.
            const yesdaw::ui::TimelineTool tool =
                activeToolProvider ? activeToolProvider() : yesdaw::ui::TimelineTool::Pointer;
            if (tool == yesdaw::ui::TimelineTool::Pencil)
            {
                if (! onNoteAdded)
                    return;

                const double normalized =
                    static_cast<double> (event.getPosition().x - geometry.grid.getX())
                    / static_cast<double> (geometry.grid.getWidth());
                const auto visibleTicks = static_cast<double> (pianoRollVisibleTicks (surface));
                yesdaw::engine::Tick tick = surface.viewScrollTicks
                    + static_cast<yesdaw::engine::Tick> (normalized * visibleTicks);
                // E12: the pencil floors to the REAL snap chooser grid (chooser Off = raw).
                if (surface.snapEnabled && surface.snapGridTicks > 0)
                    tick -= tick % surface.snapGridTicks;

                const float rowHeight = geometry.rowHeight > 1.0f ? geometry.rowHeight : 1.0f;
                const int key = pianoRollViewHighKey (surface)
                    - static_cast<int> ((event.getPosition().y - geometry.grid.getY()) / rowHeight);
                if (key >= surface.viewLowKey && key <= pianoRollViewHighKey (surface)
                    && key >= yesdaw::ui::UiThemeLayout::pianoRollKeyMin
                    && key <= yesdaw::ui::UiThemeLayout::pianoRollKeyMax)
                    onNoteAdded (surface.midiClipId, tick, static_cast<std::int16_t> (key));
                return;
            }

            if (tool == yesdaw::ui::TimelineTool::Pointer)
            {
                if (onSelectionCleared)
                    onSelectionCleared();
                marqueeState.active = true;
                marqueeState.downPosition = geometry.grid.getConstrainedPoint (event.getPosition());
                marqueeState.currentPosition = marqueeState.downPosition;
                repaint();
            }
            return;
        }

        // Shift+click toggles the note in the multi-selection (E11); a Shift+DRAG keeps the
        // historical length-edit law, so the toggle is resolved on a movement-free mouse-up.
        if (event.mods.isShiftDown())
        {
            pendingShiftToggleNoteId = hit->noteId;
        }
        else if (onNoteClicked)
        {
            onNoteClicked (surface.midiClipId, hit->noteId);
        }

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

    // Note marquee overlay (E11): painted above the roll canvas with the shared marquee style.
    void paint (juce::Graphics& g) override
    {
        if (! marqueeState.active || ! marqueeState.moved)
            return;

        const juce::Rectangle<int> marquee (marqueeState.downPosition, marqueeState.currentPosition);
        g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
            yesdaw::ui::UiTheme::Tone::pressedHighlightAlpha));
        g.fillRect (marquee);
        g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
            yesdaw::ui::UiTheme::Tone::focusRingAlpha));
        g.drawRect (marquee.toFloat(), yesdaw::ui::UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
    }

    [[nodiscard]] bool cancelInProgressEdit()
    {
        if (! dragState.active && ! marqueeState.active && ! velocityDragState.active
            && ! pendingShiftToggleNoteId.isValid())
            return false;

        dragState = {};
        marqueeState = {};
        velocityDragState = {};
        pendingShiftToggleNoteId = {};
        repaint();
        return true;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (velocityDragState.active)
        {
            if (! stateProvider)
                return;
            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            const juce::Rectangle<int> lane = pianoRollVelocityLaneArea (geometry);
            velocityDragState.currentTick = pianoRollTickForX (geometry, surface, event.getPosition().x);
            velocityDragState.currentVelocity = pianoRollVelocityForLaneY (lane, event.getPosition().y);
            const int deltaX = event.getPosition().x - velocityDragState.downPosition.x;
            const int deltaY = event.getPosition().y - velocityDragState.downPosition.y;
            velocityDragState.moved = velocityDragState.moved
                || std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                || std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            return;
        }

        if (marqueeState.active)
        {
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            marqueeState.currentPosition = geometry.grid.getConstrainedPoint (event.getPosition());
            const int deltaX = marqueeState.currentPosition.x - marqueeState.downPosition.x;
            const int deltaY = marqueeState.currentPosition.y - marqueeState.downPosition.y;
            marqueeState.moved = std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                              || std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            repaint();
            return;
        }

        if (dragState.active)
            dragState.moved = true;
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (velocityDragState.active)
        {
            const VelocityDragState drag = velocityDragState;
            velocityDragState = {};
            if (! drag.moved || ! stateProvider || ! onVelocityLanePainted)
                return;

            // E13: crossed notes are the ones whose COLUMN (note span) overlaps the swept tick
            // range; each takes the drag line's velocity at its own start tick (clamped to the
            // segment ends). If any crossed note is selected, the whole selection paints together.
            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
            const yesdaw::engine::Tick tickLo = juce::jmin (drag.downTick, drag.currentTick);
            const yesdaw::engine::Tick tickHi = juce::jmax (drag.downTick, drag.currentTick);
            const auto velocityAt = [&drag] (yesdaw::engine::Tick tick) {
                if (drag.currentTick == drag.downTick)
                    return drag.currentVelocity;
                const double t = juce::jlimit (
                    0.0, 1.0,
                    static_cast<double> (tick - drag.downTick)
                        / static_cast<double> (drag.currentTick - drag.downTick));
                return drag.downVelocity + t * (drag.currentVelocity - drag.downVelocity);
            };

            bool crossedSelected = false;
            std::vector<const yesdaw::ui::UiPianoRollNoteView*> crossed;
            for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
            {
                if (note.startTick <= tickHi && note.startTick + note.lengthTicks > tickLo)
                {
                    crossed.push_back (&note);
                    crossedSelected = crossedSelected || note.selected;
                }
            }

            std::vector<std::pair<yesdaw::engine::EntityId, double>> edits;
            if (crossedSelected)
            {
                for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
                    if (note.selected)
                        edits.emplace_back (note.noteId, velocityAt (note.startTick));
            }
            else
            {
                for (const yesdaw::ui::UiPianoRollNoteView* note : crossed)
                    edits.emplace_back (note->noteId, velocityAt (note->startTick));
            }

            if (! edits.empty())
                onVelocityLanePainted (
                    drag.midiClipId,
                    std::span<const std::pair<yesdaw::engine::EntityId, double>> (edits.data(),
                                                                                  edits.size()));
            return;
        }

        if (marqueeState.active)
        {
            const NoteMarqueeState marquee = marqueeState;
            marqueeState = {};
            repaint();

            if (marquee.moved && stateProvider && onNotesMarqueeSelected)
            {
                const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
                const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
                const juce::Rectangle<int> rect (marquee.downPosition, marquee.currentPosition);
                std::vector<yesdaw::engine::EntityId> noteIds;
                for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
                {
                    if (note.key < surface.viewLowKey || note.key > pianoRollViewHighKey (surface))
                        continue;
                    if (pianoRollNoteBounds (geometry, surface, note).intersects (rect))
                        noteIds.push_back (note.noteId);
                }
                onNotesMarqueeSelected (surface.midiClipId,
                                        std::span<const yesdaw::engine::EntityId> (noteIds.data(),
                                                                                   noteIds.size()));
            }
            return;
        }

        if (pendingShiftToggleNoteId.isValid())
        {
            const yesdaw::engine::EntityId toggledNoteId = pendingShiftToggleNoteId;
            pendingShiftToggleNoteId = {};
            if ((! dragState.active || ! dragState.moved) && stateProvider && onNoteToggled)
            {
                dragState = {};
                onNoteToggled (stateProvider().midiClipId, toggledNoteId);
                return;
            }
        }

        if (! dragState.active)
            return;

        const PianoDragState drag = dragState;
        dragState = {};

        if (! drag.moved || ! stateProvider)
            return;

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
        const int deltaX = event.getPosition().x - drag.downPosition.x;
        const int deltaY = event.getPosition().y - drag.downPosition.y;
        // E12: vertical drag transposes — a row of movement is a semitone.
        const float rowHeight = geometry.rowHeight > 1.0f ? geometry.rowHeight : 1.0f;
        const int keyDelta = drag.mode == PianoDragMode::Move && ! drag.copy
            ? -static_cast<int> (std::llround (static_cast<double> (deltaY) / static_cast<double> (rowHeight)))
            : 0;
        if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels && keyDelta == 0)
            return;

        const yesdaw::engine::Tick deltaTicks = pianoRollTickDeltaForPixels (geometry, surface, deltaX);

        if (drag.mode == PianoDragMode::SetLength)
        {
            const yesdaw::engine::Tick maxLength =
                juce::jmax<yesdaw::engine::Tick> (0, surface.timelineLength - drag.startTick);
            const yesdaw::engine::Tick snappedEnd = snappedRollTick (
                surface, drag.startTick + drag.lengthTicks + deltaTicks);
            const yesdaw::engine::Tick nextLength =
                std::clamp<yesdaw::engine::Tick> (snappedEnd - drag.startTick, 0, maxLength);
            if (nextLength != drag.lengthTicks && onNoteLengthChanged)
                onNoteLengthChanged (drag.midiClipId, drag.noteId, nextLength);
            return;
        }

        if (drag.mode == PianoDragMode::TrimHead)
        {
            if (onNoteHeadTrimmed)
                onNoteHeadTrimmed (drag.midiClipId, drag.noteId,
                                   snappedRollTick (surface, drag.startTick + deltaTicks));
            return;
        }

        const yesdaw::engine::Tick maxStart =
            juce::jmax<yesdaw::engine::Tick> (0, surface.timelineLength - drag.lengthTicks);
        // A pure pitch drag (below the horizontal dead zone) must not snap the start sideways.
        const yesdaw::engine::Tick nextStart =
            std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                ? std::clamp<yesdaw::engine::Tick> (snappedRollTick (surface, drag.startTick + deltaTicks),
                                                    0, maxStart)
                : drag.startTick;
        if (nextStart == drag.startTick && keyDelta == 0)
            return;

        if (drag.copy)
        {
            if (onNoteCopyDragged)
                onNoteCopyDragged (drag.midiClipId, drag.noteId, nextStart);
            return;
        }

        if (onNotesDragged)
            onNotesDragged (drag.midiClipId, drag.noteId, nextStart - drag.startTick, keyDelta);
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
            return;
        }

        // Plain double-click deletes the note (E11): the mouse finally has a delete gesture.
        if (onNoteDeleted)
        {
            onNoteDeleted (surface.midiClipId, hit->noteId);
        }
    }

private:
    enum class PianoDragMode
    {
        Move,
        SetLength,
        TrimHead    // E12: drag the left edge — the note end stays fixed
    };

    // E12: note gestures snap through the REAL chooser (no Ctrl inversion in the roll — Ctrl on
    // notes means copy-drag; raw edits come from switching the chooser off).
    [[nodiscard]] static yesdaw::engine::Tick snappedRollTick (
        const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface, yesdaw::engine::Tick tick) noexcept
    {
        if (! surface.snapEnabled || surface.snapGridTicks <= 0)
            return juce::jmax<yesdaw::engine::Tick> (0, tick);

        yesdaw::engine::Tick snapped = tick;
        if (! yesdaw::engine::snapTick (tick, yesdaw::engine::SnapGrid { surface.snapGridTicks }, snapped))
            return juce::jmax<yesdaw::engine::Tick> (0, tick);

        return juce::jmax<yesdaw::engine::Tick> (0, snapped);
    }

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
            if (it->key < surface.viewLowKey || it->key > pianoRollViewHighKey (surface))
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
        const juce::Rectangle<int> bounds = pianoRollNoteBounds (geometry, surface, note);
        // E12: the edge zones only bite on a note wide enough to keep a grabbable middle —
        // otherwise they would swallow a narrow note whole and it could never be moved.
        // Shift+drag stays the length edit for notes of any width.
        if (bounds.getWidth() >= yesdaw::ui::UiTheme::Layout::pianoRollNoteEdgeMinGrabWidth)
        {
            if (std::abs (position.x - bounds.getRight())
                <= yesdaw::ui::UiTheme::Layout::pianoRollNoteEdgeHitWidth)
                return PianoDragMode::SetLength;

            // The LEFT edge trims the note head (end fixed).
            if (std::abs (position.x - bounds.getX())
                <= yesdaw::ui::UiTheme::Layout::pianoRollNoteEdgeHitWidth)
                return PianoDragMode::TrimHead;
        }

        return PianoDragMode::Move;
    }

    // E13: an in-flight velocity-lane paint drag.
    struct VelocityDragState
    {
        bool active = false;
        bool moved = false;
        yesdaw::engine::EntityId midiClipId {};
        juce::Point<int> downPosition;
        yesdaw::engine::Tick downTick = 0;
        yesdaw::engine::Tick currentTick = 0;
        double downVelocity = 0.0;
        double currentVelocity = 0.0;
    };
    VelocityDragState velocityDragState;

    PianoDragState dragState;
    // Piano-roll selection tools (E11): Pointer note marquee + the pending Shift+click toggle
    // that resolves on a movement-free mouse-up (a Shift+DRAG keeps the length-edit law).
    struct NoteMarqueeState
    {
        bool active = false;
        bool moved = false;
        juce::Point<int> downPosition;
        juce::Point<int> currentPosition;
    };
    NoteMarqueeState marqueeState;
    yesdaw::engine::EntityId pendingShiftToggleNoteId;
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
    // N6: persisted per-track heights, one entry per row (0 = auto-shared). Empty or unset means
    // every row auto-shares, exactly like every row did before this field existed.
    std::function<std::vector<int>()> rowHeightsProvider;
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
    // N6: a row-boundary drag — row index, the LIVE candidate height in pixels (already clamped
    // to the legible band). Fired on every drag tick; onRowResizeEnded closes the gesture (E21
    // coalescing, mirroring the mixer fader drag).
    std::function<void (int, int)> onRowResized;
    std::function<void()> onRowResizeEnded;
    // N7: a click on the row's colour swatch (the left accent bar) — row index only, the caller
    // decides the next colour (a fixed-palette cycle, mirroring how a single click commits one
    // undo step for M/S/O).
    std::function<void (int)> onColourSwatchClicked;

    void mouseMove (const juce::MouseEvent& event) override
    {
        setMouseCursor (rowResizeHandleAt (event.getPosition()) >= 0
                             ? juce::MouseCursor::UpDownResizeCursor
                             : juce::MouseCursor::NormalCursor);
    }

    // G1.3: a right-click on a row selects its track and asks for the Track header menu.
    std::function<void (yesdaw::ui::ContextMenuTarget, int, juce::Point<int>)> onContextMenuRequested;

    void requestContextMenu (juce::Point<int> position)
    {
        if (! onContextMenuRequested)
            return;
        const int row = rowAt (position);
        if (row < 0)
            return;
        if (onRowClicked)
            onRowClicked (row);
        onContextMenuRequested (yesdaw::ui::ContextMenuTarget::TrackHeader, row, position);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown())   // the right button itself: on macOS isPopupMenu() also fires for Ctrl+click, which is a gesture modifier here
        {
            requestContextMenu (event.getPosition());
            return;
        }

        dragRow = -1;
        dragZone = MiniZone::None;
        resizeDragRow = -1;

        // N6: a row-boundary grab wins over whatever content sits under it — a real, discoverable
        // resize gesture, not a hidden hotspot.
        if (const int handleRow = rowResizeHandleAt (event.getPosition()); handleRow >= 0)
        {
            const int rows = rowCountProvider ? rowCountProvider() : 0;
            auto area = getLocalBounds();
            area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
            resizeDragRow = handleRow;
            resizeDragStartY = event.getPosition().y;
            resizeDragStartHeightPx = static_cast<int> (
                std::llround (rowGeometry (rows, area.getHeight()).heightFor (handleRow)));
            return;
        }

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

            case MiniZone::Colour:
                if (onColourSwatchClicked)
                    onColourSwatchClicked (row);
                return;

            case MiniZone::None:
                break;
        }

        if (onRowClicked)
            onRowClicked (row);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (resizeDragRow >= 0)
        {
            const int candidate = std::clamp (
                resizeDragStartHeightPx + (event.getPosition().y - resizeDragStartY),
                yesdaw::engine::kTrackHeightMinPx, yesdaw::engine::kTrackHeightMaxPx);
            if (onRowResized)
                onRowResized (resizeDragRow, candidate);
            return;
        }

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
        if (resizeDragRow >= 0)
        {
            resizeDragRow = -1;
            if (onRowResizeEnded)
                onRowResizeEnded();
            return;
        }

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

    // N6: the shared cumulative row law — every row without a persisted height auto-shares
    // whatever space is left, exactly like the historical uniform law when nothing is customized.
    [[nodiscard]] yesdaw::ui::CumulativeRowGeometry rowGeometry (int rows, int availablePixels) const
    {
        const std::vector<int> heights = rowHeightsProvider ? rowHeightsProvider() : std::vector<int> {};
        return yesdaw::ui::computeCumulativeRowGeometry (
            rows, availablePixels, yesdaw::ui::UiTheme::Layout::trackListRowMinHeight,
            static_cast<int> (heights.size()) >= rows ? heights.data() : nullptr);
    }

    [[nodiscard]] juce::Rectangle<int> rowBounds (int row) const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0 || row < 0 || row >= rows)
            return {};

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        const yesdaw::ui::CumulativeRowGeometry geometry = rowGeometry (rows, area.getHeight());
        const int scrollRows = effectiveScrollRows();
        const int rowHeight = static_cast<int> (std::llround (geometry.heightFor (row)));
        const int rowTop = area.getY()
                         + static_cast<int> (std::llround (geometry.top (row) - geometry.top (scrollRows)));
        return { area.getX(), rowTop, area.getWidth(), rowHeight };
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

    // V5: a VERTICAL rect — y controls gain (top = loud), the same orientation law as the mixer
    // strip fader, so the two controls feel like one instrument.
    [[nodiscard]] juce::Rectangle<int> volumeSliderBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withRight (bounds.getRight()
                                 - yesdaw::ui::UiTheme::Layout::trackListLevelColumnRightInset)
                     .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListLevelColumnWidth)
                     .reduced (yesdaw::ui::UiTheme::Space::none,
                               yesdaw::ui::UiTheme::Layout::trackListLevelColumnVerticalInset);
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

    // N7: the row's colour swatch IS the left accent bar — mirrors drawTrackList's accent-bar
    // rectangle exactly, so paint and hit-test cannot drift (the same law N6 established for
    // every other rail control).
    [[nodiscard]] juce::Rectangle<int> colourSwatchBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withWidth (yesdaw::ui::UiTheme::Layout::trackListAccentWidth)
                     .reduced (yesdaw::ui::UiTheme::Layout::trackListAccentHorizontalInset,
                               yesdaw::ui::UiTheme::Layout::trackListAccentVerticalInset);
    }

private:
    enum class MiniZone : std::uint8_t { None, Pan, Volume, Mute, Solo, Meter, Colour };

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
        // V5: the fader is vertical, so the hit slop widens it sideways (the vertical extent is
        // already the full column).
        if (volumeSliderBounds (row).expanded (yesdaw::ui::UiTheme::Layout::trackListLevelHitSlopX,
                                               yesdaw::ui::UiTheme::Space::none)
                .contains (position))
            return MiniZone::Volume;
        if (muteCellBounds (row).contains (position))
            return MiniZone::Mute;
        if (soloCellBounds (row).contains (position))
            return MiniZone::Solo;
        if (meterZoneBounds (row).contains (position))
            return MiniZone::Meter;
        if (colourSwatchBounds (row).contains (position))
            return MiniZone::Colour;
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
        if (slider.getHeight() <= 0)
            return;

        // V5: vertical law — the top of the column is unity, the bottom is silence, exactly the
        // mixer fader's orientation.
        const float normalized = 1.0f
                               - static_cast<float> (position.y - slider.getY())
                                     / static_cast<float> (slider.getHeight());
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
        fineDragLastY = event.getPosition().y;
        return true;
    }

    void applyFineDrag (const juce::MouseEvent& event)
    {
        const auto bounds = dragZone == MiniZone::Pan ? panKnobBounds (dragRow)
                                                      : volumeSliderBounds (dragRow);
        // V5: the fine axis follows each control's own coarse axis — pan stays horizontal, the
        // now-vertical VOL fader moves on y (upward = louder, matching applyVolume's law).
        double proportionDelta = 0.0;
        if (dragZone == MiniZone::Pan)
        {
            if (bounds.getWidth() <= 0)
                return;
            const int x = event.getPosition().x;
            proportionDelta = static_cast<double> (x - fineDragLastX)
                            / static_cast<double> (bounds.getWidth());
            fineDragLastX = x;
        }
        else
        {
            if (bounds.getHeight() <= 0)
                return;
            const int y = event.getPosition().y;
            proportionDelta = static_cast<double> (fineDragLastY - y)
                            / static_cast<double> (bounds.getHeight());
            fineDragLastY = y;
        }

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
    int fineDragLastY = 0;   // V5: the vertical VOL fader's fine axis

    // N6: row-boundary height-resize gesture state — kept separate from dragRow/dragZone (the
    // mini-control drags above) since a boundary drag can start even when the pointer isn't over
    // any zone at all.
    int resizeDragRow = -1;
    int resizeDragStartY = 0;
    int resizeDragStartHeightPx = 0;

    // N6: which row's BOTTOM edge `position` is within the grab tolerance of, or -1. Checked
    // before ordinary row/zone hit-testing so the boundary always wins over whatever content sits
    // under it.
    [[nodiscard]] int rowResizeHandleAt (juce::Point<int> position) const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0)
            return -1;

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        if (position.x < area.getX() || position.x >= area.getRight())
            return -1;

        const yesdaw::ui::CumulativeRowGeometry geometry = rowGeometry (rows, area.getHeight());
        const int scrollRows = effectiveScrollRows();
        constexpr int kGrabTolerancePixels = 4;
        for (int row = scrollRows; row < rows; ++row)
        {
            const int rowBottom = area.getY()
                                 + static_cast<int> (std::llround (
                                       geometry.top (row + 1) - geometry.top (scrollRows)));
            if (rowBottom > area.getBottom())
                break;
            if (std::abs (position.y - rowBottom) <= kGrabTolerancePixels)
                return row;
        }
        return -1;
    }

    [[nodiscard]] int rowAt (juce::Point<int> position) const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0)
            return -1;

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        if (! area.contains (position))
            return -1;

        const yesdaw::ui::CumulativeRowGeometry geometry = rowGeometry (rows, area.getHeight());
        const int scrollRows = effectiveScrollRows();
        const int row = geometry.rowAtPixel (
            static_cast<double> (position.y - area.getY()) + geometry.top (scrollRows));
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
        // N6: visibleRows stays an AUTO-SHARE-height approximation (matching the timeline's own
        // scroll-count heuristic) even with custom heights present — an honest, minor imprecision
        // rather than a full non-uniform scroll-extent solve, which the gate does not require.
        const yesdaw::ui::CumulativeRowGeometry geometry = rowGeometry (rows, area.getHeight());
        const int visibleRows = std::max (1, area.getHeight() / std::max (1, geometry.autoShare));
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
    // R16: each point carries its own curve shape so the painted line shows the REAL law the
    // engine evaluates (a segment's shape belongs to its LEFT point, the evaluator's rule).
    struct CanvasPoint
    {
        double seconds = 0.0;
        double value = 0.0;
        yesdaw::engine::AutomationCurveType curve = yesdaw::engine::AutomationCurveType::Linear;
    };

    std::function<std::vector<CanvasPoint>()> pointsProvider;
    std::function<double (int)> secondsForLocalX;
    std::function<int (double)> localXForSeconds;
    std::function<void (double, double)> onAddPoint;
    std::function<void (double, double, double)> onMovePoint;   // oldSeconds, newSeconds, newValue
    std::function<void (double)> onDeletePoint;
    // R16: Alt+click a handle cycles its curve Linear→Hold→Bezier→Log.
    std::function<void (double)> onCycleCurvePoint;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (yesdaw::ui::UiTheme::Color::controlInset());
        if (! pointsProvider || ! localXForSeconds)
            return;

        const std::vector<CanvasPoint> points = pointsProvider();
        g.setColour (yesdaw::ui::UiTheme::Color::accentPurple());
        juce::Path line;
        for (std::size_t i = 0; i + 1u < points.size(); ++i)
        {
            const juce::Point<float> from { static_cast<float> (localXForSeconds (points[i].seconds)),
                                            yForValue (points[i].value) };
            const juce::Point<float> to { static_cast<float> (localXForSeconds (points[i + 1u].seconds)),
                                          yForValue (points[i + 1u].value) };
            line.startNewSubPath (from);
            if (points[i].curve == yesdaw::engine::AutomationCurveType::Hold)
            {
                // A hold paints as the step it is: flat to the next tick, then vertical.
                line.lineTo (to.x, from.y);
                line.lineTo (to);
            }
            else
            {
                // Linear/Bezier/Log sample the engine's own curve law so the picture can never
                // drift from what actually renders.
                constexpr int kSteps = 16;
                for (int step = 1; step <= kSteps; ++step)
                {
                    const double t = static_cast<double> (step) / kSteps;
                    const double u = yesdaw::engine::automationCurveProgress (points[i].curve, t);
                    line.lineTo (from.x + (to.x - from.x) * static_cast<float> (t),
                                 from.y + (to.y - from.y) * static_cast<float> (u));
                }
            }
        }
        g.strokePath (line, juce::PathStrokeType (yesdaw::ui::UiTheme::Layout::automationCanvasLineWidth));

        for (const CanvasPoint& point : points)
        {
            const float radius = static_cast<float> (yesdaw::ui::UiTheme::Layout::automationCanvasHandleRadius);
            g.fillEllipse (static_cast<float> (localXForSeconds (point.seconds)) - radius,
                           yForValue (point.value) - radius,
                           radius + radius,
                           radius + radius);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragOldSeconds.reset();
        if (const std::optional<double> hit = handleSecondsAt (event.getPosition()))
        {
            // R16: Alt+click cycles the hit point's curve shape instead of starting a drag.
            if (event.mods.isAltDown())
            {
                if (onCycleCurvePoint)
                    onCycleCurvePoint (*hit);
                return;
            }

            dragOldSeconds = hit;
            return;
        }

        if (event.mods.isAltDown())
            return;   // Alt is the curve gesture — never an accidental add

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
        for (const CanvasPoint& point : pointsProvider())
        {
            const juce::Point<int> at { localXForSeconds (point.seconds),
                                        static_cast<int> (yForValue (point.value)) };
            if (position.getDistanceFrom (at) <= hitRadius)
                return point.seconds;
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
    // E17: double-click opens the inline bus rename editor when the strip is a bus.
    std::function<void (int)> onStripDoubleClicked;
    // Painted meter hit test (B32): a click inside a track strip's painted meter clears its
    // held peak and latched clip light instead of retargeting the strip.
    std::function<int (juce::Point<int>)> meterStripAtPosition;
    std::function<void (int)> onMeterClicked;
    // M4: painted insert-slot hit test — a click on a slot row selects the strip and that slot.
    std::function<std::pair<int, int> (juce::Point<int>)> insertSlotAtPosition;
    std::function<void (int, int)> onInsertSlotClicked;
    // M5: painted send rows — press picks the row, drag sets the level, release commits ONE
    // undoable edit (a per-pixel commit would bury the undo stack).
    std::function<std::pair<int, int> (juce::Point<int>)> sendRowAtPosition;
    std::function<double (int, int, juce::Point<int>)> sendLevelForPosition;
    std::function<void (int, int, double, bool)> onSendRowDragged;
    // N1: painted Mute/Solo cells — a click toggles THAT strip without stealing the selection.
    std::function<std::pair<int, int> (juce::Point<int>)> muteSoloCellAtPosition;
    std::function<void (int, int)> onMuteSoloCellClicked;

    // G1.3: a right-click on a strip selects it and asks for the Mixer strip menu (the master
    // strip has none; insert slots come with their own list in the next checkpoint).
    std::function<void (yesdaw::ui::ContextMenuTarget, int, juce::Point<int>)> onContextMenuRequested;
    std::function<int ()> stripCountProvider;   // track + bus strips (the master is the next index)

    void requestContextMenu (juce::Point<int> position)
    {
        if (! onContextMenuRequested || ! stripAtPosition || getParentComponent() == nullptr)
            return;
        const juce::Point<int> shellPosition = position + getPosition();
        // An insert slot first: the click selects the strip and the slot (the same law as the
        // left-click), and the slot's own menu follows.
        if (insertSlotAtPosition && onInsertSlotClicked)
        {
            const auto [slotStrip, slotIndex] = insertSlotAtPosition (shellPosition);
            if (slotStrip >= 0 && slotIndex >= 0)
            {
                onInsertSlotClicked (slotStrip, slotIndex);
                onContextMenuRequested (yesdaw::ui::ContextMenuTarget::InsertSlot, slotIndex, position);
                return;
            }
        }
        const int strip = stripAtPosition (shellPosition);
        const int stripCount = stripCountProvider ? stripCountProvider() : 0;
        if (strip < 0 || strip >= stripCount)
            return;
        if (onStripClicked)
            onStripClicked (strip);
        onContextMenuRequested (yesdaw::ui::ContextMenuTarget::MixerStrip, strip, position);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();

        if (event.mods.isRightButtonDown())   // the right button itself: on macOS isPopupMenu() also fires for Ctrl+click, which is a gesture modifier here
        {
            requestContextMenu (event.getPosition());
            return;
        }

        if (muteSoloCellAtPosition && onMuteSoloCellClicked)
        {
            const auto [cellStrip, cellIndex] = muteSoloCellAtPosition (shellPosition);
            if (cellStrip >= 0 && cellIndex >= 0)
            {
                onMuteSoloCellClicked (cellStrip, cellIndex);
                return;
            }
        }

        if (sendRowAtPosition && sendLevelForPosition && onSendRowDragged)
        {
            const auto [sendStrip, sendIndex] = sendRowAtPosition (shellPosition);
            if (sendStrip >= 0 && sendIndex >= 0)
            {
                draggingSendStrip = sendStrip;
                draggingSendIndex = sendIndex;
                onSendRowDragged (sendStrip, sendIndex,
                                  sendLevelForPosition (sendStrip, sendIndex, shellPosition), false);
                return;
            }
        }

        if (insertSlotAtPosition && onInsertSlotClicked)
        {
            const auto [slotStrip, slotIndex] = insertSlotAtPosition (shellPosition);
            if (slotStrip >= 0 && slotIndex >= 0)
            {
                onInsertSlotClicked (slotStrip, slotIndex);
                return;
            }
        }

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

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (draggingSendStrip < 0 || ! sendLevelForPosition || ! onSendRowDragged)
            return;

        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();
        onSendRowDragged (draggingSendStrip, draggingSendIndex,
                          sendLevelForPosition (draggingSendStrip, draggingSendIndex, shellPosition), false);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (draggingSendStrip < 0 || ! sendLevelForPosition || ! onSendRowDragged)
            return;

        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();
        onSendRowDragged (draggingSendStrip, draggingSendIndex,
                          sendLevelForPosition (draggingSendStrip, draggingSendIndex, shellPosition), true);
        draggingSendStrip = -1;
        draggingSendIndex = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! stripAtPosition || ! onStripDoubleClicked)
            return;

        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();
        const int strip = stripAtPosition (shellPosition);
        if (strip >= 0)
            onStripDoubleClicked (strip);
    }

private:
    int draggingSendStrip = -1;
    int draggingSendIndex = -1;
};

// G0.4 (ADR-0046 §6 "nothing is blind", plan §5.2 layered rendering): the playhead lives on its
// own transparent layer ABOVE the buffered timeline canvas, painted by the SAME drawPlayhead law
// and the SAME geometry the canvas would use. A moving playhead therefore repaints a cheap
// overlay thirty times a second instead of re-rendering every clip and waveform.
class PlayheadLayerComponent final : public juce::Component
{
public:
    std::function<yesdaw::ui::TimelineCanvasState()> stateProvider;

    PlayheadLayerComponent()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (false);
        setWantsKeyboardFocus (false);
    }

    void paint (juce::Graphics& g) override
    {
        if (! stateProvider)
            return;
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        yesdaw::ui::timeline_canvas_detail::drawPlayhead (g, geometry.rulerArea, geometry.clipArea, state, geometry.viewport);
    }
};

class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      public juce::KeyListener,    // G0.2: the command router on the top-level window
                      private juce::Timer,
                      private juce::AudioIODeviceCallback,
                      private juce::MidiInputCallback
{
public:
    explicit MainComponent (yesdaw::ui::MainComponentFileChoices choices, bool enableDesktopAudio)
        : fileChoices (std::move (choices)), desktopAudioRequested (enableDesktopAudio)
    {
        if (! fileChoices.sessionStateDirectory.empty())
            appModel.setSessionStateDirectory (fileChoices.sessionStateDirectory);

        // G0.1 State probe: debug-only; a normal launch leaves the path empty and writes nothing.
        stateProbePath = fileChoices.stateProbePath;
        launchStamp = std::chrono::steady_clock::now();

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
                repaintAll();
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
            repaintAll();
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
            repaintAll();
        };
        addAndMakeVisible (exportAudioCancelButton);

        configureActionComponent (masterLoudnessReadout, yesdaw::ui::UiActionId::MixerReadLoudness, "Master loudness");
        masterLoudnessReadout.setButtonText ("-- LUFS");
        masterLoudnessReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        masterLoudnessReadout.setColour (juce::TextButton::textColourOffId, kText);
        masterLoudnessReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadLoudness);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (masterLoudnessReadout);

        timelineInput.setComponentID (kTimelineComponentId);
        timelineInput.setTooltip ("Timeline: drag clips, drag the ruler to select a range, Shift-drag for the loop");
        timelineInput.setName ("Timeline");
        timelineInput.setTitle ("Timeline");
        // G0.4: the canvas is the STATIC layer — cached as an image and re-rendered only when
        // repaintAll() (any model/view change) or the canvas's own gesture repaints invalidate
        // it. It never paints the playhead; the layer above does, every tick, from the same law.
        timelineInput.stateProvider = [this] {
            yesdaw::ui::TimelineCanvasState state = makeTimelineState();
            state.paintPlayhead = false;
            return state;
        };
        timelineInput.setBufferedToImage (true);
        playheadLayer.stateProvider = [this] { return makeTimelineState(); };
        // A paint layer, not a control: no component id (the tooltip / dead-affordance laws
        // enumerate identified children), a name for the render-budget gate.
        playheadLayer.setName ("Playhead layer");
        timelineInput.activeToolProvider = [this] {
            return appModel.context().activeTimelineTool;
        };
        timelineInput.onZoomToolClicked = [this] (double anchorSeconds, bool zoomOut) {
            const double factor = yesdaw::ui::UiTheme::Layout::timelineZoomToolClickFactor;
            zoomTimelineAtAnchor (anchorSeconds, zoomOut ? 1.0 / factor : factor);
            repaintAll();
        };
        timelineInput.onHandToolScrolled = [this] (double secondsDelta) {
            timelineScrollSeconds += secondsDelta;
            repaintAll();
        };
        timelineInput.onVerticalScrollRows = [this] (int rowDelta) {
            scrollTrackRowsBy (rowDelta);
        };
        // M10: OS file drops land on the track under the pointer at the snapped tick under the
        // pointer. Several files go onto consecutive lanes, each import its own undo step
        // (R8); anything the WAV reader refuses is reported on the status line (R6) and
        // changes nothing.
        timelineInput.filesAreImportable = [this] (const juce::StringArray& files) {
            if (! appModel.context().projectLoaded || files.isEmpty())
                return false;

            for (const juce::String& file : files)
                if (juce::File (file).hasFileExtension ("wav"))
                    return true;

            return false;
        };
        timelineInput.onFilesDropped = [this] (const juce::StringArray& files, int lane, double seconds) {
            const auto& tracks = appModel.project().tracks;
            if (lane < 0 || lane >= static_cast<int> (tracks.size()))
                return;

            const auto tick = timelineTickFromSeconds (seconds);
            if (! tick.has_value())
                return;

            const yesdaw::engine::Tick start = snappedTimelineTick (*tick, false);
            int laneOffset = 0;
            bool anyImported = false;
            std::string refusedNames;
            for (const juce::String& file : files)
            {
                const std::filesystem::path path (file.toStdString());
                auto decoded = decodeProjectWav (path);
                if (! decoded)
                {
                    refusedNames += (refusedNames.empty() ? "" : ", ") + path.filename().string();
                    continue;
                }

                const int targetLane = std::min (lane + laneOffset,
                                                 static_cast<int> (appModel.project().tracks.size()) - 1);
                const yesdaw::engine::EntityId trackId =
                    appModel.project().tracks[static_cast<std::size_t> (targetLane)].id;
                // R7: a verb failure (rate mismatch, bundle copy, …) reports its own precise
                // reason inside the model — only decoder refusals are the shell's to name.
                if (appModel.importAudioFileAt (path, std::move (*decoded), trackId, start).ok())
                {
                    anyImported = true;
                    ++laneOffset;
                }
            }

            // R6: every file the WAV reader refused is named immediately — the good files
            // still landed.
            if (! refusedNames.empty())
                appModel.reportStatus ("Import refused (WAV only, stereo max): " + refusedNames, true);

            if (anyImported)
                selectedTrackLane = lane;

            refreshActionState();
            repaintAll();
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
            repaintAll();
        };
        timelineInput.onContextMenuRequested = [this] (yesdaw::ui::ContextMenuTarget target, int index, juce::Point<int> position) {
            if (target == yesdaw::ui::ContextMenuTarget::EmptyLane)
                selectTrackLane (index);   // the lane's track becomes the selection first
            openContextMenu (target, index, timelineInput, position);
        };
        timelineInput.onClipClicked = [this] (int timelineClipId, bool toggle) {
            selectTimelineClipByLayoutId (timelineClipId, toggle);
        };
        timelineInput.onEmptyClicked = [this] {
            appModel.clearTimelineClipSelection();
            refreshActionState();
            repaintAll();
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
            repaintAll();
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
        // E9: a double-clicked MIDI clip opens the piano roll on THAT clip (audio clips keep the
        // historical double-click split behavior through onClipSplit).
        timelineInput.onClipDoubleClicked = [this] (int timelineClipId) {
            if (timelineClipId < 0 || timelineClipId >= static_cast<int> (timelineClipIds.size()))
                return false;
            const yesdaw::engine::EntityId entityId = timelineClipIds[static_cast<std::size_t> (timelineClipId)];
            if (! appModel.openPianoRollOnMidiClip (entityId))
                return false;

            refreshActionState();
            resized();
            repaintAll();
            return true;
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
            repaintAll();
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
            repaintAll();
        };
        timelineInput.onScrollWheel = [this] (double wheelDelta) {
            const double visibleSeconds = std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                    timelineTotalSeconds / std::max (1.0, timelineZoomFactor));
            timelineScrollSeconds -= wheelDelta * visibleSeconds
                                   * yesdaw::ui::UiTheme::Layout::timelineScrollWheelFraction;
            repaintAll();
        };
        timelineInput.onRulerAltClicked = [this] (double seconds) {
            if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds))
            {
                (void) appModel.removeTimelineMarkerNearestTick (*tick);
                refreshActionState();
                repaintAll();
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
                    repaintAll();
                }
            }
        };
        // N8: Alt+Shift-drag on the ruler sets the punch region; a degenerate (non-positive)
        // span — dragging back onto the start point, effectively a click — clears it instead, so
        // the SAME gesture that creates a punch region can remove one.
        timelineInput.onPunchRegionDragged = [this] (double startSeconds, double endSeconds, bool snapInvert) {
            const std::optional<yesdaw::engine::Tick> startFrame = timelineTickFromSeconds (startSeconds);
            const std::optional<yesdaw::engine::Tick> endFrame = timelineTickFromSeconds (endSeconds);
            if (! startFrame || ! endFrame)
                return;
            const yesdaw::engine::Tick snappedStart = snappedTimelineTick (*startFrame, snapInvert);
            const yesdaw::engine::Tick snappedEnd = snappedTimelineTick (*endFrame, snapInvert);
            if (snappedEnd > snappedStart)
                (void) appModel.setPunchRegion (true, snappedStart, snappedEnd);
            else
                (void) appModel.setPunchRegion (false, 0, 0);
            refreshActionState();
            repaintAll();
        };
        timelineInput.onTimelineLocated = [this] (double seconds) {
            if (const std::optional<yesdaw::engine::Tick> frame = timelineTickFromSeconds (seconds))
            {
                (void) appModel.locatePlaybackFrame (*frame);
                refreshActionState();
                repaintAll();
            }
        };
        // Marker edits (E7): the dragged label commits a snapped MoveMarker; double-click opens
        // the inline rename editor over the painted label.
        timelineInput.onMarkerDragged = [this] (int markerIndex, double seconds, bool snapInvert) {
            const auto& markers = appModel.project().markers;
            if (markerIndex < 0 || markerIndex >= static_cast<int> (markers.size()))
                return;
            if (const auto tick = timelineTickFromSeconds (std::max (0.0, seconds)))
            {
                (void) appModel.moveTimelineMarkerTo (
                    markers[static_cast<std::size_t> (markerIndex)].id,
                    snappedTimelineTick (*tick, snapInvert));
                refreshActionState();
                repaintAll();
            }
        };
        timelineInput.onMarkerRenameRequested = [this] (int markerIndex) {
            openMarkerRenameEditor (markerIndex);
        };
        // Loop brace edits (E6): the dragged edge (or the move anchor) snaps through the snap
        // chooser; the fixed edge keeps its exact frames, and a move preserves the span exactly.
        timelineInput.onLoopBraceEdited = [this] (TimelineInputComponent::LoopBraceEdit kind,
                                                  double pointerSeconds,
                                                  double grabOffsetSeconds,
                                                  bool snapInvert) {
            const std::int64_t loopStart = appModel.playbackLoopStartFrame();
            const std::int64_t loopEnd = appModel.playbackLoopEndFrame();
            if (loopEnd <= loopStart)
                return;

            bool edited = false;
            if (kind == TimelineInputComponent::LoopBraceEdit::Start)
            {
                if (const auto tick = timelineTickFromSeconds (std::max (0.0, pointerSeconds)))
                {
                    const yesdaw::engine::Tick snapped = snappedTimelineTick (*tick, snapInvert);
                    if (static_cast<std::int64_t> (snapped) < loopEnd)
                        edited = appModel.setPlaybackLoopRegion (snapped, loopEnd).dispatched;
                }
            }
            else if (kind == TimelineInputComponent::LoopBraceEdit::End)
            {
                if (const auto tick = timelineTickFromSeconds (std::max (0.0, pointerSeconds)))
                {
                    const yesdaw::engine::Tick snapped = snappedTimelineTick (*tick, snapInvert);
                    if (static_cast<std::int64_t> (snapped) > loopStart)
                        edited = appModel.setPlaybackLoopRegion (loopStart, snapped).dispatched;
                }
            }
            else if (kind == TimelineInputComponent::LoopBraceEdit::Move)
            {
                if (const auto tick = timelineTickFromSeconds (
                        std::max (0.0, pointerSeconds - grabOffsetSeconds)))
                {
                    const std::int64_t span = loopEnd - loopStart;
                    const yesdaw::engine::Tick snapped = snappedTimelineTick (*tick, snapInvert);
                    edited = appModel.setPlaybackLoopRegion (snapped,
                                                             static_cast<std::int64_t> (snapped) + span)
                                 .dispatched;
                }
            }

            if (edited)
            {
                refreshActionState();
                repaintAll();
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
                    repaintAll();
                }
            }
        };
        timelineInput.onRulerRangeCleared = [this] {
            appModel.clearTimelineRangeSelection();
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (timelineInput);
        addAndMakeVisible (playheadLayer);   // G0.4: z-order above the buffered canvas

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
        trackListInput.rowHeightsProvider = [this] {
            std::vector<int> heights;
            if (appModel.context().projectLoaded)
            {
                heights.reserve (appModel.project().tracks.size());
                for (const yesdaw::engine::Track& track : appModel.project().tracks)
                    heights.push_back (track.heightPx);
            }
            return heights;
        };
        trackListInput.rowScrollProvider = [this] { return timelineTrackScrollRows; };
        trackListInput.onVerticalScrollRows = [this] (int rowDelta) { scrollTrackRowsBy (rowDelta); };
        trackListInput.onRowClicked = [this] (int row) { selectTrackLane (row); };
        trackListInput.onContextMenuRequested = [this] (yesdaw::ui::ContextMenuTarget target, int index, juce::Point<int> position) {
            openContextMenu (target, index, trackListInput, position);
        };
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
        // E21: rail mini drags bracket a strip gesture so one drag is ONE undo step; the rail's
        // every-mouse-up signal closes it (plain clicks stay single steps either way).
        trackListInput.onPanEdited = [this] (int row, float pan) {
            appModel.beginStripGesture();
            selectTrackLane (row);
            if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
                (void) appModel.setSelectedMixerPan (pan);
            refreshActionState();
            repaintAll();
        };
        trackListInput.onVolumeEdited = [this] (int row, float linearGain) {
            appModel.beginStripGesture();
            selectTrackLane (row);
            if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
                (void) appModel.setSelectedMixerFader (linearGain);
            showDragDbReadout (trackListInput.volumeSliderBounds (row)
                                   .translated (trackListInput.getX(), trackListInput.getY()),
                               linearGain);
            refreshActionState();
            repaintAll();
        };
        trackListInput.onMiniDragEnded = [this] {
            appModel.endStripGesture();
            hideDragDbReadout();
        };
        // N6: the row-boundary height drag — E21 coalescing (beginStripGesture on every tick,
        // closed once on release) so the whole drag is one undo step, matching the fader pattern.
        trackListInput.onRowResized = [this] (int row, int heightPx) {
            const auto& tracks = appModel.project().tracks;
            if (row < 0 || row >= static_cast<int> (tracks.size()))
                return;
            appModel.beginStripGesture();
            (void) appModel.setTrackHeight (tracks[static_cast<std::size_t> (row)].id, heightPx);
            refreshActionState();
            repaintAll();
        };
        trackListInput.onRowResizeEnded = [this] { appModel.endStripGesture(); };
        trackListInput.onMeterClicked = [this] (int row) { clearTrackMeterHold (row); };
        trackListInput.onMuteToggled = [this] (int row) {
            selectTrackLane (row);
            if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
                (void) appModel.toggleSelectedMixerMute();
            refreshActionState();
            repaintAll();
        };
        trackListInput.onSoloToggled = [this] (int row) {
            selectTrackLane (row);
            if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
                (void) appModel.toggleSelectedMixerSolo();
            refreshActionState();
            repaintAll();
        };
        // N7: one click on a row's colour swatch commits ONE undo step, advancing THAT track
        // (not necessarily the selected one) to the next colour in the fixed cycle.
        trackListInput.onColourSwatchClicked = [this] (int row) {
            const auto& tracks = appModel.project().tracks;
            if (row < 0 || row >= static_cast<int> (tracks.size()))
                return;
            const auto& track = tracks[static_cast<std::size_t> (row)];
            (void) appModel.setTrackColour (track.id, nextTrackColourInCycle (track.colour));
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (trackListInput);

        // Header tempo + time-signature editing (usable-DAW P0): the painted readouts become real
        // undoable controls. Tempo is a drag/scrub bar over the TEMPO cell; meter picks common signatures.
        configureActionComponent (headerTempoControl, yesdaw::ui::UiActionId::TransportSetTempo, "Set tempo");
        headerTempoControl.setSliderStyle (juce::Slider::LinearBar);
        // E24: the LinearBar shows its value inside the bar; the old separate TextBoxLeft was
        // narrower than "120.00" and the thumb painted over the clipped digits.
        headerTempoControl.setTextBoxStyle (juce::Slider::NoTextBox,
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
            repaintAll();
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
            repaintAll();
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
            repaintAll();
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

        // Marker rename editor (E7): same inline pattern as the clip and track editors.
        markerRenameEditor.setComponentID ("shell.timeline.marker.rename");
        markerRenameEditor.setTooltip ("Rename marker: Enter commits, Escape cancels");
        markerRenameEditor.setName ("Rename marker");
        markerRenameEditor.setSelectAllWhenFocused (true);
        markerRenameEditor.onReturnKey = [this] { commitMarkerRenameEditor(); };
        markerRenameEditor.onEscapeKey = [this] { dismissMarkerRenameEditor(); };
        markerRenameEditor.onFocusLost = [this] { dismissMarkerRenameEditor(); };
        addChildComponent (markerRenameEditor);

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
            repaintAll();
        };
        addAndMakeVisible (timelineSnapChooser);

        // G1.4: the Nudge value chooser — four registered verbs as one control.
        nudgeValueChooser.setComponentID ("timeline.nudge.chooser");
        nudgeValueChooser.setName ("Nudge value");
        nudgeValueChooser.setTitle ("Nudge value");
        nudgeValueChooser.setTooltip ("Nudge value: the distance Alt+Left / Alt+Right move the selection");
        nudgeValueChooser.addItem ("Nudge: Grid", 1);
        nudgeValueChooser.addItem ("Nudge: Bar", 2);
        nudgeValueChooser.addItem ("Nudge: Beat", 3);
        nudgeValueChooser.addItem ("Nudge: 1/16", 4);
        nudgeValueChooser.setSelectedId (1, juce::dontSendNotification);
        nudgeValueChooser.onChange = [this] {
            if (refreshingNudgeChooser)
                return;
            const int selected = nudgeValueChooser.getSelectedId();
            const yesdaw::ui::UiActionId action =
                selected == 2 ? yesdaw::ui::UiActionId::EditNudgeValueBar
                : selected == 3 ? yesdaw::ui::UiActionId::EditNudgeValueBeat
                : selected == 4 ? yesdaw::ui::UiActionId::EditNudgeValueSixteenth
                                : yesdaw::ui::UiActionId::EditNudgeValueGrid;
            (void) appModel.dispatch (action);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (nudgeValueChooser);

        // G1.4: the Inspector toggle (I) at the toolbar's right end.
        configureActionComponent (inspectorToggle, yesdaw::ui::UiActionId::ViewToggleInspector, "Inspector");
        inspectorToggle.setButtonText ("Inspector");
        inspectorToggle.setClickingTogglesState (false);
        inspectorToggle.onClick = [this] {
            handleAction (yesdaw::ui::UiActionId::ViewToggleInspector);
            refreshActionState();
            resized();
            repaintAll();
        };
        addAndMakeVisible (inspectorToggle);

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
        configureMixerDockToggle();
        configureInspectorTabs();
        configureTimelineZoomControls();

        // Automation lane canvas (usable-DAW P1): breakpoints drawn and edited against the SAME
        // timeline viewport math as the arrangement; targets the selected track's fader lane.
        automationLaneCanvas.setComponentID ("timeline.automation.canvas");
        automationLaneCanvas.setTooltip ("Automation lane: click to add a breakpoint, drag to move it");
        automationLaneCanvas.setName ("Automation Lane");
        automationLaneCanvas.setTitle ("Automation Lane");
        // E20: the canvas reads and edits the CHOSEN target's lane (fader/pan/send/FX param).
        automationLaneCanvas.pointsProvider = [this] {
            std::vector<AutomationLaneCanvasComponent::CanvasPoint> points;
            const AutomationTargetOption target = currentAutomationTarget();
            if (! target.ownerEntity.isValid() || ! appModel.project().sampleRate.isValid())
                return points;

            if (const yesdaw::engine::AutomationLaneData* const lane =
                    appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId))
            {
                const double sampleRateHz = appModel.project().sampleRate.hz;
                points.reserve (lane->points.size());
                for (const yesdaw::engine::AutomationBreakpoint& point : lane->points)
                    points.push_back ({ static_cast<double> (point.tick) / sampleRateHz,
                                        point.value,
                                        point.curveType });
            }
            return points;
        };
        // R16: Alt+click a breakpoint handle cycles its curve shape through the undoable
        // SetAutomationBreakpointCurve verb — Linear → Hold → Bezier → Log → Linear.
        automationLaneCanvas.onCycleCurvePoint = [this] (double seconds) {
            const AutomationTargetOption target = currentAutomationTarget();
            const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
                ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
                : nullptr;
            if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds);
                lane != nullptr && tick)
            {
                (void) appModel.cycleAutomationBreakpointCurveAtTick (lane->id, *tick);
                refreshActionState();
                repaintAll();
            }
        };
        automationLaneCanvas.secondsForLocalX = [this] (int localX) {
            return automationCanvasSecondsForLocalX (localX);
        };
        automationLaneCanvas.localXForSeconds = [this] (double seconds) {
            return automationCanvasLocalXForSeconds (seconds);
        };
        // E20: added and dragged breakpoints land on the snap chooser's grid (chooser Off = raw).
        automationLaneCanvas.onAddPoint = [this] (double seconds, double value) {
            const AutomationTargetOption target = currentAutomationTarget();
            if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds);
                tick && target.ownerEntity.isValid())
            {
                (void) appModel.addAutomationBreakpointToLane (
                    target.ownerEntity, target.role, target.paramId,
                    snappedTimelineTick (*tick, false), value);
                refreshActionState();
                repaintAll();
            }
        };
        automationLaneCanvas.onMovePoint = [this] (double oldSeconds, double newSeconds, double newValue) {
            const AutomationTargetOption target = currentAutomationTarget();
            const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
                ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
                : nullptr;
            const std::optional<yesdaw::engine::Tick> oldTick = timelineTickFromSeconds (oldSeconds);
            const std::optional<yesdaw::engine::Tick> newTick = timelineTickFromSeconds (newSeconds);
            if (lane != nullptr && oldTick && newTick)
            {
                (void) appModel.moveAutomationBreakpointTo (lane->id, *oldTick,
                                                            snappedTimelineTick (*newTick, false),
                                                            newValue);
                refreshActionState();
                repaintAll();
            }
        };
        automationLaneCanvas.onDeletePoint = [this] (double seconds) {
            const AutomationTargetOption target = currentAutomationTarget();
            const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
                ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
                : nullptr;
            if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds);
                lane != nullptr && tick)
            {
                (void) appModel.removeAutomationBreakpointAtTick (lane->id, *tick);
                refreshActionState();
                repaintAll();
            }
        };
        addChildComponent (automationLaneCanvas);

        pianoRollInput.setComponentID (kPianoRollComponentId);
        pianoRollInput.setTooltip ("Piano roll: click to pencil a note, drag to move, Ctrl+drag to copy, Alt+wheel for velocity");
        pianoRollInput.setName ("Piano Roll");
        pianoRollInput.setTitle ("Piano Roll");
        pianoRollInput.stateProvider = [this] { return currentPianoRollSurface(); };
        pianoRollInput.onContextMenuRequested = [this] (yesdaw::ui::ContextMenuTarget target, int index, juce::Point<int> position) {
            openContextMenu (target, index, pianoRollInput, position);
        };
        pianoRollInput.onNoteClicked = [this] (yesdaw::engine::EntityId midiClipId,
                                               yesdaw::engine::EntityId noteId) {
            // E12: a plain press on a selected member keeps the group for the drag.
            (void) appModel.selectPianoRollNoteForGesture (midiClipId, noteId);
            refreshActionState();
            repaintAll();
        };
        // Piano-roll selection tools (E11).
        pianoRollInput.activeToolProvider = [this] {
            return appModel.context().activeTimelineTool;
        };
        pianoRollInput.onNoteToggled = [this] (yesdaw::engine::EntityId midiClipId,
                                               yesdaw::engine::EntityId noteId) {
            (void) appModel.togglePianoRollNoteSelection (midiClipId, noteId);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNotesMarqueeSelected = [this] (yesdaw::engine::EntityId midiClipId,
                                                        std::span<const yesdaw::engine::EntityId> noteIds) {
            juce::ignoreUnused (midiClipId);
            (void) appModel.selectPianoRollNotes (noteIds);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onSelectionCleared = [this] {
            appModel.clearPianoRollNoteSelection();
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNoteDeleted = [this] (yesdaw::engine::EntityId midiClipId,
                                               yesdaw::engine::EntityId noteId) {
            if (appModel.selectPianoRollNote (midiClipId, noteId).dispatched)
                (void) appModel.deleteSelectedPianoRollNotes();
            refreshActionState();
            repaintAll();
        };
        // Piano-roll viewport wheel map (E10): plain wheel scrolls keys, Shift+wheel scrolls
        // time, Ctrl+wheel zooms time anchored at the pointer tick.
        pianoRollInput.onViewKeysScrolled = [this] (int keyDelta) {
            pianoRollViewLowKey = std::clamp (
                pianoRollViewLowKey + keyDelta,
                yesdaw::ui::UiThemeLayout::pianoRollKeyMin,
                yesdaw::ui::UiThemeLayout::pianoRollKeyMax
                    - (yesdaw::ui::UiTheme::Layout::pianoRollKeyCount - 1));
            repaintAll();
        };
        pianoRollInput.onViewZoomWheel = [this] (yesdaw::engine::Tick anchorTick, double wheelDelta) {
            const double factor = wheelDelta > 0.0
                ? yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep
                : 1.0 / yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep;
            const double previousZoom = pianoRollViewZoom;
            pianoRollViewZoom = std::clamp (pianoRollViewZoom * factor,
                                            yesdaw::ui::UiThemeLayout::pianoRollZoomMin,
                                            yesdaw::ui::UiThemeLayout::pianoRollZoomMax);
            if (pianoRollViewZoom != previousZoom)
            {
                const double zoomRatio = previousZoom / pianoRollViewZoom;
                pianoRollViewScrollTicks = anchorTick
                    - static_cast<yesdaw::engine::Tick> (
                        std::llround (static_cast<double> (anchorTick - pianoRollViewScrollTicks) * zoomRatio));
            }
            if (pianoRollViewZoom == yesdaw::ui::UiThemeLayout::pianoRollZoomMin)
                pianoRollViewScrollTicks = 0;
            repaintAll();
        };
        pianoRollInput.onViewTicksScrolled = [this] (double wheelDelta) {
            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = currentPianoRollSurface();
            pianoRollViewScrollTicks -= static_cast<yesdaw::engine::Tick> (
                std::llround (wheelDelta
                              * static_cast<double> (pianoRollVisibleTicks (surface))
                              * yesdaw::ui::UiTheme::Layout::timelineScrollWheelFraction));
            pianoRollViewScrollTicks = juce::jmax<yesdaw::engine::Tick> (0, pianoRollViewScrollTicks);
            repaintAll();
        };
        // E12: the drag moves the whole selection (anchored on the dragged note) by the snapped
        // tick delta and the row-derived key delta as one undo transaction.
        pianoRollInput.onNotesDragged = [this] (yesdaw::engine::EntityId midiClipId,
                                                yesdaw::engine::EntityId noteId,
                                                yesdaw::engine::Tick tickDelta,
                                                int keyDelta) {
            (void) appModel.selectPianoRollNoteForGesture (midiClipId, noteId);
            (void) appModel.moveSelectedPianoRollNotesBy (tickDelta, keyDelta);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNoteHeadTrimmed = [this] (yesdaw::engine::EntityId midiClipId,
                                                   yesdaw::engine::EntityId noteId,
                                                   yesdaw::engine::Tick newStart) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.trimSelectedPianoRollNoteHeadTo (newStart);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNoteLengthChanged = [this] (yesdaw::engine::EntityId midiClipId,
                                                     yesdaw::engine::EntityId noteId,
                                                     yesdaw::engine::Tick lengthTicks) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.setSelectedPianoRollNoteLength (lengthTicks);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNoteTransposed = [this] (yesdaw::engine::EntityId midiClipId,
                                                  yesdaw::engine::EntityId noteId,
                                                  std::int32_t semitones) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.transposeSelectedPianoRollNote (semitones);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNoteQuantized = [this] (yesdaw::engine::EntityId midiClipId,
                                                 yesdaw::engine::EntityId noteId,
                                                 yesdaw::engine::Tick snapGridTicks) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.quantizeSelectedPianoRollNoteTo (yesdaw::engine::SnapGrid { snapGridTicks });
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNoteAdded = [this] (yesdaw::engine::EntityId midiClipId, yesdaw::engine::Tick tick, std::int16_t key) {
            (void) midiClipId;
            (void) appModel.addPianoRollNoteAt (tick, kPianoRollSnapGridTicks, key);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onExpressionRead = [this] {
            (void) appModel.readPianoRollExpressionLanes();
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNoteVelocityAdjusted = [this] (yesdaw::engine::EntityId midiClipId,
                                                        yesdaw::engine::EntityId noteId,
                                                        double normalizedVelocity) {
            (void) appModel.selectPianoRollNote (midiClipId, noteId);
            (void) appModel.setSelectedPianoRollNoteVelocity (normalizedVelocity);
            refreshActionState();
            repaintAll();
        };
        // E13: the lane paint gesture-selects its anchor (keeping a group the anchor belongs to)
        // and paints the batch as one undo transaction.
        pianoRollInput.onVelocityLanePainted =
            [this] (yesdaw::engine::EntityId midiClipId,
                    std::span<const std::pair<yesdaw::engine::EntityId, double>> edits) {
                if (edits.empty())
                    return;
                (void) appModel.selectPianoRollNoteForGesture (midiClipId, edits.front().first);
                (void) appModel.paintPianoRollNoteVelocities (midiClipId, edits);
                refreshActionState();
                repaintAll();
            };
        pianoRollInput.onNoteCopyDragged = [this] (yesdaw::engine::EntityId midiClipId,
                                                   yesdaw::engine::EntityId noteId,
                                                   yesdaw::engine::Tick newStartTick) {
            (void) appModel.duplicatePianoRollNote (midiClipId, noteId, newStartTick);
            refreshActionState();
            repaintAll();
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
            repaintAll();
        };
        addAndMakeVisible (audioDeviceChooser);

        // E29: the INPUT side gets the same treatment — a real input device chooser plus the
        // recorded-channel pick (mono channel N or the stereo pair) driving the model verb.
        audioInputDeviceChooser.setComponentID ("shell.device.input.chooser");
        audioInputDeviceChooser.setTooltip ("Audio input device");
        audioInputDeviceChooser.setName ("Audio input device");
        audioInputDeviceChooser.setTitle ("Audio input device");
        audioInputDeviceChooser.setTextWhenNothingSelected ("Input Device");
        audioInputDeviceChooser.setTextWhenNoChoicesAvailable ("No Inputs");
        audioInputDeviceChooser.onChange = [this] {
            if (refreshingAudioDeviceChooser)
                return;

            const int selected = audioInputDeviceChooser.getSelectedId();
            if (selected <= 0
                || static_cast<std::size_t> (selected - 1) >= audioInputDeviceChooserNames.size())
                return;

            suspendDesktopAudioCallback();
            (void) selectAudioInputDeviceByName (
                audioInputDeviceChooserNames[static_cast<std::size_t> (selected - 1)]);
            resumeDesktopAudioCallback();
            refreshAudioDeviceChooser();
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (audioInputDeviceChooser);

        recordingInputChannelChooser.setComponentID ("shell.device.input.channel");
        recordingInputChannelChooser.setTooltip ("Recorded input: mono channel or stereo pair");
        recordingInputChannelChooser.setName ("Recorded input channel");
        recordingInputChannelChooser.setTitle ("Recorded input channel");
        recordingInputChannelChooser.setTextWhenNothingSelected ("Input");
        recordingInputChannelChooser.setTextWhenNoChoicesAvailable ("No Inputs");
        recordingInputChannelChooser.onChange = [this] {
            if (refreshingAudioDeviceChooser)
                return;

            const int selected = recordingInputChannelChooser.getSelectedId();
            if (selected <= 0)
                return;

            // Ids: mono channel N -> N+1; stereo pair (N, N+1) -> 1000 + N + 1.
            const bool stereo = selected > 1000;
            const int base = stereo ? selected - 1001 : selected - 1;
            (void) appModel.setRecordingInputChannel (static_cast<std::uint16_t> (base), stereo);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (recordingInputChannelChooser);
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
            // G0.1: a Session drive redirects the records (YESDAW_SESSION_STATE_DIR) so a driven
            // launch never reads or rewrites the owner's real last-project record.
            if (fileChoices.sessionStateDirectory.empty())
            {
                const std::string sessionUtf8 =
                    juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("YES DAW").getFullPathName().toStdString();
                const auto* sessionBytes = reinterpret_cast<const char8_t*> (sessionUtf8.data());
                appModel.setSessionStateDirectory (
                    std::filesystem::path { std::u8string (sessionBytes, sessionBytes + sessionUtf8.size()) });
            }
            // G0.1: a bundle named on the command line wins over the last-project record.
            const std::filesystem::path lastProject = ! fileChoices.openBundleAtLaunch.empty()
                                                          ? fileChoices.openBundleAtLaunch
                                                          : appModel.readLastProjectRecord();
            if (! lastProject.empty())
            {
                const StoredProjectAssetsResult stored = decodeStoredProjectAssets (lastProject);
                if (stored.assets && ! stored.assets->empty())
                    (void) appModel.loadProjectBundle (
                        lastProject,
                        std::span<const yesdaw::ui::UiDecodedAsset> (
                            stored.assets->data(), stored.assets->size()));
                else if (stored.assets)
                    (void) appModel.openProjectBundle (lastProject);
                else
                    // R5: the last project failing to reopen is a fact, not a shrug.
                    appModel.reportStatus (
                        "Open failed: " + stored.failureReason
                            + " (" + lastProject.filename().string() + ")",
                        true);
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
                ++audioCallbackAdds;   // G0.1 probe: the one legitimate startup registration
                desktopAudioCallbackRegistered = true;
                appModel.setDeviceCallbackLive (true);
                desktopAudioOpen.store (true, std::memory_order_release);
                refreshAudioDeviceChooser();   // now the current device can be marked selected
            }
            else
            {
                // R4: a soundless app must say why instead of sitting silent.
                appModel.reportStatus (
                    "No audio device could be opened: " + error.toStdString(), true);
            }
        }

        // E34: open every MIDI input so played notes reach a live capture session (native
        // shell only — harness runs stay deterministic with the injected model seam).
        if (desktopAudioRequested)
        {
            for (const auto& midiDevice : juce::MidiInput::getAvailableDevices())
            {
                if (auto midiInput = juce::MidiInput::openDevice (midiDevice.identifier, this))
                {
                    midiInput->start();
                    midiInputs.push_back (std::move (midiInput));
                }
            }
        }

        // H17 CP4: scheduled autosave is ON by default (policy lives in the headless app model, so the
        // default is covered by a headless test). The Timer fires on the message thread — which is this
        // app's control thread — so writeAutosaveTick()'s heavy SQLite/asset I/O is on the right thread.
        startTimer (kUiRefreshIntervalMs);

        // G0.2: keys go to the command router, not to widgets (ADR-0046 §4).
        applyKeyboardFocusLaw();
    }

    ~MainComponent() override
    {
        if (routedTopLevel != nullptr)
            routedTopLevel->removeKeyListener (this);
        menuBar.setModel (nullptr);
        stopTimer();
        for (auto& midiInput : midiInputs)
            if (midiInput != nullptr)
                midiInput->stop();
        midiInputs.clear();
        if (desktopAudioCallbackRegistered)
            audioDeviceManager.removeAudioCallback (this);
        audioDeviceManager.closeAudioDevice();
        setLookAndFeel (nullptr);
    }

    // The UI polls the lock-free audio-thread transport snapshot at ~30 Hz. Autosave remains on its
    // independent slow schedule and never runs in the device callback.
    void timerCallback() override
    {
        const auto tickStart = std::chrono::steady_clock::now();
        serviceUiTick();
        lastTickMs = std::chrono::duration<double, std::milli> (
                         std::chrono::steady_clock::now() - tickStart).count();
        ++probeTick;
        writeStateProbeIfEnabled();
    }

    void serviceUiTick()
    {
        // G0.3: the janitor runs on the control thread every tick — retired engines / monitor
        // chains are freed once the device thread is provably past them.
        appModel.setDeviceCallbackLive (desktopAudioCallbackRegistered
                                        && desktopAudioOpen.load (std::memory_order_acquire));
        appModel.reclaimRetiredAudioObjects();
        appModel.refreshTransportSnapshot();
        appModel.serviceRecordingCountIn();
        if (appModel.realRecordingCaptureActive())
            appModel.drainRealRecordingCapture();
        // R4: promote a device-thread error flag to a status message, then decay and paint the
        // shared status line from real model state.
        if (deviceErrorPending.exchange (false, std::memory_order_acq_rel))
            appModel.reportStatus ("Audio device error - output stopped", true);
        appModel.serviceStatusLineDecay();
        refreshStatusLine();
        updateTrackMeterHoldStates();
        pushWindowTitle();

        // G0.4: the 391-line action-state refresh runs only when the context CHANGED (the
        // playhead position is not a change — it moves every tick while playing), never as a
        // 30 Hz habit. Meter-dependent chrome is painted, not refreshed, so it needs no tick here.
        {
            yesdaw::ui::UiActionContext context = appModel.contextSnapshot();
            context.playheadFrame = 0;
            if (! lastRefreshedContextValid || ! (context == lastRefreshedContext))
            {
                lastRefreshedContext = context;
                lastRefreshedContextValid = true;
                refreshActionState();
            }
        }

        // G0.4: a follow-scroll moves the whole canvas — that IS a view change; otherwise only the
        // dynamic layers (playhead, meters, transport counter) repaint this tick.
        const double scrollBefore = timelineScrollSeconds;
        followPlaybackPlayhead();
        if (timelineScrollSeconds != scrollBefore)
            repaintAll();
        else
            repaintDynamicLayers();

        if (! appModel.autosaveSchedule().enabled)
            return;

        autosaveElapsedMs += kUiRefreshIntervalMs;
        if (autosaveElapsedMs >= appModel.autosaveSchedule().intervalMs)
        {
            autosaveElapsedMs = 0;
            const yesdaw::persistence::AutosaveResult ticked = appModel.writeAutosaveTick();
            if (! ticked.ok())
                appModel.reportStatus ("Autosave failed", true);
        }
    }

    void refreshStatusLine()
    {
        statusLine.setText (appModel.statusLineText(), juce::dontSendNotification);
        statusLine.setColour (juce::Label::textColourId,
                              appModel.statusLineIsError()
                                  ? yesdaw::ui::UiTheme::Color::dangerRed()
                                  : kMutedText);
    }

    // E34: real MIDI inputs — note on/off pairs collected on the message thread and stamped
    // with the capture session's published device-frame cursor; outside a session the model
    // refuses them, so this is inert until Record rolls.
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override
    {
        if (! message.isNoteOn() && ! message.isNoteOff())
            return;

        const std::int64_t frame = appModel.captureDeviceFrameApprox();
        const bool noteOn = message.isNoteOn();
        const int note = message.getNoteNumber();
        const float velocity = message.getFloatVelocity();
        juce::Component::SafePointer<MainComponent> safeThis (this);
        juce::MessageManager::callAsync ([safeThis, frame, noteOn, note, velocity] {
            if (safeThis != nullptr)
                safeThis->handleCapturedMidiNote (frame, noteOn, note, velocity);
        });
    }

    void handleCapturedMidiNote (std::int64_t frame, bool noteOn, int note, float velocity)
    {
        if (noteOn)
        {
            pendingMidiNoteOns[note] = { frame, velocity };
            return;
        }

        const auto pending = pendingMidiNoteOns.find (note);
        if (pending == pendingMidiNoteOns.end())
            return;

        const auto [startFrame, onVelocity] = pending->second;
        pendingMidiNoteOns.erase (pending);
        if (frame <= startFrame)
            return;

        (void) appModel.captureMidiEventDuringRecording (
            { startFrame, static_cast<std::uint8_t> (note), onVelocity, frame - startFrame });
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        if (device != nullptr)
        {
            appModel.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
            // E28: the model adopts the REAL device profile — actual input count, a stable id
            // hashed from the device name, and the driver-reported latencies — so Record
            // unlocks from real hardware and take provenance records the real device.
            yesdaw::ui::UiRealRecordingDeviceProfile profile;
            const auto nameHash = static_cast<std::uint32_t> (device->getName().hashCode());
            profile.stableDeviceId = nameHash != 0u ? nameHash : 0xFFFFFFFFu;
            profile.sampleRateHz = device->getCurrentSampleRate();
            profile.inputChannels = device->getActiveInputChannels().countNumberOfSetBits();
            profile.maxBlockSize = device->getCurrentBufferSizeSamples();
            profile.inputLatencyFrames = std::max (0, device->getInputLatencyInSamples());
            profile.outputLatencyFrames = std::max (0, device->getOutputLatencyInSamples());
            (void) appModel.adoptRealRecordingDevice (profile);
        }
        desktopAudioOpen.store (device != nullptr, std::memory_order_release);
        // G0.1 probe: the block budget the deadline-miss counter measures against, and the
        // driver's own xrun baseline (-1 when the driver cannot report one).
        deviceSampleRateHz.store (device != nullptr ? device->getCurrentSampleRate() : 0.0,
                                  std::memory_order_relaxed);
        deviceXRunBaseline.store (device != nullptr ? device->getXRunCount() : -1,
                                  std::memory_order_relaxed);
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannels,
                                           int numInputChannels,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        const auto blockStart = std::chrono::steady_clock::now();
        (void) appModel.processDeviceAudioBlock (
            inputChannels, numInputChannels, outputChannels, numOutputChannels, numFrames);
        accountDeviceBlockPeaks (outputChannels, numOutputChannels, numFrames);

        // G0.1 probe (B5): a block that took longer than the audio it produced is a deadline miss.
        // Atomics only — the device thread never allocates, locks, or logs here.
        const auto elapsed = std::chrono::steady_clock::now() - blockStart;
        const auto elapsedNs = static_cast<std::uint64_t> (
            std::chrono::duration_cast<std::chrono::nanoseconds> (elapsed).count());
        const double rateHz = deviceSampleRateHz.load (std::memory_order_relaxed);
        if (rateHz > 0.0 && numFrames > 0)
        {
            const double budgetNs = 1.0e9 * static_cast<double> (numFrames) / rateHz;
            if (static_cast<double> (elapsedNs) > budgetNs)
                deviceDeadlineMisses.fetch_add (1u, std::memory_order_relaxed);
        }
        std::uint64_t previousMax = deviceMaxCallbackNs.load (std::memory_order_relaxed);
        while (elapsedNs > previousMax
               && ! deviceMaxCallbackNs.compare_exchange_weak (previousMax, elapsedNs,
                                                               std::memory_order_relaxed))
        {
        }
    }

    void audioDeviceStopped() override
    {
        desktopAudioOpen.store (false, std::memory_order_release);
    }

    void audioDeviceError (const juce::String&) override
    {
        desktopAudioOpen.store (false, std::memory_order_release);
        // R4: device thread — flag only; the UI timer reports it on the message thread.
        deviceErrorPending.store (true, std::memory_order_release);
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
    [[nodiscard]] float harnessInputMeterPeak() const noexcept
    {
        return appModel.inputMeterPeak();
    }
    [[nodiscard]] const yesdaw::ui::UiRecordingTrackInputSelection& harnessRecordingTrackInput() const noexcept
    {
        return appModel.recordingTrackInputSelection();
    }
    // M11: the whole arm set, so gates can pin that several rows are armed at once.
    [[nodiscard]] const std::vector<yesdaw::ui::UiRecordingTrackInputSelection>&
        harnessArmedRecordingTrackInputs() const noexcept
    {
        return appModel.armedRecordingTrackInputs();
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
    [[nodiscard]] std::uint64_t harnessPlaybackReplaceCount() const noexcept { return appModel.playbackReplaceCount(); }
    [[nodiscard]] std::uint64_t harnessPlaybackLiveScalarsApplied() const noexcept { return appModel.playbackLiveScalarsApplied(); }
    [[nodiscard]] long long harnessPlaybackLoopStartFrame() const noexcept { return appModel.playbackLoopStartFrame(); }
    [[nodiscard]] long long harnessPlaybackLoopEndFrame() const noexcept { return appModel.playbackLoopEndFrame(); }
    [[nodiscard]] std::string harnessStatusLineText() const { return appModel.statusLineText(); }
    [[nodiscard]] bool harnessStatusLineIsError() const noexcept { return appModel.statusLineIsError(); }
    [[nodiscard]] long long harnessTimelineRangeStartFrame() const noexcept { return appModel.timelineRangeStartFrame(); }
    [[nodiscard]] long long harnessTimelineRangeEndFrame() const noexcept { return appModel.timelineRangeEndFrame(); }
    [[nodiscard]] double harnessTimelineZoomFactor() const noexcept { return timelineZoomFactor; }
    [[nodiscard]] double harnessTimelineScrollSeconds() const noexcept { return timelineScrollSeconds; }
    [[nodiscard]] int harnessTimelineTrackScrollRows() const noexcept { return timelineTrackScrollRows; }
    [[nodiscard]] int harnessPianoRollViewLowKey() const noexcept { return pianoRollViewLowKey; }
    [[nodiscard]] double harnessPianoRollViewZoom() const noexcept { return pianoRollViewZoom; }
    [[nodiscard]] long long harnessPianoRollViewScrollTicks() const noexcept
    {
        return static_cast<long long> (pianoRollViewScrollTicks);
    }
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
    // E23: which strip the painted mixer highlights (tracks first, then buses; -1 = none).
    [[nodiscard]] int harnessSelectedMixerStripOrdinal() const
    {
        return appModel.selectedMixerStripOrdinal();
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
    // E30: input-carrying harness block — drives the same input-aware model path the native
    // device callback uses, so input metering is CI-deterministic.
    [[nodiscard]] bool harnessProcessDeviceAudioBlock (const float* const* inputChannels,
                                                       int numInputChannels,
                                                       float* const* outputChannels,
                                                       int numOutputChannels,
                                                       int numFrames) noexcept
    {
        const bool processed = appModel.processDeviceAudioBlock (
            inputChannels, numInputChannels, outputChannels, numOutputChannels, numFrames);
        accountDeviceBlockPeaks (outputChannels, numOutputChannels, numFrames);
        return processed;
    }
    // N1: the painted Mute/Solo cell rect for a strip, in SHELL coordinates (cell 0 = Solo,
    // 1 = Mute) — the same law the paint, the click hit-test and the live buttons read.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedMuteSoloCellBounds (int stripIndex, int cellIndex) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal
            || cellIndex < 0 || cellIndex >= static_cast<int> (kMixerPaintedMuteSoloCellCount))
            return {};

        return paintedMuteSoloCellBoundsForLane (
            paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
            static_cast<std::size_t> (cellIndex));
    }

    // M4: the painted insert-slot rect for a strip, in SHELL coordinates — the same law the paint
    // and the click hit-test read.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedInsertSlotBounds (int stripIndex, int slotIndex) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal
            || slotIndex < 0 || slotIndex >= yesdaw::ui::UiTheme::Layout::mixerPaintedInsertRowCount)
            return {};

        return paintedInsertRowBoundsForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                              static_cast<std::size_t> (slotIndex));
    }

    // M5: the painted send-row rect for a strip, in SHELL coordinates.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedSendRowBounds (int stripIndex, int sendIndex) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal || sendIndex < 0)
            return {};

        return paintedSendRowBoundsForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                            static_cast<std::size_t> (sendIndex));
    }

    // M6: the painted fader rail and the y the thumb sits at for a given gain — the same law the
    // paint uses, so a gate can prove unity is NOT at the top of the rail.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedFaderRailBounds (int stripIndex) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal)
            return {};

        return paintedFaderRailForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)));
    }

    [[nodiscard]] int harnessPaintedFaderThumbY (int stripIndex, float linearGain) const
    {
        const auto rail = harnessPaintedFaderRailBounds (stripIndex);
        return rail.isEmpty() ? 0 : mixerFaderThumbYForGain (rail, linearGain);
    }

    // ---- G0.1 State probe ------------------------------------------------------------------
    // One JSON document of what the shell is right now: transport, selection, focus, view,
    // frame/audio counters, and a `layout` map of shell-coordinate hit rects keyed by element
    // id so a Session script clicks by NAME (`widget.transport.play`, `lane.0`, `clip.<hex>`),
    // never by pixel. Every rect comes from the SAME law the paint and hit-test paths use.

    [[nodiscard]] static juce::var probeRect (juce::Rectangle<int> rect)
    {
        juce::Array<juce::var> values;
        values.add (rect.getX());
        values.add (rect.getY());
        values.add (rect.getWidth());
        values.add (rect.getHeight());
        return values;
    }

    [[nodiscard]] static const char* probeFocusContextName (yesdaw::ui::UiPanel panel) noexcept
    {
        switch (panel)
        {
            case yesdaw::ui::UiPanel::Timeline:  return "Arrange";
            case yesdaw::ui::UiPanel::Mixer:     return "Mixer";
            case yesdaw::ui::UiPanel::PianoRoll: return "PianoRoll";
        }
        return "Arrange";
    }

    [[nodiscard]] static const char* probeToolName (yesdaw::ui::TimelineTool tool) noexcept
    {
        switch (tool)
        {
            case yesdaw::ui::TimelineTool::Pointer:  return "Pointer";
            case yesdaw::ui::TimelineTool::Pencil:   return "Pencil";
            case yesdaw::ui::TimelineTool::Scissors: return "Scissors";
            case yesdaw::ui::TimelineTool::Hand:     return "Hand";
            case yesdaw::ui::TimelineTool::Zoom:     return "Zoom";
        }
        return "Pointer";
    }

    [[nodiscard]] juce::String probeRendererName() const
    {
        if (const juce::ComponentPeer* peer = getPeer())
        {
            // getAvailableRenderingEngines() is non-const in JUCE's peer API; the query itself
            // mutates nothing.
            auto& mutablePeer = const_cast<juce::ComponentPeer&> (*peer);
            const juce::StringArray engines = mutablePeer.getAvailableRenderingEngines();
            const int index = peer->getCurrentRenderingEngine();
            if (index >= 0 && index < engines.size())
                return engines[index];
            return "unknown";
        }
        return "none";
    }

    [[nodiscard]] double probePaintP95Ms() const
    {
        if (paintRingCount == 0)
            return 0.0;
        std::array<double, kStateProbePaintRingSize> sorted = paintRing;
        std::sort (sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t> (paintRingCount));
        const std::size_t rank = std::min (paintRingCount - 1u, (paintRingCount * 95u) / 100u);
        return sorted[rank];
    }

    [[nodiscard]] juce::var buildProbeLayout()
    {
        auto* layout = new juce::DynamicObject();
        juce::var layoutVar (layout);
        const auto put = [layout] (const juce::String& key, juce::Rectangle<int> rect) {
            if (! rect.isEmpty())
                layout->setProperty (key, probeRect (rect));
        };

        put ("header", getLocalBounds().withHeight (headerHeightNow()));
        put ("rail", leftRailPanelBounds());
        put ("timeline", timelineBounds());
        put ("inspector", inspectorBounds());
        if (appModel.context().mixerDockVisible
            || appModel.context().activePanel == yesdaw::ui::UiPanel::Mixer)
            put ("dock", mixerPanelBounds());

        // Every visible identified child by its component id — toolbar buttons carry their
        // action's stable id (configureActionComponent), choosers their shell ids.
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (const juce::Component* child = getChildComponent (i))
                if (child->isVisible() && child->getComponentID().isNotEmpty())
                    put ("widget." + child->getComponentID(), child->getBounds());

        if (appModel.context().projectLoaded
            && appModel.context().activePanel != yesdaw::ui::UiPanel::Mixer)
        {
            const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
            const juce::Point<int> origin = timelineInput.getPosition();
            put ("ruler", geometry.rulerArea.translated (origin.x, origin.y));
            put ("clipArea", geometry.clipArea.translated (origin.x, origin.y));

            for (int lane = 0; lane < state.trackCount; ++lane)
            {
                const int top = geometry.clipArea.getY()
                    + juce::roundToInt (geometry.laneTop (lane) - geometry.viewport.laneScrollPixels);
                const int height = lane < static_cast<int> (geometry.laneHeightPixelsPerLane.size())
                    ? juce::roundToInt (geometry.laneHeightPixelsPerLane[static_cast<std::size_t> (lane)])
                    : geometry.laneHeight;
                const juce::Rectangle<int> row (geometry.clipArea.getX(), top,
                                                geometry.clipArea.getWidth(), height);
                put ("lane." + juce::String (lane),
                     row.getIntersection (geometry.clipArea).translated (origin.x, origin.y));
                put ("rail.row." + juce::String (lane), harnessPaintedRailRowBounds (lane));
            }

            std::array<yesdaw::ui::ElementRect, yesdaw::ui::UiTheme::Layout::timelineCanvasVisibleClipCapacity> visible {};
            const yesdaw::ui::Viewport clipViewport = yesdaw::ui::viewportForClipLayout (geometry);
            const int visibleCount = state.clipCount > 0
                ? yesdaw::ui::layoutVisible (state.clips, state.clipCount, clipViewport,
                                             visible.data(), static_cast<int> (visible.size()))
                : 0;
            for (int i = 0; i < visibleCount; ++i)
            {
                const auto& rect = visible[static_cast<std::size_t> (i)];
                if (rect.id < 0 || rect.id >= static_cast<int> (timelineClipIds.size()))
                    continue;
                const juce::Rectangle<int> clipRect =
                    juce::Rectangle<int> (geometry.clipArea.getX() + juce::roundToInt (rect.x),
                                          geometry.clipArea.getY() + juce::roundToInt (rect.y),
                                          juce::roundToInt (rect.w),
                                          juce::roundToInt (rect.h))
                        .getIntersection (geometry.clipArea);
                put ("clip." + juce::String (entityIdHex (timelineClipIds[static_cast<std::size_t> (rect.id)])),
                     clipRect.translated (origin.x, origin.y));
            }
        }

        return layoutVar;
    }

    // G0.4: what identified widgets SAY (combo / button / label text by component id) — the
    // drive asserts on words, never on pixels, and a stale control is visible in the document.
    [[nodiscard]] juce::var buildProbeText() const
    {
        auto* text = new juce::DynamicObject();
        juce::var textVar (text);
        for (int i = 0; i < getNumChildComponents(); ++i)
        {
            const juce::Component* child = getChildComponent (i);
            if (child == nullptr || ! child->isVisible() || child->getComponentID().isEmpty())
                continue;
            juce::String value;
            if (const auto* combo = dynamic_cast<const juce::ComboBox*> (child))
                value = combo->getText();
            else if (const auto* button = dynamic_cast<const juce::Button*> (child))
                value = button->getButtonText();
            else if (const auto* label = dynamic_cast<const juce::Label*> (child))
                value = label->getText();
            else
                continue;
            text->setProperty (child->getComponentID(), value);
        }
        return textVar;
    }

    [[nodiscard]] juce::String buildStateProbeJson()
    {
        const yesdaw::ui::UiActionContext context = appModel.contextSnapshot();
        auto* root = new juce::DynamicObject();
        juce::var rootVar (root);

        root->setProperty ("version", kStateProbeSchemaVersion);
        root->setProperty ("tick", static_cast<juce::int64> (probeTick));
        root->setProperty ("uptimeMs", std::chrono::duration<double, std::milli> (
                                           std::chrono::steady_clock::now() - launchStamp).count());
        root->setProperty ("renderer", probeRendererName());
        root->setProperty ("windowTitle", computedWindowTitle());
        root->setProperty ("projectLoaded", context.projectLoaded);
        root->setProperty ("bundlePath", juceFileFromPath (harnessBundlePath()).getFullPathName());
        root->setProperty ("window", probeRect (getScreenBounds()));
        {
            double displayScale = 1.0;
            if (const juce::Displays::Display* display =
                    juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
                displayScale = display->scale;
            root->setProperty ("displayScale", displayScale);
        }

        {
            auto* transport = new juce::DynamicObject();
            transport->setProperty ("isPlaying", context.isPlaying);
            transport->setProperty ("isRecording", context.isRecording);
            transport->setProperty ("playheadFrame", static_cast<juce::int64> (context.playheadFrame));
            transport->setProperty ("playheadSeconds",
                                    context.projectLoaded && appModel.project().sampleRate.isValid()
                                        ? static_cast<double> (context.playheadFrame)
                                              / appModel.project().sampleRate.hz
                                        : 0.0);
            transport->setProperty ("rate", context.shuttlePlaybackRate);
            transport->setProperty ("metronome", context.metronomeEnabled);
            auto* loop = new juce::DynamicObject();
            loop->setProperty ("enabled", context.loopEnabled);
            loop->setProperty ("start", static_cast<juce::int64> (harnessPlaybackLoopStartFrame()));
            loop->setProperty ("end", static_cast<juce::int64> (harnessPlaybackLoopEndFrame()));
            transport->setProperty ("loop", juce::var (loop));
            root->setProperty ("transport", juce::var (transport));
        }

        {
            auto* selection = new juce::DynamicObject();
            juce::Array<juce::var> clips;
            if (context.projectLoaded)
                for (const yesdaw::engine::Clip& clip : appModel.project().clips)
                    if (appModel.isTimelineClipSelected (clip.id))
                        clips.add (juce::String (entityIdHex (clip.id)));
            selection->setProperty ("clips", clips);
            juce::Array<juce::var> notes;
            for (const yesdaw::engine::EntityId& noteId : appModel.selectedMidiNoteIds())
                notes.add (juce::String (entityIdHex (noteId)));
            selection->setProperty ("notes", notes);
            juce::Array<juce::var> tracks;
            if (selectedTrackLane >= 0)
                tracks.add (selectedTrackLane);
            selection->setProperty ("tracks", tracks);
            selection->setProperty ("midiClip",
                                    appModel.selectedMidiClipId().isValid()
                                        ? juce::var (juce::String (entityIdHex (appModel.selectedMidiClipId())))
                                        : juce::var());
            if (context.timelineRangeSelected)
            {
                auto* range = new juce::DynamicObject();
                range->setProperty ("startFrame", static_cast<juce::int64> (harnessTimelineRangeStartFrame()));
                range->setProperty ("endFrame", static_cast<juce::int64> (harnessTimelineRangeEndFrame()));
                selection->setProperty ("timeRange", juce::var (range));
            }
            else
            {
                selection->setProperty ("timeRange", juce::var());
            }
            selection->setProperty ("mixerStrip", harnessSelectedMixerStripOrdinal());
            root->setProperty ("selection", juce::var (selection));
        }

        root->setProperty ("focusContext", probeFocusContextName (context.activePanel));
        {
            const juce::Component* focused = juce::Component::getCurrentlyFocusedComponent();
            juce::String owner = "none";
            if (focused == this)
                owner = "shell";
            else if (focused != nullptr)
                owner = focused->getComponentID().isNotEmpty() ? focused->getComponentID()
                                                                : focused->getName().isNotEmpty()
                                                                      ? focused->getName()
                                                                      : juce::String ("unnamed");
            root->setProperty ("focusOwner", owner);
            root->setProperty ("textEditorActive",
                               dynamic_cast<const juce::TextEditor*> (focused) != nullptr
                                   || trackRenameEditor.isVisible() || clipRenameEditor.isVisible()
                                   || markerRenameEditor.isVisible() || busRenameEditor.isVisible());
        }
        root->setProperty ("lastAction", juce::String (lastActionStableId));
        root->setProperty ("commandDispatchCount", context.commandDispatchCount);
        {
            auto* status = new juce::DynamicObject();
            status->setProperty ("text", juce::String (appModel.statusLineText()));
            status->setProperty ("isError", appModel.statusLineIsError());
            root->setProperty ("status", juce::var (status));
        }

        {
            auto* view = new juce::DynamicObject();
            view->setProperty ("width", getWidth());
            view->setProperty ("height", getHeight());
            view->setProperty ("zoom", timelineZoomFactor);
            view->setProperty ("scrollSec", timelineScrollSeconds);
            view->setProperty ("trackScrollRows", timelineTrackScrollRows);
            view->setProperty ("activePanel", probeFocusContextName (context.activePanel));
            view->setProperty ("inspector", ! inspectorBounds().isEmpty()
                                                && context.activePanel != yesdaw::ui::UiPanel::Mixer);
            view->setProperty ("dock", context.activePanel == yesdaw::ui::UiPanel::Mixer
                                           ? juce::String ("MixerFull")
                                           : context.mixerDockVisible ? juce::String ("Mixer")
                                                                      : juce::String ("None"));
            view->setProperty ("dockHeight", dockedMixerHeight());
            view->setProperty ("settingsRow", context.settingsRowVisible);
            view->setProperty ("headerHeight", headerHeightNow());
            view->setProperty ("nudgeValue", context.nudgeValue);
            {
                const CounterStrings counter = counterStrings();
                view->setProperty ("timeDisplay", counter.mode);
                view->setProperty ("counterPrimary", counter.primary);
                view->setProperty ("counterSecondary", counter.secondary);
            }
            view->setProperty ("tool", probeToolName (context.activeTimelineTool));
            view->setProperty ("snapEnabled", context.snapEnabled);
            view->setProperty ("snapGridTicks", static_cast<juce::int64> (context.snapGridTicks));
            view->setProperty ("playheadFollow", context.playheadFollowEnabled);
            view->setProperty ("trackCount", context.projectLoaded
                                                 ? static_cast<int> (appModel.project().tracks.size())
                                                 : 0);
            view->setProperty ("clipCount", context.projectLoaded
                                                ? static_cast<int> (appModel.project().clips.size())
                                                : 0);
            root->setProperty ("view", juce::var (view));
        }

        {
            auto* frame = new juce::DynamicObject();
            frame->setProperty ("paintMs", lastPaintMs);
            frame->setProperty ("paintP95Ms", probePaintP95Ms());
            frame->setProperty ("paintCount", static_cast<juce::int64> (paintCount));
            frame->setProperty ("tickMs", lastTickMs);
            frame->setProperty ("actionToPaintMs", lastActionToPaintMs);
            // G0.4: how the shell invalidates — full (model/view change) vs dynamic (tick) — and
            // how often the action-state refresh really runs.
            frame->setProperty ("fullInvalidations", static_cast<juce::int64> (fullInvalidations));
            frame->setProperty ("dynamicInvalidations", static_cast<juce::int64> (dynamicInvalidations));
            frame->setProperty ("actionStateRefreshes", static_cast<juce::int64> (actionStateRefreshes));
            root->setProperty ("frame", juce::var (frame));
        }

        {
            auto* audio = new juce::DynamicObject();
            audio->setProperty ("callbackAdds", static_cast<juce::int64> (audioCallbackAdds));
            audio->setProperty ("callbackRemovals", static_cast<juce::int64> (audioCallbackRemovals));
            audio->setProperty ("callbackRegistered", desktopAudioCallbackRegistered);
            // G0.3: suspend REQUESTS (registered or not) — the [no-callback-teardown] gate's
            // number in the headless harness, where no device callback ever exists.
            audio->setProperty ("suspendRequests", static_cast<juce::int64> (audioSuspendRequests));
            audio->setProperty ("retiredObjects", static_cast<juce::int64> (appModel.retiredAudioObjectCount()));
            audio->setProperty ("deviceBlocks", static_cast<juce::int64> (appModel.deviceBlocksStarted()));
            audio->setProperty ("deviceOpen", desktopAudioOpen.load (std::memory_order_acquire));
            audio->setProperty ("rebuilds", static_cast<juce::int64> (appModel.playbackReplaceCount()));
            audio->setProperty ("liveScalars", static_cast<juce::int64> (appModel.playbackLiveScalarsApplied()));
            audio->setProperty ("blocks", static_cast<juce::int64> (
                                              deviceAudioCallbackBlockCount.load (std::memory_order_acquire)));
            audio->setProperty ("deadlineMisses", static_cast<juce::int64> (
                                                      deviceDeadlineMisses.load (std::memory_order_relaxed)));
            audio->setProperty ("maxCallbackMs",
                                static_cast<double> (deviceMaxCallbackNs.load (std::memory_order_relaxed)) / 1.0e6);
            audio->setProperty ("sampleRateHz", deviceSampleRateHz.load (std::memory_order_relaxed));
            // Driver-reported xruns since the device started; -1 when the driver cannot count.
            int underruns = -1;
            if (const juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
            {
                const int baseline = deviceXRunBaseline.load (std::memory_order_relaxed);
                const int current = device->getXRunCount();
                if (baseline >= 0 && current >= 0)
                    underruns = current - baseline;
            }
            audio->setProperty ("underruns", underruns);
            root->setProperty ("audio", juce::var (audio));
        }

        root->setProperty ("layout", buildProbeLayout());
        root->setProperty ("text", buildProbeText());
        {
            auto* recording = new juce::DynamicObject();
            const auto& device = appModel.recordingDeviceSelection();
            recording->setProperty ("deviceSelected", device.selected);
            recording->setProperty ("inputChannels", static_cast<int> (device.inputChannels));
            recording->setProperty ("deviceGeneration", static_cast<juce::int64> (device.generation));
            recording->setProperty ("selectedInputChannel", context.selectedRecordingInputChannel);
            recording->setProperty ("chooserGeneration", static_cast<juce::int64> (recordingChannelChooserGeneration));
            root->setProperty ("recording", juce::var (recording));
        }
        return juce::JSON::toString (rootVar, true);
    }

    void writeStateProbeIfEnabled()
    {
        if (stateProbePath.empty())
            return;
        // Write-then-replace so a reader never sees a torn document.
        (void) juceFileFromPath (stateProbePath).replaceWithText (buildStateProbeJson());
    }

    // N6: the rail's painted row rect (shell coordinates), the SAME law rowBounds/rowAt/paint
    // share — so a gate can prove a height drag moved exactly one row and left every other row's
    // position/height alone.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedRailRowBounds (int row) const
    {
        return trackListInput.rowBounds (row)
            .translated (trackListInput.getX(), trackListInput.getY());
    }

    // N7: the painted colour-swatch rect for a rail row (the left accent bar), in shell
    // coordinates — the same law the click-to-cycle gesture hit-tests against.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedColourSwatchBounds (int row) const
    {
        return trackListInput.colourSwatchBounds (row)
            .translated (trackListInput.getX(), trackListInput.getY());
    }

    // V2: the ACTUAL bar|beat the header paints — reads the same law drawTransportReadouts uses,
    // so a test can never duplicate the formula.
    [[nodiscard]] yesdaw::engine::BarBeat harnessHeaderBarBeat() const { return headerBarBeat(); }

    // V4: the ruler's painted bar labels — the SAME state build, geometry, and label law the
    // paint path runs (makeTimelineState → timelineCanvasGeometry → computeRulerBarLabels), so a
    // gate can never re-derive the formula.
    [[nodiscard]] std::vector<yesdaw::ui::RulerBarLabel> harnessRulerBarLabels()
    {
        const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
        return yesdaw::ui::computeRulerBarLabels (geometry.clipArea, state, geometry.viewport);
    }

    // V5: the rail's live L/R meter peaks for one row — the SAME hold-state values the paint
    // passes to drawMeterWithHold, so a gate can prove the two channels really diverge.
    [[nodiscard]] std::pair<float, float> harnessRailMeterChannelPeaks (int row) const
    {
        if (row < 0 || row >= static_cast<int> (trackMeterHoldLR.size()))
            return { 0.0f, 0.0f };

        const auto& lr = trackMeterHoldLR[static_cast<std::size_t> (row)];
        return { lr[0].livePeak, lr[1].livePeak };
    }

    // V5: the rail VOL fader's rect in SHELL coordinates — the SAME law paint and the click/drag
    // hit-test share, so a gate can prove the control is genuinely vertical.
    [[nodiscard]] juce::Rectangle<int> harnessRailVolumeSliderBounds (int row) const
    {
        return trackListInput.volumeSliderBounds (row)
            .translated (trackListInput.getX(), trackListInput.getY());
    }

    // V7: the fade chart's inner rect in SHELL coordinates — the SAME law the paint uses.
    [[nodiscard]] juce::Rectangle<int> harnessInspectorFadeChartBounds() const
    {
        return inspectorFadeChartBounds();
    }

    // V4: the inverse pixel→seconds mapping of the SAME viewport the ruler paints with, so a
    // gate can cross-check a label's x against the tempo map without duplicating the paint math.
    [[nodiscard]] double harnessRulerSecondsAtX (int x)
    {
        const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
        return static_cast<double> (x - geometry.clipArea.getX()) / geometry.viewport.pixelsPerSecond
             + geometry.viewport.scrollSeconds;
    }

    // N7: the ACTUAL colour the timeline canvas will paint for one clip (by id) — reads the same
    // cached timelineClipStyles/timelineClipIds arrays paintTimelineCanvas() paints from,
    // refreshing them first so this can never report a stale value from before the caller's last
    // edit.
    [[nodiscard]] juce::Colour harnessTimelineClipColour (yesdaw::engine::EntityId clipId)
    {
        rebuildTimelineClipViews();
        for (std::size_t i = 0; i < timelineClipIds.size(); ++i)
            if (timelineClipIds[i] == clipId)
                return timelineClipStyles[i].colour;
        return {};
    }

    // N3: the painted mixer-strip lane rect for a track/bus strip, and the master pane's rect —
    // exposed so a gate can prove they share ONE law (master is always the next contiguous slot
    // after the last strip, never a detached island computed independently of it).
    [[nodiscard]] juce::Rectangle<int> harnessPaintedMixerStripBounds (int stripIndex) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal)
            return {};

        return paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex));
    }

    [[nodiscard]] juce::Rectangle<int> harnessPaintedMixerMasterBounds() const
    {
        return paintedMixerMasterBounds();
    }

    // V3: the dock's OWN reserved rect — height collapses to (near) zero when the toggle hides
    // it, the same law every layout function (timelineBounds/leftRailPanelBounds/inspectorBounds
    // /this) shares via dockedMixerHeight().
    [[nodiscard]] juce::Rectangle<int> harnessMixerPanelBounds() const
    {
        return mixerPanelBounds();
    }

    [[nodiscard]] juce::Rectangle<int> harnessTimelineBounds() const
    {
        return timelineBounds();
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

    // G0.1 probe: paint() opens the frame stamp and paintOverChildren() closes it — JUCE paints
    // this component, then every child, then paintOverChildren on the same component, so the
    // pair brackets the whole shell's paint work for one frame (the B2 budget).
    void paintOverChildren (juce::Graphics&) override
    {
        const auto now = std::chrono::steady_clock::now();
        lastPaintMs = std::chrono::duration<double, std::milli> (now - paintStartStamp).count();
        paintRing[paintRingIndex] = lastPaintMs;
        paintRingIndex = (paintRingIndex + 1u) % paintRing.size();
        paintRingCount = std::min (paintRingCount + 1u, paintRing.size());
        ++paintCount;
        if (actionStampPending)
        {
            actionStampPending = false;
            lastActionToPaintMs =
                std::chrono::duration<double, std::milli> (now - pendingActionStamp).count();
        }
    }

    void paint (juce::Graphics& g) override
    {
        paintStartStamp = std::chrono::steady_clock::now();
        g.fillAll (kBackground);
        drawHeader (g);

        const auto bounds = getLocalBounds();
        const auto top = bounds.withHeight (headerHeightNow());
        g.setColour (yesdaw::ui::UiTheme::Color::separator());
        g.fillRect (top.withBottom (headerHeightNow())
                        .removeFromBottom (yesdaw::ui::UiTheme::Layout::shellHeaderSeparatorHeight));

        auto work = bounds.withTrimmedTop (headerHeightNow());
        if (appModel.context().activePanel == yesdaw::ui::UiPanel::Mixer)
        {
            drawMixer (g, mixerPanelBounds());
            return;
        }

        work.removeFromBottom (dockedMixerHeight());
        auto left = work.removeFromLeft (kLeftRailWidth)
                        .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                  yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
        auto inspector = work.removeFromRight (inspectorWidthNow())
                             .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
        auto timeline = work.reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                      yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);

        drawTrackList (g, left);
        if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll)
            drawPianoRoll (g, timeline);
        drawInspector (g, inspector);
        // V3: a collapsed dock paints NOTHING (the "drop whole" law this codebase already uses
        // elsewhere for sections that don't fit) rather than relying on a zero/negative-height
        // rect to degrade gracefully.
        if (appModel.context().mixerDockVisible)
            drawMixer (g, mixerPanelBounds());
    }

    void resized() override
    {
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        const HeaderLayout h = headerLayout();

        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto action = toolbarActions[i];
            if (isSettingsRowAction (action))
                buttons[i].setVisible (h.settingsVisible);
            switch (action)
            {
                case yesdaw::ui::UiActionId::ProjectNew:         buttons[i].setBounds (h.newButton); break;
                case yesdaw::ui::UiActionId::ProjectOpen:        buttons[i].setBounds (h.openButton); break;
                case yesdaw::ui::UiActionId::ProjectSave:        buttons[i].setBounds (h.saveButton); break;
                case yesdaw::ui::UiActionId::ProjectImportAudio: buttons[i].setBounds (h.importButton); break;
                case yesdaw::ui::UiActionId::RecordingArmTrack:  buttons[i].setBounds (h.arm); break;
                case yesdaw::ui::UiActionId::RecordingSetMonitoringPolicy: buttons[i].setBounds (h.monitor); break;
                case yesdaw::ui::UiActionId::TransportRecord:    buttons[i].setBounds (h.record); break;
                case yesdaw::ui::UiActionId::RecordingAssembleComp: buttons[i].setBounds (h.comp); break;
                case yesdaw::ui::UiActionId::EditUndo:           buttons[i].setBounds (h.undoButton); break;
                case yesdaw::ui::UiActionId::EditRedo:           buttons[i].setBounds (h.redoButton); break;
                case yesdaw::ui::UiActionId::TransportLocateStart: buttons[i].setBounds (h.locateStart); break;
                case yesdaw::ui::UiActionId::TransportPlay:      buttons[i].setBounds (h.play); break;
                case yesdaw::ui::UiActionId::TransportStop:      buttons[i].setBounds (h.stop); break;
                case yesdaw::ui::UiActionId::TransportToggleLoop: buttons[i].setBounds (h.loop); break;
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
        audioDeviceChooser.setBounds (h.outputDevice);
        audioInputDeviceChooser.setBounds (h.inputDevice);
        recordingInputChannelChooser.setBounds (h.inputChannel);
        exportAudioButton.setBounds (h.exportButton);
        exportAudioProgress.setBounds (h.exportProgress);
        exportAudioCancelButton.setBounds (h.exportCancel);
        exportBitDepthChooser.setBounds (h.bitDepth);
        exportRangeChooser.setBounds (h.range);
        for (juce::Component* settingsControl : { static_cast<juce::Component*> (&audioDeviceChooser),
                                                  static_cast<juce::Component*> (&audioInputDeviceChooser),
                                                  static_cast<juce::Component*> (&recordingInputChannelChooser),
                                                  static_cast<juce::Component*> (&exportBitDepthChooser),
                                                  static_cast<juce::Component*> (&exportRangeChooser) })
            settingsControl->setVisible (h.settingsVisible);
        menuBar.setBounds (h.menuBar);
        // M9: the LUFS readout rides the master card — it drops with it instead of being clipped.
        masterLoudnessReadout.setBounds (headerMasterLufsBounds());
        masterLoudnessReadout.setVisible (! headerMasterLufsBounds().isEmpty());
        timelineInput.setBounds (timelineBounds());
        playheadLayer.setBounds (timelineBounds());
        pianoRollInput.setBounds (timelineBounds());
        trackListInput.setBounds (leftRailPanelBounds());
        {
            auto strips = mixerPanelBounds();
            strips.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerToolsWidth);
            mixerStripsInput.setBounds (strips);
        }
        {
            auto box = h.tempoMeterBox;
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
        {
            // V3: anchored just above the dock's CURRENT top edge (using dockedMixerHeight(),
            // not the fixed kMixerHeight) — it stays reachable whether the dock is expanded or
            // collapsed, and tracks the dock's own state instead of a second, independent law.
            using L = yesdaw::ui::UiTheme::Layout;
            mixerDockToggle.setBounds (
                getWidth() - L::mixerDockToggleWidth - L::mixerPanelHorizontalInset,
                getHeight() - dockedMixerHeight() - L::mixerDockToggleHeight - L::mixerDockToggleBottomGap,
                L::mixerDockToggleWidth,
                L::mixerDockToggleHeight);
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
            repaintAll();
        };
        button.setVisible (false);
        addAndMakeVisible (button);
    }

    // V3: a real toggle for the always-on bottom mixer dock — collapsing it reclaims vertical
    // space for the timeline/rail/inspector; the full-view Mixer panel is unaffected.
    void configureMixerDockToggle()
    {
        constexpr yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::TimelineToggleMixerDock;
        configureActionComponent (mixerDockToggle, action, "Mixer dock");
        if (const auto* descriptor = appModel.registry().descriptor (action))
            mixerDockToggle.setButtonText (descriptor->label);
        else
            mixerDockToggle.setButtonText ("Mixer Dock");
        mixerDockToggle.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        mixerDockToggle.setColour (juce::TextButton::buttonOnColourId, kPurple.darker (0.45f));
        mixerDockToggle.setColour (juce::TextButton::textColourOffId, kText);
        mixerDockToggle.setColour (juce::TextButton::textColourOnId, kText);
        mixerDockToggle.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::TimelineToggleMixerDock);
            refreshActionState();
            resized();
            repaintAll();
        };
        addAndMakeVisible (mixerDockToggle);
    }

    // V7: the inspector's CLIP/TRACK tabs become real buttons — each dispatches a genuine
    // UiActionId, the model owns the active-tab state, and layout/paint follow it.
    void configureInspectorTabs()
    {
        const auto configureTab = [this] (juce::TextButton& button,
                                          yesdaw::ui::UiActionId action,
                                          const char* fallbackText)
        {
            configureActionComponent (button, action, fallbackText);
            if (const auto* descriptor = appModel.registry().descriptor (action))
                button.setButtonText (descriptor->label);
            else
                button.setButtonText (fallbackText);
            button.setColour (juce::TextButton::buttonColourId,
                              yesdaw::ui::UiTheme::Color::buttonSurface());
            button.setColour (juce::TextButton::buttonOnColourId,
                              yesdaw::ui::UiTheme::Color::inspectorTab());
            button.setColour (juce::TextButton::textColourOffId, kMutedText);
            button.setColour (juce::TextButton::textColourOnId, kText);
            button.onClick = [this, action] {
                (void) appModel.dispatch (action);
                refreshActionState();
                resized();
                repaintAll();
            };
            addAndMakeVisible (button);
        };
        configureTab (inspectorClipTab, yesdaw::ui::UiActionId::InspectorShowClipTab, "Clip");
        configureTab (inspectorTrackTab, yesdaw::ui::UiActionId::InspectorShowTrackTab, "Track");
    }

    // V8: a visible toolbar zoom control. The buttons dispatch the EXISTING zoom actions through
    // handleAction (the same playhead-anchored law the menu/keyboard path runs), and the readout
    // shows the one shared timelineZoomFactor every zoom gesture mutates — never a second zoom
    // concept.
    void configureTimelineZoomControls()
    {
        const auto configureStep = [this] (juce::TextButton& button,
                                           yesdaw::ui::UiActionId action,
                                           const char* stepText)
        {
            configureActionComponent (button, action, stepText);
            button.setButtonText (stepText);
            button.setColour (juce::TextButton::buttonColourId,
                              yesdaw::ui::UiTheme::Color::buttonSurface());
            button.setColour (juce::TextButton::textColourOffId, kText);
            button.setColour (juce::TextButton::textColourOnId, kText);
            button.onClick = [this, action] {
                handleAction (action);
                refreshActionState();
                repaintAll();
            };
            addAndMakeVisible (button);
        };
        configureStep (timelineZoomOutButton, yesdaw::ui::UiActionId::TimelineZoomOut, "-");
        configureStep (timelineZoomInButton, yesdaw::ui::UiActionId::TimelineZoomIn, "+");

        timelineZoomReadout.setComponentID ("timeline.zoom.readout");
        timelineZoomReadout.setName ("Timeline zoom factor");
        timelineZoomReadout.setTooltip ("Current timeline zoom factor (1.0x fits the project)");
        timelineZoomReadout.setJustificationType (juce::Justification::centred);
        timelineZoomReadout.setColour (juce::Label::textColourId, kMutedText);
        timelineZoomReadout.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (timelineZoomReadout);

        // R4: the shared status line — failures from save/export/create/autosave/device paint
        // here from real model state; success stays quiet and the UI timer decays the text.
        statusLine.setComponentID ("shell.statusline");
        statusLine.setName ("Status line");
        statusLine.setTooltip ("Status messages: failures from save, export, project create, autosave, and the audio device");
        statusLine.setJustificationType (juce::Justification::centredLeft);
        statusLine.setColour (juce::Label::textColourId, kMutedText);
        statusLine.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (statusLine);
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
            repaintAll();
        };
        addAndMakeVisible (automationLaneToggle);

        // E20: the lane-target chooser — the canvas edits whatever target it names, creating
        // the lane on demand (fader / pan / each send level / each FX param).
        automationTargetChooser.setComponentID ("timeline.automation.target");
        automationTargetChooser.setTooltip ("Choose which automation lane the canvas edits");
        automationTargetChooser.onChange = [this] {
            if (refreshingAutomationTarget)
                return;

            const int selected = automationTargetChooser.getSelectedId();
            if (selected <= 0)
                return;

            selectedAutomationTargetIndex = selected - 1;
            refreshActionState();
            repaintAll();
        };
        addChildComponent (automationTargetChooser);

        // N5/R15: the automation write mode — Read (default, playback only), Touch/Latch (a
        // control drag during playback writes breakpoints instead of a plain edit), or Off
        // (lanes stay stored and editable but playback IGNORES them and nothing ever writes).
        automationModeChooser.setComponentID ("timeline.automation.mode");
        automationModeChooser.setTooltip ("Automation write mode: Read plays back; Touch/Latch "
                                          "record a control ride while the transport rolls; "
                                          "Off ignores every lane and writes nothing");
        automationModeChooser.addItem ("Read", 1);
        automationModeChooser.addItem ("Touch", 2);
        automationModeChooser.addItem ("Latch", 3);
        automationModeChooser.addItem ("Off", 4);   // id - 1 == AutomationMode::Off
        automationModeChooser.onChange = [this] {
            if (refreshingAutomationTarget)
                return;

            const int selected = automationModeChooser.getSelectedId();
            if (selected <= 0)
                return;

            (void) appModel.setAutomationMode (
                static_cast<yesdaw::engine::AutomationMode> (selected - 1));
            refreshActionState();
            repaintAll();
        };
        addChildComponent (automationModeChooser);

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
        // N4: adds to the SELECTED target's lane (creating it on first use), matching the canvas
        // click path — never the first track's fader regardless of what is chosen.
        automationBreakpointAddButton.onClick = [this] {
            const AutomationTargetOption target = currentAutomationTarget();
            if (target.ownerEntity.isValid())
                (void) appModel.addAutomationBreakpointToLane (
                    target.ownerEntity, target.role, target.paramId,
                    yesdaw::ui::UiAppModel::kFirstTrackAutomationBreakpointAddTick,
                    yesdaw::ui::UiAppModel::kFirstTrackAutomationBreakpointAddValue);
            refreshActionState();
            repaintAll();
        };
        automationBreakpointAddButton.setVisible (false);
        addAndMakeVisible (automationBreakpointAddButton);

        constexpr yesdaw::ui::UiActionId deleteAction = yesdaw::ui::UiActionId::TimelineAutomationDeleteBreakpoint;
        configureActionComponent (automationBreakpointDeleteButton, deleteAction, "Delete automation breakpoint");
        if (const auto* descriptor = appModel.registry().descriptor (deleteAction))
            automationBreakpointDeleteButton.setButtonText (descriptor->label);
        automationBreakpointDeleteButton.setColour (juce::TextButton::buttonColourId,
                                                    yesdaw::ui::UiTheme::Color::buttonSurface());
        // N4: deletes the SELECTED target's last breakpoint, matching the canvas — never the
        // first track's fader regardless of what is chosen.
        automationBreakpointDeleteButton.onClick = [this] {
            const AutomationTargetOption target = currentAutomationTarget();
            if (target.ownerEntity.isValid())
            {
                if (const yesdaw::engine::AutomationLaneData* const lane = appModel.automationLaneForTarget (
                        target.ownerEntity, target.role, target.paramId);
                    lane != nullptr && ! lane->points.empty())
                {
                    (void) appModel.removeAutomationBreakpointAtTick (lane->id, lane->points.back().tick);
                }
            }
            refreshActionState();
            repaintAll();
        };
        automationBreakpointDeleteButton.setVisible (false);
        addAndMakeVisible (automationBreakpointDeleteButton);
    }

    void configureInspectorControls()
    {
        // E33: the take stack — a real TAKES section replaces the old always-"No automation"
        // placeholder. The chooser switches the AUDIBLE take; Delete Take removes one.
        inspectorTakeChooser.setComponentID ("clip.inspector.take.chooser");
        inspectorTakeChooser.setTooltip ("Switch the audible take for this clip's window");
        inspectorTakeChooser.setName ("Take chooser");
        inspectorTakeChooser.setTitle ("Take chooser");
        inspectorTakeChooser.setTextWhenNothingSelected ("Takes");
        inspectorTakeChooser.setTextWhenNoChoicesAvailable ("No takes");
        inspectorTakeChooser.onChange = [this] {
            if (refreshingInspectorControls)
                return;

            const int selected = inspectorTakeChooser.getSelectedId();
            if (selected <= 0
                || static_cast<std::size_t> (selected - 1) >= inspectorTakeViews.size())
                return;

            (void) appModel.switchAudibleTakeForSelectedClip (
                inspectorTakeViews[static_cast<std::size_t> (selected - 1)].takeId);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (inspectorTakeChooser);

        inspectorTakeDelete.setComponentID ("clip.inspector.take.delete");
        inspectorTakeDelete.setButtonText ("Delete Take");
        inspectorTakeDelete.setTooltip ("Delete the chosen take (its clip goes with it)");
        inspectorTakeDelete.setName ("Delete take");
        inspectorTakeDelete.setTitle ("Delete take");
        inspectorTakeDelete.onClick = [this] {
            const int selected = inspectorTakeChooser.getSelectedId();
            if (selected <= 0
                || static_cast<std::size_t> (selected - 1) >= inspectorTakeViews.size())
                return;

            (void) appModel.deleteRecordingTake (
                inspectorTakeViews[static_cast<std::size_t> (selected - 1)].takeId);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (inspectorTakeDelete);

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
            repaintAll();
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
            repaintAll();
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
            repaintAll();
        };
        addAndMakeVisible (mixerTrackSelect);

        // Every mixer strip is selectable (usable-DAW P0): clicking a Track strip retargets the shared
        // fader/pan/mute/solo controls and moves them onto that strip.
        mixerStripsInput.setComponentID ("shell.mixer.strips.input");
        mixerStripsInput.setName ("Mixer Strips");
        mixerStripsInput.setTitle ("Mixer Strips");
        mixerStripsInput.setTooltip ("Mixer strips: click a strip to retarget the shared controls, click a meter to clear its clip light");
        mixerStripsInput.onContextMenuRequested = [this] (yesdaw::ui::ContextMenuTarget target, int index, juce::Point<int> position) {
            openContextMenu (target, index, mixerStripsInput, position);
        };
        mixerStripsInput.stripCountProvider = [this] {
            const auto surface = currentMixerSurface();
            return static_cast<int> (surface.tracks.size() + surface.buses.size());
        };
        mixerStripsInput.onStripClicked = [this] (int stripIndex) {
            const auto surface = currentMixerSurface();
            const int trackCount = static_cast<int> (surface.tracks.size());
            const int busCount = static_cast<int> (surface.buses.size());
            // R11: the lane after the buses is the MASTER strip, selectable for its FX chain.
            if (stripIndex < 0 || stripIndex > trackCount + busCount)
                return;

            // E16: strips past the tracks are the buses, selectable in their own right.
            if (stripIndex < trackCount)
            {
                (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
                selectedTrackLane = stripIndex;   // rail selection follows the mixer strip
            }
            else if (stripIndex < trackCount + busCount)
            {
                (void) appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount));
            }
            else
            {
                (void) appModel.selectMixerMaster();
            }
            layoutMixerControls();
            refreshActionState();
            repaintAll();
        };
        // E17: double-clicking a BUS strip opens the inline rename editor over its header.
        mixerStripsInput.onStripDoubleClicked = [this] (int stripIndex) {
            const auto surface = currentMixerSurface();
            const int trackCount = static_cast<int> (surface.tracks.size());
            const int busCount = static_cast<int> (surface.buses.size());
            if (stripIndex < trackCount || stripIndex >= trackCount + busCount)
                return;

            openBusRenameEditor (stripIndex - trackCount, stripIndex);
        };
        mixerStripsInput.meterStripAtPosition = [this] (juce::Point<int> positionInShell) {
            // E22: bus meters are clickable like track meters — the index spans tracks then buses.
            const std::size_t stripTotal = appModel.context().projectLoaded
                                               ? appModel.project().tracks.size()
                                                     + appModel.project().buses.size()
                                               : 0u;
            for (std::size_t i = 0; i < stripTotal; ++i)
                if (paintedMeterBoundsForLane (paintedMixerLaneBounds (i)).contains (positionInShell))
                    return static_cast<int> (i);
            return -1;
        };
        mixerStripsInput.onMeterClicked = [this] (int stripIndex) {
            const int trackCount = static_cast<int> (appModel.project().tracks.size());
            if (stripIndex < trackCount)
                clearTrackMeterHold (stripIndex);
            else
                clearBusMeterHold (stripIndex - trackCount);   // E22
        };
        // N1: a click on a painted Mute/Solo cell toggles THAT strip. It is not a selection
        // gesture — the mixer target the control lane edits stays where the user put it.
        mixerStripsInput.muteSoloCellAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
            for (std::size_t i = 0; i < stripTotal; ++i)
            {
                const auto lane = paintedMixerLaneBounds (i);
                for (std::size_t cell = 0; cell < kMixerPaintedMuteSoloCellCount; ++cell)
                    if (paintedMuteSoloCellBoundsForLane (lane, cell).contains (positionInShell))
                        return std::pair<int, int> { static_cast<int> (i), static_cast<int> (cell) };
            }
            return std::pair<int, int> { -1, -1 };
        };
        mixerStripsInput.onMuteSoloCellClicked = [this] (int stripIndex, int cellIndex) {
            const auto& tracks = appModel.project().tracks;
            const auto& buses = appModel.project().buses;
            if (stripIndex < 0)
                return;

            const std::size_t strip = static_cast<std::size_t> (stripIndex);
            const bool solo = cellIndex == 0;
            if (strip < tracks.size())
            {
                const yesdaw::engine::EntityId trackId = tracks[strip].id;
                (void) (solo ? appModel.toggleTrackSolo (trackId) : appModel.toggleTrackMute (trackId));
            }
            else if (strip - tracks.size() < buses.size())
            {
                const yesdaw::engine::EntityId busId = buses[strip - tracks.size()].id;
                (void) (solo ? appModel.toggleBusSolo (busId) : appModel.toggleBusMute (busId));
            }

            refreshActionState();
            repaintAll();
        };
        // M4: a click on a painted insert row selects the strip and opens THAT slot's params.
        mixerStripsInput.insertSlotAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
            for (std::size_t i = 0; i < stripTotal; ++i)
            {
                const auto lane = paintedMixerLaneBounds (i);
                for (std::size_t slot = 0; slot < static_cast<std::size_t> (
                         paintedInsertRowCountForLane (lane)); ++slot)
                    if (paintedInsertRowBoundsForLane (lane, slot).contains (positionInShell))
                        return std::pair<int, int> { static_cast<int> (i), static_cast<int> (slot) };
            }
            return std::pair<int, int> { -1, -1 };
        };
        // M5: painted send rows. The press selects the strip and previews the level; the release
        // commits ONE undoable SetSendLevel through the same model verb the control lane uses.
        mixerStripsInput.sendRowAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
            for (std::size_t i = 0; i < stripTotal; ++i)
            {
                const auto lane = paintedMixerLaneBounds (i);
                for (std::size_t sendIndex = 0;
                     sendIndex < static_cast<std::size_t> (paintedSendRowCountForLane (lane));
                     ++sendIndex)
                    if (paintedSendRowBoundsForLane (lane, sendIndex).contains (positionInShell))
                        return std::pair<int, int> { static_cast<int> (i), static_cast<int> (sendIndex) };
            }
            return std::pair<int, int> { -1, -1 };
        };
        mixerStripsInput.sendLevelForPosition = [this] (int stripIndex, int sendIndex, juce::Point<int> positionInShell) {
            const auto row = paintedSendRowBoundsForLane (
                paintedMixerLaneBounds (static_cast<std::size_t> (juce::jmax (0, stripIndex))),
                static_cast<std::size_t> (juce::jmax (0, sendIndex)));
            const auto bar = row.reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX,
                                          yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX);
            if (bar.getWidth() <= 0)
                return 0.0;

            return std::clamp (static_cast<double> (positionInShell.x - bar.getX())
                                   / static_cast<double> (bar.getWidth()),
                               0.0,
                               1.0);
        };
        mixerStripsInput.onSendRowDragged = [this] (int stripIndex, int sendIndex, double level, bool commit) {
            const auto surface = currentMixerSurface();
            const int trackCount = static_cast<int> (surface.tracks.size());
            if (stripIndex < 0 || stripIndex >= trackCount)
                return;                                   // sends originate on TRACKS only (E16)

            if (static_cast<std::size_t> (sendIndex) >= surface.tracks[static_cast<std::size_t> (stripIndex)].sends.size())
                return;                                   // an empty send well has nothing to set

            (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
            selectedTrackLane = stripIndex;
            if (! commit)
            {
                paintedSendDragPreview = { stripIndex, sendIndex, static_cast<float> (level) };
                repaintAll();
                return;
            }

            paintedSendDragPreview = {};
            (void) appModel.setSendLevelOnSelectedTrack (static_cast<std::size_t> (sendIndex),
                                                        static_cast<float> (level));
            layoutMixerControls();
            refreshActionState();
            repaintAll();
        };
        mixerStripsInput.onInsertSlotClicked = [this] (int stripIndex, int slotIndex) {
            const auto surface = currentMixerSurface();
            const int trackCount = static_cast<int> (surface.tracks.size());
            const int busCount = static_cast<int> (surface.buses.size());
            if (stripIndex < 0 || stripIndex >= trackCount + busCount)
                return;

            if (stripIndex < trackCount)
            {
                (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
                selectedTrackLane = stripIndex;
            }
            else
            {
                (void) appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount));
            }

            // An empty slot has nothing to edit — the param panel closes instead of lying.
            const std::size_t chainSize = appModel.selectedStripFxChain().size();
            selectedFxParamSlot = static_cast<std::size_t> (slotIndex) < chainSize ? slotIndex : -1;
            selectedFxParamPage = 0;
            layoutMixerControls();
            refreshActionState();
            resized();
            repaintAll();
        };
        // E25: clicks hit-test the PAINTED lanes — the same geometry the eye sees.
        mixerStripsInput.stripAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            // R11: one lane past the buses — the master strip's lane — hit-tests too.
            const std::size_t stripTotal = surface.tracks.size() + surface.buses.size() + 1u;
            for (std::size_t i = 0; i < stripTotal; ++i)
                if (paintedMixerLaneBounds (i).contains (positionInShell))
                    return static_cast<int> (i);
            return -1;
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
            repaintAll();
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
                repaintAll();
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
                repaintAll();
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
                selectedFxParamPage = 0;   // E15: a fresh slot always opens on its first page
                refreshActionState();
                resized();
                repaintAll();
            };
            addChildComponent (edit);

            // E14: move the insert one position earlier or later in the chain, undoably.
            auto& up = mixerFxSlotUps[slot];
            up.setButtonText ("^");
            up.setComponentID ("mixer.fx.slot." + juce::String (static_cast<int> (slot)) + ".up");
            up.setTooltip ("Move FX slot " + juce::String (static_cast<int> (slot) + 1) + " earlier in the chain");
            up.setName ("Move FX slot " + juce::String (static_cast<int> (slot + 1)) + " earlier");
            up.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
            up.setColour (juce::TextButton::textColourOffId, kText);
            up.onClick = [this, slot] {
                (void) appModel.moveFxInsertOnSelectedStrip (slot, -1);
                refreshActionState();
                repaintAll();
            };
            addChildComponent (up);

            auto& down = mixerFxSlotDowns[slot];
            down.setButtonText ("v");
            down.setComponentID ("mixer.fx.slot." + juce::String (static_cast<int> (slot)) + ".down");
            down.setTooltip ("Move FX slot " + juce::String (static_cast<int> (slot) + 1) + " later in the chain");
            down.setName ("Move FX slot " + juce::String (static_cast<int> (slot + 1)) + " later");
            down.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
            down.setColour (juce::TextButton::textColourOffId, kText);
            down.onClick = [this, slot] {
                (void) appModel.moveFxInsertOnSelectedStrip (slot, 1);
                refreshActionState();
                repaintAll();
            };
            addChildComponent (down);
        }

        // Send routing (ADR-0044): + Bus creates a persisted Bus; the send chooser routes the
        // selected track to a bus; each visible send row edits its level and removes undoably.
        // E19: the master fader edits the persisted master gain undoably.
        configureActionComponent (mixerMasterFader, yesdaw::ui::UiActionId::MixerMasterSetFader, "Master fader");
        mixerMasterFader.setSliderStyle (juce::Slider::LinearVertical);
        mixerMasterFader.setTextBoxStyle (juce::Slider::NoTextBox,
                                          false,
                                          yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                          yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        mixerMasterFader.setRange (yesdaw::ui::UiTheme::Layout::mixerFaderSliderMin,
                                   yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax,
                                   yesdaw::ui::UiTheme::Layout::mixerFaderSliderInterval);
        mixerMasterFader.setValue (yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault,
                                   juce::dontSendNotification);
        mixerMasterFader.setDoubleClickReturnValue (true, yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault);
        // E21: a master fader drag is ONE undo step.
        mixerMasterFader.onDragStart = [this] { appModel.beginStripGesture(); };
        mixerMasterFader.onDragEnd = [this] { appModel.endStripGesture(); };
        mixerMasterFader.onValueChange = [this] {
            if (refreshingMixerControls || ! mixerMasterFader.isEnabled())
                return;

            if (mixerMasterFader.isMouseButtonDown())
                appModel.beginStripGesture();

            (void) appModel.setMasterFader (static_cast<float> (mixerMasterFader.getValue()));
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerMasterFader);

        configureActionComponent (mixerBusAddButton, yesdaw::ui::UiActionId::MixerBusAdd, "Add bus");
        mixerBusAddButton.setButtonText ("+ Bus");
        mixerBusAddButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        mixerBusAddButton.setColour (juce::TextButton::textColourOffId, kText);
        mixerBusAddButton.onClick = [this] {
            (void) appModel.addBusToMixer();
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerBusAddButton);

        // E17: remove the SELECTED bus; the engine refuses while sends still route to it, and
        // the refusal leaves the bus in place (the gate pins that honesty).
        configureActionComponent (mixerBusRemoveButton, yesdaw::ui::UiActionId::MixerBusRemove, "Remove bus");
        mixerBusRemoveButton.setButtonText ("- Bus");
        mixerBusRemoveButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::warningButton());
        mixerBusRemoveButton.setColour (juce::TextButton::textColourOffId, kText);
        mixerBusRemoveButton.onClick = [this] {
            (void) appModel.removeSelectedBus();
            layoutMixerControls();
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerBusRemoveButton);

        // E17: inline bus rename editor, the marker/clip editor pattern on the mixer panel.
        busRenameEditor.setComponentID ("shell.mixer.bus.rename");
        busRenameEditor.setTooltip ("Rename bus: Enter commits, Escape cancels");
        busRenameEditor.setName ("Rename bus");
        busRenameEditor.setSelectAllWhenFocused (true);
        busRenameEditor.onReturnKey = [this] { commitBusRenameEditor(); };
        busRenameEditor.onEscapeKey = [this] { dismissBusRenameEditor(); };
        busRenameEditor.onFocusLost = [this] { dismissBusRenameEditor(); };
        addChildComponent (busRenameEditor);

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
            repaintAll();
        };
        addAndMakeVisible (mixerSendAddChooser);

        // M3: where the selected TRACK's main output lands — master (the default) or a bus. This is
        // the submix/group route, not a parallel send: the whole strip moves.
        configureActionComponent (mixerTrackOutputChooser, yesdaw::ui::UiActionId::MixerTrackSetOutput,
                                  "Track output");
        mixerTrackOutputChooser.setTextWhenNothingSelected ("Out: Master");
        mixerTrackOutputChooser.setTextWhenNoChoicesAvailable ("Out: Master");
        mixerTrackOutputChooser.onChange = [this] {
            if (refreshingSendControls)
                return;

            const int selected = mixerTrackOutputChooser.getSelectedId();
            if (selected <= 0)
                return;

            const auto& buses = appModel.project().buses;
            const yesdaw::engine::EntityId target =
                selected == 1 || static_cast<std::size_t> (selected - 2) >= buses.size()
                    ? yesdaw::engine::EntityId {}
                    : buses[static_cast<std::size_t> (selected - 2)].id;
            (void) appModel.setOutputOnSelectedTrack (target);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerTrackOutputChooser);

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
            // R15: a send-level drag rides Touch/Latch exactly like the fader — the lane value
            // is the FaderNode dB-law inverse of the dragged gain, so playback lands at the
            // gain that was actually ridden (send lanes drive the send's own FaderNode).
            slider.onDragStart = [this, row] {
                beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::SendLevel,
                                                 static_cast<std::uint32_t> (row));
            };
            slider.onDragEnd = [this] { endAutomationTouchRideIfActive(); };
            slider.onValueChange = [this, row] {
                if (refreshingSendControls)
                    return;

                if (automationTouchRideActive)
                    recordAutomationTouchSample (
                        automationNormalizedForFaderGain (mixerSendLevelSliders[row].getValue()));
                else
                    (void) appModel.setSendLevelOnSelectedTrack (
                        row, static_cast<float> (mixerSendLevelSliders[row].getValue()));
                refreshActionState();
                repaintAll();
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
                repaintAll();
            };
            addChildComponent (remove);

            // E18: per-row tap toggle (pre/post fader) through the undoable SetSendTap verb.
            auto& tap = mixerSendTaps[row];
            tap.setComponentID ("mixer.send." + juce::String (static_cast<int> (row)) + ".tap");
            tap.setTooltip ("Toggle send " + juce::String (static_cast<int> (row) + 1) + " pre/post fader tap");
            tap.setName ("Toggle send " + juce::String (static_cast<int> (row + 1)) + " tap");
            tap.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
            tap.setColour (juce::TextButton::textColourOffId, kText);
            tap.onClick = [this, row] {
                (void) appModel.toggleSendTapOnSelectedTrack (row);
                refreshActionState();
                repaintAll();
            };
            addChildComponent (tap);

            // E18: per-row destination chooser re-routes the send (remove+add, one undo group).
            auto& destination = mixerSendDestinations[row];
            destination.setComponentID ("mixer.send." + juce::String (static_cast<int> (row)) + ".dest");
            destination.setTooltip ("Re-route send " + juce::String (static_cast<int> (row) + 1) + " to another bus");
            destination.onChange = [this, row] {
                if (refreshingSendControls)
                    return;

                const int selected = mixerSendDestinations[row].getSelectedId();
                if (selected <= 0)
                    return;

                (void) appModel.setSendDestinationOnSelectedTrack (
                    row, static_cast<std::size_t> (selected - 1));
                refreshActionState();
                repaintAll();
            };
            addChildComponent (destination);
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
            // R15: an FX-param drag rides Touch/Latch too. The ride owner is the INSERT's own
            // id (the FxInsertParam lane law); the slider value is already the normalized 0..1
            // the lane and the node both speak.
            slider.onDragStart = [this, index] {
                const std::vector<yesdaw::engine::FxInsert> chain = appModel.selectedStripFxChain();
                if (selectedFxParamSlot >= 0
                    && static_cast<std::size_t> (selectedFxParamSlot) < chain.size())
                    beginAutomationTouchRideIfArmed (
                        yesdaw::engine::AutomationTargetRole::FxInsertParam,
                        mixerFxParamSliderIds[index],
                        chain[static_cast<std::size_t> (selectedFxParamSlot)].id);
            };
            slider.onDragEnd = [this] { endAutomationTouchRideIfActive(); };
            slider.onValueChange = [this, index] {
                if (refreshingFxParamControls || selectedFxParamSlot < 0)
                    return;

                if (automationTouchRideActive)
                    recordAutomationTouchSample (mixerFxParamSliders[index].getValue());
                else
                    (void) appModel.setFxInsertParamOnSelectedStrip (
                        static_cast<std::size_t> (selectedFxParamSlot),
                        mixerFxParamSliderIds[index],
                        mixerFxParamSliders[index].getValue());
                refreshActionState();
                repaintAll();
            };
            addChildComponent (slider);

            // E15: choice-shaped params (EQ band type, delay ping-pong) get a real chooser in
            // place of the raw slider.
            auto& choiceChooser = mixerFxParamChoosers[index];
            configureActionComponent (choiceChooser, yesdaw::ui::UiActionId::MixerFxInsertParamSet,
                                      "FX parameter choice");
            choiceChooser.setComponentID ("mixer.fx.param." + juce::String (static_cast<int> (index))
                                          + ".choice");
            choiceChooser.onChange = [this, index] {
                if (refreshingFxParamControls || selectedFxParamSlot < 0)
                    return;

                const int choice = mixerFxParamChoosers[index].getSelectedId() - 1;
                const std::vector<yesdaw::engine::FxInsert> chain = appModel.selectedStripFxChain();
                if (choice < 0 || static_cast<std::size_t> (selectedFxParamSlot) >= chain.size())
                    return;

                const yesdaw::engine::ParamSpec spec = yesdaw::engine::fxParamSpecForKind (
                    chain[static_cast<std::size_t> (selectedFxParamSlot)].kind,
                    mixerFxParamSliderIds[index]);
                (void) appModel.setFxInsertParamOnSelectedStrip (
                    static_cast<std::size_t> (selectedFxParamSlot),
                    mixerFxParamSliderIds[index],
                    yesdaw::engine::normalizedForChoice (spec, static_cast<std::uint8_t> (choice)));
                refreshActionState();
                repaintAll();
            };
            addChildComponent (choiceChooser);
        }

        // E15: params beyond one panel's worth page through this chooser.
        configureActionComponent (mixerFxParamPageChooser, yesdaw::ui::UiActionId::MixerFxInsertParamSet,
                                  "FX parameter page");
        mixerFxParamPageChooser.setComponentID ("mixer.fx.param.page");
        mixerFxParamPageChooser.onChange = [this] {
            if (refreshingFxParamControls)
                return;

            const int page = mixerFxParamPageChooser.getSelectedId() - 1;
            if (page < 0 || page == selectedFxParamPage)
                return;

            selectedFxParamPage = page;
            refreshActionState();
            resized();
            repaintAll();
        };
        addChildComponent (mixerFxParamPageChooser);

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

            // E21: a fader drag is ONE undo step — any mouse-down edit joins the gesture the
            // drag-end closes (lazy so the very first mouse-down value change is included).
            if (mixerFader.isMouseButtonDown())
                appModel.beginStripGesture();
            // N5: an armed Touch/Latch ride buffers the point instead of persisting it — see
            // recordAutomationTouchSample for why persisting on every tick would break it.
            if (automationTouchRideActive)
                recordAutomationTouchSample (automationNormalizedForFaderGain (mixerFader.getValue()));
            else
                (void) appModel.setSelectedMixerFader (static_cast<float> (mixerFader.getValue()));
            if (dragDbReadout.isVisible())
                dragDbReadout.setText (dbReadoutText (mixerFader.getValue()), juce::dontSendNotification);
            refreshActionState();
            repaintAll();
        };
        // Live dB readout while the fader is dragged (B31); the rail VOL shares the same label.
        mixerFader.onDragStart = [this] {
            appModel.beginStripGesture();
            beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::TrackFader,
                                             yesdaw::engine::FaderNode::kGainParameterId);
            showDragDbReadout (mixerFader.getBounds(), mixerFader.getValue());
        };
        // E21: the drag-end both closes the undo gesture and hides the dB readout.
        mixerFader.onDragEnd = [this] {
            endAutomationTouchRideIfActive();
            appModel.endStripGesture();
            hideDragDbReadout();
        };
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
        // E21: a pan drag is ONE undo step.
        mixerPan.onDragStart = [this] {
            appModel.beginStripGesture();
            beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::TrackPan,
                                             yesdaw::engine::PanNode::kPanParameterId);
        };
        mixerPan.onDragEnd = [this] {
            endAutomationTouchRideIfActive();
            appModel.endStripGesture();
        };
        mixerPan.onValueChange = [this] {
            if (refreshingMixerControls || ! mixerPan.isEnabled())
                return;

            if (mixerPan.isMouseButtonDown())
                appModel.beginStripGesture();

            // JUCE snaps values as `rangeStart + interval * n`; with the pan range starting at
            // -1.0, ARM FMA contraction leaves ~2e-17 dust where x64 lands exactly on 0.0. Snap
            // to the same interval grid with cancellation-free arithmetic so dead center
            // persists as exactly 0 on every platform.
            const double snapped = std::round (mixerPan.getValue()
                                               / yesdaw::ui::UiTheme::Layout::mixerPanSliderInterval)
                                 * yesdaw::ui::UiTheme::Layout::mixerPanSliderInterval;
            // N5: an armed Touch/Latch ride buffers the point instead of persisting it.
            if (automationTouchRideActive)
                recordAutomationTouchSample (automationNormalizedForPan (snapped));
            else
                (void) appModel.setSelectedMixerPan (static_cast<float> (snapped));
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerPan);

        configureActionComponent (mixerMetersReadout, yesdaw::ui::UiActionId::MixerReadMeters, "Mixer meters");
        mixerMetersReadout.setButtonText ("Meters");
        mixerMetersReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerMetersReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerMetersReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadMeters);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerMetersReadout);

        configureActionComponent (mixerSendsReadout, yesdaw::ui::UiActionId::MixerReadSends, "Mixer sends");
        mixerSendsReadout.setButtonText ("Sends");
        mixerSendsReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerSendsReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerSendsReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadSends);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerSendsReadout);

        configureActionComponent (mixerSendLevelEdit, yesdaw::ui::UiActionId::MixerSetFirstSendLevel, "Mixer send level");
        mixerSendLevelEdit.setButtonText ("Set send");
        mixerSendLevelEdit.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerSendLevelEdit.setColour (juce::TextButton::textColourOffId, kText);
        mixerSendLevelEdit.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerSetFirstSendLevel);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerSendLevelEdit);

        configureActionComponent (mixerFxSlotsReadout, yesdaw::ui::UiActionId::MixerReadFxSlots, "Mixer FX slots");
        mixerFxSlotsReadout.setButtonText ("Track FX");
        mixerFxSlotsReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerFxSlotsReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerFxSlotsReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadFxSlots);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerFxSlotsReadout);

        configureActionComponent (mixerGainReductionReadout, yesdaw::ui::UiActionId::MixerReadGainReduction, "Mixer gain reduction");
        mixerGainReductionReadout.setButtonText ("Gain reduction");
        mixerGainReductionReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerGainReductionReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerGainReductionReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadGainReduction);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerGainReductionReadout);

        configureActionComponent (mixerBusFxSlotsReadout, yesdaw::ui::UiActionId::MixerReadBusFxSlots, "Mixer Bus FX slots");
        mixerBusFxSlotsReadout.setButtonText ("Bus FX");
        mixerBusFxSlotsReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerBusFxSlotsReadout.setColour (juce::TextButton::textColourOffId, kText);
        mixerBusFxSlotsReadout.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadBusFxSlots);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerBusFxSlotsReadout);

        configureActionComponent (mixerFxSlotToggle, yesdaw::ui::UiActionId::MixerToggleFirstFxSlotEnabled, "Mixer FX slot toggle");
        mixerFxSlotToggle.setButtonText ("Bypass FX");
        mixerFxSlotToggle.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
        mixerFxSlotToggle.setColour (juce::TextButton::textColourOffId, kText);
        mixerFxSlotToggle.onClick = [this] {
            (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerToggleFirstFxSlotEnabled);
            refreshActionState();
            repaintAll();
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
            repaintAll();
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
            repaintAll();
        };
        addAndMakeVisible (mixerSolo);

        // R10: solo-safe on the selected strip — a real labelled toggle, so the flag that
        // decides whether a solo elsewhere silences this strip is visible and clickable.
        configureActionComponent (mixerSoloSafe, yesdaw::ui::UiActionId::MixerTargetToggleSoloSafe,
                                  "Mixer solo safe");
        mixerSoloSafe.setButtonText ("Safe");
        mixerSoloSafe.setColour (juce::TextButton::buttonColourId,
                                 yesdaw::ui::UiTheme::Color::buttonSurface());
        mixerSoloSafe.setColour (juce::TextButton::buttonOnColourId,
                                 yesdaw::ui::UiTheme::Color::accentPurpleDeep());
        mixerSoloSafe.setColour (juce::TextButton::textColourOffId, kText);
        mixerSoloSafe.setColour (juce::TextButton::textColourOnId, kText);
        mixerSoloSafe.onClick = [this] {
            if (refreshingMixerControls || ! mixerSoloSafe.isEnabled())
                return;

            (void) appModel.toggleSelectedMixerSoloSafe();
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (mixerSoloSafe);
    }

    // G1.4: the inspector's width right now — the token, or nothing while it is hidden (I).
    [[nodiscard]] int inspectorWidthNow() const noexcept
    {
        return appModel.context().inspectorVisible ? kInspectorWidth : 0;
    }

    [[nodiscard]] juce::Rectangle<int> timelineBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
        work.removeFromBottom (dockedMixerHeight());
        work.removeFromLeft (kLeftRailWidth);
        work.removeFromRight (inspectorWidthNow());
        return work.reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                             yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    }

    // The exact rect drawTrackList paints into; the rail input overlay shares it so hits match paint.
    [[nodiscard]] juce::Rectangle<int> leftRailPanelBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
        work.removeFromBottom (dockedMixerHeight());
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
        repaintAll();
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
        repaintAll();
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
        const int top = geometry.clipArea.getY()
                      + static_cast<int> (std::llround (
                            geometry.laneTop (clip.lane) - geometry.viewport.laneScrollPixels));
        const int width = juce::roundToInt (clip.lengthSeconds * geometry.viewport.pixelsPerSecond);
        juce::Rectangle<int> bounds {
            left, top, width, static_cast<int> (std::llround (geometry.laneHeightFor (clip.lane))) };
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
        repaintAll();
    }

    // Marker rename (E7): positioned over the painted label through the shared rect law.
    void openMarkerRenameEditor (int markerIndex)
    {
        const auto& markers = appModel.project().markers;
        if (markerIndex < 0 || markerIndex >= static_cast<int> (markers.size()))
            return;

        dismissTrackRenameEditor();
        dismissClipRenameEditor();
        const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
        juce::Rectangle<int> bounds = yesdaw::ui::timelineMarkerLabelRect (
            timelineInput.getLocalBounds(), state, markerIndex)
                                          .translated (timelineInput.getX(), timelineInput.getY());
        if (bounds.isEmpty())
            return;

        markerRenameIndex = markerIndex;
        markerRenameEditor.setBounds (bounds);
        markerRenameEditor.setText (juce::String (markers[static_cast<std::size_t> (markerIndex)].name.c_str()),
                                    juce::dontSendNotification);
        markerRenameEditor.setVisible (true);
        markerRenameEditor.grabKeyboardFocus();
    }

    // E17: inline bus rename — the editor sits over the bus strip's header area.
    void openBusRenameEditor (int busIndex, int stripOrdinal)
    {
        const auto& buses = appModel.project().buses;
        if (busIndex < 0 || busIndex >= static_cast<int> (buses.size()))
            return;

        busRenameIndex = busIndex;
        busRenameEditor.setBounds (
            mixerStripBounds (stripOrdinal)
                .removeFromTop (yesdaw::ui::UiTheme::Layout::mixerTrackSelectHeight));
        busRenameEditor.setText (juce::String (buses[static_cast<std::size_t> (busIndex)].strip.name),
                                 juce::dontSendNotification);
        busRenameEditor.setVisible (true);
        busRenameEditor.grabKeyboardFocus();
    }

    void commitBusRenameEditor()
    {
        if (busRenameIndex >= 0)
            (void) appModel.renameBusAt (static_cast<std::size_t> (busRenameIndex),
                                         busRenameEditor.getText().toStdString());
        dismissBusRenameEditor();
        refreshActionState();
        repaintAll();
    }

    void dismissBusRenameEditor()
    {
        busRenameIndex = -1;
        busRenameEditor.setVisible (false);
    }

    void commitMarkerRenameEditor()
    {
        const auto& markers = appModel.project().markers;
        if (markerRenameIndex >= 0 && markerRenameIndex < static_cast<int> (markers.size()))
            (void) appModel.renameTimelineMarker (
                markers[static_cast<std::size_t> (markerRenameIndex)].id,
                markerRenameEditor.getText().toStdString());
        dismissMarkerRenameEditor();
        refreshActionState();
        repaintAll();
    }

    void dismissMarkerRenameEditor()
    {
        markerRenameIndex = -1;
        markerRenameEditor.setVisible (false);
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
        repaintAll();
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
        repaintAll();
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
        repaintAll();
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
        trackMeterHoldLR.resize (tracks.size());
        for (std::size_t i = 0; i < tracks.size(); ++i)
        {
            float peak = playing ? appModel.trackMeterPeak (tracks[i].id) : 0.0f;
            // V5: the rail meters L and R independently from the MeterNode's per-channel peaks;
            // the aggregate hold stays for the mixer strip's single-column meter.
            float peakL = playing ? appModel.trackMeterPeakChannel (tracks[i].id, 0) : 0.0f;
            float peakR = playing ? appModel.trackMeterPeakChannel (tracks[i].id, 1) : 0.0f;
            // E30: the ARMED track's rail meter also shows the live input peak, so signal is
            // visible before recording — playing or stopped. M11: each armed track shows its
            // OWN picked input, so a whole armed kit meters honestly. V5: the picked input is a
            // single pre-track signal, so it honestly lights both channels.
            if (appModel.isRecordingTrackIndexArmed (i))
            {
                const float inputPeak = appModel.inputMeterPeakForTrackIndex (i);
                peak = std::max (peak, inputPeak);
                peakL = std::max (peakL, inputPeak);
                peakR = std::max (peakR, inputPeak);
            }
            advanceMeterHold (trackMeterHold[i], peak);
            advanceMeterHold (trackMeterHoldLR[i][0], peakL);
            advanceMeterHold (trackMeterHoldLR[i][1], peakR);
        }

        // E22: bus meters live on exactly the same B32 law.
        const auto& buses = appModel.project().buses;
        busMeterHold.resize (buses.size());
        for (std::size_t i = 0; i < buses.size(); ++i)
            advanceMeterHold (busMeterHold[i],
                              playing ? appModel.busMeterPeak (buses[i].id) : 0.0f);
    }

    void clearTrackMeterHold (int trackIndex)
    {
        if (trackIndex < 0 || trackIndex >= static_cast<int> (trackMeterHold.size()))
            return;

        MeterHoldState& state = trackMeterHold[static_cast<std::size_t> (trackIndex)];
        state.clipLatched = false;
        state.heldPeak = state.livePeak;
        state.holdTicksRemaining = 0;
        // V5: one click on the meter zone clears the L/R columns with the aggregate.
        if (trackIndex < static_cast<int> (trackMeterHoldLR.size()))
            for (MeterHoldState& channel : trackMeterHoldLR[static_cast<std::size_t> (trackIndex)])
            {
                channel.clipLatched = false;
                channel.heldPeak = channel.livePeak;
                channel.holdTicksRemaining = 0;
            }
        repaintAll();
    }

    // E22: a click on a painted BUS meter clears its hold and latch, like the track law.
    void clearBusMeterHold (int busIndex)
    {
        if (busIndex < 0 || busIndex >= static_cast<int> (busMeterHold.size()))
            return;

        MeterHoldState& state = busMeterHold[static_cast<std::size_t> (busIndex)];
        state.clipLatched = false;
        state.heldPeak = state.livePeak;
        state.holdTicksRemaining = 0;
        repaintAll();
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
        repaintAll();
    }

    // Open a project bundle at a known path (B39): shared by File > Open and Open Recent.
    void openProjectBundleAtPath (const std::filesystem::path& path)
    {
        const StoredProjectAssetsResult stored = decodeStoredProjectAssets (path);
        if (stored.assets && ! stored.assets->empty())
            (void) appModel.loadProjectBundle (
                path,
                std::span<const yesdaw::ui::UiDecodedAsset> (
                    stored.assets->data(), stored.assets->size()));
        else if (stored.assets)
            (void) appModel.openProjectBundle (path);
        else
            // R5: a project that cannot open says WHY (naming the bad audio file when one is
            // the cause) and refuses to half-open — the shell state stays untouched.
            appModel.reportStatus (
                "Open failed: " + stored.failureReason
                    + " (" + path.filename().string() + ")",
                true);
    }

    // V3: the always-on bottom dock's height — 0 collapses it entirely, reclaiming the space for
    // the timeline/rail/inspector, when the user has toggled it off (the full-view Mixer panel
    // is unaffected: it never reserves this space in the first place). ONE law shared by every
    // layout function below, so paint and every interactive component's bounds can never drift.
    [[nodiscard]] int dockedMixerHeight() const
    {
        return appModel.context().mixerDockVisible ? kMixerHeight : 0;
    }

    [[nodiscard]] juce::Rectangle<int> mixerPanelBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
        auto mixer = appModel.context().activePanel == yesdaw::ui::UiPanel::Mixer
                         ? work
                         : work.removeFromBottom (dockedMixerHeight());
        return mixer.reduced (yesdaw::ui::UiTheme::Layout::mixerPanelHorizontalInset,
                              yesdaw::ui::UiTheme::Layout::mixerPanelVerticalInset);
    }

    // E25: ONE strip geometry — the interactive control lane, the click law, and the paint all
    // share the painted-strip law (the old width/(count+1) law visibly diverged from the
    // painted lanes at real window sizes, floating the control lane off its strip).
    [[nodiscard]] juce::Rectangle<int> mixerStripBounds (int stripIndex) const
    {
        return paintedMixerLaneBounds (static_cast<std::size_t> (juce::jmax (0, stripIndex)));
    }

    [[nodiscard]] juce::Rectangle<int> mixerFirstStripBounds() const { return mixerStripBounds (0); }

    // N3: the painted MASTER pane rect. Master is lane index stripCount in the SAME
    // paintedMixerLaneBounds law every track/bus strip uses — it is the strip immediately after
    // the last one, never a detached island computed from the far right of a stale area. Before
    // N3 this peeled its slice off the right edge of the FULL panel independently of how many
    // strips were drawn from the left, so a clamped strip width (max 112px) left ~1250px of dead
    // black between the last strip and master at 1920x1080.
    [[nodiscard]] juce::Rectangle<int> paintedMixerMasterBounds() const
    {
        const auto surface = currentMixerSurface();
        const std::size_t stripCount = surface.tracks.size() + surface.buses.size();
        return paintedMixerLaneBounds (stripCount);
    }

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

    // M4: how many insert rows this strip can afford. A tall mixer-view strip shows the whole
    // column; the timeline view's short mini-mixer drops rows rather than starving the fader, and a
    // strip with no room at all falls back to the exact historical fader top.
    [[nodiscard]] static int paintedInsertRowCountForLane (juce::Rectangle<int> lane) noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const int available = lane.getHeight() - L::mixerPaintedInsertsTop
                            - L::mixerPaintedFaderBottomInset - L::mixerPaintedFaderMinHeight
                            - L::mixerPaintedInsertsFaderGap;
        return std::clamp (available / L::mixerPaintedInsertRowPitch, 0, L::mixerPaintedInsertRowCount);
    }

    // M6: ONE fader mapping. The sliders travel 0..mixerFaderSliderMax in LINEAR gain, so the
    // painted thumb, the unity mark and every dB tick must read the same law — before M6 the paint
    // put unity at the TOP of the rail while the live slider put it at half travel.
    [[nodiscard]] static float mixerFaderFractionForGain (float linearGain) noexcept
    {
        const float span = static_cast<float> (yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax);
        return span > 0.0f ? std::clamp (linearGain / span, 0.0f, 1.0f) : 0.0f;
    }

    [[nodiscard]] static float mixerFaderFractionForDb (float db) noexcept
    {
        return mixerFaderFractionForGain (std::pow (10.0f, db / 20.0f));
    }

    [[nodiscard]] static juce::Rectangle<int> paintedFaderRailForLane (juce::Rectangle<int> lane)
    {
        const auto faderArea = lane.withTrimmedTop (paintedFaderTopForLane (lane))
                                   .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderBottomInset);
        return faderArea.withWidth (yesdaw::ui::UiTheme::Layout::mixerPaintedRailWidth)
                        .withCentre ({ lane.getCentreX()
                                           - yesdaw::ui::UiTheme::Layout::mixerPaintedRailCenterOffsetX,
                                       faderArea.getCentreY() });
    }

    [[nodiscard]] static int mixerFaderThumbYForGain (juce::Rectangle<int> rail, float linearGain) noexcept
    {
        return rail.getBottom()
             - juce::roundToInt (mixerFaderFractionForGain (linearGain) * static_cast<float> (rail.getHeight()));
    }

    // M5: the send rows follow the inserts, and take space only after the inserts have taken
    // theirs — a short strip drops sends first, then inserts, and never starves the fader.
    [[nodiscard]] static int paintedSendRowCountForLane (juce::Rectangle<int> lane) noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const int used = paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch;
        const int available = lane.getHeight() - L::mixerPaintedInsertsTop - used
                            - L::mixerPaintedFaderBottomInset - L::mixerPaintedFaderMinHeight
                            - L::mixerPaintedInsertsFaderGap;
        return std::clamp (available / L::mixerPaintedSendRowPitch, 0, L::mixerPaintedSendRowCount);
    }

    [[nodiscard]] static int paintedSendsTopForLane (juce::Rectangle<int> lane) noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        return L::mixerPaintedInsertsTop
             + paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch;
    }

    [[nodiscard]] static juce::Rectangle<int> paintedSendRowBoundsForLane (juce::Rectangle<int> lane,
                                                                           std::size_t sendIndex)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        if (static_cast<int> (sendIndex) >= paintedSendRowCountForLane (lane))
            return {};

        const int top = lane.getY() + paintedSendsTopForLane (lane)
                      + static_cast<int> (sendIndex) * L::mixerPaintedSendRowPitch;
        return juce::Rectangle<int> (lane.getX() + L::mixerPaintedInsertsInsetX,
                                     top,
                                     juce::jmax (0, lane.getWidth() - 2 * L::mixerPaintedInsertsInsetX),
                                     L::mixerPaintedSendRowHeight);
    }

    [[nodiscard]] static int paintedFaderTopForLane (juce::Rectangle<int> lane) noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const int rows = paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch
                       + paintedSendRowCountForLane (lane) * L::mixerPaintedSendRowPitch;
        return rows == 0
            ? L::mixerPaintedFaderTop
            : L::mixerPaintedFaderTop + rows + L::mixerPaintedInsertsFaderGap;
    }

    // N1: ONE Mute/Solo cell law — the paint, the click hit-test, the SELECTED strip's live
    // buttons and the gates all read it, so the control you see is exactly the control you hit,
    // on every strip. Cell 0 is Solo, cell 1 is Mute (left to right, as painted).
    [[nodiscard]] static juce::Rectangle<int> paintedMuteSoloCellBoundsForLane (juce::Rectangle<int> lane,
                                                                                std::size_t cellIndex)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        if (cellIndex >= kMixerPaintedMuteSoloCellCount)
            return {};

        auto buttonsRow = lane.withTrimmedTop (L::mixerPaintedButtonsTop)
                              .withHeight (L::mixerPaintedButtonsHeight)
                              .reduced (L::mixerPaintedButtonsInsetX, L::mixerPaintedButtonsInsetY);
        buttonsRow.removeFromLeft (static_cast<int> (cellIndex) * L::mixerPaintedButtonWidth);
        return buttonsRow.removeFromLeft (L::mixerPaintedButtonWidth)
                         .reduced (L::mixerPaintedButtonInsetX, L::mixerPaintedButtonInsetY);
    }

    // M4: ONE insert-slot row law — the paint, the click hit-test and the gates all read it, so a
    // painted slot can never drift from the slot a click selects. An empty rect means the strip has
    // no room for that row.
    [[nodiscard]] static juce::Rectangle<int> paintedInsertRowBoundsForLane (juce::Rectangle<int> lane,
                                                                             std::size_t slotIndex)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        if (static_cast<int> (slotIndex) >= paintedInsertRowCountForLane (lane))
            return {};

        const int top = lane.getY() + L::mixerPaintedInsertsTop
                      + static_cast<int> (slotIndex) * L::mixerPaintedInsertRowPitch;
        return juce::Rectangle<int> (lane.getX() + L::mixerPaintedInsertsInsetX,
                                     top,
                                     juce::jmax (0, lane.getWidth() - 2 * L::mixerPaintedInsertsInsetX),
                                     L::mixerPaintedInsertRowHeight);
    }

    [[nodiscard]] static juce::Rectangle<int> paintedMeterBoundsForLane (juce::Rectangle<int> lane)
    {
        auto faderArea = lane.withTrimmedTop (paintedFaderTopForLane (lane))
                             .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderBottomInset);
        return faderArea.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterWidth)
                        .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetX,
                                  yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetY);
    }

    [[nodiscard]] juce::Rectangle<int> inspectorBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
        work.removeFromBottom (dockedMixerHeight());
        if (! appModel.context().inspectorVisible)
            return {};
        return work.removeFromRight (kInspectorWidth)
            .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                      yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    }

    void layoutInspectorControls()
    {
        auto area = inspectorBounds();
        // V7: the tab strip is owned by the two REAL tab buttons — one shared cell law for
        // layout and (absence of) paint, so the clickable tabs can never drift from the strip.
        auto tabStrip = area.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorTabHeight);
        inspectorClipTab.setBounds (
            tabStrip.removeFromLeft (tabStrip.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorTabCount));
        inspectorTrackTab.setBounds (tabStrip);
        area.reduce (yesdaw::ui::UiTheme::Layout::inspectorContentInsetX,
                     yesdaw::ui::UiTheme::Layout::inspectorContentInsetY);

        // V7: the TRACK tab shows track-scoped painted content only — every clip-scoped overlay
        // control drops WHOLE (the same empty-bounds law the section-fit drop already uses).
        if (appModel.context().inspectorTrackTabActive)
        {
            inspectorStart.setBounds ({});
            inspectorEnd.setBounds ({});
            inspectorLength.setBounds ({});
            inspectorGain.setBounds ({});
            inspectorFadeIn.setBounds ({});
            inspectorFadeOut.setBounds ({});
            inspectorFadeCurve.setBounds ({});
            inspectorTakeChooser.setBounds ({});
            inspectorTakeDelete.setBounds ({});
            return;
        }
        // E24/E27: ONE law for paint and controls — an inspector section that no longer fits
        // the column is dropped WHOLE (card, labels, and controls), never split across the
        // panel edge or bled over the bottom mixer panel.
        const juce::Rectangle<int> content = area;
        const auto sectionFits = [content] (juce::Rectangle<int> section)
        {
            return content.contains (section);
        };
        const auto statsSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                                      .withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionHeight);
        auto stats = statsSection;
        auto startCell = stats.removeFromLeft (stats.getWidth() / yesdaw::ui::UiTheme::Layout::inspectorStatsColumnCount)
                              .reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                        yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        auto endCell = stats.removeFromLeft (stats.getWidth() / (yesdaw::ui::UiTheme::Layout::inspectorStatsColumnCount - 1))
                            .reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                      yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        auto lengthCell = stats.reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                         yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        const bool statsFit = sectionFits (statsSection);
        inspectorStart.setBounds (statsFit ? startCell : juce::Rectangle<int>());
        inspectorEnd.setBounds (statsFit ? endCell : juce::Rectangle<int>());
        inspectorLength.setBounds (statsFit ? lengthCell : juce::Rectangle<int>());

        const auto gainSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorGainSectionTop)
                                     .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainSectionHeight);
        auto gain = gainSection;
        gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlTopInset);
        inspectorGain.setBounds (sectionFits (gainSection)
            ? gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlHeight)
                  .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainControlLeftInset)
            : juce::Rectangle<int>());

        const auto fadesSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionTop)
                                      .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionHeight);
        auto fades = fadesSection;
        fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadesControlTopInset);
        const bool fadesFit = sectionFits (fadesSection);
        inspectorFadeIn.setBounds (fadesFit
            ? fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHeight)
                  .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset)
                  .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHorizontalInset,
                            yesdaw::ui::UiTheme::Layout::inspectorFadeControlVerticalInset)
            : juce::Rectangle<int>());
        inspectorFadeOut.setBounds (fadesFit
            ? fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHeight)
                  .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset)
                  .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeControlHorizontalInset,
                            yesdaw::ui::UiTheme::Layout::inspectorFadeControlVerticalInset)
            : juce::Rectangle<int>());
        fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveControlTopGap);
        inspectorFadeCurve.setBounds (fadesFit
            ? fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveControlHeight)
                  .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset)
            : juce::Rectangle<int>());

        // E33: the TAKES section (the old automation placeholder's area) — the chooser and
        // the delete button share its whole-section drop law.
        auto takesSection = area.withTrimmedTop (
            yesdaw::ui::UiTheme::Layout::inspectorAutomationSectionTop);
        const int takesNeededHeight = yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight
                                    + yesdaw::ui::UiTheme::Layout::inspectorTakeRowHeight
                                    + yesdaw::ui::UiTheme::Layout::inspectorTakeRowGap;
        if (sectionFits (takesSection) && takesSection.getHeight() >= takesNeededHeight)
        {
            takesSection.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight);
            takesSection.reduce (yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetX,
                                 yesdaw::ui::UiTheme::Space::none);
            auto takesRow = takesSection.removeFromTop (
                yesdaw::ui::UiTheme::Layout::inspectorTakeRowHeight);
            inspectorTakeDelete.setBounds (
                takesRow.removeFromRight (yesdaw::ui::UiTheme::Layout::inspectorTakeDeleteWidth));
            takesRow.removeFromRight (yesdaw::ui::UiTheme::Layout::inspectorTakeRowGap);
            inspectorTakeChooser.setBounds (takesRow);
        }
        else
        {
            inspectorTakeChooser.setBounds ({});
            inspectorTakeDelete.setBounds ({});
        }
    }

    void layoutMixerControls()
    {
        auto utility = mixerPanelBounds().withWidth (yesdaw::ui::UiTheme::Layout::mixerToolsWidth)
                           .reduced (yesdaw::ui::UiTheme::Layout::mixerUtilityInsetX,
                                     yesdaw::ui::UiTheme::Space::none);
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerUtilityTop);
        // M9: a row that does not fit the column DROPS (zero bounds) instead of being painted
        // half-off the panel's bottom edge, which is what the shipped floor size did to the last
        // chooser. Same law the send rows already used: hidden rows take no column space.
        const auto placeUtilityRow = [&utility] (juce::Component& component, int height) {
            if (utility.getHeight() < height)
            {
                component.setBounds ({});
                return false;
            }

            component.setBounds (utility.removeFromTop (height));
            return true;
        };
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
            (void) placeUtilityRow (*button, yesdaw::ui::UiTheme::Layout::mixerUtilityHeight);
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerUtilityGap);
        }

        // N1: the selected target's Solo/Mute verbs share one utility row, split in half. They are
        // real labelled buttons here; on the strips themselves Mute/Solo are painted cells driven
        // by the click law, on EVERY strip.
        {
            const int soloMuteRowHeight = yesdaw::ui::UiTheme::Layout::mixerUtilityHeight;
            if (utility.getHeight() >= soloMuteRowHeight)
            {
                auto row = utility.removeFromTop (soloMuteRowHeight);
                mixerSolo.setBounds (row.removeFromLeft (
                    row.getWidth() / static_cast<int> (kMixerPaintedMuteSoloCellCount)));
                mixerMute.setBounds (row);
                utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerUtilityGap);
            }
            else
            {
                mixerSolo.setBounds ({});
                mixerMute.setBounds ({});
            }
        }

        // R10: solo-safe rides its own utility row below Solo/Mute.
        (void) placeUtilityRow (mixerSoloSafe, yesdaw::ui::UiTheme::Layout::mixerUtilityHeight);
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerUtilityGap);

        (void) placeUtilityRow (mixerFxAddChooser, yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight);
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        for (std::size_t slot = 0; slot < mixerFxSlotToggles.size(); ++slot)
        {
            if (! mixerFxSlotToggles[slot].isVisible())
            {
                mixerFxSlotToggles[slot].setBounds ({});
                mixerFxSlotEdits[slot].setBounds ({});
                mixerFxSlotRemoves[slot].setBounds ({});
                mixerFxSlotUps[slot].setBounds ({});
                mixerFxSlotDowns[slot].setBounds ({});
                continue;
            }

            auto slotRow = utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotHeight);
            mixerFxSlotRemoves[slot].setBounds (
                slotRow.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerFxSlotRemoveWidth));
            mixerFxSlotEdits[slot].setBounds (
                slotRow.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerFxSlotRemoveWidth));
            mixerFxSlotDowns[slot].setBounds (
                slotRow.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerFxSlotRemoveWidth));
            mixerFxSlotUps[slot].setBounds (
                slotRow.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerFxSlotRemoveWidth));
            mixerFxSlotToggles[slot].setBounds (slotRow);
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        }

        // Hidden rows take no column space — the tools column would otherwise overflow. The refresh
        // path calls resized() whenever a row-visibility count changes.
        (void) placeUtilityRow (mixerBusAddButton, yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight);
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        // E17: bus removal lives with the bus tools.
        (void) placeUtilityRow (mixerBusRemoveButton, yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight);
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        (void) placeUtilityRow (mixerTrackOutputChooser, yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight);
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        (void) placeUtilityRow (mixerSendAddChooser, yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight);
        utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        for (std::size_t row = 0; row < mixerSendLevelSliders.size(); ++row)
        {
            if (! mixerSendLevelSliders[row].isVisible())
            {
                mixerSendLevelSliders[row].setBounds ({});
                mixerSendLabels[row].setBounds ({});
                mixerSendRemoves[row].setBounds ({});
                mixerSendTaps[row].setBounds ({});
                mixerSendDestinations[row].setBounds ({});
                continue;
            }

            // E18: the send grows a second row for its tap toggle and destination chooser.
            auto sendRow = utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerSendRowHeight);
            mixerSendRemoves[row].setBounds (
                sendRow.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerFxSlotRemoveWidth));
            mixerSendLabels[row].setBounds (
                sendRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerFxParamLabelWidth));
            mixerSendLevelSliders[row].setBounds (sendRow);
            auto sendRouteRow = utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerSendRowHeight);
            mixerSendTaps[row].setBounds (
                sendRouteRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerFxParamLabelWidth));
            mixerSendDestinations[row].setBounds (sendRouteRow);
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        }

        // E15: the pager row (when paging) sits above the param rows; a choice-shaped row puts
        // its chooser where the slider would go.
        if (mixerFxParamPageChooser.isVisible())
        {
            mixerFxParamPageChooser.setBounds (
                utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxChooserHeight));
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        }
        else
        {
            mixerFxParamPageChooser.setBounds ({});
        }
        for (std::size_t index = 0; index < mixerFxParamSliders.size(); ++index)
        {
            if (! mixerFxParamLabels[index].isVisible())
            {
                mixerFxParamSliders[index].setBounds ({});
                mixerFxParamChoosers[index].setBounds ({});
                mixerFxParamLabels[index].setBounds ({});
                continue;
            }

            auto paramRow = utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxParamRowHeight);
            mixerFxParamLabels[index].setBounds (
                paramRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerFxParamLabelWidth));
            if (mixerFxParamChoosers[index].isVisible())
            {
                mixerFxParamChoosers[index].setBounds (paramRow);
                mixerFxParamSliders[index].setBounds ({});
            }
            else
            {
                mixerFxParamSliders[index].setBounds (paramRow);
                mixerFxParamChoosers[index].setBounds ({});
            }
            utility.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerFxSlotGap);
        }

        // E16: the control lane follows the selected strip's display ordinal (tracks, then buses).
        const int selectedStrip = appModel.selectedMixerStripOrdinal();
        auto lane = mixerStripBounds (selectedStrip > 0 ? selectedStrip : 0)
                        .reduced (yesdaw::ui::UiTheme::Layout::mixerControlLaneInsetX,
                                  yesdaw::ui::UiTheme::Layout::mixerControlLaneInsetY);
        mixerTrackSelect.setBounds (lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerTrackSelectHeight));
        lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerTrackSelectBottomGap);
        mixerPan.setBounds (lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerPanHeight)
                                .reduced (yesdaw::ui::UiTheme::Layout::mixerPanInsetX,
                                          yesdaw::ui::UiTheme::Layout::mixerPanInsetY));
        // N1: NOTHING live sits on the strip's Mute/Solo cells any more. Every strip — selected or
        // not — paints those cells and the click law drives them, so the strip you are working on
        // looks and behaves exactly like the others. The Solo/Mute VERBS keep a home in the
        // selected-target control lane (laid out with the other utility rows), where a labelled
        // button has room to be a labelled button.
        lane.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerButtonRowHeight
                            + yesdaw::ui::UiTheme::Layout::mixerButtonBottomGap);
        // M4: reserve exactly the painted insert block on the SELECTED strip, so the live fader
        // starts where the painted one does instead of sitting on top of the slot rows.
        lane.removeFromTop (juce::jmax (yesdaw::ui::UiTheme::Space::none,
                                        paintedFaderTopForLane (mixerStripBounds (selectedStrip > 0 ? selectedStrip : 0))
                                            - yesdaw::ui::UiTheme::Layout::mixerPaintedFaderTop));
        auto faderArea = lane.removeFromTop (
            juce::jmax (yesdaw::ui::UiTheme::Layout::mixerFaderMinHeight,
                        lane.getHeight() - yesdaw::ui::UiTheme::Layout::mixerFaderBottomReserve));
        mixerFader.setBounds (faderArea.withWidth (yesdaw::ui::UiTheme::Layout::mixerFaderWidth)
                                  .withCentre ({ faderArea.getCentreX(), faderArea.getCentreY() }));

        // E19/E25: the master fader lives on the PAINTED MASTER pane, inside its METER region —
        // the same walk drawMixer uses (content top, loudness card, gap, peak card, meter gap) —
        // so the fader rail can never cross the INTEGRATED / TRUE PEAK cards and the thumb
        // travels the same vertical span as the painted dB scale.
        auto masterContent = paintedMixerMasterBounds()
                                 .reduced (yesdaw::ui::UiTheme::Layout::mixerMasterContentInsetX,
                                           yesdaw::ui::UiTheme::Space::none);
        masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterContentTop
                                     + yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessCardHeight
                                     + yesdaw::ui::UiTheme::Layout::mixerMasterSectionGap
                                     + yesdaw::ui::UiTheme::Layout::mixerMasterPeakCardHeight
                                     + yesdaw::ui::UiTheme::Layout::mixerMasterMeterTopGap);
        auto masterFaderArea = masterContent.withTrimmedBottom (
            yesdaw::ui::UiTheme::Layout::mixerMasterMeterBottomInset);
        masterFaderArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerMasterScaleWidth);
        const int masterMeterPairWidth = 2 * yesdaw::ui::UiTheme::Layout::mixerMasterMeterWidth
                                       + yesdaw::ui::UiTheme::Layout::mixerMasterMeterGap;
        auto masterFaderColumn = masterFaderArea.withTrimmedRight (
            masterFaderArea.getWidth() / 2 + masterMeterPairWidth / 2);
        mixerMasterFader.setBounds (
            masterFaderColumn.withWidth (yesdaw::ui::UiTheme::Layout::mixerFaderWidth)
                .withCentre ({ masterFaderColumn.getCentreX(), masterFaderColumn.getCentreY() }));
    }

    void layoutAutomationLaneControls()
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const auto timeline = timelineBounds();
        automationLaneToggle.setBounds (L::automationLaneToggleBounds (timeline));
        // V8: the zoom cluster shares the automation toggle's toolbar row.
        timelineZoomOutButton.setBounds (L::timelineZoomOutButtonBounds (timeline));
        timelineZoomReadout.setBounds (L::timelineZoomReadoutBounds (timeline));
        timelineZoomInButton.setBounds (L::timelineZoomInButtonBounds (timeline));
        // G1.4 toolbar v2: [nudge] … status … [Inspector]; the nudge chooser drops whole when
        // the row cannot hold it next to the toggle.
        {
            juce::Rectangle<int> status = L::statusLineBounds (timeline);
            const juce::Rectangle<int> toggle = status.withX (timeline.getRight() - L::statusLineRightInset - L::inspectorToggleWidth)
                                                      .withWidth (L::inspectorToggleWidth);
            juce::Rectangle<int> nudge = status.withWidth (L::timelineNudgeChooserWidth);
            const bool nudgeFits = nudge.getRight() + L::timelineNudgeChooserGap + L::inspectorToggleGap <= toggle.getX();
            nudgeValueChooser.setBounds (nudgeFits ? nudge : juce::Rectangle<int>());
            inspectorToggle.setBounds (toggle);
            const int statusLeft = nudgeFits ? nudge.getRight() + L::timelineNudgeChooserGap : status.getX();
            const int statusRight = toggle.getX() - L::inspectorToggleGap;
            statusLine.setBounds (statusRight > statusLeft ? status.withLeft (statusLeft).withRight (statusRight)
                                                          : juce::Rectangle<int>());
        }

        // E26: the lane lives in the geometry law's reserved band — a header row (lane label
        // left, breakpoint controls right) above a FULL-WIDTH canvas, so the curve is never
        // hidden behind its own controls and never overlaps clip content.
        auto band = yesdaw::ui::timelineCanvasGeometry (timeline, makeTimelineState())
                        .automationLaneArea;
        auto header = band.removeFromTop (L::timelineCanvasAutomationHeaderHeight);
        automationTargetChooser.setBounds (
            header.removeFromRight (L::automationTargetChooserWidth));
        header.removeFromRight (L::timelineCanvasAutomationHeaderGap);
        automationModeChooser.setBounds (
            header.removeFromRight (L::automationModeChooserWidth));
        header.removeFromRight (L::timelineCanvasAutomationHeaderGap);
        automationBreakpointDeleteButton.setBounds (
            header.removeFromRight (L::automationBreakpointDeleteButtonWidth));
        header.removeFromRight (L::timelineCanvasAutomationHeaderGap);
        automationBreakpointAddButton.setBounds (
            header.removeFromRight (L::automationBreakpointAddButtonWidth));
        automationLaneRow.setBounds (header.withTrimmedLeft (L::timelineCanvasClipAreaInsetX));
        // N4: the band's X already equals clipArea's X (timelineCanvasGeometry carves it from
        // the target row, which lives inside clipArea) — no extra horizontal inset here, or the
        // canvas would drift off clipArea's span and breakpoints would stop lining up with clip
        // time positions.
        automationLaneCanvas.setBounds (
            band.withTrimmedBottom (L::timelineCanvasAutomationHeaderGap / 2));
    }

    void suspendDesktopAudioCallback()
    {
        ++audioSuspendRequests;   // G0.3 probe: every request counts, registered or not
        if (desktopAudioCallbackSuspendDepth++ != 0)
            return;

        resumeDesktopAudioAfterSuspend = desktopAudioCallbackRegistered;
        if (desktopAudioCallbackRegistered)
        {
            audioDeviceManager.removeAudioCallback (this);
            ++audioCallbackRemovals;   // G0.1 probe: B3 counts every one of these after startup
            desktopAudioCallbackRegistered = false;
            appModel.setDeviceCallbackLive (false);
        }
    }

    void resumeDesktopAudioCallback()
    {
        if (desktopAudioCallbackSuspendDepth <= 0 || --desktopAudioCallbackSuspendDepth != 0)
            return;

        if (resumeDesktopAudioAfterSuspend && audioDeviceManager.getCurrentAudioDevice() != nullptr)
        {
            audioDeviceManager.addAudioCallback (this);
            ++audioCallbackAdds;
            desktopAudioCallbackRegistered = true;
            appModel.setDeviceCallbackLive (true);
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

        // G1.1: the chord is looked up in the CURRENT Focus context (the editor that has focus),
        // then Global — never in another editor's bindings.
        const yesdaw::ui::UiActionId action = appModel.registry().keymap().actionForChord (
            chord, yesdaw::ui::focusContextForPanel (appModel.context().activePanel));
        if (action == yesdaw::ui::UiActionId::Count)
            return false;

        handleAction (action);
        refreshActionState();
        repaintAll();
        return true;
    }

    // G0.4 layered invalidation. repaintAll() is what every model/view change calls (the old
    // whole-window repaint()), and it also invalidates the buffered timeline canvas so the static
    // layer can never go stale. repaintDynamicLayers() is what the tick calls: the playhead layer,
    // the rail (meters), the dock (meters, master), and the header band (transport counter) — the
    // expensive clip/waveform canvas is left to its cache.
    void repaintAll()
    {
        ++fullInvalidations;
        timelineInput.repaint();
        repaint();
    }

    void repaintDynamicLayers()
    {
        ++dynamicInvalidations;
        playheadLayer.repaint();
        repaint (getLocalBounds().withHeight (headerHeightNow()));
        repaint (leftRailPanelBounds());
        if (appModel.context().mixerDockVisible
            || appModel.context().activePanel == yesdaw::ui::UiPanel::Mixer)
            repaint (mixerPanelBounds());
    }

    // G0.2 Command router (ADR-0046 §4). Only an active text field consumes keys: every other
    // widget declines keyboard focus, so a click on a button, combo, or slider hands focus to
    // THIS component (JUCE walks a click's focus grab up to the first ancestor that wants it)
    // and the next chord dispatches through keyPressed above. The KeyListener half below catches
    // the one remaining hole: right after launch (and whenever focus falls back to the window
    // itself) the DocumentWindow, not the shell, is the focused component — its key listeners
    // run before its own keyPressed, so the chord still reaches the router.
    void applyKeyboardFocusLaw()
    {
        applyKeyboardFocusLawTo (*this);
    }

    static void applyKeyboardFocusLawTo (juce::Component& parent)
    {
        for (int i = 0; i < parent.getNumChildComponents(); ++i)
        {
            juce::Component* child = parent.getChildComponent (i);
            if (child == nullptr)
                continue;
            if (dynamic_cast<juce::TextEditor*> (child) == nullptr)
                child->setWantsKeyboardFocus (false);
            applyKeyboardFocusLawTo (*child);
        }
    }

    void childrenChanged() override
    {
        applyKeyboardFocusLaw();
    }

    void parentHierarchyChanged() override
    {
        juce::Component* top = getTopLevelComponent();
        if (top == this)
            top = nullptr;
        if (top != routedTopLevel)
        {
            if (routedTopLevel != nullptr)
                routedTopLevel->removeKeyListener (this);
            routedTopLevel = top;
            if (routedTopLevel != nullptr)
                routedTopLevel->addKeyListener (this);
        }

        // Take focus once the window is showing so the very first chord after launch lands here.
        juce::Component::SafePointer<MainComponent> safeThis (this);
        juce::MessageManager::callAsync ([safeThis] {
            if (safeThis != nullptr && safeThis->isShowing() && ! safeThis->hasKeyboardFocus (true))
                safeThis->grabKeyboardFocus();
        });
    }

    // KeyListener half of the router: chords that reach the top-level window (focus on the
    // window itself, or on a child that did not consume them) dispatch exactly as if this
    // component were focused. A text editor that originated the event keeps its keys.
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override
    {
        if (originatingComponent == this
            || dynamic_cast<juce::TextEditor*> (originatingComponent) != nullptr)
            return false;
        return keyPressed (key);
    }

    bool keyStateChanged (bool, juce::Component*) override { return false; }
    // Declared alongside the KeyListener overload so neither hides the other (Clang's
    // -Woverloaded-virtual is an error here); the Component half keeps its default behaviour.
    bool keyStateChanged (bool isKeyDown) override { return juce::Component::keyStateChanged (isKeyDown); }

    [[nodiscard]] bool cancelInProgressEdit()
    {
        bool cancelled = timelineInput.cancelInProgressEdit();
        cancelled = pianoRollInput.cancelInProgressEdit() || cancelled;
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
        if (markerRenameEditor.isVisible())
        {
            dismissMarkerRenameEditor();
            cancelled = true;
        }

        if (cancelled)
        {
            refreshActionState();
            repaintAll();
        }
        return cancelled;
    }

    void handleAction (yesdaw::ui::UiActionId action)
    {
        // G0.1 probe: the last dispatched action by stable id, and the stamp the B1
        // action-to-paint budget is measured from (closed by the next completed paint).
        if (const auto* descriptor = appModel.registry().descriptor (action))
            lastActionStableId = descriptor->stableId;
        pendingActionStamp = std::chrono::steady_clock::now();
        actionStampPending = true;

        // G0.3 (ADR-0046 §6): no suspend/resume bracket — the device callback is never removed by
        // a UI action. Every branch below rides the atomic engine publish + retire law, the
        // transport command queue, or the live scalar lane. The only legitimate suspends left are
        // the device choosers (device (re)open).
        handleActionWhileAudioStopped (action);

        // G0.7 / G1.4: the settings row and the inspector change the layout — the whole shell
        // re-lays out.
        if (action == yesdaw::ui::UiActionId::ViewToggleSettingsRow
            || action == yesdaw::ui::UiActionId::ViewToggleInspector)
        {
            resized();
            repaintAll();
        }
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
        repaintAll();
    }

    // V8: the ONE place the toolbar readout learns the current factor — called from every path
    // that mutates timelineZoomFactor, so the visible number can never go stale against the
    // gestures (wheel, zoom tool, actions, and the fit verbs all funnel here or call it).
    void refreshTimelineZoomReadout()
    {
        timelineZoomReadout.setText (juce::String (timelineZoomFactor, 1) + "x",
                                     juce::dontSendNotification);
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
        refreshTimelineZoomReadout();
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
    // G1.2 (plan §3, Logic's order): File · Edit · Track · Clip · MIDI · View · Transport ·
    // Options · Help. Every item paints the chord that fires it in the CURRENT Focus context.
    juce::StringArray getMenuBarNames() override
    {
        return { "File", "Edit", "Track", "Clip", "MIDI", "View", "Transport", "Options", "Help" };
    }

    [[nodiscard]] static std::span<const yesdaw::ui::UiActionId> menuActionsForIndex (int topLevelMenuIndex)
    {
        using yesdaw::ui::UiActionId;
        static constexpr std::array<UiActionId, 8> kFileMenu {
            UiActionId::ProjectNew,        UiActionId::ProjectOpen,        UiActionId::ProjectSave,
            UiActionId::ProjectSaveAs,     UiActionId::ProjectImportAudio, UiActionId::ProjectExportAudio,
            UiActionId::ProjectExportDawproject, UiActionId::ProjectExportAudioCancel,
        };
        static constexpr std::array<UiActionId, 19> kEditMenu {
            UiActionId::EditUndo,          UiActionId::EditRedo,           UiActionId::TimelineClipCut,
            UiActionId::TimelineClipCopy,  UiActionId::TimelineClipPaste,  UiActionId::TimelineClipDuplicate,
            UiActionId::TimelineClipRepeatPaste, UiActionId::TimelineClipDelete,
            UiActionId::TimelineClipSelectAllProject, UiActionId::TimelineClipSelectAllTrack,
            UiActionId::EditRenameSelection,
            UiActionId::EditNudgeLeft,     UiActionId::EditNudgeRight,
            UiActionId::EditNudgeLeftFine, UiActionId::EditNudgeRightFine,
            UiActionId::EditNudgeValueGrid, UiActionId::EditNudgeValueBar,
            UiActionId::EditNudgeValueBeat, UiActionId::EditNudgeValueSixteenth,
        };
        static constexpr std::array<UiActionId, 12> kTrackMenu {
            UiActionId::TrackAdd,          UiActionId::TrackDuplicate,     UiActionId::TrackRemove,
            UiActionId::TrackRename,       UiActionId::TrackMoveUp,        UiActionId::TrackMoveDown,
            UiActionId::TrackSelectPrevious, UiActionId::TrackSelectNext,
            UiActionId::TrackToggleMute,   UiActionId::TrackToggleSolo,    UiActionId::TrackToggleArm,
            UiActionId::MixerTrackSetOutput,
        };
        static constexpr std::array<UiActionId, 12> kClipMenu {
            UiActionId::TimelineClipSplit, UiActionId::TimelineClipHeal,
            UiActionId::TimelineClipApplyDefaultFades, UiActionId::TimelineClipSetFades,
            UiActionId::TimelineClipCrossfade, UiActionId::TimelineClipSetGain,
            UiActionId::TimelineClipGainIncrease, UiActionId::TimelineClipGainDecrease,
            UiActionId::TimelineClipMove,  UiActionId::TimelineClipTrim,
            UiActionId::TimelineClipTimeStretch, UiActionId::TimelineMidiClipAdd,
        };
        static constexpr std::array<UiActionId, 10> kMidiMenu {
            UiActionId::PianoRollNoteAdd,  UiActionId::PianoRollNoteDelete, UiActionId::PianoRollNoteSelectAll,
            UiActionId::PianoRollNoteQuantizeSelection, UiActionId::PianoRollNoteTranspose,
            UiActionId::PianoRollNoteOctaveUp, UiActionId::PianoRollNoteOctaveDown,
            UiActionId::PianoRollNoteDuplicate, UiActionId::PianoRollNoteSetLength,
            UiActionId::PianoRollNoteSetVelocity,
        };
        static constexpr std::array<UiActionId, 19> kViewMenu {
            UiActionId::ViewTimeline,      UiActionId::ViewMixer,          UiActionId::ViewPianoRoll,
            UiActionId::ViewToggleInspector,
            UiActionId::TimelineToggleMixerDock, UiActionId::InspectorShowClipTab, UiActionId::InspectorShowTrackTab,
            UiActionId::TimelineAutomationToggleTrackLane,
            UiActionId::TimelineZoomIn,    UiActionId::TimelineZoomOut,
            UiActionId::TimelineZoomFitProject, UiActionId::TimelineZoomFitLoop,
            UiActionId::TimelineTogglePlayheadFollow,
            UiActionId::TimelineToolSelectPointer, UiActionId::TimelineToolSelectPencil,
            UiActionId::TimelineToolSelectScissors, UiActionId::TimelineToolSelectHand,
            UiActionId::TimelineToolSelectZoom, UiActionId::ViewToggleSettingsRow,
        };
        static constexpr std::array<UiActionId, 24> kTransportMenu {
            UiActionId::TransportTogglePlayStop, UiActionId::TransportPlay, UiActionId::TransportStop,
            UiActionId::TransportPlayFromLastLocate, UiActionId::TransportRecord,
            UiActionId::TransportReturnToZero, UiActionId::TransportLocateStart,
            UiActionId::TransportLocatePreviousBar, UiActionId::TransportLocateNextBar,
            UiActionId::TransportLocatePreviousGrid, UiActionId::TransportLocateNextGrid,
            UiActionId::TransportLocatePreviousMarker, UiActionId::TransportLocateNextMarker,
            UiActionId::TimelineMarkerAdd, UiActionId::TimelineMarkerRemove,
            UiActionId::TransportToggleLoop, UiActionId::TimelineRangeToLoop,
            UiActionId::TransportToggleMetronome, UiActionId::TransportToggleRecordCountIn,
            UiActionId::TransportToggleReturnToStartOnStop,
            UiActionId::TransportSetTempo, UiActionId::TransportSetMeter,
            UiActionId::TransportShuttleFaster, UiActionId::TransportShuttleSlower,
        };
        static constexpr std::array<UiActionId, 6> kOptionsMenu {
            UiActionId::TimelineSnapDisable,      UiActionId::TimelineSnapSetBar,
            UiActionId::TimelineSnapSetBeat,      UiActionId::TimelineSnapSetSixteenth,
            UiActionId::MixerTargetToggleSoloSafe,
            UiActionId::DeviceRefreshAudio,   // G0.8: Options ▸ Refresh Device (no toolbar button)
        };
        static constexpr std::array<UiActionId, 1> kHelpMenu { UiActionId::HelpShowKeymap };

        switch (topLevelMenuIndex)
        {
            case 0: return kFileMenu;
            case 1: return kEditMenu;
            case 2: return kTrackMenu;
            case 3: return kClipMenu;
            case 4: return kMidiMenu;
            case 5: return kViewMenu;
            case 6: return kTransportMenu;
            case 7: return kOptionsMenu;
            case 8: return kHelpMenu;
            default: return {};
        }
    }

    // G1.2: the tick a menu item shows — the registry context's own flags, one law for every
    // toggle and every "which one is current" group (views, inspector tabs, snap presets).
    [[nodiscard]] bool menuTickState (yesdaw::ui::UiActionId action) const noexcept
    {
        using yesdaw::ui::UiActionId;
        const yesdaw::ui::UiActionContext& c = appModel.context();
        switch (action)
        {
            case UiActionId::TransportToggleLoop:               return c.loopEnabled;
            case UiActionId::TransportToggleMetronome:          return c.metronomeEnabled;
            case UiActionId::TimelineTogglePlayheadFollow:      return c.playheadFollowEnabled;
            case UiActionId::TransportToggleReturnToStartOnStop: return c.returnToStartOnStopEnabled;
            case UiActionId::TransportToggleRecordCountIn:      return c.recordCountInEnabled;
            case UiActionId::ViewToggleSettingsRow:             return c.settingsRowVisible;
            case UiActionId::ViewToggleInspector:               return c.inspectorVisible;
            case UiActionId::EditNudgeValueGrid:                return c.nudgeValue == 0;
            case UiActionId::EditNudgeValueBar:                 return c.nudgeValue == 1;
            case UiActionId::EditNudgeValueBeat:                return c.nudgeValue == 2;
            case UiActionId::EditNudgeValueSixteenth:           return c.nudgeValue == 3;
            case UiActionId::TimelineToggleMixerDock:           return c.mixerDockVisible;
            case UiActionId::TimelineAutomationToggleTrackLane: return c.timelineAutomationTrackLaneVisible;
            case UiActionId::ViewTimeline:                      return c.activePanel == yesdaw::ui::UiPanel::Timeline;
            case UiActionId::ViewMixer:                         return c.activePanel == yesdaw::ui::UiPanel::Mixer;
            case UiActionId::ViewPianoRoll:                     return c.activePanel == yesdaw::ui::UiPanel::PianoRoll;
            case UiActionId::InspectorShowClipTab:              return ! c.inspectorTrackTabActive;
            case UiActionId::InspectorShowTrackTab:             return c.inspectorTrackTabActive;
            case UiActionId::TimelineSnapDisable:               return ! c.snapEnabled;
            case UiActionId::TimelineSnapSetBar:                return c.snapEnabled && c.snapGridTicks == 2048;
            case UiActionId::TimelineSnapSetBeat:               return c.snapEnabled && c.snapGridTicks == 512;
            case UiActionId::TimelineSnapSetSixteenth:          return c.snapEnabled && c.snapGridTicks == 128;
            case UiActionId::TimelineToolSelectPointer:         return c.activeTimelineTool == yesdaw::ui::TimelineTool::Pointer;
            case UiActionId::TimelineToolSelectPencil:          return c.activeTimelineTool == yesdaw::ui::TimelineTool::Pencil;
            case UiActionId::TimelineToolSelectScissors:        return c.activeTimelineTool == yesdaw::ui::TimelineTool::Scissors;
            case UiActionId::TimelineToolSelectHand:            return c.activeTimelineTool == yesdaw::ui::TimelineTool::Hand;
            case UiActionId::TimelineToolSelectZoom:            return c.activeTimelineTool == yesdaw::ui::TimelineTool::Zoom;
            default:                                            return false;
        }
    }

    // G1.3: the context menu for a target — the same registry-driven item law the menu bar
    // uses (label, enabled, tick, chord for the focus context). Recorded for the harness, and
    // shown only when the shell is on a real desktop (headless gates read the record).
    struct LastContextMenu
    {
        bool shown = false;
        yesdaw::ui::ContextMenuTarget target = yesdaw::ui::ContextMenuTarget::Clip;
        int index = -1;
        std::vector<yesdaw::ui::UiActionId> actions;
    };
    LastContextMenu lastContextMenu;

    void openContextMenu (yesdaw::ui::ContextMenuTarget target, int index, juce::Component& source,
                          juce::Point<int> sourcePosition)
    {
        lastContextMenu = {};
        lastContextMenu.shown = true;
        lastContextMenu.target = target;
        lastContextMenu.index = index;
        // The click's selection is part of the context the items are built from.
        refreshActionState();
        juce::PopupMenu menu;
        for (const yesdaw::ui::ContextMenuEntry& entry : yesdaw::ui::contextMenuEntries (target))
        {
            if (entry.separatorBefore)
                menu.addSeparator();
            const auto& descriptor = yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (entry.action)];
            const bool slotVerb = target == yesdaw::ui::ContextMenuTarget::InsertSlot;
            const bool slotEnabled = slotVerb && index >= 0
                                  && static_cast<std::size_t> (index) < appModel.selectedStripFxChain().size();
            if (slotVerb && entry.action == yesdaw::ui::UiActionId::MixerFxInsertReorder)
            {
                // §3.3: Move Up · Move Down — one reorder verb, two directions.
                juce::PopupMenu::Item up ("Move Up");
                up.itemID = kContextMenuMoveUpId;
                up.isEnabled = slotEnabled && index > 0;
                menu.addItem (std::move (up));
                juce::PopupMenu::Item down ("Move Down");
                down.itemID = kContextMenuMoveDownId;
                down.isEnabled = slotEnabled && static_cast<std::size_t> (index + 1) < appModel.selectedStripFxChain().size();
                menu.addItem (std::move (down));
                lastContextMenu.actions.push_back (entry.action);
                continue;
            }
            juce::PopupMenu::Item item (slotVerb && entry.action == yesdaw::ui::UiActionId::MixerFxInsertParamSet
                                            ? juce::String ("Open Editor")
                                            : juce::String (descriptor.label));
            item.itemID = static_cast<int> (entry.action) + 1;
            item.isEnabled = slotVerb ? slotEnabled
                                      : appModel.registry().stateFor (entry.action, appModel.context()).enabled;
            item.isTicked = slotVerb && entry.action == yesdaw::ui::UiActionId::MixerFxInsertToggle && slotEnabled
                                ? ! appModel.selectedStripFxChain()[static_cast<std::size_t> (index)].enabled
                                : menuTickState (entry.action);
            item.shortcutKeyDescription = slotVerb ? juce::String() : menuShortcutFor (entry.action);
            menu.addItem (std::move (item));
            lastContextMenu.actions.push_back (entry.action);
        }
        repaintAll();
        if (getPeer() == nullptr)
            return;   // headless: the record is the menu
        const juce::Point<int> screenPoint = source.localPointToGlobal (sourcePosition);
        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetScreenArea (juce::Rectangle<int> (screenPoint.x, screenPoint.y, 1, 1))
                                .withParentComponent (nullptr),
                            [this] (int itemId) { invokeContextMenuItem (itemId); });
    }

    static constexpr int kContextMenuMoveUpId = 2001;
    static constexpr int kContextMenuMoveDownId = 2002;

    // The one path a picked context-menu item takes (the popup's callback and the harness): the
    // insert-slot verbs act on the clicked slot through the shell's per-slot handlers; every
    // other target dispatches the action.
    void invokeContextMenuItem (int itemId)
    {
        if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::InsertSlot)
        {
            const int slot = lastContextMenu.index;
            if (slot < 0)
                return;
            const auto slotIndex = static_cast<std::size_t> (slot);
            if (itemId == kContextMenuMoveUpId)
                (void) appModel.moveFxInsertOnSelectedStrip (slotIndex, -1);
            else if (itemId == kContextMenuMoveDownId)
                (void) appModel.moveFxInsertOnSelectedStrip (slotIndex, 1);
            else if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerFxInsertToggle) + 1)
                (void) appModel.toggleFxInsertEnabledOnSelectedStrip (slotIndex);
            else if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerFxInsertRemove) + 1)
                (void) appModel.removeFxInsertFromSelectedStrip (slotIndex);
            else if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerFxInsertParamSet) + 1)
            {
                selectedFxParamSlot = selectedFxParamSlot == slot ? -1 : slot;
                selectedFxParamPage = 0;
            }
            else
                return;
            refreshActionState();
            resized();
            repaintAll();
            return;
        }
        if (itemId <= 0 || itemId > static_cast<int> (yesdaw::ui::kUiActionCount))
            return;
        handleAction (static_cast<yesdaw::ui::UiActionId> (itemId - 1));
        refreshActionState();
        resized();
        repaintAll();
    }

public:
    void harnessInvokeContextMenuItem (yesdaw::ui::UiActionId action, int direction)
    {
        if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::InsertSlot
            && action == yesdaw::ui::UiActionId::MixerFxInsertReorder)
            invokeContextMenuItem (direction < 0 ? kContextMenuMoveUpId : kContextMenuMoveDownId);
        else
            invokeContextMenuItem (static_cast<int> (action) + 1);
    }
private:

public:
    // Harness: route a shell point to the input surface under it and run its right-click law.
    [[nodiscard]] yesdaw::ui::MainComponentContextMenu harnessRequestContextMenu (juce::Point<int> shellPoint)
    {
        lastContextMenu = {};
        if (timelineInput.isVisible() && timelineInput.getBounds().contains (shellPoint))
            timelineInput.requestContextMenu (shellPoint - timelineInput.getPosition());
        else if (pianoRollInput.isVisible() && pianoRollInput.getBounds().contains (shellPoint))
            pianoRollInput.requestContextMenu (shellPoint - pianoRollInput.getPosition());
        else if (trackListInput.isVisible() && trackListInput.getBounds().contains (shellPoint))
            trackListInput.requestContextMenu (shellPoint - trackListInput.getPosition());
        else if (mixerStripsInput.isVisible() && mixerStripsInput.getBounds().contains (shellPoint))
            mixerStripsInput.requestContextMenu (shellPoint - mixerStripsInput.getPosition());
        yesdaw::ui::MainComponentContextMenu out;
        out.shown = lastContextMenu.shown;
        out.target = lastContextMenu.target;
        out.index = lastContextMenu.index;
        out.actions = lastContextMenu.actions;
        return out;
    }

private:
    // G1.2: the chord a menu item paints — the one that fires in the CURRENT Focus context
    // (its own binding or a Global one); another editor's binding paints nothing.
    [[nodiscard]] juce::String menuShortcutFor (yesdaw::ui::UiActionId action) const
    {
        const std::string& chord = appModel.registry().keymap().chordFor (action);
        if (chord.empty())
            return {};
        const yesdaw::ui::UiFocusContext context = yesdaw::ui::defaultFocusContext (action);
        const yesdaw::ui::UiFocusContext focus = yesdaw::ui::focusContextForPanel (appModel.context().activePanel);
        if (context != yesdaw::ui::UiFocusContext::Global && context != focus)
            return {};
        return juce::String (chord);
    }

    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String&) override
    {
        juce::PopupMenu menu;
        for (const yesdaw::ui::UiActionId action : menuActionsForIndex (topLevelMenuIndex))
        {
            const auto& descriptor =
                yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (action)];
            juce::PopupMenu::Item item (descriptor.label);
            item.itemID = static_cast<int> (action) + 1;
            item.isEnabled = appModel.registry().stateFor (action, appModel.context()).enabled;
            item.isTicked = menuTickState (action);
            item.shortcutKeyDescription = menuShortcutFor (action);
            menu.addItem (std::move (item));
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
            repaintAll();
            return;
        }

        if (menuItemID <= 0 || menuItemID > static_cast<int> (yesdaw::ui::kUiActionCount))
            return;

        handleAction (static_cast<yesdaw::ui::UiActionId> (menuItemID - 1));
        refreshActionState();
        repaintAll();
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

    // E29: input-side twins of the output plumbing. Switching the input device restarts the
    // JUCE device, which re-runs audioDeviceAboutToStart and re-adopts the E28 profile.
    [[nodiscard]] std::vector<std::string> enumerateAudioInputDeviceNames()
    {
        if (fileChoices.listAudioInputDevices)
            return fileChoices.listAudioInputDevices();

        std::vector<std::string> names;
        if (! desktopAudioRequested)
            return names;

        for (juce::AudioIODeviceType* type : audioDeviceManager.getAvailableDeviceTypes())
        {
            if (type == nullptr)
                continue;

            type->scanForDevices();
            for (const juce::String& name : type->getDeviceNames (true))
                names.push_back (name.toStdString());
        }
        return names;
    }

    [[nodiscard]] bool selectAudioInputDeviceByName (const std::string& name)
    {
        if (fileChoices.selectAudioInputDevice)
            return fileChoices.selectAudioInputDevice (name);

        if (! desktopAudioRequested)
            return false;

        juce::AudioDeviceManager::AudioDeviceSetup setup = audioDeviceManager.getAudioDeviceSetup();
        setup.inputDeviceName = juce::String (name);
        setup.useDefaultInputChannels = true;
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

        // E29: rebuild the input device list the same way...
        audioInputDeviceChooserNames = enumerateAudioInputDeviceNames();
        audioInputDeviceChooser.clear (juce::dontSendNotification);
        juce::String currentInput;
        if (desktopAudioRequested)
            currentInput = audioDeviceManager.getAudioDeviceSetup().inputDeviceName;
        for (std::size_t index = 0; index < audioInputDeviceChooserNames.size(); ++index)
        {
            audioInputDeviceChooser.addItem (juce::String (audioInputDeviceChooserNames[index]),
                                             static_cast<int> (index) + 1);
            if (currentInput.isNotEmpty()
                && currentInput == juce::String (audioInputDeviceChooserNames[index]))
                audioInputDeviceChooser.setSelectedId (static_cast<int> (index) + 1,
                                                       juce::dontSendNotification);
        }
        audioInputDeviceChooser.setEnabled (! audioInputDeviceChooserNames.empty());

        // ...and the channel pick from the ADOPTED device's real input count: mono "In N" for
        // each channel, "In N+M" for each adjacent stereo pair.
        refreshRecordingInputChannelChooser();
        refreshingAudioDeviceChooser = false;
    }

    // E29: options track the adopted profile's generation so a device change re-lists them.
    void refreshRecordingInputChannelChooser()
    {
        const auto& device = appModel.recordingDeviceSelection();
        recordingInputChannelChooser.clear (juce::dontSendNotification);
        for (int channel = 0; channel < static_cast<int> (device.inputChannels); ++channel)
            recordingInputChannelChooser.addItem ("In " + juce::String (channel + 1), channel + 1);
        for (int channel = 0; channel + 1 < static_cast<int> (device.inputChannels); ++channel)
            recordingInputChannelChooser.addItem (
                "In " + juce::String (channel + 1) + "+" + juce::String (channel + 2),
                1001 + channel);
        const auto& context = appModel.context();
        if (device.inputChannels > 0u)
        {
            const int pickBase = std::max (0, context.selectedRecordingInputChannel);
            recordingInputChannelChooser.setSelectedId (
                context.selectedRecordingInputStereoPair ? 1001 + pickBase : pickBase + 1,
                juce::dontSendNotification);
        }
        recordingInputChannelChooser.setEnabled (device.selected && device.inputChannels > 0u);
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
                        // R4: a failed create paints its reason instead of vanishing.
                        const yesdaw::persistence::BundleResult created =
                            fileChoices.makeNewProject
                                ? appModel.createProjectBundle (path, fileChoices.makeNewProject())
                                : appModel.createProjectBundle (path);
                        if (! created.ok())
                            appModel.reportStatus ("New project failed: " + created.message, true);
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
                            // R7: verb failures report their precise reason inside the model.
                            if (selectedTrackLane >= 0
                                && selectedTrackLane < static_cast<int> (tracks.size()))
                                (void) appModel.importAudioFileToTrack (
                                    path, std::move (*decoded),
                                    tracks[static_cast<std::size_t> (selectedTrackLane)].id);
                            else
                                (void) appModel.importAudioFile (path, std::move (*decoded));
                        }
                        else
                        {
                            // R6: a file the WAV reader refuses is named, never swallowed.
                            appModel.reportStatus (
                                "Import refused (WAV only, stereo max): "
                                    + path.filename().string(),
                                true);
                        }
                    }
                }
                return;

            case yesdaw::ui::UiActionId::ProjectSaveAs:
                if (fileChoices.chooseSaveAsProjectBundle)
                {
                    const std::filesystem::path path = fileChoices.chooseSaveAsProjectBundle();
                    if (! path.empty())
                    {
                        // R4: a failed Save As paints its refusal reason instead of vanishing.
                        const yesdaw::ui::UiActionDispatchResult savedAs =
                            appModel.saveProjectBundleAs (path);
                        if (! savedAs.dispatched)
                            appModel.reportStatus (
                                savedAs.state.disabledReason[0] != '\0'
                                    ? std::string ("Save As failed: ") + savedAs.state.disabledReason
                                    : std::string ("Save As failed"),
                                true);
                    }
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
                    refreshTimelineZoomReadout();
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
                        refreshTimelineZoomReadout();
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
        ++actionStateRefreshes;   // G0.4 probe: how often the 391-line refresh actually runs
        rebuildTimelineClipViews();
        // E29: a device change (adoption, Test Device, refresh) re-lists the channel pick.
        if (recordingChannelChooserGeneration != appModel.context().recordingDeviceGeneration)
        {
            recordingChannelChooserGeneration = appModel.context().recordingDeviceGeneration;
            refreshingAudioDeviceChooser = true;
            refreshRecordingInputChannelChooser();
            refreshingAudioDeviceChooser = false;
        }
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto action = toolbarActions[i];
            // G0.7: the device + recording cluster lives in the collapsible settings row; every
            // other toolbar button is visible in every view.
            buttons[i].setVisible (! isSettingsRowAction (action) || appModel.context().settingsRowVisible);
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
        playheadLayer.setVisible (appModel.context().activePanel == yesdaw::ui::UiPanel::Timeline);
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
                mixerFxSlotUps[slot].setVisible (present);
                mixerFxSlotDowns[slot].setVisible (present);
                if (! present)
                    continue;

                mixerFxSlotUps[slot].setEnabled (slot > 0);
                mixerFxSlotDowns[slot].setEnabled (slot + 1 < chain.size());

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
            std::size_t pageCount = 0;
            if (selectedFxParamSlot >= 0)
            {
                const yesdaw::engine::FxKind kind =
                    chain[static_cast<std::size_t> (selectedFxParamSlot)].kind;
                const bool paramEditEnabled =
                    appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerFxInsertParamSet,
                                                  appModel.context()).enabled;

                // E15: collect EVERY accepted param id, then show the selected page of rows.
                std::vector<std::uint32_t> acceptedIds;
                for (std::uint32_t paramId = 0;
                     paramId < yesdaw::ui::UiTheme::Layout::mixerFxParamProbeLimit;
                     ++paramId)
                {
                    if (yesdaw::engine::fxKindAcceptsParameterId (kind, paramId))
                        acceptedIds.push_back (paramId);
                }
                pageCount = (acceptedIds.size() + mixerFxParamSliders.size() - 1)
                          / std::max<std::size_t> (1, mixerFxParamSliders.size());
                if (selectedFxParamPage < 0
                    || static_cast<std::size_t> (selectedFxParamPage) >= pageCount)
                    selectedFxParamPage = 0;

                mixerFxParamPageChooser.clear (juce::dontSendNotification);
                for (std::size_t page = 0; page < pageCount; ++page)
                {
                    const std::size_t firstParam = page * mixerFxParamSliders.size();
                    const std::size_t lastParam = std::min (firstParam + mixerFxParamSliders.size(),
                                                            acceptedIds.size());
                    mixerFxParamPageChooser.addItem (
                        "Params " + juce::String (static_cast<int> (firstParam) + 1)
                            + "-" + juce::String (static_cast<int> (lastParam)),
                        static_cast<int> (page) + 1);
                }
                mixerFxParamPageChooser.setSelectedId (selectedFxParamPage + 1, juce::dontSendNotification);
                mixerFxParamPageChooser.setEnabled (paramEditEnabled);

                const std::size_t firstShown =
                    static_cast<std::size_t> (selectedFxParamPage) * mixerFxParamSliders.size();
                for (std::size_t i = firstShown;
                     i < acceptedIds.size() && used < mixerFxParamSliders.size();
                     ++i)
                {
                    const std::uint32_t paramId = acceptedIds[i];
                    const yesdaw::engine::ParamSpec spec = yesdaw::engine::fxParamSpecForKind (kind, paramId);
                    const double normalized = appModel.fxInsertParamValueOnSelectedStrip (
                        static_cast<std::size_t> (selectedFxParamSlot), paramId);
                    mixerFxParamSliderIds[used] = paramId;
                    if (spec.choiceCount >= 2 && spec.choiceNames != nullptr)
                    {
                        // E15: choice-shaped param — a real chooser replaces the raw slider.
                        auto& choiceChooser = mixerFxParamChoosers[used];
                        choiceChooser.clear (juce::dontSendNotification);
                        for (int choice = 0; choice < static_cast<int> (spec.choiceCount); ++choice)
                            choiceChooser.addItem (spec.choiceNames[choice], choice + 1);
                        const double real = yesdaw::engine::mapNormalized (spec, normalized);
                        const double step = (spec.max - spec.min)
                                          / static_cast<double> (spec.choiceCount - 1);
                        const int currentChoice = juce::jlimit (
                            0, static_cast<int> (spec.choiceCount) - 1,
                            static_cast<int> (std::llround ((real - spec.min) / step)));
                        choiceChooser.setSelectedId (currentChoice + 1, juce::dontSendNotification);
                        choiceChooser.setEnabled (paramEditEnabled);
                        choiceChooser.setVisible (true);
                        mixerFxParamSliders[used].setVisible (false);
                        mixerFxParamLabels[used].setText (
                            juce::String (spec.name) + " " + spec.choiceNames[currentChoice],
                            juce::dontSendNotification);
                    }
                    else
                    {
                        // Alt+click resets the bound parameter to its ParamSpec default.
                        mixerFxParamSliders[used].setDoubleClickReturnValue (
                            true, yesdaw::engine::normalizedDefault (spec));
                        mixerFxParamSliders[used].setValue (normalized, juce::dontSendNotification);
                        mixerFxParamSliders[used].setEnabled (paramEditEnabled);
                        mixerFxParamSliders[used].setVisible (true);
                        mixerFxParamChoosers[used].setVisible (false);
                        mixerFxParamLabels[used].setText (
                            juce::String (spec.name)
                                + " " + juce::String (yesdaw::engine::mapNormalized (spec, normalized), 1)
                                + spec.unit,
                            juce::dontSendNotification);
                    }
                    mixerFxParamLabels[used].setVisible (true);
                    ++used;
                }
            }
            for (std::size_t index = used; index < mixerFxParamSliders.size(); ++index)
            {
                mixerFxParamSliders[index].setVisible (false);
                mixerFxParamChoosers[index].setVisible (false);
                mixerFxParamLabels[index].setVisible (false);
            }
            const bool pagerVisible = pageCount > 1;
            mixerFxParamPageChooser.setVisible (pagerVisible);
            refreshingFxParamControls = false;
            if (used != lastVisibleFxParamRows || pagerVisible != lastFxParamPagerVisible)
            {
                lastVisibleFxParamRows = used;
                lastFxParamPagerVisible = pagerVisible;
                resized();
            }
        }
        {
            refreshingSendControls = true;
            const auto& project = appModel.project();
            mixerBusAddButton.setEnabled (
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerBusAdd, appModel.context()).enabled);
            // E17: bus removal needs a selected BUS (the ordinal past the tracks says which).
            mixerBusRemoveButton.setEnabled (
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerBusRemove, appModel.context()).enabled
                && appModel.selectedMixerStripOrdinal()
                       >= static_cast<int> (appModel.project().tracks.size())
                && appModel.selectedMixerStripOrdinal() >= 0);

            // R13: sends and outputs originate on TRACK and BUS strips (Master owns neither).
            // A bus strip's choosers exclude the bus itself — a self-route is a cycle the verb
            // would refuse anyway; the UI simply does not offer it. Item ids stay busIndex-keyed
            // so the dispatch mapping is untouched by the exclusion.
            const yesdaw::engine::EntityId sendOwnerId = appModel.selectedSendOwnerEntityId();
            mixerSendAddChooser.clear (juce::dontSendNotification);
            std::size_t sendTargetCount = 0;
            for (std::size_t busIndex = 0; busIndex < project.buses.size(); ++busIndex)
            {
                if (project.buses[busIndex].id == sendOwnerId)
                    continue;
                mixerSendAddChooser.addItem (juce::String (project.buses[busIndex].strip.name),
                                             static_cast<int> (busIndex) + 1);
                ++sendTargetCount;
            }
            const bool sendAddEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerSendAdd, appModel.context()).enabled
                && sendTargetCount > 0
                && sendOwnerId.isValid();
            mixerSendAddChooser.setEnabled (sendAddEnabled);

            // M3: Master first, then every bus; the selection mirrors the strip's persisted route.
            mixerTrackOutputChooser.clear (juce::dontSendNotification);
            mixerTrackOutputChooser.addItem ("Out: Master", 1);
            for (std::size_t busIndex = 0; busIndex < project.buses.size(); ++busIndex)
            {
                if (project.buses[busIndex].id == sendOwnerId)
                    continue;
                mixerTrackOutputChooser.addItem ("Out: " + juce::String (project.buses[busIndex].strip.name),
                                                 static_cast<int> (busIndex) + 2);
            }

            const yesdaw::engine::EntityId routedBusId = appModel.selectedTrackOutputBusId();
            int routedItemId = 1;
            for (std::size_t busIndex = 0; busIndex < project.buses.size(); ++busIndex)
                if (project.buses[busIndex].id == routedBusId)
                    routedItemId = static_cast<int> (busIndex) + 2;
            mixerTrackOutputChooser.setSelectedId (routedItemId, juce::dontSendNotification);
            mixerTrackOutputChooser.setEnabled (
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTrackSetOutput,
                                              appModel.context()).enabled
                && sendOwnerId.isValid());

            const std::vector<yesdaw::engine::SendRow> sends = appModel.selectedTrackSends();
            const bool sendEditEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerSendSetLevel,
                                              appModel.context()).enabled;
            const bool sendTapEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerSendSetTap,
                                              appModel.context()).enabled;
            const bool sendDestinationEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerSendSetDestination,
                                              appModel.context()).enabled;
            for (std::size_t row = 0; row < mixerSendLevelSliders.size(); ++row)
            {
                const bool present = row < sends.size();
                mixerSendLevelSliders[row].setVisible (present);
                mixerSendLabels[row].setVisible (present);
                mixerSendRemoves[row].setVisible (present);
                mixerSendTaps[row].setVisible (present);
                mixerSendDestinations[row].setVisible (present);
                if (! present)
                    continue;

                juce::String busName ("Bus?");
                for (const auto& bus : project.buses)
                    if (bus.id == sends[row].busId)
                        busName = juce::String (bus.strip.name);
                mixerSendLabels[row].setText (busName, juce::dontSendNotification);
                mixerSendLevelSliders[row].setValue (sends[row].linearGain, juce::dontSendNotification);
                mixerSendLevelSliders[row].setEnabled (sendEditEnabled);
                // E18: the tap toggle names the CURRENT tap; the chooser lists every bus with
                // the current destination selected.
                mixerSendTaps[row].setButtonText (
                    sends[row].tap == yesdaw::engine::SendTap::PreFader ? "Pre" : "Post");
                mixerSendTaps[row].setEnabled (sendTapEnabled);
                mixerSendDestinations[row].clear (juce::dontSendNotification);
                int currentBusId = 0;
                for (std::size_t busIndex = 0; busIndex < project.buses.size(); ++busIndex)
                {
                    if (project.buses[busIndex].id == sendOwnerId)   // R13: never offer a self-route
                        continue;
                    mixerSendDestinations[row].addItem (juce::String (project.buses[busIndex].strip.name),
                                                        static_cast<int> (busIndex) + 1);
                    if (project.buses[busIndex].id == sends[row].busId)
                        currentBusId = static_cast<int> (busIndex) + 1;
                }
                mixerSendDestinations[row].setSelectedId (currentBusId, juce::dontSendNotification);
                mixerSendDestinations[row].setEnabled (sendDestinationEnabled && project.buses.size() > 1);
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
            refreshingNudgeChooser = true;
            nudgeValueChooser.setSelectedId (appModel.context().nudgeValue + 1, juce::dontSendNotification);
            refreshingNudgeChooser = false;
            inspectorToggle.setToggleState (appModel.context().inspectorVisible, juce::dontSendNotification);
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
                                   && appModel.context().timelineClipSelected
                                   && ! appModel.context().inspectorTrackTabActive;
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
        mixerDockToggle.setToggleState (appModel.context().mixerDockVisible, juce::dontSendNotification);
        // No effect in the full-view Mixer panel (it never reserves dock space to begin with).
        mixerDockToggle.setVisible (appModel.context().activePanel != yesdaw::ui::UiPanel::Mixer);
        // V7: the tab buttons live wherever the inspector panel does; the active tab lights.
        const bool inspectorPanelVisible = appModel.context().activePanel != yesdaw::ui::UiPanel::Mixer;
        inspectorClipTab.setVisible (inspectorPanelVisible);
        inspectorTrackTab.setVisible (inspectorPanelVisible);
        inspectorClipTab.setToggleState (! appModel.context().inspectorTrackTabActive,
                                         juce::dontSendNotification);
        inspectorTrackTab.setToggleState (appModel.context().inspectorTrackTabActive,
                                          juce::dontSendNotification);
    }

    void refreshAutomationLaneControls()
    {
        constexpr yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::TimelineAutomationToggleTrackLane;
        const auto state = appModel.registry().stateFor (action, appModel.context());
        const bool timelineVisible = appModel.context().activePanel == yesdaw::ui::UiPanel::Timeline;
        automationLaneToggle.setVisible (timelineVisible);
        automationLaneToggle.setEnabled (state.enabled);
        // V8: the zoom cluster lives and dies with the same toolbar row; the readout re-reads
        // the ONE shared zoom factor every gesture mutates.
        timelineZoomOutButton.setVisible (timelineVisible);
        timelineZoomInButton.setVisible (timelineVisible);
        timelineZoomReadout.setVisible (timelineVisible);
        statusLine.setVisible (timelineVisible);
        refreshTimelineZoomReadout();
        automationLaneToggle.setToggleState (appModel.context().timelineAutomationTrackLaneVisible,
                                             juce::dontSendNotification);
        const bool laneVisible = timelineVisible && appModel.context().timelineAutomationTrackLaneVisible;

        // E20/N4: rebuild the lane-target list for the automation-target track FIRST — the
        // header text and the add/delete button enablement below both read currentAutomationTarget(),
        // so they must see this frame's target, not the previous frame's stale options.
        refreshingAutomationTarget = true;
        automationTargetOptions = buildAutomationTargetOptions();
        if (selectedAutomationTargetIndex < 0
            || selectedAutomationTargetIndex >= static_cast<int> (automationTargetOptions.size()))
            selectedAutomationTargetIndex = 0;
        automationTargetChooser.clear (juce::dontSendNotification);
        for (std::size_t option = 0; option < automationTargetOptions.size(); ++option)
            automationTargetChooser.addItem (automationTargetOptions[option].label,
                                             static_cast<int> (option) + 1);
        if (! automationTargetOptions.empty())
            automationTargetChooser.setSelectedId (selectedAutomationTargetIndex + 1,
                                                   juce::dontSendNotification);
        automationTargetChooser.setVisible (laneVisible);
        automationTargetChooser.setEnabled (laneVisible && ! automationTargetOptions.empty());

        // N5: the mode chooser reflects the persisted project.automationMode.
        automationModeChooser.setSelectedId (
            static_cast<int> (appModel.project().automationMode) + 1, juce::dontSendNotification);
        automationModeChooser.setVisible (laneVisible);
        automationModeChooser.setEnabled (laneVisible && appModel.context().projectLoaded);
        refreshingAutomationTarget = false;

        automationLaneRow.setText (automationLaneRowText(), juce::dontSendNotification);
        automationLaneRow.setVisible (laneVisible);
        automationLaneCanvas.setVisible (laneVisible);
        // N4: the lane's Y position (which track row it sits under) is part of the geometry law
        // now, not just its visibility — re-lay out on every refresh while visible so a track or
        // target switch (or a track-row scroll) moves it immediately, not just on the open/close
        // transition.
        if (laneVisible)
            layoutAutomationLaneControls();
        automationLaneLaidOutVisible = laneVisible;
        if (laneVisible)
            automationLaneCanvas.repaint();

        // N4: enablement and the click handlers below all key on the SAME selected target the
        // canvas already edits — never the first track's fader regardless of what is chosen.
        const AutomationTargetOption target = currentAutomationTarget();
        const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
            ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
            : nullptr;

        const auto addState = appModel.registry().stateFor (
            yesdaw::ui::UiActionId::TimelineAutomationAddBreakpoint,
            appModel.context());
        automationBreakpointAddButton.setVisible (laneVisible);
        automationBreakpointAddButton.setEnabled (laneVisible
                                                  && addState.enabled
                                                  && target.ownerEntity.isValid());

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

        // N4: name the REAL owner and the REAL target — automationTargetTrackId() is the same
        // track the target chooser and the canvas already edit, and currentAutomationTarget()
        // carries that target's own label ("Fader", "Pan", "Send: X", "FX1 ..."), never a
        // hardcoded "Track fader" regardless of what is actually selected.
        const yesdaw::engine::EntityId trackId = automationTargetTrackId();
        const yesdaw::engine::Track* track = nullptr;
        for (const yesdaw::engine::Track& candidate : project.tracks)
        {
            if (candidate.id == trackId)
            {
                track = &candidate;
                break;
            }
        }
        if (track == nullptr)
            return "No Track automation";

        const AutomationTargetOption target = currentAutomationTarget();
        const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
            ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
            : nullptr;

        const juce::String trackName = track->strip.name.empty() ? "Track 1" : juce::String (track->strip.name);
        const int breakpointCount = lane == nullptr ? 0 : static_cast<int> (lane->points.size());
        // R14: a bus-owned target's label already names the bus — the track prefix would lie.
        if (target.busOwned)
            return target.label + " - " + juce::String (breakpointCount) + " breakpoints";
        return trackName + " - " + target.label + " - " + juce::String (breakpointCount) + " breakpoints";
    }

    // N5: normalized [0,1] breakpoint value for a live linear-gain fader read, matching
    // FaderNode::linearGainForNormalizedEvent's dB-range mapping exactly (its inverse) — so a
    // point recorded here plays back at the SAME gain the fader was actually at.
    [[nodiscard]] static double automationNormalizedForFaderGain (double linearGain) noexcept
    {
        const double gainDb = linearGain > 0.0
            ? 20.0 * std::log10 (linearGain)
            : yesdaw::engine::FaderNode::kMinGainDb;
        return yesdaw::engine::unmapToNormalized (
            yesdaw::engine::FaderNode::parameterSpec (yesdaw::engine::FaderNode::kGainParameterId),
            gainDb);
    }

    // N5: normalized [0,1] breakpoint value for a live pan read, the exact inverse of
    // PanNode::panForNormalizedEvent (-1..1 maps linearly to 0..1).
    [[nodiscard]] static double automationNormalizedForPan (double pan) noexcept
    {
        return std::clamp ((pan + 1.0) / 2.0, 0.0, 1.0);
    }

    // N5: arm a Touch/Latch ride if the mode is armed AND the transport was already rolling when
    // the drag started — moving a control while stopped, even in Touch/Latch mode, is just a
    // normal edit (matches real-DAW semantics: Touch/Latch only writes DURING playback).
    // R15: the ride owner is the selected TRACK or BUS strip (a bus strip's fader/pan ride
    // writes the Bus roles), or an explicit owner (an FX insert's id for param rides); ONLY
    // Touch/Latch arm — Read plays back, Off ignores lanes entirely and writes nothing.
    void beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole role,
                                          std::uint32_t paramId,
                                          yesdaw::engine::EntityId ownerOverride = {})
    {
        automationTouchRideActive = false;
        automationTouchRideSamples.clear();

        if (! appModel.context().projectLoaded || ! appModel.context().isPlaying)
            return;
        const yesdaw::engine::AutomationMode mode = appModel.project().automationMode;
        if (mode != yesdaw::engine::AutomationMode::Touch
            && mode != yesdaw::engine::AutomationMode::Latch)
            return;

        yesdaw::engine::EntityId ownerId = ownerOverride;
        if (! ownerId.isValid())
        {
            ownerId = appModel.selectedSendOwnerEntityId();
            if (! ownerId.isValid())
                return;

            if (appModel.project().findBus (ownerId) != nullptr)
            {
                if (role == yesdaw::engine::AutomationTargetRole::TrackFader)
                    role = yesdaw::engine::AutomationTargetRole::BusFader;
                else if (role == yesdaw::engine::AutomationTargetRole::TrackPan)
                    role = yesdaw::engine::AutomationTargetRole::BusPan;
            }
        }

        automationTouchRideActive = true;
        automationTouchRideRole = role;
        automationTouchRideParamId = paramId;
        automationTouchRideTrackId = ownerId;
    }

    // N5: sample the live playhead tick and the control's current value into the ride buffer.
    // Deliberately does NOT touch project_/adoptEditedProject — every edit adoption resets the
    // transport to stopped (resetContextForFreshPlayback), so committing per-tick would collapse
    // every point in the ride to tick 0 after the very first write. Buffering client-side and
    // committing once, at the end of the ride, is what makes "breakpoints across a moved span"
    // possible at all.
    void recordAutomationTouchSample (double normalizedValue)
    {
        if (! automationTouchRideActive || ! appModel.project().sampleRate.isValid())
            return;

        const yesdaw::engine::Tick tick = static_cast<yesdaw::engine::Tick> (
            std::max<std::int64_t> (0, appModel.context().playheadFrame));
        automationTouchRideSamples.push_back ({ tick, normalizedValue });
    }

    // N5: commit the whole buffered ride as ONE undo step (the actual project write happens
    // here, and only here — see recordAutomationTouchSample's note on why).
    void endAutomationTouchRideIfActive()
    {
        if (! automationTouchRideActive)
            return;

        automationTouchRideActive = false;
        if (! automationTouchRideSamples.empty())
            (void) appModel.commitAutomationTouchRide (
                automationTouchRideTrackId, automationTouchRideRole, automationTouchRideParamId,
                automationTouchRideSamples);
        automationTouchRideSamples.clear();
        refreshActionState();
        repaintAll();
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

        // E33: rebuild the take stack for the selected clip's window.
        inspectorTakeViews = appModel.takesForSelectedClipWindow();
        inspectorTakeChooser.clear (juce::dontSendNotification);
        for (std::size_t view = 0; view < inspectorTakeViews.size(); ++view)
        {
            inspectorTakeChooser.addItem (
                "Take " + juce::String (inspectorTakeViews[view].takeOrdinal + 1u)
                    + (inspectorTakeViews[view].audible
                           ? juce::String (juce::CharPointer_UTF8 (" \xe2\x97\x8f"))
                           : juce::String()),
                static_cast<int> (view) + 1);
            if (inspectorTakeViews[view].audible
                && inspectorTakeChooser.getSelectedId() == 0)
                inspectorTakeChooser.setSelectedId (static_cast<int> (view) + 1,
                                                    juce::dontSendNotification);
        }
        const bool takesVisible = selected && ! inspectorTakeViews.empty()
                               && ! inspectorTakeChooser.getBounds().isEmpty();
        inspectorTakeChooser.setVisible (takesVisible);
        inspectorTakeDelete.setVisible (takesVisible);
        inspectorTakeChooser.setEnabled (takesVisible);
        inspectorTakeDelete.setEnabled (takesVisible && inspectorTakeChooser.getSelectedId() > 0);
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
        repaintAll();
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
        repaintAll();
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
        repaintAll();
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
        repaintAll();
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
        mixerSoloSafe.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetToggleSoloSafe,
                                                                appModel.context()).enabled);
        mixerSolo.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerTargetToggleSolo,
                                                            appModel.context()).enabled);
        mixerMetersReadout.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadMeters,
                                                                     appModel.context()).enabled);
        mixerSendsReadout.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadSends,
                                                                    appModel.context()).enabled);
        // G0.8: the registry state carries the "no send / no FX" reason itself (no dead affordance).
        mixerSendLevelEdit.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerSetFirstSendLevel,
                                          appModel.context()).enabled);
        mixerFxSlotsReadout.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadFxSlots,
                                                                      appModel.context()).enabled);
        mixerGainReductionReadout.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadGainReduction,
                                          appModel.context()).enabled);
        mixerBusFxSlotsReadout.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadBusFxSlots,
                                          appModel.context()).enabled);
        mixerFxSlotToggle.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerToggleFirstFxSlotEnabled,
                                          appModel.context()).enabled);
        refreshingMixerControls = true;
        // E19: the master fader reflects the persisted master gain and enables with a project.
        mixerMasterFader.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerMasterSetFader,
                                          appModel.context()).enabled);
        mixerMasterFader.setValue (appModel.context().projectLoaded
                                       ? static_cast<double> (appModel.project().masterLinearGain)
                                       : yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault,
                                   juce::dontSendNotification);
        if (projectHasTrack)
        {
            // E16: the control lane reads the SELECTED strip (track or bus), falling back to the
            // first track when nothing is targeted (the historical display).
            const yesdaw::engine::MixerStripState* stripView = appModel.selectedMixerStripView();
            if (stripView == nullptr)
                stripView = &project.tracks.front().strip;
            const auto& strip = *stripView;
            // R11: the master strip has no stored name — label it directly.
            mixerTrackSelect.setButtonText (appModel.selectedMixerTargetIsMaster()
                                                ? juce::String ("Master")
                                                : (strip.name.empty() ? "Track 1"
                                                                      : juce::String (strip.name)));
            mixerFader.setValue (strip.linearGain, juce::dontSendNotification);
            mixerPan.setValue (strip.pan, juce::dontSendNotification);
            mixerMute.setToggleState (selected && strip.muted, juce::dontSendNotification);
            mixerSolo.setToggleState (selected && strip.soloed, juce::dontSendNotification);
            mixerSoloSafe.setToggleState (selected && strip.soloSafe, juce::dontSendNotification);
            mixerMetersReadout.setButtonText (mixerMetersReadoutText());
            mixerSendsReadout.setButtonText (mixerSendsReadoutText());
            mixerSendLevelEdit.setButtonText ("Send");
            mixerFxSlotsReadout.setButtonText (mixerFxSlotsReadoutText());
            mixerGainReductionReadout.setButtonText (mixerGainReductionReadoutText());
            mixerBusFxSlotsReadout.setButtonText (mixerBusFxSlotsReadoutText());
            mixerFxSlotToggle.setButtonText ("FX");
            // E23: this readout is the FIRST-track FX bypass tool — it must read track 0's
            // chain, not the selected strip's (found by the cross-strip gate: selecting a
            // chain-less third track while track 0 had FX dereferenced an empty vector).
            mixerFxSlotToggle.setToggleState (appModel.context().firstTrackFxSlotAvailable
                                                  && project.tracks.front().strip.fxChain.front().enabled,
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
            mixerSoloSafe.setToggleState (false, juce::dontSendNotification);
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

    // N2: the strip every readout describes — the one the user SELECTED (tracks first, then
    // buses), falling back to the first strip when nothing is selected. Before N2 they all read
    // surface.tracks.front() regardless, so selecting track 3 left the panel reporting track 1.
    // Every readout NAMES the strip it describes, so it can never claim to be about another one.
    [[nodiscard]] static const yesdaw::ui::UiMixerStrip* readoutStripFor (
        const yesdaw::ui::UiMixerSurfaceSnapshot& surface, int selectedOrdinal) noexcept
    {
        const std::size_t trackCount = surface.tracks.size();
        const std::size_t stripTotal = trackCount + surface.buses.size();
        if (stripTotal == 0)
            return nullptr;

        const std::size_t ordinal =
            selectedOrdinal >= 0 && static_cast<std::size_t> (selectedOrdinal) < stripTotal
                ? static_cast<std::size_t> (selectedOrdinal)
                : 0;
        return ordinal < trackCount ? &surface.tracks[ordinal]
                                    : &surface.buses[ordinal - trackCount];
    }

    [[nodiscard]] static juce::String readoutStripName (const yesdaw::ui::UiMixerStrip& strip)
    {
        if (! strip.name.empty())
            return juce::String (strip.name);

        return strip.kind == yesdaw::ui::UiMixerTargetKind::Bus ? "Bus 1" : "Track 1";
    }

    [[nodiscard]] juce::String mixerMetersReadoutText() const
    {
        const auto surface = currentMixerSurface();
        const auto* strip = readoutStripFor (surface, appModel.selectedMixerStripOrdinal());
        if (strip == nullptr)
            return "Meters: no Track";

        // E24: a shipped readout speaks user language — no raw engine node ids.
        juce::String text = readoutStripName (*strip) + " meters:";

        if (! strip->meter.valid)
            return text + " peak n/a";

        return text
            + " L " + juce::String (strip->meter.peakLeft, 2)
            + " R " + juce::String (strip->meter.peakRight, 2);
    }

    [[nodiscard]] juce::String mixerSendsReadoutText() const
    {
        const auto surface = currentMixerSurface();
        const auto* strip = readoutStripFor (surface, appModel.selectedMixerStripOrdinal());
        if (strip == nullptr)
            return "Sends: no Track";

        // A Bus carries no sends in this model — say so instead of reporting some Track's.
        if (strip->kind == yesdaw::ui::UiMixerTargetKind::Bus)
            return readoutStripName (*strip) + " sends: n/a";

        if (strip->sends.empty())
            return readoutStripName (*strip) + " sends: none";

        const yesdaw::ui::UiMixerSendReadout& send = strip->sends.front();
        return readoutStripName (*strip)
             + " Send " + juce::String (static_cast<int> (send.sendOrdinal))
             + " level " + juce::String (send.normalizedLevel, 2)
             + " points " + juce::String (static_cast<int> (send.breakpointCount));
    }

    // M4: the strip-width name. "Compressor" does not fit a mixer strip — the slot rows use the
    // same short labels the control lane's slot buttons already use.
    [[nodiscard]] static const char* fxKindStripName (yesdaw::engine::FxKind kind) noexcept
    {
        switch (kind)
        {
            case yesdaw::engine::FxKind::Eq: return "EQ";
            case yesdaw::engine::FxKind::Compressor: return "Comp";
            case yesdaw::engine::FxKind::Delay: return "Delay";
            case yesdaw::engine::FxKind::Reverb: return "Reverb";
            case yesdaw::engine::FxKind::Limiter: return "Limiter";
        }

        return "FX";
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
        const auto* strip = readoutStripFor (surface, appModel.selectedMixerStripOrdinal());
        if (strip == nullptr)
            return "FX: no Track";

        if (strip->fxSlots.empty())
            return readoutStripName (*strip) + " FX: none";

        const yesdaw::ui::UiMixerFxSlotReadout& slot = strip->fxSlots.front();
        return readoutStripName (*strip)
             + " FX " + juce::String (static_cast<int> (slot.slotOrdinal))
             + " " + juce::String (fxKindName (slot.kind))
             + " params " + juce::String (static_cast<int> (slot.parameterCount))
             + (slot.enabled ? " on" : " off");
    }

    [[nodiscard]] juce::String mixerGainReductionReadoutText() const
    {
        const auto surface = currentMixerSurface();
        const auto* strip = readoutStripFor (surface, appModel.selectedMixerStripOrdinal());
        if (strip == nullptr)
            return "GR: no Track";

        const yesdaw::ui::UiMixerFxSlotReadout* readout = nullptr;
        for (const yesdaw::ui::UiMixerFxSlotReadout& slot : strip->fxSlots)
        {
            if (slot.gainReductionValid || slot.gainReductionAvailable)
            {
                readout = &slot;
                break;
            }
        }

        if (readout == nullptr)
            return readoutStripName (*strip) + " GR: none";

        juce::String text = readoutStripName (*strip)
            + " GR " + juce::String (static_cast<int> (readout->slotOrdinal))
            + " " + juce::String (fxKindName (readout->kind));

        if (readout->gainReductionValid)
            return text + " " + juce::String (readout->gainReductionDb, 2) + " dB";

        return text + " n/a";
    }

    [[nodiscard]] juce::String mixerBusFxSlotsReadoutText() const
    {
        const auto surface = currentMixerSurface();
        if (surface.buses.empty())
            return "Bus FX: no Bus";

        // N2: this one is Bus-specific by design — it follows the SELECTED bus, and falls back to
        // the first bus only when the selection is not a bus at all.
        const auto* selected = readoutStripFor (surface, appModel.selectedMixerStripOrdinal());
        const yesdaw::ui::UiMixerStrip& bus =
            selected != nullptr && selected->kind == yesdaw::ui::UiMixerTargetKind::Bus
                ? *selected
                : surface.buses.front();
        if (bus.fxSlots.empty())
            return readoutStripName (bus) + " FX: none";

        const yesdaw::ui::UiMixerFxSlotReadout& slot = bus.fxSlots.front();
        return readoutStripName (bus)
             + " FX " + juce::String (static_cast<int> (slot.slotOrdinal))
             + " " + juce::String (fxKindName (slot.kind))
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
        const auto headerBounds = getLocalBounds().withHeight (headerHeightNow());
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

        const HeaderLayout h = headerLayout();
        if (h.settingsVisible)
        {
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRect (h.settingsRow);
        }
        const std::array headerSections { h.toolsSection, h.transportSection, h.masterSection };
        for (const auto section : headerSections)
        {
            if (section.isEmpty())
                continue;
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
                        .withHeight (headerHeightNow())
                        .removeFromBottom (yesdaw::ui::UiTheme::Space::hairline));
    }

    // V2/V4: the project's HEAD tempo/meter with the shared no-map fallbacks (120 BPM, 4/4) —
    // the ONE read both the transport readout and the ruler's bar-label law consume, so the
    // header and the painted ruler can never disagree about what a bar is.
    struct HeadTempoMeter
    {
        double bpm = 120.0;
        std::uint16_t numerator = 4;
        std::uint16_t denominator = 4;
    };

    [[nodiscard]] HeadTempoMeter headTempoMeter() const
    {
        HeadTempoMeter head;
        if (! appModel.project().tempoMap.empty())
            head.bpm = appModel.project().tempoMap.front().bpm;
        if (! appModel.project().meterMap.empty())
        {
            head.numerator = appModel.project().meterMap.front().numerator;
            head.denominator = appModel.project().meterMap.front().denominator;
        }
        return head;
    }

    // V2: bar|beat at the current playhead — a single-tempo/meter law (the project's head
    // values, matching the existing headBarFrames() family's own scope). Shared by the paint
    // path below and the harness accessor, so a test can never duplicate this formula.
    [[nodiscard]] yesdaw::engine::BarBeat headerBarBeat() const
    {
        const double sampleRate = appModel.project().sampleRate.isValid()
                                      ? appModel.project().sampleRate.hz
                                      : 48000.0;
        const HeadTempoMeter head = headTempoMeter();
        return yesdaw::engine::computeBarBeat (
            head.bpm, head.numerator, head.denominator, sampleRate, appModel.context().playheadFrame);
    }

    // G1.4: the transport counter shows bars|beats AND minutes:seconds; a click on it swaps which
    // is the big one (Logic's display-mode click).
    struct CounterStrings
    {
        juce::String primary, secondary, mode;
    };

    [[nodiscard]] CounterStrings counterStrings() const
    {
        const yesdaw::engine::BarBeat barBeat = headerBarBeat();
        const juce::String bars = juce::String::formatted (
            "%03lld|%02lld", static_cast<long long> (barBeat.bar), static_cast<long long> (barBeat.beat));
        const double sampleRate = appModel.project().sampleRate.isValid() ? appModel.project().sampleRate.hz : 48000.0;
        const double seconds = std::max (0.0, static_cast<double> (appModel.context().playheadFrame) / sampleRate);
        const int minutes = static_cast<int> (seconds / 60.0);
        const double rest = seconds - 60.0 * minutes;
        const juce::String minSec = juce::String::formatted ("%d:%06.3f", minutes, rest);
        return timeDisplayMode == 0 ? CounterStrings { bars, minSec, "bars" }
                                    : CounterStrings { minSec, bars, "minsec" };
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (headerLayout().timeReadout.contains (event.getPosition()))
        {
            timeDisplayMode = timeDisplayMode == 0 ? 1 : 0;
            repaint (getLocalBounds().withHeight (headerHeightNow()));
        }
    }

    void drawTransportReadouts (juce::Graphics& g) const
    {
        const HeaderLayout h = headerLayout();
        auto time = h.timeReadout;
        fillPanel (g, time, yesdaw::ui::UiTheme::Radius::panel);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::transportClock));
        // V2: bar|beat, not a stopwatch clock — the SAME single-tempo/meter law V4's ruler
        // reuses, so the header readout and the ruler's bar numbers can never disagree.
        const CounterStrings counter = counterStrings();
        g.drawText (counter.primary,
                    time.reduced (yesdaw::ui::UiTheme::Layout::headerTransportTextInsetX,
                                  yesdaw::ui::UiTheme::Layout::headerTransportClockInsetY)
                        .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportClockHeight),
                    juce::Justification::centred,
                    false);
        // The caption row under the clock: trimmed from the TOP by the label inset (reducing on
        // both sides left nothing at the 44 px readout — the caption had been clipped since G0.7).
        drawSmallLabel (g,
                        counter.secondary,
                        time.withTrimmedTop (yesdaw::ui::UiTheme::Layout::headerTransportLabelInsetY)
                            .reduced (yesdaw::ui::UiTheme::Layout::headerTransportTextInsetX, yesdaw::ui::UiTheme::Space::hairline),
                        juce::Justification::centred);

        const juce::String tempo = appModel.context().projectLoaded && ! appModel.project().tempoMap.empty()
                                     ? juce::String (appModel.project().tempoMap.front().bpm, 2)
                                     : juce::String ("--");
        const juce::String meter = appModel.context().projectLoaded && ! appModel.project().meterMap.empty()
                                     ? juce::String (appModel.project().meterMap.front().numerator)
                                         + "/" + juce::String (appModel.project().meterMap.front().denominator)
                                     : juce::String ("--");
        // V2: the KEY cell is gone — D3 (no fake data): no key-signature model exists anywhere in
        // engine::Project, so a permanent "--" was a dead literal, not an honest empty state.
        const std::array<std::pair<juce::String, const char*>, 2> readouts {{
            { tempo, "TEMPO" },
            { meter, "TIME SIG" }
        }};

        auto box = h.tempoMeterBox;
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

public:
    // G0.7 (plan §3.4): the header as a flex row — tools left, transport centred on the window,
    // master card right-anchored against the gear — computed from the window width by ONE law
    // that resized(), the paint, the probe and the harness all read. Nothing here has a fixed x.
    struct HeaderLayout
    {
        juce::Rectangle<int> menuBar, toolsSection, transportSection, masterSection, settingsRow;
        juce::Rectangle<int> newButton, openButton, saveButton, importButton, undoButton, redoButton;
        juce::Rectangle<int> exportButton, exportProgress, exportCancel;
        juce::Rectangle<int> locateStart, play, stop, record, timeReadout, tempoMeterBox, loop;
        juce::Rectangle<int> masterCard, gear;
        juce::Rectangle<int> bitDepth, range, outputDevice, inputDevice, inputChannel;
        juce::Rectangle<int> arm, monitor, comp;
        bool settingsVisible = false;
    };

    [[nodiscard]] HeaderLayout headerLayout() const
    {
        using L = yesdaw::ui::UiTheme::Layout;
        HeaderLayout h;
        const int width = getWidth();
        h.menuBar = L::headerMenuBarBounds();
        const int controlY = L::menuBarHeight + (L::toolbarHeight - L::headerControlHeight) / 2;
        const int bigY = L::menuBarHeight + (L::toolbarHeight - L::headerTransportButtonSize) / 2;

        // Tools, left: New Open Save Import · Undo Redo · Export.
        int x = L::headerEdgeInset;
        const auto small = [&x] (int w)
        {
            const juce::Rectangle<int> r (x, controlY, w, L::headerControlHeight);
            x += w + L::headerButtonGap;
            return r;
        };
        h.newButton = small (L::headerSmallButtonWidth);
        h.openButton = small (L::headerSmallButtonWidth);
        h.saveButton = small (L::headerSmallButtonWidth);
        h.importButton = small (L::headerSmallButtonWidth);
        x += L::headerClusterGap - L::headerButtonGap;
        h.undoButton = small (L::headerUndoButtonWidth);
        h.redoButton = small (L::headerUndoButtonWidth);
        x += L::headerClusterGap - L::headerButtonGap;
        h.exportButton = small (L::headerExportButtonWidth);
        h.exportProgress = h.exportButton.withWidth (L::headerExportProgressWidth);
        h.exportCancel = juce::Rectangle<int> (h.exportButton.getRight() - L::headerExportCancelWidth,
                                               controlY, L::headerExportCancelWidth, L::headerControlHeight);
        const int toolsRight = x - L::headerButtonGap;
        h.toolsSection = juce::Rectangle<int> (L::headerEdgeInset, bigY, toolsRight - L::headerEdgeInset,
                                               L::headerTransportButtonSize)
                             .expanded (L::headerSectionPad);

        // Gear, right edge.
        h.gear = juce::Rectangle<int> (width - L::headerStatusIconRightInset, L::headerStatusIconY,
                                       L::headerStatusIconSize, L::headerStatusIconSize);

        // Transport, centred on the window; pushed right of the tools when the window is narrow
        // and never past the gear.
        const int centreWidth = 4 * L::headerTransportButtonSize + 3 * L::headerButtonGap
                              + L::headerClusterGap + L::headerTransportTimeWidth
                              + L::headerClusterGap + L::headerTransportBoxWidth
                              + L::headerClusterGap + L::headerLoopButtonWidth;
        const int minStart = toolsRight + L::headerGroupGap;
        int cx = juce::jmax (minStart, width / 2 - centreWidth / 2);
        cx = juce::jmax (minStart, juce::jmin (cx, h.gear.getX() - L::headerMasterGearGap - centreWidth));

        // Master card: right-anchored against the gear, shrinks toward the transport group,
        // drops WHOLE below its minimum (M9's law, now relative to the centred group).
        const int cardRight = h.gear.getX() - L::headerMasterGearGap;
        const int cardWidth = juce::jmin (L::headerMasterWidth, cardRight - (cx + centreWidth + L::headerGroupGap));
        if (cardWidth >= L::headerMasterMinWidth)
            h.masterCard = juce::Rectangle<int> (cardRight - cardWidth, L::headerMasterY, cardWidth, L::headerMasterHeight);

        x = cx;
        const auto big = [&x] (int w)
        {
            const juce::Rectangle<int> r (x, bigY, w, L::headerTransportButtonSize);
            x += w + L::headerButtonGap;
            return r;
        };
        h.locateStart = big (L::headerTransportButtonSize);
        h.play = big (L::headerTransportButtonSize);
        h.stop = big (L::headerTransportButtonSize);
        h.record = big (L::headerTransportButtonSize);
        x += L::headerClusterGap - L::headerButtonGap;
        h.timeReadout = big (L::headerTransportTimeWidth);
        x += L::headerClusterGap - L::headerButtonGap;
        h.tempoMeterBox = big (L::headerTransportBoxWidth);
        x += L::headerClusterGap - L::headerButtonGap;
        h.loop = big (L::headerLoopButtonWidth);
        h.transportSection = juce::Rectangle<int> (cx, bigY, centreWidth, L::headerTransportButtonSize)
                                 .expanded (L::headerSectionPad);
        if (! h.masterCard.isEmpty())
            h.masterSection = juce::Rectangle<int> (h.masterCard.getX(), bigY,
                                                    h.gear.getRight() - h.masterCard.getX(),
                                                    L::headerTransportButtonSize)
                                  .expanded (L::headerSectionPad);

        // The settings row (export choosers, device choosers, the recording cluster).
        h.settingsVisible = appModel.context().settingsRowVisible;
        if (h.settingsVisible)
        {
            h.settingsRow = juce::Rectangle<int> (0, kHeaderHeight, width, L::settingsRowHeight);
            const int rowY = kHeaderHeight + (L::settingsRowHeight - L::headerControlHeight) / 2;
            x = L::headerEdgeInset;
            const auto cell = [&x] (int w)
            {
                const juce::Rectangle<int> r (x, rowY, w, L::headerControlHeight);
                x += w + L::headerButtonGap;
                return r;
            };
            h.bitDepth = cell (L::settingsBitDepthWidth);
            h.range = cell (L::settingsRangeWidth);
            h.outputDevice = cell (L::settingsDeviceWidth);
            h.inputDevice = cell (L::settingsDeviceWidth);
            h.inputChannel = cell (L::settingsChannelWidth);
            h.arm = cell (L::settingsArmWidth);
            h.monitor = cell (L::settingsMonitorWidth);
            h.comp = cell (L::settingsCompWidth);
        }
        return h;
    }

    // G0.7: the header's height right now — the fixed menu + toolbar, plus the settings row when
    // it is shown. Every work-area layout trims THIS, never the constant.
    [[nodiscard]] int headerHeightNow() const
    {
        return kHeaderHeight + (appModel.context().settingsRowVisible ? yesdaw::ui::UiTheme::Layout::settingsRowHeight : 0);
    }

    // Harness: show/hide the settings row through the real action (the Options menu's toggle).
    void harnessSetSettingsRowVisible (bool visible)
    {
        if (appModel.context().settingsRowVisible != visible)
            handleAction (yesdaw::ui::UiActionId::ViewToggleSettingsRow);
    }

    // G0.8 harness: dispatch an action the way a menu item or chord would (the test device verb
    // has neither, by design); and read the registry's live state for one.
    void harnessDispatchAction (yesdaw::ui::UiActionId action)
    {
        // Exactly what a toolbar button's click does.
        handleAction (action);
        refreshActionState();
        resized();
        repaintAll();
    }
    [[nodiscard]] yesdaw::ui::UiActionState harnessActionState (yesdaw::ui::UiActionId action) const
    {
        return appModel.registry().stateFor (action, appModel.context());
    }

    // M9: the header's master card — right-anchored against the gear, drops WHOLE (empty rect)
    // when it cannot keep its minimum width next to the centred transport group.
    [[nodiscard]] juce::Rectangle<int> headerMasterCardBounds() const
    {
        return headerLayout().masterCard;
    }

    [[nodiscard]] juce::Rectangle<int> headerMasterLufsBounds() const
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const auto card = headerMasterCardBounds();
        if (card.isEmpty())
            return {};

        return juce::Rectangle<int> (card.getRight() - L::headerMasterLufsWidth,
                                     L::headerMasterLufsY,
                                     L::headerMasterLufsWidth,
                                     L::headerMasterLufsHeight);
    }

private:
    void drawMasterMeter (juce::Graphics& g) const
    {
        auto master = headerMasterCardBounds();
        if (master.isEmpty())
        {
            // The card is gone; the gear still belongs to the window edge.
            yesdaw::ui::drawSettingsIcon (
                g,
                headerLayout().gear.toFloat(),
                kMutedText);
            return;
        }

        drawSmallLabel (g, "MASTER", master.removeFromTop (yesdaw::ui::UiTheme::Layout::headerMasterLabelHeight));
        const int meterWidth = juce::jmin (yesdaw::ui::UiTheme::Layout::headerMasterMeterWidth,
                                           master.getWidth()
                                               - yesdaw::ui::UiTheme::Layout::headerMasterLufsWidth
                                               - yesdaw::ui::UiTheme::Layout::headerMasterLufsGap);
        auto meter = master.removeFromTop (yesdaw::ui::UiTheme::Layout::headerMasterMeterHeight)
                         .withWidth (juce::jmax (yesdaw::ui::UiTheme::Layout::headerMasterLufsGap, meterWidth));
        drawHorizontalMeter (g, meter, liveMasterPeakLeft.load (std::memory_order_acquire));

        yesdaw::ui::drawSettingsIcon (
            g,
            headerLayout().gear.toFloat(),
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

        // N6: row heights come from the SAME cumulative law rowBounds/rowAt use — a resized row
        // paints at exactly the height/position hit-testing agrees on.
        const int rowCount = static_cast<int> (appModel.project().tracks.size());
        const yesdaw::ui::CumulativeRowGeometry rowLaw = trackListInput.rowGeometry (rowCount, area.getHeight());
        // Vertical track scroll (E5): the rail paints from its effective (clamped) shared row
        // offset; scrolled-out rows above the window are skipped so paint matches rowBounds/rowAt.
        for (std::size_t i = static_cast<std::size_t> (trackListInput.effectiveScrollRows());
             i < appModel.project().tracks.size(); ++i)
        {
            const int rowHeight = static_cast<int> (std::llround (rowLaw.heightFor (static_cast<int> (i))));
            auto row = area.removeFromTop (rowHeight);
            if (row.getHeight() < rowHeight)
                break;
            const auto& projectTrack = appModel.project().tracks[i];
            const juce::String fallbackName = "Track " + juce::String (static_cast<int> (i + 1));
            const juce::String trackName = projectTrack.strip.name.empty()
                                               ? fallbackName
                                               : juce::String (projectTrack.strip.name);
            // N7: a customized colour overrides the historical fixed purple everywhere this
            // variable is used below (accent bar / swatch, glyph tint, pan indicator, level fill).
            const juce::Colour trackColour = colourForTrack (projectTrack, kPurple);

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

            // V5: the mini VOL is a VERTICAL fader (top = loud), the same rect law
            // volumeSliderBounds hit-tests and the same orientation the mixer strip fader uses.
            auto level = row.withRight (
                                row.getRight()
                                - yesdaw::ui::UiTheme::Layout::trackListLevelColumnRightInset)
                             .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListLevelColumnWidth)
                             .reduced (yesdaw::ui::UiTheme::Space::none,
                                       yesdaw::ui::UiTheme::Layout::trackListLevelColumnVerticalInset);
            g.setColour (yesdaw::ui::UiTheme::Color::meterTrack().withAlpha (
                yesdaw::ui::UiTheme::Tone::trackSliderRailAlpha));
            g.fillRoundedRectangle (level.toFloat(), yesdaw::ui::UiTheme::Radius::pill);
            const int liveHeight = juce::roundToInt (
                static_cast<float> (level.getHeight()) * projectTrack.strip.linearGain);
            g.setColour (trackColour.withAlpha (yesdaw::ui::UiTheme::Tone::trackSliderFillAlpha));
            g.fillRoundedRectangle (
                level.withTop (level.getBottom() - liveHeight).toFloat(),
                yesdaw::ui::UiTheme::Radius::pill);
            auto levelThumb = level.withHeight (yesdaw::ui::UiTheme::Layout::trackListLevelThumbHeight)
                                  .withY (level.getBottom() - liveHeight
                                          - yesdaw::ui::UiTheme::Layout::trackListLevelThumbHeight / 2);
            g.setColour (yesdaw::ui::UiTheme::Color::faderThumbTop());
            g.fillRoundedRectangle (levelThumb.toFloat(), yesdaw::ui::UiTheme::Radius::sm);

            g.setColour (kText);
            g.setFont (yesdaw::ui::UiTheme::Type::font (
                yesdaw::ui::UiTheme::Type::title,
                juce::Font::bold));
            // G0.7 cp2: the name cell ends where the mix cluster (PAN/VOL, knob, level, meter)
            // begins; a long name ellipsises instead of running under it.
            g.drawText (trackName,
                        row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                            .withRight (row.getRight() - yesdaw::ui::UiTheme::Layout::trackListMixSummaryRightInset
                                        - yesdaw::ui::UiTheme::Layout::trackListMixSummaryWidth
                                        - yesdaw::ui::UiTheme::Layout::trackListButtonInsetX)
                            .withHeight (yesdaw::ui::UiTheme::Layout::trackListNameHeight)
                            .translated (yesdaw::ui::UiTheme::Layout::trackListNameOffsetX,
                                         yesdaw::ui::UiTheme::Layout::trackListNameOffsetY),
                        juce::Justification::centredLeft, true);

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
            // E30: the "O" cell is the REAL record-arm badge — lit red on the armed track.
            // M11: EVERY armed track's badge lights, not just the primary's.
            const bool rowArmed = appModel.isRecordingTrackIndexArmed (i);
            const std::array<std::pair<const char*, bool>, 3> railCells {{
                { "M", projectTrack.strip.muted },
                { "S", projectTrack.strip.soloed },
                { "O", rowArmed },
            }};
            for (const auto& [label, active] : railCells)
            {
                const bool armCell = label == std::string ("O");
                auto cell = buttonsArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::trackListButtonWidth)
                                .reduced (yesdaw::ui::UiTheme::Layout::trackListButtonInsetX,
                                          yesdaw::ui::UiTheme::Layout::trackListButtonInsetY);
                g.setColour (active ? (armCell ? kRed : trackColour)
                                    : yesdaw::ui::UiTheme::Color::mixerBack());
                g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                g.setColour (active ? kText : (armCell ? kRed : kMutedText));
                g.setFont (yesdaw::ui::UiTheme::Type::font (
                    yesdaw::ui::UiTheme::Type::caption,
                    juce::Font::bold));
                g.drawText (label, cell, juce::Justification::centred, false);
            }

            // Live meter (usable-DAW P2 + B32 + V5): the rail meter renders INDEPENDENT L/R
            // columns from the MeterNode's per-channel peaks (the node taps post-pan, so a
            // hard-panned track honestly meters one-sided); each column runs the shared
            // hold/clip-latch law and a click on the zone clears both.
            auto meter = row.withRight (row.getRight() - yesdaw::ui::UiTheme::Layout::trackListMeterRightInset)
                             .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListMeterWidth)
                             .reduced (yesdaw::ui::UiTheme::Layout::trackListMeterHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::trackListMeterVerticalInset);
            const std::array<MeterHoldState, 2> railHoldLR =
                i < trackMeterHoldLR.size() ? trackMeterHoldLR[i]
                                            : std::array<MeterHoldState, 2> {};
            const int channelWidth =
                (meter.getWidth() - yesdaw::ui::UiTheme::Layout::trackListMeterChannelGap) / 2;
            const auto meterLeft = meter.withWidth (channelWidth);
            const auto meterRight = meter.withTrimmedLeft (
                meter.getWidth() - channelWidth);
            drawMeterWithHold (g, meterLeft, railHoldLR[0].livePeak, railHoldLR[0].heldPeak,
                               railHoldLR[0].clipLatched);
            drawMeterWithHold (g, meterRight, railHoldLR[1].livePeak, railHoldLR[1].heldPeak,
                               railHoldLR[1].clipLatched);
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
            state.clipNotes = timelineClipNotes.empty() ? nullptr : timelineClipNotes.data();
            state.clipNoteCount = static_cast<int> (timelineClipNotes.size());
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

        // V4: the ruler's bar length comes from the SAME head tempo/meter read the transport
        // readout uses, through the SAME engine grid law (sampleRateHz = 1.0 makes computeBarGrid
        // yield seconds) — the ruler's bar numbers and the header's bar|beat share one law.
        {
            const HeadTempoMeter head = headTempoMeter();
            state.barSeconds =
                yesdaw::engine::computeBarGrid (head.bpm, head.numerator, head.denominator, 1.0)
                    .barFrames;
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

        // N4: the automation lane anchors under the SAME track automationTargetTrackId() resolves
        // (identical clamp), so the band's position can never disagree with the header/canvas
        // about which track is being edited.
        state.automationLaneVisible = appModel.context().timelineAutomationTrackLaneVisible;
        state.automationLaneTrackRow = appModel.context().projectLoaded && ! appModel.project().tracks.empty()
            ? std::clamp (selectedTrackLane, 0, static_cast<int> (appModel.project().tracks.size()) - 1)
            : -1;

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

        // Transport loop brace (E6): painted and hit-tested from the real transport loop.
        if (appModel.context().loopEnabled
            && appModel.context().projectLoaded
            && appModel.project().sampleRate.isValid())
        {
            const std::int64_t loopStart = appModel.playbackLoopStartFrame();
            const std::int64_t loopEnd = appModel.playbackLoopEndFrame();
            if (loopEnd > loopStart && loopStart >= 0)
            {
                state.loopActive = true;
                state.loopStartSeconds = static_cast<double> (loopStart) / appModel.project().sampleRate.hz;
                state.loopEndSeconds = static_cast<double> (loopEnd) / appModel.project().sampleRate.hz;
            }
        }

        // N8: the persisted punch region — painted from the real Project field, so an unset
        // region paints nothing (bit-identical to before this field existed).
        if (appModel.context().projectLoaded && appModel.project().sampleRate.isValid())
        {
            const yesdaw::engine::PunchRegion punch = appModel.punchRegion();
            if (punch.enabled && punch.endFrame > punch.startFrame)
            {
                state.punchActive = true;
                state.punchStartSeconds = static_cast<double> (punch.startFrame) / appModel.project().sampleRate.hz;
                state.punchEndSeconds = static_cast<double> (punch.endFrame) / appModel.project().sampleRate.hz;
            }
        }
        return state;
    }

    void rebuildTimelineClipViews()
    {
        timelineClips.clear();
        timelineClipNotes.clear();
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
                                               colourForTrack (track, kPurple),
                                               0.0f,
                                               track.heightPx });

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
            // V6: selection is a painted RING, not a colour swap — the clip keeps its N7 track
            // colour while selected (the old accent-blue swap was invisible on a blue track,
            // the exact false-positive risk the N7 gate had to work around).
            timelineClipStyles.push_back ({ colourForTrack (*track, kPurple),
                                            yesdaw::ui::UiTheme::Tone::mainComponentProjectClipAlpha,
                                            appModel.isTimelineClipSelected (clip.id),
                                            static_cast<long long> (clip.timelineLength),
                                            static_cast<long long> (clip.fadeIn),
                                            static_cast<long long> (clip.fadeOut) });
            timelineClipIds.push_back (clip.id);
            timelineClipAssetHashes.push_back (asset->contentHash);
            endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
        }

        // MIDI clips are first-class timeline citizens (E8): painted on their track lanes in the
        // MIDI accent colour and hit-testable through the same layout ids as audio clips.
        for (const yesdaw::engine::MidiClip& midiClip : project.midiClips)
        {
            if (! midiClip.id.isValid() || midiClip.timelineStart < 0 || midiClip.timelineLength <= 0)
                continue;

            const auto track = std::find_if (project.tracks.begin(), project.tracks.end(), [&midiClip] (const auto& candidate) {
                return candidate.id == midiClip.trackId;
            });
            if (track == project.tracks.end())
                continue;

            const int lane = static_cast<int> (std::distance (project.tracks.begin(), track));
            const double startSeconds = static_cast<double> (midiClip.timelineStart) / sampleRate;
            const double lengthSeconds = static_cast<double> (midiClip.timelineLength) / sampleRate;
            const int id = static_cast<int> (timelineClips.size());
            timelineClips.push_back ({ id, lane, startSeconds, lengthSeconds, "MIDI" });
            // M7: hand the canvas this clip's real notes so it can paint what the clip CONTAINS
            // instead of falling through to the placeholder waveform.
            for (const yesdaw::engine::Note& note : midiClip.notes)
            {
                double noteStartFrame = 0.0;
                double noteEndFrame = 0.0;
                if (! yesdaw::engine::tickToFrame (
                        yesdaw::engine::TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                        project.sampleRate,
                        midiClip.timelineStart + note.startTick,
                        noteStartFrame)
                    || ! yesdaw::engine::tickToFrame (
                        yesdaw::engine::TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                        project.sampleRate,
                        midiClip.timelineStart + note.startTick + note.lengthTicks,
                        noteEndFrame))
                {
                    continue;
                }

                timelineClipNotes.push_back ({ id,
                                               noteStartFrame / sampleRate,
                                               std::max (0.0, (noteEndFrame - noteStartFrame) / sampleRate),
                                               static_cast<int> (note.key) });
            }
            // V6: same ring law as audio clips; MIDI clips have no fade model, so the fade tick
            // fields stay honestly zero (nothing paints).
            timelineClipStyles.push_back ({ colourForTrack (*track, yesdaw::ui::UiTheme::Color::accentCyan()),
                                            yesdaw::ui::UiTheme::Tone::mainComponentProjectClipAlpha,
                                            appModel.isTimelineClipSelected (midiClip.id),
                                            static_cast<long long> (midiClip.timelineLength),
                                            0,
                                            0 });
            timelineClipIds.push_back (midiClip.id);
            timelineClipAssetHashes.push_back ({});
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
        repaintAll();
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

    // E20: the automation lane target — what the canvas edits. R14: a target may be owned by a
    // BUS (fader/pan/send) — its label carries the bus name, and the lane-row header drops the
    // track prefix for it.
    struct AutomationTargetOption
    {
        yesdaw::engine::AutomationTargetRole role = yesdaw::engine::AutomationTargetRole::TrackFader;
        std::uint32_t paramId = 0;
        yesdaw::engine::EntityId ownerEntity {};
        juce::String label;
        bool busOwned = false;
    };

    // E20: enumerate the selected track's automation targets in a stable order — fader, pan,
    // each send level, then each FX param of each insert.
    [[nodiscard]] std::vector<AutomationTargetOption> buildAutomationTargetOptions() const
    {
        std::vector<AutomationTargetOption> options;
        const yesdaw::engine::EntityId trackId = automationTargetTrackId();
        if (! trackId.isValid())
            return options;

        options.push_back ({ yesdaw::engine::AutomationTargetRole::TrackFader,
                             yesdaw::engine::FaderNode::kGainParameterId, trackId, "Fader" });
        options.push_back ({ yesdaw::engine::AutomationTargetRole::TrackPan,
                             yesdaw::engine::PanNode::kPanParameterId, trackId, "Pan" });

        const yesdaw::engine::Track* track = nullptr;
        for (const yesdaw::engine::Track& candidate : appModel.project().tracks)
            if (candidate.id == trackId)
                track = &candidate;
        if (track == nullptr)
            return options;

        for (std::size_t sendIndex = 0; sendIndex < track->sends.size(); ++sendIndex)
        {
            juce::String busName ("Bus?");
            for (const auto& bus : appModel.project().buses)
                if (bus.id == track->sends[sendIndex].busId)
                    busName = juce::String (bus.strip.name);
            options.push_back ({ yesdaw::engine::AutomationTargetRole::SendLevel,
                                 static_cast<std::uint32_t> (sendIndex), trackId,
                                 "Send: " + busName });
        }

        for (std::size_t slot = 0; slot < track->strip.fxChain.size(); ++slot)
        {
            const yesdaw::engine::FxInsert& insert = track->strip.fxChain[slot];
            for (std::uint32_t paramId = 0;
                 paramId < yesdaw::ui::UiTheme::Layout::mixerFxParamProbeLimit;
                 ++paramId)
            {
                if (! yesdaw::engine::fxKindAcceptsParameterId (insert.kind, paramId))
                    continue;

                const yesdaw::engine::ParamSpec spec =
                    yesdaw::engine::fxParamSpecForKind (insert.kind, paramId);
                options.push_back ({ yesdaw::engine::AutomationTargetRole::FxInsertParam,
                                     paramId, insert.id,
                                     "FX" + juce::String (static_cast<int> (slot) + 1)
                                         + " " + spec.name });
            }
        }

        // R14: bus automation is reachable — every bus's fader, pan, and (R13) send levels
        // enumerate after the track's own targets, labelled by bus name. The engine targets
        // (BusFader/BusPan since M-era, bus SendLevel since R13) were dead code from the shell
        // until this list carried them; the same canvas pencils their lanes unchanged.
        for (std::size_t busIndex = 0; busIndex < appModel.project().buses.size(); ++busIndex)
        {
            const yesdaw::engine::Bus& bus = appModel.project().buses[busIndex];
            const juce::String busName = bus.strip.name.empty()
                ? "Bus " + juce::String (static_cast<int> (busIndex) + 1)
                : juce::String (bus.strip.name);
            options.push_back ({ yesdaw::engine::AutomationTargetRole::BusFader,
                                 yesdaw::engine::FaderNode::kGainParameterId, bus.id,
                                 busName + " Fader", true });
            options.push_back ({ yesdaw::engine::AutomationTargetRole::BusPan,
                                 yesdaw::engine::PanNode::kPanParameterId, bus.id,
                                 busName + " Pan", true });
            for (std::size_t sendIndex = 0; sendIndex < bus.sends.size(); ++sendIndex)
            {
                juce::String destName ("Bus?");
                for (const auto& dest : appModel.project().buses)
                    if (dest.id == bus.sends[sendIndex].busId)
                        destName = juce::String (dest.strip.name);
                options.push_back ({ yesdaw::engine::AutomationTargetRole::SendLevel,
                                     static_cast<std::uint32_t> (sendIndex), bus.id,
                                     busName + " Send: " + destName, true });
            }
        }
        return options;
    }

    [[nodiscard]] AutomationTargetOption currentAutomationTarget() const
    {
        if (selectedAutomationTargetIndex >= 0
            && selectedAutomationTargetIndex < static_cast<int> (automationTargetOptions.size()))
            return automationTargetOptions[static_cast<std::size_t> (selectedAutomationTargetIndex)];

        AutomationTargetOption fallback;
        fallback.ownerEntity = automationTargetTrackId();
        fallback.paramId = yesdaw::engine::FaderNode::kGainParameterId;
        fallback.label = "Fader";
        return fallback;
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
        repaintAll();
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
        repaintAll();
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
        repaintAll();
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
        repaintAll();
    }

    void trimTimelineClipRightByLayoutId (int layoutClipId, double endSeconds, bool snapInvert = false)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        if (const auto tick = timelineTickFromSeconds (endSeconds))
            (void) appModel.trimSelectedTimelineClipRightTo (snappedTimelineTick (*tick, snapInvert));

        refreshActionState();
        repaintAll();
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
        repaintAll();
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
        repaintAll();
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
        // E9: the header names the OPEN clip's owning track so switching clips is legible.
        juce::String rollTitle = "No MIDI Clip selected";
        if (surface.midiClipSelected)
        {
            rollTitle = "MIDI Clip";
            for (const yesdaw::engine::MidiClip& midiClip : appModel.project().midiClips)
            {
                if (midiClip.id != appModel.selectedMidiClipId())
                    continue;
                for (const yesdaw::engine::Track& track : appModel.project().tracks)
                    if (track.id == midiClip.trackId && ! track.strip.name.empty())
                        rollTitle = juce::String (track.strip.name);
                break;
            }
            rollTitle << "  |  Note edits: select move length transpose quantize";
        }
        drawSmallLabel (g, rollTitle,
                        header.reduced (yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetX,
                                        yesdaw::ui::UiTheme::Layout::pianoRollHeaderLabelInsetY),
                        juce::Justification::centredRight);

        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (panelArea);

        g.setColour (yesdaw::ui::UiTheme::Color::controlInsetBlack());
        g.fillRect (geometry.grid);

        for (int key = pianoRollViewHighKey (surface);
             key >= surface.viewLowKey;
             --key)
        {
            const int y = pianoRollKeyY (geometry, surface, key);
            auto keyRow = juce::Rectangle<int> (geometry.keyboard.getX(),
                                                y,
                                                geometry.keyboard.getWidth(),
                                                juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollKeyRowMinHeight,
                                                            juce::roundToInt (geometry.rowHeight)));
            // M8: a real keyboard — white keys light and full width, black keys dark and narrower,
            // sitting on top from the left edge exactly as they do on a piano.
            const auto keyBody = keyRow.reduced (yesdaw::ui::UiTheme::Layout::pianoRollKeyRowInsetX,
                                                 yesdaw::ui::UiTheme::Layout::pianoRollKeyRowInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::pianoWhiteKey());
            g.fillRect (keyBody);
            g.setColour (kPanelStroke);
            g.drawRect (keyBody, yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth);
            if (isBlackMidiKey (key))
            {
                g.setColour (yesdaw::ui::UiTheme::Color::pianoBlackKey());
                g.fillRect (keyBody.withWidth (juce::roundToInt (
                    static_cast<float> (keyBody.getWidth())
                    * yesdaw::ui::UiTheme::Layout::pianoRollBlackKeyWidthScale)));
            }
            g.setColour (kPanelStroke.withAlpha (0.72f));
            g.fillRect (juce::Rectangle<int> (geometry.grid.getX(),
                                             y,
                                             geometry.grid.getWidth(),
                                             yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth));

            if (key % 12 == 0)
            {
                g.setColour (yesdaw::ui::UiTheme::Color::pianoWhiteKeyText());
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
            if (x < geometry.grid.getX() || x > geometry.grid.getRight())
                continue;
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
            if (note.key < surface.viewLowKey || note.key > pianoRollViewHighKey (surface))
                continue;

            const auto noteRect = pianoRollNoteBounds (geometry, surface, note)
                                      .getIntersection (geometry.grid);
            if (noteRect.isEmpty())
                continue;

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

            // M8: velocity is a BAR per note, anchored at the note's start and rising from the lane
            // floor — the joined line read as an automation curve between notes that never existed.
            if (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity)
            {
                const int floorY = laneArea.getBottom()
                                 - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathBottomInset;
                const int span = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollVelocityBarMinHeight,
                                             laneArea.getHeight()
                                                 - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathVerticalInset);
                g.setColour (yesdaw::ui::UiTheme::Meter::nominalFill());
                for (const auto& point : lane.points)
                {
                    const double normalized = juce::jlimit (0.0, 1.0,
                                                            (point.value - minValue) / (maxValue - minValue));
                    const int x = pianoRollTickX (geometry, surface, point.tick);
                    const int height = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollVelocityBarMinHeight,
                                                   juce::roundToInt (normalized * static_cast<double> (span)));
                    g.fillRect (x, floorY - height,
                                yesdaw::ui::UiTheme::Layout::pianoRollVelocityBarWidth, height);
                }
                continue;
            }

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

    // V7: the TRACK tab's painted content — the honest track-scoped subset that already exists
    // in the model: name + N7 colour, fader/pan/mute/solo strip state, and the REAL track FX
    // chain (a clip-level FX model does not exist, so the old always-"None" CLIP FX stub is
    // gone; the reference's FX list maps to this real one).
    // V7: the fade-chart card and its inner chart rect — ONE law shared by paint and the
    // harness accessor, so a gate can cross-check the painted curve against the shared
    // clipFadeCurvePoints law without re-deriving the geometry.
    [[nodiscard]] juce::Rectangle<int> inspectorFadeChartCardBounds() const
    {
        auto area = inspectorBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorTabHeight);
        area.reduce (yesdaw::ui::UiTheme::Layout::inspectorContentInsetX,
                     yesdaw::ui::UiTheme::Layout::inspectorContentInsetY);
        return area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFxSectionTop)
                   .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFxSectionHeight);
    }

    [[nodiscard]] juce::Rectangle<int> inspectorFadeChartBounds() const
    {
        auto card = inspectorFadeChartCardBounds();
        card.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight);
        return card.reduced (yesdaw::ui::UiTheme::Layout::inspectorFxTextInsetX,
                             yesdaw::ui::UiTheme::Layout::inspectorFxTextInsetY);
    }

    void drawTrackInspector (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        const auto& tracks = appModel.project().tracks;
        if (! appModel.context().projectLoaded || tracks.empty())
        {
            drawSmallLabel (g, "No track", area, juce::Justification::centred);
            return;
        }

        const int lane = std::clamp (selectedTrackLane, 0, static_cast<int> (tracks.size()) - 1);
        const yesdaw::engine::Track& track = tracks[static_cast<std::size_t> (lane)];

        g.setColour (colourForTrack (track, kPurple));
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
        g.drawText (track.strip.name.empty() ? "Track" : track.strip.name.c_str(),
                    area.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorTitleTextLeftInset)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTitleTextHeight),
                    juce::Justification::centredLeft,
                    false);

        auto rows = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop);
        std::vector<juce::String> rowText;
        rowText.push_back ("Fader   " + dbReadoutText (track.strip.linearGain));
        const int panPercent = juce::roundToInt (std::abs (track.strip.pan) * 100.0f);
        rowText.push_back ("Pan     "
                           + (panPercent == 0 ? juce::String ("C")
                                              : (track.strip.pan < 0.0f ? juce::String ("L")
                                                                        : juce::String ("R"))
                                                    + juce::String (panPercent)));
        rowText.push_back (juce::String ("Mute ") + (track.strip.muted ? "on" : "off")
                           + "   Solo " + (track.strip.soloed ? "on" : "off"));
        if (track.strip.fxChain.empty())
            rowText.push_back ("Track FX: none");
        else
            for (std::size_t slot = 0; slot < track.strip.fxChain.size(); ++slot)
                rowText.push_back ("FX " + juce::String (static_cast<int> (slot) + 1) + "  "
                                   + fxKindName (track.strip.fxChain[slot].kind));

        for (const juce::String& text : rowText)
        {
            auto row = rows.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeRowHeight)
                           .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetX,
                                     yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetY);
            if (! area.contains (row))
                break;
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (kText);
            g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::body));
            g.drawText (text,
                        row.reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeTextInsetX,
                                     yesdaw::ui::UiTheme::Layout::inspectorFadeTextInsetY),
                        juce::Justification::centredLeft,
                        false);
        }
    }

    void drawInspector (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        fillPanel (g, area);
        // V7: the tab strip itself is two REAL buttons (inspectorClipTab/inspectorTrackTab)
        // placed by layoutInspectorControls over this reserved band — nothing decorative to
        // paint here any more.
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorTabHeight);

        area.reduce (yesdaw::ui::UiTheme::Layout::inspectorContentInsetX,
                     yesdaw::ui::UiTheme::Layout::inspectorContentInsetY);

        if (appModel.context().inspectorTrackTabActive)
        {
            drawTrackInspector (g, area);
            return;
        }

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

        // E24/E27: the SAME whole-section drop law as layoutInspectorControls — a section
        // whose card no longer fits the column paints nothing at all.
        const juce::Rectangle<int> inspectorContent = area;
        const auto drawInspectorSectionCard =
            [&g, inspectorContent] (juce::Rectangle<int> section) -> bool
        {
            if (! inspectorContent.contains (section))
                return false;

            g.setColour (yesdaw::ui::UiTheme::Color::panelRaised());
            g.fillRoundedRectangle (section.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
                yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
            g.drawRoundedRectangle (
                section.toFloat().reduced (
                    yesdaw::ui::UiTheme::Layout::panelOutlineInset),
                yesdaw::ui::UiTheme::Radius::md,
                yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
            return true;
        };

        auto stats = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                         .withHeight (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionHeight);
        if (inspectorContent.contains (stats))
        {
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
        }

        auto gain = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorGainSectionTop)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainSectionHeight);
        if (drawInspectorSectionCard (gain))
        {
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
        }

        auto fades = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionTop)
                         .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionHeight);
        if (drawInspectorSectionCard (fades))
        {
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
            // E24: the Curve row paints only its label — the overlaid combo IS the value display
            // (the old painted "Equal power" collided with the combo's own text).
            for (const auto& label : { juce::String ("Fade In     ") + juce::String (fadeInSeconds, 3) + " s",
                                       juce::String ("Fade Out    ") + juce::String (fadeOutSeconds, 3) + " s",
                                       juce::String ("Curve") })
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
        }

        // V7: the old CLIP FX card was a hardcoded "None" stub over a model that does not exist
        // (engine::Clip has no FX chain) — removed per D3, like V2's dead KEY cell; the REAL
        // track FX chain lists on the TRACK tab. Its card now shows the clip's FADE CURVE,
        // sampling the SAME clipFadeCurvePoints law the timeline clip body paints with (V6), so
        // the two displays can never disagree about a fade's shape.
        auto fadeChart = inspectorFadeChartCardBounds();
        if (drawInspectorSectionCard (fadeChart))
        {
            drawSmallLabel (g, "FADE CURVE",
                            fadeChart.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
            const auto chart = inspectorFadeChartBounds();
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (chart.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            for (const bool fadeOutRegion : { false, true })
            {
                const std::vector<juce::Point<float>> points = yesdaw::ui::clipFadeCurvePoints (
                    chart.toFloat().reduced (yesdaw::ui::UiTheme::Layout::panelOutlineInset),
                    static_cast<long long> (selectedClip->timelineLength),
                    static_cast<long long> (selectedClip->fadeIn),
                    static_cast<long long> (selectedClip->fadeOut),
                    fadeOutRegion);
                if (points.size() < 2u)
                    continue;
                juce::Path curve;
                curve.startNewSubPath (points.front());
                for (std::size_t i = 1; i < points.size(); ++i)
                    curve.lineTo (points[i]);
                g.setColour (kPurple);
                g.strokePath (curve,
                              juce::PathStrokeType (
                                  yesdaw::ui::UiTheme::Layout::timelineCanvasFadeCurveStrokeWidth));
            }
        }

        // E33: the TAKES section replaced the old placeholder that ALWAYS said "No automation"
        // — the interactive chooser + delete button overlay this card; the painted text only
        // covers the honest empty case.
        auto takes = area.withTrimmedTop (
            yesdaw::ui::UiTheme::Layout::inspectorAutomationSectionTop);
        if (! drawInspectorSectionCard (takes))
            return;
        drawSmallLabel (
            g,
            "TAKES",
            takes.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        if (inspectorTakeViews.empty())
        {
            auto chart = takes.withTrimmedTop (
                                  yesdaw::ui::UiTheme::Layout::inspectorAutomationChartTop)
                             .withHeight (
                                 yesdaw::ui::UiTheme::Layout::inspectorAutomationChartHeight)
                             .reduced (
                                 yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetX,
                                 yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetY);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (chart.toFloat(), yesdaw::ui::UiTheme::Radius::md);
            drawSmallLabel (g, "No takes", chart, juce::Justification::centred);
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
        // V3: a real, honest heading for the column instead of blank fill. The reference's
        // SENDS/RACKS/VIEW/OPTIONS are literal view-switching tabs (Logic-style alternate control
        // sets); this column has no alternate views to switch between — it is ONE unified control
        // set — so painting fake tabs that switch nothing would be dishonest interactivity (D3).
        // "MIXER" honestly names what the column holds, in the SAME band `mixerUtilityTop`
        // already reserves above the first control row (no layout math changed, no risk to the
        // dense existing row-visibility cascade below).
        drawSmallLabel (g,
                        "MIXER",
                        leftTools.withHeight (yesdaw::ui::UiTheme::Layout::mixerUtilityTop)
                            .reduced (yesdaw::ui::UiTheme::Layout::mixerUtilityInsetX,
                                      yesdaw::ui::UiTheme::Space::none),
                        juce::Justification::centredLeft);

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
            // N7: a Track strip's own persisted colour overrides the historical index-cycled
            // palette; a Bus strip (which carries no colour field) always keeps it.
            const juce::Colour stripColour = state.colour != yesdaw::engine::kTrackColourUnset
                                                  ? juce::Colour (state.colour)
                                                  : stripColourForIndex (stripIndex);
            // E23: the selected highlight keys on the E16 strip ordinal, so a selected BUS
            // strip highlights exactly like a selected track.
            const int selectedOrdinal = appModel.selectedMixerStripOrdinal();
            const bool selected = appModel.context().mixerTargetSelected
                               && selectedOrdinal >= 0
                               && stripIndex == static_cast<std::size_t> (selectedOrdinal);
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

            // N1: EVERY strip paints its Mute/Solo cells — the selected strip used to skip them
            // and show two mis-typed ToggleButtons instead, so the strip you were working on was
            // the one whose controls looked broken. The selected strip's live buttons now sit on
            // exactly these rects (paintedMuteSoloCellBoundsForLane), so the two agree by law.
            {
                const std::array<const char*, kMixerPaintedMuteSoloCellCount> cellLabels { "S", "M" };
                for (std::size_t cellIndex = 0; cellIndex < cellLabels.size(); ++cellIndex)
                {
                    const auto cell = paintedMuteSoloCellBoundsForLane (lane, cellIndex);
                    if (cell.isEmpty())
                        continue;

                    g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                    g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::md);
                    const bool on = cellIndex == 0 ? state.soloed : state.muted;
                    g.setColour (on ? stripColour.brighter (0.55f) : kText);
                    g.drawText (cellLabels[cellIndex], cell, juce::Justification::centred, false);
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

            // M4: the strip's FX chain, ON the strip. One row per slot: the insert's name, a
            // bypass dot when it is disabled, and an empty well when the chain is shorter. The
            // selected slot of the selected strip reads as selected — clicking a row opens exactly
            // these params in the panel (shared law: paintedInsertRowBoundsForLane).
            {
                const std::vector<yesdaw::ui::UiMixerFxSlotReadout>& chain = state.fxSlots;
                for (std::size_t slot = 0;
                     slot < static_cast<std::size_t> (paintedInsertRowCountForLane (lane));
                     ++slot)
                {
                    const auto row = paintedInsertRowBoundsForLane (lane, slot);
                    const bool filled = slot < chain.size();
                    const bool slotSelected = selected && selectedFxParamSlot >= 0
                                           && static_cast<std::size_t> (selectedFxParamSlot) == slot;
                    g.setColour (filled ? yesdaw::ui::UiTheme::Color::darkControl()
                                        : yesdaw::ui::UiTheme::Color::controlInset());
                    g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                    // An empty slot is a visible WELL, not a smudge — a mixer strip should read as
                    // "four inserts, none used", the way every DAW draws it.
                    g.setColour (kPanelStroke);
                    g.drawRoundedRectangle (row.toFloat().reduced (
                                                yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                            yesdaw::ui::UiTheme::Radius::sm,
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);
                    if (slotSelected)
                    {
                        g.setColour (kPurple);
                        g.drawRoundedRectangle (row.toFloat().reduced (
                                                    yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                                yesdaw::ui::UiTheme::Radius::sm,
                                                yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);
                    }

                    if (! filled)
                        continue;

                    const yesdaw::ui::UiMixerFxSlotReadout& insert = chain[slot];
                    auto dot = juce::Rectangle<int> (
                        row.getX() + yesdaw::ui::UiTheme::Layout::mixerPaintedInsertBypassDotInset,
                        row.getCentreY() - yesdaw::ui::UiTheme::Layout::mixerPaintedInsertBypassDotSize / 2,
                        yesdaw::ui::UiTheme::Layout::mixerPaintedInsertBypassDotSize,
                        yesdaw::ui::UiTheme::Layout::mixerPaintedInsertBypassDotSize);
                    g.setColour (insert.enabled ? yesdaw::ui::UiTheme::Color::accentTeal()
                                                : yesdaw::ui::UiTheme::Color::mutedText());
                    g.fillEllipse (dot.toFloat());

                    g.setColour (insert.enabled ? kText : kMutedText);
                    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
                    g.drawFittedText (
                        juce::String (fxKindStripName (insert.kind)),
                        row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedInsertLabelInsetX),
                        juce::Justification::centredLeft,
                        1);
                }
            }

            // M5: the strip's sends, ON the strip: destination bus, pre/post tap, and a level bar
            // you can drag. Empty rows are wells, exactly like the insert slots above.
            {
                const std::vector<yesdaw::ui::UiMixerSendReadout>& sends = state.sends;
                for (std::size_t sendIndex = 0;
                     sendIndex < static_cast<std::size_t> (paintedSendRowCountForLane (lane));
                     ++sendIndex)
                {
                    const auto row = paintedSendRowBoundsForLane (lane, sendIndex);
                    const bool routed = sendIndex < sends.size();
                    g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                    g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                    g.setColour (kPanelStroke);
                    g.drawRoundedRectangle (row.toFloat().reduced (
                                                yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                            yesdaw::ui::UiTheme::Radius::sm,
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);
                    if (! routed)
                        continue;

                    const yesdaw::ui::UiMixerSendReadout& send = sends[sendIndex];
                    // The level paints as a filled bar across the row — the drag law reads the same
                    // rect, so what you see is what you set.
                    auto levelBar = row.reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX,
                                                 yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX);
                    const bool previewing = paintedSendDragPreview.stripIndex == static_cast<int> (stripIndex)
                                         && paintedSendDragPreview.sendIndex == static_cast<int> (sendIndex);
                    const double paintedLevel = previewing
                        ? static_cast<double> (paintedSendDragPreview.level)
                        : static_cast<double> (send.linearGain);
                    levelBar = levelBar.withWidth (juce::roundToInt (
                        static_cast<double> (levelBar.getWidth()) * std::clamp (paintedLevel, 0.0, 1.0)));
                    g.setColour (stripColour.withAlpha (yesdaw::ui::UiTheme::Tone::mixerHeaderAlpha));
                    g.fillRoundedRectangle (levelBar.toFloat(), yesdaw::ui::UiTheme::Radius::sm);

                    auto tapCell = row.withTrimmedLeft (
                        juce::jmax (yesdaw::ui::UiTheme::Space::none,
                                    row.getWidth() - yesdaw::ui::UiTheme::Layout::mixerPaintedSendTapWidth));
                    g.setColour (kMutedText);
                    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
                    g.drawText (send.preFader ? "PRE" : "PST", tapCell, juce::Justification::centred, false);

                    g.setColour (kText);
                    g.drawFittedText (
                        juce::String (send.busName.empty() ? std::string ("Bus") : send.busName),
                        row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX)
                           .withTrimmedRight (yesdaw::ui::UiTheme::Layout::mixerPaintedSendTapWidth),
                        juce::Justification::centredLeft,
                        1);
                }
            }

            // M6: the rail and the meter each derive from the shared lane laws now — there is no
            // local fader rect left to keep in sync.
            const auto meter = paintedMeterBoundsForLane (lane);
            if (! isBus && stripIndex < trackMeterHold.size())
            {
                const MeterHoldState& hold = trackMeterHold[stripIndex];
                drawMeterWithHold (g, meter, hold.livePeak, hold.heldPeak, hold.clipLatched);
            }
            else if (isBus && stripIndex - surface.tracks.size() < busMeterHold.size())
            {
                // E22: bus meters live — same held-peak/clip-latch painting as tracks.
                const MeterHoldState& hold = busMeterHold[stripIndex - surface.tracks.size()];
                drawMeterWithHold (g, meter, hold.livePeak, hold.heldPeak, hold.clipLatched);
            }
            else
            {
                drawMeter (g, meter, state.meter.valid ? state.meter.peakLeft : 0.0f);
            }

            auto rail = paintedFaderRailForLane (lane);
            if (! interactiveStrip)
            {
                g.setColour (yesdaw::ui::UiTheme::Color::controlInsetDeep());
                g.fillRoundedRectangle (rail.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                g.setColour (yesdaw::ui::UiTheme::Color::faintText());
                for (const float markDb : yesdaw::ui::UiTheme::Layout::mixerPaintedScaleDbMarks)
                {
                    const float tickY = static_cast<float> (rail.getBottom())
                                      - mixerFaderFractionForDb (markDb) * static_cast<float> (rail.getHeight());
                    g.drawHorizontalLine (
                        juce::roundToInt (tickY),
                        static_cast<float> (rail.getX()
                                            - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickGap
                                            - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickWidth),
                        static_cast<float> (rail.getX()
                                            - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickGap));
                }

                // Unity reads at a glance: a wider, brighter mark straight across the rail.
                {
                    const float unityY = static_cast<float> (rail.getBottom())
                                       - mixerFaderFractionForDb (0.0f) * static_cast<float> (rail.getHeight());
                    g.setColour (kMutedText);
                    g.fillRect (juce::Rectangle<float> (
                        static_cast<float> (rail.getX() - yesdaw::ui::UiTheme::Layout::mixerPaintedUnityMarkOverhang),
                        unityY - yesdaw::ui::UiTheme::Layout::mixerPaintedUnityMarkThickness * 0.5f,
                        static_cast<float> (rail.getWidth() + 2 * yesdaw::ui::UiTheme::Layout::mixerPaintedUnityMarkOverhang),
                        yesdaw::ui::UiTheme::Layout::mixerPaintedUnityMarkThickness));
                }

                const int thumbY =
                    mixerFaderThumbYForGain (rail, state.linearGain)
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
            // M6: a bare "0.0" is not a level. Silence reads as -inf, everything else carries dB.
            g.drawText (state.linearGain <= yesdaw::ui::UiTheme::Mixer::paintedReadoutGainFloor
                            ? juce::String ("-inf dB")
                            : juce::String (gainDb, 1) + " dB",
                        readout,
                        juce::Justification::centred,
                        false);
        }

        // N3: master's lane comes from the SAME single law as every track/bus strip
        // (paintedMixerLaneBounds via paintedMixerMasterBounds) — it is always the next
        // contiguous slot after the last strip, so it can never drift into a detached island.
        auto masterLane = paintedMixerMasterBounds();
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

            yesdaw::ui::UiPianoRollSurfaceSnapshot surface = yesdaw::ui::projectUiPianoRollSurface (
                appModel.project(),
                midiClipId,
                appModel.selectedMidiNoteId(),
                appModel.selectedMidiNoteIds());

            // Piano-roll viewport (E10): the surface publishes the CLAMPED view so every paint,
            // hit-test, and gesture consumer shares one law.
            pianoRollViewLowKey = std::clamp (
                pianoRollViewLowKey,
                yesdaw::ui::UiThemeLayout::pianoRollKeyMin,
                yesdaw::ui::UiThemeLayout::pianoRollKeyMax
                    - (yesdaw::ui::UiTheme::Layout::pianoRollKeyCount - 1));
            pianoRollViewZoom = std::clamp (pianoRollViewZoom,
                                            yesdaw::ui::UiThemeLayout::pianoRollZoomMin,
                                            yesdaw::ui::UiThemeLayout::pianoRollZoomMax);
            surface.viewLowKey = pianoRollViewLowKey;
            surface.viewZoom = pianoRollViewZoom;
            const yesdaw::engine::Tick length = juce::jmax<yesdaw::engine::Tick> (1, surface.timelineLength);
            const yesdaw::engine::Tick visible = juce::jmax<yesdaw::engine::Tick> (
                1, static_cast<yesdaw::engine::Tick> (
                       std::llround (static_cast<double> (length) / pianoRollViewZoom)));
            pianoRollViewScrollTicks = std::clamp<yesdaw::engine::Tick> (
                pianoRollViewScrollTicks, 0, juce::jmax<yesdaw::engine::Tick> (0, length - visible));
            surface.viewScrollTicks = pianoRollViewScrollTicks;
            // E12: note gestures snap through the real chooser.
            surface.snapEnabled = appModel.context().snapEnabled;
            surface.snapGridTicks = static_cast<yesdaw::engine::Tick> (appModel.context().snapGridTicks);
            return surface;
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
    // E29: input device chooser + recorded-channel pick.
    juce::ComboBox audioInputDeviceChooser;
    std::vector<std::string> audioInputDeviceChooserNames;
    juce::ComboBox recordingInputChannelChooser;
    std::uint32_t recordingChannelChooserGeneration = 0xFFFFFFFFu;
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
    std::vector<yesdaw::ui::TimelineClipNote> timelineClipNotes;   // M7: MIDI clip note previews
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
    // Piano-roll viewport (E10): transient view state; the surface builder is the clamp
    // authority (mutable because paint-side snapshots re-clamp against the current clip).
    mutable int pianoRollViewLowKey = yesdaw::ui::UiThemeLayout::pianoRollDefaultLowKey;
    mutable double pianoRollViewZoom = 1.0;
    mutable yesdaw::engine::Tick pianoRollViewScrollTicks = 0;
    TimelineInputComponent timelineInput;
    PlayheadLayerComponent playheadLayer;   // G0.4: above the buffered canvas
    PianoRollInputComponent pianoRollInput;
    TrackListInputComponent trackListInput;
    MixerStripsInputComponent mixerStripsInput;
    FineDragSlider headerTempoControl;
    juce::ComboBox headerMeterChooser;
    juce::ComboBox mixerFxAddChooser;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerFxVisibleSlotCount> mixerFxSlotToggles;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerFxVisibleSlotCount> mixerFxSlotRemoves;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerFxVisibleSlotCount> mixerFxSlotEdits;
    // E14: per-slot chain reorder — the first UI callers of the engine's ReorderFxInsert verb.
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerFxVisibleSlotCount> mixerFxSlotUps;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerFxVisibleSlotCount> mixerFxSlotDowns;
    std::array<FineDragSlider, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamSliders;
    std::array<juce::Label, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamLabels;
    std::array<std::uint32_t, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamSliderIds {};
    // E15: choice-shaped params render as real choosers; big param lists page through the pager.
    std::array<juce::ComboBox, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamChoosers;
    juce::ComboBox mixerFxParamPageChooser;
    int selectedFxParamPage = 0;
    bool lastFxParamPagerVisible = false;
    int selectedFxParamSlot = -1;
    bool refreshingFxParamControls = false;
    // E19: the interactive, undoable master fader on the master pane.
    FineDragSlider mixerMasterFader;
    juce::TextButton mixerBusAddButton;
    // E17: bus rename + remove
    juce::TextButton mixerBusRemoveButton;
    juce::TextEditor busRenameEditor;
    int busRenameIndex = -1;
    juce::ComboBox mixerSendAddChooser;
    juce::ComboBox mixerTrackOutputChooser;   // M3: track main-output routing

    // M5: transient painted-send drag preview (strip, send, level). Nothing persists until the
    // release commits, so a drag is exactly one undo step.
    struct PaintedSendDragPreview
    {
        int stripIndex = -1;
        int sendIndex = -1;
        float level = 0.0f;
    };
    PaintedSendDragPreview paintedSendDragPreview;
    std::array<FineDragSlider, yesdaw::ui::UiTheme::Layout::mixerSendVisibleRowCount> mixerSendLevelSliders;
    std::array<juce::Label, yesdaw::ui::UiTheme::Layout::mixerSendVisibleRowCount> mixerSendLabels;
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerSendVisibleRowCount> mixerSendRemoves;
    // E18: per-row send tap toggles + destination choosers
    std::array<juce::TextButton, yesdaw::ui::UiTheme::Layout::mixerSendVisibleRowCount> mixerSendTaps;
    std::array<juce::ComboBox, yesdaw::ui::UiTheme::Layout::mixerSendVisibleRowCount> mixerSendDestinations;
    bool refreshingSendControls = false;
    std::size_t lastVisibleFxParamRows = 0;
    std::size_t lastVisibleSendRows = 0;
    std::size_t lastVisibleFxSlotRows = 0;
    juce::TextButton trackAddButton;
    juce::TextEditor trackRenameEditor;
    juce::TextEditor clipRenameEditor;
    juce::TextEditor markerRenameEditor;
    int markerRenameIndex = -1;
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
    std::vector<std::array<MeterHoldState, 2>> trackMeterHoldLR;   // V5: rail L/R columns
    std::vector<MeterHoldState> busMeterHold;     // by Bus index; same tick law (E22)
    juce::String lastPushedWindowTitle;           // dirty-title push dedupe (B38)
    juce::TextButton mixerMetersReadout;
    juce::TextButton mixerSendsReadout;
    juce::TextButton mixerSendLevelEdit;
    juce::TextButton mixerFxSlotsReadout;
    juce::TextButton mixerGainReductionReadout;
    juce::TextButton mixerBusFxSlotsReadout;
    juce::TextButton mixerFxSlotToggle;
    // N1: TextButtons, not ToggleButtons — the strip's Mute/Solo are labelled cells, and a
    // ToggleButton ignores the TextButton colour ids this code configures and paints a check box.
    juce::TextButton mixerMute;
    juce::TextButton mixerSolo;
    juce::TextButton mixerSoloSafe;   // R10
    juce::TextButton masterLoudnessReadout;
    juce::TextButton autosaveRestoreButton;
    juce::TextButton autosaveDiscardButton;
    juce::ComboBox timelineSnapChooser;
    juce::ComboBox nudgeValueChooser;      // G1.4
    juce::TextButton inspectorToggle;      // G1.4
    int timeDisplayMode = 0;               // G1.4: 0 bars|beats primary, 1 min:sec primary
    juce::ComboBox timelineRepeatPasteChooser;
    AutomationLaneCanvasComponent automationLaneCanvas;
    // E20: the automation lane target — what the canvas edits (struct declared with the
    // target helpers earlier in the class).
    std::vector<AutomationTargetOption> automationTargetOptions;
    int selectedAutomationTargetIndex = 0;
    bool refreshingAutomationTarget = false;
    juce::ComboBox automationTargetChooser;
    juce::TextButton automationLaneToggle;
    juce::TextButton mixerDockToggle;
    // V7: the inspector's REAL tab buttons (the painted CLIP/TRACK cells used to be decorative).
    juce::TextButton inspectorClipTab;
    juce::TextButton inspectorTrackTab;
    // V8: the toolbar zoom cluster — stepper buttons bound to the EXISTING zoom actions around
    // a live readout of the one shared timelineZoomFactor.
    juce::TextButton timelineZoomOutButton;
    juce::TextButton timelineZoomInButton;
    juce::Label timelineZoomReadout;
    juce::Label statusLine;
    // R4: audioDeviceError fires on the device thread — it may only flip this flag; the UI
    // timer promotes it to a status message on the message thread.
    std::atomic<bool> deviceErrorPending { false };
    juce::Label automationLaneRow;
    // N5: the client-side Touch/Latch ride buffer — see beginAutomationTouchRideIfArmed().
    juce::ComboBox automationModeChooser;
    bool automationTouchRideActive = false;
    yesdaw::engine::AutomationTargetRole automationTouchRideRole =
        yesdaw::engine::AutomationTargetRole::TrackFader;
    std::uint32_t automationTouchRideParamId = 0;
    yesdaw::engine::EntityId automationTouchRideTrackId;
    std::vector<yesdaw::ui::UiAppModel::AutomationTouchSample> automationTouchRideSamples;
    juce::TextButton automationBreakpointAddButton;
    juce::TextButton automationBreakpointDeleteButton;
    // E26: whether the lane controls were last laid out with the band reserved.
    bool automationLaneLaidOutVisible = false;
    // E33: the inspector take stack — chooser + delete over the TAKES section.
    juce::ComboBox inspectorTakeChooser;
    juce::TextButton inspectorTakeDelete;
    std::vector<yesdaw::ui::UiClipTakeView> inspectorTakeViews;
    // E34: open MIDI inputs + the message-thread note-on pairing map (note -> frame, velocity).
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
    std::map<int, std::pair<std::int64_t, float>> pendingMidiNoteOns;
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
    bool refreshingNudgeChooser = false;   // G1.4
    bool refreshingRepeatPasteChooser = false;
    bool refreshingMixerControls = false;
    int autosaveElapsedMs = 0;

    // G0.2: the top-level component this shell is registered on as a KeyListener (null in the
    // headless harness, where the shell is its own top level).
    juce::Component* routedTopLevel = nullptr;

    // G0.1 State probe (ADR-0046 §10; plan §7.2). Debug-only: `stateProbePath` is empty in a
    // normal launch and nothing below is ever written. Counters are the feel-budget inputs.
    std::filesystem::path stateProbePath;
    std::uint64_t probeTick = 0;
    std::chrono::steady_clock::time_point launchStamp {};
    std::uint64_t audioCallbackAdds = 0;
    std::uint64_t audioCallbackRemovals = 0;
    std::uint64_t audioSuspendRequests = 0;   // G0.3
    // G0.4: invalidation and refresh counters, and the context the last refresh ran against.
    std::uint64_t fullInvalidations = 0;
    std::uint64_t dynamicInvalidations = 0;
    std::uint64_t actionStateRefreshes = 0;
    yesdaw::ui::UiActionContext lastRefreshedContext {};
    bool lastRefreshedContextValid = false;
    std::atomic<double> deviceSampleRateHz { 0.0 };
    std::atomic<int> deviceXRunBaseline { -1 };
    std::atomic<std::uint32_t> deviceDeadlineMisses { 0u };
    std::atomic<std::uint64_t> deviceMaxCallbackNs { 0u };
    std::string lastActionStableId;
    std::chrono::steady_clock::time_point pendingActionStamp {};
    bool actionStampPending = false;
    double lastActionToPaintMs = -1.0;
    std::chrono::steady_clock::time_point paintStartStamp {};
    double lastPaintMs = 0.0;
    std::array<double, kStateProbePaintRingSize> paintRing {};
    std::size_t paintRingIndex = 0;
    std::size_t paintRingCount = 0;
    std::uint64_t paintCount = 0;
    double lastTickMs = 0.0;

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
    return createNativeMainComponent ({});
}

std::unique_ptr<juce::Component> createNativeMainComponent (std::filesystem::path openBundleAtLaunch)
{
    yesdaw::ui::MainComponentFileChoices choices = makeNativeFileChoices();
    choices.openBundleAtLaunch = std::move (openBundleAtLaunch);

    // G0.1: the Session drive's launch-time seams. Both are absolute paths; anything else is
    // ignored so a stray variable can never point the shell at a relative location.
    const juce::String probe = juce::SystemStats::getEnvironmentVariable ("YESDAW_STATE_PROBE", {});
    if (probe.isNotEmpty() && juce::File::isAbsolutePath (probe))
        choices.stateProbePath = pathFromJuceFile (juce::File (probe));

    const juce::String sessionDir =
        juce::SystemStats::getEnvironmentVariable ("YESDAW_SESSION_STATE_DIR", {});
    if (sessionDir.isNotEmpty() && juce::File::isAbsolutePath (sessionDir))
        choices.sessionStateDirectory = pathFromJuceFile (juce::File (sessionDir));

    return std::make_unique<MainComponent> (std::move (choices), true);
}

std::unique_ptr<juce::Component> createMainComponent (MainComponentFileChoices fileChoices)
{
    return std::make_unique<MainComponent> (std::move (fileChoices), false);
}

std::string mainComponentStateProbeJson (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->buildStateProbeJson().toStdString();

    return {};
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
        snapshot.playbackReplaceCount = mainComponent->harnessPlaybackReplaceCount();
        snapshot.playbackLiveScalarsApplied = mainComponent->harnessPlaybackLiveScalarsApplied();
        snapshot.playbackLoopStartFrame = mainComponent->harnessPlaybackLoopStartFrame();
        snapshot.playbackLoopEndFrame = mainComponent->harnessPlaybackLoopEndFrame();
        snapshot.statusLineText = mainComponent->harnessStatusLineText();
        snapshot.statusLineIsError = mainComponent->harnessStatusLineIsError();
        snapshot.timelineRangeStartFrame = mainComponent->harnessTimelineRangeStartFrame();
        snapshot.timelineRangeEndFrame = mainComponent->harnessTimelineRangeEndFrame();
        snapshot.timelineZoomFactor = mainComponent->harnessTimelineZoomFactor();
        snapshot.timelineScrollSeconds = mainComponent->harnessTimelineScrollSeconds();
        snapshot.timelineTrackScrollRows = mainComponent->harnessTimelineTrackScrollRows();
        snapshot.timelineMaxTrackScrollRows = mainComponent->harnessTimelineMaxTrackScrollRows();
        snapshot.pianoRollViewLowKey = mainComponent->harnessPianoRollViewLowKey();
        snapshot.pianoRollViewZoom = mainComponent->harnessPianoRollViewZoom();
        snapshot.pianoRollViewScrollTicks = mainComponent->harnessPianoRollViewScrollTicks();
        snapshot.visibleTimelineTrackCount = mainComponent->harnessVisibleTimelineTrackCount();
        snapshot.visibleTimelineClipCount = mainComponent->harnessVisibleTimelineClipCount();
        snapshot.visibleFirstTimelineClipName = mainComponent->harnessVisibleFirstTimelineClipName();
        snapshot.selectedTimelineClipCount = mainComponent->harnessSelectedTimelineClipCount();
        snapshot.visibleTimelineTotalSeconds = mainComponent->harnessVisibleTimelineTotalSeconds();
        snapshot.visibleMixerTrackCount = mainComponent->harnessVisibleMixerTrackCount();
        snapshot.visibleMixerBusCount = mainComponent->harnessVisibleMixerBusCount();
        snapshot.selectedMixerStripOrdinal = mainComponent->harnessSelectedMixerStripOrdinal();
        snapshot.visibleMixerLoudnessValid = mainComponent->harnessVisibleMixerLoudnessValid();
        snapshot.visibleMasterPeakLeft = mainComponent->harnessVisibleMasterPeakLeft();
        snapshot.visibleMasterPeakRight = mainComponent->harnessVisibleMasterPeakRight();
        snapshot.visiblePianoRollNoteCount = mainComponent->harnessVisiblePianoRollNoteCount();
        snapshot.bundlePath = mainComponent->harnessBundlePath();
        snapshot.context = mainComponent->harnessContext();
        snapshot.recordingDevice = mainComponent->harnessRecordingDevice();
        snapshot.recordingTrackInput = mainComponent->harnessRecordingTrackInput();
        snapshot.armedRecordingTrackInputs = mainComponent->harnessArmedRecordingTrackInputs();
        snapshot.liveInputMeterPeak = mainComponent->harnessInputMeterPeak();
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

juce::Rectangle<int> mainComponentPaintedMuteSoloCellBounds (const juce::Component& component,
                                                              int stripIndex,
                                                              int cellIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedMuteSoloCellBounds (stripIndex, cellIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedInsertSlotBounds (const juce::Component& component,
                                                            int stripIndex,
                                                            int slotIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedInsertSlotBounds (stripIndex, slotIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedSendRowBounds (const juce::Component& component,
                                                         int stripIndex,
                                                         int sendIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedSendRowBounds (stripIndex, sendIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedFaderRailBounds (const juce::Component& component, int stripIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedFaderRailBounds (stripIndex);

    return {};
}

int mainComponentPaintedFaderThumbY (const juce::Component& component, int stripIndex, float linearGain)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedFaderThumbY (stripIndex, linearGain);

    return 0;
}

juce::Rectangle<int> mainComponentPaintedMixerStripBounds (const juce::Component& component, int stripIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedMixerStripBounds (stripIndex);

    return {};
}

juce::Rectangle<int> mainComponentPaintedMixerMasterBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedMixerMasterBounds();

    return {};
}

juce::Rectangle<int> mainComponentHeaderSectionBounds (const juce::Component& component, int section)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
    {
        const MainComponent::HeaderLayout h = mainComponent->headerLayout();
        switch (section)
        {
            case 0: return h.toolsSection;
            case 1: return h.transportSection;
            case 2: return h.masterSection;
            default: return {};
        }
    }
    return {};
}

std::vector<juce::Rectangle<int>> mainComponentHeaderRects (const juce::Component& component)
{
    std::vector<juce::Rectangle<int>> rects;
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
    {
        const MainComponent::HeaderLayout h = mainComponent->headerLayout();
        for (const juce::Rectangle<int>& r : { h.menuBar, h.newButton, h.openButton, h.saveButton, h.importButton,
                                               h.undoButton, h.redoButton, h.exportButton, h.locateStart, h.play,
                                               h.stop, h.record, h.timeReadout, h.tempoMeterBox, h.loop,
                                               h.masterCard, h.gear, h.bitDepth, h.range, h.outputDevice,
                                               h.inputDevice, h.inputChannel, h.arm, h.monitor, h.comp })
            if (! r.isEmpty())
                rects.push_back (r);
    }
    return rects;
}

juce::Rectangle<int> mainComponentHeaderTimeReadoutBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->headerLayout().timeReadout;
    return {};
}

int mainComponentHeaderHeight (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->headerHeightNow();
    return 0;
}

yesdaw::ui::MainComponentContextMenu mainComponentRequestContextMenu (juce::Component& component, juce::Point<int> shellPoint)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRequestContextMenu (shellPoint);
    return {};
}

void mainComponentInvokeContextMenuItem (juce::Component& component, yesdaw::ui::UiActionId action, int direction)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInvokeContextMenuItem (action, direction);
}

void mainComponentDispatchAction (juce::Component& component, yesdaw::ui::UiActionId action)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessDispatchAction (action);
}

yesdaw::ui::UiActionState mainComponentActionState (const juce::Component& component, yesdaw::ui::UiActionId action)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessActionState (action);
    return { false, "not a MainComponent" };
}

void mainComponentSetSettingsRowVisible (juce::Component& component, bool visible)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessSetSettingsRowVisible (visible);
}

void mainComponentRevealSettingsRowFor (juce::Component& component, yesdaw::ui::UiActionId action)
{
    if (isSettingsRowAction (action))
        mainComponentSetSettingsRowVisible (component, true);
}

juce::Rectangle<int> mainComponentMixerPanelBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessMixerPanelBounds();

    return {};
}

juce::Rectangle<int> mainComponentTimelineBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessTimelineBounds();

    return {};
}

juce::Rectangle<int> mainComponentHeaderMasterCardBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->headerMasterCardBounds();

    return {};
}

juce::Rectangle<int> mainComponentPaintedRailRowBounds (const juce::Component& component, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedRailRowBounds (row);

    return {};
}

juce::Rectangle<int> mainComponentPaintedColourSwatchBounds (const juce::Component& component, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedColourSwatchBounds (row);

    return {};
}

juce::Colour mainComponentTimelineClipColour (juce::Component& component, yesdaw::engine::EntityId clipId)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessTimelineClipColour (clipId);

    return {};
}

yesdaw::engine::BarBeat mainComponentHeaderBarBeat (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessHeaderBarBeat();

    return {};
}

std::vector<yesdaw::ui::RulerBarLabel> mainComponentRulerBarLabels (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRulerBarLabels();

    return {};
}

double mainComponentRulerSecondsAtX (juce::Component& component, int x)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRulerSecondsAtX (x);

    return 0.0;
}

juce::Rectangle<int> mainComponentInspectorFadeChartBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessInspectorFadeChartBounds();

    return {};
}

std::pair<float, float> mainComponentRailMeterChannelPeaks (const juce::Component& component, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessRailMeterChannelPeaks (row);

    return { 0.0f, 0.0f };
}

juce::Rectangle<int> mainComponentRailVolumeSliderBounds (const juce::Component& component, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessRailVolumeSliderBounds (row);

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

bool processMainComponentDeviceAudioBlock (juce::Component& component,
                                           const float* const* inputChannels,
                                           int numInputChannels,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessProcessDeviceAudioBlock (
            inputChannels, numInputChannels, outputChannels, numOutputChannels, numFrames);

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
