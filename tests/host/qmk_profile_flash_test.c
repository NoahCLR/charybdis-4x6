// QMK flash adapter (D-F29): the shared backend instance, erases only on a
// quiet keyboard, the EEPROM reset that forgets the saved profiles even when
// a boot is interrupted, the flash pause read on Profile Wire (custom get
// 0x0D) and the USB half's pull of the other half's figures over the split
// link.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include QMK_KEYBOARD_H
#include "transactions.h"
#include "users/noah/lib/compat/qmk_physical_half.h"
#include "users/noah/lib/compat/qmk_profile_flash.h"
#include "users/noah/lib/compat/qmk_profile_flash_hooks.h"
#include "users/noah/lib/profile/protocol/profile_wire_v1.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

static uint8_t          region[NOAH_PROFILE_STORAGE_REGION_SIZE];
static uint32_t         clock_us;
static uint32_t         clock_ms;
static bool             master;
static bool             link_up;
static uint32_t         rpc_calls;
static slave_callback_t peer_handler;
static uint8_t          peer_figures[NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE];
static uint32_t         matrix_idle_ms = 1000u;
static matrix_row_t     matrix_rows[MATRIX_ROWS];
static uint32_t         quiet_checks;
static uint32_t         row_reads;

// QMK's EEPROM magic, kept across simulated reboots as the wear-leveling
// backing keeps it.
static bool eeprom_magic_valid = true;

// Power cuts: flash operations and QMK's magic write share one count; the
// operation numbered `cut_at` is interrupted and nothing after it runs until
// the next boot.
typedef enum {
    CUT_BEFORE = 0,
    CUT_HALFWAY,
} cut_mode_t;

static uint32_t   operations;
static uint32_t   cut_at;
static cut_mode_t cut_mode;
static bool       powered_off;

static bool power_cut_now(void) {
    if (powered_off) {
        return true;
    }
    operations++;
    if (operations == cut_at) {
        powered_off = true;
    }
    return false;
}

static bool fake_erase(void *context, uint32_t offset) {
    (void)context;
    if (power_cut_now()) {
        return false;
    }
    clock_us += 40000u;
    if (powered_off) {
        if (cut_mode == CUT_HALFWAY) {
            memset(&region[offset + NOAH_PROFILE_STORAGE_SECTOR_SIZE / 2u], 0xFF, NOAH_PROFILE_STORAGE_SECTOR_SIZE / 2u);
        }
        return false;
    }
    memset(&region[offset], 0xFF, NOAH_PROFILE_STORAGE_SECTOR_SIZE);
    return true;
}

static bool fake_program(void *context, uint32_t offset, const uint8_t *page) {
    (void)context;
    if (power_cut_now()) {
        return false;
    }
    clock_us += 700u;
    for (uint32_t index = 0u; index < NOAH_PROFILE_STORAGE_PAGE_SIZE; index++) {
        region[offset + index] &= page[index];
    }
    return !powered_off;
}

static void fake_read(void *context, uint32_t offset, uint8_t *target, uint32_t length) {
    (void)context;
    memcpy(target, &region[offset], length);
}

static uint32_t fake_now(void *context) {
    (void)context;
    return clock_us;
}

noah_profile_flash_driver_t noah_qmk_profile_flash_driver(void) {
    return (noah_profile_flash_driver_t){.erase_sector = fake_erase, .program_page = fake_program, .now_us = fake_now, .read = fake_read};
}

bool noah_qmk_profile_flash_eeprom_valid(void) {
    return eeprom_magic_valid;
}

uint32_t last_matrix_activity_elapsed(void) {
    quiet_checks++;
    return matrix_idle_ms;
}

matrix_row_t matrix_get_row(uint8_t row) {
    row_reads++;
    assert(row < MATRIX_ROWS);
    return matrix_rows[row];
}

bool noah_qmk_physical_half_origin(uint8_t *origin) {
    *origin = NOAH_PHYSICAL_HALF_ORIGIN_RIGHT;
    return true;
}

