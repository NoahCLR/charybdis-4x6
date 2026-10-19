// Profile slot flash backend (D-F29): NOR semantics with no page programmed
// twice between erases, sector erase only where a save writes and only on a
// quiet keyboard, reads in place, pacing per durable-I/O grant, pause
// diagnostics, and the store's power-loss guarantee with power cut at every
// flash operation of a save.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/storage/profile_checksum.h"
#include "users/noah/lib/profile/storage/profile_flash_store.h"
#include "users/noah/lib/profile/storage/profile_store.h"

#define CHECK(condition)                                                                    \
    do {                                                                                    \
        if (!(condition)) {                                                                 \
            fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            exit(1);                                                                        \
        }                                                                                   \
    } while (0)

enum {
    ACTION_ABI_DIGEST = UINT32_C(0x23456789),
    COMPILED_DIGEST   = UINT32_C(0x10203040),
    REGION            = NOAH_PROFILE_STORAGE_REGION_SIZE,
    SECTOR            = NOAH_PROFILE_STORAGE_SECTOR_SIZE,
    PAGE              = NOAH_PROFILE_STORAGE_PAGE_SIZE,
    PAGES             = REGION / PAGE,
    // Where a slot's marker cells and payload sit on flash.
    CELLS   = NOAH_PROFILE_STORAGE_SLOT_MARKER_CELL_FLASH_OFFSET,
    PAYLOAD = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_FLASH_OFFSET,
    ERASE_US          = 45000u,
    PROGRAM_US        = 800u,
    // The widest write one durable-I/O grant makes: one split chunk.
    GRANT_BYTES = 106u,
    // The widest single store write: one host candidate chunk.
    CHUNK = 20u,
};

typedef enum {
    // The interrupted operation left flash as it was.
    CUT_BEFORE = 0,
    // The interrupted operation reached the first half of its bytes.
    CUT_HALFWAY,
} cut_mode_t;

typedef struct {
    uint8_t    bytes[REGION];
    uint32_t   clock_us;
    uint32_t   operations;
    uint32_t   cut_at;
    cut_mode_t cut_mode;
    bool       powered_off;
    uint32_t   grant_erases;
    uint32_t   grant_programs;
    uint32_t   nor_violations;
    uint32_t   double_programs;
    uint32_t   reads;
    // Pages programmed since their sector was last erased.
    bool       programmed[PAGES];
} fake_flash_t;

static fake_flash_t flash_chip;
static uint8_t      snapshot[REGION];

static bool fake_erase(void *context, uint32_t offset) {
    fake_flash_t *chip = context;

    CHECK(offset % SECTOR == 0u && offset < REGION);
    if (chip->powered_off) {
        return false;
    }
    chip->operations++;
    chip->grant_erases++;
    chip->clock_us += ERASE_US;
    if (chip->operations == chip->cut_at) {
        if (chip->cut_mode == CUT_HALFWAY) {
            memset(&chip->bytes[offset], 0xFF, SECTOR / 2u);
        }
        chip->powered_off = true;
        return false;
    }
    memset(&chip->bytes[offset], 0xFF, SECTOR);
    memset(&chip->programmed[offset / PAGE], 0, SECTOR / PAGE);
    return true;
}

static bool fake_program(void *context, uint32_t offset, const uint8_t *page) {
    fake_flash_t *chip  = context;
    uint32_t      limit = PAGE;

    CHECK(offset % PAGE == 0u && offset < REGION);
    if (chip->powered_off) {
        return false;
    }
    chip->operations++;
    chip->grant_programs++;
    chip->clock_us += PROGRAM_US;
    // Programming a page again before its sector is erased is outside what
    // the flash part promises: the fake refuses it.
    if (chip->programmed[offset / PAGE]) {
        chip->double_programs++;
        return false;
    }
    chip->programmed[offset / PAGE] = true;
    if (chip->operations == chip->cut_at) {
        limit = chip->cut_mode == CUT_HALFWAY ? PAGE / 2u : 0u;
    }
    for (uint32_t index = 0u; index < limit; index++) {
        uint8_t *cell = &chip->bytes[offset + index];

        // Programming only clears bits: a byte meant to change must not need
        // a bit set that is already clear.
        if (page[index] != 0xFFu && (uint8_t)(*cell & page[index]) != page[index]) {
            chip->nor_violations++;
        }
        *cell &= page[index];
    }
    if (chip->operations == chip->cut_at) {
        chip->powered_off = true;
        return false;
    }
    return true;
}

