#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "profile_macro_provider.h"

#include "noah_keymap_ids.h"
#include "send_string.h"

#include "../profile/runtime/effective_keymap_macros_runtime.h"
#include "macro_slot_provider.h"

#ifndef NOAH_MACRO_SLOT_DELAY
#    define NOAH_MACRO_SLOT_DELAY TAP_CODE_DELAY
#endif

static macro_slot_metadata_t profile_macro_slots[VIA_MACRO_SLOT_COUNT];

// Where the macro being loaded starts, found by its first window; later
// windows of the same playback reuse it instead of walking the bank again.
static struct {
    bool     valid;
    uint8_t  slot;
    uint16_t offset;
    uint16_t remaining;
} profile_macro_found;

static bool profile_macro_provider_load_ir(uint8_t slot, macro_payload_ir_t *ir, macro_payload_stream_cursor_t *cursor, void *context);

static const macro_slot_provider_t profile_macro_provider = {
    .slot_count = VIA_MACRO_SLOT_COUNT,
    .load_ir = profile_macro_provider_load_ir,
};

// Keep the initial bank search's workspace off the window decoder's stack. It
// counts terminators a chunk at a time rather than one read per byte.
static __attribute__((noinline)) bool profile_macro_provider_find_slot(uint8_t slot, uint16_t *offset, uint16_t *remaining) {
    uint16_t size = noah_effective_macro_bank_length();
    uint16_t base = 0;
    uint8_t  chunk[32];

    if (!offset || !remaining || slot >= VIA_MACRO_SLOT_COUNT || size == 0) {
        return false;
    }

    if (!noah_effective_macro_bank_read(size - 1u, chunk, 1u) || chunk[0] != 0) {
        return false;
    }

    while (slot > 0) {
        if (base >= size) {
            return false;
        }
        uint16_t length = (size_t)(size - base) < sizeof(chunk) ? (uint16_t)(size - base) : (uint16_t)sizeof(chunk);
        if (!noah_effective_macro_bank_read(base, chunk, length)) return false;
        for (uint16_t index = 0; index < length && slot > 0; index++) {
            if (chunk[index] == 0) {
                slot--;
            }
            base++;
        }
    }

    *offset    = base;
    *remaining = (uint16_t)(size - base);
    return true;
}

static bool profile_macro_provider_load_ir(uint8_t slot, macro_payload_ir_t *ir, macro_payload_stream_cursor_t *cursor, void *context) {
    (void)context;

    if (!ir || !cursor) {
        return false;
    }
    if (!noah_effective_macro_bank_is_live()) {
        const char *payload = via_macro_payloads[slot];
        if (!payload || !*payload) { *ir = (macro_payload_ir_t){0}; return true; }
        return macro_payload_compile(payload, ir);
    }
    if (cursor->offset == 0u || !profile_macro_found.valid || profile_macro_found.slot != slot) {
        profile_macro_found.valid = profile_macro_provider_find_slot(slot, &profile_macro_found.offset, &profile_macro_found.remaining);
        profile_macro_found.slot  = slot;
        if (!profile_macro_found.valid) {
            return false;
        }
    }

    // The shared engine has one active window, and the cached slot offset
    // stays fixed throughout this synchronous decode.
    return macro_payload_decode_qmk_window(ir, cursor, profile_macro_found.remaining, noah_effective_macro_read_byte, &profile_macro_found.offset);
}

bool noah_profile_macro_start(uint16_t action) {
    if (!IS_QK_MACRO(action)) {
        return false;
    }

    static uint32_t publication;
    uint32_t current = noah_effective_keymap_macros_publication();
    if (current != publication) {
        profile_macro_found.valid = false;
        macro_slot_provider_invalidate_all(&profile_macro_provider, profile_macro_slots);
        publication = current;
    }
    return macro_slot_provider_start(&profile_macro_provider, profile_macro_slots, (uint8_t)(action - QK_MACRO), MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, NOAH_MACRO_SLOT_DELAY, MACRO_PAYLOAD_SOURCE_SLOT) == MACRO_PAYLOAD_START_STARTED;
}

bool noah_profile_macro_try_play(uint16_t action) {
    if (!IS_QK_MACRO(action)) return false;
    (void)noah_profile_macro_start(action);
    return true;
}
