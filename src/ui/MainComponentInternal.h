// YES DAW — the shell's private helpers (colours, small painters, menu ids, the asset decode).
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): the anonymous namespace that used to
// open MainComponent.cpp, now shared by the shell's translation units (MainComponent*.cpp) and its
// declaration. Everything here is internal to the shell: namespace yesdaw::ui::shell, inline linkage.

#pragma once

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

namespace yesdaw::ui::shell {

inline constexpr int kHeaderHeight = yesdaw::ui::UiTheme::Layout::headerHeight;

// G0.7: the controls that live in the collapsible settings row under the toolbar.
[[nodiscard]] inline constexpr bool isSettingsRowAction (yesdaw::ui::UiActionId action) noexcept
{
    using yesdaw::ui::UiActionId;
    return action == UiActionId::RecordingArmTrack || action == UiActionId::RecordingSetMonitoringPolicy
        || action == UiActionId::RecordingAssembleComp;
}
inline constexpr int kUiRefreshIntervalMs = 33;
// Open Recent menu item ids live above the action-id range (B39).
inline constexpr int kRecentMenuBaseId = 1000;
inline constexpr int kRepeatCountMenuBaseId = 1100;   // G1.7: Edit ▸ Repeat Count ▸ (+ the count: 2, 3, 4, 8)

// The menu bar names its menus itself; the tooltip mixin satisfies the every-control law (B40).
class TooltippedMenuBar final : public juce::MenuBarComponent,
                                public juce::SettableTooltipClient
{
public:
    using juce::MenuBarComponent::MenuBarComponent;
};

inline constexpr const char* kTimelineComponentId = "timeline.canvas";
inline constexpr std::array<std::pair<std::uint16_t, std::uint16_t>, 6> kHeaderMeterChoices {{
    { 4, 4 }, { 3, 4 }, { 6, 8 }, { 2, 4 }, { 5, 4 }, { 7, 8 }
}};
inline constexpr const char* kPianoRollComponentId = "piano-roll.canvas";
inline constexpr const char* kInspectorStartComponentId = "clip.inspector.start";
inline constexpr const char* kInspectorEndComponentId = "clip.inspector.end";
inline constexpr const char* kInspectorLengthComponentId = "clip.inspector.length";
inline constexpr const char* kInspectorFadeInComponentId = "clip.inspector.fade_in";
inline constexpr const char* kInspectorFadeOutComponentId = "clip.inspector.fade_out";
inline constexpr const char* kInspectorFadeCurveComponentId = "clip.inspector.fade_curve";
inline constexpr const char* kInspectorStretchComponentId = "clip.inspector.stretch";   // G2.9b
inline constexpr const char* kInspectorFadeCurveAmountComponentId = "clip.inspector.fade_curve_amount";   // G2.10
inline constexpr int kInspectorLinearFadeCurveId = 2;    // G2.10: the chooser's ids, equal power stays 1
inline constexpr int kInspectorSCurveFadeCurveId = 3;
inline constexpr int kInspectorLogFadeCurveId = 4;
inline constexpr const char* kAutomationLaneRowComponentId = "timeline.automation.track.0.lane";
// N1: a mixer strip carries exactly two painted toggle cells — Solo then Mute, left to right.
inline constexpr std::size_t kMixerPaintedMuteSoloCellCount = 2;
// G4.1: the strip's cell row is S / M / R on a Track strip, S / M on a Bus; the I/O slot rows.
inline constexpr std::size_t kMixerPaintedTrackCellCount = 3;
inline constexpr const char* kExportAudioProgressComponentId = "project.export_audio.progress";
inline constexpr int kInspectorEqualPowerFadeCurveId = 1;

inline const juce::Colour kBackground = yesdaw::ui::UiTheme::Color::appBackground();
inline const juce::Colour kPanel = yesdaw::ui::UiTheme::Color::panel();
inline const juce::Colour kPanelRaised = yesdaw::ui::UiTheme::Color::panelRaised();
inline const juce::Colour kPanelStroke = yesdaw::ui::UiTheme::Color::panelStroke();
inline const juce::Colour kText = yesdaw::ui::UiTheme::Color::text();
inline const juce::Colour kMutedText = yesdaw::ui::UiTheme::Color::mutedText();
inline const juce::Colour kBlue = yesdaw::ui::UiTheme::Color::accentBlue();
inline const juce::Colour kTeal = yesdaw::ui::UiTheme::Color::accentTeal();
inline const juce::Colour kAmber = yesdaw::ui::UiTheme::Color::accentAmber();
inline const juce::Colour kPurple = yesdaw::ui::UiTheme::Color::accentPurple();
inline const juce::Colour kCyan = yesdaw::ui::UiTheme::Color::accentCyan();
inline const juce::Colour kRed = yesdaw::ui::UiTheme::Color::dangerRed();

using TrackRow = yesdaw::ui::TimelineCanvasTrack;
using TimelineClipStyle = yesdaw::ui::TimelineCanvasClipStyle;

inline constexpr bool isBlackMidiKey (int key) noexcept
{
    const int octaveKey = key % 12;
    return octaveKey == 1 || octaveKey == 3 || octaveKey == 6 || octaveKey == 8 || octaveKey == 10;
}

inline juce::String actionButtonText (yesdaw::ui::UiActionId id)
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

inline constexpr bool toolbarActionRequiresPlayback (yesdaw::ui::UiActionId id) noexcept
{
    return id == yesdaw::ui::UiActionId::TransportPlay
        || id == yesdaw::ui::UiActionId::TransportStop
        || id == yesdaw::ui::UiActionId::TransportLocateStart
        || id == yesdaw::ui::UiActionId::TransportToggleLoop;
}

inline void fillPanel (juce::Graphics& g,
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

inline void drawSmallLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                     juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour (kMutedText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
    g.drawText (text, area, justification, false);
}

inline void drawMeter (juce::Graphics& g, juce::Rectangle<int> area, float value)
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
inline void drawMeterWithHold (juce::Graphics& g,
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

inline void drawHorizontalMeter (juce::Graphics& g, juce::Rectangle<int> area, float value)
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

inline juce::Colour stripColourForIndex (std::size_t index)
{
    const std::array colours { kBlue, kTeal, kAmber, kPurple, kCyan };
    return colours[index % colours.size()];
}

// N7: the fixed swatch palette a rail-row colour click cycles through. Position 0 is "no
// override" (kTrackColourUnset); positions 1..5 mirror the SAME five accents
// stripColourForIndex already draws from (kBlue/kTeal/kAmber/kPurple/kCyan), so a customized
// track colour always looks native to this theme instead of introducing a new arbitrary hue.
// Written as raw hex (not the juce::Colour constants above) so the array can be constexpr.
inline constexpr std::array<std::uint32_t, 6> kTrackColourCycle {
    yesdaw::engine::kTrackColourUnset,
    0xff3b8cffu,   // accentBlue
    0xff1bb5a6u,   // accentTeal
    0xffd29118u,   // accentAmber
    0xffa578ffu,   // accentPurple
    0xff20c8d8u,   // accentCyan
};

[[nodiscard]] inline std::uint32_t nextTrackColourInCycle (std::uint32_t current) noexcept
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
[[nodiscard]] inline juce::Colour colourForTrack (const yesdaw::engine::Track& track, juce::Colour fallbackColour) noexcept
{
    return track.colour == yesdaw::engine::kTrackColourUnset ? fallbackColour : juce::Colour (track.colour);
}

// Translate a JUCE KeyPress into the keymap's chord vocabulary ("Ctrl+Alt+Shift+B", "Space", "Del",
// "F2", "Ctrl+/"). Modifier order matches the descriptor table: Ctrl, Alt, Shift.
// G0.1 State probe: the shell's JSON schema version. Bumped only when a field changes meaning;
// the [state-probe] gate and tools/session-drive.ps1 pin it.
inline constexpr int kStateProbeSchemaVersion = 1;
// G0.1: paint-time ring used for the p95 the B2 feel budget reads (about eight seconds at 30 Hz).
inline constexpr std::size_t kStateProbePaintRingSize = 256;

// G0.1: an EntityId as 32 lowercase hex digits — the id form the State probe publishes and the
// Session drive clicks by (`clip.<hex>`).
inline std::string entityIdHex (const yesdaw::engine::EntityId& id)
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

inline std::string chordForKeyPress (const juce::KeyPress& key)
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

inline juce::File juceFileFromPath (const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return juce::File { juce::String::fromUTF8 (
        reinterpret_cast<const char*> (utf8.data()),
        static_cast<int> (utf8.size())) };
}

// Decode a mono or stereo WAV into an interleaved UiDecodedAsset (ADR-0042). Wider-than-stereo files
// are rejected — never silently downmixed.
inline std::optional<yesdaw::ui::UiDecodedAsset> decodeProjectWav (const std::filesystem::path& sourcePath)
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
    yesdaw::ui::UiPreparedProjectBundle prepared;
    std::optional<std::vector<yesdaw::ui::UiDecodedAsset>> assets;
    std::string failureReason;
};

inline StoredProjectAssetsResult decodeStoredProjectAssets (const std::filesystem::path& bundlePath)
{
    StoredProjectAssetsResult out;

    const auto opened = yesdaw::ui::UiPreparedProjectBundle::open (bundlePath, out.prepared);
    if (! opened.ok())
    {
        // The bundle layer's own message is the most precise fact available — e.g.
        // "committed asset bytes are missing: <path>" from the open-time integrity check.
        out.failureReason = opened.bundleResult.message.empty()
            ? (opened.status == yesdaw::ui::UiAppLoadStatus::ProjectReadFailed
                ? "the project data is invalid or corrupt" : "the project file could not be opened")
            : opened.bundleResult.message;
        return out;
    }

    const auto& project = out.prepared.project();
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

} // namespace yesdaw::ui::shell
