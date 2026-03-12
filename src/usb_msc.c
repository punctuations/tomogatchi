#include "usb_msc.h"
#include "sd_card.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// ── USB descriptors ───────────────────────────────────────────────────────────

static const tusb_desc_device_t _desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x2E8A,
    .idProduct          = 0x000A,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)
#define EP_MSC_OUT  0x01
#define EP_MSC_IN   0x81

static const uint8_t _desc_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_MSC_DESCRIPTOR(0, 0, EP_MSC_OUT, EP_MSC_IN, 64),
};

static const char *_string_desc[] = {
    "\x09\x04",
    "Hangyodon",
    "SD Card",
    "000001",
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&_desc_device;
}
const uint8_t *tud_descriptor_configuration_cb(uint8_t idx) {
    (void)idx;
    return _desc_config;
}
static uint16_t _str_buf[32];
const uint16_t *tud_descriptor_string_cb(uint8_t idx, uint16_t langid) {
    (void)langid;
    uint8_t len;
    if (idx == 0) {
        memcpy(&_str_buf[1], _string_desc[0], 2);
        len = 1;
    } else {
        if (idx >= sizeof(_string_desc)/sizeof(_string_desc[0])) return NULL;
        const char *s = _string_desc[idx];
        len = strlen(s);
        if (len > 31) len = 31;
        for (uint8_t i = 0; i < len; i++) _str_buf[1+i] = s[i];
    }
    _str_buf[0] = (TUSB_DESC_STRING << 8) | (2*len + 2);
    return _str_buf;
}

// ── MSC callbacks ─────────────────────────────────────────────────────────────

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
    (void)lun;
    memcpy(vendor_id,   "PICO    ", 8);
    memcpy(product_id,  "SD Card         ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    (void)lun;
    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
    (void)lun;
    *block_count = sd_sector_count();
    *block_size  = 512;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition,
                            bool start, bool load_eject) {
    (void)lun; (void)power_condition; (void)start; (void)load_eject;
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba,
                           uint32_t offset, void *buf, uint32_t bufsize) {
    (void)lun; (void)offset;
    uint32_t count = bufsize / 512;
    return sd_read_blocks(lba, buf, count) ? (int32_t)bufsize : -1;
}

// Implemented in main.c — called on every MSC write
extern void notify_msc_write(void);

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba,
                            uint32_t offset, uint8_t *buf, uint32_t bufsize) {
    (void)lun; (void)offset;
    uint32_t count = bufsize / 512;
    notify_msc_write();
    return sd_write_blocks(lba, buf, count) ? (int32_t)bufsize : -1;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                         void *buf, uint16_t bufsize) {
    (void)lun; (void)scsi_cmd; (void)buf; (void)bufsize;
    return -1;
}

// ── Public API ────────────────────────────────────────────────────────────────

void usb_msc_init(void) {
    tusb_init();
    // Pump USB for 800ms so Windows receives enumeration packets
    // before we start doing slow SPI work in the main loop.
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start < 800) {
        tud_task();
    }
}

void usb_msc_task(void) {
    tud_task();
}