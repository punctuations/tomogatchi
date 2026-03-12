#pragma once

// Initialise TinyUSB and register SD card as MSC drive.
// Call once from core 0 before starting the animation loop.
void usb_msc_init(void);

// Poll TinyUSB — must be called regularly from the main loop.
// Handles USB enumeration and MSC read/write requests from the PC.
void usb_msc_task(void);
