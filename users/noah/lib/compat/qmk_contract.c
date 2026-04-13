// ────────────────────────────────────────────────────────────────────────────
// QMK Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_playback_contract.h"

#ifdef VIA_ENABLE
#    include "dynamic_keymap.h"
#    include "send_string.h"

#    include "../macro/macro_payload.h"

#    ifndef DYNAMIC_KEYMAP_MACRO_DELAY
#        define DYNAMIC_KEYMAP_MACRO_DELAY TAP_CODE_DELAY
#    endif

static uint8_t qmk_contract_via_macro_read_byte(uint16_t offset) {
    uint8_t byte = 0;
    dynamic_keymap_macro_get_buffer(offset, 1, &byte);
    return byte;
}

typedef struct {
    uint16_t base_offset;
} qmk_contract_via_macro_reader_t;

static bool qmk_contract_via_macro_reader(uint16_t offset, uint8_t *byte, void *context) {
    qmk_contract_via_macro_reader_t *reader = (qmk_contract_via_macro_reader_t *)context;

    if (!byte) {
        return false;
    }

    *byte = qmk_contract_via_macro_read_byte((uint16_t)(reader->base_offset + offset));
    return true;
}

static bool qmk_contract_play_via_macro(uint8_t id) {
    uint16_t size   = dynamic_keymap_macro_get_buffer_size();
    uint16_t offset = 0;
    macro_payload_ir_t ir = {0};
    qmk_contract_via_macro_reader_t reader = {0};

    if (id >= dynamic_keymap_macro_get_count() || size == 0) {
        return false;
    }

    if (qmk_contract_via_macro_read_byte(size - 1) != 0) {
        return false;
    }

    while (id > 0) {
        if (offset == size) {
            return false;
        }
        if (qmk_contract_via_macro_read_byte(offset) == 0) {
            --id;
        }
        ++offset;
    }

    reader.base_offset = offset;

    if (!macro_payload_decode_qmk_stream(&ir, (uint16_t)(size - offset), qmk_contract_via_macro_reader, &reader)) {
        return false;
    }

    return macro_payload_play_ir_with_text_output(&ir, MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED, DYNAMIC_KEYMAP_MACRO_DELAY);
}
#endif

bool noah_qmk_contract_try_play_via_macro(uint16_t action) {
#ifdef VIA_ENABLE
    if (IS_QK_MACRO(action)) {
        (void)qmk_contract_play_via_macro((uint8_t)(action - QK_MACRO));
        return true;
    }
#else
    (void)action;
#endif

    return false;
}
