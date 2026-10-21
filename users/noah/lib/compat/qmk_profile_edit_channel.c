// ───────────────────────────────────────────────────────────────────────────
// Profile Wire: Live Edits (D-F32)
// ───────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "qmk_profile_edit_channel.h"

#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)

#    include <string.h>

#    include "qmk_keyboard_restart.h"
#    include "qmk_portable_profile.h"
#    include "../key/behavior/participation.h"
#    include "../profile/protocol/profile_candidate_v1.h"
#    include "../profile/protocol/profile_wire_v1.h"
#    include "../profile/runtime/effective_settings_runtime.h"
#    include "../profile/storage/profile_store_runtime.h"

enum {
    FRAME_SIZE   = NOAH_PROFILE_WIRE_V1_REPORT_SIZE,
    PAYLOAD_SIZE = NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE,
    // A reply to SET: bytes 0..4 echoed, then the result and the identity.
    REPLY_RESULT   = 5u,
    REPLY_PHASE    = 6u,
    REPLY_SESSION  = 7u,
    REPLY_REVISION = 11u,
    REPLY_FLAGS    = 15u,
};

static void put_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static uint32_t get_u32(const uint8_t *source) {
    return (uint32_t)source[0] | (uint32_t)source[1] << 8u | (uint32_t)source[2] << 16u | (uint32_t)source[3] << 24u;
}

static bool zero_from(const uint8_t *data, uint8_t from) {
    for (uint8_t index = from; index < FRAME_SIZE; index++) {
        if (data[index] != 0u) {
            return false;
        }
    }
    return true;
}

static uint16_t status_flags(const noah_profile_owner_working_status_t *status) {
    return (uint16_t)((status->local.active ? NOAH_PROFILE_EDIT_WIRE_UNSAVED : 0u) | (status->peer_required ? NOAH_PROFILE_EDIT_WIRE_PEER_REQUIRED : 0u) | (status->halves_agree ? NOAH_PROFILE_EDIT_WIRE_PEER_CONVERGED : 0u) | (status->peer.known ? NOAH_PROFILE_EDIT_WIRE_PEER_KNOWN : 0u) | (status->edits_blocked ? NOAH_PROFILE_EDIT_WIRE_EDITS_BLOCKED : 0u) | (status->saved_compiled ? NOAH_PROFILE_EDIT_WIRE_SAVED_COMPILED : 0u) | (status->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE ? NOAH_PROFILE_EDIT_WIRE_SAVING : 0u) | (status->mode_conflict ? NOAH_PROFILE_EDIT_WIRE_MODE_CONFLICT : 0u) | ((status->local.phase == NOAH_PROFILE_WORKING_PHASE_WAITING_SAFE || status->local.phase == NOAH_PROFILE_WORKING_PHASE_REVERTING) && status->safe_boundary_reason_mask != 0u ? NOAH_PROFILE_EDIT_WIRE_WAITING_SAFE : 0u) | (status->storage_paused ? NOAH_PROFILE_EDIT_WIRE_STORAGE_PAUSED : 0u) | (status->peer_storage_paused ? NOAH_PROFILE_EDIT_WIRE_PEER_STORAGE_PAUSED : 0u) | (status->peer_refused ? NOAH_PROFILE_EDIT_WIRE_PEER_REFUSED : 0u) | (status->mode_store_failed ? NOAH_PROFILE_EDIT_WIRE_MODE_NOT_STORED : 0u) | (status->adjustment_pending ? NOAH_PROFILE_EDIT_WIRE_ADJUSTMENT_PENDING : 0u) | (status->stopped ? NOAH_PROFILE_EDIT_WIRE_STOPPED : 0u) | (status->peer_stopped ? NOAH_PROFILE_EDIT_WIRE_PEER_STOPPED : 0u));
}

// The answer to an edit SET: the result, then where the working copy now is,
// so a stale edit learns the current session and revision at once.
static void reply(uint8_t *data, noah_profile_working_result_t result) {
    noah_profile_owner_working_status_t status;

    memset(&data[REPLY_RESULT], 0, FRAME_SIZE - REPLY_RESULT);
    data[REPLY_RESULT] = (uint8_t)result;
    if (noah_profile_store_runtime_working_status(&status)) {
        data[REPLY_PHASE] = status.local.phase;
        put_u32(&data[REPLY_SESSION], status.local.session);
        put_u32(&data[REPLY_REVISION], status.local.revision);
        put_u16(&data[REPLY_FLAGS], status_flags(&status));
    }
}

