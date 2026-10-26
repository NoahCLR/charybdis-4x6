#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/key/runtime/slot/origin_registry.h"

layer_state_t layer_state;

static uint8_t layer_on_calls[LAYER_COUNT];
static uint8_t layer_off_calls[LAYER_COUNT];
// The order of the latest layer_on and layer_off call per layer, from one count.
static uint8_t layer_call_count, layer_on_order[LAYER_COUNT], layer_off_order[LAYER_COUNT];

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

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static layer_state_t test_layer_mask(uint8_t layer) {
    return (layer_state_t)1u << layer;
}

static void test_reset_stubs(void) {
    layer_state = 0;
    memset(layer_on_calls, 0, sizeof(layer_on_calls));
    memset(layer_off_calls, 0, sizeof(layer_off_calls));
    layer_call_count = 0u;
    memset(layer_on_order, 0, sizeof(layer_on_order));
    memset(layer_off_order, 0, sizeof(layer_off_order));
    layer_ownership_reset_for_test();
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & test_layer_mask(layer)) != 0;
}

void layer_on(uint8_t layer) {
    CHECK(layer < LAYER_COUNT);
    layer_state |= test_layer_mask(layer);
    layer_on_calls[layer]++;
    layer_on_order[layer] = ++layer_call_count;
}

void layer_off(uint8_t layer) {
    CHECK(layer < LAYER_COUNT);
    layer_state &= (layer_state_t)~test_layer_mask(layer);
    layer_off_calls[layer]++;
    layer_off_order[layer] = ++layer_call_count;
}

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}
void send_keyboard_report(void) {}

static void test_single_momentary_press_and_release_toggles_layer(void) {
    keypos_t key_pos = test_keypos(1, 2);

    test_reset_stubs();

    layer_ownership_momentary_press(key_pos, 3);
    CHECK(layer_state == test_layer_mask(3));
    CHECK(layer_on_calls[3] == 1);
    CHECK(layer_off_calls[3] == 0);

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == 0);
    CHECK(layer_on_calls[3] == 1);
    CHECK(layer_off_calls[3] == 1);

    CHECK(!layer_ownership_momentary_release(key_pos));
    CHECK(layer_off_calls[3] == 1);
}

static void test_multiple_keys_share_momentary_layer_reference(void) {
    keypos_t key_a = test_keypos(0, 0);
    keypos_t key_b = test_keypos(0, 1);

    test_reset_stubs();

    layer_ownership_momentary_press(key_a, 2);
    layer_ownership_momentary_press(key_b, 2);

    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_on_calls[2] == 1);
    CHECK(layer_off_calls[2] == 0);

    CHECK(!layer_ownership_momentary_release(key_a));
    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_off_calls[2] == 0);

    CHECK(layer_ownership_momentary_release(key_b));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[2] == 1);
}

// Held means a key holds the layer, whichever key and however many; a lock
// alone is not a hold.
static void test_is_held_tracks_key_holds_not_locks(void) {
    keypos_t key_a = test_keypos(0, 0);
    keypos_t key_b = test_keypos(0, 1);

    test_reset_stubs();

    CHECK(!layer_ownership_is_held(2));
    layer_ownership_momentary_press(key_a, 2);
    layer_ownership_momentary_press(key_b, 2);
    CHECK(layer_ownership_is_held(2) && !layer_ownership_is_held(1));
    (void)layer_ownership_momentary_release(key_a);
    CHECK(layer_ownership_is_held(2));
    (void)layer_ownership_momentary_release(key_b);
    CHECK(!layer_ownership_is_held(2));

    CHECK(layer_ownership_toggle_lock_state(2));
    CHECK(!layer_ownership_is_held(2));
}

