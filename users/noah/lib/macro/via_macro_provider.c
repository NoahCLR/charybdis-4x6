#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "via_macro_provider.h"

#ifdef VIA_ENABLE

#    include "noah_keymap_ids.h"
#    include "send_string.h"

#    include "../compat/qmk_via_playback_contract.h"
#    include "macro_slot_provider.h"

#    ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#        define DYNAMIC_KEYMAP_MACRO_DELAY TAP_CODE_DELAY
#    endif

typedef struct {
    uint16_t base_offset;
} via_macro_provider_reader_t;

static macro_slot_metadata_t via_macro_slots[VIA_MACRO_SLOT_COUNT];

// Where the macro being loaded starts, found by its first window; later
// windows of the same playback reuse it instead of walking the bank again.
static struct {
    bool     valid;
    uint8_t  slot;
    uint16_t offset;
    uint16_t remaining;
} via_macro_found;

static const macro_slot_provider_t via_macro_provider = {
    .slot_count = VIA_MACRO_SLOT_COUNT,
};

static uint8_t via_macro_provider_read_byte(uint16_t offset) {
    uint8_t byte = 0;

    noah_qmk_via_macro_get_buffer(offset, 1, &byte);
    return byte;
}

static bool via_macro_provider_reader_read_byte(uint16_t offset, uint8_t *byte, void *context) {
    via_macro_provider_reader_t *reader = (via_macro_provider_reader_t *)context;

    if (!byte || !reader) {
        return false;
    }

    *byte = via_macro_provider_read_byte((uint16_t)(reader->base_offset + offset));
    return true;
}

static bool via_macro_provider_find_slot(uint8_t slot, uint16_t *offset, uint16_t *remaining) {
    uint16_t size = noah_qmk_via_macro_buffer_size();
    uint16_t base = 0;

    if (!offset || !remaining || slot >= noah_qmk_via_macro_count() || size == 0) {
        return false;
    }

    if (via_macro_provider_read_byte(size - 1u) != 0) {
        return false;
    }

    while (slot > 0) {
        if (base >= size) {
            return false;
        }
        if (via_macro_provider_read_byte(base) == 0) {
            slot--;
        }
        base++;
    }

    if (base > size) {
        return false;
    }

    *offset    = base;
    *remaining = (uint16_t)(size - base);
    return true;
}

static bool via_macro_provider_load_ir(uint8_t slot, macro_payload_ir_t *ir, macro_payload_stream_cursor_t *cursor, void *context) {
    via_macro_provider_reader_t reader = {0};

    (void)context;

    if (!ir || !cursor) {
        return false;
    }
    if (cursor->offset == 0u || !via_macro_found.valid || via_macro_found.slot != slot) {
        via_macro_found.valid = via_macro_provider_find_slot(slot, &via_macro_found.offset, &via_macro_found.remaining);
        via_macro_found.slot  = slot;
        if (!via_macro_found.valid) {
            return false;
        }
    }

    reader.base_offset = via_macro_found.offset;
    return macro_payload_decode_qmk_window(ir, cursor, via_macro_found.remaining, via_macro_provider_reader_read_byte, &reader);
}

bool via_macro_provider_try_play(uint16_t action) {
    macro_slot_provider_t provider = via_macro_provider;

    if (!IS_QK_MACRO(action)) {
        return false;
    }

    provider.load_ir = via_macro_provider_load_ir;
    (void)macro_slot_provider_start(&provider, via_macro_slots, (uint8_t)(action - QK_MACRO), MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, DYNAMIC_KEYMAP_MACRO_DELAY, MACRO_PAYLOAD_SOURCE_VIA);
    return true;
}

void via_macro_provider_storage_changing(void) {
    via_macro_found.valid = false;
    macro_slot_provider_storage_changing();
    via_macro_provider_invalidate_all();
}

void via_macro_provider_invalidate_all(void) {
    macro_slot_provider_t provider = via_macro_provider;

    via_macro_found.valid = false;

    provider.load_ir = via_macro_provider_load_ir;
    macro_slot_provider_invalidate_all(&provider, via_macro_slots);
}

#endif
