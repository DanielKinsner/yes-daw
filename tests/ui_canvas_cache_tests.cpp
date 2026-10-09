#include "ui/SoftwareCanvasCache.h"
#include "ui/TimelineCanvas.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

namespace {

struct CacheTestCanvas final : juce::Component
{
    int paints = 0;
    bool showLeft = true;
    juce::Colour left = juce::Colours::red;
    juce::Colour right = juce::Colours::blue;

    void paint (juce::Graphics& g) override
    {
        ++paints;
        const auto body = getLocalBounds().reduced (2);
        if (showLeft)
        {
            g.setColour (left);
            g.fillRect (body.withRight (getWidth() / 2));
        }
        g.setColour (right);
        g.fillRect (body.withLeft (getWidth() / 2));
    }
};

juce::Image renderCache (yesdaw::ui::SoftwareCanvasCache& cache,
                         const juce::Component& owner, float scale = 1.0f)
{
    const auto pixels = owner.getLocalBounds() * scale;
    juce::Image image (juce::Image::ARGB, std::max (1, pixels.getWidth()),
                       std::max (1, pixels.getHeight()), true, juce::SoftwareImageType());
    {
        juce::Graphics graphics (image);
        graphics.addTransform (juce::AffineTransform::scale (scale));
        cache.paint (graphics);
    }
    return image;
}

juce::Image renderDirect (CacheTestCanvas& owner, float scale)
{
    const auto pixels = owner.getLocalBounds() * scale;
    juce::Image image (juce::Image::ARGB, pixels.getWidth(), pixels.getHeight(),
                       true, juce::SoftwareImageType());
    {
        juce::Graphics graphics (image);
        graphics.addTransform (juce::AffineTransform::scale (scale));
        owner.paintEntireComponent (graphics, false);
    }
    return image;
}

int differingPixels (const juce::Image& left, const juce::Image& right)
{
    REQUIRE (left.getBounds() == right.getBounds());
    int differences = 0;
    for (int y = 0; y < left.getHeight(); ++y)
        for (int x = 0; x < left.getWidth(); ++x)
            if (left.getPixelAt (x, y) != right.getPixelAt (x, y))
                ++differences;
    return differences;
}

} // namespace

TEST_CASE ("software canvas cache retains valid pixels and clears removed transparent content",
           "[ui][input][canvas-cache]")
{
    juce::MessageManager::getInstance();
    CacheTestCanvas owner;
    owner.setSize (32, 24);
    yesdaw::ui::SoftwareCanvasCache cache (owner);

    const auto initial = renderCache (cache, owner);
    REQUIRE (owner.paints == 1);
    REQUIRE (initial.getPixelAt (4, 4) == juce::Colours::red);
    REQUIRE (initial.getPixelAt (24, 4) == juce::Colours::blue);
    REQUIRE (initial.getPixelAt (0, 0).getAlpha() == 0);
    REQUIRE (differingPixels (initial, renderCache (cache, owner)) == 0);
    REQUIRE (owner.paints == 1);

    // Only the left region is dirty. Changing the right paint source makes accidental
    // full repaint observable; its cached blue pixels must remain valid until invalidated.
    owner.showLeft = false;
    owner.right = juce::Colours::yellow;
    REQUIRE (cache.invalidate ({ 0, 0, 16, 24 }));
    const auto partial = renderCache (cache, owner);
    REQUIRE (owner.paints == 2);
    REQUIRE (partial.getPixelAt (4, 4).getAlpha() == 0);
    REQUIRE (partial.getPixelAt (24, 4) == juce::Colours::blue);
    REQUIRE (partial.getPixelAt (0, 0).getAlpha() == 0);
    REQUIRE (differingPixels (partial, renderCache (cache, owner)) == 0);
    REQUIRE (owner.paints == 2);

    REQUIRE (cache.invalidateAll());
    const auto refreshed = renderCache (cache, owner);
    REQUIRE (owner.paints == 3);
    REQUIRE (refreshed.getPixelAt (24, 4) == juce::Colours::yellow);
    REQUIRE (refreshed.getPixelAt (4, 4).getAlpha() == 0);
}

TEST_CASE ("software canvas cache preserves fractional-scale software pixels and rebuilds resources",
           "[ui][input][canvas-cache]")
{
    juce::MessageManager::getInstance();
    CacheTestCanvas owner;
    owner.setSize (32, 24);
    yesdaw::ui::SoftwareCanvasCache cache (owner);

    for (const float scale : { 1.0f, 1.25f, 1.5f, 2.0f })
    {
        INFO ("scale=" << scale);
        const int before = owner.paints;
        const auto cached = renderCache (cache, owner, scale);
        REQUIRE (owner.paints == before + 1);
        REQUIRE (differingPixels (cached, renderCache (cache, owner, scale)) == 0);
        REQUIRE (owner.paints == before + 1);
        const auto direct = renderDirect (owner, scale);
        REQUIRE (differingPixels (cached, direct) == 0);
    }

    owner.setSize (48, 32);
    const int beforeResize = owner.paints;
    const auto resized = renderCache (cache, owner, 1.5f);
    REQUIRE (owner.paints == beforeResize + 1);
    REQUIRE (resized.getWidth() == 72);
    REQUIRE (resized.getHeight() == 48);
    REQUIRE (resized.getPixelAt (60, 12) == juce::Colours::blue);
    cache.releaseResources();
    REQUIRE (differingPixels (resized, renderCache (cache, owner, 1.5f)) == 0);
    REQUIRE (owner.paints == beforeResize + 2);

    owner.setSize (0, 0);
    const int beforeEmpty = owner.paints;
    const auto empty = renderCache (cache, owner);
    REQUIRE (owner.paints == beforeEmpty);
    REQUIRE (empty.getPixelAt (0, 0).getAlpha() == 0);
}

