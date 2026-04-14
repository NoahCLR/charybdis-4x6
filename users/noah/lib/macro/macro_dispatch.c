#include QMK_KEYBOARD_H // IWYU pragma: keep

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "macro_dispatch.h"
#include "macro_slot_provider.h"
#include "noah_keymap_ids.h"

static macro_slot_cache_t hardcoded_macro_slots[HARDCODED_MACRO_SLOT_COUNT];

static bool macro_dispatch_lookup_payload(uint8_t slot, const char **payload, void *context) {
    (void)context;

    if (!payload || slot >= HARDCODED_MACRO_SLOT_COUNT) {
        return false;
    }

    *payload = hardcoded_macro_payloads[slot];
    return true;
}

static bool macro_dispatch_load_ir(uint8_t slot, macro_payload_ir_t *ir, void *context) {
    const char *payload = NULL;

    (void)context;

    if (!ir || !macro_dispatch_lookup_payload(slot, &payload, NULL)) {
        return false;
    }

    if (!payload || !*payload) {
        ir->length = 0;
        return true;
    }

    return macro_payload_compile(payload, ir);
}

static const macro_slot_provider_t macro_dispatch_provider = {
    .slot_count     = HARDCODED_MACRO_SLOT_COUNT,
    .load_ir        = macro_dispatch_load_ir,
    .lookup_payload = macro_dispatch_lookup_payload,
};

static void macro_dispatch_log_invalid_payload(uint8_t slot, const char *payload) {
#ifdef CONSOLE_ENABLE
    uprintf("Invalid hardcoded macro payload for MACRO_%u: %s\n", (unsigned int)slot, payload);
#else
    (void)slot;
    (void)payload;
#endif
}

static bool macro_dispatch_validate_slot(uint8_t slot) {
    const char                *payload = hardcoded_macro_payloads[slot];
    macro_slot_cache_state_t before  = hardcoded_macro_slots[slot].state;

    if (macro_slot_provider_load(&macro_dispatch_provider, hardcoded_macro_slots, slot)) {
        return true;
    }

    if (before != MACRO_SLOT_CACHE_INVALID) {
        macro_dispatch_log_invalid_payload(slot, payload);
    }

    return false;
}

static void macro_dispatch_log_playback_failure(uint8_t slot) {
    const char *payload = hardcoded_macro_payloads[slot];

    macro_dispatch_log_invalid_payload(slot, payload);
}

void macro_dispatch_validate_all(void) {
    for (uint8_t slot = 0; slot < HARDCODED_MACRO_SLOT_COUNT; slot++) {
        (void)macro_dispatch_validate_slot(slot);
    }
}

bool macro_dispatch(uint16_t keycode) {
    uint8_t slot = 0;
    if (keycode < MACRO_0 || keycode > MACRO_15) {
        return false;
    }

    slot = (uint8_t)(keycode - MACRO_0);

    if (!macro_dispatch_validate_slot(slot)) {
        return true;
    }

    if (!macro_slot_provider_play(&macro_dispatch_provider, hardcoded_macro_slots, slot, MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN, 0)) {
        macro_dispatch_log_playback_failure(slot);
    }

    return true;
}
