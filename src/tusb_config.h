#pragma once

// ── Device class ─────────────────────────────────────────────────────────────
#define CFG_TUSB_RHPORT0_MODE   OPT_MODE_DEVICE
// CFG_TUSB_OS is set by TinyUSB internally — do not redefine here
#define CFG_TUSB_DEBUG          0

// ── Memory ───────────────────────────────────────────────────────────────────
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN      __attribute__((aligned(4)))

// ── Device ───────────────────────────────────────────────────────────────────
#define CFG_TUD_ENDPOINT0_SIZE  64

// ── Class enable/disable ─────────────────────────────────────────────────────
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             1   // Mass Storage — SD card as USB drive
#define CFG_TUD_HID             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0

// ── MSC ──────────────────────────────────────────────────────────────────────
#define CFG_TUD_MSC_EP_BUFSIZE  512