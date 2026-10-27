// ──────────────────────────────────────────────────────────────────────────
// QMK Live-Profile Split RPCs
// ──────────────────────────────────────────────────────────────────────────
//
// The live profile's two request/reply split RPCs over QMK, one adapter for
// both. PUT_PROFILE_SPLIT_SYNC carries the durable reconciler's frames
// (profile-split-v1.md) and PUT_PROFILE_WORKING_SYNC the working copy's
// (D-F32). Each protocol keeps its own framing, mailbox and replay. This
// module holds what both owe QMK and the scan:
// - Sending: a request goes at its own length and its reply is read at the
//   length the request asks for, both inside its protocol's frame bounds;
//   anything else is refused unsent. Every request sent is first reported to
//   the durable-I/O grant, which then repeats no step (D-F29, "Pacing"),
//   whether or not QMK delivers it.
// - Answering, on the other half's split slave thread (HIGHPRIO, 1,024 usable
//   stack bytes): the request goes straight to its protocol's receive, which
//   only copies it into a mailbox and answers from a published reply. A
//   request the protocol refuses is answered with zeros, so the sender never
//   reads the reply buffer's previous contents as this request's answer.
// - Registration: both RPCs together, once per boot.
// The keyboard restart RPC is not here: it is registered from housekeeping,
// apart from the profile owner (qmk_keyboard_restart.c, D-F34).
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../profile/split/profile_split_exchange.h"
#include "../profile/split/profile_split_reconciler.h"
#include "../profile/split/profile_working_sync.h"

#if defined(SPLIT_TRANSACTION_IDS_USER)

// Registers both RPCs with QMK, answering into these two endpoints.
// Calling context: the scan (profile_store_runtime.c), once the owner has
// built both (noah_profile_owner_split_reconciler and
// noah_profile_owner_working_sync are non-NULL). Neither RPC answers before
// this call: until then QMK runs no callback, and each protocol's framing
// refuses the stale reply buffer.
// False for a missing endpoint or for any second call: an integration error
// that no later scan heals.
bool noah_qmk_profile_split_transport_init(noah_profile_split_reconciler_t *reconciler, noah_profile_working_sync_t *working);

// noah_profile_split_exchange_fn adapters; `context` is unused.
// PUT_PROFILE_SPLIT_SYNC: requests and replies of
// NOAH_PROFILE_SPLIT_V1_FRAME_SIZE up to NOAH_PROFILE_SPLIT_V1_FRAME_MAX bytes.
bool noah_qmk_profile_split_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);
// PUT_PROFILE_WORKING_SYNC: requests of NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE
// up to NOAH_PROFILE_WORKING_SYNC_FRAME_MAX bytes; replies of exactly
// NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE.
bool noah_qmk_profile_working_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);

#    ifdef NOAH_PROFILE_SPLIT_TRANSPORT_TEST
void noah_qmk_profile_split_transport_reset_for_test(void);
#    endif

#else

static inline bool noah_qmk_profile_split_transport_init(noah_profile_split_reconciler_t *reconciler, noah_profile_working_sync_t *working) {
    (void)reconciler;
    (void)working;
    return false;
}

static inline bool noah_qmk_profile_split_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    (void)context;
    (void)request;
    (void)request_length;
    (void)response;
    (void)response_length;
    return false;
}

static inline bool noah_qmk_profile_working_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    (void)context;
    (void)request;
    (void)request_length;
    (void)response;
    (void)response_length;
    return false;
}

#endif