static void test_same_key_repress_same_layer_does_not_duplicate_refcount(void) {
    keypos_t key_pos = test_keypos(2, 4);

    test_reset_stubs();

    layer_ownership_momentary_press(key_pos, 1);
    layer_ownership_momentary_press(key_pos, 1);

    CHECK(layer_state == test_layer_mask(1));
    CHECK(layer_on_calls[1] == 1);
    CHECK(layer_off_calls[1] == 0);

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[1] == 1);
}

static void test_same_key_can_move_its_momentary_binding_between_layers(void) {
    keypos_t key_pos = test_keypos(3, 5);

    test_reset_stubs();

    layer_ownership_momentary_press(key_pos, 1);
    CHECK(layer_state == test_layer_mask(1));
    CHECK(layer_on_calls[1] == 1);

    layer_ownership_momentary_press(key_pos, 4);
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_on_calls[1] == 1);
    CHECK(layer_off_calls[1] == 1);
    CHECK(layer_on_calls[4] == 1);

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[4] == 1);
}

static void test_toggle_lock_state_activates_and_deactivates_layer(void) {
    test_reset_stubs();

    CHECK(layer_ownership_toggle_lock_state(5));
    CHECK(layer_ownership_is_locked(5));
    CHECK(layer_state == test_layer_mask(5));
    CHECK(layer_on_calls[5] == 1);
    CHECK(layer_off_calls[5] == 0);

    CHECK(layer_ownership_toggle_lock_state(5));
    CHECK(!layer_ownership_is_locked(5));
    CHECK(layer_state == 0);
    CHECK(layer_on_calls[5] == 1);
    CHECK(layer_off_calls[5] == 1);
}

static void test_multiple_locked_layers_can_coexist(void) {
    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(1, true));
    CHECK(layer_ownership_set_lock_state(2, true));

    CHECK(layer_ownership_is_locked(1));
    CHECK(layer_ownership_is_locked(2));
    CHECK(layer_state == (test_layer_mask(1) | test_layer_mask(2)));
    CHECK(layer_on_calls[1] == 1);
    CHECK(layer_on_calls[2] == 1);

    CHECK(layer_ownership_set_lock_state(1, false));
    CHECK(!layer_ownership_is_locked(1));
    CHECK(layer_ownership_is_locked(2));
    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_off_calls[1] == 1);
    CHECK(layer_off_calls[2] == 0);
}

static void test_locked_layer_stays_active_after_momentary_release(void) {
    keypos_t key_pos = test_keypos(6, 1);

    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(4, true));
    CHECK(layer_ownership_is_locked(4));
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_on_calls[4] == 1);

    layer_ownership_momentary_press(key_pos, 4);
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_on_calls[4] == 1);

    CHECK(!layer_ownership_momentary_release(key_pos));
    CHECK(layer_ownership_is_locked(4));
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_off_calls[4] == 0);

    CHECK(layer_ownership_set_lock_state(4, false));
    CHECK(!layer_ownership_is_locked(4));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[4] == 1);
}

static void test_locking_another_layer_keeps_existing_locks_and_holds(void) {
    keypos_t key_pos = test_keypos(7, 3);

    test_reset_stubs();

    layer_ownership_momentary_press(key_pos, 1);
    CHECK(layer_state == test_layer_mask(1));
    CHECK(layer_on_calls[1] == 1);

    CHECK(layer_ownership_set_lock_state(1, true));
    CHECK(layer_ownership_is_locked(1));
    CHECK(layer_state == test_layer_mask(1));
    CHECK(layer_off_calls[1] == 0);

    CHECK(layer_ownership_set_lock_state(2, true));
    CHECK(layer_ownership_is_locked(1));
    CHECK(layer_ownership_is_locked(2));
    CHECK(layer_state == (test_layer_mask(1) | test_layer_mask(2)));
    CHECK(layer_off_calls[1] == 0);
    CHECK(layer_on_calls[2] == 1);

    CHECK(!layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == (test_layer_mask(1) | test_layer_mask(2)));
    CHECK(layer_off_calls[1] == 0);

    CHECK(layer_ownership_set_lock_state(1, false));
    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_off_calls[1] == 1);

    CHECK(layer_ownership_set_lock_state(2, false));
    CHECK(layer_state == 0);
    CHECK(layer_off_calls[2] == 1);
}

