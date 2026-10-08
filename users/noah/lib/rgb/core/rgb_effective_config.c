// ────────────────────────────────────────────────────────────────────────────
// Effective RGB Configuration Adapter
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_effective_config.h"

#include <string.h>
#include "../../profile/schema/profile_rgb_compiled_v1.h"

// Narrow host/feature variants can compile RGB stages without the profile
// runtime. Production's source manifest supplies strong implementations.
__attribute__((weak)) noah_effective_rgb_result_t noah_effective_rgb_capture_frame(noah_effective_rgb_frame_t *frame) {
    if (!frame) {
        return NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
    }
    *frame       = (noah_effective_rgb_frame_t){0};
    frame->valid = true;
    return NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK;
}

__attribute__((weak)) noah_effective_rgb_result_t noah_effective_rgb_frame_status(const noah_effective_rgb_frame_t *frame) {
    return frame && frame->valid && !frame->live ? NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK : NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
}

static noah_effective_rgb_result_t frame_status(const noah_effective_rgb_frame_t *frame, const noah_profile_rgb_v1_view_t **view) {
    noah_effective_rgb_result_t status = frame ? noah_effective_rgb_frame_status(frame) : NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK;
    *view                              = NULL;
    if (status == NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK) {
        *view = noah_profile_rgb_compiled_v1_view();
        return *view ? NOAH_EFFECTIVE_RGB_OK : NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
    }
    if (status == NOAH_EFFECTIVE_RGB_OK) *view = &frame->view;
    return status;
}

static hsv_t native_hsv(noah_profile_rgb_v1_hsv_t color) {
    return (hsv_t){.h = color.h, .s = color.s, .v = color.v};
}

noah_effective_rgb_result_t rgb_effective_config_capture_frame(noah_effective_rgb_frame_t *frame) {
    return noah_effective_rgb_capture_frame(frame);
}

bool rgb_effective_config_frame_current(const noah_effective_rgb_frame_t *frame) {
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    return status == NOAH_EFFECTIVE_RGB_OK;
}

bool rgb_effective_config_layer_stage_enabled(const noah_effective_rgb_frame_t *frame) {
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    return status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u;
}

bool rgb_effective_config_layer_color(const noah_effective_rgb_frame_t *frame, uint8_t layer, layer_color_config_t *color) {
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!color || layer >= LAYER_COUNT) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u && layer < view->layer_color_count) {
        noah_profile_rgb_v1_layer_color_t row;
        noah_profile_rgb_v1_error_t       error;

        if (noah_profile_rgb_v1_layer_color_at(view, layer, &row, &error) == NOAH_PROFILE_RGB_V1_OK && row.layer_id == layer) {
            *color = (layer_color_config_t){.color = native_hsv(row.color), .mode = row.mode};
            return true;
        }
    }
    return false;
}

uint8_t rgb_effective_config_layer_group_count(const noah_effective_rgb_frame_t *frame) {
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u) {
        return view->layer_group_count;
    }
    return 0u;
}

bool rgb_effective_config_layer_group_at(const noah_effective_rgb_frame_t *frame, uint8_t index, rgb_effective_layer_group_t *group) {
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u && index < view->layer_group_count) {
        noah_profile_rgb_v1_group_row_t row;
        noah_profile_rgb_v1_group_t     dictionary_group;
        noah_profile_rgb_v1_error_t     error;

        if (noah_profile_rgb_v1_layer_group_at(view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK || row.group_id >= view->group_count || noah_profile_rgb_v1_group_at(view, row.group_id, &dictionary_group, &error) != NOAH_PROFILE_RGB_V1_OK || dictionary_group.id != row.group_id) {
            return false;
        }
        group->selector = row.selector;
        group->color    = native_hsv(row.color);
        memcpy(group->bitmap, dictionary_group.bitmap, sizeof(group->bitmap));
        return true;
    }
    return false;
}

bool rgb_effective_config_automouse_stage_enabled(const noah_effective_rgb_frame_t *frame) {
#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    return status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE) != 0u;
#else
    (void)frame;
    return false;
#endif
}

bool rgb_effective_config_automouse(const noah_effective_rgb_frame_t *frame, automouse_fade_end_config_t *config) {
#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!config) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE) != 0u) {
        noah_profile_rgb_v1_automouse_t row;
        noah_profile_rgb_v1_error_t     error;

        if (noah_profile_rgb_v1_automouse(view, &row, &error) == NOAH_PROFILE_RGB_V1_OK) {
            *config = (automouse_fade_end_config_t){.mode = (automouse_fade_end_mode_t)row.mode, .end_color = native_hsv(row.end_color)};
            return true;
        }
    }
    return false;
#else
    (void)frame;
    (void)config;
    return false;
#endif
}

