#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "send_string.h"

#include "../action/owned_keycode.h"
#include "macro_payload_internal.h"

typedef struct {
    macro_payload_hold_balance_t balance;
    owned_keycode_lease_t        leases[MACRO_PAYLOAD_MAX_TAP_KEYS];
} macro_payload_owned_holds_t;

typedef struct {
    macro_payload_ir_opcode_t opcode;
    const uint8_t            *bytes;
    uint16_t                  value;
    uint8_t                   length;
} macro_payload_ir_step_t;

typedef enum {
    MACRO_PAYLOAD_PHASE_IDLE = 0,
    MACRO_PAYLOAD_PHASE_READY,
    MACRO_PAYLOAD_PHASE_WAITING,
    MACRO_PAYLOAD_PHASE_TEXT_PRESS,
    MACRO_PAYLOAD_PHASE_TRANSIENT_RELEASE,
    MACRO_PAYLOAD_PHASE_DEAD_PRESS,
    MACRO_PAYLOAD_PHASE_CLEANUP,
} macro_payload_phase_t;

typedef struct {
    const macro_payload_ir_t     *ir;
    uint16_t                      cursor;
    uint16_t                      text_cursor;
    uint8_t                       text_remaining;
    uint8_t                       text_interval;
    macro_payload_phase_t         phase;
    macro_payload_phase_t         wait_resume_phase;
    macro_payload_phase_t         transient_next_phase;
    uint32_t                      wait_started_at;
    uint32_t                      wait_duration_ms;
    uint32_t                      transient_post_delay_ms;
    owned_keycode_lease_t         transient_leases[MACRO_PAYLOAD_MAX_TAP_KEYS];
    uint8_t                       transient_count;
    macro_payload_owned_holds_t   holds;
    macro_payload_source_t        source;
    uint8_t                       slot;
    macro_payload_finish_result_t terminal_result;
    macro_payload_finish_fn       finish;
    void                         *finish_context;
} macro_payload_engine_t;

static macro_payload_engine_t         macro_payload_engine;
static macro_payload_debug_snapshot_t macro_payload_diagnostics;

static void macro_payload_increment_u16(uint16_t *value) {
    if (value && *value != UINT16_MAX) {
        (*value)++;
    }
}

static void macro_payload_increment_operations(void) {
    if (macro_payload_diagnostics.operations_executed != UINT32_MAX) {
        macro_payload_diagnostics.operations_executed++;
    }
}

static uint8_t macro_payload_active_hold_count(void) {
    return (uint8_t)(macro_payload_engine.holds.balance.count + macro_payload_engine.transient_count);
}

static void macro_payload_note_hold_high_water(void) {
    uint8_t count = macro_payload_active_hold_count();

    if (count > macro_payload_diagnostics.active_hold_high_water) {
        macro_payload_diagnostics.active_hold_high_water = count;
    }
}

static bool macro_payload_ir_next(const uint8_t **cursor, const uint8_t *end, macro_payload_ir_step_t *step) {
    const uint8_t *current;

    if (!cursor || !*cursor || !end || !step || *cursor >= end) {
        return false;
    }

    current = *cursor;
    *step   = (macro_payload_ir_step_t){.opcode = (macro_payload_ir_opcode_t)(*current++)};

    switch (step->opcode) {
        case MACRO_PAYLOAD_IR_OP_TEXT:
            if (current >= end) {
                return false;
            }
            step->length = *current++;
            if (step->length == 0u || (size_t)(end - current) < step->length) {
                return false;
            }
            for (uint8_t index = 0; index < step->length; index++) {
                if (!macro_payload_text_byte_is_supported(current[index])) {
                    return false;
                }
            }
            step->bytes = current;
            current += step->length;
            break;
        case MACRO_PAYLOAD_IR_OP_DELAY:
            if ((size_t)(end - current) < 2u) {
                return false;
            }
            step->value = (uint16_t)current[0] | ((uint16_t)current[1] << 8);
            current += 2;
            break;
        case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
        case MACRO_PAYLOAD_IR_OP_KEY_UP:
            if (current >= end) {
                return false;
            }
            step->value = *current++;
            break;
        case MACRO_PAYLOAD_IR_OP_TAP_LIST:
            if (current >= end) {
                return false;
            }
            step->length = *current++;
            if (step->length == 0u || step->length > MACRO_PAYLOAD_MAX_TAP_KEYS || (size_t)(end - current) < step->length) {
                return false;
            }
            step->bytes = current;
            current += step->length;
            break;
        default:
            return false;
    }

    *cursor = current;
    return true;
}

