// A custom key's shortcut (D-F26) held and tapped through the real owned
// output (owned_keycode.c, keyboard_mod_ownership.c) and QMK's own report
// code from the selected QMK: register_code() and register_mouse() in
// action.c, the 6-key report in action_util.c, the extrakey reports in host.c,
// mouse keys in mousekey.c and mouse buttons in pointing_device.c. A host
// driver records every report in the order QMK sends it, so the test checks
// what the host sees, report by report, not which functions were called.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "quantum.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"

typedef struct {
    char    kind; // 'K' keyboard, 'C' consumer, 'S' system, 'M' mouse
    uint8_t mods;
    uint8_t keys[KEYBOARD_REPORT_KEYS];
    uint16_t usage;
    uint8_t  buttons;
    int8_t   y;
} test_report_t;

static test_report_t reports[64];
static uint8_t       report_count;
static uint32_t      waits[8];
static uint8_t       wait_count;

static void test_fail(const char *expr, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, __FILE__, line);
    for (uint8_t i = 0; i < report_count; i++) {
        fprintf(stderr, "  %c mods=%02x keys=%02x %02x %02x %02x %02x %02x usage=%04x buttons=%02x y=%d\n", reports[i].kind, reports[i].mods, reports[i].keys[0], reports[i].keys[1], reports[i].keys[2], reports[i].keys[3], reports[i].keys[4], reports[i].keys[5], reports[i].usage, reports[i].buttons, reports[i].y);
    }
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) test_fail(#expr, __LINE__); \
    } while (0)

static void record(test_report_t report) {
    CHECK(report_count < sizeof(reports) / sizeof(reports[0]));
    reports[report_count++] = report;
}

static uint8_t keyboard_leds(void) {
    return 0;
}
static void send_keyboard(report_keyboard_t *report) {
    test_report_t out = {.kind = 'K', .mods = report->mods};
    memcpy(out.keys, report->keys, sizeof(out.keys));
    record(out);
}
static void send_nkro(report_nkro_t *report) {
    (void)report;
    CHECK(!"the shortcut build has no NKRO");
}
static void send_mouse(report_mouse_t *report) {
    record((test_report_t){.kind = 'M', .buttons = report->buttons, .y = (int8_t)report->y});
}
static void send_extra(report_extra_t *report) {
    record((test_report_t){.kind = report->report_id == REPORT_ID_CONSUMER ? 'C' : 'S', .usage = report->usage});
}
static host_driver_t driver = {keyboard_leds, send_keyboard, send_nkro, send_mouse, send_extra};

void wait_ms(uint32_t ms) {
    CHECK(wait_count < sizeof(waits) / sizeof(waits[0]));
    waits[wait_count++] = ms;
}
uint16_t timer_read(void) {
    return 0;
}
void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

static void reset(void) {
    owned_keycode_reset_for_test();
    keyboard_mod_ownership_reset_for_test();
    report_count = 0;
    wait_count   = 0;
}

static bool keys_are(const test_report_t *report, const uint8_t expected[KEYBOARD_REPORT_KEYS]) {
    return report->kind == 'K' && !memcmp(report->keys, expected, KEYBOARD_REPORT_KEYS);
}

// Held: Shift goes down in a report of its own, then each key in order, each
// in the report its kind travels in (A in the 6-key report beside Shift, Play
// in the consumer report, Sleep in the system report, the click in the mouse
// report after the fresh Shift's head start, B beside A, the cursor key as
// movement). Released, they leave last first, then Shift.
static void test_held_shortcut_reaches_each_report_in_order(void) {
    static const uint8_t keys[OWNED_KEYCODE_CHORD_KEYS] = {KC_A, KC_MEDIA_PLAY_PAUSE, KC_SYSTEM_SLEEP, MS_BTN1, KC_B, MS_UP};
    owned_keycode_chord_lease_t lease = {0};
    const uint8_t               shift = MOD_BIT(KC_LEFT_SHIFT);

    reset();
    CHECK(owned_keycode_acquire_chord(shift, keys, &lease));
    CHECK(report_count == 7);
    CHECK(keys_are(&reports[0], (const uint8_t[KEYBOARD_REPORT_KEYS]){0}) && reports[0].mods == shift);
    CHECK(keys_are(&reports[1], (const uint8_t[KEYBOARD_REPORT_KEYS]){KC_A}) && reports[1].mods == shift);
    CHECK(reports[2].kind == 'C' && reports[2].usage == TRANSPORT_PLAY_PAUSE);
    CHECK(reports[3].kind == 'S' && reports[3].usage == SYSTEM_SLEEP);
    CHECK(reports[4].kind == 'M' && reports[4].buttons == MOUSE_BTN_MASK(0));
    CHECK(wait_count == 1 && waits[0] == OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS);
    CHECK(keys_are(&reports[5], (const uint8_t[KEYBOARD_REPORT_KEYS]){KC_A, KC_B}) && reports[5].mods == shift);
    CHECK(reports[6].kind == 'M' && reports[6].y < 0);

    report_count = 0;
    CHECK(owned_keycode_release_chord(&lease) && !lease.active);
    CHECK(report_count == 7);
    CHECK(reports[0].kind == 'M' && reports[0].y == 0);
    CHECK(keys_are(&reports[1], (const uint8_t[KEYBOARD_REPORT_KEYS]){KC_A}) && reports[1].mods == shift);
    CHECK(reports[2].kind == 'M' && reports[2].buttons == 0);
    CHECK(reports[3].kind == 'S' && reports[3].usage == 0);
    CHECK(reports[4].kind == 'C' && reports[4].usage == 0);
    CHECK(keys_are(&reports[5], (const uint8_t[KEYBOARD_REPORT_KEYS]){0}) && reports[5].mods == shift);
    CHECK(keys_are(&reports[6], (const uint8_t[KEYBOARD_REPORT_KEYS]){0}) && reports[6].mods == 0);
}

