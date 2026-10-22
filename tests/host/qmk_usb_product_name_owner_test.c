// The keyboard name USB serves (D-F25), driven through the real profile owner,
// working copy and settings cache with the authored defaults: USB restarts
// only when the profile's keyboard name changes, never because the profile was
// edited, saved, reverted, adjusted, applied or booted. A restart drops every
// Raw HID request in flight, so a spurious one disconnects the app.
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_usb_product_name.h"
#include "users/noah/lib/profile/runtime/profile_owner.h"
#include "users/noah/lib/profile/runtime/effective_settings_runtime.h"
#include "users/noah/lib/profile/schema/profile_settings_defaults.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_store_runtime.h"
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
void noah_qmk_portable_apply(void) {}

static uint8_t eeprom[NOAH_PROFILE_STORAGE_REGION_SIZE];
static bool    memory_read(void *context, noah_profile_storage_address_t address, uint8_t *target, uint32_t length) {
    (void)context;
    assert(length <= NOAH_PROFILE_STORE_IO_CHUNK_MAX && (size_t)address + length <= sizeof(eeprom));
    memcpy(target, eeprom + address, length);
    return true;
}
static bool memory_write(void *context, noah_profile_storage_address_t address, const uint8_t *source, uint32_t length) {
    (void)context;
    assert(length <= 20 && (size_t)address + length <= sizeof(eeprom));
    memcpy(eeprom + address, source, length);
    return true;
}
static bool quiet(void *context) {
    (void)context;
    return true;
}
// The edit mode, kept as QMK's EEPROM keeps it.
static noah_profile_edit_mode_record_t mode_record = {.counter = 1u, .mode = NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW};
static bool                            mode_load(void *context, noah_profile_edit_mode_record_t *record) {
    (void)context;
    *record = mode_record;
    return true;
}
static bool mode_store(void *context, const noah_profile_edit_mode_record_t *record) {
    (void)context;
    mode_record = *record;
    return true;
}

static noah_profile_owner_t owner;
static uint8_t              working[NOAH_PROFILE_PAYLOAD_MAX];
static uint32_t             now = 1000u;
static unsigned             restarts;

// The USB half, enumerated, with the app silent: every gate but the name is
// open, so any difference between the live and the enumerated name restarts.
bool is_keyboard_master(void) {
    return true;
}
uint32_t timer_read32(void) {
    return now;
}
uint32_t timer_elapsed32(uint32_t last) {
    return now - last;
}
bool noah_qmk_usb_product_name_test_usb_active(void) {
    return true;
}
void noah_qmk_usb_product_name_test_restart(void) {
    restarts++;
}
bool noah_profile_store_runtime_usb_restart_safe(void) {
    return noah_profile_owner_usb_restart_safe(&owner);
}
// The name the saved profile holds, read before the owner starts.
static noah_profile_keyboard_name_t saved_name;
static bool                         saved;
bool                                noah_profile_store_runtime_boot_keyboard_name(noah_profile_keyboard_name_t *name) {
    *name = saved ? saved_name : (noah_profile_keyboard_name_t){0};
    return saved;
}

static void scan(void) {
    noah_profile_owner_scan(&owner, true, now);
    noah_qmk_usb_product_name_task();
    now += 2u;
}
static void scans(unsigned count) {
    for (unsigned i = 0; i < count; i++)
        scan();
}
// Asserts USB serves `text` and has restarted `expected` times in all.
static void serves(const char *text, unsigned expected) {
    const uint8_t *descriptor = noah_qmk_usb_product_name_descriptor();
    size_t         length     = strlen(text);
    if (restarts != expected) fprintf(stderr, "USB restarted %u times, expected %u\n", restarts, expected);
    assert(restarts == expected);
    assert(descriptor && descriptor[0] == 2u + 2u * length && descriptor[1] == 3u);
    for (size_t i = 0; i < length; i++)
        assert(descriptor[2 + 2 * i] == (uint8_t)text[i] && descriptor[3 + 2 * i] == 0u);
}
static void boot(void) {
    noah_profile_owner_config_t config = {
        .store_io                 = {.read = memory_read, .write = memory_write},
        .working_bytes            = working,
        .working_capacity         = sizeof(working),
        .working_session          = 0x5EED0001u,
        .keyboard_quiet           = quiet,
        .edit_mode_storage        = {.load = mode_load, .store = mode_store},
        .settings_defaults_length = noah_profile_settings_defaults_length,
        .settings_defaults_byte   = noah_profile_settings_defaults_byte,
    };
    noah_effective_key_behavior_runtime_uninstall(&owner.key_behaviors);
    noah_effective_rgb_runtime_uninstall(&owner.rgb);
    noah_effective_combo_runtime_uninstall(&owner.combos);
    noah_qmk_usb_product_name_pre_init();
    assert(noah_profile_owner_init(&owner, &config));
    for (unsigned i = 0; i < 1000000u && owner.state != NOAH_PROFILE_OWNER_READY_COMPILED && owner.state != NOAH_PROFILE_OWNER_READY_VALIDATED; i++)
        scan();
    assert(owner.state == NOAH_PROFILE_OWNER_READY_COMPILED || owner.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    scans(2000u);
}
static uint32_t crc_of(const uint8_t *bytes, uint32_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length));
}
static void settled(void) {
    for (unsigned i = 0; i < 100000u && (noah_profile_working_copy_busy(&owner.working) || owner.save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE); i++)
        scan();
    assert(!noah_profile_working_copy_busy(&owner.working) && owner.save.phase == NOAH_PROFILE_OWNER_SAVE_IDLE);
    scans(2000u);
}
// One live edit of a single splice, as the app sends it.
static void live_edit(uint16_t transaction, noah_profile_working_splice_t splice, const uint8_t *bytes) {
    assert(noah_profile_owner_edit_begin(&owner, transaction, owner.working.session, owner.working.revision, 1u, splice.insert, 0u) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_owner_edit_splice(&owner, transaction, 0u, &splice) == NOAH_PROFILE_WORKING_OK);
    for (uint32_t offset = 0u; offset < splice.insert; offset += 22u) {
        uint32_t count = splice.insert - offset < 22u ? splice.insert - offset : 22u;
        assert(noah_profile_owner_edit_data(&owner, transaction, offset, bytes + offset, count) == NOAH_PROFILE_WORKING_OK);
    }
    assert(noah_profile_owner_edit_commit(&owner, transaction, crc_of(bytes, splice.insert)) == NOAH_PROFILE_WORKING_QUEUED);
    settled();
    if (owner.working.last_result != NOAH_PROFILE_WORKING_OK) fprintf(stderr, "edit %u ended %u\n", transaction, owner.working.last_result);
    assert(owner.working.last_result == NOAH_PROFILE_WORKING_OK);
}
static void setting_edit(uint16_t transaction, uint8_t setting, uint32_t value) {
    uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    live_edit(transaction, (noah_profile_working_splice_t){.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .offset = NOAH_SETTINGS_SCALARS_OFFSET + setting * 4u, .remove = 4u, .insert = 4u}, bytes);
}

