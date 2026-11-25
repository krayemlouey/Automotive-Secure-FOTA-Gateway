#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/Color.hpp>
#include <touchgfx/Unicode.hpp>
#include <texts/TextKeysAndLanguages.hpp>
#include <touchgfx/canvas_widget_renderer/CanvasWidgetRenderer.hpp>
#include <cmath>
#include <cstdio>
#include <cstring>

// Registres matériels persistants aux resets (Domaine Backup RTC & SRAM4)
#define H7_PWR_CR1      (*(volatile uint32_t*)0x58024800UL)
#define H7_RTC_BKP0R    (*(volatile uint32_t*)0x58004050UL)
#define H7_SRAM4_FLAG   (*(volatile uint32_t*)0x38000400UL)
#define OTA_MAGIC_V2    0x55AA2000UL
#define H7_RCC_APB4ENR (*(volatile uint32_t*)0x580244F4UL)

uint8_t Screen1View::bigCanvasBuffer[Screen1View::BIG_CANVAS_BUFFER_SIZE];

Screen1View::Screen1View()
    : tick_div(0), current_odometer(12543.0f), splash_ticks(0), splash_finished(false),
      ota_popup_active(false), is_v2_updated(false), ota_ticks(0),
      ota_notification_pending(false), ota_modal_active(false), ota_installing(false), target_ota_v2(true), direct_alert_ticks(0),
      last_rendered_speed(9999)
{
}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();
    touchgfx::CanvasWidgetRenderer::setupBuffer(bigCanvasBuffer, BIG_CANVAS_BUFFER_SIZE);

    // ══════════════════════════════════════════════════════════════
    // PERSISTANCE APRES RESET (UN R156 SUMS) : Vérification du flag
    // ══════════════════════════════════════════════════════════════
    H7_RCC_APB4ENR |= (1UL << 4);   // Activer horloge PWR
    H7_PWR_CR1 |= 0x100;            // DBP = 1 : accès domaine Backup
    for (volatile int _dly = 0; _dly < 2000; _dly++) {} // Stabilisation
    if (H7_RTC_BKP0R == OTA_MAGIC_V2 || H7_SRAM4_FLAG == OTA_MAGIC_V2)
    {
        is_v2_updated = true; // Conserver la mise à jour après RESET !
    }

    // ══════════════════════════════════════════════════════════════
    // 1. FOND DE L'ECRAN UNIFORME SUR TOUT L'ECRAN (#131E30 BLEU NUIT)
    // ══════════════════════════════════════════════════════════════
    box1.setPosition(0, 0, 800, 480);
    box1.setColor(touchgfx::Color::getColorFromRGB(19, 30, 48)); // #131E30 Bleu uni sur tout l'écran
    box1.invalidate();

    // ══════════════════════════════════════════════════════════════
    // 2. PAINTERS (Palette exacte Qt Quick)
    // ══════════════════════════════════════════════════════════════
    painterDiscDark.setColor(touchgfx::Color::getColorFromRGB(13, 21, 32));      // #0D1520 Fond cadran
    painterDiscNavy.setColor(touchgfx::Color::getColorFromRGB(19, 30, 48));      // #131E30 Cadran intérieur
    painterBorderNavy.setColor(touchgfx::Color::getColorFromRGB(30, 45, 69));    // #1E2D45 Bord intérieur

    // Vitesse (Cyan #00E5FF)
    painterSpeedCyan.setColor(touchgfx::Color::getColorFromRGB(0, 229, 255));
    painterSpeedDarkTick.setColor(touchgfx::Color::getColorFromRGB(0, 143, 160));

    // Carburant (Vert #30D158)
    painterFuelGreen.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88));
    painterFuelDarkTick.setColor(touchgfx::Color::getColorFromRGB(30, 130, 55));

    // Température (Orange #FF9500)
    painterTempOrange.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0));
    painterTempDarkTick.setColor(touchgfx::Color::getColorFromRGB(160, 93, 0));

    // Hub & Aiguille
    painterWhite.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    painterSilver.setColor(touchgfx::Color::getColorFromRGB(142, 142, 147));

    // ══════════════════════════════════════════════════════════════
    // 3. ZONE SUPERIEURE EPUREE (Suppression textes en haut demandée)
    // ══════════════════════════════════════════════════════════════
    // canLed, txtCanStatus, badgeBox, txtBadge, otaPillBox masqués pour un écran épuré
    // Note: Restauration V2 déplacée APRES la création de tous les widgets (fin de setupScreen)

    // ══════════════════════════════════════════════════════════════
    // 4. CADRAN GAUCHE : CARBURANT (210x210, Vert #30D158) - CENTRÉ VERTICALEMENT (Y: 90)
    // ══════════════════════════════════════════════════════════════
    fuelOuterRing.setPosition(25, 90, 210, 210);
    fuelOuterRing.setCenter(105, 105);
    fuelOuterRing.setRadius(102);
    fuelOuterRing.setLineWidth(3);
    fuelOuterRing.setArc(0, 360);
    fuelOuterRing.setPainter(painterFuelGreen);
    add(fuelOuterRing);

    fuelInnerRing.setPosition(25, 90, 210, 210);
    fuelInnerRing.setCenter(105, 105);
    fuelInnerRing.setRadius(92);
    fuelInnerRing.setLineWidth(1);
    fuelInnerRing.setArc(0, 360);
    fuelInnerRing.setPainter(painterBorderNavy);
    add(fuelInnerRing);

    // 21 Graduations radiales (de -130° à +130°)
    for (int i = 0; i <= 20; i++)
    {
        float pct = (float)i / 20.0f;
        float angle = (-130.0f + pct * 260.0f) * 0.0174532925f;
        bool isMajor = (i % 5 == 0);
        float r1 = 96.0f;
        float r2 = isMajor ? 82.0f : 88.0f;

        fuelTicks[i].setPosition(25, 90, 210, 210);
        fuelTicks[i].setLineWidth(isMajor ? 2 : 1);
        fuelTicks[i].setPainter(isMajor ? painterFuelGreen : painterFuelDarkTick);
        fuelTicks[i].setLine(105 + (int)(r1 * sinf(angle)), 105 - (int)(r1 * cosf(angle)),
                             105 + (int)(r2 * sinf(angle)), 105 - (int)(r2 * cosf(angle)));
        add(fuelTicks[i]);
    }

    // Aiguille Carburant (75 % initial = +65°)
    fuelNeedle.setPosition(25, 90, 210, 210);
    fuelNeedle.setLineWidth(4);
    fuelNeedle.setPainter(painterFuelGreen);
    float aFuel = 65.0f * 0.0174532925f;
    fuelNeedle.setLine(105, 105, 105 + (int)(75.0f * sinf(aFuel)), 105 - (int)(75.0f * cosf(aFuel)));
    add(fuelNeedle);

    // Moyeu central blanc avec bordure argentée
    fuelHubBorder.setPosition(25, 90, 210, 210);
    fuelHubBorder.setCenter(105, 105);
    fuelHubBorder.setRadius(7);
    fuelHubBorder.setLineWidth(2);
    fuelHubBorder.setPainter(painterSilver);
    add(fuelHubBorder);

    fuelHub.setPosition(25, 90, 210, 210);
    fuelHub.setCenter(105, 105);
    fuelHub.setRadius(5);
    fuelHub.setLineWidth(5);
    fuelHub.setPainter(painterWhite);
    add(fuelHub);

    // Titre "CARBURANT"
    txtFuelTitle.setPosition(25, 107, 210, 20);
    txtFuelTitle.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88));
    txtFuelTitle.setLinespacing(0);
    txtFuelTitleBuf[0] = 0;
    txtFuelTitle.setWildcard(txtFuelTitleBuf);
    txtFuelTitle.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtFuelTitleBuf, "CARBURANT", 16);
    add(txtFuelTitle);

    // Valeur digitale "75"
    txtFuelVal.setPosition(25, 200, 210, 32);
    txtFuelVal.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtFuelVal.setLinespacing(0);
    txtFuelValBuf[0] = 0;
    txtFuelVal.setWildcard(txtFuelValBuf);
    txtFuelVal.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtFuelValBuf, "75", 16);
    add(txtFuelVal);

    // Unité "%"
    txtFuelUnit.setPosition(25, 230, 210, 20);
    txtFuelUnit.setColor(touchgfx::Color::getColorFromRGB(142, 142, 147));
    txtFuelUnit.setLinespacing(0);
    txtFuelUnitBuf[0] = 0;
    txtFuelUnit.setWildcard(txtFuelUnitBuf);
    txtFuelUnit.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtFuelUnitBuf, "%", 8);
    add(txtFuelUnit);

    // (Widget Humidite déplace sous le cadran droit de Temperature pour correspondre a Qt)

    // ══════════════════════════════════════════════════════════════
    // 5. CADRAN CENTRAL : VITESSE (270x270, Cyan #00E5FF) - CENTRÉ VERTICALEMENT (Y: 60)
    // ══════════════════════════════════════════════════════════════
    speedOuterRing.setPosition(265, 60, 270, 270);
    speedOuterRing.setCenter(135, 135);
    speedOuterRing.setRadius(132);
    speedOuterRing.setLineWidth(3);
    speedOuterRing.setArc(0, 360);
    speedOuterRing.setPainter(painterSpeedCyan);
    add(speedOuterRing);

    speedInnerRing.setPosition(265, 60, 270, 270);
    speedInnerRing.setCenter(135, 135);
    speedInnerRing.setRadius(122);
    speedInnerRing.setLineWidth(1);
    speedInnerRing.setArc(0, 360);
    speedInnerRing.setPainter(painterBorderNavy);
    add(speedInnerRing);

    // 21 Graduations radiales Vitesse (de -130° à +130°)
    for (int i = 0; i <= 20; i++)
    {
        float pct = (float)i / 20.0f;
        float angle = (-130.0f + pct * 260.0f) * 0.0174532925f;
        bool isMajor = (i % 5 == 0);
        float r1 = 126.0f;
        float r2 = isMajor ? 112.0f : 118.0f;

        speedTicks[i].setPosition(265, 60, 270, 270);
        speedTicks[i].setLineWidth(isMajor ? 2 : 1);
        speedTicks[i].setPainter(isMajor ? painterSpeedCyan : painterSpeedDarkTick);
        speedTicks[i].setLine(135 + (int)(r1 * sinf(angle)), 135 - (int)(r1 * cosf(angle)),
                              135 + (int)(r2 * sinf(angle)), 135 - (int)(r2 * cosf(angle)));
        add(speedTicks[i]);
    }

    // Aiguille Vitesse (0 km/h initial = -130°)
    speedNeedle.setPosition(265, 60, 270, 270);
    speedNeedle.setLineWidth(4);
    speedNeedle.setPainter(painterSpeedCyan);
    float aSpd0 = -130.0f * 0.0174532925f;
    speedNeedle.setLine(135, 135, 135 + (int)(95.0f * sinf(aSpd0)), 135 - (int)(95.0f * cosf(aSpd0)));
    add(speedNeedle);

    // Moyeu central Vitesse
    speedHubBorder.setPosition(265, 60, 270, 270);
    speedHubBorder.setCenter(135, 135);
    speedHubBorder.setRadius(7);
    speedHubBorder.setLineWidth(2);
    speedHubBorder.setPainter(painterSilver);
    add(speedHubBorder);

    speedHub.setPosition(265, 60, 270, 270);
    speedHub.setCenter(135, 135);
    speedHub.setRadius(5);
    speedHub.setLineWidth(5);
    speedHub.setPainter(painterWhite);
    add(speedHub);

    // Titre "VITESSE"
    txtSpeedTitle.setPosition(265, 77, 270, 20);
    txtSpeedTitle.setColor(touchgfx::Color::getColorFromRGB(0, 229, 255));
    txtSpeedTitle.setLinespacing(0);
    txtSpeedTitleBuf[0] = 0;
    txtSpeedTitle.setWildcard(txtSpeedTitleBuf);
    txtSpeedTitle.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtSpeedTitleBuf, "VITESSE", 16);
    add(txtSpeedTitle);

    // Grande valeur digitale vitesse
    txtSpeedVal.setPosition(265, 207, 270, 36);
    txtSpeedVal.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtSpeedVal.setLinespacing(0);
    txtSpeedValBuf[0] = 0;
    txtSpeedVal.setWildcard(txtSpeedValBuf);
    txtSpeedVal.setTypedText(touchgfx::TypedText(T___SINGLEUSE_TATV));
    Unicode::strncpy(txtSpeedValBuf, "0", 16);
    add(txtSpeedVal);

    // Unité "km/h"
    txtSpeedUnit.setPosition(265, 245, 270, 20);
    txtSpeedUnit.setColor(touchgfx::Color::getColorFromRGB(142, 142, 147));
    txtSpeedUnit.setLinespacing(0);
    txtSpeedUnitBuf[0] = 0;
    txtSpeedUnit.setWildcard(txtSpeedUnitBuf);
    txtSpeedUnit.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtSpeedUnitBuf, "km/h", 8);
    add(txtSpeedUnit);

    // Boîte de Vitesses sous le compteur
    gearBox.setPosition(368, 345, 64, 36);
    gearBox.setColor(touchgfx::Color::getColorFromRGB(13, 27, 42));
    add(gearBox);

    gearBorder.setPosition(368, 343, 64, 2);
    gearBorder.setColor(touchgfx::Color::getColorFromRGB(0, 229, 255));
    add(gearBorder);

    txtGear.setPosition(368, 349, 64, 28);
    txtGear.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtGear.setLinespacing(0);
    txtGearBuf[0] = 0;
    txtGear.setWildcard(txtGearBuf);
    txtGear.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtGearBuf, "D", 8);
    add(txtGear);

    // ══════════════════════════════════════════════════════════════
    // 6. CADRAN DROITE : TEMP. MOTEUR (210x210, Orange #FF9500) - CENTRÉ VERTICALEMENT (Y: 90)
    // ══════════════════════════════════════════════════════════════
    tempOuterRing.setPosition(565, 90, 210, 210);
    tempOuterRing.setCenter(105, 105);
    tempOuterRing.setRadius(102);
    tempOuterRing.setLineWidth(3);
    tempOuterRing.setArc(0, 360);
    tempOuterRing.setPainter(painterTempOrange);
    add(tempOuterRing);

    tempInnerRing.setPosition(565, 90, 210, 210);
    tempInnerRing.setCenter(105, 105);
    tempInnerRing.setRadius(92);
    tempInnerRing.setLineWidth(1);
    tempInnerRing.setArc(0, 360);
    tempInnerRing.setPainter(painterBorderNavy);
    add(tempInnerRing);

    // 21 Graduations radiales Température (de -130° à +130°)
    for (int i = 0; i <= 20; i++)
    {
        float pct = (float)i / 20.0f;
        float angle = (-130.0f + pct * 260.0f) * 0.0174532925f;
        bool isMajor = (i % 5 == 0);
        float r1 = 96.0f;
        float r2 = isMajor ? 82.0f : 88.0f;

        tempTicks[i].setPosition(565, 90, 210, 210);
        tempTicks[i].setLineWidth(isMajor ? 2 : 1);
        tempTicks[i].setPainter(isMajor ? painterTempOrange : painterTempDarkTick);
        tempTicks[i].setLine(105 + (int)(r1 * sinf(angle)), 105 - (int)(r1 * cosf(angle)),
                             105 + (int)(r2 * sinf(angle)), 105 - (int)(r2 * cosf(angle)));
        add(tempTicks[i]);
    }

    // Aiguille Température (23 °C = angle nominal -8°)
    tempNeedle.setPosition(565, 90, 210, 210);
    tempNeedle.setLineWidth(4);
    tempNeedle.setPainter(painterTempOrange);
    float aTemp = -8.0f * 0.0174532925f;
    tempNeedle.setLine(105, 105, 105 + (int)(75.0f * sinf(aTemp)), 105 - (int)(75.0f * cosf(aTemp)));
    add(tempNeedle);

    // Moyeu central Température
    tempHubBorder.setPosition(565, 90, 210, 210);
    tempHubBorder.setCenter(105, 105);
    tempHubBorder.setRadius(7);
    tempHubBorder.setLineWidth(2);
    tempHubBorder.setPainter(painterSilver);
    add(tempHubBorder);

    tempHub.setPosition(565, 90, 210, 210);
    tempHub.setCenter(105, 105);
    tempHub.setRadius(5);
    tempHub.setLineWidth(5);
    tempHub.setPainter(painterWhite);
    add(tempHub);

    // Titre "TEMP. MOTEUR"
    txtTempTitle.setPosition(565, 107, 210, 20);
    txtTempTitle.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0));
    txtTempTitle.setLinespacing(0);
    txtTempTitleBuf[0] = 0;
    txtTempTitle.setWildcard(txtTempTitleBuf);
    txtTempTitle.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtTempTitleBuf, "TEMP. MOTEUR", 20);
    add(txtTempTitle);

    // Valeur digitale "23"
    txtTempVal.setPosition(565, 200, 210, 32);
    txtTempVal.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtTempVal.setLinespacing(0);
    txtTempValBuf[0] = 0;
    txtTempVal.setWildcard(txtTempValBuf);
    txtTempVal.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtTempValBuf, "23", 16);
    add(txtTempVal);

    // Unité "°C"
    txtTempUnit.setPosition(565, 230, 210, 20);
    txtTempUnit.setColor(touchgfx::Color::getColorFromRGB(142, 142, 147));
    txtTempUnit.setLinespacing(0);
    txtTempUnitBuf[0] = 0;
    txtTempUnit.setWildcard(txtTempUnitBuf);
    txtTempUnit.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtTempUnitBuf, "C", 8);
    add(txtTempUnit);

    // Widget Humidité placé SOUS le cadran droit (Température)
    txtHumidity.setPosition(565, 312, 210, 20);
    txtHumidity.setColor(touchgfx::Color::getColorFromRGB(142, 142, 147));
    txtHumidity.setLinespacing(0);
    txtHumidityBuf[0] = 0;
    txtHumidity.setWildcard(txtHumidityBuf);
    txtHumidity.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtHumidityBuf, "HUMIDITE : 42 %", 24);
    add(txtHumidity);

    humProgressBg.setPosition(595, 336, 150, 6);
    humProgressBg.setColor(touchgfx::Color::getColorFromRGB(30, 45, 69));
    add(humProgressBg);

    humProgressFill.setPosition(595, 336, 63, 6); // 42% de 150 = 63px
    humProgressFill.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88));
    add(humProgressFill);

    // ══════════════════════════════════════════════════════════════
    // 7. BARRE INFERIEURE : ODOMETRE & RADAR OBSTACLE (Fond uni sans séparation)
    // ══════════════════════════════════════════════════════════════
    // footerBar supprimée : les textes reposent directement sur le fond d'écran uni comme sur Qt !

    txtOdometer.setPosition(25, 444, 350, 22);
    txtOdometer.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtOdometer.setLinespacing(0);
    txtOdometerBuf[0] = 0;
    txtOdometer.setWildcard(txtOdometerBuf);
    txtOdometer.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtOdometerBuf, "ODOMETRE :  12543.0 km", 40);
    add(txtOdometer);

    txtObstacle.setPosition(460, 448, 320, 22);
    txtObstacle.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88));
    txtObstacle.setLinespacing(0);
    txtObstacleBuf[0] = 0;
    txtObstacle.setWildcard(txtObstacleBuf);
    txtObstacle.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtObstacleBuf, "DISTANCE OBSTACLE : 200 cm", 40);
    add(txtObstacle);

    // ══════════════════════════════════════════════════════════════
    // 7bis. BANNIERE DE NOTIFICATION OTA EN HAUT (Non intrusive)
    // ══════════════════════════════════════════════════════════════
    otaBannerContainer.setPosition(110, 12, 580, 36);

    otaBannerBox.setPosition(0, 0, 580, 36);
    otaBannerBox.setColor(touchgfx::Color::getColorFromRGB(18, 30, 46));
    otaBannerContainer.add(otaBannerBox);

    otaBannerBorder.setPosition(0, 34, 580, 2);
    otaBannerBorder.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0)); // Orange Alerte
    otaBannerContainer.add(otaBannerBorder);

    txtOtaBanner.setPosition(10, 8, 560, 22);
    txtOtaBanner.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0));
    txtOtaBanner.setLinespacing(0);
    txtOtaBannerBuf[0] = 0;
    txtOtaBanner.setWildcard(txtOtaBannerBuf);
    txtOtaBanner.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtOtaBannerBuf, "NOUVELLE MAJ v2.0.0 (SPORT) ! TOUCHER ICI POUR OUVRIR", 64);
    otaBannerContainer.add(txtOtaBanner);

    otaBannerContainer.setVisible(false);
    add(otaBannerContainer);

    // ══════════════════════════════════════════════════════════════
    // 8. POP-UP MODAL OTA EPUREE (Sans bordures violettes ni passerelle)
    // ══════════════════════════════════════════════════════════════
    otaModalContainer.setPosition(0, 0, 800, 480);

    otaOverlay.setPosition(0, 0, 800, 480);
    otaOverlay.setColor(touchgfx::Color::getColorFromRGB(0, 0, 0));
    otaModalContainer.add(otaOverlay);

    // Boite centrale épurée et moderne
    otaBox.setPosition(120, 110, 560, 260);
    otaBox.setColor(touchgfx::Color::getColorFromRGB(17, 22, 34));
    otaModalContainer.add(otaBox);

    // Titre centré
    txtOtaHeader.setPosition(140, 145, 520, 36);
    txtOtaHeader.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    txtOtaHeader.setLinespacing(0);
    txtOtaHeaderBuf[0] = 0;
    txtOtaHeader.setWildcard(txtOtaHeaderBuf);
    txtOtaHeader.setTypedText(touchgfx::TypedText(T___SINGLEUSE_TATV));
    Unicode::strncpy(txtOtaHeaderBuf, "MISE A JOUR DEMANDEE", 40);
    otaModalContainer.add(txtOtaHeader);

    // Bouton 1 : [ METTRE A JOUR ] (Vert)
    btnInstallBox.setPosition(140, 225, 230, 50);
    btnInstallBox.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88));
    otaModalContainer.add(btnInstallBox);

    btnInstallBorder.setPosition(140, 273, 230, 2);
    btnInstallBorder.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255));
    otaModalContainer.add(btnInstallBorder);

    txtBtnInstall.setPosition(140, 238, 230, 26);
    txtBtnInstall.setColor(touchgfx::Color::getColorFromRGB(0, 0, 0));
    txtBtnInstall.setLinespacing(0);
    txtBtnInstallBuf[0] = 0;
    txtBtnInstall.setWildcard(txtBtnInstallBuf);
    txtBtnInstall.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtBtnInstallBuf, "METTRE A JOUR", 24);
    otaModalContainer.add(txtBtnInstall);

    // Bouton 2 : [ PLUS TARD ] (Gris moderne)
    btnLaterBox.setPosition(430, 225, 230, 50);
    btnLaterBox.setColor(touchgfx::Color::getColorFromRGB(35, 45, 62));
    otaModalContainer.add(btnLaterBox);

    btnLaterBorder.setPosition(430, 273, 230, 2);
    btnLaterBorder.setColor(touchgfx::Color::getColorFromRGB(142, 142, 147));
    otaModalContainer.add(btnLaterBorder);

    txtBtnLater.setPosition(430, 238, 230, 26);
    txtBtnLater.setColor(touchgfx::Color::getColorFromRGB(220, 225, 235));
    txtBtnLater.setLinespacing(0);
    txtBtnLaterBuf[0] = 0;
    txtBtnLater.setWildcard(txtBtnLaterBuf);
    txtBtnLater.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtBtnLaterBuf, "PLUS TARD", 24);
    otaModalContainer.add(txtBtnLater);

    // Barre d'installation et statut
    otaInstallBarBg.setPosition(160, 235, 480, 14);
    otaInstallBarBg.setColor(touchgfx::Color::getColorFromRGB(30, 45, 69));
    otaInstallBarBg.setVisible(false);
    otaModalContainer.add(otaInstallBarBg);

    otaInstallBarFill.setPosition(160, 235, 0, 14);
    otaInstallBarFill.setColor(touchgfx::Color::getColorFromRGB(0, 229, 255));
    otaInstallBarFill.setVisible(false);
    otaModalContainer.add(otaInstallBarFill);

    txtOtaInstall.setPosition(140, 265, 520, 24);
    txtOtaInstall.setColor(touchgfx::Color::getColorFromRGB(0, 229, 255));
    txtOtaInstall.setLinespacing(0);
    txtOtaInstallBuf[0] = 0;
    txtOtaInstall.setWildcard(txtOtaInstallBuf);
    txtOtaInstall.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtOtaInstallBuf, "Validation Signature & Installation...", 48);
    txtOtaInstall.setVisible(false);
    otaModalContainer.add(txtOtaInstall);

    otaModalContainer.setVisible(false);
    add(otaModalContainer);

    // ══════════════════════════════════════════════════════════════
    // 9. SPLASH SCREEN QT (Amorçage Système Réplique Exacte)
    // ══════════════════════════════════════════════════════════════
    splashContainer.setPosition(0, 0, 800, 480);

    splashBg.setPosition(0, 0, 800, 480);
    splashBg.setColor(touchgfx::Color::getColorFromRGB(11, 14, 20));
    splashContainer.add(splashBg);

    // Splash screen épuré : uniquement LOUEY KRAYEM, AUTOMOTIVE SECURE OTA, et barre de progression
    txtSplashTitle.setPosition(50, 150, 700, 44);
    txtSplashTitle.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88)); // Louey Krayem en Vert Gras
    txtSplashTitle.setLinespacing(0);
    txtSplashTitleBuf[0] = 0;
    txtSplashTitle.setWildcard(txtSplashTitleBuf);
    txtSplashTitle.setTypedText(touchgfx::TypedText(T___SINGLEUSE_TATV)); // Dubai / Verdana Gras
    Unicode::strncpy(txtSplashTitleBuf, "LOUEY KRAYEM", 32);
    splashContainer.add(txtSplashTitle);

    txtSplashSub.setPosition(50, 205, 700, 28);
    txtSplashSub.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255)); // Automotive Secure OTA en Blanc
    txtSplashSub.setLinespacing(0);
    txtSplashSubBuf[0] = 0;
    txtSplashSub.setWildcard(txtSplashSubBuf);
    txtSplashSub.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtSplashSubBuf, "AUTOMOTIVE SECURE OTA", 64);
    splashContainer.add(txtSplashSub);

    splashProgressBg.setPosition(230, 265, 340, 10);
    splashProgressBg.setColor(touchgfx::Color::getColorFromRGB(20, 32, 48));
    splashContainer.add(splashProgressBg);

    splashProgressFill.setPosition(230, 265, 0, 10);
    splashProgressFill.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88)); // Barre de progression VERTE
    splashContainer.add(splashProgressFill);

    // txtSplashPct masqué pour supprimer l'affichage du pourcentage sous la barre
    txtSplashPct.setVisible(false);

    txtSplashStatus.setPosition(50, 340, 700, 22);
    txtSplashStatus.setColor(touchgfx::Color::getColorFromRGB(142, 142, 147));
    txtSplashStatus.setLinespacing(0);
    txtSplashStatusBuf[0] = 0;
    txtSplashStatus.setWildcard(txtSplashStatusBuf);
    txtSplashStatus.setTypedText(touchgfx::TypedText(T___SINGLEUSE_AH9P));
    Unicode::strncpy(txtSplashStatusBuf, "Initialisation Cortex-M4 (Firewall & CAN)...", 64);
    // txtSplashStatus masqué

    add(splashContainer);

    // ══════════════════════════════════════════════════════════════
    // 10. RESTAURATION COMPLETE V2 SPORT APRES RESET (Persistance)
    //     Exécuté ICI car les widgets sont maintenant tous créés !
    // ══════════════════════════════════════════════════════════════
    if (is_v2_updated)
    {
        // Compteur vitesse → Rouge Racing #FF3B30
        painterSpeedCyan.setColor(touchgfx::Color::getColorFromRGB(255, 59, 48));
        speedOuterRing.setPainter(painterSpeedCyan);
        speedNeedle.setPainter(painterSpeedCyan);
        txtSpeedTitle.setColor(touchgfx::Color::getColorFromRGB(255, 59, 48));
        Unicode::strncpy(txtSpeedTitleBuf, "VITESSE SPORT", 16);

        // Carburant → Orange #FF9500 (V2)
        painterFuelGreen.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0));
        fuelOuterRing.setPainter(painterFuelGreen);
        fuelNeedle.setPainter(painterFuelGreen);
        txtFuelTitle.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0));
        Unicode::strncpy(txtFuelTitleBuf, "NIVEAU ENERGIE", 16);

        // Température → Jaune Sport #FFD60A (V2)
        painterTempOrange.setColor(touchgfx::Color::getColorFromRGB(255, 214, 10));
        tempOuterRing.setPainter(painterTempOrange);
        tempNeedle.setPainter(painterTempOrange);

        // Humidité masquée en V2 (identique à Qt)
        txtHumidity.setVisible(false);
        humProgressBg.setVisible(false);
        humProgressFill.setVisible(false);

        // Boîte de vitesses → Rouge Sport
        gearBox.setColor(touchgfx::Color::getColorFromRGB(38, 14, 18));
        gearBorder.setColor(touchgfx::Color::getColorFromRGB(255, 59, 48));
        Unicode::strncpy(txtGearBuf, "S", 8);
    }
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}


