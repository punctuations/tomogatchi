#include "bmp.h"
#include "ff.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ── Shared conversion helper ──────────────────────────────────────────────────

static uint8_t *_convert_rows(uint8_t **rows, int w, int h, bool flip) {
    if (flip) {
        for (int i = 0; i < h / 2; i++) {
            uint8_t *tmp = rows[i];
            rows[i] = rows[h-1-i];
            rows[h-1-i] = tmp;
        }
    }
    uint8_t *out = malloc(w * h * 2);
    if (!out) return NULL;
    int idx = 0;
    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            uint8_t b = rows[row][col*3];
            uint8_t g = rows[row][col*3+1];
            uint8_t r = rows[row][col*3+2];
            uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            out[idx++] = (c >> 8) & 0xFF;
            out[idx++] = c & 0xFF;
        }
        free(rows[row]);
    }
    free(rows);
    return out;
}

// ── Load from SD card (FatFs) ─────────────────────────────────────────────────

uint8_t *bmp_load(const char *path, int *out_w, int *out_h) {
    FIL f;
    if (f_open(&f, path, FA_READ) != FR_OK) {
        printf("bmp: cannot open %s\n", path);
        return NULL;
    }

    uint8_t fhdr[14]; UINT br;
    f_read(&f, fhdr, 14, &br);
    if (fhdr[0] != 'B' || fhdr[1] != 'M') {
        f_close(&f); return NULL;
    }
    uint32_t data_off = fhdr[10]|(fhdr[11]<<8)|(fhdr[12]<<16)|(fhdr[13]<<24);

    uint8_t dib[40];
    f_read(&f, dib, 40, &br);
    int32_t  w    = dib[4]|(dib[5]<<8)|(dib[6]<<16)|(dib[7]<<24);
    int32_t  h    = dib[8]|(dib[9]<<8)|(dib[10]<<16)|(dib[11]<<24);
    uint16_t bpp  = dib[14]|(dib[15]<<8);
    uint32_t comp = dib[16]|(dib[17]<<8)|(dib[18]<<16)|(dib[19]<<24);
    if (bpp != 24 || comp != 0) { f_close(&f); return NULL; }

    bool flip = (h > 0);
    if (h < 0) h = -h;
    uint32_t stride = (w * 3 + 3) & ~3u;

    f_lseek(&f, data_off);
    uint8_t **rows = malloc(h * sizeof(uint8_t *));
    for (int i = 0; i < h; i++) {
        rows[i] = malloc(stride);
        f_read(&f, rows[i], stride, &br);
    }
    f_close(&f);

    uint8_t *out = _convert_rows(rows, w, h, flip);
    *out_w = w; *out_h = h;
    printf("bmp: loaded %s (%dx%d)\n", path, w, h);
    return out;
}

// ── Load from flash/memory buffer ─────────────────────────────────────────────

uint8_t *bmp_load_mem(const uint8_t *data, uint32_t len, int *out_w, int *out_h) {
    if (len < 54 || data[0] != 'B' || data[1] != 'M') {
        printf("bmp_mem: not a valid BMP\n");
        return NULL;
    }

    uint32_t data_off = data[10]|(data[11]<<8)|(data[12]<<16)|(data[13]<<24);
    int32_t  w    = data[18]|(data[19]<<8)|(data[20]<<16)|(data[21]<<24);
    int32_t  h    = data[22]|(data[23]<<8)|(data[24]<<16)|(data[25]<<24);
    uint16_t bpp  = data[28]|(data[29]<<8);
    uint32_t comp = data[30]|(data[31]<<8)|(data[32]<<16)|(data[33]<<24);

    // Accept 24-bit uncompressed OR 32-bit RGBA (bitfields or uncompressed)
    bool is_32 = (bpp == 32 && (comp == 0 || comp == 3));
    bool is_24 = (bpp == 24 && comp == 0);
    if (!is_24 && !is_32) {
        printf("bmp_mem: unsupported format bpp=%d comp=%lu\n", bpp, (unsigned long)comp);
        return NULL;
    }

    bool flip = (h > 0);
    if (h < 0) h = -h;

    int bytes_per_pixel = is_32 ? 4 : 3;
    uint32_t stride = (w * bytes_per_pixel + 3) & ~3u;

    if (data_off + stride * (uint32_t)h > len) {
        printf("bmp_mem: data truncated\n");
        return NULL;
    }

    uint8_t *out = malloc(w * h * 2);
    if (!out) return NULL;

    int idx = 0;
    for (int row = 0; row < h; row++) {
        // flip: BMP rows are bottom-up when height is positive
        int src_row = flip ? (h - 1 - row) : row;
        const uint8_t *src = data + data_off + src_row * stride;
        for (int col = 0; col < w; col++) {
            uint8_t b, g, r;
            if (is_32) {
                b = src[col*4 + 0];
                g = src[col*4 + 1];
                r = src[col*4 + 2];
                // alpha at col*4+3 is ignored
            } else {
                b = src[col*3 + 0];
                g = src[col*3 + 1];
                r = src[col*3 + 2];
            }
            uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            out[idx++] = (c >> 8) & 0xFF;
            out[idx++] = c & 0xFF;
        }
    }

    *out_w = w;
    *out_h = h;
    printf("bmp_mem: decoded %dx%d sprite (%d bpp)\n", w, h, bpp);
    return out;
}