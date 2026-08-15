// ────────────────────────────────────────────────────────────────────────────
// VIA Macro Defaults
// ────────────────────────────────────────────────────────────────────────────

#include "via_macro_defaults.h"

#ifdef VIA_ENABLE

#    include "noah_keymap_ids.h"
#    include "macro_payload.h"
#    include "via_macro_provider.h"
#    include "../compat/qmk_via_split_sync.h"
#    include "../compat/qmk_via_storage_contract.h"
#    include "../rgb/core/rgb_runtime.h"

#    ifdef CONSOLE_ENABLE
#        include "print.h"
#    endif

#    ifndef VIA_MACRO_SEED_CHUNK_SIZE
#        define VIA_MACRO_SEED_CHUNK_SIZE 64u
#    endif

typedef struct {
    uint16_t capacity;
    uint16_t offset;
    uint16_t buffered;
    uint8_t  chunk[VIA_MACRO_SEED_CHUNK_SIZE];
} via_macro_seed_writer_t;

static bool via_macro_seed_post_init_pending = false;
static bool via_macro_seed_scan_pending      = false;

static bool via_macro_defaults_lookup_payload(uint8_t slot, const char **payload, void *context) {
    (void)context;

    if (!payload || slot >= VIA_MACRO_SLOT_COUNT) {
        return false;
    }

    *payload = via_macro_payloads[slot];
    return true;
}

static bool via_macro_defaults_load_ir(uint8_t slot, macro_payload_ir_t *ir, void *context) {
    const char *payload = NULL;

    (void)context;

    if (!ir || !via_macro_defaults_lookup_payload(slot, &payload, NULL)) {
        return false;
    }

    if (!payload || !*payload) {
        ir->length = 0;
        return true;
    }

    return macro_payload_compile(payload, ir);
}

static void log_invalid_via_macro_payload(uint8_t slot, const char *payload) {
#    ifdef CONSOLE_ENABLE
    uprintf("Invalid VIA default macro payload for VIA_MACRO_%u: %s\n", (unsigned int)slot, payload);
#    else
    (void)slot;
    (void)payload;
#    endif
}

static bool via_macro_payload_slot_is_valid(uint8_t slot) {
    const char        *payload = via_macro_payloads[slot];
    macro_payload_ir_t ir      = {0};

    if (via_macro_defaults_load_ir(slot, &ir, NULL)) {
        return true;
    }

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
    noah_qmk_via_macro_set_buffer(start_offset, writer->buffered, writer->chunk);
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
        const char        *payload = via_macro_payloads[slot];
        macro_payload_ir_t ir = {0};

        if (!via_macro_defaults_load_ir(slot, &ir, NULL)) {
            log_invalid_via_macro_payload(slot, payload);
            goto terminate_slot;
        }

        if (ir.length != 0 && !macro_payload_encode_ir_write(&ir, via_macro_seed_writer_write_byte, &writer, NULL)) {
            return false;
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
    uint16_t capacity = noah_qmk_via_macro_seed_capacity();
    uint16_t written  = 0;

    if (capacity == 0) {
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
    via_macro_seed_post_init_pending = noah_qmk_via_should_seed_defaults_post_init();
}

bool via_command_kb(uint8_t *data, uint8_t length) {
    uint8_t effects;

    (void)length;

    via_macro_provider_invalidate_all();
    effects = noah_qmk_via_command_effects(data[0]);
    if (effects & NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS) {
        via_macro_seed_scan_pending = true;
    }
    if (effects & NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB) {
        noah_rgb_runtime_invalidate_layer_maps();
    }
    if (effects & NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR) {
        noah_qmk_via_split_sync_command(data, length);
    }

    return false;
}

#endif
