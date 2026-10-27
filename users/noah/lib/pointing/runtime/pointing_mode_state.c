#include "../../profile/runtime/active_profile_pd_runtime.h"
// ────────────────────────────────────────────────────────────────────────────
// Pointing Mode State
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "pointing_mode_runtime_shared_state_internal.h"
#include "../../key/runtime/slot/origin_registry.h"
#include "../../state/diagnostics/runtime_trace.h"
#include "../../state/ownership/layer_ownership.h"
#include "../../compat/qmk_auto_mouse_contract.h"
#include "../policy/pointing_mode_policy.h"
#include "pointing_mode_key_runtime_bridge.h"
#include "pointing_mode_internal.h"

static pointing_mode_runtime_shared_state_t *pointing_mode_shared_state(void) {
    return pointing_mode_runtime_shared_state();
}

#define POINTING_MODE_LOCAL_ACTIVE_MODE (pointing_mode_shared_state()->local_active_mode)
#define POINTING_MODE_LOCAL_LOCKED_MODE (pointing_mode_shared_state()->local_locked_mode)
#define POINTING_MODE_LOCAL_OWNER_KEY_POS_VALID (pointing_mode_shared_state()->local_owner_key_pos_valid)
#define POINTING_MODE_LOCAL_OWNER_KEY_POS (pointing_mode_shared_state()->local_owner_key_pos)
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
#    define POINTING_MODE_LOCAL_OWNER_SIDES (pointing_mode_shared_state()->local_owner_sides)
#endif
#define POINTING_MODE_LOCAL_OWNERS (pointing_mode_shared_state()->local_owners)

#ifdef POINTING_MODE_PUBLISH_TEST_BACKEND
static pointing_mode_publish_seam_fn_t pointing_mode_publish_seam = NULL;

void pointing_mode_test_set_publish_seam(pointing_mode_publish_seam_fn_t seam) {
    pointing_mode_publish_seam = seam;
}

static void pointing_mode_publish_seam_reached(void) {
    if (pointing_mode_publish_seam) {
        pointing_mode_publish_seam();
    }
}
#else
static void pointing_mode_publish_seam_reached(void) {}
#endif

static bool pointing_mode_snapshot_view_changed(pointing_mode_snapshot_view_t before, pointing_mode_snapshot_view_t after) {
    return before.active_mode != after.active_mode || before.locked_mode != after.locked_mode || before.owner_sides != after.owner_sides || before.active_source != after.active_source;
}

static pointing_mode_source_t pointing_mode_local_source(void) {
    return (pointing_mode_source_t)pointing_mode_shared_state()->local_active_source;
}

// A mode its layer started outside the layer-state hook has no hook to strip
// a separate pointer layer for it, so once the command is done the pointer
// layer goes off, and the hook keeps it off while the mode runs.
//
// A layer change made inside a command (the layers a mode's key turns on,
// released or taken) runs the layer-state hook, which re-enters
// pointing_mode_apply_command with a layer-state command. That is safe because
// every such change happens while a key source still runs the mode, so the
// nested command leaves the links waiting; the source changes only after the
// layers it turned on have gone.
// The strip runs once the outer state is final, never from a nested call.
static bool pointing_mode_layer_policy_refresh_due;

static void pointing_mode_strip_pointer_layer(void) {
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    noah_qmk_contract_auto_mouse_layer_off();
#endif
}

static split_side_mask_t pointing_mode_command_owner_sides(pointing_mode_command_t command) {
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    return command.owner_sides;
#else
    (void)command;
    return SPLIT_SIDE_MASK_NONE;
#endif
}

static keypos_t pointing_mode_command_owner_key_pos(pointing_mode_command_t command) {
    return command.owner_key_pos;
}

static const uint8_t *pointing_mode_command_owner_bitmap(pointing_mode_command_t command) {
    return command.owner_bitmap;
}

