// YES DAW - H11 app shell.
//
// The visible JUCE shell and the headless tests share ui/UiActions.h. This checkpoint keeps the shell
// image-light and model-backed: later H11 slices wire Project loading, transport, timeline drawing,
// accessibility traversal, and editing through the same action IDs.

#include "ui/TimelineCanvas.h"
#include "ui/UiActions.h"
#include "ui/UiMixerSurface.h"
#include "ui/UiPianoRollSurface.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace {

constexpr int kHeaderHeight = 88;
constexpr int kLeftRailWidth = 318;
constexpr int kInspectorWidth = 248;
constexpr int kMixerHeight = 260;

const juce::Colour kBackground (0xff080c11);
const juce::Colour kPanel (0xff11161c);
const juce::Colour kPanelRaised (0xff151b22);
const juce::Colour kPanelStroke (0xff2a323b);
const juce::Colour kText (0xfff0f3f6);
const juce::Colour kMutedText (0xff9aa3ad);
const juce::Colour kBlue (0xff3b8cff);
const juce::Colour kTeal (0xff1bb5a6);
const juce::Colour kAmber (0xffd29118);
const juce::Colour kPurple (0xffa762f0);
const juce::Colour kCyan (0xff20c8d8);
const juce::Colour kGreen (0xff74df35);
const juce::Colour kYellow (0xffe2c832);
const juce::Colour kRed (0xffff5757);

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
        case yesdaw::ui::UiActionId::TransportPlay: return "Play";
        case yesdaw::ui::UiActionId::TransportStop: return "Stop";
        case yesdaw::ui::UiActionId::TransportLocateStart: return "|<";
        case yesdaw::ui::UiActionId::TransportToggleLoop: return "Loop";
        case yesdaw::ui::UiActionId::ViewMixer: return "Mixer";
        case yesdaw::ui::UiActionId::ViewPianoRoll: return "Piano";
        default: break;
    }
    return "?";
}

void fillPanel (juce::Graphics& g, juce::Rectangle<int> area, float radius = 6.0f)
{
    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat(), radius);
    g.setColour (kPanelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), radius, 1.0f);
}

void drawSmallLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                     juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour (kMutedText);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (text, area, justification, false);
}

void drawMeter (juce::Graphics& g, juce::Rectangle<int> area, float value)
{
    g.setColour (juce::Colour (0xff05080b));
    g.fillRoundedRectangle (area.toFloat(), 2.0f);

    auto fill = area.reduced (2);
    const int height = juce::roundToInt (static_cast<float> (fill.getHeight()) * juce::jlimit (0.0f, 1.0f, value));
    auto live = fill.removeFromBottom (height);
    auto hot = live.removeFromTop (juce::roundToInt (static_cast<float> (live.getHeight()) * 0.22f));

    g.setColour (kGreen);
    g.fillRect (live);
    g.setColour (kYellow);
    g.fillRect (hot);
}

void drawHorizontalMeter (juce::Graphics& g, juce::Rectangle<int> area, float value)
{
    g.setColour (juce::Colour (0xff05080b));
    g.fillRoundedRectangle (area.toFloat(), 2.0f);
    auto fill = area.reduced (2);
    const int width = juce::roundToInt (static_cast<float> (fill.getWidth()) * juce::jlimit (0.0f, 1.0f, value));
    auto live = fill.withWidth (width);
    auto hot = live.removeFromRight (juce::roundToInt (static_cast<float> (live.getWidth()) * 0.18f));
    g.setColour (kGreen);
    g.fillRect (live);
    g.setColour (kYellow);
    g.fillRect (hot);
}

} // namespace

