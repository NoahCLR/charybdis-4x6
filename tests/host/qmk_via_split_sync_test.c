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
static uint16_t         last_buffer_size;
static uint8_t          last_buffer_bytes[RPC_M2S_BUFFER_SIZE];
#ifdef ENCODER_MAP_ENABLE
static uint8_t          set_encoder_calls;
static uint8_t          last_encoder_layer;
static uint8_t          last_encoder_id;
static bool             last_encoder_clockwise;
static uint16_t         last_encoder_keycode;
#endif

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
#ifdef ENCODER_MAP_ENABLE
    set_encoder_calls       = 0;
    last_encoder_layer      = 0;
    last_encoder_id         = 0;
    last_encoder_clockwise  = false;
    last_encoder_keycode    = 0;
#endif
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

#ifdef ENCODER_MAP_ENABLE
void dynamic_keymap_set_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    set_encoder_calls++;
    last_encoder_layer     = layer;
    last_encoder_id        = encoder_id;
    last_encoder_clockwise = clockwise;
    last_encoder_keycode   = keycode;
}
#endif

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

static void test_slave_rpc_accepts_full_set_buffer_payload(void) {
    uint8_t command[RPC_M2S_BUFFER_SIZE] = {id_dynamic_keymap_set_buffer, 0x00, 0x00, RPC_M2S_BUFFER_SIZE - 4u};

    for (uint8_t index = 4u; index < RPC_M2S_BUFFER_SIZE; index++) {
        command[index] = index;
    }

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();

    rpc_registered_callback(sizeof(command), command, 0, NULL);

    CHECK(set_buffer_calls == 1);
    CHECK(last_buffer_offset == 0u);
    CHECK(last_buffer_size == RPC_M2S_BUFFER_SIZE - 4u);
    CHECK(memcmp(last_buffer_bytes, &command[4], RPC_M2S_BUFFER_SIZE - 4u) == 0);
    CHECK(rgb_invalidate_count == 1);
}

static void test_slave_rpc_set_buffer_size_and_length_matrix(void) {
    typedef struct {
        uint32_t before;
        uint8_t  data[RPC_M2S_BUFFER_SIZE];
        uint32_t after;
    } guarded_packet_t;

    for (uint16_t length = 0; length <= RPC_M2S_BUFFER_SIZE; length++) {
        for (uint16_t encoded_size = 0; encoded_size <= UINT8_MAX; encoded_size++) {
            guarded_packet_t packet = {
                .before = 0x12345678u,
                .data   = {id_dynamic_keymap_set_buffer, 0x00, 0x00, (uint8_t)encoded_size},
                .after  = 0x89ABCDEFu,
            };
            bool complete_payload = length >= 4u && encoded_size <= length - 4u && encoded_size <= RPC_M2S_BUFFER_SIZE - 4u;

            for (uint8_t index = 4u; index < RPC_M2S_BUFFER_SIZE; index++) {
                packet.data[index] = (uint8_t)(index ^ encoded_size);
            }

            test_reset();
            fake_is_master = false;
            noah_qmk_via_split_sync_init();

            rpc_registered_callback((uint8_t)length, packet.data, 0, NULL);

            CHECK(packet.before == 0x12345678u);
            CHECK(packet.after == 0x89ABCDEFu);
            CHECK(set_buffer_calls == (uint8_t)(complete_payload && encoded_size != 0u));
            CHECK(rgb_invalidate_count == (uint8_t)complete_payload);
            CHECK(set_keycode_calls == 0);
            CHECK(reset_calls == 0);
            CHECK(via_eeprom_set_valid_calls == 0);
            CHECK(eeprom_reset_calls == 0);
            if (complete_payload && encoded_size != 0u) {
                CHECK(last_buffer_size == encoded_size);
                CHECK(memcmp(last_buffer_bytes, &packet.data[4], encoded_size) == 0);
            }
        }
    }
}

static void test_slave_rpc_set_buffer_destination_bounds(void) {
    static const struct {
        uint16_t offset;
        uint8_t  size;
        bool     applies_storage;
        bool     applies_effects;
    } cases[] = {
        {.offset = 0u, .size = 1u, .applies_storage = true, .applies_effects = true},
        {.offset = (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2u) - 1u, .size = 1u, .applies_storage = true, .applies_effects = true},
        {.offset = DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2u, .size = 0u, .applies_storage = false, .applies_effects = true},
        {.offset = DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2u, .size = 1u, .applies_storage = false, .applies_effects = false},
        {.offset = (DYNAMIC_KEYMAP_LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS * 2u) + 1u, .size = 0u, .applies_storage = false, .applies_effects = false},
        {.offset = UINT16_MAX, .size = 1u, .applies_storage = false, .applies_effects = false},
    };

    for (size_t index = 0; index < ARRAY_SIZE(cases); index++) {
        uint8_t command[] = {
            id_dynamic_keymap_set_buffer,
            (uint8_t)(cases[index].offset >> 8),
            (uint8_t)cases[index].offset,
            cases[index].size,
            0xA5,
        };

        test_reset();
        fake_is_master = false;
        noah_qmk_via_split_sync_init();

        rpc_registered_callback((uint8_t)(4u + cases[index].size), command, 0, NULL);

        CHECK(set_buffer_calls == (uint8_t)cases[index].applies_storage);
        CHECK(rgb_invalidate_count == (uint8_t)cases[index].applies_effects);
    }
}