static keypos_t pointing_mode_invalid_owner_key_pos(void) {
    return (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static uint16_t pointing_mode_trace_pack_keypos(keypos_t key_pos) {
    return (uint16_t)(((uint16_t)key_pos.row << 8) | key_pos.col);
}

#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
static bool pointing_mode_bitmap_equal(const uint8_t *lhs, const uint8_t *rhs) {
    if (!(lhs && rhs)) {
        return lhs == rhs;
    }

    for (uint8_t index = 0; index < KEY_ORIGIN_BITMAP_SIZE; index++) {
        if (lhs[index] != rhs[index]) {
            return false;
        }
    }

    return true;
}
#endif

static bool pointing_mode_owner_bitmap_for_keypos(keypos_t owner_key_pos, uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (!key_origin_keypos_valid(owner_key_pos)) {
        return false;
    }

    return key_origin_registry_get_bitmap(owner_key_pos, out_bitmap);
}

static void pointing_mode_sync_local_owner_state(keypos_t owner_key_pos, split_side_mask_t owner_sides) {
    bool owner_key_pos_valid = key_origin_keypos_valid(owner_key_pos);

    POINTING_MODE_LOCAL_OWNER_KEY_POS_VALID = owner_key_pos_valid;
    POINTING_MODE_LOCAL_OWNER_KEY_POS       = owner_key_pos_valid ? owner_key_pos : pointing_mode_invalid_owner_key_pos();

#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    POINTING_MODE_LOCAL_OWNER_SIDES = owner_sides;
#else
    (void)owner_sides;
#endif
}

static uint8_t pointing_mode_owner_entry_key(keypos_t key_pos) {
    return (uint8_t)(key_runtime_keypos_pack(key_pos) + 1u);
}

static keypos_t pointing_mode_owner_entry_key_pos(const pointing_mode_owner_entry_t *entry) {
    return key_runtime_keypos_unpack((key_runtime_packed_keypos_t)(entry->key - 1u));
}

static pointing_mode_owner_entry_t *pointing_mode_find_local_owner(keypos_t owner_key_pos) {
    uint8_t key;

    if (!key_origin_keypos_valid(owner_key_pos)) {
        return NULL;
    }

    key = pointing_mode_owner_entry_key(owner_key_pos);
    for (uint16_t index = 0; index < POINTING_MODE_OWNER_CAPACITY; index++) {
        if (POINTING_MODE_LOCAL_OWNERS[index].key == key) {
            return &POINTING_MODE_LOCAL_OWNERS[index];
        }
    }

    return NULL;
}

static pointing_mode_owner_entry_t *pointing_mode_find_free_local_owner(void) {
    for (uint16_t index = 0; index < POINTING_MODE_OWNER_CAPACITY; index++) {
        if (POINTING_MODE_LOCAL_OWNERS[index].key == 0u) {
            return &POINTING_MODE_LOCAL_OWNERS[index];
        }
    }

    return NULL;
}

// Every entry belongs to the mode that was active when the command began, so
// the caller names it for the trace.
static bool pointing_mode_clear_local_owners(pointing_mode_mask_t owner_mode) {
    bool changed = false;

    for (uint16_t index = 0; index < POINTING_MODE_OWNER_CAPACITY; index++) {
        pointing_mode_owner_entry_t *entry = &POINTING_MODE_LOCAL_OWNERS[index];

        if (entry->key != 0u) {
            noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_OWNER_CLEAR, owner_mode, pointing_mode_trace_pack_keypos(pointing_mode_owner_entry_key_pos(entry)));
            *entry  = (pointing_mode_owner_entry_t){0};
            changed = true;
        }
    }

    return changed;
}

static bool pointing_mode_local_owner_add(pointing_mode_mask_t mode, keypos_t owner_key_pos, split_side_mask_t owner_sides) {
    pointing_mode_owner_entry_t *entry;

    if (!(mode != 0 && key_origin_keypos_valid(owner_key_pos))) {
        return false;
    }

    entry = pointing_mode_find_local_owner(owner_key_pos);
    if (entry && entry->owner_sides == owner_sides) {
        return false;
    }
    if (!entry) {
        entry = pointing_mode_find_free_local_owner();
    }
    if (!entry) {
        return false;
    }

    *entry = (pointing_mode_owner_entry_t){
        .key         = pointing_mode_owner_entry_key(owner_key_pos),
        .owner_sides = owner_sides,
    };
    noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_OWNER_PRESS, mode, pointing_mode_trace_pack_keypos(owner_key_pos));
    return true;
}

static bool pointing_mode_local_owner_remove(pointing_mode_mask_t mode, keypos_t owner_key_pos) {
    pointing_mode_owner_entry_t *entry = pointing_mode_find_local_owner(owner_key_pos);

    if (!entry) {
        return false;
    }

    noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_OWNER_RELEASE, mode, pointing_mode_trace_pack_keypos(owner_key_pos));
    *entry = (pointing_mode_owner_entry_t){0};
    return true;
}

