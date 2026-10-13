// ────────────────────────────────────────────────────────────────────────────
// Lighting Rules — version 4 → version 5 conversion (Standard lighting)
// ────────────────────────────────────────────────────────────────────────────
//
// docs/architecture/lighting-rules-v1.md, "Version 4 → version 5 conversion".
// The firmware converts its compiled rgb_config.c tables this way, so the
// compiled defaults and any version-4 profile are the same function of the
// same bytes. A client performs the identical conversion.
//
// Two passes over one emitter: the first collects palette colours in the
// order the payload first references them and counts the sections; the
// second writes header, palette and sections.

#include "profile_lighting_v1.h"

#include <string.h>

typedef struct {
    uint8_t *out;
    size_t   capacity;
    size_t   length;
    bool     writing;
    bool     failed;
    uint8_t  palette[NOAH_PROFILE_LIGHTING_V1_PALETTE_MAX][3];
    uint8_t  palette_count;
    uint8_t  thing_tables;
    uint8_t  rules;
    uint8_t  base_rules;
} emitter_t;

// One cold-path conversion at a time; kept off the reviewed stacks.
static emitter_t emitter;

static void put(emitter_t *e, const uint8_t *bytes, size_t length) {
    if (!e->writing || e->failed) return;
    if (length > e->capacity - e->length) {
        e->failed = true;
        return;
    }
    memcpy(e->out + e->length, bytes, length);
    e->length += length;
}

static void put_u8(emitter_t *e, uint8_t value) {
    put(e, &value, 1u);
}

static bool hsv_equal(const uint8_t *a, noah_profile_rgb_v1_hsv_t b) {
    return a[0] == b.h && a[1] == b.s && a[2] == b.v;
}

// The palette index of a colour; the first pass appends unseen colours.
static uint8_t pal(emitter_t *e, noah_profile_rgb_v1_hsv_t colour) {
    for (uint8_t index = 0u; index < e->palette_count; index++) {
        if (hsv_equal(e->palette[index], colour)) return index;
    }
    if (e->writing || e->palette_count >= NOAH_PROFILE_LIGHTING_V1_PALETTE_MAX) {
        e->failed = true;
        return 0u;
    }
    e->palette[e->palette_count][0] = colour.h;
    e->palette[e->palette_count][1] = colour.s;
    e->palette[e->palette_count][2] = colour.v;
    return e->palette_count++;
}

static bool is_black(noah_profile_rgb_v1_hsv_t colour) {
    return colour.h == 0u && colour.s == 0u && colour.v == 0u;
}

static bool supports(uint32_t mask, uint8_t id) {
    return id < 32u && (mask & (UINT32_C(1) << id)) != 0u;
}

static uint8_t place_for_locality(uint8_t locality) {
    switch (locality) {
        case 1u:
            return NOAH_LIGHTING_TARGET_ID_LEFT_HALF;
        case 2u:
            return NOAH_LIGHTING_TARGET_ID_RIGHT_HALF;
        case 3u:
            return NOAH_LIGHTING_TARGET_ID_SUBJECT_HALVES;
        case 4u:
            return NOAH_LIGHTING_TARGET_ID_SUBJECT_KEYS;
        default:
            return NOAH_LIGHTING_TARGET_ID_WHOLE_BOARD;
    }
}

// A rule of one condition, one target and one paint; a group row's colour is
// either inherit (identity) or a palette colour.
typedef struct {
    uint8_t                   fact;
    uint8_t                   fact_params[2];
    uint8_t                   fact_length;
    uint8_t                   target;
    bool                      target_key_set;
    uint8_t                   key_set;
    bool                      inherit;
    noah_profile_rgb_v1_hsv_t colour;
} converted_rule_t;

static void emit_rule(emitter_t *e, const noah_profile_lighting_v1_limits_t *limits, const converted_rule_t *rule, bool disabled) {
    uint8_t paint = rule->inherit ? NOAH_LIGHTING_PAINT_ID_IDENTITY : NOAH_LIGHTING_PAINT_ID_COLOUR;
    uint8_t body[16];
    uint8_t size = 0u;

    if (!supports(limits->facts, rule->fact) || !supports(limits->targets, rule->target) || !supports(limits->paints, paint)) return;
    body[size++] = disabled ? NOAH_PROFILE_LIGHTING_V1_RULE_DISABLED : 0u;
    body[size++] = UINT8_MAX;
    body[size++] = (uint8_t)((NOAH_PROFILE_LIGHTING_V1_SLOT_CONDITION << 5u) | rule->fact_length);
    body[size++] = rule->fact;
    for (uint8_t index = 0u; index < rule->fact_length; index++) body[size++] = rule->fact_params[index];
    body[size++] = (uint8_t)((NOAH_PROFILE_LIGHTING_V1_SLOT_TARGET << 5u) | (rule->target_key_set ? 1u : 0u));
    body[size++] = rule->target;
    if (rule->target_key_set) body[size++] = rule->key_set;
    body[size++] = (uint8_t)((NOAH_PROFILE_LIGHTING_V1_SLOT_PAINT << 5u) | (rule->inherit ? 0u : 1u));
    body[size++] = paint;
    if (!rule->inherit) body[size++] = pal(e, rule->colour);
    put_u8(e, size);
    put(e, body, size);
    e->rules++;
}

