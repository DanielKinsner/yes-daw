// YES DAW - H16 UI design-token surface.
//
// Raw visual constants live here so UI code can be audited mechanically.

#pragma once

#include "ui/UiThemeLayout.h"

#include <juce_graphics/juce_graphics.h>

#include <array>
#include <cstdint>

namespace yesdaw::ui {

struct UiTheme
{
    struct Color
    {
        static juce::Colour appBackground() noexcept { return juce::Colour (0xff070a0d); }
        static juce::Colour panel() noexcept { return juce::Colour (0xff0e1318); }
        static juce::Colour panelRaised() noexcept { return juce::Colour (0xff141a20); }
        static juce::Colour panelStroke() noexcept { return juce::Colour (0xff26303a); }
        static juce::Colour panelInnerHighlight() noexcept { return juce::Colour (0xff35414c); }
        static juce::Colour panelShadow() noexcept { return juce::Colour (0xff030507); }
        static juce::Colour text() noexcept { return juce::Colour (0xffedf2f6); }
        static juce::Colour mutedText() noexcept { return juce::Colour (0xff87929d); }
        static juce::Colour faintText() noexcept { return juce::Colour (0xff5e6974); }
        static juce::Colour white() noexcept { return juce::Colour (0xffffffff); }

        static juce::Colour accentBlue() noexcept { return juce::Colour (0xff3b8cff); }
        static juce::Colour accentTeal() noexcept { return juce::Colour (0xff1bb5a6); }
        static juce::Colour accentAmber() noexcept { return juce::Colour (0xffd29118); }
        static juce::Colour accentPurple() noexcept { return juce::Colour (0xffa578ff); }
        static juce::Colour accentPurpleDeep() noexcept { return juce::Colour (0xff35234f); }
        static juce::Colour accentPurpleGlow() noexcept { return juce::Colour (0xffc3a5ff); }
        static juce::Colour accentCyan() noexcept { return juce::Colour (0xff20c8d8); }
        static juce::Colour meterGreen() noexcept { return juce::Colour (0xff74df35); }
        static juce::Colour meterYellow() noexcept { return juce::Colour (0xffe2c832); }
        static juce::Colour dangerRed() noexcept { return juce::Colour (0xffff5757); }

        static juce::Colour timelineGrid() noexcept { return juce::Colour (0xff202a33); }
        static juce::Colour timelineCanvas() noexcept { return juce::Colour (0xff090e12); }
        static juce::Colour timelineToolbar() noexcept { return juce::Colour (0xff11171d); }
        static juce::Colour timelineRuler() noexcept { return juce::Colour (0xff0b1014); }

        static juce::Colour controlInset() noexcept { return juce::Colour (0xff0a0f13); }
        static juce::Colour controlInsetDeep() noexcept { return juce::Colour (0xff04070a); }
        static juce::Colour controlInsetBlack() noexcept { return juce::Colour (0xff070b10); }
        static juce::Colour toolButton() noexcept { return juce::Colour (0xff151c23); }
        static juce::Colour snapField() noexcept { return juce::Colour (0xff070b0f); }
        static juce::Colour buttonSurface() noexcept { return juce::Colour (0xff192129); }
        static juce::Colour buttonSurfaceTop() noexcept { return juce::Colour (0xff222c35); }
        static juce::Colour buttonPressed() noexcept { return juce::Colour (0xff0f151a); }
        static juce::Colour buttonBorder() noexcept { return juce::Colour (0xff3a4651); }
        static juce::Colour buttonTextMuted() noexcept { return juce::Colour (0xffb6c0c9); }
        static juce::Colour focusRing() noexcept { return accentPurpleGlow(); }
        static juce::Colour darkControl() noexcept { return juce::Colour (0xff10161c); }
        static juce::Colour warningButton() noexcept { return juce::Colour (0xff201b13); }
        static juce::Colour separator() noexcept { return juce::Colour (0xff1b242c); }
        static juce::Colour canvasLayer() noexcept { return juce::Colour (0xff0d1218); }
        static juce::Colour selectedLane() noexcept { return juce::Colour (0xff20182c); }
        static juce::Colour mixerBack() noexcept { return juce::Colour (0xff080c10); }
        static juce::Colour pianoBlackKey() noexcept { return juce::Colour (0xff0a0e13); }
        // M8: a piano roll needs to READ as a keyboard. The white keys were painted in the panel's
        // raised grey, so the column looked like striped rows rather than keys.
        static juce::Colour pianoWhiteKey() noexcept { return juce::Colour (0xffd8dde6); }
        static juce::Colour pianoWhiteKeyText() noexcept { return juce::Colour (0xff2a3038); }
        static juce::Colour pianoGridStrong() noexcept { return juce::Colour (0xff344150); }
        static juce::Colour pianoGridWeak() noexcept { return juce::Colour (0xff202a34); }
        static juce::Colour inspectorTab() noexcept { return juce::Colour (0xff151a22); }
        static juce::Colour selectedStrip() noexcept { return juce::Colour (0xff1c1428); }
        static juce::Colour faderThumb() noexcept { return juce::Colour (0xffc4c9cf); }
        static juce::Colour faderThumbTop() noexcept { return juce::Colour (0xfff0f3f5); }
        static juce::Colour meterTrack() noexcept { return juce::Colour (0xff030608); }
        static juce::Colour knobFace() noexcept { return juce::Colour (0xff171e24); }
        static juce::Colour knobArc() noexcept { return juce::Colour (0xff4a5661); }
        static juce::Colour transparent() noexcept { return juce::Colour (0x00000000); }
    };

    struct Meter
    {
        static juce::Colour nominalFill() noexcept { return Color::meterGreen(); }
        static juce::Colour hotFill() noexcept { return Color::meterYellow(); }
        // Latched clip indicator (B32): an exact, scan-friendly red so gates can pixel-assert it.
        static juce::Colour clipFill() noexcept { return juce::Colour (0xffff1f1f); }

        static constexpr float verticalHotBand = 0.22f;
        static constexpr float horizontalHotBand = 0.18f;
        // Peak-hold and clip-latch law (B32): the held peak survives this many 33 ms UI refresh
        // ticks (~2 s); the clip light latches at or above this linear peak (0 dBFS).
        static constexpr int peakHoldTicks = 60;
        static constexpr float clipThreshold = 1.0f;
        static constexpr int clipLightSize = 6;      // square clip cell at the meter's hot end
        static constexpr int peakTickThickness = 2;  // held-peak marker line
    };

    struct Mixer
    {
        static constexpr float paintedReadoutGainFloor = 0.0001f;
    };

