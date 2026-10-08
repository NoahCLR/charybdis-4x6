#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/runtime/profile_owner.h"
#include "users/noah/lib/profile/runtime/profile_action_runtime_v1.h"
#include "users/noah/lib/profile/runtime/effective_pd_runtime.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/noah_keymap.h"

#define PD_ROW(name, key) [PD_MODE_INDEX_##name] = {.mode_flag = PD_MODE_##name, .keycode = key, .lock_action = key##_LOCK},
const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {NOAH_PD_MODE_LIST(PD_ROW)};
#undef PD_ROW

bool is_pd_mode_lock_action(uint16_t action) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) if (pd_modes[i].lock_action == action) return true;
    return false;
}
pd_mode_mask_t pd_mode_for_keycode(uint16_t key) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) if (pd_modes[i].keycode == key) return pd_modes[i].mode_flag;
    return 0;
}

// Hardware eligibility and the independent VIA persistence adapter are the
// injected seams. Domain writing, validation, storage and caches are real.
void noah_profile_activation_policy_init(noah_profile_activation_policy_t *policy, noah_profile_activation_peer_observer_fn observer, void *context) {
    memset(policy, 0, sizeof(*policy));
    policy->peer_observer = observer;
    policy->peer_context = context;
    policy->initialized = true;
}
uint32_t noah_profile_activation_policy_safe_boundary(void *context) { (void)context; return 0; }
static unsigned settings_applies, via_accepts, via_boots;
void noah_qmk_portable_apply(void) { settings_applies++; }
static bool via_command(void *context, uint16_t transaction, uint32_t generation, uint32_t digest) {
    (void)context;
    assert(transaction && generation == 6 && digest == 0xabcdef01u);
    return true;
}
static bool via_accept(void *context, uint16_t transaction, uint32_t generation, uint32_t digest) {
    via_accepts++;
    return via_command(context, transaction, generation, digest);
}
static bool via_converged(void *context, uint32_t generation, uint32_t digest) {
    (void)context;
    assert(generation == 6 && digest == 0xabcdef01u);
    return true;
}
static bool via_boot(void *context, uint32_t generation, uint32_t digest) {
    via_boots++;
    return via_converged(context, generation, digest);
}
static void via_release(void *context) { (void)context; }
static const noah_profile_logical_via_ops_t via = {
    .ready = via_command, .accept = via_accept, .abort = via_command,
    .converged = via_converged, .boot_recover = via_boot, .boot_release = via_release,
};