// Restart (D-F34): taken in any owner state and edit mode, since it is the
// way out of a stopped owner; it reads nothing of the profile. The other half
// is asked first, then this reply goes out, then this half restarts.
static void handle_restart(uint8_t *data) {
    uint16_t transaction = (uint16_t)(data[3] | (uint16_t)data[4] << 8u);
    bool     acknowledged;

    if (transaction == 0u || get_u32(&data[5]) != NOAH_PROFILE_EDIT_WIRE_RESTART_CONFIRM || !zero_from(data, 9u)) {
        reply(data, NOAH_PROFILE_WORKING_INVALID_ARGUMENT);
        return;
    }
    acknowledged = noah_qmk_keyboard_restart_request();
    reply(data, NOAH_PROFILE_WORKING_OK);
    data[NOAH_PROFILE_EDIT_WIRE_RESTART_PEER] = acknowledged ? 1u : 0u;
}

#    if defined(NOAH_LIVE_PROFILE_MUTATION_ENABLE)
static bool handle_set(uint8_t *data) {
    uint16_t                      transaction = (uint16_t)(data[3] | (uint16_t)data[4] << 8u);
    noah_profile_working_result_t result      = NOAH_PROFILE_WORKING_INVALID_ARGUMENT;

    switch (data[2]) {
        case NOAH_PROFILE_WIRE_V1_VALUE_EDIT_BEGIN:
            if (transaction != 0u && zero_from(data, 19u)) {
                result = noah_profile_store_runtime_edit_begin(transaction, get_u32(&data[5]), get_u32(&data[9]), data[13], get_u32(&data[15]), data[14]);
            }
            break;
        case NOAH_PROFILE_WIRE_V1_VALUE_EDIT_SPLICE:
            if (transaction != 0u && zero_from(data, 20u)) {
                noah_profile_working_splice_t splice = {.domain_id = data[6], .flags = data[7], .offset = get_u32(&data[8]), .remove = get_u32(&data[12]), .insert = get_u32(&data[16])};

                result = noah_profile_store_runtime_edit_splice(transaction, data[5], &splice);
            }
            break;
        case NOAH_PROFILE_WIRE_V1_VALUE_EDIT_DATA:
            if (transaction != 0u && data[9] != 0u && data[9] <= NOAH_PROFILE_EDIT_WIRE_DATA_MAX && zero_from(data, (uint8_t)(10u + data[9]))) {
                result = noah_profile_store_runtime_edit_data(transaction, get_u32(&data[5]), &data[10], data[9]);
            }
            break;
        case NOAH_PROFILE_WIRE_V1_VALUE_EDIT_COMMIT:
            if (transaction != 0u && zero_from(data, 9u)) {
                result = noah_profile_store_runtime_edit_commit(transaction, get_u32(&data[5]));
            }
            break;
        case NOAH_PROFILE_WIRE_V1_VALUE_EDIT_ABORT:
            if (transaction != 0u && zero_from(data, 5u)) {
                result = noah_profile_store_runtime_edit_abort(transaction);
            }
            break;
        case NOAH_PROFILE_WIRE_V1_VALUE_SAVE:
            if (transaction != 0u && zero_from(data, 13u)) {
                result = noah_profile_store_runtime_save(transaction, get_u32(&data[5]), get_u32(&data[9]));
            }
            break;
        case NOAH_PROFILE_WIRE_V1_VALUE_REVERT:
            if (transaction != 0u && zero_from(data, 13u)) {
                result = noah_profile_store_runtime_revert(transaction, get_u32(&data[5]), get_u32(&data[9]));
            }
            break;
        case NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE:
            if (transaction != 0u && zero_from(data, 6u)) {
                result = noah_profile_store_runtime_set_edit_mode(data[5]);
            }
            break;
        default:
            return false;
    }
    reply(data, result);
    return true;
}
#    endif

