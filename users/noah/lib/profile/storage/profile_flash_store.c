// ───────────────────────────────────────────────────────────────────────────
// Profile Slot Flash Backend
// ───────────────────────────────────────────────────────────────────────────

#include "profile_flash_store.h"

#include <stddef.h>
#include <string.h>

enum {
    PAGE_SIZE   = NOAH_PROFILE_STORAGE_PAGE_SIZE,
    SECTOR_SIZE = NOAH_PROFILE_STORAGE_SECTOR_SIZE,
    // The store's marker byte, the header's last.
    MARKER = NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE - NOAH_PROFILE_STORAGE_COMMIT_MARKER_SIZE,
    // Payload bytes sit this much further into the slot on flash than in
    // the store's addresses.
    PAYLOAD_SHIFT = NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_FLASH_OFFSET - NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE,
};

#define NO_PAGE UINT32_MAX

static uint32_t now_us(const noah_profile_flash_store_t *flash) {
    return flash->driver.now_us ? flash->driver.now_us(flash->driver.context) : 0u;
}

static void account(noah_profile_flash_store_t *flash, uint32_t elapsed) {
    flash->grant_us += elapsed;
    if (flash->grant_operations < UINT16_MAX) {
        flash->grant_operations++;
    }
    if (flash->save_active) {
        flash->save_flash_us += elapsed;
    }
}

static bool erase_sector(noah_profile_flash_store_t *flash, uint32_t offset) {
    uint32_t started = now_us(flash);
    bool     ok      = flash->driver.erase_sector(flash->driver.context, offset);
    uint32_t elapsed = now_us(flash) - started;

    account(flash, elapsed);
    if (elapsed > flash->diagnostics.longest_erase_us) {
        flash->diagnostics.longest_erase_us = elapsed;
    }
    if (flash->diagnostics.erases != UINT16_MAX) {
        flash->diagnostics.erases++;
    }
    return ok;
}

static bool program_page(noah_profile_flash_store_t *flash) {
    uint32_t started = now_us(flash);
    bool     ok      = flash->driver.program_page(flash->driver.context, flash->page_offset, flash->page);
    uint32_t elapsed = now_us(flash) - started;

    account(flash, elapsed);
    if (elapsed > flash->diagnostics.longest_program_us) {
        flash->diagnostics.longest_program_us = elapsed;
    }
    if (flash->diagnostics.programs != UINT32_MAX) {
        flash->diagnostics.programs++;
    }
    return ok;
}

static bool flush(noah_profile_flash_store_t *flash) {
    bool ok = true;

    if (flash->page_dirty) {
        ok = program_page(flash);
    }
    flash->page_dirty  = false;
    flash->page_offset = NO_PAGE;
    return ok;
}

static bool erase_next(noah_profile_flash_store_t *flash) {
    if (!erase_sector(flash, flash->erased_end)) {
        flash->failed = true;
        return false;
    }
    flash->erased_end += SECTOR_SIZE;
    return true;
}

// Fewer than `headroom` erased bytes lie ahead of the save's writes.
static bool erase_due(const noah_profile_flash_store_t *flash, uint32_t headroom) {
    return flash->range_active && !flash->failed && flash->erased_end < flash->erase_end && flash->erased_end < flash->frontier + headroom;
}

static uint32_t slot_base(uint32_t address) {
    return address < NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR ? NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR : NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR;
}

// Where the run of storage bytes starting at `address` lies on flash, and how
// many of `length` bytes it covers; the marker is a run of its own. False for
// bytes past a slot's largest payload, which have no place on flash.
static bool locate(uint32_t address, uint32_t length, uint32_t *flash_offset, uint32_t *run) {
    uint32_t base   = slot_base(address);
    uint32_t offset = address - base;
    uint32_t limit;

    if (offset < MARKER) {
        *flash_offset = address;
        limit         = MARKER - offset;
    } else if (offset == MARKER) {
        *flash_offset = base + NOAH_PROFILE_STORAGE_SLOT_MARKER_CELL_FLASH_OFFSET;
        limit         = 1u;
    } else {
        if (offset + PAYLOAD_SHIFT >= NOAH_PROFILE_STORAGE_SLOT_SIZE) {
            return false;
        }
        *flash_offset = address + PAYLOAD_SHIFT;
        limit         = NOAH_PROFILE_STORAGE_SLOT_SIZE - (offset + PAYLOAD_SHIFT);
    }
    *run = length < limit ? length : limit;
    return true;
}

