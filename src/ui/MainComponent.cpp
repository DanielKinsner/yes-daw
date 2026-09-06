// YES DAW - H11 app shell.
//
// The visible JUCE shell and the headless tests share ui/UiActions.h. This checkpoint keeps the shell
// image-light and model-backed: later H11 slices wire Project loading, transport, timeline drawing,
// accessibility traversal, and editing through the same action IDs.

#include "ui/MainComponent.h"
#include "ui/UiIcons.h"
#include "ui/Splitters.h"
#include "engine/Time.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiAppModel.h"
#include "ui/UiMixerSurface.h"
#include "ui/UiPianoRollSurface.h"
#include "ui/UiTheme.h"
#include "ui/YesDawLookAndFeel.h"
#include "ui/TimelineInputComponent.h"
#include "ui/PianoRollInputComponent.h"
#include "ui/ShellWidgets.h"
#include "ui/TrackListInputComponent.h"
#include "ui/AutomationLaneCanvasComponent.h"
#include "ui/MixerStripsInputComponent.h"
#include "ui/InstrumentPanelComponent.h"
#include "ui/UndoHistoryComponent.h"
#include "ui/KeymapEditorComponent.h"
#include "ui/FxEditorComponent.h"

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
#include <set>
#include <span>
#include <string>
#include <utility>
#include <map>
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
constexpr int kUiRefreshIntervalMs = 33;
// Open Recent menu item ids live above the action-id range (B39).
constexpr int kRecentMenuBaseId = 1000;
constexpr int kRepeatCountMenuBaseId = 1100;   // G1.7: Edit ▸ Repeat Count ▸ (+ the count: 2, 3, 4, 8)

// The menu bar names its menus itself; the tooltip mixin satisfies the every-control law (B40).
class TooltippedMenuBar final : public juce::MenuBarComponent,
                                public juce::SettableTooltipClient
{
public:
    using juce::MenuBarComponent::MenuBarComponent;
};

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
constexpr const char* kInspectorStretchComponentId = "clip.inspector.stretch";   // G2.9b
constexpr const char* kInspectorFadeCurveAmountComponentId = "clip.inspector.fade_curve_amount";   // G2.10
constexpr int kInspectorLinearFadeCurveId = 2;    // G2.10: the chooser's ids, equal power stays 1
constexpr int kInspectorSCurveFadeCurveId = 3;
constexpr int kInspectorLogFadeCurveId = 4;
constexpr const char* kAutomationLaneRowComponentId = "timeline.automation.track.0.lane";
// N1: a mixer strip carries exactly two painted toggle cells — Solo then Mute, left to right.
constexpr std::size_t kMixerPaintedMuteSoloCellCount = 2;
// G4.1: the strip's cell row is S / M / R on a Track strip, S / M on a Bus; the I/O slot rows.
constexpr std::size_t kMixerPaintedTrackCellCount = 3;
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
        case yesdaw::ui::UiActionId::ViewPianoRoll: return "P";   // G2.1 cp3: the plan's letter cluster
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

// Plan §5.1 cp1: the carved helper components, their free functions and shared constants — by name.
using yesdaw::ui::TimelineInputComponent;
using yesdaw::ui::PianoRollCanvasGeometry;
using yesdaw::ui::PianoRollInputComponent;
using yesdaw::ui::pianoRollCanvasGeometry;
using yesdaw::ui::pianoRollTimelineLength;
using yesdaw::ui::pianoRollVisibleTicks;
using yesdaw::ui::pianoRollViewHighKey;
using yesdaw::ui::pianoRollKeyY;
using yesdaw::ui::pianoRollKeyAtY;
using yesdaw::ui::pianoRollTickX;
using yesdaw::ui::pianoRollGridLines;
using yesdaw::ui::pianoRollTickDeltaForPixels;
using yesdaw::ui::pianoRollNoteBounds;
using yesdaw::ui::pianoRollVelocityLaneArea;
using yesdaw::ui::pianoRollVelocityForLaneY;
using yesdaw::ui::pianoRollTickForX;
using yesdaw::ui::pianoRollControlLaneArea;
using yesdaw::ui::pianoRollControlLaneChooserArea;
using yesdaw::ui::pianoRollControlLaneDataArea;
using yesdaw::ui::pianoRollControlValueForLaneY;
using yesdaw::ui::pianoRollControlLaneYForValue;
using yesdaw::ui::pianoRollControlLaneOf;
using yesdaw::ui::kPianoRollSnapGridTicks;
using yesdaw::ui::ToolbarActionButton;
using yesdaw::ui::FineDragSlider;
using yesdaw::ui::PlayheadLayerComponent;
using yesdaw::ui::TrackListInputComponent;
using yesdaw::ui::AutomationLaneCanvasComponent;
using yesdaw::ui::MixerStripsInputComponent;
using yesdaw::ui::kMixerIoInputRow;
using yesdaw::ui::kMixerIoOutputRow;
using yesdaw::ui::InstrumentPanelComponent;
using yesdaw::ui::UndoHistoryComponent;
using yesdaw::ui::KeymapEditorComponent;
using yesdaw::ui::FxEditorComponent;

