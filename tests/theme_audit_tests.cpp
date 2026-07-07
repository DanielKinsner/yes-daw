#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#ifndef YESDAW_SOURCE_DIR
#error "YESDAW_SOURCE_DIR must point at the repository root"
#endif

namespace {

struct ThemeAuditFinding
{
    std::filesystem::path path;
    int line = 0;
    std::string text;
};

bool isUiSourceFile (const std::filesystem::path& path)
{
    const std::string ext = path.extension().string();
    return ext == ".h" || ext == ".hpp" || ext == ".cpp" || ext == ".mm";
}

bool isThemeDefinitionFile (const std::filesystem::path& path)
{
    return path.filename() == "UiTheme.h"
        || path.filename() == "UiThemeLayout.h";
}

std::string trimCopy (std::string_view text)
{
    while (! text.empty() && std::isspace (static_cast<unsigned char> (text.front())) != 0)
        text.remove_prefix (1);

    while (! text.empty() && std::isspace (static_cast<unsigned char> (text.back())) != 0)
        text.remove_suffix (1);

    return std::string { text };
}

bool isRawNumericToken (const std::string& text)
{
    static const std::regex rawNumeric { R"(^[0-9]+(?:\.[0-9]+)?f?$)" };
    return std::regex_match (trimCopy (text), rawNumeric);
}

std::vector<std::string> splitTopLevelArgs (std::string_view args)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    int depth = 0;

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        const char c = args[i];
        if (c == '(' || c == '{' || c == '[')
            ++depth;
        else if (c == ')' || c == '}' || c == ']')
            --depth;
        else if (c == ',' && depth == 0)
        {
            result.push_back (trimCopy (args.substr (start, i - start)));
            start = i + 1;
        }
    }

    result.push_back (trimCopy (args.substr (start)));
    return result;
}

bool roundedRectangleUsesRawRadius (std::string_view statement)
{
    const auto fillPos = statement.find ("fillRoundedRectangle");
    const auto drawPos = statement.find ("drawRoundedRectangle");
    const bool isFill = fillPos != std::string_view::npos
                     && (drawPos == std::string_view::npos || fillPos < drawPos);
    const std::size_t functionPos = isFill ? fillPos : drawPos;
    if (functionPos == std::string_view::npos)
        return false;

    const auto open = statement.find ('(', functionPos);
    const auto close = statement.rfind (')');
    if (open == std::string_view::npos || close == std::string_view::npos || close <= open)
        return false;

    const auto args = splitTopLevelArgs (statement.substr (open + 1, close - open - 1));
    if (isFill)
    {
        if (args.size() != 2u && args.size() != 5u)
            return false;

        return isRawNumericToken (args.back());
    }

    if (args.size() != 3u && args.size() != 6u)
        return false;

    return isRawNumericToken (args[args.size() - 2u]);
}

bool inspectorControlLayoutUsesRawSpacing (std::string_view line)
{
    static const std::regex rawInspectorSpacing {
        R"(\b(?:removeFromTop|reduce|withTrimmedTop|withHeight|withTrimmedLeft|reduced)\s*\([^;\n]*\b[0-9]+)"
    };
    return std::regex_search (line.begin(), line.end(), rawInspectorSpacing);
}

