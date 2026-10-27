// ────────────────────────────────────────────────────────────────────────────
// Owned Keycode Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include "owned_keycode.h"

#include "../pointing/policy/pointer_layer_policy.h"
#include "../compat/qmk_mod_contract.h"
#include "../state/ownership/keyboard_mod_ownership.h"

#define OWNED_KEYCODE_USAGE_CAPACITY ((uint16_t)QK_MOUSE_ACCELERATION_2 + 1u)

typedef struct {
    uint8_t  physical_refcounts[OWNED_KEYCODE_USAGE_CAPACITY];
    uint8_t  managed_refcounts[OWNED_KEYCODE_USAGE_CAPACITY];
    uint16_t managed_usage_count;
    uint16_t saturation_count;
    uint16_t underflow_count;
    uint16_t unsupported_count;
    uint16_t idempotent_release_count;
} owned_keycode_state_t;

static owned_keycode_state_t owned_keycode_state;

static void owned_keycode_note_unsupported(void) {
    if (owned_keycode_state.unsupported_count != UINT16_MAX) {
        owned_keycode_state.unsupported_count++;
    }
}

static bool owned_keycode_is_modded(uint16_t keycode) {
    return IS_QK_MODS(keycode);
}

static uint8_t owned_keycode_extract_mods(uint16_t keycode) {
    return noah_qmk_mods_to_report_mask(QK_MODS_GET_MODS(keycode));
}

static uint8_t owned_keycode_basic_part(uint16_t keycode) {
    return owned_keycode_is_modded(keycode) ? QK_MODS_GET_BASIC_KEYCODE(keycode) : (uint8_t)keycode;
}

static bool owned_keycode_is_report_usage(uint8_t keycode) {
    return IS_BASIC_KEYCODE(keycode) || IS_SYSTEM_KEYCODE(keycode) || IS_CONSUMER_KEYCODE(keycode) || IS_MOUSE_KEYCODE(keycode);
}

static bool owned_keycode_decompose(uint16_t keycode, owned_keycode_lease_t *lease) {
    if (!lease) {
        return false;
    }

    *lease = (owned_keycode_lease_t){0};
    if (owned_keycode_is_modded(keycode)) {
        lease->basic     = owned_keycode_basic_part(keycode);
        lease->has_basic = owned_keycode_is_report_usage(lease->basic);
        lease->mods      = owned_keycode_extract_mods(keycode);
        if (IS_MODIFIER_KEYCODE(lease->basic)) lease->mods |= MOD_BIT(lease->basic);
        return true;
    }
    if (keycode > UINT8_MAX) {
        return false;
    }
    if (IS_MODIFIER_KEYCODE(keycode)) {
        lease->mods = MOD_BIT(keycode);
    } else if (owned_keycode_is_report_usage((uint8_t)keycode)) {
        lease->basic     = (uint8_t)keycode;
        lease->has_basic = true;
    }
    return true;
}

bool owned_keycode_is_supported(uint16_t keycode) {
    owned_keycode_lease_t components;

    return owned_keycode_decompose(keycode, &components);
}

static void owned_keycode_note_saturation(void) {
    if (owned_keycode_state.saturation_count != UINT16_MAX) {
        owned_keycode_state.saturation_count++;
    }
}

static void owned_keycode_note_underflow(void) {
    if (owned_keycode_state.underflow_count != UINT16_MAX) {
        owned_keycode_state.underflow_count++;
    }
}

// Precondition: the usage's managed count is below UINT8_MAX.
static void owned_keycode_acquire_basic(uint8_t basic) {
    if (owned_keycode_state.managed_refcounts[basic]++ == 0u) {
        owned_keycode_state.managed_usage_count++;
        if (owned_keycode_state.physical_refcounts[basic] == 0u) {
            pointer_layer_policy_note_action(basic, true);
            register_code(basic);
        }
    }
}

// Precondition: the usage's managed count is above zero.
static void owned_keycode_release_basic(uint8_t basic) {
    if (--owned_keycode_state.managed_refcounts[basic] == 0u) {
        if (owned_keycode_state.managed_usage_count != 0u) {
            owned_keycode_state.managed_usage_count--;
        }
        if (owned_keycode_state.physical_refcounts[basic] == 0u) {
            unregister_code(basic);
            pointer_layer_policy_note_action(basic, false);
        }
    }
}