class MainComponent : public juce::Component,
                      public juce::ScrollBar::Listener,   // G2.16: the real scroll bars
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
        timelineInput.onToolSelected = [this] (yesdaw::ui::TimelineTool tool) {
            handleAction (yesdaw::ui::timelineToolSelectAction (tool));
            refreshActionState();
            repaintAll();
        };
        timelineInput.onZoomToolClicked = [this] (double anchorSeconds, bool zoomOut) {
            const double factor = yesdaw::ui::UiTheme::Layout::timelineZoomToolClickFactor;
            zoomTimelineAtAnchor (anchorSeconds, zoomOut ? 1.0 / factor : factor);
            repaintAll();
        };
        timelineInput.onClipErased = [this] (int layoutClipId) {   // G3.2: select, then the one delete verb
            if (layoutClipId < 0 || static_cast<std::size_t> (layoutClipId) >= timelineClipIds.size())
                return;
            (void) appModel.selectTimelineClipForGesture (timelineClipIds[static_cast<std::size_t> (layoutClipId)], false);
            handleAction (yesdaw::ui::UiActionId::TimelineClipDelete);
            refreshActionState();
            repaintAll();
        };
        timelineInput.onHandToolScrolled = [this] (double secondsDelta) {
            // G2.16: clamped to the view's range like the scroll bar — no transient overshoot.
            const double visible = timelineVisibleSecondsFor (timelineTotalSeconds);
            const double maxScroll = std::max (0.0, timelineTotalSeconds - visible);
            timelineScrollSeconds = std::clamp (timelineScrollSeconds + secondsDelta, 0.0, maxScroll);
            repaintAll();
        };
        timelineInput.onAutoScrolled = [this] (double secondsDelta) {   // G2.3: the hand tool's law
            timelineScrollSeconds = std::max (0.0, timelineScrollSeconds + secondsDelta);
            repaintAll();
        };
        timelineInput.snapSecondsForPreview = [this] (double seconds) {   // G2.3: the release's snap law
            if (const auto tick = timelineTickFromSeconds (seconds))
                if (appModel.project().sampleRate.isValid())
                    return static_cast<double> (snappedTimelineTick (*tick, true)) / appModel.project().sampleRate.hz;
            return seconds;
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
                if (juce::File (file).hasFileExtension ("wav") || juce::File (file).hasFileExtension ("mid;midi"))   // G3.7
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
                // G3.7: a .mid lands on the lane under the pointer as MIDI clips (the model names
                // its own refusals on the status line; further file tracks add lanes below).
                if (juce::File (file).hasFileExtension ("mid;midi"))
                {
                    const int midiLane = std::min (lane + laneOffset, static_cast<int> (appModel.project().tracks.size()) - 1);
                    const yesdaw::engine::EntityId midiTrackId = appModel.project().tracks[static_cast<std::size_t> (midiLane)].id;
                    if (appModel.importMidiFileAt (path, midiTrackId, start).dispatched)
                    {
                        anyImported = true;
                        ++laneOffset;
                    }
                    continue;
                }
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
        const auto hintSink = [this] (const juce::String& hint) { setHoverHint (hint); };
        timelineInput.onHoverHint = hintSink;
        pianoRollInput.onHoverHint = hintSink;
        trackListInput.onHoverHint = hintSink;
        mixerStripsInput.onHoverHint = hintSink;
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
        timelineInput.onClipStretchedRight = [this] (int timelineClipId, double endSeconds, bool snapInvert) {   // G2.9b
            stretchTimelineClipRightByLayoutId (timelineClipId, endSeconds, snapInvert);
        };
        timelineInput.onClipSlipped = [this] (int timelineClipId, double deltaSeconds) {   // G2.11
            slipTimelineClipByLayoutId (timelineClipId, deltaSeconds);
        };
        timelineInput.onClipRenameRequested = [this] (int timelineClipId) {   // G2.12
            if (timelineClipId < 0 || timelineClipId >= static_cast<int> (timelineClipIds.size()))
                return;
            (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (timelineClipId)]);
            refreshActionState();
            openClipRenameEditor();
            repaintAll();
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
        timelineInput.onClipFadeAdjusted = [this] (int timelineClipId, bool fadeIn, double fadeSeconds, double curveDelta) {
            adjustTimelineClipFadeByLayoutId (timelineClipId, fadeIn, fadeSeconds, curveDelta);
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
        timelineInput.onMapLabelClicked = [this] (int mapIndex) {
            if (mapIndex < 0 || mapIndex >= static_cast<int> (timelineMapLabelFrames.size()))
                return;
            (void) appModel.locatePlaybackFrame (timelineMapLabelFrames[static_cast<std::size_t> (mapIndex)]);
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
        trackListInput.rowZoomProvider = [this] { return timelineRowZoom; };   // G2.16
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
        trackListInput.onRowClickedWithModifiers = [this] (int row, juce::ModifierKeys mods) {   // G2.17
            if (mods.isCtrlDown())
                toggleTrackLaneSelection (row);
            else if (mods.isShiftDown())
                extendTrackLaneSelection (row);
            else
                selectTrackLane (row);
        };
        trackListInput.onRowReordered = [this] (int from, int to) {   // G2.17
            const auto& tracks = appModel.project().tracks;
            if (from < 0 || to < 0 || from >= static_cast<int> (tracks.size()) || to >= static_cast<int> (tracks.size()))
                return;
            if (appModel.reorderProjectTrack (tracks[static_cast<std::size_t> (from)].id, static_cast<std::size_t> (to)).dispatched)
                selectTrackLane (to);
            refreshActionState();
            resized();
            repaintAll();
        };
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
        // The "O" badge arms THIS row through the same verb the lane menu's Arm uses (M11: one
        // more member of the arm set, or one fewer). The verb refuses honestly without an input
        // device; the badge simply stays unlit.
        trackListInput.onArmToggled = [this] (int row) {
            selectTrackLane (row);
            if (row >= 0 && row < static_cast<int> (appModel.project().tracks.size()))
                (void) appModel.toggleRecordingArmForTrack (static_cast<std::size_t> (row));
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
        // G2.7: the Snap mode chooser — Grid / Relative / Events / Off (the unit stays in the
        // toolbar's snap chooser; Ctrl inverts during a drag; the G2.3 landing line shows it).
        configureActionComponent (snapModeChooser, yesdaw::ui::UiActionId::TimelineSnapModeGrid, "Snap mode");
        snapModeChooser.setComponentID ("timeline.snap_mode.chooser");
        snapModeChooser.setName ("Snap mode");
        snapModeChooser.setTitle ("Snap mode");
        snapModeChooser.addItem ("Snap: Grid", 1);
        snapModeChooser.addItem ("Snap: Relative", 2);
        snapModeChooser.addItem ("Snap: Events", 3);
        snapModeChooser.addItem ("Snap: Off", 4);
        snapModeChooser.setSelectedId (1, juce::dontSendNotification);
        snapModeChooser.onChange = [this] {
            if (refreshingSnapModeChooser)
                return;
            const int selected = snapModeChooser.getSelectedId();
            handleAction (selected == 2 ? yesdaw::ui::UiActionId::TimelineSnapModeRelative
                          : selected == 3 ? yesdaw::ui::UiActionId::TimelineSnapModeEvents
                          : selected == 4 ? yesdaw::ui::UiActionId::TimelineSnapModeOff
                                          : yesdaw::ui::UiActionId::TimelineSnapModeGrid);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (snapModeChooser);

        // G2.6: the Edit mode chooser — Overlap / No Overlap / Shuffle — one setting the placing
        // and removing verbs consult; the Edit menu carries the same three, ticked.
        configureActionComponent (editModeChooser, yesdaw::ui::UiActionId::EditModeOverlap, "Edit mode");
        editModeChooser.setComponentID ("timeline.edit_mode.chooser");
        editModeChooser.setName ("Edit mode");
        editModeChooser.setTitle ("Edit mode");
        editModeChooser.addItem ("Edit: Overlap", 1);
        editModeChooser.addItem ("Edit: No Overlap", 2);
        editModeChooser.addItem ("Edit: Shuffle", 3);
        editModeChooser.setSelectedId (1, juce::dontSendNotification);
        editModeChooser.onChange = [this] {
            if (refreshingEditModeChooser)
                return;
            const int selected = editModeChooser.getSelectedId();
            handleAction (selected == 2 ? yesdaw::ui::UiActionId::EditModeNoOverlap
                          : selected == 3 ? yesdaw::ui::UiActionId::EditModeShuffle
                                          : yesdaw::ui::UiActionId::EditModeOverlap);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (editModeChooser);

        nudgeValueChooser.setComponentID ("timeline.nudge.chooser");
        nudgeValueChooser.setName ("Nudge value");
        nudgeValueChooser.setTitle ("Nudge value");
        nudgeValueChooser.setTooltip ("Nudge value: the distance Alt+Left / Alt+Right move the selection");
        nudgeValueChooser.addItem ("Nudge: Grid", 1);
        nudgeValueChooser.addItem ("Nudge: Bar", 2);
        nudgeValueChooser.addItem ("Nudge: Beat", 3);
        nudgeValueChooser.addItem ("Nudge: 1/16", 4);
        nudgeValueChooser.addItem ("Nudge: 1 ms", 5);      // G2.8
        nudgeValueChooser.addItem ("Nudge: 10 ms", 6);
        nudgeValueChooser.addItem ("Nudge: 1 Frame", 7);
        nudgeValueChooser.addItem ("Nudge: 1 Sample", 8);
        nudgeValueChooser.setSelectedId (1, juce::dontSendNotification);
        nudgeValueChooser.onChange = [this] {
            if (refreshingNudgeChooser)
                return;
            const int selected = nudgeValueChooser.getSelectedId();
            const yesdaw::ui::UiActionId action =
                selected == 2 ? yesdaw::ui::UiActionId::EditNudgeValueBar
                : selected == 3 ? yesdaw::ui::UiActionId::EditNudgeValueBeat
                : selected == 4 ? yesdaw::ui::UiActionId::EditNudgeValueSixteenth
                : selected == 5 ? yesdaw::ui::UiActionId::EditNudgeValueMs1      // G2.8
                : selected == 6 ? yesdaw::ui::UiActionId::EditNudgeValueMs10
                : selected == 7 ? yesdaw::ui::UiActionId::EditNudgeValueFrame
                : selected == 8 ? yesdaw::ui::UiActionId::EditNudgeValueSample
                                : yesdaw::ui::UiActionId::EditNudgeValueGrid;
            (void) appModel.dispatch (action);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (nudgeValueChooser);

        // G1.4: the Inspector toggle (I) at the toolbar's right end.
        configureActionComponent (inspectorToggle, yesdaw::ui::UiActionId::ViewToggleInspector, "Inspector");
        inspectorToggle.setButtonText ("I");   // G2.1 cp3: the letter cluster (tooltip carries the name + chord)
        inspectorToggle.setClickingTogglesState (false);
        inspectorToggle.onClick = [this] {
            handleAction (yesdaw::ui::UiActionId::ViewToggleInspector);
            refreshActionState();
            resized();
            repaintAll();
        };
        addAndMakeVisible (inspectorToggle);

        // G2.1: three draggable splitters (header | lanes | inspector, and the dock above).
        railSplitter.setComponentID ("shell.splitter.rail");
        railSplitter.setTooltip ("Drag to resize the track headers");
        inspectorSplitter.setTooltip ("Drag to resize the inspector");
        dockSplitter.setTooltip ("Drag to resize the editor dock");
        inspectorSplitter.setComponentID ("shell.splitter.inspector");
        dockSplitter.setComponentID ("shell.splitter.dock");
        railSplitter.onDrag = [this] (juce::Point<int> pointer) { setRailWidth (pointer.x); };
        inspectorSplitter.onDrag = [this] (juce::Point<int> pointer) { setInspectorWidth (getWidth() - pointer.x); };
        dockSplitter.onDrag = [this] (juce::Point<int> pointer) { setDockHeight (getHeight() - pointer.y); };
        for (yesdaw::ui::SplitterComponent* splitter : { &railSplitter, &inspectorSplitter, &dockSplitter })
        {
            splitter->onDragEnd = [this] { saveViewState(); };
            addAndMakeVisible (*splitter);
        }

        // G1.5: the keymap editor — hidden until Alt+K; every seam is the registry / the model.
        keymapEditor.rowsProvider = [this] (const juce::String& filter) {
            std::vector<yesdaw::ui::UiActionId> rows;
            const juce::String needle = filter.trim().toLowerCase();
            for (const auto& descriptor : appModel.registry().actions())
            {
                const juce::String haystack = (juce::String (descriptor.label) + " " + descriptor.stableId + " "
                                               + appModel.registry().keymap().chordFor (descriptor.id) + " "
                                               + yesdaw::ui::focusContextName (yesdaw::ui::defaultFocusContext (descriptor.id))).toLowerCase();
                if (needle.isEmpty() || haystack.contains (needle))
                    rows.push_back (descriptor.id);
            }
            return rows;
        };
        keymapEditor.keymapProvider = [this] () -> const yesdaw::ui::Keymap& { return appModel.registry().keymap(); };
        keymapEditor.onRebind = [this] (yesdaw::ui::UiActionId action, const juce::String& chord) -> juce::String {
            const yesdaw::ui::KeymapRebindStatus status = appModel.rebindChord (action, chord.toStdString());
            refreshActionState();
            repaintAll();
            switch (status)
            {
                case yesdaw::ui::KeymapRebindStatus::Ok:             return "Bound " + chord + " to " + appModel.registry().descriptor (action)->label;
                case yesdaw::ui::KeymapRebindStatus::EmptyChord:     return "Type a chord first";
                case yesdaw::ui::KeymapRebindStatus::DuplicateChord: return juce::String (appModel.statusLineText());
                case yesdaw::ui::KeymapRebindStatus::UnknownAction:  break;
            }
            return "Unknown verb";
        };
        keymapEditor.onUnbind = [this] (yesdaw::ui::UiActionId action) {
            appModel.unbindChord (action);
            refreshActionState();
            repaintAll();
        };
        keymapEditor.onRestoreDefaults = [this] {
            appModel.restoreDefaultKeymap();
            refreshActionState();
            repaintAll();
        };
        keymapEditor.onClose = [this] {
            handleAction (yesdaw::ui::UiActionId::HelpShowKeymap);
            refreshActionState();
            resized();
            repaintAll();
        };
        addChildComponent (keymapEditor);
        // G3.1: the instrument panel — a dock tab fed by the selected Track's slot.
        instrumentPanel.kindProvider = [this] {
            const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument();
            return track != nullptr ? juce::String (instrumentKindName (track->instrumentKind)) : juce::String ("No track");
        };
        instrumentPanel.kindChoicesProvider = [] { return std::vector<juce::String> { "None (auto)", "SimpleSynth", "Sampler" }; };   // G3.9
        // G3.9: the Sampler's pad grid and its verbs.
        instrumentPanel.padsProvider = [this] {
            const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument();
            return track != nullptr && track->instrumentKind == yesdaw::engine::TrackInstrumentKind::Sampler;
        };
        instrumentPanel.padRowsProvider = [this] {
            std::vector<InstrumentPanelComponent::Pad> pads;
            if (const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument())
                for (const yesdaw::engine::SamplerPad& pad : track->samplerPads)
                    pads.push_back ({ static_cast<int> (pad.key), juce::String (std::string (pad.nameView())), pad.oneShot, true });
            return pads;
        };
        instrumentPanel.onPadClicked = [this] (int key, bool shift, bool ctrl) {
            const std::int16_t padKey = static_cast<std::int16_t> (key);
            if (ctrl)
            {
                (void) appModel.clearSamplerPadOnSelectedTrack (padKey);
                recordLastAction (yesdaw::ui::UiActionId::SamplerPadClear);
            }
            else if (shift)
            {
                (void) appModel.toggleSamplerPadModeOnSelectedTrack (padKey);
                recordLastAction (yesdaw::ui::UiActionId::SamplerPadModeToggle);
            }
            else if (fileChoices.chooseSamplerPadFile)
            {
                const std::filesystem::path path = fileChoices.chooseSamplerPadFile();
                if (! path.empty())
                    loadSamplerPadFromPath (padKey, path);
            }
            refreshActionState();
            resized();
            repaintAll();
        };
        instrumentPanel.onPadFilesDropped = [this] (int key, const juce::StringArray& files) {
            for (const juce::String& file : files)
                if (juce::File (file).hasFileExtension ("wav;wave"))
                {
                    loadSamplerPadFromPath (static_cast<std::int16_t> (key), std::filesystem::path (file.toStdString()));
                    break;   // one file, one pad
                }
            refreshActionState();
            resized();
            repaintAll();
        };
        instrumentPanel.kindIndexProvider = [this] {
            const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument();
            return track != nullptr ? static_cast<int> (track->instrumentKind) : -1;
        };
        instrumentPanel.onKindChosen = [this] (int index) {
            (void) appModel.setInstrumentOnSelectedTrack (static_cast<yesdaw::engine::TrackInstrumentKind> (index));
            refreshActionState();
            repaintAll();
        };
        instrumentPanel.rowsProvider = [this] { return instrumentPanelRows(); };
        instrumentPanel.onRowDragStart = [this] (std::uint32_t paramId) {
            appModel.beginStripGesture();   // G3.1 checkpoint (SS-4 found it): one drag = one undo step (E21)
            if (const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument())
                beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::InstrumentParam, paramId, track->id);
        };
        instrumentPanel.onRowDragEnd = [this] {
            endAutomationTouchRideIfActive();
            appModel.endStripGesture();
        };
        instrumentPanel.onRowValue = [this] (std::uint32_t paramId, double normalized) {
            if (automationTouchRideActive)
                recordAutomationTouchSample (normalized);
            else
                (void) appModel.setInstrumentParamOnSelectedTrack (paramId, normalized);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (instrumentPanel);

        // G3.1: the inspector's TRACK tab carries the kind chooser and an Edit button (opens the tab).
        configureActionComponent (inspectorInstrumentChooser, yesdaw::ui::UiActionId::TrackSetInstrument, "Track instrument");
        inspectorInstrumentChooser.setComponentID ("track.inspector.instrument");
        inspectorInstrumentChooser.addItem ("None (auto)", 1);
        inspectorInstrumentChooser.addItem ("SimpleSynth", 2);
        inspectorInstrumentChooser.addItem ("Sampler", 3);   // G3.9
        inspectorInstrumentChooser.onChange = [this] {
            if (refreshingInspectorControls)
                return;
            const int selected = inspectorInstrumentChooser.getSelectedId();
            if (selected <= 0)
                return;
            (void) appModel.setInstrumentOnSelectedTrack (static_cast<yesdaw::engine::TrackInstrumentKind> (selected - 1));
            refreshActionState();
            repaintAll();
        };
        addChildComponent (inspectorInstrumentChooser);
        configureActionComponent (inspectorInstrumentEdit, yesdaw::ui::UiActionId::ViewInstrument, "Instrument panel");
        inspectorInstrumentEdit.setComponentID ("track.inspector.instrument.edit");
        inspectorInstrumentEdit.setButtonText ("Edit");
        inspectorInstrumentEdit.onClick = [this] {
            handleAction (yesdaw::ui::UiActionId::ViewInstrument);
            refreshActionState();
            resized();
            repaintAll();
        };
        addChildComponent (inspectorInstrumentEdit);

        // G3.5: the MIDI clip's settings rows (Logic's region inspector: Mute, Transpose, Velocity, Loop)
        // sit above the quantize rows on the same CLIP tab; every control is an undoable Clip edit.
        configureActionComponent (inspectorMidiMute, yesdaw::ui::UiActionId::MidiClipMuteToggle, "Mute MIDI clip");
        inspectorMidiMute.setComponentID ("clip.inspector.midi.mute");
        inspectorMidiMute.setButtonText ("");
        inspectorMidiMute.onClick = [this] {
            if (refreshingInspectorControls)
                return;
            // The row mutes the clip the rows show (not the timeline selection, which may be empty).
            (void) appModel.toggleSelectedMidiClipMute();
            recordLastAction (yesdaw::ui::UiActionId::MidiClipMuteToggle);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (inspectorMidiMute);
        const auto setUpMidiClipSlider = [this] (juce::Slider& slider, yesdaw::ui::UiActionId action, const char* id,
                                                 double minimum, double maximum, std::function<void (int)> post) {
            configureActionComponent (slider, action, "MIDI clip");
            slider.setComponentID (id);
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
            slider.setRange (minimum, maximum, 1.0);
            slider.setValue (0.0, juce::dontSendNotification);
            slider.onValueChange = [this, &slider, action, post] {
                if (refreshingInspectorControls || ! slider.isEnabled())
                    return;
                post (juce::roundToInt (slider.getValue()));
                recordLastAction (action);
                refreshActionState();
                repaintAll();
            };
            addChildComponent (slider);
        };
        setUpMidiClipSlider (inspectorMidiTranspose, yesdaw::ui::UiActionId::MidiClipTransposeSet, "clip.inspector.midi.transpose",
                             -static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipTransposeMax),
                             static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipTransposeMax),
                             [this] (int v) { (void) appModel.setSelectedMidiClipTranspose (v); });
        setUpMidiClipSlider (inspectorMidiVelocity, yesdaw::ui::UiActionId::MidiClipVelocityOffsetSet, "clip.inspector.midi.velocity",
                             -static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipVelocityOffsetMax),
                             static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipVelocityOffsetMax),
                             [this] (int v) { (void) appModel.setSelectedMidiClipVelocityOffset (static_cast<double> (v) / 100.0); });
        configureActionComponent (inspectorMidiLoop, yesdaw::ui::UiActionId::MidiClipLoopSelect, "MIDI clip loop");
        inspectorMidiLoop.setComponentID ("clip.inspector.midi.loop");
        inspectorMidiLoop.addItem ("Off", 1);
        inspectorMidiLoop.addItem ("1 beat", 2);
        inspectorMidiLoop.addItem ("1 bar", 3);
        inspectorMidiLoop.addItem ("2 bars", 4);
        inspectorMidiLoop.addItem ("4 bars", 5);
        inspectorMidiLoop.setSelectedId (1, juce::dontSendNotification);
        inspectorMidiLoop.onChange = [this] {
            if (refreshingInspectorControls || inspectorMidiLoop.getSelectedId() <= 0)
                return;
            (void) appModel.setSelectedMidiClipLoopChoice (inspectorMidiLoop.getSelectedId() - 1);
            recordLastAction (yesdaw::ui::UiActionId::MidiClipLoopSelect);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (inspectorMidiLoop);

        // G3.4: the quantize panel — the CLIP tab's content for a MIDI clip (Logic's region
        // inspector: Quantize, Q-Strength, Q-Swing, Q-Length; humanize). Settings, not edits:
        // each control posts its value to the context and Q (or Apply) applies them.
        configureActionComponent (inspectorQuantizeGrid, yesdaw::ui::UiActionId::QuantizeGridSelect, "Quantize grid");
        inspectorQuantizeGrid.setComponentID ("clip.inspector.quantize.grid");
        inspectorQuantizeGrid.addItem ("Snap grid", 1);
        inspectorQuantizeGrid.addItem ("1/8", 2);
        inspectorQuantizeGrid.addItem ("1/16", 3);
        inspectorQuantizeGrid.addItem ("1/32", 4);
        inspectorQuantizeGrid.setSelectedId (1, juce::dontSendNotification);
        inspectorQuantizeGrid.onChange = [this] {
            if (refreshingInspectorControls || inspectorQuantizeGrid.getSelectedId() <= 0)
                return;
            (void) appModel.selectQuantizeGrid (inspectorQuantizeGrid.getSelectedId() - 1);
            recordLastAction (yesdaw::ui::UiActionId::QuantizeGridSelect);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (inspectorQuantizeGrid);
        const auto setUpQuantizeSlider = [this] (juce::Slider& slider, yesdaw::ui::UiActionId action, const char* id,
                                                 double maximum, double initial, std::function<void (int)> post) {
            configureActionComponent (slider, action, "Quantize");
            slider.setComponentID (id);
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                    yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
            slider.setRange (0.0, maximum, 1.0);
            slider.setValue (initial, juce::dontSendNotification);
            slider.onValueChange = [this, &slider, action, post] {
                if (refreshingInspectorControls || ! slider.isEnabled())
                    return;
                post (juce::roundToInt (slider.getValue()));
                recordLastAction (action);
                refreshActionState();
                repaintAll();
            };
            addChildComponent (slider);
        };
        setUpQuantizeSlider (inspectorQuantizeStrength, yesdaw::ui::UiActionId::QuantizeStrengthSet, "clip.inspector.quantize.strength",
                             100.0, 100.0, [this] (int v) { (void) appModel.setQuantizeStrength (v); });
        setUpQuantizeSlider (inspectorQuantizeSwing, yesdaw::ui::UiActionId::QuantizeSwingSet, "clip.inspector.quantize.swing",
                             static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorQuantizeSwingMax), 0.0,
                             [this] (int v) { (void) appModel.setQuantizeSwing (v); });
        setUpQuantizeSlider (inspectorQuantizeHumanize, yesdaw::ui::UiActionId::QuantizeHumanizeSet, "clip.inspector.quantize.humanize",
                             100.0, 0.0, [this] (int v) { (void) appModel.setQuantizeHumanize (v); });
        configureActionComponent (inspectorQuantizeEnds, yesdaw::ui::UiActionId::QuantizeNoteEndsToggle, "Quantize note ends");
        inspectorQuantizeEnds.setComponentID ("clip.inspector.quantize.ends");
        inspectorQuantizeEnds.setButtonText ("");   // the painted row label names it (rubric: no double "Note ends")
        inspectorQuantizeEnds.onClick = [this] {
            if (refreshingInspectorControls)
                return;
            (void) appModel.toggleQuantizeNoteEnds();
            recordLastAction (yesdaw::ui::UiActionId::QuantizeNoteEndsToggle);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (inspectorQuantizeEnds);
        configureActionComponent (inspectorQuantizeApply, yesdaw::ui::UiActionId::PianoRollNoteQuantizeSelection, "Quantize");
        inspectorQuantizeApply.setComponentID ("clip.inspector.quantize.apply");
        inspectorQuantizeApply.setButtonText ("Apply (Q)");
        inspectorQuantizeApply.onClick = [this] {
            handleAction (yesdaw::ui::UiActionId::PianoRollNoteQuantizeSelection);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (inspectorQuantizeApply);

        // G2.18: the undo history window rides the same overlay law.
        undoHistory.rowsProvider = [this] {
            std::vector<juce::String> rows;
            for (const auto& step : appModel.undoHistory().steps)
            {
                juce::String label (yesdaw::engine::projectEditVerbLabel (step.verb));
                if (step.entryCount > 1)
                    label << " (" << static_cast<int> (step.entryCount) << " edits)";
                rows.push_back (label);
            }
            return rows;
        };
        undoHistory.currentProvider = [this] { return static_cast<int> (appModel.undoHistory().current); };
        undoHistory.onRowClicked = [this] (int row) {
            if (row < 0)
                return;
            (void) appModel.jumpToUndoHistoryStep (static_cast<std::size_t> (row));
            undoHistory.refreshRows();
            refreshActionState();
            resized();
            repaintAll();
        };
        undoHistory.onClose = [this] {
            handleAction (yesdaw::ui::UiActionId::EditShowUndoHistory);
            refreshActionState();
            resized();
            repaintAll();
        };
        addChildComponent (undoHistory);


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
                    - (currentPianoRollSurface().viewKeyCount - 1));   // G3.2 FIX 3
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
            // G3.2 checkpoint FIX 2: a sixteenth of the beat in force (Logic's default division), not
            // the pre-G3.2 512-tick grid step, which drew a dot. The roll's own division chooser is G3.4.
            const yesdaw::ui::UiPianoRollSurfaceSnapshot rollSurface = currentPianoRollSurface();
            const yesdaw::engine::Tick sixteenth =
                std::max<yesdaw::engine::Tick> (1, rollSurface.beatTicks / 4);
            // G3.8: scale assist — the pencil lands on the nearest key inside the project's scale.
            (void) appModel.addPianoRollNoteAt (tick, sixteenth, yesdaw::ui::pianoRollScaleSnappedKey (key, rollSurface.scaleRoot, rollSurface.scaleChoice));
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onNoteSplit = [this] (yesdaw::engine::EntityId midiClipId, yesdaw::engine::EntityId noteId, yesdaw::engine::Tick tick) {   // G3.2
            (void) appModel.splitPianoRollNoteAt (midiClipId, noteId, tick);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onKeyAuditioned = [this] (std::int16_t key, bool on) {   // G3.2: audition through the Track's Instrument
            (void) appModel.auditionNote (key, on);
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
        // G3.3: the control lane's gestures land on the model's Clip verbs (each one undo step).
        pianoRollInput.onControlPointAdded = [this] (yesdaw::engine::EntityId midiClipId,
                                                     yesdaw::engine::Tick tick,
                                                     double value) {
            (void) appModel.addPianoRollControlPoint (midiClipId, appModel.context().pianoRollControlLaneChoice, tick, value);
            recordLastAction (yesdaw::ui::UiActionId::PianoRollControlPointAdd);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onControlPointMoved = [this] (yesdaw::engine::EntityId midiClipId,
                                                     yesdaw::engine::EntityId pointId,
                                                     yesdaw::engine::Tick tick,
                                                     double value) {
            (void) appModel.movePianoRollControlPoint (midiClipId, pointId, tick, value);
            recordLastAction (yesdaw::ui::UiActionId::PianoRollControlPointMove);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onControlPointDeleted = [this] (yesdaw::engine::EntityId midiClipId,
                                                       yesdaw::engine::EntityId pointId) {
            (void) appModel.deletePianoRollControlPoint (midiClipId, pointId);
            recordLastAction (yesdaw::ui::UiActionId::PianoRollControlPointDelete);
            refreshActionState();
            repaintAll();
        };
        pianoRollInput.onControlLanePainted = [this] (yesdaw::engine::EntityId midiClipId,
                                                      std::span<const std::pair<yesdaw::engine::Tick, double>> points,
                                                      yesdaw::engine::Tick firstTick,
                                                      yesdaw::engine::Tick lastTick) {
            (void) appModel.paintPianoRollControlLane (midiClipId, appModel.context().pianoRollControlLaneChoice, points, firstTick, lastTick);
            recordLastAction (yesdaw::ui::UiActionId::PianoRollControlLanePaint);
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

        // G3.3: the control lane's chooser sits in the lane's keyboard gutter (plan §3.2 "CC1 Mod ▾").
        pianoRollLaneChooser.setComponentID ("pianoroll.lane.chooser");
        pianoRollLaneChooser.setName ("Control lane");
        pianoRollLaneChooser.setTooltip ("Which controller the piano roll's control lane shows: Mod (CC1), Sustain (CC64), Bend (pitch bend), Touch (aftertouch), Program (program change)");
        for (std::size_t i = 0; i < yesdaw::ui::kPianoRollControlLaneChoices.size(); ++i)
            pianoRollLaneChooser.addItem (yesdaw::ui::kPianoRollControlLaneChoices[i].name, static_cast<int> (i) + 1);
        pianoRollLaneChooser.setSelectedId (1, juce::dontSendNotification);
        pianoRollLaneChooser.onChange = [this] {
            const int selected = pianoRollLaneChooser.getSelectedId();
            if (selected <= 0 || selected - 1 == appModel.context().pianoRollControlLaneChoice)
                return;
            (void) appModel.selectPianoRollControlLane (selected - 1);
            recordLastAction (yesdaw::ui::UiActionId::PianoRollControlLaneSelect);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (pianoRollLaneChooser);

        // G3.8: the roll header's Key / Scale choosers — the project's scale assist.
        pianoRollKeyChooser.setComponentID ("pianoroll.key");
        pianoRollKeyChooser.setName ("Key");
        pianoRollKeyChooser.setTooltip ("The project's key: the root of the scale assist");
        {
            static constexpr const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            for (int i = 0; i < 12; ++i)
                pianoRollKeyChooser.addItem (kKeyNames[i], i + 1);
        }
        pianoRollKeyChooser.setSelectedId (1, juce::dontSendNotification);
        pianoRollKeyChooser.onChange = [this] {
            const int selected = pianoRollKeyChooser.getSelectedId();
            if (selected <= 0)
                return;
            (void) appModel.setProjectScale (selected - 1, appModel.context().pianoRollScaleChoice, true);
            recordLastAction (yesdaw::ui::UiActionId::PianoRollScaleRootSelect);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (pianoRollKeyChooser);
        pianoRollScaleChooser.setComponentID ("pianoroll.scale");
        pianoRollScaleChooser.setName ("Scale");
        pianoRollScaleChooser.setTooltip ("Scale assist: Off shows every key; Major / Minor lift the keys inside the project's scale and the pencil lands on them");
        pianoRollScaleChooser.addItem ("Scale: Off", 1);
        pianoRollScaleChooser.addItem ("Major", 2);
        pianoRollScaleChooser.addItem ("Minor", 3);
        pianoRollScaleChooser.setSelectedId (1, juce::dontSendNotification);
        pianoRollScaleChooser.onChange = [this] {
            const int selected = pianoRollScaleChooser.getSelectedId();
            if (selected <= 0)
                return;
            (void) appModel.setProjectScale (appModel.context().pianoRollScaleRoot, selected - 1, false);
            recordLastAction (yesdaw::ui::UiActionId::PianoRollScaleSelect);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (pianoRollScaleChooser);

        // G3.6: the roll header's Typing and Step toggles (plan §3.2 "[step ⏺]"); Ctrl+K is Typing's chord.
        configureActionComponent (pianoRollTypingButton, yesdaw::ui::UiActionId::PianoRollMusicalTypingToggle, "Musical typing");
        pianoRollTypingButton.setComponentID ("pianoroll.typing");
        pianoRollTypingButton.setButtonText ("Typing");
        pianoRollTypingButton.setClickingTogglesState (false);
        pianoRollTypingButton.onClick = [this] {
            handleAction (yesdaw::ui::UiActionId::PianoRollMusicalTypingToggle);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (pianoRollTypingButton);
        configureActionComponent (pianoRollStepButton, yesdaw::ui::UiActionId::PianoRollStepInputToggle, "Step input");
        pianoRollStepButton.setComponentID ("pianoroll.step");
        pianoRollStepButton.setButtonText ("Step");
        pianoRollStepButton.setClickingTogglesState (false);
        pianoRollStepButton.onClick = [this] {
            handleAction (yesdaw::ui::UiActionId::PianoRollStepInputToggle);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (pianoRollStepButton);

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
        hideMixerControlsBehindDockTab();   // G3.2 checkpoint FIX 1

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
        // G3.10: the thru target follows the selection every tick; the input lamp holds for a moment
        // after the last played note (the counter is the device thread's, read relaxed).
        appModel.updateMidiThruTarget();
        {
            const std::uint32_t seen = appModel.midiInputQueue().seen();
            if (seen != midiInSeenLast)
            {
                midiInSeenLast = seen;
                midiInLitUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds (yesdaw::ui::UiTheme::Layout::headerMidiInLampHoldMs);
                repaint (headerLayout().midiIn);
            }
            else if (midiInLitUntil != std::chrono::steady_clock::time_point {} && std::chrono::steady_clock::now() >= midiInLitUntil)
            {
                midiInLitUntil = {};
                repaint (headerLayout().midiIn);
            }
        }
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
                hideMixerControlsBehindDockTab();   // G3.2 checkpoint FIX 1: the tick's refresh restores the lane; hide it again
            }
        }

        // G0.4: a follow-scroll moves the whole canvas — that IS a view change; otherwise only the
        // dynamic layers (playhead, meters, transport counter) repaint this tick.
        const double scrollBefore = timelineScrollSeconds;
        followPlaybackPlayhead();
        const bool rollScrolled = followPianoRollPlayhead();   // G3.2
        if (timelineScrollSeconds != scrollBefore || rollScrolled)
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
        // G1.6: a status message wins; otherwise the hovered zone's gesture hint.
        statusLine.setText (appModel.statusLineText().empty() ? hoverHintOrModeHint() : juce::String (appModel.statusLineText()),
                            juce::dontSendNotification);
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
        // G3.10: MIDI thru — straight from this (device) thread into the engine's lane, no message-
        // thread hop; the selected Track's Instrument plays it. The capture path below stays as it was.
        (void) postMidiInputFromDevice (noteOn, note, static_cast<double> (velocity), message.getChannel() - 1);
        juce::Component::SafePointer<MainComponent> safeThis (this);
        juce::MessageManager::callAsync ([safeThis, frame, noteOn, note, velocity] {
            if (safeThis != nullptr)
                safeThis->handleCapturedMidiNote (frame, noteOn, note, velocity);
        });
    }

    // G3.10: the one entry every played note takes (the device callback and the harness alike).
    [[nodiscard]] bool postMidiInputFromDevice (bool noteOn, int note, double velocity, int channel) noexcept
    {
        return appModel.postMidiInputNote (noteOn, note, velocity, channel);
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

    // G3.6: while a keyboard mode is on and nothing is hovered, the status line says so — the mode
    // is never silent (a swallowed key would otherwise look like a dead one).
    [[nodiscard]] juce::String hoverHintOrModeHint() const
    {
        if (! hoverHint.isEmpty())
            return hoverHint;
        const auto& context = appModel.context();
        if (context.musicalTypingOn)
            return "Musical typing ON: A W S E D F T G Y H U J K O L P ; play "
                 + juce::String (yesdaw::ui::pianoRollKeyName (context.typingBaseKey)) + " up \u00b7 Z / X octave \u00b7 C / V velocity ("
                 + juce::String (context.typingVelocityPercent) + " %)" + (context.stepInputOn ? " \u00b7 step input: notes enter at the playhead" : "") + " \u00b7 Ctrl+K off";
        if (context.stepInputOn)
            return "Step input ON: a typed or clicked note enters at the playhead with the snap length, Right = rest, Left = back";
        return {};
    }
    void harnessReleaseTypedKeysForTest() { harnessReleaseTypedKeys(); }
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
        if (stripIndex < 0 || stripIndex >= stripTotal || cellIndex < 0)
            return {};

        return paintedMuteSoloCellBoundsForLane (
            paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
            static_cast<std::size_t> (cellIndex),
            stripCellCount (static_cast<std::size_t> (stripIndex)));
    }

    // G4.1: the painted I/O row rect for a strip (shell coordinates) — the same law the paint and the
    // click read; empty where the strip has no such slot or is too short to carry it.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedIoRowBounds (int stripIndex, int row) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal)
            return {};
        const auto lane = paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex));
        const int ioRows = stripIoRows (static_cast<std::size_t> (stripIndex));
        if (row == kMixerIoInputRow)
            return paintedInputRowBoundsForLane (lane, ioRows);
        if (row == kMixerIoOutputRow)
            return paintedOutputRowBoundsForLane (lane, ioRows);
        return {};
    }

    // G4.1: the two slots' texts as painted.
    [[nodiscard]] yesdaw::ui::MainComponentMixerStripIo harnessMixerStripIo (int stripIndex) const
    {
        yesdaw::ui::MainComponentMixerStripIo io;
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        const int stripTotal = static_cast<int> (trackCount + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal)
            return io;
        const auto index = static_cast<std::size_t> (stripIndex);
        if (index < trackCount)
            io.input = stripInputText (index);
        io.output = stripOutputText (index < trackCount ? surface.tracks[index] : surface.buses[index - trackCount]);
        return io;
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
                                              static_cast<std::size_t> (slotIndex),
                                              stripIoRows (static_cast<std::size_t> (stripIndex)));
    }

    // M5: the painted send-row rect for a strip, in SHELL coordinates.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedSendRowBounds (int stripIndex, int sendIndex) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal || sendIndex < 0)
            return {};

        return paintedSendRowBoundsForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                            static_cast<std::size_t> (sendIndex),
                                            stripIoRows (static_cast<std::size_t> (stripIndex)));
    }

    // M6: the painted fader rail and the y the thumb sits at for a given gain — the same law the
    // paint uses, so a gate can prove unity is NOT at the top of the rail.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedFaderRailBounds (int stripIndex) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal)
            return {};

        return paintedFaderRailForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                        stripIoRows (static_cast<std::size_t> (stripIndex)));
    }

    [[nodiscard]] juce::Rectangle<int> harnessPaintedPanKnobBounds (int stripIndex) const
    {
        const auto surface = currentMixerSurface();
        const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
        if (stripIndex < 0 || stripIndex >= stripTotal)
            return {};
        return paintedPanKnobForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)));
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
            case yesdaw::ui::TimelineTool::Eraser:   return "Eraser";     // G3.2
            case yesdaw::ui::TimelineTool::Velocity: return "Velocity";
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
        put ("header.gear", headerLayout().gear);   // the settings-row toggle, clickable since 2026-09-04
        put ("rail", leftRailPanelBounds());
        put ("timeline", timelineBounds());
        put ("inspector", inspectorBounds());
        if (inspectorShowsQuantizePanel())   // G3.4: the panel's controls by name
        {
            put ("inspector.midi.mute", inspectorMidiMute.getBounds());   // G3.5
            put ("inspector.midi.transpose", inspectorMidiTranspose.getBounds());
            put ("inspector.midi.velocity", inspectorMidiVelocity.getBounds());
            put ("inspector.midi.loop", inspectorMidiLoop.getBounds());
            put ("inspector.quantize.grid", inspectorQuantizeGrid.getBounds());
            put ("inspector.quantize.strength", inspectorQuantizeStrength.getBounds());
            put ("inspector.quantize.swing", inspectorQuantizeSwing.getBounds());
            put ("inspector.quantize.ends", inspectorQuantizeEnds.getBounds());
            put ("inspector.quantize.humanize", inspectorQuantizeHumanize.getBounds());
            put ("inspector.quantize.apply", inspectorQuantizeApply.getBounds());
        }
        put ("header.midi.in", headerLayout().midiIn);   // G3.10: the input lamp
        if (appModel.context().mixerDockVisible)
        {
            put ("dock", mixerPanelBounds());
            if (dockShowsPianoRoll())
            {
                // G3.3: the control lane's data area and its chooser, so a drive draws where the lane is.
                const PianoRollCanvasGeometry rollGeometry = pianoRollCanvasGeometry (pianoRollInput.getBounds().withZeroOrigin());
                put ("pianoroll.lane", pianoRollControlLaneDataArea (rollGeometry).translated (pianoRollInput.getX(), pianoRollInput.getY()));
                put ("pianoroll.lane.chooser", pianoRollLaneChooser.getBounds());
                put ("pianoroll.key", pianoRollKeyChooser.getBounds());     // G3.8
                put ("pianoroll.scale", pianoRollScaleChooser.getBounds());
                put ("pianoroll.typing", pianoRollTypingButton.getBounds());   // G3.6
                put ("pianoroll.step", pianoRollStepButton.getBounds());
            }
            // G3.9: the Sampler's pad grid, when the Instrument tab shows one (panel-local → shell).
            if (instrumentPanel.isVisible() && instrumentPanel.padsShown())
                put ("instrument.panel.pads", instrumentPanel.currentPadGrid().translated (instrumentPanel.getX(), instrumentPanel.getY()));
            // The mixer's painted zones by name (mixer.strip.N, .solo, .mute, .fader,
            // .insert.K, .send.K) so a drive clicks the strip it sees — nothing about the mixer
            // was clickable by name before 2026-09-04, which is where the mouse-only bugs hid.
            const auto surface = currentMixerSurface();
            const int stripTotal = static_cast<int> (surface.tracks.size() + surface.buses.size());
            for (int strip = 0; strip < stripTotal; ++strip)
            {
                const juce::String base = "mixer.strip." + juce::String (strip);
                put (base, harnessPaintedMixerStripBounds (strip));
                put (base + ".solo", harnessPaintedMuteSoloCellBounds (strip, 0));
                put (base + ".mute", harnessPaintedMuteSoloCellBounds (strip, 1));
                put (base + ".arm", harnessPaintedMuteSoloCellBounds (strip, 2));   // G4.1: Track strips only
                put (base + ".input", harnessPaintedIoRowBounds (strip, kMixerIoInputRow));
                put (base + ".output", harnessPaintedIoRowBounds (strip, kMixerIoOutputRow));
                put (base + ".fader", harnessPaintedFaderRailBounds (strip));
                put (base + ".pan", harnessPaintedPanKnobBounds (strip));
                for (int slot = 0; slot < yesdaw::ui::UiTheme::Layout::mixerPaintedInsertRowCount; ++slot)
                    put (base + ".insert." + juce::String (slot), harnessPaintedInsertSlotBounds (strip, slot));
                for (int send = 0; send < yesdaw::ui::UiTheme::Layout::mixerPaintedSendRowCount; ++send)
                    put (base + ".send." + juce::String (send), harnessPaintedSendRowBounds (strip, send));
            }
        }

        // G4.1 cp2: the FX editor and its two buttons (grandchildren: the walk below sees children only).
        if (fxEditor.isVisible())
        {
            put ("mixer.fx.editor", fxEditor.getBounds());
            for (int i = 0; i < fxEditor.getNumChildComponents(); ++i)
                if (const juce::Component* child = fxEditor.getChildComponent (i))
                    if (child->isVisible() && child->getComponentID().startsWith ("mixer.fx.editor."))
                        put (child->getComponentID(), child->getBounds().translated (fxEditor.getX(), fxEditor.getY()));
        }
        // Every visible identified child by its component id — toolbar buttons carry their
        // action's stable id (configureActionComponent), choosers their shell ids.
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (const juce::Component* child = getChildComponent (i))
                if (child->isVisible() && child->getComponentID().isNotEmpty())
                    put ("widget." + child->getComponentID(), child->getBounds());

        if (appModel.context().projectLoaded)
        {
            const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
            const juce::Point<int> origin = timelineInput.getPosition();
            put ("ruler", geometry.rulerArea.translated (origin.x, origin.y));
            put ("clipArea", geometry.clipArea.translated (origin.x, origin.y));
            for (int mapIndex = 0; mapIndex < state.mapLabelCount; ++mapIndex)   // the tempo / meter change labels
                put ("ruler.map." + juce::String (mapIndex),
                     yesdaw::ui::timelineMapLabelRect (timelineInput.getLocalBounds(), state, mapIndex).translated (origin.x, origin.y));
            // The tool strip's cells by name (tool.pointer … tool.hand): drives click by NAME.
            for (std::size_t index = 0; index < yesdaw::ui::kTimelineToolStripOrder.size(); ++index)
                put ("tool." + juce::String (probeToolName (yesdaw::ui::kTimelineToolStripOrder[index])).toLowerCase(),
                     yesdaw::ui::timelineToolStripCell (geometry.toolbarArea, index).translated (origin.x, origin.y));

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
                // The row's painted M / S / O cells by name, so a drive clicks the badge, not a pixel.
                put ("rail.row." + juce::String (lane) + ".mute", harnessPaintedRailCellBounds (lane, 0));
                put ("rail.row." + juce::String (lane) + ".solo", harnessPaintedRailCellBounds (lane, 1));
                put ("rail.row." + juce::String (lane) + ".arm",  harnessPaintedRailCellBounds (lane, 2));
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
            transport->setProperty ("midiInSeen", static_cast<int> (appModel.midiInputQueue().seen()));   // G3.10
            transport->setProperty ("midiInDrained", static_cast<int> (appModel.midiInputDrained()));
            transport->setProperty ("midiInLit", midiInLitUntil != std::chrono::steady_clock::time_point {}
                                                     && std::chrono::steady_clock::now() < midiInLitUntil);
            transport->setProperty ("midiThruTarget", static_cast<juce::int64> (appModel.midiInputQueue().thruTarget()));
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
            if (selectedTrackLanes.empty())
            {
                if (selectedTrackLane >= 0)
                    tracks.add (selectedTrackLane);
            }
            else
                for (const int lane : selectedTrackLanes)   // G2.17: every selected lane, ascending
                    tracks.add (lane);
            selection->setProperty ("tracks", tracks);
            selection->setProperty ("primaryTrack", selectedTrackLane);
            juce::Array<juce::var> trackKinds;
            if (appModel.context().projectLoaded)
                for (std::size_t t = 0; t < appModel.project().tracks.size(); ++t)
                    trackKinds.add (trackHoldsMidi (t) ? "midi" : "audio");
            selection->setProperty ("trackKinds", trackKinds);
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
            view->setProperty ("inspector", ! inspectorBounds().isEmpty());
            view->setProperty ("dock", ! context.mixerDockVisible ? juce::String ("None")
                                       : context.editorDockTab == yesdaw::ui::UiEditorDockTab::PianoRoll
                                           ? juce::String ("PianoRoll")
                                       : context.editorDockTab == yesdaw::ui::UiEditorDockTab::Instrument
                                           ? juce::String ("Instrument")   // G3.1
                                           : juce::String ("Mixer"));
            if (const yesdaw::engine::Track* const instrumentTrack = appModel.selectedTrackForInstrument())
            {
                view->setProperty ("instrument", juce::String (instrumentKindName (instrumentTrack->instrumentKind)));   // G3.1
                view->setProperty ("samplerPadCount", static_cast<int> (instrumentTrack->samplerPads.size()));   // G3.9
            }
            view->setProperty ("dockHeight", dockedMixerHeight());
            view->setProperty ("mixerNarrow", context.mixerStripsNarrow);   // G4.1
            view->setProperty ("settingsRow", context.settingsRowVisible);
            view->setProperty ("headerHeight", headerHeightNow());
            view->setProperty ("nudgeValue", context.nudgeValue);
            view->setProperty ("nudgeFrames", static_cast<juce::int64> (appModel.nudgeFrames()));   // G2.8
            view->setProperty ("editMode", context.editMode == yesdaw::ui::UiEditMode::Overlap ? "overlap"
                                           : context.editMode == yesdaw::ui::UiEditMode::NoOverlap ? "no-overlap" : "shuffle");   // G2.6
            view->setProperty ("snapMode", context.snapMode == yesdaw::ui::UiSnapMode::Grid ? "grid"
                                           : context.snapMode == yesdaw::ui::UiSnapMode::Relative ? "relative"
                                           : context.snapMode == yesdaw::ui::UiSnapMode::Events ? "events" : "off");   // G2.7
            view->setProperty ("snapEffectiveTicks", static_cast<juce::int64> (effectiveSnapGridTicks()));
            view->setProperty ("keymapEditor", context.keymapVisible);
            view->setProperty ("undoHistory", context.undoHistoryVisible);   // G2.18
            view->setProperty ("hoverHint", hoverHint);
            view->setProperty ("railWidth", viewState.railWidth);          // G2.1
            view->setProperty ("inspectorWidth", inspectorWidthNow());
            view->setProperty ("dockHeight", dockedMixerHeight());
            {
                const CounterStrings counter = counterStrings();
                view->setProperty ("timeDisplay", counter.mode);
                view->setProperty ("rulerTimeFormat", juce::String (yesdaw::ui::timeline_canvas_detail::rulerTimeFormatName (timeDisplayMode)));   // G2.2
                view->setProperty ("counterPrimary", counter.primary);
                view->setProperty ("counterSecondary", counter.secondary);
            }
            view->setProperty ("tool", probeToolName (context.activeTimelineTool));
            view->setProperty ("lastAuditionKey", pianoRollInput.lastAuditionKey());   // G3.2
            {
                int noteCount = -1;   // -1 = no MIDI clip selected
                if (context.projectLoaded)
                    for (const yesdaw::engine::MidiClip& clip : appModel.project().midiClips)
                        if (clip.id == appModel.selectedMidiClipId())
                            noteCount = static_cast<int> (clip.notes.size());
                view->setProperty ("noteCount", noteCount);   // G3.2: the roll's clip's note count
            }
            {
                // G3.2 checkpoint: the roll's window and its painted notes (shell-local geometry), so a
                // session drive aims at what is painted instead of guessing.
                auto* roll = new juce::DynamicObject();
                const yesdaw::ui::UiPianoRollSurfaceSnapshot rollSurface = currentPianoRollSurface();
                const PianoRollCanvasGeometry rollGeometry = pianoRollCanvasGeometry (pianoRollInput.getBounds().withZeroOrigin());
                roll->setProperty ("viewLowKey", rollSurface.viewLowKey);
                roll->setProperty ("scaleRoot", rollSurface.scaleRoot);       // G3.8
                roll->setProperty ("drumMode", rollSurface.drumMode);         // G3.9: the clip's Track is a Sampler
                roll->setProperty ("scaleChoice", rollSurface.scaleChoice);
                roll->setProperty ("viewHighKey", pianoRollViewHighKey (rollSurface));
                roll->setProperty ("rowHeight", static_cast<double> (rollGeometry.rowHeight));
                roll->setProperty ("viewScrollTicks", static_cast<juce::int64> (rollSurface.viewScrollTicks));
                roll->setProperty ("visibleTicks", static_cast<juce::int64> (pianoRollVisibleTicks (rollSurface)));
                roll->setProperty ("gridX", rollGeometry.grid.getX());
                roll->setProperty ("gridY", rollGeometry.grid.getY());
                roll->setProperty ("gridWidth", rollGeometry.grid.getWidth());
                roll->setProperty ("gridHeight", rollGeometry.grid.getHeight());
                roll->setProperty ("keyboardX", rollGeometry.keyboard.getX());
                roll->setProperty ("keyboardWidth", rollGeometry.keyboard.getWidth());
                {
                    // G3.3: the control lane — its name, its data area (roll-local) and its point count.
                    const juce::Rectangle<int> laneData = pianoRollControlLaneDataArea (rollGeometry);
                    roll->setProperty ("controlLane", juce::String (yesdaw::ui::pianoRollControlLaneChoice (rollSurface.controlLaneChoice).name));
                    roll->setProperty ("controlLaneX", laneData.getX());
                    roll->setProperty ("controlLaneY", laneData.getY());
                    roll->setProperty ("controlLaneWidth", laneData.getWidth());
                    roll->setProperty ("controlLaneHeight", laneData.getHeight());
                    const auto* lane = pianoRollControlLaneOf (rollSurface);
                    roll->setProperty ("controlPointCount", lane != nullptr ? static_cast<int> (lane->points.size()) : 0);
                    roll->setProperty ("laneGesture", pianoRollInput.lastLaneGesture());
                }
                juce::Array<juce::var> rollNotes;
                for (const yesdaw::ui::UiPianoRollNoteView& note : rollSurface.notes)
                {
                    auto* item = new juce::DynamicObject();
                    item->setProperty ("id", juce::String (entityIdHex (note.noteId)));
                    item->setProperty ("start", static_cast<juce::int64> (note.startTick));
                    item->setProperty ("length", static_cast<juce::int64> (note.lengthTicks));
                    item->setProperty ("key", static_cast<int> (note.key));
                    rollNotes.add (juce::var (item));
                }
                roll->setProperty ("notes", rollNotes);
                view->setProperty ("pianoRoll", juce::var (roll));
            }
            view->setProperty ("snapEnabled", context.snapEnabled);
            view->setProperty ("snapGridTicks", static_cast<juce::int64> (context.snapGridTicks));
            {
                // G3.4: the quantize settings Q applies, and whether the panel is up.
                auto* quantize = new juce::DynamicObject();
                const yesdaw::engine::QuantizeSettings settings = appModel.currentQuantizeSettings();
                quantize->setProperty ("gridChoice", context.quantizeGridChoice);
                quantize->setProperty ("gridTicks", static_cast<juce::int64> (settings.grid.intervalTicks));
                quantize->setProperty ("strength", context.quantizeStrengthPercent);
                quantize->setProperty ("swing", context.quantizeSwingPercent);
                quantize->setProperty ("noteEnds", context.quantizeNoteEnds);
                quantize->setProperty ("humanize", context.quantizeHumanizePercent);
                quantize->setProperty ("seed", static_cast<juce::int64> (settings.humanizeSeed));
                quantize->setProperty ("panel", inspectorShowsQuantizePanel());
                view->setProperty ("quantize", juce::var (quantize));
            }
            {
                // G3.6: the keyboard modes.
                auto* typing = new juce::DynamicObject();
                typing->setProperty ("on", context.musicalTypingOn);
                typing->setProperty ("baseKey", context.typingBaseKey);
                typing->setProperty ("velocity", context.typingVelocityPercent);
                typing->setProperty ("lastKey", appModel.lastTypedKey());
                typing->setProperty ("heldCount", appModel.typedHeldCount());
                view->setProperty ("musicalTyping", juce::var (typing));
                auto* step = new juce::DynamicObject();
                step->setProperty ("on", context.stepInputOn);
                step->setProperty ("stepTicks", static_cast<juce::int64> (appModel.stepInputTicks()));
                view->setProperty ("stepInput", juce::var (step));
            }
            if (const yesdaw::engine::MidiClip* const midiClip = appModel.selectedMidiClip())
            {
                // G3.5: the selected MIDI clip's settings as the inspector shows them.
                auto* clipView = new juce::DynamicObject();
                clipView->setProperty ("id", juce::String (entityIdHex (midiClip->id)));
                clipView->setProperty ("muted", midiClip->muted);
                clipView->setProperty ("transpose", static_cast<int> (midiClip->transposeSemitones));
                clipView->setProperty ("velocityOffset", midiClip->velocityOffset);
                clipView->setProperty ("loopLength", static_cast<juce::int64> (midiClip->loopLengthTicks));
                clipView->setProperty ("loopChoice", appModel.midiClipLoopChoiceFor (*midiClip));
                view->setProperty ("midiClip", juce::var (clipView));
            }
            view->setProperty ("playheadFollow", context.playheadFollowEnabled);
            view->setProperty ("playheadFollowContinuous", context.playheadFollowContinuous);   // G2.16
            view->setProperty ("rowZoom", timelineRowZoom);
            view->setProperty ("zoomHistoryDepth", static_cast<int> (zoomHistory.size()));
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
            recording->setProperty ("armedTrackCount", static_cast<int> (appModel.armedRecordingTrackInputs().size()));   // G4.1
            root->setProperty ("recording", juce::var (recording));
        }
        {
            // G4.1: the mixer's strips as painted — name, the two I/O slots' texts, the arm — so a
            // drive asserts what it sees on the strip after a slot pick.
            auto* mixer = new juce::DynamicObject();
            const auto surface = currentMixerSurface();
            const std::size_t trackCount = surface.tracks.size();
            mixer->setProperty ("trackCount", static_cast<int> (trackCount));
            mixer->setProperty ("busCount", static_cast<int> (surface.buses.size()));
            mixer->setProperty ("narrow", context.mixerStripsNarrow);
            juce::Array<juce::var> strips;
            for (std::size_t i = 0; i < trackCount + surface.buses.size(); ++i)
            {
                const yesdaw::ui::UiMixerStrip& state = i < trackCount ? surface.tracks[i] : surface.buses[i - trackCount];
                auto* strip = new juce::DynamicObject();
                strip->setProperty ("name", juce::String (state.name));
                strip->setProperty ("kind", i < trackCount ? "Track" : "Bus");
                strip->setProperty ("input", i < trackCount ? stripInputText (i) : juce::String());
                strip->setProperty ("output", stripOutputText (state));
                strip->setProperty ("armed", i < trackCount && appModel.isRecordingTrackIndexArmed (i));
                strip->setProperty ("muted", state.muted);     // G4.1 cp2: the S / M cells as painted
                strip->setProperty ("soloed", state.soloed);
                // G4.1 cp2: the painted inserts and sends, as the strip reads them.
                juce::Array<juce::var> inserts;
                for (const yesdaw::ui::UiMixerFxSlotReadout& insert : state.fxSlots)
                {
                    auto* row = new juce::DynamicObject();
                    row->setProperty ("kind", fxKindName (insert.kind));
                    row->setProperty ("enabled", insert.enabled);
                    inserts.add (juce::var (row));
                }
                strip->setProperty ("inserts", inserts);
                juce::Array<juce::var> sends;
                for (const yesdaw::ui::UiMixerSendReadout& send : state.sends)
                {
                    auto* row = new juce::DynamicObject();
                    row->setProperty ("bus", juce::String (send.busName));
                    row->setProperty ("level", static_cast<double> (send.linearGain));
                    row->setProperty ("pre", send.preFader);
                    sends.add (juce::var (row));
                }
                strip->setProperty ("sends", sends);
                strips.add (juce::var (strip));
            }
            mixer->setProperty ("strips", strips);
            root->setProperty ("mixer", juce::var (mixer));
        }
        {
            // G4.1 cp2: the FX editor — what a drive sees after a slot's double-click.
            const yesdaw::ui::MainComponentFxEditor editor = harnessFxEditor();
            auto* fx = new juce::DynamicObject();
            fx->setProperty ("visible", editor.visible);
            fx->setProperty ("strip", editor.strip);
            fx->setProperty ("slot", editor.slot);
            fx->setProperty ("kind", editor.kind);
            fx->setProperty ("bypassed", editor.bypassed);
            fx->setProperty ("rows", editor.rows);
            root->setProperty ("fxEditor", juce::var (fx));
            // G4.1 cp2: the Touch / Latch ride a painted drag is buffering (N5) — what a drive sees mid-ride.
            auto* ride = new juce::DynamicObject();
            ride->setProperty ("active", automationTouchRideActive);
            ride->setProperty ("samples", static_cast<int> (automationTouchRideSamples.size()));
            root->setProperty ("ride", juce::var (ride));
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
    // G2.18: the undo history window for the harness.
    [[nodiscard]] UndoHistoryComponent& harnessUndoHistory() noexcept { return undoHistory; }
    [[nodiscard]] InstrumentPanelComponent& harnessInstrumentPanel() noexcept { return instrumentPanel; }   // G3.1
    [[nodiscard]] yesdaw::ui::UiPianoRollSurfaceSnapshot harnessPianoRollSurface() const { return currentPianoRollSurface(); }   // G3.2
    [[nodiscard]] juce::Rectangle<int> harnessPianoRollBounds() const { return pianoRollInput.getBounds(); }
    [[nodiscard]] int harnessPianoRollAuditionKey() const { return pianoRollInput.heldAuditionKey(); }   // G3.2
    void harnessSelectPianoRollControlLane (int choice) { pianoRollLaneChooser.setSelectedId (choice + 1, juce::sendNotificationSync); }   // G3.3
    [[nodiscard]] bool harnessPostMidiInput (bool on, int key, double velocity) noexcept   // G3.10: the device callback's path
    {
        return postMidiInputFromDevice (on, key, velocity, 0);
    }
    void harnessSelectPianoRollScale (int rootKey, int scaleChoice)   // G3.8: through the real choosers
    {
        pianoRollKeyChooser.setSelectedId (rootKey + 1, juce::sendNotificationSync);
        pianoRollScaleChooser.setSelectedId (scaleChoice + 1, juce::sendNotificationSync);
    }
    void harnessServiceUiTick() { serviceUiTick(); }   // G3.2 checkpoint: the timer's own refresh path
    [[nodiscard]] bool harnessAuditionNote (std::int16_t key, bool on) { return appModel.auditionNote (key, on); }   // G3.2
    [[nodiscard]] std::vector<float> harnessRenderPlayback (std::uint64_t frames, int blockSize)   // G3.2: the engine's own blocks
    {
        return appModel.renderPlaybackFrames (frames, blockSize);
    }

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

    [[nodiscard]] double harnessTimelineZoomCeiling() const noexcept { return timelineZoomCeiling(); }   // G2.19

    // The source window the canvas was handed for a painted clip (the waveform painter's law).
    [[nodiscard]] yesdaw::ui::TimelineClipSourceWindow harnessTimelineClipSourceWindow (int layoutClipId) const
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClips.size()))
            return {};
        const yesdaw::ui::Clip& clip = timelineClips[static_cast<std::size_t> (layoutClipId)];
        return { clip.sourceStartFrame, clip.sourceFrameCount };
    }

    // The rail's three painted cells (M / S / O) in shell coordinates — the rects the rail's
    // hit-test claims, so a test or a drive clicks the badge it sees.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedRailCellBounds (int row, int cell) const
    {
        const juce::Rectangle<int> local = cell == 0 ? trackListInput.muteCellBounds (row)
                                         : cell == 1 ? trackListInput.soloCellBounds (row)
                                                     : trackListInput.armCellBounds (row);
        return local.translated (trackListInput.getX(), trackListInput.getY());
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

        work.removeFromBottom (dockedMixerHeight());
        auto left = work.removeFromLeft (viewState.railWidth)
                        .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                  yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
        auto inspector = work.removeFromRight (inspectorWidthNow())
                             .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                       yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
        drawTrackList (g, left);
        drawInspector (g, inspector);
        // V3: a collapsed dock paints NOTHING (the "drop whole" law this codebase already uses
        // elsewhere for sections that don't fit) rather than relying on a zero/negative-height
        // rect to degrade gracefully.
        if (appModel.context().mixerDockVisible)
        {
            // G2.1 cp2: the dock shows ONE editor tab. G3.1: the instrument panel paints itself.
            if (dockShowsPianoRoll())
                drawPianoRoll (g, mixerPanelBounds());
            else if (! dockShowsInstrument())
                drawMixer (g, mixerPanelBounds());
        }
    }

    void resized() override
    {
        restoreControlsHiddenByDockTab();   // G2.1 cp2
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        const HeaderLayout h = headerLayout();

        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto action = toolbarActions[i];
            if (isSettingsRowAction (action))
                buttons[i].setVisible (h.settingsVisible
                                       && action != yesdaw::ui::UiActionId::RecordingAssembleComp);   // G1.7: hidden until G7
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
                // G2.1 cp2: ViewMixer / ViewPianoRoll sit in the status row's view cluster (below).
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
        {
            // G2.16: the scroll bars take a strip below and beside the timeline; the input keeps the rest.
            juce::Rectangle<int> area = timelinePanelBounds();
            const juce::Rectangle<int> hBar = area.removeFromBottom (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness);
            const juce::Rectangle<int> vBar = area.removeFromRight (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness);
            timelineHScroll.setBounds (hBar.withTrimmedRight (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness));
            timelineVScroll.setBounds (vBar);
            timelineInput.setBounds (area);
            jassert (area == timelineBounds());
        }
        playheadLayer.setBounds (timelineBounds());
        {
            // G2.1: the splitters sit on the panel edges, above every panel (last in z-order).
            const int thickness = yesdaw::ui::UiTheme::Layout::splitterThickness;
            const int half = thickness / 2;
            const int top = headerHeightNow();
            const int dockTop = getHeight() - dockedMixerHeight();
            dockSplitter.setBounds (getLocalBounds().withY (dockTop - half).withHeight (thickness));
            dockSplitter.setVisible (dockedMixerHeight() > 0);
            railSplitter.setBounds (viewState.railWidth - half, top, thickness, dockTop - top);
            railSplitter.setVisible (true);
            inspectorSplitter.setBounds (getWidth() - inspectorWidthNow() - half, top, thickness, dockTop - top);
            inspectorSplitter.setVisible (inspectorWidthNow() > 0);
            for (yesdaw::ui::SplitterComponent* splitter : { &railSplitter, &inspectorSplitter, &dockSplitter })
                splitter->toFront (false);
        }
        // G1.5: the keymap editor floats over the arrangement, centred, at most 760×520.
        {
            const juce::Rectangle<int> work = getLocalBounds().withTrimmedTop (headerHeightNow()).withTrimmedBottom (dockedMixerHeight());
            using L = yesdaw::ui::UiTheme::Layout;
            const int width = std::min (L::keymapEditorMaxWidth, work.getWidth() - L::keymapEditorMargin);
            const int height = std::min (L::keymapEditorMaxHeight, work.getHeight() - L::keymapEditorMargin);
            keymapEditor.setBounds (work.withSizeKeepingCentre (std::max (L::keymapEditorMinWidth, width), std::max (L::keymapEditorMinHeight, height)));
            // G2.18: the undo history window — the same centred law, narrower.
            const int historyWidth = std::min (L::undoHistoryMaxWidth, work.getWidth() - L::keymapEditorMargin);
            const int historyHeight = std::min (L::undoHistoryMaxHeight, work.getHeight() - L::keymapEditorMargin);
            undoHistory.setBounds (work.withSizeKeepingCentre (std::max (L::keymapEditorMinWidth, historyWidth), std::max (L::keymapEditorMinHeight, historyHeight)));
            // G4.1 cp2: the FX editor — the same centred law, sized for its parameter rows.
            const int fxWidth = std::min (L::fxEditorMaxWidth, work.getWidth() - L::keymapEditorMargin);
            const int fxHeight = std::min (L::fxEditorMaxHeight, work.getHeight() - L::keymapEditorMargin);
            fxEditor.setBounds (work.withSizeKeepingCentre (std::max (L::fxEditorMinWidth, fxWidth), std::max (L::fxEditorMinHeight, fxHeight)));
        }
        pianoRollInput.setBounds (mixerPanelBounds());   // G2.1 cp2: the piano roll is a dock tab
        pianoRollLaneChooser.setBounds (pianoRollControlLaneChooserArea (pianoRollCanvasGeometry (pianoRollInput.getBounds().withZeroOrigin()))
                                            .translated (pianoRollInput.getX(), pianoRollInput.getY()));   // G3.3
        {
            // G3.6: the header toggles sit after the "PIANO ROLL" label on the header row.
            using L = yesdaw::ui::UiTheme::Layout;
            auto header = pianoRollInput.getBounds().withHeight (L::pianoRollHeaderHeight)
                              .withTrimmedLeft (L::pianoRollHeaderButtonLeft)
                              .reduced (L::pianoRollHeaderButtonInsetX, L::pianoRollHeaderButtonInsetY);
            pianoRollTypingButton.setBounds (header.removeFromLeft (L::pianoRollHeaderButtonWidth));
            header.removeFromLeft (L::pianoRollHeaderButtonGap);
            pianoRollStepButton.setBounds (header.removeFromLeft (L::pianoRollHeaderButtonWidth));
            // G3.8: the Key / Scale choosers follow the toggles.
            header.removeFromLeft (L::pianoRollHeaderButtonGap);
            pianoRollKeyChooser.setBounds (header.removeFromLeft (L::pianoRollHeaderChooserWidth));
            header.removeFromLeft (L::pianoRollHeaderButtonGap);
            pianoRollScaleChooser.setBounds (header.removeFromLeft (L::pianoRollHeaderChooserWidth));
        }
        instrumentPanel.setBounds (mixerPanelBounds());   // G3.1: so is the instrument panel
        trackListInput.setBounds (leftRailPanelBounds());
        mixerStripsInput.setBounds (mixerPanelBounds());   // G4.1 cp2: the strips are the whole dock
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
        }
        layoutAutomationLaneControls();
        layoutInspectorControls();
        layoutMixerControls();
        hideMixerControlsBehindDockTab();   // G2.1 cp2
    }

