// Fault-injection harness for the fork's split serial protocol. Built by
// split_frame_crc_test.py with MASTER_CRC and SLAVE_CRC selecting whether each
// half was compiled with SPLIT_TRANSPORT_CRC. The master runs on this thread,
// the slave on the thread its own soft_serial_target_init() starts.

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <transactions.h>

#define LINK_TIMEOUT_MS 20

// Each half's symbols, renamed per instance by the build.
#define HALF_DECLARATIONS(p)                                                                                                     \
    void p##_soft_serial_initiator_init(void);                                                                                   \
    void p##_soft_serial_target_init(void);                                                                                      \
    bool p##_transport_execute_transaction(int8_t, const void *, uint16_t, void *, uint16_t);                                    \
    bool p##_transaction_rpc_exec(int8_t, uint8_t, const void *, uint8_t, void *);                                               \
    void p##_transaction_register_rpc(int8_t, slave_callback_t);                                                                 \
    bool p##_harness_send_if_condition(int8_t, uint32_t *, bool, void *, size_t);                                                \
    bool p##_harness_resend_due(int8_t);                                                                                         \
    extern split_shared_memory_t *const p##_split_shmem;                                                                         \
    extern split_transaction_desc_t     p##_split_transaction_table[NUM_TOTAL_TRANSACTIONS];
HALF_DECLARATIONS(m)
HALF_DECLARATIONS(s)

// ── The link: one byte queue per direction ────────────────────────────────
typedef struct {
    uint8_t bytes[4096];
    size_t  head, tail;
    long    sent; // bytes sent since the last fault was armed
} queue_t;

enum { TO_SLAVE, TO_MASTER };
static queue_t         queues[2];
static pthread_mutex_t link_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  link_moved = PTHREAD_COND_INITIALIZER;
static bool            slave_idle;
static bool            slave_holds_frame; // one shot: keep the next frame until the master's next id is queued

static struct {
    bool    armed;
    int     direction;
    long    index;
    uint8_t mask;
    bool    drop;
} fault;

static size_t available(const queue_t *q) {
    return q->tail - q->head;
}

static void deadline_in(struct timespec *deadline, long ms) {
    clock_gettime(CLOCK_REALTIME, deadline);
    deadline->tv_nsec += ms * 1000000L;
    deadline->tv_sec += deadline->tv_nsec / 1000000000L;
    deadline->tv_nsec %= 1000000000L;
}

static bool link_send(int direction, const uint8_t *source, size_t size) {
    pthread_mutex_lock(&link_lock);
    queue_t *q = &queues[direction];
    for (size_t i = 0; i < size; ++i, ++q->sent) {
        uint8_t byte = source[i];
        if (fault.armed && fault.direction == direction && fault.index == q->sent) {
            fault.armed = false;
            if (fault.drop) continue;
            byte ^= fault.mask;
        }
        assert(q->tail < sizeof(q->bytes));
        q->bytes[q->tail++] = byte;
    }
    pthread_cond_broadcast(&link_moved);
    pthread_mutex_unlock(&link_lock);
    return true;
}

// Like ChibiOS chnReadTimeout: a timeout consumes whatever arrived.
static bool link_receive(int direction, uint8_t *destination, size_t size, bool forever) {
    struct timespec deadline;
    bool            ok = true;
    pthread_mutex_lock(&link_lock);
    queue_t *q = &queues[direction];
    deadline_in(&deadline, LINK_TIMEOUT_MS);
    size_t wanted = size;
    if (direction == TO_SLAVE && !forever && slave_holds_frame) {
        slave_holds_frame = false;
        ++wanted;
    }
    while (available(q) < wanted) {
        if (forever) {
            slave_idle = available(q) == 0;
            pthread_cond_broadcast(&link_moved);
            pthread_cond_wait(&link_moved, &link_lock);
        } else if (pthread_cond_timedwait(&link_moved, &link_lock, &deadline) != 0) {
            ok = false;
            break;
        }
    }
    if (forever) slave_idle = false;
    size_t got = available(q) < size ? available(q) : size;
    memcpy(destination, &q->bytes[q->head], got);
    q->head += got;
    pthread_mutex_unlock(&link_lock);
    return ok && got == size;
}