static void fake_read(void *context, uint32_t offset, uint8_t *target, uint32_t length) {
    fake_flash_t *chip = context;

    CHECK(offset < REGION && length <= REGION - offset);
    chip->reads++;
    memcpy(target, &chip->bytes[offset], length);
}

static uint32_t fake_now(void *context) {
    return ((fake_flash_t *)context)->clock_us;
}

static noah_profile_flash_driver_t driver_for(fake_flash_t *chip) {
    return (noah_profile_flash_driver_t){
        .erase_sector = fake_erase,
        .program_page = fake_program,
        .now_us       = fake_now,
        .read         = fake_read,
        .context      = chip,
    };
}

static void reset_chip(fake_flash_t *chip, uint8_t fill) {
    memset(chip, 0, sizeof(*chip));
    memset(chip->bytes, fill, sizeof(chip->bytes));
}

static noah_profile_store_compatibility_t compatibility(void) {
    return (noah_profile_store_compatibility_t){.schema_major = NOAH_PROFILE_SCHEMA_MAJOR, .schema_minor = 0u, .compiled_default_digest = COMPILED_DIGEST, .action_abi_digest = ACTION_ABI_DIGEST};
}

// A canonical blob of `length` bytes: one RGB domain filling it, its bytes
// a pattern of `seed`.
static void build_profile(uint8_t *payload, uint32_t length, uint8_t seed) {
    static const uint8_t header[] = {'N', 'L', 'P', '1', 4u, 0u, 1u, 1u, 0x10u, NOAH_PROFILE_DOMAIN_VERSION_RGB};
    uint32_t             domain   = length - 14u;

    memcpy(payload, header, sizeof(header));
    payload[10] = (uint8_t)domain;
    payload[11] = (uint8_t)(domain >> 8u);
    payload[12] = (uint8_t)(domain >> 16u);
    payload[13] = (uint8_t)(domain >> 24u);
    for (uint32_t index = 14u; index < length; index++) {
        payload[index] = (uint8_t)(index * 13u + seed);
    }
}

static noah_profile_store_candidate_t candidate_for(const uint8_t *payload, uint32_t length, uint32_t generation) {
    return (noah_profile_store_candidate_t){
        .format_version          = NOAH_PROFILE_STORE_FORMAT_VERSION,
        .via_generation          = 17u,
        .via_digest              = UINT32_C(0x89ABCDEF),
        .schema_major            = NOAH_PROFILE_SCHEMA_MAJOR,
        .schema_minor            = 0u,
        .domain_mask             = 1u,
        .flags                   = NOAH_PROFILE_STORE_FLAG_OVERRIDE,
        .payload_length          = length,
        .generation              = generation,
        .origin_half             = 0u,
        .payload_crc32           = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, payload, length)),
        .payload_digest          = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, payload, length),
        .compiled_default_digest = COMPILED_DIGEST,
        .action_abi_digest       = ACTION_ABI_DIGEST,
    };
}

typedef struct {
    noah_profile_flash_store_t flash;
    noah_profile_store_t       store;
} half_t;

static noah_profile_store_result_t boot(half_t *half, fake_flash_t *chip, noah_profile_store_record_t *selected) {
    noah_profile_flash_store_init(&half->flash, driver_for(chip));
    noah_profile_store_init(&half->store, noah_profile_flash_store_io(&half->flash), compatibility());
    return noah_profile_store_boot_select(&half->store, selected);
}

// One durable-I/O grant: the flash work of the previous one stayed within
// the pause policy (one erase or one page program, never both), then the
// next opens; returns whether the store's writer may run in it.
static bool grant_with(half_t *half, fake_flash_t *chip, bool quiet) {
    CHECK(chip->grant_erases <= 1u);
    CHECK(chip->grant_programs <= 1u);
    CHECK(chip->grant_erases == 0u || chip->grant_programs == 0u);
    chip->grant_erases   = 0u;
    chip->grant_programs = 0u;
    noah_profile_flash_store_grant_begin(&half->flash, quiet);
    return noah_profile_flash_store_may_write(&half->flash);
}

// Grants on a quiet keyboard until the writer may run.
static void grant(half_t *half, fake_flash_t *chip) {
    uint32_t waited = 0u;

    while (!grant_with(half, chip, true)) {
        CHECK(++waited < 4u);
    }
}