// TO(layer): only the target stays locked. Held momentary layers are not
// locks, so they stay on until their keys are released.
static void test_goto_locks_only_the_target_and_keeps_held_layers(void) {
    keypos_t key_pos = test_keypos(2, 5);

    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(1, true));
    CHECK(layer_ownership_set_lock_state(2, true));
    layer_ownership_momentary_press(key_pos, 5);
    CHECK(layer_state == (test_layer_mask(1) | test_layer_mask(2) | test_layer_mask(5)));

    CHECK(layer_ownership_goto(3));
    CHECK(!layer_ownership_is_locked(1));
    CHECK(!layer_ownership_is_locked(2));
    CHECK(layer_ownership_is_locked(3));
    CHECK(layer_state == (test_layer_mask(3) | test_layer_mask(5)));
    CHECK(layer_on_calls[3] == 1);

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == test_layer_mask(3));

    // Going to a layer already locked on its own changes nothing.
    CHECK(!layer_ownership_goto(3));
    CHECK(layer_state == test_layer_mask(3));
    CHECK(layer_on_calls[3] == 1);
    CHECK(layer_off_calls[3] == 0);
}

static void test_goto_base_releases_every_lock(void) {
    keypos_t key_pos = test_keypos(3, 1);

    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(2, true));
    CHECK(layer_ownership_set_lock_state(4, true));
    layer_ownership_momentary_press(key_pos, 4);

    CHECK(layer_ownership_goto(0));
    CHECK(!layer_ownership_is_locked(0));
    CHECK(!layer_ownership_is_locked(2));
    CHECK(!layer_ownership_is_locked(4));
    CHECK(layer_state == test_layer_mask(4));

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == 0);
    CHECK(!layer_ownership_goto(0));
    CHECK(!layer_ownership_goto(LAYER_COUNT));
}

// The desync raw QMK toggles used to cause: a toggle is a lock, so a
// momentary hold of the same layer no longer turns it off on release.
static void test_toggled_layer_survives_a_momentary_hold_of_itself(void) {
    keypos_t key_pos = test_keypos(0, 4);

    test_reset_stubs();

    CHECK(layer_ownership_toggle_lock_state(2));
    layer_ownership_momentary_press(key_pos, 2);
    CHECK(!layer_ownership_momentary_release(key_pos));
    CHECK(layer_ownership_is_locked(2));
    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_off_calls[2] == 0);

    CHECK(layer_ownership_toggle_lock_state(2));
    CHECK(!layer_ownership_is_locked(2));
    CHECK(layer_state == 0);
}

// OSL(layer): a tap arms the layer until the next key uses it. A quick second
// tap cancels it, as QMK's double tap does; a slow one keeps it on. Arming
// another layer replaces it.
static void test_oneshot_arms_until_consumed_and_quick_double_tap_cancels(void) {
    test_reset_stubs();

    CHECK(layer_ownership_oneshot_layer() == UINT8_MAX);
    CHECK(layer_ownership_oneshot_tap(3, 1000u, 200u));
    CHECK(layer_ownership_oneshot_layer() == 3);
    CHECK(layer_state == test_layer_mask(3));
    CHECK(!layer_ownership_is_locked(3));

    CHECK(layer_ownership_oneshot_consume());
    CHECK(layer_ownership_oneshot_layer() == UINT8_MAX);
    CHECK(layer_state == 0);
    CHECK(!layer_ownership_oneshot_consume());

    CHECK(layer_ownership_oneshot_tap(3, 2000u, 200u));
    CHECK(layer_ownership_oneshot_tap(3, 2150u, 200u));
    CHECK(layer_ownership_oneshot_layer() == UINT8_MAX);
    CHECK(layer_state == 0);

    CHECK(layer_ownership_oneshot_tap(3, 3000u, 200u));
    CHECK(!layer_ownership_oneshot_tap(3, 3400u, 200u));
    CHECK(layer_ownership_oneshot_layer() == 3);
    CHECK(layer_ownership_oneshot_tap(3, 3500u, 200u));
    CHECK(layer_ownership_oneshot_layer() == UINT8_MAX);

    CHECK(layer_ownership_oneshot_tap(2, 4000u, 200u));
    CHECK(layer_ownership_oneshot_tap(5, 4050u, 200u));
    CHECK(layer_ownership_oneshot_layer() == 5);
    CHECK(layer_state == test_layer_mask(5));
    CHECK(layer_off_calls[2] == 1);
    CHECK(!layer_ownership_oneshot_tap(LAYER_COUNT, 4100u, 200u));
}

