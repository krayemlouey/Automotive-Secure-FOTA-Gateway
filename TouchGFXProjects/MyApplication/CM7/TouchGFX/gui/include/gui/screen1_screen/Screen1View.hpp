#ifndef SCREEN1VIEW_HPP
#define SCREEN1VIEW_HPP

#include <gui_generated/screen1_screen/Screen1ViewBase.hpp>
#include <gui/screen1_screen/Screen1Presenter.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <touchgfx/containers/Container.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/Line.hpp>
#include <touchgfx/widgets/canvas/PainterRGB888.hpp>
#include <touchgfx/events/ClickEvent.hpp>

class Screen1View : public Screen1ViewBase
{
public:
    Screen1View();
    virtual ~Screen1View() {}
    virtual void setupScreen();
    virtual void tearDownScreen();
    virtual void updatePotData(uint32_t pot, uint32_t frames, uint32_t status);
    virtual void handleClickEvent(const touchgfx::ClickEvent& evt);

protected:
    /* Background & Status Bars (Qt Look & Feel) */
    touchgfx::Box headerBar;
    touchgfx::Box footerBar;
    touchgfx::Box canLed;
    touchgfx::Box badgeBox;
    touchgfx::Box badgeBorder;
    touchgfx::Box otaPillBox;
    touchgfx::Box otaPillBorder;
    touchgfx::Box gearBox;
    touchgfx::Box gearBorder;
    touchgfx::Box humProgressBg;
    touchgfx::Box humProgressFill;
    touchgfx::Box oilProgressBg;
    touchgfx::Box oilProgressFill;

    /* Painters */
    touchgfx::PainterRGB888 painterDiscDark;     // #0D1520 Outer Disc
    touchgfx::PainterRGB888 painterDiscNavy;     // #131E30 Inner Disc
    touchgfx::PainterRGB888 painterBorderNavy;   // #1E2D45 Inner Border

    touchgfx::PainterRGB888 painterSpeedCyan;     // #00E5FF
    touchgfx::PainterRGB888 painterSpeedDarkTick; // #008FA0

    touchgfx::PainterRGB888 painterFuelGreen;     // #30D158
    touchgfx::PainterRGB888 painterFuelDarkTick;  // #1E8237

    touchgfx::PainterRGB888 painterTempOrange;    // #FF9500
    touchgfx::PainterRGB888 painterTempDarkTick;  // #A05D00

    touchgfx::PainterRGB888 painterWhite;         // #FFFFFF
    touchgfx::PainterRGB888 painterSilver;        // #8E8E93

    /* Left Gauge: CARBURANT (210x210) */
    touchgfx::Circle fuelDiscDark;
    touchgfx::Circle fuelDiscNavy;
    touchgfx::Circle fuelOuterRing;
    touchgfx::Circle fuelInnerRing;
    touchgfx::Line   fuelTicks[21];
    touchgfx::Line   fuelNeedle;
    touchgfx::Circle fuelHub;
    touchgfx::Circle fuelHubBorder;

    /* Center Gauge: VITESSE (270x270) */
    touchgfx::Circle speedDiscDark;
    touchgfx::Circle speedDiscNavy;
    touchgfx::Circle speedOuterRing;
    touchgfx::Circle speedInnerRing;
    touchgfx::Line   speedTicks[21];
    touchgfx::Line   speedNeedle;
    touchgfx::Circle speedHub;
    touchgfx::Circle speedHubBorder;

    /* Right Gauge: TEMP. MOTEUR (210x210) */
    touchgfx::Circle tempDiscDark;
    touchgfx::Circle tempDiscNavy;
    touchgfx::Circle tempOuterRing;
    touchgfx::Circle tempInnerRing;
    touchgfx::Line   tempTicks[21];
    touchgfx::Line   tempNeedle;
    touchgfx::Circle tempHub;
    touchgfx::Circle tempHubBorder;

    /* Header Texts */
    touchgfx::TextAreaWithOneWildcard txtCanStatus;
    touchgfx::Unicode::UnicodeChar txtCanStatusBuf[32];

    touchgfx::TextAreaWithOneWildcard txtBadge;
    touchgfx::Unicode::UnicodeChar txtBadgeBuf[32];

    touchgfx::TextAreaWithOneWildcard txtOtaPill;
    touchgfx::Unicode::UnicodeChar txtOtaPillBuf[32];

    /* Fuel Gauge Texts */
    touchgfx::TextAreaWithOneWildcard txtFuelTitle;
    touchgfx::Unicode::UnicodeChar txtFuelTitleBuf[16];

    touchgfx::TextAreaWithOneWildcard txtFuelVal;
    touchgfx::Unicode::UnicodeChar txtFuelValBuf[16];

    touchgfx::TextAreaWithOneWildcard txtFuelUnit;
    touchgfx::Unicode::UnicodeChar txtFuelUnitBuf[8];

    touchgfx::TextAreaWithOneWildcard txtHumidity;
    touchgfx::Unicode::UnicodeChar txtHumidityBuf[24];

    /* Speed Gauge Texts */
    touchgfx::TextAreaWithOneWildcard txtSpeedTitle;
    touchgfx::Unicode::UnicodeChar txtSpeedTitleBuf[16];