static void emit_group_rule(emitter_t *e, const noah_profile_lighting_v1_limits_t *limits, uint8_t fact, const uint8_t *params, uint8_t length, noah_profile_rgb_v1_hsv_t colour, uint8_t key_set, bool disabled) {
    converted_rule_t rule = {.fact = fact, .fact_length = length, .target = NOAH_LIGHTING_TARGET_ID_KEY_SET, .target_key_set = true, .key_set = key_set, .inherit = is_black(colour), .colour = colour};
    memcpy(rule.fact_params, params, length);
    emit_rule(e, limits, &rule, disabled);
}

static void emit_main_rule(emitter_t *e, const noah_profile_lighting_v1_limits_t *limits, uint8_t fact, const uint8_t *params, uint8_t length, bool disabled) {
    converted_rule_t rule = {.fact = fact, .fact_length = length, .target = NOAH_LIGHTING_TARGET_ID_SUBJECT_PLACE, .inherit = true};
    memcpy(rule.fact_params, params, length);
    emit_rule(e, limits, &rule, disabled);
}

#define CHECK(call)                                         \
    do {                                                    \
        if ((call) != NOAH_PROFILE_RGB_V1_OK) return false; \
    } while (0)

static bool emit_key_feedback_rules(emitter_t *e, const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits) {
    noah_profile_rgb_v1_key_feedback_t key;
    noah_profile_rgb_v1_group_row_t    row;
    bool                               off = (v4->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) == 0u;
    uint8_t                            params[2];

    if (v4->tap_branch_color_count == 0u) return true;
    CHECK(noah_profile_rgb_v1_key_feedback(v4, &key, NULL));
    for (uint8_t pass = 0u; pass < 2u; pass++) {
        for (uint8_t index = v4->key_group_count; index-- > 0u;) {
            CHECK(noah_profile_rgb_v1_key_group_at(v4, index, &row, NULL));
            if ((row.selector == NOAH_PROFILE_RGB_V1_SELECTOR_ALL) != (pass == 1u)) continue;
            params[0] = row.selector == NOAH_PROFILE_RGB_V1_SELECTOR_ALL ? NOAH_LIGHTING_STATE_ALL : (uint8_t)(1u << row.selector);
            params[1] = NOAH_LIGHTING_SCOPE_PER_STATE;
            emit_group_rule(e, limits, NOAH_LIGHTING_FACT_ID_BEHAVIOUR_STATE, params, 2u, row.color, row.group_id, off);
        }
    }
    params[0] = NOAH_LIGHTING_STATE_ALL;
    params[1] = key.locality == 4u ? NOAH_LIGHTING_SCOPE_EACH_KEY : key.locality == 3u ? NOAH_LIGHTING_SCOPE_PER_HALF : NOAH_LIGHTING_SCOPE_NEWEST;
    emit_main_rule(e, limits, NOAH_LIGHTING_FACT_ID_BEHAVIOUR_STATE, params, 2u, off);
    return true;
}

static bool emit_combo_rules(emitter_t *e, const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits, uint8_t role) {
    noah_profile_rgb_v1_combo_group_row_t row;
    bool                                  off = (v4->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_COMBO) == 0u;

    for (uint8_t index = v4->combo_group_count; index-- > 0u;) {
        CHECK(noah_profile_rgb_v1_combo_group_at(v4, index, &row, NULL));
        emit_group_rule(e, limits, NOAH_LIGHTING_FACT_ID_COMBOS_HELD, &role, 1u, row.color, row.group_id, off);
    }
    emit_main_rule(e, limits, NOAH_LIGHTING_FACT_ID_COMBOS_HELD, &role, 1u, off);
    return true;
}

