// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Core Foundation
// ────────────────────────────────────────────────────────────────────────────
//
// Single-authority handled-key runtime state and reducer surface.
// key_runtime_core owns the canonical press/tap/lease/pending-release state plus
// the effect-plan APIs consumed by the QMK-facing orchestration layer under
// users/noah/lib/key/runtime/. That orchestration layer feeds physical events
// into key_runtime_core, then projects the returned effects into the existing
// ownership registries and QMK hook seams.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../../behavior/handled_key.h"
#include "../slot/keypos_codec.h"
#include "../feedback_kind.h"
#include "../planning/effect_queue.h"
#include "../slot/slot_interaction.h"
#include "../../../pointing/defs/pd_mode_flags.h"
#include "../../../state/modifiers/keyboard_mod_state.h"

#define KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY ((uint16_t)(MATRIX_ROWS * MATRIX_COLS))
#define KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT ((KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY + 31u) / 32u)
#define KEY_RUNTIME_CORE_LEASE_CAPACITY ((uint16_t)(KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY * 4u))
#define KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY ((uint16_t)(KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY * 2u))
#define KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY 16u
#define KEY_RUNTIME_CORE_EFFECT_PLAN_CAPACITY 14u

#ifndef KEY_RUNTIME_CORE_TOKEN_ID_MAX
#    define KEY_RUNTIME_CORE_TOKEN_ID_MAX UINT16_MAX
#elif !defined(NOAH_HOST_TEST_ENV)
#    error "KEY_RUNTIME_CORE_TOKEN_ID_MAX may only be overridden by host tests"
#endif

_Static_assert(KEY_RUNTIME_CORE_TOKEN_ID_MAX > 0u && KEY_RUNTIME_CORE_TOKEN_ID_MAX <= UINT16_MAX, "press-token ID domain must contain nonzero uint16_t values");
_Static_assert(KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY < UINT8_MAX, "pending-release FIFO indices require an unused uint8_t sentinel");

enum {
    KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE = UINT8_MAX,
};

typedef enum {
    RUNTIME_EVENT_KIND_KEY_DOWN = 0,
    RUNTIME_EVENT_KIND_KEY_UP,
    RUNTIME_EVENT_KIND_TIMER_ADVANCE,
    RUNTIME_EVENT_KIND_SCAN,
    RUNTIME_EVENT_KIND_POINTER_REPORT,
    RUNTIME_EVENT_KIND_REMOTE_SNAPSHOT,
} runtime_event_kind_t;

typedef struct {
    uint16_t keycode;
    keypos_t key_pos;
} runtime_key_event_t;

typedef struct {
    uint16_t advance_ms;
} runtime_timer_advance_t;

typedef struct {
    report_mouse_t report;
} runtime_pointer_report_t;

typedef struct {
    pd_mode_mask_t active_mode;
    pd_mode_mask_t locked_mode;
} runtime_remote_snapshot_t;

typedef struct {
    runtime_event_kind_t kind;
    union {
        runtime_key_event_t       key_event;
        runtime_timer_advance_t   timer_advance;
        runtime_pointer_report_t  pointer_report;
        runtime_remote_snapshot_t remote_snapshot;
    } data;
} runtime_event_t;

typedef enum {
    PRESS_TOKEN_PHASE_IDLE = 0,
    PRESS_TOKEN_PHASE_PRESSED,
    PRESS_TOKEN_PHASE_HOLD_PENDING,
    PRESS_TOKEN_PHASE_HELD,
    PRESS_TOKEN_PHASE_RELEASE_PENDING,
    PRESS_TOKEN_PHASE_RELEASED,
    PRESS_TOKEN_PHASE_CANCELLED,
} press_token_phase_t;

typedef struct {
    bool                           active;
    uint16_t                       token_id;
    uint16_t                       physical_keycode;
    uint16_t                       resolved_keycode;
    uint16_t                       observed_release_keycode;
    uint16_t                       pressed_at;
    uint16_t                       released_at;
    uint16_t                       hold_term_ms;
    uint16_t                       longer_hold_term_ms;
    uint32_t                       feedback_sequence;
    press_token_phase_t            phase;
    uint8_t                        feedback_level;
    bool                           handled_key;
    bool                           tap_outcome_available;
    bool                           pd_mode_was_locked_on_press;
    bool                           pd_mode_lock_consumed_on_press;
    bool                           other_press_interrupted;
    bool                           momentary_layer_tap_interrupted;
    bool                           resolved_from_transparent;
    bool                           pending_release_emission;
    bool                           release_keycode_mismatched;
    key_runtime_slot_interaction_t interaction;
    key_runtime_slot_phase_t       slot_phase;
} press_token_t;

