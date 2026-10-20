// ───────────────────────────────────────────────────────────────────────────
// QMK Keyboard Restart (D-F34)
// ───────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "qmk_keyboard_restart.h"

#include <string.h>

#include "../profile/storage/profile_checksum.h"
#include "../state/shared/runtime_publication.h"

// Set by the Raw HID handler on the USB half, by the split slave thread on the
// other; read by housekeeping. The time is written before the flag.
static volatile bool restart_due;
static uint32_t      restart_noted_at;
static uint16_t      restart_delay_ms;
static bool          restart_peer_acknowledged;

void noah_qmk_keyboard_restart_encode(uint8_t kind, uint8_t frame[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE]) {
    uint16_t crc;

    frame[0] = kind;
    for (uint8_t i = 0u; i < 4u; i++) {
        frame[1u + i] = (uint8_t)(NOAH_QMK_KEYBOARD_RESTART_CONFIRM >> (8u * i));
    }
    frame[5] = NOAH_QMK_KEYBOARD_RESTART_VERSION;
    crc      = noah_profile_crc16_ccitt_update(NOAH_PROFILE_CRC16_INITIAL, frame, 6u);
    frame[6] = (uint8_t)crc;
    frame[7] = (uint8_t)(crc >> 8u);
}

bool noah_qmk_keyboard_restart_frame_valid(const void *frame, uint8_t length, uint8_t kind) {
    uint8_t expected[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE];

    if (!frame || length != sizeof(expected)) {
        return false;
    }
    noah_qmk_keyboard_restart_encode(kind, expected);
    return memcmp(frame, expected, sizeof(expected)) == 0;
}

// The first request's time stands.
static void note_restart(uint16_t delay_ms) {
    if (restart_due) {
        return;
    }
    restart_noted_at = timer_read32();
    restart_delay_ms = delay_ms;
    NOAH_RUNTIME_PUBLICATION_BARRIER();
    restart_due = true;
}

#if defined(SPLIT_KEYBOARD) && defined(SPLIT_TRANSACTION_IDS_USER)
#    include "transactions.h" // QMK

_Static_assert(NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE <= RPC_M2S_BUFFER_SIZE && NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE <= RPC_S2M_BUFFER_SIZE, "a restart frame fits QMK's RPC buffers");

static bool restart_registered;

// The other half, on its split slave thread: notes the request and answers.
static void restart_rpc(uint8_t request_size, const void *request, uint8_t response_size, void *response) {
    if (!response || response_size == 0u) {
        return;
    }
    memset(response, 0, response_size);
    if (response_size != NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE || !noah_qmk_keyboard_restart_frame_valid(request, request_size, NOAH_QMK_KEYBOARD_RESTART_REQUEST)) {
        return;
    }
    note_restart(NOAH_QMK_KEYBOARD_RESTART_PEER_DELAY_MS);
    noah_qmk_keyboard_restart_encode(NOAH_QMK_KEYBOARD_RESTART_ACK, response);
}

static bool ask_peer(void) {
    uint8_t  request[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE];
    uint8_t  reply[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE];
    uint32_t started = timer_read32();

    noah_qmk_keyboard_restart_encode(NOAH_QMK_KEYBOARD_RESTART_REQUEST, request);
    for (;;) {
        memset(reply, 0, sizeof(reply));
        if (transaction_rpc_exec(PUT_KEYBOARD_RESTART, sizeof(request), request, sizeof(reply), reply) && noah_qmk_keyboard_restart_frame_valid(reply, sizeof(reply), NOAH_QMK_KEYBOARD_RESTART_ACK)) {
            return true;
        }
        if (timer_elapsed32(started) >= NOAH_QMK_KEYBOARD_RESTART_PEER_WAIT_MS) {
            return false;
        }
        wait_ms(NOAH_QMK_KEYBOARD_RESTART_PEER_RETRY_MS);
    }
}
#else
static bool ask_peer(void) {
    return false;
}
#endif

// Out of line: inlined, its frames widened the Raw HID dispatch every report
// passes through.
__attribute__((noinline)) bool noah_qmk_keyboard_restart_request(void) {
    if (!restart_due) {
        restart_peer_acknowledged = ask_peer();
        note_restart(NOAH_QMK_KEYBOARD_RESTART_USB_DELAY_MS);
    }
    return restart_peer_acknowledged;
}

bool noah_qmk_keyboard_restart_pending(void) {
    return restart_due;
}

void noah_qmk_keyboard_restart_task(void) {
#if defined(SPLIT_KEYBOARD) && defined(SPLIT_TRANSACTION_IDS_USER)
    if (!restart_registered) {
        transaction_register_rpc(PUT_KEYBOARD_RESTART, restart_rpc);
        restart_registered = true;
    }
#endif
    if (!restart_due) {
        return;
    }
    NOAH_RUNTIME_PUBLICATION_BARRIER();
    if (timer_elapsed32(restart_noted_at) >= restart_delay_ms) {
        soft_reset_keyboard();
    }
}

#ifdef NOAH_QMK_KEYBOARD_RESTART_TEST
void noah_qmk_keyboard_restart_reset_for_test(void) {
    restart_due               = false;
    restart_noted_at          = 0u;
    restart_delay_ms          = 0u;
    restart_peer_acknowledged = false;
#    if defined(SPLIT_KEYBOARD) && defined(SPLIT_TRANSACTION_IDS_USER)
    restart_registered = false;
#    endif
}
#endif