    struct Tone
    {
        // Velocity tints the painted piano-roll note (B33): brightness scales from this floor at
        // velocity 0 up to full at velocity 1.
        static constexpr float noteVelocityTintFloor = 0.45f;
        static constexpr float disabledAlpha = 0.38f;
        static constexpr float componentHiddenAlpha = 0.0f;
        static constexpr float componentVisibleAlpha = 1.0f;
        static constexpr float mutedControlAlpha = 0.72f;
        static constexpr float hoverHighlightAlpha = 0.10f;
        static constexpr float pressedHighlightAlpha = 0.16f;
        static constexpr float innerHighlightAlpha = 0.46f;
        static constexpr float shadowAlpha = 0.72f;
        static constexpr float focusRingAlpha = 0.80f;
        static constexpr float trackIconAlpha = 0.92f;
        static constexpr float trackSliderFillAlpha = 0.76f;
        static constexpr float trackSliderRailAlpha = 0.90f;
        static constexpr float mixerHeaderAlpha = 0.36f;
        static constexpr float mixerKnobHighlightAlpha = 0.75f;
        static constexpr float clipSurfaceTopAlpha = 0.22f;
        static constexpr float timelineCanvasClipSurfaceTopBrightness = 0.10f;
        static constexpr float timelineCanvasClipSurfaceTopAlpha = 0.58f;
        static constexpr std::array<float, 6> inspectorAutomationValues {{
            0.72f, 0.32f, 0.58f, 0.44f, 0.70f, 0.62f
        }};
        static constexpr float timelineCanvasFallbackClipAmplitude = 0.7f;
        static constexpr float timelineCanvasWaveformBrightness = 0.42f;
        static constexpr float timelineCanvasCompactClipAlpha = 0.44f;
        static constexpr float timelineCanvasCompactHighlightBrightness = 0.3f;
        static constexpr float timelineCanvasClipFillAlpha = 0.42f;
        static constexpr float timelineCanvasClipOutlineBrightness = 0.35f;
        static constexpr float timelineCanvasRulerTickAlpha = 0.65f;
        static constexpr float timelineCanvasGridLaneSeparatorAlpha = 0.7f;
        static constexpr float timelineCanvasGridTrackTintAlpha = 0.20f;
        static constexpr float timelineCanvasGridMajorLineBrightness = 0.25f;
        static constexpr float timelineCanvasGridMinorLineAlpha = 0.38f;
        static constexpr float mainComponentProjectClipAlpha = 0.82f;
        // V6: the fade region shades the clip body above the gain curve at this alpha.
        static constexpr float timelineCanvasFadeShadeAlpha = 0.45f;
    };

    struct Space
    {
        static constexpr int none = 0;
        static constexpr int hairline = 1;
        static constexpr int xxs = 2;
        static constexpr int xs = 4;
        static constexpr int sm = 6;
        static constexpr int md = 8;
        static constexpr int lg = 12;
        static constexpr int xl = 16;
    };

    struct Radius
    {
        static constexpr float none = 0.0f;
        static constexpr float xs = 2.0f;
        static constexpr float sm = 4.0f;
        static constexpr float md = 5.0f;
        static constexpr float panel = 7.0f;
        static constexpr float lg = 9.0f;
        static constexpr float pill = 12.0f;
    };

    struct Type
    {
        static constexpr float tiny = 9.0f;
        static constexpr float caption = 10.5f;
        static constexpr float small = 11.5f;
        static constexpr float body = 13.0f;
        static constexpr float title = 14.0f;
        static constexpr float readout = 17.0f;
        static constexpr float statusIcon = 19.0f;
        static constexpr float transportClock = 27.0f;

        static juce::Font font (float height, int styleFlags = juce::Font::plain)
        {
           #if JUCE_WINDOWS
            static const juce::String family = []
            {
                const auto installed = juce::Font::findAllTypefaceNames();
                return installed.contains ("Segoe UI Variable", true)
                         ? juce::String { "Segoe UI Variable" }
                         : juce::Font::getDefaultSansSerifFontName();
            }();
           #else
            static const juce::String family = juce::Font::getDefaultSansSerifFontName();
           #endif
            return juce::Font (juce::FontOptions (family, height, styleFlags));
        }

        static juce::Font numericFont (float height, int styleFlags = juce::Font::plain)
        {
           #if JUCE_WINDOWS
            static const juce::String family = []
            {
                const auto installed = juce::Font::findAllTypefaceNames();
                return installed.contains ("Cascadia Mono", true)
                         ? juce::String { "Cascadia Mono" }
                         : juce::Font::getDefaultMonospacedFontName();
            }();
           #else
            static const juce::String family = juce::Font::getDefaultMonospacedFontName();
           #endif
            return juce::Font (juce::FontOptions (family, height, styleFlags));
        }
    };

