// ───────────────────────────────────────────────────────────────────────────
// One Durable Identity in Three Forms
// ───────────────────────────────────────────────────────────────────────────
//
// A saved profile has one identity tuple: generation, origin half, flags,
// domain mask, schema, payload length, checksums and compatibility digests.
// The store holds it as a record (with the slot it lives in) or, while it is
// staged, as a candidate; the halves exchange it as a split descriptor. Every
// comparison between those forms lives here. Static inline: the compiler may keep one out of line as a small leaf of
// its own; the reviewed stack paths are measured as built.
#pragma once

#include <stdbool.h>

#include "../split/profile_split_authority.h"
#include "profile_store.h"

// Two descriptors say the same thing, readability included.
static inline bool noah_profile_descriptor_equal(const noah_profile_split_descriptor_t *left, const noah_profile_split_descriptor_t *right) {
    return left && right && left->generation == right->generation && left->payload_crc32 == right->payload_crc32 && left->payload_digest == right->payload_digest && left->compiled_default_digest == right->compiled_default_digest && left->action_abi_digest == right->action_abi_digest && left->payload_length == right->payload_length && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->domain_mask == right->domain_mask && left->profile_flags == right->profile_flags && left->origin_half == right->origin_half && left->readable == right->readable && left->has_profile == right->has_profile;
}

// Two records are the same saved profile, wherever each lives: boot selection
// compares the two slots' records this way.
static inline bool noah_profile_record_same_profile(const noah_profile_store_record_t *left, const noah_profile_store_record_t *right) {
    return left && right && left->format_version == right->format_version && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->domain_mask == right->domain_mask && left->flags == right->flags && left->payload_length == right->payload_length && left->generation == right->generation && left->origin_half == right->origin_half && left->payload_crc32 == right->payload_crc32 && left->payload_digest == right->payload_digest && left->compiled_default_digest == right->compiled_default_digest && left->action_abi_digest == right->action_abi_digest;
}

// Two records are the same saved profile in the same slot.
static inline bool noah_profile_record_equal(const noah_profile_store_record_t *left, const noah_profile_store_record_t *right) {
    return noah_profile_record_same_profile(left, right) && left->slot == right->slot;
}

static inline bool noah_profile_candidate_equal(const noah_profile_store_candidate_t *left, const noah_profile_store_candidate_t *right) {
    return left && right && left->format_version == right->format_version && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->domain_mask == right->domain_mask && left->flags == right->flags && left->payload_length == right->payload_length && left->generation == right->generation && left->origin_half == right->origin_half && left->payload_crc32 == right->payload_crc32 && left->payload_digest == right->payload_digest && left->compiled_default_digest == right->compiled_default_digest && left->action_abi_digest == right->action_abi_digest;
}

// The descriptor names this saved record: it is a readable profile descriptor
// and every field of the tuple matches a record in the current store format.
// A descriptor that is unreadable, or says no profile, names no record. The
// owner asks this of its published descriptor; the other-half store asks it
// only of descriptors it admitted, which are readable profiles by then, so the
// two once-separate copies of this check (one without the readability test)
// always agreed.
static inline bool noah_profile_record_matches_descriptor(const noah_profile_store_record_t *record, const noah_profile_split_descriptor_t *descriptor) {
    return record && descriptor && descriptor->readable && descriptor->has_profile && record->slot != NOAH_PROFILE_SLOT_NONE && record->schema_major == descriptor->schema_major && record->schema_minor == descriptor->schema_minor && record->domain_mask == descriptor->domain_mask && record->flags == descriptor->profile_flags && record->payload_length == descriptor->payload_length && record->generation == descriptor->generation && record->origin_half == descriptor->origin_half && record->payload_crc32 == descriptor->payload_crc32 && record->payload_digest == descriptor->payload_digest && record->compiled_default_digest == descriptor->compiled_default_digest && record->action_abi_digest == descriptor->action_abi_digest && record->format_version == NOAH_PROFILE_STORE_FORMAT_VERSION;
}

// The descriptor is exactly the one this staged candidate would publish: a
// readable profile with every field of the candidate's tuple.
static inline bool noah_profile_candidate_matches_descriptor(const noah_profile_store_candidate_t *candidate, const noah_profile_split_descriptor_t *descriptor) {
    return candidate && descriptor && descriptor->readable && descriptor->has_profile && descriptor->generation == candidate->generation && descriptor->payload_crc32 == candidate->payload_crc32 && descriptor->payload_digest == candidate->payload_digest && descriptor->compiled_default_digest == candidate->compiled_default_digest && descriptor->action_abi_digest == candidate->action_abi_digest && descriptor->payload_length == candidate->payload_length && descriptor->schema_major == candidate->schema_major && descriptor->schema_minor == candidate->schema_minor && descriptor->domain_mask == candidate->domain_mask && descriptor->profile_flags == candidate->flags && descriptor->origin_half == candidate->origin_half;
}
