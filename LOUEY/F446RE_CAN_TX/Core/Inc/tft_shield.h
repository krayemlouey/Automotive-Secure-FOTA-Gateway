#ifndef INC_TFT_SHIELD_H_
#define INC_TFT_SHIELD_H_

#include "main.h"

/* Choix du contrôleur :
 * La grande majorité des shields 2.4" ITEAD v2 récents utilisent ILI9341.
 * Les anciennes versions utilisaient S6D1121.
 * Par défaut, ILI9341 est activé. Si l'écran reste blanc, commentez ILI9341 et activez S6D1121.
 */
//#define TFT_CONTROLLER_ILI9341
#define TFT_CONTROLLER_S6D1121

/* Dimensions physiques de l'écran (mode portrait 240x320) */
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

/* Couleurs au format RGB565 (16 bits) */
#define TFT_BLACK       0x0000
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
#define TFT_LIGHTGREY   0xC618
#define TFT_DARKGREY    0x7BEF
#define TFT_BLUE        0x001F
#define TFT_GREEN       0x07E0
#define TFT_CYAN        0x07FF
#define TFT_RED         0xF800
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_WHITE       0xFFFF
#define TFT_ORANGE      0xFD20

/* Prototypes des fonctions du driver */
void TFT_Init(void);
void TFT_WriteCmd(uint8_t cmd);
void TFT_WriteData(uint8_t data);
void TFT_WriteData16(uint16_t data);
void TFT_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void TFT_FillScreen(uint16_t color);
void TFT_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void TFT_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);
void TFT_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

#endif /* INC_TFT_SHIELD_H_ */
