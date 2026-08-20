// YES DAW - H11 native Timeline canvas.
//
// The app shell and the frame-time gate share this renderer so dense timeline drawing is measured
// through the same code path the user sees.

#pragma once

#include "ui/TimelineLayout.h"
#include "ui/UiIcons.h"
#include "ui/UiTheme.h"
#include "ui/WaveformColumns.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace yesdaw::persistence {
struct WaveformPeakCache;
}

namespace yesdaw::ui {

struct TimelineCanvasTrack
{
    const char* name;
    juce::Colour colour;
    float meter;
    // N6: persisted row height (0 = auto-shared — this track's row splits the panel's remaining
    // space equally with every other auto-shared row, exactly like every track did before this
    // field existed).
    int heightPx = 0;
};

struct TimelineCanvasClipStyle
{
    juce::Colour colour;
    float amplitude;
};

struct TimelineMarker
{
    double seconds;
    const char* label;
};

// N6: the shared "split available space between custom-height and auto-share rows" law. Used
// independently by the timeline canvas (built into timelineCanvasGeometry, which has its own
// clipArea height and legible floor) and the rail (its own available height and floor,
// trackListRowMinHeight) — same formula, two different panels, so it is a free function rather
// than living on one geometry struct. With no customized row, autoShare exactly reproduces the
// historical `availablePixels / rowCount` law and every top/height matches `row * autoShare`.
struct CumulativeRowGeometry
{
    std::vector<double> tops;
    std::vector<double> heights;
    int autoShare = 0;

    [[nodiscard]] double top (int row) const noexcept
    {
        if (tops.empty())
            return 0.0;
        if (row >= static_cast<int> (tops.size()))
            return tops.back() + heights.back();
        return tops[static_cast<std::size_t> (std::max (0, row))];
    }

    [[nodiscard]] double heightFor (int row) const noexcept
    {
        if (heights.empty())
            return 0.0;
        const std::size_t index = static_cast<std::size_t> (
            std::clamp (row, 0, static_cast<int> (heights.size()) - 1));
        return heights[index];
    }

    [[nodiscard]] int rowAtPixel (double y) const noexcept
    {
        if (tops.empty())
            return 0;
        if (y < 0.0)
            return 0;
        for (std::size_t i = 0; i < tops.size(); ++i)
            if (y < tops[i] + heights[i])
                return static_cast<int> (i);
        return static_cast<int> (tops.size()) - 1;
    }
};

[[nodiscard]] inline CumulativeRowGeometry computeCumulativeRowGeometry (
    int rowCount, int availablePixels, int minRowHeight, const int* customHeightsPx)
{
    CumulativeRowGeometry geometry;
    if (rowCount <= 0)
        return geometry;

    // N6: autoShare is derived from the TOTAL row count, independent of which/how-many rows are
    // customized — NOT "whatever's left after custom rows, split among the rest". That
    // redistribution would make every OTHER auto row's height depend on how many rows are
    // customized, violating "a drag changes THAT row's height and nothing else": resizing one
    // row would silently resize every other auto row too. A customized row instead simply grows
    // or shrinks the panel's total content height (more or less to scroll) — the same way adding
    // a track does — never its neighbours.
    geometry.autoShare = std::max (minRowHeight, availablePixels / rowCount);

    geometry.tops.resize (static_cast<std::size_t> (rowCount));
    geometry.heights.resize (static_cast<std::size_t> (rowCount));
    double cumulative = 0.0;
    for (int i = 0; i < rowCount; ++i)
    {
        const int custom = customHeightsPx != nullptr ? customHeightsPx[i] : 0;
        const double h = custom > 0 ? static_cast<double> (custom) : static_cast<double> (geometry.autoShare);
        geometry.tops[static_cast<std::size_t> (i)] = cumulative;
        geometry.heights[static_cast<std::size_t> (i)] = h;
        cumulative += h;
    }
    return geometry;
}

// M7: a MIDI Clip's own notes, so the timeline can draw what the clip CONTAINS. Before M7 a MIDI
// clip fell through to the hash-seeded placeholder waveform — a fabricated audio wiggle for a clip
// that has no audio at all.
struct TimelineClipNote
{
    int    clipId = 0;
    double startSeconds = 0.0;
    double lengthSeconds = 0.0;
    int    key = 60;
};

struct TimelineCanvasState
{
    const TimelineCanvasTrack* tracks = nullptr;
    int trackCount = 0;

    const Clip* clips = nullptr;
    const TimelineCanvasClipStyle* clipStyles = nullptr;
    int clipCount = 0;

    const TimelineMarker* markers = nullptr;
    int markerCount = 0;

    // M7: notes for MIDI clips, sorted by clipId. A clip with no entries here is an audio clip.
    const TimelineClipNote* clipNotes = nullptr;
    int clipNoteCount = 0;

    std::function<std::shared_ptr<const persistence::WaveformPeakCache> (int clipId)> waveformCacheLookup;

    Viewport viewport {};
    double totalSeconds = UiTheme::Layout::timelineCanvasDefaultTotalSeconds;
    double playheadSeconds = UiTheme::Layout::timelineCanvasDefaultPlayheadSeconds;

    // Ruler range selection (parity item 25): painted as a band across ruler and lanes when active.
    bool rangeSelectionActive = false;
    double rangeStartSeconds = 0.0;
    double rangeEndSeconds = 0.0;

    // Vertical track scroll (E5): whole lane rows above the viewport. Geometry clamps this and
    // publishes the pixel offset every paint/hit/gesture consumer shares.
    int trackScrollRows = 0;

    // Transport loop brace (E6): painted on the upper ruler band and draggable by its handles.
    bool loopActive = false;
    double loopStartSeconds = 0.0;
    double loopEndSeconds = 0.0;

