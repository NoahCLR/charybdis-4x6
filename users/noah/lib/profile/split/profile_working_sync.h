// ───────────────────────────────────────────────────────────────────────────
// Working Copy Split Sync (D-F32)
// ───────────────────────────────────────────────────────────────────────────
//
// Brings the other half's working copy to the USB half's, over a split RPC
// of its own (PUT_PROFILE_WORKING_SYNC), separate from the durable profile
// reconciler: nothing here writes flash or touches a slot.
//
// The USB half is the only initiator. After each edit it publishes, it sends
// the same edit (BEGIN, SPLICE, DATA, COMMIT) and polls until the other half
// reports the same session and revision. When the halves disagree in any
// other way, it brings the other half back: REVERT a working copy that is
// not its own, ADOPT its identity over the same saved profile, or send its
// whole working copy as a replacement. An edit the other half refuses is
// followed by one whole-copy replacement; if that is refused too, the halves
// stay apart (reported) until Revert. It also carries the edit-mode record
// both ways; each half writes it later, when its keyboard is quiet. Edits on
// the USB half wait until the other half has caught up (profile_owner.c), so
// at most one edit is ever behind.
//
// The other half's RPC callback runs on the split slave thread, which can
// preempt the main loop: it only copies a request into a one-frame mailbox
// and answers from the last published reply. The main loop's scan does the
// work and publishes the next reply. A request is retried with the same
// sequence number until a reply for it arrives.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../runtime/profile_edit_mode.h"
#include "../runtime/profile_working_copy.h"
#include "../../state/shared/runtime_publication.h"
#include "profile_split_reconciler.h"

enum {
    NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE  = 32u,
    NOAH_PROFILE_WORKING_SYNC_FRAME_MAX   = 128u,
    NOAH_PROFILE_WORKING_SYNC_HEADER_SIZE = 13u,
    // A DATA frame: header, offset and count, the bytes, CRC16.
    NOAH_PROFILE_WORKING_SYNC_DATA_MAX = NOAH_PROFILE_WORKING_SYNC_FRAME_MAX - NOAH_PROFILE_WORKING_SYNC_HEADER_SIZE - 5u - 2u,
    // Polls while the halves differ or an edit is on its way, and while they
    // agree; a half not heard from for the timeout is unknown again.
    NOAH_PROFILE_WORKING_SYNC_POLL_MS        = 100u,
    NOAH_PROFILE_WORKING_SYNC_IDLE_POLL_MS   = 1000u,
    NOAH_PROFILE_WORKING_SYNC_PEER_TIMEOUT_MS = 3000u,
    // A failed edit-mode write is tried again after this, doubling each time.
    NOAH_PROFILE_WORKING_SYNC_MODE_RETRY_MS     = 1000u,
    NOAH_PROFILE_WORKING_SYNC_MODE_RETRY_MAX_MS = 64000u,
};

typedef enum {
    NOAH_PROFILE_WORKING_SYNC_STATUS = 1u,
    NOAH_PROFILE_WORKING_SYNC_BEGIN  = 2u,
    NOAH_PROFILE_WORKING_SYNC_SPLICE = 3u,
    NOAH_PROFILE_WORKING_SYNC_DATA   = 4u,
    NOAH_PROFILE_WORKING_SYNC_COMMIT = 5u,
    NOAH_PROFILE_WORKING_SYNC_ABORT  = 6u,
    NOAH_PROFILE_WORKING_SYNC_REVERT = 7u,
    NOAH_PROFILE_WORKING_SYNC_ADOPT  = 8u,
    NOAH_PROFILE_WORKING_SYNC_MODE   = 9u,
    NOAH_PROFILE_WORKING_SYNC_KIND_MAX = NOAH_PROFILE_WORKING_SYNC_MODE,
} noah_profile_working_sync_kind_t;

enum {
    // Reply results beyond the working copy's own.
    NOAH_PROFILE_WORKING_SYNC_ADMITTED = 0xFEu, // queued; the answer comes on a retry
    NOAH_PROFILE_WORKING_SYNC_BUSY     = 0xFFu, // an earlier request is still unprocessed
    NOAH_PROFILE_WORKING_SYNC_PEER_ACTIVE         = 1u << 0,
    NOAH_PROFILE_WORKING_SYNC_PEER_SAVED_COMPILED = 1u << 1,
    // The answering half's profile owner stopped: only a restart clears it
    // (D-F34). Its replies say where it stopped; it acts on nothing.
    NOAH_PROFILE_WORKING_SYNC_PEER_STOPPED = 1u << 2,
};

// One half's working copy as the other sees it.
typedef struct {
    uint32_t session;
    uint32_t revision;
    uint32_t saved_generation;
    uint32_t mode_counter;
    uint32_t seen_at;
    uint16_t transaction_id; // the edit in progress, else the last finished
    uint8_t  phase;
    uint8_t  last_result;
    uint8_t  flags;
    uint8_t  saved_origin;
    uint8_t  mode;
    bool     known;
} noah_profile_working_peer_t;