static const pointing_mode_owner_entry_t *pointing_mode_first_local_owner(void) {
    for (uint16_t index = 0; index < POINTING_MODE_OWNER_CAPACITY; index++) {
        if (POINTING_MODE_LOCAL_OWNERS[index].key != 0u) {
            return &POINTING_MODE_LOCAL_OWNERS[index];
        }
    }

    return NULL;
}

static split_side_mask_t pointing_mode_local_owners_sides(void) {
    split_side_mask_t owner_sides = SPLIT_SIDE_MASK_NONE;

    for (uint16_t index = 0; index < POINTING_MODE_OWNER_CAPACITY; index++) {
        if (POINTING_MODE_LOCAL_OWNERS[index].key != 0u) {
            owner_sides |= POINTING_MODE_LOCAL_OWNERS[index].owner_sides;
        }
    }

    return owner_sides;
}

static bool pointing_mode_local_owners_bitmap(uint8_t *out_bitmap) {
    bool has_owner = false;

    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    for (uint16_t index = 0; index < POINTING_MODE_OWNER_CAPACITY; index++) {
        const pointing_mode_owner_entry_t *entry = &POINTING_MODE_LOCAL_OWNERS[index];
        uint8_t                            entry_bitmap[KEY_ORIGIN_BITMAP_SIZE];

        if (entry->key == 0u) {
            continue;
        }

        if (pointing_mode_owner_bitmap_for_keypos(pointing_mode_owner_entry_key_pos(entry), entry_bitmap)) {
            key_origin_bitmap_or_inplace(out_bitmap, entry_bitmap);
            has_owner = true;
        }
    }

    return has_owner;
}

static void pointing_mode_sync_local_owners_state(void) {
    const pointing_mode_owner_entry_t *first = pointing_mode_first_local_owner();

    if (!first) {
        pointing_mode_sync_local_owner_state(pointing_mode_invalid_owner_key_pos(), SPLIT_SIDE_MASK_NONE);
        return;
    }

    pointing_mode_sync_local_owner_state(pointing_mode_owner_entry_key_pos(first), pointing_mode_local_owners_sides());
}

static bool pointing_mode_apply_unlock_other_locks(pointing_mode_mask_t keep_mode) {
    pointing_mode_mask_t locked_mode = POINTING_MODE_LOCAL_LOCKED_MODE;

    if (locked_mode != 0 && locked_mode != keep_mode) {
        pointing_mode_transition_unlock(locked_mode);
        return true;
    }

    return false;
}

static bool pointing_mode_apply_deactivate_other_unlocked(pointing_mode_mask_t keep_mode) {
    pointing_mode_mask_t active_mode = POINTING_MODE_LOCAL_ACTIVE_MODE;

    if (active_mode != 0 && active_mode != keep_mode && active_mode != POINTING_MODE_LOCAL_LOCKED_MODE) {
        pointing_mode_transition_deactivate(active_mode);
        return true;
    }

    return false;
}

static bool pointing_mode_apply_activate_mode(pointing_mode_mask_t mode) {
    if (!noah_active_profile_pd_for_mask(mode)) return false;
    bool changed = false;

    if (!mode) {
        return false;
    }

    changed |= pointing_mode_apply_unlock_other_locks(mode);
    changed |= pointing_mode_apply_deactivate_other_unlocked(mode);

    if (!pointing_mode_local_active(mode)) {
        pointing_mode_transition_activate(mode);
        changed = true;
    }

    return changed;
}

static bool pointing_mode_apply_deactivate_mode(pointing_mode_mask_t mode) {
    if (!mode || !pointing_mode_local_active(mode)) {
        return false;
    }

    pointing_mode_transition_deactivate(mode);
    return true;
}

static bool pointing_mode_apply_lock_mode(pointing_mode_mask_t mode) {
    if (!noah_active_profile_pd_for_mask(mode)) return false;
    bool changed = false;

    if (!mode) {
        return false;
    }

    changed |= pointing_mode_apply_unlock_other_locks(mode);
    changed |= pointing_mode_apply_deactivate_other_unlocked(mode);

    if (!pointing_mode_local_locked(mode) || !pointing_mode_local_active(mode)) {
        pointing_mode_transition_lock(mode);
        changed = true;
    }

    return changed;
}

// The layers the running mode's key turns on: only a mode its key runs owns
// any, so a key that turns on its own linked layers cannot keep itself on
// (D-F62).
static void pointing_mode_sync_key_turns_on(void) {
    pointing_mode_mask_t active = POINTING_MODE_LOCAL_ACTIVE_MODE;
    uint16_t             layers = active != 0 && pointing_mode_local_source() == POINTING_MODE_SOURCE_KEY ? pointing_mode_key_turns_on(active) : 0u;

    (void)layer_ownership_set_pointing_mode_layers(layers);
}