static bool emit_pointing_rules(emitter_t *e, const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits) {
    noah_profile_rgb_v1_group_row_t row;
    bool                            off = (v4->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_PD_MODE) == 0u;
    uint8_t                         any = NOAH_PROFILE_LIGHTING_V1_ANY;

    if (v4->pd_color_count == 0u) return true;
    for (uint8_t index = v4->pd_group_count; index-- > 0u;) {
        CHECK(noah_profile_rgb_v1_pd_group_at(v4, index, &row, NULL));
        emit_group_rule(e, limits, NOAH_LIGHTING_FACT_ID_POINTING_MODE_ACTIVE, &row.selector, 1u, row.color, row.group_id, off);
    }
    emit_main_rule(e, limits, NOAH_LIGHTING_FACT_ID_POINTING_MODE_ACTIVE, &any, 1u, off);
    return true;
}

// Preview and layer rules share the layer group rows.
static bool emit_layer_rules(emitter_t *e, const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits, uint8_t fact) {
    noah_profile_rgb_v1_group_row_t row;
    bool                            off = (v4->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_LAYER) == 0u;
    uint8_t                         any = NOAH_PROFILE_LIGHTING_V1_ANY;

    if (v4->layer_color_count == 0u) return true;
    for (uint8_t index = v4->layer_group_count; index-- > 0u;) {
        CHECK(noah_profile_rgb_v1_layer_group_at(v4, index, &row, NULL));
        emit_group_rule(e, limits, fact, &row.selector, 1u, row.color, row.group_id, off);
    }
    emit_main_rule(e, limits, fact, &any, 1u, off);
    return true;
}

static void begin_table(emitter_t *e, uint8_t kind, uint8_t records) {
    put_u8(e, kind);
    put_u8(e, records);
    e->thing_tables++;
}

static bool emit_thing_tables(emitter_t *e, const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits) {
    if (v4->layer_color_count && supports(limits->things, NOAH_LIGHTING_THING_LAYER)) {
        begin_table(e, NOAH_LIGHTING_THING_LAYER, v4->layer_color_count);
        put_u8(e, NOAH_LIGHTING_TARGET_ID_SUBJECT_LAYER_KEYS);
        put_u8(e, NOAH_PROFILE_LIGHTING_V1_NONE);
        for (uint8_t index = 0u; index < v4->layer_color_count; index++) {
            noah_profile_rgb_v1_layer_color_t row;
            CHECK(noah_profile_rgb_v1_layer_color_at(v4, index, &row, NULL));
            put_u8(e, row.mode == 0u ? NOAH_LIGHTING_TARGET_ID_WHOLE_BOARD : NOAH_LIGHTING_TARGET_ID_SUBJECT_LAYER_KEYS);
            put_u8(e, row.color.s == 0u && row.color.v == 0u ? NOAH_PROFILE_LIGHTING_V1_NONE : pal(e, row.color));
        }
    }
    if (v4->pd_color_count && supports(limits->things, NOAH_LIGHTING_THING_POINTING_MODE)) {
        begin_table(e, NOAH_LIGHTING_THING_POINTING_MODE, v4->pd_color_count);
        put_u8(e, NOAH_LIGHTING_TARGET_ID_WHOLE_BOARD);
        put_u8(e, NOAH_PROFILE_LIGHTING_V1_NONE);
        for (uint8_t index = 0u; index < v4->pd_color_count; index++) {
            noah_profile_rgb_v1_pd_color_t row;
            CHECK(noah_profile_rgb_v1_pd_color_at(v4, index, &row, NULL));
            put_u8(e, place_for_locality(row.locality));
            put_u8(e, pal(e, row.color));
        }
    }
    if (supports(limits->things, NOAH_LIGHTING_THING_COMBO)) {
        noah_profile_rgb_v1_feedback_t combo;
        CHECK(noah_profile_rgb_v1_combo_feedback(v4, &combo, NULL));
        begin_table(e, NOAH_LIGHTING_THING_COMBO, 0u);
        put_u8(e, place_for_locality(combo.locality));
        put_u8(e, pal(e, combo.color));
    }
    if (v4->tap_branch_color_count && supports(limits->things, NOAH_LIGHTING_THING_BEHAVIOUR)) {
        noah_profile_rgb_v1_key_feedback_t key;
        if (v4->tap_branch_color_count != limits->tap_depth - 1u) return false;
        CHECK(noah_profile_rgb_v1_key_feedback(v4, &key, NULL));
        begin_table(e, NOAH_LIGHTING_THING_BEHAVIOUR, 0u);
        put_u8(e, place_for_locality(key.locality));
        put_u8(e, pal(e, key.hold_active_color));
        put_u8(e, pal(e, key.long_hold_active_color));
        put_u8(e, pal(e, key.tap_committed_color));
        for (uint8_t index = 0u; index < v4->tap_branch_color_count; index++) {
            noah_profile_rgb_v1_hsv_t colour;
            CHECK(noah_profile_rgb_v1_tap_branch_color_at(v4, index, &colour, NULL));
            put_u8(e, pal(e, colour));
        }
    }
    return true;
}

