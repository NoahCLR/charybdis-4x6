// ──────────────────────────────────────────────────────────────────────────
// Single Live-Profile Runtime Owner
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "effective_key_behavior_runtime.h"
#include "effective_profile_provider.h"
#include "effective_rgb_runtime.h"
#include "effective_combo_runtime.h"
#include "profile_activation_policy.h"
#include "../schema/profile_compiled_defaults_v1.h"
#include "../schema/profile_settings_v1.h"
#include "profile_edit_mode.h"
#include "profile_working_copy.h"
#include "../split/profile_split_reconciler.h"
#include "../split/profile_working_sync.h"
#include "../storage/profile_candidate_store_backend.h"
#include "../storage/profile_candidate_transaction.h"
#include "../storage/profile_peer_store_backend.h"
#include "../storage/profile_store.h"

enum {
    // Engineering-state regression policy for one 32-bit keyboard half. This
    // is not an RP2040 physical-SRAM limit; linked target gates remain the
    // authority for the concrete firmware artifact. D-F14's native combo
    // table (128 rows of sixteen inputs) links the owner at 10,552 bytes;
    // with D-F28's 32-bit profile offsets it links at 11,328.
    // D-F32's working copy (its edit plan, validator and the saved snapshot
    // Revert returns to), its split sync and the save that copies it to flash
    // raise it by one reviewed increment to 16,384.
    NOAH_PROFILE_OWNER_STATE_BUDGET_32BIT = 16384u,
    // Ordinary host staging expires after 15 seconds without host-owned work.
    // Split preparation has a separate no-progress window: it bounds the
    // longest stretch without transfer or correlated peer-phase progress, not
    // the whole copy. Repeated BUSY replies never extend the deadline.
    NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS             = 15000u,
    NOAH_PROFILE_OWNER_AUTO_SAVE_QUIET_MS           = 3000u,
    NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS = 60000u,
};

typedef enum {
    NOAH_PROFILE_OWNER_UNINITIALIZED = 0u,
    NOAH_PROFILE_OWNER_VALIDATING_COMPILED,
    NOAH_PROFILE_OWNER_DISCOVERING,
    NOAH_PROFILE_OWNER_ADOPTING_COMMITTED,
    NOAH_PROFILE_OWNER_RECONCILING_COMMITTED,
    NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED,
    NOAH_PROFILE_OWNER_READY_COMPILED,
    NOAH_PROFILE_OWNER_READY_VALIDATED,
    NOAH_PROFILE_OWNER_GENERATION_CONFLICT,
    NOAH_PROFILE_OWNER_DURABILITY_UNKNOWN,
    NOAH_PROFILE_OWNER_COMPILED_ERROR,
    NOAH_PROFILE_OWNER_STORAGE_ERROR,
    NOAH_PROFILE_OWNER_INTEGRATION_ERROR,
    NOAH_PROFILE_OWNER_POSTCOMMIT_AUTHORITY_LOST,
    NOAH_PROFILE_OWNER_CONCURRENT_COMMIT,
} noah_profile_owner_state_t;

typedef struct {
    noah_profile_store_io_t               store_io;
    noah_profile_split_exchange_fn        split_exchange;
    void                                 *split_transport_context;
    uint8_t                               origin_half;
    bool                                  peer_required;
    // The working copy's buffer (D-F32): a slot payload's worth of RAM. Live
    // edits are refused without one.
    uint8_t                              *working_bytes;
    uint32_t                              working_capacity;
    // Names this boot's working copy; any value that differs between boots.
    uint32_t                              working_session;
    // The working copy's own split RPC, from the USB half.
    noah_profile_split_exchange_fn        working_exchange;
    void                                 *working_transport_context;
    noah_profile_edit_mode_storage_t      edit_mode_storage;
    // Whether this half's storage step is not running just now (a save
    // waiting for a quiet keyboard). Optional; told to the other half.
    bool (*storage_paused)(void *context);
    void *storage_paused_context;
    // No held key and a quiet matrix. Missing means automatic saves disabled.
    bool (*keyboard_quiet)(void *context);
    void *keyboard_quiet_context;
    // The authored settings a profile without a settings domain inherits. An
    // adjustment key's change needs a settings domain to live in, so the
    // owner copies these into the working copy first. Missing: such a profile
    // cannot take adjustments (they stay local feedback and are not stored).
    uint16_t (*settings_defaults_length)(void);
    uint8_t (*settings_defaults_byte)(uint16_t offset);
} noah_profile_owner_config_t;