    touchgfx::TextAreaWithOneWildcard txtSpeedVal;
    touchgfx::Unicode::UnicodeChar txtSpeedValBuf[16];

    touchgfx::TextAreaWithOneWildcard txtSpeedUnit;
    touchgfx::Unicode::UnicodeChar txtSpeedUnitBuf[8];

    touchgfx::TextAreaWithOneWildcard txtGear;
    touchgfx::Unicode::UnicodeChar txtGearBuf[8];

    /* Temp Gauge Texts */
    touchgfx::TextAreaWithOneWildcard txtTempTitle;
    touchgfx::Unicode::UnicodeChar txtTempTitleBuf[20];

    touchgfx::TextAreaWithOneWildcard txtTempVal;
    touchgfx::Unicode::UnicodeChar txtTempValBuf[16];

    touchgfx::TextAreaWithOneWildcard txtTempUnit;
    touchgfx::Unicode::UnicodeChar txtTempUnitBuf[8];

    touchgfx::TextAreaWithOneWildcard txtOilStatus;
    touchgfx::Unicode::UnicodeChar txtOilStatusBuf[24];

    /* Footer Texts */
    touchgfx::TextAreaWithOneWildcard txtOdometer;
    touchgfx::Unicode::UnicodeChar txtOdometerBuf[40];

    touchgfx::TextAreaWithOneWildcard txtObstacle;
    touchgfx::Unicode::UnicodeChar txtObstacleBuf[40];

    /* Splash Screen (Amorçage Système Qt) */
    touchgfx::Container splashContainer;
    touchgfx::Box splashBg;
    touchgfx::Circle splashLogoCircle;
    touchgfx::Box splashLogoRoof;
    touchgfx::Box splashLogoBody;
    touchgfx::Circle splashWheel1;
    touchgfx::Circle splashWheel2;

    touchgfx::TextAreaWithOneWildcard txtSplashTitle;
    touchgfx::Unicode::UnicodeChar txtSplashTitleBuf[32];

    touchgfx::TextAreaWithOneWildcard txtSplashSub;
    touchgfx::Unicode::UnicodeChar txtSplashSubBuf[64];

    touchgfx::Box splashProgressBg;
    touchgfx::Box splashProgressFill;

    touchgfx::TextAreaWithOneWildcard txtSplashPct;
    touchgfx::Unicode::UnicodeChar txtSplashPctBuf[16];

    touchgfx::TextAreaWithOneWildcard txtSplashStatus;
    touchgfx::Unicode::UnicodeChar txtSplashStatusBuf[64];

    /* Notification OTA en haut de l'écran */
    touchgfx::Container otaBannerContainer;
    touchgfx::Box otaBannerBox;
    touchgfx::Box otaBannerBorder;
    touchgfx::TextAreaWithOneWildcard txtOtaBanner;
    touchgfx::Unicode::UnicodeChar txtOtaBannerBuf[64];

    /* OTA Popup Modal (ISO 21434 / UN R156) */
    touchgfx::Container otaModalContainer;
    touchgfx::Box otaOverlay;
    touchgfx::Box otaBox;
    touchgfx::Box otaBorderTop;
    touchgfx::Box otaBorderBottom;

    touchgfx::TextAreaWithOneWildcard txtOtaHeader;
    touchgfx::Unicode::UnicodeChar txtOtaHeaderBuf[40];

    touchgfx::TextAreaWithOneWildcard txtOtaDesc;
    touchgfx::Unicode::UnicodeChar txtOtaDescBuf[64];

    /* Boutons tactiles interactifs */
    touchgfx::Box btnInstallBox;
    touchgfx::Box btnInstallBorder;
    touchgfx::TextAreaWithOneWildcard txtBtnInstall;
    touchgfx::Unicode::UnicodeChar txtBtnInstallBuf[24];

    touchgfx::Box btnLaterBox;
    touchgfx::Box btnLaterBorder;
    touchgfx::TextAreaWithOneWildcard txtBtnLater;
    touchgfx::Unicode::UnicodeChar txtBtnLaterBuf[24];

    touchgfx::Box otaInstallBarBg;
    touchgfx::Box otaInstallBarFill;

    touchgfx::TextAreaWithOneWildcard txtOtaInstall;
    touchgfx::Unicode::UnicodeChar txtOtaInstallBuf[48];

    /* Dynamic internal state */
    uint32_t tick_div;
    float current_odometer;
    uint32_t splash_ticks;
    bool splash_finished;

    bool ota_popup_active;
    bool is_v2_updated;
    uint32_t ota_ticks;

    /* Nouveaux états interactifs */
    bool ota_notification_pending;
    bool ota_modal_active;
    bool ota_installing;
    bool target_ota_v2;
    uint32_t direct_alert_ticks;

    static const uint32_t BIG_CANVAS_BUFFER_SIZE = 72000;
    static uint8_t bigCanvasBuffer[BIG_CANVAS_BUFFER_SIZE];
    uint32_t last_rendered_speed;
};

#endif // SCREEN1VIEW_HPP
