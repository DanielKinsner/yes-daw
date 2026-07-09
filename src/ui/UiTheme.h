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
        static juce::Colour appBackground() noexcept { return juce::Colour (0xff080c11); }
        static juce::Colour panel() noexcept { return juce::Colour (0xff11161c); }
        static juce::Colour panelRaised() noexcept { return juce::Colour (0xff151b22); }
        static juce::Colour panelStroke() noexcept { return juce::Colour (0xff2a323b); }
        static juce::Colour text() noexcept { return juce::Colour (0xfff0f3f6); }
        static juce::Colour mutedText() noexcept { return juce::Colour (0xff9aa3ad); }
        static juce::Colour white() noexcept { return juce::Colour (0xffffffff); }

        static juce::Colour accentBlue() noexcept { return juce::Colour (0xff3b8cff); }
        static juce::Colour accentTeal() noexcept { return juce::Colour (0xff1bb5a6); }
        static juce::Colour accentAmber() noexcept { return juce::Colour (0xffd29118); }
        static juce::Colour accentPurple() noexcept { return juce::Colour (0xffa762f0); }
        static juce::Colour accentCyan() noexcept { return juce::Colour (0xff20c8d8); }
        static juce::Colour meterGreen() noexcept { return juce::Colour (0xff74df35); }
        static juce::Colour meterYellow() noexcept { return juce::Colour (0xffe2c832); }
        static juce::Colour dangerRed() noexcept { return juce::Colour (0xffff5757); }

        static juce::Colour timelineGrid() noexcept { return juce::Colour (0xff24303a); }
        static juce::Colour timelineCanvas() noexcept { return juce::Colour (0xff0c1217); }
        static juce::Colour timelineToolbar() noexcept { return juce::Colour (0xff101720); }
        static juce::Colour timelineRuler() noexcept { return juce::Colour (0xff0b1117); }

        static juce::Colour controlInset() noexcept { return juce::Colour (0xff0b1016); }
        static juce::Colour controlInsetDeep() noexcept { return juce::Colour (0xff05080b); }
        static juce::Colour controlInsetBlack() noexcept { return juce::Colour (0xff070b10); }
        static juce::Colour toolButton() noexcept { return juce::Colour (0xff080d12); }
        static juce::Colour snapField() noexcept { return juce::Colour (0xff070b0f); }
        static juce::Colour buttonSurface() noexcept { return juce::Colour (0xff111820); }
        static juce::Colour darkControl() noexcept { return juce::Colour (0xff121820); }
        static juce::Colour warningButton() noexcept { return juce::Colour (0xff201b13); }
        static juce::Colour separator() noexcept { return juce::Colour (0xff18202a); }
        static juce::Colour canvasLayer() noexcept { return juce::Colour (0xff0d1218); }
        static juce::Colour selectedLane() noexcept { return juce::Colour (0xff17131f); }
        static juce::Colour mixerBack() noexcept { return juce::Colour (0xff0a0f14); }
        static juce::Colour pianoBlackKey() noexcept { return juce::Colour (0xff0a0e13); }
        static juce::Colour pianoGridStrong() noexcept { return juce::Colour (0xff344150); }
        static juce::Colour pianoGridWeak() noexcept { return juce::Colour (0xff202a34); }
        static juce::Colour inspectorTab() noexcept { return juce::Colour (0xff151a22); }
        static juce::Colour selectedStrip() noexcept { return juce::Colour (0xff1c1428); }
        static juce::Colour faderThumb() noexcept { return juce::Colour (0xffc4c9cf); }
    };

    struct Meter
    {
        static juce::Colour nominalFill() noexcept { return Color::meterGreen(); }
        static juce::Colour hotFill() noexcept { return Color::meterYellow(); }

        static constexpr float verticalHotBand = 0.22f;
        static constexpr float horizontalHotBand = 0.18f;
        static constexpr std::array<float, 8> mainComponentDemoTrackLevels {{
            0.86f, 0.72f, 0.68f, 0.82f, 0.58f, 0.70f, 0.76f, 0.48f
        }};
        static constexpr float mainComponentProjectTimelineTrackLevel = 0.75f;
    };

    struct Mixer
    {
        static constexpr std::array<float, 11> mainComponentDemoStripFaders {{
            0.64f, 0.58f, 0.54f, 0.52f, 0.66f, 0.60f, 0.50f, 0.42f, 0.48f, 0.55f, 0.50f
        }};
        static constexpr std::array<float, 11> mainComponentDemoStripMeters {{
            0.86f, 0.70f, 0.63f, 0.66f, 0.84f, 0.61f, 0.68f, 0.73f, 0.44f, 0.57f, 0.52f
        }};
        static constexpr float mainComponentDemoSelectedPan = -0.08f;
        static constexpr float mainComponentDemoDefaultPan = 0.0f;
        static constexpr float mainComponentDemoMeterPeakRightScale = 0.92f;
        static constexpr float mainComponentDemoMeterRmsLeftScale = 0.58f;
        static constexpr float mainComponentDemoMeterRmsRightScale = 0.52f;
        static constexpr double mainComponentDemoIntegratedLufs = -7.2;
        static constexpr double mainComponentDemoMomentaryLufs = -9.4;
        static constexpr double mainComponentDemoShortTermLufs = -8.8;
        static constexpr double mainComponentDemoLoudnessRangeLu = 5.0;
        static constexpr double mainComponentDemoTruePeakDbtp = -1.0;
    };

    struct PianoRoll
    {
        static constexpr std::uint8_t mainComponentDemoMidiClipIdLow = 80;
        static constexpr std::int64_t mainComponentDemoTimelineStartTicks = 0;
        static constexpr std::int64_t mainComponentDemoTimelineLengthTicks = 4096;
        static constexpr std::array<std::uint8_t, 6> mainComponentDemoNoteIdLows {{ 81, 82, 83, 84, 85, 86 }};
        static constexpr std::array<std::int64_t, 6> mainComponentDemoNoteStartTicks {{
            0, 512, 1024, 1792, 2560, 3328
        }};
        static constexpr std::array<std::int64_t, 6> mainComponentDemoNoteLengthTicks {{
            512, 384, 512, 768, 512, 512
        }};
        static constexpr std::array<std::int16_t, 6> mainComponentDemoNoteKeys {{ 60, 64, 67, 72, 69, 67 }};
        static constexpr std::array<double, 6> mainComponentDemoNotePitches {{
            60.25, 64.10, 67.35, 72.00, 69.20, 66.85
        }};
        static constexpr std::array<double, 6> mainComponentDemoNoteVelocities {{
            0.70, 0.58, 0.82, 0.64, 0.90, 0.74
        }};
        static constexpr std::array<std::int16_t, 6> mainComponentDemoNotePortIndexes {{ 0, 0, 0, 0, 0, 0 }};
        static constexpr std::array<std::int16_t, 6> mainComponentDemoNoteChannels {{ 1, 1, 1, 1, 1, 1 }};
        static constexpr std::array<bool, 6> mainComponentDemoNoteSelected {{ true, false, false, false, false, false }};
    };

    struct Tone
    {
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
        static constexpr std::array<float, 23> mainComponentDemoTimelineClipAlphas {{
            0.82f, 0.78f, 0.80f, 0.70f, 0.76f, 0.85f,
            0.72f, 0.75f, 0.70f,
            0.64f, 0.67f, 0.70f, 0.62f,
            0.88f, 0.90f, 0.86f,
            0.62f, 0.66f, 0.58f,
            0.50f, 0.52f, 0.48f,
            0.68f
        }};
    };

    struct Space
    {
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
        static constexpr float sm = 3.0f;
        static constexpr float md = 4.0f;
        static constexpr float panel = 5.0f;
        static constexpr float lg = 6.0f;
        static constexpr float pill = 7.0f;
    };

    struct Type
    {
        static constexpr float tiny = 8.0f;
        static constexpr float caption = 10.0f;
        static constexpr float small = 11.0f;
        static constexpr float body = 12.0f;
        static constexpr float title = 13.0f;
        static constexpr float readout = 16.0f;
        static constexpr float statusIcon = 19.0f;
        static constexpr float transportClock = 25.0f;
    };

    struct Layout
    {
        static constexpr int headerHeight = 88;
        static constexpr int defaultWindowWidth = 1536;
        static constexpr int defaultWindowHeight = 960;
        static constexpr int leftRailWidth = 318;
        static constexpr int inspectorWidth = 248;
        static constexpr int mixerHeight = 260;
        static constexpr int headerMenuStartX = 22;
        static constexpr int headerMenuY = 17;
        static constexpr int headerMenuWidth = 70;
        static constexpr int headerMenuHeight = 18;
        static constexpr int headerMenuStep = 48;
        static constexpr int headerOptionsMenuStep = 72;
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
        static constexpr int headerTransportBoxWidth = 248;
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
        static juce::Rectangle<int> projectNewButtonBounds() noexcept { return { 16, 50, 44, 26 }; }
        static juce::Rectangle<int> projectOpenButtonBounds() noexcept { return { 64, 50, 50, 26 }; }
        static juce::Rectangle<int> projectSaveButtonBounds() noexcept { return { 118, 50, 48, 26 }; }
        static juce::Rectangle<int> projectImportAudioButtonBounds() noexcept { return { 170, 50, 64, 26 }; }
        static juce::Rectangle<int> projectExportAudioButtonBounds() noexcept { return { 238, 16, 82, 26 }; }
        static juce::Rectangle<int> deviceRefreshAudioButtonBounds() noexcept { return { 22, 104, 78, 26 }; }
        static juce::Rectangle<int> deviceSelectTestAudioButtonBounds() noexcept { return { 104, 104, 104, 26 }; }
        static juce::Rectangle<int> recordingArmTrackButtonBounds() noexcept { return { 212, 104, 68, 26 }; }
        static juce::Rectangle<int> recordingSetMonitoringPolicyButtonBounds() noexcept { return { 22, 134, 96, 26 }; }
        static juce::Rectangle<int> transportRecordButtonBounds() noexcept { return { 122, 134, 76, 26 }; }
        static juce::Rectangle<int> recordingAssembleCompButtonBounds() noexcept { return { 202, 134, 72, 26 }; }
        static juce::Rectangle<int> editUndoButtonBounds() noexcept { return { 244, 50, 42, 26 }; }
        static juce::Rectangle<int> editRedoButtonBounds() noexcept { return { 290, 50, 42, 26 }; }
        static juce::Rectangle<int> transportLocateStartButtonBounds() noexcept { return { 336, 16, 56, 56 }; }
        static juce::Rectangle<int> transportPlayButtonBounds() noexcept { return { 392, 16, 56, 56 }; }
        static juce::Rectangle<int> transportStopButtonBounds() noexcept { return { 448, 16, 56, 56 }; }
        static juce::Rectangle<int> transportToggleLoopButtonBounds() noexcept { return { 1008, 16, 64, 56 }; }
        static juce::Rectangle<int> viewMixerButtonBounds (int componentHeight) noexcept
        {
            return { 16, componentHeight - mixerHeight + 18, 76, 28 };
        }
        static juce::Rectangle<int> viewPianoRollButtonBounds (int componentHeight) noexcept
        {
            return { 96, componentHeight - mixerHeight + 18, 78, 28 };
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
        static constexpr int inspectorStatsCellInsetX = 4;
        static constexpr int inspectorStatsCellInsetY = 0;
        static constexpr int inspectorStatsTextInset = 4;
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
        static constexpr int inspectorFadeControlLeftInset = 78;
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
        static constexpr int inspectorFxSectionTop = 360;
        static constexpr int inspectorFxRowHeight = 28;
        static constexpr int inspectorFxRowInsetX = 0;
        static constexpr int inspectorFxRowInsetY = 2;
        static constexpr int inspectorFxTextInsetX = 10;
        static constexpr int inspectorFxTextInsetY = 0;
        static constexpr int hiddenSliderTextBoxWidth = 0;
        static constexpr int hiddenSliderTextBoxHeight = 0;

        static constexpr int mixerToolsWidth = 120;
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
        static constexpr int mixerToolsInsetX = 8;
        static constexpr int mixerToolsInsetY = 0;
        static constexpr int mixerToolsSendsLabelTop = 52;
        static constexpr int mixerToolsViewLabelTop = 96;
        static constexpr int mixerToolsModeLabelTop = 120;
        static constexpr int mixerToolsLabelHeight = 24;
        static constexpr int mixerToolsModeLabelHeight = 28;
        static constexpr int mixerToolsLabelInsetX = 12;
        static constexpr int mixerToolsLabelInsetY = 0;
        static constexpr int mixerPaintedStripMinWidth = 84;
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
        static constexpr int mixerPaintedFaderTop = 112;
        static constexpr int mixerPaintedFaderBottomInset = 28;
        static constexpr int mixerPaintedMeterWidth = 16;
        static constexpr int mixerPaintedMeterInsetX = 2;
        static constexpr int mixerPaintedMeterInsetY = 0;
        static constexpr int mixerPaintedRailWidth = 18;
        static constexpr int mixerPaintedRailCenterOffsetX = 8;
        static constexpr int mixerPaintedThumbCenterInset = 8;
        static constexpr int mixerPaintedThumbWidthOverhang = 10;
        static constexpr int mixerPaintedThumbHeight = 18;

        static constexpr int trackListHeaderHeight = 38;
        static constexpr int trackListHeaderInsetX = 16;
        static constexpr int trackListHeaderInsetY = 0;
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
        static constexpr int trackListMeterRightInset = 12;
        static constexpr int trackListMeterWidth = 14;
        static constexpr int trackListMeterHorizontalInset = 0;
        static constexpr int trackListMeterVerticalInset = 10;

        static constexpr int meterFillInset = 2;

        static constexpr int timelineViewportMinPixelWidth = 1;
        static constexpr int timelineViewportRightGutter = 26;
        static constexpr double timelineDefaultTotalSeconds = 98.0;
        static constexpr double timelineDemoPlayheadSeconds = 32.0;
        static constexpr double timelineProjectPlayheadSeconds = 0.0;
        static constexpr double timelineViewportScrollSeconds = 0.0;
        static constexpr double timelineMinVisibleSeconds = 1.0;
        static constexpr double timelineProjectEndPaddingScale = 1.25;
        static constexpr double timelineCoordinateSecondsFloor = 0.0;
        static constexpr double timelineCoordinatePixelsPerSecondFloor = 1.0;
        static constexpr int timelineSnapGridTicks = 512;
        static constexpr int timelineClipEdgeHitWidth = 8;
        static constexpr float timelineClipGainPerDragPixel = 0.01f;
        static constexpr float timelineClipMaxGestureGain = 4.0f;
        static constexpr int automationLaneToggleLeftInset = 368;
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
        static constexpr int inputDragDeadZonePixels = 2;
        static constexpr std::array<int, 23> mainComponentDemoTimelineClipLanes {{
            0, 0, 0, 0, 0, 0,
            1, 1, 1,
            2, 2, 2, 2,
            3, 3, 3,
            4, 4, 4,
            5, 5, 5,
            6
        }};
        static constexpr std::array<double, 23> mainComponentDemoTimelineClipStartsSeconds {{
            0.0, 17.0, 27.0, 48.0, 65.0, 77.0,
            4.0, 22.0, 47.0,
            3.0, 25.0, 41.0, 69.0,
            11.0, 39.0, 57.0,
            11.0, 37.0, 59.0,
            7.0, 33.0, 65.0,
            0.0
        }};
        static constexpr std::array<double, 23> mainComponentDemoTimelineClipLengthsSeconds {{
            17.0, 10.0, 17.0, 17.0, 12.0, 14.0,
            18.0, 20.0, 30.0,
            18.0, 18.0, 26.0, 16.0,
            28.0, 18.0, 30.0,
            24.0, 22.0, 28.0,
            26.0, 22.0, 24.0,
            38.0
        }};
        static constexpr std::array<double, 5> mainComponentDemoTimelineMarkerSeconds {{
            8.0, 24.0, 32.0, 64.0, 80.0
        }};

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
        static constexpr int timelineCanvasGeometryMinLaneCount = 1;
        static constexpr double timelineCanvasViewportMinPixelsPerSecond = 1.0;
        static constexpr int timelineCanvasLaneMinHeight = 8;
        static constexpr int timelineCanvasSnapLabelX = 234;
        static constexpr int timelineCanvasSnapLabelWidth = 42;
        static constexpr int timelineCanvasSnapFieldX = 276;
        static constexpr int timelineCanvasSnapFieldWidth = 80;
        static constexpr int timelineCanvasSnapFieldInsetX = 0;
        static constexpr int timelineCanvasSnapFieldInsetY = Space::sm + Space::hairline;
        static constexpr int timelineCanvasSnapValueX = 284;
        static constexpr int timelineCanvasSnapValueWidth = 54;
        static constexpr float timelineCanvasOutlineInset = 0.5f;
        static constexpr float timelineCanvasOutlineStrokeWidth = 1.0f;
        static constexpr int timelineCanvasWaveformInsetX = 7;
        static constexpr int timelineCanvasWaveformInsetY = 5;
        static constexpr float timelineCanvasWaveformMinAmplitude = 0.1f;
        static constexpr float timelineCanvasWaveformMaxAmplitude = 1.0f;
        static constexpr float timelineCanvasWaveformHeightScale = 0.42f;
        static constexpr int timelineCanvasWaveformMinStep = 8;
        static constexpr int timelineCanvasWaveformStepDivisor = 9;
        static constexpr int timelineCanvasWaveformClipPhaseMultiplier = 37;
        static constexpr int timelineCanvasWaveformXPhaseMultiplier = 13;
        static constexpr int timelineCanvasWaveformPhaseMask = 31;
        static constexpr float timelineCanvasWaveformMinScale = 0.32f;
        static constexpr float timelineCanvasWaveformScaleRange = 0.68f;
        static constexpr int timelineCanvasClipMinPaintWidth = 2;
        static constexpr int timelineCanvasClipMinPaintHeight = 2;
        static constexpr int timelineCanvasClipCompactHeight = 8;
        static constexpr int timelineCanvasClipCompactHighlightHeight = 1;
        static constexpr int timelineCanvasRulerSeparatorHeight = 1;
        static constexpr double timelineCanvasRulerDensePixelsPerSecond = 24.0;
        static constexpr double timelineCanvasRulerWideLabelStepSeconds = 8.0;
        static constexpr double timelineCanvasRulerNarrowLabelStepSeconds = 4.0;
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
