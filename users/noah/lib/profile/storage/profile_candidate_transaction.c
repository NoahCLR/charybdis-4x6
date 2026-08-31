// ───────────────────────────────────────────────────────────────────────────
// Scan-Owned Live Profile Candidate Transaction
// ───────────────────────────────────────────────────────────────────────────

#include "profile_candidate_transaction.h"

#include <string.h>

static bool metadata_equal(const noah_profile_candidate_v1_metadata_t *left, const noah_profile_candidate_v1_metadata_t *right) {
    return left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->requested_domains == right->requested_domains && left->flags == right->flags && left->payload_length == right->payload_length && left->crc32 == right->crc32 && left->digest == right->digest && left->action_abi_digest == right->action_abi_digest;
}

static void clear_error(noah_profile_candidate_transaction_t *transaction) {
    transaction->status.error = noah_profile_candidate_v1_no_error();
}

static void set_simple_error(noah_profile_candidate_transaction_t *transaction, noah_profile_candidate_v1_error_id_t code, uint16_t byte_offset) {
    transaction->status.error             = noah_profile_candidate_v1_no_error();
    transaction->status.error.code        = code;
    transaction->status.error.byte_offset = byte_offset;
}

static void poison(noah_profile_candidate_transaction_t *transaction, noah_profile_candidate_v1_error_id_t code, uint16_t byte_offset) {
    transaction->poisoned    = true;
    transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED;
    set_simple_error(transaction, code, byte_offset);
}

static void set_backend_rejection(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_v1_error_t *error) {
    transaction->poisoned     = true;
    transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED;
    if (error) {
        transaction->status.error = *error;
    } else {
        transaction->status.error = noah_profile_candidate_v1_no_error();
    }
    if (transaction->status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE) {
        transaction->status.error.code = NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED;
    }
}

static bool backend_complete(noah_profile_candidate_backend_result_t result) {
    return result == NOAH_PROFILE_CANDIDATE_BACKEND_OK;
}

void noah_profile_candidate_transaction_init(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_backend_t *backend, const noah_profile_candidate_compatibility_t *compatibility) {
    if (!transaction) {
        return;
    }

    memset(transaction, 0, sizeof(*transaction));
    if (backend) {
        transaction->backend = *backend;
    }
    if (compatibility) {
        transaction->compatibility = *compatibility;
    }
    transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
    clear_error(transaction);
}

bool noah_profile_candidate_transaction_receive(noah_profile_candidate_transaction_t *transaction, uint8_t *frame, size_t length) {
    noah_profile_candidate_v1_command_t       command;
    noah_profile_candidate_v1_frame_error_t   error;
    noah_profile_candidate_v1_decode_result_t result;

    if (!transaction || !frame) {
        return false;
    }

    result = noah_profile_candidate_v1_decode(frame, length, &command, &error);
    if (result == NOAH_PROFILE_CANDIDATE_V1_DECODE_NOT_HANDLED || result == NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_ARGUMENT || result == NOAH_PROFILE_CANDIDATE_V1_DECODE_INVALID_LENGTH) {
        return false;
    }
    if (result != NOAH_PROFILE_CANDIDATE_V1_DECODE_OK) {
        noah_profile_candidate_v1_encode_ack(frame, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_MALFORMED, error.code, error.frame_offset);
        return true;
    }
    if (transaction->mailbox.pending) {
        noah_profile_candidate_v1_encode_ack(frame, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY, NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8);
        return true;
    }

    transaction->mailbox.command = command;
    transaction->mailbox.pending = true;
    noah_profile_candidate_v1_encode_ack(frame, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_QUEUED, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8);
    return true;
}

