// ────────────────────────────────────────────────────────────────────────────
// PD Runtime
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../defs/pd_modes.h"
#include "../policy/pointer_layer_policy.h"
#include "../../compat/qmk_auto_mouse_contract.h"
#include "../../compat/qmk_pointing_contract.h"

#if defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE)
#    if !defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS)
#        error "NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS must be defined when NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE is set"
#    endif

#    if !defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ARM_IDLE_MS)
#        error "NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ARM_IDLE_MS must be defined when NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE is set"
#    endif

#    if !defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX)
#        error "NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX must be defined when NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE is set"
#    endif

static uint32_t pd_runtime_idle_noise_last_pointer_activity_ms;

static inline bool pd_runtime_report_has_motion(report_mouse_t report) {
    return report.x != 0 || report.y != 0 || report.h != 0 || report.v != 0;
}

static inline uint16_t pd_runtime_report_abs_total(report_mouse_t report) {
    uint16_t total = 0;

    total += (uint16_t)(report.x < 0 ? -report.x : report.x);
    total += (uint16_t)(report.y < 0 ? -report.y : report.y);
    total += (uint16_t)(report.h < 0 ? -report.h : report.h);
    total += (uint16_t)(report.v < 0 ? -report.v : report.v);

    return total;
}

static inline void pd_runtime_idle_noise_note_pointer_activity(void) {
    pd_runtime_idle_noise_last_pointer_activity_ms = timer_read32();
}

static inline uint32_t pd_runtime_idle_noise_any_activity_elapsed(void) {
    // Do not trust last_input_activity_elapsed() here: unsuppressed tiny
    // pointer drift can refresh it and prevent the long arm window from aging.
    uint32_t elapsed = timer_elapsed32(pd_runtime_idle_noise_last_pointer_activity_ms);
    uint32_t matrix  = last_matrix_activity_elapsed();

    if (matrix < elapsed) {
        elapsed = matrix;
    }

    return elapsed;
}
#endif

void noah_pointing_device_init_user(void) {
#if defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE)
    pd_runtime_idle_noise_note_pointer_activity();
#endif

#if defined(POINTING_DEVICE_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
    noah_qmk_contract_auto_mouse_set_layer(AUTO_MOUSE_DEFAULT_LAYER);
    noah_qmk_contract_auto_mouse_set_enable(true);
#endif
}

#if defined(POINTING_DEVICE_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)
bool noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)record;
    return pointer_layer_policy_is_mouse_record(keycode);
}
#else
bool noah_is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}
#endif // defined(POINTING_DEVICE_ENABLE) && defined(POINTING_DEVICE_AUTO_MOUSE_ENABLE)

report_mouse_t noah_pointing_device_task_user(report_mouse_t mouse_report) {
    report_mouse_t output = mouse_report;

#ifdef POINTING_DEVICE_ENABLE
    pd_mode_mask_t active_mode_id = pd_mode_local_active_snapshot();

#    if defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE)
    bool     has_motion = pd_runtime_report_has_motion(mouse_report);
    uint16_t abs_total  = has_motion ? pd_runtime_report_abs_total(mouse_report) : 0;

    if (active_mode_id != 0 || mouse_report.buttons != 0 || abs_total > NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX) {
        pd_runtime_idle_noise_note_pointer_activity();
    }

    if (active_mode_id == 0 && mouse_report.buttons == 0 && has_motion && last_input_activity_elapsed() >= NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS && pd_runtime_idle_noise_any_activity_elapsed() >= NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ARM_IDLE_MS && abs_total <= NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX) {
        output = (report_mouse_t){0};
        goto done;
    }
#    endif

    if (active_mode_id != 0) {
        const pd_mode_def_t *active_mode = pd_mode_lookup(active_mode_id);

        if (active_mode && active_mode->handler) {
            output = active_mode->handler(mouse_report);
            goto done;
        }
    }

    output = mouse_report;
#else
    output = mouse_report;
#endif

done:
    return output;
}

layer_state_t noah_layer_state_set_user(layer_state_t state) {
    layer_state_t result = state;

#ifdef POINTING_DEVICE_ENABLE
    bool auto_sniping_active = false;
#    if defined(CHARYBDIS_AUTO_SNIPING_ENABLE)
    auto_sniping_active = layer_state_cmp(state, CHARYBDIS_AUTO_SNIPING_LAYER);
#    endif
    pd_mode_set_auto_sniping_layer_active(auto_sniping_active);
    // Sniping can temporarily own CPI, so queue a re-apply of the active
    // pd-mode DPI policy after the layer-owned sniping state changes. Service
    // the actual hardware write on scan instead of from this layer hook.
    pd_mode_request_active_dpi_sync();
    result = pointer_layer_policy_apply(state);
#else
    result = state;
#endif
    return result;
}
