// ──────────────────────────────────────────────────────────────────────────
// QMK Live-Profile Split Transport Boundary
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../profile/split/profile_split_reconciler.h"

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)

// Installs the single production reconciler after its store/provider owner is
// initialized. Merely compiling this adapter does not enable live mutation.
bool noah_qmk_profile_split_transport_init(noah_profile_split_reconciler_t *reconciler);

// noah_profile_split_exchange_fn adapter used by the master scan owner.
bool noah_qmk_profile_split_transport_exchange(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]);

#else

static inline bool noah_qmk_profile_split_transport_init(noah_profile_split_reconciler_t *reconciler) {
    (void)reconciler;
    return false;
}

static inline bool noah_qmk_profile_split_transport_exchange(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    (void)context;
    (void)request;
    (void)response;
    return false;
}

#endif