void Screen1View::handleClickEvent(const touchgfx::ClickEvent& evt)
{
    Screen1ViewBase::handleClickEvent(evt);

    if (evt.getType() == touchgfx::ClickEvent::PRESSED)
    {
        int x = evt.getX();
        int y = evt.getY();

        // 1. Clic sur la bannière de notification en haut
        if (!ota_modal_active && !ota_installing)
        {
            // La modale s'ouvre UNIQUEMENT si une vraie notification OTA est en attente (et PAS lors d'une alerte directe !)
            // La garde direct_alert_ticks == 0 interdit absolument l'ouverture si une alerte rouge est active
            if (ota_notification_pending && direct_alert_ticks == 0 && (x >= 90 && x <= 710 && y >= 5 && y <= 70))
            {
                ota_modal_active = true;
                otaBannerContainer.setVisible(false);
                otaBannerContainer.invalidate();

                // Préparer la modale avec les deux boutons visibles
                btnInstallBox.setVisible(true);
                btnInstallBorder.setVisible(true);
                txtBtnInstall.setVisible(true);
                btnLaterBox.setVisible(true);
                btnLaterBorder.setVisible(true);
                txtBtnLater.setVisible(true);

                otaInstallBarBg.setVisible(false);
                otaInstallBarFill.setVisible(false);
                txtOtaInstall.setVisible(false);

                otaModalContainer.setVisible(true);
                otaModalContainer.invalidate();
                return;
            }
        }

        // 2. Clic dans la modale OTA
        if (ota_modal_active && !ota_installing)
        {
            // Clic sur "METTRE A JOUR" (X: 130..380, Y: 215..285)
            if (x >= 130 && x <= 380 && y >= 215 && y <= 285)
            {
                ota_installing = true;
                ota_ticks = 0;

                // Masquer les boutons
                btnInstallBox.setVisible(false);
                btnInstallBorder.setVisible(false);
                txtBtnInstall.setVisible(false);
                btnLaterBox.setVisible(false);
                btnLaterBorder.setVisible(false);
                txtBtnLater.setVisible(false);

                // Afficher la barre d'installation
                otaInstallBarBg.setVisible(true);
                otaInstallBarFill.setVisible(true);
                txtOtaInstall.setVisible(true);

                otaModalContainer.invalidate();
                return;
            }

            // Clic sur "PLUS TARD" (X: 420..670, Y: 215..285)
            if (x >= 420 && x <= 670 && y >= 215 && y <= 285)
            {
                ota_modal_active = false;
                otaModalContainer.setVisible(false);
                otaModalContainer.invalidate();

                // Réafficher la bannière de notification discrète en haut
                otaBannerContainer.setVisible(true);
                otaBannerContainer.invalidate();
                return;
            }
        }
    }
}