    struct Layout
    {
        static constexpr int headerHeight = 118;   // three control rows; row 3 hosts export/device/recording
        static constexpr int defaultWindowWidth = 1536;
        static constexpr int defaultWindowHeight = 960;
        // E27: the window resize floor is the smallest size every shipped layout stays honest
        // at (the judged laptop size) — below it the header rows and panels collide.
        static constexpr int windowMinWidth = 1152;
        static constexpr int windowMinHeight = 720;
        static constexpr int windowMaxWidth = 8192;
        static constexpr int windowMaxHeight = 4320;
        static constexpr int leftRailWidth = 318;
        static constexpr int inspectorWidth = 320;
        static constexpr int mixerHeight = 260;
        static juce::Rectangle<int> headerMenuBarBounds() noexcept { return { 22, 14, 320, 24 }; }
        static constexpr int headerMenuStartX = 22;
        static constexpr int headerMenuY = 17;
        static constexpr int headerMenuWidth = 70;
        static constexpr int headerMenuHeight = 18;
        static constexpr int headerMenuStep = 48;
        static constexpr int headerOptionsMenuStep = 72;
        static juce::Rectangle<int> headerProjectSectionBounds() noexcept
        {
            return { 10, 43, 310, 37 };
        }
        static juce::Rectangle<int> headerTransportSectionBounds() noexcept
        {
            return { 328, 10, 752, 68 };
        }
        static juce::Rectangle<int> headerMasterSectionBounds() noexcept
        {
            return { 1092, 10, 434, 68 };
        }
        static constexpr int headerTransportRecordX = 520;
        static constexpr int headerTransportRecordY = 36;
        static constexpr int headerTransportRecordSize = 18;
        static constexpr int headerTransportTimeX = 570;
        static constexpr int headerTransportReadoutY = 16;
        static constexpr int headerTransportTimeWidth = 190;
        static constexpr int headerTransportReadoutHeight = 56;
        static constexpr int headerTransportTextInsetX = 8;
        static constexpr int headerTransportClockInsetY = 4;
        static constexpr int headerTransportClockHeight = 30;
        static constexpr int headerTransportLabelInsetY = 34;
        static constexpr int headerTransportBoxX = 760;
        // V2: was 248 (3 cells: TEMPO, TIME SIG, KEY) — the KEY cell was a dead literal with no
        // backing model (D3) and is gone; 164 = 2 cells exactly, so no dead space is left where
        // the third cell used to be.
        static constexpr int headerTransportBoxWidth = 164;
        static constexpr int headerTransportCellWidth = 82;
        static constexpr int headerTransportCellInsetX = 4;
        static constexpr int headerTransportValueInsetY = 8;
        static constexpr int headerTransportValueHeight = 24;
        static constexpr int headerMasterX = 1110;
        static constexpr int headerMasterY = 18;
        static constexpr int headerMasterWidth = 300;
        static constexpr int headerMasterHeight = 44;
        static constexpr int headerMasterLabelHeight = 14;
        static constexpr int headerMasterMeterHeight = 16;
        static constexpr int headerMasterMeterWidth = 236;
        static constexpr int headerMasterLufsX = 1370;
        static constexpr int headerMasterLufsY = 33;
        static constexpr int headerMasterLufsWidth = 76;
        static constexpr int headerMasterLufsHeight = 16;
        // M9: the master card is RIGHT-anchored now. Below this width it drops WHOLE (label
        // included) instead of keeping a label over a clipped meter — E27's whole-section law,
        // applied to the header.
        static constexpr int headerMasterMinWidth = 150;
        static constexpr int headerMasterGearGap = 12;
        static constexpr int headerMasterLufsGap = 8;
        static constexpr int headerStatusIconRightInset = 54;
        static constexpr int headerStatusIconY = 34;
        static constexpr int headerStatusIconSize = 24;
        static constexpr int shellHeaderSeparatorHeight = 1;
        static constexpr int shellPanelHorizontalInset = 6;
        static constexpr int shellPanelVerticalInset = 10;
        static constexpr int mixerPanelHorizontalInset = 6;
        static constexpr int mixerPanelVerticalInset = 8;
        static constexpr float panelOutlineInset = 0.5f;
        static constexpr float panelOutlineStrokeWidth = 1.0f;
        static constexpr int controlShadowOffset = 2;
        static constexpr float controlOutlineInset = 0.5f;
        static constexpr float controlOutlineStrokeWidth = 1.0f;
        static constexpr int controlInnerHighlightHeight = 1;
        static constexpr int controlTextHorizontalInset = 8;
        static constexpr int controlIconInset = 7;
        static constexpr int controlIconTextGap = 5;
        static constexpr int controlFocusInset = 1;
        static constexpr float controlFocusStrokeWidth = 1.5f;
        static constexpr int sliderTrackThickness = 5;
        static constexpr int sliderThumbDiameter = 14;
        static constexpr int sliderThumbShortSide = 16;
        static constexpr int sliderThumbLongSide = 28;
        static constexpr int sliderThumbHighlightHeight = 2;
        static constexpr int comboArrowWidth = 10;
        static constexpr int comboArrowHeight = 6;
        static constexpr int comboArrowRightInset = 12;
        static constexpr float iconStrokeWidth = 1.45f;
        static constexpr float iconFineStrokeWidth = 1.1f;
        static constexpr float iconBoldStrokeWidth = 1.8f;
        static juce::Rectangle<int> projectNewButtonBounds() noexcept { return { 16, 50, 30, 26 }; }
        static juce::Rectangle<int> projectOpenButtonBounds() noexcept { return { 50, 50, 30, 26 }; }
        static juce::Rectangle<int> projectSaveButtonBounds() noexcept { return { 84, 50, 30, 26 }; }
        static juce::Rectangle<int> projectImportAudioButtonBounds() noexcept { return { 118, 50, 30, 26 }; }
        static juce::Rectangle<int> projectExportAudioButtonBounds() noexcept { return { 156, 50, 88, 26 }; }
        static juce::Rectangle<int> projectExportAudioProgressBounds() noexcept { return { 156, 50, 60, 26 }; }
        static juce::Rectangle<int> projectExportAudioCancelButtonBounds() noexcept { return { 218, 50, 30, 26 }; }
        // Header row 3 (y = 84): export options, the audio device chooser, and the recording
        // cluster live INSIDE the header instead of floating over the track rail and timeline.
        static juce::Rectangle<int> exportBitDepthChooserBounds() noexcept { return { 156, 84, 108, 26 }; }
        static juce::Rectangle<int> exportRangeChooserBounds() noexcept { return { 268, 84, 112, 26 }; }
        // E29: the device row carries BOTH sides — output chooser, input chooser, and the
        // recorded-channel pick — inside the window floor (right edge 1140 ≤ 1152).
        static juce::Rectangle<int> audioDeviceChooserBounds() noexcept { return { 388, 84, 150, 26 }; }
        static juce::Rectangle<int> audioInputDeviceChooserBounds() noexcept { return { 542, 84, 150, 26 }; }
        static juce::Rectangle<int> recordingInputChannelChooserBounds() noexcept { return { 696, 84, 64, 26 }; }
        static juce::Rectangle<int> deviceRefreshAudioButtonBounds() noexcept { return { 764, 84, 76, 26 }; }
        static juce::Rectangle<int> deviceSelectTestAudioButtonBounds() noexcept { return { 844, 84, 90, 26 }; }
        static juce::Rectangle<int> recordingArmTrackButtonBounds() noexcept { return { 938, 84, 56, 26 }; }
        static juce::Rectangle<int> recordingSetMonitoringPolicyButtonBounds() noexcept { return { 998, 84, 82, 26 }; }
        static juce::Rectangle<int> recordingAssembleCompButtonBounds() noexcept { return { 1084, 84, 60, 26 }; }
        static juce::Rectangle<int> transportRecordButtonBounds() noexcept { return { 504, 16, 56, 56 }; }
        static juce::Rectangle<int> editUndoButtonBounds() noexcept { return { 256, 50, 28, 26 }; }
        static juce::Rectangle<int> editRedoButtonBounds() noexcept { return { 288, 50, 28, 26 }; }
        static juce::Rectangle<int> transportLocateStartButtonBounds() noexcept { return { 336, 16, 56, 56 }; }
        static juce::Rectangle<int> transportPlayButtonBounds() noexcept { return { 392, 16, 56, 56 }; }
        static juce::Rectangle<int> transportStopButtonBounds() noexcept { return { 448, 16, 56, 56 }; }
        static juce::Rectangle<int> transportToggleLoopButtonBounds() noexcept { return { 1008, 16, 64, 56 }; }
        static juce::Rectangle<int> viewMixerButtonBounds (juce::Rectangle<int> mixer) noexcept
        {
            return { mixer.getX() + 10, mixer.getY() + 10, 76, 28 };
        }
        static juce::Rectangle<int> viewPianoRollButtonBounds (juce::Rectangle<int> mixer) noexcept
        {
            return { mixer.getX() + 90, mixer.getY() + 10, 78, 28 };
        }
        static juce::Rectangle<int> autosaveRestoreButtonBounds() noexcept { return { 1180, 50, 132, 26 }; }
        static juce::Rectangle<int> autosaveDiscardButtonBounds() noexcept { return { 1316, 50, 132, 26 }; }

