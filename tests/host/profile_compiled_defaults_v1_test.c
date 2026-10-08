#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/schema/profile_compiled_defaults_v1.h"
#include "users/noah/lib/profile/schema/profile_validator_v1.h"
#include "users/noah/lib/profile/schema/profile_rgb_compiled_v1.h"
#include "users/noah/lib/profile/schema/profile_settings_defaults.h"
#include "users/noah/lib/profile/runtime/profile_action_placement_v1.h"
#include "users/noah/lib/profile/runtime/profile_action_runtime_v1.h"
#include "users/noah/lib/profile/runtime/effective_pd_runtime.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/noah_keymap.h"

enum {
    OUTPUT_CAPACITY = NOAH_PROFILE_BLOB_V1_MAX_SIZE,
};

// Migration tripwire: each userspace family keeps its block, so adding
// pointing slots or layers renumbers nothing.
_Static_assert(CUSTOM_KEY_0 == 0x7e40 && CUSTOM_KEY_63 == 0x7e7f, "preserve custom key identities");
_Static_assert(PD_SLOT_0 == 0x7e80 && PD_SLOT_5 == 0x7e85 && PD_SLOT_6 == 0x7e86 && PD_SLOT_7 == 0x7e87, "preserve PD hold identities");
_Static_assert(PD_SLOT_0_LOCK == 0x7ea0 && PD_SLOT_5_LOCK == 0x7ea5 && PD_SLOT_6_LOCK == 0x7ea6 && PD_SLOT_7_LOCK == 0x7ea7, "preserve PD lock identities");
_Static_assert(LAYER_LOCK_BASE == 0x7ec0 && LOCK_LAYER(7) == 0x7ec7, "preserve layer lock identities");

static uint8_t     output[OUTPUT_CAPACITY];
static size_t      output_length;
static size_t      largest_write;
static const char *fixture_path;
static size_t behavior_visits;
static size_t domain_writes[NOAH_PROFILE_DOMAIN_REGISTRY_COUNT];
void noah_compiled_defaults_test_domain_write(uint8_t id) {
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; i++) if (noah_profile_domain_at(i)->id == id) domain_writes[i]++;
}

void noah_compiled_defaults_test_behavior_visit(void) { behavior_visits++; }

#define NOAH_PD_MODE_TEST_ROW(name, mode_keycode) [PD_MODE_INDEX_##name] = {.mode_flag = PD_MODE_##name, .keycode = (mode_keycode), .lock_action = mode_keycode##_LOCK},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(NOAH_PD_MODE_TEST_ROW)};
#undef NOAH_PD_MODE_TEST_ROW

static bool collect(void *context, const uint8_t *bytes, size_t length) {
    (void)context;
    assert(output_length + length <= sizeof(output));
    memcpy(&output[output_length], bytes, length);
    output_length += length;
    if (length > largest_write) largest_write = length;
    return true;
}

static bool reject_write(void *context, const uint8_t *bytes, size_t length) {
    (void)context;
    (void)bytes;
    (void)length;
    return false;
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u);
}

static bool fixture_value(const char *key, char *value, size_t capacity) {
    FILE  *file = fopen(fixture_path, "r");
    char   line[9000];
    size_t key_length = strlen(key);

    assert(file != NULL);
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strncmp(line, key, key_length) == 0 && line[key_length] == '=') {
            char  *start  = &line[key_length + 1u];
            size_t length = strcspn(start, "\r\n");
            assert(length + 1u <= capacity);
            memcpy(value, start, length);
            value[length] = '\0';
            fclose(file);
            return true;
        }
    }
    fclose(file);
    return false;
}

static uint32_t fixture_u32(const char *key, int base) {
    char          value[32];
    char         *end;
    unsigned long parsed;
    assert(fixture_value(key, value, sizeof(value)));
    parsed = strtoul(value, &end, base);
    assert(*value != '\0' && *end == '\0' && parsed <= UINT32_MAX);
    return (uint32_t)parsed;
}

