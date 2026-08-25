// ────────────────────────────────────────────────────────────────────────────
// Canonical RGB Domain — Profile Wire v1.0
// ────────────────────────────────────────────────────────────────────────────

#include "profile_rgb_v1.h"

#include <stdbool.h>
#include <string.h>

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_profile_rgb_v1_view_t) <= NOAH_PROFILE_RGB_V1_EMBEDDED_VIEW_BUDGET, "RGB reader-backed view exceeded its 32-bit firmware RAM budget");
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
    RGB_FIXED_PAYLOAD_SIZE      = 35u,
};

typedef enum {
    RGB_SECTION_GROUPS = 0u,
    RGB_SECTION_LAYER_COLORS,
    RGB_SECTION_LAYER_GROUPS,
    RGB_SECTION_AUTOMOUSE,
    RGB_SECTION_PD_COLORS,
    RGB_SECTION_PD_GROUPS,
    RGB_SECTION_COMBO,
    RGB_SECTION_COMBO_GROUPS,
    RGB_SECTION_TAP_BRANCH_COLORS,
    RGB_SECTION_KEY_FEEDBACK,
    RGB_SECTION_KEY_GROUPS,
    RGB_SECTION_END,
} rgb_section_t;

static uint16_t read_u16(const uint8_t bytes[2]) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u);
}

static void clear_error(noah_profile_rgb_v1_error_t *error) {
    if (!error) {
        return;
    }
    memset(error, 0, sizeof(*error));
    error->row   = UINT8_MAX;
    error->field = UINT8_MAX;
}

static noah_profile_rgb_v1_result_t fail(noah_profile_rgb_v1_error_t *error, noah_profile_rgb_v1_result_t code, size_t offset, uint8_t table, uint8_t row, uint8_t field) {
    if (error) {
        error->code   = code;
        error->offset = offset;
        error->table  = table;
        error->row    = row;
        error->field  = field;
    }
    return code;
}

noah_profile_rgb_v1_limits_t noah_profile_rgb_v1_default_limits(void) {
    noah_profile_rgb_v1_limits_t limits = {
        .compiled_stage_mask    = NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL,
        .max_payload_size       = NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE,
        .logical_layer_count    = 0u,
        .max_logical_layers     = NOAH_PROFILE_RGB_V1_MAX_LOGICAL_LAYERS,
        .maximum_brightness     = UINT8_MAX,
        .tap_branch_color_count = NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS,
        .supported_pd_mode_mask = NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL,
    };
    return limits;
}

static noah_profile_rgb_v1_result_t resolve_limits(const noah_profile_rgb_v1_limits_t *supplied, noah_profile_rgb_v1_limits_t *limits, noah_profile_rgb_v1_error_t *error) {
    *limits = supplied ? *supplied : noah_profile_rgb_v1_default_limits();
    if ((limits->compiled_stage_mask & (uint16_t)~NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL) != 0u || limits->max_payload_size == 0u || limits->max_payload_size > NOAH_PROFILE_RGB_V1_MAX_PAYLOAD_SIZE || limits->max_logical_layers > NOAH_PROFILE_RGB_V1_MAX_LOGICAL_LAYERS || limits->logical_layer_count > limits->max_logical_layers || limits->tap_branch_color_count == 0u || limits->tap_branch_color_count > NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS || (limits->supported_pd_mode_mask & (uint8_t)~NOAH_PROFILE_RGB_V1_PD_MODE_MASK_ALL) != 0u) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    }
    return NOAH_PROFILE_RGB_V1_OK;
}

static noah_profile_rgb_v1_result_t read_bytes(const noah_profile_rgb_v1_view_t *view, size_t offset, uint8_t *target, size_t length, uint8_t table, uint8_t row, uint8_t field, noah_profile_rgb_v1_error_t *error) {
    if (!view || !view->reader.read || offset > view->byte_length || length > (size_t)view->byte_length - offset) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, offset, table, row, field);
    }
    if (!noah_profile_reader_read(&view->reader, view->base_offset + offset, target, length)) {
        return fail(error, NOAH_PROFILE_RGB_V1_READ_ERROR, offset, table, row, field);
    }
    return NOAH_PROFILE_RGB_V1_OK;
}