        static constexpr int inspectorTabHeight = 40;
        static constexpr int inspectorTabCount = 2;
        static constexpr int inspectorContentInsetX = 16;
        static constexpr int inspectorContentInsetY = 14;
        static constexpr int inspectorTitleAccentTopInset = 4;
        static constexpr int inspectorTitleAccentSize = 12;
        static constexpr int inspectorTitleTextLeftInset = 20;
        static constexpr int inspectorTitleTextHeight = 24;
        static constexpr int inspectorStatsSectionTop = 42;
        static constexpr int inspectorStatsSectionHeight = 46;
        static constexpr int inspectorStatsColumnCount = 3;
        static constexpr int inspectorStatsCellInsetX = 2;
        static constexpr int inspectorStatsCellInsetY = 0;
        static constexpr int inspectorStatsTextInset = 2;
        static constexpr int inspectorStatsLabelHeight = 16;
        static constexpr int inspectorStatsValueHeight = 18;
        static constexpr int inspectorTimingControlInsetX = 2;
        static constexpr int inspectorTimingControlInsetY = 4;
        static constexpr double inspectorTimeSliderMinSeconds = 0.0;
        static constexpr double inspectorTimeSliderMaxSecondsFallback = 1.0;
        static constexpr double inspectorTimeSliderRangePaddingScale = 1.25;
        static constexpr double inspectorTimeSliderIntervalSeconds = 0.001;
        static constexpr double inspectorTimeSliderDefaultSeconds = 0.0;
        static constexpr int inspectorSectionLabelHeight = 20;
        static constexpr int inspectorGainSectionTop = 118;
        static constexpr int inspectorGainSectionHeight = 84;
        static constexpr int inspectorGainControlTopInset = 28;
        static constexpr int inspectorGainControlHeight = 24;
        static constexpr int inspectorGainControlLeftInset = 72;
        static constexpr int inspectorGainReadoutLeftInset = 72;
        static constexpr int inspectorGainReadoutHeight = 22;
        static constexpr double inspectorGainSliderMin = 0.0;
        static constexpr double inspectorGainSliderMax = 2.0;
        static constexpr double inspectorGainSliderInterval = 0.01;
        static constexpr double inspectorGainSliderDefault = 1.0;
        static constexpr float inspectorGainReadoutDefault = 1.0f;
        static constexpr double inspectorReadoutFallbackSampleRate = 48000.0;
        static constexpr int inspectorFadesSectionTop = 214;
        static constexpr int inspectorFadesSectionHeight = 124;
        static constexpr int inspectorFadesControlTopInset = 22;
        static constexpr int inspectorFadeControlHeight = 32;
        static constexpr int inspectorFadeCurveControlTopGap = 4;
        static constexpr int inspectorFadeCurveControlHeight = 26;
        // E24: the fade sliders start past the painted "Fade In 0.000 s" readout, never over it.
        static constexpr int inspectorFadeControlLeftInset = 150;
        static constexpr int inspectorFadeControlHorizontalInset = 0;
        static constexpr int inspectorFadeControlVerticalInset = 6;
        static constexpr double inspectorFadeSliderMinSeconds = 0.0;
        static constexpr double inspectorFadeSliderMaxSeconds = 1.0;
        static constexpr double inspectorFadeSliderIntervalSeconds = 0.001;
        static constexpr double inspectorFadeSliderDefaultSeconds = 0.0;
        static constexpr double inspectorFadeReadoutDefaultSeconds = inspectorFadeSliderDefaultSeconds;
        static constexpr int inspectorFadeRowHeight = 32;
        static constexpr int inspectorFadeRowInsetX = 0;
        static constexpr int inspectorFadeRowInsetY = 3;
        static constexpr int inspectorFadeTextInsetX = 8;
        static constexpr int inspectorFadeTextInsetY = 0;
        // E33: CLIP FX is a two-line stub — its height moved to the TAKES section below it.
        static constexpr int inspectorFxSectionTop = 360;
        static constexpr int inspectorFxSectionHeight = 60;
        static constexpr int inspectorFxRowHeight = 28;
        static constexpr int inspectorFxRowInsetX = 0;
        static constexpr int inspectorFxRowInsetY = 2;
        static constexpr int inspectorFxTextInsetX = 10;
        static constexpr int inspectorFxTextInsetY = 0;
        static constexpr int inspectorAutomationSectionTop = 426;
        static constexpr int inspectorAutomationChartTop = 4;
        static constexpr int inspectorAutomationChartHeight = 44;
        static constexpr int inspectorAutomationChartInsetX = 6;
        static constexpr int inspectorAutomationChartInsetY = 4;
        static constexpr int inspectorAutomationPointCount = 6;
        static constexpr float inspectorAutomationPointRadius = 2.5f;
        static constexpr float inspectorAutomationPathStrokeWidth = 1.4f;
        static constexpr int hiddenSliderTextBoxWidth = 0;
        static constexpr int hiddenSliderTextBoxHeight = 0;