static void link_clear(int direction) {
    pthread_mutex_lock(&link_lock);
    queues[direction].head = queues[direction].tail;
    pthread_mutex_unlock(&link_lock);
}

static void wait_slave_idle(void) {
    pthread_mutex_lock(&link_lock);
    while (!(slave_idle && available(&queues[TO_SLAVE]) == 0))
        pthread_cond_wait(&link_moved, &link_lock);
    pthread_mutex_unlock(&link_lock);
}

// Corrupt (or drop) the index-th byte sent in direction after this call.
static void arm_fault(int direction, long index, uint8_t mask, bool drop) {
    wait_slave_idle();
    pthread_mutex_lock(&link_lock);
    queues[direction].sent = 0;
    fault.armed = true;
    fault.direction = direction;
    fault.index = index;
    fault.mask = mask;
    fault.drop = drop;
    pthread_mutex_unlock(&link_lock);
}

#define LINK_HALF(p, rx, tx)                                                                                                     \
    bool p##_serial_transport_send(const uint8_t *source, const size_t size) { return link_send(tx, source, size); }             \
    bool p##_serial_transport_receive(uint8_t *destination, const size_t size) { return link_receive(rx, destination, size, false); } \
    bool p##_serial_transport_receive_blocking(uint8_t *destination, const size_t size) { return link_receive(rx, destination, size, true); } \
    void p##_serial_transport_driver_clear(void) { link_clear(rx); }                                                             \
    void p##_serial_transport_driver_slave_init(void) {}                                                                         \
    void p##_serial_transport_driver_master_init(void) {}                                                                        \
    bool p##_is_transport_connected(void) { return true; }
LINK_HALF(m, TO_MASTER, TO_SLAVE)
LINK_HALF(s, TO_SLAVE, TO_MASTER)

// ── Platform and diagnostics stubs ────────────────────────────────────────
static uint32_t now;
uint32_t timer_read32(void) { return now; }
uint32_t timer_elapsed32(uint32_t then) { return now - then; }
uint32_t chSysGetRealtimeCounterX(void) { return now; }
void     chRegSetThreadName(const char *name) { (void)name; }
void     chThdCreateStatic(void *area, size_t size, int priority, void *(*function)(void *), void *arg) {
    pthread_t thread;
    (void)area; (void)size; (void)priority;
    assert(pthread_create(&thread, NULL, function, arg) == 0);
    pthread_detach(thread);
}

static struct {
    uint8_t request, response;
    bool    success;
    unsigned crc_failures;
} diagnostics[NUM_TOTAL_TRANSACTIONS];
void m_split_transaction_diagnostic(uint8_t id, uint8_t request, uint8_t response, uint32_t elapsed, bool success) {
    (void)elapsed;
    diagnostics[id].request = request;
    diagnostics[id].response = response;
    diagnostics[id].success = success;
}
void m_split_transaction_diagnostic_crc(uint8_t id) { ++diagnostics[id].crc_failures; }
void s_split_transaction_diagnostic(uint8_t id, uint8_t request, uint8_t response, uint32_t elapsed, bool success) {
    (void)id; (void)request; (void)response; (void)elapsed; (void)success;
}
void s_split_transaction_diagnostic_crc(uint8_t id) { (void)id; assert(!"the slave counts no CRC failures"); }

// ── The slave's user RPC ──────────────────────────────────────────────────
static uint8_t  rpc_payload[RPC_M2S_BUFFER_SIZE];
static uint8_t  rpc_payload_size;
static unsigned rpc_calls;
static void     user_rpc(uint8_t m2s_size, const void *m2s, uint8_t s2m_size, void *s2m) {
    memcpy(rpc_payload, m2s, m2s_size);
    rpc_payload_size = m2s_size;
    ++rpc_calls;
    for (uint8_t i = 0; i < s2m_size; ++i) ((uint8_t *)s2m)[i] = (uint8_t)(0xA0 + i);
}

