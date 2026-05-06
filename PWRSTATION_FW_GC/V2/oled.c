/**
  ******************************************************************************
  * @file    oled.c
  * @brief   SSD1306 128x64 I2C OLED driver — DIYmall 0.96" module
  *
  *  Changes from the previous SSD1309 driver
  *  ─────────────────────────────────────────
  *  1. oled_init() — charge pump enable added (0x8D, 0x14). Without this
  *     the SSD1306 panel stays completely dark even though I2C works.
  *
  *  2. oled_init() — RST pulse removed. SSD1306 has internal power-on
  *     reset. No GPIO toggling needed.
  *
  *  3. oled_flush() — HAL_MAX_DELAY replaced with OLED_I2C_TIMEOUT_MS
  *     so a missing device returns an error instead of hanging forever.
  *
  *  4. Everything else (font table, frame buffer, flush logic, text
  *     rendering) is identical to the SSD1309 driver — no changes needed.
  ******************************************************************************
  */

#include "oled.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ============================================================================
 *  5x7 ASCII font, chars 0x20 (' ') through 0x7E ('~')
 *  Each entry = 5 column bytes. LSB of each byte = top pixel of that column.
 *  oled_print_str() appends a blank 6th column as a character gap.
 * ========================================================================== */
static const uint8_t FONT5X7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // 0x20 ' '
    {0x00,0x00,0x5F,0x00,0x00}, // 0x21 '!'
    {0x00,0x07,0x00,0x07,0x00}, // 0x22 '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // 0x23 '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // 0x24 '$'
    {0x23,0x13,0x08,0x64,0x62}, // 0x25 '%'
    {0x36,0x49,0x55,0x22,0x50}, // 0x26 '&'
    {0x00,0x05,0x03,0x00,0x00}, // 0x27 '\''
    {0x00,0x1C,0x22,0x41,0x00}, // 0x28 '('
    {0x00,0x41,0x22,0x1C,0x00}, // 0x29 ')'
    {0x14,0x08,0x3E,0x08,0x14}, // 0x2A '*'
    {0x08,0x08,0x3E,0x08,0x08}, // 0x2B '+'
    {0x00,0x50,0x30,0x00,0x00}, // 0x2C ','
    {0x08,0x08,0x08,0x08,0x08}, // 0x2D '-'
    {0x00,0x60,0x60,0x00,0x00}, // 0x2E '.'
    {0x20,0x10,0x08,0x04,0x02}, // 0x2F '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // 0x30 '0'
    {0x00,0x42,0x7F,0x40,0x00}, // 0x31 '1'
    {0x42,0x61,0x51,0x49,0x46}, // 0x32 '2'
    {0x21,0x41,0x45,0x4B,0x31}, // 0x33 '3'
    {0x18,0x14,0x12,0x7F,0x10}, // 0x34 '4'
    {0x27,0x45,0x45,0x45,0x39}, // 0x35 '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // 0x36 '6'
    {0x01,0x71,0x09,0x05,0x03}, // 0x37 '7'
    {0x36,0x49,0x49,0x49,0x36}, // 0x38 '8'
    {0x06,0x49,0x49,0x29,0x1E}, // 0x39 '9'
    {0x00,0x36,0x36,0x00,0x00}, // 0x3A ':'
    {0x00,0x56,0x36,0x00,0x00}, // 0x3B ';'
    {0x08,0x14,0x22,0x41,0x00}, // 0x3C '<'
    {0x14,0x14,0x14,0x14,0x14}, // 0x3D '='
    {0x00,0x41,0x22,0x14,0x08}, // 0x3E '>'
    {0x02,0x01,0x51,0x09,0x06}, // 0x3F '?'
    {0x32,0x49,0x79,0x41,0x3E}, // 0x40 '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 0x41 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 0x42 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 0x43 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 0x44 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 0x45 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 0x46 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 0x47 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 0x48 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 0x49 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 0x4A 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 0x4B 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 0x4C 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 0x4D 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 0x4E 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 0x4F 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 0x50 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 0x51 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 0x52 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 0x53 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 0x54 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 0x55 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 0x56 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 0x57 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 0x58 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 0x59 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 0x5A 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // 0x5B '['
    {0x02,0x04,0x08,0x10,0x20}, // 0x5C '\'
    {0x00,0x41,0x41,0x7F,0x00}, // 0x5D ']'
    {0x04,0x02,0x01,0x02,0x04}, // 0x5E '^'
    {0x40,0x40,0x40,0x40,0x40}, // 0x5F '_'
    {0x00,0x01,0x02,0x04,0x00}, // 0x60 '`'
    {0x20,0x54,0x54,0x54,0x78}, // 0x61 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 0x62 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 0x63 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 0x64 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 0x65 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 0x66 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 0x67 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 0x68 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 0x69 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 0x6A 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 0x6B 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 0x6C 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 0x6D 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 0x6E 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 0x6F 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 0x70 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 0x71 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 0x72 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 0x73 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 0x74 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 0x75 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 0x76 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 0x77 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 0x78 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 0x79 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 0x7A 'z'
    {0x00,0x08,0x36,0x41,0x00}, // 0x7B '{'
    {0x00,0x00,0x7F,0x00,0x00}, // 0x7C '|'
    {0x00,0x41,0x36,0x08,0x00}, // 0x7D '}'
    {0x10,0x08,0x08,0x10,0x08}, // 0x7E '~'
};