static bool process_begin(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_v1_command_t *command) {
    const noah_profile_candidate_v1_metadata_t *metadata = &command->payload.begin;
    noah_profile_candidate_backend_result_t      result;

    if (transaction->has_candidate) {
        if (command->transaction_id != transaction->status.transaction_id) {
            set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
            return false;
        }
        if (transaction->poisoned) {
            set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_POISONED, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
            return false;
        }
        if (metadata_equal(&transaction->metadata, metadata)) {
            clear_error(transaction);
        } else {
            poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_CONFLICTING_RETRY, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        }
        return false;
    }
    if (transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return false;
    }

    transaction->status.transaction_id = command->transaction_id;
    transaction->status.payload_length = metadata->payload_length;
    transaction->status.digest         = metadata->digest;

    if (metadata->schema_major != transaction->compatibility.schema_major || metadata->schema_minor != transaction->compatibility.schema_minor) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INCOMPATIBLE_SCHEMA, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return false;
    }
    if ((metadata->requested_domains & (uint8_t)~transaction->compatibility.supported_domain_mask) != 0u) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_UNSUPPORTED_DOMAIN, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return false;
    }
    if (metadata->action_abi_digest != transaction->compatibility.action_abi_digest) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INCOMPATIBLE_ACTION_ABI, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return false;
    }
    if (metadata->payload_length > transaction->compatibility.max_payload_length) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_CAPACITY_EXCEEDED, 0u);
        return false;
    }
    if (!transaction->backend.begin) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return false;
    }

    transaction->metadata      = *metadata;
    transaction->has_candidate = true;
    transaction->poisoned      = false;
    transaction->status.next_offset = 0u;
    result = transaction->backend.begin(transaction->backend.context, metadata);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_BUSY) {
        transaction->has_candidate = false;
        transaction->poisoned      = false;
        memset(&transaction->metadata, 0, sizeof(transaction->metadata));
        transaction->status.state       = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
        transaction->status.next_offset = 0u;
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return true;
    }
    if (!backend_complete(result)) {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return false;
    }

    transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING;
    clear_error(transaction);
    return false;
}

static void process_chunk(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_v1_command_t *command) {
    const uint16_t offset = command->payload.chunk.offset;
    const uint8_t  length = command->payload.chunk.length;
    const uint16_t end    = (uint16_t)(offset + length);
    noah_profile_candidate_backend_result_t result;

    if (!transaction->has_candidate) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE, offset);
        return;
    }
    if (command->transaction_id != transaction->status.transaction_id) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION, offset);
        return;
    }
    if (transaction->poisoned) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_POISONED, offset);
        return;
    }
    if (transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING && transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE, offset);
        return;
    }
    if (end < offset || end > transaction->metadata.payload_length) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_CAPACITY_EXCEEDED, offset);
        return;
    }
    if (offset > transaction->status.next_offset || (offset < transaction->status.next_offset && end > transaction->status.next_offset)) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_OUT_OF_ORDER, offset);
        return;
    }

    if (offset < transaction->status.next_offset) {
        uint8_t existing[NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX];

        if (!transaction->backend.read) {
            poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, offset);
            return;
        }
        result = transaction->backend.read(transaction->backend.context, offset, existing, length);
        if (!backend_complete(result)) {
            poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, offset);
            return;
        }
        if (memcmp(existing, command->payload.chunk.bytes, length) != 0) {
            poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_CONFLICTING_RETRY, offset);
            return;
        }
        clear_error(transaction);
        return;
    }

    if (transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING || !transaction->backend.write) {
        set_simple_error(transaction, transaction->backend.write ? NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE : NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, offset);
        return;
    }
    result = transaction->backend.write(transaction->backend.context, offset, command->payload.chunk.bytes, length);
    if (!backend_complete(result)) {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, offset);
        return;
    }

    transaction->status.next_offset = end;
    transaction->status.state       = end == transaction->metadata.payload_length ? NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE : NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING;
    clear_error(transaction);
}

static void process_validate(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_v1_command_t *command) {
    noah_profile_candidate_backend_result_t result;
    noah_profile_candidate_v1_error_t       error = noah_profile_candidate_v1_no_error();

    if (!transaction->has_candidate) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    if (command->transaction_id != transaction->status.transaction_id) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    if (transaction->poisoned) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_POISONED, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    if (transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE, transaction->status.next_offset);
        return;
    }
    if (!transaction->backend.validation_begin || !transaction->backend.validation_step) {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }

    result = transaction->backend.validation_begin(transaction->backend.context, &transaction->metadata, &error);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED;
        clear_error(transaction);
    } else if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK || result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING;
        clear_error(transaction);
    } else if (result == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED) {
        set_backend_rejection(transaction, &error);
    } else {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
    }
}