class MainComponent : public juce::Component
{
public:
    MainComponent()
    {
        setSize (1536, 960);

        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const yesdaw::ui::UiActionId action = toolbarActions[i];
            const auto* descriptor = registry.descriptor (action);
            if (descriptor == nullptr)
                continue;

            auto& button = buttons[i];
            button.setButtonText (actionButtonText (action));
            button.setTooltip (juce::String (descriptor->stableId) + "  " + descriptor->defaultKey);
            button.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff111820));
            button.setColour (juce::TextButton::buttonOnColourId, descriptor->accessibleRole == yesdaw::ui::AccessibilityRole::ToggleButton
                                                                  ? kPurple.darker (0.45f)
                                                                  : kBlue.darker (0.25f));
            button.setColour (juce::TextButton::textColourOffId, kText);
            button.setColour (juce::TextButton::textColourOnId, kText);
            button.onClick = [this, action] {
                (void) registry.dispatch (action, context);
                refreshActionState();
                repaint();
            };
            addAndMakeVisible (button);
        }

        refreshActionState();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (kBackground);
        drawHeader (g);

        const auto bounds = getLocalBounds();
        const auto top = bounds.withHeight (kHeaderHeight);
        g.setColour (juce::Colour (0xff18202a));
        g.fillRect (top.withBottom (kHeaderHeight).removeFromBottom (1));

        auto work = bounds.withTrimmedTop (kHeaderHeight);
        auto mixer = work.removeFromBottom (kMixerHeight);
        auto left = work.removeFromLeft (kLeftRailWidth).reduced (6, 10);
        auto inspector = work.removeFromRight (kInspectorWidth).reduced (6, 10);
        auto timeline = work.reduced (6, 10);

        drawTrackList (g, left);
        if (context.activePanel == yesdaw::ui::UiPanel::PianoRoll)
            drawPianoRoll (g, timeline);
        else
            drawTimeline (g, timeline);
        drawInspector (g, inspector);
        drawMixer (g, mixer.reduced (6, 8));
    }

    void resized() override
    {
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();

        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto action = toolbarActions[i];
            switch (action)
            {
                case yesdaw::ui::UiActionId::ProjectNew: buttons[i].setBounds (16, 50, 44, 26); break;
                case yesdaw::ui::UiActionId::ProjectOpen: buttons[i].setBounds (64, 50, 50, 26); break;
                case yesdaw::ui::UiActionId::ProjectSave: buttons[i].setBounds (118, 50, 48, 26); break;
                case yesdaw::ui::UiActionId::TransportLocateStart: buttons[i].setBounds (336, 16, 56, 56); break;
                case yesdaw::ui::UiActionId::TransportPlay: buttons[i].setBounds (392, 16, 56, 56); break;
                case yesdaw::ui::UiActionId::TransportStop: buttons[i].setBounds (448, 16, 56, 56); break;
                case yesdaw::ui::UiActionId::TransportToggleLoop: buttons[i].setBounds (1008, 16, 64, 56); break;
                case yesdaw::ui::UiActionId::ViewMixer: buttons[i].setBounds (16, getHeight() - kMixerHeight + 18, 76, 28); break;
                case yesdaw::ui::UiActionId::ViewPianoRoll: buttons[i].setBounds (96, getHeight() - kMixerHeight + 18, 78, 28); break;
                default: buttons[i].setBounds ({});
            }
        }
    }

