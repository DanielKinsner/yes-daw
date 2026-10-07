// YES DAW - ADR-0063 gates (G6.1 cp1): the plan's type scale, and WCAG 2.x contrast over every classified colour
// token where it is drawn.

#include "ui/UiTheme.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <string>
#include <vector>

namespace {

using Color = yesdaw::ui::UiTheme::Color;
using Token = juce::Colour (*)() noexcept;

enum class Role { Text, Surface, Fill, Indicator, Decoration };

struct Classified
{
    const char* name;
    Token token;
    Role role;
};

#define YESDAW_TOKEN(tokenName, tokenRole) Classified { #tokenName, &Color::tokenName, Role::tokenRole }

// Every UiTheme::Color token and what it is (the gate below fails on a token missing here, so this cannot go stale).
const std::vector<Classified>& classification()
{
    static const std::vector<Classified> table {
        YESDAW_TOKEN (text, Text),                    YESDAW_TOKEN (mutedText, Text),
        YESDAW_TOKEN (buttonTextMuted, Text),         YESDAW_TOKEN (soloActiveText, Text),
        YESDAW_TOKEN (pianoWhiteKeyText, Text),       YESDAW_TOKEN (pianoWhiteKeyMutedText, Text),
        YESDAW_TOKEN (textOnFill, Text),

        YESDAW_TOKEN (appBackground, Surface),        YESDAW_TOKEN (panel, Surface),
        YESDAW_TOKEN (panelRaised, Surface),          YESDAW_TOKEN (timelineCanvas, Surface),
        YESDAW_TOKEN (timelineToolbar, Surface),      YESDAW_TOKEN (timelineRuler, Surface),
        YESDAW_TOKEN (controlInset, Surface),         YESDAW_TOKEN (controlInsetDeep, Surface),
        YESDAW_TOKEN (controlInsetBlack, Surface),    YESDAW_TOKEN (toolButton, Surface),
        YESDAW_TOKEN (buttonSurface, Surface),        YESDAW_TOKEN (buttonSurfaceTop, Surface),
        YESDAW_TOKEN (buttonPressed, Surface),        YESDAW_TOKEN (darkControl, Surface),
        YESDAW_TOKEN (warningButton, Surface),        YESDAW_TOKEN (canvasLayer, Surface),
        YESDAW_TOKEN (selectedLane, Surface),         YESDAW_TOKEN (mixerBack, Surface),
        YESDAW_TOKEN (pianoBlackKey, Surface),        YESDAW_TOKEN (pianoWhiteKey, Surface),
        YESDAW_TOKEN (inspectorTab, Surface),         YESDAW_TOKEN (selectedStrip, Surface),
        YESDAW_TOKEN (knobFace, Surface),             YESDAW_TOKEN (samplerPadEmpty, Surface),
        YESDAW_TOKEN (meterTrack, Surface),

        YESDAW_TOKEN (accentBlue, Fill),              YESDAW_TOKEN (accentTeal, Fill),
        YESDAW_TOKEN (accentAmber, Fill),             YESDAW_TOKEN (accentPurple, Fill),
        YESDAW_TOKEN (accentCyan, Fill),              YESDAW_TOKEN (accentPurpleDeep, Fill),
        YESDAW_TOKEN (recordArm, Fill),               YESDAW_TOKEN (soloActive, Fill),
        YESDAW_TOKEN (dangerRed, Fill),               YESDAW_TOKEN (samplerPadLoaded, Fill),

        YESDAW_TOKEN (focusRing, Indicator),          YESDAW_TOKEN (accentPurpleGlow, Indicator),
        YESDAW_TOKEN (meterGreen, Indicator),         YESDAW_TOKEN (meterYellow, Indicator),
        YESDAW_TOKEN (midiInLampLit, Indicator),      YESDAW_TOKEN (scaleTick, Indicator),
        YESDAW_TOKEN (faderThumb, Indicator),         YESDAW_TOKEN (faderThumbTop, Indicator),
        YESDAW_TOKEN (white, Indicator),   // the playhead line

        // Exempt: lines, strokes, shadows and washes that carry no meaning of their own; the knob's unfilled track; the
        // MIDI lamp's off state (the lit state is the indicator).
        YESDAW_TOKEN (panelStroke, Decoration),       YESDAW_TOKEN (panelInnerHighlight, Decoration),
        YESDAW_TOKEN (panelShadow, Decoration),
        YESDAW_TOKEN (timelineGrid, Decoration),      YESDAW_TOKEN (separator, Decoration),
        YESDAW_TOKEN (pianoRollInScaleRow, Decoration), YESDAW_TOKEN (pianoGridStrong, Decoration),
        YESDAW_TOKEN (pianoGridWeak, Decoration),     YESDAW_TOKEN (buttonBorder, Decoration),
        YESDAW_TOKEN (midiInLampOff, Decoration),     YESDAW_TOKEN (rulerTick, Decoration),
        YESDAW_TOKEN (knobArc, Decoration),           YESDAW_TOKEN (transparent, Decoration),
    };
    return table;
}

std::string readSource (const std::filesystem::path& relative)
{
    std::ifstream in (std::filesystem::path { YESDAW_SOURCE_DIR } / relative, std::ios::binary);
    REQUIRE (in.good());
    return std::string ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
}

// The token names declared inside `struct Color` in UiTheme.h.
std::vector<std::string> declaredColourTokens()
{
    const std::string source = readSource ("src/ui/UiTheme.h");
    const std::size_t begin = source.find ("struct Color");
    REQUIRE (begin != std::string::npos);
    const std::size_t end = source.find ("\n    };", begin);
    REQUIRE (end != std::string::npos);
    const std::string body = source.substr (begin, end - begin);
    std::vector<std::string> names;
    const std::regex declaration (R"(static juce::Colour (\w+)\s*\(\))");
    for (auto it = std::sregex_iterator (body.begin(), body.end(), declaration); it != std::sregex_iterator(); ++it)
        names.push_back ((*it)[1].str());
    return names;
}

// WCAG 2.x relative luminance (gamma-correct sRGB).
double luminance (juce::Colour colour)
{
    const auto channel = [] (float value) {
        const double v = value;
        return v <= 0.03928 ? v / 12.92 : std::pow ((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel (colour.getFloatRed()) + 0.7152 * channel (colour.getFloatGreen())
         + 0.0722 * channel (colour.getFloatBlue());
}

double contrast (juce::Colour a, juce::Colour b)
{
    const double la = luminance (a);
    const double lb = luminance (b);
    return (std::max (la, lb) + 0.05) / (std::min (la, lb) + 0.05);
}

struct Pair
{
    std::string what;
    juce::Colour foreground;
    juce::Colour background;
    double minimum;
};

} // namespace

TEST_CASE ("ADR-0063 the type scale: labels at least 11 px, base UI text 12 px", "[tokens]")
{
    using Type = yesdaw::ui::UiTheme::Type;
    for (const float size : { Type::tiny, Type::caption, Type::small, Type::body, Type::title, Type::readout,
                              Type::statusIcon, Type::transportClock })
        REQUIRE (size >= 11.0f);
    STATIC_REQUIRE (Type::small == 12.0f);   // section 3.4's base UI text
}

TEST_CASE ("ADR-0063 every colour token is classified (text, surface, fill, indicator or decoration)", "[tokens]")
{
    std::set<std::string> classified;
    for (const Classified& entry : classification())
        classified.insert (entry.name);
    const std::vector<std::string> declared = declaredColourTokens();
    REQUIRE (declared.size() >= 60u);
    for (const std::string& name : declared)
    {
        INFO ("UiTheme::Color::" << name << " has no role in the contrast table");
        REQUIRE (classified.count (name) == 1u);
    }
    REQUIRE (classified.size() == declared.size());   // nothing classified that no longer exists
}

TEST_CASE ("ADR-0063 text reaches 4.5:1 and indicators 3:1 wherever they are drawn (WCAG 2.x)", "[tokens]")
{
    using Tone = yesdaw::ui::UiTheme::Tone;
    std::vector<Pair> pairs;
    const auto add = [&pairs] (const std::string& what, juce::Colour foreground, juce::Colour background, double minimum) {
        pairs.push_back ({ what, foreground, background, minimum });
    };

    const std::vector<std::pair<std::string, juce::Colour>> surfaces {
        { "appBackground", Color::appBackground() }, { "panel", Color::panel() }, { "panelRaised", Color::panelRaised() },
        { "timelineCanvas", Color::timelineCanvas() }, { "timelineToolbar", Color::timelineToolbar() },
        { "timelineRuler", Color::timelineRuler() }, { "canvasLayer", Color::canvasLayer() },
        { "buttonSurface", Color::buttonSurface() }, { "buttonSurfaceTop", Color::buttonSurfaceTop() },
        { "toolButton", Color::toolButton() }, { "darkControl", Color::darkControl() },
        { "warningButton", Color::warningButton() }, { "inspectorTab", Color::inspectorTab() },
        { "mixerBack", Color::mixerBack() }, { "controlInset", Color::controlInset() },
        { "controlInsetBlack", Color::controlInsetBlack() }, { "selectedLane", Color::selectedLane() },
        { "selectedStrip", Color::selectedStrip() }, { "knobFace", Color::knobFace() },
        { "samplerPadEmpty", Color::samplerPadEmpty() },
    };
    // Text: the three text levels on every surface; the accents and the danger red where they are drawn as text.
    for (const auto& [name, surface] : surfaces)
    {
        add ("text on " + name, Color::text(), surface, 4.5);
        add ("mutedText on " + name, Color::mutedText(), surface, 4.5);
        add ("buttonTextMuted on " + name, Color::buttonTextMuted(), surface, 4.5);
    }
    const std::vector<std::pair<std::string, juce::Colour>> darkSurfaces {
        { "appBackground", Color::appBackground() }, { "panel", Color::panel() }, { "panelRaised", Color::panelRaised() },
        { "timelineCanvas", Color::timelineCanvas() }, { "timelineToolbar", Color::timelineToolbar() },
        { "buttonSurface", Color::buttonSurface() }, { "inspectorTab", Color::inspectorTab() }, { "mixerBack", Color::mixerBack() },
    };
    const std::vector<std::pair<std::string, juce::Colour>> accents {
        { "accentBlue", Color::accentBlue() }, { "accentTeal", Color::accentTeal() }, { "accentAmber", Color::accentAmber() },
        { "accentPurple", Color::accentPurple() }, { "accentCyan", Color::accentCyan() },
    };
    for (const auto& [surfaceName, surface] : darkSurfaces)
    {
        for (const auto& [accentName, accent] : accents)
            add (accentName + " text on " + surfaceName, accent, surface, 4.5);
        add ("dangerRed text on " + surfaceName, Color::dangerRed(), surface, 4.5);
    }
    // Text on fills: the dark ink on every solid fill it is drawn on; the others' own pairs.
    for (const auto& [accentName, accent] : accents)
        add ("textOnFill on " + accentName, Color::textOnFill(), accent, 4.5);
    add ("textOnFill on recordArm", Color::textOnFill(), Color::recordArm(), 4.5);
    add ("textOnFill on soloActive", Color::textOnFill(), Color::soloActive(), 4.5);
    add ("textOnFill on dangerRed", Color::textOnFill(), Color::dangerRed(), 4.5);
    add ("soloActiveText on soloActive", Color::soloActiveText(), Color::soloActive(), 4.5);
    add ("soloActiveText on accentAmber (the lit DIM)", Color::soloActiveText(), Color::accentAmber(), 4.5);
    add ("soloActiveText on dangerRed (the lit MUTE)", Color::soloActiveText(), Color::dangerRed(), 4.5);
    add ("pianoWhiteKeyText on pianoWhiteKey", Color::pianoWhiteKeyText(), Color::pianoWhiteKey(), 4.5);
    add ("pianoWhiteKeyMutedText on pianoWhiteKey", Color::pianoWhiteKeyMutedText(), Color::pianoWhiteKey(), 4.5);
    add ("text on samplerPadLoaded", Color::text(), Color::samplerPadLoaded(), 4.5);
    add ("text on accentPurpleDeep", Color::text(), Color::accentPurpleDeep(), 4.5);
    // A clip's name on its body as painted: the accent's brighter top and its fill, each over the canvas.
    for (const auto& [accentName, accent] : accents)
    {
        const juce::Colour top = Color::timelineCanvas().overlaidWith (
            accent.brighter (Tone::timelineCanvasClipSurfaceTopBrightness).withAlpha (Tone::timelineCanvasClipSurfaceTopAlpha));
        const juce::Colour body = Color::timelineCanvas().overlaidWith (accent.withAlpha (Tone::timelineCanvasClipFillAlpha));
        add ("clip name on the top of a " + accentName + " clip", Color::text(), top, 4.5);
        add ("clip name on the body of a " + accentName + " clip", Color::text(), body, 4.5);
    }

    // Indicators: 3:1 against the surfaces they sit on.
    for (const auto& [name, surface] : darkSurfaces)
    {
        add ("focusRing on " + name, Color::focusRing(), surface, 3.0);
        add ("accentPurpleGlow on " + name, Color::accentPurpleGlow(), surface, 3.0);
        add ("midiInLampLit on " + name, Color::midiInLampLit(), surface, 3.0);
        add ("recordArm on " + name, Color::recordArm(), surface, 3.0);
        add ("soloActive on " + name, Color::soloActive(), surface, 3.0);
    }
    for (const auto& [name, colour] : std::vector<std::pair<std::string, juce::Colour>> {
             { "meterGreen", Color::meterGreen() }, { "meterYellow", Color::meterYellow() }, { "dangerRed", Color::dangerRed() } })
        add (name + " on meterTrack", colour, Color::meterTrack(), 3.0);
    add ("scaleTick on controlInsetDeep", Color::scaleTick(), Color::controlInsetDeep(), 3.0);
    add ("scaleTick on mixerBack", Color::scaleTick(), Color::mixerBack(), 3.0);
    add ("faderThumb on controlInsetDeep", Color::faderThumb(), Color::controlInsetDeep(), 3.0);
    add ("faderThumbTop on controlInsetDeep", Color::faderThumbTop(), Color::controlInsetDeep(), 3.0);
    add ("white (the playhead) on timelineCanvas", Color::white(), Color::timelineCanvas(), 3.0);

    for (const Pair& pair : pairs)
    {
        INFO (pair.what << ": " << contrast (pair.foreground, pair.background) << " (needs " << pair.minimum << ")");
        REQUIRE (contrast (pair.foreground, pair.background) >= pair.minimum);
    }
}

TEST_CASE ("ADR-0063 text is drawn in opaque tokens: no withAlpha on a text token in src/ui", "[tokens]")
{
    const std::regex alphaText (
        R"((Color::(text|mutedText|buttonTextMuted|soloActiveText|pianoWhiteKeyText|pianoWhiteKeyMutedText|textOnFill)\(\)|\bkText\b|\bkMutedText\b)\s*\.withAlpha)");
    std::vector<std::string> findings;
    for (const auto& entry : std::filesystem::recursive_directory_iterator (std::filesystem::path { YESDAW_SOURCE_DIR } / "src" / "ui"))
    {
        if (! entry.is_regular_file())
            continue;
        const std::string extension = entry.path().extension().string();
        if (extension != ".h" && extension != ".cpp")
            continue;
        std::ifstream in (entry.path(), std::ios::binary);
        std::string line;
        int number = 0;
        while (std::getline (in, line))
        {
            ++number;
            if (std::regex_search (line, alphaText))
                findings.push_back (entry.path().filename().string() + ":" + std::to_string (number) + ": " + line);
        }
    }
    for (const std::string& finding : findings)
        UNSCOPED_INFO (finding);
    REQUIRE (findings.empty());
}