        static constexpr int mixerToolsWidth = 180;
        static constexpr int mixerStripMinWidth = 84;
        static constexpr int mixerStripHorizontalInset = 3;
        static constexpr int mixerStripVerticalInset = 0;
        static constexpr int mixerControlLaneInsetX = 8;
        static constexpr int mixerControlLaneInsetY = 6;
        static constexpr int mixerTrackSelectHeight = 26;
        static constexpr int mixerTrackSelectBottomGap = 7;
        static constexpr int mixerPanHeight = 34;
        static constexpr int mixerPanInsetX = 2;
        static constexpr int mixerPanInsetY = 6;
        static constexpr int mixerButtonRowHeight = 30;
        static constexpr int mixerButtonRowInsetX = 5;
        static constexpr int mixerButtonRowInsetY = 4;
        static constexpr int mixerButtonWidth = 30;
        static constexpr int mixerButtonBottomGap = 4;
        static constexpr int mixerFaderMinHeight = 72;
        static constexpr int mixerFaderBottomReserve = 18;
        static constexpr int mixerFaderWidth = 42;
        static constexpr double mixerFaderSliderMin = 0.0;
        static constexpr double mixerFaderSliderMax = 2.0;
        static constexpr double mixerFaderSliderInterval = 0.01;
        static constexpr double mixerFaderSliderDefault = 1.0;
        static constexpr double mixerPanSliderMin = -1.0;
        static constexpr double mixerPanSliderMax = 1.0;
        static constexpr double mixerPanSliderInterval = 0.01;
        static constexpr double mixerPanSliderDefault = 0.0;
        // Shift-drag fine-adjust scale: pointer movement counts for exactly this fraction of its
        // plain effect on every slider, fader, knob, and rail mini (10x finer).
        static constexpr double fineDragScale = 0.1;
        // Live dB readout shown while a gain control is dragged (B31).
        static constexpr int dbReadoutWidth = 64;
        static constexpr int dbReadoutHeight = 16;
        // Alt+wheel velocity editing on a piano-roll note (B33): normalized velocity change per
        // unit of vertical wheel delta.
        static constexpr double pianoRollVelocityWheelScale = 0.5;
        static constexpr int mixerToolsInsetX = 8;
        static constexpr int mixerToolsInsetY = 0;
        static constexpr int mixerToolsSendsLabelTop = 52;
        static constexpr int mixerToolsViewLabelTop = 96;
        static constexpr int mixerToolsModeLabelTop = 120;
        static constexpr int mixerToolsLabelHeight = 24;
        static constexpr int mixerToolsModeLabelHeight = 28;
        static constexpr int mixerToolsLabelInsetX = 12;
        static constexpr int mixerToolsLabelInsetY = 0;
        static constexpr int mixerUtilityTop = 44;
        static constexpr int mixerUtilityHeight = 24;
        static constexpr int mixerUtilityGap = 3;
        static constexpr int mixerUtilityInsetX = 10;
        // V3: the mixer dock show/hide toggle, anchored just above the dock's current top edge.
        static constexpr int mixerDockToggleWidth = 120;
        static constexpr int mixerDockToggleHeight = 24;
        static constexpr int mixerDockToggleBottomGap = 4;
        static constexpr int mixerPaintedStripMinWidth = 84;
        // N3: widened from 112 so a small track count still fills a real share of the panel
        // instead of clamping to a sliver strip band with dead space beyond it. Judged against
        // Logic's mixer at all three D7 sizes: 156 still left the panel reading over half-empty
        // at 1920x1080 with 3 tracks; 220 fills 1152x720 completely and covers a legible
        // majority of 1920x1080 without individual strips reading as stretched or oversized.
        static constexpr int mixerPaintedStripMaxWidth = 220;
        static constexpr int mixerPaintedStripMinCount = 1;
        static constexpr int mixerPaintedStripExtraSlotCount = 1;
        static constexpr int mixerPaintedStripInsetX = 3;
        static constexpr int mixerPaintedStripInsetY = 0;
        static constexpr float mixerPaintedStripOutlineInset = 0.5f;
        static constexpr float mixerPaintedStripSelectedStrokeWidth = 2.0f;
        static constexpr float mixerPaintedStripStrokeWidth = 1.0f;
        static constexpr int mixerPaintedHeaderHeight = 28;
        static constexpr int mixerPaintedNameInsetX = 8;
        static constexpr int mixerPaintedNameInsetY = 4;
        static constexpr int mixerPaintedNameHeight = 20;
        static constexpr int mixerPaintedPanTop = 36;
        static constexpr int mixerPaintedPanHeight = 38;
        static constexpr int mixerPaintedPanRadius = 13;
        static constexpr int mixerPaintedPanTopInset = 4;
        static constexpr float mixerPaintedPanStrokeWidth = 1.2f;
        static constexpr int mixerPaintedButtonsTop = 78;
        static constexpr int mixerPaintedButtonsHeight = 28;
        static constexpr int mixerPaintedButtonsInsetX = 14;
        static constexpr int mixerPaintedButtonsInsetY = 0;
        static constexpr int mixerPaintedButtonWidth = 30;
        static constexpr int mixerPaintedButtonInsetX = 3;
        static constexpr int mixerPaintedButtonInsetY = 2;
        static constexpr int mixerPaintedSidechainTop = 106;
        static constexpr int mixerPaintedSidechainHeight = 14;
        static constexpr int mixerPaintedSidechainLeftInset = 8;
        static constexpr int mixerPaintedSidechainWidth = 28;
        // M4: the insert-slot column lives between the S/M row and the fader — a mixer strip
        // shows its FX chain, it does not hide it in a side panel.
        static constexpr int mixerPaintedInsertsTop = 112;
        static constexpr int mixerPaintedInsertRowHeight = 15;
        static constexpr int mixerPaintedInsertRowGap = 2;
        static constexpr int mixerPaintedInsertRowCount = 4;
        static constexpr int mixerPaintedInsertsInsetX = 6;
        static constexpr int mixerPaintedInsertsHeight =
            mixerPaintedInsertRowCount * (mixerPaintedInsertRowHeight + mixerPaintedInsertRowGap);
        static constexpr int mixerPaintedInsertBypassDotInset = 4;
        static constexpr int mixerPaintedInsertBypassDotSize = 5;
        static constexpr int mixerPaintedInsertLabelInsetX = 12;
        static constexpr int mixerPaintedInsertsFaderGap = 6;
        // The shortest fader rail worth painting. A strip too short for slot rows drops them and
        // hands the space back to the fader (the timeline view's mini-mixer is that short).
        static constexpr int mixerPaintedFaderMinHeight = 96;
        static constexpr int mixerPaintedInsertRowPitch =
            mixerPaintedInsertRowHeight + mixerPaintedInsertRowGap;
        // M5: the send rows sit under the inserts, the way a channel strip reads top to bottom.
        static constexpr int mixerPaintedSendRowHeight = 13;
        static constexpr int mixerPaintedSendRowGap = 2;
        static constexpr int mixerPaintedSendRowCount = 3;
        static constexpr int mixerPaintedSendRowPitch = mixerPaintedSendRowHeight + mixerPaintedSendRowGap;
        static constexpr int mixerPaintedSendLevelInsetX = 3;
        static constexpr int mixerPaintedSendTapWidth = 22;
        static constexpr int mixerPaintedFaderTop = mixerPaintedInsertsTop;
        static constexpr int mixerPaintedFaderBottomInset = 28;
        static constexpr int mixerPaintedMeterWidth = 16;
        static constexpr int mixerPaintedMeterInsetX = 2;
        static constexpr int mixerPaintedMeterInsetY = 0;
        static constexpr int mixerPaintedRailWidth = 18;
        static constexpr int mixerPaintedRailCenterOffsetX = 8;
        static constexpr int mixerPaintedThumbCenterInset = 8;
        static constexpr int mixerPaintedThumbWidthOverhang = 10;
        static constexpr int mixerPaintedThumbHeight = 18;
        // M6: the painted rail's dB marks. The fader travels 0..mixerFaderSliderMax in LINEAR gain
        // (unity therefore sits at half travel, not at the top), so every mark is placed through the
        // same law the thumb uses instead of being spread evenly down the rail.
        static constexpr int mixerPaintedScaleDbCount = 5;
        static constexpr float mixerPaintedScaleDbMarks[mixerPaintedScaleDbCount] = {
            0.0f, -6.0f, -12.0f, -24.0f, -60.0f
        };
        static constexpr int mixerPaintedUnityMarkOverhang = 4;
        static constexpr float mixerPaintedUnityMarkThickness = 1.6f;
        // M7: the MIDI clip note preview — the pitch band a single-note clip still gets, and the
        // cap on how many notes one clip paints per frame (strided, like the waveform path).
        static constexpr int timelineCanvasNotePreviewMinKeySpan = 11;
        static constexpr int timelineCanvasNotePreviewMaxNotes = 64;
        // M8: the black keys sit ON the white ones, as on a real keyboard — narrower, from the
        // column's left edge — and velocity paints one bar per note instead of a joined line.
        static constexpr float pianoRollBlackKeyWidthScale = 0.62f;
        static constexpr int pianoRollVelocityBarWidth = 3;
        static constexpr int pianoRollVelocityBarMinHeight = 2;
        static constexpr int mixerPaintedScaleTickCount = 7;
        static constexpr int mixerPaintedScaleTickWidth = 5;
        static constexpr int mixerPaintedScaleTickGap = 2;
        static constexpr int mixerPaintedReadoutHeight = 22;
        static constexpr int mixerPaintedReadoutBottomInset = 4;
        static constexpr int mixerPaintedReadoutHorizontalInset = 8;
        static constexpr int mixerMasterContentInsetX = 10;
        static constexpr int mixerMasterContentTop = 42;
        static constexpr int mixerMasterLoudnessCardHeight = 74;
        static constexpr int mixerMasterLoudnessValueTop = 20;
        static constexpr int mixerMasterLoudnessValueHeight = 32;
        static constexpr int mixerMasterLoudnessUnitHeight = 16;
        static constexpr int mixerMasterSectionGap = 10;
        static constexpr int mixerMasterPeakCardHeight = 50;
        static constexpr int mixerMasterPeakValueTop = 20;
        static constexpr int mixerMasterPeakValueHeight = 24;
        static constexpr int mixerMasterMeterTopGap = 14;
        static constexpr int mixerMasterMeterBottomInset = 24;
        static constexpr int mixerMasterScaleWidth = 28;
        static constexpr int mixerMasterMeterWidth = 16;
        static constexpr int mixerMasterMeterGap = 4;
        static constexpr int mixerMasterMeterChannelLabelHeight = 18;
        static constexpr int mixerMasterScaleLabelHeight = 14;
        static constexpr std::array<int, 4> mixerMasterScaleDb {{ 0, -12, -24, -60 }};