bool is_keyboard_master(void) {
    return master;
}

uint32_t timer_read32(void) {
    return clock_ms;
}

uint32_t timer_elapsed32(uint32_t last) {
    return clock_ms - last;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    assert(transaction_id == PUT_PROFILE_FLASH_DIAGNOSTICS);
    peer_handler = callback;
}

bool transaction_rpc_exec(int8_t transaction_id, uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    (void)request;
    assert(transaction_id == PUT_PROFILE_FLASH_DIAGNOSTICS && request_size == 0u && response_size == NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE);
    rpc_calls++;
    if (!link_up) {
        return false;
    }
    memcpy(response, peer_figures, response_size);
    return true;
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t request_size, const void *request) {
    (void)transaction_id;
    (void)request_size;
    (void)request;
    return false;
}

static void request(uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE], uint8_t request_id, uint8_t page) {
    memset(frame, 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE);
    frame[0] = NOAH_PROFILE_WIRE_V1_COMMAND_GET;
    frame[1] = NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL;
    frame[2] = NOAH_QMK_PROFILE_FLASH_WIRE_VALUE;
    frame[3] = request_id;
    frame[4] = page;
}

static uint32_t load_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) | ((uint32_t)bytes[3] << 24u);
}

static void save_a_little(void) {
    noah_profile_store_io_t io      = noah_qmk_profile_flash_io();
    uint8_t                 bytes[] = {1u, 2u, 3u, 4u};

    assert(io.reset(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, 8000u));
    // The first quiet grant erases the header sector; the writer runs in the
    // next.
    noah_qmk_profile_flash_grant_begin();
    assert(!noah_qmk_profile_flash_may_write());
    noah_qmk_profile_flash_grant_begin();
    assert(noah_qmk_profile_flash_may_write());
    assert(io.write(io.context, NOAH_PROFILE_STORAGE_SECTOR_SIZE + 4u, bytes, sizeof(bytes)));
    assert(io.sync(io.context));
    noah_qmk_profile_flash_grant_begin();
}

static void test_idle_grants_do_not_inspect_the_matrix(void) {
    uint32_t checks = quiet_checks;
    uint32_t rows   = row_reads;

    for (uint32_t scan = 0u; scan < 4096u; scan++) {
        noah_qmk_profile_flash_grant_begin();
        assert(noah_qmk_profile_flash_may_write());
        assert(!noah_qmk_profile_flash_grant_flashed());
    }
    assert(quiet_checks == checks && row_reads == rows);
}

// No key held and the matrix unchanged for 50 ms: only then may an erase
// pause the keyboard.
static void test_erases_wait_for_a_quiet_keyboard(void) {
    noah_profile_store_io_t io = noah_qmk_profile_flash_io();

    matrix_idle_ms = NOAH_QMK_PROFILE_FLASH_QUIET_MS;
    assert(noah_qmk_profile_flash_keyboard_quiet());
    matrix_idle_ms = NOAH_QMK_PROFILE_FLASH_QUIET_MS - 1u;
    assert(!noah_qmk_profile_flash_keyboard_quiet());
    matrix_idle_ms                 = 1000u;
    matrix_rows[MATRIX_ROWS - 1u] = 1u;
    assert(!noah_qmk_profile_flash_keyboard_quiet());

    // A key held: a save's header sector waits, and so does its writer.
    operations = 0u;
    assert(io.reset(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, 64u));
    for (uint8_t scan = 0u; scan < 5u; scan++) {
        noah_qmk_profile_flash_grant_begin();
        assert(!noah_qmk_profile_flash_may_write());
    }
    assert(operations == 0u);
    matrix_rows[MATRIX_ROWS - 1u] = 0u;
    noah_qmk_profile_flash_grant_begin();
    assert(operations == 1u && !noah_qmk_profile_flash_may_write());
    uint32_t checks = quiet_checks;
    uint32_t rows   = row_reads;
    noah_qmk_profile_flash_grant_begin();
    assert(noah_qmk_profile_flash_may_write());
    assert(quiet_checks == checks && row_reads == rows);
}

