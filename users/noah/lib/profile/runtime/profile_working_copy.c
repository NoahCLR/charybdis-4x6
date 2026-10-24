// ───────────────────────────────────────────────────────────────────────────
// Working Copy: Live Edits In RAM (D-F32)
// ───────────────────────────────────────────────────────────────────────────

#include "profile_working_copy.h"

#include <string.h>

#include "../storage/profile_checksum.h"
#include "../storage/profile_store.h"

static void put_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

static uint32_t staged_base(const noah_profile_working_copy_t *copy) {
    return (copy->flags & NOAH_PROFILE_WORKING_EDIT_REPLACE) ? 0u : copy->capacity - copy->insert_total;
}

static void clear_edit(noah_profile_working_copy_t *copy) {
    copy->rules            = NULL;
    copy->transaction_id   = 0u;
    copy->flags            = 0u;
    copy->splice_count     = 0u;
    copy->splices_received = 0u;
    copy->piece_count      = 0u;
    copy->validate_mask    = 0u;
    copy->changed_mask     = 0u;
    copy->new_mask         = 0u;
    copy->insert_total     = 0u;
    copy->insert_received  = 0u;
    copy->insert_crc32     = 0u;
    copy->old_length       = 0u;
    copy->new_length       = 0u;
    copy->load_offset      = 0u;
    copy->target_revision  = 0u;
    copy->phase            = NOAH_PROFILE_WORKING_PHASE_IDLE;
}

static noah_profile_working_result_t finish(noah_profile_working_copy_t *copy, noah_profile_working_result_t result) {
    copy->settling            = false;
    copy->last_transaction_id = copy->transaction_id;
    copy->last_result         = (uint8_t)result;
    copy->last_phase          = copy->phase;
    clear_edit(copy);
    return result;
}

static void note_publications(noah_profile_working_copy_t *copy) {
    noah_active_profile_status_t status;

    if (copy->provider && noah_active_profile_provider_status(copy->provider, &status) == NOAH_ACTIVE_PROFILE_OK) {
        copy->publication_count = status.publication_count;
    }
}

bool noah_profile_working_copy_observe(noah_profile_working_copy_t *copy, bool same_content) {
    noah_active_profile_status_t status;

    if (!copy || !copy->provider || noah_active_profile_provider_status(copy->provider, &status) != NOAH_ACTIVE_PROFILE_OK || status.publication_count == copy->publication_count) {
        return false;
    }
    copy->publication_count = status.publication_count;
    (void)noah_profile_working_copy_note_replaced(copy, !same_content);
    return true;
}

void noah_profile_working_copy_init(noah_profile_working_copy_t *copy, uint8_t *bytes, uint32_t capacity, noah_active_profile_provider_t *provider, const noah_profile_validator_v1_compatibility_t *host_rules, const noah_profile_validator_v1_compatibility_t *other_half_rules, uint32_t session) {
    if (!copy) {
        return;
    }
    memset(copy, 0, sizeof(*copy));
    copy->bytes      = bytes;
    copy->capacity   = bytes ? capacity : 0u;
    copy->reader     = noah_profile_reader_from_memory(bytes, copy->capacity);
    copy->provider   = provider;
    copy->host_rules = host_rules;
    copy->other_half_rules = other_half_rules ? other_half_rules : host_rules;
    copy->session    = session;
    copy->error      = noah_profile_validator_v1_no_error();
    note_publications(copy);
}

bool noah_profile_working_copy_adopt(noah_profile_working_copy_t *copy, uint32_t session, uint32_t revision) {
    if (!copy || copy->phase != NOAH_PROFILE_WORKING_PHASE_IDLE || copy->active) {
        return false;
    }
    copy->session  = session;
    copy->revision = revision;
    return true;
}

bool noah_profile_working_copy_busy(const noah_profile_working_copy_t *copy) {
    return copy && copy->phase != NOAH_PROFILE_WORKING_PHASE_IDLE;
}

