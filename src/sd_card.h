#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ff.h"
#include "pico/mutex.h"

// Shared mutex — anyone touching the SD card must hold this.
// Prevents FatFs and USB MSC from corrupting each other's transfers.
extern mutex_t sd_mutex;

bool     sd_init(void);
uint32_t sd_sector_count(void);
bool     sd_read_blocks (uint32_t lba, uint8_t *buf, uint32_t count);
bool     sd_write_blocks(uint32_t lba, const uint8_t *buf, uint32_t count);