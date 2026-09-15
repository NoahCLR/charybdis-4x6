// ────────────────────────────────────────────────────────────────────────────
// Effective Live RGB Runtime View
// ────────────────────────────────────────────────────────────────────────────

#include "effective_rgb_runtime.h"

#include <limits.h>
#include <string.h>

static noah_effective_rgb_runtime_t *installed_runtime;

static uint32_t next_epoch(uint32_t current) {
    current++;
    return current == 0u ? 1u : current;
}

static bool identity_equal(noah_effective_profile_identity_t left, noah_effective_profile_identity_t right) {
    return left.generation == right.generation && left.payload_crc32 == right.payload_crc32 && left.payload_digest == right.payload_digest && left.compiled_default_digest == right.compiled_default_digest && left.action_abi_digest == right.action_abi_digest && left.origin == right.origin && left.kind == right.kind;
}

void noah_effective_rgb_runtime_init(noah_effective_rgb_runtime_t *runtime) {
    if (!runtime) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->banks[0].valid = true;
    runtime->banks[1].valid = true;
    runtime->initialized    = true;
}

bool noah_effective_rgb_runtime_install(noah_effective_rgb_runtime_t *runtime) {
    if (!runtime || !runtime->initialized || (installed_runtime && installed_runtime != runtime)) {
        return false;
    }
    installed_runtime = runtime;
    return true;
}

void noah_effective_rgb_runtime_uninstall(noah_effective_rgb_runtime_t *runtime) {
    if (installed_runtime == runtime) {
        installed_runtime = NULL;
    }
}

void noah_effective_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view) {
    noah_effective_rgb_runtime_t *runtime = context;
    noah_effective_rgb_frame_t    next    = {0};
    uint8_t                       next_index;

    (void)previous;
    if (!runtime || !runtime->initialized) {
        return;
    }

    runtime->next_epoch    = next_epoch(runtime->next_epoch);
    next.identity          = active;
    next.publication_count = publication_count;
    next.epoch             = runtime->next_epoch;
    next.valid             = callback_view && identity_equal(callback_view->identity, active);
    if (next.valid && active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE && (callback_view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB) != 0u) {
        next.view = callback_view->profile.rgb;
        next.live = true;
    } else if (next.valid && (active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS || active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE)) {
        // Compiled identities and validated behavior-only profiles continue to
        // use the direct authored RGB configuration.
        next.live = false;
    }

    next_index = (uint8_t)(runtime->active_index ^ 1u);
    noah_runtime_publication_begin(&runtime->publication_sequence);
    runtime->banks[next_index] = next;
    runtime->active_index      = next_index;
    noah_runtime_publication_end(&runtime->publication_sequence);
}

static noah_effective_rgb_result_t copy_active(const noah_effective_rgb_runtime_t *runtime, noah_effective_rgb_frame_t *frame) {
    uint8_t attempt;

    if (!runtime || !runtime->initialized || !frame) {
        return NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
    }
    for (attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                    observed = noah_runtime_publication_observe(&runtime->publication_sequence);
        noah_effective_rgb_frame_t copied;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        copied = runtime->banks[runtime->active_index];
        if (noah_runtime_publication_settled(&runtime->publication_sequence, observed)) {
            *frame = copied;
            if (!copied.valid) {
                return NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
            }
            return copied.live ? NOAH_EFFECTIVE_RGB_OK : NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK;
        }
    }
    return NOAH_EFFECTIVE_RGB_BUSY;
}

noah_effective_rgb_result_t noah_effective_rgb_runtime_capture_frame(const noah_effective_rgb_runtime_t *runtime, noah_effective_rgb_frame_t *frame) {
    if (!frame) {
        return NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
    }
    *frame = (noah_effective_rgb_frame_t){0};
    return copy_active(runtime, frame);
}

noah_effective_rgb_result_t noah_effective_rgb_runtime_frame_status(const noah_effective_rgb_runtime_t *runtime, const noah_effective_rgb_frame_t *frame) {
    noah_effective_rgb_frame_t  active;
    noah_effective_rgb_result_t result;

    if (!frame || !frame->valid) {
        return NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
    }
    result = copy_active(runtime, &active);
    if (result != NOAH_EFFECTIVE_RGB_OK && result != NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK) {
        return result;
    }
    if (active.epoch != frame->epoch || active.publication_count != frame->publication_count || !identity_equal(active.identity, frame->identity) || active.live != frame->live) {
        return NOAH_EFFECTIVE_RGB_STALE;
    }
    return result;
}

noah_effective_rgb_result_t noah_effective_rgb_capture_frame(noah_effective_rgb_frame_t *frame) {
    if (!frame) {
        return NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
    }
    if (!installed_runtime) {
        *frame       = (noah_effective_rgb_frame_t){0};
        frame->valid = true;
        return NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK;
    }
    return noah_effective_rgb_runtime_capture_frame(installed_runtime, frame);
}

noah_effective_rgb_result_t noah_effective_rgb_frame_status(const noah_effective_rgb_frame_t *frame) {
    if (!installed_runtime) {
        return frame && frame->valid && !frame->live ? NOAH_EFFECTIVE_RGB_COMPILED_FALLBACK : NOAH_EFFECTIVE_RGB_INVALID_ARGUMENT;
    }
    return noah_effective_rgb_runtime_frame_status(installed_runtime, frame);
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_effective_rgb_runtime_t) <= NOAH_EFFECTIVE_RGB_RUNTIME_STATE_BUDGET_32BIT, "effective RGB runtime state exceeded its 32-bit regression policy");
_Static_assert(sizeof(noah_effective_rgb_frame_t) <= NOAH_EFFECTIVE_RGB_FRAME_BUDGET_32BIT, "effective RGB frame exceeded its 32-bit regression policy");
#endif
