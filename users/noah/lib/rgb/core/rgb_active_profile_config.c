// ────────────────────────────────────────────────────────────────────────────
// Active-Profile RGB Configuration Adapter
// ────────────────────────────────────────────────────────────────────────────

#include "rgb_active_profile_config.h"

#include <string.h>
#include "../../profile/schema/profile_rgb_compiled_v1.h"

// Narrow host/feature variants can compile RGB stages without the profile
// runtime. Production's source manifest supplies strong implementations.
__attribute__((weak)) noah_active_profile_rgb_result_t noah_active_profile_rgb_capture_frame(noah_active_profile_rgb_frame_t *frame) {
    if (!frame) {
        return NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
    }
    *frame       = (noah_active_profile_rgb_frame_t){0};
    frame->valid = true;
    return NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK;
}

__attribute__((weak)) noah_active_profile_rgb_result_t noah_active_profile_rgb_frame_status(const noah_active_profile_rgb_frame_t *frame) {
    return frame && frame->valid && !frame->live ? NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK : NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
}

static noah_active_profile_rgb_result_t frame_status(const noah_active_profile_rgb_frame_t *frame, const noah_profile_rgb_v1_view_t **view) {
    noah_active_profile_rgb_result_t status = frame ? noah_active_profile_rgb_frame_status(frame) : NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK;
    *view                                   = NULL;
    if (status == NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK) {
        *view = noah_profile_rgb_compiled_v1_view();
        return *view ? NOAH_ACTIVE_PROFILE_RGB_OK : NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
    }
    if (status == NOAH_ACTIVE_PROFILE_RGB_OK) *view = &frame->view;
    return status;
}

#ifdef RGB_ACTIVE_PROFILE_CONFIG_TEST_BACKEND
static rgb_active_profile_config_test_reads_t test_reads;
#    define COUNT_READ(field_) (test_reads.field_++)
#else
#    define COUNT_READ(field_) ((void)0)
#endif

static hsv_t native_hsv(noah_profile_rgb_v1_hsv_t color) {
    return (hsv_t){.h = color.h, .s = color.s, .v = color.v};
}

noah_active_profile_rgb_result_t rgb_active_profile_config_capture_frame(noah_active_profile_rgb_frame_t *frame) {
    return noah_active_profile_rgb_capture_frame(frame);
}

bool rgb_active_profile_config_frame_current(const noah_active_profile_rgb_frame_t *frame) {
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    return status == NOAH_ACTIVE_PROFILE_RGB_OK;
}

bool rgb_active_profile_config_layer_stage_enabled(const noah_active_profile_rgb_frame_t *frame) {
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    return status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u;
}

bool rgb_active_profile_config_layer_color(const noah_active_profile_rgb_frame_t *frame, uint8_t layer, layer_color_config_t *color) {
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!color || layer >= LAYER_COUNT) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u && layer < view->layer_color_count) {
        noah_profile_rgb_v1_layer_color_t row;
        noah_profile_rgb_v1_error_t       error;

        if (noah_profile_rgb_v1_layer_color_at(view, layer, &row, &error) == NOAH_PROFILE_RGB_V1_OK && row.layer_id == layer) {
            *color = (layer_color_config_t){.color = native_hsv(row.color), .mode = row.mode};
            return true;
        }
    }
    return false;
}

uint8_t rgb_active_profile_config_layer_group_count(const noah_active_profile_rgb_frame_t *frame) {
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u) {
        return view->layer_group_count;
    }
    return 0u;
}