private:
    void refreshActionState()
    {
        const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const auto action = toolbarActions[i];
            const auto state = registry.stateFor (action, context);
            buttons[i].setEnabled (state.enabled);
            buttons[i].setToggleState ((action == yesdaw::ui::UiActionId::TransportToggleLoop && context.loopEnabled)
                                           || (action == yesdaw::ui::UiActionId::ViewMixer
                                               && context.activePanel == yesdaw::ui::UiPanel::Mixer)
                                           || (action == yesdaw::ui::UiActionId::ViewPianoRoll
                                               && context.activePanel == yesdaw::ui::UiPanel::PianoRoll),
                                       juce::dontSendNotification);
        }
    }

    void drawHeader (juce::Graphics& g) const
    {
        g.setColour (juce::Colour (0xff0d1218));
        g.fillRect (getLocalBounds().withHeight (kHeaderHeight));

        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        int menuX = 22;
        for (const auto* menu : { "FILE", "EDIT", "VIEW", "OPTIONS", "HELP" })
        {
            g.drawText (menu, menuX, 17, 70, 18, juce::Justification::centredLeft, false);
            menuX += menu == std::string ("OPTIONS") ? 72 : 48;
        }

        drawTransportReadouts (g);
        drawMasterMeter (g);
        g.setColour (kPanelStroke);
        g.fillRect (0, kHeaderHeight - 1, getWidth(), 1);
    }

    void drawTransportReadouts (juce::Graphics& g) const
    {
        auto time = juce::Rectangle<int> (570, 16, 190, 56);
        fillPanel (g, time, 5.0f);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (25.0f)));
        g.drawText ("01:02:45:18", time.reduced (8, 4).removeFromTop (30), juce::Justification::centred, false);
        drawSmallLabel (g, "BAR | BEAT", time.reduced (8, 34), juce::Justification::centred);

        const std::array<std::pair<const char*, const char*>, 3> readouts {{
            { "120.00", "TEMPO" },
            { "4/4", "TIME SIG" },
            { "Cmaj", "KEY" }
        }};

        auto box = juce::Rectangle<int> (760, 16, 248, 56);
        for (const auto& readout : readouts)
        {
            auto cell = box.removeFromLeft (82);
            fillPanel (g, cell, 0.0f);
            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (16.0f)));
            g.drawText (readout.first, cell.reduced (4, 8).removeFromTop (24), juce::Justification::centred, false);
            drawSmallLabel (g, readout.second, cell.reduced (4, 34), juce::Justification::centred);
        }

        g.setColour (kRed);
        g.fillEllipse (520.0f, 36.0f, 18.0f, 18.0f);
    }

    void drawMasterMeter (juce::Graphics& g) const
    {
        auto master = juce::Rectangle<int> (1110, 18, 300, 44);
        drawSmallLabel (g, "MASTER", master.removeFromTop (14));
        auto meter = master.removeFromTop (16).withWidth (236);
        drawHorizontalMeter (g, meter, 0.76f);
        const juce::String lufs = mixerSurface.loudness.valid
            ? juce::String (mixerSurface.loudness.integratedLufs, 1) + " LUFS"
            : "-- LUFS";
        drawSmallLabel (g, lufs, juce::Rectangle<int> (1370, 33, 76, 16), juce::Justification::centred);

        g.setColour (kMutedText);
        g.setFont (juce::Font (juce::FontOptions (19.0f)));
        g.drawText ("*", getWidth() - 54, 34, 24, 24, juce::Justification::centred, false);
    }

    void drawTrackList (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        fillPanel (g, area);
        auto header = area.removeFromTop (38);
        drawSmallLabel (g, "TRACKS", header.reduced (16, 0));

        const int rowHeight = juce::jmax (56, area.getHeight() / static_cast<int> (kTracks.size()));
        for (std::size_t i = 0; i < kTracks.size(); ++i)
        {
            auto row = area.removeFromTop (rowHeight);
            const auto& track = kTracks[i];

            g.setColour (i == 3 ? juce::Colour (0xff17131f) : juce::Colour (0xff121820));
            g.fillRect (row.reduced (1, 0));
            g.setColour (track.colour);
            g.fillRect (row.withWidth (3).reduced (0, 1));
            g.setColour (kPanelStroke);
            g.fillRect (row.removeFromBottom (1));

            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            g.drawText (track.name, row.withTrimmedLeft (88).withHeight (24).translated (0, 9),
                        juce::Justification::centredLeft, false);

            g.setFont (juce::Font (juce::FontOptions (16.0f)));
            g.drawText (juce::String (static_cast<int> (i + 1)), row.withWidth (40), juce::Justification::centred, false);

            auto buttonsArea = row.withTrimmedLeft (88).withTrimmedTop (34).withHeight (18);
            for (const auto* label : { "M", "S", "O" })
            {
                auto cell = buttonsArea.removeFromLeft (24).reduced (2, 0);
                g.setColour (juce::Colour (0xff0a0f14));
                g.fillRoundedRectangle (cell.toFloat(), 3.0f);
                g.setColour (label == std::string ("O") ? kRed : kMutedText);
                g.setFont (juce::Font (juce::FontOptions (10.0f)));
                g.drawText (label, cell, juce::Justification::centred, false);
            }

            auto meter = row.withRight (row.getRight() - 12).removeFromRight (14).reduced (0, 10);
            drawMeter (g, meter, track.meter);
        }
    }

    void drawTimeline (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        yesdaw::ui::TimelineCanvasState state;
        state.tracks = kTracks.data();
        state.trackCount = static_cast<int> (kTracks.size());
        state.clips = kClips.data();
        state.clipStyles = kClipStyles.data();
        state.clipCount = static_cast<int> (kClips.size());
        state.markers = kTimelineMarkers.data();
        state.markerCount = static_cast<int> (kTimelineMarkers.size());
        state.viewport.scrollSeconds = 0.0;
        state.viewport.pixelsPerSecond = static_cast<double> (juce::jmax (1, area.getWidth() - 24)) / 98.0;
        state.totalSeconds = 98.0;
        state.playheadSeconds = 32.0;

        (void) yesdaw::ui::paintTimelineCanvas (g, area, state);
    }

    void drawPianoRoll (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        fillPanel (g, area);
        auto header = area.removeFromTop (38);
        drawSmallLabel (g, "PIANO ROLL", header.reduced (14, 0));
        drawSmallLabel (g, "MIDI Clip_01  |  Note edits: select move length transpose quantize",
                        header.reduced (14, 0), juce::Justification::centredRight);

        area.reduce (12, 8);
        auto expression = area.removeFromBottom (84);
        auto keyboard = area.removeFromLeft (70);
        auto grid = area.reduced (0, 2);

        constexpr int kLowKey = 48;
        constexpr int kHighKey = 72;
        constexpr int kKeyCount = kHighKey - kLowKey + 1;
        const float rowHeight = static_cast<float> (grid.getHeight()) / static_cast<float> (kKeyCount);

        auto keyY = [grid, rowHeight] (int key) {
            return grid.getY() + juce::roundToInt (static_cast<float> (kHighKey - key) * rowHeight);
        };

        const double timelineLength = static_cast<double> (juce::jmax<yesdaw::engine::Tick> (1, pianoSurface.timelineLength));
        auto tickX = [grid, timelineLength] (yesdaw::engine::Tick tick) {
            const double normalized = static_cast<double> (tick) / timelineLength;
            return grid.getX() + juce::roundToInt (static_cast<float> (normalized) * static_cast<float> (grid.getWidth()));
        };

        g.setColour (juce::Colour (0xff070b10));
        g.fillRect (grid);

        for (int key = kHighKey; key >= kLowKey; --key)
        {
            const int y = keyY (key);
            auto keyRow = juce::Rectangle<int> (keyboard.getX(), y, keyboard.getWidth(), juce::jmax (1, juce::roundToInt (rowHeight)));
            g.setColour (isBlackMidiKey (key) ? juce::Colour (0xff0a0e13) : juce::Colour (0xff151c24));
            g.fillRect (keyRow.reduced (0, 1));
            g.setColour (kPanelStroke.withAlpha (0.72f));
            g.fillRect (juce::Rectangle<int> (grid.getX(), y, grid.getWidth(), 1));

            if (key % 12 == 0)
            {
                g.setColour (kMutedText);
                g.setFont (juce::Font (juce::FontOptions (10.0f)));
                g.drawText ("C" + juce::String (key / 12 - 1), keyRow.reduced (8, 0),
                            juce::Justification::centredLeft, false);
            }
        }

        for (yesdaw::engine::Tick tick = 0; tick <= pianoSurface.timelineLength; tick += 512)
        {
            const int x = tickX (tick);
            g.setColour ((tick % 2048) == 0 ? juce::Colour (0xff344150) : juce::Colour (0xff202a34));
            g.fillRect (x, grid.getY(), 1, grid.getHeight());
        }

        for (const yesdaw::ui::UiPianoRollNoteView& note : pianoSurface.notes)
        {
            if (note.key < kLowKey || note.key > kHighKey)
                continue;

            const int x = tickX (note.startTick);
            const int width = juce::jmax (10, tickX (note.startTick + note.lengthTicks) - x);
            const int y = keyY (note.key) + 2;
            const int height = juce::jmax (8, juce::roundToInt (rowHeight) - 4);
            auto noteRect = juce::Rectangle<int> (x, y, width, height).reduced (1, 0);

            g.setColour ((note.selected ? kPurple : kCyan).withAlpha (0.34f));
            g.fillRoundedRectangle (noteRect.expanded (1).toFloat(), 4.0f);
            g.setColour (note.selected ? kPurple.brighter (0.35f) : kCyan);
            g.fillRoundedRectangle (noteRect.toFloat(), 3.0f);
        }

        expression.reduce (0, 6);
        for (const yesdaw::ui::UiPianoRollExpressionLaneReadout& lane : pianoSurface.expressionLanes)
        {
            auto laneArea = expression.removeFromTop (36).reduced (0, 2);
            g.setColour (juce::Colour (0xff0b1016));
            g.fillRect (laneArea);
            drawSmallLabel (g,
                            lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity ? "Velocity" : "Pitch",
                            laneArea.reduced (8, 0));

            const double minValue = lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity ? 0.0 : 48.0;
            const double maxValue = lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity ? 1.0 : 76.0;
            juce::Path path;

            for (std::size_t i = 0; i < lane.points.size(); ++i)
            {
                const auto& point = lane.points[i];
                const double normalized = juce::jlimit (0.0, 1.0, (point.value - minValue) / (maxValue - minValue));
                const float x = static_cast<float> (tickX (point.tick));
                const float y = static_cast<float> (laneArea.getBottom() - 5)
                    - static_cast<float> (normalized) * static_cast<float> (laneArea.getHeight() - 10);
                if (i == 0)
                    path.startNewSubPath (x, y);
                else
                    path.lineTo (x, y);

                g.setColour (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity ? kGreen : kPurple);
                g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
            }

            g.setColour (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Velocity ? kGreen : kPurple);
            g.strokePath (path, juce::PathStrokeType (1.5f));
        }
    }

    void drawInspector (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        fillPanel (g, area);
        auto tabs = area.removeFromTop (40);
        g.setColour (juce::Colour (0xff151a22));
        g.fillRect (tabs.removeFromLeft (area.getWidth() / 2));
        drawSmallLabel (g, "CLIP", area.withY (tabs.getY()).withHeight (40).withWidth (area.getWidth() / 2),
                        juce::Justification::centred);
        drawSmallLabel (g, "TRACK", area.withY (tabs.getY()).withHeight (40).withTrimmedLeft (area.getWidth() / 2),
                        juce::Justification::centred);

        area.reduce (16, 14);
        g.setColour (kPurple);
        g.fillRoundedRectangle (static_cast<float> (area.getX()),
                                static_cast<float> (area.getY() + 4),
                                12.0f,
                                12.0f,
                                3.0f);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText ("Vocal Lead_03", area.withTrimmedLeft (20).withHeight (24), juce::Justification::centredLeft, false);

        auto stats = area.withTrimmedTop (42).withHeight (46);
        for (const auto* label : { "Start\n33.1.1.00", "End\n41.1.1.00", "Length\n8.0.0.00" })
        {
            auto cell = stats.removeFromLeft (stats.getWidth() / 3).reduced (4, 0);
            g.setColour (juce::Colour (0xff0b1016));
            g.fillRoundedRectangle (cell.toFloat(), 4.0f);
            g.setColour (kMutedText);
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.drawFittedText (label, cell.reduced (4), juce::Justification::centred, 2);
        }

        auto gain = area.withTrimmedTop (118).withHeight (84);
        drawSmallLabel (g, "GAIN", gain.removeFromTop (20));
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText ("+2.4 dB", gain.withTrimmedLeft (72).withHeight (24), juce::Justification::centredLeft, false);
        drawHorizontalMeter (g, gain.withTrimmedLeft (72).withTrimmedTop (32).withHeight (12), 0.76f);

        auto fades = area.withTrimmedTop (214).withHeight (94);
        drawSmallLabel (g, "FADES", fades.removeFromTop (20));
        for (const auto* label : { "Fade In     0.10 s", "Fade Out    0.25 s" })
        {
            auto row = fades.removeFromTop (32).reduced (0, 3);
            g.setColour (juce::Colour (0xff0b1016));
            g.fillRoundedRectangle (row.toFloat(), 4.0f);
            g.setColour (kText);
            g.drawText (label, row.reduced (8, 0), juce::Justification::centredLeft, false);
        }

        auto fx = area.withTrimmedTop (330);
        drawSmallLabel (g, "CLIP FX", fx.removeFromTop (20));
        for (const auto* label : { "De-Esser", "Compressor", "EQ" })
        {
            auto row = fx.removeFromTop (28).reduced (0, 2);
            g.setColour (juce::Colour (0xff0b1016));
            g.fillRoundedRectangle (row.toFloat(), 4.0f);
            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText (label, row.reduced (10, 0), juce::Justification::centredLeft, false);
        }
    }

    void drawMixer (juce::Graphics& g, juce::Rectangle<int> area) const
    {
        g.setColour (juce::Colour (0xff0a0f14));
        g.fillRect (area);

        auto leftTools = area.removeFromLeft (120).reduced (8, 0);
        fillPanel (g, leftTools, 4.0f);
        drawSmallLabel (g, "SENDS", leftTools.withTrimmedTop (52).withHeight (24).reduced (12, 0));
        drawSmallLabel (g, "VIEW", leftTools.withTrimmedTop (96).withHeight (24).reduced (12, 0));
        drawSmallLabel (g, "Short", leftTools.withTrimmedTop (120).withHeight (28).reduced (12, 0));

        const int stripWidth = juce::jmax (84, area.getWidth() / (static_cast<int> (kMixer.size()) + 1));
        for (std::size_t stripIndex = 0; stripIndex < kMixer.size(); ++stripIndex)
        {
            const auto& strip = kMixer[stripIndex];
            const bool isBus = stripIndex >= mixerSurface.tracks.size();
            const auto& state = isBus ? mixerSurface.buses[stripIndex - mixerSurface.tracks.size()]
                                      : mixerSurface.tracks[stripIndex];

            auto lane = area.removeFromLeft (stripWidth).reduced (3, 0);
            g.setColour (strip.selected ? juce::Colour (0xff1c1428) : kPanelRaised);
            g.fillRoundedRectangle (lane.toFloat(), 5.0f);
            g.setColour (strip.selected ? kPurple : kPanelStroke);
            g.drawRoundedRectangle (lane.toFloat().reduced (0.5f), 5.0f, strip.selected ? 2.0f : 1.0f);

            g.setColour (strip.colour.withAlpha (0.30f));
            g.fillRect (lane.withHeight (28));
            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawFittedText (state.name, lane.reduced (8, 4).withHeight (20), juce::Justification::centred, 1);

            auto knob = lane.withTrimmedTop (36).withHeight (38);
            g.setColour (juce::Colour (0xff070b10));
            g.fillEllipse (static_cast<float> (knob.getCentreX() - 13), static_cast<float> (knob.getY() + 4), 26.0f, 26.0f);
            g.setColour (strip.colour.brighter (0.45f));
            g.drawEllipse (static_cast<float> (knob.getCentreX() - 13), static_cast<float> (knob.getY() + 4), 26.0f, 26.0f, 1.2f);

            auto buttonsRow = lane.withTrimmedTop (78).withHeight (28).reduced (14, 0);
            for (const auto* label : { "S", "M" })
            {
                auto cell = buttonsRow.removeFromLeft (30).reduced (3, 2);
                g.setColour (juce::Colour (0xff0b1016));
                g.fillRoundedRectangle (cell.toFloat(), 4.0f);
                const bool on = (label == std::string ("S") && state.soloed)
                             || (label == std::string ("M") && state.muted);
                g.setColour (on ? strip.colour.brighter (0.55f) : kText);
                g.drawText (label, cell, juce::Justification::centred, false);
            }

            if (state.sidechainVisible)
            {
                auto badge = lane.withTrimmedTop (106).withHeight (14).withTrimmedLeft (8).withWidth (28);
                g.setColour (juce::Colour (0xff0b1016));
                g.fillRoundedRectangle (badge.toFloat(), 3.0f);
                g.setColour (kMutedText);
                g.setFont (juce::Font (juce::FontOptions (8.0f)));
                g.drawText ("SC", badge, juce::Justification::centred, false);
            }

            auto faderArea = lane.withTrimmedTop (112).withTrimmedBottom (28);
            auto meter = faderArea.removeFromRight (16).reduced (2, 0);
            drawMeter (g, meter, state.meter.valid ? state.meter.peakLeft : 0.0f);

            auto rail = faderArea.withWidth (18).withCentre ({ lane.getCentreX() - 8, faderArea.getCentreY() });
            g.setColour (juce::Colour (0xff05080b));
            g.fillRoundedRectangle (rail.toFloat(), 3.0f);
            const int thumbY = rail.getBottom() - juce::roundToInt (state.linearGain * static_cast<float> (rail.getHeight())) - 8;
            auto thumb = juce::Rectangle<int> (rail.getX() - 5, thumbY, rail.getWidth() + 10, 18);
            g.setColour (juce::Colour (0xffc4c9cf));
            g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
        }
    }

    yesdaw::ui::UiActionRegistry registry;
    yesdaw::ui::UiActionContext context;
    yesdaw::ui::UiMixerSurfaceSnapshot mixerSurface = makeDemoMixerSurface();
    yesdaw::ui::UiPianoRollSurfaceSnapshot pianoSurface = makeDemoPianoRollSurface();
    std::array<juce::TextButton, yesdaw::ui::kMainShellToolbarActions.size()> buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

class YesDawApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "YesDaw"; }
    const juce::String getApplicationVersion() override { return "0.0.0"; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow ("YES DAW", new MainComponent(), *this));
    }

    void shutdown() override { mainWindow = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, juce::Component* content, JUCEApplication& a)
            : DocumentWindow (name, juce::Colour (0xff0b0f14), DocumentWindow::allButtons), app (a)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { app.systemRequestedQuit(); }

    private:
        JUCEApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (YesDawApplication)
