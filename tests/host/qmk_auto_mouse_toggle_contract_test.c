// Runs QMK's own process_auto_mouse against
// noah_qmk_contract_auto_mouse_record_toggles: the contract must name exactly
// the records on which QMK flips its auto-mouse toggle, so the runtime's
// take-back undoes each flip and nothing else.

#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "quantum_keycodes.h"
#include "users/noah/lib/compat/qmk_auto_mouse_contract.h"

#define TARGET_LAYER 4u
#define OTHER_LAYER 2u

debug_config_t debug_config;
layer_state_t  layer_state;

bool layer_state_is(uint8_t layer) {
    return (layer_state & ((layer_state_t)1u << layer)) != 0;
}

void layer_on(uint8_t layer) {
    layer_state |= (layer_state_t)1u << layer;
}

void layer_off(uint8_t layer) {
    layer_state &= ~((layer_state_t)1u << layer);
}

uint16_t timer_read(void) {
    return 0;
}

uint16_t timer_elapsed(uint16_t last) {
    (void)last;
    return 0;
}

uint8_t get_oneshot_layer(void) {
    return 0;
}

static int failures;

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            fprintf(stderr, "test failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
            failures++;                                                           \
        }                                                                         \
    } while (0)

// One event through QMK, then the contract's verdict on it: whether QMK
// flipped its toggle must be exactly whether the contract says it did.
static void check_event(uint16_t keycode, bool pressed, uint8_t tap_count) {
    keyrecord_t record = {.event = {.key = {.row = 1, .col = 1}, .pressed = pressed, .time = 1, .type = KEY_EVENT}};
    record.tap.count   = tap_count;

    bool before = get_auto_mouse_toggle();
    process_auto_mouse(keycode, &record);
    bool flipped = get_auto_mouse_toggle() != before;

    if (flipped != noah_qmk_contract_auto_mouse_record_toggles(keycode, &record)) {
        fprintf(stderr, "contract drift: keycode 0x%04X %s tap %u, QMK %s its toggle\n", keycode, pressed ? "press" : "release", tap_count, flipped ? "flipped" : "kept");
        failures++;
    }
    if (flipped) {
        auto_mouse_toggle();
    }
}

static void check_tap(uint16_t keycode, uint8_t tap_count) {
    check_event(keycode, true, tap_count);
    check_event(keycode, false, tap_count);
}

static void check_keycodes(void) {
    const uint16_t keycodes[] = {TO(TARGET_LAYER), TO(OTHER_LAYER), TO(0), TG(TARGET_LAYER), TG(OTHER_LAYER), MO(TARGET_LAYER), OSL(TARGET_LAYER), LM(TARGET_LAYER, MOD_LCTL), KC_A};

    for (size_t index = 0; index < sizeof(keycodes) / sizeof(keycodes[0]); index++) {
        check_tap(keycodes[index], 0u);
    }
    for (uint8_t taps = 0; taps <= NOAH_QMK_TAPPING_TOGGLE + 1u; taps++) {
        check_tap(TT(TARGET_LAYER), taps);
        check_tap(TT(OTHER_LAYER), taps);
    }
}

int main(void) {
    set_auto_mouse_layer(TARGET_LAYER);
    set_auto_mouse_enable(true);

    // The flip the take-back exists for: TO(0) never clears it.
    check_tap(TO(TARGET_LAYER), 0u);
    keyrecord_t release = {.event = {.key = {.row = 1, .col = 1}, .pressed = false, .time = 1, .type = KEY_EVENT}};
    process_auto_mouse(TO(TARGET_LAYER), &release);
    process_auto_mouse(TO(0), &release);
    CHECK(get_auto_mouse_toggle());
    auto_mouse_toggle();

    check_keycodes();

    set_auto_mouse_enable(false);
    check_keycodes();

    if (failures) {
        return EXIT_FAILURE;
    }
    puts("qmk auto-mouse toggle contract tests passed");
    return EXIT_SUCCESS;
}