static uint8_t hex_nibble(char value) {
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    if (value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
    assert(false);
    return 0u;
}

static void assert_golden(const noah_profile_compiled_v1_t *profile) {
    char hex[OUTPUT_CAPACITY * 2u + 1u];
    assert(fixture_path != NULL);
    assert(fixture_value("profile.full.hex", hex, sizeof(hex)));
    assert(strlen(hex) == output_length * 2u);
    for (size_t index = 0u; index < output_length; index++) {
        assert(output[index] == (uint8_t)((hex_nibble(hex[index * 2u]) << 4u) | hex_nibble(hex[index * 2u + 1u])));
    }
    assert(profile->metadata.byte_length == fixture_u32("profile.byte_length", 10));
    assert(profile->metadata.crc32 == fixture_u32("profile.crc32", 16));
    assert(profile->metadata.digest == fixture_u32("profile.fnv1a32", 16));
    assert(profile->metadata.action_abi_digest == fixture_u32("profile.action_abi", 16));
    assert(profile->metadata.action_abi_row_visits == fixture_u32("profile.action_abi_row_visits", 10));
}

static noah_profile_validator_v1_compatibility_t compatibility(uint32_t action_abi_digest) {
    noah_profile_validator_v1_compatibility_t value = noah_profile_validator_v1_default_compatibility(action_abi_digest);
    value.required_domain_mask                      = (NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB | NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    value.logical_layer_count                       = LAYER_COUNT;
    value.supported_pd_mode_mask                    = UINT32_MAX >> (32u - PD_MODE_COUNT);
    value.via_macro_slot_count                      = VIA_MACRO_SLOT_COUNT;
    value.custom_key_count                = NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS;
    value.rgb_limits.logical_layer_count            = LAYER_COUNT;
    value.rgb_limits.supported_pd_mode_mask         = value.supported_pd_mode_mask;
    value.rgb_limits.maximum_brightness             = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
    value.rgb_limits.tap_branch_color_count         = KEY_BEHAVIOR_MAX_TAP_COUNT - 1u;
    value.rgb_limits.compiled_stage_mask            = NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL;
    return value;
}

// Table lookups over the real pd_modes[], standing in for the pointing
// registry so the placement check classifies pd-mode keys as the firmware does.
bool is_pd_mode_lock_action(uint16_t action) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].lock_action == action) return true;
    }
    return false;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return pd_modes[i].mode_flag;
    }
    return 0;
}

typedef struct {
    unsigned layer_holds[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_COMBO_OUTPUT + 1u];
    unsigned refused;
} placement_log_t;

static placement_log_t placement_log;

// The keyboard's check of a profile it is asked to save, logged: the real
// authored profile must pass it, and every MO() hold must reach it as a
// press-and-hold, the only place a momentary layer is allowed.
static bool logged_placement(const noah_profile_action_v1_t *action, noah_profile_validator_v1_placement_t placement) {
    bool supported = noah_profile_action_placement_v1_supported(action, placement);

    if (action->kind == NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY) placement_log.layer_holds[placement]++;
    if (!supported) {
        placement_log.refused++;
        fprintf(stderr, "placement refused: kind=%u operand=0x%04x placement=%u\n", (unsigned)action->kind, (unsigned)action->operand, (unsigned)placement);
    }
    return supported;
}

