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

#    if !defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX)
#        error "NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX must be defined when NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE is set"
#    endif

static inline bool pd_runtime_report_has_motion(report_mouse_t report) {
    return report.x != 0 || report.y != 0 || report.h != 0 || report.v != 0;
}

static inline uint16_t pd_runtime_report_abs_total(report_mouse_t report) {
    uint16_t total    = 0;
    int16_t  deltas[] = {report.x, report.y, report.h, report.v};

    for (uint8_t i = 0; i < ARRAY_SIZE(deltas); ++i) {
        total += (uint16_t)(deltas[i] < 0 ? -deltas[i] : deltas[i]);
    }

    return total;
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
    const pd_mode_def_t *active_mode = pd_mode_lookup(snapshot.local.active_mode);

#    if defined(NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ENABLE)
    if (snapshot.local.active_mode == 0 && mouse_report.buttons == 0 && pd_runtime_report_has_motion(mouse_report) && last_input_activity_elapsed() >= NOAH_POINTING_IDLE_NOISE_SUPPRESSION_IDLE_MS && pd_runtime_report_abs_total(mouse_report) <= NOAH_POINTING_IDLE_NOISE_SUPPRESSION_ABS_MAX) {
        return (report_mouse_t){0};
    }
#    endif

    if (active_mode && active_mode->handler) {
        return active_mode->handler(mouse_report);
    }

    return mouse_report;
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