private:
    template <typename Component>
    void configureActionComponent (Component& component,
                                   yesdaw::ui::UiActionId action,
                                   const juce::String& fallbackName)
    {
        actionComponents.emplace_back (&component, action);   // G1.6: tooltips follow the live keymap
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
        mixerDockToggle.setButtonText ("X");   // G2.1 cp3: the view cluster's X (tooltip carries the name + chord)
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

        // G2.16: the zoom slider (log2 of the factor, 0..6 = 1x..64x) drives the ONE zoom law at the playhead.
        timelineZoomSlider.setComponentID ("timeline.zoom.slider");
        timelineZoomSlider.setName ("Timeline zoom");
        timelineZoomSlider.setTitle ("Timeline zoom");
        timelineZoomSlider.setTooltip ("Timeline zoom: drag to zoom at the playhead (1x fits the project)");
        timelineZoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        timelineZoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, false,
                                            yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                            yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        timelineZoomSlider.setRange (0.0, std::log2 (timelineZoomCeiling()), 0.01);
        timelineZoomSlider.setValue (0.0, juce::dontSendNotification);
        timelineZoomSlider.onValueChange = [this] {
            if (refreshingZoomSlider)
                return;
            const double wanted = std::exp2 (timelineZoomSlider.getValue());
            const double playheadSeconds = appModel.project().sampleRate.isValid()
                ? static_cast<double> (std::max<std::int64_t> (0, appModel.context().playheadFrame)) / appModel.project().sampleRate.hz
                : 0.0;
            zoomTimelineAtAnchor (playheadSeconds, wanted / std::max (1.0e-9, timelineZoomFactor));
            repaintAll();
        };
        addAndMakeVisible (timelineZoomSlider);

        // G2.16: real scroll bars — horizontal in seconds under the timeline, vertical in rows beside it.
        timelineHScroll.setComponentID ("timeline.scroll.h");
        timelineHScroll.setName ("Timeline scroll");
        timelineHScroll.setTitle ("Timeline scroll");
        timelineHScroll.setTooltip ("Scroll the timeline in time (drag the thumb; the wheel over the timeline scrolls too)");
        timelineHScroll.setAutoHide (false);
        timelineHScroll.addListener (this);
        addAndMakeVisible (timelineHScroll);
        timelineVScroll.setComponentID ("timeline.scroll.v");
        timelineVScroll.setName ("Track scroll");
        timelineVScroll.setTitle ("Track scroll");
        timelineVScroll.setTooltip ("Scroll the tracks (drag the thumb)");
        timelineVScroll.setAutoHide (false);
        timelineVScroll.addListener (this);
        addAndMakeVisible (timelineVScroll);

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
        automationLaneToggle.setButtonText ("A");   // G2.1 cp3: the view cluster's A (the tooltip carries the name + chord)
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

        // G2.14: the marker list (every marker, tick order, seconds readout); a click locates.
        inspectorMarkerList.setComponentID ("clip.inspector.markers");
        inspectorMarkerList.setName ("Markers");
        inspectorMarkerList.setTitle ("Markers");
        inspectorMarkerList.setTooltip ("Markers: click one to move the playhead there");
        inspectorMarkerList.setRowHeight (yesdaw::ui::UiTheme::Layout::inspectorMarkerRowHeight);
        inspectorMarkerListModel.rowCount = [this] { return static_cast<int> (appModel.project().markers.size()); };
        inspectorMarkerListModel.rowText = [this] (int row) -> juce::String
        {
            const auto& markers = appModel.project().markers;
            if (row < 0 || row >= static_cast<int> (markers.size()) || ! appModel.project().sampleRate.isValid())
                return {};
            const yesdaw::engine::Marker& marker = markers[static_cast<std::size_t> (row)];
            const double seconds = static_cast<double> (marker.tick) / appModel.project().sampleRate.hz;
            return juce::String (marker.name) + "   " + juce::String (seconds, 3) + " s";
        };
        inspectorMarkerListModel.onRowClicked = [this] (int row)
        {
            const auto& markers = appModel.project().markers;
            if (row < 0 || row >= static_cast<int> (markers.size()))
                return;
            (void) appModel.locatePlaybackFrame (markers[static_cast<std::size_t> (row)].tick);
            refreshActionState();
            repaintAll();
        };
        inspectorMarkerList.setModel (&inspectorMarkerListModel);
        addAndMakeVisible (inspectorMarkerList);

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

        // G2.9b: the Stretch field — percent of the source length (100 = unstretched).
        inspectorStretch.setComponentID (kInspectorStretchComponentId);
        inspectorStretch.setName ("Clip stretch");
        inspectorStretch.setTitle ("Clip stretch");
        inspectorStretch.setTooltip ("Clip stretch: percent of the source length (50..200)");
        inspectorStretch.setSliderStyle (juce::Slider::LinearHorizontal);
        inspectorStretch.setTextBoxStyle (juce::Slider::NoTextBox,
                                          false,
                                          yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                          yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        inspectorStretch.setRange (yesdaw::ui::UiTheme::Layout::inspectorStretchSliderMin,
                                   yesdaw::ui::UiTheme::Layout::inspectorStretchSliderMax,
                                   yesdaw::ui::UiTheme::Layout::inspectorStretchSliderInterval);
        inspectorStretch.setValue (yesdaw::ui::UiTheme::Layout::inspectorStretchSliderDefault,
                                   juce::dontSendNotification);
        inspectorStretch.onValueChange = [this] {
            if (refreshingInspectorControls || ! inspectorStretch.isEnabled())
                return;

            (void) appModel.setSelectedTimelineClipStretchFactor (static_cast<float> (inspectorStretch.getValue() / 100.0));
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (inspectorStretch);

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
        inspectorFadeCurve.addItem ("Linear", kInspectorLinearFadeCurveId);      // G2.10
        inspectorFadeCurve.addItem ("S-curve", kInspectorSCurveFadeCurveId);
        inspectorFadeCurve.addItem ("Log", kInspectorLogFadeCurveId);
        inspectorFadeCurve.setSelectedId (kInspectorEqualPowerFadeCurveId, juce::dontSendNotification);
        inspectorFadeCurve.onChange = [this] {
            if (refreshingInspectorControls || ! inspectorFadeCurve.isEnabled())
                return;

            // G2.10: the chooser sets BOTH ends' shape; the curve amounts stay.
            const yesdaw::engine::FadeShape shape = fadeShapeForInspectorId (inspectorFadeCurve.getSelectedId());
            if (const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId()))
                (void) appModel.setSelectedTimelineClipFadeShapes (shape, clip->fadeInCurve, shape, clip->fadeOutCurve);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (inspectorFadeCurve);

        // G2.10: the curve amount — one row for both ends (-100..100 = the engine's -1..1).
        inspectorFadeCurveAmount.setComponentID (kInspectorFadeCurveAmountComponentId);
        inspectorFadeCurveAmount.setName ("Clip fade curve amount");
        inspectorFadeCurveAmount.setTitle ("Clip fade curve amount");
        inspectorFadeCurveAmount.setTooltip ("Clip fade curve amount: bends both fades (-100 slow rise .. 100 fast rise)");
        inspectorFadeCurveAmount.setSliderStyle (juce::Slider::LinearHorizontal);
        inspectorFadeCurveAmount.setTextBoxStyle (juce::Slider::NoTextBox,
                                                  false,
                                                  yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                                  yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        inspectorFadeCurveAmount.setRange (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountMin,
                                           yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountMax,
                                           yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountInterval);
        inspectorFadeCurveAmount.setValue (0.0, juce::dontSendNotification);
        inspectorFadeCurveAmount.onValueChange = [this] {
            if (refreshingInspectorControls || ! inspectorFadeCurveAmount.isEnabled())
                return;

            const auto amount = static_cast<float> (inspectorFadeCurveAmount.getValue() / 100.0);
            if (const yesdaw::engine::Clip* const clip = findProjectClipById (appModel.selectedTimelineClipId()))
                (void) appModel.setSelectedTimelineClipFadeShapes (clip->fadeInShape, amount, clip->fadeOutShape, amount);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (inspectorFadeCurveAmount);
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

    // G2.10: the chooser id <-> engine shape.
    [[nodiscard]] static yesdaw::engine::FadeShape fadeShapeForInspectorId (int id) noexcept
    {
        switch (id)
        {
            case kInspectorLinearFadeCurveId: return yesdaw::engine::FadeShape::Linear;
            case kInspectorSCurveFadeCurveId: return yesdaw::engine::FadeShape::SCurve;
            case kInspectorLogFadeCurveId:    return yesdaw::engine::FadeShape::Log;
            default:                          return yesdaw::engine::FadeShape::EqualPower;
        }
    }

    [[nodiscard]] static int inspectorIdForFadeShape (yesdaw::engine::FadeShape shape) noexcept
    {
        switch (shape)
        {
            case yesdaw::engine::FadeShape::Linear:     return kInspectorLinearFadeCurveId;
            case yesdaw::engine::FadeShape::SCurve:     return kInspectorSCurveFadeCurveId;
            case yesdaw::engine::FadeShape::Log:        return kInspectorLogFadeCurveId;
            case yesdaw::engine::FadeShape::EqualPower: break;
        }
        return kInspectorEqualPowerFadeCurveId;
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
        // G4.1: no "select first track" button — the strip's header IS the name (a click selects,
        // a double-click renames), as on every reference mixer.

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
        mixerStripsInput.trackCountProvider = [this] {   // G4.1
            return static_cast<int> (currentMixerSurface().tracks.size());
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
        // A double-click on a strip's name band renames it inline: a bus through its own editor,
        // a track through the rail's editor placed over the strip (until 2026-09-04 only buses
        // renamed here; a track strip's double-click did nothing).
        mixerStripsInput.onStripDoubleClicked = [this] (int stripIndex) {
            const auto surface = currentMixerSurface();
            const int trackCount = static_cast<int> (surface.tracks.size());
            const int busCount = static_cast<int> (surface.buses.size());
            if (stripIndex < 0 || stripIndex >= trackCount + busCount)
                return;

            if (stripIndex < trackCount)
            {
                openTrackRenameEditorOverStrip (stripIndex);
                return;
            }
            openBusRenameEditor (stripIndex - trackCount, stripIndex);
        };
        mixerStripsInput.meterStripAtPosition = [this] (juce::Point<int> positionInShell) {
            // E22: bus meters are clickable like track meters — the index spans tracks then buses.
            const std::size_t stripTotal = appModel.context().projectLoaded
                                               ? appModel.project().tracks.size()
                                                     + appModel.project().buses.size()
                                               : 0u;
            for (std::size_t i = 0; i < stripTotal; ++i)
                if (paintedMeterBoundsForLane (paintedMixerLaneBounds (i), stripIoRows (i)).contains (positionInShell))
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
                const std::size_t cellCount = stripCellCount (i);   // G4.1: S / M / R on a Track, S / M on a Bus
                for (std::size_t cell = 0; cell < cellCount; ++cell)
                    if (paintedMuteSoloCellBoundsForLane (lane, cell, cellCount).contains (positionInShell))
                        return std::pair<int, int> { static_cast<int> (i), static_cast<int> (cell) };
            }
            return std::pair<int, int> { -1, -1 };
        };
        // 2026-09-04 sweep: every strip's painted fader and pan drag their OWN strip (the selected
        // strip carries the live controls; the others only looked draggable). The press retargets
        // the mixer to that strip, then the drag rides the same scalar verbs the control lane
        // uses, coalesced into one undo step (beginStripGesture ... endStripGesture).
        mixerStripsInput.faderRailAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            const std::size_t trackCount = surface.tracks.size();
            const std::size_t stripTotal = trackCount + surface.buses.size();
            for (std::size_t i = 0; i < stripTotal; ++i)
            {
                const float gain = i < trackCount ? surface.tracks[i].linearGain
                                                  : surface.buses[i - trackCount].linearGain;
                if (paintedFaderThumbForLane (paintedMixerLaneBounds (i), gain, stripIoRows (i))
                        .expanded (0, yesdaw::ui::UiTheme::Layout::trackListLevelHitSlopX)
                        .contains (positionInShell))
                    return static_cast<int> (i);
            }
            return -1;
        };
        mixerStripsInput.faderGainForPosition = [this] (int stripIndex, juce::Point<int> positionInShell) {
            const auto rail = paintedFaderRailForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                                       stripIoRows (static_cast<std::size_t> (stripIndex)));
            if (rail.getHeight() <= 0)
                return 1.0f;
            const float fraction = juce::jlimit (0.0f, 1.0f,
                                                 static_cast<float> (rail.getBottom() - positionInShell.y)
                                                     / static_cast<float> (rail.getHeight()));
            return fraction * static_cast<float> (yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax);
        };
        mixerStripsInput.panKnobAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
            for (std::size_t i = 0; i < stripTotal; ++i)
                if (paintedPanKnobForLane (paintedMixerLaneBounds (i)).contains (positionInShell))
                    return static_cast<int> (i);
            return -1;
        };
        mixerStripsInput.panForPosition = [this] (int stripIndex, juce::Point<int> positionInShell) {
            const auto knob = paintedPanKnobForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)));
            if (knob.getWidth() <= 0)
                return 0.0f;
            const float normalized = static_cast<float> (positionInShell.x - knob.getX())
                                   / static_cast<float> (knob.getWidth());
            return juce::jlimit (-1.0f, 1.0f, normalized + normalized - 1.0f);
        };
        const auto retargetMixerStrip = [this] (int stripIndex) {
            const auto surface = currentMixerSurface();
            const int trackCount = static_cast<int> (surface.tracks.size());
            if (stripIndex < trackCount)
            {
                selectedTrackLane = stripIndex;   // the rail follows, as a strip click does
                return appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex), false);
            }
            return appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount), false);
        };
        // G4.1 cp2: the painted drags are THE fader and pan now (the lane's live sliders are gone), so
        // they carry what those did: one undo step per drag, the dB readout, and the Touch / Latch ride
        // (R15 / N5 — an armed ride buffers the points and commits ONE lane edit on release).
        mixerStripsInput.onFaderDragged = [this, retargetMixerStrip] (int stripIndex, float linearGain, bool ended) {
            const bool pressed = paintedFaderDragStrip != stripIndex;
            appModel.beginStripGesture();
            if (retargetMixerStrip (stripIndex))
            {
                if (pressed)
                {
                    paintedFaderDragStrip = stripIndex;
                    beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::TrackFader,
                                                     yesdaw::engine::FaderNode::kGainParameterId);
                }
                if (automationTouchRideActive)
                    recordAutomationTouchSample (automationNormalizedForFaderGain (linearGain));
                else
                    (void) appModel.setSelectedMixerFader (linearGain);
                showDragDbReadout (paintedFaderRailForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                                            stripIoRows (static_cast<std::size_t> (stripIndex))),
                                   linearGain);
            }
            if (ended)
            {
                paintedFaderDragStrip = -1;
                endAutomationTouchRideIfActive();
                appModel.endStripGesture();
                hideDragDbReadout();
            }
            refreshActionState();
            resized();
            repaintAll();
        };
        mixerStripsInput.onPanDragged = [this, retargetMixerStrip] (int stripIndex, float pan, bool ended) {
            const bool pressed = paintedPanDragStrip != stripIndex;
            appModel.beginStripGesture();
            if (retargetMixerStrip (stripIndex))
            {
                if (pressed)
                {
                    paintedPanDragStrip = stripIndex;
                    beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::TrackPan,
                                                     yesdaw::engine::PanNode::kPanParameterId);
                }
                if (automationTouchRideActive)
                    recordAutomationTouchSample (automationNormalizedForPan (pan));
                else
                    (void) appModel.setSelectedMixerPan (pan);
            }
            if (ended)
            {
                paintedPanDragStrip = -1;
                endAutomationTouchRideIfActive();
                appModel.endStripGesture();
            }
            refreshActionState();
            resized();
            repaintAll();
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
                if (cellIndex == static_cast<int> (kMixerPaintedTrackCellCount) - 1)
                {
                    // G4.1: the R cell — the arm set (M11), on THAT track; transient like the rail's badge.
                    (void) appModel.toggleRecordingArmForTrack (strip);
                    refreshActionState();
                    repaintAll();
                    return;
                }
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
                    if (paintedInsertRowBoundsForLane (lane, slot, stripIoRows (i)).contains (positionInShell))
                        return std::pair<int, int> { static_cast<int> (i), static_cast<int> (slot) };
            }
            return std::pair<int, int> { -1, -1 };
        };
        // G4.1: the I/O slots — the click selects the strip and opens the slot's choices as a menu
        // (headless: the record the harness reads, like every other menu).
        mixerStripsInput.ioRowAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
            for (std::size_t i = 0; i < stripTotal; ++i)
            {
                const auto lane = paintedMixerLaneBounds (i);
                const int ioRows = stripIoRows (i);
                if (paintedInputRowBoundsForLane (lane, ioRows).contains (positionInShell))
                    return std::pair<int, int> { static_cast<int> (i), kMixerIoInputRow };
                if (paintedOutputRowBoundsForLane (lane, ioRows).contains (positionInShell))
                    return std::pair<int, int> { static_cast<int> (i), kMixerIoOutputRow };
            }
            return std::pair<int, int> { -1, -1 };
        };
        mixerStripsInput.onIoRowClicked = [this] (int stripIndex, int row, juce::Point<int> positionInStrips) {
            if (mixerStripsInput.onStripClicked)
                mixerStripsInput.onStripClicked (stripIndex);
            openContextMenu (row == kMixerIoInputRow ? yesdaw::ui::ContextMenuTarget::MixerStripInput
                                                     : yesdaw::ui::ContextMenuTarget::MixerStripOutput,
                             stripIndex, mixerStripsInput, positionInStrips);
        };
        mixerStripsInput.insertSlotFilled = [this] (int strip, int slot) {
            const auto surface = currentMixerSurface();
            const std::size_t trackCount = surface.tracks.size();
            const auto stripIndex = static_cast<std::size_t> (strip);
            if (strip < 0 || stripIndex >= trackCount + surface.buses.size())
                return false;
            const auto& state = stripIndex < trackCount ? surface.tracks[stripIndex]
                                                        : surface.buses[stripIndex - trackCount];
            return slot >= 0 && static_cast<std::size_t> (slot) < state.fxSlots.size();
        };
        // G4.1 cp2: an empty send well is the add menu; the values the fine-drag anchors read.
        mixerStripsInput.sendRowFilled = [this] (int strip, int row) {
            const auto surface = currentMixerSurface();
            const std::size_t trackCount = surface.tracks.size();
            const auto stripIndex = static_cast<std::size_t> (strip);
            if (strip < 0 || stripIndex >= trackCount + surface.buses.size())
                return false;
            const auto& state = stripIndex < trackCount ? surface.tracks[stripIndex]
                                                        : surface.buses[stripIndex - trackCount];
            return row >= 0 && static_cast<std::size_t> (row) < state.sends.size();
        };
        mixerStripsInput.faderGainForStrip = [this] (int strip) {
            const auto surface = currentMixerSurface();
            const std::size_t trackCount = surface.tracks.size();
            const auto stripIndex = static_cast<std::size_t> (juce::jmax (0, strip));
            if (stripIndex < trackCount)
                return surface.tracks[stripIndex].linearGain;
            if (stripIndex - trackCount < surface.buses.size())
                return surface.buses[stripIndex - trackCount].linearGain;
            return 1.0f;
        };
        mixerStripsInput.panForStrip = [this] (int strip) {
            const auto surface = currentMixerSurface();
            const std::size_t trackCount = surface.tracks.size();
            const auto stripIndex = static_cast<std::size_t> (juce::jmax (0, strip));
            if (stripIndex < trackCount)
                return surface.tracks[stripIndex].pan;
            if (stripIndex - trackCount < surface.buses.size())
                return surface.buses[stripIndex - trackCount].pan;
            return 0.0f;
        };
        mixerStripsInput.sendLevelForRow = [this] (int strip, int row) {
            const auto surface = currentMixerSurface();
            const std::size_t trackCount = surface.tracks.size();
            const auto stripIndex = static_cast<std::size_t> (juce::jmax (0, strip));
            const yesdaw::ui::UiMixerStrip* state = stripIndex < trackCount ? &surface.tracks[stripIndex]
                                                  : stripIndex - trackCount < surface.buses.size() ? &surface.buses[stripIndex - trackCount]
                                                                                                    : nullptr;
            if (state == nullptr || row < 0 || static_cast<std::size_t> (row) >= state->sends.size())
                return 1.0f;
            return state->sends[static_cast<std::size_t> (row)].linearGain;
        };
        // G4.1 cp2: a filled slot's double-click opens the editor on THAT slot.
        mixerStripsInput.onInsertSlotDoubleClicked = [this] (int stripIndex, int slotIndex) {
            openFxEditor (stripIndex, slotIndex);
        };
        // M5: painted send rows. The press selects the strip and previews the level; the release
        // commits ONE undoable SetSendLevel through the same model verb the control lane uses.
        // G4.1 cp2: a Bus strip's rows drag too (R13: sends originate on Tracks AND Buses), and the
        // drag rides Touch / Latch (R15: the SendLevel lane, the live slider's law).
        mixerStripsInput.sendRowAtPosition = [this] (juce::Point<int> positionInShell) {
            const auto surface = currentMixerSurface();
            const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
            for (std::size_t i = 0; i < stripTotal; ++i)
            {
                const auto lane = paintedMixerLaneBounds (i);
                const int ioRows = stripIoRows (i);
                for (std::size_t sendIndex = 0;
                     sendIndex < static_cast<std::size_t> (paintedSendRowCountForLane (lane, ioRows));
                     ++sendIndex)
                    if (paintedSendRowBoundsForLane (lane, sendIndex, ioRows).contains (positionInShell))
                        return std::pair<int, int> { static_cast<int> (i), static_cast<int> (sendIndex) };
            }
            return std::pair<int, int> { -1, -1 };
        };
        mixerStripsInput.sendLevelForPosition = [this] (int stripIndex, int sendIndex, juce::Point<int> positionInShell) {
            const auto row = paintedSendRowBoundsForLane (
                paintedMixerLaneBounds (static_cast<std::size_t> (juce::jmax (0, stripIndex))),
                static_cast<std::size_t> (juce::jmax (0, sendIndex)),
                stripIoRows (static_cast<std::size_t> (juce::jmax (0, stripIndex))));
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
            const int busCount = static_cast<int> (surface.buses.size());
            if (stripIndex < 0 || stripIndex >= trackCount + busCount || sendIndex < 0)
                return;
            const yesdaw::ui::UiMixerStrip& state = stripIndex < trackCount
                ? surface.tracks[static_cast<std::size_t> (stripIndex)]
                : surface.buses[static_cast<std::size_t> (stripIndex - trackCount)];
            if (static_cast<std::size_t> (sendIndex) >= state.sends.size())
                return;                                   // an empty send well has nothing to set

            const bool pressed = paintedSendDragPreview.stripIndex != stripIndex
                              || paintedSendDragPreview.sendIndex != sendIndex;
            if (stripIndex < trackCount)
            {
                (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
                selectedTrackLane = stripIndex;
            }
            else
                (void) appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount));
            if (pressed)
                beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::SendLevel,
                                                 static_cast<std::uint32_t> (sendIndex));
            if (! commit)
            {
                paintedSendDragPreview = { stripIndex, sendIndex, static_cast<float> (level) };
                if (automationTouchRideActive)
                    recordAutomationTouchSample (automationNormalizedForFaderGain (level));
                repaintAll();
                return;
            }

            paintedSendDragPreview = {};
            if (automationTouchRideActive)
            {
                recordAutomationTouchSample (automationNormalizedForFaderGain (level));
                endAutomationTouchRideIfActive();   // the ride is the edit (one undo step)
            }
            else
                (void) appModel.setSendLevelOnSelectedTrack (static_cast<std::size_t> (sendIndex),
                                                            static_cast<float> (level));
            layoutMixerControls();
            refreshActionState();
            repaintAll();
        };
        mixerStripsInput.onInsertSlotClicked = [this] (int stripIndex, int slotIndex) {
            lastContextMenu = {};   // a slot click opens no menu of its own (the empty slot's add menu follows separately)
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

            // The click selects the slot (the strip paints it selected); an empty slot selects nothing.
            // G4.1 cp2: the editor, when open, follows the selected slot of the selected strip.
            const std::size_t chainSize = appModel.selectedStripFxChain().size();
            selectedFxParamSlot = static_cast<std::size_t> (slotIndex) < chainSize ? slotIndex : -1;
            selectedFxParamPage = 0;
            fxEditorStripOrdinal = appModel.selectedMixerStripOrdinal();
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

        // G4.1 cp2: the lane's Add FX chooser and slot rows are gone — an empty painted slot's click is
        // the add menu, a filled slot's double-click the editor, its right-click the slot menu.
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

        // G4.1 cp2: + Bus / - Bus are the strip menus' Add Bus / Remove Bus (cp1).
        // E17: inline bus rename editor, the marker/clip editor pattern on the mixer panel.
        busRenameEditor.setComponentID ("shell.mixer.bus.rename");
        busRenameEditor.setTooltip ("Rename bus: Enter commits, Escape cancels");
        busRenameEditor.setName ("Rename bus");
        busRenameEditor.setSelectAllWhenFocused (true);
        busRenameEditor.onReturnKey = [this] { commitBusRenameEditor(); };
        busRenameEditor.onEscapeKey = [this] { dismissBusRenameEditor(); };
        busRenameEditor.onFocusLost = [this] { dismissBusRenameEditor(); };
        addChildComponent (busRenameEditor);

        // G4.1 cp2: + Send, Out: and the send rows are the strip's wells and slots (an empty send well's
        // click adds; the routed row drags its level and right-clicks its menu; the OUTPUT slot routes).

        // G4.1 cp2: the FX editor hosts the parameter rows; Bypass and Close act on the slot it shows.
        fxEditor.onClose = [this] { closeFxEditor(); };
        fxEditor.onBypass = [this] {
            if (selectedFxParamSlot < 0)
                return;
            (void) appModel.toggleFxInsertEnabledOnSelectedStrip (static_cast<std::size_t> (selectedFxParamSlot));
            refreshActionState();
            repaintAll();
        };
        addChildComponent (fxEditor);

        // FX parameter editing (usable-DAW P1): the selected slot's ParamSpecs become live sliders;
        // every committed value is one undoable SetFxInsertParam through the model.
        for (std::size_t index = 0; index < mixerFxParamSliders.size(); ++index)
        {
            auto& label = mixerFxParamLabels[index];
            label.setComponentID ("mixer.fx.param." + juce::String (static_cast<int> (index)) + ".label");
            label.setTooltip ("FX parameter " + juce::String (static_cast<int> (index) + 1) + " readout");
            label.setColour (juce::Label::textColourId, kText);
            label.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
            label.setInterceptsMouseClicks (false, false);
            fxEditor.addChildComponent (label);   // G4.1 cp2: the rows live in the editor

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
            fxEditor.addChildComponent (slider);

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
            fxEditor.addChildComponent (choiceChooser);
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
        fxEditor.addChildComponent (mixerFxParamPageChooser);

        // G4.1 cp2: the lane's live fader and pan are gone — the painted fader rail and pan knob on
        // EVERY strip drag the same verbs (Shift fine, Alt-click resets, the Touch / Latch ride).
        dragDbReadout.setComponentID ("shell.drag.db");
        dragDbReadout.setTooltip ("Live gain in dB while dragging");
        dragDbReadout.setInterceptsMouseClicks (false, false);
        dragDbReadout.setJustificationType (juce::Justification::centred);
        dragDbReadout.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
        dragDbReadout.setColour (juce::Label::textColourId, kText);
        dragDbReadout.setColour (juce::Label::backgroundColourId,
                                 yesdaw::ui::UiTheme::Color::darkControl());
        addChildComponent (dragDbReadout);

        // G4.1 cp2: the lane's M / S buttons are gone — the painted cells on every strip (N1).
        // G4.1: the seven readout rows ("Audio 1 meters: peak n/a" …), the solo-safe button and the
        // first-track select button are gone — the strip carries every one of those as a painted
        // section or a menu verb (plan §8.2: delete before you add). The read verbs stay registered
        // for the harness.
    }

    // G1.4: the inspector's width right now — the token, or nothing while it is hidden (I).
    [[nodiscard]] int inspectorWidthNow() const noexcept
    {
        return appModel.context().inspectorVisible ? viewState.inspectorWidth : 0;
    }

    // G4.1 cp2: the FX editor opens on ONE slot of ONE strip (the double-click, the slot menu's Open
    // Editor, the harness) and closes on Close / Escape / the strip or slot going away (the refresh law).
    void openFxEditor (int stripIndex, int slotIndex)
    {
        const auto surface = currentMixerSurface();
        const int trackCount = static_cast<int> (surface.tracks.size());
        const int busCount = static_cast<int> (surface.buses.size());
        if (stripIndex < 0 || stripIndex > trackCount + busCount || slotIndex < 0)
            return;
        if (stripIndex < trackCount)
        {
            (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
            selectedTrackLane = stripIndex;
        }
        else if (stripIndex < trackCount + busCount)
            (void) appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount));
        else
            (void) appModel.selectMixerMaster();   // the lane past the buses (R11: the master's chain)
        if (static_cast<std::size_t> (slotIndex) >= appModel.selectedStripFxChain().size())
            return;   // an empty slot has nothing to edit
        selectedFxParamSlot = slotIndex;
        selectedFxParamPage = 0;
        fxEditorStripOrdinal = appModel.selectedMixerStripOrdinal();
        fxEditorOpen = true;
        refreshActionState();
        resized();
        repaintAll();
    }

    void closeFxEditor()
    {
        if (! fxEditorOpen)
            return;
        fxEditorOpen = false;
        refreshActionState();
        resized();
        repaintAll();
    }

    // G2.1: the splitters set these; each clamps to the plan's §3.4 ranges, lays out and repaints.
    void setRailWidth (int width)
    {
        viewState.railWidth = juce::jlimit (yesdaw::ui::UiTheme::Layout::leftRailMinWidth,
                                            yesdaw::ui::UiTheme::Layout::leftRailMaxWidth, width);
        resized();
        repaintAll();
    }

    void setInspectorWidth (int width)
    {
        viewState.inspectorWidth = juce::jlimit (yesdaw::ui::UiTheme::Layout::inspectorMinWidth,
                                                 yesdaw::ui::UiTheme::Layout::inspectorMaxWidth, width);
        resized();
        repaintAll();
    }

    void setDockHeight (int height)
    {
        viewState.dockHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::editorDockMinHeight, height);
        resized();
        repaintAll();
    }

    // The view state follows the project: a different bundle loads its own record (or the
    // defaults); every splitter release writes the record. Polled from refreshActionState so
    // every open / new / restore path is covered by the one law.
    void loadViewStateIfBundleChanged()
    {
        const std::filesystem::path& bundle = appModel.bundlePath();
        if (bundle == viewStateBundle)
            return;
        viewStateBundle = bundle;
        viewState = {};
        appModel.setMixerStripsNarrow (false);   // G4.1: the record's default
        for (const juce::String& line : juce::StringArray::fromLines (juce::String (appModel.readViewStateRecord())))
        {
            const juce::String key = line.upToFirstOccurrenceOf ("\t", false, false);
            const int value = line.fromFirstOccurrenceOf ("\t", false, false).getIntValue();
            if (key == "rail")
                viewState.railWidth = juce::jlimit (yesdaw::ui::UiTheme::Layout::leftRailMinWidth,
                                                    yesdaw::ui::UiTheme::Layout::leftRailMaxWidth, value);
            else if (key == "inspector")
                viewState.inspectorWidth = juce::jlimit (yesdaw::ui::UiTheme::Layout::inspectorMinWidth,
                                                         yesdaw::ui::UiTheme::Layout::inspectorMaxWidth, value);
            else if (key == "dock")
                viewState.dockHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::editorDockMinHeight, value);
            else if (key == "narrow")
                appModel.setMixerStripsNarrow (value != 0);   // G4.1
        }
        resized();
    }

    // G2.1 cp2: which editor tab the dock shows.
    [[nodiscard]] bool dockShowsMixer() const noexcept
    {
        return appModel.context().mixerDockVisible
            && appModel.context().editorDockTab == yesdaw::ui::UiEditorDockTab::Mixer;
    }

    [[nodiscard]] bool dockShowsPianoRoll() const noexcept
    {
        return appModel.context().mixerDockVisible
            && appModel.context().editorDockTab == yesdaw::ui::UiEditorDockTab::PianoRoll;
    }

    [[nodiscard]] bool dockShowsInstrument() const noexcept   // G3.1
    {
        return appModel.context().mixerDockVisible
            && appModel.context().editorDockTab == yesdaw::ui::UiEditorDockTab::Instrument;
    }

    [[nodiscard]] juce::Component* toolbarButtonFor (yesdaw::ui::UiActionId action)
    {
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size() && i < toolbarActions.size(); ++i)
            if (toolbarActions[i] == action)
                return &buttons[i];
        return nullptr;
    }

    void setToolbarButtonBounds (yesdaw::ui::UiActionId action, juce::Rectangle<int> bounds)
    {
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size() && i < toolbarActions.size(); ++i)
            if (toolbarActions[i] == action)
                buttons[i].setBounds (bounds);
    }

    // The mixer's control lane (every widget the mixer tab owns). While another tab shows, they
    // are hidden as a set and restored to what their own laws last chose when the mixer returns:
    // restore at the start of every refresh / layout, hide at the end. The one list is the law —
    // the [dock-tabs] gate walks the dock rect and refuses any stray visible widget.
    [[nodiscard]] std::vector<juce::Component*> mixerLaneControls()
    {
        // G4.1 cp2: the strips' input surface, the master fader and the FX editor (its rows are its
        // children, so hiding it hides them) — everything the mixer tab shows that another tab must not.
        return { &mixerStripsInput, &mixerMasterFader, &fxEditor };
    }

    void restoreControlsHiddenByDockTab()
    {
        for (auto& [control, wasVisible] : hiddenByDockTab)
            control->setVisible (wasVisible);
        hiddenByDockTab.clear();
    }

    void hideMixerControlsBehindDockTab()
    {
        if (dockShowsMixer())
            return;
        for (juce::Component* control : mixerLaneControls())
        {
            hiddenByDockTab.try_emplace (control, control->isVisible());
            control->setVisible (false);
        }
    }

    [[nodiscard]] std::string viewStateRecordText() const
    {
        return "rail\t" + std::to_string (viewState.railWidth)
             + "\ninspector\t" + std::to_string (viewState.inspectorWidth)
             + "\ndock\t" + std::to_string (viewState.dockHeight)
             + "\nnarrow\t" + std::string (appModel.context().mixerStripsNarrow ? "1" : "0") + "\n";   // G4.1
    }

    void saveViewState()
    {
        appModel.writeViewStateRecord (viewStateRecordText());
    }

    // G2.5: what Zoom to Selection needs from the view, in one place.
    struct MainComponentSnapshotLike
    {
        double rangeStartSeconds = 0.0, rangeSeconds = 0.0, widthPixels = 0.0, fitPixelsPerSecond = 0.0;
    };
    [[nodiscard]] MainComponentSnapshotLike snapshotForZoom() const
    {
        MainComponentSnapshotLike out;
        const yesdaw::engine::Project& project = appModel.project();
        const std::int64_t start = appModel.timelineRangeStartFrame();
        const std::int64_t end = appModel.timelineRangeEndFrame();
        if (! project.sampleRate.isValid() || start < 0 || end <= start)
            return out;
        out.rangeStartSeconds = static_cast<double> (start) / project.sampleRate.hz;
        out.rangeSeconds = static_cast<double> (end - start) / project.sampleRate.hz;
        const juce::Rectangle<int> timeline = timelineBounds();
        out.widthPixels = static_cast<double> (juce::jmax (yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                                         timeline.getWidth() - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter));
        out.fitPixelsPerSecond = out.widthPixels / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds, timelineTotalSeconds);
        return out;
    }

    // G2.16: the timeline PANEL holds the canvas plus the scroll-bar strips below and beside it;
    // timelineBounds() is the canvas alone — the ONE rect the input, the playhead layer, the
    // toolbar cluster, the fit law and the probe share, so a pixel means the same time everywhere.
    [[nodiscard]] juce::Rectangle<int> timelinePanelBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
        work.removeFromBottom (dockedMixerHeight());
        work.removeFromLeft (viewState.railWidth);
        work.removeFromRight (inspectorWidthNow());
        return work.reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                             yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    }

    [[nodiscard]] juce::Rectangle<int> timelineBounds() const
    {
        return timelinePanelBounds()
            .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness)
            .withTrimmedRight (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness);
    }

    // The exact rect drawTrackList paints into; the rail input overlay shares it so hits match paint.
    [[nodiscard]] juce::Rectangle<int> leftRailPanelBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
        work.removeFromBottom (dockedMixerHeight());
        return work.removeFromLeft (viewState.railWidth)
                   .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                             yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    }

    // G2.17: multi-select — Ctrl toggles a lane in the set, Shift extends from the primary; a plain
    // click and the Up / Down verbs collapse to one. The primary stays the lane the strip verbs act on.
    void toggleTrackLaneSelection (int lane)
    {
        const int trackCount = static_cast<int> (appModel.project().tracks.size());
        if (! appModel.context().projectLoaded || lane < 0 || lane >= trackCount)
            return;
        if (selectedTrackLane >= 0 && selectedTrackLanes.empty())
            selectedTrackLanes.insert (selectedTrackLane);
        if (selectedTrackLanes.count (lane) > 0 && selectedTrackLanes.size() > 1)
            selectedTrackLanes.erase (lane);
        else
            selectedTrackLanes.insert (lane);
        dismissTrackRenameEditor();
        selectedTrackLane = lane;
        (void) appModel.selectMixerTrack (static_cast<std::size_t> (lane), /*showMixerPanel*/ false);
        refreshActionState();
        resized();   // G3.1: the inspector's TRACK tab lays out per selected Track (the instrument row)
        repaintAll();
    }

    void extendTrackLaneSelection (int lane)
    {
        const int trackCount = static_cast<int> (appModel.project().tracks.size());
        if (! appModel.context().projectLoaded || lane < 0 || lane >= trackCount)
            return;
        const int anchor = selectedTrackLane >= 0 ? selectedTrackLane : lane;
        selectedTrackLanes.clear();
        for (int row = std::min (anchor, lane); row <= std::max (anchor, lane); ++row)
            selectedTrackLanes.insert (row);
        dismissTrackRenameEditor();
        selectedTrackLane = lane;
        (void) appModel.selectMixerTrack (static_cast<std::size_t> (lane), /*showMixerPanel*/ false);
        refreshActionState();
        resized();   // G3.1: the inspector's TRACK tab lays out per selected Track (the instrument row)
        repaintAll();
    }

    [[nodiscard]] bool trackLaneIsSelected (int lane) const noexcept
    {
        return lane == selectedTrackLane || selectedTrackLanes.count (lane) > 0;
    }

    void selectTrackLane (int lane)
    {
        const int trackCount = static_cast<int> (appModel.project().tracks.size());
        if (! appModel.context().projectLoaded || lane < 0 || lane >= trackCount)
            return;

        dismissTrackRenameEditor();
        selectedTrackLanes.clear();   // G2.17: a plain selection is one lane
        selectedTrackLane = lane;
        (void) appModel.selectMixerTrack (static_cast<std::size_t> (lane), /*showMixerPanel*/ false);
        refreshActionState();
        resized();   // G3.1: the inspector's TRACK tab lays out per selected Track (the instrument row)
        repaintAll();
    }

    // G3.9: a WAV onto a Sampler pad — the WAV reader's refusal is the shell's to name (R6), every
    // other refusal the model's (R7).
    void loadSamplerPadFromPath (std::int16_t key, const std::filesystem::path& path)
    {
        auto decoded = decodeProjectWav (path);
        if (! decoded)
        {
            appModel.reportStatus ("Sampler pad refused (WAV only, stereo max): " + path.filename().string(), true);
            return;
        }
        if (appModel.importSamplerPadFromSource (path, std::move (*decoded), key).ok())
            recordLastAction (yesdaw::ui::UiActionId::SamplerPadLoad);
    }

    // G3.1: the instrument kind's name (the probe, the panel title, the inspector row).
    [[nodiscard]] static const char* instrumentKindName (yesdaw::engine::TrackInstrumentKind kind) noexcept
    {
        switch (kind)
        {
            case yesdaw::engine::TrackInstrumentKind::None: return "None (auto)";
            case yesdaw::engine::TrackInstrumentKind::SimpleSynth: return "SimpleSynth";
            case yesdaw::engine::TrackInstrumentKind::Sampler: return "Sampler";   // G3.9
        }
        return "?";
    }

    // G3.1: the panel's rows — one per ParamSpec of the selected Track's effective instrument,
    // the value read back from the project (an unset id shows the spec default).
    [[nodiscard]] std::vector<InstrumentPanelComponent::Row> instrumentPanelRows() const
    {
        std::vector<InstrumentPanelComponent::Row> rows;
        const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument();
        if (track == nullptr)
            return rows;
        for (std::uint32_t paramId = 1; paramId <= yesdaw::engine::SimpleSynthNode::kParameterCount; ++paramId)
        {
            if (! yesdaw::engine::instrumentKindAcceptsParameterId (track->instrumentKind, paramId))
                continue;
            const yesdaw::engine::ParamSpec spec = yesdaw::engine::instrumentParamSpecForKind (track->instrumentKind, paramId);
            InstrumentPanelComponent::Row row;
            row.paramId = paramId;
            row.label = juce::String (spec.name).fromLastOccurrenceOf (".", false, false).replaceCharacter ('_', ' ');
            row.normalized = track->instrumentParamNormalized (paramId);
            const double real = yesdaw::engine::mapNormalized (spec, row.normalized);
            row.readout = juce::String (real, real >= 100.0 ? 0 : 3) + (spec.unit[0] != '\0' ? juce::String (" ") + spec.unit : juce::String());
            rows.push_back (std::move (row));
        }
        return rows;
    }

    // G2.17: a track is a MIDI track when it holds MIDI clips; audio otherwise (an empty track is audio).
    [[nodiscard]] bool trackHoldsMidi (std::size_t trackIndex) const noexcept
    {
        const auto& tracks = appModel.project().tracks;
        if (trackIndex >= tracks.size())
            return false;
        for (const yesdaw::engine::MidiClip& clip : appModel.project().midiClips)
            if (clip.trackId == tracks[trackIndex].id)
                return true;
        return false;
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

    // The same editor and commit path as the rail's rename, placed over the mixer strip's name
    // band (the bus editor's law) — the strip's track becomes the selected lane first, since
    // commitTrackRenameEditor renames selectedTrackLane.
    void openTrackRenameEditorOverStrip (int stripOrdinal)
    {
        dismissClipRenameEditor();
        const auto& tracks = appModel.project().tracks;
        if (stripOrdinal < 0 || stripOrdinal >= static_cast<int> (tracks.size()))
            return;
        selectTrackLane (stripOrdinal);
        (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripOrdinal));

        const juce::Rectangle<int> band =
            mixerStripBounds (stripOrdinal).removeFromTop (yesdaw::ui::UiTheme::Layout::mixerTrackSelectHeight);
        if (band.isEmpty())
            return;

        trackRenameEditor.setBounds (band);
        trackRenameEditor.setText (juce::String (tracks[static_cast<std::size_t> (stripOrdinal)].strip.name),
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
        const yesdaw::ui::UiActionDispatchResult result =
            appModel.duplicateProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id);
        if (result.dispatched)
            selectTrackLane (selectedTrackLane + 1);   // the copy lands directly below the source
        else if (result.state.disabledReason != nullptr && result.state.disabledReason[0] != '\0')
            appModel.reportStatus (std::string ("Duplicate Track refused: ") + result.state.disabledReason, true);   // R4: a refusal says why

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
        if (! appModel.context().mixerDockVisible)
            return 0;
        // G2.1: the dragged height, never eating the arrangement below its minimum.
        const int maxDock = getHeight() - headerHeightNow() - yesdaw::ui::UiTheme::Layout::arrangeMinHeight;
        return juce::jlimit (yesdaw::ui::UiTheme::Layout::editorDockMinHeight,
                             juce::jmax (yesdaw::ui::UiTheme::Layout::editorDockMinHeight, maxDock),
                             viewState.dockHeight);
    }

    [[nodiscard]] juce::Rectangle<int> mixerPanelBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
        auto mixer = work.removeFromBottom (dockedMixerHeight());   // G2.1 cp2: always the dock
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
        const auto area = mixerPanelBounds();   // G4.1 cp2: no tools column — the strips start at the panel's edge

        const auto surface = currentMixerSurface();
        const std::size_t stripCount = surface.tracks.size() + surface.buses.size();
        // G4.1: View > Narrow Strips is ONE fixed width for every lane (the master's too).
        const int stripWidth = appModel.context().mixerStripsNarrow
            ? yesdaw::ui::UiTheme::Layout::mixerPaintedStripNarrowWidth
            : std::clamp (
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

    // G4.1: how many I/O slot rows a strip's KIND carries — a Track two (input and output), a Bus one
    // (output), the master none — and how many cells its S / M / R row has.
    [[nodiscard]] int stripIoRows (std::size_t stripIndex) const noexcept
    {
        if (! appModel.context().projectLoaded)
            return 0;
        const auto& project = appModel.project();
        if (stripIndex < project.tracks.size())
            return 2;
        if (stripIndex < project.tracks.size() + project.buses.size())
            return 1;
        return 0;
    }

    [[nodiscard]] std::size_t stripCellCount (std::size_t stripIndex) const noexcept
    {
        if (! appModel.context().projectLoaded)
            return 0;
        const auto& project = appModel.project();
        if (stripIndex < project.tracks.size())
            return kMixerPaintedTrackCellCount;
        if (stripIndex < project.tracks.size() + project.buses.size())
            return kMixerPaintedMuteSoloCellCount;
        return 0;
    }

    // G4.1: how many of the strip's I/O rows the lane can afford — after the inserts, before the sends.
    [[nodiscard]] static int paintedIoRowsShownForLane (juce::Rectangle<int> lane, int ioRows) noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const int available = lane.getHeight() - L::mixerPaintedInsertsTop
                            - L::mixerPaintedFaderBottomInset - L::mixerPaintedFaderMinHeight
                            - L::mixerPaintedInsertsFaderGap
                            - paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch;
        return available >= ioRows * L::mixerPaintedIoRowPitch ? ioRows : 0;
    }

    // The input row (a Track's) leads the slot column; the output row closes it.
    [[nodiscard]] static int paintedInputRowsForLane (juce::Rectangle<int> lane, int ioRows) noexcept
    {
        return paintedIoRowsShownForLane (lane, ioRows) == 2 ? 1 : 0;
    }

    [[nodiscard]] static juce::Rectangle<int> paintedIoRowRect (juce::Rectangle<int> lane, int top)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        return juce::Rectangle<int> (lane.getX() + L::mixerPaintedInsertsInsetX,
                                     top,
                                     juce::jmax (0, lane.getWidth() - 2 * L::mixerPaintedInsertsInsetX),
                                     L::mixerPaintedIoRowHeight);
    }

    [[nodiscard]] static juce::Rectangle<int> paintedInputRowBoundsForLane (juce::Rectangle<int> lane, int ioRows)
    {
        if (paintedInputRowsForLane (lane, ioRows) == 0)
            return {};
        return paintedIoRowRect (lane, lane.getY() + yesdaw::ui::UiTheme::Layout::mixerPaintedInsertsTop);
    }

    [[nodiscard]] static juce::Rectangle<int> paintedOutputRowBoundsForLane (juce::Rectangle<int> lane, int ioRows)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        if (paintedIoRowsShownForLane (lane, ioRows) == 0)
            return {};
        const int top = lane.getY() + paintedSendsTopForLane (lane, ioRows)
                      + paintedSendRowCountForLane (lane, ioRows) * L::mixerPaintedSendRowPitch;
        return paintedIoRowRect (lane, top);
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

    [[nodiscard]] static juce::Rectangle<int> paintedFaderRailForLane (juce::Rectangle<int> lane, int ioRows)
    {
        const auto faderArea = lane.withTrimmedTop (paintedFaderTopForLane (lane, ioRows))
                                   .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderBottomInset);
        return faderArea.withWidth (yesdaw::ui::UiTheme::Layout::mixerPaintedRailWidth)
                        .withCentre ({ lane.getCentreX()
                                           - yesdaw::ui::UiTheme::Layout::mixerPaintedRailCenterOffsetX,
                                       faderArea.getCentreY() });
    }

    // The painted pan knob's disc for a strip lane — the SAME rect the strip paint fills, so the
    // drag hit-test and the picture cannot drift.
    [[nodiscard]] static juce::Rectangle<int> paintedPanKnobForLane (juce::Rectangle<int> lane)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const auto knob = lane.withTrimmedTop (L::mixerPaintedPanTop).withHeight (L::mixerPaintedPanHeight);
        return juce::Rectangle<int> (knob.getCentreX() - L::mixerPaintedPanRadius,
                                     knob.getY() + L::mixerPaintedPanTopInset,
                                     L::mixerPaintedPanRadius * 2, L::mixerPaintedPanRadius * 2);
    }

    // The painted fader THUMB for a strip lane at a gain — the grab target (the rail itself stays a
    // strip-select click: it overlaps the strip's centre line, which every strip click lands on).
    [[nodiscard]] static juce::Rectangle<int> paintedFaderThumbForLane (juce::Rectangle<int> lane, float linearGain, int ioRows)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const auto rail = paintedFaderRailForLane (lane, ioRows);
        if (rail.isEmpty())
            return {};
        return juce::Rectangle<int> (rail.getX() - L::mixerPaintedThumbWidthOverhang / 2,
                                     mixerFaderThumbYForGain (rail, linearGain) - L::mixerPaintedThumbCenterInset,
                                     rail.getWidth() + L::mixerPaintedThumbWidthOverhang,
                                     L::mixerPaintedThumbHeight);
    }

    [[nodiscard]] static int mixerFaderThumbYForGain (juce::Rectangle<int> rail, float linearGain) noexcept
    {
        return rail.getBottom()
             - juce::roundToInt (mixerFaderFractionForGain (linearGain) * static_cast<float> (rail.getHeight()));
    }

    // M5: the send rows follow the inserts, and take space only after the inserts have taken
    // theirs — a short strip drops sends first, then inserts, and never starves the fader.
    [[nodiscard]] static int paintedSendRowCountForLane (juce::Rectangle<int> lane, int ioRows) noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const int used = paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch
                       + paintedIoRowsShownForLane (lane, ioRows) * L::mixerPaintedIoRowPitch;   // G4.1
        const int available = lane.getHeight() - L::mixerPaintedInsertsTop - used
                            - L::mixerPaintedFaderBottomInset - L::mixerPaintedFaderMinHeight
                            - L::mixerPaintedInsertsFaderGap;
        return std::clamp (available / L::mixerPaintedSendRowPitch, 0, L::mixerPaintedSendRowCount);
    }

    [[nodiscard]] static int paintedSendsTopForLane (juce::Rectangle<int> lane, int ioRows) noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        return L::mixerPaintedInsertsTop
             + paintedInputRowsForLane (lane, ioRows) * L::mixerPaintedIoRowPitch   // G4.1
             + paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch;
    }

    [[nodiscard]] static juce::Rectangle<int> paintedSendRowBoundsForLane (juce::Rectangle<int> lane,
                                                                           std::size_t sendIndex,
                                                                           int ioRows)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        if (static_cast<int> (sendIndex) >= paintedSendRowCountForLane (lane, ioRows))
            return {};

        const int top = lane.getY() + paintedSendsTopForLane (lane, ioRows)
                      + static_cast<int> (sendIndex) * L::mixerPaintedSendRowPitch;
        return juce::Rectangle<int> (lane.getX() + L::mixerPaintedInsertsInsetX,
                                     top,
                                     juce::jmax (0, lane.getWidth() - 2 * L::mixerPaintedInsertsInsetX),
                                     L::mixerPaintedSendRowHeight);
    }

    [[nodiscard]] static int paintedFaderTopForLane (juce::Rectangle<int> lane, int ioRows) noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const int rows = paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch
                       + paintedIoRowsShownForLane (lane, ioRows) * L::mixerPaintedIoRowPitch   // G4.1
                       + paintedSendRowCountForLane (lane, ioRows) * L::mixerPaintedSendRowPitch;
        return rows == 0
            ? L::mixerPaintedFaderTop
            : L::mixerPaintedFaderTop + rows + L::mixerPaintedInsertsFaderGap;
    }

    // N1: ONE Mute/Solo cell law — the paint, the click hit-test, the SELECTED strip's live
    // buttons and the gates all read it, so the control you see is exactly the control you hit,
    // on every strip. Cell 0 is Solo, cell 1 is Mute (left to right, as painted).
    // G4.1: the row is S / M / R on a Track strip (cellCount 3), S / M on a Bus (2); a narrow strip
    // shrinks the cells so they still sit side by side inside the lane.
    [[nodiscard]] juce::Rectangle<int> paintedMuteSoloCellBoundsForLane (juce::Rectangle<int> lane,
                                                                         std::size_t cellIndex,
                                                                         std::size_t cellCount) const
    {
        using L = yesdaw::ui::UiTheme::Layout;
        if (cellIndex >= cellCount)
            return {};

        const bool narrow = appModel.context().mixerStripsNarrow;
        const int cellWidth = narrow ? L::mixerPaintedButtonNarrowWidth : L::mixerPaintedButtonWidth;
        auto buttonsRow = lane.withTrimmedTop (L::mixerPaintedButtonsTop)
                              .withHeight (L::mixerPaintedButtonsHeight)
                              .reduced (narrow ? L::mixerPaintedButtonsNarrowInsetX : L::mixerPaintedButtonsInsetX,
                                        L::mixerPaintedButtonsInsetY);
        buttonsRow.removeFromLeft (static_cast<int> (cellIndex) * cellWidth);
        return buttonsRow.removeFromLeft (cellWidth)
                         .reduced (L::mixerPaintedButtonInsetX, L::mixerPaintedButtonInsetY);
    }

    // M4: ONE insert-slot row law — the paint, the click hit-test and the gates all read it, so a
    // painted slot can never drift from the slot a click selects. An empty rect means the strip has
    // no room for that row.
    [[nodiscard]] static juce::Rectangle<int> paintedInsertRowBoundsForLane (juce::Rectangle<int> lane,
                                                                             std::size_t slotIndex,
                                                                             int ioRows)
    {
        using L = yesdaw::ui::UiTheme::Layout;
        if (static_cast<int> (slotIndex) >= paintedInsertRowCountForLane (lane))
            return {};

        const int top = lane.getY() + L::mixerPaintedInsertsTop
                      + paintedInputRowsForLane (lane, ioRows) * L::mixerPaintedIoRowPitch   // G4.1: below the input row
                      + static_cast<int> (slotIndex) * L::mixerPaintedInsertRowPitch;
        return juce::Rectangle<int> (lane.getX() + L::mixerPaintedInsertsInsetX,
                                     top,
                                     juce::jmax (0, lane.getWidth() - 2 * L::mixerPaintedInsertsInsetX),
                                     L::mixerPaintedInsertRowHeight);
    }

    [[nodiscard]] static juce::Rectangle<int> paintedMeterBoundsForLane (juce::Rectangle<int> lane, int ioRows)
    {
        auto faderArea = lane.withTrimmedTop (paintedFaderTopForLane (lane, ioRows))
                             .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderBottomInset);
        return faderArea.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterWidth)
                        .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetX,
                                  yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetY);
    }

    // G3.4: the quantize panel shows on the CLIP tab when a MIDI clip is selected and no audio clip
    // is (an audio clip's own card wins; the TRACK tab is the track's).
    [[nodiscard]] bool inspectorShowsQuantizePanel() const
    {
        return ! appModel.context().inspectorTrackTabActive
            && appModel.context().projectLoaded
            && appModel.selectedMidiClipId().isValid()
            && findProjectClipById (appModel.selectedTimelineClipId()) == nullptr;
    }

    // The MIDI clip's inspector rows below the title — G3.5's four (mute, transpose, velocity, loop)
    // then G3.4's six (grid, strength, swing, note ends, humanize, apply) — ONE law for the paint
    // (labels on the left) and the controls (on the right).
    static constexpr std::size_t kMidiClipInspectorRows = static_cast<std::size_t> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipRowCount) + 6u;
    [[nodiscard]] static std::array<juce::Rectangle<int>, kMidiClipInspectorRows> midiClipInspectorRows (juce::Rectangle<int> content) noexcept
    {
        std::array<juce::Rectangle<int>, kMidiClipInspectorRows> rows {};
        auto body = content.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop);
        for (juce::Rectangle<int>& row : rows)
            row = body.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorQuantizeRowHeight)
                      .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetX,
                                yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetY);
        return rows;
    }

    [[nodiscard]] juce::Rectangle<int> inspectorBounds() const
    {
        auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
        work.removeFromBottom (dockedMixerHeight());
        if (! appModel.context().inspectorVisible)
            return {};
        return work.removeFromRight (viewState.inspectorWidth)
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
            inspectorMidiMute.setBounds ({});   // G3.5: CLIP-tab content
            inspectorMidiTranspose.setBounds ({});
            inspectorMidiVelocity.setBounds ({});
            inspectorMidiLoop.setBounds ({});
            inspectorQuantizeGrid.setBounds ({});   // G3.4: the panel is CLIP-tab content
            inspectorQuantizeStrength.setBounds ({});
            inspectorQuantizeSwing.setBounds ({});
            inspectorQuantizeEnds.setBounds ({});
            inspectorQuantizeHumanize.setBounds ({});
            inspectorQuantizeApply.setBounds ({});
            inspectorStart.setBounds ({});
            inspectorEnd.setBounds ({});
            inspectorLength.setBounds ({});
            inspectorGain.setBounds ({});
            inspectorStretch.setBounds ({});
            inspectorFadeIn.setBounds ({});
            inspectorFadeOut.setBounds ({});
            inspectorFadeCurve.setBounds ({});
            inspectorFadeCurveAmount.setBounds ({});
            inspectorTakeChooser.setBounds ({});
            inspectorTakeDelete.setBounds ({});
            // G3.1: the instrument row is the TRACK tab's first row — the chooser and Edit sit on
            // its right, the painted "Instrument" label on its left (drawTrackInspector).
            auto instrumentRow = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorStatsSectionTop)
                                     .removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeRowHeight)
                                     .reduced (yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetX,
                                               yesdaw::ui::UiTheme::Layout::inspectorFadeRowInsetY);
            const bool rowFits = area.contains (instrumentRow) && appModel.selectedTrackForInstrument() != nullptr;
            inspectorInstrumentEdit.setBounds (rowFits ? instrumentRow.removeFromRight (yesdaw::ui::UiTheme::Layout::inspectorInstrumentEditWidth) : juce::Rectangle<int> {});
            inspectorInstrumentChooser.setBounds (rowFits ? instrumentRow.removeFromRight (yesdaw::ui::UiTheme::Layout::inspectorInstrumentChooserWidth) : juce::Rectangle<int> {});
            return;
        }
        inspectorInstrumentEdit.setBounds ({});
        inspectorInstrumentChooser.setBounds ({});
        // G3.4: the CLIP tab of a MIDI clip (no audio clip selected) is the quantize panel; its
        // rows share one law with drawMidiClipInspector (quantizeInspectorRows).
        {
            const bool showQuantize = inspectorShowsQuantizePanel();
            const std::array<juce::Rectangle<int>, kMidiClipInspectorRows> rows = midiClipInspectorRows (area);
            const auto control = [&] (std::size_t row, int width) {
                juce::Rectangle<int> rect = rows[row];
                return showQuantize && area.contains (rect) ? rect.removeFromRight (width) : juce::Rectangle<int> {};
            };
            // G3.5: the clip's own rows first, then G3.4's quantize rows.
            inspectorMidiMute.setBounds (control (0, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorMidiTranspose.setBounds (control (1, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorMidiVelocity.setBounds (control (2, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorMidiLoop.setBounds (control (3, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorQuantizeGrid.setBounds (control (4, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorQuantizeStrength.setBounds (control (5, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorQuantizeSwing.setBounds (control (6, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorQuantizeEnds.setBounds (control (7, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorQuantizeHumanize.setBounds (control (8, yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth));
            inspectorQuantizeApply.setBounds (control (9, yesdaw::ui::UiTheme::Layout::inspectorQuantizeApplyWidth));
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
        const auto statsCells = yesdaw::ui::UiTheme::Layout::inspectorStatsCells (statsSection);   // G3.1 rubric FIX 1
        const auto startCell = statsCells[0].reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                                      yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        const auto endCell = statsCells[1].reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                                    yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        const auto lengthCell = statsCells[2].reduced (yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetX,
                                                       yesdaw::ui::UiTheme::Layout::inspectorTimingControlInsetY);
        const bool statsFit = sectionFits (statsSection);
        inspectorStart.setBounds (statsFit ? startCell : juce::Rectangle<int>());
        inspectorEnd.setBounds (statsFit ? endCell : juce::Rectangle<int>());
        inspectorLength.setBounds (statsFit ? lengthCell : juce::Rectangle<int>());

        // G3.7 rubric FIX (a G3.5 defect the MIDI-import shot exposed): the audio clip's gain, stretch
        // and fade controls are audio-only — with a MIDI clip in the CLIP tab they stayed laid out under
        // the MIDI rows and overlapped the loop chooser and Apply (Q). One law: they exist only when the
        // tab shows an audio clip.
        const bool audioClipControls = ! inspectorShowsQuantizePanel();
        const auto audioSectionFits = [&] (juce::Rectangle<int> section) { return audioClipControls && sectionFits (section); };
        const auto gainSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorGainSectionTop)
                                     .withHeight (yesdaw::ui::UiTheme::Layout::inspectorGainSectionHeight);
        auto gain = gainSection;
        gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlTopInset);
        inspectorGain.setBounds (audioSectionFits (gainSection)
            ? gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlHeight)
                  .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainControlLeftInset)
            : juce::Rectangle<int>());
        gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorStretchControlTopGap);   // G2.9b
        inspectorStretch.setBounds (audioSectionFits (gainSection)
            ? gain.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorGainControlHeight)
                  .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorGainControlLeftInset)
            : juce::Rectangle<int>());

        const auto fadesSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionTop)
                                      .withHeight (yesdaw::ui::UiTheme::Layout::inspectorFadesSectionHeight);
        auto fades = fadesSection;
        fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadesControlTopInset);
        const bool fadesFit = audioSectionFits (fadesSection);
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
        // G2.10: the Curve row carries the shape chooser (left half) and the amount slider (right half).
        juce::Rectangle<int> curveRow = fades.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveControlHeight)
                                             .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeControlLeftInset);
        const juce::Rectangle<int> chooserCell = curveRow.removeFromLeft (curveRow.getWidth() / 2);
        curveRow.removeFromLeft (yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountGap);
        inspectorFadeCurve.setBounds (fadesFit ? chooserCell : juce::Rectangle<int>());
        inspectorFadeCurveAmount.setBounds (fadesFit ? curveRow : juce::Rectangle<int>());

        // E33: the TAKES section (the old automation placeholder's area) — the chooser and
        // the delete button share its whole-section drop law.
        // G2.14: the MARKERS card below the takes — whole-section drop like every card.
        {
            auto markersSection = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorMarkersSectionTop)
                                      .withHeight (yesdaw::ui::UiTheme::Layout::inspectorMarkersSectionHeight);
            if (sectionFits (markersSection))
            {
                markersSection.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight);
                inspectorMarkerList.setBounds (markersSection.reduced (yesdaw::ui::UiTheme::Layout::inspectorAutomationChartInsetX,
                                                                       yesdaw::ui::UiTheme::Space::none));
            }
            else
                inspectorMarkerList.setBounds ({});
            inspectorMarkerList.updateContent();
        }
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
        // G2.1 cp2: the takes law reads these bounds — re-evaluate after layout, so a layout that
        // makes room (the settings row collapsing, the dock shrinking) shows the chooser at once.
        refreshInspectorTakesVisibility();
    }

    void layoutMixerControls()
    {
        // G4.1 cp2: the lane is gone. What is left to lay out live: the FX editor's parameter rows
        // (inside the editor's content area — the pager, then label + slider / chooser per row) and
        // the master pane's fader.
        {
            using L = yesdaw::ui::UiTheme::Layout;
            auto content = fxEditor.contentArea();
            if (mixerFxParamPageChooser.isVisible())
            {
                mixerFxParamPageChooser.setBounds (content.removeFromTop (L::mixerFxParamRowHeight));
                content.removeFromTop (L::mixerFxParamRowGap);
            }
            else
                mixerFxParamPageChooser.setBounds ({});
            for (std::size_t index = 0; index < mixerFxParamSliders.size(); ++index)
            {
                if (! mixerFxParamLabels[index].isVisible() || content.getHeight() < L::mixerFxParamRowHeight)
                {
                    mixerFxParamSliders[index].setBounds ({});
                    mixerFxParamChoosers[index].setBounds ({});
                    mixerFxParamLabels[index].setBounds ({});
                    continue;
                }
                auto paramRow = content.removeFromTop (L::mixerFxParamRowHeight);
                mixerFxParamLabels[index].setBounds (paramRow.removeFromLeft (L::mixerFxParamLabelWidth));
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
                content.removeFromTop (L::mixerFxParamRowGap);
            }
        }

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
        // V8: the zoom cluster shares the automation toggle's toolbar row.
        timelineZoomOutButton.setBounds (L::timelineZoomOutButtonBounds (timeline));
        timelineZoomReadout.setBounds (L::timelineZoomReadoutBounds (timeline));
        timelineZoomInButton.setBounds (L::timelineZoomInButtonBounds (timeline));
        timelineZoomSlider.setBounds (L::timelineZoomSliderBounds (timeline));   // G2.16
        // G1.4 toolbar v2: [nudge] … status … [Inspector]; the nudge chooser drops whole when
        // the row cannot hold it next to the toggle.
        {
            juce::Rectangle<int> status = L::statusLineBounds (timeline);
            const juce::Rectangle<int> toggle = status.withX (timeline.getRight() - L::statusLineRightInset - L::inspectorToggleWidth)
                                                      .withWidth (L::inspectorToggleWidth);
            const int clusterLeft = toggle.getX() - (L::inspectorToggleWidth + L::inspectorToggleGap) * 3;   // G2.1 cp3: [I][X][P][A]
            // G2.6 / G2.7: [Snap mode][Edit mode][Nudge] lead the status row; each drops whole when
            // the row cannot hold it.
            const juce::Rectangle<int> snapMode = status.withWidth (L::timelineSnapModeChooserWidth);
            const bool snapModeFits = snapMode.getRight() + L::timelineNudgeChooserGap + L::inspectorToggleGap <= clusterLeft;
            snapModeChooser.setBounds (snapModeFits ? snapMode : juce::Rectangle<int>());
            const juce::Rectangle<int> editMode = status.withX (snapModeFits ? snapMode.getRight() + L::timelineNudgeChooserGap : status.getX())
                                                        .withWidth (L::timelineEditModeChooserWidth);
            const bool editModeFits = editMode.getRight() + L::timelineNudgeChooserGap + L::inspectorToggleGap <= clusterLeft;
            editModeChooser.setBounds (editModeFits ? editMode : juce::Rectangle<int>());
            juce::Rectangle<int> nudge = status.withX (editModeFits ? editMode.getRight() + L::timelineNudgeChooserGap
                                                       : snapModeFits ? snapMode.getRight() + L::timelineNudgeChooserGap : status.getX())
                                               .withWidth (L::timelineNudgeChooserWidth);
            const bool nudgeFits = nudge.getRight() + L::timelineNudgeChooserGap + L::inspectorToggleGap <= clusterLeft;
            nudgeValueChooser.setBounds (nudgeFits ? nudge : juce::Rectangle<int>());
            // G2.1 cp3: the view cluster [I][X][P][A] (plan §3.1) — inspector, dock (mixer) toggle,
            // piano-roll toggle, automation lane toggle — one row of letters at the status line's
            // right, above the timeline canvas in z. The zoom trio took the old Automation slot.
            const int step = L::inspectorToggleWidth + L::inspectorToggleGap;
            const juce::Rectangle<int> automationToggle = toggle;
            const juce::Rectangle<int> pianoToggle = toggle.translated (-step, 0);
            const juce::Rectangle<int> mixerToggle = toggle.translated (-step * 2, 0);
            const juce::Rectangle<int> inspectorToggleBounds = toggle.translated (-step * 3, 0);
            inspectorToggle.setBounds (inspectorToggleBounds);
            mixerDockToggle.setBounds (mixerToggle);
            setToolbarButtonBounds (yesdaw::ui::UiActionId::ViewPianoRoll, pianoToggle);
            automationLaneToggle.setBounds (automationToggle);
            inspectorToggle.toFront (false);
            mixerDockToggle.toFront (false);
            if (juce::Component* piano = toolbarButtonFor (yesdaw::ui::UiActionId::ViewPianoRoll))
                piano->toFront (false);
            automationLaneToggle.toFront (false);
            const int statusLeft = nudgeFits ? nudge.getRight() + L::timelineNudgeChooserGap
                                 : editModeFits ? editMode.getRight() + L::timelineNudgeChooserGap
                                 : snapModeFits ? snapMode.getRight() + L::timelineNudgeChooserGap : status.getX();
            const int statusRight = clusterLeft - L::inspectorToggleGap;
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
        if (key.getKeyCode() == juce::KeyPress::escapeKey && fxEditorOpen)   // G4.1 cp2
        {
            closeFxEditor();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::escapeKey && appModel.context().undoHistoryVisible)   // G2.18
        {
            handleAction (yesdaw::ui::UiActionId::EditShowUndoHistory);
            refreshActionState();
            resized();
            repaintAll();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::escapeKey && appModel.context().keymapVisible)
        {
            handleAction (yesdaw::ui::UiActionId::HelpShowKeymap);
            refreshActionState();
            resized();
            repaintAll();
            return true;
        }
        if (key.getKeyCode() == juce::KeyPress::escapeKey && cancelInProgressEdit())
            return true;

        // G3.6: musical typing takes its note and control keys BEFORE the keymap (a plain letter,
        // no Ctrl / Alt); everything else — Space, the tools, Ctrl+K itself — still reaches the keymap.
        if (appModel.context().musicalTypingOn
            && ! key.getModifiers().isCtrlDown() && ! key.getModifiers().isCommandDown() && ! key.getModifiers().isAltDown())
        {
            const int code = key.getKeyCode();
            if (code > 32 && code < 127)
            {
                const auto press = appModel.musicalTypingPress (static_cast<char> (code));
                if (press.handled)
                {
                    if (press.key >= 0)
                        typedKeyCodes[code] = press.key;
                    refreshActionState();
                    repaintAll();
                    return true;
                }
            }
        }
        // G3.6: with step input on, Right is a rest and Left steps back (the roll's note walk waits).
        if (appModel.context().stepInputOn && key.getModifiers().isAnyModifierKeyDown() == false)
        {
            if (key.getKeyCode() == juce::KeyPress::rightKey) { (void) appModel.stepInputRest(); refreshActionState(); repaintAll(); return true; }
            if (key.getKeyCode() == juce::KeyPress::leftKey)  { (void) appModel.stepInputBack(); refreshActionState(); repaintAll(); return true; }
        }

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
        hideMixerControlsBehindDockTab();   // G2.1 cp2: after every action's refresh
        timelineInput.repaint();
        repaint();
    }

    void repaintDynamicLayers()
    {
        ++dynamicInvalidations;
        playheadLayer.repaint();
        repaint (getLocalBounds().withHeight (headerHeightNow()));
        repaint (leftRailPanelBounds());
        if (appModel.context().mixerDockVisible)
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
    bool keyStateChanged (bool isKeyDown) override
    {
        // G3.6: a typed note holds while its key is down and releases when it lifts (JUCE reports
        // the state change, not the key: every held code is checked).
        for (auto it = typedKeyCodes.begin(); it != typedKeyCodes.end();)
        {
            if (! juce::KeyPress::isKeyCurrentlyDown (it->first))
            {
                appModel.musicalTypingRelease (it->second);
                it = typedKeyCodes.erase (it);
            }
            else
                ++it;
        }
        return juce::Component::keyStateChanged (isKeyDown);
    }

    // G3.6: the harness's key-up (the headless run has no real keyboard for isKeyCurrentlyDown).
    void harnessReleaseTypedKeys()
    {
        for (const auto& [code, note] : typedKeyCodes)
            appModel.musicalTypingRelease (note);
        typedKeyCodes.clear();
    }

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

    // G3.3: a mouse gesture that lands on a model verb without passing handleAction still names
    // itself to the probe (a drive asserts on lastAction; nothing is blind).
    void recordLastAction (yesdaw::ui::UiActionId action)
    {
        if (const auto* descriptor = appModel.registry().descriptor (action))
            lastActionStableId = descriptor->stableId;
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

        // G1.5: Alt+K shows / hides the keymap editor over the arrangement.
        if (action == yesdaw::ui::UiActionId::HelpShowKeymap)
        {
            keymapEditor.setVisible (appModel.context().keymapVisible);
            if (appModel.context().keymapVisible)
            {
                keymapEditor.refreshRows();
                keymapEditor.toFront (false);
            }
        }
        if (action == yesdaw::ui::UiActionId::EditShowUndoHistory)   // G2.18
        {
            undoHistory.setVisible (appModel.context().undoHistoryVisible);
            if (appModel.context().undoHistoryVisible)
            {
                undoHistory.refreshRows();
                undoHistory.toFront (false);
            }
        }
        else if (undoHistory.isVisible())
            undoHistory.refreshRows();   // an edit while the window shows: the rows follow

        // G0.7 / G1.4: the settings row and the inspector change the layout — the whole shell
        // re-lays out.
        if (action == yesdaw::ui::UiActionId::ViewToggleSettingsRow
            || action == yesdaw::ui::UiActionId::ViewToggleInspector
            || action == yesdaw::ui::UiActionId::HelpShowKeymap
            || action == yesdaw::ui::UiActionId::EditShowUndoHistory)
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
        refreshingZoomSlider = true;   // G2.16: the slider follows the factor (log scale)
        timelineZoomSlider.setRange (0.0, std::log2 (timelineZoomCeiling()), 0.01);   // G2.19: to the ceiling
        timelineZoomSlider.setValue (std::log2 (std::clamp (timelineZoomFactor, yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                                                             timelineZoomCeiling())),
                                     juce::dontSendNotification);
        refreshingZoomSlider = false;
        refreshTimelineScrollBars();
    }

    // G2.16: the zoom history — every zoom change pushes the view it leaves; Zoom Back pops.
    struct ZoomView { double zoom; double scroll; };
    std::vector<ZoomView> zoomHistory;
    void pushZoomHistory()
    {
        if (! zoomHistory.empty() && zoomHistory.back().zoom == timelineZoomFactor && zoomHistory.back().scroll == timelineScrollSeconds)
            return;
        zoomHistory.push_back ({ timelineZoomFactor, timelineScrollSeconds });
        if (zoomHistory.size() > static_cast<std::size_t> (yesdaw::ui::UiTheme::Layout::timelineZoomHistoryDepth))
            zoomHistory.erase (zoomHistory.begin());
    }
    [[nodiscard]] bool popZoomHistory()
    {
        if (zoomHistory.empty())
            return false;
        const ZoomView view = zoomHistory.back();
        zoomHistory.pop_back();
        timelineZoomFactor = std::clamp (view.zoom, yesdaw::ui::UiTheme::Layout::timelineZoomMin, timelineZoomCeiling());
        timelineScrollSeconds = std::max (0.0, view.scroll);
        refreshTimelineZoomReadout();
        return true;
    }

    // G2.16: the vertical zoom — every auto-height row scales; the rail and the canvas share it.
    void zoomTracksBy (double factor)
    {
        timelineRowZoom = std::clamp (timelineRowZoom * factor, yesdaw::ui::UiTheme::Layout::timelineRowZoomMin,
                                      yesdaw::ui::UiTheme::Layout::timelineRowZoomMax);
        resized();
        refreshTimelineScrollBars();
    }

    // G2.16: the scroll bars mirror the view (seconds horizontally, rows vertically) and drive it.
    void refreshTimelineScrollBars()
    {
        refreshingScrollBars = true;
        const double visible = std::max (0.0, timelineVisibleSecondsFor (timelineTotalSeconds));
        timelineHScroll.setRangeLimits (0.0, std::max (timelineTotalSeconds, timelineScrollSeconds + visible), juce::dontSendNotification);
        timelineHScroll.setCurrentRange (timelineScrollSeconds, visible, juce::dontSendNotification);
        const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), makeTimelineState());
        const int maxRows = std::max (geometry.maxTrackScrollRows, trackListInput.maxScrollRows());
        const double visibleRows = std::max (1.0, static_cast<double> (std::max (1, appModel.context().projectLoaded ? static_cast<int> (appModel.project().tracks.size()) : 1) - maxRows));
        timelineVScroll.setRangeLimits (0.0, static_cast<double> (maxRows) + visibleRows, juce::dontSendNotification);
        timelineVScroll.setCurrentRange (static_cast<double> (timelineTrackScrollRows), visibleRows, juce::dontSendNotification);
        refreshingScrollBars = false;
    }

    void scrollBarMoved (juce::ScrollBar* bar, double newRangeStart) override
    {
        if (refreshingScrollBars)
            return;
        if (bar == &timelineHScroll)
            timelineScrollSeconds = std::max (0.0, newRangeStart);
        else if (bar == &timelineVScroll)
            timelineTrackScrollRows = std::max (0, juce::roundToInt (newRangeStart));
        repaintAll();
    }

    // G2.19: the zoom ceiling — one sample per pixel at the project's rate (never below the old
    // 64x floor). It follows the fit width, so it is a function, not a token.
    [[nodiscard]] double timelineZoomCeiling() const noexcept
    {
        using L = yesdaw::ui::UiTheme::Layout;
        const double width = static_cast<double> (juce::jmax (L::timelineViewportMinPixelWidth,
                                                              timelineInput.getWidth() - L::timelineViewportRightGutter));
        const double fitPixelsPerSecond = width / std::max (L::timelineMinVisibleSeconds, timelineTotalSeconds);
        const double rate = appModel.context().projectLoaded && appModel.project().sampleRate.isValid()
                              ? appModel.project().sampleRate.hz : 0.0;
        if (fitPixelsPerSecond <= 0.0 || rate <= 0.0)
            return L::timelineZoomMax;
        return std::max (L::timelineZoomMax, (rate / L::timelineZoomSamplesPerPixelCeiling) / fitPixelsPerSecond);
    }

    void zoomTimelineAtAnchor (double anchorSeconds, double factor)
    {
        pushZoomHistory();   // G2.16
        const double previousZoom = timelineZoomFactor;
        timelineZoomFactor = std::clamp (timelineZoomFactor * factor,
                                         yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                                         timelineZoomCeiling());
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

    // G3.2: the roll pages after the playhead while it shows and the transport rolls — the same
    // page law as the arrangement, in the clip's ticks. Returns true when the view moved.
    bool followPianoRollPlayhead()
    {
        if (! dockShowsPianoRoll() || ! appModel.context().playheadFollowEnabled || ! appModel.context().isPlaying)
            return false;
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = currentPianoRollSurface();
        if (! surface.midiClipSelected || surface.playheadTick < 0 || surface.playheadTick > surface.timelineLength)
            return false;
        const yesdaw::engine::Tick visible = pianoRollVisibleTicks (surface);
        if (surface.playheadTick >= surface.viewScrollTicks && surface.playheadTick < surface.viewScrollTicks + visible)
            return false;
        const yesdaw::engine::Tick lead = visible * yesdaw::ui::UiTheme::Layout::pianoRollFollowLeadPercent / 100;
        const yesdaw::engine::Tick maxScroll = std::max<yesdaw::engine::Tick> (0, surface.timelineLength - visible);
        pianoRollViewScrollTicks = std::clamp<yesdaw::engine::Tick> (surface.playheadTick - lead, 0, maxScroll);
        return true;
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
        if (appModel.context().playheadFollowContinuous)   // G2.16: the playhead stays at the middle
        {
            // Pro Tools' continuous scroll: the playhead runs from the left edge to the middle
            // and then the view moves under it; a playhead behind the window (a locate) recentres.
            if (playheadSeconds > timelineScrollSeconds + visibleSeconds * 0.5
                || playheadSeconds < timelineScrollSeconds)
                timelineScrollSeconds = std::max (0.0, playheadSeconds - visibleSeconds * 0.5);
            refreshTimelineScrollBars();
            return;
        }
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
        static constexpr std::array<UiActionId, 10> kFileMenu {
            UiActionId::ProjectNew,        UiActionId::ProjectOpen,        UiActionId::ProjectSave,
            UiActionId::ProjectSaveAs,     UiActionId::ProjectImportAudio, UiActionId::ProjectExportAudio,
            UiActionId::ProjectImportMidi, UiActionId::ProjectExportMidi,   // G3.7
            UiActionId::ProjectExportDawproject, UiActionId::ProjectExportAudioCancel,
        };
        static constexpr std::array<UiActionId, 33> kEditMenu {
            UiActionId::EditUndo,          UiActionId::EditRedo,           UiActionId::EditShowUndoHistory,   // G2.18
            UiActionId::TimelineClipCut,
            UiActionId::TimelineClipCopy,  UiActionId::TimelineClipPaste,  UiActionId::TimelineClipDuplicate,
            UiActionId::TimelineClipRepeatPaste, UiActionId::TimelineClipDelete,
            UiActionId::TimelineRangeCut,  UiActionId::TimelineRangeCopy,  UiActionId::TimelineRangeDelete,
            UiActionId::TimelineRangeSilence, UiActionId::TimelineRangeSplitEdges, UiActionId::TimelineSelectAllFollowing,
            UiActionId::EditModeOverlap,   UiActionId::EditModeNoOverlap,  UiActionId::EditModeShuffle,
            UiActionId::TimelineClipSelectAllProject, UiActionId::TimelineClipSelectAllTrack,
            UiActionId::EditRenameSelection,
            UiActionId::EditNudgeLeft,     UiActionId::EditNudgeRight,
            UiActionId::EditNudgeLeftFine, UiActionId::EditNudgeRightFine,
            UiActionId::EditNudgeValueGrid, UiActionId::EditNudgeValueBar,
            UiActionId::EditNudgeValueBeat, UiActionId::EditNudgeValueSixteenth,
            UiActionId::EditNudgeValueMs1, UiActionId::EditNudgeValueMs10,          // G2.8
            UiActionId::EditNudgeValueFrame, UiActionId::EditNudgeValueSample,
        };
        static constexpr std::array<UiActionId, 12> kTrackMenu {
            UiActionId::TrackAdd,          UiActionId::TrackDuplicate,     UiActionId::TrackRemove,
            UiActionId::TrackRename,       UiActionId::TrackMoveUp,        UiActionId::TrackMoveDown,
            UiActionId::TrackSelectPrevious, UiActionId::TrackSelectNext,
            UiActionId::TrackToggleMute,   UiActionId::TrackToggleSolo,    UiActionId::TrackToggleArm,
            UiActionId::MixerTrackSetOutput,
        };
        static constexpr std::array<UiActionId, 18> kClipMenu {
            UiActionId::TimelineClipSplit, UiActionId::TimelineClipHeal,
            UiActionId::TimelineClipApplyDefaultFades, UiActionId::TimelineClipSetFades,
            UiActionId::TimelineClipCrossfade, UiActionId::TimelineClipSetGain,
            UiActionId::TimelineClipGainIncrease, UiActionId::TimelineClipGainDecrease,
            UiActionId::TimelineClipMove,  UiActionId::TimelineClipTrim,
            UiActionId::TimelineClipTimeStretch, UiActionId::TimelineClipStretchToLoop,   // G2.9b
            UiActionId::TimelineClipToggleMute, UiActionId::TimelineClipColourNext,      // G2.12
            UiActionId::TimelineClipReverse, UiActionId::TimelineClipNormalize, UiActionId::TimelineClipStripSilence,   // G2.13
            UiActionId::TimelineMidiClipAdd,
        };
        static constexpr std::array<UiActionId, 14> kMidiMenu {
            UiActionId::PianoRollNoteAdd,  UiActionId::PianoRollNoteDelete, UiActionId::PianoRollNoteSelectAll,
            UiActionId::PianoRollNoteQuantizeSelection, UiActionId::PianoRollNoteTranspose,
            UiActionId::PianoRollMusicalTypingToggle, UiActionId::PianoRollStepInputToggle,   // G3.6
            UiActionId::PianoRollNoteOctaveUp, UiActionId::PianoRollNoteOctaveDown,
            UiActionId::PianoRollNoteDuplicate, UiActionId::PianoRollNoteSetLength,
            UiActionId::PianoRollNoteSetVelocity,
            UiActionId::PianoRollNoteSelectPrevious, UiActionId::PianoRollNoteSelectNext,   // G3.2
        };
        static constexpr std::array<UiActionId, 32> kViewMenu {
            UiActionId::ViewTimeline,      UiActionId::ViewMixer,          UiActionId::ViewPianoRoll,
            UiActionId::ViewInstrument,   // G3.1
            UiActionId::ViewToggleInspector,
            UiActionId::TimelineToggleMixerDock, UiActionId::MixerStripsNarrowToggle,   // G4.1
            UiActionId::InspectorShowClipTab, UiActionId::InspectorShowTrackTab,
            UiActionId::TimelineAutomationToggleTrackLane,
            UiActionId::TimelineZoomIn,    UiActionId::TimelineZoomOut,
            UiActionId::TimelineZoomTracksIn, UiActionId::TimelineZoomTracksOut, UiActionId::TimelineZoomBack,   // G2.16
            UiActionId::TimelinePlayheadFollowContinuous,
            UiActionId::TimelineZoomFitProject, UiActionId::TimelineZoomFitLoop, UiActionId::TimelineZoomToSelection,
            UiActionId::TimelineTogglePlayheadFollow,
            UiActionId::TimelineToolSelectPointer, UiActionId::TimelineToolSelectPencil,
            UiActionId::TimelineToolSelectScissors, UiActionId::TimelineToolSelectHand,
            UiActionId::TimelineToolSelectZoom, UiActionId::TimelineToolSelectEraser, UiActionId::TimelineToolSelectVelocity,   // G3.2
            UiActionId::ViewToggleSettingsRow,
            UiActionId::TimelineSnapModeGrid, UiActionId::TimelineSnapModeRelative,
            UiActionId::TimelineSnapModeEvents, UiActionId::TimelineSnapModeOff,
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
            case UiActionId::MixerStripsNarrowToggle:           return c.mixerStripsNarrow;   // G4.1
            case UiActionId::ViewToggleInspector:               return c.inspectorVisible;
            case UiActionId::EditNudgeValueGrid:                return c.nudgeValue == 0;
            case UiActionId::EditNudgeValueBar:                 return c.nudgeValue == 1;
            case UiActionId::EditNudgeValueBeat:                return c.nudgeValue == 2;
            case UiActionId::EditNudgeValueSixteenth:           return c.nudgeValue == 3;
            case UiActionId::EditNudgeValueMs1:                 return c.nudgeValue == 4;   // G2.8
            case UiActionId::EditNudgeValueMs10:                return c.nudgeValue == 5;
            case UiActionId::EditNudgeValueFrame:               return c.nudgeValue == 6;
            case UiActionId::EditNudgeValueSample:              return c.nudgeValue == 7;
            case UiActionId::TimelineToggleMixerDock:           return c.mixerDockVisible;
            case UiActionId::EditModeOverlap:                   return c.editMode == yesdaw::ui::UiEditMode::Overlap;    // G2.6
            case UiActionId::EditModeNoOverlap:                 return c.editMode == yesdaw::ui::UiEditMode::NoOverlap;
            case UiActionId::EditModeShuffle:                   return c.editMode == yesdaw::ui::UiEditMode::Shuffle;
            case UiActionId::TimelineSnapModeGrid:              return c.snapMode == yesdaw::ui::UiSnapMode::Grid;      // G2.7
            case UiActionId::TimelineSnapModeRelative:          return c.snapMode == yesdaw::ui::UiSnapMode::Relative;
            case UiActionId::TimelineSnapModeEvents:            return c.snapMode == yesdaw::ui::UiSnapMode::Events;
            case UiActionId::TimelineSnapModeOff:               return c.snapMode == yesdaw::ui::UiSnapMode::Off;
            case UiActionId::TimelineClipToggleMute:            return c.timelineClipMuted;   // G2.12
            case UiActionId::TimelineClipReverse:               return c.timelineClipReversed;   // G2.13
            case UiActionId::TimelineTempoChangeToggleRamp:     return c.tempoChangeAtPlayheadRamps;   // G2.15
            case UiActionId::TimelinePlayheadFollowContinuous:  return c.playheadFollowContinuous;    // G2.16
            case UiActionId::TimelineAutomationToggleTrackLane: return c.timelineAutomationTrackLaneVisible;
            case UiActionId::ViewTimeline:                      return c.activePanel == yesdaw::ui::UiPanel::Timeline;
            case UiActionId::ViewMixer:                         return c.mixerDockVisible && c.editorDockTab == yesdaw::ui::UiEditorDockTab::Mixer;
            case UiActionId::ViewPianoRoll:                     return c.mixerDockVisible && c.editorDockTab == yesdaw::ui::UiEditorDockTab::PianoRoll;
            case UiActionId::ViewInstrument:                    return c.mixerDockVisible && c.editorDockTab == yesdaw::ui::UiEditorDockTab::Instrument;   // G3.1
            case UiActionId::InspectorShowClipTab:              return ! c.inspectorTrackTabActive;
            case UiActionId::InspectorShowTrackTab:             return c.inspectorTrackTabActive;
            case UiActionId::TimelineSnapDisable:               return ! c.snapEnabled;
            case UiActionId::TimelineSnapSetBar:                return c.snapEnabled && c.snapGridTicks == 2048;
            case UiActionId::TimelineSnapSetBeat:               return c.snapEnabled && c.snapGridTicks == 512;
            case UiActionId::TimelineSnapSetSixteenth:          return c.snapEnabled && c.snapGridTicks == 128;
            case UiActionId::TimelineToolSelectPointer:         return c.activeTimelineTool == yesdaw::ui::TimelineTool::Pointer;
            case UiActionId::TimelineToolSelectPencil:          return c.activeTimelineTool == yesdaw::ui::TimelineTool::Pencil;
            case UiActionId::TimelineToolSelectScissors:        return c.activeTimelineTool == yesdaw::ui::TimelineTool::Scissors;
            case UiActionId::TimelineToolSelectEraser:          return c.activeTimelineTool == yesdaw::ui::TimelineTool::Eraser;     // G3.2
            case UiActionId::TimelineToolSelectVelocity:        return c.activeTimelineTool == yesdaw::ui::TimelineTool::Velocity;
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
        std::vector<int> addInsertKinds;   // G4.1: the kinds the Add Insert submenu offered

        [[nodiscard]] yesdaw::ui::MainComponentContextMenu toPublic (const juce::String& route = "none") const
        {
            yesdaw::ui::MainComponentContextMenu out;
            out.route = route;
            out.shown = shown;
            out.target = target;
            out.index = index;
            out.actions = actions;
            out.addInsertKinds = addInsertKinds;
            return out;
        }
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
        hideMixerControlsBehindDockTab();   // G3.2 checkpoint FIX 1
        juce::PopupMenu menu;
        // G1.7: an EMPTY insert slot offers exactly one thing — Add Insert (the kinds) — instead
        // of four disabled verbs; a strip's Add Insert is the same submenu, so a right-click
        // reaches every effect without the dock's chooser.
        const bool emptySlot = target == yesdaw::ui::ContextMenuTarget::InsertSlot
                            && ! (index >= 0 && static_cast<std::size_t> (index) < appModel.selectedStripFxChain().size());
        // G4.1 cp2: an EMPTY send well offers Add Send alone (the buses inline); a routed row its menu.
        const std::vector<yesdaw::engine::SendRow> sendRows = appModel.selectedTrackSends();
        const bool sendRowTarget = target == yesdaw::ui::ContextMenuTarget::MixerSendRow;
        const bool emptySendWell = sendRowTarget
                                && ! (index >= 0 && static_cast<std::size_t> (index) < sendRows.size());
        std::vector<yesdaw::ui::ContextMenuEntry> entries;
        if (emptySlot)
            entries.push_back ({ yesdaw::ui::UiActionId::MixerFxInsertAdd });
        else if (emptySendWell)
            entries.push_back ({ yesdaw::ui::UiActionId::MixerSendAdd });
        else
            for (const yesdaw::ui::ContextMenuEntry& entry : yesdaw::ui::contextMenuEntries (target))
                entries.push_back (entry);
        for (const yesdaw::ui::ContextMenuEntry& entry : entries)
        {
            if (entry.separatorBefore)
                menu.addSeparator();
            if (entry.action == yesdaw::ui::UiActionId::MixerFxInsertAdd)
            {
                // G4.1: the kinds THIS strip takes (a Bus / the master: the audio kinds only). G4.1 cp2: the
                // slot's own click lists the kinds inline (Logic's plug-in menu); the strip menu keeps its submenu.
                juce::PopupMenu kinds;
                for (const yesdaw::engine::FxKind kind : appModel.fxKindsForSelectedStrip())
                {
                    kinds.addItem (kContextMenuAddInsertBase + static_cast<int> (kind), fxKindName (kind));
                    lastContextMenu.addInsertKinds.push_back (static_cast<int> (kind));
                }
                if (target == yesdaw::ui::ContextMenuTarget::InsertSlot)
                {
                    menu.addSectionHeader ("Add Insert");
                    juce::PopupMenu::MenuItemIterator it (kinds);
                    while (it.next())
                        menu.addItem (juce::PopupMenu::Item (it.getItem()));
                }
                else
                    menu.addSubMenu ("Add Insert", kinds,
                                     appModel.registry().stateFor (entry.action, appModel.context()).enabled);
                lastContextMenu.actions.push_back (entry.action);
                continue;
            }
            // G4.1: the routing verbs carry their choices — a submenu on the strip menu, the items
            // themselves on the I/O slot's menu (one verb, expanded inline).
            if (entry.action == yesdaw::ui::UiActionId::MixerSendAdd
                || entry.action == yesdaw::ui::UiActionId::MixerTrackSetOutput
                || entry.action == yesdaw::ui::UiActionId::MixerTrackSetInput)
            {
                const bool inline_ = target == yesdaw::ui::ContextMenuTarget::MixerStripInput
                                  || target == yesdaw::ui::ContextMenuTarget::MixerStripOutput
                                  || emptySendWell;   // G4.1 cp2: the well's click lists the buses
                const bool enabled = appModel.registry().stateFor (entry.action, appModel.context()).enabled;
                juce::PopupMenu choices;
                int choiceCount = 0;
                const auto& project = appModel.project();
                const yesdaw::engine::EntityId self = appModel.selectedSendOwnerEntityId();
                if (entry.action == yesdaw::ui::UiActionId::MixerSendAdd)
                {
                    for (std::size_t busIndex = 0; busIndex < project.buses.size() && busIndex < kContextMenuChoiceRange; ++busIndex)
                    {
                        if (project.buses[busIndex].id == self)
                            continue;   // R13: never a self-route
                        choices.addItem (kContextMenuAddSendBase + static_cast<int> (busIndex),
                                         juce::String (project.buses[busIndex].strip.name));
                        ++choiceCount;
                    }
                }
                else if (entry.action == yesdaw::ui::UiActionId::MixerTrackSetOutput)
                {
                    const yesdaw::engine::EntityId routed = appModel.selectedTrackOutputBusId();
                    juce::PopupMenu::Item master ("Master");
                    master.itemID = kContextMenuOutputBase;
                    master.isTicked = ! routed.isValid();
                    choices.addItem (std::move (master));
                    ++choiceCount;
                    for (std::size_t busIndex = 0; busIndex < project.buses.size() && busIndex + 1 < kContextMenuChoiceRange; ++busIndex)
                    {
                        if (project.buses[busIndex].id == self)
                            continue;
                        juce::PopupMenu::Item item (juce::String (project.buses[busIndex].strip.name));
                        item.itemID = kContextMenuOutputBase + 1 + static_cast<int> (busIndex);
                        item.isTicked = project.buses[busIndex].id == routed;
                        choices.addItem (std::move (item));
                        ++choiceCount;
                    }
                }
                else
                {
                    const auto& device = appModel.recordingDeviceSelection();
                    const int inputs = device.selected ? static_cast<int> (device.inputChannels) : 0;
                    const yesdaw::ui::UiRecordingTrackInputSelection* picked = nullptr;
                    const int ordinal = appModel.selectedMixerStripOrdinal();
                    for (const yesdaw::ui::UiRecordingTrackInputSelection& armed : appModel.armedRecordingTrackInputs())
                        if (armed.armed && ordinal >= 0 && armed.trackIndex == static_cast<std::size_t> (ordinal))
                            picked = &armed;
                    for (int channel = 0; channel < inputs && channel < static_cast<int> (kContextMenuChoiceRange); ++channel)
                    {
                        juce::PopupMenu::Item item ("In " + juce::String (channel + 1));
                        item.itemID = kContextMenuInputMonoBase + channel;
                        item.isTicked = picked != nullptr && ! picked->stereoPair && picked->inputChannel == channel;
                        choices.addItem (std::move (item));
                        ++choiceCount;
                    }
                    for (int channel = 0; channel + 1 < inputs && channel < static_cast<int> (kContextMenuChoiceRange); ++channel)
                    {
                        juce::PopupMenu::Item item ("In " + juce::String (channel + 1) + "+" + juce::String (channel + 2));
                        item.itemID = kContextMenuInputPairBase + channel;
                        item.isTicked = picked != nullptr && picked->stereoPair && picked->inputChannel == channel;
                        choices.addItem (std::move (item));
                        ++choiceCount;
                    }
                    if (choiceCount == 0)
                    {
                        juce::PopupMenu::Item none ("No inputs (Options > Audio Device)");
                        none.isEnabled = false;
                        choices.addItem (std::move (none));
                    }
                }
                if (inline_)
                {
                    menu.addSectionHeader (entry.action == yesdaw::ui::UiActionId::MixerTrackSetInput ? "Input"
                                           : entry.action == yesdaw::ui::UiActionId::MixerSendAdd ? "Send to" : "Output");
                    if (entry.action == yesdaw::ui::UiActionId::MixerSendAdd && choiceCount == 0)
                    {
                        juce::PopupMenu::Item none ("No buses (Add Bus on the strip menu)");
                        none.isEnabled = false;
                        choices.addItem (std::move (none));
                    }
                    juce::PopupMenu::MenuItemIterator it (choices);
                    while (it.next())
                        menu.addItem (juce::PopupMenu::Item (it.getItem()));
                }
                else
                {
                    menu.addSubMenu (entry.action == yesdaw::ui::UiActionId::MixerSendAdd ? "Add Send"
                                     : entry.action == yesdaw::ui::UiActionId::MixerTrackSetOutput ? "Output" : "Input",
                                     choices, enabled && choiceCount > 0);
                }
                lastContextMenu.actions.push_back (entry.action);
                continue;
            }
            const auto& descriptor = yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (entry.action)];
            // G4.1 cp2: the routed send row's verbs act on THAT row — the tap ticks when pre-fader, the
            // destination is a submenu of the buses (the current one ticked, never the owner itself).
            if (sendRowTarget && ! emptySendWell)
            {
                const yesdaw::engine::SendRow& send = sendRows[static_cast<std::size_t> (index)];
                const bool enabled = appModel.registry().stateFor (entry.action, appModel.context()).enabled;
                if (entry.action == yesdaw::ui::UiActionId::MixerSendSetTap)
                {
                    juce::PopupMenu::Item item ("Pre-fader");
                    item.itemID = static_cast<int> (entry.action) + 1;
                    item.isEnabled = enabled;
                    item.isTicked = send.tap == yesdaw::engine::SendTap::PreFader;
                    menu.addItem (std::move (item));
                    lastContextMenu.actions.push_back (entry.action);
                    continue;
                }
                if (entry.action == yesdaw::ui::UiActionId::MixerSendSetDestination)
                {
                    juce::PopupMenu destinations;
                    int destinationCount = 0;
                    const auto& project = appModel.project();
                    const yesdaw::engine::EntityId self = appModel.selectedSendOwnerEntityId();
                    for (std::size_t busIndex = 0; busIndex < project.buses.size() && busIndex < kContextMenuChoiceRange; ++busIndex)
                    {
                        if (project.buses[busIndex].id == self)
                            continue;   // R13: never a self-route
                        juce::PopupMenu::Item item (juce::String (project.buses[busIndex].strip.name));
                        item.itemID = kContextMenuSendDestBase + static_cast<int> (busIndex);
                        item.isTicked = project.buses[busIndex].id == send.busId;
                        destinations.addItem (std::move (item));
                        ++destinationCount;
                    }
                    menu.addSubMenu ("Destination", destinations, enabled && destinationCount > 0);
                    lastContextMenu.actions.push_back (entry.action);
                    continue;
                }
                juce::PopupMenu::Item item (juce::String (descriptor.label));
                item.itemID = static_cast<int> (entry.action) + 1;
                item.isEnabled = enabled;
                menu.addItem (std::move (item));
                lastContextMenu.actions.push_back (entry.action);
                continue;
            }
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
        if (target == yesdaw::ui::ContextMenuTarget::Ruler)
        {
            // G2.2: the time row's format — Min:Sec / SMPTE / Samples — one app-wide setting the
            // header counter shares (ids above the action range, like Add Insert).
            juce::PopupMenu formats;
            for (const auto& [mode, label] : std::array<std::pair<int, const char*>, 3> {
                     std::pair { yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplayMinSec, "Min:Sec" },
                     std::pair { yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySmpte, "SMPTE" },
                     std::pair { yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySamples, "Samples" } })
            {
                juce::PopupMenu::Item item (label);
                item.itemID = kContextMenuTimeDisplayBase + mode;
                item.isTicked = mode == yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplayMinSec
                                    ? timeDisplayMode <= yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplayMinSec
                                    : timeDisplayMode == mode;
                formats.addItem (std::move (item));
            }
            menu.addSubMenu ("Time Display", formats);
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
    static constexpr int kContextMenuAddInsertBase = 3001;      // + FxKind
    static constexpr int kContextMenuTimeDisplayBase = 3100;    // + time display mode (G2.2)
    static constexpr int kContextMenuAddInsertKindCount = 9;    // Eq … Limiter, the four MIDI FX (G3.8)
    // G4.1: the routing choices — a send destination (+ bus index), an output (0 Master, 1 + bus
    // index), an input (mono + channel; the pair starting at + channel). One range each.
    static constexpr int kContextMenuAddSendBase = 3200;
    static constexpr int kContextMenuOutputBase = 3300;
    static constexpr int kContextMenuInputMonoBase = 3400;
    static constexpr int kContextMenuInputPairBase = 3500;
    static constexpr int kContextMenuSendDestBase = 3600;      // G4.1 cp2: the send row's Destination (+ bus index)
    static constexpr std::size_t kContextMenuChoiceRange = 100;

    // The one path a picked context-menu item takes (the popup's callback and the harness): the
    // insert-slot verbs act on the clicked slot through the shell's per-slot handlers; every
    // other target dispatches the action.
    void invokeContextMenuItem (int itemId)
    {
        if (itemId > kContextMenuTimeDisplayBase && itemId <= kContextMenuTimeDisplayBase + yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySamples)
        {
            timeDisplayMode = itemId - kContextMenuTimeDisplayBase;   // G2.2
            refreshActionState();
            repaintAll();
            return;
        }
        if (itemId >= kContextMenuAddInsertBase && itemId < kContextMenuAddInsertBase + kContextMenuAddInsertKindCount)
        {
            (void) appModel.addFxInsertToSelectedStrip (
                static_cast<yesdaw::engine::FxKind> (itemId - kContextMenuAddInsertBase));
            refreshActionState();
            resized();
            repaintAll();
            return;
        }
        // G4.1: the routing choices act on the SELECTED strip (the click that opened the menu selected it).
        const auto inChoiceRange = [itemId] (int base) {
            return itemId >= base && itemId < base + static_cast<int> (kContextMenuChoiceRange);
        };
        if (inChoiceRange (kContextMenuAddSendBase) || inChoiceRange (kContextMenuOutputBase)
            || inChoiceRange (kContextMenuInputMonoBase) || inChoiceRange (kContextMenuInputPairBase)
            || inChoiceRange (kContextMenuSendDestBase))
        {
            if (inChoiceRange (kContextMenuAddSendBase))
            {
                (void) appModel.addSendOnSelectedTrack (static_cast<std::size_t> (itemId - kContextMenuAddSendBase));
            }
            else if (inChoiceRange (kContextMenuSendDestBase))
            {
                // G4.1 cp2: the send row's Destination — re-routes THAT row (one undo group).
                if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::MixerSendRow && lastContextMenu.index >= 0)
                    (void) appModel.setSendDestinationOnSelectedTrack (static_cast<std::size_t> (lastContextMenu.index),
                                                                      static_cast<std::size_t> (itemId - kContextMenuSendDestBase));
            }
            else if (inChoiceRange (kContextMenuOutputBase))
            {
                const int choice = itemId - kContextMenuOutputBase;
                const auto& buses = appModel.project().buses;
                if (choice == 0)
                    (void) appModel.setOutputOnSelectedTrack ({});
                else if (static_cast<std::size_t> (choice - 1) < buses.size())
                    (void) appModel.setOutputOnSelectedTrack (buses[static_cast<std::size_t> (choice - 1)].id);
            }
            else
            {
                const bool stereo = inChoiceRange (kContextMenuInputPairBase);
                const int channel = itemId - (stereo ? kContextMenuInputPairBase : kContextMenuInputMonoBase);
                const int ordinal = appModel.selectedMixerStripOrdinal();
                if (ordinal >= 0 && static_cast<std::size_t> (ordinal) < appModel.project().tracks.size())
                    (void) appModel.setRecordingInputForTrack (static_cast<std::size_t> (ordinal),
                                                               static_cast<std::uint16_t> (channel), stereo);
            }
            refreshActionState();
            resized();
            repaintAll();
            return;
        }
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
                // G4.1 cp2: Open Editor — the slot's editor over the arrangement.
                selectedFxParamSlot = slot;
                selectedFxParamPage = 0;
                fxEditorStripOrdinal = appModel.selectedMixerStripOrdinal();
                fxEditorOpen = true;
            }
            else
                return;
            refreshActionState();
            resized();
            repaintAll();
            return;
        }
        // G4.1 cp2: the routed send row's verbs act on THAT row.
        if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::MixerSendRow)
        {
            const int row = lastContextMenu.index;
            if (row < 0)
                return;
            if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerSendSetTap) + 1)
                (void) appModel.toggleSendTapOnSelectedTrack (static_cast<std::size_t> (row));
            else if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerSendRemove) + 1)
                (void) appModel.removeSendOnSelectedTrack (static_cast<std::size_t> (row));
            else
                return;
            refreshActionState();
            resized();
            repaintAll();
            return;
        }
        if (itemId <= 0 || itemId > static_cast<int> (yesdaw::ui::kUiActionCount))
            return;
        // G2.14: a marker menu pick acts on the marker under the pointer (the canvas lists markers in
        // project order, so the menu's index IS the project index).
        if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::Marker
            && itemId == static_cast<int> (yesdaw::ui::UiActionId::TimelineMarkerColourNext) + 1)
        {
            const int index = lastContextMenu.index;
            if (index >= 0 && index < static_cast<int> (appModel.project().markers.size()))
                (void) appModel.cycleMarkerColour (appModel.project().markers[static_cast<std::size_t> (index)].id);
            refreshActionState();
            repaintAll();
            return;
        }
        handleAction (static_cast<yesdaw::ui::UiActionId> (itemId - 1));
        refreshActionState();
        resized();
        repaintAll();
    }

