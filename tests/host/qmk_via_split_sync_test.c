#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transactions.h"
#include "via.h"
#include "users/noah/lib/compat/qmk_via_split_sync.h"

static bool             fake_is_master;
static int8_t           rpc_registered_id;
static slave_callback_t rpc_registered_callback;
static int8_t           rpc_last_send_id;
static uint8_t          rpc_send_count;
static uint8_t          rpc_last_size;
static uint8_t          rpc_last_data[RPC_M2S_BUFFER_SIZE];
static uint8_t          rgb_invalidate_count;
static uint8_t          set_keycode_calls;
static uint8_t          set_buffer_calls;
static uint8_t          reset_calls;
static uint8_t          eeprom_reset_calls;
static uint8_t          via_eeprom_set_valid_calls;
static bool             via_eeprom_last_valid;
static uint8_t          last_layer;
static uint8_t          last_row;
static uint8_t          last_col;
static uint16_t         last_keycode;
static uint16_t         last_buffer_offset;
static uint8_t          last_buffer_size;
static uint8_t          last_buffer_bytes[RPC_M2S_BUFFER_SIZE];

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

static void test_reset(void) {
    fake_is_master          = true;
    rpc_registered_id       = -1;
    rpc_registered_callback = NULL;
    rpc_last_send_id        = -1;
    rpc_send_count          = 0;
    rpc_last_size           = 0;
    memset(rpc_last_data, 0, sizeof(rpc_last_data));
    rgb_invalidate_count       = 0;
    set_keycode_calls          = 0;
    set_buffer_calls           = 0;
    reset_calls                = 0;
    eeprom_reset_calls         = 0;
    via_eeprom_set_valid_calls = 0;
    via_eeprom_last_valid      = true;
    last_layer                 = 0;
    last_row                   = 0;
    last_col                   = 0;
    last_keycode               = 0;
    last_buffer_offset         = 0;
    last_buffer_size           = 0;
    memset(last_buffer_bytes, 0, sizeof(last_buffer_bytes));
}

bool is_keyboard_master(void) {
    return fake_is_master;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    rpc_registered_id       = transaction_id;
    rpc_registered_callback = callback;
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer) {
    CHECK(initiator2target_buffer_size <= RPC_M2S_BUFFER_SIZE);
    CHECK(initiator2target_buffer != NULL);

    rpc_last_send_id = transaction_id;
    rpc_send_count++;
    rpc_last_size = initiator2target_buffer_size;
    memcpy(rpc_last_data, initiator2target_buffer, initiator2target_buffer_size);
    return true;
}

void noah_rgb_runtime_invalidate_layer_maps(void) {
    rgb_invalidate_count++;
}

uint16_t dynamic_keymap_macro_get_buffer_size(void) {
    return 0;
}

void dynamic_keymap_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    (void)offset;
    (void)size;
    (void)data;
}

bool via_eeprom_is_valid(void) {
    return true;
}

void dynamic_keymap_set_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode) {
    set_keycode_calls++;
    last_layer   = layer;
    last_row     = row;
    last_col     = column;
    last_keycode = keycode;
}

void dynamic_keymap_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    CHECK(data != NULL);
    CHECK(size <= sizeof(last_buffer_bytes));

    set_buffer_calls++;
    last_buffer_offset = offset;
    last_buffer_size   = size;
    memcpy(last_buffer_bytes, data, size);
}

void dynamic_keymap_reset(void) {
    reset_calls++;
}

void via_eeprom_set_valid(bool valid) {
    via_eeprom_set_valid_calls++;
    via_eeprom_last_valid = valid;
}

void eeconfig_init_via(void) {
    eeprom_reset_calls++;
}

static void test_init_registers_rpc(void) {
    test_reset();
    fake_is_master = false;

    noah_qmk_via_split_sync_init();

    CHECK(rpc_registered_id == PUT_VIA_KEYMAP_SYNC);
    CHECK(rpc_registered_callback != NULL);
}

