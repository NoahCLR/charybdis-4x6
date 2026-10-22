// The live profile's QMK request/reply adapter, alone: QMK below it and both
// protocols' receives above it are faked at link time, so no protocol is
// linked. Every test runs over both RPCs.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transactions.h"
#include "users/noah/lib/compat/qmk_profile_split_transport.h"

_Static_assert(NOAH_PROFILE_SPLIT_V1_FRAME_SIZE == 32u && NOAH_PROFILE_SPLIT_V1_FRAME_MAX == 128u, "the reviewed profile split frame bounds");
_Static_assert(NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE == 32u && NOAH_PROFILE_WORKING_SYNC_FRAME_MAX == 128u, "the reviewed working-copy frame bounds");

enum {
    PROTOCOL_NONE,
    PROTOCOL_SPLIT,
    PROTOCOL_WORKING,
    TRANSACTION_IDS = PUT_KEYBOARD_RESTART + 1,
    BUFFER_SIZE     = 160u, // past the largest frame, to see what is left untouched
    UNTOUCHED       = 0xA5u,
};

// What each RPC owes, from the specs: its QMK id, its exchange, and the
// request and reply lengths it sends.
typedef struct {
    const char                    *name;
    int8_t                         id;
    uint8_t                        protocol;
    noah_profile_split_exchange_fn exchange;
    uint8_t                        request_min;
    uint8_t                        request_max;
    uint8_t                        reply_min;
    uint8_t                        reply_max;
} rpc_t;

static const rpc_t rpcs[] = {
    // A pulled chunk's reply is up to the largest frame (profile-split-v1.md).
    {"PUT_PROFILE_SPLIT_SYNC", PUT_PROFILE_SPLIT_SYNC, PROTOCOL_SPLIT, noah_qmk_profile_split_transport_exchange, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, NOAH_PROFILE_SPLIT_V1_FRAME_MAX, NOAH_PROFILE_SPLIT_V1_FRAME_SIZE, NOAH_PROFILE_SPLIT_V1_FRAME_MAX},
    // Every working-copy reply is 32 bytes (D-F32).
    {"PUT_PROFILE_WORKING_SYNC", PUT_PROFILE_WORKING_SYNC, PROTOCOL_WORKING, noah_qmk_profile_working_transport_exchange, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, NOAH_PROFILE_WORKING_SYNC_FRAME_MAX, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE, NOAH_PROFILE_WORKING_SYNC_FRAME_SIZE},
};

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

// ── QMK, faked ──────────────────────────────────────────────────────────────

static slave_callback_t registered[TRANSACTION_IDS];
static unsigned         registrations;

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    CHECK(transaction_id >= 0 && transaction_id < TRANSACTION_IDS && callback);
    registered[transaction_id] = callback;
    registrations++;
}

// Every request the adapter sends is told to the durable-I/O grant first,
// which then repeats no step (D-F29, "Pacing").
static unsigned noted;

void noah_qmk_durable_io_note_split_rpc(void) {
    noted++;
}

static struct {
    unsigned count;
    int8_t   id;
    uint8_t  request_size;
    uint8_t  response_size;
    uint8_t  request[BUFFER_SIZE];
    bool     result;
} sent;

static uint8_t reply_byte(uint8_t index) {
    return (uint8_t)(index * 7u + 3u);
}

bool transaction_rpc_exec(int8_t transaction_id, uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    CHECK(noted == sent.count + 1u);
    CHECK(request && response && request_size <= sizeof(sent.request));
    sent.count++;
    sent.id            = transaction_id;
    sent.request_size  = request_size;
    sent.response_size = response_size;
    memcpy(sent.request, request, request_size);
    for (uint8_t index = 0u; index < response_size; index++) {
        ((uint8_t *)response)[index] = reply_byte(index);
    }
    return sent.result;
}

// ── Both protocols' receives, faked ─────────────────────────────────────────

static noah_profile_split_reconciler_t reconciler, other_reconciler;
static noah_profile_working_sync_t     working, other_working;

static struct {
    unsigned       count;
    uint8_t        protocol;
    const void    *endpoint;
    const uint8_t *request;
    uint8_t        request_length;
    uint8_t       *response;
    uint8_t        response_length;
} received;
static bool refuse;

