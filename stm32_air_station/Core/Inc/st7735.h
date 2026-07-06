/**
  ******************************************************************************
  * @file    st7735.h
  * @brief   Driver cho TFT 1.44" sử dụng IC ST7735 (128x128) qua SPI
  ******************************************************************************
  */
#ifndef ST7735_H
#define ST7735_H

#include "main.h"
#include <stdint.h>

/* ---- Kích thước màn hình 1.44" ----------------------------------------- */
#define ST7735_WIDTH    128
#define ST7735_HEIGHT   160
#define ST7735_XSTART   0       // Offset cho biến thể 1.8"
#define ST7735_YSTART   0

/* ---- Màu sắc 16-bit RGB565 --------------------------------------------- */
#define ST7735_BLACK    0x0000
#define ST7735_WHITE    0xFFFF
#define ST7735_RED      0xF800
#define ST7735_GREEN    0x07E0
#define ST7735_BLUE     0x001F
#define ST7735_YELLOW   0xFFE0
#define ST7735_CYAN     0x07FF
#define ST7735_MAGENTA  0xF81F
#define ST7735_ORANGE   0xFC00
#define ST7735_GRAY     0x8410

/* ---- Lệnh ST7735 -------------------------------------------------------- */
#define ST7735_NOP      0x00
#define ST7735_SWRESET  0x01
#define ST7735_SLPOUT   0x11
#define ST7735_DISPON   0x29
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_MADCTL   0x36
#define ST7735_COLMOD   0x3A

/* ---- API ---------------------------------------------------------------- */
void ST7735_Init(SPI_HandleTypeDef *hspi);
void ST7735_FillScreen(uint16_t color);
void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7735_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size);
void ST7735_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size);
void ST7735_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void ST7735_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color);

#endif /* ST7735_H */