noah_profile_working_result_t noah_profile_working_copy_begin(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint32_t session, uint32_t revision, uint8_t splice_count, uint32_t insert_total, uint8_t flags, bool from_other_half) {
    bool replace = (flags & NOAH_PROFILE_WORKING_EDIT_REPLACE) != 0u;
    const noah_profile_validator_v1_compatibility_t *rules;

    if (!copy || !copy->bytes || !copy->provider || transaction_id == 0u) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    rules = from_other_half ? copy->other_half_rules : copy->host_rules;
    if (copy->phase != NOAH_PROFILE_WORKING_PHASE_IDLE) {
        // A repeated BEGIN whose answer was lost changes nothing.
        if (copy->phase == NOAH_PROFILE_WORKING_PHASE_RECEIVING && copy->transaction_id == transaction_id && copy->splice_count == splice_count && copy->insert_total == insert_total && copy->flags == flags && copy->rules == rules && copy->target_revision == revision + 1u && session == copy->session) {
            return NOAH_PROFILE_WORKING_OK;
        }
        return NOAH_PROFILE_WORKING_BUSY;
    }
    if (!rules || (flags & (uint8_t)~NOAH_PROFILE_WORKING_EDIT_FLAGS) != 0u) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    if (session != copy->session || revision != copy->revision) {
        return NOAH_PROFILE_WORKING_STALE;
    }
    if (revision == UINT32_MAX) {
        // Never wraps back into an identity a host may still hold.
        return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
    }
    if (noah_active_profile_provider_copy_active(copy->provider, &copy->base) != NOAH_ACTIVE_PROFILE_OK) {
        return NOAH_PROFILE_WORKING_BUSY;
    }
    if ((copy->base.identity.kind == NOAH_ACTIVE_PROFILE_KIND_WORKING_COPY) != copy->active) {
        return NOAH_PROFILE_WORKING_INVALID_STATE;
    }
    if (replace) {
        if (splice_count != 0u || insert_total < NOAH_PROFILE_BLOB_V1_HEADER_SIZE) {
            return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
        }
        if (copy->active) {
            return NOAH_PROFILE_WORKING_NOT_CLEAN;
        }
        if (insert_total > copy->capacity || insert_total > rules->max_blob_size) {
            return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
        }
    } else {
        if (splice_count == 0u || splice_count > NOAH_PROFILE_WORKING_MAX_SPLICES) {
            return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
        }
        if (copy->base.profile.byte_length > copy->capacity || insert_total > copy->capacity - copy->base.profile.byte_length) {
            return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
        }
    }
    clear_edit(copy);
    copy->rules           = rules;
    copy->transaction_id  = transaction_id;
    copy->flags           = flags;
    copy->splice_count    = splice_count;
    copy->insert_total    = insert_total;
    copy->insert_crc32    = NOAH_PROFILE_CRC32_INITIAL;
    copy->old_length      = (uint32_t)copy->base.profile.byte_length;
    copy->target_revision = revision + 1u;
    copy->error           = noah_profile_validator_v1_no_error();
    copy->phase           = NOAH_PROFILE_WORKING_PHASE_RECEIVING;
    return NOAH_PROFILE_WORKING_OK;
}

static noah_profile_working_result_t receiving(const noah_profile_working_copy_t *copy, uint16_t transaction_id) {
    if (!copy || transaction_id == 0u) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    if (copy->phase != NOAH_PROFILE_WORKING_PHASE_RECEIVING) {
        return copy->phase == NOAH_PROFILE_WORKING_PHASE_IDLE ? NOAH_PROFILE_WORKING_INVALID_STATE : NOAH_PROFILE_WORKING_BUSY;
    }
    return transaction_id == copy->transaction_id ? NOAH_PROFILE_WORKING_OK : NOAH_PROFILE_WORKING_WRONG_TRANSACTION;
}

static bool splice_equal(const noah_profile_working_splice_t *left, const noah_profile_working_splice_t *right) {
    return left->offset == right->offset && left->remove == right->remove && left->insert == right->insert && left->domain_id == right->domain_id && left->flags == right->flags;
}

noah_profile_working_result_t noah_profile_working_copy_splice(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint8_t index, const noah_profile_working_splice_t *splice) {
    noah_profile_working_result_t result = receiving(copy, transaction_id);

    if (result != NOAH_PROFILE_WORKING_OK) {
        return result;
    }
    if (!splice || index >= copy->splice_count) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    if (index < copy->splices_received) {
        return splice_equal(&copy->splices[index], splice) ? NOAH_PROFILE_WORKING_OK : NOAH_PROFILE_WORKING_OUT_OF_ORDER;
    }
    if (index != copy->splices_received) {
        return NOAH_PROFILE_WORKING_OUT_OF_ORDER;
    }
    if ((splice->flags & (uint8_t)~NOAH_PROFILE_WORKING_SPLICE_FLAGS) != 0u || splice->flags == NOAH_PROFILE_WORKING_SPLICE_FLAGS) {
        return NOAH_PROFILE_WORKING_INVALID_SPLICE;
    }
    copy->splices[index] = *splice;
    copy->splices_received++;
    return NOAH_PROFILE_WORKING_OK;
}

noah_profile_working_result_t noah_profile_working_copy_data(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint32_t offset, const uint8_t *bytes, uint32_t count) {
    noah_profile_working_result_t result = receiving(copy, transaction_id);
    uint8_t                      *staged;

    if (result != NOAH_PROFILE_WORKING_OK) {
        return result;
    }
    if (!bytes || count == 0u) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    if (offset > copy->insert_total || count > copy->insert_total - offset) {
        return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
    }
    staged = copy->bytes + staged_base(copy);
    // A repeated chunk whose answer was lost must match what was staged.
    if (offset < copy->insert_received) {
        return count <= copy->insert_received - offset && memcmp(staged + offset, bytes, count) == 0 ? NOAH_PROFILE_WORKING_OK : NOAH_PROFILE_WORKING_OUT_OF_ORDER;
    }
    if (offset != copy->insert_received) {
        return NOAH_PROFILE_WORKING_OUT_OF_ORDER;
    }
    memcpy(staged + offset, bytes, count);
    copy->insert_crc32 = noah_profile_crc32_update(copy->insert_crc32, bytes, count);
    copy->insert_received += count;
    return NOAH_PROFILE_WORKING_OK;
}

