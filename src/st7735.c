#include "st7735.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

// ── Low-level bus helpers ─────────────────────────────────────────────────────

static inline void _cs_lo(void)  { gpio_put(TFT_PIN_CS, 0); }
static inline void _cs_hi(void)  { gpio_put(TFT_PIN_CS, 1); }
static inline void _dc_cmd(void) { gpio_put(TFT_PIN_DC, 0); }
static inline void _dc_dat(void) { gpio_put(TFT_PIN_DC, 1); }

static void _cmd(uint8_t cmd) {
    _dc_cmd(); _cs_lo();
    spi_write_blocking(TFT_SPI, &cmd, 1);
    _cs_hi();
}

static void _data(const uint8_t *buf, size_t len) {
    _dc_dat(); _cs_lo();
    spi_write_blocking(TFT_SPI, buf, len);
    _cs_hi();
}

static void _data1(uint8_t b) { _data(&b, 1); }

static void _window(int x0, int y0, int x1, int y1) {
    x0 += TFT_XOFF; x1 += TFT_XOFF;
    y0 += TFT_YOFF; y1 += TFT_YOFF;
    uint8_t xb[4] = { x0>>8, x0, x1>>8, x1 };
    uint8_t yb[4] = { y0>>8, y0, y1>>8, y1 };
    _cmd(0x2A); _data(xb, 4);   // CASET
    _cmd(0x2B); _data(yb, 4);   // RASET
    _cmd(0x2C);                  // RAMWR
}

// ── Initialisation ────────────────────────────────────────────────────────────

void tft_init(void) {
    // SPI0 at 40 MHz
    spi_init(TFT_SPI, 40 * 1000 * 1000);
    spi_set_format(TFT_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(TFT_PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(TFT_PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(TFT_PIN_MISO, GPIO_FUNC_SPI);

    // Control pins
    gpio_init(TFT_PIN_CS);  gpio_set_dir(TFT_PIN_CS,  GPIO_OUT); gpio_put(TFT_PIN_CS,  1);
    gpio_init(TFT_PIN_DC);  gpio_set_dir(TFT_PIN_DC,  GPIO_OUT); gpio_put(TFT_PIN_DC,  0);
    gpio_init(TFT_PIN_RST); gpio_set_dir(TFT_PIN_RST, GPIO_OUT); gpio_put(TFT_PIN_RST, 1);
    gpio_init(TFT_PIN_BL);  gpio_set_dir(TFT_PIN_BL,  GPIO_OUT); gpio_put(TFT_PIN_BL,  1);
    gpio_init(SD_PIN_CS);   gpio_set_dir(SD_PIN_CS,   GPIO_OUT); gpio_put(SD_PIN_CS,   1);

    // Hard reset
    gpio_put(TFT_PIN_RST, 0); sleep_ms(50);
    gpio_put(TFT_PIN_RST, 1); sleep_ms(120);

    // Init sequence for ST7735R
    _cmd(0x01); sleep_ms(150);              // SWRESET
    _cmd(0x11); sleep_ms(500);              // SLPOUT

    _cmd(0xB1); _data1(0x01); _data1(0x2C); _data1(0x2D);  // FRMCTR1
    _cmd(0xB2); _data1(0x01); _data1(0x2C); _data1(0x2D);  // FRMCTR2
    _cmd(0xB3);                                              // FRMCTR3
        _data1(0x01); _data1(0x2C); _data1(0x2D);
        _data1(0x01); _data1(0x2C); _data1(0x2D);
    _cmd(0xB4); _data1(0x07);               // INVCTR
    _cmd(0xC0); _data1(0xA2); _data1(0x02); _data1(0x84);  // PWCTR1
    _cmd(0xC1); _data1(0xC5);               // PWCTR2
    _cmd(0xC2); _data1(0x0A); _data1(0x00); // PWCTR3
    _cmd(0xC3); _data1(0x8A); _data1(0x2A); // PWCTR4
    _cmd(0xC4); _data1(0x8A); _data1(0xEE); // PWCTR5
    _cmd(0xC5); _data1(0x0E);               // VMCTR1
    _cmd(0x21);                             // INVON
    _cmd(0x36); _data1(0xC8);              // MADCTL: MY+MX, portrait, RGB
    _cmd(0x3A); _data1(0x05); sleep_ms(10); // COLMOD 16-bit
    _cmd(0x13); sleep_ms(10);               // NORON
    _cmd(0x29); sleep_ms(100);              // DISPON

    tft_fill(COL_BLACK);
}

// ── Drawing ───────────────────────────────────────────────────────────────────

void tft_fill_rect(int x, int y, int w, int h, uint16_t colour_be) {
    if (w <= 0 || h <= 0) return;
    _window(x, y, x+w-1, y+h-1);
    _dc_dat(); _cs_lo();
    int total = w * h;
    // Send in 256-pixel chunks
    uint8_t chunk[512];
    for (int i = 0; i < 512; i += 2) {
        chunk[i]   = colour_be >> 8;
        chunk[i+1] = colour_be & 0xFF;
    }
    while (total >= 256) {
        spi_write_blocking(TFT_SPI, chunk, 512);
        total -= 256;
    }
    if (total > 0)
        spi_write_blocking(TFT_SPI, chunk, total * 2);
    _cs_hi();
}

void tft_fill(uint16_t colour_be) {
    tft_fill_rect(0, 0, TFT_W, TFT_H, colour_be);
}

void tft_blit(const uint8_t *buf, int x, int y, int w, int h) {
    _window(x, y, x+w-1, y+h-1);
    _dc_dat(); _cs_lo();
    spi_write_blocking(TFT_SPI, buf, w * h * 2);
    _cs_hi();
}

void tft_blit_scaled(const uint8_t *buf, int sw, int sh, bool clear_border) {
    // Largest integer scale that fits in TFT_W x TFT_H
    int scale = TFT_W / sw;
    if (TFT_H / sh < scale) scale = TFT_H / sh;
    if (scale < 1) scale = 1;

    int dw = sw * scale;
    int dh = sh * scale;
    int ox = (TFT_W - dw) / 2;
    int oy = (TFT_H - dh) / 2;

    // Clear letterbox strips only (avoids full-screen flash)
    if (clear_border) {
        if (oy > 0) {
            tft_fill_rect(0, 0,       TFT_W, oy,              COL_BLACK);
            tft_fill_rect(0, oy+dh,   TFT_W, TFT_H-oy-dh,    COL_BLACK);
        }
        if (ox > 0) {
            tft_fill_rect(0,    oy, ox,           dh, COL_BLACK);
            tft_fill_rect(ox+dw,oy, TFT_W-ox-dw, dh, COL_BLACK);
        }
    }

    // Build scaled buffer on the stack (max 80×80×2 = 12800 bytes)
    // For 16×16 @ 5× = 80×80 = 12800 bytes — fine for stack
    uint8_t *scaled = malloc(dw * dh * 2);
    if (!scaled) return;

    for (int row = 0; row < sh; row++) {
        for (int col = 0; col < sw; col++) {
            int src = (row * sw + col) * 2;
            uint8_t hi = buf[src];
            uint8_t lo = buf[src+1];
            for (int dy = 0; dy < scale; dy++) {
                int dst_row = row * scale + dy;
                for (int dx = 0; dx < scale; dx++) {
                    int dst = (dst_row * dw + col * scale + dx) * 2;
                    scaled[dst]   = hi;
                    scaled[dst+1] = lo;
                }
            }
        }
    }

    tft_blit(scaled, ox, oy, dw, dh);
    free(scaled);
}
