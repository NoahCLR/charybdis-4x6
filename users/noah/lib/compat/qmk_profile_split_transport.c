// ──────────────────────────────────────────────────────────────────────────
// QMK Live-Profile Split Transport Boundary
// ──────────────────────────────────────────────────────────────────────────

#include "qmk_profile_split_transport.h"
#include "qmk_durable_io.h"

#if defined(SPLIT_TRANSACTION_IDS_USER)

#    include <string.h>

#    include "transactions.h" // QMK

static noah_profile_split_reconciler_t *noah_qmk_profile_split_reconciler;

_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_MAX <= RPC_M2S_BUFFER_SIZE, "the largest profile split request exceeds QMK's master-to-slave RPC buffer");
_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_MAX <= RPC_S2M_BUFFER_SIZE, "a pulled profile chunk exceeds QMK's slave-to-master RPC buffer");

// QMK hands the callback the reply length the master asked for.
static void noah_qmk_profile_split_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    if (!(response && response_size >= NOAH_PROFILE_SPLIT_V1_FRAME_SIZE && response_size <= NOAH_PROFILE_SPLIT_V1_FRAME_MAX && noah_qmk_profile_split_reconciler && noah_profile_split_reconciler_receive(noah_qmk_profile_split_reconciler, request, request_size, response, response_size))) {
        if (response && response_size != 0u) {
            memset(response, 0, response_size);
        }
    }
}

bool noah_qmk_profile_split_transport_init(noah_profile_split_reconciler_t *reconciler) {
    if (!reconciler || !noah_profile_split_reconciler_authority(reconciler)) {
        return false;
    }
    if (noah_qmk_profile_split_reconciler) {
        return noah_qmk_profile_split_reconciler == reconciler;
    }
    noah_qmk_profile_split_reconciler = reconciler;
    transaction_register_rpc(PUT_PROFILE_SPLIT_SYNC, noah_qmk_profile_split_rpc);
    return true;
}

#    ifdef NOAH_PROFILE_SPLIT_TRANSPORT_TEST
void noah_qmk_profile_split_transport_reset_for_test(void) {
    noah_qmk_profile_split_reconciler = NULL;
}
#    endif

// Only this exchange sends large requests and asks for large replies: QMK's
// RPC carries each call's own lengths, so the other split RPCs keep sending
// and reading their small frames, and only a pulled chunk reads up to 128.
bool noah_qmk_profile_split_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    (void)context;
    if (!request || !response || request_length < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE || request_length > NOAH_PROFILE_SPLIT_V1_FRAME_MAX || response_length < NOAH_PROFILE_SPLIT_V1_FRAME_SIZE || response_length > NOAH_PROFILE_SPLIT_V1_FRAME_MAX) {
        return false;
    }
    // The durable-I/O grant repeats no step after one that used the link.
    noah_qmk_durable_io_note_split_rpc();
    return transaction_rpc_exec(PUT_PROFILE_SPLIT_SYNC, request_length, request, response_length, response);
}

#endif