// Six keyboard keys fill the 6-key report, in the order they went down, with
// Left Ctrl and Right Alt held beside them.
static void test_six_keys_fill_the_six_key_report(void) {
    static const uint8_t keys[OWNED_KEYCODE_CHORD_KEYS] = {KC_A, KC_S, KC_D, KC_LEFT, KC_UP, KC_EXSEL};
    owned_keycode_chord_lease_t lease = {0};
    const uint8_t               mods  = MOD_BIT(KC_LEFT_CTRL) | MOD_BIT(KC_RIGHT_ALT);

    reset();
    CHECK(owned_keycode_acquire_chord(mods, keys, &lease));
    CHECK(report_count == 7 && keys_are(&reports[6], keys) && reports[6].mods == mods);
    report_count = 0;
    CHECK(owned_keycode_release_chord(&lease));
    CHECK(report_count == 7);
    CHECK(keys_are(&reports[0], (const uint8_t[KEYBOARD_REPORT_KEYS]){KC_A, KC_S, KC_D, KC_LEFT, KC_UP}) && reports[0].mods == mods);
    CHECK(keys_are(&reports[6], (const uint8_t[KEYBOARD_REPORT_KEYS]){0}) && reports[6].mods == 0);
}

// Tapped, all of it goes down, the tap delay passes once, and all of it comes
// back up: a media key and a keyboard key with Gui.
static void test_tapped_shortcut_goes_down_and_up_inside_one_tap(void) {
    const uint8_t gui = MOD_BIT(KC_LEFT_GUI);

    reset();
    CHECK(owned_keycode_tap_chord(gui, (const uint8_t[OWNED_KEYCODE_CHORD_KEYS]){KC_AUDIO_VOL_UP, KC_J}));
    CHECK(report_count == 6);
    CHECK(keys_are(&reports[0], (const uint8_t[KEYBOARD_REPORT_KEYS]){0}) && reports[0].mods == gui);
    CHECK(reports[1].kind == 'C' && reports[1].usage == AUDIO_VOL_UP);
    CHECK(keys_are(&reports[2], (const uint8_t[KEYBOARD_REPORT_KEYS]){KC_J}) && reports[2].mods == gui);
    CHECK(keys_are(&reports[3], (const uint8_t[KEYBOARD_REPORT_KEYS]){0}) && reports[3].mods == gui);
    CHECK(reports[4].kind == 'C' && reports[4].usage == 0);
    CHECK(keys_are(&reports[5], (const uint8_t[KEYBOARD_REPORT_KEYS]){0}) && reports[5].mods == 0);
    CHECK(wait_count == 1 && waits[0] == TAP_CODE_DELAY);
}

// A shortcut the ledger cannot hold sends no report at all.
static void test_refused_shortcut_sends_no_report(void) {
    owned_keycode_chord_lease_t lease = {0};

    reset();
    for (unsigned i = 0; i < UINT8_MAX; i++) {
        CHECK(owned_keycode_register(KC_D));
    }
    report_count = 0;
    CHECK(!owned_keycode_acquire_chord(MOD_BIT(KC_LEFT_SHIFT), (const uint8_t[OWNED_KEYCODE_CHORD_KEYS]){KC_A, KC_MEDIA_PLAY_PAUSE, KC_D}, &lease));
    CHECK(!lease.active && report_count == 0 && wait_count == 0);
    for (unsigned i = 0; i < UINT8_MAX; i++) {
        CHECK(owned_keycode_unregister(KC_D));
    }
    CHECK(keys_are(&reports[report_count - 1u], (const uint8_t[KEYBOARD_REPORT_KEYS]){0}));
}

int main(void) {
    host_set_driver(&driver);
    test_held_shortcut_reaches_each_report_in_order();
    test_six_keys_fill_the_six_key_report();
    test_tapped_shortcut_goes_down_and_up_inside_one_tap();
    test_refused_shortcut_sends_no_report();
    puts("owned keycode QMK report tests passed");
    return 0;
}