// A one-shot is one more owner: using it up leaves a lock or a hold of the
// same layer on, and TO() releases it with the locks.
static void test_oneshot_shares_its_layer_with_locks_holds_and_goto(void) {
    keypos_t key_pos = test_keypos(1, 4);

    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(2, true));
    CHECK(layer_ownership_oneshot_tap(2, 1000u, 200u));
    CHECK(layer_ownership_oneshot_consume());
    CHECK(layer_state == test_layer_mask(2));
    CHECK(layer_off_calls[2] == 0);

    layer_ownership_momentary_press(key_pos, 4);
    CHECK(layer_ownership_oneshot_tap(4, 1000u, 200u));
    CHECK(!layer_ownership_momentary_release(key_pos));
    CHECK(layer_state_cmp(layer_state, 4));

    CHECK(layer_ownership_goto(0));
    CHECK(layer_ownership_oneshot_layer() == UINT8_MAX);
    CHECK(layer_state == 0);
}

// A pointing mode its key runs owns the layers its key turns on beside the key holds
// and locks (D-F62): each stays on while any owner keeps it, and the layers
// that join turn on before the ones that leave turn off.
static void test_a_pointing_mode_owns_its_layers_beside_holds_and_locks(void) {
    keypos_t key = test_keypos(1, 1);

    test_reset_stubs();
    CHECK(layer_ownership_pointing_mode_layers() == 0u);
    CHECK(layer_ownership_set_pointing_mode_layers(test_layer_mask(4)));
    CHECK(layer_state == test_layer_mask(4) && layer_ownership_pointing_mode_layers() == test_layer_mask(4));
    CHECK(!layer_ownership_set_pointing_mode_layers(test_layer_mask(4)));
    CHECK(layer_on_calls[4] == 1);

    // Held by a key too: the mode letting go leaves it on until the key does.
    layer_ownership_momentary_press(key, 4);
    CHECK(layer_ownership_set_pointing_mode_layers(0u));
    CHECK(layer_state == test_layer_mask(4));
    CHECK(layer_ownership_momentary_release(key));
    CHECK(layer_state == 0);

    // Several at once; a lock keeps one of them.
    CHECK(layer_ownership_set_pointing_mode_layers(test_layer_mask(5) | test_layer_mask(9)));
    CHECK(layer_state == (test_layer_mask(5) | test_layer_mask(9)));
    CHECK(layer_ownership_set_lock_state(5, true));
    CHECK(layer_ownership_set_pointing_mode_layers(0u));
    CHECK(layer_state == test_layer_mask(5));
    CHECK(layer_ownership_set_lock_state(5, false));
    CHECK(layer_state == 0);

    // Another mode's layers replace them: the new ones on first, then the old
    // ones off; a layer in both stays on throughout.
    CHECK(layer_ownership_set_pointing_mode_layers(test_layer_mask(6) | test_layer_mask(7)));
    layer_off_calls[6] = layer_off_calls[7] = 0;
    layer_on_calls[7] = layer_on_calls[8] = 0;
    CHECK(layer_ownership_set_pointing_mode_layers(test_layer_mask(7) | test_layer_mask(8)));
    CHECK(layer_state == (test_layer_mask(7) | test_layer_mask(8)));
    CHECK(layer_off_calls[6] == 1 && layer_off_calls[7] == 0 && layer_on_calls[7] == 0 && layer_on_calls[8] == 1);
    CHECK(layer_on_order[8] < layer_off_order[6]);
    // Layers past the keymap are left out.
    CHECK(layer_ownership_set_pointing_mode_layers(0u));
    CHECK(!layer_ownership_set_pointing_mode_layers((layer_state_t)(~UINT32_C(0) << LAYER_COUNT)));
    CHECK(layer_state == 0 && layer_ownership_pointing_mode_layers() == 0u);
}