/* ============================================================================
 *  Module-private state
 * ========================================================================== */
static I2C_HandleTypeDef *s_hi2c = NULL;
static uint8_t            s_addr = 0x3C;

/* Frame buffer: 128 columns x 8 pages = 1024 bytes, 1 bit per pixel */
static uint8_t s_buf[OLED_BUF_SIZE];

/* ============================================================================
 *  Low-level I2C command helpers
 *
 *  SSD1306 I2C control byte:
 *    0x00 = Co=0, D/C#=0 → command byte follows
 *    0x40 = Co=0, D/C#=1 → data stream follows
 * ========================================================================== */

oled_status_t oled_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };
    if (HAL_I2C_Master_Transmit(s_hi2c, (uint16_t)(s_addr << 1),
                                buf, 2, OLED_I2C_TIMEOUT_MS) != HAL_OK)
        return OLED_ERR_I2C;
    return OLED_OK;
}

oled_status_t oled_cmd2(uint8_t cmd, uint8_t arg)
{
    uint8_t buf[3] = { 0x00, cmd, arg };
    if (HAL_I2C_Master_Transmit(s_hi2c, (uint16_t)(s_addr << 1),
                                buf, 3, OLED_I2C_TIMEOUT_MS) != HAL_OK)
        return OLED_ERR_I2C;
    return OLED_OK;
}

oled_status_t oled_cmd3(uint8_t cmd, uint8_t arg1, uint8_t arg2)
{
    uint8_t buf[4] = { 0x00, cmd, arg1, arg2 };
    if (HAL_I2C_Master_Transmit(s_hi2c, (uint16_t)(s_addr << 1),
                                buf, 4, OLED_I2C_TIMEOUT_MS) != HAL_OK)
        return OLED_ERR_I2C;
    return OLED_OK;
}

/* ============================================================================
 *  oled_init — SSD1306 init sequence
 *
 *  Verified against SSD1306 datasheet Rev 0.0 section 8.5 and
 *  Adafruit SSD1306 reference implementation.
 *
 *  Critical differences from SSD1309:
 *   + Charge pump enable (0x8D, 0x14) — REQUIRED for SSD1306
 *   - No RST pulse — SSD1306 has internal power-on reset
 *   - No external VCC needed — panel driven by internal charge pump
 * ========================================================================== */
oled_status_t oled_init(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    if (!hi2c) return OLED_ERR_PARAM;

    s_hi2c = hi2c;
    s_addr = addr;

    memset(s_buf, 0, sizeof(s_buf));

    /* Power-up settling time */
    HAL_Delay(10);

    /* First command doubles as device-present check.
     * If nothing ACKs here, oled_init() returns OLED_ERR_I2C
     * and app_init() triggers the 7-blink fault code. */
    if (oled_cmd(0xAE) != OLED_OK)   // Display OFF
        return OLED_ERR_I2C;

    /* SSD1306 full init sequence */
    oled_cmd2(0xD5, 0x80);  // Clock divide ratio / oscillator frequency
    oled_cmd2(0xA8, 0x3F);  // Multiplex ratio: 64MUX (0x3F = 63)
    oled_cmd2(0xD3, 0x00);  // Display offset: 0
    oled_cmd (0x40);         // Display start line: 0

    /* ── CHARGE PUMP — mandatory for SSD1306, absent on SSD1309 ── */
    oled_cmd2(0x8D, 0x14);  // Charge pump: enable
    /* ──────────────────────────────────────────────────────────── */

    oled_cmd2(0x20, 0x00);  // Memory addressing mode: horizontal
                             // (0x00=horizontal, 0x01=vertical, 0x02=page)
                             // Horizontal mode used here for simpler flush loop

    oled_cmd (0xA1);         // Segment remap: col 127 -> SEG0
    oled_cmd (0xC8);         // COM scan direction: remapped (top to bottom)
    oled_cmd2(0xDA, 0x12);  // COM pins hardware config: alternative
    oled_cmd2(0x81, 0xCF);  // Contrast: 0xCF
    oled_cmd2(0xD9, 0xF1);  // Pre-charge period
    oled_cmd2(0xDB, 0x40);  // VCOMH deselect level
    oled_cmd (0xA4);         // Entire display ON: follow RAM
    oled_cmd (0xA6);         // Normal display (not inverted)
    oled_cmd (0xAF);         // Display ON

    /* Clear GDDRAM so power-on noise does not show as random pixels */
    oled_clear();
    oled_flush();

    return OLED_OK;
}

