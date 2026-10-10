#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "send_string.h"

#include "../action/owned_keycode.h"
#include "host_layout.h"
#include "macro_payload_internal.h"
#include "../profile/runtime/effective_settings_runtime.h"
#include "../compat/qmk_host.h"

#if defined(NOAH_PORTABLE_PROFILE_ENABLE) && !defined(NOAH_HOST_QMK_STUB) && (!defined(QMK_REPORT_MODS_OVERRIDE) || !defined(QMK_REPORT_KEYS_FILTER))
#    error "Unicode macro playback requires the QMK report modifier override and key filter contracts"
#endif

typedef struct {
    macro_payload_hold_balance_t balance;
    owned_keycode_lease_t        leases[MACRO_PAYLOAD_MAX_TAP_KEYS];
} macro_payload_owned_holds_t;

typedef struct {
    macro_payload_ir_opcode_t opcode;
    const uint8_t            *bytes;
    uint32_t                  value;
    uint8_t                   length;
} macro_payload_ir_step_t;

typedef enum {
    MACRO_PAYLOAD_PHASE_IDLE = 0,
    MACRO_PAYLOAD_PHASE_READY,
    MACRO_PAYLOAD_PHASE_WAITING,
    MACRO_PAYLOAD_PHASE_TEXT_PRESS,
    MACRO_PAYLOAD_PHASE_TRANSIENT_RELEASE,
    MACRO_PAYLOAD_PHASE_SECOND_STROKE,
    MACRO_PAYLOAD_PHASE_CLEANUP,
    MACRO_PAYLOAD_PHASE_UNICODE_PRESS,
    MACRO_PAYLOAD_PHASE_UNICODE_FINISH,
    MACRO_PAYLOAD_PHASE_UNICODE_RESTORE,
} macro_payload_phase_t;

typedef struct {
    uint8_t keys[3];
    uint8_t count;
    uint8_t mods;
} macro_payload_unicode_tap_t;

