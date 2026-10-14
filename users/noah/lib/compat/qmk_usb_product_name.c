// ──────────────────────────────────────────────────────────────────────────
// QMK USB Product Name (D-F25)
// ──────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "qmk_usb_product_name.h"

#include <string.h>

#if defined(USB_PRODUCT_STRING_USER) && defined(NOAH_LIVE_PROFILE_OWNER_ENABLE) && defined(NOAH_PORTABLE_PROFILE_ENABLE)
#    define NOAH_QMK_USB_PRODUCT_NAME_ENABLE
#    include "qmk_via_split_sync.h"
#    include "../profile/runtime/effective_settings_runtime.h"
#    include "../profile/storage/profile_store_runtime.h"
#    if defined(NOAH_HOST_QMK_STUB)
// Host tests stand in for the USB driver.
bool noah_qmk_usb_product_name_test_usb_active(void);
void noah_qmk_usb_product_name_test_restart(void);
#        define NOAH_USB_ACTIVE() noah_qmk_usb_product_name_test_usb_active()
#        define NOAH_USB_RESTART() noah_qmk_usb_product_name_test_restart()
#    else
#        include "usb_descriptor.h"
#        include "usb_main.h"
#        include "usb_util.h"
// Enumerated and not suspended.
#        define NOAH_USB_ACTIVE() usb_connected_state()
#        define NOAH_USB_RESTART() restart_usb_driver(&USB_DRIVER)
#    endif
#endif

uint8_t noah_qmk_usb_product_name_encode(const noah_profile_keyboard_name_t *name, uint8_t *descriptor, uint8_t capacity) {
    uint8_t size = 2u;

    if (!name || !descriptor || !name->length || name->length > NOAH_SETTINGS_NAME_MAX) {
        return 0u;
    }
    for (uint8_t at = 0u; at < name->length;) {
        uint8_t  lead  = name->bytes[at];
        uint8_t  extra = lead < 0x80u ? 0u : lead >= 0xf0u ? 3u : lead >= 0xe0u ? 2u : lead >= 0xc0u ? 1u : 0xffu;
        uint32_t code  = extra == 0u ? lead : extra == 1u ? lead & 0x1fu : extra == 2u ? lead & 0x0fu : lead & 0x07u;

        if (extra == 0xffu || (uint16_t)at + 1u + extra > name->length) {
            return 0u;
        }
        for (uint8_t i = 1u; i <= extra; i++) {
            if ((name->bytes[at + i] & 0xc0u) != 0x80u) {
                return 0u;
            }
            code = code << 6 | (name->bytes[at + i] & 0x3fu);
        }
        at = (uint8_t)(at + 1u + extra);
        // Past U+FFFF, a surrogate pair.
        uint16_t units[2] = {(uint16_t)code, 0u};
        uint8_t  count    = 1u;
        if (code > 0xffffu) {
            code -= 0x10000u;
            units[0] = (uint16_t)(0xd800u | (code >> 10));
            units[1] = (uint16_t)(0xdc00u | (code & 0x3ffu));
            count    = 2u;
        }
        for (uint8_t i = 0u; i < count; i++) {
            if ((uint16_t)size + 2u > capacity) {
                return 0u;
            }
            descriptor[size++] = (uint8_t)units[i];
            descriptor[size++] = (uint8_t)(units[i] >> 8);
        }
    }
    descriptor[0] = size;
    descriptor[1] = 3u; // a string descriptor
    return size;
}

#ifdef NOAH_QMK_USB_PRODUCT_NAME_ENABLE
// USB may read the served descriptor at any time, so a new name is built in
// the other buffer and served by switching to it. Aligned for the UTF-16 units
// QMK's descriptor type declares.
static uint8_t                      descriptors[2][NOAH_QMK_USB_PRODUCT_NAME_DESCRIPTOR_MAX] __attribute__((aligned(4)));
static const uint8_t               *served;
static noah_profile_keyboard_name_t enumerated; // the name last chosen for hosts to read
static noah_profile_keyboard_name_t live;
static uint32_t                     live_publication;
static uint32_t                     last_host_frame;
static bool                         live_known;
static bool                         host_frame_seen;

static void serve(const noah_profile_keyboard_name_t *name) {
    uint8_t *next = served == descriptors[0] ? descriptors[1] : descriptors[0];

    // An empty name, or one that cannot be encoded, serves QMK's built-in
    // name. Either way this name is settled, so it never restarts USB again.
    served     = noah_qmk_usb_product_name_encode(name, next, sizeof(descriptors[0])) ? next : NULL;
    enumerated = *name;
}

// Keep the boot reader's record/name workspace out of main's later key paths.
__attribute__((noinline)) void noah_qmk_usb_product_name_pre_init(void) {
    noah_profile_keyboard_name_t name;

    served     = NULL;
    live_known = host_frame_seen = false;
    memset(&enumerated, 0, sizeof(enumerated));
    // Both halves run this before either knows its role; the half without USB
    // only keeps the name in memory.
    if (noah_profile_store_runtime_boot_keyboard_name(&name)) {
        serve(&name);
    }
}

// The owner-status workspace belongs to this housekeeping call. Inlining it
// into main reserves that space beneath every later key-runtime path.
__attribute__((noinline)) void noah_qmk_usb_product_name_task(void) {
    if (!is_keyboard_master()) {
        return;
    }
    uint32_t publication = noah_effective_settings_publication();
    if (!live_known || publication != live_publication) {
        noah_effective_settings_keyboard_name(&live);
        live_publication = publication;
        live_known       = true;
    }
    if (noah_profile_keyboard_name_equal(&live, &enumerated) || !NOAH_USB_ACTIVE() || (host_frame_seen && timer_elapsed32(last_host_frame) < NOAH_QMK_USB_PRODUCT_NAME_QUIET_MS)) {
        return;
    }
    noah_qmk_via_split_sync_debug_snapshot_t via = noah_qmk_via_split_sync_debug_snapshot();
    if (!noah_qmk_via_logical_boot_output_ready() || via.local_dirty || via.replication_pending || via.receiver_active || !noah_profile_store_runtime_usb_restart_safe()) {
        return;
    }
    serve(&live);
    NOAH_USB_RESTART();
}

void noah_qmk_usb_product_name_note_host_frame(void) {
    last_host_frame = timer_read32();
    host_frame_seen = true;
}

const uint8_t *noah_qmk_usb_product_name_descriptor(void) {
    return served;
}

#    if !defined(NOAH_HOST_QMK_STUB)
const USB_Descriptor_String_t *usb_product_string_user(void) {
    return (const USB_Descriptor_String_t *)served;
}
#    endif
#else
void noah_qmk_usb_product_name_pre_init(void) {}
void noah_qmk_usb_product_name_task(void) {}
void noah_qmk_usb_product_name_note_host_frame(void) {}
const uint8_t *noah_qmk_usb_product_name_descriptor(void) {
    return NULL;
}
#endif
