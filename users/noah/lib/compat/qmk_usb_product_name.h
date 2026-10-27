// ──────────────────────────────────────────────────────────────────────────
// QMK USB Product Name (D-F25)
// ──────────────────────────────────────────────────────────────────────────
//
// The keyboard name in the live profile is the USB product string, so hosts
// list the keyboard under it. An empty name keeps QMK's built-in PRODUCT.
// Hosts read the string when they enumerate the keyboard, so:
//
// - before USB starts, the name comes from the saved profile;
// - when the live name later differs from the one USB enumerated with (an
//   Apply renamed the keyboard, or boot activated a different profile), the
//   half on USB restarts USB once it is safe: the profile owner is settled on
//   both halves with nothing held or running, and no Raw HID frame has arrived for NOAH_QMK_USB_PRODUCT_NAME_QUIET_MS, so a
//   client finishes reading first.
//
// The half without USB never restarts it: its USB stays off as QMK left it.
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../profile/schema/profile_settings_v1.h"

enum {
    NOAH_QMK_USB_PRODUCT_NAME_QUIET_MS = 1000u,
    // Two header bytes and at most one UTF-16 unit per UTF-8 byte.
    NOAH_QMK_USB_PRODUCT_NAME_DESCRIPTOR_MAX = 2u + 2u * NOAH_SETTINGS_NAME_MAX,
};

// A USB string descriptor (length, type 3, UTF-16LE) for a non-empty name.
// Returns its size, or 0 for an empty or invalid name or too small a buffer.
uint8_t noah_qmk_usb_product_name_encode(const noah_profile_keyboard_name_t *name, uint8_t *descriptor, uint8_t capacity);

// keyboard_pre_init: serve the saved profile's name before USB starts.
void noah_qmk_usb_product_name_pre_init(void);
// housekeeping: restart USB under the live name when it changed and it is safe.
void noah_qmk_usb_product_name_task(void);
// Every Raw HID frame the keyboard receives.
void noah_qmk_usb_product_name_note_host_frame(void);
// The descriptor USB serves now; NULL serves QMK's built-in PRODUCT.
const uint8_t *noah_qmk_usb_product_name_descriptor(void);
