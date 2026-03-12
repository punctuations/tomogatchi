#include "sd_card.h"
#include "st7735.h"
#include "pico/stdlib.h"
#include "pico/mutex.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "ff.h"
#include "diskio.h"
#include <string.h>
#include <stdio.h>

mutex_t sd_mutex;

// ── SD card SPI protocol ──────────────────────────────────────────────────────
// All SD communication is done at a low baud rate during init,
// then bumped up to full speed.

#define SD_INIT_BAUD   400000
#define SD_FULL_BAUD   20000000

#define SD_CMD0   0
#define SD_CMD8   8
#define SD_CMD17  17
#define SD_CMD24  24
#define SD_CMD55  55
#define SD_CMD58  58
#define SD_ACMD41 41

static uint32_t _sector_count = 0;
static bool     _hc = false;   // true = SDHC/SDXC (block addressed)

static inline void _sd_cs_lo(void) { gpio_put(SD_PIN_CS, 0); }
static inline void _sd_cs_hi(void) { gpio_put(SD_PIN_CS, 1); }

static uint8_t _spi_byte(uint8_t b) {
    uint8_t r;
    spi_write_read_blocking(TFT_SPI, &b, &r, 1);
    return r;
}

static void _spi_skip(int n) {
    while (n--) _spi_byte(0xFF);
}

static uint8_t _cmd(uint8_t cmd, uint32_t arg) {
    _spi_byte(0xFF);
    _spi_byte(0x40 | cmd);
    _spi_byte(arg >> 24);
    _spi_byte(arg >> 16);
    _spi_byte(arg >>  8);
    _spi_byte(arg);
    // CRC — only needed for CMD0 and CMD8
    uint8_t crc = 0x01;
    if (cmd == 0) crc = 0x95;
    if (cmd == 8) crc = 0x87;
    _spi_byte(crc);
    // Wait for response (up to 8 bytes)
    uint8_t r = 0xFF;
    for (int i = 0; i < 8; i++) {
        r = _spi_byte(0xFF);
        if (!(r & 0x80)) break;
    }
    return r;
}

bool sd_init(void) {
    // SD CS pin — already initialised by tft_init(), just ensure it's high
    gpio_put(SD_PIN_CS, 1);

    // Drop SPI to init speed
    spi_set_baudrate(TFT_SPI, SD_INIT_BAUD);

    // Power-up: send ≥74 clock pulses with CS high
    _sd_cs_hi();
    _spi_skip(10);

    // CMD0 — reset to SPI mode
    _sd_cs_lo();
    int retries = 0;
    while (_cmd(SD_CMD0, 0) != 0x01) {
        if (++retries > 100) { _sd_cs_hi(); return false; }
    }

    // CMD8 — check voltage range (required for SDHC)
    uint8_t r = _cmd(SD_CMD8, 0x000001AA);
    bool v2 = (r == 0x01);
    if (v2) _spi_skip(4);   // discard 32-bit response

    // ACMD41 — wait for card to leave idle state
    retries = 0;
    do {
        _cmd(SD_CMD55, 0);
        r = _cmd(SD_ACMD41, v2 ? 0x40000000 : 0);
        if (++retries > 2000) { _sd_cs_hi(); return false; }
        sleep_us(100);
    } while (r != 0x00);

    // CMD58 — read OCR to check SDHC bit
    if (_cmd(SD_CMD58, 0) == 0x00) {
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = _spi_byte(0xFF);
        _hc = (ocr[0] & 0x40) != 0;
    }

    _sd_cs_hi();
    _spi_skip(1);

    // Ramp up SPI speed
    spi_set_baudrate(TFT_SPI, SD_FULL_BAUD);

    // CMD9 — read CSD register to get actual sector count
    _sd_cs_lo();
    if (_cmd(9, 0) == 0x00) {
        // Wait for data token
        uint8_t tok = 0xFF;
        for (int t = 0; t < 2000; t++) {
            tok = _spi_byte(0xFF);
            if (tok == 0xFE) break;
        }
        if (tok == 0xFE) {
            uint8_t csd[16];
            for (int i = 0; i < 16; i++) csd[i] = _spi_byte(0xFF);
            _spi_skip(2);   // CRC

            uint8_t csd_ver = (csd[0] >> 6) & 0x03;
            if (csd_ver == 1) {
                // CSD v2 (SDHC/SDXC) — sector count direct
                uint32_t c_size = ((csd[7] & 0x3F) << 16) |
                                   (csd[8] << 8) | csd[9];
                _sector_count = (c_size + 1) * 1024;
            } else {
                // CSD v1 (SDSC)
                uint32_t read_bl_len = csd[5] & 0x0F;
                uint32_t c_size      = ((csd[6] & 0x03) << 10) |
                                       (csd[7] << 2) |
                                       ((csd[8] >> 6) & 0x03);
                uint32_t c_mult      = ((csd[9] & 0x03) << 1) |
                                       ((csd[10] >> 7) & 0x01);
                uint32_t block_len   = 1u << read_bl_len;
                uint32_t block_count = (c_size + 1) * (1u << (c_mult + 2));
                _sector_count = block_count * (block_len / 512);
            }
        }
    }
    _sd_cs_hi();
    _spi_skip(1);

    printf("SD init OK  SDHC=%d  sectors=%lu\n", _hc, (unsigned long)_sector_count);
    mutex_init(&sd_mutex);
    return true;
}

