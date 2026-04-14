// ────────────────────────────────────────────────────────────────────────────
// Keyboard Modifier Ownership
// ────────────────────────────────────────────────────────────────────────────

#ifdef CONSOLE_ENABLE
#    include "print.h"
#endif

#include "keyboard_mod_ownership.h"
#include "../runtime/runtime_context_internal.h"

static const uint8_t keyboard_mod_ownership_mod_masks[8] = {
    MOD_BIT(KC_LEFT_CTRL), MOD_BIT(KC_LEFT_SHIFT), MOD_BIT(KC_LEFT_ALT), MOD_BIT(KC_LEFT_GUI), MOD_BIT(KC_RIGHT_CTRL), MOD_BIT(KC_RIGHT_SHIFT), MOD_BIT(KC_RIGHT_ALT), MOD_BIT(KC_RIGHT_GUI),
};

static int8_t keyboard_mod_ownership_index_for_keycode(uint16_t keycode) {
    switch (keycode) {
        case KC_LEFT_CTRL:
            return 0;
        case KC_LEFT_SHIFT:
            return 1;
        case KC_LEFT_ALT:
            return 2;
        case KC_LEFT_GUI:
            return 3;
        case KC_RIGHT_CTRL:
            return 4;
        case KC_RIGHT_SHIFT:
            return 5;
        case KC_RIGHT_ALT:
            return 6;
        case KC_RIGHT_GUI:
            return 7;
        default:
            return -1;
    }
}

#ifndef KEYBOARD_MOD_OWNERSHIP_MANAGED_DRIFT_WARN_THRESHOLD
#    define KEYBOARD_MOD_OWNERSHIP_MANAGED_DRIFT_WARN_THRESHOLD 8u
#endif

static noah_keyboard_mod_ownership_state_t *keyboard_mod_ownership_state(void) {
    return &noah_runtime_context()->keyboard_mod_ownership;
}

static void keyboard_mod_ownership_validate_state(const char *context) {
#ifdef CONSOLE_ENABLE
    noah_keyboard_mod_ownership_state_t *state = keyboard_mod_ownership_state();

    enum {
        KEYBOARD_MOD_OWNERSHIP_WARNING_NONE      = 0,
        KEYBOARD_MOD_OWNERSHIP_WARNING_DRIFT     = 1u << 0,
        KEYBOARD_MOD_OWNERSHIP_WARNING_STUCK_BIT = 1u << 1,
    };

    uint8_t report_mods = get_mods();

    for (uint8_t i = 0; i < ARRAY_SIZE(keyboard_mod_ownership_mod_masks); i++) {
        uint8_t warnings = KEYBOARD_MOD_OWNERSHIP_WARNING_NONE;

        if (state->managed_refcounts[i] > state->physical_refcounts[i] + KEYBOARD_MOD_OWNERSHIP_MANAGED_DRIFT_WARN_THRESHOLD) {
            warnings |= KEYBOARD_MOD_OWNERSHIP_WARNING_DRIFT;
        }

        if (state->managed_refcounts[i] == 0 && state->physical_refcounts[i] == 0 && (report_mods & keyboard_mod_ownership_mod_masks[i]) != 0) {
            warnings |= KEYBOARD_MOD_OWNERSHIP_WARNING_STUCK_BIT;
        }

        if (warnings == state->warned_state[i]) {
            continue;
        }

        state->warned_state[i] = warnings;
        if (warnings == KEYBOARD_MOD_OWNERSHIP_WARNING_NONE) {
            continue;
        }

        uprintf("Keyboard mod ownership warning after %s: index=%u mask=0x%02X physical=%u managed=%u report=0x%02X flags=0x%02X\n", context, (unsigned int)i, (unsigned int)keyboard_mod_ownership_mod_masks[i], (unsigned int)state->physical_refcounts[i], (unsigned int)state->managed_refcounts[i], (unsigned int)report_mods, (unsigned int)warnings);
    }
#else
    (void)context;
#endif
}

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    noah_keyboard_mod_ownership_state_t *state = keyboard_mod_ownership_state();
    int8_t                               index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0 || !record || IS_NOEVENT(record->event)) {
        return;
    }

    if (record->event.pressed) {
        if (state->physical_refcounts[index] < UINT8_MAX) {
            state->physical_refcounts[index]++;
        }
    } else if (state->physical_refcounts[index] > 0) {
        state->physical_refcounts[index]--;
    }

    keyboard_mod_ownership_validate_state("track_physical_keycode_event");
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    noah_keyboard_mod_ownership_state_t *state = keyboard_mod_ownership_state();
    int8_t                               index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0 || !record || record->event.pressed) {
        return false;
    }

    keyboard_mod_ownership_validate_state("should_suppress_default");
    return state->managed_refcounts[index] > 0;
}

