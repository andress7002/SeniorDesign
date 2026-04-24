/**
  ******************************************************************************
  * @file    oled.c
  * @brief   SSD1309 128×64 I2C OLED driver — DWEII 2.42" module
  *
  *  Implementation notes
  *  ─────────────────────
  *  - oled_init()  performs a hardware RST pulse (mandatory for SSD1309),
  *    then sends the verified init sequence from the SSD1309 datasheet.
  *    The SSD1306 charge-pump command is intentionally absent.
  *
  *  - oled_flush() uses page-addressing mode (set in init) writing one
  *    page (128 bytes) per I2C transaction — 8 transactions total per frame.
  *    This avoids needing a 1025-byte stack buffer.
  *
  *  - The 5×7 font covers all printable ASCII 0x20–0x7E.
  *
  *  - All drawing functions write to the RAM frame buffer only.
  *    Call oled_flush() to push to the display.
  ******************************************************************************
  */

#include "oled.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define OLED_I2C_TIMEOUT_MS   500U
/* ============================================================================
 *  5×7 ASCII font, chars 0x20 (' ') through 0x7E ('~')
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
static I2C_HandleTypeDef *s_hi2c  = NULL;
static uint8_t            s_addr = 0x3C;

/* 128 × 8 pages = 1024 bytes — one bit per pixel */
static uint8_t s_buf[OLED_BUF_SIZE];

/* ============================================================================
 *  Low-level I2C helpers
 * ========================================================================== */

oled_status_t oled_cmd(uint8_t cmd)
{
    /*
     *  SSD1309 I2C control byte:
     *    Co=0  (stream — more bytes follow)
     *    D/C#=0 (command)
     *  → control byte = 0x00
     */
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
 *  RST pulse helper
 *  SSD1309 datasheet power-on sequence:
 *    VDD stable → wait ≥1 ms → RST LOW → wait ≥10 µs → RST HIGH → wait ≥100 µs
 * ========================================================================== */
static void _rst_pulse(void)
{
    /* Enable clock for RST port (safe to call even if already enabled) */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin   = OLED_RST_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(OLED_RST_PORT, &g);

    HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_SET);   // RST HIGH
    HAL_Delay(1);                                                     // VDD settle

    HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_RESET); // RST LOW
    HAL_Delay(1);                                                     // ≥10 µs (HAL min = 1 ms)

    HAL_GPIO_WritePin(OLED_RST_PORT, OLED_RST_PIN, GPIO_PIN_SET);   // RST HIGH
    HAL_Delay(1);                                                     // ≥100 µs settle
}

/* ============================================================================
 *  oled_init — SSD1309-specific init sequence
 *
 *  Sources:
 *   • SSD1309 datasheet Rev 1.0, section 10 (Application Example)
 *   • Verified working sequence from real-world DWEII 2.42" module reports
 *
 *  ⚠️  IMPORTANT DIFFERENCES from SSD1306:
 *   - RST pulse REQUIRED before sending commands
 *   - NO charge-pump command (0x8D) — SSD1309 uses external boost converter
 *   - COM pin config arg is 0x12 for 128×64 (same as SSD1306, but ensure
 *     you don't copy an 0x02 value from a 128×32 SSD1306 config)
 * ========================================================================== */