    // N4: when the automation lane is open, it is a SUB-LANE of THIS row (a track index into
    // `tracks`, same numbering as `Clip::lane`) — carved from the bottom of that row's own rect,
    // not a fixed strip that floats above every track regardless of which one is being edited.
    // -1 means no row is targeted (nothing paints).
    bool automationLaneVisible = false;
    int automationLaneTrackRow = -1;
};

struct TimelineCanvasPaintStats
{
    int visibleClips = 0;
    int visibleClipCapacity = 0;
    bool hitVisibleClipCapacity = false;
    int readyWaveformClips = 0;
    int pendingWaveformClips = 0;
    int readyWaveformColumns = 0;
    int placeholderWaveformClips = 0;
};

struct TimelineCanvasGeometry
{
    juce::Rectangle<int> toolbarArea;
    juce::Rectangle<int> rulerArea;
    // N4: empty unless a track row is targeted — carved from the BOTTOM of that row's own rect
    // (see timelineCanvasGeometry), never a fixed strip above every track.
    juce::Rectangle<int> automationLaneArea;
    juce::Rectangle<int> clipArea;
    Viewport viewport;
    // N6: the AUTO-SHARED row height — every row without a persisted override splits the
    // remaining space equally, floored at the legible minimum, exactly like the pre-N6 uniform
    // law. Still meaningful with custom heights present: it is what every non-customized row uses.
    int laneHeight = 0;
    // Vertical track scroll (E5): the clamped whole-row scroll range for the current lane count.
    int maxTrackScrollRows = 0;
    int trackScrollRows = 0;
    // N6: per-lane cumulative top offset (viewport-local, lane 0 at 0 — laneScrollPixels is NOT
    // subtracted here, matching Viewport::laneTopPixels' contract) and height, one entry per
    // lane. Always populated to `laneCount` entries — with no customized track this exactly
    // reproduces `lane * laneHeight`, so every reader can use these unconditionally.
    std::vector<double> laneTopPixels;
    std::vector<double> laneHeightPixelsPerLane;

    // N6: the row Y (viewport-local, BEFORE scroll) for `lane`, clamped to a valid index. `lane
    // == the lane count` (one past the last row) is a deliberately supported "bottom edge" case —
    // it returns the total cumulative content height, matching how the old `lane * laneHeight`
    // formula naturally extrapolated one row past the end for a closing border line. The shared
    // accessor every consumer (rail, automation, clip drag preview, the grid's closing border)
    // should use instead of re-deriving `lane * laneHeight`.
    [[nodiscard]] double laneTop (int lane) const noexcept
    {
        if (laneTopPixels.empty())
            return static_cast<double> (lane) * laneHeight;
        if (lane >= static_cast<int> (laneTopPixels.size()))
            return laneTopPixels.back() + laneHeightPixelsPerLane.back();
        const std::size_t index = static_cast<std::size_t> (std::max (0, lane));
        return laneTopPixels[index];
    }

    [[nodiscard]] double laneHeightFor (int lane) const noexcept
    {
        if (laneHeightPixelsPerLane.empty())
            return laneHeight;
        const std::size_t index = static_cast<std::size_t> (
            std::clamp (lane, 0, static_cast<int> (laneHeightPixelsPerLane.size()) - 1));
        return laneHeightPixelsPerLane[index];
    }

    // N6: the inverse of laneTop/laneHeightFor — which lane a scrolled, viewport-local Y pixel
    // (i.e. pointerY - clipArea.getY() + laneScrollPixels, the SAME expression every pointer-to-
    // lane call site already computes) falls in. Every consumer that used to do
    // `pixelY / laneHeight` should call this instead so a customized row's hit area matches its
    // painted rect exactly. Clamped to [0, lane count - 1]; never returns an out-of-range lane.
    [[nodiscard]] int laneAtPixel (double scrolledLocalY) const noexcept
    {
        if (laneTopPixels.empty())
            return laneHeight > 0
                ? static_cast<int> (std::floor (scrolledLocalY / laneHeight))
                : 0;

        if (scrolledLocalY < 0.0)
            return 0;

        for (std::size_t i = 0; i < laneTopPixels.size(); ++i)
            if (scrolledLocalY < laneTopPixels[i] + laneHeightPixelsPerLane[i])
                return static_cast<int> (i);

        return static_cast<int> (laneTopPixels.size()) - 1;
    }
};

namespace timeline_canvas_detail {

constexpr int kVisibleClipCapacity = UiTheme::Layout::timelineCanvasVisibleClipCapacity;

const juce::Colour kPanel = UiTheme::Color::panel();
const juce::Colour kPanelStroke = UiTheme::Color::panelStroke();
const juce::Colour kText = UiTheme::Color::text();
const juce::Colour kMutedText = UiTheme::Color::mutedText();
const juce::Colour kGrid = UiTheme::Color::timelineGrid();
const juce::Colour kCanvasBack = UiTheme::Color::timelineCanvas();
const juce::Colour kToolbarBack = UiTheme::Color::timelineToolbar();
const juce::Colour kRulerBack = UiTheme::Color::timelineRuler();
const juce::Colour kPurple = UiTheme::Color::accentPurple();

inline void drawSmallLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                            juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour (kMutedText);
    g.setFont (UiTheme::Type::font (UiTheme::Type::small));
    g.drawText (text, area, justification, false);
}

inline void fillPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (UiTheme::Color::panelShadow().withAlpha (UiTheme::Tone::shadowAlpha));
    g.fillRoundedRectangle (
        area.toFloat().translated (0.0f, static_cast<float> (UiTheme::Layout::controlShadowOffset)),
        UiTheme::Radius::lg);
    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat(), UiTheme::Radius::lg);
    g.setColour (kPanelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (UiTheme::Layout::timelineCanvasOutlineInset),
                            UiTheme::Radius::lg,
                            UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
    g.setColour (UiTheme::Color::panelInnerHighlight().withAlpha (UiTheme::Tone::innerHighlightAlpha));
    g.drawHorizontalLine (area.getY() + UiTheme::Layout::controlInnerHighlightHeight,
                          static_cast<float> (area.getX()) + UiTheme::Radius::lg,
                          static_cast<float> (area.getRight()) - UiTheme::Radius::lg);
}

inline TimelineCanvasClipStyle styleForClip (const TimelineCanvasState& state, int clipId)
{
    const TimelineCanvasClipStyle fallback {
        UiTheme::Color::accentBlue(),
        UiTheme::Tone::timelineCanvasFallbackClipAmplitude
    };
    if (state.clipStyles == nullptr || state.clips == nullptr || state.clipCount <= 0)
        return fallback;

    if (clipId >= 0 && clipId < state.clipCount && state.clips[clipId].id == clipId)
        return state.clipStyles[clipId];

    for (int i = 0; i < state.clipCount; ++i)
        if (state.clips[i].id == clipId)
            return state.clipStyles[i];

    return fallback;
}

