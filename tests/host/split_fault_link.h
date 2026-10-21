// An in-memory split link: a noah_profile_split_exchange_fn that hands each
// request to the other half's receive, as QMK's request/reply RPC does, and
// fails the ways that RPC fails on a real link. The reconciler, working-copy
// sync and owner pair tests run over it. The keyboard restart RPC does not:
// it fakes QMK one layer lower (D-F34).
//
// The link answers as the production adapter does: a request the other half
// refuses comes back as zeros, with success.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "users/noah/lib/profile/split/profile_split_exchange.h"

// The largest request or reply it carries. Each test asserts its protocol's
// largest frame fits.
enum { SPLIT_FAULT_LINK_FRAME_MAX = 128u };

typedef struct {
    noah_profile_split_exchange_fn receive;   // the other half's receive (the RPC callback's body)
    void                          *peer;      // its context
    uint32_t                       exchanges; // requests offered; a fault names the 1-based exchange it hits
    // One-shot faults, each cleared when it fires (0 is none).
    uint32_t drop_before_delivery; // the request never arrives: no reply
    uint32_t drop_after_delivery;  // the other half takes it; its reply is lost
    uint32_t replay_previous;      // the previous reply comes back and the request is not delivered (the other half skipped its callback)
    uint32_t inject;               // injected[0..injected_length) comes back, the rest zero; not delivered
    uint32_t garble;               // delivered with one bit flipped (byte 5, bit 4)
    uint8_t  injected[SPLIT_FAULT_LINK_FRAME_MAX];
    uint8_t  injected_length;
    // Standing faults; zero is a working link.
    uint32_t drop_every;         // every Nth exchange's reply is lost after delivery
    bool     disconnected;       // nothing arrives
    bool     corrupt_next_reply; // the next delivered reply has byte 31 bit 0 flipped (the CRC in both protocols)
    // QMK's reply buffer, as the last delivered answer left it.
    uint8_t previous_reply[SPLIT_FAULT_LINK_FRAME_MAX];
} split_fault_link_t;

// `context` is a split_fault_link_t. Faults are checked in this order: drop
// before delivery, replay, inject, garble, disconnected, delivery, drop after
// delivery, drop_every, corrupt.
bool split_fault_link_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);
