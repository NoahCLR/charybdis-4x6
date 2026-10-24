// Keyboard restart (D-F34): the split frame, the other half's RPC and both
// halves' timing. The module is compiled twice, the second copy's names
// prefixed `slave_` (run_qmk_keyboard_restart_tests.sh), so one binary holds
// the master half and the other half, joined by a fake split link.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_keyboard_restart.h"

typedef void (*slave_callback_t)(uint8_t initiator2target_buffer_size, const void *initiator2target_buffer, uint8_t target2initiator_buffer_size, void *target2initiator_buffer);

enum { PUT_KEYBOARD_RESTART_ID = 8 };

void slave_restart_encode(uint8_t kind, uint8_t frame[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE]);
bool slave_restart_request(void);
bool slave_restart_pending(void);
void slave_restart_task(void);
void slave_restart_reset_for_test(void);

static uint32_t         clock_ms;
static slave_callback_t usb_callback;
static slave_callback_t slave_callback;
static unsigned         usb_resets;
static unsigned         slave_resets;
static unsigned         exchanges;
static bool             link_down;
static unsigned         slave_busy_attempts; // the other half's split thread held off (an erase)

uint32_t timer_read32(void) {
    return clock_ms;
}

uint32_t timer_elapsed32(uint32_t last) {
    return clock_ms - last;
}

void wait_ms(uint16_t ms) {
    clock_ms += ms;
}

void slave_wait_ms(uint16_t ms) {
    clock_ms += ms;
}

void soft_reset_keyboard(void) {
    usb_resets++;
}

void slave_soft_reset_keyboard(void) {
    slave_resets++;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    assert(transaction_id == PUT_KEYBOARD_RESTART_ID);
    usb_callback = callback;
}

void slave_transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    assert(transaction_id == PUT_KEYBOARD_RESTART_ID);
    slave_callback = callback;
}

// The master half's RPC: QMK refuses at once while the link is down, and times
// out while the other half cannot answer.
bool transaction_rpc_exec(int8_t transaction_id, uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    assert(transaction_id == PUT_KEYBOARD_RESTART_ID && request_size == NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE && response_size == NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE);
    exchanges++;
    if (link_down) {
        return false;
    }
    if (slave_busy_attempts) {
        slave_busy_attempts--;
        clock_ms += 20u;
        return false;
    }
    clock_ms += 1u;
    if (!slave_callback) {
        // A half without the RPC answers zeros.
        memset(response, 0, response_size);
        return true;
    }
    slave_callback(request_size, request, response_size, response);
    return true;
}

// The other half never asks.
bool slave_transaction_rpc_exec(int8_t transaction_id, uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    (void)transaction_id;
    (void)request_size;
    (void)request;
    (void)response_size;
    (void)response;
    assert(false);
    return false;
}

static void reset(void) {
    noah_qmk_keyboard_restart_reset_for_test();
    slave_restart_reset_for_test();
    clock_ms           = 1000u;
    usb_callback       = NULL;
    slave_callback     = NULL;
    usb_resets         = 0u;
    slave_resets       = 0u;
    exchanges          = 0u;
    link_down          = false;
    slave_busy_attempts = 0u;
    // Housekeeping registers each half's RPC.
    noah_qmk_keyboard_restart_task();
    slave_restart_task();
    assert(usb_callback && slave_callback && !usb_resets && !slave_resets);
}

static void housekeeping_for(uint32_t ms) {
    for (uint32_t elapsed = 0u; elapsed < ms; elapsed++) {
        noah_qmk_keyboard_restart_task();
        slave_restart_task();
        clock_ms++;
    }
}

// Housekeeping every millisecond until `resets` counts one; how long it took.
static uint32_t ms_until_reset(const unsigned *resets, uint32_t since) {
    for (uint32_t guard = 0u; guard < 1000u && !*resets; guard++) {
        noah_qmk_keyboard_restart_task();
        slave_restart_task();
        if (!*resets) clock_ms++;
    }
    assert(*resets == 1u);
    return clock_ms - since;
}

static void test_frames(void) {
    uint8_t frame[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE];
    uint8_t slave_frame[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE];

    noah_qmk_keyboard_restart_encode(NOAH_QMK_KEYBOARD_RESTART_REQUEST, frame);
    assert(frame[0] == 0x01u && memcmp(&frame[1], "REST", 4u) == 0 && frame[5] == NOAH_QMK_KEYBOARD_RESTART_VERSION);
    slave_restart_encode(NOAH_QMK_KEYBOARD_RESTART_REQUEST, slave_frame);
    assert(memcmp(frame, slave_frame, sizeof(frame)) == 0);
    assert(noah_qmk_keyboard_restart_frame_valid(frame, sizeof(frame), NOAH_QMK_KEYBOARD_RESTART_REQUEST));
    assert(!noah_qmk_keyboard_restart_frame_valid(frame, sizeof(frame), NOAH_QMK_KEYBOARD_RESTART_ACK));
    assert(!noah_qmk_keyboard_restart_frame_valid(frame, sizeof(frame) - 1u, NOAH_QMK_KEYBOARD_RESTART_REQUEST));
    assert(!noah_qmk_keyboard_restart_frame_valid(NULL, sizeof(frame), NOAH_QMK_KEYBOARD_RESTART_REQUEST));
    // Every single-bit error is refused, the CRC's bytes included.
    for (uint8_t at = 0u; at < sizeof(frame); at++) {
        for (uint8_t bit = 0u; bit < 8u; bit++) {
            frame[at] ^= (uint8_t)(1u << bit);
            assert(!noah_qmk_keyboard_restart_frame_valid(frame, sizeof(frame), NOAH_QMK_KEYBOARD_RESTART_REQUEST));
            frame[at] ^= (uint8_t)(1u << bit);
        }
    }
    puts("the restart frame carries the confirmation and refuses any single-bit error");
}