public:
    void harnessSetDockHeight (int height) { setDockHeight (height); }   // G2.1
    double harnessTimelineAutoScrollTick() { return timelineInput.autoScrollTick(); }   // G2.3
    // G2.4: the Smart tool's zone and cursor under a shell point.
    [[nodiscard]] juce::String harnessTimelineZoneAt (juce::Point<int> shellPoint, juce::ModifierKeys modifiers) const
    {
        return timelineInput.zoneNameAt (shellPoint - timelineInput.getPosition(), modifiers);
    }
    [[nodiscard]] juce::String harnessTimelineCursorAt (juce::Point<int> shellPoint, juce::ModifierKeys modifiers) const
    {
        const juce::MouseCursor zoneCursor = timelineInput.cursorAt (shellPoint - timelineInput.getPosition(), modifiers);
        if (zoneCursor == juce::MouseCursor (juce::MouseCursor::LeftRightResizeCursor)) return "left-right";
        if (zoneCursor == juce::MouseCursor (juce::MouseCursor::TopLeftCornerResizeCursor)) return "top-left";
        if (zoneCursor == juce::MouseCursor (juce::MouseCursor::TopRightCornerResizeCursor)) return "top-right";
        if (zoneCursor == juce::MouseCursor (juce::MouseCursor::IBeamCursor)) return "ibeam";
        if (zoneCursor == juce::MouseCursor (juce::MouseCursor::DraggingHandCursor)) return "dragging-hand";   // G2.11
        if (zoneCursor == juce::MouseCursor (juce::MouseCursor::UpDownResizeCursor)) return "up-down";
        return "normal";
    }
    void harnessInvokeContextMenuId (int itemId) { invokeContextMenuItem (itemId); }   // G2.2
    [[nodiscard]] static constexpr int harnessTimeDisplayMenuId (int mode) noexcept { return kContextMenuTimeDisplayBase + mode; }
    // G4.1: the routing choices' ids, the last menu built, the view-state record.
    [[nodiscard]] static constexpr int harnessMixerInputMenuId (int channel, bool stereoPair) noexcept
    {
        return (stereoPair ? kContextMenuInputPairBase : kContextMenuInputMonoBase) + channel;
    }
    [[nodiscard]] static constexpr int harnessMixerOutputMenuId (int choice) noexcept { return kContextMenuOutputBase + choice; }
    [[nodiscard]] static constexpr int harnessMixerSendMenuId (int busIndex) noexcept { return kContextMenuAddSendBase + busIndex; }
    [[nodiscard]] static constexpr int harnessMixerSendDestinationMenuId (int busIndex) noexcept { return kContextMenuSendDestBase + busIndex; }
    // G4.1 cp2: the FX editor as the harness reads it, and the open the double-click performs.
    void harnessOpenFxEditor (int stripIndex, int slotIndex) { openFxEditor (stripIndex, slotIndex); }
    [[nodiscard]] yesdaw::ui::MainComponentFxEditor harnessFxEditor() const
    {
        yesdaw::ui::MainComponentFxEditor out;
        out.visible = fxEditor.isVisible();
        out.strip = fxEditorOpen ? fxEditorStripOrdinal : -1;
        out.slot = fxEditorOpen ? selectedFxParamSlot : -1;
        const std::vector<yesdaw::engine::FxInsert> chain = appModel.selectedStripFxChain();
        if (fxEditorOpen && selectedFxParamSlot >= 0 && static_cast<std::size_t> (selectedFxParamSlot) < chain.size())
            out.kind = fxKindName (chain[static_cast<std::size_t> (selectedFxParamSlot)].kind);
        out.bypassed = fxEditor.isBypassed();
        out.page = selectedFxParamPage;
        out.pageCount = mixerFxParamPageChooser.getNumItems();
        out.rows = static_cast<int> (lastVisibleFxParamRows);
        out.bounds = fxEditor.getBounds();
        return out;
    }
    [[nodiscard]] yesdaw::ui::MainComponentContextMenu harnessLastContextMenu() const { return lastContextMenu.toPublic(); }
    [[nodiscard]] juce::String harnessViewStateRecord() const { return juce::String (viewStateRecordText()); }

    void harnessInvokeContextMenuItem (yesdaw::ui::UiActionId action, int direction)
    {
        if (action == yesdaw::ui::UiActionId::MixerFxInsertAdd)
            invokeContextMenuItem (kContextMenuAddInsertBase + direction);   // direction carries the FxKind
        else if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::InsertSlot
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
        juce::String route = "none";
        if (timelineInput.isVisible() && timelineInput.getBounds().contains (shellPoint))
        {
            route = "timeline";
            timelineInput.requestContextMenu (shellPoint - timelineInput.getPosition());
        }
        else if (pianoRollInput.isVisible() && pianoRollInput.getBounds().contains (shellPoint))
        {
            route = "pianoRoll";
            pianoRollInput.requestContextMenu (shellPoint - pianoRollInput.getPosition());
        }
        else if (trackListInput.isVisible() && trackListInput.getBounds().contains (shellPoint))
        {
            route = "rail";
            trackListInput.requestContextMenu (shellPoint - trackListInput.getPosition());
        }
        else if (mixerStripsInput.isVisible() && mixerStripsInput.getBounds().contains (shellPoint))
        {
            route = "strips";
            mixerStripsInput.requestContextMenu (shellPoint - mixerStripsInput.getPosition());
        }
        return lastContextMenu.toPublic (route);
    }

