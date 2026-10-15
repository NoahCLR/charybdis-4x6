#include "lighting_priority.h"

bool noah_lighting_priority_before(const noah_lighting_priority_t *left, const noah_lighting_priority_t *right) {
    if (left->natural != right->natural) return left->natural < right->natural;
    if (left->entry != right->entry) return left->entry > right->entry;
    if (left->thing_kind != right->thing_kind) return left->thing_kind < right->thing_kind;
    if (left->thing != right->thing) return left->thing < right->thing;
    return left->ordinal < right->ordinal;
}