bool inspectorPanelLayoutUsesRawGeometry (std::string_view line)
{
    static const std::regex rawInspectorGeometry {
        R"(\b(?:removeFromTop|removeFromLeft|reduce|withY|withHeight|withWidth|withTrimmedLeft|withTrimmedTop|reduced|fillRoundedRectangle|drawFittedText)\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawInspectorGeometry);
}

bool inspectorPaintDefaultsUseRawGeometry (std::string_view line)
{
    static const std::regex rawInspectorPaintDefault {
        R"(\b(?:gainValue|sampleRate|fade(?:In|Out)Seconds)\s*=.*\?\s*[^:]+:\s*-?[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawInspectorPaintDefault);
}

bool mixerControlLayoutUsesRawSpacing (std::string_view line)
{
    static const std::regex rawMixerSpacing {
        R"(\b(?:removeFromTop|removeFromLeft|withWidth|jmax|reduced)\s*\([^;\n]*\b[0-9]+)"
    };
    return std::regex_search (line.begin(), line.end(), rawMixerSpacing);
}

bool mixerPanelLayoutUsesRawGeometry (std::string_view line)
{
    static const std::regex rawMixerGeometry {
        R"(\b(?:removeFromLeft|withTrimmedTop|withTrimmedBottom|withHeight|withWidth|reduced|jmax|fillRect|fillEllipse|drawEllipse|Rectangle<int>)\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b|\bgetCentreX\s*\(\)\s*-\s*[0-9]+\b|\bgetY\s*\(\)\s*\+\s*[0-9]+\b|\bgetWidth\s*\(\)\s*/\s*\([^;\n]*\+\s*[0-9]+\s*\)|\bgetBottom\s*\(\)[^;\n]*-\s*[0-9]+\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawMixerGeometry);
}

bool trackListLayoutUsesRawSpacing (std::string_view line)
{
    static const std::regex rawTrackListSpacing {
        R"(\b(?:jmax|removeFromTop|removeFromBottom|removeFromLeft|removeFromRight|withTrimmedLeft|withTrimmedTop|withWidth|withRight|withHeight|reduced|translated)\s*\([^;\n]*\b[0-9]+)"
    };
    return std::regex_search (line.begin(), line.end(), rawTrackListSpacing);
}

bool meterLayoutUsesRawSpacing (std::string_view line)
{
    static const std::regex rawMeterSpacing {
        R"(\breduced\s*\(\s*[0-9]+\s*\))"
    };
    return std::regex_search (line.begin(), line.end(), rawMeterSpacing);
}

bool headerLayoutUsesRawGeometry (std::string_view line)
{
    static const std::regex rawHeaderGeometry {
        R"(\b(?:Rectangle<int>|fillRect|fillEllipse|reduced|removeFromTop|removeFromLeft|withWidth)\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b|\bdrawText\s*\([^;\n]*,\s*[0-9]+(?:\.[0-9]+)?f?\b|\bint\s+menuX\s*=\s*[0-9]+\b|\bmenuX\s*\+=\s*[0-9]+\b|\bgetWidth\s*\(\)\s*-\s*[0-9]+\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawHeaderGeometry);
}

bool pianoRollLayoutUsesRawGeometry (std::string_view line)
{
    static const std::regex rawPianoRollGeometry {
        R"(\b(?:removeFromTop|removeFromBottom|removeFromLeft|reduce|reduced|jmax|Rectangle<int>|fillRect|fillEllipse|PathStrokeType|expanded)\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b|\bgetHeight\s*\(\)\s*-\s*[0-9]+\b|\bgetBottom\s*\(\)\s*-\s*[0-9]+\b|\btick\s*\+=\s*[0-9]+\b|\btick\s*%\s*[0-9]+\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawPianoRollGeometry);
}

bool resizedLayoutUsesRawButtonGeometry (std::string_view line)
{
    static const std::regex rawResizedButtonGeometry {
        R"(\bsetBounds\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b|\bgetHeight\s*\(\)\s*-\s*k[A-Za-z0-9_]+\s*\+\s*[0-9]+\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawResizedButtonGeometry);
}

bool shellPaintUsesRawGeometry (std::string_view line)
{
    static const std::regex rawShellPaintGeometry {
        R"(\bremoveFromBottom\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\s*\))"
    };
    return std::regex_search (line.begin(), line.end(), rawShellPaintGeometry);
}

bool timelineStateUsesRawGeometry (std::string_view line)
{
    static const std::regex rawTimelineStateGeometry {
        R"(\bjuce::jmax\s*\(\s*[0-9]+\b|\bgetWidth\s*\(\)\s*-\s*[0-9]+\b|\b(?:state\.(?:totalSeconds|playheadSeconds)|state\.viewport\.scrollSeconds|timelineTotalSeconds)\s*=\s*[0-9]+(?:\.[0-9]+)?f?\b|\bstd::max\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\s*,\s*state\.totalSeconds\b|\bendSeconds\s*\*\s*[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawTimelineStateGeometry);
}

bool panelChromeUsesRawGeometry (std::string_view line)
{
    static const std::regex rawPanelChromeGeometry {
        R"(\breduced\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\s*\)|\bdrawRoundedRectangle\s*\([^;\n]*,\s*[A-Za-z_][A-Za-z0-9_:]*\s*,\s*[0-9]+(?:\.[0-9]+)?f?\s*\))"
    };
    return std::regex_search (line.begin(), line.end(), rawPanelChromeGeometry);
}

bool componentWindowUsesRawGeometry (std::string_view line)
{
    static const std::regex rawWindowGeometry {
        R"(\bsetSize\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawWindowGeometry);
}

bool timelineClipGainGestureUsesRawGeometry (std::string_view line)
{
    static const std::regex rawGainGestureGeometry {
        R"(\bconstexpr\s+float\s+k(?:GainPerPixel|MaxGestureGain)\s*=\s*[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawGainGestureGeometry);
}

bool timelineCoordinateConversionUsesRawGeometry (std::string_view line)
{
    static const std::regex rawCoordinateConversionGeometry {
        R"(\bstd::max\s*\(\s*(?:0\.0|1\.0)\s*,\s*(?:geometry\.viewport\.pixelsPerSecond|seconds|drag\.startSeconds))"
    };
    return std::regex_search (line.begin(), line.end(), rawCoordinateConversionGeometry);
}

bool timelineSnapGridUsesRawDefault (std::string_view line)
{
    static const std::regex rawTimelineSnapGrid {
        R"(\bconstexpr\s+(?:[A-Za-z_:][A-Za-z0-9_:]*\s+)*kTimelineSnapGridTicks\s*=\s*[0-9]+\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawTimelineSnapGrid);
}

bool sliderTextBoxUsesRawGeometry (std::string_view line)
{
    static const std::regex rawSliderTextBoxGeometry {
        R"(\bsetTextBoxStyle\s*\([^;\n]*,\s*[0-9]+\s*,\s*[0-9]+\s*\))"
    };
    return std::regex_search (line.begin(), line.end(), rawSliderTextBoxGeometry);
}

bool inspectorFadeSliderDefaultsUseRawGeometry (std::string_view line)
{
    static const std::regex rawFadeSliderDefault {
        R"(\bslider\.set(?:Range|Value)\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawFadeSliderDefault);
}

bool inspectorGainSliderDefaultsUseRawGeometry (std::string_view line)
{
    static const std::regex rawGainSliderDefault {
        R"(\binspectorGain\.set(?:Range|Value)\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawGainSliderDefault);
}

bool mixerSliderDefaultsUseRawGeometry (std::string_view line)
{
    static const std::regex rawMixerSliderDefault {
        R"(\bmixer(?:Fader|Pan)\.set(?:Range|Value)\s*\([^;\n]*\b-?[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawMixerSliderDefault);
}

bool timelineCanvasToolbarUsesRawGeometry (std::string_view line)
{
    static const std::regex rawToolbarGeometry {
        R"(\b(?:withTrimmedLeft|withWidth|removeFromLeft)\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b|\breduced\s*\([^;\n]*(?:\b[1-9][0-9]*(?:\.[0-9]+)?f?\b|\+\s*[0-9]+(?:\.[0-9]+)?f?\b))"
    };
    return std::regex_search (line.begin(), line.end(), rawToolbarGeometry);
}

bool timelineCanvasOutlineUsesRawGeometry (std::string_view line)
{
    static const std::regex rawOutlineGeometry {
        R"(\breduced\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\s*\)|\bdrawRoundedRectangle\s*\([^;\n]*,\s*UiTheme::Radius::[A-Za-z0-9_]+,\s*[0-9]+(?:\.[0-9]+)?f?\s*\))"
    };
    return std::regex_search (line.begin(), line.end(), rawOutlineGeometry);
}

bool timelineCanvasGeometryUsesRawGeometry (std::string_view line)
{
    static const std::regex rawCanvasGeometry {
        R"(\b(?:removeFromTop|reduced)\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b|\blaneHeight\s*=\s*std::max\s*\(\s*[0-9]+\b|\blaneCount\s*=\s*std::max\s*\(\s*[0-9]+\b|\bpixelsPerSecond\s*=\s*std::max\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawCanvasGeometry);
}

bool timelineCanvasClipPaintUsesRawGeometry (std::string_view line)
{
    static const std::regex rawClipPaintGeometry {
        R"(\barea\.reduce\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b|\bstd::clamp\s*\([^;\n]*\b[0-9]+(?:\.[0-9]+)?f?\b|\bstd::max\s*\(\s*[0-9]+\b|\barea\.getWidth\s*\(\)\s*/\s*[0-9]+\b|\b(?:clipId|x)\s*\*\s*[0-9]+\b|&\s*[0-9]+\b|\bstatic_cast<float>\s*\(\s*phase\s*\)\s*/\s*[0-9]+(?:\.[0-9]+)?f?\b|\bscaled\s*=\s*[0-9]+(?:\.[0-9]+)?f?\s*\+|\*\s*[0-9]+(?:\.[0-9]+)?f?\s*;\s*$|\barea\.get(?:Width|Height)\s*\(\)\s*<=\s*[0-9]+\b|\bwithHeight\s*\(\s*[0-9]+\s*\))"
    };
    return std::regex_search (line.begin(), line.end(), rawClipPaintGeometry);
}

bool timelineCanvasRulerUsesRawGeometry (std::string_view line)
{
    static const std::regex rawRulerGeometry {
        R"(\bwithHeight\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\b|\bwithY\s*\([^;\n]*-\s*[0-9]+(?:\.[0-9]+)?f?\b|\bpixelsPerSecond\s*<\s*[0-9]+(?:\.[0-9]+)?f?\b|\?\s*[0-9]+(?:\.[0-9]+)?f?\s*:|\bclipArea\.get(?:X|Right)\s*\(\)\s*[-+]\s*[0-9]+(?:\.[0-9]+)?f?\b|\bg\.drawText\s*\([^;\n]*,\s*x\s*[-+]\s*[0-9]+(?:\.[0-9]+)?f?\b|\bruler\.getY\s*\(\)\s*\+\s*[0-9]+(?:\.[0-9]+)?f?\b|\bg\.fillRect\s*\([^;\n]*,\s*ruler\.getBottom\s*\(\)\s*-\s*[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawRulerGeometry);
}

bool timelineCanvasGridUsesRawGeometry (std::string_view line)
{
    static const std::regex rawGridGeometry {
        R"(\bstd::max\s*\(\s*[0-9]+\s*,\s*state\.trackCount\b|\bg\.fillRect\s*\([^;\n]*,\s*[0-9]+(?:\.[0-9]+)?f?\s*\)|\by\s*\+\s*[0-9]+(?:\.[0-9]+)?f?\b|\blaneHeight\s*-\s*[0-9]+(?:\.[0-9]+)?f?\b|\bstd::floor\s*\([^;\n]*/\s*[0-9]+(?:\.[0-9]+)?f?\b|\brightSeconds\s*\+\s*[0-9]+(?:\.[0-9]+)?f?\b|\bseconds\s*\+=\s*[0-9]+(?:\.[0-9]+)?f?\b|%\s*[0-9]+\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawGridGeometry);
}

bool timelineCanvasPlayheadUsesRawGeometry (std::string_view line)
{
    static const std::regex rawPlayheadGeometry {
        R"(\bg\.fillRect\s*\([^;\n]*,\s*[0-9]+(?:\.[0-9]+)?f?\s*,|\bplayheadX\s*-\s*[0-9]+(?:\.[0-9]+)?f?\b|\bruler\.getY\s*\(\)\s*\+\s*[0-9]+(?:\.[0-9]+)?f?\b|\bstatic_cast<float>\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\s*\)|\bg\.drawText\s*\([^;\n]*,\s*playheadX\s*-\s*[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawPlayheadGeometry);
}

bool timelineCanvasStateDefaultsUseRawGeometry (std::string_view line)
{
    static const std::regex rawStateDefaults {
        R"(\b(?:totalSeconds|playheadSeconds)\s*=\s*[0-9]+(?:\.[0-9]+)?f?\b)"
    };
    return std::regex_search (line.begin(), line.end(), rawStateDefaults);
}

bool timelineCanvasPaintToneUsesRawDefault (std::string_view line)
{
    static const std::regex rawPaintTone {
        R"(\bTimelineCanvasClipStyle\s+fallback\s*\{[^;\n]*,\s*[0-9]+(?:\.[0-9]+)?f?\s*\}|\b(?:withAlpha|brighter)\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\s*\))"
    };
    return std::regex_search (line.begin(), line.end(), rawPaintTone);
}

bool mainComponentTimelineClipStyleUsesRawTone (std::string_view line)
{
    static const std::regex rawClipStyleTone {
        R"(\btimelineClipStyles\.push_back\s*\(\s*\{[^;\n]*,\s*[0-9]+(?:\.[0-9]+)?f?\s*\})"
    };
    return std::regex_search (line.begin(), line.end(), rawClipStyleTone);
}

bool mainComponentDemoClipStyleUsesRawTone (std::string_view line)
{
    static const std::regex rawDemoClipStyleTone {
        R"(\{\s*k[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+\s*\([^;\n]*\))?\s*,\s*[0-9]+(?:\.[0-9]+)?f?\s*\})"
    };
    return std::regex_search (line.begin(), line.end(), rawDemoClipStyleTone);
}

bool mainComponentDemoClipPlacementUsesRawDefault (std::string_view line)
{
    static const std::regex rawDemoClipPlacement {
        R"(\{\s*[0-9]+\s*,\s*[0-9]+\s*,\s*[0-9]+(?:\.[0-9]+)?f?\s*,\s*[0-9]+(?:\.[0-9]+)?f?\s*\})"
    };
    return std::regex_search (line.begin(), line.end(), rawDemoClipPlacement);
}

bool timelineLayoutHitTestUsesRawGeometry (std::string_view line)
{
    static const std::regex rawHitTestGeometry {
        R"(\b(?:pixelsPerSecond|widthPixels|laneHeightPixels)\s*=\s*[0-9]+(?:\.[0-9]+)?f?\b|\b(?:pixelsPerSecond|widthPixels|laneHeightPixels|xPixels|yPixels|lengthSeconds)\s*(?:<=|<)\s*[0-9]+(?:\.[0-9]+)?f?\b|\bxPx\s*=\s*[0-9]+(?:\.[0-9]+)?f?\b|\bstd::max\s*\(\s*wPx\s*,\s*[0-9]+(?:\.[0-9]+)?f?\s*\))"
    };
    return std::regex_search (line.begin(), line.end(), rawHitTestGeometry);
}

std::vector<ThemeAuditFinding> auditThemeTokens (const std::filesystem::path& root)
{
    const std::regex rawHexColour { R"(\b0xff[0-9A-Fa-f]{6}\b)" };
    const std::regex juceNamedColour { R"(\bjuce::Colours::[A-Za-z_][A-Za-z0-9_]*)" };
    const std::regex rawFontSize { R"(\bFontOptions\s*\(\s*[0-9]+(?:\.[0-9]+)?f?\b)" };
    const std::regex rawLayoutSize { R"(\bconstexpr\s+int\s+k[A-Za-z0-9_]*(?:Width|Height)\s*=\s*[0-9]+\b)" };
    const std::regex rawEdgeHitGeometry { R"(\bconstexpr\s+int\s+k[A-Za-z0-9_]*EdgePixels\s*=\s*[0-9]+\b)" };
    const std::regex rawMeterBand { R"(\bremoveFrom(?:Top|Right)\s*\([^;\n]*\*\s*0(?:\.[0-9]+)?f?\s*\))" };
    const std::regex rawShellSpacing { R"(\b(?:work(?:\.[A-Za-z0-9_]+\s*\([^;\n]*\))*|mixer)\.reduced\s*\(\s*[0-9]+\s*,\s*[0-9]+\s*\))" };
    const std::regex rawInputDragThreshold { R"(\bstd::abs\s*\(\s*delta[XY]\s*\)\s*<\s*[0-9]+\b)" };
    const std::regex rawPianoRollKeyRange { R"(\bconstexpr\s+int\s+kPianoRoll(?:Low|High)Key\s*=\s*[0-9]+\b)" };
    const std::regex rawTimelineCanvasCapacity { R"(\bconstexpr\s+int\s+kVisibleClipCapacity\s*=\s*[0-9]+\b)" };
    const std::regex rawTimelineTotalMemberDefault { R"(\bdouble\s+timelineTotalSeconds\s*=\s*[0-9]+(?:\.[0-9]+)?f?\s*;)" };
    std::vector<ThemeAuditFinding> findings;

    for (const auto& entry : std::filesystem::recursive_directory_iterator (root))
    {
        if (! entry.is_regular_file() || ! isUiSourceFile (entry.path()) || isThemeDefinitionFile (entry.path()))
            continue;

        std::ifstream in (entry.path());
        REQUIRE (in.is_open());

        std::string line;
        int lineNumber = 0;
        std::string roundedStatement;
        int roundedStatementLine = 0;
        bool insideInspectorControlLayout = false;
        int inspectorControlLayoutDepth = 0;
        bool insideMixerControlLayout = false;
        int mixerControlLayoutDepth = 0;
        bool insideTrackListLayout = false;
        int trackListLayoutDepth = 0;
        bool insideMeterLayout = false;
        int meterLayoutDepth = 0;
        bool insideHeaderLayout = false;
        int headerLayoutDepth = 0;
        bool insidePianoRollLayout = false;
        int pianoRollLayoutDepth = 0;
        bool insideInspectorPanelLayout = false;
        int inspectorPanelLayoutDepth = 0;
        bool insideMixerPanelLayout = false;
        int mixerPanelLayoutDepth = 0;
        bool insideResizedLayout = false;
        int resizedLayoutDepth = 0;
        bool insideShellPaintLayout = false;
        int shellPaintLayoutDepth = 0;
        bool insideTimelineStateLayout = false;
        int timelineStateLayoutDepth = 0;
        bool insidePanelChromeLayout = false;
        int panelChromeLayoutDepth = 0;
        bool insideTimelineClipGainGesture = false;
        int timelineClipGainGestureDepth = 0;
        bool insideTimelineCoordinateConversion = false;
        int timelineCoordinateConversionDepth = 0;
        bool insideInspectorFadeSliderDefaults = false;
        int inspectorFadeSliderDefaultsDepth = 0;
        bool insideInspectorGainSliderDefaults = false;
        int inspectorGainSliderDefaultsDepth = 0;
        bool insideTimelineCanvasToolbar = false;
        int timelineCanvasToolbarDepth = 0;
        bool insideTimelineCanvasOutline = false;
        int timelineCanvasOutlineDepth = 0;
        bool insideTimelineCanvasGeometry = false;
        int timelineCanvasGeometryDepth = 0;
        bool insideTimelineCanvasClipPaint = false;
        int timelineCanvasClipPaintDepth = 0;
        bool insideTimelineCanvasRuler = false;
        int timelineCanvasRulerDepth = 0;
        bool insideTimelineCanvasGrid = false;
        int timelineCanvasGridDepth = 0;
        bool insideTimelineCanvasPlayhead = false;
        int timelineCanvasPlayheadDepth = 0;
        bool insideTimelineCanvasStateDefaults = false;
        int timelineCanvasStateDefaultsDepth = 0;
        bool insideTimelineCanvasPaintTone = false;
        int timelineCanvasPaintToneDepth = 0;
        bool insideMainComponentTimelineClipStyle = false;
        int mainComponentTimelineClipStyleDepth = 0;
        bool insideMainComponentDemoClipStyle = false;
        int mainComponentDemoClipStyleDepth = 0;
        bool insideMainComponentDemoClipPlacement = false;
        int mainComponentDemoClipPlacementDepth = 0;
        bool insideTimelineLayoutHitTest = false;
        int timelineLayoutHitTestDepth = 0;
        while (std::getline (in, line))
        {
            ++lineNumber;
            if (std::regex_search (line, rawHexColour)
                || std::regex_search (line, juceNamedColour)
                || std::regex_search (line, rawFontSize)
                || std::regex_search (line, rawLayoutSize)
                || std::regex_search (line, rawEdgeHitGeometry)
                || std::regex_search (line, rawMeterBand)
                || std::regex_search (line, rawShellSpacing)
                || std::regex_search (line, rawInputDragThreshold)
                || std::regex_search (line, rawPianoRollKeyRange)
                || std::regex_search (line, rawTimelineCanvasCapacity)
                || std::regex_search (line, rawTimelineTotalMemberDefault)
                || componentWindowUsesRawGeometry (line)
                || timelineSnapGridUsesRawDefault (line)
                || sliderTextBoxUsesRawGeometry (line)
                || mixerSliderDefaultsUseRawGeometry (line))
            {
                findings.push_back ({ entry.path(), lineNumber, line });
            }

            if (line.find ("layoutInspectorControls") != std::string::npos)
            {
                insideInspectorControlLayout = true;
                inspectorControlLayoutDepth = 0;
            }

            if (insideInspectorControlLayout && inspectorControlLayoutUsesRawSpacing (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("layoutMixerControls") != std::string::npos)
            {
                insideMixerControlLayout = true;
                mixerControlLayoutDepth = 0;
            }

            if (insideMixerControlLayout && mixerControlLayoutUsesRawSpacing (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("void drawMixer") != std::string::npos)
            {
                insideMixerPanelLayout = true;
                mixerPanelLayoutDepth = 0;
            }

            if (insideMixerPanelLayout && mixerPanelLayoutUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("drawTrackList") != std::string::npos)
            {
                insideTrackListLayout = true;
                trackListLayoutDepth = 0;
            }

            if (insideTrackListLayout && trackListLayoutUsesRawSpacing (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("void drawMeter") != std::string::npos
                || line.find ("void drawHorizontalMeter") != std::string::npos)
            {
                insideMeterLayout = true;
                meterLayoutDepth = 0;
            }

            if (insideMeterLayout && meterLayoutUsesRawSpacing (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("void drawHeader") != std::string::npos
                || line.find ("void drawTransportReadouts") != std::string::npos
                || line.find ("void drawMasterMeter") != std::string::npos)
            {
                insideHeaderLayout = true;
                headerLayoutDepth = 0;
            }

            if (insideHeaderLayout && headerLayoutUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("pianoRollCanvasGeometry") != std::string::npos
                || line.find ("pianoRollNoteBounds") != std::string::npos
                || line.find ("void drawPianoRoll") != std::string::npos)
            {
                insidePianoRollLayout = true;
                pianoRollLayoutDepth = 0;
            }

            if (insidePianoRollLayout && pianoRollLayoutUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("void resized") != std::string::npos)
            {
                insideResizedLayout = true;
                resizedLayoutDepth = 0;
            }

            if (insideResizedLayout && resizedLayoutUsesRawButtonGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "MainComponent.cpp"
                && line.find ("void paint") != std::string::npos)
            {
                insideShellPaintLayout = true;
                shellPaintLayoutDepth = 0;
            }

            if (insideShellPaintLayout && shellPaintUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("makeTimelineState") != std::string::npos
                || line.find ("rebuildTimelineClipViews") != std::string::npos)
            {
                insideTimelineStateLayout = true;
                timelineStateLayoutDepth = 0;
            }

            if (insideTimelineStateLayout && timelineStateUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("adjustTimelineClipGainByLayoutId") != std::string::npos)
            {
                insideTimelineClipGainGesture = true;
                timelineClipGainGestureDepth = 0;
            }

            if (insideTimelineClipGainGesture && timelineClipGainGestureUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("void mouseDrag") != std::string::npos
                || line.find ("timelineSecondsAt") != std::string::npos
                || line.find ("dragModeForPointer") != std::string::npos)
            {
                insideTimelineCoordinateConversion = true;
                timelineCoordinateConversionDepth = 0;
            }

            if (insideTimelineCoordinateConversion && timelineCoordinateConversionUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("configureInspectorFadeSlider") != std::string::npos)
            {
                insideInspectorFadeSliderDefaults = true;
                inspectorFadeSliderDefaultsDepth = 0;
            }

            if (insideInspectorFadeSliderDefaults && inspectorFadeSliderDefaultsUseRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("configureInspectorControls") != std::string::npos
                || line.find ("refreshInspectorControls") != std::string::npos)
            {
                insideInspectorGainSliderDefaults = true;
                inspectorGainSliderDefaultsDepth = 0;
            }

            if (insideInspectorGainSliderDefaults && inspectorGainSliderDefaultsUseRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("void drawToolbar") != std::string::npos)
            {
                insideTimelineCanvasToolbar = true;
                timelineCanvasToolbarDepth = 0;
            }

            if (insideTimelineCanvasToolbar && timelineCanvasToolbarUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineCanvas.h"
                && (line.find ("void fillPanel") != std::string::npos
                    || line.find ("void drawClip (") != std::string::npos
                    || line.find ("void drawClip(") != std::string::npos))
            {
                insideTimelineCanvasOutline = true;
                timelineCanvasOutlineDepth = 0;
            }

            if (insideTimelineCanvasOutline && timelineCanvasOutlineUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineCanvas.h"
                && line.find ("timelineCanvasGeometry") != std::string::npos)
            {
                insideTimelineCanvasGeometry = true;
                timelineCanvasGeometryDepth = 0;
            }

            if (insideTimelineCanvasGeometry && timelineCanvasGeometryUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineCanvas.h"
                && (line.find ("void drawClipWaveform") != std::string::npos
                    || line.find ("void drawClip (") != std::string::npos
                    || line.find ("void drawClip(") != std::string::npos))
            {
                insideTimelineCanvasClipPaint = true;
                timelineCanvasClipPaintDepth = 0;
            }

            if (insideTimelineCanvasClipPaint && timelineCanvasClipPaintUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineCanvas.h"
                && line.find ("void drawRuler") != std::string::npos)
            {
                insideTimelineCanvasRuler = true;
                timelineCanvasRulerDepth = 0;
            }

            if (insideTimelineCanvasRuler && timelineCanvasRulerUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineCanvas.h"
                && line.find ("void drawGrid") != std::string::npos)
            {
                insideTimelineCanvasGrid = true;
                timelineCanvasGridDepth = 0;
            }

            if (insideTimelineCanvasGrid && timelineCanvasGridUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineCanvas.h"
                && line.find ("void drawPlayhead") != std::string::npos)
            {
                insideTimelineCanvasPlayhead = true;
                timelineCanvasPlayheadDepth = 0;
            }

            if (insideTimelineCanvasPlayhead && timelineCanvasPlayheadUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineCanvas.h"
                && line.find ("struct TimelineCanvasState") != std::string::npos)
            {
                insideTimelineCanvasStateDefaults = true;
                timelineCanvasStateDefaultsDepth = 0;
            }

            if (insideTimelineCanvasStateDefaults && timelineCanvasStateDefaultsUseRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineCanvas.h"
                && (line.find ("styleForClip") != std::string::npos
                    || line.find ("void drawClipWaveform") != std::string::npos
                    || line.find ("void drawClip (") != std::string::npos
                    || line.find ("void drawClip(") != std::string::npos
                    || line.find ("void drawRuler") != std::string::npos
                    || line.find ("void drawGrid") != std::string::npos))
            {
                insideTimelineCanvasPaintTone = true;
                timelineCanvasPaintToneDepth = 0;
            }

            if (insideTimelineCanvasPaintTone && timelineCanvasPaintToneUsesRawDefault (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "MainComponent.cpp"
                && line.find ("rebuildTimelineClipViews") != std::string::npos)
            {
                insideMainComponentTimelineClipStyle = true;
                mainComponentTimelineClipStyleDepth = 0;
            }

            if (insideMainComponentTimelineClipStyle && mainComponentTimelineClipStyleUsesRawTone (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "MainComponent.cpp"
                && line.find ("kClipStyles") != std::string::npos)
            {
                insideMainComponentDemoClipStyle = true;
                mainComponentDemoClipStyleDepth = 0;
            }

            if (insideMainComponentDemoClipStyle && mainComponentDemoClipStyleUsesRawTone (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "MainComponent.cpp"
                && line.find ("kClips") != std::string::npos)
            {
                insideMainComponentDemoClipPlacement = true;
                mainComponentDemoClipPlacementDepth = 0;
            }

            if (insideMainComponentDemoClipPlacement && mainComponentDemoClipPlacementUsesRawDefault (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "TimelineLayout.h"
                && (line.find ("struct Viewport") != std::string::npos
                    || line.find ("hitTestVisibleClip") != std::string::npos))
            {
                insideTimelineLayoutHitTest = true;
                timelineLayoutHitTestDepth = 0;
            }

            if (insideTimelineLayoutHitTest && timelineLayoutHitTestUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (entry.path().filename() == "MainComponent.cpp"
                && line.find ("void fillPanel") != std::string::npos)
            {
                insidePanelChromeLayout = true;
                panelChromeLayoutDepth = 0;
            }

            if (insidePanelChromeLayout && panelChromeUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (line.find ("void drawInspector") != std::string::npos)
            {
                insideInspectorPanelLayout = true;
                inspectorPanelLayoutDepth = 0;
            }

            if (insideInspectorPanelLayout && inspectorPanelLayoutUsesRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (insideInspectorPanelLayout && inspectorPaintDefaultsUseRawGeometry (line))
                findings.push_back ({ entry.path(), lineNumber, line });

            if (insideInspectorControlLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++inspectorControlLayoutDepth;
                    else if (c == '}')
                        --inspectorControlLayoutDepth;
                }

                if (inspectorControlLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideInspectorControlLayout = false;
            }

            if (insideMixerControlLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++mixerControlLayoutDepth;
                    else if (c == '}')
                        --mixerControlLayoutDepth;
                }

                if (mixerControlLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideMixerControlLayout = false;
            }

            if (insideTrackListLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++trackListLayoutDepth;
                    else if (c == '}')
                        --trackListLayoutDepth;
                }

                if (trackListLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTrackListLayout = false;
            }

            if (insideMixerPanelLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++mixerPanelLayoutDepth;
                    else if (c == '}')
                        --mixerPanelLayoutDepth;
                }

                if (mixerPanelLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideMixerPanelLayout = false;
            }

            if (insideMeterLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++meterLayoutDepth;
                    else if (c == '}')
                        --meterLayoutDepth;
                }

                if (meterLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideMeterLayout = false;
            }

            if (insideHeaderLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++headerLayoutDepth;
                    else if (c == '}')
                        --headerLayoutDepth;
                }

                if (headerLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideHeaderLayout = false;
            }

            if (insidePianoRollLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++pianoRollLayoutDepth;
                    else if (c == '}')
                        --pianoRollLayoutDepth;
                }

                if (pianoRollLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insidePianoRollLayout = false;
            }

            if (insideInspectorPanelLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++inspectorPanelLayoutDepth;
                    else if (c == '}')
                        --inspectorPanelLayoutDepth;
                }

                if (inspectorPanelLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideInspectorPanelLayout = false;
            }

            if (insideResizedLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++resizedLayoutDepth;
                    else if (c == '}')
                        --resizedLayoutDepth;
                }

                if (resizedLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideResizedLayout = false;
            }

            if (insideShellPaintLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++shellPaintLayoutDepth;
                    else if (c == '}')
                        --shellPaintLayoutDepth;
                }

                if (shellPaintLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideShellPaintLayout = false;
            }

            if (insideTimelineStateLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineStateLayoutDepth;
                    else if (c == '}')
                        --timelineStateLayoutDepth;
                }

                if (timelineStateLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineStateLayout = false;
            }

            if (insidePanelChromeLayout)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++panelChromeLayoutDepth;
                    else if (c == '}')
                        --panelChromeLayoutDepth;
                }

                if (panelChromeLayoutDepth <= 0 && line.find ('}') != std::string::npos)
                    insidePanelChromeLayout = false;
            }

            if (insideTimelineClipGainGesture)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineClipGainGestureDepth;
                    else if (c == '}')
                        --timelineClipGainGestureDepth;
                }

                if (timelineClipGainGestureDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineClipGainGesture = false;
            }

            if (insideTimelineCoordinateConversion)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCoordinateConversionDepth;
                    else if (c == '}')
                        --timelineCoordinateConversionDepth;
                }

                if (timelineCoordinateConversionDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCoordinateConversion = false;
            }

            if (insideInspectorFadeSliderDefaults)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++inspectorFadeSliderDefaultsDepth;
                    else if (c == '}')
                        --inspectorFadeSliderDefaultsDepth;
                }

                if (inspectorFadeSliderDefaultsDepth <= 0 && line.find ('}') != std::string::npos)
                    insideInspectorFadeSliderDefaults = false;
            }

            if (insideInspectorGainSliderDefaults)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++inspectorGainSliderDefaultsDepth;
                    else if (c == '}')
                        --inspectorGainSliderDefaultsDepth;
                }

                if (inspectorGainSliderDefaultsDepth <= 0 && line.find ('}') != std::string::npos)
                    insideInspectorGainSliderDefaults = false;
            }

            if (insideTimelineCanvasToolbar)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasToolbarDepth;
                    else if (c == '}')
                        --timelineCanvasToolbarDepth;
                }

                if (timelineCanvasToolbarDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasToolbar = false;
            }

            if (insideTimelineCanvasOutline)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasOutlineDepth;
                    else if (c == '}')
                        --timelineCanvasOutlineDepth;
                }

                if (timelineCanvasOutlineDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasOutline = false;
            }

            if (insideTimelineCanvasGeometry)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasGeometryDepth;
                    else if (c == '}')
                        --timelineCanvasGeometryDepth;
                }

                if (timelineCanvasGeometryDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasGeometry = false;
            }

            if (insideTimelineCanvasClipPaint)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasClipPaintDepth;
                    else if (c == '}')
                        --timelineCanvasClipPaintDepth;
                }

                if (timelineCanvasClipPaintDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasClipPaint = false;
            }

            if (insideTimelineCanvasRuler)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasRulerDepth;
                    else if (c == '}')
                        --timelineCanvasRulerDepth;
                }
                if (timelineCanvasRulerDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasRuler = false;
            }

            if (insideTimelineCanvasGrid)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasGridDepth;
                    else if (c == '}')
                        --timelineCanvasGridDepth;
                }
                if (timelineCanvasGridDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasGrid = false;
            }

            if (insideTimelineCanvasPlayhead)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasPlayheadDepth;
                    else if (c == '}')
                        --timelineCanvasPlayheadDepth;
                }
                if (timelineCanvasPlayheadDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasPlayhead = false;
            }

            if (insideTimelineCanvasStateDefaults)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasStateDefaultsDepth;
                    else if (c == '}')
                        --timelineCanvasStateDefaultsDepth;
                }
                if (timelineCanvasStateDefaultsDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasStateDefaults = false;
            }

            if (insideTimelineCanvasPaintTone)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineCanvasPaintToneDepth;
                    else if (c == '}')
                        --timelineCanvasPaintToneDepth;
                }
                if (timelineCanvasPaintToneDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineCanvasPaintTone = false;
            }

            if (insideMainComponentTimelineClipStyle)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++mainComponentTimelineClipStyleDepth;
                    else if (c == '}')
                        --mainComponentTimelineClipStyleDepth;
                }
                if (mainComponentTimelineClipStyleDepth <= 0 && line.find ('}') != std::string::npos)
                    insideMainComponentTimelineClipStyle = false;
            }

            if (insideMainComponentDemoClipStyle)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++mainComponentDemoClipStyleDepth;
                    else if (c == '}')
                        --mainComponentDemoClipStyleDepth;
                }
                if (mainComponentDemoClipStyleDepth <= 0 && line.find ('}') != std::string::npos)
                    insideMainComponentDemoClipStyle = false;
            }

            if (insideMainComponentDemoClipPlacement)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++mainComponentDemoClipPlacementDepth;
                    else if (c == '}')
                        --mainComponentDemoClipPlacementDepth;
                }
                if (mainComponentDemoClipPlacementDepth <= 0 && line.find ('}') != std::string::npos)
                    insideMainComponentDemoClipPlacement = false;
            }

            if (insideTimelineLayoutHitTest)
            {
                for (const char c : line)
                {
                    if (c == '{')
                        ++timelineLayoutHitTestDepth;
                    else if (c == '}')
                        --timelineLayoutHitTestDepth;
                }
                if (timelineLayoutHitTestDepth <= 0 && line.find ('}') != std::string::npos)
                    insideTimelineLayoutHitTest = false;
            }

            if (roundedStatement.empty())
            {
                if (line.find ("fillRoundedRectangle") == std::string::npos
                    && line.find ("drawRoundedRectangle") == std::string::npos)
                {
                    continue;
                }

                roundedStatementLine = lineNumber;
            }

            if (! roundedStatement.empty())
                roundedStatement += ' ';
            roundedStatement += line;

            if (line.find (";") != std::string::npos)
            {
                if (roundedRectangleUsesRawRadius (roundedStatement))
                    findings.push_back ({ entry.path(), roundedStatementLine, roundedStatement });

                roundedStatement.clear();
                roundedStatementLine = 0;
            }
        }
    }

    return findings;
}

} // namespace

TEST_CASE ("H16 theme audit rejects raw UI tokens outside UiTheme", "[ui][theme]")
{
    const auto findings = auditThemeTokens (std::filesystem::path { YESDAW_SOURCE_DIR } / "src" / "ui");
    if (! findings.empty())
        INFO (findings.front().path.string() + ":" + std::to_string (findings.front().line)
              + ": " + findings.front().text);
    REQUIRE (findings.empty());
}

TEST_CASE ("H16 theme audit negative control catches inline raw tokens", "[ui][theme]")
{
    const auto scratch = std::filesystem::temp_directory_path()
                       / "yesdaw-theme-audit-negative-control";
    std::filesystem::remove_all (scratch);
    std::filesystem::create_directories (scratch);

    {
        std::ofstream out (scratch / "MainComponent.cpp");
        REQUIRE (out.is_open());
        out << "void paint() { const auto raw = juce::Colour (0xff112233); }\n";
        out << "void label() { const auto raw = juce::FontOptions (13.0f); }\n";
        out << "void panel(juce::Graphics& g, juce::Rectangle<int> r) { g.fillRoundedRectangle (r.toFloat(), 4.0f); }\n";
        out << "constexpr int kPanelWidth = 42;\n";
        out << "void meter(juce::Rectangle<int> live) { auto hot = live.removeFromTop (juce::roundToInt (live.getHeight() * 0.25f)); }\n";
        out << "void shell(juce::Rectangle<int> work) { auto left = work.removeFromLeft (42).reduced (6, 10); }\n";
        out << "void layoutInspectorControls() { auto area = inspectorBounds(); area.removeFromTop (40); }\n";
        out << "void layoutMixerControls() { auto lane = mixerFirstStripBounds().reduced (8, 6); }\n";
        out << "void drawMixer(juce::Rectangle<int> area) { auto strip = area.removeFromLeft (84).reduced (3, 0); }\n";
        out << "void drawTrackList(juce::Rectangle<int> area) { auto header = area.removeFromTop (38); }\n";
        out << "void drawMeter(juce::Rectangle<int> area) { auto fill = area.reduced (2); }\n";
        out << "void drawHeader() { auto time = juce::Rectangle<int> (570, 16, 190, 56); }\n";
        out << "void drawPianoRoll(juce::Rectangle<int> area) { auto header = area.removeFromTop (38); }\n";
        out << "void drawPianoRollExpressionPoint() { g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f); }\n";
        out << "void drawPianoRollExpressionStroke() { g.strokePath (path, juce::PathStrokeType (1.5f)); }\n";
        out << "void drawInspector(juce::Rectangle<int> area) { auto tabs = area.removeFromTop (40); }\n";
        out << "void drawInspectorDefaults() { const float gainValue = selected ? clip->gain : 1.0f; const double sampleRate = valid ? hz : 48000.0; const double fadeInSeconds = selected ? fadeIn / sampleRate : 0.0; }\n";
        out << "void resized() { button.setBounds (16, 50, 44, 26); }\n";
        out << "void makeTimelineState() { auto width = juce::jmax (1, timelineInput.getWidth() - 26); }\n";
        out << "void fillPanel(juce::Graphics& g, juce::Rectangle<int> area) { g.drawRoundedRectangle (area.toFloat().reduced (0.5f), radius, 1.0f); }\n";
        out << "constexpr int kTrimEdgePixels = 8;\n";
        out << "void mouseUp() { if (std::abs (deltaX) < 2) return; }\n";
        out << "void window() { setSize (1536, 960); }\n";
        out << "constexpr int kPianoRollLowKey = 48;\n";
        out << "constexpr yesdaw::engine::Tick kTimelineSnapGridTicks = 512;\n";
        out << "void drawPianoRollGrid() { for (Tick tick = 0; tick <= len; tick += 512) { if ((tick % 2048) == 0) {} } }\n";
        out << "void adjustTimelineClipGainByLayoutId() {\n";
        out << "constexpr float kGainPerPixel = 0.01f;\n";
        out << "constexpr float kMaxGestureGain = 4.0f;\n";
        out << "}\n";
        out << "void slider(juce::Slider& s) { s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); }\n";
        out << "void configureInspectorFadeSlider(juce::Slider& slider) {\n";
        out << "slider.setRange (0.0, 1.0, 0.001);\n";
        out << "slider.setValue (0.0, juce::dontSendNotification);\n";
        out << "}\n";
        out << "void configureInspectorControls() {\n";
        out << "inspectorGain.setRange (0.0, 2.0, 0.01);\n";
        out << "inspectorGain.setValue (1.0, juce::dontSendNotification);\n";
        out << "}\n";
        out << "void refreshInspectorControls() { inspectorGain.setValue (1.0, juce::dontSendNotification); }\n";
        out << "void drawToolbar(juce::Rectangle<int> toolbar) { auto tools = toolbar.withTrimmedLeft (16).withWidth (190); }\n";
        out << "void makeTimelineStateDefaults() { state.totalSeconds = 98.0; state.playheadSeconds = 32.0; state.viewport.scrollSeconds = 0.0; state.viewport.pixelsPerSecond = width / std::max (1.0, state.totalSeconds); }\n";
        out << "void timelineSecondsAt() {\n";
        out << "const double pixelsPerSecond = std::max (1.0, geometry.viewport.pixelsPerSecond);\n";
        out << "return std::max (0.0, seconds);\n";
        out << "}\n";
        out << "double timelineTotalSeconds = 98.0;\n";
        out << "void configureMixerControls() { mixerFader.setRange (0.0, 2.0, 0.01); mixerPan.setValue (0.0, juce::dontSendNotification); }\n";
        out << "void paint() { auto separator = top.removeFromBottom (1); }\n";
        out << "void rebuildTimelineClipViews() { timelineClipStyles.push_back ({ kPurple, 0.82f }); }\n";
        out << "const std::array<yesdaw::ui::Clip, 1> kClips {{ { 0, 0, 0.0, 17.0 } }};\n";
        out << "const std::array<TimelineClipStyle, 1> kClipStyles {{ { kBlue, 0.82f } }};\n";
        out.close();

        std::ofstream timelineOut (scratch / "TimelineCanvas.h");
        REQUIRE (timelineOut.is_open());
        timelineOut << "void drawClip(juce::Graphics& g, juce::Rectangle<int> area) { g.drawRoundedRectangle (area.toFloat().reduced (0.5f), UiTheme::Radius::md, 1.0f); }\n";
        timelineOut << "TimelineCanvasGeometry timelineCanvasGeometry(juce::Rectangle<int> area) { auto content = area.reduced (1); auto toolbar = content.removeFromTop (36); auto ruler = content.removeFromTop (48); geometry.laneHeight = std::max (8, area.getHeight()); }\n";
        timelineOut << "TimelineCanvasGeometry timelineCanvasGeometryLaneFloor() { const int laneCount = std::max (1, state.trackCount); }\n";
        timelineOut << "TimelineCanvasGeometry timelineCanvasGeometryPixelsFloor() { geometry.viewport.pixelsPerSecond = std::max (1.0, geometry.viewport.pixelsPerSecond); }\n";
        timelineOut << "void drawToolbar(juce::Rectangle<int> toolbar) { auto field = toolbar.withWidth (80).reduced (0, UiTheme::Space::sm + 1); }\n";
        timelineOut << "void drawClipWaveform(juce::Rectangle<int> area, float amplitude) { area.reduce (7, 5); const auto half = area.getHeight() * std::clamp (amplitude, 0.1f, 1.0f) * 0.42f; const auto step = std::max (8, area.getWidth() / 9); }\n";
        timelineOut << "void drawRuler(juce::Rectangle<int> ruler, juce::Rectangle<int> clipArea, Viewport vp) { g.fillRect (ruler.withHeight (1).withY (ruler.getBottom() - 1)); const double labelStep = vp.pixelsPerSecond < 24.0 ? 8.0 : 4.0; if (x < clipArea.getX() - 40) return; g.drawText (label, x - 18, ruler.getY() + 7, 36, 16, just, false); }\n";
        timelineOut << "void drawGrid(juce::Rectangle<int> clipArea) { const int laneCount = std::max (1, state.trackCount); g.fillRect (clipArea.getX(), y, clipArea.getWidth(), 1); g.fillRect (clipArea.getX(), y + 1, 3, laneHeight - 1); const double firstGrid = std::floor (vp.scrollSeconds / 4.0) * 4.0; for (double seconds = firstGrid; seconds <= rightSeconds + 4.0; seconds += 4.0) { const bool major = (seconds % 16) == 0; g.fillRect (x, clipArea.getY(), 1, clipArea.getHeight()); } }\n";
        timelineOut << "void drawPlayhead(juce::Rectangle<int> ruler) { g.fillRect (playheadX, ruler.getY(), 2, height); g.fillRoundedRectangle (static_cast<float> (playheadX - 15), static_cast<float> (ruler.getY() + 4), 30.0f, 16.0f, radius); g.drawText (label, playheadX - 12, ruler.getY() + 4, 24, 16, just, false); }\n";
        timelineOut << "constexpr int kVisibleClipCapacity = 4096;\n";
        timelineOut << "struct TimelineCanvasState { double totalSeconds = 96.0; double playheadSeconds = 32.0; };\n";
        timelineOut << "TimelineCanvasClipStyle styleForClip() { TimelineCanvasClipStyle fallback { color, 0.7f }; return fallback; }\n";
        timelineOut << "void drawGridTone() { g.setColour (colour.withAlpha (0.42f)); g.setColour (colour.brighter (0.35f)); }\n";
        timelineOut.close();

        std::ofstream layoutOut (scratch / "TimelineLayout.h");
        REQUIRE (layoutOut.is_open());
        layoutOut << "struct Viewport { double pixelsPerSecond = 100.0; double widthPixels = 1280.0; double laneHeightPixels = 64.0; };\n";
        layoutOut << "TimelineHitTestResult hitTestVisibleClip(Viewport vp, double xPixels, double yPixels) { if (vp.pixelsPerSecond <= 0.0 || vp.widthPixels <= 0.0 || vp.laneHeightPixels <= 0.0 || xPixels < 0.0 || yPixels < 0.0) return {}; if (c.lengthSeconds <= 0.0) return {}; if (xPx < 0.0) { wPx += xPx; xPx = 0.0; } wPx = std::max (wPx, 0.0); }\n";
    }

    const auto findings = auditThemeTokens (scratch);
    std::filesystem::remove_all (scratch);

    std::vector<int> mainComponentLines;
    bool foundTimelineCanvasOutline = false;
    bool foundTimelineCanvasGeometry = false;
    bool foundTimelineCanvasGeometryLaneFloor = false;
    bool foundTimelineCanvasGeometryPixelsFloor = false;
    bool foundTimelineCanvasToolbarInsets = false;
    bool foundTimelineCanvasClipPaint = false;
    bool foundTimelineCanvasRuler = false;
    bool foundTimelineCanvasGrid = false;
    bool foundTimelineCanvasPlayhead = false;
    bool foundTimelineCanvasCapacity = false;
    bool foundTimelineCanvasStateDefaults = false;
    bool foundTimelineCanvasFallbackTone = false;
    bool foundTimelineCanvasPaintTone = false;
    bool foundTimelineLayoutViewport = false;
    bool foundTimelineLayoutHitTest = false;
    for (const auto& finding : findings)
    {
        if (finding.path.filename() == "MainComponent.cpp")
            mainComponentLines.push_back (finding.line);
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 1)
            foundTimelineCanvasOutline = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 2)
            foundTimelineCanvasGeometry = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 3)
            foundTimelineCanvasGeometryLaneFloor = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 4)
            foundTimelineCanvasGeometryPixelsFloor = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 5)
            foundTimelineCanvasToolbarInsets = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 6)
            foundTimelineCanvasClipPaint = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 7)
            foundTimelineCanvasRuler = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 8)
            foundTimelineCanvasGrid = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 9)
            foundTimelineCanvasPlayhead = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 10)
            foundTimelineCanvasCapacity = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 11)
            foundTimelineCanvasStateDefaults = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 12)
            foundTimelineCanvasFallbackTone = true;
        else if (finding.path.filename() == "TimelineCanvas.h" && finding.line == 13)
            foundTimelineCanvasPaintTone = true;
        else if (finding.path.filename() == "TimelineLayout.h" && finding.line == 1)
            foundTimelineLayoutViewport = true;
        else if (finding.path.filename() == "TimelineLayout.h" && finding.line == 2)
            foundTimelineLayoutHitTest = true;
    }

    REQUIRE (findings.size() == 59u);
    REQUIRE (foundTimelineCanvasOutline);
    REQUIRE (foundTimelineCanvasGeometry);
    REQUIRE (foundTimelineCanvasGeometryLaneFloor);
    REQUIRE (foundTimelineCanvasGeometryPixelsFloor);
    REQUIRE (foundTimelineCanvasToolbarInsets);
    REQUIRE (foundTimelineCanvasClipPaint);
    REQUIRE (foundTimelineCanvasRuler);
    REQUIRE (foundTimelineCanvasGrid);
    REQUIRE (foundTimelineCanvasPlayhead);
    REQUIRE (foundTimelineCanvasCapacity);
    REQUIRE (foundTimelineCanvasStateDefaults);
    REQUIRE (foundTimelineCanvasFallbackTone);
    REQUIRE (foundTimelineCanvasPaintTone);
    REQUIRE (foundTimelineLayoutViewport);
    REQUIRE (foundTimelineLayoutHitTest);
    REQUIRE (mainComponentLines.size() == 44u);
    REQUIRE (mainComponentLines[0] == 1);
    REQUIRE (mainComponentLines[1] == 2);
    REQUIRE (mainComponentLines[2] == 3);
    REQUIRE (mainComponentLines[3] == 4);
    REQUIRE (mainComponentLines[4] == 5);
    REQUIRE (mainComponentLines[5] == 6);
    REQUIRE (mainComponentLines[6] == 7);
    REQUIRE (mainComponentLines[7] == 8);
    REQUIRE (mainComponentLines[8] == 9);
    REQUIRE (mainComponentLines[9] == 10);
    REQUIRE (mainComponentLines[10] == 11);
    REQUIRE (mainComponentLines[11] == 12);
    REQUIRE (mainComponentLines[12] == 13);
    REQUIRE (mainComponentLines[13] == 14);
    REQUIRE (mainComponentLines[14] == 15);
    REQUIRE (mainComponentLines[15] == 16);
    REQUIRE (mainComponentLines[16] == 17);
    REQUIRE (mainComponentLines[17] == 18);
    REQUIRE (mainComponentLines[18] == 19);
    REQUIRE (mainComponentLines[19] == 20);
    REQUIRE (mainComponentLines[20] == 21);
    REQUIRE (mainComponentLines[21] == 22);
    REQUIRE (mainComponentLines[23] == 24);
    REQUIRE (mainComponentLines[24] == 25);
    REQUIRE (mainComponentLines[26] == 28);
    REQUIRE (mainComponentLines[27] == 29);
    REQUIRE (mainComponentLines[28] == 31);
    REQUIRE (mainComponentLines[29] == 33);
    REQUIRE (mainComponentLines[30] == 34);
    REQUIRE (mainComponentLines[31] == 37);
    REQUIRE (mainComponentLines[32] == 38);
    REQUIRE (mainComponentLines[33] == 40);
    REQUIRE (mainComponentLines[34] == 41);
    REQUIRE (mainComponentLines[35] == 42);
    REQUIRE (mainComponentLines[36] == 44);
    REQUIRE (mainComponentLines[37] == 45);
    REQUIRE (mainComponentLines[38] == 47);
    REQUIRE (mainComponentLines[39] == 48);
    REQUIRE (mainComponentLines[40] == 49);
    REQUIRE (mainComponentLines[41] == 50);
    REQUIRE (mainComponentLines[42] == 51);
    REQUIRE (mainComponentLines[43] == 52);
}