static bool emit_key_sets(emitter_t *e, const noah_profile_rgb_v1_view_t *v4) {
    for (uint8_t id = 0u; id < v4->group_count; id++) {
        noah_profile_rgb_v1_group_t group;
        CHECK(noah_profile_rgb_v1_group_at(v4, id, &group, NULL));
        put(e, group.bitmap, sizeof(group.bitmap));
    }
    return true;
}

static bool emit_rules(emitter_t *e, const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits) {
    uint8_t before;

    e->rules = 0u;
    if (!emit_key_feedback_rules(e, v4, limits) || !emit_combo_rules(e, v4, limits, 1u) || !emit_pointing_rules(e, v4, limits) || !emit_layer_rules(e, v4, limits, NOAH_LIGHTING_FACT_ID_LAYER_PREVIEWING) || !emit_combo_rules(e, v4, limits, 0u)) return false;
    before = e->rules;
    if (!emit_layer_rules(e, v4, limits, NOAH_LIGHTING_FACT_ID_LAYER_ACTIVE)) return false;
    e->base_rules = (uint8_t)(e->rules - before);
    return true;
}

static bool emit_extensions(emitter_t *e, const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits) {
    noah_profile_rgb_v1_automouse_t    fade;
    noah_profile_rgb_v1_key_feedback_t key;
    bool fade_on = supports(limits->domain_extensions, NOAH_LIGHTING_EXTENSION_ID_AUTOMOUSE_FADE);
    bool flash   = false;
    uint8_t count;

    if (supports(limits->domain_extensions, NOAH_LIGHTING_EXTENSION_ID_TAP_COMMIT_FLASH) && v4->tap_branch_color_count && (v4->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_KEY_BEHAVIOR) != 0u) {
        CHECK(noah_profile_rgb_v1_key_feedback(v4, &key, NULL));
        flash = key.tap_commit_mode == 1u;
    }
    count = (uint8_t)((fade_on ? 1u : 0u) + (flash ? 1u : 0u));
    put_u8(e, count);
    if (fade_on) {
        uint8_t body[6];
        CHECK(noah_profile_rgb_v1_automouse(v4, &fade, NULL));
        body[0] = NOAH_LIGHTING_EXTENSION_ID_AUTOMOUSE_FADE;
        body[1] = 4u;
        body[2] = (v4->stage_enable_mask & NOAH_PROFILE_RGB_V1_STAGE_AUTOMOUSE) != 0u ? NOAH_PROFILE_LIGHTING_V1_FADE_ON : 0u;
        body[3] = fade.mode;
        body[4] = pal(e, fade.end_color);
        body[5] = e->base_rules;
        put(e, body, sizeof(body));
    }
    if (flash) {
        static const uint8_t body[3] = {NOAH_LIGHTING_EXTENSION_ID_TAP_COMMIT_FLASH, 1u, 1u};
        put(e, body, sizeof(body));
    }
    return true;
}

static bool emit_body(emitter_t *e, const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits) {
    e->thing_tables = 0u;
    return emit_thing_tables(e, v4, limits) && emit_key_sets(e, v4) && emit_rules(e, v4, limits) && emit_extensions(e, v4, limits) && !e->failed;
}

noah_profile_lighting_v1_result_t noah_profile_lighting_v1_convert_rgb_v4(const noah_profile_rgb_v1_view_t *v4, const noah_profile_lighting_v1_limits_t *limits, uint8_t *out, size_t capacity, size_t *length) {
    emitter_t *e = &emitter;

    if (!v4 || !limits || !out || !length) return NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT;
    memset(e, 0, sizeof(*e));
    if (!emit_body(e, v4, limits)) return NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT;
    e->out      = out;
    e->capacity = capacity;
    e->writing  = true;
    {
        uint8_t header[NOAH_PROFILE_LIGHTING_V1_HEADER_SIZE] = {NOAH_PROFILE_LIGHTING_V1_FORMAT_VERSION, 0u, NOAH_PROFILE_LIGHTING_V1_LED_COUNT, e->palette_count, e->thing_tables, v4->group_count, 0u, e->rules};
        put(e, header, sizeof(header));
        put(e, &e->palette[0][0], (size_t)e->palette_count * 3u);
    }
    if (!emit_body(e, v4, limits)) return e->failed ? NOAH_PROFILE_LIGHTING_V1_CAPACITY_EXCEEDED : NOAH_PROFILE_LIGHTING_V1_INVALID_ARGUMENT;
    *length = e->length;
    return NOAH_PROFILE_LIGHTING_V1_OK;
}