static uint8_t eeprom[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
static bool memory_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    (void)context;
    assert(length <= 32 && (size_t)address + length <= sizeof(eeprom));
    memcpy(target, eeprom + address, length);
    return true;
}
static bool memory_write(void *context, uint16_t address, const uint8_t *source, uint16_t length) {
    (void)context;
    assert(length <= 20 && (size_t)address + length <= sizeof(eeprom));
    memcpy(eeprom + address, source, length);
    return true;
}
static noah_profile_owner_t owner;
static uint32_t now;
static void tick(void) { noah_profile_owner_scan(&owner, true, now++); }
static void until_state(noah_profile_owner_state_t state) {
    for (unsigned i = 0; i < 10000 && owner.state != state; i++) tick();
    if (owner.state != state) fprintf(stderr, "owner=%u expected=%u candidate=%u error=%u\n", owner.state, state, owner.host_transaction.status.state, owner.host_transaction.status.error.code);
    assert(owner.state == state);
}
static void initialize(void) {
    noah_profile_owner_config_t config = {.store_io = {.read = memory_read, .write = memory_write}, .logical_via = &via};
    assert(noah_profile_owner_init(&owner, &config));
}
static uint8_t compiled_bytes[NOAH_PROFILE_PAYLOAD_MAX], imported[NOAH_PROFILE_PAYLOAD_MAX], candidate[NOAH_PROFILE_PAYLOAD_MAX];
static size_t collected;
static bool collect(void *context, const uint8_t *bytes, size_t length) {
    (void)context;
    assert(collected + length <= sizeof(compiled_bytes));
    memcpy(compiled_bytes + collected, bytes, length);
    collected += length;
    return true;
}
static void u16(uint8_t *p, uint16_t value) { p[0] = value; p[1] = value >> 8; }
static void u32(uint8_t *p, uint32_t value) { for (unsigned i = 0; i < 4; i++) p[i] = value >> (8 * i); }
static uint32_t crc(const uint8_t *bytes, size_t length) { return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length)); }
static uint32_t digest(const uint8_t *bytes, size_t length) { return noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, length); }
static void frame_header(uint8_t *frame, uint8_t value) {
    memset(frame, 0, 32);
    frame[0] = value == NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT ? NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE : NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = value;
    u16(frame + 3, 17);
}
static void send(uint8_t frame[32]) {
    assert(noah_profile_owner_receive(&owner, frame, 32));
    for (unsigned i = 0; i < 100 && owner.host_transaction.mailbox.pending; i++) tick();
    assert(!owner.host_transaction.mailbox.pending);
}
static void save(size_t length) {
    uint8_t frame[32];
    frame_header(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN);
    frame[5] = NOAH_PROFILE_SCHEMA_MAJOR;
    frame[7] = NOAH_PROFILE_DOMAIN_MASK_ALL;
    u16(frame + 9, (uint16_t)length);
    u32(frame + 11, crc(candidate, length));
    u32(frame + 15, digest(candidate, length));
    u32(frame + 19, owner.compiled.metadata.action_abi_digest);
    frame[23] = NOAH_PROFILE_LOGICAL_STORE_VERSION;
    u32(frame + 24, 6); u32(frame + 28, 0xabcdef01u);
    send(frame);
    for (size_t offset = 0; offset < length;) {
        frame_header(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK);
        uint8_t count = length - offset < 20 ? (uint8_t)(length - offset) : 20;
        u16(frame + 5, (uint16_t)offset); frame[7] = count;
        memcpy(frame + 8, candidate + offset, count);
        send(frame);
        offset += count;
    }
    frame_header(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE); send(frame);
    for (unsigned i = 0; i < 10000 && owner.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED; i++) tick();
    assert(owner.host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    frame_header(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT); send(frame);
    until_state(NOAH_PROFILE_OWNER_READY_VALIDATED);
}
static void assert_published(const noah_profile_blob_v1_t *expected) {
    noah_effective_profile_snapshot_t active;
    assert(noah_effective_profile_provider_copy_active(&owner.provider, &active) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(active.profile.domain_mask == NOAH_PROFILE_DOMAIN_MASK_ALL && active.profile.domain_count == NOAH_PROFILE_DOMAIN_REGISTRY_COUNT);
    assert(active.identity.generation == 1 && active.identity.payload_digest == expected->digest);
    // Check every domain through the published owner reader, not store internals.
    uint8_t bytes[NOAH_PROFILE_PD_V1_MAX_SIZE];
    for (size_t i = 0; i < expected->domain_count; i++) {
        const noah_profile_domain_v1_t *domain = &expected->domains[i];
        size_t offset = (size_t)(domain->payload - candidate);
        assert(domain->payload_length <= sizeof(bytes));
        for (size_t done = 0; done < domain->payload_length; done += 20) {
            size_t count = domain->payload_length - done;
            if (count > 20) count = 20;
            assert(noah_effective_profile_snapshot_read(&active, offset + done, bytes + done, count));
        }
        assert(memcmp(bytes, domain->payload, domain->payload_length) == 0);
        if (domain->id == NOAH_PROFILE_DOMAIN_V1_SETTINGS) {
            assert(noah_effective_settings_length() == domain->payload_length);
            for (size_t j = 0; j < domain->payload_length; j++) assert(noah_effective_settings_byte(j) == domain->payload[j]);
        }
    }
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_COUNT; slot++) {
        uint8_t record[96];
        noah_profile_pd_v1_encode_record(&noah_pd_defaults[slot], record);
        assert(noah_effective_pd_record(slot) && memcmp(noah_effective_pd_record(slot), record, 96) == 0);
    }
    noah_effective_rgb_frame_t rgb;
    assert(noah_effective_rgb_runtime_capture_frame(&owner.rgb, &rgb) == NOAH_EFFECTIVE_RGB_OK && rgb.live && rgb.valid);
    assert(rgb.identity.payload_digest == expected->digest);
    noah_effective_key_behavior_snapshot_t behaviors;
    assert(noah_effective_key_behavior_runtime_status(&owner.key_behaviors, &behaviors) == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK && behaviors.live && behaviors.valid);
    assert(behaviors.identity.payload_digest == expected->digest && behaviors.domain.row_count == key_behavior_count);
    assert(noah_effective_combo_valid() && owner.combos.live && owner.combos.count == active.profile.combos.row_count);
    for (uint8_t i = 0; i < owner.combos.count; i++) {
        noah_profile_combo_v1_row_t row;
        uint16_t output;
        assert(noah_profile_combo_v1_read_row(&active.reader, active.base_offset, &active.profile.combos, i, &row));
        assert(noah_profile_action_runtime_v1_to_native(&row.output, &output) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
        assert(noah_effective_combo_get(i)->keycode == output);
    }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    memset(eeprom, 0xff, sizeof(eeprom));
    initialize(); until_state(NOAH_PROFILE_OWNER_READY_COMPILED);
    assert(owner.compiled.metadata.domain_mask == (NOAH_PROFILE_DOMAIN_MASK_RGB | NOAH_PROFILE_DOMAIN_MASK_KEY_BEHAVIORS | NOAH_PROFILE_DOMAIN_MASK_PD));
    assert(noah_profile_compiled_v1_write(&owner.compiled, collect, NULL, NULL) == NOAH_PROFILE_COMPILED_V1_OK);
    noah_profile_blob_v1_t compiled, fixture, expected;
    assert(noah_profile_blob_v1_decode(compiled_bytes, collected, &compiled, NULL) == NOAH_PROFILE_CODEC_V1_OK);
    FILE *file = fopen(argv[1], "rb"); assert(file);
    size_t imported_length = fread(imported, 1, sizeof(imported), file); assert(!ferror(file)); fclose(file);
    assert(noah_profile_blob_v1_decode(imported, imported_length, &fixture, NULL) == NOAH_PROFILE_CODEC_V1_OK);
    noah_profile_domain_v1_t domains[NOAH_PROFILE_DOMAIN_REGISTRY_COUNT];
    for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; i++) {
        const noah_profile_domain_shape_t *shape = noah_profile_domain_at(i);
        bool found = false;
        const noah_profile_blob_v1_t *source = shape->id == NOAH_PROFILE_DOMAIN_V1_COMBOS || shape->id == NOAH_PROFILE_DOMAIN_V1_SETTINGS ? &fixture : &compiled;
        for (size_t j = 0; j < source->domain_count; j++) if (source->domains[j].id == shape->id) { domains[i] = source->domains[j]; found = true; }
        assert(found && domains[i].version == shape->version);
    }
    size_t length;
    assert(noah_profile_blob_v1_encode(domains, NOAH_PROFILE_DOMAIN_REGISTRY_COUNT, candidate, sizeof(candidate), &length, NULL) == NOAH_PROFILE_CODEC_V1_OK);
    assert(noah_profile_blob_v1_decode(candidate, length, &expected, NULL) == NOAH_PROFILE_CODEC_V1_OK);
    save(length);
    assert(owner.store.committed.via_generation == 6 && owner.store.committed.via_digest == 0xabcdef01u);
    assert_published(&expected);
    noah_effective_key_behavior_runtime_uninstall(&owner.key_behaviors);
    noah_effective_rgb_runtime_uninstall(&owner.rgb);
    noah_effective_combo_runtime_uninstall(&owner.combos);
    initialize(); until_state(NOAH_PROFILE_OWNER_READY_VALIDATED);
    assert(via_boots == 1 && settings_applies == 2); assert_published(&expected);
    puts("all current domains: real compiled content + imported combos/settings saved, booted and published through the owner");
    return 0;
}