private:
    // G1.6: the hovered zone's gesture hint; the status line shows it while no message is active.
    void setHoverHint (const juce::String& hint)
    {
        if (hoverHint == hint)
            return;
        hoverHint = hint;
        if (appModel.statusLineText().empty())
            statusLine.setText (hoverHintOrModeHint(), juce::dontSendNotification);
    }

    // G1.6: every action-backed control's tooltip quotes its LIVE chord (a rebind in the keymap
    // editor changes the tooltip), from the registry's descriptor name and the keymap.
    void refreshActionTooltips()
    {
        const auto& keymap = appModel.registry().keymap();
        for (const auto& [component, action] : actionComponents)
        {
            auto* client = dynamic_cast<juce::SettableTooltipClient*> (component);
            const auto* descriptor = appModel.registry().descriptor (action);
            if (client == nullptr || descriptor == nullptr)
                continue;
            const std::string& chord = keymap.chordFor (action);
            client->setTooltip (chord.empty() ? juce::String (descriptor->accessibleName)
                                              : juce::String (descriptor->accessibleName) + "  (" + chord + ")");
        }
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto* descriptor = appModel.registry().descriptor (toolbarActions[i]);
            if (descriptor == nullptr)
                continue;
            const std::string& chord = keymap.chordFor (toolbarActions[i]);
            buttons[i].setTooltip (juce::String (descriptor->stableId) + (chord.empty() ? juce::String() : "  " + juce::String (chord)));
        }
    }