static void test_the_other_half_takes_only_a_restart_frame(void) {
    uint8_t request[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE];
    uint8_t response[32];

    reset();
    noah_qmk_keyboard_restart_encode(NOAH_QMK_KEYBOARD_RESTART_REQUEST, request);
    // A garbled request, a request of another size, or a reply of another
    // size: zeros, and nothing scheduled.
    request[3] ^= 1u;
    memset(response, 0xAA, sizeof(response));
    slave_callback(sizeof(request), request, NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE, response);
    request[3] ^= 1u;
    for (uint8_t i = 0u; i < NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE; i++) assert(response[i] == 0u);
    slave_callback(sizeof(request) - 1u, request, NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE, response);
    memset(response, 0xAA, sizeof(response));
    slave_callback(sizeof(request), request, sizeof(response), response);
    for (uint8_t i = 0u; i < sizeof(response); i++) assert(response[i] == 0u);
    housekeeping_for(1000u);
    assert(!slave_restart_pending() && !slave_resets);
    // The request: acknowledged, and a restart once the acknowledgement had
    // time to go out, from housekeeping.
    uint32_t noted = clock_ms;
    slave_callback(sizeof(request), request, NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE, response);
    assert(noah_qmk_keyboard_restart_frame_valid(response, NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE, NOAH_QMK_KEYBOARD_RESTART_ACK));
    assert(slave_restart_pending() && !slave_resets);
    // A repeated request (its acknowledgement lost) keeps the first time.
    clock_ms += 5u;
    slave_callback(sizeof(request), request, NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE, response);
    assert(noah_qmk_keyboard_restart_frame_valid(response, NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE, NOAH_QMK_KEYBOARD_RESTART_ACK));
    assert(ms_until_reset(&slave_resets, noted) == NOAH_QMK_KEYBOARD_RESTART_SLAVE_DELAY_MS && !usb_resets);
    puts("the other half takes only an exact restart frame, acknowledges it and restarts from housekeeping");
}

static void test_both_halves_restart(void) {
    uint32_t asked;

    reset();
    asked = clock_ms;
    assert(noah_qmk_keyboard_restart_request());
    assert(exchanges == 1u && noah_qmk_keyboard_restart_pending() && slave_restart_pending() && clock_ms - asked < 5u);
    assert(!usb_resets && !slave_resets); // the reply goes out first
    // Asked again before it restarts: nothing more is sent.
    assert(noah_qmk_keyboard_restart_request() && exchanges == 1u);
    asked = clock_ms;
    assert(ms_until_reset(&slave_resets, asked) <= NOAH_QMK_KEYBOARD_RESTART_SLAVE_DELAY_MS && !usb_resets);
    assert(ms_until_reset(&usb_resets, asked) == NOAH_QMK_KEYBOARD_RESTART_USB_DELAY_MS);
    puts("both halves restart: the other half first, the master half once its reply is out");
}

static void test_a_busy_other_half_is_asked_again(void) {
    reset();
    slave_busy_attempts = 3u; // a sector erase holds its split thread off
    assert(noah_qmk_keyboard_restart_request() && exchanges == 4u && slave_restart_pending());
    puts("an other half busy erasing is asked again until it acknowledges");
}

static void test_an_unanswering_other_half_bounds_the_wait(void) {
    uint32_t asked;

    // The link down: the master half waits its bound, then restarts alone.
    reset();
    link_down = true;
    asked     = clock_ms;
    assert(!noah_qmk_keyboard_restart_request());
    assert(clock_ms - asked >= NOAH_QMK_KEYBOARD_RESTART_SLAVE_WAIT_MS && clock_ms - asked < NOAH_QMK_KEYBOARD_RESTART_SLAVE_WAIT_MS + NOAH_QMK_KEYBOARD_RESTART_SLAVE_RETRY_MS);
    assert(exchanges <= NOAH_QMK_KEYBOARD_RESTART_SLAVE_WAIT_MS / NOAH_QMK_KEYBOARD_RESTART_SLAVE_RETRY_MS + 1u);
    assert(noah_qmk_keyboard_restart_pending() && !slave_restart_pending());
    assert(!noah_qmk_keyboard_restart_request() && exchanges <= NOAH_QMK_KEYBOARD_RESTART_SLAVE_WAIT_MS / NOAH_QMK_KEYBOARD_RESTART_SLAVE_RETRY_MS + 1u);
    asked = clock_ms;
    assert(ms_until_reset(&usb_resets, asked) == NOAH_QMK_KEYBOARD_RESTART_USB_DELAY_MS && !slave_resets);
    // A half without the RPC answers zeros: not an acknowledgement.
    reset();
    slave_callback = NULL;
    assert(!noah_qmk_keyboard_restart_request() && exchanges > 1u && !slave_restart_pending());
    puts("an other half that does not answer bounds the wait; the master half still restarts");
}

int main(void) {
    test_frames();
    test_the_other_half_takes_only_a_restart_frame();
    test_both_halves_restart();
    test_a_busy_other_half_is_asked_again();
    test_an_unanswering_other_half_bounds_the_wait();
    puts("keyboard restart host tests passed");
    return 0;
}