inline const Clip* clipForId (const TimelineCanvasState& state, int clipId)
{
    if (state.clips == nullptr || state.clipCount <= 0)
        return nullptr;

    if (clipId >= 0 && clipId < state.clipCount && state.clips[clipId].id == clipId)
        return &state.clips[clipId];

    for (int i = 0; i < state.clipCount; ++i)
        if (state.clips[i].id == clipId)
            return &state.clips[i];

    return nullptr;
}

// M7: a clip whose peaks are not ready yet paints an honest PENDING body — a single centre line,
// no invented peaks. This used to synthesize a waveform from a hash of the clip id, which meant the
// timeline drew confident audio detail for content it had never read (and for MIDI clips, which
// have no audio at all).
inline void drawClipPendingBody (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour)
{
    area.reduce (UiTheme::Layout::timelineCanvasWaveformInsetX,
                 UiTheme::Layout::timelineCanvasWaveformInsetY);
    if (area.isEmpty())
        return;

    g.setColour (colour.brighter (UiTheme::Tone::timelineCanvasWaveformBrightness)
                       .withAlpha (UiTheme::Tone::timelineCanvasGridMinorLineAlpha));
    g.fillRect (area.getX(),
                area.getCentreY(),
                area.getWidth(),
                UiTheme::Layout::timelineCanvasGridLineWidth);
}

// M7: how many notes this clip owns (the stats branch asks before painting, so a MIDI clip is
// never counted as a pending WAVEFORM).
inline int drawClipNotePreviewCount (const TimelineCanvasState& state, const Clip& clip)
{
    if (state.clipNotes == nullptr || state.clipNoteCount <= 0)
        return 0;

    int count = 0;
    for (int i = 0; i < state.clipNoteCount; ++i)
        if (state.clipNotes[i].clipId == clip.id)
            ++count;

    return count;
}

// M7: a MIDI clip paints its NOTES — a mini piano roll inside the clip body, pitch-scaled to the
// notes actually present. Strided like the waveform path so a dense arrangement stays inside the
// frame budget.
inline int drawClipNotePreview (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                                const TimelineCanvasState& state, const Clip& clip)
{
    if (state.clipNotes == nullptr || state.clipNoteCount <= 0)
        return 0;

    area.reduce (UiTheme::Layout::timelineCanvasWaveformInsetX,
                 UiTheme::Layout::timelineCanvasWaveformInsetY);
    if (area.isEmpty() || clip.lengthSeconds <= 0.0)
        return 0;

    int lowKey = 127;
    int highKey = 0;
    int noteCount = 0;
    for (int i = 0; i < state.clipNoteCount; ++i)
    {
        if (state.clipNotes[i].clipId != clip.id)
            continue;

        lowKey = std::min (lowKey, state.clipNotes[i].key);
        highKey = std::max (highKey, state.clipNotes[i].key);
        ++noteCount;
    }

    if (noteCount == 0)
        return 0;

    // A clip whose notes share one pitch has no pitch RANGE to show: it draws in the middle of the
    // band rather than pretending the pitch means a position.
    const bool degenerateSpan = highKey == lowKey;
    const int keySpan = std::max (UiTheme::Layout::timelineCanvasNotePreviewMinKeySpan, highKey - lowKey);
    const int rowHeight = std::max (UiTheme::Layout::timelineCanvasGridLineWidth,
                                    area.getHeight() / (keySpan + 1));
    const int stride = std::max (1, noteCount / UiTheme::Layout::timelineCanvasNotePreviewMaxNotes);

    g.setColour (colour.brighter (UiTheme::Tone::timelineCanvasWaveformBrightness));
    int drawn = 0;
    int seen = 0;
    for (int i = 0; i < state.clipNoteCount; ++i)
    {
        const TimelineClipNote& note = state.clipNotes[i];
        if (note.clipId != clip.id)
            continue;

        if ((seen++ % stride) != 0)
            continue;

        const double startFraction = std::clamp ((note.startSeconds - clip.startSeconds) / clip.lengthSeconds, 0.0, 1.0);
        const double endFraction = std::clamp (
            (note.startSeconds + note.lengthSeconds - clip.startSeconds) / clip.lengthSeconds, 0.0, 1.0);
        const int x = area.getX() + juce::roundToInt (startFraction * static_cast<double> (area.getWidth()));
        const int width = std::max (UiTheme::Layout::timelineCanvasGridLineWidth,
                                    juce::roundToInt ((endFraction - startFraction)
                                                      * static_cast<double> (area.getWidth())));
        const int keyOffset = degenerateSpan ? keySpan / 2
                                            : std::clamp (highKey - note.key, 0, keySpan);
        const int y = area.getY() + keyOffset * rowHeight;
        g.fillRect (x, std::min (y, area.getBottom() - rowHeight), width, rowHeight);
        ++drawn;
    }

    return drawn;
}