bool rgb_active_profile_config_layer_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_layer_group_t *group) {
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u && index < view->layer_group_count) {
        noah_profile_rgb_v1_group_row_t row;
        noah_profile_rgb_v1_group_t     dictionary_group;
        noah_profile_rgb_v1_error_t     error;

        COUNT_READ(layer_rows);
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

bool rgb_active_profile_config_automouse_stage_enabled(const noah_active_profile_rgb_frame_t *frame) {
#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    return status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE) != 0u;
#else
    (void)frame;
    return false;
#endif
}

bool rgb_active_profile_config_automouse(const noah_active_profile_rgb_frame_t *frame, automouse_fade_end_config_t *config) {
#if defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE) && defined(RGB_AUTOMOUSE_GRADIENT_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!config) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE) != 0u) {
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

bool rgb_active_profile_config_pd_stage_enabled(const noah_active_profile_rgb_frame_t *frame) {
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    return status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_POINTING_MODE) != 0u;
#else
    (void)frame;
    return false;
#endif
}

bool rgb_active_profile_config_pd_color(const noah_active_profile_rgb_frame_t *frame, pointing_mode_id_t mode_id, pointing_mode_color_t *color) {
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!color || mode_id >= POINTING_MODE_COUNT) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_POINTING_MODE) != 0u) {
        noah_profile_rgb_v1_pd_color_t row;
        noah_profile_rgb_v1_error_t    error;

        // Rows are in ID order and the firmware's domain has one per slot, so
        // the row at the slot's index is normally its own: one record read per
        // frame however many slots exist. Otherwise scan, as rows may skip IDs
        // a narrower supported mask leaves out.
        if (mode_id < view->pd_color_count && noah_profile_rgb_v1_pd_color_at(view, mode_id, &row, &error) == NOAH_PROFILE_RGB_V1_OK && row.pointing_mode_id == mode_id) {
            *color = (pointing_mode_color_t){.pointing_mode = pointing_mode_mask_from_id(mode_id), .color = native_hsv(row.color), .locality = (rgb_locality_t)row.locality};
            return true;
        }
        for (uint8_t index = 0u; index < view->pd_color_count; index++) {
            if (noah_profile_rgb_v1_pd_color_at(view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK) {
                return false;
            }
            if (row.pointing_mode_id == mode_id) {
                *color = (pointing_mode_color_t){.pointing_mode = pointing_mode_mask_from_id(mode_id), .color = native_hsv(row.color), .locality = (rgb_locality_t)row.locality};
                return true;
            }
            if (row.pointing_mode_id > mode_id) {
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

uint8_t rgb_active_profile_config_pd_group_count(const noah_active_profile_rgb_frame_t *frame) {
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_POINTING_MODE) != 0u) {
        return view->pd_group_count;
    }
#else
    (void)frame;
#endif
    return 0u;
}

#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)

#endif

bool rgb_active_profile_config_pd_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_pd_group_t *group) {
#if defined(POINTING_DEVICE_ENABLE) && defined(RGB_POINTING_MODE_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_POINTING_MODE) != 0u && index < view->pd_group_count) {
        noah_profile_rgb_v1_group_row_t row;
        noah_profile_rgb_v1_group_t     dictionary_group;
        noah_profile_rgb_v1_error_t     error;

        COUNT_READ(pd_rows);
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

bool rgb_active_profile_config_combo_stage_enabled(const noah_active_profile_rgb_frame_t *frame) {
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    return status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) != 0u;
#else
    (void)frame;
    return false;
#endif
}

bool rgb_active_profile_config_combo_feedback(const noah_active_profile_rgb_frame_t *frame, combo_feedback_color_config_t *config) {
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!config) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) != 0u) {
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

uint8_t rgb_active_profile_config_combo_group_count(const noah_active_profile_rgb_frame_t *frame) {
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) != 0u) {
        return view->combo_group_count;
    }
#else
    (void)frame;
#endif
    return 0u;
}

#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)

#endif

bool rgb_active_profile_config_combo_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_combo_group_t *group) {
#if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) != 0u && index < view->combo_group_count) {
        noah_profile_rgb_v1_combo_group_row_t row;
        noah_profile_rgb_v1_group_t           dictionary_group;
        noah_profile_rgb_v1_error_t           error;

        COUNT_READ(combo_rows);
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

bool rgb_active_profile_config_key_stage_enabled(const noah_active_profile_rgb_frame_t *frame) {
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    return status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u;
#else
    (void)frame;
    return false;
#endif
}

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
static bool key_feedback_fixed_config(const noah_active_profile_rgb_frame_t *frame, hsv_t *tap_committed_color, hsv_t *hold_active_color, hsv_t *long_hold_active_color, key_feedback_tap_commit_mode_t *tap_commit_mode, rgb_locality_t *locality) {
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!(tap_committed_color && hold_active_color && long_hold_active_color && tap_commit_mode && locality)) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u) {
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

bool rgb_active_profile_config_key_feedback(const noah_active_profile_rgb_frame_t *frame, rgb_active_profile_key_feedback_t *config) {
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!config) {
        return false;
    }
    memset(config, 0, sizeof(*config));
    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u && view->tap_branch_color_count <= ARRAY_SIZE(config->tap_branch_colors)) {
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

uint8_t rgb_active_profile_config_key_group_count(const noah_active_profile_rgb_frame_t *frame) {
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u) {
        return view->key_group_count;
    }
#else
    (void)frame;
#endif
    return 0u;
}

