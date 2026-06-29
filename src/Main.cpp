// YES DAW - H11 app shell.
//
// The visible JUCE shell and the headless tests share ui/UiActions.h. This checkpoint keeps the shell
// image-light and model-backed: later H11 slices wire Project loading, transport, timeline drawing,
// accessibility traversal, and editing through the same action IDs.

#include "ui/UiActions.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <cmath>
#include <cstddef>
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
const juce::Colour kGrid (0xff24303a);
const juce::Colour kBlue (0xff3b8cff);
const juce::Colour kTeal (0xff1bb5a6);
const juce::Colour kAmber (0xffd29118);
const juce::Colour kPurple (0xffa762f0);
const juce::Colour kCyan (0xff20c8d8);
const juce::Colour kGreen (0xff74df35);
const juce::Colour kYellow (0xffe2c832);
const juce::Colour kRed (0xffff5757);

struct TrackRow
{
    const char* name;
    juce::Colour colour;
    float meter;
};

struct ClipBlock
{
    int track;
    int start;
    int length;
    juce::Colour colour;
    float amp;
};

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

const std::array<ClipBlock, 23> kClips {{
    { 0, 1, 17, kBlue, 0.82f }, { 0, 18, 10, kBlue, 0.78f }, { 0, 28, 17, kBlue, 0.80f },
    { 0, 49, 17, kBlue, 0.70f }, { 0, 66, 12, kBlue, 0.76f }, { 0, 78, 14, kBlue, 0.85f },
    { 1, 5, 18, kTeal, 0.72f }, { 1, 23, 20, kTeal, 0.75f }, { 1, 48, 30, kTeal, 0.70f },
    { 2, 4, 18, kAmber, 0.64f }, { 2, 26, 18, kAmber, 0.67f }, { 2, 42, 26, kAmber, 0.70f },
    { 2, 70, 16, kAmber, 0.62f },
    { 3, 12, 28, kPurple, 0.88f }, { 3, 40, 18, kPurple, 0.90f }, { 3, 58, 30, kPurple, 0.86f },
    { 4, 12, 24, kPurple.darker (0.2f), 0.62f }, { 4, 38, 22, kPurple.darker (0.2f), 0.66f },
    { 4, 60, 28, kPurple.darker (0.2f), 0.58f },
    { 5, 8, 26, kCyan, 0.50f }, { 5, 34, 22, kCyan, 0.52f }, { 5, 66, 24, kCyan, 0.48f },
    { 6, 1, 38, kBlue.darker (0.35f), 0.68f }
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

void drawWaveform (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour, float amp)
{
    const auto midY = static_cast<float> (area.getCentreY());
    const auto half = static_cast<float> (area.getHeight()) * amp * 0.42f;
    juce::Path path;
    path.startNewSubPath (static_cast<float> (area.getX()), midY);

    for (int x = 0; x < area.getWidth(); x += 3)
    {
        const float phase = static_cast<float> (x) * 0.18f;
        const float noise = std::sin (phase) * 0.62f + std::sin (phase * 2.37f) * 0.25f;
        path.lineTo (static_cast<float> (area.getX() + x), midY + noise * half);
    }

    g.setColour (colour.brighter (0.45f));
    g.strokePath (path, juce::PathStrokeType (1.2f));
}

void drawClip (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour, float amp)
{
    g.setColour (colour.withAlpha (0.42f));
    g.fillRoundedRectangle (area.toFloat(), 4.0f);
    g.setColour (colour.brighter (0.35f));
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 4.0f, 1.0f);
    drawWaveform (g, area.reduced (8, 5), colour, amp);
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
        drawSmallLabel (g, "-7.2 LUFS", juce::Rectangle<int> (1370, 33, 76, 16), juce::Justification::centred);

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
        fillPanel (g, area);
        auto toolbar = area.removeFromTop (36);
        drawSmallLabel (g, "SNAP", toolbar.withTrimmedLeft (234).withWidth (42), juce::Justification::centred);
        g.setColour (juce::Colour (0xff070b0f));
        g.fillRoundedRectangle (toolbar.withTrimmedLeft (276).withWidth (80).reduced (0, 7).toFloat(), 3.0f);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText ("Bar", toolbar.withTrimmedLeft (284).withWidth (54), juce::Justification::centredLeft, false);

        auto ruler = area.removeFromTop (48);
        g.setColour (juce::Colour (0xff0b1117));
        g.fillRect (ruler);
        g.setColour (kGrid);
        g.fillRect (ruler.removeFromBottom (1));

        for (int bar = 1; bar <= 99; bar += 8)
        {
            const int x = area.getX() + juce::roundToInt ((bar - 1) / 98.0f * static_cast<float> (area.getWidth()));
            g.setColour (kMutedText);
            g.drawText (juce::String (bar), x - 18, ruler.getY() + 7, 36, 16, juce::Justification::centred, false);
            g.setColour (kMutedText.withAlpha (0.65f));
            g.fillRect (x, ruler.getBottom() - 14, 1, 14);
        }

        for (const auto& marker : { std::pair<int, const char*> { 9, "Intro" },
                                    { 25, "Verse" },
                                    { 33, "Chorus" },
                                    { 65, "Bridge" },
                                    { 81, "Outro" } })
        {
            const int x = area.getX() + juce::roundToInt ((marker.first - 1) / 98.0f * static_cast<float> (area.getWidth()));
            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawText (marker.second, x + 4, ruler.getY() + 24, 60, 16, juce::Justification::centredLeft, false);
        }

        const auto clipArea = area.reduced (12, 0);
        const int laneHeight = clipArea.getHeight() / static_cast<int> (kTracks.size());
        g.setColour (juce::Colour (0xff0c1217));
        g.fillRect (area);

        for (int lane = 0; lane <= static_cast<int> (kTracks.size()); ++lane)
        {
            const int y = clipArea.getY() + lane * laneHeight;
            g.setColour (kGrid.withAlpha (0.7f));
            g.fillRect (clipArea.getX(), y, clipArea.getWidth(), 1);
        }

        for (int bar = 1; bar <= 99; bar += 4)
        {
            const int x = clipArea.getX() + juce::roundToInt ((bar - 1) / 98.0f * static_cast<float> (clipArea.getWidth()));
            g.setColour ((bar - 1) % 16 == 0 ? kGrid.brighter (0.25f) : kGrid.withAlpha (0.38f));
            g.fillRect (x, clipArea.getY(), 1, clipArea.getHeight());
        }

        for (const auto& clip : kClips)
        {
            auto lane = clipArea.withTop (clipArea.getY() + clip.track * laneHeight).withHeight (laneHeight).reduced (4, 5);
            const int x = clipArea.getX() + juce::roundToInt ((clip.start - 1) / 98.0f * static_cast<float> (clipArea.getWidth()));
            const int w = juce::roundToInt (clip.length / 98.0f * static_cast<float> (clipArea.getWidth()));
            drawClip (g, lane.withX (x).withWidth (juce::jmax (18, w)), clip.colour, clip.amp);
        }

        const int playheadX = clipArea.getX() + juce::roundToInt (0.39f * static_cast<float> (clipArea.getWidth()));
        g.setColour (juce::Colours::white);
        g.fillRect (playheadX, ruler.getY(), 2, clipArea.getBottom() - ruler.getY());
        g.setColour (kPurple);
        g.fillRoundedRectangle (static_cast<float> (playheadX - 15), static_cast<float> (ruler.getY() + 4), 30.0f, 16.0f, 7.0f);
        g.setColour (kText);
        g.drawText ("33", playheadX - 12, ruler.getY() + 4, 24, 16, juce::Justification::centred, false);
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
        for (const auto& strip : kMixer)
        {
            auto lane = area.removeFromLeft (stripWidth).reduced (3, 0);
            g.setColour (strip.selected ? juce::Colour (0xff1c1428) : kPanelRaised);
            g.fillRoundedRectangle (lane.toFloat(), 5.0f);
            g.setColour (strip.selected ? kPurple : kPanelStroke);
            g.drawRoundedRectangle (lane.toFloat().reduced (0.5f), 5.0f, strip.selected ? 2.0f : 1.0f);

            g.setColour (strip.colour.withAlpha (0.30f));
            g.fillRect (lane.withHeight (28));
            g.setColour (kText);
            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            g.drawFittedText (strip.name, lane.reduced (8, 4).withHeight (20), juce::Justification::centred, 1);

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
                g.setColour (kText);
                g.drawText (label, cell, juce::Justification::centred, false);
            }

            auto faderArea = lane.withTrimmedTop (112).withTrimmedBottom (28);
            auto meter = faderArea.removeFromRight (16).reduced (2, 0);
            drawMeter (g, meter, strip.meter);

            auto rail = faderArea.withWidth (18).withCentre ({ lane.getCentreX() - 8, faderArea.getCentreY() });
            g.setColour (juce::Colour (0xff05080b));
            g.fillRoundedRectangle (rail.toFloat(), 3.0f);
            const int thumbY = rail.getBottom() - juce::roundToInt (strip.fader * static_cast<float> (rail.getHeight())) - 8;
            auto thumb = juce::Rectangle<int> (rail.getX() - 5, thumbY, rail.getWidth() + 10, 18);
            g.setColour (juce::Colour (0xffc4c9cf));
            g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
        }
    }

    yesdaw::ui::UiActionRegistry registry;
    yesdaw::ui::UiActionContext context;
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