// Every lease takes its usages here, a literal keycode's (its modifiers and at
// most one key) and a chord's alike: `mods` and the first `count` of `keys`.
//
// The ledger is asked first whether it can take all of them, so a lease it
// cannot hold sends no report at all.
//
// Modifiers reach the host report before the usage they qualify, matching
// QMK's own register_code16(). Registering the usage first ships one report
// with the bare usage down, which the host has already committed as an
// unshifted character by the time the modifier arrives: holding a
// PRESS_AND_HOLD_UNTIL_RELEASE(KC_ASTR) key types "8" and then repeats "*".
//
// Mouse buttons need more than report order. They leave on their own USB
// interface and endpoint, whose output queue drains independently of the
// keyboard endpoint's, so a modifier queued microseconds earlier still lands in
// the same USB frame and the host is free to poll the mouse endpoint first. A
// host that samples modifier state at button-down then sees a bare click, which
// is how PRESS_AND_HOLD_UNTIL_RELEASE(A(MS_BTN2)) reaches a window manager as a
// plain right-drag some of the time. Let a fresh modifier settle across a few
// frames before the first button goes down. A modifier that was already down
// needs no head start: no report was sent for it, so there is nothing to
// outrun.
static bool owned_keycode_acquire_usages(uint8_t mods, const uint8_t *keys, uint8_t count) {
    bool modifier_report_sent = false;

    for (uint8_t key = 0u; key < count; key++) {
        if (owned_keycode_state.managed_refcounts[keys[key]] == UINT8_MAX) {
            owned_keycode_note_saturation();
            return false;
        }
    }
    if (mods != 0u && !keyboard_mod_ownership_can_register_mods(mods)) {
        owned_keycode_note_saturation();
        return false;
    }

    if (mods != 0u) {
        modifier_report_sent = keyboard_mod_ownership_register_mods(mods);
    }
    for (uint8_t key = 0u; key < count; key++) {
        if (modifier_report_sent && IS_MOUSEKEY_BUTTON(keys[key])) {
            wait_ms(OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS);
            modifier_report_sent = false;
        }
        owned_keycode_acquire_basic(keys[key]);
    }
    return true;
}

// Teardown mirrors the acquire order: the keys leave the report last first,
// then the modifiers, so a modifier never outlives a usage it was qualifying.
// Nothing is let go unless all of it can be.
static bool owned_keycode_release_usages(uint8_t mods, const uint8_t *keys, uint8_t count) {
    for (uint8_t key = 0u; key < count; key++) {
        if (owned_keycode_state.managed_refcounts[keys[key]] == 0u) {
            owned_keycode_note_underflow();
            return false;
        }
    }
    if (mods != 0u && !keyboard_mod_ownership_can_unregister_mods(mods)) {
        owned_keycode_note_underflow();
        return false;
    }

    while (count != 0u) {
        owned_keycode_release_basic(keys[--count]);
    }
    if (mods != 0u) {
        keyboard_mod_ownership_unregister_mods(mods);
    }
    return true;
}

static bool owned_keycode_acquire_components(const owned_keycode_lease_t *components) {
    return owned_keycode_acquire_usages(components->mods, &components->basic, components->has_basic ? 1u : 0u);
}

static bool owned_keycode_release_components(const owned_keycode_lease_t *components) {
    return owned_keycode_release_usages(components->mods, &components->basic, components->has_basic ? 1u : 0u);
}

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    owned_keycode_lease_t candidate;

    if (!lease || lease->active) {
        return false;
    }
    *lease = (owned_keycode_lease_t){0};
    if (!owned_keycode_decompose(keycode, &candidate)) {
        owned_keycode_note_unsupported();
        return false;
    }
    if (!owned_keycode_acquire_components(&candidate)) {
        return false;
    }
    candidate.active = true;
    *lease           = candidate;
    return true;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    bool released;

    if (!lease) {
        return false;
    }
    if (!lease->active) {
        if (owned_keycode_state.idempotent_release_count != UINT16_MAX) {
            owned_keycode_state.idempotent_release_count++;
        }
        return true;
    }

    released = owned_keycode_release_components(lease);
    *lease   = (owned_keycode_lease_t){0};
    return released;
}

// A chord's keys are report usages, none a modifier (those are its mask) and
// none named twice.
static bool owned_keycode_chord_keys_supported(const uint8_t keys[OWNED_KEYCODE_CHORD_KEYS], uint8_t count) {
    for (uint8_t key = 0u; key < count; key++) {
        if (IS_MODIFIER_KEYCODE(keys[key]) || !owned_keycode_is_report_usage(keys[key])) {
            return false;
        }
        for (uint8_t before = 0u; before < key; before++) {
            if (keys[before] == keys[key]) {
                return false;
            }
        }
    }
    return true;
}

bool owned_keycode_acquire_chord(uint8_t mods, const uint8_t keys[OWNED_KEYCODE_CHORD_KEYS], owned_keycode_chord_lease_t *lease) {
    owned_keycode_chord_lease_t candidate = {.mods = mods};
    uint8_t                     count;

    if (!lease || lease->active || !keys) {
        return false;
    }
    *lease = (owned_keycode_chord_lease_t){0};
    count  = noah_custom_key_shortcut_key_count(keys);
    if ((count == 0u && mods == 0u) || !owned_keycode_chord_keys_supported(keys, count)) {
        owned_keycode_note_unsupported();
        return false;
    }
    for (uint8_t key = 0u; key < count; key++) {
        candidate.keys[key] = keys[key];
    }
    if (!owned_keycode_acquire_usages(mods, candidate.keys, count)) {
        return false;
    }
    candidate.active = true;
    *lease           = candidate;
    return true;
}

