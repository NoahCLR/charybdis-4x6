// ─────────────────────────────────────────────────────────────────────────
// Compiled Authored Defaults — Profile Wire v1.0
// ─────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_defaults_v1.h"

#include <string.h>

#include "key_behavior_domain_v1.h"
#include "profile_rgb_v1.h"
#include "../runtime/profile_action_runtime_v1.h"
#include "../storage/profile_checksum.h"
#include "noah_keymap_ids.h"
#include "lib/key/behavior/key_behavior.h"
#include "lib/pointing/defs/pd_modes.h"
#include "lib/rgb/core/rgb_helpers.h"

#if defined(RGB_MATRIX_ENABLE)
extern const layer_color_config_t layer_colors[LAYER_COUNT];
extern const layer_led_group_t *const layer_led_groups;
extern const uint8_t layer_led_group_count;

#    ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
extern const automouse_fade_end_config_t automouse_fade_end_config;
#    endif

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
extern const pd_mode_color_t pd_mode_colors[];
extern const uint8_t pd_mode_color_count;
extern const pd_mode_led_group_t *const pd_mode_led_groups;
extern const uint8_t pd_mode_led_group_count;
#    endif

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
extern const combo_feedback_color_config_t combo_feedback_colors;
extern const combo_feedback_led_group_t *const combo_feedback_led_groups;
extern const uint8_t combo_feedback_led_group_count;
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
extern const key_behavior_feedback_color_config_t key_behavior_feedback_colors;
extern const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups;
extern const uint8_t key_behavior_feedback_led_group_count;
#    endif
#endif

enum {
    RGB_GROUP_RECORD_SIZE       = 9u,
    RGB_LAYER_COLOR_RECORD_SIZE = 5u,
    RGB_GROUP_ROW_RECORD_SIZE   = 5u,
    RGB_AUTOMOUSE_RECORD_SIZE   = 4u,
    RGB_PD_COLOR_RECORD_SIZE    = 5u,
    RGB_COMBO_RECORD_SIZE       = 4u,
    RGB_COMBO_GROUP_RECORD_SIZE = 4u,
    RGB_COLOR_RECORD_SIZE       = 3u,
    RGB_KEY_FEEDBACK_SIZE       = 11u,
    RGB_FIXED_PAYLOAD_SIZE      = NOAH_PROFILE_RGB_V1_HEADER_SIZE + RGB_AUTOMOUSE_RECORD_SIZE + RGB_COMBO_RECORD_SIZE + RGB_KEY_FEEDBACK_SIZE,
};

typedef struct {
    noah_profile_compiled_v1_write_fn write;
    void                             *context;
    size_t                            offset;
    noah_profile_compiled_v1_result_t result;
    bool                              allow_early_stop;
    bool                              stopped;
} compiled_writer_t;

typedef struct {
    uint32_t crc32;
    uint32_t digest;
} checksum_sink_t;

typedef struct {
    size_t   start;
    size_t   end;
    uint8_t *target;
    size_t   copied;
    size_t   stream_offset;
} range_sink_t;

static noah_profile_compiled_v1_error_t no_error(void) {
    return (noah_profile_compiled_v1_error_t){
        .code    = NOAH_PROFILE_COMPILED_V1_OK,
        .surface = NOAH_PROFILE_COMPILED_V1_SURFACE_NONE,
        .row     = UINT8_MAX,
        .step    = UINT8_MAX,
    };
}

static noah_profile_compiled_v1_result_t fail(noah_profile_compiled_v1_error_t *error, noah_profile_compiled_v1_result_t code, noah_profile_compiled_v1_surface_t surface, uint8_t row, uint8_t step) {
    if (error) {
        *error = (noah_profile_compiled_v1_error_t){.code = code, .surface = surface, .row = row, .step = step};
    }
    return code;
}

static bool emit(compiled_writer_t *writer, const uint8_t *bytes, size_t length) {
    if (writer->result != NOAH_PROFILE_COMPILED_V1_OK || writer->stopped) {
        return false;
    }
    if (length != 0u && !writer->write(writer->context, bytes, length)) {
        if (writer->allow_early_stop) {
            writer->stopped = true;
            return false;
        }
        writer->result = NOAH_PROFILE_COMPILED_V1_WRITE_ERROR;
        return false;
    }
    writer->offset += length;
    return true;
}

static bool emit_u8(compiled_writer_t *writer, uint8_t value) {
    return emit(writer, &value, 1u);
}

static bool emit_u16(compiled_writer_t *writer, uint16_t value) {
    uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
    return emit(writer, bytes, sizeof(bytes));
}

static bool emit_u32(compiled_writer_t *writer, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u), (uint8_t)(value >> 16u), (uint8_t)(value >> 24u)};
    return emit(writer, bytes, sizeof(bytes));
}

