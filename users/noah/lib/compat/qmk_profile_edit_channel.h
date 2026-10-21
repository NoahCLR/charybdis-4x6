// ───────────────────────────────────────────────────────────────────────────
// Profile Wire: Live Edits (D-F32)
// ───────────────────────────────────────────────────────────────────────────
//
// SET 0x20..0x27 admit edits, Save, Revert and the edit mode, and SET 0x28
// restarts both halves (D-F34); GET 0x20, 0x21, 0x22 and 0x27 read the edit
// status, the working copy, the runtime overrides and the edit mode. The
// layouts are in docs/architecture/profile-wire-v1.md, "Live edits".
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    NOAH_PROFILE_EDIT_WIRE_STATUS_LAYOUT  = 1u,
    NOAH_PROFILE_EDIT_WIRE_STATUS_PAGES   = 4u,
    NOAH_PROFILE_EDIT_WIRE_WORKING_LAYOUT = 1u,
    // Inserted bytes per EDIT_DATA report.
    NOAH_PROFILE_EDIT_WIRE_DATA_MAX = 22u,

    // Edit status page 0 flags.
    NOAH_PROFILE_EDIT_WIRE_UNSAVED          = 1u << 0,
    NOAH_PROFILE_EDIT_WIRE_PEER_REQUIRED    = 1u << 1,
    // The other half holds this session and revision with nothing in progress.
    NOAH_PROFILE_EDIT_WIRE_PEER_CONVERGED   = 1u << 2,
    NOAH_PROFILE_EDIT_WIRE_PEER_KNOWN       = 1u << 3,
    // Admission's edit begin would refuse a new edit now.
    NOAH_PROFILE_EDIT_WIRE_EDITS_BLOCKED    = 1u << 4,
    NOAH_PROFILE_EDIT_WIRE_SAVED_COMPILED   = 1u << 5,
    NOAH_PROFILE_EDIT_WIRE_SAVING           = 1u << 6,
    NOAH_PROFILE_EDIT_WIRE_MODE_CONFLICT    = 1u << 7,
    NOAH_PROFILE_EDIT_WIRE_WAITING_SAFE     = 1u << 8,
    // A save on this half waits for a quiet keyboard before it can erase.
    NOAH_PROFILE_EDIT_WIRE_STORAGE_PAUSED   = 1u << 9,
    // The other half's save waits for a quiet keyboard (USB half only).
    NOAH_PROFILE_EDIT_WIRE_PEER_STORAGE_PAUSED = 1u << 10,
    // The other half refused this working copy, edit and whole copy: apart
    // until Revert or an Apply; Save answers BUSY meanwhile.
    NOAH_PROFILE_EDIT_WIRE_PEER_REFUSED     = 1u << 11,
    // Writing the edit mode failed on this half; it is tried again.
    NOAH_PROFILE_EDIT_WIRE_MODE_NOT_STORED  = 1u << 12,
    // A DPI or lighting key's change awaits publication as an edit; a new
    // Apply, edit, Save and Revert are BUSY and a mode change NOT_CLEAN
    // until it is.
    NOAH_PROFILE_EDIT_WIRE_ADJUSTMENT_PENDING = 1u << 13,
    // This half's profile owner stopped: only a restart clears it (D-F34).
    NOAH_PROFILE_EDIT_WIRE_STOPPED          = 1u << 14,
    // The other half answers that its owner stopped (USB half only).
    NOAH_PROFILE_EDIT_WIRE_PEER_STOPPED     = 1u << 15,
    NOAH_PROFILE_EDIT_WIRE_STATUS_FLAGS     = 0xFFFFu,

    // SET 0x28 restart: bytes 5..8 carry "REST", little-endian, so a stray
    // report cannot restart the keyboard. Its reply's byte 17 says whether
    // the other half acknowledged.
    NOAH_PROFILE_EDIT_WIRE_RESTART_CONFIRM = 0x54534552u,
    NOAH_PROFILE_EDIT_WIRE_RESTART_PEER    = 17u,
};

#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
// True when `data` was an edit report; it then holds the answer.
bool noah_qmk_profile_edit_channel(uint8_t *data, uint8_t length);
#else
static inline bool noah_qmk_profile_edit_channel(uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;
    return false;
}
#endif
