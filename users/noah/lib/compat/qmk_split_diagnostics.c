#include "qmk_split_diagnostics.h"
#ifdef SPLIT_TRANSACTION_DIAGNOSTICS
#    include QMK_KEYBOARD_H
#    include "transactions.h"
#    include <string.h>
#    ifndef QMK_SPLIT_ACTIVITY_POLICY_VERSION
#        error "Split diagnostics require the instrumented QMK fork"
#    endif

// Only the master main thread records/reads these counters. Capture is bounded
// and explicitly armed after boot; USB readback is refused until frozen.
typedef struct {
    uint32_t attempts, failures, attempted_bytes, total_us, max_us;
    uint16_t crc_failures;
} split_measurement_t;
static split_measurement_t measurements[NUM_TOTAL_TRANSACTIONS];
static uint32_t            started, duration;
static bool                armed, frozen;
_Static_assert(NUM_TOTAL_TRANSACTIONS < 255, "diagnostic page index overflow");

static uint32_t clock_us(void) {
    return chSysGetRealtimeCounterX();
}
static void freeze_if_due(void) {
    if (armed && (uint32_t)(clock_us() - started) >= 10000000u) {
        duration = clock_us() - started;
        armed    = false;
        frozen   = true;
    }
}
void split_transaction_diagnostic(uint8_t id, uint8_t request_bytes, uint8_t response_bytes, uint32_t elapsed_us, bool success) {
    if (!armed || id >= NUM_TOTAL_TRANSACTIONS) return;
    // Record a transaction that straddles the deadline before freezing, so its
    // cost is not dropped. Capture duration includes this final transaction.
    split_measurement_t *m = &measurements[id];
    ++m->attempts;
    m->failures += !success;
    m->attempted_bytes += 2u + request_bytes + response_bytes;
    m->total_us += elapsed_us;
    if (elapsed_us > m->max_us) m->max_us = elapsed_us;
    freeze_if_due();
}
#    ifdef SPLIT_TRANSPORT_CRC
// A frame of this transaction failed its CRC: a write the slave dropped and
// reported, or a read this half rejected.
void split_transaction_diagnostic_crc(uint8_t id) {
    if (!armed || id >= NUM_TOTAL_TRANSACTIONS) return;
    if (measurements[id].crc_failures != UINT16_MAX) ++measurements[id].crc_failures;
}
#    endif
static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (uint8_t)(v >> (8 * i));
}
bool noah_split_diagnostics_command(uint8_t *data, uint8_t length) {
    // VIA custom channel 0, reserved diagnostic value 0x0A. SET starts capture;
    // GET page 0 reads metadata, pages 1..N read transaction counters.
    if (!data || length != 32 || data[1] != 0 || data[2] != 0x0a || (data[0] != 7 && data[0] != 8)) return false;
    uint8_t command = data[0], page = data[4];
    bool    malformed = data[3] == 0;
    for (unsigned i = 5; i < 32; ++i)
        malformed |= data[i] != 0;
    memset(data + 5, 0, 27);
    if (malformed || (command == 8 && page != 0)) {
        data[5] = 1;
        return true;
    }
    if (!is_keyboard_master()) {
        data[5] = 3;
        return true;
    }
    if (command == 8) {
        memset(measurements, 0, sizeof(measurements));
        started  = clock_us();
        duration = 0;
        armed    = true;
        frozen   = false;
        return true;
    }
    freeze_if_due();
    if (page > NUM_TOTAL_TRANSACTIONS) {
        data[5] = 2;
        return true;
    }
    data[6] = 25;
    data[7] = 2; // format version
    if (page == 0) {
        data[8]  = NUM_TOTAL_TRANSACTIONS;
        data[9]  = armed;
        data[10] = frozen;
        put32(data + 11, frozen ? duration : clock_us() - started);
        data[15] = PUT_ACTIVITY;
        return true;
    }
    if (!frozen) {
        memset(data + 6, 0, 26);
        data[5] = 3;
        return true;
    }
    const split_measurement_t *m = &measurements[page - 1];
    data[8]                      = page - 1;
    put32(data + 9, m->attempts);
    put32(data + 13, m->failures);
    put32(data + 17, m->attempted_bytes);
    put32(data + 21, m->total_us);
    put32(data + 25, m->max_us);
    data[29] = (uint8_t)m->crc_failures;
    data[30] = (uint8_t)(m->crc_failures >> 8);
    return true;
}
#endif
