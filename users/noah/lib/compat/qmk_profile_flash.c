// ───────────────────────────────────────────────────────────────────────────
// QMK Profile Slot Flash Adapter
// ───────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "qmk_profile_flash.h"

#include <string.h>

#include "qmk_physical_half.h"
#include "../profile/protocol/profile_wire_v1.h"

#if defined(MCU_RP) && defined(PROTOCOL_CHIBIOS)

#    include "hardware/flash.h"
#    include "hardware/regs/addressmap.h"
#    include "hardware/structs/timer.h"
#    include "hardware/sync.h"
#    include "wear_leveling_rp2040_flash_config.h"

_Static_assert(NOAH_PROFILE_STORAGE_SECTOR_SIZE == FLASH_SECTOR_SIZE, "slot sectors are the flash's erase sectors");
_Static_assert(NOAH_PROFILE_STORAGE_PAGE_SIZE == FLASH_PAGE_SIZE, "slot pages are the flash's program pages");
_Static_assert(NOAH_PROFILE_STORAGE_FLASH_SIZE == WEAR_LEVELING_RP2040_FLASH_SIZE, "the slot region assumes the flash size wear leveling assumes");
_Static_assert(NOAH_PROFILE_STORAGE_FLASH_OFFSET + NOAH_PROFILE_STORAGE_REGION_SIZE <= WEAR_LEVELING_RP2040_FLASH_BASE, "the slot region overlaps the wear-leveling backing");

// Erase and program run with XIP off. Interrupts stay off throughout, so no
// handler in flash runs meanwhile; pico-sdk keeps the flash routines in RAM,
// as QMK's wear-leveling driver relies on.
static bool rp2040_erase_sector(void *context, uint32_t offset) {
    uint32_t interrupts;

    (void)context;
    interrupts = save_and_disable_interrupts();
    flash_range_erase(NOAH_PROFILE_STORAGE_FLASH_OFFSET + offset, FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
    return true;
}

static bool rp2040_program_page(void *context, uint32_t offset, const uint8_t *page) {
    uint32_t interrupts;

    (void)context;
    interrupts = save_and_disable_interrupts();
    flash_range_program(NOAH_PROFILE_STORAGE_FLASH_OFFSET + offset, page, FLASH_PAGE_SIZE);
    restore_interrupts(interrupts);
    return true;
}

static void rp2040_read(void *context, uint32_t offset, uint8_t *target, uint32_t length) {
    (void)context;
    memcpy(target, (const uint8_t *)(XIP_BASE + NOAH_PROFILE_STORAGE_FLASH_OFFSET + offset), length);
}

// The free-running microsecond timer counts on while interrupts are off.
static uint32_t rp2040_now_us(void *context) {
    (void)context;
    return timer_hw->timerawl;
}

noah_profile_flash_driver_t noah_qmk_profile_flash_driver(void) {
    return (noah_profile_flash_driver_t){
        .erase_sector = rp2040_erase_sector,
        .program_page = rp2040_program_page,
        .now_us       = rp2040_now_us,
        .read         = rp2040_read,
        .context      = NULL,
    };
}

#endif

#if !defined(NOAH_HOST_QMK_STUB)
#    include "nvm_eeconfig.h"

bool noah_qmk_profile_flash_eeprom_valid(void) {
    return nvm_eeconfig_is_enabled();
}
#endif

static noah_profile_flash_store_t noah_qmk_profile_flash;
static bool                       noah_qmk_profile_flash_ready;

static noah_profile_flash_store_t *flash_store(void) {
    if (!noah_qmk_profile_flash_ready) {
        noah_profile_flash_store_init(&noah_qmk_profile_flash, noah_qmk_profile_flash_driver());
        noah_qmk_profile_flash_ready = true;
    }
    return &noah_qmk_profile_flash;
}

noah_profile_store_io_t noah_qmk_profile_flash_read_only_io(void) {
    return noah_profile_flash_store_read_only_io(flash_store());
}

noah_profile_store_io_t noah_qmk_profile_flash_io(void) {
    return noah_profile_flash_store_io(flash_store());
}

// No key held, and the matrix unchanged for the quiet time: an erase now
// cannot swallow a key press (D-F29). The USB half's matrix holds both
// halves' rows; the other half sees its own.
bool noah_qmk_profile_flash_keyboard_quiet(void) {
    if (last_matrix_activity_elapsed() < NOAH_QMK_PROFILE_FLASH_QUIET_MS) {
        return false;
    }
    for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
        if (matrix_get_row(row) != 0u) {
            return false;
        }
    }
    return true;
}

void noah_qmk_profile_flash_grant_begin(void) {
    noah_profile_flash_store_t *flash = flash_store();

    noah_profile_flash_store_grant_begin(flash, noah_qmk_profile_flash_keyboard_quiet());
}

bool noah_qmk_profile_flash_may_write(void) {
    return noah_profile_flash_store_may_write(flash_store());
}

bool noah_qmk_profile_flash_grant_flashed(void) {
    return noah_profile_flash_store_grant_flashed(flash_store());
}

uint32_t noah_qmk_profile_flash_now_us(void) {
    noah_profile_flash_store_t *flash = flash_store();

    return flash->driver.now_us ? flash->driver.now_us(flash->driver.context) : 0u;
}

noah_profile_flash_diagnostics_t noah_qmk_profile_flash_diagnostics(void) {
    return noah_profile_flash_store_diagnostics(flash_store());
}

