// ────────────────────────────────────────────────────────────────────────────
// VIA Macro Defaults
// ────────────────────────────────────────────────────────────────────────────

#include "via_macro_defaults.h"

#ifdef VIA_ENABLE

#    include QMK_KEYBOARD_H // IWYU pragma: keep

#    include "dynamic_keymap.h"
#    include "eeprom.h"
#    include "nvm_eeprom_eeconfig_internal.h" // IWYU pragma: keep
#    include "nvm_eeprom_via_internal.h"
#    include "via.h"
#    ifdef ENCODER_MAP_ENABLE
#        include "encoder.h"
#    endif

#    include "noah_keymap.h"
#    include "macro_payload.h"
#    include "../rgb/rgb_runtime.h"

#    ifdef CONSOLE_ENABLE
#        include "print.h"
#    endif

#    ifndef DYNAMIC_KEYMAP_EEPROM_MAX_ADDR
#        define DYNAMIC_KEYMAP_EEPROM_MAX_ADDR (TOTAL_EEPROM_BYTE_COUNT - 1)
#    endif

#    ifndef DYNAMIC_KEYMAP_EEPROM_ADDR
#        define DYNAMIC_KEYMAP_EEPROM_ADDR (VIA_EEPROM_CONFIG_END)
#    endif

#    ifndef DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR
#        define DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR (DYNAMIC_KEYMAP_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2))
#    endif

#    ifdef ENCODER_MAP_ENABLE
#        ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#            define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR + (DYNAMIC_KEYMAP_LAYER_COUNT * NUM_ENCODERS * 2 * 2))
#        endif
#    else
#        ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR
#            define DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR (DYNAMIC_KEYMAP_ENCODER_EEPROM_ADDR)
#        endif
#    endif

#    ifndef DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE
#        define DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE (DYNAMIC_KEYMAP_EEPROM_MAX_ADDR - DYNAMIC_KEYMAP_MACRO_EEPROM_ADDR + 1)
#    endif

typedef enum {
    VIA_MACRO_SLOT_UNCHECKED = 0,
    VIA_MACRO_SLOT_VALID,
    VIA_MACRO_SLOT_INVALID,
} via_macro_slot_state_t;

#    ifndef VIA_MACRO_SEED_CHUNK_SIZE
#        define VIA_MACRO_SEED_CHUNK_SIZE 64u
#    endif

typedef struct {
    uint16_t capacity;
    uint16_t offset;
    uint16_t buffered;
    uint8_t  chunk[VIA_MACRO_SEED_CHUNK_SIZE];
} via_macro_seed_writer_t;

static bool    via_macro_seed_post_init_pending = false;
static bool    via_macro_seed_scan_pending      = false;
static uint8_t via_macro_slot_state[VIA_MACRO_SLOT_COUNT];

static void log_invalid_via_macro_payload(uint8_t slot, const char *payload) {
#    ifdef CONSOLE_ENABLE
    uprintf("Invalid VIA default macro payload for VIA_MACRO_%u: %s\n", (unsigned int)slot, payload);
#    else
    (void)slot;
    (void)payload;
#    endif
}

static bool via_macro_payload_slot_is_valid(uint8_t slot) {
    const char *payload = via_macro_payloads[slot];

    if (via_macro_slot_state[slot] == VIA_MACRO_SLOT_VALID) {
        return true;
    }
    if (via_macro_slot_state[slot] == VIA_MACRO_SLOT_INVALID) {
        return false;
    }

    if (!payload || !*payload || macro_payload_validate(payload)) {
        via_macro_slot_state[slot] = VIA_MACRO_SLOT_VALID;
        return true;
    }

    via_macro_slot_state[slot] = VIA_MACRO_SLOT_INVALID;
    log_invalid_via_macro_payload(slot, payload);
    return false;
}

static void validate_via_default_macro_payloads(void) {
    for (uint8_t slot = 0; slot < VIA_MACRO_SLOT_COUNT; slot++) {
        (void)via_macro_payload_slot_is_valid(slot);
    }
}

static bool via_macro_seed_writer_flush(via_macro_seed_writer_t *writer) {
    uint16_t start_offset;

    if (!writer || writer->buffered == 0) {
        return true;
    }

    start_offset = (uint16_t)(writer->offset - writer->buffered);
    dynamic_keymap_macro_set_buffer(start_offset, writer->buffered, writer->chunk);
    writer->buffered = 0;
    return true;
}

static bool via_macro_seed_writer_write_byte(uint8_t byte, void *context) {
    via_macro_seed_writer_t *writer = (via_macro_seed_writer_t *)context;

    if (!writer || writer->offset >= writer->capacity) {
        return false;
    }

    writer->chunk[writer->buffered++] = byte;
    writer->offset++;

    if (writer->buffered == ARRAY_SIZE(writer->chunk)) {
        return via_macro_seed_writer_flush(writer);
    }

    return true;
}

static bool seed_via_default_macros(uint16_t capacity, uint16_t *written) {
    via_macro_seed_writer_t writer = {
        .capacity = capacity,
    };

    for (uint8_t slot = 0; slot < VIA_MACRO_SLOT_COUNT; slot++) {
        const char *payload = via_macro_payloads[slot];

        if (payload && *payload) {
            if (!via_macro_payload_slot_is_valid(slot)) {
                goto terminate_slot;
            }
            if (!macro_payload_encode_write(payload, via_macro_seed_writer_write_byte, &writer, NULL)) {
                return false;
            }
        }

    terminate_slot:
        if (!via_macro_seed_writer_write_byte(0x00, &writer)) {
            return false;
        }
    }

    if (!via_macro_seed_writer_flush(&writer)) {
        return false;
    }

    *written = writer.offset;
    return true;
}

static void apply_via_default_macros(void) {
    uint16_t capacity = dynamic_keymap_macro_get_buffer_size();
    uint16_t written  = 0;

    if (capacity == 0 || capacity > DYNAMIC_KEYMAP_MACRO_EEPROM_SIZE) {
        return;
    }

    // Every call site runs after QMK has already reset the macro region to
    // zero, so we only need to write the authored macro prefix here.
    if (!seed_via_default_macros(capacity, &written) || written == 0) {
        return;
    }
}

void noah_via_macro_defaults_eeconfig_init(void) {
    apply_via_default_macros();
    via_macro_seed_post_init_pending = false;
}

void noah_via_macro_defaults_matrix_scan(void) {
    if (via_macro_seed_scan_pending) {
        apply_via_default_macros();
        via_macro_seed_scan_pending = false;
    }
}

void noah_via_macro_defaults_keyboard_post_init(void) {
    validate_via_default_macro_payloads();
    if (via_macro_seed_post_init_pending) {
        apply_via_default_macros();
        via_macro_seed_post_init_pending = false;
    }
}

void via_init_kb(void) {
    via_macro_seed_post_init_pending = !via_eeprom_is_valid();
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    (void)length;

    switch (data[0]) {
#    ifdef VIA_EEPROM_ALLOW_RESET
        case id_eeprom_reset:
            via_macro_seed_scan_pending = true;
            noah_rgb_runtime_invalidate_layer_maps();
            return false;
#    endif
        case id_dynamic_keymap_set_keycode:
        case id_dynamic_keymap_set_buffer:
        case id_dynamic_keymap_reset:
            noah_rgb_runtime_invalidate_layer_maps();
            return false;
        case id_dynamic_keymap_macro_reset:
            via_macro_seed_scan_pending = true;
            return false;
        default:
            return false;
    }
}

#endif