bool rgb_active_profile_config_key_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_key_group_t *group) {
#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u && index < view->key_group_count) {
        noah_profile_rgb_v1_group_row_t row;
        noah_profile_rgb_v1_group_t     dictionary_group;
        noah_profile_rgb_v1_error_t     error;

        COUNT_READ(key_rows);
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
    noah_active_profile_rgb_frame_t  frame;
    noah_active_profile_rgb_result_t result;
    hsv_t                            tap_committed_color;
    hsv_t                            hold_active_color;
    hsv_t                            long_hold_active_color;
    key_feedback_tap_commit_mode_t   tap_commit_mode;
    rgb_locality_t                   locality;

    result = rgb_active_profile_config_capture_frame(&frame);
    if ((result != NOAH_ACTIVE_PROFILE_RGB_OK && result != NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK) || !key_feedback_fixed_config(&frame, &tap_committed_color, &hold_active_color, &long_hold_active_color, &tap_commit_mode, &locality) || !rgb_active_profile_config_frame_current(&frame)) {
        return KEY_FEEDBACK_TAP_COMMIT_OFF;
    }
    return tap_commit_mode;
}
#endif

bool rgb_active_profile_config_autocorrect_stage_enabled(const noah_active_profile_rgb_frame_t *frame) {
#ifdef RGB_AUTOCORRECT_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    return status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT) != 0u;
#else
    (void)frame;
    return false;
#endif
}

bool rgb_active_profile_config_autocorrect_feedback(const noah_active_profile_rgb_frame_t *frame, rgb_active_profile_autocorrect_feedback_t *config) {
#ifdef RGB_AUTOCORRECT_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!config) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT) != 0u) {
        noah_profile_rgb_v1_autocorrect_feedback_t row;
        noah_profile_rgb_v1_error_t                error;

        if (noah_profile_rgb_v1_autocorrect_feedback(view, &row, &error) == NOAH_PROFILE_RGB_V1_OK) {
            for (uint8_t semantic = 0u; semantic < ARRAY_SIZE(config->colors); semantic++) {
                config->colors[semantic] = native_hsv(row.colors[semantic]);
            }
            config->locality = (rgb_locality_t)row.locality;
            return true;
        }
    }
#else
    (void)frame;
    (void)config;
#endif
    return false;
}

uint8_t rgb_active_profile_config_autocorrect_group_count(const noah_active_profile_rgb_frame_t *frame) {
#ifdef RGB_AUTOCORRECT_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT) != 0u) {
        return view->autocorrect_group_count;
    }
#else
    (void)frame;
#endif
    return 0u;
}

