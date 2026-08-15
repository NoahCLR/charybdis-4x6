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

enum {
    MACRO_SLOT_CACHE_UNCHECKED = 0,
    MACRO_SLOT_CACHE_VALID,
    MACRO_SLOT_CACHE_INVALID,
};

typedef uint8_t macro_slot_cache_state_t;

typedef struct {
    macro_slot_cache_state_t state;
} macro_slot_metadata_t;

_Static_assert(sizeof(macro_slot_metadata_t) == 1u, "macro slot metadata must remain one byte");

bool                         macro_slot_provider_validate(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot);
bool                         macro_slot_provider_encode_write(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
macro_payload_start_result_t macro_slot_provider_start(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source);
void                         macro_slot_provider_invalidate(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata, uint8_t slot);
void                         macro_slot_provider_invalidate_all(const macro_slot_provider_t *provider, macro_slot_metadata_t *metadata);
