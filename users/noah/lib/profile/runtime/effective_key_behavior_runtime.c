// ────────────────────────────────────────────────────────────────────────────
// Effective Live Key-Behavior Runtime View
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "effective_key_behavior_runtime.h"

#include <limits.h>
#include <string.h>

#include "profile_action_runtime_v1.h"

static noah_effective_key_behavior_runtime_t *installed_runtime;

static uint32_t next_epoch(uint32_t current) {
    current++;
    return current == 0u ? 1u : current;
}

void noah_effective_key_behavior_runtime_init(noah_effective_key_behavior_runtime_t *runtime) {
    if (!runtime) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->banks[0].valid = true;
    runtime->banks[1].valid = true;
    runtime->initialized    = true;
}

bool noah_effective_key_behavior_runtime_install(noah_effective_key_behavior_runtime_t *runtime) {
    if (!runtime || !runtime->initialized || (installed_runtime && installed_runtime != runtime)) {
        return false;
    }
    installed_runtime = runtime;
    return true;
}

void noah_effective_key_behavior_runtime_uninstall(noah_effective_key_behavior_runtime_t *runtime) {
    if (installed_runtime == runtime) {
        installed_runtime = NULL;
    }
}

static bool identity_equal(noah_effective_profile_identity_t left, noah_effective_profile_identity_t right) {
    return left.generation == right.generation && left.payload_crc32 == right.payload_crc32 && left.payload_digest == right.payload_digest && left.compiled_default_digest == right.compiled_default_digest && left.action_abi_digest == right.action_abi_digest && left.origin == right.origin && left.kind == right.kind;
}

void noah_effective_key_behavior_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view) {
    noah_effective_key_behavior_runtime_t *runtime = context;
    noah_effective_key_behavior_snapshot_t next    = {0};
    uint8_t                                next_index;

    (void)previous;
    if (!runtime || !runtime->initialized) {
        return;
    }

    runtime->next_epoch = next_epoch(runtime->next_epoch);
    next.identity       = active;
    next.publication_count = publication_count;
    next.epoch             = runtime->next_epoch;
    next.valid             = callback_view && identity_equal(callback_view->identity, active);
    if (next.valid && active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE && (callback_view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS) != 0u) {
        next.domain = callback_view->profile.key_behaviors;
        next.live   = true;
    } else if (next.valid && (active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS || active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE)) {
        // Compiled identities and validated RGB-only profiles use the direct
        // authored tables; the compiled virtual blob is never read here.
        next.live = false;
    }

    next_index = (uint8_t)(runtime->active_index ^ 1u);
    noah_runtime_publication_begin(&runtime->publication_sequence);
    runtime->banks[next_index] = next;
    runtime->active_index      = next_index;
    noah_runtime_publication_end(&runtime->publication_sequence);
}

noah_effective_key_behavior_result_t noah_effective_key_behavior_runtime_status(const noah_effective_key_behavior_runtime_t *runtime, noah_effective_key_behavior_snapshot_t *snapshot) {
    uint8_t attempt;

    if (!runtime || !runtime->initialized || !snapshot) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ARGUMENT;
    }
    for (attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                                observed = noah_runtime_publication_observe(&runtime->publication_sequence);
        noah_effective_key_behavior_snapshot_t copied;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        copied = runtime->banks[runtime->active_index];
        if (noah_runtime_publication_settled(&runtime->publication_sequence, observed)) {
            *snapshot = copied;
            if (!copied.valid) return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ARGUMENT;
            return copied.live ? NOAH_EFFECTIVE_KEY_BEHAVIOR_OK : NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK;
        }
    }
    return NOAH_EFFECTIVE_KEY_BEHAVIOR_BUSY;
}

