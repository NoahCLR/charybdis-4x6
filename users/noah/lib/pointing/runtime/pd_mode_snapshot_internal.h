#pragma once

#include "../defs/pd_mode_flags.h"

// Compact read-only projection for consumers that need mode identity and
// owners together, without traits or the unused second local/display view.
typedef struct {
    uint8_t           active_index, locked_index;
    split_side_mask_t owner_sides;
    bool              has_owner_keys;
} pd_mode_owner_view_t;

void pd_mode_local_owner_view(pd_mode_owner_view_t *out, uint8_t *out_owner_bitmap);
void pd_mode_display_owner_view(pd_mode_owner_view_t *out, uint8_t *out_owner_bitmap);