typedef struct {
    bool                          unicode_active;
    bool                          protected;
    bool                          unicode_cancel_sent;
    bool                          unicode_neutral_sent;
    bool                          unicode_caps_toggled;
    uint8_t                       unicode_mode;
    uint8_t                       unicode_mods;
    uint8_t                       unicode_index;
    uint8_t                       unicode_count;
    uint8_t                       unicode_settle_after; // Taps that start entry; the host settles after them.
    macro_payload_phase_t         unicode_next;
    macro_payload_unicode_tap_t   unicode_taps[12];
    owned_keycode_lease_t         unicode_alt;
    const host_layout_t          *layout;
    bool                          layout_iso;
    uint16_t                      second_stroke;
    macro_payload_phase_t         stroke_next;
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

// Suppressed ordinary usages stay suppressed until their live owners release
// them. They never acquire a new host press edge just because playback ended.
static uint8_t macro_suppressed_keys[32];
static bool    macro_capture_suppressed;

static void macro_payload_lease_keys(const owned_keycode_lease_t *lease, uint8_t keys[32]) {
    if (lease->active && lease->has_basic && IS_BASIC_KEYCODE(lease->basic)) keys[lease->basic / 8u] |= (uint8_t)(1u << (lease->basic % 8u));
}

static void macro_payload_output_keys(uint8_t keys[32]) {
    for (uint8_t i = 0u; i < macro_payload_engine.holds.balance.count; i++)
        macro_payload_lease_keys(&macro_payload_engine.holds.leases[i], keys);
    for (uint8_t i = 0u; i < macro_payload_engine.transient_count; i++)
        macro_payload_lease_keys(&macro_payload_engine.transient_leases[i], keys);
}

// QMK filters a copy: physical and managed ownership keeps processing releases.
static void macro_payload_project_keys(uint8_t keys[32]) {
    if (macro_capture_suppressed)
        for (uint8_t i = 0u; i < 32u; i++)
            macro_suppressed_keys[i] = keys[i];
    if (macro_payload_engine_protected()) {
        for (uint8_t i = 0u; i < 32u; i++)
            keys[i] = 0u;
        macro_payload_output_keys(keys);
    } else {
        for (uint8_t i = 0u; i < 32u; i++) {
            macro_suppressed_keys[i] &= keys[i];
            keys[i] &= (uint8_t)~macro_suppressed_keys[i];
        }
        // A later macro may reuse a usage whose physical owner is still
        // suppressed. Its explicit output still gets an independent edge.
        macro_payload_output_keys(keys);
    }
}

static bool macro_payload_filter_needed(void) {
    if (macro_payload_engine_protected() || macro_capture_suppressed) return true;
    for (uint8_t i = 0u; i < sizeof(macro_suppressed_keys); i++)
        if (macro_suppressed_keys[i]) return true;
    return false;
}

void keyboard_report_keys_filter_user(report_keyboard_t *report) {
    if (!macro_payload_filter_needed()) return;
    uint8_t keys[32] = {0};
    for (uint8_t i = 0u; i < sizeof(report->keys); i++)
        keys[report->keys[i] / 8u] |= (uint8_t)(1u << (report->keys[i] % 8u));
    keys[0] &= (uint8_t)~1u;
    macro_payload_project_keys(keys);
    uint8_t index = 0u;
    for (uint16_t key = 1u; key < 256u && index < sizeof(report->keys); key++)
        if (keys[key / 8u] & (1u << (key % 8u))) report->keys[index++] = (uint8_t)key;
    while (index < sizeof(report->keys))
        report->keys[index++] = 0u;
}

#ifdef NKRO_ENABLE
void nkro_report_keys_filter_user(report_nkro_t *report) {
    if (!macro_payload_filter_needed()) return;
    uint8_t keys[32] = {0};
    for (uint8_t i = 0u; i < sizeof(report->bits); i++)
        keys[i] = report->bits[i];
    macro_payload_project_keys(keys);
    for (uint8_t i = 0u; i < sizeof(report->bits); i++)
        report->bits[i] = keys[i];
}
#endif

bool keyboard_report_mods_override_user(uint8_t *mods) {
    if (macro_payload_engine.unicode_active) {
        *mods = macro_payload_engine.unicode_mods;
        return true;
    }
    if (!macro_payload_engine_protected()) return false;
    *mods = 0u;
    for (uint8_t i = 0u; i < macro_payload_engine.holds.balance.count; i++)
        *mods |= macro_payload_engine.holds.leases[i].mods;
    for (uint8_t i = 0u; i < macro_payload_engine.transient_count; i++)
        *mods |= macro_payload_engine.transient_leases[i].mods;
    return true;
}

static bool macro_payload_key_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    bool ok = owned_keycode_acquire(keycode, lease);
    // A colliding live usage may not change the ledger's aggregate report.
    // The macro's own lease still changes its projected output.
    if (macro_payload_filter_needed()) send_keyboard_report();
    return ok;
}

static bool macro_payload_key_release(owned_keycode_lease_t *lease) {
    bool ok = owned_keycode_release(lease);
    if (macro_payload_filter_needed()) send_keyboard_report();
    return ok;
}

__attribute__((weak)) uint32_t macro_payload_host_setting(void) {
    return noah_setting(NOAH_SETTING_UNICODE_HOST_MODE, 0u);
}

__attribute__((weak)) uint8_t macro_payload_host_os(void) {
    return noah_host_effective(macro_payload_host_setting(), noah_host_detected());
}

typedef struct {
    const host_layout_t *layout;
    uint8_t              unicode_mode; // 0, or the effective OS whose entry method types scalars.
    bool                 iso;
} macro_payload_host_t;

// Unicode entry types its digits, and U on Windows and Linux, through the
// layout, so each must be one stroke there.
static bool macro_payload_unicode_entry_typeable(const host_layout_t *layout, uint8_t mode) {
    static const char entry_chars[] = "0123456789abcdefu";
    for (uint8_t i = 0u; i < (mode == NOAH_HOST_MACOS ? 16u : 17u); i++) {
        uint16_t strokes[2];
        if (!host_layout_lookup(layout, (uint8_t)entry_chars[i], strokes) || strokes[1]) return false;
    }
    return true;
}