static void pointing_mode_start_layer_mode(pointing_mode_mask_t mode, bool in_layer_hook) {
    pointing_mode_transition_activate_from(mode, POINTING_MODE_SOURCE_LAYER);
    noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_LAYER_START, mode, 0u);
    pointing_mode_layer_policy_refresh_due |= !in_layer_hook;
}

// While a key runs a mode, layer links wait. Otherwise the highest linked
// layer that is on decides, and a mode its layers no longer start ends.
static bool pointing_mode_apply_layer_links(layer_state_t state, bool in_layer_hook) {
    pointing_mode_mask_t active = POINTING_MODE_LOCAL_ACTIVE_MODE;
    pointing_mode_mask_t wanted;

    if (active != 0 && pointing_mode_local_source() != POINTING_MODE_SOURCE_LAYER) {
        return false;
    }
    wanted = pointing_mode_layer_started_mode(state);
    if (wanted == active) {
        return false;
    }
    if (active != 0) {
        pointing_mode_transition_deactivate(active);
    }
    if (wanted != 0) {
        pointing_mode_start_layer_mode(wanted, in_layer_hook);
    }
    return true;
}

// The key that ran `mode` let it go: released, unlocked or deactivated. The
// layers it turned on go first, while the key still runs the mode, so the
// layer change waits like any other; then the layer links decide what runs.
// The mode keeps running, without a restart, when its own layers start it.
static void pointing_mode_end_key_source(pointing_mode_mask_t mode) {
    pointing_mode_mask_t layer_mode;

    (void)layer_ownership_set_pointing_mode_layers(0u);
    layer_mode = pointing_mode_layer_started_mode(layer_state);
    if (layer_mode == mode) {
        pointing_mode_transition_hand_to_layer(mode);
        pointing_mode_layer_policy_refresh_due = true;
        return;
    }
    if (pointing_mode_local_locked(mode)) {
        pointing_mode_transition_unlock(mode);
    } else {
        pointing_mode_transition_deactivate(mode);
    }
    if (layer_mode != 0) {
        pointing_mode_start_layer_mode(layer_mode, false);
    }
}

static bool pointing_mode_apply_remote_display_snapshot(pointing_mode_mask_t active_mode, pointing_mode_mask_t locked_mode, split_side_mask_t owner_sides, const uint8_t *owner_bitmap) {
    // Remote sync only mirrors mode state for the slave half's policy/UI.
    // Do not replay local side effects such as dragscroll or auto-mouse
    // ownership changes from this path. Keep only one effective mode so the
    // mirrored UI matches the local exclusivity invariant.
    pointing_mode_runtime_shared_state_t       *state     = pointing_mode_shared_state();
    const pointing_mode_remote_display_state_t *published = pointing_mode_remote_display_published(state);
    pointing_mode_remote_display_state_t       *pending   = pointing_mode_remote_display_pending(state);
    bool                                        changed   = published->active_mode != active_mode || published->locked_mode != locked_mode;

#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    changed |= published->owner_sides != owner_sides;
    if (owner_bitmap) {
        changed |= !pointing_mode_bitmap_equal(published->owner_bitmap, owner_bitmap);
    } else {
        changed |= key_origin_bitmap_has_any(published->owner_bitmap);
    }
#else
    (void)owner_sides;
    (void)owner_bitmap;
#endif

    pending->locked_mode = locked_mode;
    pending->active_mode = active_mode;
    pointing_mode_publish_seam_reached();
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    pending->owner_sides = owner_sides;
    if (owner_bitmap) {
        key_origin_bitmap_copy(pending->owner_bitmap, owner_bitmap);
    } else {
        key_origin_bitmap_clear(pending->owner_bitmap);
    }
#endif
    noah_runtime_publication_publish(&state->remote_display_generation);

    noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_REMOTE_SNAPSHOT, active_mode, locked_mode);
    return changed;
}

static pointing_mode_apply_result_t pointing_mode_apply_result_begin(void) {
    return (pointing_mode_apply_result_t){
        .before = pointing_mode_snapshot(),
    };
}

static void pointing_mode_apply_result_finish(pointing_mode_apply_result_t *result) {
    if (!result) {
        return;
    }

    result->after                 = pointing_mode_snapshot();
    result->local_state_changed   = pointing_mode_snapshot_view_changed(result->before.local, result->after.local);
    result->display_state_changed = pointing_mode_snapshot_view_changed(result->before.display, result->after.display);
}