static noah_effective_key_behavior_result_t convert_hold(const noah_key_behavior_hold_v1_t *source, hold_behavior_t *target) {
    uint16_t native_action;

    if (!source || !target || noah_profile_action_runtime_v1_to_native(&source->action, &native_action) != NOAH_PROFILE_ACTION_RUNTIME_V1_OK) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ACTION;
    }
    *target = (hold_behavior_t){
        .present   = true,
        .action    = native_action,
        .repeat_hz = source->repeat_hz,
    };
    switch (source->mode) {
        case NOAH_KEY_BEHAVIOR_HOLD_V1_PRESS_AND_HOLD_UNTIL_RELEASE:
            target->mode = HOLD_BEHAVIOR_PRESS_AND_HOLD_UNTIL_RELEASE;
            break;
        case NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_AT_THRESHOLD:
            target->mode = HOLD_BEHAVIOR_TAP_AT_HOLD_THRESHOLD;
            break;
        case NOAH_KEY_BEHAVIOR_HOLD_V1_REPEAT_WHILE_HELD:
            target->mode = HOLD_BEHAVIOR_REPEAT_WHILE_HELD;
            break;
        case NOAH_KEY_BEHAVIOR_HOLD_V1_TAP_ON_RELEASE_AFTER_HOLD:
            target->mode = HOLD_BEHAVIOR_TAP_ON_RELEASE_AFTER_HOLD;
            break;
        default:
            *target = hold_behavior_none();
            return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ACTION;
    }
    return NOAH_EFFECTIVE_KEY_BEHAVIOR_OK;
}

static noah_effective_key_behavior_result_t convert_step(const noah_key_behavior_step_v1_t *source, key_behavior_step_t *target) {
    uint16_t native_action;

    if (!source || !target) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ARGUMENT;
    }
    *target = key_behavior_step_none();
    if ((source->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_TAP) != 0u) {
        if (noah_profile_action_runtime_v1_to_native(&source->tap, &native_action) != NOAH_PROFILE_ACTION_RUNTIME_V1_OK) {
            return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ACTION;
        }
        target->tap = (tap_behavior_t){.present = true, .action = native_action};
    }
    if ((source->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_HOLD) != 0u && convert_hold(&source->hold, &target->hold) != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ACTION;
    }
    if ((source->presence_mask & NOAH_KEY_BEHAVIOR_DOMAIN_V1_STEP_HAS_LONG_HOLD) != 0u && convert_hold(&source->long_hold, &target->long_hold) != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ACTION;
    }
    return NOAH_EFFECTIVE_KEY_BEHAVIOR_OK;
}

static noah_effective_key_behavior_result_t codec_result(noah_profile_codec_v1_result_t result) {
    if (result == NOAH_PROFILE_CODEC_V1_OK) return NOAH_EFFECTIVE_KEY_BEHAVIOR_OK;
    if (result == NOAH_PROFILE_CODEC_V1_READ_ERROR) return NOAH_EFFECTIVE_KEY_BEHAVIOR_READ_ERROR;
    return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ARGUMENT;
}

