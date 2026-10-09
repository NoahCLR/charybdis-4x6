#include "participation.h"

#include "../../profile/runtime/effective_key_behavior_runtime.h"
#include "../../profile/runtime/effective_settings_runtime.h"
#ifdef COMBO_ENABLE
#    include "../../compat/qmk_effective_combos.h"
#endif

// One bit per matrix position: set when that press bypassed its behaviour or
// stayed out of combos, so an untouched position allows both.
_Static_assert(MATRIX_ROWS * MATRIX_COLS <= 64, "a press decision covers at most 64 positions");
static uint64_t pressed_bypassed;
static uint64_t pressed_excluded;
#ifdef KEYRECORD_USER_DATA
static uint8_t pressed_context[MATRIX_ROWS * MATRIX_COLS];
enum { CONTEXT_VALID = 0x80, CONTEXT_GENERATED = 0x40, CONTEXT_COMBO = 0x20, CONTEXT_BEHAVIOR = 0x10, CONTEXT_LAYER = 0x0f };
#endif

static bool position_of(keypos_t key_pos, uint8_t *position) {
    if (key_pos.row >= MATRIX_ROWS || key_pos.col >= MATRIX_COLS) {
        return false;
    }
    *position = (uint8_t)(key_pos.row * MATRIX_COLS + key_pos.col);
    return true;
}

void noah_participation_press_store(keypos_t key_pos, bool behavior, bool combo) {
    uint8_t position;

    if (!position_of(key_pos, &position)) {
        return;
    }
    pressed_bypassed = behavior ? pressed_bypassed & ~(UINT64_C(1) << position) : pressed_bypassed | (UINT64_C(1) << position);
    pressed_excluded = combo ? pressed_excluded & ~(UINT64_C(1) << position) : pressed_excluded | (UINT64_C(1) << position);
}

bool noah_participation_press_behavior(keypos_t key_pos) {
    uint8_t position;
    return !position_of(key_pos, &position) || ((pressed_bypassed >> position) & 1u) == 0u;
}

bool noah_participation_press_combo(keypos_t key_pos) {
    uint8_t position;
    return !position_of(key_pos, &position) || ((pressed_excluded >> position) & 1u) == 0u;
}

// Builds without the live-profile runtime use the compiled tables, whose rows
// are always enabled; production supplies the strong definition.
__attribute__((weak)) noah_effective_key_behavior_result_t noah_effective_key_behavior_lookup(uint16_t keycode, noah_effective_key_behavior_row_t *row) {
    (void)keycode;
    (void)row;
    return NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK;
}

static bool layer_bit(uint32_t mask, uint8_t layer) {
    return layer < 32u && ((mask >> layer) & 1u) != 0u;
}

// One bit of a layer's placement bitmap: position row × columns + column.
static bool placement_bit(uint8_t layer, uint8_t field, keypos_t key_pos) {
    uint16_t position;

    if (key_pos.row >= MATRIX_ROWS || key_pos.col >= MATRIX_COLS) {
        return false;
    }
    position = (uint16_t)(key_pos.row * MATRIX_COLS + key_pos.col);
    return ((noah_setting_layer_record(layer, (uint8_t)(field + position / 8u)) >> (position % 8u)) & 1u) != 0u;
}

// The keycode's behaviour row, as the definition scope sees it: whether there
// is one, whether it is enabled, and the layers it may act on. A compiled row
// is enabled on every layer.
static bool behavior_row(uint16_t keycode, bool *enabled, uint32_t *allowed_layers) {
    noah_effective_key_behavior_row_t    row    = {0};
    noah_effective_key_behavior_result_t result = noah_effective_key_behavior_lookup(keycode, &row);

    *enabled        = true;
    *allowed_layers = UINT32_MAX;
    if (result != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) {
        // No live row, or the compiled tables: those rows are always enabled.
        return result == NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK;
    }
    *enabled        = (row.flags & NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_DISABLED) == 0u;
    *allowed_layers = row.allowed_layers;
    return true;
}

static bool behavior_definition(uint16_t keycode, uint8_t layer) {
    bool     enabled;
    uint32_t allowed_layers;

    if (!behavior_row(keycode, &enabled, &allowed_layers)) {
        return true;
    }
    return enabled && layer_bit(allowed_layers, layer);
}