// Answers with a pattern, or refuses after writing over part of the reply:
// the adapter, not the protocol, makes a refused request's answer zeros.
static bool receive(uint8_t protocol, const void *endpoint, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    received.count++;
    received.protocol        = protocol;
    received.endpoint        = endpoint;
    received.request         = request;
    received.request_length  = request_length;
    received.response        = response;
    received.response_length = response_length;
    if (response) {
        for (uint8_t index = 0u; index < response_length; index++) {
            response[index] = (uint8_t)(protocol + index);
        }
    }
    return !refuse && response;
}

bool noah_profile_split_reconciler_receive(noah_profile_split_reconciler_t *endpoint, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    return receive(PROTOCOL_SPLIT, endpoint, request, request_length, response, response_length);
}

bool noah_profile_working_sync_receive(noah_profile_working_sync_t *endpoint, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    return receive(PROTOCOL_WORKING, endpoint, request, request_length, response, response_length);
}

static const void *endpoint_of(const rpc_t *rpc) {
    return rpc->protocol == PROTOCOL_SPLIT ? (const void *)&reconciler : (const void *)&working;
}

static void reset(void) {
    noah_qmk_profile_split_transport_reset_for_test();
    memset(registered, 0, sizeof(registered));
    memset(&sent, 0, sizeof(sent));
    memset(&received, 0, sizeof(received));
    registrations = 0u;
    noted         = 0u;
    sent.result   = true;
    refuse        = false;
}

static void registered_pair(void) {
    reset();
    CHECK(noah_qmk_profile_split_transport_init(&reconciler, &working));
}

static void test_init_registers_both_rpcs_once(void) {
    slave_callback_t first[TRANSACTION_IDS];

    reset();
    // A missing endpoint registers nothing.
    CHECK(!noah_qmk_profile_split_transport_init(NULL, &working));
    CHECK(!noah_qmk_profile_split_transport_init(&reconciler, NULL));
    CHECK(registrations == 0u);
    // Both RPCs together, and nothing else.
    CHECK(noah_qmk_profile_split_transport_init(&reconciler, &working));
    CHECK(registrations == 2u);
    for (int8_t id = 0; id < TRANSACTION_IDS; id++) {
        CHECK((registered[id] != NULL) == (id == PUT_PROFILE_SPLIT_SYNC || id == PUT_PROFILE_WORKING_SYNC));
    }
    CHECK(registered[PUT_PROFILE_SPLIT_SYNC] != registered[PUT_PROFILE_WORKING_SYNC]);
    // Once a boot: a second call, with the same endpoints or others, is refused
    // and leaves the first registration answering.
    memcpy(first, registered, sizeof(first));
    CHECK(!noah_qmk_profile_split_transport_init(&reconciler, &working));
    CHECK(!noah_qmk_profile_split_transport_init(&other_reconciler, &other_working));
    CHECK(registrations == 2u && memcmp(first, registered, sizeof(first)) == 0);
    for (size_t index = 0u; index < sizeof(rpcs) / sizeof(rpcs[0]); index++) {
        uint8_t request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {0};
        uint8_t response[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE];

        registered[rpcs[index].id](sizeof(request), request, sizeof(response), response);
        CHECK(received.endpoint == endpoint_of(&rpcs[index]));
    }
}

// Each callback hands its protocol the request exactly as QMK gave it, with
// the reply buffer the sender asked for, and returns what the protocol wrote.
static void test_each_callback_answers_from_its_protocol(void) {
    for (size_t index = 0u; index < sizeof(rpcs) / sizeof(rpcs[0]); index++) {
        const rpc_t *rpc = &rpcs[index];
        uint8_t      request[NOAH_PROFILE_SPLIT_V1_FRAME_MAX];
        uint8_t      response[BUFFER_SIZE];

        registered_pair();
        for (uint8_t byte = 0u; byte < sizeof(request); byte++) request[byte] = byte;
        memset(response, UNTOUCHED, sizeof(response));
        registered[rpc->id](rpc->request_max, request, rpc->reply_max, response);
        CHECK(received.count == 1u && received.protocol == rpc->protocol && received.endpoint == endpoint_of(rpc));
        CHECK(received.request == request && received.request_length == rpc->request_max);
        CHECK(received.response == response && received.response_length == rpc->reply_max);
        for (uint16_t byte = 0u; byte < sizeof(response); byte++) {
            CHECK(response[byte] == (byte < rpc->reply_max ? (uint8_t)(rpc->protocol + byte) : UNTOUCHED));
        }
    }
}