static bool macro_payload_ir_preflight(const macro_payload_ir_t *ir) {
    const uint8_t               *cursor;
    const uint8_t               *end;
    macro_payload_hold_balance_t balance = {0};

    if (!ir || ir->length > sizeof(ir->bytes)) {
        return false;
    }

    cursor = ir->bytes;
    end    = ir->bytes + ir->length;
    while (cursor < end) {
        macro_payload_ir_step_t step;

        if (!macro_payload_ir_next(&cursor, end, &step)) {
            return false;
        }
        if (step.opcode == MACRO_PAYLOAD_IR_OP_KEY_DOWN && !macro_payload_hold_balance_note_down(&balance, (uint8_t)step.value)) {
            return false;
        }
        if (step.opcode == MACRO_PAYLOAD_IR_OP_KEY_UP && !macro_payload_hold_balance_note_up(&balance, (uint8_t)step.value)) {
            return false;
        }
    }
    return cursor == end && macro_payload_hold_balance_is_clear(&balance);
}

static bool macro_payload_owned_holds_acquire(macro_payload_owned_holds_t *holds, uint8_t keycode) {
    uint8_t index;

    if (!holds || !macro_payload_hold_balance_note_down(&holds->balance, keycode)) {
        return false;
    }
    index = (uint8_t)(holds->balance.count - 1u);
    if (!owned_keycode_acquire(keycode, &holds->leases[index])) {
        holds->balance.count--;
        return false;
    }
    macro_payload_note_hold_high_water();
    return true;
}

static bool macro_payload_owned_holds_release(macro_payload_owned_holds_t *holds, uint8_t keycode) {
    int8_t index;
    bool   released;

    if (!holds || (index = macro_payload_hold_balance_find(&holds->balance, keycode)) < 0) {
        return false;
    }
    released = owned_keycode_release(&holds->leases[index]);
    for (uint8_t i = (uint8_t)index; i + 1u < holds->balance.count; i++) {
        holds->balance.keycodes[i] = holds->balance.keycodes[i + 1u];
        holds->leases[i]           = holds->leases[i + 1u];
    }
    holds->balance.count--;
    holds->leases[holds->balance.count] = (owned_keycode_lease_t){0};
    return released;
}

static void macro_payload_schedule_wait(uint32_t now, uint32_t duration_ms, macro_payload_phase_t resume_phase) {
    macro_payload_engine.wait_started_at   = now;
    macro_payload_engine.wait_duration_ms  = duration_ms;
    macro_payload_engine.wait_resume_phase = resume_phase;
    macro_payload_engine.phase             = MACRO_PAYLOAD_PHASE_WAITING;
}

static void macro_payload_begin_cleanup(macro_payload_finish_result_t result) {
    if (macro_payload_engine.phase == MACRO_PAYLOAD_PHASE_IDLE || macro_payload_engine.phase == MACRO_PAYLOAD_PHASE_CLEANUP) {
        return;
    }
    macro_payload_engine.terminal_result = result;
    macro_payload_engine.phase           = MACRO_PAYLOAD_PHASE_CLEANUP;
}

static void macro_payload_begin_runtime_error(void) {
    macro_payload_begin_cleanup(MACRO_PAYLOAD_FINISH_RUNTIME_ERROR);
}

static void macro_payload_finish(macro_payload_finish_result_t result) {
    macro_payload_finish_fn finish         = macro_payload_engine.finish;
    void                   *finish_context = macro_payload_engine.finish_context;

    macro_payload_diagnostics.last_finish = result;
    switch (result) {
        case MACRO_PAYLOAD_FINISH_SUCCESS:
            macro_payload_increment_u16(&macro_payload_diagnostics.completed_count);
            break;
        case MACRO_PAYLOAD_FINISH_CANCELLED:
            macro_payload_increment_u16(&macro_payload_diagnostics.cancellation_count);
            break;
        case MACRO_PAYLOAD_FINISH_RUNTIME_ERROR:
            macro_payload_increment_u16(&macro_payload_diagnostics.runtime_error_count);
            break;
        default:
            break;
    }

    macro_payload_engine = (macro_payload_engine_t){0};
    if (finish) {
        finish(result, finish_context);
    }
}