static void test_master_command_sends_packet(void) {
    uint8_t command[] = {id_dynamic_keymap_set_keycode, 2, 3, 4, 0x12, 0x34};

    test_reset();

    noah_qmk_via_split_sync_command(command, (uint8_t)sizeof(command));

    CHECK(rpc_send_count == 1);
    CHECK(rpc_last_send_id == PUT_VIA_KEYMAP_SYNC);
    CHECK(rpc_last_size == sizeof(command));
    CHECK(memcmp(rpc_last_data, command, sizeof(command)) == 0);
}

static void test_slave_does_not_send_packet(void) {
    uint8_t command[] = {id_dynamic_keymap_set_keycode, 2, 3, 4, 0x12, 0x34};

    test_reset();
    fake_is_master = false;

    noah_qmk_via_split_sync_command(command, (uint8_t)sizeof(command));

    CHECK(rpc_send_count == 0);
}

static void test_slave_rpc_replays_set_keycode_and_invalidates_rgb(void) {
    uint8_t command[] = {id_dynamic_keymap_set_keycode, 1, 2, 3, 0xAB, 0xCD};

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();

    rpc_registered_callback(sizeof(command), command, 0, NULL);

    CHECK(set_keycode_calls == 1);
    CHECK(last_layer == 1);
    CHECK(last_row == 2);
    CHECK(last_col == 3);
    CHECK(last_keycode == 0xABCDu);
    CHECK(rgb_invalidate_count == 1);
}

static void test_slave_rpc_replays_set_buffer_and_invalidates_rgb(void) {
    uint8_t command[] = {id_dynamic_keymap_set_buffer, 0x01, 0x23, 3, 0xAA, 0xBB, 0xCC};

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();

    rpc_registered_callback(sizeof(command), command, 0, NULL);

    CHECK(set_buffer_calls == 1);
    CHECK(last_buffer_offset == 0x0123u);
    CHECK(last_buffer_size == 3);
    CHECK(last_buffer_bytes[0] == 0xAA);
    CHECK(last_buffer_bytes[1] == 0xBB);
    CHECK(last_buffer_bytes[2] == 0xCC);
    CHECK(rgb_invalidate_count == 1);
}

static void test_slave_rpc_replays_reset_and_invalidates_rgb(void) {
    uint8_t command[] = {id_dynamic_keymap_reset};

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();

    rpc_registered_callback(sizeof(command), command, 0, NULL);

    CHECK(reset_calls == 1);
    CHECK(rgb_invalidate_count == 1);
}

static void test_slave_rpc_replays_eeprom_reset_and_invalidates_rgb(void) {
    uint8_t command[] = {id_eeprom_reset};

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();

    rpc_registered_callback(sizeof(command), command, 0, NULL);

    CHECK(via_eeprom_set_valid_calls == 1);
    CHECK(!via_eeprom_last_valid);
    CHECK(eeprom_reset_calls == 1);
    CHECK(rgb_invalidate_count == 1);
}

static void test_oversized_command_is_ignored(void) {
    uint8_t command[RPC_M2S_BUFFER_SIZE + 1] = {id_dynamic_keymap_set_buffer};

    test_reset();

    noah_qmk_via_split_sync_command(command, (uint8_t)sizeof(command));

    CHECK(rpc_send_count == 0);
}

int main(void) {
    test_init_registers_rpc();
    test_master_command_sends_packet();
    test_slave_does_not_send_packet();
    test_slave_rpc_replays_set_keycode_and_invalidates_rgb();
    test_slave_rpc_replays_set_buffer_and_invalidates_rgb();
    test_slave_rpc_replays_reset_and_invalidates_rgb();
    test_slave_rpc_replays_eeprom_reset_and_invalidates_rgb();
    test_oversized_command_is_ignored();

    puts("qmk_via_split_sync host tests passed");
    return 0;
}