typedef enum {
    NOAH_PROFILE_WORKING_FORWARD_NONE = 0u,
    NOAH_PROFILE_WORKING_FORWARD_BEGIN,
    NOAH_PROFILE_WORKING_FORWARD_SPLICES,
    NOAH_PROFILE_WORKING_FORWARD_DATA,
    NOAH_PROFILE_WORKING_FORWARD_COMMIT,
    NOAH_PROFILE_WORKING_FORWARD_WAIT,
} noah_profile_working_forward_phase_t;

typedef struct {
    noah_profile_working_copy_t      *copy;
    noah_profile_split_exchange_fn    exchange;
    void                             *transport_context;
    noah_profile_edit_mode_storage_t  mode_storage;
    noah_profile_edit_mode_record_t   mode;

    // USB half: the other half as last heard, and the request in flight.
    noah_profile_working_peer_t peer;
    uint8_t                     request[NOAH_PROFILE_WORKING_SYNC_FRAME_MAX];
    uint8_t                     request_length;
    bool                        request_pending;
    uint16_t                    next_sequence;
    uint16_t                    next_transaction;
    uint32_t                    last_poll_at;
    uint32_t                    exchanges;
    uint32_t                    failed_exchanges;
    // The edit being sent: which revision it makes, its own transaction on
    // the other half, how far it got, and a running CRC for a replacement.
    uint32_t                    forward_revision;
    uint32_t                    forward_offset;
    uint32_t                    forward_total;
    uint32_t                    forward_crc32;
    uint32_t                    forward_failed_revision; // not sent again
    // The last identity both halves were seen to hold: only an edit made
    // right on top of it is sent as an edit; anything else sends the whole
    // working copy. Forgotten whenever the other half's bytes may have
    // changed without an edit (ADOPT, REVERT, a half not heard from).
    uint32_t                    agreed_session;
    uint32_t                    agreed_revision;
    bool                        agreed;
    uint16_t                    forward_transaction;
    uint8_t                     forward_phase;
    uint8_t                     forward_index;
    bool                        forward_replace;
    bool                        forward_failed_whole; // the whole copy was refused too
    bool                        mode_conflict;

    // Both halves: an edit-mode record not yet written, and a failed write.
    uint32_t                    mode_store_failed_at;
    uint32_t                    mode_store_backoff_ms;
    bool                        mode_store_pending;
    bool                        mode_store_failed;

    // Other half: its owner stopped (D-F34). Requests are still answered, a
    // status poll with OK and anything else INVALID_STATE, and none acted on.
    bool                                  stopped;
    // Other half: the mailbox the callback fills and the published reply.
    volatile bool                         mailbox_full;
    uint8_t                               mailbox[NOAH_PROFILE_WORKING_SYNC_FRAME_MAX];
    uint8_t                               mailbox_length;
    noah_runtime_publication_generation_t reply_sequence;
    uint8_t                               reply[NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE];
} noah_profile_working_sync_t;

void noah_profile_working_sync_init(noah_profile_working_sync_t *sync, noah_profile_working_copy_t *copy, noah_profile_split_exchange_fn exchange, void *transport_context, const noah_profile_edit_mode_storage_t *mode_storage);

// USB half, one scan grant: at most one exchange. True when it exchanged.
bool noah_profile_working_sync_master_scan(noah_profile_working_sync_t *sync, uint32_t now_ms);
// Whether the other half holds exactly this half's working copy (same
// session and revision, nothing in progress), as last heard and recently.
bool noah_profile_working_sync_converged(const noah_profile_working_sync_t *sync, uint32_t now_ms);
// Records a new edit mode on this half (a counter above both halves') and
// sends it to the other half. It is written later, by store_mode. False
// when this half has nowhere to store it.
bool noah_profile_working_sync_set_mode(noah_profile_working_sync_t *sync, uint8_t mode);
// The other half refused both the edit and the whole working copy at this
// revision: the halves stay apart until a Revert or an Apply.
bool noah_profile_working_sync_refused(const noah_profile_working_sync_t *sync);
// The other half answers that its owner stopped (D-F34), as last heard and
// recently.
bool noah_profile_working_sync_peer_stopped(const noah_profile_working_sync_t *sync, uint32_t now_ms);

// Both halves, one scan grant: writes a pending edit-mode record if the
// storage allows it now. True when it tried.
bool noah_profile_working_sync_store_mode(noah_profile_working_sync_t *sync, uint32_t now_ms);

// Other half: the RPC callback (slave thread) and the scan that answers.
bool noah_profile_working_sync_receive(noah_profile_working_sync_t *sync, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);
bool noah_profile_working_sync_peer_scan(noah_profile_working_sync_t *sync);

// Frame codec, shared by both sides and the tests.
uint8_t noah_profile_working_sync_frame_length(const uint8_t *frame, uint8_t capacity);
void    noah_profile_working_sync_seal(uint8_t *frame, uint8_t length);
