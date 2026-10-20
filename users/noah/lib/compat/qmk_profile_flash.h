// ───────────────────────────────────────────────────────────────────────────
// QMK Profile Slot Flash Adapter
// ───────────────────────────────────────────────────────────────────────────
//
// The profile store's storage on this half: the slot region in the RP2040's
// flash (D-F29), one backend instance shared by boot discovery and the owner,
// and its pause diagnostics on Profile Wire.
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../profile/storage/profile_flash_store.h"
#include "qmk_profile_flash_hooks.h"

enum {
    // Profile Wire custom get on channel 0: page 0 this half's flash pauses,
    // page 1 the other half's as last pulled over the split link.
    NOAH_QMK_PROFILE_FLASH_WIRE_VALUE = 0x0Du,
    NOAH_QMK_PROFILE_FLASH_WIRE_PAGES = 2u,
    // Bytes of one half's figures, a full Profile Wire report payload.
    NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE = 25u,
    NOAH_QMK_PROFILE_FLASH_FIGURES_PRESENT = 1u << 0,
    // Minimum interval between requested pulls, including failed attempts.
    // Page-1 reads queue one pull; no reads means no periodic split traffic.
    NOAH_QMK_PROFILE_FLASH_PEER_PULL_MS = 1000u,
    // How long the matrix must be unchanged, with no key held, before a
    // sector erase may pause the keyboard.
    NOAH_QMK_PROFILE_FLASH_QUIET_MS = 50u,
};

// The flash this adapter drives: the RP2040's on the keyboard; a host test
// links its own.
noah_profile_flash_driver_t noah_qmk_profile_flash_driver(void);

// Boot discovery reads alone; the owner gets the writable form.
noah_profile_store_io_t noah_qmk_profile_flash_read_only_io(void);
noah_profile_store_io_t noah_qmk_profile_flash_io(void);

noah_profile_flash_diagnostics_t noah_qmk_profile_flash_diagnostics(void);

// Erases both slots' header sectors, so neither is a boot candidate: what
// formatting QMK's logical EEPROM did while the slots lived in it. Only for
// boot, before anything reads or writes a slot.
void noah_qmk_profile_flash_forget_slots(void);

// Whether QMK's EEPROM magic is valid, so QMK will not format logical EEPROM
// at this boot: QMK's on the keyboard; a host test links its own.
bool noah_qmk_profile_flash_eeprom_valid(void);

bool noah_qmk_profile_flash_keyboard_quiet(void);

// One half's figures as Profile Wire carries them (profile-wire-v1.md,
// "Flash pauses").
void noah_qmk_profile_flash_encode_figures(const noah_profile_flash_diagnostics_t *diagnostics, uint8_t physical_half, uint8_t target[NOAH_QMK_PROFILE_FLASH_FIGURES_SIZE]);
