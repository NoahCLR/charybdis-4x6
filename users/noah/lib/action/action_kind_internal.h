// ────────────────────────────────────────────────────────────────────────────
// Action Kind Internals
// ────────────────────────────────────────────────────────────────────────────
//
// Shared internal action-kind metadata for authored validation and
// handled-key/runtime policy decisions.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "action_dispatch.h"

typedef enum {
    NOAH_ACTION_DISPATCH_MACRO_PREFLIGHT   = (1u << 0),
    NOAH_ACTION_DISPATCH_PD_PRESS_INTERCEPT = (1u << 1),
    NOAH_ACTION_DISPATCH_PD_RELEASE_INTERCEPT = (1u << 2),
} noah_action_dispatch_flag_t;

typedef struct {
    bool                        defined;
    uint16_t                    caps;
    bool                        keeps_registered_feedback;
    bool                        preview_layer_uses_desc_layer;
    uint8_t                     dispatch_flags;
} noah_action_kind_def_t;

const noah_action_kind_def_t *noah_action_kind_def(noah_action_kind_t kind);
bool                         noah_action_kind_metadata_defined(noah_action_kind_t kind);
bool noah_action_desc_dispatches_macro_preflight(noah_action_desc_t desc);
bool noah_action_desc_intercepts_pd_mode_press(noah_action_desc_t desc);
bool noah_action_desc_intercepts_pd_mode_release(noah_action_desc_t desc);

static inline bool noah_action_desc_keeps_registered_feedback(noah_action_desc_t desc) {
    const noah_action_kind_def_t *def = noah_action_kind_def(desc.kind);
    return def ? def->keeps_registered_feedback : false;
}

static inline uint8_t noah_action_desc_preview_layer(noah_action_desc_t desc) {
    const noah_action_kind_def_t *def = noah_action_kind_def(desc.kind);
    return def && def->preview_layer_uses_desc_layer ? desc.layer : UINT8_MAX;
}