bool rgb_effective_config_pd_stage_enabled(const noah_effective_rgb_frame_t *frame) {
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    return status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_PD_MODE) != 0u;
#else
    (void)frame;
    return false;
#endif
}

bool rgb_effective_config_pd_color(const noah_effective_rgb_frame_t *frame, pd_mode_id_t mode_id, pd_mode_color_t *color) {
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!color || mode_id >= PD_MODE_COUNT) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_PD_MODE) != 0u) {
        noah_profile_rgb_v1_pd_color_t row;
        noah_profile_rgb_v1_error_t    error;

        // Rows are in ID order and the firmware's domain has one per slot, so
        // the row at the slot's index is normally its own: one record read per
        // frame however many slots exist. Otherwise scan, as rows may skip IDs
        // a narrower supported mask leaves out.
        if (mode_id < view->pd_color_count && noah_profile_rgb_v1_pd_color_at(view, mode_id, &row, &error) == NOAH_PROFILE_RGB_V1_OK && row.pd_mode_id == mode_id) {
            *color = (pd_mode_color_t){.pointing_mode = pd_mode_mask_from_id(mode_id), .color = native_hsv(row.color), .locality = (rgb_locality_t)row.locality};
            return true;
        }
        for (uint8_t index = 0u; index < view->pd_color_count; index++) {
            if (noah_profile_rgb_v1_pd_color_at(view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK) {
                return false;
            }
            if (row.pd_mode_id == mode_id) {
                *color = (pd_mode_color_t){.pointing_mode = pd_mode_mask_from_id(mode_id), .color = native_hsv(row.color), .locality = (rgb_locality_t)row.locality};
                return true;
            }
            if (row.pd_mode_id > mode_id) {
                break;
            }
        }
    }
    return false;
#else
    (void)frame;
    (void)mode_id;
    (void)color;
    return false;
#endif
}

uint8_t rgb_effective_config_pd_group_count(const noah_effective_rgb_frame_t *frame) {
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_PD_MODE) != 0u) {
        return view->pd_group_count;
    }
#else
    (void)frame;
#endif
    return 0u;
}

#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)

#endif

bool rgb_effective_config_pd_group_at(const noah_effective_rgb_frame_t *frame, uint8_t index, rgb_effective_pd_group_t *group) {
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_PD_MODE) != 0u && index < view->pd_group_count) {
        noah_profile_rgb_v1_group_row_t row;
        noah_profile_rgb_v1_group_t     dictionary_group;
        noah_profile_rgb_v1_error_t     error;

        if (noah_profile_rgb_v1_pd_group_at(view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK || row.group_id >= view->group_count || noah_profile_rgb_v1_group_at(view, row.group_id, &dictionary_group, &error) != NOAH_PROFILE_RGB_V1_OK || dictionary_group.id != row.group_id) {
            return false;
        }
        group->selector = row.selector;
        group->color    = native_hsv(row.color);
        memcpy(group->bitmap, dictionary_group.bitmap, sizeof(group->bitmap));
        return true;
    }
#else
    (void)frame;
    (void)index;
    (void)group;
#endif
    return false;
}

bool rgb_effective_config_combo_stage_enabled(const noah_effective_rgb_frame_t *frame) {
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    return status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) != 0u;
#else
    (void)frame;
    return false;
#endif
}

bool rgb_effective_config_combo_feedback(const noah_effective_rgb_frame_t *frame, combo_feedback_color_config_t *config) {
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!config) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) != 0u) {
        noah_profile_rgb_v1_feedback_t row;
        noah_profile_rgb_v1_error_t    error;

        if (noah_profile_rgb_v1_combo_feedback(view, &row, &error) == NOAH_PROFILE_RGB_V1_OK) {
            *config = (combo_feedback_color_config_t){.color = native_hsv(row.color), .locality = (rgb_locality_t)row.locality};
            return true;
        }
    }
    return false;
#else
    (void)frame;
    (void)config;
    return false;
#endif
}

uint8_t rgb_effective_config_combo_group_count(const noah_effective_rgb_frame_t *frame) {
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) != 0u) {
        return view->combo_group_count;
    }
#else
    (void)frame;
#endif
    return 0u;
}

#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)

#endif

bool rgb_effective_config_combo_group_at(const noah_effective_rgb_frame_t *frame, uint8_t index, rgb_effective_combo_group_t *group) {
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) != 0u && index < view->combo_group_count) {
        noah_profile_rgb_v1_combo_group_row_t row;
        noah_profile_rgb_v1_group_t           dictionary_group;
        noah_profile_rgb_v1_error_t           error;

        if (noah_profile_rgb_v1_combo_group_at(view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK || row.group_id >= view->group_count || noah_profile_rgb_v1_group_at(view, row.group_id, &dictionary_group, &error) != NOAH_PROFILE_RGB_V1_OK || dictionary_group.id != row.group_id) {
            return false;
        }
        group->color = native_hsv(row.color);
        memcpy(group->bitmap, dictionary_group.bitmap, sizeof(group->bitmap));
        return true;
    }
#else
    (void)frame;
    (void)index;
    (void)group;
#endif
    return false;
}