// The host is latched at playback start, so a later setting or detection
// change cannot switch layout or entry method mid-macro.
static macro_payload_host_t macro_payload_host_resolve(void) {
    uint32_t             setting = macro_payload_host_setting();
    uint8_t              os      = macro_payload_host_os();
    const host_layout_t *layout  = host_layout_get(noah_host_layout_id(setting));
    bool                 unicode = false;

    if (!layout) layout = host_layout_get(HOST_LAYOUT_US);
    if (os == NOAH_HOST_MACOS) {
        // macOS hexadecimal entry needs the Unicode Hex Input source: the
        // layout naming it, or US with the Unicode switch on.
        unicode = (layout->flags & HOST_LAYOUT_FLAG_UNICODE_HEX_INPUT) || (layout->id == HOST_LAYOUT_US && (setting & NOAH_HOST_UNICODE_ENABLED));
    } else if (os == NOAH_HOST_WINDOWS || os == NOAH_HOST_LINUX) {
        unicode = (setting & NOAH_HOST_UNICODE_ENABLED) != 0u;
    }
    if (unicode && !macro_payload_unicode_entry_typeable(layout, os)) unicode = false;
    return (macro_payload_host_t){
        .layout       = layout,
        .unicode_mode = unicode ? os : 0u,
        .iso          = (setting & NOAH_HOST_MACOS_ISO) && (layout->flags & HOST_LAYOUT_FLAG_MACOS),
    };
}

__attribute__((weak)) bool macro_payload_unicode_caps_lock(void) {
#ifdef QMK_REPORT_MODS_OVERRIDE
    return host_keyboard_led_state().caps_lock;
#else
    return false;
#endif
}

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
    return (uint8_t)(macro_payload_engine.holds.balance.count + macro_payload_engine.transient_count + (macro_payload_engine.unicode_alt.active ? 1u : 0u));
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
        case MACRO_PAYLOAD_IR_OP_UNICODE:
            if ((size_t)(end - current) < 3u) return false;
            step->value = (uint32_t)current[0] | ((uint32_t)current[1] << 8u) | ((uint32_t)current[2] << 16u);
            current += 3;
            if (step->value < 0xA0u || step->value > 0x10FFFFu || (step->value >= 0xD800u && step->value <= 0xDFFFu)) return false;
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