// A refused request is answered with zeros across the reply the sender asked
// for, never with what the buffer held, and nothing past it is written.
static void test_a_refused_request_is_answered_with_zeros(void) {
    for (size_t index = 0u; index < sizeof(rpcs) / sizeof(rpcs[0]); index++) {
        const rpc_t *rpc     = &rpcs[index];
        uint8_t      sizes[] = {1u, rpc->reply_min, rpc->reply_max};
        uint8_t      request[NOAH_PROFILE_SPLIT_V1_FRAME_SIZE] = {0};
        uint8_t      response[BUFFER_SIZE];

        registered_pair();
        refuse = true;
        for (size_t size = 0u; size < sizeof(sizes); size++) {
            memset(response, UNTOUCHED, sizeof(response));
            registered[rpc->id](sizeof(request), request, sizes[size], response);
            CHECK(received.protocol == rpc->protocol);
            for (uint16_t byte = 0u; byte < sizeof(response); byte++) {
                CHECK(response[byte] == (byte < sizes[size] ? 0u : UNTOUCHED));
            }
        }
        // No reply buffer, or one of no bytes: nothing is written.
        memset(response, UNTOUCHED, sizeof(response));
        registered[rpc->id](sizeof(request), request, 0u, response);
        for (uint16_t byte = 0u; byte < sizeof(response); byte++) CHECK(response[byte] == UNTOUCHED);
        registered[rpc->id](sizeof(request), request, rpc->reply_max, NULL);
    }
}

// Each exchange sends on its own RPC at the request's own length and reads
// the reply length its caller asks for, inside its protocol's frame bounds;
// anything else is refused unsent and unnoted. A request QMK fails to deliver
// was still noted.
static void test_each_exchange_sends_its_rpc_at_its_lengths(void) {
    for (size_t index = 0u; index < sizeof(rpcs) / sizeof(rpcs[0]); index++) {
        const rpc_t *rpc = &rpcs[index];
        const struct {
            uint8_t request;
            uint8_t reply;
        } accepted[] = {
            {rpc->request_min, rpc->reply_min},
            {rpc->request_max, rpc->reply_min},
            {rpc->request_min, rpc->reply_max},
            {rpc->request_max, rpc->reply_max},
        }, refused[] = {
            {(uint8_t)(rpc->request_min - 1u), rpc->reply_min},
            {(uint8_t)(rpc->request_max + 1u), rpc->reply_min},
            {rpc->request_min, (uint8_t)(rpc->reply_min - 1u)},
            {rpc->request_min, (uint8_t)(rpc->reply_max + 1u)},
        };
        uint8_t request[BUFFER_SIZE];
        uint8_t response[BUFFER_SIZE];

        reset();
        for (uint8_t byte = 0u; byte < sizeof(request); byte++) request[byte] = (uint8_t)(byte ^ 0x5Au);
        for (size_t at = 0u; at < sizeof(accepted) / sizeof(accepted[0]); at++) {
            memset(response, UNTOUCHED, sizeof(response));
            CHECK(rpc->exchange(NULL, request, accepted[at].request, response, accepted[at].reply));
            CHECK(sent.count == at + 1u && noted == sent.count && sent.id == rpc->id);
            CHECK(sent.request_size == accepted[at].request && sent.response_size == accepted[at].reply);
            CHECK(memcmp(sent.request, request, accepted[at].request) == 0);
            for (uint16_t byte = 0u; byte < sizeof(response); byte++) {
                CHECK(response[byte] == (byte < accepted[at].reply ? reply_byte((uint8_t)byte) : UNTOUCHED));
            }
        }
        for (size_t at = 0u; at < sizeof(refused) / sizeof(refused[0]); at++) {
            CHECK(!rpc->exchange(NULL, request, refused[at].request, response, refused[at].reply));
        }
        CHECK(!rpc->exchange(NULL, NULL, rpc->request_min, response, rpc->reply_min));
        CHECK(!rpc->exchange(NULL, request, rpc->request_min, NULL, rpc->reply_min));
        CHECK(sent.count == 4u && noted == 4u);
        sent.result = false;
        CHECK(!rpc->exchange(NULL, request, rpc->request_min, response, rpc->reply_min));
        CHECK(sent.count == 5u && noted == 5u && sent.id == rpc->id);
        // Sending registers nothing and reaches no protocol.
        CHECK(registrations == 0u && received.count == 0u);
    }
}

int main(void) {
    test_init_registers_both_rpcs_once();
    test_each_callback_answers_from_its_protocol();
    test_a_refused_request_is_answered_with_zeros();
    test_each_exchange_sends_its_rpc_at_its_lengths();

    puts("qmk profile split transport host tests passed");
    return 0;
}