bool rgb_effective_config_key_stage_enabled(const noah_effective_rgb_frame_t *frame) {
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    return status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u;
#else
    (void)frame;
    return false;
#endif
}

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static bool key_feedback_fixed_config(const noah_effective_rgb_frame_t *frame, hsv_t *tap_committed_color, hsv_t *hold_active_color, hsv_t *long_hold_active_color, key_feedback_tap_commit_mode_t *tap_commit_mode, rgb_locality_t *locality) {
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!(tap_committed_color && hold_active_color && long_hold_active_color && tap_commit_mode && locality)) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u) {
        noah_profile_rgb_v1_key_feedback_t row;
        noah_profile_rgb_v1_error_t        error;

        if (noah_profile_rgb_v1_key_feedback(view, &row, &error) == NOAH_PROFILE_RGB_V1_OK) {
            *tap_committed_color    = native_hsv(row.tap_committed_color);
            *hold_active_color      = native_hsv(row.hold_active_color);
            *long_hold_active_color = native_hsv(row.long_hold_active_color);
            *tap_commit_mode        = (key_feedback_tap_commit_mode_t)row.tap_commit_mode;
            *locality               = (rgb_locality_t)row.locality;
            return true;
        }
    }
    return false;
}
#endif

bool rgb_effective_config_key_feedback(const noah_effective_rgb_frame_t *frame, rgb_effective_key_feedback_t *config) {
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!config) {
        return false;
    }
    memset(config, 0, sizeof(*config));
    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u && view->tap_branch_color_count <= ARRAY_SIZE(config->tap_branch_colors)) {
        config->tap_branch_color_count = view->tap_branch_color_count;
        for (uint8_t index = 0u; index < config->tap_branch_color_count; index++) {
            noah_profile_rgb_v1_hsv_t   color;
            noah_profile_rgb_v1_error_t error;

            if (noah_profile_rgb_v1_tap_branch_color_at(view, index, &color, &error) != NOAH_PROFILE_RGB_V1_OK) {
                return false;
            }
            config->tap_branch_colors[index] = native_hsv(color);
        }
    } else {
        return false;
    }

    return key_feedback_fixed_config(frame, &config->tap_committed_color, &config->hold_active_color, &config->long_hold_active_color, &config->tap_commit_mode, &config->locality);
#else
    (void)frame;
    (void)config;
    return false;
#endif
}

uint8_t rgb_effective_config_key_group_count(const noah_effective_rgb_frame_t *frame) {
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u) {
        return view->key_group_count;
    }
#else
    (void)frame;
#endif
    return 0u;
}

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE

#endif

bool rgb_effective_config_key_group_at(const noah_effective_rgb_frame_t *frame, uint8_t index, rgb_effective_key_group_t *group) {
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_effective_rgb_result_t       status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_EFFECTIVE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u && index < view->key_group_count) {
        noah_profile_rgb_v1_group_row_t row;
        noah_profile_rgb_v1_group_t     dictionary_group;
        noah_profile_rgb_v1_error_t     error;

        if (noah_profile_rgb_v1_key_group_at(view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK || row.group_id >= view->group_count || noah_profile_rgb_v1_group_at(view, row.group_id, &dictionary_group, &error) != NOAH_PROFILE_RGB_V1_OK || dictionary_group.id != row.group_id) {
            return false;
        }
        group->selector = row.selector;
        group->color    = native_hsv(row.color);
        memcpy(group->bitmap, dictionary_group.bitmap, sizeof(group->bitmap));
        return true;
    }
#else
    (void)frame;
    (void)index;
    (void)group;
#endif
    return false;
}

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
key_feedback_tap_commit_mode_t key_feedback_tap_commit_mode(void) {
    noah_effective_rgb_frame_t     frame;
    noah_effective_rgb_result_t    result;
    hsv_t                          tap_committed_color;
    hsv_t                          hold_active_color;
    hsv_t                          long_hold_active_color;
    key_feedback_tap_commit_mode_t tap_commit_mode;
    rgb_locality_t                 locality;

    result = rgb_effective_config_capture_frame(&frame);
    if ((result != NOAH_EFFECTIVE_RGB_OK && result != NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK) || !key_feedback_fixed_config(&frame, &tap_committed_color, &hold_active_color, &long_hold_active_color, &tap_commit_mode, &locality) || !rgb_effective_config_frame_current(&frame)) {
        return KEY_FEEDBACK_TAP_COMMIT_OFF;
    }
    return tap_commit_mode;
}
#endif