        static constexpr int trackListHeaderHeight = 86;
        static constexpr int trackListHeaderLabelHeight = 24;
        static constexpr int trackListHeaderInsetX = 16;
        static constexpr int trackListHeaderInsetY = 0;
        static constexpr int automationCanvasHandleRadius = 4;
        static constexpr int automationCanvasHandleHitRadius = 7;
        static constexpr float automationCanvasLineWidth = 1.6f;
        static constexpr int timelineSnapChooserWidth = 96;
        static constexpr int timelineSnapChooserGap = 8;
        static constexpr int timelineRepeatPasteChooserWidth = 72;
        // Wide enough for the painted SNAP caption to sit between the repeat-paste and snap
        // choosers without being clipped by either (B41).
        static constexpr int timelineRepeatPasteChooserGap = 57;
        // Vertical track scroll (E5): when tracks overflow the viewport, lanes hold this fixed
        // row height and the shared row offset scrolls them; few tracks still stretch to fill.
        static constexpr int timelineCanvasLaneRowHeight = 36;
        // Transport loop brace (E6): the band on the upper ruler and its end drag handles.
        static constexpr int timelineCanvasLoopBraceHeight = 12;
        static constexpr int timelineCanvasLoopHandleWidth = 8;
        static constexpr double timelineZoomWheelStep = 1.25;
        // Zoom tool (E3): one click doubles the zoom (Alt+click halves it) through the same
        // anchored viewport math as Ctrl+wheel.
        static constexpr double timelineZoomToolClickFactor = 2.0;
        static constexpr double timelineZoomMin = 1.0;
        static constexpr double timelineZoomMax = 64.0;
        static constexpr double timelineScrollWheelFraction = 0.15;
        static constexpr int headerTempoTextWidth = 52;
        static constexpr int headerTempoTextHeight = 18;
        static constexpr double headerTempoMinBpm = 20.0;
        static constexpr double headerTempoMaxBpm = 400.0;
        static constexpr double headerTempoStepBpm = 0.5;
        static constexpr double headerTempoDefaultBpm = 120.0;
        static constexpr int mixerFxChooserHeight = 22;
        static constexpr int mixerFxSlotHeight = 20;
        static constexpr int mixerFxSlotGap = 2;
        static constexpr int mixerFxSlotRemoveWidth = 20;
        static constexpr std::size_t mixerFxVisibleSlotCount = 5;
        static constexpr int mixerFxParamRowHeight = 18;
        static constexpr int mixerFxParamLabelWidth = 64;
        static constexpr std::size_t mixerFxParamSliderCount = 8;
        // E15: high enough to reach every ParamSpec of every FxKind (EQ band 5 tops out at 83).
        static constexpr std::uint32_t mixerFxParamProbeLimit = 96;
        static constexpr int mixerSendRowHeight = 18;
        static constexpr std::size_t mixerSendVisibleRowCount = 4;
        static constexpr float trackListPanArcRadians = 2.35619449f;   // +/-135 degrees full throw
        static constexpr int trackListAddButtonWidth = 72;
        static constexpr int trackListAddButtonHeight = 24;
        static constexpr int trackListAddButtonInset = 12;
        static constexpr int trackListRenameEditorHeight = 24;
        static constexpr int trackListEmptyLabelInset = 24;
        static constexpr int trackListRowMinHeight = 56;
        static constexpr int trackListRowHorizontalInset = 1;
        static constexpr int trackListRowVerticalInset = 0;
        static constexpr int trackListAccentWidth = 3;
        static constexpr int trackListAccentHorizontalInset = 0;
        static constexpr int trackListAccentVerticalInset = 1;
        static constexpr int trackListSeparatorHeight = 1;
        static constexpr int trackListNameLeftInset = 88;
        static constexpr int trackListNameHeight = 24;
        static constexpr int trackListNameOffsetX = 0;
        static constexpr int trackListNameOffsetY = 9;
        static constexpr int trackListNumberWidth = 40;
        static constexpr int trackListButtonsTop = 34;
        static constexpr int trackListButtonsHeight = 18;
        static constexpr int trackListButtonWidth = 24;
        static constexpr int trackListButtonInsetX = 2;
        static constexpr int trackListButtonInsetY = 0;
        static constexpr int trackListIconLeftInset = 45;
        static constexpr int trackListIconTopInset = 13;
        static constexpr int trackListIconSize = 28;
        static constexpr int trackListMixSummaryWidth = 68;
        static constexpr int trackListMixSummaryRightInset = 34;
        static constexpr int trackListMixSummaryVerticalInset = 4;
        static constexpr int trackListMixLabelLeftInset = 6;
        static constexpr int trackListMixLabelWidth = 24;
        static constexpr int trackListMixLabelHeight = 12;
        static constexpr int trackListPanLabelTopInset = 8;
        static constexpr int trackListPanValueTopInset = 32;
        static constexpr int trackListVolumeLabelTopInset = 38;
        static constexpr int trackListPanRightInset = 44;
        static constexpr int trackListPanTopInset = 8;
        static constexpr int trackListPanDiameter = 24;
        static constexpr int trackListPanIndicatorInset = 4;
        // V5: the rail's mini VOL is a VERTICAL fader (y-axis controls gain, top = loud) sharing
        // the mixer strip fader's orientation law — the retired horizontal-bar tokens are gone.
        // The column sits between the pan knob (right inset 44) and the meter (right inset 12).
        static constexpr int trackListLevelColumnRightInset = 30;
        static constexpr int trackListLevelColumnWidth = 10;
        static constexpr int trackListLevelColumnVerticalInset = 10;
        static constexpr int trackListLevelThumbHeight = 7;
        static constexpr int trackListLevelHitSlopX = 6;
        static constexpr int trackListMeterRightInset = 12;
        static constexpr int trackListMeterWidth = 14;
        static constexpr int trackListMeterHorizontalInset = 0;
        static constexpr int trackListMeterVerticalInset = 10;
        // V5: the rail meter splits into independent L/R columns separated by this gap.
        static constexpr int trackListMeterChannelGap = 2;

        // V6: painted clip identity — fade-curve sampling density (the curve samples the engine's
        // own envelope law at this many points per fade region), its stroke, and the selection
        // ring that makes a selected clip unmistakable on ANY track colour.
        static constexpr int timelineCanvasFadeCurveSamples = 24;
        static constexpr float timelineCanvasFadeCurveStrokeWidth = 1.5f;
        static constexpr float timelineCanvasSelectionStrokeWidth = 2.0f;

        static constexpr int meterFillInset = 2;
        static constexpr int meterSegmentSize = 3;
        static constexpr int meterSegmentGap = 1;

        static constexpr int timelineViewportMinPixelWidth = 1;
        static constexpr int timelineViewportRightGutter = 26;
        static constexpr double timelineDefaultTotalSeconds = 98.0;
        static constexpr double timelineInitialPlayheadSeconds = 0.0;
        static constexpr double timelineViewportScrollSeconds = 0.0;
        static constexpr double timelineMinVisibleSeconds = 1.0;
        static constexpr double timelineProjectEndPaddingScale = 1.25;
        static constexpr double timelineCoordinateSecondsFloor = 0.0;
        static constexpr double timelineCoordinatePixelsPerSecondFloor = 1.0;
        static constexpr int timelineSnapGridTicks = 512;
        static constexpr int timelineClipEdgeHitWidth = 8;
        static constexpr float timelineClipGainPerDragPixel = 0.01f;
        static constexpr float timelineClipMaxGestureGain = 4.0f;
        static constexpr double timelineClipDefaultFadeSeconds = UiThemeLayout::timelineClipDefaultFadeSeconds;
        // Shifted right (B41) so the tool row, repeat-paste chooser, SNAP caption, and snap
        // chooser all fit without overlap to its left.
        static constexpr int automationLaneToggleLeftInset = 420;
        static constexpr int automationLaneToggleTopInset = 8;
        static constexpr int automationLaneToggleWidth = 116;
        static constexpr int automationLaneToggleHeight = 26;
        static constexpr int automationLaneRowLeftInset = 12;
        static constexpr int automationLaneRowTopInset = 92;
        static constexpr int automationLaneRowWidth = 320;
        static constexpr int automationLaneRowHeight = 28;
        static constexpr int automationBreakpointAddButtonLeftInset = 340;
        static constexpr int automationBreakpointAddButtonTopInset = automationLaneRowTopInset;
        static constexpr int automationBreakpointAddButtonWidth = 104;
        static constexpr int automationBreakpointAddButtonHeight = automationLaneRowHeight;
        static constexpr int automationBreakpointDeleteButtonLeftInset = 452;
        static constexpr int automationBreakpointDeleteButtonTopInset = automationLaneRowTopInset;
        static constexpr int automationBreakpointDeleteButtonWidth = 112;
        static constexpr int automationBreakpointDeleteButtonHeight = automationLaneRowHeight;
        static juce::Rectangle<int> automationLaneToggleBounds (juce::Rectangle<int> timeline) noexcept
        {
            return timeline.withTrimmedLeft (automationLaneToggleLeftInset)
                           .withTrimmedTop (automationLaneToggleTopInset)
                           .withWidth (automationLaneToggleWidth)
                           .withHeight (automationLaneToggleHeight);
        }