typedef enum {
    KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_NONE = 0,
    KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_DELAYED_ACTION,
    KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_RELEASE_HOLD_PENDING,
    KEY_RUNTIME_TAP_SERIES_BRANCH_CONFIRM_THRESHOLD_HOLD,
} key_runtime_tap_series_branch_confirm_kind_t;

typedef struct {
    bool                  active;
    uint16_t              keycode;
    uint8_t               tap_count;
    bool                  pending_hold;
    bool                  branch_confirmed;
    bool                  branch_confirming;
    uint8_t               branch_confirm_kind;
    uint8_t               branch_confirm_tap_count;
    uint16_t              branch_confirm_started_at;
    uint16_t              branch_confirm_duration_ms;
    uint16_t              branch_confirm_action;
    uint8_t               branch_confirm_repeat_count;
    delayed_action_mods_t branch_confirm_mods;
    bool                  branch_confirm_tap_commit_feedback;
    bool                  branch_confirm_action_feedback;
    uint8_t               branch_confirm_action_feedback_kind;
    bool                  branch_confirm_long_hold_level;
    uint16_t              single_action;
    uint16_t              tap_action;
    uint8_t               tap_repeat_count;
    bool                  tap_branch_has_authored_step;
    bool                  tap_branch_has_authored_tap;
    bool                  has_more_taps;
    bool                  authored_has_more_taps;
    hold_behavior_t       hold;
    hold_behavior_t       long_hold;
    uint16_t              tap_hold_term_ms;
    uint16_t              branch_confirm_term_ms;
    uint16_t              last_action;
    uint16_t              last_tap_at;
    uint16_t              tap_term_ms;
    uint32_t              feedback_sequence;
    keyboard_mod_state_t  saved_mod_state;
} tap_series_t;

typedef struct {
    KEY_RUNTIME_EFFECT_QUEUE_FIELDS(KEY_RUNTIME_CORE_EFFECT_PLAN_CAPACITY);
    void (*sink)(void *ctx, key_runtime_effect_t effect);
    void *sink_ctx;
} key_runtime_core_effect_plan_t;

_Static_assert(sizeof(key_runtime_core_effect_plan_t) <= 196u, "key_runtime_core_effect_plan_t must stay within the approved stack budget");

typedef enum {
    KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE              = (1u << 0),
    KEY_RUNTIME_PENDING_RELEASE_FLAG_TAP_COMMIT_FEEDBACK = (1u << 1),
} key_runtime_pending_release_flag_t;

typedef struct {
    uint16_t                    owner_token_id;
    uint16_t                    action;
    keyboard_mod_state_t        mods;
    uint8_t                     next_queue_index;
    key_runtime_packed_keypos_t packed_key_pos;
    uint8_t                     flags;
} pending_release_slot_t;

_Static_assert(sizeof(pending_release_slot_t) <= 12u, "pending_release_slot_t must stay compact");

typedef struct {
    bool                 active;
    bool                 tap_commit_feedback;
    uint16_t             owner_token_id;
    keypos_t             key_pos;
    uint16_t             action;
    keyboard_mod_state_t mods;
} pending_release_t;

typedef enum {
    LEASE_KIND_NONE = 0,
    LEASE_KIND_LAYER,
    LEASE_KIND_MODIFIER,
    LEASE_KIND_HELD_ACTION,
    LEASE_KIND_REPEAT,
    LEASE_KIND_PD_MODE,
    LEASE_KIND_POINTER_ANCHOR,
} lease_kind_t;

typedef struct {
    bool                        active;
    uint8_t                     kind;
    uint16_t                    owner_token_id;
    key_runtime_packed_keypos_t owner_packed_key_pos;
    uint16_t                    feedback_started_at;
    uint32_t                    feedback_sequence;
    union {
        uint8_t layer;
        struct {
            uint8_t modifiers;
            bool    physical;
        } modifier;
        uint16_t       action;
        pd_mode_mask_t pd_mode;
        struct {
            uint16_t action;
            uint8_t  repeat_hz;
        } repeat;
        struct {
            uint8_t layer;
            bool    keep_typing_surface;
        } pointer_anchor;
    } data;
} lease_t;

_Static_assert(sizeof(lease_t) <= 16u, "lease_t must stay compact");

typedef enum {
    PERSISTENT_INTENT_KIND_NONE = 0,
    PERSISTENT_INTENT_KIND_LAYER_LOCK,
    PERSISTENT_INTENT_KIND_PD_MODE_LOCK,
    PERSISTENT_INTENT_KIND_POINTER_TOGGLE,
} persistent_intent_kind_t;

