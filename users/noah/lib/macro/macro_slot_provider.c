#include "macro_slot_provider.h"

static bool macro_slot_provider_slot_in_range(const macro_slot_provider_t *provider, uint8_t slot) {
    return provider && slot < provider->slot_count;
}

bool macro_slot_provider_load(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot) {
    if (!cache || !macro_slot_provider_slot_in_range(provider, slot) || !provider->load_ir) {
        return false;
    }

    if (cache[slot].state == MACRO_SLOT_CACHE_VALID) {
        return true;
    }
    if (cache[slot].state == MACRO_SLOT_CACHE_INVALID) {
        return false;
    }

    cache[slot].ir.length = 0;
    if (provider->load_ir(slot, &cache[slot].ir, provider->context)) {
        cache[slot].state = MACRO_SLOT_CACHE_VALID;
        return true;
    }

    cache[slot].state     = MACRO_SLOT_CACHE_INVALID;
    cache[slot].ir.length = 0;
    return false;
}

bool macro_slot_provider_encode_write(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written) {
    if (written) {
        *written = 0;
    }

    if (!write_byte || !macro_slot_provider_load(provider, cache, slot)) {
        return false;
    }

    if (cache[slot].ir.length == 0) {
        return true;
    }

    return macro_payload_encode_ir_write(&cache[slot].ir, write_byte, context, written);
}

static void macro_slot_provider_finish(macro_payload_finish_result_t result, void *context) {
    macro_slot_cache_t *slot = (macro_slot_cache_t *)context;

    (void)result;
    if (!slot) {
        return;
    }
    slot->pinned = false;
    if (slot->stale) {
        slot->state     = MACRO_SLOT_CACHE_UNCHECKED;
        slot->ir.length = 0u;
        slot->stale     = false;
    }
}

macro_payload_start_result_t macro_slot_provider_start(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source) {
    macro_payload_start_result_t result;

    if (!macro_slot_provider_load(provider, cache, slot)) {
        return MACRO_PAYLOAD_START_INVALID;
    }

    if (cache[slot].ir.length == 0) {
        return MACRO_PAYLOAD_START_EMPTY;
    }

    result = macro_payload_start_ir(&cache[slot].ir, text_output, interval, source, slot, macro_slot_provider_finish, &cache[slot]);
    if (result == MACRO_PAYLOAD_START_STARTED) {
        cache[slot].pinned = true;
        return result;
    }
    if (result == MACRO_PAYLOAD_START_INVALID) {
        macro_slot_provider_invalidate(provider, cache, slot);
        cache[slot].state = MACRO_SLOT_CACHE_INVALID;
    }
    return result;
}

void macro_slot_provider_invalidate(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot) {
    if (!cache || !macro_slot_provider_slot_in_range(provider, slot)) {
        return;
    }

    if (cache[slot].pinned) {
        cache[slot].stale = true;
        return;
    }

    cache[slot].state     = MACRO_SLOT_CACHE_UNCHECKED;
    cache[slot].ir.length = 0;
    cache[slot].stale     = false;
}

void macro_slot_provider_invalidate_all(const macro_slot_provider_t *provider, macro_slot_cache_t *cache) {
    if (!provider || !cache) {
        return;
    }

    for (uint8_t slot = 0; slot < provider->slot_count; slot++) {
        macro_slot_provider_invalidate(provider, cache, slot);
    }
}