// ── Master operations ─────────────────────────────────────────────────────
static bool write_layer(uint32_t value) {
    return m_transport_execute_transaction(PUT_LAYER_STATE, &value, sizeof(value), NULL, 0);
}
static bool read_matrix(uint8_t matrix[5]) {
    return m_transport_execute_transaction(GET_SLAVE_MATRIX_DATA, NULL, 0, matrix, 5);
}
static bool read_checksum(uint8_t *checksum) {
    return m_transport_execute_transaction(GET_SLAVE_MATRIX_CHECKSUM, NULL, 0, checksum, 1);
}
static bool rpc(uint8_t size, uint8_t seed, uint8_t reply[4]) {
    uint8_t request[RPC_M2S_BUFFER_SIZE];
    for (uint8_t i = 0; i < size; ++i) request[i] = (uint8_t)(seed + i);
    bool ok = m_transaction_rpc_exec(PUT_USER_RPC, size, request, 4, reply);
    wait_slave_idle();
    return ok;
}
static bool rpc_delivered(uint8_t size, uint8_t seed) {
    if (rpc_payload_size != size) return false;
    for (uint8_t i = 0; i < size; ++i)
        if (rpc_payload[i] != (uint8_t)(seed + i)) return false;
    return true;
}

// Byte offsets of an RPC sent from the master: info, request, execute.
enum { INFO_DATA = 1, REQUEST_ID = 6 };
#define REQUEST_DATA (REQUEST_ID + 1)
#define EXECUTE_DATA(size) (REQUEST_DATA + (size) + 1 + 1)
// And to the master: four handshakes, then the reply.
enum { REPLY_DATA = 4 };

#if MASTER_CRC && SLAVE_CRC
static void test_clean_link(void) {
    uint8_t matrix[5], checksum = 0, reply[4];
    assert(write_layer(0x11223344u));
    wait_slave_idle();
    assert(s_split_shmem->layer_state == 0x11223344u);
    assert(diagnostics[PUT_LAYER_STATE].request == 5 && diagnostics[PUT_LAYER_STATE].success);
    memcpy(s_split_shmem->smatrix.matrix, (uint8_t[5]){1, 2, 3, 4, 5}, 5);
    s_split_shmem->smatrix.checksum = 0x5A;
    assert(read_matrix(matrix) && memcmp(matrix, (uint8_t[5]){1, 2, 3, 4, 5}, 5) == 0);
    assert(diagnostics[GET_SLAVE_MATRIX_DATA].response == 6);
    // The checksum poll carries no CRC.
    assert(read_checksum(&checksum) && checksum == 0x5A);
    assert(diagnostics[GET_SLAVE_MATRIX_CHECKSUM].response == 1);
    assert(rpc(8, 10, reply) && rpc_calls == 1 && rpc_delivered(8, 10));
    assert(memcmp(reply, (uint8_t[4]){0xA0, 0xA1, 0xA2, 0xA3}, 4) == 0);
    for (int id = 0; id < NUM_TOTAL_TRANSACTIONS; ++id) {
        assert(diagnostics[id].crc_failures == 0);
        assert(!m_harness_resend_due((int8_t)id));
    }
}

// A garbled write is not committed, is reported in the next handshake and is
// resent by send_if_condition on the next scan.
static void test_dropped_write_is_reported_and_resent(void) {
    uint8_t  checksum;
    uint32_t value = 0x55667788u, last_update = now;
    assert(s_split_shmem->layer_state == 0x11223344u);
    arm_fault(TO_SLAVE, 1, 0x04, false);
    // The slave keeps the bad frame until the next id is queued behind it:
    // a CRC failure must leave that id in the queue.
    slave_holds_frame = true;
    assert(write_layer(value));
    assert(read_checksum(&checksum));
    wait_slave_idle();
    assert(s_split_shmem->layer_state == 0x11223344u);
    assert(m_harness_resend_due(PUT_LAYER_STATE) && diagnostics[PUT_LAYER_STATE].crc_failures == 1);
    // Nothing changed and no forced resend is due, but the drop is.
    assert(m_harness_send_if_condition(PUT_LAYER_STATE, &last_update, false, &value, sizeof(value)));
    wait_slave_idle();
    assert(s_split_shmem->layer_state == 0x55667788u && !m_harness_resend_due(PUT_LAYER_STATE));
    assert(m_harness_send_if_condition(PUT_LAYER_STATE, &last_update, false, &value, sizeof(value)));
    assert(diagnostics[PUT_LAYER_STATE].crc_failures == 1);
}