// One save as the firmware runs it: a grant per begin, per split chunk of
// writes and per commit step. Stops at the first failure, as a keyboard
// whose power went would.
static noah_profile_store_result_t save(half_t *half, fake_flash_t *chip, const uint8_t *payload, uint32_t length, uint32_t generation) {
    noah_profile_store_candidate_t candidate = candidate_for(payload, length, generation);
    noah_profile_store_record_t    record;
    noah_profile_store_result_t    result;
    uint32_t                       offset = 0u;

    grant(half, chip);
    result = noah_profile_store_prepare_begin(&half->store, &candidate);
    if (result != NOAH_PROFILE_STORE_OK) {
        return result;
    }
    while (offset < length) {
        uint32_t grant_end = offset + GRANT_BYTES < length ? offset + GRANT_BYTES : length;

        grant(half, chip);
        while (offset < grant_end) {
            uint32_t piece = grant_end - offset < CHUNK ? grant_end - offset : CHUNK;

            result = noah_profile_store_prepare_write(&half->store, offset, &payload[offset], piece);
            if (result != NOAH_PROFILE_STORE_OK) {
                return result;
            }
            offset += piece;
        }
    }
    grant(half, chip);
    result = noah_profile_store_prepare_commit_begin(&half->store);
    while (result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        grant(half, chip);
        result = noah_profile_store_prepare_commit_step(&half->store, 20u, &record);
    }
    grant(half, chip);
    return result;
}

static void test_reset_takes_the_slot_and_erases_ahead_of_writes(void) {
    half_t                      half;
    noah_profile_store_io_t     io;
    noah_profile_store_record_t selected;
    uint8_t                     bytes[NOAH_PROFILE_STORE_IO_CHUNK_MAX];
    static const uint8_t        pattern[4] = {1u, 2u, 3u, 4u};

    reset_chip(&flash_chip, 0x5Au);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    io = noah_profile_flash_store_io(&half.flash);
    CHECK(io.read && io.write && io.reset && io.sync);

    // No write lands before a reset names its range, and none outside it.
    CHECK(!io.write(io.context, 64u, pattern, sizeof(pattern)));
    CHECK(!io.reset(io.context, 0x1000u, 64u));
    CHECK(!io.reset(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX + 1u));
    CHECK(!io.reset(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, 31u));
    // A reset touches no flash; the slot already reads as erased.
    CHECK(io.reset(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, 32u + 2u * SECTOR));
    CHECK(flash_chip.operations == 0u);
    CHECK(io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR, bytes, 32u) && bytes[0] == 0xFFu && bytes[31] == 0xFFu);
    CHECK(flash_chip.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR] == 0x5Au);

    // While the keyboard is busy nothing is erased and the writer waits.
    for (uint32_t scan = 0u; scan < 3u; scan++) {
        CHECK(!grant_with(&half, &flash_chip, false));
    }
    CHECK(flash_chip.operations == 0u);
    // Quiet: one grant erases the header sector, and the writer waits for the
    // next grant.
    CHECK(!grant_with(&half, &flash_chip, true));
    CHECK(flash_chip.operations == 1u);
    for (uint32_t index = 0u; index < SECTOR; index++) {
        CHECK(flash_chip.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + index] == 0xFFu);
    }
    CHECK(grant_with(&half, &flash_chip, false));
    // Later sectors keep their old bytes until erased, but read as erased.
    CHECK(flash_chip.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + SECTOR] == 0x5Au);
    CHECK(io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + SECTOR, bytes, 4u) && bytes[0] == 0xFFu && bytes[3] == 0xFFu);
    // Outside the range, and in slot A, reads are the flash as it is.
    CHECK(io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + 3u * SECTOR, bytes, 4u) && bytes[0] == 0x5Au);
    CHECK(io.read(io.context, 0u, bytes, 4u) && bytes[0] == 0x5Au);
    CHECK(!io.write(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + 3u * SECTOR, pattern, 1u));
    CHECK(!io.write(io.context, 0u, pattern, 1u));

    // A payload write stays in the page buffer, readable, until its page
    // fills or a sync; it lands a kilobyte further on flash.
    CHECK(io.write(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + 40u, pattern, sizeof(pattern)));
    CHECK(flash_chip.operations == 1u);
    CHECK(io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + 38u, bytes, 8u));
    CHECK(bytes[0] == 0xFFu && bytes[1] == 0xFFu && memcmp(&bytes[2], pattern, sizeof(pattern)) == 0 && bytes[6] == 0xFFu);
    CHECK(io.sync(io.context));
    CHECK(flash_chip.operations == 2u);
    CHECK(memcmp(&flash_chip.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + PAYLOAD + 8u], pattern, sizeof(pattern)) == 0);
    CHECK(io.sync(io.context) && flash_chip.operations == 2u);
    // With writes under way, a quiet grant erases the next sector ahead;
    // a busy one does not.
    CHECK(grant_with(&half, &flash_chip, false) && flash_chip.operations == 2u);
    CHECK(!grant_with(&half, &flash_chip, true) && flash_chip.operations == 3u);
    CHECK(grant_with(&half, &flash_chip, true) && flash_chip.operations == 3u);
    CHECK(flash_chip.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + SECTOR] == 0xFFu);

    // Reads and writes stay within one store chunk and the bytes a slot can
    // hold.
    CHECK(!io.read(io.context, 0u, bytes, NOAH_PROFILE_STORE_IO_CHUNK_MAX + 1u));
    CHECK(io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + 32u + NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX - 1u, bytes, 1u));
    CHECK(!io.read(io.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + 32u + NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX - 1u, bytes, 2u));
    CHECK(!io.read(io.context, REGION - 1u, bytes, 1u));
    CHECK(flash_chip.nor_violations == 0u && flash_chip.double_programs == 0u);

    io = noah_profile_flash_store_read_only_io(&half.flash);
    CHECK(io.read && !io.write && !io.reset && !io.sync);
}

