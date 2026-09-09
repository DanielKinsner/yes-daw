#pragma once

#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace yesdaw::ui {

// Keep the static canvas between transport ticks. JUCE's native Windows image context
// reads the entire GPU bitmap back to its software backing on destruction; doing that
// on every placement edit dominates the shell's paint budget. Rasterise this cache in
// software instead, leaving the window renderer and the canvas drawing laws unchanged.
class SoftwareCanvasCache final : public juce::CachedComponentImage
{
public:
    explicit SoftwareCanvasCache (juce::Component& component) : owner (component) {}

    void paint (juce::Graphics& destination) override
    {
        const auto bounds = owner.getLocalBounds();
        if (bounds.isEmpty())
            return;

        const float scale = destination.getInternalContext().getPhysicalPixelScaleFactor();
        const auto pixels = bounds * scale;
        const int width = juce::jmax (1, pixels.getWidth());
        const int height = juce::jmax (1, pixels.getHeight());
        if (image.isNull() || image.getWidth() != width || image.getHeight() != height
            || logicalBounds != bounds || cachedScale != scale)
        {
            image = juce::Image (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
            logicalBounds = bounds;
            cachedScale = scale;
            validArea.clear();
        }

        if (! validArea.containsRectangle (bounds))
        {
            {
                juce::Graphics raster (image);
                auto& context = raster.getInternalContext();
                context.addTransform (juce::AffineTransform::scale (scale));
                for (const auto& valid : validArea)
                    context.excludeClipRectangle (valid);
                // Replace dirty pixels, including transparent corners and removed content.
                context.setFill (UiTheme::Color::transparent());
                context.fillRect (bounds, true);
                context.setFill (UiTheme::Color::transparent().withAlpha (UiTheme::Tone::componentVisibleAlpha));
                owner.paintEntireComponent (raster, true);
            }
            validArea = bounds;
        }

        const juce::Graphics::ScopedSaveState saved (destination);
        destination.setColour (UiTheme::Color::transparent().withAlpha (owner.getAlpha()));
        destination.drawImageTransformed (image, juce::AffineTransform::scale (
            static_cast<float> (bounds.getWidth()) / static_cast<float> (width),
            static_cast<float> (bounds.getHeight()) / static_cast<float> (height)), false);
    }

    bool invalidateAll() override { validArea.clear(); return true; }
    bool invalidate (const juce::Rectangle<int>& area) override { validArea.subtract (area); return true; }
    void releaseResources() override { image = {}; validArea.clear(); }

private:
    juce::Component& owner;
    juce::Image image;
    juce::Rectangle<int> logicalBounds;
    float cachedScale = 0.0f;
    juce::RectangleList<int> validArea;
};

} // namespace yesdaw::ui
