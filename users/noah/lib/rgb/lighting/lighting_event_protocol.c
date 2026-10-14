#include "lighting_event_protocol.h"
#include <string.h>

void noah_lighting_event_put_u32(uint8_t *bytes, uint32_t value) {
    for (uint8_t i = 0; i < 4; i++) bytes[i] = (uint8_t)(value >> (8u * i));
}
uint32_t noah_lighting_event_get_u32(const uint8_t *bytes) {
    uint32_t result = 0;
    for (uint8_t i = 0; i < 4; i++) result |= (uint32_t)bytes[i] << (8u * i);
    return result;
}
static bool group_valid(const noah_lighting_event_group_t *group) {
    uint8_t any = 0;
    for (uint8_t i = 0; i < NOAH_LIGHTING_EVENT_RULE_MASK_BYTES; i++) any |= group->qualified[i];
    return any != 0 && (group->qualified[31] & 0x80u) == 0 && group->sides <= 3u && group->has_keys <= 1u;
}
bool noah_lighting_event_group_encode(uint8_t *bytes, size_t size, const noah_lighting_event_group_t *group) {
    if (!bytes || !group || size != NOAH_LIGHTING_EVENT_GROUP_BYTES || !group_valid(group)) return false;
    for (uint8_t i = 0; i < 8; i++) bytes[i] = (uint8_t)(group->order >> (8u * i));
    bytes[8] = group->thing_kind;
    bytes[9] = group->thing;
    bytes[10] = group->slot;
    bytes[11] = group->outcome;
    bytes[12] = group->sides;
    bytes[13] = group->has_keys;
    memcpy(bytes + 14, group->owners, 8);
    memcpy(bytes + 22, group->qualified, 32);
    return true;
}
bool noah_lighting_event_group_decode(noah_lighting_event_group_t *group, const uint8_t *bytes, size_t size) {
    if (!group || !bytes || size != NOAH_LIGHTING_EVENT_GROUP_BYTES) return false;
    // Validate before touching the destination; a failed read keeps its
    // previous coherent group without a second whole-group stack copy.
    uint8_t any = 0;
    for (uint8_t i = 0; i < 32; i++) any |= bytes[22u + i];
    if (!any || (bytes[53] & 0x80u) != 0 || bytes[12] > 3u || bytes[13] > 1u) return false;
    group->order = 0;
    for (uint8_t i = 0; i < 8; i++) group->order |= (uint64_t)bytes[i] << (8u * i);
    group->thing_kind = bytes[8];
    group->thing = bytes[9];
    group->slot = bytes[10];
    group->outcome = bytes[11];
    group->sides = bytes[12];
    group->has_keys = bytes[13];
    memcpy(group->owners, bytes + 14, 8);
    memcpy(group->qualified, bytes + 22, 32);
    return true;
}
