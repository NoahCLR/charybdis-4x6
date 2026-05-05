// ────────────────────────────────────────────────────────────────────────────
// Macro Slot Providers
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "macro_payload.h"

typedef bool (*macro_slot_provider_load_ir_fn)(uint8_t slot, macro_payload_ir_t *ir, void *context);
typedef bool (*macro_slot_provider_lookup_payload_fn)(uint8_t slot, const char **payload, void *context);

typedef struct {
    uint8_t                               slot_count;
    macro_slot_provider_load_ir_fn        load_ir;
    macro_slot_provider_lookup_payload_fn lookup_payload;
    void                                 *context;
} macro_slot_provider_t;

typedef enum {
    MACRO_SLOT_CACHE_UNCHECKED = 0,
    MACRO_SLOT_CACHE_VALID,
    MACRO_SLOT_CACHE_INVALID,
} macro_slot_cache_state_t;

typedef struct {
    macro_slot_cache_state_t state;
    macro_payload_ir_t       ir;
} macro_slot_cache_t;

bool macro_slot_provider_load(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot);
bool macro_slot_provider_encode_write(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
bool macro_slot_provider_play(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot, macro_payload_text_output_t text_output, uint8_t interval);
void macro_slot_provider_invalidate(const macro_slot_provider_t *provider, macro_slot_cache_t *cache, uint8_t slot);
void macro_slot_provider_invalidate_all(const macro_slot_provider_t *provider, macro_slot_cache_t *cache);