static size_t section_offset(const noah_profile_rgb_v1_view_t *view, rgb_section_t section) {
    size_t offset = NOAH_PROFILE_RGB_V1_HEADER_SIZE;

    if (section == RGB_SECTION_GROUPS) return offset;
    offset += (size_t)view->group_count * RGB_GROUP_RECORD_SIZE;
    if (section == RGB_SECTION_LAYER_COLORS) return offset;
    offset += (size_t)view->layer_color_count * RGB_LAYER_COLOR_RECORD_SIZE;
    if (section == RGB_SECTION_LAYER_GROUPS) return offset;
    offset += (size_t)view->layer_group_count * RGB_GROUP_ROW_RECORD_SIZE;
    if (section == RGB_SECTION_AUTOMOUSE) return offset;
    offset += RGB_AUTOMOUSE_RECORD_SIZE;
    if (section == RGB_SECTION_PD_COLORS) return offset;
    offset += (size_t)view->pd_color_count * RGB_PD_COLOR_RECORD_SIZE;
    if (section == RGB_SECTION_PD_GROUPS) return offset;
    offset += (size_t)view->pd_group_count * RGB_GROUP_ROW_RECORD_SIZE;
    if (section == RGB_SECTION_COMBO) return offset;
    offset += RGB_COMBO_RECORD_SIZE;
    if (section == RGB_SECTION_COMBO_GROUPS) return offset;
    offset += (size_t)view->combo_group_count * RGB_COMBO_GROUP_RECORD_SIZE;
    if (section == RGB_SECTION_TAP_BRANCH_COLORS) return offset;
    offset += (size_t)view->tap_branch_color_count * RGB_COLOR_RECORD_SIZE;
    if (section == RGB_SECTION_KEY_FEEDBACK) return offset;
    offset += RGB_KEY_FEEDBACK_SIZE;
    if (section == RGB_SECTION_KEY_GROUPS) return offset;
    offset += (size_t)view->key_group_count * RGB_GROUP_ROW_RECORD_SIZE;
    return offset;
}

static bool view_is_valid(const noah_profile_rgb_v1_view_t *view) {
    return view && view->reader.read && view->base_offset <= view->reader.length && view->byte_length <= view->reader.length - view->base_offset && section_offset(view, RGB_SECTION_END) == view->byte_length;
}

static noah_profile_rgb_v1_hsv_t hsv_from_bytes(const uint8_t *bytes) {
    noah_profile_rgb_v1_hsv_t color = {.h = bytes[0], .s = bytes[1], .v = bytes[2]};
    return color;
}

static bool hsv_is_black(const uint8_t *bytes) {
    return bytes[0] == 0u && bytes[1] == 0u && bytes[2] == 0u;
}

static noah_profile_rgb_v1_result_t validate_hsv(const noah_profile_rgb_v1_view_t *view, const uint8_t bytes[3], size_t offset, uint8_t table, uint8_t row, noah_profile_rgb_v1_error_t *error) {
    if (bytes[2] > view->limits.maximum_brightness) {
        return fail(error, NOAH_PROFILE_RGB_V1_BRIGHTNESS_EXCEEDED, offset + 2u, table, row, NOAH_PROFILE_RGB_V1_FIELD_COLOR);
    }
    return NOAH_PROFILE_RGB_V1_OK;
}