// The resulting profile, assembled from the plan's pieces without moving a
// byte of the current one.
static bool overlay_read(void *context, size_t offset, uint8_t *target, size_t length) {
    const noah_profile_working_copy_t *copy = context;
    uint32_t                           base = staged_base(copy);

    for (uint8_t index = 0u; index < copy->piece_count && length != 0u; index++) {
        const noah_profile_working_piece_t *piece = &copy->pieces[index];
        uint32_t                            skip;
        uint32_t                            count;
        const uint8_t                      *source;

        if (offset >= (size_t)piece->offset + piece->length || offset < piece->offset) {
            continue;
        }
        skip  = (uint32_t)offset - piece->offset;
        count = piece->length - skip;
        if (count > length) {
            count = (uint32_t)length;
        }
        source = piece->kind == NOAH_PROFILE_WORKING_PIECE_SOURCE ? copy->bytes + piece->source : piece->kind == NOAH_PROFILE_WORKING_PIECE_STAGED ? copy->bytes + base + piece->source : copy->literal + piece->source;
        memcpy(target, source + skip, count);
        target += count;
        offset += count;
        length -= count;
    }
    return length == 0u;
}

static noah_profile_reader_t overlay_reader(noah_profile_working_copy_t *copy) {
    return (noah_profile_reader_t){.read = overlay_read, .context = copy, .length = copy->new_length};
}

static bool add_piece(noah_profile_working_copy_t *copy, uint32_t *offset, uint32_t length, uint32_t source, noah_profile_working_piece_kind_t kind) {
    if (length == 0u) {
        return true;
    }
    if (copy->piece_count >= NOAH_PROFILE_WORKING_MAX_PIECES || length > UINT32_MAX - *offset) {
        return false;
    }
    copy->pieces[copy->piece_count++] = (noah_profile_working_piece_t){.offset = *offset, .length = length, .source = source, .kind = (uint8_t)kind};
    *offset += length;
    return true;
}

// Reads the current profile's envelope: where each domain's payload is.
static noah_profile_working_result_t read_layout(noah_profile_working_copy_t *copy) {
    uint8_t                 header[NOAH_PROFILE_BLOB_V1_HEADER_SIZE];
    noah_profile_envelope_t walk;

    memset(copy->old_domains, 0, sizeof(copy->old_domains));
    if (!noah_active_profile_snapshot_read(&copy->base, 0u, header, sizeof(header)) || noah_profile_envelope_begin(&walk, header, copy->old_length, NULL) != NOAH_PROFILE_CODEC_V1_OK) {
        return NOAH_PROFILE_WORKING_READ_ERROR;
    }
    memcpy(copy->literal, header, sizeof(header));
    while (walk.index < walk.count) {
        noah_profile_domain_record_t record;

        if (walk.byte_length - walk.offset < NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE || !noah_active_profile_snapshot_read(&copy->base, walk.offset, header, NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE) || noah_profile_envelope_next(&walk, header, &record, NULL) != NOAH_PROFILE_CODEC_V1_OK) {
            return NOAH_PROFILE_WORKING_READ_ERROR;
        }
        copy->old_domains[record.shape->index] = (noah_profile_working_domain_t){.payload_offset = record.offset, .length = record.length, .present = true};
    }
    return noah_profile_envelope_finish(&walk, NULL) == NOAH_PROFILE_CODEC_V1_OK ? NOAH_PROFILE_WORKING_OK : NOAH_PROFILE_WORKING_READ_ERROR;
}

