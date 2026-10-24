// ────────────────────────────────────────────────────────────────────────────
// Active-Profile RGB Runtime View
// ────────────────────────────────────────────────────────────────────────────

#include "active_profile_rgb_runtime.h"

#include <limits.h>
#include <string.h>
#ifdef RGB_MATRIX_ENABLE
#    include "../schema/profile_rgb_compiled_v1.h"
#endif

static noah_active_profile_rgb_runtime_t *installed_runtime;

static uint32_t next_epoch(uint32_t current) {
    current++;
    return current == 0u ? 1u : current;
}

static bool identity_equal(noah_active_profile_identity_t left, noah_active_profile_identity_t right) {
    return left.generation == right.generation && left.payload_crc32 == right.payload_crc32 && left.payload_digest == right.payload_digest && left.compiled_default_digest == right.compiled_default_digest && left.action_abi_digest == right.action_abi_digest && left.origin == right.origin && left.kind == right.kind;
}

void noah_active_profile_rgb_runtime_init(noah_active_profile_rgb_runtime_t *runtime) {
    if (!runtime) {
        return;
    }
    memset(runtime, 0, sizeof(*runtime));
    runtime->banks[0].valid = true;
    runtime->banks[1].valid = true;
    runtime->initialized    = true;
}

bool noah_active_profile_rgb_runtime_install(noah_active_profile_rgb_runtime_t *runtime) {
    if (!runtime || !runtime->initialized || (installed_runtime && installed_runtime != runtime)) {
        return false;
    }
    installed_runtime = runtime;
    return true;
}

void noah_active_profile_rgb_runtime_uninstall(noah_active_profile_rgb_runtime_t *runtime) {
    if (installed_runtime == runtime) {
        installed_runtime = NULL;
    }
}

void noah_active_profile_rgb_runtime_invalidate(void *context, uint32_t publication_count, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *callback_view) {
    noah_active_profile_rgb_runtime_t *runtime = context;
    noah_active_profile_rgb_frame_t    next    = {0};
    uint8_t                            next_index;

    (void)previous;
    if (!runtime || !runtime->initialized) {
        return;
    }

    runtime->next_epoch    = next_epoch(runtime->next_epoch);
    next.identity          = active;
    next.publication_count = publication_count;
    next.epoch             = runtime->next_epoch;
    next.valid             = callback_view && identity_equal(callback_view->identity, active);
    if (next.valid && (active.kind == NOAH_ACTIVE_PROFILE_KIND_VALIDATED_PROFILE || active.kind == NOAH_ACTIVE_PROFILE_KIND_WORKING_COPY) && (callback_view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB) != 0u) {
        next.view = callback_view->profile.rgb;
        next.live = true;
    } else if (next.valid && (active.kind == NOAH_ACTIVE_PROFILE_KIND_COMPILED_DEFAULTS || active.kind == NOAH_ACTIVE_PROFILE_KIND_VALIDATED_PROFILE || active.kind == NOAH_ACTIVE_PROFILE_KIND_WORKING_COPY)) {
#ifdef RGB_MATRIX_ENABLE
        const noah_profile_rgb_v1_view_t *compiled = noah_profile_rgb_compiled_v1_view();
        next.valid                                 = compiled != NULL;
        next.live                                  = next.valid;
        if (compiled) next.view = *compiled;
#else
        next.live = false;
#endif
    }

    next_index = (uint8_t)(runtime->active_index ^ 1u);
    noah_runtime_publication_begin(&runtime->publication_sequence);
    runtime->banks[next_index] = next;
    runtime->active_index      = next_index;
    noah_runtime_publication_end(&runtime->publication_sequence);
}

static noah_active_profile_rgb_result_t copy_active(const noah_active_profile_rgb_runtime_t *runtime, noah_active_profile_rgb_frame_t *frame) {
    uint8_t attempt;

    if (!runtime || !runtime->initialized || !frame) {
        return NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
    }
    for (attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                         observed = noah_runtime_publication_observe(&runtime->publication_sequence);
        noah_active_profile_rgb_frame_t copied;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        copied = runtime->banks[runtime->active_index];
        if (noah_runtime_publication_settled(&runtime->publication_sequence, observed)) {
            *frame = copied;
            if (!copied.valid) {
                return NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
            }
            return copied.live ? NOAH_ACTIVE_PROFILE_RGB_OK : NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK;
        }
    }
    return NOAH_ACTIVE_PROFILE_RGB_BUSY;
}

noah_active_profile_rgb_result_t noah_active_profile_rgb_runtime_capture_frame(const noah_active_profile_rgb_runtime_t *runtime, noah_active_profile_rgb_frame_t *frame) {
    if (!frame) {
        return NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
    }
    *frame = (noah_active_profile_rgb_frame_t){0};
    return copy_active(runtime, frame);
}

noah_active_profile_rgb_result_t noah_active_profile_rgb_runtime_frame_status(const noah_active_profile_rgb_runtime_t *runtime, const noah_active_profile_rgb_frame_t *frame) {
    noah_active_profile_rgb_frame_t  active;
    noah_active_profile_rgb_result_t result;

    if (!frame || !frame->valid) {
        return NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
    }
    result = copy_active(runtime, &active);
    if (result != NOAH_ACTIVE_PROFILE_RGB_OK && result != NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK) {
        return result;
    }
    if (active.epoch != frame->epoch || active.publication_count != frame->publication_count || !identity_equal(active.identity, frame->identity) || active.live != frame->live) {
        return NOAH_ACTIVE_PROFILE_RGB_STALE;
    }
    return result;
}

noah_active_profile_rgb_result_t noah_active_profile_rgb_capture_frame(noah_active_profile_rgb_frame_t *frame) {
    if (!frame) {
        return NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
    }
    if (!installed_runtime) {
        *frame       = (noah_active_profile_rgb_frame_t){0};
        frame->valid = true;
        return NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK;
    }
    return noah_active_profile_rgb_runtime_capture_frame(installed_runtime, frame);
}

noah_active_profile_rgb_result_t noah_active_profile_rgb_frame_status(const noah_active_profile_rgb_frame_t *frame) {
    if (!installed_runtime) {
        return frame && frame->valid && !frame->live ? NOAH_ACTIVE_PROFILE_RGB_COMPILED_FALLBACK : NOAH_ACTIVE_PROFILE_RGB_INVALID_ARGUMENT;
    }
    return noah_active_profile_rgb_runtime_frame_status(installed_runtime, frame);
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_active_profile_rgb_runtime_t) <= NOAH_ACTIVE_PROFILE_RGB_RUNTIME_STATE_BUDGET_32BIT, "active-profile RGB runtime state exceeded its 32-bit regression policy");
_Static_assert(sizeof(noah_active_profile_rgb_frame_t) <= NOAH_ACTIVE_PROFILE_RGB_FRAME_BUDGET_32BIT, "active-profile RGB frame exceeded its 32-bit regression policy");
#endif

uint32_t noah_active_profile_rgb_publication_epoch(void) {
    return installed_runtime ? installed_runtime->next_epoch : 0u;
}