static bool macro_payload_transient_acquire(const uint8_t *keycodes, uint8_t count) {
    if (!keycodes || count == 0u || count > ARRAY_SIZE(macro_payload_engine.transient_leases) || macro_payload_engine.transient_count != 0u) {
        return false;
    }

    for (uint8_t index = 0; index < count; index++) {
        if (!owned_keycode_acquire(keycodes[index], &macro_payload_engine.transient_leases[index])) {
            macro_payload_engine.transient_count = index;
            return false;
        }
        macro_payload_engine.transient_count++;
    }
    macro_payload_note_hold_high_water();
    return true;
}

static void macro_payload_start_transient(const uint8_t *keycodes, uint8_t count, uint32_t now, uint32_t hold_ms, uint32_t post_release_ms, macro_payload_phase_t next_phase) {
    if (!macro_payload_transient_acquire(keycodes, count)) {
        macro_payload_begin_runtime_error();
        return;
    }
    macro_payload_engine.transient_post_delay_ms = post_release_ms;
    macro_payload_engine.transient_next_phase    = next_phase;
    macro_payload_schedule_wait(now, hold_ms, MACRO_PAYLOAD_PHASE_TRANSIENT_RELEASE);
}

static bool macro_payload_ascii_lut_bit(const uint8_t *lut, uint8_t ascii_code) {
    return ((pgm_read_byte(&lut[ascii_code / 8u]) >> (ascii_code % 8u)) & 0x01u) != 0u;
}

static void macro_payload_start_text_char(uint32_t now) {
    uint8_t               ascii_code;
    uint8_t               keycodes[3];
    uint8_t               keycode_count = 0u;
    bool                  dead;
    macro_payload_phase_t next_phase;

    if (!macro_payload_engine.ir || macro_payload_engine.text_remaining == 0u) {
        macro_payload_begin_runtime_error();
        return;
    }

    ascii_code = macro_payload_engine.ir->bytes[macro_payload_engine.text_cursor++];
    macro_payload_engine.text_remaining--;
    if (macro_payload_ascii_lut_bit(ascii_to_shift_lut, ascii_code)) {
        keycodes[keycode_count++] = KC_LEFT_SHIFT;
    }
    if (macro_payload_ascii_lut_bit(ascii_to_altgr_lut, ascii_code)) {
        keycodes[keycode_count++] = KC_RIGHT_ALT;
    }
    keycodes[keycode_count++] = pgm_read_byte(&ascii_to_keycode_lut[ascii_code]);
    dead                      = macro_payload_ascii_lut_bit(ascii_to_dead_lut, ascii_code);
    next_phase                = dead ? MACRO_PAYLOAD_PHASE_DEAD_PRESS : (macro_payload_engine.text_remaining ? MACRO_PAYLOAD_PHASE_TEXT_PRESS : MACRO_PAYLOAD_PHASE_READY);
    macro_payload_start_transient(keycodes, keycode_count, now, macro_payload_engine.text_interval, macro_payload_engine.text_interval, next_phase);
}

static void macro_payload_start_dead_space(uint32_t now) {
    static const uint8_t  space_keycode[] = {KC_SPACE};
    macro_payload_phase_t next_phase      = macro_payload_engine.text_remaining ? MACRO_PAYLOAD_PHASE_TEXT_PRESS : MACRO_PAYLOAD_PHASE_READY;

    macro_payload_start_transient(space_keycode, 1u, now, TAP_CODE_DELAY, macro_payload_engine.text_interval, next_phase);
}

static void macro_payload_release_transients(uint32_t now) {
    bool ok = true;

    while (macro_payload_engine.transient_count > 0u) {
        macro_payload_engine.transient_count--;
        if (!owned_keycode_release(&macro_payload_engine.transient_leases[macro_payload_engine.transient_count])) {
            ok = false;
        }
    }
    if (!ok) {
        macro_payload_begin_runtime_error();
        return;
    }
    macro_payload_schedule_wait(now, macro_payload_engine.transient_post_delay_ms, macro_payload_engine.transient_next_phase);
}

