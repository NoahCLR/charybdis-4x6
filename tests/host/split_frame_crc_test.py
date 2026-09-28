"""Drive the fork's real split serial protocol over a fault-injectable link.

Compiles QMK's platforms/chibios/drivers/serial_protocol.c twice, once per
half, together with the frame-CRC resend flags, send_if_condition, the RPC
table entries and the RPC sequence functions extracted from
quantum/split_common/transactions.c and the serial transport_execute_transaction
from quantum/split_common/transport.c. The halves run in two threads joined by
a byte queue per direction that can corrupt any byte (split_frame_crc_harness.c).

Variants: both halves with the frame CRC, both without (upstream behaviour),
and each mixed pair.
"""

from pathlib import Path
import re
import subprocess
import sys

qmk, build, root = map(Path, sys.argv[1:])
transactions = (qmk / "quantum/split_common/transactions.c").read_text()
transport = (qmk / "quantum/split_common/transport.c").read_text()


def between(text, start, end, include_end=False):
    first = text.index(start)
    last = text.index(end, first + len(start))
    return text[first:last + (len(end) if include_end else 0)]


macros = "\n".join(line for line in transactions.splitlines() if re.match(r"#define (sizeof_member|trans_|transport_(write|read|exec)\b)", line))
forward = between(transactions, "#if defined(SPLIT_TRANSACTION_RPC)\n// Forward-declare", "#endif // defined(SPLIT_TRANSACTION_RPC)", include_end=True)
resends = between(transactions, "// Frame CRC resends", "////////////////////////////////////////////////////\n// Helpers")
send_if = between(transactions, "inline static bool send_if_condition(", "inline static bool send_if_data_mismatch(")
rpc_table = between(transactions, "#if defined(SPLIT_TRANSACTION_RPC)\n        [PUT_RPC_INFO]", "#endif // defined(SPLIT_TRANSACTION_RPC)\n};", include_end=False) + "#endif\n"
rpc_functions = transactions[transactions.index("#if defined(SPLIT_TRANSACTION_RPC)\n\nvoid transaction_register_rpc"):]
# The I2C branch defines the same function first; take the serial one.
serial_transport = between(transport[transport.index("#else // USE_I2C"):], "bool transport_execute_transaction", "#endif // USE_I2C")

stubs = build / "stubs"
stubs.mkdir(exist_ok=True)
(stubs / "transactions.h").write_text(r'''
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#define SPLIT_TRANSACTION_RPC
#define RPC_M2S_BUFFER_SIZE 32
#define RPC_S2M_BUFFER_SIZE 32
#define FORCED_SYNC_THROTTLE_MS 100
enum serial_transaction_id {
    GET_SLAVE_MATRIX_CHECKSUM, GET_SLAVE_MATRIX_DATA, PUT_LAYER_STATE,
    PUT_RPC_INFO, PUT_RPC_REQ_DATA, EXECUTE_RPC, GET_RPC_RESP_DATA, PUT_USER_RPC,
    NUM_TOTAL_TRANSACTIONS
};
typedef struct { uint8_t checksum; struct { int8_t transaction_id; uint8_t m2s_length; uint8_t s2m_length; } payload; } rpc_sync_info_t;
typedef struct {
    struct { uint8_t matrix[5]; uint8_t checksum; } smatrix;
    uint32_t        layer_state;
    rpc_sync_info_t rpc_info;
    uint8_t         rpc_m2s_buffer[RPC_M2S_BUFFER_SIZE];
    uint8_t         rpc_s2m_buffer[RPC_S2M_BUFFER_SIZE];
} split_shared_memory_t;
extern split_shared_memory_t *const split_shmem;
typedef void (*slave_callback_t)(uint8_t, const void *, uint8_t, void *);
typedef struct _split_transaction_desc_t {
    uint8_t initiator2target_buffer_size; uint16_t initiator2target_offset;
    uint8_t target2initiator_buffer_size; uint16_t target2initiator_offset;
    slave_callback_t slave_callback;
} split_transaction_desc_t;
extern split_transaction_desc_t split_transaction_table[NUM_TOTAL_TRANSACTIONS];
#define split_shmem_offset_ptr(offset) (((uint8_t *)split_shmem) + (offset))
#define split_trans_initiator2target_buffer(trans) (split_shmem_offset_ptr((trans)->initiator2target_offset))
#define split_trans_target2initiator_buffer(trans) (split_shmem_offset_ptr((trans)->target2initiator_offset))
void split_transaction_crc_dropped(uint8_t id);
void split_transaction_diagnostic_crc(uint8_t id);
void split_transaction_diagnostic(uint8_t id, uint8_t request_bytes, uint8_t response_bytes, uint32_t elapsed_us, bool success);
bool transport_execute_transaction(int8_t id, const void *i2t, uint16_t i2t_length, void *t2i, uint16_t t2i_length);
bool transaction_rpc_exec(int8_t transaction_id, uint8_t m2s_size, const void *m2s, uint8_t s2m_size, void *s2m);
void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback);
bool soft_serial_transaction(int index);
bool is_transport_connected(void);
uint32_t timer_read32(void);
uint32_t timer_elapsed32(uint32_t then);
uint8_t crc8(const void *data, size_t length);
bool harness_send_if_condition(int8_t id, uint32_t *last_update, bool condition, void *source, size_t length);
bool harness_resend_due(int8_t id);
''')
(stubs / "ch.h").write_text(r'''
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define THD_WORKING_AREA(name, size) int name[1]
#define THD_FUNCTION(name, arg) void *name(void *arg)
#define HIGHPRIO 0
#define unlikely(x) (x)
void chRegSetThreadName(const char *name);
void chThdCreateStatic(void *area, size_t size, int priority, void *(*function)(void *), void *arg);
uint32_t chSysGetRealtimeCounterX(void);
''')
(stubs / "synchronization_util.h").write_text("#pragma once\n#define split_shared_memory_lock_autounlock() ((void)0)\n")

