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
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++)
        if (pd_modes[i].lock_action == action) return true;
    return false;
}
pd_mode_mask_t pd_mode_for_keycode(uint16_t key) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++)
        if (pd_modes[i].keycode == key) return pd_modes[i].mode_flag;
    return 0;
}

// Hardware eligibility and the independent VIA persistence adapter are the
// injected seams. Domain writing, validation, storage and caches are real.
void noah_profile_activation_policy_init(noah_profile_activation_policy_t *policy, noah_profile_activation_peer_observer_fn observer, void *context) {
    memset(policy, 0, sizeof(*policy));
    policy->peer_observer = observer;
    policy->peer_context  = context;
    policy->initialized   = true;
}
uint32_t noah_profile_activation_policy_safe_boundary(void *context) {
    (void)context;
    return 0;
}
static unsigned settings_applies, via_accepts, via_boots;
void            noah_qmk_portable_apply(void) {
    settings_applies++;
}
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
static void via_release(void *context) {
    (void)context;
}
static const noah_profile_logical_via_ops_t via = {
    .ready        = via_command,
    .accept       = via_accept,
    .abort        = via_command,
    .converged    = via_converged,
    .boot_recover = via_boot,
    .boot_release = via_release,
};

static uint8_t  eeprom[NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE];
static uint32_t writes;
// Power is lost before write number cut_at (counted from zero): it and every
// later write never reach storage. UINT32_MAX never cuts.
static uint32_t cut_at = UINT32_MAX;
static bool     power_lost(void) {
    return writes > cut_at;
}
static bool    memory_read(void *context, noah_profile_storage_address_t address, uint8_t *target, uint16_t length) {
    (void)context;
    assert(length <= 32 && (size_t)address + length <= sizeof(eeprom));
    memcpy(target, eeprom + address, length);
    return true;
}
static bool memory_write(void *context, noah_profile_storage_address_t address, const uint8_t *source, uint16_t length) {
    (void)context;
    assert(length <= 20 && (size_t)address + length <= sizeof(eeprom));
    if (writes++ >= cut_at) return true;
    memcpy(eeprom + address, source, length);
    return true;
}
static noah_profile_owner_t owner;
static uint32_t             now;
static void                 tick(void) {
    noah_profile_owner_scan(&owner, true, now++);
}
static uint32_t state_scans;
static void     until_state(noah_profile_owner_state_t state) {
    for (state_scans = 0; state_scans < 1000000u && owner.state != state; state_scans++)
        tick();
    if (owner.state != state) fprintf(stderr, "owner=%u expected=%u candidate=%u error=%u\n", owner.state, state, owner.host_transaction.status.state, owner.host_transaction.status.error.code);
    assert(owner.state == state);
}
static void initialize(void) {
    noah_profile_owner_config_t config = {.store_io = {.read = memory_read, .write = memory_write}, .logical_via = &via};
    assert(noah_profile_owner_init(&owner, &config));
}
static uint8_t compiled_bytes[NOAH_PROFILE_PAYLOAD_MAX], imported[NOAH_PROFILE_PAYLOAD_MAX], candidate[NOAH_PROFILE_PAYLOAD_MAX];
static size_t  collected;
static bool    collect(void *context, const uint8_t *bytes, size_t length) {
    (void)context;
    assert(collected + length <= sizeof(compiled_bytes));
    memcpy(compiled_bytes + collected, bytes, length);
    collected += length;
    return true;
}
static void u16(uint8_t *p, uint16_t value) {
    p[0] = value;
    p[1] = value >> 8;
}
static void u32(uint8_t *p, uint32_t value) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = value >> (8 * i);
}
static uint32_t crc(const uint8_t *bytes, size_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length));
}
static uint32_t digest(const uint8_t *bytes, size_t length) {
    return noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, length);
}
static void frame_header(uint8_t *frame, uint8_t value) {
    memset(frame, 0, 32);
    frame[0] = value == NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT ? NOAH_PROFILE_CANDIDATE_V1_COMMAND_SAVE : NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = value;
    u16(frame + 3, 17);
}
static void send(uint8_t frame[32]) {
    assert(noah_profile_owner_receive(&owner, frame, 32));
    for (unsigned i = 0; i < 100 && owner.host_transaction.mailbox.pending; i++)
        tick();
    assert(!owner.host_transaction.mailbox.pending);
}
// Sends the candidate and asks for validation; returns its final state.
static uint32_t validation_scans;
static uint8_t submit(size_t length) {
    uint8_t frame[32];
    frame_header(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN);
    frame[5] = NOAH_PROFILE_SCHEMA_MAJOR;
    frame[7] = NOAH_PROFILE_DOMAIN_MASK_ALL;
    u16(frame + 9, (uint16_t)length);
    u32(frame + 11, crc(candidate, length));
    u32(frame + 15, digest(candidate, length));
    u32(frame + 19, owner.compiled.metadata.action_abi_digest);
    frame[23] = NOAH_PROFILE_LOGICAL_STORE_VERSION;
    u32(frame + 24, 6);
    u32(frame + 28, 0xabcdef01u);
    send(frame);
    for (size_t offset = 0; offset < length;) {
        frame_header(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK);
        uint8_t count = length - offset < 20 ? (uint8_t)(length - offset) : 20;
        u16(frame + 5, (uint16_t)offset);
        frame[7] = count;
        memcpy(frame + 8, candidate + offset, count);
        send(frame);
        offset += count;
    }
    frame_header(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE);
    send(frame);
    validation_scans = 0;
    while (validation_scans < 1000000u && owner.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED && owner.host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED) {
        tick();
        validation_scans++;
    }
    return owner.host_transaction.status.state;
}
// Commits a validated candidate. Returns false if power was lost first; the
// owner is then abandoned mid-commit, as a keyboard losing power would be.
static bool commit(void) {
    uint8_t  frame[32];
    uint32_t prior = owner.store.committed.generation;
    frame_header(frame, NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT);
    assert(noah_profile_owner_receive(&owner, frame, 32));
    for (unsigned i = 0; i < 1000000u && !(owner.state == NOAH_PROFILE_OWNER_READY_VALIDATED && owner.store.committed.generation != prior && !owner.host_transaction.mailbox.pending) && !power_lost(); i++)
        tick();
    if (power_lost()) return false;
    assert(owner.state == NOAH_PROFILE_OWNER_READY_VALIDATED && owner.store.committed.generation == prior + 1u);
    return true;
}
static void save(size_t length) {
    uint8_t state = submit(length);
    if (state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED) {
        const noah_profile_candidate_v1_error_t *e = &owner.host_transaction.status.error;
        fprintf(stderr, "candidate state %u error %u offset %u domain %02x table %u row %u tap %u field %u\n", state, e->code, (unsigned)e->byte_offset, e->domain_id, e->table_id, e->row_index, e->tap_index, e->field_id);
    }
    assert(state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    assert(commit());
}
// expected is decoded from base, so a later candidate edit cannot change it.
static void assert_published(const uint8_t *base, const noah_profile_blob_v1_t *expected, uint32_t generation) {
    noah_effective_profile_snapshot_t active;
    assert(noah_effective_profile_provider_copy_active(&owner.provider, &active) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(active.profile.domain_mask == NOAH_PROFILE_DOMAIN_MASK_ALL && active.profile.domain_count == NOAH_PROFILE_DOMAIN_REGISTRY_COUNT);
    assert(active.identity.generation == generation && active.identity.payload_digest == expected->digest);
    // Check every domain through the published owner reader, not store internals.
    static uint8_t bytes[NOAH_PROFILE_PAYLOAD_MAX];
    for (size_t i = 0; i < expected->domain_count; i++) {
        const noah_profile_domain_v1_t *domain = &expected->domains[i];
        size_t                          offset = (size_t)(domain->payload - base);
        assert(domain->payload_length <= sizeof(bytes));
        for (size_t done = 0; done < domain->payload_length; done += 20) {
            size_t count = domain->payload_length - done;
            if (count > 20) count = 20;
            assert(noah_effective_profile_snapshot_read(&active, offset + done, bytes + done, count));
        }
        for (size_t j = 0; j < domain->payload_length; j++)
            if (bytes[j] != domain->payload[j]) {
                fprintf(stderr, "domain %02x byte %u published %02x expected %02x\n", domain->id, (unsigned)j, bytes[j], domain->payload[j]);
                break;
            }
        assert(memcmp(bytes, domain->payload, domain->payload_length) == 0);
        if (domain->id == NOAH_PROFILE_DOMAIN_V1_SETTINGS) {
            assert(noah_effective_settings_length() == domain->payload_length);
            for (size_t j = 0; j < domain->payload_length; j++)
                assert(noah_effective_settings_byte(j) == domain->payload[j]);
        }
    }
    // A stored slot publishes its record; the profiles here omit only slots
    // whose compiled default is disabled and unnamed.
    const noah_profile_domain_v1_t *pd = NULL;
    for (size_t i = 0; i < expected->domain_count; i++)
        if (expected->domains[i].id == NOAH_PROFILE_DOMAIN_V1_PD) pd = &expected->domains[i];
    assert(pd);
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_COUNT; slot++) {
        uint8_t        record[NOAH_PROFILE_PD_V1_RECORD_SIZE];
        const uint8_t *stored = NULL;
        noah_profile_pd_v1_encode_record(&noah_pd_defaults[slot], record);
        for (size_t at = NOAH_PROFILE_PD_V1_HEADER_SIZE; at < pd->payload_length; at += NOAH_PROFILE_PD_V1_RECORD_SIZE)
            if (pd->payload[at] == slot) stored = pd->payload + at;
        assert(noah_effective_pd_record(slot) && memcmp(noah_effective_pd_record(slot), stored ? stored : record, NOAH_PROFILE_PD_V1_RECORD_SIZE) == 0);
    }
    noah_effective_rgb_frame_t rgb;
    assert(noah_effective_rgb_runtime_capture_frame(&owner.rgb, &rgb) == NOAH_EFFECTIVE_RGB_OK && rgb.live && rgb.valid);
    assert(rgb.identity.payload_digest == expected->digest);
    noah_effective_key_behavior_snapshot_t behaviors;
    assert(noah_effective_key_behavior_runtime_status(&owner.key_behaviors, &behaviors) == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK && behaviors.live && behaviors.valid);
    assert(behaviors.identity.payload_digest == expected->digest && behaviors.live && behaviors.indexed);
    for (size_t i = 0; i < expected->domain_count; i++)
        if (expected->domains[i].id == NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS) assert(behaviors.domain.row_count == expected->domains[i].payload[0]);
    noah_profile_domain_range_t combos;
    assert(noah_profile_blob_v1_find_domain(&active.reader, active.base_offset, active.profile.byte_length, NOAH_PROFILE_DOMAIN_V1_COMBOS, &combos));
    noah_profile_combo_v1_header_t combo_header;
    assert(noah_profile_combo_v1_read_header(&active.reader, active.base_offset, combos, &combo_header));
    assert(noah_effective_combo_valid() && owner.combos.live && owner.combos.count == combo_header.row_count);
    for (uint8_t i = 0; i < owner.combos.count; i++) {
        noah_profile_combo_v1_row_t row;
        uint16_t                    output;
        assert(noah_profile_combo_v1_read_row(&active.reader, active.base_offset, combos, i, &row));
        assert(noah_profile_action_runtime_v1_to_native(&row.output, &output) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
        assert(noah_effective_combo_get(i)->keycode == output);
        assert(noah_effective_combo_allowed_layers(i) == row.allowed_layers);
        for (uint8_t j = 0; j < row.input_count; j++) {
            uint16_t input;
            assert(noah_profile_action_runtime_v1_to_native(&row.inputs[j], &input) == NOAH_PROFILE_ACTION_RUNTIME_V1_OK);
            assert(noah_effective_combo_get(i)->keys[j] == input);
        }
        assert(noah_effective_combo_get(i)->keys[row.input_count] == COMBO_END);
    }
}
static void uninstall(void) {
    noah_effective_key_behavior_runtime_uninstall(&owner.key_behaviors);
    noah_effective_rgb_runtime_uninstall(&owner.rgb);
    noah_effective_combo_runtime_uninstall(&owner.combos);
}
static void reboot(void) {
    uninstall();
    initialize();
}

// ─── The maximum profile (D-F14) ────────────────────────────────────────────
//
// tests/fixtures/maximum_profile_v3.fixture holds every table at its
// simultaneous maximum (tests/host/make_maximum_profile.py). It must validate,
// commit, boot and publish whole; mutated or truncated copies must be refused
// without touching the active generation; and a save cut by lost power must
// boot either the prior generation or the new one, never a mixture.
static uint8_t maximum[NOAH_PROFILE_PAYLOAD_MAX];
static size_t  maximum_length;
static void    load_maximum(const char *path) {
    static char line[2 * NOAH_PROFILE_PAYLOAD_MAX + 64];
    FILE       *file = fopen(path, "r");
    assert(file);
    while (fgets(line, sizeof(line), file))
        if (strncmp(line, "profile.hex=", 12) == 0)
            for (const char *hex = line + 12; hex[0] && hex[0] != '\n'; hex += 2) {
                unsigned byte;
                assert(maximum_length < sizeof(maximum) && sscanf(hex, "%2x", &byte) == 1);
                maximum[maximum_length++] = (uint8_t)byte;
            }
    fclose(file);
    assert(maximum_length > 32768u && maximum_length + 12288u <= NOAH_PROFILE_PAYLOAD_MAX);
}
static void use_maximum(noah_profile_blob_v1_t *expected) {
    memcpy(candidate, maximum, maximum_length);
    assert(noah_profile_blob_v1_decode(maximum, maximum_length, expected, NULL) == NOAH_PROFILE_CODEC_V1_OK);
    assert(expected->domain_count == NOAH_PROFILE_DOMAIN_REGISTRY_COUNT);
}
// The edges the plan names, read through the published runtimes.
static void assert_maximum_edges(void) {
    static const uint8_t custom_keys[] = {0, 63, 64, 127};
    for (size_t i = 0; i < sizeof(custom_keys); i++) {
        uint8_t                           slot = custom_keys[i];
        noah_effective_key_behavior_row_t row;
        key_behavior_step_t               step;
        assert(noah_effective_key_behavior_runtime_lookup(&owner.key_behaviors, CUSTOM_KEY_0 + slot, &row) == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK);
        assert(row.row_index == slot && row.step_count == KEY_BEHAVIOR_MAX_TAP_COUNT && row.tap_hold_term == 150 + slot);
        assert(row.allowed_layers == (slot % 3 ? 0xffffu : (1u << 7) | (1u << 8) | (1u << 15)));
        assert(noah_effective_key_behavior_runtime_step(&owner.key_behaviors, row.epoch, row.row_index, KEY_BEHAVIOR_MAX_TAP_COUNT, &step) == NOAH_EFFECTIVE_KEY_BEHAVIOR_OK);
        assert(step.tap.present && step.tap.action == 0x04 + (slot + 4) % 0x60);
        assert(step.hold.present && step.hold.action == MO(2));
        assert(step.long_hold.present && step.long_hold.action == QK_MACRO_0 + (uint16_t)((uint8_t[]){0, 63, 64, 127}[(slot + 4) % 4]));
    }
    noah_effective_key_behavior_row_t none;
    assert(noah_effective_key_behavior_runtime_lookup(&owner.key_behaviors, CUSTOM_KEY_0 + 128, &none) == NOAH_EFFECTIVE_KEY_BEHAVIOR_NOT_FOUND);
    static const uint8_t combo_rows[] = {31, 32, 63, 64, 127};
    assert(noah_effective_combo_count() == 128);
    for (size_t i = 0; i < sizeof(combo_rows); i++) {
        uint8_t  index = combo_rows[i];
        combo_t *combo = noah_effective_combo_get(index);
        assert(combo && combo->keys[15] == 0x04 + (3 * index + 15) % 0x60 && combo->keys[16] == COMBO_END);
    }
    assert(noah_effective_combo_get(0)->keycode == QK_MACRO_0 && noah_effective_combo_get(63)->keycode == QK_MACRO_0 + 63 && noah_effective_combo_get(127)->keycode == CUSTOM_KEY_0 + 0);
    assert(noah_effective_combo_get(1)->keycode == CUSTOM_KEY_0 + 126 && !noah_effective_combo_get(128));
    assert(noah_setting(NOAH_SETTING_AUTO_MOUSE_LAYER, 0) == 15 && noah_setting(NOAH_SETTING_LAYER_COMBOS, 0) == 0x7fff);
}

static void put_u16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}
static size_t domain_offset(uint8_t id) {
    for (size_t at = 8; at < maximum_length; at += 4u + (size_t)(maximum[at + 2] | maximum[at + 3] << 8))
        if (maximum[at] == id) return at + 4u;
    assert(!"domain missing");
    return 0;
}
typedef struct {
    const char *what;
    size_t      offset;
    uint8_t     value;
} mutation_t;

static void test_maximum_profile(void) {
    noah_profile_blob_v1_t expected;
    settings_applies = via_accepts = via_boots = 0;
    memset(eeprom, 0xff, sizeof(eeprom));
    initialize();
    until_state(NOAH_PROFILE_OWNER_READY_COMPILED);
    use_maximum(&expected);
    save(maximum_length);
    printf("maximum profile: %u bytes validated in %u scans\n", (unsigned)maximum_length, (unsigned)validation_scans);
    // One validator step a scan, each one read of at most 20 bytes. The peer's
    // copy of this validation is the longest stretch a split prepare spends
    // without transfer progress, inside the owner's no-progress window.
    assert(validation_scans < 8000u);
    assert_published(maximum, &expected, 1);
    assert_maximum_edges();
    reboot();
    until_state(NOAH_PROFILE_OWNER_READY_VALIDATED);
    printf("maximum profile: booted in %u scans\n", (unsigned)state_scans);
    assert(via_boots == 1);
    assert_published(maximum, &expected, 1);
    assert_maximum_edges();

    // Every over-limit or malformed edit is refused whole; the active
    // generation and what it publishes stay as they were.
    size_t     rgb = domain_offset(0x10), behaviors = domain_offset(0x20), combos = domain_offset(0x30), settings = domain_offset(0x40), pd = domain_offset(0x50);
    size_t     first_combo = combos + 8, names = settings + 404;
    mutation_t mutations[] = {
        {"seventeen layer colours", rgb + 5, 17},
        {"a layer-colour row for layer 16", rgb + 16 + 9 * 16 + 5 * 15, 16},
        {"a 129th behaviour row", behaviors, 129},
        {"641 behaviour steps", behaviors + 2, 0x81},
        {"a sixth step", behaviors + 4 + 2 + 15, 6},
        {"behaviour layer bit 16", behaviors + 4 + 2 + 12 + 2, 1},
        {"a 129th combo", combos, 129},
        {"a seventeenth combo input", first_combo, 17},
        {"combo layer bit 16", first_combo + 6, 1},
        {"custom key 128 as a combo output", first_combo + 12 + 76 + 10, 0},
        {"macro 128 as a combo output", first_combo + 10, 0},
        {"a 33-byte layer name", names, 33},
        {"a name cut inside a character", names + 32, 0xc3},
        {"layer behaviours bit 16", settings + 8 + 4 * 29 + 2, 1},
        {"a combo reference layer of 16", settings + 132, 16},
        {"a placement bit past position 59", settings + 132 + 8, 0x10},
        {"a 33-byte pointing-slot name", pd + 8 + 8, 33},
    };
    for (size_t i = 0; i < sizeof(mutations) / sizeof(mutations[0]); i++) {
        memcpy(candidate, maximum, maximum_length);
        if (mutations[i].offset == first_combo + 12 + 76 + 10 || mutations[i].offset == first_combo + 10) {
            // Row 1 outputs custom key 126 and row 0 macro 0; make them 128.
            put_u16(candidate + mutations[i].offset, 128);
        } else {
            candidate[mutations[i].offset] = mutations[i].value;
        }
        uint8_t state = submit(maximum_length);
        if (state != NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED) fprintf(stderr, "maximum profile with %s was not refused (state %u)\n", mutations[i].what, state);
        assert(state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
        assert(owner.store.committed.generation == 1);
        assert_published(maximum, &expected, 1);
    }
    // A truncated profile: its declared length cuts the last domain short.
    memcpy(candidate, maximum, maximum_length);
    assert(submit(maximum_length - 1) == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED);
    assert(owner.store.committed.generation == 1);
    assert_published(maximum, &expected, 1);
    puts("maximum profile: every table at its maximum saved, booted and published whole; over-limit, malformed and truncated copies refused");
    uninstall();
}

// Lost power at sampled writes of a maximum save over a committed profile,
// at every write of its commit, and just after its last: the next boot
// publishes the prior generation or the new one, whole, and the new one only
// once its commit has begun.
static void test_maximum_save_interrupted(void) {
    static uint8_t         prior_eeprom[sizeof(eeprom)];
    noah_profile_blob_v1_t expected;
    uint32_t               total;

    memset(eeprom, 0xff, sizeof(eeprom));
    initialize();
    until_state(NOAH_PROFILE_OWNER_READY_COMPILED);
    use_maximum(&expected);
    save(maximum_length);
    uninstall();
    memcpy(prior_eeprom, eeprom, sizeof(eeprom));

    // An edited maximum profile, saved over the first.
    static uint8_t         edited_bytes[NOAH_PROFILE_PAYLOAD_MAX];
    noah_profile_blob_v1_t edited;
    memcpy(edited_bytes, maximum, maximum_length);
    edited_bytes[domain_offset(0x40) + 8 + 4 * 0] ^= 1u; // tapping term 200 -> 201
    assert(noah_profile_blob_v1_decode(edited_bytes, maximum_length, &edited, NULL) == NOAH_PROFILE_CODEC_V1_OK);

    // A clean run counts the writes and finds where the commit decision begins.
    memcpy(candidate, edited_bytes, maximum_length);
    initialize();
    until_state(NOAH_PROFILE_OWNER_READY_VALIDATED);
    writes = 0;
    assert(submit(maximum_length) == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED);
    uint32_t staged = writes;
    assert(commit());
    total = writes;
    uninstall();
    assert(total > staged);

    unsigned prior_boots = 0, new_boots = 0;
    // cut == total loses power just after the last write.
    for (uint32_t cut = 0; cut <= total; cut = cut + 97u < staged ? cut + 97u : cut < staged ? staged : cut + 1u) {
        memcpy(eeprom, prior_eeprom, sizeof(eeprom));
        memcpy(candidate, edited_bytes, maximum_length);
        initialize();
        until_state(NOAH_PROFILE_OWNER_READY_VALIDATED);
        writes = 0;
        cut_at = cut;
        if (submit(maximum_length) == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED) commit();
        cut_at = UINT32_MAX;
        reboot();
        until_state(NOAH_PROFILE_OWNER_READY_VALIDATED);
        if (owner.store.committed.generation == 1) {
            assert_published(maximum, &expected, 1);
            prior_boots++;
        } else {
            assert(owner.store.committed.generation == 2);
            assert(cut >= staged);
            assert_published(edited_bytes, &edited, 2);
            new_boots++;
        }
        uninstall();
    }
    assert(prior_boots > 0 && new_boots > 0);
    printf("maximum profile: power lost at %u points of a %u-write save booted the prior generation %u times and the new one %u times\n", prior_boots + new_boots, (unsigned)total, prior_boots, new_boots);
}

int main(int argc, char **argv) {
    noah_effective_settings_set_apply(noah_qmk_portable_apply);
    assert(argc == 3);
    load_maximum(argv[2]);
    test_maximum_profile();
    test_maximum_save_interrupted();
    for (unsigned use_imported = 0; use_imported < 2; use_imported++) {
        settings_applies = via_accepts = via_boots = collected = 0;
        memset(eeprom, 0xff, sizeof(eeprom));
        initialize();
        until_state(NOAH_PROFILE_OWNER_READY_COMPILED);
        assert(owner.compiled.metadata.domain_mask == NOAH_PROFILE_DOMAIN_MASK_ALL);
        assert(owner.combos.valid && owner.combos.live && owner.combos.count == noah_combo_count);
        for (uint16_t i = 0; i < noah_combo_count; i++) {
            combo_t *cached = noah_effective_combo_get(i);
            assert(cached && cached->keycode == key_combos[i].keycode);
            assert(noah_effective_combo_follows_default(i) == (noah_combo_terms[i] == 0));
            assert(noah_effective_combo_term(i) == (noah_combo_terms[i] ? noah_combo_terms[i] : COMBO_TERM));
            for (uint8_t j = 0; j < 5; j++) {
                uint16_t authored = pgm_read_word(&key_combos[i].keys[j]);
                assert(cached->keys[j] == authored);
                if (authored == COMBO_END) break;
            }
        }
        assert(noah_effective_settings_length() > 0 && noah_setting(NOAH_SETTING_TAPPING_TERM, 0) == TAPPING_TERM);
        noah_effective_rgb_frame_t factory_rgb;
        assert(noah_effective_rgb_runtime_capture_frame(&owner.rgb, &factory_rgb) == NOAH_EFFECTIVE_RGB_OK);
        assert(factory_rgb.identity.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS && factory_rgb.view.reader.read != owner.compiled_reader.read);
        assert(noah_profile_compiled_v1_write(&owner.compiled, collect, NULL, NULL) == NOAH_PROFILE_COMPILED_V1_OK);
        noah_profile_blob_v1_t compiled, fixture, expected;
        assert(noah_profile_blob_v1_decode(compiled_bytes, collected, &compiled, NULL) == NOAH_PROFILE_CODEC_V1_OK);
        FILE *file = fopen(argv[1], "rb");
        assert(file);
        size_t imported_length = fread(imported, 1, sizeof(imported), file);
        assert(!ferror(file));
        fclose(file);
        assert(noah_profile_blob_v1_decode(imported, imported_length, &fixture, NULL) == NOAH_PROFILE_CODEC_V1_OK);
        noah_profile_domain_v1_t domains[NOAH_PROFILE_DOMAIN_REGISTRY_COUNT];
        for (size_t i = 0; i < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; i++) {
            const noah_profile_domain_shape_t *shape  = noah_profile_domain_at(i);
            bool                               found  = false;
            const noah_profile_blob_v1_t      *source = use_imported && (shape->id == NOAH_PROFILE_DOMAIN_V1_COMBOS || shape->id == NOAH_PROFILE_DOMAIN_V1_SETTINGS) ? &fixture : &compiled;
            for (size_t j = 0; j < source->domain_count; j++)
                if (source->domains[j].id == shape->id) {
                    domains[i] = source->domains[j];
                    found      = true;
                }
            assert(found && domains[i].version == shape->version);
        }
        size_t length;
        assert(noah_profile_blob_v1_encode(domains, NOAH_PROFILE_DOMAIN_REGISTRY_COUNT, candidate, sizeof(candidate), &length, NULL) == NOAH_PROFILE_CODEC_V1_OK);
        assert(noah_profile_blob_v1_decode(candidate, length, &expected, NULL) == NOAH_PROFILE_CODEC_V1_OK);
        save(length);
        assert(owner.store.committed.via_generation == 6 && owner.store.committed.via_digest == 0xabcdef01u);
        assert_published(candidate, &expected, 1);
        reboot();
        until_state(NOAH_PROFILE_OWNER_READY_VALIDATED);
        assert(via_boots == 1 && settings_applies == 2);
        assert_published(candidate, &expected, 1);
        puts(use_imported ? "all current domains: populated imported combos/settings saved, booted and published through the owner" : "all current domains: complete compiled defaults saved, booted and published through the owner");
        uninstall();
    }
    return 0;
}
