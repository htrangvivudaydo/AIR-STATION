/**
  ******************************************************************************
  * @file    st7735.c
  * @brief   Driver TFT 1.44" ST7735 - tích hợp font 5x7 cơ bản
  ******************************************************************************
  */
#include "st7735.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;
static SPI_HandleTypeDef *st7735_hspi;

/* ---- Macro điều khiển chân (đồng bộ với CubeMX) ------------------------ */
#define CS_LOW()    HAL_GPIO_WritePin(TFT_CS_GPIO_Port,  TFT_CS_Pin,  GPIO_PIN_RESET)
#define CS_HIGH()   HAL_GPIO_WritePin(TFT_CS_GPIO_Port,  TFT_CS_Pin,  GPIO_PIN_SET)
#define DC_LOW()    HAL_GPIO_WritePin(TFT_DC_GPIO_Port,  TFT_DC_Pin,  GPIO_PIN_RESET)
#define DC_HIGH()   HAL_GPIO_WritePin(TFT_DC_GPIO_Port,  TFT_DC_Pin,  GPIO_PIN_SET)
#define RST_LOW()   HAL_GPIO_WritePin(TFT_RES_GPIO_Port, TFT_RES_Pin, GPIO_PIN_RESET)
#define RST_HIGH()  HAL_GPIO_WritePin(TFT_RES_GPIO_Port, TFT_RES_Pin, GPIO_PIN_SET)

/* ============================================================================
 *  Font 5x7 ASCII (32-126) - mỗi ký tự 5 byte, mỗi byte là 1 cột pixel
 * ==========================================================================*/
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' ' (32)
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x08,0x2A,0x1C,0x2A,0x08}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x00,0x08,0x14,0x22,0x41}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x41,0x22,0x14,0x08,0x00}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x01,0x01}, // 'F'
    {0x3E,0x41,0x41,0x51,0x32}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x04,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x7F,0x20,0x18,0x20,0x7F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x03,0x04,0x78,0x04,0x03}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x00,0x7F,0x41,0x41}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x41,0x41,0x7F,0x00,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x08,0x14,0x54,0x54,0x3C}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x00,0x7F,0x10,0x28,0x44}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
};

/* ============================================================================
 *  Hàm truyền SPI mức thấp
 * ==========================================================================*/
static void ST7735_WriteCmd(uint8_t cmd)
{
    DC_LOW();
    CS_LOW();
    HAL_SPI_Transmit(st7735_hspi, &cmd, 1, HAL_MAX_DELAY);
    CS_HIGH();
}

static void ST7735_WriteData(uint8_t *data, uint16_t len)
{
    DC_HIGH();
    CS_LOW();
    HAL_SPI_Transmit(st7735_hspi, data, len, HAL_MAX_DELAY);
    CS_HIGH();
}

static void ST7735_WriteData8(uint8_t data)
{
    ST7735_WriteData(&data, 1);
}

/* Đặt cửa sổ vẽ */
static void ST7735_SetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];

    ST7735_WriteCmd(ST7735_CASET);
    buf[0] = 0x00; buf[1] = x0 + ST7735_XSTART;
    buf[2] = 0x00; buf[3] = x1 + ST7735_XSTART;
    ST7735_WriteData(buf, 4);

    ST7735_WriteCmd(ST7735_RASET);
    buf[0] = 0x00; buf[1] = y0 + ST7735_YSTART;
    buf[2] = 0x00; buf[3] = y1 + ST7735_YSTART;
    ST7735_WriteData(buf, 4);

    ST7735_WriteCmd(ST7735_RAMWR);
}

/* ============================================================================
 *  Khởi tạo
 * ==========================================================================*/
void ST7735_Init(SPI_HandleTypeDef *hspi)
{
    st7735_hspi = hspi;

    /* Reset cứng */
    CS_HIGH();
    RST_HIGH(); HAL_Delay(50);
    RST_LOW();  HAL_Delay(50);
    RST_HIGH(); HAL_Delay(150);

    ST7735_WriteCmd(ST7735_SWRESET);
    HAL_Delay(150);

    ST7735_WriteCmd(ST7735_SLPOUT);
    HAL_Delay(150);

    /* Color mode: 16-bit RGB565 */
    ST7735_WriteCmd(ST7735_COLMOD);
    ST7735_WriteData8(0x05);

    /* MADCTL: hướng quay màn hình - 0xC0 = xoay 0°, RGB */
    ST7735_WriteCmd(ST7735_MADCTL);
    ST7735_WriteData8(0xC0);

    ST7735_WriteCmd(ST7735_DISPON);
    HAL_Delay(100);

    ST7735_FillScreen(ST7735_BLACK);
}

/* ============================================================================
 *  Vẽ pixel & vùng
 * ==========================================================================*/
void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    ST7735_SetAddrWindow(x, y, x, y);
    uint8_t data[2] = { color >> 8, color & 0xFF };
    ST7735_WriteData(data, 2);
}

void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    if (x + w > ST7735_WIDTH)  w = ST7735_WIDTH  - x;
    if (y + h > ST7735_HEIGHT) h = ST7735_HEIGHT - y;

    ST7735_SetAddrWindow(x, y, x + w - 1, y + h - 1);

    uint8_t hi = color >> 8, lo = color & 0xFF;
    uint8_t line_buf[ST7735_WIDTH * 2];
    for (uint16_t i = 0; i < w; i++) {
        line_buf[i * 2]     = hi;
        line_buf[i * 2 + 1] = lo;
    }

    DC_HIGH();
    CS_LOW();
    for (uint16_t row = 0; row < h; row++) {
        HAL_SPI_Transmit(st7735_hspi, line_buf, w * 2, HAL_MAX_DELAY);
    }
    CS_HIGH();
}

void ST7735_FillScreen(uint16_t color)
{
    ST7735_FillRect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    ST7735_FillRect(x, y, w, 1, color);
}

void ST7735_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    ST7735_FillRect(x, y, 1, h, color);
}

/* ============================================================================
 *  Vẽ chữ
 * ==========================================================================*/
void ST7735_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t size)
{
    if (c < 32 || c > 122) c = '?';
    const uint8_t *glyph = font5x7[c - 32];

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            uint16_t color = (line & (1 << row)) ? fg : bg;
            if (size == 1) {
                ST7735_DrawPixel(x + col, y + row, color);
            } else {
                ST7735_FillRect(x + col * size, y + row * size, size, size, color);
            }
        }
    }
    /* Cột trống phân cách */
    if (size == 1) {
        for (uint8_t row = 0; row < 7; row++) ST7735_DrawPixel(x + 5, y + row, bg);
    } else {
        ST7735_FillRect(x + 5 * size, y, size, 7 * size, bg);
    }
}

void ST7735_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size)
{
    uint16_t cur_x = x;
    while (*str) {
        if (cur_x + 6 * size > ST7735_WIDTH) break;
        ST7735_DrawChar(cur_x, y, *str, fg, bg, size);
        cur_x += 6 * size;
        str++;
    }
}
