#include "macro_slot_provider.h"

static bool macro_slot_provider_slot_in_range(const macro_slot_provider_t *provider, uint8_t slot) {
    return provider && slot < provider->slot_count;
}

bool macro_slot_provider_lookup(const macro_slot_provider_t *provider, uint8_t slot, const char **payload) {
    if (!payload || !macro_slot_provider_slot_in_range(provider, slot)) {
        return false;
    }

    *payload = NULL;
    if (!provider->lookup_payload) {
        return true;
    }

    return provider->lookup_payload(slot, payload, provider->context);
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

void macro_slot_provider_validate_all(const macro_slot_provider_t *provider, macro_slot_cache_t *cache) {
    if (!provider || !cache) {
        return;
    }

    for (uint8_t slot = 0; slot < provider->slot_count; slot++) {
        (void)macro_slot_provider_load(provider, cache, slot);
    }
}

bool macro_slot_provider_play(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot, macro_payload_text_output_t text_output, uint8_t interval) {
    if (!macro_slot_provider_load(provider, cache, slot)) {
        return false;
    }

    if (cache[slot].ir.length == 0) {
        return true;
    }

    if (macro_payload_play_ir_with_text_output(&cache[slot].ir, text_output, interval)) {
        return true;
    }

    macro_slot_provider_invalidate(provider, cache, slot);
    cache[slot].state = MACRO_SLOT_CACHE_INVALID;
    return false;
}

void macro_slot_provider_invalidate(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot) {
    if (!cache || !macro_slot_provider_slot_in_range(provider, slot)) {
        return;
    }

    cache[slot].state     = MACRO_SLOT_CACHE_UNCHECKED;
    cache[slot].ir.length = 0;
}

void macro_slot_provider_invalidate_all(const macro_slot_provider_t *provider, macro_slot_cache_t *cache) {
    if (!provider || !cache) {
        return;
    }

    for (uint8_t slot = 0; slot < provider->slot_count; slot++) {
        macro_slot_provider_invalidate(provider, cache, slot);
    }
}
