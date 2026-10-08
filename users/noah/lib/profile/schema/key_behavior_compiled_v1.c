// ────────────────────────────────────────────────────────────────────────────
// Compiled Key Behaviors — the authored key_behaviors[] as domain 0x20
// ────────────────────────────────────────────────────────────────────────────

#include "profile_compiled_writer.h"

#include <string.h>

#include "key_behavior_domain_v1.h"
#include "noah_keymap_ids.h"
#include "lib/key/behavior/key_behavior.h"

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

noah_profile_compiled_v1_result_t noah_profile_key_behaviors_compiled_v1_write(compiled_writer_t *writer, noah_profile_compiled_v1_error_t *error) {
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