static void macro_payload_execute_ready(uint32_t now) {
    const uint8_t          *cursor;
    const uint8_t          *end;
    macro_payload_ir_step_t step;

    if (!macro_payload_engine.ir || macro_payload_engine.cursor > macro_payload_engine.ir->length) {
        macro_payload_begin_runtime_error();
        return;
    }
    if (macro_payload_engine.cursor == macro_payload_engine.ir->length) {
        if (!macro_payload_hold_balance_is_clear(&macro_payload_engine.holds.balance)) {
            macro_payload_begin_runtime_error();
        } else {
            macro_payload_finish(MACRO_PAYLOAD_FINISH_SUCCESS);
        }
        return;
    }

    cursor = macro_payload_engine.ir->bytes + macro_payload_engine.cursor;
    end    = macro_payload_engine.ir->bytes + macro_payload_engine.ir->length;
    if (!macro_payload_ir_next(&cursor, end, &step)) {
        macro_payload_begin_runtime_error();
        return;
    }
    macro_payload_engine.cursor = (uint16_t)(cursor - macro_payload_engine.ir->bytes);

    switch (step.opcode) {
        case MACRO_PAYLOAD_IR_OP_TEXT:
            macro_payload_engine.text_cursor    = (uint16_t)(step.bytes - macro_payload_engine.ir->bytes);
            macro_payload_engine.text_remaining = step.length;
            macro_payload_start_text_char(now);
            break;
        case MACRO_PAYLOAD_IR_OP_DELAY:
            macro_payload_schedule_wait(now, (uint32_t)step.value + TAP_CODE_DELAY, MACRO_PAYLOAD_PHASE_READY);
            break;
        case MACRO_PAYLOAD_IR_OP_KEY_DOWN:
            if (!macro_payload_owned_holds_acquire(&macro_payload_engine.holds, (uint8_t)step.value)) {
                macro_payload_begin_runtime_error();
            } else {
                macro_payload_schedule_wait(now, TAP_CODE_DELAY, MACRO_PAYLOAD_PHASE_READY);
            }
            break;
        case MACRO_PAYLOAD_IR_OP_KEY_UP:
            if (!macro_payload_owned_holds_release(&macro_payload_engine.holds, (uint8_t)step.value)) {
                macro_payload_begin_runtime_error();
            } else {
                macro_payload_schedule_wait(now, TAP_CODE_DELAY, MACRO_PAYLOAD_PHASE_READY);
            }
            break;
        case MACRO_PAYLOAD_IR_OP_TAP_LIST: {
            uint32_t hold_ms = step.bytes[step.length - 1u] == KC_CAPS_LOCK ? TAP_HOLD_CAPS_DELAY : TAP_CODE_DELAY;
            macro_payload_start_transient(step.bytes, step.length, now, hold_ms, TAP_CODE_DELAY, MACRO_PAYLOAD_PHASE_READY);
            break;
        }
        default:
            macro_payload_begin_runtime_error();
            break;
    }
}

static void macro_payload_cleanup_one(void) {
    if (macro_payload_engine.transient_count > 0u) {
        macro_payload_engine.transient_count--;
        (void)owned_keycode_release(&macro_payload_engine.transient_leases[macro_payload_engine.transient_count]);
        return;
    }
    if (macro_payload_engine.holds.balance.count > 0u) {
        macro_payload_engine.holds.balance.count--;
        (void)owned_keycode_release(&macro_payload_engine.holds.leases[macro_payload_engine.holds.balance.count]);
        return;
    }
    macro_payload_finish(macro_payload_engine.terminal_result == MACRO_PAYLOAD_FINISH_NONE ? MACRO_PAYLOAD_FINISH_RUNTIME_ERROR : macro_payload_engine.terminal_result);
}