TEST_CASE ("software canvas cache applies component alpha once when compositing",
           "[ui][input][canvas-cache]")
{
    juce::MessageManager::getInstance();
    CacheTestCanvas owner;
    owner.setSize (32, 24);
    owner.setAlpha (0.5f);
    yesdaw::ui::SoftwareCanvasCache cache (owner);
    const auto image = renderCache (cache, owner);
    const auto alpha = image.getPixelAt (4, 4).getAlpha();
    REQUIRE (alpha >= 126);
    REQUIRE (alpha <= 129);
    REQUIRE (image.getPixelAt (0, 0).getAlpha() == 0);
}

TEST_CASE ("timeline panel omits only pixels hidden by its opaque content",
           "[ui][input][canvas-cache][canvas-panel-occlusion]")
{
    juce::MessageManager::getInstance();
    using namespace yesdaw::ui;
    TimelineCanvasState state;
    state.paintPlayhead = false;
    state.trackCount = 8;
    state.trackScrollRows = 2;
    state.viewport.scrollSeconds = 0.375;
    state.viewport.pixelsPerSecond = 73.0;

    // The legacy painter's complete background pass. The following clip/overlay stages
    // are empty in this fixture; production paintTimelineCanvas supplies the new pass.
    const auto legacy = [&state] (juce::Graphics& g, juce::Rectangle<int> area) {
        using namespace timeline_canvas_detail;
        fillPanel (g, area);
        const auto geometry = timelineCanvasGeometry (area, state);
        drawToolbar (g, geometry.toolbarArea, state.activeTool, state.snapLabelShown);
        drawRuler (g, geometry.rulerArea, geometry.clipArea, state, geometry.viewport);
        drawGrid (g, geometry.clipArea, state, geometry);
    };

    for (const auto area : { juce::Rectangle<int> { 7, 9, 321, 239 },
                             juce::Rectangle<int> { 13, 5, 1001, 701 },
                             juce::Rectangle<int> { 5, 7, 47, 33 },
                             juce::Rectangle<int> { 3, 11, 3, 3 } })
    {
        INFO ("area=" << area.toString().toStdString());
        const auto geometry = timelineCanvasGeometry (area, state);
        for (const float scale : { 1.0f, 1.25f, 1.5f, 2.0f })
        {
            // Full repaint, a narrow strip across the band joins, and a corner-only repaint.
            for (const auto clip : { area, area.withWidth (7).withX (area.getCentreX()),
                                     area.withSize (17, 19) })
            {
                INFO ("area=" << area.toString().toStdString() << " scale=" << scale
                               << " clip=" << clip.toString().toStdString());
                const auto render = [&] (bool old) {
                    const auto pixels = juce::Rectangle<int> { 0, 0, area.getRight() + 3, area.getBottom() + 3 } * scale;
                    juce::Image image (juce::Image::ARGB, pixels.getWidth(), pixels.getHeight(), true,
                                       juce::SoftwareImageType());
                    juce::Graphics g (image);
                    g.addTransform (juce::AffineTransform::scale (scale));
                    g.reduceClipRegion (area);
                    g.reduceClipRegion (clip);
                    if (old)
                        legacy (g, area);
                    else
                        (void) paintTimelineCanvas (g, area, state);
                    return image;
                };
                REQUIRE (differingPixels (render (true), render (false)) == 0);
            }
        }

        // This assertion bites if the optimization silently returns to full panel painting.
        // It also makes the saved scope observable: later opaque fills still reach the interior.
        juce::Image panel (juce::Image::ARGB, area.getRight() + 3, area.getBottom() + 3, true,
                           juce::SoftwareImageType());
        juce::Graphics g (panel);
        timeline_canvas_detail::fillVisiblePanel (g, area, geometry);
        juce::Image oldPanel (juce::Image::ARGB, panel.getWidth(), panel.getHeight(), true,
                              juce::SoftwareImageType());
        juce::Graphics oldGraphics (oldPanel);
        timeline_canvas_detail::fillPanel (oldGraphics, area);
        for (const auto covered : { geometry.toolbarArea, geometry.rulerArea, geometry.clipArea })
        {
            const auto interior = covered.reduced (UiTheme::Space::hairline).getIntersection (area);
            if (! interior.isEmpty())
            {
                REQUIRE (panel.getPixelAt (interior.getCentreX(), interior.getCentreY()).getAlpha() == 0);
                REQUIRE (oldPanel.getPixelAt (interior.getCentreX(), interior.getCentreY()).getAlpha() > 0);
            }
        }
        REQUIRE (panel.getPixelAt (area.getX(), area.getY()) == oldPanel.getPixelAt (area.getX(), area.getY()));
        if (area.getWidth() > UiTheme::Radius::lg * 2 && area.getHeight() > UiTheme::Radius::lg * 2)
            REQUIRE (panel.getPixelAt (area.getX(), area.getY()).getAlpha() == 0);
    }
}
