// ────────────────────────────────────────────────────────────────────────────
// Owned Keycode Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include "owned_keycode.h"

#include "../pointing/policy/pointer_layer_policy.h"
#include "../state/ownership/keyboard_mod_ownership.h"

#define OWNED_KEYCODE_USAGE_CAPACITY ((uint16_t)QK_MOUSE_ACCELERATION_2 + 1u)

typedef struct {
    uint8_t  physical_refcounts[OWNED_KEYCODE_USAGE_CAPACITY];
    uint8_t  managed_refcounts[OWNED_KEYCODE_USAGE_CAPACITY];
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
    uint8_t mods_to_send = 0;

    if (keycode & QK_RMODS_MIN) {
        if (keycode & QK_LCTL) mods_to_send |= MOD_BIT(KC_RIGHT_CTRL);
        if (keycode & QK_LSFT) mods_to_send |= MOD_BIT(KC_RIGHT_SHIFT);
        if (keycode & QK_LALT) mods_to_send |= MOD_BIT(KC_RIGHT_ALT);
        if (keycode & QK_LGUI) mods_to_send |= MOD_BIT(KC_RIGHT_GUI);
    } else {
        if (keycode & QK_LCTL) mods_to_send |= MOD_BIT(KC_LEFT_CTRL);
        if (keycode & QK_LSFT) mods_to_send |= MOD_BIT(KC_LEFT_SHIFT);
        if (keycode & QK_LALT) mods_to_send |= MOD_BIT(KC_LEFT_ALT);
        if (keycode & QK_LGUI) mods_to_send |= MOD_BIT(KC_LEFT_GUI);
    }

    return mods_to_send;
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

static bool owned_keycode_acquire_basic(uint8_t basic) {
    uint8_t *managed = &owned_keycode_state.managed_refcounts[basic];

    if (*managed == UINT8_MAX) {
        if (owned_keycode_state.saturation_count != UINT16_MAX) {
            owned_keycode_state.saturation_count++;
        }
        return false;
    }
    if ((*managed)++ == 0u && owned_keycode_state.physical_refcounts[basic] == 0u) {
        pointer_layer_policy_note_action(basic, true);
        register_code(basic);
    }
    return true;
}

static bool owned_keycode_release_basic(uint8_t basic) {
    uint8_t *managed = &owned_keycode_state.managed_refcounts[basic];

    if (*managed == 0u) {
        if (owned_keycode_state.underflow_count != UINT16_MAX) {
            owned_keycode_state.underflow_count++;
        }
        return false;
    }
    (*managed)--;
    if (*managed == 0u && owned_keycode_state.physical_refcounts[basic] == 0u) {
        unregister_code(basic);
        pointer_layer_policy_note_action(basic, false);
    }
    return true;
}

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
// frames before the button goes down. A modifier that was already down needs no
// head start: no report was sent for it, so there is nothing to outrun.
static bool owned_keycode_acquire_components(const owned_keycode_lease_t *components) {
    bool modifier_report_sent = false;

    if (components->mods != 0u && !keyboard_mod_ownership_can_register_mods(components->mods)) {
        if (owned_keycode_state.saturation_count != UINT16_MAX) {
            owned_keycode_state.saturation_count++;
        }
        return false;
    }

    if (components->mods != 0u) {
        modifier_report_sent = keyboard_mod_ownership_register_mods(components->mods);
    }

    if (components->has_basic) {
        if (modifier_report_sent && IS_MOUSEKEY_BUTTON(components->basic)) {
            wait_ms(OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS);
        }
        if (!owned_keycode_acquire_basic(components->basic)) {
            if (components->mods != 0u) {
                keyboard_mod_ownership_unregister_mods(components->mods);
            }
            return false;
        }
    }

    return true;
}

// Teardown mirrors the acquire order: the usage leaves the report first, so a
// modifier never outlives the usage it was qualifying.
static bool owned_keycode_release_components(const owned_keycode_lease_t *components) {
    if ((components->has_basic && owned_keycode_state.managed_refcounts[components->basic] == 0u) || (components->mods != 0u && !keyboard_mod_ownership_can_unregister_mods(components->mods))) {
        if (owned_keycode_state.underflow_count != UINT16_MAX) {
            owned_keycode_state.underflow_count++;
        }
        return false;
    }

    if (components->has_basic) {
        (void)owned_keycode_release_basic(components->basic);
    }
    if (components->mods != 0u) {
        keyboard_mod_ownership_unregister_mods(components->mods);
    }

    return true;
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

void owned_keycode_track_physical_event(uint16_t keycode, keyrecord_t *record) {
    owned_keycode_lease_t components;
    uint8_t              *physical;

    if (!record || IS_NOEVENT(record->event) || !owned_keycode_decompose(keycode, &components) || !components.has_basic) {
        return;
    }
    physical = &owned_keycode_state.physical_refcounts[components.basic];
    if (record->event.pressed) {
        if (*physical == UINT8_MAX) {
            if (owned_keycode_state.saturation_count != UINT16_MAX) {
                owned_keycode_state.saturation_count++;
            }
            return;
        }
        (*physical)++;
    } else if (*physical != 0u) {
        (*physical)--;
    } else if (owned_keycode_state.underflow_count != UINT16_MAX) {
        owned_keycode_state.underflow_count++;
    }
}

bool owned_keycode_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    owned_keycode_lease_t components;

    return record && owned_keycode_decompose(keycode, &components) && components.has_basic && owned_keycode_state.managed_refcounts[components.basic] != 0u;
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