static void process_abort(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_v1_command_t *command) {
    noah_profile_candidate_backend_result_t result;

    if (!transaction->has_candidate) {
        transaction->last_aborted_transaction_id = command->transaction_id;
        transaction->status.transaction_id       = command->transaction_id;
        transaction->status.state                = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
        transaction->status.next_offset          = 0u;
        transaction->status.payload_length       = 0u;
        transaction->status.digest               = 0u;
        transaction->poisoned                    = false;
        memset(&transaction->metadata, 0, sizeof(transaction->metadata));
        clear_error(transaction);
        return;
    }
    if (command->transaction_id != transaction->status.transaction_id) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    if (transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    if (!transaction->backend.abort) {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }

    result = transaction->backend.abort(transaction->backend.context);
    if (!backend_complete(result)) {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }

    transaction->last_aborted_transaction_id = command->transaction_id;
    transaction->has_candidate                = false;
    transaction->poisoned                     = false;
    memset(&transaction->metadata, 0, sizeof(transaction->metadata));
    transaction->status.state          = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
    transaction->status.next_offset    = 0u;
    transaction->status.payload_length = 0u;
    transaction->status.digest         = 0u;
    clear_error(transaction);
}

static void activation_failed(noah_profile_candidate_transaction_t *transaction) {
    transaction->has_candidate = false;
    transaction->poisoned      = true;
    transaction->status.state  = NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED;
    set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_ACTIVATION_FAILED, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
}

static void durability_unknown(noah_profile_candidate_transaction_t *transaction) {
    transaction->has_candidate = false;
    transaction->poisoned      = true;
    transaction->status.state  = NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED;
    set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_DURABILITY_UNKNOWN, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
}

static void begin_activation(noah_profile_candidate_transaction_t *transaction) {
    noah_profile_candidate_backend_result_t result;

    if (!transaction->backend.activation_begin || !transaction->backend.activation_step) {
        activation_failed(transaction);
        return;
    }
    result = transaction->backend.activation_begin(transaction->backend.context);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK || result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING;
        clear_error(transaction);
    } else {
        activation_failed(transaction);
    }
}

static void process_commit(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_v1_command_t *command) {
    noah_profile_candidate_backend_result_t result;

    if (!transaction->has_candidate) {
        if (transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE && command->transaction_id == transaction->last_committed_transaction_id && transaction->last_committed_transaction_id != 0u) {
            transaction->status.transaction_id = command->transaction_id;
            transaction->status.state          = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
            clear_error(transaction);
        } else {
            set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        }
        return;
    }
    if (command->transaction_id != transaction->status.transaction_id) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_WRONG_TRANSACTION, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    if (transaction->poisoned) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_POISONED, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    if (transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING || transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING) {
        clear_error(transaction);
        return;
    }
    if (transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED) {
        set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    if (!transaction->backend.commit_begin || !transaction->backend.commit_step) {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return;
    }
    result = transaction->backend.commit_begin(transaction->backend.context);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING;
        clear_error(transaction);
    } else if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        transaction->last_committed_transaction_id = transaction->status.transaction_id;
        begin_activation(transaction);
    } else {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
    }
}

static bool process_mailbox(noah_profile_candidate_transaction_t *transaction, const noah_profile_candidate_v1_command_t *command) {
    transaction->status.last_operation = command->operation;
    transaction->status.operation_sequence++;

    switch (command->operation) {
        case NOAH_PROFILE_CANDIDATE_V1_OPERATION_BEGIN:
            return process_begin(transaction, command);
        case NOAH_PROFILE_CANDIDATE_V1_OPERATION_CHUNK:
            process_chunk(transaction, command);
            break;
        case NOAH_PROFILE_CANDIDATE_V1_OPERATION_VALIDATE:
            process_validate(transaction, command);
            break;
        case NOAH_PROFILE_CANDIDATE_V1_OPERATION_ABORT:
            process_abort(transaction, command);
            break;
        case NOAH_PROFILE_CANDIDATE_V1_OPERATION_COMMIT:
            process_commit(transaction, command);
            break;
        default:
            set_simple_error(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_UNSUPPORTED_OPERATION, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
            break;
    }
    return false;
}

static void process_validation_step(noah_profile_candidate_transaction_t *transaction) {
    noah_profile_candidate_v1_error_t       error = noah_profile_candidate_v1_no_error();
    noah_profile_candidate_backend_result_t result = transaction->backend.validation_step(transaction->backend.context, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET, &error);

    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        return;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID) {
        transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED;
        clear_error(transaction);
    } else if (result == NOAH_PROFILE_CANDIDATE_BACKEND_REJECTED) {
        set_backend_rejection(transaction, &error);
    } else {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
    }
}

static void process_commit_step(noah_profile_candidate_transaction_t *transaction) {
    noah_profile_candidate_backend_result_t result = transaction->backend.commit_step(transaction->backend.context, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET);

    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        return;
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        transaction->last_committed_transaction_id = transaction->status.transaction_id;
        begin_activation(transaction);
    } else if (result == NOAH_PROFILE_CANDIDATE_BACKEND_DURABILITY_UNKNOWN) {
        durability_unknown(transaction);
    } else {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
    }
}

