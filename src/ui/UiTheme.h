// YES DAW - H16 UI design-token surface.
//
// Raw visual constants live here so UI code can be audited mechanically.

#pragma once

#include <juce_graphics/juce_graphics.h>

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
        static constexpr int shellPanelHorizontalInset = 6;
        static constexpr int shellPanelVerticalInset = 10;
        static constexpr int mixerPanelHorizontalInset = 6;
        static constexpr int mixerPanelVerticalInset = 8;

        static constexpr int inspectorTabHeight = 40;
        static constexpr int inspectorContentInsetX = 16;
        static constexpr int inspectorContentInsetY = 14;
        static constexpr int inspectorGainSectionTop = 118;
        static constexpr int inspectorGainSectionHeight = 84;
        static constexpr int inspectorGainControlTopInset = 28;
        static constexpr int inspectorGainControlHeight = 24;
        static constexpr int inspectorGainControlLeftInset = 72;
        static constexpr int inspectorFadesSectionTop = 214;
        static constexpr int inspectorFadesSectionHeight = 94;
        static constexpr int inspectorFadesControlTopInset = 22;
        static constexpr int inspectorFadeControlHeight = 32;
        static constexpr int inspectorFadeControlLeftInset = 78;
        static constexpr int inspectorFadeControlHorizontalInset = 0;
        static constexpr int inspectorFadeControlVerticalInset = 6;

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
    };
};

} // namespace yesdaw::ui