static noah_profile_rgb_v1_result_t validate_group_row(const noah_profile_rgb_v1_view_t *view, size_t offset, uint8_t table, uint8_t row, uint8_t selector_kind, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_GROUP_ROW_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result = read_bytes(view, offset, bytes, sizeof(bytes), table, row, NOAH_PROFILE_RGB_V1_FIELD_SELECTOR, error);

    if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    if (selector_kind == NOAH_PROFILE_RGB_V1_TABLE_LAYER_GROUPS && bytes[0] != NOAH_PROFILE_RGB_V1_SELECTOR_ALL && bytes[0] >= view->layer_color_count) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_SELECTOR, offset, table, row, NOAH_PROFILE_RGB_V1_FIELD_SELECTOR);
    }
    if (selector_kind == NOAH_PROFILE_RGB_V1_TABLE_PD_GROUPS && bytes[0] != NOAH_PROFILE_RGB_V1_SELECTOR_ALL && (bytes[0] >= NOAH_PROFILE_RGB_V1_MAX_PD_MODES || (view->limits.supported_pd_mode_mask & (1u << bytes[0])) == 0u)) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_SELECTOR, offset, table, row, NOAH_PROFILE_RGB_V1_FIELD_SELECTOR);
    }
    if (selector_kind == NOAH_PROFILE_RGB_V1_TABLE_KEY_GROUPS && bytes[0] != NOAH_PROFILE_RGB_V1_SELECTOR_ALL && bytes[0] > 3u) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_SELECTOR, offset, table, row, NOAH_PROFILE_RGB_V1_FIELD_SEMANTIC);
    }
    result = validate_hsv(view, &bytes[1], offset + 1u, table, row, error);
    if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    if (bytes[4] >= view->group_count) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_REFERENCE, offset + 4u, table, row, NOAH_PROFILE_RGB_V1_FIELD_GROUP_ID);
    }
    return NOAH_PROFILE_RGB_V1_OK;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_decode_reader(const noah_profile_reader_t *reader, size_t base_offset, size_t length, const noah_profile_rgb_v1_limits_t *supplied_limits, noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_error_t *error) {
    noah_profile_rgb_v1_view_t candidate;
    uint8_t                    header[NOAH_PROFILE_RGB_V1_HEADER_SIZE];
    size_t                     expected_length;
    size_t                     offset;
    uint8_t                    bytes[RGB_KEY_FEEDBACK_SIZE];
    uint8_t                    previous_bitmap[NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE];
    bool                       have_previous_bitmap = false;
    uint8_t                    observed_pd_mask = 0u;
    int16_t                    previous_pd_id = -1;
    noah_profile_rgb_v1_result_t result;

    clear_error(error);
    if (!reader || !reader->read || !view || base_offset > reader->length || length > reader->length - base_offset) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    }
    memset(&candidate, 0, sizeof(candidate));
    memset(view, 0, sizeof(*view));
    result = resolve_limits(supplied_limits, &candidate.limits, error);
    if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    if (length < NOAH_PROFILE_RGB_V1_HEADER_SIZE) {
        return fail(error, NOAH_PROFILE_RGB_V1_TRUNCATED, length, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    }
    if (length > candidate.limits.max_payload_size || length > UINT16_MAX) {
        return fail(error, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED, length, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    }
    candidate.reader      = *reader;
    candidate.base_offset = base_offset;
    candidate.byte_length = (uint16_t)length;
    result = read_bytes(&candidate, 0u, header, sizeof(header), NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_HEADER, error);
    if (result != NOAH_PROFILE_RGB_V1_OK) return result;

    if (header[0] != NOAH_PROFILE_RGB_V1_FORMAT_VERSION) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_VERSION, 0u, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_FORMAT_VERSION);
    }
    if (header[1] != 0u || header[14] != 0u || header[15] != 0u) {
        size_t bad_offset = header[1] != 0u ? 1u : (header[14] != 0u ? 14u : 15u);
        return fail(error, NOAH_PROFILE_RGB_V1_RESERVED_BITS, bad_offset, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_RESERVED);
    }
    candidate.stage_enable_mask = read_u16(&header[2]);
    if ((candidate.stage_enable_mask & (uint16_t)~NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL) != 0u) {
        return fail(error, NOAH_PROFILE_RGB_V1_RESERVED_BITS, 2u, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_STAGE_MASK);
    }
    if ((candidate.stage_enable_mask & (uint16_t)~candidate.limits.compiled_stage_mask) != 0u) {
        return fail(error, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE, 2u, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_STAGE_MASK);
    }
    if (header[12] != NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT || header[13] != NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE) {
        return fail(error, NOAH_PROFILE_RGB_V1_INCOMPATIBLE_GEOMETRY, header[12] != NOAH_PROFILE_RGB_V1_PHYSICAL_LED_COUNT ? 12u : 13u, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_GEOMETRY);
    }

    candidate.group_count            = header[4];
    candidate.layer_color_count      = header[5];
    candidate.layer_group_count      = header[6];
    candidate.pd_color_count         = header[7];
    candidate.pd_group_count         = header[8];
    candidate.combo_group_count      = header[9];
    candidate.tap_branch_color_count = header[10];
    candidate.key_group_count        = header[11];
    if (candidate.group_count > NOAH_PROFILE_RGB_V1_MAX_GROUPS || candidate.layer_color_count > candidate.limits.max_logical_layers || candidate.pd_color_count > NOAH_PROFILE_RGB_V1_MAX_PD_MODES || candidate.tap_branch_color_count > NOAH_PROFILE_RGB_V1_MAX_TAP_BRANCH_COLORS || (uint16_t)candidate.layer_group_count + candidate.pd_group_count + candidate.combo_group_count + candidate.key_group_count > NOAH_PROFILE_RGB_V1_MAX_STAGE_GROUP_ROWS) {
        return fail(error, NOAH_PROFILE_RGB_V1_CAPACITY_EXCEEDED, 4u, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_COUNT);
    }
    expected_length = RGB_FIXED_PAYLOAD_SIZE + (size_t)candidate.group_count * RGB_GROUP_RECORD_SIZE + (size_t)candidate.layer_color_count * RGB_LAYER_COLOR_RECORD_SIZE + (size_t)candidate.layer_group_count * RGB_GROUP_ROW_RECORD_SIZE + (size_t)candidate.pd_color_count * RGB_PD_COLOR_RECORD_SIZE + (size_t)candidate.pd_group_count * RGB_GROUP_ROW_RECORD_SIZE + (size_t)candidate.combo_group_count * RGB_COMBO_GROUP_RECORD_SIZE + (size_t)candidate.tap_branch_color_count * RGB_COLOR_RECORD_SIZE + (size_t)candidate.key_group_count * RGB_GROUP_ROW_RECORD_SIZE;
    if (length < expected_length) {
        return fail(error, NOAH_PROFILE_RGB_V1_TRUNCATED, length, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_COUNT);
    }
    if (length > expected_length) {
        return fail(error, NOAH_PROFILE_RGB_V1_TRAILING_BYTES, expected_length, NOAH_PROFILE_RGB_V1_TABLE_HEADER, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_COUNT);
    }

    offset = section_offset(&candidate, RGB_SECTION_GROUPS);
    for (uint8_t index = 0u; index < candidate.group_count; index++, offset += RGB_GROUP_RECORD_SIZE) {
        int order;
        result = read_bytes(&candidate, offset, bytes, RGB_GROUP_RECORD_SIZE, NOAH_PROFILE_RGB_V1_TABLE_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_ID, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
        if (bytes[0] != index) {
            return fail(error, NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER, offset, NOAH_PROFILE_RGB_V1_TABLE_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_ID);
        }
        if ((bytes[8] & 0xfcu) != 0u) {
            return fail(error, NOAH_PROFILE_RGB_V1_RESERVED_BITS, offset + 8u, NOAH_PROFILE_RGB_V1_TABLE_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_BITMAP);
        }
        if (have_previous_bitmap) {
            order = memcmp(previous_bitmap, &bytes[1], NOAH_PROFILE_RGB_V1_LED_BITMAP_SIZE);
            if (order == 0) {
                return fail(error, NOAH_PROFILE_RGB_V1_DUPLICATE_BITMAP, offset + 1u, NOAH_PROFILE_RGB_V1_TABLE_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_BITMAP);
            }
            if (order > 0) {
                return fail(error, NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER, offset + 1u, NOAH_PROFILE_RGB_V1_TABLE_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_BITMAP);
            }
        }
        memcpy(previous_bitmap, &bytes[1], sizeof(previous_bitmap));
        have_previous_bitmap = true;
    }

    offset = section_offset(&candidate, RGB_SECTION_LAYER_COLORS);
    for (uint8_t index = 0u; index < candidate.layer_color_count; index++, offset += RGB_LAYER_COLOR_RECORD_SIZE) {
        result = read_bytes(&candidate, offset, bytes, RGB_LAYER_COLOR_RECORD_SIZE, NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_ID, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
        if (bytes[0] != index) return fail(error, NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER, offset, NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_ID);
        result = validate_hsv(&candidate, &bytes[1], offset + 1u, NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS, index, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
        if (bytes[4] > 1u) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ENUM, offset + 4u, NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_MODE);
    }
    if ((candidate.limits.compiled_stage_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) != 0u) {
        if (candidate.layer_color_count == 0u || (candidate.limits.logical_layer_count != 0u && candidate.layer_color_count != candidate.limits.logical_layer_count)) {
            return fail(error, NOAH_PROFILE_RGB_V1_INCOMPLETE_SURFACE, 5u, NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_COUNT);
        }
    } else if (candidate.layer_color_count != 0u || candidate.layer_group_count != 0u) {
        return fail(error, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA, 5u, NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_COUNT);
    }

    offset = section_offset(&candidate, RGB_SECTION_LAYER_GROUPS);
    for (uint8_t index = 0u; index < candidate.layer_group_count; index++, offset += RGB_GROUP_ROW_RECORD_SIZE) {
        result = validate_group_row(&candidate, offset, NOAH_PROFILE_RGB_V1_TABLE_LAYER_GROUPS, index, NOAH_PROFILE_RGB_V1_TABLE_LAYER_GROUPS, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    }

    offset = section_offset(&candidate, RGB_SECTION_AUTOMOUSE);
    result = read_bytes(&candidate, offset, bytes, RGB_AUTOMOUSE_RECORD_SIZE, NOAH_PROFILE_RGB_V1_TABLE_AUTOMOUSE, 0u, NOAH_PROFILE_RGB_V1_FIELD_MODE, error);
    if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    if (bytes[0] > 2u) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ENUM, offset, NOAH_PROFILE_RGB_V1_TABLE_AUTOMOUSE, 0u, NOAH_PROFILE_RGB_V1_FIELD_MODE);
    result = validate_hsv(&candidate, &bytes[1], offset + 1u, NOAH_PROFILE_RGB_V1_TABLE_AUTOMOUSE, 0u, error);
    if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    if ((candidate.limits.compiled_stage_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE) == 0u && (bytes[0] != 0u || !hsv_is_black(&bytes[1]))) {
        return fail(error, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA, offset, NOAH_PROFILE_RGB_V1_TABLE_AUTOMOUSE, 0u, NOAH_PROFILE_RGB_V1_FIELD_MODE);
    }

    offset = section_offset(&candidate, RGB_SECTION_PD_COLORS);
    for (uint8_t index = 0u; index < candidate.pd_color_count; index++, offset += RGB_PD_COLOR_RECORD_SIZE) {
        uint8_t pd_id;
        result = read_bytes(&candidate, offset, bytes, RGB_PD_COLOR_RECORD_SIZE, NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_ID, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
        pd_id = bytes[0];
        if (pd_id >= NOAH_PROFILE_RGB_V1_MAX_PD_MODES || (candidate.limits.supported_pd_mode_mask & (1u << pd_id)) == 0u) {
            return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ID, offset, NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_ID);
        }
        if ((int16_t)pd_id <= previous_pd_id) return fail(error, NOAH_PROFILE_RGB_V1_NONCANONICAL_ORDER, offset, NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_ID);
        observed_pd_mask = (uint8_t)(observed_pd_mask | (1u << pd_id));
        previous_pd_id = pd_id;
        result = validate_hsv(&candidate, &bytes[1], offset + 1u, NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, index, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
        if (bytes[4] > 4u) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ENUM, offset + 4u, NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_LOCALITY);
    }
    if ((candidate.limits.compiled_stage_mask & NOAH_PROFILE_RGB_V1_STAGE_PD_MODE) != 0u) {
        if (observed_pd_mask != candidate.limits.supported_pd_mode_mask) return fail(error, NOAH_PROFILE_RGB_V1_INCOMPLETE_SURFACE, 7u, NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_COUNT);
    } else if (candidate.pd_color_count != 0u || candidate.pd_group_count != 0u) {
        return fail(error, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA, 7u, NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_COUNT);
    }

    offset = section_offset(&candidate, RGB_SECTION_PD_GROUPS);
    for (uint8_t index = 0u; index < candidate.pd_group_count; index++, offset += RGB_GROUP_ROW_RECORD_SIZE) {
        result = validate_group_row(&candidate, offset, NOAH_PROFILE_RGB_V1_TABLE_PD_GROUPS, index, NOAH_PROFILE_RGB_V1_TABLE_PD_GROUPS, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    }

    offset = section_offset(&candidate, RGB_SECTION_COMBO);
    result = read_bytes(&candidate, offset, bytes, RGB_COMBO_RECORD_SIZE, NOAH_PROFILE_RGB_V1_TABLE_COMBO, 0u, NOAH_PROFILE_RGB_V1_FIELD_COLOR, error);
    if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    result = validate_hsv(&candidate, bytes, offset, NOAH_PROFILE_RGB_V1_TABLE_COMBO, 0u, error);
    if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    if (bytes[3] > 4u) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ENUM, offset + 3u, NOAH_PROFILE_RGB_V1_TABLE_COMBO, 0u, NOAH_PROFILE_RGB_V1_FIELD_LOCALITY);
    if ((candidate.limits.compiled_stage_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) == 0u && (candidate.combo_group_count != 0u || bytes[3] != 0u || !hsv_is_black(bytes))) {
        return fail(error, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA, offset, NOAH_PROFILE_RGB_V1_TABLE_COMBO, 0u, NOAH_PROFILE_RGB_V1_FIELD_COLOR);
    }

    offset = section_offset(&candidate, RGB_SECTION_COMBO_GROUPS);
    for (uint8_t index = 0u; index < candidate.combo_group_count; index++, offset += RGB_COMBO_GROUP_RECORD_SIZE) {
        result = read_bytes(&candidate, offset, bytes, RGB_COMBO_GROUP_RECORD_SIZE, NOAH_PROFILE_RGB_V1_TABLE_COMBO_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_COLOR, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
        result = validate_hsv(&candidate, bytes, offset, NOAH_PROFILE_RGB_V1_TABLE_COMBO_GROUPS, index, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
        if (bytes[3] >= candidate.group_count) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_REFERENCE, offset + 3u, NOAH_PROFILE_RGB_V1_TABLE_COMBO_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_GROUP_ID);
    }

    offset = section_offset(&candidate, RGB_SECTION_TAP_BRANCH_COLORS);
    for (uint8_t index = 0u; index < candidate.tap_branch_color_count; index++, offset += RGB_COLOR_RECORD_SIZE) {
        result = read_bytes(&candidate, offset, bytes, RGB_COLOR_RECORD_SIZE, NOAH_PROFILE_RGB_V1_TABLE_TAP_BRANCH_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_COLOR, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
        result = validate_hsv(&candidate, bytes, offset, NOAH_PROFILE_RGB_V1_TABLE_TAP_BRANCH_COLORS, index, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    }

    offset = section_offset(&candidate, RGB_SECTION_KEY_FEEDBACK);
    result = read_bytes(&candidate, offset, bytes, RGB_KEY_FEEDBACK_SIZE, NOAH_PROFILE_RGB_V1_TABLE_KEY_FEEDBACK, 0u, NOAH_PROFILE_RGB_V1_FIELD_COLOR, error);
    if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    for (uint8_t color_index = 0u; color_index < 3u; color_index++) {
        result = validate_hsv(&candidate, &bytes[color_index * 3u], offset + color_index * 3u, NOAH_PROFILE_RGB_V1_TABLE_KEY_FEEDBACK, 0u, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    }
    if (bytes[9] > 1u) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ENUM, offset + 9u, NOAH_PROFILE_RGB_V1_TABLE_KEY_FEEDBACK, 0u, NOAH_PROFILE_RGB_V1_FIELD_MODE);
    if (bytes[10] > 4u) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ENUM, offset + 10u, NOAH_PROFILE_RGB_V1_TABLE_KEY_FEEDBACK, 0u, NOAH_PROFILE_RGB_V1_FIELD_LOCALITY);
    if ((candidate.limits.compiled_stage_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u) {
        if (candidate.tap_branch_color_count != candidate.limits.tap_branch_color_count) return fail(error, NOAH_PROFILE_RGB_V1_INCOMPLETE_SURFACE, 10u, NOAH_PROFILE_RGB_V1_TABLE_TAP_BRANCH_COLORS, UINT8_MAX, NOAH_PROFILE_RGB_V1_FIELD_COUNT);
    } else if (candidate.tap_branch_color_count != 0u || candidate.key_group_count != 0u || bytes[9] != 0u || bytes[10] != 0u || !hsv_is_black(&bytes[0]) || !hsv_is_black(&bytes[3]) || !hsv_is_black(&bytes[6])) {
        return fail(error, NOAH_PROFILE_RGB_V1_UNSUPPORTED_STAGE_DATA, offset, NOAH_PROFILE_RGB_V1_TABLE_KEY_FEEDBACK, 0u, NOAH_PROFILE_RGB_V1_FIELD_COLOR);
    }

    offset = section_offset(&candidate, RGB_SECTION_KEY_GROUPS);
    for (uint8_t index = 0u; index < candidate.key_group_count; index++, offset += RGB_GROUP_ROW_RECORD_SIZE) {
        result = validate_group_row(&candidate, offset, NOAH_PROFILE_RGB_V1_TABLE_KEY_GROUPS, index, NOAH_PROFILE_RGB_V1_TABLE_KEY_GROUPS, error);
        if (result != NOAH_PROFILE_RGB_V1_OK) return result;
    }

    *view = candidate;
    return NOAH_PROFILE_RGB_V1_OK;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_decode(const uint8_t *bytes, size_t length, const noah_profile_rgb_v1_limits_t *limits, noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_error_t *error) {
    noah_profile_reader_t reader = noah_profile_reader_from_memory(bytes, length);
    return noah_profile_rgb_v1_decode_reader(&reader, 0u, length, limits, view, error);
}

static noah_profile_rgb_v1_result_t accessor_read(const noah_profile_rgb_v1_view_t *view, rgb_section_t section, uint8_t index, uint8_t count, size_t record_size, uint8_t table, uint8_t *bytes, noah_profile_rgb_v1_error_t *error) {
    clear_error(error);
    if (!view_is_valid(view) || !bytes || index >= count) {
        return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, table, index, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    }
    return read_bytes(view, section_offset(view, section) + (size_t)index * record_size, bytes, record_size, table, index, NOAH_PROFILE_RGB_V1_FIELD_HEADER, error);
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_t *group, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_GROUP_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!group) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, RGB_SECTION_GROUPS, index, view ? view->group_count : 0u, sizeof(bytes), NOAH_PROFILE_RGB_V1_TABLE_GROUPS, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) {
        group->id = bytes[0];
        memcpy(group->bitmap, &bytes[1], sizeof(group->bitmap));
    }
    return result;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_layer_color_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_layer_color_t *row, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_LAYER_COLOR_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!row) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, RGB_SECTION_LAYER_COLORS, index, view ? view->layer_color_count : 0u, sizeof(bytes), NOAH_PROFILE_RGB_V1_TABLE_LAYER_COLORS, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) {
        row->layer_id = bytes[0]; row->color = hsv_from_bytes(&bytes[1]); row->mode = bytes[4];
    }
    return result;
}

static noah_profile_rgb_v1_result_t group_row_at(const noah_profile_rgb_v1_view_t *view, rgb_section_t section, uint8_t index, uint8_t count, uint8_t table, noah_profile_rgb_v1_group_row_t *row, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_GROUP_ROW_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!row) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, table, index, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, section, index, count, sizeof(bytes), table, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) {
        row->selector = bytes[0]; row->color = hsv_from_bytes(&bytes[1]); row->group_id = bytes[4];
    }
    return result;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_layer_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_row_t *row, noah_profile_rgb_v1_error_t *error) {
    return group_row_at(view, RGB_SECTION_LAYER_GROUPS, index, view ? view->layer_group_count : 0u, NOAH_PROFILE_RGB_V1_TABLE_LAYER_GROUPS, row, error);
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_automouse(const noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_automouse_t *value, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_AUTOMOUSE_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!value) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_AUTOMOUSE, 0u, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, RGB_SECTION_AUTOMOUSE, 0u, 1u, sizeof(bytes), NOAH_PROFILE_RGB_V1_TABLE_AUTOMOUSE, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) { value->mode = bytes[0]; value->end_color = hsv_from_bytes(&bytes[1]); }
    return result;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_pd_color_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_pd_color_t *row, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_PD_COLOR_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!row) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, RGB_SECTION_PD_COLORS, index, view ? view->pd_color_count : 0u, sizeof(bytes), NOAH_PROFILE_RGB_V1_TABLE_PD_COLORS, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) { row->pd_mode_id = bytes[0]; row->color = hsv_from_bytes(&bytes[1]); row->locality = bytes[4]; }
    return result;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_pd_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_row_t *row, noah_profile_rgb_v1_error_t *error) {
    return group_row_at(view, RGB_SECTION_PD_GROUPS, index, view ? view->pd_group_count : 0u, NOAH_PROFILE_RGB_V1_TABLE_PD_GROUPS, row, error);
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_combo_feedback(const noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_feedback_t *value, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_COMBO_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!value) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_COMBO, 0u, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, RGB_SECTION_COMBO, 0u, 1u, sizeof(bytes), NOAH_PROFILE_RGB_V1_TABLE_COMBO, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) { value->color = hsv_from_bytes(bytes); value->locality = bytes[3]; }
    return result;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_combo_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_combo_group_row_t *row, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_COMBO_GROUP_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!row) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_COMBO_GROUPS, index, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, RGB_SECTION_COMBO_GROUPS, index, view ? view->combo_group_count : 0u, sizeof(bytes), NOAH_PROFILE_RGB_V1_TABLE_COMBO_GROUPS, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) { row->color = hsv_from_bytes(bytes); row->group_id = bytes[3]; }
    return result;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_tap_branch_color_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_hsv_t *color, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_COLOR_RECORD_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!color) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_TAP_BRANCH_COLORS, index, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, RGB_SECTION_TAP_BRANCH_COLORS, index, view ? view->tap_branch_color_count : 0u, sizeof(bytes), NOAH_PROFILE_RGB_V1_TABLE_TAP_BRANCH_COLORS, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) *color = hsv_from_bytes(bytes);
    return result;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_key_feedback(const noah_profile_rgb_v1_view_t *view, noah_profile_rgb_v1_key_feedback_t *value, noah_profile_rgb_v1_error_t *error) {
    uint8_t bytes[RGB_KEY_FEEDBACK_SIZE];
    noah_profile_rgb_v1_result_t result;
    if (!value) return fail(error, NOAH_PROFILE_RGB_V1_INVALID_ARGUMENT, 0u, NOAH_PROFILE_RGB_V1_TABLE_KEY_FEEDBACK, 0u, NOAH_PROFILE_RGB_V1_FIELD_HEADER);
    result = accessor_read(view, RGB_SECTION_KEY_FEEDBACK, 0u, 1u, sizeof(bytes), NOAH_PROFILE_RGB_V1_TABLE_KEY_FEEDBACK, bytes, error);
    if (result == NOAH_PROFILE_RGB_V1_OK) {
        value->tap_committed_color = hsv_from_bytes(&bytes[0]);
        value->hold_active_color = hsv_from_bytes(&bytes[3]);
        value->long_hold_active_color = hsv_from_bytes(&bytes[6]);
        value->tap_commit_mode = bytes[9];
        value->locality = bytes[10];
    }
    return result;
}

noah_profile_rgb_v1_result_t noah_profile_rgb_v1_key_group_at(const noah_profile_rgb_v1_view_t *view, uint8_t index, noah_profile_rgb_v1_group_row_t *row, noah_profile_rgb_v1_error_t *error) {
    return group_row_at(view, RGB_SECTION_KEY_GROUPS, index, view ? view->key_group_count : 0u, NOAH_PROFILE_RGB_V1_TABLE_KEY_GROUPS, row, error);
}