macro_payload_start_result_t macro_payload_start_ir(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context) {
    if (macro_payload_engine.phase != MACRO_PAYLOAD_PHASE_IDLE) {
        macro_payload_increment_u16(&macro_payload_diagnostics.busy_rejection_count);
        return MACRO_PAYLOAD_START_BUSY;
    }
    if (!macro_payload_ir_preflight(ir)) {
        return MACRO_PAYLOAD_START_INVALID;
    }
    if (ir->length == 0u) {
        return MACRO_PAYLOAD_START_EMPTY;
    }

    macro_payload_engine = (macro_payload_engine_t){
        .ir             = ir,
        .phase          = MACRO_PAYLOAD_PHASE_READY,
        .text_interval  = text_output == MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED ? interval : TAP_CODE_DELAY,
        .source         = source,
        .slot           = slot,
        .finish         = finish,
        .finish_context = context,
    };
    macro_payload_diagnostics.last_finish   = MACRO_PAYLOAD_FINISH_NONE;
    macro_payload_diagnostics.active_source = source;
    macro_payload_diagnostics.active_slot   = slot;
    return MACRO_PAYLOAD_START_STARTED;
}

void macro_payload_engine_scan(void) {
    uint32_t now;

    if (macro_payload_engine.phase == MACRO_PAYLOAD_PHASE_IDLE) {
        return;
    }
    now = timer_read32();
    if (macro_payload_engine.phase == MACRO_PAYLOAD_PHASE_WAITING) {
        uint32_t elapsed = now - macro_payload_engine.wait_started_at;

        if (elapsed < macro_payload_engine.wait_duration_ms) {
            return;
        }
        if (elapsed - macro_payload_engine.wait_duration_ms > macro_payload_diagnostics.maximum_lateness_ms) {
            macro_payload_diagnostics.maximum_lateness_ms = elapsed - macro_payload_engine.wait_duration_ms;
        }
        macro_payload_engine.phase = macro_payload_engine.wait_resume_phase;
        macro_payload_increment_operations();
        return;
    }

    switch (macro_payload_engine.phase) {
        case MACRO_PAYLOAD_PHASE_READY:
            macro_payload_execute_ready(now);
            break;
        case MACRO_PAYLOAD_PHASE_TEXT_PRESS:
            macro_payload_start_text_char(now);
            break;
        case MACRO_PAYLOAD_PHASE_TRANSIENT_RELEASE:
            macro_payload_release_transients(now);
            break;
        case MACRO_PAYLOAD_PHASE_DEAD_PRESS:
            macro_payload_start_dead_space(now);
            break;
        case MACRO_PAYLOAD_PHASE_CLEANUP:
            macro_payload_cleanup_one();
            break;
        default:
            macro_payload_begin_runtime_error();
            break;
    }
    macro_payload_increment_operations();
}

bool macro_payload_engine_cancel(void) {
    if (macro_payload_engine.phase == MACRO_PAYLOAD_PHASE_IDLE) {
        return false;
    }
    macro_payload_begin_cleanup(MACRO_PAYLOAD_FINISH_CANCELLED);
    return true;
}

void macro_payload_engine_init(void) {
    if (macro_payload_engine.phase != MACRO_PAYLOAD_PHASE_IDLE) {
        macro_payload_begin_cleanup(MACRO_PAYLOAD_FINISH_CANCELLED);
        return;
    }
    macro_payload_engine      = (macro_payload_engine_t){0};
    macro_payload_diagnostics = (macro_payload_debug_snapshot_t){0};
}

void macro_payload_debug_snapshot(macro_payload_debug_snapshot_t *out) {
    if (!out) {
        return;
    }
    *out                   = macro_payload_diagnostics;
    out->active_source     = macro_payload_engine.source;
    out->active_slot       = macro_payload_engine.slot;
    out->active_hold_count = macro_payload_active_hold_count();
    switch (macro_payload_engine.phase) {
        case MACRO_PAYLOAD_PHASE_IDLE:
            out->state = MACRO_PAYLOAD_ENGINE_IDLE;
            break;
        case MACRO_PAYLOAD_PHASE_READY:
            out->state = MACRO_PAYLOAD_ENGINE_READY;
            break;
        case MACRO_PAYLOAD_PHASE_WAITING:
            out->state = MACRO_PAYLOAD_ENGINE_WAITING;
            break;
        case MACRO_PAYLOAD_PHASE_CLEANUP:
            out->state = MACRO_PAYLOAD_ENGINE_CLEANUP;
            break;
        default:
            out->state = MACRO_PAYLOAD_ENGINE_OUTPUT;
            break;
    }
}