// Checks the splices against the current profile and lays out the result.
static noah_profile_working_result_t plan(noah_profile_working_copy_t *copy) {
    noah_profile_working_result_t result = read_layout(copy);
    uint32_t                      inserted = 0u;
    uint32_t                      staged   = 0u;
    uint32_t                      cursor   = 0u;
    uint32_t                      literal  = NOAH_PROFILE_BLOB_V1_HEADER_SIZE;
    uint8_t                       count    = 0u;
    uint8_t                       next     = 0u;

    if (result != NOAH_PROFILE_WORKING_OK) {
        return result;
    }
    // Splices come in domain order, then offset order, and never overlap. A
    // domain created or dropped has that one splice only.
    for (uint8_t index = 0u; index < copy->splice_count; index++) {
        const noah_profile_working_splice_t *splice = &copy->splices[index];
        const noah_profile_domain_shape_t   *shape  = noah_profile_domain_find(splice->domain_id);
        const noah_profile_working_domain_t *old;

        if (!shape || !(shape->mask & copy->rules->allowed_domain_mask)) {
            return NOAH_PROFILE_WORKING_UNSUPPORTED_DOMAIN;
        }
        old = &copy->old_domains[shape->index];
        if (index != 0u) {
            const noah_profile_working_splice_t *prior       = &copy->splices[index - 1u];
            const noah_profile_domain_shape_t   *prior_shape = noah_profile_domain_find(prior->domain_id);

            if (shape->index < prior_shape->index || (shape->index == prior_shape->index && (prior->flags != 0u || splice->flags != 0u || splice->offset <= prior->offset || splice->offset < prior->offset + prior->remove))) {
                return NOAH_PROFILE_WORKING_INVALID_SPLICE;
            }
        }
        if (splice->flags & NOAH_PROFILE_WORKING_SPLICE_CREATE) {
            if (old->present || splice->offset != 0u || splice->remove != 0u) {
                return NOAH_PROFILE_WORKING_INVALID_SPLICE;
            }
        } else if (splice->flags & NOAH_PROFILE_WORKING_SPLICE_DROP) {
            if (!old->present || splice->offset != 0u || splice->remove != old->length || splice->insert != 0u || (shape->mask & copy->rules->required_domain_mask)) {
                return NOAH_PROFILE_WORKING_INVALID_SPLICE;
            }
        } else if (!old->present || splice->offset > old->length || splice->remove > old->length - splice->offset) {
            return NOAH_PROFILE_WORKING_INVALID_SPLICE;
        }
        if (splice->insert > UINT32_MAX - inserted) {
            return NOAH_PROFILE_WORKING_INVALID_SPLICE;
        }
        inserted += splice->insert;
    }
    if (inserted != copy->insert_total) {
        return NOAH_PROFILE_WORKING_INVALID_SPLICE;
    }

    memset(copy->new_domains, 0, sizeof(copy->new_domains));
    copy->piece_count = 0u;
    if (!add_piece(copy, &cursor, NOAH_PROFILE_BLOB_V1_HEADER_SIZE, 0u, NOAH_PROFILE_WORKING_PIECE_LITERAL)) {
        return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
    }
    for (uint8_t domain = 0u; domain < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; domain++) {
        const noah_profile_domain_shape_t   *shape = noah_profile_domain_at(domain);
        const noah_profile_working_domain_t *old   = &copy->old_domains[domain];
        uint8_t                              first = next;
        uint64_t                             length;
        uint32_t                             source = 0u;
        bool                                 created = false;
        bool                                 dropped = false;

        while (next < copy->splice_count && noah_profile_domain_find(copy->splices[next].domain_id)->index == domain) {
            created |= (copy->splices[next].flags & NOAH_PROFILE_WORKING_SPLICE_CREATE) != 0u;
            dropped |= (copy->splices[next].flags & NOAH_PROFILE_WORKING_SPLICE_DROP) != 0u;
            next++;
        }
        if (next != first) {
            copy->changed_mask |= shape->mask;
        }
        if (dropped || (!old->present && !created)) {
            continue;
        }
        length = old->present ? old->length : 0u;
        for (uint8_t index = first; index < next; index++) {
            length = length + copy->splices[index].insert - copy->splices[index].remove;
        }
        if (length > UINT32_MAX) {
            return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
        }
        copy->literal[literal]      = shape->id;
        copy->literal[literal + 1u] = shape->version;
        put_u32(&copy->literal[literal + 2u], (uint32_t)length);
        if (!add_piece(copy, &cursor, NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE, literal, NOAH_PROFILE_WORKING_PIECE_LITERAL)) {
            return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
        }
        literal += NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE;
        copy->new_domains[domain] = (noah_profile_working_domain_t){.payload_offset = cursor, .length = (uint32_t)length, .present = true};
        for (uint8_t index = first; index < next; index++) {
            const noah_profile_working_splice_t *splice = &copy->splices[index];

            if (!created && !add_piece(copy, &cursor, splice->offset - source, old->payload_offset + source, NOAH_PROFILE_WORKING_PIECE_SOURCE)) {
                return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
            }
            if (!add_piece(copy, &cursor, splice->insert, staged, NOAH_PROFILE_WORKING_PIECE_STAGED)) {
                return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
            }
            staged += splice->insert;
            source = splice->offset + splice->remove;
        }
        if (!created && !add_piece(copy, &cursor, old->length - source, old->payload_offset + source, NOAH_PROFILE_WORKING_PIECE_SOURCE)) {
            return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
        }
        if (next != first) {
            copy->validate_mask |= shape->mask;
        }
        copy->new_mask |= shape->mask;
        count++;
    }
    copy->literal[6]   = count;
    copy->new_length   = cursor;
    // Staging sits above both profiles, so neither is ever written over it.
    if (cursor > copy->rules->max_blob_size || cursor > copy->capacity || copy->insert_total > copy->capacity - cursor) {
        return NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED;
    }
    if ((copy->rules->required_domain_mask & (uint8_t)~copy->new_mask) != 0u) {
        return NOAH_PROFILE_WORKING_INVALID_SPLICE;
    }
    return NOAH_PROFILE_WORKING_OK;
}