        // V8: the toolbar zoom cluster — [-] readout [+] to the right of the automation toggle,
        // sharing its row (the same toolbar band the tools/SNAP/repeat cluster already fills).
        static constexpr int timelineZoomOutButtonLeftInset = 548;
        static constexpr int timelineZoomButtonWidth = 28;
        static constexpr int timelineZoomReadoutWidth = 48;
        static constexpr int timelineZoomClusterGap = 4;
        static juce::Rectangle<int> timelineZoomOutButtonBounds (juce::Rectangle<int> timeline) noexcept
        {
            return timeline.withTrimmedLeft (timelineZoomOutButtonLeftInset)
                           .withTrimmedTop (automationLaneToggleTopInset)
                           .withWidth (timelineZoomButtonWidth)
                           .withHeight (automationLaneToggleHeight);
        }
        static juce::Rectangle<int> timelineZoomReadoutBounds (juce::Rectangle<int> timeline) noexcept
        {
            return timelineZoomOutButtonBounds (timeline)
                .translated (timelineZoomButtonWidth + timelineZoomClusterGap, 0)
                .withWidth (timelineZoomReadoutWidth);
        }
        static juce::Rectangle<int> timelineZoomInButtonBounds (juce::Rectangle<int> timeline) noexcept
        {
            return timelineZoomReadoutBounds (timeline)
                .translated (timelineZoomReadoutWidth + timelineZoomClusterGap, 0)
                .withWidth (timelineZoomButtonWidth);
        }
        static juce::Rectangle<int> automationLaneRowBounds (juce::Rectangle<int> timeline) noexcept
        {
            return timeline.withTrimmedLeft (automationLaneRowLeftInset)
                           .withTrimmedTop (automationLaneRowTopInset)
                           .withWidth (automationLaneRowWidth)
                           .withHeight (automationLaneRowHeight);
        }

        static juce::Rectangle<int> automationBreakpointAddButtonBounds (juce::Rectangle<int> timeline) noexcept
        {
            return timeline.withTrimmedLeft (automationBreakpointAddButtonLeftInset)
                           .withTrimmedTop (automationBreakpointAddButtonTopInset)
                           .withWidth (automationBreakpointAddButtonWidth)
                           .withHeight (automationBreakpointAddButtonHeight);
        }

        static juce::Rectangle<int> automationBreakpointDeleteButtonBounds (juce::Rectangle<int> timeline) noexcept
        {
            return timeline.withTrimmedLeft (automationBreakpointDeleteButtonLeftInset)
                           .withTrimmedTop (automationBreakpointDeleteButtonTopInset)
                           .withWidth (automationBreakpointDeleteButtonWidth)
                           .withHeight (automationBreakpointDeleteButtonHeight);
        }

        // E20: the lane-target chooser sits beside the delete button on the automation row.
        static constexpr int automationTargetChooserLeftInset = automationBreakpointDeleteButtonLeftInset
                                                              + automationBreakpointDeleteButtonWidth + 8;
        static constexpr int automationTargetChooserWidth = 172;
        // N5: the write-mode chooser (Read/Touch/Latch) — shorter labels than the target chooser.
        static constexpr int automationModeChooserWidth = 88;
        static juce::Rectangle<int> automationTargetChooserBounds (juce::Rectangle<int> timeline) noexcept
        {
            return timeline.withTrimmedLeft (automationTargetChooserLeftInset)
                           .withTrimmedTop (automationBreakpointDeleteButtonTopInset)
                           .withWidth (automationTargetChooserWidth)
                           .withHeight (automationBreakpointDeleteButtonHeight);
        }
        static constexpr int inputDragDeadZonePixels = 2;
        // E33: the inspector TAKES section rows (chooser + delete) in the old placeholder area.
        static constexpr int inspectorTakeRowHeight = 26;
        static constexpr int inspectorTakeRowGap = 4;
        static constexpr int inspectorTakeDeleteWidth = 110;
        static constexpr int pianoRollHeaderHeight = 38;
        static constexpr int pianoRollPanelInsetX = 12;
        static constexpr int pianoRollPanelInsetY = 8;
        static constexpr int pianoRollExpressionHeight = 84;
        static constexpr int pianoRollKeyboardWidth = 70;
        static constexpr int pianoRollLowKey = 48;
        static constexpr int pianoRollHighKey = 72;
        static constexpr int pianoRollKeyCount = pianoRollHighKey - pianoRollLowKey + 1;
        static constexpr int pianoRollGridTickStep = 512;
        static constexpr int pianoRollGridStrongTickStep = 2048;
        static constexpr int pianoRollGridInsetX = 0;
        static constexpr int pianoRollGridInsetY = 2;
        static constexpr int pianoRollGridMinHeight = 1;
        static constexpr int pianoRollNoteMinWidth = 10;
        static constexpr int pianoRollNoteTopInset = 2;
        static constexpr int pianoRollNoteMinHeight = 8;
        static constexpr int pianoRollNoteHeightTrim = 4;
        static constexpr int pianoRollNoteInsetX = 1;
        static constexpr int pianoRollNoteInsetY = 0;
        static constexpr int pianoRollNoteEdgeHitWidth = 8;
        static constexpr int pianoRollNoteEdgeMinGrabWidth = 24;
        static constexpr int pianoRollHeaderLabelInsetX = 14;
        static constexpr int pianoRollHeaderLabelInsetY = 0;
        static constexpr int pianoRollKeyRowMinHeight = 1;
        static constexpr int pianoRollKeyRowInsetX = 0;
        static constexpr int pianoRollKeyRowInsetY = 1;
        static constexpr int pianoRollGridLineWidth = 1;
        static constexpr int pianoRollKeyLabelInsetX = 8;
        static constexpr int pianoRollKeyLabelInsetY = 0;
        static constexpr int pianoRollSelectedNoteHalo = 1;
        static constexpr int pianoRollExpressionInsetX = 0;
        static constexpr int pianoRollExpressionInsetY = 6;
        static constexpr int pianoRollExpressionLaneHeight = 36;
        static constexpr int pianoRollExpressionLaneInsetX = 0;
        static constexpr int pianoRollExpressionLaneInsetY = 2;
        static constexpr int pianoRollExpressionLabelInsetX = 8;
        static constexpr int pianoRollExpressionLabelInsetY = 0;
        static constexpr int pianoRollExpressionPathBottomInset = 5;
        static constexpr int pianoRollExpressionPathVerticalInset = 10;
        static constexpr float pianoRollExpressionPointRadius = 2.5f;
        static constexpr float pianoRollExpressionPointDiameter = 5.0f;
        static constexpr float pianoRollExpressionPathStrokeWidth = 1.5f;