public:
    [[nodiscard]] juce::String harnessHoverHintAt (juce::Point<int> shellPoint)
    {
        juce::String hint;
        if (timelineInput.isVisible() && timelineInput.getBounds().contains (shellPoint))
            hint = timelineInput.hintAt (shellPoint - timelineInput.getPosition(), {});
        else if (pianoRollInput.isVisible() && pianoRollInput.getBounds().contains (shellPoint))
            hint = pianoRollInput.hintAt (shellPoint - pianoRollInput.getPosition(), {});
        else if (trackListInput.isVisible() && trackListInput.getBounds().contains (shellPoint))
            hint = trackListInput.hintAt (shellPoint - trackListInput.getPosition(), {});
        else if (mixerStripsInput.isVisible() && mixerStripsInput.getBounds().contains (shellPoint))
            hint = mixerStripsInput.hintAt (shellPoint - mixerStripsInput.getPosition(), {});
        setHoverHint (hint);
        return hint;
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

    // Transport ▸ Locate Points (a submenu, not a flat run of ten): the store / recall pairs.
    static constexpr std::array<yesdaw::ui::UiActionId, 10> kLocatePointMenu {
        yesdaw::ui::UiActionId::TransportStoreLocatePoint1,  yesdaw::ui::UiActionId::TransportStoreLocatePoint2,
        yesdaw::ui::UiActionId::TransportStoreLocatePoint3,  yesdaw::ui::UiActionId::TransportStoreLocatePoint4,
        yesdaw::ui::UiActionId::TransportStoreLocatePoint5,
        yesdaw::ui::UiActionId::TransportRecallLocatePoint1, yesdaw::ui::UiActionId::TransportRecallLocatePoint2,
        yesdaw::ui::UiActionId::TransportRecallLocatePoint3, yesdaw::ui::UiActionId::TransportRecallLocatePoint4,
        yesdaw::ui::UiActionId::TransportRecallLocatePoint5,
    };

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

        // G1.7: the repeat-paste count (Ctrl+R) is a ticked Edit ▸ Repeat Count submenu — the
        // toolbar "2x" combo the sweep flagged is gone (the reference toolbar has no such thing).
        if (topLevelMenuIndex == 1)
        {
            juce::PopupMenu counts;
            for (const int count : { 2, 3, 4, 8 })
            {
                juce::PopupMenu::Item item (juce::String (count) + juce::String::charToString (0xd7));
                item.itemID = kRepeatCountMenuBaseId + count;
                item.isTicked = appModel.repeatPasteCount() == count;
                counts.addItem (std::move (item));
            }
            menu.addSubMenu ("Repeat Count", counts);
        }

        // Transport ▸ Locate Points: the five store / recall pairs. Fully implemented verbs that
        // had no chord (plan §4 assigns none — Logic has no default), no menu entry and no
        // button, so nothing but a test could reach them until 2026-09-04. Same item law as
        // the flat entries: the action's id, its live enabled state, its (empty) chord.
        if (topLevelMenuIndex == 6)
        {
            juce::PopupMenu locatePoints;
            for (const yesdaw::ui::UiActionId action : kLocatePointMenu)
            {
                const auto& descriptor =
                    yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (action)];
                juce::PopupMenu::Item item (descriptor.label);
                item.itemID = static_cast<int> (action) + 1;
                item.isEnabled = appModel.registry().stateFor (action, appModel.context()).enabled;
                item.shortcutKeyDescription = menuShortcutFor (action);
                if (action == yesdaw::ui::UiActionId::TransportRecallLocatePoint1)
                    locatePoints.addSeparator();
                locatePoints.addItem (std::move (item));
            }
            menu.addSubMenu ("Locate Points", locatePoints);
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
        if (menuItemID > kRepeatCountMenuBaseId && menuItemID <= kRepeatCountMenuBaseId + 8)
        {
            appModel.setRepeatPasteCount (menuItemID - kRepeatCountMenuBaseId);
            refreshActionState();
            repaintAll();
            return;
        }
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

            // G3.7: a MIDI file lands on the SELECTED track (else the first) at the playhead; the
            // export takes the selection, else the whole project. The model names every refusal.
            case yesdaw::ui::UiActionId::ProjectImportMidi:
                if (fileChoices.chooseImportMidiFile)
                {
                    const std::filesystem::path path = fileChoices.chooseImportMidiFile();
                    const auto& tracks = appModel.project().tracks;
                    if (! path.empty() && ! tracks.empty())
                    {
                        const std::size_t lane = selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size())
                            ? static_cast<std::size_t> (selectedTrackLane) : std::size_t {};
                        (void) appModel.importMidiFileAt (
                            path, tracks[lane].id,
                            static_cast<yesdaw::engine::Tick> (std::max<std::int64_t> (0, appModel.context().playheadFrame)));
                    }
                }
                return;

            case yesdaw::ui::UiActionId::ProjectExportMidi:
                if (fileChoices.chooseExportMidiFile)
                {
                    const std::filesystem::path path = fileChoices.chooseExportMidiFile();
                    if (! path.empty())
                        (void) appModel.exportMidiFile (path);
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

            // G4.1: the strip menus' target verbs act on the SELECTED mixer target (a Bus's menu used
            // to reach the rail's track through the Track verbs); the bus verbs need the shell too.
            case yesdaw::ui::UiActionId::MixerTargetToggleMute:
                (void) appModel.toggleSelectedMixerMute();
                return;
            case yesdaw::ui::UiActionId::MixerTargetToggleSolo:
                (void) appModel.toggleSelectedMixerSolo();
                return;
            case yesdaw::ui::UiActionId::MixerTargetToggleSoloSafe:
                (void) appModel.toggleSelectedMixerSoloSafe();
                return;
            case yesdaw::ui::UiActionId::MixerBusRemove:
                (void) appModel.removeSelectedBus();
                layoutMixerControls();
                return;
            case yesdaw::ui::UiActionId::MixerBusRename:
                if (appModel.selectedMixerTargetIsBus())
                {
                    const int ordinal = appModel.selectedMixerStripOrdinal();
                    openBusRenameEditor (ordinal - static_cast<int> (appModel.project().tracks.size()), ordinal);
                }
                return;
            case yesdaw::ui::UiActionId::MixerStripsNarrowToggle:
                (void) appModel.dispatch (action);
                saveViewState();   // the view state follows the project, like the splitters
                resized();
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
                // G2.5: with a Time selection and no clip selected, Del clears the range.
                if (appModel.context().timelineRangeSelected && ! appModel.context().timelineClipSelected
                    && appModel.context().activePanel != yesdaw::ui::UiPanel::PianoRoll)
                {
                    (void) appModel.dispatch (yesdaw::ui::UiActionId::TimelineRangeDelete);
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
                    (void) appModel.duplicateSelectedPianoRollNote (0);   // G3.2 checkpoint: right after the note
                    return;
                }
                (void) appModel.dispatch (action);
                return;

            case yesdaw::ui::UiActionId::PianoRollNoteDuplicate:
                (void) appModel.duplicateSelectedPianoRollNote (0);   // G3.2 checkpoint: right after the note
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
                            timelineZoomCeiling());
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

            case yesdaw::ui::UiActionId::TimelineZoomTracksIn:   // G2.16
                if (appModel.dispatch (action).dispatched)
                    zoomTracksBy (yesdaw::ui::UiTheme::Layout::timelineRowZoomStep);
                return;
            case yesdaw::ui::UiActionId::TimelineZoomTracksOut:
                if (appModel.dispatch (action).dispatched)
                    zoomTracksBy (1.0 / yesdaw::ui::UiTheme::Layout::timelineRowZoomStep);
                return;
            case yesdaw::ui::UiActionId::TimelineZoomBack:
                if (appModel.dispatch (action).dispatched)
                    (void) popZoomHistory();
                return;
            case yesdaw::ui::UiActionId::TimelineZoomToSelection:
            {
                // G2.5 (R22): fit the Time selection with a small margin — the zoom law is the
                // view's (fit × factor), so the factor and scroll are set here, then clamped.
                // G2.16: Z toggles — when the view already IS the selection's, go back instead.
                if (lastSelectionZoom && lastSelectionZoom->zoom == timelineZoomFactor
                    && lastSelectionZoom->scroll == timelineScrollSeconds && popZoomHistory())
                {
                    lastSelectionZoom.reset();
                    (void) appModel.dispatch (action);
                    return;
                }
                const MainComponentSnapshotLike view = snapshotForZoom();
                if (view.rangeSeconds > 0.0 && view.fitPixelsPerSecond > 0.0)
                {
                    pushZoomHistory();
                    const double margin = view.rangeSeconds * yesdaw::ui::UiTheme::Layout::timelineZoomToSelectionMarginFraction;
                    const double wanted = view.rangeSeconds + margin * 2.0;
                    timelineZoomFactor = std::clamp (
                                                     (view.widthPixels / wanted) / view.fitPixelsPerSecond,
                                                     yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                                                     timelineZoomCeiling());
                    timelineScrollSeconds = std::max (0.0, view.rangeStartSeconds - margin);
                    refreshTimelineZoomReadout();
                    lastSelectionZoom = ZoomView { timelineZoomFactor, timelineScrollSeconds };
                }
                (void) appModel.dispatch (action);
                return;
            }

            case yesdaw::ui::UiActionId::TimelineClipCut:
            case yesdaw::ui::UiActionId::TimelineClipCopy:
                // G2.5: with a Time selection and no clip selected, the clip chords act on the range.
                if (appModel.context().timelineRangeSelected && ! appModel.context().timelineClipSelected
                    && appModel.context().activePanel != yesdaw::ui::UiPanel::PianoRoll)
                {
                    (void) appModel.dispatch (action == yesdaw::ui::UiActionId::TimelineClipCut ? yesdaw::ui::UiActionId::TimelineRangeCut
                                              : action == yesdaw::ui::UiActionId::TimelineClipCopy ? yesdaw::ui::UiActionId::TimelineRangeCopy
                                                                                                    : yesdaw::ui::UiActionId::TimelineRangeDelete);
                    return;
                }
                (void) appModel.dispatch (action);
                return;

            case yesdaw::ui::UiActionId::ViewPianoRoll:
                (void) appModel.dispatch (action);
                // G2.1 cp2: P toggles the tab — only a SHOWN roll wants a clip (selecting one
                // opens the editor, which would undo the close).
                if (dockShowsPianoRoll())
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
        loadViewStateIfBundleChanged();   // G2.1
        restoreControlsHiddenByDockTab();   // G2.1 cp2: the laws below decide afresh
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
            // G1.7: Comp belongs to the G7 take-lane UI — hidden until then (the verb stays
            // dispatchable through the menu and the harness).
            buttons[i].setVisible ((! isSettingsRowAction (action) || appModel.context().settingsRowVisible)
                                   && action != yesdaw::ui::UiActionId::RecordingAssembleComp);
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
                                           || (action == yesdaw::ui::UiActionId::ViewMixer && dockShowsMixer())
                                           || (action == yesdaw::ui::UiActionId::ViewPianoRoll && dockShowsPianoRoll()),
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
        // G2.1 cp2: the arrangement is always there; the dock shows one editor tab.
        timelineInput.setVisible (true);
        playheadLayer.setVisible (true);
        pianoRollInput.setVisible (dockShowsPianoRoll());
        pianoRollLaneChooser.setVisible (dockShowsPianoRoll());   // G3.3
        pianoRollKeyChooser.setVisible (dockShowsPianoRoll());    // G3.8
        pianoRollScaleChooser.setVisible (dockShowsPianoRoll());
        pianoRollKeyChooser.setSelectedId (appModel.context().pianoRollScaleRoot + 1, juce::dontSendNotification);
        pianoRollScaleChooser.setSelectedId (appModel.context().pianoRollScaleChoice + 1, juce::dontSendNotification);
        pianoRollLaneChooser.setSelectedId (appModel.context().pianoRollControlLaneChoice + 1, juce::dontSendNotification);
        pianoRollTypingButton.setVisible (dockShowsPianoRoll());   // G3.6
        pianoRollTypingButton.setToggleState (appModel.context().musicalTypingOn, juce::dontSendNotification);
        pianoRollStepButton.setVisible (dockShowsPianoRoll());
        pianoRollStepButton.setToggleState (appModel.context().stepInputOn, juce::dontSendNotification);
        pianoRollStepButton.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::PianoRollStepInputToggle, appModel.context()).enabled);
        mixerStripsInput.setVisible (dockShowsMixer());
        instrumentPanel.setVisible (dockShowsInstrument());   // G3.1
        if (dockShowsInstrument())
            instrumentPanel.refresh();
        {
            refreshingTimeMapControls = true;
            const bool tempoEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::TransportSetTempo, appModel.context()).enabled;
            headerTempoControl.setEnabled (tempoEnabled);
            headerMeterChooser.setEnabled (
                appModel.registry().stateFor (yesdaw::ui::UiActionId::TransportSetMeter, appModel.context()).enabled);
            if (appModel.context().projectLoaded && ! appModel.project().tempoMap.empty())
                headerTempoControl.setValue (appModel.tempoAtPlayhead(), juce::dontSendNotification);   // G2.15: the tempo in force
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
            // G4.1 cp2: the editor follows the SELECTED strip's selected slot; a strip change, an empty
            // selection or a removed insert closes it (it never shows another strip's effect by accident).
            if (selectedFxParamSlot >= 0 && static_cast<std::size_t> (selectedFxParamSlot) >= chain.size())
                selectedFxParamSlot = -1;
            if (fxEditorOpen && (selectedFxParamSlot < 0 || appModel.selectedMixerStripOrdinal() != fxEditorStripOrdinal))
                fxEditorOpen = false;
            const bool editorShown = fxEditorOpen && dockShowsMixer();
            if (editorShown)
            {
                const yesdaw::engine::FxInsert& insert = chain[static_cast<std::size_t> (selectedFxParamSlot)];
                const yesdaw::ui::UiMixerStrip* strip = nullptr;
                const auto surface = currentMixerSurface();
                const int ordinal = appModel.selectedMixerStripOrdinal();
                if (ordinal >= 0 && static_cast<std::size_t> (ordinal) < surface.tracks.size())
                    strip = &surface.tracks[static_cast<std::size_t> (ordinal)];
                else if (ordinal >= 0 && static_cast<std::size_t> (ordinal) - surface.tracks.size() < surface.buses.size())
                    strip = &surface.buses[static_cast<std::size_t> (ordinal) - surface.tracks.size()];
                fxEditor.setTitleText (juce::String (fxKindName (insert.kind))
                                       + juce::String::fromUTF8 (" \xc2\xb7 ") + (strip != nullptr ? juce::String (strip->name) : juce::String ("Master"))
                                       + juce::String::fromUTF8 (" \xc2\xb7 slot ") + juce::String (selectedFxParamSlot + 1));
                fxEditor.setBypassed (! insert.enabled);
            }
            if (fxEditor.isVisible() != editorShown)
            {
                fxEditor.setVisible (editorShown);
                if (editorShown)
                    fxEditor.toFront (false);
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
            refreshingEditModeChooser = true;   // G2.6
            editModeChooser.setSelectedId (static_cast<int> (appModel.context().editMode) + 1, juce::dontSendNotification);
            refreshingEditModeChooser = false;
            refreshingSnapModeChooser = true;   // G2.7
            snapModeChooser.setSelectedId (static_cast<int> (appModel.context().snapMode) + 1, juce::dontSendNotification);
            refreshingSnapModeChooser = false;
            refreshingNudgeChooser = false;
            refreshActionTooltips();
            inspectorToggle.setToggleState (appModel.context().inspectorVisible, juce::dontSendNotification);
            refreshingSnapChooser = false;
        }
        const bool railVisible = true;   // G2.1 cp2: no modal mixer view
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
        const bool inspectorVisible = appModel.context().timelineClipSelected
                                   && ! appModel.context().inspectorTrackTabActive;
        inspectorStart.setVisible (inspectorVisible);
        inspectorEnd.setVisible (inspectorVisible);
        inspectorLength.setVisible (inspectorVisible);
        inspectorGain.setVisible (inspectorVisible);
        inspectorStretch.setVisible (inspectorVisible);
        inspectorFadeIn.setVisible (inspectorVisible);
        inspectorFadeOut.setVisible (inspectorVisible);
        inspectorFadeCurve.setVisible (inspectorVisible);
        inspectorFadeCurveAmount.setVisible (inspectorVisible);
        inspectorMarkerList.setVisible (inspectorVisible);   // G2.14
        refreshAutomationLaneControls();
        refreshInspectorControls();
        refreshMixerControls();
        {
            // G3.1: the inspector's instrument chooser mirrors the selected Track's slot; the
            // panel re-reads its rows whenever it shows.
            refreshingInspectorControls = true;
            const yesdaw::engine::Track* const instrumentTrack = appModel.selectedTrackForInstrument();
            const bool instrumentEnabled = instrumentTrack != nullptr
                && appModel.registry().stateFor (yesdaw::ui::UiActionId::TrackSetInstrument, appModel.context()).enabled;
            inspectorInstrumentChooser.setEnabled (instrumentEnabled);
            inspectorInstrumentChooser.setVisible (appModel.context().inspectorTrackTabActive);
            inspectorInstrumentEdit.setVisible (appModel.context().inspectorTrackTabActive);
            inspectorInstrumentEdit.setEnabled (instrumentTrack != nullptr);
            inspectorInstrumentEdit.setToggleState (dockShowsInstrument(), juce::dontSendNotification);
            if (instrumentTrack != nullptr)
                inspectorInstrumentChooser.setSelectedId (static_cast<int> (instrumentTrack->instrumentKind) + 1, juce::dontSendNotification);
            refreshingInspectorControls = false;
            if (dockShowsInstrument())
                instrumentPanel.refresh();
        }
        mixerDockToggle.setToggleState (dockShowsMixer(), juce::dontSendNotification);   // G2.1 cp2: the mixer TAB
        // No effect in the full-view Mixer panel (it never reserves dock space to begin with).
        mixerDockToggle.setVisible (true);
        // V7: the tab buttons live wherever the inspector panel does; the active tab lights.
        const bool inspectorPanelVisible = true;   // G2.1 cp2
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
        const bool timelineVisible = true;   // G2.1 cp2/cp3: the arrangement never leaves (no modal views)
        automationLaneToggle.setVisible (timelineVisible);
        automationLaneToggle.setEnabled (state.enabled);
        // V8: the zoom cluster lives and dies with the same toolbar row; the readout re-reads
        // the ONE shared zoom factor every gesture mutates.
        timelineZoomOutButton.setVisible (timelineVisible);
        timelineZoomInButton.setVisible (timelineVisible);
        timelineZoomSlider.setVisible (timelineVisible);   // G2.16
        timelineHScroll.setVisible (timelineVisible);
        timelineVScroll.setVisible (timelineVisible);
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
        // G4.1 cp2: one sample per tick — the painted drags sample on the release too (the live slider
        // spoke only on a value change), and a second breakpoint at one tick refuses the whole commit.
        if (! automationTouchRideSamples.empty() && automationTouchRideSamples.back().tick == tick)
        {
            automationTouchRideSamples.back().value = normalizedValue;
            return;
        }
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

    void refreshInspectorTakesVisibility()
    {
        const bool selected = appModel.context().timelineClipSelected
                           && findProjectClipById (appModel.selectedTimelineClipId()) != nullptr;
        const bool takesVisible = selected && ! inspectorTakeViews.empty()
                               && ! inspectorTakeChooser.getBounds().isEmpty();
        inspectorTakeChooser.setVisible (takesVisible);
        inspectorTakeDelete.setVisible (takesVisible);
        inspectorTakeChooser.setEnabled (takesVisible);
        inspectorTakeDelete.setEnabled (takesVisible && inspectorTakeChooser.getSelectedId() > 0);
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
        inspectorStretch.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipTimeStretch,
                                                                   appModel.context()).enabled);   // G2.9b
        inspectorFadeIn.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                  appModel.context()).enabled);
        inspectorFadeOut.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                   appModel.context()).enabled);
        inspectorFadeCurve.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                     appModel.context()).enabled);
        inspectorFadeCurveAmount.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::TimelineClipSetFades,
                                                                           appModel.context()).enabled);   // G2.10

        refreshingInspectorControls = true;
        {
            // G3.4: the quantize panel follows the context's settings; it shows only for a MIDI clip on the CLIP tab.
            const bool showQuantize = inspectorShowsQuantizePanel();
            const auto& context = appModel.context();
            inspectorQuantizeGrid.setSelectedId (context.quantizeGridChoice + 1, juce::dontSendNotification);
            inspectorQuantizeStrength.setValue (context.quantizeStrengthPercent, juce::dontSendNotification);
            inspectorQuantizeSwing.setValue (context.quantizeSwingPercent, juce::dontSendNotification);
            inspectorQuantizeEnds.setToggleState (context.quantizeNoteEnds, juce::dontSendNotification);
            inspectorQuantizeHumanize.setValue (context.quantizeHumanizePercent, juce::dontSendNotification);
            // G3.5: the clip's own rows follow the selected MIDI clip.
            if (const yesdaw::engine::MidiClip* const midiClip = appModel.selectedMidiClip())
            {
                inspectorMidiMute.setToggleState (midiClip->muted, juce::dontSendNotification);
                inspectorMidiTranspose.setValue (midiClip->transposeSemitones, juce::dontSendNotification);
                inspectorMidiVelocity.setValue (juce::roundToInt (midiClip->velocityOffset * 100.0), juce::dontSendNotification);
                const int loopChoice = appModel.midiClipLoopChoiceFor (*midiClip);
                inspectorMidiLoop.setSelectedId (loopChoice >= 0 ? loopChoice + 1 : 0, juce::dontSendNotification);
            }
            for (juce::Component* component : { static_cast<juce::Component*> (&inspectorMidiMute),
                                                static_cast<juce::Component*> (&inspectorMidiTranspose),
                                                static_cast<juce::Component*> (&inspectorMidiVelocity),
                                                static_cast<juce::Component*> (&inspectorMidiLoop),
                                                static_cast<juce::Component*> (&inspectorQuantizeGrid),
                                                static_cast<juce::Component*> (&inspectorQuantizeStrength),
                                                static_cast<juce::Component*> (&inspectorQuantizeSwing),
                                                static_cast<juce::Component*> (&inspectorQuantizeEnds),
                                                static_cast<juce::Component*> (&inspectorQuantizeHumanize),
                                                static_cast<juce::Component*> (&inspectorQuantizeApply) })
            {
                component->setVisible (showQuantize);
                component->setEnabled (showQuantize);
            }
            inspectorQuantizeApply.setEnabled (showQuantize
                && appModel.registry().stateFor (yesdaw::ui::UiActionId::PianoRollNoteQuantizeSelection, context).enabled);
        }
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
            inspectorStretch.setValue (std::clamp (static_cast<double> (clip->stretchFactor) * 100.0,
                                                   yesdaw::ui::UiTheme::Layout::inspectorStretchSliderMin,
                                                   yesdaw::ui::UiTheme::Layout::inspectorStretchSliderMax),
                                       juce::dontSendNotification);   // G2.9b
            inspectorFadeIn.setValue (std::clamp (static_cast<double> (clip->fadeIn) / sampleRate,
                                                  yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMinSeconds,
                                                  yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMaxSeconds),
                                      juce::dontSendNotification);
            inspectorFadeOut.setValue (std::clamp (static_cast<double> (clip->fadeOut) / sampleRate,
                                                   yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMinSeconds,
                                                   yesdaw::ui::UiTheme::Layout::inspectorFadeSliderMaxSeconds),
                                        juce::dontSendNotification);
            inspectorFadeCurve.setSelectedId (inspectorIdForFadeShape (clip->fadeInShape), juce::dontSendNotification);   // G2.10
            inspectorFadeCurveAmount.setValue (std::clamp (static_cast<double> (clip->fadeInCurve) * 100.0,
                                                           yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountMin,
                                                           yesdaw::ui::UiTheme::Layout::inspectorFadeCurveAmountMax),
                                               juce::dontSendNotification);
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
            inspectorFadeCurveAmount.setValue (0.0, juce::dontSendNotification);
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
        // G4.1 cp2: the lane's live fader / pan / M / S are gone; the master fader is the one live
        // strip control left, on the master pane.
        refreshingMixerControls = true;
        // E19: the master fader reflects the persisted master gain and enables with a project.
        mixerMasterFader.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerMasterSetFader,
                                          appModel.context()).enabled);
        mixerMasterFader.setValue (appModel.context().projectLoaded
                                       ? static_cast<double> (appModel.project().masterLinearGain)
                                       : yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault,
                                   juce::dontSendNotification);
        refreshingMixerControls = false;
    }

    // G4.1: the I/O slots' texts — ONE law for the paint and the harness.
    [[nodiscard]] juce::String stripInputText (std::size_t trackIndex) const
    {
        for (const yesdaw::ui::UiRecordingTrackInputSelection& armed : appModel.armedRecordingTrackInputs())
            if (armed.armed && armed.trackIndex == trackIndex)
                return "In: " + juce::String (static_cast<int> (armed.inputChannel) + 1)
                     + (armed.stereoPair ? "+" + juce::String (static_cast<int> (armed.inputChannel) + 2) : juce::String());
        return juce::String::fromUTF8 ("In: \xe2\x80\x94");
    }

    [[nodiscard]] juce::String stripOutputText (const yesdaw::ui::UiMixerStrip& strip) const
    {
        if (strip.outputBusId.isValid())
            if (const yesdaw::engine::Bus* const bus = appModel.project().findBus (strip.outputBusId))
                return "Out: " + juce::String (bus->strip.name);
        return "Out: Master";
    }

    [[nodiscard]] static const char* fxKindStripName (yesdaw::engine::FxKind kind) noexcept
    {
        switch (kind)
        {
            case yesdaw::engine::FxKind::Eq: return "EQ";
            case yesdaw::engine::FxKind::Compressor: return "Comp";
            case yesdaw::engine::FxKind::Delay: return "Delay";
            case yesdaw::engine::FxKind::Reverb: return "Reverb";
            case yesdaw::engine::FxKind::Limiter: return "Limiter";
            case yesdaw::engine::FxKind::MidiTranspose: return "Transpose";   // G3.8
            case yesdaw::engine::FxKind::MidiScaleMap: return "Scale";
            case yesdaw::engine::FxKind::MidiArpeggiator: return "Arp";
            case yesdaw::engine::FxKind::MidiChord: return "Chord";
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
            case yesdaw::engine::FxKind::MidiTranspose: return "MIDI Transpose";   // G3.8
            case yesdaw::engine::FxKind::MidiScaleMap: return "MIDI Scale";
            case yesdaw::engine::FxKind::MidiArpeggiator: return "Arpeggiator";
            case yesdaw::engine::FxKind::MidiChord: return "Chord Trigger";
        }

        return "Unknown";
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
        // G2.15: the FULL maps — frame -> tick through the compiled tempo map's inverse, then the
        // meter walk; the single-tempo law only when the map cannot be built.
        yesdaw::engine::CompiledTempoMap compiled;
        yesdaw::engine::BarBeat piecewise;
        if (appModel.project().sampleRate.isValid() && appModel.compiledTempoMap (compiled)
            && yesdaw::engine::computeBarBeatPiecewise (
                   compiled,
                   yesdaw::engine::MeterMapView { appModel.project().meterMap.data(), appModel.project().meterMap.size() },
                   appModel.context().playheadFrame, piecewise))
            return piecewise;
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
        // G2.2: SMPTE and samples share the ruler's formatters (one law, two readouts).
        if (timeDisplayMode == yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySmpte)
            return CounterStrings { yesdaw::ui::timeline_canvas_detail::formatSmpte (seconds), bars, "smpte" };
        if (timeDisplayMode == yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySamples)
            return CounterStrings { yesdaw::ui::timeline_canvas_detail::formatRulerTime (seconds, timeDisplayMode, 0.0, sampleRate), bars, "samples" };
        return timeDisplayMode == 0 ? CounterStrings { bars, minSec, "bars" }
                                    : CounterStrings { minSec, bars, "minsec" };
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const HeaderLayout header = headerLayout();
        if (header.timeReadout.contains (event.getPosition()))
        {
            timeDisplayMode = (timeDisplayMode + 1) % (yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySamples + 1);   // G2.2: bars → min:sec → SMPTE → samples
            repaintAll();   // the ruler's time row follows
            return;
        }
        // The header's gear: painted since G0.7, dead to the mouse until 2026-09-04. It is the
        // settings row's toggle — the same action the View menu carries — and lights while the
        // row shows.
        if (header.gear.contains (event.getPosition()))
        {
            handleAction (yesdaw::ui::UiActionId::ViewToggleSettingsRow);
            refreshActionState();
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

        // G3.10: the MIDI input lamp in the time readout's corner — lit while a played note is fresh.
        if (! h.midiIn.isEmpty())
        {
            const bool lit = midiInLitUntil != std::chrono::steady_clock::time_point {}
                          && std::chrono::steady_clock::now() < midiInLitUntil;
            g.setColour (lit ? yesdaw::ui::UiTheme::Color::midiInLampLit() : yesdaw::ui::UiTheme::Color::midiInLampOff());
            g.fillRoundedRectangle (h.midiIn.toFloat(), yesdaw::ui::UiTheme::Layout::headerMidiInLampCornerRadius);
            g.setColour (lit ? yesdaw::ui::UiTheme::Color::pianoWhiteKeyText() : yesdaw::ui::UiTheme::Color::mutedText());
            g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny, juce::Font::bold));
            g.drawText ("MIDI", h.midiIn, juce::Justification::centred, false);
        }
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
        juce::Rectangle<int> midiIn;   // G3.10: the MIDI input lamp
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
        // G3.10: the MIDI input lamp lives in the time readout's top-right corner (Logic's LCD carries
        // its MIDI activity the same way) — no width added to the centred cluster.
        h.midiIn = juce::Rectangle<int> (h.timeReadout.getRight() - L::headerMidiInLampInset - L::headerMidiInLampWidth,
                                         h.timeReadout.getY() + L::headerMidiInLampInset,
                                         L::headerMidiInLampWidth, L::headerMidiInLampHeight);
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
    KeymapEditorComponent& harnessKeymapEditor() noexcept { return keymapEditor; }

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
                appModel.context().settingsRowVisible ? kText : kMutedText);
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
            appModel.context().settingsRowVisible ? kText : kMutedText);
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
                trackLaneIsSelected (static_cast<int> (i))   // G2.17: every selected lane highlights
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
            // G2.17: the kind badge — MIDI when the track holds MIDI clips, audio otherwise.
            yesdaw::ui::drawTrackKindBadge (
                g,
                trackHoldsMidi (i),
                juce::Rectangle<float> (
                    static_cast<float> (row.getX() + yesdaw::ui::UiTheme::Layout::trackListIconLeftInset
                                        + yesdaw::ui::UiTheme::Layout::trackListIconSize + yesdaw::ui::UiTheme::Layout::trackListKindBadgeGap),
                    static_cast<float> (row.getY() + yesdaw::ui::UiTheme::Layout::trackListIconTopInset
                                        + yesdaw::ui::UiTheme::Layout::trackListIconSize - yesdaw::ui::UiTheme::Layout::trackListKindBadgeSize),
                    static_cast<float> (yesdaw::ui::UiTheme::Layout::trackListKindBadgeSize),
                    static_cast<float> (yesdaw::ui::UiTheme::Layout::trackListKindBadgeSize)),
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
        state.activeTool = appModel.context().activeTimelineTool;   // the strip lights this cell
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
            // G2.19: decoded samples for the zoomed-in paint — the same buffers playback reads.
            state.waveformSampleLookup = [this] (int layoutClipId) -> yesdaw::ui::WaveformSampleSource
            {
                if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipAssetIds.size()))
                    return {};
                const yesdaw::ui::UiDecodedAsset* decoded =
                    appModel.findDecodedAsset (timelineClipAssetIds[static_cast<std::size_t> (layoutClipId)]);
                if (decoded == nullptr)
                    return {};
                return { std::span<const float> (decoded->interleavedSamples.data(), decoded->interleavedSamples.size()),
                         decoded->channels, decoded->frames };
            };
            state.totalSeconds = timelineTotalSeconds;
            state.rowZoom = timelineRowZoom;   // G2.16
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
        // G2.2: the time row follows the app-wide time display (the header counter's law).
        state.timeDisplayMode = timeDisplayMode;
        state.sampleRateHz = appModel.project().sampleRate.isValid() ? appModel.project().sampleRate.hz : 48000.0;

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
                                                 timelineMarkerLabels.back().c_str(),
                                                 marker.colour });   // G2.14
            }
        }
        state.markers = timelineMarkerViews.empty() ? nullptr : timelineMarkerViews.data();
        state.markerCount = static_cast<int> (timelineMarkerViews.size());

        // G2.15: the tempo and meter changes after the head, placed by the compiled map (so a ramp
        // puts the next change where it really lands), labelled "120" / "120~" / "3/4".
        timelineMapLabelTexts.clear();
        timelineMapLabelViews.clear();
        timelineMapLabelFrames.clear();
        {
            yesdaw::engine::CompiledTempoMap compiled;
            if (appModel.context().projectLoaded && appModel.project().sampleRate.isValid()
                && appModel.compiledTempoMap (compiled))
            {
                const double sampleRateHz = appModel.project().sampleRate.hz;
                const auto& tempoMap = appModel.project().tempoMap;
                const auto& meterMap = appModel.project().meterMap;
                timelineMapLabelTexts.reserve (tempoMap.size() + meterMap.size());
                timelineMapLabelViews.reserve (tempoMap.size() + meterMap.size());
                for (std::size_t i = 1; i < tempoMap.size(); ++i)
                {
                    double frame = 0.0;
                    if (! compiled.frameForTick (tempoMap[i].tick, frame))
                        continue;
                    timelineMapLabelTexts.push_back (juce::String (juce::roundToInt (tempoMap[i].bpm)).toStdString()
                                                     + (tempoMap[i].curveToNext == yesdaw::engine::TempoCurve::LinearRamp ? "~" : ""));
                    timelineMapLabelViews.push_back ({ frame / sampleRateHz, timelineMapLabelTexts.back().c_str() });
                    timelineMapLabelFrames.push_back (static_cast<std::int64_t> (std::llround (frame)));
                }
                for (std::size_t i = 1; i < meterMap.size(); ++i)
                {
                    double frame = 0.0;
                    if (! compiled.frameForTick (meterMap[i].tick, frame))
                        continue;
                    timelineMapLabelTexts.push_back (std::to_string (meterMap[i].numerator) + "/" + std::to_string (meterMap[i].denominator));
                    timelineMapLabelViews.push_back ({ frame / sampleRateHz, timelineMapLabelTexts.back().c_str() });
                    timelineMapLabelFrames.push_back (static_cast<std::int64_t> (std::llround (frame)));
                }
            }
        }
        state.mapLabels = timelineMapLabelViews.empty() ? nullptr : timelineMapLabelViews.data();
        state.mapLabelCount = static_cast<int> (timelineMapLabelViews.size());

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
        timelineClipAssetIds.clear();
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
            // The clip's source window rides along (srcOffset / srcLen, asset frames) so the
            // painter draws THIS clip's audio, not the whole file squeezed into its width.
            timelineClips.push_back ({ id, lane, startSeconds, lengthSeconds, clip.name.c_str(),
                                       clip.srcOffset, clip.srcLen });
            // V6: selection is a painted RING, not a colour swap — the clip keeps its N7 track
            // colour while selected (the old accent-blue swap was invisible on a blue track,
            // the exact false-positive risk the N7 gate had to work around).
            timelineClipStyles.push_back ({ clip.colour != yesdaw::engine::kTrackColourUnset
                                                ? juce::Colour (clip.colour)                 // G2.12: the clip's own colour
                                                : colourForTrack (*track, kPurple),
                                            yesdaw::ui::UiTheme::Tone::mainComponentProjectClipAlpha,
                                            appModel.isTimelineClipSelected (clip.id),
                                            static_cast<long long> (clip.timelineLength),
                                            static_cast<long long> (clip.fadeIn),
                                            static_cast<long long> (clip.fadeOut),
                                            static_cast<int> (clip.fadeInShape),      // G2.10
                                            static_cast<int> (clip.fadeOutShape),
                                            clip.fadeInCurve,
                                            clip.fadeOutCurve,
                                            clip.muted,      // G2.12
                                            clip.reversed });   // G2.13
            timelineClipIds.push_back (clip.id);
            timelineClipAssetHashes.push_back (asset->contentHash);
            timelineClipAssetIds.push_back (asset->id);
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
                                            0,
                                            1, 1, 0.0f, 0.0f,
                                            midiClip.muted,   // G3.5: the same dim wash an audio clip gets
                                            false });
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

        // G3.1: the Track's instrument parameters (when it holds MIDI — the instrument exists).
        {
            bool holdsMidi = false;
            for (const yesdaw::engine::MidiClip& clip : appModel.project().midiClips)
                if (clip.trackId == trackId)
                    holdsMidi = true;
            if (holdsMidi)
                for (std::uint32_t paramId = 1; paramId <= yesdaw::engine::SimpleSynthNode::kParameterCount; ++paramId)
                {
                    if (! yesdaw::engine::instrumentKindAcceptsParameterId (track->instrumentKind, paramId))
                        continue;
                    const yesdaw::engine::ParamSpec spec = yesdaw::engine::instrumentParamSpecForKind (track->instrumentKind, paramId);
                    options.push_back ({ yesdaw::engine::AutomationTargetRole::InstrumentParam, paramId, trackId,
                                         "Inst " + juce::String (spec.name).fromLastOccurrenceOf (".", false, false) });
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
        {
            const yesdaw::engine::Clip* moving = nullptr;   // G2.7: Relative / Events read the clip
            for (const yesdaw::engine::Clip& clip : appModel.project().clips)
                if (clip.id == draggedClipId)
                    moving = &clip;
            (void) appModel.moveSelectedTimelineClipTo (snappedTimelineTickFrom (
                *tick, snapToGrid,
                moving != nullptr ? std::optional<yesdaw::engine::Tick> (moving->timelineStart) : std::nullopt,
                std::optional<yesdaw::engine::EntityId> (draggedClipId)));
        }

        refreshActionState();
        repaintAll();
    }

    // The active snap grid applied to a gesture tick. The gesture's Ctrl flag INVERTS the global
    // grid: grid on -> Ctrl drags fine; grid off -> Ctrl snaps one-shot.
    // G2.7: the grid a drag lands on. Grid mode subdivides the chosen unit while a cell stays at
    // least timelineSnapMinGridPx wide at the current zoom (halving as you zoom in); the other
    // modes use the unit as chosen.
    [[nodiscard]] std::int64_t effectiveSnapGridTicks() const
    {
        std::int64_t grid = appModel.context().snapGridTicks;
        if (grid <= 0 || appModel.context().snapMode != yesdaw::ui::UiSnapMode::Grid)
            return grid;
        const yesdaw::engine::Project& project = appModel.project();
        if (! project.sampleRate.isValid())
            return grid;
        const double pps = timelinePixelsPerSecondFor (timelineTotalSeconds);
        if (pps <= 0.0)
            return grid;
        for (int k = 0; k < yesdaw::ui::UiTheme::Layout::timelineSnapMaxSubdivisions; ++k)
        {
            const std::int64_t half = grid / 2;
            if (half <= 0 || grid % 2 != 0)
                break;
            if (static_cast<double> (half) / project.sampleRate.hz * pps < static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineSnapMinGridPx))
                break;
            grid = half;
        }
        return grid;
    }

    // The Events mode's candidates: every clip edge (except the dragged clip's own), every marker,
    // the playhead and the loop edges; the nearest within the tolerance wins, else no snap.
    [[nodiscard]] std::optional<yesdaw::engine::Tick> snapTickToEvents (yesdaw::engine::Tick tick,
                                                                       std::optional<yesdaw::engine::EntityId> excludeClip) const
    {
        const yesdaw::engine::Project& project = appModel.project();
        if (! project.sampleRate.isValid())
            return std::nullopt;
        const double pps = timelinePixelsPerSecondFor (timelineTotalSeconds);
        if (pps <= 0.0)
            return std::nullopt;
        const auto tolerance = static_cast<yesdaw::engine::Tick> (
            std::llround (static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineSnapEventTolerancePx) / pps * project.sampleRate.hz));
        std::optional<yesdaw::engine::Tick> best;
        const auto consider = [&] (yesdaw::engine::Tick candidate)
        {
            const auto distance = static_cast<yesdaw::engine::Tick> (std::llabs (static_cast<long long> (candidate) - static_cast<long long> (tick)));
            if (distance > tolerance)
                return;
            if (! best || distance < static_cast<yesdaw::engine::Tick> (std::llabs (static_cast<long long> (*best) - static_cast<long long> (tick))))
                best = candidate;
        };
        for (const yesdaw::engine::Clip& clip : project.clips)
        {
            if (excludeClip && clip.id == *excludeClip)
                continue;
            consider (clip.timelineStart);
            consider (clip.timelineStart + clip.timelineLength);
        }
        for (const yesdaw::engine::Marker& marker : project.markers)
            consider (marker.tick);
        consider (static_cast<yesdaw::engine::Tick> (std::max<std::int64_t> (0, appModel.context().playheadFrame)));
        if (appModel.playbackLoopEndFrame() > appModel.playbackLoopStartFrame())
        {
            consider (static_cast<yesdaw::engine::Tick> (appModel.playbackLoopStartFrame()));
            consider (static_cast<yesdaw::engine::Tick> (appModel.playbackLoopEndFrame()));
        }
        return best;
    }

    [[nodiscard]] yesdaw::engine::Tick snappedTimelineTick (yesdaw::engine::Tick tick, bool invertSnap) const
    {
        return snappedTimelineTickFrom (tick, invertSnap, std::nullopt, std::nullopt);
    }

    // G2.7: ONE snap law for every drop. `origin` is the dragged clip's start before the drag
    // (Relative mode snaps the distance from it); `movingClip` is excluded from the Events.
    [[nodiscard]] yesdaw::engine::Tick snappedTimelineTickFrom (yesdaw::engine::Tick tick, bool invertSnap,
                                                               std::optional<yesdaw::engine::Tick> origin,
                                                               std::optional<yesdaw::engine::EntityId> movingClip) const
    {
        const yesdaw::ui::UiSnapMode mode = appModel.context().snapMode;
        const bool unitOn = appModel.context().snapEnabled;
        // Off (or the unit chooser's Off) snaps nothing; Ctrl inverts: it snaps to the grid.
        const bool modeOff = mode == yesdaw::ui::UiSnapMode::Off || ! unitOn;
        const bool shouldSnap = modeOff ? invertSnap : ! invertSnap;
        if (! shouldSnap)
            return tick;
        if (! modeOff && mode == yesdaw::ui::UiSnapMode::Events)
        {
            if (const std::optional<yesdaw::engine::Tick> hit = snapTickToEvents (tick, movingClip))
                return std::max<yesdaw::engine::Tick> (0, *hit);
            return tick;
        }
        const std::int64_t gridTicks = modeOff ? appModel.context().snapGridTicks : effectiveSnapGridTicks();
        if (gridTicks <= 0)
            return tick;
        yesdaw::engine::Tick snapped = 0;
        if (! modeOff && mode == yesdaw::ui::UiSnapMode::Relative && origin)
        {
            const auto delta = static_cast<yesdaw::engine::Tick> (static_cast<long long> (tick) - static_cast<long long> (*origin));
            const long long rounded = std::llround (static_cast<double> (delta) / static_cast<double> (gridTicks)) * gridTicks;
            return std::max<yesdaw::engine::Tick> (0, static_cast<yesdaw::engine::Tick> (static_cast<long long> (*origin) + rounded));
        }
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

    // G2.11: the slip — the dragged distance, snapped to the effective grid when snap is on (Ctrl is
    // part of the gesture, so it cannot defeat snap here; Snap: Off does), moves the source.
    void slipTimelineClipByLayoutId (int layoutClipId, double deltaSeconds)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;
        const yesdaw::engine::Project& project = appModel.project();
        if (! project.sampleRate.isValid())
            return;
        auto deltaTicks = static_cast<yesdaw::engine::Tick> (std::llround (deltaSeconds * project.sampleRate.hz));
        const bool snapOn = appModel.context().snapEnabled && appModel.context().snapMode != yesdaw::ui::UiSnapMode::Off;
        const std::int64_t grid = effectiveSnapGridTicks();
        if (snapOn && grid > 0)
            deltaTicks = static_cast<yesdaw::engine::Tick> (std::llround (static_cast<double> (deltaTicks) / static_cast<double> (grid)) * grid);
        if (deltaTicks == 0)
            return;

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        (void) appModel.slipSelectedTimelineClipBy (deltaTicks);
        refreshActionState();
        repaintAll();
    }

    // G2.9b: the Alt-drag on the right edge lands a NEW END; the model turns it into the factor.
    void stretchTimelineClipRightByLayoutId (int layoutClipId, double endSeconds, bool snapInvert = false)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
        if (const auto tick = timelineTickFromSeconds (endSeconds))
            (void) appModel.stretchSelectedTimelineClipTo (snappedTimelineTick (*tick, snapInvert));

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

    void adjustTimelineClipFadeByLayoutId (int layoutClipId, bool fadeIn, double fadeSeconds, double curveDelta = 0.0)
    {
        if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        const yesdaw::engine::EntityId clipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
        const yesdaw::engine::Clip* const clip = findProjectClipById (clipId);
        if (clip == nullptr)
            return;

        // G2.10: a negative fadeSeconds is the sentinel for "keep the current fade" (a pure bend).
        const std::optional<yesdaw::engine::Tick> fadeTicks = fadeSeconds < 0.0
            ? std::optional<yesdaw::engine::Tick> (fadeIn ? clip->fadeIn : clip->fadeOut)
            : timelineTickFromSeconds (fadeSeconds);
        if (! fadeTicks)
            return;

        const yesdaw::engine::Tick clampedFade =
            std::clamp<yesdaw::engine::Tick> (*fadeTicks, 0, std::max<yesdaw::engine::Tick> (0, clip->timelineLength));
        const yesdaw::engine::Tick nextFadeIn = fadeIn ? clampedFade : clip->fadeIn;
        const yesdaw::engine::Tick nextFadeOut = fadeIn ? clip->fadeOut : clampedFade;
        const bool bends = std::fabs (curveDelta) > 0.0;
        if (nextFadeIn == clip->fadeIn && nextFadeOut == clip->fadeOut && ! bends)
            return;

        (void) appModel.selectTimelineClip (clipId);
        if (bends)   // G2.10: length + curve bend as one undo step
            (void) appModel.adjustSelectedTimelineClipFade (nextFadeIn, nextFadeOut, fadeIn, static_cast<float> (curveDelta));
        else
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
            // G3.8: scale assist — a grid row inside the project's scale lifts off the black grid.
            if (surface.scaleChoice != yesdaw::engine::ProjectScale::kScaleOff
                && yesdaw::ui::pianoRollKeyInScale (key, surface.scaleRoot, surface.scaleChoice))
            {
                g.setColour (yesdaw::ui::UiTheme::Color::pianoRollInScaleRow());
                g.fillRect (juce::Rectangle<int> (geometry.grid.getX(), y, geometry.grid.getWidth(), keyRow.getHeight()));
            }
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

            // G3.9: drum mode — a Sampler's pad names its key (bold, over the key body); a key
            // without a pad keeps the note name, dimmed.
            const std::string* const padName = surface.drumMode ? surface.padNameForKey (key) : nullptr;
            if (padName != nullptr)
            {
                g.setColour (yesdaw::ui::UiTheme::Color::pianoWhiteKeyText());
                g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::caption, juce::Font::bold));
                g.drawText (juce::String (*padName),
                            keyRow.reduced (yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetX,
                                            yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetY),
                            juce::Justification::centredLeft, true);
            }
            // G3.2: every white key names itself once its row is tall enough; C keeps its bold octave
            // label at any height (the landmark Logic paints).
            else if (key % 12 == 0
                || (! isBlackMidiKey (key)
                    && juce::roundToInt (geometry.rowHeight) >= yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelMinRowHeight))
            {
                g.setColour (surface.drumMode ? yesdaw::ui::UiTheme::Color::pianoWhiteKeyText().withAlpha (0.5f)
                                              : yesdaw::ui::UiTheme::Color::pianoWhiteKeyText());
                g.setFont (yesdaw::ui::UiTheme::Type::font (
                    yesdaw::ui::UiTheme::Type::caption,
                    key % 12 == 0 ? juce::Font::bold : juce::Font::plain));
                g.drawText (juce::String (yesdaw::ui::pianoRollKeyName (key)),
                            keyRow.reduced (yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetX,
                                            yesdaw::ui::UiTheme::Layout::pianoRollKeyLabelInsetY),
                            juce::Justification::centredLeft, false);
            }
        }

        // G3.2: the grid follows the meter and the snap (plan §3.2): bar lines strong, beat lines
        // weak, snap subdivisions fainter and only while their cells are wide enough to read.
        for (const yesdaw::ui::PianoRollGridLine& line : pianoRollGridLines (geometry, surface))
        {
            g.setColour (line.kind == yesdaw::ui::PianoRollGridLineKind::Bar ? yesdaw::ui::UiTheme::Color::pianoGridStrong()
                         : line.kind == yesdaw::ui::PianoRollGridLineKind::Beat ? yesdaw::ui::UiTheme::Color::pianoGridWeak()
                         : yesdaw::ui::UiTheme::Color::pianoGridWeak().withAlpha (0.45f));
            g.fillRect (line.x,
                        geometry.grid.getY(),
                        yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth,
                        geometry.grid.getHeight());
        }

        // G3.2: the shared playhead, clip-relative.
        if (surface.playheadTick >= 0 && surface.playheadTick <= surface.timelineLength)
        {
            const int x = pianoRollTickX (geometry, surface, surface.playheadTick);
            if (x >= geometry.grid.getX() && x <= geometry.grid.getRight())
            {
                g.setColour (kText);
                g.fillRect (x, geometry.grid.getY(), yesdaw::ui::UiTheme::Layout::pianoRollPlayheadWidth, geometry.grid.getHeight());
            }
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
            // G3.3: the control lane's name is its chooser (a child in the gutter); the velocity lane keeps its label.
            if (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity)
                drawSmallLabel (g,
                                "Velocity",
                                laneArea.reduced (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLabelInsetX,
                                                  yesdaw::ui::UiTheme::Layout::pianoRollExpressionLabelInsetY));

            const double minValue = lane.valueMin;
            const double maxValue = lane.valueMax;

            if (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Control)
            {
                // The lane's data area (the grid's x span) and, for a bend, its centre line at 0.
                const juce::Rectangle<int> data = pianoRollControlLaneDataArea (geometry);
                g.setColour (yesdaw::ui::UiTheme::Color::controlInsetBlack());
                g.fillRect (data);
                if (minValue < 0.0)
                {
                    const int centreY = pianoRollControlLaneYForValue (data, 0.0, minValue, maxValue);
                    g.setColour (kPanelStroke.withAlpha (0.72f));
                    g.fillRect (juce::Rectangle<int> (data.getX(), centreY, data.getWidth(),
                                                     yesdaw::ui::UiTheme::Layout::pianoRollGridLineWidth));
                }
            }

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
        rowText.push_back ("Instrument");   // G3.1: the chooser + Edit ride this row's right
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

    // G3.4: the MIDI clip's inspector card — the title and the quantize panel's row labels with
    // the values in force (the controls themselves are children placed by layoutInspectorControls).
    void drawMidiClipInspector (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        g.setColour (kCyan);
        g.fillRoundedRectangle (static_cast<float> (area.getX()),
                                static_cast<float> (area.getY() + yesdaw::ui::UiTheme::Layout::inspectorTitleAccentTopInset),
                                static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                                static_cast<float> (yesdaw::ui::UiTheme::Layout::inspectorTitleAccentSize),
                                yesdaw::ui::UiTheme::Radius::sm);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::title, juce::Font::bold));
        g.drawText ("MIDI Clip",
                    area.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::inspectorTitleTextLeftInset)
                        .withHeight (yesdaw::ui::UiTheme::Layout::inspectorTitleTextHeight),
                    juce::Justification::centredLeft, false);

        const auto& context = appModel.context();
        const yesdaw::engine::MidiClip* const midiClip = appModel.selectedMidiClip();
        const int transpose = midiClip != nullptr ? static_cast<int> (midiClip->transposeSemitones) : 0;
        const int velocity = midiClip != nullptr ? juce::roundToInt (midiClip->velocityOffset * 100.0) : 0;
        const std::array<juce::Rectangle<int>, kMidiClipInspectorRows> rows = midiClipInspectorRows (area);
        const std::array<juce::String, kMidiClipInspectorRows> labels {
            juce::String (midiClip != nullptr && midiClip->muted ? "Mute (muted)" : "Mute"),
            "Transpose " + juce::String (transpose > 0 ? "+" : "") + juce::String (transpose) + " st",
            "Velocity " + juce::String (velocity > 0 ? "+" : "") + juce::String (velocity) + " %",
            juce::String ("Loop"),
            juce::String ("Quantize grid"),
            "Strength " + juce::String (context.quantizeStrengthPercent) + " %",
            "Swing " + juce::String (context.quantizeSwingPercent) + " %",
            juce::String ("Note ends"),
            "Humanize " + juce::String (context.quantizeHumanizePercent) + " %",
            juce::String ("Q applies to the selected notes") };
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            if (! area.contains (rows[i]))
                break;
            drawSmallLabel (g, labels[i],
                            rows[i].withTrimmedRight (i == rows.size() - 1u ? yesdaw::ui::UiTheme::Layout::inspectorQuantizeApplyWidth
                                                                            : yesdaw::ui::UiTheme::Layout::inspectorQuantizeControlWidth),
                            juce::Justification::centredLeft);
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
            if (inspectorShowsQuantizePanel())
            {
                drawMidiClipInspector (g, area);
                return;
            }
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
            const auto statsCells = yesdaw::ui::UiTheme::Layout::inspectorStatsCells (stats);   // G3.1 rubric FIX 1
            std::size_t statIndex = 0;
            for (const auto& [label, value] : statsText)
            {
                auto cell = statsCells[statIndex++]
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
                                       juce::String ("Curve") })   // G2.10: the overlaid chooser + amount slider are the values
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
                    fadeOutRegion,
                    static_cast<int> (selectedClip->fadeInShape), selectedClip->fadeInCurve,
                    static_cast<int> (selectedClip->fadeOutShape), selectedClip->fadeOutCurve);
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
        {
            auto markersCard = area.withTrimmedTop (yesdaw::ui::UiTheme::Layout::inspectorMarkersSectionTop)
                                   .withHeight (yesdaw::ui::UiTheme::Layout::inspectorMarkersSectionHeight);
            if (drawInspectorSectionCard (markersCard))   // G2.14
                drawSmallLabel (g, "MARKERS", markersCard.removeFromTop (yesdaw::ui::UiTheme::Layout::inspectorSectionLabelHeight));
        }
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

        // G4.1 cp2: no "MIXER" column — the strips are the mixer (plan §8.2: delete before you add).

        for (std::size_t stripIndex = 0; stripIndex < stripCount; ++stripIndex)
        {
            const bool isBus = stripIndex >= surface.tracks.size();
            const int ioRows = stripIoRows (stripIndex);   // G4.1
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
            // G4.1 cp2: EVERY strip paints its pan knob and fader rail — the selected strip used to leave
            // those to the lane's live pan / fader, which are gone (the painted drags are the controls).

            // G4.1: the lane from the ONE geometry law (paintedMixerLaneBounds) — the paint, the
            // hit-tests and the harness can never disagree on where a strip is (narrow or wide).
            auto lane = paintedMixerLaneBounds (stripIndex);
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

            // (every strip, the selected one included — G4.1 cp2)
            {
                const juce::Rectangle<int> panDisc = paintedPanKnobForLane (lane);
                const int panDiameter = panDisc.getWidth();
                const int panX = panDisc.getX();
                const int panY = panDisc.getY();
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
                // G4.1: S / M / R on a Track strip (the R cell lit while the track is in the arm set),
                // S / M on a Bus — the cell count is the strip kind's.
                const std::array<const char*, kMixerPaintedTrackCellCount> cellLabels { "S", "M", "R" };
                const std::size_t cellCount = stripCellCount (stripIndex);
                for (std::size_t cellIndex = 0; cellIndex < cellCount && cellIndex < cellLabels.size(); ++cellIndex)
                {
                    const auto cell = paintedMuteSoloCellBoundsForLane (lane, cellIndex, cellCount);
                    if (cell.isEmpty())
                        continue;

                    const bool armCell = cellIndex == kMixerPaintedTrackCellCount - 1;
                    const bool on = cellIndex == 0 ? state.soloed
                                  : cellIndex == 1 ? state.muted
                                                   : (! isBus && appModel.isRecordingTrackIndexArmed (stripIndex));
                    g.setColour (armCell && on ? yesdaw::ui::UiTheme::Color::recordArm()
                                               : yesdaw::ui::UiTheme::Color::controlInset());
                    g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::md);
                    g.setColour (on && ! armCell ? stripColour.brighter (0.55f) : kText);
                    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small, juce::Font::bold));
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
                    const auto row = paintedInsertRowBoundsForLane (lane, slot, ioRows);
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
                     sendIndex < static_cast<std::size_t> (paintedSendRowCountForLane (lane, ioRows));
                     ++sendIndex)
                {
                    const auto row = paintedSendRowBoundsForLane (lane, sendIndex, ioRows);
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

            // G4.1: the I/O slots — the input row leads the slot column (Track strips: the input the
            // track records from, "—" until one is picked), the output row closes it (Master or a bus).
            // Both are wells like the slot rows; a click opens the slot's choices.
            {
                const auto drawIoSlot = [&g] (juce::Rectangle<int> row, const juce::String& text, bool picked)
                {
                    g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                    g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                    g.setColour (kPanelStroke);
                    g.drawRoundedRectangle (row.toFloat().reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                            yesdaw::ui::UiTheme::Radius::sm,
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);
                    g.setColour (picked ? kText : kMutedText);
                    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
                    g.drawFittedText (text,
                                      row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedIoLabelInsetX),
                                      juce::Justification::centredLeft, 1);
                };
                const auto input = paintedInputRowBoundsForLane (lane, ioRows);
                if (! input.isEmpty())
                    drawIoSlot (input, stripInputText (stripIndex), ! isBus && appModel.isRecordingTrackIndexArmed (stripIndex));
                const auto output = paintedOutputRowBoundsForLane (lane, ioRows);
                if (! output.isEmpty())
                    drawIoSlot (output, stripOutputText (state), state.outputBusId.isValid());
            }

            // M6: the rail and the meter each derive from the shared lane laws now — there is no
            // local fader rect left to keep in sync.
            const auto meter = paintedMeterBoundsForLane (lane, ioRows);
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

            auto rail = paintedFaderRailForLane (lane, ioRows);
            // (every strip, the selected one included — G4.1 cp2)
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
            // G4.1: fitted, so a narrow strip's readout shrinks instead of clipping.
            g.drawFittedText (state.linearGain <= yesdaw::ui::UiTheme::Mixer::paintedReadoutGainFloor
                                  ? juce::String ("-inf dB")
                                  : juce::String (gainDb, 1) + " dB",
                              readout,
                              juce::Justification::centred,
                              1);
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

        // G4.1 rubric FIX: the master's card texts are FITTED — a narrow master pane shrinks them
        // instead of clipping "INTEGRATED" to "INTEGRA".
        auto loudnessCard = masterContent.removeFromTop (
            yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessCardHeight);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (loudnessCard.toFloat(), yesdaw::ui::UiTheme::Radius::md);
        g.setColour (kMutedText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::tiny,
            juce::Font::bold));
        g.drawFittedText ("INTEGRATED", loudnessCard.withHeight (
                        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop),
                    juce::Justification::centred,
                    1);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::readout,
            juce::Font::bold));
        const juce::String integrated = surface.loudness.valid
            ? juce::String (surface.loudness.integratedLufs, 1)
            : juce::String ("--");
        g.drawFittedText (integrated,
                    loudnessCard.withTrimmedTop (
                        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop)
                        .withHeight (
                            yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueHeight),
                    juce::Justification::centred,
                    1);
        g.setColour (kMutedText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
        g.drawFittedText ("LUFS-I",
                    loudnessCard.withTrimmedTop (
                        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop
                        + yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueHeight)
                        .withHeight (
                            yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessUnitHeight),
                    juce::Justification::centred,
                    1);

        masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterSectionGap);
        auto peakCard = masterContent.removeFromTop (
            yesdaw::ui::UiTheme::Layout::mixerMasterPeakCardHeight);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (peakCard.toFloat(), yesdaw::ui::UiTheme::Radius::md);
        g.setColour (kMutedText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::tiny,
            juce::Font::bold));
        g.drawFittedText ("TRUE PEAK",
                    peakCard.withHeight (
                        yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueTop),
                    juce::Justification::centred,
                    1);
        g.setColour (surface.loudness.valid && surface.loudness.truePeakDbtp > 0.0
                         ? yesdaw::ui::UiTheme::Color::dangerRed()
                         : kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::body,
            juce::Font::bold));
        const juce::String truePeak = surface.loudness.valid
            ? juce::String (surface.loudness.truePeakDbtp, 1) + " dBTP"
            : juce::String ("-- dBTP");
        g.drawFittedText (truePeak,
                    peakCard.withTrimmedTop (
                        yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueTop)
                        .withHeight (
                            yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueHeight),
                    juce::Justification::centred,
                    1);

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
                appModel.selectedMidiNoteIds(),
                appModel.context().pianoRollControlLaneChoice);   // G3.3

            // Piano-roll viewport (E10): the surface publishes the CLAMPED view so every paint,
            // hit-test, and gesture consumer shares one law.
            // G3.2 FIX 3: the key window follows the roll's grid height (one law with the geometry).
            surface.viewKeyCount = pianoRollCanvasGeometry (pianoRollInput.getBounds().withZeroOrigin()).visibleKeys;
            pianoRollViewLowKey = std::clamp (
                pianoRollViewLowKey,
                yesdaw::ui::UiThemeLayout::pianoRollKeyMin,
                yesdaw::ui::UiThemeLayout::pianoRollKeyMax - (surface.viewKeyCount - 1));
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
            // G3.2: the grid follows the meter in force at the clip (bars / beats) and the snap; the
            // shared playhead is published clip-relative (-1 when it has no tick).
            {
                // G3.6 (found by the step gate): the grid and the playhead speak the CLIP's ticks — every
                // shell-made MIDI clip is SampleLocked (tick == frame), so a beat is the head tempo's
                // beat in frames and the playhead is the transport frame; G3.2 had used the tempo map's
                // musical ticks for both, which ran 0.64x slow on a 120 BPM / 48 kHz clip.
                const yesdaw::engine::MidiClip* clip = nullptr;
                for (const yesdaw::engine::MidiClip& candidate : appModel.project().midiClips)
                    if (candidate.id == midiClipId)
                        clip = &candidate;
                if (clip != nullptr)
                {
                    surface.beatTicks = appModel.midiClipBeatTicks (*clip);
                    surface.barTicks = appModel.midiClipBarTicks (*clip);
                    if (const std::optional<yesdaw::engine::Tick> playhead = appModel.playheadTickForClip (*clip))
                        surface.playheadTick = *playhead - surface.timelineStart;
                }
            }
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
    std::vector<yesdaw::engine::EntityId> timelineClipAssetIds;   // G2.19: the decoded audio for the zoomed-in paint
    double timelineTotalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
    std::vector<std::string> timelineMarkerLabels;
    std::vector<yesdaw::ui::TimelineMarker> timelineMarkerViews;
    std::vector<std::string> timelineMapLabelTexts;                // G2.15
    std::vector<yesdaw::ui::TimelineMapLabel> timelineMapLabelViews;
    std::vector<std::int64_t> timelineMapLabelFrames;   // the change's exact frame per label (a click locates there)
    double timelineZoomFactor = 1.0;   // 1.0 == whole timeline fits the window
    double timelineRowZoom = 1.0;      // G2.16: multiplies every auto-height row
    std::optional<ZoomView> lastSelectionZoom;   // G2.16: the view Z produced (Z again goes back)
    bool refreshingZoomSlider = false;
    bool refreshingScrollBars = false;
    FineDragSlider timelineZoomSlider;
    // G2.16: a scroll bar that carries a tooltip like every other identified control.
    struct TooltipScrollBar final : juce::ScrollBar, juce::SettableTooltipClient
    {
        using juce::ScrollBar::ScrollBar;
    };
    TooltipScrollBar timelineHScroll { false };
    TooltipScrollBar timelineVScroll { true };
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
    juce::ComboBox pianoRollLaneChooser;   // G3.3: the control lane's chooser (a child over the lane's gutter)
    juce::ComboBox pianoRollKeyChooser;    // G3.8: the roll header's Key chooser (the project's key)
    juce::ComboBox pianoRollScaleChooser;  // G3.8: the roll header's Scale chooser (Off / Major / Minor)
    juce::TextButton pianoRollTypingButton;   // G3.6: the roll header's Typing toggle (Ctrl+K)
    juce::TextButton pianoRollStepButton;     // G3.6: the roll header's Step toggle
    std::map<int, std::int16_t> typedKeyCodes;   // G3.6: key code -> the note it holds
    // G3.5: the MIDI clip's settings rows (the inspector's CLIP tab)
    juce::ToggleButton inspectorMidiMute;
    FineDragSlider inspectorMidiTranspose;
    FineDragSlider inspectorMidiVelocity;
    juce::ComboBox inspectorMidiLoop;
    // G3.4: the quantize panel's controls (the inspector's CLIP tab for a MIDI clip)
    juce::ComboBox inspectorQuantizeGrid;
    FineDragSlider inspectorQuantizeStrength;
    FineDragSlider inspectorQuantizeSwing;
    juce::ToggleButton inspectorQuantizeEnds;
    FineDragSlider inspectorQuantizeHumanize;
    juce::TextButton inspectorQuantizeApply;
    TrackListInputComponent trackListInput;
    MixerStripsInputComponent mixerStripsInput;
    FineDragSlider headerTempoControl;
    juce::ComboBox headerMeterChooser;
    // G4.1 cp2: the FX editor — the lane's parameter widgets live inside it now (their ids unchanged);
    // it shows the SELECTED strip's selectedFxParamSlot while open and closes when that strip or slot goes.
    FxEditorComponent fxEditor;
    bool fxEditorOpen = false;
    int fxEditorStripOrdinal = -1;
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
    // E17: the inline bus rename editor (the strip header's double-click).
    juce::TextEditor busRenameEditor;
    int busRenameIndex = -1;

    // M5: transient painted-send drag preview (strip, send, level). Nothing persists until the
    // release commits, so a drag is exactly one undo step.
    struct PaintedSendDragPreview
    {
        int stripIndex = -1;
        int sendIndex = -1;
        float level = 0.0f;
    };
    PaintedSendDragPreview paintedSendDragPreview;
    int paintedFaderDragStrip = -1;   // G4.1 cp2: the strip whose painted fader / pan is mid-drag (the press begins the ride)
    int paintedPanDragStrip = -1;
    std::size_t lastVisibleFxParamRows = 0;
    juce::TextButton trackAddButton;
    juce::TextEditor trackRenameEditor;
    juce::TextEditor clipRenameEditor;
    juce::TextEditor markerRenameEditor;
    int markerRenameIndex = -1;
    int selectedTrackLane = -1;
    std::set<int> selectedTrackLanes;   // G2.17: the multi-selection (empty = just the primary)
    juce::TextButton exportAudioButton;
    juce::ComboBox exportBitDepthChooser;
    juce::ComboBox exportRangeChooser;
    juce::Label exportAudioProgress;
    juce::TextButton exportAudioCancelButton;
    juce::Label dragDbReadout;
    std::vector<MeterHoldState> trackMeterHold;   // by Track index; advanced per UI tick (B32)
    std::vector<std::array<MeterHoldState, 2>> trackMeterHoldLR;   // V5: rail L/R columns
    std::vector<MeterHoldState> busMeterHold;     // by Bus index; same tick law (E22)
    juce::String lastPushedWindowTitle;           // dirty-title push dedupe (B38)
    // (G4.1: the seven readout buttons and the solo-safe button are gone — the strip and its menu.
    //  G4.1 cp2: the lane's live fader / pan / M / S went with the lane — the painted strip is the mixer.)
    juce::TextButton masterLoudnessReadout;
    juce::TextButton autosaveRestoreButton;
    juce::TextButton autosaveDiscardButton;
    juce::ComboBox timelineSnapChooser;
    juce::ComboBox nudgeValueChooser;      // G1.4
    juce::String hoverHint;                // G1.6: the status line's gesture hint
    std::vector<std::pair<juce::Component*, yesdaw::ui::UiActionId>> actionComponents;   // G1.6: live tooltips
    KeymapEditorComponent keymapEditor;    // G1.5
    UndoHistoryComponent undoHistory;      // G2.18
    InstrumentPanelComponent instrumentPanel;   // G3.1
    juce::ComboBox inspectorInstrumentChooser;   // G3.1
    juce::TextButton inspectorInstrumentEdit;
    juce::TextButton inspectorToggle;      // G1.4
    juce::ComboBox editModeChooser;        // G2.6
    bool refreshingEditModeChooser = false;
    juce::ComboBox snapModeChooser;        // G2.7
    bool refreshingSnapModeChooser = false;
    // G2.1: the Arrange window's splitter sizes (persisted per project as view-state.txt).
    struct ViewState
    {
        int railWidth = yesdaw::ui::UiTheme::Layout::leftRailWidth;
        int inspectorWidth = yesdaw::ui::UiTheme::Layout::inspectorWidth;
        int dockHeight = yesdaw::ui::UiTheme::Layout::mixerHeight;
    };
    ViewState viewState;
    std::filesystem::path viewStateBundle;
    std::map<juce::Component*, bool> hiddenByDockTab;   // G2.1 cp2
    yesdaw::ui::SplitterComponent railSplitter { yesdaw::ui::SplitterComponent::Axis::Vertical };
    yesdaw::ui::SplitterComponent inspectorSplitter { yesdaw::ui::SplitterComponent::Axis::Vertical };
    yesdaw::ui::SplitterComponent dockSplitter { yesdaw::ui::SplitterComponent::Axis::Horizontal };
    int timeDisplayMode = 0;               // G1.4: 0 bars|beats primary, 1 min:sec primary
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
    // G2.14: the inspector's marker list — every marker in tick order; a click locates the playhead.
    struct MarkerListModel final : public juce::ListBoxModel
    {
        std::function<int()> rowCount;
        std::function<juce::String (int)> rowText;
        std::function<void (int)> onRowClicked;
        int getNumRows() override { return rowCount ? rowCount() : 0; }
        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
        {
            if (selected)
            {
                g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                g.fillRect (0, 0, width, height);
            }
            g.setColour (yesdaw::ui::UiTheme::Color::text());
            g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
            g.drawText (rowText ? rowText (row) : juce::String(), yesdaw::ui::UiTheme::Space::sm, 0,
                        width - 2 * yesdaw::ui::UiTheme::Space::sm, height, juce::Justification::centredLeft, true);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (onRowClicked)
                onRowClicked (row);
        }
    };
    MarkerListModel inspectorMarkerListModel;
    juce::ListBox inspectorMarkerList;
    juce::TextButton inspectorTakeDelete;
    std::vector<yesdaw::ui::UiClipTakeView> inspectorTakeViews;
    // E34: open MIDI inputs + the message-thread note-on pairing map (note -> frame, velocity).
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
    std::uint32_t midiInSeenLast = 0;                                   // G3.10: the lamp's last seen count
    std::chrono::steady_clock::time_point midiInLitUntil {};            // G3.10: lit until this instant
    std::map<int, std::pair<std::int64_t, float>> pendingMidiNoteOns;
    FineDragSlider inspectorStart;
    FineDragSlider inspectorEnd;
    FineDragSlider inspectorLength;
    FineDragSlider inspectorGain;
    FineDragSlider inspectorStretch;   // G2.9b: percent of the source length
    FineDragSlider inspectorFadeIn;
    FineDragSlider inspectorFadeOut;
    juce::ComboBox inspectorFadeCurve;
    FineDragSlider inspectorFadeCurveAmount;   // G2.10
    std::array<ToolbarActionButton, yesdaw::ui::kMainShellToolbarActions.size()> buttons;
    bool refreshingInspectorControls = false;
    bool refreshingTimeMapControls = false;
    bool refreshingSnapChooser = false;
    bool refreshingNudgeChooser = false;   // G1.4
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

    // G3.7: the MIDI file choosers.
    choices.chooseImportMidiFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Import MIDI File", documents, "*.mid;*.midi", true);
        if (! chooser.browseForFileToOpen())
            return std::filesystem::path {};

        return pathFromJuceFile (chooser.getResult());
    };

    // G3.9: the Sampler pad's WAV chooser.
    choices.chooseSamplerPadFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Load Sample onto Pad", documents, "*.wav;*.wave", true);
        if (! chooser.browseForFileToOpen())
            return std::filesystem::path {};

        return pathFromJuceFile (chooser.getResult());
    };

    choices.chooseExportMidiFile = [] {
        const juce::File documents = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        juce::FileChooser chooser ("Export MIDI File", documents.getChildFile ("YES DAW.mid"), "*.mid", true);
        if (! chooser.browseForFileToSave (true))
            return std::filesystem::path {};

        return withExtension (pathFromJuceFile (chooser.getResult()), ".mid");
    };

    return choices;
}