void pointing_mode_set(pointing_mode_mask_t mode) {
    POINTING_MODE_LOCAL_ACTIVE_MODE = mode;
}

void pointing_mode_clear(pointing_mode_mask_t mode) {
    if (POINTING_MODE_LOCAL_ACTIVE_MODE == mode) {
        POINTING_MODE_LOCAL_ACTIVE_MODE = 0;
    }
}

void pointing_mode_set_locked(pointing_mode_mask_t mode) {
    POINTING_MODE_LOCAL_LOCKED_MODE = mode;
}

void pointing_mode_clear_locked(pointing_mode_mask_t mode) {
    if (POINTING_MODE_LOCAL_LOCKED_MODE == mode) {
        POINTING_MODE_LOCAL_LOCKED_MODE = 0;
    }
}

pointing_mode_mask_t pointing_mode_local_active_snapshot(void) {
    return POINTING_MODE_LOCAL_ACTIVE_MODE;
}

pointing_mode_mask_t pointing_mode_local_locked_snapshot(void) {
    return POINTING_MODE_LOCAL_LOCKED_MODE;
}

pointing_mode_mask_t pointing_mode_display_active_snapshot(void) {
    return pointing_mode_snapshot().display.active_mode;
}

pointing_mode_mask_t pointing_mode_display_locked_snapshot(void) {
    return pointing_mode_snapshot().display.locked_mode;
}

split_side_mask_t pointing_mode_local_owner_sides_snapshot(void) {
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    return POINTING_MODE_LOCAL_OWNER_SIDES;
#else
    return SPLIT_SIDE_MASK_NONE;
#endif
}

split_side_mask_t pointing_mode_display_owner_sides_snapshot(void) {
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    if (is_keyboard_master()) {
        return POINTING_MODE_LOCAL_OWNER_SIDES;
    }

    return pointing_mode_remote_display_snapshot(pointing_mode_shared_state()).owner_sides;
#else
    return SPLIT_SIDE_MASK_NONE;
#endif
}

bool pointing_mode_local_owner_key_pos_snapshot(keypos_t *out) {
    if (out) {
        *out = (keypos_t){0};
    }

    if (!(out && POINTING_MODE_LOCAL_OWNER_KEY_POS_VALID)) {
        return false;
    }

    *out = POINTING_MODE_LOCAL_OWNER_KEY_POS;
    return true;
}

bool pointing_mode_local_owner_bitmap_snapshot(uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (POINTING_MODE_LOCAL_ACTIVE_MODE != 0 && pointing_mode_local_owners_bitmap(out_bitmap)) {
        return true;
    }

    if (!POINTING_MODE_LOCAL_OWNER_KEY_POS_VALID) {
        return false;
    }

    return pointing_mode_owner_bitmap_for_keypos(POINTING_MODE_LOCAL_OWNER_KEY_POS, out_bitmap);
}

