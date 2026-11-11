#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include QMK_KEYBOARD_H

#include "users/noah/lib/pointing/policy/auto_mouse_switch.h"
#include "users/noah/lib/profile/runtime/active_profile_settings_runtime.h"

// QMK's auto-mouse as the BK pin has it: set_auto_mouse_enable() resets its
// tracker and timers and never touches the layer.
static bool          fake_enabled;
static uint8_t       fake_layer;
static int8_t        fake_key_tracker;
static uint32_t      fake_stored;
static bool          fake_master;
static layer_state_t fake_owned_layers;
static unsigned      layer_off_calls;

layer_state_t layer_state;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

bool get_auto_mouse_enable(void) {
    return fake_enabled;
}

void set_auto_mouse_enable(bool enable) {
    if (fake_enabled == enable) return;
    fake_enabled     = enable;
    fake_key_tracker = 0;
}

uint8_t get_auto_mouse_layer(void) {
    return fake_layer;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return (state & ((layer_state_t)1u << layer)) != 0u;
}

void layer_off(uint8_t layer) {
    layer_off_calls++;
    layer_state &= ~((layer_state_t)1u << layer);
}

bool is_keyboard_master(void) {
    return fake_master;
}

bool layer_ownership_owns(uint8_t layer) {
    return (fake_owned_layers & ((layer_state_t)1u << layer)) != 0u;
}

uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    return id == NOAH_SETTING_AUTO_MOUSE_ENABLED ? fake_stored : fallback;
}

static void reset(void) {
    auto_mouse_switch_set_override(true);
    fake_enabled      = true;
    fake_layer        = 4u;
    fake_key_tracker  = 0;
    fake_stored       = 1u;
    fake_master       = true;
    fake_owned_layers = 0u;
    layer_state       = 0u;
    layer_off_calls   = 0u;
}

// Auto-mouse runs only while the stored setting and the override both allow
// it, and the override starts allowed.
static void test_qmk_runs_stored_and_override(void) {
    reset();
    CHECK(auto_mouse_switch_override());

    auto_mouse_switch_set_override(false);
    CHECK(!auto_mouse_switch_override() && !fake_enabled);
    auto_mouse_switch_set_override(true);
    CHECK(auto_mouse_switch_override() && fake_enabled);

    // With the stored setting off, allowing it at runtime does not turn it on.
    fake_stored = 0u;
    auto_mouse_switch_apply(false);
    CHECK(!fake_enabled);
    auto_mouse_switch_set_override(true);
    CHECK(!fake_enabled);
    fake_stored = 1u;
    auto_mouse_switch_apply(true);
    CHECK(fake_enabled);
}

// Pushing the stored setting again (activation, a settings edit) keeps the
// override: auto-mouse stays off, and the override is not reset.
static void test_settings_push_keeps_the_override(void) {
    reset();
    auto_mouse_switch_set_override(false);
    auto_mouse_switch_apply(true);
    CHECK(!fake_enabled && !auto_mouse_switch_override());
    auto_mouse_switch_apply(false);
    auto_mouse_switch_apply(true);
    CHECK(!fake_enabled && !auto_mouse_switch_override());
}

// A pointer layer auto-mouse turned on goes off with it, since QMK leaves it
// on and would never time it out.
static void test_switching_off_turns_its_pointer_layer_off(void) {
    reset();
    layer_state = (layer_state_t)1u << 4 | (layer_state_t)1u << 1;
    auto_mouse_switch_set_override(false);
    CHECK(!fake_enabled && !layer_state_cmp(layer_state, 4u) && layer_state_cmp(layer_state, 1u));

    // A stored setting switched off from the settings does the same.
    reset();
    layer_state = (layer_state_t)1u << 4;
    auto_mouse_switch_apply(false);
    CHECK(!fake_enabled && layer_state == 0u);
}

// A pointer layer something else owns (a lock, a key's hold, a pointing
// mode) stays on with its owner.
static void test_a_pointer_layer_another_owner_keeps_stays(void) {
    reset();
    layer_state       = (layer_state_t)1u << 4;
    fake_owned_layers = (layer_state_t)1u << 4;
    auto_mouse_switch_set_override(false);
    CHECK(!fake_enabled && layer_state_cmp(layer_state, 4u) && layer_off_calls == 0u);
}

// Nothing turns off when auto-mouse was already off, when it stays on, or
// on the slave half, whose layers follow the master half.
static void test_only_the_switch_off_on_the_master_turns_the_layer_off(void) {
    reset();
    layer_state = (layer_state_t)1u << 4;
    auto_mouse_switch_apply(true);
    auto_mouse_switch_set_override(true);
    CHECK(layer_state_cmp(layer_state, 4u) && layer_off_calls == 0u);

    fake_enabled = false;
    auto_mouse_switch_apply(false);
    CHECK(layer_state_cmp(layer_state, 4u) && layer_off_calls == 0u);

    reset();
    fake_master = false;
    layer_state = (layer_state_t)1u << 4;
    auto_mouse_switch_set_override(false);
    CHECK(!fake_enabled && layer_state_cmp(layer_state, 4u) && layer_off_calls == 0u);
}

int main(void) {
    test_qmk_runs_stored_and_override();
    test_settings_push_keeps_the_override();
    test_switching_off_turns_its_pointer_layer_off();
    test_a_pointer_layer_another_owner_keeps_stays();
    test_only_the_switch_off_on_the_master_turns_the_layer_off();
    puts("auto-mouse switch tests passed");
    return 0;
}
