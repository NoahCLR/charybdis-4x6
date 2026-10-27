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