static void validate_whole_profile(const noah_profile_compiled_v1_t *profile) {
    noah_profile_reader_t                   reader        = noah_profile_compiled_v1_reader(profile);
    noah_profile_reader_t                   copied_reader = reader;
    noah_profile_validator_v1_declaration_t declaration   = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = profile->metadata.domain_mask,
        .byte_length       = profile->metadata.byte_length,
        .crc32             = profile->metadata.crc32,
        .digest            = profile->metadata.digest,
        .action_abi_digest = profile->metadata.action_abi_digest,
    };
    noah_profile_validator_v1_compatibility_t compatible = compatibility(profile->metadata.action_abi_digest);
    noah_profile_validator_v1_t               validator;
    noah_profile_validator_v1_error_t         error;
    noah_profile_validator_v1_result_t        result = noah_profile_validator_v1_begin(&validator, &copied_reader, 0u, &declaration, &compatible, &error);

    assert(result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS);
    for (size_t steps = 0u; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && steps < 2u * NOAH_PROFILE_BLOB_V1_MAX_SIZE; steps++) {
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX, &error);
    }
    if (result != NOAH_PROFILE_VALIDATOR_V1_VALID) {
        fprintf(stderr, "validator failed: code=%u offset=%zu domain=%u table=%u row=%u step=%u field=%u detail=%u\n", (unsigned)result, error.byte_offset, error.domain_id, error.table_id, error.row_index, error.step_index, error.field_id, error.detail_code);
    }
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);

    noah_profile_validator_v1_compatibility_t runtime_compatibility;
    noah_profile_validator_v1_runtime_t       candidate_runtime;

    assert(noah_profile_compiled_v1_compatibility(profile, &runtime_compatibility));
    candidate_runtime = (noah_profile_validator_v1_runtime_t){
        .combo_to_native     = runtime_compatibility.runtime ? runtime_compatibility.runtime->combo_to_native : NULL,
        .placement_supported = logged_placement,
    };
    compatible.runtime = &candidate_runtime;
    memset(&placement_log, 0, sizeof(placement_log));
    copied_reader = reader;
    result        = noah_profile_validator_v1_begin(&validator, &copied_reader, 0u, &declaration, &compatible, &error);
    for (size_t steps = 0u; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && steps < 2u * NOAH_PROFILE_BLOB_V1_MAX_SIZE; steps++) {
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX, &error);
    }
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(placement_log.refused == 0u);
    assert(placement_log.layer_holds[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_PRESS_AND_HOLD] > 0u);
    assert(placement_log.layer_holds[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_HOLD_OTHER] == 0u);
    assert(placement_log.layer_holds[NOAH_PROFILE_VALIDATOR_V1_PLACEMENT_BEHAVIOR_TAP] == 0u);
}

