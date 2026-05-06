#pragma once
/**
  ******************************************************************************
  * @file    oled.h
  * @brief   SSD1306 128x64 I2C OLED driver — DIYmall 0.96" module
  *
  *  KEY DIFFERENCES vs SSD1309 (previous driver)
  *  ─────────────────────────────────────────────
  *  1. RST pin is NOT required.
  *     The SSD1306 has an internal power-on reset circuit. No GPIO needed
  *     for reset — just power VCC and send the init sequence over I2C.
  *     PB0 is now free for other use.
  *
  *  2. Internal charge pump required.
  *     The SSD1306 needs its internal charge pump enabled to drive the OLED
  *     panel. Send commands 0x8D, 0x14 during init. Without this the display
  *     stays completely blank even though I2C ACKs correctly.
  *
  *  3. Init sequence is slightly longer than SSD1309.
  *     COM pin config: 0x12 for 128x64 panels (same value, different meaning
  *     than on SSD1309 — keep it at 0x12 for your DIYmall module).
  *
  *  4. I2C address.
  *     DIYmall 0.96" module: address is 0x3C (default, SA0 pin tied LOW).
  *     Some modules have SA0 tied HIGH giving 0x3D — confirmed 0x3C since
  *     the firmware already ACKd at that address.
  *
  *  WIRING (Blue Pill)
  *  ───────────────────
  *   Module pin   Blue Pill pin   Notes
  *   ──────────   ─────────────   ──────────────────────────────────────────
  *   GND          GND
  *   VCC          3.3V            Module regulates internally to panel voltage
  *   SCL          PB6             I2C1 SCL — 2.2 kOhm pull-up to 3.3V
  *   SDA          PB7             I2C1 SDA — 2.2 kOhm pull-up to 3.3V
  *   (no RST pin needed — SSD1306 resets itself at power-on)
  *
  *  DISPLAY LAYOUT (128x64, 5x7 font, 6px wide with gap)
  *  ───────────────────────────────────────────────────────
  *   21 characters per row, 8 rows total
  *
  *   Row 0   header / label
  *   Row 1   V:  12.345 V
  *   Row 2   I:   3.456 A
  *   Row 3   P:  42.345 W
  *   Row 4   T:   27.5 C
  *   Row 5   E: 1234.5  J
  *   Row 6   Q:  567.8  C
  *   Row 7   status / alerts
  ******************************************************************************
  */

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdarg.h>

/* ============================================================================
 *  Display geometry — unchanged from SSD1309 driver
 * ========================================================================== */
#define OLED_WIDTH      128U
#define OLED_HEIGHT      64U
#define OLED_PAGES       (OLED_HEIGHT / 8U)          // 8
#define OLED_BUF_SIZE    (OLED_WIDTH * OLED_PAGES)   // 1024 bytes

#define OLED_FONT_W       6U     // 5 pixel glyph + 1 pixel gap
#define OLED_FONT_H       8U     // 8 pixels tall
#define OLED_COLS        (OLED_WIDTH  / OLED_FONT_W) // 21 chars per row
#define OLED_ROWS        (OLED_HEIGHT / OLED_FONT_H) //  8 rows

/* ============================================================================
 *  I2C timeout
 * ========================================================================== */
#define OLED_I2C_TIMEOUT_MS   500U

/* ============================================================================
 *  Status codes
 * ========================================================================== */
typedef enum {
    OLED_OK        =  0,
    OLED_ERR_I2C   = -1,
    OLED_ERR_PARAM = -2
} oled_status_t;

/* ============================================================================
 *  Layer 1 — Hardware primitives
 * ========================================================================== */

/**
  * @brief  Initialise the SSD1306 over I2C.
  *         Sends the full SSD1306 init sequence including charge pump enable.
  *         The first command (0xAE) doubles as an ACK check — returns
  *         OLED_ERR_I2C immediately if the device does not respond.
  * @param  hi2c   HAL I2C handle (already initialised).
  * @param  addr   7-bit I2C address (0x3C for DIYmall module).
  * @retval OLED_OK on success, OLED_ERR_I2C if device absent.
  */
oled_status_t oled_init(I2C_HandleTypeDef *hi2c, uint8_t addr);

/** Zero the frame buffer. Does NOT push to display. */
void oled_clear(void);

/**
  * @brief  Push the 1024-byte frame buffer to SSD1306 GDDRAM.
  *         Uses page-addressing mode: 8 transactions of 129 bytes each.
  * @retval OLED_OK / OLED_ERR_I2C
  */
oled_status_t oled_flush(void);

/**
  * @brief  Set or clear one pixel in the frame buffer.
  * @param  x      0..127
  * @param  y      0..63
  * @param  color  1=on, 0=off
  */
void oled_set_pixel(uint8_t x, uint8_t y, uint8_t color);

/* ============================================================================
 *  Layer 1 — Drawing helpers (write to frame buffer; call oled_flush() after)
 * ========================================================================== */
void oled_hline    (uint8_t x0, uint8_t x1, uint8_t y,  uint8_t color);
void oled_vline    (uint8_t x,  uint8_t y0, uint8_t y1, uint8_t color);
void oled_fill_rect(uint8_t x,  uint8_t y,  uint8_t w,  uint8_t h, uint8_t color);

/* ============================================================================
 *  Layer 2 — Text rendering
 * ========================================================================== */

/**
  * @brief  Write a null-terminated ASCII string at text position (col, row).
  *         Characters outside 0x20-0x7E render as space.
  *         Call oled_flush() to push to display.
  * @param  col  0..20
  * @param  row  0..7
  */
void oled_print_str(uint8_t col, uint8_t row, const char *str);

/**
  * @brief  printf-style formatter that fills one complete text row.
  *         The entire 21-character row is always overwritten with spaces
  *         first so stale digits from wider previous values are erased.
  *         Call oled_flush() after updating all rows.
  *
  * @param  row  0..7
  * @param  fmt  printf format string
  *
  * Examples used in app_fast_task():
  *   oled_printf_row(1, "V:%7.3fV",  (double)g_vbus_V);
  *   oled_printf_row(2, "I:%7.4fA",  (double)g_current_A);
  *   oled_printf_row(3, "P:%7.3fW",  (double)g_power_W);
  *   oled_printf_row(4, "T:%5.1fC",  (double)g_dietemp_C);
  *   oled_flush();
  */
void oled_printf_row(uint8_t row, const char *fmt, ...);

/* ============================================================================
 *  Low-level I2C command helpers
 * ========================================================================== */
oled_status_t oled_cmd (uint8_t cmd);
oled_status_t oled_cmd2(uint8_t cmd, uint8_t arg);
oled_status_t oled_cmd3(uint8_t cmd, uint8_t arg1, uint8_t arg2);