#include "profile_rgb_compiled_v1.h"
#include "profile_compiled_writer.h"
#include "noah_keymap_ids.h"
#include "lib/rgb/core/rgb_helpers.h"

#if defined(RGB_MATRIX_ENABLE)
extern const layer_color_config_t                  layer_colors[LAYER_COUNT];
__attribute__((weak)) const rgb_led_group_t *const saved_led_groups      = NULL;
__attribute__((weak)) const uint8_t                saved_led_group_count = 0;
extern const layer_led_group_t *const              layer_led_groups;
extern const uint8_t                               layer_led_group_count;

#    ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
extern const automouse_fade_end_config_t automouse_fade_end_config;
#    endif

#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
extern const pd_mode_color_t            pd_mode_colors[];
extern const uint8_t                    pd_mode_color_count;
extern const pd_mode_led_group_t *const pd_mode_led_groups;
extern const uint8_t                    pd_mode_led_group_count;
#    endif

#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
extern const combo_feedback_color_config_t     combo_feedback_colors;
extern const combo_feedback_led_group_t *const combo_feedback_led_groups;
extern const uint8_t                           combo_feedback_led_group_count;
#    endif

#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
extern const key_behavior_feedback_color_config_t     key_behavior_feedback_colors;
extern const key_behavior_feedback_led_group_t *const key_behavior_feedback_led_groups;
extern const uint8_t                                  key_behavior_feedback_led_group_count;
#    endif
#endif
#if defined(RGB_MATRIX_ENABLE)
uint16_t noah_profile_rgb_compiled_v1_stage_mask(void) {
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
    uint16_t count = saved_led_group_count + layer_led_group_count;
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

// Saved groups come first; they are groups only, painted by no stage row.
static const rgb_led_group_t *rgb_group_row_at(uint8_t index) {
    if (index < saved_led_group_count) return &saved_led_groups[index];
    index = (uint8_t)(index - saved_led_group_count);
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
    uint8_t rows   = rgb_group_row_count();
    uint8_t unique = 0u;
    // Saved groups are groups, not stage rows: each limit bounds its own rows.
    if (!count || saved_led_group_count > NOAH_PROFILE_RGB_V1_MAX_GROUPS || rows - saved_led_group_count > NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
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

noah_profile_compiled_v1_result_t noah_profile_rgb_compiled_v1_length(size_t *length, uint8_t *groups, noah_profile_compiled_v1_error_t *error) {
    uint16_t                          group_rows = layer_led_group_count;
    size_t                            total;
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
    total = NOAH_PROFILE_RGB_V1_FIXED_PAYLOAD_SIZE + (size_t)*groups * NOAH_PROFILE_RGB_V1_GROUP_RECORD_SIZE + (size_t)LAYER_COUNT * NOAH_PROFILE_RGB_V1_LAYER_COLOR_RECORD_SIZE + (size_t)layer_led_group_count * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE;
#    if defined(POINTING_DEVICE_ENABLE) && defined(RGB_PD_MODE_FEEDBACK_ENABLE)
    if (pd_mode_color_count != PD_MODE_COUNT) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    total += (size_t)pd_mode_color_count * NOAH_PROFILE_RGB_V1_PD_COLOR_RECORD_SIZE + (size_t)pd_mode_led_group_count * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE;
#    endif
#    if defined(COMBO_ENABLE) && defined(RGB_COMBO_FEEDBACK_ENABLE)
    total += (size_t)combo_feedback_led_group_count * NOAH_PROFILE_RGB_V1_COMBO_GROUP_RECORD_SIZE;
#    endif
#    ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
    if (key_behavior_feedback_colors.tap_branch_color_count != KEY_BEHAVIOR_MAX_TAP_COUNT - 1u) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_RGB, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    total += (size_t)key_behavior_feedback_colors.tap_branch_color_count * NOAH_PROFILE_RGB_V1_COLOR_RECORD_SIZE + (size_t)key_behavior_feedback_led_group_count * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE;
#    endif
    if (total > NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_RGB, UINT8_MAX, UINT8_MAX);
    *length = total;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static noah_profile_compiled_v1_result_t emit_group_row(compiled_writer_t *writer, uint8_t selector, hsv_t color, const rgb_led_group_t *group, noah_profile_compiled_v1_error_t *error) {
    uint8_t                           id;
    noah_profile_compiled_v1_result_t result = rgb_group_id(group, &id, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (!(emit_u8(writer, selector) && emit_hsv(writer, color) && emit_u8(writer, id))) return writer->result;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

noah_profile_compiled_v1_result_t noah_profile_rgb_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
#    ifdef NOAH_COMPILED_DEFAULTS_TEST
    extern void noah_compiled_defaults_test_domain_write(uint8_t id);
    noah_compiled_defaults_test_domain_write(NOAH_PROFILE_DOMAIN_V1_RGB);
#    endif
    size_t                            payload_length;
    uint8_t                           group_count;
    noah_profile_compiled_v1_result_t result = noah_profile_rgb_compiled_v1_length(&payload_length, &group_count, error);
    (void)payload_length;
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (!(emit_u8(writer, NOAH_PROFILE_RGB_V1_FORMAT_VERSION) && emit_u8(writer, 0u) && emit_u16(writer, noah_profile_rgb_compiled_v1_stage_mask()) && emit_u8(writer, group_count) && emit_u8(writer, LAYER_COUNT) && emit_u8(writer, layer_led_group_count)
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
          && emit_u8(writer, NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT) && emit_u8(writer, NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE) && emit_u16(writer, 0u)))
        return writer->result;

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
        result           = emit_group_row(writer, selector, key_behavior_feedback_led_groups[index].color, &key_behavior_feedback_led_groups[index].led_group, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    }
#    else
    for (uint8_t index = 0u; index < NOAH_PROFILE_RGB_V1_KEY_FEEDBACK_SIZE; index++) {
        if (!emit_u8(writer, 0u)) return writer->result;
    }
#    endif
    return writer->result;
}
#endif

#if defined(RGB_MATRIX_ENABLE)
// Maximum canonical RGB geometry: fixed records, sixteen bitmap groups,
// eight layer colors, 32 PD colors, 32 stage rows total, four tap colors.
// This is a structural bound, not a profile-sized buffer or a RAM claim.
enum { RGB_DEFAULTS_CACHE_SIZE = NOAH_PROFILE_RGB_V1_FIXED_PAYLOAD_SIZE + NOAH_PROFILE_RGB_V1_MAX_GROUPS * NOAH_PROFILE_RGB_V1_GROUP_RECORD_SIZE + NOAH_PROFILE_RGB_V1_MAX_LOGICAL_LAYERS * NOAH_PROFILE_RGB_V1_LAYER_COLOR_RECORD_SIZE + NOAH_PROFILE_RGB_V1_MAX_PD_MODES * NOAH_PROFILE_RGB_V1_PD_COLOR_RECORD_SIZE + NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS * NOAH_PROFILE_RGB_V1_GROUP_ROW_RECORD_SIZE + NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS * NOAH_PROFILE_RGB_V1_COLOR_RECORD_SIZE };
static uint8_t                    cached_bytes[RGB_DEFAULTS_CACHE_SIZE];
static noah_profile_rgb_v1_view_t cached_view;
static bool                       cached;
typedef struct {
    size_t length;
} cache_sink_t;
static bool cache_write(void *context, const uint8_t *bytes, size_t length) {
    cache_sink_t *sink = context;
    if (length > sizeof(cached_bytes) - sink->length) return false;
    memcpy(cached_bytes + sink->length, bytes, length);
    sink->length += length;
    return true;
}
const noah_profile_rgb_v1_view_t *noah_profile_rgb_compiled_v1_view(void) {
    if (!cached) {
        cache_sink_t                 sink   = {0};
        compiled_writer_t            writer = {.write = cache_write, .context = &sink};
        noah_profile_rgb_v1_limits_t limits = noah_profile_rgb_v1_default_limits();
        limits.logical_layer_count          = LAYER_COUNT;
        limits.supported_pd_mode_mask       = UINT32_MAX >> (32u - PD_MODE_COUNT);
#    ifdef RGB_MATRIX_MAXIMUM_BRIGHTNESS
        limits.maximum_brightness = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
#    endif
        limits.tap_branch_color_count = KEY_BEHAVIOR_MAX_TAP_COUNT - 1u;
        limits.compiled_stage_mask    = noah_profile_rgb_compiled_v1_stage_mask();
        if (noah_profile_rgb_compiled_v1_write(&writer, NULL) != NOAH_PROFILE_COMPILED_V1_OK || noah_profile_rgb_v1_decode(cached_bytes, sink.length, &limits, &cached_view, NULL) != NOAH_PROFILE_RGB_V1_OK) return NULL;
        cached = true;
    }
    return &cached_view;
}
#endif
