#include <stddef.h>
#include <stdio.h>

#ifdef QMK_CONTRACT_USE_STUB
#    include "qmk_stub.h"
#    include "send_string.h"
#else
#    include "quantum_keycodes.h"
#    include "send_string.h"
#    include "action.h"
#    include "report.h"
#    include "keycode.h"
#endif

_Static_assert(offsetof(keyrecord_t, event) == 0, "keyrecord_t.event must remain the first field");

static void qmk_contract_field_smoke(void) {
    keyrecord_t    record = {0};
    report_mouse_t report = {0};

    (void)record.event.key.row;
    (void)record.event.key.col;
    (void)record.event.pressed;

    (void)report.buttons;
    (void)report.x;
    (void)report.y;
    (void)report.h;
    (void)report.v;
}

int main(void) {
    qmk_contract_field_smoke();

    printf("SAFE_RANGE=0x%04X\n", (unsigned)SAFE_RANGE);
    printf("QK_MODS=0x%04X\n", (unsigned)QK_MODS);
    printf("QK_RMODS_MIN=0x%04X\n", (unsigned)QK_RMODS_MIN);
    printf("QK_LAYER_TAP=0x%04X\n", (unsigned)QK_LAYER_TAP);
    printf("QK_MOMENTARY=0x%04X\n", (unsigned)QK_MOMENTARY);
    printf("QK_MOUSE_BUTTON_1=0x%04X\n", (unsigned)QK_MOUSE_BUTTON_1);
    printf("QK_MACRO_0=0x%04X\n", (unsigned)QK_MACRO_0);
    printf("MOD_BIT_KC_LEFT_SHIFT=0x%02X\n", (unsigned)MOD_BIT(KC_LEFT_SHIFT));
    printf("MO_1=0x%04X\n", (unsigned)MO(1));
    printf("IS_QK_MOMENTARY_MO_1=%d\n", IS_QK_MOMENTARY(MO(1)) ? 1 : 0);
    printf("QK_MOMENTARY_GET_LAYER_MO_1=%u\n", (unsigned)QK_MOMENTARY_GET_LAYER(MO(1)));
    printf("LT_2_KC_C=0x%04X\n", (unsigned)LT(2, KC_C));
    printf("IS_QK_LAYER_TAP_LT_2_KC_C=%d\n", IS_QK_LAYER_TAP(LT(2, KC_C)) ? 1 : 0);
    printf("QK_LAYER_TAP_GET_LAYER_LT_2_KC_C=%u\n", (unsigned)QK_LAYER_TAP_GET_LAYER(LT(2, KC_C)));
    printf("QK_LAYER_TAP_GET_TAP_KEYCODE_LT_2_KC_C=0x%02X\n", (unsigned)QK_LAYER_TAP_GET_TAP_KEYCODE(LT(2, KC_C)));
    printf("G_KC_C=0x%04X\n", (unsigned)G(KC_C));
    printf("IS_QK_MODS_G_KC_C=%d\n", IS_QK_MODS(G(KC_C)) ? 1 : 0);
    printf("QK_MODS_GET_BASIC_KEYCODE_G_KC_C=0x%02X\n", (unsigned)QK_MODS_GET_BASIC_KEYCODE(G(KC_C)));
    printf("MS_BTN1=0x%04X\n", (unsigned)MS_BTN1);
    printf("IS_MOUSEKEY_BUTTON_MS_BTN1=%d\n", IS_MOUSEKEY_BUTTON(MS_BTN1) ? 1 : 0);
    printf("IS_QK_MACRO_QK_MACRO_0=%d\n", IS_QK_MACRO((uint16_t)QK_MACRO_0) ? 1 : 0);
    printf("SS_QMK_PREFIX=%u\n", (unsigned)SS_QMK_PREFIX);
    printf("SS_TAP_CODE=%u\n", (unsigned)SS_TAP_CODE);
    printf("SS_DOWN_CODE=%u\n", (unsigned)SS_DOWN_CODE);
    printf("SS_UP_CODE=%u\n", (unsigned)SS_UP_CODE);
    printf("SS_DELAY_CODE=%u\n", (unsigned)SS_DELAY_CODE);

    return 0;
}
