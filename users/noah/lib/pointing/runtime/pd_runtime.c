// ────────────────────────────────────────────────────────────────────────────
// PD Runtime
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../defs/pd_modes.h"
#include "../policy/pointer_layer_policy.h"
#include "../../compat/qmk_auto_mouse_contract.h"
#include "../../compat/qmk_pointing_contract.h"

static uint16_t pd_runtime_report_abs_total(report_mouse_t report) {
    uint16_t total = 0;
    int16_t  deltas[] = {report.x, report.y, report.h, report.v};

    for (uint8_t i = 0; i < ARRAY_SIZE(deltas); ++i) {
        total += (uint16_t)(deltas[i] < 0 ? -deltas[i] : deltas[i]);
    }

    return total;
}

#if defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE)
#    ifndef NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS
#        define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS 1000
#    endif

#    ifndef NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX
#        define NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX 1
#    endif
#endif

static bool pd_runtime_should_suppress_idle_noise(report_mouse_t raw_report, pd_mode_mask_t active_mode) {
#if defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE)
    if (active_mode != 0 || raw_report.buttons != 0) {
        return false;
    }

    if (pd_runtime_report_abs_total(raw_report) > NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX) {
        return false;
    }

    return last_input_activity_elapsed() >= NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS;
#else
    (void)raw_report;
    (void)active_mode;
    return false;
#endif
}

#if defined(CONSOLE_ENABLE) && defined(NOAH_POINTING_MOTION_TRACE_ENABLE)
#    include "print.h"

#    ifndef NOAH_POINTING_MOTION_TRACE_RATE_LIMIT_MS
#        define NOAH_POINTING_MOTION_TRACE_RATE_LIMIT_MS 250
#    endif

typedef struct {
    bool     active;
    uint32_t start_time;
    uint32_t start_idle_ms;
    uint32_t last_activity_time;
    uint32_t last_log_time;
    uint16_t report_count;
    uint32_t raw_sum_abs;
    uint16_t raw_peak_abs;
    uint8_t  raw_peak_x;
    uint8_t  raw_peak_y;
    uint8_t  raw_peak_h;
    uint8_t  raw_peak_v;
} pd_runtime_trace_burst_t;

static bool pd_runtime_report_has_activity(report_mouse_t report) {
    return report.x != 0 || report.y != 0 || report.h != 0 || report.v != 0 || report.buttons != 0;
}

static uint8_t pd_runtime_abs_component(int16_t value) {
    return (uint8_t)(value < 0 ? -value : value);
}

static void pd_runtime_trace_reset_burst(pd_runtime_trace_burst_t *burst) {
    *burst = (pd_runtime_trace_burst_t){0};
}

static void pd_runtime_trace_begin_burst(pd_runtime_trace_burst_t *burst, uint32_t now, uint32_t idle_ms) {
    pd_runtime_trace_reset_burst(burst);
    burst->active             = true;
    burst->start_time         = now;
    burst->start_idle_ms      = idle_ms;
    burst->last_activity_time = now;
}

static void pd_runtime_trace_update_burst(pd_runtime_trace_burst_t *burst, report_mouse_t raw_report, uint32_t now) {
    uint16_t raw_abs = pd_runtime_report_abs_total(raw_report);
    uint8_t  raw_x   = pd_runtime_abs_component(raw_report.x);
    uint8_t  raw_y   = pd_runtime_abs_component(raw_report.y);
    uint8_t  raw_h   = pd_runtime_abs_component(raw_report.h);
    uint8_t  raw_v   = pd_runtime_abs_component(raw_report.v);

    burst->last_activity_time = now;
    burst->report_count++;
    burst->raw_sum_abs += raw_abs;

    if (raw_abs > burst->raw_peak_abs) {
        burst->raw_peak_abs = raw_abs;
    }
    if (raw_x > burst->raw_peak_x) {
        burst->raw_peak_x = raw_x;
    }
    if (raw_y > burst->raw_peak_y) {
        burst->raw_peak_y = raw_y;
    }
    if (raw_h > burst->raw_peak_h) {
        burst->raw_peak_h = raw_h;
    }
    if (raw_v > burst->raw_peak_v) {
        burst->raw_peak_v = raw_v;
    }
}

