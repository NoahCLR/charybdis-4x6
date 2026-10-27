// ──────────────────────────────────────────────────────────────────────────
// QMK Live-Profile Split RPCs
// ──────────────────────────────────────────────────────────────────────────

#include "qmk_profile_split_transport.h"
#include "qmk_durable_io.h"

#if defined(SPLIT_TRANSACTION_IDS_USER)

#    include <string.h>

#    include "transactions.h" // QMK
#    include "../state/shared/runtime_publication.h"

_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_MAX <= RPC_M2S_BUFFER_SIZE && NOAH_PROFILE_SPLIT_V1_FRAME_MAX <= RPC_S2M_BUFFER_SIZE, "the largest profile split frame, pushed or pulled, fits QMK's RPC buffers");
_Static_assert(NOAH_PROFILE_WORKING_SYNC_FRAME_MAX <= RPC_M2S_BUFFER_SIZE && NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE <= RPC_S2M_BUFFER_SIZE, "the largest working-copy request and its reply fit QMK's RPC buffers");

// QMK's RPC callbacks carry no context, so the endpoints are this boot's,
// set once before either RPC is registered.
static noah_profile_split_reconciler_t *split_reconciler;
static noah_profile_working_sync_t     *working_sync;

// QMK hands each callback the request as it came and the reply length the
// sender asked for. Each protocol's receive refuses a missing endpoint, a
// wrong length and an unframed request; its refusal is answered with zeros.
static inline __attribute__((always_inline)) void answer(bool answered, uint8_t response_size, void *response) {
    if (!answered && response && response_size != 0u) {
        memset(response, 0, response_size);
    }
}

static void noah_qmk_profile_split_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    answer(noah_profile_split_reconciler_receive(split_reconciler, request, request_size, response, response_size), response_size, response);
}

static void working_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    answer(noah_profile_working_sync_receive(working_sync, request, request_size, response, response_size), response_size, response);
}

bool noah_qmk_profile_split_transport_init(noah_profile_split_reconciler_t *reconciler, noah_profile_working_sync_t *working) {
    if (!reconciler || !working || split_reconciler) {
        return false;
    }
    split_reconciler = reconciler;
    working_sync     = working;
    // The slave thread may run a callback as soon as it is registered.
    NOAH_RUNTIME_PUBLICATION_BARRIER();
    transaction_register_rpc(PUT_PROFILE_SPLIT_SYNC, noah_qmk_profile_split_rpc);
    transaction_register_rpc(PUT_PROFILE_WORKING_SYNC, working_rpc);
    return true;
}

#    ifdef NOAH_PROFILE_SPLIT_TRANSPORT_TEST
void noah_qmk_profile_split_transport_reset_for_test(void) {
    split_reconciler = NULL;
    working_sync     = NULL;
}
#    endif

// Inlined into each exchange, so each stays one frame on the reviewed scan
// paths (tools/firmware_stack_budget_live_profile_owner.json).
static inline __attribute__((always_inline)) bool send(int8_t id, uint8_t request_min, uint8_t request_max, uint8_t reply_min, uint8_t reply_max, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    if (!request || !response || request_length < request_min || request_length > request_max || response_length < reply_min || response_length > reply_max) {
        return false;
    }
    noah_qmk_durable_io_note_split_rpc();
    return transaction_rpc_exec(id, request_length, request, response_length, response);
}

// Only these two RPCs send large requests or read large replies: QMK carries
// each call's own lengths, so the other split RPCs keep their small frames.
bool noah_qmk_profile_split_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    (void)context;
    return send(PUT_PROFILE_SPLIT_SYNC, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, NOAH_PROFILE_SPLIT_V1_FRAME_MAX, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, NOAH_PROFILE_SPLIT_V1_FRAME_MAX, request, request_length, response, response_length);
}

bool noah_qmk_profile_working_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    (void)context;
    return send(PUT_PROFILE_WORKING_SYNC, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, NOAH_PROFILE_WORKING_SYNC_FRAME_MAX, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, request, request_length, response, response_length);
}

#endif
