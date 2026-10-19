// ───────────────────────────────────────────────────────────────────────────
// Profile Slot Flash Backend
// ───────────────────────────────────────────────────────────────────────────
//
// The store's I/O over NOR flash (D-F29). The two slots are a fixed region of
// flash, read in place: no RAM buffer holds a slot. Writes collect in one
// 256-byte page buffer and reach flash as whole-page programs, and no page is
// programmed twice between erases: the store's header, each marker step and
// the payload each have pages of their own (profile_storage_layout.h). The
// backend maps the store's addresses onto that layout.
//
// Pausing the keyboard is the cost of every flash operation: the RP2040 runs
// them with interrupts off and nothing else running (qmk_profile_flash.c).
// The pause policy:
//
// - A sector erase (tens of milliseconds, at worst hundreds) runs only from
//   grant_begin, at most one per durable-I/O grant, and only when the caller
//   says the keyboard is quiet. A save's reset erases nothing; its sectors are
//   erased ahead of its writes, the header sector first.
// - The store's writer may run in a grant only while erased flash lies at
//   least a page ahead of its writes and the grant has not used its flash
//   time budget (may_write); so an erase never shares a grant with the
//   writer, and a save waits while the keyboard is busy. One grant's writes
//   (at most one split chunk, 106 bytes) program at most one page.
//
// The backend measures the pauses so a keyboard session can read them
// (diagnostics). Host tests drive a fake that can cut power at any operation.
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "profile_store.h"

enum {
    // The flash time after which a grant takes no further flash work: an
    // erase always exceeds it, a page program (under a millisecond typical,
    // 3 ms at worst) never starts past it.
    NOAH_PROFILE_FLASH_GRANT_BUDGET_US = 2000u,
};

typedef struct {
    // Erases the 4 KiB sector at `offset` into the slot region.
    bool (*erase_sector)(void *context, uint32_t offset);
    // Programs the 256-byte page at `offset`: NOR semantics, every bit only
    // ever clears, so 0xFF bytes leave the flash unchanged.
    bool (*program_page)(void *context, uint32_t offset, const uint8_t *page);
    // A microsecond clock for the diagnostics; it keeps counting while a
    // flash operation runs.
    uint32_t (*now_us)(void *context);
    // Reads `length` bytes at `offset` as the CPU sees them: in place
    // through XIP on the RP2040, never from a RAM copy of a slot.
    void (*read)(void *context, uint32_t offset, uint8_t *target, uint32_t length);
    void *context;
} noah_profile_flash_driver_t;

// Pause figures for this half since boot, in microseconds. "Grant" is one
// durable-I/O grant: the flash time of one scan's storage step.
typedef struct {
    uint32_t longest_erase_us;
    uint32_t longest_program_us;
    uint32_t longest_grant_us;
    // The last save: from its reset to its last sync (the commit marker on
    // success), and the flash time inside it.
    uint32_t last_save_us;
    uint32_t last_save_flash_us;
    uint16_t saves;
    uint16_t erases;
    uint32_t programs;
} noah_profile_flash_diagnostics_t;

typedef struct {
    noah_profile_flash_driver_t      driver;
    uint8_t                          page[NOAH_PROFILE_STORAGE_PAGE_SIZE];
    uint32_t                         page_offset;
    bool                             page_dirty;
    bool                             range_active;
    bool                             save_active;
    // A flash operation outside the writer's call failed: the save's next
    // write or sync fails too.
    bool                             failed;
    uint8_t                          marker_steps;
    // The store's range, in storage addresses.
    uint32_t                         range_start;
    uint32_t                         range_end;
    // The same range on flash; sectors in [erase_start, erased_end) are
    // erased for this save, and writes have reached `frontier`.
    uint32_t                         erase_start;
    uint32_t                         erase_end;
    uint32_t                         erased_end;
    uint32_t                         frontier;
    uint32_t                         save_started_us;
    uint32_t                         save_flash_us;
    uint32_t                         grant_us;
    // Erases and programs in this grant.
    uint16_t                         grant_operations;
    noah_profile_flash_diagnostics_t diagnostics;
} noah_profile_flash_store_t;

void noah_profile_flash_store_init(noah_profile_flash_store_t *flash, noah_profile_flash_driver_t driver);
// Store I/O over `flash`: read, write, reset and sync. The read-only form
// gives boot discovery reads alone.
noah_profile_store_io_t noah_profile_flash_store_io(noah_profile_flash_store_t *flash);
noah_profile_store_io_t noah_profile_flash_store_read_only_io(noah_profile_flash_store_t *flash);
// Called once at the start of every durable-I/O grant: closes the previous
// grant's flash time into the longest-grant figure, then, if `quiet` and a
// save has fewer than a sector of erased flash ahead of its writes, erases
// the next sector.
void noah_profile_flash_store_grant_begin(noah_profile_flash_store_t *flash, bool quiet);
// Whether the store's writer may run in this grant (the pause policy above).
bool                             noah_profile_flash_store_may_write(const noah_profile_flash_store_t *flash);
// Whether this grant has erased or programmed anything: a grant may give the
// store more local steps only until it has, so a grant still erases at most
// one sector or programs at most one page (D-F29).
bool                             noah_profile_flash_store_grant_flashed(const noah_profile_flash_store_t *flash);
noah_profile_flash_diagnostics_t noah_profile_flash_store_diagnostics(const noah_profile_flash_store_t *flash);