names = ["soft_serial_transaction", "soft_serial_initiator_init", "soft_serial_target_init", "serial_transport_send", "serial_transport_receive",
         "serial_transport_receive_blocking", "serial_transport_driver_clear", "serial_transport_driver_slave_init", "serial_transport_driver_master_init",
         "split_transaction_table", "split_shmem", "split_transaction_crc_dropped", "split_transaction_diagnostic_crc", "split_transaction_diagnostic",
         "transport_execute_transaction", "transaction_rpc_exec", "transaction_register_rpc", "slave_rpc_info_callback", "slave_rpc_exec_callback",
         "slave_rpc_request_callback", "is_transport_connected", "harness_send_if_condition", "harness_resend_due"]
for prefix in ("m", "s"):
    (stubs / f"instance_{prefix}.h").write_text("".join(f"#define {name} {prefix}_{name}\n" for name in names))

part = build / "transactions_part.c"
part.write_text(f'''
#include <transactions.h>
{macros}
{forward}
{resends}
{send_if}
static split_shared_memory_t shared_memory;
split_shared_memory_t *const split_shmem = &shared_memory;
{serial_transport}
split_transaction_desc_t split_transaction_table[NUM_TOTAL_TRANSACTIONS] = {{
    [GET_SLAVE_MATRIX_CHECKSUM] = trans_target2initiator_initializer(smatrix.checksum),
    [GET_SLAVE_MATRIX_DATA]     = trans_target2initiator_initializer(smatrix.matrix),
    [PUT_LAYER_STATE]           = trans_initiator2target_initializer(layer_state),
{rpc_table}}};
{rpc_functions}
bool harness_send_if_condition(int8_t id, uint32_t *last_update, bool condition, void *source, size_t length) {{
    return send_if_condition(id, last_update, condition, source, length);
}}
bool harness_resend_due(int8_t id) {{
    return split_resend_due(id);
}}
''')

protocol = qmk / "platforms/chibios/drivers/serial_protocol.c"
includes = ["-I", str(stubs), "-I", str(qmk / "drivers"), "-I", str(qmk / "quantum"), "-I", str(qmk / "platforms/chibios/drivers")]
flags = ["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter", "-Wno-unused-function", "-DSPLIT_TRANSACTION_DIAGNOSTICS"]
for variant, master_crc, slave_crc in (("crc", True, True), ("plain", False, False), ("master_crc", True, False), ("slave_crc", False, True)):
    objects = []
    for prefix, crc in (("m", master_crc), ("s", slave_crc)):
        defines = ["-DSPLIT_TRANSPORT_CRC"] if crc else []
        for source in (protocol, part):
            obj = build / f"{variant}_{prefix}_{source.stem}.o"
            subprocess.run(flags + defines + includes + ["-include", str(stubs / f"instance_{prefix}.h"), "-c", str(source), "-o", str(obj)], check=True)
            objects.append(str(obj))
    binary = build / f"frame_crc_{variant}"
    harness_defines = [f"-DMASTER_CRC={int(master_crc)}", f"-DSLAVE_CRC={int(slave_crc)}"]
    subprocess.run(flags + harness_defines + includes + [str(root / "tests/host/split_frame_crc_harness.c"), str(qmk / "quantum/crc.c"), *objects, "-lpthread", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