static void pd_runtime_trace_motion(report_mouse_t raw_report, report_mouse_t output_report, pd_mode_mask_t active_mode, bool caught) {
    static pd_runtime_trace_burst_t burst   = {0};
    uint32_t                        idle_ms = last_input_activity_elapsed();
    uint32_t                        now     = timer_read32();

    if (!pd_runtime_report_has_activity(raw_report) && !pd_runtime_report_has_activity(output_report)) {
        if (burst.active && timer_elapsed32(burst.last_activity_time) >= NOAH_POINTING_MOTION_TRACE_RATE_LIMIT_MS) {
            pd_runtime_trace_reset_burst(&burst);
        }
        return;
    }

    if (!burst.active || timer_elapsed32(burst.last_activity_time) >= NOAH_POINTING_MOTION_TRACE_RATE_LIMIT_MS) {
        pd_runtime_trace_begin_burst(&burst, now, idle_ms);
    }

    pd_runtime_trace_update_burst(&burst, raw_report, now);

    if (burst.report_count != 1u && timer_elapsed32(burst.last_log_time) < NOAH_POINTING_MOTION_TRACE_RATE_LIMIT_MS) {
        return;
    }

    burst.last_log_time = now;
    uprintf("Pointing motion trace idle=%lu burst=%lu reports=%u raw=(%d,%d,%d,%d btn=0x%02X abs=%u) raw_sum=%lu raw_peak=%u raw_axes=(%u,%u,%u,%u) out=(%d,%d,%d,%d btn=0x%02X abs=%u) caught=%u mode=0x%04X sniping=%u automouse=%u\n",
            (unsigned long)burst.start_idle_ms,
            (unsigned long)(now - burst.start_time),
            (unsigned int)burst.report_count,
            (int)raw_report.x,
            (int)raw_report.y,
            (int)raw_report.h,
            (int)raw_report.v,
            (unsigned int)raw_report.buttons,
            (unsigned int)pd_runtime_report_abs_total(raw_report),
            (unsigned long)burst.raw_sum_abs,
            (unsigned int)burst.raw_peak_abs,
            (unsigned int)burst.raw_peak_x,
            (unsigned int)burst.raw_peak_y,
            (unsigned int)burst.raw_peak_h,
            (unsigned int)burst.raw_peak_v,
            (int)output_report.x,
            (int)output_report.y,
            (int)output_report.h,
            (int)output_report.v,
            (unsigned int)output_report.buttons,
            (unsigned int)pd_runtime_report_abs_total(output_report),
            caught ? 1u : 0u,
            (unsigned int)active_mode,
            noah_qmk_contract_pointer_sniping_enabled() ? 1u : 0u,
            noah_qmk_contract_auto_mouse_active() ? 1u : 0u);
}
#else
static void pd_runtime_trace_motion(report_mouse_t raw_report, report_mouse_t output_report, pd_mode_mask_t active_mode, bool caught) {
    (void)raw_report;
    (void)output_report;
    (void)active_mode;
    (void)caught;
}
#endif

#if defined(POINTING_DEVICE_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
void noah_pointing_device_init_user(void) {
    noah_qmk_contract_auto_mouse_set_layer(AUTO_MOUSE_DEFAULT_LAYER);
    noah_qmk_contract_auto_mouse_set_enable(true);
}

bool noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)record;
    return pointer_layer_policy_is_mouse_record(keycode);
}
#else
void noah_pointing_device_init_user(void) {}

bool noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}
#endif // defined(POINTING_DEVICE_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

report_mouse_t noah_pointing_device_task_user(report_mouse_t mouse_report) {
#ifdef POINTING_DEVICE_ENABLE
    pd_mode_snapshot_t   snapshot    = pd_mode_snapshot();
    report_mouse_t       output      = mouse_report;
    bool                 caught      = false;
    const pd_mode_def_t *active_mode = pd_mode_lookup(snapshot.local.active_mode);

    if (pd_runtime_should_suppress_idle_noise(mouse_report, snapshot.local.active_mode)) {
        output = (report_mouse_t){0};
        caught = true;
    } else if (active_mode && active_mode->handler) {
        output = active_mode->handler(mouse_report);
    }

    pd_runtime_trace_motion(mouse_report, output, snapshot.local.active_mode, caught);
    return output;
#else
    return mouse_report;
#endif
}

layer_state_t noah_layer_state_set_user(layer_state_t state) {
#ifdef POINTING_DEVICE_ENABLE
#    if defined(CHARYBDIS_AUTO_SNIPING_ENABLE)
    noah_qmk_contract_pointer_set_sniping_enabled(layer_state_cmp(state, CHARYBDIS_AUTO_SNIPING_LAYER));
#    else
    noah_qmk_contract_pointer_set_sniping_enabled(false);
#    endif
    // Sniping can temporarily own CPI, so always re-apply the active pd-mode DPI
    // policy after the layer-owned sniping state changes.
    pd_mode_apply_active_dpi();
    return pointer_layer_policy_apply(state);
#else
    return state;
#endif
}