oled_status_t oled_init(I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    if (!hi2c) return OLED_ERR_PARAM;

    s_hi2c  = hi2c;
    s_addr = addr;

    memset(s_buf, 0, sizeof(s_buf));

    /* Step 1: Hardware reset pulse — mandatory for SSD1309 */
    _rst_pulse();

    /* Step 2: First command — if this fails the OLED is not present */
    if (oled_cmd(0xAE) != OLED_OK)   // Display OFF — first ACK check
        return OLED_ERR_I2C;


    /* Step 3: Remaining init sequence (OLED confirmed present) */
    oled_cmd2(0xD5, 0x80);     // Set display clock: divide ratio=1, osc freq=8
    oled_cmd2(0xA8, 0x3F);     // Set multiplex ratio: 64MUX (0x3F = 63)
    oled_cmd2(0xD3, 0x00);     // Set display offset: 0
    oled_cmd(0x40);             // Set display start line: 0 (0x40 | 0)

    /* Segment and COM remapping — sets correct orientation for DWEII module */
    oled_cmd(0xA1);             // Segment remap: col 127 → SEG0 (mirrors horizontally)
    oled_cmd(0xC8);             // COM output scan direction: remapped (mirrors vertically)

    oled_cmd2(0xDA, 0x12);     // COM pins hardware config: alternative, no remap
                                // 0x12 = alternative COM config for 128×64
                                // (use 0x02 for 128×32 displays — don't mix up)

    oled_cmd2(0x81, 0xCF);     // Set contrast: 0xCF (adjustable, range 0x00–0xFF)

    oled_cmd2(0xD9, 0xF1);     // Set pre-charge period: phase1=1, phase2=15
                                // SSD1309 note: uses external VCC so this
                                // mainly controls discharge timing

    oled_cmd2(0xDB, 0x40);     // Set VCOMH deselect level: ~0.77 × VCC

    /* ⚠️  NO 0x8D charge-pump command here — SSD1309 does not have one */

    oled_cmd2(0x20, 0x02);     // Memory addressing mode: page addressing
                                // 0x00=horizontal, 0x01=vertical, 0x02=page

    oled_cmd(0xA4);             // Entire display ON: follow RAM (not all-on)
    oled_cmd(0xA6);             // Normal display: 1=pixel ON (not inverted)

    oled_cmd(0xAF);             // Display ON

    /* Step 3: Clear GDDRAM to remove power-on noise */
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
 *  oled_flush — page-addressing mode write
 *
 *  For each of the 8 pages:
 *    1. Set page address (0xB0 | page)
 *    2. Set column start to 0 (two-nibble command: 0x00, 0x10)
 *    3. Send 128 data bytes preceded by 0x40 control byte
 *
 *  Total I2C bytes per frame: 8 × (3 cmds + 129 data) = 8 × 132 = 1056 bytes
 *  At 400 kHz: ~21 ms per full frame — acceptable for 500 ms refresh.
 * ========================================================================== */
oled_status_t oled_flush(void)
{
    /* Reusable tx buffer: [control byte 0x40] + [128 data bytes] */
    uint8_t tx[OLED_WIDTH + 1U];
    tx[0] = 0x40;   /* Co=0, D/C#=1 → data stream */

    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        /* Set page, lower column = 0, upper column = 0 */
        if (oled_cmd(0xB0 | page) != OLED_OK) return OLED_ERR_I2C;
        if (oled_cmd(0x00)        != OLED_OK) return OLED_ERR_I2C;  // lower nibble
        if (oled_cmd(0x10)        != OLED_OK) return OLED_ERR_I2C;  // upper nibble

        /* Copy this page from the frame buffer */
        memcpy(&tx[1], &s_buf[(uint16_t)page * OLED_WIDTH], OLED_WIDTH);

        /* Send 129 bytes (1 control + 128 data) */
        if (HAL_I2C_Master_Transmit(s_hi2c, (uint16_t)(s_addr << 1),
                                    tx, sizeof(tx),
                                    HAL_MAX_DELAY) != HAL_OK)
            return OLED_ERR_I2C;
    }
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
        if (ch < 0x20 || ch > 0x7E) ch = 0x20;   /* replace unprintable */

        const uint8_t *glyph = FONT5X7[ch - 0x20];

        uint8_t px = c * OLED_FONT_W;    /* pixel x of character start */
        uint8_t py = row * OLED_FONT_H;  /* pixel y of character start */

        /* Draw 5 glyph columns */
        for (uint8_t ci = 0; ci < 5U; ci++)
        {
            uint8_t col_data = glyph[ci];
            for (uint8_t bit = 0; bit < 8U; bit++)
                oled_set_pixel(px + ci, py + bit, (col_data >> bit) & 0x01U);
        }
        /* 6th column: inter-character gap (always off) */
        for (uint8_t bit = 0; bit < 8U; bit++)
            oled_set_pixel(px + 5U, py + bit, 0U);

        c++;
    }
}

void oled_printf_row(uint8_t row, const char *fmt, ...)
{
    if (row >= OLED_ROWS || !fmt) return;

    /* 22-char buffer: 21 visible + null terminator */
    char buf[OLED_COLS + 1U];

    /* Pre-fill with spaces so the entire row is cleared */
    memset(buf, ' ', OLED_COLS);
    buf[OLED_COLS] = '\0';

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* If vsnprintf truncated, ensure null-termination and re-pad */
    if (n < 0 || n >= (int)sizeof(buf))
        buf[OLED_COLS] = '\0';

    /* Pad remainder with spaces to overwrite stale characters */
    size_t len = strlen(buf);
    while (len < OLED_COLS)
        buf[len++] = ' ';
    buf[OLED_COLS] = '\0';

    oled_print_str(0, row, buf);
}