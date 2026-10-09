#pragma once

// Profile Wire capabilities exactly as this keyboard reports them. The VIA
// profile channel and the host contract probe (tests/host/contract_probe.c)
// both build them from here, so the contract a client is checked against cannot
// drift from the firmware. Include after the Profile Wire, storage-layout and
// candidate headers and the keyboard's config.

#ifndef VIA_FIRMWARE_VERSION
#    define VIA_FIRMWARE_VERSION 0u
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

#ifdef NOAH_LIVE_PROFILE_MUTATION_ENABLE
#    define NOAH_PROFILE_MUTATION_CAPABILITIES (NOAH_PROFILE_FEATURE_CANDIDATE_WRITE | NOAH_PROFILE_FEATURE_PERSISTENT_COMMIT | NOAH_PROFILE_FEATURE_RUNTIME_ACTIVATION | NOAH_PROFILE_FEATURE_PEER_RECONCILIATION | NOAH_PROFILE_FEATURE_ATOMIC_LOGICAL_APPLY)
#    define NOAH_PROFILE_MUTATION_CHUNK_MAX NOAH_PROFILE_CANDIDATE_V1_CHUNK_MAX
#else
#    define NOAH_PROFILE_MUTATION_CAPABILITIES 0u
#    define NOAH_PROFILE_MUTATION_CHUNK_MAX 0u
#endif

// Before the live-profile owner reports: read-only, digests unknown.
#define NOAH_PROFILE_CHANNEL_READ_ONLY_FEATURES (NOAH_PROFILE_FEATURE_READ_SURFACE | NOAH_PROFILE_FEATURE_STORAGE_LAYOUT | NOAH_PROFILE_SPLIT_CAPABILITY | NOAH_PROFILE_FEATURE_OWNED_LAYER_TOGGLES | NOAH_PROFILE_FEATURE_BEHAVIOR_QMK_FUNCTIONS | NOAH_PROFILE_FEATURE_CUSTOM_KEYS | NOAH_PROFILE_FEATURE_PHYSICAL_GESTURE_TIMING | NOAH_PROFILE_FEATURE_OWNED_TAPPING | NOAH_PROFILE_FEATURE_WIDE_PAGES)

// Once the owner reports its compiled defaults.
#define NOAH_PROFILE_CHANNEL_OWNER_FEATURES (NOAH_PROFILE_FEATURE_READ_SURFACE | NOAH_PROFILE_FEATURE_STORAGE_LAYOUT | NOAH_PROFILE_SPLIT_CAPABILITY | NOAH_PROFILE_FEATURE_RGB_SCHEMA | NOAH_PROFILE_FEATURE_KEY_BEHAVIOR_SCHEMA | NOAH_PROFILE_FEATURE_ACTION_ABI_DIGEST | NOAH_PROFILE_FEATURE_COMPILED_PROFILE_HASH | NOAH_PROFILE_MUTATION_CAPABILITIES | NOAH_PROFILE_FEATURE_OWNED_LAYER_TOGGLES | NOAH_PROFILE_FEATURE_BEHAVIOR_QMK_FUNCTIONS | NOAH_PROFILE_FEATURE_CUSTOM_KEYS | NOAH_PROFILE_FEATURE_PHYSICAL_GESTURE_TIMING | NOAH_PROFILE_FEATURE_OWNED_TAPPING | NOAH_PROFILE_FEATURE_WIDE_PAGES)

#define NOAH_PROFILE_CHANNEL_BASE_CAPABILITIES                                                                                                                                                                                                                                                                           \
    {                                                                                                                                                                                                                                                                                                                    \
        .protocol_major               = 1u,                                                                                                                                                                                                                                                                              \
        .protocol_minor               = 0u,                                                                                                                                                                                                                                                                              \
        .schema_major                 = NOAH_PROFILE_SCHEMA_MAJOR,                                                                                                                                                                                                                                                       \
        .schema_minor                 = 0u, /* Stage 01 is intentionally read-only. Candidate capacity and domain */ /* support remain zero until the corresponding decoders and activation */ /* paths exist; the frozen schema ceilings below are declaration */ /* metadata, not a claim that writes are accepted. */ \
        .candidate_chunk_max          = 0u,                                                                                                                                                                                                                                                                              \
        .feature_flags                = NOAH_PROFILE_CHANNEL_READ_ONLY_FEATURES,                                                                                                                                                                                                                                         \
        .action_abi_digest            = 0u,                                                                                                                                                                                                                                                                              \
        .firmware_version             = VIA_FIRMWARE_VERSION,                                                                                                                                                                                                                                                            \
        .compiled_default_digest      = 0u,                                                                                                                                                                                                                                                                              \
        .compiled_layer_count         = DYNAMIC_KEYMAP_LAYER_COUNT,                                                                                                                                                                                                                                                      \
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
        .via_macro_slots              = DYNAMIC_KEYMAP_MACRO_COUNT,                                                                                                                                                                                                                                                      \
        .max_profile_payload          = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX,                                                                                                                                                                                                                                           \
        .profile_slot_payload         = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX,                                                                                                                                                                                                                                           \
        .profile_slot_size            = NOAH_PROFILE_STORAGE_SLOT_A_SIZE,                                                                                                                                                                                                                                                \
        .via_macro_bytes              = NOAH_PROFILE_STORAGE_VIA_MACRO_SIZE,                                                                                                                                                                                                                                             \
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