namespace yesdaw::ui {

std::unique_ptr<juce::Component> createMainComponent()
{
    return createNativeMainComponent ({});
}

std::unique_ptr<juce::Component> createNativeMainComponent (std::filesystem::path openBundleAtLaunch,
                                                            std::filesystem::path sessionStateDirectory)
{
    yesdaw::ui::MainComponentFileChoices choices = makeNativeFileChoices();
    choices.openBundleAtLaunch = std::move (openBundleAtLaunch);
    if (! sessionStateDirectory.empty())
    {
        choices.sessionStateDirectory = std::move (sessionStateDirectory);
        return std::make_unique<MainComponent> (std::move (choices), true);
    }

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

juce::Rectangle<int> mainComponentPaintedPanKnobBounds (const juce::Component& component, int stripIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedPanKnobBounds (stripIndex);
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
                                               h.stop, h.record, h.timeReadout, h.tempoMeterBox, h.loop,   // G3.10: the MIDI lamp sits INSIDE the time readout, not beside it
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

yesdaw::ui::MainComponentKeymapEditor mainComponentKeymapEditor (juce::Component& component)
{
    yesdaw::ui::MainComponentKeymapEditor out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        out.visible = mainComponent->harnessKeymapEditor().isVisible();
        out.rows = mainComponent->harnessKeymapEditor().currentRows();
        out.status = mainComponent->harnessKeymapEditor().statusText();
    }
    return out;
}

juce::String mainComponentHoverHintAt (juce::Component& component, juce::Point<int> shellPoint)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessHoverHintAt (shellPoint);
    return {};
}

void mainComponentKeymapEditorSearch (juce::Component& component, const juce::String& text)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessKeymapEditor().harnessSearch (text);
}