void noah_qmk_profile_flash_forget_slots(void) {
    noah_profile_flash_driver_t driver = noah_qmk_profile_flash_driver();

    if (driver.erase_sector) {
        (void)driver.erase_sector(driver.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR);
        (void)driver.erase_sector(driver.context, NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR);
    }
}

// QMK formats logical EEPROM at boot exactly when its magic is not valid,
// and makes the magic valid early in formatting (quantum/eeconfig.c), before
// any userspace hook runs. So the slots are forgotten here, before keyboard
// init: until both header sectors are erased the magic stays invalid, and a
// boot interrupted anywhere in between forgets them again.
void noah_qmk_profile_flash_pre_init(void) {
    if (!noah_qmk_profile_flash_eeprom_valid()) {
        noah_qmk_profile_flash_forget_slots();
    }
}

static void write_u16(uint8_t *target, uint16_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t *target, uint32_t value) {
    target[0] = (uint8_t)value;
    target[1] = (uint8_t)(value >> 8u);
    target[2] = (uint8_t)(value >> 16u);
    target[3] = (uint8_t)(value >> 24u);
}

void noah_qmk_profile_flash_encode_figures(const noah_profile_flash_diagnostics_t *diagnostics, uint8_t physical_half, uint8_t target[NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE]) {
    memset(target, 0, NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE);
    target[0] = NOAH_QMK_PROFILE_FLASH_FIGURES_PRESENT;
    target[1] = physical_half;
    write_u32(&target[2], diagnostics->longest_erase_us);
    write_u32(&target[6], diagnostics->longest_program_us);
    write_u32(&target[10], diagnostics->longest_grant_us);
    write_u32(&target[14], diagnostics->last_save_us);
    write_u32(&target[18], diagnostics->last_save_flash_us);
    write_u16(&target[22], diagnostics->saves);
}

static uint8_t own_physical_half(void) {
    uint8_t origin;

    return noah_qmk_physical_half_origin(&origin) ? origin : 0xFFu;
}

static uint8_t noah_qmk_profile_flash_peer[NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE];

bool noah_qmk_profile_flash_wire_get(uint8_t *data, uint8_t length) {
    noah_profile_flash_diagnostics_t diagnostics;
    bool                             malformed;

    if (!data || length != NOAH_PROFILE_WIRE_V1_REPORT_SIZE || data[0] != NOAH_PROFILE_WIRE_V1_COMMAND_GET || data[1] != NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL || data[2] != NOAH_QMK_PROFILE_FLASH_WIRE_VALUE) {
        return false;
    }
    // Request id nonzero, reserved bytes zero, a known page.
    malformed = data[3] == 0u;
    for (uint8_t index = 5u; index < NOAH_PROFILE_WIRE_V1_REPORT_SIZE; index++) {
        malformed = malformed || data[index] != 0u;
    }
    if (malformed || data[4] >= NOAH_QMK_PROFILE_FLASH_WIRE_PAGES) {
        memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
        data[5] = malformed ? NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED : NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE;
        return true;
    }
    memset(&data[5], 0, NOAH_PROFILE_WIRE_V1_REPORT_SIZE - 5u);
    if (data[4] == 0u) {
        diagnostics = noah_qmk_profile_flash_diagnostics();
        noah_qmk_profile_flash_encode_figures(&diagnostics, own_physical_half(), &data[7]);
    } else if (noah_qmk_profile_flash_peer[0] & NOAH_QMK_PROFILE_FLASH_FIGURES_PRESENT) {
        memcpy(&data[7], noah_qmk_profile_flash_peer, NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE);
    } else {
        data[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
        return true;
    }
    data[5] = NOAH_PROFILE_WIRE_V1_STATUS_OK;
    data[6] = NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE;
    return true;
}

#if defined(SPLIT_KEYBOARD) && defined(SPLIT_TRANSACTION_IDS_USER)

#    include "transactions.h" // QMK

_Static_assert(NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE <= RPC_S2M_BUFFER_SIZE, "a half's flash figures must fit QMK's slave-to-master RPC buffer");

static void noah_qmk_profile_flash_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    noah_profile_flash_diagnostics_t diagnostics;

    (void)request_size;
    (void)request;
    if (!response || response_size != NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE) {
        return;
    }
    diagnostics = noah_qmk_profile_flash_diagnostics();
    noah_qmk_profile_flash_encode_figures(&diagnostics, own_physical_half(), response);
}

void noah_qmk_profile_flash_housekeeping(void) {
    static bool     registered;
    static uint32_t last_pull;
    uint8_t         figures[NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE];

    if (!registered) {
        transaction_register_rpc(PUT_PROFILE_FLASH_DIAGNOSTICS, noah_qmk_profile_flash_rpc);
        registered = true;
        last_pull  = timer_read32();
    }
    if (!is_keyboard_master() || timer_elapsed32(last_pull) < NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS) {
        return;
    }
    last_pull = timer_read32();
    if (transaction_rpc_exec(PUT_PROFILE_FLASH_DIAGNOSTICS, 0u, NULL, sizeof(figures), figures) && (figures[0] & NOAH_QMK_PROFILE_FLASH_FIGURES_PRESENT)) {
        memcpy(noah_qmk_profile_flash_peer, figures, sizeof(figures));
    }
}

#else

void noah_qmk_profile_flash_housekeeping(void) {}

#endif

_Static_assert((uint32_t)NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE == (uint32_t)NOAH_PROFILE_WIRE_V1_PAYLOAD_SIZE, "one half's figures fill one Profile Wire report payload");
