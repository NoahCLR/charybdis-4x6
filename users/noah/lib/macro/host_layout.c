#include "host_layout.h"

#include <stddef.h>

const host_layout_t *host_layout_get(uint8_t id) {
    for (uint8_t index = 0u; index < host_layout_table_count; index++) {
        if (host_layouts[index].id == id) return &host_layouts[index];
    }
    return NULL;
}

bool host_layout_lookup(const host_layout_t *layout, uint32_t codepoint, uint16_t strokes[2]) {
    if (!layout || !strokes) return false;
    uint16_t low = 0u, high = layout->count;
    while (low < high) {
        uint16_t middle = (uint16_t)(low + (high - low) / 2u);
        uint32_t found  = layout->chars[middle].codepoint;
        if (found == codepoint) {
            strokes[0] = layout->chars[middle].strokes[0];
            strokes[1] = layout->chars[middle].strokes[1];
            return true;
        }
        if (found < codepoint) low = (uint16_t)(middle + 1u);
        else high = middle;
    }
    return false;
}

// The generator refuses a key list this does not index.
static bool host_layout_key_slot(uint8_t keycode, uint8_t *slot) {
    if (keycode >= 0x04u && keycode <= 0x27u) *slot = (uint8_t)(keycode - 0x04u);
    else if (keycode >= 0x2Cu && keycode <= 0x38u) *slot = (uint8_t)(keycode - 0x2Cu + 36u);
    else if (keycode == 0x64u) *slot = 49u;
    else return false;
    return true;
}

host_layout_types_t host_layout_stroke_types(const host_layout_t *layout, uint16_t stroke, uint16_t *codepoint) {
    uint8_t slot;
    if (!layout || !codepoint || (stroke & ~(0xFFu | HOST_LAYOUT_STROKE_SHIFT | HOST_LAYOUT_STROKE_ALTGR)) || !host_layout_key_slot(HOST_LAYOUT_STROKE_KEYCODE(stroke), &slot)) return HOST_LAYOUT_TYPES_NOTHING;
    uint16_t cell = layout->keys[slot][(stroke >> 8u) & 3u];
    if (cell == 0u) return HOST_LAYOUT_TYPES_NOTHING;
    if (cell == HOST_LAYOUT_DEAD_KEY) return HOST_LAYOUT_TYPES_DEAD_KEY;
    *codepoint = cell;
    return HOST_LAYOUT_TYPES_CHARACTER;
}

bool host_layout_compose(const host_layout_t *layout, uint16_t dead, uint16_t stroke, uint16_t *codepoint) {
    if (!layout || !codepoint) return false;
    for (uint16_t index = 0u; index < layout->dead_count; index++) {
        const host_layout_dead_t *entry = &layout->deads[index];
        if (entry->stroke != dead) continue;
        uint16_t low = entry->first, high = (uint16_t)(entry->first + entry->count);
        while (low < high) {
            uint16_t middle = (uint16_t)(low + (high - low) / 2u);
            uint16_t found  = layout->composes[middle].stroke;
            if (found == stroke) {
                *codepoint = layout->composes[middle].codepoint;
                return true;
            }
            if (found < stroke) low = (uint16_t)(middle + 1u);
            else high = middle;
        }
        return false;
    }
    return false;
}