bool pointing_mode_display_owner_bitmap_snapshot(uint8_t *out_bitmap) {
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    pointing_mode_remote_display_state_t display;
#endif

    if (!out_bitmap) {
        return false;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (is_keyboard_master()) {
        return pointing_mode_local_owner_bitmap_snapshot(out_bitmap);
    }

#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
    display = pointing_mode_remote_display_snapshot(pointing_mode_shared_state());
    key_origin_bitmap_copy(out_bitmap, display.owner_bitmap);
    return key_origin_bitmap_has_any(out_bitmap);
#else
    return false;
#endif
}

bool pointing_mode_local_active(pointing_mode_mask_t mode) {
    return mode != 0 && POINTING_MODE_LOCAL_ACTIVE_MODE == mode;
}

bool pointing_mode_local_locked(pointing_mode_mask_t mode) {
    return mode != 0 && POINTING_MODE_LOCAL_LOCKED_MODE == mode;
}

bool pointing_mode_display_active(pointing_mode_mask_t mode) {
    return mode != 0 && pointing_mode_snapshot().display.active_mode == mode;
}

bool pointing_mode_display_locked(pointing_mode_mask_t mode) {
    return mode != 0 && pointing_mode_snapshot().display.locked_mode == mode;
}

bool pd_any_local_mode_active(void) {
    return POINTING_MODE_LOCAL_ACTIVE_MODE != 0;
}

bool pd_any_local_mode_locked(void) {
    return POINTING_MODE_LOCAL_LOCKED_MODE != 0;
}

bool pointing_mode_local_key_source_active(void) {
    return POINTING_MODE_LOCAL_ACTIVE_MODE != 0 && pointing_mode_local_source() == POINTING_MODE_SOURCE_KEY;
}

pointing_mode_source_t pointing_mode_local_source_snapshot(void) {
    return POINTING_MODE_LOCAL_ACTIVE_MODE != 0 ? pointing_mode_local_source() : POINTING_MODE_SOURCE_NONE;
}

bool pd_any_display_mode_locked(void) {
    return pointing_mode_snapshot().display.locked_mode != 0;
}

pointing_mode_apply_result_t pointing_mode_apply_command(pointing_mode_command_t command) {
    pointing_mode_apply_result_t result       = pointing_mode_apply_result_begin();
    pointing_mode_mask_t         mode         = command.mode;
    bool                         keyed        = false;
    bool                         layer_before = result.before.local.active_source == POINTING_MODE_SOURCE_LAYER;

    switch (command.kind) {
        case POINTING_MODE_COMMAND_ACTIVATE:
            result.handled = mode != 0;
            if (pointing_mode_apply_activate_mode(mode)) {
                (void)pointing_mode_clear_local_owners(result.before.local.active_mode);
                pointing_mode_sync_local_owner_state(pointing_mode_command_owner_key_pos(command), pointing_mode_command_owner_sides(command));
            }
            keyed = true;
            break;
        case POINTING_MODE_COMMAND_DEACTIVATE:
            result.handled = mode != 0;
            if (pointing_mode_local_active(mode)) {
                (void)pointing_mode_clear_local_owners(result.before.local.active_mode);
                pointing_mode_sync_local_owner_state(pointing_mode_invalid_owner_key_pos(), SPLIT_SIDE_MASK_NONE);
                if (pointing_mode_local_source() == POINTING_MODE_SOURCE_KEY) {
                    pointing_mode_end_key_source(mode);
                } else {
                    (void)pointing_mode_apply_deactivate_mode(mode);
                }
            }
            break;
        case POINTING_MODE_COMMAND_LOCK:
            result.handled = mode != 0;
            if (pointing_mode_apply_lock_mode(mode)) {
                (void)pointing_mode_clear_local_owners(result.before.local.active_mode);
                pointing_mode_sync_local_owner_state(pointing_mode_command_owner_key_pos(command), pointing_mode_command_owner_sides(command));
            }
            keyed = true;
            break;
        case POINTING_MODE_COMMAND_UNLOCK:
            result.handled = mode != 0;
            if (pointing_mode_local_locked(mode)) {
                (void)pointing_mode_clear_local_owners(result.before.local.active_mode);
                pointing_mode_sync_local_owner_state(pointing_mode_invalid_owner_key_pos(), SPLIT_SIDE_MASK_NONE);
                pointing_mode_end_key_source(mode);
            }
            break;
        case POINTING_MODE_COMMAND_KEY_PRESS:
            mode           = pointing_mode_for_keycode(command.keycode);
            result.handled = mode != 0;
            if (mode != 0) {
                bool locked_same_mode = pointing_mode_local_locked(mode);
                bool mode_changed;

                noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_KEY_PRESS, mode, pointing_mode_trace_pack_keypos(pointing_mode_command_owner_key_pos(command)));
                mode_changed = pointing_mode_apply_activate_mode(mode);
                // A slot with no record never activates, so its key owns
                // nothing: an owner is recorded only for the active mode.
                if (!locked_same_mode && pointing_mode_local_active(mode)) {
                    bool owner_changed;

                    if (mode_changed) {
                        (void)pointing_mode_clear_local_owners(result.before.local.active_mode);
                    }
                    owner_changed = pointing_mode_local_owner_add(mode, pointing_mode_command_owner_key_pos(command), pointing_mode_command_owner_sides(command));
                    if (mode_changed || owner_changed) {
                        pointing_mode_sync_local_owners_state();
                    }
                }
                keyed = true;
            }
            break;
        case POINTING_MODE_COMMAND_KEY_RELEASE:
            mode           = pointing_mode_for_keycode(command.keycode);
            result.handled = mode != 0;
            if (mode != 0) {
                noah_runtime_trace_emit(NOAH_TRACE_POINTING_MODE, NOAH_TRACE_POINTING_MODE_EVENT_KEY_RELEASE, mode, pointing_mode_trace_pack_keypos(pointing_mode_command_owner_key_pos(command)));
            }
            if (mode != 0 && pointing_mode_local_active(mode) && !pointing_mode_local_locked(mode) && pointing_mode_local_source() == POINTING_MODE_SOURCE_KEY) {
                (void)pointing_mode_local_owner_remove(mode, pointing_mode_command_owner_key_pos(command));

                if (pointing_mode_first_local_owner()) {
                    pointing_mode_sync_local_owners_state();
                } else {
                    pointing_mode_end_key_source(mode);
                    pointing_mode_sync_local_owner_state(pointing_mode_invalid_owner_key_pos(), SPLIT_SIDE_MASK_NONE);
                }
            }
            break;
        case POINTING_MODE_COMMAND_REMOTE_SNAPSHOT:
            result.handled = true;
            (void)pointing_mode_apply_remote_display_snapshot(pointing_mode_mask_from_id(command.active_mode_id), pointing_mode_mask_from_id(command.locked_mode_id), command.owner_sides, pointing_mode_command_owner_bitmap(command));
            break;
        case POINTING_MODE_COMMAND_LAYER_STATE:
            result.handled = true;
            (void)pointing_mode_apply_layer_links(command.layer_state, true);
            break;
        case POINTING_MODE_COMMAND_NONE:
        default:
            break;
    }

    // A mode its key starts or locks runs from that key, even one its layer
    // ran a moment ago. The layers its key turns on come on once it runs; the
    // layer-state hook then sees a key source and leaves the links waiting.
    if (keyed) {
        if (mode != 0 && pointing_mode_local_active(mode)) {
            if (layer_before && result.before.local.active_mode == mode) {
                pointing_mode_transition_take_from_layer(mode);
            } else {
                pointing_mode_shared_state()->local_active_source = POINTING_MODE_SOURCE_KEY;
            }
        }
        pointing_mode_sync_key_turns_on();
    }
    pointing_mode_apply_result_finish(&result);
    if (pointing_mode_layer_policy_refresh_due) {
        pointing_mode_layer_policy_refresh_due = false;
        pointing_mode_strip_pointer_layer();
    }
    return result;
}

