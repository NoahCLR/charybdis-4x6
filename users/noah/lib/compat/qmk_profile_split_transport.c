// ──────────────────────────────────────────────────────────────────────────
// QMK Live-Profile Split Transport Boundary
// ──────────────────────────────────────────────────────────────────────────

#include "qmk_profile_split_transport.h"

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)

#    include <string.h>

#    include "transactions.h" // QMK

static noah_profile_split_reconciler_t *noah_qmk_profile_split_reconciler;

_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_SIZE <= RPC_M2S_BUFFER_SIZE, "profile split request exceeds QMK RPC buffer");
_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_SIZE <= RPC_S2M_BUFFER_SIZE, "profile split response exceeds QMK RPC buffer");

static void noah_qmk_profile_split_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    if (!(response && response_size == NOAH_PROFILE_SPLIT_V1_FRAME_SIZE && noah_qmk_profile_split_reconciler && noah_profile_split_reconciler_receive(noah_qmk_profile_split_reconciler, request, request_size, response, response_size))) {
        if (response && response_size != 0u) {
            memset(response, 0, response_size);
        }
    }
}

bool noah_qmk_profile_split_transport_init(noah_profile_split_reconciler_t *reconciler) {
    if (!reconciler || !noah_profile_split_reconciler_authority(reconciler)) {
        return false;
    }
    noah_qmk_profile_split_reconciler = reconciler;
    transaction_register_rpc(PUT_PROFILE_SPLIT_SYNC, noah_qmk_profile_split_rpc);
    return true;
}

bool noah_qmk_profile_split_transport_exchange(void *context, const uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE], uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE]) {
    (void)context;
    return request && response && transaction_rpc_exec(PUT_PROFILE_SPLIT_SYNC, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, request, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, response);
}

#endif