void keyboard_mod_ownership_register_mods(uint8_t mods) {
    noah_keyboard_mod_ownership_state_t *state         = keyboard_mod_ownership_state();
    bool                                 report_needed = false;

    if (mods == 0) {
        return;
    }

    for (uint8_t i = 0; i < ARRAY_SIZE(keyboard_mod_ownership_mod_masks); i++) {
        if (!(mods & keyboard_mod_ownership_mod_masks[i])) {
            continue;
        }

        if (state->managed_refcounts[i] < UINT8_MAX) {
            state->managed_refcounts[i]++;
        }

        // keyboard_mod_state_suspend() intentionally clears the live report
        // bits without touching ownership refcounts. If a nested action
        // registers the same managed modifier while suspended, re-assert the
        // real mod bit so the nested action still sees the modifier.
        if ((get_mods() & keyboard_mod_ownership_mod_masks[i]) == 0 && (state->managed_refcounts[i] > 0 || state->physical_refcounts[i] > 0)) {
            add_mods(keyboard_mod_ownership_mod_masks[i]);
            report_needed = true;
        }
    }

    if (report_needed) {
        send_keyboard_report();
    }

    keyboard_mod_ownership_validate_state("register_mods");
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    noah_keyboard_mod_ownership_state_t *state         = keyboard_mod_ownership_state();
    bool                                 report_needed = false;

    if (mods == 0) {
        return;
    }

    for (uint8_t i = 0; i < ARRAY_SIZE(keyboard_mod_ownership_mod_masks); i++) {
        if (!(mods & keyboard_mod_ownership_mod_masks[i]) || state->managed_refcounts[i] == 0) {
            continue;
        }

        state->managed_refcounts[i]--;
        if (state->managed_refcounts[i] == 0 && state->physical_refcounts[i] == 0) {
            del_mods(keyboard_mod_ownership_mod_masks[i]);
            report_needed = true;
        }
    }

    if (report_needed) {
        send_keyboard_report();
    }

    keyboard_mod_ownership_validate_state("unregister_mods");
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    int8_t index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0) {
        return;
    }

    keyboard_mod_ownership_register_mods(keyboard_mod_ownership_mod_masks[index]);
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    int8_t index = keyboard_mod_ownership_index_for_keycode(keycode);

    if (index < 0) {
        return;
    }

    keyboard_mod_ownership_unregister_mods(keyboard_mod_ownership_mod_masks[index]);
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    noah_keyboard_mod_ownership_state_t *state = keyboard_mod_ownership_state();
    uint8_t                              mask  = 0;

    if (mods == 0) {
        return 0;
    }

    for (uint8_t i = 0; i < ARRAY_SIZE(keyboard_mod_ownership_mod_masks); i++) {
        uint8_t mod_mask = keyboard_mod_ownership_mod_masks[i];

        if (!(mods & mod_mask)) {
            continue;
        }

        if (state->managed_refcounts[i] > 0 && state->physical_refcounts[i] == 0) {
            mask |= mod_mask;
        }
    }

    return mask;
}

void keyboard_mod_ownership_debug_snapshot(keyboard_mod_ownership_debug_snapshot_t *out) {
    noah_keyboard_mod_ownership_state_t *state = keyboard_mod_ownership_state();

    if (!out) {
        return;
    }

    *out = (keyboard_mod_ownership_debug_snapshot_t){
        .live_state =
            {
                .real           = get_mods(),
                .weak           = get_weak_mods(),
                .oneshot        = get_oneshot_mods(),
                .oneshot_locked = get_oneshot_locked_mods(),
            },
    };

    memcpy(out->physical_refcounts, state->physical_refcounts, sizeof(state->physical_refcounts));
    memcpy(out->managed_refcounts, state->managed_refcounts, sizeof(state->managed_refcounts));
}

void keyboard_mod_ownership_reset_for_test(void) {
    memset(keyboard_mod_ownership_state(), 0, sizeof(*keyboard_mod_ownership_state()));
}