// A garbled handshake loses the report with its transaction; the forced
// resend is then the fallback.
static void test_garbled_report_is_lost(void) {
    uint8_t checksum;
    arm_fault(TO_SLAVE, 1, 0x01, false);
    assert(write_layer(0x01010101u));
    wait_slave_idle();
    arm_fault(TO_MASTER, 0, 0x02, false);
    assert(!read_checksum(&checksum));
    assert(read_checksum(&checksum));
    assert(!m_harness_resend_due(PUT_LAYER_STATE) && diagnostics[PUT_LAYER_STATE].crc_failures == 1);
    assert(s_split_shmem->layer_state == 0x55667788u);
}

static void test_garbled_read_is_rejected_and_retried(void) {
    uint8_t matrix[5] = {9, 9, 9, 9, 9};
    arm_fault(TO_MASTER, 1, 0x10, false);
    assert(!read_matrix(matrix));
    assert(memcmp(matrix, (uint8_t[5]){9, 9, 9, 9, 9}, 5) == 0);
    assert(diagnostics[GET_SLAVE_MATRIX_DATA].crc_failures == 1);
    assert(read_matrix(matrix) && memcmp(matrix, (uint8_t[5]){1, 2, 3, 4, 5}, 5) == 0);
}

// A garbled checksum poll is taken as read; the data read that a changed
// checksum triggers carries the CRC.
static void test_checksum_poll_has_no_crc(void) {
    uint8_t checksum = 0;
    arm_fault(TO_MASTER, 1, 0x01, false);
    assert(read_checksum(&checksum) && checksum == 0x5B);
    assert(diagnostics[GET_SLAVE_MATRIX_CHECKSUM].crc_failures == 0);
}

// A garbled frame at any RPC step stops the sequence on the master, and the
// slave never runs the callback on a stale or partial sequence.
static void test_rpc_step_failures(void) {
    uint8_t reply[4];
    unsigned calls = rpc_calls;

    arm_fault(TO_SLAVE, INFO_DATA, 0x01, false);
    assert(!rpc(8, 20, reply) && rpc_calls == calls);
    assert(rpc(8, 21, reply) && rpc_calls == ++calls && rpc_delivered(8, 21));

    memset(s_split_shmem->rpc_s2m_buffer, 0xEE, RPC_S2M_BUFFER_SIZE);
    arm_fault(TO_SLAVE, REQUEST_DATA + 3, 0x01, false);
    assert(!rpc(8, 30, reply) && rpc_calls == calls);
    for (int i = 0; i < RPC_S2M_BUFFER_SIZE; ++i) assert(s_split_shmem->rpc_s2m_buffer[i] == 0);
    assert(rpc(8, 31, reply) && rpc_calls == ++calls && rpc_delivered(8, 31));

    arm_fault(TO_SLAVE, EXECUTE_DATA(8), 0x01, false);
    assert(!rpc(8, 40, reply) && rpc_calls == calls);
    assert(rpc(8, 41, reply) && rpc_calls == ++calls);

    arm_fault(TO_MASTER, REPLY_DATA, 0x01, false);
    assert(!rpc(8, 50, reply) && diagnostics[GET_RPC_RESP_DATA].crc_failures == 1);
    ++calls; // the slave ran it; the caller's retry repeats an idempotent request
    assert(rpc(8, 51, reply) && rpc_calls == ++calls && memcmp(reply, (uint8_t[4]){0xA0, 0xA1, 0xA2, 0xA3}, 4) == 0);
}

