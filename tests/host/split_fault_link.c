#include "split_fault_link.h"

#include <assert.h>
#include <string.h>

enum {
    GARBLED_BYTE   = 5u,
    GARBLED_BIT    = 0x10u,
    CORRUPTED_BYTE = 31u,
    CORRUPTED_BIT  = 0x01u,
};

bool split_fault_link_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    split_fault_link_t *link = context;
    uint8_t             delivered[SPLIT_FAULT_LINK_FRAME_MAX];

    assert(link && request && response);
    assert(request_length > GARBLED_BYTE && request_length <= SPLIT_FAULT_LINK_FRAME_MAX);
    assert(response_length > CORRUPTED_BYTE && response_length <= SPLIT_FAULT_LINK_FRAME_MAX);
    link->exchanges++;
    if (link->drop_before_delivery == link->exchanges) {
        link->drop_before_delivery = 0u;
        return false;
    }
    if (link->replay_previous == link->exchanges) {
        link->replay_previous = 0u;
        memcpy(response, link->previous_reply, response_length);
        return true;
    }
    if (link->inject == link->exchanges) {
        link->inject = 0u;
        assert(link->injected_length <= response_length);
        memset(response, 0, response_length);
        memcpy(response, link->injected, link->injected_length);
        return true;
    }
    memcpy(delivered, request, request_length);
    if (link->garble == link->exchanges) {
        link->garble = 0u;
        delivered[GARBLED_BYTE] ^= GARBLED_BIT;
    }
    if (link->disconnected) {
        return false;
    }
    assert(link->receive);
    if (!link->receive(link->other_half, delivered, request_length, response, response_length)) {
        memset(response, 0, response_length);
    }
    // QMK's reply buffer keeps whatever this reply left in it.
    memcpy(link->previous_reply, response, response_length);
    if (link->drop_after_delivery == link->exchanges) {
        link->drop_after_delivery = 0u;
        return false;
    }
    if (link->drop_every != 0u && link->exchanges % link->drop_every == 0u) {
        return false;
    }
    if (link->corrupt_next_reply) {
        link->corrupt_next_reply = false;
        response[CORRUPTED_BYTE] ^= CORRUPTED_BIT;
    }
    return true;
}