void pointing_mode_apply_layer_state(layer_state_t state) {
    (void)pointing_mode_apply_command((pointing_mode_command_t){
        .kind          = POINTING_MODE_COMMAND_LAYER_STATE,
        .owner_key_pos = pointing_mode_invalid_owner_key_pos(),
        .layer_state   = state,
    });
}

void pointing_mode_end_layer_started_mode(void) {
    pointing_mode_mask_t active = POINTING_MODE_LOCAL_ACTIVE_MODE;

    if (active != 0 && pointing_mode_local_source() == POINTING_MODE_SOURCE_LAYER) {
        (void)pointing_mode_apply_command((pointing_mode_command_t){
            .kind          = POINTING_MODE_COMMAND_DEACTIVATE,
            .mode          = active,
            .owner_key_pos = pointing_mode_invalid_owner_key_pos(),
        });
    }
}

void pointing_mode_reevaluate_layer_links(void) {
    // The slave half runs no mode of its own; it mirrors the master's.
    if (!is_keyboard_master()) {
        return;
    }
    if (pointing_mode_apply_layer_links(layer_state, false) && pointing_mode_local_source() == POINTING_MODE_SOURCE_LAYER) {
        pointing_mode_strip_pointer_layer();
    }
    pointing_mode_layer_policy_refresh_due = false;
    pointing_mode_sync_key_turns_on();
}

void pointing_mode_activate(pointing_mode_mask_t mode) {
    (void)pointing_mode_apply_command((pointing_mode_command_t){
        .kind          = POINTING_MODE_COMMAND_ACTIVATE,
        .mode          = mode,
        .owner_key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS},
    });
}

void pointing_mode_deactivate(pointing_mode_mask_t mode) {
    (void)pointing_mode_apply_command((pointing_mode_command_t){
        .kind          = POINTING_MODE_COMMAND_DEACTIVATE,
        .mode          = mode,
        .owner_key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS},
    });
}

void pointing_mode_apply_remote_mode_ids(pointing_mode_id_t active_mode_id, pointing_mode_id_t locked_mode_id, split_side_mask_t owner_sides) {
    pointing_mode_apply_remote_mode_ids_with_owner_bitmap(active_mode_id, locked_mode_id, owner_sides, NULL);
}