// A writer that outruns the erases (it never does under the policy) still
// never writes unerased flash: the backend erases first.
static void test_writes_never_land_on_unerased_flash(void) {
    half_t                      half;
    noah_profile_store_io_t     io;
    noah_profile_store_record_t selected;
    static const uint8_t        pattern[4] = {1u, 2u, 3u, 4u};

    reset_chip(&flash_chip, 0x00u);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    io = noah_profile_flash_store_io(&half.flash);
    CHECK(io.reset(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, 32u + 3u * SECTOR));
    CHECK(io.write(io.context, 32u + 2u * SECTOR, pattern, sizeof(pattern)) && io.sync(io.context));
    CHECK(flash_chip.operations == 4u);
    CHECK(memcmp(&flash_chip.bytes[PAYLOAD + 2u * SECTOR], pattern, sizeof(pattern)) == 0);
    CHECK(flash_chip.nor_violations == 0u && flash_chip.double_programs == 0u);
}

// The marker only loses bits: erased 0xFF, prepared 0x3C, committed 0x18,
// invalid 0x00. Each step programs a cell of its own, in a page of its own,
// and the marker reads as the cells' AND.
static void test_marker_steps_each_program_their_own_cell(void) {
    half_t                      half;
    noah_profile_store_io_t     io;
    noah_profile_store_record_t selected;
    static const uint8_t        steps[] = {0x3Cu, 0x18u, 0x00u};
    static const uint8_t        header[NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE - 1u] = {7u};
    uint8_t                     byte;

    reset_chip(&flash_chip, 0x00u);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    io = noah_profile_flash_store_io(&half.flash);
    CHECK(io.reset(io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, 64u));
    grant(&half, &flash_chip);
    CHECK(io.write(io.context, 0u, header, 20u) && io.write(io.context, 20u, &header[20], 11u) && io.sync(io.context));
    CHECK(flash_chip.bytes[0] == 7u && flash_chip.bytes[30] == 0u && flash_chip.bytes[31] == 0xFFu);
    for (size_t step = 0u; step < sizeof(steps); step++) {
        CHECK(io.write(io.context, 31u, &steps[step], 1u) && io.sync(io.context));
        CHECK(io.read(io.context, 31u, &byte, 1u) && byte == steps[step]);
        CHECK(flash_chip.bytes[CELLS + step * PAGE] == steps[step]);
    }
    // There is no fourth step.
    CHECK(!io.write(io.context, 31u, &steps[2], 1u));
    // The header page was programmed once, as was each cell's page.
    CHECK(flash_chip.nor_violations == 0u && flash_chip.double_programs == 0u);
}