static void process_activation_step(noah_profile_candidate_transaction_t *transaction) {
    noah_profile_candidate_backend_result_t result = transaction->backend.activation_step(transaction->backend.context);

    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        return;
    }
    if (result != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        activation_failed(transaction);
        return;
    }
    transaction->has_candidate                 = false;
    transaction->poisoned                      = false;
    memset(&transaction->metadata, 0, sizeof(transaction->metadata));
    transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
    clear_error(transaction);
}

bool noah_profile_candidate_transaction_scan(noah_profile_candidate_transaction_t *transaction) {
    if (!transaction) {
        return false;
    }
    if (transaction->mailbox.pending) {
        noah_profile_candidate_v1_command_t command = transaction->mailbox.command;
        transaction->mailbox.pending               = false;
        if (process_mailbox(transaction, &command)) {
            transaction->mailbox.command = command;
            transaction->mailbox.pending = true;
        }
        return true;
    }
    if (transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING && transaction->backend.validation_step) {
        process_validation_step(transaction);
        return true;
    }
    if (transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING && transaction->backend.commit_step) {
        process_commit_step(transaction);
        return true;
    }
    if (transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING && transaction->backend.activation_step) {
        process_activation_step(transaction);
        return true;
    }
    return false;
}

static noah_profile_candidate_expire_result_t cancel_precommit(noah_profile_candidate_transaction_t *transaction, noah_profile_candidate_v1_error_id_t reason, bool discard_mailbox) {
    noah_profile_candidate_v1_state_t state;

    if (!transaction || !transaction->has_candidate) {
        return NOAH_PROFILE_CANDIDATE_EXPIRE_NOTHING;
    }
    state = transaction->status.state;
    if (state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING || state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING) {
        return NOAH_PROFILE_CANDIDATE_EXPIRE_DURABLE_PHASE;
    }
    if (state != NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING && state != NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE && state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING && state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED && state != NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED) {
        return NOAH_PROFILE_CANDIDATE_EXPIRE_NOTHING;
    }
    if (transaction->mailbox.pending) {
        if (!discard_mailbox) {
            return NOAH_PROFILE_CANDIDATE_EXPIRE_MAILBOX_BUSY;
        }
        transaction->status.last_operation = transaction->mailbox.command.operation;
        transaction->mailbox.pending = false;
        memset(&transaction->mailbox.command, 0, sizeof(transaction->mailbox.command));
    }
    if (reason == NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED) {
        // Supersession is an asynchronous status event even when no host
        // command was queued. Advance exactly once so polling can distinguish
        // it from the prior processed operation.
        transaction->status.operation_sequence++;
    }
    if (!transaction->backend.abort || transaction->backend.abort(transaction->backend.context) != NOAH_PROFILE_CANDIDATE_BACKEND_OK) {
        poison(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
        return NOAH_PROFILE_CANDIDATE_EXPIRE_BACKEND_ERROR;
    }

    transaction->last_aborted_transaction_id = transaction->status.transaction_id;
    transaction->has_candidate                = false;
    transaction->poisoned                     = false;
    memset(&transaction->metadata, 0, sizeof(transaction->metadata));
    transaction->status.state = NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE;
    if (reason == NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT) {
        transaction->status.next_offset    = 0u;
        transaction->status.payload_length = 0u;
        transaction->status.digest         = 0u;
    }
    set_simple_error(transaction, reason, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U16);
    return NOAH_PROFILE_CANDIDATE_EXPIRE_DONE;
}

noah_profile_candidate_expire_result_t noah_profile_candidate_transaction_expire_precommit(noah_profile_candidate_transaction_t *transaction) {
    return cancel_precommit(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT, false);
}

noah_profile_candidate_expire_result_t noah_profile_candidate_transaction_supersede_precommit(noah_profile_candidate_transaction_t *transaction) {
    return cancel_precommit(transaction, NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED, true);
}

void noah_profile_candidate_transaction_status(const noah_profile_candidate_transaction_t *transaction, noah_profile_candidate_v1_status_t *status) {
    if (!status) {
        return;
    }
    if (!transaction) {
        memset(status, 0, sizeof(*status));
        status->error = noah_profile_candidate_v1_no_error();
        return;
    }

    *status = transaction->status;
    status->flags = 0u;
    if (transaction->mailbox.pending) {
        status->flags |= NOAH_PROFILE_CANDIDATE_V1_STATUS_MAILBOX_PENDING;
    }
    if (transaction->poisoned) {
        status->flags |= NOAH_PROFILE_CANDIDATE_V1_STATUS_POISONED;
    }
}

_Static_assert(sizeof(noah_profile_candidate_transaction_t) <= 256u, "Candidate owner must stay bounded and must never contain a full profile buffer");