typedef struct {
    bool                     active;
    persistent_intent_kind_t kind;
    union {
        uint8_t        layer;
        pd_mode_mask_t pd_mode;
        uint8_t        pointer_layer;
        struct {
            uint8_t        pointer_layer;
            pd_mode_mask_t mode;
        } pointer_toggle;
    } data;
} persistent_intent_t;

typedef struct {
    layer_state_t        layer_state;
    layer_state_t        locked_layer_mask;
    keyboard_mod_state_t keyboard_mod_state;
    uint8_t              keyboard_managed_mod_mask;
    uint8_t              keyboard_physical_mod_mask;
    pd_mode_mask_t       pd_mode_local_active;
    pd_mode_mask_t       pd_mode_local_locked;
    bool                 pointer_anchor_active;
    bool                 pointer_pd_mode_anchor_active;
    bool                 pointer_prefers_typing_layer;
    bool                 pointer_toggle_enabled;
} key_runtime_core_shadow_projection_t;

typedef struct {
    layer_state_t        layer_state;
    layer_state_t        locked_layer_mask;
    keyboard_mod_state_t keyboard_mod_state;
    uint8_t              keyboard_managed_mod_mask;
    uint8_t              keyboard_physical_mod_mask;
    pd_mode_mask_t       pd_mode_local_active;
    pd_mode_mask_t       pd_mode_local_locked;
    pd_mode_mask_t       pd_mode_display_active;
    pd_mode_mask_t       pd_mode_display_locked;
    bool                 pointer_anchor_active;
    bool                 pointer_pd_mode_anchor_active;
    bool                 pointer_prefers_typing_layer;
    bool                 pointer_toggle_enabled;
    bool                 pointer_sniping_layer_active;
    int8_t               pointer_key_tracker;
    uint8_t              pointer_layer;
    uint8_t              active_slot_count;
    uint8_t              pending_multi_tap_slot_count;
    uint8_t              deferred_release_count;
    uint8_t              deferred_release_blocker_count;
    uint8_t              deferred_release_timed_blocker_count;
    layer_state_t        core_shadow_layer_state;
    layer_state_t        core_shadow_locked_layer_mask;
    keyboard_mod_state_t core_shadow_keyboard_mod_state;
    uint8_t              core_shadow_keyboard_managed_mod_mask;
    uint8_t              core_shadow_keyboard_physical_mod_mask;
    pd_mode_mask_t       core_shadow_pd_mode_local_active;
    pd_mode_mask_t       core_shadow_pd_mode_local_locked;
    bool                 core_shadow_pointer_anchor_active;
    bool                 core_shadow_pointer_pd_mode_anchor_active;
    bool                 core_shadow_pointer_prefers_typing_layer;
    bool                 core_shadow_pointer_toggle_enabled;
    uint8_t              core_press_token_count;
    uint8_t              core_tap_series_count;
    uint8_t              core_lease_count;
    uint8_t              core_pending_release_count;
    uint8_t              core_pending_release_high_water_mark;
    uint8_t              core_pending_release_validation_failure_count;
    uint8_t              core_deferred_release_blocker_count;
    uint8_t              core_deferred_release_timed_blocker_count;
    uint8_t              core_persistent_intent_count;
    uint8_t              core_release_keycode_mismatch_count;
    uint8_t              core_orphan_release_count;
    uint8_t              core_cancelled_press_count;
    uint8_t              core_token_allocation_failure_count;
} projection_snapshot_t;