noah_effective_key_behavior_result_t noah_effective_key_behavior_runtime_lookup(const noah_effective_key_behavior_runtime_t *runtime, uint16_t keycode, noah_effective_key_behavior_row_t *row) {
    noah_effective_key_behavior_snapshot_t snapshot;
    noah_key_behavior_row_v1_view_t         encoded_row;
    noah_profile_action_v1_t                target;
    noah_profile_codec_v1_error_t           error;
    noah_effective_key_behavior_result_t    result;
    bool                                    found;

    if (!row) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ARGUMENT;
    }
    *row   = (noah_effective_key_behavior_row_t){0};
    result = noah_effective_key_behavior_runtime_status(runtime, &snapshot);
    if (result != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) {
        return result;
    }
    if (noah_profile_action_runtime_v1_from_native(keycode, &target) != NOAH_PROFILE_ACTION_RUNTIME_V1_OK || target.kind == NOAH_PROFILE_ACTION_V1_NONE) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_NOT_FOUND;
    }
    result = codec_result(noah_key_behavior_domain_v1_find_target(&snapshot.domain, &target, &encoded_row, &found, &error));
    if (result != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK || !found) {
        return result == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK ? NOAH_EFFECTIVE_KEY_BEHAVIOR_NOT_FOUND : result;
    }

    row->epoch            = snapshot.epoch;
    row->row_index        = encoded_row.row_index;
    row->flags            = encoded_row.flags;
    row->step_count       = encoded_row.step_count;
    row->tap_hold_term    = encoded_row.tap_hold_term;
    row->longer_hold_term = encoded_row.longer_hold_term;
    row->multi_tap_term   = encoded_row.multi_tap_term;
    for (uint8_t index = 0u; index < encoded_row.step_count; index++) {
        noah_key_behavior_step_v1_t encoded_step;
        key_behavior_step_t         native_step;

        result = codec_result(noah_key_behavior_domain_v1_step_in_row(&snapshot.domain, &encoded_row, index, &encoded_step, &error));
        if (result != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK || convert_step(&encoded_step, &native_step) != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) {
            return result == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK ? NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ACTION : result;
        }
        if ((uint8_t)(encoded_step.tap_index + 1u) > row->authored_tap_depth) {
            row->authored_tap_depth = (uint8_t)(encoded_step.tap_index + 1u);
        }
        if (encoded_step.tap_index == 0u) {
            row->single = native_step;
        }
    }
    return NOAH_EFFECTIVE_KEY_BEHAVIOR_OK;
}

noah_effective_key_behavior_result_t noah_effective_key_behavior_runtime_step(const noah_effective_key_behavior_runtime_t *runtime, uint32_t epoch, uint8_t row_index, uint8_t tap_count, key_behavior_step_t *step) {
    noah_effective_key_behavior_snapshot_t snapshot;
    noah_key_behavior_row_v1_view_t         row;
    noah_profile_codec_v1_error_t           error;
    noah_effective_key_behavior_result_t    result;

    if (!step || tap_count == 0u || tap_count > KEY_BEHAVIOR_MAX_TAP_COUNT) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_INVALID_ARGUMENT;
    }
    *step  = key_behavior_step_none();
    result = noah_effective_key_behavior_runtime_status(runtime, &snapshot);
    if (result != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) {
        return result;
    }
    if (snapshot.epoch != epoch) {
        return NOAH_EFFECTIVE_KEY_BEHAVIOR_STALE;
    }
    result = codec_result(noah_key_behavior_domain_v1_row_at(&snapshot.domain, row_index, &row, &error));
    if (result != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) {
        return result;
    }
    for (uint8_t index = 0u; index < row.step_count; index++) {
        noah_key_behavior_step_v1_t encoded_step;

        result = codec_result(noah_key_behavior_domain_v1_step_in_row(&snapshot.domain, &row, index, &encoded_step, &error));
        if (result != NOAH_EFFECTIVE_KEY_BEHAVIOR_OK) {
            return result;
        }
        if (encoded_step.tap_index == (uint8_t)(tap_count - 1u)) {
            return convert_step(&encoded_step, step);
        }
        if (encoded_step.tap_index >= tap_count) {
            break;
        }
    }
    return NOAH_EFFECTIVE_KEY_BEHAVIOR_NOT_FOUND;
}

noah_effective_key_behavior_result_t noah_effective_key_behavior_lookup(uint16_t keycode, noah_effective_key_behavior_row_t *row) {
    return installed_runtime ? noah_effective_key_behavior_runtime_lookup(installed_runtime, keycode, row) : NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK;
}

noah_effective_key_behavior_result_t noah_effective_key_behavior_step(uint32_t epoch, uint8_t row_index, uint8_t tap_count, key_behavior_step_t *step) {
    return installed_runtime ? noah_effective_key_behavior_runtime_step(installed_runtime, epoch, row_index, tap_count, step) : NOAH_EFFECTIVE_KEY_BEHAVIOR_COMPILED_FALLBACK;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_effective_key_behavior_runtime_t) <= NOAH_EFFECTIVE_KEY_BEHAVIOR_RUNTIME_STATE_BUDGET_32BIT, "effective key-behavior runtime state exceeded its 32-bit regression policy");
#endif
