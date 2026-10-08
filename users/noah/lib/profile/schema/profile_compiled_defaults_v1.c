// ─────────────────────────────────────────────────────────────────────────
// Compiled Authored Defaults — Profile Wire v1.0
// ─────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_defaults_v1.h"

#include <string.h>

#include "key_behavior_domain_v1.h"
#include "profile_rgb_v1.h"
#include "profile_rgb_compiled_v1.h"
#include "profile_compiled_writer.h"
#include "profile_settings_defaults.h"
#include "profile_combo_v1.h"
#include "../runtime/profile_action_runtime_v1.h"
#include "../storage/profile_checksum.h"
#include "noah_keymap_ids.h"
#include "lib/key/behavior/key_behavior.h"
#include "lib/pointing/defs/pd_modes.h"
#include "lib/rgb/core/rgb_helpers.h"

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

static bool checksum_write(void *context, const uint8_t *bytes, size_t length) {
    checksum_sink_t *sink = context;
    sink->crc32           = noah_profile_crc32_update(sink->crc32, bytes, length);
    sink->digest          = noah_profile_fnv1a_update(sink->digest, bytes, length);
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
#ifdef NOAH_COMPILED_DEFAULTS_TEST
    extern void noah_compiled_defaults_test_behavior_visit(void);
    noah_compiled_defaults_test_behavior_visit();
#endif
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

// The authored rows in canonical order, worked out once. Finding the order is
// cubic in the row count, the table is compiled data, so the order never
// changes, and every serialization emits it: open, a full write, and each
// chunk a host or the validator reads through the compiled reader, which
// replays the stream up to that chunk. A failure is not remembered, so every
// later serialization reports it again, as before.
static uint8_t canonical_behavior_rows[NOAH_KEY_BEHAVIOR_DOMAIN_V1_MAX_ROWS];
static bool    canonical_behavior_rows_ready;

static noah_profile_compiled_v1_result_t canonical_behavior_order(noah_profile_compiled_v1_error_t *error) {
    if (canonical_behavior_rows_ready) return NOAH_PROFILE_COMPILED_V1_OK;
    for (uint8_t order = 0u; order < key_behavior_count; order++) {
        const key_behavior_t *row = behavior_row_in_canonical_order(order, error);
        if (!row) return error ? error->code : NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR;
        canonical_behavior_rows[order] = (uint8_t)(row - key_behaviors);
    }
    canonical_behavior_rows_ready = true;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static noah_profile_compiled_v1_result_t map_hold(const hold_behavior_t *source, noah_key_behavior_hold_v1_t *target, uint8_t row, uint8_t step, noah_profile_compiled_v1_error_t *error) {
    noah_profile_compiled_v1_result_t result;
    if (!source->present || !target) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, row, step);
    switch (source->mode) {
        case HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE:
            target->mode = NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE;
            break;
        case HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD:
            target->mode = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD;
            break;
        case HOLD_BEHAVIOR_REPEAT_WHILE_HELD:
            target->mode = NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD;
            break;
        case HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD:
            target->mode = NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_ON_RELEASE_AFTER_HOLD;
            break;
        default:
            return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, row, step);
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
        size_t  body      = behavior_row_body_length(&key_behaviors[row]);
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
    size_t                            payload_length;
    uint8_t                           populated_steps;
    noah_profile_compiled_v1_result_t result = behavior_payload_size(&payload_length, &populated_steps, error);
    (void)payload_length;
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (!(emit_u8(writer, key_behavior_count) && emit_u8(writer, populated_steps) && emit_u16(writer, 0u))) return writer->result;
    result = canonical_behavior_order(error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;

    for (uint8_t order = 0u; order < key_behavior_count; order++) {
        uint8_t                  source_row = canonical_behavior_rows[order];
        const key_behavior_t    *row        = &key_behaviors[source_row];
        noah_profile_action_v1_t target;
        result = noah_profile_compiled_v1_action(row->keycode, &target);
        if (result != NOAH_PROFILE_COMPILED_V1_OK || target.kind == NOAH_PROFILE_ACTION_V1_NONE) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_KEY_BEHAVIOR, source_row, UINT8_MAX);
        if (!(emit_u16(writer, (uint16_t)behavior_row_body_length(row)) && emit_action(writer, &target) && emit_u16(writer, row->tap_hold_term) && emit_u16(writer, row->longer_hold_term) && emit_u16(writer, row->multi_tap_term) && emit_u8(writer, row->keeps_auto_mouse_anchored ? NOAH_KEY_BEHAVIOR_DOMAIN_V1_ROW_FLAG_AUTO_MOUSE : 0u) && emit_u8(writer, behavior_step_count(row)))) return writer->result;

        for (uint8_t step_index = 0u; step_index < KEY_BEHAVIOR_MAX_TAP_COUNT; step_index++) {
            const key_behavior_step_t *step = &row->tap_counts[step_index];
            uint8_t                    mask;
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

static noah_profile_compiled_v1_result_t action_abi_digest(uint32_t *digest, uint16_t *row_visits, noah_profile_compiled_v1_error_t *error) {
    checksum_sink_t      sink     = {.crc32 = NOAH_PROFILE_CRC32_INITIAL, .digest = NOAH_PROFILE_FNV1A_INITIAL};
    compiled_writer_t    writer   = {.write = checksum_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK};
    static const uint8_t magic[4] = {'N', 'L', 'A', '1'};

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
    emit_u8(&writer, NOAH_PROFILE_ACTION_V1_CUSTOM_KEY);
    emit_u8(&writer, LAYER_COUNT);
    emit_u8(&writer, PD_MODE_COUNT);
    emit_u8(&writer, VIA_MACRO_SLOT_COUNT);
    emit_u8(&writer, NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS);
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
    emit_u16(&writer, CUSTOM_KEY_0);
    emit_u16(&writer, LAYER_LOCK_BASE);
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        emit_u8(&writer, layer);
        emit_u16(&writer, MO(layer));
        emit_u16(&writer, LOCK_LAYER(layer));
    }
    for (uint8_t id = 0u; id < PD_MODE_COUNT; id++) {
        emit_u8(&writer, id);
        // The whole 32-bit flag: slots 16..31 have no bits in a u16.
        emit_u32(&writer, pd_modes[id].mode_flag);
        emit_u16(&writer, pd_modes[id].keycode);
        emit_u16(&writer, pd_modes[id].lock_action);
    }
    emit_u16(&writer, NOAH_KEYCODE_USERSPACE_END);
    emit_u16(&writer, UINT16_MAX);
    if (writer.result != NOAH_PROFILE_COMPILED_V1_OK) {
        return fail(error, writer.result, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    if (*row_visits > NOAH_PROFILE_COMPILED_V1_ACTION_ABI_ROW_VISITS_MAX) {
        return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_ACTION_ABI, UINT8_MAX, UINT8_MAX);
    }
    *digest = sink.digest;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

#ifdef NOAH_PD_PROFILE_ENABLE
// The sparse version-2 domain stores only the slots a profile uses.
static uint16_t pd_payload_size(void) {
    return (uint16_t)(NOAH_PROFILE_PD_V1_HEADER_SIZE + (size_t)noah_profile_pd_v1_default_record_count(noah_pd_defaults) * NOAH_PROFILE_PD_V1_RECORD_SIZE);
}

static noah_profile_compiled_v1_result_t write_pd_payload(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    const uint8_t               pd_header[8] = {NOAH_PROFILE_PD_V1_VERSION, NOAH_PROFILE_PD_V1_SLOT_COUNT, NOAH_PROFILE_PD_V1_RECORD_SIZE, noah_profile_pd_v1_default_record_count(noah_pd_defaults), 0, 0, 0, 0};
    noah_profile_pd_v1_cursor_t cursor;
    if (noah_profile_pd_v1_cursor_begin(&cursor, pd_header, pd_payload_size(), NULL) != NOAH_PROFILE_PD_V1_OK) return NOAH_PROFILE_COMPILED_V1_INVALID_ACTION;
    if (!emit(writer, pd_header, sizeof(pd_header))) return writer->result;
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) {
        uint8_t record[96];
        noah_profile_pd_v1_encode_record(&noah_pd_defaults[slot], record);
        if (noah_profile_pd_v1_validate_record(record, sizeof(record), slot, NULL) != NOAH_PROFILE_PD_V1_OK) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, slot, UINT8_MAX);
        if (!noah_profile_pd_v1_record_present(record)) continue;
        if (noah_profile_pd_v1_cursor_next(&cursor, record, NULL) != NOAH_PROFILE_PD_V1_OK) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, slot, UINT8_MAX);
        for (uint8_t offset = 0; offset < sizeof(record); offset++)
            if (!emit_u8(writer, record[offset])) return writer->result;
    }
    return writer->result;
}
#endif

static bool emit_domain_header(compiled_writer_t *writer, uint8_t id, uint16_t length) {
    const noah_profile_domain_shape_t *shape = noah_profile_domain_find(id);
    return shape && emit_u8(writer, shape->id) && emit_u8(writer, shape->version) && emit_u16(writer, length);
}

#ifdef COMBO_ENABLE
static noah_profile_compiled_v1_result_t write_combo_payload(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    if (noah_combo_count > NOAH_PROFILE_COMBO_V1_MAX_ROWS) return NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED;
    if (!(emit_u8(writer, noah_combo_count) && emit_u8(writer, 0) && emit_u16(writer, 0) && emit_u16(writer, COMBO_TERM) && emit_u16(writer, TAPPING_TERM))) return writer->result;
    for (uint16_t row = 0; row < noah_combo_count; row++) {
        const combo_t *combo = &key_combos[row];
        uint8_t        count = 0, flags = 0;
        while (count <= NOAH_PROFILE_COMBO_V1_MAX_INPUTS && pgm_read_word(&combo->keys[count]) != COMBO_END)
            count++;
        if (count < 2 || count > NOAH_PROFILE_COMBO_V1_MAX_INPUTS) return NOAH_PROFILE_COMPILED_V1_INVALID_ACTION;
#    ifdef COMBO_MUST_PRESS_IN_ORDER
        flags |= 4u;
#    endif
#    ifdef COMBO_MUST_HOLD_MODS
        uint16_t key = combo->keycode;
        if ((key >= 0xe0u && key <= 0xe7u) || (key >= 0x100u && key <= 0x1fffu && (key & 0xffu) == 0u) || (key >= 0x5220u && key <= 0x523fu)) flags |= 1u;
#    endif
        if (!(emit_u8(writer, count) && emit_u8(writer, flags) && emit_u16(writer, noah_combo_terms[row]) && emit_u32(writer, 0))) return writer->result;
        for (uint8_t member = 0; member <= NOAH_PROFILE_COMBO_V1_MAX_INPUTS; member++) {
            noah_profile_action_v1_t action = {0};
            if (member == 0 || member <= count) {
                uint16_t native = member == 0 ? combo->keycode : pgm_read_word(&combo->keys[member - 1]);
                if (noah_profile_compiled_v1_action(native, &action) != NOAH_PROFILE_COMPILED_V1_OK || action.kind == NOAH_PROFILE_ACTION_V1_NONE) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ACTION, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, row, member);
            }
            if (!emit_action(writer, &action)) return writer->result;
        }
    }
    return writer->result;
}
#endif

static noah_profile_compiled_v1_result_t write_domain(uint8_t id, compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
#ifdef NOAH_COMPILED_DEFAULTS_TEST
    extern void noah_compiled_defaults_test_domain_write(uint8_t id);
    if (id != NOAH_PROFILE_DOMAIN_V1_RGB) noah_compiled_defaults_test_domain_write(id);
#endif
    switch (id) {
#if defined(RGB_MATRIX_ENABLE)
        case NOAH_PROFILE_DOMAIN_V1_RGB:
            return noah_profile_rgb_compiled_v1_write(writer, error);
#endif
        case NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS:
            return write_behavior_payload(writer, error);
#ifdef COMBO_ENABLE
        case NOAH_PROFILE_DOMAIN_V1_COMBOS:
            return write_combo_payload(writer, error);
#endif
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
        case NOAH_PROFILE_DOMAIN_V1_SETTINGS: {
            uint16_t length = noah_profile_settings_defaults_length();
            for (uint16_t offset = 0; offset < length; offset++)
                if (!emit_u8(writer, noah_profile_settings_defaults_byte(offset))) return writer->result;
            return writer->result;
        }
#endif
#ifdef NOAH_PD_PROFILE_ENABLE
        case NOAH_PROFILE_DOMAIN_V1_PD:
            return write_pd_payload(writer, error);
#endif
        default:
            return NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT;
    }
}

static noah_profile_compiled_v1_result_t layout(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    size_t offset = NOAH_PROFILE_BLOB_V1_HEADER_SIZE;
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; i++) {
        const noah_profile_domain_shape_t *shape  = noah_profile_domain_at(i);
        size_t                             length = 0;
        noah_profile_compiled_v1_result_t  result = NOAH_PROFILE_COMPILED_V1_OK;
        switch (shape->id) {
#if defined(RGB_MATRIX_ENABLE)
            case NOAH_PROFILE_DOMAIN_V1_RGB: {
                uint8_t groups;
                result = noah_profile_rgb_compiled_v1_length(&length, &groups, error);
                break;
            }
#endif
            case NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS: {
                uint8_t steps;
                result = behavior_payload_size(&length, &steps, error);
                break;
            }
#ifdef COMBO_ENABLE
            case NOAH_PROFILE_DOMAIN_V1_COMBOS:
                if (noah_combo_count > NOAH_PROFILE_COMBO_V1_MAX_ROWS) return NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED;
                length = NOAH_PROFILE_COMBO_V2_HEADER_SIZE + (size_t)noah_combo_count * NOAH_PROFILE_COMBO_V1_ROW_SIZE;
                break;
#endif
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
            case NOAH_PROFILE_DOMAIN_V1_SETTINGS:
                length = noah_profile_settings_defaults_length();
                break;
#endif
#ifdef NOAH_PD_PROFILE_ENABLE
            case NOAH_PROFILE_DOMAIN_V1_PD:
                length = pd_payload_size();
                break;
#endif
            default:
                break;
        }
        if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
        if (!length) continue;
        offset += NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE;
        if (offset > NOAH_PROFILE_BLOB_V1_MAX_SIZE || length > NOAH_PROFILE_BLOB_V1_MAX_SIZE - offset) return NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED;
        profile->domains[i].offset = offset;
        profile->domains[i].length = length;
        profile->metadata.domain_mask |= shape->mask;
        offset += length;
    }
    profile->metadata.byte_length = offset;
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static bool emit_blob_header(const noah_profile_compiled_v1_t *profile, compiled_writer_t *writer) {
    const uint8_t header[8] = {'N', 'L', 'P', '1', NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR, NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR, noah_profile_domain_count(profile->metadata.domain_mask), NOAH_PROFILE_BLOB_V1_CANONICAL_FLAG};
    return emit(writer, header, sizeof(header));
}

static noah_profile_compiled_v1_result_t write_blob(const noah_profile_compiled_v1_t *profile, compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
    if (!emit_blob_header(profile, writer)) return writer->result;
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; i++) {
        if (!profile->domains[i].length) continue;
        const noah_profile_domain_shape_t *shape = noah_profile_domain_at(i);
        if (!emit_domain_header(writer, shape->id, profile->domains[i].length)) return writer->result;
        noah_profile_compiled_v1_result_t result = write_domain(shape->id, writer, error);
        if (result != NOAH_PROFILE_COMPILED_V1_OK || writer->stopped) return result;
    }
    return writer->result;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_open(noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_error_t *error) {
    checksum_sink_t                   sink   = {.crc32 = NOAH_PROFILE_CRC32_INITIAL, .digest = NOAH_PROFILE_FNV1A_INITIAL};
    compiled_writer_t                 writer = {.write = checksum_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK};
    noah_profile_compiled_v1_result_t result;
    uint32_t                          action_abi;
    uint16_t                          action_abi_row_visits;

    if (error) *error = no_error();
    if (!profile) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    memset(profile, 0, sizeof(*profile));
    result = action_abi_digest(&action_abi, &action_abi_row_visits, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    result = layout(profile, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    result = write_blob(profile, &writer, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (writer.offset > UINT16_MAX || writer.offset > NOAH_PROFILE_BLOB_V1_MAX_SIZE) return fail(error, NOAH_PROFILE_COMPILED_V1_CAPACITY_EXCEEDED, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    profile->metadata = (noah_profile_compiled_v1_metadata_t){
        .byte_length           = (uint16_t)writer.offset,
        .crc32                 = noah_profile_crc32_finish(sink.crc32),
        .digest                = sink.digest,
        .action_abi_digest     = action_abi,
        .action_abi_row_visits = action_abi_row_visits,
        .domain_mask           = profile->metadata.domain_mask,
    };
    return NOAH_PROFILE_COMPILED_V1_OK;
}

noah_profile_compiled_v1_result_t noah_profile_compiled_v1_write(const noah_profile_compiled_v1_t *profile, noah_profile_compiled_v1_write_fn write, void *context, noah_profile_compiled_v1_error_t *error) {
    compiled_writer_t                 writer = {.write = write, .context = context, .result = NOAH_PROFILE_COMPILED_V1_OK};
    noah_profile_compiled_v1_result_t result;
    if (error) *error = no_error();
    if (!profile || !write || profile->metadata.byte_length == 0u) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    result = write_blob(profile, &writer, error);
    if (result != NOAH_PROFILE_COMPILED_V1_OK) return result;
    if (writer.offset != profile->metadata.byte_length) return fail(error, NOAH_PROFILE_COMPILED_V1_INVALID_BEHAVIOR, NOAH_PROFILE_COMPILED_V1_SURFACE_NONE, UINT8_MAX, UINT8_MAX);
    return NOAH_PROFILE_COMPILED_V1_OK;
}

static bool compiled_reader_read(void *context, size_t offset, uint8_t *target, size_t length) {
    const noah_profile_compiled_v1_t *profile = context;
    range_sink_t                      sink    = {.start = offset, .end = offset + length, .target = target};
    noah_profile_compiled_v1_error_t  error;
    compiled_writer_t                 writer = {.write = range_write, .context = &sink, .result = NOAH_PROFILE_COMPILED_V1_OK, .allow_early_stop = true};
    if (!profile) return false;
    if (offset < NOAH_PROFILE_BLOB_V1_HEADER_SIZE) {
        if (!emit_blob_header(profile, &writer)) return writer.result == NOAH_PROFILE_COMPILED_V1_OK && sink.copied == length;
    }
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT && sink.copied < length; i++) {
        size_t start = profile->domains[i].offset, size = profile->domains[i].length;
        if (!size || offset >= start + size || offset + length <= start - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE) continue;
        const noah_profile_domain_shape_t *shape = noah_profile_domain_at(i);
        sink.stream_offset                       = start - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE;
        if (offset < start && !emit_domain_header(&writer, shape->id, size)) return writer.result == NOAH_PROFILE_COMPILED_V1_OK && sink.copied == length;
        if (sink.copied == length) break;
        sink.stream_offset = start;
        if (write_domain(shape->id, &writer, &error) != NOAH_PROFILE_COMPILED_V1_OK) return false;
        if (writer.stopped) break;
    }
    return writer.result == NOAH_PROFILE_COMPILED_V1_OK && sink.copied == length;
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

static const noah_profile_validator_v1_runtime_t compiled_runtime = {
    .combo_to_native = combo_to_native,
};
#endif

bool noah_profile_compiled_v1_compatibility(const noah_profile_compiled_v1_t *profile, noah_profile_validator_v1_compatibility_t *compatibility) {
    noah_profile_validator_v1_compatibility_t result;

    if (!profile || !compatibility || profile->metadata.domain_mask == 0u || (profile->metadata.domain_mask & (uint8_t)~NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS) != 0u) {
        return false;
    }
    result                     = noah_profile_validator_v1_default_compatibility(profile->metadata.action_abi_digest);
    result.allowed_domain_mask = profile->metadata.domain_mask;
#ifdef COMBO_ENABLE
    result.allowed_domain_mask |= NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS;
    result.runtime = &compiled_runtime;
#endif
    result.required_domain_mask              = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD;
    result.logical_layer_count               = LAYER_COUNT;
    result.supported_pd_mode_mask            = UINT32_MAX >> (32u - PD_MODE_COUNT);
    result.via_macro_slot_count              = VIA_MACRO_SLOT_COUNT;
    result.custom_key_count                  = NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS;
    result.rgb_limits.logical_layer_count    = LAYER_COUNT;
    result.rgb_limits.supported_pd_mode_mask = result.supported_pd_mode_mask;
    result.rgb_limits.tap_branch_color_count = KEY_BEHAVIOR_MAX_TAP_COUNT - 1u;
#if defined(RGB_MATRIX_ENABLE)
    result.rgb_limits.maximum_brightness  = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    result.rgb_limits.compiled_stage_mask = noah_profile_rgb_compiled_v1_stage_mask();
#else
    result.rgb_limits.compiled_stage_mask = 0u;
#endif
    *compatibility = result;
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    compatibility->allowed_domain_mask |= NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS;
#endif
    return true;
}

_Static_assert(sizeof(noah_profile_compiled_v1_t) <= 40u, "compiled-profile handle retains metadata and five ranges only");