void mainComponentKeymapEditorSelectRow (juce::Component& component, int row)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessKeymapEditor().harnessSelectRow (row);
}

void mainComponentKeymapEditorBind (juce::Component& component, const juce::String& chord)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessKeymapEditor().harnessBind (chord);
}

yesdaw::ui::MainComponentContextMenu mainComponentRequestContextMenu (juce::Component& component, juce::Point<int> shellPoint)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRequestContextMenu (shellPoint);
    return {};
}

void mainComponentSetDockHeight (juce::Component& component, int height)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessSetDockHeight (height);
}

juce::String mainComponentTimelineZoneAt (juce::Component& component, juce::Point<int> shellPoint, juce::ModifierKeys modifiers)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessTimelineZoneAt (shellPoint, modifiers);
    return "none";
}

juce::String mainComponentTimelineCursorAt (juce::Component& component, juce::Point<int> shellPoint, juce::ModifierKeys modifiers)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessTimelineCursorAt (shellPoint, modifiers);
    return "normal";
}

double mainComponentTimelineAutoScrollTick (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessTimelineAutoScrollTick();
    return 0.0;
}

void mainComponentInvokeContextMenuId (juce::Component& component, int itemId)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInvokeContextMenuId (itemId);
}

int mainComponentTimeDisplayMenuId (int mode)
{
    return MainComponent::harnessTimeDisplayMenuId (mode);
}

// G4.1: the strip menus' routing choices, the last menu, the I/O rows, the slot texts, the view state.
int mainComponentMixerInputMenuId (int channel, bool stereoPair)
{
    return MainComponent::harnessMixerInputMenuId (channel, stereoPair);
}

int mainComponentMixerOutputMenuId (int choice)
{
    return MainComponent::harnessMixerOutputMenuId (choice);
}

int mainComponentMixerSendDestinationMenuId (int busIndex)
{
    return MainComponent::harnessMixerSendDestinationMenuId (busIndex);
}

MainComponentFxEditor mainComponentFxEditor (const juce::Component& component)
{
    if (const auto* shell = dynamic_cast<const MainComponent*> (&component))
        return shell->harnessFxEditor();
    return {};
}

void mainComponentOpenFxEditor (juce::Component& component, int stripIndex, int slotIndex)
{
    if (auto* shell = dynamic_cast<MainComponent*> (&component))
        shell->harnessOpenFxEditor (stripIndex, slotIndex);
}

int mainComponentMixerSendMenuId (int busIndex)
{
    return MainComponent::harnessMixerSendMenuId (busIndex);
}

MainComponentContextMenu mainComponentLastContextMenu (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessLastContextMenu();
    return {};
}

juce::Rectangle<int> mainComponentPaintedIoRowBounds (const juce::Component& component, int stripIndex, int row)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedIoRowBounds (stripIndex, row);
    return {};
}

MainComponentMixerStripIo mainComponentMixerStripIo (const juce::Component& component, int stripIndex)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessMixerStripIo (stripIndex);
    return {};
}

juce::String mainComponentViewStateRecord (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessViewStateRecord();
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

MainComponentUndoHistory mainComponentUndoHistory (juce::Component& component)
{
    MainComponentUndoHistory out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        out.visible = mainComponent->harnessUndoHistory().isVisible();
        mainComponent->harnessUndoHistory().refreshRows();
        out.rows = mainComponent->harnessUndoHistory().currentRows();
        out.current = mainComponent->harnessUndoHistory().currentRow();
    }
    return out;
}

MainComponentPianoRollGrid mainComponentPianoRollGrid (juce::Component& component)
{
    MainComponentPianoRollGrid out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = mainComponent->harnessPianoRollSurface();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (mainComponent->harnessPianoRollBounds().withZeroOrigin());
        for (const yesdaw::ui::PianoRollGridLine& line : pianoRollGridLines (geometry, surface))
            out.lines.emplace_back (static_cast<std::int64_t> (line.tick), line.x, static_cast<int> (line.kind));
        out.playheadTick = surface.playheadTick;
        out.viewScrollTicks = surface.viewScrollTicks;
        out.visibleTicks = pianoRollVisibleTicks (surface);
    }
    return out;
}

MainComponentPianoRollAudition mainComponentPianoRollAudition (juce::Component& component, int key)
{
    MainComponentPianoRollAudition out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = mainComponent->harnessPianoRollSurface();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (mainComponent->harnessPianoRollBounds().withZeroOrigin());
        out.heldKey = mainComponent->harnessPianoRollAuditionKey();
        out.keyboardX = geometry.keyboard.getCentreX();
        out.keyY = pianoRollKeyY (geometry, surface, key) + juce::roundToInt (geometry.rowHeight * 0.5f);
        out.gridLeft = geometry.grid.getX();
        out.gridRight = geometry.grid.getRight();
    }
    return out;
}

void mainComponentServiceUiTick (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessServiceUiTick();
}

MainComponentPianoRollControlLane mainComponentPianoRollControlLane (juce::Component& component)
{
    MainComponentPianoRollControlLane out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = mainComponent->harnessPianoRollSurface();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (mainComponent->harnessPianoRollBounds().withZeroOrigin());
        out.lane = pianoRollControlLaneDataArea (geometry);
        out.chooser = pianoRollControlLaneChooserArea (geometry);
        out.name = yesdaw::ui::pianoRollControlLaneChoice (surface.controlLaneChoice).name;
        if (const auto* lane = pianoRollControlLaneOf (surface))
        {
            out.valueMin = lane->valueMin;
            out.valueMax = lane->valueMax;
            for (const yesdaw::ui::UiPianoRollExpressionPoint& point : lane->points)
            {
                out.points.emplace_back (static_cast<std::int64_t> (point.tick), point.value);
                out.pointIds.push_back (juce::String (entityIdHex (point.entityId)));
            }
        }
    }
    return out;
}

void mainComponentReleaseTypedKeys (juce::Component& component)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessReleaseTypedKeysForTest();
}

void mainComponentSelectPianoRollScale (juce::Component& component, int rootKey, int scaleChoice)   // G3.8
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessSelectPianoRollScale (rootKey, scaleChoice);
}

void mainComponentSelectPianoRollControlLane (juce::Component& component, int choice)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessSelectPianoRollControlLane (choice);
}

double mainComponentPianoRollControlValueForY (juce::Component& component, int rollLocalY)
{
    const MainComponentPianoRollControlLane lane = mainComponentPianoRollControlLane (component);
    return pianoRollControlValueForLaneY (lane.lane, rollLocalY, lane.valueMin, lane.valueMax);
}

int mainComponentPianoRollControlYForValue (juce::Component& component, double value)
{
    const MainComponentPianoRollControlLane lane = mainComponentPianoRollControlLane (component);
    return pianoRollControlLaneYForValue (lane.lane, value, lane.valueMin, lane.valueMax);
}

bool mainComponentAuditionNote (juce::Component& component, std::int16_t key, bool on)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessAuditionNote (key, on);
    return false;
}

std::vector<float> mainComponentRenderPlaybackFrames (juce::Component& component, std::uint64_t frames, int blockSize)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessRenderPlayback (frames, blockSize);
    return {};
}

MainComponentInstrumentPanel mainComponentInstrumentPanel (juce::Component& component)
{
    MainComponentInstrumentPanel out;
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
    {
        InstrumentPanelComponent& panel = mainComponent->harnessInstrumentPanel();
        out.visible = panel.isVisible();
        panel.refresh();
        out.kind = panel.currentKind();
        for (const InstrumentPanelComponent::Row& row : panel.currentRows())
            out.rows.emplace_back (row.label, row.normalized);
        panel.resized();   // G3.9: the pad grid is laid out in resized(); the readout reports what is painted
        out.padsVisible = panel.padsShown();
        out.padGrid = panel.currentPadGrid();
        for (const InstrumentPanelComponent::Pad& pad : panel.currentPads())
            out.pads.emplace_back (pad.key, pad.name);
    }
    return out;
}

bool mainComponentPostMidiInput (juce::Component& component, bool on, int key, double velocity)   // G3.10
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        return mainComponent->harnessPostMidiInput (on, key, velocity);
    return false;
}

void mainComponentInstrumentPanelClickPad (juce::Component& component, int key, bool shift, bool ctrl)   // G3.9
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInstrumentPanel().harnessClickPad (key, shift, ctrl);
}

void mainComponentInstrumentPanelDropFileOnPad (juce::Component& component, int key, const juce::String& path)   // G3.9
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInstrumentPanel().harnessDropOnPad (key, juce::StringArray { path });
}

void mainComponentInstrumentPanelSetRow (juce::Component& component, int row, double normalized)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInstrumentPanel().harnessSetRow (row, normalized);
}

void mainComponentInstrumentPanelDragRow (juce::Component& component, int row, double first, double second)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessInstrumentPanel().harnessDragRow (row, { first, second });
}

void mainComponentUndoHistoryClickRow (juce::Component& component, int row)
{
    if (auto* mainComponent = dynamic_cast<MainComponent*> (&component))
        mainComponent->harnessUndoHistory().clickRow (row);
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

juce::Rectangle<int> mainComponentPaintedRailCellBounds (const juce::Component& component, int row, int cell)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessPaintedRailCellBounds (row, cell);
    return {};
}

juce::Rectangle<int> mainComponentPaintedHeaderGearBounds (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->headerLayout().gear;
    return {};
}

TimelineClipSourceWindow mainComponentTimelineClipSourceWindow (const juce::Component& component, int layoutClipId)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessTimelineClipSourceWindow (layoutClipId);
    return {};
}

double mainComponentTimelineZoomCeiling (const juce::Component& component)
{
    if (const auto* mainComponent = dynamic_cast<const MainComponent*> (&component))
        return mainComponent->harnessTimelineZoomCeiling();
    return 0.0;
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