// Flash bytes as the save sees them: sectors it has not erased yet read as
// the erased bytes they are about to become, and the page buffer is merged.
static void read_flash(const noah_profile_flash_store_t *flash, uint32_t offset, uint8_t *target, uint32_t length) {
    flash->driver.read(flash->driver.context, offset, target, length);
    for (uint32_t index = 0u; index < length; index++) {
        uint32_t at = offset + index;

        if (flash->range_active && at >= flash->erased_end && at < flash->erase_end) {
            target[index] = 0xFFu;
        }
        if (flash->page_dirty && at - flash->page_offset < PAGE_SIZE) {
            target[index] &= flash->page[at - flash->page_offset];
        }
    }
}

// A span of one store call: inside one slot and the bytes it can hold.
static bool region_span(uint32_t address, uint32_t length) {
    uint32_t last = address + length - 1u;
    uint32_t offset;
    uint32_t run;

    return length != 0u && length <= NOAH_PROFILE_STORE_IO_CHUNK_MAX && address < NOAH_PROFILE_STORAGE_REGION_SIZE && length <= NOAH_PROFILE_STORAGE_REGION_SIZE - address && slot_base(address) == slot_base(last) && locate(last, 1u, &offset, &run);
}

static bool flash_read(void *context, noah_profile_storage_address_t address, uint8_t *target, uint32_t length) {
    const noah_profile_flash_store_t *flash = context;
    uint32_t                          offset;
    uint32_t                          run;

    if (!flash || !flash->driver.read || !target || !region_span(address, length)) {
        return false;
    }
    while (length != 0u) {
        if (!locate(address, length, &offset, &run)) {
            return false;
        }
        if (address - slot_base(address) == MARKER) {
            // The marker is its cells' bitwise AND.
            uint8_t value = 0xFFu;

            for (uint32_t cell = 0u; cell < NOAH_PROFILE_STORAGE_SLOT_MARKER_CELLS; cell++) {
                uint8_t byte;

                read_flash(flash, offset + cell * PAGE_SIZE, &byte, 1u);
                value &= byte;
            }
            *target = value;
        } else {
            read_flash(flash, offset, target, run);
        }
        address += run;
        target += run;
        length -= run;
    }
    return true;
}

// Collects `length` bytes at flash `offset` (inside one page or running on
// into the next) into the page buffer.
static bool write_flash(noah_profile_flash_store_t *flash, uint32_t offset, const uint8_t *source, uint32_t length) {
    while (length != 0u) {
        uint32_t page  = offset & ~(uint32_t)(PAGE_SIZE - 1u);
        uint32_t index = offset - page;
        uint32_t piece = PAGE_SIZE - index < length ? PAGE_SIZE - index : length;

        if (page != flash->page_offset) {
            if (!flush(flash)) {
                return false;
            }
            // The pause policy keeps erased flash ahead of the writer; a
            // write that outruns it still never lands on unerased flash.
            while (flash->erased_end <= offset) {
                if (!erase_next(flash)) {
                    return false;
                }
            }
            memset(flash->page, 0xFF, sizeof(flash->page));
            flash->page_offset = page;
        }
        for (uint32_t byte = 0u; byte < piece; byte++) {
            flash->page[index + byte] &= source[byte];
        }
        flash->page_dirty = true;
        offset += piece;
        source += piece;
        length -= piece;
        if (offset > flash->frontier) {
            flash->frontier = offset;
        }
        // A full page goes to flash at once; a partial one waits for the
        // next page or a sync.
        if (index + piece == PAGE_SIZE && !flush(flash)) {
            return false;
        }
    }
    return true;
}

static bool flash_write(void *context, noah_profile_storage_address_t address, const uint8_t *source, uint32_t length) {
    noah_profile_flash_store_t *flash = context;
    uint32_t                    offset;
    uint32_t                    run;

    if (!flash || flash->failed || !source || !region_span(address, length) || !flash->range_active || address < flash->range_start || length > flash->range_end - address || address > flash->range_end) {
        return false;
    }
    while (length != 0u) {
        if (!locate(address, length, &offset, &run)) {
            return false;
        }
        if (address - slot_base(address) == MARKER) {
            // Each marker step programs the next cell. A save abandoned
            // before its header sector was erased never touched the slot, so
            // its invalid marker has nothing to mark.
            if (flash->marker_steps == NOAH_PROFILE_STORAGE_SLOT_MARKER_CELLS) {
                return false;
            }
            if (flash->erased_end != flash->erase_start && !write_flash(flash, offset + flash->marker_steps * PAGE_SIZE, source, 1u)) {
                return false;
            }
            flash->marker_steps++;
        } else if (!write_flash(flash, offset, source, run)) {
            return false;
        }
        address += run;
        source += run;
        length -= run;
    }
    return true;
}

