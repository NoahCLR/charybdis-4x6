#pragma once

// Profile Wire capabilities exactly as this keyboard reports them. The Raw
// HID profile channel and the host contract probe (tests/host/contract_probe.c)
// both build them from here, so the contract a client is checked against cannot
// drift from the firmware. Include after the Profile Wire, storage-layout and
// candidate headers and the keyboard's config.

#include "../profile/schema/profile_macros_v1.h"

#ifndef NOAH_FIRMWARE_VERSION
#    define NOAH_FIRMWARE_VERSION 0u
#endif

#ifdef SPLIT_KEYBOARD
#    define NOAH_PROFILE_SPLIT_CAPABILITY NOAH_PROFILE_FEATURE_SPLIT_KEYBOARD
#else
#    define NOAH_PROFILE_SPLIT_CAPABILITY 0u
#endif

#ifdef RGB_MATRIX_ENABLE
#    define NOAH_PROFILE_LED_COUNT RGB_MATRIX_LED_COUNT
#else
#    define NOAH_PROFILE_LED_COUNT 0u
#endif

#ifdef POINTING_DEVICE_ENABLE
#    define NOAH_PROFILE_POINTING_CAPABILITY NOAH_PROFILE_FEATURE_POINTING
#else
#    define NOAH_PROFILE_POINTING_CAPABILITY 0u
#endif

#ifdef NOAH_LIVE_PROFILE_MUTATION_ENABLE
#    define NOAH_PROFILE_MUTATION_CAPABILITIES NOAH_PROFILE_FEATURE_SAVES
#    define NOAH_PROFILE_MUTATION_CHUNK_MAX NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX
#else
#    define NOAH_PROFILE_MUTATION_CAPABILITIES 0u
#    define NOAH_PROFILE_MUTATION_CHUNK_MAX 0u
#endif

// The feature word holds only what this build varies by (D-F28). Before the
// live-profile owner reports, nothing can be saved yet.
#define NOAH_PROFILE_CHANNEL_READ_ONLY_FEATURES (NOAH_PROFILE_SPLIT_CAPABILITY | NOAH_PROFILE_POINTING_CAPABILITY)

// Once the owner reports its compiled defaults.
#define NOAH_PROFILE_CHANNEL_OWNER_FEATURES (NOAH_PROFILE_CHANNEL_READ_ONLY_FEATURES | NOAH_PROFILE_MUTATION_CAPABILITIES)

#define NOAH_PROFILE_CHANNEL_BASE_CAPABILITIES                                                                                                                                                                                                                                                                           \
    {                                                                                                                                                                                                                                                                                                                    \
        .protocol_major               = NOAH_PROFILE_WIRE_V1_PROTOCOL_MAJOR,                                                                                                                                                                                                                                                              \
        .protocol_minor               = NOAH_PROFILE_WIRE_V1_PROTOCOL_MINOR,                                                                                                                                                                                                                                                              \
        .schema_major                 = NOAH_PROFILE_SCHEMA_MAJOR,                                                                                                                                                                                                                                                       \
        .schema_minor                 = NOAH_PROFILE_SCHEMA_MINOR,                                                                                                                                                                                                                                                       \
        .candidate_chunk_max          = 0u,                                                                                                                                                                                                                                                                              \
        .feature_flags                = NOAH_PROFILE_CHANNEL_READ_ONLY_FEATURES,                                                                                                                                                                                                                                         \
        .action_abi_digest            = 0u,                                                                                                                                                                                                                                                                              \
        .firmware_version             = NOAH_FIRMWARE_VERSION,                                                                                                                                                                                                                                                             \
        .compiled_default_digest      = 0u,                                                                                                                                                                                                                                                                              \
        .compiled_layer_count         = LAYER_COUNT,                                                                                                                                                                                                                                                                     \
        .max_logical_layers           = NOAH_PROFILE_WIRE_V1_MAX_LOGICAL_LAYERS,                                                                                                                                                                                                                                         \
        .max_behavior_rows            = NOAH_PROFILE_WIRE_V1_MAX_BEHAVIOR_ROWS,                                                                                                                                                                                                                                          \
        .max_tap_steps_per_behavior   = NOAH_PROFILE_WIRE_V1_MAX_TAP_STEPS_PER_BEHAVIOR,                                                                                                                                                                                                                                 \
        .max_populated_behavior_steps = NOAH_PROFILE_WIRE_V1_MAX_POPULATED_BEHAVIOR_STEPS,                                                                                                                                                                                                                               \
        .max_combos                   = NOAH_PROFILE_WIRE_V1_MAX_COMBOS,                                                                                                                                                                                                                                                 \
        .max_keys_per_combo           = NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO,                                                                                                                                                                                                                                         \
        .max_reusable_rgb_groups      = NOAH_PROFILE_WIRE_V1_MAX_REUSABLE_RGB_GROUPS,                                                                                                                                                                                                                                    \
        .max_rgb_stage_group_rows     = NOAH_PROFILE_WIRE_V1_MAX_RGB_STAGE_GROUP_ROWS,                                                                                                                                                                                                                                   \
        .physical_led_count           = NOAH_PROFILE_LED_COUNT,                                                                                                                                                                                                                                                          \
        .led_bitmap_size              = NOAH_PROFILE_WIRE_V1_LED_BITMAP_SIZE,                                                                                                                                                                                                                                            \
        .custom_key_slots             = NOAH_PROFILE_WIRE_V1_MAX_CUSTOM_KEYS,                                                                                                                                                                                                                                            \
        .macro_slots                  = NOAH_PROFILE_MACROS_V1_SLOTS,                                                                                                                                                                                                                                                     \
        .max_profile_payload          = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX,                                                                                                                                                                                                                                           \
        .profile_slot_payload         = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX,                                                                                                                                                                                                                                           \
        .profile_slot_size            = NOAH_PROFILE_STORAGE_SLOT_A_SIZE,                                                                                                                                                                                                                                                \
        .macro_bytes                  = NOAH_PROFILE_MACROS_V1_MAX_SIZE,                                                                                                                                                                                                                                                 \
        .supported_domain_mask        = 0u,                                                                                                                                                                                                                                                                              \
        .max_name_bytes               = NOAH_PROFILE_WIRE_V1_MAX_NAME_BYTES,                                                                                                                                                                                                                                             \
        .layer_mask_bits              = NOAH_PROFILE_WIRE_V1_LAYER_MASK_BITS,                                                                                                                                                                                                                                            \
        .placement_positions          = MATRIX_ROWS * MATRIX_COLS,                                                                                                                                                                                                                                                       \
    }

static inline void noah_profile_channel_apply_owner_capabilities(noah_profile_wire_v1_capabilities_t *capabilities, uint32_t action_abi_digest, uint32_t compiled_default_digest, uint8_t supported_domain_mask) {
    capabilities->feature_flags           = NOAH_PROFILE_CHANNEL_OWNER_FEATURES;
    capabilities->action_abi_digest       = action_abi_digest;
    capabilities->compiled_default_digest = compiled_default_digest;
    capabilities->supported_domain_mask   = supported_domain_mask;
    capabilities->candidate_chunk_max     = NOAH_PROFILE_MUTATION_CHUNK_MAX;
}