static void get_status(uint8_t *data, noah_profile_wire_v1_status_code_t code, uint8_t length) {
    data[5] = (uint8_t)code;
    data[6] = code == NOAH_PROFILE_WIRE_V1_STATUS_OK ? length : 0u;
}

static void status_page(const noah_profile_owner_working_status_t *status, uint8_t page, uint8_t *payload) {
    if (page == 0u) {
        payload[0] = NOAH_PROFILE_EDIT_WIRE_STATUS_LAYOUT;
        payload[1] = status->local.phase;
        put_u16(&payload[2], status_flags(status));
        put_u32(&payload[4], status->local.session);
        put_u32(&payload[8], status->local.revision);
        put_u32(&payload[12], status->saved_generation);
        payload[16] = status->saved_origin;
        payload[17] = status->local.last_result;
        put_u16(&payload[18], status->local.last_transaction_id);
        put_u16(&payload[20], status->local.transaction_id);
        payload[22] = status->mode.mode;
        payload[23] = status->local.splices_received;
    } else if (page == 1u) {
        payload[0] = status->peer.phase;
        payload[1] = status->peer.last_result;
        put_u16(&payload[2], status->peer.transaction_id);
        put_u32(&payload[4], status->peer.session);
        put_u32(&payload[8], status->peer.revision);
        payload[12] = status->peer.flags;
        payload[13] = status->save.phase;
        payload[14] = status->save.last_result;
        payload[15] = status->save.last_error;
        put_u16(&payload[16], status->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE ? status->save.transaction_id : 0u);
        put_u16(&payload[18], status->save.last_transaction_id);
        put_u32(&payload[20], status->local.staged_bytes);
        payload[24] = status->peer.mode;
    } else if (page == 2u) {
        const noah_profile_validator_v1_error_t *error = &status->local.error;

        payload[0] = (uint8_t)error->code;
        payload[1] = error->domain_id;
        payload[2] = error->table_id;
        put_u16(&payload[3], error->row_index);
        payload[5] = error->step_index;
        payload[6] = error->field_id;
        put_u32(&payload[7], (uint32_t)error->byte_offset);
        payload[11] = (uint8_t)error->detail_kind;
        put_u16(&payload[12], error->detail_code);
        put_u32(&payload[14], status->safe_boundary_reason_mask);
        put_u32(&payload[18], status->local.length);
    } else {
        payload[0] = 1u;
        payload[1] = (status->save.automatic ? 1u : 0u) | (status->save.last_automatic ? 2u : 0u);
        put_u32(&payload[2], status->save.keyboard_saves);
        put_u32(&payload[6], status->save.keyboard_revision);
        put_u32(&payload[10], status->save.keyboard_generation);
        put_u16(&payload[14], NOAH_PROFILE_OWNER_AUTO_SAVE_QUIET_MS);
    }
}

static void runtime_page(uint8_t *payload) {
    payload[0] = (uint8_t)noah_setting(NOAH_SETTING_COMBOS_ENABLED, 1u);
#    ifdef COMBO_ENABLE
    payload[1] = is_combo_enabled() ? 1u : 0u;
#    else
    payload[1] = 0u;
#    endif
    payload[2] = (uint8_t)noah_setting(NOAH_SETTING_BEHAVIORS_ENABLED, 1u);
    payload[3] = noah_participation_behaviors_override() ? 1u : 0u;
#    ifdef NOAH_PORTABLE_PROFILE_ENABLE
    put_u16(&payload[4], (uint16_t)noah_qmk_portable_live_setting(NOAH_SETTING_DEFAULT_DPI));
    put_u16(&payload[6], (uint16_t)noah_qmk_portable_live_setting(NOAH_SETTING_SNIPING_DPI));
    put_u32(&payload[8], noah_qmk_portable_live_setting(NOAH_SETTING_RGB_MODE));
    put_u32(&payload[12], noah_qmk_portable_live_setting(NOAH_SETTING_RGB_COLOR));
    put_u32(&payload[16], noah_qmk_portable_live_setting(NOAH_SETTING_DEFAULT_LAYERS));
    put_u32(&payload[20], noah_qmk_portable_live_setting(NOAH_SETTING_KEYMAP_OPTIONS));
#    endif
}