static void test_slave_rpc_rejects_invalid_keycode_shapes_without_effects(void) {
    uint8_t truncated[]    = {id_dynamic_keymap_set_keycode, 0, 0, 0, 0};
    uint8_t invalid_layer[] = {id_dynamic_keymap_set_keycode, DYNAMIC_KEYMAP_LAYER_COUNT, 0, 0, 0, 1};
    uint8_t invalid_row[]   = {id_dynamic_keymap_set_keycode, 0, MATRIX_ROWS, 0, 0, 1};
    uint8_t invalid_column[] = {id_dynamic_keymap_set_keycode, 0, 0, MATRIX_COLS, 0, 1};
    const struct {
        uint8_t *data;
        uint8_t  size;
    } cases[] = {
        {.data = truncated, .size = sizeof(truncated)},
        {.data = invalid_layer, .size = sizeof(invalid_layer)},
        {.data = invalid_row, .size = sizeof(invalid_row)},
        {.data = invalid_column, .size = sizeof(invalid_column)},
    };

    for (size_t index = 0; index < ARRAY_SIZE(cases); index++) {
        test_reset();
        fake_is_master = false;
        noah_qmk_via_split_sync_init();

        rpc_registered_callback(cases[index].size, cases[index].data, 0, NULL);

        CHECK(set_keycode_calls == 0);
        CHECK(rgb_invalidate_count == 0);
    }
}

#ifdef ENCODER_MAP_ENABLE
static void test_slave_rpc_validates_encoder_shape_and_coordinates(void) {
    uint8_t valid[]          = {id_dynamic_keymap_set_encoder, 1, 1, 1, 0xAB, 0xCD};
    uint8_t truncated[]      = {id_dynamic_keymap_set_encoder, 1, 1, 1, 0xAB};
    uint8_t invalid_layer[]  = {id_dynamic_keymap_set_encoder, DYNAMIC_KEYMAP_LAYER_COUNT, 0, 0, 0, 1};
    uint8_t invalid_encoder[] = {id_dynamic_keymap_set_encoder, 0, NUM_ENCODERS, 0, 0, 1};
    const struct {
        uint8_t *data;
        uint8_t  size;
    } invalid_cases[] = {
        {.data = truncated, .size = sizeof(truncated)},
        {.data = invalid_layer, .size = sizeof(invalid_layer)},
        {.data = invalid_encoder, .size = sizeof(invalid_encoder)},
    };

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();
    rpc_registered_callback(sizeof(valid), valid, 0, NULL);

    CHECK(set_encoder_calls == 1);
    CHECK(last_encoder_layer == 1);
    CHECK(last_encoder_id == 1);
    CHECK(last_encoder_clockwise);
    CHECK(last_encoder_keycode == 0xABCDu);
    CHECK(rgb_invalidate_count == 0);

    for (size_t index = 0; index < ARRAY_SIZE(invalid_cases); index++) {
        test_reset();
        fake_is_master = false;
        noah_qmk_via_split_sync_init();
        rpc_registered_callback(invalid_cases[index].size, invalid_cases[index].data, 0, NULL);

        CHECK(set_encoder_calls == 0);
        CHECK(rgb_invalidate_count == 0);
    }
}
#endif

static void test_slave_rpc_accepts_defined_reset_padding(void) {
    uint8_t command[RPC_M2S_BUFFER_SIZE] = {id_dynamic_keymap_reset};

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();

    rpc_registered_callback(sizeof(command), command, 0, NULL);

    CHECK(reset_calls == 1);
    CHECK(rgb_invalidate_count == 1);
}

static void test_slave_rpc_rejects_oversized_transport(void) {
    uint8_t command[RPC_M2S_BUFFER_SIZE + 1] = {id_dynamic_keymap_reset};

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();

    rpc_registered_callback(sizeof(command), command, 0, NULL);

    CHECK(reset_calls == 0);
    CHECK(rgb_invalidate_count == 0);
}

static void test_slave_rpc_rejects_null_and_unsupported_packets_without_effects(void) {
    uint8_t unsupported[] = {0xFF, 0xAA, 0x55};

    test_reset();
    fake_is_master = false;
    noah_qmk_via_split_sync_init();

    rpc_registered_callback(1u, NULL, 0, NULL);
    rpc_registered_callback(sizeof(unsupported), unsupported, 0, NULL);

    CHECK(set_keycode_calls == 0);
    CHECK(set_buffer_calls == 0);
    CHECK(reset_calls == 0);
    CHECK(via_eeprom_set_valid_calls == 0);
    CHECK(eeprom_reset_calls == 0);
    CHECK(rgb_invalidate_count == 0);
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
    test_slave_rpc_accepts_full_set_buffer_payload();
    test_slave_rpc_set_buffer_size_and_length_matrix();
    test_slave_rpc_set_buffer_destination_bounds();
    test_slave_rpc_rejects_invalid_keycode_shapes_without_effects();
#ifdef ENCODER_MAP_ENABLE
    test_slave_rpc_validates_encoder_shape_and_coordinates();
#endif
    test_slave_rpc_replays_reset_and_invalidates_rgb();
    test_slave_rpc_accepts_defined_reset_padding();
    test_slave_rpc_replays_eeprom_reset_and_invalidates_rgb();
    test_slave_rpc_rejects_oversized_transport();
    test_slave_rpc_rejects_null_and_unsupported_packets_without_effects();
    test_oversized_command_is_ignored();

    puts("qmk_via_split_sync host tests passed");
    return 0;
}