// Moves the current profile's bytes into the new layout in place, then
// writes the inserted bytes and headers. Kept bytes keep their order, so
// those moving down go first, lowest first, and those moving up after,
// highest first: no move overwrites bytes another has yet to make. The
// staging lies above both layouts and is never written.
static void apply_plan(void *context) {
    noah_profile_working_copy_t *copy = context;
    uint32_t                     base = staged_base(copy);

    for (uint8_t index = 0u; index < copy->piece_count; index++) {
        const noah_profile_working_piece_t *piece = &copy->pieces[index];

        if (piece->kind == NOAH_PROFILE_WORKING_PIECE_SOURCE && piece->offset < piece->source) {
            memmove(copy->bytes + piece->offset, copy->bytes + piece->source, piece->length);
        }
    }
    for (uint8_t index = copy->piece_count; index-- > 0u;) {
        const noah_profile_working_piece_t *piece = &copy->pieces[index];

        if (piece->kind == NOAH_PROFILE_WORKING_PIECE_SOURCE && piece->offset > piece->source) {
            memmove(copy->bytes + piece->offset, copy->bytes + piece->source, piece->length);
        }
    }
    for (uint8_t index = 0u; index < copy->piece_count; index++) {
        const noah_profile_working_piece_t *piece = &copy->pieces[index];

        if (piece->kind == NOAH_PROFILE_WORKING_PIECE_STAGED) {
            memcpy(copy->bytes + piece->offset, copy->bytes + base + piece->source, piece->length);
        } else if (piece->kind == NOAH_PROFILE_WORKING_PIECE_LITERAL) {
            memcpy(copy->bytes + piece->offset, copy->literal + piece->source, piece->length);
        }
    }
}

static bool assemble_profile(noah_profile_working_copy_t *copy);

static noah_profile_working_result_t begin_next_domain(noah_profile_working_copy_t *copy) {
    const noah_profile_domain_shape_t *shape = NULL;
    noah_profile_reader_t              reader;

    for (uint8_t domain = 0u; domain < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; domain++) {
        if (copy->validate_mask & (1u << domain)) {
            shape = noah_profile_domain_at(domain);
            break;
        }
    }
    if (!shape) {
        return NOAH_PROFILE_WORKING_OK;
    }
    reader = overlay_reader(copy);
    if (noah_profile_validator_v1_begin_domain(&copy->validator, &reader, 0u, shape->id, (noah_profile_domain_range_t){copy->new_domains[shape->index].payload_offset, copy->new_domains[shape->index].length}, copy->rules, &copy->error) != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        return NOAH_PROFILE_WORKING_VALIDATION_REJECTED;
    }
    return NOAH_PROFILE_WORKING_QUEUED;
}

static noah_profile_working_result_t begin_validation(noah_profile_working_copy_t *copy) {
    noah_profile_working_result_t result;

    memset(&copy->next, 0, sizeof(copy->next));
    copy->phase = NOAH_PROFILE_WORKING_PHASE_VALIDATING;
    if (copy->flags & NOAH_PROFILE_WORKING_EDIT_REPLACE) {
        return noah_profile_validator_v1_begin_structure(&copy->validator, &copy->reader, 0u, copy->new_length, copy->rules->action_abi_digest, copy->rules, &copy->error) == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS ? NOAH_PROFILE_WORKING_QUEUED : NOAH_PROFILE_WORKING_VALIDATION_REJECTED;
    }
    result = begin_next_domain(copy);
    if (result == NOAH_PROFILE_WORKING_OK) {
        // Nothing left to check (the edit only drops a domain).
        if (!assemble_profile(copy)) {
            return NOAH_PROFILE_WORKING_PUBLICATION_FAILED;
        }
        copy->phase = NOAH_PROFILE_WORKING_PHASE_WAITING_SAFE;
        return NOAH_PROFILE_WORKING_QUEUED;
    }
    return result;
}

noah_profile_working_result_t noah_profile_working_copy_commit(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint32_t crc32) {
    noah_profile_working_result_t result = receiving(copy, transaction_id);

    if (result != NOAH_PROFILE_WORKING_OK) {
        return result;
    }
    if (copy->splices_received != copy->splice_count || copy->insert_received != copy->insert_total) {
        return NOAH_PROFILE_WORKING_INVALID_STATE;
    }
    if (noah_profile_crc32_finish(copy->insert_crc32) != crc32) {
        return finish(copy, NOAH_PROFILE_WORKING_CHECKSUM_MISMATCH);
    }
    if (copy->flags & NOAH_PROFILE_WORKING_EDIT_REPLACE) {
        copy->new_length   = copy->insert_total;
        copy->changed_mask = NOAH_PROFILE_DOMAIN_MASK_ALL;
        result             = begin_validation(copy);
        return result == NOAH_PROFILE_WORKING_QUEUED ? result : finish(copy, result);
    }
    result = plan(copy);
    if (result != NOAH_PROFILE_WORKING_OK) {
        return finish(copy, result);
    }
    if (!copy->active) {
        copy->phase       = NOAH_PROFILE_WORKING_PHASE_LOADING;
        copy->load_offset = 0u;
        return NOAH_PROFILE_WORKING_QUEUED;
    }
    result = begin_validation(copy);
    return result == NOAH_PROFILE_WORKING_QUEUED ? result : finish(copy, result);
}

noah_profile_working_result_t noah_profile_working_copy_abort(noah_profile_working_copy_t *copy, uint16_t transaction_id) {
    if (!copy || transaction_id == 0u) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    if (copy->phase == NOAH_PROFILE_WORKING_PHASE_IDLE) {
        return NOAH_PROFILE_WORKING_OK;
    }
    if (copy->transaction_id != transaction_id) {
        return NOAH_PROFILE_WORKING_WRONG_TRANSACTION;
    }
    // Before publication nothing of the working copy has changed: loading
    // only filled an unused buffer, staging sits above the profile.
    if (copy->phase == NOAH_PROFILE_WORKING_PHASE_REVERTING) {
        return NOAH_PROFILE_WORKING_BUSY;
    }
    (void)finish(copy, NOAH_PROFILE_WORKING_ABORTED);
    return NOAH_PROFILE_WORKING_OK;
}