static bool macro_payload_ir_preflight(const macro_payload_ir_t *ir, const macro_payload_host_t *host, bool *needs_unicode) {
    const uint8_t               *cursor;
    const uint8_t               *end;
    macro_payload_hold_balance_t balance = {0};

    if (!ir || ir->length > sizeof(ir->bytes) || ir->protection > 2u) {
        return false;
    }

    cursor = ir->bytes;
    end    = ir->bytes + ir->length;
    while (cursor < end) {
        macro_payload_ir_step_t step;

        if (!macro_payload_ir_next(&cursor, end, &step)) {
            return false;
        }
        uint16_t strokes[2];
        if (step.opcode == MACRO_PAYLOAD_IR_OP_TEXT) {
            for (uint8_t i = 0u; i < step.length; i++)
                if (!host_layout_lookup(host->layout, step.bytes[i], strokes)) return false;
        }
        // A scalar the layout cannot type needs Unicode entry, which an
        // ordinary key held across it would corrupt.
        if (step.opcode == MACRO_PAYLOAD_IR_OP_UNICODE && !host_layout_lookup(host->layout, step.value, strokes)) {
            *needs_unicode = true;
            if (host->unicode_mode < 1u || host->unicode_mode > 3u) return false;
            for (uint8_t i = 0u; i < balance.count; i++)
                if (balance.keycodes[i] < KC_LEFT_CTRL) return false;
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
    if (!macro_payload_key_acquire(keycode, &holds->leases[index])) {
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
    released = macro_payload_key_release(&holds->leases[index]);
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

    if (macro_payload_engine_protected()) {
        // Cleanup has released every macro lease. Capture only remaining live
        // ordinary usages, send the final neutral output, then restore live mods.
        macro_capture_suppressed = true;
        send_keyboard_report();
        macro_capture_suppressed = false;
        macro_payload_engine     = (macro_payload_engine_t){0};
        send_keyboard_report();
    } else
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
        macro_payload_engine.transient_count = (uint8_t)(index + 1u);
        if (!macro_payload_key_acquire(keycodes[index], &macro_payload_engine.transient_leases[index])) {
            macro_payload_engine.transient_count = index;
            return false;
        }
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

// An ISO-classified Mac exchanges these two keys (host-layouts-v1).
static uint8_t macro_payload_stroke_key(uint16_t stroke) {
    uint8_t key = HOST_LAYOUT_STROKE_KEYCODE(stroke);
    if (macro_payload_engine.layout_iso && key == KC_GRAVE) return KC_NONUS_BACKSLASH;
    if (macro_payload_engine.layout_iso && key == KC_NONUS_BACKSLASH) return KC_GRAVE;
    return key;
}

static uint8_t macro_payload_stroke_keys(uint16_t stroke, uint8_t keys[3]) {
    uint8_t count = 0u;
    if (stroke & HOST_LAYOUT_STROKE_SHIFT) keys[count++] = KC_LEFT_SHIFT;
    if (stroke & HOST_LAYOUT_STROKE_ALTGR) keys[count++] = KC_RIGHT_ALT;
    keys[count++] = macro_payload_stroke_key(stroke);
    return count;
}

// Types one character as the layout does: one stroke, or a dead key then a
// second stroke.
static void macro_payload_start_strokes(const uint16_t strokes[2], uint32_t now, macro_payload_phase_t next) {
    uint8_t keys[3];
    uint8_t count                      = macro_payload_stroke_keys(strokes[0], keys);
    macro_payload_engine.second_stroke = strokes[1];
    macro_payload_engine.stroke_next   = next;
    macro_payload_start_transient(keys, count, now, macro_payload_engine.text_interval, macro_payload_engine.text_interval, strokes[1] ? MACRO_PAYLOAD_PHASE_SECOND_STROKE : next);
}

static void macro_payload_start_second_stroke(uint32_t now) {
    uint8_t keys[3];
    uint8_t count                      = macro_payload_stroke_keys(macro_payload_engine.second_stroke, keys);
    macro_payload_engine.second_stroke = 0u;
    macro_payload_start_transient(keys, count, now, TAP_CODE_DELAY, macro_payload_engine.text_interval, macro_payload_engine.stroke_next);
}

// The host's entry method needs a moment once entry starts, as QMK's
// UNICODE_TYPE_DELAY allows; the taps after it go at the macro's text pace.
#define MACRO_PAYLOAD_UNICODE_PACE_MS 10u

static void macro_payload_unicode_add_tap(uint8_t key, uint8_t mods) {
    macro_payload_unicode_tap_t *tap = &macro_payload_engine.unicode_taps[macro_payload_engine.unicode_count++];
    *tap                             = (macro_payload_unicode_tap_t){.keys = {key}, .count = 1u, .mods = mods};
}

// An entry character typed through the layout; its Shift or AltGr joins the
// tap's report modifiers, which the override otherwise masks.
static void macro_payload_unicode_add_char(char c, uint8_t mods) {
    uint16_t strokes[2] = {0u, 0u};
    (void)host_layout_lookup(macro_payload_engine.layout, (uint8_t)c, strokes);
    if (strokes[0] & HOST_LAYOUT_STROKE_SHIFT) mods |= MOD_BIT(KC_LEFT_SHIFT);
    if (strokes[0] & HOST_LAYOUT_STROKE_ALTGR) mods |= MOD_BIT(KC_RIGHT_ALT);
    macro_payload_unicode_add_tap(macro_payload_stroke_key(strokes[0]), mods);
}

static void macro_payload_unicode_add_hex(uint32_t value, uint8_t digits, uint8_t mods) {
    static const char hex[] = "0123456789abcdef";
    for (uint8_t i = digits; i > 0u; i--)
        macro_payload_unicode_add_char(hex[(value >> ((i - 1u) * 4u)) & 15u], mods);
}

static void macro_payload_start_unicode(uint32_t scalar, uint32_t now, macro_payload_phase_t next) {
    uint8_t mode = macro_payload_engine.unicode_mode;
    if (mode < 1u || mode > 3u) {
        macro_payload_begin_runtime_error();
        return;
    }
    macro_payload_engine.unicode_count        = 0u;
    macro_payload_engine.unicode_index        = 0u;
    macro_payload_engine.unicode_next         = next;
    macro_payload_engine.unicode_cancel_sent  = false;
    macro_payload_engine.unicode_neutral_sent = false;
    macro_payload_engine.unicode_settle_after = 0u;
    bool caps                                 = mode == 3u && macro_payload_unicode_caps_lock();
    if (caps) macro_payload_unicode_add_tap(KC_CAPS_LOCK, 0u);
    if (mode == 1u) {
        if (scalar > 0xFFFFu) {
            scalar -= 0x10000u;
            macro_payload_unicode_add_hex(0xD800u | (scalar >> 10u), 4u, MOD_BIT(KC_LEFT_ALT));
            macro_payload_unicode_add_hex(0xDC00u | (scalar & 0x3FFu), 4u, MOD_BIT(KC_LEFT_ALT));
        } else
            macro_payload_unicode_add_hex(scalar, 4u, MOD_BIT(KC_LEFT_ALT));
    } else {
        if (mode == 2u) {
            macro_payload_unicode_add_tap(KC_RIGHT_ALT, MOD_BIT(KC_RIGHT_ALT));
            macro_payload_unicode_add_char('u', 0u);
        } else {
            uint16_t strokes[2] = {0u, 0u};
            (void)host_layout_lookup(macro_payload_engine.layout, 'u', strokes);
            macro_payload_unicode_tap_t *tap = &macro_payload_engine.unicode_taps[macro_payload_engine.unicode_count++];
            *tap                             = (macro_payload_unicode_tap_t){.keys = {KC_LEFT_CTRL, KC_LEFT_SHIFT, macro_payload_stroke_key(strokes[0])}, .count = 3u, .mods = MOD_BIT(KC_LEFT_CTRL) | MOD_BIT(KC_LEFT_SHIFT)};
        }
        macro_payload_engine.unicode_settle_after = macro_payload_engine.unicode_count;
        uint8_t digits = scalar > 0xFFFFFu ? 6u : scalar > 0xFFFFu ? 5u : 4u;
        // WinCompose treats an initial hex letter as a compose sequence.
        if (mode == 2u && ((scalar >> ((digits - 1u) * 4u)) & 15u) > 9u) macro_payload_unicode_add_char('0', 0u);
        macro_payload_unicode_add_hex(scalar, digits, 0u);
        macro_payload_unicode_add_tap(mode == 2u ? KC_ENTER : KC_SPACE, 0u);
    }
    if (caps) macro_payload_unicode_add_tap(KC_CAPS_LOCK, 0u);
    macro_payload_engine.unicode_active = true;
    macro_payload_engine.unicode_mods   = 0u;
    send_keyboard_report();
    if (mode == 1u) {
        macro_payload_engine.unicode_mods = MOD_BIT(KC_LEFT_ALT);
        if (!macro_payload_key_acquire(KC_LEFT_ALT, &macro_payload_engine.unicode_alt)) {
            macro_payload_begin_runtime_error();
            return;
        }
        send_keyboard_report();
    }
    // Holding Option starts macOS entry; Windows and Linux start it with taps.
    macro_payload_schedule_wait(now, mode == 1u ? MACRO_PAYLOAD_UNICODE_PACE_MS : macro_payload_engine.text_interval, MACRO_PAYLOAD_PHASE_UNICODE_PRESS);
}

static void macro_payload_unicode_press(uint32_t now) {
    if (macro_payload_engine.unicode_index == macro_payload_engine.unicode_count) {
        macro_payload_engine.phase = MACRO_PAYLOAD_PHASE_UNICODE_FINISH;
        return;
    }
    macro_payload_unicode_tap_t *tap  = &macro_payload_engine.unicode_taps[macro_payload_engine.unicode_index++];
    macro_payload_engine.unicode_mods = tap->mods;
    send_keyboard_report();
    uint32_t after = macro_payload_engine.unicode_index == macro_payload_engine.unicode_settle_after ? MACRO_PAYLOAD_UNICODE_PACE_MS : macro_payload_engine.text_interval;
    macro_payload_start_transient(tap->keys, tap->count, now, macro_payload_engine.text_interval, after, MACRO_PAYLOAD_PHASE_UNICODE_PRESS);
    if (tap->keys[0] == KC_CAPS_LOCK && macro_payload_engine.phase != MACRO_PAYLOAD_PHASE_CLEANUP) macro_payload_engine.unicode_caps_toggled = !macro_payload_engine.unicode_caps_toggled;
}

static void macro_payload_unicode_finish(uint32_t now) {
    macro_payload_engine.unicode_mods         = 0u;
    macro_payload_engine.unicode_neutral_sent = true;
    if (macro_payload_engine.unicode_alt.active) (void)macro_payload_key_release(&macro_payload_engine.unicode_alt);
    send_keyboard_report();
    // A held physical Option must not hide the release committing macOS input:
    // the neutral report goes out on its own before live modifiers return.
    macro_payload_schedule_wait(now, macro_payload_engine.text_interval, MACRO_PAYLOAD_PHASE_UNICODE_RESTORE);
}

static void macro_payload_unicode_restore(uint32_t now) {
    macro_payload_engine.unicode_active = false;
    send_keyboard_report();
    macro_payload_schedule_wait(now, macro_payload_engine.text_interval, macro_payload_engine.unicode_next);
}

static void macro_payload_start_text_char(uint32_t now) {
    uint16_t strokes[2];

    if (!macro_payload_engine.ir || macro_payload_engine.text_remaining == 0u) {
        macro_payload_begin_runtime_error();
        return;
    }

    uint8_t ascii_code = macro_payload_engine.ir->bytes[macro_payload_engine.text_cursor++];
    macro_payload_engine.text_remaining--;
    if (!host_layout_lookup(macro_payload_engine.layout, ascii_code, strokes)) {
        macro_payload_begin_runtime_error();
        return;
    }
    macro_payload_start_strokes(strokes, now, macro_payload_engine.text_remaining ? MACRO_PAYLOAD_PHASE_TEXT_PRESS : MACRO_PAYLOAD_PHASE_READY);
}

static void macro_payload_release_transients(uint32_t now) {
    bool ok = true;

    while (macro_payload_engine.transient_count > 0u) {
        macro_payload_engine.transient_count--;
        if (!macro_payload_key_release(&macro_payload_engine.transient_leases[macro_payload_engine.transient_count])) {
            ok = false;
        }
    }
    if (macro_payload_engine.unicode_active && macro_payload_engine.unicode_mode != 1u) {
        macro_payload_engine.unicode_mods = 0u;
        send_keyboard_report();
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
        case MACRO_PAYLOAD_IR_OP_UNICODE: {
            uint16_t strokes[2];
            if (host_layout_lookup(macro_payload_engine.layout, step.value, strokes))
                macro_payload_start_strokes(strokes, now, MACRO_PAYLOAD_PHASE_READY);
            else
                macro_payload_start_unicode(step.value, now, MACRO_PAYLOAD_PHASE_READY);
            break;
        }
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
        (void)macro_payload_key_release(&macro_payload_engine.transient_leases[macro_payload_engine.transient_count]);
        return;
    }
    if (macro_payload_engine.unicode_active) {
        if (!macro_payload_engine.unicode_cancel_sent && macro_payload_engine.unicode_mode != 1u) {
            static const uint8_t escape[]            = {KC_ESCAPE};
            macro_payload_engine.unicode_cancel_sent = true;
            macro_payload_engine.unicode_mods        = 0u;
            send_keyboard_report();
            macro_payload_start_transient(escape, 1u, timer_read32(), MACRO_PAYLOAD_UNICODE_PACE_MS, MACRO_PAYLOAD_UNICODE_PACE_MS, MACRO_PAYLOAD_PHASE_CLEANUP);
            return;
        }
        if (macro_payload_engine.unicode_caps_toggled) {
            static const uint8_t caps[]               = {KC_CAPS_LOCK};
            macro_payload_engine.unicode_caps_toggled = false;
            macro_payload_engine.unicode_mods         = 0u;
            send_keyboard_report();
            macro_payload_start_transient(caps, 1u, timer_read32(), MACRO_PAYLOAD_UNICODE_PACE_MS, MACRO_PAYLOAD_UNICODE_PACE_MS, MACRO_PAYLOAD_PHASE_CLEANUP);
            return;
        }
        if (!macro_payload_engine.unicode_neutral_sent) {
            macro_payload_engine.unicode_neutral_sent = true;
            macro_payload_engine.unicode_mods         = 0u;
            if (macro_payload_engine.unicode_alt.active) (void)macro_payload_key_release(&macro_payload_engine.unicode_alt);
            send_keyboard_report();
            macro_payload_schedule_wait(timer_read32(), MACRO_PAYLOAD_UNICODE_PACE_MS, MACRO_PAYLOAD_PHASE_CLEANUP);
            return;
        }
        macro_payload_engine.unicode_active = false;
        send_keyboard_report();
        return;
    }
    if (macro_payload_engine.holds.balance.count > 0u) {
        macro_payload_engine.holds.balance.count--;
        (void)macro_payload_key_release(&macro_payload_engine.holds.leases[macro_payload_engine.holds.balance.count]);
        return;
    }
    macro_payload_finish(macro_payload_engine.terminal_result == MACRO_PAYLOAD_FINISH_NONE ? MACRO_PAYLOAD_FINISH_RUNTIME_ERROR : macro_payload_engine.terminal_result);
}

macro_payload_start_result_t macro_payload_start_ir(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context) {
    if (macro_payload_engine.phase != MACRO_PAYLOAD_PHASE_IDLE) {
        macro_payload_increment_u16(&macro_payload_diagnostics.busy_rejection_count);
        return MACRO_PAYLOAD_START_BUSY;
    }
    const macro_payload_host_t host          = macro_payload_host_resolve();
    bool                       needs_unicode = false;
    if (!macro_payload_ir_preflight(ir, &host, &needs_unicode)) {
        return MACRO_PAYLOAD_START_INVALID;
    }
    if (ir->length == 0u) {
        return MACRO_PAYLOAD_START_EMPTY;
    }

    macro_payload_engine = (macro_payload_engine_t){
        .protected      = ir->protection == 1u || (ir->protection == 0u && needs_unicode),
        .unicode_mode   = host.unicode_mode,
        .layout         = host.layout,
        .layout_iso     = host.iso,
        .ir             = ir,
        .phase          = MACRO_PAYLOAD_PHASE_READY,
        .text_interval  = text_output == MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED ? interval : TAP_CODE_DELAY,
        .source         = source,
        .slot           = slot,
        .finish         = finish,
        .finish_context = context,
    };
    if (macro_payload_engine_protected()) {
        send_keyboard_report();
        macro_payload_schedule_wait(timer_read32(), MACRO_PAYLOAD_UNICODE_PACE_MS, MACRO_PAYLOAD_PHASE_READY);
    }
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
        case MACRO_PAYLOAD_PHASE_SECOND_STROKE:
            macro_payload_start_second_stroke(now);
            break;
        case MACRO_PAYLOAD_PHASE_UNICODE_PRESS:
            macro_payload_unicode_press(now);
            break;
        case MACRO_PAYLOAD_PHASE_UNICODE_FINISH:
            macro_payload_unicode_finish(now);
            break;
        case MACRO_PAYLOAD_PHASE_UNICODE_RESTORE:
            macro_payload_unicode_restore(now);
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

bool macro_payload_engine_protected(void) {
    return macro_payload_engine.phase != MACRO_PAYLOAD_PHASE_IDLE && macro_payload_engine.protected;
}

void macro_payload_engine_init(void) {
    if (macro_payload_engine.phase != MACRO_PAYLOAD_PHASE_IDLE) {
        macro_payload_begin_cleanup(MACRO_PAYLOAD_FINISH_CANCELLED);
        return;
    }
    macro_payload_engine      = (macro_payload_engine_t){0};
    macro_payload_diagnostics = (macro_payload_debug_snapshot_t){0};
    for (uint8_t i = 0u; i < sizeof(macro_suppressed_keys); i++)
        macro_suppressed_keys[i] = 0u;
    macro_capture_suppressed = false;
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