void pointing_mode_apply_remote_mode_ids_with_owner_bitmap(pointing_mode_id_t active_mode_id, pointing_mode_id_t locked_mode_id, split_side_mask_t owner_sides, const uint8_t *owner_bitmap) {
    (void)pointing_mode_apply_command((pointing_mode_command_t){
        .kind           = POINTING_MODE_COMMAND_REMOTE_SNAPSHOT,
        .active_mode_id = active_mode_id,
        .locked_mode_id = locked_mode_id,
        .owner_sides    = owner_sides,
        .owner_key_pos  = {.row = MATRIX_ROWS, .col = MATRIX_COLS},
        .owner_bitmap   = owner_bitmap,
    });
}

void pointing_mode_apply_remote_snapshot(pointing_mode_mask_t active_flags, pointing_mode_mask_t locked_flags) {
    pointing_mode_mask_t locked_mode = locked_flags != 0 ? locked_flags : 0;
    pointing_mode_mask_t active_mode = locked_mode != 0 ? locked_mode : active_flags;

    pointing_mode_apply_remote_mode_ids(pointing_mode_id_from_mask(active_mode), pointing_mode_id_from_mask(locked_mode), SPLIT_SIDE_MASK_NONE);
}

bool pointing_mode_set_lock_state(pointing_mode_mask_t mode, bool locked) {
    return pointing_mode_set_lock_state_at(mode, locked, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS});
}

static bool pointing_mode_apply_local_lock_state_at(pointing_mode_mask_t mode, bool locked, keypos_t key_pos) {
    pointing_mode_apply_result_t result = pointing_mode_apply_command((pointing_mode_command_t){
        .kind          = locked ? POINTING_MODE_COMMAND_LOCK : POINTING_MODE_COMMAND_UNLOCK,
        .mode          = mode,
        .owner_sides   = key_origin_registry_side_mask(key_pos),
        .owner_key_pos = key_pos,
    });

    if (result.local_state_changed) {
        pointing_mode_key_runtime_bridge_observe_local_lock_state(mode, locked);
    }

    return result.local_state_changed;
}

bool pointing_mode_set_lock_state_at(pointing_mode_mask_t mode, bool locked, keypos_t key_pos) {
    return pointing_mode_apply_local_lock_state_at(mode, locked, key_pos);
}

void pointing_mode_lock(pointing_mode_mask_t mode) {
    (void)pointing_mode_apply_local_lock_state_at(mode, true, pointing_mode_invalid_owner_key_pos());
}

void pointing_mode_unlock(pointing_mode_mask_t mode) {
    (void)pointing_mode_apply_local_lock_state_at(mode, false, pointing_mode_invalid_owner_key_pos());
}

bool pointing_mode_toggle_lock_state(pointing_mode_mask_t mode) {
    return pointing_mode_toggle_lock_state_at(mode, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS});
}

bool pointing_mode_toggle_lock_state_at(pointing_mode_mask_t mode, keypos_t key_pos) {
    return pointing_mode_set_lock_state_at(mode, !pointing_mode_local_locked(mode), key_pos);
}

bool pointing_mode_handle_keycode_press(uint16_t keycode) {
    return pointing_mode_handle_keycode_press_at(keycode, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS});
}

bool pointing_mode_handle_keycode_press_at(uint16_t keycode, keypos_t key_pos) {
    pointing_mode_apply_result_t result = pointing_mode_apply_command((pointing_mode_command_t){
        .kind          = POINTING_MODE_COMMAND_KEY_PRESS,
        .keycode       = keycode,
        .owner_sides   = key_origin_registry_side_mask(key_pos),
        .owner_key_pos = key_pos,
    });

    return result.handled;
}

bool pointing_mode_handle_keycode_release(uint16_t keycode) {
    return pointing_mode_handle_keycode_release_at(keycode, (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS});
}

bool pointing_mode_handle_keycode_release_at(uint16_t keycode, keypos_t key_pos) {
    pointing_mode_apply_result_t result = pointing_mode_apply_command((pointing_mode_command_t){
        .kind          = POINTING_MODE_COMMAND_KEY_RELEASE,
        .keycode       = keycode,
        .owner_sides   = key_origin_registry_side_mask(key_pos),
        .owner_key_pos = key_pos,
    });

    return result.handled;
}

#undef POINTING_MODE_LOCAL_ACTIVE_MODE
#undef POINTING_MODE_LOCAL_LOCKED_MODE
#undef POINTING_MODE_LOCAL_OWNER_KEY_POS_VALID
#undef POINTING_MODE_LOCAL_OWNER_KEY_POS
#undef POINTING_MODE_LOCAL_OWNERS
#ifdef RGB_POINTING_MODE_ACTIVE_HALF_ENABLE
#    undef POINTING_MODE_LOCAL_OWNER_SIDES
#endif