static void test_real_authored_profile(void) {
    noah_profile_compiled_v1_t                profile;
    noah_profile_validator_v1_compatibility_t runtime_compatibility;
    noah_profile_compiled_v1_error_t          error;
    noah_profile_blob_v1_t                    decoded;
    noah_profile_codec_v1_error_t             codec_error;
    noah_profile_reader_t                     reader;
    noah_profile_reader_t                     copied_reader;

    uint8_t slice[37];

    assert(noah_profile_compiled_v1_open(NULL, &error) == NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT);
    assert(noah_profile_compiled_v1_open(&profile, &error) == NOAH_PROFILE_COMPILED_V1_OK);
    assert(profile.metadata.domain_mask == NOAH_PROFILE_DOMAIN_MASK_ALL);
    assert(profile.metadata.byte_length > NOAH_PROFILE_BLOB_V1_HEADER_SIZE);
    assert(profile.metadata.byte_length <= NOAH_PROFILE_BLOB_V1_MAX_SIZE);
    assert(profile.metadata.crc32 != 0u);
    assert(profile.metadata.digest != 0u);
    assert(profile.metadata.action_abi_digest != 0u);
    assert(profile.metadata.action_abi_row_visits == 0);
    assert(profile.metadata.action_abi_row_visits <= NOAH_PROFILE_COMPILED_V1_ACTION_ABI_ROW_VISITS_MAX);
    assert(sizeof(profile) <= 40u);
    assert(!noah_profile_compiled_v1_compatibility(NULL, &runtime_compatibility));
    assert(!noah_profile_compiled_v1_compatibility(&profile, NULL));
    assert(noah_profile_compiled_v1_compatibility(&profile, &runtime_compatibility));
    assert(runtime_compatibility.required_domain_mask == NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD);
    uint8_t allowed_domains = profile.metadata.domain_mask;
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    allowed_domains |= NOAH_PROFILE_VALIDATOR_V1_DOMAIN_SETTINGS;
#endif
#ifdef COMBO_ENABLE
    allowed_domains |= NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS;
    assert(runtime_compatibility.runtime != NULL && runtime_compatibility.runtime->combo_to_native != NULL);
#endif
    assert(runtime_compatibility.allowed_domain_mask == allowed_domains);
    assert(runtime_compatibility.action_abi_digest == profile.metadata.action_abi_digest);
    assert(runtime_compatibility.logical_layer_count == LAYER_COUNT);
    assert(PD_MODE_COUNT == 32 && runtime_compatibility.supported_pd_mode_mask == UINT32_MAX);
    assert(runtime_compatibility.via_macro_slot_count == VIA_MACRO_SLOT_COUNT);
    assert(runtime_compatibility.custom_key_count == NOAH_PROFILE_ACTION_V1_MAX_CUSTOM_KEYS);
    assert(runtime_compatibility.rgb_limits.logical_layer_count == LAYER_COUNT);
    assert(runtime_compatibility.rgb_limits.maximum_brightness == RGB_MATRIX_MAXIMUM_BRIGHTNESS);
    assert(runtime_compatibility.rgb_limits.compiled_stage_mask == NOAH_PROFILE_RGB_V1_STAGE_MASK_ALL);

    output_length = 0u;
    largest_write = 0u;
    assert(noah_profile_compiled_v1_write(NULL, collect, NULL, &error) == NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT);
    assert(noah_profile_compiled_v1_write(&profile, NULL, NULL, &error) == NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT);
    assert(noah_profile_compiled_v1_write(&profile, reject_write, NULL, &error) == NOAH_PROFILE_COMPILED_V1_WRITE_ERROR);
    assert(noah_profile_compiled_v1_write(&profile, collect, NULL, &error) == NOAH_PROFILE_COMPILED_V1_OK);
    assert(output_length == profile.metadata.byte_length);
    assert(largest_write <= NOAH_PROFILE_RGB_V1_HEADER_SIZE);
    assert(noah_profile_blob_v1_decode(output, output_length, &decoded, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    assert(decoded.domain_count == NOAH_PROFILE_DOMAIN_REGISTRY_COUNT);
    assert(decoded.domains[0].id == NOAH_PROFILE_DOMAIN_V1_RGB);
    assert(decoded.domains[1].id == NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS);
    assert(decoded.crc32 == profile.metadata.crc32);
    assert(decoded.digest == profile.metadata.digest);
    assert(decoded.domains[1].payload[0] == key_behavior_count);
    assert(read_u16(&output[10]) == decoded.domains[0].payload_length);
    assert_golden(&profile);

    reader        = noah_profile_compiled_v1_reader(&profile);
    copied_reader = reader;
    assert((unsigned)NOAH_PROFILE_COMPILED_V1_READER_REPLAY_MAX == (unsigned)NOAH_PROFILE_BLOB_V1_MAX_SIZE);
    assert(noah_profile_reader_read(&copied_reader, 0u, slice, sizeof(slice)));
    assert(memcmp(slice, output, sizeof(slice)) == 0);
    // The profile is open, so its canonical behaviour order is known: reading
    // every chunk, as a host does, sorts no behaviour row again.
    behavior_visits = 0;
    for (size_t offset = 0u; offset < output_length; offset += sizeof(slice)) {
        size_t length = output_length - offset;
        if (length > sizeof(slice)) length = sizeof(slice);
        memset(slice, 0, sizeof(slice));
        assert(noah_profile_reader_read(&reader, offset, slice, length));
        assert(memcmp(slice, &output[offset], length) == 0);
    }
    assert(behavior_visits == 0);
    for (size_t i = 0; i < decoded.domain_count; i++) {
        const noah_profile_domain_v1_t *domain = &decoded.domains[i];
        memset(domain_writes, 0, sizeof(domain_writes));
        size_t length = domain->payload_length < sizeof(slice) ? domain->payload_length : sizeof(slice);
        assert(noah_profile_reader_read(&reader, (size_t)(domain->payload - output), slice, length));
        assert(!memcmp(slice, domain->payload, length));
        for (size_t j = 0; j < decoded.domain_count; j++) assert(domain_writes[j] == (i == j ? 1u : 0u));
    }
    // Exercise every envelope and payload seam with byte-granular windows.
    for (size_t offset = 0; offset < output_length; offset++) {
        size_t length = output_length - offset < sizeof(slice) ? output_length - offset : sizeof(slice);
        assert(noah_profile_reader_read(&reader, offset, slice, length));
        assert(!memcmp(slice, output + offset, length));
    }
    assert(!noah_profile_reader_read(&reader, output_length - 1u, slice, 2u));
#ifdef NOAH_PD_PROFILE_ENABLE
    // Boot and compiled fallback must warm the real PD cache without replaying
    // canonical behavior sorting for every 20-byte read in the same scan.
    noah_effective_profile_snapshot_t snapshot = {.reader = reader};
    snapshot.profile.domain_mask = NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD;
    snapshot.profile.pd.offset = (size_t)(decoded.domains[4].payload - output);
    snapshot.profile.pd.length = decoded.domains[4].payload_length;
    behavior_visits = 0;
    noah_effective_pd_invalidate(NULL, 0, snapshot.identity, snapshot.identity, &snapshot);
    assert(noah_effective_pd_ready());
    // The sparse domain stores the authored slots that say something; the
    // cache holds all 32, omitted ones disabled with an empty name.
    const uint8_t *pd_payload = decoded.domains[4].payload;
    assert(decoded.domains[4].version == NOAH_PROFILE_PD_V1_VERSION);
    assert(pd_payload[0] == 2 && pd_payload[1] == 32 && pd_payload[2] == 96 && decoded.domains[4].payload_length == 8u + 96u * pd_payload[3]);
    uint8_t stored = 0;
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) {
        const uint8_t *record = noah_effective_pd_record(slot);
        assert(record && record[0] == slot);
        if (stored < pd_payload[3] && pd_payload[8 + stored * 96] == slot) {
            assert(memcmp(record, pd_payload + 8 + stored * 96, 96) == 0);
            stored++;
        } else {
            for (size_t i = 1; i < 96; i++) assert(record[i] == 0);
        }
    }
    assert(stored == pd_payload[3]);
    // Slots 0..6 carry the authored presets; slot 7 and slots 8..31 are empty.
    assert(pd_payload[3] == 7 && noah_effective_pd_record(6)[1] == 1 && noah_effective_pd_record(7)[1] == 0);
    fprintf(stderr, "compiled PD cache warmup: %zu behavior-row sorts\n", behavior_visits);
    assert(behavior_visits == 0);
    // Exercise every PD byte and record boundary against the full golden stream.
    for (size_t offset = snapshot.profile.pd.offset; offset < output_length; offset++) {
        size_t length = output_length - offset;
        if (length > sizeof(slice)) length = sizeof(slice);
        assert(noah_profile_reader_read(&reader, offset, slice, length));
        assert(memcmp(slice, output + offset, length) == 0);
    }
    assert(behavior_visits == 0);
#endif
    noah_profile_rgb_v1_view_t cached_rgb;
    memset(domain_writes, 0, sizeof(domain_writes));
    assert(noah_profile_rgb_compiled_v1_view());
    cached_rgb = *noah_profile_rgb_compiled_v1_view();
    assert(domain_writes[0] == 1);
    assert(cached_rgb.byte_length == decoded.domains[0].payload_length);
    for (size_t offset = 0; offset < cached_rgb.byte_length; offset += sizeof(slice)) {
        size_t length = cached_rgb.byte_length - offset < sizeof(slice) ? cached_rgb.byte_length - offset : sizeof(slice);
        assert(noah_profile_reader_read(&cached_rgb.reader, cached_rgb.base_offset + offset, slice, length));
        assert(!memcmp(slice, decoded.domains[0].payload + offset, length));
    }
    memset(domain_writes, 0, sizeof(domain_writes));
    for (size_t i = 0; i < 100; i++) {
        noah_profile_rgb_v1_layer_color_t color;
        assert(noah_profile_rgb_compiled_v1_view());
    cached_rgb = *noah_profile_rgb_compiled_v1_view();
        assert(noah_profile_rgb_v1_layer_color_at(&cached_rgb, 0, &color, NULL) == NOAH_PROFILE_RGB_V1_OK);
    }
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; i++) assert(domain_writes[i] == 0);
    assert(noah_profile_settings_default(NOAH_SETTING_DEFAULT_DPI) == CHARYBDIS_MINIMUM_DEFAULT_DPI);
    assert(noah_profile_settings_default(NOAH_SETTING_SNIPING_DPI) == CHARYBDIS_MINIMUM_SNIPING_DPI);
    assert(noah_profile_settings_default(NOAH_SETTING_KEYMAP_OPTIONS) == 0x1400);
    validate_whole_profile(&profile);

    printf("compiled profile v1: %u bytes crc32=%08x fnv1a=%08x action_abi=%08x\n", profile.metadata.byte_length, (unsigned)profile.metadata.crc32, (unsigned)profile.metadata.digest, (unsigned)profile.metadata.action_abi_digest);
}