static bool emit_action(compiled_writer_t *writer, const noah_profile_action_v1_t *action) {
    uint8_t bytes[NOAH_PROFILE_BLOB_V1_ACTION_SIZE] = {action->kind, action->flags, (uint8_t)action->operand, (uint8_t)(action->operand >> 8u)};
    return emit(writer, bytes, sizeof(bytes));
}

static bool checksum_write(void *context, const uint8_t *bytes, size_t length) {
    checksum_sink_t *sink = context;
    sink->crc32            = noah_profile_crc32_update(sink->crc32, bytes, length);
    sink->digest           = noah_profile_fnv1a_update(sink->digest, bytes, length);
    return true;
}

static bool range_write(void *context, const uint8_t *bytes, size_t length) {
    range_sink_t *sink        = context;
    size_t        chunk_start = sink->stream_offset;
    size_t        chunk_end   = chunk_start + length;
    size_t        copy_start  = chunk_start > sink->start ? chunk_start : sink->start;
    size_t        copy_end    = chunk_end < sink->end ? chunk_end : sink->end;

    if (copy_start < copy_end) {
        size_t amount = copy_end - copy_start;
        memcpy(&sink->target[sink->copied], &bytes[copy_start - chunk_start], amount);
        sink->copied += amount;
    }
    sink->stream_offset = chunk_end;
    return chunk_end < sink->end;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_action(uint16_t native_action, noah_profile_action_v1_t *action) {
    noah_profile_action_runtime_v1_result_t result = noah_profile_action_runtime_v1_from_native(native_action, action);

    if (result == NOAH_PROFILE_ACTION_RUNTIME_V1_OK) {
        return NOAH_PROFILE_COMPILED_V1_OK;
    }
    return result == NOAH_PROFILE_ACTION_RUNTIME_V1_INVALID_ARGUMENT ? NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT : NOAH_PROFILE_COMPILED_V1_INVALID_ACTION;
}

static int action_compare(noah_profile_action_v1_t left, noah_profile_action_v1_t right) {
    uint8_t left_bytes[4]  = {left.kind, left.flags, (uint8_t)left.operand, (uint8_t)(left.operand >> 8u)};
    uint8_t right_bytes[4] = {right.kind, right.flags, (uint8_t)right.operand, (uint8_t)(right.operand >> 8u)};
    return memcmp(left_bytes, right_bytes, sizeof(left_bytes));
}

static bool step_present(const key_behavior_step_t *step) {
    return step->tap.present || step->hold.present || step->long_hold.present;
}

static uint8_t behavior_step_count(const key_behavior_t *row) {
    uint8_t count = 0u;
    for (uint8_t index = 0u; index < KEY_BEHAVIOR_MAX_TAP_COUNT; index++) {
        if (step_present(&row->tap_counts[index])) count++;
    }
    return count;
}

static uint8_t behavior_step_mask(const key_behavior_step_t *step) {
    return (uint8_t)((step->tap.present ? NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP : 0u) | (step->hold.present ? NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD : 0u) | (step->long_hold.present ? NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD : 0u));
}

static size_t behavior_row_body_length(const key_behavior_t *row) {
    size_t length = NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FIXED_SIZE;
    for (uint8_t index = 0u; index < KEY_BEHAVIOR_MAX_TAP_COUNT; index++) {
        uint8_t mask;
        if (!step_present(&row->tap_counts[index])) continue;
        mask = behavior_step_mask(&row->tap_counts[index]);
        length += NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HEADER_SIZE;
        if ((mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP) != 0u) length += NOAH_PROFILE_BLOB_V1_ACTION_SIZE;
        if ((mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD) != 0u) length += NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE;
        if ((mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD) != 0u) length += NOAH_KEY_BEHAVIOR_DOMAIN_V1_HOLD_SIZE;
    }
    return length;
}

static const key_behavior_t *behavior_row_in_canonical_order(uint8_t wanted, noah_profile_compiled_v1_error_t *error) {
    for (uint8_t candidate = 0u; candidate < key_behavior_count; candidate++) {
        noah_profile_action_v1_t candidate_action;
        uint8_t                  rank = 0u;

        if (noah_profile_compiled_v1_action(key_behaviors[candidate].keycode, &candidate_action) != NOAH_PROFILE_COMPILED_V1_OK || candidate_action.kind == NOAH_PROFILE_ACTION_V1_NONE) {
            fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, candidate, UINT8_MAX);
            return NULL;
        }
        for (uint8_t other = 0u; other < key_behavior_count; other++) {
            noah_profile_action_v1_t other_action;
            int                      order;
            if (noah_profile_compiled_v1_action(key_behaviors[other].keycode, &other_action) != NOAH_PROFILE_COMPILED_V1_OK) {
                fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, other, UINT8_MAX);
                return NULL;
            }
            order = action_compare(other_action, candidate_action);
            if (order < 0) rank++;
            if (order == 0 && other != candidate) {
                fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, candidate, UINT8_MAX);
                return NULL;
            }
        }
        if (rank == wanted) return &key_behaviors[candidate];
    }
    fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, wanted, UINT8_MAX);
    return NULL;
}

