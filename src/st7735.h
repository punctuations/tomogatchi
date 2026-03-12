#pragma once
#include <stdint.h>
#include "hardware/spi.h"

// ── Wiring ────────────────────────────────────────────────────────────────────
// All on SPI0 — shared by display and SD card (different CS pins)
//
//   Signal    GPIO   Physical pin
//   ───────   ────   ────────────
//   SCK       18     24
//   MOSI      19     25
//   MISO      16     21    (SD only)
//   TFT CS    17     22
//   SD  CS    15     20
//   DC        20     26
//   RST       21     27
//   BL        22     29

#define TFT_SPI       spi0
#define TFT_PIN_SCK   18
#define TFT_PIN_MOSI  19
#define TFT_PIN_MISO  16
#define TFT_PIN_CS    17
#define TFT_PIN_DC    20
#define TFT_PIN_RST   21
#define TFT_PIN_BL    22
#define SD_PIN_CS     15

// ── Display geometry ─────────────────────────────────────────────────────────
// Physical orientation: portrait, 80 wide x 160 tall
#define TFT_W         80
#define TFT_H         160
#define TFT_XOFF      26    // ST7735 GRAM offset for this panel
#define TFT_YOFF      1

// ── Colour helpers ────────────────────────────────────────────────────────────
#define RGB565(r,g,b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))
#define SWAP16(x)     ((uint16_t)(((x) << 8) | ((x) >> 8)))   // host → big-endian

// Common colours (big-endian, ready to send)
#define COL_BLACK   SWAP16(RGB565(  0,  0,  0))
#define COL_RED     SWAP16(RGB565(255,  0,  0))
#define COL_GREEN   SWAP16(RGB565(  0,255,  0))
#define COL_BLUE    SWAP16(RGB565(  0,  0,255))
#define COL_WHITE   SWAP16(RGB565(255,255,255))

// ── API ───────────────────────────────────────────────────────────────────────
void     tft_init(void);
void     tft_fill(uint16_t colour_be);
void     tft_fill_rect(int x, int y, int w, int h, uint16_t colour_be);
void     tft_blit(const uint8_t *buf, int x, int y, int w, int h);

// Scale src (sw×sh, RGB565 big-endian) up to fit TFT_W×TFT_H,
// centred with black letterbox.  Only redraws letterbox on first call
// or when clear_border is true.
void     tft_blit_scaled(const uint8_t *buf, int sw, int sh, bool clear_border);