static void test_own_figures_on_page_zero(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    // Before any save: present, all zero.
    request(frame, 9u, 0u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)));
    assert(frame[0] == NOAH_PROFILE_WIRE_V1_COMMAND_GET && frame[3] == 9u && frame[4] == 0u);
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK && frame[6] == NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE);
    assert(frame[7] == NOAH_QMK_PROFILE_FLASH_FIGURES_PRESENT && frame[8] == NOAH_PHYSICAL_HALF_ORIGIN_RIGHT);
    assert(load_u32(&frame[9]) == 0u && load_u32(&frame[21]) == 0u);

    // A save of two sectors: the header sector at reset, the second on its
    // first write, then one page program at the sync.
    save_a_little();
    request(frame, 10u, 0u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)));
    assert(frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
    assert(load_u32(&frame[9]) == 40000u);
    assert(load_u32(&frame[13]) == 700u);
    assert(load_u32(&frame[17]) == 40700u);
    assert(load_u32(&frame[21]) == 80700u);
    assert(load_u32(&frame[25]) == 80700u);
    assert(frame[29] == 1u && frame[30] == 0u && frame[31] == 0u);
}

static void test_requests_are_checked(void) {
    uint8_t frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];

    request(frame, 0u, 0u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED && frame[6] == 0u);
    request(frame, 4u, 0u);
    frame[31] = 1u;
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[3] == 4u && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED && frame[31] == 0u);
    request(frame, 4u, NOAH_QMK_PROFILE_FLASH_WIRE_PAGES);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE);
    request(frame, 4u, 0u);
    frame[2] = NOAH_PROFILE_WIRE_V1_VALUE_STATUS;
    assert(!noah_qmk_profile_flash_wire_get(frame, sizeof(frame)));
    request(frame, 4u, 0u);
    assert(!noah_qmk_profile_flash_wire_get(frame, sizeof(frame) - 1u));
}