// A save programs each page once as it fills, erases each sector it reaches
// once, and reports its pauses.
static void test_save_paces_and_reports_its_pauses(void) {
    enum { LENGTH = 70000u };
    static uint8_t                   payload[LENGTH];
    half_t                           half;
    noah_profile_store_record_t      selected;
    noah_profile_flash_diagnostics_t diagnostics;
    uint32_t                         sectors = (PAYLOAD + LENGTH + SECTOR - 1u) / SECTOR;
    uint32_t                         pages   = (LENGTH + PAGE - 1u) / PAGE;

    build_profile(payload, LENGTH, 3u);
    reset_chip(&flash_chip, 0x00u);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(save(&half, &flash_chip, payload, LENGTH, 1u) == NOAH_PROFILE_STORE_OK);
    CHECK(flash_chip.nor_violations == 0u && flash_chip.double_programs == 0u);

    diagnostics = noah_profile_flash_store_diagnostics(&half.flash);
    CHECK(diagnostics.saves == 1u);
    CHECK(diagnostics.erases == sectors);
    // Every payload page once, then the header's page and two marker cells'.
    CHECK(diagnostics.programs == pages + 3u);
    CHECK(diagnostics.longest_erase_us == ERASE_US);
    CHECK(diagnostics.longest_program_us == PROGRAM_US);
    CHECK(diagnostics.longest_grant_us == ERASE_US);
    CHECK(diagnostics.last_save_flash_us == sectors * ERASE_US + (pages + 3u) * PROGRAM_US);
    CHECK(diagnostics.last_save_us == diagnostics.last_save_flash_us);

    // Read in place after a reboot, past the 64 KiB line.
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_OK);
    CHECK(selected.slot == NOAH_PROFILE_SLOT_A && selected.generation == 1u && selected.payload_length == LENGTH);
    CHECK(memcmp(&flash_chip.bytes[PAYLOAD], payload, LENGTH) == 0);
    diagnostics = noah_profile_flash_store_diagnostics(&half.flash);
    CHECK(diagnostics.saves == 0u && diagnostics.erases == 0u && diagnostics.programs == 0u);

    // The next save goes to slot B; a third back to A over the old bytes.
    build_profile(payload, LENGTH, 9u);
    CHECK(save(&half, &flash_chip, payload, LENGTH, 2u) == NOAH_PROFILE_STORE_OK);
    build_profile(payload, 9000u, 11u);
    CHECK(save(&half, &flash_chip, payload, 9000u, 3u) == NOAH_PROFILE_STORE_OK);
    CHECK(flash_chip.nor_violations == 0u && flash_chip.double_programs == 0u);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_OK);
    CHECK(selected.slot == NOAH_PROFILE_SLOT_A && selected.generation == 3u && selected.payload_length == 9000u);
}

// Power cut at every flash operation of a save leaves the previous profile
// active, or the new one once its commit marker was programmed; a later save
// then succeeds.
static void sweep_power_cuts(uint32_t length, cut_mode_t mode) {
    static uint8_t              previous[NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX];
    static uint8_t              next[NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX];
    half_t                      half;
    noah_profile_store_record_t selected;
    uint32_t                    operations;

    build_profile(previous, length, 5u);
    build_profile(next, length, 6u);
    // Slot A holds generation 1 and slot B generation 2, so the save under
    // test (3) erases a slot that held a committed profile.
    reset_chip(&flash_chip, 0x00u);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(save(&half, &flash_chip, previous, length, 1u) == NOAH_PROFILE_STORE_OK);
    CHECK(save(&half, &flash_chip, previous, length, 2u) == NOAH_PROFILE_STORE_OK);
    memcpy(snapshot, flash_chip.bytes, sizeof(snapshot));

    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_OK && selected.generation == 2u);
    flash_chip.operations = 0u;
    CHECK(save(&half, &flash_chip, next, length, 3u) == NOAH_PROFILE_STORE_OK);
    operations = flash_chip.operations;
    CHECK(operations > 3u && flash_chip.nor_violations == 0u && flash_chip.double_programs == 0u);

    for (uint32_t cut = 1u; cut <= operations; cut++) {
        // The commit marker is the last operation; its first half holds it.
        bool committed = cut == operations && mode == CUT_HALFWAY;

        memcpy(flash_chip.bytes, snapshot, sizeof(snapshot));
        CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_OK && selected.generation == 2u);
        flash_chip.operations  = 0u;
        flash_chip.cut_at      = cut;
        flash_chip.cut_mode    = mode;
        flash_chip.powered_off = false;
        CHECK(save(&half, &flash_chip, next, length, 3u) != NOAH_PROFILE_STORE_OK);
        CHECK(flash_chip.powered_off);

        flash_chip.cut_at      = 0u;
        flash_chip.powered_off = false;
        CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_OK);
        CHECK(selected.generation == (committed ? 3u : 2u));
        CHECK(selected.slot == (committed ? NOAH_PROFILE_SLOT_A : NOAH_PROFILE_SLOT_B));
        CHECK(selected.payload_digest == candidate_for(committed ? next : previous, length, 1u).payload_digest);

        // The interrupted slot is reused by the next save.
        CHECK(save(&half, &flash_chip, next, length, selected.generation + 1u) == NOAH_PROFILE_STORE_OK);
        CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_OK && selected.generation == (committed ? 4u : 3u));
        CHECK(flash_chip.nor_violations == 0u && flash_chip.double_programs == 0u);
    }
}