/* ============================================================================
 *  Frame buffer operations
 * ========================================================================== */

void oled_clear(void)
{
    memset(s_buf, 0, sizeof(s_buf));
}

void oled_set_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;

    uint16_t idx = (uint16_t)x + (uint16_t)(y / 8U) * OLED_WIDTH;

    if (color)
        s_buf[idx] |=  (uint8_t)(1U << (y & 7U));
    else
        s_buf[idx] &= ~(uint8_t)(1U << (y & 7U));
}

/* ============================================================================
 *  oled_flush — push frame buffer to SSD1306 GDDRAM
 *
 *  Uses horizontal addressing mode (set in init as 0x20, 0x00).
 *  In horizontal mode, after setting column and page range the controller
 *  auto-increments column then page — all 1024 bytes in one I2C burst.
 *
 *  Single-transaction flush is faster than page-by-page but needs a
 *  1025-byte stack buffer. At 400 kHz this takes ~22 ms per frame,
 *  well within the 500 ms refresh interval.
 * ========================================================================== */
oled_status_t oled_flush(void)
{
    /* Set column address: 0 to 127 */
    if (oled_cmd3(0x21, 0, 127)   != OLED_OK) return OLED_ERR_I2C;
    /* Set page address: 0 to 7 */
    if (oled_cmd3(0x22, 0, 7)     != OLED_OK) return OLED_ERR_I2C;

    /* Send all 1024 bytes in one transaction:
     * [0x40 control byte] + [1024 frame buffer bytes] */
    uint8_t tx[OLED_BUF_SIZE + 1U];
    tx[0] = 0x40;   /* Co=0, D/C#=1 → data stream */
    memcpy(&tx[1], s_buf, OLED_BUF_SIZE);

    if (HAL_I2C_Master_Transmit(s_hi2c, (uint16_t)(s_addr << 1),
                                tx, sizeof(tx),
                                OLED_I2C_TIMEOUT_MS) != HAL_OK)
        return OLED_ERR_I2C;

    return OLED_OK;
}

/* ============================================================================
 *  Drawing helpers
 * ========================================================================== */

void oled_hline(uint8_t x0, uint8_t x1, uint8_t y, uint8_t color)
{
    if (x0 > x1) { uint8_t t = x0; x0 = x1; x1 = t; }
    for (uint8_t x = x0; x <= x1; x++) oled_set_pixel(x, y, color);
}

void oled_vline(uint8_t x, uint8_t y0, uint8_t y1, uint8_t color)
{
    if (y0 > y1) { uint8_t t = y0; y0 = y1; y1 = t; }
    for (uint8_t y = y0; y <= y1; y++) oled_set_pixel(x, y, color);
}

void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    for (uint8_t row = y; row < y + h; row++)
        oled_hline(x, (uint8_t)(x + w - 1U), row, color);
}

/* ============================================================================
 *  Text rendering
 * ========================================================================== */

void oled_print_str(uint8_t col, uint8_t row, const char *str)
{
    if (!str || row >= OLED_ROWS) return;

    uint8_t c = col;
    while (*str && c < OLED_COLS)
    {
        uint8_t ch = (uint8_t)*str++;
        if (ch < 0x20 || ch > 0x7E) ch = 0x20;

        const uint8_t *glyph = FONT5X7[ch - 0x20];

        uint8_t px = c * OLED_FONT_W;
        uint8_t py = row * OLED_FONT_H;

        for (uint8_t ci = 0; ci < 5U; ci++)
        {
            uint8_t col_data = glyph[ci];
            for (uint8_t bit = 0; bit < 8U; bit++)
                oled_set_pixel(px + ci, py + bit, (col_data >> bit) & 0x01U);
        }
        /* 6th column gap — always off */
        for (uint8_t bit = 0; bit < 8U; bit++)
            oled_set_pixel(px + 5U, py + bit, 0U);

        c++;
    }
}

void oled_printf_row(uint8_t row, const char *fmt, ...)
{
    if (row >= OLED_ROWS || !fmt) return;

    char buf[OLED_COLS + 1U];
    memset(buf, ' ', OLED_COLS);
    buf[OLED_COLS] = '\0';

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n < 0 || n >= (int)sizeof(buf))
        buf[OLED_COLS] = '\0';

    size_t len = strlen(buf);
    while (len < OLED_COLS)
        buf[len++] = ' ';
    buf[OLED_COLS] = '\0';

    oled_print_str(0, row, buf);
}