static void test_invalid_layer_requests_are_ignored(void) {
    keypos_t key_pos = test_keypos(4, 4);

    test_reset_stubs();

    CHECK(!layer_ownership_set_lock_state(LAYER_COUNT, true));
    CHECK(!layer_ownership_set_lock_state(LAYER_COUNT, false));
    layer_ownership_momentary_press(key_pos, LAYER_COUNT);

    CHECK(layer_state == 0);
    CHECK(!layer_ownership_momentary_release(key_pos));
}

// A held shortcut owns each of its layers (D-F26) beside the other owners: on
// release a layer goes off only if no lock, other key or mode keeps it.
static void test_a_held_shortcut_owns_its_layers_beside_other_owners(void) {
    keypos_t      shortcut_key = test_keypos(3, 1);
    keypos_t      layer_key    = test_keypos(3, 2);
    layer_state_t layers       = test_layer_mask(2) | test_layer_mask(3) | test_layer_mask(4);

    test_reset_stubs();
    layer_ownership_momentary_press(layer_key, 2);
    CHECK(layer_ownership_set_lock_state(4, true));

    layer_ownership_shortcut_press(shortcut_key, layers);
    CHECK(layer_state == layers);
    CHECK(layer_on_calls[2] == 1 && layer_on_calls[3] == 1 && layer_on_calls[4] == 1);
    CHECK(layer_ownership_is_held(3));

    CHECK(layer_ownership_shortcut_release(shortcut_key));
    CHECK(layer_state == (test_layer_mask(2) | test_layer_mask(4)));
    CHECK(layer_off_calls[2] == 0 && layer_off_calls[3] == 1 && layer_off_calls[4] == 0);
    CHECK(!layer_ownership_is_held(3));
    CHECK(!layer_ownership_shortcut_release(shortcut_key));

    CHECK(layer_ownership_momentary_release(layer_key));
    CHECK(layer_state == test_layer_mask(4));
}

// A key's own layer hold and the layers of a shortcut it holds are separate
// owners, so the end of one never takes the other's layers; pressing the key's
// shortcut again moves it to the new layers, the new ones on first.
static void test_a_key_s_shortcut_and_layer_hold_are_separate_owners(void) {
    keypos_t key = test_keypos(1, 1);
    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset_stubs();
    layer_ownership_momentary_press(key, 2);
    layer_ownership_shortcut_press(key, test_layer_mask(2) | test_layer_mask(5));
    layer_ownership_momentary_owner_bitmap(bitmap);
    CHECK(key_origin_bitmap_has_keypos(bitmap, key));

    CHECK(!layer_ownership_momentary_release(key));
    CHECK(layer_state == (test_layer_mask(2) | test_layer_mask(5)));

    layer_ownership_shortcut_press(key, test_layer_mask(6));
    CHECK(layer_state == test_layer_mask(6));
    CHECK(layer_on_calls[6] == 1 && layer_off_calls[2] == 1 && layer_off_calls[5] == 1);

    CHECK(layer_ownership_shortcut_release(key));
    CHECK(layer_state == 0);
    layer_ownership_momentary_owner_bitmap(bitmap);
    CHECK(!key_origin_bitmap_has_keypos(bitmap, key));

    // No layers, or none in the keymap, binds nothing.
    layer_ownership_shortcut_press(key, (layer_state_t)0u);
    CHECK(!layer_ownership_shortcut_release(key));
}