static noah_profile_compiled_v1_result_t map_hold(const hold_behavior_t *source, noah_key_behavior_hold_v1_t *target, uint8_t row, uint8_t step, noah_profile_compiled_v1_error_t *error) {
    noah_profile_compiled_v1_result_t result;
    if (!source->present || !target) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, row, step);
    switch (source->mode) {
        case HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE: target->mode = NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE; break;
        case HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD: target->mode = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD; break;
        case HOLD_BEHAVIOR_REPEAT_WHILE_HELD: target->mode = NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD; break;
        case HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD: target->mode = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_ON_RELEASE_AFTER_HOLD; break;
        default: return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, row, step);
    }
    if (source->repeat_hz > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_REPEAT_HZ || (target->mode == NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD ? source->repeat_hz == 0u : source->repeat_hz != 0u)) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, row, step);
    }
    target->repeat_hz = (uint8_t)source->repeat_hz;
    result            = noah_profile_compiled_v1_action(source->action, &target->action);
    if (result != NOAH_PROFILE_COMPILED_V1_OK || target->action.kind == NOAH_PROFILE_ACTION_V1_NONE) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, row, step);
    }
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static bool emit_hold(compiled_writer_t *writer, const noah_key_behavior_hold_v1_t *hold) {
    return emit_u8(writer, hold->mode) && emit_u8(writer, hold->repeat_hz) && emit_action(writer, &hold->action);
}

static noah_profile_compiled_v1_result_t behavior_payload_size(size_t *length, uint8_t *populated_steps, noah_profile_compiled_v1_error_t *error) {
    size_t  total = NOAH_KEY_BEHAVIOR_DOMAIN_V1_HEADER_SIZE;
    uint8_t steps = 0u;
    if (!length || !populated_steps || key_behavior_count > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, UINT8_MAX, UINT8_MAX);
    }
    for (uint8_t row = 0u; row < key_behavior_count; row++) {
        size_t body = behavior_row_body_length(&key_behaviors[row]);
        uint8_t row_steps = behavior_step_count(&key_behaviors[row]);
        if ((uint16_t)steps + row_steps > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_POPULATED_STEPS || total > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_PAYLOAD_SIZE - NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE - body) {
            return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, row, UINT8_MAX);
        }
        steps = (uint8_t)(steps + row_steps);
        total += NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_LENGTH_SIZE + body;
    }
    *length          = total;
    *populated_steps = steps;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static noah_profile_compiled_v1_result_t write_behavior_payload(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    size_t  payload_length;
    uint8_t populated_steps;
    noah_profile_compiled_v1_result_t result = behavior_payload_size(&payload_length, &populated_steps, error);
    (void)payload_length;
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (!(emit_u8(writer, key_behavior_count) && emit_u8(writer, populated_steps) && emit_u16(writer, 0u))) return writer->result;

    for (uint8_t order = 0u; order < key_behavior_count; order++) {
        const key_behavior_t *row = behavior_row_in_canonical_order(order, error);
        noah_profile_action_v1_t target;
        uint8_t source_row = row ? (uint8_t)(row - key_behaviors) : UINT8_MAX;
        if (!row) return error ? error->code : NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR;
        result = noah_profile_compiled_v1_action(row->keycode, &target);
        if (result != NOAH_PROFILE_COMPILED_V1_OK || target.kind == NOAH_PROFILE_ACTION_V1_NONE) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, source_row, UINT8_MAX);
        if (!(emit_u16(writer, (uint16_t)behavior_row_body_length(row)) && emit_action(writer, &target) && emit_u16(writer, row->tap_hold_term) && emit_u16(writer, row->longer_hold_term) && emit_u16(writer, row->multi_tap_term) && emit_u8(writer, row->keeps_auto_mouse_anchored ? NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_AUTO_MOUSE : 0u) && emit_u8(writer, behavior_step_count(row)))) return writer->result;

        for (uint8_t step_index = 0u; step_index < KEY_BEHAVIOR_MAX_TAP_COUNT; step_index++) {
            const key_behavior_step_t *step = &row->tap_counts[step_index];
            uint8_t mask;
            if (!step_present(step)) continue;
            mask = behavior_step_mask(step);
            if (!(emit_u8(writer, step_index) && emit_u8(writer, mask))) return writer->result;
            if (step->tap.present) {
                noah_profile_action_v1_t action;
                result = noah_profile_compiled_v1_action(step->tap.action, &action);
                if (result != NOAH_PROFILE_COMPILED_V1_OK || action.kind == NOAH_PROFILE_ACTION_V1_NONE) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, source_row, step_index);
                if (!emit_action(writer, &action)) return writer->result;
            }
            if (step->hold.present) {
                noah_key_behavior_hold_v1_t hold;
                result = map_hold(&step->hold, &hold, source_row, step_index, error);
                if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
                if (!emit_hold(writer, &hold)) return writer->result;
            }
            if (step->long_hold.present) {
                noah_key_behavior_hold_v1_t hold;
                result = map_hold(&step->long_hold, &hold, source_row, step_index, error);
                if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
                if (!emit_hold(writer, &hold)) return writer->result;
            }
        }
    }
    return writer->result;
}

