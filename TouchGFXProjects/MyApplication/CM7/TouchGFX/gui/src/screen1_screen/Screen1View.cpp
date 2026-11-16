#include <gui/screen1_screen/Screen1View.hpp>
#include <touchgfx/Color.hpp>
#include <touchgfx/Unicode.hpp>
#include <cstdio>
#include <cstring>

Screen1View::Screen1View()
{

}

void Screen1View::setupScreen()
{
    Screen1ViewBase::setupScreen();

    // Configuration des échelles des graphes TouchGFX
    // BLOC 1 : Potentiomètre analogique (0.0V à 3.3V)
    dynamicGraph1.clear();
    dynamicGraph1.setGraphRangeY(0.0f, 3.3f);

    // BLOC 3 : Trame créneaux IA Rafale (0.0 ou 1.0)
    graphSquareBurst.clear();
    graphSquareBurst.setGraphRangeY(0.0f, 1.2f);

    // BLOC 5 : Capture d'erreur gelée (0.0 ou 1.0)
    dynamicGraph2.clear();
    dynamicGraph2.setGraphRangeY(0.0f, 1.2f);

    // Couleurs par défaut (État normal de démarrage)
    boxStatusGreen.setColor(touchgfx::Color::getColorFromRGB(10, 45, 25));
    boxStatusGreen.invalidate();

    boxErrorCapture.setColor(touchgfx::Color::getColorFromRGB(25, 25, 32));
    boxErrorCapture.invalidate();

    Unicode::strncpy(textErrorDescriptionBuffer, "Trafic normal certifie (TinyML)", TEXTERRORDESCRIPTION_SIZE);
    textErrorDescription.setColor(touchgfx::Color::getColorFromRGB(200, 255, 200));
    textErrorDescription.invalidate();
}

void Screen1View::tearDownScreen()
{
    Screen1ViewBase::tearDownScreen();
}

