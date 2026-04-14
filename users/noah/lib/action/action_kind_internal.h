// ────────────────────────────────────────────────────────────────────────────
// Action Kind Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Shared internal action-kind metadata for authored validation, lifecycle
// dispatch, and handled-key hold policy.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "action_dispatch.h"

typedef struct {
    uint16_t caps;
    bool     keeps_registered_feedback;
    bool     preview_layer_uses_desc_layer;
} noah_action_kind_def_t;

const noah_action_kind_def_t *noah_action_kind_def(noah_action_kind_t kind);

static inline bool noah_action_desc_keeps_registered_feedback(noah_action_desc_t desc) {
    const noah_action_kind_def_t *def = noah_action_kind_def(desc.kind);
    return def ? def->keeps_registered_feedback : true;
}

static inline uint8_t noah_action_desc_preview_layer(noah_action_desc_t desc) {
    const noah_action_kind_def_t *def = noah_action_kind_def(desc.kind);
    return def && def->preview_layer_uses_desc_layer ? desc.layer : UINT8_MAX;
}