bool noah_participation_behavior(uint16_t keycode, uint8_t source_layer, keypos_t key_pos) {
    return noah_setting(NOAH_SETTING_BEHAVIORS_ENABLED, 1u) != 0u                                       // master
           && layer_bit(noah_setting(NOAH_SETTING_LAYER_BEHAVIORS, UINT32_MAX), source_layer)          // layer
           && behavior_definition(keycode, source_layer)                                             // definition
           && !placement_bit(source_layer, NOAH_SETTINGS_LAYER_BYPASS, key_pos);                       // placement
}

bool noah_participation_behavior_generated(uint16_t keycode, uint8_t origin_layer) {
    return noah_setting(NOAH_SETTING_BEHAVIORS_ENABLED, 1u) != 0u && behavior_definition(keycode, origin_layer);
}

bool noah_participation_combo_press(uint8_t source_layer, keypos_t key_pos) {
    return layer_bit(noah_setting(NOAH_SETTING_LAYER_COMBOS, UINT32_MAX), source_layer) && !placement_bit(source_layer, NOAH_SETTINGS_LAYER_EXCLUDE, key_pos);
}

bool noah_participation_combo_row(uint16_t combo_index, uint8_t source_layer) {
#ifdef COMBO_ENABLE
    return noah_qmk_combo_enabled(combo_index) && layer_bit(noah_qmk_combo_allowed_layers(combo_index), source_layer);
#else
    (void)combo_index;
    (void)source_layer;
    return false;
#endif
}

void noah_participation_record_capture(keyrecord_t *record, uint8_t source_layer) {
#ifdef KEYRECORD_USER_DATA
    uint8_t position;
    if (!record || record->event.type != KEY_EVENT || !position_of(record->event.key, &position)) return;
    if (record->event.pressed) {
        pressed_context[position] = CONTEXT_VALID | (source_layer & CONTEXT_LAYER)
            | (noah_participation_press_behavior(record->event.key) ? CONTEXT_BEHAVIOR : 0)
            | (noah_participation_press_combo(record->event.key) ? CONTEXT_COMBO : 0);
    }
    record->user_data = pressed_context[position];
#else
    (void)record;
    (void)source_layer;
#endif
}
bool noah_participation_record_behavior(const keyrecord_t *record) {
#ifdef KEYRECORD_USER_DATA
    if (record && (record->user_data & CONTEXT_VALID)) return (record->user_data & CONTEXT_BEHAVIOR) != 0;
#endif
    return !record || noah_participation_press_behavior(record->event.key);
}
bool noah_participation_record_combo(const keyrecord_t *record) {
#ifdef KEYRECORD_USER_DATA
    if (record && (record->user_data & CONTEXT_VALID)) return (record->user_data & CONTEXT_COMBO) != 0;
#endif
    return !record || noah_participation_press_combo(record->event.key);
}
uint8_t noah_participation_record_source(const keyrecord_t *record) {
#ifdef KEYRECORD_USER_DATA
    if (record && (record->user_data & CONTEXT_VALID)) return record->user_data & CONTEXT_LAYER;
#endif
    return 0;
}
bool noah_participation_record_generated_captured(const keyrecord_t *record) {
#ifdef KEYRECORD_USER_DATA
    return record && record->event.type == COMBO_EVENT && (record->user_data & (CONTEXT_VALID | CONTEXT_GENERATED)) == (CONTEXT_VALID | CONTEXT_GENERATED);
#else
    (void)record;
    return false;
#endif
}
void noah_participation_record_generated(keyrecord_t *record, uint16_t keycode, uint8_t origin_layer) {
#ifdef KEYRECORD_USER_DATA
    if (record) record->user_data = CONTEXT_VALID | CONTEXT_GENERATED | (origin_layer & CONTEXT_LAYER)
        | (noah_participation_behavior_generated(keycode, origin_layer) ? CONTEXT_BEHAVIOR : 0);
#else
    (void)record;
    (void)keycode;
    (void)origin_layer;
#endif
}

// Bit 5 means combo permission on physical records and normalized origin on
// generated records. Context capture does not consume the origin tracker.
bool noah_participation_record_generated_normalized(const keyrecord_t *record) {
#ifdef KEYRECORD_USER_DATA
    return noah_participation_record_generated_captured(record) && (record->user_data & CONTEXT_COMBO);
#else
    (void)record;
    return false;
#endif
}
void noah_participation_record_generated_normalize(keyrecord_t *record, uint16_t keycode, uint8_t origin_layer) {
#ifdef KEYRECORD_USER_DATA
    if (!noah_participation_record_generated_captured(record)) noah_participation_record_generated(record, keycode, origin_layer);
    record->user_data |= CONTEXT_COMBO;
#else
    (void)record; (void)keycode; (void)origin_layer;
#endif
}