#if defined(RGB_MATRIX_ENABLE)
static uint16_t rgb_stage_mask(void) {
    uint16_t mask = NOAH_PROFILE_RGB_V1_STAGE_LAYER;
#    ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
    mask |= NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE;
#    endif
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    mask |= NOAH_PROFILE_RGB_V1_STAGE_PD_MODE;
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    mask |= NOAH_PROFILE_RGB_V1_STAGE_COMBO;
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    mask |= NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR;
#    endif
    return mask;
}

static uint8_t rgb_group_row_count(void) {
    uint16_t count = layer_led_group_count;
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    count += pd_mode_led_group_count;
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    count += combo_feedback_led_group_count;
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    count += key_behavior_feedback_led_group_count;
#    endif
    return count <= UINT8_MAX ? (uint8_t)count : UINT8_MAX;
}

static const rgb_led_group_t *rgb_group_row_at(uint8_t index) {
    if (index < layer_led_group_count) return &layer_led_groups[index].led_group;
    index = (uint8_t)(index - layer_led_group_count);
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    if (index < pd_mode_led_group_count) return &pd_mode_led_groups[index].led_group;
    index = (uint8_t)(index - pd_mode_led_group_count);
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    if (index < combo_feedback_led_group_count) return &combo_feedback_led_groups[index].led_group;
    index = (uint8_t)(index - combo_feedback_led_group_count);
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    if (index < key_behavior_feedback_led_group_count) return &key_behavior_feedback_led_groups[index].led_group;
#    endif
    return NULL;
}

static bool rgb_group_bitmap(const rgb_led_group_t *group, uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE]) {
    if (!group || group->count == 0u || group->count > NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT) return false;
    memset(bitmap, 0, NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE);
    for (uint8_t index = 0u; index < group->count; index++) {
        uint8_t led = group->leds[index];
        if (led >= NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT || (bitmap[led >> 3u] & (uint8_t)(1u << (led & 7u))) != 0u) return false;
        bitmap[led >> 3u] |= (uint8_t)(1u << (led & 7u));
    }
    return true;
}

static bool rgb_group_is_first(uint8_t index, const uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE]) {
    for (uint8_t prior = 0u; prior < index; prior++) {
        uint8_t prior_bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
        if (rgb_group_bitmap(rgb_group_row_at(prior), prior_bitmap) && memcmp(bitmap, prior_bitmap, sizeof(prior_bitmap)) == 0) return false;
    }
    return true;
}

