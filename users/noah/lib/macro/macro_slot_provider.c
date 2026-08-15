#include "macro_slot_provider.h"

static macro_payload_ir_t    macro_slot_active_ir;
static macro_slot_metadata_t *macro_slot_active_metadata;
static bool                   macro_slot_active_stale;

static bool macro_slot_provider_slot_in_range(const macro_slot_provider_t *provider, uint8_t slot) {
    return provider && slot < provider->slot_count;
}

static bool macro_slot_provider_compile(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot) {
    if (!metadata || !macro_slot_provider_slot_in_range(provider, slot) || !provider->load_ir || metadata[slot].state == MACRO_SLOT_CACHE_INVALID) {
        return false;
    }

    macro_slot_active_ir.length = 0u;
    if (provider->load_ir(slot, &macro_slot_active_ir, provider->context)) {
        metadata[slot].state = MACRO_SLOT_CACHE_VALID;
        return true;
    }

    metadata[slot].state        = MACRO_SLOT_CACHE_INVALID;
    macro_slot_active_ir.length = 0u;
    return false;
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
    return macro_slot_provider_compile(provider, metadata, slot);
}

bool macro_slot_provider_encode_write(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written) {
    if (written) {
        *written = 0;
    }

    if (!write_byte || macro_slot_active_metadata || !macro_slot_provider_compile(provider, metadata, slot)) {
        return false;
    }

    if (macro_slot_active_ir.length == 0u) {
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
    macro_slot_active_metadata  = NULL;
    macro_slot_active_stale     = false;
}

macro_payload_start_result_t macro_slot_provider_start(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source) {
    macro_payload_start_result_t result;

    if (!metadata || !macro_slot_provider_slot_in_range(provider, slot) || !provider->load_ir) {
        return MACRO_PAYLOAD_START_INVALID;
    }
    if (macro_slot_active_metadata) {
        // The engine rejects before reading the IR while active. Route through
        // that path so its existing busy diagnostics remain authoritative.
        return macro_payload_start_ir(NULL, text_output, interval, source, slot, NULL, NULL);
    }
    if (!macro_slot_provider_compile(provider, metadata, slot)) {
        return MACRO_PAYLOAD_START_INVALID;
    }
    if (macro_slot_active_ir.length == 0u) {
        return MACRO_PAYLOAD_START_EMPTY;
    }

    result = macro_payload_start_ir(&macro_slot_active_ir, text_output, interval, source, slot, macro_slot_provider_finish, &metadata[slot]);
    if (result == MACRO_PAYLOAD_START_STARTED) {
        macro_slot_active_metadata = &metadata[slot];
        macro_slot_active_stale    = false;
        return result;
    }
    if (result == MACRO_PAYLOAD_START_INVALID) {
        metadata[slot].state = MACRO_SLOT_CACHE_INVALID;
    }
    return result;
}

void macro_slot_provider_invalidate(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot) {
    if (!metadata || !macro_slot_provider_slot_in_range(provider, slot)) {
        return;
    }

    if (&metadata[slot] == macro_slot_active_metadata) {
        macro_slot_active_stale = true;
        return;
    }

    metadata[slot].state = MACRO_SLOT_CACHE_UNCHECKED;
}

void macro_slot_provider_invalidate_all(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata) {
    if (!provider || !metadata) {
        return;
    }

    for (uint8_t slot = 0; slot < provider->slot_count; slot++) {
        macro_slot_provider_invalidate(provider, metadata, slot);
    }
}
