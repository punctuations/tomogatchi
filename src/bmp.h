#pragma once
#include <stdint.h>
#include <stdbool.h>

// Load a 24-bit uncompressed BMP from the SD card (FatFs path).
// Allocates and returns an RGB565 big-endian buffer.
// Caller must free() the returned buffer. Returns NULL on failure.
uint8_t *bmp_load(const char *path, int *w, int *h);

// Load a 24-bit uncompressed BMP from a buffer in memory (e.g. flash).
// Allocates and returns an RGB565 big-endian buffer.
// Caller must free() the returned buffer. Returns NULL on failure.
uint8_t *bmp_load_mem(const uint8_t *data, uint32_t len, int *w, int *h);