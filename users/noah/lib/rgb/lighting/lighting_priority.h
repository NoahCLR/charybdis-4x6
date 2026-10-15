#pragma once
#include <stdbool.h>
#include <stdint.h>

// The fact module supplies its existing natural priority, normalized so lower
// comes first. Entry recency only breaks exact equality; it is not a clock or
// a live/retained flag. Callers retain authored order when all fields tie.
typedef struct {
    uint64_t natural, entry;
    uint8_t  thing_kind, thing, ordinal;
} noah_lighting_priority_t;
bool noah_lighting_priority_before(const noah_lighting_priority_t *left, const noah_lighting_priority_t *right);