static void test_peer_figures_are_pulled_by_the_usb_half(void) {
    uint8_t                          frame[NOAH_PROFILE_WIRE_V1_REPORT_SIZE];
    uint8_t                          answered[NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE];
    noah_profile_flash_diagnostics_t diagnostics = {.longest_erase_us = 51000u, .longest_program_us = 900u, .longest_grant_us = 52000u, .last_save_us = 3000000u, .last_save_flash_us = 2000000u, .saves = 3u};

    // Housekeeping registers the handler on both halves; the other half
    // answers with its own figures.
    clock_ms = 100u;
    master   = false;
    noah_qmk_profile_flash_housekeeping();
    assert(peer_handler != NULL && rpc_calls == 0u);
    peer_handler(0u, NULL, sizeof(answered), answered);
    assert(answered[0] == NOAH_QMK_PROFILE_FLASH_FIGURES_PRESENT && answered[1] == NOAH_PHYSICAL_HALF_ORIGIN_RIGHT && load_u32(&answered[2]) == 40000u);
    clock_ms += 10u * NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 0u);

    // No page-1 demand: neither idle scans nor local/invalid reads cause
    // peer traffic.
    master = true;
    for (uint8_t tick = 0u; tick < 10u; tick++) {
        clock_ms += NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS;
        request(frame, 5u, 0u);
        assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)));
        request(frame, 0u, 1u);
        assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED);
        request(frame, 5u, NOAH_QMK_PROFILE_FLASH_WIRE_PAGES);
        assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE);
        noah_qmk_profile_flash_housekeeping();
    }
    assert(rpc_calls == 0u);
    // Nothing pulled yet: unavailable. The read only queues the work.
    request(frame, 5u, 1u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE && frame[6] == 0u);
    assert(rpc_calls == 0u);

    // One deferred attempt per demand, rate limited to once a second;
    // failure does not leave a perpetual background retry.
    noah_qmk_profile_flash_encode_figures(&diagnostics, NOAH_PHYSICAL_HALF_ORIGIN_LEFT, peer_figures);
    master  = true;
    link_up = false;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 1u);
    clock_ms += 10u * NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 1u);
    request(frame, 6u, 1u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE);
    link_up = true;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 2u);
    for (uint8_t read = 0u; read < 10u; read++) {
        request(frame, 7u, 1u);
        assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK);
        noah_qmk_profile_flash_housekeeping();
    }
    assert(rpc_calls == 2u);
    clock_ms += NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS - 1u;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 2u);
    clock_ms += 1u;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 3u);
    request(frame, 7u, 1u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK && frame[6] == NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE);
    assert(memcmp(&frame[7], peer_figures, NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE) == 0);
    assert(frame[8] == NOAH_PHYSICAL_HALF_ORIGIN_LEFT && load_u32(&frame[9]) == 51000u && load_u32(&frame[21]) == 3000000u && frame[29] == 3u);
    link_up = false;
    clock_ms += NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 4u);
    request(frame, 8u, 1u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && frame[5] == NOAH_PROFILE_WIRE_V1_STATUS_OK && load_u32(&frame[9]) == 51000u);
    // Consume that last request across timer wrap, then stop all traffic.
    clock_ms = UINT32_MAX - 500u;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 5u);
    request(frame, 9u, 1u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)));
    clock_ms += NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS - 1u;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 5u);
    clock_ms++;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 6u);
    for (uint8_t tick = 0u; tick < 10u; tick++) {
        clock_ms += NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS;
        noah_qmk_profile_flash_housekeeping();
    }
    assert(rpc_calls == 6u);

    // A later reader first sees the old cache, then the refreshed figures;
    // the other half never originates a diagnostic pull.
    diagnostics.longest_erase_us = 54000u;
    noah_qmk_profile_flash_encode_figures(&diagnostics, NOAH_PHYSICAL_HALF_ORIGIN_LEFT, peer_figures);
    link_up = true;
    request(frame, 10u, 1u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && load_u32(&frame[9]) == 51000u);
    master = false;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 6u);
    master = true;
    noah_qmk_profile_flash_housekeeping();
    assert(rpc_calls == 7u);
    request(frame, 11u, 1u);
    assert(noah_qmk_profile_flash_wire_get(frame, sizeof(frame)) && load_u32(&frame[9]) == 54000u);
}

enum {
    TEST_COMPILED_DIGEST = 0x10203040u,
    TEST_ACTION_DIGEST   = 0x23456789u,
};

static noah_profile_store_compatibility_t compatibility(void) {
    return (noah_profile_store_compatibility_t){.schema_major = NOAH_PROFILE_SCHEMA_MAJOR, .schema_minor = NOAH_PROFILE_STORE_SCHEMA_MINOR, .compiled_default_digest = TEST_COMPILED_DIGEST, .action_abi_digest = TEST_ACTION_DIGEST};
}

// Commits a small profile of generation `generation` as a save on a quiet
// keyboard would, through a backend of its own.
static void commit(uint32_t generation) {
    static const uint8_t           payload[] = {'N', 'L', 'P', '1', 4u, 0u, 0u, 1u};
    noah_profile_flash_store_t     flash;
    noah_profile_store_t           store;
    noah_profile_store_record_t    record;
    noah_profile_store_result_t    result;
    noah_profile_store_candidate_t candidate = {
        .format_version          = NOAH_PROFILE_STORE_FORMAT_VERSION,
        .schema_major            = NOAH_PROFILE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .payload_length          = sizeof(payload),
        .generation              = generation,
        .payload_crc32           = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, sizeof(payload))),
        .payload_digest          = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, sizeof(payload)),
        .compiled_default_digest = TEST_COMPILED_DIGEST,
        .action_abi_digest       = TEST_ACTION_DIGEST,
    };

    noah_profile_flash_store_init(&flash, noah_qmk_profile_flash_driver());
    noah_profile_store_init(&store, noah_profile_flash_store_io(&flash), compatibility());
    (void)noah_profile_store_boot_select(&store, &record);
    assert(noah_profile_store_prepare_begin(&store, &candidate) == NOAH_PROFILE_STORE_OK);
    noah_profile_flash_store_grant_begin(&flash, true);
    noah_profile_flash_store_grant_begin(&flash, true);
    assert(noah_profile_flash_store_may_write(&flash));
    assert(noah_profile_store_prepare_write(&store, 0u, payload, sizeof(payload)) == NOAH_PROFILE_STORE_OK);
    result = noah_profile_store_prepare_commit_begin(&store);
    while (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        result = noah_profile_store_prepare_commit_step(&store, 20u, &record);
    }
    assert(result == NOAH_PROFILE_STORE_OK && record.generation == generation);
}