inline int drawClipCachedWaveform (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                                   float amplitude, const Clip& clip,
                                   const persistence::WaveformPeakCache& cache,
                                   const Viewport& vp)
{
    area.reduce (UiTheme::Layout::timelineCanvasWaveformInsetX,
                 UiTheme::Layout::timelineCanvasWaveformInsetY);
    if (area.isEmpty()
        || clip.lengthSeconds <= UiThemeLayout::timelineLayoutZeroFloor
        || vp.pixelsPerSecond <= UiThemeLayout::timelineLayoutZeroFloor)
    {
        return 0;
    }

    const double sampleRate = static_cast<double> (cache.sourceFrames) / clip.lengthSeconds;
    const double clipLocalStartSeconds = std::max (UiThemeLayout::timelineLayoutZeroFloor,
                                                   vp.scrollSeconds - clip.startSeconds);
    const double visibleSeconds = static_cast<double> (area.getWidth()) / vp.pixelsPerSecond;
    const auto sourceFrameOffset = static_cast<std::uint64_t> (
        std::llround (clipLocalStartSeconds * sampleRate));
    const auto sourceFrameCount = static_cast<std::uint64_t> (
        std::llround (visibleSeconds * sampleRate));

    const WaveformColumnViewport columnViewport {
        sourceFrameOffset,
        sourceFrameCount,
        sampleRate,
        vp.pixelsPerSecond,
        area.getWidth()
    };
    const WaveformColumns columns = computeWaveformColumns (cache, columnViewport);
    if (columns.columns.empty())
        return 0;

    const float midY = static_cast<float> (area.getCentreY());
    const float half = static_cast<float> (area.getHeight())
                     * std::clamp (amplitude,
                                   UiTheme::Layout::timelineCanvasWaveformMinAmplitude,
                                   UiTheme::Layout::timelineCanvasWaveformMaxAmplitude)
                     * UiTheme::Layout::timelineCanvasWaveformHeightScale;
    const float minValue = -UiTheme::Layout::timelineCanvasWaveformMaxAmplitude;
    const float maxValue = UiTheme::Layout::timelineCanvasWaveformMaxAmplitude;
    const juce::Colour peakColour = colour.brighter (UiTheme::Tone::timelineCanvasWaveformBrightness);
    const juce::Colour rmsColour = peakColour.withAlpha (UiTheme::Tone::timelineCanvasGridMinorLineAlpha);

    int x = area.getX();
    for (const auto& column : columns.columns)
    {
        const float top = midY - half * std::clamp (column.max, minValue, maxValue);
        const float bottom = midY - half * std::clamp (column.min, minValue, maxValue);
        const float rms = half * std::clamp (column.rms, UiTheme::Layout::timelineCanvasWaveformMinAmplitude,
                                            UiTheme::Layout::timelineCanvasWaveformMaxAmplitude);

        g.setColour (rmsColour);
        g.drawVerticalLine (x, midY - rms, midY + rms);
        g.setColour (peakColour);
        g.drawVerticalLine (x, top, bottom);

        ++x;
        if (x >= area.getRight())
            break;
    }

    return static_cast<int> (columns.columns.size());
}