noah_profile_working_result_t noah_profile_working_copy_revert(noah_profile_working_copy_t *copy, uint16_t transaction_id, uint32_t session, uint32_t revision) {
    if (!copy || transaction_id == 0u) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    if (copy->phase == NOAH_PROFILE_WORKING_PHASE_REVERTING && copy->transaction_id == transaction_id) {
        return NOAH_PROFILE_WORKING_QUEUED;
    }
    if (copy->phase != NOAH_PROFILE_WORKING_PHASE_IDLE) {
        return NOAH_PROFILE_WORKING_BUSY;
    }
    if (session != copy->session || revision != copy->revision) {
        return NOAH_PROFILE_WORKING_STALE;
    }
    copy->transaction_id = transaction_id;
    if (!copy->active) {
        return finish(copy, NOAH_PROFILE_WORKING_OK);
    }
    copy->phase = NOAH_PROFILE_WORKING_PHASE_REVERTING;
    return NOAH_PROFILE_WORKING_QUEUED;
}

static bool scan_load(noah_profile_working_copy_t *copy) {
    uint32_t                      count = copy->old_length - copy->load_offset;
    noah_profile_working_result_t result;

    if (count > NOAH_PROFILE_WORKING_LOAD_BYTES) {
        count = NOAH_PROFILE_WORKING_LOAD_BYTES;
    }
    for (uint32_t done = 0u; done < count;) {
        uint32_t piece = count - done < NOAH_PROFILE_WORKING_LOAD_PIECE ? count - done : NOAH_PROFILE_WORKING_LOAD_PIECE;

        if (!noah_active_profile_snapshot_read(&copy->base, copy->load_offset, copy->bytes + copy->load_offset, piece)) {
            (void)finish(copy, NOAH_PROFILE_WORKING_READ_ERROR);
            return true;
        }
        copy->load_offset += piece;
        done += piece;
    }
    if (copy->load_offset == copy->old_length) {
        result = begin_validation(copy);
        if (result != NOAH_PROFILE_WORKING_QUEUED) {
            (void)finish(copy, result);
        }
    }
    return true;
}

// A view of a domain the edit left alone, moved to where the domain now is
// in the working copy.
static bool keep_view(const noah_profile_working_copy_t *copy, uint8_t domain, size_t *base_offset) {
    const noah_profile_working_domain_t *old = &copy->old_domains[domain];

    if (*base_offset != copy->base.base_offset + old->payload_offset) {
        return false;
    }
    *base_offset = copy->new_domains[domain].payload_offset;
    return true;
}

static bool assemble_profile(noah_profile_working_copy_t *copy) {
    noah_profile_validator_v1_profile_t *next = &copy->next;

    next->domain_mask       = copy->new_mask;
    next->domain_count      = noah_profile_domain_count(copy->new_mask);
    next->byte_length       = copy->new_length;
    next->crc32             = 0u;
    next->digest            = 0u;
    next->action_abi_digest = copy->rules->action_abi_digest;
    if ((copy->new_mask & NOAH_PROFILE_DOMAIN_MASK_RGB) && !(copy->changed_mask & NOAH_PROFILE_DOMAIN_MASK_RGB)) {
        next->rgb = copy->base.profile.rgb;
        if (!keep_view(copy, NOAH_PROFILE_DOMAIN_INDEX_RGB, &next->rgb.base_offset)) {
            return false;
        }
    }
    if ((copy->new_mask & NOAH_PROFILE_DOMAIN_MASK_KEY_BEHAVIORS) && !(copy->changed_mask & NOAH_PROFILE_DOMAIN_MASK_KEY_BEHAVIORS)) {
        next->key_behaviors = copy->base.profile.key_behaviors;
        if (!keep_view(copy, NOAH_PROFILE_DOMAIN_INDEX_KEY_BEHAVIORS, &next->key_behaviors.base_offset)) {
            return false;
        }
    }
    next->rgb.reader           = copy->reader;
    next->key_behaviors.reader = copy->reader;
    return true;
}