        static constexpr int timelineCanvasToolbarWidth = 190;
        static constexpr int timelineCanvasToolbarInsetX = 0;
        static constexpr int timelineCanvasToolbarInsetY = Space::sm;
        static constexpr int timelineCanvasToolCellWidth = 34;
        static constexpr int timelineCanvasToolCellInsetX = Space::xxs + Space::hairline;
        static constexpr int timelineCanvasToolCellInsetY = 0;
        static constexpr int timelineCanvasOuterInset = 1;
        static constexpr int timelineCanvasToolbarHeight = 36;
        static constexpr int timelineCanvasRulerHeight = 48;
        static constexpr int timelineCanvasClipAreaInsetX = 12;
        static constexpr int timelineCanvasClipAreaInsetY = 0;
        // E26: the automation lane is a real full-width band between the ruler and the clip
        // area — reserved by the shared geometry law when visible, never stamped over clips.
        static constexpr int timelineCanvasAutomationBandHeight = 56;
        static constexpr int timelineCanvasAutomationHeaderHeight = 24;
        static constexpr int timelineCanvasAutomationHeaderGap = 8;
        static constexpr int timelineCanvasGeometryMinLaneCount = 1;
        static constexpr double timelineCanvasViewportMinPixelsPerSecond = 1.0;
        // Caption sits in the gap between the repeat-paste chooser and the snap chooser (B41).
        static constexpr int timelineCanvasSnapLabelX = 266;
        static constexpr int timelineCanvasSnapLabelWidth = 42;
        static constexpr int timelineCanvasSnapFieldX = 316;
        static constexpr int timelineCanvasSnapFieldWidth = 80;
        static constexpr int timelineCanvasSnapFieldInsetX = 0;
        static constexpr int timelineCanvasSnapFieldInsetY = Space::sm + Space::hairline;
        static constexpr int timelineCanvasSnapValueX = 324;
        static constexpr int timelineCanvasSnapValueWidth = 54;
        static constexpr float timelineCanvasOutlineInset = 0.5f;
        static constexpr float timelineCanvasOutlineStrokeWidth = 1.0f;
        static constexpr int timelineCanvasWaveformInsetX = 7;
        static constexpr int timelineCanvasWaveformInsetY = 5;
        static constexpr float timelineCanvasWaveformMinAmplitude = 0.1f;
        static constexpr float timelineCanvasWaveformMaxAmplitude = 1.0f;
        static constexpr float timelineCanvasWaveformHeightScale = 0.42f;
        static constexpr int timelineCanvasWaveformMinStep = 2;
        // E5 perf repair: row-height clips stride coarser — hundreds paint per frame on CI.
        static constexpr int timelineCanvasWaveformCompactMinStep = 4;
        static constexpr int timelineCanvasWaveformStepDivisor = 64;
        static constexpr int timelineCanvasWaveformClipPhaseMultiplier = 37;
        static constexpr int timelineCanvasWaveformXPhaseMultiplier = 13;
        static constexpr int timelineCanvasWaveformHashOffset = 17;
        static constexpr int timelineCanvasWaveformHashShiftA = 5;
        static constexpr int timelineCanvasWaveformHashShiftB = 11;
        static constexpr int timelineCanvasWaveformHashMultiplier = 29;
        static constexpr int timelineCanvasWaveformPhaseMask = 31;
        static constexpr float timelineCanvasWaveformMinScale = 0.32f;
        static constexpr float timelineCanvasWaveformScaleRange = 0.68f;
        static constexpr int timelineCanvasClipMinPaintWidth = 2;
        static constexpr int timelineCanvasClipMinPaintHeight = 2;
        static constexpr int timelineCanvasClipCompactHeight = 8;
        // E5 perf tier: clips below this height draw a FLAT frame (fill + square outline) with
        // the waveform; the antialiased gradient/rounded frame is reserved for taller clips. At
        // the fixed 36px overflow row height, hundreds of visible clips per frame must stay under
        // the 60fps budget on CI hardware.
        static constexpr int timelineCanvasClipRichPaintHeight = 48;
        static constexpr int timelineCanvasClipCompactHighlightHeight = 1;
        static constexpr int timelineCanvasRulerSeparatorHeight = 1;
        // V4: bar labels thin themselves to power-of-two bar steps (1, 2, 4, 8, ...) until
        // neighbouring labels are at least this far apart — replaces the retired fake
        // seconds-step label tokens (the ruler now labels real tempo-map bars, never seconds).
        static constexpr double timelineCanvasRulerMinBarLabelSpacingPx = 56.0;
        static constexpr int timelineCanvasRulerLabelCullPadding = 40;
        static constexpr int timelineCanvasRulerLabelLeftInset = 18;
        static constexpr int timelineCanvasRulerLabelTopInset = 7;
        static constexpr int timelineCanvasRulerLabelWidth = 36;
        static constexpr int timelineCanvasRulerLabelHeight = 16;
        static constexpr int timelineCanvasRulerTickHeight = 14;
        static constexpr int timelineCanvasRulerTickWidth = 1;
        static constexpr int timelineCanvasRulerMarkerCullPadding = 60;
        static constexpr int timelineCanvasRulerMarkerLabelLeftInset = 4;
        static constexpr int timelineCanvasRulerMarkerLabelTopInset = 24;
        static constexpr int timelineCanvasRulerMarkerLabelWidth = 76;
        static constexpr int timelineCanvasRulerMarkerLabelHeight = 16;
        static constexpr int timelineCanvasGridMinLaneCount = 1;
        static constexpr int timelineCanvasGridLaneSeparatorHeight = 1;
        static constexpr int timelineCanvasGridTrackTintTopInset = 1;
        static constexpr int timelineCanvasGridTrackTintWidth = 3;
        static constexpr int timelineCanvasGridTrackTintHeightTrim = 1;
        static constexpr double timelineCanvasGridStepSeconds = 4.0;
        static constexpr int timelineCanvasGridMajorStepSeconds = 16;
        static constexpr int timelineCanvasGridLineWidth = 1;
        static constexpr int timelineCanvasPlayheadLineWidth = 2;
        static constexpr int timelineCanvasPlayheadBadgeHalfWidth = 15;
        static constexpr int timelineCanvasPlayheadBadgeTopInset = 4;
        static constexpr int timelineCanvasPlayheadBadgeWidth = 30;
        static constexpr int timelineCanvasPlayheadBadgeHeight = 16;
        static constexpr int timelineCanvasPlayheadTextHalfWidth = 12;
        static constexpr int timelineCanvasPlayheadTextWidth = 24;
        static constexpr int timelineCanvasPlayheadTextHeight = 16;
        static constexpr int timelineCanvasVisibleClipCapacity = 4096;
        static constexpr double timelineCanvasDefaultTotalSeconds = 96.0;
        static constexpr double timelineCanvasDefaultPlayheadSeconds = 32.0;
        // V4: the no-project ruler's bar length — the SAME 120 BPM 4/4 fallback the transport
        // readout's headerBarBeat() commits to when no tempo/meter map is loaded.
        static constexpr double timelineCanvasDefaultBarSeconds = 2.0;
        static constexpr double timelineLayoutDefaultPixelsPerSecond =
            UiThemeLayout::timelineLayoutDefaultPixelsPerSecond;
        static constexpr double timelineLayoutDefaultWidthPixels =
            UiThemeLayout::timelineLayoutDefaultWidthPixels;
        static constexpr double timelineLayoutDefaultLaneHeightPixels =
            UiThemeLayout::timelineLayoutDefaultLaneHeightPixels;
        static constexpr double timelineLayoutZeroFloor = UiThemeLayout::timelineLayoutZeroFloor;
    };
};

} // namespace yesdaw::ui