typedef enum {
    NOAH_PROFILE_OWNER_SAVE_IDLE = 0u,
    NOAH_PROFILE_OWNER_SAVE_CHECKSUM,
    NOAH_PROFILE_OWNER_SAVE_BEGIN,
    NOAH_PROFILE_OWNER_SAVE_REUSE,
    NOAH_PROFILE_OWNER_SAVE_VALIDATE,
    NOAH_PROFILE_OWNER_SAVE_COMMIT,
    NOAH_PROFILE_OWNER_SAVE_WAIT,
    // The working copy's checksums are the saved profile's: its bytes are
    // compared with it, and if they are the same nothing is written.
    NOAH_PROFILE_OWNER_SAVE_SAME,
    // The same bytes: the working copy gives way to the saved profile.
    NOAH_PROFILE_OWNER_SAVE_SETTLE,
} noah_profile_owner_save_phase_t;

// Save (D-F32): the working copy written as a new generation through the
// ordinary candidate path, made entirely of REUSE from the working copy, so
// the other half copies it from its own working copy too.
typedef struct {
    uint32_t offset;
    uint32_t length;
    uint32_t crc32;
    uint32_t digest;
    uint16_t transaction_id;
    uint16_t last_transaction_id;
    uint8_t  phase;
    uint8_t  domain_mask;
    uint8_t  last_result; // noah_profile_working_result_t
    bool     automatic;
    bool     last_automatic;
    uint32_t keyboard_saves;
    uint32_t keyboard_revision;
    uint32_t keyboard_generation;
    uint8_t  last_error;  // noah_profile_candidate_v1_error_id_t of a failed save
} noah_profile_owner_save_t;

// The adjustment scalars' latest values (profile_settings_v1.h), coalesced
// while another edit, a Save or the other half holds the profile. They never
// change the bytes a save is reading.
typedef struct {
    uint32_t values[NOAH_SETTINGS_ADJUSTMENTS]; // by adjustment index
    uint32_t quiet_since;
    uint32_t observed_revision;
    uint16_t sequence;
    uint16_t edit_transaction;
    uint8_t pending; // bit i: values[i] awaits publication
    bool apply_adjustments_only;
    bool attempted;
} noah_profile_owner_auto_save_t;

// The compiled validator is needed only before the candidate backend exists,
// so both lifetimes deliberately share one stable union. The backend address
// never changes after it is constructed.
typedef union {
    noah_profile_validator_v1_t            compiled_validator;
    noah_profile_candidate_store_backend_t candidate_backend;
} noah_profile_owner_staging_t;

typedef struct {
    noah_profile_owner_config_t               config;
    noah_profile_compiled_v1_t                compiled;
    noah_profile_reader_t                     compiled_reader;
    noah_profile_validator_v1_compatibility_t compatibility;
    noah_profile_validator_v1_runtime_t       candidate_runtime;
    noah_effective_profile_snapshot_t         compiled_snapshot;
    noah_profile_owner_staging_t              staging;
    noah_profile_store_t                      store;
    noah_effective_profile_provider_t         provider;
    noah_profile_activation_policy_t          activation_policy;
    noah_effective_key_behavior_runtime_t     key_behaviors;
    noah_effective_rgb_runtime_t              rgb;
#ifdef COMBO_ENABLE
    noah_effective_combo_runtime_t combos;
#endif
    noah_profile_candidate_transaction_t host_transaction;
    noah_profile_peer_store_backend_t    peer_store;
    noah_profile_split_reconciler_t      reconciler;
    // Edits are held to what a host's candidate is held to.
    noah_profile_validator_v1_compatibility_t edit_compatibility;
    noah_profile_working_copy_t          working;
    noah_profile_working_sync_t          working_sync;
    noah_profile_owner_save_t            save;
    noah_profile_owner_auto_save_t       auto_save;
    uint32_t                             now_ms;
    // The host's last frame of the edit it is sending: an edit not finished
    // within the host timeout is aborted, as an abandoned Apply is.
    uint32_t                             edit_activity_at;
    noah_profile_split_descriptor_t      committed_descriptor;
    noah_profile_split_descriptor_t      host_barrier_descriptor;
    noah_profile_candidate_v1_error_t    adoption_error;
    noah_profile_candidate_v1_error_id_t host_cancel_reason;
    noah_profile_store_result_t          discovery_result;
    noah_profile_owner_state_t           state;
    uint32_t                             host_last_activity_at;
    uint32_t                             host_barrier_progress_offset;
    uint8_t                              scheduler_cursor;
    // The subsystem whose step last worked: a repeated step in the same
    // durable-I/O grant visits it alone (D-F29, "Pacing").
    uint8_t                              scheduler_last;
    uint8_t                              host_barrier_progress_phase;
    bool                                 boot_activation_started : 1;
    bool                                 peer_activation_started : 1;
    bool                                 host_activity_known : 1;
    bool                                 descriptor_readable : 1;
    bool                                 split_initialized : 1;
    bool                                 runtimes_installed : 1;
    bool                                 host_barrier_descriptor_known : 1;
    bool                                 host_barrier_started : 1;
    bool                                 host_barrier_local_published : 1;
    bool                                 host_barrier_peer_commit_authorized : 1;
    bool                                 host_barrier_waiting_boundary : 1;
    bool                                 host_barrier_peer_confirmed : 1;
    bool                                 host_cancel_pending : 1;
    bool                                 working_ready : 1;
    bool                                 master : 1;
    bool                                 scan_again : 1;
} noah_profile_owner_t;

