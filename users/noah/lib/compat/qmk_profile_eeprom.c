// ───────────────────────────────────────────────────────────────────────────
// QMK Live-Profile EEPROM Adapter
// ──────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "qmk_profile_eeprom.h"

#ifdef VIA_ENABLE

#    include <stddef.h>
#    include <stdint.h>

#    include "eeprom.h"

static bool noah_qmk_profile_eeprom_range_valid(uint16_t address, uint16_t length) {
    return length != 0u && address >= NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR && (uint32_t)address + length <= NOAH_PROFILE_STORAGE_LOGICAL_EEPROM_SIZE;
}

static bool noah_qmk_profile_eeprom_read(void *context, uint16_t address, uint8_t *target, uint16_t length) {
    (void)context;

    if (!target || !noah_qmk_profile_eeprom_range_valid(address, length)) {
        return false;
    }
    eeprom_read_block(target, (const void *)(uintptr_t)address, length);
    return true;
}

#endif

noah_profile_store_io_t noah_qmk_profile_eeprom_read_only_io(void) {
#ifdef VIA_ENABLE
    return (noah_profile_store_io_t){
        .read    = noah_qmk_profile_eeprom_read,
        .write   = NULL,
        .context = NULL,
    };
#else
    return (noah_profile_store_io_t){0};
#endif
}