static void test_the_name_only_changes_when_the_profile_names_it(void) {
    const char *home = NOAH_KEYBOARD_NAME;
    uint16_t    length = noah_profile_settings_defaults_length();

    memset(eeprom, 0xff, sizeof(eeprom));
    saved = false;
    restarts = 0;
    boot();
    assert(owner.state == NOAH_PROFILE_OWNER_READY_COMPILED);
    serves(home, 0u);
    puts("nothing saved: the keyboard enumerates under the compiled name and keeps it");

    // Live edits of settings and of keys: the working copy becomes active.
    setting_edit(1u, NOAH_SETTING_TAPPING_TERM, TAPPING_TERM + 10u);
    serves(home, 0u);
    {
        const uint8_t key[2] = {0x05u, 0x00u};
        live_edit(2u, (noah_profile_working_splice_t){.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = 130u, .remove = 2u, .insert = 2u}, key);
    }
    serves(home, 0u);
    setting_edit(3u, NOAH_SETTING_TAPPING_TERM, TAPPING_TERM + 20u);
    serves(home, 0u);
    puts("live edits of settings and keys keep the name");

    // Keyboard adjustments republish the settings.
    assert(noah_profile_owner_adjust(&owner, NOAH_SETTING_DEFAULT_DPI, 1200u));
    settled();
    serves(home, 0u);
    puts("an adjustment key keeps the name");

    // Revert, then edits and Save.
    assert(noah_profile_owner_revert(&owner, 4u, owner.working.session, owner.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    settled();
    serves(home, 0u);
    setting_edit(5u, NOAH_SETTING_TAPPING_TERM, TAPPING_TERM + 30u);
    assert(noah_profile_owner_save(&owner, 6u, owner.working.session, owner.working.revision) == NOAH_PROFILE_WORKING_QUEUED);
    settled();
    assert(owner.state == NOAH_PROFILE_OWNER_READY_VALIDATED && !owner.working.active);
    serves(home, 0u);
    puts("Revert and Save keep the name");

    // Live: edits over the saved profile and an adjustment, which the
    // keyboard then saves by itself.
    assert(noah_profile_owner_set_edit_mode(&owner, NOAH_PROFILE_EDIT_MODE_LIVE) == NOAH_PROFILE_WORKING_OK);
    scans(100u);
    assert(mode_record.mode == NOAH_PROFILE_EDIT_MODE_LIVE);
    uint32_t saves = owner.save.keyboard_saves;
    setting_edit(7u, NOAH_SETTING_TAPPING_TERM, TAPPING_TERM + 40u);
    assert(noah_profile_owner_adjust(&owner, NOAH_SETTING_DEFAULT_DPI, 1600u));
    settled();
    serves(home, 0u);
    for (unsigned i = 0; i < 200000u && owner.working.active; i++)
        scan();
    settled();
    assert(!owner.working.active && owner.save.keyboard_saves > saves);
    serves(home, 0u);
    puts("Live: the keyboard's own save keeps the name");
    saved = true;
    saved_name.length = (uint8_t)strlen(home);
    memcpy(saved_name.bytes, home, saved_name.length);
    boot();
    assert(owner.state == NOAH_PROFILE_OWNER_READY_VALIDATED);
    serves(home, 0u);
    puts("a replug keeps the name");

    // The control: renaming the keyboard restarts USB once, under the new name.
    {
        const char *desk = "Desk";
        uint8_t     bytes[1 + 4] = {4u, 'D', 'e', 's', 'k'};
        live_edit(8u, (noah_profile_working_splice_t){.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .offset = (uint32_t)(length - 1u - strlen(home)), .remove = (uint32_t)(1u + strlen(home)), .insert = sizeof(bytes)}, bytes);
        serves(desk, 1u);
    }
    puts("renaming the keyboard restarts USB once, under the new name");
}

int main(void) {
    noah_effective_settings_set_apply(noah_qmk_portable_apply);
    test_the_name_only_changes_when_the_profile_names_it();
    return 0;
}
