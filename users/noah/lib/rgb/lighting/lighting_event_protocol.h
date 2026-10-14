#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Internal split codec, independent of native padding and the profile format.
enum {
    NOAH_LIGHTING_EVENT_HELLO = 1,
    NOAH_LIGHTING_EVENT_CONFIRM = 2,
    NOAH_LIGHTING_EVENT_DATA = 3,
    NOAH_LIGHTING_EVENT_REPLY = 4,
    NOAH_LIGHTING_EVENT_PROFILE_BYTES = 22,
    NOAH_LIGHTING_EVENT_CONTROL_BYTES = 9,
    NOAH_LIGHTING_EVENT_HELLO_BYTES = 27,
    NOAH_LIGHTING_EVENT_GROUP_BYTES = 54,
    NOAH_LIGHTING_EVENT_DATA_BYTES = 63,
    NOAH_LIGHTING_EVENT_REPLY_BYTES = 10,
    NOAH_LIGHTING_EVENT_RULE_MASK_BYTES = 32,
    NOAH_LIGHTING_EVENT_OWNER_BYTES = 8,
};
_Static_assert(NOAH_LIGHTING_EVENT_DATA_BYTES <= 64, "event request convention");
_Static_assert(NOAH_LIGHTING_EVENT_REPLY_BYTES <= 32, "event reply convention");

typedef struct {
    uint64_t order;
    uint8_t thing_kind, thing, slot, outcome, sides, has_keys;
    uint8_t owners[NOAH_LIGHTING_EVENT_OWNER_BYTES];
    uint8_t qualified[NOAH_LIGHTING_EVENT_RULE_MASK_BYTES];
} noah_lighting_event_group_t;

bool noah_lighting_event_group_encode(uint8_t *bytes, size_t size, const noah_lighting_event_group_t *group);
bool noah_lighting_event_group_decode(noah_lighting_event_group_t *group, const uint8_t *bytes, size_t size);
void noah_lighting_event_put_u32(uint8_t *bytes, uint32_t value);
uint32_t noah_lighting_event_get_u32(const uint8_t *bytes);

#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE && defined(RPC_M2S_BUFFER_SIZE) && defined(RPC_S2M_BUFFER_SIZE)
_Static_assert(NOAH_LIGHTING_EVENT_DATA_BYTES <= RPC_M2S_BUFFER_SIZE, "event request fits existing QMK allocation");
_Static_assert(NOAH_LIGHTING_EVENT_REPLY_BYTES <= RPC_S2M_BUFFER_SIZE, "event reply fits existing QMK allocation");
#endif