// A tapped shortcut's layers are on only for the tap and shared like a hold:
// a layer another owner keeps stays on after it.
static void test_a_tapped_shortcut_turns_its_layers_on_only_for_the_tap(void) {
    keypos_t      key    = test_keypos(2, 2);
    layer_state_t layers = test_layer_mask(3) | test_layer_mask(7);

    test_reset_stubs();
    layer_ownership_momentary_press(key, 7);

    layer_ownership_shortcut_tap_begin(layers);
    CHECK(layer_state == layers && layer_on_calls[3] == 1 && layer_on_calls[7] == 1);
    layer_ownership_shortcut_tap_end(layers);
    CHECK(layer_state == test_layer_mask(7));
    CHECK(layer_off_calls[3] == 1 && layer_off_calls[7] == 0);

    CHECK(layer_ownership_momentary_release(key));
    CHECK(layer_state == 0);
}

// A shortcut shares a layer with a pointing mode's key too (D-F26, D-F62):
// whichever lets go first leaves it on for the other.
static void test_a_held_shortcut_shares_a_layer_with_a_pointing_mode(void) {
    keypos_t key = test_keypos(2, 3);

    test_reset_stubs();
    CHECK(layer_ownership_set_pointing_mode_layers(test_layer_mask(4) | test_layer_mask(5)));
    layer_ownership_shortcut_press(key, test_layer_mask(5) | test_layer_mask(6));
    CHECK(layer_state == (test_layer_mask(4) | test_layer_mask(5) | test_layer_mask(6)));
    CHECK(layer_on_calls[5] == 1);
    CHECK(layer_ownership_set_pointing_mode_layers(0u));
    CHECK(layer_state == (test_layer_mask(5) | test_layer_mask(6)) && layer_off_calls[5] == 0);
    CHECK(layer_ownership_shortcut_release(key));
    CHECK(layer_state == 0 && layer_off_calls[5] == 1);
}

// A tap's layers are held while the tap is sent, as a key holds them.
static void test_a_tapped_shortcut_holds_its_layers_while_it_is_sent(void) {
    test_reset_stubs();
    CHECK(!layer_ownership_is_held(3));
    layer_ownership_shortcut_tap_begin(test_layer_mask(3));
    CHECK(layer_ownership_is_held(3));
    layer_ownership_shortcut_tap_end(test_layer_mask(3));
    CHECK(!layer_ownership_is_held(3));
}

// The table holds one entry per matrix position; a key past it binds nothing
// and changes no layer.
static void test_shortcut_owners_beyond_the_table_bind_nothing(void) {
    test_reset_stubs();
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) layer_ownership_shortcut_press(test_keypos(row, col), test_layer_mask(2));
    }
    CHECK(layer_state == test_layer_mask(2));
    keypos_t outside = test_keypos(MATRIX_ROWS, 0);
    layer_ownership_shortcut_press(outside, test_layer_mask(7));
    CHECK(layer_state == test_layer_mask(2) && layer_on_calls[7] == 0);
    CHECK(!layer_ownership_shortcut_release(outside));
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) CHECK(layer_ownership_shortcut_release(test_keypos(row, col)));
    }
    CHECK(layer_state == 0 && layer_off_calls[2] == 1);
}