void Screen1View::updatePotData(uint32_t pot, uint32_t frames, uint32_t status)
{
    tick_div++;
    uint32_t can_id = status; // Passed directly from Model.cpp

    // ══════════════════════════════════════════════════════════════
    // 1. GESTION DU SPLASH SCREEN (~1.8 seconde)
    // ══════════════════════════════════════════════════════════════
    if (!splash_finished)
    {
        splash_ticks++;
        uint32_t pct = (splash_ticks * 100UL) / 100UL;
        if (pct > 100) pct = 100;

        int fillWidth = (int)((splash_ticks * 340UL) / 100UL);
        if (fillWidth > 340) fillWidth = 340;
        splashProgressFill.setWidth(fillWidth);
        splashProgressFill.invalidate();

        Unicode::snprintf(txtSplashPctBuf, 16, "%lu %%", (unsigned long)pct);
        txtSplashPct.invalidate();

        if (pct < 35)
        {
            Unicode::strncpy(txtSplashStatusBuf, "Initialisation Cortex-M4 (Firewall & CAN)...", 64);
        }
        else if (pct < 75)
        {
            Unicode::strncpy(txtSplashStatusBuf, "Initialisation Cortex-M7 (FreeRTOS & Cockpit)...", 64);
        }
        else
        {
            Unicode::strncpy(txtSplashStatusBuf, "Connexion CAN 500 kbps etablie. Demarrage...", 64);
        }
        txtSplashStatus.invalidate();

        if (splash_ticks >= 100)
        {
            splash_finished = true;
            splashContainer.setVisible(false);
            splashContainer.invalidate();
            invalidate(); // Révèle l'ensemble du dashboard Qt en dessous !
        }
        return;
    }

    // ══════════════════════════════════════════════════════════════
    // 2. GESTION DES COMMANDES CAN (0x170 Alerte, 0x180 OTA, 0x190 Reset)
    // ══════════════════════════════════════════════════════════════
    volatile uint32_t* pOtaCmd = (volatile uint32_t*)0x38000024UL;
    uint32_t ota_event_id = *pOtaCmd;
    if (ota_event_id != 0)
    {
        *pOtaCmd = 0; // Acquittement immédiat dans SRAM4 partagée
    }
    else if (can_id == 0x170 || can_id == 0x180 || can_id == 0x190)
    {
        static uint32_t last_raw_can_id = 0;
        if (can_id != last_raw_can_id)
        {
            ota_event_id = can_id;
            last_raw_can_id = can_id;
        }
    }

    if (ota_event_id != 0)
    {
        // A. CAN ID 0x170 : ALERTE VEHICULE DIRECTE (SANS AUCUNE MISE A JOUR)
        if (ota_event_id == 0x170)
        {
            direct_alert_ticks = 240; // ~4 secondes à 60 FPS
            ota_notification_pending = false; // Ne JAMAIS ouvrir de modale MAJ !
            ota_modal_active = false;
            ota_installing = false;
            otaModalContainer.setVisible(false); // Fermer toute modale éventuellement ouverte
            otaModalContainer.invalidate();

            otaBannerBorder.setColor(touchgfx::Color::getColorFromRGB(255, 59, 48)); // Rouge Alerte
            txtOtaBanner.setColor(touchgfx::Color::getColorFromRGB(255, 59, 48));
            Unicode::strncpy(txtOtaBannerBuf, "ALERTE VEHICULE : CONTROLE TELEMATIQUE ACTIF", 64);
            otaBannerContainer.setVisible(true);
            otaBannerContainer.invalidate();
        }
        // B. CAN ID 0x190 : NOTIFICATION DEMANDE RESTAURATION v1.0.0 ECO (par normal.bin)
        else if (ota_event_id == 0x190 && !ota_installing)
        {
            direct_alert_ticks = 0;
            ota_notification_pending = true;
            target_ota_v2 = false; // Cible = Mode Normal v1.0.0
            otaBannerBorder.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0)); // Bordure Orange
            txtOtaBanner.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255)); // Blanc gras
            Unicode::strncpy(txtOtaBannerBuf, "MISE A JOUR DEMANDEE", 64);
            otaBannerContainer.setVisible(true);
            otaBannerContainer.invalidate();
        }
        // C. CAN ID 0x180 : NOTIFICATION DEMANDE MAJ v2.0.0 SPORT (par update.bin)
        else if (ota_event_id == 0x180 && !ota_installing)
        {
            direct_alert_ticks = 0;
            ota_notification_pending = true;
            target_ota_v2 = true; // Cible = Mode Sport v2.0.0
            otaBannerBorder.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0)); // Bordure Orange
            txtOtaBanner.setColor(touchgfx::Color::getColorFromRGB(255, 255, 255)); // Blanc gras
            Unicode::strncpy(txtOtaBannerBuf, "MISE A JOUR DEMANDEE", 64);
            otaBannerContainer.setVisible(true);
            otaBannerContainer.invalidate();
        }
    }

    // Décrément timer alerte directe / confirmation de restauration
    if (direct_alert_ticks > 0)
    {
        direct_alert_ticks--;
        if (direct_alert_ticks == 0 && !ota_notification_pending)
        {
            otaBannerContainer.setVisible(false);
            otaBannerContainer.invalidate();
        }
    }

    // Installation UNIQUEMENT si l'utilisateur a clique sur [METTRE A JOUR]
    if (ota_installing)
    {
        ota_ticks++;
        int barW = (int)((ota_ticks * 480UL) / 100UL);
        if (barW > 480) barW = 480;
        otaInstallBarFill.setWidth(barW);
        otaInstallBarFill.invalidate();

        if (ota_ticks < 35)
        {
            Unicode::strncpy(txtOtaInstallBuf, "Reception flux binaire OTA...", 48);
        }
        else if (ota_ticks < 75)
        {
            Unicode::strncpy(txtOtaInstallBuf, "Verification Signature cryptographique (ISO 21434)...", 48);
        }
        else
        {
            Unicode::strncpy(txtOtaInstallBuf, "Installation Firmware v2.0.0 Terminee !", 48);
        }
        txtOtaInstall.invalidate();

        if (ota_ticks >= 100)
        {
            ota_installing = false;
            ota_modal_active = false;
            ota_notification_pending = false;

            otaModalContainer.setVisible(false);
            otaModalContainer.invalidate();

            otaBannerContainer.setVisible(false);
            otaBannerContainer.invalidate();

            H7_RCC_APB4ENR |= (1UL << 4);
            H7_PWR_CR1 |= 0x100;
            for (volatile int _dly = 0; _dly < 2000; _dly++) {}

            if (target_ota_v2)
            {
                // ══════════════════════════════════════════════════════════════
                // BASCULE VERS FIRMWARE v2.0.0 SPORT COCKPIT !
                // ══════════════════════════════════════════════════════════════
                is_v2_updated = true;
                H7_RTC_BKP0R = OTA_MAGIC_V2;
                H7_SRAM4_FLAG = OTA_MAGIC_V2;

                painterSpeedCyan.setColor(touchgfx::Color::getColorFromRGB(255, 59, 48)); // #FF3B30 Rouge Racing
                speedOuterRing.setPainter(painterSpeedCyan);
                speedNeedle.setPainter(painterSpeedCyan);
                txtSpeedTitle.setColor(touchgfx::Color::getColorFromRGB(255, 59, 48));
                Unicode::strncpy(txtSpeedTitleBuf, "VITESSE SPORT", 16);

                // Carburant → Orange #FF9500 (V2)
                painterFuelGreen.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0));
                fuelOuterRing.setPainter(painterFuelGreen);
                fuelNeedle.setPainter(painterFuelGreen);
                txtFuelTitle.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0));
                Unicode::strncpy(txtFuelTitleBuf, "NIVEAU ENERGIE", 16);

                // Température → Jaune Sport #FFD60A (V2)
                painterTempOrange.setColor(touchgfx::Color::getColorFromRGB(255, 214, 10));
                tempOuterRing.setPainter(painterTempOrange);
                tempNeedle.setPainter(painterTempOrange);

                // Humidité masquée en V2 (identique à Qt)
                txtHumidity.setVisible(false);
                humProgressBg.setVisible(false);
                humProgressFill.setVisible(false);

                gearBox.setColor(touchgfx::Color::getColorFromRGB(38, 14, 18));
                gearBorder.setColor(touchgfx::Color::getColorFromRGB(255, 59, 48));
                Unicode::strncpy(txtGearBuf, "S", 8);
            }
            else
            {
                // ══════════════════════════════════════════════════════════════
                // RESTAURATION FIRMWARE v1.0.0 STANDARD ECO !
                // ══════════════════════════════════════════════════════════════
                is_v2_updated = false;
                H7_RTC_BKP0R = 0x00000000UL;
                H7_SRAM4_FLAG = 0x00000000UL;

                painterSpeedCyan.setColor(touchgfx::Color::getColorFromRGB(0, 229, 255)); // #00E5FF Cyan
                speedOuterRing.setPainter(painterSpeedCyan);
                speedNeedle.setPainter(painterSpeedCyan);
                txtSpeedTitle.setColor(touchgfx::Color::getColorFromRGB(0, 229, 255));
                Unicode::strncpy(txtSpeedTitleBuf, "VITESSE", 16);

                // Carburant → Vert #30D158 (V1)
                painterFuelGreen.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88));
                fuelOuterRing.setPainter(painterFuelGreen);
                fuelNeedle.setPainter(painterFuelGreen);
                txtFuelTitle.setColor(touchgfx::Color::getColorFromRGB(48, 209, 88));
                Unicode::strncpy(txtFuelTitleBuf, "CARBURANT", 16);

                // Température → Orange #FF9500 (V1)
                painterTempOrange.setColor(touchgfx::Color::getColorFromRGB(255, 149, 0));
                tempOuterRing.setPainter(painterTempOrange);
                tempNeedle.setPainter(painterTempOrange);

                // Humidité réaffichée en V1
                txtHumidity.setVisible(true);
                humProgressBg.setVisible(true);
                humProgressFill.setVisible(true);

                gearBox.setColor(touchgfx::Color::getColorFromRGB(13, 27, 42));
                gearBorder.setColor(touchgfx::Color::getColorFromRGB(0, 229, 255));
                Unicode::strncpy(txtGearBuf, "D", 8);
            }

            last_rendered_speed = 9999;
            invalidate();
        }
        return;
    }

    // ══════════════════════════════════════════════════════════════
    // 3. DASHBOARD PRINCIPAL DYNAMIQUE
    // ══════════════════════════════════════════════════════════════

    // 1. Vitesse Réelle (0..4095 -> 0..200 km/h en v1, 0..260 km/h en v2)
    uint32_t max_spd = is_v2_updated ? 260UL : 200UL;
    uint32_t speed_kmh = (pot * max_spd) / 4095UL;
    if (speed_kmh > max_spd) speed_kmh = max_spd;

    if (speed_kmh != last_rendered_speed)
    {
        last_rendered_speed = speed_kmh;

        // Angle aiguille : -130° (0 km/h) à +130° (max km/h)
        float speed_deg = -130.0f + ((float)speed_kmh * 260.0f) / (float)max_spd;
        float aSpd = speed_deg * 0.0174532925f;
        int nx = 135 + (int)(95.0f * sinf(aSpd));
        int ny = 135 - (int)(95.0f * cosf(aSpd));

        speedNeedle.invalidate();
        speedNeedle.setLine(135, 135, nx, ny);
        touchgfx::Rect rOld = speedNeedle.getMinimalRect();
        speedNeedle.setLine(135, 135, nx, ny);
        touchgfx::Rect rNew = speedNeedle.getMinimalRect();
        speedNeedle.invalidateRect(rOld);
        speedNeedle.invalidateRect(rNew);
        speedHub.invalidate();

        // Mise à jour de la valeur digitale de vitesse en temps réel !
        Unicode::snprintf(txtSpeedValBuf, 16, "%u", (unsigned int)speed_kmh);
        txtSpeedVal.invalidate();
    }

    // 2. Boîte de Vitesses Réactive (mise à jour sélective pour 60 FPS purs)
    static char last_gear = ' ';
    char cur_gear = (speed_kmh == 0) ? 'P' : (is_v2_updated ? 'S' : 'D');
    if (cur_gear != last_gear)
    {
        last_gear = cur_gear;
        txtGearBuf[0] = cur_gear;
        txtGearBuf[1] = 0;
        txtGear.invalidate();
    }

    // 3. Mise à jour douce de l'odomètre
    if ((tick_div % 30) == 0 && speed_kmh > 0)
    {
        current_odometer += (float)speed_kmh * 0.0003f;
        int odo_int = (int)current_odometer;
        int odo_dec = (int)((current_odometer - (float)odo_int) * 10.0f);
        Unicode::snprintf(txtOdometerBuf, 40, "ODOMETRE :  %d.%d km", odo_int, odo_dec);
        txtOdometer.invalidate();
    }
}