void Screen1View::updatePotData(uint32_t pot, uint32_t frames, uint32_t status)
{
    static uint32_t prev_pot    = 0xFFFFFFFFUL;
    static uint32_t prev_frames = 0xFFFFFFFFUL;
    static uint32_t prev_status = 0xFFFFFFFFUL;
    static uint32_t tick_counter = 0;

    tick_counter++;

    bool pot_changed    = (pot != prev_pot);
    bool frames_changed = (frames != prev_frames);
    bool status_changed = (status != prev_status);

    if (!pot_changed && !frames_changed && !status_changed && (tick_counter % 2 != 0))
    {
        return;
    }

    prev_pot    = pot;
    prev_frames = frames;

    // =========================================================================
    // BLOC 1 : Signal analogique (Courbe verte fluide du potentiomètre)
    // =========================================================================
    float pot_volts = ((float)pot * 3.3f) / 4095.0f;
    dynamicGraph1.addDataPoint(pot_volts);

    // =========================================================================
    // BLOC 2 & 3 : Trame Créneaux & Mode IA Rafale
    // =========================================================================
    if (status != 0)
    {
        // En cas d'attaque (DoS, Spoofing, Fuzzing) : activité haute fréquence
        float burst_val = ((tick_counter % 2) == 0) ? 1.0f : 0.0f;
        graphSquareBurst.addDataPoint(burst_val);
    }
    else
    {
        // Au repos : niveau bas ou activité intermittente
        float square_burst_val = ((tick_counter % 16) < 2) ? 1.0f : 0.0f;
        graphSquareBurst.addDataPoint(square_burst_val);
    }

    // =========================================================================
    // BLOCS 4, 5, 6 : ECU Contrôleur CAN & Diagnostic TinyML H7
    // =========================================================================
    if (status_changed || frames_changed)
    {
        prev_status = status;

        if (frames == 0)
        {
            // --- EN ATTENTE DU SIGNAL CAN PHYSIQUE ---
            boxStatusGreen.setColor(touchgfx::Color::getColorFromRGB(20, 20, 25));
            boxStatusGreen.invalidate();

            boxErrorCapture.setColor(touchgfx::Color::getColorFromRGB(25, 25, 32));
            boxErrorCapture.invalidate();

            textTinyMLStatus.setColor(touchgfx::Color::getColorFromRGB(255, 179, 0));
            textTinyMLStatus.invalidate();

            Unicode::strncpy(textFrameDetailsBuffer, "ATTENTE CAN (PMOD P3)", TEXTFRAMEDETAILS_SIZE);
            textFrameDetails.invalidate();

            Unicode::strncpy(textErrorDescriptionBuffer, "PMOD: Pin1=RX, Pin4=TX", TEXTERRORDESCRIPTION_SIZE);
            textErrorDescription.setColor(touchgfx::Color::getColorFromRGB(255, 200, 100));
            textErrorDescription.invalidate();

            dynamicGraph2.addDataPoint(0.0f);
        }
        else if (status == 0)
        {
            // --- ÉTAT NORMAL (Trafic certifié sain par TinyML) ---
            boxStatusGreen.setColor(touchgfx::Color::getColorFromRGB(10, 45, 25));
            boxStatusGreen.invalidate();

            boxErrorCapture.setColor(touchgfx::Color::getColorFromRGB(25, 25, 32));
            boxErrorCapture.invalidate();

            textTinyMLStatus.setColor(touchgfx::Color::getColorFromRGB(0, 255, 136));
            textTinyMLStatus.invalidate();

            Unicode::snprintf(textFrameDetailsBuffer, TEXTFRAMEDETAILS_SIZE, "CAN ACTIF | Trame #%lu", (unsigned long)frames);
            textFrameDetails.invalidate();

            Unicode::strncpy(textErrorDescriptionBuffer, "Trafic normal certifie (TinyML)", TEXTERRORDESCRIPTION_SIZE);
            textErrorDescription.setColor(touchgfx::Color::getColorFromRGB(0, 255, 136));
            textErrorDescription.invalidate();

            // Pas d'anomalie : capture au repos
            dynamicGraph2.addDataPoint(0.0f);
        }
        else
        {
            // --- ÉTAT ANOMALIE / ATTAQUE DÉTECTÉE ---
            // Surlignage visuel du bloc erreur en rouge
            boxStatusGreen.setColor(touchgfx::Color::getColorFromRGB(30, 30, 35));
            boxStatusGreen.invalidate();

            boxErrorCapture.setColor(touchgfx::Color::getColorFromRGB(70, 15, 20));
            boxErrorCapture.invalidate();

            textTinyMLStatus.setColor(touchgfx::Color::getColorFromRGB(255, 80, 80));
            textTinyMLStatus.invalidate();

            // BLOC 5 : Capture d'erreur gelée / signal corrompu
            float error_pulse = ((tick_counter % 3) == 0) ? 1.0f : 0.0f;
            dynamicGraph2.addDataPoint(error_pulse);

            // BLOC 6 : Message d'erreur explicite selon le diagnostic TinyML
            const char* detail_msg;
            switch (status)
            {
                case 1:
                    detail_msg = "ALERTE DoS FLOODING: dt < 2ms";
                    Unicode::strncpy(textFrameDetailsBuffer, "0x000 | FLOOD | dt: 1ms", TEXTFRAMEDETAILS_SIZE);
                    break;
                case 2:
                    detail_msg = "ALERTE SPOOFING: ID 0x100 usurp";
                    Unicode::strncpy(textFrameDetailsBuffer, "0x100 | SPOOF | dt: 5ms", TEXTFRAMEDETAILS_SIZE);
                    break;
                case 3:
                    detail_msg = "ALERTE FUZZING: Corruption DLC";
                    Unicode::strncpy(textFrameDetailsBuffer, "0x??? | FUZZ | DLC err", TEXTFRAMEDETAILS_SIZE);
                    break;
                default:
                    detail_msg = "ANOMALIE PROTOCOLE: Timing CAN";
                    Unicode::strncpy(textFrameDetailsBuffer, "ANOMALIE CAN INCONNUE", TEXTFRAMEDETAILS_SIZE);
                    break;
            }

            Unicode::strncpy(textErrorDescriptionBuffer, detail_msg, TEXTERRORDESCRIPTION_SIZE);
            textErrorDescription.setColor(touchgfx::Color::getColorFromRGB(255, 80, 80));
            textErrorDescription.invalidate();
            textFrameDetails.invalidate();
        }
    }
}