// A shortcut's Lock (D-F26) locks its layers as one: all on, or all off when
// all are locked already. A layer of the set locked by itself stays locked
// while the rest join it.
static void test_a_set_lock_locks_all_or_releases_all(void) {
    layer_state_t set = test_layer_mask(2) | test_layer_mask(5);

    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(5, true));
    CHECK(layer_ownership_toggle_lock_set(set));
    CHECK(layer_ownership_is_locked(2) && layer_ownership_is_locked(5));
    CHECK(layer_state == set);
    CHECK(layer_on_calls[5] == 1 && layer_off_calls[5] == 0);

    CHECK(layer_ownership_toggle_lock_set(set));
    CHECK(!layer_ownership_is_locked(2) && !layer_ownership_is_locked(5));
    CHECK(layer_state == 0);

    // The base layer is never locked, and layers past the keymap are left out.
    CHECK(layer_ownership_toggle_lock_set(test_layer_mask(0) | test_layer_mask(3) | ((layer_state_t)1u << LAYER_COUNT)));
    CHECK(!layer_ownership_is_locked(0) && layer_ownership_is_locked(3));
    CHECK(layer_ownership_toggle_lock_set(test_layer_mask(0) | test_layer_mask(3)));
    CHECK(layer_state == 0);
    CHECK(!layer_ownership_toggle_lock_set(test_layer_mask(0)));
    CHECK(!layer_ownership_toggle_lock_set(0));
}

// A shortcut's Move to (D-F26) is TO() of a set: its layers lock, on before
// any other lock releases, and every other lock and the one-shot go; held
// layers stay until their keys let go.
static void test_goto_a_set_locks_it_alone(void) {
    keypos_t      key_pos = test_keypos(2, 2);
    layer_state_t set     = test_layer_mask(3) | test_layer_mask(6);

    test_reset_stubs();

    CHECK(layer_ownership_set_lock_state(1, true));
    CHECK(layer_ownership_set_lock_state(6, true));
    layer_ownership_momentary_press(key_pos, 4);
    CHECK(layer_ownership_oneshot_tap(7, 1000u, 200u));

    CHECK(layer_ownership_goto_set(set));
    CHECK(!layer_ownership_is_locked(1) && layer_ownership_is_locked(3) && layer_ownership_is_locked(6));
    CHECK(layer_ownership_oneshot_layers() == 0);
    CHECK(layer_state == (set | test_layer_mask(4)));
    CHECK(layer_on_order[3] < layer_off_order[1]);
    CHECK(layer_on_calls[6] == 1 && layer_off_calls[6] == 0);

    CHECK(layer_ownership_momentary_release(key_pos));
    CHECK(layer_state == set);
    CHECK(!layer_ownership_goto_set(set));
    // The base layer in the set locks nothing.
    CHECK(layer_ownership_goto_set(test_layer_mask(0)));
    CHECK(layer_state == 0);
}

// A shortcut's One-shot (D-F26) arms its layers together until the next key
// uses them, as OSL() arms one: a quick second tap of the same set cancels it,
// and another set replaces it, its layers on before the old ones go.
static void test_a_oneshot_set_arms_together(void) {
    layer_state_t set   = test_layer_mask(2) | test_layer_mask(4);
    layer_state_t other = test_layer_mask(4) | test_layer_mask(6);

    test_reset_stubs();

    CHECK(layer_ownership_oneshot_tap_set(set, 1000u, 200u));
    CHECK(layer_ownership_oneshot_layers() == set);
    CHECK(layer_ownership_oneshot_layer() == 2);
    CHECK(layer_state == set);
    CHECK(layer_ownership_oneshot_consume());
    CHECK(layer_state == 0 && layer_ownership_oneshot_layers() == 0);

    CHECK(layer_ownership_oneshot_tap_set(set, 2000u, 200u));
    CHECK(layer_ownership_oneshot_tap_set(set, 2100u, 200u));
    CHECK(layer_state == 0);

    CHECK(layer_ownership_oneshot_tap_set(set, 3000u, 200u));
    CHECK(!layer_ownership_oneshot_tap_set(set, 3500u, 200u));
    uint8_t layer_4_offs = layer_off_calls[4];
    CHECK(layer_ownership_oneshot_tap_set(other, 3600u, 200u));
    CHECK(layer_ownership_oneshot_layers() == other);
    CHECK(layer_state == other);
    CHECK(layer_off_calls[4] == layer_4_offs); // in both sets, it stays on
    CHECK(layer_on_order[6] < layer_off_order[2]);

    // OSL(n) is a set of one, and replaces a set.
    CHECK(layer_ownership_oneshot_tap(5, 4000u, 200u));
    CHECK(layer_ownership_oneshot_layers() == test_layer_mask(5));
    CHECK(layer_state == test_layer_mask(5));
    CHECK(!layer_ownership_oneshot_tap_set(0, 4100u, 200u));
}

