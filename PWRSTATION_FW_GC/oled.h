#pragma once
/**
  ******************************************************************************
  * @file    oled.h
  * @brief   SSD1309 128×64 I2C OLED driver — DWEII 2.42" module
  *
  *  KEY DIFFERENCES vs SSD1306 in this driver
  *  ──────────────────────────────────────────
  *  1. RST pin is MANDATORY on the SSD1309.
  *     The SSD1306 has an internal power-on reset; the SSD1309 does NOT.
  *     Without a hardware reset pulse the display will not initialise.
  *     Wire RST to any free GPIO (default: PB0 — change OLED_RST_* below).
  *
  *     Hardware alternative (no GPIO needed):
  *       10 kΩ from VCC → RST,  1 µF from RST → GND
  *     This RC circuit holds RST low at power-up then releases it, which
  *     satisfies the SSD1309 reset requirement without firmware involvement.
  *     If you use this circuit, set OLED_RST_PIN to 0xFF in oled.c to skip
  *     the software reset.
  *
  *  2. No internal charge pump.
  *     The SSD1309 requires an external 7-15 V supply on VCC (the VBAT/OLED
  *     rail), NOT 3.3 V.  The DWEII module has an onboard boost converter
  *     (SY7201 or similar) that generates this from 3.3–5 V on the VDD pin.
  *     Power the module VDD from the Blue Pill's 3.3 V pin.
  *     The charge-pump enable command (0x8D, 0x14) used in SSD1306 drivers
  *     is NOT valid for the SSD1309 and must NOT be sent.
  *
  *  3. Init sequence is simpler — fewer registers need explicit programming
  *     because the SSD1309 has better power-on defaults than the SSD1306.
  *
  *  4. Module ships in SPI mode by default.
  *     To use I2C on the DWEII board, two 0Ω resistors on the back must be
  *     moved from the SPI position to the I2C position (R3→R4 and bridge R5).
  *     Check the silkscreen on the back of your module.
  *
  *  WIRING (I2C mode, Blue Pill)
  *  ─────────────────────────────
  *   Module pin   Blue Pill pin   Notes
  *   ──────────   ─────────────   ─────────────────────────────────────────
  *   GND          GND
  *   VDD          3.3 V           Logic + onboard boost converter input
  *   SCL          PB6             I2C1 clock  (4.7 kΩ pull-up to 3.3 V)
  *   SDA          PB7             I2C1 data   (4.7 kΩ pull-up to 3.3 V)
  *   RST          PB0             Active-LOW reset — GPIO output
  *   D/C          GND (or NC)     In I2C mode D/C acts as SA0 address bit
  *                                GND → I2C address 0x3C
  *                                VCC → I2C address 0x3D
  *
  *  I2C address: 0x3C (SA0/D-C tied LOW) or 0x3D (SA0/D-C tied HIGH)
  ******************************************************************************
  */

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdarg.h>

/* ============================================================================
 *  Display geometry
 * ========================================================================== */
#define OLED_WIDTH      128U
#define OLED_HEIGHT      64U
#define OLED_PAGES       (OLED_HEIGHT / 8U)          // 8
#define OLED_BUF_SIZE    (OLED_WIDTH * OLED_PAGES)   // 1024 bytes

#define OLED_FONT_W       6U    // 5 pixels + 1 gap column
#define OLED_FONT_H       8U    // 8 pixels tall
#define OLED_COLS        (OLED_WIDTH  / OLED_FONT_W) // 21 chars
#define OLED_ROWS        (OLED_HEIGHT / OLED_FONT_H) //  8 rows

/* ============================================================================
 *  RST GPIO configuration
 *  Change these two defines to whatever GPIO pin you wire RST to.
 * ========================================================================== */
#define OLED_RST_PORT    GPIOB
#define OLED_RST_PIN     GPIO_PIN_0    // PB0 — change if needed

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
  * @brief  Initialise the SSD1309 over I2C.
  *
  *         Performs hardware RST pulse on OLED_RST_PORT/PIN, then sends the
  *         SSD1309-specific init command sequence.
  *
  *           Do NOT send the SSD1306 charge-pump command (0x8D, 0x14).
  *           The SSD1309 has no internal charge pump — that command is
  *           undefined/harmful on this controller.
  *
  * @param  hi2c   HAL I2C handle (initialised by MX_I2C1_Init).
  * @param  addr  7-bit I2C address (0x3C or 0x3D).
  * @retval OLED_OK on success, OLED_ERR_I2C on bus fault.
  */
oled_status_t oled_init(I2C_HandleTypeDef *hi2c, uint8_t addr);

/**
  * @brief  Zero the frame buffer (all pixels off). Does NOT flush.
  */
void oled_clear(void);

/**
  * @brief  Push the entire 1024-byte frame buffer to the SSD1309 GDDRAM.
  *         Uses page-by-page I2C writes (compatible with all addressing modes).
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
 *  Layer 1 — Drawing helpers (all write to frame buffer, call oled_flush())
 * ========================================================================== */
void oled_hline    (uint8_t x0, uint8_t x1, uint8_t y,  uint8_t color);
void oled_vline    (uint8_t x,  uint8_t y0, uint8_t y1, uint8_t color);
void oled_fill_rect(uint8_t x,  uint8_t y,  uint8_t w,  uint8_t h, uint8_t color);

/* ============================================================================
 *  Layer 2 — Text rendering
 * ========================================================================== */

/**
  * @brief  Write a null-terminated ASCII string at (col, row).
  *         Characters outside 0x20-0x7E are rendered as space.
  *         Call oled_flush() to push to display.
  * @param  col  0..20
  * @param  row  0..7
  */
void oled_print_str(uint8_t col, uint8_t row, const char *str);

/**
  * @brief  printf-style formatter targeting one complete text row.
  *         The entire row (21 chars) is always overwritten with spaces,
  *         so stale digits from a previous wider value are erased.
  *         Call oled_flush() after all rows are updated.
  *
  * @param  row  0..7
  * @param  fmt  printf format string
  *
  * Example:
  *   oled_printf_row(0, "V:%7.3fV", (double)g_vbus_V);
  *   oled_printf_row(1, "I:%7.4fA", (double)g_current_A);
  *   oled_flush();
  */
void oled_printf_row(uint8_t row, const char *fmt, ...);

/* ============================================================================
 *  Low-level I2C helpers (exposed for testing / custom command sequences)
 * ========================================================================== */

/** Send one command byte. */
oled_status_t oled_cmd(uint8_t cmd);

/** Send one command byte + one argument byte (two-byte commands). */
oled_status_t oled_cmd2(uint8_t cmd, uint8_t arg);

/** Send one command byte + two argument bytes (three-byte commands). */
oled_status_t oled_cmd3(uint8_t cmd, uint8_t arg1, uint8_t arg2);