static noah_profile_compiled_v1_result_t rgb_group_count(uint8_t *count, noah_profile_compiled_v1_error_t *error) {
    uint8_t rows = rgb_group_row_count();
    uint8_t unique = 0u;
    if (!count || rows > NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    for (uint8_t index = 0u; index < rows; index++) {
        uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
        if (!rgb_group_bitmap(rgb_group_row_at(index), bitmap)) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, index, UINT8_MAX);
        if (rgb_group_is_first(index, bitmap)) unique++;
    }
    if (unique > NOAH_PROFILE_RGB_V1_MAX_GROUPS) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    *count = unique;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static noah_profile_compiled_v1_result_t rgb_group_id(const rgb_led_group_t *group, uint8_t *id, noah_profile_compiled_v1_error_t *error) {
    uint8_t target[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
    uint8_t rank = 0u;
    uint8_t rows = rgb_group_row_count();
    if (!id || !rgb_group_bitmap(group, target)) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    for (uint8_t index = 0u; index < rows; index++) {
        uint8_t candidate[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
        if (!rgb_group_bitmap(rgb_group_row_at(index), candidate)) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, index, UINT8_MAX);
        if (rgb_group_is_first(index, candidate) && memcmp(candidate, target, sizeof(candidate)) < 0) rank++;
    }
    *id = rank;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static noah_profile_compiled_v1_result_t rgb_group_bitmap_for_id(uint8_t id, uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE], noah_profile_compiled_v1_error_t *error) {
    uint8_t rows = rgb_group_row_count();
    for (uint8_t index = 0u; index < rows; index++) {
        uint8_t candidate[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
        uint8_t candidate_id;
        if (!rgb_group_bitmap(rgb_group_row_at(index), candidate)) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, index, UINT8_MAX);
        if (!rgb_group_is_first(index, candidate)) continue;
        noah_profile_compiled_v1_result_t result = rgb_group_id(rgb_group_row_at(index), &candidate_id, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
        if (candidate_id == id) {
            memcpy(bitmap, candidate, sizeof(candidate));
            return NOAH_PROFILE_COMPILED_V1_OK;
        }
    }
    return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, id, UINT8_MAX);
}

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
static uint8_t pd_id_from_mask(pd_mode_mask_t mask) {
    return pd_mode_id_from_mask(mask);
}

static const pd_mode_color_t *pd_color_for_id(uint8_t id) {
    for (uint8_t index = 0u; index < pd_mode_color_count; index++) {
        if (pd_id_from_mask(pd_mode_colors[index].pointing_mode) == id) return &pd_mode_colors[index];
    }
    return NULL;
}
#    endif

static bool emit_hsv(compiled_writer_t *writer, hsv_t color) {
    uint8_t bytes[3] = {color.h, color.s, color.v};
    return emit(writer, bytes, sizeof(bytes));
}

static noah_profile_compiled_v1_result_t rgb_payload_size(size_t *length, uint8_t *groups, noah_profile_compiled_v1_error_t *error) {
    uint16_t group_rows = layer_led_group_count;
    size_t   total;
    noah_profile_compiled_v1_result_t result;
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    group_rows += pd_mode_led_group_count;
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    group_rows += combo_feedback_led_group_count;
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    group_rows += key_behavior_feedback_led_group_count;
#    endif
    if (!length || !groups || (uint32_t)LAYER_COUNT > (uint32_t)NOAH_PROFILE_RGB_V1_MAX_LOGICAL_LAYERS || group_rows > NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    result = rgb_group_count(groups, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    total = RGB_FIXED_PAYLOAD_SIZE + (size_t)*groups * RGB_GROUP_RECORD_SIZE + (size_t)LAYER_COUNT * RGB_LAYER_COLOR_RECORD_SIZE + (size_t)layer_led_group_count * RGB_GROUP_ROW_RECORD_SIZE;
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    if (pd_mode_color_count != PD_MODE_COUNT) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    total += (size_t)pd_mode_color_count * RGB_PD_COLOR_RECORD_SIZE + (size_t)pd_mode_led_group_count * RGB_GROUP_ROW_RECORD_SIZE;
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    total += (size_t)combo_feedback_led_group_count * RGB_COMBO_GROUP_RECORD_SIZE;
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    if (key_behavior_feedback_colors.tap_branch_color_count != KEY_BEHAVIOR_MAX_TAP_COUNT - 1u) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    total += (size_t)key_behavior_feedback_colors.tap_branch_color_count * RGB_COLOR_RECORD_SIZE + (size_t)key_behavior_feedback_led_group_count * RGB_GROUP_ROW_RECORD_SIZE;
#    endif
    if (total > NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    *length = total;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static noah_profile_compiled_v1_result_t emit_group_row(compiled_writer_t *writer, uint8_t selector, hsv_t color, const rgb_led_group_t *group, noah_profile_compiled_v1_error_t *error) {
    uint8_t id;
    noah_profile_compiled_v1_result_t result = rgb_group_id(group, &id, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (!(emit_u8(writer, selector) && emit_hsv(writer, color) && emit_u8(writer, id))) return writer->result;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static noah_profile_compiled_v1_result_t write_rgb_payload(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    size_t payload_length;
    uint8_t group_count;
    noah_profile_compiled_v1_result_t result = rgb_payload_size(&payload_length, &group_count, error);
    (void)payload_length;
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (!(emit_u8(writer, NOAH_PROFILE_RGB_V1_FORMAT_VERSION) && emit_u8(writer, 0u) && emit_u16(writer, rgb_stage_mask()) && emit_u8(writer, group_count) && emit_u8(writer, LAYER_COUNT) && emit_u8(writer, layer_led_group_count)
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
          && emit_u8(writer, pd_mode_color_count) && emit_u8(writer, pd_mode_led_group_count)
#    else
          && emit_u8(writer, 0u) && emit_u8(writer, 0u)
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
          && emit_u8(writer, combo_feedback_led_group_count)
#    else
          && emit_u8(writer, 0u)
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
          && emit_u8(writer, key_behavior_feedback_colors.tap_branch_color_count) && emit_u8(writer, key_behavior_feedback_led_group_count)
#    else
          && emit_u8(writer, 0u) && emit_u8(writer, 0u)
#    endif
          && emit_u8(writer, NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT) && emit_u8(writer, NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE) && emit_u16(writer, 0u))) return writer->result;

    for (uint8_t id = 0u; id < group_count; id++) {
        uint8_t bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
        result = rgb_group_bitmap_for_id(id, bitmap, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
        if (!(emit_u8(writer, id) && emit(writer, bitmap, sizeof(bitmap)))) return writer->result;
    }
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        if (!(emit_u8(writer, layer) && emit_hsv(writer, layer_colors[layer].color) && emit_u8(writer, layer_colors[layer].mode))) return writer->result;
    }
    for (uint8_t index = 0u; index < layer_led_group_count; index++) {
        uint8_t selector = layer_led_groups[index].layer == RGB_LAYER_GROUP_ALL ? NOAH_PROFILE_RGB_V1_SELECTOR_ALL : layer_led_groups[index].layer;
        if (selector != NOAH_PROFILE_RGB_V1_SELECTOR_ALL && selector >= LAYER_COUNT) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, index, UINT8_MAX);
        result = emit_group_row(writer, selector, layer_led_groups[index].color, &layer_led_groups[index].led_group, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    }
#    ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
    if (!(emit_u8(writer, automouse_fade_end_config.mode) && emit_hsv(writer, automouse_fade_end_config.end_color))) return writer->result;
#    else
    if (!(emit_u8(writer, 0u) && emit_u8(writer, 0u) && emit_u8(writer, 0u) && emit_u8(writer, 0u))) return writer->result;
#    endif
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    for (uint8_t id = 0u; id < PD_MODE_COUNT; id++) {
        const pd_mode_color_t *color = pd_color_for_id(id);
        if (!color || !(emit_u8(writer, id) && emit_hsv(writer, color->color) && emit_u8(writer, color->locality))) return color ? writer->result : fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, id, UINT8_MAX);
    }
    for (uint8_t index = 0u; index < pd_mode_led_group_count; index++) {
        uint8_t selector = pd_mode_led_groups[index].pointing_mode == RGB_PD_MODE_GROUP_ALL ? NOAH_PROFILE_RGB_V1_SELECTOR_ALL : pd_id_from_mask(pd_mode_led_groups[index].pointing_mode);
        if (selector != NOAH_PROFILE_RGB_V1_SELECTOR_ALL && selector >= PD_MODE_COUNT) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, index, UINT8_MAX);
        result = emit_group_row(writer, selector, pd_mode_led_groups[index].color, &pd_mode_led_groups[index].led_group, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    }
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    if (!(emit_hsv(writer, combo_feedback_colors.color) && emit_u8(writer, combo_feedback_colors.locality))) return writer->result;
    for (uint8_t index = 0u; index < combo_feedback_led_group_count; index++) {
        uint8_t id;
        result = rgb_group_id(&combo_feedback_led_groups[index].led_group, &id, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
        if (!(emit_hsv(writer, combo_feedback_led_groups[index].color) && emit_u8(writer, id))) return writer->result;
    }
#    else
    if (!(emit_u8(writer, 0u) && emit_u8(writer, 0u) && emit_u8(writer, 0u) && emit_u8(writer, 0u))) return writer->result;
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    for (uint8_t index = 0u; index < key_behavior_feedback_colors.tap_branch_color_count; index++) {
        if (!emit_hsv(writer, key_behavior_feedback_colors.tap_branch_colors[index])) return writer->result;
    }
    if (!(emit_hsv(writer, key_behavior_feedback_colors.tap_committed_color) && emit_hsv(writer, key_behavior_feedback_colors.hold_active_color) && emit_hsv(writer, key_behavior_feedback_colors.long_hold_active_color) && emit_u8(writer, key_behavior_feedback_colors.tap_commit_mode) && emit_u8(writer, key_behavior_feedback_colors.locality))) return writer->result;
    for (uint8_t index = 0u; index < key_behavior_feedback_led_group_count; index++) {
        uint8_t selector = key_behavior_feedback_led_groups[index].semantic == KEY_FEEDBACK_GROUP_ALL ? NOAH_PROFILE_RGB_V1_SELECTOR_ALL : key_behavior_feedback_led_groups[index].semantic;
        result = emit_group_row(writer, selector, key_behavior_feedback_led_groups[index].color, &key_behavior_feedback_led_groups[index].led_group, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    }
#    else
    for (uint8_t index = 0u; index < RGB_KEY_FEEDBACK_SIZE; index++) {
        if (!emit_u8(writer, 0u)) return writer->result;
    }
#    endif
    return writer->result;
}
#endif

static noah_profile_compiled_v1_result_t action_abi_digest(uint32_t *digest, uint16_t *row_visits, noah_profile_compiled_v1_error_t *error) {
    checksum_sink_t  sink = {.crc32 = NOAH_PROFILE_CRC32_INITIAL, .digest = NOAH_PROFILE_FNV1A_INITIAL};
    compiled_writer_t writer = {.write = checksum_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK};
    static const uint8_t magic[4] = {'N', 'L', 'A', '1'};
    uint8_t custom_count = 0u;

    if (!digest || !row_visits) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    if (key_behavior_count > NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    *digest     = 0u;
    *row_visits = 0u;

    emit(&writer, magic, sizeof(magic));
    emit_u8(&writer, NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR);
    emit_u8(&writer, NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR);
    emit_u8(&writer, NOAH_PROFILE_ACTION_V1_HARDCODED_MACRO);
    emit_u8(&writer, LAYER_COUNT);
    emit_u8(&writer, PD_MODE_COUNT);
    emit_u8(&writer, VIA_MACRO_SLOT_COUNT);
    emit_u8(&writer, HARDCODED_MACRO_SLOT_COUNT);
#ifdef VIA_FIRMWARE_VERSION
    emit_u32(&writer, VIA_FIRMWARE_VERSION);
#else
    emit_u32(&writer, 0u);
#endif
    emit_u16(&writer, KC_NO);
    emit_u16(&writer, KC_TRNS);
    emit_u16(&writer, MO(0));
    emit_u16(&writer, LT(0, KC_NO));
    emit_u16(&writer, OSM(0));
    emit_u16(&writer, MT(0, KC_NO));
    emit_u16(&writer, QK_MACRO_0);
    emit_u16(&writer, MACRO_0);
    emit_u16(&writer, LAYER_LOCK_BASE);
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        emit_u8(&writer, layer);
        emit_u16(&writer, MO(layer));
        emit_u16(&writer, LOCK_LAYER(layer));
    }
    for (uint8_t id = 0u; id < PD_MODE_COUNT; id++) {
        emit_u8(&writer, id);
        emit_u16(&writer, pd_modes[id].mode_flag);
        emit_u16(&writer, pd_modes[id].keycode);
        emit_u16(&writer, pd_modes[id].lock_action);
    }
    for (uint8_t row = 0u; row < key_behavior_count; row++) {
        (*row_visits)++;
        if (key_behaviors[row].keycode >= NOAH_KEYMAP_SAFE_RANGE) custom_count++;
    }
    emit_u8(&writer, custom_count);
    uint16_t previous_custom = 0u;
    bool     have_previous_custom = false;
    for (uint8_t position = 0u; position < custom_count; position++) {
        uint16_t next_custom = UINT16_MAX;
        bool     found       = false;
        for (uint8_t row = 0u; row < key_behavior_count; row++) {
            uint16_t keycode = key_behaviors[row].keycode;
            (*row_visits)++;
            if (keycode < NOAH_KEYMAP_SAFE_RANGE || (have_previous_custom && keycode <= previous_custom)) continue;
            if (!found || keycode < next_custom) {
                next_custom = keycode;
                found       = true;
            }
        }
        if (!found) {
            return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
        }
        emit_u16(&writer, next_custom);
        previous_custom      = next_custom;
        have_previous_custom = true;
    }
    if (writer.result != NOAH_PROFILE_COMPILED_V1_OK) {
        return fail(error, writer.result, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    if (*row_visits > NOAH_PROFILE_COMPILED_V1_ACTION_ABI_ROW_VISITS_MAX) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    *digest = sink.digest;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static noah_profile_compiled_v1_result_t write_blob(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    static const uint8_t magic[4] = {'N', 'L', 'P', '1'};
    size_t behavior_length;
    uint8_t behavior_steps;
    noah_profile_compiled_v1_result_t result;
#if defined(RGB_MATRIX_ENABLE)
    size_t rgb_length;
    uint8_t rgb_groups;
#endif

    result = behavior_payload_size(&behavior_length, &behavior_steps, error);
    (void)behavior_steps;
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
#if defined(RGB_MATRIX_ENABLE)
    result = rgb_payload_size(&rgb_length, &rgb_groups, error);
    (void)rgb_groups;
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (NOAH_PROFILE_BLOB_V1_HEADER_SIZE + 2u * NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + rgb_length + behavior_length > NOAH_PROFILE_BLOB_V1_MAX_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
#else
    if (NOAH_PROFILE_BLOB_V1_HEADER_SIZE + NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE + behavior_length > NOAH_PROFILE_BLOB_V1_MAX_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
#endif

    if (!(emit(writer, magic, sizeof(magic)) && emit_u8(writer, NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR) && emit_u8(writer, NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR)
#if defined(RGB_MATRIX_ENABLE)
          && emit_u8(writer, 2u)
#else
          && emit_u8(writer, 1u)
#endif
          && emit_u8(writer, NOAH_PROFILE_BLOB_V1_CANONICAL_FLAG))) return writer->result;
#if defined(RGB_MATRIX_ENABLE)
    if (!(emit_u8(writer, NOAH_PROFILE_DOMAIN_V1_RGB) && emit_u8(writer, NOAH_PROFILE_DOMAIN_V1_RGB_VERSION) && emit_u16(writer, (uint16_t)rgb_length))) return writer->result;
    result = write_rgb_payload(writer, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
#endif
    if (!(emit_u8(writer, NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS) && emit_u8(writer, NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIOR_VERSION) && emit_u16(writer, (uint16_t)behavior_length))) return writer->result;
    return write_behavior_payload(writer, error);
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_open(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    checksum_sink_t   sink = {.crc32 = NOAH_PROFILE_CRC32_INITIAL, .digest = NOAH_PROFILE_FNV1A_INITIAL};
    compiled_writer_t writer = {.write = checksum_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK};
    noah_profile_compiled_v1_result_t result;
    uint32_t action_abi;
    uint16_t action_abi_row_visits;
    uint8_t  domain_mask = NOAH_PROFILE_COMPILED_V1_DOMAIN_MASK_KEY_BEHAVIORS;

    if (error) *error = no_error();
    if (!profile) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    memset(profile, 0, sizeof(*profile));
    result = action_abi_digest(&action_abi, &action_abi_row_visits, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    result = write_blob(&writer, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (writer.offset > UINT16_MAX || writer.offset > NOAH_PROFILE_BLOB_V1_MAX_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
#if defined(RGB_MATRIX_ENABLE)
    domain_mask |= NOAH_PROFILE_COMPILED_V1_DOMAIN_MASK_RGB;
#endif
    profile->metadata = (noah_profile_compiled_v1_metadata_t){
        .byte_length      = (uint16_t)writer.offset,
        .crc32            = noah_profile_crc32_finish(sink.crc32),
        .digest           = sink.digest,
        .action_abi_digest = action_abi,
        .action_abi_row_visits = action_abi_row_visits,
        .domain_mask      = domain_mask,
    };
    return NOAH_PROFILE_COMPILED_V1_OK;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_write(const noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_write_fn write, void *context, noah_profile_compiled_v1_error_t *error) {
    compiled_writer_t writer = {.write = write, .context = context, .result = NOAH_PROFILE_COMPILED_V1_OK};
    noah_profile_compiled_v1_result_t result;
    if (error) *error = no_error();
    if (!profile || !write || profile->metadata.byte_length == 0u) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    result = write_blob(&writer, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (writer.offset != profile->metadata.byte_length) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static bool compiled_reader_read(void *context, size_t offset, uint8_t *target, size_t length) {
    const noah_profile_compiled_v1_t *profile = context;
    range_sink_t sink = {.start = offset, .end = offset + length, .target = target};
    noah_profile_compiled_v1_error_t error;
    compiled_writer_t writer = {.write = range_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK, .allow_early_stop = true};
    return profile && write_blob(&writer, &error) == NOAH_PROFILE_COMPILED_V1_OK && sink.copied == length;
}

noah_profile_reader_t noah_profile_compiled_v1_reader(const noah_profile_compiled_v1_t *profile) {
    return (noah_profile_reader_t){
        .read    = profile && profile->metadata.byte_length != 0u ? compiled_reader_read : NULL,
        .context = (void *)profile,
        .length  = profile ? profile->metadata.byte_length : 0u,
    };
}

#ifdef COMBO_ENABLE
static bool combo_to_native(const noah_profile_action_v1_t *action, uint16_t *native) {
    return noah_profile_action_runtime_v1_to_native(action, native) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK;
}
#endif

bool noah_profile_compiled_v1_compatibility(const noah_profile_compiled_v1_t *profile, noah_profile_validator_v1_compatibility_t *compatibility) {
    noah_profile_validator_v1_compatibility_t result;

    if (!profile || !compatibility || profile->metadata.domain_mask == 0u || (profile->metadata.domain_mask & (uint8_t)~NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS) != 0u) {
        return false;
    }
    result                              = noah_profile_validator_v1_default_compatibility(profile->metadata.action_abi_digest);
    result.allowed_domain_mask          = profile->metadata.domain_mask;
#ifdef COMBO_ENABLE
    result.allowed_domain_mask |= NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS;
    result.combo_to_native = combo_to_native;
#endif
    result.required_domain_mask         = 0u;
    result.logical_layer_count          = LAYER_COUNT;
    result.supported_pd_mode_mask       = (uint8_t)((UINT32_C(1) << PD_MODE_COUNT) - 1u);
    result.via_macro_slot_count         = VIA_MACRO_SLOT_COUNT;
    result.hardcoded_macro_slot_count   = HARDCODED_MACRO_SLOT_COUNT;
    result.rgb_limits.logical_layer_count     = LAYER_COUNT;
    result.rgb_limits.supported_pd_mode_mask  = result.supported_pd_mode_mask;
    result.rgb_limits.tap_branch_color_count  = KEY_BEHAVIOR_MAX_TAP_COUNT - 1u;
#if defined(RGB_MATRIX_ENABLE)
    result.rgb_limits.maximum_brightness = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    result.rgb_limits.compiled_stage_mask = rgb_stage_mask();
#else
    result.rgb_limits.compiled_stage_mask = 0u;
#endif
    *compatibility = result;
    return true;
}

_Static_assert(sizeof(noah_profile_compiled_v1_t) <= 20u, "compiled-profile handle must remain metadata-only");