bool rgb_active_profile_config_autocorrect_group_at(const noah_active_profile_rgb_frame_t *frame, uint8_t index, rgb_active_profile_autocorrect_group_t *group) {
#ifdef RGB_AUTOCORRECT_FEEDBACK_ENABLE
    const noah_profile_rgb_v1_view_t *view;
    noah_active_profile_rgb_result_t  status = frame_status(frame, &view);

    if (!group) {
        return false;
    }

    if (status == NOAH_ACTIVE_PROFILE_RGB_OK && (view->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOCORRECT) != 0u && index < view->autocorrect_group_count) {
        noah_profile_rgb_v1_group_row_t row;
        noah_profile_rgb_v1_group_t     dictionary_group;
        noah_profile_rgb_v1_error_t     error;

        COUNT_READ(autocorrect_rows);
        if (noah_profile_rgb_v1_autocorrect_group_at(view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK || row.group_id >= view->group_count || noah_profile_rgb_v1_group_at(view, row.group_id, &dictionary_group, &error) != NOAH_PROFILE_RGB_V1_OK || dictionary_group.id != row.group_id) {
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

// The selectors of the frame's rows, keyed like the layer stage's colour cache
// (rgb_layer_stage.c): by the frame's epoch, publication count and liveness.
// An epoch is never reused while the runtime lives, so selectors read while a
// publication landed can never be current again, and a frame that is no longer
// current gets none.
static struct {
    uint8_t  selectors[NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS];
    uint32_t epoch;
    uint32_t publication_count;
    uint8_t  layer_count;
    uint8_t  pd_count;
    uint8_t  key_count;
    uint8_t  autocorrect_count;
    bool     live;
    bool     valid;
} selector_cache;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(selector_cache) <= NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS + 16u, "the lighting selector cache is one byte per group row");
#endif

typedef noah_profile_rgb_v1_result_t (*group_row_reader_t)(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_row_t *row, noah_profile_rgb_v1_error_t *error);

// One table's selectors, each row read only while the frame is current.
static bool read_table_selectors(const noah_active_profile_rgb_frame_t *frame, group_row_reader_t read_row, uint8_t count, uint16_t *next) {
    for (uint8_t index = 0u; index < count; index++) {
        const noah_profile_rgb_v1_view_t *view;
        noah_profile_rgb_v1_group_row_t   row;
        noah_profile_rgb_v1_error_t       error;

        COUNT_READ(selector_reads);
        if (frame_status(frame, &view) != NOAH_ACTIVE_PROFILE_RGB_OK || read_row(view, index, &row, &error) != NOAH_PROFILE_RGB_V1_OK) {
            return false;
        }
        selector_cache.selectors[(*next)++] = row.selector;
    }
    return true;
}

static bool read_group_selectors(const noah_active_profile_rgb_frame_t *frame) {
    const noah_profile_rgb_v1_view_t *view;
    uint16_t                          next  = 0u;
    uint8_t                           layer = rgb_active_profile_config_layer_group_count(frame);
    uint8_t                           pd    = rgb_active_profile_config_pd_group_count(frame);
    uint8_t                           key   = rgb_active_profile_config_key_group_count(frame);
    uint8_t                           autocorrect = rgb_active_profile_config_autocorrect_group_count(frame);

    if ((uint16_t)layer + pd + key + autocorrect > NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS || !read_table_selectors(frame, noah_profile_rgb_v1_layer_group_at, layer, &next) || !read_table_selectors(frame, noah_profile_rgb_v1_pd_group_at, pd, &next) || !read_table_selectors(frame, noah_profile_rgb_v1_key_group_at, key, &next) || !read_table_selectors(frame, noah_profile_rgb_v1_autocorrect_group_at, autocorrect, &next)) {
        return false;
    }
    selector_cache.layer_count       = layer;
    selector_cache.pd_count          = pd;
    selector_cache.key_count         = key;
    selector_cache.autocorrect_count = autocorrect;
    // A publication that landed during the reads leaves nothing to keep.
    return frame_status(frame, &view) == NOAH_ACTIVE_PROFILE_RGB_OK;
}

bool rgb_active_profile_config_group_selectors(const noah_active_profile_rgb_frame_t *frame, rgb_active_profile_group_selectors_t *selectors) {
    // Without a frame the accessors read the compiled view, as an uninstalled
    // runtime's frame does.
    const noah_profile_rgb_v1_view_t *view;
    uint32_t                          epoch             = frame ? frame->epoch : 0u;
    uint32_t                          publication_count = frame ? frame->publication_count : 0u;
    bool                              live              = frame && frame->live;

    if (!selectors || frame_status(frame, &view) != NOAH_ACTIVE_PROFILE_RGB_OK) {
        return false;
    }
    if (!(selector_cache.valid && selector_cache.epoch == epoch && selector_cache.publication_count == publication_count && selector_cache.live == live)) {
        selector_cache.valid = false;
        if (!read_group_selectors(frame)) {
            return false;
        }
        selector_cache.epoch             = epoch;
        selector_cache.publication_count = publication_count;
        selector_cache.live              = live;
        selector_cache.valid             = true;
    }
    *selectors = (rgb_active_profile_group_selectors_t){
        .layer       = selector_cache.selectors,
        .pd          = selector_cache.selectors + selector_cache.layer_count,
        .key         = selector_cache.selectors + selector_cache.layer_count + selector_cache.pd_count,
        .autocorrect = selector_cache.selectors + selector_cache.layer_count + selector_cache.pd_count + selector_cache.key_count,
        .layer_count = selector_cache.layer_count,
        .pd_count    = selector_cache.pd_count,
        .key_count   = selector_cache.key_count,
        .autocorrect_count = selector_cache.autocorrect_count,
    };
    return true;
}

void rgb_active_profile_config_invalidate_group_selectors(void) {
    selector_cache.valid = false;
}

#ifdef RGB_ACTIVE_PROFILE_CONFIG_TEST_BACKEND
void rgb_active_profile_config_test_reset_reads(void) {
    test_reads = (rgb_active_profile_config_test_reads_t){0};
}

rgb_active_profile_config_test_reads_t rgb_active_profile_config_test_reads(void) {
    return test_reads;
}
#endif