static void test_semantic_action_translation(void) {
    noah_profile_action_v1_t action;
    uint16_t                 native;

    assert(noah_profile_compiled_v1_action(KC_A, NULL) == NOAH_PROFILE_COMPILED_V1_INVALID_ARGUMENT);
    assert(noah_profile_compiled_v1_action(KC_NO, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_NONE);
    assert(noah_profile_compiled_v1_action(KC_A, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && action.operand == KC_A);
    assert(noah_profile_compiled_v1_action(MO(LAYER_NAV), &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_LAYER_MOMENTARY && action.operand == LAYER_NAV);
    assert(noah_profile_compiled_v1_action(LOCK_LAYER(LAYER_SYM), &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_LAYER_LOCK && action.operand == LAYER_SYM);
    assert(noah_profile_compiled_v1_action(VIA_MACRO_10, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_VIA_MACRO && action.operand == 10u);
    assert(noah_profile_compiled_v1_action(CUSTOM_KEY_7, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_CUSTOM_KEY && action.operand == 7u);
    assert(noah_profile_compiled_v1_action(pd_modes[PD_MODE_INDEX_ZOOM].keycode, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY && action.operand == PD_MODE_INDEX_ZOOM);
    assert(noah_profile_compiled_v1_action(pd_modes[PD_MODE_INDEX_ARROW].lock_action, &action) == NOAH_PROFILE_COMPILED_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK && action.operand == PD_MODE_INDEX_ARROW);

    const uint16_t round_trip_actions[] = {
        KC_NO, KC_A, MO(LAYER_NAV), LOCK_LAYER(LAYER_SYM), VIA_MACRO_10, CUSTOM_KEY_7, CUSTOM_KEY_63, pd_modes[PD_MODE_INDEX_ZOOM].keycode, pd_modes[PD_MODE_INDEX_ARROW].lock_action,
    };
    for (size_t index = 0u; index < ARRAY_SIZE(round_trip_actions); index++) {
        assert(noah_profile_action_runtime_v1_from_native(round_trip_actions[index], &action) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
        assert(noah_profile_action_runtime_v1_to_native(&action, &native) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
        assert(native == round_trip_actions[index]);
    }
    // Every slot's hold and lock keycode reaches that slot and comes back, and
    // the codes either side of both blocks are not pointing keys.
    for (uint8_t slot = 0u; slot < PD_MODE_COUNT; slot++) {
        assert(noah_profile_action_runtime_v1_from_native(pd_modes[slot].keycode, &action) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_PD_MODE_MOMENTARY && action.operand == slot);
        assert(noah_profile_action_runtime_v1_to_native(&action, &native) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK && native == pd_modes[slot].keycode);
        assert(noah_profile_action_runtime_v1_from_native(pd_modes[slot].lock_action, &action) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_PD_MODE_LOCK && action.operand == slot);
        assert(noah_profile_action_runtime_v1_to_native(&action, &native) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK && native == pd_modes[slot].lock_action);
    }
    assert(noah_profile_action_runtime_v1_from_native(CUSTOM_KEY_63, &action) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_CUSTOM_KEY);
    assert(noah_profile_action_runtime_v1_from_native(LOCK_LAYER(0), &action) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK && action.kind == NOAH_PROFILE_ACTION_V1_LAYER_LOCK && action.operand == 0u);
    action = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_LAYER_LOCK, .operand = LAYER_COUNT};
    assert(noah_profile_action_runtime_v1_to_native(&action, &native) == NOAH_PROFILE_ACTION_RUNTIME_V1_UNSUPPORTED && native == KC_NO);
    action = (noah_profile_action_v1_t){.kind = NOAH_PROFILE_ACTION_V1_QMK_KEYCODE, .flags = 1u, .operand = KC_A};
    assert(noah_profile_action_runtime_v1_to_native(&action, &native) == NOAH_PROFILE_ACTION_RUNTIME_V1_INVALID_ARGUMENT);
}

static void validate_portable_import(const noah_profile_compiled_v1_t *compiled, const char *path, bool expect_valid) {
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t length = fread(output, 1, sizeof(output), file);
    assert(feof(file));
    fclose(file);
    noah_profile_reader_t                     reader = noah_profile_reader_from_memory(output, length);
    noah_profile_validator_v1_compatibility_t compatible;
    assert(noah_profile_compiled_v1_compatibility(compiled, &compatible));
    // The app's profile is a candidate, so it is held to where its actions are
    // placed, as the keyboard holds it when asked to save it.
    noah_profile_validator_v1_runtime_t candidate_runtime = {
        .combo_to_native     = compatible.runtime ? compatible.runtime->combo_to_native : NULL,
        .placement_supported = logged_placement,
    };
    compatible.runtime = &candidate_runtime;
    memset(&placement_log, 0, sizeof(placement_log));
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = 0,
        .domain_mask       = NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS,
        .byte_length       = length,
        .crc32             = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, output, length)),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, output, length),
        .action_abi_digest = compiled->metadata.action_abi_digest,
    };
    noah_profile_validator_v1_t        validator;
    noah_profile_validator_v1_error_t  error;
    noah_profile_validator_v1_result_t result = noah_profile_validator_v1_begin(&validator, &reader, 0, &declaration, &compatible, &error);
    for (size_t step = 0; result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS && step < 5000; step++)
        result = noah_profile_validator_v1_step(&validator, 20, &error);
    if (!expect_valid) {
        // An eight-slot backup (RGB v2, PD v1) is refused at the first of
        // those domains; Ark translates it before it reaches the keyboard.
        assert(result == NOAH_PROFILE_VALIDATOR_V1_INVALID_DOMAIN && error.domain_id == NOAH_PROFILE_DOMAIN_V1_RGB);
        return;
    }
    if (result != NOAH_PROFILE_VALIDATOR_V1_VALID) fprintf(stderr, "portable import failed: %u domain=%u field=%u byte=%zu\n", result, error.domain_id, error.field_id, error.byte_offset);
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(placement_log.refused == 0u);
    assert(validator.profile.domain_mask == NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS);
    assert(validator.profile.settings.length >= 344);
#ifdef NOAH_PD_PROFILE_ENABLE
    assert(validator.profile.pd.length >= NOAH_PROFILE_PD_V1_HEADER_SIZE && (validator.profile.pd.length - NOAH_PROFILE_PD_V1_HEADER_SIZE) % NOAH_PROFILE_PD_V1_RECORD_SIZE == 0u);
#endif
}

int main(int argc, char **argv) {
    assert(argc >= 2 && argc <= 4);
    if (argc == 3 && strcmp(argv[2], "--write-fixture") == 0) {
        noah_profile_compiled_v1_t profile;
        assert(noah_profile_compiled_v1_open(&profile, NULL) == NOAH_PROFILE_COMPILED_V1_OK);
        assert(noah_profile_compiled_v1_write(&profile, collect, NULL, NULL) == NOAH_PROFILE_COMPILED_V1_OK);
        FILE *file = fopen(argv[1], "w");
        assert(file);
        fprintf(file, "profile.byte_length=%u\nprofile.crc32=%08x\nprofile.fnv1a32=%08x\nprofile.action_abi=%08x\nprofile.action_abi_row_visits=%u\nprofile.full.hex=", profile.metadata.byte_length, (unsigned)profile.metadata.crc32, (unsigned)profile.metadata.digest, (unsigned)profile.metadata.action_abi_digest, profile.metadata.action_abi_row_visits);
        for (size_t i = 0; i < output_length; i++)
            fprintf(file, "%02x", output[i]);
        fputc('\n', file);
        fclose(file);
        return 0;
    }
    if (argc == 4 && (strcmp(argv[2], "--empty-profile") == 0 || strcmp(argv[2], "--import-profile") == 0 || strcmp(argv[2], "--reject-profile") == 0)) {
        noah_profile_compiled_v1_t profile;
        fixture_path = argv[1];
        if (strcmp(argv[2], "--empty-profile") == 0) assert(key_behavior_count == 0 && noah_combo_count == 0);
        assert(noah_profile_compiled_v1_open(&profile, NULL) == NOAH_PROFILE_COMPILED_V1_OK);
        assert(profile.metadata.action_abi_digest == fixture_u32("profile.action_abi", 16));
        if (strcmp(argv[2], "--reject-profile") == 0) {
            validate_portable_import(&profile, argv[3], false);
            puts("firmware refuses an untranslated eight-slot profile");
            return 0;
        }
        validate_portable_import(&profile, argv[3], true);
        puts("firmware accepts the app's complete populated profile and preserves its action ABI");
        return 0;
    }
    fixture_path = argv[1];
    test_semantic_action_translation();
    test_real_authored_profile();
    puts("compiled profile defaults v1 host tests passed");
    return 0;
}
