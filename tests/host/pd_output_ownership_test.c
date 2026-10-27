#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "host_runtime_reset_fixture.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/action/synthetic_record.h"
#include "users/noah/lib/pointing/modes/pointing_mode_configured.h"

static host_runtime_fixture_t fixture = HOST_RUNTIME_FIXTURE_INIT;
static bool down[256];
static unsigned presses, releases;
static bool verify_tap_mods;
static uint8_t expected_tap_mods;
static noah_pd_config_t slot;

HOST_RUNTIME_FIXTURE_DEFINE_BASIC_QMK_STUBS(fixture)

void register_code(uint8_t key) {
    if (verify_tap_mods && key == KC_LEFT) assert(get_mods() == expected_tap_mods);
    if (!down[key]) presses++;
    down[key] = true;
}
void unregister_code(uint8_t key) {
    if (down[key]) releases++;
    down[key] = false;
}
void tap_code16(uint16_t key) {
    assert(false); // All allowed PD taps are representable by the ledger.
}
void wait_ms(uint16_t delay) {}
void pointer_layer_policy_note_action(uint16_t key, bool pressed) {}
bool noah_key_runtime_settle_pending_fallback_hold(void) { return false; }
void noah_action_tap(uint16_t key) { assert(false); }
void noah_action_tap_at(keypos_t pos, uint16_t key) { assert(false); }
bool noah_profile_macro_start(uint16_t key) { return false; }
report_mouse_t noah_pd_configured_scroll(const noah_pd_config_t *scrolling, report_mouse_t report) { return report; }
void reset_dragscroll_mode(void) {}
report_mouse_t pointing_device_get_report(void) { return (report_mouse_t){0}; }
void pointing_device_set_report(report_mouse_t report) {}

bool keyboard_mod_ownership_can_register_mods(uint8_t mods) { return true; }
bool keyboard_mod_ownership_can_unregister_mods(uint8_t mods) { return true; }
bool keyboard_mod_ownership_register_mods(uint8_t mods) {
    bool changed = (get_mods() & mods) != mods;
    add_mods(mods);
    return changed;
}
void keyboard_mod_ownership_unregister_mods(uint8_t mods) { del_mods(mods); }
void keyboard_mod_ownership_register(uint16_t key) { add_mods(MOD_BIT(key)); }
void keyboard_mod_ownership_unregister(uint16_t key) { del_mods(MOD_BIT(key)); }
uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) { return 0; }

// QMK's basic ACT_LMODS path: a synthetic press/release changes the report
// directly. The regression must reach this path when the engine bypasses
// the ledger, rather than replacing either emitter with a logging mock.
bool process_record(keyrecord_t *record) {
    assert(noah_synthetic_record_active());
    if (record->event.pressed) register_code((uint8_t)record->keycode);
    else unregister_code((uint8_t)record->keycode);
    return true;
}
bool process_record_user(uint16_t key, keyrecord_t *record) { return true; }

static void emit(void) {
    noah_pd_engine_motion((report_mouse_t){.x = 1});
}

static void check_shared_output(uint16_t output, uint8_t policy, bool physical) {
    host_runtime_fixture_reset(&fixture);
    owned_keycode_reset_for_test();
    memset(down, 0, sizeof(down));
    presses = releases = 0;
    // Motion to the right taps the output.
    noah_pd_tap_t tap = {output, policy, policy == NOAH_PD_TAP_MASK ? MOD_BIT(KC_LEFT_CTRL) : 0};
    slot = (noah_pd_config_t){
        .kind        = NOAH_PD_KIND_DIRECTIONAL,
        .axis        = NOAH_PD_AXIS_HORIZONTAL,
        .threshold_x = 1,
        .threshold_y = 1,
        .directions  = {[NOAH_PD_DIRECTION_RIGHT] = tap},
    };
    noah_pd_engine_enter(&slot);
    fixture.mods = MOD_BIT(KC_LEFT_CTRL);
    owned_keycode_lease_t hold = {0};
    keyrecord_t record = {.event = {.type = KEY_EVENT, .pressed = true, .key = {.row = 2, .col = 2}}};
    if (physical) {
        owned_keycode_track_physical_event(KC_LEFT, &record);
        register_code(KC_LEFT);
    } else {
        assert(owned_keycode_acquire(KC_LEFT, &hold));
    }
    expected_tap_mods = policy == 0 ? MOD_BIT(KC_LEFT_CTRL) : 0;
    if (output == S(KC_LEFT)) expected_tap_mods |= MOD_BIT(KC_LEFT_SHIFT);
    if (output == RSFT(KC_LEFT)) expected_tap_mods |= MOD_BIT(KC_RIGHT_SHIFT);
    verify_tap_mods = true;
    emit();
    verify_tap_mods = false;
    assert(down[KC_LEFT]);
    assert(presses == 1 && releases == 0);
    assert(get_mods() == MOD_BIT(KC_LEFT_CTRL));
    if (physical) {
        record.event.pressed = false;
        owned_keycode_track_physical_event(KC_LEFT, &record);
        unregister_code(KC_LEFT);
    } else {
        assert(owned_keycode_release(&hold));
    }
    assert(!down[KC_LEFT] && releases == 1);
    // With no other owner, the same output produces one balanced tap.
    noah_pd_engine_enter(&slot);
    verify_tap_mods = true;
    emit();
    verify_tap_mods = false;
    assert(!down[KC_LEFT] && presses == 2 && releases == 2);
    assert(get_mods() == MOD_BIT(KC_LEFT_CTRL));
    noah_pd_engine_exit();
    owned_keycode_debug_snapshot_t snapshot;
    owned_keycode_debug_snapshot(KC_LEFT, &snapshot);
    assert(snapshot.physical_count == 0 && snapshot.managed_count == 0 && snapshot.underflow_count == 0);
}

int main(void) {
    const uint16_t outputs[] = {KC_LEFT, S(KC_LEFT), RSFT(KC_LEFT)};
    for (uint8_t i = 0; i < ARRAY_SIZE(outputs); i++)
        for (uint8_t policy = 0; policy < 3; policy++)
            for (uint8_t physical = 0; physical < 2; physical++)
                check_shared_output(outputs[i], policy, physical);
    puts("PD direction taps preserve physical and managed output owners under every modifier policy");
}