static bool scan_validate(noah_profile_working_copy_t *copy) {
    for (uint8_t step = 0u; step < NOAH_PROFILE_WORKING_VALIDATION_STEPS; step++) {
        noah_profile_validator_v1_result_t result = noah_profile_validator_v1_step(&copy->validator, NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX, &copy->error);
        noah_profile_validator_v1_profile_t validated;

        if (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
            continue;
        }
        if (result != NOAH_PROFILE_VALIDATOR_V1_VALID || noah_profile_validator_v1_profile(&copy->validator, &validated, NULL) != NOAH_PROFILE_VALIDATOR_V1_VALID) {
            // Say which domain refused, where its check left that out.
            if (!(copy->flags & NOAH_PROFILE_WORKING_EDIT_REPLACE) && copy->error.domain_id == NOAH_PROFILE_VALIDATOR_V1_LOCATION_NONE_U8 && noah_profile_domain_at(copy->validator.domain)) {
                copy->error.domain_id = noah_profile_domain_at(copy->validator.domain)->id;
            }
            (void)finish(copy, result == NOAH_PROFILE_VALIDATOR_V1_READ_ERROR ? NOAH_PROFILE_WORKING_READ_ERROR : NOAH_PROFILE_WORKING_VALIDATION_REJECTED);
            return true;
        }
        if (copy->flags & NOAH_PROFILE_WORKING_EDIT_REPLACE) {
            copy->next          = validated;
            copy->new_mask      = validated.domain_mask;
            copy->validate_mask = 0u;
        } else {
            uint8_t domain = copy->validator.domain;

            if (domain == NOAH_PROFILE_DOMAIN_INDEX_RGB) {
                copy->next.rgb = validated.rgb;
            } else if (domain == NOAH_PROFILE_DOMAIN_INDEX_KEY_BEHAVIORS) {
                copy->next.key_behaviors = validated.key_behaviors;
            }
            copy->validate_mask &= (uint8_t)~(1u << domain);
            switch (begin_next_domain(copy)) {
                case NOAH_PROFILE_WORKING_QUEUED:
                    return true;
                case NOAH_PROFILE_WORKING_OK:
                    break;
                default:
                    (void)finish(copy, NOAH_PROFILE_WORKING_VALIDATION_REJECTED);
                    return true;
            }
            if (!assemble_profile(copy)) {
                (void)finish(copy, NOAH_PROFILE_WORKING_PUBLICATION_FAILED);
                return true;
            }
        }
        copy->next.rgb.reader           = copy->reader;
        copy->next.key_behaviors.reader = copy->reader;
        copy->phase                     = NOAH_PROFILE_WORKING_PHASE_WAITING_SAFE;
        return true;
    }
    return true;
}

static void record_published(noah_profile_working_copy_t *copy) {
    noah_profile_working_edit_record_t *record = &copy->published;
    uint8_t                             stream = 0u;

    memset(record, 0, sizeof(*record));
    record->insert_total = copy->insert_total;
    record->insert_crc32 = noah_profile_crc32_finish(copy->insert_crc32);
    record->revision     = copy->target_revision;
    record->splice_count = copy->splice_count;
    record->flags        = copy->flags;
    memcpy(record->splices, copy->splices, sizeof(record->splices));
    for (uint8_t index = 0u; index < copy->piece_count; index++) {
        if (copy->pieces[index].kind == NOAH_PROFILE_WORKING_PIECE_STAGED && stream < NOAH_PROFILE_WORKING_MAX_SPLICES) {
            record->stream_offsets[stream++] = copy->pieces[index].offset;
        }
    }
    record->valid = true;
}

bool noah_profile_working_copy_published_stream(const noah_profile_working_copy_t *copy, uint32_t offset, uint8_t *bytes, uint32_t count) {
    const noah_profile_working_edit_record_t *record;
    uint32_t                                  start = 0u;
    uint8_t                                   piece = 0u;

    if (!copy || !bytes || !copy->active || !copy->published.valid || copy->published.revision != copy->revision) {
        return false;
    }
    record = &copy->published;
    if (offset > record->insert_total || count > record->insert_total - offset) {
        return false;
    }
    if (record->flags & NOAH_PROFILE_WORKING_EDIT_REPLACE) {
        memcpy(bytes, copy->bytes + offset, count);
        return true;
    }
    // Inserted bytes were laid out splice by splice, skipping empty inserts.
    for (uint8_t index = 0u; index < record->splice_count && count != 0u; index++) {
        uint32_t length = record->splices[index].insert;
        uint32_t skip;
        uint32_t take;

        if (length == 0u) {
            continue;
        }
        if (offset < start + length) {
            skip = offset - start;
            take = length - skip < count ? length - skip : count;
            memcpy(bytes, copy->bytes + record->stream_offsets[piece] + skip, take);
            bytes += take;
            offset += take;
            count -= take;
        }
        start += length;
        piece++;
    }
    return count == 0u;
}

static bool scan_publish(noah_profile_working_copy_t *copy) {
    const noah_active_profile_identity_t *base = copy->active ? &copy->saved.identity : &copy->base.identity;
    noah_active_profile_snapshot_t           snapshot;
    noah_active_profile_result_t             result;
    bool                                     replace = (copy->flags & NOAH_PROFILE_WORKING_EDIT_REPLACE) != 0u;

    if (noah_active_profile_snapshot_make_working(&copy->next, &copy->reader, 0u, base, copy->session, copy->target_revision, &snapshot) != NOAH_ACTIVE_PROFILE_OK) {
        (void)finish(copy, NOAH_PROFILE_WORKING_PUBLICATION_FAILED);
        return true;
    }
    result = noah_active_profile_provider_publish_in_place(copy->provider, &snapshot, copy->changed_mask, replace ? NULL : apply_plan, copy);
    if (result == NOAH_ACTIVE_PROFILE_WAITING || result == NOAH_ACTIVE_PROFILE_BUSY || result == NOAH_ACTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS) {
        return false;
    }
    if (result != NOAH_ACTIVE_PROFILE_PUBLISHED) {
        (void)finish(copy, NOAH_PROFILE_WORKING_PUBLICATION_FAILED);
        return true;
    }
    if (!copy->active) {
        copy->saved  = copy->base;
        copy->active = true;
    }
    record_published(copy);
    note_publications(copy);
    copy->revision = copy->target_revision;
    (void)finish(copy, NOAH_PROFILE_WORKING_OK);
    return true;
}