bool sd_read_blocks(uint32_t lba, uint8_t *buf, uint32_t count) {
    mutex_enter_blocking(&sd_mutex);
    if (!_hc) lba <<= 9;
    bool ok = true;
    for (uint32_t i = 0; i < count; i++) {
        _sd_cs_lo();
        if (_cmd(SD_CMD17, lba + (i << (_hc ? 0 : 9))) != 0x00) {
            ok = false; break;
        }
        uint8_t tok = 0xFF;
        for (int t = 0; t < 2000; t++) {
            tok = _spi_byte(0xFF);
            if (tok == 0xFE) break;
        }
        if (tok != 0xFE) { ok = false; break; }
        for (int j = 0; j < 512; j++) buf[i*512+j] = _spi_byte(0xFF);
        _spi_skip(2);
        _sd_cs_hi(); _spi_skip(1);
        if (_hc) lba++;
    }
    _sd_cs_hi();
    mutex_exit(&sd_mutex);
    return ok;
}

bool sd_write_blocks(uint32_t lba, const uint8_t *buf, uint32_t count) {
    mutex_enter_blocking(&sd_mutex);
    if (!_hc) lba <<= 9;
    bool ok = true;
    for (uint32_t i = 0; i < count; i++) {
        _sd_cs_lo();
        if (_cmd(SD_CMD24, lba + (i << (_hc ? 0 : 9))) != 0x00) {
            ok = false; break;
        }
        _spi_byte(0xFE);
        const uint8_t *src = buf + i * 512;
        spi_write_blocking(TFT_SPI, src, 512);
        _spi_skip(2);
        uint8_t resp = _spi_byte(0xFF) & 0x1F;
        if (resp != 0x05) { ok = false; break; }
        for (int t = 0; t < 100000; t++) {
            if (_spi_byte(0xFF) == 0xFF) break;
        }
        _sd_cs_hi(); _spi_skip(1);
        if (_hc) lba++;
    }
    _sd_cs_hi();
    mutex_exit(&sd_mutex);
    return ok;
}

uint32_t sd_sector_count(void) {
    return _sector_count;
}

// ── FatFs diskio interface ────────────────────────────────────────────────────
// FatFs calls these functions; we bridge them to our SD driver.

static FATFS _fs;
static bool  _mounted = false;

DSTATUS disk_initialize(BYTE drv) {
    if (drv != 0) return STA_NOINIT;
    return sd_init() ? 0 : STA_NOINIT;
}

DSTATUS disk_status(BYTE drv) {
    return (drv == 0) ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE drv, BYTE *buf, LBA_t sector, UINT count) {
    if (drv != 0) return RES_PARERR;
    return sd_read_blocks(sector, buf, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE drv, const BYTE *buf, LBA_t sector, UINT count) {
    if (drv != 0) return RES_PARERR;
    return sd_write_blocks(sector, buf, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE drv, BYTE cmd, void *buf) {
    if (drv != 0) return RES_PARERR;
    switch (cmd) {
        case CTRL_SYNC:    return RES_OK;
        case GET_SECTOR_SIZE: *(WORD*)buf = 512; return RES_OK;
        case GET_SECTOR_COUNT: {
            *(DWORD*)buf = _sector_count;
            return _sector_count > 0 ? RES_OK : RES_ERROR;
        }
        case GET_BLOCK_SIZE: *(DWORD*)buf = 1; return RES_OK;
    }
    return RES_PARERR;
}

DWORD get_fattime(void) { return 0; }

// Mount helper called from main
bool sd_mount(void) {
    if (_mounted) return true;
    FRESULT r = f_mount(&_fs, "", 1);
    _mounted = (r == FR_OK);
    if (_mounted) {
        // Read actual sector count from filesystem info
        FATFS *fsp;
        DWORD free_clust;
        if (f_getfree("", &free_clust, &fsp) == FR_OK) {
            _sector_count = (fsp->n_fatent - 2 + free_clust) *
                             fsp->csize;
        }
    }
    return _mounted;
}