// Protocol-neutral, caller-owned observation of the complete live-profile
// graph. The Profile Wire channel maps this snapshot into its status pages;
// no mutable owner pointer crosses the runtime boundary.
typedef struct {
    noah_profile_owner_state_t           owner_state;
    noah_effective_profile_identity_t    active;
    noah_effective_profile_identity_t    pending;
    noah_profile_split_descriptor_t      committed;
    noah_profile_split_descriptor_t      peer;
    noah_profile_candidate_v1_status_t   candidate;
    noah_profile_split_authority_state_t authority_state;
    uint32_t                             compiled_default_digest;
    uint32_t                             action_abi_digest;
    uint32_t                             safe_boundary_reason_mask;
    uint16_t                             last_committed_transaction_id;
    uint8_t                              supported_domain_mask;
    bool                                 provider_known;
    bool                                 has_pending;
    bool                                 has_committed;
    bool                                 candidate_pending;
    bool                                 peer_known;
    bool                                 peer_converged;
    bool                                 transfer_pending;
    bool                                 peer_cleanup_pending;
    // Nothing is committed because a slot holds an earlier store format or retired test layout,
    // which is never read (D-F28): the keyboard runs its compiled defaults.
    bool                                 stored_earlier_format;
} noah_profile_owner_status_t;

// The save's copy to the other half, for candidate status page 1.
bool noah_profile_owner_peer_transfer(const noah_profile_owner_t *owner, noah_profile_candidate_v1_peer_status_t *peer);

// Initializes metadata and begins incremental validation of the compiled
// canonical profile. It performs no EEPROM or split-transport I/O.
bool noah_profile_owner_init(noah_profile_owner_t *owner, const noah_profile_owner_config_t *config);

// One scan grant performs at most one validator/store/provider operation or
// one split exchange. `master` is current transport role; `now_ms` is the
// caller's wrapping millisecond clock.
bool noah_profile_owner_scan(noah_profile_owner_t *owner, bool master, uint32_t now_ms);
// Whether this half has storage work in progress that its steps move on
// locally: boot validation and discovery, this half's candidate (a save or an
// Apply), the copy it receives from the other half, or the working copy
// loading or checking an edit. The durable-I/O scheduler repeats steps within
// one grant only then (D-F29, "Pacing").
bool noah_profile_owner_storage_busy(const noah_profile_owner_t *owner);
// The next scan repeats a step within the same grant: it visits the subsystem
// whose step last worked first, so the code it runs stays the same (on the
// keyboard a step through every subsystem cost about 0.2 ms, mostly fetching
// code from flash), and the ordinary round only if that one has nothing.
void noah_profile_owner_scan_again(noah_profile_owner_t *owner, bool again);

// Callback-side candidate admission remains a bounded decode/mailbox copy.
// Advertising or routing this surface is a separate capability decision.
bool noah_profile_owner_receive(noah_profile_owner_t *owner, uint8_t *frame, size_t length);

noah_profile_owner_state_t         noah_profile_owner_state(const noah_profile_owner_t *owner);
// The owner stopped (D-F34): a conflict, durability, compiled, storage,
// integration or authority error, which nothing but a restart leaves (a fresh
// read of flash at boot). It saves, edits, reverts and follows the other half
// no more; its status stays readable. A slow save is never one of these.
bool                               noah_profile_owner_stopped(const noah_profile_owner_t *owner);
noah_profile_store_result_t        noah_profile_owner_discovery_result(const noah_profile_owner_t *owner);
const noah_profile_store_record_t *noah_profile_owner_committed(const noah_profile_owner_t *owner);