inline bool drawClipFrame (juce::Graphics& g, juce::Rectangle<int> area,
                           const TimelineCanvasClipStyle& style)
{
    if (area.getWidth() <= UiTheme::Layout::timelineCanvasClipMinPaintWidth
        || area.getHeight() <= UiTheme::Layout::timelineCanvasClipMinPaintHeight)
        return false;

    if (area.getHeight() <= UiTheme::Layout::timelineCanvasClipCompactHeight)
    {
        g.setColour (style.colour.withAlpha (UiTheme::Tone::timelineCanvasCompactClipAlpha));
        g.fillRect (area);
        g.setColour (style.colour.brighter (UiTheme::Tone::timelineCanvasCompactHighlightBrightness));
        g.fillRect (area.withHeight (UiTheme::Layout::timelineCanvasClipCompactHighlightHeight));
        return false;
    }

    // Mid tier (E5): row-height clips draw a FLAT frame — no antialiased gradient or rounded
    // corners — so a dense overflow arrangement stays inside the 60fps frame budget; the
    // waveform still draws on top.
    if (area.getHeight() < UiTheme::Layout::timelineCanvasClipRichPaintHeight)
    {
        g.setColour (style.colour.withAlpha (UiTheme::Tone::timelineCanvasClipFillAlpha));
        g.fillRect (area);
        g.setColour (style.colour.brighter (UiTheme::Tone::timelineCanvasCompactHighlightBrightness));
        g.fillRect (area.withHeight (UiTheme::Layout::timelineCanvasClipCompactHighlightHeight));
        g.setColour (style.colour.brighter (UiTheme::Tone::timelineCanvasClipOutlineBrightness));
        g.drawRect (area, UiTheme::Layout::timelineCanvasGridLineWidth);
        return true;
    }

    const auto clipBounds = area.toFloat();
    juce::ColourGradient clipGradient (
        style.colour.brighter (UiTheme::Tone::timelineCanvasClipSurfaceTopBrightness)
            .withAlpha (UiTheme::Tone::timelineCanvasClipSurfaceTopAlpha),
        clipBounds.getCentreX(), clipBounds.getY(),
        style.colour.withAlpha (UiTheme::Tone::timelineCanvasClipFillAlpha),
        clipBounds.getCentreX(), clipBounds.getBottom(), false);
    g.setGradientFill (clipGradient);
    g.fillRoundedRectangle (clipBounds, UiTheme::Radius::md);
    g.setColour (style.colour.brighter (UiTheme::Tone::timelineCanvasClipOutlineBrightness));
    g.drawRoundedRectangle (area.toFloat().reduced (UiTheme::Layout::timelineCanvasOutlineInset),
                            UiTheme::Radius::md,
                            UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
    return true;
}

// M7: a clip with no cached peaks draws its notes when it HAS notes (MIDI), and an honest pending
// body otherwise. Nothing is ever invented.
inline void drawClip (juce::Graphics& g, juce::Rectangle<int> area, const TimelineCanvasClipStyle& style,
                      const TimelineCanvasState& state, const Clip* clip)
{
    if (! drawClipFrame (g, area, style))
        return;

    if (clip != nullptr && drawClipNotePreview (g, area, style.colour, state, *clip) > 0)
        return;

    drawClipPendingBody (g, area, style.colour);
}

inline void drawToolbar (juce::Graphics& g, juce::Rectangle<int> toolbar)
{
    g.setColour (kToolbarBack);
    g.fillRect (toolbar);

    auto tools = toolbar.withTrimmedLeft (UiTheme::Space::xl)
                         .withWidth (UiTheme::Layout::timelineCanvasToolbarWidth)
                         .reduced (UiTheme::Layout::timelineCanvasToolbarInsetX,
                                   UiTheme::Layout::timelineCanvasToolbarInsetY);
    const std::array<TimelineTool, 5> toolsInOrder {{
        TimelineTool::Pointer,
        TimelineTool::Pencil,
        TimelineTool::Scissors,
        TimelineTool::Hand,
        TimelineTool::Zoom
    }};
    for (std::size_t index = 0; index < toolsInOrder.size(); ++index)
    {
        auto cell = tools.removeFromLeft (UiTheme::Layout::timelineCanvasToolCellWidth)
                         .reduced (UiTheme::Layout::timelineCanvasToolCellInsetX,
                                   UiTheme::Layout::timelineCanvasToolCellInsetY);
        g.setColour (index == 0u ? UiTheme::Color::accentPurpleDeep() : UiTheme::Color::toolButton());
        g.fillRoundedRectangle (cell.toFloat(), UiTheme::Radius::sm);
        g.setColour (UiTheme::Color::buttonBorder());
        g.drawRoundedRectangle (cell.toFloat().reduced (UiTheme::Layout::controlOutlineInset),
                                UiTheme::Radius::sm,
                                UiTheme::Layout::controlOutlineStrokeWidth);
        drawTimelineToolIcon (
            g,
            toolsInOrder[index],
            cell.toFloat().reduced (static_cast<float> (UiTheme::Layout::controlIconInset)),
            index == 0u ? UiTheme::Color::text() : UiTheme::Color::buttonTextMuted());
    }

    drawSmallLabel (g,
                    "SNAP",
                    toolbar.withTrimmedLeft (UiTheme::Layout::timelineCanvasSnapLabelX)
                           .withWidth (UiTheme::Layout::timelineCanvasSnapLabelWidth),
                    juce::Justification::centred);
    g.setColour (UiTheme::Color::snapField());
    g.fillRoundedRectangle (toolbar.withTrimmedLeft (UiTheme::Layout::timelineCanvasSnapFieldX)
                                   .withWidth (UiTheme::Layout::timelineCanvasSnapFieldWidth)
                                   .reduced (UiTheme::Layout::timelineCanvasSnapFieldInsetX,
                                             UiTheme::Layout::timelineCanvasSnapFieldInsetY)
                                   .toFloat(),
                            UiTheme::Radius::sm);
    g.setColour (kText);
    g.setFont (UiTheme::Type::font (UiTheme::Type::body, juce::Font::bold));
    g.drawText ("Bar",
                toolbar.withTrimmedLeft (UiTheme::Layout::timelineCanvasSnapValueX)
                       .withWidth (UiTheme::Layout::timelineCanvasSnapValueWidth),
                juce::Justification::centredLeft,
                false);
}

inline void drawRuler (juce::Graphics& g, juce::Rectangle<int> ruler, juce::Rectangle<int> clipArea,
                       const TimelineCanvasState& state, const Viewport& vp)
{
    g.setColour (kRulerBack);
    g.fillRect (ruler);
    g.setColour (kGrid);
    g.fillRect (ruler.withHeight (UiTheme::Layout::timelineCanvasRulerSeparatorHeight)
                      .withY (ruler.getBottom() - UiTheme::Layout::timelineCanvasRulerSeparatorHeight));

    const double rightSeconds = vp.scrollSeconds + static_cast<double> (clipArea.getWidth()) / vp.pixelsPerSecond;
    const double labelStep = vp.pixelsPerSecond < UiTheme::Layout::timelineCanvasRulerDensePixelsPerSecond
                                 ? UiTheme::Layout::timelineCanvasRulerWideLabelStepSeconds
                                 : UiTheme::Layout::timelineCanvasRulerNarrowLabelStepSeconds;
    const double firstLabel = std::floor (vp.scrollSeconds / labelStep) * labelStep;

    for (double seconds = firstLabel; seconds <= rightSeconds + labelStep; seconds += labelStep)
    {
        const int x = clipArea.getX() + juce::roundToInt ((seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() - UiTheme::Layout::timelineCanvasRulerLabelCullPadding
            || x > clipArea.getRight() + UiTheme::Layout::timelineCanvasRulerLabelCullPadding)
            continue;

        const int barNumber = std::max (1, juce::roundToInt (seconds) + 1);
        g.setColour (kMutedText);
        g.setFont (UiTheme::Type::numericFont (UiTheme::Type::small));
        g.drawText (juce::String (barNumber),
                    x - UiTheme::Layout::timelineCanvasRulerLabelLeftInset,
                    ruler.getY() + UiTheme::Layout::timelineCanvasRulerLabelTopInset,
                    UiTheme::Layout::timelineCanvasRulerLabelWidth,
                    UiTheme::Layout::timelineCanvasRulerLabelHeight,
                    juce::Justification::centred, false);
        g.setColour (kMutedText.withAlpha (UiTheme::Tone::timelineCanvasRulerTickAlpha));
        g.fillRect (x,
                    ruler.getBottom() - UiTheme::Layout::timelineCanvasRulerTickHeight,
                    UiTheme::Layout::timelineCanvasRulerTickWidth,
                    UiTheme::Layout::timelineCanvasRulerTickHeight);
    }

    if (state.markers == nullptr)
        return;

    for (int i = 0; i < state.markerCount; ++i)
    {
        const auto& marker = state.markers[i];
        const int x = clipArea.getX() + juce::roundToInt ((marker.seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() - UiTheme::Layout::timelineCanvasRulerMarkerCullPadding
            || x > clipArea.getRight())
            continue;

        g.setColour (kText);
        g.setFont (UiTheme::Type::font (UiTheme::Type::small, juce::Font::bold));
        g.drawText (marker.label,
                    x + UiTheme::Layout::timelineCanvasRulerMarkerLabelLeftInset,
                    ruler.getY() + UiTheme::Layout::timelineCanvasRulerMarkerLabelTopInset,
                    UiTheme::Layout::timelineCanvasRulerMarkerLabelWidth,
                    UiTheme::Layout::timelineCanvasRulerMarkerLabelHeight,
                    juce::Justification::centredLeft, false);
    }
}

inline void drawRangeSelection (juce::Graphics& g, juce::Rectangle<int> ruler,
                                juce::Rectangle<int> clipArea,
                                const TimelineCanvasState& state, const Viewport& vp)
{
    if (! state.rangeSelectionActive || state.rangeEndSeconds <= state.rangeStartSeconds)
        return;

    const int startX = clipArea.getX()
                     + juce::roundToInt ((state.rangeStartSeconds - vp.scrollSeconds) * vp.pixelsPerSecond);
    const int endX = clipArea.getX()
                   + juce::roundToInt ((state.rangeEndSeconds - vp.scrollSeconds) * vp.pixelsPerSecond);
    const int left = std::max (clipArea.getX(), std::min (startX, endX));
    const int right = std::min (clipArea.getRight(), std::max (startX, endX));
    if (right <= left)
        return;

    const juce::Rectangle<int> band { left, ruler.getY(), right - left,
                                      clipArea.getBottom() - ruler.getY() };
    g.setColour (UiTheme::Color::accentBlue().withAlpha (UiTheme::Tone::pressedHighlightAlpha));
    g.fillRect (band);
    g.setColour (UiTheme::Color::accentBlue().withAlpha (UiTheme::Tone::focusRingAlpha));
    g.drawRect (band.toFloat(), UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
}

inline void drawGrid (juce::Graphics& g, juce::Rectangle<int> clipArea, const TimelineCanvasState& state,
                      const TimelineCanvasGeometry& geometry)
{
    const Viewport& vp = geometry.viewport;
    g.setColour (kCanvasBack);
    g.fillRect (clipArea);

    // N6: row separators and track-colour tint follow the SAME per-lane cumulative geometry as
    // clips and hit-testing — a resized row's grid line moves with it, not just its clips.
    const int laneCount = std::max (UiTheme::Layout::timelineCanvasGridMinLaneCount, state.trackCount);
    const double laneScroll = geometry.laneTop (geometry.trackScrollRows);
    for (int lane = 0; lane <= laneCount; ++lane)
    {
        const int laneHeight = static_cast<int> (std::llround (geometry.laneHeightFor (lane)));
        const int y = clipArea.getY()
                    + static_cast<int> (std::llround (geometry.laneTop (lane) - laneScroll));
        if (y + laneHeight < clipArea.getY() || y > clipArea.getBottom())
            continue;
        g.setColour (kGrid.withAlpha (UiTheme::Tone::timelineCanvasGridLaneSeparatorAlpha));
        g.fillRect (clipArea.getX(), y, clipArea.getWidth(), UiTheme::Layout::timelineCanvasGridLaneSeparatorHeight);

        if (lane < state.trackCount && state.tracks != nullptr)
        {
            g.setColour (state.tracks[lane].colour.withAlpha (UiTheme::Tone::timelineCanvasGridTrackTintAlpha));
            g.fillRect (clipArea.getX(),
                        y + UiTheme::Layout::timelineCanvasGridTrackTintTopInset,
                        UiTheme::Layout::timelineCanvasGridTrackTintWidth,
                        std::max (0, laneHeight - UiTheme::Layout::timelineCanvasGridTrackTintHeightTrim));
        }
    }

    const double rightSeconds = vp.scrollSeconds + static_cast<double> (clipArea.getWidth()) / vp.pixelsPerSecond;
    const double gridStepSeconds = UiTheme::Layout::timelineCanvasGridStepSeconds;
    const double firstGrid = std::floor (vp.scrollSeconds / gridStepSeconds) * gridStepSeconds;
    for (double seconds = firstGrid; seconds <= rightSeconds + gridStepSeconds; seconds += gridStepSeconds)
    {
        const int x = clipArea.getX() + juce::roundToInt ((seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() || x > clipArea.getRight())
            continue;

        const bool major = (juce::roundToInt (seconds) % UiTheme::Layout::timelineCanvasGridMajorStepSeconds) == 0;
        g.setColour (major ? kGrid.brighter (UiTheme::Tone::timelineCanvasGridMajorLineBrightness)
                            : kGrid.withAlpha (UiTheme::Tone::timelineCanvasGridMinorLineAlpha));
        g.fillRect (x, clipArea.getY(), UiTheme::Layout::timelineCanvasGridLineWidth, clipArea.getHeight());
    }
}

inline void drawPlayhead (juce::Graphics& g, juce::Rectangle<int> ruler, juce::Rectangle<int> clipArea,
                          const TimelineCanvasState& state, const Viewport& vp)
{
    const int playheadX = clipArea.getX() + juce::roundToInt ((state.playheadSeconds - vp.scrollSeconds)
                                                             * vp.pixelsPerSecond);
    if (playheadX < clipArea.getX() || playheadX > clipArea.getRight())
        return;

    g.setColour (UiTheme::Color::white());
    g.fillRect (playheadX,
                ruler.getY(),
                UiTheme::Layout::timelineCanvasPlayheadLineWidth,
                clipArea.getBottom() - ruler.getY());
    g.setColour (kPurple);
    g.fillRoundedRectangle (
        static_cast<float> (playheadX - UiTheme::Layout::timelineCanvasPlayheadBadgeHalfWidth),
        static_cast<float> (ruler.getY() + UiTheme::Layout::timelineCanvasPlayheadBadgeTopInset),
        static_cast<float> (UiTheme::Layout::timelineCanvasPlayheadBadgeWidth),
        static_cast<float> (UiTheme::Layout::timelineCanvasPlayheadBadgeHeight),
        UiTheme::Radius::pill);
    g.setColour (kText);
    g.setFont (UiTheme::Type::numericFont (UiTheme::Type::small, juce::Font::bold));
    g.drawText (juce::String (std::max (1, juce::roundToInt (state.playheadSeconds) + 1)),
                playheadX - UiTheme::Layout::timelineCanvasPlayheadTextHalfWidth,
                ruler.getY() + UiTheme::Layout::timelineCanvasPlayheadBadgeTopInset,
                UiTheme::Layout::timelineCanvasPlayheadTextWidth,
                UiTheme::Layout::timelineCanvasPlayheadTextHeight,
                juce::Justification::centred,
                false);
}

} // namespace timeline_canvas_detail

inline TimelineCanvasGeometry timelineCanvasGeometry (juce::Rectangle<int> area,
                                                       const TimelineCanvasState& state)
{
    TimelineCanvasGeometry geometry;
    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return geometry;

    auto content = area.reduced (UiTheme::Layout::timelineCanvasOuterInset);
    geometry.toolbarArea = content.removeFromTop (UiTheme::Layout::timelineCanvasToolbarHeight);
    geometry.rulerArea = content.removeFromTop (UiTheme::Layout::timelineCanvasRulerHeight);
    geometry.clipArea = content.reduced (UiTheme::Layout::timelineCanvasClipAreaInsetX,
                                         UiTheme::Layout::timelineCanvasClipAreaInsetY);

    // E5 lane law: few tracks stretch to fill the viewport; once rows would fall below the fixed
    // row height, lanes hold that height and the shared row offset scrolls them.
    const int laneCount = std::max (UiTheme::Layout::timelineCanvasGeometryMinLaneCount, state.trackCount);

    // N6: a persisted per-track height (state.tracks[i].heightPx > 0) takes that row's exact
    // pixel height; every other row keeps the historical uniform share — with no customized
    // track this reduces to the historical uniform law bit-for-bit. Delegates to the SAME shared
    // law the rail uses (computeCumulativeRowGeometry) so the two panels can never drift apart.
    std::vector<int> laneCustomHeights (static_cast<std::size_t> (laneCount), 0);
    for (int i = 0; i < laneCount; ++i)
        if (state.tracks != nullptr && i < state.trackCount)
            laneCustomHeights[static_cast<std::size_t> (i)] = state.tracks[i].heightPx;

    const CumulativeRowGeometry laneLaw = computeCumulativeRowGeometry (
        laneCount, geometry.clipArea.getHeight (), UiTheme::Layout::timelineCanvasLaneRowHeight,
        laneCustomHeights.data());
    geometry.laneHeight = laneLaw.autoShare;
    geometry.laneTopPixels = laneLaw.tops;
    geometry.laneHeightPixelsPerLane = laneLaw.heights;

    const int visibleRows = std::max (1, geometry.clipArea.getHeight() / geometry.laneHeight);
    geometry.maxTrackScrollRows = std::max (0, laneCount - visibleRows);
    geometry.trackScrollRows = std::clamp (state.trackScrollRows, 0, geometry.maxTrackScrollRows);

    // N4/N6: the automation lane is a SUB-LANE of its own track row — carved from the BOTTOM of
    // that row's own rect, using the SAME per-row cumulative geometry every clip and rail row now
    // shares. Carving from the row's own space (rather than requesting new space beside it) is
    // what makes this work even with few tracks: the E5 lane law stretches a lone row to fill the
    // whole viewport, so there is no "room after it" to insert into — but there is always room to
    // carve FROM it. A row scrolled out of view honestly paints nothing rather than a misplaced
    // band.
    if (state.automationLaneVisible && state.automationLaneTrackRow >= 0)
    {
        const int row = std::clamp (state.automationLaneTrackRow, 0, laneCount - 1);
        const double rowHeight = geometry.laneHeightFor (row);
        const int rowTop = geometry.clipArea.getY()
                          + static_cast<int> (std::llround (
                                geometry.laneTop (row) - geometry.laneTop (geometry.trackScrollRows)));
        const int rowBottom = rowTop + static_cast<int> (std::llround (rowHeight));
        if (rowTop >= geometry.clipArea.getY() && rowBottom <= geometry.clipArea.getBottom())
        {
            const int bandHeight = static_cast<int> (
                std::min<double> (UiTheme::Layout::timelineCanvasAutomationBandHeight, rowHeight));
            geometry.automationLaneArea = juce::Rectangle<int> (
                geometry.clipArea.getX(), rowBottom - bandHeight,
                geometry.clipArea.getWidth(), bandHeight);
        }
    }

    geometry.viewport = state.viewport;
    geometry.viewport.pixelsPerSecond =
        std::max (UiTheme::Layout::timelineCanvasViewportMinPixelsPerSecond,
                  geometry.viewport.pixelsPerSecond);
    geometry.viewport.widthPixels = static_cast<double> (geometry.clipArea.getWidth());
    geometry.viewport.laneHeightPixels = static_cast<double> (geometry.laneHeight);
    geometry.viewport.laneScrollPixels = geometry.laneTop (geometry.trackScrollRows);
    // N6: geometry.viewport.laneTopPixels/laneHeightPixelsPerLane are deliberately left null
    // here — they would be raw pointers into geometry.laneTopPixels/laneHeightPixelsPerLane, and
    // TimelineCanvasGeometry is an ordinary copyable value (returned by value here, often copied
    // again by callers); a pointer baked in at this point would dangle or alias the wrong vector
    // the moment it is copied. A caller that needs TimelineLayout.h's clip virtualization to
    // honour per-track height sets them itself, immediately before the layoutVisible/
    // hitTestVisibleClip call, from THIS SAME geometry object (see MainComponent.cpp's
    // viewportForClipLayout()) — never store the pointer past that one call.
    return geometry;
}

// N6: builds the Viewport TimelineLayout.h's layoutVisible/hitTestVisibleClip actually need,
// wiring the per-lane arrays from `geometry` (which must outlive the call). Every consumer of
// clip virtualization should go through this instead of touching geometry.viewport directly, so
// the "never store this pointer" rule above has exactly one place to be honoured.
[[nodiscard]] inline Viewport viewportForClipLayout (const TimelineCanvasGeometry& geometry) noexcept
{
    Viewport vp = geometry.viewport;
    vp.laneTopPixels = geometry.laneTopPixels.data();
    vp.laneHeightPixelsPerLane = geometry.laneHeightPixelsPerLane.data();
    return vp;
}

// Marker labels (E7): one geometry law shared by the ruler painter and the gesture hit-test so
// dragging and renaming target exactly the painted label.
inline juce::Rectangle<int> timelineMarkerLabelRect (juce::Rectangle<int> area,
                                                     const TimelineCanvasState& state,
                                                     int markerIndex)
{
    if (state.markers == nullptr || markerIndex < 0 || markerIndex >= state.markerCount)
        return {};

    const TimelineCanvasGeometry geometry = timelineCanvasGeometry (area, state);
    const int x = geometry.clipArea.getX()
                + juce::roundToInt ((state.markers[markerIndex].seconds - geometry.viewport.scrollSeconds)
                                    * geometry.viewport.pixelsPerSecond);
    return { x + UiTheme::Layout::timelineCanvasRulerMarkerLabelLeftInset,
             geometry.rulerArea.getY() + UiTheme::Layout::timelineCanvasRulerMarkerLabelTopInset,
             UiTheme::Layout::timelineCanvasRulerMarkerLabelWidth,
             UiTheme::Layout::timelineCanvasRulerMarkerLabelHeight };
}

// Transport loop brace (E6): one geometry law shared by the painter and the ruler gesture
// hit-test, so the drag handles can never drift from the painted brace.
struct TimelineLoopBraceRects
{
    bool valid = false;
    juce::Rectangle<int> band;
    juce::Rectangle<int> startHandle;
    juce::Rectangle<int> endHandle;
};

inline TimelineLoopBraceRects timelineLoopBraceRects (juce::Rectangle<int> area,
                                                      const TimelineCanvasState& state)
{
    TimelineLoopBraceRects rects;
    if (! state.loopActive || state.loopEndSeconds <= state.loopStartSeconds)
        return rects;

    const TimelineCanvasGeometry geometry = timelineCanvasGeometry (area, state);
    const double pps = geometry.viewport.pixelsPerSecond;
    const int startX = geometry.clipArea.getX()
                     + juce::roundToInt ((state.loopStartSeconds - geometry.viewport.scrollSeconds) * pps);
    const int endX = geometry.clipArea.getX()
                   + juce::roundToInt ((state.loopEndSeconds - geometry.viewport.scrollSeconds) * pps);
    const int left = std::max (geometry.clipArea.getX(), std::min (startX, endX));
    const int right = std::min (geometry.clipArea.getRight(), std::max (startX, endX));
    if (right <= left)
        return rects;

    rects.valid = true;
    rects.band = { left, geometry.rulerArea.getY(), right - left,
                   UiTheme::Layout::timelineCanvasLoopBraceHeight };
    rects.startHandle = { left, geometry.rulerArea.getY(),
                          UiTheme::Layout::timelineCanvasLoopHandleWidth,
                          UiTheme::Layout::timelineCanvasLoopBraceHeight };
    rects.endHandle = { right - UiTheme::Layout::timelineCanvasLoopHandleWidth,
                        geometry.rulerArea.getY(),
                        UiTheme::Layout::timelineCanvasLoopHandleWidth,
                        UiTheme::Layout::timelineCanvasLoopBraceHeight };
    return rects;
}

inline TimelineHitTestResult hitTestTimelineCanvas (juce::Rectangle<int> area,
                                                    const TimelineCanvasState& state,
                                                    juce::Point<int> position)
{
    const TimelineCanvasGeometry geometry = timelineCanvasGeometry (area, state);
    if (state.clips == nullptr || state.clipCount <= 0 || ! geometry.clipArea.contains (position))
        return {};

    return hitTestVisibleClip (
        state.clips,
        state.clipCount,
        viewportForClipLayout (geometry),
        static_cast<double> (position.x - geometry.clipArea.getX()),
        static_cast<double> (position.y - geometry.clipArea.getY()));
}

inline TimelineCanvasPaintStats paintTimelineCanvas (juce::Graphics& g, juce::Rectangle<int> area,
                                                     const TimelineCanvasState& state)
{
    using namespace timeline_canvas_detail;

    TimelineCanvasPaintStats stats;
    stats.visibleClipCapacity = kVisibleClipCapacity;

    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return stats;

    fillPanel (g, area);

    const TimelineCanvasGeometry geometry = timelineCanvasGeometry (area, state);
    const auto clipArea = geometry.clipArea;
    const auto ruler = geometry.rulerArea;
    const auto vp = geometry.viewport;
    // N6: layoutVisible needs the per-lane arrays (viewportForClipLayout wires them from
    // `geometry`, which outlives this whole function) — every OTHER paint helper below still
    // takes the plain `vp` since none of them lay out individual clips by lane index.
    const Viewport clipVp = viewportForClipLayout (geometry);

    drawToolbar (g, geometry.toolbarArea);
    drawRuler (g, ruler, clipArea, state, vp);
    drawGrid (g, clipArea, state, geometry);
    drawRangeSelection (g, ruler, clipArea, state, vp);

    // Transport loop brace (E6): accent band across the upper ruler with brighter end handles.
    if (const TimelineLoopBraceRects loopRects = timelineLoopBraceRects (area, state); loopRects.valid)
    {
        g.setColour (UiTheme::Color::accentTeal().withAlpha (UiTheme::Tone::pressedHighlightAlpha));
        g.fillRect (loopRects.band);
        g.setColour (UiTheme::Color::accentTeal().withAlpha (UiTheme::Tone::focusRingAlpha));
        g.fillRect (loopRects.startHandle);
        g.fillRect (loopRects.endHandle);
        g.drawRect (loopRects.band.toFloat(), UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
    }

    std::array<ElementRect, kVisibleClipCapacity> visible {};
    if (state.clips != nullptr && state.clipCount > 0)
        stats.visibleClips = layoutVisible (state.clips, state.clipCount, clipVp, visible.data(),
                                            static_cast<int> (visible.size()));
    stats.hitVisibleClipCapacity = stats.visibleClips == static_cast<int> (visible.size());

    for (int i = 0; i < stats.visibleClips; ++i)
    {
        const auto& rect = visible[static_cast<std::size_t> (i)];
        auto clipRect = juce::Rectangle<int> (clipArea.getX() + juce::roundToInt (rect.x),
                                             clipArea.getY() + juce::roundToInt (rect.y),
                                             juce::roundToInt (rect.w),
                                             juce::roundToInt (rect.h))
                            .reduced (UiTheme::Space::xs, UiTheme::Space::xs + UiTheme::Space::hairline);
        clipRect = clipRect.getIntersection (clipArea);
        // Vertically scrolled-out rows clamp to empty here (E5): skip their paint work outright.
        if (clipRect.isEmpty())
            continue;
        const auto style = styleForClip (state, rect.id);
        const auto readyCache = state.waveformCacheLookup ? state.waveformCacheLookup (rect.id) : nullptr;
        if (readyCache != nullptr)
        {
            ++stats.readyWaveformClips;
            if (drawClipFrame (g, clipRect, style))
            {
                const Clip* clip = clipForId (state, rect.id);
                if (clip != nullptr)
                {
                    stats.readyWaveformColumns += drawClipCachedWaveform (g, clipRect, style.colour,
                                                                           style.amplitude, *clip,
                                                                           *readyCache, vp);
                }
            }
        }
        else
        {
            const Clip* const pendingClip = clipForId (state, rect.id);
            const bool hasNotes = pendingClip != nullptr
                               && drawClipNotePreviewCount (state, *pendingClip) > 0;
            if (state.waveformCacheLookup && ! hasNotes)
            {
                ++stats.pendingWaveformClips;
                ++stats.placeholderWaveformClips;
            }
            drawClip (g, clipRect, style, state, pendingClip);
        }

        const Clip* const clip = clipForId (state, rect.id);
        if (clip != nullptr && clip->name != nullptr && clip->name[0] != '\0')
        {
            g.setColour (kText);
            g.setFont (UiTheme::Type::font (UiTheme::Type::small, juce::Font::bold));
            g.drawText (clip->name,
                        clipRect.reduced (UiTheme::Space::sm),
                        juce::Justification::topLeft,
                        true);
        }
    }

    drawPlayhead (g, ruler, clipArea, state, vp);

    return stats;
}

} // namespace yesdaw::ui