// After a dropped info the slave still expects the previous request length.
static void test_stale_request_length(void) {
    uint8_t reply[4];
    unsigned calls = rpc_calls;
    // Longer than expected: the frame fails its CRC.
    assert(rpc(4, 60, reply) && rpc_calls == ++calls);
    arm_fault(TO_SLAVE, INFO_DATA, 0x01, false);
    assert(!rpc(20, 61, reply) && rpc_calls == calls);
    assert(rpc(20, 62, reply) && rpc_calls == ++calls && rpc_delivered(20, 62));
    // Shorter than expected: the slave times out waiting for the rest.
    arm_fault(TO_SLAVE, INFO_DATA, 0x01, false);
    assert(!rpc(4, 63, reply) && rpc_calls == calls);
    assert(rpc(4, 64, reply) && rpc_calls == ++calls && rpc_delivered(4, 64));
}

// A lost byte shifts the frame; the slave eats the next id as its CRC byte.
static void test_lost_byte_recovers(void) {
    uint8_t checksum;
    arm_fault(TO_SLAVE, 2, 0, true);
    assert(write_layer(0x0BADF00Du));
    assert(!read_checksum(&checksum));
    wait_slave_idle();
    assert(s_split_shmem->layer_state != 0x0BADF00Du);
    assert(read_checksum(&checksum));
}
#endif

#if !MASTER_CRC && !SLAVE_CRC
// Upstream behaviour, kept when the CRC is off: no extra byte, and a garbled
// write is committed.
static void test_without_crc(void) {
    uint8_t matrix[5], reply[4];
    assert(write_layer(0x11223344u));
    wait_slave_idle();
    assert(s_split_shmem->layer_state == 0x11223344u && diagnostics[PUT_LAYER_STATE].request == 4);
    memcpy(s_split_shmem->smatrix.matrix, (uint8_t[5]){1, 2, 3, 4, 5}, 5);
    assert(read_matrix(matrix) && diagnostics[GET_SLAVE_MATRIX_DATA].response == 5);
    assert(rpc(8, 10, reply) && rpc_calls == 1 && rpc_delivered(8, 10));
    arm_fault(TO_SLAVE, 1, 0x04, false);
    assert(write_layer(0x55667788u));
    wait_slave_idle();
    assert(s_split_shmem->layer_state == 0x5566778Cu);
}
#endif

#if MASTER_CRC != SLAVE_CRC
// A mixed pair fails at the handshake, before any data is sent.
static void test_mixed_pair(void) {
    uint8_t matrix[5], reply[4];
    assert(!write_layer(0x11223344u));
    wait_slave_idle();
    assert(s_split_shmem->layer_state == 0);
    assert(!read_matrix(matrix));
    assert(!rpc(8, 10, reply) && rpc_calls == 0);
}
#endif

int main(void) {
    s_soft_serial_target_init();
    m_soft_serial_initiator_init();
    s_transaction_register_rpc(PUT_USER_RPC, user_rpc);
    m_transaction_register_rpc(PUT_USER_RPC, user_rpc);
#if MASTER_CRC && SLAVE_CRC
    test_clean_link();
    test_dropped_write_is_reported_and_resent();
    test_garbled_report_is_lost();
    test_garbled_read_is_rejected_and_retried();
    test_checksum_poll_has_no_crc();
    test_rpc_step_failures();
    test_stale_request_length();
    test_lost_byte_recovers();
    puts("split frame CRC tests passed: both halves with the CRC");
#elif !MASTER_CRC && !SLAVE_CRC
    test_without_crc();
    puts("split frame CRC tests passed: both halves without the CRC");
#else
    test_mixed_pair();
    printf("split frame CRC tests passed: mixed pair (master %s CRC) fails at the handshake\n", MASTER_CRC ? "with" : "without");
#endif
    return 0;
}