typedef struct {
    // Owner-ID stores are part of the allocator's liveness contract. Any new
    // owner-bearing store added here must also be scanned by
    // key_runtime_core_token_id_is_reserved() and covered by its host test.
    press_token_t                        press_tokens[KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY];
    tap_series_t                         tap_series[KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY];
    uint32_t                             press_token_active_bitmap[KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT];
    uint32_t                             tap_series_active_bitmap[KEY_RUNTIME_CORE_ACTIVE_BITMAP_WORD_COUNT];
    lease_t                              leases[KEY_RUNTIME_CORE_LEASE_CAPACITY];
    pending_release_slot_t               pending_releases[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY];
    persistent_intent_t                  persistent_intents[KEY_RUNTIME_CORE_PERSISTENT_INTENT_CAPACITY];
    key_runtime_core_shadow_projection_t shadow_projection;
    uint16_t                             current_time;
    uint16_t                             next_token_id;
    uint32_t                             next_feedback_sequence;
    uint8_t                              press_token_count;
    uint8_t                              tap_series_count;
    uint8_t                              lease_count;
    uint8_t                              pending_release_count;
    uint8_t                              pending_release_head_index;
    uint8_t                              pending_release_tail_index;
    uint8_t                              pending_release_high_water_mark;
    uint8_t                              pending_release_validation_failure_count;
    uint8_t                              persistent_intent_count;
    uint8_t                              release_keycode_mismatch_count;
    uint8_t                              orphan_release_count;
    uint8_t                              cancelled_press_count;
    uint8_t                              token_allocation_failure_count;
    key_runtime_packed_keypos_t          token_allocation_failed_packed_key_pos;
    uint16_t                             feedback_pulse_timer;
    uint32_t                             feedback_pulse_sequence;
    bool                                 feedback_pulse_active;
    key_feedback_pulse_kind_t            feedback_pulse_kind;
    keypos_t                             feedback_pulse_key_pos;
    uint8_t                              feedback_pulse_tap_branch;
    bool                                 feedback_pulse_queued;
    uint32_t                             feedback_pulse_queued_sequence;
    key_feedback_pulse_kind_t            feedback_pulse_queued_kind;
    keypos_t                             feedback_pulse_queued_key_pos;
    uint8_t                              feedback_pulse_queued_tap_branch;
    uint16_t                             preview_display_bridge_started_at;
    uint8_t                              preview_display_last_semantic_layer;
    uint8_t                              preview_display_bridge_layer;
    bool                                 preview_display_bridge_active;
    uint8_t                              keyboard_event_masked_real_mods;
    bool                                 keyboard_event_mask_active;
} key_runtime_core_state_t;

#ifdef KEY_RUNTIME_HOT_PATH_TEST_INSTRUMENTATION
typedef struct {
    uint16_t refresh_slot_visit_count;
    uint16_t scan_press_slot_visit_count;
    uint16_t scan_tap_series_slot_visit_count;
} key_runtime_hot_path_test_counters_t;

void key_runtime_hot_path_test_counters_reset(void);
void key_runtime_hot_path_test_counters_snapshot(key_runtime_hot_path_test_counters_t *out);
bool key_runtime_hot_path_test_active_indexes_consistent(void);
#endif

static inline uint32_t key_runtime_core_state_next_feedback_sequence(key_runtime_core_state_t *state) {
    uint32_t sequence;

    if (!state) {
        return 0u;
    }

    sequence = state->next_feedback_sequence;
    if (sequence == 0u) {
        sequence = 1u;
    }

    state->next_feedback_sequence = sequence + 1u;
    if (state->next_feedback_sequence == 0u) {
        state->next_feedback_sequence = 1u;
    }

    return sequence;
}

key_runtime_core_state_t *key_runtime_core_state(void);
void                      key_runtime_core_apply_event(const runtime_event_t *event, uint16_t event_time);
__attribute__((noinline)) void key_runtime_core_observe_process_record_event(uint16_t keycode, keyrecord_t *record);
void                      key_runtime_core_observe_scan_cycle(uint16_t now);
void                      key_runtime_core_interrupt_active_keys_on_other_press(keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
void                      key_runtime_core_flush_foreign_multi_tap(uint16_t keycode, keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
void                      key_runtime_core_flush_multi_tap(key_runtime_core_effect_plan_t *plan);
void                      key_runtime_core_flush_active_keys_except(keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
bool                      key_runtime_core_handle_handled_key_press(uint16_t keycode, keypos_t key_pos, key_runtime_core_effect_plan_t *plan);
bool                      key_runtime_core_handle_handled_key_release(uint16_t keycode, keypos_t key_pos, const handled_key_resolution_t *resolution, keyboard_mod_state_t keyboard_mod_state, key_runtime_core_effect_plan_t *plan);
void                      key_runtime_core_scan(key_runtime_core_effect_plan_t *plan, uint16_t now);
bool                      key_runtime_core_settle_pending_fallback_hold(key_runtime_core_effect_plan_t *plan);

static inline void key_runtime_core_state_reset(key_runtime_core_state_t *state) {
    if (!state) {
        return;
    }

    *state                                        = (key_runtime_core_state_t){0};
    state->next_token_id                          = 1u;
    state->next_feedback_sequence                 = 1u;
    state->pending_release_head_index             = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE;
    state->pending_release_tail_index             = KEY_RUNTIME_CORE_PENDING_RELEASE_INDEX_NONE;
    state->token_allocation_failed_packed_key_pos = KEY_RUNTIME_PACKED_KEYPOS_NONE;
    state->preview_display_last_semantic_layer    = UINT8_MAX;
    state->preview_display_bridge_layer           = UINT8_MAX;
}
