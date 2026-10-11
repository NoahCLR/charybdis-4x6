#include "macro_slot_provider.h"

// One window of the active macro, and where the next one starts. A macro
// longer than one window refills it from storage while it plays.
static macro_payload_ir_t            macro_slot_active_ir;
static macro_payload_stream_cursor_t macro_slot_active_cursor;
static macro_slot_provider_t         macro_slot_active_provider;
static uint8_t                       macro_slot_active_slot;
static macro_slot_metadata_t        *macro_slot_active_metadata;
static bool                          macro_slot_active_stale;

static bool macro_slot_provider_slot_in_range(const macro_slot_provider_t *provider, uint8_t slot) {
    return provider && slot < provider->slot_count;
}

static bool macro_slot_provider_load_window(const macro_slot_provider_t *provider, uint8_t slot) {
    macro_slot_active_ir.length     = 0u;
    macro_slot_active_ir.protection = 0u;
    macro_slot_active_ir.more       = false;
    return provider->load_ir(slot, &macro_slot_active_ir, &macro_slot_active_cursor, provider->context);
}

// Decodes every window of the slot, checking each against preflight when one
// is given, and leaves the first window loaded. Only a decoding failure marks
// the slot invalid: a preflight rejection depends on the host settings.
static bool macro_slot_provider_compile(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot, macro_payload_preflight_t *preflight) {
    bool windows = false;

    if (!metadata || !macro_slot_provider_slot_in_range(provider, slot) || !provider->load_ir || metadata[slot].state == MACRO_SLOT_CACHE_INVALID) {
        return false;
    }

    macro_slot_active_cursor = (macro_payload_stream_cursor_t){0};
    do {
        if (!macro_slot_provider_load_window(provider, slot)) {
            metadata[slot].state        = MACRO_SLOT_CACHE_INVALID;
            macro_slot_active_ir.length = 0u;
            macro_slot_active_ir.more   = false;
            return false;
        }
        if (preflight && !macro_payload_preflight_window(preflight, &macro_slot_active_ir)) {
            macro_slot_active_ir.length = 0u;
            macro_slot_active_ir.more   = false;
            return false;
        }
        windows = windows || macro_slot_active_ir.more;
    } while (macro_slot_active_ir.more);
    metadata[slot].state = MACRO_SLOT_CACHE_VALID;

    if (windows) {
        macro_slot_active_cursor = (macro_payload_stream_cursor_t){0};
        if (!macro_slot_provider_load_window(provider, slot)) {
            macro_slot_active_ir.length = 0u;
            macro_slot_active_ir.more   = false;
            return false;
        }
    }
    return true;
}

static bool macro_slot_provider_load_next(void *context) {
    (void)context;
    return macro_slot_active_metadata && !macro_slot_active_stale && macro_slot_provider_load_window(&macro_slot_active_provider, macro_slot_active_slot);
}

bool macro_slot_provider_validate(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot) {
    if (!metadata || !macro_slot_provider_slot_in_range(provider, slot) || !provider->load_ir) {
        return false;
    }
    if (metadata[slot].state == MACRO_SLOT_CACHE_VALID) {
        return true;
    }
    if (metadata[slot].state == MACRO_SLOT_CACHE_INVALID || macro_slot_active_metadata) {
        return false;
    }
    return macro_slot_provider_compile(provider, metadata, slot, NULL);
}

bool macro_slot_provider_encode_write(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written) {
    if (written) {
        *written = 0;
    }

    // The encoder writes one window; a macro longer than that is not re-encoded.
    if (!write_byte || macro_slot_active_metadata || !macro_slot_provider_compile(provider, metadata, slot, NULL) || macro_slot_active_cursor.offset != 0u) {
        return false;
    }

    if (macro_slot_active_ir.length == 0u && macro_slot_active_ir.protection == 0u) {
        return true;
    }

    return macro_payload_encode_ir_write(&macro_slot_active_ir, write_byte, context, written);
}

static void macro_slot_provider_finish(macro_payload_finish_result_t result, void *context) {
    macro_slot_metadata_t *slot = (macro_slot_metadata_t *)context;

    (void)result;
    if (!slot || slot != macro_slot_active_metadata) {
        return;
    }
    if (macro_slot_active_stale) {
        slot->state = MACRO_SLOT_CACHE_UNCHECKED;
    }
    macro_slot_active_ir.length = 0u;
    macro_slot_active_ir.more   = false;
    macro_slot_active_metadata  = NULL;
    macro_slot_active_stale     = false;
}

macro_payload_start_result_t macro_slot_provider_start(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source) {
    macro_payload_start_result_t result;
    macro_payload_preflight_t    preflight;

    if (!metadata || !macro_slot_provider_slot_in_range(provider, slot) || !provider->load_ir) {
        return MACRO_PAYLOAD_START_INVALID;
    }
    if (macro_slot_active_metadata) {
        // The engine rejects before reading the IR while active. Route through
        // that path so its existing busy diagnostics remain authoritative.
        return macro_payload_start_ir(NULL, text_output, interval, source, slot, NULL, NULL);
    }
    // Every window passes before the first key is typed, so a macro that
    // cannot play to its end never types part of itself.
    macro_payload_preflight_begin(&preflight);
    if (!macro_slot_provider_compile(provider, metadata, slot, &preflight) || !macro_payload_preflight_end(&preflight)) {
        return MACRO_PAYLOAD_START_INVALID;
    }
    if (macro_slot_active_ir.length == 0u) {
        return MACRO_PAYLOAD_START_EMPTY;
    }

    macro_slot_active_provider = *provider;
    macro_slot_active_slot     = slot;
    result                     = macro_payload_start_windows(&macro_slot_active_ir, &preflight, macro_slot_provider_load_next, NULL, text_output, interval, source, slot, macro_slot_provider_finish, &metadata[slot]);
    if (result == MACRO_PAYLOAD_START_STARTED) {
        macro_slot_active_metadata = &metadata[slot];
        macro_slot_active_stale    = false;
        return result;
    }
    // A successfully decoded slot remains structurally valid. Playback
    // preflight also depends on volatile host detection and current settings;
    // rejecting it must not poison the decoder cache until the bytes change.
    return result;
}

void macro_slot_provider_invalidate(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot) {
    if (!metadata || !macro_slot_provider_slot_in_range(provider, slot)) {
        return;
    }

    if (&metadata[slot] == macro_slot_active_metadata) {
        macro_slot_active_stale = true;
        if (macro_slot_active_ir.more) macro_slot_provider_storage_changing();
        return;
    }

    metadata[slot].state = MACRO_SLOT_CACHE_UNCHECKED;
}

void macro_slot_provider_storage_changing(void) {
    // Stop before the bank changes, including during the final window.
    if (macro_slot_active_metadata) {
        macro_slot_active_stale = true;
        (void)macro_payload_engine_cancel();
    }
}

void macro_slot_provider_invalidate_all(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata) {
    if (!provider || !metadata) {
        return;
    }

    for (uint8_t slot = 0; slot < provider->slot_count; slot++) {
        macro_slot_provider_invalidate(provider, metadata, slot);
    }
}