// What a reboot finds: a fresh backend over the same flash.
static noah_profile_store_result_t boot_selection(noah_profile_store_record_t *selected) {
    noah_profile_flash_store_t flash;
    noah_profile_store_t       store;

    noah_profile_flash_store_init(&flash, noah_qmk_profile_flash_driver());
    noah_profile_store_init(&store, noah_profile_flash_store_read_only_io(&flash), compatibility());
    return noah_profile_store_boot_select(&store, selected);
}

// One boot as QMK runs it: keyboard_pre_init (this adapter), then keyboard
// init, which formats logical EEPROM when its magic is not valid and makes
// the magic valid first (quantum/eeconfig.c). A power cut stops the boot.
static void boot(void) {
    noah_qmk_profile_flash_pre_init();
    // A magic write the power cut interrupts is no valid magic.
    if (!eeprom_magic_valid && !power_cut_now() && !powered_off) {
        eeprom_magic_valid = true;
    }
}

// An EEPROM reset forgets the saved profiles, however the boots that carry
// it out are interrupted: before, during or between the two header erases,
// or before QMK writes its magic.
static void test_eeprom_reset_forgets_the_saved_profiles(void) {
    static uint8_t              saved[NOAH_PROFILE_STORAGE_REGION_SIZE];
    noah_profile_store_record_t selected;

    memset(region, 0xFF, sizeof(region));
    commit(1u);
    commit(2u);
    assert(boot_selection(&selected) == NOAH_PROFILE_STORE_OK && selected.slot == NOAH_PROFILE_SLOT_B && selected.generation == 2u);
    memcpy(saved, region, sizeof(saved));

    // A valid magic: boot erases nothing.
    operations = 0u;
    boot();
    assert(operations == 0u && boot_selection(&selected) == NOAH_PROFILE_STORE_OK);

    for (cut_mode_t mode = CUT_BEFORE; mode <= CUT_HALFWAY; mode++) {
        // Operations: slot A's header erase, slot B's, QMK's magic write.
        for (uint32_t cut = 1u; cut <= 3u; cut++) {
            memcpy(region, saved, sizeof(region));
            eeprom_magic_valid = false;
            operations         = 0u;
            cut_at             = cut;
            cut_mode           = mode;
            powered_off        = false;
            boot();
            assert(powered_off && !eeprom_magic_valid);

            // The next boot finishes the reset.
            operations  = 0u;
            cut_at      = 0u;
            powered_off = false;
            boot();
            assert(eeprom_magic_valid && operations == 3u);
            assert(boot_selection(&selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
            boot();
            assert(operations == 3u);
        }
    }
    cut_at = 0u;
}

int main(void) {
    test_idle_grants_do_not_inspect_the_matrix();
    test_eeprom_reset_forgets_the_saved_profiles();
    memset(region, 0x00, sizeof(region));
    test_own_figures_on_page_zero();
    test_requests_are_checked();
    test_peer_figures_are_pulled_by_the_usb_half();
    test_erases_wait_for_a_quiet_keyboard();
    puts("qmk profile flash adapter tests passed");
    return 0;
}
