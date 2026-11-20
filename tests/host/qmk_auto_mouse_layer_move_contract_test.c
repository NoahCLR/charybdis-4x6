// Runs QMK's own auto-mouse, with a clock the test moves, against
// auto_mouse_switch_set_layer: when the pointer layer setting moves while
// auto-mouse has the old layer on, QMK's set_auto_mouse_layer() leaves that
// layer on and from then on times out only the new one. The switch must turn
// the old one off, and auto-mouse must go on serving the new one.

#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "quantum_keycodes.h"
#include "users/noah/lib/compat/qmk_auto_mouse_contract.h"
#include "users/noah/lib/pointing/policy/auto_mouse_switch.h"

#define OLD_LAYER 3u
#define NEW_LAYER 4u
#define TIMEOUT_MS 650u

debug_config_t  debug_config;
layer_state_t   layer_state;
static uint16_t now = 1000u;

bool layer_state_is(uint8_t layer) {
    return (layer_state & ((layer_state_t)1u << layer)) != 0;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return (state & ((layer_state_t)1u << layer)) != 0;
}

void layer_on(uint8_t layer) {
    layer_state |= (layer_state_t)1u << layer;
}

void layer_off(uint8_t layer) {
    layer_state &= ~((layer_state_t)1u << layer);
}

uint16_t timer_read(void) {
    return now;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(now - last);
}

uint8_t get_oneshot_layer(void) {
    return 0;
}

bool is_keyboard_master(void) {
    return true;
}

bool layer_ownership_owns(uint8_t layer) {
    (void)layer;
    return false;
}

static int failures;

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            fprintf(stderr, "test failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
            failures++;                                                                    \
        }                                                                                  \
    } while (0)

// `ms` of pointing-device task at 10 ms a report, moving by `dx` each.
static void run(uint16_t ms, int8_t dx) {
    for (uint16_t elapsed = 0; elapsed < ms; elapsed += 10u) {
        now += 10u;
        report_mouse_t report = {0};
        report.x              = dx;
        pointing_device_task_auto_mouse(report);
    }
}

int main(void) {
    noah_qmk_contract_auto_mouse_set_layer(OLD_LAYER);
    noah_qmk_contract_auto_mouse_set_enable(true);
    set_auto_mouse_timeout(TIMEOUT_MS);

    // Moving the trackball turns the pointer layer on.
    run(500u, 0);
    run(50u, 20);
    CHECK(layer_state == (layer_state_t)1u << OLD_LAYER);

    // The pushed setting moves the pointer layer while it is on: the old
    // layer goes off at once.
    auto_mouse_switch_set_layer(NEW_LAYER);
    CHECK(noah_qmk_contract_auto_mouse_layer() == NEW_LAYER);
    CHECK(layer_state == 0u);

    // Auto-mouse serves the new layer, turning it on and timing it out, and
    // never brings the old one back.
    run(500u, 0);
    run(50u, 20);
    CHECK(layer_state == (layer_state_t)1u << NEW_LAYER);
    run(3000u, 0);
    CHECK(layer_state == 0u);

    // Pushing the same layer again (every settings push) keeps it on.
    run(50u, 20);
    auto_mouse_switch_set_layer(NEW_LAYER);
    CHECK(layer_state == (layer_state_t)1u << NEW_LAYER);

    // Moving it back while it is on turns that one off the same way, and
    // auto-mouse serves the old layer again.
    auto_mouse_switch_set_layer(OLD_LAYER);
    CHECK(noah_qmk_contract_auto_mouse_layer() == OLD_LAYER);
    CHECK(layer_state == 0u);
    run(500u, 0);
    run(50u, 20);
    CHECK(layer_state == (layer_state_t)1u << OLD_LAYER);
    run(3000u, 0);
    CHECK(layer_state == 0u);

    if (failures) {
        return 1;
    }
    puts("qmk auto-mouse layer move contract passed");
    return 0;
}
