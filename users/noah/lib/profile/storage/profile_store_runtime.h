// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Boot Owner
// ────────────────────────────────────────────────────────────────────────
#pragma once

#include "profile_store.h"
#include "../schema/profile_compiled_defaults_v1.h"
#include "profile_store_runtime_hooks.h"
#include "../runtime/profile_owner.h"
#include "../schema/profile_settings_v1.h"

typedef enum {
    NOAH_PROFILE_STORE_RUNTIME_UNINITIALIZED = 0u,
    NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING,
    NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK,
    NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND,
    NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT,
    NOAH_PROFILE_STORE_RUNTIME_STORAGE_ERROR,
    NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR,
} noah_profile_store_runtime_state_t;

noah_profile_store_runtime_state_t noah_profile_store_runtime_state(void);
noah_profile_store_result_t        noah_profile_store_runtime_discovery_result(void);
const noah_profile_store_record_t *noah_profile_store_runtime_committed(void);

// Bounded read of the committed payload for host readback. Refuses any range
// outside the committed record, so a malformed host request cannot walk
// storage. Returns false when nothing is committed.
bool noah_profile_store_runtime_read_committed(uint32_t offset, uint8_t *target, uint32_t length);

// The compiled defaults this firmware was built with. A keyboard with no
// committed profile is still running these, so the host can show what the
// board actually does rather than an empty editor. The canonical bytes are a
// virtual view over authored const data; no payload-sized buffer is retained.
bool noah_profile_store_runtime_compiled_metadata(noah_profile_compiled_v1_metadata_t *metadata);
bool noah_profile_store_runtime_read_compiled(uint32_t offset, uint8_t *target, uint32_t length);

// Narrow engineering bridge. Ordinary/read-only builds return false; the
// static runtime owner is never exposed for direct mutation.
bool noah_profile_store_runtime_owner_status(noah_profile_owner_status_t *status);
bool noah_profile_store_runtime_candidate_status(noah_profile_candidate_v1_status_t *status);
bool noah_profile_store_runtime_peer_transfer(noah_profile_candidate_v1_peer_status_t *peer);
bool noah_profile_store_runtime_candidate_receive(uint8_t *frame, size_t length);

// The committed profile's keyboard name (D-F25), read before the owner starts
// so USB can enumerate with it. It borrows the runtime's not yet started store
// and leaves it cleared; once the runtime has started it returns false. Boot
// selection checks the profile's checksum and compatibility, but not the peer:
// the activated profile is authoritative, so a different name there is only
// corrected by a later USB restart. Empty, and false, when nothing usable is
// committed.
bool noah_profile_store_runtime_boot_keyboard_name(noah_profile_keyboard_name_t *name);
// The keyboard name of the profile `store` would boot, through its IO and
// compatibility. The boot read above and tests use it.
bool noah_profile_store_runtime_read_keyboard_name(noah_profile_store_t *store, noah_profile_keyboard_name_t *name);
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
// Live edits, Save, Revert and the edit mode (D-F32), for the Profile Wire
// channel; see profile_owner.h. INVALID_STATE without a working owner.
bool noah_profile_store_runtime_pending_adjustment(uint8_t setting, uint32_t *value);
bool noah_profile_store_runtime_adjust(uint8_t setting, uint32_t value);
noah_profile_working_result_t noah_profile_store_runtime_edit_begin(uint16_t transaction_id, uint32_t session, uint32_t revision, uint8_t splice_count, uint32_t insert_total, uint8_t flags);
noah_profile_working_result_t noah_profile_store_runtime_edit_splice(uint16_t transaction_id, uint8_t index, const noah_profile_working_splice_t *splice);
noah_profile_working_result_t noah_profile_store_runtime_edit_data(uint16_t transaction_id, uint32_t offset, const uint8_t *bytes, uint32_t count);
noah_profile_working_result_t noah_profile_store_runtime_edit_commit(uint16_t transaction_id, uint32_t crc32);
noah_profile_working_result_t noah_profile_store_runtime_edit_abort(uint16_t transaction_id);
noah_profile_working_result_t noah_profile_store_runtime_revert(uint16_t transaction_id, uint32_t session, uint32_t revision);
noah_profile_working_result_t noah_profile_store_runtime_save(uint16_t transaction_id, uint32_t session, uint32_t revision);
noah_profile_working_result_t noah_profile_store_runtime_set_edit_mode(uint8_t mode);
bool                          noah_profile_store_runtime_working_status(noah_profile_owner_working_status_t *status);
// Reads the profile now active: the working copy, or the saved profile.
bool noah_profile_store_runtime_read_working(uint32_t offset, uint8_t *target, uint32_t length);
#endif

// The owner's USB restart check (noah_profile_owner_usb_restart_safe); false
// without a working owner.
bool noah_profile_store_runtime_usb_restart_safe(void);