// One key can own shortcut layers twice (D-F26): a Tap-toggle or One-shot
// custom key's own hold from the press, and a custom key its behaviour holds.
// Each lets go only of its own.
static void test_a_key_s_momentary_and_held_shortcuts_are_separate_owners(void) {
    keypos_t key = test_keypos(2, 4);

    test_reset_stubs();

    layer_ownership_shortcut_momentary_press(key, test_layer_mask(9));
    layer_ownership_shortcut_press(key, test_layer_mask(12));
    CHECK(layer_state == (test_layer_mask(9) | test_layer_mask(12)));
    CHECK(layer_ownership_shortcut_release(key));
    CHECK(layer_state == test_layer_mask(9));
    CHECK(!layer_ownership_shortcut_release(key));
    CHECK(layer_ownership_shortcut_momentary_release(key));
    CHECK(layer_state == 0);

    // The same layers from both: the first release keeps them on.
    layer_ownership_shortcut_momentary_press(key, test_layer_mask(9));
    layer_ownership_shortcut_press(key, test_layer_mask(9));
    CHECK(layer_ownership_shortcut_release(key));
    CHECK(layer_state == test_layer_mask(9) && layer_off_calls[9] == 1);
    CHECK(layer_ownership_shortcut_momentary_release(key));
    CHECK(layer_state == 0 && !layer_ownership_shortcut_momentary_release(key));
}

int main(void) {
    test_single_momentary_press_and_release_toggles_layer();
    test_multiple_keys_share_momentary_layer_reference();
    test_is_held_tracks_key_holds_not_locks();
    test_same_key_repress_same_layer_does_not_duplicate_refcount();
    test_same_key_can_move_its_momentary_binding_between_layers();
    test_toggle_lock_state_activates_and_deactivates_layer();
    test_multiple_locked_layers_can_coexist();
    test_locked_layer_stays_active_after_momentary_release();
    test_locking_another_layer_keeps_existing_locks_and_holds();
    test_goto_locks_only_the_target_and_keeps_held_layers();
    test_goto_base_releases_every_lock();
    test_toggled_layer_survives_a_momentary_hold_of_itself();
    test_oneshot_arms_until_consumed_and_quick_double_tap_cancels();
    test_oneshot_shares_its_layer_with_locks_holds_and_goto();
    test_a_pointing_mode_owns_its_layers_beside_holds_and_locks();
    test_invalid_layer_requests_are_ignored();
    test_a_held_shortcut_owns_its_layers_beside_other_owners();
    test_a_key_s_shortcut_and_layer_hold_are_separate_owners();
    test_a_tapped_shortcut_turns_its_layers_on_only_for_the_tap();
    test_a_held_shortcut_shares_a_layer_with_a_pointing_mode();
    test_a_tapped_shortcut_holds_its_layers_while_it_is_sent();
    test_shortcut_owners_beyond_the_table_bind_nothing();
    test_a_set_lock_locks_all_or_releases_all();
    test_goto_a_set_locks_it_alone();
    test_a_oneshot_set_arms_together();
    test_a_key_s_momentary_and_held_shortcuts_are_separate_owners();

    puts("layer_ownership host tests passed");
    return 0;
}