static void test_power_loss_at_every_flash_operation(void) {
    sweep_power_cuts(9000u, CUT_BEFORE);
    sweep_power_cuts(9000u, CUT_HALFWAY);
    sweep_power_cuts(70000u, CUT_HALFWAY);
}

// An aborted save leaves its slot invalid and the committed one active.
static void test_abort_invalidates_the_target(void) {
    static uint8_t                 payload[5000u];
    half_t                         half;
    noah_profile_store_record_t    selected;
    noah_profile_store_candidate_t candidate;

    build_profile(payload, sizeof(payload), 1u);
    reset_chip(&flash_chip, 0x00u);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(save(&half, &flash_chip, payload, sizeof(payload), 1u) == NOAH_PROFILE_STORE_OK);
    candidate = candidate_for(payload, sizeof(payload), 2u);
    CHECK(noah_profile_store_prepare_begin(&half.store, &candidate) == NOAH_PROFILE_STORE_OK);
    grant(&half, &flash_chip);
    CHECK(noah_profile_store_prepare_write(&half.store, 0u, payload, 20u) == NOAH_PROFILE_STORE_OK);
    CHECK(noah_profile_store_prepare_abort(&half.store) == NOAH_PROFILE_STORE_OK);
    CHECK(flash_chip.bytes[NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + CELLS] == 0x00u);
    CHECK(flash_chip.nor_violations == 0u && flash_chip.double_programs == 0u);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_OK && selected.slot == NOAH_PROFILE_SLOT_A && selected.generation == 1u);
}

// A save abandoned before its header sector was erased never touched its
// slot: the abort erases and programs nothing.
static void test_abort_before_any_erase_touches_nothing(void) {
    static uint8_t                 payload[5000u];
    half_t                         half;
    noah_profile_store_record_t    selected;
    noah_profile_store_candidate_t candidate;

    build_profile(payload, sizeof(payload), 1u);
    reset_chip(&flash_chip, 0x00u);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE);
    CHECK(save(&half, &flash_chip, payload, sizeof(payload), 1u) == NOAH_PROFILE_STORE_OK);
    CHECK(save(&half, &flash_chip, payload, sizeof(payload), 2u) == NOAH_PROFILE_STORE_OK);
    memcpy(snapshot, flash_chip.bytes, sizeof(snapshot));
    flash_chip.operations = 0u;
    candidate             = candidate_for(payload, sizeof(payload), 3u);
    CHECK(noah_profile_store_prepare_begin(&half.store, &candidate) == NOAH_PROFILE_STORE_OK);
    CHECK(!grant_with(&half, &flash_chip, false));
    CHECK(noah_profile_store_prepare_abort(&half.store) == NOAH_PROFILE_STORE_OK);
    CHECK(flash_chip.operations == 0u && memcmp(snapshot, flash_chip.bytes, sizeof(snapshot)) == 0);
    CHECK(boot(&half, &flash_chip, &selected) == NOAH_PROFILE_STORE_OK && selected.slot == NOAH_PROFILE_SLOT_B && selected.generation == 2u);
}

int main(void) {
    test_reset_takes_the_slot_and_erases_ahead_of_writes();
    test_writes_never_land_on_unerased_flash();
    test_marker_steps_each_program_their_own_cell();
    test_save_paces_and_reports_its_pauses();
    test_abort_invalidates_the_target();
    test_abort_before_any_erase_touches_nothing();
    test_power_loss_at_every_flash_operation();
    puts("profile flash store host tests passed");
    return 0;
}