bool owned_keycode_release_chord(owned_keycode_chord_lease_t *lease) {
    bool released;

    if (!lease) {
        return false;
    }
    if (!lease->active) {
        if (owned_keycode_state.idempotent_release_count != UINT16_MAX) {
            owned_keycode_state.idempotent_release_count++;
        }
        return true;
    }
    released = owned_keycode_release_usages(lease->mods, lease->keys, noah_custom_key_shortcut_key_count(lease->keys));
    *lease   = (owned_keycode_chord_lease_t){0};
    return released;
}

// A tap holds the whole chord for one tap delay, Caps Lock's longer one when
// the chord has it, as tap_code() would.
bool owned_keycode_tap_chord(uint8_t mods, const uint8_t keys[OWNED_KEYCODE_CHORD_KEYS]) {
    owned_keycode_chord_lease_t lease = {0};
    uint16_t                    delay = TAP_CODE_DELAY;
    uint8_t                     count;

    if (!owned_keycode_acquire_chord(mods, keys, &lease)) {
        return false;
    }
    count = noah_custom_key_shortcut_key_count(lease.keys);
    for (uint8_t key = 0u; key < count; key++) {
        if (lease.keys[key] == KC_CAPS_LOCK) delay = TAP_HOLD_CAPS_DELAY;
    }
    wait_ms(delay);
    (void)owned_keycode_release_chord(&lease);
    return true;
}

bool owned_keycode_register(uint16_t keycode) {
    owned_keycode_lease_t components;

    if (!owned_keycode_decompose(keycode, &components)) {
        owned_keycode_note_unsupported();
        return false;
    }
    return owned_keycode_acquire_components(&components);
}

bool owned_keycode_unregister(uint16_t keycode) {
    owned_keycode_lease_t components;

    if (!owned_keycode_decompose(keycode, &components)) {
        owned_keycode_note_unsupported();
        return false;
    }
    return owned_keycode_release_components(&components);
}

bool owned_keycode_tap(uint16_t keycode) {
    owned_keycode_lease_t lease = {0};

    if (!owned_keycode_acquire(keycode, &lease)) {
        return false;
    }

    uint8_t  basic = owned_keycode_basic_part(keycode);
    uint16_t delay = (basic == KC_CAPS_LOCK) ? TAP_HOLD_CAPS_DELAY : TAP_CODE_DELAY;

    wait_ms(delay);
    (void)owned_keycode_release(&lease);
    return true;
}

void owned_keycode_tap_literal(uint16_t keycode) {
    if (owned_keycode_is_supported(keycode)) {
        (void)owned_keycode_tap(keycode);
        return;
    }
    pointer_layer_policy_note_action(keycode, true);
    tap_code16(keycode);
    pointer_layer_policy_note_action(keycode, false);
}

void owned_keycode_track_physical_event(uint16_t keycode, keyrecord_t *record) {
    owned_keycode_lease_t components;
    uint8_t              *physical;

    if (!record || IS_NOEVENT(record->event) || !owned_keycode_decompose(keycode, &components) || !components.has_basic) {
        return;
    }
    physical = &owned_keycode_state.physical_refcounts[components.basic];
    if (record->event.pressed) {
        if (*physical == UINT8_MAX) {
            owned_keycode_note_saturation();
            return;
        }
        (*physical)++;
    } else if (*physical != 0u) {
        (*physical)--;
    } else {
        owned_keycode_note_underflow();
    }
}

bool owned_keycode_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    owned_keycode_lease_t components;

    if (!record || !owned_keycode_decompose(keycode, &components) || !components.has_basic) return false;
    // Counts are settled after the native path chooses whether to send an
    // edge, so a release still includes its own physical owner here.
    uint8_t physical = owned_keycode_state.physical_refcounts[components.basic];
    return owned_keycode_state.managed_refcounts[components.basic] != 0u || physical > (record->event.pressed ? 0u : 1u);
}

uint16_t owned_keycode_managed_usage_count(void) {
    return owned_keycode_state.managed_usage_count;
}

void owned_keycode_debug_snapshot(uint8_t keycode, owned_keycode_debug_snapshot_t *out) {
    if (!out) {
        return;
    }
    *out = (owned_keycode_debug_snapshot_t){
        .physical_count           = keycode < OWNED_KEYCODE_USAGE_CAPACITY ? owned_keycode_state.physical_refcounts[keycode] : 0u,
        .managed_count            = keycode < OWNED_KEYCODE_USAGE_CAPACITY ? owned_keycode_state.managed_refcounts[keycode] : 0u,
        .saturation_count         = owned_keycode_state.saturation_count,
        .underflow_count          = owned_keycode_state.underflow_count,
        .unsupported_count        = owned_keycode_state.unsupported_count,
        .idempotent_release_count = owned_keycode_state.idempotent_release_count,
    };
}

void owned_keycode_reset_for_test(void) {
    owned_keycode_state = (owned_keycode_state_t){0};
}

void noah_owned_keycode_reset_for_test(void) {
    owned_keycode_reset_for_test();
}