// Read-only view of the backing store, for bounded host readback of the
// committed payload. Callers must not mutate through it.
const noah_profile_store_t           *noah_profile_owner_store(const noah_profile_owner_t *owner);
noah_profile_candidate_transaction_t *noah_profile_owner_host_transaction(noah_profile_owner_t *owner);
noah_profile_split_reconciler_t      *noah_profile_owner_split_reconciler(noah_profile_owner_t *owner);
bool                                  noah_profile_owner_status(const noah_profile_owner_t *owner, noah_profile_owner_status_t *status);
// Whether the keyboard may drop off USB now (D-F25): the owner is ready with
// no candidate, save, activation or cleanup in flight, both halves hold the
// same profile, and nothing is held or running on either half (the boundary a
// profile activates at).
bool                                  noah_profile_owner_usb_restart_safe(noah_profile_owner_t *owner);

// Live edits (D-F32), from the Profile Wire channel on the USB half. Each
// admission is bounded; the work happens in later scans. A new edit, Save and
// a mode change wait until nothing else holds the profile (an Apply, a save,
// an activation) and the other half holds the same working copy.
noah_profile_working_result_t noah_profile_owner_edit_begin(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t session, uint32_t revision, uint8_t splice_count, uint32_t insert_total, uint8_t flags);
noah_profile_working_result_t noah_profile_owner_edit_splice(noah_profile_owner_t *owner, uint16_t transaction_id, uint8_t index, const noah_profile_working_splice_t *splice);
noah_profile_working_result_t noah_profile_owner_edit_data(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t offset, const uint8_t *bytes, uint32_t count);
noah_profile_working_result_t noah_profile_owner_edit_commit(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t crc32);
noah_profile_working_result_t noah_profile_owner_edit_abort(noah_profile_owner_t *owner, uint16_t transaction_id);
noah_profile_working_result_t noah_profile_owner_revert(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t session, uint32_t revision);
// QUEUED, then candidate status (value 0x18) follows it as transaction_id,
// as it follows an Apply. OK at once when nothing is unsaved.
noah_profile_working_result_t noah_profile_owner_save(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t session, uint32_t revision);
noah_profile_working_result_t noah_profile_owner_set_edit_mode(noah_profile_owner_t *owner, uint8_t mode);

// Called from key processing (main loop). Queues one adjustment scalar
// (profile_settings_v1.h). False for any other setting (toggles are never
// edits), an invalid value, a half that is not master, or an owner that is not
// ready. Repeated keys coalesce to the latest value, and the next available
// edit publishes them at the ordinary safe boundary, advancing the revision.
// Never gated by edit mode.
bool noah_profile_owner_adjust(noah_profile_owner_t *owner, uint8_t setting, uint32_t value);
// The queued, unpublished value of an adjustment scalar, which settings
// projection keeps as newer local feedback. False when none is pending.
bool noah_profile_owner_pending_adjustment(const noah_profile_owner_t *owner, uint8_t setting, uint32_t *value);

typedef struct {
    noah_profile_working_status_t   local;
    noah_profile_working_peer_t     peer;
    noah_profile_edit_mode_record_t mode;
    noah_profile_owner_save_t       save;
    uint32_t                        saved_generation;
    uint32_t                        safe_boundary_reason_mask;
    uint8_t                         saved_origin;
    bool                            supported;      // this build and half can edit live
    bool                            saved_compiled; // nothing saved: the compiled defaults
    bool                            peer_required;
    bool                            peer_converged;
    bool                            mode_conflict;
    bool                            edits_blocked;  // a new edit would be refused BUSY now
    bool                            adjustment_pending; // a keyboard adjustment awaits publication; a new Apply is BUSY
    bool                            storage_paused; // a save here waits for a quiet keyboard
    bool                            peer_storage_paused; // the other half's save waits for a quiet keyboard
    bool                            peer_refused;        // the other half refused this working copy, whole
    bool                            mode_store_failed;   // writing the edit mode failed; it is tried again
    bool                            stopped;             // this half's owner stopped: only a restart clears it (D-F34)
    bool                            peer_stopped;        // the other half answers that its owner stopped
} noah_profile_owner_working_status_t;

bool noah_profile_owner_working_status(const noah_profile_owner_t *owner, noah_profile_owner_working_status_t *status);
// Reads the profile now active (the working copy, or the saved profile).
bool noah_profile_owner_working_read(const noah_profile_owner_t *owner, uint32_t offset, uint8_t *bytes, uint32_t length);
// The other half's RPC callback for the working copy's sync.
bool noah_profile_owner_working_receive(noah_profile_owner_t *owner, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);

#ifdef NOAH_PROFILE_OWNER_TEST_DIAGNOSTICS
uint32_t noah_profile_owner_test_ready_refresh_count(void);
uint32_t noah_profile_owner_test_profile_held_count(void);
#endif