// Takes a slot for a save without touching flash: until its header sector is
// erased the slot holds what it held, and every write waits for that erase.
static bool flash_reset(void *context, noah_profile_storage_address_t address, uint32_t length) {
    noah_profile_flash_store_t *flash = context;

    if (!flash || (address != NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR && address != NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR) || length < NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE || length - NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE > NOAH_PROFILE_STORAGE_SLOT_PAYLOAD_MAX) {
        return false;
    }
    // Whatever an abandoned save left buffered belongs to no slot any more.
    flash->page_dirty      = false;
    flash->page_offset     = NO_PAGE;
    flash->failed          = false;
    flash->marker_steps    = 0u;
    flash->range_active    = true;
    flash->range_start     = address;
    flash->range_end       = address + length;
    flash->erase_start     = address;
    flash->erase_end       = address + length + PAYLOAD_SHIFT;
    flash->erased_end      = address;
    flash->frontier        = address;
    flash->save_active     = true;
    flash->save_started_us = now_us(flash);
    flash->save_flash_us   = 0u;
    if (flash->diagnostics.saves != UINT16_MAX) {
        flash->diagnostics.saves++;
    }
    return true;
}

static bool flash_sync(void *context) {
    noah_profile_flash_store_t *flash = context;
    bool                        ok;

    if (!flash) {
        return false;
    }
    ok = !flash->failed && flush(flash);
    if (flash->save_active) {
        flash->diagnostics.last_save_us       = now_us(flash) - flash->save_started_us;
        flash->diagnostics.last_save_flash_us = flash->save_flash_us;
    }
    return ok;
}

void noah_profile_flash_store_init(noah_profile_flash_store_t *flash, noah_profile_flash_driver_t driver) {
    if (!flash) {
        return;
    }
    memset(flash, 0, sizeof(*flash));
    flash->driver      = driver;
    flash->page_offset = NO_PAGE;
}

noah_profile_store_io_t noah_profile_flash_store_io(noah_profile_flash_store_t *flash) {
    if (!flash || !flash->driver.read || !flash->driver.erase_sector || !flash->driver.program_page) {
        return (noah_profile_store_io_t){0};
    }
    return (noah_profile_store_io_t){
        .read    = flash_read,
        .write   = flash_write,
        .reset   = flash_reset,
        .sync    = flash_sync,
        .context = flash,
    };
}

noah_profile_store_io_t noah_profile_flash_store_read_only_io(noah_profile_flash_store_t *flash) {
    if (!flash || !flash->driver.read) {
        return (noah_profile_store_io_t){0};
    }
    return (noah_profile_store_io_t){
        .read    = flash_read,
        .context = flash,
    };
}

bool noah_profile_flash_store_erase_pending(const noah_profile_flash_store_t *flash) {
    return flash && erase_due(flash, SECTOR_SIZE);
}

void noah_profile_flash_store_grant_begin(noah_profile_flash_store_t *flash, bool quiet) {
    if (!flash) {
        return;
    }
    if (flash->grant_us > flash->diagnostics.longest_grant_us) {
        flash->diagnostics.longest_grant_us = flash->grant_us;
    }
    flash->grant_us         = 0u;
    flash->grant_operations = 0u;
    // Erase a sector ahead, so typing can go on while the writer fills it.
    if (quiet && noah_profile_flash_store_erase_pending(flash)) {
        (void)erase_next(flash);
    }
}

bool noah_profile_flash_store_may_write(const noah_profile_flash_store_t *flash) {
    // After a failed erase the writer runs, so the store learns of it.
    return flash && (flash->failed || (!erase_due(flash, PAGE_SIZE) && flash->grant_us < NOAH_PROFILE_FLASH_GRANT_BUDGET_US));
}

bool noah_profile_flash_store_grant_flashed(const noah_profile_flash_store_t *flash) {
    return flash && flash->grant_operations != 0u;
}

noah_profile_flash_diagnostics_t noah_profile_flash_store_diagnostics(const noah_profile_flash_store_t *flash) {
    noah_profile_flash_diagnostics_t diagnostics = {0};

    if (flash) {
        diagnostics = flash->diagnostics;
        if (flash->grant_us > diagnostics.longest_grant_us) {
            diagnostics.longest_grant_us = flash->grant_us;
        }
    }
    return diagnostics;
}

_Static_assert(NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR % NOAH_PROFILE_STORAGE_SECTOR_SIZE == 0u && NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR % NOAH_PROFILE_STORAGE_SECTOR_SIZE == 0u, "each slot starts on an erase sector");
_Static_assert((NOAH_PROFILE_STORAGE_PAGE_SIZE & (NOAH_PROFILE_STORAGE_PAGE_SIZE - 1u)) == 0u, "program pages are a power of two");
_Static_assert(NOAH_PROFILE_STORE_IO_CHUNK_MAX <= NOAH_PROFILE_STORAGE_PAGE_SIZE, "one store write spans at most two pages");