static bool handle_get(uint8_t *data) {
    noah_profile_owner_working_status_t status;
    uint16_t                            page  = (uint16_t)(data[4] | (uint16_t)data[5] << 8u);
    bool                                wide  = data[2] == NOAH_PROFILE_WIRE_V1_VALUE_WORKING;
    uint8_t                            *payload = &data[7];

    if (data[2] != NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS && data[2] != NOAH_PROFILE_WIRE_V1_VALUE_WORKING && data[2] != NOAH_PROFILE_WIRE_V1_VALUE_RUNTIME && data[2] != NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE) {
        return false;
    }
    if (data[3] == 0u || !zero_from(data, wide ? 6u : 5u)) {
        memset(&data[5], 0, FRAME_SIZE - 5u);
        get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED, 0u);
        return true;
    }
    if (!wide) {
        page = data[4];
    }
    memset(&data[5], 0, FRAME_SIZE - 5u);
    if (!noah_profile_store_runtime_working_status(&status)) {
        get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE, 0u);
        return true;
    }
    switch (data[2]) {
        case NOAH_PROFILE_WIRE_V1_VALUE_EDIT_STATUS:
            if (page >= NOAH_PROFILE_EDIT_WIRE_STATUS_PAGES) {
                get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE, 0u);
                return true;
            }
            status_page(&status, (uint8_t)page, payload);
            get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_OK, PAYLOAD_SIZE);
            return true;
        case NOAH_PROFILE_WIRE_V1_VALUE_RUNTIME:
            if (page != 0u) {
                get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE, 0u);
                return true;
            }
            runtime_page(payload);
            get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_OK, 24u);
            return true;
        case NOAH_PROFILE_WIRE_V1_VALUE_EDIT_MODE:
            if (page != 0u) {
                get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE, 0u);
                return true;
            }
            payload[0] = status.mode.mode;
            put_u32(&payload[1], status.mode.counter);
            payload[5] = status.peer.mode;
            put_u32(&payload[6], status.peer.mode_counter);
            payload[10] = (uint8_t)((status.peer.known ? 1u : 0u) | (status.mode_conflict ? 2u : 0u) | (status.peer.known && status.peer.mode_counter == status.mode.counter && status.peer.mode == status.mode.mode ? 4u : 0u));
            get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_OK, 11u);
            return true;
        default:
            break;
    }
    // The working copy, paged as the saved profile is: page 0 says what it
    // is; reading page 0 again after the bytes proves they are one revision.
    if (page == 0u) {
        payload[0] = NOAH_PROFILE_EDIT_WIRE_WORKING_LAYOUT;
        payload[1] = PAYLOAD_SIZE;
        put_u32(&payload[2], status.local.length);
        put_u32(&payload[6], status.local.session);
        put_u32(&payload[10], status.local.revision);
        put_u32(&payload[14], status.saved_generation);
        payload[18] = status.saved_origin;
        payload[19] = status.local.domain_mask;
        payload[20] = status.local.active ? 1u : 0u;
        get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_OK, PAYLOAD_SIZE);
        return true;
    }
    {
        uint32_t offset = (uint32_t)(page - 1u) * PAYLOAD_SIZE;
        uint8_t  count;

        if (offset >= status.local.length) {
            get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE, 0u);
            return true;
        }
        count = status.local.length - offset < PAYLOAD_SIZE ? (uint8_t)(status.local.length - offset) : PAYLOAD_SIZE;
        if (!noah_profile_store_runtime_read_working(offset, payload, count)) {
            memset(payload, 0, PAYLOAD_SIZE);
            get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE, 0u);
            return true;
        }
        get_status(data, NOAH_PROFILE_WIRE_V1_STATUS_OK, count);
    }
    return true;
}

bool noah_qmk_profile_edit_channel(uint8_t *data, uint8_t length) {
    if (!data || length != FRAME_SIZE || data[1] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL) {
        return false;
    }
    if (data[0] == NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET && data[2] == NOAH_PROFILE_WIRE_V1_VALUE_RESTART) {
        handle_restart(data);
        return true;
    }
#    if defined(NOAH_LIVE_PROFILE_MUTATION_ENABLE)
    if (data[0] == NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET) {
        return handle_set(data);
    }
#    endif
    return data[0] == NOAH_PROFILE_WIRE_V1_COMMAND_GET && handle_get(data);
}

#endif