static bool scan_revert(noah_profile_working_copy_t *copy) {
    noah_active_profile_result_t result = noah_active_profile_provider_publish_in_place(copy->provider, &copy->saved, NOAH_PROFILE_DOMAIN_MASK_ALL, NULL, NULL);

    if (result == NOAH_ACTIVE_PROFILE_WAITING || result == NOAH_ACTIVE_PROFILE_BUSY || result == NOAH_ACTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS) {
        return false;
    }
    if (result != NOAH_ACTIVE_PROFILE_PUBLISHED) {
        if (copy->settling) {
            // The copy stays; the Save that asked fails.
            copy->settling = false;
            copy->phase    = NOAH_PROFILE_WORKING_PHASE_IDLE;
            return true;
        }
        (void)finish(copy, NOAH_PROFILE_WORKING_PUBLICATION_FAILED);
        return true;
    }
    copy->active = false;
    copy->published.valid = false;
    note_publications(copy);
    if (copy->settling) {
        copy->settling = false;
        copy->phase    = NOAH_PROFILE_WORKING_PHASE_IDLE;
        return true;
    }
    copy->revision++;
    (void)finish(copy, NOAH_PROFILE_WORKING_OK);
    return true;
}

noah_profile_working_result_t noah_profile_working_copy_settle_saved(noah_profile_working_copy_t *copy) {
    if (!copy) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    if (copy->phase != NOAH_PROFILE_WORKING_PHASE_IDLE) {
        return NOAH_PROFILE_WORKING_BUSY;
    }
    if (!copy->active) {
        return NOAH_PROFILE_WORKING_OK;
    }
    copy->settling = true;
    copy->phase    = NOAH_PROFILE_WORKING_PHASE_REVERTING;
    return NOAH_PROFILE_WORKING_QUEUED;
}

bool noah_profile_working_copy_scan(noah_profile_working_copy_t *copy) {
    if (!copy) {
        return false;
    }
    switch (copy->phase) {
        case NOAH_PROFILE_WORKING_PHASE_LOADING:
            return scan_load(copy);
        case NOAH_PROFILE_WORKING_PHASE_VALIDATING:
            return scan_validate(copy);
        case NOAH_PROFILE_WORKING_PHASE_WAITING_SAFE:
            return scan_publish(copy);
        case NOAH_PROFILE_WORKING_PHASE_REVERTING:
            return scan_revert(copy);
        default:
            return false;
    }
}

bool noah_profile_working_copy_note_replaced(noah_profile_working_copy_t *copy, bool content_changed) {
    bool was_active;

    if (!copy) {
        return false;
    }
    if (copy->phase != NOAH_PROFILE_WORKING_PHASE_IDLE) {
        (void)finish(copy, NOAH_PROFILE_WORKING_ABORTED);
    }
    was_active   = copy->active;
    copy->active = false;
    copy->published.valid = false;
    memset(&copy->saved, 0, sizeof(copy->saved));
    if (content_changed && copy->revision != UINT32_MAX) {
        copy->revision++;
    }
    return was_active;
}

bool noah_profile_working_copy_read(const noah_profile_working_copy_t *copy, uint32_t offset, uint8_t *bytes, uint32_t length) {
    noah_active_profile_snapshot_t active;

    return copy && copy->provider && noah_active_profile_provider_copy_active(copy->provider, &active) == NOAH_ACTIVE_PROFILE_OK && noah_active_profile_snapshot_read(&active, offset, bytes, length);
}

bool noah_profile_working_copy_status(const noah_profile_working_copy_t *copy, noah_profile_working_status_t *status) {
    noah_active_profile_status_t provider;

    if (!copy || !status || !copy->provider || noah_active_profile_provider_status(copy->provider, &provider) != NOAH_ACTIVE_PROFILE_OK) {
        return false;
    }
    *status = (noah_profile_working_status_t){
        .session             = copy->session,
        .revision            = copy->revision,
        .staged_bytes        = copy->insert_received,
        .transaction_id      = copy->transaction_id,
        .last_transaction_id = copy->last_transaction_id,
        .phase               = copy->phase,
        .last_result         = copy->last_result,
        .splice_count        = copy->splice_count,
        .splices_received    = copy->splices_received,
        .active              = copy->active,
        .error               = copy->error,
    };
    {
        noah_active_profile_snapshot_t active;

        if (noah_active_profile_provider_copy_active(copy->provider, &active) == NOAH_ACTIVE_PROFILE_OK) {
            status->length      = (uint32_t)active.profile.byte_length;
            status->domain_mask = active.profile.domain_mask;
        }
    }
    return true;
}

// A saved slot's reader reads the store directly, a load piece at a time.
_Static_assert((uint32_t)NOAH_PROFILE_WORKING_LOAD_PIECE <= (uint32_t)NOAH_PROFILE_STORE_IO_CHUNK_MAX, "a load piece is one store read");
